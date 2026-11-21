// paint schemes on found guns (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- paint schemes on found guns ----
// The player guns are drawn by segmentation shaders (Custom/StandardColor5, StandardColor3, LambertColor5): _MainTex is
// a mask whose five key colours pick _Color1.._Color5, so a gun's colours are material properties, not texture pixels.
// Custom/PbrBC (3-inch, 5-inch, Oerlikon, Thunderbolt) tints a baked albedo by mask region the same way. A painted gun
// wears copies of its materials with new _ColorN values, one copy per (stock material, scheme, slots), shared by every
// gun that wears it (a copy per gun would pile up). A material property block is not used: VisualReactor's damage
// material would carry it. Two game scripts would undo the paint:
// - VisualReactor snapshots each renderer's first material in Awake (fetchDefaults) and puts it back on enable,
//   highlight and repair, so every reactor under the mount (near and far LOD ones) takes a new snapshot afterwards;
// - CustomColorSync writes the boat's hull colour into _Color{targetColorIndex} x colorBrightness through a property
//   block on enable and at every hull pattern refresh. Its slot and brightness say which slots are the hull paint.
//   After the materials are painted, its renderer list keeps only the renderers that took no paint (never null: sync
//   reads it unchecked) and the block it left on the painted ones is cleared, so a gun or part that could not be painted
//   still follows the hull camo. The component stays: CustomPatternSlot keeps a list of them.
// fx.set.paint=<index into kPaint> is rolled with paint_chance (RollItem). Uniques are gold and the pink tier pink, from
// their rarity: RefreshUniques rebuilds a Unique's ops at every launch, so a stored op would not last.
struct PaintScheme { const char* name; float rgb[3]; float metallic; bool rolled; };   // rgb: on the scale of the stock _ColorN values (mats.json), as Material.SetColor takes them
static const PaintScheme kPaint[] = {                                 // APPEND ONLY: the index is saved in inventory.ini and sent in co-op
  { "none", { 0, 0, 0 }, -1, false },
  { "Haze Grey", { 0.47f, 0.50f, 0.53f }, -1, true },                 // 5-H, the 1941 overall grey
  { "Ocean Grey", { 0.40f, 0.44f, 0.47f }, -1, true },                // 5-O
  { "Measure 21 Navy Blue", { 0.16f, 0.20f, 0.28f }, -1, true },      // 5-N, the Pacific 1942-43 dark scheme
  { "Deck Blue", { 0.22f, 0.27f, 0.33f }, -1, true },                 // 20-B
  { "Pale Grey", { 0.70f, 0.72f, 0.74f }, -1, true },                 // 5-P
  { "Olive Drab", { 0.24f, 0.27f, 0.16f }, -1, true },                // Army guns
  { "Desert Sand", { 0.62f, 0.52f, 0.36f }, -1, true },
  { "Flat Black", { 0.05f, 0.05f, 0.06f }, -1, true },
  { "Kure Grey", { 0.36f, 0.38f, 0.40f }, -1, true },                 // IJN arsenal grey
  { "Grey-Green", { 0.38f, 0.42f, 0.36f }, -1, true },                // captured Japanese
  { "Rust Primer", { 0.42f, 0.16f, 0.10f }, -1, true },               // red oxide, never top-coated
  { "Arctic White", { 0.88f, 0.90f, 0.92f }, -1, true },
  { "Gold", { 0.85f, 0.65f, 0.20f }, 0.8f, false },                   // every Unique, from rarity
  { "Pink", { 1.0f, 0.37f, 0.78f }, -1, false },                      // the pink tier, from rarity (RarityHex's colour)
};
static const int kPaintN = (int)(sizeof kPaint / sizeof kPaint[0]);
static const int kPaintGold = 13, kPaintPink = 14;
// Which _ColorN of a stock material are its paint, from the stock colours (mats.json): 1 = the scheme colour, below 1 = a
// shade part kept as much darker than the main part as on the stock gun (shade luminance / main luminance), 0 = kept
// (wood, white details, black, red-brown). M4 and M45 swap their mask keys, so this goes by slot number. A segmentation
// material not listed paints _Color1 only (Dark MG Metal has no mask: only _Color1 shows on it).
struct PaintSlots { const char* mat; float f[5]; };
static const PaintSlots kPaintSlots[] = {
  { "Bofors Gun Blue",     { 1, 1, 0.88f, 0.78f, 0 } },             // 3 the grey accent, 5 black
  { "M3 37 MM AT",         { 1, 0, 0, 0.70f, 0.70f } },             // 2, 3 greys
  { "M1A1 75 MM Howitzer", { 1, 0, 0, 0.51f, 0 } },
  { "M4 37 MM Autocannon", { 1, 1, 0, 0.60f, 0 } },
  { "Rocket Launcher",     { 1, 1, 0, 1, 0 } },                     // 3 grey, 5 white
  { "M45 Meat Chopper",    { 1, 0.75f, 0, 0, 0 } },                 // 3 white, 4 red-brown
};
// A PbrBC renderer no hull sync names keeps its baked colour (the Oerlikon's barrels and magazine): only the sync slots are paint.
struct PaintApi { void *setColor, *setFloat, *setMats, *setBlock, *mpbClass, *mpbCtor, *inChildren, *vrType, *vrFetch, *ccsType, *rendElem;
                  int ccsRenderers, ccsIndex, ccsBright; void* (*arrayNew)(void*, size_t); bool ok; };
