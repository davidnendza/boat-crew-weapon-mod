// Big Brother, helpers' looks, Kamikaze Gull steering (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Big Brother ------------------------------------------------------------------------------------------------------
// The Fletcher arrives like the game's boat call-ins (FactionCallinSpawner): CallInManager.instance.callinMethods
// .setSurfaceSpawnBySearch(team, distance) finds free sea, then callInBoat(prefab, team) (0x180945630) spawns it there.
// Without a CallinMethods, or if the call fails, SpawnFriendly places it kBigBrotherDist right of the line of fire.
static const float kBigBrotherDist = 300.0f;
static int g_bbLogs = 0;                                              // destroyer events logged in full this battle (first 3)
static float HullHp(void* ent) {
  static void* mGetHp = nullptr; if (!mGetHp) { void* hp = FindClass("Core", "HealthPart"); mGetHp = hp ? FindMethod(hp, "get_HP", 0) : nullptr; }
  void* hull = ent && g_offCeHull >= 0 ? *(void**)((char*)ent + g_offCeHull) : nullptr; float sum = 0;
  for (int i = 0; i < ListCount(hull) && mGetHp; i++) { void* part = ListAt(hull, i); void* h = part ? Invoke(mGetHp, part) : nullptr; if (h) sum += *(float*)((char*)h + 0x10); }
  return sum;
}
static void* BigBrotherCall(V3 boat, V3 flat, bool forOther, bool* asCallin) {
  *asCallin = false;
  void* pooler = EntityPooler(); if (!pooler) return nullptr;
  const char* want = "Fletcher Callin";                                // the call-in card's own prefab first, then any Fletcher
  void* prefab = FindEntityPrefab(pooler, want); if (!prefab) prefab = FindEntityPrefab(pooler, want = "Fletcher");
  if (!prefab) return nullptr;
  static void *mgrInst = nullptr, *mSearch = nullptr, *mCall = nullptr; static int offCm = -2, offCenter = -1;
  if (offCm == -2) {
    void* cmK = FindClass("Core", "CallInManager"); void* cmM = FindClass("Core", "CallinMethods");
    mgrInst = cmK ? il2cpp_class_get_field_from_name(cmK, "instance") : nullptr;
    offCm = FieldOffset(cmK, "callinMethods");
    offCenter = FieldOffset(cmM, "spawnCenter");
    mSearch = cmM ? FindMethod(cmM, "setSurfaceSpawnBySearch", 2) : nullptr; mCall = cmM ? FindMethod(cmM, "callInBoat", 2) : nullptr;
    Log("big brother: CallInManager.instance %p callinMethods +0x%X spawnCenter +0x%X setSurfaceSpawnBySearch %p callInBoat %p", mgrInst, offCm, offCenter, mSearch, mCall);
  }
  void* craft = PlayerCraft();
  int team = craft && g_offLeTeam >= 0 ? *(int*)((char*)craft + g_offLeTeam) : 0;
  void* mgr = nullptr; if (mgrInst) il2cpp_field_static_get_value(mgrInst, &mgr);
  void* cm = mgr && offCm >= 0 ? *(void**)((char*)mgr + offCm) : nullptr;
  if (!forOther && cm && mSearch && mCall && offCenter >= 0 && *(void**)((char*)cm + offCenter)) {   // the host's own call: its boat is the player craft
    float d = kBigBrotherDist; void* sa[2] = { &team, &d };
    void* exc = nullptr; il2cpp_runtime_invoke(mSearch, cm, sa, &exc);
    void* ent = nullptr;
    if (!exc) { void* ca[2] = { prefab, &team }; ent = Invoke(mCall, cm, ca); }
    if (ent) { *asCallin = true; return ent; }
    Log("big brother: the call-in route failed (%s); placing the destroyer by hand", exc ? "setSurfaceSpawnBySearch threw" : "callInBoat gave nothing");
  }
  V3 side{ flat.z, 0, -flat.x };                                       // to the right of where the gun is firing
  V3 p{ boat.x + side.x * kBigBrotherDist, boat.y, boat.z + side.z * kBigBrotherDist };
  return SpawnFriendly(want, p, flat, 1.0f, 0);
}
// ---- helpers' looks --------------------------------------------------------------------------------------------------
// Helpers are pooled entities and the game never resets a transform's scale, so a helper gets its own size (and look)
// back whenever it leaves: sent home, back in its pool, reused by the game, or at battle end.
// Kamikaze Gull planes wear a gull_bomber.obj copy on the root with the plane's meshes hidden (radar blip kept).
// On a co-op client the host's LOOK message names the gulls; their size is left to CoopLooksBack.
// ai: the plane's PlaneControllerAI while a steered gull has it switched off (GullSteer); it is switched on again here too.
static const float kGullPlaneScale = 0.6f;                            // the gull planes' size against a Dauntless
struct HelperLook { void* ent; void* handle; V3 scale; bool restoreScale; void* holder; void* holderHandle; bool sawDead; void* ai = nullptr; };
static std::vector<HelperLook> g_helperLooks;
static int g_gullsDressed = 0, g_looksBack = 0;
static void GullAiEnable(void* ai, bool on) {                         // Behaviour.set_enabled on a plane's PlaneControllerAI
  static void* mSetEn = nullptr; if (!mSetEn) { void* b = FindClass("UnityEngine", "Behaviour"); mSetEn = b ? FindMethod(b, "set_enabled", 1) : nullptr; }
  if (ai && mSetEn && *(void**)((char*)ai + 0x10)) { void* a[1] = { &on }; Invoke(mSetEn, ai, a); }   // m_CachedPtr: still a live object
}

