// gun effects per shot, kill, tick and battle change (GfxShot to GfxBattleChange) (a fragment of weaponmod.cpp, included in order, not standalone)
// Each own new shot (CritScan, after the co-op copies are left out): Opening Salvo's count, Frugal's next roll.
static void GfxShot(void* pj, void* mount, bool leap) {
  g_salvoShots.erase(pj);                                              // a pooled shell's earlier life is over
  if (leap || !g_gfxReady || !g_gfxCraft || *(void**)((char*)mount + g_oGmEntity) != g_gfxCraft) return;
  const GunFx& gx = GfxOf(mount);
  if (!gx.item) return;
  bool frag = false;                                                   // a fragment of one of this gun's shells
  if ((gx.salvo > 0 || gx.saveAmmo > 0) && g_oPjType >= 0 && g_oPtFrag >= 0) {
    void* pt = *(void**)((char*)pj + g_oPjType);
    std::set<void*>& ft = g_gfxFragTypes[mount];
    if (pt && ft.count(pt)) frag = true;
    else if (pt) { void* fr = *(void**)((char*)pt + g_oPtFrag); if (fr && fr != pt) ft.insert(fr); }
  }
  if (gx.salvo > 0) {
    auto s = g_salvo.find(mount);
    if (s == g_salvo.end()) {
      SalvoState n; n.mult = gx.salvo; n.name = gx.name;
      void* gt = g_oGmGunType >= 0 ? *(void**)((char*)mount + g_oGmGunType) : nullptr;
      int mag = gt && g_oGtMag >= 0 ? *(int*)((char*)gt + g_oGtMag) : 4; n.mag = (std::max)(4, mag);
      s = g_salvo.emplace(mount, n).first;
      GFXLOG(GL_SALVO, "opening salvo: '%s' opens fire: x%.2f damage for its first magazine (%d rounds)", gx.name.c_str(), gx.salvo, mag);
    }
    if (s->second.active) { g_salvoShots[pj] = mount; if (!frag) { s->second.shots++; GfxStat(gx.name).salvoShots++; } }
  }
  if (gx.saveAmmo > 0 && !frag && g_oGmBarrels >= 0 && g_oBarSkip >= 0) {
    auto f = g_frugal.find(mount);
    if (f == g_frugal.end()) {
      FrugalState n; void* arr = *(void**)((char*)mount + g_oGmBarrels);
      int nb = ArrLen(arr);
      for (int i = 0; i < nb && i < 16; i++) { void* b = ArrAt(arr, i); if (b && !*(bool*)((char*)b + g_oBarSkip)) n.barrels.push_back(b); }
      void* hm = Pin(mount); n.pinned = hm != nullptr; n.handles.push_back(hm);
      for (void* b : n.barrels) { void* hb = Pin(b); if (!hb) n.pinned = false; n.handles.push_back(hb); }
      f = g_frugal.emplace(mount, n).first;
      DLog("frugal: '%s' has %d barrel(s) that use ammo%s", gx.name.c_str(), (int)n.barrels.size(), n.pinned ? "" : " (not pinned: its flags are not put back at the battle's end)");
    }
    FrugalState& fs = f->second;
    unsigned frame = g_shotFrame;
    if (fs.frame != frame) {                                           // this frame's shots were all fired under the last roll
      fs.frameArmed = fs.armed; fs.frame = frame;
      fs.armed = Rand01() < gx.saveAmmo;
      for (void* b : fs.barrels) *(bool*)((char*)b + g_oBarSkip) = fs.armed;
    }
    GfxStats& st = GfxStat(gx.name); st.shots++;
    if (fs.frameArmed) { st.freeShots++; GFXLOG(GL_FRUGAL, "frugal: '%s' fired a shot that used no ammo", gx.name.c_str()); }
  }
}
// Frugal: every tracked barrel's skip flag back to off (the stock state; only barrels where it was off are tracked), and
// the pins freed. At the battle change, and from every fault handler that stops GfxShot, so a gun is never left firing
// for free. Without pins nothing is written: the pointers may be stale.
static void FrugalRestore() {
  for (auto& e : g_frugal) {
    FrugalState& fs = e.second;
    if (fs.pinned && g_oBarSkip >= 0 && *(void**)((char*)e.first + 0x10))   // UnityEngine.Object.m_CachedPtr: not destroyed
      for (void* b : fs.barrels) *(bool*)((char*)b + g_oBarSkip) = false;
    for (void* h : fs.handles) Unpin(h);
  }
  g_frugal.clear();
}
static void SafeFrugalRestore() {
  if (g_frugal.empty()) return;
  __try { FrugalRestore(); }
  __except (FaultFilter("gun effects (frugal restore)", GetExceptionInformation())) { g_frugal.clear(); }
}
static void SafeGfxShot(void* pj, void* mount, bool leap) {
  if (g_gfxFaults < 0) return;
  __try { GfxShot(pj, mount, leap); }
  __except (FaultFilter("gun effects (shot)", GetExceptionInformation())) { g_gfxFaults = -1; g_gfxHitOn = false; g_gfxCrewOn = false; SafeFrugalRestore(); }
}

