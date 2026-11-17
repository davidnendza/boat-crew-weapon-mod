// items from weapons.ini, rich-text colors, the mod's turret guns, side-grades and utility gear, LoadItems (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- items from weapons.ini ----------------------------------------------------------------------------------
struct Op { std::string field; bool mul = true; double value = 1; bool proj = false; bool mount = false; bool sphere = false; bool fx = false; };   // fx: read by the mod at run time (crits, on-kill effects)   // proj: applies to the gun's projectile types
struct Item {
  std::string name, family, base, description;
  int price = -1;                  // -1 keeps the base gun's price
  bool retired = false;
  int uses = 0;                    // missions left; 0 means unlimited (set on found guns)
  bool fromInventory = false;      // found in play (inventory.ini) rather than written by hand in weapons.ini
  float scale = 1.0f;              // size of the mounted gun (1 = stock); makes a gun easy to spot
  std::string rarity;              // Common, Uncommon or Rare for found guns; colors the name
  int gx = -1, gy = -1;            // cell of the top-left corner in the inventory grid (-1: not placed)
  int gw = 0, gh = 0;              // size in cells; 0 takes the default for the base gun (see ItemSize)
  bool held = false;               // in the loot grid: found this mission, not kept yet (no position, not on any boat)
  std::string fragmentFrom;        // fragment_from=<stock gun>: exploding shells burst into that gun's first ammo type
  std::string projectileFrom;      // projectile_from=<stock gun>: fires copies of that gun's projectiles (see CloneProjectiles)
  std::string model;               // model=<file in BoatCrewMod\models>: an .obj that replaces the gun's look (see ApplyModel)
  float modelScale = 1.0f;         // model_scale=: size of the model (1 = its .obj units are metres)
  bool modelOwnMaterial = false;   // model_material=own keeps the .obj's materials; default 'gun' uses the stock gun's
  std::vector<std::string> modelKeep;   // model_keep=Mount,Bullets: stock parts (transform name prefixes) left visible
  bool builtIn = false;            // one of the mod's own guns (kTurretGuns, kSideGrades), not from weapons.ini
  std::string soundFrom;           // sound_from=<stock gun>: fires with that gun's sounds (see CopySounds)
  std::string builtFrom;           // built_from=<mod gun>: a found gun on one of kTurretGuns or kSideGrades (see ExpandBuiltFrom)
  int cratesPerRound = 0;          // crates_per_round=N: only every Nth ammo crate a loader brings adds a round (see Hooked_SetMunitions)
  std::string vetCrew; int vetKills = 0;   // Veteran's (0.9.3): the gunner it is bonded to and his kills with it (vet_crew=, vet_kills=)
  std::vector<Op> ops;
};
static std::vector<Item> g_items;
// Torpedo slot families (the TorpedoLaunchers' SelectionOrder names, read from the game data 2026-11-04; more are
// learned as launchers are filled). An item in one of them is a found torpedo (see the found torpedoes section).
static std::set<std::string> g_torpFams = { "New Standard Torpedo Slot", "Rear Torpedo Slot", "Sub Chaser Side Slot", "Sub Chaser Depth Charge Rack Slot", "Secondary Slot", "Depth Charge Slot" };
static bool IsTorpFamily(const std::string& f) { return g_torpFams.count(f) > 0; }
static bool g_torpOff = false;                                       // a torpedo hook or lookup failed: no torpedo drops
static double g_chompVolume = 1.0;                                    // drops.ini chomp_volume: the chompers' sound (0 = silent)
static void* TorpSprite(const std::string& base);
static void InjectTorpBoats();
static int TorpUnlist(const std::string& name);
static void TorpFamiliesOf(void* craft, std::vector<std::string>& out);
static void TorpEquipped(void* craft, std::vector<std::string>& out);
static void TorpUnequipRetired(void* craft);
// co-op (see the co-op section near the end): drops.ini switches, the session role and the lookups the effects share
static bool g_coopGuns = true, g_coopElites = true, g_coopVisuals = true, g_coopHelpers = true;
static bool g_coopTorps = true;                                       // drops.ini coop_torpedoes: other players' found torpedoes
static int g_coopTorpFaults = 0;                                      // -1: a fault switched the co-op torpedoes off
struct CoopCounts { int msgsOut, msgsIn, bytesOut, bytesIn, gunsSent, gunsIn, gunsBuilt, elitesSent, elitesApplied, helpersAsked, helpersMade, looksSent, looksApplied, killsSent, killsIn,
                    torpsIn, torpTiersBuilt, torpCopies, healsSent, healsIn; };   // this battle's co-op traffic (the summary in CoopBattleWatch)
static CoopCounts g_cc = {};
static int g_coopRole = 0;                                           // 0 no co-op game, 1 hosting other players, 2 joined a host
static const Item* ItemOfMount(void* mount);                         // the mod gun on a mount, another player's included
static bool IsRemoteMount(void* mount);                              // a gun built for another player's boat
static bool CoopHostElites();                                         // a client whose host sends its Rare/Elite rolls
static double ItemFx(const Item& it, const char* name) { for (const Op& o : it.ops) if (o.fx && o.field == name) return o.value; return 0; }
static const char* kMatName[3] = { "Scrap Iron", "Brass", "Gunmetal" };       // crate tiers 0/1/2, see g_crateCost
static const char* kCrateName[3] = { "Iron Crate", "Brass Crate", "Gunmetal Crate" };
static int g_mat[3] = { 0, 0, 0 }, g_crates[3] = { 0, 0, 0 };         // materials and unopened crates (top of inventory.ini)
static int g_invUniqueUses = 0;                                        // inventory.ini unique_uses=: 0 = saved while Uniques lasted 6 missions (RefreshUniques)
static bool g_tracerRarity = true;
static bool g_bannerOn = true;                                       // drops.ini modded_banner: MODDED on the title screen (see TitleBanner)
static double g_bannerPlace[4] = { 0.324, 0.201, 0.197, 0.55 };     // modded_banner_place: center x, top, width (shares of the screen height), squash
static bool g_elitesOn = true, g_eliteTags = true, g_eliteAnnounce = true;
static bool g_tracerAtLaunch = true, g_elitesAtLaunch = true;   // tracer_rarity / enemies enabled as read at launch; the runtime fallbacks clear only the live flags
static double g_rareChance = 0.08, g_eliteChance = 0.02, g_eliteHp = 1.75, g_tagRange = 3000, g_rareCallin = 2, g_eliteCallin = 3;
static std::vector<RepPara> g_aarParas; static int g_aarSeq = 0;      // the last battle's prose report (crew log), for the after-battle window
static bool g_aarOpen = false, g_aarOff = false;                       // the after-action report window; off after a fault
static bool g_optOpen = false, g_newsOpen = false, g_optInit = false;                  // the Mod Options / What's New windows
static double g_priceScale = 1.0;                                  // drops.ini price_scale: found guns cost their base gun's RP price x this (0 = free)
static double g_cosmeticChance = 0.3;                               // drops.ini cosmetic_chance: share of found guns with one cosmetic extra                                  // drops.ini tracer_rarity: Uncommon guns fire blue tracers, Rare gold
static bool g_tracerSet = false;                                    // weapons.ini tracer_color: recolors the BasicYellow projectile material
static float g_tracerRGBA[4] = { 1, 1, 1, 1 };

// "red", "green", ... or "r,g,b" with values 0..1 (more than 1 is allowed, it is just brighter)
static bool ParseColor(const std::string& v, float* out) {
  struct Named { const char* n; float r, g, b; };
  static const Named named[] = {
    { "red", 1.0f, 0.04f, 0.0f }, { "green", 0.1f, 1.0f, 0.12f }, { "blue", 0.15f, 0.35f, 1.0f }, { "cyan", 0.05f, 0.9f, 1.0f },
    { "magenta", 1.0f, 0.1f, 0.9f }, { "orange", 1.0f, 0.4f, 0.0f }, { "purple", 0.55f, 0.1f, 1.0f }, { "yellow", 1.0f, 0.85f, 0.0f }, { "white", 1.0f, 1.0f, 1.0f } };
  for (const Named& c : named) if (v == c.n) { out[0] = c.r; out[1] = c.g; out[2] = c.b; out[3] = 1.0f; return true; }
  float r, g, b;
  if (sscanf(v.c_str(), "%f , %f , %f", &r, &g, &b) == 3) { out[0] = r; out[1] = g; out[2] = b; out[3] = 1.0f; return true; }
  return false;
}

