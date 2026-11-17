// inventory grid limit, boats that already exist, the RP crate pickup watcher (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- inventory grid (Diablo 2 style): the grid is the inventory limit -----------------------------------------------
// Each found gun covers a rectangle of cells by gun type. A drop goes to the first free spot, row by row; if there is
// none it is left behind. Retired guns free their cells. Guns from weapons.ini are not in the grid.
static void ItemSize(const Item& it, int* w, int* h) {
  if (it.gw > 0 && it.gh > 0) { *w = it.gw; *h = it.gh; return; }
  if (IsTorpFamily(it.family)) { *w = 4; *h = 1; return; }            // a torpedo: long and thin
  auto has = [&](const char* t) { return it.base.find(t) != std::string::npos; };
  if (has("3-inch") || has("5-inch") || has("Dual Bofors")) { *w = 4; *h = 2; }
  else if (has("Mousetrap") || (has("Launcher") && !has("Grenade")) || has("107 mm") || has("75 mm")) { *w = 3; *h = 2; }
  else if (has("60 mm")) { *w = 2; *h = 2; }
  else if (has("Bazooka")) { *w = 3; *h = 1; }
  else if (has("Bofors") || has("Twin") || has("Chopper") || has("Elco")) { *w = 3; *h = 2; }
  else if (has("37 mm") || has("20 mm")) { *w = 3; *h = 1; }
  else { *w = 2; *h = 1; }
}
static int GridCols(bool loot) { return loot ? g_lootCols : g_gridCols; }
static int GridRows(bool loot) { return loot ? g_lootRows : g_gridRows; }
static bool GridFree(bool loot, int x, int y, int w, int h, const Item* skip) {
  if (x < 0 || y < 0 || x + w > GridCols(loot) || y + h > GridRows(loot)) return false;
  for (const Item& o : g_items) {
    if (&o == skip || !o.fromInventory || o.retired || o.gx < 0 || o.held != loot) continue;
    int ow, oh; ItemSize(o, &ow, &oh);
    if (x < o.gx + ow && o.gx < x + w && y < o.gy + oh && o.gy < y + h) return false;
  }
  return true;
}
static bool GridPlace(Item& it) {
  int w, h; ItemSize(it, &w, &h);
  for (int y = 0; y + h <= GridRows(it.held); y++)
    for (int x = 0; x + w <= GridCols(it.held); x++)
      if (GridFree(it.held, x, y, w, h, &it)) { it.gx = x; it.gy = y; return true; }
  it.gx = it.gy = -1;
  return false;
}
// At launch: keep saved spots that are still valid, then place the rest, biggest first. Guns that do not fit stay
// usable in loadouts but have no cell (the window counts them), so nothing already found is lost by the change.
static void GridLayout() {
  bool changed = false;
  std::vector<Item*> todo;
  for (Item& it : g_items) {
    if (!it.fromInventory || it.retired) continue;
    if (it.gx >= 0) { int w, h; ItemSize(it, &w, &h); if (GridFree(it.held, it.gx, it.gy, w, h, &it)) continue; it.gx = it.gy = -1; changed = true; }
    todo.push_back(&it);
  }
  std::stable_sort(todo.begin(), todo.end(), [](Item* a, Item* b) { int aw, ah, bw, bh; ItemSize(*a, &aw, &ah); ItemSize(*b, &bw, &bh); return aw * ah > bw * bh; });
  int overflow = 0;
  for (Item* it : todo) {
    if (GridPlace(*it)) { changed = true; continue; }
    if (it->held) {                                                    // a loot gun with no cell could never be dragged or kept
      it->held = false;
      if (GridPlace(*it)) { ListPositions(*it);
                            Log("loot '%s' had no cell in the loot grid; moved to your guns", it->name.c_str()); }
      else { it->retired = true; Log("loot '%s' had no cell in either grid; left behind", it->name.c_str()); }
      changed = true; continue;
    }
    overflow++;
  }
  Log("inventory grid %dx%d: %d guns placed now, %d do not fit (they stay usable)", g_gridCols, g_gridRows, (int)todo.size() - overflow, overflow);
  if (changed) { SaveInventory(); SavePositions(); }
}

// A loot gun stays out of positions.ini and off the boats until it is dragged into the inventory grid, so dropping one
// never shifts a position. Loot stays across battles; when a new drop does not fit, the oldest loot makes room
// (David, 2026-10-24: leaving it all behind at the next battle lost guns from a battle with no drops after it).
static void ShowMessage(const std::string& text);
static bool MakeRoomInLoot(Item& incoming, std::vector<std::string>* dropped) {
  { int w, h; ItemSize(incoming, &w, &h); if (w > g_lootCols || h > g_lootRows) return false; }   // too big for the grid: drop nothing
  for (;;) {
    if (GridPlace(incoming)) return true;
    Item* oldest = nullptr;                                            // g_items keeps the order guns were found in
    for (Item& o : g_items) if (o.fromInventory && o.held && !o.retired) { oldest = &o; break; }
    if (!oldest) return false;
    oldest->retired = true; oldest->gx = oldest->gy = -1;
    dropped->push_back(oldest->name);
    Log("loot grid full: '%s' left behind to make room for '%s'", oldest->name.c_str(), incoming.name.c_str());
  }
}

