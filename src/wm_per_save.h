// per-save inventory sidecars, the stall watch, save/load hooks (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- per-save inventory ---------------------------------------------------------------------------------------------
// The game keeps a campaign in .BCS files: named saves in GameSaves/Campaign/<name>_<id>.BCS and 12 rolling autosaves in
// Campaign/SaveSlot0 that are renamed as they age. Whenever it writes a campaign save, the mod copies inventory.ini to
// BoatCrewMod/saves/<FNV-1a 64 of the save file>.ini, so the copy follows the file through those renames. Loading a save
// puts its copy back as inventory.ini and reloads the found guns; a save with no copy (made before this) gets
// saves/legacy.ini, the inventory as it was when this first ran. A new campaign starts with no found guns. positions.ini
// stays shared: a gun from another save is a hidden placeholder there, so saved loadout positions never move.
// GameSave.saveState(IDataObject) and GameSave.readAndDecompress(IDataObject, string) are static and called directly, so
// a MethodInfo patch would miss them: their first 15 bytes (plain register saves, read in the disassembly 2026-10-29)
// become a jump to the hook, and a trampoline runs those bytes and jumps back.
static void* g_clsCampaignState = nullptr;
static void *g_fCampaignCurrent = nullptr, *g_fGameSavePath = nullptr, *g_fStartState = nullptr;
static void (*g_saveStateOrig)(void*, void*) = nullptr;
static void (*g_readSaveOrig)(void*, void*, void*) = nullptr;
static void* g_campaignState = nullptr;                                // the CampaignState the live inventory belongs to
static void* g_campaignUnknown = nullptr;                              // a state that is neither loaded nor new (logged once)
static int g_lastStartState = 0; static bool g_newCampaignSeen = false;   // CampaignPrimer.campaignStartState, see CheckNewCampaign

