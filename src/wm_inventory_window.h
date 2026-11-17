// inventory cap, uses per gun, the inventory window (IMGUI grid) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- inventory cap and uses per gun ---------------------------------------------------------------------------------
// Slot families of the boat on the loadout screen (if open) or the player's boat, by each weapon system's SelectionOrder.
static std::vector<std::string> BoatFamilies() {
  std::vector<std::string> out;
  if (g_offCraftWM < 0 || g_offWMSystems < 0 || g_offOrder < 0) return out;
  void* craft = g_ccm ? *(void**)((char*)g_ccm + g_offSelCraft) : nullptr;
  if (!craft) {
    craft = PlayerCraft();
  }
  if (!craft) return out;
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int n = ArrLen(systems);
  for (int i = 0; i < n && i < 32; i++) {
    void* ws = ArrAt(systems, i);
    void* order = ws ? *(void**)((char*)ws + g_offOrder) : nullptr;
    if (!order) continue;
    std::string f = FamilyOf(order);
    if (std::find(out.begin(), out.end(), f) == out.end()) out.push_back(f);
  }
  TorpFamiliesOf(craft, out);                                          // and its torpedo launchers
  return out;
}

// Names of the guns selected on the player's boat right now (empty until the boat exists and its loadout is applied).
// craft: the boat to read; null means the player's boat in battle (Player.playerCraft). The loadout screen edits its own
// boat (CraftCustomizationManager.selectedCraft), and at port playerCraft does not show that loadout.
static std::vector<std::string> EquippedNames(void* craft = nullptr) {
  std::vector<std::string> out;
  if (g_offCraftWM < 0 || g_offWMSystems < 0 || g_offWsSelected < 0) return out;
  if (!craft) {
    craft = PlayerCraft();
  }
  if (!craft) return out;
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int n = ArrLen(systems);
  if (n > 32) n = 32;
  for (int i = 0; i < n; i++) {
    void* ws = ArrAt(systems, i);
    void* sel = ws ? *(void**)((char*)ws + g_offWsSelected) : nullptr;
    if (!sel) continue;
    std::string nm = NameOf(sel);
    nm = StripTags(nm);
    if (nm != "None") out.push_back(nm);
  }
  TorpEquipped(craft, out);                                            // found torpedoes selected in its launchers
  return out;
}

