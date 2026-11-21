// crew files, crew lives, the crew files / mail call / letter windows (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- crew files ----
// Each crew member's battles, kept by crew id (MOD_CREWID). BoatCrewMod\crew-files.ini is the live file, rewritten
// after every battle (CrewFilesRecord, from the crew log's summary); every game save gets a copy as
// saves\<hash of the save>.crew, like the inventory. Loading a save puts its copy back; a save with no copy and a new
// campaign start with none. The old live file goes to saves\backup\crew-<time>.ini first.
// Format: 'battles=N', then per crew '[crew <id>]', 'name=', 'female=', and per battle
// 'battle=<n>|<date>|<length>|<role>|<outcome>|<kills>|<hits>|<HP lost>|<wounds>|<downs>|<overboard>' + 'p=<paragraph>'.
// Bonds (crew_bonds.h): 'bond=<other id>|<battles>|<same gun>|<kills>|<sinkings>|<swims>|<last battle>'.
// Crew lives (crew_life.h): 'home=', 'job=', 'keep=', 'plan=', 'kin=<relation>|<first names>', 'kids=', for the lost
// 'fate=<battle>|<kind>|<by>|<part>|<role>|<minutes>|<boat sunk>|<gun>', and a signed letter 'letter=1|<truth>|<pick>' + 'l=<paragraph>'.
// Nicknames (crew_nick.h): 'nick=<p offered|k kept|v vetoed>|<nickname>|<battle earned>', one per record. The lost:
// 'station=<heir id>|<battle>', who took over their gun or job (told once).
// Mail call (crew_mail.h): 'mailcall=<battle>' in the header; per crew 'mail=<thread>|<next step>|<last battle>|<returned>|<used,...>|<sender>|<family facts>'
// and every letter 'ml=<battle>|<kind>|<thread>|<step>|<who>|<sender>|<Dear ...>|<body>|<sign>'.
struct CrewFileEntry { int battle = 0; std::string when, len, role, outcome; int kills = 0, hits = 0, dmg = 0, wounds = 0, downs = 0, over = 0; std::vector<std::string> paras; };
struct CrewFile {
  uint32_t id = 0; std::string name; bool female = false; std::vector<CrewFileEntry> entries;
  LifeFacts facts; LifeFate fate;                                      // crew lives: rolled once; how they were lost
  int letter = 0; bool truth = true; int pick = 0; std::vector<std::string> letterText;   // the signed letter home
  MailState mail;                                                      // mail call: the thread, its step, every letter
  std::string voice;                                                   // the talking crew: joker, stoic, ... (crew_talk.h)
  std::vector<BondTally> bonds;                                        // what they shared with each other crew member (crew_bonds.h), the closest 64
  std::vector<NickRec> nicks;        // history: a vetoed name is never offered to him again; at most one 'k', at most one 'p'
  uint32_t station = 0; int stationBattle = 0;   // lost crew: who took over his gun or job, and in which battle (said once)
};
static std::vector<CrewFile> g_crewFiles;
static int g_crewBattleNo = 0, g_crewFilesSeq = 0;                     // battles recorded in this campaign; bumped on every change
static volatile LONG g_crewFilesStale = 1;                             // the live file changed on disk (a load may run off the game thread)
static volatile LONG g_crewFilesGen = 0; static LONG g_crewFilesGenAtStart = 0;   // switches so far / at the battle's start
static std::string CrewFilesPath() { return g_folder + "\\crew-files.ini"; }
static std::string CrewFileClean(std::string s) { for (char& c : s) if (c == '\r' || c == '\n') c = ' '; else if (c == '|') c = '/'; return s; }
static void CrewFilesRead() {
  g_crewFiles.clear(); g_crewBattleNo = 0; g_crewFilesSeq++;
  g_letterOffered = 0; g_crewBattleNoShown = 0;                        // battle numbers belong to the files they came with
  g_mailLast = 0; g_mailNow.clear(); g_mailBattle = 0; g_mailUnseen = false; g_mailOpen = false;
  std::string all; FILE* f = fopen(CrewFilesPath().c_str(), "rb");
  if (!f) return;
  bool mailFile = false;                                               // written by a build with mail call
  char b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) all.append(b, n);
  fclose(f);
  size_t pos = 0;
  while (pos < all.size()) {
    size_t e = all.find('\n', pos); if (e == std::string::npos) e = all.size();
    std::string l = all.substr(pos, e - pos); pos = e + 1;
    if (!l.empty() && l.back() == '\r') l.pop_back();
    if (l.rfind("battles=", 0) == 0) g_crewBattleNo = atoi(l.c_str() + 8);
    else if (l.rfind("mailcall=", 0) == 0) { g_mailLast = atoi(l.c_str() + 9); mailFile = true; }
    else if (l.rfind("[crew ", 0) == 0) { CrewFile c; c.id = (uint32_t)strtoul(l.c_str() + 6, nullptr, 16); if (c.id) g_crewFiles.push_back(c); }
    else if (g_crewFiles.empty()) continue;
    else if (l.rfind("name=", 0) == 0) g_crewFiles.back().name = l.substr(5);
    else if (l.rfind("female=", 0) == 0) g_crewFiles.back().female = l[7] == '1';
    else if (l.rfind("home=", 0) == 0) g_crewFiles.back().facts.home = l.substr(5);
    else if (l.rfind("job=", 0) == 0) g_crewFiles.back().facts.job = l.substr(4);
    else if (l.rfind("keep=", 0) == 0) g_crewFiles.back().facts.keep = l.substr(5);
    else if (l.rfind("plan=", 0) == 0) g_crewFiles.back().facts.plan = l.substr(5);
    else if (l.rfind("kids=", 0) == 0) g_crewFiles.back().facts.kids = l[5] == '1';
    else if (l.rfind("orphan=", 0) == 0) g_crewFiles.back().facts.orphan = l[7] == '1';
    else if (l.rfind("voice=", 0) == 0) g_crewFiles.back().voice = l.substr(6);
    else if (l.rfind("nick=", 0) == 0) {
      std::vector<std::string> v; size_t a = 5;
      for (;;) { size_t z = l.find('|', a); v.push_back(l.substr(a, z == std::string::npos ? std::string::npos : z - a)); if (z == std::string::npos) break; a = z + 1; }
      v.resize(3);
      if (v[0].size() == 1 && strchr("pkv", v[0][0]) && !v[1].empty()) { NickRec r; r.state = v[0][0]; r.nick = v[1]; r.battle = atoi(v[2].c_str()); g_crewFiles.back().nicks.push_back(r); }
    }
    else if (l.rfind("station=", 0) == 0) {
      unsigned h = 0; int sb = 0;
      if (sscanf(l.c_str() + 8, "%x|%d", &h, &sb) >= 1 && h) { g_crewFiles.back().station = h; g_crewFiles.back().stationBattle = sb; }
    }
    else if (l.rfind("bond=", 0) == 0) {
      BondTally b; unsigned o = 0;
      if (sscanf(l.c_str() + 5, "%x|%d|%d|%d|%d|%d|%d", &o, &b.battles, &b.team, &b.kills, &b.sank, &b.water, &b.last) >= 6 && o) { b.other = o; g_crewFiles.back().bonds.push_back(b); }
    }
    else if (l.rfind("kin=", 0) == 0) { size_t b = l.find('|'); LifeFacts& lf = g_crewFiles.back().facts; lf.kin = l.substr(4, b == std::string::npos ? std::string::npos : b - 4); lf.kinFirst = b == std::string::npos ? "" : l.substr(b + 1); }
    else if (l.rfind("fate=", 0) == 0 || l.rfind("letter=", 0) == 0) {
      bool fate = l[0] == 'f'; std::vector<std::string> v; size_t a = fate ? 5 : 7;
      for (;;) { size_t z = l.find('|', a); v.push_back(l.substr(a, z == std::string::npos ? std::string::npos : z - a)); if (z == std::string::npos) break; a = z + 1; }
      v.resize(8);
      CrewFile& c = g_crewFiles.back();
      if (fate) { c.fate.battle = atoi(v[0].c_str()); c.fate.kind = v[1].empty() ? 'k' : v[1][0]; c.fate.by = v[2]; c.fate.part = v[3]; c.fate.role = v[4].empty() ? -1 : atoi(v[4].c_str()); c.fate.mins = (float)atof(v[5].c_str()); c.fate.sunk = v[6] == "1"; c.fate.gun = v[7]; }
      else { c.letter = atoi(v[0].c_str()); c.truth = v[1] != "0"; c.pick = atoi(v[2].c_str()); }
    }
    else if (l.rfind("l=", 0) == 0) g_crewFiles.back().letterText.push_back(l.substr(2));
    else if (l.rfind("mail=", 0) == 0 || l.rfind("ml=", 0) == 0) {
      bool st = l[1] == 'a'; std::vector<std::string> v; size_t a = st ? 5 : 3;
      for (;;) { size_t z = l.find('|', a); v.push_back(l.substr(a, z == std::string::npos ? std::string::npos : z - a)); if (z == std::string::npos) break; a = z + 1; }
      v.resize(9);                                                     // mail= has 8 fields, ml= 9
      MailState& ms = g_crewFiles.back().mail;
      if (st) {
        ms.thread = v[0]; ms.step = (std::max)(0, atoi(v[1].c_str())); ms.last = atoi(v[2].c_str()); ms.returned = v[3] == "1"; ms.used.clear(); ms.sender = v[5]; ms.facts = v[6]; ms.told = atoi(v[7].c_str());
        for (size_t b = 0; b < v[4].size();) { size_t c = v[4].find(',', b); std::string u = v[4].substr(b, c == std::string::npos ? std::string::npos : c - b); if (!u.empty()) ms.used.push_back(u); if (c == std::string::npos) break; b = c + 1; }
      } else {
        MailLetter m; m.battle = atoi(v[0].c_str()); m.kind = atoi(v[1].c_str()); m.thread = v[2]; m.step = atoi(v[3].c_str());
        m.who = v[4]; m.sender = v[5]; m.dear = v[6]; m.body = v[7]; m.sign = v[8];
        ms.got.push_back(m);
      }
    }
    else if (l.rfind("battle=", 0) == 0) {
      std::vector<std::string> v; size_t a = 7;
      for (;;) { size_t z = l.find('|', a); v.push_back(l.substr(a, z == std::string::npos ? std::string::npos : z - a)); if (z == std::string::npos) break; a = z + 1; }
      v.resize(11);
      CrewFileEntry en; en.battle = atoi(v[0].c_str()); en.when = v[1]; en.len = v[2]; en.role = v[3]; en.outcome = v[4];
      en.kills = atoi(v[5].c_str()); en.hits = atoi(v[6].c_str()); en.dmg = atoi(v[7].c_str()); en.wounds = atoi(v[8].c_str()); en.downs = atoi(v[9].c_str()); en.over = atoi(v[10].c_str());
      g_crewFiles.back().entries.push_back(en);
    } else if (l.rfind("p=", 0) == 0 && !g_crewFiles.back().entries.empty()) g_crewFiles.back().entries.back().paras.push_back(l.substr(2));
  }
  if (!mailFile) for (CrewFile& c : g_crewFiles) if (c.fate.battle) c.mail.returned = true;   // lost before mail call: no letter comes back for them
}
static bool CrewFilesWrite() {
  std::string o = "; Boat Crew mod: each crew member's battles (the crew files on the crew screen). A copy goes with every game save.\n";
  o += "battles=" + std::to_string(g_crewBattleNo) + "\nmailcall=" + std::to_string(g_mailLast) + "\n";
  for (const CrewFile& c : g_crewFiles) {
    char id[16]; snprintf(id, sizeof id, "%08x", c.id);
    o += std::string("\n[crew ") + id + "]\nname=" + CrewFileClean(c.name) + "\nfemale=" + (c.female ? "1" : "0") + "\n";
    const LifeFacts& lf = c.facts;
    if (!lf.empty()) o += "home=" + CrewFileClean(lf.home) + "\njob=" + CrewFileClean(lf.job) + "\nkeep=" + CrewFileClean(lf.keep) + "\nplan=" + CrewFileClean(lf.plan) +
                          "\nkin=" + CrewFileClean(lf.kin) + "|" + CrewFileClean(lf.kinFirst) + "\nkids=" + (lf.kids ? "1" : "0") + "\norphan=" + (lf.orphan ? "1" : "0") + "\n";
    if (!c.voice.empty()) o += "voice=" + CrewFileClean(c.voice) + "\n";
    for (auto& n : c.nicks) o += "nick=" + std::string(1, n.state) + "|" + CrewFileClean(n.nick) + "|" + std::to_string(n.battle) + "\n";
    for (const BondTally& b : c.bonds) { char bl[96]; snprintf(bl, sizeof bl, "bond=%08x|%d|%d|%d|%d|%d|%d\n", b.other, b.battles, b.team, b.kills, b.sank, b.water, b.last); o += bl; }
    if (c.fate.battle) { char m[24]; snprintf(m, sizeof m, "%.2f", c.fate.mins);
      o += "fate=" + std::to_string(c.fate.battle) + "|" + std::string(1, c.fate.kind ? c.fate.kind : 'k') + "|" + CrewFileClean(c.fate.by) + "|" + CrewFileClean(c.fate.part) + "|" + std::to_string(c.fate.role) + "|" + m + "|" + (c.fate.sunk ? "1" : "0") + "|" + CrewFileClean(c.fate.gun) + "\n"; }
    if (c.station) { char st[40]; snprintf(st, sizeof st, "station=%08x|%d\n", c.station, c.stationBattle); o += st; }
    if (c.letter) { o += "letter=1|" + std::string(c.truth ? "1" : "0") + "|" + std::to_string(c.pick) + "\n"; for (const std::string& p : c.letterText) o += "l=" + CrewFileClean(p) + "\n"; }
    const MailState& ms = c.mail;
    if (!ms.thread.empty() || ms.last || ms.returned || !ms.used.empty() || !ms.facts.empty() || ms.told) {
      std::string used; for (const std::string& u : ms.used) used += (used.empty() ? "" : ",") + CrewFileClean(u);
      o += "mail=" + CrewFileClean(ms.thread) + "|" + std::to_string(ms.step) + "|" + std::to_string(ms.last) + "|" + (ms.returned ? "1" : "0") + "|" + used + "|" + CrewFileClean(ms.sender) + "|" + CrewFileClean(ms.facts) + "|" + std::to_string(ms.told) + "\n";
    }
    for (const MailLetter& m : ms.got)
      o += "ml=" + std::to_string(m.battle) + "|" + std::to_string(m.kind) + "|" + CrewFileClean(m.thread) + "|" + std::to_string(m.step) + "|" + CrewFileClean(m.who) + "|" +
           CrewFileClean(m.sender) + "|" + CrewFileClean(m.dear) + "|" + CrewFileClean(m.body) + "|" + CrewFileClean(m.sign) + "\n";
    for (const CrewFileEntry& e : c.entries) {
      char nums[96]; snprintf(nums, sizeof nums, "|%d|%d|%d|%d|%d|%d", e.kills, e.hits, e.dmg, e.wounds, e.downs, e.over);
      o += "battle=" + std::to_string(e.battle) + "|" + CrewFileClean(e.when) + "|" + CrewFileClean(e.len) + "|" + CrewFileClean(e.role) + "|" + CrewFileClean(e.outcome) + nums + "\n";
      for (const std::string& p : e.paras) o += "p=" + CrewFileClean(p) + "\n";
    }
  }
  std::string live = CrewFilesPath(), tmp = live + ".tmp";
  FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
  bool ok = fwrite(o.data(), 1, o.size(), f) == o.size(); ok = fclose(f) == 0 && ok;
  return ok && MoveFileExA(tmp.c_str(), live.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
static void CrewFilesEnsure() { if (InterlockedExchange(&g_crewFilesStale, 0)) CrewFilesRead(); }
// src: a save's copy, or empty for none. Only files on disk and a flag: the game thread reads it again when it needs it.
static void CrewFilesSwitch(const std::string& src, const std::string& why) {
  std::string live = CrewFilesPath();
  bool had = FileExists(live);
  InterlockedIncrement(&g_crewFilesGen);
  if (!had && src.empty()) { InterlockedExchange(&g_crewFilesStale, 1); return; }
  if (had) BackupLive("crew-files.ini", "crew");
  bool ok = src.empty() ? DeleteFileA(live.c_str()) != 0 : CopyAtomic(src, live);
  InterlockedExchange(&g_crewFilesStale, 1);
  if (!ok) Log("crew files: %s: could not %s (error %lu); the crew files may be another save's", why.c_str(), src.empty() ? "clear the live file" : "copy the save's crew files", GetLastError());
  else Log("crew files: %s: %s", why.c_str(), src.empty() ? (had ? "no crew files with it, starting empty (the last ones are in saves\\backup)" : "no crew files with it") : "its crew files are back");
}
// ---- crew lives (crew_life.h): facts, memorial, letters home ----
// Facts are rolled once per crew file (at the first battle on file, or when the crew screen first lists an older file)
// and kept in crew-files.ini; the vanilla traits steer the roll. Nobody's home, job, keepsake or plan repeats aboard.
static std::string CrewReadText(const std::string& path);
static const LifeTables& LifeTabs() {                                  // life.ini replaces built-in sections; read once a session
  static LifeTables t; static bool done = false;
  if (!done) {
    done = true;
    FILE* f = fopen((g_folder + "\\life.default.ini").c_str(), "wb"); if (f) { fputs(LifeDefaultIni(), f); fclose(f); }
    std::string text = CrewReadText(g_folder + "\\life.ini");
    if (!text.empty()) { t = ParseLifeIni(text); Log("crew lives: life.ini has %d section(s); each replaces the built-in one", (int)t.size()); }
  }
  return t;
}
// The vanilla trait codes and asset names of a crew member (CrewCharacter._traits -> CharacterTraitSet.traits).
static std::vector<std::string> CrewTraitWords(void* ch) {
  std::vector<std::string> out;
  static int oSet = -2, oList = -1, oCode = -1; static void* mName = nullptr; static int logged = 0;
  if (!ch) return out;
  if (oSet == -2) {
    oSet = FieldOffset(il2cpp_object_get_class(ch), "_traits");
    void* uo = FindClass("UnityEngine", "Object"); mName = uo ? FindMethod(uo, "get_name", 0) : nullptr;
    Log("crew lives: crew traits %s (_traits +0x%X)", oSet > 0 ? "found" : "NOT found; facts are rolled without them", oSet);
  }
  void* set = oSet > 0 ? *(void**)((char*)ch + oSet) : nullptr;       // null until the game first asks: no traits, no roll forced
  if (!set) return out;
  if (oList < 0) oList = FieldOffset(il2cpp_object_get_class(set), "traits");
  void* list = oList > 0 ? *(void**)((char*)set + oList) : nullptr;
  for (int i = 0, n = ListCount(list); i < n && i < 16; i++) {
    void* tr = ListAt(list, i); if (!tr) continue;
    if (oCode < 0) oCode = FieldOffset(il2cpp_object_get_class(tr), "code");
    if (oCode > 0) { void* s = *(void**)((char*)tr + oCode); if (s) out.push_back(FemUtf8(s)); }
    if (mName && Alive(tr)) { void* s = Invoke(mName, tr); if (s) out.push_back(FemUtf8(s)); }
  }
  if (logged < 10) { logged++; std::string j; for (auto& w : out) j += (j.empty() ? "" : ", ") + w; Log("crew lives: '%s' traits: %s", FemGShown(ch).c_str(), j.empty() ? "none" : j.c_str()); }
  return out;
}
static bool CrewTraitsLoaded(void* ch) { static int o = -2; if (o == -2) o = FieldOffset(il2cpp_object_get_class(ch), "_traits"); return o <= 0 || *(void**)((char*)ch + o) != nullptr; }
// shortcut: 'taken' is whoever already has facts, so a file rolled again after a reload without a save can differ
static LifeFacts LifeRoll(const CrewFile& cf, void* ch) {
  std::vector<LifeFacts> taken;
  for (const CrewFile& o : g_crewFiles) if (o.id != cf.id && !o.facts.empty()) taken.push_back(o.facts);
  LifeFacts f = RollFacts(cf.id, cf.female, cf.name, CrewTraitWords(ch), taken, LifeTabs());
  Log("crew lives: %s: from %s, %s, carries %s, kin %s", cf.name.c_str(), f.home.c_str(), f.job.c_str(), f.keep.c_str(), f.kin.c_str());
  return f;
}
static CrewFile* CrewFileById(uint32_t id);
static std::string NickOf(const CrewFile& f) { for (auto& n : f.nicks) if (n.state == 'k') return n.nick; return ""; }
static bool LostAtSea(const LifeFate* f) { return f && (f->kind == 'm' || f->kind == 's'); }   // never "went into the water ... and came back"
// The closest mate on file among those served with in this man's last 3 battles (a mate who left the crew long ago is
// not "closest aboard"), or, with lostOk, lost ones too. Points into f.bonds.
static const BondTally* BondClosestOf(const CrewFile& f, bool lostOk) {
  int last = f.entries.empty() ? 0 : f.entries.back().battle;
  std::set<uint32_t> ok;
  for (const BondTally& b : f.bonds) if (const CrewFile* o = CrewFileById(b.other)) if (b.last + 3 >= last || (lostOk && o->fate.battle)) ok.insert(b.other);
  return ClosestBond(f.bonds, &ok);
}
static LifeService LifeServiceOf(const CrewFile& f) {
  LifeService s;
  if (const BondTally* b = BondClosestOf(f, false)) if (const CrewFile* o = CrewFileById(b->other))
    if (!f.fate.battle || !o->fate.battle || o->fate.battle >= f.fate.battle) {   // a mate who died first is not named as at his side
      s.friendName = o->name; s.friendWhy = BondWhy(*b, LostAtSea(&f.fate) || LostAtSea(&o->fate));
    }
  for (const CrewFileEntry& e : f.entries) {
    if (!s.first) s.first = e.battle;
    s.last = e.battle; s.battles++; s.kills += e.kills;
    if (!f.fate.battle || e.battle != f.fate.battle) { s.wounds += e.wounds; s.downs += e.downs; s.over += e.over; }   // what they came through
  }
  s.nick = NickOf(f); s.nickWhy = NickWhy(s.nick);
  if (f.station) if (const CrewFile* h = CrewFileById(f.station)) s.heir = h->name;
  return s;
}
static CrewFile* CrewFileById(uint32_t id) { for (CrewFile& f : g_crewFiles) if (f.id == id) return &f; return nullptr; }
static BondLedger BondsOnFile() { BondLedger l; for (const CrewFile& f : g_crewFiles) if (!f.bonds.empty()) l[f.id] = f.bonds; return l; }
static int LettersDue(int battle) {                                    // lost crew of that battle with no signed letter
  CrewFilesEnsure(); int n = 0;
  for (const CrewFile& f : g_crewFiles) if (battle > 0 && f.fate.battle == battle && !f.letter) n++;
  return n;
}
// Nickname offers (crew_nick.h) on the after-battle report: ids of files with an offer waiting, newest battle first, 3
// at most. Built the same way on every IMGUI event; a click looks the file up again by id.
static std::vector<uint32_t> AarNickOffers() {
  CrewFilesEnsure();                                                   // first: LettersDue below must not re-read under the list
  std::vector<std::pair<int, uint32_t>> v;
  for (const CrewFile& f : g_crewFiles) for (const NickRec& n : f.nicks) if (n.state == 'p') v.push_back({ n.battle, f.id });
  std::stable_sort(v.begin(), v.end(), [](const std::pair<int, uint32_t>& a, const std::pair<int, uint32_t>& b) { return a.first > b.first; });
  std::vector<uint32_t> ids;
  for (size_t i = 0; i < v.size() && i < 3; i++) ids.push_back(v[i].second);
  return ids;
}
static void AarNickRow(uint32_t id, Rect4 S, float oy, int type, Vec2 mouse) {
  CrewFile* f = CrewFileById(id); NickRec* rec = nullptr;
  if (f) for (NickRec& n : f->nicks) if (n.state == 'p') rec = &n;
  if (!rec) return;
  if (type == 7) {
    FillC({ S.x + 52, oy - 4, S.w - 104, 1 }, Hex(kInk, 0.25f));        // a light rule above each offer: apart from the report and each other
    Text({ S.x + 52, oy + 4, S.w - 104 - 200, 20 }, Upper(f->name) + ": \"" + Upper(rec->nick) + "\"", 13, kInkHex, kLeft, true);
    Text({ S.x + 52, oy + 24, S.w - 104 - 200, 20 }, std::string("Earned a name, for ") + NickWhy(rec->nick) + ".", 11, kMutedHex, kLeft);
  }
  bool keep = InkButton({ S.x + S.w - 52 - 190, oy + 8, 90, 32 }, "KEEP", type, mouse, true);
  bool veto = InkButton({ S.x + S.w - 52 - 90, oy + 8, 90, 32 }, "VETO", type, mouse, false);
  if (!keep && !veto) return;
  rec->state = keep ? 'k' : 'v';
  g_crewFilesSeq++;
  bool ok = CrewFilesWrite();
  Log("crew nicknames: %s \"%s\" %s%s", f->name.c_str(), rec->nick.c_str(), keep ? "kept" : "vetoed", ok ? "" : "; crew-files.ini could NOT be written");
}
// The letter window: one letter per lost crew member of a battle, paged; the truth or the kind version is one choice
// for all of them, and SIGN ALL signs every one, so a sinking is one click and never ten.
static std::vector<uint32_t> g_letterIds; static size_t g_letterIdx = 0; static bool g_letterTruth = true;
static float g_letterScroll = 0; static std::map<uint32_t, int> g_letterPick;
static void LetterOpen(int battle, uint32_t first, int act) {
  CrewFilesEnsure();
  g_letterIds.clear();
  for (const CrewFile& f : g_crewFiles) if (battle > 0 && f.fate.battle == battle && !f.letter) g_letterIds.push_back(f.id);
  if (g_letterIds.empty()) return;
  g_letterIdx = 0;
  for (size_t k = 0; k < g_letterIds.size(); k++) if (g_letterIds[k] == first) g_letterIdx = k;
  g_letterOpen = true; g_letterAct = act; g_letterScroll = 0;
  if (act == 9) g_letterOffered = battle;                              // opened by hand: DONE does not offer it again
  Log("crew lives: letter window open, %d letter(s) for battle %d", (int)g_letterIds.size(), battle);
}
static int LetterPickOf(const CrewFile& f) { auto it = g_letterPick.find(f.id); return it != g_letterPick.end() ? it->second : (int)(f.id % 7); }
static std::vector<std::string> LetterDraftOf(const CrewFile& f, bool truth, int pick) {
  return DraftLetter(f.name, f.female, f.id, f.facts, LifeServiceOf(f), f.fate, truth, pick, LifeTabs());
}
static void LetterClear(CrewFile& f) { f.letter = 0; f.truth = true; f.pick = 0; f.letterText.clear(); f.mail.returned = false; }   // a new loss: the next letter comes back
static void LetterSign(CrewFile& f) {
  f.letter = 1; f.truth = g_letterTruth || f.fate.kind == 'c'; f.pick = LetterPickOf(f); f.letterText = LetterDraftOf(f, f.truth, f.pick);
}

static void EliteTags();
static void SafeCrewMarks();
static void SafeCoopTick();
static void TryInstallFemale();
static void* g_fLastAct = nullptr; static bool g_lastActTried = false;
static int LastActivity() {                                           // CampaignTriggers.lastActivityRaw: 9 = the after-battle screen
  if (!g_lastActTried) {
    g_lastActTried = true;
    void* ct = FindClass("Core.Campaign", "CampaignTriggers");
    g_fLastAct = ct ? il2cpp_class_get_field_from_name(ct, "lastActivityRaw") : nullptr;
    Log("after-action report: CampaignTriggers.lastActivityRaw %s", g_fLastAct ? "found" : "MISSING; no report window");
  }
  int v = -1; if (g_fLastAct) il2cpp_field_static_get_value(g_fLastAct, &v); return v;
}
// While the report is open the stock screen under it must not take the clicks (IMGUI's Event.Use does not reach uGUI):
// the EventSystem is switched off, and back on when the window closes or the screen changes.
static void *g_aarEvSys = nullptr, *g_aarEvSysPin = nullptr;           // the EventSystem we switched off (pinned)
static void AarBlockStock(bool block) {
  static void *mCur = nullptr, *mSetEn = nullptr; static bool tried = false;
  if (!tried) {
    tried = true;
    void* es = FindClass("UnityEngine.EventSystems", "EventSystem"); void* b = FindClass("UnityEngine", "Behaviour");
    mCur = es ? FindMethod(es, "get_current", 0) : nullptr; mSetEn = b ? FindMethod(b, "set_enabled", 1) : nullptr;
    Log("after-action report: stock clicks %s while the report is open", mCur && mSetEn ? "blocked" : "NOT blocked (EventSystem lookup failed)");
  }
  if (!mSetEn) return;
  if (block && !g_aarEvSys) {
    void* es = mCur ? Invoke(mCur, nullptr) : nullptr;
    if (Alive(es)) { bool v = false; void* a[1] = { &v }; Invoke(mSetEn, es, a); g_aarEvSys = es; g_aarEvSysPin = Pin(es); }
  } else if (!block && g_aarEvSys) {
    void* es = g_aarEvSys;
    if (Alive(es)) { bool v = true; void* a[1] = { &v }; Invoke(mSetEn, es, a); }
    Unpin(g_aarEvSysPin); g_aarEvSysPin = nullptr; g_aarEvSys = nullptr;
  }
}
static void AfterActionGui() {
  static int lastAct = -2; static int shownSeq = 0;
  int act = LastActivity();
  if (act != lastAct) { Log("after-action report: screen %d -> %d (drawn via %s, report of %d paragraph(s))", lastAct, act, g_guiDrawHost >= 0 ? g_guiHosts[g_guiDrawHost].cls : "none", (int)g_aarParas.size()); lastAct = act; }
  // shortcut: raw may stay 9 after quitting to title from this screen; guard if seen
  if (act != 9) { g_aarOpen = false; g_mailOpen = false; if (g_letterAct == 9) g_letterOpen = false; AarBlockStock(g_fileOpen || g_letterOpen); return; }   // the crew files window may hold the block on its screen
  int type = 0; Vec2 mouse = {};
  void* ev = ModGuiBegin(&type, &mouse); if (!ev) return;
  if (!g_storiesOn) { g_aarOpen = false; shownSeq = g_aarSeq; }        // crew_stories=0: the report stays shut, and is not owed when switched back on
  if (!g_lettersOn) { g_letterOpen = g_letterAct == 9 ? false : g_letterOpen; g_mailOpen = false; g_mailUnseen = false; }
  if (shownSeq != g_aarSeq && !g_aarParas.empty()) { shownSeq = g_aarSeq; g_aarOpen = true; g_optOpen = g_newsOpen = false; g_aarScroll = 0; }
  bool menus = ModMenus(ev, type, mouse);
  if (!menus && g_storiesOn && !g_aarParas.empty() && PaperTag({ g_virtW - 40 - 170, 24, 170, 26 }, "REPORT", type, mouse)) { g_aarOpen = true; g_optOpen = g_newsOpen = false; g_aarScroll = 0; }
  int bn = g_crewBattleNoShown, dueAll = g_lettersOn ? LettersDue(bn) : 0, due = menus ? 0 : dueAll;
  bool mail = g_lettersOn && bn > 0 && g_mailBattle == bn && !g_mailNow.empty();
  if (due && PaperTag({ g_virtW - 40 - 170, 24 + 34, 170, 26 }, due == 1 ? "LETTER HOME" : "LETTERS HOME", type, mouse)) LetterOpen(bn, 0, 9);
  if (!menus && mail && PaperTag({ g_virtW - 40 - 170, 24.0f + (due ? 68.0f : 34.0f), 170, 26 }, "MAIL CALL", type, mouse)) { g_mailOpen = true; g_mailUnseen = false; g_mailScroll = 0; }
  // once the report has been read: the letters home (once per battle), then the mail (once)
  if (!menus && !g_aarOpen && !g_letterOpen && !g_mailOpen && !g_fileOpen && (shownSeq == g_aarSeq || g_aarParas.empty())) {
    if (dueAll && g_letterOffered != bn) LetterOpen(bn, 0, 9);
    else if (mail && g_mailUnseen) { g_mailOpen = true; g_mailUnseen = false; g_mailScroll = 0; }
  }
  ModGuiEnd();
  AarBlockStock(g_aarOpen || g_fileOpen || g_letterOpen || g_mailOpen);
}
static void SafeAfterActionGui() {
  static bool off = false; if (off) return;
  __try { AfterActionGui(); }
  __except (FaultFilter("after-action report", GetExceptionInformation())) { off = true; g_aarOff = true; g_aarOpen = false; g_mailOpen = false; if (g_letterAct == 9) g_letterOpen = false; GuiScale(1.0f); GuiThemeEnd(); }
  if (off) __try { AarBlockStock(g_fileOpen || g_letterOpen); } __except (EXCEPTION_EXECUTE_HANDLER) {}   // never leave the stock screen dead after a fault (the crew files window may hold it)
}
// ---- the crew files window on the crew screen (CampaignTriggers.lastActivityRaw 2) ----
// A CREW FILES tag top right opens a sheet: the whole roster on the left (the living first, then the dead, then crew on
// file who are no longer on the roster), the picked crew member's file on the right: portrait, status, lifetime
// tallies and each battle's report paragraphs, newest first. It opens on the crew member the stock card showed last.
// The roster is CampaignState._current.characterRoster: groups of characters; the dead sit in groups whose id ends in
// the game's DEAD mark (scanDead). Every object is rebuilt on the screen's Cancel, so nothing is kept by pointer.
static uint32_t g_cabSel = 0;
static float g_cabListScroll = 0, g_cabBodyScroll = 0;
struct CabRow { uint32_t id = 0; std::string name, status; bool gone = false; void* ch = nullptr; int file = -1; };
static std::vector<CabRow> g_cabRows;                                  // rebuilt at each Layout event, so one frame's events agree
static bool g_cabLost = false; static int g_cabLostN = 0;              // THE LOST tab; how many are lost
static bool g_cabBuilt = false, g_cabFlipped = false;                 // rows are current; the tab was switched to the selection
static int g_cabOffPortrait = -1;
static void CabRoster() {
  g_cabRows.clear(); CrewFilesEnsure();
  static const std::string kDead = "\xC2\xB4" "DEAD" "\xC2\xB4";      // CharacterRoster's mark, U+00B4 both sides
  static int oRoster = -2, oGroups = -1, oChars = -1, oTarget = -1, oHealth = -1, oCraft = -1;
  void* cur = nullptr; if (g_fCampaignCurrent) il2cpp_field_static_get_value(g_fCampaignCurrent, &cur);
  if (cur && oRoster == -2) oRoster = FieldOffset(il2cpp_object_get_class(cur), "characterRoster");
  void* roster = cur && oRoster > 0 ? *(void**)((char*)cur + oRoster) : nullptr;
  if (roster && oGroups < 0) oGroups = FieldOffset(il2cpp_object_get_class(roster), "groups");
  void* groups = roster && oGroups > 0 ? *(void**)((char*)roster + oGroups) : nullptr;
  std::set<uint32_t> seen;
  for (int g = 0, gn = ListCount(groups); g < gn && g < 64; g++) {
    void* grp = ListAt(groups, g); if (!grp) continue;
    if (oChars < 0) { void* k = il2cpp_object_get_class(grp); oChars = FieldOffset(k, "characters"); oTarget = FieldOffset(k, "targetID"); }
    std::string target = oTarget > 0 ? FemUtf8(*(void**)((char*)grp + oTarget)) : std::string();
    void* chars = oChars > 0 ? *(void**)((char*)grp + oChars) : nullptr;
    for (int i = 0, n = ListCount(chars); i < n && i < 200; i++) {
      void* ch = ListAt(chars, i); uint32_t id = CrewIdOf(ch);
      if (!id || !seen.insert(id).second) continue;
      if (oHealth < 0) { void* k = il2cpp_object_get_class(ch); oHealth = FieldOffset(k, "healthState"); oCraft = FieldOffset(k, "designatedCraftID"); g_cabOffPortrait = FieldOffset(k, "portrait"); }
      int hs = oHealth > 0 ? *(int*)((char*)ch + oHealth) : 0;
      std::string craft = oCraft > 0 ? FemUtf8(*(void**)((char*)ch + oCraft)) : std::string();
      CabRow r; r.id = id; r.ch = ch; r.name = FemGShown(ch);
      r.gone = target.find(kDead) != std::string::npos || craft.find(kDead) != std::string::npos;
      r.status = r.gone ? (hs == 4 ? "Captured" : hs == 3 ? "Lost at sea" : "Killed in action")
                        : hs == 1 ? "Wounded" : hs == 3 ? "Missing" : hs == 4 ? "Captured" : hs == 5 ? "Recovered" : "Fit for duty";
      if (!r.gone && target.find("RESERVE") != std::string::npos) r.status += ", in reserve";
      g_cabRows.push_back(r);
    }
  }
  for (const CrewFile& f : g_crewFiles)                                // on file, no longer on the roster (traded, another boat's)
    if (seen.insert(f.id).second) { CabRow r; r.id = f.id; r.name = f.name; r.gone = true; r.status = "No longer with the crew"; g_cabRows.push_back(r); }
  for (CabRow& r : g_cabRows) {
    for (size_t k = 0; k < g_crewFiles.size(); k++) if (g_crewFiles[k].id == r.id) r.file = (int)k;
    if (r.name.empty()) r.name = r.file >= 0 && !g_crewFiles[r.file].name.empty() ? g_crewFiles[r.file].name : "Unnamed";
  }
  g_cabRows.erase(std::remove_if(g_cabRows.begin(), g_cabRows.end(), [](const CabRow& r) {   // only crew with a battle on file
    return r.file < 0 || g_crewFiles[r.file].entries.empty(); }), g_cabRows.end());
  bool changed = false;                                                // older files: facts (with traits while the roster has them), a fate
  for (CabRow& r : g_cabRows) {
    CrewFile& f = g_crewFiles[r.file];
    static std::set<uint32_t> tried;                                   // a life.ini whose rows never fit: once a session, not every frame
    if (f.facts.empty() && (r.gone || !r.ch || CrewTraitsLoaded(r.ch)) && tried.insert(f.id).second) { f.facts = LifeRoll(f, r.ch); changed = !f.facts.empty(); }   // else at the next battle
    if (f.voice.empty() && (r.gone || !r.ch || CrewTraitsLoaded(r.ch))) { f.voice = RollVoice(f.id, CrewTraitWords(r.ch)); changed = true; }
    if (!r.gone && (f.fate.kind == 'm' || f.fate.kind == 'c') && f.fate.battle && r.status.rfind("Missing", 0) != 0 && r.status.rfind("Captured", 0) != 0) {
      Log("crew lives: %s is back (%s); no longer listed lost in battle %d", f.name.c_str(), r.status.c_str(), f.fate.battle);
      f.fate = LifeFate(); LetterClear(f); changed = true;               // missing or captured, and back on the roster
    }
    if (r.gone && r.status != "No longer with the crew" && !f.fate.battle) {
      f.fate.battle = f.entries.back().battle; f.fate.kind = r.status == "Captured" ? 'c' : r.status == "Lost at sea" ? 'm' : 'k'; changed = true;
      Log("crew lives: %s was lost before the mod kept fates; listed as lost in battle %d", f.name.c_str(), f.fate.battle);
      f.mail.returned = true;                                          // long gone: no letter comes back for him now
    }
  }
  if (changed) { g_crewFilesSeq++; if (!CrewFilesWrite()) Log("crew lives: crew-files.ini could NOT be written"); }
  g_cabLostN = 0;
  for (const CabRow& r : g_cabRows) if (g_crewFiles[r.file].fate.battle) g_cabLostN++;
  g_cabRows.erase(std::remove_if(g_cabRows.begin(), g_cabRows.end(), [](const CabRow& r) {   // the CREW and THE LOST tabs
    return (g_crewFiles[r.file].fate.battle != 0) != g_cabLost; }), g_cabRows.end());
  std::stable_partition(g_cabRows.begin(), g_cabRows.end(), [](const CabRow& r) { return !r.gone; });
}
// The crew member on the stock card (CharacterCardInformation.instance.lastCharacter), and whether that card's screen
// is up: lastActivityRaw stays 2 after the screen closes while a new campaign is being set up.
static void *g_cabCardF = nullptr, *g_cabGoActive = nullptr; static int g_cabOffLast = -1; static bool g_cabCardTried = false;
static void* CabCard() {
  if (!g_cabCardTried) {
    g_cabCardTried = true;
    void* k = FindClass("Core.CharacterManagement", "CharacterCardInformation"); void* go = FindClass("UnityEngine", "GameObject");
    g_cabCardF = k ? il2cpp_class_get_field_from_name(k, "instance") : nullptr; g_cabOffLast = FieldOffset(k, "lastCharacter");
    g_cabGoActive = go ? FindMethod(go, "get_activeInHierarchy", 0) : nullptr;
    Log("crew files: crew card %s (instance %p, lastCharacter +0x%X, activeInHierarchy %p)", g_cabCardF && g_cabOffLast > 0 && g_cabGoActive && m_getGameObject ? "found" : "NOT found; the window opens on the first crew member",
        g_cabCardF, g_cabOffLast, g_cabGoActive);
  }
  void* inst = nullptr; if (g_cabCardF) il2cpp_field_static_get_value(g_cabCardF, &inst);
  return Alive(inst) ? inst : nullptr;
}
static bool CabScreenUp(int act) {
  if (act != 2) return false;
  void* inst = CabCard();
  if (!g_cabCardF || !g_cabGoActive || !m_getGameObject) return true;   // no card to check: the screen value alone
  void* go = inst ? Invoke(m_getGameObject, inst) : nullptr; void* r = Alive(go) ? Invoke(g_cabGoActive, go) : nullptr;
  return r && *(bool*)((char*)r + 0x10);
}
static void* CabCardCharacter() { void* inst = CabCard(); return inst && g_cabOffLast > 0 ? *(void**)((char*)inst + g_cabOffLast) : nullptr; }
static std::string CabTimes(int n, const char* what) { return std::string(what) + (n == 1 ? " once" : n == 2 ? " twice" : " " + std::to_string(n) + " times"); }
static const std::vector<AarLine>& CabLines(int width, int file) {     // wrapped once per width, file and change
  static std::vector<AarLine> lines; static int madeW = -1, madeSeq = -1; static uint32_t madeId = 0xFFFFFFFF; static bool madeLetters = true;
  uint32_t id = file >= 0 ? g_crewFiles[file].id : 0;
  if (madeW == width && madeSeq == g_crewFilesSeq && madeId == id && madeLetters == g_lettersOn) return lines;
  madeW = width; madeSeq = g_crewFilesSeq; madeId = id; madeLetters = g_lettersOn; lines.clear();
  if (file < 0) return lines;
  const CrewFile& cf = g_crewFiles[file];
  auto block = [&](const std::string& head, const std::vector<std::string>& ps, bool tight) {
    if (ps.empty()) return;
    if (!lines.empty()) lines.push_back({ "", 0, true });
    lines.push_back({ head, 1, false });
    for (size_t i = 0; i < ps.size(); i++) {
      if (i && !(tight && i + 1 == ps.size())) lines.push_back({ "", 0, true });   // the signature sits under "Sincerely,"
      std::vector<std::string> w; WrapChars(ps[i], (size_t)(width / 7.3f), "", &w);
      for (std::string& t : w) lines.push_back({ t, 0, false });
    }
  };
  if (cf.fate.battle) {
    block(cf.fate.kind == 'c' ? "PRISONER OF WAR" : "IN MEMORY", MemorialText(cf.name, cf.female, cf.id, cf.facts, LifeServiceOf(cf), cf.fate, LifeTabs()), false);
    if (!g_lettersOn) {}                                               // crew_letters=0: no letters in the files either
    else if (cf.letter) block(std::string("LETTER HOME, SIGNED") + (cf.fate.kind == 'c' ? "" : cf.truth ? " (THE TRUTH)" : " (TO SPARE THEM)"), cf.letterText, true);
    else block("LETTER HOME", { "Not written yet: WRITE THE LETTER below drafts it from this file." }, false);
  } else {
    std::vector<std::string> ps = LifeFileText(cf.name, cf.female, cf.facts, LifeTabs());
    if (!cf.voice.empty() && !ps.empty()) ps.push_back("Aboard: " + VoiceTitle(cf.voice) + ".");
    if (!NickOf(cf).empty()) ps.push_back("Known aboard as \"" + NickOf(cf) + "\", for " + NickWhy(NickOf(cf)) + ".");
    if (const BondTally* b = BondClosestOf(cf, true)) if (const CrewFile* o = CrewFileById(b->other))
      ps.push_back("Closest aboard: " + o->name + (o->fate.battle ? (o->fate.kind == 'c' ? " (taken prisoner in battle " : " (lost in battle ") + std::to_string(o->fate.battle) + ")" : "") +
                   ". They " + BondWhy(*b, LostAtSea(&o->fate)) + ".");
    block("PERSONAL", ps, false);
  }
  if (g_lettersOn && !cf.mail.got.empty()) {                           // newest first: a bold line (style 2) and the letter under it
    if (!lines.empty()) lines.push_back({ "", 0, true });
    lines.push_back({ cf.mail.got.size() == 1 ? "A LETTER FROM HOME" : "LETTERS FROM HOME", 1, false });
    for (size_t i = cf.mail.got.size(); i-- > 0;) {
      const MailLetter& m = cf.mail.got[i];
      std::string from = MailFrom(m, cf.female); if (!from.empty()) from[0] = (char)toupper((unsigned char)from[0]);
      if (i + 1 != cf.mail.got.size()) lines.push_back({ "", 0, true });
      std::vector<std::string> w;
      WrapChars("After battle " + std::to_string(m.battle) + ", from " + MailFrom(m, cf.female) + (m.kind ? std::string(". ") + MailStamp(m.kind) : std::string()), (size_t)(width / 7.6f), "", &w);
      for (std::string& t : w) lines.push_back({ t, m.kind ? 3 : 2, false });
      w.clear(); WrapChars(m.dear + " " + m.body + " " + m.sign, (size_t)(width / 7.3f), "", &w);
      for (std::string& t : w) lines.push_back({ t, 0, false });
    }
  }
  const std::vector<CrewFileEntry>& es = g_crewFiles[file].entries;
  for (size_t k = es.size(); k-- > 0;) {
    const CrewFileEntry& e = es[k];
    if (!lines.empty()) lines.push_back({ "", 0, true });
    std::string h = "BATTLE " + std::to_string(e.battle) + "  -  " + e.when + "  -  " + e.len + (e.role.empty() ? "" : "  -  " + Upper(e.role));
    lines.push_back({ h, 1, false });
    std::vector<std::string> ps = e.paras;
    if (ps.empty()) ps.push_back("Aboard for this one: " + e.outcome + ".");
    for (size_t i = 0; i < ps.size(); i++) {
      if (i) lines.push_back({ "", 0, true });
      std::vector<std::string> w; WrapChars(ps[i], (size_t)(width / 7.3f), "", &w);
      for (std::string& t : w) lines.push_back({ t, 0, false });
    }
  }
  return lines;
}
static void CabWindow(void* ev, int type, Vec2 mouse) {
  if (type == 8 || !g_cabBuilt) { CabRoster(); g_cabBuilt = true; }   // Layout comes first in a frame
  {                                                                    // the card's crew member is on the other tab: go there once
    bool here = false; for (const CabRow& r : g_cabRows) if (r.id == g_cabSel) here = true;
    const CrewFile* sf = here ? nullptr : CrewFileById(g_cabSel);
    if (sf && !sf->entries.empty() && (sf->fate.battle != 0) != g_cabLost && !g_cabFlipped) { g_cabLost = !g_cabLost; g_cabFlipped = true; CabRoster(); }
  }
  Rect4 S = OptPanel(960, 620);
  Rect4 tabs = { S.x + 40, S.y + 86, 250, 28 };
  Rect4 list = { S.x + 40, S.y + 86 + 38, 250, S.h - 86 - 38 - 58 - 12 };
  Rect4 body = { list.x + list.w + 30, S.y + 86, S.x + S.w - 52 - (list.x + list.w + 30), list.h + 38 };
  Rect4 text = { body.x, body.y + 158, body.w - 12, body.h - 158 };
  int sel = -1;
  for (size_t k = 0; k < g_cabRows.size(); k++) if (g_cabRows[k].id == g_cabSel) sel = (int)k;
  if (sel < 0 && !g_cabRows.empty()) { sel = 0; g_cabSel = g_cabRows[0].id; }
  const CabRow* row = sel >= 0 ? &g_cabRows[sel] : nullptr;
  int file = row ? row->file : -1;
  const float rowH = 44;                                               // the status line clips at 38
  float listMax = (std::max)(0.0f, g_cabRows.size() * rowH - list.h);
  const std::vector<AarLine>& lines = CabLines((int)text.w, file);
  auto lineH = [](const AarLine& l) { return l.gap ? 10.0f : l.style == 1 ? 28.0f : 20.0f; };
  float total = 0; for (const AarLine& l : lines) total += lineH(l);
  float bodyMax = (std::max)(0.0f, total - text.h);
  if (type == 6 && m_evDelta && Inside(mouse, S)) {                     // ScrollWheel: the pane under the mouse
    void* d = Invoke(m_evDelta, ev); float dy = d ? ((float*)((char*)d + 0x10))[1] : 0;
    if (Inside(mouse, list)) g_cabListScroll += dy * 20; else g_cabBodyScroll += dy * 20;
    if (m_evUse) Invoke(m_evUse, ev);
  }
  g_cabListScroll = (std::min)((std::max)(g_cabListScroll, 0.0f), listMax);
  g_cabBodyScroll = (std::min)((std::max)(g_cabBodyScroll, 0.0f), bodyMax);
  if (type == 7) PaperSheet(S);
  if (ManualHeader(S, Spaced("CREW FILES"), type, mouse)) g_fileOpen = false;
  {                                                                    // the CREW and THE LOST tabs (rows rebuilt next Layout)
    Rect4 a = { tabs.x, tabs.y, 112, tabs.h }, b = { tabs.x + 122, tabs.y, tabs.w - 122, tabs.h };
    if (InkButton(a, "CREW", type, mouse, !g_cabLost) && g_cabLost) { g_cabLost = false; g_cabRows.clear(); g_cabBuilt = false; g_cabFlipped = true; g_cabListScroll = g_cabBodyScroll = 0; }
    if (InkButton(b, "THE LOST (" + std::to_string(g_cabLostN) + ")", type, mouse, g_cabLost) && !g_cabLost) { g_cabLost = true; g_cabRows.clear(); g_cabBuilt = false; g_cabFlipped = true; g_cabListScroll = g_cabBodyScroll = 0; }
  }
  for (size_t k = 0; k < g_cabRows.size(); k++) {                      // rows wholly inside the list only: HitBoxes stay off the header
    Rect4 r = { list.x, list.y + k * rowH - g_cabListScroll, list.w, rowH - 4 };
    if (r.y < list.y - 0.5f || r.y + r.h > list.y + list.h + 0.5f) continue;
    const CabRow& c = g_cabRows[k];
    if (HitBox(r)) { g_cabSel = c.id; g_cabBodyScroll = 0; }
    if (type != 7) continue;
    if ((int)k == sel || Inside(mouse, r)) FillC(r, Hilite());
    Text({ r.x + 6, r.y + 1, r.w - 50, 20 }, c.name, 13, c.gone ? kMutedHex : kInkHex, kLeft, true);
    int lostIn = c.file >= 0 ? g_crewFiles[c.file].fate.battle : 0;
    Text({ r.x + 6, r.y + 19, r.w - 50, 20 }, lostIn ? c.status + ", battle " + std::to_string(lostIn) : c.status, 11, c.gone && c.status != "No longer with the crew" ? kRedHex : kMutedHex, kLeft);
    int nb = c.file >= 0 ? (int)g_crewFiles[c.file].entries.size() : 0;
    if (nb) Text({ r.x + r.w - 46, r.y + 6, 40, 20 }, std::to_string(nb), 12, kMutedHex, kRight);
  }
  float fy = S.y + S.h - 58;
  if (type == 7) {
    FillC({ list.x + list.w + 14, list.y, 1, list.h }, Hex(kInk, 0.4f));
    if (listMax > 0) { float bh = list.h * list.h / (g_cabRows.size() * rowH), by = list.y + (list.h - bh) * (g_cabListScroll / listMax);
      FillC({ list.x + list.w + 4, by, 3, bh }, Hex(kInk, 0.6f)); }
    if (g_cabRows.empty()) Text(list, g_cabLost ? "Nobody lost. Long may it last." : "Nobody on file yet: each battle adds the crew who fought it.", 13, kMutedHex, kUpperLeft);
    if (row) {
      Rect4 ph = { body.x, body.y, 112, 140 };
      void* rt = row->ch && !row->gone && g_cabOffPortrait > 0 ? *(void**)((char*)row->ch + g_cabOffPortrait) : nullptr;   // a dead man's portrait is released
      if (Alive(rt)) { GuiColor({ 1, 1, 1, 1 }); DrawTex(ph, rt); }
      else { FillC(ph, Hex(kCard)); Text(ph, "NO PHOTO", 11, kMutedHex, kCenter); }
      Frame(ph, 2, Hex(kInk));
      float tx = ph.x + ph.w + 18, tw = body.w - ph.w - 18;
      Text({ tx, body.y - 2, tw, 28 }, Upper(row->file >= 0 ? crew_report_detail::NickName(row->name, NickOf(g_crewFiles[row->file])) : row->name), 20, kInkHex, kLeft, true);
      Text({ tx, body.y + 26, tw, 20 }, row->status, 13, row->gone && row->status != "No longer with the crew" ? kRedHex : kInkHex, kLeft);
      int kills = 0, wounds = 0, downs = 0, over = 0, dmg = 0, nb = 0;
      if (file >= 0) for (const CrewFileEntry& e : g_crewFiles[file].entries) { nb++; kills += e.kills; wounds += e.wounds; downs += e.downs; over += e.over; dmg += e.dmg; }
      std::string t1 = nb ? std::to_string(nb) + (nb == 1 ? " battle on file" : " battles on file") + ", " + std::to_string(kills) + (kills == 1 ? " kill" : " kills") : "No battles on file yet.";
      std::vector<std::string> t2;
      if (wounds) t2.push_back(CabTimes(wounds, "wounded")); if (downs) t2.push_back(CabTimes(downs, "downed")); if (over) t2.push_back(CabTimes(over, "overboard"));
      if (dmg) t2.push_back(std::to_string(dmg) + " HP lost");
      std::string s2; for (size_t i = 0; i < t2.size(); i++) s2 += (i ? (i + 1 == t2.size() ? " and " : ", ") : "") + t2[i];
      if (!s2.empty()) s2[0] = (char)toupper((unsigned char)s2[0]);
      Text({ tx, body.y + 54, tw, 20 }, t1, 12, kMutedHex, kLeft);
      if (!s2.empty()) Text({ tx, body.y + 74, tw, 20 }, s2 + ".", 12, kMutedHex, kLeft);
      if (row->gone && row->status != "No longer with the crew") {        // a stamp across the corner of the photo
        Rect4 st = { ph.x + 8, ph.y + ph.h - 40, ph.w - 16, 28 };
        Frame(st, 3, Hex(kStampRed, 0.85f)); Text(st, Spaced(row->status == "Killed in action" ? "KIA" : row->status == "Lost at sea" ? "LOST" : "CAPTURED"), 12, kRedHex, kCenter, true);
      }
      FillC({ body.x, body.y + 148, body.w, 1 }, Hex(kInk, 0.5f));
      void* g[3] = { &text, g_guiContentNone, g_guiStyleNone };
      Invoke(m_guiBeginGroup, nullptr, g);
      float y = -g_cabBodyScroll;
      for (const AarLine& l : lines) {
        float h = lineH(l);
        if (!l.gap && y + h > 0 && y < text.h) {
          if (l.style == 1) { Text({ 0, y + 3, text.w, 24 }, l.text, 12, kInkHex, kLeft, true); FillC({ 0, y + 25, text.w, 1 }, Hex(kInk, 0.3f)); }
          else if (l.style == 2 || l.style == 3) Text({ 0, y, text.w, h }, l.text, 12, l.style == 3 ? kRedHex : kInkHex, kLeft, true);   // a letter's line; red: returned
          else Text({ 0, y, text.w, h }, l.text, 13, kInkHex, kLeft);
        }
        y += h;
      }
      Invoke(m_guiEndGroup, nullptr);
      if (lines.empty()) Text(text, row->gone ? "Nothing on file: lost before the mod kept files." : "Nothing on file yet. Each battle adds its report here.", 13, kMutedHex, kUpperLeft);
      if (bodyMax > 0) { float bh = text.h * text.h / total, by = text.y + (text.h - bh) * (g_cabBodyScroll / bodyMax), bx = text.x + text.w + 9;
        FillC({ bx, text.y, 1, text.h }, Hex(kInk, 0.3f)); FillC({ bx - 1, by, 3, bh }, Hex(kInk)); }
    }
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    Text({ S.x + 52, fy + 2, 520, 56 }, "Each battle's report goes into the crew member's file. Kept with every save.", 11, kMutedHex, kLeft);
  }
  if (g_lettersOn && file >= 0 && g_crewFiles[file].fate.battle && !g_crewFiles[file].letter &&
      InkButton({ S.x + S.w - 52 - 84 - 14 - 170, fy + 14, 170, 32 }, "WRITE THE LETTER", type, mouse, false))
    LetterOpen(g_crewFiles[file].fate.battle, g_crewFiles[file].id, 2);
  if (InkButton({ S.x + S.w - 52 - 84, fy + 14, 84, 32 }, "DONE", type, mouse, true)) g_fileOpen = false;
}
// ---- the mail call window: this battle's letters from home ----
static void MailWindow(void* ev, int type, Vec2 mouse) {
  if (g_mailNow.empty()) { g_mailOpen = false; return; }
  Rect4 S = OptPanel(860, 640), body = { S.x + 52, S.y + 86, S.w - 104 - 12, S.h - 86 - 58 - 12 };
  static std::vector<AarLine> lines; static int madeW = -1, madeSeq = -1;   // style 3: the red returned stamp
  if (madeW != (int)body.w || madeSeq != g_mailSeq) {
    madeW = (int)body.w; madeSeq = g_mailSeq; lines.clear();
    auto wrap = [&](const std::string& s, int style) { std::vector<std::string> w; WrapChars(s, (size_t)(body.w / (style ? 8.0f : 7.3f)), "", &w); for (std::string& t : w) lines.push_back({ t, style, false }); };
    for (const MailShown& m : g_mailNow) {
      if (!lines.empty()) { lines.push_back({ "", 0, true }); lines.push_back({ "", 0, true }); }
      wrap("FOR " + Upper(crew_life_detail::Surname(m.name)) + ", FROM " + Upper(MailFrom(m.l, m.female)), 1);
      if (m.l.kind) wrap(MailStamp(m.l.kind), 3);
      lines.push_back({ m.l.dear, 0, false });
      wrap(m.l.body, 0);
      lines.push_back({ m.l.sign, 0, false });
    }
  }
  auto lineH = [](const AarLine& l) { return l.gap ? 10.0f : l.style == 1 ? 28.0f : 20.0f; };
  float total = 0; for (const AarLine& l : lines) total += lineH(l);
  float maxScroll = (std::max)(0.0f, total - body.h);
  if (type == 6 && m_evDelta && Inside(mouse, S)) {
    void* d = Invoke(m_evDelta, ev); float dy = d ? ((float*)((char*)d + 0x10))[1] : 0;
    g_mailScroll += dy * 20; if (m_evUse) Invoke(m_evUse, ev);
  }
  g_mailScroll = (std::min)((std::max)(g_mailScroll, 0.0f), maxScroll);
  if (type == 7) PaperSheet(S);
  if (ManualHeader(S, Spaced("MAIL CALL"), type, mouse)) g_mailOpen = false;
  float fy = S.y + S.h - 58;
  if (type == 7) {
    void* g[3] = { &body, g_guiContentNone, g_guiStyleNone };
    Invoke(m_guiBeginGroup, nullptr, g);
    float y = -g_mailScroll;
    for (const AarLine& l : lines) {
      float h = lineH(l);
      if (!l.gap && y + h > 0 && y < body.h) {
        if (l.style == 1) { Text({ 0, y + 3, body.w, 24 }, l.text, 13, kInkHex, kLeft, true); FillC({ 0, y + 25, body.w, 1 }, Hex(kInk, 0.4f)); }
        else if (l.style == 3) Text({ 0, y, body.w, h }, l.text, 12, kRedHex, kLeft, true);
        else Text({ 0, y, body.w, h }, l.text, 13, kInkHex, kLeft);
      }
      y += h;
    }
    Invoke(m_guiEndGroup, nullptr);
    if (maxScroll > 0) { float bh = body.h * body.h / total, by = body.y + (body.h - bh) * (g_mailScroll / maxScroll), bx = body.x + body.w + 9;
      FillC({ bx, body.y, 1, body.h }, Hex(kInk, 0.3f)); FillC({ bx - 1, by, 3, bh }, Hex(kInk)); }
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    Text({ S.x + 52, fy + 2, 480, 56 }, "Every letter is kept in the crew member's file.", 11, kMutedHex, kLeft);
  }
  if (InkButton({ S.x + S.w - 52 - 84, fy + 14, 84, 32 }, "DONE", type, mouse, true)) g_mailOpen = false;
}
// ---- the letter window ----
static void LetterWindow(void* ev, int type, Vec2 mouse) {
  while (g_letterIdx < g_letterIds.size()) {                           // drop letters signed elsewhere
    CrewFile* x = CrewFileById(g_letterIds[g_letterIdx]);
    if (x && !x->letter && x->fate.battle) break;
    g_letterIds.erase(g_letterIds.begin() + g_letterIdx);
    if (g_letterIdx >= g_letterIds.size() && g_letterIdx) g_letterIdx--;
  }
  if (g_letterIds.empty()) { g_letterOpen = false; return; }
  CrewFile& f = *CrewFileById(g_letterIds[g_letterIdx]);
  int pick = LetterPickOf(f), nLines = LetterLineCount(f.facts, LifeServiceOf(f), f.fate, LifeTabs());
  bool both = f.fate.kind != 'c';
  Rect4 S = OptPanel(820, 640);
  Rect4 body = { S.x + 52, S.y + 126, S.w - 104 - 12, S.h - 126 - 120 };
  static std::vector<AarLine> lines; static uint32_t madeId = 0; static int madePick = -1, madeSeq = -1, madeW = -1; static bool madeTruth = false;
  if (madeId != f.id || madePick != pick || madeTruth != g_letterTruth || madeSeq != g_crewFilesSeq || madeW != (int)body.w) {
    madeId = f.id; madePick = pick; madeTruth = g_letterTruth; madeSeq = g_crewFilesSeq; madeW = (int)body.w; lines.clear();
    std::vector<std::string> ps = LetterDraftOf(f, g_letterTruth || !both, pick);
    for (size_t i = 0; i < ps.size(); i++) {
      if (i && i + 1 != ps.size()) lines.push_back({ "", 0, true });
      std::vector<std::string> w; WrapChars(ps[i], (size_t)(body.w / 7.3f), "", &w);
      for (std::string& t : w) lines.push_back({ t, 0, false });
    }
    g_letterScroll = 0;
  }
  auto lineH = [](const AarLine& l) { return l.gap ? 10.0f : 20.0f; };
  float total = 0; for (const AarLine& l : lines) total += lineH(l);
  float maxScroll = (std::max)(0.0f, total - body.h);
  if (type == 6 && m_evDelta && Inside(mouse, S)) {
    void* d = Invoke(m_evDelta, ev); float dy = d ? ((float*)((char*)d + 0x10))[1] : 0;
    g_letterScroll += dy * 20; if (m_evUse) Invoke(m_evUse, ev);
  }
  g_letterScroll = (std::min)((std::max)(g_letterScroll, 0.0f), maxScroll);
  size_t n = g_letterIds.size();
  if (type == 7) PaperSheet(S);
  if (ManualHeader(S, Spaced(n > 1 ? "LETTERS HOME" : "LETTER HOME"), type, mouse)) g_letterOpen = false;
  std::string kinFull = f.facts.kinFirst.empty() ? std::string("the family") : f.facts.kinFirst + " " + crew_life_detail::Surname(f.name);
  std::string kinRel = f.facts.kin.empty() ? std::string() : crew_life_detail::KinRel(f.facts.kin, f.female);
  if (type == 7) {
    Text({ S.x + 52, S.y + 84, S.w - 104 - 120, 22 }, "To " + kinFull + (kinRel.empty() ? "" : ", " + Upper(kinRel.substr(0, 1)) + kinRel.substr(1)) + " of " + f.name, 13, kInkHex, kLeft, true);
    Text({ S.x + 52, S.y + 104, S.w - 104, 18 }, f.fate.kind == 'c' ? "Taken prisoner." : f.fate.kind == 'm' ? "Missing in action." : f.fate.kind == 'w' ? "Died of wounds." :
         f.fate.kind == 's' ? "Lost with the boat." : "Killed in action.", 11, kMutedHex, kLeft);
    if (n > 1) Text({ S.x + S.w - 52 - 120, S.y + 84, 120, 22 }, std::to_string(g_letterIdx + 1) + " of " + std::to_string(n), 12, kMutedHex, kRight);
    void* g[3] = { &body, g_guiContentNone, g_guiStyleNone };
    Invoke(m_guiBeginGroup, nullptr, g);
    float y = -g_letterScroll;
    for (const AarLine& l : lines) { float h = lineH(l); if (!l.gap && y + h > 0 && y < body.h) Text({ 0, y, body.w, h }, l.text, 13, kInkHex, kLeft); y += h; }
    Invoke(m_guiEndGroup, nullptr);
    if (maxScroll > 0) { float bh = body.h * body.h / total, by = body.y + (body.h - bh) * (g_letterScroll / maxScroll), bx = body.x + body.w + 9;
      FillC({ bx, body.y, 1, body.h }, Hex(kInk, 0.3f)); FillC({ bx - 1, by, 3, bh }, Hex(kInk)); }
  }
  float cy = S.y + S.h - 110, fy = S.y + S.h - 58;
  if (both) {                                                          // one choice for every letter in the window
    if (type == 7) Text({ S.x + 52, cy - 18, 300, 18 }, n > 1 ? "How it happened, in every letter:" : "How it happened:", 11, kMutedHex, kLeft);
    if (InkButton({ S.x + 52, cy, 130, 32 }, "THE TRUTH", type, mouse, g_letterTruth)) g_letterTruth = true;
    if (InkButton({ S.x + 52 + 140, cy, 130, 32 }, "SPARE THEM", type, mouse, !g_letterTruth)) g_letterTruth = false;
  }
  if (nLines > 1 && InkButton({ S.x + 52 + 300, cy, 150, 32 }, "OTHER LINES", type, mouse, false)) g_letterPick[f.id] = pick + 1;
  if (n > 1) {
    if (InkButton({ S.x + S.w - 52 - 2 * 84 - 10, cy, 84, 32 }, "< PREV", type, mouse, false)) g_letterIdx = (g_letterIdx + n - 1) % n;
    if (InkButton({ S.x + S.w - 52 - 84, cy, 84, 32 }, "NEXT >", type, mouse, false)) g_letterIdx = (g_letterIdx + 1) % n;
  }
  if (type == 7) {
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    Text({ S.x + 52, fy + 2, 340, 56 }, "Signed letters are kept in the crew files.", 11, kMutedHex, kLeft);
  }
  Rect4 sign = { S.x + S.w - 52 - 150, fy + 14, 150, 32 }, later = { sign.x - 14 - 84, fy + 14, 84, 32 };
  if (InkButton(later, "LATER", type, mouse, false)) g_letterOpen = false;
  if (InkButton(sign, n > 1 ? "SIGN ALL (" + std::to_string(n) + ")" : "SIGN", type, mouse, true)) {
    for (uint32_t id : g_letterIds) if (CrewFile* x = CrewFileById(id)) if (!x->letter) LetterSign(*x);
    g_crewFilesSeq++;
    bool ok = CrewFilesWrite();
    Log("crew lives: %d letter(s) signed (%s)%s", (int)n, g_letterTruth ? "the truth" : "to spare them", ok ? "" : "; crew-files.ini could NOT be written");
    g_letterIds.clear(); g_letterOpen = false;
  }
}
static void FileCabinetGui() {
  static int wasUp = -1;
  int act = LastActivity(); bool up = CabScreenUp(act);
  if ((int)up != wasUp) {
    wasUp = up;
    if (up) { g_optOpen = g_newsOpen = false; CrewFilesEnsure(); Log("crew files: crew screen up (drawn via %s, %d crew file(s), %d battle(s) on file)", g_guiDrawHost >= 0 ? g_guiHosts[g_guiDrawHost].cls : "none", (int)g_crewFiles.size(), g_crewBattleNo); }
  }
  if (!up || g_diagFrames > 0) { g_fileOpen = false; if (g_letterAct == 2) g_letterOpen = false; g_cabRows.clear(); g_cabBuilt = false; AarBlockStock(g_aarOpen || g_letterOpen || g_mailOpen); return; }   // never two ModMenus passes in a frame
  int type = 0; Vec2 mouse = {};
  void* ev = ModGuiBegin(&type, &mouse); if (!ev) return;
  if (!ModMenus(ev, type, mouse) && PaperTag({ g_virtW - 40 - 170, 24, 170, 26 }, "CREW FILES", type, mouse)) {
    g_fileOpen = true; g_optOpen = g_newsOpen = g_aarOpen = false;
    g_cabSel = CrewIdOf(CabCardCharacter()); g_cabListScroll = g_cabBodyScroll = 0; g_cabRows.clear(); g_cabBuilt = false; g_cabFlipped = false;
    CrewFilesEnsure(); const CrewFile* cardFile = CrewFileById(g_cabSel); g_cabLost = cardFile && cardFile->fate.battle;   // the card's tab (corrected once fates update)
  }
  ModGuiEnd();
  AarBlockStock(g_aarOpen || g_fileOpen || g_letterOpen || g_mailOpen);
}
static void SafeFileCabinetGui() {
  static bool off = false; if (off) return;
  __try { FileCabinetGui(); }
  __except (FaultFilter("crew files window", GetExceptionInformation())) { off = true; g_fileOff = true; g_fileOpen = false; g_letterOpen = false; GuiScale(1.0f); GuiThemeEnd(); }
  if (off) __try { AarBlockStock(g_aarOpen || g_letterOpen || g_mailOpen); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
