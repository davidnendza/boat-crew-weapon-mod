// field crate look, the workshop strip and the crate opening reel (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- the field crate look for the inventory ----
// Birch plywood with a veneer edge, hairline seams and steel corner brackets; dark compartments for the grids; paper for
// the buttons, labels and slips; stencilled ink. The ply, brackets, ISSUED stamp, arrow, tile and shade are PNGs in
// models\ (tools\make_ui_assets.py), each with a flat fallback; the seams and the edge are drawn here so they stay crisp
// at any window size.
static const unsigned kPly = 0xcfae7c, kCompart = 0x22170d, kCompartRim = 0x6b4e2a, kCream = 0xf3e6c8, kIssuedRed = 0xc9372c;
static const char *kCreamHex = "#f3e6c8", *kStencilHex = "#2d1e0f", *kAmberHex = "#f0c050", *kFootHex = "#4a4232";
static const char* kDotSep = "   \xC2\xB7   ";                              // between letter-spaced words
// The ply body: shadow, face, the veneer edge (1 px layers, outside in), two hairline seams (seamX / seamY < 0: none),
// then the corner brackets flush with the corners. Compartments, slips and labels drawn after it cover the seams.
static void CrateBody(Rect4 p, float seamX, float seamY) {
  if (!DrawUi({ p.x - 24, p.y - 24, p.w + 48, p.h + 48 }, "ui_shadow.png")) FillC({ p.x + 4, p.y + 10, p.w, p.h }, Hex(0, 0.4f));
  if (!DrawUi(p, "ui_ply.png")) FillC(p, Hex(kPly));
  static const unsigned edge[5] = { 0x7a5a30, 0xe0c495, 0x9c7848, 0xe4cb9c, 0x8a6a3e };
  for (int k = 0; k < 5; k++) Frame({ p.x + k, p.y + k, p.w - 2 * k, p.h - 2 * k }, 1, Hex(edge[k]));
  if (seamX >= 0) { FillC({ seamX, p.y + 5, 1, p.h - 10 }, Hex(0x462d14, 0.55f)); FillC({ seamX + 1, p.y + 5, 1, p.h - 10 }, Hex(0xfff0d2, 0.25f)); }
  if (seamY >= 0) { FillC({ p.x + 5, seamY, p.w - 10, 1 }, Hex(0x462d14, 0.55f)); FillC({ p.x + 5, seamY + 1, p.w - 10, 1 }, Hex(0xfff0d2, 0.25f)); }
  static const char* br[4] = { "ui_bracket_tl.png", "ui_bracket_tr.png", "ui_bracket_bl.png", "ui_bracket_br.png" };
  for (int k = 0; k < 4; k++) {
    float x = k & 1 ? p.x + p.w - 44 : p.x, y = k & 2 ? p.y + p.h - 44 : p.y;
    if (DrawUi({ x, y, 44, 44 }, br[k])) continue;
    FillC({ x, k & 2 ? y + 31 : y, 44, 13 }, Hex(0x555b60)); FillC({ k & 1 ? x + 31 : x, y, 13, 44 }, Hex(0x555b60));   // flat: the L in two fills
  }
  GuiColor({ 1, 1, 1, 1 });
}
static void Stencil(Rect4 r, const std::string& t, int size, int align = kLeft) { g_textAlpha = 0.5f; Text(r, t, size, kStencilHex, align, true); g_textAlpha = 1; }
static void PaperSlip(Rect4 r, float shadow = 0.35f) { FillC({ r.x + 1, r.y + 2, r.w, r.h }, Hex(0, shadow)); FillC(r, Hex(kPaper)); }
// A dark compartment: fill, faint cream lines every cell px (0: none), an inner shadow at the top, a 1 px rim.
static void Compartment(Rect4 r, float cell) {
  FillC(r, Hex(kCompart));
  if (cell > 0) {
    GuiColor(Hex(kCream, 0.09f));
    for (float x = r.x + cell; x < r.x + r.w - 1; x += cell) Fill({ x, r.y, 1, r.h });
    for (float y = r.y + cell; y < r.y + r.h - 1; y += cell) Fill({ r.x, y, r.w, 1 });
  }
  DrawUi({ r.x + 1, r.y + 1, r.w - 2, 14 }, "ui_shade.png");          // without it the compartment is just flat
  Frame(r, 1, Hex(kCompartRim));
  GuiColor({ 1, 1, 1, 1 });
}
// A paper tag button: paper, a 2 px ink frame, ink text, highlighter on hover; filled = ink with paper text. Like
// StyledButton, an inactive tag is drawn (50% text) but not called, so the reel's buttons under it still get the click.
static bool CrateTag(Rect4 r, const std::string& label, int type, Vec2 mouse, bool active = true, int size = 11, bool filled = false) {
  bool click = active && HitBox(r);
  if (type == 7) {
    bool hot = active && Inside(mouse, r);
    FillC({ r.x + 1, r.y + 2, r.w, r.h }, Hex(0, 0.3f));
    if (filled) FillC(r, Hex(hot ? 0x3d3b35 : kInk));
    else { FillC(r, Hex(kPaper)); if (hot) FillC(r, Hilite()); Frame(r, 2, Hex(kInk)); }
    g_textAlpha = active ? 1.0f : 0.5f;
    Text(r, label, size, filled ? kPaperHex : kInkHex, kCenter, filled);
    g_textAlpha = 1;
  }
  return click;
}
static void DrawGrid(float ox, float oy, int cols, int rows, float cell) { Compartment({ ox - 1, oy - 1, cols * cell + 1, rows * cell + 1 }, cell); }
// A rarity tile: dark box, a see-through fill of the rarity color and a solid bar of it along the bottom (CS style).
// glow (0..1) brightens the fill, for the reel's winning tile.
static void RarityTile(Rect4 r, Col4 c, float alpha, float glow) {
  GuiColor({ 1, 1, 1, alpha }); GuiBox(r); GuiBox(r);
  if (!g_white) { c.a = alpha; GuiColor(c); GuiBox(r); return; }
  GuiColor({ c.r, c.g, c.b, (0.22f + 0.35f * glow) * alpha }); Fill(r);
  GuiColor({ c.r, c.g, c.b, alpha }); Fill({ r.x, r.y + r.h - 4, r.w, 4 });
}
// The rarity on a paper shipping tag: a band of the rarity color, a punched hole, the word. No text measuring: the
// widths are a table (measured in Arial Bold at 8 px bold plus a margin for Kanit, about 7 px a letter otherwise);
// tiles under 88 px get the band and the hole only. The word
// is not letter-spaced with spaces: that would double its width.
static void RarityTag(Rect4 r, const std::string& rarity, float alpha) {
  std::string w = Upper(rarity.empty() ? std::string("Common") : rarity);
  float tw = w == "COMMON" ? 44.0f : w == "UNCOMMON" ? 58.0f : w == "RARE" ? 27.0f : w == "UNIQUE" ? 38.0f : 7.0f * w.size();
  bool word = r.w >= 88;                                             // the widest tag (UNCOMMON, 79 px) ends at r.x + 83
  Rect4 t = { r.x + 4, r.y + 4, word ? 15 + tw + 6 : 16, 14 };
  FillC({ t.x + 1, t.y + 1, t.w, t.h }, Hex(0, 0.5f * alpha));
  FillC(t, Hex(kPaper, alpha));
  Col4 c = RarityCol(rarity); c.a = alpha; FillC({ t.x, t.y, 4, t.h }, c);
  FillC({ t.x + 8, t.y + 5, 4, 4 }, Hex(kCompart, alpha));
  if (word) Text({ t.x + 15, t.y - 3, r.w - 24, 20 }, w, 8, kInkHex, kLeft, true);
  GuiColor({ 1, 1, 1, 1 });
}
// A gun: an oilcloth tile, the picture, the rarity tag, missions left in cream and an ISSUED stamp when equipped (a
// framed red 'I' on narrow guns).
static void DrawItem(const Item& it, Rect4 r, float alpha, bool hot = false) {
  if (!DrawUi(r, "ui_tile.png", { 1, 1, 1, alpha })) {
    FillC(r, Hex(0x4c4b39, alpha)); FillC({ r.x, r.y + 1, r.w, 1 }, Hex(0xffffff, 0.08f * alpha)); Frame(r, 1, Hex(0, 0.5f * alpha));
  }
  g_textAlpha = alpha;
  GuiColor({ 1, 1, 1, alpha });
  void* sp = ItemSprite(it);
  if (sp) DrawSprite(sp, { r.x + 1, r.y + 1, r.w - 2, r.h - 2 });
  else Text({ r.x + 6, r.y + 18, r.w - 12, r.h - 36 }, it.name, 12, kCreamHex, kUpperLeft);
  RarityTag(r, it.rarity, alpha);
  if (it.uses > 0) Text({ r.x + 6, r.y + r.h - 23, 40, 22 }, std::to_string(it.uses), 14, kCreamHex, kLeft, true);
  if (std::find(g_invEquipped.begin(), g_invEquipped.end(), it.name) != g_invEquipped.end()) {
    if (r.w >= 95) {
      Rect4 st = { r.x + r.w - 64, r.y + r.h - 26, 60, 22 };
      if (!DrawUi(st, "ui_stamp_issued.png", { 1, 1, 1, alpha })) { Frame(st, 2, Hex(kIssuedRed, alpha)); Text({ st.x - 4, st.y - 6, st.w + 8, st.h + 12 }, "ISSUED", 9, "#c9372c", kCenter, true); }
    } else {
      Rect4 st = { r.x + r.w - 18, r.y + r.h - 18, 14, 14 };
      FillC({ st.x + 1, st.y + 1, st.w, st.h }, Hex(0, 0.5f * alpha));   // a paper chip, as the rarity tag: red on olive is unreadable
      FillC(st, Hex(kPaper, alpha));
      Frame(st, 2, Hex(kStampRed, alpha));
      Text({ st.x - 6, st.y - 8, st.w + 12, st.h + 16 }, "I", 10, kRedHex, kCenter, true);   // taller than the frame: Kanit's line height clips in 14 px
    }
  }
  if (hot) Frame(r, 1, Hex(kCream, 0.7f));
  GuiColor({ 1, 1, 1, 1 });
  g_textAlpha = 1;
}

