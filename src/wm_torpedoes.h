// found torpedoes: launchers, tiers, torpedoes in the water (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- found torpedoes -------------------------------------------------------------------------------------------------
//  - A TorpedoLauncher is a SelectableObject like a WeaponSystem, and a save keeps the chosen POSITION in its
//    List<TorpedoTier> torpedos. A found torpedo is a copy of a stock TorpedoTier at a stable position (positions.ini, as
//    for guns) with its own TorpedoType copy, filled in TorpedoLauncher.Awake and in selectIndex (a saved position beyond
//    the list fills it first). A launcher and its mirror (symmetric, FL/FR) get the same list.
//  - A launched Torpedo is a pooled copy of tier.prefab and keeps its PREFAB's type (+0x48), which FixedUpdate reads
//    live; a copied prefab would not be in the pool. So the Torpedo.FixedUpdate hook gives each new launch (projectileCode)
//    its tier's type plus detonateTime/armingTime, and puts the stock type back for a stock launch.
//  - Homing (fx torpSeek, deg/s) steers after the game's FixedUpdate; the game's own radarGuided homing is switched off
//    on every torpedo the mod steers.
//  - Chomp / Crawl: the torpedo runs 2.5-4 m deep, so a model (models\chomper_*.obj, commando_*.obj) rides the water
//    above it. Sounds use the FMOD core API in fmodstudio.dll: Unity's own audio is stripped from this build. The Crawl
//    type sets detectionByEntityChanceMultiplier 0, so no ship spots it or steers away.
// Torpedoes give no kill credit (TorpedoLauncher.creditKill is an empty stub), so no on-kill effects roll on them.
// Co-op: every machine fires its own copy of a torpedo from its own launcher's list (ClaimFiredTorpedoes sends only the
// launcher index), so another player sees a found torpedo as whatever sits at that position on their machine. Not synced.
struct TorpApi {
  bool tried = false, ok = false;
  void *launcherCls = nullptr, *tierCls = nullptr, *torpCls = nullptr, *tierCtor = nullptr, *tierGetType = nullptr, *regField = nullptr, *selectMI = nullptr;
  int lTorpedos = -1, lSelIndex = -1, lSymmetric = -1, lEntity = -1, wmLaunchers = -1, leVel = -1;
  int tName = -1, tDisplay = -1, tType = -1, tProps = -1;
  int tDummies = -1;                                                  // TorpedoTier.dummies: the torpedo models waiting on the boat
  int yIdle = -1;                                                    // TorpedoType.idleDepth: the running depth the game holds
  int yTag = -1, yPrice = -1, yDesc = -1, yUnlock = -1, ySprite = -1, yDetonation = -1, yArming = -1;
  int pDetected = -1;                                                 // Torpedo.detectedByFactions (HashSet<int>): teams that spotted it
  int pBody = -1, pFrom = -1, pTier = -1, pType = -1, pDetonate = -1, pArm = -1, pExploded = -1, pCode = -1, pTend = -1, pInWater = -1, pFloat = -1, fpDepth = -1;
  void *rbVel = nullptr, *rbSetVel = nullptr, *rbPos = nullptr, *rbSetRot = nullptr, *timeNow = nullptr, *timeScale = nullptr, *fdt = nullptr;
  void *goActive = nullptr, *trSetPR = nullptr, *trPos = nullptr, *hideFlags = nullptr, *trSetRot = nullptr;
  void *trRot = nullptr, *vfxSpawn = nullptr, *vfxInst = nullptr; int ySplash = -1;   // the dolphin leap (optional)
  std::vector<std::pair<int, int>> copy;                              // TorpedoTier fields a copy takes: offset, bytes (0 = reference)
};
static TorpApi g_tp;
static int g_torpInjectFaults = 0, g_torpRunFaults = 0, g_torpLookFaults = 0;
static bool TorpSetup() {
  if (g_tp.tried) return g_tp.ok;
  TorpApi& a = g_tp; a.tried = true;
  a.launcherCls = FindClass("Core", "TorpedoLauncher"); a.torpCls = FindClass("Core", "Torpedo");
  void* ty = FindClass("Core", "TorpedoType"); void* fp = FindClass("Core", "FloatPoint"); void* wm = FindClass("Core", "WeaponManager"); void* le = FindClass("Core", "LiveEntity");
  if (a.launcherCls) { void* it = nullptr; void* nk; while ((nk = il2cpp_class_get_nested_types(a.launcherCls, &it))) if (!strcmp(il2cpp_class_get_name(nk), "TorpedoTier")) a.tierCls = nk; }
  a.tierCtor = a.tierCls ? FindMethod(a.tierCls, ".ctor", 0) : nullptr; a.tierGetType = a.tierCls ? FindMethod(a.tierCls, "get_type", 0) : nullptr;
  a.regField = a.launcherCls ? il2cpp_class_get_field_from_name(a.launcherCls, "register") : nullptr;
  a.selectMI = a.launcherCls ? il2cpp_class_get_method_from_name(a.launcherCls, "selectIndex", 1) : nullptr;
  a.lTorpedos = FieldOffset(a.launcherCls, "torpedos"); a.lSelIndex = FieldOffset(a.launcherCls, "selectedIndex"); a.lSymmetric = FieldOffset(a.launcherCls, "symmetric"); a.lEntity = FieldOffset(a.launcherCls, "entity");
  a.wmLaunchers = FieldOffset(wm, "launchers"); a.leVel = FieldOffset(le, "realtimeVelocity");
  a.tDummies = FieldOffset(a.tierCls, "dummies");
  a.tName = FieldOffset(a.tierCls, "name"); a.tDisplay = FieldOffset(a.tierCls, "displayName"); a.tType = FieldOffset(a.tierCls, "_type"); a.tProps = FieldOffset(a.tierCls, "torpedoProperties");
  a.yIdle = FieldOffset(ty, "idleDepth");
  a.yTag = FieldOffset(ty, "overrideTag"); a.yPrice = FieldOffset(ty, "price"); a.yDesc = FieldOffset(ty, "description"); a.yUnlock = FieldOffset(ty, "unlock"); a.ySprite = FieldOffset(ty, "sprite");
  a.yDetonation = FieldOffset(ty, "detonationTime"); a.yArming = FieldOffset(ty, "armingTime");
  a.pDetected = FieldOffset(a.torpCls, "detectedByFactions");
  a.pBody = FieldOffset(a.torpCls, "body"); a.pFrom = FieldOffset(a.torpCls, "launchedFrom"); a.pTier = FieldOffset(a.torpCls, "launchedFromTier"); a.pType = FieldOffset(a.torpCls, "torpedoType");
  a.pDetonate = FieldOffset(a.torpCls, "detonateTime"); a.pArm = FieldOffset(a.torpCls, "armingTime"); a.pExploded = FieldOffset(a.torpCls, "explodedAfterLaunch"); a.pCode = FieldOffset(a.torpCls, "projectileCode");
  a.pTend = FieldOffset(a.torpCls, "tendToLine"); a.pInWater = FieldOffset(a.torpCls, "timeInWater"); a.pFloat = FieldOffset(a.torpCls, "floatPoint"); a.fpDepth = FieldOffset(fp, "_depth");
  void* rb = FindClass("UnityEngine", "Rigidbody"); void* tm = FindClass("UnityEngine", "Time"); void* go = FindClass("UnityEngine", "GameObject");
  void* tr = FindClass("UnityEngine", "Transform"); void* uo = FindClass("UnityEngine", "Object");
  a.rbVel = rb ? FindMethod(rb, "get_linearVelocity", 0) : nullptr; a.rbSetVel = rb ? FindMethod(rb, "set_linearVelocity", 1) : nullptr;
  a.rbPos = rb ? FindMethod(rb, "get_position", 0) : nullptr; a.rbSetRot = rb ? FindMethod(rb, "set_rotation", 1) : nullptr;
  a.timeNow = tm ? FindMethod(tm, "get_time", 0) : nullptr; a.timeScale = tm ? FindMethod(tm, "get_timeScale", 0) : nullptr; a.fdt = tm ? FindMethod(tm, "get_fixedDeltaTime", 0) : nullptr;
  a.goActive = go ? FindMethod(go, "get_activeInHierarchy", 0) : nullptr; a.trSetPR = tr ? FindMethod(tr, "SetPositionAndRotation", 2) : nullptr;
  a.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; a.hideFlags = uo ? FindMethod(uo, "set_hideFlags", 1) : nullptr;
  a.trSetRot = tr ? FindMethod(tr, "set_rotation", 1) : nullptr; a.trRot = tr ? FindMethod(tr, "get_rotation", 0) : nullptr;
  { void* vk = FindClass("Core", "VFXManager"); a.vfxSpawn = vk ? FindMethod(vk, "spawnVFX", 4) : nullptr; a.vfxInst = vk ? il2cpp_class_get_field_from_name(vk, "instance") : nullptr;
    a.ySplash = FieldOffset(FindClass("Core", "ProjectileType"), "impactSplashPrefab");
    if (!a.trRot || !a.vfxSpawn || !a.vfxInst || a.ySplash < 0) Log("dolphin torpedoes: a lookup failed (get_rotation %p spawnVFX %p instance %p impactSplashPrefab %d); no leaps or no splash", a.trRot, a.vfxSpawn, a.vfxInst, a.ySplash); }
  // A found torpedo's tier is a field-by-field copy of its stock tier (TorpedoTier is a plain serialized class, so
  // Instantiate cannot copy it): references through SetRef, numbers as bytes.
  int missingCopy = 0;
  for (const char* n : { "name", "displayName", "prefab", "dummies", "launchDirection", "sound", "launchStrength", "tendToLine", "alwaysUseFirstDummyTransform", "_type" }) {
    void* f = a.tierCls ? il2cpp_class_get_field_from_name(a.tierCls, n) : nullptr;
    if (!f) { missingCopy++; continue; }
    std::string tn = TypeName(il2cpp_field_get_type(f)); void* fc = il2cpp_class_from_type(il2cpp_field_get_type(f));
    int bytes = tn == "System.Single" || tn == "System.Int32" || (fc && il2cpp_class_is_enum(fc)) ? 4 : tn == "System.Boolean" ? 1 : 0;
    a.copy.push_back({ (int)il2cpp_field_get_offset(f), bytes });
  }
  a.ok = a.launcherCls && a.tierCls && a.torpCls && a.tierCtor && a.tierGetType && a.regField && a.selectMI && !missingCopy && a.lTorpedos >= 0 && a.lSelIndex >= 0 &&
         a.lSymmetric >= 0 && a.lEntity >= 0 && a.wmLaunchers >= 0 && a.leVel >= 0 && a.tName >= 0 && a.tDisplay >= 0 && a.tType >= 0 && a.tProps >= 0 && a.yTag >= 0 &&
         a.yPrice >= 0 && a.yDesc >= 0 && a.yUnlock >= 0 && a.ySprite >= 0 && a.yDetonation >= 0 && a.yArming >= 0 && a.pBody >= 0 && a.pFrom >= 0 && a.pTier >= 0 &&
         a.pType >= 0 && a.pDetonate >= 0 && a.pArm >= 0 && a.pExploded >= 0 && a.pCode >= 0 && a.pTend >= 0 && a.pInWater >= 0 && a.pFloat >= 0 && a.fpDepth >= 0 &&
         a.rbVel && a.rbSetVel && a.rbPos && a.rbSetRot && a.timeNow && a.timeScale && a.fdt && a.goActive && a.trSetPR && a.trPos;
  if (!a.ok) Log("found torpedoes: a lookup failed (launcher %p tier %p torpedo %p ctor %p type %p register %p selectIndex %p, %d tier fields missing; offsets %d %d %d %d %d %d / %d %d %d %d / "
                 "%d %d %d %d %d %d %d / %d %d %d %d %d %d %d %d %d %d %d %d; Unity %p %p %p %p %p %p %p %p %p %p); torpedo drops are off",
                 a.launcherCls, a.tierCls, a.torpCls, a.tierCtor, a.tierGetType, a.regField, a.selectMI, missingCopy, a.lTorpedos, a.lSelIndex, a.lSymmetric, a.lEntity, a.wmLaunchers, a.leVel,
                 a.tName, a.tDisplay, a.tType, a.tProps, a.yTag, a.yPrice, a.yDesc, a.yUnlock, a.ySprite, a.yDetonation, a.yArming, a.pBody, a.pFrom, a.pTier, a.pType, a.pDetonate,
                 a.pArm, a.pExploded, a.pCode, a.pTend, a.pInWater, a.pFloat, a.fpDepth, a.rbVel, a.rbSetVel, a.rbPos, a.rbSetRot, a.timeNow, a.timeScale, a.fdt, a.goActive, a.trSetPR, a.trPos);
  return a.ok;
}
static std::string TierName(void* tier) { return Str(*(void**)((char*)tier + g_tp.tName)); }
// The stock tier's choice, as refreshAssets names it ("#7 Mk 24 Torpedo x2" -> "Mk 24 Torpedo x2"): the base= of a
// found torpedo. "" for one of the mod's tiers (named after its item).
static std::string TierBase(void* tier) {
  std::string n = TierName(tier);
  if (n.size() < 3 || n[0] != '#') return "";
  size_t sp = n.find(' ');
  return sp == std::string::npos ? "" : n.substr(sp + 1);
}
static void* TierType(void* tier) { return tier ? Invoke(g_tp.tierGetType, tier) : nullptr; }
static std::vector<void*> LaunchersOf(void* craft) {
  std::vector<void*> out;
  if (!craft || g_offCraftWM < 0 || !g_tp.ok) return out;
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* arr = wm ? *(void**)((char*)wm + g_tp.wmLaunchers) : nullptr;
  int n = ArrLen(arr);
  for (int i = 0; i < n && i < 32; i++) { void* l = ArrAt(arr, i); if (l) out.push_back(l); }
  return out;
}
static void TorpFamiliesOf(void* craft, std::vector<std::string>& out) {
  for (void* l : LaunchersOf(craft)) {
    void* order = *(void**)((char*)l + g_offOrder); if (!order) continue;
    std::string f = FamilyOf(order);
    if (std::find(out.begin(), out.end(), f) == out.end()) out.push_back(f);
  }
}
static void SafeTubeSync(void* l);                                    // the tube models' looks (after the torpedoes in the water)
static void SafeRemoteTubeSync(void* l);                              // the same for another player's launcher (coop_torpedoes)

