// crew_life.h: who each crew member was, rolled once from the crew id (home, job before the war, a keepsake, a plan for
// after it, next of kin), the memorial in a lost crew member's file and the letter to the family.
// Pure C++17 like crew_report.h, so the offline test can draft letters for real battles. None of it is game data: the
// game has no homes or families, so all of it is invented.
#pragma once
#include "crew_report.h"
#include <set>

// ---- interface (weaponmod.cpp and tools/crew_report_test.cpp depend on it) ----------------------------------------
struct LifeFacts {                     // the table rows as written: pronoun tokens ({his}) are filled when the text is built
  std::string home, job, keep, plan;   // "Toledo, Ohio" / "delivered ice for {his} uncle" / "a St. Christopher medal" / "open a garage"
  std::string kin, kinFirst;           // "mother" / "Ellen" ("parents": "Walter and Ellen")
  bool kids = false;
  bool orphan = false;                 // the vanilla Orphaned trait: next of kin from [kin.orphan], no parents
  bool empty() const { return home.empty(); }
};
struct LifeFate {                      // how a crew member was lost, from the battle it happened in
  int battle = 0;                      // 0: not lost
  char kind = 0;                       // 'k' killed outright, 'w' died of wounds, 's' went down with the boat, 'm' lost at sea, 'c' captured
  std::string by;                      // "plane", "ship", "shore", "torpedo", "fire" or "" (unknown)
  std::string part;                    // "the chest" or ""
  int role = -1;                       // crew_report_detail RoleCat of the job at the time, -1 unknown
  float mins = 0;                      // 'w': minutes from going down to dying, -1 unknown
  bool sunk = false;                   // the boat sank in that battle: their effects went down with it
  std::string gun;                     // the player gun they were on when lost ("" for other jobs and "on a gun")
};
struct LifeService {
  int first = 0, last = 0, battles = 0, wounds = 0, downs = 0, over = 0, kills = 0;
  std::string friendName, friendWhy;   // the closest mate aboard (crew_bonds.h) and what they shared: "manned the same gun for four battles"
  std::string nick, nickWhy;           // kept nickname (crew_nick.h) and its reason, "" none
  std::string heir;                    // lost crew: who took over their gun or job (station=), "" none
};
typedef std::map<std::string, std::vector<std::string>> LifeTables;

// traits: the crew member's vanilla trait codes and names (any case); taken: everyone else on file (no two share a
// home, job, keepsake or plan while the tables last). The same inputs always give the same facts. service: the
// battles on file; wounds there leave out the battle the crew member was lost in (the caller's job).
LifeFacts RollFacts(uint32_t id, bool female, const std::string& name, const std::vector<std::string>& traits,
                    const std::vector<LifeFacts>& taken, const LifeTables& t);
// The fate of b.crew[idx] in the battle they were lost in (outcome KIA, missing or captured). by is "sank" for crew
// missing after the boat went down.
// report: the report's tables (reports.ini on top of the built-in ones) for [planes] and [ground]; null: built-in.
LifeFate FateOf(const RepBattle& b, size_t idx, int battle, const LifeTables* report = nullptr);
// The block at the top of a living crew member's file, and the memorial of a lost one.
std::vector<std::string> LifeFileText(const std::string& name, bool female, const LifeFacts& f, const LifeTables& t);
std::vector<std::string> MemorialText(const std::string& name, bool female, uint32_t id, const LifeFacts& f,
                                      const LifeService& s, const LifeFate& fate, const LifeTables& t);
// The letter home, one string per paragraph. truth: how they died as it happened, or the kind version (captured crew
// have one version). pick: which of the personal lines go in (any number; it wraps).
std::vector<std::string> DraftLetter(const std::string& name, bool female, uint32_t id, const LifeFacts& f,
                                     const LifeService& s, const LifeFate& fate, bool truth, int pick, const LifeTables& t);
int LetterLineCount(const LifeFacts& f, const LifeService& s, const LifeFate& fate, const LifeTables& t);
const char* LifeDefaultIni();
LifeTables ParseLifeIni(const std::string& text);   // a section in the file replaces the built-in one
// ---- end of interface ----------------------------------------------------------------------------------------------

