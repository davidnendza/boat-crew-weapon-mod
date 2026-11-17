// custom gun models (OBJ meshes on found guns) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- custom gun models ----------------------------------------------------------------------------------------------
// The game ships the Dummiesman runtime OBJ importer: OBJLoader.Load(path) builds a GameObject with one child per OBJ
// object (MeshFilter + MeshRenderer each, materials from the .mtl). model=<file> on an item swaps the copied gun's look
// for it: objects named stand* or base* go under GunMount.stand, barrel* under every Barrel.transform (recoil moves
// them), yaw* on the traverse at the pivot (turns, does not elevate), the rest under pivotPoint (turns and elevates).
// The gun's own mesh renderers are switched off; aiming, seats, shoot points, muzzle flashes and sounds stay the stock gun's.
// Component.GetComponentsInChildren(Type, bool) is stripped from this build (first test 2026-10-27), so renderers are
// found by walking the transforms with GetChild and asking each for its Renderer.
struct ModelApi { void *loaderCls, *ctor, *load, *goTransform, *childCount, *getChild, *lossy, *localEuler, *getComponent,
                  *rendererType, *forceOff, *getMat, *getMats, *setMat, *getShader, *bounds, *destroy, *dontDestroy; bool ok; };
static ModelApi g_mapi = {};
static void LogMethodsOf(void* klass) {                              // what a stripped class still has, for the next round
  std::string s;
  for (void* k = klass; k; k = il2cpp_class_get_parent(k)) {
    void* iter = nullptr; void* m;
    while ((m = il2cpp_class_get_methods(k, &iter))) {
      s += " "; s += il2cpp_method_get_name(m); s += "/" + std::to_string(il2cpp_method_get_param_count(m));
    }
  }
  Log("  methods of %s:%s", il2cpp_class_get_name(klass), s.c_str());
}
static bool ModelApiReady() {
  static bool tried = false;
  if (tried) return g_mapi.ok;
  tried = true;
  ModelApi& a = g_mapi;
  a.loaderCls = FindClass("Dummiesman", "OBJLoader");
  a.ctor = a.loaderCls ? FindMethod(a.loaderCls, ".ctor", 0) : nullptr;
  a.load = a.loaderCls ? FindMethod(a.loaderCls, "Load", 1, "System.String") : nullptr;
  void* go = FindClass("UnityEngine", "GameObject"); a.goTransform = go ? FindMethod(go, "get_transform", 0) : nullptr;
  void* tr = FindClass("UnityEngine", "Transform");
  a.childCount = tr ? FindMethod(tr, "get_childCount", 0) : nullptr; a.getChild = tr ? FindMethod(tr, "GetChild", 1) : nullptr;
  a.lossy = tr ? FindMethod(tr, "get_lossyScale", 0) : nullptr; a.localEuler = tr ? FindMethod(tr, "get_localEulerAngles", 0) : nullptr;
  void* comp = FindClass("UnityEngine", "Component");
  a.getComponent = comp ? FindMethod(comp, "GetComponent", 1, "System.Type") : nullptr;
  void* rk = FindClass("UnityEngine", "Renderer");
  a.rendererType = rk ? il2cpp_type_get_object(il2cpp_class_get_type(rk)) : nullptr;
  a.forceOff = rk ? FindMethod(rk, "set_forceRenderingOff", 1) : nullptr; a.getMat = rk ? FindMethod(rk, "get_sharedMaterial", 0) : nullptr;
  a.setMat = rk ? FindMethod(rk, "set_sharedMaterial", 1) : nullptr; a.bounds = rk ? FindMethod(rk, "get_bounds", 0) : nullptr;
  a.getMats = rk ? FindMethod(rk, "get_sharedMaterials", 0) : nullptr;   // optional: without it only first materials are found
  void* mat = FindClass("UnityEngine", "Material"); a.getShader = mat ? FindMethod(mat, "get_shader", 0) : nullptr;
  void* obj = FindClass("UnityEngine", "Object"); a.destroy = obj ? FindMethod(obj, "Destroy", 1, "UnityEngine.Object") : nullptr;
  a.dontDestroy = obj ? FindMethod(obj, "DontDestroyOnLoad", 1) : nullptr;   // optional: without it every gun parses its .obj
  std::string missing;
  const std::pair<const char*, void*> need[] = { { "OBJLoader..ctor", a.ctor }, { "OBJLoader.Load", a.load }, { "GameObject.get_transform", a.goTransform },
    { "Transform.get_childCount", a.childCount }, { "Transform.GetChild", a.getChild }, { "Transform.get_lossyScale", a.lossy },
    { "Component.GetComponent(Type)", a.getComponent }, { "Renderer type", a.rendererType }, { "Renderer.set_forceRenderingOff", a.forceOff },
    { "Renderer.get_sharedMaterial", a.getMat }, { "Renderer.set_sharedMaterial", a.setMat }, { "Material.get_shader", a.getShader },
    { "Renderer.get_bounds", a.bounds }, { "Transform.SetParent(t, bool)", g_setParentTwoArgs ? m_setParent : nullptr } };
  for (auto& n : need) if (!n.second) missing += std::string(" ") + n.first;
  a.ok = missing.empty();
  if (a.ok) Log("gun models: ready (Destroy %s)", a.destroy ? "found" : "missing, the loader's empty holder stays");
  else {
    Log("gun models: missing%s; model= is ignored", missing.c_str());
    for (void* k : { comp, rk, tr, go }) if (k) LogMethodsOf(k);
  }
  return a.ok;
}
static int Unbox32(void* o) { return o ? *(int*)((char*)o + 0x10) : 0; }
static V3 GetV3(void* method, void* obj) { void* r = method && obj ? Invoke(method, obj) : nullptr; return r ? *(V3*)((char*)r + 0x10) : V3{ 0, 0, 0 }; }
static void CollectRenderers(void* t, int depth, std::vector<void*>& out) {
  if (!t || depth > 16) return;
  void* r = Invoke(g_mapi.getComponent, t, (void**)&g_mapi.rendererType);
  if (r) out.push_back(r);
  int n = Unbox32(Invoke(g_mapi.childCount, t));
  for (int i = 0; i < n && i < 128; i++) { void* a[1] = { &i }; CollectRenderers(Invoke(g_mapi.getChild, t, a), depth + 1, out); }
}
static std::vector<void*> RenderersUnder(void* t) {                  // every Renderer under a transform, inactive ones too
  std::vector<void*> out; CollectRenderers(t, 0, out); return out;
}
// The same, leaving out the subtrees whose transform name starts with one of `keep` (model_keep); *kept counts them.
static void CollectRenderersExcept(void* t, int depth, const std::vector<std::string>& keep, std::vector<void*>& out, int* kept) {
  if (!t || depth > 16) return;
  if (depth > 0 && !keep.empty()) {
    std::string n = NameOf(t);
    for (const std::string& k : keep) if (n.compare(0, k.size(), k) == 0) { (*kept)++; return; }
  }
  void* r = Invoke(g_mapi.getComponent, t, (void**)&g_mapi.rendererType);
  if (r) out.push_back(r);
  int n = Unbox32(Invoke(g_mapi.childCount, t));
  for (int i = 0; i < n && i < 128; i++) { void* a[1] = { &i }; CollectRenderersExcept(Invoke(g_mapi.getChild, t, a), depth + 1, keep, out, kept); }
}
static bool IsMeshRenderer(void* r) { const char* c = il2cpp_class_get_name(il2cpp_object_get_class(r)); return !strcmp(c, "MeshRenderer") || !strcmp(c, "SkinnedMeshRenderer"); }
static std::string MaterialInfo(void* r) {
  void* m = Invoke(g_mapi.getMat, r); void* s = m ? Invoke(g_mapi.getShader, m) : nullptr;
  return "material '" + (m ? NameOf(m) : std::string("none")) + "' shader '" + (s ? NameOf(s) : std::string("none")) + "'";
}
// Game materials by name. A model part whose .mtl material is called game.<name> (spaces as '_', alternatives split by
// '|', e.g. game.M4_37_MM_Autocannon|Dark_MG_Metal) wears the boat's own material of that name, so models built from
// the game's meshes and palette UVs (tools/make_turret_models.py) look like the stock guns. Found by walking every
// renderer of the gun's own boat (GunMount.entity) once; all guns of a boat are built together, so the list is kept for
// a few seconds. The shared material is preferred: an "X (Instance)" copy belongs to one renderer (a damage tint, say)
// and is only used when the boat has no shared X. Each listed material is held by a GC handle until the list is redone.
static std::string MatKey(std::string n) {
  size_t p = n.find(" ("); if (p != std::string::npos) n.resize(p);
  for (char& c : n) if (c == ' ') c = '_';
  return n;
}
struct GameMat { void* mat; void* handle; bool instance; };
static std::unordered_map<std::string, GameMat> g_gameMats;
static void* g_gameMatsRoot = nullptr;
static DWORD g_gameMatsAt = 0;
static bool g_gameMatsWide = false;                                  // the tree above the boat was searched too
static void AddGameMaterial(void* m) {
  std::string name = NameOf(m);
  bool instance = name.find(" (Instance)") != std::string::npos;
  auto e = g_gameMats.find(MatKey(name));
  if (e != g_gameMats.end()) {
    if (!e->second.instance || instance) return;                       // keep the shared one, or the first copy
    Unpin(e->second.handle); e->second = { m, Pin(m), false };
    return;
  }
  g_gameMats.emplace(MatKey(name), GameMat{ m, Pin(m), instance });
}
static void CollectMaterials(void* t, int depth, int& budget) {
  if (!t || depth > 24 || --budget < 0) return;
  void* r = Invoke(g_mapi.getComponent, t, (void**)&g_mapi.rendererType);
  if (r && IsMeshRenderer(r)) {
    void* arr = g_mapi.getMats ? Invoke(g_mapi.getMats, r) : nullptr;
    size_t n = ArrLen(arr);
    if (arr) { for (size_t i = 0; i < n && i < 16; i++) { void* m = ArrAt(arr, i); if (m) AddGameMaterial(m); } }
    else if (void* m = Invoke(g_mapi.getMat, r)) AddGameMaterial(m);
  }
  int n = Unbox32(Invoke(g_mapi.childCount, t));
  for (int i = 0; i < n && i < 512; i++) { void* a[1] = { &i }; CollectMaterials(Invoke(g_mapi.getChild, t, a), depth + 1, budget); }
}
static void* GameMaterial(void* mount, const std::string& key) {
  static int offEntity = -2;
  if (offEntity == -2) offEntity = FieldOffset("Core", "GunMount", "entity");
  auto topOf = [&]() { void* t = Invoke(m_getTransform, mount); for (int i = 0; i < 64 && t; i++) { void* p = Invoke(m_getParent, t); if (!p) break; t = p; } return t; };
  void* ent = offEntity >= 0 ? *(void**)((char*)mount + offEntity) : nullptr;
  void* root = ent ? Invoke(m_getTransform, ent) : nullptr;
  bool wide = !root;
  if (wide) root = topOf();                                           // no boat (a preview copy?): the top of the tree
  if (root != g_gameMatsRoot || GetTickCount() - g_gameMatsAt > 10000) {
    for (auto& e : g_gameMats) Unpin(e.second.handle);
    g_gameMats.clear();
    int budget = 12000;
    CollectMaterials(root, 0, budget);
    g_gameMatsRoot = root; g_gameMatsAt = GetTickCount(); g_gameMatsWide = wide;
    DLog("gun models: %d materials found on '%s'%s", (int)g_gameMats.size(), root ? NameOf(root).c_str() : "?", budget < 0 ? " (stopped early, very big tree)" : "");
  }
  auto f = g_gameMats.find(key);
  if (f == g_gameMats.end() && !g_gameMatsWide) {                     // not on the boat: look once above it too
    g_gameMatsWide = true;
    void* top = topOf(); int budget = 12000;
    if (top && top != root) CollectMaterials(top, 0, budget);
    DLog("gun models: '%s' is not on the boat; searched above it, %d materials now%s", key.c_str(), (int)g_gameMats.size(), budget < 0 ? " (stopped early)" : "");
    f = g_gameMats.find(key);
  }
  return f == g_gameMats.end() ? nullptr : f->second.mat;
}