// A found gun spends one use per slot the first time it fires in a battle, so a gun that never fired (the AI sank
// everything first) costs nothing. Each slot pays on its own, so one good drop still cannot fill every slot for free.
static std::vector<std::string> g_battleEquipped;                   // found guns with uses, equipped at this battle's start
static std::unordered_map<std::string, int> g_battleSpent;          // name -> slots that fired and paid this battle
static void OnMissionStart(const std::vector<std::string>& eq) {
  g_battleEquipped.clear();                                           // g_battleSpent: a gun may fire before this runs
  for (const Item& it : g_items)
    if (it.fromInventory && !it.retired && it.uses > 0 && std::count(eq.begin(), eq.end(), it.name)) g_battleEquipped.push_back(it.name);
  DLog("mission start: %d guns equipped, %d of them found guns that wear; each slot spends a use when it first fires", (int)eq.size(), (int)g_battleEquipped.size());
}
static Item* ItemByName(const std::string& name);
static std::unordered_set<void*> g_firedMounts;                      // mounts that already fired this battle
static std::unordered_map<void*, DWORD> g_mountShotAt;               // mount -> tick of its last shot this battle (newsreel deck shots)
static void SpendUseOnShot(void* mount) {
  if (g_usesBase <= 0) return;                                         // drops.ini uses=0: guns never wear out
  if (!g_firedMounts.insert(mount).second || !OnPlayerBoat(mount) || IsRemoteMount(mount) || IsHiddenMount(mount)) return;
  Item* it = ItemByName(StripTags(NameOf(mount)));
  if (!it || !it->fromInventory || it->retired || it->uses <= 0) return;
  it->uses--; g_battleSpent[it->name]++;
  if (it->uses <= 0) {
    int t = 0, got = ScrapYield(it->rarity, &t);                      // the same as scrapping it
    it->uses = 0; it->retired = true; g_mat[t] += got;
    Log("worn out: '%s' (%s) fired its last mission, +%d %s", it->name.c_str(), it->rarity.empty() ? "Common" : it->rarity.c_str(), got, kMatName[t]);
    ShowMessage("Worn out after this battle: " + it->name + " (+" + std::to_string(got) + " " + kMatName[t] + ")");
  }
  else Log("'%s' fired, so this battle costs it a mission (%d slot(s) fired it this battle, one mission each); %d mission(s) left", it->name.c_str(), g_battleSpent[it->name], it->uses);
  SaveInventory();
}
static void SafeSpendUse(void* mount) {                              // its own guard: a fault elsewhere in CritScan must not stop wear
  static bool off = false;
  if (off) return;
  __try { SpendUseOnShot(mount); } __except (EXCEPTION_EXECUTE_HANDLER) { off = true; Log("uses: a fault while counting a shot; found guns stop wearing out this session"); }
}
static void UsesSummary() {                                           // at battle end
  std::string idle;
  for (const std::string& n : g_battleEquipped) if (!g_battleSpent.count(n)) idle += (idle.empty() ? "'" : ", '") + n + "'";
  if (!g_battleEquipped.empty()) Log("uses, last battle: %d found gun(s) fired and paid a mission; never fired, so no mission spent: %s",
                                    (int)g_battleSpent.size(), idle.empty() ? "none" : idle.c_str());
  g_battleEquipped.clear(); g_battleSpent.clear(); g_firedMounts.clear();
}

// Called every frame by the ProjectileManager hook. The ProjectileManager lives for the whole game (it starts once per
// launch), so a battle is detected by WorldEngagementManager.instance instead:
// a new non-null instance is a new battle. Then wait until the player's boat has guns selected, a little longer, and
// count the uses.
static void* g_missionEngagement = nullptr;
static int g_missionFrames = 0, g_missionReadyAt = -1, g_missionFaults = 0;
static bool g_missionDone = true;
static void* EngagementInstance() {
  static void* f = nullptr; static bool looked = false;
  if (!looked) {
    looked = true;
    void* k = FindClass("Core.World", "WorldEngagementManager");
    f = k ? il2cpp_class_get_field_from_name(k, "instance") : nullptr;
    if (!f) Log("mission counting: WorldEngagementManager.instance not found; uses are not counted");
  }
  void* v = nullptr;
  if (f) il2cpp_field_static_get_value(f, &v);
  return v;
}
static void MissionWatch(void* manager) {
  (void)manager;
  void* eng = EngagementInstance();
  if (eng != g_missionEngagement) {
    g_missionEngagement = eng;
    if (eng) { g_missionFrames = 0; g_missionReadyAt = -1; g_missionDone = false; DLog("battle detected (engagement %p)", eng); }
    else { g_missionDone = true; DLog("battle over"); }
  }
  if (g_missionDone) return;
  if (++g_missionFrames > 1800) { g_missionDone = true; Log("mission start: no equipped guns found; uses not counted"); return; }
  if (g_missionFrames % 15 != 0) return;
  std::vector<std::string> eq = EquippedNames();
  if (eq.empty()) return;
  if (g_missionReadyAt < 0) { g_missionReadyAt = g_missionFrames + 90; return; }
  if (g_missionFrames < g_missionReadyAt) return;
  g_missionDone = true;
  OnMissionStart(eq);
}

