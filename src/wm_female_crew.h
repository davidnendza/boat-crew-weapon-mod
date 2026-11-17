// female crew (looks, stored gender, gear flash, names) and crew id (MOD_CREWID) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- female crew ----
// A crew member's look is built on the portrait rigs only: CrewPortraiter.showUp runs CrewMemberVisual.initVisuals (face
// parts), CrewMemberGear.initGear (hats; a removeHair hat picks hair[0], Hair_Bald), then CrewMemberVisual.combineMesh.
// That merges every active SkinnedMeshRenderer that is a DIRECT child of skinnedMeshHolder (BaseCharacter), on body[0]'s
// layer and without a VisualReactor, into a NEW Mesh on the character (CrewCharacter.combinedMesh; rebuilt only while
// dirtyMesh is set). MeshExtension.mergeMeshes copies the vertices raw, so the mesh is in the parts' shared mesh space.
// The crew on the boat, overboard and co-op copies take that mesh by pointer (CrewMember.checkMesh ->
// setByMeshByCharacter), so a change made to it in place shows everywhere; the portrait photo is taken a frame later.
// combineMesh (16 plain bytes) is detoured:
//  * before it, for a woman: facial hair 5+ becomes 0..4 (the shaven entries) and the pipe (special1 4) goes; the stock
//    hair is switched off and 'WM female hair' on: a copy of the rig's Hair_1 renderer under BaseCharacter whose mesh
//    comes from models\female_hair_<style>.bchair (style = selectedHair % 4, so it is saved; victory rolls become the
//    bun under H_Helmet / H_HelmetTalker, which keep the hair). Under a hat that took the hair (Hair_Bald on with a
//    non-bald roll, or H_HelmetBlast, the only removeHair hat) the copy stays off. For a man on the same rig (rigs are
//    reused) the copy is switched off, and a mesh we reshaped is rebuilt.
//  * after it: the new combined mesh is reshaped by the 'v2' field (deform_female.py field(): bust, waist, hips,
//    shoulders, slimmer arms) in the BaseCharacter frame, one field for body and clothes alike, so they stay together.
//    The vertices that moved get new normals, welded by position.
// Who is a woman: a first name (up to its first space) in kFemFirst, else FNV-1a of the whole name (UTF-8) % 100 <
// female_crew, decided ONCE per crew member and then stored (CrewCharacter+0x103, the MOD_FEMALE save key and the co-op
// typeHair channel; see "stored gender" below), so a rename or a later female_crew change keeps it. "UNNAMED" (the
// game's name when no NameImporter is loaded) is a man. New crew: CharacterGenerator.generateCharacter is detoured
// (FemGenName) so female_crew % of them are women; existing crew keep their names.
//  * bra: models\female_bra.bcbra (tools\make_female_bra_assets.py), weighted to the Chest/Shoulder bones, stored in mesh
//    space BEFORE the v2 field, so FemAfter moves it onto the bust with the body. A second copy of Hair_1 ('WM female
//    bra') carries it, switched on before the bake only for a bare chest (FemBra). A change of that state forces a
//    rebuild.
//  * lips: the mouth part's vertices get their UVs moved to an empty 32x32 block of the crew atlas that the mod fills
//    with a muted rose DXT5 block on the GPU (FemLipPaint, a Graphics.CopyTexture icall). Without the icall: an
//    untinted pale peach patch.
// shortcut: the Flasher and Body Outline renderers keep the stock 'Body' mesh; reshape them too if the outline shows.
// The face changes (facial hair, pipe) stay in the save.
// Gear flash: a gear change in the loadout hides the stock parts on women at once (FemGearHide, see "the gear flash").
struct FemApi {
  int offCm, offHolder, offHair, offBodyMesh, offBodyMat, offBody;        // CrewMemberVisual
  int offName, offNameCustom, offSelHair, offSelFacial, offSelSpecial1, offCombined, offDirtyMesh;   // CrewCharacter
  void *selFacial, *selSpecial1;                                           // CrewMemberVisual.selectFacialHair / selectSpecial1(int)
  void *meshCls, *meshCtor, *getVerts, *setVerts, *getNormals, *setNormals, *setUv, *getTris, *setTris, *getBw, *setBw, *getBind, *setBind, *bounds, *vcount;
  void *bsCount, *bsName, *bsFrame;                                        // optional: the Jawline_Weak bake
  void *v3Cls, *v2Cls, *intCls, *bwCls;
  void *goTransform, *goActive, *childCount, *getChild, *getComponent, *smrType, *reactorType, *smrGetMesh, *smrSetMesh, *hide, *destroy;
  void* (*arrayNew)(void*, size_t);
  void *getUv;                                                             // Mesh.get_uv (lips)
  void *rGetMat, *matMainTex, *texW, *texH, *texMips, *texFmt, *tex2dCls, *tex2dCtor, *texLoadRaw, *texApply, *copySupport;   // lip paint (optional)
  void (*copyRegion)(void*, int, int, int, int, int, int, void*, int, int, int, int);   // Graphics.CopyTexture_Region_Injected
};
static FemApi g_fa = {};
static bool g_femOn = false, g_femTried = false;
typedef void (*fn_combine)(void*, void*);
static fn_combine g_combineOrig = nullptr;
static const char* kFemStyle[4] = { "bob", "bun", "ponytail", "rolls" };
static void* g_femMesh[4] = {}; static void* g_femMeshPin[4] = {}; static bool g_femMeshTried[4] = {};
struct FemHair { void* go; void* pin; int style; };                   // the renderer is fetched from go each time (its wrapper is not pinned)
static std::unordered_map<void*, FemHair> g_femHair;                       // the hair copy per CrewMemberVisual (portrait rig)
struct FemMesh { void* native; bool bra; bool rose; };                    // a combined mesh we reshaped: its native object, bra in it, rose lips
static std::unordered_map<void*, FemMesh> g_femDone;                       // combined meshes we reshaped
static int g_femBuilt = 0, g_femCalls = 0;                                 // women reshaped this session, combineMesh calls seen
static int g_femStyle = -1; static const char* g_femWhy = "";             // the hair the last FemBefore chose, for FemAfter's line
static void* g_femBraMesh = nullptr; static void* g_femBraMeshPin = nullptr; static bool g_femBraTried = false;
static std::unordered_map<void*, FemHair> g_femBra;                        // the bra copy per CrewMemberVisual (style 0: mesh set)
static bool g_femBraOn = false; static const char* g_femBraWhy = "";       // the last FemBefore's bra choice, for FemAfter
static int g_femBras = 0, g_femLips = 0;                                   // women baked with a bra / with recoloured lips
static void* g_femLipTex = nullptr; static bool g_femLipTried = false;     // the 32x32 rose DXT5 block (pinned for the session)
static int g_femLipPaints = 0; static bool g_femLipFail = false;           // atlas paints done; painting found impossible

static bool FemCall(void* m, void* obj, void** args = nullptr) { void* exc = nullptr; il2cpp_runtime_invoke(m, obj, args, &exc); return !exc; }
static bool FemActive(void* go) { void* r = go ? Invoke(g_fa.goActive, go) : nullptr; return r && *(bool*)((char*)r + 0x10); }
static void FemSetActive(void* go, bool on) { void* a[1] = { &on }; Invoke(m_setActive, go, a); }
static void* FemArray(void* cls, size_t n, const void* data, size_t elem) {
  void* a = g_fa.arrayNew(cls, n);
  if (a && data) memcpy(ArrData(a), data, n * elem);
  return a;
}
// A mesh we reshaped is known by its wrapper AND its native object, so a new Mesh that reuses a collected wrapper's
// address is not taken for one of ours.
static bool FemDone(void* m) { auto it = g_femDone.find(m); return it != g_femDone.end() && Alive(m) && *(void**)((char*)m + 0x10) == it->second.native; }
static void FemPrune() {
  for (auto it = g_femDone.begin(); it != g_femDone.end();) { void* m = it->first; if (!Alive(m) || *(void**)((char*)m + 0x10) != it->second.native) it = g_femDone.erase(it); else ++it; }
  for (auto* map : { &g_femHair, &g_femBra })
    for (auto it = map->begin(); it != map->end();) { if (!Alive(it->first) || !Alive(it->second.go)) { Unpin(it->second.pin); it = map->erase(it); } else ++it; }
}

// The lip paint lookups, all optional: without them the lips use the pale peach patch. The 12-argument
// Graphics.CopyTexture is stripped from the managed side, but UnityPlayer registers its icall
// (UnityEngine.Graphics::CopyTexture_Region_Injected, in UnityPlayer.dll). The managed 6-argument
// overload shows the calling convention: the textures go in as their native objects (wrapper +0x10), the rest as ints.
static void FemLipApi() {
  FemApi& a = g_fa;
  void* rk = FindClass("UnityEngine", "Renderer"); a.rGetMat = rk ? FindMethod(rk, "get_sharedMaterial", 0) : nullptr;
  void* mc = FindClass("UnityEngine", "Material"); a.matMainTex = mc ? FindMethod(mc, "get_mainTexture", 0) : nullptr;
  void* tx = FindClass("UnityEngine", "Texture");
  a.texW = tx ? FindMethod(tx, "get_width", 0) : nullptr; a.texH = tx ? FindMethod(tx, "get_height", 0) : nullptr; a.texMips = tx ? FindMethod(tx, "get_mipmapCount", 0) : nullptr;
  void* t2 = FindClass("UnityEngine", "Texture2D"); a.tex2dCls = t2;
  if (t2) {
    a.texFmt = FindMethod(t2, "get_format", 0);
    a.texLoadRaw = FindMethod(t2, "LoadRawTextureData", 2, "System.IntPtr"); a.texApply = FindMethod(t2, "Apply", 2);
    void* iter = nullptr; void* m;                                         // Texture2D(int width, int height, TextureFormat, bool mipChain)
    while ((m = il2cpp_class_get_methods(t2, &iter)))
      if (!strcmp(il2cpp_method_get_name(m), ".ctor") && il2cpp_method_get_param_count(m) == 4 &&
          TypeName(il2cpp_method_get_param(m, 2)) == "UnityEngine.TextureFormat" && TypeName(il2cpp_method_get_param(m, 3)) == "System.Boolean") { a.tex2dCtor = m; break; }
  }
  void* si = FindClass("UnityEngine", "SystemInfo"); a.copySupport = si ? FindMethod(si, "get_copyTextureSupport", 0) : nullptr;
  auto resolve = (void* (*)(const char*))GetProcAddress(g_ga, "il2cpp_resolve_icall");
  a.copyRegion = resolve ? (decltype(a.copyRegion))resolve("UnityEngine.Graphics::CopyTexture_Region_Injected") : nullptr;
  std::string missing;
  const std::pair<const char*, bool> need[] = { { "Mesh.get_uv", a.getUv != nullptr }, { "Renderer.get_sharedMaterial", a.rGetMat != nullptr },
    { "Material.get_mainTexture", a.matMainTex != nullptr }, { "Texture.width/height", a.texW && a.texH }, { "Texture2D(int,int,TextureFormat,bool)", a.tex2dCtor != nullptr },
    { "Texture2D.LoadRawTextureData(IntPtr,int)", a.texLoadRaw != nullptr }, { "Texture2D.Apply(bool,bool)", a.texApply != nullptr },
    { "icall Graphics::CopyTexture_Region_Injected", a.copyRegion != nullptr } };
  for (auto& n : need) if (!n.second) missing += std::string(" ") + n.first;
  if (!missing.empty()) { g_femLipFail = true; Log("female crew: lip paint unavailable (missing%s); women's lips use the pale peach patch", missing.c_str()); }
  else if (a.copySupport) {
    int cs = Unbox32(Invoke(a.copySupport, nullptr));
    if (!(cs & 1)) { g_femLipFail = true; Log("female crew: this graphics device cannot copy textures (copyTextureSupport %d); women's lips use the pale peach patch", cs); }
  }
}

