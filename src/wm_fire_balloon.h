// Fire Mission and Up, Up and Away (balloon) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Fire Mission: every Nth direct hit marks the target with smoke; 4 s of game time later 6 splashes walk across it ---
// The hits come from the hitFlash hook (GfxBefore: direct hits of the player's own shells only); the splashes are
// QueueSplash credited to the gun's WeaponSystem (their kills reach UnusualKill as the gun's), with a stock shell's water
// splash (TorpSplash) for the look. Each splash goes where the target is at that moment, so the barrage follows it.
static bool FireMissionSmoke(V3 p);                                   // the utility gear section: one small smoke cloud
struct FireMission { void* mount; void* target; void* th; V3 mark, last, dir; float t0; int fired, queued; bool gone; std::string who; };
static std::vector<FireMission> g_fireMissions;
static std::unordered_map<void*, float> g_fmReady;                   // mount -> game time its next mission may be called
static int g_fmLogs = 0;                                              // per battle, the first 3 barrages in full
static const float kFmDelay = 4, kFmGap = 0.35f, kFmCooldown = 20, kFmRadius = 18, kFmDamage = 180;   // 180, not 250: 6 x 250 sank every small boat
static const int kFmShells = 6;
static void FireMissionHit(void* mount, void* ce) {
  const UnusualCfg& c = UnuCfgOf(mount); if (c.firemission <= 0) return;
  UnuStats& st = StatsOf(mount);
  st.fmHits++;
  if (++st.fmCount < c.firemission) return;
  if (g_coopRole == 2) {                                              // as Overkill: a client's splashes are not the host's damage
    st.fmCount = 0; if (!st.fmClient++) Log("fire mission: '%s' hit %d times on a co-op client; only the host calls barrages", st.name.c_str(), st.fmHits); return; }
  float now = CrewLogNow();
  auto r = g_fmReady.find(mount);
  if (r != g_fmReady.end() && now < r->second) { st.fmCooling++; return; }   // the count stays full: the first hit after the cooldown calls it
  V3 p = PosOf(ce); if (p.y < -5000) return;
  st.fmCount = 0; g_fmReady[mount] = now + kFmCooldown;
  bool smoke = FireMissionSmoke(p); st.fmSmoke += smoke;
  std::string who = KindOf(ce).ship ? ShipTitle(ce) : CrewClean(StripTags(NameOf(ce)));
  g_fireMissions.push_back({ mount, ce, Pin(ce), p, p, V3{ 0, 0, 0 }, now, 0, 0, false, who });
  st.fmCalled++;
  if (st.fmCalled <= 3) Log("fire mission: '%s' hit %s (hit %d this battle): marked%s, barrage in %.0f s", st.name.c_str(), who.c_str(), st.fmHits, smoke ? " with smoke" : " (NO smoke cloud)", kFmDelay);
  else DLog("fire mission: '%s' marked %s", st.name.c_str(), who.c_str());
  ShowMessage(Colored("Fire Mission: " + who + " marked", "#ff5fc8"));
}
static void SafeFireMissionHit(void* mount, void* ce) {
  if (g_unuFaults < 0) return;
  __try { FireMissionHit(mount, ce); }
  __except (FaultFilter("extra effects (fire mission hit)", GetExceptionInformation())) { g_unuFaults = -1; }
}
static void FireMissionEnd(size_t i) { Unpin(g_fireMissions[i].th); g_fireMissions.erase(g_fireMissions.begin() + i); }
static void FireMissionTick() {
  if (g_fireMissions.empty()) return;
  TorpSetup();                                                        // the splash prefab
  float now = CrewLogNow();
  for (size_t i = g_fireMissions.size(); i-- > 0; ) {
    FireMission& f = g_fireMissions[i];
    auto si = g_unuStats.find(f.mount); if (si == g_unuStats.end()) { FireMissionEnd(i); continue; }
    UnuStats& st = si->second;
    if (!f.gone) {                                                    // follow it; a sunk or repooled target leaves its last place
      V3 p = Alive(f.target) ? PosOf(f.target) : V3{ 0, -10000, 0 };
      if (p.y < -5000 || Dist3(p, f.last) > 150) f.gone = true; else f.last = p;
    }
    float t = now - f.t0 - kFmDelay;
    if (t < 0) continue;
    if (f.fired < kFmShells && t >= kFmGap * f.fired) {
      if (!f.fired) {                                                 // walk along the way it moved; away from the gun if it sat still
        V3 g = PosOf(f.mount);
        float dx = f.last.x - f.mark.x, dz = f.last.z - f.mark.z;
        if (dx * dx + dz * dz < 25 && g.y > -5000) { dx = f.last.x - g.x; dz = f.last.z - g.z; }
        float l = sqrtf(dx * dx + dz * dz); f.dir = l > 0.1f ? V3{ dx / l, 0, dz / l } : V3{ 1, 0, 0 };
      }
      float off = -30 + 60.0f * f.fired / (kFmShells - 1);
      V3 at{ f.last.x + f.dir.x * off, f.last.y, f.last.z + f.dir.z * off };
      void* ws = Alive(f.mount) && g_oGmWs >= 0 ? *(void**)((char*)f.mount + g_oGmWs) : nullptr;
      bool ok = ws && QueueSplash(at, kFmRadius, kFmDamage, ws, g_gfxEntityCode, g_gfxTeam);
      TorpSplash(at);
      f.fired++; f.queued += ok; if (ok) st.fmShells++; else st.fmNotQueued++;
      DLog("fire mission: '%s' shell %d of %d on %s at %+.0f m (splash %s)", st.name.c_str(), f.fired, kFmShells, f.who.c_str(), off, ok ? "queued" : "NOT queued");
      if (f.fired == kFmShells) {
        st.fmBarrages++;
        if (++g_fmLogs <= 3) Log("fire mission: '%s' barrage on %s done: %d of %d splashes queued (%.0f damage, %.0f m each); the target is %s, %.0f m from where it was marked",
                                 st.name.c_str(), f.who.c_str(), f.queued, kFmShells, kFmDamage, kFmRadius, f.gone ? "gone" : "still there", Dist3(f.last, f.mark));
        else DLog("fire mission: '%s' barrage on %s done (%d of %d queued)", st.name.c_str(), f.who.c_str(), f.queued, kFmShells);
      }
    }
    if (f.fired >= kFmShells && t > kFmGap * kFmShells + 3) FireMissionEnd(i);   // kept 3 s more: a late kill still counts as the barrage's
  }
}
// The gun's kill of a target whose barrage has started counts as the barrage's (its own shells may have done it too).
static void FireMissionKill(UnuStats& st, void* by, void* killed) {
  for (auto& f : g_fireMissions) {
    if (f.target != killed || f.mount != by || !f.fired) continue;
    st.fmKills++;
    if (st.fmKills <= 3) Log("fire mission: '%s' barrage killed %s (after %d of %d splashes)", st.name.c_str(), f.who.c_str(), f.fired, kFmShells);
    else DLog("fire mission: '%s' barrage killed %s", st.name.c_str(), f.who.c_str());
    return;
  }
}
static void FireMissionBattleOver() {
  for (auto& e : g_unuStats) {                                       // always logged, like the gulls
    const UnuStats& s = e.second; if (s.cfg.firemission <= 0 || !s.fmHits) continue;
    Log("fire mission, last battle: '%s' %d direct hits, %d missions called (%d with smoke; %d hits waited on the %.0f s cooldown), %d barrages, %d splashes queued (%d not), %d targets killed during a barrage",
        s.name.c_str(), s.fmHits, s.fmCalled, s.fmSmoke, s.fmCooling, kFmCooldown, s.fmBarrages, s.fmShells, s.fmNotQueued, s.fmKills);
  }
  for (auto& f : g_fireMissions) Unpin(f.th);
  g_fireMissions.clear(); g_fmReady.clear(); g_fmLogs = 0;
}