// Veteran's: wipes the bond of a gunner who is Dead (2), Missing (3) or Captured (4) on two checks in a row (or at the
// battle's end), on every found gun bonded to that name.
// A gunner thrown overboard leaves CrewManager.crew at once (throwOverboard -> removeMember), so his later death or
// capture never shows in the list: he is kept here, pinned, and read on every check until back aboard or lost, across
// the battle change too, as the game may settle a man in the water after it.
struct VetGone { void* ch; void* handle; };
static std::map<std::string, VetGone> g_vetGone;                      // bond key -> the gunner off the boat
static std::map<std::string, void*> g_vetSeenCh;                      // bond key -> his character at the last check (this battle)
static void VetForget(const std::string& n) { auto g = g_vetGone.find(n); if (g != g_vetGone.end()) { Unpin(g->second.handle); g_vetGone.erase(g); } }
static void VetCheck(bool final) {
  std::set<std::string> bonded; for (const Item& it : g_items) if (!it.vetCrew.empty()) bonded.insert(it.vetCrew);
  if (bonded.empty()) { for (auto& g : g_vetGone) Unpin(g.second.handle); g_vetGone.clear(); return; }
  if (g_oChHealth < 0 || g_oCmChar < 0) return;
  void* list = GfxCrewList(); std::set<std::string> dead, present; int n = ListCount(list);
  std::vector<std::pair<std::string, std::string>> customs;            // (nameCustom, key) of renamed crew
  for (int i = 0; i < n; i++) {
    void* cm = ListAt(list, i); void* ch = cm ? *(void**)((char*)cm + g_oCmChar) : nullptr; if (!ch) continue;
    std::string key = GfxCrewKey(ch), shown = GfxCrewName(ch); if (key.empty()) continue;
    present.insert(key);
    if (shown != key) customs.push_back({ shown, key });
    if (bonded.count(key)) g_vetSeenCh[key] = ch;
    int hs = *(int*)((char*)ch + g_oChHealth);
    if (hs == 2 || hs == 3 || hs == 4) dead.insert(key);
  }
  // bonds made by an older build under a renamed gunner's display name move to his stable name
  for (const auto& c : customs) {
    if (!bonded.count(c.first) || present.count(c.first)) continue;
    for (Item& it : g_items) if (it.vetCrew == c.first) {
      Log("veteran's: '%s' bond moved from the display name '%s' to the crew member's name '%s'", it.name.c_str(), c.first.c_str(), c.second.c_str());
      it.vetCrew = c.second; g_vetDirty = true;
    }
    bonded.erase(c.first); bonded.insert(c.second);
  }
  if (n > 0) {                                                         // not while the boat is still loading its crew
    for (const std::string& b : bonded) {
      if (present.count(b) || g_vetGone.count(b)) continue;
      auto s = g_vetSeenCh.find(b); if (s == g_vetSeenCh.end()) continue;   // not seen this battle: left ashore, say
      void* h = Pin(s->second); if (!h) continue;
      g_vetGone[b] = { s->second, h };
      Log("veteran's: gunner '%s' is off the boat (overboard?); his gun's bond waits on his fate", b.c_str());
    }
    for (auto g = g_vetGone.begin(); g != g_vetGone.end(); ) {
      if (present.count(g->first) || !bonded.count(g->first)) {         // back aboard (picked up), or no bond left
        if (present.count(g->first)) Log("veteran's: gunner '%s' is back aboard", g->first.c_str());
        Unpin(g->second.handle); g = g_vetGone.erase(g); continue;
      }
      int hs = *(int*)((char*)g->second.ch + g_oChHealth);
      if (hs == 2 || hs == 3 || hs == 4) dead.insert(g->first);
      ++g;
    }
  }
  for (const std::string& n : dead) {
    if (!final && !g_vetDeadSeen.count(n)) continue;
    for (Item& it : g_items) if (it.vetCrew == n && !n.empty()) {
      GFXLOG(GL_VET, "veteran's: '%s' is lost: its %d kills are gone with gunner '%s'", it.name.c_str(), it.vetKills, n.c_str());
      ShowMessage(Colored("Veteran's", "#ffb300") + ": " + it.name + " lost its veteran gunner");
      it.vetCrew.clear(); it.vetKills = 0; g_vetDirty = true;
    }
    VetForget(n);
  }
  g_vetDeadSeen = dead;
}
// One credit per death: the WeaponSystem and CrewMember creditKill calls of one death arrive together; a pooled ship or
// plane that dies again later in the battle is a new kill.
static bool GfxKillNew(void* killed, void* by, const char* what) {
  DWORD now = GetTickCount();
  auto f = g_gfxKillSeen.find({ killed, by });
  if (f != g_gfxKillSeen.end() && now - f->second <= 1000) { DLog("%s: the same death credited again (%p); counted once", what, killed); return false; }
  g_gfxKillSeen[{ killed, by }] = now;
  return true;
}
static void GfxKill(void* mount, void* killed, void* gunner) {
  if (!mount || !killed || !g_gfxReady || !g_gfxCraft || IsRemoteMount(mount) || IsHiddenMount(mount) || *(void**)((char*)mount + g_oGmEntity) != g_gfxCraft) return;
  const GunFx& gx = GfxOf(mount);
  if (!gx.item || (gx.rally <= 0 && gx.vetPer <= 0)) return;
  std::string killedName = KindOf(killed).ship ? ShipTitle(killed) : CrewClean(StripTags(NameOf(killed)));
  if (gx.vetPer > 0 && gunner && g_oCmChar >= 0) {                     // only crew kills: a gun with no gunner has no veteran
    Item* it = GfxMutItem(gx.name); void* ch = *(void**)((char*)gunner + g_oCmChar);
    std::string who = ch ? GfxCrewKey(ch) : std::string(), shown = ch ? GfxCrewName(ch) : std::string();   // the bond's key, the name on screen
    if (it && !who.empty() && (it->vetCrew.empty() || it->vetCrew == who) && GfxKillNew(killed, gunner, "veteran's")) {
      bool first = it->vetCrew.empty();
      it->vetCrew = who; it->vetKills++; g_vetDirty = true; GfxStat(gx.name).vetKills++;
      float k = gx.vetPer * (float)(std::min)(it->vetKills, gx.vetCap);
      GFXLOG(GL_VET, "veteran's: '%s' gunner '%s' killed '%s': kill %d%s, +%d%% damage while that gunner mans it", gx.name.c_str(), shown.c_str(), killedName.c_str(), it->vetKills,
             first ? " (the gun is now theirs)" : "", (int)std::lround(k * 100));
      if (first || it->vetKills == gx.vetCap) ShowMessage(Colored("Veteran's", "#ffb300") + ": " + shown + (first ? " takes " : " masters ") + it->name);
    } else if (it && !who.empty() && it->vetCrew != who && !it->vetCrew.empty()) DLog("veteran's: '%s' kill by '%s', not its gunner '%s'", gx.name.c_str(), shown.c_str(), it->vetCrew.c_str());
  }
  if (gx.rally > 0 && GfxKillNew(killed, mount, "rallying") && g_oCmDowned >= 0 && g_oCmHp >= 0 && g_oCmChar >= 0 && m_allowedDowned) {
    void* list = GfxCrewList(); int helped = 0; std::string names;
    for (int i = 0; i < ListCount(list); i++) {
      void* cm = ListAt(list, i); if (!cm) continue;
      float* dt = (float*)((char*)cm + g_oCmDowned);
      if (*(float*)((char*)cm + g_oCmHp) > 0 || *dt <= 0) continue;     // not downed
      void* ch = *(void**)((char*)cm + g_oCmChar); if (!ch) continue;
      if (g_oChHealth >= 0 && *(int*)((char*)ch + g_oChHealth) == 2) continue;
      void* ab = Invoke(m_allowedDowned, ch); float allowed = ab ? *(float*)((char*)ab + 0x10) : 0;
      if (allowed <= 0 || *dt >= allowed) continue;                     // already bleeding out (or an instant death)
      float before = *dt; *dt = (std::max)(0.1f, before - gx.rally);
      helped++; GfxStat(gx.name).rallySecs += before - *dt;
      char b[160]; snprintf(b, sizeof b, "%s'%s' %.0f s left", names.empty() ? "" : ", ", GfxCrewName(ch).c_str(), allowed - *dt); names += b;
    }
    GfxStats& st = GfxStat(gx.name); st.rallyKills++; st.rallyCrew += helped;
    if (helped) { GFXLOG(GL_RALLY, "rallying: '%s' killed '%s': %d downed crew get %.0f s more (%s)", gx.name.c_str(), killedName.c_str(), helped, gx.rally, names.c_str()); ShowMessage(Colored("Rallying", "#4fc3f7") + ": your downed crew hold on longer"); }
    else DLog("rallying: '%s' killed '%s'; nobody is down", gx.name.c_str(), killedName.c_str());
  }
}
static void SafeGfxKill(void* mount, void* killed, void* gunner) {
  if (g_gfxFaults < 0) return;
  __try { GfxKill(mount, killed, gunner); }
  __except (FaultFilter("gun effects (kill)", GetExceptionInformation())) { g_gfxFaults = -1; g_gfxHitOn = false; g_gfxCrewOn = false; SafeFrugalRestore(); }
}