static bool FemApiReady() {
  FemApi& a = g_fa;
  void* cmv = FindClass("Core", "CrewMemberVisual"); void* cc = FindClass("Core", "CrewCharacter");
  a.offCm = FieldOffset(cmv, "cM"); a.offHolder = FieldOffset(cmv, "skinnedMeshHolder"); a.offHair = FieldOffset(cmv, "hair"); a.offBodyMesh = FieldOffset(cmv, "bodyMesh");
  a.offBodyMat = FieldOffset(cmv, "bodyMaterial"); a.offBody = FieldOffset(cmv, "body");
  a.offName = FieldOffset(cc, "name"); a.offNameCustom = FieldOffset(cc, "nameCustom"); a.offSelHair = FieldOffset(cc, "selectedHair"); a.offSelFacial = FieldOffset(cc, "selectedFacialHair");
  a.offSelSpecial1 = FieldOffset(cc, "selectedSpecial1"); a.offCombined = FieldOffset(cc, "combinedMesh"); a.offDirtyMesh = FieldOffset(cc, "dirtyMesh");
  a.selFacial = cmv ? FindMethod(cmv, "selectFacialHair", 1) : nullptr; a.selSpecial1 = cmv ? FindMethod(cmv, "selectSpecial1", 1) : nullptr;
  void* mk = FindClass("UnityEngine", "Mesh"); a.meshCls = mk;
  if (mk) {
    a.meshCtor = FindMethod(mk, ".ctor", 0); a.getVerts = FindMethod(mk, "get_vertices", 0); a.setVerts = FindMethod(mk, "set_vertices", 1);
    a.getNormals = FindMethod(mk, "get_normals", 0); a.setNormals = FindMethod(mk, "set_normals", 1); a.setUv = FindMethod(mk, "set_uv", 1);
    a.getTris = FindMethod(mk, "get_triangles", 0); a.setTris = FindMethod(mk, "set_triangles", 1); a.getBw = FindMethod(mk, "get_boneWeights", 0);
    a.setBw = FindMethod(mk, "set_boneWeights", 1); a.getBind = FindMethod(mk, "get_bindposes", 0); a.setBind = FindMethod(mk, "set_bindposes", 1);
    a.bounds = FindMethod(mk, "RecalculateBounds", 0); a.vcount = FindMethod(mk, "get_vertexCount", 0);
    a.bsCount = FindMethod(mk, "get_blendShapeCount", 0); a.bsName = FindMethod(mk, "GetBlendShapeName", 1); a.bsFrame = FindMethod(mk, "GetBlendShapeFrameVertices", 5);
    a.getUv = FindMethod(mk, "get_uv", 0);
  }
  a.v3Cls = FindClass("UnityEngine", "Vector3"); a.v2Cls = FindClass("UnityEngine", "Vector2"); a.bwCls = FindClass("UnityEngine", "BoneWeight"); a.intCls = FindClass("System", "Int32");
  void* go = FindClass("UnityEngine", "GameObject"); a.goTransform = go ? FindMethod(go, "get_transform", 0) : nullptr; a.goActive = go ? FindMethod(go, "get_activeSelf", 0) : nullptr;
  void* tr = FindClass("UnityEngine", "Transform"); a.childCount = tr ? FindMethod(tr, "get_childCount", 0) : nullptr; a.getChild = tr ? FindMethod(tr, "GetChild", 1) : nullptr;
  void* comp = FindClass("UnityEngine", "Component"); a.getComponent = comp ? FindMethod(comp, "GetComponent", 1, "System.Type") : nullptr;
  void* smr = FindClass("UnityEngine", "SkinnedMeshRenderer"); a.smrType = smr ? il2cpp_type_get_object(il2cpp_class_get_type(smr)) : nullptr;
  a.smrGetMesh = smr ? FindMethod(smr, "get_sharedMesh", 0) : nullptr; a.smrSetMesh = smr ? FindMethod(smr, "set_sharedMesh", 1) : nullptr;
  void* vr = FindClass("Core", "VisualReactor"); a.reactorType = vr ? il2cpp_type_get_object(il2cpp_class_get_type(vr)) : nullptr;
  void* uo = FindClass("UnityEngine", "Object"); a.hide = uo ? FindMethod(uo, "set_hideFlags", 1) : nullptr;
  a.destroy = uo ? FindMethod(uo, "Destroy", 1, "UnityEngine.Object") : nullptr;   // optional: a failed hair copy is then only switched off
  a.arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  std::string missing;
  const std::pair<const char*, bool> need[] = {
    { "CrewMemberVisual.cM", a.offCm >= 0 }, { "CrewMemberVisual.skinnedMeshHolder", a.offHolder >= 0 }, { "CrewMemberVisual.hair", a.offHair >= 0 },
    { "CrewCharacter.name", a.offName >= 0 }, { "CrewCharacter.nameCustom", a.offNameCustom >= 0 }, { "CrewCharacter.selectedHair", a.offSelHair >= 0 },
    { "CrewCharacter.selectedFacialHair", a.offSelFacial >= 0 }, { "CrewCharacter.selectedSpecial1", a.offSelSpecial1 >= 0 },
    { "CrewCharacter.combinedMesh", a.offCombined >= 0 }, { "CrewCharacter.dirtyMesh", a.offDirtyMesh >= 0 },
    { "selectFacialHair", a.selFacial != nullptr }, { "selectSpecial1", a.selSpecial1 != nullptr }, { "Mesh..ctor", a.meshCtor != nullptr },
    { "Mesh.vertices", a.getVerts && a.setVerts }, { "Mesh.normals", a.getNormals && a.setNormals }, { "Mesh.set_uv", a.setUv != nullptr },
    { "Mesh.triangles", a.getTris && a.setTris }, { "Mesh.boneWeights", a.getBw && a.setBw }, { "Mesh.bindposes", a.getBind && a.setBind },
    { "Mesh.RecalculateBounds", a.bounds != nullptr }, { "Vector3/Vector2/BoneWeight/Int32", a.v3Cls && a.v2Cls && a.bwCls && a.intCls },
    { "GameObject.get_transform/get_activeSelf", a.goTransform && a.goActive }, { "Transform.get_childCount/GetChild", a.childCount && a.getChild },
    { "Component.GetComponent(Type)", a.getComponent != nullptr }, { "SkinnedMeshRenderer", a.smrType && a.smrGetMesh && a.smrSetMesh },
    { "il2cpp_array_new", a.arrayNew != nullptr }, { "Instantiate / SetParent", m_instantiate && m_setParent && m_setActive && m_getParent } };
  for (auto& n : need) if (!n.second) missing += std::string(" ") + n.first;
  if (!missing.empty()) { Log("female crew: missing%s; the crew stay stock", missing.c_str()); return false; }
  if (a.offBodyMesh < 0 || !a.bsCount || !a.bsName || !a.bsFrame || !a.vcount) { a.bsFrame = nullptr; Log("female crew: no blend shape access (bodyMesh +%d); the softer jaw is skipped", a.offBodyMesh); }
  if (!a.reactorType) DLog("female crew: Core.VisualReactor not found; the hair copy is not checked for one");
  if (!a.getUv) Log("female crew: Mesh.get_uv not found; the lips keep the stock colour");
  FemLipApi();
  DLog("female crew: CrewMemberVisual cM +0x%X skinnedMeshHolder +0x%X hair +0x%X bodyMesh +0x%X; CrewCharacter name +0x%X nameCustom +0x%X selectedHair +0x%X "
       "selectedFacialHair +0x%X selectedSpecial1 +0x%X combinedMesh +0x%X dirtyMesh +0x%X", a.offCm, a.offHolder, a.offHair, a.offBodyMesh, a.offName, a.offNameCustom,
       a.offSelHair, a.offSelFacial, a.offSelSpecial1, a.offCombined, a.offDirtyMesh);
  return true;
}

// The crew member behind a rig: CrewMemberVisual.cM is an ICarriesCharacter (CrewMember, CrewMemberOverboard); its
// Character getter is looked up per class.
static void* FemCharacter(void* cmv) {
  void* cm = *(void**)((char*)cmv + g_fa.offCm);
  if (!cm) return nullptr;
  static std::unordered_map<void*, void*> getters;
  void* k = il2cpp_object_get_class(cm);
  auto it = getters.find(k);
  if (it == getters.end()) {
    void* m = FindMethod(k, "get_Character", 0);
    it = getters.emplace(k, m).first;
    if (!m) Log("female crew: %s has no Character getter; its rigs stay stock", il2cpp_class_get_name(k));
  }
  return it->second ? Invoke(it->second, cm) : nullptr;
}
// A managed string (length +0x10, UTF-16 at +0x14) as UTF-8; "" for null.
static std::string FemUtf8(void* s) {
  int len = s ? *(int*)((char*)s + 0x10) : 0;
  if (len <= 0) return std::string();
  const wchar_t* w = (const wchar_t*)((char*)s + 0x14);
  int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
  std::string u(n > 0 ? n : 0, '\0');
  if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, len, &u[0], n, nullptr, nullptr);
  return u;
}
static std::string FemName(void* c) {
  void* s = *(void**)((char*)c + g_fa.offName);
  if (!s || *(int*)((char*)s + 0x10) <= 0) s = *(void**)((char*)c + g_fa.offNameCustom);
  return FemUtf8(s);
}
// Women's first names for new crew (US names of women born 1900-1925, plus a few rarer ones to match the game's odd
// men's names). None is in the game's Info/names (129 men's names) or Info/surnames, and
// none of the unisex names (Leslie, Lynn, Ora, Genoa, Ynez, Alex, Francis, Terry, Jay, Lavon, Clancy, Marion, Jean, ...).
// There is no Japanese names list in the game; Japanese crews use the same English lists, so this one table serves all.
static const char* const kFemFirst[] = {
  "Ada", "Adele", "Agnes", "Alberta", "Alice", "Alma", "Amelia", "Anna", "Annabelle", "Annie", "Antoinette",
  "Audrey", "Augusta", "Barbara", "Beatrice", "Bernice", "Bertha", "Betty", "Blanche", "Bonnie", "Catherine",
  "Cecilia", "Charlotte", "Clara", "Constance", "Cora", "Daisy", "Delia", "Della", "Dolores", "Dora", "Doris",
  "Dorothy", "Edith", "Edna", "Eleanor", "Elizabeth", "Ella", "Ellen", "Eloise", "Elsie", "Emma", "Esther", "Ethel",
  "Etta", "Eudora", "Eunice", "Eva", "Evangeline", "Evelyn", "Faye", "Fern", "Flora", "Florence", "Geraldine",
  "Gertrude", "Gladys", "Grace", "Greta", "Gwendolyn", "Harriet", "Hazel", "Helen", "Henrietta", "Hilda", "Ida",
  "Imogene", "Inez", "Irene", "Iris", "Isabel", "Ivy", "Josephine", "Juanita", "Julia", "June", "Katherine", "Laura",
  "Lillian", "Lois", "Loretta", "Lorraine", "Louise", "Lucille", "Lucy", "Mabel", "Madeline", "Mae", "Margaret",
  "Marguerite", "Marjorie", "Martha", "Mary", "Maude", "Maxine", "Mildred", "Minnie", "Miriam", "Muriel", "Myrtle",
  "Nadine", "Nancy", "Naomi", "Nellie", "Nora", "Norma", "Olive", "Opal", "Pauline", "Pearl", "Peggy", "Phyllis",
  "Priscilla", "Rachel", "Rita", "Rosalind", "Rose", "Rosemary", "Ruby", "Ruth", "Sadie", "Sarah", "Sophia",
  "Stella", "Sylvia", "Thelma", "Theodora", "Vera", "Verna", "Viola", "Violet", "Virginia", "Wanda", "Wilhelmina",
  "Winifred", "Zelda",
};
static const int kFemFirstN = (int)(sizeof kFemFirst / sizeof kFemFirst[0]);
// Exact match of a first name (s, n bytes) against kFemFirst; no allocation, it runs on every combine.
static bool FemFirstIsFemale(const char* s, size_t n) {
  if (!n) return false;
  for (const char* f : kFemFirst) if (f[0] == s[0] && !strncmp(f, s, n) && f[n] == '\0') return true;
  return false;
}
// The name rule: a woman's first name, else FNV-1a(name) % 100 < female_crew. Used to decide a crew member's gender the
// first time the mod sees them (then it is stored, see FemIsWoman) and to roll names for new crew.
static bool FemNameRule(const std::string& name) {
  if (FemPct() <= 0 || name.empty() || name == "UNNAMED") return false;
  size_t sp = name.find(' ');
  if (FemFirstIsFemale(name.data(), sp == std::string::npos ? name.size() : sp)) return true;
  uint32_t h = 2166136261u;                                                // FNV-1a, 32 bit
  for (unsigned char ch : name) { h ^= ch; h *= 16777619u; }
  return (int)(h % 100) < FemPct();
}

// ---- female crew, stored gender ----
// Crew can be renamed in game, and a rename must never change a crew member's gender.
// A rename (CharacterRename.handleRename) only writes CrewCharacter.nameCustom and the rule reads name (+0x18), so a
// rename alone never flips anyone. What the stored flag fixes is a later change of female_crew, peers with another
// female_crew, and co-op copies that have no name at all.
//  * The flag is one byte in CrewCharacter's padding at +0x103 (inSinkingBoat +0x100, dirty +0x101, dirtyMesh +0x102,
//    usedByMember +0x108; no game code writes 0x103..0x107, scanned): 0 = not decided yet, 1 = man, 2 = woman. A new
//    object is zeroed, so it starts undecided; the byte dies with the object. The offsets are checked at install.
//  * Decided once: new crew in the generateCharacter hook (FemGenName), else the first time the mod sees the crew
//    member with female_crew > 0 (load, first portrait, save, co-op) by the name rule, then kept.
//  * Saved: CrewCharacter.getParameters (the IDataObject vtable entry) gets an extra Parameter MOD_FEMALE = "1" (woman)
//    or "0" (man); setParameters (detoured; every DataBlock load goes through it: the save, the crew screen's Cancel,
//    co-op roster sends both ways, trades, Wave mode) reads it back. The game ignores the unknown key. Both hooks go in
//    even at female_crew 0, so a save made at 0% keeps the flags; at 0% nothing is decided and every body is stock.
//  * Co-op battle copies (CrewMemberFake): the owner's ObjectSync.sendCrewToServer puts the flag into the
//    CrewStatePersistent's typeHair (sent, never read by the game): (hair & 0xFFFF) | 0xBC << 16 | g << 24. The fake's
//    delayedSetup reads it back (with no marker: the name rule on the special code's name, which a rename keeps).
// So a crew member with a stored flag keeps it when female_crew changes; only new crew and crew never seen follow the
// new share. With female_crew 0 the flags are kept (and saved) but ignored.
struct FemGenderApi { int offName, offNameCustom, offG, offPName, offPValue; void* paramCls; void* (*arrayNew)(void*, size_t); };
static FemGenderApi g_fgd = { -1, -1, -1, -1, -1, nullptr, nullptr };
static SRWLOCK g_femGLock = SRWLOCK_INIT;                                  // the save may run off the game thread
struct FemGSeen { std::string name, custom; };
static std::unordered_map<void*, FemGSeen> g_femGSeen;                     // crew with a stored flag: names last seen (rename log)
struct FemGCounts { int created, createdW, loaded, loadedW, byName, byNameW, saved, savedW, coopOwner, coopName, renames, odd; };
static FemGCounts g_femGC = {};
static FemGCounts g_femGCLast = {};                                        // at the last summary
static uint8_t FemGGet(void* c) {
  if (g_fgd.offG <= 0 || !c) return 0;
  uint8_t g = *((uint8_t*)c + g_fgd.offG);
  if (g > 2) {
    AcquireSRWLockExclusive(&g_femGLock); int odd = ++g_femGC.odd; ReleaseSRWLockExclusive(&g_femGLock);
    if (odd <= 3) Log("female crew: crew member %p has %d in the stored gender byte (+0x%X); something else writes there, taken as not decided", c, g, g_fgd.offG);
    return 0;
  }
  return g;
}
static std::string FemGStr(void* c, int off) { return off > 0 && c ? FemUtf8(*(void**)((char*)c + off)) : std::string(); }
static std::string FemGName(void* c) { std::string n = FemGStr(c, g_fgd.offName); return n.empty() ? FemGStr(c, g_fgd.offNameCustom) : n; }
static std::string FemGShown(void* c) { std::string n = FemGStr(c, g_fgd.offNameCustom); return n.empty() ? FemGStr(c, g_fgd.offName) : n; }
// The names of a crew member with a stored flag, kept to notice a rename (nameCustom changed, name the same). A
// different name means another object at a reused address: recorded again, nothing logged.
static void FemGNote(void* c, bool check) {
  if (!c || g_fgd.offName <= 0) return;
  std::string n = FemGStr(c, g_fgd.offName), cu = FemGStr(c, g_fgd.offNameCustom);
  std::string was; int renames = 0;
  AcquireSRWLockExclusive(&g_femGLock);
  auto it = g_femGSeen.find(c);
  if (it == g_femGSeen.end()) { if (g_femGSeen.size() > 4096) g_femGSeen.clear(); g_femGSeen[c] = { n, cu }; }
  else if (!check || it->second.name != n) it->second = { n, cu };        // (re)stored, or another object at this address
  else if (check && it->second.custom != cu) { was = it->second.custom.empty() ? n : it->second.custom; it->second.custom = cu; renames = ++g_femGC.renames; }
  ReleaseSRWLockExclusive(&g_femGLock);
  if (renames && (renames <= 10 || renames % 50 == 0))
    Log("female crew: '%s' was renamed '%s'; still a %s (stored gender; %d renames seen this session)", was.c_str(), cu.empty() ? n.c_str() : cu.c_str(),
        *((uint8_t*)c + g_fgd.offG) == 2 ? "woman" : "man", renames);
}
static void FemGSet(void* c, uint8_t g) {
  if (g_fgd.offG <= 0 || !c || (g != 1 && g != 2)) return;
  *((uint8_t*)c + g_fgd.offG) = g;
  FemGNote(c, false);
}
static void FemGCount(int FemGCounts::* total, int FemGCounts::* women, bool woman, int* outTotal) {
  AcquireSRWLockExclusive(&g_femGLock);
  int t = ++(g_femGC.*total); if (woman && women) ++(g_femGC.*women);
  ReleaseSRWLockExclusive(&g_femGLock);
  if (outTotal) *outTotal = t;
}
// Who is a woman: the stored flag; else (female_crew > 0) the name rule, and the result is stored. An empty name (a
// co-op copy the owner's flag did not reach) is a man and stays undecided. female_crew 0: always stock.
static bool FemIsWoman(void* c, const std::string& name) {
  if (FemPct() <= 0) return false;
  uint8_t g = FemGGet(c);
  if (g) return g == 2;
  bool w = FemNameRule(name);
  if (g_fgd.offG > 0 && c && !name.empty()) {
    FemGSet(c, w ? 2 : 1);
    int t = 0; FemGCount(&FemGCounts::byName, &FemGCounts::byNameW, w, &t);
    if (t <= 5 || t % 100 == 0) Log("female crew: '%s' first seen with no stored gender: a %s by the name rule (female_crew=%d%%), stored (%d decided by name this session)",
                                     name.c_str(), w ? "woman" : "man", FemPct(), t);
  }
  return w;
}

