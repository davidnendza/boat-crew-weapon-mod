// critical hits (Deadeye), smoking barrels, CritScan (the per-frame shot scan) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- critical hits ('Deadeye') -------------------------------------------------------------------------------------
// GunMount.shoot gives each shot Projectile.damageMultiplier (+0x84); ProjectileManager.endJobs, called from
// ProjectileManager.Update, resolves hits with damage = BaseDamage x damageMultiplier (splash ignores it). So at the start
// of ProjectileManager.Update every shot that appeared since the last frame is new and has not hit anything yet: a shot
// from a Deadeye gun rolls critChance and has its multiplier scaled by critMult. fx.set.nthShot=N (The Metronome) makes
// every Nth shot of a mount a sure critical hit of nthMult instead.
static int g_offPmLive = -2, g_offPjFrom = -1, g_offPjDmg = -1, g_offPjAlive = -1;
// Shots are pooled: a shell that hits can be fired again, by any gun, before the next frame, so the same object is a
// new shot. A life is told apart by its gun (shotFrom) and Projectile.alive, which shoot() sets to the full life and
// iterate() counts down: the same object with a higher alive is a new shot.
struct ShotSig { void* from; float alive; unsigned frame; };
// every pooled shell object seen this battle, with the frame it was last live: kept, not rebuilt each frame (the pool is
// bounded, so after the first busy seconds no frame allocates)
static std::unordered_map<void*, ShotSig> g_shots;
static unsigned g_shotFrame = 0;
static std::unordered_set<void*> g_chainBoost;                       // follow-up shots from chaining: steered at >= 360 deg/s
static bool SameShot(const ShotSig& was, void* from, float alive) { return was.from == from && alive <= was.alive + 1e-4f; }
// Co-op: a shell another player fired, as this machine shows it. The shot sync fires the gun of its own copy of that
// boat and clears Projectile.networkOwner (+0xAC; reset() sets it for every shell fired here), and such a shell deals no
// damage: the shooter's machine does that. Its fragments inherit the flag (explode). Never true in single player.
static int g_offPjNetOwner = -1;
static bool IsCopyShot(void* pj) { return g_coopRole != 0 && g_offPjNetOwner >= 0 && !*(bool*)((char*)pj + g_offPjNetOwner); }   // never in single player
static int g_copyShots = 0, g_copyChains = 0;                         // other players' shots with mod effects, per battle
static std::unordered_map<void*, bool> g_copyHasItem;                 // their gun mount -> it is one of the mod's guns, per battle
static void SafeUnusualRecycled(void* pj);
struct CritCfg { float chance, mult; int nth; float nthMult; int shots; };
static std::unordered_map<void*, CritCfg> g_critCache;               // mount -> settings and shot count, per battle
static void* g_critEng = nullptr;
static int g_critCount = 0;
static DWORD g_critMsgAt = 0;
static float EliteDamageOf(void* mount);
static void AllKnowingShot(void* mount);
static void SafeUnusualOnShot(void* pj, void* mount);
// gun effects (the 'gun effects' section after the on-kill effects): each own new shot, and the battle change
static void SafeGfxShot(void* pj, void* mount, bool leap);
static void SafeGfxBattleChange(bool hadBattle);
// Marking: ships marked by the player's hits, drawn by EliteTags (per machine: co-op players each mark their own)
struct MarkInfo { DWORD until; float bonus; std::string by, ship; };   // by: the gun that marked it; ship: ShipTitle
static std::unordered_map<void*, MarkInfo> g_marks;                   // CombatEntity -> its mark, per battle