static const char* kDefaultIni =
  "# BoatCrewWeaponMod: extra guns for the base customization screen.\n"
  "#\n"
  "# Each [item] copies one stock gun into one slot family. Fields:\n"
  "#   name         what the list shows (keep it short, long names wrap onto a second line)\n"
  "#   family       the slot family: Tub Turret, Heavy Weapon, Heavy+ Weapon, Medium Weapon, Utility Weapon, Rockets or Crew Weapons (the log\n"
  "#                lists every family and the guns in it the first time it is seen)\n"
  "#   base         the stock gun to copy, by its GunType asset name (see the catalogue lines in weaponmod.log)\n"
  "#   description  optional text for the stat card\n"
  "#   price        optional RP price; omit to keep the base gun's price\n"
  "#   mul.<field>  multiply a numeric gun field, e.g. mul.roundsPerMinute=2\n"
  "#   set.<field>  set a numeric or true/false gun field, e.g. set.MagSize=120 or set.infiniteAmmo=1\n"
  "#   mount.set.<field> / sphere.set.<field>  edits on the gun mount or its targeting sphere, e.g. mount.set.requiresCrew=0\n"
  "#   proj.mul.<field> / proj.set.<field>  the same edits applied to the gun's projectiles, e.g. proj.mul.BaseDamage=2,\n"
  "#                proj.mul.SplashDamageRange=3 (fields: BaseDamage SplashDamageBase SplashDamageRange MuzzleVelocity ...)\n"
  "#                proj.set.style=BasicYellow changes the tracer look (BasicWhite, BasicYellowish, BasicYellow, Rocket, Rocket_ASW, Shell)\n"
  "#   tracer_color=red   (a line on its own, outside any item) recolors the BasicYellow tracer material. Names: red green blue\n"
  "#                cyan magenta orange purple yellow white, or r,g,b like 1,0.2,0. Items opt in with proj.set.style=BasicYellow.\n"
  "#   scale        size of the mounted gun, e.g. scale=1.5 (1 = stock). Handy for spotting a modded gun\n"
  "#   retired      1 hides the gun but keeps its position, so saves that used it keep loading\n"
  "#   model        experimental: an .obj file in BoatCrewMod\\models that replaces the gun's look. Objects named\n"
  "#                stand* or base* sit on the gun's base, barrel* on each barrel (they recoil), yaw* turn with the aim\n"
  "#                but do not elevate, the rest turns and elevates with the aim. model_scale=1 means the .obj is in metres; model_material=gun paints it like the stock gun,\n"
  "#                model_material=own keeps the colors from its .mtl file. model_keep=Mount,Bullets leaves the stock\n"
  "#                gun's parts whose names start with those words in place (see the README)\n"
  "#   elevation=-5,30  lowest and highest barrel angle in degrees (negative = below level); crew do not pick targets outside it\n"
  "#   crew_offset=0,0,-0.3  moves the crew's spots at the gun by x,y,z metres (the gun's right, up, forward)\n"
  "#   projectile_from=<stock gun>  fires copies of that gun's shells instead of its own; proj.* edits then act on those\n"
  "#   crates_per_round=10  (2-100) only every Nth ammo crate a loader brings adds rounds\n"
  "#\n"
  "# IMPORTANT: items get their position in the slot from their order in this file. Do not delete or reorder items you\n"
  "# have equipped in a save; add new ones at the bottom and use retired=1 to take one away.\n"
  "\n"
  "# Examples (remove the # in front of an item's lines to use it):\n"
  "# [item]\n"
  "# name=Lewis X2\n"
  "# family=Tub Turret\n"
  "# base=30 Cal Dual MG (Lewis)\n"
  "# description=Lewis gun with double the rate of fire.\n"
  "# price=0\n"
  "# mul.roundsPerMinute=2\n"
  "# mul.shotCooldown=0.5\n"
  "#\n"
  "# [item]\n"
  "# name=Browning HV\n"
  "# family=Tub Turret\n"
  "# base=M2 Browning\n"
  "# description=Browning tuned for sustained fire.\n"
  "# price=0\n"
  "# mul.roundsPerMinute=1.5\n"
  "# mul.shotCooldown=0.667\n"
  "# mul.reloadCooldown=0.5\n"
  "#\n"
  "# [item]\n"
  "# name=37mm Rapid\n"
  "# family=Utility Weapon\n"
  "# base=M3 37 mm AT\n"
  "# description=Anti-tank gun with a fast reload.\n"
  "# price=0\n"
  "# mul.roundsPerMinute=2\n"
  "# mul.shotCooldown=0.5\n"
  "# mul.reloadCooldown=0.4\n"
  "#\n"
  "# [item]\n"
  "# name=Oerlikon Hot\n"
  "# family=Heavy Weapon\n"
  "# base=20 mm Oerlikon\n"
  "# description=Oerlikon with a much higher rate of fire.\n"
  "# price=0\n"
  "# mul.roundsPerMinute=2.5\n"
  "# mul.shotCooldown=0.4\n"
  "#\n";

// "true", "BasicYellow" and plain numbers. Style names map to the game's ProjectileStyle enum numbers.
static double ParseValue(const std::string& field, const std::string& v) {
  if (v == "true" || v == "True") return 1.0;
  if (v == "false" || v == "False") return 0.0;
  if (field == "style") {
    static const char* names[] = { "BasicWhite", "BasicYellowish", "BasicYellow", "Rocket", "Rocket_ASW", "Shell" };
    for (int i = 0; i < 6; i++) if (v == names[i]) return i;
  }
  return atof(v.c_str());
}

// "mul.X", "set.X", "proj.mul.X", "mount.set.X", "sphere.set.X": what an edit applies to (the gun type, its projectiles,
// the gun mount itself or the mount's targeting sphere), whether it multiplies or sets, and the field.
static bool ParseOpKey(const std::string& key, Op* op) {
  std::string k = key;
  op->proj = op->mount = op->sphere = op->fx = false;
  if (k.compare(0, 3, "fx.") == 0) { op->fx = true; k = k.substr(3); }
  else if (k.compare(0, 5, "proj.") == 0) { op->proj = true; k = k.substr(5); }
  else if (k.compare(0, 6, "mount.") == 0) { op->mount = true; k = k.substr(6); }
  else if (k.compare(0, 7, "sphere.") == 0) { op->sphere = true; k = k.substr(7); }
  if (k.compare(0, 4, "mul.") == 0) op->mul = true;
  else if (k.compare(0, 4, "set.") == 0) op->mul = false;
  else return false;
  op->field = k.substr(4);
  return !op->field.empty();
}
static const char* OpPrefix(const Op& o) { return o.fx ? "fx." : (o.proj ? "proj." : (o.mount ? "mount." : (o.sphere ? "sphere." : ""))); }
// fx.set.<name> values: critChance, critMult (Deadeye), killAmmo, killRepair, killRP (on-kill effects). 0 when absent.
// 0.9.3 (see 'gun effects'): mark, markTime, giantSlayer, salvo, closeBonus/closeDist, longBonus/longDist, overkill,
// vetPerKill/vetCap, rally, steady/steadyCap/steadyDecay, saveAmmo, brittle.
static double ItemFx(const struct Item& it, const char* name);
static int RarityOf(const std::string& r) { return r == "Unusual" ? 4 : r == "Unique" ? 3 : (r == "Rare" ? 2 : (r == "Uncommon" ? 1 : 0)); }
// What scrapping a gun gives, and wearing it out too: materials of its rarity's tier (tier 0 Scrap Iron for Common,
// 1 Brass for Uncommon, 2 Gunmetal for Rare and up). One, but a Unique gives 2 Gunmetal and an Unusual 3. A worn-out gun
// gave 1 Scrap Iron whatever its rarity, so players would scrap a gun by hand on its last mission (David 2026-10-30).
static int ScrapYield(const std::string& rarity, int* tier) { int rq = RarityOf(rarity); *tier = rq > 2 ? 2 : rq; return rq == 4 ? 3 : (rq == 3 ? 2 : 1); }

// ---- rich-text colors (TextMeshPro <color> tags) --------------------------------------------------------------------
static bool g_colorText = true;                                      // drops.ini color_text=0 turns the colors off
static const char* kGoodHex = "#66d17a";
static const char* kBadHex = "#ff5a5a";
static const char* kFunHex = "#c39bd3";                              // cosmetic 'Look:' lines
static const char* RarityHex(const std::string& r) { return r == "Unusual" ? "#ff5fc8" : r == "Unique" ? "#ff7a1a" : (r == "Rare" ? "#ffb300" : (r == "Uncommon" ? "#4fc3f7" : "#d8d8d8")); }
static std::string Colored(const std::string& text, const char* hex) {
  return g_colorText ? std::string("<color=") + hex + ">" + text + "</color>" : text;
}
// Mount names may carry color tags; anything that matches a mount to an item compares the plain text.
static std::string StripTags(const std::string& s) {
  std::string o; bool in = false;
  for (char c : s) { if (c == '<') { in = true; continue; } if (c == '>' && in) { in = false; continue; } if (!in) o += c; }
  return o;
}

static std::string Trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static std::string g_folder;                                        // BoatCrewMod folder next to the game exe

static std::vector<std::string> SplitList(const std::string& v) {   // "Mount, Bullets" -> { "Mount", "Bullets" }
  std::vector<std::string> out;
  for (size_t a = 0; a <= v.size(); ) {
    size_t c = v.find(',', a); if (c == std::string::npos) c = v.size();
    std::string s = Trim(v.substr(a, c - a)); if (!s.empty()) out.push_back(s);
    a = c + 1;
  }
  return out;
}
static std::string JoinList(const std::vector<std::string>& v) { std::string s; for (const std::string& x : v) s += (s.empty() ? "" : ",") + x; return s; }

