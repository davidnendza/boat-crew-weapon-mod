// crew_report.h: turns one battle's crew events into an after-action report written as prose.
// Pure C++17, no game or IL2CPP calls: weaponmod.cpp
// fills a RepBattle at the battle change and draws or writes the returned paragraphs.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ---- interface (fixed; weaponmod.cpp depends on it) ----------------------------------------------------------------
enum RepKind {
  // hits that got a report line (not grazes); tier: 1 wound, 2 serious, 3 grievous
  R_ROUND, R_PAIR, R_CLOSE, R_FAR, R_TORPEDO, R_FIRE, R_ABOARD, R_WRECK, R_OTHER,
  // state events
  S_WOUNDED,      // healthState became Wounded
  S_DOWN,         // a hit took HP to 0 (always right after the hit event that did it)
  S_BLEED,        // collapsed from blood loss (HP 0 without a hit)
  S_UP,           // back on their feet after being down
  S_OVER_HIT, S_OVER_BLAST,   // thrown overboard by a hit / a blast (whileDown: was down at the time)
  S_SIDE,         // went over the side (no hit seen)
  S_PULLED,       // pulled back aboard
  S_SANK,         // went down with the boat
  S_DIED_WOUNDS,  // died after being down
  S_KILLED,       // killed outright
  S_PICKED,       // picked up after the battle (was missing)
  S_PRISONER,     // taken prisoner after the battle
  S_ROLE,         // started a new job: part = the role text (see RepCrew.role)
  K_KILL          // credited with a kill: enemy/enemyBase, gun = the player's gun (empty: no gun, e.g. at the helm), mates, killId
};
enum RepOutcome { O_UNHURT, O_LIGHT, O_WOUNDED, O_DOWN_AT_END, O_KIA, O_MISSING, O_CAPTURED, O_LOST_PICKED };
struct RepEv {
  float t = 0;                    // battle seconds
  int kind = R_OTHER, tier = 1;
  std::string part, part2;        // "the left leg", "the head", "the back" ... or empty (unknown); part2: the burst's part for R_PAIR
  std::string enemy, enemyBase;   // "Long Boi No.1 Aux. Sub. Chaser" / "No.1 Aux. Sub. Chaser" (equal when not an elite); empty: unknown
  std::string gun;                // hits: the enemy gun ("Generic Tank Gun", "40 mm Dual Bofors IJN"); K_KILL: the player's gun
  std::string shell;              // the shell asset name ("57 mm Tank Round (JAP)", "40 mm round IJN"), hits only, may be empty
  bool close = false;             // R_CLOSE / R_PAIR: the burst was close
  bool whileDown = false;         // S_OVER_*: was down at the time
  float lost = 0, hpLeft = 0;     // hits: HP lost (pairs: both), HP left after it
  std::vector<std::string> mates; // K_KILL: other crew credited with the same kill (full names)
  int killId = -1;                // K_KILL: the same id for every crew member credited with that one kill
};
struct RepGraze { float t; std::string enemy; float hp; };   // enemy as in RepEv.enemy ("" unknown, "fire", "our own guns")
struct RepCrew {
  std::string name; bool female = false;
  std::string role;               // the job when first seen: "passing ammunition", "repairing", "tending the wounded", "on the <gun>", "at the helm", "standing by"
  std::string gun, gunBase;       // the player gun they manned ("Heavy Lewis of Salvage" / "30 Cal Dual MG (Lewis)"), empty if none
  float maxHp = 0, endHp = 0;
  int outcome = O_UNHURT;
  std::vector<RepEv> ev;          // in time order
  std::vector<RepGraze> grazes;   // in time order
  std::string nick;               // kept nickname from the crew files ("" none)
  std::string tookDead, tookGun;  // station: the crew member lost lately whose gun or job this one took over ("" none)
  int tookJob = -1;               // station: the dead man's RoleCat (RC_GUN: tookGun names the gun), -1 none
};
struct RepBattle {
  float len = 0;                  // battle seconds
  uint32_t seed = 0;              // e.g. a hash of the date and time: the same report always renders the same way
  std::vector<RepCrew> crew;
  struct Kill { int id; std::string enemy, enemyBase; float t; };
  std::vector<Kill> kills;        // one per distinct enemy killed (killId -> name)
};
// The shell burst that threw c over the side at t (an enemy's graze over 3 HP up to 0.6 s before), or null: a wave, the deck.
inline const RepGraze* BlastBefore(const RepCrew& c, float t) {
  const RepGraze* bg = nullptr;
  for (auto& g : c.grazes)
    if (g.enemy != "fire" && g.enemy != "our own guns" && g.enemy.find_first_not_of(" \t") != std::string::npos && g.hp > 3 &&
        t - g.t >= 0 && t - g.t <= 0.6f) bg = &g;
  return bg;
}
// The boat sank at sinkT (< 0: it did not): e took c into the sea with her. The report and the letters home agree on it.
inline bool InSinkingWindow(const RepCrew& c, const RepEv& e, float sinkT) {
  if (sinkT < 0) return false;
  if (e.kind == S_SANK) return e.t >= sinkT - 3 && e.t <= sinkT + 3;
  if (e.kind != S_SIDE || e.t < sinkT - 12 || e.t > sinkT + 3) return false;   // a capsizing boat sheds men for seconds first
  if (BlastBefore(c, e.t)) return false;                                       // a shell threw him: his paragraph tells it
  for (auto& p : c.ev) if (p.kind == S_PULLED && p.t > e.t) return false;     // pulled back aboard: not the sinking
  return true;
}
struct RepPara { int style; std::string text; };   // 0 body, 1 crew heading, 2 battle title
// The report. tables: the [section] -> rows read from BoatCrewMod/reports.ini (may be empty: built-in rows are used;
// a section present in the file replaces the built-in one).
std::vector<RepPara> RenderReport(const RepBattle& b, const std::map<std::string, std::vector<std::string>>& tables);
// The built-in rows as ini text (written to BoatCrewMod/reports.default.ini for reference) and an ini parser.
const char* ReportDefaultIni();
std::map<std::string, std::vector<std::string>> ParseReportIni(const std::string& text);
// One battle's raw data as text (BoatCrewMod/battles/*.bcb) and back, so tools/crew_report_test.exe can re-render real
// battles with changed wording. ParseBattle returns false on a file it does not understand.
std::string DumpBattle(const RepBattle& b);
bool ParseBattle(const std::string& text, RepBattle& b);
// ---- end of interface ----------------------------------------------------------------------------------------------

// ---- implementation (internal; everything below may change) -------------------------------------------------------
// battle dumps: "BCB 1", then one tab-separated record per line (battle, kill, crew, ev, gz; ev and gz belong to the last
// crew). Strings escape \ tab newline; kinds and outcomes are the enum numbers.
// shortcut: enum numbers, not names; a reordered RepKind/RepOutcome needs "BCB 2" and a mapping for old dumps
namespace bcb {
inline std::string Esc(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '\\') o += "\\\\"; else if (c == '\t') o += "\\t"; else if (c == '\n') o += "\\n"; else if (c != '\r') o += c; }
  return o;
}
inline std::string Unesc(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '\\' || i + 1 >= s.size()) { o += s[i]; continue; }
    char n = s[++i];
    o += n == 't' ? '\t' : n == 'n' ? '\n' : n;
  }
  return o;
}
inline std::string F(float v) { char b[32]; snprintf(b, sizeof b, "%.9g", v); return b; }
inline std::vector<std::string> Split(const std::string& line) {
  std::vector<std::string> f(1);
  for (char c : line) { if (c == '\t') f.emplace_back(); else f.back() += c; }
  for (auto& s : f) s = Unesc(s);
  return f;
}
}  // namespace bcb
inline std::string DumpBattle(const RepBattle& b) {
  using namespace bcb;
  std::string o = "BCB 1\nbattle\t" + F(b.len) + "\t" + std::to_string(b.seed) + "\n";
  for (auto& k : b.kills) o += "kill\t" + std::to_string(k.id) + "\t" + F(k.t) + "\t" + Esc(k.enemy) + "\t" + Esc(k.enemyBase) + "\n";
  for (auto& c : b.crew) {
    o += "crew\t" + Esc(c.name) + "\t" + (c.female ? "1" : "0") + "\t" + Esc(c.role) + "\t" + Esc(c.gun) + "\t" + Esc(c.gunBase) + "\t" +
         F(c.maxHp) + "\t" + F(c.endHp) + "\t" + std::to_string(c.outcome) + "\t" + Esc(c.nick) + "\t" + Esc(c.tookDead) + "\t" +
         Esc(c.tookGun) + "\t" + std::to_string(c.tookJob) + "\n";
    for (auto& e : c.ev) {
      std::string mates;
      for (size_t i = 0; i < e.mates.size(); ++i) mates += (i ? "\x1f" : "") + Esc(e.mates[i]);
      o += "ev\t" + F(e.t) + "\t" + std::to_string(e.kind) + "\t" + std::to_string(e.tier) + "\t" + Esc(e.part) + "\t" + Esc(e.part2) + "\t" +
           Esc(e.enemy) + "\t" + Esc(e.enemyBase) + "\t" + Esc(e.gun) + "\t" + Esc(e.shell) + "\t" + (e.close ? "1" : "0") + "\t" +
           (e.whileDown ? "1" : "0") + "\t" + F(e.lost) + "\t" + F(e.hpLeft) + "\t" + std::to_string(e.killId) + "\t" + mates + "\n";
    }
    for (auto& g : c.grazes) o += "gz\t" + F(g.t) + "\t" + Esc(g.enemy) + "\t" + F(g.hp) + "\n";
  }
  return o;
}
inline bool ParseBattle(const std::string& text, RepBattle& b) {
  using namespace bcb;
  b = RepBattle();
  size_t p = 0;
  bool head = false;
  auto num = [](const std::string& s) { return (float)strtod(s.c_str(), nullptr); };
  auto inum = [](const std::string& s) { return (int)strtol(s.c_str(), nullptr, 10); };
  while (p < text.size()) {
    size_t e = text.find('\n', p);
    std::string line = text.substr(p, e == std::string::npos ? std::string::npos : e - p);
    p = e == std::string::npos ? text.size() : e + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    if (!head) { if (line != "BCB 1") return false; head = true; continue; }
    std::vector<std::string> f = Split(line);
    const std::string& k = f[0];
    if (k == "battle" && f.size() >= 3) { b.len = num(f[1]); b.seed = (uint32_t)strtoul(f[2].c_str(), nullptr, 10); }
    else if (k == "kill" && f.size() >= 5) b.kills.push_back(RepBattle::Kill{inum(f[1]), f[3], f[4], num(f[2])});
    else if (k == "crew" && f.size() >= 9) {
      RepCrew c;
      c.name = f[1]; c.female = f[2] == "1"; c.role = f[3]; c.gun = f[4]; c.gunBase = f[5];
      c.maxHp = num(f[6]); c.endHp = num(f[7]); c.outcome = inum(f[8]);
      if (f.size() >= 13) { c.nick = f[9]; c.tookDead = f[10]; c.tookGun = f[11]; c.tookJob = inum(f[12]); }   // older dumps end at 9
      b.crew.push_back(std::move(c));
    } else if (k == "ev" && f.size() >= 16 && !b.crew.empty()) {
      RepEv v;
      v.t = num(f[1]); v.kind = inum(f[2]); v.tier = inum(f[3]); v.part = f[4]; v.part2 = f[5]; v.enemy = f[6]; v.enemyBase = f[7];
      v.gun = f[8]; v.shell = f[9]; v.close = f[10] == "1"; v.whileDown = f[11] == "1"; v.lost = num(f[12]); v.hpLeft = num(f[13]);
      v.killId = inum(f[14]);
      std::string m;
      for (char c : f[15]) { if (c == '\x1f') { v.mates.push_back(m); m.clear(); } else m += c; }
      if (!m.empty()) v.mates.push_back(m);
      b.crew.back().ev.push_back(std::move(v));
    } else if (k == "gz" && f.size() >= 4 && !b.crew.empty()) b.crew.back().grazes.push_back(RepGraze{num(f[1]), f[2], num(f[3])});
    else return false;
  }
  return head;
}
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <functional>

namespace crew_report_detail {

// The built-in rows. Split into pieces because MSVC limits one string literal to about 16 KB.
static const char kDefaultIni[] =
R"INI(; reports.default.ini - the built-in lines of the prose after-action report (written by the mod for reference).
; Copy it to reports.ini and edit: a [section] present in reports.ini replaces the built-in one, rows are drawn at
; random (the same report always draws the same rows). One sentence per line; ';' or '#' starts a comment line.
; Tags at the start of a row: [h] = humorous (never used for the dead, missing or captured, or in a summary when
; anyone was lost); [down] = the sentence says the crew member fell (used only when a "went down" follows the hit);
; [m] = the row speaks of "the medics" (never used for a medic).
; Tokens: {When}/{when} time phrase, {Name} full name, {Last} surname, {Nick} kept nickname, {he} {his} {him} {He} {His} {Him},
; {enemy} {enemys} (possessive), {shell}, {part} {part2} {parts}, {n} {N} counts in words, {span} {spanlen},
; {dur} {Dur}, {gun} {gunshort}, {mates}, {bywhen}; section-specific tokens are noted at the section.

[settings]
; report_times=1 adds the battle clock "(04:27)" after the first time phrase of each paragraph and after every
; down, overboard and death sentence. report_numbers=1 adds "(52 of 204 left)" to the outcome line.
report_times=1
report_numbers=1

[ships]
; Full ship name = short name | singular noun | plural noun. The longest name that ends the ship's name wins.
; A name ending in " Elite" reads as its plain type with "elite" in front ("elite sub chasers").
Ka-Mi Amphibious = Ka-Mi | Ka-Mi tank | Ka-Mi tanks
Armored Ka-Mi Amphibious = armored Ka-Mi | armored Ka-Mi tank | armored Ka-Mi tanks
IJA Type A Landing Boat = landing boat | landing boat | landing boats
Soukou-Tei = Soukou-Tei | Soukou-Tei gunboat | Soukou-Tei gunboats
IJN Motor Gunboat H35 = motor gunboat | motor gunboat | motor gunboats
IJN Patrol T1 = patrol boat | patrol boat | patrol boats
Mutsuki Destroyer = Mutsuki | Mutsuki destroyer | Mutsuki destroyers
Fubuki Destroyer (Supply) = Fubuki | Fubuki destroyer | Fubuki destroyers
Fubuki Destroyer = Fubuki | Fubuki destroyer | Fubuki destroyers
Akizuki Destroyer = Akizuki | Akizuki destroyer | Akizuki destroyers
I-168 Submarine = I-168 | I-168 submarine | I-168 submarines
No.1 Aux. Sub. Chaser = sub chaser | sub chaser | sub chasers
No.101 Aux. Minesweeper = minesweeper | minesweeper | minesweepers
Japanese Tanker Sugar Baker Sugar = tanker | tanker | tankers
A6M Zero = Zero | Zero fighter | Zero fighters
E8N = E8N | E8N floatplane | E8N floatplanes
Ground AA = AA battery | AA battery | AA batteries
Ground MG Nest Sand = machine-gun nest | machine-gun nest | machine-gun nests
Ground Mortar 90 mm = mortar pit | mortar pit | mortar pits
Watch Tower = watch tower | watch tower | watch towers
Tanker = tanker | tanker | tankers
Japanese Merchant Type B Sugar Baker Love = merchant | merchant ship | merchant ships
Nagara Cruiser = Nagara | Nagara cruiser | Nagara cruisers
K7 Subchaser = K7 | K7 sub chaser | K7 sub chasers
D3A Carrier = D3A | D3A dive bomber | D3A dive bombers
D3A = D3A | D3A dive bomber | D3A dive bombers
Ki-84 Heavy = Ki-84 | Ki-84 fighter | Ki-84 fighters
Ki-84 = Ki-84 | Ki-84 fighter | Ki-84 fighters
Ryuujou = Ryuujou | Ryuujou carrier | Ryuujou carriers
Jingei = Jingei | Jingei sub tender | Jingei sub tenders
Nisshin = Nisshin | Nisshin seaplane tender | Nisshin seaplane tenders
Coastal Bombard = coastal gun | coastal gun | coastal guns

[ground]
; A kill whose ship name contains one of these words is a shore position (destroyed, not sunk).
Ground
Watch Tower
Bunker
Coastal

[planes]
; A kill whose ship name contains one of these words is an aircraft.
Zero
Val
Kate
Betty
Rufe
Nell
Oscar
A6M
D3A
E8N
Ki-45
Ki-84
F4F
F4U
P-38
P-40
SBD
Wildcat
Corsair
Warhawk
Dauntless

[big]
; A kill whose ship name contains one of these words is a big ship; the first listed is the biggest.
; (no bare "Carrier": the D3A dive bombers are named "D3A Carrier")
Ryuujou
Cruiser
Destroyer
Jingei
Tender
Transport
Tanker
Freighter
Maru
Supply
Merchant
Minesweeper
Chaser

[hit.round.1]
{When} a {shell} from {enemy} clipped {his} {part}.
{When} {enemy} put a {shell} through the meat of {his} {part}.
[h] {Last} took a {shell} in the {part} {when}, swore, and carried on.
{Last} was hit in the {part} {when} by a {shell} from {enemy}.

[hit.round.2]
{When} a {shell} from {enemy} smashed into {his} {part} and knocked {him} off {his} feet.
{Last} was hit hard {when}: a {shell} in the {part}, courtesy of {enemy}.
{When} {enemy} hit {him} square in the {part}.
{When} a {shell} from {enemy} went into {his} {part} and spun {him} round.

[hit.round.3]
{When} a {shell} from {enemy} struck {him} full in the {part} and {he} staggered.
{When} {enemy} hit {him} in the {part} with a {shell} that should have killed {him}.
[down] {Last} was struck in the {part} {when} and dropped where {he} stood.
[down] {When} a {shell} from {enemy} took {him} in the {part}. {He} went down and did not move.

[hit.pair.same]
; a round and its own burst on one part
{When} a {shell} from {enemy} burst against {his} {part}.
{When} {enemy} hit {him} with a {shell} that went off on impact, tearing up {his} {part}.
{When} a {shell} struck {his} {part} and exploded.

[hit.pair.diff]
; a round and its own burst on two parts: {part} the round's, {part2} the burst's
{When} a {shell} from {enemy} struck {his} {part} and burst, the splinters catching {his} {part2}.
{When} {enemy} hit {him} with a {shell} that went off on impact, wounding {his} {part} and {part2}.
{When} a {shell} hit {his} {part}; what was left of it found {his} {part2}.

[hit.close.1]
{When} a shell from {enemy} burst close by and a splinter tore into {his} {part}.
{When} {enemy} landed one a few yards off; the fragments caught {his} {part}.
{When} a near miss from {enemy} sprayed {his} {part} with splinters.
{When} a burst from {enemy} opened a gash in {his} {part}.
{When} {enemy} dropped a shell alongside, and a fragment cut {his} {part}.
{When} a shell from {enemy} went off near {him}, and a splinter laid open {his} {part}.
{When} a shell from {enemy} burst short and a splinter nicked {his} {part}.
{When} {enemy} straddled the boat and a fragment caught {his} {part}.
{When} {enemy} put one close aboard, and a splinter scored {his} {part}.
{When} a shell from {enemy} burst against the rail beside {him}, and a fragment struck {his} {part}.

[hit.close.2]
{When} a shell from {enemy} burst almost on top of {him}, driving fragments deep into {his} {part}.
{When} a close burst from {enemy} flung {him} against the rail with a splinter in {his} {part}.
{When} {enemys} shell went off at arm's length, and {his} {part} took the worst of it.
{When} {enemy} burst one close enough to touch, and a splinter went deep into {his} {part}.

[hit.close.3]
{When} a shell from {enemy} burst right beside {him} and tore up {his} {part}.
[down] {When} {enemy} landed one at {his} feet; the blast laid {him} flat.
{When} a burst from {enemy} caught {him} in the open and ripped into {his} {part}.

[hit.far.1]
{When} a stray splinter from {enemys} fire cut {his} {part}.
{When} {enemys} shooting was wide, but one fragment still found {his} {part}.
{When} spent shrapnel from {enemy} struck {his} {part}.

[hit.far.2]
{When} a fragment from a burst well off the bow hit {his} {part} hard.
{When} a long splinter from {enemys} shell buried itself in {his} {part}.
{When} a burst {he} never saw put a splinter into {his} {part}.

[hit.far.3]
[h] {When} a single fragment from far off struck {his} {part}. Rotten luck.
{When} a splinter from a distant burst tore into {his} {part}.
[down] {When} a fragment from nowhere took {him} in the {part} and {he} fell.

[hit.torpedo.1]
{When} the torpedo hit threw {him} across the deck, bruising {his} {part}.
{When} the boat bucked under the torpedo and {he} came down hard on {his} {part}.
{When} the torpedo blast slammed {him} into a stanchion.

[hit.torpedo.2]
{When} the torpedo hit lifted {him} off the deck and {he} landed badly on {his} {part}.
{When} the torpedo blast hurled {him} against the bridge, hurting {his} {part} badly.
{When} the shock of the torpedo broke something in {his} {part}.

[hit.torpedo.3]
[down] {When} the torpedo hit flung {him} like a rag doll; {he} did not get up.
{When} {he} was standing over the spot where the torpedo struck.
{When} the torpedo blast caught {him} full on.

[hit.fire.1]
; every fire tick of one fire (gaps of 10 s or less) is one sentence; {part} is a burn site
{When} {he} was burned on the {part} fighting the fire.
{When} the flames caught {his} {part}.
[h] {When} {he} got too close to the fire and paid for it: scorched {part}.

[hit.fire.2]
{When} the fire reached {him} and {he} was badly burned about the {part}.
{When} a flare-up caught {him}, burning {his} {part}.
{When} {he} fought the fire too long and was badly burned.

[hit.fire.3]
{When} the fire swept over {him}.
{He} was badly burned before {he} could get clear {when}.
[down] {When} the flames took {him} and {he} collapsed.

[hit.aboard.1]
{When} something aboard went up, ammunition by the sound of it, and {he} took a fragment in the {part}.
{When} an explosion on deck knocked {him} down and cut {his} {part}.
{When} ready ammunition cooked off and a piece caught {his} {part}.

[hit.aboard.2]
{When} an explosion aboard threw {him} across the deck, hurting {his} {part} badly.
{When} the boat's own ammunition went up beside {him}.
{When} a blast below decks blew a hatch into {his} {part}.

[hit.aboard.3]
{When} an explosion aboard caught {him} at close quarters.
[down] {When} the deck erupted under {him}.
{When} the blast from the magazine reached {him}.

[hit.wreck.1]
{When} {enemy} blew up close aboard and a piece of it struck {his} {part}.
{When} {enemy} went up, and {Last} caught some of it in the {part}.
[h] {When} {enemy} had the last word: debris in {his} {part}.

[hit.wreck.2]
[h] {When} {enemy} exploded alongside, sending wreckage into {his} {part}. A parting gift.
{When} the blast from {enemy} as it died threw {him} down hard.
{When} burning wreckage from {enemy} struck {his} {part}.

[hit.wreck.3]
{When} {enemy} blew apart close aboard and the blast took {him} with it.
[down] {When} {enemy} exploded, and {Last} was too close.
{When} {enemy} went up alongside and {he} caught the full force of it.

[hit.other.1]
{When} something struck {his} {part}.
{When} {he} was hit in the {part}.

[hit.other.2]
{When} {he} was hit hard in the {part}.
{When} something struck {him} in the {part} and knocked {him} down.

[hit.other.3]
{When} {he} was struck in the {part} and badly hurt.
[down] {When} {he} was hit in the {part} and went down.
)INI"
R"INI(
[cluster]
; 2-5 hits from one enemy, each within 20 s of the last: {n} {N} the count, {ntimes} "twice", "three times", {span} "in five seconds",
; {spanlen} "five seconds", {parts} "his left leg twice and back"
{When} {enemy} found the range: {n} hits {span}, in {parts}.
{When} {enemy} hit {him} {ntimes} in {spanlen}: {parts}.
{When} {enemy} caught {him} {ntimes} running: {parts}.
{When} {enemy} would not leave {him} alone. {N} hits {span}: {parts}.
{When} {n} hits from {enemy} landed on {him} {span}: {parts}.
{When} {he} was hit {ntimes} {span} by {enemy}: {parts}.
{When} {enemy} hit {him} {ntimes} {span}, in {parts}.