static std::string SavesDir() { return g_folder + "\\saves"; }
static std::wstring WideOf(void* s) { return s ? std::wstring((const wchar_t*)((char*)s + 0x14), *(int*)((char*)s + 0x10)) : std::wstring(); }
static bool HashFile(const std::wstring& path, unsigned long long* out) {
  FILE* f = _wfopen(path.c_str(), L"rb");
  if (!f) return false;
  unsigned long long h = 1469598103934665603ULL; unsigned char buf[16384]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
  fclose(f); *out = h; return true;
}
static std::string SidecarPath(unsigned long long h) { char b[32]; snprintf(b, sizeof b, "\\%016llx.ini", h); return SavesDir() + b; }
static std::string FileNameOf(const std::wstring& w) {
  size_t s = w.find_last_of(L"\\/"); std::wstring n = s == std::wstring::npos ? w : w.substr(s + 1);
  char b[260]; int c = WideCharToMultiByte(CP_UTF8, 0, n.c_str(), (int)n.size(), b, 259, nullptr, nullptr); b[c < 0 ? 0 : c] = 0; return b;
}
static bool FileExists(const std::string& p) { DWORD a = GetFileAttributesA(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
// src over dst through dst.tmp and a replacing move, as SaveInventory writes: a crash never leaves a half file.
static bool CopyAtomic(const std::string& src, const std::string& dst) {
  std::string tmp = dst + ".tmp";
  if (!CopyFileA(src.c_str(), tmp.c_str(), FALSE)) return false;
  return MoveFileExA(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
// Before a switch the live inventory goes to saves\backup\live-<date>-<time>.ini; the newest 20 are kept. (One
// last-live.ini was overwritten by a second switch in a row, losing changes made after the last game save; review
// 2026-10-29.)
static void BackupLive(const char* file = "inventory.ini", const char* pre = "live") {
  std::string dir = SavesDir() + "\\backup", live = g_folder + "\\" + file;
  if (!FileExists(live)) return;
  CreateDirectoryA(dir.c_str(), nullptr);
  SYSTEMTIME t; GetLocalTime(&t); char name[64];
  snprintf(name, sizeof name, "\\%s-%04d%02d%02d-%02d%02d%02d-%03d.ini", pre, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  CopyFileA(live.c_str(), (dir + name).c_str(), FALSE);
  std::vector<std::string> all; WIN32_FIND_DATAA fa; HANDLE h = FindFirstFileA((dir + "\\" + pre + "-*.ini").c_str(), &fa);
  if (h == INVALID_HANDLE_VALUE) return;
  do all.push_back(fa.cFileName); while (FindNextFileA(h, &fa));
  FindClose(h);
  std::sort(all.begin(), all.end());                                   // the names sort by time
  for (size_t i = 0; i + 20 < all.size(); i++) DeleteFileA((dir + "\\" + all[i]).c_str());
}

// Puts a saved inventory (src) in place of the live one and reloads the found guns; src empty starts with none (a new
// campaign). The live file is backed up first: anything changed after the last game save is not in any save's copy.
// rebuild: the boats exist already (the loadout screen), so their lists are redone too.
static void SwitchInventory(const std::string& src, const std::string& why, bool rebuild) {
  std::string live = g_folder + "\\inventory.ini";
  std::vector<std::string> before;
  for (const Item& it : g_items) if (it.fromInventory && !it.held && !it.retired) before.push_back(it.name);
  BackupLive();
  if (!src.empty() && !CopyAtomic(src, live)) { Log("saves: could not copy %s (error %lu); inventory kept", src.c_str(), GetLastError()); return; }
  g_items.erase(std::remove_if(g_items.begin(), g_items.end(), [](const Item& it) { return it.fromInventory; }), g_items.end());
  g_mat[0] = g_mat[1] = g_mat[2] = 0; g_crates[0] = g_crates[1] = g_crates[2] = 0; g_invUniqueUses = 0;
  int n = src.empty() ? 0 : ParseItemsFile(live.c_str(), "inventory.ini", true);
  if (src.empty()) SaveInventory();                                    // an empty inventory.ini, so the next game save copies that
  PurgeRetired(); ConvertSlowShells(); ConvertAutomated(); RefreshUniques(); GridLayout(); SyncPositions();
  g_propsCache.clear();
  int gone = 0;
  for (const std::string& nm : before) if (!FindItem(nm) && UnlistItem(nm) > 0) gone++;
  if (rebuild) { InjectBoats(); g_refreshGunSlots = true; }
  int kept = 0, loot = 0;
  for (const Item& it : g_items) if (it.fromInventory && !it.retired) (it.held ? loot : kept)++;
  Log("saves: %s: %d kept guns, %d in the loot grid, materials %d/%d/%d, crates %d/%d/%d%s", why.c_str(), kept, loot,
      g_mat[0], g_mat[1], g_mat[2], g_crates[0], g_crates[1], g_crates[2], gone ? (", " + std::to_string(gone) + " guns of the last save taken off the lists").c_str() : "");
  (void)n;
}

static void CrewFilesSwitch(const std::string& src, const std::string& why);   // crew files (crew personality phase 4)
static std::string CrewSidecar(unsigned long long h) { char b[32]; snprintf(b, sizeof b, "\\%016llx.crew", h); return SavesDir() + b; }
static bool IsCurrentCampaign(void* state) {
  if (!state || !g_clsCampaignState || il2cpp_object_get_class(state) != g_clsCampaignState) return false;
  void* cur = nullptr; if (g_fCampaignCurrent) il2cpp_field_static_get_value(g_fCampaignCurrent, &cur);
  return cur == state;
}
static void CheckNewCampaign(bool rebuild);
static void AfterCampaignSave(void* state) {
  if (!IsCurrentCampaign(state)) return;
  CheckNewCampaign(false);                                             // a new campaign's first save copies its empty inventory
  void* ps = nullptr; il2cpp_field_static_get_value(g_fGameSavePath, &ps);
  std::wstring path = WideOf(ps); unsigned long long h;
  if (path.empty() || !HashFile(path, &h)) { Log("saves: the game saved, but its file could not be read; no inventory copy"); return; }
  std::string to = SidecarPath(h);
  if (!CopyAtomic(g_folder + "\\inventory.ini", to)) Log("saves: could not write %s (error %lu)", to.c_str(), GetLastError());
  else DLog("saves: game saved %s; inventory copied to saves\\%016llx.ini", FileNameOf(path).c_str(), h);
  std::string crew = g_folder + "\\crew-files.ini";                    // the crew files go with the save the same way
  if (FileExists(crew) && !CopyAtomic(crew, CrewSidecar(h))) Log("crew files: could not write %s (error %lu)", CrewSidecar(h).c_str(), GetLastError());
}
// ---- stall watch ----
// Loading a save from the main menu sometimes sits on a black screen for minutes
// (the menu sound still playing); the devs know it and not why. For 10 minutes after a save load a watchdog thread
// (StallWatchThread) checks that the game still runs frames. When none ran for 5 s it samples the main thread's stack
// every 2 s and logs where it is, so the log names the code the load hangs in. Heartbeats: ProjectileManager.LateUpdate
// and the OnGUI hosts, both on the main thread.
static volatile LONG g_beatCount = 0;
static volatile DWORD g_lastBeat = 0, g_mainTid = 0, g_stallArmedAt = 0;   // g_stallArmedAt: tick of the arming load, 0 = off
static char g_stallWhat[160] = "";                                    // what was loaded, for the log
static void StallBeat() { g_lastBeat = GetTickCount(); g_mainTid = GetCurrentThreadId(); InterlockedIncrement(&g_beatCount); }
static void StallArm(const std::string& what) {
  snprintf(g_stallWhat, sizeof g_stallWhat, "%s", what.c_str());
  g_stallArmedAt = GetTickCount(); if (!g_stallArmedAt) g_stallArmedAt = 1;
}

static void AfterCampaignRead(void* state, void* pathStr) {
  if (!IsCurrentCampaign(state)) return;                               // the load menu reads every save into a spare state
  StallArm(FileNameOf(WideOf(pathStr)));
  std::wstring path = WideOf(pathStr); unsigned long long h = 0;
  std::string name = FileNameOf(path);
  if (path.empty() || !HashFile(path, &h)) { Log("saves: a campaign was loaded from a file the mod cannot read; inventory kept"); g_campaignState = state; return; }
  std::string side = SidecarPath(h), legacy = SavesDir() + "\\legacy.ini";
  g_campaignState = state; g_newCampaignSeen = false;
  // rebuild: the game may have built the boat already, with the last save's guns (see RealizePlaceholder)
  CrewFilesSwitch(FileExists(CrewSidecar(h)) ? CrewSidecar(h) : std::string(), "loaded " + name);   // no copy: no files (never another campaign's)
  if (FileExists(side)) SwitchInventory(side, "loaded " + name, true);
  else if (FileExists(legacy)) SwitchInventory(legacy, "loaded " + name + " (saved before per-save inventories: the inventory from then)", true);
  else Log("saves: loaded %s; it has no inventory copy, so the current inventory carries on", name.c_str());
}
static void Hooked_SaveState(void* state, void* method) {
  g_saveStateOrig(state, method);
  __try { AfterCampaignSave(state); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("saves: fault after a game save; that save has no inventory copy"); }
}
static void Hooked_ReadSave(void* state, void* path, void* method) {
  g_readSaveOrig(state, path, method);
  __try { AfterCampaignRead(state, path); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("saves: fault after a game load; inventory kept"); }
}

// A new campaign never reads a file: CampaignPrimer.startCampaignGame makes a fresh CampaignState and sets
// CampaignPrimer.campaignStartState (0 idle, 1 boat select, 2 first character, 3 first loadout, 4 tutorial). Checked when a
// boat is built, on the loadout screen and before a save is copied; boats built before the flag was set get their lists
// redone when rebuild is set.
// Only the flag turning to 1-3 counts (the game may leave it at 4 after a tutorial), and only for a state no file was
// loaded into, so a later odd state change never empties the inventory. The flag is dropped when the game sets it back
// to 0 and after 15 minutes: a new-campaign screen the player backed out of must not empty the inventory later.
static void CheckNewCampaign(bool rebuild) {
  if (!g_fCampaignCurrent || !g_fStartState) return;
  int start = 0; il2cpp_field_static_get_value(g_fStartState, &start);
  static DWORD seenAt = 0;
  if (start != g_lastStartState) {
    if (start >= 1 && start <= 3) { g_newCampaignSeen = true; seenAt = GetTickCount(); } else if (start == 0) g_newCampaignSeen = false;
    g_lastStartState = start;
  }
  if (g_newCampaignSeen && GetTickCount() - seenAt > 15 * 60 * 1000) { g_newCampaignSeen = false; DLog("saves: the new-campaign flag expired"); }
  void* cur = nullptr; il2cpp_field_static_get_value(g_fCampaignCurrent, &cur);
  if (!cur || cur == g_campaignState) return;
  if (!g_newCampaignSeen) {
    if (cur != g_campaignUnknown) { g_campaignUnknown = cur; DLog("saves: the campaign state changed without a load or a new campaign (yet); inventory kept"); }
    return;
  }
  g_campaignState = cur; g_newCampaignSeen = false;
  SwitchInventory("", "new campaign", rebuild);
  CrewFilesSwitch("", "new campaign");
}
static void TryInstallPerSave(const char* from);
static void SafeCheckNewCampaign() { __try { TryInstallPerSave("WeaponSystem.Awake"); CheckNewCampaign(false); } __except (EXCEPTION_EXECUTE_HANDLER) { g_fStartState = nullptr; Log("saves: fault in the new-campaign check; switched off"); } }

// Writes a 14-byte absolute jump over the first len (15) bytes of a 16-byte aligned function in one 16-byte atomic
// store (bytes 14-15 keep their values) and returns a trampoline: the saved bytes, then a jump to target+len. Only if
// the code is exactly what the disassembly showed. A prologue with a rip-relative instruction cannot run from the
// trampoline as it is: then body (at most 48 bytes) is what the trampoline runs instead of the saved bytes.
static void* Detour(void* target, void* hook, const unsigned char* expect, size_t len, const char* what, const unsigned char* body = nullptr, size_t bodyLen = 0) {
  if (!target || len < 14 || len > 16 || ((uintptr_t)target & 15) || bodyLen > 48) { Log("hook: %s cannot be hooked (%p)", what, target); return nullptr; }
  if (memcmp(target, expect, len) != 0) { Log("hook: %s is not the code this version expects (game updated?)", what); return nullptr; }
  unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!tramp) return nullptr;
  if (!body) { body = (const unsigned char*)target; bodyLen = len; }
  memcpy(tramp, body, bodyLen);
  tramp[bodyLen] = 0xFF; tramp[bodyLen + 1] = 0x25; memset(tramp + bodyLen + 2, 0, 4); uint64_t back = (uint64_t)target + len; memcpy(tramp + bodyLen + 6, &back, 8);
  DWORD tp = 0; VirtualProtect(tramp, 64, PAGE_EXECUTE_READ, &tp);     // written once: execute only from here on
  FlushInstructionCache(GetCurrentProcess(), tramp, 64);
  alignas(16) long long cmp[2], want[2];
  memcpy(cmp, target, 16); memcpy(want, target, 16);
  unsigned char* w = (unsigned char*)want; w[0] = 0xFF; w[1] = 0x25; memset(w + 2, 0, 4); uint64_t to = (uint64_t)hook; memcpy(w + 6, &to, 8);
  DWORD old = 0;
  if (!VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(tramp, 0, MEM_RELEASE); return nullptr; }
  bool ok = _InterlockedCompareExchange128((volatile long long*)target, want[1], want[0], cmp) != 0;
  VirtualProtect(target, 16, old, &old);
  FlushInstructionCache(GetCurrentProcess(), target, 16);
  if (!ok) { VirtualFree(tramp, 0, MEM_RELEASE); Log("hook: %s changed while hooking", what); return nullptr; }
  return tramp;
}

static void InstallPerSave() {
  static const unsigned char kSaveState[15] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20 };
  static const unsigned char kReadSave[15] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18 };
  void* gs = FindClass("Core", "GameSave"); g_clsCampaignState = FindClass("Core.Campaign", "CampaignState"); void* cp = FindClass("Core.Campaign", "CampaignPrimer");
  void* mSave = gs ? FindMethod(gs, "saveState", 1) : nullptr; void* mRead = gs ? FindMethod(gs, "readAndDecompress", 2) : nullptr;
  g_fGameSavePath = gs ? il2cpp_class_get_field_from_name(gs, "filePath") : nullptr;
  g_fCampaignCurrent = g_clsCampaignState ? il2cpp_class_get_field_from_name(g_clsCampaignState, "_current") : nullptr;
  g_fStartState = cp ? il2cpp_class_get_field_from_name(cp, "campaignStartState") : nullptr;
  if (!mSave || !mRead || !g_fGameSavePath || !g_fCampaignCurrent || !g_fStartState) {
    Log("saves: a lookup failed (saveState %p readAndDecompress %p filePath %p _current %p campaignStartState %p); per-save inventories are off", mSave, mRead, g_fGameSavePath, g_fCampaignCurrent, g_fStartState);
    g_fStartState = nullptr; return;
  }
  void* pSave = *(void**)mSave; void* pRead = *(void**)mRead;
  g_saveStateOrig = (void (*)(void*, void*))Detour(pSave, (void*)&Hooked_SaveState, kSaveState, 15, "GameSave.saveState");
  if (!g_saveStateOrig) { g_fStartState = nullptr; Log("saves: per-save inventories are off"); return; }
  g_readSaveOrig = (void (*)(void*, void*, void*))Detour(pRead, (void*)&Hooked_ReadSave, kReadSave, 15, "GameSave.readAndDecompress");
  if (!g_readSaveOrig) { g_fStartState = nullptr; Log("saves: loads are not seen, so saves keep a copy but loading one does not switch"); return; }
  DLog("saves: hooked GameSave.saveState (rva 0x%llX) and readAndDecompress (rva 0x%llX)", (unsigned long long)((uintptr_t)pSave - (uintptr_t)g_ga), (unsigned long long)((uintptr_t)pRead - (uintptr_t)g_ga));
}

// Patching code the game thread may be running is only safe on the game thread, so the first game-thread hook that runs
// (the inventory window's OnGUI, a boat being built, the loadout screen) installs the detours (review 2026-10-29). If
// none has run 10 s after setup, the mod thread does it as before.
static volatile LONG g_perSaveClaim = 0; static bool g_perSaveWanted = false;
static void TryInstallPerSave(const char* from) {
  if (!g_perSaveWanted || g_perSaveClaim || InterlockedExchange(&g_perSaveClaim, 1) != 0) return;
  DLog("saves: installing the save hooks from %s", from);
  InstallPerSave();
}

// At launch (before the game runs): the first time, the current inventory becomes saves/legacy.ini, the inventory of
// every save made before this. Copies whose save file no longer exists are deleted.
static void PrepareSaves() {
  std::string dir = SavesDir();
  if (GetFileAttributesA(dir.c_str()) == INVALID_FILE_ATTRIBUTES) {
    CreateDirectoryA(dir.c_str(), nullptr);
    if (CopyFileA((g_folder + "\\inventory.ini").c_str(), (dir + "\\legacy.ini").c_str(), TRUE))
      Log("saves: per-save inventories start now; your current guns stay with every save made so far (saves\\legacy.ini)");
    return;
  }
  wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring root(exe); root = root.substr(0, root.find_last_of(L'\\')) + L"\\Boat Crew_Data\\GameData\\GameSaves\\Campaign";
  std::set<unsigned long long> live;
  std::vector<std::wstring> dirs = { root };
  WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW((root + L"\\SaveSlot*").c_str(), &fd);
  if (fh != INVALID_HANDLE_VALUE) { do if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs.push_back(root + L"\\" + fd.cFileName); while (FindNextFileW(fh, &fd)); FindClose(fh); }
  for (const std::wstring& d : dirs) {
    fh = FindFirstFileW((d + L"\\*.BCS").c_str(), &fd);
    if (fh == INVALID_HANDLE_VALUE) continue;
    do { unsigned long long h; if (HashFile(d + L"\\" + fd.cFileName, &h)) live.insert(h); } while (FindNextFileW(fh, &fd));
    FindClose(fh);
  }
  if (live.empty()) return;                                            // no saves found: the path is wrong, delete nothing
  // A copy whose save is missing is only deleted after 30 days: the save may be in a backup or not synced back yet
  // (review 2026-10-29: restoring a backed-up save found its copy pruned).
  FILETIME nowFt; GetSystemTimeAsFileTime(&nowFt);
  unsigned long long now = ((unsigned long long)nowFt.dwHighDateTime << 32) | nowFt.dwLowDateTime;
  const unsigned long long kKeep = 30ULL * 24 * 3600 * 10000000ULL;    // 30 days in 100 ns units
  int removed = 0, kept = 0, waiting = 0;
  WIN32_FIND_DATAA fa; HANDLE ah = FindFirstFileA((dir + "\\*").c_str(), &fa);   // <hash>.ini inventories and <hash>.crew crew files
  if (ah == INVALID_HANDLE_VALUE) return;
  do {
    unsigned long long h; char tail[8] = ""; size_t len = strlen(fa.cFileName);
    if ((len != 20 && len != 21) || sscanf(fa.cFileName, "%16llx%5s", &h, tail) != 2 || (_stricmp(tail, ".ini") != 0 && _stricmp(tail, ".crew") != 0)) continue;   // legacy.ini
    if (live.count(h)) { kept++; continue; }
    unsigned long long wt = ((unsigned long long)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
    if (now < wt || now - wt < kKeep) { waiting++; continue; }
    if (DeleteFileA((dir + "\\" + fa.cFileName).c_str())) removed++;
  } while (FindNextFileA(ah, &fa));
  FindClose(ah);
  DLog("saves: %d inventory and crew copies match a save file, %d wait for theirs (under 30 days), %d older ones removed", kept, waiting, removed);
}

static void KeepLoot(Item& it, int x, int y) {
  it.held = false; it.gx = x; it.gy = y;
  ListPositions(it);
  SavePositions(); SaveInventory();
  InjectBoats();                                                       // onto the boats now
  g_refreshGunSlots = true;                                            // and the open loadout screen rebuilds its gun slots next frame
  Notice("Kept " + Colored(it.name, RarityHex(it.rarity)) + ". Click a matching weapon slot to equip it.");
}

static void DropDragged(const InvLayout& L, Vec2 m) {
  Item* it = ItemByName(g_dragName);
  if (!it || it->retired) return;
  if (Inside(m, L.scrap)) {
    if (!it->held && std::count(g_invEquipped.begin(), g_invEquipped.end(), it->name)) { Notice("Unequip " + Colored(it->name, RarityHex(it->rarity)) + " before scrapping it."); return; }
    it->retired = true; it->gx = it->gy = -1;
    int t = 0, got = ScrapYield(it->rarity, &t);
    g_mat[t] += got;
    SaveInventory();
    if (!it->held) {                                                   // a kept gun: off the gun lists now, not at the next rebuild
      int n = UnlistItem(it->name);
      g_refreshGunSlots = true;
      DLog("scrap: '%s' taken out of %d slot lists; gun slots refill next frame", it->name.c_str(), n);
    }
    Notice("Scrapped " + Colored(it->name, RarityHex(it->rarity)) + ": +" + std::to_string(got) + " " + kMatName[t] + ".");
    return;
  }
  bool loot; int x, y;
  if (!DropTarget(L, m, &loot, &x, &y)) return;                        // dropped outside: it goes back where it was
  if (!DropAllowed(*it, loot, x, y)) { if (loot && !it->held) Notice("Kept guns cannot go back to the loot grid."); return; }
  if (it->held && !loot) { KeepLoot(*it, x, y); return; }
  it->gx = x; it->gy = y;
  SaveInventory();
}

static void* g_white = nullptr;                                        // Texture2D.whiteTexture, for plain fills
static void Fill(Rect4 r) { if (!g_white) return; void* d[2] = { &r, g_white }; Invoke(m_guiDrawTex, nullptr, d); }

