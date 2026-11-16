// Battle newsreel (2026-11-14): a second camera, never shown on screen, films enemy planes going down (chase camera),
// enemy ships sinking (orbit), planes or ships coming close (from the deck) and our own crew at work; at the battle's end the shots are written
// as a black-and-white GIF to BoatCrewMod\newsreels\.
// Included from weaponmod.cpp after the kill cam (uses its helpers). Research: memory note boat-crew-highlight-camera-research.
// Planes: PlaneControllerAI.FixedUpdate is hooked to collect the live controllers; flightState 2 = Dive, 6 = Decease.
// Camera: a disabled Camera with a 320x180 RenderTexture, drawn with Camera.Render() 10 times a second during a shot and
// read back with Texture2D.ReadPixels (the texture overloads of AsyncGPUReadback are stripped from this build).
#pragma once
#include "gif_writer.h"
#include "newsreel_tv.h"                                            // kNrTvHtml: the Newsreel TV page

static const int kNrMaxFalls = 5, kNrMaxSwoops = 3;                   // plane caps per battle (David 2026-11-15; 07:56 film: 11 falls filled it by minute 2)
static const int kNrW = 640, kNrH = 360, kNrMaxFrames = 1200;         // 60 s of film per battle at 20 frames/s (David 2026-11-14: double size and fps)

struct NrApi {
  bool ok = false, tried = false;
  void *goCls = nullptr, *goCtor = nullptr, *addComp = nullptr, *camType = nullptr, *setEnabled = nullptr, *setFov = nullptr, *setNear = nullptr,
       *setFar = nullptr, *getFar = nullptr, *getMask = nullptr, *setMask = nullptr, *setTarget = nullptr, *render = nullptr, *camMain = nullptr,
       *compTr = nullptr, *trSetPR = nullptr, *trPos = nullptr, *rtCls = nullptr, *rtCtor = nullptr, *rtGetActive = nullptr, *rtSetActive = nullptr,
       *texCls = nullptr, *texCtor = nullptr, *readPx = nullptr, *getRaw = nullptr, *dontDestroy = nullptr,
       *trUp = nullptr, *linecast = nullptr, *linecastHit = nullptr, *hitCollider = nullptr, *trIsChild = nullptr;   // hit: what blocks a gunner's spot                             // optional: deck shots check their camera spot with Physics.Linecast
  int ctrlPlane = -1, flight = -1, leTeam = -1, leAlive = -1;
};
static NrApi g_nr;
static int g_nrFaults = 0;
static void (*g_nrPlaneFixedOrig)(void*, void*) = nullptr;
static std::unordered_map<void*, DWORD> g_nrPlanes;                   // PlaneControllerAI -> last FixedUpdate tick

static void NrSeePlane(void* ctrl) { g_nrPlanes[ctrl] = GetTickCount(); }
static void Hooked_NrPlaneFixed(void* self, void* method) {
  g_nrPlaneFixedOrig(self, method);
  if (g_nrFaults >= 0 && self) NrSeePlane(self);
}

static void NrWriteTv(const std::string& dir);
static void NewsreelInstall() {
  NrApi& a = g_nr;
  auto off = [](void* k, const char* n) { void* f = k ? il2cpp_class_get_field_from_name(k, n) : nullptr; return f ? (int)il2cpp_field_get_offset(f) : -1; };
  void* pc = FindClass("Core", "PlaneControllerAI"); void* le = FindClass("Core", "LiveEntity");
  a.ctrlPlane = off(pc, "plane"); a.flight = off(pc, "flightState"); a.leTeam = off(le, "teamCode"); a.leAlive = off(le, "_alive");
  a.goCls = FindClass("UnityEngine", "GameObject");
  a.goCtor = a.goCls ? FindMethod(a.goCls, ".ctor", 1, "System.String") : nullptr; a.addComp = a.goCls ? FindMethod(a.goCls, "AddComponent", 1, "System.Type") : nullptr;
  void* cam = FindClass("UnityEngine", "Camera"); a.camType = cam ? il2cpp_type_get_object(il2cpp_class_get_type(cam)) : nullptr;
  void* bh = FindClass("UnityEngine", "Behaviour"); a.setEnabled = bh ? FindMethod(bh, "set_enabled", 1) : nullptr;
  if (cam) {
    a.setFov = FindMethod(cam, "set_fieldOfView", 1); a.setNear = FindMethod(cam, "set_nearClipPlane", 1); a.setFar = FindMethod(cam, "set_farClipPlane", 1);
    a.getFar = FindMethod(cam, "get_farClipPlane", 0); a.getMask = FindMethod(cam, "get_cullingMask", 0); a.setMask = FindMethod(cam, "set_cullingMask", 1);
    a.setTarget = FindMethod(cam, "set_targetTexture", 1); a.render = FindMethod(cam, "Render", 0); a.camMain = FindMethod(cam, "get_main", 0);
  }
  void* comp = FindClass("UnityEngine", "Component"); a.compTr = comp ? FindMethod(comp, "get_transform", 0) : nullptr;
  void* tr = FindClass("UnityEngine", "Transform"); a.trSetPR = tr ? FindMethod(tr, "SetPositionAndRotation", 2) : nullptr; a.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr;
  a.rtCls = FindClass("UnityEngine", "RenderTexture");
  if (a.rtCls) { a.rtCtor = FindMethod(a.rtCls, ".ctor", 3, "System.Int32", "System.Int32"); a.rtGetActive = FindMethod(a.rtCls, "get_active", 0); a.rtSetActive = FindMethod(a.rtCls, "set_active", 1); }
  a.texCls = FindClass("UnityEngine", "Texture2D");
  if (a.texCls) {
    void* iter = nullptr; void* m;                                     // Texture2D(int width, int height, TextureFormat, bool mipChain)
    while ((m = il2cpp_class_get_methods(a.texCls, &iter)))
      if (!strcmp(il2cpp_method_get_name(m), ".ctor") && il2cpp_method_get_param_count(m) == 4 &&
          TypeName(il2cpp_method_get_param(m, 2)) == "UnityEngine.TextureFormat" && TypeName(il2cpp_method_get_param(m, 3)) == "System.Boolean") { a.texCtor = m; break; }
    a.readPx = FindMethod(a.texCls, "ReadPixels", 3); a.getRaw = FindMethod(a.texCls, "GetRawTextureData", 0);
  }
  if (tr) a.trUp = FindMethod(tr, "get_up", 0);
  void* phys = FindClass("UnityEngine", "Physics"); a.linecast = nullptr;
  if (phys) {                                                          // the mask + trigger overload: smoke clouds (layer 12) and trigger
    void* iter = nullptr; void* m;                                     // volumes must not block a spot (review 2026-11-14)
    while ((m = il2cpp_class_get_methods(phys, &iter)))
      if (!strcmp(il2cpp_method_get_name(m), "Linecast") && il2cpp_method_get_param_count(m) == 4 &&
          TypeName(il2cpp_method_get_param(m, 2)) == "System.Int32" && TypeName(il2cpp_method_get_param(m, 3)) == "UnityEngine.QueryTriggerInteraction") a.linecast = m;
      else if (!strcmp(il2cpp_method_get_name(m), "Linecast") && il2cpp_method_get_param_count(m) == 5 &&   // (start, end, out RaycastHit, mask, triggers)
          TypeName(il2cpp_method_get_param(m, 2)).rfind("UnityEngine.RaycastHit", 0) == 0 && TypeName(il2cpp_method_get_param(m, 3)) == "System.Int32") a.linecastHit = m;
  }
  void* rh = FindClass("UnityEngine", "RaycastHit"); a.hitCollider = rh ? FindMethod(rh, "get_collider", 0) : nullptr;
  if (tr) a.trIsChild = FindMethod(tr, "IsChildOf", 1);
  void* ok = FindClass("UnityEngine", "Object"); a.dontDestroy = ok ? FindMethod(ok, "DontDestroyOnLoad", 1) : nullptr;
  std::string missing;
  const std::pair<const char*, bool> need[] = { { "PlaneControllerAI.plane/flightState", a.ctrlPlane >= 0 && a.flight >= 0 }, { "LiveEntity.teamCode/_alive", a.leTeam >= 0 && a.leAlive >= 0 },
    { "GameObject(string)/AddComponent", a.goCtor && a.addComp }, { "Camera", a.camType && a.setEnabled && a.setFov && a.setNear && a.setFar && a.getFar && a.getMask && a.setMask && a.setTarget && a.render && a.camMain },
    { "Transform", a.compTr && a.trSetPR && a.trPos }, { "RenderTexture(int,int,int)/active", a.rtCtor && a.rtGetActive && a.rtSetActive },
    { "Texture2D(int,int,TextureFormat,bool)/ReadPixels/GetRawTextureData", a.texCtor && a.readPx && a.getRaw } };
  for (auto& n : need) if (!n.second) missing += std::string(" ") + n.first;
  if (!missing.empty()) { Log("newsreel: off (missing%s)", missing.c_str()); g_nrFaults = -1; return; }
  if (!pc || !PatchPointer(pc, "FixedUpdate", 0, (void*)&Hooked_NrPlaneFixed, (void**)&g_nrPlaneFixedOrig)) { Log("newsreel: off (could not hook PlaneControllerAI.FixedUpdate)"); g_nrFaults = -1; return; }
  a.ok = true;
  if (!a.linecastHit || !a.hitCollider || !a.trIsChild) Log("newsreel: Linecast with hit %s, RaycastHit.collider %s, Transform.IsChildOf %s; a gunner's own gun blocks his spots",
      a.linecastHit ? "found" : "NOT found", a.hitCollider ? "found" : "NOT found", a.trIsChild ? "found" : "NOT found");
  if (!a.linecast || !a.trUp) Log("newsreel: Physics.Linecast(mask, triggers) %s, Transform.get_up %s; deck shots stand where a crewmate stands, unchecked", a.linecast ? "found" : "NOT found", a.trUp ? "found" : "NOT found");
  Log("newsreel: ready (the best %d planes going down, planes swooping at us, ships sinking, close ships and our crew at work are filmed %dx%d at 20 frames/s, up to %d s per battle, written to BoatCrewMod\\newsreels)", kNrMaxFalls, kNrW, kNrH, kNrMaxFrames / 20);
  { std::string d = g_folder + "\\newsreels"; if (GetFileAttributesA(d.c_str()) != INVALID_FILE_ATTRIBUTES) NrWriteTv(d); }   // the TV page lists the GIFs already there
}