// ---- inventory window (Diablo 2 style grid) ------------------------------------------------------------------------
// Drawn with Unity IMGUI from an OnGUI handler the game already has. CharacterManager.OnGUI runs on every screen (title,
// map, loadout, battle); the other hosts are fallbacks. The window only shows on the loadout screen: a button at the
// top left or the I key opens it.
struct Rect4 { float x, y, w, h; };
struct Col4 { float r, g, b, a; };
struct Vec2 { float x, y; };
struct GuiHost { const char* ns; const char* cls; void (*orig)(void*, void*); };
static GuiHost g_guiHosts[] = {
  { "Core", "CharacterManager", nullptr }, { "Core", "TitleScreenManagerUI", nullptr }, { "Core", "DeveloperControls", nullptr },
  { "Core.Campaign", "CampaignManager", nullptr }, { "Core.Record", "RecordManager", nullptr }, { "BoatCrewWorld.Unity", "WorldMapDebugGUI", nullptr } };
static const int kGuiHosts = 6;
static int g_guiDrawHost = -1, g_guiFaults = 0;
static DWORD g_guiLastDraw = 0;
static void *m_guiLabel = nullptr, *m_guiBox = nullptr, *m_guiButton = nullptr, *m_guiSetColor = nullptr, *m_guiDrawTex = nullptr;
static void *m_guiBeginGroup = nullptr, *m_guiEndGroup = nullptr, *g_guiContentNone = nullptr, *g_guiStyleNone = nullptr;
static void *m_evCurrent = nullptr, *m_evType = nullptr, *m_evKey = nullptr, *m_evMouse = nullptr, *m_evUse = nullptr, *m_evButton = nullptr;
static void *m_scrW = nullptr, *m_scrH = nullptr, *m_spriteTex = nullptr, *m_spriteRect = nullptr, *m_texW = nullptr, *m_texH = nullptr;
static void *m_mountSprite = nullptr, *m_mountProps = nullptr, *m_guiSetMatrix = nullptr;
static bool g_invOpen = false, g_invKeyHeld = false, g_invReady = false;
struct SpriteEntry { void* sprite; DWORD tried; };
static std::map<std::string, SpriteEntry> g_spriteCache;
static std::map<std::string, std::string> g_propsCache;
static std::vector<std::string> g_invEquipped;
static DWORD g_invEquippedAt = 0;

static int Unbox(void* o) { return o ? *(int*)((char*)o + 0x10) : 0; }
static void GuiColor(Col4 c) { void* a[1] = { &c }; Invoke(m_guiSetColor, nullptr, a); }
static void GuiBox(Rect4 r) { void* a[2] = { &r, il2cpp_string_new("") }; Invoke(m_guiBox, nullptr, a); }
static void GuiLabel(Rect4 r, const std::string& t) { void* a[2] = { &r, il2cpp_string_new(t.c_str()) }; Invoke(m_guiLabel, nullptr, a); }
static bool GuiButton(Rect4 r, const char* t) { void* a[2] = { &r, il2cpp_string_new(t) }; void* res = Invoke(m_guiButton, nullptr, a); return res && *(bool*)((char*)res + 0x10); }
static bool Inside(Vec2 m, Rect4 r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; }
static Col4 RarityCol(const std::string& r) { if (r == "Unusual") return { 1.0f, 0.37f, 0.78f, 1 }; if (r == "Unique") return { 1.0f, 0.48f, 0.10f, 1 }; if (r == "Rare") return { 1.0f, 0.70f, 0.0f, 1 }; if (r == "Uncommon") return { 0.31f, 0.76f, 0.97f, 1 }; return { 0.85f, 0.85f, 0.85f, 1 }; }

