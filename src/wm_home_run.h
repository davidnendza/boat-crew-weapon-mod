// Home Run, UnusualTick and Hooked_PMUpdate (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Home Run: a ship the gun kills is thrown into the air with a spin and lands with a triple splash ----------------
// Nothing in the game makes a dead ship kinematic or damps it out of the water, and the wreck model swaps in only below
// a depth, so a hull in the air flies on plain physics. (set_isKinematic callers: LiveEntity's one-frame spawn freeze,
// torpedoes, FishNet's RigidbodyPauser; Ship.FixedUpdateMe sleeps the body only during its spawn 'sleep' timer;
// FloatBehaviour damps vertical speed only while _inWater; ship damping is the prefab's; the wreck is ReplaceObjectOnSink.)
// The first launch logs isKinematic, damping and mass.
static bool TorpSetup(); static void TorpSplash(V3 at);
struct HomeRun { void* ship; void* h; void* mount; float y0, top, t0, vUp; V3 dir, at; bool told; };
static std::vector<HomeRun> g_homeRuns;                               // one per gun at a time
static int g_homeRunLogs = 0;                                         // per battle, the first 3 landings in full
static void *g_hrBody = nullptr, *g_hrForce = nullptr, *g_hrTorque = nullptr, *g_hrKin = nullptr, *g_hrDamp = nullptr, *g_hrMass = nullptr;
static float HrFloat(void* m, void* o) { void* r = m && o ? Invoke(m, o) : nullptr; return r ? *(float*)((char*)r + 0x10) : -1.0f; }
static void HrPush(void* body, V3 v, bool spin) {
  int mode = 2;                                                       // ForceMode.VelocityChange: no mass, no frame time
  void* a[2] = { &v, &mode }; Invoke(spin ? g_hrTorque : g_hrForce, body, a);
}
// Launch speed for apex h under the body's linear damping d: h(v) = v/d - g/d^2 ln(1 + d v/g), solved by bisection.
static float HomeRunSpeed(float h, float d) {
  const float g = 9.81f;
  if (d < 0.01f) return sqrtf(2 * g * h);
  float lo = 0, hi = 150;
  for (int i = 0; i < 30; i++) { float v = (lo + hi) / 2; ((v / d - g / (d * d) * logf(1 + d * v / g)) < h ? lo : hi) = v; }
  return hi;
}
static void HomeRunLaunch(void* mount, void* ship, UnuStats& st) {
  if (!IsShip(ship)) return;                                          // shore guns and planes stay put
  if (g_coopRole == 2) { if (!st.homeSkipped++) Log("home run: '%s' kill on a co-op client; the host moves the ships, so no launch", st.name.c_str()); return; }
  for (auto& h : g_homeRuns) if (h.mount == mount || h.ship == ship) { st.homeSkipped++; DLog("home run: '%s' kill not launched, one already in the air", st.name.c_str()); return; }
  static bool tried = false;
  if (!tried) {
    tried = true; void* le = FindClass("Core", "LiveEntity"); void* rb = FindClass("UnityEngine", "Rigidbody");
    g_hrBody = le ? FindMethod(le, "get_Body", 0) : nullptr;
    g_hrForce = rb ? FindMethod(rb, "AddForce", 2, "UnityEngine.Vector3", "UnityEngine.ForceMode") : nullptr;
    g_hrTorque = rb ? FindMethod(rb, "AddTorque", 2, "UnityEngine.Vector3", "UnityEngine.ForceMode") : nullptr;
    g_hrKin = rb ? FindMethod(rb, "get_isKinematic", 0) : nullptr; g_hrDamp = rb ? FindMethod(rb, "get_linearDamping", 0) : nullptr; g_hrMass = rb ? FindMethod(rb, "get_mass", 0) : nullptr;
    if (!g_hrBody || !g_hrForce || !g_hrTorque) Log("home run: a lookup failed (get_Body %p AddForce %p AddTorque %p); kills are not launched", g_hrBody, g_hrForce, g_hrTorque);
  }
  void* body = g_hrBody && g_hrForce && g_hrTorque ? Invoke(g_hrBody, ship) : nullptr;
  if (!body) { st.homeSkipped++; DLog("home run: '%s' killed '%s' but it has no Rigidbody", st.name.c_str(), NameOf(ship).c_str()); return; }
  void* k = g_hrKin ? Invoke(g_hrKin, body) : nullptr; bool kin = k && *(bool*)((char*)k + 0x10);
  float damp = HrFloat(g_hrDamp, body);
  static bool toldBody = false;
  if (!toldBody) { toldBody = true; Log("home run: a just-killed ship's Rigidbody: isKinematic %d (read %d), linearDamping %.3f, mass %.0f", kin, k != nullptr, damp, HrFloat(g_hrMass, body)); }
  if (kin) { st.homeSkipped++; DLog("home run: '%s' is kinematic, not launched", NameOf(ship).c_str()); return; }
  float want = st.cfg.homerun * (0.86f + 0.28f * (float)Rand01()), vUp = HomeRunSpeed(want, damp > 0 ? damp : 0);
  V3 g = PosOf(mount), p = PosOf(ship); float dx = p.x - g.x, dz = p.z - g.z, dl = sqrtf(dx * dx + dz * dz);
  V3 dir = dl > 1 && g.y > -5000 ? V3{ dx / dl, 0, dz / dl } : V3{ 0, 0, 0 };   // along the shot, flat
  V3 ax{ (float)Rand01() * 2 - 1, (float)Rand01() * 2 - 1, (float)Rand01() * 2 - 1 }; float al = sqrtf(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z);
  if (al < 0.1f) ax = { 1, 0, 0 }, al = 1;
  float spin = 1.5f + 1.5f * (float)Rand01();
  HrPush(body, V3{ dir.x * 6, vUp, dir.z * 6 }, false);
  HrPush(body, V3{ ax.x / al * spin, ax.y / al * spin, ax.z / al * spin }, true);
  g_homeRuns.push_back({ ship, Pin(ship), mount, p.y, p.y, CrewLogNow(), vUp, dir, p, false });
  st.homeRuns++;
  if (st.homeRuns <= 3) Log("home run: '%s' knocked %s up at %.1f m/s (aiming for %.0f m), cartwheeling at %.1f rad/s", st.name.c_str(), ShipTitle(ship).c_str(), vUp, want, spin);
  else DLog("home run: '%s' knocked %s up at %.1f m/s (aiming for %.0f m)", st.name.c_str(), ShipTitle(ship).c_str(), vUp, want);
}
static void HomeRunEnd(size_t i) { Unpin(g_homeRuns[i].h); g_homeRuns.erase(g_homeRuns.begin() + i); }
static void HomeRunTick() {
  if (g_homeRuns.empty()) return;
  TorpSetup();                                                        // the splash prefab
  static void* goActive = nullptr; if (!goActive) { void* gc = FindClass("UnityEngine", "GameObject"); goActive = gc ? FindMethod(gc, "get_activeSelf", 0) : nullptr; }
  float now = CrewLogNow();
  for (size_t i = g_homeRuns.size(); i-- > 0; ) {
    HomeRun& h = g_homeRuns[i];
    auto si = g_unuStats.find(h.mount); if (si == g_unuStats.end()) { HomeRunEnd(i); continue; }
    UnuStats& st = si->second; std::string who = ShipTitle(h.ship);
    void* go = goActive && Alive(h.ship) ? Invoke(m_getGameObject, h.ship) : nullptr; void* act = go ? Invoke(goActive, go) : nullptr;
    if (!Alive(h.ship) || (act && !*(bool*)((char*)act + 0x10))) {    // repooled while flying
      DLog("home run: %s left the battle in the air", who.c_str()); HomeRunEnd(i); continue; }
    V3 p = PosOf(h.ship); float t = now - h.t0;
    if (p.y > h.top) h.top = p.y;
    float up = h.top - h.y0;
    if (!h.told && p.y < h.top - 1 && up > 5) {                         // past the top: the kill feed line
      h.told = true; if (up > st.homeBest) st.homeBest = up;
      char b[32]; snprintf(b, sizeof b, " - %.0f m", up);
      ShowMessage(Colored("Home Run! " + who + b, "#ff5fc8"));
    }
    if (!h.told && t > 2 * h.vUp / 9.81f + 2) {                        // still rising: no gravity on this body, push it down once
      h.told = true; if (void* b = Invoke(g_hrBody, h.ship)) HrPush(b, V3{ 0, -2 * h.vUp, 0 }, false);
      Log("home run: %s was still rising after %.1f s (%.0f m up); pushed down (no gravity on it?)", who.c_str(), t, up);
    }
    if (h.told && p.y <= h.y0 + 2) {                                    // back on the water: three splashes along the flight
      V3 d = h.dir.x == 0 && h.dir.z == 0 ? V3{ 1, 0, 0 } : h.dir;
      for (int k = -1; k <= 1; k++) TorpSplash(V3{ p.x + d.x * 12 * k, h.y0, p.z + d.z * 12 * k });
      st.homeLanded++;
      if (++g_homeRunLogs <= 3) Log("home run: %s came down after %.1f s, %.0f m up at the top, landed %.0f m from where it was hit", who.c_str(), t, up, Dist3(V3{ p.x, 0, p.z }, V3{ h.at.x, 0, h.at.z }));
      else DLog("home run: %s came down after %.1f s, %.0f m up", who.c_str(), t, up);
      HomeRunEnd(i); continue;
    }
    if (t > 25 || (!h.told && t > 3 && up < 3)) {                      // never left the water, or lost track of it
      Log("home run: %s %s after %.1f s (%.0f m up); stopped watching it", who.c_str(), up < 3 ? "did not lift off" : "had not come down", t, up);
      HomeRunEnd(i);
    }
  }
}

