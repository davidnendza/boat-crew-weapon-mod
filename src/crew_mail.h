// crew_mail.h: mail call. Every few battles a few crew members get a letter from home. Each follows a thread: one sender's story told across 3-5 letters, one per
// mail call, so the people at home move on between battles. A crew member lost before his next letter arrives still
// gets it, stamped returned to sender. Pure C++17 like crew_life.h; the letters are in mail_threads.h.
#pragma once
#include "crew_life.h"
#include "mail_threads.h"
#include <algorithm>

// ---- interface (weaponmod.cpp and tools/crew_report_test.cpp depend on it) ----------------------------------------
struct MailLetter {
  int battle = 0;                      // the battle it arrived after
  int kind = 0;                        // 0 delivered, 1 returned (deceased), 2 returned (missing), 3 forwarded (prisoner)
  std::string thread, who, sender;     // "mail.mother.dog", "mother", "Ellen"
  int step = 0;                        // 0-based step of the thread
  std::string dear, body, sign;        // "Dear George," / the letter / "Love, Mother" (filled, kept as delivered)
};
struct MailState {
  std::string thread; int step = 0;    // the thread and its next step
  int last = 0;                        // battle of the last letter
  bool returned = false;               // the lost crew member's last letter came back
  std::vector<std::string> used;       // threads finished
  std::string sender;                  // the current thread's sender's name
  std::string facts;                   // family facts the letters so far have set: "dadjob=store;kidbrother=Eddie"
  std::vector<MailLetter> got;         // every letter, oldest first
  int told = 0;                        // the talking crew: letters before this index were talked about already
};
struct MailCrew {                      // one crew file, as mail call sees it
  uint32_t id = 0; std::string name; bool female = false;
  const LifeFacts* facts = nullptr; const LifeFate* fate = nullptr;
  bool fought = false;                 // in this battle (and so aboard for mail call)
  MailState* st = nullptr;
};
struct MailDelivery { size_t crew; MailLetter letter; };
// After battle `battle`: whether mail comes (lastCall: the battle of the last mail call, 0 none) and who gets what.
// Updates the states; returns the letters in the order to show them (returned ones last).
std::vector<MailDelivery> MailCall(int battle, int lastCall, uint32_t seed, std::vector<MailCrew>& crew, const LifeTables& t);
// "his wife", "a girl back home", "an old friend": who a letter is from, as the crew member's file says it
std::string MailFrom(const MailLetter& l, bool female);
const char* MailStamp(int kind);       // "RETURNED TO SENDER: DECEASED" or ""
int MailThreadCount();
// ---- end of interface ----------------------------------------------------------------------------------------------

