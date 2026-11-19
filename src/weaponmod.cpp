// BoatCrewWeaponMod: extra guns that live in the base customization screen and survive save and load.
//
// Built as version.dll and placed next to "Boat Crew.exe" (UnityPlayer imports version.dll, so the game loads this
// copy, which forwards the real exports and starts one thread). Everything is found by name through the IL2CPP C API.
//
// How it works (all of it read out of the game's disassembly, see the notes in the repo):
//  * A loadout save stores, per weapon slot, only the POSITION of the chosen gun in WeaponSystem.mounts
//    (WeaponManager.setParameters reads parameters named SYSTEM_<n> and calls select(position)).
//  * The base screen builds its list from the slot's SelectionOrder asset (entries of name + mount position), not
//    from the mounts themselves.
//  * WeaponSystem.Awake fills in nothing itself, it only reads the serialized mounts. So right AFTER Awake we append
//    the extra guns from BoatCrewMod/weapons.ini to the mounts list, always in file order, and add a matching entry to
//    a private copy of the slot's SelectionOrder. A saved position therefore points at the same gun every time.
//
// Each [item] in weapons.ini copies one stock gun (base=) into one slot family (family=, the SelectionOrder name),
// then applies numeric edits to the copy of its GunType (mul.<field>=2 or set.<field>=100). Never delete or reorder
// items you have already used in a save: set retired=1 instead, which keeps the position but hides the gun.
//
// Remove by deleting version.dll. Unequip the extra guns first: a save that points at a missing position makes the
// game's loadout restore fail for that slot and the slots after it.

#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <map>
#include <set>
#include <random>
#include <array>
#include "crew_report.h"
#include "crew_talk.h"                                             // includes crew_mail.h and crew_life.h
#include "crew_bonds.h"
#include "crew_nick.h"
#include <string>
#include <vector>

#define FWD(n) __pragma(comment(linker, "/export:" #n "=C:\\Windows\\System32\\version." #n))
FWD(GetFileVersionInfoA) FWD(GetFileVersionInfoW) FWD(GetFileVersionInfoExA) FWD(GetFileVersionInfoExW)
FWD(GetFileVersionInfoSizeA) FWD(GetFileVersionInfoSizeW) FWD(GetFileVersionInfoSizeExA) FWD(GetFileVersionInfoSizeExW)
FWD(VerFindFileA) FWD(VerFindFileW) FWD(VerInstallFileA) FWD(VerInstallFileW)
FWD(VerLanguageNameA) FWD(VerLanguageNameW) FWD(VerQueryValueA) FWD(VerQueryValueW)

// ---- log ---------------------------------------------------------------------------------------------------
static const char* kModVersion = "0.9.9";                   // shown in the log's first line
static FILE* g_log = nullptr;
static bool g_debug = false;                                         // drops.ini debug=1: detailed logging for troubleshooting
static void Log(const char* fmt, ...) {
  if (!g_log) return;
  SYSTEMTIME t; GetLocalTime(&t);
  _lock_file(g_log);                                                   // whole lines: the newsreel writer thread logs too
  fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
  fputc('\n', g_log); fflush(g_log);
  _unlock_file(g_log);
}
// Detail that only helps when hunting a bug (every loadout click, catalogues, timing); written only with debug=1.
static void DLog(const char* fmt, ...) {
  if (!g_log || !g_debug) return;
  SYSTEMTIME t; GetLocalTime(&t);
  _lock_file(g_log);                                                   // whole lines: the newsreel writer thread logs too
  fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
  fputc('\n', g_log); fflush(g_log);
  _unlock_file(g_log);
}