// A found torpedo's TorpedoType: a copy of its stock tier's type with the item's edits, shared by every launcher it is
// in (one per item and stock type, kept for the session).
// Another player's found torpedo (coop_torpedoes) gets an entry of its own: owner >= 0 and its definition in remote, as
// they sent it. Entries are never erased: g_torpOfType and the diving copies (TorpDiveType) point into them.
struct TorpClone { void* type = nullptr; void* handle = nullptr; void* stock = nullptr; std::string item; int owner = -1; Item remote; };
static std::map<std::string, TorpClone> g_torpClones;                 // "<item>|<stock type>" (hidden ones "WM_HIDDEN_<item>|...");
                                                                     // another player's "R<owner>|<item>|<stamp>|<stock type>"
static std::unordered_map<void*, TorpClone*> g_torpOfType;           // a copy -> its entry
// The launchers of other players' boats (filled by RefreshRemoteWs) and the tiers this mod built in them for their
// found torpedoes (EnsureRemoteTorps). Both hold GC handles, so a destroyed launcher's address is never reused as a key.
struct RemoteLauncher { int owner, index; void* h; };
static std::unordered_map<void*, RemoteLauncher> g_remoteLaunchers;  // launcher -> its owner and index in their WeaponManager.launchers
struct RemoteTier { int owner; std::string name; unsigned long long stamp; bool hidden; void* h; };
static std::unordered_map<void*, RemoteTier> g_remoteTiers;          // TorpedoTier built for another player's launcher
static void ClearRemoteLaunchers() { for (auto& e : g_remoteLaunchers) Unpin(e.second.h); g_remoteLaunchers.clear(); }
static std::set<std::string> g_remoteTorpFailed;                     // "<owner>|<launcher>|<position>|<name>|<stamp>" (or "...|pad"):
                                                                     // a build that failed, not tried again this battle