// ---- Up, Up and Away: direct hits swell an enemy ship like a balloon; after N it lifts off, then pops ------------------
// The hits come from the hitFlash hook (GfxBefore, direct hits of the player's own shells on ships). Each puff scales the
// ship's localScale by 1.08 (to 1.5x) as the Elite size traits do at spawn; rescaling a LIVE ship is new, so the first
// balloon of a battle logs its height, vertical speed, scale and FloatBehaviour._inWater every second. Lift = an upward
// Rigidbody.AddForce(VelocityChange) of 1.15 g x dt each frame while vertical speed is under 6 m/s. It pops after 12 s of
// lift or 60 m up: QueueSplash credited to the gun's WeaponSystem, a water splash below, size back. The size goes back
// on every path: pop, death, back in the pool (inactive), battle end and an UnusualTick fault. Elite-resized ships are
// left to RestoreAllShips at battle end (it runs first and puts their spawn size back). Host only in co-op.
struct Balloon { void* ship; void* h; void* mount; V3 scale0; int puffs; float factor, lastPuff, y0, yPuff, top, tLift, tPop, lastT, nextLog; bool lifting, popped, elite, loud, pushed; void* fb; std::string who; };
static std::vector<Balloon> g_balloons;
static std::unordered_set<void*> g_balloonDone;                       // popped this battle: never pumped up again
static int g_balloonLogs = 0, g_balloonLoud = 0;                      // per battle: first 3 lift-offs/pops in full; samples for the first balloon
static const float kUpPuff = 1.08f, kUpMax = 1.5f, kUpGap = 0.4f, kUpLiftG = 1.15f, kUpVyCap = 6, kUpSecs = 12, kUpHeight = 60, kUpDamage = 400, kUpRadius = 15;
static void *g_upVel = nullptr, *g_upGrav = nullptr, *g_upFbReg = nullptr; static int g_upFbEnt = -1, g_upFbIn = -1;
static void BalloonLookups() {
  static bool tried = false; if (tried) return; tried = true;
  void* le = FindClass("Core", "LiveEntity"); void* rb = FindClass("UnityEngine", "Rigidbody"); void* fb = FindClass("Core", "FloatBehaviour");
  if (!g_hrBody) g_hrBody = le ? FindMethod(le, "get_Body", 0) : nullptr;
  if (!g_hrForce) g_hrForce = rb ? FindMethod(rb, "AddForce", 2, "UnityEngine.Vector3", "UnityEngine.ForceMode") : nullptr;
  g_upVel = rb ? FindMethod(rb, "get_linearVelocity", 0) : nullptr; g_upGrav = rb ? FindMethod(rb, "get_useGravity", 0) : nullptr;
  g_upFbReg = fb ? il2cpp_class_get_field_from_name(fb, "register") : nullptr;
  g_upFbEnt = FieldOffset(fb, "entity"); g_upFbIn = FieldOffset(fb, "_inWater");
  if (!g_hrBody || !g_hrForce || !g_upVel) Log("up, up and away: a lookup failed (get_Body %p AddForce %p get_linearVelocity %p); ships swell but do not lift", g_hrBody, g_hrForce, g_upVel);
  if (!g_upFbReg || g_upFbEnt < 0 || g_upFbIn < 0) Log("up, up and away: FloatBehaviour register/entity/_inWater not found (%p %d %d); in-water state not logged", g_upFbReg, g_upFbEnt, g_upFbIn);
}
static void* BalloonFloat(void* ship) {                               // the ship's FloatBehaviour (only for the log)
  void* reg = nullptr; if (!g_upFbReg || g_upFbEnt < 0) return nullptr;
  il2cpp_field_static_get_value(g_upFbReg, &reg);
  for (int i = 0; i < ListCount(reg); i++) { void* f = ListAt(reg, i); if (f && *(void**)((char*)f + g_upFbEnt) == ship) return f; }
  return nullptr;
}
static int BalloonInWater(const Balloon& b) { return b.fb && Alive(b.fb) && g_upFbIn >= 0 ? *(bool*)((char*)b.fb + g_upFbIn) : -1; }
static float BalloonVy(void* ship) {
  void* body = g_hrBody ? Invoke(g_hrBody, ship) : nullptr; void* v = body && g_upVel ? Invoke(g_upVel, body) : nullptr;
  return v ? ((V3*)((char*)v + 0x10))->y : 0;
}
static void BalloonScale(void* ship, V3 s) { void* tr = Invoke(m_getTransform, ship); if (tr) { void* a[1] = { &s }; Invoke(m_setLocalScale, tr, a); } }
static void BalloonDeflate(Balloon& b) {                               // size back (once); the record stays for the fall log
  if (b.factor != 1 && Alive(b.ship)) BalloonScale(b.ship, b.scale0);
  b.factor = 1;
}
static void BalloonEnd(size_t i) { BalloonDeflate(g_balloons[i]); Unpin(g_balloons[i].h); g_balloons.erase(g_balloons.begin() + i); }
static void BalloonHit(void* mount, void* ship) {
  const UnusualCfg& c = UnuCfgOf(mount); if (c.upup <= 0) return;
  UnuStats& st = StatsOf(mount);
  st.upHits++;
  if (g_coopRole == 2) { if (!st.upClient++) Log("up, up and away: '%s' hit on a co-op client; the host moves the ships, so no balloons here", st.name.c_str()); return; }
  if (g_balloonDone.count(ship) || !IsShip(ship)) return;
  float now = CrewLogNow();
  Balloon* b = nullptr;
  for (auto& x : g_balloons) if (x.ship == ship) { b = &x; break; }
  if (b && (b->popped || now - b->lastPuff < kUpGap)) return;         // a machine gun's burst counts as one puff per 0.4 s
  if (!b) {
    void* tr = Invoke(m_getTransform, ship); void* sb = tr ? Invoke(m_getLocalScale, tr) : nullptr; if (!sb) return;
    BalloonLookups();
    V3 p = PosOf(ship);
    Balloon n{ ship, Pin(ship), mount, *(V3*)((char*)sb + 0x10), 0, 1, 0, p.y, p.y, p.y, 0, 0, now, 0, false, false, false, false, false, BalloonFloat(ship), ShipTitle(ship) };
    auto er = g_eliteRestore.find(ship); n.elite = er != g_eliteRestore.end() && er->second.scaled;
    n.loud = g_balloonLoud++ == 0;
    g_balloons.push_back(n); b = &g_balloons.back();
  }
  b->puffs++; b->lastPuff = now; st.upPuffs++;
  float f = b->factor * kUpPuff; if (f > kUpMax) f = kUpMax;
  if (f != b->factor) { b->factor = f; BalloonScale(ship, V3{ b->scale0.x * f, b->scale0.y * f, b->scale0.z * f }); }
  void* tr = Invoke(m_getTransform, ship); void* sb = tr ? Invoke(m_getLocalScale, tr) : nullptr; V3 s = sb ? *(V3*)((char*)sb + 0x10) : V3{ 0, 0, 0 };
  float y = PosOf(ship).y;
  if (st.upPuffs <= 3) Log("up, up and away: '%s' puff %d on %s: scale x%.2f (localScale %.2f,%.2f,%.2f, was %.2f,%.2f,%.2f%s), y %.1f (%.1f before the first puff), in water %d",
                           st.name.c_str(), b->puffs, b->who.c_str(), b->factor, s.x, s.y, s.z, b->scale0.x, b->scale0.y, b->scale0.z, b->elite ? ", Elite size" : "", y, b->y0, BalloonInWater(*b));
  else DLog("up, up and away: '%s' puff %d on %s, scale x%.2f, y %.1f", st.name.c_str(), b->puffs, b->who.c_str(), b->factor, y);
  if (!b->lifting && b->puffs >= c.upup) {
    b->lifting = true; b->tLift = now; b->lastT = now; b->y0 = y; b->top = y; st.upLifts++;
    if (g_upGrav && g_hrBody) { static bool told = false; void* body = Invoke(g_hrBody, ship); void* g = body ? Invoke(g_upGrav, body) : nullptr;
      if (!told && g) { told = true; Log("up, up and away: a swollen ship's Rigidbody useGravity %d", *(bool*)((char*)g + 0x10)); } }
    if (++g_balloonLogs <= 3) Log("up, up and away: '%s' %s lifts off after %d puffs (scale x%.2f, y %.1f; it sat %+.1f m from where it floated unswollen)", st.name.c_str(), b->who.c_str(), b->puffs, b->factor, y, y - b->yPuff);
    else DLog("up, up and away: '%s' %s lifts off", st.name.c_str(), b->who.c_str());
    ShowMessage(Colored("Up, up and away: " + b->who, "#ff5fc8"));
  }
}
static void SafeBalloonHit(void* mount, void* ship) {
  if (g_unuFaults < 0) return;
  __try { BalloonHit(mount, ship); }
  __except (FaultFilter("extra effects (balloon hit)", GetExceptionInformation())) { g_unuFaults = -1; SafeBalloonRestoreAll(); }
}
static void BalloonPop(Balloon& b, UnuStats& st, V3 p, float t) {
  b.popped = true; b.tPop = CrewLogNow(); st.upPops++; g_balloonDone.insert(b.ship);
  BalloonDeflate(b);
  void* ws = Alive(b.mount) && g_oGmWs >= 0 ? *(void**)((char*)b.mount + g_oGmWs) : nullptr;
  bool ok = ws && QueueSplash(p, kUpRadius, kUpDamage, ws, g_gfxEntityCode, g_gfxTeam);
  TorpSetup(); TorpSplash(V3{ p.x, b.y0, p.z });                     // the splash on the water below
  if (void* body = g_hrBody ? Invoke(g_hrBody, b.ship) : nullptr) { V3 v{ 0, -BalloonVy(b.ship) - 2, 0 }; int mode = 2; void* a[2] = { &v, &mode }; Invoke(g_hrForce, body, a); }   // stop climbing
  float up = p.y - b.y0;
  if (st.upPops <= 3) Log("up, up and away: '%s' %s popped %.0f m up after %.1f s of lift (blast %s, %.0f damage / %.0f m); size back", st.name.c_str(), b.who.c_str(), up, t, ok ? "queued" : "NOT queued", kUpDamage, kUpRadius);
  else DLog("up, up and away: '%s' %s popped %.0f m up", st.name.c_str(), b.who.c_str(), up);
  ShowMessage(Colored("Pop! " + b.who, "#ff5fc8"));
}
static void BalloonTick() {
  if (g_balloons.empty()) return;
  static void* goActive = nullptr; if (!goActive) { void* gc = FindClass("UnityEngine", "GameObject"); goActive = gc ? FindMethod(gc, "get_activeSelf", 0) : nullptr; }
  float now = CrewLogNow();
  for (size_t i = g_balloons.size(); i-- > 0; ) {
    Balloon& b = g_balloons[i];
    auto si = g_unuStats.find(b.mount); if (si == g_unuStats.end()) { BalloonEnd(i); continue; }
    UnuStats& st = si->second;
    void* go = goActive && Alive(b.ship) ? Invoke(m_getGameObject, b.ship) : nullptr; void* act = go ? Invoke(goActive, go) : nullptr;
    if (!Alive(b.ship) || (act && !*(bool*)((char*)act + 0x10))) {    // back in the pool (or destroyed): size back before it is reused
      st.upLost++; DLog("up, up and away: %s left the battle (puffs %d, %s); size back", b.who.c_str(), b.puffs, b.popped ? "popped" : b.lifting ? "in the air" : "swollen");
      BalloonEnd(i); continue; }
    V3 p = PosOf(b.ship);
    bool alive = g_offLeAlive >= 0 && *(bool*)((char*)b.ship + g_offLeAlive);
    float up = p.y - b.y0, dt = now - b.lastT; b.lastT = now;
    if (b.lifting && p.y > b.top) { b.top = p.y; if (up > st.upBest) st.upBest = up; }
    if (!alive && !b.popped) {                                        // shot down while swollen: it falls (or sinks) at its own size
      st.upDowned++; b.popped = true; b.tPop = now; g_balloonDone.insert(b.ship); BalloonDeflate(b);
      if (st.upDowned <= 3) Log("up, up and away: %s was sunk while swollen (%d puffs, %s, %.0f m up); size back", b.who.c_str(), b.puffs, b.lifting ? "in the air" : "still on the water", b.lifting ? up : 0.0f);
      else DLog("up, up and away: %s sunk while swollen", b.who.c_str());
      if (!b.lifting) { BalloonEnd(i); continue; }
    }
    if (b.lifting && !b.popped) {
      float t = now - b.tLift;
      if (b.loud && now >= b.nextLog) {                               // the first balloon: how a scaled ship floats and rises
        b.nextLog = now + 1;
        Log("up, up and away: %s %.1f s into the lift: %.1f m up, vertical speed %.1f m/s, scale x%.2f, in water %d", b.who.c_str(), t, up, BalloonVy(b.ship), b.factor, BalloonInWater(b));
      }
      if (t >= kUpSecs || up >= kUpHeight) { BalloonPop(b, st, p, t); continue; }
      void* body = g_hrBody && g_hrForce && g_upVel ? Invoke(g_hrBody, b.ship) : nullptr;
      if (body && dt > 0 && dt < 0.5f) {
        float vy = BalloonVy(b.ship), dv = kUpLiftG * 9.81f * dt;
        if (vy + dv > kUpVyCap) dv = kUpVyCap - vy;
        if (dv > 0) { V3 v{ 0, dv, 0 }; int mode = 2; void* a[2] = { &v, &mode }; Invoke(g_hrForce, body, a); }   // ForceMode.VelocityChange
      }
      continue;
    }
    if (b.popped) {                                                   // falling: log the landing, push it down once if it hangs
      float t = now - b.tPop;
      if (p.y <= b.y0 + 2) {
        if (g_balloonLogs <= 3 || b.loud) Log("up, up and away: %s came down %.1f s after the pop (top %.0f m)", b.who.c_str(), t, b.top - b.y0);
        BalloonEnd(i); continue;
      }
      if (!b.pushed && t > 4 && p.y > b.top - 3) {
        b.pushed = true; void* body = Invoke(g_hrBody, b.ship);
        if (body) { V3 v{ 0, -15, 0 }; int mode = 2; void* a[2] = { &v, &mode }; Invoke(g_hrForce, body, a); }
        Log("up, up and away: %s still hung %.0f m up %.0f s after the pop; pushed down (no gravity on it?)", b.who.c_str(), up, t);
      }
      if (t > 25) { Log("up, up and away: %s had not come down 25 s after the pop (%.0f m up); stopped watching it", b.who.c_str(), up); BalloonEnd(i); }
      continue;
    }
    if (now - b.lastPuff > 60) { DLog("up, up and away: %s not hit for 60 s, %d puffs; size back", b.who.c_str(), b.puffs); BalloonEnd(i); }   // swollen but not lifted
  }
}
static void BalloonRestoreAll(bool battleEnd) {
  for (auto& b : g_balloons) { if (!(battleEnd && b.elite)) BalloonDeflate(b); Unpin(b.h); }   // Elite-sized: RestoreAllShips already put the spawn size back
  g_balloons.clear();
}
static void SafeBalloonRestoreAll() { __try { BalloonRestoreAll(false); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("up, up and away: ships could not be restored after the fault"); } }
static void BalloonBattleOver() {
  for (auto& e : g_unuStats) {                                       // always logged, like the gulls
    const UnuStats& s = e.second; if (s.cfg.upup <= 0 || !s.upHits) continue;
    Log("up, up and away, last battle: '%s' %d direct hits on ships, %d puffs, %d ships lifted off, %d popped, %d sunk while swollen, %d left while swollen, best %.0f m up",
        s.name.c_str(), s.upHits, s.upPuffs, s.upLifts, s.upPops, s.upDowned, s.upLost, s.upBest);
  }
  if (!g_balloons.empty()) Log("up, up and away: %d swollen ships put back to size at the battle change", (int)g_balloons.size());
  BalloonRestoreAll(true);
  g_balloonDone.clear(); g_balloonLogs = g_balloonLoud = 0;
}