// Read [item] blocks (and the global tracer_color line) from text into g_items. Returns how many were added.
// items: g_items, or another player's guns in co-op (their text never sets tracer_color, materials or crates)
static int ParseItemsInto(std::vector<Item>& items, const std::string& text, const char* label, bool inv) {
  size_t first = items.size();
  Item* cur = nullptr;
  for (size_t at = 0; at < text.size(); ) {
    size_t nl = text.find('\n', at); if (nl == std::string::npos) nl = text.size();
    std::string l = Trim(text.substr(at, nl - at));
    at = nl + 1;
    if (l.empty() || l[0] == '#' || l[0] == ';') continue;
    if (l == "[item]") { items.emplace_back(); cur = &items.back(); continue; }
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
    if (k == "tracer_color") {
      if (&items != &g_items) continue;                                  // another player's text never recolors this machine's tracers
      if (ParseColor(v, g_tracerRGBA)) { g_tracerSet = true; Log("%s: tracer_color = (%.2f, %.2f, %.2f)", label, g_tracerRGBA[0], g_tracerRGBA[1], g_tracerRGBA[2]); }
      else Log("%s: tracer_color '%s' is not a color name or r,g,b", label, v.c_str());
      continue;
    }
    if (!cur) {
      if (inv && k == "materials") sscanf(v.c_str(), "%d , %d , %d", &g_mat[0], &g_mat[1], &g_mat[2]);
      else if (inv && k == "crates") sscanf(v.c_str(), "%d , %d , %d", &g_crates[0], &g_crates[1], &g_crates[2]);
      else if (inv && k == "unique_uses") g_invUniqueUses = atoi(v.c_str());
      continue;
    }
    if (k == "name") cur->name = v;
    else if (k == "family") cur->family = v;
    else if (k == "base") cur->base = v;
    else if (k == "description") cur->description = v;
    else if (k == "price") cur->price = atoi(v.c_str());
    else if (k == "retired") cur->retired = atoi(v.c_str()) != 0;
    else if (k == "uses") cur->uses = atoi(v.c_str());
    else if (k == "rarity") cur->rarity = v;
    else if (k == "held") cur->held = atoi(v.c_str()) != 0;
    else if (k == "fragment_from") cur->fragmentFrom = v;
    else if (k == "projectile_from") cur->projectileFrom = v;
    else if (k == "elevation") {                                      // stored as mount ops, so saves, built_from and co-op carry it
      // GunMount Angle Limits (fixPivot 0x180975080, isBlockedByDepression 0x180976F00): the pivot's pitch stays within
      // gunSad..gunHappy degrees, positive up, so elevation=<down>,<up> is gunSad,gunHappy (-5,30: 5 below level to 30 up)
      float dn, up;
      if (sscanf(v.c_str(), "%f , %f", &dn, &up) == 2 && dn >= -90 && up <= 90 && dn < up) {
        Op o; o.mount = true; o.mul = false; o.field = "gunSad"; o.value = dn; cur->ops.push_back(o);
        o.field = "gunHappy"; o.value = up; cur->ops.push_back(o);
      } else Log("%s: elevation '%s' is not <down>,<up> degrees from -90 to 90; ignored", label, v.c_str());
    }
    else if (k == "crew_offset") {                                    // fx ops for the same reason; read by ApplyCrewOffset
      float x, y, z;
      if (sscanf(v.c_str(), "%f , %f , %f", &x, &y, &z) == 3 && fabs(x) <= 3 && fabs(y) <= 3 && fabs(z) <= 3) {
        Op o; o.fx = true; o.mul = false; o.field = "crewOffsetX"; o.value = x; cur->ops.push_back(o);
        o.field = "crewOffsetY"; o.value = y; cur->ops.push_back(o); o.field = "crewOffsetZ"; o.value = z; cur->ops.push_back(o);
      } else Log("%s: crew_offset '%s' is not x,y,z metres (each within 3); ignored", label, v.c_str());
    }
    else if (k == "pivot_offset") {                                   // fx ops like crew_offset; read by ApplyPivotOffset
      float x, y, z;
      if (sscanf(v.c_str(), "%f , %f , %f", &x, &y, &z) == 3 && fabs(x) <= 3 && fabs(y) <= 3 && fabs(z) <= 3) {
        Op o; o.fx = true; o.mul = false; o.field = "pivotOffsetX"; o.value = x; cur->ops.push_back(o);
        o.field = "pivotOffsetY"; o.value = y; cur->ops.push_back(o); o.field = "pivotOffsetZ"; o.value = z; cur->ops.push_back(o);
      } else Log("%s: pivot_offset '%s' is not x,y,z metres (each within 3); ignored", label, v.c_str());
    }
    else if (k == "sound_from") cur->soundFrom = v;
    else if (k == "built_from") cur->builtFrom = v;
    else if (k == "crates_per_round") { int c = atoi(v.c_str()); cur->cratesPerRound = c >= 2 && c <= 100 ? c : 0; }
    else if (k == "vet_crew") cur->vetCrew = v;
    else if (k == "vet_kills") cur->vetKills = atoi(v.c_str());
    else if (k == "model") cur->model = v;
    else if (k == "model_scale") { float sc = (float)atof(v.c_str()); if (sc > 0.01f && sc < 100.f) cur->modelScale = sc; }
    else if (k == "model_material") cur->modelOwnMaterial = (v == "own");
    else if (k == "model_keep") cur->modelKeep = SplitList(v);
    else if (k == "grid") { int x, y; if (sscanf(v.c_str(), "%d , %d", &x, &y) == 2) { cur->gx = x; cur->gy = y; } }
    else if (k == "size") { int w, h; if (sscanf(v.c_str(), "%d x %d", &w, &h) == 2 && w > 0 && h > 0 && w <= 8 && h <= 8) { cur->gw = w; cur->gh = h; } }
    else if (k == "scale") { float sc = (float)atof(v.c_str()); if (sc > 0.1f && sc < 10.f) cur->scale = sc; }
    else if (k == "from_inventory") { if (&items != &g_items) cur->fromInventory = atoi(v.c_str()) != 0; }   // co-op text only: how another player's gun was made
    else if (k == "built_in") { if (&items != &g_items) cur->builtIn = atoi(v.c_str()) != 0; }
    else { Op o; if (ParseOpKey(k, &o)) { o.value = o.mul ? atof(v.c_str()) : ParseValue(o.field, v); cur->ops.push_back(o); } else Log("%s: unknown key '%s'", label, k.c_str()); }
  }
  for (size_t i = first; i < items.size(); ) {                        // drop incomplete items and repeated names
    Item& it = items[i];
    bool dup = false;
    for (size_t j = 0; j < i; j++) if (items[j].name == it.name) dup = true;
    if (it.name.empty() || it.family.empty() || it.base.empty()) { Log("%s: an [item] is missing name, family or base; ignored", label); items.erase(items.begin() + i); }
    else if (it.name[0] == '#' || it.name[0] == ';' || it.name[0] == '[') {   // positions.ini would read it as a comment or a heading
      Log("%s: item name '%s' cannot start with # ; or [; ignored", label, it.name.c_str()); items.erase(items.begin() + i);
    }
    else if (dup && inv) {                                            // found guns are never thrown away: rename the copy
      std::string base = it.name, nn; int k = 2;
      do { nn = base + " " + std::to_string(k++); } while (std::any_of(items.begin(), items.end(), [&](const Item& o) { return o.name == nn; }));
      Log("%s: item '%s' repeats an earlier name; renamed to '%s'", label, base.c_str(), nn.c_str());
      it.name = nn; i++;
    }
    else if (dup) { Log("%s: item '%s' repeats an earlier name; ignored", label, it.name.c_str()); items.erase(items.begin() + i); }
    else i++;
  }
  if (inv) for (size_t i = first; i < items.size(); i++) items[i].fromInventory = true;
  return (int)(items.size() - first);
}
static int ParseItemsText(const std::string& text, const char* label, bool inv) { return ParseItemsInto(g_items, text, label, inv); }
// The same from a file. Returns -1 if the file is missing.
static int ParseItemsFile(const char* path, const char* label, bool inv) {
  FILE* f = fopen(path, "rb");
  if (!f) return -1;
  std::string text; char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  fclose(f);
  return ParseItemsText(text, label, inv);
}