static void AddToInventory(const Item& it) {
  g_items.push_back(it);
  if (it.held) { SaveInventory(); return; }
  ListPositions(it);
  SavePositions();
  SaveInventory();
}

// A text line in the game's kill feed (KillFeedUI.triggerScore(text, score, fresh, kill)).
static void ShowMessage(const std::string& text) {
  void* kc = FindClass("Core", "KillFeedUI");
  void* fInst = kc ? il2cpp_class_get_field_from_name(kc, "instance") : nullptr;
  void* m = kc ? FindMethod(kc, "triggerScore", 4) : nullptr;
  if (!fInst || !m) { Log("kill feed: KillFeedUI.instance or triggerScore not found"); return; }
  void* inst = nullptr; il2cpp_field_static_get_value(fInst, &inst);
  if (!inst) { Log("kill feed: no instance right now, message not shown: %s", text.c_str()); return; }
  int score = 1; bool fresh = false, kill = false;                    // the feed prints this as '+1', which reads as one item added
  void* a[4] = { il2cpp_string_new(text.c_str()), &score, &fresh, &kill };
  Invoke(m, inst, a);
}

// ---- boats that already exist -----------------------------------------------------------------------------------
// Boats are only given the extra guns when they are built. A gun that drops later would be missing from every boat
// built earlier, and a loadout that points at its position could not be applied to those boats. So a new drop is also
// added to all live weapon systems (WeaponSystem.register); Inject skips guns a system already has.
static void* LiveRegister() {
  static void* f = nullptr;
  if (!f) { void* wsc = FindClass("Core", "WeaponSystem"); f = wsc ? il2cpp_class_get_field_from_name(wsc, "register") : nullptr; }
  void* reg = nullptr;
  if (f) il2cpp_field_static_get_value(f, &reg);
  return reg;
}

static void LogRegisterSummary(const char* why) {
  void* reg = LiveRegister();
  int n = ListCount(reg);
  std::map<std::string, std::map<int, int>> groups;                   // family -> mounts count -> how many systems
  for (int i = 0; i < n; i++) {
    void* ws = ListAt(reg, i); if (!ws) continue;
    void* order = *(void**)((char*)ws + g_offOrder); void* mounts = *(void**)((char*)ws + g_offMounts);
    if (!order || !mounts) continue;
    groups[FamilyOf(order)][ListCount(mounts)]++;
  }
  DLog("LIVE[%s]: %d weapon systems", why, n);
  for (auto& fam : groups) {
    std::string line;
    for (auto& c : fam.second) line += std::to_string(c.second) + " x " + std::to_string(c.first) + " mounts   ";
    DLog("LIVE[%s]   family '%s': %s", why, fam.first.c_str(), line.c_str());
  }
}

static void SafeInject(void* ws) {
  __try { Inject(ws); }
  __except (EXCEPTION_EXECUTE_HANDLER) { Log("fault while adding a new gun to a live weapon system %p", ws); }
}


