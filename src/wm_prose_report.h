// the prose after-action report, crew files record, battle summary (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- the prose report (crew_report.h renders it) ----
// reports.ini in BoatCrewMod replaces any built-in [section] it has; reports.default.ini is the built-in set, written
// for reference (each session) so a player can copy a section and reword it without a rebuild.
static std::string CrewReadText(const std::string& path) {
  std::string out; FILE* f = fopen(path.c_str(), "rb"); if (!f) return out;
  char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0 && out.size() < (4u << 20)) out.append(b, n);
  fclose(f);
  if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) out.erase(0, 3);   // Notepad's "UTF-8 with BOM"
  return out;
}
static std::map<std::string, std::vector<std::string>> CrewReportTables() {
  static bool wrote = false;
  if (!wrote) { wrote = true; FILE* f = fopen((g_folder + "\\reports.default.ini").c_str(), "wb"); if (f) { fputs(ReportDefaultIni(), f); fclose(f); } }
  std::string text = CrewReadText(g_folder + "\\reports.ini");
  auto t = text.empty() ? std::map<std::string, std::vector<std::string>>() : ParseReportIni(text);
  if (!text.empty()) Log("crew report: reports.ini has %d section(s); each one with rows replaces the built-in section of that name", (int)t.size());
  return t;
}
static int CrewRepOutcome(const CrewRec& r) {
  switch (r.state) { case 2: return O_KIA; case 3: return O_MISSING; case 4: return O_CAPTURED; case 5: return O_LOST_PICKED; default: break; }
  return r.down ? O_DOWN_AT_END : r.state == 1 || r.wounded ? O_WOUNDED : r.hits ? O_LIGHT : O_UNHURT;
}
static std::vector<std::pair<uint32_t, uint32_t>> g_stationNow;      // this report's stations: (dead id, heir id), saved by CrewFilesRecord
static const char* StationNoun(const LifeFate& f) {                    // for the log
  static const char* kNoun[] = { "ammunition party place", "damage control place", "medical kit", "gun", "wheel" };
  return f.role == crew_report_detail::RC_GUN && !f.gun.empty() ? f.gun.c_str() : f.role >= 0 && f.role <= crew_report_detail::RC_HELM ? kNoun[f.role] : "station";
}
static RepBattle CrewRepBattle(uint32_t seed) {
  RepBattle b; b.len = CrewEvT(); b.seed = seed;
  CrewFilesEnsure();
  for (size_t i = 0; i < g_crewKills.size(); i++) {
    RepBattle::Kill k; k.id = (int)i; k.enemy = k.enemyBase = g_crewKills[i].who; k.t = g_crewKills[i].at - g_crewLogStart; if (k.t < 0) k.t = 0;
    b.kills.push_back(k);
  }
  for (CrewRec& r : g_crewRecs) {
    RepCrew c; c.name = r.name; c.female = r.ch && FemGGet(r.ch) == 2;
    c.role = !r.firstRole.empty() ? r.firstRole : r.role.empty() ? std::string("standing by") : r.role;
    if (c.role.rfind("on the ", 0) == 0) c.gun = c.role.substr(7);
    c.maxHp = r.maxHp; c.endHp = r.lastHp < 0 ? 0 : r.lastHp; c.outcome = CrewRepOutcome(r);
    c.ev = r.ev; c.grazes = r.grazeEv;
    for (RepEv& e : c.ev) {
      if (e.kind != K_KILL) continue;
      if (c.gun.empty() && !e.gun.empty()) c.gun = e.gun;
      if (e.killId >= 0 && e.killId < (int)g_crewKills.size()) {
        for (size_t j : g_crewKills[e.killId].recs) if (j < g_crewRecs.size() && &g_crewRecs[j] != &r) e.mates.push_back(g_crewRecs[j].name);
        RepBattle::Kill& k = b.kills[e.killId]; k.enemy = e.enemy; k.enemyBase = e.enemyBase;   // the elite name, as the crew saw it
      }
    }
    {                                                                  // a gunner is named for the gun that made most of his kills
      std::map<std::string, int> n; std::string best; int bn = 0;
      for (const RepEv& e : c.ev) if (e.kind == K_KILL && !e.gun.empty() && ++n[e.gun] > bn) { bn = n[e.gun]; best = e.gun; }
      if (!best.empty()) c.gun = best;
    }
    if (!c.gun.empty()) { const Item* it = FindItem(c.gun); c.gunBase = it && !it->base.empty() ? it->base : std::string(); }
    if (CrewFile* f = CrewFileById(CrewIdOf(r.ch))) c.nick = NickOf(*f);
    b.crew.push_back(std::move(c));
  }
  // The dead man's station: who holds the gun or job of a crew member lost in the last three battles, decided here
  // because the report renders before the battle is filed. Told once (station= in his file).
  g_stationNow.clear();
  if (g_crewFilesGen == g_crewFilesGenAtStart) {
    using crew_report_detail::RC_GUN; using crew_report_detail::RC_OTHER;
    int bn = g_crewBattleNo + 1;
    std::set<uint32_t> aboard = TalkAboard();
    // shortcut: two same-named guns aboard count as one station; key on mount if it reads wrong
    for (const CrewFile& f : g_crewFiles) {
      const LifeFate& fa = f.fate;
      if (g_stationNow.size() >= 2) break;
      if (!fa.battle || bn - fa.battle > 3 || f.station || aboard.count(f.id)) continue;
      if (fa.role < 0 || fa.role == RC_OTHER) continue;                // no station known
      if (fa.role == RC_GUN && (fa.sunk || fa.gun.empty())) continue;  // his gun went down with the boat, or a fate filed before guns were
      int heir = -1;
      for (size_t i = 0; i < b.crew.size() && heir < 0; i++) {
        RepCrew& c = b.crew[i];
        if (!c.tookDead.empty()) continue;
        bool holds = StationRole(c.role, fa);
        for (const RepEv& v : c.ev) if (v.kind == S_ROLE && StationRole(v.part, fa)) holds = true;
        if (!holds) continue;
        bool old = false;                                              // he held it already in the battle the man was lost in
        if (const CrewFile* hf = CrewFileById(CrewIdOf(g_crewRecs[i].ch)))
          for (const CrewFileEntry& en : hf->entries) if (en.battle == fa.battle && StationRole(en.role, fa)) old = true;
        if (!old) heir = (int)i;
      }
      if (heir < 0) { DLog("crew station: nobody new holds %s's %s this battle (lost in battle %d)", f.name.c_str(), StationNoun(fa), fa.battle); continue; }
      RepCrew& h = b.crew[heir];
      h.tookDead = f.name; h.tookGun = fa.gun; h.tookJob = fa.role;
      g_stationNow.push_back({ f.id, CrewIdOf(g_crewRecs[heir].ch) });
      Log("crew station: %s took over %s's %s (lost in battle %d)", h.name.c_str(), f.name.c_str(), StationNoun(fa), fa.battle);
    }
  }
  int nicks = 0; for (const RepCrew& c : b.crew) if (!c.nick.empty()) nicks++;
  Log("crew nicknames: %d kept nickname(s) in this report; crew station: %d line(s)", nicks, (int)g_stationNow.size());
  return b;
}
// The battle's raw data for offline re-rendering (tools/crew_report_test.exe render <folder>): BoatCrewMod\battles.
// One file per battle (tens of KB); the newest 50 are kept.
static void CrewBattleDump(const RepBattle& b, const SYSTEMTIME& t) {
  try {
    std::string dir = g_folder + "\\battles";
    CreateDirectoryA(dir.c_str(), nullptr);
    char name[64]; snprintf(name, sizeof name, "\\%04d-%02d-%02d_%02d%02d%02d.bcb", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    std::string text = DumpBattle(b);
    FILE* f = fopen((dir + name).c_str(), "wb");
    if (!f) { Log("crew report: could not write battles%s", name); return; }
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    DLog("crew report: raw battle data written to battles%s (%d bytes)", name, (int)text.size());
    std::vector<std::string> all; WIN32_FIND_DATAA fd; HANDLE fh = FindFirstFileA((dir + "\\*.bcb").c_str(), &fd);
    if (fh != INVALID_HANDLE_VALUE) { do all.push_back(fd.cFileName); while (FindNextFileA(fh, &fd)); FindClose(fh); }
    std::sort(all.begin(), all.end());                                 // names are date and time: oldest first
    for (size_t i = 0; i + 50 < all.size(); i++) DeleteFileA((dir + "\\" + all[i]).c_str());
  } catch (...) { Log("crew report: the raw battle data could not be written"); }
}
static std::vector<RepPara> CrewRender(const RepBattle& b, const std::map<std::string, std::vector<std::string>>& tables) {
  try { return RenderReport(b, tables); }
  catch (...) { Log("crew report: the prose report failed; only the log is written"); return {}; }
}
// Each crew member's part of the report goes into their file. The paragraphs after the k-th crew
// heading (style 1) are g_crewRecs[k]'s (RenderReport keeps the crew order); a quiet battle has no headings, and then
// everyone gets the summary.
static void CrewFilesRecord(const std::vector<RepPara>& paras, const SYSTEMTIME& t, const RepBattle* rb, const LifeTables* report) {
  if (g_crewFilesGen != g_crewFilesGenAtStart) { Log("crew files: a save was loaded or a campaign started during that battle; it is not added to the crew files"); return; }
  CrewFilesEnsure();
  std::vector<std::vector<std::string>> per(g_crewRecs.size()); std::vector<std::string> roles(g_crewRecs.size()), common;
  int k = -1;
  for (const RepPara& p : paras) {
    if (p.style == 1) { if (++k < (int)roles.size()) { size_t d = p.text.rfind(" - "); roles[k] = d == std::string::npos ? std::string() : p.text.substr(d + 3); } continue; }
    if (p.style != 0) continue;
    if (k < 0) common.push_back(p.text); else if (k < (int)per.size()) per[k].push_back(p.text);
  }
  if (k < 0 && common.size() == 1 && common[0].rfind("The report could not be written", 0) == 0) common.clear();   // RenderReport's failure line
  char when[32]; snprintf(when, sizeof when, "%04d-%02d-%02d %02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
  int bn = g_crewBattleNo + 1, n = 0, lost = 0;
  int nickWaiting = 0, nickNew = 0; bool nickCapSaid = false;                       // offers waiting across all files: 3 at most
  for (const CrewFile& f : g_crewFiles) for (const NickRec& r : f.nicks) if (r.state == 'p') nickWaiting++;
  for (size_t i = 0; i < g_crewRecs.size(); i++) {
    const CrewRec& r = g_crewRecs[i];
    uint32_t id = CrewIdOf(r.ch);                                      // still pinned here; unpinned after the summary
    if (!id) continue;
    CrewFile* cf = nullptr;
    for (CrewFile& f : g_crewFiles) if (f.id == id) cf = &f;
    if (!cf) { g_crewFiles.push_back(CrewFile()); cf = &g_crewFiles.back(); cf->id = id; }
    cf->name = r.name; cf->female = FemGGet(r.ch) == 2;
    if (cf->facts.empty()) cf->facts = LifeRoll(*cf, r.ch);
    if (cf->voice.empty()) cf->voice = RollVoice(cf->id, CrewTraitWords(r.ch));
    int oc = rb && i < rb->crew.size() ? rb->crew[i].outcome : O_UNHURT;
    if (oc == O_KIA || oc == O_MISSING || oc == O_CAPTURED) { cf->fate = FateOf(*rb, i, bn, report); lost++; LetterClear(*cf); }
    else if (cf->fate.battle) { Log("crew lives: %s fought again after being listed lost in battle %d; no longer lost", cf->name.c_str(), cf->fate.battle); cf->fate = LifeFate(); LetterClear(*cf); }
    CrewFileEntry e; e.battle = bn; e.when = when; e.len = CrewLogClock(); e.outcome = CrewOutcome(r);
    e.role = !roles[i].empty() ? roles[i] : !r.firstRole.empty() ? r.firstRole : r.role;
    e.kills = r.kills; e.hits = r.hits; e.dmg = (int)(r.dmg + 0.5f); e.downs = r.downs; e.over = r.overboards;
    for (const RepEv& v : r.ev) if (v.kind == S_WOUNDED) e.wounds++;
    if (!e.wounds && r.wounded) e.wounds = 1;
    e.paras = k >= 0 ? per[i] : common;
    cf->entries.push_back(std::move(e));
    if (cf->entries.size() > 300) cf->entries.erase(cf->entries.begin());   // shortcut: oldest battles dropped past 300; keep all if a file view of more is wanted
    {                                                                  // a nickname earned (crew_nick.h): offered on the after-battle screen
      std::set<std::string> taken;
      for (const CrewFile& o : g_crewFiles) if (&o != cf) for (const NickRec& r : o.nicks) if (r.state == 'k' || r.state == 'p') taken.insert(r.nick);
      // crew_stories=0: offers could never be answered (KEEP/VETO live on the report), so none are made
      if (const char* nk = g_storiesOn ? NickEarned(cf->entries, cf->fate.battle, cf->nicks, taken) : nullptr) {
        if (nickWaiting >= 3) {
          if (!nickCapSaid) { if (nickNew) Log("crew nicknames: %d offered this battle, more held back by the cap of 3 waiting", nickNew); else Log("crew nicknames: %d offer(s) waiting; no new offers this battle", nickWaiting); }
          nickCapSaid = true;
        }
        else {
          NickRec r; r.state = 'p'; r.nick = nk; r.battle = bn; cf->nicks.push_back(r); nickWaiting++; nickNew++;
          Log("crew nicknames: %s earned \"%s\" in battle %d (for %s); offered on the after-battle screen", cf->name.c_str(), nk, bn, NickWhy(nk));
        }
      }
    }
    n++;
  }
  if (!n) return;
  if (rb) {                                                            // bonds: what each pair shared in this battle (saved with it below)
    std::vector<uint32_t> ids; std::vector<uint64_t> guns;
    for (const CrewRec& r : g_crewRecs) { ids.push_back(CrewIdOf(r.ch)); guns.push_back(r.firstGun); }
    BondLedger led = BondsOnFile();
    BondsAfterBattle(*rb, ids, bn, led, &guns);
    int close = 0;
    for (CrewFile& f : g_crewFiles) {
      auto it = led.find(f.id); if (it == led.end()) continue;
      std::vector<BondTally> v = it->second;
      BondTrim(v, bn);                                                 // the 64 closest kept, this battle's mates always
      f.bonds = v;
      if (!f.fate.battle && BondClosestOf(f, false)) close++;
    }
    Log("crew bonds: battle %d added; %d crew member(s) now have a close mate", bn, close);
  }
  for (auto& p : g_stationNow) if (CrewFile* d = CrewFileById(p.first)) {   // told: never again, but only if the heir's paragraphs carry it (a quiet report has none)
    std::string sur = d->name.substr(d->name.rfind(' ') + 1);
    bool told = false;
    for (size_t i = 0; i < g_crewRecs.size() && i < per.size(); i++) if (CrewIdOf(g_crewRecs[i].ch) == p.second) for (const std::string& t : per[i]) if (t.find(sur) != std::string::npos) told = true;
    if (told) { d->station = p.second; d->stationBattle = bn; }
    else Log("crew station: %s's station line is not in the report; offered again next battle", d->name.c_str());
  }
  g_crewBattleNo = bn; g_crewFilesSeq++;
  bool ok = CrewFilesWrite();
  Log("crew files: battle %d added to %d crew file(s) (%d on file)%s", bn, n, (int)g_crewFiles.size(), ok ? "" : "; crew-files.ini could NOT be written");
  if (lost && g_lettersOn) Log("crew lives: %d crew lost in battle %d; their letters home wait on the after-battle screen", lost, bn);
  if (ok && !g_lettersOn) Log("crew lives: no mail call after battle %d (crew_letters=0)", bn);
  if (ok && g_lettersOn) {                                             // mail call: decided and written after the battle is on file
    std::vector<MailCrew> mc;
    for (CrewFile& f : g_crewFiles) {
      MailCrew c; c.id = f.id; c.name = f.name; c.female = f.female; c.facts = &f.facts; c.fate = &f.fate;
      c.fought = !f.entries.empty() && f.entries.back().battle == bn; c.st = &f.mail; mc.push_back(c);
    }
    std::vector<MailDelivery> got = MailCall(bn, g_mailLast, rb ? rb->seed : (uint32_t)bn, mc, LifeTabs());
    g_mailNow.clear();
    for (const MailDelivery& d : got) g_mailNow.push_back({ g_crewFiles[d.crew].name, g_crewFiles[d.crew].female, d.letter });
    if (!got.empty()) {
      g_mailLast = bn; g_mailBattle = bn; g_mailUnseen = true; g_mailSeq++; g_crewFilesSeq++;
      int back = 0; for (const MailDelivery& d : got) if (d.letter.kind) back++;
      bool ok2 = CrewFilesWrite();
      Log("crew lives: mail call after battle %d: %d letter(s), %d of them returned or forwarded (%d threads)%s", bn, (int)got.size(), back, MailThreadCount(), ok2 ? "" : "; crew-files.ini could NOT be written");
      for (const MailDelivery& d : got) DLog("crew lives: mail for %s from %s (%s, step %d)%s", g_crewFiles[d.crew].name.c_str(), MailFrom(d.letter, g_crewFiles[d.crew].female).c_str(), d.letter.thread.c_str(), d.letter.step + 1, d.letter.kind ? " RETURNED" : "");
    }
  }
}
static void SafeCrewFilesRecord(const std::vector<RepPara>& paras, const SYSTEMTIME& t, const RepBattle* rb, const LifeTables* report) {
  __try { CrewFilesRecord(paras, t, rb, report); } __except (FaultFilter("crew files (recording a battle)", GetExceptionInformation())) {}
}
static void CrewLogSummary() {
  if (g_crewRecs.empty()) return;
  SYSTEMTIME t; GetLocalTime(&t);
  { std::string cr = g_folder + "\\crew-reports.txt"; WIN32_FILE_ATTRIBUTE_DATA fa;   // over 2 MB: it moves to crew-reports.old.txt
    if (GetFileAttributesExA(cr.c_str(), GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > 2u * 1024 * 1024))
      MoveFileExA(cr.c_str(), (g_folder + "\\crew-reports.old.txt").c_str(), MOVEFILE_REPLACE_EXISTING); }
  FILE* f = fopen((g_folder + "\\crew-reports.txt").c_str(), "a");
  char head[160]; snprintf(head, sizeof head, "==== After-action report, %04d-%02d-%02d %02d:%02d (battle time %s) ====", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, CrewLogClock().c_str());
  Log("crew log, last battle: %s", head);
  if (f) fprintf(f, "\n%s\n", head);
  {
    uint32_t seed = 2166136261u; for (const char* c = head; *c; c++) seed = (seed ^ (uint8_t)*c) * 16777619u;
    RepBattle rb = CrewRepBattle(seed);
    CrewBattleDump(rb, t);
    auto tables = CrewReportTables();                                  // read once: the report and the fates use the same
    std::vector<RepPara> paras = CrewRender(rb, tables);
    if (f) for (const RepPara& p : paras) fprintf(f, "\n%s\n", p.text.c_str());
    int before = g_crewBattleNo;
    SafeCrewFilesRecord(paras, t, &rb, &tables);                       // before the move below
    g_crewBattleNoShown = g_crewBattleNo != before ? g_crewBattleNo : 0;   // the report's letters: none if the battle was not filed
    g_aarParas = std::move(paras); if (!g_aarParas.empty()) g_aarSeq++;   // a failed render leaves no stale report
    Log("crew report: %d paragraph(s) for the after-battle screen", (int)g_aarParas.size());
    if (f) fprintf(f, "\n---- the log ----\n");
  }
  int hits = 0, kills = 0, dead = 0;
  for (CrewRec& r : g_crewRecs) {
    hits += r.hits; kills += r.kills; if (r.state == 2) dead++;
    std::string h = r.name + " (" + (r.role.empty() ? "not seen aboard" : r.role) + "): " + CrewOutcome(r) + ".";
    auto count = [](int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); };
    if (r.hits) {
      std::vector<std::string> kinds;
      if (r.shells) kinds.push_back(count(r.shells, "round", "rounds"));
      if (r.blasts) kinds.push_back(count(r.blasts, "blast", "blasts"));
      if (r.burns) kinds.push_back(count(r.burns, "burn", "burns"));
      if (r.other) kinds.push_back(count(r.other, "other hit", "other hits"));
      char b[64]; snprintf(b, sizeof b, " for %.0f HP", r.dmg);
      h += " " + count(r.hits, "hit", "hits") + b + ": " + CrewJoin(kinds);
      if (r.grazes) h += r.grazes == r.hits ? (r.hits == 1 ? ", a graze" : ", all grazes") : r.grazes == 1 ? ", one of them a graze" : ", " + std::to_string(r.grazes) + " of them grazes";
      h += ".";
    } else if (!r.hitsDown) h += " Not hit.";
    if (!r.dmgBy.empty()) {                                            // the worst of it, by who dealt it
      std::vector<std::pair<float, std::string>> by;
      for (auto& e : r.dmgBy) by.push_back({ e.second.hp, e.first });
      std::sort(by.rbegin(), by.rend());
      std::vector<std::string> top;
      for (size_t i = 0; i < by.size() && i < 3; i++) {                 // "'X' (222 HP in 105 hits, 01:10-03:50, its Type 96)"
        const CrewRec::Dmg& g = r.dmgBy[by[i].second];
        auto mmss = [](float t) { int s = t < 0 ? 0 : (int)t; char c[16]; snprintf(c, sizeof c, "%02d:%02d", s / 60, s % 60); return std::string(c); };
        std::vector<std::string> guns(g.guns.begin(), g.guns.end());
        char b[200]; snprintf(b, sizeof b, "%s (%.0f HP in %s, %s%s%s", by[i].second.c_str(), g.hp, count(g.n, "hit", "hits").c_str(), mmss(g.first).c_str(), (int)g.last != (int)g.first ? "-" : "", (int)g.last != (int)g.first ? mmss(g.last).c_str() : "");
        top.push_back(std::string(b) + (guns.empty() ? "" : (by[i].second == "our own guns" ? ", our " : ", its ") + CrewJoin(guns)) + ")");
      }
      h += " Hurt by " + CrewJoin(top) + (by.size() > 3 ? " and others" : "") + ".";
    }
    if (r.hitsDown) h += " Hit " + count(r.hitsDown, r.hits ? "more time" : "time", r.hits ? "more times" : "times") + " while down.";
    if (r.kills) h += " " + count(r.kills, "kill", "kills") + (r.shared ? " (" + std::to_string(r.shared) + " shared)" : "") + ".";
    if (r.downs) h += r.downs == 1 ? " Went down once." : " Went down " + std::to_string(r.downs) + " times.";
    if (r.overboards) h += r.overboards == 1 ? " Overboard once." : " Overboard " + std::to_string(r.overboards) + " times.";
    Log("crew log, last battle: %s", h.c_str());
    if (f) { fprintf(f, "\n%s\n", h.c_str()); for (auto& l : r.log) fprintf(f, "    %s\n", l.c_str()); if (r.lines > (int)r.log.size()) fprintf(f, "    ... and %d more\n", r.lines - (int)r.log.size()); }
    for (auto& l : r.log) DLog("crew log:   %s: %s", r.name.c_str(), l.c_str());
  }
  Log("crew log, last battle: %d crew, %d hit(s), %d enemies killed (%d credits), %d of ours killed%s", (int)g_crewRecs.size(), hits, (int)g_crewKills.size(), kills, dead, f ? "; full report in crew-reports.txt" : "; crew-reports.txt could not be opened");
  if (f) fclose(f);
}
// At every battle change (CritScan): the last battle's report, then a clean slate.
static void SafeCrewLogSummary() { __try { CrewLogSummary(); } __except (CrewLogFilter(GetExceptionInformation())) {} }
static void SafeCrewLogBattleChange(bool hadBattle) {
  if (hadBattle) g_aarParas.clear();                                  // every way out of the summary leaves no older report behind
  if (g_crewLogOn && g_crewLogFaults >= 0 && hadBattle) {
    g_crewLogPollAt = 0; SafeCrewLogPoll();                             // the end-of-battle scan has run: who was picked up, who was lost
    SafeCrewLogSummary();
  }
  for (CrewRec& r : g_crewRecs) Unpin(r.pin);
  if (g_saySaid || g_sayFail) Log("crew bubbles, last battle: %d line(s) queued, %d failed call(s), %d nickname call(s)", g_saySaid, g_sayFail, TalkNickUses());
  TalkNickUses() = 0;
  g_saySaid = g_sayFail = 0; g_sayLastAny = -100; g_sayKills.clear();
  for (SayProbe& p : g_sayProbes) Unpin(p.pin);
  g_sayProbes.clear(); g_sayOpened = false; g_sayProbed = 0;
  TalkReset();
  g_crewRecs.clear(); g_crewRecOf.clear(); g_crewKills.clear(); g_crewSrcSeen.clear(); g_crewEliteName.clear(); g_crewLogStart = -1; g_crewLogLines = 0;
  g_crewFilesGenAtStart = g_crewFilesGen;                              // the next battle belongs to the crew files loaded now
}

