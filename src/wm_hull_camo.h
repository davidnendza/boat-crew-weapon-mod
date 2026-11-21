// ---- extra hull camouflage schemes (Measure 22/31/32, Western Approaches, Admiralty 1944 and five fun ones: kCamoSchemes) ----
// The game's camo list is CustomPatternSlot.patterns, a List<ColorPattern> of plain [Serializable] objects on each boat:
// name, GlobalPattern, a key-colour mask texture in the hull's UV layout, a picker sprite and up to 5 ColorEntries
// (_Color slot, colour, channel). selectIndex writes the mask and colours into the hull renderers' property block.
// This file appends the mod's schemes to every player boat's list (CustomPatternSlot.Awake), loads a scheme's mask
// (models\camo_<boat>_<id>.png, made by tools\hull_camo.py) the first time it is picked, and adds picker buttons:
// CamoGroupUI.fillCamo indexes its camoElements buttons by pattern index and throws when there are too few.
// The game saves the camo as a list INDEX ("SLOT~Hull Camo") and colour edits by pattern NAME. A mod camo is saved as
// index 0 (Plain) plus CustomParameter_MOD_CAMO=<scheme name>, so a game without the mod loads Plain; on load the key
// turns select(0) into the scheme. Schemes are only ever appended, in kCamoSchemes order. Co-op still sends the live
// index (untested): a partner without the mod has no clamp for an index past the stock five.
#include "wm_hull_camo_data.h"
static void KillMarksAttach(void* slot);                             // wm_kill_marks.h, included at the end of this file
static int FaultFilter(const char* job, EXCEPTION_POINTERS* ep);
static const char* g_kmStep = "";                                    // the kill-marks step a fault happened in

static struct {
  bool ready = false;
  int offPatterns = -1, offName = -1, offGlobal = -1, offBaseTex = -1, offAddr = -1, offSprite = -1, offColors = -1, offSyncs = -1, offActivate = -1;
  int offEntName = -1, offEntDefault = -1, offEntChannel = -1, offEntRealtime = -1, offEntAllowEdit = -1, offEntSlot = -1;
  int offSyncCode = -1, offSyncSource = -1, offGuid = -1, offElems = -1, offCamoGroup = -1;
  void *fInstance = nullptr, *spriteCreate = nullptr, *apply = nullptr, *texW = nullptr, *texH = nullptr;
  void* (*arrayNew)(void*, size_t) = nullptr;
  int maxPatterns = 0;                                                  // longest camo list seen, so the picker gets as many buttons
} g_camo;
static void (*g_patternAwakeOrig)(void*, void*) = nullptr;
static void (*g_patternSelectOrig)(void*, int, void*) = nullptr;
static void (*g_camoGroupEnableOrig)(void*, void*) = nullptr;
static constexpr int kCamoCount = (int)(sizeof(kCamoSchemes) / sizeof(kCamoSchemes[0]));

// Which boat a camo slot belongs to: the GUID of its second stock pattern's addressable mask (PT-20 and PT-109 share Plain).
static int CamoBoatOf(void* patterns) {
  if (ListCount(patterns) < 2 || g_camo.offGuid < 0) return -1;
  void* ref = *(void**)((char*)ListAt(patterns, 1) + g_camo.offAddr);
  const char* g = ref ? Str(*(void**)((char*)ref + g_camo.offGuid)) : "";
  for (int b = 0; b < (int)(sizeof(kCamoBoats) / sizeof(kCamoBoats[0])); b++)
    if (!strncmp(g, kCamoBoats[b].guid8, 8)) return b;
  return -1;
}
static int CamoSchemeOf(void* pattern) {
  const char* n = Str(*(void**)((char*)pattern + g_camo.offName));
  for (int s = 0; s < kCamoCount; s++) if (!strcmp(n, kCamoSchemes[s].name)) return s;
  return -1;
}

static void* CamoIcon(int s) {                                          // picker sprite from models\camo_icon_<id>.png, once
  static void* got[kCamoCount] = {};
  static bool tried[kCamoCount] = {};
  if (got[s] && Alive(got[s])) return got[s];
  if (got[s]) { got[s] = nullptr; tried[s] = false; }                   // destroyed: build it again
  if (tried[s] || !g_camo.spriteCreate) return nullptr;
  tried[s] = true;
  void* tex = nullptr; std::string name = std::string("camo_icon_") + kCamoSchemes[s].id + ".png";
  if (LoadPngTex(g_folder + "\\models\\" + name, &tex)) { Log("hull camo: models\\%s missing or unreadable; %s has no picture in the camo list", name.c_str(), kCamoSchemes[s].name); return nullptr; }
  void* w = g_camo.texW ? Invoke(g_camo.texW, tex) : nullptr; void* h = g_camo.texH ? Invoke(g_camo.texH, tex) : nullptr;
  float rect[4] = { 0, 0, w ? (float)*(int*)((char*)w + 0x10) : 220.0f, h ? (float)*(int*)((char*)h + 0x10) : 110.0f }, pivot[2] = { 0.5f, 0.5f }, ppu = 100;
  void* a[4] = { tex, rect, pivot, &ppu };
  got[s] = Invoke(g_camo.spriteCreate, nullptr, a);
  if (got[s]) Pin(got[s]);
  else Log("hull camo: Sprite.Create failed for %s (%.0fx%.0f); it has no picture in the camo list", name.c_str(), rect[2], rect[3]);
  return got[s];
}

