// crew weapon looks (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- crew weapon looks ---------------------------------------------------------------------------------------------
// Core.CrewMemberCombat.LateUpdate (read 2026-11-04) keeps its target in closest and counts aimTime down from 1 while it
// holds one; below 0 it calls GunMount.shoot every frame and shoot keeps the gun's cooldown. A hook around it sees a shot
// as a change of the mount's shotCounter (a burst resets the counter, so any change is one shot). Four parts, the last
// three switched in drops.ini / Mod Options while playing: log lines and a battle summary; crew_tracers: the gun's
// projectile types get red copies (palette slot kCrewSlot; rockets and rifle grenades keep their look, a gun with a
// rarity or cosmetic color keeps it); crew_marker: a square over the crew member, drawn in the OnGUI pass;
// crew_aim_line: dashes from the weapon to the target while aiming, one solid line once ready, bright on each shot.
// The player's own boat only, in battle only. Co-op: the red copies sit on the GunType, so on this machine the other
// players' copies of that gun fire them too; marker and line show this machine's own crew only.
static const int kCrewSlot = kPaletteN - 1;                          // the red palette color
static const int kDashN = 8;                                         // dashes in an aim line while the crew member aims
struct CrewAim {
  void* h = nullptr;                                                 // pins the CrewMemberCombat
  void *mount = nullptr, *gt = nullptr, *target = nullptr;
  std::string who, gun;
  float aim = 1; DWORD seen = 0, flashAt = 0;
  V3 from{}, to{}, head{};
  void* seg[kDashN] = {}; void* segH[kDashN] = {}; bool segOn[kDashN] = {};
};
static std::unordered_map<void*, CrewAim> g_crewAims;                // CrewMemberCombat -> its state, this battle
struct CrewStat { int shots = 0, kills = 0; std::set<std::string> targets; };
static std::map<std::string, CrewStat> g_crewStats;                  // "name (gun)" -> numbers for the battle summary
static void* g_crewEng = nullptr;
static bool g_crewOn = false;                                        // the hook is in
static int g_crewLogs = 0, g_crewAimLogs = 0, g_crewKillLogs = 0, g_crewLinesMade = 0, g_crewFaults = 0, g_crewBattleFaults = 0, g_crewMarkFaults = 0;
struct CrewApi {
  int weapon = -1, aimTime = -1, closest = -1, member = -1, wsSel = -1, counter = -1, gunType = -1, character = -1, cName = -1, cNameCustom = -1, hl = -1;
  void *trPos = nullptr, *camMain = nullptr, *camW2S = nullptr, *goCls = nullptr, *goCtor = nullptr, *addComp = nullptr, *lrType = nullptr;
  void *lrSetPos = nullptr, *lrStart = nullptr, *lrEnd = nullptr, *lrWidth = nullptr, *lrCount = nullptr;
  void *rEnabled = nullptr, *rShadows = nullptr, *rRecv = nullptr, *rGetMat = nullptr, *rSetMat = nullptr, *matSetColor = nullptr, *matShader = nullptr;
  void *destroy = nullptr, *mat = nullptr; bool matTried = false, lineOk = false;
};
static CrewApi g_cw;

static std::string CrewClean(std::string n) {
  const std::string suffix = "(Clone)";
  while (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0) n.erase(n.size() - suffix.size());
  while (!n.empty() && n.back() == ' ') n.pop_back();
  return n;
}
static bool CrewPos(void* comp, V3* out) {
  void* t = comp ? Invoke(m_getTransform, comp) : nullptr;
  void* p = t ? Invoke(g_cw.trPos, t) : nullptr;
  if (!p) return false;
  *out = *(V3*)((char*)p + 0x10); return true;
}
static float CrewDist(const V3& a, const V3& b) { float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z; return sqrtf(x * x + y * y + z * z); }
static std::string CrewName(void* cmc) {
  void* cm = g_cw.member >= 0 ? *(void**)((char*)cmc + g_cw.member) : nullptr;
  void* ch = cm && g_cw.character >= 0 ? *(void**)((char*)cm + g_cw.character) : nullptr;
  for (int off : { g_cw.cNameCustom, g_cw.cName }) {
    void* s = ch && off >= 0 ? *(void**)((char*)ch + off) : nullptr;
    if (s && *(int*)((char*)s + 0x10) > 0) return Str(s);
  }
  return cm ? CrewClean(NameOf(cm)) : std::string("a crew member");
}

