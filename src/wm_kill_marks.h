// ---- kill markings: WWII-style tallies on the player boat's hull (planes shot down, ships sunk, shore guns) ----
// Three rows of silhouettes under the deck edge, both sides: a dark mark per kill, a gold one per ten. The quads are
// baked per boat by tools\kill_marks.py (wm_kill_marks_data.h) in the hull node's own coordinates; this file hangs one
// mesh ("WM kill marks") under that node whenever the boat's camo slot is picked (load, shipyard, battle start) and
// rebuilds it after each kill. Tallies are per boat type in kill-marks.ini, copied with each save (saves\<hash>.kill).
// Counted: kills the game credits to a gun on the player's own boat (not a co-op partner's, not allies or call-ins).
// shortcut: a co-op partner's boat of the same type shows this player's tallies; needs the partner's craft to fix.
#include "wm_kill_marks_data.h"

static const char* const kKillRowName[3] = { "planes", "ships", "shore targets" };
static struct {
  bool tried = false, on = false;
  void *goCls = nullptr, *goCtor = nullptr, *addComp = nullptr, *mfType = nullptr, *mrType = nullptr, *slotType = nullptr;
  void *meshCls = nullptr, *meshCtor = nullptr, *clear = nullptr, *setVerts = nullptr, *setUv = nullptr, *setTris = nullptr;
  void *normals = nullptr, *bounds = nullptr, *setMesh = nullptr, *setMat = nullptr, *setShadows = nullptr;
  void *v3 = nullptr, *v2 = nullptr, *i32 = nullptr, *mat = nullptr, *matPin = nullptr, *tex = nullptr, *destroy = nullptr;
  void* (*arrayNew)(void*, size_t) = nullptr;
} g_km;
static int g_kmCount[kKillBoatCount][3] = {};
struct KmLive { void* slot; void* go; void* mesh; int boat; std::vector<void*> pins; };
static std::vector<KmLive> g_kmLive;                                   // one marks mesh per boat object (pinned)

static std::string KmPath() { return g_folder + "\\kill-marks.ini"; }
static void KmLoad() {                                                 // lines "PT-109=planes,ships,shore"
  memset(g_kmCount, 0, sizeof g_kmCount);
  FILE* f = fopen(KmPath().c_str(), "rb");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char name[64]; int a = 0, b = 0, c = 0;
    if (sscanf(line, " %63[^=]=%d,%d,%d", name, &a, &b, &c) != 4) continue;
    for (int i = 0; i < kKillBoatCount; i++)
      if (!strcmp(name, kKillBoats[i].boat)) { g_kmCount[i][0] = (std::max)(a, 0); g_kmCount[i][1] = (std::max)(b, 0); g_kmCount[i][2] = (std::max)(c, 0); }
  }
  fclose(f);
}
static void KmSave() {
  std::string tmp = KmPath() + ".tmp";
  FILE* f = fopen(tmp.c_str(), "wb");
  if (!f) { Log("kill marks: could not write kill-marks.ini (error %lu)", GetLastError()); return; }
  fputs("; kill markings per boat: planes shot down, ships sunk, shore targets destroyed (the mod writes this file)\r\n", f);
  for (int i = 0; i < kKillBoatCount; i++)
    if (g_kmCount[i][0] || g_kmCount[i][1] || g_kmCount[i][2]) fprintf(f, "%s=%d,%d,%d\r\n", kKillBoats[i].boat, g_kmCount[i][0], g_kmCount[i][1], g_kmCount[i][2]);
  fclose(f);
  if (!MoveFileExA(tmp.c_str(), KmPath().c_str(), MOVEFILE_REPLACE_EXISTING)) Log("kill marks: could not replace kill-marks.ini (error %lu)", GetLastError());
}