static void* CamoMask(int boat, int s) {                                // the scheme's mask for this boat, loaded on first pick
  static std::map<int, void*> got;
  void*& t = got[boat * 64 + s];
  if (t && Alive(t)) return t;
  std::string name = std::string("camo_") + kCamoBoats[boat].boat + "_" + kCamoSchemes[s].id + ".png";
  std::string det;
  int r = LoadPngTex(g_folder + "\\models\\" + name, &t, &det);
  if (r) { Log("hull camo: models\\%s %s; Plain is picked instead", name.c_str(), r == 1 ? "not found" : "could not be loaded"); t = nullptr; return nullptr; }
  // No DXT: it put the wrong key on 4x4 blocks where three keys meet (0.4-1% of texels, seen as dashes in game). 16 MB each
  bool mips = false, unreadable = true;
  if (g_camo.apply) { void* a[2] = { &mips, &unreadable }; Invoke(g_camo.apply, t, a); }
  Log("hull camo: loaded %s for the %s", kCamoSchemes[s].name, kCamoBoats[boat].boat);
  return t;
}

// One new ColorPattern built from the boat's Plain pattern: its classes, its sync codes and its activateObjects list.
static void* MakeCamoPattern(void* plain, int s) {
  const CamoScheme& sc = kCamoSchemes[s];
  void* kPat = il2cpp_object_get_class(plain);
  void* colors = *(void**)((char*)plain + g_camo.offColors);
  void* syncs = *(void**)((char*)plain + g_camo.offSyncs);
  if (!colors || ArrLen(colors) < 1) return nullptr;
  void* kEnt = il2cpp_object_get_class(ArrAt(colors, 0));
  void* p = il2cpp_object_new(kPat);
  void* ctor = il2cpp_class_get_method_from_name(kPat, ".ctor", 0);
  if (!p || !ctor) return nullptr;
  Invoke(ctor, p);
  char* P = (char*)p;
  SetRef(p, P + g_camo.offName, il2cpp_string_new(sc.name));
  *(int*)(P + g_camo.offGlobal) = sc.global;                          // fleet boats copy the player's camo by this kind
  SetRef(p, P + g_camo.offBaseTex, nullptr);                          // loaded on first pick (CamoMask)
  SetRef(p, P + g_camo.offAddr, nullptr);                             // no addressable: the game then uses baseTexture
  SetRef(p, P + g_camo.offSprite, CamoIcon(s));
  void* arr = g_camo.arrayNew(kEnt, sc.ncols);
  void* entCtor = il2cpp_class_get_method_from_name(kEnt, ".ctor", 0);
  if (!arr || !entCtor) return nullptr;
  int dark = 1;
  for (int i = 0; i < sc.ncols; i++) {
    const CamoColour& c = sc.cols[i];
    void* e = il2cpp_object_new(kEnt); Invoke(entCtor, e);
    char* E = (char*)e; float col[4] = { c.r, c.g, c.b, 1.0f };
    SetRef(e, E + g_camo.offEntName, il2cpp_string_new(c.name));
    memcpy(E + g_camo.offEntDefault, col, 16);
    memcpy(E + g_camo.offEntRealtime, col, 16);
    *(int*)(E + g_camo.offEntChannel) = c.channel;
    *(bool*)(E + g_camo.offEntAllowEdit) = true;
    *(int*)(E + g_camo.offEntSlot) = c.slot;
    if (c.channel == 2) dark = c.slot;
    SetRef(arr, (char*)ArrData(arr) + 8 * i, e);
  }
  SetRef(p, P + g_camo.offColors, arr);
  // deck guns and fittings take a hull colour through the sync codes: weapons and intakes the dark one, the rest the base
  void* mySyncs = syncs ? CopyList(syncs) : nullptr;
  for (int i = 0; mySyncs && i < ListCount(mySyncs); i++) {
    void* old = ListAt(mySyncs, i); void* kSync = il2cpp_object_get_class(old);
    void* e = il2cpp_object_new(kSync);
    const char* code = Str(*(void**)((char*)old + g_camo.offSyncCode));
    bool darkPart = !strcmp(code, "TwoColorWeapon") || !strcmp(code, "DeckIntakes") || !strcmp(code, "DeckRockets");
    SetRef(e, (char*)e + g_camo.offSyncCode, *(void**)((char*)old + g_camo.offSyncCode));
    *(int*)((char*)e + g_camo.offSyncSource) = darkPart ? dark : 1;
    ListSetAt(mySyncs, i, e);
  }
  if (mySyncs) SetRef(p, P + g_camo.offSyncs, mySyncs);
  SetRef(p, P + g_camo.offActivate, *(void**)((char*)plain + g_camo.offActivate));   // shown like Plain; ours come later in the list
  return p;
}

