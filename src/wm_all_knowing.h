// All-Knowing (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- All-Knowing -----------------------------------------------------------------------------------------------------
// David: "those manually aimed things ... when you are controlling it, the people on your boat can auto-spot things in
// range ... it's annoying when you are using them and getting close to other boats and no one is shooting because you
// didn't mark them". Read in the disassembly first (2026-10-30): the crew only fires at marked targets, i.e. locked
// CombatEntity.Target entries in the player boat's assignedTargets. FireControlMaster.evaluateTargets keeps only those
// when constrainToAssignedTargets is set, and EntitySphereOverlap.calculateClosest does the same for the fire control's
// search sphere when constrainTargets is set. Marking (EntityTargetingManager.Update's lock-on) only runs while
// PlayerCameraControlManager.controlMode is 1 (targeting), so on a hand-aimed gun nothing new gets marked.
// PlayerAuxiliaryWeaponController.Update works the aux weapon whose fireMode equals controlMode (4 for the utility slot,
// 2 for the rockets on a PT boat). While such a slot has a gun with fx.set.allKnowing selected and the player is in its
// mode, both flags are cleared on the boat's fire control, so the crew fires at any enemy the team can see, each gun
// within its own range. They are put back when the player leaves that gun, at battle end and on a fault. Checked every
// 10 frames.
static void SetItems(void* set, std::vector<void*>& out);
static bool g_akOn = false, g_akOrigFcm = false, g_akOrigSearch = false;
static void *g_akFcm = nullptr, *g_akFcmHandle = nullptr, *g_akSearch = nullptr, *g_akCraft = nullptr, *g_akMount = nullptr, *g_akEng = nullptr, *g_akLoggedEng = nullptr;
static int g_akOffConstrain = -2, g_akOffSearch = -1, g_akOffSphere = -1, g_akOffSelf = -1, g_akOffMountEntity = -1;
static int g_akFrames = 0, g_akFaults = 0, g_akTimes = 0, g_akCrewShots = 0, g_akShotsAtOn = 0;
static DWORD g_akOnAt = 0, g_akMsTotal = 0;

// Called by CritScan for every new shot fired on this machine: counts the player's other guns while All-Knowing is on.
static void AllKnowingShot(void* mount) {
  if (g_akOn && mount != g_akMount && g_akOffMountEntity >= 0 && *(void**)((char*)mount + g_akOffMountEntity) == g_akCraft) g_akCrewShots++;
}

static void AllKnowingSet(bool on, void* mount) {
  if (on == g_akOn) { if (on) g_akMount = mount; return; }             // another All-Knowing gun aimed: stays on
  if (!g_akFcm || g_akOffConstrain < 0) { g_akOn = false; return; }
  bool* sphere = g_akSearch && g_akOffSphere >= 0 ? (bool*)((char*)g_akSearch + g_akOffSphere) : nullptr;
  bool* fcm = (bool*)((char*)g_akFcm + g_akOffConstrain);
  if (on) {
    g_akOrigFcm = *fcm; g_akOrigSearch = sphere && *sphere;
    *fcm = false; if (sphere) *sphere = false;
    g_akOn = true; g_akMount = mount; g_akOnAt = GetTickCount(); g_akShotsAtOn = g_akCrewShots;
    if (++g_akTimes <= 20)
      Log("all-knowing: on while you aim '%s': the crew fires at every enemy in sight (fire control marked-only %d -> 0, its search sphere %d -> 0)",
          StripTags(NameOf(mount)).c_str(), g_akOrigFcm, g_akOrigSearch);
  } else {
    *fcm = g_akOrigFcm; if (sphere) *sphere = g_akOrigSearch;
    DWORD ms = GetTickCount() - g_akOnAt; g_akMsTotal += ms;
    g_akOn = false; g_akMount = nullptr;
    if (g_akTimes <= 20) Log("all-knowing: off after %.1f s, settings put back; the crew fired %d shots meanwhile", ms / 1000.0, g_akCrewShots - g_akShotsAtOn);
  }
}
static void SafeAllKnowingOff() {
  __try { AllKnowingSet(false, nullptr); }
  __except (EXCEPTION_EXECUTE_HANDLER) { g_akOn = false; }
}