static void DrawTooltip(const Item& it, Vec2 mouse) {
  std::string t = ItemTooltip(it);
  int lines = 1; size_t run = 0;
  for (char ch : t) { if (ch == '\n') { lines++; run = 0; } else if (++run > 58) { lines++; run = 0; } }
  float tw = 400, th = lines * 18.0f + 14;
  Rect4 tr = { mouse.x + 18, mouse.y + 12, tw, th };
  if (tr.x + tw > g_virtW) tr.x = mouse.x - tw - 8;
  if (tr.y + th > g_virtH) tr.y = g_virtH - th - 4;
  FillC(tr, Hex(0x1e150c, 0.97f));                                     // stays dark: its color tags are made for it
  Frame(tr, 1, Hex(kCream));
  FillC({ tr.x, tr.y, 3, tr.h }, RarityCol(it.rarity));
  Text({ tr.x + 8, tr.y + 6, tw - 16, th - 8 }, t, 13, "#eef1f4", kUpperLeft);
}

// Draws the window (Repaint only) and returns the gun under the mouse, whose tooltip goes on top of everything else.
static const Item* InventoryWindow(Vec2 mouse) {
  InvLayout L = GetLayout();
  int found = 0, unplaced = 0, used = 0, lootN = 0;
  for (const Item& it : g_items) {
    if (!it.fromInventory || it.retired) continue;
    if (it.held) { lootN++; continue; }
    found++;
    if (it.gx >= 0) { int w, h; ItemSize(it, &w, &h); used += w * h; } else unplaced++;
  }
  if (GetTickCount() - g_invEquippedAt > 1000) {
    void* craft = g_ccm ? *(void**)((char*)g_ccm + g_offSelCraft) : nullptr;
    g_invEquipped = craft ? EquippedNames(craft) : std::vector<std::string>();
    g_invEquippedAt = GetTickCount();
  }
  float iw = g_gridCols * L.cell, lw = g_lootCols * L.cell;
  // the crate: ply, edge, a vertical seam in the gap between the grids and one across the middle, brackets
  Rect4 p = L.panel;
  CrateBody(p, L.lx - 16, p.y + floorf(p.h / 2));
  // stencils: left over the manifest label (clear of the bracket), THIS SIDE UP left of CLOSE when there is room
  std::string top = Spaced("U.S.N.") + kDotSep + Spaced("ORDNANCE") + kDotSep + Spaced("HANDLE WITH CARE");
  float topW = CapsWidth(top, 13) * 1.06f;                             // rough: CalcSize is stripped
  Stencil({ p.x + 48, p.y + 6, topW + 40, 22 }, top, 13);
  std::string up = Spaced("THIS SIDE UP");
  float upW = CapsWidth(up, 13) * 1.06f, ar = L.close.x - 18 - 12, al = ar - 8 - upW - 8 - 12;
  if (al > p.x + 48 + topW + 24) {
    Stencil({ al + 12, p.y + 6, ar - al - 12, 22 }, up, 13, kCenter);
    for (float x : { al, ar }) if (!DrawUi({ x, p.y + 11, 12, 12 }, "ui_up.png", { 1, 1, 1, 0.5f })) Stencil({ x - 4, p.y + 6, 20, 22 }, "^", 13, kCenter);
  }
  // the manifest label: the old header bar's title, on paper with two staples
  Rect4 lab = { p.x + 26, L.title.y + L.title.h - 46, g_uiFontKanit ? 340.0f : 360.0f, 46 };
  PaperSlip(lab, 0.4f);
  GuiColor(Hex(0x8d969e)); Fill({ lab.x + 6, lab.y + 4, 10, 2 }); Fill({ lab.x + lab.w - 16, lab.y + 4, 10, 2 });
  Text({ lab.x + 14, lab.y, 240, lab.h }, Spaced("INVENTORY"), 22, kInkHex, kLeft, true);
  float mx = lab.x + 14 + (g_uiFontKanit ? 186 : 204);                 // right of the title, measured per font (no CalcSize)
  Text({ mx, lab.y + 6, 150, 17 }, "MANIFEST", 10, kMutedHex);
  Text({ mx, lab.y + 21, 150, 17 }, std::string("WEAPON MOD v") + kModVersion, 10, kMutedHex);
  // section headers and grids
  Rect4 sh = { L.ix, L.iy - 30, iw, 26 }, sl = { L.lx, L.ly - 30, lw, 26 };
  Text(sh, Spaced("YOUR GUNS"), 15, kInkHex, kLeft, true);
  Text(sh, std::to_string(used) + " / " + std::to_string(g_gridCols * g_gridRows) + " CELLS", 12, kMutedHex, kRight);
  Text(sl, Spaced("LOOT"), 15, kInkHex, kLeft, true);
  if (sl.w - 128 >= 130) Text({ sl.x, sl.y, sl.w - 128, sl.h }, std::to_string(lootN) + " FOUND", 12, kMutedHex, kRight);   // left of SCRAP ALL
  DrawGrid(L.ix, L.iy, g_gridCols, g_gridRows, L.cell);
  DrawGrid(L.lx, L.ly, g_lootCols, g_lootRows, L.cell);
  if (lootN == 0) {
    Rect4 b = { L.lx + lw / 2 - 62, L.ly + g_lootRows * L.cell / 2 - 14, 124, 26 };
    PaperSlip(b);
    Text(b, Spaced("NO LOOT YET"), 11, kInkHex, kCenter);
  }
  // scrap zone: hazard stripes around a see-through dark box
  bool dragging = !g_dragName.empty(), overScrap = dragging && Inside(mouse, L.scrap);
  Rect4 s = L.scrap;
  GuiColor({ 1, 1, 1, 1 });
  void* hz = HazardTex((int)(s.w * g_uiScale + 0.5f), (int)(s.h * g_uiScale + 0.5f));
  if (Alive(hz)) DrawTex(s, hz); else FillC(s, Hex(0xe0a222));
  Rect4 in = { s.x + 4, s.y + 4, s.w - 8, s.h - 8 };
  FillC(in, overScrap ? Hex(0x3a2a0c) : Hex(kCompart, 0.85f));
  Text(in, Spaced(overScrap ? "RELEASE TO SCRAP" : "DROP HERE TO SCRAP"), 13, kAmberHex, kCenter, true);
  // guns
  const Item* hover = nullptr; const Item* drag = nullptr;
  for (const Item& it : g_items) {
    if (!it.fromInventory || it.retired || it.gx < 0 || it.name == g_invHideName) continue;
    Rect4 r = ItemRect(L, it);
    bool isDrag = dragging && it.name == g_dragName;
    if (isDrag) drag = &it;
    bool hot = !dragging && Inside(mouse, r);
    if (hot) hover = &it;
    DrawItem(it, r, isDrag ? 0.3f : 1.0f, hot);
  }
  if (drag) {                                                          // target outline, then the gun under the mouse
    bool loot; int x, y;
    int w, h; ItemSize(*drag, &w, &h);
    if (DropTarget(L, mouse, &loot, &x, &y)) {
      bool ok = DropAllowed(*drag, loot, x, y);
      float ox = loot ? L.lx : L.ix, oy = loot ? L.ly : L.iy;
      Frame({ ox + x * L.cell, oy + y * L.cell, w * L.cell - 1, h * L.cell - 1 }, 2, ok ? Hex(0x66d17a) : Hex(0xff5a5a));
    }
    Rect4 r = { mouse.x - g_dragOX * L.cell - L.cell / 2, mouse.y - g_dragOY * L.cell - L.cell / 2, w * L.cell - 1, h * L.cell - 1 };
    FillC(r, Hex(0x0c0e12, 0.85f));
    DrawItem(*drag, r, 0.9f);
  }
  // footer: a paper strip with the hints, or the latest notice (its light color tags are dropped: they are made for a
  // dark background); under it the lot stencil
  std::string foot = std::string("NUMBER = MISSIONS LEFT") + kSep + "CLICK A WEAPON SLOT TO EQUIP";
  if (lootN) foot += std::string(kSep) + "DRAG LOOT INTO YOUR GUNS TO KEEP IT";
  if (unplaced) foot += kSep + std::to_string(unplaced) + (unplaced == 1 ? " OLDER GUN HAS NO CELL" : " OLDER GUNS HAVE NO CELL");
  bool notice = !g_invNotice.empty() && GetTickCount() - g_invNoticeAt < 6000;
  FillC(L.foot, Hex(kPaper, 0.88f));
  Text({ L.foot.x + 12, L.foot.y, L.foot.w - 24, L.foot.h }, notice ? StripTags(g_invNotice) : foot, 11, notice ? kInkHex : kFootHex);
  std::string lot = Spaced(std::string("LOT ") + kModVersion) + kDotSep + (p.w >= 760 ? Spaced("CONTENTS: SMALL ARMS AND ORDNANCE") + kDotSep : std::string()) + Spaced("KEEP DRY");
  Stencil({ p.x + 48, L.foot.y + L.foot.h + 2, p.w - 96, 20 }, lot, 12);
  return hover;
}