// Each frame, twice: before CritScan (install, the player's boat) and after it (salvo ends, spills, Brittle guns, marks).
static void GfxTick(bool before) {
  if (before) {
    if (!g_gfxTried) GfxInit();
    if (!g_gfxReady) return;
    g_gfxCraft = PlayerCraft();                                        // also between battles: the battle-end check reads its crew
    g_gfxInBattle = EngagementInstance() != nullptr;
    if (g_gfxCraft) { g_gfxTeam = *(int*)((char*)g_gfxCraft + g_oLeTeam); g_gfxEntityCode = *(int*)((char*)g_gfxCraft + g_oLeCode); }
    return;
  }
  if (!g_gfxReady || !g_gfxInBattle) return;
  SafeCrewLogPoll();                                                   // also after our boat sank: the crew records are pinned
  if (!g_gfxCraft) return;
  DWORD now = GetTickCount();
  for (auto& e : g_salvo) {                                            // a salvo ends when the gun first reloads after 6+ shots
    SalvoState& s = e.second; if (!s.active) continue;
    bool reloading = g_oGmReloading >= 0 && *(bool*)((char*)e.first + g_oGmReloading);
    // a floor of 6 shots: magazine-1 guns (mortars, the howitzer, the 37 mm AT) reload after every round
    if ((reloading && s.shots >= (std::max)(s.mag, 6)) || s.shots >= 3 * s.mag) {
      s.active = false; s.endedAt = now;
      GFXLOG(GL_SALVO, "opening salvo: '%s' fired %d shots at x%.2f; the salvo is over (%s)", s.name.c_str(), s.shots, s.mult, reloading ? "reloading" : "no reload seen");
    }
  }
  for (auto it = g_salvoShots.begin(); it != g_salvoShots.end(); ) {   // salvo shells still in flight keep its blast window open
    auto sh = g_shots.find(it->first);
    bool live = sh != g_shots.end() && sh->second.frame == g_shotFrame && sh->second.from == it->second;
    auto s = g_salvo.find(it->second);
    if (live && s != g_salvo.end()) s->second.liveAt = now;
    if (!live && (s == g_salvo.end() || !s->second.active)) it = g_salvoShots.erase(it);   // landed (or gone) after the salvo
    else ++it;
  }
  if (!g_spills.empty()) {                                             // Overkill: sent here, outside the game's damage loops
    std::vector<Spill> todo; todo.swap(g_spills);
    for (const Spill& s : todo) {
      void* tr = Invoke(m_getTransform, s.ce); void* pb = tr && m_trGetPos ? Invoke(m_trGetPos, tr) : nullptr; if (!pb) continue;
      V3 from = *(V3*)((char*)pb + 0x10), to{};
      void* tgt = NearestEnemyTarget(&from.x, 300.0f, &to, s.ce);
      void* ws = g_oGmWs >= 0 ? *(void**)((char*)s.mount + g_oGmWs) : nullptr;
      if (!tgt || !ws) { GFXLOG(GL_SPILL, "overkill: '%s' sank '%s' with %.0f to spare, but no other enemy is within 300 m", s.gun.c_str(), s.ship.c_str(), s.damage); continue; }
      float radius = (std::max)(15.0f, EntitySize(tgt) * 0.5f);
      if (!QueueSplash(to, radius, s.damage, ws, g_gfxEntityCode, g_gfxTeam, kSpillCode)) continue;
      GfxStats& st = GfxStat(s.gun); st.spills++; st.spillDmg += s.damage;
      std::string tn = KindOf(tgt).ship ? ShipTitle(tgt) : CrewClean(StripTags(NameOf(tgt)));
      GFXLOG(GL_SPILL, "overkill: '%s' sank '%s'; %.0f damage spills onto '%s' (%.0f m away, %.0f m blast)", s.gun.c_str(), s.ship.c_str(), s.damage, tn.c_str(),
             sqrtf((to.x - from.x) * (to.x - from.x) + (to.z - from.z) * (to.z - from.z)), radius);
    }
  }
  if (now - g_brittleAt > 2000 && g_oWsHull >= 0 && g_offCraftWM >= 0 && g_offWMSystems >= 0) {   // Brittle: the selected guns, every 2 s
    g_brittleAt = now;
    void* wm = *(void**)((char*)g_gfxCraft + g_offCraftWM); void* arr = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
    std::unordered_map<void*, BrittleGun> nb;
    for (int i = 0; arr && i < ArrLen(arr) && i < 64; i++) {
      void* ws = ArrAt(arr, i); if (!ws) continue;
      void* sel = *(void**)((char*)ws + g_offWsSelected); void* hull = *(void**)((char*)ws + g_oWsHull); if (!sel || !hull) continue;
      const GunFx& gx = GfxOf(sel); if (gx.brittle <= 1.0f) continue;
      if (!g_brittle.count(hull)) GFXLOG(GL_BRITTLE, "brittle: '%s' takes x%.2f damage this battle", gx.name.c_str(), gx.brittle);
      nb[hull] = { gx.brittle, gx.name };
    }
    g_brittle.swap(nb);
  }
  for (auto it = g_marks.begin(); it != g_marks.end(); ) {             // marks run out, or their ship sinks
    bool dead = g_oLeAlive >= 0 && !*(bool*)((char*)it->first + g_oLeAlive);
    if (dead || (int)(it->second.until - now) <= -2000) { if (dead) DLog("marking: '%s' is gone", it->second.ship.c_str()); it = g_marks.erase(it); } else ++it;
  }
  if (now - g_vetCheckAt > 1000) { g_vetCheckAt = now; VetCheck(false); }
}
static void SafeGfxTick(bool before) {
  if (g_gfxFaults < 0) return;
  __try { GfxTick(before); }
  __except (FaultFilter("gun effects", GetExceptionInformation())) { g_gfxFaults = -1; g_gfxHitOn = false; g_gfxCrewOn = false; SafeFrugalRestore(); }
}