// ---- the mod's own turret guns ----
// Period guns for the PT boat's Tub Turret slots, each a copy of the stock gun it is closest to, with a
// model from tools/make_turret_models.py. model_keep leaves the stock ring mount, belts, pedestal, cradle and shoulder
// rests in place, so only the gun itself is new. Stock prices for scale: Lewis 500, M1919 1500, M2 2500, Oerlikon 4500.
// Ladder: Vickers K sits beside the M1919 (more bullets, shorter reach), Vickers .50 beside the M2 (faster, softer),
// Type 93 above the M2, Flak 38 beside the Oerlikon (faster, smaller magazine), Type 96 and the M4 above it.
// Positions are kept by name in positions.ini, so new guns are only ever added at the bottom.
// Sounds (sound_from): the Vickers K takes the Japanese 7.7 mm ship MG (the same .303-class round), the Type 93 the
// IJN armored boats' MG ('Generic MG IJN' in the sound bank), the Type 96 the enemy's own 25 mm, the M4 the player's
// 37 mm M4. The Vickers .50 and Flak 38 keep the M2's and the Oerlikon's: the game has no closer sounds.
static const char* kTurretGuns =
  "[item]\nname=Twin Vickers K\nfamily=Tub Turret\nbase=30 Cal Dual MG (Lewis)\nprice=1500\nmodel=twin_vickers_k\nmodel_keep=Mount\nsound_from=7.7 mm MG\n"
  "description=British twin Vickers K .303 from the bridge wings of the Coastal Forces' gunboats. Much faster than the Lewis with lighter rounds and less reach: best up close.\n"
  "mul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nset.MagSize=96\nset.MaxAmmo=288\nset.ammoPerFill=120\nset.burstNumberOfShots=12\nset.Accuracy=85\nset.engagementDistance=650\n"
  "proj.mul.BaseDamage=0.9\nproj.mul.SplashDamageBase=0.9\n\n"
  "[item]\nname=Twin Vickers .50\nfamily=Tub Turret\nbase=M2 Browning\nprice=2500\nmodel=twin_vickers_50\nmodel_keep=Mount,Bullets\n"
  "description=British twin Vickers .50 Mk III, water-cooled. Fires faster than the M2 and holds more belt, but hits a little softer and reaches less far.\n"
  "mul.roundsPerMinute=1.25\nmul.shotCooldown=0.8\nset.MagSize=80\nset.MaxAmmo=160\nset.ammoPerFill=50\nset.Accuracy=90\nset.engagementDistance=1000\n"
  "proj.mul.BaseDamage=0.85\nproj.mul.SplashDamageBase=0.85\nproj.mul.MuzzleVelocity=0.85\nproj.mul.MuzzleVelocityRealistic=0.85\nproj.mul.FrameLifeTime=1.18\n\n"
  "[item]\nname=Twin Type 93\nfamily=Tub Turret\nbase=M2 Browning\nprice=3500\nmodel=twin_type93\nmodel_keep=Mount\nsound_from=Generic Gun IJN\n"
  "description=Captured Japanese twin Type 93 13.2 mm. Heavier rounds and a longer reach than the M2, fed from 30-round boxes.\n"
  "mul.roundsPerMinute=0.8\nmul.shotCooldown=1.25\nset.MagSize=30\nset.reloadCooldown=2.5\nset.ammoPerFill=30\nset.engagementDistance=1300\n"
  "proj.mul.BaseDamage=1.35\nproj.mul.SplashDamageBase=1.2\nproj.mul.SplashDamageRange=1.2\nproj.mul.MuzzleVelocity=0.9\nproj.mul.MuzzleVelocityRealistic=0.9\nproj.mul.FrameLifeTime=1.11\n\n"
  "[item]\nname=2 cm Flak 38\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=4500\nmodel=flak38\nmodel_keep=Mount,Mount3_Holder,ArmRest\n"
  "description=German 2 cm Flak 38. A faster action and faster shells than the Oerlikon, but only 20 rounds before a magazine change.\n"
  "mul.roundsPerMinute=1.35\nmul.shotCooldown=0.74\nset.MagSize=20\nset.reloadCooldown=3\nset.Accuracy=92\nset.burstNumberOfShots=10\n"
  "proj.mul.MuzzleVelocity=1.1\nproj.mul.MuzzleVelocityRealistic=1.1\n\n"
  "[item]\nname=Type 96 25 mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=6000\nmodel=type96_25mm\nmodel_keep=Mount,Mount3_Holder,ArmRest\nsound_from=25 mm Type96\n"
  "description=Captured Japanese Type 96 25 mm. A heavier shell with a bigger burst than any 20 mm, but slower and fed from 15-round boxes.\n"
  "mul.roundsPerMinute=0.8\nmul.shotCooldown=1.25\nset.MagSize=15\nset.reloadCooldown=3\nset.Accuracy=88\nset.engagementDistance=1350\n"
  "proj.mul.BaseDamage=1.75\nproj.mul.SplashDamageBase=1.6\nproj.mul.SplashDamageRange=1.25\nproj.mul.MuzzleVelocity=0.9\nproj.mul.MuzzleVelocityRealistic=0.9\nproj.mul.FrameLifeTime=1.11\n\n"
  "[item]\nname=M4 Barge Buster\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=7500\nmodel=m4_barge_buster\nmodel_keep=Mount,Mount3_Holder\nsound_from=M4 37 mm Autocannon\n"
  "description=A 37 mm M4 cannon salvaged from a crashed P-39, as PT crews on Guadalcanal did to hunt supply barges. Slow, but every shell lands like a hammer.\n"
  "mul.roundsPerMinute=0.47\nmul.shotCooldown=2.13\nset.MagSize=15\nset.MaxAmmo=30\nset.reloadCooldown=5\nset.Accuracy=93\nset.engagementDistance=1400\nset.ammoPerFill=10\nset.burstNumberOfShots=5\nset.burstCooldown=0.8\n"
  "proj.mul.BaseDamage=3.75\nproj.mul.SplashDamageBase=2.5\nproj.mul.SplashDamageRange=1.4\nproj.mul.MuzzleVelocity=0.85\nproj.mul.MuzzleVelocityRealistic=0.85\nproj.mul.FrameLifeTime=1.18\n\n"
  // batch 2 (2026-11-15), Medium group: Breda, Madsen and Polsten on the Oerlikon, MG 34 on the Lewis, DShK on the M2
  "[item]\nname=Breda 20/65 Mod. 40\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=4000\nmodel=breda_20mm\nmodel_keep=Mount,Mount3_Holder,ArmRest\n"
  "description=Italian Breda 20/65, the Regia Marina's standard 20 mm on corvettes and MAS boats. Fed from 12-round strips, so it fires far fewer shells than the Oerlikon, but it aims truer, hits a little harder and reaches further.\n"
  "mul.roundsPerMinute=0.6\nmul.shotCooldown=1.667\nset.MagSize=12\nset.reloadCooldown=1.8\nset.Accuracy=96\nset.engagementDistance=1500\nset.burstNumberOfShots=6\n"
  "proj.mul.BaseDamage=1.1\nproj.mul.SplashDamageBase=1.1\nproj.mul.FrameLifeTime=1.25\n\n"
  "[item]\nname=Madsen 20 mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=4000\nmodel=madsen_20mm\nmodel_keep=Mount,Mount3_Holder,ArmRest\n"
  "description=Danish Madsen 20 mm, the Finnish Navy's light AA gun on its torpedo boats, fed from a 40-round drum. Slower than the Oerlikon, but more accurate, with faster shells and a longer reach.\n"
  "mul.roundsPerMinute=0.7\nmul.shotCooldown=1.43\nset.MagSize=40\nset.MaxAmmo=60\nset.Accuracy=93\nset.engagementDistance=1320\n"
  "proj.mul.MuzzleVelocity=1.05\nproj.mul.MuzzleVelocityRealistic=1.05\nproj.mul.FrameLifeTime=1.05\n\n"
  "[item]\nname=Polsten 20 mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\nprice=2700\nmodel=polsten_20mm\nmodel_keep=Mount,Mount3_Holder,ArmRest,Oerlikon Barrel,Oerlikon Recoil,Oerlikon Spring,Oerlikon Sights\n"
  "description=Polish-British Polsten, the Oerlikon simplified to a fraction of its parts and cost, with a 30-round box. Far cheaper than the Oerlikon, but a little less accurate and slower to reload.\n"
  "set.Accuracy=85\nset.reloadCooldown=5\n\n"
  "[item]\nname=Twin MG 34\nfamily=Tub Turret\nbase=30 Cal Dual MG (Lewis)\nprice=1200\nmodel=twin_mg34\nmodel_keep=Mount\nsound_from=30-06 Cal Single MG AIR (USA)\n"
  "description=German twin MG 34 on the standard twin AA mount, with 75-round saddle drums. Fires faster than the Lewis, but it is less accurate and shorter-ranged, each round hits a little softer, and changing both drums takes a long time.\n"
  "mul.roundsPerMinute=1.3\nmul.shotCooldown=0.77\nset.MagSize=150\nset.MaxAmmo=300\nset.ammoPerFill=150\nset.reloadCooldown=5\nset.Accuracy=78\nset.engagementDistance=650\nset.burstNumberOfShots=12\n"
  "proj.mul.BaseDamage=0.9\nproj.mul.SplashDamageBase=0.9\n\n"
  "[item]\nname=Twin DShK\nfamily=Tub Turret\nbase=M2 Browning\nprice=2000\nmodel=twin_dshk\nmodel_keep=Mount,Bullets\nsound_from=12.7 mm MG AIR (JAP)\n"
  "description=Soviet twin DShK 12.7 mm, as on the Soviet Navy's patrol and torpedo boats. Cheaper than the M2 and each round hits a little harder, but its short belts mean frequent reloads, and it is less accurate.\n"
  "set.MagSize=30\nset.reloadCooldown=1.6\nset.Accuracy=84\n"
  "proj.mul.BaseDamage=1.1\nproj.mul.SplashDamageBase=1.1\nproj.mul.MuzzleVelocity=0.95\nproj.mul.MuzzleVelocityRealistic=0.95\n";
static bool g_turretGuns = true;                                    // drops.ini turret_guns: 0 hides them (positions kept)