// models\female_hair_<style>.bchair (tools\make_female_hair_assets.py): 'BCH1', uint32 nv, uint32 ntri, float pos[nv*3],
// nrm[nv*3] (mesh space, as Hair_1), uv[nv*2] (the hair colour patch), int32 tri[ntri*3] (Unity winding). Every vertex is
// 100% bone 3 (Head); the bindposes are the stock Hair_1 mesh's. Built once and kept for the session.
static void* FemHairMesh(int style, void* stockSmr) {
  if (g_femMeshTried[style]) return Alive(g_femMesh[style]) ? g_femMesh[style] : nullptr;
  g_femMeshTried[style] = true;
  std::string path = g_folder + "\\models\\female_hair_" + kFemStyle[style] + ".bchair";
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { Log("female crew: models\\female_hair_%s.bchair not found; women with that style keep the stock hair", kFemStyle[style]); return nullptr; }
  std::vector<unsigned char> d; unsigned char buf[65536]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
  fclose(f);
  uint32_t nv = 0, nt = 0;
  if (d.size() >= 12) { memcpy(&nv, &d[4], 4); memcpy(&nt, &d[8], 4); }
  size_t want = 12 + (size_t)nv * (12 + 12 + 8) + (size_t)nt * 12;
  if (d.size() < 12 || memcmp(&d[0], "BCH1", 4) || nv == 0 || nv >= 65535 || nt == 0 || nt > 200000 || d.size() != want) {
    Log("female crew: models\\female_hair_%s.bchair is not a hair file (%u vertices, %u triangles, %u bytes); that style is off", kFemStyle[style], nv, nt, (unsigned)d.size());
    return nullptr;
  }
  const unsigned char* pos = &d[12]; const unsigned char* nrm = pos + nv * 12; const unsigned char* uv = nrm + nv * 12; const int32_t* tri = (const int32_t*)(uv + nv * 8);
  for (size_t i = 0; i < (size_t)nt * 3; i++) if (tri[i] < 0 || (uint32_t)tri[i] >= nv) { Log("female crew: female_hair_%s.bchair has a triangle index out of range; that style is off", kFemStyle[style]); return nullptr; }
  void* mesh = il2cpp_object_new(g_fa.meshCls);
  if (!mesh || !FemCall(g_fa.meshCtor, mesh)) { Log("female crew: could not make a Mesh"); return nullptr; }
  struct BW { float w[4]; int i[4]; };                                     // UnityEngine.BoneWeight: 4 weights, then 4 bone indices
  std::vector<BW> bw(nv, BW{ { 1, 0, 0, 0 }, { 3, 0, 0, 0 } });           // bone 3 = Head
  void* a[1];
  bool ok = true;
  a[0] = FemArray(g_fa.v3Cls, nv, pos, 12); ok = ok && a[0] && FemCall(g_fa.setVerts, mesh, a);
  a[0] = FemArray(g_fa.v3Cls, nv, nrm, 12); ok = ok && a[0] && FemCall(g_fa.setNormals, mesh, a);
  a[0] = FemArray(g_fa.v2Cls, nv, uv, 8); ok = ok && a[0] && FemCall(g_fa.setUv, mesh, a);
  a[0] = FemArray(g_fa.intCls, (size_t)nt * 3, tri, 4); ok = ok && a[0] && FemCall(g_fa.setTris, mesh, a);
  a[0] = FemArray(g_fa.bwCls, nv, bw.data(), sizeof(BW)); ok = ok && a[0] && FemCall(g_fa.setBw, mesh, a);
  void* sm = stockSmr ? Invoke(g_fa.smrGetMesh, stockSmr) : nullptr;
  void* bp = Alive(sm) ? Invoke(g_fa.getBind, sm) : nullptr;
  a[0] = bp; ok = ok && bp && ArrLen(bp) == 27 && FemCall(g_fa.setBind, mesh, a);
  ok = ok && FemCall(g_fa.bounds, mesh);
  if (!ok) { Log("female crew: building the %s hair mesh failed (bindposes %d); that style is off", kFemStyle[style], ArrLen(bp)); return nullptr; }
  SetName(mesh, (std::string("WM female hair ") + kFemStyle[style]).c_str());
  if (g_fa.hide) { int keep = 32; void* h[1] = { &keep }; Invoke(g_fa.hide, mesh, h); }   // DontUnloadUnusedAsset
  g_femMeshPin[style] = Pin(mesh); g_femMesh[style] = mesh;
  Log("female crew: hair '%s' loaded (%u vertices, %u triangles)", kFemStyle[style], nv, nt);
  return mesh;
}

// models\female_bra.bcbra (tools\make_female_bra_assets.py): 'BCB1', uint32 nv, uint32 ntri, float pos[nv*3] (mesh
// space, BEFORE the v2 field: FemAfter moves it onto the bust), nrm[nv*3], uv[nv*2] (the untinted grey of the Underwear
// briefs), BoneWeight[nv] (Unity's layout: float w[4], int idx[4], into the 27 crew bones), int32 tri[ntri*3]. Built
// once and kept for the session; bindposes from the stock Hair_1 mesh (the same 27 as the body's).
static const float kFemBraU = 0.9510f, kFemBraV = 0.4530f;
static void* FemBraMesh(void* stockSmr) {
  if (g_femBraTried) return Alive(g_femBraMesh) ? g_femBraMesh : nullptr;
  g_femBraTried = true;
  std::string path = g_folder + "\\models\\female_bra.bcbra";
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { Log("female crew: models\\female_bra.bcbra not found; bare-chested women get no bra"); return nullptr; }
  std::vector<unsigned char> d; unsigned char buf[65536]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
  fclose(f);
  uint32_t nv = 0, nt = 0;
  if (d.size() >= 12) { memcpy(&nv, &d[4], 4); memcpy(&nt, &d[8], 4); }
  size_t want = 12 + (size_t)nv * (12 + 12 + 8 + 32) + (size_t)nt * 12;
  if (d.size() < 12 || memcmp(&d[0], "BCB1", 4) || nv == 0 || nv >= 65535 || nt == 0 || nt > 200000 || d.size() != want) {
    Log("female crew: models\\female_bra.bcbra is not a bra file (%u vertices, %u triangles, %u bytes); no bra", nv, nt, (unsigned)d.size());
    return nullptr;
  }
  struct BW { float w[4]; int i[4]; };
  const unsigned char* pos = &d[12]; const unsigned char* nrm = pos + nv * 12; const unsigned char* uv = nrm + nv * 12;
  std::vector<BW> bw(nv); memcpy(bw.data(), uv + nv * 8, (size_t)nv * sizeof(BW));
  std::vector<int32_t> tri((size_t)nt * 3); memcpy(tri.data(), uv + nv * 8 + (size_t)nv * sizeof(BW), tri.size() * 4);
  for (int32_t t : tri) if (t < 0 || (uint32_t)t >= nv) { Log("female crew: female_bra.bcbra has a triangle index out of range; no bra"); return nullptr; }
  for (const BW& b : bw) {
    float sum = b.w[0] + b.w[1] + b.w[2] + b.w[3];
    bool ok = sum > 0.99f && sum < 1.01f;
    for (int k = 0; k < 4; k++) if (b.i[k] < 0 || b.i[k] >= 27 || b.w[k] < 0) ok = false;
    if (!ok) { Log("female crew: female_bra.bcbra has a bad bone weight (bones 0..26, weights summing to 1); no bra"); return nullptr; }
  }
  void* mesh = il2cpp_object_new(g_fa.meshCls);
  if (!mesh || !FemCall(g_fa.meshCtor, mesh)) { Log("female crew: could not make a Mesh for the bra"); return nullptr; }
  void* a[1];
  bool ok = true;
  a[0] = FemArray(g_fa.v3Cls, nv, pos, 12); ok = ok && a[0] && FemCall(g_fa.setVerts, mesh, a);
  a[0] = FemArray(g_fa.v3Cls, nv, nrm, 12); ok = ok && a[0] && FemCall(g_fa.setNormals, mesh, a);
  a[0] = FemArray(g_fa.v2Cls, nv, uv, 8); ok = ok && a[0] && FemCall(g_fa.setUv, mesh, a);
  a[0] = FemArray(g_fa.intCls, tri.size(), tri.data(), 4); ok = ok && a[0] && FemCall(g_fa.setTris, mesh, a);
  a[0] = FemArray(g_fa.bwCls, nv, bw.data(), sizeof(BW)); ok = ok && a[0] && FemCall(g_fa.setBw, mesh, a);
  void* sm = stockSmr ? Invoke(g_fa.smrGetMesh, stockSmr) : nullptr;
  void* bp = Alive(sm) ? Invoke(g_fa.getBind, sm) : nullptr;
  a[0] = bp; ok = ok && bp && ArrLen(bp) == 27 && FemCall(g_fa.setBind, mesh, a);
  ok = ok && FemCall(g_fa.bounds, mesh);
  if (!ok) { Log("female crew: building the bra mesh failed (bindposes %d); no bra", ArrLen(bp)); return nullptr; }
  SetName(mesh, "WM female bra");
  if (g_fa.hide) { int keep = 32; void* h[1] = { &keep }; Invoke(g_fa.hide, mesh, h); }   // DontUnloadUnusedAsset
  g_femBraMeshPin = Pin(mesh); g_femBraMesh = mesh;
  Log("female crew: bra loaded (%u vertices, %u triangles)", nv, nt);
  return mesh;
}