namespace crew_life_detail {
using namespace crew_report_detail;

static const char kLifeIni[] = R"INI(; Boat Crew mod: crew lives (homes, jobs, keepsakes, plans, next of kin), the memorial and the letter home.
; Copy this file to life.ini and edit it; a section in life.ini replaces the built-in one of that name.
; Facts are rolled ONCE per crew member and kept in crew-files.ini, so edits only reach crew rolled afterwards.
; Pronoun tokens: {he} {his} {him} {He} {His} {Him}; {man} = man or woman. Sections ending .<trait> are used instead of
; the plain one 3 times in 4 when the crew member has that vanilla trait. Row tags: [male] never for a woman,
; [nospouse] not for married crew, [noletter] a job the letter home leaves out, [dear] a keepsake the letter speaks of
; gently (and the only kind it lists in the effects), [vice] a keepsake the letter never mentions, [coast] a keepsake
; only for crew from a [home.fisher] town. Keepsakes and plans must read in any tense (the living file,
; the memorial and the letter all use them).

[home]
Toledo, Ohio
Scranton, Pennsylvania
Duluth, Minnesota
Mobile, Alabama
Tulsa, Oklahoma
Fresno, California
Butte, Montana
Lowell, Massachusetts
Paterson, New Jersey
Akron, Ohio
Savannah, Georgia
Wichita, Kansas
Gary, Indiana
Flint, Michigan
Lubbock, Texas
Spokane, Washington
Baton Rouge, Louisiana
Knoxville, Tennessee
Wheeling, West Virginia
Bangor, Maine
Burlington, Vermont
Dubuque, Iowa
Omaha, Nebraska
Sioux Falls, South Dakota
Bismarck, North Dakota
Pueblo, Colorado
Albuquerque, New Mexico
Tucson, Arizona
Reno, Nevada
Boise, Idaho
Salem, Oregon
Sacramento, California
Brooklyn, New York
Buffalo, New York
Utica, New York
Providence, Rhode Island
New Haven, Connecticut
Baltimore, Maryland
Wilmington, Delaware
Norfolk, Virginia
Asheville, North Carolina
Charleston, South Carolina
Tampa, Florida
Birmingham, Alabama
Jackson, Mississippi
Little Rock, Arkansas
Memphis, Tennessee
Louisville, Kentucky
Peoria, Illinois
Chicago, Illinois
Milwaukee, Wisconsin
Green Bay, Wisconsin
St. Paul, Minnesota
Kansas City, Missouri
St. Louis, Missouri
Erie, Pennsylvania
Pittsburgh, Pennsylvania
Youngstown, Ohio
Grand Rapids, Michigan
Fort Wayne, Indiana
Terre Haute, Indiana
El Paso, Texas
Shreveport, Louisiana
Fall River, Massachusetts

[home.fisher]
Gloucester, Massachusetts
New Bedford, Massachusetts
Astoria, Oregon
Biloxi, Mississippi
San Pedro, California
Bar Harbor, Maine

[home.cowboy]
Miles City, Montana
Cheyenne, Wyoming
Amarillo, Texas
Elko, Nevada
Prescott, Arizona

; "Before the war {he} ..." (past tense)
[job]
delivered ice for {his} uncle
[male] worked the open hearth at a steel mill
pumped gas at a filling station on Route 66
drove a bread truck
was a soda jerk at the corner drugstore
set pins at a bowling alley
picked fruit up and down the valley
worked the line at a tire plant
set type for the local paper
sold shoes, and hated every minute of it
was halfway through teachers' college
ran the projector at the picture house
[male] was a brakeman on the railroad
cut hair in {his} father's barbershop
[male] worked the stockyards
strung wire for the telephone company
played trumpet in a dance band that never quite made it
clerked at a hardware store
[male] mined coal, like {his} father and grandfather
worked a lathe in a machine shop
wrote obituaries for the evening paper
drove a taxi on the night shift
[male] loaded freight on the docks
was studying to be a pharmacist
worked in a cannery
was a bank teller
worked {his} family's farm
[male] felled timber in the north woods
was a short-order cook at an all-night diner
sold encyclopedias door to door
[male] welded hulls at the shipyard
[male] planted trees with the Civilian Conservation Corps
taught the fourth grade
fixed radios in a shop the size of a closet
was a lifeguard at the municipal pool
drove a milk wagon

[job.fisher]
worked {his} father's fishing boat
went out with the shrimp boats
hauled crab pots every morning before dawn
crewed on a tuna clipper

[job.cowboy]
broke horses on a cattle ranch
rode for an outfit that ran three thousand head
worked the rodeo circuit, mostly falling off

[job.crook]
[noletter] did something {he} never talked about; the crew guessed bootlegging
[noletter] ran numbers for a man {he} would never name
[noletter] was in what {he} called import and export

[job.olympian]
swam at the 1936 Olympics in Berlin
ran the hurdles at the 1936 Games and kept the blazer
[male] rowed in an Olympic eight

[job.underage]
[noletter] was still in high school, whatever {he} told the recruiters
[noletter] delivered newspapers until {he} lied about {his} age to enlist

[job.dilbert]
filed claims at an insurance office
kept the books for a paper company
was an accounts clerk who never once left on time

[job.acrobat]
worked the trapeze in a traveling circus
had a tumbling act in vaudeville

[keep]
a harmonica nobody ever heard {him} play well
[dear] a pocket Bible with {his} mother's handwriting inside the cover
[dear] a St. Christopher medal
a lucky silver dollar
a baseball signed by half the 1941 Dodgers
[dear][nospouse] a photograph of {his} sweetheart
a deck of cards with the jack of hearts missing
[dear] {his} grandfather's pocketknife
a rabbit's foot worn bald
a wristwatch set ten minutes fast
[dear] a bundle of letters from home tied with string
[dear] a fountain pen kept for letters home
a small brass compass
a dog-eared Western paperback
a Zippo lighter engraved with {his} initials
a pressed four-leaf clover
a sketchbook full of drawings of the crew
a ukulele with three strings
[dear][coast] a seashell from the beach at home
[dear] a rosary
{his} high-school class ring
a ticket stub from the 1941 World Series
[dear] a map of home drawn on the back of a menu
a Brownie camera with one roll of film left
a pocket chess set with two pawns missing
a postcard of the Golden Gate Bridge that {he} never sent

[keep.kids]
[dear] a photograph of {his} children
[dear] a crayon drawing of the boat from {his} daughter
[dear] one of {his} baby's shoes
[dear] a letter in {his} son's handwriting

[keep.drunk]
[vice] a dented hip flask
[vice] a bottle of bourbon wrapped in a sock

[keep.hero]
[dear] a medal {he} never explained

[keep.gambler]
[vice] a pair of lucky dice
[vice] a notebook of what half the crew owed {him}

; "After the war {he} means to ..." / "{He} was going to ..."
[plan]
open a garage with {his} brother
buy a farm in the hills and never see the sea again
go back to school and become an engineer
[nospouse] marry {his} sweetheart the week of {his} homecoming
open a diner on the highway
take over {his} father's store
see a game in every ballpark in the country
build a house with {his} own two hands
become a teacher
start a band
buy a fishing boat of {his} own
drive to California and stay there
learn to fly
go home and sleep for a month
open a bakery with {his} mother's recipes
write a book about all of this
go back to {his} old job and be glad of it
see Paris
coach the high-school team
run for mayor, as a joke at first
open a bar with a jukebox that plays only Glenn Miller
travel the whole country by train

[plan.kids]
watch {his} children grow up
[male] meet {his} youngest, born after {he} shipped out
take {his} son fishing
walk {his} daughter to her first day of school

[plan.underage]
finish high school
learn to drive

[plan.optimist]
make a million dollars, then a second one
own half of {his} hometown by fifty

; next of kin: mother, father, parents, sister, brother, grandmother, aunt, uncle; spouse comes from kids or marriage
[kin]
mother
mother
mother
parents
parents
parents
father
sister
brother
grandmother

[kin.orphan]
aunt
uncle
brother
sister
grandmother

[name.f]
Ellen
Mary
Dorothy
Helen
Margaret
Ruth
Mildred
Virginia
Frances
Alice
Edna
Lillian
Marie
Irene
Louise

[name.m]
Walter
Harold
Frank
Joseph
Charles
Arthur
Albert
Henry
Ernest
Raymond
Earl
Clarence
Howard
Fred
Leonard

; small things sent home with the keepsake
[effects]
a wallet
a shaving kit
a pocket comb and a sewing kit
two letters {he} never finished
a pack of cigarettes

; ---- the crew files ----
; {kinFull} "Ellen Kessler", {kinRel} "mother"
[file.who]
From {home}. Before the war {he} {job}.
[file.keep]
Carries {keep}.
[file.plan]
After the war {he} means to {plan}.
[file.kin]
Next of kin: {kinFull}, {kinRel}.

[mem.who]
{Name}, from {home}. Before the war {he} {job}.
; {span} "from battle 3 to battle 11" or "battle 4 only"; {tally} "Wounded twice, in the water once."; {credit}
[mem.service]
On file for {n} {engagements}, {span}.{tally}{credit}
; how they were lost, as plain fact; {station} "at his gun", {by} "an enemy aircraft", {hitin} " (hit in the chest)"
; {hitin} ", hit in the chest"
[mem.fate.k]
Killed by fire from {by} while {station}{hitin}, in {his} {ord} engagement.
[mem.fate.k.none]
Killed in action while {station}, in {his} {ord} engagement.
[mem.fate.k.torpedo]
Killed when an enemy torpedo struck the boat, while {station}, in {his} {ord} engagement.
[mem.fate.k.fire]
Killed in the fire aboard, in {his} {ord} engagement.
; {after} "moments later", "about five minutes later", "soon after"
[mem.fate.w]
Wounded while {station} and died {after}, in {his} {ord} engagement.
[mem.fate.s]
Went down with the boat in {his} {ord} engagement.
[mem.fate.m]
Lost overboard in {his} {ord} engagement and never found.
[mem.fate.m.sank]
Lost at sea when the boat went down, in {his} {ord} engagement.
[mem.fate.c]
Taken prisoner after {his} {ord} engagement.
[mem.friend]
Closest aboard: {friend}; they {why}.
[mem.nick]
; a kept nickname; {nick}, {nickwhy} "going over the side in three battles and coming back every time"
The crew called {him} "{nick}", for {nickwhy}.
; who took over the station (crew files); {heir} full name, {gun} the gun's name
[mem.station.gun]
{His} {gun} went to {heir}.
[mem.station]
{heir} took over {his} station.
[mem.effects]
Effects returned: {effects}.
[mem.effects.m]
Belongings held: {effects}.
[mem.effects.sank]
Nothing of {his} was saved, not even {keep}.
[mem.effects.sank.plain]
Nothing of {his} was saved.
[mem.plan]
{He} was going to {plan}.
[mem.plan.c]
{He} still means to {plan}.

; ---- the letter ----
; {salute} "Mrs. Kessler", {rel} "son", {Name} full name
[letter.open.k]
It is my sad duty to tell you that your {rel}, {Name}, was killed in action against the enemy.
It is with deep regret that I write to tell you that your {rel}, {Name}, has been killed in action.
[letter.open.w]
It is my sad duty to tell you that your {rel}, {Name}, has died of wounds received in action.
It is with deep regret that I write to tell you that your {rel}, {Name}, died of wounds received in action against the enemy.
[letter.open.s]
It is my sad duty to tell you that your {rel}, {Name}, was killed in action when our boat was sunk.
[letter.open.m]
I am sorry to have to tell you that your {rel}, {Name}, is missing in action.
[letter.open.c]
I write to tell you that your {rel}, {Name}, has been taken prisoner by the enemy.

; how it happened: .true as it was, .kind to spare them
[letter.true.k]
{He} was {station} when fire from {by} hit {him}{hitin}. {He} died at once.
[letter.true.k.none]
{He} was {station} when {he} was hit, and {he} died at once.
[letter.true.k.torpedo]
{He} was {station} when an enemy torpedo struck the boat, and {he} died in the explosion.
[letter.true.k.fire]
A fire broke out aboard during the fight, and {he} could not get clear of it.
; {after} "about five minutes later"; .quick under a minute; .unknown when the time is not known; .fire/.torpedo by cause
[letter.true.w]
{He} was hit while {station} and died {after}, in spite of everything that was done for {him}.
[letter.true.w.quick]
{He} was hit while {station} and died moments later.
[letter.true.w.unknown]
{He} was hit while {station} and could not be saved, in spite of everything that was done for {him}.
[letter.true.w.fire]
{He} was badly burned while {station} and died {after}, in spite of everything that was done for {him}.
[letter.true.w.torpedo]
{He} was caught in the explosion when an enemy torpedo struck the boat, and died {after}, in spite of everything that was done for {him}.
[letter.true.s]
{He} did not get clear in time.
[letter.true.m]
{He} went over the side during the action and could not be found afterwards. I must tell you honestly that there is very little hope.
[letter.true.m.sank]
When our boat was sunk, {he} went into the water with the rest of us and was not found. I must tell you honestly that there is very little hope.
[letter.kind.k]
It was very quick. {He} would not have felt anything.
Death came instantly, and {he} did not suffer.
[letter.kind.w]
{He} was given every care, and {he} was not alone at the end; we were with {him}.
[letter.kind.s]
{He} was at {his} station, and it was over in moments.
[letter.kind.m]
{He} went over the side during the action and has not been found. I will not raise false hope, but neither should you give up hope yet.
[letter.kind.m.sank]
{He} went into the water when our boat was sunk, and has not been found. I will not raise false hope, but neither should you give up hope yet.
[letter.true.c]
When the action ended, {he} was still in the water and was picked up by an enemy ship. The Red Cross will tell you how you can write to {him}.

; personal lines (the player cycles them): {n} engagements, {times} "twice", {kills} "three"
[letter.line.new]
{He} had not been with us long, but {he} had already made {his} place aboard.
[letter.line.mid]
In {n} engagements with us, {he} never once flinched.
[letter.line.long]
{He} had been with us for {n} engagements, and every one of us had come to depend on {him}.
[letter.line.wounds]
{He} was wounded {times}, and each time went back to {his} post as soon as {he} could.
[letter.line.wounds.1]
{He} was wounded once, and went back to {his} post as soon as {he} could.
[letter.line.kills]
{He} was credited with a part in {kills} enemy losses, though {he} never made much of it.
[letter.line.kills.1]
{He} was credited with a part in one enemy loss, though {he} never made much of it.
[letter.line.keep]
{He} was hardly ever without {keep}, and the men used to tease {him} about it.
[letter.line.keep.dear]
{He} always carried {keep}.
[letter.line.plan]
{He} had it all worked out: after the war, {he} would {plan}.
[letter.line.kids]
{He} talked about {his} children all the time, and showed their picture to anyone who would look.
[letter.line.friend]
{He} and {friend} {why}; the two of them were never far apart.
[letter.line.nick]
The men called {him} "{nick}", and {he} never minded.
To every man aboard {he} was "{nick}".
[letter.line.home]
{He} made {home} sound like the finest place in the country.
[letter.line.job]
{He} liked to tell us about the days when {he} {job}.

[letter.effects]
{His} personal effects{incl} are being sent on to you.
[letter.effects.sank]
{His} belongings went down with the boat.
[letter.effects.s]
{His} belongings were lost with {him}.
[letter.effects.m]
{His} belongings are being held here for now.
[letter.effects.c]
{His} belongings will be kept safe until {he} can claim them.
[letter.close]
{He} was a fine shipmate, and we will all miss {him}. Please accept our deepest sympathy.
{He} was a good {man} and a good shipmate, and {his} loss is felt by all of us.
[letter.close.m]
If there is any news at all, you will hear it at once. All of us are thinking of you.
[letter.close.c]
{He} is a fine shipmate, and we all hope to see {him} again before long.
[letter.sign]
Sincerely,
Commanding Officer
)INI";