static void EnsureCamoButtons(void* group) {                            // CamoGroupUI: one button per pattern of the longest list
  if (!Alive(group)) return;                                            // instance can outlive the shipyard scene
  void* elems = *(void**)((char*)group + g_camo.offElems);
  int n = ListCount(elems);
  if (!elems || n < 1 || n >= g_camo.maxPatterns) return;
  void* last = ListAt(elems, n - 1);
  void* parent = Invoke(m_getParent, Invoke(m_getTransform, last));
  // The stock buttons are placed by hand (no layout group: a copy lands exactly on the one it copies) and the list does
  // not scroll, so the copies go in extra rows of n under the stock row: same step along the row, one button size
  // (plus the stock gap) across it.
  static void *getPos = nullptr, *setPos = nullptr, *getRect = nullptr;
  if (!getPos) { void* rt = FindClass("UnityEngine", "RectTransform");
                 getPos = rt ? FindMethod(rt, "get_anchoredPosition", 0) : nullptr; setPos = rt ? FindMethod(rt, "set_anchoredPosition", 1) : nullptr;
                 getRect = rt ? FindMethod(rt, "get_rect", 0) : nullptr; }
  float p0[2] = {}, step[2] = {}, cross[2] = {};
  bool place = getPos && setPos && getRect && n >= 2;
  if (place) {
    void* a0 = Invoke(getPos, Invoke(m_getTransform, ListAt(elems, 0))); void* a1 = Invoke(getPos, Invoke(m_getTransform, ListAt(elems, 1)));
    void* rc = Invoke(getRect, Invoke(m_getTransform, ListAt(elems, 0)));
    place = a0 && a1 && rc;
    if (place) {
      memcpy(p0, (char*)a0 + 0x10, 8); float p1[2]; memcpy(p1, (char*)a1 + 0x10, 8); float r[4]; memcpy(r, (char*)rc + 0x10, 16);
      step[0] = p1[0] - p0[0]; step[1] = p1[1] - p0[1];
      bool across = fabsf(step[0]) >= fabsf(step[1]);                   // a row: the next rows go down; a column: to the right
      float gap = across ? fabsf(step[0]) - r[2] : fabsf(step[1]) - r[3];
      if (gap < 0) gap = 0;
      if (across) cross[1] = -(r[3] + gap); else cross[0] = r[2] + gap;
      Log("hull camo: stock camo buttons from (%.0f, %.0f) every (%.0f, %.0f), %.0fx%.0f; extra rows every (%.0f, %.0f)", p0[0], p0[1], step[0], step[1], r[2], r[3], cross[0], cross[1]);
    }
  }
  bool keepLocal = false;
  int cols = n;
  while (ListCount(elems) < g_camo.maxPatterns) {
    void* b = CloneObject(last);
    if (!b) { Log("hull camo: could not copy a camo button; the list shows %d of %d camos", ListCount(elems), g_camo.maxPatterns); return; }
    void* tr = Invoke(m_getTransform, b);
    if (g_setParentTwoArgs) { void* a[2] = { parent, &keepLocal }; Invoke(m_setParent, tr, a); }
    else { void* a[1] = { parent }; Invoke(m_setParent, tr, a); }
    if (place) {
      int j = ListCount(elems), col = j % cols, row = j / cols;
      float pos[2] = { p0[0] + col * step[0] + row * cross[0], p0[1] + col * step[1] + row * cross[1] };
      void* a[1] = { pos }; Invoke(setPos, tr, a);
    }
    if (!ListAdd(elems, b)) { Log("hull camo: could not add a camo button; the list shows %d of %d camos", ListCount(elems), g_camo.maxPatterns); return; }
  }
  Log("hull camo: the camo list now has %d buttons%s", ListCount(elems), place ? "" : " (not placed: RectTransform lookups failed, they sit on the last one)");
}
static void* CamoGroupNow() {                                           // CraftCustomizationManager.instance.camoGroup, if open
  void* mgr = nullptr;
  if (g_camo.fInstance) il2cpp_field_static_get_value(g_camo.fInstance, &mgr);
  return mgr && g_camo.offCamoGroup >= 0 ? *(void**)((char*)mgr + g_camo.offCamoGroup) : nullptr;
}

