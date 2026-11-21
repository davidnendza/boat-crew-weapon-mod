// helm view: ride along in the helmsman's head (a fragment of weaponmod.cpp, included in order, not standalone)
// Insert, in battle, puts the game camera at the eyes of the crew member at the helm (CrewMember.assignedRole 5); Insert
// again gives the game camera back. Nothing else changes: the helmsman keeps steering for you, WASD still drives the
// boat, the gunners pick targets and fire on their own as always. Mouse look without capturing the mouse: the cursor's
// offset from the centre of the game window turns the view (edge = 160 deg to the side, 45 deg up or down). The cursor
// is hidden meanwhile by answering the game window's WM_SETCURSOR (the first-person mod's lesson: Unity's
// Cursor.visible is fought by the game), and a banner at the top says how to leave. Only in the game's free camera
// (controlMode 0): targeting, rockets, torpedoes and the kill cam show their own view, with the cursor, and the helm
// view comes back after them. In co-op, Escape ends it: that menu does not pause, so nothing else gives the cursor back.
// Camera at CrewMember.headEnd + 0.18 m along the flat view direction, as in the first-person mod (confirmed there to
// look straight ahead, not through the head). The helmsman is read from the crew battle log's records (wm_crew_log.h).
// The player's crew only fire at targets marked in targeting mode (FireControlMaster.constrainToAssignedTargets),
// which the helm view never is in, so while it shows it lifts that limit the way an All-Knowing gun does (g_akHelm):
// the crew fire at every enemy in sight, shore guns included. Docks (StructureEntity) are never fire-control targets.
// shortcut: Insert is fixed, no drops.ini key or Mod Options row; add one if a player needs Insert for something else.
static bool GameInForeground();                                       // weaponmod.cpp
struct HvApi { bool ok = false, tried = false; void *camMain = nullptr, *compTr = nullptr, *trPos = nullptr, *trSetPR = nullptr, *modeField = nullptr; int headEnd = -1; };
static HvApi g_hv;
static bool g_hvOn = false, g_hvKeyWas = false, g_hvShowing = false;
static bool g_hvHideCursor = false;                                   // showing and not paused (the pause menu needs the cursor)
static DWORD g_hvHideAt = 0;                                          // GetTickCount of the last tick that wanted it hidden: the hide expires
                                                                      // by itself if the camera hook stops running with the flag still set