[cluster.pair]
; a cluster in which every hit was a round with its own burst
{When} {n} {shell}s from {enemy} struck {him} and burst where they hit, in {parts}.
{When} {enemy} put {n} {shell}s into {him} {span}, each one exploding: {parts}.

[cond.half]
; added to a cluster or a serious hit by the HP left: 50-75% (cond.half), 25-50% (cond.bad), under 25% (cond.worst);
; left out when {he} goes down or over the side right after it
, but {he} kept going
, and {he} stayed on {his} feet
, but {he} shrugged it off
, and {he} carried on regardless
, though it slowed {him} down
, but {he} stayed at it

[cond.bad]
, which left {him} in a bad way
, and {he} was badly knocked about
, and it took a lot out of {him}

[cond.worst]
, which left {him} barely standing
, and {he} could hardly stand afterwards

[fold.down]
; a "went down" within 2 s of the hit, when the hit row has no [down] form
, and {he} went down
, and it put {him} on the deck

[fold.over]
; thrown overboard within 2 s of the hit
, and the blast threw {him} over the side
, and the blast pitched {him} into the sea
, and the blast flung {him} into the water
, and {he} was knocked clean overboard

[fold.over.hit]
; as [fold.over] when the hit itself, not a blast, threw {him} over
, and the impact knocked {him} into the sea
, and the force of it sent {him} over the side
, and {he} was knocked clean overboard

[fold.over.far]
; as [fold.over] after a distant burst ([hit.far.N]): no "blast"
, and the shock of it sent {him} staggering over the side
, and the jolt of it sent {him} into the water

[fold.over.down]
, and a second blast rolled {him}, senseless, over the side

[fold.rake]
; {enemy} is the raking ship
, while {enemy} raked the deck around {him}

[fold.rake.own]
; as [fold.rake], told as a sentence of its own after a row that ends in {parts}
{Enemy} had been raking the deck around {him} all the while.
All the while {enemy} had been raking the deck around {him}.

[overflow]
; replaces the lesser hits when a paragraph runs long: {n} the hits left out, {between} "between the fourth and the
; ninth minute", {toppart} the part hit most, {mostin} ", most of them in the leg" / ", both in the leg" / "" (no part
; had most of them), {noneN} "none" ("neither" for two)
{He} took {n} lesser hits {between}{mostin}.
There were {n} lesser hits as well {between}{mostin}.
Another {n} smaller hits found {him} {between}{mostin}.

[overflow.end]
; as [overflow], used when no narrated hit follows it
Another {n} hits found {him} over the course of the action, {noneN} worse than those.
{He} took {n} lesser hits {between}{mostin}.
There were {n} lesser hits as well {between}{mostin}.

[graze.lead]
; opens the paragraph of a crew member who was only grazed; {roleprose} "at work on the hull"
{Last} was {roleprose} when the shooting started.
{Last} spent the action {roleprose}.
{Last} was {roleprose} from the first shot.

[graze.rake]
; five or more grazes from one enemy within 15 s, not during a narrated cluster: the whole graze sentence;
; {rake} the raking enemy, {enemy} the enemy that grazed most, {G} the count of grazes in words, {R} the grazes from
; the raking enemy, {more} ", with two more from elsewhere" when {G} is more than {R} (else empty); told at the rake's time
{rake} raked the deck {when}, and a hail of splinters left {him} with {G} cuts and scrapes, mostly from {enemy}.
{When} {rake} raked the deck, and by the end of it {he} had {R} cuts and scrapes{more}.
{rake} swept the deck {when}, and {he} counted {R} cuts and scrapes from it{more}.
{When} {rake} sprayed the deck, and {R} splinters caught {him}{more}.
{rake} put a long burst along the deck {when}, and {he} took {R} splinters from it{more}.

[graze.shared]
; the deck-wide rake told once in the summary ([summary.rake]); this crew member's part of it, after other sentences
{He} took {his} share of the splinters too: {G}.
{He} caught {G} splinters in {raking} as well.
{Raking} added {G} cuts and scrapes on top of that.
When the deck was {raked}, {he} collected {G} splinter cuts besides.

[graze.shared.more]
; after a [graze.shared*] sentence: the splinters from other enemies than the rake; {M} "Four", {m} "four"
{M} more splinters came from other guns.
{M} more cuts came from elsewhere in the action.
Other guns added {m} more.
The rest of the action added another {m}.
{M} more found {him} from other quarters.
On top of that, other enemies left {him} {m} more.
Another {m} came from other enemies before the end.
{M} more splinters came {his} way from elsewhere.

[graze.shared.more.oneenemy]
; as [graze.shared.more] when all of them came from one enemy, {enemy} "a patrol boat"
{M} more splinters came from {enemy}.
On top of that, {enemy} left {him} {m} more.
{He} took {m} more from {enemy} as well.
Another {m} came from {enemy}.

[graze.shared.more.one]
One more splinter came from another gun.
Another enemy added a single splinter.
One more came from elsewhere before the end.

[graze.shared.before]
; as [graze.shared] when the raking came before every narrated hit: it opens the paragraph, no "besides" or "too"
{Raking} had already left {him} with {G} splinter cuts.
{He} had taken {G} splinter cuts in {raking} by then.
{Raking} had already cost {him} {G} cuts and scrapes.

[graze.shared.before.one]
{Raking} had already left {him} a scratch.
{He} had picked up a scratch in {raking} by then.

[graze.shared.only]
; as [graze.shared], when it is all there is to tell: the whole paragraph; {roleprose} "passing ammunition",
; {staying} "kept passing ammunition" / "stayed at the helm"
{Last} was {roleprose} when the deck was {raked}, and took {G} splinter cuts without leaving the job.
{Last}, {roleprose}, collected {G} cuts and scrapes and carried on.
{G} cuts and scrapes for {Last}, {everyG} taken while {roleprose}.
[h] {Last} counted {G} scratches while {roleprose} and wants {everyG} written down.
{Last} {staying} through {raking} and had {G} small cuts to show for it.
{Last} took {G} splinters while {roleprose}, {allofG} small.
{Raking} caught {Last} {roleprose}; {G} cuts, and {he} kept at it.
{Last} {staying} while the splinters flew, and paid for it with {G} cuts and scrapes.
While {roleprose}, {Last} picked up {G} cuts and scrapes; {noneG} needed more than a dressing.
[h] {G} splinters for {Last}, {allG} collected {roleprose}, and {he} has complained about each one.
When {raking} came, {Last} was {roleprose}, and {he} came out of it with {G} small wounds.
{Last} had {G} splinter cuts from {raking}, {allofG} picked up {roleprose}.

[summary.burst]
; as [summary.rake] when nobody was raked (five splinters inside 15 s): a shell or two burst over the deck; the
; [graze.shared*] rows then read {raking} "the shelling", {Raking}, {raked} "shelled" ("the raking", "raked" otherwise)
{When} shells burst over the deck, and splinters caught {who}: {range} cuts and scrapes apiece, none of them serious.
{When} shellfire burst over the boat; {who} took {range} splinter cuts, none worth more than a dressing.
{When} shell bursts showered the deck with splinters, and {who} took {range} small cuts each.

[summary.burst.one]
{When} a shell burst over the deck, and splinters caught {who}: a scratch apiece, nothing more.
{When} shellfire burst over the boat; {who} took a single splinter each, none worth more than a dressing.

[summary.rake]
; told once when the same enemy grazed most of the crew: {rake} the enemy, {who} "nearly everyone aboard",
; {range} "one to three" (the fewest and most of its grazes per crew member)
{When} {rake} raked the deck, and splinters caught {who}: {range} cuts and scrapes apiece, none of them serious.
{When} {rake} swept the deck from end to end; {who} took {range} splinter cuts, none worth more than a dressing.
{When} {rake} raked the boat from stem to stern, and {who} paid for it: {range} small splinter wounds each.

[summary.rake.swarm]
; as [summary.rake] when someone took more than twenty grazes: no count, it would read like a tally sheet;
; {spanlen} "half a minute" (from its first splinter to its last, over the whole crew)
{When} {rake} kept the deck under fire for {spanlen}, and splinters peppered {who}.
{When} {rake} swept the boat again and again; {who} came out of it peppered with splinters.
{When} {rake} raked the deck until nobody kept count of the splinters; {who} had their share.

[summary.fire]
; a fire aboard that touched most of the crew (its many small ticks are one event); {who} "most of the crew"
{When} fire broke out aboard; {who} came away from fighting it with minor burns.
{When} a fire took hold on deck, and {who} were singed putting it out.

[graze.fire]
; this crew member's minor burns from a fire aboard (not told in the summary); after other sentences or alone
{When} the fire aboard singed {him}; blisters, nothing worse.
{When} {he} got too close to the fire on deck and came away with minor burns.

[graze.fire.late]
; as [graze.fire] when it can only follow later events (it waits while {he} is down): no time, no "before"
At some point the fire aboard had singed {him} as well.
Somewhere along the way the fire on deck had left {him} with minor burns too.

[graze.swarm]
; more than twenty grazes: no count; {hurt} "none of them worse than a scratch" (by the HP they cost)
{He} was peppered with splinters all through the fight; {hurt}.
Splinters found {him} again and again; {hurt}.
By the end {he} was cut about by more splinters than anyone could count; {hurt}.
{He} was showered with splinters every time the deck was swept; {hurt}.
The splinters kept finding {him} all through the action; {hurt}.
Hardly a minute passed without another splinter finding {him}; {hurt}.
{He} lost count of the splinters long before the end; {hurt}.
{He} spent the action under a steady rain of splinters; {hurt}.

[graze.shared.swarm]
; as [graze.shared] with more than twenty grazes, after other sentences ([summary.rake.swarm] owns "peppered")
{He} came out of {raking} cut about by splinters; {hurt}.
{He} had {his} full share of {raking}'s splinters; {hurt}.
{Raking} caught {him} as well, and the splinters with it; {hurt}.
{He} was in the thick of {raking} and took splinters all through it; {hurt}.
Like the others, {he} caught a hail of splinters in {raking}; {hurt}.
{He} did not escape {raking} either; {his} splinters were past counting, and {hurt}.
On top of everything else, {raking} showered {him} with splinters; {hurt}.
Through all of it {raking} kept {him} under a shower of splinters; {hurt}.
{Raking} added more splinters than anyone could count; {hurt}.
{He} was caught in {raking} with everyone else, and the splinters found {him} again and again; {hurt}.

[graze.shared.swarm.lost]
; as [graze.shared.swarm] for crew killed, lost or captured (told before the fate): nothing about coming out of it
{Raking} had already peppered {him} with splinters; {hurt}.
{He} had been in the thick of {raking}, with splinters flying all round {him}; {hurt}.
{Raking} had caught {him} as well, and the splinters with it; {hurt}.
Like the others, {he} had caught a hail of splinters in {raking}; {hurt}.

[graze.shared.swarm.only]
; as [graze.shared.swarm] when it is all the paragraph has: the summary told the rake, so only this crew member and
; what it cost; {roleprose} "passing ammunition"
{Last} {staying} while the splinters flew; {hurt}.
{Last} took {his} splinters while {roleprose}; {hurt}.
{Last}, {roleprose}, had {his} share of the splinters; {hurt}.
The splinters caught {Last} {roleprose}; {hurt}.
{Last} was {roleprose} the whole time and collected splinters with every pass; {hurt}.
Splinters found {Last} {roleprose} again and again; {hurt}.
{Last} {staying} through it all and paid in splinters; {hurt}.
{Last} lost count of {his} splinters, all of them picked up {roleprose}; {hurt}.
While {roleprose}, {Last} was hit by splinters more times than anyone could count; {hurt}.
{Last} was {roleprose} when the splinters came, and stayed at it; {hurt}.
For {Last}, {roleprose}, the splinters never seemed to stop; {hurt}.
{Last} spent {raking} {roleprose} and took splinters all the while; {hurt}.
)INI"
R"INI(
[swarm.cost.1]
; what the splinters of a swarm cost, by HP: under 5%; a full clause after "; " ("them" = the splinters).
; Capped by the outcome: lightly hurt crew get at most [swarm.cost.2], wounded with over 60% HP left at most .3
none of them was worse than a scratch
a dressing would have covered any one of them
they hardly drew blood
the worst of them was a scratch
none of them went deeper than the skin
they stung more than they hurt
iodine and a few plasters saw to them
they were scratches, every one

[swarm.cost.2]
; under 15%
none of them went deep, but they added up
they were small, but there were plenty of them
they will mean a sore few days, nothing more
they left {him} sore and stiff, but no worse
[m] the medics had them cleaned up in a few minutes
most were shallow, but there were a great many of them
{he} will be stiff for a while
they took some cleaning up, nothing more
they will have healed inside a week
[m] the medics called them a nuisance rather than a worry

[swarm.cost.3]
; under 30%
enough of them went deep to need stitches afterwards
[m] the medics spent a while on {him} with the tweezers
several of them had to be dug out
a few went deep enough to need the needle
one or two of them will leave scars
[m] they kept the medics busy with {him} for some time
one or two went deep, and the rest made up for it in numbers
the sick bay spent the best part of an hour on {him}
[m] the medics were still picking them out of {him} long after the shooting stopped
{he} was still finding bits of them in {his} skin hours later

[swarm.cost.4]
; 30% or more
between them they bled {him} badly
together they cost {him} a lot of blood
{he} lost a good deal of blood to them
the loss of blood told on {him} before it was over
the bleeding left {him} grey and shaky
{he} was weak from them afterwards
they left {him} badly drained
the loss of blood left {him} pale
{he} was left white and unsteady
they took more blood out of {him} than any one wound could have

[swarm.cost.1.lost]
; as [swarm.cost.N] for crew killed, lost or captured: nothing about treatment or the days after
none of them was worse than a scratch
they hardly drew blood
the worst of them was a scratch

[swarm.cost.2.lost]
none of them went deep, but they added up
they were small, but there were plenty of them
most were shallow, but there were a great many of them

[swarm.cost.3.lost]
several of them cut deep
enough of them went deep to draw a lot of blood
a few of them went deep

[swarm.cost.4.lost]
between them they bled {him} badly
together they cost {him} a lot of blood
{he} lost a good deal of blood to them
the loss of blood told on {him}

[summary.rake.one]
; as [summary.rake] when nobody took more than one graze
{When} {rake} raked the deck, and splinters caught {who}: a scratch apiece, nothing more.
{When} {rake} swept the deck; {who} took a single splinter, none worth more than a dressing.

[graze.shared.one]
; as [graze.shared] for a single graze
{He} caught a splinter in it as well.
{Raking} left {him} a scratch on top of that.

[graze.shared.only.one]
; as [graze.shared.only] for a single graze
{Last} was {roleprose} when the deck was {raked}, and got off with a scratch.
A single splinter found {Last} {roleprose}; it needed a dressing and nothing more.
{Last} {staying} through {raking} and came out of it with one scratch.
[h] {Last} took one splinter while {roleprose}, and wants it in the report. It is in the report.

[graze.only]
; a crew member who was only grazed (no rake, no narrated hit): the whole sentence; {G} the count, {roleprose}
{Last} was {roleprose} when the shooting started, and came away with {G} cuts and scrapes.
{Last} spent the action {roleprose} and collected {G} small cuts, {noneofG} deep.
{G} cuts and scrapes for {Last}, {allG} picked up while {roleprose}.
{Last} was {roleprose} from the first shot; {G} splinters found {him} there, {noneofG} deep.
[h] {Last} took {G} splinters while {roleprose}, and has shown each of them to anyone who will look.
{Last} {staying} and took {G} splinters for it, nothing worse.
Stray splinters found {Last} {roleprose}: {G} small cuts in all.
{Last} came off the action with {G} small splinter cuts.
[m] {Last} picked up {G} small cuts {roleprose}; the medics were done with {him} in a minute.

[graze.only.one]
; as [graze.only] for a single graze
{Last} was {roleprose} when the shooting started, and came away with a single scratch.
{Last} spent the action {roleprose}; one splinter found {him}, and not deeply.
A lone splinter caught {Last} while {roleprose}.
{Last} was {roleprose} from the first shot and got off with one scratch.

[graze.one]
{He} picked up a scratch as well.
There was a scratch besides.

[graze.few]
; {G} the count of grazes in words
{G} minor cuts and scrapes, which {he} did not mention.
{He} also picked up {G} cuts and scrapes.
There were {G} small cuts besides.

[graze.many]
; {enemy} is the enemy that grazed most
A hail of splinters left {him} with {G} cuts and scrapes, mostly from {enemy}.
[h] {G} lesser splinters {he} did not bother to count.
{G} splinter scratches besides, {noneG} worth a dressing alone.
Besides all that, {he} carried {G} splinter cuts, {mostG} from {enemy}.

[graze.none]
The small hits added up.

[graze.blood]
; graze HP at least 20% of max HP and more than any single wound; joined to the graze sentence with "; "
together they cost {him} more blood than any single wound
between them they bled {him} more than any one wound
it was these, not the shells, that wore {him} down
all told they took more out of {him} than any single hit
together they did more harm than any single wound

[graze.wounded]
; a "wounded" state that no narrated hit explains
, enough to leave {him} wounded by {bywhen}

[over.hit]
The blast threw {him} over the side.
{He} was blown clean overboard.
The last of them knocked {him} into the sea.

[over.down]
Lying senseless on deck, {he} was blown over the side.
{He} was still out cold when a blast rolled {him} into the water.

[over.side]
{When} {he} lost {his} footing and went over the side.
{When} {Last} went into the water.
{When} the boat heeled and {he} slid over the side.
{When} {he} was washed overboard.
{When} a lurch of the deck sent {him} into the sea.
{When} {he} went over the rail.
{When} {he} lost {his} grip and fell into the water.
{When} a wave took {him} off the deck.
{When} {he} was gone over the rail before anyone could grab {him}.

[over.side.blast]
; as [over.side] when a shell had just burst beside {him} (a graze within a moment): {enemy} whose shell it was
{When} a shell from {enemy} burst close by and threw {him} into the sea.
{When} a near miss from {enemy} knocked {him} over the side.
{When} the blast of a shell from {enemy} sent {him} over the rail.

[over.pulled]
; {dur} {Dur} the time in the water; every row ends with {he} as the subject ([over.back] follows: ", and went back to ...")
{He} was hauled back aboard after {dur} in the water.
{He} was fished out {dur} later.
{Dur} later {he} was dragged back over the rail, coughing.
After {dur} in the water {he} was pulled back aboard.

[over.back]
; added after a rescue or getting up; {post} "his repairs", "the wheel", "the Lewis"
, and went back to {post}
, and returned to {post}
, and was soon back at {post}

[over.back.again]
; as [over.back] after the first one in a paragraph: {post} is not named again
, and carried on
, and got back to work
, and was soon at it again

[over.picked]
; picked up out of the water in the last half minute of the battle or after it ([h] rows: not when anyone was lost)
[h] {He} was still in the water when the shooting stopped, and was picked up afterwards, cold and furious.
{He} drifted clear of the fight and was recovered when it was over.
The boat came back for {him} after the action.
{He} was still afloat when the firing stopped and was hauled in.
A boat hook brought {him} in once the shooting was over.
{He} kept {his} head above water until the boat could turn for {him}.
{He} was found swimming after the fight and brought aboard.

[over.picked.early]
; as [over.picked] while the fighting was still going on
{When} the boat came round and picked {him} up.
{He} was hauled out {when}, before the fighting was done.
{When} the boat circled back and a boat hook brought {him} in.
{He} kept {his} head above water until the boat could turn for {him}, and was picked up {when}.

[down.hit]
{He} went down.
{He} fell and stayed down.
That put {him} on the deck.

[down.bleed]
{When} the bleeding caught up with {him} and {he} collapsed.
{He} kept going on nerve until {bywhen}, then collapsed from loss of blood.

[down.up]
; {dur} {Dur} the time spent down
{Dur} later {he} was back on {his} feet.
{He} came round {dur} later and got up again.
Somehow {he} got up {when}.

[death.killed]
{He} was killed {when}.

[death.post]
; gunners and the helm only
{He} died at {his} post {when}.

[death.afterdown]
{He} did not get up again.

[death.wounds]
{He} died of {his} wounds after the action.
The medics worked on {him} after the fighting, but {he} died of {his} wounds.

[death.sank]
{He} went down with the boat.
{He} was aboard when the boat sank and was not seen again.