// At CritScan's battle change: the summary lines (one per effect that did something), then a clean slate.
static void GfxBattleChange(bool had) {
  SafeFrugalRestore();                                                 // first, in its own guard: nothing below can skip it
  if (had && g_gfxReady) {
    VetCheck(true);
    auto line = [](const char* what, std::string (*part)(const std::string&, const GfxStats&)) {
      std::string all;
      for (auto& e : g_gfxStats) { std::string p = part(e.first, e.second); if (!p.empty()) all += (all.empty() ? "" : "; ") + p; }
      if (!all.empty()) Log("%s, last battle: %s", what, all.c_str());
    };
    char b[400];
    line("marking", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.marks && !s.markedHits) return std::string();
      snprintf(b, sizeof b, "'%s' marked %d ship(s), hit marked ships %d times (+%.0f damage)", n.c_str(), s.marks, s.markedHits, s.markDmg); return std::string(b); });
    line("giant slayer", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.giantHits) return std::string();
      snprintf(b, sizeof b, "'%s' %d hits on Rare/Elite ships (+%.0f damage)", n.c_str(), s.giantHits, s.giantDmg); return std::string(b); });
    line("opening salvo", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.salvoShots) return std::string();
      snprintf(b, sizeof b, "'%s' %d salvo shots, %d hits and blasts (+%.0f damage)", n.c_str(), s.salvoShots, s.salvoHits, s.salvoDmg); return std::string(b); });
    line("close quarters / long shot", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.closeHits && !s.longHits) return std::string();
      if (s.closeHits) snprintf(b, sizeof b, "'%s' %d close hits (+%.0f damage)", n.c_str(), s.closeHits, s.closeDmg); else snprintf(b, sizeof b, "'%s' %d long hits (+%.0f damage)", n.c_str(), s.longHits, s.longDmg);
      return std::string(b); });
    line("overkill", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.spills) return std::string();
      snprintf(b, sizeof b, "'%s' %d spill(s), %.0f damage sent on", n.c_str(), s.spills, s.spillDmg); return std::string(b); });
    line("veteran's", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.vetHits && !s.vetKills) return std::string();
      const Item* it = FindItem(n); snprintf(b, sizeof b, "'%s' %d new kill(s), %d boosted hits (+%.0f damage); gunner '%s', %d kills", n.c_str(), s.vetKills, s.vetHits, s.vetDmg,
      it && !it->vetCrew.empty() ? it->vetCrew.c_str() : "none", it ? it->vetKills : 0); return std::string(b); });
    line("rallying", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.rallyKills) return std::string();
      snprintf(b, sizeof b, "'%s' %d kill(s), %d downed crew helped, %.0f s given", n.c_str(), s.rallyKills, s.rallyCrew, s.rallySecs); return std::string(b); });
    line("sure-footed", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.stacks) return std::string();
      snprintf(b, sizeof b, "'%s' %d stacking hits", n.c_str(), s.stacks); return std::string(b); });
    if (g_steadyPeak > 0) { snprintf(b, sizeof b, "sure-footed, last battle: at most %.1f stacks; %d hits on your crew while steady, %d of them braced", g_steadyPeak, g_steadyHits, g_steadyBraced); Log("%s", b); }
    line("frugal", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.shots) return std::string();
      snprintf(b, sizeof b, "'%s' %d of %d shots used no ammo", n.c_str(), s.freeShots, s.shots); return std::string(b); });
    line("bonus cap", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.capHits) return std::string();
      snprintf(b, sizeof b, "'%s' %d hits held to +200%% (%.0f damage cut)", n.c_str(), s.capHits, s.capCut); return std::string(b); });
    line("brittle", [](const std::string& n, const GfxStats& s) { char b[300]; if (!s.brittleHits) return std::string();
      snprintf(b, sizeof b, "'%s' hit %d times (+%.0f damage taken)", n.c_str(), s.brittleHits, s.brittleDmg); return std::string(b); });
    if (g_vetDirty) { g_vetDirty = false; SaveInventory(); }
  }
  g_gfxCache.clear(); g_gfxStats.clear(); g_marks.clear(); g_salvo.clear(); g_salvoShots.clear(); g_gfxFragTypes.clear(); g_brittle.clear(); g_brittleAt = 0;
  g_spills.clear(); g_spilled.clear(); g_gfxKillSeen.clear(); g_vetDeadSeen.clear(); g_vetSeenCh.clear(); g_hitLastMount = g_hitLastCe = nullptr;
  g_steadyStacks = g_steadyPer = g_steadyPeak = 0; g_steadyCap = g_steadyBraced = g_steadyHits = 0;
  memset(g_gfxLines, 0, sizeof g_gfxLines);
}
static void SafeGfxBattleChange(bool had) {
  __try { GfxBattleChange(had); }
  __except (FaultFilter("gun effects (battle end)", GetExceptionInformation())) { g_gfxFaults = -1; g_gfxHitOn = false; g_gfxCrewOn = false; SafeFrugalRestore(); }
}

