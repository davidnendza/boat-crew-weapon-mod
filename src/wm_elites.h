// Rare and Elite enemies (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Rare and Elite enemies ------------------------------------------------------------------------------------------
// Diablo style: some enemy ships spawn as Rare (one trait) or Elite (more HP and 2-3 traits). Traits are read from
// enemies.ini: HP, speed, damage of their shots, size (scale_x = width, scale_y = height, scale_z = length), bonus RP.
// Killing one gives a guaranteed gun (Rare: Uncommon or better, Elite: Rare).
// HealthPart.forceStart(m) sets multiplierMAX = m and HP = MAXHp x m x progression x upgrade (the game's own spawn
// path: CombatEntity.reset calls it for every hull part, so a pooled ship comes back with normal HP on its own).
// Ship.maxSpeedProgressionMultiplier (+0x2D0) is read by getMaxSpeed and only ever set to 1 by the constructor, and a
// transform's scale is never reset either: both are restored by the mod (when the ship spawns again, or at battle end).
// New spawns are found by walking HealthPart.register every 20 frames: a ship seen alive that was not alive before.
struct EnemyTrait { std::string name; double weight = 1, hp = 1, speed = 1, damage = 1, sx = 1, sy = 1, sz = 1, blastDamage = 0, blastRadius = 0; int rp = 0; bool eliteOnly = false, armed = false; };
static std::vector<EnemyTrait> g_traits;
static int g_eliteTraitsLo = 2, g_eliteTraitsHi = 3, g_rareDropMin = 2, g_eliteDropMin = 3, g_rareRp = 0, g_eliteRp = 0;

static const char* kDefaultEnemies =
  "# BoatCrewWeaponMod: Rare and Elite enemy ships.\n"
  "#\n"
  "# Each enemy ship that spawns may become Rare (blue, one trait) or Elite (gold, elite_hp times the HP and 2-3 traits).\n"
  "# Killing one drops a gun into the loot grid: rare_drop / elite_drop is the least number of effects (2 = Uncommon or\n"
  "# better, 3 = Rare). rare_rp / elite_rp pay extra RP on the kill. rare_callin / elite_callin multiply the call-in\n"
  "# progress the kill gives (2 = twice a normal kill of that ship, 1 = no bonus). tags=1 shows a name tag over them in battle (up to\n"
  "# tag_range metres away), announce=1 puts a line in the kill feed when one appears. enabled=0 turns all of it off.\n"
  "#\n"
  "# [trait] blocks: name (shown in front of the ship's name), weight (how often it is picked), hp, speed, damage (of the\n"
  "# ship's shots), scale_x (width), scale_y (height), scale_z (length), rp (extra RP on the kill), elite_only=1,\n"
  "# blast_damage and blast_radius (metres): the ship explodes when it sinks and hurts your side within that range.\n"
  "# armed=1: the ship carries the gun it will drop, and its own guns fire with that gun's effects.\n"
  "# Two traits that scale the same direction never go on one ship. Sizes are an experiment: if a ship floats or handles\n"
  "# strangely, set that trait's weight to 0.\n"
  "\n"
  "[enemies]\n"
  "enabled=1\n"
  "rare_chance=0.08\n"
  "elite_chance=0.02\n"
  "elite_hp=1.75\n"
  "elite_traits=2-3\n"
  "rare_drop=2\n"
  "elite_drop=3\n"
  "rare_rp=0\n"
  "elite_rp=0\n"
  "rare_callin=2\n"
  "elite_callin=3\n"
  "tags=1\n"
  "tag_range=3000\n"
  "announce=1\n"
  "\n"
  "[trait]\nname=Armored\nhp=2.0\n\n"
  "[trait]\nname=Swift\nspeed=1.35\n\n"
  "[trait]\nname=Hard-Hitting\ndamage=1.5\n\n"
  "[trait]\nname=Loaded\nrp=100\n\n"
  "[trait]\nname=Volatile\nblast_damage=400\nblast_radius=40\n\n"
  "[trait]\nname=Armed\narmed=1\nelite_only=1\n\n"
  "[trait]\nname=Ironclad\nhp=3.0\nspeed=0.85\nelite_only=1\n\n"
  "[trait]\nname=Long Boi\nscale_z=2.0\nhp=1.3\n\n"
  "[trait]\nname=Short King\nscale_z=0.5\nspeed=1.2\n\n"
  "[trait]\nname=Thicc\nscale_x=2.0\nhp=1.3\nspeed=0.9\n\n"
  "[trait]\nname=Noodle\nscale_x=0.45\nspeed=1.15\n\n"
  "[trait]\nname=Chonker\nscale_x=1.6\nscale_y=1.6\nscale_z=1.6\nhp=1.5\nweight=0.6\n\n"
  "[trait]\nname=Smol\nscale_x=0.55\nscale_y=0.55\nscale_z=0.55\nspeed=1.25\nweight=0.6\n";