// The player boat's FireControlMaster, from FireControlMaster.register (self == the boat), else from
// Resources.FindObjectsOfTypeAll as AutoUtilityFireControl does.
static void AllKnowingFind(void* craft) {
  void* k = FindClass("Core", "FireControlMaster");
  if (g_akOffConstrain == -2) {
    g_akOffConstrain = FieldOffset(k, "constrainToAssignedTargets"); g_akOffSearch = FieldOffset(k, "search"); g_akOffSelf = FieldOffset(k, "self");
    g_akOffSphere = FieldOffset(FindClass("Core", "EntitySphereOverlap"), "constrainTargets"); g_akOffMountEntity = FieldOffset(FindClass("Core", "GunMount"), "entity");
    Log("all-knowing: offsets FireControlMaster.constrainToAssignedTargets +0x%X search +0x%X self +0x%X, EntitySphereOverlap.constrainTargets +0x%X, GunMount.entity +0x%X",
        g_akOffConstrain, g_akOffSearch, g_akOffSelf, g_akOffSphere, g_akOffMountEntity);
  }
  Unpin(g_akFcmHandle); g_akFcmHandle = nullptr; g_akFcm = g_akSearch = nullptr;
  if (!k || g_akOffConstrain < 0 || g_akOffSelf < 0) return;
  std::vector<void*> all;
  void* fr = il2cpp_class_get_field_from_name(k, "register"); void* set = nullptr;
  if (fr) il2cpp_field_static_get_value(fr, &set);
  if (set) SetItems(set, all);
  if (all.empty() && g_resFindAll) {
    void* a[1] = { il2cpp_type_get_object(il2cpp_class_get_type(k)) };
    void* arr = Invoke(g_resFindAll, nullptr, a); int n = ArrLen(arr);
    for (int i = 0; i < n; i++) all.push_back(ArrAt(arr, i));
  }
  for (void* f : all) if (f && *(void**)((char*)f + g_akOffSelf) == craft) { g_akFcm = f; break; }
  if (!g_akFcm) return;
  g_akFcmHandle = Pin(g_akFcm);
  g_akSearch = g_akOffSearch >= 0 ? *(void**)((char*)g_akFcm + g_akOffSearch) : nullptr;
}