// A live mount of the gun (any boat): the source of its picture and stat list. Mounts are not kept, only the sprite
// (an asset) and the stat text.
static void* FindLiveMount(const std::string& name) {
  void* reg = LiveRegister();
  for (int i = 0; i < ListCount(reg); i++) {
    void* ws = ListAt(reg, i); if (!ws) continue;
    void* mounts = *(void**)((char*)ws + g_offMounts);
    for (int j = 0; j < ListCount(mounts); j++) { void* m = ListAt(mounts, j); if (m && StripTags(NameOf(m)) == name) return m; }
  }
  return nullptr;
}
static void* BaseSprite(const std::string& base);
static void* ItemSprite(const Item& it) {
  if (IsTorpFamily(it.family)) return TorpSprite(it.base);             // the stock torpedo's picture (TorpedoType.sprite)
  const std::string& name = it.name;
  SpriteEntry& e = g_spriteCache[name];
  if (e.sprite) return e.sprite;
  // between retries a loot gun keeps the stock picture (returning e.sprite would draw the bare name and flicker)
  if (e.tried && GetTickCount() - e.tried < 3000) return BaseSprite(it.base);
  e.tried = GetTickCount();
  void* m = FindLiveMount(name);
  e.sprite = m && m_mountSprite ? Invoke(m_mountSprite, m) : nullptr;
  if (e.sprite) Pin(e.sprite);                                         // kept for the session (sprites are assets)
  else { e.tried = GetTickCount(); return BaseSprite(it.base); }       // a loot gun is on no boat yet: the stock gun's picture, not
                                                                       // cached, so its own picture and stat lines come once it is kept
  if (m && !g_propsCache.count(name) && m_mountProps) {                // the game's own stat lines, List<(string, string)>
    void* list = Invoke(m_mountProps, m);
    std::string t;
    for (int i = 0; i < ListCount(list) && i < 20; i++) {
      void* arr = *(void**)((char*)list + 0x10);
      // raw on purpose: each element is a (string, string) ValueTuple, 16 bytes, so ArrAt's 8-byte stride does not fit
      void* k = *(void**)((char*)arr + 0x20 + 16 * (size_t)i); void* v = *(void**)((char*)arr + 0x28 + 16 * (size_t)i);
      t += std::string(Str(k)) + ": " + Str(v) + "\n";
    }
    g_propsCache[name] = t;
  }
  return e.sprite;
}

// Sprites share atlas textures and GUI.DrawTextureWithTexCoords is stripped from this build, so the whole texture is
// drawn inside a clipping group, shifted so only the sprite's rectangle shows.
static void DrawSprite(void* sprite, Rect4 cell) {
  void* tex = Invoke(m_spriteTex, sprite);
  void* rb = Invoke(m_spriteRect, sprite);
  if (!tex || !rb) return;
  Rect4 sr = *(Rect4*)((char*)rb + 0x10);
  float tw = (float)Unbox(Invoke(m_texW, tex)), th = (float)Unbox(Invoke(m_texH, tex));
  if (tw <= 0 || th <= 0 || sr.w <= 0 || sr.h <= 0) return;
  float pad = 4, aw = cell.w - 2 * pad, ah = cell.h - 2 * pad;
  float sc = aw / sr.w < ah / sr.h ? aw / sr.w : ah / sr.h;
  Rect4 clip = { cell.x + pad + (aw - sr.w * sc) / 2, cell.y + pad + (ah - sr.h * sc) / 2, sr.w * sc, sr.h * sc };
  void* g[3] = { &clip, g_guiContentNone, g_guiStyleNone };
  Invoke(m_guiBeginGroup, nullptr, g);
  Rect4 full = { -sr.x * sc, -(th - sr.y - sr.h) * sc, tw * sc, th * sc };   // sprite rects count y from the bottom
  void* d[2] = { &full, tex };
  Invoke(m_guiDrawTex, nullptr, d);
  Invoke(m_guiEndGroup, nullptr);
}

static std::string ItemTooltip(const Item& it) {
  int w, h; ItemSize(it, &w, &h);
  std::string t = "<b>" + Colored(it.name, RarityHex(it.rarity)) + "</b>\n" + it.description + "\n\n";
  t += it.family + ", " + std::to_string(w) + "x" + std::to_string(h) + " cells\n";
  if (it.held) t += "In the loot grid: drag it into your guns to keep it.\n";
  if (it.uses > 0) t += "Missions left: " + std::to_string(it.uses) + " (a slot spends one in a battle where it fires)\n";
  int eq = (int)std::count(g_invEquipped.begin(), g_invEquipped.end(), it.name);
  if (eq) t += "Equipped in " + std::to_string(eq) + " slot" + (eq > 1 ? "s" : "") + "\n";
  auto p = g_propsCache.find(it.name);
  if (p != g_propsCache.end() && !p->second.empty()) t += "\n" + p->second;
  return t;
}