// Red copies of a crew gun's projectile types, made once per GunType and put in its ammo lists (own lists, so a shell
// type shared with mounted guns, like the handheld M1919's '30-06 cal Round', keeps its look there). CrewBattleTick
// points their style at the red renderer while crew_tracers is on, and back at their own style when it is off.
static void CrewTracerFor(void* gt, const std::string& gun) {
  if (!gt || g_crewTypes.count(gt) || g_paletteDead) return;
  std::vector<PaletteShot>& v = g_crewTypes[gt];
  Pin(gt);
  if (g_offPtStyle == -2) g_offPtStyle = FieldOffset("Core", "ProjectileType", "style");
  if (g_offPtStyle < 0 || g_offProjTag < 0) { Log("crew tracers: ProjectileType.style or overrideTag missing; '%s' keeps its tracers", gun.c_str()); return; }
  for (int a = 0; a < 3; a++) {
    void* l = g_offAmmo[a] >= 0 ? *(void**)((char*)gt + g_offAmmo[a]) : nullptr;
    for (int i = 0; i < ListCount(l); i++)
      for (const PaletteShot& ps : g_paletteShots) if (ps.ptype == ListAt(l, i)) { Log("crew tracers: '%s' keeps its own tracer color", gun.c_str()); return; }
  }
  std::vector<std::pair<void*, void*>> done; int rockets = 0;
  for (int a = 0; a < 3; a++) {
    void* l = g_offAmmo[a] >= 0 ? *(void**)((char*)gt + g_offAmmo[a]) : nullptr;
    if (!l || ListCount(l) == 0) continue;
    void* own = CopyList(l);
    if (!own) { Log("crew tracers: could not copy an ammo list of '%s'", gun.c_str()); continue; }
    SetRef(gt, (char*)gt + g_offAmmo[a], own);
    void* arr = *(void**)((char*)own + 0x10);
    for (int i = 0; i < ListCount(own); i++) {
      void* src = ListAt(own, i);
      if (!src) continue;
      if (g_crewOrigOf.count(src)) continue;                           // already a red copy (a GunType copied from a crew gun)
      int style = *(int*)((char*)src + g_offPtStyle);
      if (style == 3 || style == 4) { rockets++; continue; }          // Rocket / Rocket_ASW: a rocket body, not a tracer
      void* p2 = nullptr;
      for (auto& d : done) if (d.first == src) p2 = d.second;
      if (!p2) {
        p2 = CloneObject(src);
        if (!p2) continue;
        std::string nm = NameOf(src) + " [crew]";
        SetName(p2, nm.c_str());
        SetRef(p2, (char*)p2 + g_offProjTag, il2cpp_string_new(("WM_" + nm).c_str()));
        v.push_back({ p2, Pin(p2), kCrewSlot, style });
        g_crewOrigOf[p2] = src;
        done.push_back({ src, p2 });
      }
      SetRef(arr, (char*)arr + 0x20 + 8 * (size_t)i, p2);
    }
  }
  Log("crew tracers: '%s' gets red tracers (%d projectile type(s)%s)", gun.c_str(), (int)done.size(), rockets ? "; its rockets keep their look" : "");
}