// Hangs a copy of gull_bomber.obj on a gull plane and hides the plane's own meshes. Returns the copy, or null (the
// plane then keeps the Dauntless look).
static void* DressAsGull(void* ent) {
  static bool told = false, missing = false;
  if (g_modelsOff || !ModelApiReady() || !m_setActive || !g_setParentTwoArgs) return nullptr;
  std::string path = g_folder + "\\models\\gull_bomber.obj";
  if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    if (!missing) { missing = true; Log("helpers: %s not found; gull planes keep the Dauntless look", path.c_str()); }
    return nullptr;
  }
  void* entT = Invoke(m_getTransform, ent); if (!entT) return nullptr;
  void* root = LoadModelCopy(path, "gull plane"); if (!root) return nullptr;
  // 'Radar Display 3D' is the radar blip (a 160 m mesh 50 m below the plane): left shown, and left out of any centring.
  // The gull sits at a fixed spot on the plane's root instead (+Z nose, +Y up, like the .obj).
  float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }; int hidden = 0, kept = 0;
  std::vector<void*> rs; CollectRenderersExcept(entT, 0, { "Radar Display" }, rs, &kept);
  for (void* r : rs) {
    if (!IsMeshRenderer(r)) continue;
    if (void* b = Invoke(g_mapi.bounds, r)) {                          // Bounds: centre, extents (world); only for the log
      const float* c = (const float*)((char*)b + 0x10);
      for (int k = 0; k < 3; k++) { lo[k] = (std::min)(lo[k], c[k] - c[k + 3]); hi[k] = (std::max)(hi[k], c[k] + c[k + 3]); }
    }
    bool off = true; void* a[1] = { &off }; Invoke(g_mapi.forceOff, r, a); hidden++;
  }
  V3 centre = { 0, 0, -0.8f };                                         // root's local units: the gull's wings on the Dauntless's
  void* rootT = Invoke(g_mapi.goTransform, root);
  bool keepWorld = false; void* pa[2] = { entT, &keepWorld }; Invoke(m_setParent, rootT, pa);
  V3 sc = { 1, 1, 1 };                                                 // the .obj is a full-size Dauntless; it takes the plane's own scale
  Q4 ident = { 0, 0, 0, 1 };
  void* a1[1] = { &centre }; Invoke(m_setLocalPos, rootT, a1);
  void* a2[1] = { &ident }; Invoke(m_setLocalRot, rootT, a2);
  void* a3[1] = { &sc }; Invoke(m_setLocalScale, rootT, a3);
  bool on = true; void* a4[1] = { &on }; Invoke(m_setActive, root, a4);
  g_gullsDressed++;
  if (!told) {
    told = true;
    V3 ls = GetV3(g_mapi.lossy, entT);
    Log("helpers: gull planes wear gull_bomber.obj (%d plane meshes hidden, spanning %.1f x %.1f x %.1f m; %d radar blip part(s) left shown; the gull hangs at %.2f, %.2f, %.2f on the plane's root, scale 1 of the plane's %.2f)",
        hidden, hidden ? hi[0] - lo[0] : 0, hidden ? hi[1] - lo[1] : 0, hidden ? hi[2] - lo[2] : 0, kept, centre.x, centre.y, centre.z, ls.x);
  }
  return root;
}
static void AddHelperLook(void* ent, V3 scale0, bool restoreScale, bool gull) {
  void* holder = gull ? DressAsGull(ent) : nullptr;
  if (!restoreScale && !holder) return;
  g_helperLooks.push_back({ ent, Pin(ent), scale0, restoreScale, holder, holder ? Pin(holder) : nullptr, false });
}
// A steered gull's AI is off (ai) or back on (null): kept on its look so every way a look ends switches it on again.
static void HelperLookAi(void* ent, void* ai) {
  for (HelperLook& L : g_helperLooks) if (L.ent == ent) { L.ai = ai; return; }
  if (ai) g_helperLooks.push_back({ ent, Pin(ent), V3{ 0, 0, 0 }, false, nullptr, nullptr, false, ai });
}
static void RestoreHelperLook(HelperLook& L, const char* why) {
  void* entT = Invoke(m_getTransform, L.ent);
  if (L.holder) {
    if (*(void**)((char*)L.holder + 0x10)) {                           // m_CachedPtr: not destroyed with the plane already
      if (g_mapi.destroy) { void* a[1] = { L.holder }; Invoke(g_mapi.destroy, nullptr, a); }
      else { bool off = false; void* a[1] = { &off }; Invoke(m_setActive, L.holder, a); }
    }
    Unpin(L.holderHandle);
    std::vector<void*> rs; int kept = 0; if (entT) CollectRenderersExcept(entT, 0, { "Radar Display" }, rs, &kept);
    for (void* r : rs) if (IsMeshRenderer(r)) { bool off = false; void* a[1] = { &off }; Invoke(g_mapi.forceOff, r, a); }
  }
  if (L.ai) { GullAiEnable(L.ai, true); L.ai = nullptr; }             // a steered gull's own AI back on: a later call-in flies normally
  if (L.restoreScale && entT) { void* a[1] = { &L.scale }; Invoke(m_setLocalScale, entT, a); }
  Unpin(L.handle);
  g_looksBack++;
  DLog("helpers: helper %p has its own %s back (%s)", L.ent, L.holder ? "look and size" : "size", why);
}
static void ReleaseHelperLook(void* ent, const char* why) {
  for (size_t i = 0; i < g_helperLooks.size(); i++)
    if (g_helperLooks[i].ent == ent) { RestoreHelperLook(g_helperLooks[i], why); g_helperLooks.erase(g_helperLooks.begin() + i); return; }
}
static void RestoreAllHelperLooks(const char* why) {
  for (HelperLook& L : g_helperLooks) RestoreHelperLook(L, why);
  g_helperLooks.clear();
}
static void SafeRestoreAllHelperLooks() {
  __try { RestoreAllHelperLooks("a fault"); }
  __except (EXCEPTION_EXECUTE_HANDLER) { g_helperLooks.clear(); }
}
// Every frame: a helper that went back to its pool, or that the game has handed out again after it died, is undone.
static void HelperLooksTick() {
  static void* mActive = nullptr;
  if (g_helperLooks.empty()) return;
  if (!mActive) { void* go = FindClass("UnityEngine", "GameObject"); mActive = go ? FindMethod(go, "get_activeSelf", 0) : nullptr; }
  for (size_t i = 0; i < g_helperLooks.size(); ) {
    HelperLook& L = g_helperLooks[i];
    void* go = Invoke(m_getGameObject, L.ent); void* ab = go && mActive ? Invoke(mActive, go) : nullptr;
    bool active = ab && *(bool*)((char*)ab + 0x10);
    bool alive = g_offLeAlive >= 0 && *(bool*)((char*)L.ent + g_offLeAlive);
    const char* why = !active ? "back in its pool" : (alive && L.sawDead ? "the game used it again" : nullptr);
    if (!alive) L.sawDead = true;
    if (!why) { i++; continue; }
    RestoreHelperLook(L, why);
    g_helperLooks.erase(g_helperLooks.begin() + i);
  }
}