static void ClearRemoteTorps() { ClearRemoteLaunchers(); for (auto& e : g_remoteTiers) Unpin(e.second.h); g_remoteTiers.clear(); g_remoteTorpFailed.clear(); }
static void* TorpTypeFor(const Item& found, void* stock, int owner = -1, unsigned long long stamp = 0) {
  Item buf; const Item& it = ExpandBuiltFrom(found, &buf);           // a found torpedo built_from a torpedo side-grade starts as that one
  std::string key = (it.retired ? std::string(kHiddenTag) : std::string()) + it.name + "|";
  if (owner >= 0) key = "R" + std::to_string(owner) + "|" + key + std::to_string(stamp) + "|";
  key += NameOf(stock);
  TorpClone& c = g_torpClones[key];
  if (!Alive(c.type)) {
    if (c.type) { g_torpOfType.erase(c.type); Unpin(c.handle); c = TorpClone(); }
    void* t = CloneObject(stock);
    if (!t) {
      static std::set<std::string> told;                               // by key: it is asked again on later builds
      if (told.insert(key).second) Log("found torpedoes: could not copy the TorpedoType of '%s'", it.name.c_str());
      return nullptr;
    }
    SetName(t, it.name.c_str());
    SetRef(t, (char*)t + g_tp.yTag, il2cpp_string_new(((it.retired ? kHiddenTag : "WM_") + it.name).c_str()));
    SetRef(t, (char*)t + g_tp.yUnlock, nullptr);                       // a found torpedo needs no progression unlock
    if (g_tp.hideFlags) { int keep = 32; void* a[1] = { &keep }; Invoke(g_tp.hideFlags, t, a); }   // DontUnloadUnusedAsset
    if (!it.retired) for (const Op& op : it.ops) if (!op.fx && !op.proj && !op.mount && !op.sphere) ApplyOp(t, op);
    // the mod steers a homing one itself, so the game's own radar homing (Mk 21/24/27) is switched off on the copy; it
    // would add its own turn and depth change toward a target of its choosing
    if (!it.retired && ItemFx(it, "torpSeek") > 0) { Op off; off.field = "radarGuided"; off.mul = false; off.value = 0; ApplyOp(t, off); }
    c.type = t; c.handle = Pin(t); c.stock = stock; c.item = it.retired ? "" : it.name;
    c.owner = owner; if (owner >= 0 && !it.retired) c.remote = it;    // a co-op placeholder keeps item empty, as a hidden one does
    g_torpOfType[t] = &c;
    if (owner >= 0) {
      static std::set<std::string> told;                               // by key: their item may share a name with one here
      if (!it.retired && told.insert(key).second)
        Log("co-op torpedoes: player %d's '%s' (a %s): %.0f damage over %.0f m, thrust %.0f, terminal velocity %.1f m/s, range %.0f m, hitbox %.1f m, spotted x%.2f; homing %.0f deg/s, look %s",
            owner, it.name.c_str(), it.base.c_str(), NumField(t, "damage"), NumField(t, "damageRange"), NumField(t, "thrust"), NumField(t, "terminalVelocity"),
            NumField(t, "despawnDistance"), NumField(t, "hitboxRadius"), NumField(t, "detectionByEntityChanceMultiplier"), ItemFx(it, "torpSeek"),
            ItemFx(it, "chomp") >= 2 ? "Chompette" : ItemFx(it, "chomp") >= 1 ? "Chomper" : ItemFx(it, "skim") > 0 ? "Flying Fish fish" : ItemFx(it, "crawl") > 0 ? "Sneaking Eel commando" : "stock");
    }
    else if (!it.retired) {
      static std::set<std::string> told;
      if (told.insert(it.name).second)
        Log("found torpedo '%s' (a %s): %.0f damage over %.0f m, thrust %.0f, terminal velocity %.1f m/s, range %.0f m, hitbox %.1f m, spotted x%.2f; mod homing %.0f deg/s (0 = off, any stock radar homing stays), look %s",
            it.name.c_str(), it.base.c_str(), NumField(t, "damage"), NumField(t, "damageRange"), NumField(t, "thrust"), NumField(t, "terminalVelocity"),
            NumField(t, "despawnDistance"), NumField(t, "hitboxRadius"), NumField(t, "detectionByEntityChanceMultiplier"), ItemFx(it, "torpSeek"),
            ItemFx(it, "chomp") >= 2 ? "Chompette" : ItemFx(it, "chomp") >= 1 ? "Chomper" : ItemFx(it, "skim") > 0 ? "Flying Fish fish" : ItemFx(it, "crawl") > 0 ? "Sneaking Eel commando" : "stock");
    }
  }
  if (owner >= 0) return c.type;                                       // another player's: never on this player's loadout screen
  // price and text follow the item (missions left change)
  int stockPrice = *(int*)((char*)stock + g_tp.yPrice), price = stockPrice;
  double cut = ItemFx(it, "priceCut");
  if (it.retired) price = 0;
  else if (it.fromInventory) price = (int)std::lround((!it.builtFrom.empty() && it.price > 0 ? it.price : stockPrice) * g_priceScale * (cut > 0 && cut < 1 ? 1 - cut : 1));
  else if (it.price >= 0) price = it.price;
  *(int*)((char*)c.type + g_tp.yPrice) = price;
  if (!it.retired && (!it.description.empty() || it.uses > 0)) {
    std::string d = it.description;
    if (it.uses > 0) d += (d.empty() ? "" : " ") + std::string("Missions left: ") + std::to_string(it.uses) + " (a slot spends one in a battle where it fires).";
    SetRef(c.type, (char*)c.type + g_tp.yDesc, il2cpp_string_new(d.c_str()));
  }
  return c.type;
}
static bool IsHiddenTier(void* tier) {
  void* ty = tier ? *(void**)((char*)tier + g_tp.tType) : nullptr;
  return ty && strncmp(Str(*(void**)((char*)ty + g_tp.yTag)), kHiddenTag, strlen(kHiddenTag)) == 0;
}
// The found torpedo a TorpedoType copy fires: this player's item, or another player's as they sent it (nullptr for a
// stock type or a hidden placeholder).
static const Item* ItemOfTorpType(void* type) {
  auto c = type ? g_torpOfType.find(type) : g_torpOfType.end();
  if (c == g_torpOfType.end() || c->second->item.empty()) return nullptr;
  return c->second->owner >= 0 ? &c->second->remote : FindItem(c->second->item);
}
// A copy of the stock tier src (same tube models, launch direction and sound) that fires the item.
static void* MakeTier(void* src, const Item& it, int owner = -1, unsigned long long stamp = 0) {   // owner >= 0: another player's
  void* stock = TierType(src); if (!stock) return nullptr;
  void* type = TorpTypeFor(it, stock, owner, stamp); if (!type) return nullptr;
  void* t = il2cpp_object_new(g_tp.tierCls); if (!t) return nullptr;
  Invoke(g_tp.tierCtor, t);
  for (const auto& f : g_tp.copy) {
    char* s = (char*)src + f.first; char* d = (char*)t + f.first;
    if (f.second == 0) SetRef(t, d, *(void**)s); else memcpy(d, s, f.second);
  }
  std::string shown = (!it.rarity.empty() && !it.retired) ? Colored(it.name, RarityHex(it.rarity)) : it.name;
  SetRef(t, (char*)t + g_tp.tName, il2cpp_string_new(it.name.c_str()));
  SetRef(t, (char*)t + g_tp.tDisplay, il2cpp_string_new(shown.c_str()));
  SetRef(t, (char*)t + g_tp.tType, type);
  // torpedoProperties (+0x60) keeps the empty list the constructor made: getProperties Clear()s it before rebuilding
  // the stat lines from the type (0x1809E940E); a null there throws inside InfoStatUI.fillValues and breaks the
  // loadout list's highlight
  return t;
}
static std::unordered_map<void*, std::pair<void*, void*>> g_torpOrderOwner;   // order copy -> its launcher's GC handle (pinned), so copies of gone launchers can be let go
static std::unordered_map<void*, void*> g_torpOrders;                // SelectionOrder copies made for launchers -> their GC handle (pinned,
                                                                     // so the address never comes back as another object)
