// crew battle log, speech bubbles, the talking crew (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- crew battle log ----------------------------------------------------------------------------------------------
// What happened to each crew member of the player's boat in a battle, reported at the battle change
// (Log and BoatCrewMod/crew-reports.txt); log only, nothing in the game changes.
// Crew damage is ONE HP pool; every hit goes through CrewMember.hitFlash(true, d), told apart by return address:
// ProjectileManager.endJobs (rva 0x9F5B22, shell or bullet) and SplashDamageManager.FixedUpdate (rva 0x983815, blast).
// The blast frame holds the splash copy: killer [rsp+0xB8], entityCode [rsp+0xC0] (-999 = fire), teamCode [rsp+0xC4].
// Hits of 3 HP or less raise no wound event in the game ('grazes' here). Downs, deaths and overboard are polled from
// CrewCharacter (HP +0x5C, healthState: 0 Healthy, 1 Wounded, 2 Dead, 3 Missing (overboard), 4 Captured, 5 Recovered).
// Own boat only: who applies crew damage in co-op is not traced.
struct CrewRec {
  void* ch = nullptr; void* pin = nullptr; std::string name, role, gun;
  void* member = nullptr; float sayAt = -100;                          // the CrewMember last seen (speech bubbles), the last line they said
  uint64_t firstGun = 0;                                               // the mount of their first job, if a gun (bonds: the same gun, not one of a name)
  float maxHp = 0, lastHp = 0, dmg = 0; int state = -1, hits = 0, shells = 0, blasts = 0, burns = 0, other = 0, grazes = 0, hitsDown = 0, kills = 0, shared = 0, downs = 0, overboards = 0, lines = 0;
  bool down = false, seenAboard = false, wounded = false; std::vector<std::string> log;
  int pjCode = 0; size_t pjLine = (size_t)-1; float pjLost = 0, pjAt = 0; std::string pjWhat; bool pjWound = false;
  std::vector<RepEv> ev; std::vector<RepGraze> grazeEv; size_t pjEv = (size_t)-1;   // the prose report's events (crew_report.h)
  std::string firstRole, stableRole, roleCand; float roleCandAt = 0; int roleEvs = 0;   // the last round's line, for its burst
  struct Dmg { float hp = 0, first = -1, last = 0; int n = 0; std::set<std::string> guns; };
  std::map<std::string, Dmg> dmgBy;                                   // HP lost to each enemy (grazes included)
};
static std::unordered_map<void*, std::string> g_crewEliteName;          // elite names seen this battle
static std::set<std::string> g_crewSrcSeen;                            // hit sources already described in the log
static std::vector<CrewRec> g_crewRecs;                               // this battle's crew, in the order first seen
static std::unordered_map<void*, size_t> g_crewRecOf;                 // CrewCharacter -> index
static bool g_crewLogOn = false; static int g_crewLogFaults = 0, g_crewLogLines = 0; static float g_crewLogStart = -1; static DWORD g_crewLogPollAt = 0;
static int g_oChHP = -1, g_oCmRole = -1, g_oChSinking = -1; static void* m_chMaxHealth = nullptr;
static int g_oPjCode = -1, g_oCmFeet = -1, g_oCmChestP = -1, g_oCmHeadP = -1, g_oPjVelo = -1, g_oTlEntity = -1, g_crewGeoLogs = 0; static void *m_crewTrFwd = nullptr, *m_crewTrRight = nullptr, *g_clsTorpLauncher = nullptr;
static float CrewLogNow() {                                           // game seconds (Time.time); 0 if missing
  static void* mTime = nullptr; static bool tried = false;
  if (!tried) { tried = true; void* tm = FindClass("UnityEngine", "Time"); mTime = tm ? FindMethod(tm, "get_time", 0) : nullptr; }
  void* r = mTime ? Invoke(mTime, nullptr) : nullptr;
  return r ? *(float*)((char*)r + 0x10) : 0.0f;
}
static std::string CrewLogClock() {
  if (g_crewLogStart < 0) g_crewLogStart = CrewLogNow();
  int s = (int)(CrewLogNow() - g_crewLogStart); if (s < 0) s = 0;
  char b[16]; snprintf(b, sizeof b, "%02d:%02d", s / 60, s % 60); return b;
}
static float CrewEvT() { if (g_crewLogStart < 0) g_crewLogStart = CrewLogNow(); float t = CrewLogNow() - g_crewLogStart; return t < 0 ? 0 : t; }
static void TalkOnEvent(CrewRec& r, int kind);
static RepEv& CrewEv(CrewRec& r, int kind) {                         // a prose-report event (capped: the report folds the rest)
  static RepEv spare;
  TalkOnEvent(r, kind);                                                // the talking crew hear about it first (no RepEv held across it)
  if (r.ev.size() >= 400) { spare = RepEv(); return spare; }
  r.ev.emplace_back(); RepEv& e = r.ev.back(); e.t = CrewEvT(); e.kind = kind; return e;
}
static const char* CrewRoleText(int r) {
  switch (r) { case 1: return "repairing"; case 2: return "tending the wounded"; case 3: return "passing ammunition"; case 4: return "on a gun"; case 5: return "at the helm"; default: return "standing by"; }
}
static void CrewNote(CrewRec& r, const char* fmt, ...) {
  char b[400]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  std::string line = CrewLogClock() + "  " + b;
  if (r.log.size() < 60) r.log.push_back(line);
  r.lines++;
  if (g_crewLogLines++ < 12) Log("crew log: %s: %s", r.name.c_str(), line.c_str()); else DLog("crew log: %s: %s", r.name.c_str(), line.c_str());
}
// ---- crew speech bubbles ----
// CrewMember.headText (CrewMemberText) is the bubble over a crew member's head. Its enqueueText(string) (0x1809B86B0)
// switches the bubble on, sets a random delay and queues the text; Update shows it (triggerText) for 1.25-1.75 s +
// 0.06 s a letter, with no distance or state check. At most one line a man every 20 s and one a boat every 4 s.
static int g_saySaid = 0, g_sayFail = 0; static float g_sayLastAny = -100; static std::set<void*> g_sayKills;
// debug=1: a queued line is checked again 1 s later (what the bubble shows, whether it is on, distance to the camera).
struct SayProbe { void* head; void* pin; std::string name, text; float at; };
static std::vector<SayProbe> g_sayProbes; static bool g_sayOpened = false; static int g_sayProbed = 0;
static bool CrewSayBlocked(const CrewRec& r, bool priority) {         // the 20 s a man and 4 s a boat limits
  float now = CrewLogNow(); return now - r.sayAt < 20 || (!priority && now - g_sayLastAny < 4);
}
static bool CrewSay(CrewRec& r, const char* why, const std::string& text, bool force = false, bool priority = false) {   // true: queued
  static int offHead = -2; static void* mEnq = nullptr;
  if (text.empty() || !r.member || !Alive(r.member)) return false;
  float now = CrewLogNow();
  if (!force && CrewSayBlocked(r, priority)) { DLog("crew bubbles: %s would say \"%s\" (%s) but spoke too recently", r.name.c_str(), text.c_str(), why); return false; }
  if (offHead == -2) offHead = FieldOffset(il2cpp_object_get_class(r.member), "headText");
  void* head = offHead > 0 ? *(void**)((char*)r.member + offHead) : nullptr;
  if (!mEnq && Alive(head)) {
    mEnq = FindMethod(il2cpp_object_get_class(head), "enqueueText", 1);
    Log("crew bubbles: CrewMember.headText +0x%X, CrewMemberText.enqueueText %s", offHead, mEnq ? "found" : "NOT found; no bubbles");
  }
  if (!mEnq || !Alive(head)) {
    if (++g_sayFail <= 3) Log("crew bubbles: %s: no bubble (%s)", r.name.c_str(), offHead <= 0 ? "no headText field" : !head ? "no head text" : !Alive(head) ? "head text destroyed" : "no enqueueText");
    return false;
  }
  void* a[1] = { il2cpp_string_new(text.c_str()) }; void* exc = nullptr;
  il2cpp_runtime_invoke(mEnq, head, a, &exc);                         // Invoke would hide a throw: enqueueText returns nothing
  if (exc) { if (++g_sayFail <= 3) Log("crew bubbles: %s: enqueueText threw", r.name.c_str()); return false; }
  r.sayAt = now; g_sayLastAny = now; g_saySaid++;
  if (g_saySaid <= 12 || g_debug) Log("crew bubbles: %s says \"%s\" (%s, at %s)", r.name.c_str(), text.c_str(), why, CrewLogClock().c_str());
  if (g_debug && g_sayProbes.size() < 2 && g_sayProbed < 2) g_sayProbes.push_back({ head, Pin(head), r.name, text, now });   // the 1 s check, twice a battle
  return true;
}
static std::string SayPick(const CrewRec& r, std::initializer_list<const char*> lines) {
  size_t n = lines.size(), k = (size_t)((uintptr_t)r.ch / 16 + (size_t)g_saySaid * 7) % n;
  return *(lines.begin() + k);
}
// ---- the talking crew (crew_talk.h) ----
// Lines through CrewSay. Events (CrewEv): a man wounded speaks; a crewmate downed, killed or thrown over the side, or a
// fire, makes someone else speak. ~6 s in: the opening (a roll call after losses the boat's crew saw, a word about the
// dead, or a battle-start line). Lulls (nothing hit or killed for 25 s): small talk, home, letters, the dead, a returned
// letter, often with a reply. g_talkChatter scales every chance; each voice has its own rate (VoiceRate).
static const TalkTables& TalkTabs() {                                  // talk.ini replaces built-in sections; read once a session
  static TalkTables t; static bool done = false;
  if (!done) {
    done = true;
    FILE* f = fopen((g_folder + "\\talk.default.ini").c_str(), "wb"); if (f) { fputs(TalkDefaultIni(), f); fclose(f); }
    std::string text = CrewReadText(g_folder + "\\talk.ini");
    if (!text.empty()) { t = ParseTalkIni(text); Log("crew talk: talk.ini has %d section(s); each replaces the built-in one", (int)t.size()); }
  }
  return t;
}
struct TalkStep { size_t rec; std::string text; float at; const char* why; size_t to; float said; };   // to: who a reply answers (SIZE_MAX: none); said: the replier's sayAt then
static std::vector<TalkStep> g_talkQ;
static std::set<std::string> g_talkUsed;
static std::map<uint32_t, std::string> g_talkVoiceOf;                  // crew not on file yet: the voice rolled this session
static std::vector<size_t> g_talkDeadNow;                              // crew records killed this battle
static float g_talkNextQuiet = 0; static bool g_talkOpened = false, g_talkFire = false, g_talkReturned = false, g_talkStarted = false;
static float g_talkNextCombat = 0, g_talkRakedAt = -100; static std::vector<float> g_talkGrazes;   // battle chatter; a raking's splinters
static std::map<std::string, int> g_talkCount;                         // lines said this battle, by event
static uint32_t g_talkSeed = 1;
static TalkCrew TalkCrewOf(CrewRec& r) {
  TalkCrew c; c.id = CrewIdOf(r.ch); c.name = r.name; c.female = r.ch && FemGGet(r.ch) == 2;
  CrewFile* f = c.id ? CrewFileById(c.id) : nullptr;
  if (f) { c.facts = &f->facts; c.fate = &f->fate; c.mail = &f->mail; c.voice = f->voice; c.nick = NickOf(*f); }
  if (c.voice.empty()) {
    auto it = g_talkVoiceOf.find(c.id);
    c.voice = it != g_talkVoiceOf.end() ? it->second : (g_talkVoiceOf[c.id] = RollVoice(c.id, CrewTraitWords(r.ch)));
  }
  return c;
}
static bool CanTalk(const CrewRec& r) {
  if (!r.member || !Alive(r.member) || r.down || !r.ch) return false;
  int st = g_oChHealth >= 0 ? *(int*)((char*)r.ch + g_oChHealth) : r.state;   // read live: the record lags a tick
  float hp = g_oChHP >= 0 ? *(float*)((char*)r.ch + g_oChHP) : 1;
  return (st == 0 || st == 1) && hp > 0;
}
static float TalkRand() { g_talkSeed = g_talkSeed * 1664525u + 1013904223u; return (g_talkSeed >> 8) / 16777216.0f; }
// someone else who can talk, weighted by how talkative they are
static CrewRec* TalkOther(const CrewRec* not1, const CrewRec* not2 = nullptr, const TalkCrew* knew = nullptr);
static bool TalkKnew(CrewRec& sp, const TalkCrew& dead);
static CrewRec* TalkOther(const CrewRec* not1, const CrewRec* not2, const TalkCrew* knew) {
  float total = 0; std::vector<std::pair<CrewRec*, float>> c;
  float now = CrewLogNow();
  for (CrewRec& r : g_crewRecs) {
    if (&r == not1 || &r == not2 || !CanTalk(r) || now - r.sayAt < 20) continue;
    if (knew && !TalkKnew(r, *knew)) continue;
    float w = VoiceRate(TalkCrewOf(r).voice) + 0.05f; c.push_back({ &r, w }); total += w;
  }
  float x = TalkRand() * total;
  for (auto& p : c) { if (x < p.second) return p.first; x -= p.second; }
  return c.empty() ? nullptr : c.back().first;
}
static bool OnGun(const std::string& role) { return crew_report_detail::RoleCat(role) == crew_report_detail::RC_GUN; }   // [gun] / [deadgun] rows
static bool TalkSay(CrewRec& sp, const char* event, const TalkCtx& ctx, float chance, bool force = false, bool priority = false) {
  if (!g_talkOn || !CanTalk(sp)) return false;
  if (!force && CrewSayBlocked(sp, priority)) return false;            // before a line is used up
  TalkCrew me = TalkCrewOf(sp);
  if (chance < 1 && TalkRand() > chance * (float)g_talkChatter * VoiceRate(me.voice)) return false;
  TalkCtx c = ctx; TalkCrew mate;                                     // {mate}: someone else aboard, when the moment names nobody
  c.speakerGun = OnGun(sp.role);
  if (!c.mate) if (CrewRec* o = TalkOther(&sp)) { mate = TalkCrewOf(*o); c.mate = &mate; }
  std::string line = TalkLine(event, me, c, (uint32_t)(TalkRand() * 4e9f), TalkTabs(), &g_talkUsed);
  if (!CrewSay(sp, event, line, force, priority)) { g_talkUsed.erase(line); return false; }
  g_talkCount[event]++;
  return true;
}
// Bonds in the talk: the ledger as the files had it when the battle began (read once a battle).
static BondLedger g_talkBonds; static bool g_talkBondsRead = false;
static const BondLedger& TalkBonds() { if (!g_talkBondsRead) { CrewFilesEnsure(); g_talkBonds = BondsOnFile(); g_talkBondsRead = true; } return g_talkBonds; }
// The closest mate of crew id `of` who is aboard and can talk now (not `not1`); *why: what they shared (noWater: `of`
// was lost at sea, so not "went into the water together and came back").
static CrewRec* TalkClosest(uint32_t of, const CrewRec* not1, std::string* why, bool noWater = false) {
  auto it = TalkBonds().find(of);
  if (it == TalkBonds().end()) return nullptr;
  std::set<uint32_t> aboard; for (CrewRec& r : g_crewRecs) if (&r != not1 && CanTalk(r)) aboard.insert(CrewIdOf(r.ch));
  const BondTally* b = ClosestBond(it->second, &aboard);
  if (!b) return nullptr;
  for (CrewRec& r : g_crewRecs) if (&r != not1 && CanTalk(r) && CrewIdOf(r.ch) == b->other) { if (why) *why = BondWhy(*b, noWater); return &r; }
  return nullptr;
}
// to: the man this line answers; the answer is dropped if he can no longer talk or sp spoke again before it came up
static void TalkQueue(CrewRec& sp, const std::string& text, float delay, const char* why, const CrewRec* to = nullptr) {
  if (!text.empty()) g_talkQ.push_back({ (size_t)(&sp - g_crewRecs.data()), text, CrewLogNow() + delay, why, to ? (size_t)(to - g_crewRecs.data()) : SIZE_MAX, sp.sayAt });
}
// A raking: four or more splinter hits on the crew within 2 s; someone shouts about it (at most every 20 s).
static void TalkGraze(CrewRec& r) {
  float now = CrewLogNow();
  g_talkGrazes.push_back(now);
  while (!g_talkGrazes.empty() && now - g_talkGrazes.front() > 2) g_talkGrazes.erase(g_talkGrazes.begin());
  if (!g_talkOn || g_talkGrazes.size() < 4 || now - g_talkRakedAt < 20) return;
  CrewRec* sp = TalkOther(&r); if (!sp && CanTalk(r)) sp = &r;
  if (sp && TalkSay(*sp, "raked", TalkCtx(), 1, false, true)) g_talkRakedAt = now;
}
static void TalkOnEvent(CrewRec& r, int kind) {
  float now = CrewLogNow();
  if (kind <= R_OTHER || kind == K_KILL) {                             // shots landing: no lull, and a roll call stops
    g_talkAction = now;
    g_talkQ.erase(std::remove_if(g_talkQ.begin(), g_talkQ.end(), [](const TalkStep& s) { return strncmp(s.why, "rollcall", 8) == 0; }), g_talkQ.end());
  }
  if (!g_talkOn || g_crewRecs.empty()) return;
  if (kind == S_WOUNDED) TalkSay(r, "wounded", TalkCtx(), 0.7f);
  else if (kind == S_DOWN || kind == S_BLEED) {
    TalkCrew who = TalkCrewOf(r); TalkCtx c; c.down = &who;
    CrewRec* m = TalkClosest(who.id, &r, nullptr);                     // the closest mate first, else anyone
    if (!m || !TalkSay(*m, "crew_down", c, 0.95f, false, true))
      if (CrewRec* o = TalkOther(&r, m)) TalkSay(*o, "crew_down", c, 0.7f);
  } else if (kind == S_KILLED || kind == S_DIED_WOUNDS) {
    g_talkDeadNow.push_back((size_t)(&r - g_crewRecs.data()));
    TalkCrew who = TalkCrewOf(r); TalkCtx c; c.dead = &who; c.deadGun = OnGun(!r.stableRole.empty() ? r.stableRole : r.role);   // the job he held, not "down"
    std::string why; CrewRec* m = TalkClosest(who.id, &r, &why);       // a close mate takes it hardest
    bool said = false;
    if (m) { c.why = why; said = TalkSay(*m, "crew_killed_close", c, 0.95f, false, true); if (said) Log("crew bonds: %s lost their closest mate, %s", m->name.c_str(), r.name.c_str()); }
    if (!said) if (CrewRec* o = TalkOther(&r, m)) TalkSay(*o, "crew_killed", c, 0.9f, false, true);
  } else if (kind == S_OVER_HIT || kind == S_OVER_BLAST) {             // not S_SIDE: a sinking puts everyone over at once
    TalkCrew who = TalkCrewOf(r); TalkCtx c; c.down = &who;
    if (CrewRec* o = TalkOther(&r)) TalkSay(*o, TalkRand() < 0.5f ? "man_overboard" : "crew_overboard", c, 0.7f, false, true);
  } else if (kind == R_FIRE && !g_talkFire) {
    CrewRec* sp = CanTalk(r) ? &r : TalkOther(&r);
    if (sp && TalkSay(*sp, "fire", TalkCtx(), 0.8f)) g_talkFire = true; // once said, not just tried
  }
}
// Whether the speaker knew the dead: killed this battle (everyone aboard saw it), or served in the battle they were lost.
static bool TalkFought(const CrewFile* f, int battle) { if (f) for (const CrewFileEntry& e : f->entries) if (e.battle == battle) return true; return false; }
static bool TalkKnew(CrewRec& sp, const TalkCrew& dead) {
  for (size_t i : g_talkDeadNow) if (i < g_crewRecs.size() && CrewIdOf(g_crewRecs[i].ch) == dead.id) return true;
  return dead.fate && dead.fate->battle && TalkFought(CrewFileById(CrewIdOf(sp.ch)), dead.fate->battle);
}
static TalkCrew TalkCrewOfFile(const CrewFile& f) {
  TalkCrew d; d.id = f.id; d.name = f.name; d.female = f.female; d.facts = &f.facts; d.fate = &f.fate; d.mail = &f.mail; d.voice = f.voice; d.nick = NickOf(f); return d;
}
static std::set<uint32_t> TalkAboard() { std::set<uint32_t> a; for (CrewRec& r : g_crewRecs) a.insert(CrewIdOf(r.ch)); return a; }
static float TalkSecs(const std::string& line) { return 1.8f + 0.06f * (float)line.size() + 0.4f; }   // a bubble's time on screen, and a breath
// ~6 s into a battle: a roll call if the boat lost people last battle (said by those who were there; the captured are
// not called), else a word for a man lost in the last two battles from someone who served with him. The battle-start
// line waits for our guns to open up (TalkTick).
static void TalkOpening() {
  g_talkOpened = true;
  if (!g_talkOn || g_crewRecs.empty()) return;
  CrewFilesEnsure();
  int last = g_crewBattleNo;                                           // this battle is not filed yet: the last one is
  std::set<uint32_t> aboard = TalkAboard();                            // anyone aboard today is not lost, whatever the file says
  std::vector<CrewRec*> witness;
  for (CrewRec& r : g_crewRecs) if (CanTalk(r) && TalkFought(CrewFileById(CrewIdOf(r.ch)), last)) witness.push_back(&r);
  std::vector<const CrewFile*> lost, grief;
  for (const CrewFile& f : g_crewFiles) {
    if (!f.fate.battle || f.fate.kind == 'c' || aboard.count(f.id)) continue;
    if (f.fate.battle == last) lost.push_back(&f);
    if (f.fate.battle >= last - 1) grief.push_back(&f);
  }
  if (!lost.empty() && !witness.empty()) {                             // the roll call
    CrewRec* caller = witness[0];
    for (CrewRec* w : witness) { std::string v = TalkCrewOf(*w).voice; if (v == "professional" || v == "stoic") { caller = w; break; } }
    TalkCrew cv = TalkCrewOf(*caller);
    uint32_t seed = (uint32_t)(TalkRand() * 4e9f);
    float t = 0;
    std::string open = TalkLine("rollcall_open", cv, TalkCtx(), seed, TalkTabs(), &g_talkUsed);
    TalkQueue(*caller, open, t, "rollcall_open"); t += TalkSecs(open);
    int answered = 0, byNick = 0;
    for (CrewRec* w : witness) {
      if (w == caller || answered >= 2) continue;
      answered++;
      std::string nk = TalkCrewOf(*w).nick;
      bool useNick = !nk.empty() && TalkRand() < 1.0f / 3;            // the living: by nickname now and then
      if (useNick) byNick++;
      std::string call = (useNick ? nk : crew_life_detail::Surname(w->name)) + "?";
      TalkQueue(*caller, call, t, "rollcall_call"); t += 1.6f;
      std::string here = TalkLine("rollcall_here", TalkCrewOf(*w), TalkCtx(), seed + answered, TalkTabs(), &g_talkUsed);
      TalkQueue(*w, here, t, "rollcall_here"); t += TalkSecs(here);
    }
    int named = 0;
    for (const CrewFile* f : lost) {
      if (named++ >= 5) break;
      std::string nk = NickOf(*f);                                     // the dead: always by the name the crew gave him
      if (!nk.empty()) byNick++;
      std::string call = (nk.empty() ? crew_life_detail::Surname(f->name) : nk) + "?";
      TalkQueue(*caller, call, t, "rollcall_call"); t += 1.6f + 2.4f;   // nobody answers
      TalkCrew dead = TalkCrewOfFile(*f); TalkCtx c; c.dead = &dead; c.deadGun = f->fate.role == crew_report_detail::RC_GUN;
      CrewRec* who = caller;                                           // someone else says it when there is someone
      if (witness.size() > 1) { who = witness[(size_t)(TalkRand() * witness.size()) % witness.size()]; if (who == caller) who = witness[(caller == witness[0]) ? 1 : 0]; }
      std::string why; CrewRec* fr = TalkClosest(f->id, caller, &why, LostAtSea(&f->fate));   // their closest mate, if they were there (never the caller)
      bool close = fr && std::find(witness.begin(), witness.end(), fr) != witness.end();
      if (close) { who = fr; c.why = why; }
      std::string sil = TalkLine(close ? "rollcall_silence_close" : "rollcall_silence", TalkCrewOf(*who), c, seed + 10 + named, TalkTabs(), &g_talkUsed);
      TalkQueue(*who, sil, t, "rollcall_silence"); t += TalkSecs(sil);
    }
    TalkQueue(*caller, TalkLine("rollcall_close", cv, TalkCtx(), seed + 20, TalkTabs(), &g_talkUsed), t, "rollcall_close");
    Log("crew talk: roll call for %d crew lost in battle %d (%d of today's crew were there), %d by nickname", (int)lost.size(), last, (int)witness.size(), byNick);
    return;
  }
  if (!grief.empty() && TalkRand() < 0.8f) {
    TalkCrew dead = TalkCrewOfFile(*grief[(size_t)(TalkRand() * grief.size()) % grief.size()]);
    TalkCtx c; c.dead = &dead; c.deadGun = dead.fate && dead.fate->role == crew_report_detail::RC_GUN;
    std::string why;
    if (CrewRec* fr = TalkClosest(dead.id, nullptr, &why, LostAtSea(dead.fate))) { c.why = why; TalkSay(*fr, "grief_start_close", c, 1, true); }
    else if (CrewRec* sp = TalkOther(nullptr, nullptr, &dead)) TalkSay(*sp, "grief_start", c, 1, true);
  }
}
// Two close mates: a says `open` to b, and b answers from `reply` (a [warm] opener from `warm`) with chance
// answer * crew_chatter. false: a could not speak (force: past the 20 s / 4 s limits, as in a lull).
static bool TalkPair(CrewRec& a, CrewRec& b, const std::string& why, const char* open, const char* reply, const char* warm, float answer, bool force) {
  if (!force && CrewSayBlocked(a, false)) return false;                // before a line is used up
  TalkCrew ac = TalkCrewOf(a), bc = TalkCrewOf(b);
  bool guns = OnGun(a.role) && OnGun(b.role);                          // [gun] partner rows talk to the mate as a gunner too
  TalkCtx ca; ca.mate = &bc; ca.why = why; ca.speakerGun = guns;
  std::string tg, l1 = TalkLine(open, ac, ca, (uint32_t)(TalkRand() * 4e9f), TalkTabs(), &g_talkUsed, &tg);
  if (!CrewSay(a, open, l1, force)) { g_talkUsed.erase(l1); return false; }
  g_talkCount[open]++;
  TalkCtx cb; cb.mate = &ac; cb.why = why; cb.speakerGun = guns;
  const char* rev = tg.find("|warm|") != std::string::npos ? warm : reply;   // a sincere word gets a sincere answer
  if (TalkRand() < answer * (float)g_talkChatter) TalkQueue(b, TalkLine(rev, bc, cb, (uint32_t)(TalkRand() * 4e9f), TalkTabs(), &g_talkUsed), TalkSecs(l1) - 0.6f, rev, &a);
  return true;
}
// A lull: small talk, home, a letter, the dead, a returned letter; often answered.
static void TalkQuiet() {
  CrewFilesEnsure();
  std::vector<CrewRec*> can; for (CrewRec& r : g_crewRecs) if (CanTalk(r)) can.push_back(&r);
  if (can.empty()) return;
  int last = g_crewBattleNo;
  std::set<uint32_t> aboard = TalkAboard();
  std::vector<TalkCrew> dead;                                          // killed today, or lost in the last two battles (not captured, not back)
  for (size_t i : g_talkDeadNow) if (i < g_crewRecs.size()) dead.push_back(TalkCrewOf(g_crewRecs[i]));
  for (const CrewFile& f : g_crewFiles)
    if (f.fate.battle >= last - 1 && f.fate.battle > 0 && f.fate.kind != 'c' && !aboard.count(f.id)) dead.push_back(TalkCrewOfFile(f));
  const MailLetter* back = nullptr; TalkCrew backOf;                    // a letter to one of the dead came back at the last mail call
  if (g_lettersOn && !g_talkReturned && g_mailLast > 0 && g_mailLast >= last)   // crew_letters=0: no talk of letters at all
    for (const CrewFile& f : g_crewFiles) for (const MailLetter& m : f.mail.got)
      if (m.kind == 1 && m.battle == g_mailLast && !aboard.count(f.id)) { back = &m; backOf = TalkCrewOfFile(f); }
  auto knowers = [&](const TalkCrew& d) { std::vector<CrewRec*> k; for (CrewRec* r : can) if (TalkKnew(*r, d)) k.push_back(r); return k; };
  std::vector<std::pair<CrewRec*, int>> letters;
  if (g_lettersOn) for (CrewRec* r : can) { TalkCrew c = TalkCrewOf(*r); int i = TalkLetterToTell(c); if (i >= 0) letters.push_back({ r, i }); }
  const TalkCrew* griefOf = dead.empty() ? nullptr : &dead[(size_t)(TalkRand() * dead.size()) % dead.size()];
  std::vector<CrewRec*> griefBy = griefOf ? knowers(*griefOf) : std::vector<CrewRec*>(), backBy = back ? knowers(backOf) : std::vector<CrewRec*>();
  struct Pair { CrewRec *a, *b; std::string why; };                    // close mates who can both talk, and what they shared
  std::vector<Pair> pairs; std::set<std::pair<uint32_t, uint32_t>> seenPairs;
  for (CrewRec* a : can) {
    std::string wy; CrewRec* b = TalkClosest(CrewIdOf(a->ch), a, &wy);
    if (!b) continue;
    uint32_t ia = CrewIdOf(a->ch), ib = CrewIdOf(b->ch);
    if (seenPairs.insert({ (std::min)(ia, ib), (std::max)(ia, ib) }).second) pairs.push_back({ a, b, wy });
  }
  float w[6] = { griefBy.empty() ? 0.0f : 3.0f, backBy.empty() ? 0.0f : 3.0f, letters.empty() ? 0.0f : 3.0f, 2.0f, 2.0f, pairs.empty() ? 0.0f : 2.5f };   // grief returned letter home quiet partners
  float total = 0; for (float x : w) total += x;
  float x = TalkRand() * total; int kind = 4;
  for (int i = 0; i < 6; i++) { if (x < w[i]) { kind = i; break; } x -= w[i]; }
  if (kind == 5) {                                                     // two close mates: one says something, the other answers
    Pair& p = pairs[(size_t)(TalkRand() * pairs.size()) % pairs.size()];
    CrewRec *a = TalkRand() < 0.5f ? p.a : p.b, *b = a == p.a ? p.b : p.a;
    TalkPair(*a, *b, p.why, "quiet_partner", "reply_partner", "reply_partner_warm", 0.85f, true);
    return;
  }
  CrewRec* sp = nullptr; std::string line; const char* ev = "quiet"; const char* reply = "reply_quiet";
  TalkCtx ctx; TalkCrew mate; bool bad = false;
  uint32_t seed = (uint32_t)(TalkRand() * 4e9f);
  if (kind == 2) {
    auto& p = letters[(size_t)(TalkRand() * letters.size()) % letters.size()];
    sp = p.first; TalkCrew me = TalkCrewOf(*sp);
    line = TalkLetterLine(me, p.second, seed, TalkTabs(), &g_talkUsed, &bad);
    if (CrewFile* f = CrewFileById(me.id)) f->mail.told = p.second + 1; // each letter talked about once
    ev = "letter"; reply = bad ? "reply_letter_bad" : "reply_letter";
  } else {
    std::vector<CrewRec*>& pool = kind == 0 ? griefBy : kind == 1 ? backBy : can;
    float tw = 0; for (CrewRec* r : pool) tw += VoiceRate(TalkCrewOf(*r).voice) + 0.05f;
    float y = TalkRand() * tw; sp = pool.back();
    for (CrewRec* r : pool) { float rw = VoiceRate(TalkCrewOf(*r).voice) + 0.05f; if (y < rw) { sp = r; break; } y -= rw; }
    TalkCrew me = TalkCrewOf(*sp);
    if (kind == 0) {
      ctx.dead = griefOf; ev = "grief"; reply = "reply_grief"; ctx.deadGun = griefOf->fate && griefOf->fate->role == crew_report_detail::RC_GUN;
      CrewRec* heir = nullptr;                                         // someone at the dead man's old gun or job speaks of it, once a battle
      const LifeFate* gf = griefOf->fate;                              // the report's rules: not a gun lost with the boat, a man new to it who knew him
      if (gf && gf->battle && !(gf->sunk && gf->role == crew_report_detail::RC_GUN) && !g_talkCount.count("station_gun") && !g_talkCount.count("station_job"))
        for (CrewRec* r : griefBy) {
          if (!StationRole(!r->stableRole.empty() ? r->stableRole : r->role, *gf)) continue;
          bool old = false;
          if (const CrewFile* hf = CrewFileById(CrewIdOf(r->ch)))
            for (const CrewFileEntry& en : hf->entries) if (en.battle == gf->battle && StationRole(en.role, *gf)) old = true;
          if (!old) { heir = r; break; }
        }
      if (heir && TalkRand() < 0.5f) { sp = heir; me = TalkCrewOf(*sp); ev = griefOf->fate->role == crew_report_detail::RC_GUN ? "station_gun" : "station_job"; }
      else {
        std::string why; CrewRec* fr = TalkClosest(griefOf->id, nullptr, &why, LostAtSea(griefOf->fate));   // their closest mate grieves them most
        if (fr && TalkRand() < 0.7f) { sp = fr; me = TalkCrewOf(*sp); ev = "grief_close"; ctx.why = why; }
      }
    }
    else if (kind == 1) { ctx.dead = &backOf; ctx.letter = back; ev = "letter_returned"; reply = "reply_grief"; g_talkReturned = true; }
    else if (kind == 3) ev = "quiet_home";
    if (CrewRec* o = TalkOther(sp)) { mate = TalkCrewOf(*o); ctx.mate = &mate; }
    line = TalkLine(ev, me, ctx, seed, TalkTabs(), &g_talkUsed);
    if (line.empty() && kind == 3) { ev = "quiet"; line = TalkLine(ev, me, ctx, seed + 1, TalkTabs(), &g_talkUsed); }
  }
  if (!sp || !CrewSay(*sp, ev, line, true)) return;
  g_talkCount[ev]++;
  if (ctx.dead && (!strcmp(ev, "station_gun") || !strcmp(ev, "station_job")))
    Log("crew talk: %s spoke at %s's old station (%s)", sp->name.c_str(), ctx.dead->name.c_str(), ctx.dead->fate && !ctx.dead->fate->gun.empty() ? ctx.dead->fate->gun.c_str() : "his job");
  if (TalkRand() < 0.55f * (float)g_talkChatter) {                     // an answer, from someone who knew the dead when it is about them
    const TalkCrew* about = ctx.dead;
    if (CrewRec* o = TalkOther(sp, nullptr, about)) {
      TalkCtx rc; rc.dead = ctx.dead; rc.letter = ctx.letter; TalkCrew spc = TalkCrewOf(*sp); rc.mate = &spc;
      std::string ans = TalkLine(reply, TalkCrewOf(*o), rc, seed + 7, TalkTabs(), &g_talkUsed);
      TalkQueue(*o, ans, TalkSecs(line) - 0.6f, reply, sp);
    }
  }
}
static void TalkTick() {
  if (!g_talkOn) { g_talkQ.clear(); return; }                          // switched off mid-battle: queued roll calls and replies go too
  float now = CrewLogNow();
  for (size_t i = 0; i < g_talkQ.size();) {                            // queued lines: roll calls and replies
    if (g_talkQ[i].at > now) { i++; continue; }
    TalkStep st = g_talkQ[i]; g_talkQ.erase(g_talkQ.begin() + i);
    if (st.rec >= g_crewRecs.size() || !CanTalk(g_crewRecs[st.rec])) continue;
    if (st.to != SIZE_MAX && (st.to >= g_crewRecs.size() || !CanTalk(g_crewRecs[st.to]) || g_crewRecs[st.rec].sayAt != st.said)) {   // the man answered went down, or a cry came first
      g_talkCount[std::string("dropped ") + st.why]++; continue;
    }
    if (CrewSay(g_crewRecs[st.rec], st.why, st.text, true)) g_talkCount[st.why]++;
  }
  if (!g_talkOn) return;
  float t = CrewEvT();
  for (CrewRec& r : g_crewRecs) if (r.down || r.state == 3) g_talkAction = now;   // nobody chats while a mate is down or in the water
  if (!g_talkOpened && t > 6) TalkOpening();
  if (!g_talkStarted && g_talkShot && g_talkQ.empty()) {               // our guns open up: someone on a gun says so
    g_talkStarted = true;
    std::vector<CrewRec*> guns; for (CrewRec& r : g_crewRecs) if (CanTalk(r) && r.role.rfind("on the ", 0) == 0) guns.push_back(&r);
    CrewRec* sp = guns.empty() ? TalkOther(nullptr) : guns[(size_t)(TalkRand() * guns.size()) % guns.size()];
    if (sp) TalkSay(*sp, "battle_start", TalkCtx(), 1, true);
  }
  // battle chatter: while shots fly, someone speaks every
  // 9-19 s (scaled by crew_chatter), in line with their job, a quarter of the time a line anyone could say
  if (t > 6 && g_talkShot && now - g_talkAction < 8 && now >= g_talkNextCombat && g_talkQ.empty()) {
    g_talkNextCombat = now + (9.0f + 10.0f * TalkRand()) / (float)g_talkChatter;
    std::string wy; CrewRec* sp = TalkOther(nullptr);
    CrewRec* pm = sp && TalkRand() < 0.4f ? TalkClosest(CrewIdOf(sp->ch), sp, &wy) : nullptr;   // close mates talk in battle too
    if (pm && now - pm->sayAt < 20) pm = nullptr;                      // the answer is forced: not on top of the mate's own last line
    if (pm && TalkPair(*sp, *pm, wy, "combat_partner", "reply_combat_partner", "reply_combat_partner_warm", 0.8f, false)) {}
    else if (sp) {
      int rc = crew_report_detail::RoleCat(sp->role);
      const char* ev = TalkRand() < 0.25f ? "combat_any" : rc == crew_report_detail::RC_GUN ? "combat_gun" : rc == crew_report_detail::RC_AMMO ? "combat_ammo" :
                       rc == crew_report_detail::RC_REPAIR ? "combat_repair" : rc == crew_report_detail::RC_MEDIC ? "combat_medic" : rc == crew_report_detail::RC_HELM ? "combat_helm" : "combat_any";
      TalkSay(*sp, ev, TalkCtx(), 1);
    }
  }
  if (g_talkNextQuiet <= 0) g_talkNextQuiet = now + 40;
  if (t > 40 && now - g_talkAction > 25 && now >= g_talkNextQuiet && g_talkQ.empty()) {
    TalkQuiet();
    g_talkNextQuiet = now + (35.0f + 35.0f * TalkRand()) / (float)g_talkChatter;
  }
}
// The last battle's lines ("~" + key, see TalkLine) are kept in talk-last.txt, so a game restart keeps them too.
static std::string TalkLastPath() { return g_folder + "\\talk-last.txt"; }
static void TalkLastKeep() {
  static bool loaded = false;
  if (!loaded) {                                                       // the first battle change of the session
    loaded = true;
    if (FILE* f = fopen(TalkLastPath().c_str(), "r")) {
      char buf[512];
      while (fgets(buf, sizeof buf, f)) { std::string k = buf; while (!k.empty() && (k.back() == '\n' || k.back() == '\r')) k.pop_back(); if (k.size() > 1 && k[0] == '~') g_talkUsed.insert(k); }
      fclose(f);
    }
  }
  bool said = false;                                                   // a battle change with no battle before it says nothing:
  for (auto& k : g_talkUsed) if (k[0] != '~') { said = true; break; }  // keep what the last battle said
  if (!said) return;
  std::set<std::string> last;
  for (auto& k : g_talkUsed) if (k[0] != '~') last.insert("~" + k);
  g_talkUsed.swap(last);
  if (FILE* f = OpenForSave(TalkLastPath())) { for (auto& k : g_talkUsed) fprintf(f, "%s\n", k.c_str()); FinishSave(f, TalkLastPath()); }
}
static void TalkReset() {
  if (!g_talkCount.empty()) {
    std::string s; for (auto& kv : g_talkCount) s += (s.empty() ? "" : ", ") + kv.first + " " + std::to_string(kv.second);
    Log("crew talk, last battle: %s", s.c_str());
  }
  TalkLastKeep();                                                      // last battle's lines: said again only when nothing else fits
  g_talkQ.clear(); g_talkDeadNow.clear(); g_talkCount.clear(); g_talkOurGun.clear();
  g_talkAction = 0; g_talkNextQuiet = 0; g_talkOpened = g_talkFire = g_talkReturned = g_talkStarted = g_talkShot = false;
  g_talkNextCombat = 0; g_talkRakedAt = -100; g_talkGrazes.clear();
  g_talkBonds.clear(); g_talkBondsRead = false;
  g_talkSeed = GetTickCount() | 1;
}
static bool CrewIsOurs(void* member) {
  void* mgr = member && g_oCmMgr >= 0 ? *(void**)((char*)member + g_oCmMgr) : nullptr;
  return mgr && g_gfxCraft && *(void**)((char*)mgr + g_oMgrEnt) == g_gfxCraft;
}
// The record of a crew member of the player's boat (member may be null: then ch only, as the poll finds it)
static CrewRec* CrewRecOf(void* member, void* ch) {
  if (!ch) return nullptr;
  auto f = g_crewRecOf.find(ch);
  CrewRec* r;
  if (f != g_crewRecOf.end()) r = &g_crewRecs[f->second];
  else {
    if (g_crewRecs.size() >= 64) return nullptr;
    g_crewRecOf[ch] = g_crewRecs.size(); g_crewRecs.emplace_back(); r = &g_crewRecs.back();
    r->ch = ch; r->pin = Pin(ch); r->name = GfxCrewName(ch); if (r->name.empty()) r->name = "a crew member";
    r->state = g_oChHealth >= 0 ? *(int*)((char*)ch + g_oChHealth) : -1;
    r->lastHp = g_oChHP >= 0 ? *(float*)((char*)ch + g_oChHP) : 0;
    void* mh = m_chMaxHealth ? Invoke(m_chMaxHealth, ch) : nullptr;
    r->maxHp = mh ? (float)Unbox32(mh) : 0; if (r->maxHp < r->lastHp) r->maxHp = r->lastHp;
    r->down = r->lastHp <= 0 && r->state != 3;
  }
  if (member) {                                                        // what they were doing when last seen
    r->seenAboard = true; r->member = member;
    int role = g_oCmRole >= 0 ? *(int*)((char*)member + g_oCmRole) : 0;
    void* mount = g_offCrewGun >= 0 ? *(void**)((char*)member + g_offCrewGun) : nullptr;
    r->gun = role == 4 && mount ? GfxOf(mount).name : std::string();
    r->role = role == 4 && mount ? "on the " + r->gun : CrewRoleText(role);
    if (r->firstRole.empty() || CrewEvT() < 15) { r->firstRole = r->stableRole = r->role; r->roleCand.clear(); r->firstGun = role == 4 && mount ? (uint64_t)(uintptr_t)mount : 0; }   // the first 15 s: still taking stations
    else if (r->down) r->roleCand.clear();                            // a man who is down has no job; the 10 s start again when he is up
    else if (r->role != r->stableRole) {                               // walking between jobs flickers: only a job held 10 s counts
      float now = CrewEvT();
      if (r->role != r->roleCand) { r->roleCand = r->role; r->roleCandAt = now; }
      else if (now - r->roleCandAt > 10 && r->roleEvs < 6) { r->stableRole = r->role; r->roleEvs++; CrewEv(*r, S_ROLE).part = r->role; }
    } else r->roleCand.clear();
  }
  return r;
}
static void CrewLogInit() {
  void* ch = FindClass("Core", "CrewCharacter"); void* cm = FindClass("Core", "CrewMember");
  g_oChHP = FieldOffset(ch, "HP"); g_oCmRole = FieldOffset(cm, "assignedRole"); g_oChSinking = FieldOffset(ch, "inSinkingBoat");
  m_chMaxHealth = ch ? FindMethod(ch, "maxHealth", 0) : nullptr;
  g_oCmFeet = FieldOffset(cm, "smoothPos"); g_oCmChestP = FieldOffset(cm, "smoothChestPos"); g_oCmHeadP = FieldOffset(cm, "smoothHeadPos");
  g_oPjVelo = FieldOffset(FindClass("Core", "Projectile"), "velocity"); g_oPjCode = FieldOffset(FindClass("Core", "Projectile"), "projectileCode");
  g_clsTorpLauncher = FindClass("Core", "TorpedoLauncher"); g_oTlEntity = FieldOffset(g_clsTorpLauncher, "entity");
  void* tr = FindClass("UnityEngine", "Transform");
  m_crewTrFwd = tr ? FindMethod(tr, "get_forward", 0) : nullptr; m_crewTrRight = tr ? FindMethod(tr, "get_right", 0) : nullptr;
  g_crewLogOn = g_gfxCrewOn && g_oChHP >= 0 && g_oChHealth >= 0 && g_oCmHp >= 0 && g_oCmChar >= 0;
  if (g_crewLogOn) Log("crew log: ready (hits %s, HP +0x%X, role +0x%X, maxHealth %p; body +0x%X/+0x%X/+0x%X, facing %s, shell velocity +0x%X, torpedo launcher entity +0x%X); reports go to crew-reports.txt",
                       g_raCrewDirect ? "by kind" : "kind unknown (game updated?)", g_oChHP, g_oCmRole, m_chMaxHealth, g_oCmFeet, g_oCmChestP, g_oCmHeadP, m_crewTrFwd && m_crewTrRight ? "yes" : "NO", g_oPjVelo, g_oTlEntity);
  else Log("crew log: off (CrewMember.hitFlash detour %d, HP %d, healthState %d)", (int)g_gfxCrewOn, g_oChHP, g_oChHealth);
}
static void CrewLogPre(void* self, CrewHitPre* p) {
  if (!CrewIsOurs(self)) return;
  void* ch = *(void**)((char*)self + g_oCmChar); if (!ch) return;
  p->on = true; p->ch = ch; p->hp = *(float*)((char*)self + g_oCmHp); p->state = *(int*)((char*)ch + g_oChHealth);
}