static PaintApi g_papi = {};
static bool g_paintOff = false;                                       // a failed lookup or a fault: no paint until restart
static int g_paintGuns = 0, g_paintCopies = 0, g_paintGunsLogged = -1;
struct PaintCopy { void* mat; void* handle; void* stock; void* stockHandle; };   // the stock material is pinned too: the key and g_paintStock hold its pointer
static std::unordered_map<std::string, PaintCopy> g_paintCopy;        // "stock name|stock|scheme|slots" -> its painted copy
static std::unordered_map<void*, void*> g_paintStock;                 // painted copy -> its stock material (a copy is never copied again)
static bool Alive(void* o);
static bool PaintApiReady() {
  static bool tried = false;
  if (tried) return g_papi.ok;
  tried = true;
  ModelApiReady();                                                    // fills the renderer lookups (paint needs them, not the OBJ loader)
  PaintApi& a = g_papi;
  void* mc = FindClass("UnityEngine", "Material");
  a.setColor = mc ? FindMethod(mc, "SetColor", 2, "System.String", "UnityEngine.Color") : nullptr;
  a.setFloat = mc ? FindMethod(mc, "SetFloat", 2, "System.String", "System.Single") : nullptr;   // optional: Gold's sheen
  void* rk = FindClass("UnityEngine", "Renderer");
  a.setMats = rk ? FindMethod(rk, "set_sharedMaterials", 1) : nullptr;
  a.setBlock = rk ? FindMethod(rk, "SetPropertyBlock", 1) : nullptr;
  a.mpbClass = FindClass("UnityEngine", "MaterialPropertyBlock"); a.mpbCtor = a.mpbClass ? FindMethod(a.mpbClass, ".ctor", 0) : nullptr;
  void* go = FindClass("UnityEngine", "GameObject"); a.inChildren = go ? FindMethod(go, "GetComponentsInternal", 6) : nullptr;   // GetComponentsInChildren is stripped
  void* vr = FindClass("Core", "VisualReactor");
  a.vrType = vr ? il2cpp_type_get_object(il2cpp_class_get_type(vr)) : nullptr; a.vrFetch = vr ? FindMethod(vr, "fetchDefaults", 1) : nullptr;
  void* cc = FindClass("Core.Customization", "CustomColorSync");
  a.ccsType = cc ? il2cpp_type_get_object(il2cpp_class_get_type(cc)) : nullptr;
  a.ccsRenderers = FieldOffset(cc, "renderers"); a.ccsIndex = FieldOffset(cc, "targetColorIndex"); a.ccsBright = FieldOffset(cc, "colorBrightness");
  void* fr = cc ? il2cpp_class_get_field_from_name(cc, "renderers") : nullptr;   // its element class, for the empty array
  auto fieldGetType = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_field_get_type");
  auto classFromType = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_from_type");
  auto elementClass = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_get_element_class");
  void* arrK = fr && fieldGetType && classFromType ? classFromType(fieldGetType(fr)) : nullptr;
  a.rendElem = arrK && elementClass ? elementClass(arrK) : nullptr;
  a.arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  std::string missing;
  const std::pair<const char*, const void*> need[] = { { "Material.SetColor", a.setColor }, { "Renderer.set_sharedMaterials", a.setMats },
    { "Renderer.get_sharedMaterials", g_mapi.getMats }, { "Renderer.SetPropertyBlock", a.setBlock }, { "GameObject.GetComponentsInternal", a.inChildren },
    { "VisualReactor", a.vrType }, { "VisualReactor.fetchDefaults", a.vrFetch }, { "CustomColorSync", a.ccsType }, { "its renderers' element class", a.rendElem },
    { "il2cpp_array_new", (const void*)a.arrayNew }, { "Component.GetComponent", g_mapi.getComponent }, { "Renderer type", g_mapi.rendererType },
    { "Transform.GetChild", g_mapi.getChild }, { "Material.get_shader", g_mapi.getShader }, { "Component.get_gameObject", m_getGameObject } };
  for (auto& n : need) if (!n.second) missing += std::string(" ") + n.first;
  if (a.ccsRenderers < 0 || a.ccsIndex < 0 || a.ccsBright < 0) missing += " CustomColorSync.renderers/targetColorIndex/colorBrightness";
  a.ok = missing.empty();
  if (a.ok) Log("paint: ready (CustomColorSync renderers +0x%X, slot +0x%X, brightness +0x%X; SetFloat %s)", a.ccsRenderers, a.ccsIndex, a.ccsBright, a.setFloat ? "found" : "missing, Gold has no sheen");
  else { Log("paint: missing%s; paint schemes are off", missing.c_str()); g_paintOff = true; }
  return a.ok;
}
static int PaintOf(const Item& it) {                                  // the scheme a gun wears (0: stock colours)
  if (it.rarity == "Unique") return kPaintGold;
  if (it.rarity == "Unusual") return kPaintPink;
  int p = (int)ItemFx(it, "paint"); return p > 0 && p < kPaintN ? p : 0;
}
// Every component of a type under a mount, inactive ones too (the copy is switched off while it is built).
static std::vector<void*> PaintComponents(void* mount, void* type) {
  std::vector<void*> out;
  void* go = Invoke(m_getGameObject, mount);
  bool typed = true, recursive = true, inactive = true, reverse = false; void* a[6] = { type, &typed, &recursive, &inactive, &reverse, nullptr };
  void* arr = go ? Invoke(g_papi.inChildren, go, a) : nullptr; int n = ArrLen(arr);
  for (int i = 0; i < n && i < 256; i++) if (void* c = ArrAt(arr, i)) out.push_back(c);
  return out;
}
// Clears the hull colour a CustomColorSync left on a renderer: SetPropertyBlock(null) is Unity's 'clear'; an empty
// block if this build throws on null.
static void PaintClearBlock(void* r) {
  static int way = 0;                                                 // 0 untried, 1 null works, 2 an empty block
  void* exc = nullptr;
  if (way != 2) { void* a[1] = { nullptr }; il2cpp_runtime_invoke(g_papi.setBlock, r, a, &exc); if (!exc) { if (!way) { way = 1; DLog("paint: SetPropertyBlock(null) clears a hull-sync block"); } return; } }
  void* b = g_papi.mpbClass ? il2cpp_object_new(g_papi.mpbClass) : nullptr;
  if (b && g_papi.mpbCtor) Invoke(g_papi.mpbCtor, b);
  exc = nullptr; void* a2[1] = { b }; if (b) il2cpp_runtime_invoke(g_papi.setBlock, r, a2, &exc);
  if (way != 2) { way = 2; Log("paint: SetPropertyBlock(null) threw; hull-sync blocks are replaced by an empty one%s", !b || exc ? " (THAT FAILED TOO: those slots may keep the hull colour)" : ""); }
}
// 0 never painted, 1 a segmentation shader (_Color1.._Color5 by mask), 2 Custom/PbrBC (tints a baked albedo).
static int PaintKind(void* m, std::string* base) {
  std::string n = NameOf(m); size_t p = n.find(" ("); *base = p == std::string::npos ? n : n.substr(0, p);
  static const char* part[] = { "Brass", "Leather", "Ammo", "Crew", "Bandolier", "Wheels", "Wire", "Default-Material", "Damage" };   // brass, leather, belts, crew, tyres, effects
  static const char* whole[] = { "Red", "Blue", "Green", "Orange", "Slot" };   // the marker and highlight materials
  for (const char* x : part) if (base->find(x) != std::string::npos) return 0;
  for (const char* x : whole) if (*base == x) return 0;
  void* s = Invoke(g_mapi.getShader, m); std::string sn = s ? NameOf(s) : std::string();
  if (sn.compare(0, 20, "Custom/StandardColor") == 0 || sn.compare(0, 19, "Custom/LambertColor") == 0) return 1;
  return sn == "Custom/PbrBC" ? 2 : 0;
}
static std::unordered_map<std::string, PaintCopy>::iterator PaintDrop(std::unordered_map<std::string, PaintCopy>::iterator c) {
  Unpin(c->second.handle); Unpin(c->second.stockHandle); g_paintStock.erase(c->second.mat); return g_paintCopy.erase(c);
}
static void* PaintCopyOf(void* stock, const std::string& base, int p, const float f[5]) {
  char key[160]; snprintf(key, sizeof key, "%s|%p|%d|%.2f,%.2f,%.2f,%.2f,%.2f", base.c_str(), stock, p, f[0], f[1], f[2], f[3], f[4]);
  auto c = g_paintCopy.find(key);
  if (c != g_paintCopy.end()) {
    if (Alive(c->second.mat) && Alive(c->second.stock)) return c->second.mat;
    PaintDrop(c);                                                     // unloaded with unused assets: made again
  }
  // copies of stock materials that are gone (a per-boat instance, unloaded assets) are let go, so they do not pile up
  for (auto d = g_paintCopy.begin(); d != g_paintCopy.end();) d = Alive(d->second.stock) ? std::next(d) : PaintDrop(d);
  void* m = CloneObject(stock); if (!m) return nullptr;
  const PaintScheme& s = kPaint[p]; std::string slots;
  for (int k = 0; k < 5; k++) {
    if (f[k] <= 0) continue;
    float col[4] = { s.rgb[0] * f[k], s.rgb[1] * f[k], s.rgb[2] * f[k], 1.0f };
    char prop[16]; snprintf(prop, sizeof prop, "_Color%d", k + 1);
    void* a[2] = { il2cpp_string_new(prop), col }; Invoke(g_papi.setColor, m, a);
    char b[24]; snprintf(b, sizeof b, " %d x%.2f", k + 1, f[k]); slots += b;
  }
  if (s.metallic >= 0 && g_papi.setFloat) { float v = s.metallic; void* a[2] = { il2cpp_string_new("_Metallic"), &v }; Invoke(g_papi.setFloat, m, a); }
  // '(Instance)': GameMaterial's game.<name> lookup prefers the shared stock material, so a model never wears another gun's paint
  SetName(m, (base + " (Instance) WM paint " + std::to_string(p)).c_str());
  g_paintCopy[key] = { m, Pin(m), stock, Pin(stock) }; g_paintStock[m] = stock; g_paintCopies++;
  DLog("paint: new copy of '%s' in %s (slots%s)", base.c_str(), s.name, slots.c_str());
  return m;
}
static void ApplyPaint(void* mount, const Item& it) {
  if (it.retired || IsTorpFamily(it.family)) return;                  // hidden placeholders; torpedoes have TorpedoColorer
  int p = PaintOf(it);
  if (!p || g_paintOff || !PaintApiReady()) return;
  void* mt = Invoke(m_getTransform, mount);
  if (!mt) return;
  // 1. the hull-colour links: which slots they paint (they let go of a renderer only once it wears paint, in 3)
  std::vector<void*> ccs = PaintComponents(mount, g_papi.ccsType);
  std::unordered_map<void*, std::vector<std::pair<int, float>>> sync;   // renderer -> (slot, brightness) its syncs write
  for (void* c : ccs) {
    int slot = *(int*)((char*)c + g_papi.ccsIndex); float br = *(float*)((char*)c + g_papi.ccsBright);
    if (!(br > 0.05f && br < 4.0f)) br = 1.0f;
    void* rs = *(void**)((char*)c + g_papi.ccsRenderers);
    int n = ArrLen(rs);
    for (int i = 0; i < n && i < 256; i++) if (void* r = ArrAt(rs, i)) if (slot >= 1 && slot <= 5) sync[r].push_back({ slot, br });
  }
  // 2. the materials, near and far LOD renderers alike
  std::set<std::string> painted; std::set<void*> took; int slots = 0;   // took: renderers wearing at least one painted copy
  for (void* r : RenderersUnder(mt)) {
    if (!IsMeshRenderer(r)) continue;
    void* arr = Invoke(g_mapi.getMats, r);                            // a fresh Material[]: edited, then set back
    int n = ArrLen(arr);
    auto sf = sync.find(r); bool changed = false;
    for (int i = 0; i < n && i < 16; i++) {
      void* worn = ArrAt(arr, i); if (!worn) continue;
      void* m = worn; auto back = g_paintStock.find(m); if (back != g_paintStock.end() && Alive(back->second)) m = back->second;   // a copy of a painted gun
      std::string base; int kind = PaintKind(m, &base);
      const PaintSlots* t = nullptr; if (kind == 1) for (const PaintSlots& x : kPaintSlots) if (base == x.mat) { t = &x; break; }
      float f[5] = { 0, 0, 0, 0, 0 };
      if (t) memcpy(f, t->f, sizeof f);
      bool synced = false;                                            // a slot the hull sync paints is paint, at its brightness
      if (kind && sf != sync.end()) for (auto& sb : sf->second) if (!t || f[sb.first - 1] == 0) { f[sb.first - 1] = sb.second; synced = true; }
      if (kind == 1 && !t && !synced) f[0] = 1.0f;                     // PbrBC with no sync stays stock (see above)
      void* copy = kind == 1 || synced ? PaintCopyOf(m, base, p, f) : nullptr;
      if (!copy) { if (m != worn) { SetRef(arr, (char*)arr + 0x20 + 8 * (size_t)i, m); changed = true; } continue; }   // another scheme's copy goes back to stock
      SetRef(arr, (char*)arr + 0x20 + 8 * (size_t)i, copy); changed = true; took.insert(r);
      char k[96]; snprintf(k, sizeof k, "%s|%.2f,%.2f,%.2f,%.2f,%.2f", base.c_str(), f[0], f[1], f[2], f[3], f[4]);
      if (painted.insert(k).second) for (float x : f) if (x > 0) slots++;
    }
    if (changed) { void* a[1] = { arr }; Invoke(g_papi.setMats, r, a); }
  }
  // 3. a hull sync lets go of the renderers that wear paint (its block would cover it) and keeps the rest
  int taken = 0;
  for (void* c : ccs) {
    void* rs = *(void**)((char*)c + g_papi.ccsRenderers);
    int n = ArrLen(rs);
    if (n > 256) continue;                                            // never seen; a shortened list would drop renderers
    std::vector<void*> keep; bool cut = false;
    for (int i = 0; i < n; i++) {
      void* r = ArrAt(rs, i); if (!r) continue;
      if (took.count(r)) { PaintClearBlock(r); cut = true; } else keep.push_back(r);
    }
    if (!cut) continue;
    void* list = g_papi.arrayNew(g_papi.rendElem, keep.size()); if (!list) continue;
    for (size_t i = 0; i < keep.size(); i++) SetRef(list, (char*)list + 0x20 + 8 * i, keep[i]);
    SetRef(c, (char*)c + g_papi.ccsRenderers, list); taken++;
  }
  // 4. every reactor's snapshot takes the painted materials (SwapRenderersByDistance links near and far ones)
  int reactors = 0;
  for (void* v : PaintComponents(mount, g_papi.vrType)) { bool force = true; void* a[1] = { &force }; void* exc = nullptr; il2cpp_runtime_invoke(g_papi.vrFetch, v, a, &exc); if (!exc) reactors++; }
  if (painted.empty()) {
    static std::set<std::string> told;
    if (told.insert(it.base).second) Log("paint: '%s' was to wear %s, but its %s has no paintable material", it.name.c_str(), kPaint[p].name, it.base.c_str());
    return;
  }
  g_paintGuns++;
  static int shown = 0;                                               // every boat build repeats it
  (shown++ < 6 ? Log : DLog)("paint: '%s' wears %s (%d materials, %d slots, %d hull-sync slots taken, %d reactors refreshed)", it.name.c_str(), kPaint[p].name,
                             (int)painted.size(), slots, taken, reactors);
}
static void ApplyPaintGuarded(void* mount, const Item& it) {
  if (g_paintOff) return;
  __try { ApplyPaint(mount, it); }
  __except (FaultFilter("gun paint", GetExceptionInformation())) { g_paintOff = true; Log("  '%s': the paint could not be applied; paint schemes are off until restart", it.name.c_str()); }
}
static void PaintSummary() {                                          // at each battle change, when guns were painted since the last one
  if (g_paintGuns == g_paintGunsLogged || (!g_paintGuns && !g_paintOff)) return;
  g_paintGunsLogged = g_paintGuns;
  Log("paint: %d guns painted this session, %d material copies made%s", g_paintGuns, g_paintCopies, g_paintOff ? " (paint is OFF: a lookup failed or a fault)" : "");
}