// 0.9.0: bigger default sizes, visible at fighting range. An enemies.ini that still has the old default sizes gets the
// new ones; a size someone edited by hand is left alone.
static const char* kOldTraitSizes[][2] = {
  { "name=Long Boi\nscale_z=1.4\n", "name=Long Boi\nscale_z=2.0\n" },
  { "name=Short King\nscale_z=0.7\n", "name=Short King\nscale_z=0.5\n" },
  { "name=Thicc\nscale_x=1.45\n", "name=Thicc\nscale_x=2.0\n" },
  { "name=Noodle\nscale_x=0.65\n", "name=Noodle\nscale_x=0.45\n" },
  { "name=Chonker\nscale_x=1.25\nscale_y=1.25\nscale_z=1.25\n", "name=Chonker\nscale_x=1.6\nscale_y=1.6\nscale_z=1.6\n" },
  { "name=Smol\nscale_x=0.75\nscale_y=0.75\nscale_z=0.75\n", "name=Smol\nscale_x=0.55\nscale_y=0.55\nscale_z=0.55\n" } };

// A newer version's built-in traits are appended to an existing enemies.ini (by name), like MigrateDrops does for effects.
static void MigrateEnemies(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return;
  std::set<std::string> have; char line[512]; std::string text;
  while (fgets(line, sizeof line, f)) { text += line; std::string l = Trim(line); if (l.compare(0, 5, "name=") == 0) have.insert(Trim(l.substr(5))); }
  fclose(f);
  std::string resized;
  for (auto& s : kOldTraitSizes) {
    size_t at = text.find(s[0]);
    if (at == std::string::npos) continue;
    text.replace(at, strlen(s[0]), s[1]);
    resized += (resized.empty() ? "" : ", ") + Trim(std::string(s[0]).substr(5, std::string(s[0]).find('\n') - 5));
  }
  if (!resized.empty()) {
    FILE* w = OpenForSave(path);
    if (w) { fputs(text.c_str(), w); FinishSave(w, path); Log("enemies.ini: bigger size changes for %s", resized.c_str()); }
  }
  std::string def = kDefaultEnemies, add, names;
  for (size_t at = def.find("[trait]"); at != std::string::npos; ) {
    size_t next = def.find("[trait]", at + 7);
    std::string block = def.substr(at, next == std::string::npos ? std::string::npos : next - at);
    size_t n = block.find("name="), e = n == std::string::npos ? n : block.find('\n', n);
    std::string nm = n == std::string::npos ? "" : Trim(block.substr(n + 5, e - n - 5));
    if (!nm.empty() && !have.count(nm)) { add += "\n" + block; names += (names.empty() ? "" : ", ") + nm; }
    at = next;
  }
  if (add.empty()) return;
  FILE* w = fopen(path.c_str(), "a");
  if (!w) return;
  fputs(add.c_str(), w); fclose(w);
  Log("enemies.ini: added new traits %s", names.c_str());
}

static void LoadEnemies() {
  std::string path = g_folder + "\\enemies.ini";
  MigrateEnemies(path);
  FILE* f = fopen(path.c_str(), "r");
  if (!f) {
    f = fopen(path.c_str(), "w");
    if (f) { fputs(kDefaultEnemies, f); fclose(f); }
    Log("enemies.ini did not exist; wrote the default one");
    f = fopen(path.c_str(), "r");
    if (!f) { g_elitesOn = false; return; }
  }
  char line[512]; int sec = 0;
  while (fgets(line, sizeof line, f)) {
    std::string l = Trim(line);
    if (l.empty() || l[0] == '#' || l[0] == ';') continue;
    if (l[0] == '[') { sec = l == "[enemies]" ? 1 : (l == "[trait]" ? 2 : 0); if (sec == 2) g_traits.emplace_back(); continue; }
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
    double d = atof(v.c_str());
    if (sec == 1) {
      if (k == "enabled") g_elitesOn = d != 0; else if (k == "rare_chance") g_rareChance = d; else if (k == "elite_chance") g_eliteChance = d;
      else if (k == "elite_hp") g_eliteHp = d > 0.1 ? d : 1; else if (k == "rare_drop") g_rareDropMin = (int)d; else if (k == "elite_drop") g_eliteDropMin = (int)d;
      else if (k == "rare_rp") g_rareRp = (int)d; else if (k == "elite_rp") g_eliteRp = (int)d; else if (k == "tags") g_eliteTags = d != 0;
      else if (k == "rare_callin") g_rareCallin = d; else if (k == "elite_callin") g_eliteCallin = d;
      else if (k == "tag_range") g_tagRange = d; else if (k == "announce") g_eliteAnnounce = d != 0;
      else if (k == "elite_traits") { int lo = 2, hi = 3; if (sscanf(v.c_str(), "%d - %d", &lo, &hi) == 2 || sscanf(v.c_str(), "%d-%d", &lo, &hi) == 2) { g_eliteTraitsLo = lo; g_eliteTraitsHi = hi < lo ? lo : hi; } else g_eliteTraitsLo = g_eliteTraitsHi = (int)d; }
      else Log("enemies.ini: unknown key '%s'", k.c_str());
    } else if (sec == 2) {
      EnemyTrait& t = g_traits.back();
      if (k == "name") t.name = v; else if (k == "weight") t.weight = d; else if (k == "hp") t.hp = d; else if (k == "speed") t.speed = d;
      else if (k == "damage") t.damage = d; else if (k == "scale_x") t.sx = d; else if (k == "scale_y") t.sy = d; else if (k == "scale_z") t.sz = d;
      else if (k == "rp") t.rp = (int)d; else if (k == "elite_only") t.eliteOnly = d != 0;
      else if (k == "blast_damage") t.blastDamage = d; else if (k == "blast_radius") t.blastRadius = d; else if (k == "armed") t.armed = d != 0;
      else Log("enemies.ini: unknown trait key '%s'", k.c_str());
    }
  }
  fclose(f);
  for (size_t i = 0; i < g_traits.size(); ) {
    EnemyTrait& t = g_traits[i];
    auto clampS = [](double& s) { if (s < 0.3) s = 0.3; if (s > 2.5) s = 2.5; };   // a hull at 0 or 10x size would break the physics
    clampS(t.sx); clampS(t.sy); clampS(t.sz);
    if (t.name.empty()) g_traits.erase(g_traits.begin() + i); else i++;
  }
  if (g_eliteTraitsLo < 1) g_eliteTraitsLo = 1;
  Log("enemies.ini: %s, rare %.3f, elite %.3f (x%.2f HP, %d-%d traits), %d traits", g_elitesOn ? "on" : "off", g_rareChance, g_eliteChance, g_eliteHp,
      g_eliteTraitsLo, g_eliteTraitsHi, (int)g_traits.size());
}