// Mouse: press on a gun picks it up, release drops it (on a grid cell or the scrap box). IMGUI sends one OnGUI call
// per event, so the press and release are seen once each.
static bool g_winDragging = false;
static float g_winGrabX = 0, g_winGrabY = 0;
static void InventoryMouse(void* ev, int type, Vec2 mouse) {
  if (type != 0 && type != 1 && type != 3) return;
  if (Unbox(Invoke(m_evButton, ev)) != 0) return;
  InvLayout L = GetLayout();
  if (g_winDragging) {                                                 // moving the window (MouseDrag / MouseUp)
    g_winX = mouse.x - g_winGrabX; g_winY = mouse.y - g_winGrabY;
    if (type == 1) { g_winDragging = false; InvLayout C = GetLayout(); g_winX = C.panel.x; g_winY = C.panel.y; SaveUi(); }
    if (m_evUse) Invoke(m_evUse, ev);
    return;
  }
  if (type == 3) return;
  if (type == 0 && g_dragName.empty()) {
    for (const Item& it : g_items) {
      if (!it.fromInventory || it.retired || it.gx < 0) continue;
      Rect4 r = ItemRect(L, it);
      if (!Inside(mouse, r)) continue;
      g_dragName = it.name;
      g_dragOX = (int)floorf((mouse.x - r.x) / L.cell); g_dragOY = (int)floorf((mouse.y - r.y) / L.cell);
      if (m_evUse) Invoke(m_evUse, ev);
      return;
    }
    if (Inside(mouse, L.title) && !Inside(mouse, L.close)) { g_winDragging = true; g_winGrabX = mouse.x - L.panel.x; g_winGrabY = mouse.y - L.panel.y; if (m_evUse) Invoke(m_evUse, ev); }
  } else if (type == 1 && !g_dragName.empty()) {
    DropDragged(L, mouse);
    g_dragName.clear();
    if (m_evUse) Invoke(m_evUse, ev);
  }
}

