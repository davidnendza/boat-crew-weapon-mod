// mount building: AddCloneMount, order entries, the slot catalogue (a fragment of weaponmod.cpp, included in order, not standalone)
// Copy one mount (and its GunType) and put it at the end of the slot's mounts list. Returns the new mount or null.
static const float kAutoRange = 3000.0f;                            // crewless guns: engagementDistance and fire control search (m)
// A found gun built on one of the mod's turret guns (built_from=, from a loot template) is that turret gun first, its
// stats, model, kept stock parts, sound and RP price, with its own rolled effects on top. inventory.ini keeps only the
// name, so a rebalanced turret gun reaches the found ones too.
static const Item& ExpandBuiltFrom(const Item& it, Item* buf) {
  if (it.builtFrom.empty()) return it;
  const Item* b = nullptr;
  for (const Item& x : g_items) if (x.builtIn && x.name == it.builtFrom) { b = &x; break; }
  if (!b) {
    static std::set<std::string> told;
    if (told.insert(it.builtFrom).second) Log("'%s': built_from '%s' is not one of the mod's own guns; it is a plain %s", it.name.c_str(), it.builtFrom.c_str(), it.base.c_str());
    return it;
  }
  *buf = it;
  buf->ops = b->ops; buf->ops.insert(buf->ops.end(), it.ops.begin(), it.ops.end());
  if (buf->model.empty()) { buf->model = b->model; buf->modelScale = b->modelScale; buf->modelOwnMaterial = b->modelOwnMaterial; buf->modelKeep = b->modelKeep; }
  if (buf->soundFrom.empty()) buf->soundFrom = b->soundFrom;
  if (buf->projectileFrom.empty()) buf->projectileFrom = b->projectileFrom;
  if (!buf->cratesPerRound) buf->cratesPerRound = b->cratesPerRound;
  buf->price = b->price;
  return *buf;
}
// The overrideTag of a hidden placeholder's GunType (a gun this inventory lacks or has worn out, holding its position).
// The mod's other guns are tagged WM_<name>, so it starts the same.
static const char* const kHiddenTag = "WM_HIDDEN_";
// A hidden placeholder carries its gun's name but is a copy of the slot's first stock gun: it must not take that gun's
// effects, uses or kill rewards (a loadout still pointing at a worn-out gun's position selects one).
// Cached per mount, cleared at every battle change (CritScan).
static std::unordered_map<void*, bool> g_hiddenMount;
static bool IsHiddenMount(void* mount) {
  if (!mount || g_offGunType < 0 || g_offOverrideTag < 0) return false;
  auto c = g_hiddenMount.find(mount);
  if (c != g_hiddenMount.end()) return c->second;
  void* gt = *(void**)((char*)mount + g_offGunType);
  bool hidden = gt && strncmp(Str(*(void**)((char*)gt + g_offOverrideTag)), kHiddenTag, strlen(kHiddenTag)) == 0;
  g_hiddenMount.emplace(mount, hidden);
  return hidden;
}
// crew_offset=x,y,z (fx crewOffsetX/Y/Z): moves the crew spots on this mount copy, metres in the mount's own frame.
// CrewMember.followPath (0x1809A52F0, from 0x1809A6474) sets a crewman at his GunMount.Seat.transform's position and turns
// him toward its rotation every frame he mans the gun, so moving the seat transforms moves the crew. The hand targets
// (Seat.holdL/holdR, GunMount.holdLeft/holdRight) move with them so the pose stays. Only transforms inside this copy are
// moved: one outside it would be shared with the stock gun.
static V3 QRotV(Q4 q, V3 v);
// A gun drawn far bigger than stock (Longer Tom's gunScale 2.5) carries its seats out with the scale, off the deck where the
// crew cannot path; above 1.7 (past Oversized's 1.6) the seats go back to the stock gun's spots around the mount's origin.
static void ApplyCrewOffset(void* mount, const Item& it) {
  V3 off = { (float)ItemFx(it, "crewOffsetX"), (float)ItemFx(it, "crewOffsetY"), (float)ItemFx(it, "crewOffsetZ") };
  float gs = (float)ItemFx(it, "gunScale"), sc = it.scale * (gs > 0 ? gs : 1.0f);
  bool unscale = sc > 1.7f;
  if (off.x == 0 && off.y == 0 && off.z == 0 && !unscale) return;
  static void *getPos = nullptr, *setPos = nullptr, *getRot = nullptr;
  static int state = 0;
  if (!state) {
    void* tr = FindClass("UnityEngine", "Transform");
    getPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; setPos = tr ? FindMethod(tr, "set_position", 1) : nullptr;
    getRot = tr ? FindMethod(tr, "get_rotation", 0) : nullptr;
    state = getPos && setPos && getRot && m_getParent && SeatsOffset() >= 0 ? 1 : -1;
    if (state > 0) Log("crew_offset: ready (GunMount.seats +0x%X)", g_offSeats);
    else Log("crew_offset: MISSING PIECES (Transform get_position %p set_position %p get_rotation %p, GunMount.seats %d); crew stay at the stock spots", getPos, setPos, getRot, g_offSeats);
  }
  if (state < 0) return;
  void* mt = Invoke(m_getTransform, mount);
  void* seats = *(void**)((char*)mount + g_offSeats);
  if (!mt || !seats) return;
  std::vector<std::pair<int, void*>> ts; int outside = 0;           // (depth below the mount, transform)
  auto add = [&](void* t) {
    if (!t) return;
    for (auto& e : ts) if (e.second == t) return;
    int d = 0; for (void* q = t; q && d < 32; q = Invoke(m_getParent, q), d++) if (q == mt) { ts.push_back({ d, t }); return; }
    outside++;
  };
  auto ref = [](void* obj, const char* f) -> void* { void* fi = il2cpp_class_get_field_from_name(il2cpp_object_get_class(obj), f); return fi ? *(void**)((char*)obj + il2cpp_field_get_offset(fi)) : nullptr; };
  size_t n = ArrLen(seats);
  for (size_t i = 0; i < n && i < 16; i++) {
    void* seat = ArrAt(seats, i);
    if (seat) { add(ref(seat, "transform")); add(ref(seat, "holdL")); add(ref(seat, "holdR")); }
  }
  add(ref(mount, "holdLeft")); add(ref(mount, "holdRight"));
  void* r = Invoke(getRot, mt);
  V3 w = QRotV(r ? *(Q4*)((char*)r + 0x10) : Q4{ 0, 0, 0, 1 }, off);
  // all read first, then set parents first: a parent's move carries its children, whose own set puts them back at at+w
  std::stable_sort(ts.begin(), ts.end(), [](const std::pair<int, void*>& x, const std::pair<int, void*>& y) { return x.first < y.first; });
  std::vector<V3> at; for (auto& e : ts) at.push_back(GetV3(getPos, e.second));
  V3 o = GetV3(getPos, mt); float k = unscale ? 1.0f / sc : 1.0f;      // the scale's origin is the mount's own position
  for (size_t i = 0; i < ts.size(); i++) {
    V3 p = { o.x + (at[i].x - o.x) * k + w.x, o.y + (at[i].y - o.y) * k + w.y, o.z + (at[i].z - o.z) * k + w.z };
    void* a[1] = { &p }; Invoke(setPos, ts[i].second, a);
  }
  static std::set<std::string> told;                                   // once per gun: every boat build moves them again
  if (told.insert(it.name).second)
    Log("  '%s': crew_offset (%.2f, %.2f, %.2f) m%s applied to %d seats/hand targets (%d outside the copy left alone)", it.name.c_str(), off.x, off.y, off.z,
        unscale ? ", seats kept at the stock gun's spots (gun drawn bigger)" : "", (int)ts.size(), outside);
}
// pivot_offset=x,y,z (fx pivotOffsetX/Y/Z): moves GunMount.pivotPoint, the transform that elevates, on this mount copy,
// metres in the mount's own frame; the barrels, shoot points and the model's turret/barrel/yaw parts move with it.
// (The 3-inch's pivot is at its breech, so a model built on it sits off centre without one.)
// The game only ever rotates pivotPoint (fixPivot, aimAt, turn, LateUpdateMe set rotations),
// so its new position stays. Seats and hand targets on the 3-inch are not under it: crew_offset moves the crew.
// Runs before the model is applied: yaw parts are placed at the pivot's local position.
static void ApplyPivotOffset(void* mount, const Item& it) {
  V3 off = { (float)ItemFx(it, "pivotOffsetX"), (float)ItemFx(it, "pivotOffsetY"), (float)ItemFx(it, "pivotOffsetZ") };
  if (off.x == 0 && off.y == 0 && off.z == 0) return;
  static void *getPos = nullptr, *setPos = nullptr, *getRot = nullptr;
  static int state = 0, offPivot = -1;
  if (!state) {
    void* tr = FindClass("UnityEngine", "Transform");
    getPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; setPos = tr ? FindMethod(tr, "set_position", 1) : nullptr;
    getRot = tr ? FindMethod(tr, "get_rotation", 0) : nullptr;
    offPivot = FieldOffset(il2cpp_object_get_class(mount), "pivotPoint");
    state = getPos && setPos && getRot && offPivot >= 0 ? 1 : -1;
    if (state > 0) Log("pivot_offset: ready (GunMount.pivotPoint +0x%X)", offPivot);
    else Log("pivot_offset: MISSING PIECES (Transform get_position %p set_position %p get_rotation %p, GunMount.pivotPoint %d); pivots stay", getPos, setPos, getRot, offPivot);
  }
  if (state < 0) return;
  void* mt = Invoke(m_getTransform, mount);
  void* pivot = *(void**)((char*)mount + offPivot);
  if (!mt || !pivot) return;
  void* r = Invoke(getRot, mt);
  V3 w = QRotV(r ? *(Q4*)((char*)r + 0x10) : Q4{ 0, 0, 0, 1 }, off);
  V3 at = GetV3(getPos, pivot);
  V3 p = { at.x + w.x, at.y + w.y, at.z + w.z }; void* a[1] = { &p }; Invoke(setPos, pivot, a);
  static std::set<std::string> told;
  if (told.insert(it.name).second) Log("  '%s': pivot_offset (%.2f, %.2f, %.2f) m applied", it.name.c_str(), off.x, off.y, off.z);
}
static void* AddCloneMount(void* wsObj, void* mounts, void* source, const Item& found) {
  Item expanded; const Item& it = ExpandBuiltFrom(found, &expanded);
  void* gunType = *(void**)((char*)source + g_offGunType);
  void* gt2 = CloneObject(gunType);
  if (!gt2) { Log("  could not copy the GunType"); return nullptr; }
  SetName(gt2, it.name.c_str());
  SetRef(gt2, (char*)gt2 + g_offOverrideTag, il2cpp_string_new(((it.retired ? kHiddenTag : "WM_") + it.name).c_str()));
  // Found guns cost what their base gun costs (a price= in inventory.ini is ignored). RP is a pool: an equipped gun
  // ties up its price until it is swapped out.
  int* price = (int*)((char*)gt2 + g_offPrice);
  if (it.fromInventory) {                                              // built_from: the turret gun's price
    double cut = ItemFx(it, "priceCut");                               // of Cheapness: this share off (0.4 = 40% less)
    *price = (int)std::lround((!it.builtFrom.empty() && it.price > 0 ? it.price : *price) * g_priceScale * (cut > 0 && cut < 1 ? 1 - cut : 1));
  }
  else if (it.price >= 0) *price = it.price;
  // A hidden placeholder (worn out, scrapped, or missing from this save) costs nothing: as a copy of the slot's first
  // stock gun it would keep that price, and CustomCollection.getTotalCost (also the boat-loss penalty and
  // CampaignState.craftCostOffset) sums the selected items live until the loadout screen empties the slot (UnequipRetired).
  if (it.retired) *price = 0;
  if (!it.description.empty() || it.uses > 0) {
    std::string d = it.description;
    if (it.uses > 0) d += (d.empty() ? "" : " ") + std::string("Missions left: ") + std::to_string(it.uses) + " (a slot spends one in a battle where it fires).";
    SetRef(gt2, (char*)gt2 + g_offDescription, il2cpp_string_new(d.c_str()));
  }
  for (const Op& op : it.ops) if (!op.proj && !op.mount && !op.sphere && !op.fx) ApplyOp(gt2, op);
  // Crewless guns are fired by the boat's FireControlMaster: fillCasters drops a non-plane target beyond
  // GunType.engagementDistance (code 5) and aimAtTargets fires only inside it (x3 for planes). Utility guns and rocket
  // launchers have 0, so an Automated one would never get a ship target. The fire control sees ships out to
  // EntityType.fireControlSystemSearchDistance x weather visibility (2500 on the player's boat; AutoUtilitySearch raises it).
  bool crewless = false;
  for (const Op& op : it.ops) if (op.mount && !op.mul && op.field == "requiresCrew" && op.value == 0) crewless = true;
  static int offEngage = -2;
  if (offEngage == -2) offEngage = FieldOffset("Core", "GunType", "engagementDistance");
  if (crewless && offEngage >= 0 && *(float*)((char*)gt2 + offEngage) <= 0) {
    *(float*)((char*)gt2 + offEngage) = kAutoRange;                     // the gun's physical range (maxRange) still applies
    DLog("  '%s': crewless, engagementDistance 0 -> %.0f so the fire control can pick ship targets", it.name.c_str(), kAutoRange);
  }
  // found Uncommon and Rare guns always get their own projectile types, so their tracers can take the rarity color
  bool ownShots = (g_tracerRarity && it.fromInventory && RarityOf(it.rarity) >= 1) || ItemFx(it, "tracerColor") > 0;
  for (const Op& op : it.ops) if (op.proj) ownShots = true;
  if (!it.fragmentFrom.empty() || !it.projectileFrom.empty()) ownShots = true;
  if (ownShots) CloneProjectiles(gunType, gt2, it);
  bool soundsCopied = !it.soundFrom.empty() && CopySounds(gt2, it);
  if (it.builtIn || !it.builtFrom.empty()) {                          // the numbers the game will use, once per gun
    static std::set<std::string> told;
    if (told.insert(it.name).second) {
      void* ammo = g_offAmmo[0] >= 0 ? *(void**)((char*)gt2 + g_offAmmo[0]) : nullptr;
      void* pt = ammo && ListCount(ammo) ? ListAt(ammo, 0) : nullptr;
      std::string sounds = soundsCopied ? "the " + it.soundFrom + "'s" : it.soundFrom.empty() ? "its base gun's" : "its base gun's (sound_from not found)";
      Log("%s '%s' (a %s): %d RP, %.0f rounds/min, %d-round magazine, %.1f s reload, accuracy %d, opens fire at %.0f m; "
          "shells %.0f damage, %.0f splash over %.1f m, %.0f m/s; sounds: %s", it.family == "Tub Turret" ? "turret gun" : "side-grade", it.name.c_str(), (it.builtFrom.empty() ? it.base : "found " + it.builtFrom).c_str(), *price, NumField(gt2, "roundsPerMinute"),
          (int)NumField(gt2, "MagSize"), NumField(gt2, "reloadCooldown"), (int)NumField(gt2, "Accuracy"), NumField(gt2, "engagementDistance"),
          NumField(pt, "BaseDamage"), NumField(pt, "SplashDamageBase"), NumField(pt, "SplashDamageRange"), NumField(pt, "MuzzleVelocity"), sounds.c_str());
    }
  }

  void* mount2 = CloneObject(source);
  if (!mount2) { Log("  could not copy the mount"); return nullptr; }
  { void* go = Invoke(m_getGameObject, mount2); bool off = false; void* a[1] = { &off }; if (go) Invoke(m_setActive, go, a); }   // a copy of the equipped gun would otherwise sit on deck, live
  void* srcT = Invoke(m_getTransform, source);
  void* t2 = Invoke(m_getTransform, mount2);
  void* parent = srcT ? Invoke(m_getParent, srcT) : nullptr;
  if (t2 && parent) {
    bool keepWorld = false;
    void* a[2] = { parent, &keepWorld };
    Invoke(m_setParent, t2, a);
    // Instantiate leaves the copy at the boat's origin; copy the original's local placement or it renders inside the hull.
    void* exc = nullptr; V3 lp = {}; Q4 lr = { 0, 0, 0, 1 }; V3 ls = { 1, 1, 1 };
    void* r = il2cpp_runtime_invoke(m_getLocalPos, srcT, nullptr, &exc); if (r && !exc) lp = *(V3*)((char*)r + 0x10);
    r = il2cpp_runtime_invoke(m_getLocalRot, srcT, nullptr, &exc); if (r && !exc) lr = *(Q4*)((char*)r + 0x10);
    r = il2cpp_runtime_invoke(m_getLocalScale, srcT, nullptr, &exc); if (r && !exc) ls = *(V3*)((char*)r + 0x10);
    void* a1[1] = { &lp }; Invoke(m_setLocalPos, t2, a1);
    void* a2[1] = { &lr }; Invoke(m_setLocalRot, t2, a2);
    float gs = (float)ItemFx(it, "gunScale"), sc = it.scale * (gs > 0 ? gs : 1.0f);   // gunScale: cosmetic Oversized/Compact
    if (sc != 1.0f) { ls.x *= sc; ls.y *= sc; ls.z *= sc; }
    void* a3[1] = { &ls }; Invoke(m_setLocalScale, t2, a3);
  } else Log("  could not place the copy (transform %p parent %p)", t2, parent);
  std::string shown = (!it.rarity.empty() && !it.retired) ? Colored(it.name, RarityHex(it.rarity)) : it.name;
  SetName(mount2, shown.c_str());
  SetRef(mount2, (char*)mount2 + g_offGunType, gt2);
  // mount.* edits act on this GunMount (e.g. requiresCrew), sphere.* on its targeting sphere (EntitySphereOverlap).
  // WeaponSystem.freeFireAI, the game's own crewless mode for AI ships, sets requiresCrew=0, infiniteAmmo=1 on the mount
  // and constrainTargets=0 on the sphere; the 'Automated' effect copies that.
  bool anySphere = false;
  int elevOps = 0, elevOk = 0;                                        // elevation= (gunSad/gunHappy, see the item keys)
  double sad0 = NumField(mount2, "gunSad"), happy0 = NumField(mount2, "gunHappy");
  for (const Op& op : it.ops) {
    if (op.mount && op.field == "seatsRemoved") { RemoveSeats(mount2, (int)op.value); continue; }
    bool elev = op.mount && (op.field == "gunSad" || op.field == "gunHappy");
    if (op.mount && ApplyOp(mount2, op) && elev) elevOk++;
    if (elev) elevOps++;
    if (op.sphere) anySphere = true;
  }
  if (elevOps) {
    static std::set<std::string> told;                                 // once per gun
    if (told.insert(it.name).second) {
      if (elevOk == elevOps) Log("  '%s': elevation applied: %.0f to %.0f deg (stock mount %.0f to %.0f)", it.name.c_str(), NumField(mount2, "gunSad"), NumField(mount2, "gunHappy"), sad0, happy0);
      else Log("  '%s': elevation MISSING PIECES: GunMount gunSad/gunHappy not found; stock limits kept", it.name.c_str());
    }
  }
  // fillCasters blocks a target (code 2) via GunMount.isBlockedByDepression unless GunMount.barrage is set: the straight-line
  // angle to it must lie in gunSad..gunHappy. Player mortars have both 0, so nothing below the tube passes; the game's AI
  // mortars set barrage instead (90/150 mm), so a crewless copy does the same.
  // Barrage alone is not enough: aimAt lifts the aim by a lob angle, but clamps the pivot to -gunHappy..-gunSad,
  // and aimAtTargets shoots only once Quaternion.Angle(pivot, aim) < maxGunAngleForShot (2.25). AI mortars have gunHappy 90.
  if (crewless && NumField(mount2, "gunSad") == 0 && NumField(mount2, "gunHappy") == 0) {
    Op o; o.mount = true; o.mul = false; o.field = "barrage"; o.value = 1;
    bool ok = ApplyOp(mount2, o);
    o.field = "gunHappy"; o.value = 90;
    bool okUp = ApplyOp(mount2, o);
    static std::set<std::string> told;
    if (told.insert(it.name).second) Log("  '%s': crewless with 0/0 elevation limits: barrage %s, gunHappy 90 %s (as on AI mortars: no elevation check, pivot may lob)", it.name.c_str(), ok ? "set" : "NOT FOUND", okUp ? "set" : "NOT FOUND");
  }
  ApplyCrewOffset(mount2, it);
  ApplyPivotOffset(mount2, it);
  if (anySphere) {
    void* sphere = FindChildComponent(mount2, "Core", "EntitySphereOverlap");
    if (sphere) { for (const Op& op : it.ops) if (op.sphere) ApplyOp(sphere, op); }
    else DLog("  '%s': no targeting sphere under the mount; sphere edits skipped", it.name.c_str());
  }
  if (!it.model.empty()) ApplyModelGuarded(mount2, it);
  ApplyPaintGuarded(mount2, it);                                      // after the model: its parts in the gun's materials get the paint too
  if (!ListAdd(mounts, mount2)) { Log("  could not add the copy to the mounts list"); return nullptr; }
  if (g_offDetailables >= 0) { void* det = *(void**)((char*)wsObj + g_offDetailables); if (det) ListAdd(det, mount2); }
  return mount2;
}

