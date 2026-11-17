// crew hits: where a hit landed and who fired it (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- where a hit landed and who fired it ----
// Direct hit, caller ProjectileManager.endJobs (fixed frame, sub rsp,0x310): [crsp+0xA0] = the CrewMember (== self),
// [crsp+0x168] = the Projectile, [crsp+0x70] = RaycastHit.m_Point (the true contact; posi is the sweep START),
// [crsp+0x8C] = its distance, [crsp+0xD0] = (point + posi) / 2 (the game's fallback). Blast, caller
// SplashDamageManager.FixedUpdate (sub rsp,0x5B0): the SplashData copy at [crsp+0x90] (pos, range +0xA8, damage +0xB4,
// killer +0xB8, entityCode +0xC0, projectileCode +0xC8) and [crsp+0x2B0] = closest point on the man - blast centre.
// Splash killers: WeaponSystem (shells; selected gun only, the firing mount is not in the splash), TorpedoLauncher
// (torpedoes), null (fire: entity -999 + projectileCode 0; a damage-control explosion point: all three -999).
// The body is CrewMember.smoothPos (feet) / smoothChestPos / smoothHeadPos (world, eased per frame); the zone is a guess
// from where the hit point sits along feet->head and how far it is to the man's right.
struct CrewHitWhere { std::string part, source, enemy, enemyBase, how, shell; int pjCode = 0; bool close = false; };
static V3 V3Sub(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static float V3Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static std::string CrewEnemyOf(void* ce) {                            // a ship's title, else the entity's name (planes)
  if (!Alive(ce)) return std::string();
  if (ce == g_gfxCraft) return "our own boat";
  // elite names ('Long Boi No.1 Aux. Sub. Chaser') are kept for the battle: g_elites drops a ship as it dies, before
  // its death blast and the shells still in flight land
  auto& names = g_crewEliteName;
  auto e = g_elites.find(ce); if (e != g_elites.end() && !e->second.name.empty()) return names[ce] = e->second.name;
  auto n = names.find(ce); if (n != names.end()) return n->second;
  return IsShip(ce) ? ShipTitle(ce) : CrewClean(StripTags(NameOf(ce)));
}
static std::string CrewEnemyBase(void* ce) {                         // the plain ship name, without an elite's prefix
  if (!Alive(ce)) return std::string();
  if (ce == g_gfxCraft) return "our own boat";
  return IsShip(ce) ? ShipTitle(ce) : CrewClean(StripTags(NameOf(ce)));
}
static std::string CrewGunName(void* mount) {
  if (!Alive(mount)) return std::string();
  if (*(void**)((char*)mount + g_oGmEntity) == g_gfxCraft) return GfxOf(mount).name;
  void* gt = g_offGunType >= 0 ? *(void**)((char*)mount + g_offGunType) : nullptr;
  return Alive(gt) ? CrewClean(StripTags(NameOf(gt))) : std::string();
}
// Which part of the man is nearest point q; dir = the travel direction of what hit him (shell velocity, or blast centre
// -> man), for front or back.
static std::string CrewBodyPart(void* self, V3 q, V3 dir, bool log) {
  if (g_oCmFeet < 0 || g_oCmChestP < 0 || g_oCmHeadP < 0) return std::string();
  V3 F = *(V3*)((char*)self + g_oCmFeet), C = *(V3*)((char*)self + g_oCmChestP), H = *(V3*)((char*)self + g_oCmHeadP);
  V3 a = V3Sub(H, F); float L = sqrtf(V3Dot(a, a));
  if (!(L > 0.4f && L < 2.6f)) return std::string();                  // no sane body axis (not set up yet, or teleported)
  V3 u = { a.x / L, a.y / L, a.z / L };
  V3 qf = V3Sub(q, F);
  float t = V3Dot(qf, u) / L, tc = V3Dot(V3Sub(C, F), u) / L;
  float tNeck = tc + 0.5f * (1 - tc), tHip = 0.7f * tc;
  void* tr = Invoke(m_getTransform, self);
  V3 fwd = GetV3(m_crewTrFwd, tr), right = GetV3(m_crewTrRight, tr);
  V3 rad = { qf.x - t * L * u.x, qf.y - t * L * u.y, qf.z - t * L * u.z };
  bool haveSide = V3Dot(right, right) > 0.5f, haveFace = V3Dot(fwd, fwd) > 0.5f && V3Dot(dir, dir) > 1e-6f;
  float side = haveSide ? V3Dot(rad, right) : 0;
  bool back = haveFace && V3Dot(dir, fwd) > 0;                         // travelling the way he faces: it came from behind
  std::string lr = haveSide ? (side >= 0 ? "right " : "left ") : "";
  std::string part;
  if (t >= tNeck) part = back ? "the back of the head" : "the head";
  else if (t >= tHip) part = haveSide && fabsf(side) > 0.16f ? "the " + lr + "arm" : back ? "the back" : t > (tc + tHip) / 2 ? "the chest" : "the belly";
  else part = "the " + lr + "leg";
  if (log && g_debug && g_crewGeoLogs++ < 8)                          // debug=1 only
    Log("crew log: hit geometry: t %.2f (chest %.2f, neck %.2f, hip %.2f), body %.2f m, side %+.2f m, %s -> %s", t, tc, tNeck, tHip, L, side, back ? "from behind" : "from the front", part.c_str());
  return part;
}
static void CrewHitWhereOf(void* self, bool direct, bool splash, char* crsp, CrewHitWhere* w) {
  if (direct) {
    if (*(void**)(crsp + 0xA0) != self) return;                       // not the frame we traced
    void* pj = *(void**)(crsp + 0x168); if (!pj) return;
    if (g_oPjCode >= 0) w->pjCode = *(int*)((char*)pj + g_oPjCode);  // its burst's splash carries the same code
    void* pt = g_oPjType >= 0 ? *(void**)((char*)pj + g_oPjType) : nullptr;
    void* mount = g_offPjFrom >= 0 ? *(void**)((char*)pj + g_offPjFrom) : nullptr;
    V3 q = *(V3*)(crsp + 0x70); float dist = *(float*)(crsp + 0x8C);
    if (q.x == 0 && q.y == 0 && q.z == 0 && dist == 0) q = *(V3*)(crsp + 0xD0);   // the sweep started inside him
    V3 v = g_oPjVelo >= 0 ? *(V3*)((char*)pj + g_oPjVelo) : V3{ 0, 0, 0 };
    w->part = CrewBodyPart(self, q, v, true);
    std::string shell = Alive(pt) ? CrewClean(StripTags(NameOf(pt))) : std::string();
    w->source = CrewGunName(mount);
    w->enemy = Alive(mount) ? CrewEnemyOf(*(void**)((char*)mount + g_oGmEntity)) : std::string();
    w->enemyBase = Alive(mount) ? CrewEnemyBase(*(void**)((char*)mount + g_oGmEntity)) : std::string();
    w->shell = shell;
    w->how = shell.empty() ? "a round" : "a round (" + shell + ")";
    return;
  }
  if (!splash) return;
  SplashData* sd = (SplashData*)(crsp + 0x90);
  V3 d = *(V3*)(crsp + 0x2B0); float dist = sqrtf(V3Dot(d, d));
  V3 at = { sd->pos.x + d.x, sd->pos.y + d.y, sd->pos.z + d.z };
  if (dist > 0.05f) w->part = CrewBodyPart(self, at, d, false);
  bool close = sd->range > 0 && dist < 0.3f * sd->range;
  void* k = sd->killer;
  if (!k) { w->how = sd->entityCode == -999 && sd->projectileCode == 0 ? "fire" : "an explosion aboard"; return; }
  if (!Alive(k)) { w->how = close ? "a close blast" : "a blast"; return; }
  void* kc = il2cpp_object_get_class(k);
  if (kc == g_clsWs) {
    void* mount = g_offWsSelected >= 0 ? *(void**)((char*)k + g_offWsSelected) : nullptr;
    if (sd->projectileCode != -1) w->source = CrewGunName(mount);     // -1: the mod's elite death blast, no gun fired it
    w->enemy = Alive(mount) ? CrewEnemyOf(*(void**)((char*)mount + g_oGmEntity)) : std::string();
    w->enemyBase = Alive(mount) ? CrewEnemyBase(*(void**)((char*)mount + g_oGmEntity)) : std::string();
    w->close = close;
    w->how = sd->projectileCode == -1 ? "the blast of a dying ship" : close ? "a close shell burst" : "a shell burst";
  } else if (kc == g_clsTorpLauncher) {
    w->enemy = g_oTlEntity >= 0 ? CrewEnemyOf(*(void**)((char*)k + g_oTlEntity)) : std::string();
    w->enemyBase = g_oTlEntity >= 0 ? CrewEnemyBase(*(void**)((char*)k + g_oTlEntity)) : std::string();
    w->how = "a torpedo blast";
  } else w->how = close ? "a close blast" : "a blast";
}
static std::string CrewFrom(const CrewHitWhere& w) {                  // " from the Type 96 of 'IJN Patrol T1'"
  if (w.source.empty() && w.enemy.empty()) return std::string();
  if (w.enemy == "our own boat") return " from our own " + (w.source.empty() ? std::string("guns") : w.source);
  std::string o = " from ";
  if (!w.source.empty()) o += "the " + w.source + (w.enemy.empty() ? "" : " of ");
  if (!w.enemy.empty()) o += "'" + w.enemy + "'";
  return o;
}
static void CrewLogPost(void* self, float d, uintptr_t ra, char* crsp, const CrewHitPre* p) {
  CrewRec* r = CrewRecOf(self, p->ch); if (!r) return;
  float hp = *(float*)((char*)self + g_oCmHp); int st = *(int*)((char*)p->ch + g_oChHealth);
  float lost = p->hp - hp; if (lost < 0) lost = 0;
  if (p->hp > r->maxHp) r->maxHp = p->hp;
  bool direct = g_raCrewDirect && ra == g_raCrewDirect, splash = g_raCrewSplash && ra == g_raCrewSplash;
  int ent = splash ? *(int*)(crsp + 0xC0) : 0;
  bool fire = splash && ent == -999 && !*(void**)(crsp + 0xB8) && *(int*)(crsp + 0xC8) == 0;
  CrewHitWhere w;
  bool downed = hp <= 0 && p->hp > 0;
  if (p->hp > 0) CrewHitWhereOf(self, direct, splash, crsp, &w);
  std::string what;                                                    // "struck in the left arm by a round (7.7mm) from ..."
  if (direct) what = "struck" + (w.part.empty() ? std::string() : " in " + w.part) + " by " + (w.how.empty() ? "a round" : w.how) + CrewFrom(w);
  else if (fire) what = "burned by fire" + (w.part.empty() ? std::string() : " on " + w.part);
  else if (splash && w.how.find("shell burst") != std::string::npos) {   // "hit in the left leg by shrapnel from a shell that burst close by (the ... of 'X')"
    std::string from = CrewFrom(w); if (from.rfind(" from ", 0) == 0) from = " (" + from.substr(6) + ")";
    what = "hit" + (w.part.empty() ? std::string() : " in " + w.part) + " by shrapnel from a shell" + (w.how.find("close") != std::string::npos ? " that burst close by" : "") + from;
  }
  else if (splash) what = "caught by " + (w.how.empty() ? std::string(ent == g_gfxEntityCode ? "an explosion aboard" : "a blast") : w.how) + CrewFrom(w) + (w.part.empty() ? std::string() : ", hit in " + w.part);
  else what = "hurt";
  if (lost > 0) {
    std::string key = w.enemy == "our own boat" ? std::string("our own guns") : !w.enemy.empty() ? "'" + w.enemy + "'" : fire ? "fire" : w.how == "an explosion aboard" ? "explosions aboard" : "unknown";
    CrewRec::Dmg& g = r->dmgBy[key];
    if (g_crewLogStart < 0) g_crewLogStart = CrewLogNow();
    float at = CrewLogNow() - g_crewLogStart;
    g.hp += lost; g.n++; if (g.first < 0) g.first = at; g.last = at;
    if (!w.source.empty() && g.guns.size() < 4) g.guns.insert(w.source);
    std::string src = key + "|" + w.source + "|" + (direct ? "round" : w.how);
    if (g_debug && g_crewSrcSeen.size() < 16 && g_crewSrcSeen.insert(src).second) {   // debug=1: each new source once, to see what hurts the crew
      if (direct) Log("crew log: new hit source: %s from gun '%s' of %s, -%.1f HP to %s", w.how.c_str(), w.source.c_str(), key.c_str(), lost, r->name.c_str());
      else if (splash) {
        V3 dv = *(V3*)(crsp + 0x2B0); SplashData* sd = (SplashData*)(crsp + 0x90);
        Log("crew log: new hit source: %s, selected gun '%s' of %s (splash range %.1f m, damage %.1f, %.1f m away, projectileCode %d, boostRange %d, force %d, entity %d, team %d), -%.1f HP to %s",
            w.how.empty() ? "a blast" : w.how.c_str(), w.source.c_str(), key.c_str(), sd->range, sd->damage, sqrtf(V3Dot(dv, dv)), sd->projectileCode, (int)sd->boostRange, (int)sd->force, sd->entityCode, sd->teamCode, lost, r->name.c_str());
      }
    }
  }
  if (p->hp <= 0 && !downed) r->hitsDown++;                            // blasts still reach a man who is down (no HP to lose): counted, no line
  else {
    r->hits++; r->dmg += lost;
    if (direct) r->shells++; else if (fire) r->burns++; else if (splash) r->blasts++; else r->other++;
    float pct = r->maxHp > 0 ? lost / r->maxHp : 0;
    int code = splash ? *(int*)(crsp + 0xC8) : 0;
    bool burst = splash && !fire && code != 0 && code == r->pjCode && CrewLogNow() - r->pjAt < 1.0f;   // no line needed: the event pairs anyway
    auto tier = [](float f) { return f < 0.20f ? "a wound" : f < 0.45f ? "a serious wound" : "a grievous wound"; };
    std::string role = r->role.empty() ? std::string() : ", " + r->role;
    int kind = direct ? R_ROUND : fire ? R_FIRE : !splash ? R_OTHER : w.how.find("shell burst") != std::string::npos ? (w.close ? R_CLOSE : R_FAR)
             : w.how == "a torpedo blast" ? R_TORPEDO : w.how == "the blast of a dying ship" ? R_WRECK : w.how == "an explosion aboard" || (w.how.empty() && ent == g_gfxEntityCode) ? R_ABOARD : R_OTHER;
    std::string evEnemy = w.enemy == "our own boat" ? std::string("our own guns") : w.enemy;
    auto evHit = [&](int k, float l, float f) -> RepEv& {
      RepEv& e = CrewEv(*r, k); e.tier = f < 0.20f ? 1 : f < 0.45f ? 2 : 3; if (downed) e.tier = 3;
      e.part = w.part; e.enemy = evEnemy; e.enemyBase = w.enemyBase == "our own boat" ? evEnemy : w.enemyBase; e.gun = w.source; e.shell = w.shell;
      e.close = w.close; e.lost = l; e.hpLeft = hp; return e;
    };
    if (burst) {                                                       // the round that hit him burst: one line for both
      float both = r->pjLost + lost, bp = r->maxHp > 0 ? both / r->maxHp : 0;
      char b[400];
      if (downed) snprintf(b, sizeof b, "%s, then by its shrapnel, and went down (-%.0f HP, %.0f of %.0f left)%s", r->pjWhat.c_str(), both, hp, r->maxHp, role.c_str());
      else snprintf(b, sizeof b, "%s, then by its shrapnel: %s (-%.0f HP, %.0f of %.0f left)%s%s", r->pjWhat.c_str(), tier(bp), both, hp, r->maxHp, role.c_str(), r->pjWound ? "; now wounded" : "");
      if (r->pjLine < r->log.size()) {                                 // past the 60-line cap the round had no line to rewrite
        std::string& line = r->log[r->pjLine]; line = line.substr(0, line.find("  ") + 2) + b;
        DLog("crew log: %s: %s", r->name.c_str(), line.c_str());
      }
      r->pjCode = 0; r->pjWound = false;
      if (r->pjEv < r->ev.size()) {                                    // the round's event becomes the pair
        RepEv& e = r->ev[r->pjEv]; e.kind = R_PAIR; e.part2 = w.part; e.close = w.close; e.lost = both; e.hpLeft = hp;
        e.tier = downed ? 3 : bp < 0.20f ? 1 : bp < 0.45f ? 2 : 3;
      }
      if (downed) CrewEv(*r, S_DOWN);
      if (!downed && (!(d > 3.0f) || pct < 0.05f)) r->grazes++;        // the burst itself still counts as a graze when small
    }
    else if (!downed && (!(d > 3.0f) || pct < 0.05f)) {               // grazes (incl. the game's silent hits of 3 or less): counted, no line each
      r->grazes++;
      if (r->grazeEv.size() < 2000) r->grazeEv.push_back({ CrewEvT(), fire ? std::string("fire") : evEnemy, lost });
      g_talkAction = CrewLogNow(); if (fire) TalkOnEvent(*r, R_FIRE);
      else TalkGraze(*r);
    }
    else if (downed) { CrewNote(*r, "%s and went down (-%.0f HP, %.0f of %.0f left)%s", what.c_str(), lost, hp, r->maxHp, role.c_str()); evHit(kind, lost, pct); CrewEv(*r, S_DOWN); }
    else {
      size_t evAt = r->ev.size(); evHit(kind, lost, pct);
      size_t before = r->log.size();
      CrewNote(*r, "%s: %s (-%.0f HP, %.0f of %.0f left)%s", what.c_str(), tier(pct), lost, hp, r->maxHp, role.c_str());
      if (direct && w.pjCode) {
        bool hasLine = r->log.size() > before;
        r->pjCode = w.pjCode; r->pjLine = hasLine ? before : (size_t)-1; r->pjLost = lost; r->pjAt = CrewLogNow(); r->pjWhat = what; r->pjEv = evAt < r->ev.size() ? evAt : (size_t)-1;
        r->pjWound = st == 1 && p->state != 1;                         // said on this line (with the burst's HP if it bursts), not as a note after it
        if (r->pjWound && hasLine) r->log[before] += "; now wounded";
      }
    }
  }
  if (downed) { r->down = true; r->downs++; r->wounded = true; }
  if (st == 1 && p->state != 1 && !downed) CrewEv(*r, S_WOUNDED);   // the talking crew: TalkOnEvent
  if (st == 1 && p->state != 1 && !downed && !(direct && r->pjWound && r->pjCode == w.pjCode && w.pjCode)) CrewNote(*r, "is wounded (%.0f of %.0f HP left)", hp, r->maxHp);
  if (st == 1) r->wounded = true;
  if (st == 3 && p->state != 3) { r->overboards++; CrewNote(*r, "thrown overboard by the %s%s", direct ? "hit" : "blast", p->hp <= 0 ? " while down" : ""); CrewEv(*r, direct ? S_OVER_HIT : S_OVER_BLAST).whileDown = p->hp <= 0; }
  r->state = st; r->lastHp = hp;
}
static int CrewLogFilter(EXCEPTION_POINTERS* ep) { g_crewLogFaults = -1; g_crewLogOn = false; return FaultFilter("crew log", ep); }
static void SafeCrewLogPre(void* self, CrewHitPre* p) {
  if (!g_crewLogOn || g_crewLogFaults < 0 || g_gfxFaults < 0 || !g_gfxInBattle) return;   // gun effects dead: the poll and craft are stale too
  __try { CrewLogPre(self, p); } __except (CrewLogFilter(GetExceptionInformation())) { p->on = false; }
}
static void SafeCrewLogPost(void* self, float d, uintptr_t ra, char* crsp, const CrewHitPre* p) {
  if (!g_crewLogOn || g_crewLogFaults < 0) return;
  __try { CrewLogPost(self, d, ra, crsp, p); } __except (CrewLogFilter(GetExceptionInformation())) {}
}
// One kill and everyone the game credited with it: the vanilla game credits every contributor (gunners, the helm), so
// a kill credited to several of ours within 2 s is one shared kill, and the report lines name the others.
struct CrewKillShare { void* killed; float at; std::string who; std::vector<size_t> recs, lines; };
static std::vector<CrewKillShare> g_crewKills;
static std::string CrewJoin(const std::vector<std::string>& v) {
  std::string o;
  for (size_t i = 0; i < v.size(); i++) o += (i == 0 ? "" : i + 1 == v.size() ? " and " : ", ") + v[i];
  return o;
}
static void CrewLogKill(void* member, void* killed) {
  if (!CrewIsOurs(member)) return;
  CrewRec* r = CrewRecOf(member, *(void**)((char*)member + g_oCmChar)); if (!r) return;
  size_t me = r - g_crewRecs.data();
  std::string who = !killed ? std::string("an enemy") : IsShip(killed) ? ShipTitle(killed) : CrewClean(NameOf(killed));
  float now = CrewLogNow();
  CrewKillShare* k = nullptr;
  for (auto& e : g_crewKills) if (killed && e.killed == killed && now - e.at < 2.0f) { k = &e; break; }
  if (k && std::find(k->recs.begin(), k->recs.end(), me) != k->recs.end()) return;   // credited twice for one kill: once is enough
  if (!k && g_crewKills.size() < 256) { g_crewKills.push_back({killed, now, who, {}, {}}); k = &g_crewKills.back(); }
  size_t before = r->log.size();
  r->kills++;
  { RepEv& e = CrewEv(*r, K_KILL); e.enemy = killed ? CrewEnemyOf(killed) : std::string(); e.enemyBase = killed ? CrewEnemyBase(killed) : std::string();
    if (e.enemy.empty()) e.enemy = who; if (e.enemyBase.empty()) e.enemyBase = e.enemy;
    e.gun = r->gun; e.killId = k ? (int)(k - g_crewKills.data()) : -1; }
  if (!r->gun.empty() && killed && !g_sayKills.count(killed)) {       // the gun that got it, once per kill (the helm shares every kill)
    EntityKind kd = KindOf(killed); bool plane = !kd.ship && !kd.surface;
    if (TalkSay(*r, plane ? "kill_plane" : "kill", TalkCtx(), 0.8f))
      g_sayKills.insert(killed);                                       // a gunner held back by the rate limit leaves it to the next one credited
  }
  CrewNote(*r, "killed '%s'%s", who.c_str(), r->gun.empty() ? "" : (" with the " + r->gun).c_str());
  if (!k) return;
  k->recs.push_back(me); k->lines.push_back(r->log.size() > before ? before : (size_t)-1);   // -1: past the 60-line cap
  if (k->recs.size() < 2) return;
  for (size_t i = 0; i < k->recs.size(); i++) {                       // rewrite every participant's line in the report
    CrewRec& p = g_crewRecs[k->recs[i]];
    if (k->recs.size() == 2 || i + 1 == k->recs.size()) p.shared++;    // the first two when it becomes shared, then each newcomer
    std::vector<std::string> others;
    for (size_t j = 0; j < k->recs.size(); j++) if (j != i) others.push_back(g_crewRecs[k->recs[j]].name);
    if (k->lines[i] == (size_t)-1 || k->lines[i] >= p.log.size()) continue;
    std::string& line = p.log[k->lines[i]];
    line = line.substr(0, line.find("  ") + 2) + "killed '" + k->who + "'" + (p.gun.empty() ? "" : " with the " + p.gun) + ", shared with " + CrewJoin(others);
  }
}
static void SafeCrewLogKill(void* member, void* killed) {
  if (!g_crewLogOn || g_crewLogFaults < 0 || g_gfxFaults < 0 || !g_gfxInBattle) return;
  __try { CrewLogKill(member, killed); } __except (CrewLogFilter(GetExceptionInformation())) {}
}
// Four times a second in battle: who is aboard and what they do; state changes the hits do not show (bleeding out,
// the water counter throwing someone over, rescue, revival, death).
static void SayProbeTick() {
  static int offCur = -2; static void *mActive = nullptr, *mTr = nullptr, *mPos = nullptr, *mCam = nullptr;
  if (offCur == -2 && !g_sayProbes.empty() && Alive(g_sayProbes[0].head)) {
    offCur = FieldOffset(il2cpp_object_get_class(g_sayProbes[0].head), "currentText");
    void* go = FindClass("UnityEngine", "GameObject"); mActive = go ? FindMethod(go, "get_activeInHierarchy", 0) : nullptr;
    void* c = FindClass("UnityEngine", "Component"); mTr = c ? FindMethod(c, "get_transform", 0) : nullptr;
    void* t = FindClass("UnityEngine", "Transform"); mPos = t ? FindMethod(t, "get_position", 0) : nullptr;
    void* cam = FindClass("UnityEngine", "Camera"); mCam = cam ? FindMethod(cam, "get_main", 0) : nullptr;
  }
  float now = CrewLogNow();
  for (size_t i = 0; i < g_sayProbes.size();) {
    SayProbe& p = g_sayProbes[i];
    if (now - p.at < 1.0f) { i++; continue; }
    if (g_sayProbed++ < 12) {
      if (!Alive(p.head)) Log("crew bubbles, 1 s later: %s's head text is gone", p.name.c_str());
      else {
        void* cur = offCur > 0 ? *(void**)((char*)p.head + offCur) : nullptr;
        void* go = m_getGameObject ? Invoke(m_getGameObject, p.head) : nullptr; void* act = go && mActive ? Invoke(mActive, go) : nullptr;
        float dist = -1;
        void* cam = mCam ? Invoke(mCam, nullptr) : nullptr;
        void* tr = mTr ? Invoke(mTr, p.head) : nullptr; void* ctr = Alive(cam) && mTr ? Invoke(mTr, cam) : nullptr;
        void* a = tr && mPos ? Invoke(mPos, tr) : nullptr; void* b = ctr && mPos ? Invoke(mPos, ctr) : nullptr;
        if (a && b) { V3 x = *(V3*)((char*)a + 0x10), y = *(V3*)((char*)b + 0x10); dist = sqrtf((x.x - y.x) * (x.x - y.x) + (x.y - y.y) * (x.y - y.y) + (x.z - y.z) * (x.z - y.z)); }
        Log("crew bubbles, 1 s later: %s's bubble shows \"%s\" (queued \"%s\"), switched %s, %.0f m from the camera",
            p.name.c_str(), cur ? FemUtf8(cur).c_str() : "(nothing)", p.text.c_str(), !act ? "?" : *(bool*)((char*)act + 0x10) ? "on" : "OFF", dist);
      }
    }
    Unpin(p.pin); g_sayProbes.erase(g_sayProbes.begin() + i);
  }
}
static void CrewLogPoll() {
  DWORD now = GetTickCount(); if (now - g_crewLogPollAt < 250) return; g_crewLogPollAt = now;
  if (g_crewLogStart < 0) g_crewLogStart = CrewLogNow();
  if (g_debug) SayProbeTick();
  TalkTick();
  void* list = GfxCrewList(); int n = list ? ListCount(list) : 0;
  for (int i = 0; i < n && i < 64; i++) { void* m = ListAt(list, i); void* ch = m ? *(void**)((char*)m + g_oCmChar) : nullptr; if (ch) CrewRecOf(m, ch); }
  for (CrewRec& r : g_crewRecs) {
    float hp = *(float*)((char*)r.ch + g_oChHP); int st = *(int*)((char*)r.ch + g_oChHealth);
    if (st != r.state) {
      // 4 and 5 (and a sinking boat's crew turning 3) come from the game's end-of-battle scan (scanDead), which runs
      // before the battle change is seen; Wounded -> Healthy there too (healAllWoundedCharacters), so 'wounded' sticks
      bool sinking = g_oChSinking >= 0 && *(bool*)((char*)r.ch + g_oChSinking);
      if (st == 2) { CrewNote(r, r.down ? "died of their wounds" : "was killed"); CrewEv(r, r.down ? S_DIED_WOUNDS : S_KILLED); }
      else if (st == 3) { r.overboards++; CrewNote(r, sinking ? "went down with the boat" : "went over the side"); CrewEv(r, sinking ? S_SANK : S_SIDE); }
      else if (st == 5) { CrewNote(r, "picked up after the battle"); CrewEv(r, S_PICKED); }
      else if (st == 4) { CrewNote(r, "taken prisoner after the battle"); CrewEv(r, S_PRISONER); }
      else if (r.state == 3 && (st == 0 || st == 1)) { CrewNote(r, "pulled back aboard"); CrewEv(r, S_PULLED); }
      if (st == 1) r.wounded = true;
      r.state = st;
    }
    if (st != 2 && st != 3) {
      if (!r.down && hp <= 0 && r.lastHp > 0) { r.down = true; r.downs++; CrewNote(r, "collapsed from blood loss"); CrewEv(r, S_BLEED); }
      else if (r.down && hp > 0) { r.down = false; CrewNote(r, "back on their feet (%.0f HP)", hp); CrewEv(r, S_UP); }
    }
    r.lastHp = hp;
  }
}
static void SafeCrewLogPoll() {
  if (!g_crewLogOn || g_crewLogFaults < 0) return;
  __try { CrewLogPoll(); } __except (CrewLogFilter(GetExceptionInformation())) {}
}
static const char* CrewOutcome(const CrewRec& r) {
  switch (r.state) {
    case 2: return "killed in action"; case 3: return "in the water when the battle ended (missing)"; case 4: return "captured";
    case 5: return "lost at sea, then picked up"; case 1: return r.down ? "down when the battle ended" : "wounded";
    default: return r.down ? "down when the battle ended" : r.wounded ? "wounded" : r.hits ? "lightly hurt" : "unhurt";
  }
}