// The mod's turret guns in battle: shots and kills per gun, a line per kill (the first few) and a summary when the
// battle ends, because in a fight nobody can tell which gun did what. Automated (crewless) guns on the player's boat
// too, counting apart the shots the game fired at a target (Projectile.target is set when the shell appears, before
// the homing code looks at it), so the log shows whether such a gun fires on its own.
struct TurretStats { std::string name; bool automated = false, sideGrade = false; int shots = 0, kills = 0, aimed = 0, atDead = 0, lobN = 0, skyMoved = 0; double lobSum = 0, flightSum = 0, skyLeadSum = 0; };   // sideGrade: one of kSideGrades (or found on one)
static std::unordered_map<void*, TurretStats> g_turretStats;
static std::unordered_map<void*, bool> g_isTurret;                    // mount -> tracked here; per battle
static int g_turretFaults = 0;
static bool OnPlayerBoat(void* mount) {
  static int off = -2;
  if (off == -2) off = FieldOffset("Core", "GunMount", "entity");
  void* craft = off >= 0 ? PlayerCraft() : nullptr;
  return craft && *(void**)((char*)mount + off) == craft;
}
static bool ShotHasTarget(void* pj) {
  static int off = -2;
  if (off == -2) off = FieldOffset("Core", "Projectile", "target");
  return off >= 0 && *(void**)((char*)pj + off);
}
// Automated shots: the launch angle, an estimated flight time (2 vy / 9.81, no drag) and whether the target was already
// dead (LiveEntity._alive; the fire control drops dead ships itself, fillCasters code 7).
// Sky shots: a GunType with skyShot (+0x70, the player mortars) spawns its shell at GunMount.skyShotPosition (+0x1C0),
// falling straight down, at about the impact height, so it lands within a frame (GunMount.shoot 0x18097039f). Only the
// player's map aim writes that point, never the fire control, so AutoSkyAim sets it; this logs how far off they land.
static V3 PosOf(void* component);
static void AutoShotInfo(void* pj, TurretStats* ts) {
  static int offTgt = -2, offVel = -1, offAlive = -1, offPos = -1;
  if (offTgt == -2) {
    offTgt = FieldOffset("Core", "Projectile", "target"); offVel = FieldOffset("Core", "Projectile", "velocity"); offAlive = FieldOffset("Core", "LiveEntity", "_alive");
    offPos = FieldOffset("Core", "Projectile", "posi");
  }
  void* tgt = offTgt >= 0 ? *(void**)((char*)pj + offTgt) : nullptr;
  if (!tgt || offVel < 0) return;
  bool dead = offAlive >= 0 && !*(bool*)((char*)tgt + offAlive);
  if (dead) ts->atDead++;
  void* from = g_offPjFrom >= 0 ? *(void**)((char*)pj + g_offPjFrom) : nullptr;
  if (from && !dead) SkyRemember(from, tgt);
  if (from) SkyShotFired(from);                                        // the next sky shell lands somewhere else in the disk
  float* v = (float*)((char*)pj + offVel);
  if (offPos >= 0 && v[1] < 0 && fabsf(v[0]) < 0.5f && fabsf(v[2]) < 0.5f) {   // falling straight down: a sky shot; how far off it lands
    const float* ps = (const float*)((char*)pj + offPos); V3 tp = PosOf(tgt);
    if (tp.y > -9000) {
      float miss = sqrtf((ps[0] - tp.x) * (ps[0] - tp.x) + (ps[2] - tp.z) * (ps[2] - tp.z));
      ts->skyMoved++; ts->skyLeadSum += miss;
      if (ts->skyMoved <= 3) Log("automated shot: '%s' sky shell comes down %.0f m from '%s' (%.0f m up)", ts->name.c_str(), miss, StripTags(NameOf(tgt)).c_str(), ps[1] - tp.y);
    }
  }
  float h = sqrtf(v[0] * v[0] + v[2] * v[2]), lob = atan2f(v[1], h) * 57.2958f, fly = v[1] > 0 ? 2 * v[1] / 9.81f : 0;
  ts->lobN++; ts->lobSum += lob; ts->flightSum += fly;
  if (ts->lobN <= 3 || (dead && ts->atDead <= 3))
    Log("automated shot: '%s' at '%s'%s: launched %.0f deg up at %.0f m/s, about %.0f s in the air", ts->name.c_str(), StripTags(NameOf(tgt)).c_str(), dead ? " (ALREADY DEAD)" : "", lob, sqrtf(h * h + v[1] * v[1]), fly);
}
static TurretStats* TurretOf(void* mount) {
  if (!mount) return nullptr;
  auto c = g_isTurret.find(mount);
  if (c == g_isTurret.end()) {
    const Item* it = ItemOfMount(mount);                               // found turret guns too
    bool selfFired = it && (it->family == "Rockets" || it->family == "Crew Weapons");   // need no gunner anyway: not 'automated'
    c = g_isTurret.emplace(mount, it && (it->builtIn || !it->builtFrom.empty() || (!selfFired && CrewlessMount(mount) == 1 && OnPlayerBoat(mount)))).first;
  }
  if (!c->second) return nullptr;
  TurretStats& s = g_turretStats[mount];
  if (s.name.empty()) {
    s.name = StripTags(NameOf(mount)); s.automated = CrewlessMount(mount) == 1;
    const Item* it = ItemOfMount(mount);
    s.sideGrade = it && (it->builtIn || !it->builtFrom.empty()) && it->family != "Tub Turret";
    if (it && (it->family == "Rockets" || it->family == "Crew Weapons")) s.automated = false;   // launchers and crew weapons need no gunner anyway
  }
  return &s;
}
static void TurretKill(void* mount, void* killed) {
  if (!killed || g_turretFaults < 0) return;
  TurretStats* s = TurretOf(mount); if (!s) return;
  static void* seen[16][2]; static DWORD seenAt[16]; static int seenPos = 0;   // the gun and the gunner both report a kill
  DWORD now = GetTickCount();
  for (int i = 0; i < 16; i++) if (seen[i][0] == killed && seen[i][1] == mount && now - seenAt[i] < 3000) return;
  seen[seenPos][0] = killed; seen[seenPos][1] = mount; seenAt[seenPos] = now; seenPos = (seenPos + 1) % 16;
  s->kills++;
  if (s->kills <= 5) Log("%s '%s' kill: %s '%s'", s->automated ? "automated gun" : s->sideGrade ? "side-grade" : "turret gun", s->name.c_str(), il2cpp_class_get_name(il2cpp_object_get_class(killed)), NameOf(killed).c_str());
}
static std::string SkySummary(void* mount, const TurretStats& s) {   // only for guns that dropped sky shells
  auto it = g_skySrcBy.find(mount);
  if (!s.skyMoved && it == g_skySrcBy.end()) return "";
  long long f[3] = { 0, 0, 0 }; if (it != g_skySrcBy.end()) for (int i = 0; i < 3; i++) f[i] = it->second[i];
  char b[256]; snprintf(b, sizeof b, "; %d sky shells came down %.0f m from their target on average (sky aim frames: %lld fire-control target, %lld last shell's ship, %lld aim point)",
                        s.skyMoved, s.skyMoved ? s.skyLeadSum / s.skyMoved : 0.0, f[1], f[2], f[0]);
  return b;
}
static void TurretSummary() {
  for (auto& e : g_turretStats) {
    const TurretStats& s = e.second;
    if (s.automated) Log("automated guns, last battle: '%s' fired %d shots (%d at a target the game picked, %d without one, %d at a target already dead), %d kills; launched %.0f deg up on average, about %.0f s in the air%s", s.name.c_str(), s.shots, s.aimed, s.shots - s.aimed, s.atDead, s.kills, s.lobN ? s.lobSum / s.lobN : 0.0, s.lobN ? s.flightSum / s.lobN : 0.0, SkySummary(e.first, s).c_str());
    else if (s.shots || s.kills) Log("%s, last battle: '%s' fired %d shots, %d kills", s.sideGrade ? "side-grades" : "turret guns", s.name.c_str(), s.shots, s.kills);
  }
  g_turretStats.clear(); g_isTurret.clear();
  g_skySrcBy.clear(); g_skyDisk.clear(); SkyForgetAll();             // sky aim: a new battle, new ships
}

