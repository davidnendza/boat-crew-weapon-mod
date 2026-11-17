// Unity lookups resolved once (Setup) and object cloning (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- resolved once -------------------------------------------------------------------------------------------
struct V3 { float x, y, z; };
struct Q4 { float x, y, z, w; };
static void *m_instantiate = nullptr, *m_getName = nullptr, *m_setName = nullptr, *m_getTransform = nullptr, *m_getParent = nullptr, *m_setParent = nullptr;
static void *m_getLocalPos = nullptr, *m_setLocalPos = nullptr, *m_getLocalRot = nullptr, *m_setLocalRot = nullptr, *m_getLocalScale = nullptr, *m_setLocalScale = nullptr;
static bool g_setParentTwoArgs = false;
static void *m_getGameObject = nullptr, *m_setActive = nullptr;
static int g_offMounts = -1, g_offDetailables = -1, g_offOrder = -1, g_offDisplayName = -1;     // WeaponSystem / SelectableObject
static int g_offGunType = -1;                                                                    // GunMount
static int g_offPrice = -1, g_offDescription = -1, g_offOverrideTag = -1;                       // GunType
static int g_offProjTag = -1;                                                                    // ProjectileType.overrideTag
static int g_offAmmo[3] = { -1, -1, -1 };                                                        // GunType.Ammunition, AmmunitionAir, AmmunitionLightTarget
static int g_offEntries = -1, g_offEName = -1, g_offEIndex = -1, g_offEIndexAlt = -1, g_offEAlt = -1;   // SelectionOrder / Entry
static void* g_entryClass = nullptr;
static bool g_ready = false;