static void InjectTorpedoes(void* launcher) {
  if (!g_tp.ok) return;
  void* order = *(void**)((char*)launcher + g_offOrder);
  void* list = *(void**)((char*)launcher + g_tp.lTorpedos);
  if (!order || ListCount(list) < 2) return;                          // planes' bomb bays have no slot family
  std::string family = FamilyOf(order);
  if (g_torpFams.insert(family).second) Log("found torpedoes: added '%s' to the torpedo slot families", family.c_str());
  if (!g_remoteLaunchers.empty() && g_remoteLaunchers.count(launcher)) return;   // another player's: holds their torpedoes (EnsureRemoteTorps)
  std::unordered_map<std::string, void*> byBase;
  std::unordered_map<std::string, int> at;                             // the mod's tiers already in this launcher
  void* firstReal = nullptr;
  for (int i = 0; i < ListCount(list); i++) {
    void* t = ListAt(list, i); if (!t) continue;
    std::string b = TierBase(t);
    if (!b.empty()) { byBase.emplace(b, t); if (!firstReal && i > 0 && TierType(t)) firstReal = t; }
    else at.emplace(TierName(t), i);
  }
  static std::set<std::string> listed;
  if (listed.insert(family).second) {
    std::string s; for (int i = 1; i < ListCount(list); i++) { void* t = ListAt(list, i); std::string b = t ? TierBase(t) : ""; if (!b.empty()) s += (s.empty() ? "" : ", ") + b; }
    Log("torpedo slot '%s' offers (base= values): %s", family.c_str(), s.c_str());
  }
  LearnFamily(family, byBase);                                         // a found Mk 8 fits any torpedo slot that offers the Mk 8, as found guns do
  std::vector<std::string>* registered = PosList(family, false);
  if (!registered || registered->empty() || !firstReal) return;
  std::vector<std::string> names = *registered;
  void* orderClone = g_torpOrders.count(order) ? order : nullptr;
  auto listIt = [&](const std::string& name, int index) {
    if (!orderClone) { orderClone = CloneObject(order); if (orderClone) { SetRef(launcher, (char*)launcher + g_offOrder, orderClone); g_torpOrders[orderClone] = Pin(orderClone); g_torpOrderOwner[orderClone] = { launcher, Pin(launcher) }; } }
    return orderClone && AddOrderEntry(orderClone, name.c_str(), index);
  };
  int added = 0, realized = 0;
  for (const std::string& name : names) {
    const Item* item = FindItemIn(name, family);
    if (item && item->held) item = nullptr;
    auto b = item ? byBase.find(item->base) : byBase.end();
    auto h = at.find(name);
    if (h != at.end()) {                                               // filled before: a placeholder whose torpedo is in this inventory now
      void* cur = ListAt(list, h->second);
      if (!item || item->retired || b == byBase.end() || !IsHiddenTier(cur)) continue;
      bool picked = *(int*)((char*)launcher + g_tp.lSelIndex) == h->second;
      void* t2 = MakeTier(b->second, *item);
      if (!t2 || !ListSetAt(list, h->second, t2)) continue;
      listIt(name, h->second); realized++;
      if (picked) { int idx = h->second; void* a[1] = { &idx }; Invoke(g_tp.selectMI, launcher, a); SafeTubeSync(launcher); }   // shows its own tube models, mirrors to the twin
      continue;
    }
    Item hidden; hidden.name = name; hidden.retired = true;            // stands in for a torpedo that is gone, so later positions keep their place
    const Item* use = item && !item->retired ? item : &hidden;
    void* src = b != byBase.end() ? b->second : nullptr;
    if (!src) {
      static std::set<std::string> told;
      if (item && told.insert(name + "|" + family).second) Log("torpedo '%s': base '%s' not found in slot '%s'; a hidden placeholder keeps its position", name.c_str(), item->base.c_str(), family.c_str());
      use = &hidden; src = firstReal;
    }
    void* t2 = MakeTier(src, *use);
    if (!t2 && use != &hidden) { use = &hidden; t2 = MakeTier(firstReal, hidden); }
    if (!t2 || !ListAdd(list, t2)) { Log("torpedo slot '%s': could not add '%s'; the torpedoes after it are left out so no saved position points at the wrong one", family.c_str(), name.c_str()); break; }
    at.emplace(name, ListCount(list) - 1);
    added++;
    if (!use->retired) listIt(name, ListCount(list) - 1);
  }
  if (added) DLog("torpedo slot %p ('%s'): %d added, %d choices now", launcher, family.c_str(), added, ListCount(list));
  if (realized) Log("torpedo slot '%s': %d hidden placeholders became the real torpedoes", family.c_str(), realized);
}
static void SafeInjectTorp(void* launcher) {
  if (g_torpInjectFaults < 0) return;
  __try { InjectTorpedoes(launcher); }
  __except (FaultFilter("found torpedoes (loadout)", GetExceptionInformation())) { g_torpInjectFaults = -1; }
}
static std::vector<void*> TorpLaunchers() {                          // the register, then both boats' launchers
  std::vector<void*> out;
  if (!g_tp.ok) return out;
  auto add = [&](void* l) { if (l && std::find(out.begin(), out.end(), l) == out.end()) out.push_back(l); };
  void* reg = nullptr; il2cpp_field_static_get_value(g_tp.regField, &reg);
  for (int i = 0; i < ListCount(reg); i++) add(ListAt(reg, i));
  for (void* craft : { g_ccm ? *(void**)((char*)g_ccm + g_offSelCraft) : nullptr, PlayerCraft() }) for (void* l : LaunchersOf(craft)) add(l);
  return out;
}
static void InjectTorpBoats() { for (void* l : TorpLaunchers()) SafeInjectTorp(l); }
static int TorpUnlist(const std::string& name) {                     // a torpedo scrapped on the open loadout screen leaves the lists
  int removed = 0;
  for (void* l : TorpLaunchers()) {
    void* order = *(void**)((char*)l + g_offOrder);
    void* entries = order && g_torpOrders.count(order) ? *(void**)((char*)order + g_offEntries) : nullptr;
    for (int j = ListCount(entries) - 1; j >= 0; j--) {
      void* e = ListAt(entries, j);
      if (e && name == Str(*(void**)((char*)e + g_offEName)) && ListRemoveAt(entries, j)) removed++;
    }
  }
  return removed;
}
static void* TorpSprite(const std::string& base) {                  // the stock torpedo's picture, from any launcher that has it
  SpriteEntry& e = g_baseSprites["torpedo|" + base];
  if (e.sprite || !g_tp.ok || (e.tried && GetTickCount() - e.tried < 3000)) return e.sprite;
  e.tried = GetTickCount();
  for (void* l : TorpLaunchers()) {
    void* list = *(void**)((char*)l + g_tp.lTorpedos);
    for (int i = 0; i < ListCount(list); i++) {
      void* t = ListAt(list, i); if (!t || TierBase(t) != base) continue;
      void* ty = TierType(t); void* s = ty ? *(void**)((char*)ty + g_tp.ySprite) : nullptr;
      if (s) { e.sprite = s; Pin(s); return s; }
    }
  }
  return nullptr;
}

