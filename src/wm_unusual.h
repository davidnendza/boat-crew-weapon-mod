// Unusual guns and heal/chain streaks (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Unusual guns (kept out of the README on purpose) ------------------------------------------------------------------
// A tier above Unique, in pink. The guns are built into the DLL (kUnusualDefs, parsed like [unique] blocks) instead of
// drops.ini, so players only meet them in play. Drop odds: drops.ini unusual_chance (not written to the default file).
// Their powers are fx values read per gun mount:
//   hangar=N     every Nth shot launches a mini friendly plane (F4F Wildcat) that flies and attacks on its own
//   duck=N       every Nth shot a mini friendly PT boat joins you
//   gull=N       every Nth shot launches a tiny dive bomber that explodes near an enemy boat or shore gun (credited to the gun)
//   boomerang=1  shots that fly past their target turn round for a second pass
//   fireworks=1  every shot (and fragment) gets a random tracer color
//   orbital=R    shells circle the boat at R metres and streak off at enemy ships and ground targets within kOrbitReach
//   magnet=V     a hit pulls the enemy ship toward you by V m/s
//   homerun=H    a ship the gun kills is thrown about H m (+-14%) into the air with a spin (HomeRunLaunch)
//   firemission=N every Nth direct hit marks the target with smoke; 4 s later a barrage of 6 splashes walks across it
//   upup=N       direct hits swell a ship 1.08x each (to 1.5x); after N it lifts off, then pops (BalloonHit / BalloonTick)
//   bigbrother=N every Nth shot a full-size friendly Fletcher comes as a call-in for 45 s (one at a time; BigBrotherCall)
// Spawning uses the path the game's own console command takes (SpawnEntityCommand.Spawn, verified in the disassembly):
// EntitySpawn.spawnFrom(pos, euler, spawner null, prefab, GameManager.instance.entityPooler), prefab from the pooler's
// ObjType list (+0x28: name +0x10, prefab +0x18). A plane gets its speed like PlaneCatapult gives it:
// FlightObject.resetVelocity(float3). Despawn: LiveEntity.poolableObject (+0xC8).repool().
extern const char* kUnusualDefs;
const char* kUnusualDefs =
  "[unique]\nname=The Hangar\nmodel=hangar\nmodel_material=own\nbase=40 mm Bofors\nuses=6\nline=every 8th shot launches a mini fighter plane that hunts on its own (up to 3)\nflavor=Ground crew not included.\nfx.set.hangar=8\n\n"
  "[unique]\nname=Mother Duck\nmodel=mother_duck\nmodel_material=own\nbase=M3 37 mm AT\nuses=6\nline=every 5th shot a mini PT boat joins you for two minutes (up to 2)\nflavor=Stay close, little ones.\nfx.set.duck=5\n\n"
  "[unique]\nname=Kamikaze Gull\nmodel=kamikaze_gull\nmodel_material=own\nbase=20 mm Oerlikon\nuses=6\nline=every 12th shot launches a tiny dive bomber that explodes on the nearest boat or shore gun\nflavor=It has made its peace.\nfx.set.gull=12\n\n"
  "[unique]\nname=Boomerang\nmodel=boomerang\nmodel_material=own\nbase=M2 Browning\nuses=6\nline=shots that miss turn round for a second pass\nflavor=What goes around comes around.\nfx.set.boomerang=1\nproj.mul.FrameLifeTime=2.5\n\n"
  "[unique]\nname=Fireworks\nmodel=fireworks\nmodel_material=own\nbase=40 mm Dual Bofors\nuses=6\nline=shells burst near the target into a shower of colored sparks\nflavor=Happy Fourth of July.\nfx.set.fireworks=1\nproj.set.proximity=1\nproj.set.proximityMaxErrorSeconds=0.03\nproj.set.proximityOffsetSeconds=0\nproj.set.SplashDamage=1\nproj.mul.SplashDamageRange=1.5\nproj.set.fragmentCount=10\nproj.set.fragmentInheritSourceVelocity=0.3\nfragment_from=M2 Browning\n\n"
  "[unique]\nname=Orbital\nmodel=orbital\nmodel_material=own\nbase=20 mm Elco Thunderbolt\nuses=6\nline=shells circle your boat, then streak off at enemy boats and shore guns within 1.5 km\nflavor=Personal space, enforced.\nfx.set.orbital=25\nproj.mul.FrameLifeTime=25\n\n"
  // Magnet: weight 0 = out of the drop pool (it works, but the pull is not noticeable at fighting range)
  "[unique]\nname=Magnet\nmodel=magnet\nmodel_material=own\nbase=M4 37 mm Autocannon\nuses=6\nweight=0\nline=shells home in gently, and every hit pulls the enemy ship toward you\nflavor=Get over here.\nfx.set.magnet=6\nfx.set.seek=45\n\n"
  "[unique]\nname=Home Run\nmodel=home_run\nmodel_material=own\nbase=M4 37 mm Autocannon\nuses=6\nline=ships it kills are knocked 60-80 m into the air\nflavor=Swing for the fences.\nfx.set.homerun=70\n\n"
  "[unique]\nname=Fire Mission\nmodel=fire_mission\nmodel_material=own\nbase=3-inch Gun Player\nuses=6\nline=every 6th hit marks the target with smoke, and a barrage walks across it 4 s later\nflavor=Fire for effect.\nfx.set.firemission=6\n\n"
  "[unique]\nname=Up, Up and Away\nmodel=up_up_away\nmodel_material=own\nbase=M1919 Browning\nuses=6\nline=hits pump enemy boats up like balloons: after 5 they float off and pop\nflavor=Lighter than air. Briefly.\nfx.set.upup=5\n\n"
  "[unique]\nname=Big Brother\nmodel=big_brother\nmodel_material=own\nbase=3-inch Gun Player\nuses=6\nline=every 40th shot a full-size Fletcher destroyer steams in to fight beside you for 45 s\nflavor=Don\'t make me come over there.\nfx.set.bigbrother=40\n";