struct EliteInfo { int tier; std::string name, ship, traits; float damage; int rp; float blastDamage, blastRadius; bool rewarded = false; bool hasLoot = false; Item loot; std::string lootRarity;
  int netId = -1; };   // tier 1 Rare, 2 Elite; netId: its network object in co-op (KILL messages name it)
struct EliteRestore { void* handle; V3 scale; float speed; bool scaled, sped; std::vector<std::pair<void*, void*>> guns; std::vector<void*> gunHandles; };   // guns: mount, its own GunType (both pinned)
static std::unordered_map<void*, EliteInfo> g_elites;                 // living Rare/Elite ships this battle
static std::unordered_map<void*, EliteRestore> g_eliteRestore;        // ships the mod resized or sped up (pinned)
static std::unordered_set<void*> g_seenAlive;                        // every ship seen alive this battle
static std::unordered_map<void*, std::pair<EliteInfo, DWORD>> g_eliteDead;   // sunk a moment ago: the kill credit may come after the scan
static void* g_eliteEng = nullptr;
static int g_eliteFrame = 0, g_eliteFaults = 0;
static int g_offHpReg = -2, g_offHpEntity = -1, g_offHpMult = -1, g_offLeTeam = -1, g_offCeHull = -1, g_offCeParams = -1;   // g_offLeAlive: see LiveEnemies
static int g_offEtTitle = -1, g_offShipSpeed = -1, g_offGmEntity = -1;
static void *g_hpRegField = nullptr, *m_hpForceStart = nullptr, *g_shipClass = nullptr, *m_trGetPos = nullptr, *m_camMain = nullptr, *m_camW2S = nullptr;