// ---- Smoking barrels (barrel_smoke) ---------------------------------------------------------------------------------
// A gun of yours that fires steadily for barrel_smoke_after seconds smokes from its muzzle(s) for about
// barrel_smoke_seconds once it stops. Looks only, on this machine only: nothing is sent and nothing pooled is touched.
// Shots come from CritScan (SafeSmokeShot): heat is the time spent firing, each gap counted up to three shot cooldowns;
// longer gaps up to gapCap (reloads) only hold the burst together, so it does not depend on the gun's rate of fire. The
// puffs are fed by our own code into one parked copy of the game's own 'Muzzle Smoke' ParticleSystem with
// ParticleSystem.Emit(pos, vel, size, life, Color32); its emission module stays off, so only our Emit calls make
// particles. The template is the system under a player barrel's emmiter prefab (Barrel +0x40, the prefab
// Barrel.emitFlash hands to VFXManager.spawnVFX); never Barrel.pgE, the pooled live flash the game keeps at each
// muzzle. Timed in game time (Time.time), so the smoke slows with the kill cam. Co-op: other players' mod guns
// (IsRemoteMount) smoke here too when coop_visuals is on; enemy copies never do.
struct SmokeApi { void *emit, *play, *clear, *count, *goTr, *trPos, *trFwd, *trSetPos, *trSetRot, *trFind, *getComp, *psType, *pgType, *dontDestroy, *destroy, *time;
                  int offBarrels, offShoot, offLen, offCounter, offEmm, offSystems; bool tried, ok; };
static SmokeApi g_sa = {};
struct SmokeGun { std::string name; bool excluded = true, copy = false, smoking = false; float last = -1, burst = 0, peak = 0, gapCap = 1, stopDelay = 0.6f, sizeK = 1, cd = 1,
                  len = 0, smokeT0 = 0, smokeLen = 0, inten = 1, accum[2] = {}; int shots = 0, burstShots = 0, times = 0, nsp = 0, cnt0 = 0; void* sp[2] = {}; void* h[3] = {}; };