// The found torpedo a launcher has selected ("" for none, a stock one or a hidden placeholder).
static std::string TorpSelectedItem(void* l) {
  void* list = *(void**)((char*)l + g_tp.lTorpedos);
  int idx = *(int*)((char*)l + g_tp.lSelIndex);
  void* tier = idx > 0 && idx < ListCount(list) ? ListAt(list, idx) : nullptr;
  if (!tier || !TierBase(tier).empty()) return "";
  return IsHiddenTier(tier) ? std::string(kHiddenTag) + TierName(tier) : TierName(tier);
}
static void TorpEquipped(void* craft, std::vector<std::string>& out) {   // a launcher and its mirror count as one slot
  for (void* l : LaunchersOf(craft)) {
    void* sym = *(void**)((char*)l + g_tp.lSymmetric);
    if (sym && sym < l) continue;
    std::string n = TorpSelectedItem(l);
    if (!n.empty() && n.compare(0, strlen(kHiddenTag), kHiddenTag) != 0) out.push_back(n);
  }
}
// On the loadout screen: a worn-out or scrapped found torpedo (or the hidden placeholder that took its place after a
// restart) is taken out of its launcher, as UnequipRetired does for guns.
static void TorpUnequipRetired(void* craft) {
  for (void* l : LaunchersOf(craft)) {
    std::string n = TorpSelectedItem(l);
    if (n.empty()) continue;
    bool hidden = n.compare(0, strlen(kHiddenTag), kHiddenTag) == 0;
    const Item* it = hidden ? nullptr : FindItem(n);
    if (!hidden && it && !it->retired) continue;
    int zero = 0; void* a[1] = { &zero };
    Invoke(g_tp.selectMI, l, a);
    SafeTubeSync(l);                                                   // a direct call: the hook does not see it
    Log("emptied torpedo slot %p: '%s' is worn out or scrapped", l, hidden ? n.c_str() + strlen(kHiddenTag) : n.c_str());
  }
}

typedef void (*fn_selIdx)(void*, int, void*);
static void (*g_torpAwakeOrig)(void*, void*) = nullptr;
static fn_selIdx g_torpSelectOrig = nullptr;
static void Hooked_TorpAwake(void* self, void* method) {
  g_torpAwakeOrig(self, method);
  if (g_ready) SafeInjectTorp(self);
}
static void Hooked_TorpSelectIndex(void* self, int index, void* method) {
  if (g_ready && g_tp.ok && g_torpInjectFaults >= 0) {
    __try {
      int n = ListCount(*(void**)((char*)self + g_tp.lTorpedos));
      if (index > 0 && index >= n) { DLog("torpedo selectIndex %d beyond %d choices; adding the found torpedoes first", index, n); InjectTorpedoes(self); }
    } __except (FaultFilter("found torpedoes (saved choice)", GetExceptionInformation())) { g_torpInjectFaults = -1; }
  }
  g_torpSelectOrig(self, index, method);
  SafeTubeSync(self);                                                  // the tube models show the new choice's look
}

