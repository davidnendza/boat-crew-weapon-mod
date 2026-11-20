// Hooked_PMLateUpdate (the per-frame dispatcher) and ItemElementUI.Awake (a fragment of weaponmod.cpp, included in order, not standalone)
static void (*g_pmLateUpdateOriginal)(void*, void*) = nullptr;
static int g_pmFaults = 0;
static double g_bPerfMs = 0, g_bPerfMax = 0; static int g_bPerfFrames = 0; static DWORD g_bPerfAt = 0;
static void Hooked_PMLateUpdate(void* self, void* method) {
  StallBeat();
  LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
  if (g_ready && PaletteOn()) {
    __try { ExtendPalette(self); }
    __except (FaultFilter("rarity tracers", GetExceptionInformation())) { g_tracerRarity = false; SafePaletteFailed(); }
  }
  if (g_ready && g_tracerSet) {
    __try { ApplyTracerColor(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (++g_pmFaults <= 3) Log("fault while recoloring the tracer material (%d)", g_pmFaults); g_tracerSet = false; }
  }
  g_pmLateUpdateOriginal(self, method);
  if (g_ready && g_usesBase > 0 && g_missionFaults >= 0) {           // a fault stops the counting only: drops keep their uses
    __try { MissionWatch(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("fault in the mission watcher; missions are not counted for the rest of this session"); g_missionFaults = -1; }
  }
  if (g_ready && g_chainFaults >= 0) {
    __try { ChainScan(self); }
    __except (FaultFilter("chaining shots", GetExceptionInformation())) { g_chainFaults = -1; }
  }
  if (g_ready && g_elitesOn && g_eliteFaults >= 0) {
    __try { EliteScan(); }
    __except (FaultFilter("rare/elite enemies", GetExceptionInformation())) { g_eliteFaults = -1; SafeRestoreAllShips(); }
  }
  if (g_ready && g_unuFaults >= 0) {
    __try { UnusualTick(); }
    __except (FaultFilter("extra effects (spawns)", GetExceptionInformation())) { g_unuFaults = -1; SafeRestoreAllHelperLooks(); SafeBalloonRestoreAll(); }
  }
  if (g_ready && g_autoUtility && g_autoFaults >= 0) {
    __try { AutoUtilityTick(); }
    __except (FaultFilter("auto utility", GetExceptionInformation())) { g_autoFaults = -1; }
  }
  if (g_ready && g_akFaults >= 0) {
    __try { AllKnowingTick(); }
    __except (FaultFilter("all-knowing", GetExceptionInformation())) { g_akFaults = -1; SafeAllKnowingOff(); }
  }
  if (g_ready && g_tp.ok && g_torpBattleFaults >= 0) {
    __try { TorpBattleTick(); }
    __except (FaultFilter("found torpedoes (battle)", GetExceptionInformation())) { g_torpBattleFaults = -1; }
  }
  if (g_ready && g_utilFaults >= 0) {
    __try { UtilTick(); }
    __except (FaultFilter("utility gear (rafts)", GetExceptionInformation())) { g_utilFaults = -1; }
  }
  if (g_ready && g_tp.ok && g_torpLookFaults >= 0) {
    __try { TorpLookTick(); }
    __except (FaultFilter("torpedo looks (chomper, commando), chomp sound, dolphin leaps", GetExceptionInformation())) { g_torpLookFaults = -1; SafeTorpLooksOff(); }
  }
  if (g_ready && g_tp.ok && g_tubeFaults >= 0) {
    __try { TubeTick(); }
    __except (FaultFilter("torpedo looks in the tubes", GetExceptionInformation())) { g_tubeFaults = -1; SafeTubeLooksOff(); }
  }
  if (g_ready && g_crewOn && g_crewBattleFaults >= 0) {
    __try { CrewBattleTick(); }
    __except (FaultFilter("crew weapon looks (battle)", GetExceptionInformation())) { g_crewBattleFaults = -1; g_crewFaults = -1; SafeCrewHideAll(); }
  }
  if (g_ready && g_streakFaults >= 0) {                                // after ChainScan: this frame's leaps and kills are queued
    __try { StreakTick(); }
    __except (FaultFilter("effect streaks", GetExceptionInformation())) { g_streakFaults = -1; SafeStreaksOff(); }
  }
  if (g_ready && g_nrFaults >= 0) SafeNewsreelTick();
  if (g_ready) KillMarksFrame();
  if (g_ready && EngagementInstance()) {                                // battle only; the time includes the game's own LateUpdate
    double ms = PerfMs(t0); g_bPerfMs += ms; if (ms > g_bPerfMax) g_bPerfMax = ms; g_bPerfFrames++;
    DWORD now = GetTickCount();
    if (!g_bPerfAt) g_bPerfAt = now;
    else if (now - g_bPerfAt >= 10000) {
      double secs = (now - g_bPerfAt) / 1000.0;
      DLog("perf (battle, %.0f s): %.1f frames/s; ProjectileManager.LateUpdate incl. mod %.3f ms per frame (worst %.1f ms)", secs, g_bPerfFrames / secs, g_bPerfMs / g_bPerfFrames, g_bPerfMax);
      g_bPerfAt = now; g_bPerfMs = g_bPerfMax = 0; g_bPerfFrames = 0;
    }
  } else g_bPerfAt = 0;
}

// ---- hook: ItemElementUI.Awake (an entry in the loadout lists) ------------------------------------------------
// The name is a TextMeshPro text that wraps; long rolled names shrink to fit one line instead.
static void (*g_itemElemAwakeOrig)(void*, void*) = nullptr;
static int g_offElemName = -1;
static void *m_tmpGetSize = nullptr, *m_tmpAuto = nullptr, *m_tmpMin = nullptr, *m_tmpMax = nullptr, *m_tmpWrap = nullptr;
static void FitItemName(void* self) {
  void* txt = *(void**)((char*)self + g_offElemName); if (!txt) return;
  void* r = Invoke(m_tmpGetSize, txt); float fs = r ? *(float*)((char*)r + 0x10) : 0; if (fs <= 1) return;   // boxed float: value after the 0x10 object header
  float lo = fs * 0.6f; bool on = true; int noWrap = 0;
  void* a1[1] = { &lo }; Invoke(m_tmpMin, txt, a1);
  void* a2[1] = { &fs }; Invoke(m_tmpMax, txt, a2);
  if (m_tmpWrap) { void* a3[1] = { &noWrap }; Invoke(m_tmpWrap, txt, a3); }   // TextWrappingModes.NoWrap: shrink, never a second line
  void* a4[1] = { &on }; Invoke(m_tmpAuto, txt, a4);
}
static bool g_nameFitOn = false;
static void RegisterRowSafe(void* self) { __try { RegisterItemRow(self); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void Hooked_ItemElemAwake(void* self, void* method) {
  g_itemElemAwakeOrig(self, method);
  if (g_ready) RegisterRowSafe(self);                                  // the seat icon pass's row list, see RefreshRowLists
  static int faults = 0;
  if (faults >= 5 || !g_nameFitOn) return;
  __try { FitItemName(self); }
  __except (EXCEPTION_EXECUTE_HANDLER) { if (++faults == 5) Log("fault while fitting list names; names keep the game's size"); }
}
static void InstallNameFit() {
  void* k = FindClass("Core.Customization", "ItemElementUI");
  void* mi = k ? il2cpp_class_get_method_from_name(k, "Awake", 0) : nullptr;
  void* f = k ? il2cpp_class_get_field_from_name(k, "textName") : nullptr;
  void* tmp = FindClass("TMPro", "TMP_Text");
  if (tmp) {
    m_tmpGetSize = FindMethod(tmp, "get_fontSize", 0); m_tmpAuto = FindMethod(tmp, "set_enableAutoSizing", 1);
    m_tmpMin = FindMethod(tmp, "set_fontSizeMin", 1); m_tmpMax = FindMethod(tmp, "set_fontSizeMax", 1);
    m_tmpWrap = FindMethod(tmp, "set_textWrappingMode", 1);
  }
  if (!mi) { Log("ItemElementUI.Awake not found; long names wrap and the seat icon pass scans for rows once"); return; }
  if (!f || !m_tmpGetSize || !m_tmpAuto || !m_tmpMin || !m_tmpMax) Log("list names: a lookup failed; long names wrap as before");
  else { g_offElemName = (int)il2cpp_field_get_offset(f); g_nameFitOn = true; }
  void** slot = (void**)mi; DWORD old = 0;                            // MethodInfo starts with methodPointer: swap it
  g_itemElemAwakeOrig = (void (*)(void*, void*))*slot;
  if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) { *slot = (void*)&Hooked_ItemElemAwake; VirtualProtect(slot, sizeof(void*), old, &old); DLog("hooked ItemElementUI.Awake: long names shrink to fit (wrap setter %p)", m_tmpWrap); }
}