static bool Setup() {
  void* uobj = FindClass("UnityEngine", "Object");
  void* comp = FindClass("UnityEngine", "Component");
  void* tr = FindClass("UnityEngine", "Transform");
  void* ws = FindClass("Core", "WeaponSystem");
  void* gm = FindClass("Core", "GunMount");
  void* gt = FindClass("Core", "GunType");
  void* sel = FindClass("Core.Customization", "SelectableObject");
  void* so = FindClass("Core.Customization", "SelectionOrder");
  if (!uobj || !comp || !tr || !ws || !gm || !gt || !sel || !so) { Log("class lookup failed: Object %p Component %p Transform %p WeaponSystem %p GunMount %p GunType %p SelectableObject %p SelectionOrder %p", uobj, comp, tr, ws, gm, gt, sel, so); return false; }
  m_instantiate = FindMethod(uobj, "Instantiate", 1, "UnityEngine.Object");
  m_getName = FindMethod(uobj, "get_name", 0);
  m_setName = FindMethod(uobj, "set_name", 1);
  m_getTransform = FindMethod(comp, "get_transform", 0);
  m_getParent = FindMethod(tr, "get_parent", 0);
  m_getGameObject = FindMethod(comp, "get_gameObject", 0);
  { void* go = FindClass("UnityEngine", "GameObject"); m_setActive = go ? FindMethod(go, "SetActive", 1) : nullptr; }
  m_setParent = FindMethod(tr, "SetParent", 2, "UnityEngine.Transform", "System.Boolean");
  if (m_setParent) g_setParentTwoArgs = true; else m_setParent = FindMethod(tr, "SetParent", 1, "UnityEngine.Transform");
  m_getLocalPos = FindMethod(tr, "get_localPosition", 0); m_setLocalPos = FindMethod(tr, "set_localPosition", 1);
  m_getLocalRot = FindMethod(tr, "get_localRotation", 0); m_setLocalRot = FindMethod(tr, "set_localRotation", 1);
  m_getLocalScale = FindMethod(tr, "get_localScale", 0); m_setLocalScale = FindMethod(tr, "set_localScale", 1);
  if (!m_getGameObject || !m_setActive) { Log("GameObject.SetActive is missing"); return false; }
  if (!m_instantiate || !m_getName || !m_setName || !m_getTransform || !m_getParent || !m_setParent || !m_getLocalPos || !m_setLocalPos || !m_getLocalRot || !m_setLocalRot || !m_getLocalScale || !m_setLocalScale) { Log("a Unity method is missing (stripped?)"); return false; }

  void* fM = il2cpp_class_get_field_from_name(ws, "mounts");
  void* fD = il2cpp_class_get_field_from_name(ws, "detailables");
  void* fO = il2cpp_class_get_field_from_name(sel, "order");
  void* fDN = il2cpp_class_get_field_from_name(sel, "displayName");
  void* fG = il2cpp_class_get_field_from_name(gm, "gunType");
  void* fP = il2cpp_class_get_field_from_name(gt, "price");
  void* fDesc = il2cpp_class_get_field_from_name(gt, "description");
  void* fTag = il2cpp_class_get_field_from_name(gt, "overrideTag");
  { const char* an[3] = { "Ammunition", "AmmunitionAir", "AmmunitionLightTarget" };
    for (int i = 0; i < 3; i++) { void* f = il2cpp_class_get_field_from_name(gt, an[i]); if (!f) { Log("GunType.%s not found", an[i]); return false; } g_offAmmo[i] = (int)il2cpp_field_get_offset(f); } }
  { void* pt = FindClass("Core", "ProjectileType"); void* f = pt ? il2cpp_class_get_field_from_name(pt, "overrideTag") : nullptr; if (!f) { Log("ProjectileType.overrideTag not found"); return false; } g_offProjTag = (int)il2cpp_field_get_offset(f); }
  void* fE = il2cpp_class_get_field_from_name(so, "entries");
  void* soe = nullptr; { void* it = nullptr; void* nk; while ((nk = il2cpp_class_get_nested_types(so, &it))) if (!strcmp(il2cpp_class_get_name(nk), "Entry")) soe = nk; }
  void* fEN = soe ? il2cpp_class_get_field_from_name(soe, "name") : nullptr;
  void* fEI = soe ? il2cpp_class_get_field_from_name(soe, "index") : nullptr;
  void* fEIA = soe ? il2cpp_class_get_field_from_name(soe, "indexAlternate") : nullptr;
  void* fEA = soe ? il2cpp_class_get_field_from_name(soe, "alternateAccessible") : nullptr;
  if (!fM || !fO || !fG || !fP || !fDesc || !fTag || !fE || !fEN || !fEI || !fEIA || !fEA) { Log("a field lookup failed"); return false; }
  g_offMounts = (int)il2cpp_field_get_offset(fM);
  g_offDetailables = fD ? (int)il2cpp_field_get_offset(fD) : -1;
  g_offOrder = (int)il2cpp_field_get_offset(fO);
  g_offDisplayName = fDN ? (int)il2cpp_field_get_offset(fDN) : -1;
  g_offGunType = (int)il2cpp_field_get_offset(fG);
  g_offPrice = (int)il2cpp_field_get_offset(fP);
  g_offDescription = (int)il2cpp_field_get_offset(fDesc);
  g_offOverrideTag = (int)il2cpp_field_get_offset(fTag);
  g_offEntries = (int)il2cpp_field_get_offset(fE);
  g_offEName = (int)il2cpp_field_get_offset(fEN); g_offEIndex = (int)il2cpp_field_get_offset(fEI);
  g_offEIndexAlt = (int)il2cpp_field_get_offset(fEIA); g_offEAlt = (int)il2cpp_field_get_offset(fEA);
  g_entryClass = soe;
  DLog("setup ok: WeaponSystem.mounts 0x%X order 0x%X, GunMount.gunType 0x%X, GunType.price 0x%X", g_offMounts, g_offOrder, g_offGunType, g_offPrice);
  return true;
}

// ---- cloning -------------------------------------------------------------------------------------------------
static std::string NameOf(void* unityObject) {
  void* n = Invoke(m_getName, unityObject);
  return n ? Str(n) : std::string();
}

// The first time guns are added to a slot, its SelectionOrder is copied and Unity names the copy "<name>(Clone)". A slot
// family is always looked up by the stock name, or boats that were already filled would never be found again.
static std::string FamilyOf(void* order) {
  std::string n = NameOf(order);
  const std::string suffix = "(Clone)";
  while (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0) n.erase(n.size() - suffix.size());
  return n;
}