// The aim line's material, found once: a built-in shader that draws the line's own colors, else the crew member's own
// LineRenderer (CrewMember.HL), else the red tracer material. A copy is used, so the game's own stays as it is.
// A new Material on a built-in shader found by name (Shader.Find), or null when the build has no such shader. Shared by
// the crew aim line and the effect streaks.
static void* MaterialFromShader(const char* n) {
  static void *find = nullptr, *mk = nullptr, *mctor = nullptr; static bool tried = false;
  if (!tried) {
    tried = true;
    void* sk = FindClass("UnityEngine", "Shader"); find = sk ? FindMethod(sk, "Find", 1) : nullptr;
    mk = FindClass("UnityEngine", "Material"); mctor = mk ? FindMethod(mk, ".ctor", 1, "UnityEngine.Shader") : nullptr;
  }
  void* fa[1] = { il2cpp_string_new(n) };
  void* sh = find ? Invoke(find, nullptr, fa) : nullptr;
  if (!sh || !Alive(sh) || !mctor) return nullptr;
  void* m = il2cpp_object_new(mk); void* ca[1] = { sh }; void* exc = nullptr;
  il2cpp_runtime_invoke(mctor, m, ca, &exc);
  return !exc && Alive(m) ? m : nullptr;
}
static void* CrewLineMaterial(void* cmc) {
  CrewApi& a = g_cw;
  if (a.matTried) return a.mat;
  a.matTried = true;
  void* base = nullptr; std::string from;
  if ((base = MaterialFromShader("Sprites/Default"))) from = "shader Sprites/Default";
  if (!base) {
    void* cm = cmc && a.member >= 0 ? *(void**)((char*)cmc + a.member) : nullptr;
    void* hl = cm && a.hl >= 0 ? *(void**)((char*)cm + a.hl) : nullptr;
    if (hl && Alive(hl) && a.rGetMat && (base = Invoke(a.rGetMat, hl))) from = "the crew member's own line (CrewMember.HL)";
  }
  for (const char* n : { "Legacy Shaders/Particles/Alpha Blended", "Particles/Standard Unlit", "Unlit/Color" })
    if (!base && (base = MaterialFromShader(n))) from = std::string("shader ") + n;
  if (!base && g_paletteBase >= 0) {                                   // the red tracer renderer's material
    void* pm = FindClass("Core", "ProjectileManager");
    void* fi = pm ? il2cpp_class_get_field_from_name(pm, "instance") : nullptr; void* fr = pm ? il2cpp_class_get_field_from_name(pm, "render") : nullptr;
    void* inst = nullptr; if (fi) il2cpp_field_static_get_value(fi, &inst);
    void* arr = inst && fr ? *(void**)((char*)inst + il2cpp_field_get_offset(fr)) : nullptr;
    int at = g_paletteBase + kCrewSlot;
    void* r = arr && ArrLen(arr) > at ? ArrAt(arr, at) : nullptr;
    void* fLod = r ? il2cpp_class_get_field_from_name(il2cpp_object_get_class(r), "LOD") : nullptr;
    void* lod = fLod ? *(void**)((char*)r + il2cpp_field_get_offset(fLod)) : nullptr;
    void* rt = lod && ArrLen(lod) > 0 ? ArrAt(lod, 0) : nullptr;
    void* fMat = rt ? il2cpp_class_get_field_from_name(il2cpp_object_get_class(rt), "material") : nullptr;
    if (fMat && (base = *(void**)((char*)rt + il2cpp_field_get_offset(fMat)))) from = "the red tracer material";
  }
  void* m = base ? CloneObject(base) : nullptr;
  if (!m) { Log("crew aim line: no material to draw it with; aim lines are off"); a.lineOk = false; return nullptr; }
  Pin(m); a.mat = m;
  if (from == "shader Unlit/Color" && a.matSetColor) {                 // no vertex colors there: the line is red
    float c[4] = { kPalette[kCrewSlot][0], kPalette[kCrewSlot][1], kPalette[kCrewSlot][2], 0.7f };
    void* sa[2] = { il2cpp_string_new("_Color"), c }; Invoke(a.matSetColor, m, sa);
  }
  void* sh = a.matShader ? Invoke(a.matShader, m) : nullptr;
  Log("crew aim line: material '%s' (shader '%s'), from %s", NameOf(m).c_str(), sh ? NameOf(sh).c_str() : "?", from.c_str());
  return m;
}
static bool CrewLineMake(CrewAim& c, void* cmc) {
  if (!CrewLineMaterial(cmc)) return false;
  for (int i = 0; i < kDashN; i++) {
    void* go = il2cpp_object_new(g_cw.goCls);
    void* na[1] = { il2cpp_string_new("WM crew aim line") }; void* exc = nullptr;
    il2cpp_runtime_invoke(g_cw.goCtor, go, na, &exc);
    void* ta[1] = { g_cw.lrType };
    void* lr = exc ? nullptr : Invoke(g_cw.addComp, go, ta);
    if (!lr) { Log("crew aim line: could not make a LineRenderer; aim lines are off"); g_cw.lineOk = false; return false; }
    void* ma[1] = { g_cw.mat }; Invoke(g_cw.rSetMat, lr, ma);
    if (g_cw.rShadows) { int off = 0; void* sa[1] = { &off }; Invoke(g_cw.rShadows, lr, sa); }
    if (g_cw.rRecv) { bool no = false; void* ra[1] = { &no }; Invoke(g_cw.rRecv, lr, ra); }
    if (g_cw.lrCount) { int two = 2; void* pa[1] = { &two }; Invoke(g_cw.lrCount, lr, pa); }
    c.seg[i] = lr; c.segH[i] = Pin(lr); c.segOn[i] = true;          // a new renderer is on; CrewLine sets each one
  }
  if (g_crewLinesMade++ == 0) Log("crew aim line: first line made for %s (%s)", c.who.c_str(), c.gun.c_str());
  return true;
}
static void CrewLineHide(CrewAim& c) {
  for (int i = 0; i < kDashN; i++) {
    if (!c.segOn[i] || !Alive(c.seg[i])) continue;
    bool off = false; void* ea[1] = { &off }; Invoke(g_cw.rEnabled, c.seg[i], ea); c.segOn[i] = false;
  }
}
static void CrewLineKill(CrewAim& c) {
  for (int i = 0; i < kDashN; i++) {
    if (!Alive(c.seg[i])) continue;
    void* go = Invoke(m_getGameObject, c.seg[i]);
    if (go && g_cw.destroy) { void* da[1] = { go }; Invoke(g_cw.destroy, nullptr, da); }
    else { bool off = false; void* ea[1] = { &off }; Invoke(g_cw.rEnabled, c.seg[i], ea); }
  }
}
static void CrewLineDrop(CrewAim& c) { for (int i = 0; i < kDashN; i++) { Unpin(c.segH[i]); c.segH[i] = c.seg[i] = nullptr; c.segOn[i] = false; } }
// Dashes while the crew member aims (they brighten as aimTime runs out), one solid line once ready, bright on a shot.
static void CrewLine(CrewAim& c, void* cmc, DWORD now) {
  if (!c.target) { CrewLineHide(c); return; }
  if (c.seg[0] && !Alive(c.seg[0])) CrewLineDrop(c);                   // gone with its scene
  if (!c.seg[0] && !CrewLineMake(c, cmc)) return;
  int look = now - c.flashAt < 140 ? 2 : (c.aim > 0 ? 0 : 1);
  float ready = c.aim <= 0 ? 1.0f : (c.aim >= 1 ? 0.0f : 1.0f - c.aim);
  const float* r = kPalette[kCrewSlot];
  Col4 col = look == 2 ? Col4{ 1.0f, 0.92f, 0.55f, 1.0f } : Col4{ r[0], r[1] + 0.15f, r[2] + 0.15f, look == 1 ? 0.75f : 0.18f + 0.3f * ready };
  Col4 end = col; if (look == 0) end.a *= 0.6f;
  float w = look == 2 ? 0.9f : (look == 1 ? 0.5f : 0.35f);
  V3 d{ c.to.x - c.from.x, c.to.y - c.from.y, c.to.z - c.from.z };
  for (int i = 0; i < kDashN; i++) {
    bool on = look == 0 || i == 0;
    if (on != c.segOn[i]) { bool b = on; void* ea[1] = { &b }; Invoke(g_cw.rEnabled, c.seg[i], ea); c.segOn[i] = on; }
    if (!on) continue;
    float t0 = look == 0 ? (float)i / kDashN : 0.0f, t1 = look == 0 ? (i + 0.55f) / kDashN : 1.0f;
    V3 p0{ c.from.x + d.x * t0, c.from.y + d.y * t0, c.from.z + d.z * t0 }, p1{ c.from.x + d.x * t1, c.from.y + d.y * t1, c.from.z + d.z * t1 };
    int i0 = 0, i1 = 1;
    void* a0[2] = { &i0, &p0 }; Invoke(g_cw.lrSetPos, c.seg[i], a0);
    void* a1[2] = { &i1, &p1 }; Invoke(g_cw.lrSetPos, c.seg[i], a1);
    void* sc[1] = { &col }; Invoke(g_cw.lrStart, c.seg[i], sc);
    void* ec[1] = { &end }; Invoke(g_cw.lrEnd, c.seg[i], ec);
    void* wa[1] = { &w }; Invoke(g_cw.lrWidth, c.seg[i], wa);
  }
}