struct UnusualCfg { int hangar, duck, gull; bool boomerang, fireworks; float orbital, magnet; int shots; float homerun; int firemission, upup, bigbro; };
static std::unordered_map<void*, UnusualCfg> g_unuCfg;               // mount -> its powers, per battle
static void* g_unuEng = nullptr;
static int g_unuFaults = 0;
// kind 0 plane, 1 boat, 2 gull, 3 Big Brother's destroyer. owner >= 0: asked for by that co-op player (the host spawns
// it; boat is where their boat is)
struct SpawnReq { int kind; void* mount; V3 pos, dir; int shot; int owner = -1; V3 boat = { 0, 0, 0 }; };
static std::vector<SpawnReq> g_spawnQueue;
static bool CoopAskHelper(const SpawnReq& r);
static void CoopHelperMade(void* ent, int owner, int kind);
struct Minion { void* ent; void* handle; void* mount; int kind; DWORD born, life; int owner;   // owner: the co-op player it was spawned for (-1 this machine's)
  // a Kamikaze Gull steered by the mod (GullSteer): its target (pinned), the plane's AI it switched off, its velocity
  void* target = nullptr; void* tHandle = nullptr; void* ai = nullptr; void* fo = nullptr; V3 vel{}, lastPos{}, aim{};
  float lastT = -1, speed = 0, nearest = 1e30f, lastD = 1e30f, fromD = 0; DWORD lastTry = 0; bool steered = false, full = false;
  float age = 0, ageT = -1, hp0 = -1; };                              // age: in game time (s), so pauses don't eat it; hp0: a destroyer's hull HP on arrival
static std::vector<Minion> g_minions;
struct OrbitShot { float angle; bool released; };
static std::unordered_map<void*, OrbitShot> g_orbit;                 // shot -> its place in the ring
struct BoomerShot { void* mount; int state; };                       // state 0 no target yet, 1 has one, 2 turned round
static std::unordered_map<void*, BoomerShot> g_boomer;
struct MagnetShot { void* target; V3 last; float pull; void* mount; };
static std::unordered_map<void*, MagnetShot> g_magnet;
static std::unordered_map<void*, std::vector<void*>> g_fireworkTypes;   // projectile type -> 8 colored copies
static LARGE_INTEGER g_unuLast = {};
static DWORD g_orbitReleaseAt = 0, g_orbitCalmCheck = 0, g_orbitEnemySeen = 0;
static int g_orbitReleases = 0;                                       // per battle, the first few are logged
// ships fight from 1 km and more: a shorter reach leaves the ring circling
static const float kOrbitReach = 1500.0f;
// WorldEngagementManager.enemyDistanceLimitLeave: no enemy may be this close for the battle to let you leave
static float LeaveDistance() {
  static int off = -2;
  if (off == -2) {
    off = FieldOffset("Core.World", "WorldEngagementManager", "enemyDistanceLimitLeave");
  }
  void* eng = EngagementInstance();
  float d = eng && off >= 0 ? *(float*)((char*)eng + off) : 0;
  return d > 0 ? d : 1500.0f;
}

static const UnusualCfg& UnuCfgOf(void* mount) {
  auto c = g_unuCfg.find(mount);
  if (c != g_unuCfg.end()) return c->second;
  const Item* it = ItemOfMount(mount);
  UnusualCfg u{};
  if (it) {
    u.hangar = (int)ItemFx(*it, "hangar"); u.duck = (int)ItemFx(*it, "duck"); u.gull = (int)ItemFx(*it, "gull");
    u.boomerang = ItemFx(*it, "boomerang") > 0; u.fireworks = ItemFx(*it, "fireworks") > 0;
    u.orbital = (float)ItemFx(*it, "orbital"); u.magnet = (float)ItemFx(*it, "magnet"); u.homerun = (float)ItemFx(*it, "homerun"); u.firemission = (int)ItemFx(*it, "firemission"); u.upup = (int)ItemFx(*it, "upup"); u.bigbro = (int)ItemFx(*it, "bigbrother");
  }
  return g_unuCfg.emplace(mount, u).first->second;
}
static V3 PosOf(void* component) {
  void* tr = component ? Invoke(m_getTransform, component) : nullptr; void* pb = tr && m_trGetPos ? Invoke(m_trGetPos, tr) : nullptr;
  return pb ? *(V3*)((char*)pb + 0x10) : V3{ 0, -10000, 0 };
}
static int MinionsOf(void* mount, int kind) { int n = 0; for (auto& m : g_minions) if (m.mount == mount && m.kind == kind) n++; return n; }

