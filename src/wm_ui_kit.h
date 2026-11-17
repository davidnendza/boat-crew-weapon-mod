// window look ("Armory stencil") and the Field Manual kit (paper, ink, stamps, PNGs) (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- the window's look: "Armory stencil" -----------------------------------------------------------------------------
// Flat dark panels, uppercase headers, rarity as a corner triangle on each gun, a hazard-striped scrap zone. Triangles,
// gradients and stripes are small textures made once at run time (GUI.DrawTextureWithTexCoords is stripped, so nothing
// tiles). Text goes through GUI.skin.label with its alignment set per call (the GUIStyle copy constructor and its font
// setter are stripped), in the game's Jost font when one is loaded, else Unity's default; both are put back afterwards.
static void *m_guiLabelSt = nullptr, *m_guiButtonSt = nullptr, *m_guiGetSkin = nullptr, *m_skinGetFont = nullptr, *m_skinSetFont = nullptr;
static void *m_skinGetLabel = nullptr, *m_styleGetAlign = nullptr, *m_styleSetAlign = nullptr;
static void *g_skin = nullptr, *g_skinLabel = nullptr, *g_skinFont0 = nullptr, *g_jost = nullptr;
static int g_skinAlign0 = 0;
static bool g_uiFontKanit = false;                                     // the window's font is Kanit (see kUiFonts)
static float g_textAlpha = 1;
enum { kUpperLeft = 0, kLeft = 3, kCenter = 4, kRight = 5 };          // TextAnchor
static const char* kSep = "  \xC2\xB7  ";                              // a middle dot between hints

static Col4 Hex(unsigned rgb, float a = 1) { return { ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a }; }
static Col4 Mix(Col4 a, Col4 b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t }; }
static void FillC(Rect4 r, Col4 c) { GuiColor(c); Fill(r); }
static void Frame(Rect4 r, float t, Col4 c) {
  GuiColor(c); Fill({ r.x, r.y, r.w, t }); Fill({ r.x, r.y + r.h - t, r.w, t }); Fill({ r.x, r.y, t, r.h }); Fill({ r.x + r.w - t, r.y, t, r.h });
}
static bool Alive(void* o) { return o && *(void**)((char*)o + 0x10); }   // a destroyed UnityEngine.Object keeps its wrapper, without the native object
static void DrawTex(Rect4 r, void* t) { if (!Alive(t)) return; void* d[2] = { &r, t }; Invoke(m_guiDrawTex, nullptr, d); }