static void CamoAddTo(void* slot) {
  void* patterns = *(void**)((char*)slot + g_camo.offPatterns);
  int n = ListCount(patterns);
  if (n < 2) return;
  for (int i = 0; i < n; i++) if (CamoSchemeOf(ListAt(patterns, i)) >= 0) return;   // a copy of a boat that already has them
  int boat = CamoBoatOf(patterns);
  if (boat < 0) { DLog("hull camo: a camo list (%d patterns) of no known boat; left as it is", n); return; }
  void* plain = ListAt(patterns, 0);
  int added = 0;
  for (int s = 0; s < kCamoCount; s++) { void* p = MakeCamoPattern(plain, s); if (p && ListAdd(patterns, p)) added++; else break; }
  int total = ListCount(patterns);
  if (total > g_camo.maxPatterns) g_camo.maxPatterns = total;
  static std::set<int> told;
  if (told.insert(boat).second || added != kCamoCount) Log("hull camo: %d schemes added to the %s (%d camos)", added, kCamoBoats[boat].boat, total);
  EnsureCamoButtons(CamoGroupNow());
}
static void Hooked_PatternAwake(void* self, void* method) {
  static int faults = 0;
  if (g_camo.ready && faults < 5) {
    __try { CamoAddTo(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("hull camo: faults while adding schemes; no more are added this session"); }
  }
  g_patternAwakeOrig(self, method);
}

// ---- save format: a mod camo is saved as Plain plus a MOD_CAMO key, so a save never holds an index past the stock five.
// Without the mod (or with a scheme a later version dropped) the boat simply loads in Plain, the default grey.
// Saving: CustomCollection.getParameters (IDataObject vtable entry; the save, saved loadouts and lastConfig all use it)
// returns a fresh Parameter[]: its "SLOT~Hull Camo" element is swapped for a new one with "0" and
// "CustomParameter_MOD_CAMO" = scheme name is added (or replaces a live one a load has not used yet); the live objects
// are left alone.
// Loading: setParameters files CustomParameter_ entries with setCustomParameter, then validate() selects each slot from
// parameters[slot.name]; that select(0) reaches the selectIndex hook, which finds MOD_CAMO on the boat's collection, picks
// the scheme instead, writes its index into the live "Hull Camo" parameter (so a later validate keeps it) and removes
// MOD_CAMO (so a later pick of Plain stays Plain).
static const char* kCamoSlot = "Hull Camo";
static const char* kCamoKey = "MOD_CAMO";
static void* (*g_collGetParamsOrig)(void*, void*) = nullptr;
static struct {
  bool on = false;
  void *kColl = nullptr, *kParam = nullptr, *collType = nullptr, *getSlot = nullptr, *getCustom = nullptr, *removeCustom = nullptr, *inParent = nullptr;
  int offPName = -1, offPValue = -1, offPType = -1, offMain = -1, offParams = -1;
} g_cs;
static void* CamoParam(const char* name, const char* value, void* like) {
  void* p = il2cpp_object_new(g_cs.kParam);
  void* ctor = il2cpp_class_get_method_from_name(g_cs.kParam, ".ctor", 0);
  if (!p || !ctor) return nullptr;
  Invoke(ctor, p);
  SetRef(p, (char*)p + g_cs.offPName, il2cpp_string_new(name));
  SetRef(p, (char*)p + g_cs.offPValue, il2cpp_string_new(value));
  if (like) *(int*)((char*)p + g_cs.offPType) = *(int*)((char*)like + g_cs.offPType);
  return p;
}
static void* CamoSlotOf(void* coll) {                                  // the collection's CustomPatternSlot for "Hull Camo"
  void* a[1] = { il2cpp_string_new(kCamoSlot) };
  void* cs = Invoke(g_cs.getSlot, coll, a);
  return cs ? *(void**)((char*)cs + g_cs.offMain) : nullptr;
}
static void* CamoSaveSwap(void* coll, void* arr) {
  int n = ArrLen(arr);
  void** el = (void**)ArrData(arr);
  int at = -1, old = -1;                                               // old: a live MOD_CAMO not yet used by a load
  std::string keyName = std::string("CustomParameter_") + kCamoKey;
  for (int i = 0; i < n; i++) {
    if (!el[i]) return arr;
    const char* nm = Str(*(void**)((char*)el[i] + g_cs.offPName));
    if (!strcmp(nm, "SLOT~Hull Camo")) at = i; else if (keyName == nm) old = i;
  }
  if (at < 0) return arr;
  void* slot = CamoSlotOf(coll);
  if (!slot) return arr;
  void* patterns = *(void**)((char*)slot + g_camo.offPatterns);
  int idx = atoi(Str(*(void**)((char*)el[at] + g_cs.offPValue)));
  int s = idx >= 0 && idx < ListCount(patterns) ? CamoSchemeOf(ListAt(patterns, idx)) : -1;
  if (s < 0) {                                                         // a stock camo: saved as the game does it,
    if (old < 0) return arr;                                           // minus a MOD_CAMO no load used, which would
    void* na = g_camo.arrayNew(g_cs.kParam, (size_t)(n - 1));          // turn a later Plain pick into the scheme
    if (!na) return arr;
    void** nel = (void**)ArrData(na);
    for (int i = 0, j = 0; i < n; i++) if (i != old) SetRef(na, &nel[j++], el[i]);
    return na;
  }
  void* plain = CamoParam("SLOT~Hull Camo", "0", el[at]);
  void* key = CamoParam(keyName.c_str(), kCamoSchemes[s].name, el[at]);
  int m = old >= 0 ? n : n + 1;
  void* na = plain && key ? g_camo.arrayNew(g_cs.kParam, (size_t)m) : nullptr;
  if (!na) return arr;
  void** nel = (void**)ArrData(na);
  for (int i = 0; i < n; i++) SetRef(na, &nel[i], i == at ? plain : i == old ? key : el[i]);
  if (old < 0) SetRef(na, &nel[n], key);
  static int told = 0;
  if (++told <= 3) Log("hull camo: saved as Plain + %s=%s (a game without the mod loads it in Plain)", kCamoKey, kCamoSchemes[s].name);
  return na;
}
static void* Hooked_CollGetParams(void* coll, void* mi) {
  static int faults = 0;
  void* arr = g_collGetParamsOrig(coll, mi);
  if (g_cs.on && faults < 5 && arr) {
    __try { arr = CamoSaveSwap(coll, arr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("hull camo: faults while saving; mod camos are saved by index again (a save from then on needs the mod)"); }
  }
  return arr;
}
static void CamoLoadRedirect(void* slot, int* i) {                      // select(0) from validate(): MOD_CAMO wins
  if (!g_cs.on) return;
  bool inactive = true; void* a[2] = { g_cs.collType, &inactive };
  void* coll = Invoke(g_cs.inParent, slot, a);
  if (!coll || CamoSlotOf(coll) != slot) return;
  void* ka[1] = { il2cpp_string_new(kCamoKey) };
  void* p = Invoke(g_cs.getCustom, coll, ka);
  if (!p) return;
  void* dict = *(void**)((char*)coll + g_cs.offParams);
  void* getItem = dict ? il2cpp_class_get_method_from_name(il2cpp_object_get_class(dict), "get_Item", 1) : nullptr;
  void* ga[1] = { il2cpp_string_new(kCamoSlot) };
  void* live = getItem ? Invoke(getItem, dict, ga) : nullptr;
  if (*i != 0) {                                                       // a stock camo saved by a game without the mod: the key is stale,
    std::string cur = live ? std::string(Str(*(void**)((char*)live + g_cs.offPValue))) : std::string();   // and would turn the next Plain pick into the scheme
    if (!cur.empty() && atoi(cur.c_str()) == *i) { Invoke(g_cs.removeCustom, coll, ka); DLog("hull camo: stock camo %d is saved, dropped the unused %s key", *i, kCamoKey); }
    return;
  }
  std::string want = Str(*(void**)((char*)p + g_cs.offPValue));
  Invoke(g_cs.removeCustom, coll, ka);
  void* patterns = *(void**)((char*)slot + g_camo.offPatterns);
  for (int k = 0; k < ListCount(patterns); k++) {
    if (strcmp(Str(*(void**)((char*)ListAt(patterns, k) + g_camo.offName)), want.c_str())) continue;
    *i = k;
    if (live) SetRef(live, (char*)live + g_cs.offPValue, il2cpp_string_new(std::to_string(k).c_str()));
    DLog("hull camo: loaded %s from the save's %s key (camo %d)", want.c_str(), kCamoKey, k);
    return;
  }
  Log("hull camo: the save asks for camo '%s', which this version does not have; Plain instead", want.c_str());
}

static void CamoBeforeSelect(void* self, int* i) {
  CamoLoadRedirect(self, i);
  void* patterns = *(void**)((char*)self + g_camo.offPatterns);
  int n = ListCount(patterns);
  if ((*i >= n || *i < 0) && n > 0) { Log("hull camo: camo %d picked but the list has %d; Plain instead (a save from a newer mod?)", *i, n); *i = 0; }
  if (*i < 0 || *i >= n) return;
  void* p = ListAt(patterns, *i);
  int s = CamoSchemeOf(p);
  if (s < 0 || Alive(*(void**)((char*)p + g_camo.offBaseTex))) return;
  int boat = CamoBoatOf(patterns);
  void* tex = boat >= 0 ? CamoMask(boat, s) : nullptr;
  if (tex) SetRef(p, (char*)p + g_camo.offBaseTex, tex);
  else *i = 0;                                                         // no mask: the hull would turn one flat colour
}
// The game never uses ColorPattern.baseTexture: without an addressable mask modifyPropertyBlockByMaterialName sets
// _MainTex to Texture2D.whiteTexture (all key 1, a flat hull). So after it ran, the mod's mask goes into the same
// per-material property blocks it just wrote: the slot's renderers list and every Renderer under targetHolder, each
// material whose name contains targetMaterial.
static struct { void *rendererType = nullptr, *inChildren = nullptr, *getMats = nullptr, *getBlock = nullptr, *setBlock = nullptr, *setTex = nullptr, *block = nullptr, *matName = nullptr;
                int offRenderers = -1, offHolder = -1, offTarget = -1, offSelected = -1; } g_cm;
static bool CamoMaskOn(void* r, const char* target, void* tex) {
  void* mats = Invoke(g_cm.getMats, r);
  void* texName = il2cpp_string_new("_MainTex");
  bool hit = false;
  for (int k = 0; k < ArrLen(mats); k++) {
    void* m = ArrAt(mats, k);
    if (!Alive(m) || !strstr(Str(Invoke(g_cm.matName, m)), target)) continue;
    void* ga[2] = { g_cm.block, &k }; Invoke(g_cm.getBlock, r, ga);
    void* ta[2] = { texName, tex }; Invoke(g_cm.setTex, g_cm.block, ta);
    void* sa[2] = { g_cm.block, &k }; Invoke(g_cm.setBlock, r, sa);
    hit = true;
  }
  return hit;
}

// The lower-detail hull meshes (LOD1+) share the mask but not LOD0's UV layout (PC-461 LOD2: 39% of its triangles land
// on texels painted for another place), and the long PC-461 drops to them at play distance. ForceLOD is stripped, so
// while a mod camo is on: LODGroup off, its LOD1+ renderers off, LOD0 always drawn. A stock camo puts both back.
static struct { void *lodType = nullptr, *inParent = nullptr, *getLODs = nullptr, *groupEnable = nullptr, *rendEnable = nullptr, *rendEnabled = nullptr;
                int elem = 0, offRend = -1; bool on = false; } g_lod;
struct CamoLodHold { void* group; std::vector<void*> hidden; };        // hidden: LOD1+ renderers that were on
struct CamoLodHeld { std::vector<CamoLodHold> groups; std::vector<void*> pins; };   // pins keep every pointer here valid
static std::map<void*, CamoLodHeld> g_lodHeld;                         // slot (pinned) -> groups held at full detail
static void CamoLodRelease(std::map<void*, CamoLodHeld>::iterator it, bool restore) {
  bool t = true; void* a[1] = { &t };
  if (restore)
    for (auto& h : it->second.groups) {
      if (Alive(h.group)) Invoke(g_lod.groupEnable, h.group, a);
      for (void* r : h.hidden) if (Alive(r)) Invoke(g_lod.rendEnable, r, a);
    }
  for (void* pin : it->second.pins) Unpin(pin);
  g_lodHeld.erase(it);
}
static void CamoLodRestore(void* slot) {
  auto it = g_lodHeld.find(slot);
  if (it != g_lodHeld.end()) CamoLodRelease(it, true);
}
static void CamoLodPin(void* slot, const std::vector<void*>& hull) {
  if (!g_lod.on) return;
  for (auto it = g_lodHeld.begin(); it != g_lodHeld.end();) {          // boats gone with their scene: just let go
    auto next = std::next(it);
    if (!Alive(it->first)) CamoLodRelease(it, false);
    it = next;
  }
  CamoLodRestore(slot);
  CamoLodHeld held;
  held.pins.push_back(Pin(slot));
  bool f = false; void* off[1] = { &f };
  int hidden = 0;
  for (void* r : hull) {
    bool inactive = true; void* a[2] = { g_lod.lodType, &inactive };
    void* g = Invoke(g_lod.inParent, r, a);
    if (!Alive(g) || std::any_of(held.groups.begin(), held.groups.end(), [&](const CamoLodHold& h) { return h.group == g; })) continue;
    void* lods = Invoke(g_lod.getLODs, g);
    int n = ArrLen(lods);
    if (n < 2) continue;
    auto rends = [&](int i) { return *(void**)((char*)lods + 0x20 + (size_t)g_lod.elem * i + g_lod.offRend); };
    std::set<void*> lod0;
    void* r0 = rends(0);
    for (int k = 0; k < ArrLen(r0); k++) lod0.insert(ArrAt(r0, k));
    CamoLodHold h{ g, {} };
    std::set<void*> seen;
    for (int i = 1; i < n; i++) {
      void* rs = rends(i);
      for (int k = 0; k < ArrLen(rs) && k < 256; k++) {
        void* x = ArrAt(rs, k);
        if (!Alive(x) || lod0.count(x) || !seen.insert(x).second) continue;
        void* on = Invoke(g_lod.rendEnabled, x);
        if (!on || !*(bool*)((char*)on + 0x10)) continue;               // already off (the game's choice): leave it
        Invoke(g_lod.rendEnable, x, off); h.hidden.push_back(x); held.pins.push_back(Pin(x));
      }
    }
    Invoke(g_lod.groupEnable, g, off);                                 // shortcut: LODGroup has no enabled getter here, assumed on
    hidden += (int)h.hidden.size();
    held.pins.push_back(Pin(g));
    held.groups.push_back(h);
  }
  int groups = (int)held.groups.size();
  if (groups) g_lodHeld[slot] = held;
  else for (void* pin : held.pins) Unpin(pin);
  DLog("hull camo: %d LOD group(s) held at full detail, %d low-detail renderers off", groups, hidden);
}
static void CamoAfterSelect(void* self) {
  if (!g_cm.block) return;
  void* patterns = *(void**)((char*)self + g_camo.offPatterns);
  int i = *(int*)((char*)self + g_cm.offSelected);                      // just stored by selectIndex
  if (i < 0 || i >= ListCount(patterns)) return;
  void* p = ListAt(patterns, i);
  void* tex = CamoSchemeOf(p) >= 0 ? *(void**)((char*)p + g_camo.offBaseTex) : nullptr;
  if (!Alive(tex)) { CamoLodRestore(self); return; }
  const char* target = Str(*(void**)((char*)self + g_cm.offTarget));
  std::string tgt = target;
  int n = 0;
  void* list = *(void**)((char*)self + g_cm.offRenderers);
  std::vector<void*> hull;
  for (int k = 0; k < ListCount(list); k++) if (void* r = ListAt(list, k)) if (Alive(r)) { if (CamoMaskOn(r, tgt.c_str(), tex)) hull.push_back(r); n++; }
  void* holder = *(void**)((char*)self + g_cm.offHolder);
  if (Alive(holder)) {
    void* go = Invoke(m_getGameObject, holder);
    bool typed = true, recursive = true, inactive = true, reverse = false;
    void* a[6] = { g_cm.rendererType, &typed, &recursive, &inactive, &reverse, nullptr };
    void* arr = go ? Invoke(g_cm.inChildren, go, a) : nullptr;
    for (int k = 0; k < ArrLen(arr) && k < 512; k++) if (void* r = ArrAt(arr, k)) { if (CamoMaskOn(r, tgt.c_str(), tex)) hull.push_back(r); n++; }
  }
  DLog("hull camo: mask on %d renderers (material '%s')", n, tgt.c_str());
  CamoLodPin(self, hull);
}
static void Hooked_PatternSelect(void* self, int i, void* method) {
  static int faults = 0;
  if (g_camo.ready && faults < 5) {
    __try { CamoBeforeSelect(self, &i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("hull camo: faults while picking a camo; the mod's camos may show a flat hull"); }
  }
  g_patternSelectOrig(self, i, method);
  if (g_camo.ready && faults < 5) {
    __try { CamoAfterSelect(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("hull camo: faults while painting a camo; the mod's camos may show a flat hull"); }
  }
  static int kmFaults = 0;                                             // kill markings hang off every camo pick, stock ones too
  if (g_cm.block && kmFaults < 5) {
    __try { KillMarksAttach(self); }
    __except (FaultFilter(g_kmStep, GetExceptionInformation())) { if (++kmFaults == 5) Log("kill marks: faults while hanging the marks; boats may show none"); }
  }
}
static void Hooked_CamoGroupEnable(void* self, void* method) {
  static int faults = 0;
  if (g_camo.ready && faults < 5) {
    __try { EnsureCamoButtons(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("hull camo: faults while adding camo buttons; the mod's camos may be missing from the list"); }
  }
  g_camoGroupEnableOrig(self, method);
}

static bool SwapMethodPointer(void* mi, void* hook, void** orig) {     // Unity messages are called through MethodInfo
  void** slot = (void**)mi; DWORD old = 0;
  if (!mi || !VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
  *orig = *slot; *slot = hook;
  VirtualProtect(slot, sizeof(void*), old, &old);
  return true;
}
static void HullCamoInstall() {
  void* kSlot = FindClass("Core.Customization", "CustomPatternSlot");
  void* kPat = FindClass("Core.Customization", "ColorPattern");
  void* kRef = FindClass("UnityEngine.AddressableAssets", "AssetReference");
  void* kGroup = FindClass("Core.Customization", "CamoGroupUI");
  void* kMgr = FindClass("Core.Customization", "CraftCustomizationManager");
  void* kSprite = FindClass("UnityEngine", "Sprite"); void* kTex = FindClass("UnityEngine", "Texture2D");
  void* kEnt = nullptr; void* kSync = nullptr;
  if (kPat) {                                                           // nested: ColorPattern.ColorEntry, ColorPattern.SyncEntry
    void* it = nullptr; void* k;
    while ((k = il2cpp_class_get_nested_types(kPat, &it))) {
      const char* nm = il2cpp_class_get_name(k);
      if (!strcmp(nm, "ColorEntry")) kEnt = k; else if (!strcmp(nm, "SyncEntry")) kSync = k;
    }
  }
  auto& c = g_camo;
  c.offPatterns = FieldOffset(kSlot, "patterns");
  c.offName = FieldOffset(kPat, "patternName"); c.offGlobal = FieldOffset(kPat, "globalPattern"); c.offBaseTex = FieldOffset(kPat, "baseTexture");
  c.offAddr = FieldOffset(kPat, "baseTexture2dAddressable"); c.offSprite = FieldOffset(kPat, "sprite"); c.offColors = FieldOffset(kPat, "colors");
  c.offSyncs = FieldOffset(kPat, "syncs"); c.offActivate = FieldOffset(kPat, "activateObjects");
  c.offEntName = FieldOffset(kEnt, "name"); c.offEntDefault = FieldOffset(kEnt, "defaultColor"); c.offEntChannel = FieldOffset(kEnt, "globalChannel");
  c.offEntRealtime = FieldOffset(kEnt, "realtimeColor"); c.offEntAllowEdit = FieldOffset(kEnt, "allowEdit"); c.offEntSlot = FieldOffset(kEnt, "materialColorSlotNumber");
  c.offSyncCode = FieldOffset(kSync, "syncCode"); c.offSyncSource = FieldOffset(kSync, "sourceIndex");
  c.offGuid = FieldOffset(kRef, "m_AssetGUID"); c.offElems = FieldOffset(kGroup, "camoElements"); c.offCamoGroup = FieldOffset(kMgr, "camoGroup");
  c.fInstance = kMgr ? il2cpp_class_get_field_from_name(kMgr, "instance") : nullptr;
  c.spriteCreate = kSprite ? FindMethod(kSprite, "Create", 4) : nullptr;
  c.apply = kTex ? FindMethod(kTex, "Apply", 2) : nullptr;
  void* kTexBase = FindClass("UnityEngine", "Texture");
  c.texW = kTexBase ? FindMethod(kTexBase, "get_width", 0) : nullptr; c.texH = kTexBase ? FindMethod(kTexBase, "get_height", 0) : nullptr;
  c.arrayNew = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  int offs[] = { c.offPatterns, c.offName, c.offGlobal, c.offBaseTex, c.offAddr, c.offSprite, c.offColors, c.offSyncs, c.offActivate, c.offEntName,
                 c.offEntDefault, c.offEntChannel, c.offEntRealtime, c.offEntAllowEdit, c.offEntSlot, c.offSyncCode, c.offSyncSource, c.offGuid, c.offElems };
  bool ok = c.arrayNew && m_getParent && m_getTransform && m_setParent;
  for (int o : offs) ok = ok && o >= 0;
  void* awake = kSlot ? il2cpp_class_get_method_from_name(kSlot, "Awake", 0) : nullptr;
  void* sel = kSlot ? il2cpp_class_get_method_from_name(kSlot, "selectIndex", 1) : nullptr;
  void* en = kGroup ? il2cpp_class_get_method_from_name(kGroup, "OnEnable", 0) : nullptr;
  if (!ok || !awake || !sel || !en) {
    Log("hull camo: a lookup failed (patterns +%d, ColorEntry %p, SyncEntry %p, AssetReference.m_AssetGUID +%d, camoElements +%d, Awake %p, selectIndex %p, "
        "OnEnable %p); the extra camo schemes are off", c.offPatterns, kEnt, kSync, c.offGuid, c.offElems, awake, sel, en);
    return;
  }
  auto classInit = (void (*)(void*))GetProcAddress(g_ga, "il2cpp_runtime_class_init");
  if (classInit) { classInit(kSlot); if (kMgr) classInit(kMgr); }     // instance is read in battle too, before any shipyard
  if (!PatchVtableEntry(kSlot, sel, (void*)&Hooked_PatternSelect, (void**)&g_patternSelectOrig)) {
    Log("hull camo: could not hook CustomPatternSlot.selectIndex; the extra camo schemes are off"); return;
  }
  if (!SwapMethodPointer(awake, (void*)&Hooked_PatternAwake, (void**)&g_patternAwakeOrig) ||
      !SwapMethodPointer(en, (void*)&Hooked_CamoGroupEnable, (void**)&g_camoGroupEnableOrig)) {
    Log("hull camo: could not hook CustomPatternSlot.Awake / CamoGroupUI.OnEnable; the extra camo schemes are off"); return;
  }
  if (!c.spriteCreate) Log("hull camo: Sprite.Create not found; the mod's camos have no picture in the list");
  {                                                                    // putting the mask on the hull (CamoAfterSelect)
    void* rk = FindClass("UnityEngine", "Renderer"); void* go = FindClass("UnityEngine", "GameObject");
    void* mpb = FindClass("UnityEngine", "MaterialPropertyBlock"); void* uo = FindClass("UnityEngine", "Object");
    g_cm.rendererType = rk ? il2cpp_type_get_object(il2cpp_class_get_type(rk)) : nullptr;
    g_cm.inChildren = go ? FindMethod(go, "GetComponentsInternal", 6) : nullptr;     // GetComponentsInChildren is stripped
    g_cm.getMats = rk ? FindMethod(rk, "get_sharedMaterials", 0) : nullptr;
    g_cm.getBlock = rk ? FindMethod(rk, "GetPropertyBlock", 2) : nullptr; g_cm.setBlock = rk ? FindMethod(rk, "SetPropertyBlock", 2) : nullptr;
    g_cm.setTex = mpb ? FindMethod(mpb, "SetTexture", 2, "System.String") : nullptr;
    g_cm.matName = uo ? FindMethod(uo, "get_name", 0) : nullptr;
    g_cm.offRenderers = FieldOffset(kSlot, "renderers"); g_cm.offHolder = FieldOffset(kSlot, "targetHolder");
    g_cm.offTarget = FieldOffset(kSlot, "targetMaterial"); g_cm.offSelected = FieldOffset(kSlot, "selectedIndex");
    void* ctor = mpb ? il2cpp_class_get_method_from_name(mpb, ".ctor", 0) : nullptr;
    void* block = mpb && ctor ? il2cpp_object_new(mpb) : nullptr;
    if (block) { Invoke(ctor, block); Pin(block); }
    if (g_cm.rendererType) Pin(g_cm.rendererType);
    if (block && g_cm.rendererType && g_cm.inChildren && g_cm.getMats && g_cm.getBlock && g_cm.setBlock && g_cm.setTex && g_cm.matName &&
        g_cm.offRenderers >= 0 && g_cm.offHolder >= 0 && g_cm.offTarget >= 0 && g_cm.offSelected >= 0) g_cm.block = block;
    else { Log("hull camo: a renderer lookup failed (Get/SetPropertyBlock %p/%p, SetTexture %p, GetComponentsInternal %p); the mod's camos would be a flat hull, so they are off",
               g_cm.getBlock, g_cm.setBlock, g_cm.setTex, g_cm.inChildren); return; }
  }
  {                                                                    // full-detail hull while a mod camo is on (CamoLodPin)
    void* kLodGroup = FindClass("UnityEngine", "LODGroup"); void* kLod = FindClass("UnityEngine", "LOD");
    void* kComp = FindClass("UnityEngine", "Component"); void* kBeh = FindClass("UnityEngine", "Renderer");
    auto valueSize = (uint32_t (*)(void*, uint32_t*))GetProcAddress(g_ga, "il2cpp_class_value_size");
    uint32_t align = 0;
    g_lod.lodType = kLodGroup ? il2cpp_type_get_object(il2cpp_class_get_type(kLodGroup)) : nullptr;
    g_lod.inParent = kComp ? FindMethod(kComp, "GetComponentInParent", 2, "System.Type", "System.Boolean") : nullptr;
    g_lod.getLODs = kLodGroup ? FindMethod(kLodGroup, "GetLODs", 0) : nullptr;
    g_lod.groupEnable = kLodGroup ? FindMethod(kLodGroup, "set_enabled", 1) : nullptr;
    g_lod.rendEnable = kBeh ? FindMethod(kBeh, "set_enabled", 1) : nullptr; g_lod.rendEnabled = kBeh ? FindMethod(kBeh, "get_enabled", 0) : nullptr;
    g_lod.elem = kLod && valueSize ? (int)valueSize(kLod, &align) : 0;
    int off = FieldOffset(kLod, "renderers");
    g_lod.offRend = off >= 0x10 ? off - 0x10 : -1;                      // value-type field offsets count the object header
    if (g_lod.lodType) Pin(g_lod.lodType);
    g_lod.on = g_lod.lodType && g_lod.inParent && g_lod.getLODs && g_lod.groupEnable && g_lod.rendEnable && g_lod.rendEnabled && g_lod.offRend >= 0 && g_lod.elem >= g_lod.offRend + 8;
    if (!g_lod.on) Log("hull camo: LODGroup lookups failed (GetLODs %p, set_enabled %p/%p, LOD size %d, renderers +%d); far-off hulls may show the camo scrambled",
                       g_lod.getLODs, g_lod.groupEnable, g_lod.rendEnable, g_lod.elem, g_lod.offRend);
  }
  c.ready = true;
  {                                                                    // the save format (Plain + MOD_CAMO), see above
    auto& s = g_cs;
    s.kColl = FindClass("Core.Customization", "CustomCollection"); s.kParam = FindClass("Cotanius.Modules.Data", "Parameter");
    void* kCustomSlot = FindClass("Core.Customization", "CustomSlot"); void* kComp = FindClass("UnityEngine", "Component");
    s.offPName = FieldOffset(s.kParam, "name"); s.offPValue = FieldOffset(s.kParam, "value"); s.offPType = FieldOffset(s.kParam, "type");
    s.offMain = FieldOffset(kCustomSlot, "mainObject"); s.offParams = FieldOffset(s.kColl, "parameters");
    s.getSlot = s.kColl ? FindMethod(s.kColl, "getSlot", 1) : nullptr;
    s.getCustom = s.kColl ? FindMethod(s.kColl, "getCustomParameter", 1) : nullptr;
    s.removeCustom = s.kColl ? FindMethod(s.kColl, "removeCustomParameter", 1) : nullptr;
    s.inParent = kComp ? FindMethod(kComp, "GetComponentInParent", 2, "System.Type", "System.Boolean") : nullptr;   // the 1-argument one is stripped
    s.collType = s.kColl ? il2cpp_type_get_object(il2cpp_class_get_type(s.kColl)) : nullptr;
    if (s.collType) Pin(s.collType);
    void* gmi = s.kColl ? il2cpp_class_get_method_from_name(s.kColl, "getParameters", 0) : nullptr;
    bool found = s.kParam && s.offPName >= 0 && s.offPValue >= 0 && s.offPType >= 0 && s.offMain >= 0 && s.offParams >= 0 &&
                 s.getSlot && s.getCustom && s.removeCustom && s.inParent && s.collType && gmi;
    if (found && classInit) classInit(s.kColl);
    if (found && PatchVtableEntry(s.kColl, gmi, (void*)&Hooked_CollGetParams, (void**)&g_collGetParamsOrig)) s.on = true;
    else Log("hull camo: the save hook failed (Parameter %p, getSlot %p, getCustomParameter %p, GetComponentInParent %p, getParameters %p); "
             "a mod camo is saved by index, so that save needs the mod to load", s.kParam, s.getSlot, s.getCustom, s.inParent, gmi);
  }
  DLog("hull camo: %d schemes for %d boats, hooks on CustomPatternSlot.Awake/selectIndex and CamoGroupUI.OnEnable", kCamoCount,
       (int)(sizeof(kCamoBoats) / sizeof(kCamoBoats[0])));
}

#include "wm_kill_marks.h"                                          // kill markings use this file's camo slot lookups