#include "wm_il2cpp.h"
#include "wm_items.h"
#include "wm_unity.h"
#include "wm_gun_models.h"
#include "wm_gun_paint.h"
#include "wm_inject.h"
#include "wm_positions.h"
#include "wm_drops_data.h"
#include "wm_drops.h"
#include "wm_grid_pickups.h"
#include "wm_loadout_screen.h"
#include "wm_inventory_window.h"
#include "wm_per_save.h"
#include "wm_ui_kit.h"
#include "wm_crates.h"
#include "wm_windows.h"
#include "wm_crew_files.h"
#include "wm_gui_host.h"
#include "wm_tracers.h"
#include "wm_auto_utility.h"
#include "wm_crits_smoke.h"
#include "wm_seek_chain.h"
#include "wm_elites.h"
#include "wm_crew_looks.h"
#include "wm_female_crew.h"
#include "wm_unusual.h"
#include "wm_helpers.h"
#include "wm_home_run.h"
#include "wm_gun_effects.h"
#include "wm_fire_balloon.h"
#include "wm_crew_log.h"
#include "wm_crew_hits.h"
#include "wm_prose_report.h"
#include "wm_gun_effects_shots.h"
#include "wm_all_knowing.h"
#include "wm_kill_cam.h"
#include "wm_torpedoes.h"
#include "wm_utility_gear.h"
#include "wm_torp_looks.h"
#include "newsreel.h"                                                // battle newsreel: a second camera films plane dives into a GIF
#include "wm_hull_camo.h"                                            // extra hull camo schemes in the boat's camo list
#include "wm_frame_hooks.h"
#include "wm_coop.h"
// ---- hook: WeaponSystem.Awake --------------------------------------------------------------------------------
typedef void (*fn_message)(void* self, void* method);
static fn_message g_original = nullptr;
static int g_faults = 0;
static void Hooked_Awake(void* self, void* method) {
  g_original(self, method);
  if (!g_ready) return;
  SafeCheckNewCampaign();                                              // a new campaign's boats get its (empty) inventory
  __try { Inject(self); }
  __except (EXCEPTION_EXECUTE_HANDLER) {
    if (++g_faults <= 5) Log("fault while adding guns to slot %p (%d)", self, g_faults);
    if (g_faults >= 20) { g_ready = false; Log("too many faults; the mod switched itself off"); }
  }
  if (!g_stylesDumped && g_dumpTries <= 60) {
    __try { DumpRenderStyles(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_dumpTries = 99; Log("fault while looking at the projectile style renderers; skipped"); }
  }
}

static bool InstallHook() {
  void* klass = FindClass("Core", "WeaponSystem");
  void* mi = klass ? il2cpp_class_get_method_from_name(klass, "Awake", 0) : nullptr;
  if (!mi) { Log("WeaponSystem.Awake not found"); return false; }
  void** slot = (void**)mi;
  g_original = (fn_message)*slot;
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) { Log("VirtualProtect failed"); return false; }
  *slot = (void*)&Hooked_Awake;
  VirtualProtect(slot, sizeof(void*), old, &old);
  DLog("hooked WeaponSystem.Awake (original rva 0x%llX)", (unsigned long long)((uintptr_t)g_original - (uintptr_t)g_ga));
  {                                                                    // tracer_color, uses per mission and Automated utility guns need it
    void* pmk = FindClass("Core", "ProjectileManager");
    void* pmi = pmk ? il2cpp_class_get_method_from_name(pmk, "LateUpdate", 0) : nullptr;
    if (!pmi) Log("ProjectileManager.LateUpdate not found; tracer_color is ignored");
    else {
      void** ps = (void**)pmi; g_pmLateUpdateOriginal = (void (*)(void*, void*))*ps;
      DWORD old2 = 0;
      if (VirtualProtect(ps, sizeof(void*), PAGE_READWRITE, &old2)) { *ps = (void*)&Hooked_PMLateUpdate; VirtualProtect(ps, sizeof(void*), old2, &old2); DLog("hooked ProjectileManager.LateUpdate for tracer_color"); }
      else Log("VirtualProtect failed for ProjectileManager.LateUpdate");
    }
  }
  {                                                                    // the torpedo kill cam: the game camera's LateUpdate and BlurModulator.Update (game speed)
    void* ck = FindClass("Core.Player", "PlayerCameraControlManager"); void* cmi = ck ? il2cpp_class_get_method_from_name(ck, "LateUpdate", 0) : nullptr;
    void* bk = FindClass("Core", "BlurModulator"); void* bmi = bk ? il2cpp_class_get_method_from_name(bk, "Update", 0) : nullptr;
    if (!cmi || !bmi) Log("kill cam: PlayerCameraControlManager.LateUpdate %p / BlurModulator.Update %p not found; the kill cam is off", cmi, bmi);
    else {
      void** cs = (void**)cmi; void** bs = (void**)bmi; DWORD o4 = 0, o5 = 0;
      g_camLateOriginal = (void (*)(void*, void*))*cs; g_blurUpdateOriginal = (void (*)(void*, void*))*bs;
      if (VirtualProtect(cs, sizeof(void*), PAGE_READWRITE, &o4)) { *cs = (void*)&Hooked_CamLateUpdate; VirtualProtect(cs, sizeof(void*), o4, &o4); }
      if (VirtualProtect(bs, sizeof(void*), PAGE_READWRITE, &o5)) { *bs = (void*)&Hooked_BlurUpdate; VirtualProtect(bs, sizeof(void*), o5, &o5); }
      DLog("hooked PlayerCameraControlManager.LateUpdate and BlurModulator.Update for the kill cam");
    }
  }
  {                                                                    // critical hits: ProjectileManager.Update, before it resolves hits
    void* pmk = FindClass("Core", "ProjectileManager");
    void* umi = pmk ? il2cpp_class_get_method_from_name(pmk, "Update", 0) : nullptr;
    if (!umi) Log("ProjectileManager.Update not found; critical hits are off");
    else {
      void** us = (void**)umi; g_pmUpdateOriginal = (void (*)(void*, void*))*us; DWORD o3 = 0;
      if (VirtualProtect(us, sizeof(void*), PAGE_READWRITE, &o3)) { *us = (void*)&Hooked_PMUpdate; VirtualProtect(us, sizeof(void*), o3, &o3); DLog("hooked ProjectileManager.Update for critical hits"); }
    }
  }
  {                                                                    // on-kill effects: WeaponSystem's IKillCredible.creditKill vtable entry
    void* wsc = FindClass("Core", "WeaponSystem");
    void* cmi = wsc ? il2cpp_class_get_method_from_name(wsc, "creditKill", 2) : nullptr;
    if (!cmi || !PatchVtableEntry(wsc, cmi, (void*)&Hooked_CreditKill, (void**)&g_creditKillOrig)) Log("could not hook WeaponSystem.creditKill; on-kill effects are off");
    else DLog("hooked WeaponSystem.creditKill for on-kill effects");
    void* cmk = FindClass("Core", "CrewMember");
    void* cmm = cmk ? il2cpp_class_get_method_from_name(cmk, "creditKill", 2) : nullptr;
    g_offCrewGun = FieldOffset(cmk, "achievedGunMount");
    if (!cmm || g_offCrewGun < 0 || !PatchVtableEntry(cmk, cmm, (void*)&Hooked_CrewCreditKill, (void**)&g_crewCreditKillOrig)) Log("could not hook CrewMember.creditKill; on-kill effects only work for crewless guns");
    else DLog("hooked CrewMember.creditKill for on-kill effects (achievedGunMount +0x%X)", g_offCrewGun);
  }
  InstallDiagnostics();
  TorpInstall();                                                       // found torpedoes (needs InstallDiagnostics' offsets)
  CrewVisInstall();                                                    // crew weapon log, red tracers, marker, aim line
  NewsreelInstall();                                                   // hooks always installed; drops.ini newsreel=0 stops the filming
  HullCamoInstall();
  InstallInventoryGui();
  InstallNameFit();
  g_perSaveWanted = true;                                              // installed from the game thread, see TryInstallPerSave
  g_dropsOn = g_dropChance > 0 && !g_templates.empty() && !g_affixes.empty();
  {                                                                    // the random drops and Dave's crate list
    void* pbk = FindClass("Core", "PickupBehaviour"); void* ptk = FindClass("Core", "PickupType");
    void* fR = pbk ? il2cpp_class_get_field_from_name(pbk, "resourceDrop") : nullptr; void* rdk = FindClass("Core", "ResourceDropBehaviour");
    void* fS = rdk ? il2cpp_class_get_field_from_name(rdk, "splashed") : nullptr;
    if (fR && fS) { g_offResDrop = (int)il2cpp_field_get_offset(fR); g_offSplashed = (int)il2cpp_field_get_offset(fS); }
    else Log("Dave: a crate lookup failed (resourceDrop %p splashed %p); Dave picks up nothing", fR, fS);
    void* fP = pbk ? il2cpp_class_get_field_from_name(pbk, "pickup") : nullptr;
    void* fD = pbk ? il2cpp_class_get_field_from_name(pbk, "pickedUp") : nullptr;
    void* fI = ptk ? il2cpp_class_get_field_from_name(ptk, "item") : nullptr;
    void* fC = ptk ? il2cpp_class_get_field_from_name(ptk, "baseAddCount") : nullptr;
    void* pmi = pbk ? il2cpp_class_get_method_from_name(pbk, "Update", 0) : nullptr;
    if (!fP || !fD || !fI || !fC || !pmi) { g_dropsOn = false; g_offSplashed = -1; Log("random drops: a lookup failed (pickup %p pickedUp %p item %p baseAddCount %p Update %p); drops and Dave's crates are off", fP, fD, fI, fC, pmi); }
    else {
      g_offPickup = (int)il2cpp_field_get_offset(fP); g_offPickedUp = (int)il2cpp_field_get_offset(fD);
      g_offPtItem = (int)il2cpp_field_get_offset(fI); g_offPtCount = (int)il2cpp_field_get_offset(fC);
      void** ps = (void**)pmi; g_pickupUpdateOriginal = (void (*)(void*, void*))*ps;
      DWORD old3 = 0;
      if (VirtualProtect(ps, sizeof(void*), PAGE_READWRITE, &old3)) { *ps = (void*)&Hooked_PickupUpdate; VirtualProtect(ps, sizeof(void*), old3, &old3); DLog("hooked PickupBehaviour.Update for random drops (%s, chance %.2f) and Dave's crates", g_dropsOn ? "on" : "off", g_dropChance); }
      else Log("VirtualProtect failed for PickupBehaviour.Update");
    }
  }
  return true;
}