static bool KmMaterial() {                                             // the shared material; made again if a scene unload took it
  if (g_km.mat && Alive(g_km.mat)) return true;
  if (g_km.matPin) { Unpin(g_km.matPin); g_km.matPin = nullptr; }
  g_km.mat = nullptr;
  if (!Alive(g_km.tex)) { Log("kill marks: the marks picture was unloaded; kill markings are off"); g_km.on = false; return false; }
  const char* used = nullptr;
  for (const char* n : { "Legacy Shaders/Transparent/Cutout/VertexLit", "Sprites/Default", "Unlit/Transparent" })
    if (!g_km.mat && (g_km.mat = MaterialFromShader(n))) used = n;
  if (!g_km.mat) { Log("kill marks: none of the see-through shaders is in this build; kill markings are off"); g_km.on = false; return false; }
  g_km.matPin = Pin(g_km.mat);
  void* mk = FindClass("UnityEngine", "Material"); void* uo = FindClass("UnityEngine", "Object");
  void* setTex = mk ? FindMethod(mk, "SetTexture", 2, "System.String", "UnityEngine.Texture") : nullptr;
  void* setFloat = mk ? FindMethod(mk, "SetFloat", 2, "System.String", "System.Single") : nullptr;
  void* hide = uo ? FindMethod(uo, "set_hideFlags", 1) : nullptr;
  if (setTex) { void* a[2] = { il2cpp_string_new("_MainTex"), g_km.tex }; Invoke(setTex, g_km.mat, a); }
  float cut = 0.5f;
  if (setFloat) { void* a[2] = { il2cpp_string_new("_Cutoff"), &cut }; Invoke(setFloat, g_km.mat, a); }
  int keep = 32;                                                       // HideFlags.DontUnloadUnusedAsset
  if (hide) { void* a[1] = { &keep }; Invoke(hide, g_km.mat, a); }
  static int told = 0;
  if (++told <= 3) Log("kill marks: material made (shader %s%s)", used, setTex ? "" : "; Material.SetTexture missing, the marks may be blank");
  return true;
}
static bool KmApi() {
  if (g_km.tried) return g_km.on;
  g_km.tried = true;
  auto type = [](void* k) { void* t = k ? il2cpp_type_get_object(il2cpp_class_get_type(k)) : nullptr; if (t) Pin(t); return t; };
  void* mf = FindClass("UnityEngine", "MeshFilter"); void* mr = FindClass("UnityEngine", "MeshRenderer"); void* rk = FindClass("UnityEngine", "Renderer");
  g_km.goCls = FindClass("UnityEngine", "GameObject");
  g_km.goCtor = g_km.goCls ? FindMethod(g_km.goCls, ".ctor", 1, "System.String") : nullptr;
  g_km.addComp = g_km.goCls ? FindMethod(g_km.goCls, "AddComponent", 1, "System.Type") : nullptr;
  g_km.mfType = type(mf); g_km.mrType = type(mr); g_km.slotType = type(FindClass("Core.Customization", "CustomPatternSlot"));
  g_km.meshCls = FindClass("UnityEngine", "Mesh");
  g_km.meshCtor = g_km.meshCls ? FindMethod(g_km.meshCls, ".ctor", 0) : nullptr;
  g_km.clear = g_km.meshCls ? FindMethod(g_km.meshCls, "Clear", 0) : nullptr;
  g_km.setVerts = g_km.meshCls ? FindMethod(g_km.meshCls, "set_vertices", 1) : nullptr;
  g_km.setUv = g_km.meshCls ? FindMethod(g_km.meshCls, "set_uv", 1) : nullptr;
  g_km.setTris = g_km.meshCls ? FindMethod(g_km.meshCls, "set_triangles", 1) : nullptr;
  g_km.normals = g_km.meshCls ? FindMethod(g_km.meshCls, "RecalculateNormals", 0) : nullptr;
  g_km.bounds = g_km.meshCls ? FindMethod(g_km.meshCls, "RecalculateBounds", 0) : nullptr;
  g_km.setMesh = mf ? FindMethod(mf, "set_sharedMesh", 1) : nullptr;
  g_km.setMat = rk ? FindMethod(rk, "set_sharedMaterial", 1) : nullptr;
  g_km.setShadows = rk ? FindMethod(rk, "set_shadowCastingMode", 1) : nullptr;
  { void* uo = FindClass("UnityEngine", "Object"); g_km.destroy = uo ? FindMethod(uo, "Destroy", 1) : nullptr; }
  g_km.v3 = FindClass("UnityEngine", "Vector3"); g_km.v2 = FindClass("UnityEngine", "Vector2"); g_km.i32 = FindClass("System", "Int32");
  g_km.arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  bool ok = g_km.goCtor && g_km.addComp && g_km.mfType && g_km.mrType && g_km.slotType && g_km.meshCtor && g_km.clear && g_km.setVerts && g_km.setUv &&
            g_km.setTris && g_km.normals && g_km.bounds && g_km.setMesh && g_km.setMat && g_km.v3 && g_km.v2 && g_km.i32 && g_km.arrayNew && m_setParent && g_cm.inChildren;
  if (!ok) { Log("kill marks: a Unity lookup failed (GameObject %p/%p, Mesh %p/%p/%p/%p, MeshFilter %p, set_sharedMaterial %p); kill markings are off",
                 g_km.goCtor, g_km.addComp, g_km.meshCtor, g_km.setVerts, g_km.setUv, g_km.setTris, g_km.setMesh, g_km.setMat); return false; }
  void* tex = nullptr; std::string det;
  int r = LoadPngTex(g_folder + "\\models\\killmarks.png", &tex, &det);
  if (r) { Log("kill marks: models\\killmarks.png %s; kill markings are off", r == 1 ? "is missing" : "could not be loaded"); return false; }
  g_km.tex = tex;
  if (!KmMaterial()) return false;
  KmLoad();
  g_km.on = true;
  return true;
}