static int g_hvFaults = 0;
static void* g_hvMember = nullptr;                                    // the helmsman being shown, for the log
static void* g_hvEng = nullptr;                                       // the battle it was switched on in: never carried into the next
static bool HvInit() {
  if (g_hv.tried) return g_hv.ok;
  HvApi& h = g_hv; h.tried = true;
  void* cam = FindClass("UnityEngine", "Camera"); void* comp = FindClass("UnityEngine", "Component"); void* tr = FindClass("UnityEngine", "Transform");
  void* pc = FindClass("Core.Player", "PlayerCameraControlManager");
  h.camMain = cam ? FindMethod(cam, "get_main", 0) : nullptr; h.compTr = comp ? FindMethod(comp, "get_transform", 0) : nullptr;
  h.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr; h.trSetPR = tr ? FindMethod(tr, "SetPositionAndRotation", 2) : nullptr;
  h.modeField = pc ? il2cpp_class_get_field_from_name(pc, "controlMode") : nullptr;
  h.headEnd = FieldOffset(FindClass("Core", "CrewMember"), "headEnd");
  h.ok = h.camMain && h.compTr && h.trPos && h.trSetPR && h.modeField && h.headEnd >= 0 && g_oCmRole >= 0 && m_crewTrFwd;
  Log("helm view: %s (camera %p/%p/%p/%p, controlMode %p, headEnd +0x%X, role +0x%X, crew log %s)", h.ok ? "ready, Insert in battle" : "MISSING PIECES, off",
      h.camMain, h.compTr, h.trPos, h.trSetPR, h.modeField, h.headEnd, g_oCmRole, g_crewLogOn ? "on" : "OFF");
  return h.ok;
}
static CrewRec* HvHelmsman() {
  for (CrewRec& r : g_crewRecs) if (NrCrewUp(r) && *(int*)((char*)r.member + g_oCmRole) == 5) return &r;
  return nullptr;
}
static WNDPROC g_hvOrigProc = nullptr;
static LRESULT CALLBACK HvWndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_SETCURSOR && g_hvHideCursor && LOWORD(lp) == HTCLIENT && GetTickCount() - g_hvHideAt < 500 && GameInForeground()) { SetCursor(nullptr); return TRUE; }
  return CallWindowProcW(g_hvOrigProc, w, msg, wp, lp);
}
static void HvHookWindow() {                                          // once, on the game thread, while the game window is in front
  if (g_hvOrigProc) return;
  HWND w = GameInForeground() ? GetForegroundWindow() : nullptr; if (!w) return;
  g_hvOrigProc = (WNDPROC)SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)&HvWndProc);
  Log("helm view: %s", g_hvOrigProc ? "game window hooked, the cursor hides in the view" : "could not hook the game window, the cursor stays visible");
}
static void HvSet(bool on, const char* why) {
  if (on != g_hvOn) Log("helm view: %s (%s)", on ? "ON" : "off", why);
  if (on) HvHookWindow();
  g_hvOn = on; g_hvShowing = g_hvHideCursor = g_akHelm = false; g_hvMember = nullptr;
}
static void HvTick() {
  bool key = GameInForeground() && (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
  bool press = key && !g_hvKeyWas; g_hvKeyWas = key;
  void* eng = EngagementInstance(); bool battle = eng != nullptr;
  if (press) { if (g_hvOn) HvSet(false, "Insert"); else if (!battle) Log("helm view: Insert outside a battle, nothing to show"); else if (HvInit()) { HvSet(true, "Insert"); g_hvEng = eng; } }
  // A menu that does not pause the game (co-op) needs the cursor, and nothing else here sees it open.
  if (g_hvShowing && g_coopRole && GameInForeground() && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) HvSet(false, "Escape in co-op");
  if (!g_hvOn) return;
  if (!battle || eng != g_hvEng) { HvSet(false, "the battle ended"); return; }
  if (!g_crewLogOn || g_crewLogFaults < 0) { HvSet(false, "the crew battle log is off, so the helmsman cannot be found"); return; }
  int mode = -1; il2cpp_field_static_get_value(g_hv.modeField, &mode);
  CrewRec* r = HvHelmsman();
  bool show = mode == 0 && r && !g_kcState;                            // the torpedo kill cam shows its own view, without the banner
  bool ak = mode == 0 && r;                                            // the crew keep firing at everything through a kill cam
  if (show != g_hvShowing || ak != g_akHelm || (r && r->member != g_hvMember)) {
    if (!r) Log("helm view: nobody at the helm, game camera until someone takes it");
    else if (mode != 0) Log("helm view: camera mode %d, the game's view until it is back to free", mode);
    else if (g_kcState) Log("helm view: kill cam, its view until it is over");
    else Log("helm view: riding with %s at the helm", r->name.c_str());
    g_hvShowing = show; g_akHelm = ak; g_hvMember = r ? r->member : nullptr;   // g_akHelm: the crew fire at every enemy in sight (wm_all_knowing.h)
  }
  g_hvHideCursor = show && GameTimeScale() > 0.01f;                    // Time.timeScale is 0 while paused
  if (g_hvHideCursor) g_hvHideAt = GetTickCount();
  if (!ak) return;                                                     // under a kill cam the camera is still placed: it runs after and wins
  void* head = *(void**)((char*)r->member + g_hv.headEnd); if (!UnityAlive(head)) return;
  V3 eye = GetV3(g_hv.trPos, head);
  V3 f = GetV3(m_crewTrFwd, Invoke(g_hv.compTr, r->member)); float fl = sqrtf(f.x * f.x + f.z * f.z);
  if (fl < 0.2f) return;
  f = { f.x / fl, 0, f.z / fl };
  float nx = 0, ny = 0; HWND w = GameInForeground() ? GetForegroundWindow() : nullptr; RECT rc; POINT p;  // cursor offset from the game window's centre, -1..1
  if (w && GetClientRect(w, &rc) && rc.right > 0 && rc.bottom > 0 && GetCursorPos(&p) && ScreenToClient(w, &p)) {
    if (g_hvHideCursor && PtInRect(&rc, p)) SetCursor(nullptr);        // between mouse moves too; not over the title bar or another window
    nx = (std::max)(-1.0f, (std::min)(1.0f, (p.x - rc.right * 0.5f) / (rc.right * 0.5f)));
    ny = (std::max)(-1.0f, (std::min)(1.0f, (p.y - rc.bottom * 0.5f) / (rc.bottom * 0.5f)));
  }
  float yaw = nx * 160 * 0.01745329f, pitch = -ny * 45 * 0.01745329f, cy = cosf(yaw), sy = sinf(yaw);
  V3 d = { f.x * cy + f.z * sy, 0, f.z * cy - f.x * sy };              // turned right by yaw about world up
  eye = { eye.x + d.x * 0.18f, eye.y, eye.z + d.z * 0.18f };           // ahead along the VIEW, so the face stays behind the camera
  V3 look = { d.x * cosf(pitch), sinf(pitch), d.z * cosf(pitch) };
  void* cam = Invoke(g_hv.camMain, nullptr); void* t = cam ? Invoke(g_hv.compTr, cam) : nullptr; if (!t) return;
  Q4 q = LookRot(look); void* a[2] = { &eye, &q }; Invoke(g_hv.trSetPR, t, a);
}
// The banner at the top of the screen while the helm view is showing (OnGUI, wm_gui_host.h).
static void HelmViewGui() {
  if (!g_hvShowing || g_hvFaults < 0 || !m_scrW || !m_scrH) return;
  void* ev = Invoke(m_evCurrent, nullptr); if (!ev || Unbox(Invoke(m_evType, ev)) != 7) return;   // Repaint only
  float sw = (float)Unbox(Invoke(m_scrW, nullptr)), sh = (float)Unbox(Invoke(m_scrH, nullptr)); if (sw <= 0 || sh <= 0) return;
  float k = sh / 1080.0f, w = 520 * k, h = 40 * k;
  Rect4 r{ (sw - w) / 2, 14 * k, w, h };
  GuiThemeBegin();
  FillC(r, Hex(0x0a0c10, 0.6f));
  Text(r, "HELM VIEW  -  press Insert to exit", (int)(18 * k + 0.5f), "#eef1f4", kCenter, true);
  GuiThemeEnd();
}
// From the game camera's LateUpdate, after the game's own update and before the kill cam (which then wins when it runs).
static void HelmViewFrame() {
  if (g_hvFaults < 0) return;
  __try { HvTick(); }
  __except (FaultFilter("helm view", GetExceptionInformation())) { g_hvFaults = -1; g_hvOn = g_hvShowing = g_hvHideCursor = g_akHelm = false; }
}