static void* FemSmrOf(void* go) { void* t = go ? Invoke(g_fa.goTransform, go) : nullptr; void* a[1] = { g_fa.smrType }; return t ? Invoke(g_fa.getComponent, t, a) : nullptr; }
// The rig's hair (and bra) copy: Instantiate of the stock Hair_1 GameObject (only a SkinnedMeshRenderer, layer 25, read
// from the CrewMember prefab), made a direct child of BaseCharacter with Hair_1's local transform. Kept per rig, pinned.
// Its bones are the rig's (Instantiate keeps references outside the copied object); combineMesh uses body[0]'s anyway.
static FemHair* FemCopyObject(std::unordered_map<void*, FemHair>& map, const char* what, void* cmv, void* holder, void* src) {
  void* holderT = Invoke(g_fa.goTransform, holder);
  if (!holderT) return nullptr;
  auto it = map.find(cmv);
  if (it != map.end()) {
    FemHair& h = it->second;
    void* t = Alive(h.go) ? Invoke(g_fa.goTransform, h.go) : nullptr;
    if (t && Invoke(m_getParent, t) == holderT && FemSmrOf(h.go)) return &h;
    if (Alive(h.go)) FemSetActive(h.go, false);                            // never left on under another rig
    Unpin(h.pin); map.erase(it);                                           // a new rig at a reused address, or ours was destroyed
  }
  void* go = CloneObject(src);
  if (!go) { Log("female crew: could not copy Hair_1 for the %s", what); return nullptr; }
  SetName(go, (std::string("WM female ") + what).c_str());
  void* t = Invoke(g_fa.goTransform, go); void* st = Invoke(g_fa.goTransform, src);
  // a copy we cannot use is destroyed (or at least switched off), so it does not stay in the scene as an extra part
  auto drop = [&](const char* why) { FemSetActive(go, false); if (g_fa.destroy) { void* a[1] = { go }; FemCall(g_fa.destroy, nullptr, a); }
                                     Log("female crew: the Hair_1 copy for the %s %s; no female %s on rig %p", what, why, what, cmv); return (FemHair*)nullptr; };
  if (!t || !st) return drop("has no transform");
  if (g_setParentTwoArgs) { bool keep = false; void* a[2] = { holderT, &keep }; Invoke(m_setParent, t, a); }
  else { void* a[1] = { holderT }; Invoke(m_setParent, t, a); }
  V3 lp = GetV3(m_getLocalPos, st), ls = GetV3(m_getLocalScale, st);
  void* rr = Invoke(m_getLocalRot, st); Q4 lr = rr ? *(Q4*)((char*)rr + 0x10) : Q4{ 0, 0, 0, 1 };
  { void* a[1] = { &lp }; Invoke(m_setLocalPos, t, a); } { void* a[1] = { &lr }; Invoke(m_setLocalRot, t, a); } { void* a[1] = { &ls }; Invoke(m_setLocalScale, t, a); }
  FemSetActive(go, false);
  void* smr = FemSmrOf(go);
  void* reactor = nullptr; if (g_fa.reactorType) { void* a[1] = { g_fa.reactorType }; reactor = Invoke(g_fa.getComponent, t, a); }
  if (!smr || reactor) return drop(!smr ? "has no SkinnedMeshRenderer" : "has a VisualReactor (combineMesh would skip it)");
  if (Invoke(m_getParent, st) != holderT) DLog("female crew: Hair_1 is not a direct child of BaseCharacter on this rig; the %s copy is", what);
  FemHair h = { go, Pin(go), -1 };
  DLog("female crew: made the %s copy on rig %p", what, cmv);
  return &map.emplace(cmv, h).first->second;
}
// Hat parts by name, active children of BaseCharacter. In the CrewMember prefab's Head GearSlots only H_HelmetBlast has
// removeHair (cutBodyAndHair then calls selectHair(0)); the caps, H_Helmet and H_HelmetTalker leave the hair on.
static bool FemChildOn(void* holderT, const char* const* names) {
  int n = Unbox32(Invoke(g_fa.childCount, holderT));
  for (int i = 0; i < n && i < 200; i++) {
    void* a[1] = { &i }; void* ch = Invoke(g_fa.getChild, holderT, a);
    if (!ch) continue;
    std::string nm = NameOf(ch); bool hit = false;
    for (const char* const* k = names; *k; k++) if (nm == *k) hit = true;
    if (hit && FemActive(Invoke(m_getGameObject, ch))) return true;
  }
  return false;
}
static const char* const kFemHairHats[] = { "H_HelmetBlast", nullptr };              // removeHair = 1
static const char* const kFemHelmets[] = { "H_Helmet", "H_HelmetTalker", nullptr };  // keep the hair; the rolls poke through H_Helmet
// What covers the chest, by the active direct children of BaseCharacter (null: nothing). The shirts (Shirt_Sailor,
// Shirt_Captain, Shirt_Short) and, defensively, the officer uniforms and their decor parts cover it; of the vests only the
// life vest (checked offline: the bra shows a little in its front opening) and the bandolier strap leave the bra on.
static const char* const kFemClosedVests[] = { "V_VestArmor", "V_VestAssault", "V_FlakJacket", nullptr };
static const char* FemChestCover(void* holderT) {
  static std::string hitName;
  int n = Unbox32(Invoke(g_fa.childCount, holderT));
  for (int i = 0; i < n && i < 200; i++) {
    void* a[1] = { &i }; void* ch = Invoke(g_fa.getChild, holderT, a);
    if (!ch) continue;
    std::string nm = NameOf(ch);
    bool hit = nm.compare(0, 6, "Shirt_") == 0 || nm.find("Uniform") != std::string::npos;
    for (const char* const* k = kFemClosedVests; *k && !hit; k++) if (nm == *k) hit = true;
    if (hit && FemActive(Invoke(m_getGameObject, ch))) { hitName = nm; return hitName.c_str(); }
  }
  return nullptr;
}
// The bra for this bake: on (and returns true) only for a bare chest. The game's own choice comes first:
// cutBodyAndHair stores the body mesh it picked in visual.bodyMesh, 'Body' for shirt 'none', 'Body_CutCovered' under a
// shirt (every cutBody gear). g_femBraWhy says why not.
static bool FemBra(void* cmv, void* holder, void* hairSrc, const std::string& name) {
  void* holderT = holder ? Invoke(g_fa.goTransform, holder) : nullptr;
  if (!holderT || !hairSrc) { g_femBraWhy = "no Hair_1 on this rig"; return false; }
  void* bm = g_fa.offBodyMesh >= 0 ? *(void**)((char*)cmv + g_fa.offBodyMesh) : nullptr;
  std::string body = Alive(bm) ? NameOf(bm) : std::string();
  if (!body.empty() && body != "Body") {
    g_femBraWhy = body == "Body_CutCovered" ? "a shirt" : "an unknown body mesh";
    if (body != "Body_CutCovered") DLog("female crew: '%s' body mesh '%s'; no bra", name.c_str(), body.c_str());
    return false;
  }
  if (const char* cover = FemChestCover(holderT)) { static std::string why; why = std::string("under ") + cover; g_femBraWhy = why.c_str(); return false; }
  void* mesh = FemBraMesh(FemSmrOf(hairSrc));
  if (!mesh) { g_femBraWhy = "no bra file"; return false; }
  FemHair* b = FemCopyObject(g_femBra, "bra", cmv, holder, hairSrc);
  if (!b) { g_femBraWhy = "no bra copy"; return false; }
  void* smr = b->style != 0 ? FemSmrOf(b->go) : nullptr;
  if (smr) { void* a[1] = { mesh }; if (!FemCall(g_fa.smrSetMesh, smr, a)) { g_femBraWhy = "setting the bra mesh failed"; FemSetActive(b->go, false); return false; } b->style = 0; }
  if (b->style != 0) { g_femBraWhy = "the bra copy has no renderer"; FemSetActive(b->go, false); return false; }
  FemSetActive(b->go, true);
  g_femBraWhy = "bare chest";
  return true;
}
static void FemBraOff(void* cmv) { auto it = g_femBra.find(cmv); if (it != g_femBra.end() && Alive(it->second.go)) FemSetActive(it->second.go, false); }
// Before combineMesh: the face, the hair, then the bra. Returns the character (null: leave everything to the game).
static void* FemBefore(void* cmv, bool* woman) {
  *woman = false;
  void* c = FemCharacter(cmv);
  if (!c) return nullptr;
  if ((++g_femCalls & 63) == 0) FemPrune();
  std::string name = FemName(c);
  void* holder = *(void**)((char*)cmv + g_fa.offHolder);
  void* m = *(void**)((char*)c + g_fa.offCombined);
  bool* dirty = (bool*)((char*)c + g_fa.offDirtyMesh);
  auto hit = g_femHair.find(cmv);
  if (FemGGet(c)) FemGNote(c, true);                                       // a rename since we last looked: logged, gender kept
  if (!FemIsWoman(c, name)) {
    if (hit != g_femHair.end() && Alive(hit->second.go)) FemSetActive(hit->second.go, false);
    FemBraOff(cmv);
    if (m && FemDone(m)) { *dirty = true; g_femDone.erase(m); DLog("female crew: '%s' is not a woman (stored %d, female_crew=%d%%); the stock look is rebuilt", name.c_str(), (int)FemGGet(c), FemPct()); }
    return c;
  }
  *woman = true;
  bool changed = false;
  int* fh = (int*)((char*)c + g_fa.offSelFacial);
  if (*fh > 4) { int was = *fh; *fh %= 5; int i = *fh; void* a[1] = { &i }; Invoke(g_fa.selFacial, cmv, a); changed = true; DLog("female crew: '%s' facial hair %d -> %d (shaven)", name.c_str(), was, i); }
  int* sp = (int*)((char*)c + g_fa.offSelSpecial1);
  if (*sp == 4) { *sp = 0; int i = 0; void* a[1] = { &i }; Invoke(g_fa.selSpecial1, cmv, a); changed = true; DLog("female crew: '%s' put the pipe away", name.c_str()); }
  // the hair: hair[] = Hair_Bald, Hair_1, Hair_1, Hair_2 .. Hair_5 (CrewMember prefab)
  g_femStyle = -1; g_femWhy = "";
  void* arr = *(void**)((char*)cmv + g_fa.offHair);
  int len = ArrLen(arr); void** el = arr ? (void**)ArrData(arr) : nullptr;
  if (len < 2 || !el[1] || !holder) g_femWhy = "no stock hair on this rig";
  else {
    int sel = *(int*)((char*)c + g_fa.offSelHair); if (sel < 0) sel = 0;
    bool bald = el[0] && FemActive(el[0]);                                  // Hair_Bald: a removeHair hat, or a bald roll
    void* holderT = Invoke(g_fa.goTransform, holder);
    // a hat took the hair: Hair_Bald is on although the roll is not bald, or a removeHair hat (H_HelmetBlast) is on.
    // A bald roll under any other hat gets our hair, as the stock hair stays under those hats.
    bool hat = bald && (sel % len != 0 || (holderT && FemChildOn(holderT, kFemHairHats)));
    int style = sel % 4;
    if (style == 3 && !hat && holderT && FemChildOn(holderT, kFemHelmets)) { style = 1; DLog("female crew: '%s' rolls -> bun under a helmet", name.c_str()); }
    void* mesh = hat ? nullptr : FemHairMesh(style, FemSmrOf(el[1]));
    FemHair* h = hat || !mesh ? nullptr : FemCopyObject(g_femHair, "hair", cmv, holder, el[1]);
    if (!h) {
      hit = g_femHair.find(cmv);
      if (hit != g_femHair.end() && Alive(hit->second.go)) FemSetActive(hit->second.go, false);
      g_femWhy = hat ? "under a hat" : !mesh ? "no hair file" : "no hair copy";
    } else {
      for (int k = 0; k < len; k++) if (el[k] && el[k] != h->go && FemActive(el[k])) FemSetActive(el[k], false);
      void* smr = h->style != style ? FemSmrOf(h->go) : nullptr;
      if (smr) { void* a[1] = { mesh }; Invoke(g_fa.smrSetMesh, smr, a); h->style = style; }
      FemSetActive(h->go, true);
      g_femStyle = style;
    }
  }
  g_femBraOn = false; g_femBraWhy = "";
  g_femBraOn = FemBra(cmv, holder, len >= 2 ? el[1] : nullptr, name);
  if (!g_femBraOn) FemBraOff(cmv);
  if (changed || !m || !FemDone(m)) *dirty = true;                          // rebuild: a face change, or not reshaped yet
  else if (g_femDone[m].bra != g_femBraOn) {                                // gear changed the chest: the cached mesh is stale
    *dirty = true; DLog("female crew: '%s' bra %s (%s); rebuilt", name.c_str(), g_femBraOn ? "on" : "off", g_femBraWhy);
  }
  else if (g_femDone[m].rose && g_femLipFail) {                             // rose lips, but the paint no longer works: rebuilt with peach
    *dirty = true; DLog("female crew: '%s' lips point at the rose block, which can no longer be painted; rebuilt with the peach patch", name.c_str());
  }
  return c;
}

