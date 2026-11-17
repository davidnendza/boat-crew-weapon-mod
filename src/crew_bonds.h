// crew_bonds.h: bonds between crew members.
// For every pair of crew the mod keeps what they went through together: battles side by side, battles on the same
// gun, kills they shared, a sinking or a swim they both came through. The closest mate of a crew member speaks first
// when they fall, grieves them, is named in their file, memorial and letter home, and the two banter in quiet moments.
// Pure C++17 like crew_life.h, so the offline test can build bonds from real battle dumps.
#pragma once
#include "crew_report.h"
#include <set>

// ---- interface ----------------------------------------------------------------------------------------------------
struct BondTally {
  uint32_t other = 0;                  // the other crew member's crew id
  int battles = 0;                     // battles both fought
  int team = 0;                        // battles both were on the same gun
  int kills = 0;                       // kills credited to both while both were on a gun, at most 2 a battle (so not the real count)
  int sank = 0;                        // sinkings both came through
  int water = 0;                       // battles both went into the water and came back
  int last = 0;                        // the last battle they shared
};
typedef std::map<uint32_t, std::vector<BondTally>> BondLedger;
float BondScore(const BondTally& b);
const float kBondClose = 6.0f;          // from here two are "close": the closest mate reacts, grieves, is named
// After battle `battle`: b.crew[i] is crew id ids[i] (0 = unknown, skipped). Adds what each pair shared to the ledger.
// guns: the mount each crew member manned first (0 = none), so two guns of one name are not one gun; null: compared
// by gun name (the offline dumps have no mounts).
void BondsAfterBattle(const RepBattle& b, const std::vector<uint32_t>& ids, int battle, BondLedger& ledger,
                      const std::vector<uint64_t>* guns = nullptr);
// The closest mate among one crew member's bonds (score >= kBondClose, and clearly ahead of the next: by kBondLead),
// optionally only among `among`; nullptr when none. The result points into `bonds`.
const float kBondLead = 1.0f;
const BondTally* ClosestBond(const std::vector<BondTally>& bonds, const std::set<uint32_t>* among = nullptr);
// What two shared, as a verb phrase after "we" / "they": "manned the same gun for four battles", "came through a
// sinking together", "shared kills", "served six battles side by side".
std::string BondWhy(const BondTally& b, bool noWater = false);   // noWater: never "went into the water ... and came back" (for the lost at sea)
// ---- end of interface ----------------------------------------------------------------------------------------------

// Weights: a battle side by side counts little on its own; a gun shared, a swim and above all a sinking count most.
inline float BondScore(const BondTally& b) { return 0.5f * b.battles + 2.0f * b.team + 0.5f * b.kills + 6.0f * b.sank + 3.0f * b.water; }
// Keeps at most cap tallies, closest first: those shared in battle bn always stay, so a veteran with a long list of
// old (dead) mates still bonds with this battle's newcomers (release review 0.9.6).
inline void BondTrim(std::vector<BondTally>& v, int bn, size_t cap = 64) {
  std::sort(v.begin(), v.end(), [bn](const BondTally& a, const BondTally& b) {
    bool ra = a.last == bn, rb = b.last == bn;
    if (ra != rb) return ra;
    return BondScore(a) > BondScore(b);
  });
  if (v.size() > cap) v.resize(cap);
}

inline void BondsAfterBattle(const RepBattle& b, const std::vector<uint32_t>& ids, int battle, BondLedger& ledger,
                             const std::vector<uint64_t>* guns) {
  size_t n = b.crew.size() < ids.size() ? b.crew.size() : ids.size();
  bool sinking = false;
  for (auto& c : b.crew) for (auto& e : c.ev) if (e.kind == S_SANK) sinking = true;
  auto lostOf = [](const RepCrew& c) { return c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED; };
  auto overAt = [](const RepCrew& c) { std::vector<float> t; for (auto& e : c.ev) if (e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST || e.kind == S_SIDE) t.push_back(e.t); return t; };
  auto together = [](const std::vector<float>& x, const std::vector<float>& y) {   // into the water within 3 s of each other (as InSinking)
    for (float a : x) for (float o : y) if (a - o <= 3 && o - a <= 3) return true;
    return false;
  };
  auto gunKills = [](const RepCrew& c) { std::set<int> k; for (auto& e : c.ev) if (e.kind == K_KILL && e.killId >= 0 && !e.gun.empty()) k.insert(e.killId); return k; };
  std::vector<std::vector<float>> over(n); std::vector<std::set<int>> kills(n);
  for (size_t i = 0; i < n; i++) { over[i] = overAt(b.crew[i]); kills[i] = gunKills(b.crew[i]); }
  for (size_t i = 0; i < n; i++) for (size_t j = 0; j < n; j++) {
    if (i == j || !ids[i] || !ids[j] || ids[i] == ids[j]) continue;
    const RepCrew &a = b.crew[i], &o = b.crew[j];
    BondTally* t = nullptr;
    for (auto& x : ledger[ids[i]]) if (x.other == ids[j]) t = &x;
    if (!t) { ledger[ids[i]].push_back(BondTally()); t = &ledger[ids[i]].back(); t->other = ids[j]; }
    if (t->last == battle) continue;                                   // counted already (a re-run)
    t->last = battle; t->battles++;
    if (guns && guns->size() > j) { if ((*guns)[i] && (*guns)[i] == (*guns)[j]) t->team++; }
    else { std::string ga = crew_report_detail::RoleGun(a.role), go = crew_report_detail::RoleGun(o.role); if (!ga.empty() && ga == go) t->team++; }
    // Kills credited to both, each while on a gun (the helm is credited with many kills it did not fight: its K_KILL has no gun)
    int shared = 0; for (int k : kills[i]) if (kills[j].count(k)) shared++;
    t->kills += shared < 2 ? shared : 2;                               // at most two a battle
    bool bothBack = !lostOf(a) && !lostOf(o);
    if (sinking && bothBack) t->sank++;
    else if (bothBack && together(over[i], over[j])) {
      int swim = 0;                                                    // who came back from this swim: one the whole crew took says nothing about two
      for (size_t k = 0; k < n; k++) if (!lostOf(b.crew[k]) && together(over[k], over[i]) && together(over[k], over[j])) swim++;
      if (swim <= 3) t->water++;
    }
  }
}
inline const BondTally* ClosestBond(const std::vector<BondTally>& bonds, const std::set<uint32_t>* among) {
  const BondTally* best = nullptr; float second = 0;
  for (auto& b : bonds) {
    if (among && !among->count(b.other)) continue;
    float s = BondScore(b);
    if (!best || s > BondScore(*best)) { if (best) second = BondScore(*best); best = &b; }
    else if (s > second) second = s;
  }
  if (!best || BondScore(*best) < kBondClose || BondScore(*best) - second < kBondLead) return nullptr;   // no clear closest mate
  return best;
}
inline std::string BondWhy(const BondTally& b, bool noWater) {    // the most personal thing they shared, not the biggest number
  using crew_report_detail::Num;
  if (b.sank) return b.sank == 1 ? "came through a sinking together" : "came through " + Num(b.sank) + " sinkings together";
  if (b.team) return b.team == 1 ? "manned the same gun together" : "manned the same gun for " + Num(b.team) + " battles";
  if (b.water && !noWater) return b.water == 1 ? "went into the water together and came back" : "went into the water together " + Num(b.water) + " times";
  if (b.kills) return b.kills == 1 ? "shared a kill" : "shared kills";   // the tally is capped a battle: no number
  return b.battles == 1 ? "served a battle side by side" : "served " + Num(b.battles) + " battles side by side";
}
