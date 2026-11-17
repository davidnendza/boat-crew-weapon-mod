// loadout screen: selection diagnostics, seat icons, WeaponSystem.selectIndex (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- diagnostics: what the loadout screen has selected (on open, on every change, at confirm) -----------------------
static int g_offSelCraft = -1, g_offCraftWM = -1, g_offWMSystems = -1, g_offWsSelected = -1, g_offWsSelIndex = -1;
static void* g_lastSel[32];
static int g_diagFrames = 0;

static void DumpSelection(void* manager, const char* why, bool onlyChanges) {
  void* craft = *(void**)((char*)manager + g_offSelCraft);
  if (!craft) return;
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int n = ArrLen(systems);
  if (n > 32) n = 32;
  for (int i = 0; i < n; i++) {
    void* ws = ArrAt(systems, i);
    if (!ws) continue;
    void* sel = *(void**)((char*)ws + g_offWsSelected);
    if (onlyChanges && sel == g_lastSel[i]) continue;
    g_lastSel[i] = sel;
    void* order = *(void**)((char*)ws + g_offOrder);
    void* mounts = *(void**)((char*)ws + g_offMounts);
    void* dn = g_offDisplayName >= 0 ? *(void**)((char*)ws + g_offDisplayName) : nullptr;
    DLog("SELECT[%s] system %d ws=%p '%s' family '%s' mounts=%d selectedIndex=%d selected='%s'", why, i, ws, Str(dn), order ? FamilyOf(order).c_str() : "-",
        ListCount(mounts), *(int*)((char*)ws + g_offWsSelIndex), sel ? NameOf(sel).c_str() : "(none)");
  }
}

// A worn-out or scrapped gun is hidden from the list but can still be the saved choice of a slot, which the game shows as
// 'Report Me To Dev :('. When the loadout screen opens (after the saved loadout has been applied), such slots are emptied.
static void* g_selectIndexMI = nullptr;
static bool g_refreshGunSlots = false;                                 // JobRefreshGuns rebuilds the screen's gun slots next frame
static bool IsTombstone(const std::string& name) {
  for (auto& p : g_positions) if (std::find(p.second.begin(), p.second.end(), name) != p.second.end()) return true;
  return false;
}
static void UnequipRetired(void* manager) {
  if (!g_selectIndexMI) return;
  void* craft = *(void**)((char*)manager + g_offSelCraft);
  if (!craft) return;
  TorpUnequipRetired(craft);
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int n = ArrLen(systems);
  for (int i = 0; i < n && i < 32; i++) {
    void* ws = ArrAt(systems, i);
    void* sel = ws ? *(void**)((char*)ws + g_offWsSelected) : nullptr;
    if (!sel) continue;
    std::string nm = StripTags(NameOf(sel));
    const Item* it = FindItem(nm);
    bool gone = !it && IsTombstone(nm);                                // deleted at an earlier launch; its placeholder holds the slot
    if (!gone && (!it || !it->retired)) continue;
    int zero = 0; void* a[1] = { &zero };
    Invoke(g_selectIndexMI, ws, a);
    // so the slot's list stops highlighting the gun; at most every 2 s, in case the refill ever re-selects it
    static DWORD lastRefresh = 0;
    if (GetTickCount() - lastRefresh > 2000) { g_refreshGunSlots = true; lastRefresh = GetTickCount(); }
    Log("emptied slot %p: '%s' is worn out or scrapped", ws, nm.c_str());
  }
}

// ---- seat icons: a crewless gun (requiresCrew off, e.g. Automated) should not show person icons ---------------------
// ItemElementUI.setSeats (list rows) and SlotElementUI.setSeats (slot panel) turn on one icon per entry of GunMount.seats.
// The seats themselves must stay (11 GunMount methods read them), so only the icon rows are hidden, every frame while the
// loadout screen is open. A row the mod hid is shown again when it is reused for a gun that needs crew.
static void *g_resFindAll = nullptr, *g_objFindBy = nullptr, *g_typeItemEl = nullptr, *g_typeSlotEl = nullptr, *g_clsGunMount = nullptr, *g_clsWeaponSystem = nullptr, *m_activeSelf = nullptr;
static int g_offIeHolder = -1, g_offIeDet = -1, g_offSeHolder = -1, g_offSeEmpty = -1, g_offSeMissing = -1, g_offSeObj = -1, g_offReqCrew = -1;
static bool g_seatIconsReady = false, g_seatIconsTried = false;
struct HiddenHolder { void* holder; void* handle; bool wasActive; };   // a UI row the mod hid, held by a GC handle
static std::vector<HiddenHolder> g_hiddenHolders;