inline std::string Low(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return s; }
inline std::string Fill(std::string s, const std::map<std::string, std::string>& v) {
  for (size_t i = 0; (i = s.find('{', i)) != std::string::npos;) {
    size_t e = s.find('}', i);
    if (e == std::string::npos) break;
    auto it = v.find(s.substr(i + 1, e - i - 1));
    if (it == v.end()) { i = e + 1; continue; }
    s.replace(i, e - i + 1, it->second);
    i += it->second.size();
  }
  return s;
}
// a row's leading [tag]s: removed from the text, returned in tags ("|male|dear|")
inline std::string Untag(const std::string& row, std::string* tags = nullptr) {
  std::string r = row, tg = "|";
  while (!r.empty() && r[0] == '[') {
    size_t e = r.find(']'); if (e == std::string::npos) break;
    tg += r.substr(1, e - 1) + "|"; r = crew_report_detail::Trim(r.substr(e + 1));
  }
  if (tags) *tags = tg;
  return r;
}
inline void Pronouns(std::map<std::string, std::string>& v, bool female) {
  v["he"] = female ? "she" : "he"; v["his"] = female ? "her" : "his"; v["him"] = female ? "her" : "him";
  v["He"] = female ? "She" : "He"; v["His"] = female ? "Her" : "His"; v["Him"] = female ? "Her" : "Him";
  v["man"] = female ? "woman" : "man";
}
inline const std::vector<std::string>& Sec(const LifeTables& t, const std::string& name) {
  static const LifeTables def = [] { LifeTables d; for (auto& kv : ParseIniText(kLifeIni)) d[kv.first] = kv.second; return d; }();
  static const std::vector<std::string> none;
  auto it = t.find(name);
  if (it != t.end() && !it->second.empty()) return it->second;
  auto d = def.find(name);
  return d != def.end() ? d->second : none;
}
inline std::string Pick(const LifeTables& t, const std::string& name, uint32_t h) {
  const auto& r = Sec(t, name);
  return r.empty() ? std::string() : Untag(r[h % r.size()]);
}
// where a stored fact came from: its section ("keep.kids") and tags, or "" when the tables no longer have it
inline std::string SourceOf(const LifeTables& t, const char* field, const std::string& raw, std::string* tags) {
  static const char* const kSecs[] = { "", ".kids", ".drunk", ".hero", ".gambler", ".fisher", ".cowboy", ".crook", ".olympian",
    ".underage", ".dilbert", ".acrobat", ".optimist" };
  if (tags) *tags = "|";
  for (auto sx : kSecs) {
    std::string sec = std::string(field) + sx;
    for (auto& r : Sec(t, sec)) if (Untag(r, tags) == raw) return sec;
  }
  if (tags) *tags = "|";
  return "";
}
inline std::string Surname(const std::string& name) { size_t s = name.rfind(' '); return s == std::string::npos ? name : name.substr(s + 1); }
inline std::string FirstName(const std::string& name) { size_t s = name.find(' '); return s == std::string::npos ? name : name.substr(0, s); }
inline std::string Times(int n) { return n == 1 ? "once" : n == 2 ? "twice" : Num(n) + " times"; }
inline bool KinFemale(const std::string& k, bool crewFemale) {
  return k == "mother" || k == "sister" || k == "grandmother" || k == "aunt" || (k == "spouse" && !crewFemale);
}
inline std::string KinRel(const std::string& k, bool crewFemale) {   // what the kin is to the crew member
  return k == "spouse" ? (crewFemale ? "husband" : "wife") : k;
}
inline std::string RelOfCrew(const std::string& k, bool f) {           // what the crew member is to the kin
  if (k == "mother" || k == "father" || k == "parents") return f ? "daughter" : "son";
  if (k == "spouse") return f ? "wife" : "husband";
  if (k == "sister" || k == "brother") return f ? "sister" : "brother";
  if (k == "grandmother") return f ? "granddaughter" : "grandson";
  if (k == "aunt" || k == "uncle") return f ? "niece" : "nephew";
  return f ? "daughter" : "son";
}
inline std::string Salute(const std::string& k, bool f, const std::string& last) {
  if (k == "parents") return "Mr. and Mrs. " + last;
  if (k == "sister") return "Miss " + last;
  return (KinFemale(k, f) ? "Mrs. " : "Mr. ") + last;
}
inline std::string Station(int role, bool female) {
  std::string his = female ? "her" : "his";
  switch (role) {
    case RC_GUN: return "at " + his + " gun";
    case RC_AMMO: return "passing ammunition to the guns";
    case RC_REPAIR: return "working to keep the boat afloat";
    case RC_MEDIC: return "caring for the injured";
    case RC_HELM: return "at the wheel";
    default: return "at " + his + " station";
  }
}
inline std::string ByWords(const std::string& by) {
  return by == "plane" ? "an enemy aircraft" : by == "shore" ? "an enemy shore battery" : by == "ship" ? "an enemy ship" : "";
}
inline std::string ForMins(float m) { int n = RoundI(m); return n <= 1 ? "about a minute" : "about " + Num(n) + " minutes"; }
inline std::string After(float m) { return m < 0 ? "soon after" : m < 0.75f ? "moments later" : ForMins(m) + " later"; }
// the facts, pronouns filled, plus the pronouns themselves
inline std::map<std::string, std::string> Vars(const LifeFacts& f, bool female) {
  std::map<std::string, std::string> v; Pronouns(v, female);
  v["home"] = f.home; v["job"] = Fill(f.job, v); v["keep"] = Fill(f.keep, v); v["plan"] = Fill(f.plan, v);
  return v;
}
}  // namespace crew_life_detail