// A gull's splash: who it was aimed at, so UnusualKill can tell the gull's kills from the gun's own (both are the gun's).
struct GullBlast { void* mount; void* target; V3 at; DWORD t; };
static std::vector<GullBlast> g_gullBlasts;
// A steered gull lets go: its AI back on (the game's own dying or flying plane), its target unpinned.
static void GullRelease(Minion& m) {
  if (m.ai) { GullAiEnable(m.ai, true); HelperLookAi(m.ent, nullptr); m.ai = nullptr; }
  if (m.tHandle) { Unpin(m.tHandle); m.tHandle = nullptr; } m.target = nullptr;
}
// A gull's credited splash (25 m, 600 damage) at p.
static void QueueGullSplash(Minion& m, V3 p, void* target) {
  void* ent = m.ent; void* ws = m.mount ? *(void**)((char*)m.mount + 0x90) : nullptr;   // GunMount.weaponSystem: the kill is the gun's
  bool queued = ws && p.y > -5000 && QueueSplash(p, 25.0f, 600.0f, ws, g_offLeEntityCode >= 0 ? *(int*)((char*)ent + g_offLeEntityCode) : 0, *(int*)((char*)ent + g_offLeTeam));
  if (queued) { if (g_gullBlasts.size() > 32) g_gullBlasts.erase(g_gullBlasts.begin()); g_gullBlasts.push_back({ m.mount, target, p, GetTickCount() }); }
  DLog("extra effects: gull %p exploded at %.0f,%.0f,%.0f (splash %s)", ent, p.x, p.y, p.z, queued ? "queued" : "NOT queued");
}
// splashAt: where a gull's splash goes (its aim point on the target); the plane's own position if null
static void Despawn(Minion& m, bool explode, const V3* splashAt = nullptr, void* target = nullptr) {
  void* ent = m.ent;
  GullRelease(m);
  if (!explode) ReleaseHelperLook(ent, "sent home");                   // before the pool can hand it out again
  if (explode) {
    // a credited splash, then the plane is destroyed (its own crash effect): hull parts to 0
    QueueGullSplash(m, splashAt ? *splashAt : PosOf(ent), target);
    static void* mSetHp = nullptr; if (!mSetHp) { void* hp = FindClass("Core", "HealthPart"); mSetHp = hp ? FindMethod(hp, "set_HP", 1) : nullptr; }
    void* hull = *(void**)((char*)ent + g_offCeHull); float zero = 0;
    for (int i = 0; i < ListCount(hull) && mSetHp; i++) { void* part = ListAt(hull, i); if (part) { void* a[1] = { &zero }; Invoke(mSetHp, part, a); } }
  } else {
    void* po = g_offLePool >= 0 ? *(void**)((char*)ent + g_offLePool) : nullptr;
    static void* mRepool = nullptr; if (!mRepool) { void* k = FindClass("", "PoolableObject"); mRepool = k ? FindMethod(k, "repool", 0) : nullptr; }
    if (po && mRepool) Invoke(mRepool, po);
    else { void* go = Invoke(m_getGameObject, ent); bool off = false; void* a[1] = { &off }; if (go) Invoke(m_setActive, go, a); }
  }
  Unpin(m.handle);
}

