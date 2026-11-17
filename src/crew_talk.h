// crew_talk.h: the talking crew. Each crew member has a voice (one of eight, rolled once from the crew id, steered by
// the vanilla traits), and says short lines over their head (CrewMember.headText) when something happens, and in lulls
// about home, their letters and the dead.
// Pure C++17 like crew_mail.h: weaponmod.cpp decides when; this picks and fills the words. The lines are generated
// into talk_lines.h from tools/talk/*.json; BoatCrewMod/talk.ini replaces any section of the same name.
#pragma once
#include "crew_mail.h"
#include "talk_lines.h"

// ---- interface (weaponmod.cpp and tools/crew_report_test.cpp depend on it) ----------------------------------------
struct TalkCrew {
  uint32_t id = 0; std::string name; bool female = false;
  std::string voice;                   // joker, stoic, crazy, silent, hothead, worrier, professional, cheerful
  std::string nick;                    // kept nickname (crew files), "" none: {mate} {down} {dead} use it now and then
  const LifeFacts* facts = nullptr; const LifeFate* fate = nullptr; const MailState* mail = nullptr;
};
struct TalkCtx {                       // who a line is about
  const TalkCrew* mate = nullptr;      // {mate}: someone the speaker talks to or about
  const TalkCrew* down = nullptr;      // {down}: just downed or overboard
  const TalkCrew* dead = nullptr;      // {dead...}: killed (this battle or lately)
  const MailLetter* letter = nullptr;  // {from}: the letter talked about (the speaker's own, or the dead one's returned one)
  std::string why;                     // {why}: what the speaker and their close mate shared (crew_bonds.h BondWhy)
  bool speakerGun = false, deadGun = false;   // rows tagged [gun] / [deadgun] need the speaker / the dead on a gun
};
typedef std::map<std::string, std::vector<std::string>> TalkTables;
std::string RollVoice(uint32_t id, const std::vector<std::string>& traits);
float VoiceRate(const std::string& voice);            // how often they speak up: Silent 0.12 .. Joker 1.5
std::string VoiceTitle(const std::string& voice);     // "the boat's joker", for the crew file
// A line for event ("wounded", "kill", "quiet_home", "rollcall_here", ...): "<voice>.<event>", else "any.<event>".
// Lines whose tokens cannot be filled are skipped, and a line said this battle (used) is not said again while others
// are left. Empty: nothing fits.
std::string TalkLine(const std::string& event, const TalkCrew& sp, const TalkCtx& c, uint32_t seed, const TalkTables& t,
                     std::set<std::string>* used, std::string* tags = nullptr);   // *tags: the picked row's tags ("|warm|")
// The first letter the speaker got and has not talked about yet (index into mail->got, delivered ones only), or -1.
int TalkLetterToTell(const TalkCrew& sp);
// What the speaker says about that letter: "letter.<thread>.<step>", else "<voice>.letter" with {from}. *bad: the
// letter brought bad news (its rows are tagged [bad]), so a reply comes from reply_letter_bad.
std::string TalkLetterLine(const TalkCrew& sp, int idx, uint32_t seed, const TalkTables& t, std::set<std::string>* used, bool* bad = nullptr);
std::string TalkFrom(const MailLetter& l, bool firstPerson, bool female);   // "my mother" / "his wife" / "Dot"
const char* TalkDefaultIni();                          // the built-in lines as ini text
TalkTables ParseTalkIni(const std::string& text);      // a section in talk.ini replaces the built-in one
inline int& TalkNickUses() { static int n = 0; return n; }   // lines said with a nickname for {mate} {down} {dead}; the caller logs and resets it
// ---- end of interface ----------------------------------------------------------------------------------------------

static const char* const kTalkVoices[8] = { "joker", "stoic", "crazy", "silent", "hothead", "worrier", "professional", "cheerful" };

namespace crew_talk_detail {
using namespace crew_life_detail;
inline const std::string& DefaultText() { static const std::string s = [] { std::string x; for (const char* p : kTalkLines) x += p; return x; }(); return s; }
inline const std::vector<std::string>& TalkSec(const TalkTables& t, const std::string& name) {
  static const TalkTables def = [] { TalkTables d; for (auto& kv : crew_report_detail::ParseIniText(DefaultText())) d[kv.first] = kv.second; return d; }();
  static const std::vector<std::string> none;
  auto it = t.find(name);
  if (it != t.end() && !it->second.empty()) return it->second;
  auto d = def.find(name);
  return d != def.end() ? d->second : none;
}
// the speaker's own facts in the first person: "{his} uncle" -> "my uncle", "{he} lied" -> "I lied"
inline std::string FirstPerson(const std::string& s) {
  std::map<std::string, std::string> v = { { "he", "I" }, { "his", "my" }, { "him", "me" }, { "He", "I" }, { "His", "My" }, { "Him", "Me" }, { "man", "man" } };
  return Fill(s, v);
}
inline std::string Third(const std::string& s, bool female) { std::map<std::string, std::string> v; Pronouns(v, female); return Fill(s, v); }
inline std::string City(const std::string& home) { size_t c = home.find(','); return c == std::string::npos ? home : home.substr(0, c); }
}  // namespace crew_talk_detail