[death.missing]
{He} was not found after the action.

[death.captured]
{He} was picked up by the enemy and is presumed a prisoner.

[role.later]
; {roletask} "help with damage control", "tend the wounded", "man the Lewis" ({rolejob} "the wounded" also works)
Later {he} went to {roletask}.
{When} {he} was sent to {roletask}.

[station.gun]
; a crew member lost in the last three battles had this gun; {dead} his surname, {oldgun} the gun's name
{Last} took over {dead}'s old {oldgun}.
{Last} was on the {oldgun} now, the gun that had been {dead}'s.

[station.ammo]
{Last} passed ammunition in {dead}'s place.

[station.repair]
{Last} took {dead}'s place in damage control.

[station.medic]
{Last} had {dead}'s medical kit now.

[station.helm]
{Last} had the wheel that had been {dead}'s.
)INI"
R"INI(
[kill.solo]
; one kill, alone; {enemy} the kill
{When} {gunshort} finished {enemy}.
{Last} got {enemy} {when} with {gunshort}, and the credit was {hers} alone.
{When} {he} put enough into {enemy} to finish it.
{When} {Last} finished off {enemy} with {gunshort}.
{Last} accounted for {enemy} {when}, and nobody else had a hand in it.

[kill.shared]
; one kill, shared; {mates} the others credited ("half the crew" for more than three, "four others" when well short of half)
{When} {he} shared {enemy} with {mates}.
; {all} "Tipper, Jack Janovec and Earl Davenport" (this crew member and the others)
; {they} the same as the subject of a sentence: "he and Janovec" once both have been named in the paragraph
{they} between them finished {enemy} {when}.
{When} {enemy} went down under the fire of {all}.
{When} {they} finished {enemy} together.

[kill.shared.again]
; as [kill.shared] when {mate}'s paragraph already told this kill they shared: a short nod, not the story again
{He} was {mate}'s partner on {enemy} {when}.
{When} {he} was the other gun on {enemy}, with {mate}.

[kill.plane.solo]
; one aircraft, alone (also the boat's first kill)
{When} {he} brought down {enemy} with {gunshort}.
{Last} shot down {enemy} {when} with {gunshort}, and the credit was {hers} alone.
{When} {he} caught {enemy} low over the water and put it into the sea.

[kill.plane.shared]
{When} {they} brought down {enemy} between them.
{Last} shared {enemy} with {mates} {when}; it went into the sea in pieces.

[kill.ground.solo]
; one shore position ([ground]), alone: destroyed, not sunk
{When} {gunshort} knocked out {enemy}.
{Last} silenced {enemy} {when} with {gunshort}, and the credit was {hers} alone.
{When} {he} kept firing into {enemy} until nothing answered.

[kill.ground.shared]
{When} {they} knocked out {enemy} between them.
{they} between them silenced {enemy} {when}.

[kill.opener.solo]
; the boat's first kill of the battle, alone
{Last} opened the boat's account {when}; {enemy} was {hers} alone.
{Last} drew first blood {when} with {gunshort}: {enemy} was {hers} alone.

[kill.opener.shared]
{Last} opened the boat's account {when}, sharing {enemy} with {mates}.
{Last} drew first blood {when}, sharing {enemy} with {mates}.

[kill.few]
; 2-4 kills; {list} "a landing boat early in the fight, a Ka-Mi tank and a sub chaser"
{He} had a hand in {n} kills: {list}.
{N} kills went into the book with {Last}'s name on them: {list}.

[kill.few.more]
; after the opener sentence: the rest of 2-4 kills; {list} "two watch towers and a machine-gun nest"
Later {he} had a hand in {list} as well.
{He} added {list} before it was over.
{List} followed, with {his} name on them too.

[kill.many]
; 5 or more kills; {typeList} "thirteen Ka-Mi tanks, four landing boats and a Zero fighter"
By the end {he} and {gunshort} had a hand in {n} kills: {typeList}.
{He} had a hand in {n} kills in all: {typeList}.
{His} tally came to {n}: {typeList}.
{He} finished the action with {n} kills to {his} credit: {typeList}.
{N} kills were credited to {him}: {typeList}.

[kill.same]
; instead of [kill.many], when {other} (an earlier paragraph's surname) had exactly the same kills
{He} was in on the same {n} kills as {other}.
{His} {n} kills were the same as {other}'s.

[kill.alone]
; {solo} "the Zero fighter and one Ka-Mi", {waswere}; {hers} = his/hers
{solo} {waswere} {hers} alone.
{He} accounted for {solo} on {his} own.
{He} got {solo} without help.

[kill.nogun.alone]
; as [kill.alone], without a gun
Nobody else was credited with {solo}.
{solo} went into the book under {his} name alone.

[kill.allalone]
Every one of them was {hers} alone.

[kill.partners]
; {partners} "Jack Janovec (fourteen) and Earl Davenport (eleven)": who shared a quarter of the kills or more
{He} shared most of the rest with {partners}.
The rest {he} mostly shared with {partners}.
Most of the others were shared with {partners}.

[kill.partners.every]
; as [kill.partners], when {partners} "Bernard Tipper" shared all the rest; {more} "; Thomas Wimer (eight) had a share too" or empty
The rest {he} shared with {partners}{more}.
{partners} {waswere} in on all the rest{more}.

[kill.partners.all]
; as [kill.partners], when none of the kills was alone
{He} shared every one of them, mostly with {partners}.
All of them were shared, most with {partners}.
None of them was {hers} alone; {partners} {waswere} in on most.
Every one was shared, chiefly with {partners}.

[kill.partners.all.every]
; as [kill.partners.every], when none of the kills was alone
{He} shared every one of them with {partners}{more}.
Every one was shared with {partners}{more}.
Not one was {hers} alone: {partners} {waswere} in on all of them{more}.

[kill.big]
; {big} "a Mutsuki destroyer"
The best of it was {big}, sunk {when}.
The prize was {big}, sunk {when}.
The biggest of them was {big}, sent to the bottom {when}.

[kill.big.shared]
; as [kill.big], when others shared it; {mates} as in [kill.shared]
The best of it was {big}, sunk {when} and shared with {mates}.
The prize was {big}, sunk {when}; {mates} shared in it.

[kill.plane]
; {plane} "a Zero fighter": an aircraft among the "others" of the tally
One of the other kills was {plane}.
[h] Among the other kills, for good measure, was {plane}.

[kill.nogun]
; a shared kill credited without a gun (the helm)
; {why} ", presumably for keeping the guns fed" from [kill.why.*] by the job, or empty
{He} was credited with a share of {enemy} {when}{why}.
{He} was given a share of {enemy} {when}{why}.
{When} some of the credit for {enemy} went to {him}{why}.

[kill.nogun.solo]
; a kill credited without a gun and without anyone else
{He} was credited with {enemy} {when}{why}.
{When} {enemy} went into the book under {his} name alone{why}.

[kill.nogun.again]
; a shared kill without a gun, when {mate} (another crew member without a gun) was already given a share of it
Like {mate}, {he} was given a share of {enemy} {when}.
{He} also got a share of {enemy} {when}, as {mate} had.

[kill.nogun.many]
; 5 or more kills without a gun; {list} ": two tankers and a minesweeper", or empty
{He} was credited with {n} kills{list}.

[kill.nogun.many.why]
; as [kill.nogun.many] with a reason, {Why} "Presumably for keeping the guns fed": first, so the list ends the sentence
{Why}, {he} was credited with {n} kills{list}.

[kill.why.helm]
; {why} for the helm, after a comma; [kill.why.helm.solo] when nobody else was credited
presumably for putting the boat where the guns could reach it
for the handling of the boat, it seems
no doubt for bringing the guns to bear

[kill.why.helm.solo]
presumably for putting the boat where it could be finished
for the way the boat was handled, it seems
for keeping the boat in the fight, it seems

[kill.why.ammo]
presumably for keeping the guns fed
for keeping the ammunition coming, it seems
no doubt for keeping the shells coming up

[kill.why.repair]
presumably for keeping the boat in the fight
for keeping the boat in action, it seems
no doubt for keeping the boat afloat

[out.kia]
Killed in action.

[out.kia.gun]
Killed in action at {gunshort}.

[out.missing]
Lost at sea.

[out.kia.nick]
; a kept nickname (crew files); {Nick}
Killed in action. To the crew {he} was always "{Nick}".
Killed in action at {gunshort}. Nobody aboard called {him} anything but "{Nick}".
Killed in action. The boat lost "{Nick}" that day.

[out.missing.nick]
Lost at sea. They had called {him} "{Nick}".
Lost at sea; the men kept calling for "{Nick}" long after the fight.

[out.picked.sank]
; after the boat sank ([summary.sank] tells it): picked up out of the water
Pulled from the water after the action.
Swam clear of the wreck and was picked up afterwards.
Found in the water among the wreckage and brought in.
Kept afloat until the shooting stopped, and was picked up.
Recovered from the water after the action, soaked through.
Came through the sinking and was hauled out of the water afterwards.
Picked up after the action, cold and soaked.
Fished out of the sea when it was over.
Hauled out of the sea once the fighting was done.
Spent the rest of the action in the water and was picked up.

[out.missing.sank]
; after the boat sank: not found
Lost at sea.
Not found after the sinking. Lost at sea.
Last seen in the water near the wreck. Lost at sea.
Went into the water with the rest and was never found.
Missing since the sinking. Lost at sea.
No trace of {him} was found afterwards.
Never seen again once the boat had gone.
Not among those found when the search was given up.
Swept away from the wreck and never found.
The search found no sign of {him}.

[out.captured]
; the paragraph has told the capture ([death.captured]): only where it leaves {him}
A prisoner of war.
Now in enemy hands.
Listed as a prisoner of war.

[out.captured.sank]
; after the boat sank ([summary.sank] tells it): fished out of the water by the enemy
Picked out of the water by the enemy and taken prisoner.
Came through the sinking, only to be fished out by the enemy. A prisoner now.
Hauled out of the sea by an enemy boat and taken prisoner.
Survived the sinking and was taken prisoner.
Kept afloat until the enemy picked {him} up. A prisoner of war.

[out.picked]
Lost overboard; recovered after the action.
Went into the sea; picked up after the action.

[out.picked.told]
; the paragraph already told the pickup: only how it left {him}; three quarters of the HP or more and no bad swarm
Back on the roster afterwards, none the worse for the swim.
Dried out and fit for duty.
Back at {his} post once {he} had dried out.
None the worse for the soaking.
A change of clothes, and {he} was back on duty.
Dried off and back on duty the same day.
[h] Back on the roster, and not keen on swimming.
[h] Back aboard, and asking for dry socks.
Wrung out and back at work.

[out.picked.told.wounded]
; as [out.picked.told] for crew who came out of the water hurt: under three quarters of the HP, or a bad swarm
Brought back aboard wounded, with {frac} {hpline}.
Taken below wounded once {he} was out of the water.
Wounded, and in no state to swim again for a while.
[m] Out of the water and into the medics' hands, wounded.
[m] Hauled aboard hurt and soaked through; the medics took {him} from there.
Wounded, with {frac} {hpline} by the time {he} was hauled out.

[out.wounded]
; {frac} "a quarter of his strength", {hpline} "(52 of 204 left)" when report_numbers=1
Wounded, with {frac} {hpline}.
Came home wounded.
Brought home wounded, with {frac} {hpline}.
Ended the action wounded, with {frac} {hpline}.

[out.wounded.ok]
; more than half the HP left (always used above three quarters)
Wounded, but walked off the boat.
Wounded, but still on {his} feet.
Wounded, though nothing that will keep {him} ashore for long.
Wounded, but back on duty before the boat made harbour.
[h] [m] Wounded, and complaining about it, which the medics took as a good sign.

[out.wounded.bad]
; under a quarter of the HP left
Wounded and carried below.
Wounded badly enough to be carried below.

[out.post]
; wounded and still on the first job in the last fifth of the battle, never down or overboard
, still at {his} post at the end
, and never left {his} post
, still on the job when the shooting stopped
, still working when it ended
, and stayed at it to the end
, still at it when the firing stopped

[out.post.back]
; as [out.post] for crew who were down or in the water for a while
, and was back at {his} post by the end
, and back on the job before it was over
, and back at work when the shooting stopped

[out.downend]
Still down when the shooting stopped, with {frac} {hpline}.
Carried below, out cold.

[out.light]
Lightly hurt.
[h] Nothing a dressing won't fix.
Patched up and back on duty.
Fit for duty after a few dressings.
Nothing serious.
[h] Bandaged up, and already asking about a medal.
Back at it the same day.
A dab of iodine and back to work.
Nothing that kept {him} off duty.
Scratched, nothing more.

[out.unhurt]
Unhurt.

[out.unhurt.kills]
Came through it without a scratch.
Not a mark on {him}.
Untouched throughout.
Unhurt, for all the shooting.
[h] Unhurt, and keen to go again.

[quiet.ammunition]
; an unhurt crew member with no kills and nothing else to tell: the whole paragraph
{Last} kept the pans and belts moving and never needed to duck.
{Last} hauled ammunition from start to finish without a scratch.
Not a shell came near {Last}, who passed ammunition all through the action.
[h] {Last} carried ammunition the whole action and says {he} was bored. Nobody believes {him}.
{Last} kept the guns fed and came through without a mark.

[quiet.repairing]
[h] {Last} had nothing to mend, and said so, often.
{Last} stood by with the damage-control gear; it was not needed.
The hull held, and {Last} never had to lift a hammer.
{Last} spent the action on damage control and came through untouched.
Nothing broke that needed {Last}, and nothing touched {him}.

[quiet.medic]
[h] {Last} had no patients, which suited everyone.
{Last}'s kit stayed packed.
[h] No one needed the medic, least of all the medic.
{Last} waited with the dressings; nobody came.
{Last} stood ready with the kit and never had to open it.

[quiet.medic.busy]
; as [quiet.medic] when the medic was not touched but others were hurt
{Last} came through untouched and saw to the others' wounds.
Nothing touched {Last}, who had the others to patch up.
{Last} was not hit, and had dressings to put on the others afterwards.
{Last} was never touched, and spent the end of the action dressing the others.

[quiet.helm]
{Last} kept the boat steady and out of trouble.
{Last} had the helm throughout and kept the enemy guessing.
At the helm, {Last} was never touched.
{Last} held the wheel through it all and came off without a mark.

[quiet.gunner]
{Last} manned {gunshort}; nothing fell to it this time.
{Last} spent the action behind {gunshort} with nothing to show for it, and not a mark on {him}.
{gunshort} found no target worth the name, and {Last} found no trouble.
{Last} worked {gunshort} all action without luck, and without a scratch.

[quiet.standing]
{Last} stood by throughout and was not needed.
{Last} spent the action waiting for orders that never came.
{Last} watched the action from the deck and walked away from it.
Nobody needed {Last}, and nothing came near {him}.

[summary.action]
; {Dur} "nearly eleven minutes", {enemyDesc} "a force of Ka-Mi tanks and landing boats"
{Dur} of action against {enemyDesc}.

[summary.sunk]
; {crewN} crew count, {sunk} ships sunk, {vessels} "enemy vessels", {among} ", among them a Mutsuki destroyer",
; {ground} ", destroyed two enemy positions" ([ground] kills), {planes} ", and shot down one aircraft"
The boat's {crewN} crew sank {sunk} {vessels}{among}{ground}{planes}.

[summary.ground]
; shore positions only (no ships sunk); {groundN}, {positions} "enemy positions", {planes}
The boat's {crewN} crew destroyed {groundN} {positions}{planes}.

[summary.planes]
; aircraft only; {planeN}
The boat's {crewN} crew shot down {planeN} aircraft.

[summary.nosink]
The boat's guns found nothing to sink.

[summary.unhurt]
Nobody aboard was hurt.

[summary.sank]
; the boat sank: {When}, {all} "all ten crew", "seven of the crew", "all nine still aboard"
{When} the boat went down, and {all} went into the water.
{When} the boat capsized and sank under them; {all} went into the sea.
{When} the boat was lost under them, and {all} went into the water.

[summary.picked]
; {P} the count picked up out of the water, {waswere}
{P} {waswere} pulled from the water afterwards.

[summary.hurt]
; {hurt} "eight were wounded and one was lightly hurt"
{hurt}; nobody was lost.

[summary.lost]
; {k} the count lost, {waswere}, {names}
{k} {waswere} lost: {names}.

[summary.captured]
; as [summary.lost] for the crew the enemy took: {k} the count, {waswere}, {names}
{k} {waswere} taken prisoner: {names}.

[summary.lost.all]
; as [summary.lost] when the whole crew was lost (more than one): no list of every name; {k} {crewN}
Not one of the {crewN} came back.
None of the {crewN} crew came back.

[summary.quiet]
; the whole report when nobody was hurt and nothing was sunk; {dur}, {crewN}
A short action ({dur}). The enemy never found the range, and the boat's guns found nothing to sink. All {crewN} crew came through unhurt.

[summary.aboard]
; {crewlist} "George Thomason (ammunition party), Mae Cushman (damage control)"
Aboard: {crewlist}.
)INI";

typedef std::map<std::string, std::vector<std::string>> Tables;

// ---- text helpers ----
inline std::string Trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
inline bool Starts(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
inline bool Ends(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
inline bool IsLow(char c) { return c >= 'a' && c <= 'z'; }
inline bool IsUp(char c) { return c >= 'A' && c <= 'Z'; }
inline bool IsDig(char c) { return c >= '0' && c <= '9'; }
inline std::string Upper(std::string s) { for (auto& c : s) if (IsLow(c)) c = char(c - 'a' + 'A'); return s; }
inline std::string CapFirst(std::string s) { if (!s.empty() && IsLow(s[0])) s[0] = char(s[0] - 'a' + 'A'); return s; }
inline std::string ReplaceAll(std::string s, const std::string& a, const std::string& b) {
  for (size_t p = s.find(a); p != std::string::npos; p = s.find(a, p + b.size())) s.replace(p, a.size(), b);
  return s;
}
inline std::string JoinAnd(const std::vector<std::string>& v) {
  std::string r;
  for (size_t i = 0; i < v.size(); ++i) r += (i == 0 ? "" : i + 1 == v.size() ? " and " : ", ") + v[i];
  return r;
}
inline std::string Num(int n) {
  static const char* const ones[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
  static const char* const tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
  if (n < 0 || n >= 100) return std::to_string(n);
  if (n < 20) return ones[n];
  return std::string(tens[n / 10]) + (n % 10 ? std::string("-") + ones[n % 10] : std::string());
}
inline std::string Ordinal(int n) {
  if (n < 1 || n >= 100) {
    int m = n % 100, d = n % 10;
    return std::to_string(n) + ((m >= 11 && m <= 13) ? "th" : d == 1 ? "st" : d == 2 ? "nd" : d == 3 ? "rd" : "th");
  }
  std::string w = Num(n);
  size_t dash = w.rfind('-');
  std::string head = dash == std::string::npos ? "" : w.substr(0, dash + 1);
  std::string last = dash == std::string::npos ? w : w.substr(dash + 1);
  static const char* const irr[][2] = {{"one", "first"}, {"two", "second"}, {"three", "third"}, {"five", "fifth"},
    {"eight", "eighth"}, {"nine", "ninth"}, {"twelve", "twelfth"}};
  for (auto& p : irr) if (last == p[0]) return head + p[1];
  if (Ends(last, "ty")) return head + last.substr(0, last.size() - 1) + "ieth";
  return head + last + "th";
}
inline std::string Mmss(float t) {
  int s = (int)t;   // truncated like the game log's clock, so the report and the log show the same second
  if (s < 0) s = 0;
  char buf[24];
  snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
  return buf;
}
inline int RoundI(float v) { return (int)std::floor(v + 0.5f); }
inline std::string Dur(float d) {
  if (d < 10) return "a few seconds";
  if (d < 25) return Num((std::max)(10, RoundI(d / 5) * 5)) + " seconds";   // "twenty seconds": 19 s is not half a minute
  if (d < 40) return "half a minute";
  if (d < 75) return "a minute";
  if (d < 120) return "a minute and a half";
  return Num(RoundI(d / 60)) + " minutes";
}
inline std::string Span(float d) {
  if (d <= 5) return "in a few seconds";
  if (d <= 15) return "in " + Num((std::max)(5, RoundI(d / 5) * 5)) + " seconds";
  if (d <= 40) return "in half a minute";
  if (d <= 90) return "in a minute";
  return "over " + Num(RoundI(d / 60)) + " minutes";
}
inline std::string SpanLen(float d) {
  if (d <= 5) return "a few seconds";
  if (d <= 15) return Num((std::max)(5, RoundI(d / 5) * 5)) + " seconds";
  if (d <= 40) return "half a minute";
  if (d <= 55) return "the best part of a minute";   // "in the last minute ... for a minute" read twice
  if (d <= 90) return "a minute";
  return Num(RoundI(d / 60)) + " minutes";
}
// "57 mm Tank Round (JAP)" -> "57 mm shell"; no calibre: "round" or "shell"
inline std::string ShellWord(const std::string& s) {
  for (size_t p = s.find("mm"); p != std::string::npos; p = s.find("mm", p + 2)) {
    size_t e = p;
    if (e > 0 && s[e - 1] == ' ') --e;
    size_t a = e;
    while (a > 0 && (IsDig(s[a - 1]) || s[a - 1] == '.')) --a;
    if (a < e && IsDig(s[a])) {
      std::string cal = s.substr(a, e - a);
      return cal + " mm " + (atof(cal.c_str()) >= 37 ? "shell" : "round");
    }
  }
  return s.find("Round") != std::string::npos ? "round" : "shell";
}
// a -> an before a vowel, "8", "11" and "18"
inline std::string FixArticles(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    o += s[i];
    bool start = i == 0 || s[i - 1] == ' ' || s[i - 1] == '(';
    if (!start || !(s[i] == 'a' || (s[i] == 'A' && i == 0)) || i + 1 >= s.size() || s[i + 1] != ' ') continue;
    size_t j = i + 2;
    std::string w;
    while (j < s.size() && s[j] != ' ' && s[j] != ',' && s[j] != '.' && s[j] != ';' && s[j] != ':') w += s[j++];
    if (w.empty()) continue;
    std::string lw = w;
    for (auto& c : lw) if (IsUp(c)) c = char(c - 'A' + 'a');
    bool an = strchr("aeiou", lw[0]) != nullptr && !Starts(lw, "one") && !Starts(lw, "uni") && !Starts(lw, "use");
    if (lw[0] == '8' || lw == "11" || lw == "18") an = true;
    if (an) o += 'n';
  }
  return o;
}
// collapse spaces, drop the space before punctuation, merge ",," ", ." ";," and "()" left by empty tokens
inline std::string Clean(const std::string& in) {
  std::string o;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i], n = i + 1 < in.size() ? in[i + 1] : '\0';
    if (c == ' ' || c == '\t') {
      if (!o.empty() && o.back() != ' ' && o.back() != '(') o += ' ';
      continue;
    }
    bool punct = c == ',' || c == ';' || c == ':' || c == ')' || (c == '.' && !IsDig(n));
    if (punct && !o.empty() && o.back() == ' ') o.pop_back();
    if ((c == ',' || c == ';' || c == '.') && !o.empty() && (o.back() == ',' || o.back() == ';' || (c == '.' && o.back() == ':'))) {
      if (c != ',') o.back() = c;
      continue;
    }
    if (c == '.' && !o.empty() && o.back() == '.') continue;
    if (c == ')' && !o.empty() && o.back() == '(') {
      o.pop_back();
      while (!o.empty() && o.back() == ' ') o.pop_back();
      continue;
    }
    o += c;
  }
  return Trim(o);
}
inline uint32_t Fnv(const std::string& s, uint32_t h = 2166136261u) {
  for (unsigned char c : s) { h ^= c; h *= 16777619u; }
  return h;
}
inline uint32_t Mix(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
  return x;
}
inline uint32_t Hash(uint32_t a, uint32_t b, uint32_t c = 0, uint32_t d = 0) {
  return Mix(a ^ Mix(b + 0x9e3779b9u) ^ Mix(c * 0x85ebca6bu + 1u) ^ Mix(d * 0xc2b2ae35u + 2u));
}

inline Tables ParseIniText(const std::string& text) {
  Tables t;
  std::string cur;
  size_t i = 0;
  while (i < text.size()) {
    size_t e = text.find('\n', i);
    if (e == std::string::npos) e = text.size();
    std::string line = Trim(text.substr(i, e - i));
    i = e + 1;
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    if (line[0] == '[' && line.back() == ']' && line.find(']') == line.size() - 1) {
      cur = Trim(line.substr(1, line.size() - 2));
      t[cur];
      continue;
    }
    if (!cur.empty()) t[cur].push_back(line);
  }
  return t;
}

struct Row { std::string text; bool h = false, down = false, m = false; };
inline Row ParseRow(std::string s) {
  Row r;
  for (;;) {
    if (Starts(s, "[h]")) { r.h = true; s = Trim(s.substr(3)); }
    else if (Starts(s, "[down]")) { r.down = true; s = Trim(s.substr(6)); }
    else if (Starts(s, "[m]")) { r.m = true; s = Trim(s.substr(3)); }
    else break;
  }
  r.text = s;
  return r;
}

struct Ship { std::string shortName, singular, plural; };

// ---- body parts ----
inline std::string Plain(const std::string& p) {
  std::string s = Trim(p);
  return Starts(s, "the ") ? s.substr(4) : s;
}
// the plain part, or a weighted draw for an unknown one (torso 40, arms 25, legs 25, head 10)
inline std::string PlainOrDraw(const std::string& part, uint32_t h) {
  std::string s = Plain(part);
  if (!s.empty()) return s;
  uint32_t r = h % 100, side = (h >> 8) & 1;
  if (r < 40) { static const char* const t[] = {"chest", "belly", "back"}; return t[(h >> 9) % 3]; }
  if (r < 65) return side ? "left arm" : "right arm";
  if (r < 90) return side ? "left leg" : "right leg";
  return "head";
}
// the sub-part for wound/serious tiers, the plain part for grievous, the burn site for fire
inline std::string SubPart(const std::string& part, int kind, int tier, uint32_t h) {
  std::string p = PlainOrDraw(part, h), side, limb = p;
  if (Starts(p, "left ")) { side = "left "; limb = p.substr(5); }
  else if (Starts(p, "right ")) { side = "right "; limb = p.substr(6); }
  uint32_t k = Mix(h ^ 0x5bd1e995u);
  auto pick = [&](std::initializer_list<const char*> l) { return std::string(l.begin()[k % l.size()]); };
  bool fire = kind == R_FIRE, grave = tier >= 3;
  if (limb.find("head") != std::string::npos) {
    bool backOf = limb.find("back of") != std::string::npos;
    if (fire) return backOf ? "neck" : "face";
    if (grave) return "head";
    return backOf ? "scalp" : pick({"scalp", "brow", "cheek", "jaw", "ear"});   // no "skull" or "neck": a fracture, or the wrong place
  }
  if (limb == "chest") return fire || grave ? "chest" : pick({"chest", "ribs", "shoulder", "collarbone"});
  if (limb == "belly" || limb == "stomach") return fire ? "side" : grave ? "stomach" : pick({"belly", "side", "hip"});
  if (limb == "back") return fire || grave ? "back" : pick({"back", "shoulder blade", "lower back"});
  if (limb == "arm") return fire ? "hands and forearms" : grave ? side + "arm" : side + pick({"forearm", "upper arm", "shoulder", "hand", "elbow"});
  if (limb == "leg") return fire ? "legs" : grave ? side + "leg" : side + pick({"thigh", "calf", "knee", "shin", "foot"});
  return p;
}
// "his left leg" (every [cluster] row states the count) / "his right arm twice, his left leg and his back" / "both legs";
// the most-hit part first
inline std::string PartsList(const std::vector<std::string>& plains, const std::string& his) {
  std::vector<std::pair<std::string, int>> g;
  for (auto& p : plains) {
    auto it = std::find_if(g.begin(), g.end(), [&](const std::pair<std::string, int>& x) { return x.first == p; });
    if (it == g.end()) g.push_back({p, 1}); else ++it->second;
  }
  if (g.empty()) return his + " body";
  auto own = [&](const std::string& p) { return Starts(p, "back of the ") ? "the back of " + his + " " + p.substr(12) : his + " " + p; };
  if (g.size() == 1) return own(g[0].first);
  for (const char* limb : {"leg", "arm"}) {
    auto l = std::find_if(g.begin(), g.end(), [&](const std::pair<std::string, int>& x) { return x.first == std::string("left ") + limb; });
    auto r = std::find_if(g.begin(), g.end(), [&](const std::pair<std::string, int>& x) { return x.first == std::string("right ") + limb; });
    if (l != g.end() && r != g.end() && l->second == 1 && r->second == 1) {
      auto first = l < r ? l : r, second = l < r ? r : l;
      first->first = std::string("both ") + limb + "s";
      g.erase(second);
    }
  }
  std::stable_sort(g.begin(), g.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& z) { return a.second > z.second; });
  std::vector<std::string> items;
  for (auto& x : g) {
    std::string p = Starts(x.first, "both ") ? "both " + his + " " + x.first.substr(5) : own(x.first);
    items.push_back(p + (x.second == 2 ? " twice" : x.second > 2 ? " " + Num(x.second) + " times" : ""));
  }
  return JoinAnd(items);
}

