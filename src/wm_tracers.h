// tracer color and rarity tracers (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- tracer color: recolor the shared BasicYellow projectile material (no stock projectile uses style 2) ------------
// ProjectileManager.render[style].LOD[i].material is what draws a projectile; its look is the shader color _MainColor.
static void* g_tracerManager = nullptr;                             // the ProjectileManager we already recolored for (a new mission has a new one)
static void ApplyTracerColor(void* manager) {
  if (!g_tracerSet || manager == g_tracerManager) return;
  g_tracerManager = manager;                                          // even on failure: try once per manager
  void* pm = FindClass("Core", "ProjectileManager");
  void* fRender = pm ? il2cpp_class_get_field_from_name(pm, "render") : nullptr;
  void* matC = FindClass("UnityEngine", "Material");
  void* mSet = matC ? FindMethod(matC, "SetColor", 2, "System.String", "UnityEngine.Color") : nullptr;
  void* mGet = matC ? FindMethod(matC, "GetColor", 1, "System.String") : nullptr;
  if (!fRender || !mSet || !mGet) { Log("tracer color: a lookup failed (render %p SetColor %p GetColor %p)", fRender, mSet, mGet); return; }
  void* arr = *(void**)((char*)manager + il2cpp_field_get_offset(fRender));
  if (!arr || ArrLen(arr) < 3) { Log("tracer color: render array is missing or too short"); return; }
  void* r = ArrAt(arr, 2);                     // style 2 = BasicYellow
  void* fLod = r ? il2cpp_class_get_field_from_name(il2cpp_object_get_class(r), "LOD") : nullptr;
  void* lod = fLod ? *(void**)((char*)r + il2cpp_field_get_offset(fLod)) : nullptr;
  int ln = ArrLen(lod);
  int done = 0;
  for (int i = 0; i < ln; i++) {
    void* rt = ArrAt(lod, i);
    void* fMat = rt ? il2cpp_class_get_field_from_name(il2cpp_object_get_class(rt), "material") : nullptr;
    void* mat = fMat ? *(void**)((char*)rt + il2cpp_field_get_offset(fMat)) : nullptr;
    if (!mat) continue;
    void* name = il2cpp_string_new("_MainColor");
    void* ga[1] = { name };
    void* before = Invoke(mGet, mat, ga);
    float old[4] = { -1, -1, -1, -1 }; if (before) memcpy(old, (char*)before + 0x10, sizeof old);   // boxed Color: after the 0x10 header
    float c[4] = { g_tracerRGBA[0], g_tracerRGBA[1], g_tracerRGBA[2], g_tracerRGBA[3] };
    void* sa[2] = { name, c };
    Invoke(mSet, mat, sa);
    void* after = Invoke(mGet, mat, ga);
    float now[4] = { -1, -1, -1, -1 }; if (after) memcpy(now, (char*)after + 0x10, sizeof now);
    DLog("tracer color: material '%s' _MainColor (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)", NameOf(mat).c_str(), old[0], old[1], old[2], now[0], now[1], now[2]);
    done++;
  }
  if (!done) Log("tracer color: the BasicYellow renderer has no material to recolor");
}

// ---- rarity tracers: two more projectile renderers, blue (Uncommon) and gold (Rare) ---------------------------------
// ProjectileManager.render (RenderInstanced[]) is indexed by ProjectileType.style in fillRenderer (styles 3 and 4 are
// special-cased, the rest share one path). Each RenderInstanced is a self-drawing component; its LOD RenderTypes hold the
// mesh and a material whose _MainColor is the tracer color. The palette copies render[0] twice (parented next to the
// original so scene changes treat it the same), gives each copy its own colored material, runs its Setup() now (its Start
// would only run next frame, after the first fillRenderer), and swaps in a longer array. Only then do registered cloned
// projectile types get style 6 or 7, so no shot ever indexes past the array.
struct PaletteShot { void* ptype; void* handle; int slot; int orig; };   // slot 0 Uncommon, 1 Rare, 2.. cosmetic colors (kPalette); orig: its own style
static const float kPalette[kPaletteN][4] = {
  { 0.20f, 0.55f, 1.0f, 1.0f }, { 1.0f, 0.70f, 0.05f, 1.0f },          // Uncommon blue, Rare gold
  { 0.15f, 1.0f, 0.25f, 1.0f }, { 0.60f, 0.20f, 1.0f, 1.0f },          // green, purple   (tracerColor 1, 2)
  { 0.10f, 0.95f, 1.0f, 1.0f }, { 1.0f, 0.45f, 0.05f, 1.0f },          // cyan, orange    (3, 4)
  { 1.0f, 0.35f, 0.75f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },            // pink, white     (5, 6)
  { 1.0f, 0.18f, 0.12f, 1.0f } };                                    // red: crew weapons (kCrewSlot, crew_tracers)