static bool EliteLookups() {
  if (g_offHpReg != -2) return g_offHpReg >= 0;
  g_offHpReg = -1;
  void* hp = FindClass("Core", "HealthPart"); void* le = FindClass("Core", "LiveEntity"); void* ce = FindClass("Core", "CombatEntity");
  void* et = FindClass("Core", "EntityType"); void* gm = FindClass("Core", "GunMount"); g_shipClass = FindClass("Core", "Ship");
  void* tr = FindClass("UnityEngine", "Transform"); void* cam = FindClass("UnityEngine", "Camera");
  g_hpRegField = hp ? il2cpp_class_get_field_from_name(hp, "register") : nullptr;
  m_hpForceStart = hp ? FindMethod(hp, "forceStart", 1) : nullptr;
  g_offHpEntity = FieldOffset(hp, "combatEntity"); g_offHpMult = FieldOffset(hp, "multiplierMAX");
  g_offLeAlive = FieldOffset(le, "_alive"); g_offLeTeam = FieldOffset(le, "teamCode");
  g_offCeHull = FieldOffset(ce, "hull"); g_offCeParams = FieldOffset(ce, "parameters"); g_offEtTitle = FieldOffset(et, "entityTitle");
  g_offShipSpeed = FieldOffset(g_shipClass, "maxSpeedProgressionMultiplier"); g_offGmEntity = FieldOffset(gm, "entity");
  m_trGetPos = tr ? FindMethod(tr, "get_position", 0) : nullptr;
  m_camMain = cam ? FindMethod(cam, "get_main", 0) : nullptr;
  m_camW2S = cam ? FindMethod(cam, "WorldToScreenPoint", 1) : nullptr;
  if (!g_hpRegField || !m_hpForceStart || !g_shipClass || g_offHpEntity < 0 || g_offHpMult < 0 || g_offLeAlive < 0 || g_offLeTeam < 0 || g_offCeHull < 0) {
    Log("elites: a lookup failed (register %p forceStart %p Ship %p entity %d mult %d alive %d team %d hull %d); Rare/Elite enemies are off",
        g_hpRegField, m_hpForceStart, g_shipClass, g_offHpEntity, g_offHpMult, g_offLeAlive, g_offLeTeam, g_offCeHull);
    return false;
  }
  if (!m_trGetPos || !m_camMain || !m_camW2S) Log("elites: Camera/Transform methods missing (pos %p main %p w2s %p); no name tags", m_trGetPos, m_camMain, m_camW2S);
  DLog("elites: HealthPart.combatEntity +0x%X multiplierMAX +0x%X, LiveEntity._alive +0x%X teamCode +0x%X, hull +0x%X, Ship speed +0x%X, GunMount.entity +0x%X",
       g_offHpEntity, g_offHpMult, g_offLeAlive, g_offLeTeam, g_offCeHull, g_offShipSpeed, g_offGmEntity);
  g_offHpReg = 0;
  return true;
}

static bool IsShip(void* obj) {
  for (void* k = obj ? il2cpp_object_get_class(obj) : nullptr; k; k = il2cpp_class_get_parent(k)) if (k == g_shipClass) return true;
  return false;
}
static std::string ShipTitle(void* ce) {
  void* et = g_offCeParams >= 0 ? *(void**)((char*)ce + g_offCeParams) : nullptr;
  void* t = et && g_offEtTitle >= 0 ? *(void**)((char*)et + g_offEtTitle) : nullptr;
  std::string s = t ? Str(t) : std::string();
  if (s.empty() || s == "Unnamed Entity") s = FamilyOf(ce);            // most ships leave the title at the default
  return s;
}
static void RestoreShip(void* ce, EliteRestore& r) {
  if (r.sped && g_offShipSpeed >= 0) *(float*)((char*)ce + g_offShipSpeed) = r.speed;
  if (r.scaled) { void* tr = Invoke(m_getTransform, ce); if (tr) { void* a[1] = { &r.scale }; Invoke(m_setLocalScale, tr, a); } }
  for (auto& g : r.guns) SetRef(g.first, (char*)g.first + g_offGunType, g.second);
  for (void* h : r.gunHandles) Unpin(h);
  Unpin(r.handle);
}
static void RestoreAllShips() {
  for (auto& e : g_eliteRestore) RestoreShip(e.first, e.second);
  if (!g_eliteRestore.empty()) DLog("elites: restored %d resized/sped-up ships", (int)g_eliteRestore.size());
  g_eliteRestore.clear();
}
// After an EliteScan fault nothing would ever put pooled ships back (size, speed, guns carry into later battles).
static void SafeRestoreAllShips() {
  __try { RestoreAllShips(); g_elites.clear(); }
  __except (EXCEPTION_EXECUTE_HANDLER) { Log("elites: ships could not be restored after the fault"); }
}

// Armed: the loot gun is rolled at spawn, and every gun mount of the ship gets a copy of its own GunType with the loot's
// plain number edits (mul. on the gun) applied. GunMount.shoot and FixedUpdateMe read gunType (+0x88) on every shot, the
// same thing found guns rely on. A damage edit on the loot's shells (proj.mul.BaseDamage) becomes the ship's shot damage.
// Other projectile, mount and fx edits do not carry over. The ship's own GunTypes are put back by RestoreShip.
static int ArmShip(void* ce, const Item& it, EliteRestore* r, float* damage) {
  for (const Op& op : it.ops) if (op.proj && op.mul && op.field == "BaseDamage") *damage *= (float)op.value;
  void* wm = g_offCraftWM >= 0 ? *(void**)((char*)ce + g_offCraftWM) : nullptr;
  void* arr = wm && g_offWMSystems >= 0 ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  std::map<void*, void*> copies;                                       // the ship's GunType -> its armed copy
  int n = 0;
  for (int i = 0; arr && i < ArrLen(arr); i++) {
    void* ws = ArrAt(arr, i); if (!ws) continue;
    void* mounts = *(void**)((char*)ws + g_offMounts);
    for (int j = 0; j < ListCount(mounts); j++) {
      void* m = ListAt(mounts, j); if (!m) continue;
      void* gt = *(void**)((char*)m + g_offGunType); if (!gt) continue;
      auto c = copies.find(gt);
      if (c == copies.end()) {
        void* cp = CloneObject(gt); if (!cp) continue;
        for (const Op& op : it.ops) if (!op.proj && !op.mount && !op.sphere && !op.fx && op.mul) ApplyOp(cp, op);
        c = copies.emplace(gt, cp).first;
      }
      r->guns.push_back({ m, gt }); r->gunHandles.push_back(Pin(m)); r->gunHandles.push_back(Pin(gt));
      SetRef(m, (char*)m + g_offGunType, c->second); n++;
    }
  }
  return n;
}