// The camera, its texture and the readback texture; made once and kept across scenes.
static void *g_nrCam = nullptr, *g_nrCamTr = nullptr, *g_nrRt = nullptr, *g_nrTex = nullptr;
static bool UnityAlive(void* o) { return o && *(void**)((char*)o + 0x10); }
static bool NrMakeCamera() {
  if (UnityAlive(g_nrCam) && UnityAlive(g_nrRt) && UnityAlive(g_nrTex)) return true;
  NrApi& a = g_nr;
  bool newCam = !UnityAlive(g_nrCam);
  if (newCam) {
    void* go = il2cpp_object_new(a.goCls); void* ga[1] = { il2cpp_string_new("Newsreel Camera") }; Invoke(a.goCtor, go, ga);
    if (a.dontDestroy) { void* da[1] = { go }; Invoke(a.dontDestroy, nullptr, da); }
    void* ca[1] = { a.camType }; void* cam = Invoke(a.addComp, go, ca);
    if (!cam) { Log("newsreel: could not add a Camera; off"); return false; }
    bool off = false; void* ea[1] = { &off }; Invoke(a.setEnabled, cam, ea);       // drawn only by Render(), never to the screen
    float fov = 55, nearP = 0.5f; void* fa[1] = { &fov }; Invoke(a.setFov, cam, fa); void* na[1] = { &nearP }; Invoke(a.setNear, cam, na);
    Pin(go); Pin(cam);                                                 // shortcut: never freed, one per session
    g_nrCam = cam; g_nrCamTr = Invoke(a.compTr, cam);
  }
  // The textures are assets, not scene objects: the game unloads them between battles even while pinned (second battle of
  // the 2026-11-14 19:41 session filmed 40 shots and kept 0 frames), so they are remade when gone.
  int w = kNrW, h = kNrH, d = 24, fmt = 4 /* RGBA32 */; bool mip = false;
  void* rt = il2cpp_object_new(a.rtCls); void* ra[3] = { &w, &h, &d }; Invoke(a.rtCtor, rt, ra);
  void* tex = il2cpp_object_new(a.texCls); void* ta[4] = { &w, &h, &fmt, &mip }; Invoke(a.texCtor, tex, ta);
  void* sa[1] = { rt }; Invoke(a.setTarget, g_nrCam, sa);
  Pin(rt); Pin(tex);                                                   // shortcut: old pins leak a few bytes per remake
  g_nrRt = rt; g_nrTex = tex;
  DLog("newsreel: %s (%p, texture %p)", newCam ? "camera made" : "textures remade (the old ones were unloaded)", g_nrCam, rt);
  return g_nrCamTr != nullptr;
}

static GifFrames g_nrFilm;
static void* g_nrEng = nullptr;
static int g_nrShots = 0, g_nrDowned = 0;
// Shot kinds, most important first: a plane going down, an enemy ship sinking, a plane diving at us or passing low, a ship close by.
// Last: one of our own crew at work on deck, in a quiet spell (David 2026-11-14: "like a camera person on the deck").
enum NrKind { kNrPlaneFall = 0, kNrShipSink = 1, kNrPlaneClose = 2, kNrShipClose = 3, kNrDeck = 4 };
static const char* kNrKindName[] = { "plane going down", "ship sinking", "plane swooping", "ship close by", "crew on deck" };
// ent: the filmed Plane, Ship or CrewMember (shot active when set); ctrl: its PlaneControllerAI (planes only);
// ch: the CrewCharacter and role the job (deck shots only).
struct NrShot { void* ent = nullptr; void* ctrl = nullptr; void* h = nullptr; int kind = 0; DWORD start = 0, lastDive = 0, downAt = 0, lastCap = 0, lastSeen = 0; std::string name;
                V3 look{}; bool lookSet = false, gone = false, velOk = false; float closest = 1e9f, orbit0 = 0, size = 0; V3 lastP{}, vel{}, fdir{};
                void* ch = nullptr; int role = 0; V3 camL{}; bool follow = false; DWORD gunAt = 0; int rounds = 0; };   // follow: walking behind an ammunition runner                  // camL: the deck camera's spot in the boat's frame
static const DWORD kNrFallMs = 5000, kNrSinkMs = 7000, kNrCloseShipMs = 6000, kNrDeckMs = 4000;
// Deck shots: at most 4 a battle (about 16 s of the 60 s film), 15 s apart, none in the first 15 s (crew still taking
// stations), each man once; the job turns gunner (only while his gun fires) -> helmsman -> ammunition/repair/medic.
static const int kNrMaxDeck = 6; static int g_nrDeckShots = 0; static DWORD g_nrLastDeck = 0, g_nrBattleAt = 0, g_nrCombatAt = 0;
static const DWORD kNrGunFireMs = 1500, kNrGunMinMs = 3000, kNrGunMaxMs = 6000;   // a gunner on film: while his gun fires, 3-6 s
static std::unordered_set<void*> g_nrDeckFilmed;                      // CrewCharacters already filmed this battle
static std::unordered_map<void*, V3> g_nrShipsPrev;                   // live enemy ships last frame -> position
static std::unordered_set<void*> g_nrSunk, g_nrShipFilmed;            // ships seen dying / already given a close-by shot
static std::unordered_set<void*> g_nrPlaneFilmed;                     // planes already given a close-by shot (review 2026-11-14)
static void* g_nrPendSink = nullptr; static void* g_nrPendSinkH = nullptr; static V3 g_nrPendSinkP{}; static DWORD g_nrPendSinkAt = 0;   // a sinking that came while busy
static int g_nrSinks = 0, g_nrSmallSinks = 0;
// Small boats (Ka-Mi, landing boats, gunboats) sink by the dozen and filled the 20:21 film: at most 3 per battle.
static const float kNrSmallBoat = 25; static const int kNrMaxSmallSinks = 3;
static std::unordered_set<void*> g_nrDownSeen;                        // planes already filmed going down (cleared when seen flying again)
// David 2026-11-15: "a cap on the number of planes ... the ones that are 'swooping' or good action shots". Each fall is scored
// once when the camera could take it; later falls of a battle must score higher, at most kNrMaxFalls.
static const int kNrFallBar[kNrMaxFalls] = { 15, 25, 35, 50, 65 };    // score needed by falls already filmed
static const float kNrFallR = 2500, kNrFallMinY = 25, kNrFallNear = 500;   // below 25 m and over 500 m off: only a splash (07:56: falls from 7-16 m)
static const float kNrSwoopR = 1000, kNrSwoopClosing = 15, kNrLowPassR = 350, kNrLowPassY = 150;   // closest dive measured: 627 m
static const DWORD kNrSwoopDiveMs = 3000, kNrDiveBonusMs = 10000;
// Per-plane flight history (keyed by PlaneControllerAI; reset when a pooled controller flies again or drops off the hook).
struct NrPlaneHist { DWORD diveAt = 0, prevAt = 0, downAt = 0; float prevD = 0, closing = 0, downY = 0, downD = 0, downClosing = 0; bool down = false; };
static std::unordered_map<void*, NrPlaneHist> g_nrHist;
static int g_nrSwoops = 0, g_nrFallsSkipped = 0;
struct NrFallParts { int dist, height, dive, closing, total; };
static NrFallParts NrFallScore(const NrPlaneHist& h) {                 // from the moment it went down (queued planes only lose height)
  NrFallParts r;
  r.dist = (int)(40 * (std::max)(0.0f, (std::min)(1.0f, (kNrFallR - h.downD) / 2100)));   // 40 within 400 m, 0 at 2.5 km
  r.height = (int)(30 * (std::min)(h.downY, 300.0f) / 300);           // a long burning fall
  r.dive = h.diveAt && h.downAt - h.diveAt < kNrDiveBonusMs ? 20 : 0;  // hit in or just after an attack run
  r.closing = h.downClosing > 20 ? 10 : 0;                             // coming at us
  r.total = r.dist + r.height + r.dive + r.closing; return r;
}
static NrShot g_nrShot;
static DWORD g_nrLastEnd = 0;
static bool g_nrCapLogged = false, g_nrFullLogged = false;
static void NrDropPending(const char* why) {
  if (g_nrPendSink && why) DLog("newsreel: a queued ship sinking was not filmed (%s)", why);
  Unpin(g_nrPendSinkH); g_nrPendSinkH = nullptr; g_nrPendSink = nullptr;
}
// Per-battle summary: what the plane hook saw, so a battle without shots says why.
static std::unordered_set<void*> g_nrSeenAll, g_nrSeenEnemy;
static int g_nrStateFrames[8] = {}; static float g_nrClosestAny = 1e9f, g_nrClosestDive = 1e9f;