struct HitCtx { bool post; void* mount; void* ce; const GunFx* fx; float hpBefore; bool client; bool direct; };
// Before the game's hitFlash: the damage to apply. ctx->post asks for GfxAfter.
static float GfxBefore(void* self, bool direct, char* crsp, float dmg, HitCtx* ctx) {
  ctx->post = false;
  if (!g_gfxCraft) return dmg;
  if (!g_brittle.empty()) {                                            // Brittle: the player's own gun hit
    auto b = g_brittle.find(self);
    if (b != g_brittle.end()) {
      GfxStats& st = GfxStat(b->second.name); st.brittleHits++; st.brittleDmg += dmg * (b->second.factor - 1);
      GFXLOG(GL_BRITTLE, "brittle: '%s' hit for %.0f, x%.2f = %.0f", b->second.name.c_str(), dmg, b->second.factor, dmg * b->second.factor);
      return dmg * b->second.factor;
    }
  }
  void* mount = nullptr; void* pj = nullptr; V3 at{};
  if (direct) {
    if (*(void**)(crsp + 0x160) != self) { if (g_gfxBadFrames++ < 3) Log("gun effects: a direct hit's frame did not hold its hull part (%d); that hit is left alone", g_gfxBadFrames); return dmg; }
    pj = *(void**)(crsp + 0x168); if (!pj || IsCopyShot(pj)) return dmg;
    mount = *(void**)((char*)pj + g_offPjFrom);
    at = *(V3*)((char*)pj + g_oPjPosi);
  } else {
    SplashData* sd = (SplashData*)(crsp + 0x90);
    if (sd->projectileCode == kSpillCode || (g_coopRole != 0 && !sd->networkOwner) || !sd->killer) return dmg;
    if (il2cpp_object_get_class(sd->killer) != g_clsWs) return dmg;   // a ship's own explosion, Volatile and so on
    mount = *(void**)((char*)sd->killer + g_offWsSelected);
    at = sd->pos;
  }
  if (!mount || *(void**)((char*)mount + g_oGmEntity) != g_gfxCraft) return dmg;   // the player's own guns only
  if (*(int*)((char*)self + g_oHpTeam) == g_gfxTeam) return dmg;
  void* ce = *(void**)((char*)self + g_oHpCe); if (!ce) return dmg;
  if (direct && KindOf(ce).surface) SafeFireMissionHit(mount, ce);     // one call per shell that hits (blasts never count)
  if (direct && KindOf(ce).ship) SafeBalloonHit(mount, ce);
  bool ship = KindOf(ce).ship;
  const GunFx& gx = GfxOf(mount);
  float f = 1.0f;
  if (ship && !g_marks.empty()) {                                      // Marking: every gun of the player's on a marked ship
    auto m = g_marks.find(ce);
    if (m != g_marks.end() && (int)(m->second.until - GetTickCount()) > 0) {
      GfxStats& st = GfxStat(gx.name); st.markedHits++; st.markDmg += dmg * m->second.bonus; f *= 1 + m->second.bonus;
      DLog("marking: '%s' hit marked '%s' for %.0f +%d%%", gx.name.c_str(), m->second.ship.c_str(), dmg, (int)std::lround(m->second.bonus * 100));
    }
  }
  if (gx.item) {
    float add = 0;                                                     // the conditional bonuses add up (cap +200%), so co-rolls do not multiply
    if (gx.giant > 0 && g_elites.count(ce)) {
      GfxStats& st = GfxStat(gx.name); st.giantHits++; st.giantDmg += dmg * (gx.giant - 1); add += gx.giant - 1;
      GFXLOG(GL_GIANT, "giant slayer: '%s' hit %s '%s' for %.0f x%.2f", gx.name.c_str(), g_elites[ce].tier == 2 ? "Elite" : "Rare", g_elites[ce].name.c_str(), dmg, gx.giant);
    }
    if (gx.salvo > 0 && SalvoOn(mount, direct, pj)) {
      GfxStats& st = GfxStat(gx.name); st.salvoHits++; st.salvoDmg += dmg * (gx.salvo - 1); add += gx.salvo - 1;
      DLog("opening salvo: '%s' %s for %.0f x%.2f", gx.name.c_str(), direct ? "hit" : "blast", dmg, gx.salvo);
    }
    if ((gx.closeB > 0 || gx.longB > 0) && m_trGetPos) {
      void* tr = Invoke(m_getTransform, mount); void* pb = tr ? Invoke(m_trGetPos, tr) : nullptr;
      if (pb) {
        V3 g = *(V3*)((char*)pb + 0x10); float d = sqrtf((g.x - at.x) * (g.x - at.x) + (g.y - at.y) * (g.y - at.y) + (g.z - at.z) * (g.z - at.z));
        float k = gx.closeB > 0 && d < gx.closeD ? gx.closeB : (gx.longB > 0 && d > gx.longD ? gx.longB : 1.0f);
        if (k != 1.0f) {
          GfxStats& st = GfxStat(gx.name); bool close = gx.closeB > 0;
          if (close) { st.closeHits++; st.closeDmg += dmg * (k - 1); } else { st.longHits++; st.longDmg += dmg * (k - 1); }
          add += k - 1;
          GFXLOG(GL_RANGE, "%s: '%s' %s at %.0f m for %.0f x%.2f", close ? "close quarters" : "long shot", gx.name.c_str(), direct ? "hit" : "blast", d, dmg, k);
        }
      }
    }
    if (gx.vetPer > 0) {
      const Item* it = FindItem(gx.name); int kills = GunnerKillsOn(mount, it);
      if (kills > 0) {
        float k = 1 + gx.vetPer * (float)(std::min)(kills, gx.vetCap);
        GfxStats& st = GfxStat(gx.name); st.vetHits++; st.vetDmg += dmg * (k - 1); add += k - 1;
        DLog("veteran's: '%s' (gunner '%s', %d kills) hit for %.0f x%.2f", gx.name.c_str(), it->vetCrew.c_str(), kills, dmg, k);
      }
    }
    if (add > 2.0f) {
      GfxStats& st = GfxStat(gx.name); st.capHits++; st.capCut += dmg * f * (add - 2.0f);
      GFXLOG(GL_CAP, "bonus cap: '%s' bonuses +%.0f%% held to +200%% on a %.0f hit", gx.name.c_str(), add * 100, dmg);
      add = 2.0f;
    }
    f *= 1 + add;
    if (ship && (gx.mark > 0 || gx.steady > 0 || (gx.overkill > 0 && g_coopRole != 2))) {
      ctx->post = true; ctx->mount = mount; ctx->ce = ce; ctx->fx = &gx; ctx->direct = direct;
      ctx->hpBefore = *(float*)((char*)self + g_oHpHp); ctx->client = *(bool*)((char*)self + g_oHpClient);
    }
  }
  return dmg * f;
}
static bool AllVitalDown(void* ce) {
  void* hull = g_oCeHull >= 0 ? *(void**)((char*)ce + g_oCeHull) : nullptr; if (!hull) return false;
  for (int i = 0; i < ListCount(hull); i++) {
    void* p = ListAt(hull, i);
    if (p && *(bool*)((char*)p + g_oHpVital) && *(float*)((char*)p + g_oHpHp) > 0) return false;
  }
  return true;
}
// After it: marks, steadying stacks, Overkill's kill check.
static void GfxAfter(void* self, const HitCtx* c) {
  const GunFx& gx = *c->fx; DWORD now = GetTickCount();
  // a direct hit is always its own hit; a blast's parts (all in one game step) count once, and so does the blast of a
  // shell that just hit directly (it arrives a step later: review 2026-11-06, a Bofors round gave two stacks). Both set
  // the window; only blasts are deduped by it, so steady direct fire from a fast gun still stacks with every round
  bool fresh = c->direct || !(c->mount == g_hitLastMount && c->ce == g_hitLastCe && now - g_hitLastAt < 120);
  g_hitLastMount = c->mount; g_hitLastCe = c->ce; g_hitLastAt = now;
  if (gx.mark > 0 && fresh) {
    auto m = g_marks.find(c->ce); bool was = m != g_marks.end() && (int)(m->second.until - now) > 0;
    MarkInfo& mi = g_marks[c->ce];
    if (mi.ship.empty()) mi.ship = ShipTitle(c->ce);
    if (!was || gx.mark >= mi.bonus) { mi.bonus = was ? (std::max)(mi.bonus, gx.mark) : gx.mark; mi.by = gx.name; }
    mi.until = now + (DWORD)(gx.markTime * 1000);
    if (!was) { GfxStat(gx.name).marks++; GFXLOG(GL_MARK, "marking: '%s' marked '%s' for %.0f s (+%d%% damage from all your guns)", gx.name.c_str(), mi.ship.c_str(), gx.markTime, (int)std::lround(mi.bonus * 100)); }
  }
  if (gx.steady > 0 && fresh && g_gfxCrewOn) {
    SteadyDecay();
    g_steadyPer = (std::max)(g_steadyPer, gx.steady); g_steadyCap = (std::max)(g_steadyCap, gx.steadyCap); g_steadyDecay = gx.steadyDecay;
    g_steadyStacks = (std::min)((float)g_steadyCap, g_steadyStacks + 1); if (g_steadyStacks > g_steadyPeak) g_steadyPeak = g_steadyStacks;
    GfxStat(gx.name).stacks++;
    DLog("sure-footed: '%s' hit: %.1f stacks (%d%% bracing)", gx.name.c_str(), g_steadyStacks, (int)std::lround((std::min)(0.95f, g_steadyStacks * g_steadyPer) * 100));
  }
  if (gx.overkill > 0 && g_coopRole != 2 && !c->client && c->hpBefore > 0 && *(float*)((char*)self + g_oHpHp) <= 0 && *(bool*)((char*)self + g_oHpVital)
      && AllVitalDown(c->ce)) {
    auto sp = g_spilled.find(c->ce);
    if (sp != g_spilled.end() && now - sp->second <= 1000) { DLog("overkill: '%s' hit '%s' again in the same death; no second spill", gx.name.c_str(), ShipTitle(c->ce).c_str()); return; }
    float excess = *(float*)((char*)self + g_oHpLastEff) - c->hpBefore;
    g_spilled[c->ce] = now;
    if (excess > 1) g_spills.push_back({ c->ce, c->mount, excess * gx.overkill, gx.name, ShipTitle(c->ce) });
    else DLog("overkill: '%s' sank '%s' with only %.0f to spare", gx.name.c_str(), ShipTitle(c->ce).c_str(), excess);
  }
}
static int GfxHitFilter(EXCEPTION_POINTERS* ep) { g_gfxHitOn = false; return FaultFilter("gun effects (hit)", ep); }
static void Hooked_HpHit(void* self, bool hit, float dmg, bool flash, float heat, void* mi) {
  uintptr_t ra = (uintptr_t)_ReturnAddress();
  if (!g_gfxHitOn || g_gfxFaults < 0 || !hit || !(dmg > 0) || (ra != g_raDirect && ra != g_raSplash)) { g_hpHitOrig(self, hit, dmg, flash, heat, mi); return; }
  char* crsp = (char*)_AddressOfReturnAddress() + 8;                  // the caller's rsp at the call
  HitCtx c = {}; float d = dmg;
  __try { d = GfxBefore(self, ra == g_raDirect, crsp, dmg, &c); }
  __except (GfxHitFilter(GetExceptionInformation())) { d = dmg; c.post = false; }
  g_hpHitOrig(self, hit, d, flash, heat, mi);
  if (!c.post || !g_gfxHitOn) return;
  __try { GfxAfter(self, &c); }
  __except (GfxHitFilter(GetExceptionInformation())) {}
}
// Sure-Footed's brace: overboardChance is 0 for one hitFlash call of the player's crew, with stacks x steady chance.
static bool SteadyBrace(void* self, float d) {
  void* mgr = *(void**)((char*)self + g_oCmMgr);
  if (!mgr || !g_gfxCraft || *(void**)((char*)mgr + g_oMgrEnt) != g_gfxCraft) return false;
  SteadyDecay();
  if (g_steadyStacks <= 0) return false;
  g_steadyHits++;
  float q = (std::min)(0.95f, g_steadyStacks * g_steadyPer);
  if (Rand01() >= q) return false;
  g_steadyBraced++;
  void* ch = g_oCmChar >= 0 ? *(void**)((char*)self + g_oCmChar) : nullptr;
  GFXLOG(GL_STEADY, "sure-footed: '%s' braced against a hit of %.0f (%.1f stacks, %d%%)", ch ? GfxCrewName(ch).c_str() : "a crew member", d, g_steadyStacks, (int)std::lround(q * 100));
  return true;
}
static int GfxCrewFilter(EXCEPTION_POINTERS* ep) { g_gfxCrewOn = false; return FaultFilter("gun effects (crew hit)", ep); }
static void SafeOverboardSet(float v) { __try { g_fieldStaticSet(g_fOverboard, &v); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
struct CrewHitPre { bool on; void* ch; float hp; int state; };
static void SafeCrewLogPre(void* self, CrewHitPre* p);
static void SafeCrewLogPost(void* self, float d, uintptr_t ra, char* crsp, const CrewHitPre* p);
static void CrewHitCall(void* self, bool hit, float d, void* mi);
static void Hooked_CrewHit(void* self, bool hit, float d, void* mi) {
  if (!hit) { g_crewHitOrig(self, hit, d, mi); return; }               // UpdateMe's per-frame tick: hitFlash(false, 0)
  uintptr_t ra = (uintptr_t)_ReturnAddress();
  char* crsp = (char*)_AddressOfReturnAddress() + 8;                  // the caller's rsp at the call
  CrewHitPre pre = {};
  SafeCrewLogPre(self, &pre);
  CrewHitCall(self, hit, d, mi);
  if (pre.on) SafeCrewLogPost(self, d, ra, crsp, &pre);
}
static void CrewHitCall(void* self, bool hit, float d, void* mi) {
  if (!(d > 3.0f) || !g_gfxCrewOn || g_gfxFaults < 0 || g_steadyStacks <= 0) { g_crewHitOrig(self, hit, d, mi); return; }
  bool brace = false;
  __try { brace = SteadyBrace(self, d); }
  __except (GfxCrewFilter(GetExceptionInformation())) { brace = false; }
  if (!brace) { g_crewHitOrig(self, hit, d, mi); return; }
  float saved = 0.02f; il2cpp_field_static_get_value(g_fOverboard, &saved);
  SafeOverboardSet(0.0f);
  __try { g_crewHitOrig(self, hit, d, mi); }
  __finally { SafeOverboardSet(saved); }                              // also when the game's code throws
}