inline const char* LifeDefaultIni() { return crew_life_detail::kLifeIni; }
inline LifeTables ParseLifeIni(const std::string& text) {
  LifeTables t;
  for (auto& kv : crew_report_detail::ParseIniText(text)) if (!kv.second.empty()) t[kv.first] = kv.second;
  return t;
}

inline LifeFacts RollFacts(uint32_t id, bool female, const std::string& name, const std::vector<std::string>& traits,
                           const std::vector<LifeFacts>& taken, const LifeTables& t) {
  using namespace crew_life_detail;
  std::string tw; for (auto& s : traits) tw += "|" + Low(s);
  auto has = [&](const char* k) { return tw.find(k) != std::string::npos; };
  bool underage = has("underage");
  LifeFacts f;
  f.orphan = has("orphan") && !underage;
  f.kids = has("kid") && !underage;
  // next of kin first: a married crew member never rolls the sweetheart rows
  if (f.kids || (!underage && Hash(id, 15) % 5 == 0)) f.kin = "spouse";
  else if (underage) { uint32_t u = Hash(id, 16) % 6; f.kin = u < 3 ? "mother" : u < 5 ? "parents" : "grandmother"; }
  else f.kin = Pick(t, has("orphan") ? "kin.orphan" : "kin", Hash(id, 17));
  if (f.kin.empty()) f.kin = "mother";
  bool kf = KinFemale(f.kin, female);
  if (f.kin == "parents") f.kinFirst = Pick(t, "name.m", Hash(id, 18)) + " and " + Pick(t, "name.f", Hash(id, 19));
  else f.kinFirst = Pick(t, kf ? "name.f" : "name.m", Hash(id, 18));
  // one field: the trait's section 3 times in 4 when the trait is there (or a forced one), then the plain section;
  // the first fitting row nobody else has
  std::string jobSec, homeSec;
  auto roll = [&](const char* field, uint32_t salt, const std::vector<std::pair<const char*, const char*>>& special,
                  std::string LifeFacts::*member, const std::string& forced, std::string* secOut) {
    auto fits = [&](const std::string& row) {
      std::string tg; std::string r = Untag(row, &tg);
      if (female && tg.find("|male|") != std::string::npos) return false;
      if (f.kin == "spouse" && tg.find("|nospouse|") != std::string::npos) return false;
      if (tg.find("|coast|") != std::string::npos && homeSec != "home.fisher") return false;
      if (member == &LifeFacts::plan) {
        if (r.find("teacher") != std::string::npos && f.job.find("taught") != std::string::npos) return false;
        if (r.find("youngest") != std::string::npos && f.keep.find("drawing") != std::string::npos) return false;   // a school-age artist, not a newborn
        if (r.find("old job") != std::string::npos && (jobSec == "job.crook" || jobSec == "job.underage" || jobSec == "job.olympian" ||
            f.job.find("studying") != std::string::npos || f.job.find("halfway") != std::string::npos)) return false;
      }
      return !r.empty();
    };
    std::set<std::string> used; for (auto& o : taken) used.insert(o.*member);
    std::vector<std::string> order;
    if (!forced.empty()) order.push_back(forced);
    else for (auto& sp : special) if (has(sp.first) && Hash(id, salt, 7) % 4 != 0) { order.push_back(std::string(field) + "." + sp.second); break; }
    order.push_back(field);
    std::string first, firstSec;
    for (auto& sec : order) {
      const auto& rows = Sec(t, sec);
      size_t start = rows.empty() ? 0 : Hash(id, salt) % rows.size();
      for (size_t k = 0; k < rows.size(); k++) {
        const std::string& row = rows[(start + k) % rows.size()];
        if (!fits(row)) continue;
        std::string r = Untag(row);
        if (first.empty()) { first = r; firstSec = sec; }
        if (!used.count(r)) { if (secOut) *secOut = sec; return r; }
      }
    }
    if (secOut) *secOut = firstSec;
    return first;                                                      // the tables ran out: a repeat
  };
  f.job = roll("job", 12, {{"underage", "underage"}, {"fisher", "fisher"}, {"cowboy", "cowboy"}, {"crook", "crook"},
                           {"olymp", "olympian"}, {"dilbert", "dilbert"}, {"acrobat", "acrobat"}}, &LifeFacts::job, "", &jobSec);
  // a fisherman or a cowboy comes from a town that fits the job
  f.home = roll("home", 11, {}, &LifeFacts::home, jobSec == "job.fisher" ? "home.fisher" : jobSec == "job.cowboy" ? "home.cowboy" : "", &homeSec);
  std::vector<std::pair<const char*, const char*>> keepSp = {{"drunk", "drunk"}, {"hero", "hero"}, {"gambl", "gambler"}};
  if (f.kids) keepSp.insert(keepSp.begin(), {"kid", "kids"});
  f.keep = roll("keep", 13, keepSp, &LifeFacts::keep, "", nullptr);
  std::vector<std::pair<const char*, const char*>> planSp = {{"underage", "underage"}, {"optimist", "optimist"}};
  if (f.kids) planSp.insert(planSp.begin(), {"kid", "kids"});
  f.plan = roll("plan", 14, planSp, &LifeFacts::plan, "", nullptr);
  (void)name;
  return f;
}