static bool InitSeatIcons() {
  if (g_seatIconsTried) return g_seatIconsReady;
  g_seatIconsTried = true;
  void* res = FindClass("UnityEngine", "Resources");
  g_resFindAll = res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
  // Object.FindObjectsByType(type, Exclude inactive, no sorting) looks only at live scene objects; FindObjectsOfTypeAll
  // also walks assets and prefabs (with about 920k objects loaded, two such scans cost 26-32 ms)
  { void* o = FindClass("UnityEngine", "Object"); g_objFindBy = o ? FindMethod(o, "FindObjectsByType", 3, "System.Type") : nullptr; }
  void* ie = FindClass("Core.Customization", "ItemElementUI");
  void* se = FindClass("Core.Customization", "SlotElementUI");
  g_clsGunMount = FindClass("Core", "GunMount"); g_clsWeaponSystem = FindClass("Core", "WeaponSystem");
  { void* go = FindClass("UnityEngine", "GameObject"); m_activeSelf = go ? FindMethod(go, "get_activeSelf", 0) : nullptr; }
  if (!g_resFindAll || !ie || !se || !g_clsGunMount || !g_clsWeaponSystem || !m_activeSelf) { Log("seat icons: a lookup failed (FindObjectsOfTypeAll %p ItemElementUI %p SlotElementUI %p)", g_resFindAll, ie, se); return false; }
  g_offIeHolder = FieldOffset(ie, "seatsHolder"); g_offIeDet = FieldOffset(ie, "detailable");
  g_offSeHolder = FieldOffset(se, "seatsHolder"); g_offSeEmpty = FieldOffset(se, "seatsHolderEmpties"); g_offSeMissing = FieldOffset(se, "seatsHolderMissing"); g_offSeObj = FieldOffset(se, "selectableObject");
  g_offReqCrew = FieldOffset(g_clsGunMount, "requiresCrew");
  if (g_offIeHolder < 0 || g_offIeDet < 0 || g_offSeHolder < 0 || g_offSeObj < 0 || g_offReqCrew < 0) { Log("seat icons: a field lookup failed"); return false; }
  g_typeItemEl = il2cpp_type_get_object(il2cpp_class_get_type(ie));
  g_typeSlotEl = il2cpp_type_get_object(il2cpp_class_get_type(se));
  g_seatIconsReady = true;
  DLog("seat icons: ready (row scan by %s)", g_objFindBy ? "Object.FindObjectsByType" : "Resources.FindObjectsOfTypeAll");
  return true;
}

static void SetHolder(void* holder, bool show) {
  if (!holder) return;
  auto it = std::find_if(g_hiddenHolders.begin(), g_hiddenHolders.end(), [&](const HiddenHolder& h) { return h.holder == holder; });
  if (show && it == g_hiddenHolders.end()) return;                  // the game manages rows the mod never hid
  void* go = Invoke(m_getGameObject, holder);
  if (!go) return;
  if (!show) {
    void* r = Invoke(m_activeSelf, go);
    bool active = r && *(bool*)((char*)r + 0x10);
    if (it == g_hiddenHolders.end()) g_hiddenHolders.push_back({ holder, Pin(holder), active });
    if (active) { bool off = false; void* a[1] = { &off }; Invoke(m_setActive, go, a); }
  } else {
    bool was = it->wasActive; void* a[1] = { &was }; Invoke(m_setActive, go, a);   // back to what it was before the mod hid it
    Unpin(it->handle);
    g_hiddenHolders.erase(it);
  }
}
static void RestoreHolders() {                                      // loadout screen closing: give back every hidden row
  for (HiddenHolder& h : g_hiddenHolders) {
    void* go = Invoke(m_getGameObject, h.holder);
    if (go) { bool was = h.wasActive; void* a[1] = { &was }; Invoke(m_setActive, go, a); }
    Unpin(h.handle);
  }
  g_hiddenHolders.clear();
}