// ---- side-grades for the other slots ----
// Like the turret guns: each a copy of the stock gun it trades against, with a model from tools/make_sidegrade_models.py
// that keeps the base gun's carriage, stand and cranks. family= may list several slot families: the gun is copied into
// each (LoadItems), as a Bofors sits in both Heavy and Heavy+ slots. Prices are mul.price on the base gun's.
// Pom-pom and 4.7-inch follow Discord feedback (2026-11-01): the pom-pom has a bigger belt and twice the Bofors' rate
// but much worse accuracy and slower shells; the 4.7-inch has 3 rounds a magazine to the 5-inch's 2.
// Stock numbers (Dump/Guns, Dump/Projectiles 2026-11-01): Bofors 200 rpm, 6-round clips, 24 carried, 250 damage, 1000 m/s;
// 3-inch 75 rpm, 4/8, 800 damage, 400 splash over 30 m, 400 m/s; 5-inch 75 rpm, 2/4, 3200 damage, 800 over 40 m, 360 m/s;
// Twin Oerlikon 640 rpm, 60/120, reload 6, 32 damage; Elco 1920 rpm, 60/480; 60 mm mortar 8 carried, reload 1.5, 600
// splash over 90 m; 37 mm AT 4 carried, reload 3, 2400 damage; 75 mm howitzer 4 carried, reload 3, 2400 splash over
// 110 m; Mk50 8/16, Mk50 Modded 14/28; BAR 650 rpm, 20-round magazine, reload 5, 10 damage, 850 m/s.
static const char* kSideGrades =
  // Heavy
  "[item]\nname=QF 2-pdr Pom-pom\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Bofors\nmodel=pompom_quad\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=British QF 2-pounder 'pom-pom' on a quad mount. Twice the Bofors' rate of fire from a 14-round belt, but its slower shells scatter wide and fall short.\n"
  "mul.price=1.2\nmul.roundsPerMinute=2\nmul.shotCooldown=0.5\nset.MagSize=14\nset.MaxAmmo=36\nset.ammoPerFill=8\nset.reloadCooldown=2.5\nset.Accuracy=82\nset.engagementDistance=1400\n"
  "proj.mul.BaseDamage=0.8\nproj.mul.SplashDamageBase=0.8\nproj.mul.MuzzleVelocity=0.72\nproj.mul.MuzzleVelocityRealistic=0.72\nproj.mul.FrameLifeTime=1.2\n\n"
  "[item]\nname=3.7 cm Flak M42\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Bofors\nmodel=flak_m42\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=German 3.7 cm Flak M42. Fires faster than the Bofors from 8-round clips, with lighter shells: better against planes, softer against hulls.\n"
  "mul.roundsPerMinute=1.3\nmul.shotCooldown=0.77\nset.MagSize=8\nset.MaxAmmo=32\nset.ammoPerFill=8\nset.reloadCooldown=1.3\nset.Accuracy=96\n"
  "proj.mul.BaseDamage=0.72\nproj.mul.SplashDamageBase=0.72\nproj.mul.SplashDamageRange=0.85\n\n"
  "[item]\nname=QF 6-pdr Molins\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=3-inch Gun Player\nmodel=molins_6pdr\nmodel_keep=Stand,US_3in_base,US_3in_shield,Bofors Hand Crank\n"
  "description=British QF 6-pounder with the Molins autoloader, as fitted to Coastal Forces gunboats. Fires faster than the 3-inch with faster, flatter shells, but the smaller shell has far less blast.\n"
  "mul.price=1.15\nmul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nset.MagSize=6\nset.MaxAmmo=12\nset.ammoPerFill=3\n"
  "proj.mul.BaseDamage=0.7\nproj.mul.SplashDamageBase=0.4\nproj.mul.SplashDamageRange=0.6\nproj.mul.MuzzleVelocity=1.45\nproj.mul.MuzzleVelocityRealistic=1.06\n\n"
  "[item]\nname=8.8 cm Flak 36\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=3-inch Gun Player\nmodel=flak88\nmodel_keep=Stand,US_3in_base,US_3in_shield,Bofors Hand Crank\n"
  "description=German 8.8 cm Flak 36, the famous 88, as carried on Siebel ferries and F-lighters. A heavier shell than the 3-inch and a longer reach, but three rounds to a load, fewer shells carried, a slower rate and a higher price.\n"
  "mul.price=1.5\nmul.roundsPerMinute=0.8\nmul.shotCooldown=1.25\nset.MagSize=3\nset.MaxAmmo=6\nset.engagementDistance=3600\n"
  "proj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.3\nproj.mul.SplashDamageRange=1.1\n\n"
  "[item]\nname=Type 96 25 mm Triple\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=20 mm Oerlikon Twin\nmodel=type96_triple\nmodel_keep=Mount,Stand,ArmRest\nsound_from=25 mm Type96\n"
  "description=Captured Japanese Type 96 25 mm triple mount. Three barrels and heavier shells than the twin Oerlikon, but it scatters its fire and reaches less far.\n"
  "mul.price=1.4\nmul.roundsPerMinute=1.15\nmul.shotCooldown=0.87\nset.MagSize=45\nset.MaxAmmo=90\nset.ammoPerFill=15\nset.reloadCooldown=5\nset.Accuracy=82\nset.engagementDistance=1000\n"
  "proj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.4\nproj.mul.SplashDamageRange=1.15\nproj.mul.MuzzleVelocity=0.9\nproj.mul.MuzzleVelocityRealistic=0.9\nproj.mul.FrameLifeTime=1.11\n\n"
  // Heavy+
  "[item]\nname=8 cm Type 3\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nmodel=type3_80mm\nmodel_keep=Stand,US_3in_base,Bofors Hand Crank\nsound_from=80 mm Type 3\n"
  "description=Captured Japanese 8 cm/40 Type 3. A step between the 3-inch and the 5-inch: a heavier shell than the 3-inch that flies faster, at a slightly slower rate.\n"
  "mul.price=1.35\nmul.roundsPerMinute=0.85\nmul.shotCooldown=1.18\nset.reloadCooldown=1.3\nset.engagementDistance=3200\n"
  "proj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.25\nproj.mul.SplashDamageRange=1.15\nproj.mul.MuzzleVelocity=1.2\nproj.mul.MuzzleVelocityRealistic=1.2\n\n"
  "[item]\nname=10 cm Type 98 Twin\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nmodel=type98_100mm\nmodel_keep=Stand,5in_wet_mount,5 inch 25 Hand Crank\nsound_from=100 mm Type98 Dual Barrel Turret\n"
  "description=Captured Japanese 10 cm/65 Type 98 twin from an Akizuki-class destroyer. Four shells before a reload and the longest reach of any gun, but each shell is lighter than the 5-inch's and the reload is long.\n"
  "mul.price=1.4\nmul.roundsPerMinute=1.3\nmul.shotCooldown=0.77\nset.MagSize=4\nset.MaxAmmo=8\nset.ammoPerFill=2\nset.reloadCooldown=3\nset.Accuracy=95\nset.engagementDistance=4500\n"
  "proj.mul.BaseDamage=0.6\nproj.mul.SplashDamageBase=0.7\nproj.mul.SplashDamageRange=0.85\nproj.mul.MuzzleVelocity=1.6\nproj.mul.MuzzleVelocityRealistic=1.58\n\n"
  "[item]\nname=QF 4.7-inch Mk IX\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nmodel=qf_47inch\nmodel_keep=Stand,5in_wet_mount,5 inch 25 Hand Crank\n"
  "description=British QF 4.7-inch Mk IX destroyer gun. Three rounds to the 5-inch's two and faster, flatter shells, but a lighter shell with a smaller blast.\n"
  "set.MagSize=3\nset.MaxAmmo=6\nset.Accuracy=95\n"
  "proj.mul.BaseDamage=0.8\nproj.mul.SplashDamageBase=0.75\nproj.mul.SplashDamageRange=0.85\nproj.mul.MuzzleVelocity=1.25\nproj.mul.MuzzleVelocityRealistic=1.25\n\n"
  // Medium (the Elco also sits in Heavy and Heavy+ slots)
  "[item]\nname=2 cm Flakvierling 38\nfamily=Heavy Weapon,Heavy+ Weapon,Medium Weapon\nbase=20 mm Elco Thunderbolt\nmodel=flakvierling\nmodel_keep=Stand,Dummy Stand,Thunderbolt LOD Group\n"
  "description=German 2 cm Flakvierling 38: four 20 mm guns on one mount. Fires slower than the Thunderbolt, but with faster shells, tighter aim and a bigger magazine.\n"
  "mul.roundsPerMinute=0.75\nmul.shotCooldown=1.33\nset.MagSize=80\nset.reloadCooldown=7\nset.Accuracy=93\n"
  "proj.mul.MuzzleVelocity=1.15\nproj.mul.MuzzleVelocityRealistic=1.15\n\n"
  "[item]\nname=Type 96 25 mm Twin\nfamily=Medium Weapon\nbase=20 mm Oerlikon Twin\nmodel=type96_dual\nmodel_keep=Mount,Stand,ArmRest\nsound_from=25 mm Type96\n"
  "description=Captured Japanese Type 96 25 mm twin mount. Heavier shells with a bigger burst than the twin Oerlikon, but a little slower, less accurate and fed from 15-round boxes.\n"
  "mul.price=1.1\nmul.roundsPerMinute=0.85\nmul.shotCooldown=1.18\nset.MagSize=30\nset.MaxAmmo=80\nset.ammoPerFill=15\nset.reloadCooldown=4\nset.Accuracy=86\nset.engagementDistance=1250\n"
  "proj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.4\nproj.mul.SplashDamageRange=1.2\nproj.mul.MuzzleVelocity=0.9\nproj.mul.MuzzleVelocityRealistic=0.9\nproj.mul.FrameLifeTime=1.11\n\n"
  // Utility
  "[item]\nname=M1 81 mm Mortar\nfamily=Utility Weapon\nbase=M2 60 mm Mortar\nmodel=mortar_81mm\n"
  "description=US 81 mm M1 mortar. A step between the 60 mm and the 107 mm: a much bigger blast than the 60 mm, but fewer rounds carried and a slower reload.\n"
  "mul.price=1.3\nset.MaxAmmo=6\nset.ammoPerFill=3\nset.reloadCooldown=2.2\nset.engagementDistance=1800\n"
  "proj.mul.BaseDamage=1.15\nproj.mul.SplashDamageBase=1.7\nproj.mul.SplashDamageRange=1.1\nproj.mul.MuzzleVelocity=1.1\nproj.mul.MuzzleVelocityRealistic=1.1\n\n"
  "[item]\nname=M18 57 mm Recoilless\nfamily=Utility Weapon\nbase=M3 37 mm AT\nmodel=m18_recoilless\nmodel_keep=Stand,Crank\n"
  "description=M18 57 mm recoilless rifle. Hits much harder than the 37 mm with a bigger blast, but its slow shell drops fast and the reload is long.\n"
  "mul.price=1.25\nset.reloadCooldown=5\n"
  "proj.mul.BaseDamage=1.4\nproj.mul.SplashDamageBase=1.6\nproj.mul.SplashDamageRange=1.5\nproj.mul.MuzzleVelocity=0.55\nproj.mul.MuzzleVelocityRealistic=0.55\n\n"
  "[item]\nname=Type 92 70 mm\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nmodel=type92_70mm\nmodel_keep=Stand,pack_howitzer_75mm_carriage,Crank\n"
  "description=Captured Japanese Type 92 70 mm battalion gun. A smaller blast than the 75 mm howitzer, but it reloads much faster and carries more shells.\n"
  "mul.price=0.85\nset.MaxAmmo=6\nset.ammoPerFill=3\nset.reloadCooldown=1.8\n"
  "proj.mul.SplashDamageBase=0.7\nproj.mul.SplashDamageRange=0.8\nproj.mul.MuzzleVelocity=0.8\nproj.mul.MuzzleVelocityRealistic=0.8\n\n"
  // Rockets (launchers have no fire rate; they trade load, blast and spread)
  "[item]\nname=7.2-inch Woofus\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nmodel=woofus_72inch\nmodel_keep=Mount\n"
  "description=US 7.2-inch 'Woofus' rocket rack. Only four rockets to a load, but each has a far bigger blast than the Mk 50's.\n"
  "mul.price=1.2\nset.MagSize=4\nset.MaxAmmo=8\nset.ammoPerFill=2\nset.Accuracy=72\n"
  "proj.mul.BaseDamage=2.5\nproj.mul.SplashDamageBase=3\nproj.mul.SplashDamageRange=1.8\nproj.mul.MuzzleVelocity=0.85\nproj.mul.MuzzleVelocityRealistic=0.85\nproj.mul.projectileWidth=1.5\n\n"
  "[item]\nname=Land Mattress\nfamily=Rockets\nbase=Mk50 Modded Launcher\nmodel=land_mattress\nmodel_keep=Mount\n"
  "description=British 'Land Mattress' launcher. A huge salvo of small rockets that spread wide and reach less far; each hits softer than the Mk 50's.\n"
  "set.MagSize=24\nset.MaxAmmo=48\nset.ammoPerFill=8\nset.Accuracy=70\n"
  "proj.mul.BaseDamage=0.6\nproj.mul.SplashDamageBase=0.65\nproj.mul.SplashDamageRange=0.9\nproj.mul.MuzzleVelocity=0.85\nproj.mul.MuzzleVelocityRealistic=0.85\n\n"
  // Crew weapons (infinite ammo, so MaxAmmo stays). CrewMemberCombat.LateUpdate (read 2026-11-01) only picks targets
  // within 270 m whatever the gun, holds aim 1 s, then fires as fast as the gun's cooldown allows, so a slow gun gets
  // few shots in the short time a boat is that close: the first Boys (x0.1 rate, x15 damage) fired 3 shots a battle.
  "[item]\nname=Thompson M1928\nfamily=Crew Weapons\nbase=M1918 BAR\nmodel=thompson_m1928\n"
  "description=Thompson M1928 with a 50-round drum. Fast and steady, but its pistol rounds hit soft and fall short.\n"
  "mul.roundsPerMinute=1.15\nmul.shotCooldown=0.87\nset.MagSize=50\nset.reloadCooldown=4\nset.Accuracy=88\nset.engagementDistance=450\nset.burstNumberOfShots=8\n"
  "proj.mul.BaseDamage=0.75\nproj.mul.SplashDamageBase=0.75\nproj.mul.MuzzleVelocity=0.5\nproj.mul.MuzzleVelocityRealistic=0.5\nproj.mul.FrameLifeTime=0.9\n\n"
  "[item]\nname=Johnson M1941 LMG\nfamily=Crew Weapons\nbase=M1918 BAR\nmodel=johnson_m1941\n"
  "description=Johnson M1941 light machine gun. A little slower than the BAR but more accurate, and it reloads in half the time.\n"
  "mul.price=1.1\nmul.roundsPerMinute=0.85\nmul.shotCooldown=1.18\nset.reloadCooldown=2.5\nset.Accuracy=96\n\n"
  "[item]\nname=Boys AT Rifle\nfamily=Crew Weapons\nbase=M1918 BAR\nmodel=boys_at_rifle\n"
  "description=British Boys anti-tank rifle. Five rounds and slower to fire, but each hits eight times as hard as a BAR round, with no blast.\n"
  "mul.price=1.2\nmul.roundsPerMinute=0.25\nmul.shotCooldown=4\nset.MagSize=5\nset.reloadCooldown=3\nset.Accuracy=98\nset.burstNumberOfShots=0\nset.burstCooldown=0\n"
  "proj.mul.BaseDamage=8\nproj.set.SplashDamage=0\nproj.set.SplashDamageBase=0\n\n"
  // batch 2 (2026-11-15): Heavy and Heavy+
  "[item]\nname=T-34 Turret 76 mm\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=3-inch Gun Player\nmodel=t34_turret\nmodel_keep=Stand\nsound_from=Soukou Tei Tank Gun\nelevation=-5,30\npivot_offset=0,0,1\ncrew_offset=0,0.4,0.67\n"
  "description=A T-34 tank turret with its 76 mm F-34 gun, as Soviet Project 1125 armoured river boats carried on the Volga, Dnieper and Danube. Cheap, with six ready rounds, but its slower shell reaches less far and it fires a little slower than the 3-inch. Like the tank's gun it elevates only to 30 degrees, so it cannot fire at planes overhead.\n"
  "mul.price=0.6\nmul.roundsPerMinute=0.8\nmul.shotCooldown=1.25\nset.MagSize=6\nset.MaxAmmo=8\nset.ammoPerFill=2\nset.reloadCooldown=1.2\nset.engagementDistance=1950\n"
  "proj.mul.MuzzleVelocity=0.8\nproj.mul.MuzzleVelocityRealistic=0.8\n\n"
  "[item]\nname=1.1-inch Chicago Piano\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Dual Bofors\nmodel=chicago_piano\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=US 1.1-inch/75 quad mount, the 'Chicago Piano', the Navy's standard close-range AA gun on destroyers and larger ships from 1937 until the Bofors and Oerlikon replaced it. A faster stream of shells from 32-round loads, but each shell is small, with little blast against a hull, and reaches less far.\n"
  "mul.price=0.7\nmul.roundsPerMinute=1.3\nmul.shotCooldown=0.77\nset.MagSize=32\nset.MaxAmmo=64\nset.ammoPerFill=8\nset.reloadCooldown=1.5\nset.Accuracy=90\nset.engagementDistance=1260\n"
  "proj.mul.BaseDamage=0.5\nproj.mul.SplashDamageBase=0.4\nproj.mul.MuzzleVelocity=0.92\nproj.mul.MuzzleVelocityRealistic=0.92\n\n"
  "[item]\nname=QF 2-pdr Single\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Bofors\nmodel=qf2pdr_single\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=British QF 2-pounder on the single power-operated Mk XVI mount, widely used on Coastal Forces craft such as the early Fairmile D boats. A long 56-round belt keeps it firing, but it fires slower than the Bofors, and its slower, lighter shells hit softer and reach less far.\n"
  "mul.price=0.9\nmul.roundsPerMinute=0.75\nmul.shotCooldown=1.33\nset.MagSize=56\nset.MaxAmmo=40\nset.ammoPerFill=8\nset.reloadCooldown=4\nset.Accuracy=93\nset.engagementDistance=1400\n"
  "proj.mul.BaseDamage=0.85\nproj.mul.SplashDamageBase=0.85\nproj.mul.MuzzleVelocity=0.75\nproj.mul.MuzzleVelocityRealistic=0.75\nproj.mul.FrameLifeTime=1.15\n\n"
  "[item]\nname=3.7 cm SK C/30\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Bofors\nmodel=skc30_37mm\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=German 3.7 cm SK C/30, the Kriegsmarine's hand-loaded medium AA gun, on the single mount carried by S-boats. It fires far slower than the Bofors, but it is cheap and its fast shells fly flat, land true and reach further.\n"
  "mul.price=0.6\nmul.roundsPerMinute=0.35\nmul.shotCooldown=2.86\nset.MagSize=2\nset.MaxAmmo=24\nset.ammoPerFill=6\nset.reloadCooldown=0.5\nset.Accuracy=99\nset.engagementDistance=2070\n"
  "proj.mul.SplashDamageBase=0.8\nproj.mul.MuzzleVelocity=1.13\nproj.mul.MuzzleVelocityRealistic=1.13\n\n"
  "[item]\nname=Breda 37/54 Twin\nfamily=Heavy Weapon,Heavy+ Weapon\nbase=40 mm Dual Bofors\nmodel=breda_37_twin\nmodel_keep=Mount,Stand,Bofors Hand Crank\n"
  "description=Italian Breda 37/54 Model 1938 twin, the Regia Marina's standard medium AA gun on battleships and cruisers, fed with 6-round clips one after another. It fires slower than the Dual Bofors and hits a little softer, but it is steadier, quick to reload and cheaper.\n"
  "mul.price=0.8\nmul.roundsPerMinute=0.6\nmul.shotCooldown=1.67\nset.reloadCooldown=0.5\nset.Accuracy=97\nset.engagementDistance=1700\n"
  "proj.mul.BaseDamage=0.9\nproj.mul.SplashDamageBase=0.9\nproj.mul.MuzzleVelocity=0.91\nproj.mul.MuzzleVelocityRealistic=0.91\n\n"
  "[item]\nname=10.5 cm SK C/32\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nmodel=skc32_105mm\nmodel_keep=Stand,5in_wet_mount,5 inch 25 Hand Crank\n"
  "description=German 10.5 cm SK C/32, the deck gun of torpedo boats, F-boats and minesweepers, and of Type I, IX and XB U-boats until 1942-43. Far cheaper than the 5-inch, with a faster, flatter shell, but the lighter shell hits softer with a smaller blast.\n"
  "mul.price=0.55\n"
  "proj.mul.BaseDamage=0.7\nproj.mul.SplashDamageBase=0.75\nproj.mul.SplashDamageRange=0.9\nproj.mul.MuzzleVelocity=1.15\nproj.mul.MuzzleVelocityRealistic=1.15\n\n"
  // batch 2 (2026-11-15): Medium
  "[item]\nname=MG 151/20 Drilling\nfamily=Medium Weapon\nbase=20 mm Oerlikon Twin\nmodel=mg151_drilling\nmodel_keep=Mount,Stand,ArmRest\nsound_from=20 mm Cannon AIR (JAP)\n"
  "description=German MG 151/20 Drilling: three surplus Luftwaffe cannon on one naval pedestal, from 1944. Fires much faster than the twin Oerlikon with a bigger burst from each shell, but it scatters, reaches less far and empties its belts fast, then reloads slowly.\n"
  "mul.price=1.2\nmul.roundsPerMinute=1.4\nmul.shotCooldown=0.714\nset.MagSize=48\nset.reloadCooldown=9\nset.Accuracy=81\nset.engagementDistance=900\nset.burstNumberOfShots=12\n"
  "proj.mul.BaseDamage=0.95\nproj.mul.SplashDamageBase=1.4\nproj.mul.MuzzleVelocity=0.9\nproj.mul.MuzzleVelocityRealistic=0.9\nproj.mul.FrameLifeTime=0.83\n\n"
  // batch 2 (2026-11-15): Utility
  "[item]\nname=Holman Projector\nfamily=Utility Weapon\nbase=M2 60 mm Mortar\nmodel=holman_projector\n"
  "description=British Holman Projector, a cheap compressed-air or steam thrower of Mills bombs carried by trawlers, merchant ships and Coastal Forces gunboats from 1940. It reloads twice as fast as the 60 mm and costs far less, but each bomb is small: a weak blast over a short reach.\n"
  "mul.price=0.3\nset.reloadCooldown=0.75\nset.MaxAmmo=24\nset.ammoPerFill=8\nset.Accuracy=80\nset.engagementDistance=500\n"
  "proj.mul.BaseDamage=0.3\nproj.mul.SplashDamageBase=0.4\nproj.mul.SplashDamageRange=0.3\n\n"
  "[item]\nname=7.5 cm Pak 40\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nmodel=pak40\nmodel_keep=Stand,pack_howitzer_75mm_carriage,Crank\nsound_from=Generic Tank Gun\n"
  "description=German 7.5 cm Pak 40, the standard heavy anti-tank gun from 1942, also carried by the Marder tank destroyers. Its fast, flat shell hits hard where it strikes, but bursts small instead of spreading the howitzer's wide blast, and it reloads slower.\n"
  "mul.price=1.35\nset.reloadCooldown=3.5\n"
  "proj.set.BaseDamage=3000\nproj.mul.SplashDamageBase=0.3\nproj.mul.SplashDamageRange=0.25\nproj.mul.MuzzleVelocity=2.1\nproj.mul.MuzzleVelocityRealistic=2.1\n\n"
  "[item]\nname=45 mm 21-K\nfamily=Utility Weapon\nbase=M3 37 mm AT\nmodel=k21_45mm\nmodel_keep=Stand,Crank\n"
  "description=Soviet 45 mm 21-K, the navy's standard light deck gun from 1934, two to each MO-4 sub chaser. Its bigger shell bursts much wider than the 37 mm's, but it strikes a little softer, flies a little slower and reloads slower.\n"
  "mul.price=1.1\nset.reloadCooldown=3.6\nset.Accuracy=95\n"
  "proj.mul.BaseDamage=0.9\nproj.mul.SplashDamageBase=1.5\nproj.mul.SplashDamageRange=1.6\nproj.mul.MuzzleVelocity=0.95\nproj.mul.MuzzleVelocityRealistic=0.96\n\n"
  "[item]\nname=Type 1 47 mm AT\nfamily=Utility Weapon\nbase=M3 37 mm AT\nmodel=type1_47mm\nmodel_keep=Stand,Crank\nsound_from=Soukou Tei Tank Gun\n"
  "description=Captured Japanese Type 1 47 mm anti-tank gun, built after Nomonhan showed the 37 mm too weak, and Japan's main gun against the M4 tank. A faster shell that strikes harder than the 37 mm's, but with a smaller burst and a slower reload.\n"
  "mul.price=1.15\nset.reloadCooldown=3.5\n"
  "proj.mul.BaseDamage=1.35\nproj.mul.SplashDamageBase=0.85\nproj.mul.MuzzleVelocity=1.05\nproj.mul.MuzzleVelocityRealistic=1.04\n\n"
  "[item]\nname=Blacker Bombard\nfamily=Utility Weapon\nbase=M3 37 mm AT\nmodel=blacker_bombard\nmodel_keep=Stand,Crank\nsound_from=90 mm Jap Mortar\n"
  "description=British Blacker Bombard, the Home Guard's spigot mortar that led to the Hedgehog. Its 20 lb bomb hits far harder than a 37 mm shell with a bigger blast, but it flies slowly in a high arc, cannot reach far, scatters more and is slow to reload.\n"
  "mul.price=0.7\nset.reloadCooldown=5.4\nset.Accuracy=80\n"
  "proj.mul.BaseDamage=1.8\nproj.mul.SplashDamageBase=1.5\nproj.mul.SplashDamageRange=1.5\nproj.mul.MuzzleVelocity=0.07\nproj.mul.MuzzleVelocityRealistic=0.1\nproj.mul.FrameLifeTime=4\nproj.mul.projectileWidth=2\n\n"
  "[item]\nname=380mm RW 61\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nmodel=sturmtiger_38cm\nmodel_keep=Stand,pack_howitzer_75mm_carriage,Crank\nsound_from=US Superheavy Artillery\n"
  "description=The 38 cm RW 61 rocket mortar of the German Sturmtiger, firing a huge rocket-assisted shell: it crawls out of the barrel, then its rocket fires and it speeds away. One enormous shell with a colossal blast, then a very long reload; ruinously expensive, only two carried, ten ammo crates make one round, and do not fire it at anything close.\n"
  "mul.price=3\nset.MagSize=1\nset.MaxAmmo=1\nset.ammoPerFill=1\nset.reloadCooldown=60\nset.Accuracy=88\ncrates_per_round=10\n"
  "proj.set.BaseDamage=6000\nproj.mul.SplashDamageBase=4\nproj.mul.SplashDamageRange=1.5\nproj.set.slowStart=1\nproj.set.slowStartFactor=0.2\nproj.set.slowStartRecoveryTime=1.2\nproj.mul.projectileWidth=5\n\n"   // rocket assist: Projectile.shoot (0x1809F2D47) starts any slowStart shell at slowStartFactor and iterate eases it to full speed (~2 s here); the trajectory is unchanged, only its first seconds run slow
  // batch 2 (2026-11-15): Rockets
  "[item]\nname=16-M-8 Katyusha\nfamily=Rockets\nbase=X50 Rocket Launcher\nmodel=katyusha_m8\nmodel_keep=Mount\n"
  "description=Soviet 16-rail M-8 Katyusha launcher, as fitted to river armoured boats and torpedo boats to fire on beaches ahead of landings. Sixteen 82 mm rockets to the X50's four that reach much farther, but each hits far softer and they scatter wide.\n"
  "mul.price=1.2\nset.MagSize=16\nset.MaxAmmo=32\nset.ammoPerFill=8\nset.reloadCooldown=3\nset.Accuracy=64\n"
  "proj.mul.BaseDamage=0.3\nproj.mul.SplashDamageBase=0.45\nproj.mul.SplashDamageRange=0.75\nproj.mul.MuzzleVelocity=1.15\nproj.mul.MuzzleVelocityRealistic=1.15\nproj.mul.projectileWidth=0.75\n\n"
  "[item]\nname=M-13 Katyusha\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nmodel=katyusha_m13\nmodel_keep=Mount\n"
  "description=Soviet M-13 132 mm Katyusha rails, which replaced the after turret on a few river armoured boats. Heavier rockets that fly much farther than the Mk 50's, but only six to a load, slower to reload and they scatter wide.\n"
  "mul.price=1.3\nset.MagSize=6\nset.MaxAmmo=12\nset.ammoPerFill=3\nset.reloadCooldown=6\nset.Accuracy=60\n"
  "proj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.4\nproj.mul.SplashDamageRange=1.2\nproj.mul.MuzzleVelocity=1.25\nproj.mul.MuzzleVelocityRealistic=1.25\nproj.mul.projectileWidth=1.15\n\n"
  "[item]\nname=4.5-inch Beach Barrage Rocket\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nmodel=bbr_mk7\nmodel_keep=Mount\n"
  "description=US 4.5-inch Beach Barrage Rocket on Mk 7 racks, fired in ripples by LCS(L)(3) support craft ahead of landings. Twelve cheap rockets with a wide blast, but each hits softer than the Mk 50's, they scatter and they fall far short.\n"
  "mul.price=0.75\nset.MagSize=12\nset.MaxAmmo=24\nset.ammoPerFill=6\nset.reloadCooldown=7\nset.Accuracy=64\n"
  "proj.mul.BaseDamage=0.6\nproj.mul.SplashDamageBase=0.9\nproj.mul.SplashDamageRange=1.25\nproj.mul.MuzzleVelocity=0.6\nproj.mul.MuzzleVelocityRealistic=0.6\n\n"
  "[item]\nname=20 cm Naval Rocket\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nmodel=naval_rocket_20cm\nmodel_keep=Mount\n"
  "description=Japanese 20 cm naval rocket, a 90 kg spin-stabilised round fired from a trough, used ashore at Peleliu and Iwo Jima. One cheap rocket at a time with a far bigger hit and blast than the Mk 50's, but it wobbles wide and falls short.\n"
  "mul.price=0.6\nset.MagSize=1\nset.MaxAmmo=6\nset.ammoPerFill=1\nset.reloadCooldown=4\nset.Accuracy=50\n"
  "proj.mul.BaseDamage=4\nproj.mul.SplashDamageBase=2.5\nproj.mul.SplashDamageRange=1.5\nproj.mul.MuzzleVelocity=0.75\nproj.mul.MuzzleVelocityRealistic=0.75\nproj.mul.projectileWidth=1.8\n\n"
  "[item]\nname=Hedgerow\nfamily=Rockets\nbase=Mousetrap Mk20\nmodel=hedgerow\nsound_from=90 mm Jap Mortar\nprojectile_from=150 mm Jap Mortar\n"
  "description=Hedgerow: Hedgehog spigot bombs on landing craft at Normandy, fired to blast paths through the beach defences. Twenty-four bombs in one salvo to the Mousetrap's four, but each bursts softer, they land in a wide ring, the spigots throw them short and it costs more.\n"
  "mul.price=1.5\nset.MagSize=24\nset.MaxAmmo=24\nset.ammoPerFill=6\nset.Accuracy=55\n"
  // projectile_from: Hedgehog-like bombs from the 150 mm Jap Mortar (26 kg shell, the nearest stock round to a 29 kg
  // Hedgehog bomb; 4000 damage, 400 splash over 25 m, 145 m/s), set to the Mousetrap rocket x0.3 damage and splash, x0.8
  // blast radius and speed as before (15, 18 over 36 m, 48 m/s), timeScale 1 like the rocket; still hurts submarines.
  // fragileTransmitter and assistFactor as on the Mousetrap rocket (the shell has False and 0): Projectile.explode
  // (0x1809F1EF6) hits FragileReciever parts within fragileRange (150 m on both) only when ptype+0x68 is set
  "proj.set.BaseDamage=15\nproj.set.SplashDamageBase=18\nproj.set.SplashDamageRange=36\nproj.set.MuzzleVelocity=48\nproj.set.MuzzleVelocityRealistic=48\n"
  "proj.set.timeScale=1\nproj.set.ignoreUnderwaterDamage=0\nproj.set.fragileTransmitter=1\nproj.set.assistFactor=1\n\n"
  // batch 2 (2026-11-15): Crew weapons
  "[item]\nname=M2 Stinger\nfamily=Crew Weapons\nbase=M1919 Browning Handheld\nmodel=stinger_anm2\nsound_from=30-06 Cal Single MG AIR (USA)\n"
  "description=The Stinger: an AN/M2 .30 aircraft gun from a wrecked plane, made hand-held by Marines of the 28th Regiment and carried by Cpl. Tony Stein on Iwo Jima. Twice the M1919's rate from a 100-round belt, but it sprays wide and is slow to reload.\n"
  "mul.price=1.3\nmul.roundsPerMinute=2\nmul.shotCooldown=0.5\nset.MagSize=100\nset.reloadCooldown=16\nset.Accuracy=72\nset.burstNumberOfShots=12\n\n"
  "[item]\nname=M1 Garand\nfamily=Crew Weapons\nbase=M1918 BAR\nmodel=m1_garand\n"
  "description=US M1 Garand rifle with its 8-round clip, the standard US rifle from 1936. Cheap and very accurate, with aimed single shots that hit a little harder, but it fires far fewer of them than the BAR.\n"
  "mul.price=0.6\nmul.roundsPerMinute=0.3\nmul.shotCooldown=3.33\nset.MagSize=8\nset.reloadCooldown=2.5\nset.Accuracy=99\nset.burstNumberOfShots=0\nset.burstCooldown=0\n"
  "proj.mul.BaseDamage=1.2\nproj.mul.SplashDamageBase=1.2\n\n"
  "[item]\nname=Bren Mk I\nfamily=Crew Weapons\nbase=M1918 BAR\nmodel=bren_mk1\n"
  "description=British Bren Mk I light machine gun, carried on Coastal Forces boats and landing craft. Fires slower than the BAR but is the steadiest of the light machine guns, in longer bursts from 28-round magazines.\n"
  "mul.price=1.0\nmul.roundsPerMinute=0.8\nmul.shotCooldown=1.25\nset.MagSize=28\nset.reloadCooldown=4.5\nset.Accuracy=99\nset.burstNumberOfShots=6\n\n"
  "[item]\nname=PIAT\nfamily=Crew Weapons\nbase=Bazooka\nmodel=piat_mk1\n"
  "description=British PIAT, a spring-and-spigot launcher with no backblast that re-cocks on its own recoil. Cheaper than the Bazooka and each bomb hits a little harder, but its slower bomb flies less true and bursts a little smaller.\n"
  "mul.price=0.9\nset.reloadCooldown=6\nset.Accuracy=85\n"
  "proj.mul.BaseDamage=1.05\nproj.mul.SplashDamageBase=0.9\nproj.mul.MuzzleVelocity=0.8\nproj.mul.MuzzleVelocityRealistic=0.8\n\n"
  "[item]\nname=Type 89 Knee Mortar\nfamily=Crew Weapons\nbase=M7 Grenade Launcher\nmodel=type89_discharger\nsound_from=M2 60 mm Mortar\n"
  "description=Captured Japanese Type 89 50 mm grenade discharger, the 'knee mortar': a mistranslated name, as firing it off the thigh broke bones. Fast but light: it reloads faster than the rifle grenade, but its burst is smaller and it scatters far wider.\n"
  "mul.price=1.1\nset.reloadCooldown=6\nset.Accuracy=55\n"
  "proj.mul.SplashDamageBase=0.8\n";