// ---- Heal and chain streaks (effect_streaks) -------------------------------------------------------------------------
// A green streak arcs from the wreck to your boat when a kill repairs your hull (killRepair: Vampiric, Lazarus,
// Kurogane, ...), and a short bright one runs along each chain leap to the next target (on top of the leap shell's own
// tracer). A small pool of LineRenderers built from the crew aim line's lookups (g_cw), on a material of their own.
// The kill and chain hooks only queue a request; StreakTick takes them in after ChainScan in the same frame and moves
// them in game time (UnityEngine.Time.time), so a streak keeps pace with the shells in the kill cam's slow motion and
// stops in a pause. Co-op: a heal streak is sent to the other players (HEAL) when coop_visuals is on; chain leaps of
// other players' shots are rolled on each machine (ChainScan), so they get streaks with no message.
struct Streak { void* lr = nullptr; void* h = nullptr; bool on = false; int kind = 0; V3 a{}, b{}; void* follow = nullptr; void* followH = nullptr; float lift = 0, t0 = 0, yOff = 0; };
static const int kStreakN = 12, kStreakPts = 8, kStreakQueue = 16;
static Streak g_streaks[kStreakN];                                   // kind 0 heal, 1 chain leap
struct StreakReq { int kind; V3 a, b; void* follow; };
static std::vector<StreakReq> g_streakQ;                             // queued by the hooks, taken in by StreakTick
static void* g_streakEng = nullptr; static void* g_streakMat = nullptr; static void* g_streakMatH = nullptr;
static bool g_streakMatTried = false, g_streakDead = false;          // dead: no material or renderer, or a fault in a hook
static int g_streakHeal = 0, g_streakHealIn = 0, g_streakChain = 0, g_streakChainCopy = 0, g_streakSkipped = 0, g_streakLogsH = 0, g_streakLogsC = 0, g_streakFaults = 0;
static void CoopSendHeal(const V3& w, const V3& me);                 // the co-op section: a heal streak for the other players
static bool StreakReady() {                                           // not g_cw.lineOk: that stays false when only crew lookups failed
  const CrewApi& a = g_cw;
  return a.goCls && a.goCtor && a.addComp && a.lrType && a.lrSetPos && a.lrStart && a.lrEnd && a.lrWidth && a.lrCount && a.rEnabled && a.rSetMat;
}
// The streaks' own material: a glow first, then shaders that draw the line's own colors. Unlit/Color has no vertex
// colors, so it is set to white. Never the crew line's material (its HL fallback and red tracer would tint everything).
static void* StreakMaterial() {
  if (g_streakMatTried) return g_streakMat;
  g_streakMatTried = true;
  const char* from = nullptr; void* m = nullptr;
  for (const char* n : { "Legacy Shaders/Particles/Additive", "Sprites/Default", "Legacy Shaders/Particles/Alpha Blended", "Particles/Standard Unlit", "Unlit/Color" })
    if ((m = MaterialFromShader(n))) { from = n; break; }
  if (!m) { Log("effect streaks: no material; streaks are off"); g_streakDead = true; return nullptr; }
  g_streakMatH = Pin(m); g_streakMat = m;
  if (!strcmp(from, "Unlit/Color") && g_cw.matSetColor) { float c[4] = { 1, 1, 1, 1 }; void* sa[2] = { il2cpp_string_new("_Color"), c }; Invoke(g_cw.matSetColor, m, sa); }
  void* sh = g_cw.matShader ? Invoke(g_cw.matShader, m) : nullptr;
  Log("effect streaks: material '%s' (shader '%s'), from shader %s", NameOf(m).c_str(), sh ? NameOf(sh).c_str() : "?", from);
  return m;
}
static bool StreakMake(Streak& s) {
  void* go = il2cpp_object_new(g_cw.goCls);
  void* na[1] = { il2cpp_string_new("WM effect streak") }; void* exc = nullptr;
  il2cpp_runtime_invoke(g_cw.goCtor, go, na, &exc);
  void* ta[1] = { g_cw.lrType };
  void* lr = exc ? nullptr : Invoke(g_cw.addComp, go, ta);
  if (!lr) { Log("effect streaks: could not make a LineRenderer; streaks are off"); g_streakDead = true; return false; }
  void* ma[1] = { g_streakMat }; Invoke(g_cw.rSetMat, lr, ma);
  if (g_cw.rShadows) { int off = 0; void* sa[1] = { &off }; Invoke(g_cw.rShadows, lr, sa); }
  if (g_cw.rRecv) { bool no = false; void* ra[1] = { &no }; Invoke(g_cw.rRecv, lr, ra); }
  int n = kStreakPts; void* pa[1] = { &n }; Invoke(g_cw.lrCount, lr, pa);
  s.lr = lr; s.h = Pin(lr);                                            // useWorldSpace keeps Unity's default (true), as the crew line
  return true;
}
// Queues a streak from a to b (b follows 'follow' while it lives). No Unity calls: safe from the kill and chain hooks.
static bool StreakAdd(int kind, const V3& a, const V3& b, void* follow) {
  if (!g_streaksOn || g_streakDead || g_streakFaults < 0) return false;
  if (!StreakReady()) { static bool told = false; if (!told) { told = true; Log("effect streaks: LineRenderer pieces missing; no streaks"); } return false; }
  if (!g_streakEng || EngagementInstance() != g_streakEng) return false;   // battle only, once StreakTick has seen it
  if ((int)g_streakQ.size() >= kStreakQueue) { g_streakSkipped++; return false; }
  g_streakQ.push_back({ kind, a, b, follow });
  return true;
}
static float StreakRand() { static unsigned s = 0x2545F491u; s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; }   // own stream: Rand01 rolls the drops and chains
static void StreakFree(Streak& s) { Unpin(s.followH); s.followH = s.follow = nullptr; s.on = false; }
static void StreakHideAll() {
  for (Streak& s : g_streaks) {
    if (!s.on) continue;
    if (Alive(s.lr)) { bool off = false; void* ea[1] = { &off }; Invoke(g_cw.rEnabled, s.lr, ea); }
    StreakFree(s);
  }
}
static void SafeStreaksOff() { __try { StreakHideAll(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void StreakTick() {
  void* eng = EngagementInstance();
  if (eng != g_streakEng) {
    if (g_streakEng && (g_streakHeal || g_streakHealIn || g_streakChain || g_streakSkipped))
      Log("effect streaks, last battle: %d heal streak(s), %d from other players' kills, %d chain streak(s) (%d on other players' shots), %d skipped (pool full)%s",
          g_streakHeal, g_streakHealIn, g_streakChain, g_streakChainCopy, g_streakSkipped, g_streaksOn ? "" : "; effect_streaks=0");
    for (Streak& s : g_streaks) {
      if (s.lr && Alive(s.lr) && g_cw.destroy) { void* go = Invoke(m_getGameObject, s.lr); if (go) { void* da[1] = { go }; Invoke(g_cw.destroy, nullptr, da); } }
      Unpin(s.h); Unpin(s.followH); s = Streak();
    }
    g_streakQ.clear();
    g_streakHeal = g_streakHealIn = g_streakChain = g_streakChainCopy = g_streakSkipped = g_streakLogsH = g_streakLogsC = 0;
    if (g_streakMat && !Alive(g_streakMat)) { Unpin(g_streakMatH); g_streakMat = g_streakMatH = nullptr; g_streakMatTried = false; }   // unloaded with a scene: made again
    g_streakEng = eng;
  }
  if (!eng) return;
  if (!g_streaksOn || g_streakDead) { StreakHideAll(); g_streakQ.clear(); return; }
  bool any = !g_streakQ.empty();
  for (const Streak& s : g_streaks) any = any || s.on;
  if (!any) return;
  static void* mTime = nullptr; static bool timeTried = false;
  if (!timeTried) { timeTried = true; void* tm = FindClass("UnityEngine", "Time"); mTime = tm ? FindMethod(tm, "get_time", 0) : nullptr; }
  void* tb = mTime ? Invoke(mTime, nullptr) : nullptr;
  if (!tb) { Log("effect streaks: Time.time missing; streaks are off"); g_streakDead = true; g_streakQ.clear(); return; }
  float now = *(float*)((char*)tb + 0x10);
  if (!g_streakQ.empty() && !g_streakMatTried) {
    if (StreakMaterial()) Log("effect streaks: ready (pool of %d lines, timed in game time)", kStreakN);
  }
  if (!g_streakMat) { g_streakQ.clear(); return; }
  for (const StreakReq& r : g_streakQ) {
    Streak* s = nullptr;
    for (Streak& x : g_streaks) if (!x.on) { s = &x; break; }
    if (!s) { g_streakSkipped++; continue; }
    if (s->lr && !Alive(s->lr)) { Unpin(s->h); s->lr = s->h = nullptr; }   // gone with its scene
    if (!s->lr) { if (!StreakMake(*s)) break; }
    else { bool on = true; void* ea[1] = { &on }; Invoke(g_cw.rEnabled, s->lr, ea); }
    s->on = true; s->kind = r.kind; s->a = r.a; s->b = r.b; s->t0 = now;
    s->follow = r.follow; s->followH = r.follow ? Pin(r.follow) : nullptr;
    float len = CrewDist(r.a, r.b);
    s->lift = r.kind == 0 ? (0.15f * len < 60.0f ? 0.15f * len : 60.0f) : 0.0f;   // the heal arcs up, the leap is straight
    s->yOff = r.kind == 0 ? 2.5f : 1.5f;
  }
  g_streakQ.clear();
  void* cam = g_cw.camMain ? Invoke(g_cw.camMain, nullptr) : nullptr;
  V3 cp = cam ? PosOf(cam) : V3{ 0, -10000, 0 };
  for (Streak& s : g_streaks) {
    if (!s.on) continue;
    if (!Alive(s.lr)) { Unpin(s.h); s.lr = s.h = nullptr; StreakFree(s); continue; }
    bool heal = s.kind == 0;
    float T = heal ? 0.45f : 0.12f, fade = heal ? 0.2f : 0.3f, t = now - s.t0;   // head travel, then fade out
    if (t < 0) t = 0;
    if (t > T + fade) { bool off = false; void* ea[1] = { &off }; Invoke(g_cw.rEnabled, s.lr, ea); StreakFree(s); continue; }
    if (s.follow) {
      if (Alive(s.follow)) { V3 p = PosOf(s.follow); if (p.y > -9000) s.b = { p.x, p.y + s.yOff, p.z }; }
      else { Unpin(s.followH); s.followH = s.follow = nullptr; }
    }
    float head = t / T > 1 ? 1 : t / T, tail = heal ? (t - 0.15f) / T : 0.0f;   // the heal's tail runs 0.15 s behind its head
    if (tail < 0) tail = 0; if (tail > head) tail = head;
    V3 hp{};
    for (int i = 0; i < kStreakPts; i++) {
      float u = tail + (head - tail) * i / (kStreakPts - 1), arc = s.lift * 4 * u * (1 - u);
      V3 P{ s.a.x + (s.b.x - s.a.x) * u, s.a.y + (s.b.y - s.a.y) * u + arc, s.a.z + (s.b.z - s.a.z) * u };
      void* pa[2] = { &i, &P }; Invoke(g_cw.lrSetPos, s.lr, pa);
      hp = P;
    }
    float fa = t > T ? 1 - (t - T) / fade : 1; if (fa < 0) fa = 0;
    Col4 c0 = heal ? Col4{ 0.2f, 0.9f, 0.35f, 0.15f } : Col4{ 0.55f, 0.8f, 1.0f, 0.6f };   // start = tail (index 0), end = head
    Col4 c1 = heal ? Col4{ 0.35f, 1.0f, 0.45f, 1.0f } : Col4{ 0.9f, 0.97f, 1.0f, 1.0f };
    c0.a *= fa; c1.a *= fa;
    void* sc[1] = { &c0 }; Invoke(g_cw.lrStart, s.lr, sc);
    void* ec[1] = { &c1 }; Invoke(g_cw.lrEnd, s.lr, ec);
    float w = cp.y > -9000 ? 0.6f + CrewDist(cp, hp) * 0.0015f : 1.0f;   // wider far off, so it still shows at 2 km
    if (w < 0.6f) w = 0.6f; if (w > 3.0f) w = 3.0f;
    if (!heal) w *= 0.75f + 0.5f * StreakRand();                       // the leap flickers
    void* wa[1] = { &w }; Invoke(g_cw.lrWidth, s.lr, wa);
  }
}
// From OnKillCredit, after a kill repaired the hull: wreck -> your boat. Its own fault guard, so a fault here never turns
// off Salvager, Vampiric and Bounty (g_killFaults).
static void HealStreak(void* killed, void* craft, const char* gun, const char* type, int pct) {
  bool share = g_coopRole && g_coopVisuals;
  if ((!g_streaksOn || g_streakDead || g_streakFaults < 0) && !share) return;
  V3 w = PosOf(killed), me = PosOf(craft);
  if (w.y < -9000 || me.y < -9000) return;
  w.y += 3; me.y += 2.5f;
  if (share) CoopSendHeal(w, me);
  if (!StreakAdd(0, w, me, craft)) return;
  g_streakHeal++;
  if (g_streakLogsH < 10) {
    g_streakLogsH++;
    Log("effect streaks: heal streak for '%s': %s '%s' %.0f m away -> your boat (+%d%%)", gun, type, NameOf(killed).c_str(), CrewDist(w, me), pct);
  }
}
static bool g_healStreakFault = false;                               // a fault in HealStreak: also stops its co-op send, which
                                                                     // runs past g_streakDead (no material only stops the drawing)
static void SafeHealStreak(void* killed, void* craft, const char* gun, const char* type, int pct) {
  if (g_healStreakFault) return;
  __try { HealStreak(killed, craft, gun, type, pct); }
  __except (FaultFilter("effect streaks (heal)", GetExceptionInformation())) { g_streakDead = true; g_healStreakFault = true; }
}
// From ChainScan, for a leap to another target.
static void ChainStreak(const ChainState& cs, void* next, const V3& np) {
  V3 from{ cs.pos[0], cs.pos[1] + 1, cs.pos[2] }, to{ np.x, np.y + 1.5f, np.z };   // np: the transform origin (sea level, or the ground)
  if (!StreakAdd(1, from, to, next)) return;
  g_streakChain++; if (cs.copy) g_streakChainCopy++;
  if (g_streakLogsC < 10) {
    g_streakLogsC++;
    Log("effect streaks: chain streak: leap %d of '%s' from %s to %s, %.0f m%s", cs.leap + 1, StripTags(NameOf(cs.mount)).c_str(),
        cs.target ? NameOf(cs.target).c_str() : "?", NameOf(next).c_str(), CrewDist(from, to), cs.copy ? " (another player's shot)" : "");
  }
}
static void SafeChainStreak(const ChainState& cs, void* next, const V3& np) {
  if (!g_streaksOn || g_streakDead || g_streakFaults < 0) return;
  __try { ChainStreak(cs, next, np); }
  __except (FaultFilter("effect streaks (chain)", GetExceptionInformation())) { g_streakDead = true; }
}

// What each Unusual gun did this battle, per gun mount. In the chaos of a battle the effects are hard to see, so each
// event gets a log line (busy ones only the first few per battle) and a summary per gun is logged every 2 minutes while
// something happens and when the battle ends.
struct UnuStats {
  std::string name; UnusualCfg cfg{};
  int shots = 0, spawned[4] = {}, capped[4] = {}, lost[4] = {}, expired[4] = {}, minionKills[4] = {}, kills = 0;
  int gullHits = 0, gullSteered = 0, gullMissed = 0, gullTerrain = 0, gullSea = 0, gullNearN = 0; float gullNearSum = 0;   // Kamikaze Gull
  int turned = 0, noTarget = 0, orbitJoined = 0, orbitFull = 0, orbitLaunched = 0, orbitDropped = 0;
  int sparks = 0, bursts = 0, magnetPulls = 0;
  int homeRuns = 0, homeLanded = 0, homeSkipped = 0; float homeBest = 0;   // Home Run
  int fmHits = 0, fmCount = 0, fmCalled = 0, fmCooling = 0, fmSmoke = 0, fmBarrages = 0, fmShells = 0, fmNotQueued = 0, fmKills = 0, fmClient = 0;   // Fire Mission
  int upHits = 0, upPuffs = 0, upLifts = 0, upPops = 0, upDowned = 0, upLost = 0, upClient = 0; float upBest = 0;   // Up, Up and Away
  int bbCallin = 0, bbFallback = 0; float bbDamage = 0;                // Big Brother: how each destroyer came, hull HP it lost
  DWORD sparkAt = 0; V3 sparkPos{};
};
static std::unordered_map<void*, UnuStats> g_unuStats;
static bool g_unuStatsNew = false; static DWORD g_unuStatsAt = 0;
static const char* kMinionWord[4] = { "fighter", "PT boat", "gull", "destroyer" };
static UnuStats& StatsOf(void* mount) {
  g_unuStatsNew = true;
  auto s = g_unuStats.find(mount);
  if (s != g_unuStats.end()) return s->second;
  UnuStats n; n.name = StripTags(NameOf(mount)); n.cfg = UnuCfgOf(mount);
  return g_unuStats.emplace(mount, n).first->second;
}
static void Append(std::string& t, const char* fmt, ...) {
  char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); t += b;
}
static void UnusualSummary(const char* when) {
  static const char* gone[4] = { "lost", "sunk", "lost", "sunk" };
  static const char* home[4] = { "flew home", "sailed home", "found no target in time", "steamed home" };
  for (auto& e : g_unuStats) {
    const UnuStats& s = e.second; const UnusualCfg& c = s.cfg; std::string t;
    Append(t, "'%s': %d shots", s.name.c_str(), s.shots);
    int every[4] = { c.hangar, c.duck, c.gull, c.bigbro };
    for (int k = 0; k < 4; k++) {
      if (every[k] <= 0) continue;
      Append(t, "; %d %ss launched (%d more skipped, cap reached), %d %s, %d %s", s.spawned[k], kMinionWord[k], s.capped[k], s.lost[k], gone[k], s.expired[k], home[k]);
      if (k == 2) Append(t, ", %d blew up next to a ship", s.gullHits);
      else Append(t, ", %d kills by them", s.minionKills[k]);
      if (k == 3) Append(t, " (%d as call-ins, %d placed by the mod; %.0f hull HP lost)", s.bbCallin, s.bbFallback, s.bbDamage);
    }
    if (c.boomerang) Append(t, "; %d shots turned round for a second pass, %d had no target to turn to", s.turned, s.noTarget);
    if (c.fireworks) Append(t, "; %d bursts (%d sparks)", s.bursts, s.sparks);
    if (c.orbital > 0) Append(t, "; %d shells joined the ring (%d not, ring full), %d launched at ships, %d dropped into the sea", s.orbitJoined, s.orbitFull, s.orbitLaunched, s.orbitDropped);
    if (c.magnet > 0) Append(t, "; %d ships pulled", s.magnetPulls);
    if (c.homerun > 0) Append(t, "; %d ships launched (%d landed, %d kills not launched), best %.0f m", s.homeRuns, s.homeLanded, s.homeSkipped, s.homeBest);
    if (c.firemission > 0) Append(t, "; %d hits, %d fire missions (%d marked with smoke), %d barrages, %d kills during one", s.fmHits, s.fmCalled, s.fmSmoke, s.fmBarrages, s.fmKills);
    if (c.upup > 0) Append(t, "; %d hits, %d puffs, %d ships lifted off, %d popped, %d shot down while swollen, best %.0f m up", s.upHits, s.upPuffs, s.upLifts, s.upPops, s.upDowned, s.upBest);
    Append(t, "; %d kills credited to the gun", s.kills);
    DLog("extra effects summary (%s): %s", when, t.c_str());
  }
  g_unuStatsNew = false;
}

// Called by CritScan for every new shot.
// Fireworks: the shell flies as one of 8 palette-colored copies of its projectile type.
static void FireworksColor(void* pj) {
  void* pt = CrewSource(*(void**)((char*)pj + 0x20));                  // Projectile.ptype (a crew copy's original)
  if (!pt) return;
  auto f = g_fireworkTypes.find(pt);
  if (f == g_fireworkTypes.end()) {
    std::vector<void*> copies;
    for (int s = 0; s < kPaletteN - 1; s++) {                        // red (the last) stays the crew weapons' color
      void* cp = CloneObject(pt); if (!cp) break;
      std::string nm = NameOf(pt) + " [Fireworks " + std::to_string(s) + "]";
      SetName(cp, nm.c_str()); SetRef(cp, (char*)cp + g_offProjTag, il2cpp_string_new(("WM_" + nm).c_str()));
      Pin(cp); RegisterPaletteShot(cp, s); copies.push_back(cp);       // pinned here: RegisterPaletteShot skips it with the palette off
    }
    f = g_fireworkTypes.emplace(pt, copies).first;
    for (void* cp : copies) g_fireworkTypes.emplace(cp, copies);      // a colored shot's fragments are colored too
    DLog("extra effects: fireworks colors for '%s' (%d copies)", NameOf(pt).c_str(), (int)copies.size());
  }
  if (!f->second.empty()) SetRef(pj, (char*)pj + 0x20, f->second[(size_t)(Rand01() * f->second.size()) % f->second.size()]);
}
static void UnusualOnShot(void* pj, void* mount) {
  if (g_offPjVel < 0 || g_offPjPos < 0 || g_offPjTarget < 0) return;  // SeekScan looks these up on its first frame
  const UnusualCfg& cfg = UnuCfgOf(mount);
  if (!cfg.hangar && !cfg.duck && !cfg.gull && !cfg.boomerang && !cfg.fireworks && cfg.orbital <= 0 && cfg.magnet <= 0 && cfg.homerun <= 0 && cfg.firemission <= 0 && cfg.upup <= 0 && cfg.bigbro <= 0) return;
  if (IsCopyShot(pj)) {                                               // co-op: another player's shot. Only the colors show here: their
    if (cfg.fireworks) FireworksColor(pj);                            // helpers come from the host, and the ring, turns and pulls
    return;                                                           // need targets only the shooter's machine has
  }
  UnusualCfg& c = g_unuCfg[mount];
  c.shots++;
  const float* v = (const float*)((char*)pj + g_offPjVel); const float* p = (const float*)((char*)pj + g_offPjPos);
  float sp = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  V3 dir = sp > 0.1f ? V3{ v[0] / sp, v[1] / sp, v[2] / sp } : V3{ 0, 0, 1 }, pos{ p[0], p[1], p[2] };
  UnuStats& st = StatsOf(mount);
  if (c.hangar > 0 && c.shots % c.hangar == 0) g_spawnQueue.push_back({ 0, mount, pos, dir, c.shots });
  if (c.duck > 0 && c.shots % c.duck == 0) g_spawnQueue.push_back({ 1, mount, pos, dir, c.shots });
  if (c.gull > 0 && c.shots % c.gull == 0) g_spawnQueue.push_back({ 2, mount, pos, dir, c.shots });
  if (c.bigbro > 0 && c.shots % c.bigbro == 0) g_spawnQueue.push_back({ 3, mount, pos, dir, c.shots });
  if (c.boomerang) g_boomer[pj] = { mount, 0 };
  if (c.magnet > 0) g_magnet[pj] = { nullptr, pos, c.magnet, mount };
  if (c.orbital > 0) {
    int n = 0; for (auto& o : g_orbit) if (!o.second.released) n++;
    if (n < 10) { g_orbit[pj] = { (float)(Rand01() * 6.2831853), false }; st.orbitJoined++; } else st.orbitFull++;
  }
  // Fireworks sparks are new shots from the same gun, made where the shell burst: one far from the gun is a spark,
  // and sparks more than 50 ms or 20 m apart are a new burst
  bool spark = false;
  if (c.fireworks) {
    V3 g = PosOf(mount); float gx = pos.x - g.x, gy = pos.y - g.y, gz = pos.z - g.z;
    spark = g.y > -5000 && gx * gx + gy * gy + gz * gz > 25.0f * 25.0f;
    if (spark) {
      DWORD now = GetTickCount(); float sx = pos.x - st.sparkPos.x, sz = pos.z - st.sparkPos.z;
      if (!st.sparkAt || now - st.sparkAt > 50 || sx * sx + sz * sz > 20.0f * 20.0f) st.bursts++;
      st.sparks++; st.sparkAt = now; st.sparkPos = pos;
    }
  }
  if (!spark) st.shots++;
  if (c.fireworks) FireworksColor(pj);
}

// Steering for Boomerang and Orbital, magnet hit detection. Called every frame from the ProjectileManager.Update hook.
static void TurnToward(float* v, float tx, float ty, float tz, float rateDeg, float dt) {
  float sp = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), tl = sqrtf(tx * tx + ty * ty + tz * tz);
  if (sp < 0.1f || tl < 0.1f) return;
  float ux = v[0] / sp, uy = v[1] / sp, uz = v[2] / sp; tx /= tl; ty /= tl; tz /= tl;
  float dot = ux * tx + uy * ty + uz * tz; if (dot > 1) dot = 1; if (dot < -1) dot = -1;
  float ang = acosf(dot), maxA = rateDeg * 3.14159265f / 180.0f * dt; if (ang < 1e-4f) return;
  float a = ang <= maxA ? ang : maxA;
  float wx = tx - ux * dot, wy = ty - uy * dot, wz = tz - uz * dot, wl = sqrtf(wx * wx + wy * wy + wz * wz);
  if (wl < 1e-5f) { wx = -uz; wy = 0; wz = ux; wl = sqrtf(wx * wx + wz * wz); if (wl < 1e-5f) return; }   // straight away: turn sideways
  float ca = cosf(a), sa = sinf(a);
  v[0] = (ux * ca + wx / wl * sa) * sp; v[1] = (uy * ca + wy / wl * sa) * sp; v[2] = (uz * ca + wz / wl * sa) * sp;
}
// A Magnet shot is over: if it ended within 30 m of an enemy ship it was a hit, and the ship is pulled toward you
// (hand-aimed shots usually have no target set, so the nearest ship counts).
static void MagnetShotEnded(const MagnetShot& ms) {
  V3 tp{}; void* t = NearestOtherEnemyShip(&ms.last.x, nullptr, 30.0f, &tp);
  if (!t) return;
  static void *mBody = nullptr, *mAddForce = nullptr;
  if (!mBody) { void* le = FindClass("Core", "LiveEntity"); mBody = le ? FindMethod(le, "get_Body", 0) : nullptr; void* rb = FindClass("UnityEngine", "Rigidbody"); mAddForce = rb ? FindMethod(rb, "AddForce", 2, "UnityEngine.Vector3", "UnityEngine.ForceMode") : nullptr; }
  void* body = mBody ? Invoke(mBody, t) : nullptr;
  V3 me = PosOf(PlayerCraft());
  float px = me.x - tp.x, pz = me.z - tp.z, pl = sqrtf(px * px + pz * pz);
  if (body && mAddForce && pl > 30) {
    V3 f{ px / pl * ms.pull, 0, pz / pl * ms.pull }; int mode = 2;   // ForceMode.VelocityChange
    void* fa[2] = { &f, &mode }; Invoke(mAddForce, body, fa);
    StatsOf(ms.mount).magnetPulls++;
    DLog("extra effects: magnet pulled '%s' %.1f m/s toward you (%.0f m away)", NameOf(t).c_str(), ms.pull, pl);
    static DWORD said = 0; DWORD tnow = GetTickCount();
    if (tnow - said > 4000) { said = tnow; ShowMessage(Colored("Magnet: pulling " + ShipTitle(t), "#ff5fc8")); }
  } else if (!body || !mAddForce) DLog("extra effects: magnet hit '%s' but no Rigidbody/AddForce (%p %p)", NameOf(t).c_str(), body, mAddForce);
}
// CritScan found a pooled shell fired again (see ShotSig): whatever the mod tracked for its last life ends now, before
// the new shot is looked at, so an Orbital or Boomerang power never carries over to another gun's shell.
static std::vector<MagnetShot> g_magnetEnded;                         // ended that way; handled by the next scan
static void UnusualRecycled(void* pj) {
  auto m = g_magnet.find(pj);
  if (m != g_magnet.end()) { g_magnetEnded.push_back(m->second); g_magnet.erase(m); }
  auto b = g_boomer.find(pj);
  if (b != g_boomer.end()) { if (b->second.state == 0) StatsOf(b->second.mount).noTarget++; g_boomer.erase(b); }
  g_orbit.erase(pj);
}
static void SafeUnusualRecycled(void* pj) {
  if (g_unuFaults < 0) return;
  __try { UnusualRecycled(pj); }
  __except (FaultFilter("extra effects (reused shell)", GetExceptionInformation())) { g_unuFaults = -1; }
}
static void UnusualProjScan(void* pm) {
  if (g_offPjVel < 0 || g_offPjPos < 0 || g_offPjTarget < 0 || g_offPmLive < 0) return;
  for (const MagnetShot& ms : g_magnetEnded) MagnetShotEnded(ms);
  g_magnetEnded.clear();
  if (g_boomer.empty() && g_orbit.empty() && g_magnet.empty()) { QueryPerformanceCounter(&g_unuLast); return; }
  LARGE_INTEGER now; QueryPerformanceCounter(&now);
  double dtd = g_unuLast.QuadPart ? PerfMs(g_unuLast) / 1000.0 : 0; g_unuLast = now;
  float dt = (float)dtd;
  void* live = *(void**)((char*)pm + g_offPmLive);
  std::unordered_set<void*> alive;
  for (int i = 0; i < ListCount(live); i++) { void* pj = ListAt(live, i); if (pj && !*(bool*)((char*)pj + 0x68)) alive.insert(pj); }
  // magnet: a tracked shot that is gone, close to its target, was a hit
  for (auto m = g_magnet.begin(); m != g_magnet.end(); ) {
    if (alive.count(m->first)) {
      m->second.target = *(void**)((char*)m->first + g_offPjTarget);
      const float* p = (const float*)((char*)m->first + g_offPjPos); m->second.last = { p[0], p[1], p[2] };
      ++m; continue;
    }
    MagnetShotEnded(m->second);
    m = g_magnet.erase(m);
  }
  for (auto b = g_boomer.begin(); b != g_boomer.end(); ) {
    if (alive.count(b->first)) { ++b; continue; }
    if (b->second.state == 0) StatsOf(b->second.mount).noTarget++;     // hand-aimed shots usually have no target set
    b = g_boomer.erase(b);
  }
  for (auto o = g_orbit.begin(); o != g_orbit.end(); ) { if (!alive.count(o->first)) o = g_orbit.erase(o); else ++o; }
  if (g_orbit.empty()) g_orbitEnemySeen = 0;
  if (dt <= 0 || dt > 0.25f) return;
  // boomerang: once a shot is past its target, it turns round toward it (once)
  for (auto& b : g_boomer) {
    void* pj = b.first; void* t = *(void**)((char*)pj + g_offPjTarget); if (!t) continue;
    if (b.second.state == 0) b.second.state = 1;
    float* v = (float*)((char*)pj + g_offPjVel); const float* p = (const float*)((char*)pj + g_offPjPos);
    V3 tp = PosOf(t); if (tp.y < -5000) continue;
    float dx = tp.x - p[0], dy = tp.y + 3 - p[1], dz = tp.z - p[2];
    if (b.second.state < 2) {
      if (v[0] * dx + v[1] * dy + v[2] * dz < 0 && dx * dx + dz * dz > 15 * 15) {
        b.second.state = 2;
        UnuStats& st = StatsOf(b.second.mount);
        if (++st.turned <= 3) DLog("extra effects: '%s' shot flew past '%s' and turned round (%d this battle)", st.name.c_str(), NameOf(t).c_str(), st.turned);
      } else continue;
    }
    v[1] += 9.81f * dt;
    TurnToward(v, dx, dy, dz, 260.0f, dt);
  }
  // orbital: ring of shells around the player's boat; one flies off every 0.3 s while an enemy ship is within kOrbitReach
  if (!g_orbit.empty()) {
    void* craft = PlayerCraft(); if (!craft) return;
    V3 c = PosOf(craft); if (c.y < -5000) return;
    V3 ep{}; void* enemy = nullptr;
    DWORD tick = GetTickCount();
    bool unreleased = false; for (auto& o : g_orbit) if (!o.second.released) { unreleased = true; break; }
    if (unreleased && tick - g_orbitReleaseAt > 300) {
      float at[3] = { c.x, c.y, c.z }; enemy = NearestEnemyTarget(at, kOrbitReach, &ep);
      if (enemy) g_orbitEnemySeen = tick;
    }
    // The battle lets you leave only when no enemy is within enemyDistanceLimitLeave and no shell of any team is within
    // half that of your boat (WorldEngagementManager.enemiesNearby -> ProjectileManager.withinProjectiles), so the ring
    // would keep the battle open after the last kill. Once no enemy ship has been that close for 3 s
    // (a new ring gets 3 s too), the ring is dropped into the sea.
    if (!g_orbitEnemySeen) g_orbitEnemySeen = tick;
    if (tick - g_orbitCalmCheck > 1000) {
      g_orbitCalmCheck = tick;
      float at[3] = { c.x, c.y, c.z }; V3 q;
      if (NearestEnemyTarget(at, LeaveDistance(), &q)) g_orbitEnemySeen = tick;
    }
    bool drop = !enemy && tick - g_orbitEnemySeen > 3000;
    int dropped = 0;
    for (auto& o : g_orbit) {
      void* pj = o.first; float* v = (float*)((char*)pj + g_offPjVel); const float* p = (const float*)((char*)pj + g_offPjPos);
      if (o.second.released) {                                        // flying at its ship: steer in at 360 deg/s
        void* t = *(void**)((char*)pj + g_offPjTarget);
        // a shot that missed would circle a sunk ship for its whole (25x) life and keep the battle open the same way
        if (t && (drop || (g_offLeAlive >= 0 && !*(bool*)((char*)t + g_offLeAlive)))) { SetRef(pj, (char*)pj + g_offPjTarget, nullptr); t = nullptr; }
        V3 tp = PosOf(t);
        if (t && tp.y > -5000) { v[1] += 9.81f * dt; TurnToward(v, tp.x - p[0], tp.y + 3 - p[1], tp.z - p[2], 360.0f, dt); }
        continue;
      }
      if (enemy) {
        SetRef(pj, (char*)pj + g_offPjTarget, enemy);
        float dx = ep.x - p[0], dy = ep.y + 3 - p[1], dz = ep.z - p[2], dl = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dl > 1) { v[0] = dx / dl * 250; v[1] = dy / dl * 250; v[2] = dz / dl * 250; }
        void* from = *(void**)((char*)pj + g_offPjFrom);
        if (from) StatsOf(from).orbitLaunched++;
        if (++g_orbitReleases <= 3) DLog("extra effects: orbital: shell launched at %s '%s' %.0f m away", il2cpp_class_get_name(il2cpp_object_get_class(enemy)), NameOf(enemy).c_str(), dl);
        o.second.released = true; enemy = nullptr; g_orbitReleaseAt = tick;
        continue;
      }
      if (drop) {                                                       // outward and down: it splashes a few metres out
        float ox = p[0] - c.x, oz = p[2] - c.z, ol = sqrtf(ox * ox + oz * oz);
        if (ol < 1) { ox = 1; oz = 0; ol = 1; }
        v[0] = ox / ol * 40; v[1] = -60; v[2] = oz / ol * 40;
        SetRef(pj, (char*)pj + g_offPjTarget, nullptr);
        void* from = *(void**)((char*)pj + g_offPjFrom);
        if (from) StatsOf(from).orbitDropped++;
        o.second.released = true; dropped++;
        continue;
      }
      UnusualCfg cfg{}; void* mount = *(void**)((char*)pj + g_offPjFrom); if (mount) cfg = UnuCfgOf(mount);
      float r = cfg.orbital > 0 ? cfg.orbital : 25.0f;
      o.second.angle += 45.0f / r * dt;                                 // 45 m/s along the ring
      float gx = c.x + cosf(o.second.angle) * r, gy = c.y + 7.0f, gz = c.z + sinf(o.second.angle) * r;
      float nx = (gx - p[0]) / 0.2f, ny = (gy - p[1]) / 0.2f, nz = (gz - p[2]) / 0.2f, nl = sqrtf(nx * nx + ny * ny + nz * nz);
      if (nl > 150) { nx *= 150 / nl; ny *= 150 / nl; nz *= 150 / nl; }
      v[0] = nx; v[1] = ny + 9.81f * dt; v[2] = nz;
    }
    static int dropSum = 0; static DWORD dropLogAt = 0;
    dropSum += dropped;
    if (dropSum && tick - dropLogAt > 5000) { DLog("extra effects: orbital: no enemy within %.0f m for 3 s, %d circling shells dropped into the sea", LeaveDistance(), dropSum); dropSum = 0; dropLogAt = tick; }
  }
}