// ---- roles ----
enum { RC_AMMO, RC_REPAIR, RC_MEDIC, RC_GUN, RC_HELM, RC_OTHER };
inline int RoleCat(const std::string& r) {
  if (r == "passing ammunition") return RC_AMMO;
  if (r == "repairing") return RC_REPAIR;
  if (r == "tending the wounded") return RC_MEDIC;
  if (Starts(r, "on the ") || r == "on a gun") return RC_GUN;   // "on a gun": the mod knew the job but not the gun
  if (r == "at the helm") return RC_HELM;
  return RC_OTHER;
}
inline std::string RoleGun(const std::string& r) { return Starts(r, "on the ") ? r.substr(7) : ""; }   // "" for "on a gun"
// A crew file entry's role: the heading noun ("gunner, <gun>", "ammunition party") or an older raw role ("on the <gun>").
inline int EntryRoleCat(const std::string& r) {
  if (r == "ammunition party" || r == "passing ammunition") return RC_AMMO;
  if (r == "damage control" || r == "repairing") return RC_REPAIR;
  if (r == "medic" || r == "tending the wounded") return RC_MEDIC;
  if (Starts(r, "gunner") || Starts(r, "on the ") || r == "on a gun") return RC_GUN;
  if (r == "helmsman" || r == "at the helm") return RC_HELM;
  return RC_OTHER;
}
inline std::string EntryGun(const std::string& r) { return Starts(r, "gunner, ") ? r.substr(8) : Starts(r, "on the ") ? r.substr(7) : ""; }
// George "Lucky" Thomason: the nickname before the surname
inline std::string NickName(const std::string& name, const std::string& nick) {
  if (nick.empty()) return name;
  size_t sp = name.rfind(' ');
  return sp == std::string::npos ? name + " \"" + nick + "\"" : name.substr(0, sp) + " \"" + nick + "\"" + name.substr(sp);
}

inline const char* KindName(int k) {
  switch (k) {
    case R_ROUND: return "round"; case R_PAIR: return "pair"; case R_CLOSE: return "close"; case R_FAR: return "far";
    case R_TORPEDO: return "torpedo"; case R_FIRE: return "fire"; case R_ABOARD: return "aboard"; case R_WRECK: return "wreck";
    default: return "other";
  }
}

// ---- folded P-fire items ----
struct Unit {
  float t = 0, tEnd = 0, lost = 0, hpLeft = 0, worstOne = 0, fT = -1;
  int kind = R_OTHER, tier = 1, n = 1, ev = -1;
  bool allPair = false, fDown = false, fOver = false, fOverDown = false, fOverHit = false;   // fOverHit: S_OVER_HIT, no blast
  bool soonAway = false;   // down or over the side within 10 s after it: no "but he kept going"
  std::string enemy, base, shell, part, part2, rake;
  std::vector<std::string> plains;
};
struct Item {
  int type = 0;   // 0 hit unit or cluster, 1 state event, 2 overflow summary
  Unit u;
  int ev = -1;
  float t = 0;
  int ovN = 0; float ovT1 = 0, ovT2 = 0; std::string ovPart; int ovPartN = 0, ovPartTot = 0;
};
struct Para {
  std::vector<std::string> s; float lastT = -1; bool stamped = false; std::string lastRel; std::set<std::string> rows;
  std::set<std::string> abs;   // absolute time phrases used ("midway through" once)
  bool backSaid = false;       // [over.back] named the post once; later returns use [over.back.again]
  bool whySaid = false;        // one [kill.why.*] reason per paragraph: not "presumably" twice in a row
};
struct Sent { float t = -1; std::string enemy, base, whenCache; std::map<std::string, std::string> v; bool itOk = false; };
struct Ctx {
  const RepCrew* c = nullptr;
  int idx = 0;
  bool allowH = true, gunSaid = false, wasDown = false;
  bool nameDue = false;   // a sentence that opens the paragraph names the crew member instead of "he"
  bool noGunRows = false;  // another crew member mans a gun of the same name: rows that name the gun would confuse
  std::string last, role, prevEnemy, curEnemy;
  std::string prevSubj, curSubj;   // the enemy that was the subject of the last sentence: only it may become "it"
  std::set<std::string> said;
  float lastDownT = -1, lastOverT = -1;
  Para* p = nullptr;
};

struct Renderer {
  RepBattle b;   // a copy: the names are trimmed
  const Tables& user;
  Tables builtin;
  bool times = true, numbers = true;
  std::map<std::string, Ship> ships;
  std::vector<std::string> planes, bigs, grounds;
  std::map<std::string, std::string> baseOf;
  std::vector<RepBattle::Kill> kills;   // b.kills, or derived from the crew's K_KILL events
  struct Bag { std::vector<int> order; std::vector<char> used; uint32_t round = 0; };
  std::map<std::string, Bag> bags;
  std::string sharedRake;   // the enemy that raked most of the crew; [summary.rake] tells it, [graze.shared] per man
  bool sharedBurst = false; // sharedRake was a shell or two, not a rake: nobody took five of its splinters inside 15 s
  float sinkT = -1;          // our boat sank: when; [summary.sank] tells it once
  int sinkN = 0;
  std::set<int> sankCrew;    // crew who went into the water with it (their paragraphs leave it out)
  bool openerSaid = false;   // the boat's first kill is claimed in one paragraph only
  std::set<std::string> namedOne;   // types the summary's first sentence named as a single ship ("a tanker"): later "the tanker"
  std::set<int> bigSaid;     // kill ids [kill.big] told: a shared big kill is told in the first paragraph only
  std::map<int, std::string> sharedSaid;   // kill id -> the full name whose [kill.shared] told it (two sharers)
  bool medicNow = false;                   // the crew member being rendered is a medic: no [m] rows
  int postN = 0;                           // [out.post] clauses so far: three per report at most
  std::map<int, std::string> nogunSaid;   // kill id -> the surname [kill.nogun] gave a share of it ([kill.nogun.again])
  std::map<std::set<int>, std::string> killSets;   // 2+ kills -> the name of the crew member whose paragraph listed them ([kill.same])
  std::map<int, std::map<std::string, int>> absMinute;  // battle minute -> absolute phrase -> paragraphs that used it
  bool anyLost = false;                            // someone was killed, lost or captured: no [h] rows about the water
  bool InSinking(const RepCrew& c, const RepEv& e) const { return InSinkingWindow(c, e, sinkT); }
  std::string Crowd(size_t m) const { return (m + 1) * 2 >= b.crew.size() ? std::string("half the crew") : Num((int)m) + " others"; }   // m mates of 4+
  static void SetG(Sent& s, int n) {   // {G} and the words that agree with it: "both of them", not "all of them", for two
    bool two = n == 2;
    s.v["G"] = Num(n);
    s.v["allG"] = two ? "both" : "all";
    s.v["allofG"] = two ? "both of them" : "all of them";
    s.v["everyG"] = two ? "both of them" : "every one of them";
    s.v["noneG"] = two ? "neither" : "none";
    s.v["noneofG"] = two ? "neither of them" : "none of them";
    s.v["mostG"] = two ? "both of them" : "most of them";
  }

  Renderer(const RepBattle& bb, const Tables& u) : b(bb), user(u), builtin(ParseIniText(kDefaultIni)) {
    for (auto& c : b.crew) {
      c.name = Trim(c.name);
      if (c.name.empty()) c.name = "a crew member";
      for (auto& e : c.ev) for (auto& mt : e.mates) mt = Trim(mt);
    }
    if (auto* r = Rows("settings"))
      for (auto& row : *r) {
        size_t eq = row.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(row.substr(0, eq)), v = Trim(row.substr(eq + 1));
        if (k == "report_times") times = v != "0";
        if (k == "report_numbers") numbers = v != "0";
      }
    if (auto* r = Rows("ships"))
      for (auto& row : *r) {
        size_t eq = row.find('=');
        if (eq == std::string::npos) continue;
        std::vector<std::string> f;
        std::string rest = row.substr(eq + 1);
        for (size_t a = 0;;) {
          size_t bar = rest.find('|', a);
          f.push_back(Trim(rest.substr(a, bar == std::string::npos ? std::string::npos : bar - a)));
          if (bar == std::string::npos) break;
          a = bar + 1;
        }
        Ship s;
        s.shortName = f[0];
        s.singular = f.size() > 1 && !f[1].empty() ? f[1] : s.shortName;
        s.plural = f.size() > 2 && !f[2].empty() ? f[2] : s.singular + "s";
        std::string key = Trim(row.substr(0, eq));
        if (!key.empty() && !s.shortName.empty()) ships[key] = s;
      }
    if (auto* r = Rows("planes")) planes = *r;
    if (auto* r = Rows("big")) bigs = *r;
    if (auto* r = Rows("ground")) grounds = *r;
    for (auto& c : b.crew)
      for (auto& e : c.ev)
        if (!e.enemy.empty()) baseOf[e.enemy] = e.enemyBase.empty() ? e.enemy : e.enemyBase;
    for (auto& k : b.kills) if (!k.enemy.empty()) baseOf[k.enemy] = k.enemyBase.empty() ? k.enemy : k.enemyBase;
    kills = b.kills;
    if (kills.empty()) {
      std::set<int> seen;
      int next = 1000000;
      for (auto& c : b.crew)
        for (auto& e : c.ev)
          if (e.kind == K_KILL && (e.killId < 0 || seen.insert(e.killId).second))
            kills.push_back({e.killId < 0 ? next++ : e.killId, e.enemy, e.enemyBase, e.t});
    }
    std::stable_sort(kills.begin(), kills.end(), [](const RepBattle::Kill& a, const RepBattle::Kill& z) { return a.t < z.t; });
  }

  const std::vector<std::string>* Rows(const std::string& sec) const {
    auto it = user.find(sec);
    if (it != user.end() && !it->second.empty()) return &it->second;
    auto jt = builtin.find(sec);
    if (jt != builtin.end() && !jt->second.empty()) return &jt->second;
    return nullptr;
  }
  // One shuffle bag per section for the whole report: rows are drawn without replacement, then reshuffled.
  // para: rows already used in this paragraph are skipped while another row fits.
  std::string Draw(const std::string& sec, bool allowH, bool down, bool hasGun, Para* para = nullptr) {
    const auto* rows = Rows(sec);
    if (!rows) return "";
    std::vector<Row> rs;
    for (auto& r : *rows) rs.push_back(ParseRow(r));
    auto ok = [&](const Row& r) { return (allowH || !r.h) && r.down == down && !(r.m && medicNow) && (hasGun || r.text.find("{gun") == std::string::npos); };
    Bag& bag = bags[sec];
    auto shuffle = [&]() {
      uint32_t s = Hash(b.seed, Fnv(sec), bag.round++);
      for (size_t i = bag.order.size(); i > 1; --i) { s = Mix(s + 0x6d2b79f5u); std::swap(bag.order[i - 1], bag.order[s % i]); }
    };
    if (bag.order.size() != rs.size()) {
      bag.order.clear();
      for (int i = 0; i < (int)rs.size(); ++i) bag.order.push_back(i);
      bag.used.assign(rs.size(), 0);
      shuffle();
    }
    auto fresh = [&](int i) { return !para || !para->rows.count(rs[i].text); };
    auto pick = [&](bool strict) {
      for (int i : bag.order) if (!bag.used[i] && ok(rs[i]) && (!strict || fresh(i))) return i;
      return -1;
    };
    auto refill = [&]() {
      bool any = false;
      for (int i : bag.order) if (ok(rs[i])) { bag.used[i] = 0; any = true; }
      if (any) shuffle();
      return any;
    };
    bool anyFresh = false;
    for (int i = 0; i < (int)rs.size(); ++i) if (ok(rs[i]) && fresh(i)) anyFresh = true;
    int i = pick(anyFresh);
    if (i < 0 && refill()) i = pick(anyFresh);
    if (i < 0) {
      if (!hasGun) return Draw(sec, allowH, down, true, para);   // every row names the gun: "his gun" it is
      return "";
    }
    bag.used[i] = 1;
    if (para) para->rows.insert(rs[i].text);
    return rs[i].text;
  }

  Ship ShipOf(const std::string& base) const {
    if (Trim(base).empty()) return Ship{"enemy vessel", "enemy vessel", "enemy vessels"};
    auto it = ships.find(base);
    if (it != ships.end()) return it->second;
    const Ship* best = nullptr;
    size_t len = 0;
    for (auto& kv : ships)
      if (kv.first.size() > len && Ends(base, " " + kv.first)) { best = &kv.second; len = kv.first.size(); }
    if (best) return *best;
    if (Ends(base, " Elite") && Known(base.substr(0, base.size() - 6))) {   // "elite sub chasers"
      Ship e = ShipOf(base.substr(0, base.size() - 6));
      e.shortName = "elite " + e.shortName; e.singular = "elite " + e.singular; e.plural = "elite " + e.plural;
      return e;
    }
    Ship s;
    std::string n = base;
    if (Starts(n, "IJN ") || Starts(n, "IJA ")) n = n.substr(4);
    s.shortName = s.singular = n;
    bool cy = n.size() > 1 && n.back() == 'y' && std::string("aeiouAEIOU").find(n[n.size() - 2]) == std::string::npos;
    s.plural = Ends(n, "s") || Ends(n, "x") ? n + " craft" : cy ? n.substr(0, n.size() - 1) + "ies" : n + "s";   // "Heavy" -> "Heavies"
    return s;
  }
  bool Known(const std::string& base) const {   // a [ships] row names this type
    if (ships.count(base)) return true;
    for (auto& kv : ships) if (Ends(base, " " + kv.first)) return true;
    return Ends(base, " Elite") && Known(base.substr(0, base.size() - 6));
  }
  std::string BaseOf(const std::string& enemy) const {
    auto it = baseOf.find(enemy);
    if (it != baseOf.end()) return it->second;
    std::string best;
    for (auto& kv : baseOf)
      if (kv.second.size() > best.size() && Ends(enemy, " " + kv.second)) best = kv.second;
    return best.empty() ? enemy : best;
  }
  static bool HasWord(const std::string& name, const std::vector<std::string>& words) {
    for (auto& w : words) if (!w.empty() && name.find(w) != std::string::npos) return true;
    return false;
  }
  bool IsPlane(const std::string& base) const { return HasWord(base, planes); }
  bool IsGround(const std::string& base) const { return HasWord(base, grounds); }
  int BigRank(const std::string& base) const {   // 0 = the biggest ([big] lists the biggest first), -1 not big
    for (size_t i = 0; i < bigs.size(); ++i) if (!bigs[i].empty() && base.find(bigs[i]) != std::string::npos) return (int)i;
    return -1;
  }
  static std::string Prefix(const std::string& enemy, const std::string& base) {
    if (base.empty() || enemy == base || !Ends(enemy, " " + base)) return "";
    return Trim(enemy.substr(0, enemy.size() - base.size()));
  }