struct EliteRoll { double hp, speed, sx, sy, sz; };                 // what a roll changes on the ship itself
static void ApplyElite(void* ce, const EliteRoll& roll, EliteInfo info, bool fromHost, const std::string& shown);
static void CoopEliteMade(void* ce, const EliteRoll& roll, const EliteInfo& info, const std::string& shown);
static void MakeElite(void* ce, int tier) {
  // pick traits: weighted, no repeats, no two that scale the same direction
  std::vector<const EnemyTrait*> chosen;
  int want = tier == 2 ? g_eliteTraitsLo + (int)(Rand01() * (g_eliteTraitsHi - g_eliteTraitsLo + 1)) : 1;
  if (want > g_eliteTraitsHi && tier == 2) want = g_eliteTraitsHi;
  for (int pick = 0; pick < want; pick++) {
    double tw = 0;
    auto ok = [&](const EnemyTrait& t) {
      if (t.weight <= 0 || (t.eliteOnly && tier != 2)) return false;
      for (const EnemyTrait* c : chosen) {
        if (c == &t) return false;
        if ((c->sx != 1 && t.sx != 1) || (c->sy != 1 && t.sy != 1) || (c->sz != 1 && t.sz != 1)) return false;
      }
      return true;
    };
    for (auto& t : g_traits) if (ok(t)) tw += t.weight;
    if (tw <= 0) break;
    double x = Rand01() * tw; const EnemyTrait* got = nullptr;
    for (auto& t : g_traits) { if (!ok(t)) continue; got = &t; x -= t.weight; if (x <= 0) break; }
    if (got) chosen.push_back(got);
  }
  double hp = tier == 2 ? g_eliteHp : 1, speed = 1, dmg = 1, sx = 1, sy = 1, sz = 1, bd = 0, br = 0; int rp = tier == 2 ? g_eliteRp : g_rareRp;
  std::string traits;
  for (const EnemyTrait* t : chosen) {
    hp *= t->hp; speed *= t->speed; dmg *= t->damage; sx *= t->sx; sy *= t->sy; sz *= t->sz; rp += t->rp;
    if (t->blastDamage > bd) bd = t->blastDamage; if (t->blastRadius > br) br = t->blastRadius;
    traits += (traits.empty() ? "" : ", ") + t->name;
  }
  EliteInfo info{ tier, "", ShipTitle(ce), traits, (float)dmg, rp, (float)bd, (float)br };
  info.name = chosen.empty() ? info.ship : chosen.front()->name + " " + info.ship;
  bool armed = false; for (const EnemyTrait* t : chosen) armed |= t->armed;
  if (armed && RollItem(&info.loot, &info.lootRarity, tier == 2 ? g_eliteDropMin : g_rareDropMin, true)) {   // a gun it can fire
    info.hasLoot = true;
    std::string plain = StripTags(info.loot.name);
    size_t at = info.traits.find("Armed");
    if (at != std::string::npos) info.traits.insert(at + 5, " (" + plain + ")");
  }
  ApplyElite(ce, EliteRoll{ hp, speed, sx, sy, sz }, info, false, traits);
}