// Apply one numeric edit to a copied object (a GunType). Returns false if the field is missing or not a number.
static int g_editLogs = 0;                                          // log the first few edits so the effect is visible without flooding
static bool ApplyOp(void* obj, const Op& op) {
  void* cls = il2cpp_object_get_class(obj);
  // Inaccuracy (review 2026-11-06): GunMount.shotVelocity spreads a shot by clamp(100 - Accuracy x ..., 0, 100), so a
  // drawback scales that gap, not the score (mul.Accuracy=0.8 on a 95 gun was 4.8x the spread)
  bool spread = op.field == "Inaccuracy";
  void* f = il2cpp_class_get_field_from_name(cls, spread ? "Accuracy" : op.field.c_str());
  if (!f) { Log("  edit '%s': no such field on %s", op.field.c_str(), il2cpp_class_get_name(cls)); return false; }
  std::string tns = TypeName(il2cpp_field_get_type(f)); const char* tn = tns.c_str();
  char* p = (char*)obj + il2cpp_field_get_offset(f);
  bool log = g_editLogs < 60;
  if (spread) {
    if (!op.mul || strcmp(tn, "System.Int32")) { Log("  edit '%s': only mul. works, on an int Accuracy (%s)", op.field.c_str(), tn); return false; }
    int* x = (int*)p; int old = *x; double a = 100.0 - (100.0 - *x) * op.value;
    *x = (int)std::lround(a < 0 ? 0 : (a > 100 ? 100 : a));
    if (log) { g_editLogs++; DLog("  %s.Accuracy (spread x%.2f): %d -> %d", il2cpp_class_get_name(cls), op.value, old, *x); }
    return true;
  }
  if (!strcmp(tn, "System.Single")) { float* x = (float*)p; float old = *x; *x = op.mul ? (float)(*x * op.value) : (float)op.value; if (log) { g_editLogs++; DLog("  %s.%s: %g -> %g", il2cpp_class_get_name(cls), op.field.c_str(), old, *x); } return true; }
  if (!strcmp(tn, "System.Int32")) {
    int* x = (int*)p; int old = *x;
    if (!op.mul) *x = (int)std::lround(op.value);
    else if (*x != INT_MAX) {                                          // INT_MAX: infinite (crew weapons' MaxAmmo); a mul stays in range
      double d = *x * op.value + 0.5; *x = d >= 2147483647.0 ? INT_MAX : (d <= -2147483648.0 ? INT_MIN : (int)d);
    }
    if (log) { g_editLogs++; DLog("  %s.%s: %d -> %d", il2cpp_class_get_name(cls), op.field.c_str(), old, *x); } return true; }
  if (!strcmp(tn, "System.Boolean") && !op.mul) { *(bool*)p = op.value != 0; return true; }
  void* ecls = il2cpp_class_from_type(il2cpp_field_get_type(f));
  if (ecls && il2cpp_class_is_enum(ecls) && !op.mul && TypeName(il2cpp_class_enum_basetype(ecls)) == "System.Int32") {
    int* x = (int*)p; int old = *x; *x = (int)op.value;
    if (log) { g_editLogs++; DLog("  %s.%s: %d -> %d (%s)", il2cpp_class_get_name(cls), op.field.c_str(), old, *x, tn); }
    return true;
  }
  Log("  edit '%s': unsupported field type %s%s", op.field.c_str(), tn, op.mul ? " (mul only works on numbers)" : "");
  return false;
}

static double NumField(void* obj, const char* name) {                // a float or int field by name, 0 if there is none
  void* f = obj ? il2cpp_class_get_field_from_name(il2cpp_object_get_class(obj), name) : nullptr;
  if (!f) return 0;
  std::string tns = TypeName(il2cpp_field_get_type(f)); const char* tn = tns.c_str(); char* p = (char*)obj + il2cpp_field_get_offset(f);
  if (!strcmp(tn, "System.Single")) return *(float*)p;
  if (!strcmp(tn, "System.Int32")) return *(int*)p;
  return 0;
}

static void* CloneObject(void* src) { void* a[1] = { src }; return Invoke(m_instantiate, nullptr, a); }
static void SetName(void* obj, const char* name) { void* a[1] = { il2cpp_string_new(name) }; Invoke(m_setName, obj, a); }

// The first component of a class found under a GameObject (inactive ones included).
static void* m_getComponentInChildren = nullptr;
static void* FindChildComponent(void* comp, const char* ns, const char* cls) {
  if (!m_getComponentInChildren) { void* c = FindClass("UnityEngine", "Component"); m_getComponentInChildren = c ? FindMethod(c, "GetComponentInChildren", 2, "System.Type", "System.Boolean") : nullptr; }
  void* k = FindClass(ns, cls);
  if (!m_getComponentInChildren || !k) return nullptr;
  void* type = il2cpp_type_get_object(il2cpp_class_get_type(k));
  bool inactive = true;
  void* a[2] = { type, &inactive };
  return Invoke(m_getComponentInChildren, comp, a);
}