  // ---- tokens ----
  std::string Absolute(Ctx& x, float t) {
    float T = b.len > 0 ? b.len : t + 1;
    uint32_t h = Hash(b.seed, (uint32_t)x.idx, (uint32_t)RoundI(t), 7);
    // never one phrase twice in a paragraph, and another paragraph in the same stretch gets another one
    int mi = (int)(t / 60), sec = (int)t % 60, key = mi;   // key: the minute, or a negative bucket for the edges
    std::vector<std::string> alts;
    if (t < 60) { key = -1; alts = {"in the opening minute", "within the first minute", "before the first minute was out"}; }
    else if (t > T - 15) { key = -2; alts = {"in the closing seconds", "in the dying seconds"}; }
    else if (t > T - 60 && t > 0.7f * T) { key = -3; alts = {"in the last minute", "in the closing minute", "with under a minute to go"}; }
    else if (t > 0.8f * T) { key = -4; alts = h & 1 ? std::vector<std::string>{"near the end", "late in the action", "late on"}
                                                     : std::vector<std::string>{"late in the action", "near the end", "late on"}; }
    else if (t < 0.2f * T) { key = -5; alts = {"early in the fight", "early on", "early in the action"}; }
    else {
      std::string nth = "in the " + Ordinal(mi + 1) + " minute";
      std::string mark = sec < 20 ? "just after the " + Num(mi) + "-minute mark"           // 01:27 is not "just after" 01:00
                         : sec >= 40 ? "just before the " + Num(mi + 1) + "-minute mark" : std::string();
      bool mid = t >= 0.4f * T && t <= 0.6f * T;
      std::string about = "about " + Num(RoundI(t / 60)) + " minutes in";
      if (mid && (h & 2)) alts = {"midway through", nth, mark};
      else if (mid) alts = {nth, "midway through", mark};
      else alts = {nth, mark};
      if (RoundI(t / 60) >= 2) alts.push_back(about);   // not "about one minutes in"
      alts.erase(std::remove(alts.begin(), alts.end(), std::string()), alts.end());
    }
    std::map<std::string, int>& used = absMinute[key];
    std::string r;   // the least used for this minute, never one this paragraph has
    for (auto& a : alts) if (!x.p->abs.count(a) && (r.empty() || used[a] < used[r])) r = a;
    if (r.empty()) r = alts[0];
    ++used[r];
    x.p->abs.insert(r);
    return r;
  }
  std::string ByWhen(float t) {
    float T = b.len > 0 ? b.len : t + 1;
    if (t < 60) return "the end of the first minute";
    if (t > T - 45) return "the end";
    return "the " + Ordinal((int)(t / 60) + 1) + " minute";
  }
  std::string When(Ctx& x, Sent& s) {
    if (s.t < 0) return "";
    if (!s.whenCache.empty()) return s.whenCache;
    std::string ph;
    float lt = x.p->lastT;
    if (lt >= 0 && s.t >= lt) {
      float d = s.t - lt;
      uint32_t h = Hash(b.seed, (uint32_t)x.idx, (uint32_t)RoundI(s.t), 11);
      std::vector<const char*> alts;
      if (d < 1.5f) alts = {"in the same instant", "at the same moment"};
      else if (d < 5) alts = {"almost at once", "a moment later"};
      else if (d < 20) alts = {"moments later", "seconds later"};
      else if (d < 38) alts = {"half a minute later", "shortly afterwards"};
      else if (d < 45) alts = {"shortly afterwards", "less than a minute later"};
      else if (d < 90) alts = {"a minute later", "about a minute after that", "soon afterwards"};
      else if (d < 180) alts = {"a couple of minutes later", "a little later"};
      for (size_t k = 0; k < alts.size(); ++k) {   // never the same relative phrase twice running
        ph = alts[(h + k) % alts.size()];
        if (ph != x.p->lastRel) break;
        ph.clear();
      }
    } else if (lt >= 0 && lt - s.t < 60 && (int)(s.t / 60) == (int)(lt / 60)) ph = "the same minute";   // a rake just before
    x.p->lastRel = ph;
    if (ph.empty()) ph = Absolute(x, s.t);
    if (times && !x.p->stamped) { ph += " (" + Mmss(s.t) + ")"; x.p->stamped = true; }
    x.p->lastT = s.t;
    return s.whenCache = ph;
  }
  std::string Mention(Ctx& x, const std::string& enemy, const std::string& base0, bool poss, bool itOk) {
    if (enemy.empty()) return poss ? "the enemy's" : "the enemy";
    if (enemy == "fire") return poss ? "the fire's" : "the fire";
    if (enemy == "our own guns") return poss ? "our own guns'" : "our own guns";
    std::string base = base0.empty() ? BaseOf(enemy) : base0;
    std::string pre = Prefix(enemy, base), r;
    bool said = x.said.count(enemy) > 0;
    if (said && itOk && x.prevEnemy == enemy && x.prevSubj == enemy) r = poss ? "its" : "it";
    else if (!pre.empty()) r = "the " + pre + " " + (said ? ShipOf(base).shortName : ShipOf(base).singular) + (poss ? "'s" : "");
    else if (!said && Known(base) && std::count_if(kills.begin(), kills.end(), [&](const RepBattle::Kill& o) { return o.enemy == enemy; }) > 1)
      r = "a " + ShipOf(base).singular + (poss ? "'s" : "");   // which of them fired is not known
    else r = "the " + (said ? ShipOf(base).shortName : Known(base) ? ShipOf(base).singular : enemy) + (poss ? "'s" : "");
    x.said.insert(enemy);
    x.curEnemy = enemy;
    return r;
  }
  // "a Ka-Mi tank" for lists; an elite: "a Smol Ka-Mi tank" first, then "the Smol Ka-Mi"
  std::string Article(Ctx& x, const std::string& enemy, const std::string& base0) {
    std::string base = base0.empty() ? BaseOf(enemy) : base0, pre = Prefix(enemy, base);
    std::string a = "a " + ShipOf(base).singular;
    if (pre.empty()) return a;
    bool said = x.said.count(enemy) > 0;
    x.said.insert(enemy);
    return said ? "the " + pre + " " + ShipOf(base).shortName : "a " + pre + " " + ShipOf(base).singular;
  }
  std::string GunLabel(const Ctx& x) const {
    if (!x.c->gun.empty()) return x.c->gun;
    return RoleGun(x.c->role);
  }
  std::string GunName(Ctx& x) {
    std::string g = GunLabel(x);
    if (g.empty()) return x.c->female ? "her gun" : "his gun";
    if (!x.gunSaid) { x.gunSaid = true; return "the " + g; }
    const std::string& gb = x.c->gunBase;
    if (gb.empty() || gb == g) return "the " + g;
    std::string best, w;   // "Homing Oerlikon of the Long Shot" + "20 mm Oerlikon" -> "Oerlikon"
    static const char* const skip[] = {"mm", "Cal", "Dual", "Twin", "Quad", "Gun", "MG", "IJN", "the", "of"};
    auto word = [&]() {
      bool sk = w.size() < 3 || !IsUp(w[0]);
      for (auto* k : skip) if (w == k) sk = true;
      if (!sk && (" " + g + " ").find(" " + w + " ") != std::string::npos && w.size() > best.size()) best = w;
      w.clear();
    };
    for (char ch : gb) { if (ch == ' ' || ch == '(' || ch == ')') word(); else w += ch; }
    word();
    return best.empty() ? (x.c->female ? "her gun" : "his gun") : "the " + best;
  }
  std::string Tok(Ctx& x, const std::string& k, Sent& s) {
    if (k == "parts" && x.nameDue && !x.last.empty() && s.v.count("parts.named")) { x.nameDue = false; return s.v["parts.named"]; }
    auto it = s.v.find(k);
    if (it != s.v.end()) return it->second;
    bool f = x.c->female;
    if (x.nameDue && !x.last.empty() && (k == "he" || k == "He" || k == "him" || k == "Him")) { x.nameDue = false; return x.last; }
    if (x.nameDue && !x.last.empty() && (k == "his" || k == "His")) { x.nameDue = false; return x.last + "'s"; }
    if (k == "he") return f ? "she" : "he";
    if (k == "He") return f ? "She" : "He";
    if (k == "his") return f ? "her" : "his";
    if (k == "His") return f ? "Her" : "His";
    if (k == "him") return f ? "her" : "him";
    if (k == "Him") return f ? "Her" : "Him";
    if (k == "hers") return f ? "hers" : "his";
    if (k == "Name") { x.nameDue = false; return x.c->name; }
    if (k == "Nick") return x.c->nick;
    if (k == "Last") { x.nameDue = false; return x.last; }
    if (k == "when" || k == "When") return When(x, s);
    if (k == "enemy" || k == "Enemy") return Mention(x, s.enemy, s.base, false, s.itOk);
    if (k == "enemys") return Mention(x, s.enemy, s.base, true, s.itOk);
    if (k == "gun" || k == "gunshort") return GunName(x);
    if (k == "bywhen") return s.t >= 0 ? ByWhen(s.t) : "the end";
    if (k == "raking") return sharedBurst ? "the shelling" : "the raking";
    if (k == "Raking") return sharedBurst ? "The shelling" : "The raking";
    if (k == "raked") return sharedBurst ? "shelled" : "raked";
    return "";
  }
  std::string Fill(Ctx& x, const std::string& tpl, Sent& s) {
    std::string out;
    for (size_t i = 0; i < tpl.size();) {
      if (tpl[i] == '{') {
        size_t e = tpl.find('}', i);
        if (e == std::string::npos) { ++i; continue; }
        s.itOk = out.empty() || out == s.whenCache + " ";   // "it" only as the subject
        std::string k = tpl.substr(i + 1, e - i - 1);
        if (s.itOk && (k == "enemy" || k == "Enemy")) x.curSubj = s.enemy;
        out += Tok(x, k, s);
        i = e + 1;
      } else if (tpl[i] == '}') ++i;
      else out += tpl[i++];
    }
    return out;
  }
  static std::string Attach(std::string text, const std::vector<std::string>& clauses) {
    if (clauses.empty()) return text;
    text = Trim(text);
    while (!text.empty() && text.back() == '.') text.pop_back();
    for (auto& c : clauses) {
      std::string t = Trim(c);
      if (t.empty()) continue;
      text += (t[0] == ',' || t[0] == ';') ? t : " " + t;
    }
    return text + ".";
  }
  void Finish(Ctx& x, std::string text, float anchor, float stampT = -1) {
    text = CapFirst(FixArticles(Clean(text)));
    if (text.empty()) return;
    if (stampT >= 0 && times) {
      std::string st = "(" + Mmss(stampT) + ")";
      if (text.find(st) == std::string::npos) {
        size_t end = text.size();
        while (end > 0 && (text[end - 1] == '.' || text[end - 1] == '!')) --end;
        text.insert(end, " " + st);
      }
    }
    if (text.back() != '.' && text.back() != '!' && text.back() != '?') text += '.';
    x.p->s.push_back(text);
    if (anchor >= 0) x.p->lastT = anchor;
    x.prevEnemy = x.curEnemy;
    x.prevSubj = x.curSubj;
    x.curEnemy.clear();
    x.curSubj.clear();
  }
  std::string Line(Ctx& x, const std::string& sec, Sent& s, bool down = false) {
    std::string tpl = Draw(sec, x.allowH, down, !GunLabel(x).empty() && !x.noGunRows, x.p);
    return tpl.empty() ? "" : Fill(x, tpl, s);
  }
  // as Line, skipping rows that start with avoid (", and") while another row fits
  std::string LineAvoid(Ctx& x, const std::string& sec, Sent& s, const std::string& avoid) {
    std::vector<std::string> added;
    if (!avoid.empty() && x.p)
      if (auto* rs = Rows(sec))
        for (auto& r : *rs) { Row pr = ParseRow(r); if (Starts(pr.text, avoid) && x.p->rows.insert(pr.text).second) added.push_back(pr.text); }
    std::string t = Line(x, sec, s);
    for (auto& a : added) x.p->rows.erase(a);
    return t;
  }
  void Say(Ctx& x, const std::string& sec, Sent& s, float anchor, float stampT = -1) {
    x.nameDue = x.p && x.p->s.empty();
    std::string t = Line(x, sec, s);
    x.nameDue = false;
    if (!t.empty()) Finish(x, t, anchor, stampT);
  }

  // ---- role words ----
  static float FirstHurt(const RepCrew& c) {
    float t = 1e9f;
    for (auto& e : c.ev) if (e.kind <= R_OTHER) t = (std::min)(t, e.t);
    for (auto& g : c.grazes) t = (std::min)(t, g.t);
    return t;
  }
  bool HurtBefore(const Ctx& x) const {   // someone else was hurt over 5 s before this crew member: the medic had wounded to tend
    float t1 = FirstHurt(*x.c);
    for (size_t i = 0; i < b.crew.size(); ++i) if ((int)i != x.idx && FirstHurt(b.crew[i]) < t1 - 5) return true;
    return false;
  }
  bool MedicBusy(const Ctx& x) const {   // others hurt and his own hits went on a while: tending the wounded, not standing by
    if (HurtBefore(x)) return true;
    float t0 = 1e9f, t1 = -1e9f;
    for (auto& e : x.c->ev) if (e.kind <= R_OTHER) { t0 = (std::min)(t0, e.t); t1 = (std::max)(t1, e.t); }
    for (auto& g : x.c->grazes) { t0 = (std::min)(t0, g.t); t1 = (std::max)(t1, g.t); }
    if (t1 - t0 <= 10) return false;
    for (size_t i = 0; i < b.crew.size(); ++i) if ((int)i != x.idx && b.crew[i].outcome != O_UNHURT) return true;
    return false;
  }
  bool OwnGun(const Ctx& x, const std::string& role) const { std::string g = RoleGun(role); return g.empty() || g == GunLabel(x); }
  std::string RoleNoun(const RepCrew& c) const {
    switch (RoleCat(c.role)) {
      case RC_AMMO: return "ammunition party";
      case RC_REPAIR: return "damage control";
      case RC_MEDIC: return "medic";
      case RC_GUN: { std::string g = c.gun.empty() ? RoleGun(c.role) : c.gun; return g.empty() ? "gunner" : "gunner, " + g; }
      case RC_HELM: return "helmsman";
      default: return c.role.empty() ? "standing by" : c.role;
    }
  }
  std::string RoleProse(Ctx& x, const std::string& role) {
    switch (RoleCat(role)) {
      case RC_AMMO: return "passing ammunition";
      case RC_REPAIR: return "at work on the hull";
      case RC_MEDIC: return MedicBusy(x) ? "tending the wounded" : "standing by with the medical kit";
      case RC_GUN: return OwnGun(x, role) ? "on " + GunName(x) : role;
      case RC_HELM: return "at the helm";
      default: return role.empty() ? "standing by" : role;
    }
  }
  std::string Staying(Ctx& x, const std::string& role) {   // "{Last} {staying} through the raking": not "stayed passing ammunition"
    switch (RoleCat(role)) {
      case RC_AMMO: return "kept passing ammunition";
      case RC_REPAIR: return "stayed at work on the hull";
      case RC_MEDIC: return MedicBusy(x) ? "kept tending the wounded" : "stayed by the medical kit";
      case RC_GUN: return OwnGun(x, role) ? "stayed on " + GunName(x) : "stayed " + role;
      case RC_HELM: return "stayed at the helm";
      default: return "stayed put";
    }
  }
  void RoleVars(Ctx& x, const std::string& role, Sent& s) { s.v["roleprose"] = RoleProse(x, role); s.v["staying"] = Staying(x, role); }
  std::string RoleJob(Ctx& x, const std::string& role) {
    switch (RoleCat(role)) {
      case RC_AMMO: return "the ammunition party";
      case RC_REPAIR: return "damage control";
      case RC_MEDIC: return "the wounded";
      case RC_GUN: return OwnGun(x, role) ? GunName(x) : role.substr(3);
      case RC_HELM: return "the helm";
      default: return "the deck";
    }
  }
  std::string RoleTask(Ctx& x, const std::string& role) {
    switch (RoleCat(role)) {
      case RC_AMMO: return "pass ammunition";
      case RC_REPAIR: return "help with damage control";
      case RC_MEDIC: return "tend the wounded";
      case RC_GUN: return "man " + (OwnGun(x, role) ? GunName(x) : role.substr(3));
      case RC_HELM: return "take the helm";
      default: return "stand by";
    }
  }
  std::string Post(Ctx& x) {
    switch (RoleCat(x.role)) {
      case RC_AMMO: return "the ammunition";
      case RC_REPAIR: return x.c->female ? "her repairs" : "his repairs";
      case RC_MEDIC: return "the wounded";
      case RC_GUN: return OwnGun(x, x.role) ? GunName(x) : x.role.substr(3);
      case RC_HELM: return "the wheel";
      default: return x.c->female ? "her post" : "his post";
    }
  }
  static const char* QuietSec(int cat) {
    static const char* const s[] = {"quiet.ammunition", "quiet.repairing", "quiet.medic", "quiet.gunner", "quiet.helm", "quiet.standing"};
    return s[cat];
  }
  std::string Frac(const RepCrew& c) const {
    float r = c.maxHp > 0 ? c.endHp / c.maxHp : 1.f;
    const char* his = c.female ? "her" : "his";
    // the nearest phrase, so the words agree with "(86 of 258 left)" next to them
    const char* w = r > 0.85f ? "nearly all of " : r > 0.6f ? "most of " : r > 0.42f ? "half " : r > 0.29f ? "a third of "
                    : r > 0.18f ? "a quarter of " : "very little of ";
    return w + std::string(his) + " strength";
  }
  std::string HpLine(const RepCrew& c) const {
    if (!numbers || c.maxHp <= 0) return "";
    return "(" + std::to_string(RoundI(c.endHp)) + " of " + std::to_string(RoundI(c.maxHp)) + " left)";
  }

  // ---- one crew member ----
  struct Grazes { int g = 0; float hp = 0; std::string top, rake; float r0 = -1, r1 = -1; int rn = 0; int fireN = 0; float fireT = -1; };
  bool fireShared = false;   // [summary.fire] told the fire aboard: no [graze.fire] per man
  // the enemy behind this crew member's grazes, for the deck-wide summary: the rake, or the only enemy of a short burst
  std::string DeckSrc(const RepCrew& c, const Grazes& z, float* t0) const {
    if (z.rn) { *t0 = z.r0; return z.rake; }
    if (!z.g) return "";
    float a = 1e9f, e = -1e9f;
    for (auto& g : c.grazes) { if (g.enemy == "fire") continue; if (g.enemy != z.top) return ""; a = (std::min)(a, g.t); e = (std::max)(e, g.t); }
    *t0 = a;
    return e - a <= 20 ? z.top : "";
  }
  static int GrazesFrom(const RepCrew& c, const std::string& enemy) {
    int n = 0;
    for (auto& g : c.grazes) if (g.enemy == enemy) ++n;
    return n;
  }
  Grazes ReadGrazes(const RepCrew& c) const {
    Grazes z;
    std::map<std::string, std::vector<float>> by;
    std::vector<std::string> order;
    for (auto& g : c.grazes) {
      if (g.enemy == "fire") { if (!z.fireN++) z.fireT = g.t; continue; }   // a fire ticks every frame: one burn, never "377 cuts"
      ++z.g;
      z.hp += g.hp;
      if (!by.count(g.enemy)) order.push_back(g.enemy);
      by[g.enemy].push_back(g.t);
    }
    size_t topN = 0;
    for (auto& e : order) {
      auto& v = by[e];
      if (v.size() > topN) { topN = v.size(); z.top = e; }
      std::sort(v.begin(), v.end());
      for (size_t i = 0, j = 0; j < v.size(); ++j) {
        while (v[j] - v[i] > 15) ++i;
        int n = (int)(j - i + 1);
        if (n >= 5 && n > z.rn) { z.rn = n; z.rake = e; z.r0 = v[i]; z.r1 = v[j]; }
      }
    }
    return z;
  }