static int CrewlessMount(void* obj) {                                 // 1 crewless gun, 0 gun that needs crew, -1 not a gun
  if (!obj) return -1;
  void* k = il2cpp_object_get_class(obj);
  if (k == g_clsWeaponSystem) { obj = *(void**)((char*)obj + g_offWsSelected); if (!obj) return -1; k = il2cpp_object_get_class(obj); }
  if (k != g_clsGunMount) return -1;
  return *(bool*)((char*)obj + g_offReqCrew) ? 0 : 1;
}

// Gun-list rows (ItemElementUI) are collected as they wake (Hooked_ItemElemAwake), held by GC handles and dropped once
// Unity destroys them; slot rows are read from the screen's own CraftCustomizationManager.slotGroup.slots list. One
// include-inactive scan, the first time the screen opens, catches rows that woke before the mod's hooks (a periodic
// scan costs 11-18 ms even with FindObjectsByType). The lists are refreshed every 120 frames and the per-frame pass reuses them.
static std::vector<void*> g_itemEls, g_slotEls;
static int g_seatScanFrame = -100;
static double g_perfScanMax = 0; static int g_perfScans = 0;           // row list refreshes, for the loadout perf line
static void* g_scanHandles[2] = { nullptr, nullptr };
static std::unordered_map<void*, void*> g_itemRows;                    // ItemElementUI -> GC handle
static bool g_itemRowsScanned = false;
static int g_offCachedPtr = -2, g_offCcmSlotGroup = -2, g_offGroupSlots = -2;
static bool RowAlive(void* obj) { return obj && g_offCachedPtr >= 0 && *(void**)((char*)obj + g_offCachedPtr) != nullptr; }   // Object.m_CachedPtr: 0 once destroyed
static void RegisterItemRow(void* e) { if (e && !g_itemRows.count(e)) g_itemRows[e] = Pin(e); }
static void ReleaseScans() {
  for (void*& h : g_scanHandles) { if (h && il2cpp_gchandle_free) il2cpp_gchandle_free(h); h = nullptr; }
}
// slot 0 or 1: the GC handle that keeps this scan's array, and with it every element, from being collected while the
// list is in use. includeInactive: pooled rows that are switched off right now too (scene objects only).
static void ScanElements(void* type, std::vector<void*>& out, int slot, bool includeInactive = false) {
  out.clear();
  if (g_scanHandles[slot] && il2cpp_gchandle_free) { il2cpp_gchandle_free(g_scanHandles[slot]); g_scanHandles[slot] = nullptr; }
  int inactive = includeInactive ? 1 : 0, sort = 0;                    // FindObjectsInactive.Exclude/Include, FindObjectsSortMode.None
  void* a3[3] = { type, &inactive, &sort }; void* a1[1] = { type };
  void* arr = g_objFindBy ? Invoke(g_objFindBy, nullptr, a3) : Invoke(g_resFindAll, nullptr, a1);
  if (arr && il2cpp_gchandle_new) g_scanHandles[slot] = il2cpp_gchandle_new(arr, false);
  int n = ArrLen(arr);
  for (int i = 0; i < n; i++) { void* e = ArrAt(arr, i); if (e) out.push_back(e); }
}
static void RefreshRowLists(void* ccm) {
  if (g_offCachedPtr == -2) {
    g_offCachedPtr = FieldOffset("UnityEngine", "Object", "m_CachedPtr");
    g_offCcmSlotGroup = FieldOffset("Core.Customization", "CraftCustomizationManager", "slotGroup");
    g_offGroupSlots = FieldOffset("Core.Customization", "SlotGroupUI", "slots");
    DLog("seat icons: rows from Awake (m_CachedPtr +0x%X), slots from CraftCustomizationManager.slotGroup +0x%X .slots +0x%X", g_offCachedPtr, g_offCcmSlotGroup, g_offGroupSlots);
  }
  if (g_offCachedPtr < 0) { ScanElements(g_typeItemEl, g_itemEls, 0); ScanElements(g_typeSlotEl, g_slotEls, 1); return; }   // old way
  if (!g_itemRowsScanned) {                                            // once: rows that woke before the hook
    g_itemRowsScanned = true;
    std::vector<void*> v; ScanElements(g_typeItemEl, v, 0, true);
    for (void* e : v) RegisterItemRow(e);
    ReleaseScans();
  }
  g_itemEls.clear();
  for (auto it = g_itemRows.begin(); it != g_itemRows.end(); ) {
    if (RowAlive(it->first)) { g_itemEls.push_back(it->first); ++it; } else { Unpin(it->second); it = g_itemRows.erase(it); }
  }
  void* grp = ccm && g_offCcmSlotGroup >= 0 ? *(void**)((char*)ccm + g_offCcmSlotGroup) : nullptr;
  void* slots = grp && g_offGroupSlots >= 0 ? *(void**)((char*)grp + g_offGroupSlots) : nullptr;
  if (!slots) { ScanElements(g_typeSlotEl, g_slotEls, 1); return; }   // no list: fall back to the scan
  g_slotEls.clear();
  for (int i = 0; i < ListCount(slots); i++) { void* e = ListAt(slots, i); if (RowAlive(e)) g_slotEls.push_back(e); }
}
static void HideCrewlessSeatIcons(void* ccm) {
  if (!InitSeatIcons()) return;
  if (g_diagFrames < g_seatScanFrame || g_diagFrames - g_seatScanFrame >= 120) {
    LARGE_INTEGER a, b, f; QueryPerformanceCounter(&a);
    g_seatScanFrame = g_diagFrames;
    RefreshRowLists(ccm);
    QueryPerformanceCounter(&b); QueryPerformanceFrequency(&f);
    double ms = (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart; g_perfScans++; if (ms > g_perfScanMax) g_perfScanMax = ms;
  }
  for (void* e : g_itemEls) {
    int c = CrewlessMount(*(void**)((char*)e + g_offIeDet));
    if (c >= 0) SetHolder(*(void**)((char*)e + g_offIeHolder), c == 0);
  }
  for (void* e : g_slotEls) {
    int c = CrewlessMount(*(void**)((char*)e + g_offSeObj));
    if (c < 0) continue;
    SetHolder(*(void**)((char*)e + g_offSeHolder), c == 0);
    if (g_offSeEmpty >= 0) SetHolder(*(void**)((char*)e + g_offSeEmpty), c == 0);
    if (g_offSeMissing >= 0) SetHolder(*(void**)((char*)e + g_offSeMissing), c == 0);
  }
}

static LARGE_INTEGER g_perfFreq;
static double g_perfSeatMax = 0;
static double g_perfSeatMs = 0, g_perfGuiMs = 0; static int g_perfFrames = 0, g_perfGuiCalls = 0; static DWORD g_perfAt = 0;
static double PerfMs(LARGE_INTEGER a) { LARGE_INTEGER b; QueryPerformanceCounter(&b); if (!g_perfFreq.QuadPart) QueryPerformanceFrequency(&g_perfFreq); return (b.QuadPart - a.QuadPart) * 1000.0 / g_perfFreq.QuadPart; }
static void PerfReport() {
  DWORD now = GetTickCount();
  if (!g_perfAt) { g_perfAt = now; return; }
  if (now - g_perfAt < 10000) return;
  double secs = (now - g_perfAt) / 1000.0;
  if (g_perfFrames) DLog("perf (loadout screen, %.0f s): %.1f frames/s; mod per frame: seat icons + selection %.3f ms (worst %.1f ms; %d row list refreshes, worst %.1f ms), inventory window %.3f ms (%d OnGUI calls/s)",
      secs, g_perfFrames / secs, g_perfSeatMs / g_perfFrames, g_perfSeatMax, g_perfScans, g_perfScanMax, g_perfGuiMs / g_perfFrames, (int)(g_perfGuiCalls / secs));
  g_perfAt = now; g_perfSeatMs = g_perfGuiMs = g_perfSeatMax = g_perfScanMax = 0; g_perfFrames = g_perfGuiCalls = g_perfScans = 0;
}

static void (*g_ccmLateUpdateOrig)(void*, void*) = nullptr;
static void (*g_ccmOnDisableOrig)(void*, void*) = nullptr;
static void (*g_ccmReturnOrig)(void*, bool, void*) = nullptr;
static void* g_ccm = nullptr;                                         // the open loadout screen (CraftCustomizationManager)
static unsigned g_jobsOff = 0;                                         // loadout-screen jobs switched off after a fault
static int FaultFilter(const char* job, EXCEPTION_POINTERS* ep) {
  void* at = ep->ExceptionRecord->ExceptionAddress; HMODULE mod = nullptr; char name[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)at, &mod)) GetModuleFileNameA(mod, name, MAX_PATH);
  const char* base = strrchr(name, '\\');
  Log("fault 0x%08lX in '%s' at %s+0x%llX; that part of the mod is off until restart", ep->ExceptionRecord->ExceptionCode, job,
      base ? base + 1 : name, (unsigned long long)((char*)at - (char*)mod));
  return EXCEPTION_EXECUTE_HANDLER;
}
static void JobSelection(void* self) {
  if (!g_debug) return;
  if (g_diagFrames == 1 || g_diagFrames == 5 || g_diagFrames == 120) { char w[32]; snprintf(w, sizeof w, "open frame %d", g_diagFrames); DumpSelection(self, w, false); if (g_diagFrames == 1) LogRegisterSummary("screen open"); }
  else DumpSelection(self, "change", true);
}
// every 30 frames, not only at 10 and 60: a gun scrapped while the screen is open is still in the slot's list until the
// boat is rebuilt, and picking it again would take it into battle uncounted
static void JobUnequip(void* self) { if (g_diagFrames % 30 == 10) UnequipRetired(self); }
static void JobSeats(void* self) { HideCrewlessSeatIcons(self); }
// A gun kept from the loot grid is injected into the boats at once, but the open loadout screen can keep showing its
// old list: the first found gun of a slot family gives that slot a new SelectionOrder copy. One frame after a keep, the screen's own
// CraftCustomizationManager.fillCraftSlotsGuns() rebuilds the gun slots, as opening the screen does.
static void JobRefreshGuns(void* self) {
  if (!g_refreshGunSlots) return;
  g_refreshGunSlots = false;
  static void* m = nullptr;
  if (!m) { void* k = FindClass("Core.Customization", "CraftCustomizationManager"); m = k ? il2cpp_class_get_method_from_name(k, "fillCraftSlotsGuns", 0) : nullptr; }
  if (m) { Invoke(m, self); DLog("loadout screen: gun slots refilled (a gun was kept, scrapped or taken off)"); }
}
static void CheckNewCampaign(bool rebuild);
static void TryInstallPerSave(const char* from);
static void JobCampaign(void*) { TryInstallPerSave("the loadout screen"); CheckNewCampaign(true); }   // per-save inventories: a new campaign's first loadout
static void RunJob(unsigned bit, const char* name, void (*fn)(void*), void* self) {
  if (g_jobsOff & bit) return;
  __try { fn(self); } __except (FaultFilter(name, GetExceptionInformation())) { g_jobsOff |= bit; }
}
static void Hooked_CcmLateUpdate(void* self, void* method) {
  g_ccmLateUpdateOrig(self, method);
  if (!g_ready) return;
  g_ccm = self;
  LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
  g_diagFrames++;
  RunJob(1, "selection log", JobSelection, self);
  RunJob(2, "unequip worn-out", JobUnequip, self);
  RunJob(4, "seat icons", JobSeats, self);
  RunJob(8, "refill gun slots", JobRefreshGuns, self);
  RunJob(16, "new campaign check", JobCampaign, self);
  { double ms = PerfMs(t0); g_perfSeatMs += ms; if (ms > g_perfSeatMax) g_perfSeatMax = ms; } g_perfFrames++;
  PerfReport();
}
static void Hooked_CcmOnDisable(void* self, void* method) {
  g_ccmOnDisableOrig(self, method);
  g_ccm = nullptr;
  g_diagFrames = 0; memset(g_lastSel, 0, sizeof g_lastSel); g_itemEls.clear(); g_slotEls.clear(); g_seatScanFrame = -100;
  __try { ReleaseScans(); RestoreHolders(); } __except (FaultFilter("release seat lists", GetExceptionInformation())) { g_scanHandles[0] = g_scanHandles[1] = nullptr; g_hiddenHolders.clear(); }
  DLog("SELECT: loadout screen closed");
}
static void Hooked_CcmReturn(void* self, bool confirm, void* method) {
  if (g_ready) { __try { DumpSelection(self, confirm ? "CONFIRM pressed" : "CANCEL pressed", false); LogRegisterSummary("confirm"); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
  g_ccmReturnOrig(self, confirm, method);
}

static void* PatchPointer(void* klass, const char* name, int nparams, void* hook, void** original) {
  void* mi = il2cpp_class_get_method_from_name(klass, name, nparams);
  if (!mi) { Log("diagnostics: %s not found", name); return nullptr; }
  void** slot = (void**)mi; *original = *slot;
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) { Log("diagnostics: VirtualProtect failed for %s", name); return nullptr; }
  *slot = hook; VirtualProtect(slot, sizeof(void*), old, &old);
  return mi;
}

// ---- WeaponSystem.selectIndex: where a saved position is applied -------------------------------------------------------
// CustomCollection.selectByIndex calls selectIndex through the vtable. WeaponSystem.select silently ignores a position
// that is not below the system's gun count, so a saved loadout is dropped when it is applied to a boat that lacks the
// gun. This hook logs each call and, when the position is out of range, adds the missing guns first.
typedef void (*fn_selectIndex)(void*, int, void*);
static fn_selectIndex g_selectIndexOrig = nullptr;
static int g_selectLogs = 0, g_selFaults = 0;

static void BeforeSelectIndex(void* ws, int index) {
  void* mounts = *(void**)((char*)ws + g_offMounts);
  int cnt = ListCount(mounts);
  void* order = *(void**)((char*)ws + g_offOrder);
  if (g_selectLogs < 400) {
    g_selectLogs++;
    DLog("selectIndex ws=%p family '%s' index=%d mounts=%d selectedIndex=%d prevent=%d", ws, order ? FamilyOf(order).c_str() : "-", index, cnt,
        *(int*)((char*)ws + g_offWsSelIndex), (int)*(bool*)((char*)ws + 0xB4));   // 0xB4: WeaponSystem.forcePreventSelectionChange
  }
  if (index > 0 && index >= cnt) {
    DLog("  position %d is beyond this system's %d guns; adding the missing guns first", index, cnt);
    SafeInject(ws);
    DLog("  this system now has %d guns", ListCount(*(void**)((char*)ws + g_offMounts)));
  }
}

static void Hooked_SelectIndex(void* self, int index, void* method) {
  if (g_ready) {
    __try { BeforeSelectIndex(self, index); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++g_selFaults <= 3) Log("fault in the selectIndex hook (%d)", g_selFaults); }
  }
  g_selectIndexOrig(self, index, method);
}