inline LifeFate FateOf(const RepBattle& b, size_t idx, int battle, const LifeTables* report) {
  const RepCrew& c = b.crew[idx];
  using namespace crew_report_detail;
  static const Tables def = ParseIniText(ReportDefaultIni());
  auto has = [&](const char* sec, const std::string& s) {
    const Tables* m = report;
    auto it = m ? m->find(sec) : def.end();
    if (!m || it == m->end() || it->second.empty()) { m = &def; it = def.find(sec); }
    if (it == m->end()) return false;
    for (auto& w : it->second) if (!w.empty() && s.find(w) != std::string::npos) return true;
    return false;
  };
  LifeFate f; f.battle = battle;
  f.kind = c.outcome == O_CAPTURED ? 'c' : c.outcome == O_MISSING ? 'm' : 'k';
  float sinkT = -1;
  for (auto& o : b.crew) for (auto& e : o.ev) if (e.kind == S_SANK && (sinkT < 0 || e.t < sinkT)) sinkT = e.t;
  f.sunk = sinkT >= 0;
  int death = -1;
  for (int i = (int)c.ev.size() - 1; i >= 0 && death < 0; --i) {
    int k = c.ev[i].kind;
    if (k == S_SANK || k == S_DIED_WOUNDS || k == S_KILLED) death = i;
  }
  int end = death >= 0 ? death : (int)c.ev.size();
  if (f.kind == 'm')                                                   // as the report tells a sinking (Renderer::InSinking)
    for (auto& e : c.ev) if (InSinkingWindow(c, e, sinkT)) f.by = "sank";
  if (f.kind == 'k' && death >= 0) f.kind = c.ev[death].kind == S_SANK ? 's' : c.ev[death].kind == S_DIED_WOUNDS ? 'w' : 'k';
  std::string role = c.role;
  for (int i = 0; i < end; ++i) if (c.ev[i].kind == S_ROLE && !c.ev[i].part.empty()) role = c.ev[i].part;
  f.role = role.empty() ? -1 : RoleCat(role);
  f.gun = RoleGun(role);
  if (f.kind == 'w') {                                                 // the hit that put them down
    int down = -1;
    for (int i = end - 1; i >= 0 && down < 0; --i) if (c.ev[i].kind == S_DOWN || c.ev[i].kind == S_BLEED) down = i;
    if (down >= 0) { f.mins = (c.ev[end].t - c.ev[down].t) / 60.0f; end = down; }
    else f.mins = -1;
  }
  if (f.kind == 'k' || f.kind == 'w') {
    for (int i = end - 1; i >= 0; --i) {
      const RepEv& e = c.ev[i];
      if (e.kind > R_OTHER) { if (e.kind == S_UP) break; continue; }    // only the hits since they were last on their feet
      f.part = e.part;
      std::string base = e.enemyBase.empty() ? e.enemy : e.enemyBase;
      f.by = e.kind == R_TORPEDO ? "torpedo" : e.kind == R_FIRE || e.enemy == "fire" ? "fire"
           : base.empty() || e.enemy == "our own guns" ? "" : has("planes", base) ? "plane" : has("ground", base) ? "shore" : "ship";
      break;
    }
  }
  return f;
}