static void FireMissionTick(); static void FireMissionBattleOver(); static void FireMissionKill(UnuStats& st, void* by, void* killed);
static void BalloonTick(); static void BalloonBattleOver(); static void SafeBalloonRestoreAll();
static void UnusualTick() {
  void* eng = EngagementInstance();
  if (eng != g_unuEng) {
    if (g_unuEng && !g_unuStats.empty()) UnusualSummary("battle over");
    for (auto& m : g_minions) Despawn(m, false);                      // the battle is over: helpers go home too
    RestoreAllHelperLooks("battle over");                              // the ones that died on the way too
    if (g_gullsDressed) Log("helpers: %d gull planes flew as gulls last battle; %d helpers got their own look or size back", g_gullsDressed, g_looksBack);
    for (auto& e : g_unuStats) {                                       // always logged, debug=0 too
      const UnuStats& s = e.second; if (s.cfg.gull <= 0 || !s.spawned[2]) continue;
      Log("kamikaze gull, last battle: '%s' launched %d, steered %d, blew up on a target %d, no target %d, shot down %d, kills credited %d (also %d ran out of time, %d hit the sea, %d hit the ground; %d not launched, cap of 4 out; closest to the aim point %.0f m on average)",
          s.name.c_str(), s.spawned[2], s.gullSteered, s.gullHits, s.expired[2], s.lost[2], s.minionKills[2], s.gullMissed, s.gullSea, s.gullTerrain, s.capped[2], s.gullNearN ? s.gullNearSum / s.gullNearN : 0.0f);
    }
    for (auto& e : g_unuStats) {                                       // always logged, like the gulls
      const UnuStats& s = e.second; if (s.cfg.homerun <= 0 || !(s.homeRuns + s.homeSkipped)) continue;
      Log("home run, last battle: '%s' launched %d ships, %d came down, best %.0f m; %d kills not launched (one in the air already, no body, or co-op client)", s.name.c_str(), s.homeRuns, s.homeLanded, s.homeBest, s.homeSkipped);
    }
    for (auto& e : g_unuStats) {                                       // always logged, like the gulls
      const UnuStats& s = e.second; if (s.cfg.bigbro <= 0 || !(s.spawned[3] + s.capped[3])) continue;
      Log("big brother, last battle: '%s' %d shots, %d destroyers came (%d as call-ins, %d placed by the mod; %d skipped, one already out), %d sunk, %d steamed home, %d kills by them, %.0f hull HP lost",
          s.name.c_str(), s.shots, s.spawned[3], s.bbCallin, s.bbFallback, s.capped[3], s.lost[3], s.expired[3], s.minionKills[3], s.bbDamage);
    }
    g_bbLogs = 0;
    for (auto& h : g_homeRuns) Unpin(h.h);
    g_homeRuns.clear(); g_homeRunLogs = 0;
    FireMissionBattleOver(); BalloonBattleOver();
    g_gullsDressed = g_looksBack = 0; g_gullFull = 0; g_gullBlasts.clear();
    g_minions.clear(); g_spawnQueue.clear(); g_unuCfg.clear(); g_orbit.clear(); g_orbitEnemySeen = 0; g_orbitReleases = 0; g_boomer.clear(); g_magnet.clear(); g_magnetEnded.clear(); g_fireworkTypes.clear(); g_unuEng = eng;   // shortcut: up to 7 pinned colored copies per fired type are left each battle (keys can be stale next battle); free them if Fireworks sees heavy use
    g_unuStats.clear(); g_unuStatsNew = false; g_unuStatsAt = GetTickCount();
  }
  if (!eng) return;
  HomeRunTick();
  FireMissionTick();
  BalloonTick();
  if (g_unuStatsNew && GetTickCount() - g_unuStatsAt > 120000) { g_unuStatsAt = GetTickCount(); UnusualSummary("so far"); }
  if (!EliteLookups()) return;                                         // shares the entity field offsets
  void* craft = PlayerCraft();
  for (SpawnReq& r : g_spawnQueue) {
    static const int cap[4] = { 3, 2, 4, 1 };
    static const char* says[4] = { "Fighter launched!", "A duckling joins you!", "Gull away!", "Big Brother is here!" };
    if (r.owner < 0 && g_coopRole == 2 && g_coopHelpers) {             // co-op client: the host spawns it, so everyone sees it
      if (CoopAskHelper(r)) ShowMessage(Colored(says[r.kind], "#ff5fc8"));
      continue;
    }
    if (MinionsOf(r.mount, r.kind) >= cap[r.kind]) {
      UnuStats& st = StatsOf(r.mount);
      if (++st.capped[r.kind] <= 2) DLog("extra effects: '%s' shot %d: no %s, %d already out (the most it keeps)", st.name.c_str(), r.shot, kMinionWord[r.kind], cap[r.kind]);
      continue;
    }
    V3 flat{ r.dir.x, 0, r.dir.z }; float fl = sqrtf(flat.x * flat.x + flat.z * flat.z); if (fl < 0.01f) flat = { 0, 0, 1 }; else { flat.x /= fl; flat.z /= fl; }
    void* ent = nullptr; DWORD life = 0; V3 s0 = { 0, 0, 0 };
    if (r.kind == 0) { V3 p{ r.pos.x, r.pos.y + 40, r.pos.z }; ent = SpawnFriendly("Wildcat", p, flat, 0.4f, 70, &s0); life = 45000; }
    else if (r.kind == 2) { V3 p{ r.pos.x, r.pos.y + 25, r.pos.z }; ent = SpawnFriendly("Dauntless", p, flat, kGullPlaneScale, 90, &s0); life = 25000; }
    else if (r.kind == 1 && (craft || r.owner >= 0)) {
      V3 c = r.owner >= 0 ? r.boat : PosOf(craft); V3 side{ flat.z, 0, -flat.x };   // to the right of where the gun is firing
      V3 p{ c.x + side.x * 40, c.y, c.z + side.z * 40 };
      ent = SpawnFriendly("PT-109", p, flat, 0.6f, 0, &s0); life = 120000;
    }
    else if (r.kind == 3 && (craft || r.owner >= 0)) {               // full size: nothing to restore
      V3 c = r.owner >= 0 ? r.boat : PosOf(craft); bool callin = false;
      ent = BigBrotherCall(c, flat, r.owner >= 0, &callin); life = 45000;
      if (ent) {
        UnuStats& st = StatsOf(r.mount); (callin ? st.bbCallin : st.bbFallback)++;
        float hp = HullHp(ent); V3 at = PosOf(ent);
        if (g_bbLogs < 3) { g_bbLogs++; Log("big brother: '%s' shot %d: %s '%s' arrived %s, %.0f m from your boat (hull %.0f HP, team %d)", st.name.c_str(), r.shot, ClassOf(ent), NameOf(ent).c_str(),
                                             callin ? "as a call-in" : "placed by the mod", Dist3(at, c), hp, g_offLeTeam >= 0 ? *(int*)((char*)ent + g_offLeTeam) : -1); }
      } else Log("big brother: '%s' shot %d: no destroyer (no Fletcher in the entity pool, or no boat)", StatsOf(r.mount).name.c_str(), r.shot);
    }
    if (ent) {
      AddHelperLook(ent, s0, s0.x != 0, r.kind == 2);
      g_minions.push_back({ ent, Pin(ent), r.mount, r.kind, GetTickCount(), life, r.owner });
      if (r.kind == 3) g_minions.back().hp0 = HullHp(ent);
      if (r.kind == 2) { g_minions.back().vel = V3{ flat.x * 90, 0, flat.z * 90 }; g_minions.back().lastPos = PosOf(ent); }   // its launch, for GullSteer
      if (r.owner < 0) ShowMessage(Colored(says[r.kind], "#ff5fc8"));
      UnuStats& st = StatsOf(r.mount); st.spawned[r.kind]++;
      DLog("extra effects: '%s' shot %d: %s out (%d of %d now, stays %u s)", st.name.c_str(), r.shot, kMinionWord[r.kind], MinionsOf(r.mount, r.kind), cap[r.kind], (unsigned)(life / 1000));
      if (g_coopRole == 1) CoopHelperMade(ent, r.owner, r.kind);       // co-op host: its size (and a gull's look) goes to the others
    }
  }
  g_spawnQueue.clear();
  DWORD tick = GetTickCount();
  static void *mTime = nullptr, *mActiveSelf = nullptr; static bool timeTried = false;
  if (!timeTried) { timeTried = true; void* tm = FindClass("UnityEngine", "Time"); mTime = tm ? FindMethod(tm, "get_time", 0) : nullptr; void* go = FindClass("UnityEngine", "GameObject"); mActiveSelf = go ? FindMethod(go, "get_activeSelf", 0) : nullptr; }
  void* tb = mTime ? Invoke(mTime, nullptr) : nullptr; float now = tb ? *(float*)((char*)tb + 0x10) : tick / 1000.0f;   // game time (slow motion too)
  for (size_t i = 0; i < g_minions.size(); ) {
    Minion& m = g_minions[i];
    auto si = g_unuStats.find(m.mount); UnuStats& st = si != g_unuStats.end() ? si->second : StatsOf(m.mount);
    bool alive = *(bool*)((char*)m.ent + g_offLeAlive);
    if (m.kind == 2) { float da = m.ageT >= 0 ? now - m.ageT : 0; if (da > 0 && da < 2) m.age += da; m.ageT = now; }   // game time: 0 while paused
    void* boom = nullptr; V3 boomAt{}; bool sea = false;               // the ship a gull reached, and where its splash goes
    if (alive && m.kind == 2) {
      void* go = Invoke(m_getGameObject, m.ent); void* ab = go && mActiveSelf ? Invoke(mActiveSelf, go) : nullptr;
      if (ab && !*(bool*)((char*)ab + 0x10)) {                         // back in its pool while alive: PlaneTerrainDetector (a hill)
        unsigned secs = (unsigned)m.age; V3 p = PosOf(m.ent);
        if (!m.full && g_gullFull < 3) { g_gullFull++; m.full = true; }
        if (m.steered) { st.gullNearSum += (std::min)(m.nearest, 3000.0f); st.gullNearN++; }
        if (m.target && Dist3(p, m.aim) < 60) {                        // the hill in front of its shore gun: count it, splash there
          st.gullHits++; QueueGullSplash(m, m.aim, m.target);
          if (m.full) Log("kamikaze gull: '%s' gull hit the ground %.0f m from %s '%s' after %u s; its splash went on the target", st.name.c_str(), Dist3(p, m.aim), ClassOf(m.target), NameOf(m.target).c_str(), secs);
        } else {
          st.gullTerrain++;
          if (m.full) Log("kamikaze gull: '%s' gull hit the ground after %u s%s", st.name.c_str(), secs, m.target ? ", on its way to a target" : "");
        }
        GullRelease(m); Unpin(m.handle); g_minions.erase(g_minions.begin() + i); continue;
      }
      V3 p = PosOf(m.ent);
      if (p.y > -5000) {
        int r = GullSteer(m, st, p, now, tick); float at[3] = { p.x, p.y, p.z }; V3 q;
        if (r == 1) { boom = m.target; boomAt = m.aim; }
        else if (r == 2) sea = true;
        else if (m.target && (boom = NearestOtherEnemyShip(at, nullptr, 12.0f, &q, true)) != nullptr) boomAt = boom == m.target ? m.aim : AimPointOn(boom);   // ran into one
        else if ((boom = NearestOtherEnemyShip(at, m.target, 35.0f, &q, true)) != nullptr) boomAt = AimPointOn(boom);   // boats and shore guns
      }
    }
    bool expired = m.kind == 2 ? m.age * 1000 > m.life : tick - m.born > m.life;
    if (expired && m.kind == 3 && craft && tick - m.born < m.life + 30000 && Dist3(PosOf(m.ent), PosOf(craft)) < 700) expired = false;   // not under your nose
   // after GullSteer: a steered gull gets time to get there; gulls in game time
    if (alive && !boom && !sea && !expired) { i++; continue; }
    unsigned secs = m.kind == 2 ? (unsigned)m.age : (tick - m.born) / 1000;
    if (m.kind == 2) {
      if (!m.full && g_gullFull < 3) { g_gullFull++; m.full = true; }  // the first 3 gulls a battle are logged in full
      if (m.steered) { st.gullNearSum += (std::min)(m.nearest, 3000.0f); st.gullNearN++; }
    }
    std::string tn = m.target ? NameOf(m.target) : std::string();
    if (!alive) {
      st.lost[m.kind]++;
      DLog("extra effects: '%s' %s %s after %u s", st.name.c_str(), kMinionWord[m.kind], m.kind == 1 || m.kind == 3 ? "sunk" : "lost (shot down or crashed)", secs);
      if (m.kind == 3) { st.bbDamage += m.hp0 > 0 ? m.hp0 : 0; if (g_bbLogs < 6) { g_bbLogs++; Log("big brother: '%s' destroyer was sunk after %u s (%d kills by it this battle)", st.name.c_str(), secs, st.minionKills[3]); } }
      if (m.kind == 2 && m.full) {
        if (m.target) Log("kamikaze gull: '%s' gull was shot down after %u s, %.0f m short of '%s'", st.name.c_str(), secs, m.lastD < 1e29f ? m.lastD : -1.0f, tn.c_str());
        else Log("kamikaze gull: '%s' gull was shot down after %u s", st.name.c_str(), secs);
      }
      GullRelease(m);                                                  // its AI back on: the game flies its fall
      Unpin(m.handle);
    } else if (boom) {
      st.gullHits++;
      DLog("extra effects: '%s' gull blew up next to '%s' after %u s", st.name.c_str(), NameOf(boom).c_str(), secs);
      if (m.full) Log("kamikaze gull: '%s' gull blew up next to %s '%s' after %u s (%.0f m from its aim point%s)", st.name.c_str(), ClassOf(boom), NameOf(boom).c_str(), secs,
                      Dist3(PosOf(m.ent), boomAt), m.steered ? "" : ", flying on the game's AI");
      Despawn(m, true, &boomAt, boom);
    } else if (sea) {
      st.gullSea++;
      if (m.full) Log("kamikaze gull: '%s' gull came down in the sea %.0f m short of '%s' after %u s", st.name.c_str(), m.lastD, tn.c_str(), secs);
      Despawn(m, true);
    } else {
      if (m.kind == 2 && m.steered) {
        st.gullMissed++;
        if (m.full) {
          if (m.target) Log("kamikaze gull: '%s' gull ran out of time after %u s, %.0f m from '%s' (closest %.0f m), and was removed", st.name.c_str(), secs, m.lastD, tn.c_str(), (std::min)(m.nearest, 9999.0f));
          else Log("kamikaze gull: '%s' gull's target was gone and it found no other in %u s; it was removed", st.name.c_str(), secs);
        }
      } else {
        st.expired[m.kind]++;
        if (m.kind == 2 && m.full) Log("kamikaze gull: '%s' gull found no target in %u s and was removed", st.name.c_str(), secs);
      }
      DLog("extra effects: '%s' %s %s after %u s", st.name.c_str(), kMinionWord[m.kind], m.kind == 2 ? "found no target and was removed" : (m.kind == 1 ? "sailed home" : m.kind == 3 ? "steamed home" : "flew home"), secs);
      if (m.kind == 3) {
        float lost = m.hp0 > 0 ? m.hp0 - HullHp(m.ent) : 0; if (lost > 0) st.bbDamage += lost;
        if (g_bbLogs < 6) { g_bbLogs++; Log("big brother: '%s' destroyer steamed home after %u s, %.0f m from your boat, %.0f hull HP lost (%d kills by it this battle)", st.name.c_str(), secs, craft ? Dist3(PosOf(m.ent), PosOf(craft)) : -1.0f, lost, st.minionKills[3]); }
      }
      Despawn(m, m.kind == 2);
    }
    g_minions.erase(g_minions.begin() + i);
  }
  HelperLooksTick();
}