// Spawning and minder for planes, PT boats and gulls. Called every frame from the ProjectileManager.LateUpdate hook.
static void* g_spawnFromM = nullptr; static void* g_gmInst = nullptr;
static int g_offGmPooler = -2, g_offPoolTypes = -1, g_offLePool = -1, g_offLeEntityCode = -1;
static void* EntityPooler() {
  if (g_offGmPooler == -2) {
    void* gm = FindClass("Core", "GameManager"); void* op = FindClass("", "ObjPooler"); void* es = FindClass("Core", "EntitySpawn"); void* le = FindClass("Core", "LiveEntity");
    g_gmInst = gm ? il2cpp_class_get_field_from_name(gm, "instance") : nullptr;
    g_offGmPooler = FieldOffset(gm, "entityPooler");
    void* ft = op ? il2cpp_class_get_field_from_name(op, "types") : nullptr; g_offPoolTypes = ft ? (int)il2cpp_field_get_offset(ft) : 0x28;
    g_offLePool = FieldOffset(le, "poolableObject");
    g_offLeEntityCode = FieldOffset(le, "entityCode");
    g_spawnFromM = es ? FindMethod(es, "spawnFrom", 5) : nullptr;
    DLog("spawner: GameManager.instance %p entityPooler +0x%X types +0x%X spawnFrom %p", g_gmInst, g_offGmPooler, g_offPoolTypes, g_spawnFromM);
  }
  void* gm = nullptr; if (g_gmInst) il2cpp_field_static_get_value(g_gmInst, &gm);
  return gm && g_offGmPooler >= 0 ? *(void**)((char*)gm + g_offGmPooler) : nullptr;
}
static void* FindEntityPrefab(void* pooler, const char* want) {
  static std::map<std::string, void*> cache; static bool listed = false;
  auto c = cache.find(want); if (c != cache.end()) return c->second;
  void* arr = *(void**)((char*)pooler + g_offPoolTypes);
  int n = ArrLen(arr); std::string all; void* hit = nullptr;
  for (int i = 0; i < n; i++) {
    void* ot = ArrAt(arr, i); if (!ot) continue;
    void* nm = *(void**)((char*)ot + 0x10); void* pf = *(void**)((char*)ot + 0x18);
    std::string s = nm ? Str(nm) : (pf ? NameOf(pf) : "");
    if (!listed) all += s + "; ";
    if (!hit && pf && s.find(want) != std::string::npos) hit = pf;
  }
  if (!listed) { listed = true; Log("spawner: %d spawnable entities: %s", n, all.c_str()); }   // always: Big Brother needs a Fletcher in it
  if (!hit) Log("spawner: no spawnable entity named like '%s'", want);
  cache[want] = hit;                                                   // prefabs are assets: they live for the whole game
  return hit;
}
// scale0: the entity's own localScale before `scale` was applied (its pooled object keeps the new one otherwise)
static void* SpawnFriendly(const char* want, V3 pos, V3 dir, float scale, float speed, V3* scale0 = nullptr) {
  void* pooler = EntityPooler(); if (!pooler || !g_spawnFromM) return nullptr;
  void* prefab = FindEntityPrefab(pooler, want); if (!prefab) return nullptr;
  float yaw = atan2f(dir.x, dir.z) * 57.2957795f;
  V3 euler{ 0, yaw, 0 };
  void* a[5] = { &pos, &euler, nullptr, prefab, pooler };
  void* ent = Invoke(g_spawnFromM, nullptr, a);
  if (!ent) { Log("spawner: spawning '%s' failed", want); return nullptr; }
  void* craft = PlayerCraft();
  int myTeam = craft ? *(int*)((char*)craft + g_offLeTeam) : 0, team = *(int*)((char*)ent + g_offLeTeam);
  if (team != myTeam) { void* m = FindMethod(il2cpp_object_get_class(ent), "setTeamCode", 1); if (m) { void* ta[1] = { &myTeam }; Invoke(m, ent, ta); } }
  if (scale != 1.0f) {
    void* tr = Invoke(m_getTransform, ent); void* b = tr ? Invoke(m_getLocalScale, tr) : nullptr;
    if (b) { V3 s = *(V3*)((char*)b + 0x10); if (scale0) *scale0 = s; s.x *= scale; s.y *= scale; s.z *= scale; void* sa[1] = { &s }; Invoke(m_setLocalScale, tr, sa); }
  }
  if (speed > 0) {                                                     // planes: the way PlaneCatapult launches them
    static void *flight = nullptr, *mReset = nullptr, *mGetComp = nullptr;
    if (!flight) { flight = FindClass("Cotanius.Modules.HybridFlight", "FlightObject"); mReset = flight ? FindMethod(flight, "resetVelocity", 1) : nullptr; void* comp = FindClass("UnityEngine", "Component"); mGetComp = comp ? FindMethod(comp, "GetComponent", 1, "System.Type") : nullptr; }
    void* fo = nullptr;
    if (flight && mGetComp) { void* ga[1] = { il2cpp_type_get_object(il2cpp_class_get_type(flight)) }; fo = Invoke(mGetComp, ent, ga); }
    if (fo && mReset) { float vel[3] = { dir.x * speed, 0, dir.z * speed }; void* va[1] = { vel }; Invoke(mReset, fo, va); }
    else DLog("spawner: '%s' has no FlightObject (%p) to launch", want, fo);
  }
  DLog("spawner: spawned '%s' %p (team %d -> %d) at %.0f,%.0f,%.0f", want, ent, team, myTeam, pos.x, pos.y, pos.z);
  return ent;
}