// GUI.matrix scales everything drawn (and the mouse position IMGUI reports) so the window is readable at any resolution.
static void GuiScale(float sc) {
  if (!m_guiSetMatrix) return;
  float m[16] = { sc, 0, 0, 0,  0, sc, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };   // Matrix4x4, column-major
  void* a[1] = { m }; Invoke(m_guiSetMatrix, nullptr, a);
}
static void InventoryGuiScaled();
static void InventoryGui() {
  if (!g_invReady || g_diagFrames <= 0) { g_dragName.clear(); g_winDragging = false; return; }  // loadout screen only
  float sw = (float)Unbox(Invoke(m_scrW, nullptr)), sh = (float)Unbox(Invoke(m_scrH, nullptr));
  g_uiScale = g_uiScaleSet > 0 ? g_uiScaleSet : (sh > 1080 ? sh / 1080.0f : 1.0f);
  if (sw > 0 && sh > 0) { g_virtW = sw / g_uiScale; g_virtH = sh / g_uiScale; }
  GuiScale(g_uiScale);
  InventoryGuiScaled();
  GuiScale(1.0f);
}
// ---- crates: the workshop strip (craft and open) and the opening reel ------------------------------------------------
// Opening rolls the gun first and checks the loot grid has room, then plays a three-reel slot machine:
// gun, rarity and first effect spin vertically and stop left to right on what was rolled; further effects
// are revealed under the cabinet with their numbers shuffling, then the full name and a banner (JACKPOT for a Rare).
struct SlotSym { std::string text, sub, base; int rarity; bool bad; };   // rarity: 0..3 (3 = Unique), -1 = none
static std::vector<SlotSym> g_slot[3];
static bool g_reelOn = false, g_reelDone = false;
static DWORD g_reelStart = 0, g_reelDoneAt = 0;
static int g_reelTier = 0, g_reelRarityIx = 0;
static std::vector<std::string> g_reelParts;                         // every effect's colored text; [0] is on the third reel
static std::string g_reelRarity, g_reelName, g_reelDesc;
static const int kSlotSyms = 34, kSlotWin = 30;                        // symbols per reel, index of the one that lands
static const DWORD kSlotStop[3] = { 2600, 3700, 4900 };                // ms after the start each reel stops
static const DWORD kRevealStepMs = 800, kScrambleMs = 650;
static std::map<std::string, SpriteEntry> g_baseSprites;