static void* AddOrderEntry(void* orderClone, const char* name, int index) {
  void* entries = *(void**)((char*)orderClone + g_offEntries);
  void* e = il2cpp_object_new(g_entryClass);
  if (!e || !entries) return nullptr;
  void* last = ListCount(entries) ? ListAt(entries, ListCount(entries) - 1) : nullptr;
  SetRef(e, (char*)e + g_offEName, il2cpp_string_new(name));
  *(int*)((char*)e + g_offEIndex) = index;
  *(int*)((char*)e + g_offEIndexAlt) = last ? *(int*)((char*)last + g_offEIndexAlt) : -1;
  *(bool*)((char*)e + g_offEAlt) = false;
  return ListAdd(entries, e) ? e : nullptr;
}
// A gun scrapped on the open loadout screen leaves every slot's list at once, or it stays pickable until the boat is
// rebuilt and the unequip job takes it straight back off. Only the list entry goes: the mount keeps its position, as a
// rebuilt boat's hidden placeholder does. Mod guns are only in the per-slot SelectionOrder copies; the register alone
// misses a turret slot of the loadout screen's boat, so SlotSystems adds both boats' slots.
static std::vector<void*> SlotSystems();
static int UnlistItem(const std::string& name) {
  int removed = 0;
  for (void* ws : SlotSystems()) {
    void* order = *(void**)((char*)ws + g_offOrder);
    void* entries = order ? *(void**)((char*)order + g_offEntries) : nullptr;
    for (int j = ListCount(entries) - 1; j >= 0; j--) {
      void* e = ListAt(entries, j);
      if (e && name == Str(*(void**)((char*)e + g_offEName)) && ListRemoveAt(entries, j)) removed++;
    }
  }
  return removed + TorpUnlist(name);
}