// Makes ce the Rare/Elite of info: more HP, speed, size, an Armed ship's guns. fromHost: a co-op client showing the
// host's roll; the host moves the ship and fires its guns, so only HP and size are set here. shown: the traits as the log
// and the kill feed name them (without an Armed ship's loot).
static void ApplyElite(void* ce, const EliteRoll& roll, EliteInfo info, bool fromHost, const std::string& shown) {
  void* hull = *(void**)((char*)ce + g_offCeHull);
  int parts = 0;
  if (roll.hp != 1) for (int i = 0; i < ListCount(hull); i++) {
    void* part = ListAt(hull, i); if (!part) continue;
    float m = *(float*)((char*)part + g_offHpMult) * (float)roll.hp; void* a[1] = { &m };
    Invoke(m_hpForceStart, part, a); parts++;                          // the game's own spawn call: sets the multiplier and fills HP
  }
  EliteRestore r{ nullptr, { 1, 1, 1 }, 1, false, false };
  if (!fromHost && roll.speed != 1 && g_offShipSpeed >= 0) { r.speed = *(float*)((char*)ce + g_offShipSpeed); r.sped = true; *(float*)((char*)ce + g_offShipSpeed) = r.speed * (float)roll.speed; }
  if (roll.sx != 1 || roll.sy != 1 || roll.sz != 1) {
    void* tr = Invoke(m_getTransform, ce);
    void* b = tr ? Invoke(m_getLocalScale, tr) : nullptr;
    if (b) {
      r.scale = *(V3*)((char*)b + 0x10); r.scaled = true;
      V3 s{ r.scale.x * (float)roll.sx, r.scale.y * (float)roll.sy, r.scale.z * (float)roll.sz }; void* a[1] = { &s };
      Invoke(m_setLocalScale, tr, a);
    }
  }
  int armedMounts = info.hasLoot && !fromHost ? ArmShip(ce, info.loot, &r, &info.damage) : 0;
  if (info.hasLoot && !fromHost) Log("ELITE armed: '%s' carries '%s' (%s) on %d mounts: %s", info.ship.c_str(), info.loot.name.c_str(), info.lootRarity.c_str(), armedMounts, info.loot.description.c_str());
  if (r.sped || r.scaled || !r.guns.empty()) { r.handle = Pin(ce); g_eliteRestore[ce] = r; }
  g_elites[ce] = info;
  if (g_traits.empty() && !fromHost) DLog("elites: no traits in enemies.ini");
  Log("ELITE%s: %s '%s' (%s): hp x%.2f on %d parts, speed x%.2f, damage x%.2f, size %.2f/%.2f/%.2f, +%d RP", fromHost ? " (picked by the co-op host)" : "",
      info.tier == 2 ? "Elite" : "Rare", info.ship.c_str(), shown.c_str(), roll.hp, parts, roll.speed, info.damage, roll.sx, roll.sy, roll.sz, info.rp);
  if (g_eliteAnnounce) {
    const char* hex = info.tier == 2 ? "#ffb300" : "#4fc3f7";
    ShowMessage(Colored(info.tier == 2 ? "Elite" : "Rare", hex) + " enemy: " + Colored(info.name, hex) + (shown.empty() ? "" : " (" + shown + ")"));
  }
  if (!fromHost && g_coopRole == 1) CoopEliteMade(ce, roll, info, shown);    // co-op host: the other players get the same one
}

static void* PlayerCraft() {
  static void* m = nullptr; if (!m) { void* pc = FindClass("Core.Player", "Player"); m = pc ? FindMethod(pc, "get_playerCraft", 0) : nullptr; }
  return m ? Invoke(m, nullptr) : nullptr;
}

// Volatile: when the ship sinks it explodes. CombatEntity.explodeAt(pos) is the game's own damage-control explosion (its
// effect; ships without a DamageControlObject show none), then a splash of our own is queued the way
// DamageControlObject.explodeAt does it (SplashDamageManager.instance.splashdamage.Enqueue). SplashDamageManager.FixedUpdate
// skips hull parts of the splash's team, so it hurts the player's side only. killer must not be null: it is added to each
// damaged entity's damagedBy and gets creditKill later, so it is one of the ship's own weapon systems.
struct SplashData { V3 pos, dirTend; float range, visRange; bool boostRange; char pad0[3]; float damage; void* killer; int entityCode, teamCode, projectileCode; float massBonus; bool force, shake, bypassDir, ignoreUnderwater, networkOwner; char pad1[3]; };
static_assert(sizeof(SplashData) == 0x48, "SplashDamageManager.splash layout");
// Hurts every hull part not on teamCode within radius; killer gets the kill credit (an IKillCredible, never null).
// projectileCode: -1 for the mod's splashes; Overkill's spill uses its own (kSpillCode) so the gun effects leave it alone.
static bool QueueSplash(V3 pos, float radius, float damage, void* killer, int entityCode, int teamCode, int projectileCode = -1) {
  static void *fInst = nullptr, *mEnqueue = nullptr; static int offQueue = -2;
  if (offQueue == -2) {
    void* sk = FindClass("Core", "SplashDamageManager");
    fInst = sk ? il2cpp_class_get_field_from_name(sk, "instance") : nullptr;
    offQueue = FieldOffset(sk, "splashdamage");
  }
  void* mgr = nullptr; if (fInst) il2cpp_field_static_get_value(fInst, &mgr);
  void* queue = mgr && offQueue >= 0 ? *(void**)((char*)mgr + offQueue) : nullptr;
  if (!queue || !killer) return false;
  if (!mEnqueue) mEnqueue = FindMethod(il2cpp_object_get_class(queue), "Enqueue", 1);
  if (!mEnqueue) { Log("splash: Queue<splash>.Enqueue not found"); return false; }
  SplashData sd{};
  sd.pos = pos; sd.range = radius; sd.visRange = radius; sd.damage = damage; sd.killer = killer;
  sd.entityCode = entityCode; sd.teamCode = teamCode; sd.projectileCode = projectileCode;
  sd.force = true; sd.shake = true; sd.networkOwner = true;
  void* a[1] = { &sd }; Invoke(mEnqueue, queue, a);
  return true;
}
static void EliteBlast(void* ce, const EliteInfo& info) {
  static void* mExplode = nullptr; static int offEntityCode = -2;
  if (offEntityCode == -2) {
    void* cek = FindClass("Core", "CombatEntity"); void* le = FindClass("Core", "LiveEntity");
    offEntityCode = FieldOffset(le, "entityCode");
    mExplode = cek ? FindMethod(cek, "explodeAt", 1) : nullptr;
  }
  void* tr = Invoke(m_getTransform, ce); void* pb = tr && m_trGetPos ? Invoke(m_trGetPos, tr) : nullptr;
  if (!pb) return;
  V3 pos = *(V3*)((char*)pb + 0x10);
  if (mExplode) { void* a[1] = { &pos }; Invoke(mExplode, ce, a); }
  if (CoopHostElites()) { Log("ELITE blast: '%s' exploded (the co-op host deals its damage)", info.name.c_str()); return; }
  void* wm = g_offCraftWM >= 0 ? *(void**)((char*)ce + g_offCraftWM) : nullptr;
  void* arr = wm && g_offWMSystems >= 0 ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  void* killer = nullptr;
  for (int i = 0; arr && i < ArrLen(arr) && !killer; i++) killer = ArrAt(arr, i);
  if (!killer || offEntityCode < 0 || !QueueSplash(pos, info.blastRadius, info.blastDamage, killer, *(int*)((char*)ce + offEntityCode), *(int*)((char*)ce + g_offLeTeam))) {
    Log("elites: '%s' could not explode (weapon system %p)", info.name.c_str(), killer); return;
  }
  Log("ELITE blast: '%s' exploded (%.0f damage, %.0f m)", info.name.c_str(), info.blastDamage, info.blastRadius);
  ShowMessage(Colored(info.name, info.tier == 2 ? "#ffb300" : "#4fc3f7") + " exploded!");
}