namespace crew_mail_detail {
using namespace crew_life_detail;
struct Thread { std::string id, who, sign; std::vector<std::string> needs, steps; std::vector<std::pair<std::string, std::string>> keys; int era = 0; };   // era 1 1942-43, 2 1944, 3 1945
inline const std::vector<Thread>& Threads() {
  static const std::vector<Thread> all = [] {
    std::string text; for (const char* p : kMailThreads) text += p;
    std::vector<Thread> v;
    for (auto& kv : crew_report_detail::ParseIniText(text)) {
      if (kv.first.rfind("mail.", 0) != 0) continue;
      Thread th; th.id = kv.first; std::map<int, std::string> steps;
      for (auto& row : kv.second) {
        size_t eq = row.find('='); if (eq == std::string::npos) continue;
        std::string k = crew_report_detail::Trim(row.substr(0, eq)), val = crew_report_detail::Trim(row.substr(eq + 1));
        if (k == "who") th.who = val;
        else if (k == "sign") th.sign = val;
        else if (k == "era") th.era = atoi(val.c_str());
        else if (k == "keys") { size_t a = 0; while (a < val.size()) { size_t c = val.find(',', a); std::string kv = crew_report_detail::Trim(val.substr(a, c == std::string::npos ? std::string::npos : c - a));
            size_t col = kv.find(':'); if (col != std::string::npos) th.keys.push_back({ kv.substr(0, col), kv.substr(col + 1) }); if (c == std::string::npos) break; a = c + 1; } }
        else if (k == "needs") { size_t a = 0; while (a < val.size()) { size_t c = val.find(',', a); std::string n = crew_report_detail::Trim(val.substr(a, c == std::string::npos ? std::string::npos : c - a)); if (!n.empty()) th.needs.push_back(n); if (c == std::string::npos) break; a = c + 1; } }
        else if (!k.empty() && k[0] >= '1' && k[0] <= '9') steps[atoi(k.c_str())] = val;
      }
      for (auto& s : steps) th.steps.push_back(s.second);
      if (!th.who.empty() && !th.steps.empty()) v.push_back(th);
    }
    return v;
  }();
  return all;
}
inline const Thread* FindThread(const std::string& id) { for (auto& th : Threads()) if (th.id == id) return &th; return nullptr; }
// who may write: the next of kin, siblings, a sweetheart for single crew, friends
inline int WhoWeight(const std::string& who, const LifeFacts& f, bool female) {
  const std::string& k = f.kin;
  if (who == "mother") return k == "mother" ? 4 : k == "parents" ? 3 : k == "spouse" ? 1 : 0;
  if (who == "father") return k == "father" ? 4 : k == "parents" ? 2 : k == "spouse" ? 1 : 0;
  if (who == "parents") return k == "parents" ? 3 : 0;
  if (who == "wife") return k == "spouse" && !female ? 5 : 0;
  if (who == "husband") return k == "spouse" && female ? 5 : 0;
  if (who == "grandmother" || who == "aunt" || who == "uncle") return k == who ? 4 : 0;
  if (who == "sister" || who == "brother") return k == who ? 4 : 1;
  if (who == "sweetheart") return k != "spouse" ? 2 : 0;
  if (who == "friend") return 1;
  return 0;
}
inline bool NeedsOk(const Thread& th, const LifeFacts& f, bool female, const LifeTables& t) {
  for (auto& n : th.needs) {
    if (n == "kids" && !f.kids) return false;
    if (n == "nokids" && f.kids) return false;
    if (n == "single" && f.kin == "spouse") return false;
    if (n == "male" && female) return false;
    if (n == "female" && !female) return false;
    if (n == "underage" && SourceOf(t, "job", f.job, nullptr) != "job.underage") return false;
  }
  return true;
}
// The family as the crew member's facts and the letters so far have it: key -> value ("-" = there is none).
// A thread may not start if it says otherwise (a second father's job, a brother with another name, a wife for a single man).
typedef std::map<std::string, std::string> Family;
inline Family ParseFamily(const std::string& s) {
  Family f; size_t a = 0;
  while (a < s.size()) { size_t c = s.find(';', a); std::string kv = s.substr(a, c == std::string::npos ? std::string::npos : c - a);
    size_t eq = kv.find('='); if (eq != std::string::npos) f[kv.substr(0, eq)] = kv.substr(eq + 1); if (c == std::string::npos) break; a = c + 1; }
  return f;
}
inline std::string FamilyText(const Family& f) { std::string s; for (auto& kv : f) s += (s.empty() ? "" : ";") + kv.first + "=" + kv.second; return s; }
inline Family BaseFamily(const LifeFacts& f, bool female) {
  Family m; const std::string& k = f.kin;
  if (k == "spouse") { m[female ? "husband" : "wife"] = f.kinFirst; m["sweetheart"] = "-"; }
  else { m["wife"] = "-"; m["husband"] = "-"; }
  if (k == "mother" || k == "father" || k == "parents") m["parents"] = "alive";
  if (k == "aunt" || k == "uncle" || k == "grandmother" || f.orphan) m["parents"] = "dead";   // why they are next of kin (a sibling may be either)
  if (k == "sister" || k == "brother") m[k] = f.kinFirst;
  if (k == "grandmother") m["grandma"] = "alive";
  m["children"] = f.kids ? "yes" : "no";
  std::string jp = f.job + "|" + f.plan;
  bool farm = f.job.find("family's farm") != std::string::npos;      // a ranch hand's family may live in town
  m["home"] = farm ? "farm" : "town";                                   // the homes are towns and cities: farm letters only for farm jobs
  if (jp.find("father's barbershop") != std::string::npos) { m["dadjob"] = "barber"; m["barber"] = "dad"; }
  if (jp.find("father's fishing boat") != std::string::npos) m["dadjob"] = "fishing";
  if (jp.find("father and grandfather") != std::string::npos) m["dadjob"] = "mine";
  if (jp.find("father's store") != std::string::npos) m["dadjob"] = "store";
  if (f.job.find("family's farm") != std::string::npos) m["dadjob"] = "farm";   // the job he had beats a plan
  m["job"] = f.job.find("machine shop") != std::string::npos ? "shop" : "other";
  if (f.keep.find("class ring") != std::string::npos) m["ring"] = "carried";
  if (f.plan.find("youngest") != std::string::npos || f.keep.find("baby's shoes") != std::string::npos) m["baby"] = "yes";
  if (f.plan.find("first day of school") != std::string::npos) m["daughterage"] = "baby";
  if (f.keep.find("drawing") != std::string::npos && f.keep.find("daughter") != std::string::npos) m["daughterage"] = "school";
  return m;
}
inline bool Needs(const Thread& th, const char* n) { return std::find(th.needs.begin(), th.needs.end(), n) != th.needs.end(); }
inline bool IsRole(const std::string& k) { return k == "brother" || k == "sister" || k == "kidbrother" || k == "kidsister" || k == "sweetheart"; }
// Whether th fits the family (fam: facts and letters; said: what the letters so far set). *sender: its sender's name
// (a sibling the letters already named keeps that name). Each brother, sister or sweetheart tells one story: a second
// would have the 4-F brother write from France, or the girl who married someone else say yes after all.
inline bool FitsFamily(const Thread& th, const Family& fam, const Family& said, std::string* sender) {
  bool born = Needs(th, "nokids");                                     // a baby on the way: the thread brings the children
  if (born && fam.count("children") && fam.at("children") == "yes") return false;
  if (th.era > 0 && said.count("era") && th.era < atoi(said.at("era").c_str())) return false;   // no coffee rationing after V-E Day
  for (auto& kv : th.keys) {
    if (kv.first == "sendersex") continue;
    if (born && (kv.first == "son" || kv.first == "daughter" || kv.first == "children")) continue;
    auto it = fam.find(kv.first);
    if (kv.second == "@sender") {
      if (IsRole(kv.first) && said.count(kv.first)) return false;      // that one's story is told
      if (it == fam.end() || it->second == "yes") continue;            // "yes": exists, not named yet: the rolled name
      if (it->second == "-") return false;
      *sender = it->second;
      continue;
    }
    if (it != fam.end() && it->second != kv.second) return false;      // "-" (none) never matches a name
    if ((kv.first == "son" || kv.first == "daughter") && fam.count("children") && fam.at("children") == "no") return false;
  }
  return true;
}
// The sender's first name: the next of kin writes as himself; anyone else gets a name rolled from the crew id and
// their role (a kid brother and an older one differ; every friend thread is a different friend), never a name the
// family already uses.
inline std::string RollSender(const Thread& th, const MailCrew& c, const Family& fam, const LifeTables& t) {
  const LifeFacts& f = *c.facts; const std::string& k = f.kin; const std::string& who = th.who;
  if (who == "parents") return k == "parents" ? f.kinFirst : "";
  if (k == "parents" && (who == "mother" || who == "father")) {        // "Walter and Ellen": father first
    size_t a = f.kinFirst.find(" and ");
    if (a != std::string::npos) return who == "father" ? f.kinFirst.substr(0, a) : f.kinFirst.substr(a + 5);
  }
  std::string role = who;
  for (auto& kv : th.keys) if (kv.second == "@sender") role = kv.first;
  if (role == k || ((who == "wife" || who == "husband") && k == "spouse")) return f.kinFirst;
  role = who;
  bool fem = who == "mother" || who == "wife" || who == "grandmother" || who == "aunt" || who == "sister" || (who == "sweetheart" && !c.female);
  for (auto& kv : th.keys) {
    if (kv.second == "@sender") { role = kv.first; if (role == "kidsister") fem = true; if (role == "kidbrother") fem = false; }
    if (kv.first == "sendersex") fem = kv.second == "f";
  }
  std::set<std::string> taken = { FirstName(c.name) };
  for (auto& kv : fam) taken.insert(kv.second);
  size_t and_ = f.kinFirst.find(" and ");
  if (and_ != std::string::npos) { taken.insert(f.kinFirst.substr(0, and_)); taken.insert(f.kinFirst.substr(and_ + 5)); } else taken.insert(f.kinFirst);
  uint32_t salt = who == "friend" ? Fnv(th.id) : Fnv(role);
  std::string first;
  for (uint32_t n = 0; n < 16; n++) {
    std::string name = Pick(t, fem ? "name.f" : "name.m", Hash(c.id, salt, 41 + n));
    if (first.empty()) first = name;
    if (!taken.count(name)) return name;
  }
  return first;
}
inline bool StartThread(MailCrew& c, uint32_t salt, const std::set<std::string>& active, const LifeTables& t) {
  Family said = ParseFamily(c.st->facts);
  Family fam = BaseFamily(*c.facts, c.female);
  for (auto& kv : said) fam[kv.first] = kv.second;                    // what earlier letters said wins over nothing
  std::vector<std::pair<const Thread*, int>> cand; std::vector<std::string> names; int total = 0;
  for (int pass = 0; pass < 3 && cand.empty(); pass++)                // pass 1: also threads someone else is on; pass 2: also ones he had
    for (auto& th : Threads()) {
      int w = WhoWeight(th.who, *c.facts, c.female);
      if (!w || !NeedsOk(th, *c.facts, c.female, t)) continue;
      if (pass < 1 && active.count(th.id)) continue;
      if (pass < 2 && std::find(c.st->used.begin(), c.st->used.end(), th.id) != c.st->used.end()) continue;
      if (pass == 2 && std::find(c.st->used.begin(), c.st->used.end(), th.id) != c.st->used.end()) {
        bool role = false; for (auto& kv : th.keys) if (kv.second == "@sender" && IsRole(kv.first)) role = true;
        if (role) continue;                                            // a told story is never told again
      }
      std::string sender = RollSender(th, c, fam, t);
      if (!FitsFamily(th, fam, said, &sender)) continue;
      cand.push_back({ &th, w }); names.push_back(sender); total += w;
    }
  if (cand.empty()) return false;
  int x = (int)(Hash(c.id, salt, (uint32_t)c.st->used.size()) % (uint32_t)total);
  size_t pick = 0;
  for (size_t i = 0; i < cand.size(); i++) { if (x < cand[i].second) { pick = i; break; } x -= cand[i].second; }
  const Thread& th = *cand[pick].first;
  c.st->thread = th.id; c.st->step = 0; c.st->sender = names[pick];
  for (auto& kv : th.keys) if (kv.first != "sendersex") said[kv.first] = kv.second == "@sender" ? names[pick] : kv.second;
  if (!names[pick].empty()) said["n." + th.id.substr(th.id.rfind('.') + 1)] = names[pick];   // taken from now on
  if (th.era > 0 && (!said.count("era") || th.era > atoi(said["era"].c_str()))) said["era"] = std::to_string(th.era);
  c.st->facts = FamilyText(said);
  return true;
}
inline MailLetter Deliver(MailCrew& c, int battle, int kind, const LifeTables& t) {
  MailLetter l; l.battle = battle; l.kind = kind;
  const Thread* th = FindThread(c.st->thread);
  if (!th) return l;
  int step = c.st->step < 0 ? 0 : c.st->step < (int)th->steps.size() ? c.st->step : (int)th->steps.size() - 1;   // a hand-edited file may hold any number
  l.thread = th->id; l.who = th->who; l.step = step;
  l.sender = c.st->sender;
  std::map<std::string, std::string> v; Pronouns(v, c.female);
  v["First"] = FirstName(c.name); v["Last"] = Surname(c.name);
  std::string home = c.facts->home; size_t comma = home.find(','); v["town"] = comma == std::string::npos ? home : home.substr(0, comma);
  v["son"] = c.female ? "daughter" : "son"; v["boy"] = c.female ? "girl" : "boy"; v["brother"] = c.female ? "sister" : "brother";
  v["sender"] = l.sender.empty() ? std::string(th->who == "parents" ? "Mom and Dad" : "") : l.sender;
  l.dear = "Dear " + v["First"] + ",";
  l.body = CapFirst(Trim(Fill(th->steps[step], v)));
  l.sign = Trim(Fill(th->sign, v));
  c.st->step = step + 1; c.st->last = battle;
  if (c.st->step >= (int)th->steps.size()) { c.st->used.push_back(th->id); c.st->thread.clear(); c.st->step = 0; c.st->sender.clear(); }   // the next letter starts a new story
  c.st->got.push_back(l);
  return l;
}
}  // namespace crew_mail_detail