// Where everything sits, as a field crate: a stencil
// band, the manifest row (the drag handle), YOUR GUNS on the left with three material slips under it, LOOT on the right
// with the scrap box under it, a paper footer and a stencil line. The default grids (10x6 guns, 8x8 loot) make it
// 1008 x 668: no wasted wood, and it fits a 720 px tall screen with the toggle buttons above it.
// The window can be dragged by its title area; its spot is kept in ui.ini in unscaled units (see g_uiScale).
static float g_uiScale = 1.0f;                                      // GUI scale in use (drops.ini ui_scale, or screen height / 1080)
static float g_winX = -1, g_winY = -1;                              // -1: bottom center, 200 screen pixels up
static float g_virtW = 1920, g_virtH = 1080;                        // screen size divided by the scale
// slips: the workshop as three slips stacked under YOUR GUNS; false (a guns grid under 8 columns is too narrow for a
// slip's text and tags) puts the three side by side in a strip under both grids. shop is their area.
struct InvLayout { float cell, ix, iy, lx, ly; Rect4 panel, scrap, title, close, toggle, shop, foot, scrapAll; bool slips; };
static const float kSlipH = 44, kSlipGap = 8, kStripSlipH = 70;   // strip slips are two rows: text, then the tags
static InvLayout GetLayout() {
  InvLayout L; L.cell = 52;
  const float pad = 20, band = 34, head = 46, sec = 42, gap = 32, scrapH = 46, footH = 28, lotH = 22;
  float iw = g_gridCols * L.cell, lw = g_lootCols * L.cell, slipsH = 3 * kSlipH + 2 * kSlipGap;
  float gunsH = g_gridRows * L.cell, lootH = g_lootRows * L.cell + 10 + scrapH;
  float stackBody = (std::max)(gunsH + 18 + slipsH, lootH), stripBody = (std::max)(gunsH, lootH) + 18 + kStripSlipH;
  L.slips = g_gridCols >= 8 && stackBody <= stripBody;              // stack under YOUR GUNS only when it is no taller than the strip
  float gridH = gunsH + (L.slips ? 18 + slipsH : 0), bodyH = gridH > lootH ? gridH : lootH;
  float stripH = L.slips ? 0 : 18 + kStripSlipH;
  float pw = pad + iw + gap + lw + pad, ph = band + head + sec + bodyH + stripH + 6 + footH + lotH + 12;
  float ox = g_winX, oy = g_winY;
  if (ox < 0 || oy < 0) { ox = (g_virtW - pw) / 2; oy = g_virtH - ph - 200 / g_uiScale; }
  if (ox > g_virtW - 60) ox = g_virtW - 60; if (oy > g_virtH - 40) oy = g_virtH - 40;
  if (ox < 0) ox = 0; if (oy < 34) oy = 34;                          // keep the toggle button above it on screen
  L.panel = { ox, oy, pw, ph };
  L.title = { ox, oy, pw, band + head };                              // stencil band and manifest row: drag it to move the window
  L.close = { ox + pw - pad - 112, oy + band + (head - 32) / 2, 112, 32 };
  L.toggle = { ox, oy - 34, 160, 28 };                                // shown while the window is closed
  L.ix = ox + pad; L.iy = oy + band + head + sec; L.lx = L.ix + iw + gap; L.ly = L.iy;
  L.scrap = { L.lx - 1, L.ly + g_lootRows * L.cell + 10, lw + 1, scrapH };
  L.scrapAll = { L.lx + lw - 120, L.ly - 29, 120, 22 };                  // in the LOOT header row, right
  L.shop = L.slips ? Rect4{ L.ix - 1, L.iy + g_gridRows * L.cell + 18, iw + 1, slipsH } : Rect4{ L.ix - 1, L.iy + bodyH + 18, pw - 2 * pad + 2, kStripSlipH };
  L.foot = { L.ix, L.iy + bodyH + stripH + 6, pw - 2 * pad, footH };
  return L;
}
static void SaveUi() {
  std::string path = g_folder + "\\ui.ini";
  FILE* f = OpenForSave(path);
  if (!f) return;
  fprintf(f, "# Written by BoatCrewWeaponMod: where the inventory window sits. Delete this file to put it back at the bottom center.\nwindow=%.0f,%.0f\n", g_winX, g_winY);
  FinishSave(f, path);
}
static void LoadUi() {
  FILE* f = fopen((g_folder + "\\ui.ini").c_str(), "r");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof line, f)) { float x, y; if (sscanf(line, "window=%f,%f", &x, &y) == 2) { g_winX = x; g_winY = y; } }
  fclose(f);
}
static Rect4 ItemRect(const InvLayout& L, const Item& it) {
  int w, h; ItemSize(it, &w, &h);
  float ox = it.held ? L.lx : L.ix, oy = it.held ? L.ly : L.iy;
  return { ox + it.gx * L.cell, oy + it.gy * L.cell, w * L.cell - 1, h * L.cell - 1 };   // between the 1 px grid lines
}
static Item* ItemByName(const std::string& name) { for (Item& it : g_items) if (it.name == name) return &it; return nullptr; }

