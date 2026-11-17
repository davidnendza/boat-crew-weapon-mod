// OnGUI hosts: OnGuiTick, GUI_HOOK, InstallInventoryGui (a fragment of weaponmod.cpp, included in order, not standalone)
static void OnGuiTick(int i) {
  StallBeat();
  if (!strcmp(g_guiHosts[i].cls, "TitleScreenManagerUI")) { SafeTitleBanner(); SafeTitleMenu(); }
  DWORD now = GetTickCount();
  if (g_guiDrawHost < 0 || now - g_guiLastDraw > 1500) {              // take over drawing if the current host stopped running
    if (g_guiDrawHost != i) DLog("OnGUI: drawing now via %s", g_guiHosts[i].cls);
    g_guiDrawHost = i;
  }
  TryInstallPerSave("OnGUI");
  TryInstallFemale();                                                  // female crew: the combineMesh detour, on this thread
  if (i == g_guiDrawHost) {
    g_guiLastDraw = now;
    LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
    InventoryGui();
    EliteTags();
    SafeAfterActionGui();
    SafeFileCabinetGui();
    SafeCrewMarks();
    SafeCoopTick();
    if (g_diagFrames > 0) { g_perfGuiMs += PerfMs(t0); g_perfGuiCalls++; }
  }
}

#define GUI_HOOK(i) static void Hooked_OnGUI_##i(void* self, void* m) { \
  g_guiHosts[i].orig(self, m); \
  if (!g_ready) return; \
  __try { OnGuiTick(i); } __except (EXCEPTION_EXECUTE_HANDLER) { GuiScale(1.0f); GuiThemeEnd(); if (++g_guiFaults <= 3) Log("fault in the inventory window (%d)", g_guiFaults); \
    if (g_guiFaults == 5) { g_invReady = false; Log("inventory window switched off after repeated faults"); } } }
GUI_HOOK(0) GUI_HOOK(1) GUI_HOOK(2) GUI_HOOK(3) GUI_HOOK(4) GUI_HOOK(5)
static void* g_guiHookFns[kGuiHosts] = { (void*)&Hooked_OnGUI_0, (void*)&Hooked_OnGUI_1, (void*)&Hooked_OnGUI_2, (void*)&Hooked_OnGUI_3, (void*)&Hooked_OnGUI_4, (void*)&Hooked_OnGUI_5 };