inline int MailThreadCount() { return (int)crew_mail_detail::Threads().size(); }
inline const char* MailStamp(int kind) {
  return kind == 1 ? "RETURNED TO SENDER: DECEASED" : kind == 2 ? "RETURNED TO SENDER: MISSING IN ACTION" : kind == 3 ? "FORWARDED THROUGH THE RED CROSS" : "";
}
inline std::string MailFrom(const MailLetter& l, bool female) {
  std::string his = female ? "her " : "his ";
  if (l.who == "sweetheart") return (female ? "a fellow back home" : "a girl back home") + (l.sender.empty() ? "" : ", " + l.sender);
  if (l.who == "friend") return "an old friend" + (l.sender.empty() ? "" : ", " + l.sender);
  if (l.who == "parents") return his + "parents";
  return his + l.who;
}

inline std::vector<MailDelivery> MailCall(int battle, int lastCall, uint32_t seed, std::vector<MailCrew>& crew, const LifeTables& t) {
  using namespace crew_mail_detail;
  std::vector<MailDelivery> out;
  if (Threads().empty() || battle <= 0) return out;
  auto alive = [](const MailCrew& c) { return c.facts && !c.facts->empty() && c.st && (!c.fate || !c.fate->battle); };
  auto owed = [&](const MailCrew& c) {                                 // lost before this battle, last letter not yet back
    return c.facts && !c.facts->empty() && c.st && c.fate && c.fate->battle && c.fate->battle < battle && !c.st->returned; };
  bool anyOwed = false; for (auto& c : crew) if (owed(c)) anyOwed = true;
  int gap = battle - lastCall;
  bool due = gap >= 3 || (gap >= 2 && Hash(seed, (uint32_t)battle, 51) % 2 == 0) || (anyOwed && gap >= 1);
  if (lastCall == 0 && battle < 2) due = false;                        // not after the very first battle on file
  if (!due) return out;
  std::set<std::string> active;                                        // threads the living are on (a lost man's stays where it stopped)
  for (auto& c : crew) {
    if (c.st && !c.st->thread.empty() && !FindThread(c.st->thread)) { c.st->thread.clear(); c.st->step = 0; c.st->sender.clear(); }   // a thread a later build dropped
    if (c.st && !c.st->thread.empty() && alive(c)) active.insert(c.st->thread);
  }
  // the living who fought: the longest without a letter first, 2 or 3 of them
  std::vector<size_t> rec;
  for (size_t i = 0; i < crew.size(); i++) if (crew[i].fought && alive(crew[i])) rec.push_back(i);
  std::sort(rec.begin(), rec.end(), [&](size_t a, size_t b) {
    if (crew[a].st->last != crew[b].st->last) return crew[a].st->last < crew[b].st->last;
    return Hash(crew[a].id, (uint32_t)battle) < Hash(crew[b].id, (uint32_t)battle); });
  size_t n = 2 + Hash(seed, (uint32_t)battle, 52) % 2;
  if (rec.size() > n) rec.resize(n);
  for (size_t i : rec) {
    MailCrew& c = crew[i];
    if (c.st->thread.empty() && !StartThread(c, 61, active, t)) continue;
    active.insert(c.st->thread);
    out.push_back({ i, Deliver(c, battle, 0, t) });
  }
  for (size_t i = 0; i < crew.size(); i++) {
    MailCrew& c = crew[i];
    if (!owed(c)) continue;
    c.st->returned = true;
    if (c.st->thread.empty() && !StartThread(c, 62, active, t)) continue;
    active.insert(c.st->thread);
    char k = c.fate->kind;
    out.push_back({ i, Deliver(c, battle, k == 'c' ? 3 : k == 'm' ? 2 : 1, t) });
  }
  return out;
}