static std::string g_dragName, g_invNotice;
static std::string g_invHideName;                                     // a crate's gun stays out of the loot grid until its reel is closed
static int g_dragOX = 0, g_dragOY = 0;
static DWORD g_invNoticeAt = 0;
static void Notice(const std::string& t) { g_invNotice = t; g_invNoticeAt = GetTickCount(); Log("inventory: %s", StripTags(t).c_str()); }

// The grid cell the dragged gun's top-left corner would land on, if the mouse is over a grid.
static bool DropTarget(const InvLayout& L, Vec2 m, bool* loot, int* x, int* y) {
  for (int g = 0; g < 2; g++) {
    bool lt = g == 1;
    float ox = lt ? L.lx : L.ix, oy = lt ? L.ly : L.iy;
    if (!Inside(m, { ox, oy, GridCols(lt) * L.cell, GridRows(lt) * L.cell })) continue;
    *loot = lt; *x = (int)floorf((m.x - ox) / L.cell) - g_dragOX; *y = (int)floorf((m.y - oy) / L.cell) - g_dragOY;
    return true;
  }
  return false;
}
static bool DropAllowed(const Item& it, bool loot, int x, int y) {
  if (loot && !it.held) return false;                                // a kept gun cannot go back to the loot grid
  int w, h; ItemSize(it, &w, &h);
  return GridFree(loot, x, y, w, h, &it);
}

// WeaponSystem.register does not hold every slot (the loadout screen's Aft heavy slot can be missing), so after the
// register come every slot of the boat on the loadout screen and of the player's boat, each system once.
static std::vector<void*> SlotSystems() {
  std::vector<void*> out;
  auto add = [&](void* ws) { if (ws && std::find(out.begin(), out.end(), ws) == out.end()) out.push_back(ws); };
  void* reg = LiveRegister();
  for (int i = 0; i < ListCount(reg); i++) add(ListAt(reg, i));
  void* crafts[2] = { g_ccm ? *(void**)((char*)g_ccm + g_offSelCraft) : nullptr, PlayerCraft() };
  for (void* craft : crafts) {
    if (!craft || g_offCraftWM < 0 || g_offWMSystems < 0) continue;
    void* wm = *(void**)((char*)craft + g_offCraftWM);
    void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
    int n = ArrLen(systems);
    for (int i = 0; i < n && i < 32; i++) add(ArrAt(systems, i));
  }
  return out;
}
static void InjectBoats() { for (void* ws : SlotSystems()) SafeInject(ws); InjectTorpBoats(); }