// A private List<ProjectileType> for a copied GunType whose list turned out to be shared with the original.
static void* CopyList(void* list) {
  void* cls = il2cpp_object_get_class(list);
  void* nl = il2cpp_object_new(cls);
  void* ctor = il2cpp_class_get_method_from_name(cls, ".ctor", 0);
  if (!nl || !ctor) return nullptr;
  void* exc = nullptr; il2cpp_runtime_invoke(ctor, nl, nullptr, &exc);
  if (exc) return nullptr;
  for (int i = 0; i < ListCount(list); i++) if (!ListAdd(nl, ListAt(list, i))) return nullptr;
  return nl;
}

// Give the copied gun its own copies of the projectile types in its ammo lists, then apply the proj.* edits to them.
// Unity normally gives a copied ScriptableObject its own list objects; if it ever shares them, editing would change the
// stock gun too, so a shared list is replaced with a private one first (logged).
static void RegisterPaletteShot(void* ptype, int slot);
static const int kPaletteN = 9;                                     // 2 rarity colors, 6 cosmetic, red for crew weapons (kCrewSlot)
static void* LiveRegister();
// A stock gun's GunType by name, from any live mount of it; enemy-only guns (the Japanese 25 mm, say) are found
// among the loaded GunType assets. withAmmo skips copies whose ammo list is empty (fragment_from needs a shell).
static bool HasAmmo(void* gt) { return g_offAmmo[0] >= 0 && ListCount(*(void**)((char*)gt + g_offAmmo[0])) > 0; }
static void* StockGunType(const std::string& name, bool withAmmo = false) {
  // a find is pinned and kept (GunTypes are assets); a miss is tried again after 30 s, in case it was not loaded yet
  struct Found { void* gt; DWORD at; };
  static std::map<std::string, Found> cache;
  std::string key = name + (withAmmo ? "#ammo" : "");
  auto c = cache.find(key);
  if (c != cache.end() && c->second.gt) return c->second.gt;
  void* found = nullptr;
  void* reg = LiveRegister();                                          // on a live mount first: no search of every object
  for (int i = 0; i < ListCount(reg) && !found; i++) {
    void* ws = ListAt(reg, i); if (!ws) continue;
    void* mounts = *(void**)((char*)ws + g_offMounts);
    for (int j = 0; j < ListCount(mounts) && !found; j++) {
      void* m = ListAt(mounts, j); void* gt = m ? *(void**)((char*)m + g_offGunType) : nullptr;
      if (gt && NameOf(gt) == name && (!withAmmo || HasAmmo(gt))) found = gt;
    }
  }
  if (!found && c != cache.end() && GetTickCount() - c->second.at < 30000) return nullptr;
  // a boat's slots register one by one as it spawns, so the gun may not be live yet: the loaded GunType assets
  // (Resources.FindObjectsOfTypeAll walks every object, so not more than once per name every 30 s)
  void* res = FindClass("UnityEngine", "Resources");
  void* findAll = !found && res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
  void* gtk = FindClass("Core", "GunType");
  if (findAll && gtk) {
    void* a[1] = { il2cpp_type_get_object(il2cpp_class_get_type(gtk)) };
    void* arr = Invoke(findAll, nullptr, a);
    int n = ArrLen(arr);
    for (int i = 0; i < n && !found; i++) {
      void* gt = ArrAt(arr, i);
      if (gt && NameOf(gt) == name && (!withAmmo || HasAmmo(gt))) found = gt;
    }
  }
  if (found) Pin(found);
  cache[key] = { found, GetTickCount() };
  return found;
}
// The first ammunition type of a stock gun (by its GunType name).
// The red crew-weapon copies (CrewTracerFor) sit in the stock crew GunTypes' ammo lists; anything copied from those
// lists (found guns, side-grades, fragments, Fireworks) is copied from the original, or it would inherit the crew style.
static std::unordered_map<void*, void*> g_crewOrigOf;              // red crew copy -> the ProjectileType it was copied from
static void* CrewSource(void* p) { auto f = g_crewOrigOf.find(p); return f == g_crewOrigOf.end() ? p : f->second; }
static void* StockAmmo(const std::string& base) {
  void* gt = StockGunType(base, true);
  return gt ? CrewSource(ListAt(*(void**)((char*)gt + g_offAmmo[0]), 0)) : nullptr;
}