static void AllKnowingTick() {
  void* eng = EngagementInstance();
  void* craft = eng ? PlayerCraft() : nullptr;
  if (eng != g_akEng || craft != g_akCraft) {                          // battle over, a new battle, or another boat
    AllKnowingSet(false, nullptr);
    if (eng != g_akEng) {
      if (g_akTimes) Log("all-knowing: last battle on %d times for %.0f s in all; the crew fired %d shots while it was on", g_akTimes, g_akMsTotal / 1000.0, g_akCrewShots);
      g_akTimes = g_akCrewShots = 0; g_akMsTotal = 0;
    }
    g_akEng = eng; g_akCraft = craft;
    Unpin(g_akFcmHandle); g_akFcmHandle = nullptr; g_akFcm = g_akSearch = nullptr;
  }
  if (!eng || !craft || ++g_akFrames % 10 != 0) return;
  static void *fMode = nullptr, *ctl = nullptr, *ctlHandle = nullptr, *ctlEng = nullptr;
  static int offAux = -2, offAwSystem = -1, offAwFire = -1;
  if (offAux == -2) {
    void* cam = FindClass("Core.Player", "PlayerCameraControlManager"); fMode = cam ? il2cpp_class_get_field_from_name(cam, "controlMode") : nullptr;
    void* ck = FindClass("Core.Player", "PlayerAuxiliaryWeaponController");
    offAux = FieldOffset(ck, "weapons");
    void* iter = nullptr; void* nested; void* wk = nullptr;
    while (ck && (nested = il2cpp_class_get_nested_types(ck, &iter))) if (!strcmp(il2cpp_class_get_name(nested), "Weapon")) wk = nested;
    offAwSystem = FieldOffset(wk, "system"); offAwFire = FieldOffset(wk, "fireMode");
    if (!fMode || offAux < 0 || offAwSystem < 0 || offAwFire < 0) Log("all-knowing: MISSING PIECES (controlMode %p, aux weapons %d, system %d, fireMode %d); it stays off", fMode, offAux, offAwSystem, offAwFire);
  }
  if (!fMode || offAux < 0 || offAwSystem < 0 || offAwFire < 0) return;
  if (eng != ctlEng || !ctl) {                                         // once per battle, asked again every 60 frames until live
    if (g_akFrames % 60 != 0 && ctlEng == eng) return;
    Unpin(ctlHandle); ctlHandle = nullptr; ctlEng = eng;
    ctl = AuxController(); if (!ctl) return;
    ctlHandle = Pin(ctl);
  }
  void* aux = *(void**)((char*)ctl + offAux);                          // Weapon[]
  int nAux = ArrLen(aux);
  void* wm = g_offCraftWM >= 0 ? *(void**)((char*)craft + g_offCraftWM) : nullptr;
  void* systems = wm && g_offWMSystems >= 0 ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int nSys = ArrLen(systems);
  int mode = -1; il2cpp_field_static_get_value(fMode, &mode);
  void* want = nullptr; void* ready = nullptr; void* readySlot = nullptr;
  for (int j = 0; j < nAux && j < 16; j++) {
    void* w = ArrAt(aux, j); if (!w) continue;
    void* sys = *(void**)((char*)w + offAwSystem); if (!sys) continue;
    bool aimed = *(int*)((char*)w + offAwFire) == mode;
    if (!aimed && g_akLoggedEng == eng) continue;
    std::string sysName;
    for (int i = 0; i < nSys && i < 32; i++) {
      void* ws = ArrAt(systems, i); if (!ws) continue;
      if (ws != sys) {                                                 // the aux entry's system can be another object with the slot's name
        if (sysName.empty()) sysName = NameOf(sys);
        if (NameOf(ws) != sysName) continue;
      }
      void* mount = g_offWsSelected >= 0 ? *(void**)((char*)ws + g_offWsSelected) : nullptr;
      const Item* it = mount ? ItemOfMount(mount) : nullptr;
      if (!it || ItemFx(*it, "allKnowing") <= 0) continue;
      if (aimed) want = mount;
      if (!ready) { ready = mount; readySlot = ws; }
    }
  }
  if (ready && g_akLoggedEng != eng) {                                 // once per battle: what the boat's fire control does
    g_akLoggedEng = eng;
    if (!g_akFcm) AllKnowingFind(craft);
    Log("all-knowing: '%s' is in slot '%s'; the boat's fire control %s", StripTags(NameOf(ready)).c_str(), NameOf(readySlot).c_str(),
        !g_akFcm ? "was NOT found, so it cannot work" : (*(bool*)((char*)g_akFcm + g_akOffConstrain) ? "fires only at marked targets (as expected)" :
        "does NOT limit itself to marked targets, so clearing it changes nothing"));
  }
  if (want && !g_akFcm && g_akFrames % 60 == 0) AllKnowingFind(craft);
  // why it does or does not turn on (a player on a mortar thought it never did, 2026-11-16): the first 6 camera mode
  // changes per battle while an All-Knowing gun is fitted, with the fire mode of its aux entry
  static int lastMode = -99, modeLogs = 0; static void* modeEng = nullptr;
  if (modeEng != eng) { modeEng = eng; modeLogs = 0; lastMode = -99; }
  if (ready && mode != lastMode && modeLogs < 6) {
    int slotMode = -1;
    for (int j = 0; j < nAux && j < 16; j++) { void* w = ArrAt(aux, j); void* sys = w ? *(void**)((char*)w + offAwSystem) : nullptr;
      if (sys && (sys == readySlot || NameOf(sys) == NameOf(readySlot))) slotMode = *(int*)((char*)w + offAwFire); }
    modeLogs++; lastMode = mode;
    Log("all-knowing: camera mode %d (1 targeting, 2 rocket, 4 special, 6 magnified, 7 squadron); '%s' is aimed in mode %d, so it is %s", mode,
        StripTags(NameOf(ready)).c_str(), slotMode, want ? "ON" : "off");
  }
  AllKnowingSet(want && g_akFcm, want);
}