static int KmBoatIndex(void* slot) {                                   // kKillBoats index of a camo slot's boat, or -1
  int b = CamoBoatOf(*(void**)((char*)slot + g_camo.offPatterns));
  if (b < 0) return -1;
  for (int i = 0; i < kKillBoatCount; i++) if (!strcmp(kCamoBoats[b].boat, kKillBoats[i].boat)) return i;
  return -1;
}
static void* KmNewArray(void* cls, int n) { return g_km.arrayNew(cls, (size_t)n); }
static void KmBuild(KmLive& m) {                                       // the mesh for this boat's current tallies
  const KillMarkBoat& kb = kKillBoats[m.boat];
  std::vector<float> v, uv; std::vector<int> tri;
  int shown[3] = {};
  for (int r = 0; r < kKillRows; r++) {
    int n = g_kmCount[m.boat][r], tens = (std::min)(n / 10, kKillCols), ones = (std::min)(n % 10, kKillCols - tens);
    for (int c = 0; c < tens + ones; c++) {
      const KillMarkSpot& sp = kb.spot[r * kKillCols + c];
      if (!sp.ok) continue;
      float u0 = r / 3.0f, v0 = c < tens ? 0.5f : 0.0f;                  // killmarks.png: columns plane, ship, gun; gold row on top
      for (int side = 0; side < 2; side++) {
        int base = (int)v.size() / 3;
        for (const KillMarkCorner& k : sp.c[side]) { v.insert(v.end(), { k.x, k.y, k.z }); uv.insert(uv.end(), { u0 + k.u / 3.0f, v0 + k.v / 2.0f }); }
        tri.insert(tri.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
      }
      shown[r]++;
    }
  }
  Invoke(g_km.clear, m.mesh);
  void* va = KmNewArray(g_km.v3, (int)v.size() / 3); void* ua = KmNewArray(g_km.v2, (int)uv.size() / 2); void* ta = KmNewArray(g_km.i32, (int)tri.size());
  if (!va || !ua || !ta) return;
  if (!v.empty()) { memcpy(ArrData(va), v.data(), v.size() * 4); memcpy(ArrData(ua), uv.data(), uv.size() * 4); memcpy(ArrData(ta), tri.data(), tri.size() * 4); }
  void* a[1] = { va }; Invoke(g_km.setVerts, m.mesh, a);
  a[0] = ua; Invoke(g_km.setUv, m.mesh, a);
  a[0] = ta; Invoke(g_km.setTris, m.mesh, a);
  Invoke(g_km.normals, m.mesh); Invoke(g_km.bounds, m.mesh);
  DLog("kill marks: %s shows %d plane, %d ship and %d shore marks", kb.boat, shown[0], shown[1], shown[2]);
}

// Called after every camo pick (wm_hull_camo.h): the marks mesh under the boat's hull node, made once per boat object.
static void KillMarksAttach(void* slot) {
  if (!KmApi()) return;
  for (auto it = g_kmLive.begin(); it != g_kmLive.end();) {            // boats gone with their scene
    if (Alive(it->slot) && Alive(it->go)) { ++it; continue; }
    for (void* p : it->pins) Unpin(p);
    it = g_kmLive.erase(it);
  }
  if (!KmMaterial()) return;
  for (KmLive& m : g_kmLive) if (m.slot == slot) { KmBuild(m); return; }
  static std::set<void*> failed;                                       // compared only, never followed
  if (failed.count(slot)) return;
  int boat = KmBoatIndex(slot);
  if (boat < 0) return;
  void* craft = EngagementInstance() ? PlayerCraft() : nullptr;        // in battle only the player's own boat (not call-ins)
  if (craft) {
    void* tr = Invoke(m_getTransform, slot); void* ctr = Invoke(m_getTransform, craft);
    bool under = false;
    for (int up = 0; tr && up < 32 && !under; up++) { under = tr == ctr; tr = Invoke(m_getParent, tr); }
    if (!under) { failed.insert(slot); return; }
  }
  void* holder = *(void**)((char*)slot + g_cm.offHolder);
  void* hgo = Alive(holder) ? Invoke(m_getGameObject, holder) : nullptr;
  bool typed = true, recursive = true, inactive = true, reverse = false;
  void* a6[6] = { g_cm.rendererType, &typed, &recursive, &inactive, &reverse, nullptr };
  void* arr = hgo ? Invoke(g_cm.inChildren, hgo, a6) : nullptr;
  void* node = nullptr;
  for (int k = 0; k < ArrLen(arr) && !node; k++)
    if (void* r = ArrAt(arr, k)) if (NameOf(r) == kKillBoats[boat].node) node = Invoke(m_getTransform, r);
  if (!node) { failed.insert(slot); Log("kill marks: no '%s' under the %s's camo holder; no marks on it", kKillBoats[boat].node, kKillBoats[boat].boat); return; }
  for (KmLive& m : g_kmLive)                                           // a second slot on the same hull
    if (Alive(m.go) && Invoke(m_getParent, Invoke(m_getTransform, m.go)) == node) { KmBuild(m); return; }
  void* go = il2cpp_object_new(g_km.goCls);
  void* na[1] = { il2cpp_string_new("WM kill marks") }; Invoke(g_km.goCtor, go, na);
  void* tr = Invoke(m_getTransform, go);
  if (g_setParentTwoArgs) { bool keep = false; void* pa[2] = { node, &keep }; Invoke(m_setParent, tr, pa); }
  else { void* pa[1] = { node }; Invoke(m_setParent, tr, pa); }        // a fresh object sits at the origin: local = world
  void* ca[1] = { g_km.mfType }; void* mf = Invoke(g_km.addComp, go, ca);
  ca[0] = g_km.mrType; void* mr = Invoke(g_km.addComp, go, ca);
  void* mesh = il2cpp_object_new(g_km.meshCls); Invoke(g_km.meshCtor, mesh);
  if (!mf || !mr || !Alive(mesh)) {
    failed.insert(slot);
    if (g_km.destroy) { void* da[1] = { go }; Invoke(g_km.destroy, nullptr, da); }
    Log("kill marks: could not build the marks object for the %s", kKillBoats[boat].boat); return;
  }
  void* ma[1] = { mesh }; Invoke(g_km.setMesh, mf, ma);
  ma[0] = g_km.mat; Invoke(g_km.setMat, mr, ma);
  if (g_km.setShadows) { int off = 0; void* sa[1] = { &off }; Invoke(g_km.setShadows, mr, sa); }
  KmLive m{ slot, go, mesh, boat, { Pin(slot), Pin(go), Pin(mesh) } };
  KmBuild(m);
  g_kmLive.push_back(m);
  static int told = 0;
  if (++told <= 6) Log("kill marks: hung under the %s's hull (%d planes, %d ships, %d shore targets)", kKillBoats[boat].boat,
                       g_kmCount[boat][0], g_kmCount[boat][1], g_kmCount[boat][2]);
}

// A kill the game credits to a gun (WeaponSystem / CrewMember.creditKill): the tally of the player's boat goes up.
static void KillMarkCredit(void* mount, void* killed) {
  if (!mount || !killed || IsRemoteMount(mount) || IsHiddenMount(mount) || !OnPlayerBoat(mount) || !KmApi()) return;
  void* craft = PlayerCraft();
  if (killed == craft) return;
  static void* recent[32] = {}; static DWORD recentAt[32] = {}; static int at = 0;   // every gunner and the helm get credit
  DWORD now = GetTickCount();                                          // for one kill; pooled planes die again later
  for (int i = 0; i < 32; i++) if (recent[i] == killed && now - recentAt[i] < 3000) return;
  recent[at] = killed; recentAt[at] = now; at = (at + 1) % 32;
  static void *planeK = FindClass("Core", "Plane"), *shipK = FindClass("Core", "Ship"), *groundK = FindClass("Core", "GroundEntity");
  int row = -1;                                                        // GroundEntity: shore guns and positions (TargetEntity is not)
  for (void* p = il2cpp_object_get_class(killed); p && row < 0; p = il2cpp_class_get_parent(p))
    row = p == planeK ? 0 : p == shipK ? 1 : p == groundK ? 2 : -1;
  if (row < 0) return;
  void* cgo = craft ? Invoke(m_getGameObject, craft) : nullptr;
  bool typed = true, recursive = true, inactive = true, reverse = false;
  void* a6[6] = { g_km.slotType, &typed, &recursive, &inactive, &reverse, nullptr };
  void* slots = cgo ? Invoke(g_cm.inChildren, cgo, a6) : nullptr;
  int boat = -1; void* slot = nullptr;
  for (int k = 0; k < ArrLen(slots) && boat < 0; k++) if ((slot = ArrAt(slots, k))) boat = KmBoatIndex(slot);
  if (boat < 0) { DLog("kill marks: a kill, but the player's boat is none of the six; not counted"); return; }
  int was = g_kmCount[boat][row]++;
  KmSave();
  Log("kill marks: %s %s %d -> %d (%s, %s)", kKillBoats[boat].boat, kKillRowName[row], was, was + 1, StripTags(NameOf(killed)).c_str(),
      il2cpp_class_get_name(il2cpp_object_get_class(killed)));
  for (KmLive& m : g_kmLive) if (m.boat == boat && Alive(m.slot) && Alive(m.go)) KmBuild(m);
}
static void SafeKillMarkCredit(void* mount, void* killed) {
  static int faults = 0;
  if (faults >= 5) return;
  __try { KillMarkCredit(mount, killed); }
  __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("kill marks: faults while counting kills; kill counting is off"); }
}