// After the game's CrewMemberCombat.LateUpdate: the target, the aim, a shot, the log lines and the line.
static void CrewTick(void* cmc, void* mountBefore, int before) {
  if (!EngagementInstance()) return;
  void* ws = *(void**)((char*)cmc + g_cw.weapon);
  void* mount = ws ? *(void**)((char*)ws + g_cw.wsSel) : nullptr;
  if (!mount || !OnPlayerBoat(mount)) return;
  DWORD now = GetTickCount();
  CrewAim& c = g_crewAims[cmc];
  if (!c.h) { c.h = Pin(cmc); c.who = CrewName(cmc); }
  if (c.mount != mount) { c.mount = mount; c.gt = *(void**)((char*)mount + g_cw.gunType); c.gun = c.gt ? CrewClean(NameOf(c.gt)) : std::string("?"); }
  if (g_crewTracers && c.gt) CrewTracerFor(c.gt, c.gun);
  void* tgt = *(void**)((char*)cmc + g_cw.closest);
  if (tgt && !Alive(tgt)) tgt = nullptr;
  float aim = *(float*)((char*)cmc + g_cw.aimTime);
  bool shot = mount == mountBefore && *(int*)((char*)mount + g_cw.counter) != before;
  CrewPos(mount, &c.from);
  void* cm = g_cw.member >= 0 ? *(void**)((char*)cmc + g_cw.member) : nullptr;
  if (cm && CrewPos(cm, &c.head)) c.head.y += 2.2f; else { c.head = c.from; c.head.y += 1.0f; }
  if (tgt && CrewPos(tgt, &c.to)) c.to.y += 1.5f;
  else if (tgt) tgt = nullptr;
  if (tgt && tgt != c.target && g_crewAimLogs < 8) {
    g_crewAimLogs++;
    Log("crew: %s (%s) aims at %s, %.0f m", c.who.c_str(), c.gun.c_str(), CrewClean(NameOf(tgt)).c_str(), CrewDist(c.from, c.to));
  }
  if (shot) {
    c.flashAt = now;
    CrewStat& s = g_crewStats[c.who + " (" + c.gun + ")"];
    s.shots++;
    if (tgt && s.targets.size() < 12) s.targets.insert(CrewClean(NameOf(tgt)));
    if (g_crewLogs < 10) {
      g_crewLogs++;
      Log("crew: %s (%s) fires at %s, %.0f m (shot %d this battle)", c.who.c_str(), c.gun.c_str(), tgt ? CrewClean(NameOf(tgt)).c_str() : "(target gone)", tgt ? CrewDist(c.from, c.to) : 0.0f, s.shots);
    }
  }
  c.target = tgt; c.aim = aim; c.seen = now;
  if (g_crewLine && g_cw.lineOk) CrewLine(c, cmc, now); else CrewLineHide(c);
}
static void (*g_cmcLateOrig)(void*, void*) = nullptr;
static void Hooked_CmcLate(void* self, void* method) {
  void* mount = nullptr; int before = 0;
  if (g_ready && g_crewFaults >= 0) {
    __try {
      void* ws = *(void**)((char*)self + g_cw.weapon);
      mount = ws ? *(void**)((char*)ws + g_cw.wsSel) : nullptr;
      if (mount) before = *(int*)((char*)mount + g_cw.counter);
    } __except (EXCEPTION_EXECUTE_HANDLER) { mount = nullptr; }
  }
  g_cmcLateOrig(self, method);
  if (!g_ready || g_crewFaults < 0) return;
  __try { CrewTick(self, mount, before); }
  __except (FaultFilter("crew weapon looks", GetExceptionInformation())) { g_crewFaults = -1; }
}
// A kill credited to a crew member (CrewMember.creditKill) with their handheld weapon.
static void CrewKill(void* mount, void* killed) {
  if (!mount) return;
  for (auto& kv : g_crewAims) {
    if (kv.second.mount != mount) continue;
    CrewStat& s = g_crewStats[kv.second.who + " (" + kv.second.gun + ")"];
    s.kills++;
    if (g_crewKillLogs++ < 20) Log("crew: %s (%s) killed %s", kv.second.who.c_str(), kv.second.gun.c_str(), killed ? CrewClean(NameOf(killed)).c_str() : "?");
    return;
  }
}
static void CrewBattleSummary() {
  if (g_crewStats.empty()) {
    if (!g_crewAims.empty()) Log("crew weapons, last battle: no shots from your crew (%d crew member(s) with a weapon seen)", (int)g_crewAims.size());
    return;
  }
  int total = 0;
  for (auto& s : g_crewStats) {
    total += s.second.shots;
    std::string t; for (const std::string& n : s.second.targets) t += (t.empty() ? "" : ", ") + n;
    Log("crew weapons, last battle: %s fired %d shot(s), %d kill(s), at %s", s.first.c_str(), s.second.shots, s.second.kills, t.empty() ? "-" : t.c_str());
  }
  Log("crew weapons, last battle: %d shot(s) in all; %d aim line(s) made; tracers %s, marker %s, aim line %s", total, g_crewLinesMade,
      g_crewTracers ? "on" : "off", g_crewMarker ? "on" : "off", g_crewLine ? "on" : "off");
}
static void CrewHideAll() { for (auto& kv : g_crewAims) CrewLineKill(kv.second); }
static void SafeCrewHideAll() { __try { CrewHideAll(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
// Every frame from ProjectileManager.LateUpdate (after ExtendPalette): the red copies follow crew_tracers, lines of crew
// members no longer seen are hidden, and at a battle change the summary is logged and the state dropped.
static void CrewBattleTick() {
  if (g_offPtStyle >= 0)
    for (auto& kv : g_crewTypes)
      for (const PaletteShot& ps : kv.second) {
        int want = g_crewTracers && g_paletteBase >= 0 && !g_paletteDead ? g_paletteBase + ps.slot : ps.orig;
        int* st = (int*)((char*)ps.ptype + g_offPtStyle);
        if (*st != want) *st = want;
      }
  DWORD now = GetTickCount();
  for (auto& kv : g_crewAims) if (kv.second.seg[0] && (!g_crewLine || now - kv.second.seen > 300)) CrewLineHide(kv.second);
  void* eng = EngagementInstance();
  if (eng == g_crewEng) return;
  if (g_crewEng) CrewBattleSummary();
  for (auto& kv : g_crewAims) { CrewLineKill(kv.second); Unpin(kv.second.h); for (void* h : kv.second.segH) Unpin(h); }
  g_crewAims.clear(); g_crewStats.clear();
  g_crewLogs = g_crewAimLogs = g_crewKillLogs = g_crewLinesMade = 0;
  g_crewEng = eng;
}
// The marker: a square over each crew member holding a target, dim while aiming, red once ready, pale yellow on a shot.
static void CrewMarks() {
  if (!g_crewOn || !g_crewMarker || g_crewMarkFaults < 0 || g_crewAims.empty() || !EngagementInstance() || !g_cw.camMain || !g_cw.camW2S || g_diagFrames > 0) return;
  void* ev = Invoke(m_evCurrent, nullptr);
  if (!ev || Unbox(Invoke(m_evType, ev)) != 7) return;                 // Repaint only
  void* cam = Invoke(g_cw.camMain, nullptr); if (!cam) return;
  float sh = (float)Unbox(Invoke(m_scrH, nullptr)); if (sh <= 0) return;
  float k = sh / 1080.0f;
  DWORD now = GetTickCount();
  for (auto& kv : g_crewAims) {
    const CrewAim& c = kv.second;
    if (!c.target || now - c.seen > 400) continue;
    V3 p = c.head; void* a[1] = { &p };
    void* sb = Invoke(g_cw.camW2S, cam, a); if (!sb) continue;
    V3 s = *(V3*)((char*)sb + 0x10);
    if (s.z <= 0) continue;                                            // behind the camera
    bool flash = now - c.flashAt < 160;
    float ready = c.aim <= 0 ? 1.0f : (c.aim >= 1 ? 0.0f : 1.0f - c.aim);
    float sz = (flash ? 16.0f : 11.0f) * k, x = s.x, y = sh - s.y, o = 2.0f * k;
    Col4 col = flash ? Hex(0xffe48a) : (c.aim > 0 ? Hex(0xff5a4a, 0.35f + 0.45f * ready) : Hex(0xff3a2a));
    FillC({ x - sz / 2 - o, y - sz / 2 - o, sz + 2 * o, sz + 2 * o }, Hex(0x000000, 0.55f));   // dark edge: the sea and sky are bright
    FillC({ x - sz / 2, y - sz / 2, sz, sz }, col);
  }
  GuiColor({ 1, 1, 1, 1 });
}
static void SafeCrewMarks() { __try { CrewMarks(); } __except (FaultFilter("crew marker", GetExceptionInformation())) { g_crewMarkFaults = -1; } }
static void CrewVisInstall() {
  void* cmc = FindClass("Core", "CrewMemberCombat"); void* ws = FindClass("Core", "WeaponSystem"); void* gm = FindClass("Core", "GunMount");
  void* cm = FindClass("Core", "CrewMember"); void* cc = FindClass("Core", "CrewCharacter");
  CrewApi& a = g_cw;
  a.weapon = FieldOffset(cmc, "crewWeapon"); a.aimTime = FieldOffset(cmc, "aimTime"); a.closest = FieldOffset(cmc, "closest"); a.member = FieldOffset(cmc, "crewMember");
  a.wsSel = FieldOffset(ws, "selected"); a.counter = FieldOffset(gm, "shotCounter"); a.gunType = FieldOffset(gm, "gunType");
  a.character = FieldOffset(cm, "_character"); a.cName = FieldOffset(cc, "name"); a.cNameCustom = FieldOffset(cc, "nameCustom"); a.hl = FieldOffset(cm, "HL");
  void* tr = FindClass("UnityEngine", "Transform"); a.trPos = tr ? FindMethod(tr, "get_position", 0) : nullptr;
  void* cam = FindClass("UnityEngine", "Camera"); a.camMain = cam ? FindMethod(cam, "get_main", 0) : nullptr; a.camW2S = cam ? FindMethod(cam, "WorldToScreenPoint", 1) : nullptr;
  a.goCls = FindClass("UnityEngine", "GameObject");
  a.goCtor = a.goCls ? FindMethod(a.goCls, ".ctor", 1, "System.String") : nullptr; a.addComp = a.goCls ? FindMethod(a.goCls, "AddComponent", 1, "System.Type") : nullptr;
  void* lr = FindClass("UnityEngine", "LineRenderer");
  a.lrType = lr ? il2cpp_type_get_object(il2cpp_class_get_type(lr)) : nullptr;
  a.lrSetPos = lr ? FindMethod(lr, "SetPosition", 2) : nullptr; a.lrStart = lr ? FindMethod(lr, "set_startColor", 1) : nullptr; a.lrEnd = lr ? FindMethod(lr, "set_endColor", 1) : nullptr;
  a.lrWidth = lr ? FindMethod(lr, "set_widthMultiplier", 1) : nullptr; a.lrCount = lr ? FindMethod(lr, "set_positionCount", 1) : nullptr;
  void* rk = FindClass("UnityEngine", "Renderer");
  a.rEnabled = rk ? FindMethod(rk, "set_enabled", 1) : nullptr; a.rShadows = rk ? FindMethod(rk, "set_shadowCastingMode", 1) : nullptr; a.rRecv = rk ? FindMethod(rk, "set_receiveShadows", 1) : nullptr;
  a.rGetMat = rk ? FindMethod(rk, "get_sharedMaterial", 0) : nullptr; a.rSetMat = rk ? FindMethod(rk, "set_sharedMaterial", 1) : nullptr;
  void* mc = FindClass("UnityEngine", "Material");
  a.matSetColor = mc ? FindMethod(mc, "SetColor", 2, "System.String", "UnityEngine.Color") : nullptr; a.matShader = mc ? FindMethod(mc, "get_shader", 0) : nullptr;
  void* ok = FindClass("UnityEngine", "Object"); a.destroy = ok ? FindMethod(ok, "Destroy", 1, "UnityEngine.Object") : nullptr;
  if (!cmc || a.weapon < 0 || a.aimTime < 0 || a.closest < 0 || a.wsSel < 0 || a.counter < 0 || a.gunType < 0 || !a.trPos || !m_getTransform) {
    Log("crew weapons: a lookup failed (crewWeapon %d aimTime %d closest %d selected %d shotCounter %d gunType %d position %p); crew weapon looks are off",
        a.weapon, a.aimTime, a.closest, a.wsSel, a.counter, a.gunType, a.trPos);
    return;
  }
  a.lineOk = a.goCls && a.goCtor && a.addComp && a.lrType && a.lrSetPos && a.lrStart && a.lrEnd && a.lrWidth && a.rEnabled && a.rSetMat;
  if (!PatchPointer(cmc, "LateUpdate", 0, (void*)&Hooked_CmcLate, (void**)&g_cmcLateOrig)) { Log("crew weapons: could not hook CrewMemberCombat.LateUpdate; crew weapon looks are off"); return; }
  g_crewOn = true;
  Log("crew weapons: ready (CrewMemberCombat.LateUpdate hooked; tracers %s, marker %s%s, aim line %s%s)", g_crewTracers ? "on" : "off", g_crewMarker ? "on" : "off",
      a.camMain && a.camW2S ? "" : " (Camera methods MISSING, no marker)", g_crewLine ? "on" : "off", a.lineOk ? "" : " (LineRenderer pieces MISSING, no lines)");
}

// Name tags over living Rare/Elite ships, drawn from the OnGUI hook during battle.
// Marked ships (Marking, 0.9.3) get one too, in red, with the bonus and the seconds left; a marked Rare/Elite keeps its own
// tag and adds MARKED to its second line. enemies.ini tags=0 turns both off.
static void EliteTags() {
  bool elites = g_elitesOn && !g_elites.empty();
  if (!g_eliteTags || (!elites && g_marks.empty()) || !EngagementInstance() || !m_camMain || !m_camW2S || !m_trGetPos || g_diagFrames > 0) return;
  void* ev = Invoke(m_evCurrent, nullptr);
  if (!ev || Unbox(Invoke(m_evType, ev)) != 7) return;                 // Repaint only
  void* cam = Invoke(m_camMain, nullptr); if (!cam) return;
  float sh = (float)Unbox(Invoke(m_scrH, nullptr)); if (sh <= 0) return;
  float k = sh / 1080.0f;
  // Tags are placed nearest ship first; a tag that would cover one already placed moves up above it (ships far away
  // sit close together on the horizon, test 2026-10-27). Far tags are smaller and leave out the trait line.
  struct Tag { float x, y, dist; const EliteInfo* info; const MarkInfo* mark; void* ce; };
  std::vector<Tag> tags;
  DWORD now = GetTickCount();
  auto add = [&](void* ce, const EliteInfo* info, const MarkInfo* mark) {
    void* tr = Invoke(m_getTransform, ce); if (!tr) return;
    void* pb = Invoke(m_trGetPos, tr); if (!pb) return;
    V3 p = *(V3*)((char*)pb + 0x10); p.y += 12.0f;
    void* a[1] = { &p }; void* sb = Invoke(m_camW2S, cam, a); if (!sb) return;
    V3 s = *(V3*)((char*)sb + 0x10);
    if (s.z <= 0 || s.z > g_tagRange) return;                          // behind the camera or too far
    tags.push_back({ s.x, sh - s.y, s.z, info, mark, ce });
  };
  auto markOf = [&](void* ce) -> const MarkInfo* { auto m = g_marks.find(ce); return m != g_marks.end() && (int)(m->second.until - now) > 0 ? &m->second : nullptr; };
  if (elites) for (auto& e : g_elites) add(e.first, &e.second, markOf(e.first));
  for (auto& m : g_marks) if ((int)(m.second.until - now) > 0 && !(elites && g_elites.count(m.first))) add(m.first, nullptr, &m.second);
  std::sort(tags.begin(), tags.end(), [](const Tag& a, const Tag& b) { return a.dist < b.dist; });
  std::vector<Rect4> placed;
  GuiThemeBegin();
  for (const Tag& t : tags) {
    float z = t.dist < 400 ? 1.0f : (t.dist > 1600 ? 0.6f : 1.0f - 0.4f * (t.dist - 400) / 1200);
    bool sub = t.dist < 1200 || t.mark;                               // a marked ship keeps its MARKED line at any range
    float f1 = 18 * k * z, f2 = 13 * k * z, w = 26 * f1, h = f1 * 1.45f + (sub ? f2 * 1.45f : 0);
    Rect4 r{ t.x - w / 2, t.y - h, w, h };
    for (bool moved = true; moved; ) {
      moved = false;
      for (const Rect4& q : placed)
        if (r.x < q.x + q.w && q.x < r.x + r.w && r.y < q.y + q.h && q.y < r.y + r.h) { r.y = q.y - r.h - 2; moved = true; }
    }
    placed.push_back(r);
    const char* hex = !t.info ? "#ff4d4d" : (t.info->tier == 2 ? "#ffb300" : "#4fc3f7");
    const std::string& title = t.info ? t.info->name : t.mark->ship;
    std::string marked;
    if (t.mark) { char b[64]; snprintf(b, sizeof b, "MARKED +%d%%  ·  %d s", (int)std::lround(t.mark->bonus * 100), (int)((t.mark->until - now + 999) / 1000)); marked = b; }
    float o = 1.5f * k;                                                // dark shadow: the sky behind the tags is bright
    Text({ r.x + o, r.y + o, w, f1 * 1.45f }, title, (int)f1, "#000000c0", kCenter, true);
    Text({ r.x, r.y, w, f1 * 1.45f }, title, (int)f1, hex, kCenter, true);
    if (sub) {
      std::string line = !t.info || t.dist >= 1200 ? marked : std::string(t.info->tier == 2 ? "ELITE" : "RARE") + (t.info->traits.empty() ? "" : "  ·  " + t.info->traits) + (marked.empty() ? "" : "  ·  " + marked);
      Text({ r.x + o, r.y + f1 * 1.45f + o, w, f2 * 1.45f }, line, (int)f2, "#000000c0", kCenter);
      Text({ r.x, r.y + f1 * 1.45f, w, f2 * 1.45f }, line, (int)f2, "#f2f2f2", kCenter);
    }
  }
  GuiThemeEnd();
}