// The game builds the player's boat while a save loads, a moment before the mod switches to that save's inventory, so the
// boat gets the last save's guns: this save's ones become hidden placeholders, a copy of the slot's first stock gun under
// the gun's name, picked by position (a found gun would be a stock gun all session). Inject swaps such a placeholder
// for the real gun at the same position, and selects it again if the slot had picked it.
// In co-op only on this player's own boat: another player's positions are EnsureRemoteGun's, which also keeps the shot
// sync's table pointing at the gun there, and a swap here would leave that table on the old copy.
static bool OnPlayerBoat(void* mount);
static bool RealizePlaceholder(void* ws, void* mounts, int j, const std::string& name, const std::string& family,
                               const std::unordered_map<std::string, void*>& byBase, void* order, void*& orderClone) {
  const Item* item = FindItemIn(name, family);
  if (!item || item->held || item->retired) return false;
  void* cur = ListAt(mounts, j);
  void* gt = cur ? *(void**)((char*)cur + g_offGunType) : nullptr;
  if (!gt || g_offOverrideTag < 0 || strncmp(Str(*(void**)((char*)gt + g_offOverrideTag)), kHiddenTag, strlen(kHiddenTag)) != 0) return false;
  if (IsRemoteMount(cur) || (g_coopRole && !OnPlayerBoat(cur))) return false;
  auto b = byBase.find(item->base);
  if (b == byBase.end()) return false;                                 // its base gun is not in this slot: it stays a placeholder
  void* m2 = AddCloneMount(ws, mounts, b->second, *item);
  if (!m2) return false;
  int last = ListCount(mounts) - 1;                                    // added at the end: moved to position j
  if (!ListSetAt(mounts, j, m2) || !ListRemoveAt(mounts, last)) {
    if (ListCount(mounts) == last + 1 && ListAt(mounts, last) == m2) ListRemoveAt(mounts, last);
    Log("'%s': could not put the real gun at position %d of slot '%s'; the placeholder stays", name.c_str(), j, family.c_str());
    return false;
  }
  void* det = g_offDetailables >= 0 ? *(void**)((char*)ws + g_offDetailables) : nullptr;
  for (int i = ListCount(det) - 1; i >= 0; i--) if (ListAt(det, i) == cur) { ListRemoveAt(det, i); break; }
  bool picked = g_offWsSelected >= 0 && *(void**)((char*)ws + g_offWsSelected) == cur;
  { void* go = Invoke(m_getGameObject, cur); bool off = false; void* a[1] = { &off }; if (go) Invoke(m_setActive, go, a); }   // out of the list, select() no longer hides it
  if (!orderClone) { orderClone = CloneObject(order); if (orderClone) SetRef(ws, (char*)ws + g_offOrder, orderClone); }
  if (orderClone) AddOrderEntry(orderClone, name.c_str(), j);
  if (picked && g_selectIndexMI) { int idx = j; void* a[1] = { &idx }; Invoke(g_selectIndexMI, ws, a); }
  Log("'%s': position %d of slot '%s' held a placeholder from another save's guns; it is the real gun now%s", name.c_str(), j, family.c_str(), picked ? " (equipped, selected again)" : "");
  return true;
}