static void EliteScan() {
  void* eng = EngagementInstance();
  if (eng != g_eliteEng) {
    RestoreAllShips(); g_elites.clear(); g_eliteDead.clear(); g_seenAlive.clear(); g_eliteEng = eng; g_eliteFrame = 0;
    if (eng) DLog("elites: new battle");
  }
  if (!eng || ++g_eliteFrame % 20 != 0) return;
  if (!EliteLookups()) { g_elitesOn = false; return; }
  DWORD tick = GetTickCount();
  for (auto d = g_eliteDead.begin(); d != g_eliteDead.end(); ) { if (tick - d->second.second > 10000) d = g_eliteDead.erase(d); else ++d; }
  void* craft = PlayerCraft(); if (!craft) return;
  int myTeam = *(int*)((char*)craft + g_offLeTeam);
  void* reg = nullptr; il2cpp_field_static_get_value(g_hpRegField, &reg);
  std::unordered_set<void*> now;
  for (int i = 0; i < ListCount(reg); i++) {
    void* part = ListAt(reg, i); if (!part) continue;
    void* ce = *(void**)((char*)part + g_offHpEntity);
    if (!ce || ce == craft || !now.insert(ce).second) continue;
    bool alive = *(bool*)((char*)ce + g_offLeAlive);
    if (!alive) {                                                       // sunk (or not spawned yet); a reused ship is rolled again
      g_seenAlive.erase(ce);
      auto el = g_elites.find(ce);
      if (el != g_elites.end()) {
        if (el->second.blastDamage > 0 && el->second.blastRadius > 0) EliteBlast(ce, el->second);
        g_eliteDead[ce] = { el->second, tick }; g_elites.erase(el);
      }
      continue;
    }
    if (!g_seenAlive.insert(ce).second) continue;                       // already handled this life
    auto rs = g_eliteRestore.find(ce);
    if (rs != g_eliteRestore.end()) { RestoreShip(ce, rs->second); g_eliteRestore.erase(rs); }   // pooled ship from an earlier life
    if (*(int*)((char*)ce + g_offLeTeam) == myTeam || !IsShip(ce)) continue;
    if (CoopHostElites()) continue;                                    // co-op client: the host's rolls arrive by message
    double x = Rand01();
    if (x < g_eliteChance) MakeElite(ce, 2);
    else if (x < g_eliteChance + g_rareChance) MakeElite(ce, 1);
  }
}

// Shots from a Rare/Elite ship: the damage multiplier of its traits (CritScan sees every new shot).
static float EliteDamageOf(void* mount) {
  if (g_elites.empty() || g_offGmEntity < 0 || !mount) return 1.0f;
  auto e = g_elites.find(*(void**)((char*)mount + g_offGmEntity));
  return e == g_elites.end() ? 1.0f : e->second.damage;
}

