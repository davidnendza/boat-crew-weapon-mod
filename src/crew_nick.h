// crew_nick.h: nicknames the crew earn from their battles on file (crew files). The rules read only the
// battle= rows already in crew-files.ini; the player keeps or vetoes each offer on the after-battle screen. Pure C++17
// like crew_report.h, so the offline test can check the rules. Every nickname is one or two plain words: no "|", no
// " - " (the crew file splits headings on " - ").
#pragma once
#include "crew_report.h"
#include <set>

struct NickRec { char state = 'p'; std::string nick; int battle = 0; };   // 'p' offered, 'k' kept, 'v' vetoed
struct NickRule { const char* nick; const char* why; };                   // why reads after "for"

// in priority order: the first rule met and still allowed is the one offered
static const NickRule kNickRules[] = {
  { "Jonah", "being lost at sea and picked up again" },
  { "Lazarus", "going down twice and getting back up" },
  { "Lucky", "going over the side in three battles and coming back every time" },
  { "Stitches", "being sewn up after seven battles" },
  { "Slick", "thirty battles with hardly a scratch" },
  { "Plank Owner", "being aboard since the very first battle" },
  { "Doc", "patching up the crew through twenty battles" },
  { "Chips", "keeping the boat afloat through fifteen battles" },
  { "Wheels", "twenty battles at the wheel" },
  { "Mule", "carrying shells through thirty battles" },
  { "Scrounger", "manning ten different guns" },
  { "Trigger", "ten kills or more in ten different fights" },
  { "Splinters", "coming back from every fight full of splinters" },
};

inline const char* NickWhy(const std::string& nick) {   // the table's reason, "" if not in it
  for (auto& r : kNickRules) if (nick == r.nick) return r.why;
  return "";
}

// The nickname entries (a crew file's battle= rows, CrewFileEntry field names) earn now, or null. lostBattle: the
// fate's battle (0 alive; the lost earn nothing). had: his own records (a 'k' or 'p' gives nothing, a 'v' name is never
// offered again). takenByOthers: names another crew file holds as 'k' or 'p'.
template <class E>
const char* NickEarned(const std::vector<E>& entries, int lostBattle, const std::vector<NickRec>& had, const std::set<std::string>& takenByOthers) {
  using namespace crew_report_detail;
  if (lostBattle || entries.empty()) return nullptr;
  for (auto& h : had) if (h.state == 'k' || h.state == 'p') return nullptr;
  int downs = 0, overN = 0, woundN = 0, woundAll = 0, hits = 0, killN = 0, picked = 0;
  int cat[RC_OTHER + 1] = {};
  std::set<std::string> guns;
  for (auto& e : entries) {
    bool lost = e.outcome == "killed in action" || e.outcome == "captured" || Starts(e.outcome, "in the water when the battle ended");
    if (!lost) { downs += e.downs; if (e.over > 0) ++overN; if (e.wounds > 0) ++woundN; }
    if (e.wounds > 0) ++woundAll;
    if (e.outcome == "lost at sea, then picked up") ++picked;
    hits += e.hits;
    if (e.kills >= 10) ++killN;
    int rc = EntryRoleCat(e.role);
    ++cat[rc];
    std::string g = EntryGun(e.role);
    if (!g.empty()) guns.insert(g);
  }
  int n = (int)entries.size();
  bool met[] = {
    picked > 0, downs >= 2, overN >= 3, woundN >= 7, n >= 30 && woundAll <= 1, entries.front().battle == 1 && n >= 50,
    cat[RC_MEDIC] >= 20, cat[RC_REPAIR] >= 15, cat[RC_HELM] >= 20, cat[RC_AMMO] >= 30, (int)guns.size() >= 10, killN >= 10, hits >= 1500,
  };
  static_assert(sizeof met / sizeof met[0] == sizeof kNickRules / sizeof kNickRules[0], "one test per rule");
  for (size_t i = 0; i < sizeof met / sizeof met[0]; ++i) {
    if (!met[i] || takenByOthers.count(kNickRules[i].nick)) continue;
    bool vetoed = false;
    for (auto& h : had) if (h.state == 'v' && h.nick == kNickRules[i].nick) vetoed = true;
    if (!vetoed) return kNickRules[i].nick;
  }
  return nullptr;
}