// The vtable entry is {methodPtr, MethodInfo*}; find it by looking for that pair in the class memory.
static bool PatchVtableEntry(void* klass, void* methodInfo, void* hook, void** original) {
  void* origPtr = *(void**)methodInfo;
  __try {
    uint8_t* base = (uint8_t*)klass;
    for (size_t off = 0x40; off < 0x2000; off += 8) {
      void** e = (void**)(base + off);
      if (e[0] == origPtr && e[1] == methodInfo) {
        DWORD old = 0;
        if (!VirtualProtect(e, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *original = origPtr; e[0] = hook;
        VirtualProtect(e, sizeof(void*), old, &old);
        DLog("selectIndex: patched vtable entry at class+0x%zX", off);
        return true;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
  return false;
}

static void InstallDiagnostics() {
  void* ccm = FindClass("Core.Customization", "CraftCustomizationManager");
  void* ce = FindClass("Core", "CombatEntity"); void* wmc = FindClass("Core", "WeaponManager"); void* wsc = FindClass("Core", "WeaponSystem");
  void* fSC = ccm ? il2cpp_class_get_field_from_name(ccm, "selectedCraft") : nullptr;
  void* fWM = ce ? il2cpp_class_get_field_from_name(ce, "weaponManager") : nullptr;
  void* fSys = wmc ? il2cpp_class_get_field_from_name(wmc, "systems") : nullptr;
  void* fSel = wsc ? il2cpp_class_get_field_from_name(wsc, "selected") : nullptr;
  void* fSelI = wsc ? il2cpp_class_get_field_from_name(wsc, "selectedIndex") : nullptr;
  if (!ccm || !fSC || !fWM || !fSys || !fSel || !fSelI) { Log("diagnostics: a lookup failed; selection logging is off"); return; }
  g_offSelCraft = (int)il2cpp_field_get_offset(fSC); g_offCraftWM = (int)il2cpp_field_get_offset(fWM); g_offWMSystems = (int)il2cpp_field_get_offset(fSys);
  g_offWsSelected = (int)il2cpp_field_get_offset(fSel); g_offWsSelIndex = (int)il2cpp_field_get_offset(fSelI);
  bool a = PatchPointer(ccm, "LateUpdate", 0, (void*)&Hooked_CcmLateUpdate, (void**)&g_ccmLateUpdateOrig) != nullptr;
  bool b = PatchPointer(ccm, "OnDisable", 0, (void*)&Hooked_CcmOnDisable, (void**)&g_ccmOnDisableOrig) != nullptr;
  bool c = PatchPointer(ccm, "returnToGame", 1, (void*)&Hooked_CcmReturn, (void**)&g_ccmReturnOrig) != nullptr;
  DLog("diagnostics: loadout screen hooks LateUpdate=%d OnDisable=%d returnToGame=%d", (int)a, (int)b, (int)c);
  void* selMi = il2cpp_class_get_method_from_name(wsc, "selectIndex", 1);
  g_selectIndexMI = selMi;
  if (!selMi || !PatchVtableEntry(wsc, selMi, (void*)&Hooked_SelectIndex, (void**)&g_selectIndexOrig)) Log("could not hook WeaponSystem.selectIndex (saved positions will not be repaired)");
}