// A kill credited to an Unusual gun, or to one of the planes and boats it called in (their own guns are credited:
// WeaponSystem.weaponManager +0x68 -> WeaponManager.entity +0x20, from the dump). Two gunners on one gun credit the
// same kill twice, so a kill counts once per gun or helper.
static void UnusualKill(void* ws, void* mount, void* killed) {
  if (!g_unuEng || !killed || g_unuStats.empty()) return;
  void* wm = ws ? *(void**)((char*)ws + 0x68) : nullptr; void* ent = wm ? *(void**)((char*)wm + 0x20) : nullptr;
  void* by = nullptr; int kind = -1;
  for (auto& m : g_minions) if (ent && m.ent == ent) { by = m.mount; kind = m.kind; break; }
  DWORD now = GetTickCount(); void* key = nullptr;
  if (!by && !g_gullBlasts.empty()) {                                  // a gull's splash (credited to its gun's WeaponSystem): the gull's kill
    V3 kp = PosOf(killed);
    for (auto b = g_gullBlasts.rbegin(); b != g_gullBlasts.rend(); ++b)
      if (now - b->t < 6000 && b->mount == mount && (b->target == killed || Dist3(kp, b->at) < 50) && g_unuStats.count(b->mount)) { by = b->mount; kind = 2; key = b->mount; break; }
  }
  if (!by) { if (!mount || !g_unuStats.count(mount)) return; by = mount; }
  static void* seen[16][2]; static DWORD seenAt[16]; static int seenPos = 0;
  if (!key) key = kind >= 0 ? ent : mount;
  for (int i = 0; i < 16; i++) if (seen[i][0] == killed && seen[i][1] == key && now - seenAt[i] < 3000) return;
  seen[seenPos][0] = killed; seen[seenPos][1] = key; seenAt[seenPos] = now; seenPos = (seenPos + 1) % 16;
  UnuStats& st = StatsOf(by);
  const char* what = il2cpp_class_get_name(il2cpp_object_get_class(killed));
  if (kind == 3 && st.minionKills[3] < 3) Log("big brother: '%s' destroyer got a kill: %s '%s'", st.name.c_str(), what, NameOf(killed).c_str());
  if (kind == 2 && st.minionKills[2] < 5) Log("kamikaze gull: '%s' gull's blast got a kill: %s '%s'", st.name.c_str(), what, NameOf(killed).c_str());
  if (kind >= 0) { st.minionKills[kind]++; DLog("extra effects: '%s' %s got a kill: %s '%s'", st.name.c_str(), kMinionWord[kind], what, NameOf(killed).c_str()); }
  else { st.kills++; DLog("extra effects: '%s' kill: %s '%s'", st.name.c_str(), what, NameOf(killed).c_str()); if (st.cfg.homerun > 0) HomeRunLaunch(by, killed, st); if (st.cfg.firemission > 0) FireMissionKill(st, by, killed); }
}