// sound_from=<stock gun>: the copy fires with that gun's sounds: its shot, loop and distant events and the distance the
// distant one takes over (its recoil click too, when it has one; else the base gun's stays). The fire rate stays the
// copy's own, so the rhythm follows roundsPerMinute. The turret guns use it so a captured Type 96 sounds like the
// enemy's Type 96 rather than the Oerlikon it is built on (David 2026-10-28: "make the rat-a-tat-tat sound right").
// FMOD EventReference is a 16-byte GUID in a build (no path), so the events are copied as bytes.
static bool CopySounds(void* gt2, const Item& it) {
  static int offShot = -2, offLoop = -1, offRecoil = -1, offSwap = -1, offDistant = -1;
  if (offShot == -2) {
    void* k = FindClass("Core", "GunType");
    offShot = FieldOffset(k, "soundEvent"); offLoop = FieldOffset(k, "soundLoopEvent"); offRecoil = FieldOffset(k, "soundRecoilEvent");
    offSwap = FieldOffset(k, "distantSoundSwapDistance"); offDistant = FieldOffset(k, "soundEventDistant");
    if (offShot < 0 || offLoop - offShot != 16 || offRecoil - offLoop != 16 || offDistant < 0 || offSwap < 0) {
      Log("gun sounds: GunType sound fields not as expected (soundEvent +0x%X, loop +0x%X); sound_from is ignored", offShot, offLoop);
      offShot = -1;
    }
  }
  if (offShot < 0) return false;
  void* src = StockGunType(it.soundFrom);
  if (!src) {
    static std::set<std::string> told;                                 // once per gun: every boat build copies it again
    if (told.insert(it.name).second) Log("  '%s': sound_from '%s' is not a gun the game has loaded; it keeps its base gun's sounds", it.name.c_str(), it.soundFrom.c_str());
    return false;
  }
  static const char zero[16] = {};
  memcpy((char*)gt2 + offShot, (char*)src + offShot, 16);
  memcpy((char*)gt2 + offLoop, (char*)src + offLoop, 16);
  if (memcmp((char*)src + offRecoil, zero, 16) != 0) memcpy((char*)gt2 + offRecoil, (char*)src + offRecoil, 16);
  memcpy((char*)gt2 + offDistant, (char*)src + offDistant, 16);
  *(float*)((char*)gt2 + offSwap) = *(float*)((char*)src + offSwap);
  return true;
}
static void SetFragment(void* ptype, const Item& it) {
  static int offFrag = -2;
  if (offFrag == -2) offFrag = FieldOffset("Core", "ProjectileType", "fragment");
  void* frag = offFrag >= 0 ? StockAmmo(it.fragmentFrom) : nullptr;
  if (!frag || frag == ptype) { Log("  '%s': fragment_from '%s' not found on any live gun; no fragments", it.name.c_str(), it.fragmentFrom.c_str()); return; }
  SetRef(ptype, (char*)ptype + offFrag, frag);
  DLog("  '%s': shells burst into '%s'", it.name.c_str(), NameOf(frag).c_str());
}
static void CloneProjectiles(void* origGun, void* gun2, const Item& it) {
  std::vector<std::pair<void*, void*>> done;                         // source projectile -> its copy, so shared entries map to one copy
  // projectile_from=<stock gun>: each ammo entry is copied from that gun's list of the same kind (its first shell when that
  // list is empty) instead of the base gun's, then takes the item's proj.* edits like any other copy
  void* from = it.projectileFrom.empty() ? nullptr : StockGunType(it.projectileFrom, true);
  if (!it.projectileFrom.empty()) {
    static std::set<std::string> told;                                 // once per gun and outcome: every boat build copies it again
    if (told.insert(it.name + (from ? "+" : "-")).second) {
      if (from) Log("  '%s': projectile_from '%s' applied: fires copies of its '%s'", it.name.c_str(), it.projectileFrom.c_str(), NameOf(CrewSource(ListAt(*(void**)((char*)from + g_offAmmo[0]), 0))).c_str());
      else Log("  '%s': projectile_from '%s' MISSING: no loaded gun of that name with ammo; it fires its base gun's shots", it.name.c_str(), it.projectileFrom.c_str());
    }
  }
  for (int a = 0; a < 3; a++) {
    void* l2 = *(void**)((char*)gun2 + g_offAmmo[a]);
    void* l1 = *(void**)((char*)origGun + g_offAmmo[a]);
    if (!l2 || ListCount(l2) == 0) continue;
    if (l2 == l1) {
      void* own = CopyList(l2);
      if (!own) { Log("  ammo list %d is shared with the stock gun and could not be copied; projectile edits skipped", a); continue; }
      SetRef(gun2, (char*)gun2 + g_offAmmo[a], own); l2 = own;
      Log("  ammo list %d was shared with the stock gun; gave the copy its own list", a);
    }
    void* arr = *(void**)((char*)l2 + 0x10);
    for (int i = 0; i < ListCount(l2); i++) {
      void* src = CrewSource(ListAt(l2, i));
      if (from) {
        void* fl = *(void**)((char*)from + g_offAmmo[a]); int fn = ListCount(fl);
        void* rp = fn > 0 ? ListAt(fl, i % fn) : ListAt(*(void**)((char*)from + g_offAmmo[0]), 0);
        if (rp) src = CrewSource(rp);
      }
      if (!src) continue;
      void* p2 = nullptr;
      for (auto& d : done) if (d.first == src) p2 = d.second;
      if (!p2) {
        p2 = CloneObject(src);
        if (!p2) { Log("  could not copy projectile type"); continue; }
        std::string nm = NameOf(src) + " [" + it.name + "]";
        SetName(p2, nm.c_str());
        SetRef(p2, (char*)p2 + g_offProjTag, il2cpp_string_new(("WM_" + nm).c_str()));
        DLog("  projectile '%s' (ammo list %d)", nm.c_str(), a);
        // the howitzer's Ammunition list also holds '75 mm Howitzer round SMOKE' (read from the game data 2026-11-15): a
        // set damage (Pak 40 3000, RW 61 6000) is meant for the shell, so a smoke round keeps its own damage
        std::string lower = NameOf(src); for (char& ch : lower) ch = (char)tolower((unsigned char)ch);
        bool smoke = lower.find("smoke") != std::string::npos;
        for (const Op& op : it.ops) {
          if (!op.proj) continue;
          if (smoke && !op.mul && (op.field == "BaseDamage" || op.field == "SplashDamageBase")) { DLog("  '%s': proj.set.%s skipped on the smoke round", nm.c_str(), op.field.c_str()); continue; }
          ApplyOp(p2, op);
        }
        if (!it.fragmentFrom.empty()) SetFragment(p2, it);
        bool ownStyle = false; for (const Op& op : it.ops) if (op.proj && op.field == "style") ownStyle = true;
        int tc = (int)ItemFx(it, "tracerColor");
        int rq = RarityOf(it.rarity);                                    // Uncommon blue (0), Rare gold (1), Unique orange (5)
        int slot = tc >= 1 && tc <= kPaletteN - 3 ? 1 + tc : (g_tracerRarity && it.fromInventory ? (rq == 4 ? 6 : rq == 3 ? 5 : rq - 1) : -1);   // Unusual pink (6)
        if (!ownStyle) RegisterPaletteShot(p2, slot);
        done.push_back({ src, p2 });
      }
      SetRef(arr, (char*)arr + 0x20 + 8 * (size_t)i, p2);
    }
  }
}

// GunMount.seats (Seat[]): shortened in place by lowering the array length, so no array is allocated or written.
static int g_offSeats = -2;
static int SeatsOffset() {
  if (g_offSeats == -2) g_offSeats = FieldOffset("Core", "GunMount", "seats");
  return g_offSeats;
}
static int SeatCount(void* mount) {
  if (!mount || SeatsOffset() < 0) return -1;
  void* arr = *(void**)((char*)mount + g_offSeats);
  return ArrLen(arr);
}
static void RemoveSeats(void* mount, int n) {
  int have = SeatCount(mount);
  if (have <= 1 || n <= 0) { Log("  seatsRemoved: gun has %d seats, left alone", have); return; }
  int keep = have - n < 1 ? 1 : have - n;
  void* arr = *(void**)((char*)mount + g_offSeats);
  *(size_t*)((char*)arr + 0x18) = (size_t)keep;
  DLog("  seats: %d -> %d", have, keep);
}