struct TexApi { void *cls, *ctor, *setPx, *apply, *wrap, *hide; bool tried; };
static TexApi g_tx = {};
static bool TexApiReady() {
  if (!g_tx.tried) {
    g_tx.tried = true;
    g_tx.cls = FindClass("UnityEngine", "Texture2D");
    if (g_tx.cls) { g_tx.ctor = FindMethod(g_tx.cls, ".ctor", 2, "System.Int32", "System.Int32"); g_tx.setPx = FindMethod(g_tx.cls, "SetPixel", 3); g_tx.apply = FindMethod(g_tx.cls, "Apply", 0); }
    void* t = FindClass("UnityEngine", "Texture"); g_tx.wrap = t ? FindMethod(t, "set_wrapMode", 1) : nullptr;
    void* o = FindClass("UnityEngine", "Object"); g_tx.hide = o ? FindMethod(o, "set_hideFlags", 1) : nullptr;
    if (!g_tx.ctor || !g_tx.setPx || !g_tx.apply) Log("inventory look: Texture2D lookups failed; triangles, stripes and gradients are drawn flat");
  }
  return g_tx.ctor && g_tx.setPx && g_tx.apply;
}
// A texture made pixel by pixel: px(x, y, &color) with y counted from the top; returning false leaves that pixel unset.
// Kept for the session: pinned, and flagged so Resources.UnloadUnusedAssets leaves it.
static void* NewTex(int w, int h) {
  if (!TexApiReady() || w < 1 || h < 1) return nullptr;
  void* t = il2cpp_object_new(g_tx.cls);
  if (!t) return nullptr;
  void* ca[2] = { &w, &h }; Invoke(g_tx.ctor, t, ca);
  int clamp = 1, keep = 32;                                            // TextureWrapMode.Clamp, HideFlags.DontUnloadUnusedAsset
  if (g_tx.wrap) { void* a[1] = { &clamp }; Invoke(g_tx.wrap, t, a); }
  if (g_tx.hide) { void* a[1] = { &keep }; Invoke(g_tx.hide, t, a); }
  return t;
}
template <class F> static void* MakeTex(int w, int h, F px) {
  void* t = NewTex(w, h);
  if (!t) return nullptr;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) { Col4 c; if (!px(x, h - 1 - y, &c)) continue; void* a[3] = { &x, &y, &c }; Invoke(g_tx.setPx, t, a); }
  Invoke(g_tx.apply, t);
  Pin(t);
  return t;
}
static void *g_texTri = nullptr, *g_texHead = nullptr, *g_texBtn = nullptr;
static std::map<std::pair<int, int>, void*> g_texHazard;
static void* TriTex() {                                                // white right triangle in the top-left corner, soft long edge
  if (!Alive(g_texTri)) g_texTri = MakeTex(32, 32, [](int x, int y, Col4* c) { float a = 31.0f - (x + y); *c = { 1, 1, 1, a < 0 ? 0 : (a > 1 ? 1 : a) }; return true; });
  return g_texTri;
}
static void* HeadTex() {                                               // header bar, #0a0c10 to #1c2027 left to right
  if (!Alive(g_texHead)) g_texHead = MakeTex(64, 1, [](int x, int, Col4* c) { *c = Mix(Hex(0x0a0c10), Hex(0x1c2027), x / 63.0f); return true; });
  return g_texHead;
}
static void* BtnTex() {                                                // button face, #2a2f37 to #14171c top to bottom
  if (!Alive(g_texBtn)) g_texBtn = MakeTex(1, 32, [](int, int y, Col4* c) { *c = Mix(Hex(0x2a2f37), Hex(0x14171c), y / 31.0f); return true; });
  return g_texBtn;
}
// Amber and black diagonal stripes, made at the drawn size in screen pixels; inside the border band it is clear, since
// the crate's scrap box is see-through (2026-11-05).
static void* HazardTex(int w, int h) {
  void*& t = g_texHazard[{ w, h }];
  if (!Alive(t)) {
    int band = (int)(4 * g_uiScale + 0.5f) + 1;
    float stripe = 8 * g_uiScale;
    t = MakeTex(w, h, [&](int x, int y, Col4* c) {
      if (x >= band && x < w - band && y >= band && y < h - band) { *c = { 0, 0, 0, 0 }; return true; }
      *c = (int)floorf((x + y) / 1.41421f / stripe) % 2 == 0 ? Hex(0xe0a222) : Hex(0x1e150c);
      return true;
    });
  }
  return t;
}