// ---- utility gear for the depth charge launchers ----------------------------------------------------------------------
// Smoke pots and life rafts, always in the depth charge slots' lists like stock gear. Each is a copy of a stock depth
// charge tier (so it rolls off the side or the stern rack the same way); when it lands on the water the mod takes the
// charge away before it sinks and leaves a smoke cloud or a raft there (UtilLanded). Names carry the count, because an
// item has one base and the slots hold 1, 2 or 4. Prices are per pot or raft (a stock depth charge is 250 RP). The PT-6's
// own Depth Charge Slot (its rear depth launchers FL/FR) holds 2, 3 or 4 (David 2026-11-09: it listed none of these).
#define UTIL_SPLIT(name, fams, bases) "[item]\nname=" name "\nfamily=" fams "\nbase=" bases "\nprice=250\nfx.set.smoke=1\nfx.set.raft=1\nmul.damage=0\n" \
  "description=Both on one boat: the left launcher drops life rafts, the right one smoke pots. A raft heads for your nearest crew member in the water and picks them up (safe, back after the battle); a smoke pot burns where it lands and most enemy ships cannot spot you through its smoke.\n\n"
static const char* kUtilityGear =
  "[item]\nname=Smoke Pot\nfamily=Secondary Slot\nbase=Mk 6 Depth Charge x1\nprice=200\nfx.set.smoke=1\nmul.damage=0\n"
  "description=A floating smoke pot. It burns where it lands, puffing the smoke artillery's cloud every 12 s (for 150 s by default, smoke_seconds in drops.ini) into a screen about 150 m across; most enemy ships cannot spot you through it.\n\n"
  "[item]\nname=Smoke Pots x2\nfamily=Sub Chaser Side Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge K-Gun Carried x2,Mk 6 Depth Charge x2\nprice=200\nfx.set.smoke=1\nmul.damage=0\n"
  "description=Floating smoke pots. Each burns where it lands, puffing the smoke artillery's cloud every 12 s (for 150 s by default, smoke_seconds in drops.ini) into a screen about 150 m across; most enemy ships cannot spot you through it.\n\n"
  "[item]\nname=Smoke Pots x4\nfamily=Sub Chaser Depth Charge Rack Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge x4,Mk 6 Depth Charge x4\nprice=200\nfx.set.smoke=1\nmul.damage=0\n"
  "description=Floating smoke pots. Each burns where it lands, puffing the smoke artillery's cloud every 12 s (for 150 s by default, smoke_seconds in drops.ini) into a screen about 150 m across; most enemy ships cannot spot you through it.\n\n"
  "[item]\nname=Smoke Pots x3\nfamily=Depth Charge Slot\nbase=Mk 6 Depth Charge x3\nprice=200\nfx.set.smoke=1\nmul.damage=0\n"
  "description=Floating smoke pots. Each burns where it lands, puffing the smoke artillery's cloud every 12 s (for 150 s by default, smoke_seconds in drops.ini) into a screen about 150 m across; most enemy ships cannot spot you through it.\n\n"
  "[item]\nname=Life Raft\nfamily=Secondary Slot\nbase=Mk 6 Depth Charge x1\nprice=300\nfx.set.raft=1\nmul.damage=0\n"
  "description=A life raft. It heads for your nearest crew member in the water and picks them up: they are safe, back with your crew after the battle, but out of this one.\n\n"
  "[item]\nname=Life Rafts x2\nfamily=Sub Chaser Side Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge K-Gun Carried x2,Mk 6 Depth Charge x2\nprice=300\nfx.set.raft=1\nmul.damage=0\n"
  "description=Life rafts. Each heads for your nearest crew member in the water and picks them up: they are safe, back with your crew after the battle, but out of this one.\n\n"
  "[item]\nname=Life Rafts x4\nfamily=Sub Chaser Depth Charge Rack Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge x4,Mk 6 Depth Charge x4\nprice=300\nfx.set.raft=1\nmul.damage=0\n"
  "description=Life rafts. Each heads for your nearest crew member in the water and picks them up: they are safe, back with your crew after the battle, but out of this one.\n\n"
  "[item]\nname=Life Rafts x3\nfamily=Depth Charge Slot\nbase=Mk 6 Depth Charge x3\nprice=300\nfx.set.raft=1\nmul.damage=0\n"
  "description=Life rafts. Each heads for your nearest crew member in the water and picks them up: they are safe, back with your crew after the battle, but out of this one.\n\n"
  // both on one boat (David 2026-11-08): the left and right launchers have their own fire buttons, so a load is split by
  // side, not mixed in one rack: the LEFT launcher drops life rafts, the RIGHT one smoke pots, the count on each side
  // (UtilKind). The same name sits in several slot families, each with its own base (SplitFamilies pairs them).
  UTIL_SPLIT("Life Raft L + Smoke Pot R", "Secondary Slot", "Mk 6 Depth Charge x1")
  UTIL_SPLIT("Life Rafts L + Smoke Pots R x2", "Sub Chaser Side Slot,New Standard Torpedo Slot,Depth Charge Slot", "Mk 6 Depth Charge K-Gun Carried x2,Mk 6 Depth Charge x2,Mk 6 Depth Charge x2")
  UTIL_SPLIT("Life Rafts L + Smoke Pots R x3", "New Standard Torpedo Slot,Depth Charge Slot", "Mk 6 Depth Charge x3,Mk 6 Depth Charge x3")
  UTIL_SPLIT("Life Rafts L + Smoke Pots R x4", "Sub Chaser Side Slot,Sub Chaser Depth Charge Rack Slot,New Standard Torpedo Slot,Depth Charge Slot",
             "Mk 6 Depth Charge K-Gun Carried x4,Mk 6 Depth Charge x4,Mk 6 Depth Charge x4,Mk 6 Depth Charge x4")
  UTIL_SPLIT("Life Rafts L + Smoke Pots R x6", "Sub Chaser Depth Charge Rack Slot", "Mk 6 Depth Charge x6")
  UTIL_SPLIT("Life Rafts L + Smoke Pots R x8", "Sub Chaser Depth Charge Rack Slot", "Mk 6 Depth Charge x8")
  // Dave (David 2026-11-09, a joke for the lulz): the mod's dev himself, a stiff T-posed placeholder model that glides
  // over the water to the RP crates and picks them up for you (models/dave.obj, tools/make_utility_models.py)
  "[item]\nname=Dave\nfamily=Secondary Slot\nbase=Mk 6 Depth Charge x1\nprice=0\nfx.set.dave=1\nmul.damage=0\n"
  "description=Dave, the mod's developer, as an unfinished placeholder: no animation, stuck in a T-pose. He glides over the water to the nearest RP crates and picks them up for you, for 2 minutes.\n\n"
  "[item]\nname=Daves x2\nfamily=Sub Chaser Side Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge K-Gun Carried x2,Mk 6 Depth Charge x2\nprice=0\nfx.set.dave=1\nmul.damage=0\n"
  "description=More Daves, the mod's developer, as unfinished placeholders: no animation, stuck in a T-pose. Each glides over the water to the nearest RP crates and picks them up for you, for 2 minutes.\n\n"
  "[item]\nname=Daves x4\nfamily=Sub Chaser Depth Charge Rack Slot,Depth Charge Slot\nbase=Mk 6 Depth Charge x4,Mk 6 Depth Charge x4\nprice=0\nfx.set.dave=1\nmul.damage=0\n"
  "description=More Daves, the mod's developer, as unfinished placeholders: no animation, stuck in a T-pose. Each glides over the water to the nearest RP crates and picks them up for you, for 2 minutes.\n\n";