  void RenderUnit(Ctx& x, Unit& u) {
    const RepCrew& c = *x.c;
    Sent s;
    s.t = u.t;
    s.enemy = u.enemy;
    s.base = u.base;
    uint32_t h = Hash(b.seed, (uint32_t)x.idx, (uint32_t)u.ev, 3);
    std::string sec;
    if (u.n > 1) sec = u.allPair ? "cluster.pair" : "cluster";
    else if (u.kind == R_PAIR) sec = Plain(u.part2).empty() || Plain(u.part2) == Plain(u.part) ? "hit.pair.same" : "hit.pair.diff";
    else sec = std::string("hit.") + KindName(u.kind) + "." + std::to_string((std::min)(3, (std::max)(1, u.tier)));
    std::string his = c.female ? "her" : "his";
    s.v["shell"] = ShellWord(u.shell);
    s.v["part"] = SubPart(u.part, u.kind, u.tier, h);
    s.v["part2"] = SubPart(u.part2.empty() ? u.part : u.part2, u.kind, u.tier, Mix(h + 1));
    s.v["parts"] = PartsList(u.plains, his);
    {   // {parts} in a paragraph's first sentence, before any {he}: the first "his" becomes the name
      std::string pn = " " + s.v["parts"];
      size_t at = pn.find(" " + his + " ");
      if (at != std::string::npos) s.v["parts.named"] = pn.replace(at + 1, his.size(), x.last + "'s").substr(1);
    }
    s.v["n"] = Num(u.n);
    s.v["N"] = CapFirst(Num(u.n));
    s.v["ntimes"] = u.n == 2 ? "twice" : u.n == 1 ? "once" : Num(u.n) + " times";
    s.v["span"] = Span(u.tEnd - u.t);
    s.v["spanlen"] = SpanLen(u.tEnd - u.t);
    bool hasGun = !GunLabel(x).empty(), downDone = false;
    std::string tpl;
    if (u.fDown) { tpl = Draw(sec, x.allowH, true, hasGun, x.p); downDone = !tpl.empty(); }
    if (tpl.empty()) tpl = Draw(sec, x.allowH, false, hasGun, x.p);
    if (tpl.empty()) tpl = "{When} {he} was hit.";
    x.nameDue = x.p && x.p->s.empty();   // a hit that opens the paragraph names the crew member
    std::string text = Fill(x, tpl, s);
    x.nameDue = false;
    {   // "half a minute later ... three hits in half a minute": the span says it another way
      size_t hm = s.whenCache.find("half a minute");
      size_t at = hm == std::string::npos ? hm : text.find("in half a minute", text.find("half a minute") + 13);
      if (at != std::string::npos) text.replace(at, 16, "within thirty seconds");
    }
    // a row that ends in its {parts} list takes one clause at most; the rake becomes a sentence of its own
    std::string tt = Trim(tpl);
    while (!tt.empty() && tt.back() == '.') tt.pop_back();
    bool partsEnd = Ends(tt, "{parts}");
    // a row ending in a short sentence ("... Rotten luck.", "... A parting gift.") takes no clause: the condition is
    // dropped and the fold-ins become sentences of their own
    std::string body = Trim(text);
    while (!body.empty() && body.back() == '.') body.pop_back();
    size_t dot = body.rfind(". ");
    bool shortTail = dot != std::string::npos && std::count(body.begin() + dot + 2, body.end(), ' ') < 3;
    std::vector<std::string> cl;
    bool rakeOwn = !u.rake.empty() && !shortTail && partsEnd;
    if (!u.rake.empty() && !shortTail && !partsEnd) {
      Sent rs;
      rs.enemy = u.rake;
      rs.base = BaseOf(u.rake);
      cl.push_back(Line(x, "fold.rake", rs));
    }
    if (!shortTail && !u.fDown && !u.fOver && (u.n > 1 || u.tier >= 2) && c.maxHp > 0 && !(partsEnd && !cl.empty())) {
      float r = u.hpLeft / c.maxHp;
      const char* cs = r > 0.75f ? nullptr : r >= 0.5f ? "cond.half" : r >= 0.25f ? "cond.bad" : "cond.worst";
      if (cs && u.soonAway && !strcmp(cs, "cond.half")) cs = nullptr;   // "he kept going", then went over the side
      // the row has its ", and ..." already: not a second one
      if (cs) { Sent cs2; cl.push_back(LineAvoid(x, cs, cs2, text.find(", and ") != std::string::npos ? ", and" : "")); }
    }
    std::vector<const char*> own;   // fold-ins told as their own sentence
    bool full = shortTail || (partsEnd && !cl.empty());
    if (u.fDown && !downDone) { if (full) own.push_back("down.hit"); else { Sent ds; cl.push_back(Line(x, "fold.down", ds)); } }
    full = shortTail || (partsEnd && !cl.empty());
    if (u.fOver) {
      // a distant burst has no "blast"; thrown over by the hit itself (S_OVER_HIT), not by a blast
      const char* fo = u.fOverDown ? "fold.over.down" : u.kind == R_FAR ? "fold.over.far" : u.fOverHit || u.kind == R_ROUND ? "fold.over.hit" : "fold.over";
      if (full) own.push_back(u.fOverDown ? "over.down" : "over.hit");
      else { Sent os; cl.push_back(Line(x, fo, os)); }
    }
    if (u.fDown) { x.wasDown = true; x.lastDownT = u.fT; }
    if (u.fOver) x.lastOverT = u.fT;
    {   // "..., and a fragment cut his jaw, and he was knocked clean overboard": the second "and" becomes "; "
      bool hasAnd = text.find(", and ") != std::string::npos, semi = text.find(';') != std::string::npos;
      for (auto& c2 : cl) {
        std::string t = Trim(c2);
        if (!Starts(t, ", and ")) continue;
        if (hasAnd && !semi) { c2 = "; " + t.substr(6); semi = true; }
        hasAnd = true;
      }
    }
    float anchor = u.fT >= 0 ? u.fT : u.tEnd;
    Finish(x, Attach(text, cl), anchor, (u.fDown || u.fOver) && own.empty() ? u.fT : -1);
    if (rakeOwn) { Sent rs; rs.enemy = u.rake; rs.base = BaseOf(u.rake); Say(x, "fold.rake.own", rs, -1); }
    for (const char* sec2 : own) { Sent os; Say(x, sec2, os, anchor, u.fT); }
  }

  void RenderState(Ctx& x, const RepEv& e, const Item* next) {
    const RepCrew& c = *x.c;
    Sent s;
    s.t = e.t;
    auto backClause = [&](std::string text) {
      bool stop = next && next->type == 1 && next->ev >= 0 &&
                  (c.ev[next->ev].kind == S_ROLE || c.ev[next->ev].kind == S_KILLED || c.ev[next->ev].kind == S_DIED_WOUNDS ||
                   c.ev[next->ev].kind == S_SANK);
      if (stop || text.empty()) return text;
      Sent ps;
      ps.v["post"] = Post(x);
      std::string cl = Line(x, x.p->backSaid ? "over.back.again" : "over.back", ps);   // "his repairs" once a paragraph
      x.p->backSaid = true;
      return Attach(text, {cl});
    };
    // "A minute later ... A minute later he was dragged back": a {Dur} row must not repeat the last relative phrase
    auto durLine = [&](const char* sec, float d) {
      std::string rel = Dur(d) + " later";
      std::string t = LineAvoid(x, sec, s, rel == x.p->lastRel ? "{Dur} later" : "");
      if (Starts(t, CapFirst(rel))) x.p->lastRel = rel;
      return t;
    };
    switch (e.kind) {
      case S_DOWN: x.wasDown = true; x.lastDownT = e.t; Say(x, "down.hit", s, e.t, e.t); break;
      case S_BLEED: x.wasDown = true; x.lastDownT = e.t; Say(x, "down.bleed", s, e.t, e.t); break;
      case S_UP: {
        float d = x.lastDownT >= 0 ? e.t - x.lastDownT : 0;
        s.v["dur"] = Dur(d);
        s.v["Dur"] = CapFirst(Dur(d));
        x.wasDown = false;
        std::string t = durLine("down.up", d);
        if (!t.empty()) Finish(x, backClause(t), e.t);
        break;
      }
      case S_OVER_HIT: case S_OVER_BLAST:
        x.lastOverT = e.t;
        Say(x, e.whileDown ? "over.down" : "over.hit", s, e.t, e.t);
        break;
      case S_SIDE: {   // a shell had just burst beside him: the blast threw him, not a wave
        x.lastOverT = e.t;
        const RepGraze* bg = BlastBefore(*x.c, e.t);
        if (bg) { s.enemy = bg->enemy; s.base = BaseOf(bg->enemy); }
        Say(x, bg ? "over.side.blast" : "over.side", s, e.t, e.t);
        break;
      }
      case S_PULLED: {
        float d = x.lastOverT >= 0 ? e.t - x.lastOverT : 0;
        s.v["dur"] = Dur(d);
        s.v["Dur"] = CapFirst(Dur(d));
        std::string t = durLine("over.pulled", d);
        if (!t.empty()) Finish(x, backClause(t), e.t, e.t);
        break;
      }
      case S_PICKED: {   // "when the shooting stopped" only if it had; no joke about the water when men were lost
        bool h = x.allowH;
        x.allowH = h && !anyLost;
        Say(x, b.len > 0 && e.t < b.len - 30 ? "over.picked.early" : "over.picked", s, e.t, e.t);
        x.allowH = h;
        break;
      }
      case S_SANK: Say(x, "death.sank", s, e.t, e.t); break;
      case S_DIED_WOUNDS: Say(x, "death.wounds", s, e.t, e.t); break;
      case S_KILLED: {
        int cat = RoleCat(x.role);
        const char* sec = x.wasDown ? "death.afterdown"
                          : (cat == RC_GUN || cat == RC_HELM) && (Hash(b.seed, (uint32_t)x.idx, 5) & 1) ? "death.post" : "death.killed";
        Say(x, sec, s, e.t, e.t);
        break;
      }
      case S_PRISONER: Say(x, "death.captured", s, e.t, e.t); break;
      case S_ROLE:
        s.v["rolejob"] = RoleJob(x, e.part);
        s.v["roletask"] = RoleTask(x, e.part);
        x.role = e.part;
        {   // "Later" needs something before it: an opening job change takes the timed row
          x.nameDue = x.p->s.empty();
          std::string t = LineAvoid(x, "role.later", s, x.nameDue ? "Later" : "");
          x.nameDue = false;
          if (!t.empty()) Finish(x, t, e.t);
        }
        break;
      default: break;
    }
  }