// role (a report role or a crew file entry's heading noun) is the station the lost crew member held: the same gun by name,
// or the same job for ammo, repair, medic and helm
inline bool StationRole(const std::string& role, const LifeFate& f) {
  using namespace crew_report_detail;
  if (f.role == RC_GUN) return !f.gun.empty() && EntryGun(role) == f.gun;   // "on the X" and "gunner, X" both
  return f.role >= 0 && f.role != RC_GUN && f.role != RC_OTHER && EntryRoleCat(role) == f.role;
}

inline std::vector<std::string> LifeFileText(const std::string& name, bool female, const LifeFacts& f, const LifeTables& t) {
  using namespace crew_life_detail;
  if (f.empty()) return {};
  auto v = Vars(f, female);
  v["kinFull"] = f.kinFirst + " " + Surname(name); v["kinRel"] = KinRel(f.kin, female);
  uint32_t h = Fnv(name);
  std::string a = Fill(Pick(t, "file.who", h), v) + " " + Fill(Pick(t, "file.keep", h), v) + " " + Fill(Pick(t, "file.plan", h), v);
  return { CapFirst(Trim(a)), Fill(Pick(t, "file.kin", h), v) };
}

inline std::vector<std::string> MemorialText(const std::string& name, bool female, uint32_t id, const LifeFacts& f,
                                             const LifeService& s, const LifeFate& fate, const LifeTables& t) {
  using namespace crew_life_detail;
  auto v = Vars(f, female);
  v["Name"] = crew_report_detail::NickName(name, s.nick);
  std::vector<std::string> out;
  if (!f.empty()) out.push_back(Fill(Pick(t, "mem.who", id), v));
  if (!s.nick.empty()) {
    v["nick"] = s.nick; v["nickwhy"] = s.nickWhy;
    std::string l = Fill(Pick(t, "mem.nick", id), v);
    if (!l.empty()) out.push_back(l);
  }
  if (s.battles > 0) {
    v["n"] = Num(s.battles); v["engagements"] = s.battles == 1 ? "engagement" : "engagements";
    v["span"] = s.first == s.last ? "battle " + std::to_string(s.first) + " only" : "from battle " + std::to_string(s.first) + " to battle " + std::to_string(s.last);
    std::vector<std::string> tl;
    if (s.wounds) tl.push_back("wounded " + Times(s.wounds));
    if (s.downs) tl.push_back("downed " + Times(s.downs));
    if (s.over) tl.push_back("in the water " + Times(s.over));
    v["tally"] = tl.empty() ? "" : " " + CapFirst(JoinAnd(tl)) + ".";
    v["credit"] = s.kills ? std::string(" Credited with ") + (s.kills == 1 ? "a share in one kill." : "a share in " + Num(s.kills) + " kills.") : "";
    out.push_back(Fill(Pick(t, "mem.service", id), v));
  }
  if (!s.friendName.empty()) { v["friend"] = s.friendName; v["why"] = s.friendWhy; out.push_back(Fill(Pick(t, "mem.friend", id), v)); }
  if (fate.battle > 0) {
    v["ord"] = Ordinal(s.battles > 0 ? s.battles : 1);
    v["station"] = Station(fate.role, female); v["by"] = ByWords(fate.by); v["after"] = After(fate.mins);
    v["hitin"] = fate.part.empty() ? "" : ", hit in " + fate.part;
    std::string sec = std::string("mem.fate.") + fate.kind;
    if (fate.kind == 'k') sec += fate.by == "torpedo" ? ".torpedo" : fate.by == "fire" ? ".fire" : ByWords(fate.by).empty() ? ".none" : "";
    if (fate.kind == 'm' && fate.by == "sank") sec += ".sank";
    out.push_back(Fill(Pick(t, sec, id), v));
    if (!s.heir.empty()) {
      v["heir"] = s.heir; v["gun"] = fate.gun;
      std::string l = Fill(Pick(t, fate.gun.empty() ? "mem.station" : "mem.station.gun", id), v);
      if (!l.empty()) out.push_back(l);
    }
    if (!f.empty() && fate.kind != 'c') {
      std::string fx = Fill(Pick(t, "effects", Hash(id, 21)), v);
      int cents = (int)(Hash(id, 22) % 1400) + 35;
      char money[48]; snprintf(money, sizeof money, "$%d.%02d", cents / 100, cents % 100);
      if (fx == "a wallet") fx += std::string(" with ") + money + " in it";
      std::string kt; SourceOf(t, "keep", f.keep, &kt);
      bool vice = kt.find("|vice|") != std::string::npos;               // not listed as sent home
      v["effects"] = vice ? fx : v["keep"] + ", " + fx;
      bool sank = fate.kind == 's' || fate.by == "sank" || fate.sunk;
      out.push_back(Fill(Pick(t, sank ? (vice ? "mem.effects.sank.plain" : "mem.effects.sank") : fate.kind == 'm' ? "mem.effects.m" : "mem.effects", id), v));
    }
    if (!f.empty()) out.push_back(Fill(Pick(t, fate.kind == 'c' ? "mem.plan.c" : "mem.plan", id), v));
  }
  return out;
}