inline const char* TalkDefaultIni() { return crew_talk_detail::DefaultText().c_str(); }
inline TalkTables ParseTalkIni(const std::string& text) {
  TalkTables t; for (auto& kv : crew_report_detail::ParseIniText(text)) if (!kv.second.empty()) t[kv.first] = kv.second; return t;
}

inline std::string RollVoice(uint32_t id, const std::vector<std::string>& traits) {
  using namespace crew_report_detail;
  std::string tw; for (auto& s : traits) { std::string l = s; for (auto& ch : l) if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a'); tw += "|" + l; }
  auto has = [&](const char* k) { return tw.find(k) != std::string::npos; };
  float w[8] = { 1, 1, 1, 0.5f, 1, 1, 1, 1 };   // joker stoic crazy silent hothead worrier professional cheerful
  if (has("steady") || has("humble") || has("tenacious")) { w[1] += 3; w[6] += 2; }
  if (has("furious") || has("stubborn")) w[4] += 3;
  if (has("defeatist") || has("traumat")) w[5] += 3;
  if (has("kid")) w[5] += 1;
  if (has("drunk") || has("gambl")) { w[0] += 2; w[2] += 1; }
  if (has("cowboy") || has("hubris")) { w[2] += 2; w[4] += 1; }
  if (has("optimist") || has("gentle")) w[7] += 3;
  if (has("straight") || has("shooter")) { w[3] += 2; w[6] += 1; }
  float total = 0; for (float x : w) total += x;
  float r = (Hash(id, 71) % 10000) / 10000.0f * total;
  for (int i = 0; i < 8; i++) { if (r < w[i]) return kTalkVoices[i]; r -= w[i]; }
  return kTalkVoices[7];
}
inline float VoiceRate(const std::string& v) {
  return v == "joker" || v == "crazy" ? 1.5f : v == "hothead" ? 1.2f : v == "stoic" ? 0.35f : v == "silent" ? 0.12f : 1.0f;
}
inline std::string VoiceTitle(const std::string& v) {
  if (v == "joker") return "the boat's joker";
  if (v == "stoic") return "a stoic; says little";
  if (v == "crazy") return "a little crazy in a fight";
  if (v == "silent") return "quiet, and deadly with a gun";
  if (v == "hothead") return "a hothead";
  if (v == "worrier") return "a worrier";
  if (v == "professional") return "all business";
  if (v == "cheerful") return "the cheerful one";
  return "";
}
inline std::string TalkFrom(const MailLetter& l, bool firstPerson, bool female) {
  std::string my = firstPerson ? "my " : female ? "her " : "his ";
  if (l.who == "sweetheart" || l.who == "friend") return l.sender.empty() ? (l.who == "friend" ? my + "old pal" : my + "sweetheart") : l.sender;
  if (l.who == "parents") return my + "folks";
  return my + l.who;
}

inline std::string TalkLine(const std::string& event, const TalkCrew& sp, const TalkCtx& c, uint32_t seed, const TalkTables& t,
                            std::set<std::string>* used, std::string* tagsOut) {
  using namespace crew_talk_detail;
  std::map<std::string, std::string> v;
  std::set<std::string> byNick;   // the name tokens that took the nickname this time
  auto called = [&](const TalkCrew* m, const char* tok, uint32_t salt, uint32_t every) {
    if (m->nick.empty() || Hash(sp.id, seed, salt) % every != 0) return Surname(m->name);
    byNick.insert(std::string("{") + tok + "}");
    return m->nick;
  };
  if (c.mate) v["mate"] = called(c.mate, "mate", 83, 3);
  if (!c.why.empty()) v["why"] = c.why;
  if (c.down) v["down"] = called(c.down, "down", 84, 3);
  if (c.dead) {
    v["dead"] = called(c.dead, "dead", 85, 2); v["deadhis"] = c.dead->female ? "her" : "his"; v["deadhe"] = c.dead->female ? "she" : "he";
    if (c.dead->facts && !c.dead->facts->empty()) {
      v["deadplan"] = Third(c.dead->facts->plan, c.dead->female); v["deadhome"] = City(c.dead->facts->home);
      // the keepsake only if something of theirs was kept: not after a sinking, never a bottle or the dice
      const LifeFate* fa = c.dead->fate;
      bool sank = fa && (fa->kind == 's' || fa->by == "sank" || fa->sunk);
      std::string kt; SourceOf(LifeTables(), "keep", c.dead->facts->keep, &kt);
      if (!sank && kt.find("|vice|") == std::string::npos) v["deadkeep"] = Third(c.dead->facts->keep, c.dead->female);
    }
  }
  if (sp.facts && !sp.facts->empty()) {
    v["home"] = City(sp.facts->home); v["plan"] = FirstPerson(sp.facts->plan); v["keep"] = FirstPerson(sp.facts->keep);
    if (sp.facts->job.find_first_of(";\"") == std::string::npos) v["job"] = FirstPerson(sp.facts->job);
  }
  if (c.letter) v["from"] = c.dead ? TalkFrom(*c.letter, false, c.dead->female) : TalkFrom(*c.letter, true, sp.female);
  // used keys: the line, or "#" + the row for a row with tokens (said once a battle, whatever name it took);
  // "~" + a key: said last battle, picked only when everything else was. shortcut: a caller that erases a
  // templated line it could not say leaves its row used; rare, as callers check the speaker first.
  std::vector<std::string> fit, now, fresh; std::map<std::string, std::string> tagOf, keyOf, rawOf;
  for (const char* sec : { "voice", "any" }) {
    std::string name = std::string(sec[0] == 'v' ? sp.voice : "any") + "." + event;
    for (auto& row : TalkSec(t, name)) {
      std::string tg, raw = Untag(row, &tg), s = Fill(raw, v);
      if (s.empty() || s.find('{') != std::string::npos || s.size() > 90) continue;   // a token this moment cannot fill, or too long to read
      if ((tg.find("|gun|") != std::string::npos && !c.speakerGun) || (tg.find("|deadgun|") != std::string::npos && !c.deadGun)) continue;
      std::string key = raw.find('{') != std::string::npos ? "#" + raw : CapFirst(Trim(s));
      fit.push_back(s); tagOf[s] = tg; keyOf[s] = key; rawOf[s] = raw;
      if (!used || !used->count(key)) { now.push_back(s); if (!used || !used->count("~" + key)) fresh.push_back(s); }
    }
    if (!fit.empty()) break;                                            // the voice's own lines first
  }
  const std::vector<std::string>& from = !fresh.empty() ? fresh : !now.empty() ? now : fit;
  if (from.empty()) return "";
  const std::string& pick = from[Hash(sp.id, seed, 81) % from.size()];
  if (tagsOut) *tagsOut = tagOf[pick];
  if (used) used->insert(keyOf[pick]);
  for (auto& k : byNick) if (rawOf[pick].find(k) != std::string::npos) { ++TalkNickUses(); break; }
  return CapFirst(Trim(pick));
}
inline int TalkLetterToTell(const TalkCrew& sp) {
  if (!sp.mail) return -1;
  for (int i = (int)sp.mail->got.size() - 1; i >= 0 && i >= sp.mail->told; --i)   // the newest untold one
    if (sp.mail->got[i].kind == 0) return i;
  return -1;
}
inline std::string TalkLetterLine(const TalkCrew& sp, int idx, uint32_t seed, const TalkTables& t, std::set<std::string>* used, bool* bad) {
  using namespace crew_talk_detail;
  if (!sp.mail || idx < 0 || idx >= (int)sp.mail->got.size()) return "";
  const MailLetter& l = sp.mail->got[idx];
  std::map<std::string, std::string> v;
  v["from"] = TalkFrom(l, true, sp.female); v["sender"] = l.sender;
  std::vector<std::string> fit;
  if (bad) *bad = false;
  for (auto& row : TalkSec(t, "letter." + l.thread + "." + std::to_string(l.step + 1))) {
    std::string tags, s = Fill(Untag(row, &tags), v);
    if (tags.find("|bad|") != std::string::npos && bad) *bad = true;
    if (!s.empty() && s.find('{') == std::string::npos && (!used || !used->count(s))) fit.push_back(s);
  }
  if (!fit.empty()) { std::string s = CapFirst(Trim(fit[Hash(sp.id, seed, 82) % fit.size()])); if (used) used->insert(s); return s; }
  TalkCtx c; c.letter = &l;
  return TalkLine("letter", sp, c, seed, t, used);
}