// ---- found torpedoes in the water: their type, wear, homing ----------------------------------------------------------
struct TorpRun {                                                       // one launch of a pooled Torpedo
  int code = -1; std::string item;                                    // projectileCode of the launch; the found torpedo it was ("" = stock)
  double seek = 0; int chomp = 0; bool player = false, ended = false, exploded = false, gulped = false;
  int look = 0;                                                       // the model shown in its place: TorpLook kinds (0 = its own)
  int owner = -1;                                                     // another player's found torpedo (a copy here): their ClientId
  void* target = nullptr; float turned = 0, t0 = 0, waterY = 0, jaw = 0; bool waterSet = false; V3 last = {}, heading = { 0, 0, 1 };
  float spin = 0;                                                     // signed turning since the lock: a missed torpedo circling
  float closeTurn = 0;                                                // unsigned turning within 80 m of it: passes back and forth
  float closest = 1e9f, path = 0, range = 0, launchT = 0; bool gaveUp = false, reported = false, pathSet = false; std::string targetName;
  void* handle = nullptr;                                             // a found torpedo's run pins its Torpedo (read in later frames)
  // of the Dolphin: seconds between leaps (0 = none), when the next may start, the leap under way (start < 0: none) and its
  // pose this frame: height above the rest pose and nose-up pitch
  float leap = 0, leapAt = 0, leapStart = -1, leapDur = 0, leapAmp = 0, leapY = 0, leapPitch = 0, speed = 0; int leaps = 0; bool leapOut = false;
  // Flying Fish (fx skim): the model rides the surface in skips; leapStart/leapDur/leapAmp hold the skip under way
  // (leapStart < 0: gliding), skimGlide when the glide ends, skips the count
  float skim = 0, skimGlide = 0; int skips = 0;
  float tailAmp = 0;                                                  // the flying fish look: how far its tail beats (radians)
  // diving on a submerged target (TorpDepth): the target's height and flat distance this step, the item's own running
  // depth, the depth now and the deepest
  float aimY = 0, aimD = 1e9f, baseDepth = 0, depth = 0, maxDepth = 0; void* itemType = nullptr; bool dived = false, diveFailed = false;
  int util = 0;                                                       // utility gear: 1 smoke pot, 2 life raft, 3 Dave (UtilLanded)
  bool dud = false;                                                   // fx dud rolled on this launch: it never goes off (TorpDudRoll)
};
static std::unordered_map<void*, TorpRun> g_torpRuns;                 // Torpedo -> its latest launch
struct TorpStat { int launched = 0, exploded = 0, locks = 0, leaps = 0, skips = 0, dives = 0, duds = 0; float topSpeed = 0; };
static std::map<std::string, TorpStat> g_torpStats;                  // this battle, by found torpedo
static std::set<std::pair<std::string, void*>> g_torpPaid;           // (torpedo, slot) pairs that spent a mission this battle
static int g_torpLogs = 0, g_torpCoopMiss = 0;                       // g_torpCoopMiss: launches from other players' launchers that were not theirs
static int g_torpDudLogs = 0;                                         // dud lines this battle: the first few in full, the rest debug only
static float TorpTime() { void* r = Invoke(g_tp.timeNow, nullptr); return r ? *(float*)((char*)r + 0x10) : 0; }
static void TorpSpendUse(const std::string& name, void* launcher) {
  if (g_usesBase <= 0) return;
  void* sym = *(void**)((char*)launcher + g_tp.lSymmetric);            // a launcher and its mirror are one slot on the loadout screen
  if (!g_torpPaid.insert({ name, sym && sym < launcher ? sym : launcher }).second) return;
  Item* it = ItemByName(name);
  if (!it || !it->fromInventory || it->retired || it->uses <= 0) return;
  it->uses--; g_battleSpent[it->name]++;
  if (it->uses <= 0) {
    int t = 0, got = ScrapYield(it->rarity, &t);
    it->uses = 0; it->retired = true; g_mat[t] += got;
    Log("worn out: '%s' (%s) fired its last mission, +%d %s", it->name.c_str(), it->rarity.empty() ? "Common" : it->rarity.c_str(), got, kMatName[t]);
    ShowMessage("Worn out after this battle: " + it->name + " (+" + std::to_string(got) + " " + kMatName[t] + ")");
  }
  else Log("'%s' fired, so this battle costs it a mission (%d slot(s) fired it this battle, one mission each); %d mission(s) left", it->name.c_str(), g_battleSpent[it->name], it->uses);
  SaveInventory();
}
static void TorpRunReport(TorpRun& r);
// The model a found torpedo shows in place of its own (TorpLook kinds): 1 Chomper, 2 Chompette, 3 the commando, 4 the
// flying fish, 0 its own.
static int TorpLookKind(const Item& it) {
  int chomp = (int)ItemFx(it, "chomp");
  if (ItemFx(it, "smoke") > 0) return 5;                              // utility gear: the smoke pot or the packed raft
  if (ItemFx(it, "raft") > 0 || ItemFx(it, "dave") > 0) return 6;   // Dave rides in the rack as a packed bundle too
  return chomp > 0 ? (chomp >= 2 ? 2 : 1) : ItemFx(it, "crawl") > 0 ? 3 : ItemFx(it, "skim") > 0 ? 4 : 0;
}
// The launcher look for utility gear (UtilKind): 5 the smoke pot, 6 the packed raft, which Dave uses too.
static int UtilLook(int u) { return u == 3 ? 6 : u + 4; }
// Utility gear from one launcher: 1 smoke pot, 2 life raft, 3 Dave, 0 none. A split load (both set) goes by the launcher's side:
// its name ends in L (Depth Charge Launcher L, Sub Chaser Rear Launcher L, Side Tube FL/RL) for rafts, R for smoke.
static int UtilKind(const Item& it, void* launcher) {
  if (ItemFx(it, "dave") > 0) return 3;                               // Dave: never in a split load
  bool sm = ItemFx(it, "smoke") > 0, rf = ItemFx(it, "raft") > 0;
  if (!sm || !rf) return sm ? 1 : rf ? 2 : 0;
  // the PT-658 (Higgins 78) names its launchers 'Forward Left', 'Forward Right', 'Rear Left', 'Rear Right'
  std::string n = launcher ? Trim(NameOf(launcher)) : "";
  bool right = n.size() >= 5 && _stricmp(n.c_str() + n.size() - 5, "Right") == 0;
  return right || (!n.empty() && n.back() == 'R') ? 1 : 2;
}
// fx dud (0..1, the Mk 14 Magnetic), rolled once per launch. A dud runs on a copy of its type with detectionDistance 0:
// Torpedo.FixedUpdate casts for a hit only while that is above 0, so it runs under the ship and on to the end of its
// range without going off. A damage-0 copy would still explode, splash and all, as a hit that does nothing.
static std::unordered_map<void*, std::pair<void*, void*>> g_torpDudTypes;   // a found torpedo's type -> its dud copy, GC handle
static bool g_torpDudOff = false;                                     // copying or editing a type failed once: no more duds
static void* TorpDudType(void* type) {
  auto& e = g_torpDudTypes[type];
  auto src = g_torpOfType.find(type), dud = e.first ? g_torpOfType.find(e.first) : g_torpOfType.end();
  bool same = src == g_torpOfType.end() || (dud != g_torpOfType.end() && dud->second == src->second);   // a reused address is another torpedo
  if (Alive(e.first) && same) return e.first;
  if (e.first) { g_torpOfType.erase(e.first); Unpin(e.second); e = {}; }
  void* d = CloneObject(type);
  Op off; off.field = "detectionDistance"; off.mul = false; off.value = 0;
  if (!d || !ApplyOp(d, off)) { g_torpDudOff = true; Log("torpedo duds: MISSING PIECES (TorpedoType copy %p or its detectionDistance); no torpedo is ever a dud", d); return nullptr; }
  if (g_tp.hideFlags) { int keep = 32; void* a[1] = { &keep }; Invoke(g_tp.hideFlags, d, a); }   // DontUnloadUnusedAsset
  e = { d, Pin(d) };
  auto c = g_torpOfType.find(type); if (c != g_torpOfType.end()) g_torpOfType[d] = c->second;   // still that found torpedo, same stock type
  return d;
}
static void TorpSize(void* t, float scale, const std::string& item);  // wm_torp_looks.h: fx torpScale, the model's size
static void TorpDudRoll(void* t, TorpRun& r, const Item& it, int owner) {
  if (g_torpDudOff || !r.itemType || owner >= 0) return;              // another player's torpedo: their machine rolls it
  Item buf; const Item& x = ExpandBuiltFrom(it, &buf);                 // a found Mk 14 Magnetic's chance is on its side-grade
  double chance = ItemFx(x, "dud");
  if (chance <= 0 || Rand01() >= chance) return;
  void* d = TorpDudType(r.itemType); if (!d) return;
  SetRef(t, (char*)t + g_tp.pType, d);                                 // before the game's first FixedUpdate of the launch
  r.itemType = d; r.dud = true;                                       // TorpDepth dives and surfaces it on the dud copy
  int n = ++g_torpStats[r.item].duds;
  if (g_torpDudLogs++ < 5) Log("torpedoes: '%s' is a dud (%.0f%% chance, %d this battle): it will not go off", r.item.c_str(), chance * 100, n);
  else DLog("torpedoes: '%s' is a dud (%.0f%% chance, %d this battle)", r.item.c_str(), chance * 100, n);
}
// Before the game's FixedUpdate: a new launch (a new projectileCode) gets its tier's type.
static void TorpBegin(void* t) {
  int code = *(int*)((char*)t + g_tp.pCode);
  auto f = g_torpRuns.find(t);
  if (f != g_torpRuns.end() && f->second.code == code) return;
  if (f != g_torpRuns.end() && !f->second.ended) { f->second.ended = true; TorpRunReport(f->second); }   // relaunched before the frame's tick saw it end
  TorpRun& r = g_torpRuns[t];
  Unpin(r.handle);
  r = TorpRun(); r.code = code;
  void* tier = *(void**)((char*)t + g_tp.pTier);
  void* cur = *(void**)((char*)t + g_tp.pType);
  void* want = TierType(tier);
  if (!want) { auto c = g_torpOfType.find(cur); if (c != g_torpOfType.end()) want = c->second->stock; }   // no tier: never one of ours
  auto c = want ? g_torpOfType.find(want) : g_torpOfType.end();
  const Item* it = ItemOfTorpType(want);
  int owner = c != g_torpOfType.end() ? c->second->owner : -1;         // another player's (their machine judged its wear)
  // a found torpedo gone from the inventory, or worn out in an earlier battle, fires as its stock torpedo until the
  // loadout screen takes it out of the launcher (TorpUnequipRetired); one worn out this battle keeps going, as guns do
  if (owner < 0 && c != g_torpOfType.end() && !c->second->item.empty() && (!it || (it->retired && !std::any_of(g_torpPaid.begin(), g_torpPaid.end(),
      [&](const std::pair<std::string, void*>& p) { return p.first == it->name; })))) { want = c->second->stock; it = nullptr; }
  void* launcher = *(void**)((char*)t + g_tp.pFrom);
  void* ent = launcher ? *(void**)((char*)launcher + g_tp.lEntity) : nullptr;
  r.player = ent && ent == PlayerCraft();
  if (want && want != cur) {
    SetRef(t, (char*)t + g_tp.pType, want);
    *(float*)((char*)t + g_tp.pDetonate) = *(float*)((char*)want + g_tp.yDetonation);
    *(float*)((char*)t + g_tp.pArm) = *(float*)((char*)want + g_tp.yArming);
    // TorpedoLauncher.launch already added the pooled type's damage to the battle's torpedo damage stat
    // (WorldEngagementManager.damageOutputTorpedoes) for the player's boat; correct it to this launch's type
    static void* fDmg = (void*)-1;
    if (fDmg == (void*)-1) { void* k = FindClass("Core.World", "WorldEngagementManager"); fDmg = k ? il2cpp_class_get_field_from_name(k, "damageOutputTorpedoes") : nullptr; }
    static void (*setStatic)(void*, void*) = (void (*)(void*, void*))GetProcAddress(g_ga, "il2cpp_field_static_set_value");
    if (r.player && fDmg && setStatic && cur) {
      float v = 0; il2cpp_field_static_get_value(fDmg, &v);
      v += (float)(NumField(want, "damage") - NumField(cur, "damage"));
      setStatic(fDmg, &v);
    }
  }
  if (owner < 0 && launcher && !g_remoteLaunchers.empty() && tier && TierBase(tier).empty()) {   // fired before their list was built here
    auto rl = g_remoteLaunchers.find(launcher);
    if (rl != g_remoteLaunchers.end() && g_torpCoopMiss++ < 10)
      Log("co-op torpedoes: player %d's launcher %d fired '%s' (position %d) here, not one of their found torpedoes", rl->second.owner, rl->second.index,
          TierName(tier).c_str(), *(int*)((char*)launcher + g_tp.lSelIndex));
  }
  if (!it) { TorpSize(t, 1.0f, ""); return; }                          // a stock torpedo, or a hidden placeholder's: stock size
  r.itemType = *(void**)((char*)t + g_tp.pType);
  if (r.itemType && g_tp.yIdle >= 0) r.baseDepth = r.depth = *(float*)((char*)r.itemType + g_tp.yIdle);
  // a type that cannot be spotted (Sneaking Eel): the game may have rolled a spot on the pooled torpedo's old type between
  // its launch and this first FixedUpdate, and a spot is permanent, so it is forgotten
  if (g_tp.pDetected >= 0 && NumField(*(void**)((char*)t + g_tp.pType), "detectionByEntityChanceMultiplier") <= 0) {
    void* set = *(void**)((char*)t + g_tp.pDetected);
    static void* clear = nullptr;
    if (set && !clear) clear = FindMethod(il2cpp_object_get_class(set), "Clear", 0);
    if (set && clear) Invoke(clear, set);
  }
  r.item = it->name;
  if (owner >= 0) { r.owner = owner; r.item = it->name + " (player " + std::to_string(owner) + "'s)"; g_cc.torpCopies++; }   // its own stats and log lines
  r.handle = Pin(t);
  r.seek = ItemFx(*it, "torpSeek"); r.chomp = (int)ItemFx(*it, "chomp");
  r.look = TorpLookKind(*it);
  r.t0 = r.launchT = TorpTime();
  r.leap = (float)ItemFx(*it, "leap");
  if (r.leap > 0) { if (r.leap < 2) r.leap = 2; r.leapAt = r.launchT + 2.5f + 0.3f * (float)(code & 7); }   // launches of a salvo leap apart
  r.skim = (float)ItemFx(*it, "skim");
  if (r.skim > 0) r.leap = 0;                                          // a skimmer never dives for a dolphin leap
  r.util = UtilKind(*it, launcher);
  if (r.util) r.look = UtilLook(r.util);                               // a split load: this side's pot or raft
  { float dd = (float)NumField(*(void**)((char*)t + g_tp.pType), "despawnDistance"); r.range = dd > 0 ? dd + 60 : 0; }   // the game repools it silently past this much path
  g_torpStats[r.item].launched++;
  TorpDudRoll(t, r, *it, owner);
  { Item buf; const Item& x = owner >= 0 ? *it : ExpandBuiltFrom(*it, &buf); float s = (float)ItemFx(x, "torpScale"); TorpSize(t, s > 0 ? s : 1.0f, r.item); }
  if (r.player && launcher && EngagementInstance()) TorpSpendUse(r.item, launcher);
  if (g_torpLogs++ < 10) {
    std::string how = r.seek > 0 ? "homing " + std::to_string((int)std::lround(r.seek)) + " deg/s" : "no homing";
    if (r.chomp) how += r.chomp >= 2 ? ", Chompette look" : ", Chomper look";
    if (r.look == 3) how += ", commando look, never spotted";
    if (r.leap > 0) how += ", leaps about every " + std::to_string((int)std::lround(r.leap)) + " s";
    if (r.skim > 0) how += r.look == 4 ? ", skims the surface as a flying fish" : ", skims the surface";
    if (r.dud) how += ", a dud";
    if (r.owner >= 0) Log("torpedoes: '%s' launched from player %d's boat (a copy here; their machine deals the damage) (%s)", r.item.c_str(), r.owner, how.c_str());
    else Log("torpedoes: '%s' launched from %s (%s)", r.item.c_str(), r.player ? "your boat" : "another boat", how.c_str());
  }
}
// One line per found torpedo when its run ends: how it ended, how far it got and how close it came.
static void TorpRunReport(TorpRun& r) {
  if (r.reported || r.item.empty()) return;
  r.reported = true;
  static int n = 0; if (n++ >= 60) return;
  float secs = TorpTime() - r.launchT;
  const char* how = r.exploded ? "went off" : (r.range > 0 && r.path > r.range - 80 ? "ran out of range" : "was gone without going off");
  std::string lp = r.leap > 0 ? ", leapt " + std::to_string(r.leaps) + " time(s)" : r.skim > 0 ? ", skipped " + std::to_string(r.skips) + " time(s)" : "";
  if (r.dived) lp += ", dived to " + std::to_string((int)std::lround(r.maxDepth)) + " m";
  if (r.dud) lp += ", a dud";
  if (r.targetName.empty()) Log("torpedoes: '%s' %s after %.1f s, %.0f m of path (range %.0f m)%s%s", r.item.c_str(), how, secs, r.path, r.range, r.seek > 0 ? "; it never found a ship" : "", lp.c_str());
  else Log("torpedoes: '%s' %s after %.1f s, %.0f m of path (range %.0f m); closest to '%s' %.0f m, turned %.0f deg%s%s", r.item.c_str(), how, secs, r.path, r.range,
           r.targetName.c_str(), r.closest, r.turned * 57.2958f, r.gaveUp ? ", had stopped homing (circling)" : "", lp.c_str());
}
// Is another found torpedo already chasing that ship? (a Chompette picks a different one)
static bool TorpTaken(void* ship, void* self) {
  for (const auto& kv : g_torpRuns) if (kv.first != self && kv.second.target == ship && !kv.second.ended && !kv.second.gaveUp && !kv.second.item.empty()) return true;
  return false;
}
static void TorpSteer(void* t, TorpRun& r, void* body, V3 p, V3 v, float dt) {
  if (*(float*)((char*)t + g_tp.pInWater) < 0.6f || r.gaveUp) return;   // after it is in the water, until it circles
  float sp = sqrtf(v.x * v.x + v.z * v.z); if (sp < 2) return;
  float hx = v.x / sp, hz = v.z / sp;
  const std::vector<EnemyAt>& es = LiveEnemiesAndSubs();             // a submarine below the water too: the torpedo dives (TorpDepth)
  const EnemyAt* tgt = nullptr;
  if (r.target) for (const EnemyAt& e : es) if (e.ent == r.target) { tgt = &e; break; }
  if (!tgt) {                                                          // the ship ahead: close to its heading, then near
    float best = 1e9f;
    for (const EnemyAt& e : es) {
      if (!e.ship || !TorpHittable(e.ent)) continue;                   // too small to be hit by a torpedo
      float dx = e.pos.x - p.x, dz = e.pos.z - p.z, d = sqrtf(dx * dx + dz * dz);
      if (d < 1 || d > 2000) continue;
      if (r.range > 0 && d * 1.3f > r.range - r.path) continue;      // too far for the path it has left (the game repools it silently)
      float ang = acosf((std::max)(-1.0f, (std::min)(1.0f, (dx * hx + dz * hz) / d))) * 57.2958f;
      if (ang > 75) continue;
      // distance counts double, so a ship nearby off to the side beats one at the edge of the range dead ahead
      float score = ang / 75 + 2 * d / 2000 + (r.chomp == 2 && TorpTaken(e.ent, t) ? 1.0f : 0.0f);
      if (score < best) { best = score; tgt = &e; }
    }
    if (!tgt) { r.aimD = 1e9f; return; }
    r.target = tgt->ent; g_torpStats[r.item].locks++; r.spin = 0; r.closeTurn = 0; r.closest = 1e9f; r.targetName = CrewClean(NameOf(tgt->ent));
    if (g_torpLogs++ < 20) {
      float dx = tgt->pos.x - p.x, dz = tgt->pos.z - p.z;
      Log("torpedoes: '%s' homes on '%s', %.0f m away%s", r.item.c_str(), r.targetName.c_str(), sqrtf(dx * dx + dz * dz),
          tgt->pos.y < -3 ? (" and " + std::to_string((int)std::lround(-tgt->pos.y)) + " m down").c_str() : "");
    }
  }
  const float* ev = (const float*)((char*)tgt->ent + g_tp.leVel);
  float dx = tgt->pos.x - p.x, dz = tgt->pos.z - p.z, d = sqrtf(dx * dx + dz * dz);
  if (d < r.closest) r.closest = d;
  r.aimY = tgt->pos.y; r.aimD = d;
  float lead = (std::min)(d / sp, 12.0f);
  float ax = dx + ev[0] * lead, az = dz + ev[2] * lead, al = sqrtf(ax * ax + az * az); if (al < 0.5f) return;
  ax /= al; az /= al;
  float a = atan2f(hx * az - hz * ax, hx * ax + hz * az);              // from the heading to the aim, in the x/z plane
  float rate = (float)r.seek * (d < 80 ? 4.0f : 1.0f) * 0.0174533f * dt;   // close in it turns 4x as hard, or it circles
  float step = a > rate ? rate : (a < -rate ? -rate : a);
  float cs = cosf(step), sn = sinf(step);
  V3 h = { hx * cs - hz * sn, 0, hx * sn + hz * cs };
  r.turned += fabsf(step); r.spin += step;
  if (d < 80) r.closeTurn += fabsf(step);
  // a circle and a half the same way round, or a full turn's worth of swinging back and forth up close (passes over
  // the ship cancel in the signed sum): it missed
  if (fabsf(r.spin) > 9.5f || r.closeTurn > 6.3f) {
    r.gaveUp = true;
    Log("torpedoes: '%s' stopped homing: it %s '%s' (closest %.0f m across; the ship %.0f m down, the torpedo %.0f m)", r.item.c_str(),
        fabsf(r.spin) > 9.5f ? "circled" : "kept passing", r.targetName.c_str(), r.closest, -r.aimY, -p.y);
  }
  *(bool*)((char*)t + g_tp.pTend) = false;                             // the game would pull it back onto the tube's line
  V3 nv = { h.x * sp, v.y, h.z * sp }; void* a1[1] = { &nv }; Invoke(g_tp.rbSetVel, body, a1);
  Q4 q = LookRot(h); void* a2[1] = { &q }; Invoke(g_tp.rbSetRot, body, a2);
  // the game's FixedUpdate set the TRANSFORM, and Unity copies a changed transform onto the body at the physics step,
  // which would undo the turn above: the transform gets the new facing too.
  if (g_tp.trSetRot) { void* tf = Invoke(m_getTransform, t); if (tf) { void* a3[1] = { &q }; Invoke(g_tp.trSetRot, tf, a3); } }
}