static void* BaseSprite(const std::string& base) {                   // the picture of a stock gun, from any live mount of it
  if (!m_mountSprite) return nullptr;
  SpriteEntry& e = g_baseSprites[base];
  if (e.sprite || (e.tried && GetTickCount() - e.tried < 3000)) return e.sprite;
  e.tried = GetTickCount();
  void* reg = LiveRegister();
  for (int i = 0; i < ListCount(reg) && !e.sprite; i++) {
    void* ws = ListAt(reg, i); if (!ws) continue;
    void* mounts = *(void**)((char*)ws + g_offMounts);
    for (int j = 0; j < ListCount(mounts); j++) {
      void* m = ListAt(mounts, j); void* gt = m ? *(void**)((char*)m + g_offGunType) : nullptr;
      if (gt && NameOf(gt) == base) { e.sprite = Invoke(m_mountSprite, m); Pin(e.sprite); break; }
    }
  }
  // No boat carries that gun right now (in port after a battle no boat is live): read GunType.sprite (what
  // GunMount.getSprite returns) off the loaded asset.
  if (!e.sprite) {
    static int offSprite = -2;
    if (offSprite == -2) offSprite = FieldOffset("Core", "GunType", "sprite");
    void* gt = offSprite >= 0 ? StockGunType(base) : nullptr;
    if (gt) { e.sprite = *(void**)((char*)gt + offSprite); if (e.sprite) Pin(e.sprite); }
  }
  return e.sprite;
}

static int FakeRarity(int tier) {                                    // what passing symbols show; the real roll is separate
  if (Rand01() < 0.04) return 3;                                     // a Unique flashes past now and then
  double r = Rand01();
  if (tier == 2) return r < 0.12 ? 0 : (r < 0.40 ? 1 : 2);
  if (tier == 1) return r < 0.20 ? 0 : (r < 0.70 ? 1 : 2);
  return r < 0.55 ? 0 : (r < 0.85 ? 1 : 2);
}

static const char* kRarityWord[4] = { "Common", "Uncommon", "Rare", "Unique" };

static int RarityIx(const std::string& r) { return RarityOf(r); }
static void OpenCrate(int tier) {
  Item it; std::string rarity;
  int minAff = tier == 2 ? 3 : (tier == 1 ? 2 : 0), needRix = tier == 2 ? 2 : (tier == 1 ? 1 : 0);
  bool rolled = g_uniqueCrate[tier] > 0 && Rand01() < g_uniqueCrate[tier] && RollUnique(&it, &rarity);
  for (int tries = 0; tries < 20 && !rolled; tries++)                // a template with too few fitting effects can roll below the floor
    if (RollItem(&it, &rarity, minAff) && RarityIx(rarity) >= needRix) rolled = true;
  if (!rolled) { Notice(std::string("Could not roll a gun good enough for this crate; it stays shut (check drops.ini).")); return; }
  it.held = true;
  if (!GridPlace(it)) { int w, h; ItemSize(it, &w, &h); Notice("No room in the loot grid for a " + std::to_string(w) + "x" + std::to_string(h) + " gun. Make room, then open it."); return; }
  if (g_crateBonusUses > 0 && it.uses > 0 && rarity != "Unique") it.uses += g_crateBonusUses;   // a Unique's missions are part of its design
  g_crates[tier]--;
  AddToInventory(it);                                                  // saves materials and crates too
  Log("CRATE: %s opened: '%s' (%s), %d missions: %s", kCrateName[tier], it.name.c_str(), rarity.c_str(), it.uses, it.description.c_str());
  int rix = RarityIx(rarity);
  for (auto& r : g_slot) r.clear();
  for (int i = 0; i < kSlotSyms; i++) {
    const DropTemplate& t = g_templates[(size_t)(Rand01() * g_templates.size()) % g_templates.size()];
    g_slot[0].push_back({ t.label, "", t.base, -1, false });
    int fr = FakeRarity(tier);
    g_slot[1].push_back({ kRarityWord[fr], "", "", fr, false });
    const Affix& af = g_affixes[(size_t)(Rand01() * g_affixes.size()) % g_affixes.size()];
    g_slot[2].push_back({ af.name, "", "", -1, af.bad });
  }
  g_slot[0][kSlotWin] = { g_rollLabel, "", it.base, -1, false };
  g_slot[1][kSlotWin] = { kRarityWord[rix], "", "", rix, false };
  if (!g_rollAffixes.empty()) g_slot[2][kSlotWin] = { g_rollAffixes[0].first, g_rollParts.empty() ? "" : g_rollParts[0], "", -1, g_rollAffixes[0].second };
  if (rix < 2 && Rand01() < 0.5) g_slot[1][kSlotWin + (Rand01() < 0.5 ? 1 : -1)] = { "Rare", "", "", 2, false };   // near miss
  else if (rix == 2 && Rand01() < 0.4) g_slot[1][kSlotWin + (Rand01() < 0.5 ? 1 : -1)] = { "Unique", "", "", 3, false };
  g_reelName = it.name; g_reelDesc = it.uses > 0 ? "Missions: " + std::to_string(it.uses) + "." : std::string("Never wears out.");
  g_reelParts = g_rollParts; g_reelRarity = rarity; g_reelRarityIx = rix; g_invHideName = it.name;
  g_reelTier = tier; g_reelOn = true; g_reelDone = false; g_reelStart = GetTickCount();
  g_dragName.clear();
}