// ---- Kamikaze Gull steering -------------------------------------------------------------------------------------------
// The game's dive bomber AI (PlaneControllerAI) only dives from over 1800 m out and rewrites FlightObject.targetCoordinate
// every physics step, so it never hits within a gull's 25 s life. A gull with an enemy within 2 km has its AI disabled and
// is flown by the mod: targetCoordinate (nose) and resetVelocity every frame, toward the target's aim point plus lead.
// The AI goes back on in GullRelease and RestoreHelperLook.
static int g_gullFull = 0;                                             // gulls logged in full this battle (the first 3)
static V3 AimPointOn(void* ent) {
  V3 o = PosOf(ent);
  if (KindOf(ent).ship) { o.y += ShipAimHeight(ent, o); return o; }
  V3 g = GroundAimOffset(ent, o); return V3{ o.x + g.x, o.y + g.y, o.z + g.z };
}
static float Dist3(V3 a, V3 b) { float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z; return sqrtf(x * x + y * y + z * z); }
static const char* ClassOf(void* o) { return o ? il2cpp_class_get_name(il2cpp_object_get_class(o)) : "?"; }
// Returns 1 when the gull should blow up on its target (at m.aim), 2 when it is about to hit the sea away from it.
static int GullSteer(Minion& m, UnuStats& st, V3 p, float now, DWORD tick) {
  static bool init = false, ready = false; static void *mGetAi = nullptr, *mGetFo = nullptr, *mReset = nullptr; static int offTc = -1, offUta = -1, offTVel = -1;
  if (!init) {
    init = true;
    void* pl = FindClass("Core", "Plane"); mGetAi = pl ? FindMethod(pl, "get_PlaneController", 0) : nullptr; mGetFo = pl ? FindMethod(pl, "get_FlightObject", 0) : nullptr;
    void* fl = FindClass("Cotanius.Modules.HybridFlight", "FlightObject"); mReset = fl ? FindMethod(fl, "resetVelocity", 1) : nullptr;
    offTc = FieldOffset(fl, "targetCoordinate");
    offUta = FieldOffset(fl, "useTargetAlitude");
    void* le = FindClass("Core", "LiveEntity"); offTVel = FieldOffset(le, "realtimeVelocity");
    ready = mGetAi && mGetFo && mReset && offTc >= 0 && g_offLeAlive >= 0;
    Log("kamikaze gull: steering %s (get_PlaneController %p get_FlightObject %p resetVelocity %p targetCoordinate +0x%X useTargetAlitude +0x%X realtimeVelocity +0x%X)",
        ready ? "ready" : "MISSING PIECES, gulls fly on the game's dive bomber AI", mGetAi, mGetFo, mReset, offTc, offUta, offTVel);
  }
  if (!ready) return 0;
  float dt = m.lastT >= 0 ? now - m.lastT : 0; if (dt < 0 || dt > 0.5f) dt = 0; m.lastT = now;
  if (dt > 0 && !m.ai) { V3 v{ (p.x - m.lastPos.x) / dt, (p.y - m.lastPos.y) / dt, (p.z - m.lastPos.z) / dt }; float s = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); if (s > 20 && s < 400) m.vel = v; }
  m.lastPos = p;
  if (m.target && (!*(bool*)((char*)m.target + g_offLeAlive) || PosOf(m.target).y < -5000)) { Unpin(m.tHandle); m.tHandle = nullptr; m.target = nullptr; m.nearest = m.lastD = 1e30f; }
  if (!m.target && tick - m.lastTry > 500) {
    m.lastTry = tick; float at[3] = { p.x, p.y, p.z }; V3 q;
    void* t = NearestEnemyTarget(at, m.steered ? 1200.0f : 2000.0f, &q);   // boats and shore guns: no planes, nothing under water
    if (t) {
      m.target = t; m.tHandle = Pin(t); float d = Dist3(p, AimPointOn(t));
      if (!m.steered) {
        m.steered = true; st.gullSteered++; m.fromD = d;
        if (g_gullFull < 3) { g_gullFull++; m.full = true; }
        if (m.full) Log("kamikaze gull: '%s' gull dove at %s '%s' from %.0f m", st.name.c_str(), ClassOf(t), NameOf(t).c_str(), d);
        DWORD need = (DWORD)(d / 120.0f * 1000) + 4000, age = (DWORD)(m.age * 1000);   // game time to get there, within 35 s in all
        if (age + need > m.life) m.life = (std::min)(age + need, (DWORD)35000);
      } else if (m.full) Log("kamikaze gull: '%s' gull's target is gone; it turned to %s '%s' %.0f m away", st.name.c_str(), ClassOf(t), NameOf(t).c_str(), d);
    }
  }
  if (!m.target) { if (m.ai) { GullAiEnable(m.ai, true); HelperLookAi(m.ent, nullptr); m.ai = nullptr; } return 0; }   // the game flies it
  if (!m.ai) {
    void* ai = Invoke(mGetAi, m.ent);
    if (ai) { GullAiEnable(ai, false); m.ai = ai; HelperLookAi(m.ent, ai); }
    if (!m.fo) m.fo = Invoke(mGetFo, m.ent);
    if (m.speed <= 0) { float s = sqrtf(m.vel.x * m.vel.x + m.vel.y * m.vel.y + m.vel.z * m.vel.z); m.speed = s < 70 ? 90.0f : (s > 160 ? 160.0f : s); }
    static bool told = false;
    if (!told && m.fo) { told = true; Log("kamikaze gull: the first steered gull's FlightObject has useTargetAlitude %d (1 would ignore targetCoordinate's height), AI %p", offUta >= 0 ? *(bool*)((char*)m.fo + offUta) : -1, ai); }
  }
  if (!m.fo) return 0;
  m.aim = AimPointOn(m.target);
  float d = Dist3(p, m.aim);
  if (d < m.nearest) m.nearest = d;
  if (d < 14) return 1;                                                // there
  if (m.nearest < 40 && d > m.nearest + 4) return 1;                   // came close and is passing it (vs its closest, not last frame: any frame rate)
  m.lastD = d;
  if (p.y + m.vel.y * 0.15f < 3.0f) return d < 60 ? 1 : 2;             // the sea next (a plane under 3.5 m splashes)
  m.speed = (std::min)(140.0f, (std::max)(m.speed, 90.0f) + 20.0f * dt);
  V3 tv = offTVel >= 0 ? *(V3*)((char*)m.target + offTVel) : V3{ 0, 0, 0 };
  float lead = (std::min)(d / m.speed, 4.0f);
  V3 to{ m.aim.x + tv.x * lead - p.x, m.aim.y + tv.y * lead - p.y, m.aim.z + tv.z * lead - p.z };
  if (d > 250) to.y += (std::min)(0.15f * d, 200.0f);                  // far out: a little high, so it comes down in a dive
  float tl = sqrtf(to.x * to.x + to.y * to.y + to.z * to.z); if (tl < 0.01f) return 1;
  V3 want{ to.x / tl, to.y / tl, to.z / tl };
  float vl = sqrtf(m.vel.x * m.vel.x + m.vel.y * m.vel.y + m.vel.z * m.vel.z);
  V3 cur = vl > 1 ? V3{ m.vel.x / vl, m.vel.y / vl, m.vel.z / vl } : want, dir = want;
  float cs = cur.x * want.x + cur.y * want.y + cur.z * want.z; cs = cs > 1 ? 1 : (cs < -1 ? -1 : cs);
  float ang = acosf(cs), maxA = (d < 150 ? 220.0f : 110.0f) * 0.0174533f * dt;
  if (ang > maxA && ang > 1e-4f) {                                     // turn toward it by maxA (slerp)
    float sa = sinf(ang);
    if (sa < 1e-3f) dir = V3{ cur.x, cur.y + 0.1f, cur.z };            // straight behind: pull up and round
    else { float a = sinf(ang - maxA) / sa, b = sinf(maxA) / sa; dir = V3{ cur.x * a + want.x * b, cur.y * a + want.y * b, cur.z * a + want.z * b }; }
    float l = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z); dir = V3{ dir.x / l, dir.y / l, dir.z / l };
  }
  m.vel = V3{ dir.x * m.speed, dir.y * m.speed, dir.z * m.speed };
  float* tc = (float*)((char*)m.fo + offTc); tc[0] = p.x + dir.x * 300; tc[1] = p.y + dir.y * 300; tc[2] = p.z + dir.z * 300;   // the nose
  float vel[3] = { m.vel.x, m.vel.y, m.vel.z }; void* va[1] = { vel }; Invoke(mReset, m.fo, va);
  return 0;
}
