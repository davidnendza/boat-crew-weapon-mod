// utility gear on the water: smoke pots, life rafts, Dave (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- utility gear on the water: smoke pots and life rafts (kUtilityGear) ---------------------------------------------
// Smoke: the game's own smoke shell cloud ('Smoke Shell Large', the smoke artillery's impactPrefab), spawned through
// VFXManager.spawnVFX like a landing smoke shell. Its 'Smoke Collider' (layer 12) blocks most enemy spotting rays. It
// lasts the game's 42 s and is networked by the game to the other players (only the dropping player's machine spawns it).
// Raft: a copy of the ships' lifeboat decor mesh that heads for the nearest of this machine's own crew in the water and
// rescues them on contact the game's own way for a saved swimmer: healthState Recovered (5), which the end-of-battle scan
// keeps and recoverCharacters turns back into Healthy; then the removal steps of a pickup.
struct UtilApi {
  bool tried = false, ok = false, dave = false;
  void *vfxInst = nullptr, *spawnVfx = nullptr, *pmInst = nullptr, *findProj = nullptr, *findBy = nullptr, *findAll = nullptr, *cmoType = nullptr;
  void *trPos = nullptr, *trSetPos = nullptr, *trSetRot = nullptr, *goType = nullptr, *poolType = nullptr, *repool = nullptr, *dt = nullptr, *time = nullptr;
  void* pickUp = nullptr;                                             // PickupBehaviour.pickUp: Dave's crates (null: Dave picks up nothing)
  int offImpact = -1, oOwner = -1, oFrom = -1, oChar = -1, cHealth = -1, cHP = -1, cSinking = -1, cName = -1, cNameCustom = -1;
};
static UtilApi g_ua;
static int g_utilFaults = 0;                                          // -1: the utility gear faulted and is off for the session
static bool UtilReady() {
  UtilApi& a = g_ua;
  if (a.tried) return a.ok;
  a.tried = true;
  auto typeOf = [](void* k) { return k ? il2cpp_type_get_object(il2cpp_class_get_type(k)) : nullptr; };
  void* vfx = FindClass("Core", "VFXManager");
  a.vfxInst = vfx ? il2cpp_class_get_field_from_name(vfx, "instance") : nullptr; a.spawnVfx = vfx ? FindMethod(vfx, "spawnVFX", 4) : nullptr;
  void* pm = FindClass("Core", "ProjectileManager");
  a.pmInst = pm ? il2cpp_class_get_field_from_name(pm, "instance") : nullptr; a.findProj = pm ? FindMethod(pm, "findProjectileType", 1) : nullptr;
  a.offImpact = FieldOffset(FindClass("Core", "ProjectileType"), "impactPrefab");
  void* o = FindClass("UnityEngine", "Object"); a.findBy = o ? FindMethod(o, "FindObjectsByType", 3, "System.Type") : nullptr;
  void* res = FindClass("UnityEngine", "Resources"); a.findAll = res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
  void* cmo = FindClass("Core", "CrewMemberOverboard"); a.cmoType = typeOf(cmo);
  a.oOwner = FieldOffset(cmo, "networkOwner"); a.oFrom = FieldOffset(cmo, "spawnedFrom"); a.oChar = FieldOffset(cmo, "character");
  void* cc = FindClass("Core", "CrewCharacter");
  a.cHealth = FieldOffset(cc, "healthState"); a.cHP = FieldOffset(cc, "HP"); a.cSinking = FieldOffset(cc, "inSinkingBoat"); a.cName = FieldOffset(cc, "name"); a.cNameCustom = FieldOffset(cc, "nameCustom");
  void* tr = FindClass("UnityEngine", "Transform");
  a.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; a.trSetPos = tr ? FindMethod(tr, "set_position", 1) : nullptr; a.trSetRot = tr ? FindMethod(tr, "set_rotation", 1) : nullptr;
  a.goType = typeOf(FindClass("UnityEngine", "GameObject"));
  void* po = FindClass("", "PoolableObject"); a.poolType = typeOf(po); a.repool = po ? FindMethod(po, "repool", 0) : nullptr;
  void* tm = FindClass("UnityEngine", "Time"); a.dt = tm ? FindMethod(tm, "get_deltaTime", 0) : nullptr; a.time = tm ? FindMethod(tm, "get_time", 0) : nullptr;
  bool smoke = a.vfxInst && a.spawnVfx && a.pmInst && a.findProj && a.offImpact >= 0;
  bool raft = a.findBy && a.findAll && a.cmoType && a.oOwner >= 0 && a.oFrom >= 0 && a.oChar >= 0 && a.cHealth >= 0 && a.cHP >= 0 && a.cSinking >= 0 &&
              a.trPos && a.trSetPos && a.trSetRot && a.goType && a.poolType && a.repool && a.dt && a.time && ModelApiReady() && g_mapi.getComponent;
  void* pbk = FindClass("Core", "PickupBehaviour"); a.pickUp = pbk ? FindMethod(pbk, "pickUp", 0) : nullptr;
  bool dave = a.trPos && a.trSetPos && a.trSetRot && a.dt && a.time && m_getTransform;
  if (!a.pickUp || g_offSplashed < 0) { Log("utility gear: Dave's crate pickup is missing (pickUp %p, crate list %s); Daves only stand there", a.pickUp, g_offSplashed < 0 ? "off" : "on"); a.pickUp = nullptr; }
  a.ok = smoke || raft || dave;
  if (!smoke) { a.spawnVfx = nullptr; Log("utility gear: the smoke spawn is missing (VFXManager %p spawnVFX %p ProjectileManager %p impactPrefab +0x%X); smoke pots do nothing", a.vfxInst, a.spawnVfx, a.pmInst, a.offImpact); }
  if (!raft) { a.findBy = nullptr; Log("utility gear: a swimmer or raft lookup is missing (FindObjectsByType %p swimmer fields %d/%d/%d crew fields %d/%d/%d repool %p); life rafts do nothing",
                                     a.findBy, a.oOwner, a.oFrom, a.oChar, a.cHealth, a.cHP, a.cSinking, a.repool); }
  if (!dave) Log("utility gear: a transform or time lookup is missing; Daves do nothing");
  a.dave = dave;
  if (a.ok) Log("utility gear: ready (smoke pots %s, life rafts %s, Dave %s)", smoke ? "on" : "off", raft ? "on" : "off", !dave ? "off" : a.pickUp ? "on" : "on, without crates");
  return a.ok;
}
static float UtilTime(void* m) { void* r = m ? Invoke(m, nullptr) : nullptr; return r ? *(float*)((char*)r + 0x10) : 0; }
struct UtilStat { int smokes = 0, smokeFails = 0, smokeSkips = 0, rafts = 0, saved = 0, daves = 0, crates = 0, rp = 0; };
static UtilStat g_utilStat;
static void* g_utilEng = nullptr;                                     // the battle the counts and rafts belong to
// The game's smoke clouds by the shell that makes them: 'Smoke Shell Large' (smoke artillery) and 'Smoke Shell' (60 mm
// mortar). Two pools of 24 each, so a pot's ring puffs use the small one and leave the large pool to its centre puffs.
static void* SmokePrefab(bool large) {
  static void* prefab[2] = {}; static void* ph[2] = {};
  int i = large ? 1 : 0;
  if (!Alive(prefab[i])) {
    Unpin(ph[i]); prefab[i] = ph[i] = nullptr;
    void* pm = nullptr; il2cpp_field_static_get_value(g_ua.pmInst, &pm);
    void* a[1] = { il2cpp_string_new(large ? "Projectile_US_SmokeArtyShell" : "Projectile_US_Mortar_60mm_Smoke") };
    void* pt = pm ? Invoke(g_ua.findProj, pm, a) : nullptr;
    prefab[i] = pt ? *(void**)((char*)pt + g_ua.offImpact) : nullptr;
    if (prefab[i]) ph[i] = Pin(prefab[i]);
  }
  return prefab[i];
}
// A co-op client's spawnVFX also goes to the host, which echoes it to every client, the sender too (sendVfxToClients
// sends byID -1), so a client only sends it, as the game's onClientVfxTrigger does (VFXManager +0x48 with the prefab's
// pool name), and the host's copy is its one cloud.
// False when that path is not there: the caller spawns it the normal way.
static bool SmokeSendToHost(void* vfx, void* prefab, V3* pos) {
  static int oPooler = -2, oTrig = -1, oTypes = -1, oName = -1, oPrefab = -1; static void* inv = nullptr;
  if (oPooler == -2) { void* k = il2cpp_object_get_class(vfx); oPooler = FieldOffset(k, "pooler"); oTrig = FieldOffset(k, "onClientVfxTrigger"); }
  void* del = oTrig >= 0 ? *(void**)((char*)vfx + oTrig) : nullptr;
  void* pooler = oPooler >= 0 ? *(void**)((char*)vfx + oPooler) : nullptr;
  if (!del || !pooler) return false;
  if (oTypes < 0) oTypes = FieldOffset(il2cpp_object_get_class(pooler), "types");
  void* types = oTypes >= 0 ? *(void**)((char*)pooler + oTypes) : nullptr;
  int n = ArrLen(types);
  void* name = nullptr;
  for (int i = 0; i < n && !name; i++) {                               // ObjPooler.types: the pool whose prefab this is
    void* t = ArrAt(types, i); if (!t) continue;
    if (oName < 0) { void* k = il2cpp_object_get_class(t); oName = FieldOffset(k, "name"); oPrefab = FieldOffset(k, "prefab"); if (oName < 0 || oPrefab < 0) return false; }
    if (*(void**)((char*)t + oPrefab) == prefab) name = *(void**)((char*)t + oName);
  }
  if (!inv) inv = FindMethod(il2cpp_object_get_class(del), "Invoke", 2);
  if (!name || !inv) return false;
  void* a[2] = { name, pos }; Invoke(inv, del, a);
  return true;
}
// The pots' clouds alive, by pool (0 small, 1 large): their spawn times, as each lasts the game's 42 s. The pools (24
// each) are shared with the game's own smoke shells, so the pots leave a third of each free.
static std::vector<float> g_smokeLive[2];
static const int kSmokeCap = 16;
// Fire Mission's marker: one small cloud, counted with the pots' so they leave room for it.
static bool FireMissionSmoke(V3 p) {
  if (!UtilReady() || !g_ua.spawnVfx) return false;
  void* prefab = SmokePrefab(false); void* vfx = nullptr; il2cpp_field_static_get_value(g_ua.vfxInst, &vfx);
  V3 pos = { p.x, 0, p.z }; bool force = true, local = false;          // as a pot's: never capped, networked as the game does
  void* a[4] = { prefab, &pos, &force, &local };
  bool ok = vfx && prefab && Invoke(g_ua.spawnVfx, vfx, a);
  if (ok) g_smokeLive[0].push_back(UtilTime(g_ua.time));
  return ok;
}
// 1 a cloud, 0 the pool gave none, -1 left out to keep clouds free for the smoke shells
static int SmokePuff(V3 p, bool large) {
  float now = UtilTime(g_ua.time);
  std::vector<float>& live = g_smokeLive[large ? 1 : 0];
  live.erase(std::remove_if(live.begin(), live.end(), [&](float t) { return now - t > 42 || now < t; }), live.end());
  if ((int)live.size() >= kSmokeCap) { g_utilStat.smokeSkips++; return -1; }
  void* prefab = SmokePrefab(large);
  void* vfx = nullptr; il2cpp_field_static_get_value(g_ua.vfxInst, &vfx);
  V3 pos = { p.x, 0, p.z }; bool force = true, local = false;          // force: never skipped by the 30-cloud cap; networked as the game does
  void* a[4] = { prefab, &pos, &force, &local };
  bool sent = vfx && prefab && g_coopRole == 2 && SmokeSendToHost(vfx, prefab, &pos);
  static bool told = false;
  if (sent && !told) { told = true; Log("utility gear: co-op client: smoke clouds are sent to the host, whose copy comes back here"); }
  bool ok = sent || (vfx && prefab && Invoke(g_ua.spawnVfx, vfx, a));
  if (ok) { g_utilStat.smokes++; live.push_back(now); } else g_utilStat.smokeFails++;
  return ok ? 1 : 0;
}
// A smoke pot floats where it landed and every kSmokePuffEvery s puffs five clouds (one large on itself, two large 40 m
// out, two small 70 m out, the axis turned by the golden angle each puff) into a screen about 150 m across that stands
// until smoke_seconds after the landing. The pots share 16 clouds of each 24-cloud pool (kSmokeCap), so several at once
// come out thinner (skips are logged).
static Q4 QMul(Q4 a, Q4 b);
static Q4 QAxis(int axis, float a);
static const float kSmokePuffEvery = 12;
struct SmokePot { V3 p = {}; float next = 0, until = 0; int puffs = 0, missed = 0, skipped = 0; std::string item; void *go = nullptr, *h = nullptr; };
static std::vector<SmokePot> g_pots;
static void SmokePotPuff(SmokePot& s) {                               // the centre cloud first, so a short pool skips the ring
  float a = 2.39996f * (float)s.puffs, cs = cosf(a), sn = sinf(a);
  auto at = [&](float along, float across) { return V3{ s.p.x + along * cs - across * sn, 0, s.p.z + along * sn + across * cs }; };
  int ok[5] = { SmokePuff(s.p, true), SmokePuff(at(40, 0), true), SmokePuff(at(-40, 0), true), SmokePuff(at(0, 70), false), SmokePuff(at(0, -70), false) };
  s.puffs++; for (int o : ok) { s.missed += o == 0; s.skipped += o < 0; }
}
static void SmokePotModel(SmokePot& s, float now) {                  // the pot afloat: upright (its drum axis is the .obj's +Z), bobbing
  if (!Alive(s.go)) return;
  void* tf = Invoke(g_mapi.goTransform, s.go); if (!tf) return;
  V3 p = { s.p.x, 0.05f + 0.04f * sinf(now * 2.1f), s.p.z };
  Q4 q = QMul(QAxis(1, 0.7f * (float)s.puffs), QAxis(0, -1.5707963f + 0.08f * sinf(now * 1.3f)));
  void* a[1] = { &p }; Invoke(g_ua.trSetPos, tf, a);
  void* b[1] = { &q }; Invoke(g_ua.trSetRot, tf, b);
}
static void SmokePotDrop(SmokePot& s) { if (Alive(s.go) && g_mapi.destroy) { void* a[1] = { s.go }; Invoke(g_mapi.destroy, nullptr, a); } Unpin(s.h); s.go = s.h = nullptr; }
static void UtilSmoke(V3 p, const std::string& item) {
  if (!g_ua.spawnVfx) return;
  float now = UtilTime(g_ua.time);
  SmokePot s; s.p = { p.x, 0, p.z }; s.item = item; s.until = now + (float)g_smokeSeconds; s.next = now + kSmokePuffEvery;
  SmokePotPuff(s);
  std::string f = g_folder + "\\models\\smoke_pot.obj";
  if (!g_modelsOff && ModelApiReady() && g_ua.trSetPos && GetFileAttributesA(f.c_str()) != INVALID_FILE_ATTRIBUTES) s.go = LoadModelCopy(f, "smoke pot");
  if (s.go) { s.h = Pin(s.go); bool on = true; void* a[1] = { &on }; Invoke(m_setActive, s.go, a); SmokePotModel(s, now); }
  Log("utility gear: '%s' landed at (%.0f, %.0f): %s%s", item.c_str(), p.x, p.z, s.missed + s.skipped < 5
      ? ("smoke pot burning: five clouds about 150 m across every 12 s for " + std::to_string((int)g_smokeSeconds) + " s").c_str()
      : s.skipped == 5 ? "smoke pot burning, no clouds yet: the other pots have the mod's share of the smoke pools"
      : !SmokePrefab(true) ? "no smoke: the smoke artillery round's cloud was not found" : "no smoke: the game's smoke pools gave none (all in use?)",
      s.go ? "; the pot floats there" : "; no pot model (smoke_pot.obj did not load)");
  g_pots.push_back(s);
}
static void SmokePotTick(float now) {
  for (size_t i = 0; i < g_pots.size(); ) {
    SmokePot& s = g_pots[i];
    if (now >= s.next && now < s.until) { s.next += kSmokePuffEvery; SmokePotPuff(s); }
    if (now < s.until) { SmokePotModel(s, now); i++; continue; }
    Log("utility gear: a '%s' smoke pot burnt out after %d puffs (%d clouds the pools could not give, %d left out to share the pools)", s.item.c_str(), s.puffs, s.missed, s.skipped);
    SmokePotDrop(s); g_pots.erase(g_pots.begin() + i);
  }
}
struct Raft { void *go = nullptr, *h = nullptr; V3 p = {}; float until = 0; std::string item; int saved = 0; };
static std::vector<Raft> g_rafts;
static void* RaftModel() {                                            // the ships' lifeboat decor mesh, found once
  static void* model = nullptr; static bool tried = false;
  if (tried) return Alive(model) ? model : nullptr;
  tried = true;
  void* a[1] = { g_ua.goType }; void* arr = Invoke(g_ua.findAll, nullptr, a);
  int n = ArrLen(arr);
  for (int i = 0; i < n && !model; i++) { void* go = ArrAt(arr, i); if (go && NameOf(go) == "Ship_decor_lifeboat_LOD0") model = go; }
  if (model) Pin(model);
  Log("utility gear: raft model %s", model ? "is the ships' lifeboat (Ship_decor_lifeboat_LOD0)" : "not found among the game's objects; rafts are invisible but still pick crew up");
  return model;
}
static void RaftDrop(Raft& r) { if (Alive(r.go) && g_mapi.destroy) { void* a[1] = { r.go }; Invoke(g_mapi.destroy, nullptr, a); } Unpin(r.h); r = Raft(); }
static void UtilRaft(V3 p, const std::string& item) {
  if (!g_ua.findBy) return;
  Raft r; r.p = { p.x, 0, p.z }; r.item = item; r.until = UtilTime(g_ua.time) + 180;   // three minutes on the water
  // the mod's Carley float (tools/make_utility_models.py, water line at y 0); else the ships' lifeboat
  std::string f = g_folder + "\\models\\life_raft.obj";
  if (!g_modelsOff && GetFileAttributesA(f.c_str()) != INVALID_FILE_ATTRIBUTES) r.go = LoadModelCopy(f, "life raft");
  static bool told = false;
  if (!told) { told = true; Log("utility gear: raft model %s", r.go ? "is models\\life_raft.obj" : "life_raft.obj did not load; trying the ships' lifeboat"); }
  if (!r.go) { void* model = RaftModel(); void* src[1] = { model }; r.go = model ? Invoke(m_instantiate, nullptr, src) : nullptr; }
  if (r.go) {
    r.h = Pin(r.go);
    bool on = true; void* a[1] = { &on }; Invoke(m_setActive, r.go, a);
    void* tf = Invoke(g_mapi.goTransform, r.go);
    if (tf) { void* b[1] = { &r.p }; Invoke(g_ua.trSetPos, tf, b); }
  }
  g_rafts.push_back(r); g_utilStat.rafts++;
  Log("utility gear: '%s' landed at (%.0f, %.0f): a life raft is out for 180 s, heading for your crew in the water at %.0f m/s", item.c_str(), p.x, p.z, g_raftSpeed);
}
// Dave: a stiff T-posed placeholder (models/dave.obj, soles at y 0, facing +Z) that glides to the nearest RP crate on
// the water and picks it up the game's own way, PickupBehaviour.pickUp (RP, sound and rise animation), for 2 minutes.
// The crates are the ones the pickup hook saw this frame (g_crateFrame). Co-op: only this machine's copy of a crate;
// a client's RP is overwritten by the host's.
static const float kDaveSpeed = 40, kDaveReach = 4, kDaveLife = 120;   // m/s, m, s; shortcut: fixed, a drops.ini key if asked
struct Dave { void *go = nullptr, *h = nullptr; V3 p = {}; float until = 0; std::string item; int crates = 0, rp = 0; Q4 q = {}; };
static std::vector<Dave> g_daves;
static void DaveDrop(Dave& d) { if (Alive(d.go) && g_mapi.destroy) { void* a[1] = { d.go }; Invoke(g_mapi.destroy, nullptr, a); } Unpin(d.h); d = Dave(); }
static void DavePose(Dave& d) {
  if (!Alive(d.go)) return;
  void* tf = Invoke(g_mapi.goTransform, d.go); if (!tf) return;
  V3 p = { d.p.x, 0.02f, d.p.z };                                      // socks just on the water (sea level; no wave riding)
  void* a[1] = { &p }; Invoke(g_ua.trSetPos, tf, a);
  void* b[1] = { &d.q }; Invoke(g_ua.trSetRot, tf, b);
}
// Dave's size: dave.obj is 1.8 m like a crew body mesh, but the boat scales its crew, so Dave copies one crew member's
// lossyScale; logged once with that crew member's world body height.
static float DaveCrewScale() {
  void* craft = g_oCeCrew >= 0 && g_oMgrCrew >= 0 ? PlayerCraft() : nullptr;
  void* mgr = craft ? *(void**)((char*)craft + g_oCeCrew) : nullptr;
  void* list = mgr ? *(void**)((char*)mgr + g_oMgrCrew) : nullptr;
  void* cm = ListCount(list) > 0 ? ListAt(list, 0) : nullptr;
  static void *lossy = nullptr, *bounds = nullptr; static int oVis = -2, oBody = -1;
  if (!lossy) { void* tr = FindClass("UnityEngine", "Transform"); lossy = tr ? FindMethod(tr, "get_lossyScale", 0) : nullptr; }
  if (!bounds) { void* r = FindClass("UnityEngine", "Renderer"); bounds = r ? FindMethod(r, "get_bounds", 0) : nullptr; }
  if (oVis == -2) { oVis = FieldOffset(FindClass("Core", "CrewMember"), "crewMemberVisual"); oBody = FieldOffset(FindClass("Core", "CrewMemberVisual"), "body"); }
  void* tf = cm ? Invoke(m_getTransform, cm) : nullptr;
  V3 ls = tf && lossy ? GetV3(lossy, tf) : V3{ 1, 1, 1 };
  float k = ls.y > 0.5f && ls.y < 10 ? ls.y : 1.0f;
  static bool told = false;
  if (!told) {
    told = true;
    void* vis = cm && oVis >= 0 ? *(void**)((char*)cm + oVis) : nullptr;
    void* body = vis && oBody >= 0 ? *(void**)((char*)vis + oBody) : nullptr;
    void* r0 = body && ArrLen(body) > 0 ? ArrAt(body, 0) : nullptr;
    void* b = r0 && bounds ? Invoke(bounds, r0) : nullptr;               // boxed Bounds: centre +0x10, extents +0x1C
    float h = b ? 2 * *(float*)((char*)b + 0x20) : -1;
    Log("Dave: a crew member's world scale is (%.2f, %.2f, %.2f), body %.2f m tall in the world (pose included); Dave is scaled x%.2f", ls.x, ls.y, ls.z, h, k);
  }
  return k;
}
static void UtilDave(V3 p, const std::string& item) {
  if (!g_ua.dave) return;
  Dave d; d.p = { p.x, 0, p.z }; d.item = item; d.until = UtilTime(g_ua.time) + kDaveLife; d.q = LookRot({ 0, 0, 1 });
  std::string f = g_folder + "\\models\\dave.obj";
  if (!g_modelsOff && ModelApiReady() && GetFileAttributesA(f.c_str()) != INVALID_FILE_ATTRIBUTES) d.go = LoadModelCopy(f, "Dave");
  if (d.go) {
    d.h = Pin(d.go); bool on = true; void* a[1] = { &on }; Invoke(m_setActive, d.go, a);
    float k = DaveCrewScale(); V3 sc = { k, k, k }; void* tf = Invoke(g_mapi.goTransform, d.go);
    if (tf) { void* b[1] = { &sc }; Invoke(m_setLocalScale, tf, b); }
    DavePose(d);
  }
  g_daves.push_back(d); g_utilStat.daves++;
  Log("utility gear: '%s' landed at (%.0f, %.0f): Dave is out for %.0f s, gliding to RP crates at %.0f m/s%s%s", item.c_str(), p.x, p.z, kDaveLife, kDaveSpeed,
      d.go ? "" : " (invisible: dave.obj did not load)", g_ua.pickUp ? "" : "; he cannot pick crates up (lookup missing)");
}
static void DaveTick(float now) {
  for (size_t i = 0; i < g_daves.size(); ) {
    if (now > g_daves[i].until) {
      Log("utility gear: a '%s' Dave clocked out after picking up %d crate(s), %d RP", g_daves[i].item.c_str(), g_daves[i].crates, g_daves[i].rp);
      DaveDrop(g_daves[i]); g_daves.erase(g_daves.begin() + i);
    } else i++;
  }
  if (g_daves.empty()) { g_crateFrame.clear(); return; }
  std::vector<std::pair<void*, V3>> crates;                            // this frame's crates on the water, where they are
  for (void* pb : g_crateFrame) {
    if (!Alive(pb) || *(bool*)((char*)pb + g_offPickedUp)) continue;   // another Dave or the boat took it this frame
    void* tf = Invoke(m_getTransform, pb); if (tf) crates.push_back({ pb, GetV3(g_ua.trPos, tf) });
  }
  g_crateFrame.clear();
  float dt = UtilTime(g_ua.dt);
  for (Dave& d : g_daves) {
    int best = -1; float bd = kDaveSpeed * (d.until - now) + kDaveReach; // any crate he can reach before he clocks out
    for (size_t i = 0; i < crates.size(); i++) {
      if (!crates[i].first) continue;
      float dx = crates[i].second.x - d.p.x, dz = crates[i].second.z - d.p.z, dd = sqrtf(dx * dx + dz * dz);
      if (dd < bd) { bd = dd; best = (int)i; }
    }
    if (best >= 0) {
      void* pb = crates[best].first;
      float dx = crates[best].second.x - d.p.x, dz = crates[best].second.z - d.p.z;
      if (bd < kDaveReach && g_ua.pickUp) {
        void* pt = *(void**)((char*)pb + g_offPickup); int rp = pt ? *(int*)((char*)pt + g_offPtCount) : 0;
        Invoke(g_ua.pickUp, pb);                                       // as the boat touching it; returns at once if already taken
        crates[best].first = nullptr;
        static bool toldMiss = false;
        if (!*(bool*)((char*)pb + g_offPickedUp) && !toldMiss) { toldMiss = true; Log("utility gear: Dave's pickUp on a crate did not take (still not picked up)"); }
        if (*(bool*)((char*)pb + g_offPickedUp)) {
          d.crates++; d.rp += rp; g_utilStat.crates++; g_utilStat.rp += rp;
          if (g_utilStat.crates <= 20) Log("utility gear: Dave picked up a crate (%d RP)", rp);
          if (g_dropsOn && g_dropChance > 0) WatchPickup(pb);          // the mod's loot roll, as for the boat's own pickups (deduped)
        }
      } else if (bd > 0.01f) {
        float step = kDaveSpeed * dt; if (step > bd) step = bd;
        d.p.x += dx / bd * step; d.p.z += dz / bd * step;
        d.q = LookRot({ dx / bd, 0, dz / bd });
      }
    }
    DavePose(d);
  }
}
// The smoke or raft in its own guard (no C++ objects here): a fault in it turns off only the utility gear, not the found
// torpedoes, whose fallback (TorpRestoreStock) would make the next pot or raft a live depth charge.
static void SafeUtilSpawn(TorpRun& r, V3 p) {
  if (g_utilFaults < 0) return;
  __try { if (r.util == 1) UtilSmoke(p, r.item); else if (r.util == 3) UtilDave(p, r.item); else UtilRaft(p, r.item); }
  __except (FaultFilter("utility gear (landing)", GetExceptionInformation())) { g_utilFaults = -1; }
}
// Called after the game's FixedUpdate once the dropped charge is in the water: the charge goes back to its pool before it
// sinks or goes off, first, and then the dropping player's machine lays the smoke or puts out the raft.
static void UtilLanded(void* t, TorpRun& r, V3 p) {
  r.ended = r.reported = true;
  void* go = Invoke(m_getGameObject, t);
  bool off = false; void* a[1] = { &off };
  if (go) Invoke(m_setActive, go, a);                                  // OnDisable hands it back to the torpedo pool
  if (r.player && r.owner < 0 && UtilReady()) SafeUtilSpawn(r, p);
}
static std::string CrewNameOf(void* ch) {
  void* c = g_ua.cNameCustom >= 0 ? *(void**)((char*)ch + g_ua.cNameCustom) : nullptr;   // Str(null) is "(null)"
  if (c && *(int*)((char*)c + 0x10) > 0) return CrewClean(Str(c));
  void* n = g_ua.cName >= 0 ? *(void**)((char*)ch + g_ua.cName) : nullptr;
  return n ? CrewClean(Str(n)) : "a crew member";
}
static std::vector<std::pair<void*, V3>> g_utilSw;                    // the swimmers of the last lookup; only dereferenced on its frame
static std::set<void*> g_utilOwnCh;                                   // this battle's crew of the player's boat, noted while aboard
static float g_utilScanAt = 0;                                        // the next lookup (4 a second)
// Every frame: rafts move toward this machine's own swimmers and pick them up; the battle's summary when it ends.
static void UtilTick() {
  void* eng = EngagementInstance();
  if (eng != g_utilEng) {
    if (g_utilStat.smokes || g_utilStat.smokeFails || g_utilStat.rafts || g_utilStat.daves)
      Log("utility gear, last battle: %d smoke clouds laid (%d the pools could not give, %d left out to share the pools), %d life raft(s) out, %d crew picked up, %d Dave(s) out, %d crate(s) and %d RP picked up by Dave",
          g_utilStat.smokes, g_utilStat.smokeFails, g_utilStat.smokeSkips, g_utilStat.rafts, g_utilStat.saved, g_utilStat.daves, g_utilStat.crates, g_utilStat.rp);
    g_utilStat = UtilStat(); g_utilEng = eng;
    for (Raft& r : g_rafts) RaftDrop(r);
    g_rafts.clear(); for (SmokePot& p : g_pots) SmokePotDrop(p); g_pots.clear(); for (Dave& d : g_daves) DaveDrop(d); g_daves.clear(); g_crateFrame.clear();
    g_utilSw.clear(); g_utilOwnCh.clear(); g_utilScanAt = 0; g_smokeLive[0].clear(); g_smokeLive[1].clear();
  }
  float now = UtilTime(g_ua.time);
  bool scan = eng && now >= g_utilScanAt;
  bool known = g_oCeCrew >= 0 && g_oMgrCrew >= 0 && g_oCmChar >= 0;  // the crew list's offsets (GfxReady); else no crew check
  if (scan) {
    g_utilScanAt = now + 0.25f;
    // the player's crew, noted while still aboard (a man goes overboard from there): only they get a raft, never an
    // enemy sailor in the water
    void* craft = known ? PlayerCraft() : nullptr;
    void* mgr = craft ? *(void**)((char*)craft + g_oCeCrew) : nullptr;
    void* list = mgr ? *(void**)((char*)mgr + g_oMgrCrew) : nullptr;
    for (int i = 0, n = ListCount(list); i < n; i++) { void* cm = ListAt(list, i); void* ch = cm ? *(void**)((char*)cm + g_oCmChar) : nullptr; if (ch) g_utilOwnCh.insert(ch); }
  }
  if (!g_pots.empty()) SmokePotTick(now);
  DaveTick(now);
  if (g_rafts.empty() || !g_ua.findBy) return;
  float dt = UtilTime(g_ua.dt);
  for (size_t i = 0; i < g_rafts.size(); ) {
    if (now > g_rafts[i].until) {
      Log("utility gear: a '%s' raft drifted off after picking up %d crew", g_rafts[i].item.c_str(), g_rafts[i].saved);
      RaftDrop(g_rafts[i]); g_rafts.erase(g_rafts.begin() + i);
    } else i++;
  }
  if (g_rafts.empty()) return;
  // this machine's own crew in the water: owned here, not a mission's stranded survivor, one of the player's crew, alive
  // and still Missing (3). Looked up 4 times a second (FindObjectsByType makes a new array each call): in between, the
  // rafts steer for where the swimmers were, and a pickup waits for a lookup frame, when the swimmers are fresh.
  if (scan) {
    int inactive = 0, sorting = 0; void* a[3] = { g_ua.cmoType, &inactive, &sorting };
    void* arr = Invoke(g_ua.findBy, nullptr, a);
    int n = ArrLen(arr);
    g_utilSw.clear();
    for (int i = 0; i < n; i++) {
      void* s = ArrAt(arr, i); if (!s) continue;
      void* ch = *(void**)((char*)s + g_ua.oChar);
      if (!*(bool*)((char*)s + g_ua.oOwner) || *(void**)((char*)s + g_ua.oFrom) || !ch) continue;
      if (*(float*)((char*)ch + g_ua.cHP) <= 0 || *(int*)((char*)ch + g_ua.cHealth) != 3) continue;
      if (known && !g_utilOwnCh.count(ch)) {
        static bool told = false;
        if (!told) { told = true; Log("utility gear: %s is in the water but not one of your crew; rafts leave them to the game", CrewNameOf(ch).c_str()); }
        continue;
      }
      void* tf = Invoke(m_getTransform, s); if (!tf) continue;
      g_utilSw.push_back({ s, GetV3(g_ua.trPos, tf) });
    }
  }
  std::vector<std::pair<void*, V3>>& sw = g_utilSw;
  for (Raft& r : g_rafts) {
    int best = -1; float bd = 1000;                                    // a swimmer within a kilometre
    for (size_t i = 0; i < sw.size(); i++) {
      if (!sw[i].first) continue;
      float dx = sw[i].second.x - r.p.x, dz = sw[i].second.z - r.p.z, d = sqrtf(dx * dx + dz * dz);
      if (d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) continue;
    V3 to = sw[best].second;
    float dx = to.x - r.p.x, dz = to.z - r.p.z;
    if (bd < 3) {                                                      // alongside: aboard the raft
      if (!scan) continue;                                             // the swimmer is looked up again first
      void* s = sw[best].first; void* ch = *(void**)((char*)s + g_ua.oChar);
      std::string name = CrewNameOf(ch);
      *(int*)((char*)ch + g_ua.cHealth) = 5;                           // Recovered: kept by the end-of-battle scan
      *(bool*)((char*)ch + g_ua.cSinking) = false;
      // the pickup's own steps, in its order: a swimmer left active with no character becomes a stranded stranger
      void* go = Invoke(m_getGameObject, s);
      bool off = false; void* b[1] = { &off }; if (go) Invoke(m_setActive, go, b);
      void* c[1] = { g_ua.poolType }; void* po = Invoke(g_mapi.getComponent, s, c);
      if (po) Invoke(g_ua.repool, po);
      SetRef(s, (char*)s + g_ua.oChar, nullptr);
      sw[best].first = nullptr;
      r.saved++; g_utilStat.saved++;
      Log("utility gear: the '%s' raft picked up %s (safe; back with your crew after the battle)", r.item.c_str(), name.c_str());
      ShowMessage("Life raft picked up " + name);
      continue;
    }
    // full speed, easing over the last 10 m so it stops alongside rather than past them
    float step = (float)g_raftSpeed * dt * (bd < 10 ? (std::max)(0.3f, bd / 10) : 1.0f);
    if (step > bd - 1) step = bd - 1;
    if (step <= 0) continue;
    r.p.x += dx / bd * step; r.p.z += dz / bd * step;
    if (Alive(r.go)) {
      void* tf = Invoke(g_mapi.goTransform, r.go);
      // shortcut: the raft sits at sea level (y 0) and does not ride the waves; sample the Crest height if it looks wrong
      if (tf) { void* b[1] = { &r.p }; Invoke(g_ua.trSetPos, tf, b); Q4 q = LookRot({ dx / bd, 0, dz / bd }); void* c[1] = { &q }; Invoke(g_ua.trSetRot, tf, c); }
    }
  }
}
static void TorpDepth(void* t, TorpRun& r, float dt);
// After the game's FixedUpdate: the hit, homing, numbers for the battle summary.
static void TorpAfter(void* t) {
  auto f = g_torpRuns.find(t);
  if (f == g_torpRuns.end() || f->second.item.empty() || f->second.ended) return;
  TorpRun& r = f->second;
  if (*(bool*)((char*)t + g_tp.pExploded)) { r.ended = r.exploded = true; g_torpStats[r.item].exploded++; TorpRunReport(r); return; }
  void* body = *(void**)((char*)t + g_tp.pBody); if (!body) return;
  void* rp = Invoke(g_tp.rbPos, body); void* rv = Invoke(g_tp.rbVel, body); if (!rp || !rv) return;
  V3 p = *(V3*)((char*)rp + 0x10), v = *(V3*)((char*)rv + 0x10);
  if (r.util) { if (*(float*)((char*)t + g_tp.pInWater) > 0) UtilLanded(t, r, p); return; }   // a smoke pot or raft: no run of its own
  // the flat path run, summed the way the game sums trackedDistance for its silent range despawn (once in the water a
  // second); the mod's own sum, because the game keeps it only on the torpedo's owner and every co-op peer must lock alike
  if (*(float*)((char*)t + g_tp.pInWater) > 1.0f) { if (r.pathSet) r.path += sqrtf((p.x - r.last.x) * (p.x - r.last.x) + (p.z - r.last.z) * (p.z - r.last.z)); r.pathSet = true; }
  r.last = p;
  float sp = sqrtf(v.x * v.x + v.z * v.z);
  r.speed = sp;
  if (sp > 1) { r.heading = { v.x / sp, 0, v.z / sp }; TorpStat& s = g_torpStats[r.item]; if (sp > s.topSpeed) s.topSpeed = sp; }
  if (r.seek > 0) { float dt = KcFloat(g_tp.fdt); if (dt > 0 && dt < 0.2f) { TorpSteer(t, r, body, p, v, dt); TorpDepth(t, r, dt); } }
}
// Diving on a submarine: every physics step the game holds a torpedo at -idleDepth before its hit test (Torpedo.FixedUpdate
// 0x1809dbe7a), and only the radar branch (off on the mod's copies) moves that toward a target. So a torpedo homing on a
// ship more than 3 m down within 500 m gets its own type copy whose idleDepth the mod lowers toward the target, 8 m a
// game second, at most 60 m; losing the target brings it back up, and at its own depth it runs on its item's type again.
struct TorpDive { void* type = nullptr; void* h = nullptr; void* from = nullptr; };
static std::unordered_map<void*, TorpDive> g_torpDives;             // Torpedo -> its diving copy
static void TorpDiveDrop(void* t, TorpDive& D) {                    // the torpedo back on its item's type, the copy destroyed
  if (!D.type) return;
  if (Alive(t) && *(void**)((char*)t + g_tp.pType) == D.type && Alive(D.from)) SetRef(t, (char*)t + g_tp.pType, D.from);
  g_torpOfType.erase(D.type);
  if (Alive(D.type) && g_mapi.destroy) { void* a[1] = { D.type }; Invoke(g_mapi.destroy, nullptr, a); }
  Unpin(D.h); D = TorpDive();
}
static void TorpDepth(void* t, TorpRun& r, float dt) {
  if (g_tp.yIdle < 0 || !r.itemType || r.diveFailed || r.baseDepth <= 0) return;
  float want = r.baseDepth;
  if (r.target && !r.gaveUp && r.aimY < -3.0f && r.aimD < 500.0f) want = (std::min)(60.0f, (std::max)(r.baseDepth, -r.aimY));
  float step = 8.0f * dt;
  r.depth = want > r.depth ? (std::min)(want, r.depth + step) : (std::max)(want, r.depth - step);
  void* cur = *(void**)((char*)t + g_tp.pType);
  if (r.depth <= r.baseDepth + 0.01f) { r.depth = r.baseDepth; if (cur != r.itemType) SetRef(t, (char*)t + g_tp.pType, r.itemType); return; }
  TorpDive& D = g_torpDives[t];
  if (!Alive(D.type) || D.from != r.itemType) {
    TorpDiveDrop(t, D);
    void* c = CloneObject(r.itemType);
    if (!c) { r.diveFailed = true; Log("torpedoes: '%s' could not get a diving copy of its type; it stays at its depth", r.item.c_str()); return; }
    if (g_tp.hideFlags) { int keep = 32; void* a[1] = { &keep }; Invoke(g_tp.hideFlags, c, a); }   // DontUnloadUnusedAsset
    D.type = c; D.h = Pin(c); D.from = r.itemType;
    auto e = g_torpOfType.find(r.itemType); if (e != g_torpOfType.end()) g_torpOfType[c] = e->second;   // still that found torpedo
  }
  *(float*)((char*)D.type + g_tp.yIdle) = r.depth;
  if (cur != D.type) SetRef(t, (char*)t + g_tp.pType, D.type);
  if (r.depth > r.maxDepth) r.maxDepth = r.depth;
  if (!r.dived) {
    r.dived = true; g_torpStats[r.item].dives++;
    if (g_torpLogs++ < 20) Log("torpedoes: '%s' dives after '%s', %.0f m down and %.0f m away (it runs %.1f m down)", r.item.c_str(), r.targetName.c_str(), -r.aimY, r.aimD, r.baseDepth);
  }
}
static void (*g_torpFixedOrig)(void*, void*) = nullptr;
// The found torpedoes are off: a pooled torpedo fires stock again. A smoke pot or raft (its tier's item) keeps its
// zero-damage type instead and returns true, so Hooked_TorpFixed takes it out of the water rather than letting it sink
// and go off as a stock depth charge behind the boat; it lays no smoke and puts out no raft then.
static bool TorpRestoreStock(void* t) {
  void* cur = *(void**)((char*)t + g_tp.pType);
  void* tierType = TierType(*(void**)((char*)t + g_tp.pTier));
  const Item* ti = ItemOfTorpType(tierType);
  bool util = ti && UtilKind(*ti, nullptr) != 0;                      // a smoke pot, raft or Dave
  void* want = util ? tierType : nullptr;
  if (!want) { auto c = g_torpOfType.find(cur); if (c == g_torpOfType.end() || !c->second->stock) return false; want = c->second->stock; }
  if (want != cur) {
    SetRef(t, (char*)t + g_tp.pType, want);
    *(float*)((char*)t + g_tp.pDetonate) = *(float*)((char*)want + g_tp.yDetonation);
    *(float*)((char*)t + g_tp.pArm) = *(float*)((char*)want + g_tp.yArming);
  }
  return util;
}
static void Hooked_TorpFixed(void* self, void* method) {
  bool run = g_ready && g_tp.ok && g_torpRunFaults >= 0, util = false;
  if (run) { __try { TorpBegin(self); } __except (FaultFilter("found torpedoes (launch)", GetExceptionInformation())) { g_torpRunFaults = -1; run = false; } }
  else if (g_ready && g_tp.ok && g_torpRunFaults > -2) { __try { util = TorpRestoreStock(self); } __except (EXCEPTION_EXECUTE_HANDLER) { g_torpRunFaults = -2; } }
  g_torpFixedOrig(self, method);
  if (run) { __try { TorpAfter(self); } __except (FaultFilter("found torpedoes (homing)", GetExceptionInformation())) { g_torpRunFaults = -1; } }
  else if (util) {                                                     // a utility charge with the found torpedoes off: off the water
    __try {
      if (*(float*)((char*)self + g_tp.pInWater) > 0) {
        void* go = Invoke(m_getGameObject, self); bool off = false; void* a[1] = { &off };
        if (go) Invoke(m_setActive, go, a);
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_torpRunFaults = -2; }
  }
}