static void InstallInventoryGui() {
  void* gui = FindClass("UnityEngine", "GUI");
  if (gui) {
    m_guiLabel = FindMethod(gui, "Label", 2, "UnityEngine.Rect", "System.String");
    m_guiBox = FindMethod(gui, "Box", 2, "UnityEngine.Rect", "System.String");
    m_guiButton = FindMethod(gui, "Button", 2, "UnityEngine.Rect", "System.String");
    m_guiSetColor = FindMethod(gui, "set_color", 1);
    m_guiDrawTex = FindMethod(gui, "DrawTexture", 2, "UnityEngine.Rect", "UnityEngine.Texture");
    m_guiBeginGroup = FindMethod(gui, "BeginGroup", 3, "UnityEngine.Rect", "UnityEngine.GUIContent");
    m_guiEndGroup = FindMethod(gui, "EndGroup", 0);
    m_guiSetMatrix = FindMethod(gui, "set_matrix", 1);
    m_guiLabelSt = FindMethod(gui, "Label", 3, "UnityEngine.Rect", "System.String");
    m_guiButtonSt = FindMethod(gui, "Button", 3, "UnityEngine.Rect", "UnityEngine.GUIContent");
    m_guiGetSkin = FindMethod(gui, "get_skin", 0);
  }
  void* skinC = FindClass("UnityEngine", "GUISkin");
  if (skinC) { m_skinGetFont = FindMethod(skinC, "get_font", 0); m_skinSetFont = FindMethod(skinC, "set_font", 1); m_skinGetLabel = FindMethod(skinC, "get_label", 0); }
  void* styleC = FindClass("UnityEngine", "GUIStyle");
  if (styleC) { m_styleGetAlign = FindMethod(styleC, "get_alignment", 0); m_styleSetAlign = FindMethod(styleC, "set_alignment", 1); }
  Log("inventory look: styled label %s, styled button %s, skin %s, label alignment %s", m_guiLabelSt ? "ok" : "MISSING", m_guiButtonSt ? "ok" : "MISSING (default buttons)",
      m_guiGetSkin && m_skinGetLabel && m_skinSetFont ? "ok" : "MISSING", m_styleGetAlign && m_styleSetAlign ? "ok" : "MISSING (all text left-aligned)");
  void* gc = FindClass("UnityEngine", "GUIContent");
  void* fNone = gc ? il2cpp_class_get_field_from_name(gc, "none") : nullptr;
  if (fNone) il2cpp_field_static_get_value(fNone, &g_guiContentNone);
  void* gs = FindClass("UnityEngine", "GUIStyle");
  void* mNone = gs ? FindMethod(gs, "get_none", 0) : nullptr;
  g_guiStyleNone = mNone ? Invoke(mNone, nullptr) : nullptr;
  void* ev = FindClass("UnityEngine", "Event");
  if (ev) { m_evCurrent = FindMethod(ev, "get_current", 0); m_evType = FindMethod(ev, "get_type", 0); m_evKey = FindMethod(ev, "get_keyCode", 0); m_evMouse = FindMethod(ev, "get_mousePosition", 0); m_evUse = FindMethod(ev, "Use", 0); m_evButton = FindMethod(ev, "get_button", 0); m_evDelta = FindMethod(ev, "get_delta", 0); }
  void* scr = FindClass("UnityEngine", "Screen");
  if (scr) { m_scrW = FindMethod(scr, "get_width", 0); m_scrH = FindMethod(scr, "get_height", 0); }
  void* spr = FindClass("UnityEngine", "Sprite");
  if (spr) { m_spriteTex = FindMethod(spr, "get_texture", 0); m_spriteRect = FindMethod(spr, "get_rect", 0); }
  void* t2 = FindClass("UnityEngine", "Texture2D");
  if (t2) { m_texW = FindMethod(t2, "get_width", 0); m_texH = FindMethod(t2, "get_height", 0); void* wt = FindMethod(t2, "get_whiteTexture", 0); g_white = wt ? Invoke(wt, nullptr) : nullptr; Pin(g_white); }
  void* gm = FindClass("Core", "GunMount");
  if (gm) { m_mountSprite = FindMethod(gm, "getSprite", 0); m_mountProps = FindMethod(gm, "getProperties", 0); }
  g_invReady = m_guiLabel && m_guiBox && m_guiButton && m_guiSetColor && m_guiDrawTex && m_guiBeginGroup && m_guiEndGroup && g_guiStyleNone &&
               m_evCurrent && m_evType && m_evKey && m_evMouse && m_evButton && m_scrW && m_scrH && m_spriteTex && m_spriteRect && m_texW && m_texH;
  DLog("inventory window: %s (Label %p Box %p Button %p color %p DrawTexture %p BeginGroup %p EndGroup %p content.none %p style.none %p event %p/%p/%p/%p screen %p sprite %p/%p tex %p/%p mount %p/%p)",
      g_invReady ? "ready" : "MISSING PIECES, not shown", m_guiLabel, m_guiBox, m_guiButton, m_guiSetColor, m_guiDrawTex, m_guiBeginGroup, m_guiEndGroup,
      g_guiContentNone, g_guiStyleNone, m_evCurrent, m_evType, m_evKey, m_evMouse, m_scrW, m_spriteTex, m_spriteRect, m_texW, m_texH, m_mountSprite, m_mountProps);
  for (int i = 0; i < kGuiHosts; i++) {
    void* k = FindClass(g_guiHosts[i].ns, g_guiHosts[i].cls);
    void* mi = k ? il2cpp_class_get_method_from_name(k, "OnGUI", 0) : nullptr;
    if (!mi) { Log("inventory window: %s.%s has no OnGUI", g_guiHosts[i].ns, g_guiHosts[i].cls); continue; }
    void** slot = (void**)mi; DWORD old = 0;
    g_guiHosts[i].orig = (void (*)(void*, void*))*slot;
    if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) { *slot = g_guiHookFns[i]; VirtualProtect(slot, sizeof(void*), old, &old); }
  }
}