// Latched: crew_tracers can be switched off while playing, but shots already set to styles 6.. need every later
// ProjectileManager to have the longer render array.
static bool PaletteOn() { static bool on = false; if (!on) on = g_tracerRarity || g_cosmeticChance > 0 || g_crewTracers; return on; }
static std::vector<PaletteShot> g_paletteShots;
static std::unordered_map<void*, std::vector<PaletteShot>> g_crewTypes;   // crew weapon GunType -> its red copies (empty: left alone); CrewTracerFor
static int g_paletteBase = -1;                                       // style index of the Uncommon renderer once ready
static bool g_paletteDead = false;                                   // the palette could not be set up: shots keep their own styles
static int g_offPtStyle = -2;
static void SetShotStyle(const PaletteShot& ps) {
  if (g_offPtStyle == -2) g_offPtStyle = FieldOffset("Core", "ProjectileType", "style");
  if (g_offPtStyle < 0 || g_paletteBase < 0 || ps.slot < 0) return;
  *(int*)((char*)ps.ptype + g_offPtStyle) = g_paletteBase + ps.slot;
}
static void RegisterPaletteShot(void* ptype, int slot) {
  if (!PaletteOn() || slot < 0 || slot >= kPaletteN || !ptype) return;
  if (g_offPtStyle == -2) g_offPtStyle = FieldOffset("Core", "ProjectileType", "style");
  int style = g_offPtStyle >= 0 ? *(int*)((char*)ptype + g_offPtStyle) : 0;
  if (style == 3 || style == 4) return;                               // Rocket / Rocket_ASW: a rocket body, not a tracer; keep its look
  PaletteShot ps = { ptype, Pin(ptype), slot, style };
  if (g_paletteDead) return;
  g_paletteShots.push_back(ps);
  if (g_paletteBase >= 0) SetShotStyle(ps);
}
// The palette failed: every colored shot type gets its own style back, or the game would index past the 6 renderers.
static void PaletteFailed() {
  g_paletteDead = true; g_paletteBase = -1;
  for (const PaletteShot& ps : g_paletteShots) if (g_offPtStyle >= 0) *(int*)((char*)ps.ptype + g_offPtStyle) = ps.orig;
  for (auto& kv : g_crewTypes) for (const PaletteShot& ps : kv.second) if (g_offPtStyle >= 0) *(int*)((char*)ps.ptype + g_offPtStyle) = ps.orig;
  Log("rarity tracers: off for this session; %d shot types keep their own tracers", (int)g_paletteShots.size());
}
static void SafePaletteFailed() { __try { PaletteFailed(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_paletteDead = true; } }
// Checked every frame by the render array's length, not by the manager's address: a new battle's manager could reuse
// the old one's memory, and a skipped one would leave styles 6-13 pointing past its 6 renderers.
static void ExtendPalette(void* manager) {
  if (!PaletteOn() || g_paletteDead) return;
  static int offRender = -2; static void *mSet = nullptr, *(*arrayNew)(void*, size_t) = nullptr;
  if (offRender == -2) {
    void* pmk = FindClass("Core", "ProjectileManager");
    void* matC = FindClass("UnityEngine", "Material");
    mSet = matC ? FindMethod(matC, "SetColor", 2, "System.String", "UnityEngine.Color") : nullptr;
    arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
    offRender = FieldOffset(pmk, "render");
    if (offRender < 0 || !mSet || !arrayNew) { Log("rarity tracers: a lookup failed (render %d SetColor %p array_new %p)", offRender, mSet, arrayNew); PaletteFailed(); return; }
  }
  if (offRender < 0) return;
  void* arr = *(void**)((char*)manager + offRender);
  int n = ArrLen(arr);
  if (n == 6 + kPaletteN) return;                                      // this manager already has the palette
  if (n != 6) { Log("rarity tracers: render array has %d entries (expected 6); left alone", n); PaletteFailed(); return; }
  void* src = *(void**)((char*)arr + 0x20);                            // style 0, BasicWhite (Unlit projectile shader)
  if (!src) { PaletteFailed(); return; }
  void* rk = il2cpp_object_get_class(src);
  void* fLod = il2cpp_class_get_field_from_name(rk, "LOD");
  void* mSetup = il2cpp_class_get_method_from_name(rk, "Setup", 0);
  if (!fLod || !mSetup) { Log("rarity tracers: RenderInstanced.LOD or Setup missing"); PaletteFailed(); return; }
  int offLod = (int)il2cpp_field_get_offset(fLod);
  void* srcLod = *(void**)((char*)src + offLod);
  void* srcT = Invoke(m_getTransform, src); void* parent = srcT ? Invoke(m_getParent, srcT) : nullptr;
  void* made[kPaletteN] = {};
  for (int k = 0; k < kPaletteN; k++) {
    void* r = CloneObject(src);
    if (!r) { Log("rarity tracers: could not copy the renderer"); PaletteFailed(); return; }
    void* rt = Invoke(m_getTransform, r);
    if (rt && parent) { bool keepWorld = false; void* a[2] = { parent, &keepWorld }; Invoke(m_setParent, rt, a); }
    SetName(r, ("Instanced Renderer (WM palette " + std::to_string(k) + ")").c_str());
    void* lod = *(void**)((char*)r + offLod);
    if (!lod || lod == srcLod) { Log("rarity tracers: the copy shares its LOD list with the original; stopped"); PaletteFailed(); return; }
    int ln = ArrLen(lod);
    for (int i = 0; i < ln; i++) {
      void* t = ArrAt(lod, i); if (!t) continue;
      void* fMat = il2cpp_class_get_field_from_name(il2cpp_object_get_class(t), "material");
      void* mat = fMat ? *(void**)((char*)t + il2cpp_field_get_offset(fMat)) : nullptr;
      if (!mat) continue;
      void* mat2 = CloneObject(mat);
      if (!mat2) continue;
      float c[4] = { kPalette[k][0], kPalette[k][1], kPalette[k][2], kPalette[k][3] };
      void* sa[2] = { il2cpp_string_new("_MainColor"), c };
      Invoke(mSet, mat2, sa);
      SetRef(t, (char*)t + il2cpp_field_get_offset(fMat), mat2);
    }
    Invoke(mSetup, r);                                                 // builds its pools now; Start would be a frame late
    made[k] = r;
  }
  void* arr2 = arrayNew(rk, (size_t)n + kPaletteN);
  if (!arr2) { Log("rarity tracers: could not make the longer array"); PaletteFailed(); return; }
  for (int i = 0; i < n; i++) SetRef(arr2, (char*)arr2 + 0x20 + 8 * (size_t)i, ArrAt(arr, i));
  for (int k = 0; k < kPaletteN; k++) SetRef(arr2, (char*)arr2 + 0x20 + 8 * (size_t)(n + k), made[k]);
  SetRef(manager, (char*)manager + offRender, arr2);
  g_paletteBase = n;
  for (const PaletteShot& ps : g_paletteShots) SetShotStyle(ps);
  static int told = 0;
  if (told++ < 3) Log("tracer palette: ready (styles %d to %d; %d projectile types colored)", n, n + kPaletteN - 1, (int)g_paletteShots.size());
}