// David 2026-11-15: keep only the newest newsreel_keep films, and never delete anything else. A film name is exactly
// newsreel_YYYY-MM-DD_HH-MM-SS.gif (what NrWriteThread writes), so a renamed or moved GIF is safe; read-only ones too.
static bool NrIsFilmName(const char* n) {
  static const char pat[] = "newsreel_####-##-##_##-##-##.gif";
  if (strlen(n) != sizeof pat - 1) return false;
  for (size_t i = 0; pat[i]; i++) if (pat[i] == '#' ? !isdigit((unsigned char)n[i]) : n[i] != pat[i]) return false;
  return true;
}
static void NrPrune(const std::string& dir, const char* newest) {
  int keep = g_newsreelKeep; if (keep <= 0) return;
  std::vector<std::string> films; WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA((dir + "\\newsreel_*.gif").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return;
  do if (!(fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_REPARSE_POINT)) && NrIsFilmName(fd.cFileName)) films.push_back(fd.cFileName);
  while (FindNextFileA(h, &fd));
  FindClose(h);
  std::sort(films.begin(), films.end());                               // the name is the date and time: oldest first
  for (size_t i = 0; i + keep < films.size(); i++) {
    if (films[i] == newest) continue;                                  // never the one just written (a clock set back)
    if (DeleteFileA((dir + "\\" + films[i]).c_str())) Log("newsreel: deleted the oldest film newsreels\\%s (newsreel_keep=%d)", films[i].c_str(), keep);
    else Log("newsreel: could not delete newsreels\\%s (error %lu)", films[i].c_str(), GetLastError());
  }
}
// David 2026-11-15: "a simple HTML page ... like an old TV and you can change the 'channel' to show the different clips".
// A page opened from disk cannot list its folder, so the mod rewrites "Newsreel TV.html" with every GIF in the folder
// (renamed ones too) after each film and at launch. Names go in as a JSON array of UTF-8 strings, sorted (= oldest first).
static SRWLOCK g_nrTvLock = SRWLOCK_INIT;                              // two writer threads (battles ending close together) share the .tmp
static void NrWriteTv(const std::string& dir) {
  AcquireSRWLockExclusive(&g_nrTvLock); struct Rel { ~Rel() { ReleaseSRWLockExclusive(&g_nrTvLock); } } rel;
  std::vector<std::string> gifs; WIN32_FIND_DATAW fd;
  int wn = MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, nullptr, 0); if (wn <= 1) return;   // g_folder is ANSI (GetModuleFileNameA), maybe not ASCII
  std::wstring wdir(wn - 1, 0); MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, &wdir[0], wn);
  HANDLE h = FindFirstFileW((wdir + L"\\*.gif").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND) { Log("newsreel: could not list newsreels for the TV page (error %lu)", GetLastError()); return; }
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      int n = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nullptr, 0, nullptr, nullptr); if (n <= 1) continue;
      std::string u(n - 1, 0); WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, &u[0], n, nullptr, nullptr); gifs.push_back(u);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(gifs.begin(), gifs.end());
  std::string list = "[";
  for (const std::string& g : gifs) {
    if (list.size() > 1) list += ",";
    list += '"';
    for (unsigned char c : g) {
      if (c == '"' || c == '\\') { list += '\\'; list += (char)c; }
      else if (c == '<') list += "\\u003c";                            // never "</script>" inside the script
      else if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); list += e; }
      else list += (char)c;
    }
    list += '"';
  }
  list += "]";
  std::string page = kNrTvHtml; size_t a = page.find("/*FILMS*/"), b = page.find("/*END*/");
  if (a == std::string::npos || b == std::string::npos || b < a) return;
  page.replace(a + 9, b - a - 9, list);
  std::string path = dir + "\\Newsreel TV.html";
  FILE* f = fopen((path + ".tmp").c_str(), "wb"); if (!f) { Log("newsreel: could not write newsreels\\Newsreel TV.html"); return; }
  fwrite(page.data(), 1, page.size(), f); fclose(f);
  if (!MoveFileExA((path + ".tmp").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileA((path + ".tmp").c_str()); Log("newsreel: could not replace newsreels\\Newsreel TV.html (error %lu)", GetLastError()); }
}
static DWORD WINAPI NrWriteThread(void* p) {
  GifFrames* f = (GifFrames*)p;
  try {                                                                // an exception escaping a thread ends the game (0.9.6 review)
  SYSTEMTIME t; GetLocalTime(&t); char name[96];
  snprintf(name, sizeof name, "newsreel_%04d-%02d-%02d_%02d-%02d-%02d.gif", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
  std::string dir = g_folder + "\\newsreels"; CreateDirectoryA(dir.c_str(), nullptr);
  DWORD t0 = GetTickCount(); long n = GifWrite(dir + "\\" + name, *f);
  if (n > 0) { Log("newsreel: wrote newsreels\\%s (%d frames, %ld KB) in %lu ms", name, (int)f->frames.size(), n / 1024, GetTickCount() - t0); NrPrune(dir, name); NrWriteTv(dir); }
  else Log("newsreel: could not write newsreels\\%s", name);
  } catch (...) { Log("newsreel: out of memory writing the GIF; this battle's film was dropped"); }
  delete f; return 0;
}
static void NrEndShot(const char* why) {
  DLog("newsreel: shot %d (%s) of '%s' ended (%s) after %.1f s; closest %.0f m; film now %d frames", g_nrShots, kNrKindName[g_nrShot.kind], g_nrShot.name.c_str(), why,
      (GetTickCount() - g_nrShot.start) / 1000.0, g_nrShot.closest, (int)g_nrFilm.frames.size());
  Unpin(g_nrShot.h); g_nrShot = NrShot(); g_nrLastEnd = GetTickCount();
}
static void NrFinishBattle() {
  if (g_nrShot.ent) NrEndShot("the battle ended");
  if (!g_nrFilm.frames.empty()) {
    Log("newsreel: battle over: %d shots (%d planes going down, %d swooping, %d ships sinking, %d of our crew on film), %d frames, %d falls passed over; writing the GIF",
        g_nrShots, g_nrDowned, g_nrSwoops, g_nrSinks, g_nrDeckShots, (int)g_nrFilm.frames.size(), g_nrFallsSkipped);
    GifFrames* job = new GifFrames(std::move(g_nrFilm));
    HANDLE th = CreateThread(nullptr, 0, NrWriteThread, job, 0, nullptr);
    if (th) CloseHandle(th); else { Log("newsreel: could not start the writer thread"); delete job; }
  }
  if (g_nrEng) {
    std::string st; for (int i = 0; i < 7; i++) if (g_nrStateFrames[i]) st += " " + std::to_string(i) + ":" + std::to_string(g_nrStateFrames[i]);
    char cd[48] = "none dived"; if (g_nrClosestDive < 1e8f) snprintf(cd, sizeof cd, "%.0f m while diving", g_nrClosestDive);
    DLog("newsreel: planes last battle: %d seen by the hook, %d enemy; closest enemy %.0f m (%s); enemy frames by flight state (0 idle 1 focus 2 dive 3 break 4 recover 5 dismiss 6 down):%s",
        (int)g_nrSeenAll.size(), (int)g_nrSeenEnemy.size(), g_nrClosestAny, cd, st.empty() ? " none" : st.c_str());
  }
  g_nrSeenAll.clear(); g_nrSeenEnemy.clear(); g_nrDownSeen.clear(); memset(g_nrStateFrames, 0, sizeof g_nrStateFrames); g_nrClosestAny = g_nrClosestDive = 1e9f;
  g_nrFilm = GifFrames(); g_nrShots = g_nrDowned = g_nrSinks = g_nrSmallSinks = 0; g_nrCapLogged = g_nrFullLogged = false; g_nrPlanes.clear();
  g_nrShipsPrev.clear(); g_nrSunk.clear(); g_nrShipFilmed.clear(); g_nrPlaneFilmed.clear(); NrDropPending(g_nrEng ? "the battle ended" : nullptr);
  g_nrDeckShots = 0; g_nrLastDeck = 0; g_nrDeckFilmed.clear(); g_nrBattleAt = GetTickCount(); g_nrCombatAt = 0;
  g_nrHist.clear(); g_nrSwoops = g_nrFallsSkipped = 0;
}
// Deck shots read the crew battle log's records (weaponmod.cpp CrewRec, polled every 250 ms): the CrewMember, state, job.
static bool NrCrewOk() { return g_crewLogOn && g_crewLogFaults >= 0 && g_oCmRole >= 0 && g_oCmHeadP >= 0 && g_oCmChestP >= 0 && g_oCmFeet >= 0 && m_crewTrFwd && m_crewTrRight; }
static bool NrCrewUp(const CrewRec& r) {                               // aboard, alive and on their feet
  return UnityAlive(r.member) && (r.state == 0 || r.state == 1) && !r.down;
}
// A crew member to film: the job in turn first, then the other jobs. Roles: 1 repair, 2 medic, 3 ammo, 4 gun, 5 helm.
// A gunner's turn waits up to 5 s from waitFrom for a gun to fire before another job is filmed (gun shots are the best ones).
// David 2026-11-15: "camera man shots of the crew while they are in combat, shooting at things": two turns in three are a gunner's.
static void* NrGunOf(void* member) { return g_offCrewGun >= 0 ? *(void**)((char*)member + g_offCrewGun) : nullptr; }
static DWORD NrGunShotAt(void* member) { void* m = NrGunOf(member); auto f = m ? g_mountShotAt.find(m) : g_mountShotAt.end(); return f != g_mountShotAt.end() ? f->second : 0; }
static bool NrInCombat(DWORD now) { for (auto& m : g_mountShotAt) if (now - m.second < 4000 && OnPlayerBoat(m.first)) return true; return false; }   // one of OUR guns fired lately (the map holds enemy shots too)
static CrewRec* NrPickCrew(DWORD now, DWORD waitFrom) {
  bool gunTurn = g_nrDeckShots % 3 != 2, anyGunner = false;
  static const int kOrder[2][3] = { { 0, 2, 1 }, { 2, 1, 0 } };        // 0 gunner, 1 helmsman, 2 ammunition / repair / medic
  for (int k = 0; k < 3; k++) {
    if (k == 1 && gunTurn && anyGunner && now - waitFrom < 5000) return nullptr;
    int want = kOrder[gunTurn ? 0 : 1][k];
    for (CrewRec& r : g_crewRecs) {
      if (!NrCrewUp(r) || g_nrDeckFilmed.count(r.ch)) continue;
      int role = *(int*)((char*)r.member + g_oCmRole);
      if (want == 0 && role == 4) {                                    // a gunner only while his gun is firing
        anyGunner = true;
        DWORD at = NrGunShotAt(r.member);
        if (at && now - at < kNrGunFireMs) return &r;                   // 700 ms missed the 6-pdr between rounds
      }
      else if (want == 1 && role == 5) return &r;
      else if (want == 2 && role >= 1 && role <= 3) return &r;
    }
  }
  return nullptr;
}
static CrewRec* NrCrewRecOf(void* ch) { auto f = g_crewRecOf.find(ch); return f != g_crewRecOf.end() && f->second < g_crewRecs.size() ? &g_crewRecs[f->second] : nullptr; }
static bool NrPos(void* comp, V3& out) {
  void* t = Invoke(g_nr.compTr, comp); void* p = t ? Invoke(g_nr.trPos, t) : nullptr; if (!p) return false;
  out = *(V3*)((char*)p + 0x10); return true;
}
// LookRot with a given up: a camera braced on deck rolls and pitches with the hull (David 2026-11-15).
static Q4 NrLookRotUp(V3 f, V3 u) {
  float l = sqrtf(V3Dot(f, f)); if (l < 1e-5f) return { 0, 0, 0, 1 };
  f = { f.x / l, f.y / l, f.z / l };
  V3 r = { u.y * f.z - u.z * f.y, u.z * f.x - u.x * f.z, u.x * f.y - u.y * f.x };   // Cross(up, forward)
  float rl = sqrtf(V3Dot(r, r)); if (rl < 1e-3f) return LookRot(f);
  r = { r.x / rl, r.y / rl, r.z / rl };
  u = { f.y * r.z - f.z * r.y, f.z * r.x - f.x * r.z, f.x * r.y - f.y * r.x };
  float m00 = r.x, m01 = u.x, m02 = f.x, m10 = r.y, m11 = u.y, m12 = f.y, m20 = r.z, m21 = u.z, m22 = f.z, tr = m00 + m11 + m22; Q4 q;
  if (tr > 0) { float k = sqrtf(tr + 1) * 2; q = { (m21 - m12) / k, (m02 - m20) / k, (m10 - m01) / k, 0.25f * k }; }
  else if (m00 > m11 && m00 > m22) { float k = sqrtf(1 + m00 - m11 - m22) * 2; q = { 0.25f * k, (m01 + m10) / k, (m02 + m20) / k, (m21 - m12) / k }; }
  else if (m11 > m22) { float k = sqrtf(1 + m11 - m00 - m22) * 2; q = { (m01 + m10) / k, 0.25f * k, (m12 + m21) / k, (m02 - m20) / k }; }
  else { float k = sqrtf(1 + m22 - m00 - m11) * 2; q = { (m02 + m20) / k, (m12 + m21) / k, 0.25f * k, (m10 - m01) / k }; }
  return q;
}
static DWORD g_nrExpoShot = 0; static float g_nrExpoW = 255, g_nrExpoG = 1;
static void NrCapture(V3 cam, V3 look, int delay, float fov, V3 up = { 0, 1, 0 }) {
  NrApi& a = g_nr;
  Q4 q = NrLookRotUp({ look.x - cam.x, look.y - cam.y, look.z - cam.z }, up); void* pa[2] = { &cam, &q }; Invoke(a.trSetPR, g_nrCamTr, pa);
  void* fa0[1] = { &fov }; Invoke(a.setFov, g_nrCam, fa0);
  void* mc = Invoke(a.camMain, nullptr);                               // follow the game's view distance and layers
  if (mc) { void* fr = Invoke(a.getFar, mc); void* mk = Invoke(a.getMask, mc);
            if (fr) { float f = *(float*)((char*)fr + 0x10); void* fa[1] = { &f }; Invoke(a.setFar, g_nrCam, fa); }
            if (mk) { int m = *(int*)((char*)mk + 0x10); void* ma[1] = { &m }; Invoke(a.setMask, g_nrCam, ma); } }
  Invoke(a.render, g_nrCam);
  void* prev = Invoke(a.rtGetActive, nullptr); void* sa[1] = { g_nrRt }; Invoke(a.rtSetActive, nullptr, sa);
  float r[4] = { 0, 0, (float)kNrW, (float)kNrH }; int z = 0; void* ra[3] = { r, &z, &z }; Invoke(a.readPx, g_nrTex, ra);
  void* pv[1] = { prev }; Invoke(a.rtSetActive, nullptr, pv);
  void* arr = Invoke(a.getRaw, g_nrTex);
  if (!arr) { if (!g_nrCapLogged) { g_nrCapLogged = true; Log("newsreel: readback gave nothing; frame skipped"); } return; }
  size_t len = *(size_t*)((char*)arr + 0x18); const uint8_t* px = (const uint8_t*)arr + 0x20;
  if (len < (size_t)kNrW * kNrH * 4) { if (!g_nrCapLogged) { g_nrCapLogged = true; Log("newsreel: readback gave %zu bytes, expected %d; frame skipped", len, kNrW * kNrH * 4); } return; }
  std::vector<uint8_t> g((size_t)kNrW * kNrH);
  for (int y = 0; y < kNrH; y++) {                                     // the texture's first row is the bottom one
    const uint8_t* s = px + (size_t)(kNrH - 1 - y) * kNrW * 4;
    for (int x = 0; x < kNrW; x++, s += 4) g[(size_t)y * kNrW + x] = (uint8_t)((s[0] * 77 + s[1] * 150 + s[2] * 29) >> 8);
  }
  // Exposure: the 2026-11-15 07:45 night film averaged 1-2 of 255 (the game's own eye adaptation never reaches this camera).
  // The brightest 0.5% go to white and a gamma lifts the darks toward an average near 0.22; held steady within a shot.
  // A normal daylight frame comes out unchanged (white point 255, gamma 1).
  int hist[256] = {}; for (uint8_t v : g) hist[v]++;
  size_t n = g.size(), acc = 0; int wp = 255; double sum = 0;
  for (int v = 255; v > 0; v--) if ((acc += hist[v]) > n / 200) { wp = v; break; }
  for (int v = 0; v < 256; v++) sum += (double)v * hist[v];
  float W = (float)(std::max)(wp, 24), m = (std::min)(0.95f, (float)(sum / n) / W);
  float G = m > 0.002f ? logf(0.22f) / logf(m) : 0.45f; G = (std::max)(0.45f, (std::min)(1.0f, G));
  float dark = (std::max)(0.0f, (std::min)(1.0f, (40 - (float)(sum / n)) / 20));   // full correction at a mean under 20, none over 40 (daylight)
  W = 255 + (W - 255) * dark; G = 1 + (G - 1) * dark;
  if (g_nrExpoShot != g_nrShot.start) { g_nrExpoShot = g_nrShot.start; g_nrExpoW = W; g_nrExpoG = G; }
  else { g_nrExpoW += (W - g_nrExpoW) * 0.15f; g_nrExpoG += (G - g_nrExpoG) * 0.15f; }
  if (g_nrExpoW < 254 || g_nrExpoG < 0.99f) {
    uint8_t lut[256]; for (int v = 0; v < 256; v++) lut[v] = (uint8_t)(255 * powf((std::min)(1.0f, v / g_nrExpoW), g_nrExpoG) + 0.5f);
    for (uint8_t& v : g) v = lut[v];
  }
  g_nrFilm.frames.push_back(std::move(g)); g_nrFilm.delays.push_back(delay);
}

// Every frame from ProjectileManager.LateUpdate.
static float NrDist(V3 a, V3 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
static V3 NrLerp(V3 a, V3 b, float k) { return { a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k }; }
// The boat's frame (origin, forward, right, up), to keep the deck camera's spot fixed on the hull.
struct NrBasis { V3 o, f, r, u; };
static bool NrBoatBasis(void* boat, NrBasis& b) {
  void* tr = Invoke(g_nr.compTr, boat); if (!tr || !NrPos(boat, b.o)) return false;
  b.f = GetV3(m_crewTrFwd, tr); b.r = GetV3(m_crewTrRight, tr); b.u = g_nr.trUp ? GetV3(g_nr.trUp, tr) : V3{ b.f.y * b.r.z - b.f.z * b.r.y, b.f.z * b.r.x - b.f.x * b.r.z, b.f.x * b.r.y - b.f.y * b.r.x };
  return V3Dot(b.f, b.f) > 0.5f && V3Dot(b.r, b.r) > 0.5f && V3Dot(b.u, b.u) > 0.5f;
}
static V3 NrToLocal(const NrBasis& b, V3 w) { V3 d = V3Sub(w, b.o); return { V3Dot(d, b.f), V3Dot(d, b.r), V3Dot(d, b.u) }; }
static V3 NrToWorld(const NrBasis& b, V3 l) {
  return { b.o.x + b.f.x * l.x + b.r.x * l.y + b.u.x * l.z, b.o.y + b.f.y * l.x + b.r.y * l.y + b.u.y * l.z, b.o.z + b.f.z * l.x + b.r.z * l.y + b.u.z * l.z };
}
static bool NrBlocked(V3 a, V3 b) {                                    // Physics.Linecast: something solid between a and b
  int mask = ~((1 << 12) | (1 << 2)), qti = 1;                       // not smoke, not Ignore Raycast; QueryTriggerInteraction.Ignore
  void* args[4] = { &a, &b, &mask, &qti }; void* r = Invoke(g_nr.linecast, nullptr, args); return r && *(bool*)((char*)r + 0x10);
}
// A spot to stand on deck: a clear view of the man both ways (a camera inside the cabin would see out through its back
// faces, so the man must see the camera too), stopping 0.5 m short of him (his own colliders), and deck under foot.
// The Collider a Linecast hit (or (void*)1 when it cannot be named), null when the line is clear; at = the hit point.
// David 2026-11-15: Tipper on the 6-pdr had all 15 spots "something in the way" while his loader was filmed fine.
static void* NrHitWhat(V3 a, V3 b, V3& at) {
  alignas(16) unsigned char box[0x10 + 128] = {}; unsigned char* hit = box + 0x10;   // a fake boxed RaycastHit: invoke unboxes at +0x10
  int mask = ~((1 << 12) | (1 << 2)), qti = 1;
  void* args[5] = { &a, &b, hit, &mask, &qti };
  void* r = Invoke(g_nr.linecastHit, nullptr, args); if (!r || !*(bool*)((char*)r + 0x10)) return nullptr;
  at = *(V3*)hit;                                                      // RaycastHit.m_Point
  void* c = Invoke(g_nr.hitCollider, box, nullptr); return c ? c : (void*)1;
}
// Blocked from a to b, looking through parts of skipTr (a gunner's own gun: in the foreground of his shot, fine); what = the blocker.
// Going out (back = false) the parts looked through are kept in seen; coming back only those may be looked through again, so a
// gun part between camera and man (hit both ways) is fine but a camera INSIDE the gun (hit only coming back) is not (review).
static bool NrBlockedPast(V3 a, V3 b, void* skipTr, std::string* what, std::vector<void*>& seen, bool back) {
  if (!g_nr.linecastHit || !g_nr.hitCollider) return NrBlocked(a, b);
  for (int i = 0; i < 5; i++) {
    V3 at; void* c = NrHitWhat(a, b, at); if (!c) return false;
    void* ct = c != (void*)1 ? Invoke(g_nr.compTr, c) : nullptr; void* arg[1] = { skipTr };
    void* r = ct && skipTr && g_nr.trIsChild ? Invoke(g_nr.trIsChild, ct, arg) : nullptr;
    bool part = r && *(bool*)((char*)r + 0x10) && (!back || std::find(seen.begin(), seen.end(), c) != seen.end());
    if (!part) { if (what) *what = ct ? CrewClean(NameOf(c)) : std::string("?"); return true; }
    if (!back) seen.push_back(c);
    V3 d = V3Sub(b, at); float l = sqrtf(V3Dot(d, d)); if (l < 0.1f) return false;
    a = { at.x + d.x / l * 0.05f, at.y + d.y / l * 0.05f, at.z + d.z / l * 0.05f };   // a ray starting inside a collider does not hit it
  }
  return true;
}
// 0 clear, 1 too close, 2 something in the way, 3 seen only from his side (camera inside something), 4 no deck under it.
static int NrSpotWhy(V3 cam, V3 at, void* skipTr = nullptr, std::string* what = nullptr, bool* through = nullptr) {   // through: clear only past his gun
  std::vector<void*> seen; if (through) *through = false;
  if (!g_nr.linecast) return 0;
  V3 d = V3Sub(at, cam); float L = sqrtf(V3Dot(d, d)); if (L < 1.2f) return 1;
  V3 nearP = { cam.x + d.x * (L - 0.5f) / L, cam.y + d.y * (L - 0.5f) / L, cam.z + d.z * (L - 0.5f) / L };
  if (NrBlockedPast(cam, nearP, skipTr, what, seen, false)) return 2;
  if (NrBlockedPast(nearP, cam, skipTr, what, seen, true)) return 3;
  if (through) *through = !seen.empty();
  return NrBlocked(cam, { cam.x, cam.y - 2.4f, cam.z }) ? 0 : 4;
}
// Where the camera person stands to film this man, in the boat's frame: just in front of a crewmate 2-8 m away (a spot the
// crew walk, so it is deck), else a point on deck fore or aft of the man at eye height, inside the hull's length. The best
// clear spot wins: about 4 m off, behind a gunner (his gun and what he fires at in view), in front of anyone else (faces).
static bool NrDeckSpot(void* me, const CrewRec& who, int role, V3& camL) {
  NrBasis bb; if (!NrBoatBasis(me, bb)) return false;
  V3 H = *(V3*)((char*)who.member + g_oCmHeadP), C = *(V3*)((char*)who.member + g_oCmChestP), F = *(V3*)((char*)who.member + g_oCmFeet);
  if (NrDist(H, C) > 1.5f || NrDist(H, F) > 2.6f) return false;
  V3 fw = GetV3(m_crewTrFwd, Invoke(g_nr.compTr, who.member));
  struct Cand { float score; V3 w; const char* from; };
  std::vector<Cand> cands;
  auto add = [&](V3 w, float bonus, const char* from) {
    V3 d = V3Sub(w, H); float hd = sqrtf(d.x * d.x + d.z * d.z);
    if (hd < 2 || hd > 8) return;
    float score = fabsf(hd - 4) - bonus, ahead = d.x * fw.x + d.z * fw.z;
    if (role == 4 ? ahead > 0 : ahead < -0.5f) score += role == 4 ? 3 : 1.5f;
    cands.push_back({ score, w, from });
  };
  for (CrewRec& r : g_crewRecs) {
    if (&r == &who || !NrCrewUp(r)) continue;
    int rr = *(int*)((char*)r.member + g_oCmRole); if (rr != 4 && rr != 5) continue;   // runners walk off and through the lens (review)
    V3 h = *(V3*)((char*)r.member + g_oCmHeadP), dh = V3Sub(H, h); float l = sqrtf(dh.x * dh.x + dh.z * dh.z);
    if (l > 0.1f && NrDist(h, *(V3*)((char*)r.member + g_oCmFeet)) < 2.6f)
      add({ h.x + dh.x / l * 0.65f, h.y + 0.05f, h.z + dh.z / l * 0.65f }, 1, "by a crewmate");   // a step in front of him, toward the man
  }
  float fwl = sqrtf(fw.x * fw.x + fw.z * fw.z);
  if (role == 4 && fwl > 0.2f) {                                       // behind a gunner and a little above, straight or to either shoulder
    V3 f = { fw.x / fwl, 0, fw.z / fwl }, sd = { f.z, 0, -f.x };
    for (float back : { 2.5f, 3.5f })
      for (float side : { 0.0f, 0.6f, -0.6f })
        add({ H.x - f.x * back + sd.x * back * side, H.y + 0.4f, H.z - f.z * back + sd.z * back * side }, 1.5f, "over his shoulder");
  }
  if (g_nr.linecast) {                                                 // open deck only where Linecast can prove it
    float half = EntitySize(me) * 0.5f; if (half < 6) half = 6;
    V3 fl = NrToLocal(bb, F);
    for (float along : { -4.0f, 4.0f, -6.0f, 6.0f, -3.0f, 3.0f })
      for (float across : { 0.0f, 0.5f })
        if (fabsf(fl.x + along) < half - 2) add(NrToWorld(bb, { fl.x + along, fl.y * across, fl.z + 1.6f }), 0, "on open deck");
  }
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
  int tried = 0, why[5] = {}; std::map<std::string, int> blockers;
  void* gun = role == 4 ? NrGunOf(who.member) : nullptr; void* gunTr = gun ? Invoke(g_nr.compTr, gun) : nullptr;
  for (const Cand& c : cands) {
    if (++tried > 18) break;
    std::string what; bool through = false, throughHead = false;
    int w = NrSpotWhy(c.w, C, gunTr, &what, &through);
    if (w && role == 4) { V3 Hu = { H.x, H.y + 0.15f, H.z }; if (NrSpotWhy(c.w, Hu, gunTr, nullptr, &throughHead) == 0) { w = 0; through = throughHead; } }   // a gun shield hides his chest, not his head
    if (w) { why[w]++; if ((w == 2 || w == 3) && !what.empty()) blockers[what]++; continue; }
    camL = NrToLocal(bb, c.w);
    DLog("newsreel: filming '%s' from %s, %.1f m off (%d of %d spots tried%s)", who.name.c_str(), c.from, NrDist(c.w, H), tried, (int)cands.size(),
        through ? ", looking past his own gun" : "");
    return true;
  }
  std::string bl; for (auto& b : blockers) bl += (bl.empty() ? "" : ", ") + b.first + " x" + std::to_string(b.second);
  DLog("newsreel: no clear spot on deck to film '%s' from (%d spots tried: %d too close, %d something in the way, %d from inside something, %d no deck under)%s%s%s%s",
      who.name.c_str(), (std::min)(tried, (int)cands.size()), why[1], why[2], why[3], why[4], bl.empty() ? "" : "; blocked by ", bl.c_str(),
      gun ? "; his gun mount is " : "", gun ? CrewClean(NameOf(gun)).c_str() : "");
  return false;
}
// Behind a walking man (David 2026-11-15: "follow the ammo runner from behind"): 2.4 m back and a little above his head,
// closer when something is in the way; the line is checked from a point just behind his head (his own colliders).
static bool NrFollowSpot(const NrBasis& bb, V3 H, V3 fdir, V3& camL) {
  V3 Hb = { H.x - fdir.x * 0.4f, H.y, H.z - fdir.z * 0.4f };
  for (float back : { 2.4f, 1.8f, 1.3f }) {
    V3 w = { H.x - fdir.x * back + bb.u.x * 0.35f, H.y + bb.u.y * 0.35f, H.z - fdir.z * back + bb.u.z * 0.35f };
    if (!g_nr.linecast || (!NrBlocked(Hb, w) && !NrBlocked(w, Hb) && NrBlocked(w, { w.x, w.y - 2.4f, w.z }))) { camL = NrToLocal(bb, w); return true; }   // deck under it
  }
  return false;
}
static V3 NrCrewFacing(void* member) {                                 // flat facing of a crew member, {0,0,0} if unknown
  V3 f = GetV3(m_crewTrFwd, Invoke(g_nr.compTr, member)); float fl = sqrtf(f.x * f.x + f.z * f.z);
  return fl > 0.2f ? V3{ f.x / fl, 0, f.z / fl } : V3{ 0, 0, 0 };
}
static void NewsreelTick() {
  if (!g_nr.ok) return;
  void* eng = EngagementInstance();
  if (eng != g_nrEng) { NrFinishBattle(); g_nrEng = eng; }
  if (!g_newsreelOn) {                                                 // off: forget what it was watching, so switching it back on
    if (g_nrShot.ent) NrEndShot("the newsreel was switched off");    // mid-battle does not film a ship that sank meanwhile (review)
    NrDropPending("the newsreel was switched off"); g_nrShipsPrev.clear(); g_nrPlanes.clear(); g_nrHist.clear(); return;
  }   // drops.ini newsreel=0 / Mod Options; the film so far is still written at the battle's end
  if (!eng || GameTimeScale() < 0.01f) return;                         // no battle, or paused
  DWORD now = GetTickCount();
  void* me = PlayerCraft(); V3 boat; if (!me || !NrPos(me, boat)) return;
  int myTeam = *(int*)((char*)me + g_nr.leTeam);

  // Planes: one close by within 600 m (the first test's dive-state trigger filmed nothing in a battle with planes), or one
  // starting to go down within 3 km (most are shot down before they reach the boat, David 2026-11-14).
  void* closeP = nullptr; float closePD = kNrSwoopR; V3 closePP{};
  void* fall = nullptr; void* fallPl = nullptr; float fallD = 0; V3 fallP{}; int fallScore = -1;
  float ts = (std::max)(GameTimeScale(), 0.05f);
  for (auto it = g_nrPlanes.begin(); it != g_nrPlanes.end();) {
    if (now - it->second > 1000) { g_nrHist.erase(it->first); it = g_nrPlanes.erase(it); continue; }
    void* ctrl = it->first; ++it; g_nrSeenAll.insert(ctrl);
    void* pl = *(void**)((char*)ctrl + g_nr.ctrlPlane);
    if (!UnityAlive(pl) || *(int*)((char*)pl + g_nr.leTeam) == myTeam) continue;
    int st = *(int*)((char*)ctrl + g_nr.flight);
    V3 p; if (!NrPos(pl, p)) continue;
    float d = NrDist(p, boat);
    NrPlaneHist& h = g_nrHist[ctrl];
    if (st == 6 || !*(bool*)((char*)pl + g_nr.leAlive)) {             // going down: the best-scoring one within 2.5 km is the candidate;
      if (!h.down) { h.down = true; h.downAt = now; h.downY = p.y; h.downD = d; h.downClosing = h.closing; }   // one that falls while the
      if (g_nrDownSeen.count(ctrl) || ctrl == g_nrShot.ctrl) continue;   // camera is busy waits for it
      if (h.downD >= kNrFallR) continue;                               // far off when it went down: never a candidate, not counted
      if (p.y <= 5 || (p.y < kNrFallMinY && d >= kNrFallNear && g_nrDowned > 0)) {   // the first fall of a battle may be low (night: 1 fall)
        g_nrDownSeen.insert(ctrl); g_nrFallsSkipped++;
        DLog("newsreel: '%s' going down at %.0f m not filmed (%s, now %.0f m up)", CrewClean(NameOf(pl)).c_str(), d,
                                    h.downY >= kNrFallMinY || h.downD < kNrFallNear ? "went under 25 m while waiting for the camera" : "only a splash", p.y);
        continue;
      }
      int sc = NrFallScore(h).total; if (sc > fallScore) { fallScore = sc; fall = ctrl; fallPl = pl; fallD = d; fallP = p; }
      continue;
    }
    if (h.down) h = NrPlaneHist();                                     // a pooled controller flying again is a new plane
    if (st == 2 || st == 3) h.diveAt = now;                            // diving or breaking off its attack
    if (!h.prevAt) { h.prevD = d; h.prevAt = now; }
    else if (now - h.prevAt >= 250) {                                  // closing speed in game m/s (the kill cam slows time)
      float c = (h.prevD - d) * 1000.0f / (now - h.prevAt) / ts; h.closing += (c - h.closing) * 0.4f; h.prevD = d; h.prevAt = now;
    }
    g_nrDownSeen.erase(ctrl);                                          // flying again: a pooled controller is a new plane
    g_nrSeenEnemy.insert(ctrl);
    if (st >= 0 && st < 8) g_nrStateFrames[st]++;
    if (d < g_nrClosestAny) g_nrClosestAny = d;
    if (st == 2 && d < g_nrClosestDive) g_nrClosestDive = d;
    bool swoop = (d < kNrSwoopR && h.diveAt && now - h.diveAt < kNrSwoopDiveMs && h.closing > kNrSwoopClosing) || (d < kNrLowPassR && p.y < kNrLowPassY);   // diving at us, or a low near pass
    if (swoop && g_nrSwoops < kNrMaxSwoops && d < closePD && !g_nrPlaneFilmed.count(ctrl)) { closePD = d; closeP = ctrl; closePP = p; }
  }

  // judged once, never offered again (the next best next tick); one going down within 500 m always passes while under the cap
  if (fall && (g_nrDowned >= kNrMaxFalls || (fallScore < kNrFallBar[g_nrDowned] && g_nrHist[fall].downD >= kNrFallNear))) {
    const NrPlaneHist& h = g_nrHist[fall]; NrFallParts sp = NrFallScore(h); char need[40];
    if (g_nrDowned >= kNrMaxFalls) snprintf(need, sizeof need, "%d falls already filmed", kNrMaxFalls); else snprintf(need, sizeof need, "needed %d", kNrFallBar[g_nrDowned]);
    DLog("newsreel: '%s' going down at %.0f m not filmed (score %d = distance %d + height %d + dive %d + closing %d, %s; went down %.0f m up)",
        CrewClean(NameOf(fallPl)).c_str(), h.downD, sp.total, sp.dist, sp.height, sp.dive, sp.closing, need, h.downY);
    g_nrDownSeen.insert(fall); g_nrFallsSkipped++; fall = nullptr;
  }

  // Ships (David 2026-11-14: follow boat attacks too): an enemy ship that drops off the live list dead within 3 km is
  // sinking; an enemy ship within 300 m gets one close-by shot per battle.
  void* sink = nullptr; float sinkD = 3000; V3 sinkP{};
  void* closeS = nullptr; float closeSD = 300; V3 closeSP{};
  std::unordered_map<void*, V3> ships;
  for (const EnemyAt& e : LiveEnemies()) {
    if (!e.ship) continue;
    ships[e.ent] = e.pos;
    g_nrSunk.erase(e.ent);                                             // alive: a new ship at a reused address, not the sunk one
    float d = NrDist(e.pos, boat);
    if (d < closeSD && !g_nrShipFilmed.count(e.ent)) { closeSD = d; closeS = e.ent; closeSP = e.pos; }
  }
  for (auto& kv : g_nrShipsPrev) {
    if (ships.count(kv.first)) continue;
    void* e = kv.first;
    if (!UnityAlive(e) || g_offLeAlive < 0 || *(bool*)((char*)e + g_offLeAlive) || !g_nrSunk.insert(e).second) continue;   // gone, or alive (submerged)
    float d = NrDist(kv.second, boat);
    if (kv.second.y < -8) continue;                                    // already under: the 19:46 film held on empty sea
    float sz = EntitySize(e);
    if (sz > 0 && sz < kNrSmallBoat && g_nrSmallSinks >= kNrMaxSmallSinks) continue;   // g_nrSunk keeps it from coming back
    if (d < sinkD){ sinkD = d; sink = e; sinkP = kv.second; }
  }
  g_nrShipsPrev.swap(ships);
  if (sink && g_nrShot.ent && g_nrShot.downAt) {                      // busy with a fall or sinking: film it next
    if (g_nrPendSink) DLog("newsreel: '%s' sank while the camera was busy and one sinking is already queued; not filmed", CrewClean(NameOf(sink)).c_str());
    else { g_nrPendSink = sink; g_nrPendSinkH = Pin(sink); g_nrPendSinkP = sinkP; g_nrPendSinkAt = now; }
    sink = nullptr;
  }
  if (!sink && g_nrPendSink && !g_nrShot.ent) {                       // waits up to 4 s after the busy shot ends (it can sink for a while)
    float psz = UnityAlive(g_nrPendSink) ? EntitySize(g_nrPendSink) : 0; V3 pp = g_nrPendSinkP;
    if (now - (std::max)(g_nrPendSinkAt, g_nrLastEnd) >= 4000 || !UnityAlive(g_nrPendSink) || !NrPos(g_nrPendSink, pp)) NrDropPending("it was gone or waited too long");
    else if (pp.y < -6) NrDropPending("it went under while queued");   // review 2026-11-14: queued ships were never re-checked
    else if (psz > 0 && psz < kNrSmallBoat && g_nrSmallSinks >= kNrMaxSmallSinks) NrDropPending("small-boat sinkings already filmed");
    else {
      sink = g_nrPendSink; sinkP = pp; sinkD = NrDist(sinkP, boat);
      Unpin(g_nrPendSinkH); g_nrPendSinkH = nullptr; g_nrPendSink = nullptr;   // the shot pins it from here
    }
  }

  // A fall or a sinking cuts a close-by shot short; then pick the next shot, most important first.
  if (g_nrShot.ent && (fall || sink) && (g_nrShot.kind == kNrShipClose || g_nrShot.kind == kNrDeck || (sink && g_nrShot.kind == kNrPlaneClose)) && !g_nrShot.downAt)
    NrEndShot(fall ? "cut to a plane going down" : "cut to a ship sinking");
  if (!g_nrCombatAt && NrInCombat(now)) g_nrCombatAt = now;            // the first of our guns firing this battle
  if (!g_nrShot.ent) {
    // less than a second left: a new shot would be just its splice (22:30 film)
    if ((int)g_nrFilm.frames.size() >= kNrMaxFrames - 20) { if (!g_nrFullLogged) { g_nrFullLogged = true; DLog("newsreel: the film is full (%d frames); no more shots this battle", kNrMaxFrames); } return; }
    int kind; void* ent; void* ctrl = nullptr; float d; V3 p; CrewRec* crew = nullptr; V3 deckL{}; bool follow = false;
    if (fall) { kind = kNrPlaneFall; ctrl = fall; ent = *(void**)((char*)fall + g_nr.ctrlPlane); d = fallD; p = fallP; }
    else if (sink) { kind = kNrShipSink; ent = sink; d = sinkD; p = sinkP; }
    else if (g_nrLastEnd && now - g_nrLastEnd < 3000) return;           // close-by and deck shots keep a 3 s gap
    else if (closeP) { kind = kNrPlaneClose; ctrl = closeP; ent = *(void**)((char*)closeP + g_nr.ctrlPlane); d = closePD; p = closePP; }
    else if (closeS) { kind = kNrShipClose; ent = closeS; d = closeSD; p = closeSP; }
    else if (g_nrDeckShots < kNrMaxDeck && NrInCombat(now) &&   // crew at work: our guns firing
             (!g_nrLastDeck || now - g_nrLastDeck > 15000) && NrCrewOk() &&
             (crew = NrPickCrew(now, (std::max)((std::max)(g_nrCombatAt, g_nrLastDeck ? g_nrLastDeck + 15000 : 0UL), g_nrLastEnd + 3000))) != nullptr && NrPos(crew->member, p)) {
      int role = *(int*)((char*)crew->member + g_oCmRole); V3 fd = NrCrewFacing(crew->member); NrBasis fb;
      if (role == 3 && (fd.x != 0 || fd.z != 0) && NrBoatBasis(me, fb) && NrFollowSpot(fb, *(V3*)((char*)crew->member + g_oCmHeadP), fd, deckL)) {
        follow = true; DLog("newsreel: following '%s' from behind", crew->name.c_str());
      }
      else if (!NrDeckSpot(me, *crew, role, deckL)) {                   // nowhere clear to stand: someone else next time
        g_nrDeckFilmed.insert(crew->ch); g_nrLastDeck = now - 10000;   // next try in 5 s (review 2026-11-14)
        DLog("newsreel: '%s' not filmed", crew->name.c_str()); return;
      }
      kind = kNrDeck; ent = crew->member; d = NrDist(p, boat);
    }
    else return;
    if (!NrMakeCamera()) return;
    if (!g_nrFilm.frames.empty()) for (int i = 0; i < 3; i++) { g_nrFilm.frames.push_back(std::vector<uint8_t>((size_t)kNrW * kNrH, 16)); g_nrFilm.delays.push_back(8); }   // a splice
    g_nrFilm.w = kNrW; g_nrFilm.h = kNrH;
    NrShot& n = g_nrShot;
    n.ent = ent; n.ctrl = ctrl; n.kind = kind; n.h = Pin(ctrl ? ctrl : ent); n.start = n.lastDive = n.lastSeen = now; n.lastP = p;
    n.name = CrewClean(NameOf(ent));
    if (kind == kNrPlaneFall) { n.downAt = now; g_nrDowned++; g_nrDownSeen.insert(ctrl); }
    if (kind == kNrPlaneClose) { g_nrPlaneFilmed.insert(ctrl); g_nrSwoops++; }
    if (kind == kNrShipSink) { n.downAt = now; g_nrSinks++; n.orbit0 = atan2f(boat.z - p.z, boat.x - p.x); n.size = EntitySize(ent);
                               if (n.size > 0 && n.size < kNrSmallBoat) g_nrSmallSinks++; }
    if (kind == kNrShipClose) g_nrShipFilmed.insert(ent);
    if (kind == kNrDeck) {
      n.ch = crew->ch; n.role = *(int*)((char*)ent + g_oCmRole); n.name = crew->name; g_nrDeckFilmed.insert(crew->ch); g_nrDeckShots++; g_nrLastDeck = now; n.camL = deckL; n.follow = follow;
      n.gunAt = n.role == 4 ? NrGunShotAt(ent) : 0;
    }
    g_nrShots++;
    char extra[128] = "";
    if (ctrl) {
      const NrPlaneHist& h = g_nrHist[ctrl]; int st = *(int*)((char*)ctrl + g_nr.flight);
      if (kind == kNrPlaneFall) { NrFallParts sp = NrFallScore(h); snprintf(extra, sizeof extra, ", flight state %d, score %d = %d+%d+%d+%d, went down %.0f m up", st, sp.total, sp.dist, sp.height, sp.dive, sp.closing, h.downY); }
      else snprintf(extra, sizeof extra, ", flight state %d, closing %.0f m/s, %s", st, h.closing, h.diveAt && now - h.diveAt < kNrSwoopDiveMs ? "diving at us" : "low pass");
    }
    else if (kind == kNrDeck) snprintf(extra, sizeof extra, ", %s%s", crew->role.c_str(), follow ? ", followed" : "");
    else if (n.size > 0) snprintf(extra, sizeof extra, ", %.0f m long%s", n.size, n.size < kNrSmallBoat ? ", small boat" : "");
    DLog("newsreel: shot %d: '%s' %s at %.0f m (height %.0f m%s)", g_nrShots, n.name.c_str(), kNrKindName[kind], d, p.y, extra);
  }

  NrShot& s = g_nrShot;
  bool plane = s.ctrl != nullptr;
  V3 p = s.lastP, q;
  bool read = !s.gone && UnityAlive(s.ent) && NrPos(s.ent, q);
  float sdt = (now - s.lastSeen) / 1000.0f;
  if (read && plane && s.downAt && NrDist(q, s.lastP) > 300 + 250 * sdt) read = false;   // a pooled plane jumps away: treat as gone
  if (!read) {
    if (!s.downAt || !plane) { NrEndShot(plane ? "the plane is gone" : s.kind == kNrDeck ? "they are gone" : "the ship is gone"); return; }   // a gone ship left empty sea (20:21 film)
    if (!s.gone) { s.gone = true; DLog("newsreel: '%s' left the scene %.1f s in; holding on the spot", s.name.c_str(), (now - s.start) / 1000.0); }
  }
  else {
    p = q;
    if (plane && !s.downAt) {
      int st = *(int*)((char*)s.ctrl + g_nr.flight);
      if (st == 6 || !*(bool*)((char*)s.ent + g_nr.leAlive)) { s.downAt = now; g_nrDowned++; g_nrDownSeen.insert(s.ctrl); DLog("newsreel: '%s' is going down on film", s.name.c_str()); }
    }
    if (sdt > 0.5f) { s.lastP = p; s.lastSeen = now; }                // after a hitch or pause: restart the velocity, no blend
    else if (sdt > 0.02f) {                                            // velocity from the last two reads (real time), smoothed
      V3 v = { (p.x - s.lastP.x) / sdt, (p.y - s.lastP.y) / sdt, (p.z - s.lastP.z) / sdt };
      s.vel = s.velOk ? NrLerp(s.vel, v, 0.4f) : v; s.velOk = true;
      float hv = sqrtf(s.vel.x * s.vel.x + s.vel.z * s.vel.z);
      if (hv > 3 && s.kind != kNrDeck) s.fdir = { s.vel.x / hv, 0, s.vel.z / hv };   // last good heading, kept through the splash (a deck shot's
                                                                       // fdir is the man's facing, not the boat's course: review 2026-11-14)
      s.lastP = p; s.lastSeen = now;
    }
  }
  float dx = p.x - boat.x, dz = p.z - boat.z, hd = sqrtf(dx * dx + dz * dz), d = NrDist(p, boat);
  if (d < s.closest) s.closest = d;
  if (now - s.start > 9000) { NrEndShot("9 s"); return; }
  if (s.kind == kNrShipSink && now - s.start > kNrSinkMs) { NrEndShot("sinking filmed"); return; }
  if (s.kind == kNrShipSink && p.y < -6 && now - s.start > 1500) { NrEndShot("it went under"); return; }
  if (s.kind == kNrShipClose && (now - s.start > kNrCloseShipMs || d > 450)) { NrEndShot(d > 450 ? "it moved off" : "6 s"); return; }
  if (plane && s.downAt && now - s.downAt > kNrFallMs) { NrEndShot("shot down"); return; }
  if (s.kind == kNrDeck) {                                             // a man killed on film stays in the shot; one overboard does not
    CrewRec* r = NrCrewRecOf(s.ch);
    if (!r) { NrEndShot("their crew record is gone"); return; }
    if (r->state == 3) { NrEndShot("went over the side"); return; }
    if (s.role == 4) {
      DWORD at = NrGunShotAt(s.ent); if (at && at != s.gunAt) { s.gunAt = at; s.rounds++; }
      char why[64] = "";
      if (now - s.start > kNrGunMaxMs) snprintf(why, sizeof why, "6 s, %d rounds fired on film", s.rounds);
      else if (now - s.start > kNrGunMinMs && (!at || now - at > kNrGunFireMs)) snprintf(why, sizeof why, "the gun stopped firing, %d rounds on film", s.rounds);
      if (why[0]) { NrEndShot(why); return; }
    }
    else if (now - s.start > kNrDeckMs) { NrEndShot("4 s"); return; }
  }
  if (d < 1100) s.lastDive = now;                                      // lastDive: last time the plane was close (a swoop starts up to 1 km off)
  if (s.kind == kNrPlaneClose && !s.downAt && d > s.closest + 300) { NrEndShot("it pulled away"); return; }
  if (s.kind == kNrPlaneClose && !s.downAt && now - s.start > 4000 && s.closest > 800) { NrEndShot("it stayed far off"); return; }   // no 9 s of dots (07:56 film)
  if (plane && !s.downAt && now - s.lastDive > 1500) { NrEndShot("it flew off"); return; }
  if ((int)g_nrFilm.frames.size() >= kNrMaxFrames) { NrEndShot("the film is full"); return; }
  if (s.lastCap && now - s.lastCap < 50) return;
  if (plane && s.downAt && !s.velOk && !s.gone && now - s.start < 400) return;   // chase: wait for the plane's heading (review 2026-11-14)
  int delay = s.lastCap ? (int)(std::min)(20UL, (std::max)(3UL, (now - s.lastCap + 5) / 10)) : 5;
  s.lastCap = now;
  V3 cam, want, camUp = { 0, 1, 0 }; float manR = 0;
  if (plane && s.downAt) {
    // Chase camera for a falling plane: behind it along its ground track, off to the side and above, so the horizon,
    // the smoke trail and the splash stay in frame (the first test's camera stayed put and ended up staring at the sea).
    bool haveDir = s.fdir.x != 0 || s.fdir.z != 0;                    // its last good heading; none yet: face it from the boat's side
    V3 f = haveDir ? s.fdir : (hd > 1 ? V3{ -dx / hd, 0, -dz / hd } : V3{ 0, 0, 1 });
    V3 side = { f.z, 0, -f.x };
    // Not position-smoothed: a lagging camera fell ~40 m behind a 100 m/s plane (19:46 film). The heading is smoothed already.
    cam = { p.x - f.x * 45 + side.x * 25, (std::max)(p.y + 14, 6.0f), p.z - f.z * 45 + side.z * 25 };
    want = s.gone ? p : V3{ p.x + s.vel.x * 0.15f, p.y + s.vel.y * 0.15f, p.z + s.vel.z * 0.15f };   // a little ahead of it
  }
  else if (s.kind == kNrDeck) {
    // A camera person standing on our deck (David 2026-11-14: "like the camera man is on the boat"): the spot picked at the
    // start stays put in the boat's frame, so it rolls and runs with the hull; he turns to keep the man in frame, a slight
    // sway as if hand-held. A gunner is watched past his shoulder toward where his gun points.
    NrBasis bb; if (!NrBoatBasis(me, bb)) { NrEndShot("the boat is gone"); return; }
    V3 spot = NrToWorld(bb, s.camL);
    for (CrewRec& r : g_crewRecs)                                      // a crewmate walked into the lens: cut
      if (r.ch != s.ch && NrCrewUp(r) && (NrDist(*(V3*)((char*)r.member + g_oCmHeadP), spot) < 0.45f || NrDist(*(V3*)((char*)r.member + g_oCmChestP), spot) < 0.45f)) {
        NrEndShot("someone walked into the lens"); return; }
    V3 H = *(V3*)((char*)s.ent + g_oCmHeadP), C = *(V3*)((char*)s.ent + g_oCmChestP);
    if (NrDist(H, p) > 4 || NrDist(H, C) > 1.5f) { H = { p.x, p.y + 1.6f, p.z }; C = { p.x, p.y + 1.2f, p.z }; }   // smoothed points not set up yet
    float t = (now - s.start) / 1000.0f;
    want = NrLerp(C, H, 0.4f);
    if (s.role == 4 || s.follow) {
      V3 f = NrCrewFacing(s.ent); bool had = s.fdir.x != 0 || s.fdir.z != 0;
      if (f.x != 0 || f.z != 0) s.fdir = had ? NrLerp(s.fdir, f, s.follow ? 0.15f : 0.3f) : f;   // smoothed: a gun swinging round, a man turning
      float fl = sqrtf(s.fdir.x * s.fdir.x + s.fdir.z * s.fdir.z); if (fl > 0.3f) s.fdir = { s.fdir.x / fl, 0, s.fdir.z / fl }; else if (f.x != 0 || f.z != 0) s.fdir = f;   // turned about: snap
    }
    if (s.follow && (s.fdir.x != 0 || s.fdir.z != 0)) {                // walk behind him; the spot eases along in the boat's frame
      V3 tl; if (NrFollowSpot(bb, H, s.fdir, tl)) s.camL = NrLerp(s.camL, tl, 0.25f);
      want = { want.x + s.fdir.x * 1.2f, want.y, want.z + s.fdir.z * 1.2f };   // over his shoulder, where he is going
    }
    else if (s.role == 4 && (s.fdir.x != 0 || s.fdir.z != 0)) {        // along his gun, as far as the camera stands behind him (from his side he would leave the frame)
      V3 c0 = NrToWorld(bb, s.camL); float bx = H.x - c0.x, bz = H.z - c0.z, bl = sqrtf(bx * bx + bz * bz);
      float k = bl > 0.1f ? (std::max)(0.0f, (std::min)(1.0f, (bx * s.fdir.x + bz * s.fdir.z) / bl)) : 0;
      want = { want.x + s.fdir.x * 2.5f * k, want.y, want.z + s.fdir.z * 2.5f * k };
    }
    manR = NrDist(NrToWorld(bb, s.camL), NrLerp(C, H, 0.4f));
    float bob = s.follow ? 0.06f * fabsf(sinf(t * 5.5f)) : 0;          // footsteps
    cam = NrToWorld(bb, { s.camL.x, s.camL.y + 0.04f * sinf(t * 1.3f + 1), s.camL.z + 0.03f * sinf(t * 2.1f) + bob });
    camUp = bb.u;
  }
  else if (s.kind == kNrShipSink) {
    // A slow orbit around the sinking ship, starting on the boat's side. Distance from its size so a 15 m landing boat is
    // not a few pixels (20:21 film): about 48 m out and 13 m up for that, up to 110 m out and 28 m up; unknown size = the most.
    float R = s.size > 0 ? (std::min)(110.0f, (std::max)(30.0f, s.size * 2.2f + 15)) : 110, up = R * 0.25f;
    float a = s.orbit0 + (now - s.start) / 1000.0f * 0.2f;
    cam = { p.x + cosf(a) * R, (std::max)(p.y, 0.0f) + up, p.z + sinf(a) * R };
    want = { p.x, p.y + 2, p.z };
  }
  else {
    // A low camera on the boat, a few metres on the far side from the plane or ship, looking at it over the deck.
    V3 dir = hd > 1 ? V3{ dx / hd, 0, dz / hd } : V3{ 0, 0, 1 };
    cam = { boat.x - dir.x * 8, boat.y + 4, boat.z - dir.z * 8 };
    want = NrLerp(boat, p, 0.9f); if (!plane) want.y = p.y + 3;
    NrBasis cb; if (NrBoatBasis(me, cb)) camUp = cb.u;                 // standing on our deck: rolls with it
  }
  if (cam.y < 6 && s.kind != kNrPlaneClose && s.kind != kNrShipClose && s.kind != kNrDeck) cam.y = 6;   // never under the waves
  // Deck: no world-space smoothing, the boat moves ~1 m between captures and the man drifted off centre (review 2026-11-14).
  if (!s.lookSet || s.kind == kNrDeck) { s.look = want; s.lookSet = true; } else s.look = NrLerp(s.look, want, 0.5f);
  // Zoom: the deck camera narrows its view for a far plane or ship so it is more than a few pixels (review 2026-11-14).
  float fov = 55;
  if (s.kind == kNrDeck) { float r = manR > 0 ? manR : NrDist(cam, s.look); fov = (std::max)(s.role == 4 ? 48.0f : 34.0f, (std::min)(65.0f, 2 * atanf(1.9f / (std::max)(r, 0.5f)) * 57.2958f)); }   // the man about half the frame tall; a gunner with his gun
  if (s.kind == kNrPlaneClose || s.kind == kNrShipClose) {
    float r = NrDist(cam, p); if (r > 1) fov = 2 * atanf((plane ? 25.0f : 45.0f) / r) * 57.2958f;
    fov = (std::max)(plane ? 5.0f : 14.0f, (std::min)(55.0f, fov));   // vertical fov: 5 deg shows a 12 m plane at 900 m about 55 px tall/wide
  }
  NrCapture(cam, s.look, delay, fov, camUp);
}
static void SafeNewsreelCleanup() {                                    // after a fault: free the shot and the film (up to ~35 MB)
  __try { Unpin(g_nrShot.h); NrDropPending(nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void NrForget() { g_nrShot = NrShot(); g_nrFilm = GifFrames(); }
static void SafeNewsreelTick() {
  __try { NewsreelTick(); }
  __except (FaultFilter("newsreel", GetExceptionInformation())) { g_nrFaults = -1; g_nr.ok = false; SafeNewsreelCleanup(); NrForget(); }
}