namespace crew_life_detail {
inline std::vector<std::string> LineSections(const LifeFacts& f, const LifeService& s, const LifeFate& fate, const LifeTables& t) {
  std::vector<std::string> L;
  L.push_back(s.battles >= 5 ? "letter.line.long" : s.battles <= 2 ? "letter.line.new" : "letter.line.mid");
  if (s.wounds) L.push_back(s.wounds == 1 ? "letter.line.wounds.1" : "letter.line.wounds");
  if (s.kills) L.push_back(s.kills == 1 ? "letter.line.kills.1" : "letter.line.kills");
  if (!s.friendName.empty()) L.push_back("letter.line.friend");
  if (!s.nick.empty()) L.push_back("letter.line.nick");
  if (!f.empty()) {
    std::string kt, jt;
    std::string ks = SourceOf(t, "keep", f.keep, &kt);
    SourceOf(t, "job", f.job, &jt);
    if (f.kids) L.push_back("letter.line.kids");
    if (kt.find("|vice|") == std::string::npos && !(f.kids && ks == "keep.kids"))   // the kids line already speaks of them
      L.push_back(kt.find("|dear|") != std::string::npos ? "letter.line.keep.dear" : "letter.line.keep");
    bool kidsPlan = f.kids && SourceOf(t, "plan", f.plan, nullptr) == "plan.kids";   // the kids line already speaks of them
    if (fate.kind != 'c' && !kidsPlan) L.push_back("letter.line.plan");
    L.push_back("letter.line.home");
    if (jt.find("|noletter|") == std::string::npos && f.job.find_first_of(",;\"") == std::string::npos) L.push_back("letter.line.job");   // "sold shoes, and hated it" reads badly there
  }
  return L;
}
// what the letter says of the hit: the head and chest only (no "shot in the back" to a family)
inline std::string LetterPart(const std::string& part) {
  if (part.find("head") != std::string::npos) return " in the head";
  if (part.find("chest") != std::string::npos) return " in the chest";
  return "";
}
}  // namespace crew_life_detail
inline int LetterLineCount(const LifeFacts& f, const LifeService& s, const LifeFate& fate, const LifeTables& t) {
  return (int)crew_life_detail::LineSections(f, s, fate, t).size();
}