// The approved 'v2' shape, ported 1:1 from deform_female.py field(). P is the BaseCharacter frame in metres (x lateral,
// y up, z forward); arm: the vertex's strongest bone is ArmUpper/ArmLower L/R.
static inline double FemG(double x, double s) { return exp(-0.5 * (x / s) * (x / s)); }
static void FemField(double x, double y, double z, bool arm, double* D) {
  D[0] = D[1] = D[2] = 0;
  double torso = fabs(x) < 0.26 ? 1 : 0;
  double front = (z + 0.02) / 0.06; front = front < 0 ? 0 : (front > 1 ? 1 : front);
  for (double sx : { -0.078, 0.078 }) {                                      // bust: two soft bumps forward on the chest front
    double b = FemG(x - sx, 0.060) * FemG(y - 1.295, 0.050) * front * torso;
    D[2] += 0.055 * b; D[1] -= 0.014 * b;
  }
  double w = 0.12 * FemG(y - 1.06, 0.07) * torso;                           // waist pulled in toward the spine axis
  D[0] += -x * w; D[2] += -(z + 0.07) * w * 0.6;
  double h = 0.10 * FemG(y - 0.82, 0.07) * (fabs(x) < 0.30 ? 1 : 0);        // hips out
  D[0] += x * h;
  double s = 0.07 * FemG(y - 1.42, 0.06) * torso;                           // narrower shoulders and upper chest
  D[0] += -x * s;
  if (arm) D[0] += -(x > 0 ? 1 : (x < 0 ? -1 : 0)) * 0.012;                 // slimmer arms
}
// Jawline_Weak at weight 1, as deform_female.py bakes it. Read from the plain body mesh (visual.bodyMesh, Body or
// Body_CutCovered): the combined mesh's own blend shape frames are copied by index although GetSubmesh reorders the
// vertices, so they do not line up. The body's vertices are copied bit for bit, so they are found by position.
static int FemJaw(void* cmv, std::vector<V3>& pos) {
  if (!g_fa.bsFrame) return -1;
  void* body = *(void**)((char*)cmv + g_fa.offBodyMesh);
  if (!Alive(body)) return -1;
  int shapes = Unbox32(Invoke(g_fa.bsCount, body)), idx = -1;
  for (int i = 0; i < shapes && idx < 0; i++) { void* a[1] = { &i }; void* s = Invoke(g_fa.bsName, body, a); if (s && !strcmp(Str(s), "Jawline_Weak")) idx = i; }
  if (idx < 0) return -1;
  int bv = Unbox32(Invoke(g_fa.vcount, body));
  void* bverts = Invoke(g_fa.getVerts, body);
  if (bv <= 0 || ArrLen(bverts) != bv) return -1;
  void* dv = FemArray(g_fa.v3Cls, bv, nullptr, 0), *dn = FemArray(g_fa.v3Cls, bv, nullptr, 0), *dt = FemArray(g_fa.v3Cls, bv, nullptr, 0);
  int frame = 0; void* a[5] = { &idx, &frame, dv, dn, dt };
  if (!dv || !dn || !dt || !FemCall(g_fa.bsFrame, body, a)) return -1;
  auto key = [](const V3& p) { uint32_t k[3]; memcpy(k, &p, 12); return ((uint64_t)k[0] * 0x9E3779B1u) ^ ((uint64_t)k[1] << 21) ^ ((uint64_t)k[2] * 0x85EBCA77u); };
  struct Src { V3 p, d; };
  std::unordered_multimap<uint64_t, Src> src;
  const V3* bp = (const V3*)ArrData(bverts); const V3* bd = (const V3*)ArrData(dv);
  for (int i = 0; i < bv; i++) if (bd[i].x != 0 || bd[i].y != 0 || bd[i].z != 0) src.emplace(key(bp[i]), Src{ bp[i], bd[i] });
  int n = 0;
  for (V3& p : pos) {
    auto r = src.equal_range(key(p));
    for (auto it = r.first; it != r.second; ++it) {
      if (memcmp(&it->second.p, &p, 12)) continue;
      p.x += it->second.d.x; p.y += it->second.d.y; p.z += it->second.d.z; n++; break;
    }
  }
  return n;
}
// The lips. The atlas ('Crew Full Baked' _MainTex, 2048x2048 DXT5, one mip, not readable) has no red anywhere, so an
// empty 32x32 block (pixels x 1928-1959, y 1656-1687 from the top: colour, alpha and mask all 0, used by no crew part,
// 8 px of empty border) is filled on the GPU with one repeated DXT5 block of sRGB (172,82,88), a muted rose, alpha 255.
// Its mask is black, so _Color5 (left at white) shows it as it is. Repainted on every woman's combineMesh, a reused mesh
// included (FemLipKeep; one tiny GPU copy), so an atlas that lost its GPU contents gets it back at the next woman's
// portrait; live, overboard and co-op crew share the atlas, so they get it too. Men never sample there.
static const float kFemLipU = 0.94922f, kFemLipV = 0.18359f;            // centre of the painted block
static const float kFemPeachU = 0.75195f, kFemPeachV = 0.95215f;        // fallback: untinted pale peach (228,192,176)
static bool FemLipPaint(void* cmv) {
  if (g_femLipFail) return false;
  FemApi& a = g_fa;
  void* mat = a.offBodyMat >= 0 ? *(void**)((char*)cmv + a.offBodyMat) : nullptr;
  if (!Alive(mat) && a.offBody >= 0) {                                    // body[0], the renderer combineMesh fills
    void* arr = *(void**)((char*)cmv + a.offBody);
    void* smr = ArrLen(arr) > 0 ? ((void**)ArrData(arr))[0] : nullptr;
    mat = Alive(smr) ? Invoke(a.rGetMat, smr) : nullptr;
  }
  void* atlas = Alive(mat) ? Invoke(a.matMainTex, mat) : nullptr;
  if (!Alive(atlas)) { DLog("female crew: no crew atlas on rig %p; these lips use the peach patch", cmv); return false; }
  int w = Unbox32(Invoke(a.texW, atlas)), h = Unbox32(Invoke(a.texH, atlas));
  int fmt = a.texFmt && il2cpp_object_get_class(atlas) == a.tex2dCls ? Unbox32(Invoke(a.texFmt, atlas)) : -1;
  int mips = a.texMips ? Unbox32(Invoke(a.texMips, atlas)) : 1;
  if (w != 2048 || h != 2048 || fmt != 12 || mips != 1) {                 // 12 = TextureFormat.DXT5; a copy needs the same format
    g_femLipFail = true;
    Log("female crew: the crew atlas is %dx%d, format %d, %d mips (expected 2048x2048 DXT5, 1 mip); women's lips use the pale peach patch", w, h, fmt, mips);
    return false;
  }
  if (!Alive(g_femLipTex)) {
    if (g_femLipTried) { g_femLipFail = true; Log("female crew: the lip colour block was lost; women's lips use the pale peach patch"); return false; }
    g_femLipTried = true;
    static unsigned char raw[64 * 16];                                     // 8x8 blocks of 4x4 texels
    static const unsigned char blk[16] = { 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0x8B, 0xAA, 0x8B, 0xAA, 0, 0, 0, 0 };   // alpha 255; RGB565 0xAA8B
    for (int i = 0; i < 64; i++) memcpy(raw + 16 * i, blk, 16);
    void* t = il2cpp_object_new(a.tex2dCls);
    int sz = 32, f12 = 12; bool mipChain = false;
    void* ca[4] = { &sz, &sz, &f12, &mipChain };
    bool ok = t && FemCall(a.tex2dCtor, t, ca);
    void* ptr = raw; int len = (int)sizeof raw; void* la[2] = { &ptr, &len };
    ok = ok && FemCall(a.texLoadRaw, t, la);
    bool noMips = false, unreadable = true; void* aa[2] = { &noMips, &unreadable };
    ok = ok && FemCall(a.texApply, t, aa) && Alive(t);
    if (!ok) { g_femLipFail = true; Log("female crew: making the lip colour block failed; women's lips use the pale peach patch"); return false; }
    SetName(t, "WM female lips");
    if (g_fa.hide) { int keep = 32; void* hf[1] = { &keep }; Invoke(g_fa.hide, t, hf); }
    Pin(t); g_femLipTex = t;
  }
  void* src = *(void**)((char*)g_femLipTex + 0x10); void* dst = *(void**)((char*)atlas + 0x10);
  if (!src || !dst) return false;
  a.copyRegion(src, 0, 0, 0, 0, 32, 32, dst, 0, 0, 1928, 2048 - 1688);    // y counted from the bottom
  if (++g_femLipPaints == 1) Log("female crew: lip colour painted into the crew atlas '%s' (32x32 at x 1928, y 1656 from the top)", NameOf(atlas).c_str());
  return true;
}
// The mouth part's vertices in a combined mesh: they keep Mouth_1/2/3's UVs (only the mouths use those islands), are
// 100% Head and sit in a thin strip in front of the face (mesh space x +-0.0003, y -0.00119..-0.00103, z 0.01538..0.01546).
static bool FemIsMouth(const float* uv, const V3& p, const float* w, const int* idx) {
  float u = uv[0], vv = uv[1];
  bool island = (u > 0.9470f && u < 0.9515f && vv > 0.0020f && vv < 0.0130f) || (u > 0.9050f && u < 0.9135f && vv > 0.0485f && vv < 0.0570f);
  if (!island) return false;
  int best = 0; for (int k = 1; k < 4; k++) if (w[k] > w[best]) best = k;
  if (idx[best] != 3 || w[best] < 0.999f) return false;
  return fabsf(p.x) < 0.0004f && p.y < -0.0009f && p.z > 0.01530f && p.z < 0.01555f;
}
// After combineMesh: the new combined mesh is reshaped in place. Mesh space -> BaseCharacter frame: P = (100x, 100z, -100y).
// Timed, because every loadout change re-bakes every woman, one per portrait rig per frame.
static int g_femAfterN = 0; static double g_femAfterMax = 0, g_femAfterSum = 0;
static double FemMs(const LARGE_INTEGER& t0) {
  static LARGE_INTEGER f = {}; if (!f.QuadPart) QueryPerformanceFrequency(&f);
  LARGE_INTEGER t1; QueryPerformanceCounter(&t1);
  return f.QuadPart ? (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart : 0;
}
static void FemAfter(void* cmv, void* c) {
  void* m = *(void**)((char*)c + g_fa.offCombined);
  if (!Alive(m) || FemDone(m)) return;                                    // the game reused a mesh we already reshaped
  LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
  std::string name = FemName(c);
  void* va = Invoke(g_fa.getVerts, m); void* na = Invoke(g_fa.getNormals, m); void* wa = Invoke(g_fa.getBw, m); void* ta = Invoke(g_fa.getTris, m);
  int n = ArrLen(va), nt = ArrLen(ta);
  if (n <= 0 || ArrLen(na) != n || ArrLen(wa) != n || nt % 3) { Log("female crew: '%s': the combined mesh has %d vertices, %d normals, %d weights, %d indices; left as it is", name.c_str(), n, ArrLen(na), ArrLen(wa), nt); return; }
  V3* v = (V3*)ArrData(va); V3* nr = (V3*)ArrData(na); const int32_t* tri = (const int32_t*)ArrData(ta);
  struct BW { float w[4]; int i[4]; }; const BW* bw = (const BW*)ArrData(wa);
  // work buffers kept across bakes (game thread only, FemAfter never nests), so a re-bake allocates nothing native
  static std::vector<V3> orig, pos; static std::vector<char> moved; static std::vector<int> wid;
  static std::vector<std::pair<uint64_t, int>> kv; static std::vector<double> acc;
  orig.assign(v, v + n); pos = orig;
  int jaw = FemJaw(cmv, pos);
  // the lips: mouth vertices get the rose block's UV (or the peach one); with the softer jaw the chin recedes about
  // 1 cm while the separate mouth part stays, so it is set back by half the recession estimated at its height
  // (measured on the mesh: dy = 0.0001121 * (0.01562 - z) / 0.000525 mesh units), which keeps it in front of the face.
  // The bra's vertices are counted by their UV, for the log.
  void* uva = g_fa.getUv ? Invoke(g_fa.getUv, m) : nullptr;
  int nMouth = 0, nBra = 0; bool rose = false;
  if (uva && ArrLen(uva) == n) {
    float* uv = (float*)ArrData(uva);
    std::vector<int> mouth;
    for (int i = 0; i < n; i++) {
      if (uv[2 * i] == kFemBraU && uv[2 * i + 1] == kFemBraV) nBra++;
      else if (FemIsMouth(uv + 2 * i, orig[i], bw[i].w, bw[i].i)) mouth.push_back(i);
    }
    nMouth = (int)mouth.size();
    if (nMouth > 0 && nMouth <= 400) {
      rose = FemLipPaint(cmv);
      for (int i : mouth) {
        uv[2 * i] = rose ? kFemLipU : kFemPeachU; uv[2 * i + 1] = rose ? kFemLipV : kFemPeachV;
        if (jaw > 0) { double dy = 0.5 * 0.0001121 * (0.01562 - orig[i].z) / 0.000525; pos[i].y += (float)(dy < 0 ? 0 : (dy > 3e-5 ? 3e-5 : dy)); }
      }
    } else if (nMouth > 400) { DLog("female crew: '%s': %d vertices look like the mouth (expected 124); lips left as they are", name.c_str(), nMouth); nMouth = -nMouth; }
  } else if (g_fa.getUv) DLog("female crew: '%s': the combined mesh has %d UVs for %d vertices; lips left as they are", name.c_str(), ArrLen(uva), n);
  moved.assign(n, 0); int nMoved = 0; double maxShift = 0;
  for (int i = 0; i < n; i++) {
    int best = 0; for (int k = 1; k < 4; k++) if (bw[i].w[k] > bw[i].w[best]) best = k;
    int bone = bw[i].i[best];
    bool arm = bone == 5 || bone == 6 || bone == 9 || bone == 10;          // ArmUpper.L, ArmLower.L, ArmUpper.R, ArmLower.R
    double D[3]; FemField(100.0 * pos[i].x, 100.0 * pos[i].z, -100.0 * pos[i].y, arm, D);
    double sh = sqrt(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]); if (sh > maxShift) maxShift = sh;
    pos[i].x += (float)(D[0] / 100.0); pos[i].y += (float)(-D[2] / 100.0); pos[i].z += (float)(D[1] / 100.0);
    double dx = pos[i].x - orig[i].x, dy = pos[i].y - orig[i].y, dz = pos[i].z - orig[i].z;
    if (dx * dx + dy * dy + dz * dz > 1e-14) { moved[i] = 1; nMoved++; }     // 1e-7 mesh units, as deform_female.py
  }
  // normals of the moved vertices: area-weighted face normals, summed over every vertex at the same place (rounded to
  // 0.01 mm), so split seams stay smooth; the triangles' winding matches the stock normals (checked on the export).
  // The weld groups vertices by sorting (key, index) pairs, so a re-bake makes no per-node allocations.
  kv.resize(n); wid.resize(n);
  for (int i = 0; i < n; i++) {
    long long qx = llround(pos[i].x * 1e7), qy = llround(pos[i].y * 1e7), qz = llround(pos[i].z * 1e7);
    uint64_t k = ((uint64_t)(qx & 0x1FFFFF)) | ((uint64_t)(qy & 0x1FFFFF) << 21) | ((uint64_t)(qz & 0x1FFFFF) << 42);
    kv[i] = { k, i };
  }
  std::sort(kv.begin(), kv.end());
  int nWeld = 0;
  for (int j = 0; j < n; j++) { if (j == 0 || kv[j].first != kv[j - 1].first) nWeld++; wid[kv[j].second] = nWeld - 1; }
  acc.assign((size_t)nWeld * 3, 0.0);
  for (int t = 0; t + 2 < nt; t += 3) {
    int a = tri[t], b = tri[t + 1], cI = tri[t + 2];
    if (a < 0 || b < 0 || cI < 0 || a >= n || b >= n || cI >= n) continue;
    double ux = pos[b].x - pos[a].x, uy = pos[b].y - pos[a].y, uz = pos[b].z - pos[a].z;
    double wx = pos[cI].x - pos[a].x, wy = pos[cI].y - pos[a].y, wz = pos[cI].z - pos[a].z;
    double fx = uy * wz - uz * wy, fy = uz * wx - ux * wz, fz = ux * wy - uy * wx;
    for (int q : { a, b, cI }) { double* p = &acc[3 * (size_t)wid[q]]; p[0] += fx; p[1] += fy; p[2] += fz; }
  }
  for (int i = 0; i < n; i++) {
    v[i] = pos[i];
    if (!moved[i]) continue;
    const double* p = &acc[3 * (size_t)wid[i]]; double l = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (l > 1e-30) nr[i] = V3{ (float)(p[0] / l), (float)(p[1] / l), (float)(p[2] / l) };
  }
  void* a1[1] = { va }; void* a2[1] = { na }; void* a3[1] = { uva };
  if (!FemCall(g_fa.setVerts, m, a1) || !FemCall(g_fa.setNormals, m, a2) || !FemCall(g_fa.bounds, m)) { Log("female crew: '%s': writing the reshaped mesh failed", name.c_str()); return; }
  if (nMouth > 0 && !FemCall(g_fa.setUv, m, a3)) { Log("female crew: '%s': writing the lip UVs failed", name.c_str()); nMouth = 0; }
  bool braIn = g_femBraOn && nBra > 0;
  if (g_femBraOn && !nBra) Log("female crew: '%s': the bra was switched on but is not in the combined mesh", name.c_str());
  g_femDone[m] = FemMesh{ *(void**)((char*)m + 0x10), g_femBraOn, rose && nMouth > 0 };
  const char* hair = g_femStyle >= 0 ? kFemStyle[g_femStyle] : g_femWhy;
  double ms = FemMs(t0); g_femAfterN++; g_femAfterSum += ms;
  bool newMax = ms > g_femAfterMax; if (newMax) g_femAfterMax = ms;
  if (g_femAfterN <= 5 || newMax)
    DLog("female crew: reshape of '%s' took %.2f ms (%d vertices, %d welded, %d indices); max %.2f ms, mean %.2f ms over %d reshapes", name.c_str(), ms, n, nWeld, nt,
         g_femAfterMax, g_femAfterSum / g_femAfterN, g_femAfterN);
  if (++g_femBuilt == 1) Log("female crew: first woman built: '%s', hair %s, %d of %d vertices moved (jaw %d), max shift %.0f mm, %.2f ms", name.c_str(), hair, nMoved, n, jaw, maxShift * 1000, ms);
  else DLog("female crew: '%s', hair %s, %d of %d vertices moved (jaw %d), max shift %.0f mm (%d women this session)", name.c_str(), hair, nMoved, n, jaw, maxShift * 1000, g_femBuilt);
  if (nMouth > 0 && ++g_femLips == 1) Log("female crew: first lips recoloured: '%s', %d mouth vertices -> %s", name.c_str(), nMouth, rose ? "rose (painted block)" : "pale peach (no paint)");
  else if (nMouth > 0) DLog("female crew: '%s' lips: %d vertices, %s", name.c_str(), nMouth, rose ? "rose" : "peach");
  else if (g_fa.getUv && nMouth == 0) DLog("female crew: '%s': no mouth vertices found; lips left as they are", name.c_str());
  if (braIn && ++g_femBras == 1) Log("female crew: first bra shown: '%s', %d bra vertices of %d", name.c_str(), nBra, n);
  else if (braIn) DLog("female crew: '%s' bra on (%d vertices)", name.c_str(), nBra);
  else DLog("female crew: '%s' no bra (%s)", name.c_str(), g_femBraWhy);
}