  // P-kills; returns the number of distinct kills
  int RenderKills(Ctx& x, const std::vector<const RepEv*>& ks) {
    int n = (int)ks.size();
    if (!n) return 0;
    bool nogun = GunLabel(x).empty();
    for (auto* k : ks) if (!k->gun.empty()) nogun = false;
    bool opener = false;
    if (!kills.empty())
      for (auto* k : ks) if (k->killId >= 0 && k->killId == kills.front().id) opener = true;
    if (b.kills.empty() && !kills.empty()) opener = std::fabs(ks.front()->t - kills.front().t) < 0.5f;
    opener = opener && !openerSaid && !nogun && kills.size() > 1;   // the helm fired nothing; a lone kill opens no account
    if (opener) openerSaid = true;
    auto surnameOf = [](const std::string& m) { size_t sp = m.rfind(' '); return sp == std::string::npos ? m : m.substr(sp + 1); };
    auto shortName = [&](const std::string& m) {   // a mate already named in this paragraph: the surname, unless two share it
      int same = 0;
      for (auto& c : b.crew) if (surnameOf(c.name) == surnameOf(m)) ++same;
      for (auto& t : x.p->s) if (t.find(m) != std::string::npos) return same <= 1 ? surnameOf(m) : m;
      return m;
    };
    auto matesOf = [&](const RepEv& k) {   // no duplicate names, and never this crew member
      std::vector<std::string> ms;
      for (auto& m : k.mates) if (m != x.c->name && std::find(ms.begin(), ms.end(), m) == ms.end()) ms.push_back(m);
      return ms;
    };
    auto why = [&](bool solo) {
      int rc = RoleCat(x.c->role);
      std::string ws = rc == RC_HELM ? (solo ? "kill.why.helm.solo" : "kill.why.helm") : rc == RC_AMMO ? "kill.why.ammo" : rc == RC_REPAIR ? "kill.why.repair" : "";
      if (x.p->whySaid) return std::string();
      std::string w = ws.empty() ? "" : Draw(ws, x.allowH, false, true, x.p);
      if (!w.empty()) x.p->whySaid = true;
      return w.empty() ? w : ", " + w;
    };
    auto one = [&](const RepEv& k, const char* sec, const std::string& enemyAs = "") {
      Sent s;
      if (!enemyAs.empty()) s.v["enemy"] = enemyAs;   // "another watch tower": a second kill of the same name is not "it"
      else {   // one of several kills of the same name: "a Soukou-Tei gunboat", not "the" (an elite keeps its name)
        std::string kb = k.enemyBase.empty() ? BaseOf(k.enemy) : k.enemyBase;
        int same = 0;
        for (auto& o : kills) if (o.enemy == k.enemy) ++same;
        if (same > 1 && Prefix(k.enemy, kb).empty()) s.v["enemy"] = "a " + ShipOf(kb).singular;
      }
      s.t = k.t;
      s.enemy = k.enemy;
      s.base = k.enemyBase;
      std::vector<std::string> raw = matesOf(k), ms, all{x.last}, they{x.p->s.empty() ? x.last : x.c->female ? "she" : "he"};
      for (auto& m : raw) ms.push_back(shortName(m));
      all.insert(all.end(), ms.begin(), ms.end());
      they.insert(they.end(), ms.begin(), ms.end());
      s.v["mates"] = ms.size() > 3 ? Crowd(ms.size()) : JoinAnd(ms);
      s.v["all"] = ms.size() > 3 ? x.last + " and " + Crowd(ms.size()) : JoinAnd(all);
      s.v["they"] = ms.size() > 3 ? they[0] + " and " + Crowd(ms.size()) : JoinAnd(they);
      std::string sc = sec;
      if (IsPlane(k.enemyBase.empty() ? BaseOf(k.enemy) : k.enemyBase) && sc != "kill.nogun")
        sec = ms.empty() ? "kill.plane.solo" : "kill.plane.shared";   // nothing "sinks" an aircraft
      else if (IsGround(k.enemyBase.empty() ? BaseOf(k.enemy) : k.enemyBase) && (sc == "kill.solo" || sc == "kill.shared"))
        sec = ms.empty() ? "kill.ground.solo" : "kill.ground.shared";  // a shore position is knocked out
      if (std::string(sec) == "kill.shared" && raw.size() == 1 && k.killId >= 0) {
        auto it = sharedSaid.find(k.killId);
        if (it == sharedSaid.end()) sharedSaid[k.killId] = x.c->name;
        else if (it->second == raw[0]) { sec = "kill.shared.again"; s.v["mate"] = ms[0]; }
      }
      if (sc == "kill.nogun") {
        auto it = k.killId >= 0 ? nogunSaid.find(k.killId) : nogunSaid.end();
        if (ms.empty()) sec = "kill.nogun.solo";
        else if (it != nogunSaid.end()) { sec = "kill.nogun.again"; s.v["mate"] = it->second; }
        else if (k.killId >= 0) nogunSaid[k.killId] = x.last;
        s.v["why"] = why(ms.empty());
      }
      Say(x, sec, s, k.t);
    };
    const RepEv& k0 = *ks[0];
    bool shared0 = !matesOf(k0).empty();
    auto another = [&](int i) {
      std::string b1 = ks[i]->enemyBase.empty() ? BaseOf(ks[i]->enemy) : ks[i]->enemyBase;
      return i > 0 && ks[i]->enemy == ks[i - 1]->enemy && Prefix(ks[i]->enemy, b1).empty() ? "another " + ShipOf(b1).singular : std::string();
    };
    if (n == 1 || (nogun && n == 2)) {   // the helm: a sentence per kill, each with its own share or credit
      for (int i = 0; i < n; ++i)
        one(*ks[i], nogun ? "kill.nogun" : opener ? (shared0 ? "kill.opener.shared" : "kill.opener.solo") : (shared0 ? "kill.shared" : "kill.solo"), another(i));
      return n;
    }
    if (opener) one(k0, shared0 ? "kill.opener.shared" : "kill.opener.solo");
    if (n <= 4) {
      int from = opener ? 1 : 0;   // the opener sentence told the first one
      {   // the same 2-4 kills as an earlier paragraph (the other gunner of a twin mount): not the list again
        std::set<int> ids;
        for (auto* k : ks) if (k->killId >= 0) ids.insert(k->killId);
        if ((int)ids.size() == n) {
          auto same = killSets.find(ids);
          if (same == killSets.end()) killSets[ids] = x.c->name;
          else {
            Sent s; int dup = 0;
            for (auto& c : b.crew) if (surnameOf(c.name) == surnameOf(same->second)) ++dup;
            s.v["other"] = dup > 1 ? same->second : surnameOf(same->second);
            s.v["n"] = Num(n); s.v["N"] = CapFirst(Num(n));
            Say(x, "kill.same", s, -1);
            return n;
          }
        }
      }
      if (n - from == 1) {
        one(*ks[1], matesOf(*ks[1]).empty() ? "kill.solo" : "kill.shared", another(1));
        return n;
      }
      struct G { std::string enemy, base; int n = 0; float t = 0, tEnd = 0; };
      std::vector<G> gs;   // "two watch towers and a machine-gun nest", not "a watch tower and another watch tower"
      for (int i = from; i < n; ++i) {
        auto it = std::find_if(gs.begin(), gs.end(), [&](const G& g) { return g.enemy == ks[i]->enemy; });
        if (it != gs.end()) { ++it->n; it->tEnd = ks[i]->t; continue; }
        gs.push_back(G{ks[i]->enemy, ks[i]->enemyBase.empty() ? BaseOf(ks[i]->enemy) : ks[i]->enemyBase, 1, ks[i]->t, ks[i]->t});
      }
      std::vector<std::string> items;
      for (size_t i = 0; i < gs.size(); ++i) {
        std::string it = gs[i].n == 1 ? Article(x, gs[i].enemy, gs[i].base) : Num(gs[i].n) + " " + ShipOf(gs[i].base).plural;
        if (gs.size() == 1 && gs[i].n == n) it = (n == 2 ? "both " : "all ") + ShipOf(gs[i].base).plural;   // "three kills: all watch towers"
        if (i == 0 && !opener) {   // a group spread over time: the time is the first one's
          Sent ws; ws.t = gs[i].t;
          std::string w = When(x, ws);
          bool spread = gs[i].n > 1 && gs[i].tEnd - gs[i].t >= 5;
          it += spread ? ", the first " + w + (gs.size() == 2 ? "," : "") : " " + w;
        }
        items.push_back(it);
      }
      Sent s;
      s.v["list"] = JoinAnd(items);
      s.v["List"] = CapFirst(s.v["list"]);
      s.v["n"] = Num(n);
      s.v["N"] = CapFirst(Num(n));
      Say(x, opener ? "kill.few.more" : "kill.few", s, ks.back()->t);
      return n;
    }
    // 5 or more: counts per type, top 3
    struct T { std::string base; int n = 0, solo = 0; float t = 0; };
    std::vector<T> types;
    std::map<std::string, int> partner;
    std::vector<std::string> partnerOrder;
    int soloN = 0;
    for (auto* k : ks) {
      std::string base = k->enemyBase.empty() ? BaseOf(k->enemy) : k->enemyBase;
      auto it = std::find_if(types.begin(), types.end(), [&](const T& t) { return t.base == base; });
      if (it == types.end()) { types.push_back(T{base, 0, 0, k->t}); it = types.end() - 1; }
      ++it->n;
      if (matesOf(*k).empty()) { ++it->solo; ++soloN; }
      for (auto& m : matesOf(*k)) { if (!partner.count(m)) partnerOrder.push_back(m); ++partner[m]; }
    }
    std::stable_sort(types.begin(), types.end(), [](const T& a, const T& z) { return a.n > z.n; });
    auto baseOfK = [&](const RepEv* k) { return k->enemyBase.empty() ? BaseOf(k->enemy) : k->enemyBase; };
    std::vector<std::string> items;
    std::set<std::string> named;   // types the list names; the plane and big sentences leave them out
    std::vector<size_t> left;
    int leftN = 0;
    auto item = [&](const T& t) {   // the opener's one-off is "the floatplane": named already, but counted
      Ship sh = ShipOf(t.base);
      return t.n == 1 ? (opener && t.base == baseOfK(ks[0]) ? "the " : "a ") + sh.singular : Num(t.n) + " " + sh.plural;
    };
    for (size_t i = 0; i < types.size(); ++i) {
      if (opener && types[i].n == 1 && types[i].base == baseOfK(ks[0])) named.insert(types[i].base);   // "the floatplane" later
      if (i < 3) { items.push_back(item(types[i])); named.insert(types[i].base); continue; }
      left.push_back(i);
      leftN += types[i].n;
    }
    if (left.size() <= 2) for (size_t i : left) { items.push_back(item(types[i])); named.insert(types[i].base); }   // "a tanker and a cruiser", not "two others"
    else if (leftN) items.push_back(Num(leftN) + (leftN == 1 ? " other" : " others"));
    if (types.size() == 1 && Trim(types[0].base).empty()) items.clear();   // "five kills", not "five kills: five enemy vessels"
    std::string sameAs;
    {
      Sent s;
      s.v["typeList"] = JoinAnd(items);
      s.v["n"] = Num(n);
      s.v["N"] = CapFirst(Num(n));
      std::set<int> ids;
      for (auto* k : ks) if (k->killId >= 0) ids.insert(k->killId);
      auto same = (int)ids.size() == n ? killSets.find(ids) : killSets.end();
      if ((int)ids.size() == n && same == killSets.end()) killSets[ids] = x.c->name;
      if (same != killSets.end()) {   // the list is in the earlier paragraph
        sameAs = same->second;
        int dup = 0;
        for (auto& c : b.crew) if (surnameOf(c.name) == surnameOf(sameAs)) ++dup;
        s.v["other"] = dup > 1 ? sameAs : surnameOf(sameAs);
        Say(x, "kill.same", s, -1);   // the rows say no time: the next {when} is not "the same minute" as an unseen kill
      } else if (nogun) {
        s.v["list"] = items.empty() ? "" : ": " + s.v["typeList"];
        std::string w = why(soloN == n);
        s.v["Why"] = w.empty() ? w : CapFirst(w.substr(2));
        Say(x, w.empty() ? "kill.nogun.many" : "kill.nogun.many.why", s, -1);
      } else {
        std::string gl = GunLabel(x);
        for (auto& c : b.crew) { Ctx y; y.c = &c; if (c.name != x.c->name && GunLabel(y) == gl) x.noGunRows = true; }   // two guns of one name
        Say(x, "kill.many", s, -1);
        x.noGunRows = false;
      }
    }
    std::set<std::string> aloneNamed;   // solo kills [kill.alone] named by type: [kill.big] does not tell them again
    std::set<std::string> everyNamed;   // partners in on every shared kill, named just before: [kill.big] does not name them again
    if (soloN == n) { Sent s; Say(x, "kill.allalone", s, -1); }
    else {
      if (soloN > 0) {
        std::vector<std::string> solo;
        for (auto& t : types) {
          if (!t.solo) continue;
          Ship sh = ShipOf(t.base);
          solo.push_back(t.solo == 1 ? (t.n > 1 ? "one " + sh.singular : (named.count(t.base) ? "the " : "a ") + sh.singular) : Num(t.solo) + " " + sh.plural);
          aloneNamed.insert(t.base);
        }
        Sent s;
        s.v["solo"] = JoinAnd(solo);
        s.v["waswere"] = soloN == 1 ? "was" : "were";
        Say(x, nogun ? "kill.nogun.alone" : "kill.alone", s, -1);
      }
      std::vector<std::pair<std::string, int>> ps;
      for (auto& m : partnerOrder) if (partner[m] * 3 >= n && m != sameAs) ps.push_back({m, partner[m]});   // [kill.same] named that one
      std::stable_sort(ps.begin(), ps.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& z) { return a.second > z.second; });
      if (!ps.empty() && sameAs.empty()) {   // after [kill.same] the earlier paragraph named the partners
        std::vector<std::string> every, names;   // every: in on all the shared kills, so not "mostly"
        for (auto& p : ps) {
          if (p.second == n - soloN) { every.push_back(p.first); everyNamed.insert(p.first); }
          else names.push_back(p.first + " (" + Num(p.second) + ")");
        }
        Sent s;
        if (!every.empty()) {
          s.v["partners"] = JoinAnd(every);
          s.v["waswere"] = every.size() == 1 ? "was" : "were";
          s.v["more"] = names.empty() ? "" : "; " + JoinAnd(names) + " had a share too";
          Say(x, soloN ? "kill.partners.every" : "kill.partners.all.every", s, -1);
        } else {
          s.v["partners"] = JoinAnd(names);
          s.v["waswere"] = names.size() == 1 ? "was" : "were";
          Say(x, soloN ? "kill.partners" : "kill.partners.all", s, -1);
        }
      }
    }
    int namedRank = 1 << 30;   // a big kill must outrank every type the list already names
    for (auto& t : named) if (BigRank(t) >= 0) namedRank = (std::min)(namedRank, BigRank(t));
    const RepEv* big = nullptr;
    int bigRank = 1 << 30;
    bool planeDone = false;
    for (auto* k : ks) {
      if (opener && k == ks[0]) continue;                               // the opener sentence told it
      std::string base = baseOfK(k);
      if (named.count(base) || aloneNamed.count(base)) continue;
      if (!planeDone && IsPlane(base) && sameAs.empty()) {   // [kill.same]: the earlier paragraph told the plane
        Sent s;
        s.v["plane"] = Article(x, k->enemy, base);
        Say(x, "kill.plane", s, -1);
        planeDone = true;
      } else if (!IsPlane(base) && BigRank(base) >= 0 && BigRank(base) < bigRank) { big = k; bigRank = BigRank(base); }
    }
    if (big && (bigRank >= namedRank || (matesOf(*big).empty() && aloneNamed.count(baseOfK(big))))) big = nullptr;
    if (big && big->killId >= 0 && !bigSaid.insert(big->killId).second) big = nullptr;   // told in an earlier paragraph
    if (big) {
      Sent s;
      s.t = big->t;
      s.v["big"] = Article(x, big->enemy, baseOfK(big));
      std::vector<std::string> ms;
      bool told = true;
      for (auto& m : matesOf(*big)) { ms.push_back(shortName(m)); if (!everyNamed.count(m)) told = false; }
      if (told) ms.clear();
      s.v["mates"] = ms.size() > 3 ? Crowd(ms.size()) : JoinAnd(ms);
      Say(x, ms.empty() ? "kill.big" : "kill.big.shared", s, big->t);
    }
    return n;
  }

  void RenderCrew(std::vector<RepPara>& out, const RepCrew& c, int idx, const std::string& last, const std::map<std::string, int>& lastN) {
    Ctx x;
    x.c = &c;
    medicNow = RoleCat(c.role) == RC_MEDIC;
    x.idx = idx;
    x.last = last;
    x.role = c.role;
    x.allowH = c.outcome != O_KIA && c.outcome != O_MISSING && c.outcome != O_CAPTURED;
    for (auto& e : c.ev) if (e.kind == S_KILLED || e.kind == S_SANK || e.kind == S_DIED_WOUNDS) x.allowH = false;
    out.push_back({1, Upper(NickName(c.name, c.nick)) + " - " + RoleNoun(c)});
    if (!c.tookDead.empty() && c.tookJob >= RC_AMMO && c.tookJob <= RC_HELM && (c.tookJob != RC_GUN || !c.tookGun.empty())) {   // the dead man's station: its own one-line paragraph
      static const char* kJob[] = {"ammo", "repair", "medic", "", "helm"};
      Sent s;
      size_t sp = c.tookDead.rfind(' ');
      std::string dl = sp == std::string::npos ? c.tookDead : c.tookDead.substr(sp + 1);
      s.v["dead"] = lastN.count(dl) ? c.tookDead : dl;                // a Gray aboard too: the dead Gray by full name
      s.v["oldgun"] = c.tookGun;
      std::string t = Line(x, c.tookJob == RC_GUN ? "station.gun" : std::string("station.") + kJob[c.tookJob], s);   // x.p unset: Draw and Fill do not need it
      if (!t.empty()) out.push_back({0, CapFirst(FixArticles(Clean(t)))});
    }

    // fold the events: fire episodes, down/overboard within 2 s of a hit
    std::vector<Item> items;
    std::vector<float> wounded;
    std::vector<const RepEv*> ks;
    std::set<int> killIds;
    float worstOne = 0, lastEvT = -1;
    bool roleChanged = false;
    for (size_t i = 0; i < c.ev.size(); ++i) {
      const RepEv& e = c.ev[i];
      lastEvT = (std::max)(lastEvT, e.t);
      Unit* lu = !items.empty() && items.back().type == 0 ? &items.back().u : nullptr;
      if (e.kind <= R_OTHER) {
        worstOne = (std::max)(worstOne, e.lost);
        if (e.kind == R_FIRE && lu && lu->kind == R_FIRE && e.t - lu->tEnd <= 10 && !lu->fDown && !lu->fOver) {
          lu->lost += e.lost;
          lu->hpLeft = e.hpLeft;
          lu->tEnd = e.t;
          lu->tier = (std::max)(lu->tier, e.tier);
          if (c.maxHp > 0) lu->tier = (std::max)(lu->tier, lu->lost >= 0.25f * c.maxHp ? 3 : lu->lost >= 0.1f * c.maxHp ? 2 : 1);
          worstOne = (std::max)(worstOne, lu->lost);
          continue;
        }
        Item it;
        it.ev = (int)i;
        it.t = e.t;
        Unit& u = it.u;
        u.t = u.tEnd = e.t;
        u.kind = e.kind;
        u.tier = e.tier;
        u.ev = (int)i;
        u.allPair = e.kind == R_PAIR;
        bool noEnemy = e.kind == R_TORPEDO || e.kind == R_FIRE || e.kind == R_ABOARD;
        u.enemy = noEnemy ? "" : e.enemy;
        u.base = noEnemy ? "" : (e.enemyBase.empty() ? BaseOf(e.enemy) : e.enemyBase);
        u.shell = e.shell;
        u.part = e.part;
        u.part2 = e.part2;
        u.lost = u.worstOne = e.lost;
        u.hpLeft = e.hpLeft;
        u.plains.push_back(PlainOrDraw(e.part, Hash(b.seed, (uint32_t)idx, (uint32_t)i, 3)));
        items.push_back(it);
      } else if (e.kind == S_WOUNDED) wounded.push_back(e.t);
      else if (e.kind == K_KILL) { if (e.killId < 0 || killIds.insert(e.killId).second) ks.push_back(&e); }
      else if ((e.kind == S_DOWN && lu && !lu->fDown && e.t - lu->tEnd <= 2) ||
               ((e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST) && lu && !lu->fOver && e.t - lu->tEnd <= 2)) {
        if (e.kind == S_DOWN) lu->fDown = true;
        else { lu->fOver = true; lu->fOverDown = e.whileDown; lu->fOverHit = e.kind == S_OVER_HIT; }
        lu->fT = e.t;
      } else {
        if (e.kind == S_ROLE) roleChanged = true;
        Item it;
        it.type = 1;
        it.ev = (int)i;
        it.t = e.t;
        items.push_back(it);
      }
    }
    for (auto& g : c.grazes) lastEvT = (std::max)(lastEvT, g.t);
    // clusters: one enemy, gaps of 20 s or less, at most 5; a fold-in ends a cluster
    std::vector<Item> m;
    auto clusterable = [](int k) { return k != R_FIRE && k != R_TORPEDO && k != R_ABOARD; };
    for (auto& it : items) {
      if (it.type == 0 && !m.empty() && m.back().type == 0) {
        Unit& a = m.back().u;
        const Unit& u = it.u;
        if (!a.enemy.empty() && a.enemy == u.enemy && clusterable(a.kind) && clusterable(u.kind) && !a.fDown && !a.fOver &&
            u.t - a.tEnd <= 20 && a.n < 5) {
          ++a.n;
          a.tEnd = u.t;
          a.lost += u.lost;
          a.hpLeft = u.hpLeft;
          a.tier = (std::max)(a.tier, u.tier);
          a.allPair = a.allPair && u.kind == R_PAIR;
          a.plains.insert(a.plains.end(), u.plains.begin(), u.plains.end());
          a.fDown = u.fDown; a.fOver = u.fOver; a.fOverDown = u.fOverDown; a.fOverHit = u.fOverHit; a.fT = u.fT;
          continue;
        }
      }
      m.push_back(it);
    }
    // a "wounded" state no narrated hit explains goes into the graze sentence
    float orphan = -1;
    for (float w : wounded) {
      bool cov = false;
      for (auto& it : m) if (it.type == 0 && w >= it.u.t - 1 && w <= it.u.tEnd + 10) cov = true;
      if (!cov) { orphan = w; break; }
    }
    Grazes z = ReadGrazes(c);
    float srcT = 0;
    std::string src = DeckSrc(c, z, &srcT);
    bool rakeUsed = false;   // a rake rides on a narrated cluster it overlaps, else it opens the graze sentence
    if (z.rn)
      for (auto& it : m)
        if (it.type == 0 && it.u.n > 1 && z.r0 <= it.u.tEnd && z.r1 >= it.u.t) { it.u.rake = z.rake; rakeUsed = true; break; }
    bool grazeS = z.g > 0 || orphan >= 0;
    bool fireLine = z.fireN > 0 && !fireShared;
    for (auto& e : c.ev) if (e.kind == R_FIRE) fireLine = false;      // a fire wound is told already
    // overflow: P-fire keeps at most 6 sentences. State events and overboard hits (a rescue refers back to them) always
    // stay, and so do the first and the last two "went down" hits; the other hits get what is left (one sentence goes to
    // the summary): the first hit, then the rake cluster, then the worst by HP
    {
      std::vector<int> units, downs;
      std::set<int> keep;
      int fixedN = 0;
      for (int i = 0; i < (int)m.size(); ++i) {
        if (m[i].type != 0) { ++fixedN; continue; }
        units.push_back(i);
        if (m[i].u.fOver) keep.insert(i);
        else if (m[i].u.fDown) downs.push_back(i);
      }
      for (size_t k = 0; k < downs.size(); ++k) if (k == 0 || k + 2 >= downs.size()) keep.insert(downs[k]);
      if (!units.empty()) keep.insert(units[0]);
      fixedN += (int)keep.size();
      int budget = (std::max)(0, 6 - fixedN - (grazeS ? 1 : 0) - 1);   // more hits on top of the kept ones
      int droppable = (int)units.size() - (int)keep.size();
      if ((int)m.size() + (grazeS ? 1 : 0) > 6 && droppable - budget >= 2) {
        std::vector<int> pri;
        for (int i : units) if (!keep.count(i) && !m[i].u.rake.empty()) pri.push_back(i);
        std::vector<int> byLost;
        for (int i : units) if (!keep.count(i)) byLost.push_back(i);
        std::stable_sort(byLost.begin(), byLost.end(), [&](int p1, int p2) { return m[p1].u.lost > m[p2].u.lost; });
        pri.insert(pri.end(), byLost.begin(), byLost.end());
        int extra = 0;
        for (int i : pri) if (extra < budget && keep.insert(i).second) ++extra;
        Item ov;
        ov.type = 2;
        ov.ovT1 = 1e9f;
        std::map<std::string, int> partN;
        int firstDrop = -1;
        std::set<int> dropUp;   // the "got up" after a left-out "went down" hit goes with it
        for (int i : units) {
          if (keep.count(i)) continue;
          if (m[i].u.fDown)
            for (int j = i + 1; j < (int)m.size(); ++j) {
              if (m[j].type == 0 && m[j].u.fDown) break;
              if (m[j].type == 1 && c.ev[m[j].ev].kind == S_UP) { dropUp.insert(j); break; }
            }
          const Unit& u = m[i].u;
          ov.ovN += u.n;
          ov.ovT1 = (std::min)(ov.ovT1, u.t);
          ov.ovT2 = (std::max)(ov.ovT2, u.tEnd);
          for (auto& pp : u.plains) ++partN[pp];
          if (!m[i].u.rake.empty()) rakeUsed = false;
          if (firstDrop < 0) firstDrop = i;
        }
        int best = 0;
        for (auto& kv : partN) { ov.ovPartTot += kv.second; if (kv.second > best) { best = kv.second; ov.ovPart = kv.first; } }
        ov.ovPartN = best;
        ov.t = ov.ovT2;
        std::vector<Item> m2;   // the summary follows the kept items inside its span; [overflow.end] only when no hit follows
        bool ovDone = false;
        for (int i = 0; i < (int)m.size(); ++i) {
          bool kept = (m[i].type != 0 || keep.count(i)) && !dropUp.count(i);
          float ti = m[i].type == 0 ? m[i].u.t : m[i].t;
          int k = m[i].type == 1 ? c.ev[m[i].ev].kind : -1;
          bool fate = k == S_KILLED || k == S_SANK || k == S_DIED_WOUNDS || k == S_PICKED || k == S_PRISONER;
          if (!ovDone && i > firstDrop && kept && (ti > ov.ovT2 || fate)) { m2.push_back(ov); ovDone = true; }
          if (kept) m2.push_back(m[i]);
        }
        if (!ovDone) m2.push_back(ov);
        m = m2;
      }
    }
    bool deathSaid = sankCrew.count(idx) > 0;                         // the outcome line ends a sinking
    for (auto& e : c.ev) if (e.kind == S_PICKED || e.kind == S_SANK || e.kind == S_PRISONER) deathSaid = true;
    bool lost = c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED;
    int pfCount = (int)m.size() + (grazeS ? 1 + (m.empty() ? 1 : 0) : 0) + (!deathSaid && (c.outcome == O_MISSING || c.outcome == O_CAPTURED));
    // kills first when they outnumber the fire sentences, or when the fire paragraph ends the story (lost crew)
    bool killsFirst = !ks.empty() && ((int)ks.size() >= pfCount || lost);

    auto start = [&](Para& p) { x.p = &p; x.prevEnemy.clear(); x.curEnemy.clear(); x.prevSubj.clear(); x.curSubj.clear(); };
    Para pf, pk;
    int swarmTier = 0;   // the [swarm.cost.N] the splinters drew: the outcome line must not play them down
    std::function<void()> sayGrazes;   // the graze sentence; renderFire places it in time
    auto renderFire = [&]() {
      start(pf);
      bool firePending = fireLine;
      auto sayFire = [&]() {
        firePending = false;
        Sent s;
        s.t = z.fireT;
        bool late = pf.lastT > z.fireT + 1;
        Say(x, late ? "graze.fire.late" : "graze.fire", s, late ? -1 : z.fireT);
      };
      // the graze sentence: a rake at its own time (before the hits that came after it), anything else at the end, and
      // always before the fate ("He was killed", "He was not found after the action")
      bool shRake = z.g <= 20 && !src.empty() && !rakeUsed && src == sharedRake && orphan < 0;
      float grazeT = z.g > 20 || m.empty() ? 1e9f : shRake ? srcT : z.rn && !rakeUsed ? z.r0 : 1e9f;
      bool grazePending = grazeS;
      for (size_t i = 0; i < m.size(); ++i) {
        Item& it = m[i];
        float ti = it.type == 0 ? it.u.t : it.t;                         // an overflow by the end of its span
        int k = it.type == 1 ? c.ev[it.ev].kind : -1;
        bool death = k == S_KILLED || k == S_SANK || k == S_DIED_WOUNDS;
        if (firePending && (death || (!x.wasDown && ti > z.fireT))) sayFire();   // never after the death sentence
        if (grazePending && (ti > grazeT || (k == S_ROLE && z.rn && !rakeUsed && ti > z.r1 && !pf.s.empty()) || ((death || k == S_PRISONER) && !pf.s.empty()))) { grazePending = false; sayGrazes(); }
        if (it.type == 0) {
          int nk = i + 1 < m.size() && m[i + 1].type == 1 ? c.ev[m[i + 1].ev].kind : -1;
          it.u.soonAway = (nk == S_SIDE || nk == S_DOWN || nk == S_BLEED || nk == S_OVER_HIT || nk == S_OVER_BLAST || nk == S_KILLED) &&
                          m[i + 1].t - it.u.tEnd <= 10;
          RenderUnit(x, it.u);
        }
        else if (it.type == 2) {
          Sent s;
          int m1 = (int)(it.ovT1 / 60) + 1, m2 = (int)(it.ovT2 / 60) + 1;
          s.v["n"] = Num(it.ovN);
          s.v["between"] = m1 == m2 ? "in the " + Ordinal(m1) + " minute" : "between the " + Ordinal(m1) + " and the " + Ordinal(m2) + " minute";
          s.v["toppart"] = it.ovPart.empty() ? "body" : it.ovPart;
          // "most of them in the leg" only for a real majority: two hits are "both", a tie names no part
          s.v["mostin"] = it.ovPart.empty() || it.ovPartN * 2 <= it.ovPartTot ? std::string()
                          : it.ovPartN == it.ovPartTot ? (it.ovPartTot == 2 ? ", both in the " : ", all of them in the ") + it.ovPart
                          : ", most of them in the " + it.ovPart;
          s.v["noneN"] = it.ovN == 2 ? "neither" : "none";
          bool hitAfter = false;
          for (size_t j = i + 1; j < m.size(); ++j) if (m[j].type == 0) hitAfter = true;
          Say(x, hitAfter ? "overflow" : "overflow.end", s, -1);
          pf.lastT = -1;   // the next sentence takes an absolute time
          pf.lastRel.clear();
        } else RenderState(x, c.ev[it.ev], i + 1 < m.size() ? &m[i + 1] : nullptr);
      }
      if (firePending) sayFire();
      if (grazePending) sayGrazes();
      if (!deathSaid && c.outcome == O_MISSING) { Sent s; Say(x, "death.missing", s, -1); }
      if (!deathSaid && c.outcome == O_CAPTURED) { Sent s; Say(x, "death.captured", s, -1); }
    };
    sayGrazes = [&]() {
      if (!grazeS) return;
      if (z.g > 20) {                                                  // a swarm: how much it cost, not a count
        float r = c.maxHp > 0 ? z.hp / c.maxHp : 0;
        std::string his = c.female ? "her" : "his", him = c.female ? "her" : "him";
        Sent s;
        Sent cs;
        bool dead = c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED;
        bool gone = !x.allowH || dead;   // no "stitches afterwards"
        swarmTier = r < 0.05f ? 1 : r < 0.15f ? 2 : r < 0.3f ? 3 : 4;
        // grazes heal during the battle, so their sum overstates the result: the outcome caps the tier
        float rEnd = c.maxHp > 0 ? c.endHp / c.maxHp : 1.f;
        if (!dead) swarmTier = (std::min)(swarmTier, c.outcome == O_LIGHT || c.outcome == O_UNHURT ? 2 : rEnd > 0.6f ? 3 : 4);
        s.v["hurt"] = Trim(Line(x, "swarm.cost." + std::to_string(swarmTier) + (gone ? ".lost" : ""), cs));
        bool sh = !src.empty() && !rakeUsed && src == sharedRake;
        if (sh && m.empty()) RoleVars(x, c.role, s);
        Say(x, !sh ? "graze.swarm" : m.empty() ? "graze.shared.swarm.only" : gone ? "graze.shared.swarm.lost" : "graze.shared.swarm", s, -1);
        return;
      }
      bool shared = !src.empty() && !rakeUsed && src == sharedRake && orphan < 0;
      if (shared) {
        Sent s;
        int rn = GrazesFrom(c, src), more = z.g - rn;   // {G} the rake's splinters; the rest get their own sentence
        SetG(s, rn);
        if (m.empty()) RoleVars(x, c.role, s);
        // the raking came before every narrated hit: this sentence opens the paragraph
        bool before = !m.empty() && x.p->s.empty();
        Say(x, std::string(m.empty() ? "graze.shared.only" : before ? "graze.shared.before" : "graze.shared") + (rn == 1 ? ".one" : ""), s, -1);
        if (more > 0 && !before) {   // not between "by then" and the hit it points at
          Sent ms; ms.v["M"] = CapFirst(Num(more)); ms.v["m"] = Num(more);
          std::set<std::string> others;   // "two more came from a patrol boat", not "other enemies", when there was one
          for (auto& g : c.grazes) if (g.enemy != src) others.insert(g.enemy);
          bool oneEnemy = more > 1 && others.size() == 1 && !Trim(*others.begin()).empty();
          if (oneEnemy) ms.v["enemy"] = Mention(x, *others.begin(), BaseOf(*others.begin()), false, false);
          Say(x, more == 1 ? "graze.shared.more.one" : oneEnemy ? "graze.shared.more.oneenemy" : "graze.shared.more", ms, -1); }
        return;
      }
      if (m.empty() && !z.rn && orphan < 0 && z.g > 0) {   // only grazed: one sentence, no "besides" after an empty lead
        Sent s;
        SetG(s, z.g);
        RoleVars(x, c.role, s);
        Say(x, z.g == 1 ? "graze.only.one" : "graze.only", s, -1);
        return;
      }
      if (m.empty()) { Sent s; RoleVars(x, c.role, s); Say(x, "graze.lead", s, -1); }
      Sent s;
      s.enemy = z.top;
      s.base = BaseOf(z.top);
      SetG(s, z.g);
      const char* sec = z.g == 0 ? "graze.none" : z.g == 1 ? "graze.one" : z.g <= 5 ? "graze.few" : "graze.many";
      if (z.rn && !rakeUsed) {
        sec = "graze.rake";
        s.t = z.r0;
        s.v["rake"] = Mention(x, z.rake, BaseOf(z.rake), false, false);
        int rTot = 0;   // {R}: the raking enemy's own splinters, {G} is every graze
        for (auto& g : c.grazes) if (g.enemy == z.rake) ++rTot;
        s.v["R"] = Num(rTot);
        s.v["more"] = z.g > rTot ? ", with " + Num(z.g - rTot) + " more from elsewhere" : "";
      }
      x.nameDue = x.p->s.empty();   // told at the rake's time, it can open the paragraph
      std::string body = Line(x, sec, s);
      x.nameDue = false;
      std::vector<std::string> cl;
      if (z.g > 0 && worstOne > 0 && c.maxHp > 0 && z.hp >= 0.2f * c.maxHp && z.hp > worstOne) {
        Sent bs;
        std::string t = Trim(Line(x, "graze.blood", bs));
        if (!t.empty()) cl.push_back(t[0] == ',' || t[0] == ';' ? t : "; " + t);
      }
      if (orphan >= 0) { Sent ws; ws.t = orphan; cl.push_back(Line(x, "graze.wounded", ws)); }
      if (!body.empty()) Finish(x, Attach(body, cl), -1);
    };
    auto renderKills = [&]() { start(pk); RenderKills(x, ks); };
    if (killsFirst) { renderKills(); renderFire(); } else { renderFire(); renderKills(); }

    // the outcome line
    Para po;
    start(po);
    bool quiet = c.outcome == O_UNHURT && c.ev.empty() && c.grazes.empty();
    bool rakeOnly = c.outcome == O_LIGHT && m.empty() && ks.empty() && !sharedRake.empty() && src == sharedRake && !rakeUsed && orphan < 0;
    if (rakeOnly) {
    } else if (quiet) {
      Sent s;
      int cat = RoleCat(c.role);
      bool others = false;   // a medic whose kit "stayed packed" while others were hurt reads wrong
      for (auto& o : b.crew) if (o.outcome != O_UNHURT || !o.grazes.empty()) others = true;
      Say(x, cat == RC_MEDIC && others ? "quiet.medic.busy" : QuietSec(cat), s, -1);
    } else {
      Sent s;
      s.v["frac"] = Frac(c);
      s.v["hpline"] = HpLine(c);
      float r = c.maxHp > 0 ? c.endHp / c.maxHp : 1.f;
      uint32_t h = Hash(b.seed, (uint32_t)idx, 9);
      std::string sec, plain;   // plain: the section a .nick one falls back to when it gives nothing
      switch (c.outcome) {
        case O_KIA: plain = !GunLabel(x).empty() && (h & 1) ? "out.kia.gun" : "out.kia"; sec = !c.nick.empty() ? "out.kia.nick" : plain; break;
        case O_MISSING: plain = "out.missing"; sec = sankCrew.count(idx) ? "out.missing.sank" : !c.nick.empty() ? "out.missing.nick" : plain; break;
        case O_CAPTURED: sec = sankCrew.count(idx) ? "out.captured.sank" : "out.captured"; break;
        case O_LOST_PICKED: {
          bool told = false;
          for (auto& e : c.ev) if (e.kind == S_PICKED) told = true;   // the paragraph ends with the pickup: not again
          sec = sankCrew.count(idx) || sinkT >= 0 ? "out.picked.sank" : !told ? "out.picked" : r < 0.75f || swarmTier >= 3 ? "out.picked.told.wounded" : "out.picked.told";
          break;
        }
        case O_DOWN_AT_END: sec = "out.downend"; break;
        case O_LIGHT: sec = "out.light"; break;
        case O_WOUNDED: sec = r < 0.25f && (h & 1) ? "out.wounded.bad" : r > 0.75f || (r > 0.5f && h % 3 == 0) ? "out.wounded.ok" : "out.wounded"; break;
        default: sec = ks.empty() ? "out.unhurt" : "out.unhurt.kills"; break;
      }
      bool allowH = x.allowH;
      if (swarmTier >= 4) x.allowH = false;   // no joke after "between them they bled him badly"
      if (anyLost && Starts(sec, "out.picked")) x.allowH = false;   // nor about the swim when others were lost at sea
      std::string t = Line(x, sec, s);
      if (t.empty() && Ends(sec, ".nick")) t = Line(x, plain, s);
      x.allowH = allowH;
      if (c.outcome == O_WOUNDED && sec == "out.wounded" && !roleChanged && b.len > 0 && lastEvT >= 0.8f * b.len && postN < 3 &&
          !Starts(t, "Came home") && !Starts(t, "Brought home")) {   // "came home ... and never left his post" reads backwards
        ++postN;
        bool away = false;   // down or in the water at some point: he did not "never leave his post"
        for (auto& e : c.ev) if (e.kind == S_DOWN || e.kind == S_BLEED || e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST || e.kind == S_SIDE) away = true;
        Sent ps;
        t = Attach(t, {Line(x, away ? "out.post.back" : "out.post", ps)});
      }
      if (!t.empty()) Finish(x, t, -1);
    }

    auto join = [](const Para& p) {
      std::string r;
      for (auto& s : p.s) r += (r.empty() ? "" : " ") + s;
      return r;
    };
    std::string fire = join(pf), kill = join(pk), outc = join(po);
    if (killsFirst) std::swap(fire, kill);
    for (auto* t : {&fire, &kill, &outc}) if (!t->empty()) out.push_back({0, *t});
  }

  std::vector<RepPara> Render() {
    std::vector<RepPara> out;
    out.push_back({2, "ACTION REPORT"});
    // surnames; a shared surname falls back to the full name
    std::map<std::string, int> lastN;
    auto surname = [](const std::string& n) {
      size_t p = n.rfind(' ');
      return p == std::string::npos || n == "a crew member" ? n : n.substr(p + 1);
    };
    for (auto& c : b.crew) ++lastN[surname(c.name)];
    anyLost = false;
    bool allQuiet = kills.empty();
    for (auto& c : b.crew) {
      if (c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED) anyLost = true;
      if (c.outcome != O_UNHURT || !c.ev.empty() || !c.grazes.empty()) allQuiet = false;
    }
    RepCrew dummy;
    Ctx x;
    x.c = &dummy;
    x.idx = -1;
    x.allowH = !anyLost;
    Para ps;
    x.p = &ps;
    std::string crewN = Num((int)b.crew.size());
    if (allQuiet) {
      Sent s;
      s.v["dur"] = Dur(b.len);
      s.v["crewN"] = crewN;
      std::string t = Line(x, "summary.quiet", s);
      if (!t.empty()) out.push_back({0, CapFirst(FixArticles(Clean(t)))});
      std::vector<std::string> names;
      for (auto& c : b.crew) names.push_back(NickName(c.name, c.nick) + " (" + RoleNoun(c) + ")");
      Sent a;
      a.v["crewlist"] = names.empty() ? "nobody" : "";
      for (size_t i = 0; i < names.size(); ++i) a.v["crewlist"] += (i ? ", " : "") + names[i];
      Para pa;
      x.p = &pa;
      Say(x, "summary.aboard", a, -1);
      if (!pa.s.empty()) out.push_back({0, pa.s[0]});
      return out;
    }
    // 1. duration and enemy
    {
      int hm = RoundI(b.len / 30), r = hm / 2;   // to the nearest half minute: 91 s is "a minute and a half", not "nearly two"
      bool half = b.len >= 75 && b.len < 165 && (hm & 1);   // longer actions keep whole minutes
      if (!half) r = RoundI(b.len / 60);
      float diff = b.len - (r * 60.f + (half ? 30.f : 0.f));
      std::string d = r <= 0 ? "less than a minute" : (diff < -5 ? "nearly " : diff > 5 ? "just over " : "") +
                                                          (half ? (r == 1 ? std::string("a minute and a half") : Num(r) + " and a half minutes")
                                                                : r == 1 ? std::string("a minute") : Num(r) + " minutes");
      std::map<std::string, int> cnt;
      std::map<std::string, float> dmg;
      std::map<std::string, std::string> sing;
      std::map<std::string, int> rank;
      std::map<std::string, bool> allElite;
      std::vector<std::string> order;
      bool allGround = true;
      auto key = [&](const std::string& enemy, const std::string& base0) {
        if (enemy.empty() || enemy == "fire" || enemy == "our own guns") return std::string();
        std::string base = base0.empty() ? BaseOf(enemy) : base0;
        Ship sh = ShipOf(base);
        std::string pl = sh.plural, sg = sh.singular;   // by type: "armored Ka-Mi tanks" count as Ka-Mi tanks
        if (Starts(pl, "armored ")) pl = pl.substr(8);
        if (Starts(sg, "armored ")) sg = sg.substr(8);
        bool el = Starts(pl, "elite ");   // "elite D3A dive bombers and a D3A dive bomber" are D3A dive bombers
        if (el) pl = pl.substr(6);
        if (Starts(sg, "elite ")) sg = sg.substr(6);
        if (!cnt.count(pl)) { order.push_back(pl); cnt[pl] = 0; sing[pl] = sg; rank[pl] = BigRank(base) < 0 ? 1 << 20 : BigRank(base); allElite[pl] = true; }
        if (!el) allElite[pl] = false;
        if (!IsGround(base)) allGround = false;
        return pl;
      };
      std::set<std::string> seen;
      for (auto& k : kills) { std::string pl = key(k.enemy, k.enemyBase); if (!pl.empty()) { ++cnt[pl]; seen.insert(k.enemy); } }
      // an enemy that only shot at us counts once; one that was also killed is counted by its kills
      auto shooter = [&](const std::string& enemy, const std::string& base0, float hp) {
        std::string pl = key(enemy, base0);
        if (pl.empty()) return;
        dmg[pl] += hp;
        if (seen.insert(enemy).second) ++cnt[pl];
      };
      for (auto& c : b.crew) {
        for (auto& e : c.ev) if (e.kind <= R_OTHER) shooter(e.enemy, e.enemyBase, e.lost);
        for (auto& g : c.grazes) shooter(g.enemy, "", g.hp);
      }
      // the most of a type first, then the one that did the crew most harm (the Nagara that did all the damage), then the biggest
      std::stable_sort(order.begin(), order.end(), [&](const std::string& a, const std::string& z) {
        if (cnt[a] != cnt[z]) return cnt[a] > cnt[z];
        if (dmg[a] != dmg[z]) return dmg[a] > dmg[z];
        return rank[a] < rank[z];
      });
      bool more = order.size() > 3;   // more types than the line names, or kills of unknown name
      for (auto& k : kills) if (Trim(k.enemy).empty()) more = true;
      if (more) order.resize(3);
      std::vector<std::string> items;
      bool allOne = true;
      int total = 0;
      for (auto& kv : cnt) { total += kv.second; if (kv.second > 1) allOne = false; }
      for (auto& o : order) {   // one tanker is "a tanker", not "tankers"
        std::string one = allElite[o] ? "elite " + sing[o] : sing[o];
        items.push_back(cnt[o] == 1 ? (allElite[o] ? "an " : "a ") + one : allElite[o] ? "elite " + o : o);
        if (cnt[o] == 1) namedOne.insert(one);
      }
      std::string desc = order.empty() ? "an unseen enemy"
                         : total == 1 ? items[0]
                         : allGround ? "shore positions" + std::string(more ? ", among them " : ": ") + JoinAnd(items)
                         : more ? "a force that included " + JoinAnd(items)
                         : allOne ? JoinAnd(items) : "a force of " + JoinAnd(items);
      Sent s;
      s.v["Dur"] = CapFirst(d);
      s.v["dur"] = d;
      s.v["enemyDesc"] = desc;
      Say(x, "summary.action", s, -1);
    }
    // 2. sinkings
    {
      int planeN = 0, groundN = 0;
      std::string big;
      int bigRank = 1 << 30;
      for (auto& k : kills) {
        std::string base = k.enemyBase.empty() ? BaseOf(k.enemy) : k.enemyBase;
        if (IsPlane(base)) ++planeN;
        else if (IsGround(base)) ++groundN;
        else if (BigRank(base) >= 0 && BigRank(base) < bigRank) { big = "a " + ShipOf(base).singular; bigRank = BigRank(base); }
      }
      int sunk = (int)kills.size() - planeN - groundN;
      if (!big.empty()) {
        std::string one = big.substr(2);   // "a tanker" -> "tanker"; Tanker and Japanese Tanker ... share the noun
        int bigN = 0;
        std::string plural;
        for (auto& k : kills) {
          std::string base = k.enemyBase.empty() ? BaseOf(k.enemy) : k.enemyBase;
          if (IsPlane(base) || IsGround(base)) continue;
          Ship sh = ShipOf(base);
          if (sh.singular == one) { ++bigN; plural = sh.plural; }
        }
        if (bigN > 1) big = bigN == sunk ? (bigN == 2 ? "both of them " : "all of them ") + plural : Num(bigN) + " " + plural;
        else if (namedOne.count(one)) big = "the " + one;   // "against a tanker ... sank the tanker"
      }
      Sent s;
      std::string positions = groundN == 1 ? "enemy position" : "enemy positions";
      s.v["groundN"] = Num(groundN);
      s.v["positions"] = positions;
      s.v["ground"] = groundN ? std::string(planeN ? ", destroyed " : big.empty() ? " and destroyed " : ", and destroyed ") + Num(groundN) + " " + positions : "";
      s.v["crewN"] = crewN;
      s.v["sunk"] = Num(sunk);
      s.v["vessels"] = sunk == 1 ? "enemy vessel" : "enemy vessels";
      s.v["among"] = big.empty() ? "" : sunk == 1 || Starts(big, "all of them ") || Starts(big, "both of them ") ? ", " + big : ", among them " + big;
      s.v["planes"] = planeN ? ", and shot down " + Num(planeN) + " aircraft" : "";
      s.v["planeN"] = Num(planeN);
      Say(x, sunk > 0 ? "summary.sunk" : groundN > 0 ? "summary.ground" : planeN > 0 ? "summary.planes" : "summary.nosink", s, -1);
    }
    bool boatSank = false;
    for (auto& c : b.crew) for (auto& e : c.ev) if (e.kind == S_SANK) boatSank = true;
    auto sinkAndLosses = [&]() {
    // 3. the boat sank (any crew "went down with the boat"; everyone who went over the side in the 12 s before went in with her)
    {
      for (auto& c : b.crew) for (auto& e : c.ev) if (e.kind == S_SANK && (sinkT < 0 || e.t < sinkT)) sinkT = e.t;
      for (size_t i = 0; i < b.crew.size() && sinkT >= 0; ++i)
        for (auto& e : b.crew[i].ev) if (InSinking(b.crew[i], e)) { sankCrew.insert((int)i); break; }
      sinkN = (int)sankCrew.size();
      if (sinkN) {
        int away = 0;   // already in the water or dead when she went: the rest were "all nine still aboard"
        for (size_t i = 0; i < b.crew.size(); ++i) {
          if (sankCrew.count((int)i)) continue;
          bool out = false;
          for (auto& e : b.crew[i].ev) {
            if (e.t > sinkT + 3) continue;
            if (e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST || e.kind == S_SIDE || e.kind == S_KILLED || e.kind == S_DIED_WOUNDS) out = true;
            else if (e.kind == S_PULLED || e.kind == S_PICKED) out = false;
          }
          if (out) ++away;
        }
        Sent s;
        s.t = sinkT;
        s.v["all"] = sinkN == (int)b.crew.size() ? (sinkN == 1 ? std::string("the only crewman") : sinkN == 2 ? std::string("both crew") : "all " + Num(sinkN) + " crew")
                     : away && sinkN + away == (int)b.crew.size() ? (sinkN == 1 ? std::string("the last of the crew still aboard") : sinkN == 2 ? std::string("both still aboard") : "all " + Num(sinkN) + " still aboard")
                                                 : (sinkN == 1 ? std::string("one of the crew") : Num(sinkN) + " of the crew");
        Say(x, "summary.sank", s, -1);
      }
    }
    // 4. casualties
    {
      std::vector<std::string> lost, gone, taken;
      int w = 0, l = 0, picked = 0;
      for (auto& c : b.crew) {
        if (c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED) lost.push_back(NickName(c.name, c.nick));
        if (c.outcome == O_CAPTURED) taken.push_back(NickName(c.name, c.nick));
        else if (c.outcome == O_KIA || c.outcome == O_MISSING) gone.push_back(NickName(c.name, c.nick));
        else if (c.outcome == O_LOST_PICKED) ++picked;
        else if (c.outcome == O_WOUNDED || c.outcome == O_DOWN_AT_END) ++w;
        else if (c.outcome == O_LIGHT) ++l;
      }
      Sent s;
      if (picked) {
        Sent ps;
        ps.v["P"] = CapFirst(Num(picked));
        ps.v["waswere"] = picked == 1 ? "was" : "were";
        Say(x, "summary.picked", ps, -1);
      }
      if (!lost.empty()) {
        s.v["k"] = Num((int)lost.size());
        s.v["waswere"] = lost.size() == 1 ? "was" : "were";
        s.v["names"] = JoinAnd(lost);
        s.v["crewN"] = crewN;
        // the whole crew: no list of every name, their paragraphs follow
        bool all = lost.size() == b.crew.size() && lost.size() > 1;
        if (all || taken.empty()) Say(x, all ? "summary.lost.all" : "summary.lost", s, -1);
        else {   // "One was lost: Thomason. Ten were taken prisoner: ..."
          if (!gone.empty()) {
            s.v["k"] = Num((int)gone.size());
            s.v["waswere"] = gone.size() == 1 ? "was" : "were";
            s.v["names"] = JoinAnd(gone);
            Say(x, "summary.lost", s, -1);
          }
          Sent t;
          t.v["k"] = Num((int)taken.size());
          t.v["waswere"] = taken.size() == 1 ? "was" : "were";
          t.v["names"] = JoinAnd(taken);
          Say(x, "summary.captured", t, -1);
        }
      } else if (!w && !l && !picked) Say(x, "summary.unhurt", s, -1);
      else if (!w && !l) {}
      else {
        std::vector<std::string> parts;
        int all = (int)b.crew.size();   // the whole crew: "all ten were lightly hurt", not a tally of "ten"
        auto many = [&](int n) { return n == all && n > 1 ? (n == 2 ? std::string("both") : "all " + Num(n)) : Num(n); };
        if (w) parts.push_back(many(w) + (w == 1 ? " was wounded" : " were wounded"));
        if (l) parts.push_back(many(l) + (l == 1 ? " was lightly hurt" : " were lightly hurt"));
        s.v["hurt"] = JoinAnd(parts);
        Say(x, "summary.hurt", s, -1);
      }
    }
    };
    auto rakeAndFire = [&]() {
    // 5. a rake that found most of the crew: once here instead of in every paragraph
    {
      std::map<std::string, std::vector<int>> n;
      std::map<std::string, float> t0;
      for (auto& c : b.crew) {
        Grazes z = ReadGrazes(c);
        float t = 0;
        std::string src = DeckSrc(c, z, &t);
        if (src.empty()) continue;
        n[src].push_back(GrazesFrom(c, src));   // only this enemy's splinters: the others' are not the rake's
        if (!t0.count(src) || t < t0[src]) t0[src] = t;
      }
      for (auto& kv : n)
        if (kv.second.size() >= 3 && kv.second.size() * 2 > b.crew.size() && (sharedRake.empty() || kv.second.size() > n[sharedRake].size())) sharedRake = kv.first;
      if (!sharedRake.empty()) {
        auto& v = n[sharedRake];
        Sent s;
        s.t = t0[sharedRake];
        std::string rb = BaseOf(sharedRake), pre = Prefix(sharedRake, rb);
        std::string rs = ShipOf(rb).singular;
        if (Starts(rs, "armored ")) rs = rs.substr(8);
        // an elite is named, not "a Noodle motor gunboat"; the only one of its type the first sentence named is "the"
        s.v["rake"] = !pre.empty() ? "the " + pre + " " + ShipOf(rb).singular : namedOne.count(rs) ? "the " + ShipOf(rb).singular : Article(x, sharedRake, rb);
        float g0 = 1e9f, g1 = -1e9f;   // {spanlen}: how long its splinters kept coming, over the whole crew
        for (auto& c : b.crew) for (auto& g : c.grazes) if (g.enemy == sharedRake) { g0 = (std::min)(g0, g.t); g1 = (std::max)(g1, g.t); }
        s.v["spanlen"] = SpanLen(g1 - (std::max)(g0, s.t));   // from the time the sentence gives, or it runs past what follows
        int lo = *std::min_element(v.begin(), v.end()), hi = *std::max_element(v.begin(), v.end());
        s.v["lo"] = Num(lo);
        s.v["hi"] = Num(hi);
        s.v["range"] = lo == hi ? Num(lo) : Num(lo) + " to " + Num(hi);
        int k = (int)v.size(), all = (int)b.crew.size();
        s.v["who"] = k == all ? "everyone aboard" : k * 10 >= all * 8 ? "nearly everyone aboard" : "most of the crew";
        sharedBurst = !IsPlane(rb);                                    // a plane's pass is a raking however short
        for (auto& c : b.crew) { Grazes z = ReadGrazes(c); if (z.rn && z.rake == sharedRake) sharedBurst = false; }
        Say(x, sharedBurst ? (hi == 1 ? "summary.burst.one" : "summary.burst") : hi == 1 ? "summary.rake.one" : hi > 20 ? "summary.rake.swarm" : "summary.rake", s, -1);
      }
      int fireN = 0; float fireT = 1e9f;
      for (auto& c : b.crew) { Grazes z = ReadGrazes(c); if (z.fireN) { ++fireN; fireT = (std::min)(fireT, z.fireT); } }
      if (fireN >= 3 && fireN * 2 > (int)b.crew.size()) {
        Sent s;
        s.t = fireT;
        int all = (int)b.crew.size();
        s.v["who"] = fireN == all ? "the whole crew" : fireN * 10 >= all * 8 ? "nearly all of the crew" : "most of the crew";
        Say(x, "summary.fire", s, -1);
        fireShared = true;
      }
    }
    };
    if (boatSank) { rakeAndFire(); sinkAndLosses(); }   // the raking that led up to the sinking comes before it
    else { sinkAndLosses(); rakeAndFire(); }
    std::string sum;
    for (auto& s : ps.s) sum += (sum.empty() ? "" : " ") + s;
    if (!sum.empty()) out.push_back({0, sum});
    for (size_t i = 0; i < b.crew.size(); ++i) {
      const RepCrew& c = b.crew[i];
      std::string ln = surname(c.name);
      if (!sankCrew.count((int)i)) {
        RepCrew cc = c;                                                 // after a sinking [out.picked.sank] tells the pickup
        if (sinkT >= 0) cc.ev.erase(std::remove_if(cc.ev.begin(), cc.ev.end(), [&](const RepEv& e) { return e.kind == S_PICKED && e.t >= sinkT; }), cc.ev.end());
        RenderCrew(out, cc, (int)i, lastN[ln] > 1 ? c.name : ln, lastN);
        continue;
      }
      RepCrew cc = c;                                                   // the sinking is in the summary: not in every paragraph
      bool in = false;
      std::vector<RepEv> keep;
      for (auto& e : cc.ev) {
        if (InSinking(c, e)) { in = true; continue; }
        if (in && (e.kind == S_PICKED || e.kind == S_PRISONER)) continue;   // the outcome line tells it
        keep.push_back(e);
      }
      cc.ev = keep;
      RenderCrew(out, cc, (int)i, lastN[ln] > 1 ? c.name : ln, lastN);
    }
    return out;
  }
};

}  // namespace crew_report_detail

inline std::vector<RepPara> RenderReport(const RepBattle& b, const std::map<std::string, std::vector<std::string>>& tables) {
  try {
    crew_report_detail::Renderer r(b, tables);
    return r.Render();
  } catch (...) {
    return {{2, "ACTION REPORT"}, {0, "The report could not be written for this battle."}};
  }
}
inline const char* ReportDefaultIni() { return crew_report_detail::kDefaultIni; }
inline std::map<std::string, std::vector<std::string>> ParseReportIni(const std::string& text) {
  try {
    return crew_report_detail::ParseIniText(text);
  } catch (...) {
    return {};
  }
}