// A kill adds call-in progress in PlayerScoreManager.addScore: PlayerCombatHeat.instance.enqueueScore(round(killScore x
// parameters.gainRate)), and enqueueScore applies the host multiplier. The bonus queues (mult - 1) more of the same.
static void EliteCallinBonus(void* killed, double mult) {
  if (mult <= 1) return;
  static void *fInst = nullptr, *mEnqueue = nullptr, *mKillScore = nullptr; static int offParams = -2, offGain = -1;
  if (offParams == -2) {
    void* hk = FindClass("Core.Player", "PlayerCombatHeat"); void* ce = FindClass("Core", "CombatEntity"); void* pk = FindClass("Core", "CombatHeatParameters");
    fInst = hk ? il2cpp_class_get_field_from_name(hk, "instance") : nullptr;
    mEnqueue = hk ? FindMethod(hk, "enqueueScore", 1) : nullptr;
    mKillScore = ce ? FindMethod(ce, "getKillScore", 0) : nullptr;
    offParams = FieldOffset(hk, "parameters"); offGain = FieldOffset(pk, "gainRate");
  }
  if (!fInst || !mEnqueue || !mKillScore || offParams < 0 || offGain < 0) { Log("elites: call-in bonus lookups failed"); return; }
  void* heat = nullptr; il2cpp_field_static_get_value(fInst, &heat);
  void* params = heat ? *(void**)((char*)heat + offParams) : nullptr;
  if (!params) return;
  int score = Unbox(Invoke(mKillScore, killed));
  int extra = (int)std::lround(score * *(float*)((char*)params + offGain) * (mult - 1));
  DLog("elites: call-in bonus %d (kill score %d, x%.2f)", extra, score, mult);
  if (extra <= 0) return;
  void* a[1] = { &extra }; Invoke(mEnqueue, heat, a);
}

static void PayRp(int amount, const char* reason);
static void CoopKillOut(const EliteInfo& info);
// Pays this machine's player for a Rare/Elite that sank: on a kill credited here, or (fromPeer) when another co-op player's
// machine says it credited one. Every player gets their own reward once per ship, as with RP crates.
static void EliteKilled(void* killed, bool fromPeer = false) {
  // the kill credit usually comes before the scan sees the ship sink: mark it, the scan still needs it (Volatile)
  EliteInfo info;
  auto e = g_elites.find(killed);
  if (e != g_elites.end()) { if (e->second.rewarded) return; e->second.rewarded = true; info = e->second; }
  else { auto d = g_eliteDead.find(killed); if (d == g_eliteDead.end() || d->second.first.rewarded) return; info = d->second.first; g_eliteDead.erase(d); }
  const char* hex = info.tier == 2 ? "#ffb300" : "#4fc3f7";
  Log("ELITE killed: %s '%s'%s", info.tier == 2 ? "Elite" : "Rare", info.name.c_str(), fromPeer ? " (sunk on another player's screen; your own reward)" : "");
  if (!fromPeer) CoopKillOut(info);
  ShowMessage(Colored(info.tier == 2 ? "Elite" : "Rare", hex) + " sunk: " + Colored(info.name, hex));
  if (info.rp > 0) PayRp(info.rp, info.tier == 2 ? "Elite bounty" : "Rare bounty");
  EliteCallinBonus(killed, info.tier == 2 ? g_eliteCallin : g_rareCallin);
  Item it; std::string rarity;
  if (info.hasLoot) { it = info.loot; rarity = info.lootRarity; }       // Armed: the gun it was firing
  else if (g_unusualChance > 0 && Rand01() < g_unusualChance && RollUnique(&it, &rarity, true)) {}   // Unusual from Elite kills too
  else if (info.tier == 2 && g_uniqueCrate[2] > 0 && Rand01() < g_uniqueCrate[2] && RollUnique(&it, &rarity)) {}   // Elites: a Gunmetal crate's Unique odds
  else if (!RollItem(&it, &rarity, info.tier == 2 ? g_eliteDropMin : g_rareDropMin)) { Log("elite drop: nothing to roll (check drops.ini)"); return; }
  if (AutoScrapDrop(it, rarity, "DROP (elite)")) return;
  it.held = true;
  std::vector<std::string> dropped;
  if (!MakeRoomInLoot(it, &dropped)) { if (g_dropMessage) ShowMessage("Too big for the loot grid: " + Colored(it.name, RarityHex(rarity))); return; }
  if (!dropped.empty() && g_dropMessage) ShowMessage("Loot grid full: left behind " + dropped.front() + (dropped.size() > 1 ? " and " + std::to_string(dropped.size() - 1) + " more" : ""));
  AddToInventory(it);
  Log("DROP (elite): '%s' (%s) in '%s' based on '%s': %s", it.name.c_str(), rarity.c_str(), it.family.c_str(), it.base.c_str(), it.description.c_str());
  if (g_dropMessage) ShowMessage("Found: " + Colored(it.name, RarityHex(rarity)) + " (" + rarity + "), in the loot grid");
}