static void FemOff() {                                                     // after a fault: back to the game's own look
  g_femOn = false;
  __try { for (auto* map : { &g_femHair, &g_femBra }) for (auto& e : *map) if (Alive(e.second.go)) FemSetActive(e.second.go, false); } __except (EXCEPTION_EXECUTE_HANDLER) {}
  Log("female crew: off until restart (%d women were built); new portraits are stock", g_femBuilt);
}
// After a woman's combineMesh that reused a mesh with rose lips (FemAfter returned early, nothing painted): the block is
// painted again, in case the atlas lost its GPU contents. If painting has become impossible, FemBefore rebuilds every
// rose-lipped mesh with the peach patch at its next bake (FemMesh.rose && g_femLipFail).
static void FemLipKeep(void* cmv, void* c, int paintsBefore) {
  if (g_femLipFail || g_femLipPaints != paintsBefore) return;              // impossible, or FemAfter just painted
  void* m = *(void**)((char*)c + g_fa.offCombined);
  if (!FemDone(m) || !g_femDone[m].rose) return;
  if (!FemLipPaint(cmv) && g_femLipFail) Log("female crew: the lip paint stopped working; rose-lipped women get the peach patch at their next portrait");
}
static void Hooked_Combine(void* cmv, void* method) {
  void* c = nullptr; bool woman = false;
  if (g_femOn && cmv) {
    __try { c = FemBefore(cmv, &woman); }
    __except (FaultFilter("female crew (before the bake)", GetExceptionInformation())) { c = nullptr; woman = false; FemOff(); }
  }
  g_combineOrig(cmv, method);
  if (!woman || !c || !g_femOn) return;
  // the bra copy is merged by now (or the cached mesh was reused); switched off so it never renders on its own, unshaped
  __try { int paints = g_femLipPaints; FemBraOff(cmv); FemAfter(cmv, c); FemLipKeep(cmv, c, paints); }
  __except (FaultFilter("female crew (reshape)", GetExceptionInformation())) { FemOff(); }
}
// ---- the gear flash ----
// A gear pick in the loadout runs GearSlot.selectIndex -> selectGear(int) on the crew member ON THE BOAT, which switches
// the stock gear objects on and clears CrewMember.lastMesh (+0x250); nothing hides them until the next
// CrewMember.checkMesh (next frame). For one frame the crew renders stock parts over the combined mesh, which on a woman
// shows the male-shaped, untinted clothes. The new look comes later from the portrait rigs (showUp -> combineMesh).
// Fix: after the original selectIndex, for a crew member whose combined mesh is one we reshaped (so never a man), that
// is active in the scene (never a portrait rig mid-build: the rig is inactive then, and its gear goes through
// selectGear directly), and whose lastMesh no longer matches, checkMesh is called at once: exactly what the game does a
// frame later, before anything renders. Hooked by GearSlot's vtable entry for selectIndex (the slot CustomSlot.select(int)
// calls), else by a jump over the 16-byte selectIndex thunk (xor r8d,r8d; jmp selectGear).
typedef void (*fn_gearSel)(void*, int, void*);
static fn_gearSel g_gearSelOrig = nullptr;
struct FemGearApi { int offMember, offChar, offLastMesh, offMats; void *checkMesh, *inHierarchy; };
static FemGearApi g_fg = { -1, -1, -1, -1, nullptr, nullptr };
static bool g_femGearOn = false; static const char* g_femGearPath = "";
static int g_femGearHides = 0, g_femGearSkips = 0, g_femGearFails = 0;
static void FemGearHide(void* gs) {
  void* member = *(void**)((char*)gs + g_fg.offMember);
  if (!Alive(member)) return;
  void* c = *(void**)((char*)member + g_fg.offChar);
  if (!c) return;
  void* m = *(void**)((char*)c + g_fa.offCombined);
  if (!m || !FemDone(m)) return;                                          // not a woman's reshaped mesh: the game's own way
  if (*(void**)((char*)member + g_fg.offLastMesh) == m) return;            // nothing to hide (checkMesh would do nothing)
  if (!*(void**)((char*)c + g_fg.offMats)) return;
  void* go = Invoke(m_getGameObject, member);
  void* r = go ? Invoke(g_fg.inHierarchy, go) : nullptr;
  if (!r || !*(bool*)((char*)r + 0x10)) { if (++g_femGearSkips <= 3) DLog("female crew: gear change on an inactive crew member (%p); left to the game", member); return; }
  if (!FemCall(g_fg.checkMesh, member)) { if (++g_femGearFails <= 3) Log("female crew: CrewMember.checkMesh threw after a gear change (%d)", g_femGearFails); return; }
  if (++g_femGearHides == 1) Log("female crew: first gear change on a woman ('%s'): stock parts hidden in the same call (%s)", FemName(c).c_str(), g_femGearPath);
  else if (g_femGearHides <= 10 || (g_femGearHides % 100) == 0) DLog("female crew: gear change on '%s': stock parts hidden (%d this session)", FemName(c).c_str(), g_femGearHides);
}
static void Hooked_GearSelectIndex(void* gs, int i, void* method) {
  g_gearSelOrig(gs, i, method);
  if (!g_femOn || !g_femGearOn || !gs) return;
  __try { FemGearHide(gs); }
  __except (FaultFilter("female crew (gear change)", GetExceptionInformation())) { g_femGearOn = false; }
}
static void FemGearInstall() {
  void* gk = FindClass("Core", "GearSlot"); void* ck = FindClass("Core", "CrewMember"); void* cc = FindClass("Core", "CrewCharacter");
  void* go = FindClass("UnityEngine", "GameObject");
  g_fg.offMember = FieldOffset(gk, "member"); g_fg.offChar = FieldOffset(ck, "_character"); g_fg.offLastMesh = FieldOffset(ck, "lastMesh"); g_fg.offMats = FieldOffset(cc, "combinedMaterials");
  g_fg.checkMesh = ck ? FindMethod(ck, "checkMesh", 0) : nullptr; g_fg.inHierarchy = go ? FindMethod(go, "get_activeInHierarchy", 0) : nullptr;
  void* mi = gk ? il2cpp_class_get_method_from_name(gk, "selectIndex", 1) : nullptr;
  if (g_fg.offMember < 0 || g_fg.offChar < 0 || g_fg.offLastMesh < 0 || g_fg.offMats < 0 || !g_fg.checkMesh || !g_fg.inHierarchy || !mi || !m_getGameObject) {
    Log("female crew: a gear lookup failed (GearSlot.member +%d, CrewMember._character +%d lastMesh +%d, combinedMaterials +%d, checkMesh %p, activeInHierarchy %p, "
        "selectIndex %p); stock clothes may flash on women for a frame when gear changes", g_fg.offMember, g_fg.offChar, g_fg.offLastMesh, g_fg.offMats, g_fg.checkMesh, g_fg.inHierarchy, mi);
    return;
  }
  // the vtable is filled when the class is initialised; GearSlots exist by now, this only makes sure
  auto classInit = (void (*)(void*))GetProcAddress(g_ga, "il2cpp_runtime_class_init");
  if (classInit) classInit(gk);
  void* p = *(void**)mi;
  if (PatchVtableEntry(gk, mi, (void*)&Hooked_GearSelectIndex, (void**)&g_gearSelOrig)) g_femGearPath = "GearSlot.selectIndex vtable entry";
  else {
    // xor r8d,r8d; jmp selectGear(int) (rel32 -0xA88 from 0x1809C7318, at rva 0x9C7310); the trampoline
    // gets the same two steps with an absolute jump
    static const unsigned char kThunk[16] = { 0x45, 0x33, 0xC0, 0xE9, 0x78, 0xF5, 0xFF, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
    unsigned char body[17] = { 0x45, 0x33, 0xC0, 0xFF, 0x25, 0, 0, 0, 0 };
    uint64_t sel = (uint64_t)p + 8 + (uint64_t)(int64_t)(int32_t)0xFFFFF578; memcpy(body + 9, &sel, 8);
    g_gearSelOrig = (fn_gearSel)Detour(p, (void*)&Hooked_GearSelectIndex, kThunk, 16, "GearSlot.selectIndex", body, sizeof body);
    if (!g_gearSelOrig) { Log("female crew: GearSlot.selectIndex could not be hooked; stock clothes may flash on women for a frame when gear changes"); return; }
    g_femGearPath = "GearSlot.selectIndex thunk jump";
  }
  g_femGearOn = true;
  Log("female crew: hooked GearSlot.selectIndex (rva 0x%llX, %s); a gear change on a woman hides the stock parts at once", (unsigned long long)((uintptr_t)p - (uintptr_t)g_ga), g_femGearPath);
}
// ---- female crew, names of new crew --------------------------------------------------------------------------------
// CharacterGenerator.generateCharacter() (static, no parameters; every new crew member comes from it: campaign start
// crew and recruits, wave mode, challenges, enemy and AI crews via CrewSpawner.spawnCrew /
// CharacterManager.getCharacter, the co-op client's first connect) builds the name as names[r] + " " + middle[r] +
// surnames[r] from NameImporter.instance (System.Random draws in that order; middle is "A. ".."Z. " and 100 empty
// entries), or "UNNAMED" when there is no live NameImporter. Nothing reads the name before it returns (no seed,
// specialCode is made on demand from it), so the post-hook rewrites CrewCharacter.name:
//  * roll < female_crew: a woman. Her first name is replaced by one from kFemFirst; middle initial and surname stay.
//  * else a man. A vanilla name the hash calls a man is kept (75% at 25%). Otherwise the whole name is rolled again from
//    NameImporter's live arrays, up to 32 times, then every middle x surname for his first name; if none hashes as a
//    man (only near 100%) she becomes a woman.
// So new crew are women at exactly female_crew, and the names stay vanilla for men. female_crew 0: no hook at launch,
// and the hook does nothing once set to 0 in Mod Options.
// Prologue (rva 0x951CE0): push rbx; sub rsp,30h; cmp byte [rip+X],0; jne +0x8F (to p+0xA2). The rip-relative cmp and
// the jne rel32 cannot run from a trampoline, so its body (43 bytes) redoes them with absolute addresses.
struct FemNamesApi { void* fInst; int offNames, offSurnames, offMiddle, offName; };
static FemNamesApi g_fn = { nullptr, -1, -1, -1, -1 };
typedef void* (*fn_genChar)(void*);
static fn_genChar g_genOrig = nullptr;
static bool g_femGenOn = false, g_femGenChecked = false;
static int g_femGenWomen = 0, g_femGenMen = 0, g_femGenRenamed = 0, g_femGenFallback = 0, g_femGenSkips = 0;   // this session
static int FemPick(int n) { return std::uniform_int_distribution<int>(0, n - 1)(Rng()); }
static void FemGenName(void* c) {
  if (!c) return;
  if (g_noFemales) {                                                   // "I don't want females!": a man, stored, so he stays one
    if (g_fgd.offG > 0 && !FemGGet(c)) {                               // when the switch goes off again (a new character has
      FemGSet(c, 1); g_femGenMen++;                                    // no earlier gender to lose); the game's name is kept
      int t = 0; FemGCount(&FemGCounts::created, &FemGCounts::createdW, false, &t);
      if (t <= 5 || t % 100 == 0) Log("female crew: new crew member '%s' stored as a man (no_females=1; %d stored at creation this session)",
                                       FemUtf8(*(void**)((char*)c + g_fn.offName)).c_str(), t);
    }
    return;
  }
  if (FemPct() <= 0) return;
  std::string cur = FemUtf8(*(void**)((char*)c + g_fn.offName));
  if (cur.empty() || cur == "UNNAMED") { if (++g_femGenSkips <= 3) DLog("female crew: new crew member named '%s'; left as the game made it", cur.c_str()); return; }
  // NameImporter's arrays, read on every call (its Awake can replace them)
  void* ni = nullptr; il2cpp_field_static_get_value(g_fn.fInst, &ni);
  void *na = nullptr, *sa = nullptr, *ma = nullptr;
  if (ni && *(void**)((char*)ni + 0x10)) { na = *(void**)((char*)ni + g_fn.offNames); sa = *(void**)((char*)ni + g_fn.offSurnames); ma = *(void**)((char*)ni + g_fn.offMiddle); }
  int nn = ArrLen(na), sn = ArrLen(sa), mn = ArrLen(ma);
  if (!g_femGenChecked && nn > 0) {                                    // once: no women's name may be in the game's list
    g_femGenChecked = true; std::string clash;
    for (int i = 0; i < nn; i++) { std::string f = FemUtf8(ArrAt(na, i)); if (FemFirstIsFemale(f.data(), f.size())) clash += " " + f; }
    if (clash.empty()) Log("female crew: none of the %d women's names is among the game's %d first names (%d surnames, %d middle)", kFemFirstN, nn, sn, mn);
    else Log("female crew: the game's first names now include women's names from the mod's list:%s; men with them become women", clash.c_str());
  }
  int roll = FemPick(100);
  bool woman = roll < FemPct();
  std::string out = cur; int tries = 0; const char* how = "";
  if (!woman) {
    if (!FemNameRule(cur)) how = "vanilla name kept";
    else if (nn > 0 && sn > 0 && mn > 0) {
      bool found = false;
      while (!found && tries < 32) {                                   // the game's own format and order
        tries++;
        std::string t = FemUtf8(ArrAt(na, FemPick(nn))) + " " + FemUtf8(ArrAt(ma, FemPick(mn))) + FemUtf8(ArrAt(sa, FemPick(sn)));
        if (!t.empty() && t[0] != ' ' && !FemNameRule(t)) { out = t; found = true; }
      }
      if (!found) {                                                    // only likely near 100%: every middle x surname
        size_t sp = cur.find(' '); std::string first = cur.substr(0, sp);
        for (int m = 0; m < mn && !found; m++) {
          std::string fm = first + " " + FemUtf8(ArrAt(ma, m));
          for (int s = 0; s < sn && !found; s++) { std::string t = fm + FemUtf8(ArrAt(sa, s)); tries++; if (!FemNameRule(t)) { out = t; found = true; } }
        }
      }
      if (found) { how = "renamed"; g_femGenRenamed++; }
      else { woman = true; how = "no man's name found"; g_femGenFallback++; }
    } else { woman = true; how = "no name lists to roll a man's name"; g_femGenFallback++; }
  }
  if (woman) {
    size_t sp = cur.find(' ');
    out = std::string(kFemFirst[FemPick(kFemFirstN)]) + (sp == std::string::npos ? std::string() : cur.substr(sp));
    g_femGenWomen++;
  } else g_femGenMen++;
  if (out != cur) SetRef(c, (char*)c + g_fn.offName, il2cpp_string_new(out.c_str()));
  if (g_fgd.offG > 0) {                                                // stored now: a rename or a new female_crew never changes it
    FemGSet(c, woman ? 2 : 1);
    int t = 0; FemGCount(&FemGCounts::created, &FemGCounts::createdW, woman, &t);
    if (t <= 5 || t % 100 == 0) Log("female crew: stored gender of new crew member '%s': %s (%d stored at creation this session)", out.c_str(), woman ? "woman" : "man", t);
  }
  int total = g_femGenWomen + g_femGenMen;
  if (total <= 5 || (total % 50) == 0)
    Log("female crew: new crew member '%s' is a %s (roll %d, female_crew=%d%%; was '%s'%s%s%s); %d new this session: %d women, %d men, %d men renamed",
        out.c_str(), woman ? "woman" : "man", roll, FemPct(), cur.c_str(), *how ? ", " : "", how, tries ? (", " + std::to_string(tries) + " tries").c_str() : "",
        total, g_femGenWomen, g_femGenMen, g_femGenRenamed);
  else DLog("female crew: new crew member '%s' is a %s (roll %d; was '%s'%s%s); %d women, %d men this session", out.c_str(), woman ? "woman" : "man", roll, cur.c_str(),
            *how ? ", " : "", how, g_femGenWomen, g_femGenMen);
}
static void FemGenSafe(void* c) {
  if (!g_femGenOn) return;
  __try { FemGenName(c); }
  __except (FaultFilter("female crew (new crew names)", GetExceptionInformation())) { g_femGenOn = false; Log("female crew: new crew names off until restart"); }
}
static void* Hooked_GenChar(void* mi) {
  void* c = g_genOrig(mi);
  FemGenSafe(c);
  return c;
}
static void FemGenInstall() {
  void* cg = FindClass("Core", "CharacterGenerator"); void* nk = FindClass("", "NameImporter"); void* cc = FindClass("Core", "CrewCharacter");
  g_fn.fInst = nk ? il2cpp_class_get_field_from_name(nk, "instance") : nullptr;
  g_fn.offNames = FieldOffset(nk, "names"); g_fn.offSurnames = FieldOffset(nk, "surnames"); g_fn.offMiddle = FieldOffset(nk, "middle"); g_fn.offName = FieldOffset(cc, "name");
  void* mi = cg ? FindMethod(cg, "generateCharacter", 0) : nullptr;
  if (!mi || !g_fn.fInst || g_fn.offNames < 0 || g_fn.offSurnames < 0 || g_fn.offMiddle < 0 || g_fn.offName < 0) {
    Log("female crew: a name lookup failed (generateCharacter %p, NameImporter.instance %p, names +%d surnames +%d middle +%d, CrewCharacter.name +%d); "
        "new crew keep the game's names", mi, g_fn.fInst, g_fn.offNames, g_fn.offSurnames, g_fn.offMiddle, g_fn.offName);
    return;
  }
  // push rbx; sub rsp,30h; cmp byte ptr [rip+disp32],0; jne rel32 (0x8F) (at rva 0x951CE0)
  static const unsigned char kGen[16] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x80, 0x3D, 0xC3, 0x7C, 0x89, 0x03, 0x00, 0x0F, 0x85, 0x8F };
  unsigned char* p = *(unsigned char**)mi;
  if (!p || memcmp(p + 16, "\0\0\0", 3) != 0) { Log("female crew: generateCharacter is not the code this version expects (game updated?); new crew keep the game's names"); return; }
  uint64_t flag = (uint64_t)p + 13 + (int64_t)*(int32_t*)(p + 8), skip = (uint64_t)p + 0xA2, next = (uint64_t)p + 0x13;
  unsigned char body[43] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x30,          // push rbx; sub rsp,30h
                             0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,          // mov rax, flag
                             0x80, 0x38, 0x00,                            // cmp byte ptr [rax],0
                             0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,          // mov rax, p+0xA2 (mov keeps the flags)
                             0x75, 0x0A,                                  // jne -> jmp rax
                             0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,          // mov rax, p+0x13
                             0xFF, 0xE0 };                                // jmp rax
  memcpy(body + 8, &flag, 8); memcpy(body + 21, &skip, 8); memcpy(body + 33, &next, 8);
  g_genOrig = (fn_genChar)Detour(p, (void*)&Hooked_GenChar, kGen, 16, "CharacterGenerator.generateCharacter", body, sizeof body);
  if (!g_genOrig) { Log("female crew: generateCharacter could not be hooked; new crew keep the game's names"); return; }
  g_femGenOn = true;
  Log("female crew: hooked CharacterGenerator.generateCharacter (rva 0x%llX); new crew are women at female_crew=%d%%, women get one of %d women's first names",
      (unsigned long long)((uintptr_t)p - (uintptr_t)g_ga), FemPct(), kFemFirstN);
}
// ---- female crew, stored gender: the save key and the co-op channel (see "stored gender" above) --------------------
// getParameters (IDataObject vtable entry of CrewCharacter; DataBlock.fillFromObject calls it through the interface and
// deep-copies each Parameter): the array it returns is copied into a new Parameter[] with MOD_FEMALE added. No element
// and no name may be null (getParameterByName throws on either).
typedef void* (*fn_getParams)(void*, void*);
static fn_getParams g_getParamsOrig = nullptr;
typedef void (*fn_setParams)(void*, void*, void*);
static fn_setParams g_setParamsOrig = nullptr;
static bool g_femGTried = false, g_femSaveOn = false, g_femLoadOn = false;
static void* FemSaveAdd(void* c, void* arr) {
  if (!c || !arr || !g_fgd.paramCls || !g_fgd.arrayNew) return arr;
  uint8_t g = FemGGet(c);
  if (!g && FemPct() > 0) { FemIsWoman(c, FemGName(c)); g = FemGGet(c); }  // never seen before: decided now, so the save has it
  if (!g) return arr;                                                     // at 0%, or no name: stays undecided
  FemGNote(c, true);
  int n = ArrLen(arr);
  void** el = (void**)ArrData(arr);
  for (int i = 0; i < n; i++) if (!el[i]) return arr;                     // not the array we expect: left alone
  void* p = il2cpp_object_new(g_fgd.paramCls);
  void* key = il2cpp_string_new("MOD_FEMALE"); void* val = il2cpp_string_new(g == 2 ? "1" : "0");
  void* na = p && key && val ? g_fgd.arrayNew(g_fgd.paramCls, (size_t)n + 1) : nullptr;
  if (!na || ArrLen(na) != n + 1) return arr;
  SetRef(p, (char*)p + g_fgd.offPName, key); SetRef(p, (char*)p + g_fgd.offPValue, val);
  void** nel = (void**)ArrData(na);
  for (int i = 0; i < n; i++) SetRef(na, &nel[i], el[i]);
  SetRef(na, &nel[n], p);
  int t = 0; FemGCount(&FemGCounts::saved, &FemGCounts::savedW, g == 2, &t);
  if (t == 1) Log("female crew: the stored gender is saved with each crew member (MOD_FEMALE key; first: '%s' = %s, female_crew=%d%%)", FemGShown(c).c_str(), g == 2 ? "woman" : "man", FemPct());
  else if (t <= 5 || t % 200 == 0) DLog("female crew: MOD_FEMALE=%s written for '%s' (%d this session)", g == 2 ? "1" : "0", FemGShown(c).c_str(), t);
  return na;
}
static void* FemSaveAddSafe(void* c, void* arr) {
  __try { return FemSaveAdd(c, arr); }
  __except (FaultFilter("female crew (saving the stored gender)", GetExceptionInformation())) { g_femSaveOn = false; return arr; }
}
static void* CrewIdAddSafe(void* c, void* arr);
static void CrewIdReadSafe(void* c, void* params);
static bool g_cidSaveOn = false, g_cidLoadOn = false;                    // crew files: MOD_CREWID written / read back
static void* Hooked_GetParams(void* c, void* mi) {
  void* arr = g_getParamsOrig(c, mi);
  if (g_femSaveOn) arr = FemSaveAddSafe(c, arr);
  return g_cidSaveOn ? CrewIdAddSafe(c, arr) : arr;                    // its own step: FemSaveAdd adds nothing at female_crew 0
}
static bool FemStrIs(void* s, const char* ascii) {
  size_t n = strlen(ascii);
  if (!s || *(int*)((char*)s + 0x10) != (int)n) return false;
  const wchar_t* w = (const wchar_t*)((char*)s + 0x14);
  for (size_t i = 0; i < n; i++) if (w[i] != (wchar_t)(unsigned char)ascii[i]) return false;
  return true;
}
// After setParameters (name and nameCustom are set by now): MOD_FEMALE wins; without it, at female_crew > 0, a crew
// member not decided yet is decided by the name rule (an old save, or a vanilla host's roster).
static void FemLoadRead(void* c, void* params) {
  if (!c || !params) return;
  int n = ArrLen(params); void** el = (void**)ArrData(params);
  uint8_t found = 0; bool odd = false; std::string oddVal;
  for (int i = 0; i < n; i++) {
    void* p = el[i];
    if (!p || !FemStrIs(*(void**)((char*)p + g_fgd.offPName), "MOD_FEMALE")) continue;
    void* v = *(void**)((char*)p + g_fgd.offPValue);
    if (FemStrIs(v, "1")) found = 2; else if (FemStrIs(v, "0")) found = 1; else { odd = true; oddVal = FemUtf8(v); }
    break;
  }
  if (odd) { static int odds = 0; if (++odds <= 3) Log("female crew: '%s' has MOD_FEMALE='%s' in the save, not 0 or 1; ignored", FemGShown(c).c_str(), oddVal.c_str()); }
  if (found) {
    FemGSet(c, found);
    int t = 0; FemGCount(&FemGCounts::loaded, &FemGCounts::loadedW, found == 2, &t);
    if (t <= 5 || t % 200 == 0) Log("female crew: '%s' loaded as a %s (stored gender, MOD_FEMALE=%s; %d loaded this session)", FemGShown(c).c_str(), found == 2 ? "woman" : "man", found == 2 ? "1" : "0", t);
    return;
  }
  if (FemPct() > 0 && !FemGGet(c)) FemIsWoman(c, FemGName(c));        // logs 'first seen ... by the name rule'
}
static void FemLoadReadSafe(void* c, void* params) {
  __try { FemLoadRead(c, params); }
  __except (FaultFilter("female crew (loading the stored gender)", GetExceptionInformation())) { g_femLoadOn = false; }
}
static void Hooked_SetParams(void* c, void* params, void* mi) {
  g_setParamsOrig(c, params, mi);
  if (g_femLoadOn) FemLoadReadSafe(c, params);
  if (g_cidLoadOn) CrewIdReadSafe(c, params);
}
// ---- crew id (crew files): MOD_CREWID, 8 hex digits, saved with each crew member ----
// Kept in the object at CrewCharacter+0x104, the free padding next to the gender byte (0 = none yet; a new object is
// zeroed, so stale pointers never carry an id). Without a saved key (an old save, a vanilla co-op peer) the id is a
// hash of name + selectedEye + selectedEyeBrow, the same on every load until a save writes the key. The mouth is left
// out: the game saves it as SELECTED_EYEMOUTH but reads SELECTED_MOUTH, so it is re-rolled on every load.
// shortcut: two crew with the same name and the same eyes and brows get one id; check generateCharacter if it is seen.
static int g_cidOffEye = -1, g_cidOffBrow = -1;
static uint32_t CrewIdOf(void* c) {
  if (!c || g_fgd.offG != 0x103) return 0;
  uint32_t* slot = (uint32_t*)((char*)c + 0x104);
  if (*slot) return *slot;
  std::string n = FemGName(c);                                         // name (a rename writes nameCustom), set before anyone reads it
  if (n.empty()) return 0;                                             // a bare co-op copy or placeholder: no file
  uint32_t h = 2166136261u;
  auto mix = [&h](const void* p, size_t k) { for (size_t i = 0; i < k; i++) h = (h ^ ((const uint8_t*)p)[i]) * 16777619u; };
  mix(n.data(), n.size());
  if (g_cidOffEye > 0) mix((char*)c + g_cidOffEye, 4);
  if (g_cidOffBrow > 0) mix((char*)c + g_cidOffBrow, 4);
  if (!h) h = 1;
  *slot = h; return h;
}
static void* CrewIdAdd(void* c, void* arr) {
  uint32_t id = CrewIdOf(c);
  if (!id || !arr || !g_fgd.paramCls || !g_fgd.arrayNew) return arr;
  int n = ArrLen(arr); void** el = (void**)ArrData(arr);
  for (int i = 0; i < n; i++) {
    if (!el[i]) return arr;                                            // not the array we expect: left alone
    if (FemStrIs(*(void**)((char*)el[i] + g_fgd.offPName), "MOD_CREWID")) return arr;
  }
  char v[16]; snprintf(v, sizeof v, "%08x", id);
  void* p = il2cpp_object_new(g_fgd.paramCls);
  void* key = il2cpp_string_new("MOD_CREWID"); void* val = il2cpp_string_new(v);
  void* na = p && key && val ? g_fgd.arrayNew(g_fgd.paramCls, (size_t)n + 1) : nullptr;
  if (!na || ArrLen(na) != n + 1) return arr;
  SetRef(p, (char*)p + g_fgd.offPName, key); SetRef(p, (char*)p + g_fgd.offPValue, val);
  void** nel = (void**)ArrData(na);
  for (int i = 0; i < n; i++) SetRef(na, &nel[i], el[i]);
  SetRef(na, &nel[n], p);
  static volatile LONG t = 0;
  if (InterlockedIncrement(&t) == 1) Log("crew files: each crew member's id is saved with them (MOD_CREWID; first: '%s' = %s)", FemGShown(c).c_str(), v);
  return na;
}
static void* CrewIdAddSafe(void* c, void* arr) {
  __try { return CrewIdAdd(c, arr); }
  __except (FaultFilter("crew files (saving the crew id)", GetExceptionInformation())) { g_cidSaveOn = false; return arr; }
}
static void CrewIdRead(void* c, void* params) {
  if (!c || !params || g_fgd.offG != 0x103) return;
  int n = ArrLen(params); void** el = (void**)ArrData(params);
  for (int i = 0; i < n; i++) {
    void* p = el[i];
    if (!p || !FemStrIs(*(void**)((char*)p + g_fgd.offPName), "MOD_CREWID")) continue;
    std::string v = FemUtf8(*(void**)((char*)p + g_fgd.offPValue));
    uint32_t id = (uint32_t)strtoul(v.c_str(), nullptr, 16);
    if (id) { *(uint32_t*)((char*)c + 0x104) = id; return; }
    static int odd = 0; if (++odd <= 3) Log("crew files: '%s' has MOD_CREWID='%s' in the save; ignored, the id is made from the name", FemGShown(c).c_str(), v.c_str());
    break;
  }
  CrewIdOf(c);                                                         // no key: the hash (an old save, a vanilla peer)
}
static void CrewIdReadSafe(void* c, void* params) {
  __try { CrewIdRead(c, params); }
  __except (FaultFilter("crew files (loading the crew id)", GetExceptionInformation())) { g_cidLoadOn = false; }
}
// Session totals, at each battle change when something happened since the last one.
static void FemGSummary() {
  AcquireSRWLockExclusive(&g_femGLock); FemGCounts a = g_femGC, b = g_femGCLast; g_femGCLast = a; ReleaseSRWLockExclusive(&g_femGLock);
  if (!memcmp(&a, &b, sizeof a)) return;
  Log("female crew: stored gender this session: %d new crew (%d women), %d loaded with the key (%d women), %d decided by the name rule (%d women), "
      "%d saved with the key (%d women), co-op copies %d from the owner's flag / %d by the name rule, %d renames kept the gender%s",
      a.created, a.createdW, a.loaded, a.loadedW, a.byName, a.byNameW, a.saved, a.savedW, a.coopOwner, a.coopName, a.renames,
      a.odd ? " (stored byte overwritten by something else!)" : "");
}
static void SafeFemGSummary() { __try { FemGSummary(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }

// Co-op. Sender: ObjectSync.sendCrewToServer fills each CrewStatePersistent from its crew member's character; 17 bytes
// at +0xF6C (rva 0xA9B7AC: mov eax,[rax+0A4h]; mov [r13+3Ch],eax; mov rax,[r14+80h]; rax = the
// character, checked for null just before, r13 = the persistent, r14 = the CrewMember; rcx and the flags are dead until
// +0x1012) jump to a stub that writes typeHair = selectedHair, or with a stored flag
// (selectedHair & 0xFFFF) | 0xBC0000 | g << 24. Vanilla code never reads typeHair (the bare copy below skips it), so
// vanilla peers and a vanilla host (which relays it) are unaffected.
// Receiver: CrewMemberFake.<delayedSetup>d__18.MoveNext (rva 0xA67460) finds the character by special code or builds a
// bare one (name null; it copies colorHair into selectedHair, a vanilla bug) and queues its portrait; the combine runs
// a later frame. After it finishes, the flag goes onto the character: with the marker, from the owner (a bare copy also
// gets the owner's selectedHair back when she is a woman, so her hair style matches); without it, by the name rule on
// the special code's name (up to the first '|'; that is CrewCharacter.name, which a rename keeps). Its prologue has a
// rip-relative cmp inside the first 16 bytes, so the MethodInfo pointer and the vtable entry are patched instead.
struct FemCoopApi { int offState, offThis, offFakeMember, offCmChar, offPsCode, offPsHair, offSelHair; void* getPersistent; };
static FemCoopApi g_fco = { -1, -1, -1, -1, -1, -1, -1, nullptr };
typedef bool (*fn_moveNext)(void*, void*);
static fn_moveNext g_fakeMoveOrig = nullptr;
static bool g_femCoopSendOn = false, g_femCoopRecvOn = false, g_femCoopTried = false;
static int g_femCoopFixHair = 0;
static void FemCoopRecv(void* it) {
  void* fake = *(void**)((char*)it + g_fco.offThis);
  if (!Alive(fake)) return;
  void* cm = *(void**)((char*)fake + g_fco.offFakeMember);
  void* c = cm ? *(void**)((char*)cm + g_fco.offCmChar) : nullptr;
  if (!c) return;
  void* ps = Invoke(g_fco.getPersistent, fake);
  if (!ps) return;
  int v = *(int*)((char*)ps + g_fco.offPsHair);
  std::string code = FemUtf8(*(void**)((char*)ps + g_fco.offPsCode));
  std::string nm = code.substr(0, code.find('|'));
  bool bare = FemGStr(c, g_fgd.offName).empty();
  uint8_t g = FemGGet(c);
  int og = (v >> 24) & 3;
  if (((v >> 16) & 0xFF) == 0xBC && (og == 1 || og == 2)) {
    // the owner's flag wins also over one this PC decided by its own name rule (a roster character relayed without
    // MOD_FEMALE by a vanilla or older host, or a peer with another female_crew)
    if (g && g != og) Log("female crew: remote crew '%s' was a %s here (name rule); the owner says %s", nm.c_str(), g == 2 ? "woman" : "man", og == 2 ? "woman" : "man");
    FemGSet(c, (uint8_t)og);
    bool hair = bare && og == 2;                                       // her own index, also right for a stock look
    if (hair) { *(int*)((char*)c + g_fco.offSelHair) = (int)(int16_t)(v & 0xFFFF); g_femCoopFixHair++; }
    int t = 0; FemGCount(&FemGCounts::coopOwner, nullptr, false, &t);
    if (t <= 5 || t % 100 == 0) Log("female crew: remote crew '%s' is a %s (from the owner%s%s; %d co-op copies from the owner this session)", nm.c_str(), og == 2 ? "woman" : "man",
                                     bare ? ", bare copy" : ", found in a roster", hair ? ", her hair style restored" : "", t);
    return;
  }
  if (g || FemPct() <= 0 || nm.empty()) return;                          // found characters with a flag, or nothing to go on
  bool w = FemNameRule(nm);
  FemGSet(c, w ? 2 : 1);
  int t = 0; FemGCount(&FemGCounts::coopName, nullptr, false, &t);
  if (t <= 5 || t % 100 == 0) Log("female crew: remote crew '%s' is a %s (from the name rule: the owner sent no flag, female_crew=%d%%; %d this session)", nm.c_str(), w ? "woman" : "man", FemPct(), t);
}
static bool Hooked_FakeMoveNext(void* it, void* mi) {
  int before = it && g_fco.offState >= 0 ? *(int*)((char*)it + g_fco.offState) : -1;
  bool r = g_fakeMoveOrig(it, mi);
  if (!r && g_femCoopRecvOn && before != -1 && *(int*)((char*)it + g_fco.offState) == -1) {
    __try { FemCoopRecv(it); }
    __except (FaultFilter("female crew (co-op crew copy)", GetExceptionInformation())) { g_femCoopRecvOn = false; }
  }
  return r;
}
static void FemCoopInstall() {
  g_femCoopTried = true;
  void* fk = FindClass("Core.Multiplayer", "CrewMemberFake"); void* psk = FindClass("Core.Multiplayer", "CrewStatePersistent");
  void* ck = FindClass("Core", "CrewMember"); void* cc = FindClass("Core", "CrewCharacter"); void* osk = FindClass("Core.Multiplayer", "ObjectSync");
  void* nk = nullptr;
  if (fk) { void* iter = nullptr; void* k; while ((k = il2cpp_class_get_nested_types(fk, &iter))) if (!strncmp(il2cpp_class_get_name(k), "<delayedSetup>d__", 17)) nk = k; }
  g_fco.offState = FieldOffset(nk, "<>1__state"); g_fco.offThis = FieldOffset(nk, "<>4__this"); g_fco.offFakeMember = FieldOffset(fk, "crewMember");
  g_fco.offCmChar = FieldOffset(ck, "_character"); g_fco.offPsCode = FieldOffset(psk, "characterSpecialCode"); g_fco.offPsHair = FieldOffset(psk, "typeHair");
  g_fco.offSelHair = FieldOffset(cc, "selectedHair");
  g_fco.getPersistent = fk ? FindMethod(fk, "get_ItemPersistent", 0) : nullptr;
  // the stub hard-codes these offsets, so a game update that moves them turns the channel off
  bool layout = g_fco.offCmChar == 0x80 && g_fco.offPsHair == 0x3C && g_fco.offSelHair == 0xA4 && g_fgd.offG == 0x103;
  // sender
  void* smi = osk ? il2cpp_class_get_method_from_name(osk, "sendCrewToServer", 0) : nullptr;
  unsigned char* site = smi ? *(unsigned char**)smi + 0xF6C : nullptr;
  static const unsigned char kSite[17] = { 0x8B, 0x80, 0xA4, 0x00, 0x00, 0x00, 0x41, 0x89, 0x45, 0x3C, 0x49, 0x8B, 0x86, 0x80, 0x00, 0x00, 0x00 };
  static const unsigned char kBefore[5] = { 0x48, 0x85, 0xC0, 0x0F, 0x84 }, kAfter[3] = { 0x48, 0x85, 0xC0 };   // test rax,rax; je ... / test rax,rax
  if (!layout) Log("female crew: co-op gender channel off: field layout changed (CrewMember._character +0x%X, typeHair +0x%X, selectedHair +0x%X, flag +0x%X)",
                   g_fco.offCmChar, g_fco.offPsHair, g_fco.offSelHair, g_fgd.offG);
  else if (!site || memcmp(site, kSite, 17) || memcmp(site - 9, kBefore, 5) || memcmp(site + 17, kAfter, 3))
    Log("female crew: co-op send patch skipped: ObjectSync.sendCrewToServer is not the code this version expects (game updated?); peers use the name rule");
  else {
    unsigned char st[64] = {
      0x0F, 0xB6, 0x88, 0x03, 0x01, 0x00, 0x00,      // movzx ecx, byte [rax+103h]   the stored gender
      0x83, 0xE1, 0x03,                              // and ecx, 3
      0x8B, 0x80, 0xA4, 0x00, 0x00, 0x00,            // mov eax, [rax+0A4h]          selectedHair (the original load)
      0x85, 0xC9,                                    // test ecx, ecx
      0x74, 0x0D,                                    // jz store
      0x0F, 0xB7, 0xC0,                              // movzx eax, ax
      0xC1, 0xE1, 0x18,                              // shl ecx, 24
      0x09, 0xC8,                                    // or eax, ecx
      0x0D, 0x00, 0x00, 0xBC, 0x00,                  // or eax, 0BC0000h
      0x41, 0x89, 0x45, 0x3C,                        // store: mov [r13+3Ch], eax    (the original store)
      0x49, 0x8B, 0x86, 0x80, 0x00, 0x00, 0x00,      // mov rax, [r14+80h]           (the original reload)
      0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };          // jmp [rip] -> site+17
    const size_t stLen = 50; uint64_t back = (uint64_t)site + 17; memcpy(st + stLen, &back, 8);
    unsigned char* stub = (unsigned char*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    DWORD old = 0;
    if (stub && VirtualProtect(site, 17, PAGE_EXECUTE_READWRITE, &old)) {
      memcpy(stub, st, stLen + 8);
      DWORD tp = 0; VirtualProtect(stub, 64, PAGE_EXECUTE_READ, &tp); FlushInstructionCache(GetCurrentProcess(), stub, 64);
      unsigned char j[17] = { 0xFF, 0x25, 0, 0, 0, 0 }; uint64_t to = (uint64_t)stub; memcpy(j + 6, &to, 8); j[14] = j[15] = j[16] = 0x90;
      memcpy(site, j, 17);                                                  // game thread: sendCrewToServer is not running now
      VirtualProtect(site, 17, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 17);
      g_femCoopSendOn = true;
      Log("female crew: co-op send patch at rva 0x%llX: each crew member's stored gender goes to the other players in typeHair", (unsigned long long)((uintptr_t)site - (uintptr_t)g_ga));
    } else { if (stub) VirtualFree(stub, 0, MEM_RELEASE); Log("female crew: co-op send patch failed (VirtualProtect); peers use the name rule"); }
  }
  // receiver
  void* mmi = nk ? il2cpp_class_get_method_from_name(nk, "MoveNext", 0) : nullptr;
  if (!layout || !mmi || g_fco.offState < 0 || g_fco.offThis < 0 || g_fco.offFakeMember < 0 || g_fco.offPsCode < 0 || !g_fco.getPersistent || g_fgd.offName <= 0) {
    Log("female crew: co-op receive hook off (delayedSetup %p MoveNext %p, state +%d this +%d crewMember +%d specialCode +%d ItemPersistent %p); "
        "other players' crew copies are decided on this PC by their name", nk, mmi, g_fco.offState, g_fco.offThis, g_fco.offFakeMember, g_fco.offPsCode, g_fco.getPersistent);
    return;
  }
  // Unity runs coroutines through the MethodInfo (runtime invoke), compiled code through the vtable: both are patched
  void** m = (void**)mmi; void* orig = m[0];
  void* vtOrig = nullptr;
  bool vt = PatchVtableEntry(nk, mmi, (void*)&Hooked_FakeMoveNext, &vtOrig);
  g_fakeMoveOrig = (fn_moveNext)orig;
  DWORD old = 0;
  if (!VirtualProtect(m, 16, PAGE_READWRITE, &old)) { Log("female crew: co-op receive hook: VirtualProtect failed%s", vt ? " (vtable entry patched only)" : ""); g_femCoopRecvOn = vt; return; }
  bool both = m[1] == orig;                                               // virtualMethodPointer, when it is the same code
  m[0] = (void*)&Hooked_FakeMoveNext; if (both) m[1] = (void*)&Hooked_FakeMoveNext;
  VirtualProtect(m, 16, old, &old);
  g_femCoopRecvOn = true;
  Log("female crew: co-op receive hook on CrewMemberFake.%s.MoveNext (rva 0x%llX; MethodInfo%s%s): other players' crew copies take the owner's stored gender",
      il2cpp_class_get_name(nk), (unsigned long long)((uintptr_t)orig - (uintptr_t)g_ga), both ? " + virtual pointer" : "", vt ? " + vtable entry" : "");
}
// The save key, at every launch (female_crew 0 too, so a save made at 0% keeps the flags). setParameters prologue
// (rva 0x959810): push rbx/rbp/rsi/rdi; sub rsp,48h; cmp byte [rip+X],0 | mov rbx,rdx; mov rsi,rcx;
// jne. The trampoline redoes the cmp with an absolute address; the two movs after it keep the flags for the jne.
static void FemGenderInstall() {
  g_femGTried = true;
  void* cc = FindClass("Core", "CrewCharacter"); void* pk = FindClass("Cotanius.Modules.Data", "Parameter");
  int dm = FieldOffset(cc, "dirtyMesh"), ub = FieldOffset(cc, "usedByMember");
  g_fgd.offName = FieldOffset(cc, "name"); g_fgd.offNameCustom = FieldOffset(cc, "nameCustom");
  g_fgd.offPName = FieldOffset(pk, "name"); g_fgd.offPValue = FieldOffset(pk, "value"); g_fgd.paramCls = pk;
  g_fgd.arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  if (dm != 0x102 || ub != 0x108 || g_fgd.offName <= 0 || g_fgd.offNameCustom <= 0) {
    Log("female crew: stored gender off: CrewCharacter layout changed (dirtyMesh +0x%X, usedByMember +0x%X, name +0x%X); women are decided by name each time", dm, ub, g_fgd.offName);
    return;
  }
  g_fgd.offG = 0x103;                                                      // padding between dirtyMesh and usedByMember
  if (!pk || g_fgd.offPName < 0 || g_fgd.offPValue < 0 || !g_fgd.arrayNew) { Log("female crew: Parameter lookups failed; the stored gender is not saved (decided by name each launch)"); return; }
  void* gmi = il2cpp_class_get_method_from_name(cc, "getParameters", 0);
  g_cidOffEye = FieldOffset(cc, "selectedEye"); g_cidOffBrow = FieldOffset(cc, "selectedEyeBrow");
  if (gmi && PatchVtableEntry(cc, gmi, (void*)&Hooked_GetParams, (void**)&g_getParamsOrig)) g_femSaveOn = g_cidSaveOn = true;
  else Log("female crew: CrewCharacter.getParameters could not be hooked; the stored gender is not saved");
  void* smi = il2cpp_class_get_method_from_name(cc, "setParameters", 1);
  unsigned char* p = smi ? *(unsigned char**)smi : nullptr;
  static const unsigned char kSet[16] = { 0x40, 0x53, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x80, 0x3D, 0xBD, 0x01, 0x89, 0x03, 0x00 };
  static const unsigned char kNext[8] = { 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF1, 0x0F, 0x85 };
  if (!p || memcmp(p + 16, kNext, 8)) Log("female crew: setParameters is not the code this version expects (game updated?); saved genders are not read back");
  else {
    uint64_t flag = (uint64_t)p + 16 + (int64_t)*(int32_t*)(p + 11);
    unsigned char body[22] = { 0x40, 0x53, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x48,   // push rbx/rbp/rsi/rdi; sub rsp,48h
                               0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,                     // mov rax, flag
                               0x80, 0x38, 0x00 };                                     // cmp byte ptr [rax],0
    memcpy(body + 11, &flag, 8);
    g_setParamsOrig = (fn_setParams)Detour(p, (void*)&Hooked_SetParams, kSet, 16, "CrewCharacter.setParameters", body, sizeof body);
    if (g_setParamsOrig) g_femLoadOn = g_cidLoadOn = true;
    else Log("female crew: setParameters could not be hooked; saved genders are not read back");
  }
  Log("female crew: stored gender at CrewCharacter+0x%X; save key MOD_FEMALE %s, read back %s (female_crew=%d%%%s)", g_fgd.offG, g_femSaveOn ? "written" : "OFF",
      g_femLoadOn ? "on load" : "OFF", FemPct(), FemPct() > 0 ? "" : g_noFemales ? (" (no_females=1 overrides female_crew=" + std::to_string(g_femalePct) + "): flags are kept, new crew are stored as men, every body is stock").c_str() : ": flags are kept but nobody is decided and every body is stock");
}

// The new-crew name hook and the co-op gender channel, once each: from FemInstall, or with no_females=1 at once (the
// combineMesh hook then waits until women are switched on).
static bool g_femGenTried = false;
static void FemGenAndCoopInstall();
static void CrewIdInstallLog() { Log("crew files: crew id at CrewCharacter+0x104 (MOD_CREWID %s, read back %s; eye +0x%X, brow +0x%X for saves without it)", g_cidSaveOn ? "written" : "OFF", g_cidLoadOn ? "on load" : "OFF", g_cidOffEye, g_cidOffBrow); }
static void FemGenAndCoopInstall() {
  if (!g_femGenTried) {
    g_femGenTried = true;
    __try { FemGenInstall(); } __except (FaultFilter("female crew (name hook install)", GetExceptionInformation())) { g_femGenOn = false; }
  }
  if (g_fgd.offG > 0 && !g_femCoopTried) {
    __try { FemCoopInstall(); } __except (FaultFilter("female crew (co-op gender install)", GetExceptionInformation())) { g_femCoopSendOn = g_femCoopRecvOn = false; }
  }
}
// On the game thread (the first OnGUI after setup), so combineMesh is not running while it is patched. female_crew=0 at
// launch leaves it unhooked; a Mod Options change from 0 hooks it then.
static void FemInstall() {
  g_femTried = true;
  if (!FemApiReady()) return;
  void* k = FindClass("Core", "CrewMemberVisual");
  void* mi = k ? il2cpp_class_get_method_from_name(k, "combineMesh", 0) : nullptr;
  if (!mi) { Log("female crew: CrewMemberVisual.combineMesh not found; the crew stay stock"); return; }
  // mov [rsp+8],rcx; push rbx/rsi/rdi/r12/r13/r14/r15 (at rva 0x9BD680)
  static const unsigned char kCombine[16] = { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57 };
  void* p = *(void**)mi;
  g_combineOrig = (fn_combine)Detour(p, (void*)&Hooked_Combine, kCombine, 16, "CrewMemberVisual.combineMesh");
  if (!g_combineOrig) { Log("female crew: combineMesh could not be hooked; the crew stay stock"); return; }
  g_femOn = true;
  Log("female crew: hooked CrewMemberVisual.combineMesh (rva 0x%llX), female_crew=%d%%", (unsigned long long)((uintptr_t)p - (uintptr_t)g_ga), FemPct());
  __try { FemGearInstall(); } __except (FaultFilter("female crew (gear hook install)", GetExceptionInformation())) { g_femGearOn = false; }
  FemGenAndCoopInstall();
}
static void TryInstallFemale() {
  if (!g_femGTried) {                                                  // the save key: always, female_crew 0 too
    __try { FemGenderInstall(); } __except (FaultFilter("female crew (stored gender install)", GetExceptionInformation())) { g_femSaveOn = g_femLoadOn = g_cidSaveOn = g_cidLoadOn = false; }
    CrewIdInstallLog();
  }
  if (g_noFemales) FemGenAndCoopInstall();                             // no_females: new crew are stored as men, and co-op
  if (g_femTried || FemPct() <= 0) return;                             // partners still get this player's stored genders
  __try { FemInstall(); } __except (FaultFilter("female crew (install)", GetExceptionInformation())) { g_femOn = false; }
}