static void SafeUnusualOnShot(void* pj, void* mount) {
  if (g_unuFaults < 0) return;
  __try { UnusualOnShot(pj, mount); }
  __except (FaultFilter("extra effects (shot)", GetExceptionInformation())) { g_unuFaults = -1; }
}

static void (*g_pmUpdateOriginal)(void*, void*) = nullptr;
static int g_critFaults = 0;
static void SafeGfxTick(bool beforeCrits);                             // gun effects: hooks, spills, salvos, marks
static void SafeFrugalRestore();                                       // Frugal: barrels' skip-ammo flags back (GfxShot runs inside CritScan)
static void Hooked_PMUpdate(void* self, void* method) {
  if (g_ready) SafeGfxTick(true);
  if (g_ready && g_critFaults >= 0) {
    __try { CritScan(self); }
    __except (FaultFilter("critical hits", GetExceptionInformation())) { g_critFaults = -1; SafeFrugalRestore(); }
  }
  if (g_ready && g_seekFaults >= 0) {
    __try { SeekScan(self); }
    __except (FaultFilter("seeking shots", GetExceptionInformation())) { g_seekFaults = -1; }
  }
  if (g_ready && g_unuFaults >= 0) {
    __try { UnusualProjScan(self); }
    __except (FaultFilter("extra effects (shots)", GetExceptionInformation())) { g_unuFaults = -1; }
  }
  if (g_ready && g_smokeFaults >= 0) {                                 // also when barrel_smoke was just switched off: it ends the smoke
    __try { SmokeTick(); }
    __except (FaultFilter("barrel smoke", GetExceptionInformation())) { g_smokeFaults = -1; }
  }
  if (g_ready) SafeGfxTick(false);                                     // after CritScan saw this frame's new shots, before endJobs
  g_pmUpdateOriginal(self, method);
}