static double g_smokeSeconds = 150;                                   // drops.ini smoke_seconds: how long a pot keeps puffing
static double g_raftSpeed = 8;                                      // drops.ini raft_speed: metres a second on the water
static bool g_sideGrades = true;                                    // drops.ini side_grades: 0 hides them (positions kept)
static bool IsTurretGun(const Item& it) { return it.builtIn && it.family == "Tub Turret"; }   // else a side-grade or utility gear
static bool IsUtilityGear(const Item& it) { return it.builtIn && it.family.find("Slot") != std::string::npos; }   // torpedo families end in "Slot"

// One Item per slot family: a built-in with family=A,B becomes a copy in A and one in B, under the same name. Only
// FindItemIn tells them apart; every other lookup by name may take either, as the copies match in all but the family
// (and, for utility gear with base=A,B, the base: co-op sends one copy per name, so EnsureRemoteTorps takes the base here).
static void SplitFamilies(size_t from) {
  for (size_t i = from; i < g_items.size(); i++) {
    std::vector<std::string> fams = SplitList(g_items[i].family);
    if (fams.size() < 2) continue;
    std::vector<std::string> bases = SplitList(g_items[i].base);      // base=A,B: one per family, in order (utility gear)
    bool paired = bases.size() == fams.size();
    g_items[i].family = fams[0]; if (paired) g_items[i].base = bases[0];
    Item copy = g_items[i];
    for (size_t f = 1; f < fams.size(); f++) { copy.family = fams[f]; if (paired) copy.base = bases[f]; g_items.insert(g_items.begin() + ++i, copy); }
  }
}