// The workshop: one paper slip per material with its swatch, amount and crates and the CRAFT / OPEN tags (inactive
// behind the reel while it runs). Stacked under YOUR GUNS, or side by side in a strip under both grids when the guns grid
// is narrow (L.slips false). Tags go through every event; drawing only on Repaint.
static const unsigned kMatSwatch[3] = { 0x9aa3ab, 0xc9a24a, 0x5a6570 };   // scrap iron, brass, gunmetal
static void Workshop(const InvLayout& L, int type, Vec2 mouse) {
  Rect4 r = L.shop;
  float cw = L.slips ? r.w : (r.w - 2 * kSlipGap) / 3;
  for (int t = 0; t < 3; t++) {
    Rect4 c = L.slips ? Rect4{ r.x, r.y + t * (kSlipH + kSlipGap), r.w, kSlipH } : Rect4{ r.x + t * (cw + kSlipGap), r.y, cw, kStripSlipH };
    // stacked: one row, tags right; strip: the text on top at full width, the tags on a second row, right-aligned
    float tagY = L.slips ? c.y + (c.h - 26) / 2 : c.y + 38;
    Rect4 open = { c.x + c.w - 12 - 54, tagY, 54, 26 }, craft = { open.x - 8 - 74, tagY, 74, 26 };
    if (type == 7) {
      PaperSlip(c, 0.4f);
      Rect4 sw = { c.x + 12, L.slips ? c.y + (c.h - 26) / 2 : c.y + 8, 26, 26 };
      FillC(sw, Hex(kMatSwatch[t])); Frame(sw, 2, Hex(0, 0.35f));
      float tx = sw.x + 26 + 10, tw = (L.slips ? craft.x - 6 : c.x + c.w - 12) - tx;
      if (tw < 0) tw = 0;
      Text({ tx, c.y + 2, tw, 18 }, Spaced(Upper(kMatName[t])), 11, kMutedHex);
      Text({ tx, c.y + 14, tw, L.slips ? 30.0f : 24.0f }, "<b>" + std::to_string(g_mat[t]) + "</b><size=12><color=" + kMutedHex + ">" + kSep + std::to_string(g_crates[t]) + (g_crates[t] == 1 ? " crate" : " crates") + "</color></size>", 18, kInkHex);
    }
    if (CrateTag(craft, "CRAFT " + std::to_string(g_crateCost[t]), type, mouse, !g_reelOn)) {
      if (g_reelOn) {}
      else if (g_mat[t] < g_crateCost[t]) Notice(std::string(t == 0 ? "An " : "A ") + kCrateName[t] + " takes " + std::to_string(g_crateCost[t]) + " " + kMatName[t] + ".");
      else { g_mat[t] -= g_crateCost[t]; g_crates[t]++; SaveInventory(); Notice(std::string(t == 0 ? "Crafted an " : "Crafted a ") + kCrateName[t] + "."); }
    }
    if (CrateTag(open, "OPEN", type, mouse, !g_reelOn, 11, true)) {
      if (g_reelOn) {}
      else if (g_crates[t] <= 0) Notice(std::string("No ") + kCrateName[t] + "s. Craft one or find one in battle.");
      else OpenCrate(t);
    }
  }
}

// Ease out with a small overshoot, so a reel lands with a little bounce.
static float EaseOutBack(float t) { const float c1 = 1.2f, c3 = c1 + 1; float u = t - 1; return 1 + c3 * u * u * u + c1 * u * u; }