// The game's own text is TextMeshPro, which IMGUI cannot use, and Jost is loaded only as a TextMeshPro asset (test
// 2026-10-27). So the first dynamic Font in kUiFonts order is used: Jost if it ever is loaded, else Kanit, a geometric
// sans close to it that the game loads for its translations, else Be Vietnam Pro; else Unity's default.
static const char* kUiFonts[] = { "jost-regular", "jost-medium", "jost", "kanit-regular", "kanit-medium", "bevietnampro-regular" };
static void FindGameFont() {
  static bool tried = false;
  if (tried) return;
  tried = true;
  void* res = FindClass("UnityEngine", "Resources"); void* fk = FindClass("UnityEngine", "Font");
  void* mAll = res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
  void* mDyn = fk ? FindMethod(fk, "get_dynamic", 0) : nullptr; void* mHas = fk ? FindMethod(fk, "HasCharacter", 1) : nullptr;
  if (!mAll || !fk) { Log("inventory font: lookups failed, Unity's default font is used"); return; }
  void* a[1] = { il2cpp_type_get_object(il2cpp_class_get_type(fk)) };
  void* arr = Invoke(mAll, nullptr, a);
  size_t n = ArrLen(arr);
  std::string names; int best = (int)(sizeof kUiFonts / sizeof *kUiFonts);
  for (size_t i = 0; i < n && i < 200; i++) {
    void* f = ArrAt(arr, i);
    if (!f) continue;
    std::string nm = NameOf(f), lo = nm;
    for (char& ch : lo) ch = (char)tolower((unsigned char)ch);
    void* d = mDyn ? Invoke(mDyn, f) : nullptr; bool dyn = d && *(bool*)((char*)d + 0x10);
    unsigned short A = 'A'; void* ha[1] = { &A }; void* hr = mHas ? Invoke(mHas, f, ha) : nullptr; bool has = hr && *(bool*)((char*)hr + 0x10);
    names += " '" + nm + "'" + (dyn ? "" : " (static)") + (has ? "" : " (no glyphs)");
    if (!dyn || !has) continue;
    for (int k = 0; k < best; k++) if (lo.find(kUiFonts[k]) != std::string::npos) { best = k; g_jost = f; break; }
  }
  if (g_jost) { Pin(g_jost); g_uiFontKanit = !strncmp(kUiFonts[best], "kanit", 5); Log("inventory font: %s", NameOf(g_jost).c_str()); }
  else Log("inventory font: none of the preferred fonts is loaded, Unity's default is used. Fonts:%s", names.c_str());
}
static void GuiThemeBegin() {
  FindGameFont();
  g_skin = m_guiGetSkin ? Invoke(m_guiGetSkin, nullptr) : nullptr;
  g_skinLabel = g_skin && m_skinGetLabel ? Invoke(m_skinGetLabel, g_skin) : nullptr;
  if (g_skinLabel && m_styleGetAlign) g_skinAlign0 = Unbox(Invoke(m_styleGetAlign, g_skinLabel));
  g_skinFont0 = nullptr;
  if (g_skin && Alive(g_jost) && m_skinGetFont && m_skinSetFont) { g_skinFont0 = Invoke(m_skinGetFont, g_skin); void* a[1] = { g_jost }; Invoke(m_skinSetFont, g_skin, a); }
}
static void GuiAlignReset() { if (g_skinLabel && m_styleSetAlign) { void* a[1] = { &g_skinAlign0 }; Invoke(m_styleSetAlign, g_skinLabel, a); } }
static void GuiThemeEnd() {
  GuiAlignReset();
  if (g_skin && Alive(g_jost) && m_skinSetFont) { void* a[1] = { g_skinFont0 }; Invoke(m_skinSetFont, g_skin, a); }
  g_skin = g_skinLabel = nullptr;
}
static void Text(Rect4 r, const std::string& t, int size, const char* hex, int align = kLeft, bool bold = false) {
  std::string s = "<size=" + std::to_string(size) + "><color=" + hex + ">" + (bold ? "<b>" + t + "</b>" : t) + "</color></size>";
  GuiColor({ 1, 1, 1, g_textAlpha });
  if (!g_skinLabel || !m_guiLabelSt || !m_styleSetAlign) { GuiLabel(r, s); return; }
  void* aa[1] = { &align }; Invoke(m_styleSetAlign, g_skinLabel, aa);
  void* a[3] = { &r, il2cpp_string_new(s.c_str()), g_skinLabel }; Invoke(m_guiLabelSt, nullptr, a);
}
static std::string Upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }
// IMGUI buttons go through every event, the same buttons in the same order; the look is drawn on Repaint only.
static bool StyledButton(Rect4 r, const std::string& label, int type, Vec2 mouse, bool active = true, int size = 11) {
  if (!m_guiButtonSt) return active && GuiButton(r, label.c_str());
  bool click = false;
  if (active) { void* a[3] = { &r, g_guiContentNone, g_guiStyleNone }; void* res = Invoke(m_guiButtonSt, nullptr, a); click = res && *(bool*)((char*)res + 0x10); }
  if (type == 7) {
    GuiColor({ 1, 1, 1, 1 });
    void* face = BtnTex();
    if (Alive(face)) DrawTex(r, face); else FillC(r, Hex(0x1f242b));
    Frame(r, 1, Hex(active && Inside(mouse, r) ? 0xd6dce1 : 0x7a838c));
    Text(r, label, size, active ? "#eef1f4" : "#8a929b", kCenter);
  }
  return click;
}
// ---- the Field Manual kit: paper, ink, stamps and the PNGs behind them ----
// (moved above the inventory 2026-11-05 so the field crate look can use it; unchanged)
static const unsigned kTabOn = 0xdccdaf;   // mean of ui_paper.png's darkened top band, so the active tab joins the textured sheet
static const unsigned kPaper = 0xe8dcc0, kInk = 0x2b2a26, kInkMuted = 0x6b5f45, kStampRed = 0xb3261e, kTabOff = 0xc9b88f, kCard = 0xf6f1e3;
static const char *kInkHex = "#2b2a26", *kMutedHex = "#6b5f45", *kBlueHex = "#1f3f8a", *kRedHex = "#b3261e", *kTabTextHex = "#5a4f38", *kPaperHex = "#e8dcc0";
static Col4 Hilite() { return Hex(0xf0c83c, 0.35f); }                // highlighter, 35%
// A PNG read with ImageConversion.LoadImage (the game kept it) into a new Texture2D, pinned and DontUnloadUnusedAsset
// (NewTex). Returns 0 = loaded, 1 = no file, 2 = a lookup failed (*detail says which), 3 = not an image the game reads.
static int LoadPngTex(const std::string& path, void** out, std::string* detail = nullptr) {
  *out = nullptr;
  std::vector<unsigned char> png;
  if (FILE* f = fopen(path.c_str(), "rb")) {
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0 && n < (16 << 20)) { png.resize(n); if (fread(png.data(), 1, n, f) != (size_t)n) png.clear(); }
    fclose(f);
  }
  if (png.empty()) return 1;
  static void *load = nullptr, *byteK = nullptr; static bool looked = false;
  static void* (*arrayNew)(void*, size_t) = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
  if (!looked) {
    looked = true;
    void* ic = FindClass("UnityEngine", "ImageConversion");
    load = ic ? FindMethod(ic, "LoadImage", 2) : nullptr;            // LoadImage(Texture2D, byte[])
    byteK = FindClass("System", "Byte");
  }
  void* tex = load && byteK && arrayNew ? NewTex(2, 2) : nullptr;
  void* arr = tex ? arrayNew(byteK, png.size()) : nullptr;
  if (!arr) { if (detail) { char b[160]; snprintf(b, sizeof b, "LoadImage %p, Byte %p, texture %p", load, byteK, tex); *detail = b; } return 2; }
  memcpy((char*)arr + 0x20, png.data(), png.size());
  void* la[2] = { tex, arr }; void* ok = Invoke(load, nullptr, la);
  if (!ok || !*(bool*)((char*)ok + 0x10)) return 3;
  Pin(tex); *out = tex;
  return 0;
}
// models\<name>, loaded once; a missing or bad file is logged once and stays flat for the session. A texture Unity
// destroyed anyway is loaded again.
static void* UiTex(const char* name) {
  static std::map<std::string, void*> got; static std::set<std::string> failed;
  void*& t = got[name];
  if (Alive(t)) return t;
  if (failed.count(name)) return nullptr;
  std::string det; int r = LoadPngTex(g_folder + "\\models\\" + name, &t, &det);
  if (r) {
    failed.insert(name); t = nullptr;
    Log("ui look: models\\%s %s; drawn flat", name, r == 1 ? "not found" : r == 2 ? ("could not be loaded (" + det + ")").c_str() : "is not an image the game can read");
  }
  return t;
}
static bool DrawUi(Rect4 r, const char* name, Col4 tint = { 1, 1, 1, 1 }) {   // false: no texture, draw the fallback
  void* t = UiTex(name); if (!t) return false;
  GuiColor(tint); DrawTex(r, t); GuiColor({ 1, 1, 1, 1 });
  return true;
}
// An invisible button: call it on every event, in the same order; the look is drawn separately on Repaint.
static bool HitBox(Rect4 r) {
  if (!m_guiButtonSt) return GuiButton(r, "");
  void* a[3] = { &r, g_guiContentNone, g_guiStyleNone }; void* res = Invoke(m_guiButtonSt, nullptr, a);
  return res && *(bool*)((char*)res + 0x10);
}
// Letter spacing without measuring: a space between characters (UTF-8 aware), so "LOOT" reads "L O O T".
static std::string Spaced(const std::string& s) { std::string o; for (size_t k = 0; k < s.size(); k++) { if (k && ((unsigned char)s[k] & 0xC0) != 0x80) o += ' '; o += s[k]; } return o; }
// Rough text widths (no CalcSize in this build): about 7.3 px per character at 13 px in Kanit, 5% more in Arial.
static float CharPx(int size) { return 7.3f * size / 13.0f * (g_uiFontKanit ? 1.0f : 1.05f); }
static float CapsWidth(const std::string& s, int size) { float w = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) w += (c == ' ' ? 0.3f : 0.62f) * size; return w; }
// Word wrap by character count; follow-on lines start with indent. NewsLines and the note card share it.
static void WrapChars(std::string l, size_t maxc, const std::string& indent, std::vector<std::string>* out) {
  if (maxc < indent.size() + 8) maxc = indent.size() + 8;
  while (l.size() > maxc) {
    size_t cut = l.rfind(' ', maxc); if (cut == std::string::npos || cut < maxc / 2) cut = maxc;
    out->push_back(l.substr(0, cut)); l = indent + l.substr(cut + (l[cut] == ' ' ? 1 : 0));
  }
  out->push_back(l);
}
static std::vector<std::string> Wrap(const std::string& s, float width, int size, bool bold = false) {
  std::vector<std::string> v; WrapChars(s, (size_t)(width / (CharPx(size) * (bold ? 1.06f : 1.0f))), "", &v); return v;
}
static void PaperSheet(Rect4 s) {                                      // drop shadow, then the manila sheet
  if (!DrawUi({ s.x - 24, s.y - 24, s.w + 48, s.h + 48 }, "ui_shadow.png")) FillC({ s.x + 4, s.y + 8, s.w, s.h }, Hex(0, 0.35f));
  if (!DrawUi(s, "ui_paper.png")) { FillC(s, Hex(kPaper)); Frame(s, 1, Hex(0xb5a47c)); }
  GuiColor({ 1, 1, 1, 1 });
}
static void DashedRule(float x, float y, float w, Col4 c) { GuiColor(c); for (float k = 0; k < w; k += 7) Fill({ x + k, y, w - k < 4 ? w - k : 4, 1 }); }
static void InkCheck(Rect4 r, bool on) {                               // r: 16x16
  Frame(r, 2, Hex(kInk));
  if (on && !DrawUi({ r.x - 1, r.y - 1, 18, 18 }, "ui_x.png")) Text({ r.x - 4, r.y - 8, 24, 32 }, "X", 15, kBlueHex, kCenter, true);
}
static void InkArrow(Rect4 r, bool right, bool dim) {                  // r: 14x16; dim at the value's limit
  Col4 tint = { 1, 1, 1, dim ? 0.4f : 1.0f };
  if (DrawUi(r, right ? "ui_arrow_r.png" : "ui_arrow_l.png", tint)) return;
  g_textAlpha = tint.a; Text({ r.x - 4, r.y - 6, r.w + 8, 28 }, right ? ">" : "<", 16, kInkHex, kCenter, true); g_textAlpha = 1;
}
// Outline: ink frame and text, highlighter on hover. Filled: ink box, paper text.
static bool InkButton(Rect4 r, const std::string& label, int type, Vec2 mouse, bool filled) {
  bool click = HitBox(r);
  if (type == 7) {
    bool hot = Inside(mouse, r);
    if (filled) FillC(r, Hex(hot ? 0x3d3b35 : kInk));
    else { if (hot) FillC(r, Hilite()); Frame(r, 2, Hex(kInk)); }
    Text(r, label, 12, filled ? kPaperHex : kInkHex, kCenter, filled);
  }
  return click;
}