static std::unordered_map<void*, SmokeGun> g_smoke;                  // mount -> heat and smoke, per battle (CritScan's battle change)
static int g_smokePuffs = 0, g_smokeCopyTimes = 0, g_smokeLogs = 0, g_smokeMissed = 0, g_smokePeak = 0, g_smokeFaults = 0;
static bool g_smokeDead = false;                                      // a lookup failed: no smoke this session
static void *g_smokePS = nullptr, *g_smokePSH = nullptr; static int g_smokeRank = -1;   // our parked copy, and its template's rank
static void *g_smokeBest = nullptr, *g_smokeBestH = nullptr; static int g_smokeBestRank = -1; static std::string g_smokeBestName;   // best template seen
static DWORD g_smokeSearchAt = 0; static int g_smokeLooked = 0;     // the last search of every loaded ParticleGroup; guns looked at
static float g_smokeLastTick = -1, g_smokeBudget = 0;
static bool SmokeApiReady() {
  SmokeApi& a = g_sa;
  if (a.tried) return a.ok;
  a.tried = true;
  void* ps = FindClass("UnityEngine", "ParticleSystem"); void* tr = FindClass("UnityEngine", "Transform"); void* go = FindClass("UnityEngine", "GameObject");
  void* obj = FindClass("UnityEngine", "Object"); void* tm = FindClass("UnityEngine", "Time"); void* comp = FindClass("UnityEngine", "Component");
  void* pg = FindClass("Core", "ParticleGroup"); if (!pg) pg = FindClass("", "ParticleGroup");
  a.emit = ps ? FindMethod(ps, "Emit", 5) : nullptr;                   // the only Emit with 5 parameters (pos, vel, size, life, Color32)
  a.play = ps ? FindMethod(ps, "Play", 0) : nullptr; a.clear = ps ? FindMethod(ps, "Clear", 0) : nullptr; a.count = ps ? FindMethod(ps, "get_particleCount", 0) : nullptr;
  a.psType = ps ? il2cpp_type_get_object(il2cpp_class_get_type(ps)) : nullptr; a.pgType = pg ? il2cpp_type_get_object(il2cpp_class_get_type(pg)) : nullptr;
  a.goTr = go ? FindMethod(go, "get_transform", 0) : nullptr;
  a.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; a.trFwd = tr ? FindMethod(tr, "get_forward", 0) : nullptr;
  a.trSetPos = tr ? FindMethod(tr, "set_position", 1) : nullptr; a.trSetRot = tr ? FindMethod(tr, "set_rotation", 1) : nullptr; a.trFind = tr ? FindMethod(tr, "Find", 1) : nullptr;
  a.getComp = comp ? FindMethod(comp, "GetComponent", 1, "System.Type") : nullptr;
  a.dontDestroy = obj ? FindMethod(obj, "DontDestroyOnLoad", 1) : nullptr; a.destroy = obj ? FindMethod(obj, "Destroy", 1, "UnityEngine.Object") : nullptr;
  a.time = tm ? FindMethod(tm, "get_time", 0) : nullptr;
  void* gm = FindClass("Core", "GunMount");
  a.offBarrels = FieldOffset(gm, "barrels"); a.offShoot = FieldOffset(gm, "shootPoints"); a.offLen = FieldOffset(gm, "barrelLength"); a.offCounter = FieldOffset(gm, "shotCounter");
  a.offSystems = FieldOffset(pg, "systems");
  a.offEmm = 0x40;                                                     // Barrel.emmiter (dump); by name when the element class is there
  void* fb = gm ? il2cpp_class_get_field_from_name(gm, "barrels") : nullptr;
  auto elementClass = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_get_element_class");
  void* arrK = fb ? il2cpp_class_from_type(il2cpp_field_get_type(fb)) : nullptr; void* bk = arrK && elementClass ? elementClass(arrK) : nullptr;
  if (bk) { int e = FieldOffset(bk, "emmiter"); if (e >= 0) a.offEmm = e; }
  a.ok = a.emit && a.play && a.clear && a.goTr && a.trPos && a.trFwd && a.time && m_instantiate && a.offBarrels >= 0 && a.offShoot >= 0 && a.offLen >= 0;
  if (!a.ok) Log("barrel smoke: a lookup failed (Emit %p, Play %p, Clear %p, Time.time %p, get_forward %p, GunMount.barrels %d, shootPoints %d, barrelLength %d); guns do not smoke",
                 a.emit, a.play, a.clear, a.time, a.trFwd, a.offBarrels, a.offShoot, a.offLen);
  else DLog("barrel smoke: lookups ok (Barrel.emmiter +0x%X, ParticleGroup.systems %d, GunMount.shotCounter %d, set_rotation %s, Transform.Find %s)",
            a.offEmm, a.offSystems, a.offCounter, a.trSetRot ? "found" : "missing", a.trFind ? "found" : "missing");
  if (!a.ok) g_smokeDead = true;
  return a.ok;
}
static float SmokeNow() { void* tb = Invoke(g_sa.time, nullptr); return tb ? *(float*)((char*)tb + 0x10) : 0.0f; }
static float SmokeRand() { static unsigned s = 0x9E3779B9u; s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; }   // own stream, as StreakRand
// Which muzzle flash prefab makes the best smoke: the 5-inch's rises and has no velocity module (Basic's flings its
// particles sideways), then the smaller ones. -1: not a muzzle flash.
static int SmokeRankOf(std::string n) {
  const std::string suffix = "(Clone)";
  while (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0) n.erase(n.size() - suffix.size());
  if (n == "ParticleGroup_MuzzleFlash_Massive_5inch25") return 5;
  if (n == "ParticleGroup_MuzzleFlash_Basic") return 4;
  if (n == "ParticleGroup_MuzzleFlash_Basic_Enlarged1") return 3;
  if (n == "ParticleGroup_MuzzleFlash_Basic_Enlarged2") return 2;
  if (n == "ParticleGroup_MuzzleFlash_Basic_Mortar60mm") return 1;
  return n.find("Muzzle") != std::string::npos ? 0 : -1;
}
// The smoke system of a ParticleGroup: 'Muzzle Smoke' in its systems list, else any '...Smoke', else its child by name.
static void* SmokeSystemIn(void* pg) {
  void* list = g_sa.offSystems >= 0 ? *(void**)((char*)pg + g_sa.offSystems) : nullptr;
  void* other = nullptr;
  for (int i = 0; i < ListCount(list); i++) {
    void* ps = ListAt(list, i); if (!Alive(ps)) continue;
    std::string n = NameOf(ps);
    if (n.compare(0, 12, "Muzzle Smoke") == 0) return ps;
    if (!other && n.find("Smoke") != std::string::npos) other = ps;
  }
  if (!other && g_sa.trFind && g_sa.getComp && g_sa.psType) {
    void* t = Invoke(m_getTransform, pg); void* fa[1] = { il2cpp_string_new("Muzzle Smoke") };
    void* ft = t ? Invoke(g_sa.trFind, t, fa) : nullptr; void* ca[1] = { g_sa.psType };
    other = ft ? Invoke(g_sa.getComp, ft, ca) : nullptr;
  }
  return other;
}
static void SmokeConsider(void* pg, const std::string& name, const char* via) {
  static std::set<std::string> told;
  int rank = SmokeRankOf(name);
  if (rank >= 0 && told.insert(name).second) DLog("barrel smoke: muzzle flash '%s' (%s)", name.c_str(), via);
  if (rank <= g_smokeBestRank || !Alive(pg)) return;
  void* ps = SmokeSystemIn(pg); if (!ps) return;
  Unpin(g_smokeBestH); g_smokeBest = ps; g_smokeBestH = Pin(ps); g_smokeBestRank = rank;
  g_smokeBestName = name + "/" + NameOf(ps);
}
static void SmokeConsiderMount(void* mount) {                         // (a) the emmiter prefabs of a gun's barrels
  void* arr = *(void**)((char*)mount + g_sa.offBarrels);
  int n = ArrLen(arr);
  g_smokeLooked++;
  for (int i = 0; i < n && i < 8; i++) {
    void* b = ArrAt(arr, i); if (!b) continue;
    void* emm = *(void**)((char*)b + g_sa.offEmm); if (!Alive(emm)) continue;
    std::string name = NameOf(emm);
    if (SmokeRankOf(name) <= g_smokeBestRank) continue;
    void* t = Invoke(g_sa.goTr, emm); void* pg = t ? FindChildComponent(t, "Core", "ParticleGroup") : nullptr;
    if (pg) SmokeConsider(pg, name, "a barrel's emmiter");
  }
}
static void SmokeSearchAll() {                                        // (c) every loaded ParticleGroup: walks every object, so once per 30 s
  if (g_smokeSearchAt && GetTickCount() - g_smokeSearchAt < 30000) return;
  g_smokeSearchAt = GetTickCount();
  void* res = FindClass("UnityEngine", "Resources");
  void* findAll = res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
  if (!findAll || !g_sa.pgType) return;
  void* a[1] = { g_sa.pgType };
  void* arr = Invoke(findAll, nullptr, a);
  int n = ArrLen(arr);
  for (int i = 0; i < n; i++) { void* pg = ArrAt(arr, i); if (pg) SmokeConsider(pg, NameOf(pg), "loaded assets"); }
  DLog("barrel smoke: searched %d ParticleGroups (best '%s')", n, g_smokeBestRank >= 0 ? g_smokeBestName.c_str() : "none");
}
// Our parked copy of the best template: active, kept across scenes, at the origin. Made again when a better template
// turns up (only while nothing smokes) or when it was destroyed.
static bool SmokeEmitter() {
  if (Alive(g_smokePS) && g_smokeRank >= g_smokeBestRank) return true;
  if (g_smokeBestRank >= 0 && !Alive(g_smokeBest)) {                 // the best template died before we copied it
    Unpin(g_smokeBestH); g_smokeBest = g_smokeBestH = nullptr; g_smokeBestRank = -1;
    if (Alive(g_smokePS)) { g_smokeBestRank = g_smokeRank; return true; }   // our copy stays the best; only a better one replaces it
    g_smokeBestName.clear();
  }
  if (g_smokeBestRank < 0) SmokeSearchAll();
  if (g_smokeBestRank < 0 || !Alive(g_smokeBest)) {
    static bool told = false;
    if (!told) { told = true; Log("barrel smoke: no muzzle smoke found (looked at %d guns and the loaded assets); guns do not smoke", g_smokeLooked); }
    return false;
  }
  if (Alive(g_smokePS)) { for (auto& e : g_smoke) if (e.second.smoking) return true; }   // upgrade later: it would cut off the smoke
  if (Alive(g_smokePS) && g_sa.count && Unbox32(Invoke(g_sa.count, g_smokePS)) > 0) return true;   // or its last puffs, still fading
  if (g_smokePS) {
    if (Alive(g_smokePS) && g_sa.destroy) { void* go = Invoke(m_getGameObject, g_smokePS); if (go) { void* da[1] = { go }; Invoke(g_sa.destroy, nullptr, da); } }
    Unpin(g_smokePSH); g_smokePS = g_smokePSH = nullptr; g_smokeRank = -1;
  }
  void* c = CloneObject(g_smokeBest);
  if (!c) { Log("barrel smoke: could not copy '%s'; guns do not smoke", g_smokeBestName.c_str()); g_smokeDead = true; return false; }
  void* go = Invoke(m_getGameObject, c);
  if (go) {
    bool on = true; void* sa[1] = { &on }; Invoke(m_setActive, go, sa);   // a pooled instance may have been inactive
    if (g_sa.dontDestroy) { void* da[1] = { go }; Invoke(g_sa.dontDestroy, nullptr, da); }
    SetName(go, "WM barrel smoke");
  }
  if (void* t = Invoke(m_getTransform, c)) {
    V3 z{ 0, 0, 0 }, one{ 1, 1, 1 }; Q4 q{ 0, 0, 0, 1 };
    if (g_sa.trSetPos) { void* pa[1] = { &z }; Invoke(g_sa.trSetPos, t, pa); }
    if (g_sa.trSetRot) { void* ra[1] = { &q }; Invoke(g_sa.trSetRot, t, ra); }
    void* la[1] = { &one }; Invoke(m_setLocalScale, t, la);
  }
  Invoke(g_sa.play, c);
  g_smokePS = c; g_smokePSH = Pin(c); g_smokeRank = g_smokeBestRank;
  Log("barrel smoke: ready (puffs from '%s')", g_smokeBestName.c_str());
  return true;
}
// First sight of a mount this battle: excluded, or its heat settings and muzzles.
static void SmokeFirstSight(void* mount, bool copy, SmokeGun& s) {
  s.copy = copy;
  if (IsHiddenMount(mount) || (!copy && !OnPlayerBoat(mount))) return;   // enemy guns, and the hidden placeholders
  const Item* it = ItemOfMount(mount);
  if (it && (it->family == "Rockets" || it->family == "Crew Weapons")) return;
  void* gt = g_offGunType >= 0 ? *(void**)((char*)mount + g_offGunType) : nullptr;
  if (!gt) return;
  std::string gn = StripTags(NameOf(gt));
  for (const char* x : { "Rocket", "Mk50", "X50", "Mousetrap" }) if (gn.find(x) != std::string::npos) return;
  for (const char* x : { "M1918 BAR", "Bazooka", "M1919 Browning Handheld", "M7 Grenade Launcher" }) if (gn == x) return;
  s.name = it ? it->name : gn;
  float cd = (float)NumField(gt, "shotCooldown"), rpm = (float)NumField(gt, "roundsPerMinute"), rl = (float)NumField(gt, "reloadCooldown");
  if (cd <= 0 && rpm > 0) cd = 60.0f / rpm;
  if (cd <= 0) cd = rl;                                                // mortars and launchers: no cooldown and no rate
  s.cd = cd;
  float cap = 3 * cd > rl + 0.75f ? 3 * cd : rl + 0.75f;
  s.gapCap = cap < 1 ? 1 : (cap > 6 ? 6 : cap);
  float sd = 2.5f * cd > 0.6f ? 2.5f * cd : 0.6f;
  s.stopDelay = sd < s.gapCap ? sd : s.gapCap;
  s.len = *(float*)((char*)mount + g_sa.offLen);
  float k = s.len / 2.5f; s.sizeK = k < 0.6f ? 0.6f : (k > 1.6f ? 1.6f : k);
  void* pts = *(void**)((char*)mount + g_sa.offShoot);
  int n = ArrLen(pts);
  std::vector<void*> got;                                              // distinct shoot points: the first and the last
  for (int i = 0; i < n; i++) { void* t = ArrAt(pts, i); if (t && std::find(got.begin(), got.end(), t) == got.end()) got.push_back(t); }
  if (got.empty()) {                                                   // none: the barrels' own transforms (Barrel +0x10)
    void* arr = *(void**)((char*)mount + g_sa.offBarrels); int nb = ArrLen(arr);
    for (int i = 0; i < nb; i++) { void* b = ArrAt(arr, i); void* t = b ? *(void**)((char*)b + 0x10) : nullptr; if (t && std::find(got.begin(), got.end(), t) == got.end()) got.push_back(t); }
  }
  if (got.empty()) { DLog("barrel smoke: '%s' has no shoot points; it does not smoke", s.name.c_str()); return; }
  s.sp[0] = got.front(); s.nsp = 1;
  if (got.size() > 1) { s.sp[1] = got.back(); s.nsp = 2; }
  s.h[0] = Pin(mount); for (int i = 0; i < s.nsp; i++) s.h[1 + i] = Pin(s.sp[i]);
  s.excluded = false;
  SmokeConsiderMount(mount);
  DLog("barrel smoke: watching '%s'%s (gaps up to %.2f s, smokes %.2f s after the last shot, size x%.2f, %d muzzle(s))",
       s.name.c_str(), copy ? " (another player's gun)" : "", s.gapCap, s.stopDelay, s.sizeK, s.nsp);
}
static void SmokeDrop(SmokeGun& s) { for (void*& h : s.h) { Unpin(h); h = nullptr; } }
static void SmokeOnShot(void* mount, bool copy) {
  if (!g_barrelSmoke || g_smokeDead || !mount || !SmokeApiReady()) return;
  auto f = g_smoke.find(mount);
  if (f == g_smoke.end()) { f = g_smoke.emplace(mount, SmokeGun()).first; SmokeFirstSight(mount, copy, f->second); }
  SmokeGun& s = f->second;
  if (s.excluded) return;
  float now = SmokeNow(), gap = now - s.last;
  if (s.last >= 0 && gap <= s.gapCap) s.burst += gap < 3 * s.cd ? gap : 3 * s.cd;   // a reload pause holds the burst, not heats it
  else { s.burst = s.burst - gap > 0 ? s.burst - gap : 0; }
  // a new burst: the first shot, or one after a gap the burst did not outlast (shells in the same frame, gap 0, are not)
  if (s.last < 0 || (gap > s.gapCap && s.burst <= 0)) {                // its shots, and the game's own count to check them
    s.burstShots = 0;
    s.cnt0 = g_sa.offCounter >= 0 ? *(int*)((char*)mount + g_sa.offCounter) - 1 : 0;
  }
  s.last = now; s.shots++; s.burstShots++;
  if (s.burst > s.peak) s.peak = s.burst;
  s.smoking = false;                                                   // firing again: the smoke stops (its puffs fade on their own)
}
static float CrewLogNow();
static float g_talkAction = 0; static bool g_talkShot = false;        // the talking crew: last action; our guns have fired
static std::unordered_map<void*, bool> g_talkOurGun;                   // mount -> on our boat (cleared each battle)
static void TalkShot(void* mount) {
  auto f = g_talkOurGun.find(mount);
  if (f == g_talkOurGun.end()) f = g_talkOurGun.emplace(mount, OnPlayerBoat(mount)).first;
  if (f->second) { g_talkAction = CrewLogNow(); g_talkShot = true; }
}
static void SafeTalkShot(void* mount) { __try { TalkShot(mount); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void SafeSmokeShot(void* mount, bool copy) {
  if (g_smokeFaults < 0) return;
  __try { SmokeOnShot(mount, copy); }
  __except (FaultFilter("barrel smoke (shot)", GetExceptionInformation())) { g_smokeFaults = -1; }
}
static void SmokeClear() { if (Alive(g_smokePS) && g_sa.clear) Invoke(g_sa.clear, g_smokePS); }
static void SmokeTick() {
  if (g_smokeDead || g_smoke.empty() || !g_sa.ok || !EngagementInstance()) return;
  if (!g_barrelSmoke) {                                                // switched off in Mod Options: end it now
    bool was = false;
    for (auto& e : g_smoke) { was = was || e.second.smoking; e.second.smoking = false; e.second.burst = 0; e.second.last = -1; }
    if (was) SmokeClear();
    return;
  }
  float now = SmokeNow(), dt = g_smokeLastTick < 0 ? 0 : now - g_smokeLastTick;
  g_smokeLastTick = now;
  if (dt < 0) dt = 0; if (dt > 0.1f) dt = 0.1f;
  g_smokeBudget += 40 * dt; if (g_smokeBudget > 10) g_smokeBudget = 10;   // at most 40 puffs a second, all guns together
  float after = (float)g_smokeAfter, secs = (float)g_smokeSecs;
  int on = 0;
  for (auto& e : g_smoke) on += e.second.smoking ? 1 : 0;
  for (auto& e : g_smoke) {
    SmokeGun& s = e.second;
    if (s.excluded || s.smoking || s.burst < after || now - s.last <= s.stopDelay) continue;
    if (!SmokeEmitter()) { g_smokeMissed++; s.burst = 0; s.last = -1; continue; }
    if (on >= 4) {                                                     // at most 4 guns: the one closest to its end stops
      SmokeGun* end = nullptr;
      for (auto& o : g_smoke) if (o.second.smoking && (!end || o.second.smokeT0 + o.second.smokeLen < end->smokeT0 + end->smokeLen)) end = &o.second;
      if (end) { end->smoking = false; end->burst = 0; end->last = -1; on--; }
    }
    float len = secs + 0.5f * (s.burst - after);
    s.smokeLen = len < secs ? secs : (len > 2 * secs ? 2 * secs : len);
    float in = s.burst / after; s.inten = in < 1 ? 1 : (in > 2.5f ? 2.5f : in);
    s.smoking = true; s.smokeT0 = now; s.accum[0] = s.accum[1] = 0; s.times++; on++;
    if (s.copy) g_smokeCopyTimes++;
    Invoke(g_sa.play, g_smokePS);                                      // in case it stopped (a short duration, not looping)
    if (g_smokeLogs < 8) { g_smokeLogs++; Log("barrel smoke: '%s' smokes for %.1f s after a %.1f s burst (%d shots, %d barrel(s))%s", s.name.c_str(), s.smokeLen, s.burst, s.burstShots, s.nsp, s.copy ? " (another player's gun)" : ""); }
    else DLog("barrel smoke: '%s' smokes for %.1f s after a %.1f s burst (%d shots, %d barrel(s))%s", s.name.c_str(), s.smokeLen, s.burst, s.burstShots, s.nsp, s.copy ? " (another player's gun)" : "");
    if (g_sa.offCounter >= 0 && Alive(e.first)) DLog("barrel smoke: '%s' shotCounter +%d over the burst (we counted %d)", s.name.c_str(), *(int*)((char*)e.first + g_sa.offCounter) - s.cnt0, s.burstShots);
  }
  if (!on || dt <= 0) return;
  if (!Alive(g_smokePS) && !SmokeEmitter()) return;                   // gone with a scene: made again
  for (auto it = g_smoke.begin(); it != g_smoke.end();) {
    SmokeGun& s = it->second;
    if (!s.smoking) { ++it; continue; }
    float k = 1 - (now - s.smokeT0) / s.smokeLen;
    if (k <= 0) { s.smoking = false; s.burst = 0; s.last = -1; ++it; continue; }   // last -1: its next shot starts a burst
    bool alive = Alive(it->first);
    for (int i = 0; i < s.nsp && alive; i++) alive = Alive(s.sp[i]);
    if (!alive) { SmokeDrop(s); it = g_smoke.erase(it); continue; }   // the gun is gone (sunk, or its boat rebuilt)
    for (int i = 0; i < s.nsp; i++) {
      s.accum[i] += 4 * s.inten * k * dt;
      if (s.accum[i] < 1) continue;
      V3 pos = GetV3(g_sa.trPos, s.sp[i]), fwd = GetV3(g_sa.trFwd, s.sp[i]);   // once per muzzle per frame (boxed returns)
      float out = s.len - 1.25f;                                       // where Barrel.emitFlash puts the flash
      V3 m{ pos.x + fwd.x * out, pos.y + fwd.y * out, pos.z + fwd.z * out };
      while (s.accum[i] >= 1) {
        if (g_smokeBudget < 1) { if (s.accum[i] > 2) s.accum[i] = 2; break; }
        s.accum[i] -= 1; g_smokeBudget -= 1;
        V3 p{ m.x + (SmokeRand() - 0.5f) * 0.2f, m.y + (SmokeRand() - 0.5f) * 0.2f, m.z + (SmokeRand() - 0.5f) * 0.2f };
        V3 v{ fwd.x * 0.3f + (SmokeRand() - 0.5f) * 0.4f, 0.8f + 0.6f * SmokeRand() + fwd.y * 0.3f, fwd.z * 0.3f + (SmokeRand() - 0.5f) * 0.4f };
        float size = (0.35f + 0.25f * SmokeRand()) * s.sizeK * (0.8f + 0.2f * s.inten), life = 1.8f + 1.2f * SmokeRand();
        struct { unsigned char r, g, b, a; } col = { 215, 215, 215, (unsigned char)(60 + 110 * k) };
        void* ea[5] = { &p, &v, &size, &life, &col }; Invoke(g_sa.emit, g_smokePS, ea);
        g_smokePuffs++;
      }
    }
    ++it;
  }
  if (g_sa.count) { int c = Unbox32(Invoke(g_sa.count, g_smokePS)); if (c > g_smokePeak) g_smokePeak = c; }
}
// At CritScan's battle change: the summary line, then a clean slate.
static void SmokeSummary() {
  if (!g_barrelSmoke) Log("barrel smoke, last battle: off");
  else {
    std::string guns; int times = 0, n = 0; const SmokeGun* top = nullptr;
    for (auto& e : g_smoke) {
      const SmokeGun& s = e.second; if (s.excluded) continue;
      if (!top || s.peak > top->peak) top = &s;
      if (!s.times) continue;
      char b[200]; snprintf(b, sizeof b, "%s'%s' %d (longest burst %.1f s)", n ? ", " : "", s.name.c_str(), s.times, s.peak);
      guns += b; times += s.times; n++;
    }
    if (times) Log("barrel smoke, last battle: %d time(s) on %d gun(s): %s; %d puffs (at most %d at once); others' guns %d", times, n, guns.c_str(), g_smokePuffs, g_smokePeak, g_smokeCopyTimes);
    else if (g_smokeMissed) Log("barrel smoke, last battle: %d burst(s) reached %.1f s, but no muzzle smoke was found to show them", g_smokeMissed, g_smokeAfter);
    else if (top) Log("barrel smoke, last battle: no burst reached %.1f s (longest: '%s' %.1f s)", g_smokeAfter, top->name.c_str(), top->peak);
    else Log("barrel smoke, last battle: no burst reached %.1f s (none of your guns fired)", g_smokeAfter);
  }
  for (auto& e : g_smoke) SmokeDrop(e.second);
  g_smoke.clear();
  g_smokePuffs = g_smokeCopyTimes = g_smokeLogs = g_smokeMissed = g_smokePeak = 0;
  g_smokeLastTick = -1; g_smokeBudget = 0;
  SmokeClear();
}
static void SafeFemGSummary();
static void SafeCrewLogBattleChange(bool hadBattle);                  // crew battle log
static void SafeCrewLogKill(void* member, void* killed);
static void SafeSmokeSummary() {
  __try { SmokeSummary(); }
  __except (FaultFilter("barrel smoke (summary)", GetExceptionInformation())) { g_smokeFaults = -1; }
}

static void CrateBattleChange(bool hadBattle);
static void CritScan(void* pm) {
  if (g_offPmLive == -2) {
    void* k = FindClass("Core", "ProjectileManager"); void* pk = FindClass("Core", "Projectile");
    g_offPmLive = FieldOffset(k, "projectiles_live"); g_offPjFrom = FieldOffset(pk, "shotFrom"); g_offPjDmg = FieldOffset(pk, "damageMultiplier");
    g_offPjAlive = FieldOffset(pk, "alive"); g_offPjNetOwner = FieldOffset(pk, "networkOwner");
    Log("crits: projectiles_live +0x%X, shotFrom +0x%X, damageMultiplier +0x%X, alive +0x%X, networkOwner +0x%X", g_offPmLive, g_offPjFrom, g_offPjDmg, g_offPjAlive, g_offPjNetOwner);
  }
  if (g_offPmLive < 0 || g_offPjFrom < 0 || g_offPjDmg < 0) return;
  void* eng = EngagementInstance();
  if (eng != g_critEng) {
    if (g_critEng && g_critCount) Log("crits: %d critical hits last battle", g_critCount);
    if (g_critEng) { TurretSummary(); UsesSummary(); SafeSmokeSummary(); }
    CrateBattleChange(g_critEng != nullptr);                          // crates per round: the summary, then a clean slate
    SafeGfxBattleChange(g_critEng != nullptr);                         // gun effects: summary lines, then a clean slate
    SafeCrewLogBattleChange(g_critEng != nullptr);                     // crew battle log: the report, then a clean slate
    PaintSummary();                                                    // boats are built between battles: a session count
    SafeFemGSummary();                                                 // female crew: stored genders, session totals
    g_firedMounts.clear(); g_mountShotAt.clear();
    g_critEng = eng; g_critCache.clear(); g_critCount = 0; g_shots.clear(); g_shotFrame = 1; g_isTurret.clear();   // frame 1: a new entry (frame 0) is never "last frame"
    g_copyHasItem.clear(); g_hiddenMount.clear();
  }
  if (!eng) return;
  void* live = *(void**)((char*)pm + g_offPmLive);
  int n = ListCount(live);
  unsigned frame = ++g_shotFrame;
  for (int i = 0; i < n; i++) {
    void* pj = ListAt(live, i); if (!pj) continue;
    void* mount = *(void**)((char*)pj + g_offPjFrom);
    float alive = g_offPjAlive >= 0 ? *(float*)((char*)pj + g_offPjAlive) : 0.0f;
    ShotSig& e = g_shots[pj];
    bool lastFrame = e.frame == frame - 1, same = lastFrame && SameShot(e, mount, alive);
    e = { mount, alive, frame };
    if (same) continue;                                                // seen last frame: already handled
    if (lastFrame) SafeUnusualRecycled(pj);                            // a pooled shell fired again: its old life is over
    if (!mount) continue;
    if (IsCopyShot(pj)) {                                               // co-op: another player's shot; its crits and stats are theirs
      auto c = g_copyHasItem.find(mount);
      if (c == g_copyHasItem.end()) c = g_copyHasItem.emplace(mount, ItemOfMount(mount) != nullptr).first;
      if (c->second) { g_copyShots++; if (g_coopVisuals) SafeUnusualOnShot(pj, mount); }
      if (g_coopVisuals && IsRemoteMount(mount) && !g_chainBoost.count(pj)) SafeSmokeShot(mount, true);   // their mod guns smoke here too (looks only; not our chain leaps); enemy copies never
      continue;
    }
    SafeSpendUse(mount);                                                 // first, in its own guard, so a fault below never skips the use count
    g_mountShotAt[mount] = GetTickCount();
    float ed = EliteDamageOf(mount);                                    // a shot from a Rare/Elite ship with a damage trait
    if (ed != 1.0f) *(float*)((char*)pj + g_offPjDmg) *= ed;
    SafeUnusualOnShot(pj, mount);
    SafeGfxShot(pj, mount, g_chainBoost.count(pj) > 0);                // Opening Salvo, Frugal; a chain leap only clears its old life
    if (g_chainBoost.count(pj)) continue;                              // a chain leap: its parent's hit already rolled, and counted
    SafeSmokeShot(mount, false);                                       // barrel heat (barrel_smoke); enemy guns are filtered there
    SafeTalkShot(mount);                                               // the talking crew: our guns firing is no lull
    AllKnowingShot(mount);
    if (TurretStats* ts = TurretOf(mount)) { ts->shots++; if (ts->automated && ShotHasTarget(pj)) { ts->aimed++; AutoShotInfo(pj, ts); } }
    auto c = g_critCache.find(mount);
    if (c == g_critCache.end()) {
      const Item* it = ItemOfMount(mount);
      float ch = it ? (float)ItemFx(*it, "critChance") : 0, mu = it ? (float)ItemFx(*it, "critMult") : 0;
      int nth = it ? (int)ItemFx(*it, "nthShot") : 0; float nm = it ? (float)ItemFx(*it, "nthMult") : 0;
      c = g_critCache.emplace(mount, CritCfg{ ch, mu > 0 ? mu : 3.0f, nth, nm > 0 ? nm : 3.0f, 0 }).first;
    }
    CritCfg& cc = c->second;
    float mult = 0;
    if (cc.nth > 0 && ++cc.shots % cc.nth == 0) mult = cc.nthMult;
    else if (cc.chance > 0 && Rand01() < cc.chance) mult = cc.mult;
    if (mult <= 0) continue;
    *(float*)((char*)pj + g_offPjDmg) *= mult;
    g_critCount++;
    DWORD now = GetTickCount();
    // at most every 20 s, or the kill feed fills up
    if (now - g_critMsgAt > 20000) { g_critMsgAt = now; ShowMessage("Critical hit! (x" + std::to_string((int)mult) + ")"); }
  }
}