static void ReadDebugFlag() {                                        // drops.ini debug=1, before drops.ini is fully loaded
  FILE* df = fopen((g_folder + "\\drops.ini").c_str(), "r"); char dl[256];
  while (df && fgets(dl, sizeof dl, df)) { std::string t = Trim(dl); if (t.compare(0, 6, "debug=") == 0) g_debug = atoi(t.c_str() + 6) != 0; }
  if (df) fclose(df);
}

// The main thread is suspended only while its registers and the top 64 KB of its stack are copied: no lock is taken
// then (GetThreadContext, VirtualQuery and memcpy only), so a sample can never hang the game. The copy is read after
// it runs again: every word that points just after a call instruction inside a loaded module is a likely return
// address, which gives the call chain without symbols (GameAssembly offsets map to methods with the IL2CPP dump).
static unsigned char g_stackCopy[65536];
static bool CaptureMainStack(DWORD tid, DWORD64* rip, DWORD64* rsp, size_t* got) {
  HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
  if (!th) return false;
  bool ok = false;
  if (SuspendThread(th) != (DWORD)-1) {
    __try {
      alignas(16) CONTEXT ctx; memset(&ctx, 0, sizeof ctx); ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
      if (GetThreadContext(th, &ctx)) {
        *rip = ctx.Rip; *rsp = ctx.Rsp; *got = 0;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)ctx.Rsp, &mbi, sizeof mbi)) {
          size_t avail = (size_t)((char*)mbi.BaseAddress + mbi.RegionSize - (char*)ctx.Rsp);
          *got = avail < sizeof g_stackCopy ? avail : sizeof g_stackCopy;
          memcpy(g_stackCopy, (void*)ctx.Rsp, *got);
        }
        ok = true;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    ResumeThread(th);
  }
  CloseHandle(th);
  return ok;
}
static bool AfterCall(DWORD64 a) {                                    // the bytes before a are a call instruction
  __try {
    const unsigned char* b = (const unsigned char*)a;
    if (b[-5] == 0xE8) return true;                                    // call rel32
    if (b[-6] == 0xFF && (b[-5] == 0x15 || (b[-5] & 0xF8) == 0x90)) return true;   // call [rip+d32] / [reg+d32]
    if (b[-2] == 0xFF && (b[-1] & 0xF8) == 0xD0) return true;          // call reg
    if (b[-3] == 0xFF && (b[-2] & 0xF8) == 0x50) return true;          // call [reg+d8]
    if (b[-3] == 0x41 && b[-2] == 0xFF && (b[-1] & 0xF8) == 0xD0) return true;   // call r8-r15
    if (b[-4] == 0xFF && b[-3] == 0x54) return true;                   // call [rsp+d8]
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
  return false;
}
static std::string AddrName(DWORD64 a) {                               // "GameAssembly.dll+0x1234", or the bare address
  static std::map<HMODULE, std::string> names;
  HMODULE h = nullptr; char b[96];
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)a, &h) || !h) {
    snprintf(b, sizeof b, "0x%llX", (unsigned long long)a); return b;
  }
  auto n = names.find(h);
  if (n == names.end()) {
    char p[MAX_PATH] = ""; GetModuleFileNameA(h, p, MAX_PATH); const char* s = strrchr(p, '\\');
    n = names.emplace(h, s ? s + 1 : p).first;
  }
  snprintf(b, sizeof b, "%s+0x%llX", n->second.c_str(), (unsigned long long)(a - (DWORD64)h));
  return b;
}
static std::string StackText(DWORD64 rip, size_t got) {
  std::string s = "at " + AddrName(rip) + "; called from";
  int frames = 0;
  for (size_t off = 0; off + 8 <= got && frames < 28; off += 8) {
    DWORD64 v = *(DWORD64*)(g_stackCopy + off);
    if (v < 0x10000 || v > 0x7FFFFFFFFFFFull || !AfterCall(v)) continue;
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)v, &h) || !h) continue;
    s += " " + AddrName(v); frames++;
  }
  return frames ? s : s + " (nothing found)";
}
static bool GameInForeground() {
  HWND w = GetForegroundWindow(); DWORD pid = 0;
  if (w) GetWindowThreadProcessId(w, &pid);
  return pid == GetCurrentProcessId();
}
static DWORD g_swArmed = 0, g_swStallFrom = 0, g_swNextSample = 0, g_swNextReport = 0;
static int g_swSamples = 0, g_swReports = 0; static LONG g_swBeatsAt = 0; static std::string g_swLastStack;
static void StallWatchStep() {
  DWORD a = g_stallArmedAt, now = GetTickCount();
  if (a != g_swArmed) {                                                  // a new save load
    g_swArmed = a; g_swStallFrom = 0; g_swSamples = 0; g_swReports = 0; g_swBeatsAt = g_beatCount; g_swNextReport = now + 10000; g_swLastStack.clear();
    if (a) DLog("load watch: %s loaded; watching for frames that stop for 5 s or more (10 minutes)", g_stallWhat);
  }
  if (!g_swArmed) return;
  if (now - g_swArmed > 600000) { DLog("load watch: done (10 minutes after the load)"); g_stallArmedAt = 0; g_swArmed = 0; return; }
  if (g_swReports < 12 && (LONG)(now - g_swNextReport) >= 0) {          // the first 2 minutes: frames per 10 s
    LONG b = g_beatCount;
    DLog("load watch: +%lu s after the load, %ld game ticks (frames and UI events) in the last 10 s%s", (now - g_swArmed) / 1000, b - g_swBeatsAt, GameInForeground() ? "" : " (game in the background)");
    g_swBeatsAt = b; g_swNextReport = now + 10000; g_swReports++;
  }
  DWORD last = g_lastBeat, tid = g_mainTid;
  if (!last || !tid) return;                                            // no heartbeat seen yet
  DWORD gap = now - last;
  if (!g_swStallFrom && gap >= 5000 && GameInForeground()) {
    g_swStallFrom = last; g_swNextSample = now; g_swSamples = 0;
    Log("load watch: the game has run no frame for %lu s (+%lu s after the load); sampling the main thread every 2 s", gap / 1000, (long)(last - g_swArmed) > 0 ? (unsigned long)(last - g_swArmed) / 1000 : 0ul);
  }
  if (g_swStallFrom && gap < 1000) {
    Log("load watch: frames again after %lu s without one (%d samples)", (last - g_swStallFrom) / 1000, g_swSamples);
    g_swStallFrom = 0; g_swLastStack.clear(); return;
  }
  if (g_swStallFrom && (LONG)(now - g_swNextSample) >= 0) {
    DWORD64 rip = 0, rsp = 0; size_t got = 0;
    if (CaptureMainStack(tid, &rip, &rsp, &got)) {
      std::string st = StackText(rip, got);
      g_swSamples++;
      if (st == g_swLastStack) Log("load watch: sample %d (+%lu s): same as before", g_swSamples, (now - g_swStallFrom) / 1000);
      else Log("load watch: sample %d (+%lu s): main thread %s", g_swSamples, (now - g_swStallFrom) / 1000, st.c_str());
      g_swLastStack = st;
    }
    g_swNextSample = now + (g_swSamples < 60 ? 2000 : 10000);
  }
}
// During a slow load the main thread keeps running frames: it only waits ("[ECM] Waiting for world") while
// thread-pool tasks build the world ("Creating factions", "Loading Save", ...). So
// during the load window the game's own Player.log lines are copied here with times (Player.log has none), until
// "Finished Loading World", and every 10 s the threads that used CPU are listed and the busiest one is sampled.
static HANDLE g_swGameLog = INVALID_HANDLE_VALUE;
static std::string g_swPartial;
static int g_swLines = 0;
static bool g_swWorldDone = false;
static DWORD g_swNextCpu = 0, g_swWorldDoneAt = 0;
static std::map<DWORD, ULONGLONG> g_swCpuPrev;
static void GameLogOpen() {
  if (g_swGameLog != INVALID_HANDLE_VALUE) CloseHandle(g_swGameLog);
  g_swGameLog = INVALID_HANDLE_VALUE; g_swPartial.clear(); g_swLines = 0; g_swWorldDone = false; g_swWorldDoneAt = 0;
  const char* home = getenv("USERPROFILE");
  if (!home) return;
  std::string p = std::string(home) + "\\AppData\\LocalLow\\Tabbing Tabby\\Boat Crew\\Player.log";
  g_swGameLog = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (g_swGameLog == INVALID_HANDLE_VALUE) { Log("load watch: cannot read %s; the world load time is not logged", p.c_str()); return; }
  // the save was read moments ago: start a little before the end, at the start of a line, to keep the read's lines
  LARGE_INTEGER sz; GetFileSizeEx(g_swGameLog, &sz);
  LARGE_INTEGER at; at.QuadPart = sz.QuadPart > 4096 ? sz.QuadPart - 4096 : 0;
  SetFilePointerEx(g_swGameLog, at, nullptr, FILE_BEGIN);
  if (at.QuadPart) g_swPartial = "\x01";                               // drop the first, partial line
}
static void GameLogTail(DWORD sinceArm) {
  if (g_swGameLog == INVALID_HANDLE_VALUE) return;
  char buf[16384]; DWORD n = 0;
  for (int rounds = 0; rounds < 8 && ReadFile(g_swGameLog, buf, sizeof buf, &n, nullptr) && n; rounds++) {
    g_swPartial.append(buf, n);
    size_t at;
    while ((at = g_swPartial.find('\n')) != std::string::npos) {
      std::string l = g_swPartial.substr(0, at); g_swPartial.erase(0, at + 1);
      if (!l.empty() && l[0] == '\x01') continue;                       // the partial first line
      while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
      if (l.empty() || l[0] == ' ' || l[0] == '\t' || l.find("(at ") != std::string::npos || l.compare(0, 17, "System.Threading.") == 0 ||
          l.compare(0, 12, "UnityEngine.") == 0 || l.compare(0, 23, "System.Runtime.Compiler") == 0) continue;   // stack-trace lines
      for (char& c : l) if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '?';
      if (l.size() > 180) l = l.substr(0, 180) + "...";
      if (g_swLines++ < 400) DLog("load watch: +%.1f s game log: %s", sinceArm / 1000.0, l.c_str());
      if (!g_swWorldDone && l.find("Finished Loading World") != std::string::npos) {
        g_swWorldDone = true; g_swWorldDoneAt = GetTickCount();
        Log("load watch: the world finished loading %.1f s after the save was read", sinceArm / 1000.0);
      }
    }
  }
}
static void ThreadCpu(DWORD sinceArm, DWORD mainTid) {                // threads that used CPU in the last 10 s; sample the busiest
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return;
  std::map<DWORD, ULONGLONG> cur; std::vector<std::pair<ULONGLONG, DWORD>> used;
  THREADENTRY32 te; te.dwSize = sizeof te;
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
    HANDLE th = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID); if (!th) continue;
    FILETIME c, e, k, u;
    if (GetThreadTimes(th, &c, &e, &k, &u)) {
      ULONGLONG t = ((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime);
      cur[te.th32ThreadID] = t;
      auto p = g_swCpuPrev.find(te.th32ThreadID);
      if (p != g_swCpuPrev.end() && t > p->second) used.push_back({ t - p->second, te.th32ThreadID });
    }
    CloseHandle(th);
  }
  CloseHandle(snap);
  bool first = g_swCpuPrev.empty();
  g_swCpuPrev.swap(cur);
  if (first) return;
  std::sort(used.rbegin(), used.rend());
  std::string s; ULONGLONG total = 0;
  for (auto& u : used) total += u.first;
  for (size_t i = 0; i < used.size() && i < 5; i++) {                 // 100-ns units over 10 s (1e8): /1e6 = percent of one core
    char b[64]; snprintf(b, sizeof b, " %lu%s %.0f%%", used[i].second, used[i].second == mainTid ? "(main)" : "", used[i].first / 1e6); s += b;
  }
  DLog("load watch: +%lu s: CPU in the last 10 s, all threads %.0f%% of a core; busiest:%s", sinceArm / 1000, total / 1e6, s.empty() ? " none" : s.c_str());
  if (!used.empty() && used[0].first > 20000000) {                    // the busiest used over 20% of a core: where is it?
    DWORD64 rip = 0, rsp = 0; size_t got = 0;
    if (CaptureMainStack(used[0].second, &rip, &rsp, &got)) DLog("load watch: busiest thread %lu %s", used[0].second, StackText(rip, got).c_str());
  }
}
static void StallWatchExtras() {
  DWORD a = g_stallArmedAt, now = GetTickCount();
  static DWORD opened = 0;
  if (a && a != opened) { opened = a; GameLogOpen(); g_swCpuPrev.clear(); g_swNextCpu = now; }
  if (!a) return;
  bool active = !g_swWorldDone || now - g_swWorldDoneAt < 20000;       // until 20 s after the world is done
  if (!active) return;
  GameLogTail(now - a);
  if ((LONG)(now - g_swNextCpu) >= 0) { ThreadCpu(now - a, g_mainTid); g_swNextCpu = now + 10000; }
}
static DWORD WINAPI StallWatchThread(LPVOID) {
  for (;;) {
    Sleep(250);
    if (!g_debug) continue;                                            // the load watch is a diagnostic: debug=1 only
    __try { StallWatchStep(); StallWatchExtras(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("load watch: fault; it stops"); return 0; }
  }
}

static DWORD WINAPI InitThread(LPVOID) {
  char dir[MAX_PATH]; GetModuleFileNameA(nullptr, dir, MAX_PATH);
  char* slash = strrchr(dir, '\\'); if (slash) *slash = 0;
  char folder[MAX_PATH]; snprintf(folder, sizeof folder, "%s\\BoatCrewMod", dir);
  CreateDirectoryA(folder, nullptr);
  char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\weaponmod.log", folder);
  g_log = fopen(path, "a");
  g_folder = folder;
  ReadDebugFlag();                                                     // first, so loading is logged in detail too
  Log("---- BoatCrewWeaponMod %s pid=%lu%s", kModVersion, GetCurrentProcessId(), g_debug ? " (debug logging on)" : "");
  g_folder = folder;
  LoadItems(folder);
  LoadDrops();
  g_tracerAtLaunch = g_tracerRarity;
  if (!g_turretGuns) {                                                  // hidden, not removed: their positions stay taken
    for (Item& it : g_items) if (IsTurretGun(it)) it.retired = true;
    Log("turret guns: off (drops.ini turret_guns=0)");
  }
  if (!g_sideGrades) {
    for (Item& it : g_items) if (it.builtIn && !IsTurretGun(it) && !IsUtilityGear(it)) it.retired = true;
    Log("side-grades: off (drops.ini side_grades=0)");
  }
  UnusualModels();
  LoadEnemies();
  g_elitesAtLaunch = g_elitesOn;
  LoadPositions();
  PurgeRetired();
  ConvertSlowShells();
  ConvertAutomated();
  RefreshUniques();
  GridLayout();
  LoadUi();
  SyncPositions();
  PrepareSaves();
  for (int i = 0; i < 1200 && !(g_ga = GetModuleHandleA("GameAssembly.dll")); i++) Sleep(100);
  if (!g_ga) { Log("GameAssembly.dll was never loaded; giving up"); return 0; }
  if (!Resolve()) { Log("some il2cpp exports are missing; giving up"); return 0; }
  for (int i = 0; i < 1200; i++) { if (DomainReady(&g_domain)) break; Sleep(250); }
  if (!g_domain) { Log("the IL2CPP runtime never became ready; giving up"); return 0; }
  DLog("runtime ready. Waiting 5 s for startup to settle");
  Sleep(5000);
  __try {
    il2cpp_thread_attach(g_domain);
    if (Setup() && InstallHook()) {
      g_ready = true; Log("ready. Guns from weapons.ini are added to every boat as it spawns.");
      CreateThread(nullptr, 0, StallWatchThread, nullptr, 0, nullptr);   // load watch (see StallBeat)
      for (int i = 0; i < 100 && !g_perSaveClaim; i++) Sleep(100);
      TryInstallPerSave("the mod thread (no game hook ran within 10 s)");
    }
    DLog("GC: incremental=%d, write barrier %s", il2cpp_gc_is_incremental ? (int)il2cpp_gc_is_incremental() : -1, il2cpp_gc_wbarrier_set_field ? "used" : "NOT exported");
  } __except (EXCEPTION_EXECUTE_HANDLER) { Log("setup faulted; the mod stays idle"); }
  return 0;
}

// The game folder also holds helper programs (UnityCrashHandler64.exe) that load this DLL too; only the game runs the mod.
static bool IsGameProcess() {
  char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
  const char* name = strrchr(exe, '\\'); name = name ? name + 1 : exe;
  return _stricmp(name, "Boat Crew.exe") == 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);
    if (IsGameProcess()) CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
  }
  return TRUE;
}
