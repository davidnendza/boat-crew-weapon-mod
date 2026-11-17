// stable positions (positions.ini) and the one-time render-style dump (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- stable positions ----------------------------------------------------------------------------------------
// A loadout save stores only the POSITION of a gun in its slot's mounts list, so an item must keep the same position
// forever, no matter how weapons.ini or inventory.ini are edited later. positions.ini remembers, per slot family, the
// item names in position order. New names are appended; a name whose item is gone still occupies its position (hidden).
static std::vector<std::pair<std::string, std::vector<std::string>>> g_positions;

static std::vector<std::string>* PosList(const std::string& family, bool create) {
  for (auto& p : g_positions) if (p.first == family) return &p.second;
  if (!create) return nullptr;
  g_positions.push_back({ family, {} });
  return &g_positions.back().second;
}

// Save files are written to <name>.tmp and then moved over the real file, so a crash mid-write never leaves a half file.
static FILE* OpenForSave(const std::string& path) { return fopen((path + ".tmp").c_str(), "w"); }
static void FinishSave(FILE* f, const std::string& path) {
  bool ok = fflush(f) == 0; fclose(f);
  if (!ok || !MoveFileExA((path + ".tmp").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) Log("could not replace %s (error %lu)", path.c_str(), GetLastError());
}

// A found gun fits every gun slot its stock gun goes in, as in the game (David 2026-11-03: 'some of the weapons can be
// equipped on heavy and heavy+ slots ... but my drops aren't showing up' there; then 'add the dropped guns to all the
// slots that they should be able to go in'). Its family= is the slot it dropped for. Which stock guns each slot family
// offers is learned from the boats as Inject fills them (LearnFamily), seeded with what the log catalogue showed so it
// works before the first boat is built. Built-ins already list their families (SplitFamilies).
static std::map<std::string, std::set<std::string>> g_famBases = {
  { "Tub Turret", { "30 Cal Dual MG (Lewis)", "M1919 Browning", "M2 Browning", "20 mm Oerlikon" } },
  { "Heavy Weapon", { "20 mm Oerlikon", "20 mm Oerlikon Twin", "40 mm Bofors", "40 mm Dual Bofors", "M45 Meat Chopper", "3-inch Gun Player", "20 mm Elco Thunderbolt" } },
  { "Heavy+ Weapon", { "20 mm Oerlikon", "20 mm Oerlikon Twin", "40 mm Bofors", "40 mm Dual Bofors", "M45 Meat Chopper", "3-inch Gun Player", "20 mm Elco Thunderbolt", "5-inch 25 Gun Player" } },
  { "Medium Weapon", { "20 mm Oerlikon", "20 mm Oerlikon Twin", "M45 Meat Chopper", "20 mm Elco Thunderbolt" } },
};
static bool FitsFamily(const Item& it, const std::string& family) {
  if (it.family == family) return true;
  if (it.builtIn) return false;
  if (!it.builtFrom.empty()) {                                         // a found turret gun or side-grade fits only where its mod gun does (its model needs that mount)
    for (const Item& b : g_items) if (b.builtIn && b.name == it.builtFrom && b.family == family) return true;
    return false;
  }
  auto f = g_famBases.find(family);
  return f != g_famBases.end() && f->second.count(it.base) > 0;
}
// Puts a gun's name on the position list of every slot family it fits; true if a list grew.
static bool ListPositions(const Item& it) {
  bool grew = false;
  std::vector<std::string> fams = { it.family };
  for (const auto& f : g_famBases) if (f.first != it.family && FitsFamily(it, f.first)) fams.push_back(f.first);
  for (const std::string& f : fams) {
    std::vector<std::string>* l = PosList(f, true);
    if (std::find(l->begin(), l->end(), it.name) == l->end()) { l->push_back(it.name); grew = true; }
  }
  return grew;
}
static void SavePositions();
static const Item* FindItem(const std::string& name);
// The stock guns in one boat's slot of a family: any not known yet are added, and found guns they fit are listed there.
static void LearnFamily(const std::string& family, const std::unordered_map<std::string, void*>& byBase) {
  std::set<std::string>& known = g_famBases[family];
  std::string added;
  for (const auto& b : byBase) {
    const std::string& n = b.first;                                    // the mod's own clones may carry a copied gun type
    if (n.size() >= 7 && n.compare(n.size() - 7, 7, "(Clone)") == 0) continue;
    if (FindItem(n)) continue;
    if (known.insert(n).second) added += (added.empty() ? "" : ", ") + n;
  }
  if (added.empty()) return;
  int listed = 0;
  for (const Item& it : g_items) if (!it.held && !it.retired && !it.builtIn && it.family != family && FitsFamily(it, family)) {
    std::vector<std::string>* l = PosList(family, true);
    if (std::find(l->begin(), l->end(), it.name) == l->end()) { l->push_back(it.name); listed++; }
  }
  if (listed) SavePositions();
  Log("slot family '%s' also offers %s; %d found %s from other slots listed there", family.c_str(), added.c_str(), listed, IsTorpFamily(family) ? "torpedoes" : "guns");
}

static void SavePositions() {
  std::string path = g_folder + "\\positions.ini";
  FILE* f = OpenForSave(path);
  if (!f) { Log("could not write positions.ini"); return; }
  fputs("# Written by BoatCrewWeaponMod. The order of names under each slot family is the position of that gun in the slot,\n"
        "# which is what your saved loadouts remember. Do not reorder or delete lines; edit weapons.ini or inventory.ini instead\n"
        "# (set retired=1 to hide a gun while keeping its position).\n", f);
  for (auto& p : g_positions) { fprintf(f, "\n[%s]\n", p.first.c_str()); for (auto& n : p.second) fprintf(f, "%s\n", n.c_str()); }
  FinishSave(f, path);
}

static void LoadPositions() {
  std::string path = g_folder + "\\positions.ini";
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return;
  char line[512]; std::vector<std::string>* cur = nullptr;
  while (fgets(line, sizeof line, f)) {
    std::string l = Trim(line);
    if (l.empty() || l[0] == '#') continue;
    if (l[0] == '[' && l.back() == ']') { cur = PosList(l.substr(1, l.size() - 2), true); continue; }
    if (cur) cur->push_back(l);
  }
  fclose(f);
}

static void SyncPositions() {
  bool dirty = false;
  for (const Item& it : g_items) {
    if (it.held) continue;
    if (ListPositions(it)) dirty = true;
  }
  FILE* probe = fopen((g_folder + "\\positions.ini").c_str(), "r");
  if (!probe) dirty = true; else fclose(probe);
  if (dirty) SavePositions();
  for (auto& p : g_positions) DLog("positions: family '%s' has %d extra guns", p.first.c_str(), (int)p.second.size());
}

static const Item* FindItem(const std::string& name) {
  for (const Item& it : g_items) if (it.name == name) return &it;
  return nullptr;
}
// The item of that name in that slot family: a side-grade has a copy per family (SplitFamilies)
static const Item* FindItemIn(const std::string& name, const std::string& family) {
  for (const Item& it : g_items) if (it.name == name && it.family == family) return &it;
  for (const Item& it : g_items) if (it.name == name && FitsFamily(it, family)) return &it;   // a found gun from a sibling slot
  return nullptr;
}

static void* FirstRealGunMount(void* mounts) {
  for (int i = 1; i < ListCount(mounts); i++) { void* m = ListAt(mounts, i); if (m && *(void**)((char*)m + g_offGunType)) return m; }
  return nullptr;
}

static bool RealizePlaceholder(void* ws, void* mounts, int j, const std::string& name, const std::string& family,
                               const std::unordered_map<std::string, void*>& byBase, void* order, void*& orderClone);
static void Inject(void* wsObj) {
  void* order = *(void**)((char*)wsObj + g_offOrder);
  void* mounts = *(void**)((char*)wsObj + g_offMounts);
  if (!order || !mounts || ListCount(mounts) == 0) return;            // enemy and utility systems without a slot family
  std::string family = FamilyOf(order);
  LogCatalogue(family, mounts, wsObj);
  // the guns already in this system and the first mount of each stock gun, read once (a name per mount per position
  // was a managed call each, and the position lists only grow)
  std::unordered_set<std::string> have;
  std::unordered_map<std::string, int> at;                             // where each of them is, for RealizePlaceholder
  std::unordered_map<std::string, void*> byBase;
  for (int i = 0; i < ListCount(mounts); i++) {
    void* m = ListAt(mounts, i); if (!m) continue;
    std::string nm = StripTags(NameOf(m));
    have.insert(nm); at.emplace(nm, i);
    void* gt = *(void**)((char*)m + g_offGunType);
    if (gt) byBase.emplace(NameOf(gt), m);
  }
  LearnFamily(family, byBase);                                         // before the list is read: it may grow
  std::vector<std::string>* registered = PosList(family, false);
  if (!registered || registered->empty()) return;
  std::vector<std::string> names = *registered;                       // a copy: the list can grow while the game runs
  void* orderClone = nullptr;
  int added = 0;
  int realized = 0;
  for (const std::string& name : names) {
    if (have.count(name)) {                                            // this system was already filled (pooled object)
      if (RealizePlaceholder(wsObj, mounts, at[name], name, family, byBase, order, orderClone)) realized++;
      continue;
    }
    const Item* item = FindItemIn(name, family);
    if (item && item->held) item = nullptr;                            // a name reused by loot: not this gun
    Item hidden;                                                       // stands in for an item that is gone, so later positions do not shift
    hidden.name = name; hidden.retired = true;
    const Item* use = item && !item->retired ? item : &hidden;         // retired guns are not built in full (models, shells, sounds)
    auto b = item ? byBase.find(item->base) : byBase.end();
    void* source = b != byBase.end() ? b->second : nullptr;
    if (!source) {
      static std::set<std::string> told;                               // once per gun and slot type: every boat build repeats it
      if (item && told.insert(name + "|" + family).second)
        Log("item '%s': base gun '%s' not found in family '%s'; a hidden placeholder keeps its position", name.c_str(), item->base.c_str(), family.c_str());
      use = &hidden;
      source = FirstRealGunMount(mounts);
      if (!source) break;
    }
    void* mount2 = AddCloneMount(wsObj, mounts, source, *use);
    if (!mount2 && use != &hidden) { use = &hidden; mount2 = AddCloneMount(wsObj, mounts, FirstRealGunMount(mounts), hidden); }
    if (!mount2) { Log("slot '%s': could not add '%s'; the guns after it are left out so no saved position points at the wrong gun", family.c_str(), name.c_str()); break; }
    have.insert(name);
    int index = ListCount(mounts) - 1;
    added++;
    if (use->retired) { DLog("  '%s' -> position %d in '%s' (hidden)", name.c_str(), index, family.c_str()); continue; }
    if (!orderClone) {                                                 // the SelectionOrder asset is shared by every boat: copy it per slot
      orderClone = CloneObject(order);
      if (orderClone) SetRef(wsObj, (char*)wsObj + g_offOrder, orderClone);
    }
    bool listed = orderClone && AddOrderEntry(orderClone, name.c_str(), index);
    void* dn = g_offDisplayName >= 0 ? *(void**)((char*)wsObj + g_offDisplayName) : nullptr;
    DLog("  '%s' -> position %d in '%s' (slot '%s', listed=%d)", name.c_str(), index, family.c_str(), Str(dn), (int)listed);
  }
  if (added) DLog("slot %p: %d guns added, mounts now %d", wsObj, added, ListCount(mounts));
  if (realized) Log("slot '%s' (%s): %d hidden placeholders became the real guns", family.c_str(),
                    g_offDisplayName >= 0 ? Str(*(void**)((char*)wsObj + g_offDisplayName)) : "?", realized);
}

// ---- one-time look at the shared projectile renderers (what decides a tracer's look) ---------------------------
static bool g_stylesDumped = false;
static int g_dumpTries = 0;
static int BoxedInt(void* boxed) { return boxed ? *(int*)((char*)boxed + 0x10) : -1; }
static void DumpRenderStyles() {
  if (g_stylesDumped || g_dumpTries > 60) return;
  g_dumpTries++;
  void* pm = FindClass("Core", "ProjectileManager");
  void* fInst = pm ? il2cpp_class_get_field_from_name(pm, "instance") : nullptr;
  void* fRender = pm ? il2cpp_class_get_field_from_name(pm, "render") : nullptr;
  if (!fInst || !fRender) { Log("ProjectileManager.instance/render not found"); g_dumpTries = 99; return; }
  void* inst = nullptr; il2cpp_field_static_get_value(fInst, &inst);
  if (!inst) return;                                                  // no mission running yet; try again at the next boat spawn
  void* arr = *(void**)((char*)inst + il2cpp_field_get_offset(fRender));
  if (!arr) return;
  g_stylesDumped = true;
  void* matC = FindClass("UnityEngine", "Material"); void* shC = FindClass("UnityEngine", "Shader");
  void* mGetShader = matC ? FindMethod(matC, "get_shader", 0) : nullptr;
  void* mGetColor = matC ? FindMethod(matC, "GetColor", 1, "System.String") : nullptr;
  void* mCount = shC ? FindMethod(shC, "GetPropertyCount", 0) : nullptr;
  void* mPName = shC ? FindMethod(shC, "GetPropertyName", 1, "System.Int32") : nullptr;
  void* mPType = shC ? FindMethod(shC, "GetPropertyType", 1, "System.Int32") : nullptr;
  int n = ArrLen(arr);
  DLog("ProjectileManager.render holds %d style renderers (index = ProjectileStyle: 0 BasicWhite, 1 BasicYellowish, 2 BasicYellow, 3 Rocket, 4 Rocket_ASW, 5 Shell)", n);
  for (int i = 0; i < n; i++) {
    void* r = ArrAt(arr, i);
    if (!r) { DLog("  style %d: null", i); continue; }
    void* rk = il2cpp_object_get_class(r);
    void* fLod = il2cpp_class_get_field_from_name(rk, "LOD");
    void* lod = fLod ? *(void**)((char*)r + il2cpp_field_get_offset(fLod)) : nullptr;
    int ln = ArrLen(lod);
    void* rt = ln ? *(void**)((char*)lod + 0x20) : nullptr;
    if (!rt) { DLog("  style %d: renderer '%s', no LOD render type", i, NameOf(r).c_str()); continue; }
    void* tk = il2cpp_object_get_class(rt);
    int oMesh = FieldOffset(tk, "mesh"), oMat = FieldOffset(tk, "material"), oColor = FieldOffset(tk, "mayColorData"), oMatrix = FieldOffset(tk, "mayMatrixData"), oSpT = FieldOffset(tk, "specialMaterialForType"), oSpB = FieldOffset(tk, "specialMaterialPerBlock");
    void* mesh = oMesh >= 0 ? *(void**)((char*)rt + oMesh) : nullptr;
    void* mat = oMat >= 0 ? *(void**)((char*)rt + oMat) : nullptr;
    DLog("  style %d: renderer '%s', %d LODs, mesh '%s', material '%s', mayColorData=%d mayMatrixData=%d specialMaterialForType=%d specialMaterialPerBlock=%d",
        i, NameOf(r).c_str(), ln, mesh ? NameOf(mesh).c_str() : "-", mat ? NameOf(mat).c_str() : "-",
        oColor >= 0 ? (int)*(bool*)((char*)rt + oColor) : -1, oMatrix >= 0 ? (int)*(bool*)((char*)rt + oMatrix) : -1,
        oSpT >= 0 ? (int)*(bool*)((char*)rt + oSpT) : -1, oSpB >= 0 ? (int)*(bool*)((char*)rt + oSpB) : -1);
    void* sh = (mat && mGetShader) ? Invoke(mGetShader, mat) : nullptr;
    if (!sh) continue;
    DLog("    shader '%s'", NameOf(sh).c_str());
    int cnt = mCount ? BoxedInt(Invoke(mCount, sh)) : 0;
    for (int k = 0; k < cnt && k < 40; k++) {
      int idx = k; void* a[1] = { &idx };
      void* nm = mPName ? Invoke(mPName, sh, a) : nullptr;
      int ty = mPType ? BoxedInt(Invoke(mPType, sh, a)) : -1;
      if (!nm) continue;
      std::string pn = Str(nm);
      if (ty == 0 && mGetColor) {                                     // ShaderPropertyType.Color
        void* a2[1] = { nm }; void* c = Invoke(mGetColor, mat, a2);
        if (c) { float* cf = (float*)((char*)c + 0x10); DLog("      color %s = (%.2f, %.2f, %.2f, %.2f)", pn.c_str(), cf[0], cf[1], cf[2], cf[3]); continue; }
      }
      DLog("      %s (type %d)", pn.c_str(), ty);
    }
  }
}