static void Reel(int type, Vec2 mouse) {
  const float symH = 70, cabPad = 22;
  const float colW[3] = { 180, 150, 210 };
  float reelsW = colW[0] + colW[1] + colW[2] + 16;
  // 230 fits three revealed lines plus the name, text and banner; each further line (a Rare with a drawback and a Look,
  // a Unique with many lines) adds a row so nothing runs under the Close button
  float extraRows = g_reelParts.size() > 4 ? (float)(g_reelParts.size() - 4) : 0.0f;
  float pw = reelsW + 2 * cabPad + 40, ph = 3 * symH + 2 * cabPad + 230 + 22 * extraRows;
  Rect4 panel = { (g_virtW - pw) / 2, (g_virtH - ph) / 2, pw, ph };
  DWORD el = GetTickCount() - g_reelStart;
  if (el >= kSlotStop[2] && !g_reelDone) { g_reelDone = true; g_reelDoneAt = GetTickCount(); DLog("CRATE: reels stopped on '%s'", g_reelName.c_str()); }
  DWORD since = g_reelDone ? GetTickCount() - g_reelDoneAt : 0;
  size_t extra = g_reelParts.size() > 1 ? g_reelParts.size() - 1 : 0;   // effects after the one on the third reel
  bool revealed = g_reelDone && since >= kRevealStepMs * (DWORD)extra + 300;
  bool jackpot = revealed && g_reelRarityIx >= 2;
  if (type == 7) {
    // cabinet: a small plywood crate like the inventory, the title on a paper manifest label (clear of the
    // bracket), dark compartments for the reels and the reveal text (its color tags need a dark background)
    CrateBody(panel, -1, -1);
    PaperSlip({ panel.x + 50, panel.y + 6, 300, 24 }, 0.4f);
    GuiColor({ 1, 1, 1, 1 });
    GuiLabel({ panel.x + 60, panel.y + 6, 290, 24 }, std::string("<color=#2b2a26><b>") + kCrateName[g_reelTier] + "</b>   pull!</color>");
    float rx = panel.x + cabPad + 20, ry = panel.y + 44;
    // chasing lights above and below the reels; all blink together on a jackpot
    if (g_white) {
      int n = (int)((pw - 2 * cabPad) / 22), phase = (int)(GetTickCount() / 140);
      bool blinkAll = jackpot && (GetTickCount() / 180) % 2 == 0;
      for (int row = 0; row < 2; row++) for (int i = 0; i < n; i++) {
        bool lit = jackpot ? blinkAll : ((i + phase + row) % 3 == 0);
        GuiColor(lit ? Col4{ 1, 0.85f, 0.25f, 1 } : Col4{ 0.35f, 0.28f, 0.12f, 1 });
        Fill({ panel.x + cabPad + i * 22 + 6, row == 0 ? ry - 12 : ry + 3 * symH + 6, 8, 8 });
      }
    }
    // the three reels
    float x = rx;
    for (int r = 0; r < 3; r++) {
      float w = colW[r];
      float tr = el / (float)kSlotStop[r]; if (tr > 1) tr = 1;
      float endOff = kSlotWin * symH;
      float off = endOff * EaseOutBack(tr);
      Rect4 win = { x, ry, w, 3 * symH };
      Compartment(win, 0);
      void* g[3] = { &win, g_guiContentNone, g_guiStyleNone };
      Invoke(m_guiBeginGroup, nullptr, g);
      int lo = (int)((off - 2 * symH) / symH) - 1, hi = (int)((off + 2 * symH) / symH) + 1;
      for (int i = lo; i <= hi; i++) {
        if (i < 0 || i >= (int)g_slot[r].size()) continue;
        const SlotSym& sym = g_slot[r][i];
        float y = symH - i * symH + off;                                 // symbols move down; the winner ends on the middle row
        Rect4 cell = { 3, y + 3, w - 6, symH - 6 };
        bool landed = tr >= 1 && i == kSlotWin;
        if (sym.rarity >= 0) RarityTile(cell, RarityCol(kRarityWord[sym.rarity]), 1.0f, landed ? 0.5f + 0.5f * (float)sin(GetTickCount() / 120.0) : 0.0f);
        else { GuiColor({ 1, 1, 1, 1 }); GuiBox(cell); }
        GuiColor({ 1, 1, 1, 1 });
        if (r == 0) {
          void* sp = BaseSprite(sym.base);
          if (sp) DrawSprite(sp, { cell.x, cell.y, cell.w, cell.h - 18 });
          GuiLabel({ cell.x + 6, cell.y + cell.h - 20, cell.w - 12, 20 }, sym.text);
        } else if (r == 1) {
          GuiLabel({ cell.x + 10, cell.y + cell.h / 2 - 12, cell.w - 20, 24 }, "<b><size=18>" + Colored(sym.text, RarityHex(sym.text)) + "</size></b>");
        } else {
          GuiLabel({ cell.x + 8, cell.y + 6, cell.w - 16, 22 }, "<b>" + Colored(sym.text, sym.bad ? kBadHex : kGoodHex) + "</b>");
          if (landed && !sym.sub.empty()) GuiLabel({ cell.x + 8, cell.y + 28, cell.w - 16, 40 }, sym.sub);
        }
      }
      Invoke(m_guiEndGroup, nullptr);
      x += w + 8;
    }
    // payline across the middle row
    if (g_white) {
      GuiColor({ 1, 0.85f, 0.2f, 0.9f });
      Fill({ rx - 14, ry + symH - 1, reelsW + 12, 2 }); Fill({ rx - 14, ry + 2 * symH - 1, reelsW + 12, 2 });
    }
    GuiColor({ 1, 0.85f, 0.2f, 1 });
    GuiLabel({ rx - 22, ry + symH + 24, 20, 22 }, "<b>></b>");
    GuiLabel({ rx + reelsW - 4, ry + symH + 24, 20, 22 }, "<b><</b>");
    float cy = ry + 3 * symH + 18;
    Compartment({ panel.x + cabPad - 6, cy, pw - 2 * cabPad + 12, panel.y + ph - 14 - cy }, 0);
    GuiColor({ 1, 1, 1, 1 });
    // under the cabinet: the remaining effects, then the name and the banner
    float y = ry + 3 * symH + 24;
    if (g_reelDone) {
      for (size_t k = 1; k < g_reelParts.size(); k++) {
        DWORD at = kRevealStepMs * (DWORD)(k - 1);
        if (since < at) break;
        std::string line = g_reelParts[k];
        if (since - at < kScrambleMs) {                                   // shuffle the digits (tags left alone)
          bool tag = false;
          for (char& ch : line) { if (ch == '<') tag = true; else if (ch == '>') tag = false; else if (!tag && ch >= '0' && ch <= '9') ch = (char)('0' + (int)(Rand01() * 10) % 10); }
        }
        GuiLabel({ panel.x + cabPad, y, pw - 2 * cabPad, 22 }, "+ " + line);
        y += 22;
      }
    }
    if (revealed) {
      static const char* banner[4] = { "WINNER", "NICE ONE!", "*** J A C K P O T ***", "*** U N I Q U E ***" };
      float a = g_reelRarityIx >= 2 ? 0.6f + 0.4f * (float)sin(GetTickCount() / 90.0) : 1.0f;
      Col4 c = RarityCol(g_reelRarity); c.a = a;
      GuiColor(c);
      GuiLabel({ panel.x + cabPad, y + 4, pw - 2 * cabPad, 30 }, std::string("<b><size=22>") + banner[g_reelRarityIx] + "</size></b>");
      GuiColor({ 1, 1, 1, 1 });
      GuiLabel({ panel.x + cabPad, y + 36, pw - 2 * cabPad, 24 }, "<b>You got: " + Colored(g_reelName, RarityHex(g_reelRarity)) + " (" + g_reelRarity + ")</b>");
      GuiLabel({ panel.x + cabPad, y + 60, pw - 2 * cabPad - 120, 44 }, g_reelDesc + " It is in the loot grid: drag it into your guns to keep it.");
    }
  }
  Rect4 rb = { panel.x + pw - 130, panel.y + ph - 48, 100, 26 };   // inside the reveal compartment, clear of the bracket
  GuiThemeBegin();                                                     // the tag in the window's font; the reel's labels keep Unity's
  bool hit = CrateTag(rb, Spaced(revealed ? "CLOSE" : "SKIP"), type, mouse);
  GuiThemeEnd();
  if (hit) {
    if (revealed) { g_reelOn = false; g_invHideName.clear(); }
    else if (g_reelDone) g_reelDoneAt = GetTickCount() - 60000;           // skip the reveal
    else g_reelStart = GetTickCount() - kSlotStop[2];                   // stop all reels now
  }
}