static void LogCatalogue(const std::string& family, void* mounts, void* ws) {
  static std::vector<std::string> seen;
  for (auto& s : seen) if (s == family) return;
  seen.push_back(family);
  std::string guns;
  for (int i = 0; i < ListCount(mounts); i++) {
    void* m = ListAt(mounts, i); if (!m) continue;
    void* gt = *(void**)((char*)m + g_offGunType);
    guns += "[" + std::to_string(i) + "] '" + (gt ? NameOf(gt) : std::string("?")) + "' ";
  }
  DLog("catalogue: family '%s' has guns (base= values): %s", family.c_str(), guns.c_str());
  // which flags differ between player-aimed (utility) and crew-aimed guns; read-only
  static int offFollow = -2, offIndep = -2, offNoAim = -2, offReq = -2;
  if (offFollow == -2) {
    offFollow = FieldOffset("Core", "WeaponSystem", "followTargetingMode"); offIndep = FieldOffset("Core", "GunMount", "independentFromEntityAI");
    offNoAim = FieldOffset("Core", "GunMount", "noAim"); offReq = FieldOffset("Core", "GunMount", "requiresCrew");
  }
  std::string flags;
  for (int i = 0; i < ListCount(mounts); i++) {
    void* m = ListAt(mounts, i); if (!m) continue;
    void* gt = *(void**)((char*)m + g_offGunType); if (!gt) continue;
    char b[160]; snprintf(b, sizeof b, "'%s' seats=%d requiresCrew=%d independentFromEntityAI=%d noAim=%d; ", NameOf(gt).c_str(), SeatCount(m),
      offReq >= 0 ? *(bool*)((char*)m + offReq) : -1, offIndep >= 0 ? *(bool*)((char*)m + offIndep) : -1, offNoAim >= 0 ? *(bool*)((char*)m + offNoAim) : -1);
    flags += b;
  }
  DLog("catalogue: family '%s' slot followTargetingMode=%d; %s", family.c_str(), offFollow >= 0 && ws ? *(bool*)((char*)ws + offFollow) : -1, flags.c_str());
}