// One line per transform under the stock gun: local position and rotation, world scale, and the size of its mesh, with
// the parts the model attaches to marked. Logged once per base gun, to size and orient models.
static void LogModelTree(void* t, int depth, const std::vector<std::pair<void*, std::string>>& marks) {
  if (!t || depth > 8) return;
  std::string tag; for (auto& m : marks) if (m.first == t) tag += " <" + m.second + ">";
  V3 lp = GetV3(m_getLocalPos, t), le = GetV3(g_mapi.localEuler, t), ls = GetV3(g_mapi.lossy, t);
  void* r = Invoke(g_mapi.getComponent, t, (void**)&g_mapi.rendererType);
  std::string rs;
  if (r) { void* b = Invoke(g_mapi.bounds, r); V3 ext = b ? *(V3*)((char*)b + 0x1C) : V3{ 0, 0, 0 };
           char buf[96]; snprintf(buf, sizeof buf, " %s size (%.2f, %.2f, %.2f) ", il2cpp_class_get_name(il2cpp_object_get_class(r)), ext.x * 2, ext.y * 2, ext.z * 2); rs = buf + MaterialInfo(r); }
  DLog("  %*s'%s' pos (%.2f, %.2f, %.2f) rot (%.0f, %.0f, %.0f) worldScale (%.2f, %.2f, %.2f)%s%s", depth * 2, "", NameOf(t).c_str(),
       lp.x, lp.y, lp.z, le.x, le.y, le.z, ls.x, ls.y, ls.z, rs.c_str(), tag.c_str());
  int n = Unbox32(Invoke(g_mapi.childCount, t));
  for (int i = 0; i < n && i < 64; i++) { void* a[1] = { &i }; LogModelTree(Invoke(g_mapi.getChild, t, a), depth + 1, marks); }
}
// Each .obj is parsed once. The first load is kept as a hidden template (inactive, kept across scene loads, held by a
// GC handle) and every later gun gets a copy of it: the six turret guns go in both turret slots of every boat, and
// parsing a dozen files at each boat build showed as a hitch. A template Unity has destroyed anyway is loaded again.
struct ModelTemplate { void* root; void* handle; };
static std::unordered_map<std::string, ModelTemplate> g_modelTemplates;
static void* LoadModelCopy(const std::string& path, const std::string& gunName) {
  auto t = g_modelTemplates.find(path);
  if (t != g_modelTemplates.end()) {
    if (*(void**)((char*)t->second.root + 0x10)) return CloneObject(t->second.root);   // m_CachedPtr is 0 once destroyed
    Unpin(t->second.handle);
    g_modelTemplates.erase(t);
  }
  LARGE_INTEGER f, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
  void* ldr = il2cpp_object_new(g_mapi.loaderCls);
  if (!ldr) return nullptr;
  Invoke(g_mapi.ctor, ldr);
  void* exc = nullptr; void* la[1] = { il2cpp_string_new(path.c_str()) };
  void* root = il2cpp_runtime_invoke(g_mapi.load, ldr, la, &exc);
  QueryPerformanceCounter(&b);
  if (exc || !root) { Log("gun model: loading %s failed%s; '%s' keeps its look", path.c_str(), exc ? " (exception)" : "", gunName.c_str()); return nullptr; }
  double ms = (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart;
  if (!il2cpp_gchandle_new || !g_mapi.dontDestroy || !m_setActive) { DLog("gun model: parsed %s in %.0f ms (no template: a piece is missing)", path.c_str(), ms); return root; }
  bool off = false; void* a1[1] = { &off }; Invoke(m_setActive, root, a1);
  void* a2[1] = { root }; Invoke(g_mapi.dontDestroy, nullptr, a2);
  g_modelTemplates[path] = { root, Pin(root) };
  DLog("gun model: parsed %s in %.0f ms; later guns copy it", path.c_str(), ms);
  return CloneObject(root);
}

static void ApplyModel(void* mount, const Item& it) {
  if (it.model.empty() || !ModelApiReady()) return;
  static int offStand = -2, offPivot = -1, offShoot = -1, offBarrels = -1, offYaw = -1;
  if (offStand == -2) {
    void* k = FindClass("Core", "GunMount");
    offStand = FieldOffset(k, "stand"); offPivot = FieldOffset(k, "pivotPoint"); offShoot = FieldOffset(k, "shootPoints"); offBarrels = FieldOffset(k, "barrels"); offYaw = FieldOffset(k, "yawFineTuner");
  }
  if (offStand < 0 || offPivot < 0 || offBarrels < 0) { Log("gun model: GunMount fields missing; '%s' keeps its look", it.name.c_str()); return; }
  void* mt = Invoke(m_getTransform, mount);
  void* stand = *(void**)((char*)mount + offStand); void* pivot = *(void**)((char*)mount + offPivot);
  void* barrels = *(void**)((char*)mount + offBarrels);
  size_t nb = ArrLen(barrels);
  std::vector<void*> barrelTs;                                         // Barrel.transform is its first field (+0x10)
  for (size_t i = 0; i < nb && i < 16; i++) {                         // the twin M2 lists one transform twice: one copy is enough
    void* b = ArrAt(barrels, i); void* bt = b ? *(void**)((char*)b + 0x10) : nullptr;
    if (bt && std::find(barrelTs.begin(), barrelTs.end(), bt) == barrelTs.end()) barrelTs.push_back(bt);
  }
  static std::set<std::string> logged;
  if (logged.insert(it.base).second) {
    std::vector<std::pair<void*, std::string>> marks = { { stand, "stand" }, { pivot, "pivotPoint" } };
    for (size_t i = 0; i < barrelTs.size(); i++) marks.push_back({ barrelTs[i], "barrel " + std::to_string(i) });
    void* sp = offShoot >= 0 ? *(void**)((char*)mount + offShoot) : nullptr;
    size_t ns = ArrLen(sp);
    for (size_t i = 0; i < ns && i < 16; i++) { void* s = ArrAt(sp, i); if (s) marks.push_back({ s, "shootPoint " + std::to_string(i) }); }
    DLog("gun model: parts of '%s' (%d barrels):", it.base.c_str(), (int)barrelTs.size());
    LogModelTree(mt, 0, marks);
  }
  // model= names a file in BoatCrewMod\models (sub-folders allowed); a shared ini must not reach anything else on disk
  if (it.model.find("..") != std::string::npos || it.model.find(':') != std::string::npos || it.model[0] == '\\' || it.model[0] == '/') {
    Log("gun model: '%s' is not a file name inside BoatCrewMod\\models; '%s' keeps its look", it.model.c_str(), it.name.c_str()); return;
  }
  std::string path = g_folder + "\\models\\" + it.model;
  if (path.size() < 4 || _stricmp(path.c_str() + path.size() - 4, ".obj") != 0) path += ".obj";
  if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) { Log("gun model: %s not found; '%s' keeps its look", path.c_str(), it.name.c_str()); return; }
  void* root = LoadModelCopy(path, it.name);
  if (!root) return;
  void* rootT = Invoke(g_mapi.goTransform, root);
  int n = Unbox32(Invoke(g_mapi.childCount, rootT));
  std::vector<void*> parts;
  for (int i = 0; i < n && i < 64; i++) { void* a[1] = { &i }; void* c = Invoke(g_mapi.getChild, rootT, a); if (c) parts.push_back(c); }
  // The stock meshes go dark through forceRenderingOff: SwapRenderersByDistance, VisualReactor, GunMountAnimation and
  // GunMountMunitionsListener all switch Renderer.enabled back on, and nothing in the game sets forceRenderingOff (test 2,
  // 2026-10-27). Particles (muzzle flash) stay. The gun's material (used unless model_material=own) is its most used
  // one: the first renderer on the Bofors is an empty glowing 'Red' one under pivotPoint.
  // model_keep names stock parts that stay (a turret gun's ring mount, belts, pedestal), so only the gun is replaced.
  std::map<void*, int> matUse; int hidden = 0, keptParts = 0;
  std::vector<void*> stockRenderers;
  CollectRenderersExcept(mt, 0, it.modelKeep, stockRenderers, &keptParts);
  for (void* r : stockRenderers) {
    if (!IsMeshRenderer(r)) continue;
    if (void* m = Invoke(g_mapi.getMat, r)) matUse[m]++;
    bool off = true; void* a[1] = { &off }; Invoke(g_mapi.forceOff, r, a); hidden++;
  }
  void* gunMat = nullptr; int best = 0;
  for (auto& mu : matUse) if (mu.second > best) { best = mu.second; gunMat = mu.first; }
  V3 ms = GetV3(g_mapi.lossy, mt);
  int gameMats = 0, ownMats = 0;
  static std::set<std::string> missingMats;
  auto attach = [&](void* partT, void* target) {
    bool keepWorld = false; void* a[2] = { target, &keepWorld }; Invoke(m_setParent, partT, a);
    V3 zero = { 0, 0, 0 }; Q4 ident = { 0, 0, 0, 1 }; V3 ts = GetV3(g_mapi.lossy, target);
    V3 sc = { it.modelScale * (ts.x != 0 ? ms.x / ts.x : 1), it.modelScale * (ts.y != 0 ? ms.y / ts.y : 1), it.modelScale * (ts.z != 0 ? ms.z / ts.z : 1) };
    void* a1[1] = { &zero }; Invoke(m_setLocalPos, partT, a1);
    void* a2[1] = { &ident }; Invoke(m_setLocalRot, partT, a2);
    void* a3[1] = { &sc }; Invoke(m_setLocalScale, partT, a3);
    for (void* r : RenderersUnder(partT)) {
      static int shown = 0;
      if (shown++ < 4) DLog("gun model: part '%s' loaded with %s", NameOf(partT).c_str(), MaterialInfo(r).c_str());
      void* cur = Invoke(g_mapi.getMat, r);
      std::string cn = cur ? NameOf(cur) : std::string();
      if (cn.compare(0, 5, "game.") == 0) {
        std::string alts = MatKey(cn.substr(5)); void* gmat = nullptr;
        for (size_t a = 0; a <= alts.size() && !gmat; ) {
          size_t b = alts.find('|', a); if (b == std::string::npos) b = alts.size();
          gmat = GameMaterial(mount, alts.substr(a, b - a)); a = b + 1;
        }
        if (gmat) { void* m[1] = { gmat }; Invoke(g_mapi.setMat, r, m); gameMats++; }
        else {
          ownMats++;
          if (missingMats.insert(cn).second) Log("gun model: '%s' wants the game material %s, which this boat does not have; its .mtl color is used", it.name.c_str(), cn.c_str());
        }
        continue;
      }
      if (!it.modelOwnMaterial && gunMat) { void* m[1] = { gunMat }; Invoke(g_mapi.setMat, r, m); }
    }
  };
  int placed = 0;
  for (void* p : parts) {
    std::string nm = NameOf(p); for (char& c : nm) c = (char)tolower((unsigned char)c);
    if (nm.compare(0, 6, "barrel") == 0 && !barrelTs.empty()) {
      for (size_t i = 1; i < barrelTs.size(); i++) { void* copy = CloneObject(p); if (copy) { attach(copy, barrelTs[i]); placed++; } }
      attach(p, barrelTs[0]);
    } else if (nm.compare(0, 3, "yaw") == 0 && pivot) {
      // yaw*: turns with the aim but does not elevate. aimAt(rotate) and turn() (read 2026-11-15) yaw yawFineTuner, or the
      // mount's own transform when it has none (the 3-inch), and give pivotPoint only its pitch. The part is authored in
      // the pivot frame, so it hangs on that traverse at the pivot's position, level.
      void* yawT = offYaw >= 0 ? *(void**)((char*)mount + offYaw) : nullptr;
      if (!yawT) yawT = mt;
      attach(p, yawT);
      if (Invoke(m_getParent, pivot) == yawT) { V3 lp = GetV3(m_getLocalPos, pivot); void* a[1] = { &lp }; Invoke(m_setLocalPos, p, a); }
      else { static std::set<std::string> toldYaw; if (toldYaw.insert(it.name).second) Log("gun model: '%s': pivotPoint does not hang on its traverse; yaw parts sit at the traverse's origin", it.name.c_str()); }
    } else if (nm.compare(0, 5, "stand") == 0 || nm.compare(0, 4, "base") == 0) attach(p, stand ? stand : mt);   // the .50 Cal has no stand: its root turns with the gun
    else attach(p, pivot ? pivot : mt);
    placed++;
  }
  if (g_mapi.destroy) { void* a[1] = { root }; Invoke(g_mapi.destroy, nullptr, a); }   // the loader's empty holder
  std::string matText = gameMats || ownMats ? std::to_string(gameMats) + " meshes in the game's materials, " + std::to_string(ownMats) + " in .mtl colors"
                      : it.modelOwnMaterial ? std::string("own material") : gunMat ? "the gun's '" + NameOf(gunMat) + "' material" : std::string("own material (no stock material found)");
  static std::set<std::string> told;                                   // every boat build repeats it: the first time per gun is enough
  (told.insert(it.name).second ? Log : DLog)("gun model: '%s' wears %s (%d parts placed, %d stock meshes hidden, %d stock parts kept, %s)", it.name.c_str(), it.model.c_str(), placed, hidden, keptParts, matText.c_str());
}
static int FaultFilter(const char* job, EXCEPTION_POINTERS* ep);
static bool g_modelsOff = false;                                     // a model fault: models are off until restart, as FaultFilter says
static void ApplyModelGuarded(void* mount, const Item& it) {
  if (g_modelsOff) return;
  __try { ApplyModel(mount, it); }
  __except (FaultFilter("gun model", GetExceptionInformation())) { g_modelsOff = true; Log("  '%s': the model could not be applied", it.name.c_str()); }
}