// Per save (wm_per_save.h): the live file goes with each save as saves\<hash>.kill and comes back on load. A save made
// before kill markings has none: the tallies start at zero.
static std::string KillMarksSidecar(unsigned long long h) { char b[32]; snprintf(b, sizeof b, "\\%016llx.kill", h); return SavesDir() + b; }
static void KillMarksAfterSave(unsigned long long h) {
  if (FileExists(KmPath()) && !CopyAtomic(KmPath(), KillMarksSidecar(h))) Log("kill marks: could not write %s (error %lu)", KillMarksSidecar(h).c_str(), GetLastError());
}
static void KillMarksReset(const char* why) {                          // a new campaign starts with clean hulls
  bool had = FileExists(KmPath());
  if (had && !DeleteFileA(KmPath().c_str())) Log("kill marks: %s: could not clear kill-marks.ini (error %lu)", why, GetLastError());
  memset(g_kmCount, 0, sizeof g_kmCount);
  if (g_km.on) for (KmLive& m : g_kmLive) if (Alive(m.slot) && Alive(m.go)) KmBuild(m);
  if (had) Log("kill marks: %s: tallies start at zero", why);
}
static void KillMarksAfterLoad(unsigned long long h, const std::string& name) {
  std::string side = KillMarksSidecar(h);
  bool ok = FileExists(side) ? CopyAtomic(side, KmPath()) : (DeleteFileA(KmPath().c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND);
  if (!ok) Log("kill marks: loaded %s, but kill-marks.ini could not be switched (error %lu)", name.c_str(), GetLastError());
  else Log("kill marks: loaded %s: %s", name.c_str(), FileExists(side) ? "its tallies are back" : "no tallies with it, starting at zero");
  if (g_km.on) { KmLoad(); for (KmLive& m : g_kmLive) if (Alive(m.slot) && Alive(m.go)) KmBuild(m); }
}