static void LoadItems(const char* folder) {
  int s = ParseItemsText(kTurretGuns, "turret guns", false);          // first, so a weapons.ini item of the same name is the one ignored
  int sg = ParseItemsText(kSideGrades, "side-grades", false);
  int ug = ParseItemsText(kUtilityGear, "utility gear", false);
  for (size_t i = g_items.size() - s - sg - ug; i < g_items.size(); i++) g_items[i].builtIn = true;
  SplitFamilies(g_items.size() - sg - ug);
  char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\weapons.ini", folder);
  FILE* f = fopen(path, "r");
  if (!f) {
    f = fopen(path, "w");
    if (f) { fputs(kDefaultIni, f); fclose(f); }
    Log("weapons.ini did not exist; wrote the default one at %s", path);
  } else fclose(f);
  int a = ParseItemsFile(path, "weapons.ini", false);
  snprintf(path, sizeof path, "%s\\inventory.ini", folder);
  int b = ParseItemsFile(path, "inventory.ini", true);                     // guns found in play, written by the mod; missing is fine
  Log("items: %d turret guns and %d side-grades of the mod's own, %d from weapons.ini, %d from inventory.ini", s, sg, a < 0 ? 0 : a, b < 0 ? 0 : b);
  for (size_t i = 0; i < g_items.size(); i++)
    DLog("  item %d: '%s' family '%s' base '%s'%s, %d edits", (int)i, g_items[i].name.c_str(), g_items[i].family.c_str(), g_items[i].base.c_str(), g_items[i].retired ? " (retired)" : "", (int)g_items[i].ops.size());
}