inline std::vector<std::string> DraftLetter(const std::string& name, bool female, uint32_t id, const LifeFacts& f,
                                            const LifeService& s, const LifeFate& fate, bool truth, int pick, const LifeTables& t) {
  using namespace crew_life_detail;
  auto v = Vars(f, female);
  std::string kin = f.kin.empty() ? "parents" : f.kin;
  v["Name"] = name;
  v["rel"] = RelOfCrew(kin, female);
  v["station"] = Station(fate.role, female); v["by"] = ByWords(fate.by); v["after"] = After(fate.mins);
  v["hitin"] = LetterPart(fate.part);
  v["n"] = Num(s.battles > 0 ? s.battles : 1); v["times"] = Times(s.wounds); v["kills"] = Num(s.kills);
  v["friend"] = s.friendName; v["why"] = s.friendWhy;
  v["nick"] = s.nick;
  char k = fate.kind ? fate.kind : 'k';
  std::vector<std::string> out;
  out.push_back("Dear " + Salute(kin, female, Surname(name)) + ",");
  std::string p1 = Fill(Pick(t, std::string("letter.open.") + k, Hash(id, 31)), v);
  std::string how;
  if (k == 'c') how = Pick(t, "letter.true.c", id);
  else {
    std::string sec = std::string(truth ? "letter.true." : "letter.kind.") + k;
    if (truth && k == 'k') sec += fate.by == "torpedo" ? ".torpedo" : fate.by == "fire" ? ".fire" : ByWords(fate.by).empty() ? ".none" : "";
    if (truth && k == 'w') sec += fate.by == "fire" ? ".fire" : fate.by == "torpedo" ? ".torpedo" : fate.mins < 0 ? ".unknown" : fate.mins < 0.75f ? ".quick" : "";
    if (k == 'm' && fate.by == "sank") sec += ".sank";
    how = Pick(t, sec, Hash(id, 32));
  }
  out.push_back(p1 + " " + Fill(how, v));
  // two personal lines, picked by the player; the keepsake is named once
  std::vector<std::string> L = LineSections(f, s, fate, t);
  int n = (int)L.size(), p = ((pick % n) + n) % n;
  std::vector<std::string> mine = { L[p] };
  if (n > 1) mine.push_back(L[(p + 1) % n]);
  bool keepSaid = false;
  std::string p2;
  for (auto& sec : mine) { if (sec.rfind("letter.line.keep", 0) == 0) keepSaid = true; p2 += (p2.empty() ? "" : " ") + Fill(Pick(t, sec, Hash(id, 33, (uint32_t)p)), v); }
  out.push_back(p2);
  std::string kt; SourceOf(t, "keep", f.keep, &kt);
  v["incl"] = keepSaid || f.keep.empty() || kt.find("|dear|") == std::string::npos ? "" : ", including " + v["keep"] + ",";   // never a joke or a vice
  bool sank = k == 's' || fate.by == "sank" || fate.sunk;
  std::string fx = Pick(t, k == 's' ? "letter.effects.s" : sank ? "letter.effects.sank" : k == 'm' ? "letter.effects.m" : k == 'c' ? "letter.effects.c" : "letter.effects", id);
  std::string cl = Pick(t, k == 'm' ? "letter.close.m" : k == 'c' ? "letter.close.c" : "letter.close", Hash(id, 34));
  out.push_back(Fill(fx, v) + " " + Fill(cl, v));
  // the family is "you": "his mother's handwriting" to his mother is "your handwriting", "his children" to his wife "your children"
  std::string hs = female ? "her " : "his ";
  std::vector<std::pair<std::string, std::string>> you;
  if (kin == "mother" || kin == "parents") you.push_back({ hs + "mother's", "your" });
  if (kin == "father" || kin == "parents") you.push_back({ hs + "father's", "your" });
  if (kin == "brother") you.push_back({ hs + "brother", "you" });
  if (kin == "sister") you.push_back({ hs + "sister", "you" });
  if (kin == "spouse") for (const char* w : { "children", "son", "daughter", "baby", "youngest" }) you.push_back({ hs + w, std::string("your ") + w });
  for (size_t i = 1; i < out.size(); i++)
    for (auto& y : you) for (size_t at; (at = out[i].find(y.first)) != std::string::npos;) out[i].replace(at, y.first.size(), y.second);
  for (auto& r : Sec(t, "letter.sign")) out.push_back(Untag(r));
  for (auto& o : out) o = CapFirst(crew_report_detail::Trim(o));
  return out;
}