static bool ModMenus(void* ev, int type, Vec2 mouse);
// SCRAP ALL in the loot header: every loot gun and torpedo to materials at once, as the scrap box does
// one by one; Uniques stay. The first click arms it and says what it gives, a second within 3 s scraps.
static DWORD g_scrapAllArmedAt = 0; static int g_scrapAllN = 0;
struct LootYield { int n = 0, kept = 0, mat[3] = { 0, 0, 0 }; };
static bool ScrapAllCandidate(const Item& it) { return it.fromInventory && !it.retired && it.held && it.gx >= 0 && it.name != g_invHideName; }
static LootYield LootTotals() {
  LootYield y;
  for (const Item& it : g_items) {
    if (!ScrapAllCandidate(it)) continue;
    if (RarityOf(it.rarity) >= 3) { y.kept++; continue; }
    int t = 0, got = ScrapYield(it.rarity, &t); y.mat[t] += got; y.n++;
  }
  return y;
}
static std::string YieldText(const int mat[3]) {
  std::string s;
  for (int t = 0; t < 3; t++) if (mat[t]) s += (s.empty() ? "+" : ", +") + std::to_string(mat[t]) + " " + kMatName[t];
  return s.empty() ? "nothing" : s;
}
static void ScrapAllLoot() {
  int n = 0, mat[3] = { 0, 0, 0 };
  for (Item& it : g_items) {
    if (!ScrapAllCandidate(it) || RarityOf(it.rarity) >= 3) continue;
    int t = 0, got = ScrapYield(it.rarity, &t);
    it.retired = true; it.gx = it.gy = -1; g_mat[t] += got; mat[t] += got; n++;   // loot is on no boat: no unlisting
    DLog("scrap all: '%s' (%s) -> +%d %s", it.name.c_str(), it.rarity.c_str(), got, kMatName[t]);
  }
  if (n) SaveInventory();
  Notice("Scrapped " + std::to_string(n) + (n == 1 ? " loot item: " : " loot items: ") + YieldText(mat) + ".");
}
static void ScrapAllButton(const InvLayout& L, int type, Vec2 m) {
  DWORD now = GetTickCount();
  LootYield ly = LootTotals();
  bool armed = g_scrapAllArmedAt && now - g_scrapAllArmedAt < 3000 && ly.n == g_scrapAllN && g_dragName.empty();
  if (!armed) g_scrapAllArmedAt = 0;
  if (CrateTag(L.scrapAll, Spaced(armed ? "CONFIRM" : "SCRAP ALL"), type, m, !g_reelOn && g_dragName.empty() && ly.n > 0, 11)) {
    if (armed) { g_scrapAllArmedAt = 0; ScrapAllLoot(); }
    else {
      g_scrapAllArmedAt = now ? now : 1; g_scrapAllN = ly.n;
      Notice("Scrap " + std::to_string(ly.n) + (ly.n == 1 ? " loot item" : " loot items") + " for " + YieldText(ly.mat) + (ly.kept ? " (Uniques stay)" : "") + "? Click CONFIRM within 3 s.");
    }
  }
  if (armed && type == 7) Frame(L.scrapAll, 2, Hex(kStampRed));         // stamp red while armed
}
static void InventoryGuiScaled() {
  void* ev = Invoke(m_evCurrent, nullptr);
  if (!ev) return;
  int type = Unbox(Invoke(m_evType, ev));
  if (type == 4 || type == 5) {                                        // KeyDown / KeyUp: I toggles (key repeat ignored)
    if (Unbox(Invoke(m_evKey, ev)) == 105) {
      if (type == 4 && !g_invKeyHeld) { g_invOpen = !g_invOpen; DLog("inventory window %s (I key)", g_invOpen ? "opened" : "closed"); }
      g_invKeyHeld = type == 4;
    }
  }
  void* mp = Invoke(m_evMouse, ev);
  Vec2 mouse = mp ? *(Vec2*)((char*)mp + 0x10) : Vec2{ -1, -1 };
  GuiColor({ 1, 1, 1, 1 });
  InvLayout L = GetLayout();
  GuiThemeBegin();
  if (ModMenus(ev, type, mouse)) { g_dragName.clear(); g_winDragging = false; GuiThemeEnd(); return; }   // options or What's New on top: the inventory waits
  if (!g_reelOn && CrateTag({ L.toggle.x + L.toggle.w + 8, L.toggle.y, 140, L.toggle.h }, Spaced("MOD OPTIONS"), type, mouse)) { g_optOpen = true; g_newsOpen = false; }
  if (!g_invOpen) {
    if (CrateTag(L.toggle, Spaced("INVENTORY [I]"), type, mouse)) { g_invOpen = true; DLog("inventory window opened (button)"); }
    g_dragName.clear(); g_winDragging = false;
    GuiThemeEnd();
    return;
  }
  if (!g_reelOn) InventoryMouse(ev, type, mouse);
  Vec2 m = g_reelOn ? Vec2{ -1, -1 } : mouse;
  const Item* hover = type == 7 ? InventoryWindow(m) : nullptr;     // Repaint; Layout and input events draw nothing
  if (CrateTag(L.close, Spaced("CLOSE [I]"), type, m, !g_reelOn, 12)) { g_invOpen = false; g_scrapAllArmedAt = 0; DLog("inventory window closed (button)"); }
  ScrapAllButton(L, type, m);
  Workshop(L, type, m);
  if (hover) DrawTooltip(*hover, mouse);
  GuiThemeEnd();                                                       // the reel keeps its old look: Unity's font, the skin's alignment
  if (g_reelOn) Reel(type, mouse);                                            // (its text boxes clip Kanit's taller lines)
  GuiColor({ 1, 1, 1, 1 });
  GuiThemeEnd();
}