static DWORD g_lastCrateTick = 0;
// auto_scrap: a drop of that rarity or lower goes straight to its material, before the loot grid makes room for it.
// Never Uniques or torpedoes; crates opened at port do not come through here.
static bool AutoScrapDrop(const Item& it, const std::string& rarity, const char* what) {
  int rq = RarityOf(rarity);
  if (g_autoScrap < 0 || rq > 2 || rq > g_autoScrap || IsTorpFamily(it.family)) return false;
  int t = 0, got = ScrapYield(rarity, &t);
  g_mat[t] += got; SaveInventory();
  Log("%s (auto-scrapped, auto_scrap=%s): '%s' (%s) in '%s' based on '%s' -> +%d %s", what, kAutoScrapNames[g_autoScrap + 1], it.name.c_str(), rarity.c_str(), it.family.c_str(), it.base.c_str(), got, kMatName[t]);
  if (g_dropMessage) ShowMessage("Scrapped: " + Colored(it.name, RarityHex(rarity)) + " (+" + std::to_string(got) + " " + kMatName[t] + ")");
  return true;
}
static void OnRpCollected(int count) {
  // One kill (or one sweep through wreckage) yields many crates. Crates picked up within burst_seconds of the previous
  // one belong to the same drop event and get no roll of their own, so 'chance' means a chance per event.
  DWORD now = GetTickCount();
  bool sameBurst = g_burstMs > 0 && g_lastCrateTick != 0 && now - g_lastCrateTick <= (DWORD)g_burstMs;
  g_lastCrateTick = now;
  if (sameBurst) { DLog("RP crate collected (%d RP): same pickup burst, no extra roll", count); return; }
  double roll = Rand01();
  DLog("RP crate collected (%d RP): drop roll %.3f against chance %.3f", count, roll, g_dropChance);
  if (roll >= g_dropChance) return;
  if (Rand01() < g_crateChance) {
    double tw = g_crateWeight[0] + g_crateWeight[1] + g_crateWeight[2], y = Rand01() * tw; int t = 0;
    while (t < 2 && y >= g_crateWeight[t]) { y -= g_crateWeight[t]; t++; }
    g_crates[t]++; SaveInventory();
    Log("DROP: an unopened %s (now %d)", kCrateName[t], g_crates[t]);
    if (g_dropMessage) ShowMessage(std::string("Found: ") + Colored(kCrateName[t], t == 2 ? "#ffb300" : (t == 1 ? "#4fc3f7" : "#d8d8d8")) + ", open it at port");
    return;
  }
  Item it; std::string rarity;
  bool unique = g_unusualChance > 0 && Rand01() < g_unusualChance && RollUnique(&it, &rarity, true);
  if (!unique) unique = g_uniqueChance > 0 && Rand01() < g_uniqueChance && RollUnique(&it, &rarity);
  if (!unique && !RollItem(&it, &rarity)) { Log("drop: nothing to roll (check drops.ini)"); return; }
  if (AutoScrapDrop(it, rarity, "DROP")) return;
  it.held = true;
  std::vector<std::string> dropped;
  if (!MakeRoomInLoot(it, &dropped)) {
    int w, h; ItemSize(it, &w, &h);
    Log("drop: '%s' (%dx%d) is bigger than the loot grid; left behind", it.name.c_str(), w, h);
    if (g_dropMessage) ShowMessage("Too big for the loot grid: " + Colored(it.name, RarityHex(rarity)));
    return;
  }
  if (!dropped.empty() && g_dropMessage) ShowMessage("Loot grid full: left behind " + dropped.front() + (dropped.size() > 1 ? " and " + std::to_string(dropped.size() - 1) + " more" : ""));
  AddToInventory(it);
  Log("DROP: '%s' (%s) in '%s' based on '%s': %s", it.name.c_str(), rarity.c_str(), it.family.c_str(), it.base.c_str(), it.description.c_str());
  if (g_dropMessage) ShowMessage("Found: " + Colored(it.name, RarityHex(rarity)) + " (" + rarity + "), in the loot grid");
}

// ---- pickup watcher: PickupBehaviour.Update runs pickUp(); an RP crate has pickup.item == none ----------------------
static int g_offPickup = -1, g_offPickedUp = -1, g_offPtItem = -1, g_offPtCount = -1;
static void (*g_pickupUpdateOriginal)(void*, void*) = nullptr;
static std::vector<void*> g_pickedSeen;                              // crates already counted (pooled crates reset pickedUp when reused)
static int g_pickupFaults = 0;

static void WatchPickup(void* pb) {
  bool picked = *(bool*)((char*)pb + g_offPickedUp);
  auto it = std::find(g_pickedSeen.begin(), g_pickedSeen.end(), pb);
  if (!picked) { if (it != g_pickedSeen.end()) g_pickedSeen.erase(it); return; }
  if (it != g_pickedSeen.end()) return;
  if (g_pickedSeen.size() > 512) g_pickedSeen.clear();
  g_pickedSeen.push_back(pb);
  void* pt = *(void**)((char*)pb + g_offPickup);
  if (!pt) return;
  int item = *(int*)((char*)pt + g_offPtItem), count = *(int*)((char*)pt + g_offPtCount);
  if (item != 0) return;                                              // health, repair and ammo crates are not RP
  OnRpCollected(count);
}

// Dave's crates: this frame's RP crates that are on the water and not picked up yet, noted as their Update runs (before
// the LateUpdate where UtilTick reads them); only dereferenced on that frame.
static int g_offResDrop = -1, g_offSplashed = -1;
static bool g_dropsOn = false;                                       // the random drops are set up (else the hook is only Dave's)
static std::vector<void*> g_crateFrame;
static void NoteCrate(void* pb) {
  if (g_offResDrop < 0 || g_offSplashed < 0 || *(bool*)((char*)pb + g_offPickedUp)) return;
  void* rd = *(void**)((char*)pb + g_offResDrop); void* pt = *(void**)((char*)pb + g_offPickup);
  if (!rd || !pt || !*(bool*)((char*)rd + g_offSplashed) || *(int*)((char*)pt + g_offPtItem) != 0) return;
  if (g_crateFrame.size() > 2048) g_crateFrame.clear();               // UtilTick is not running: nothing reads the list
  g_crateFrame.push_back(pb);
}
static void Hooked_PickupUpdate(void* self, void* method) {
  g_pickupUpdateOriginal(self, method);
  if (g_ready) { __try { NoteCrate(self); } __except (EXCEPTION_EXECUTE_HANDLER) { g_offSplashed = -1; Log("fault noting a crate for Dave; Dave stops picking crates up"); } }
  if (!g_ready || !g_dropsOn || g_dropChance <= 0) return;
  __try { WatchPickup(self); }
  __except (EXCEPTION_EXECUTE_HANDLER) { if (++g_pickupFaults <= 3) Log("fault in the pickup watcher (%d)", g_pickupFaults); if (g_pickupFaults >= 10) g_dropChance = 0; }
}

