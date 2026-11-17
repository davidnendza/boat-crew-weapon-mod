// torpedo looks: chomper + FMOD sound, the Dolphin leap, tube looks, TorpInstall (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- the chomper look and sound ---------------------------------------------------------------------------------------
// FMOD core API straight from fmodstudio.dll (FMOD 2.03.13 in this build; Unity's AudioSource is stripped). The game's
// Studio system is FMODUnity.RuntimeManager.instance.studioSystem; its core system plays our own .wav files as 3D
// channels. The core 3D listener is set from the Studio listener every frame, and the volume follows the game's sound
// effects slider (SettingsAudio.settings[1], a Studio global parameter, which BlurModulator also scales by game speed).
struct FmodVec { float x, y, z; };
struct Fmod3D { FmodVec pos, vel, fwd, up; };
struct FmodApi {
  bool tried = false, ok = false; DWORD lastTry = 0;
  void *studio = nullptr, *core = nullptr, *chomp = nullptr, *gulp = nullptr, *boom = nullptr;   // boom: guns with fx kick
  int (*createSound)(void*, const char*, unsigned, void*, void**) = nullptr;
  int (*playSound)(void*, void*, void*, int, void**) = nullptr;
  int (*set3D)(void*, const FmodVec*, const FmodVec*) = nullptr;
  int (*minMax)(void*, float, float) = nullptr;
  int (*volume)(void*, float) = nullptr;
  int (*pitch)(void*, float) = nullptr;
  int (*paused)(void*, int) = nullptr;
  int (*stop)(void*) = nullptr;
  int (*coreOf)(void*, void**) = nullptr;
  int (*studioListener)(void*, int, Fmod3D*, FmodVec*) = nullptr;
  int (*setListener)(void*, int, const FmodVec*, const FmodVec*, const FmodVec*, const FmodVec*) = nullptr;
  int (*param)(void*, const char*, float*, float*) = nullptr;
  int (*position)(void*, unsigned*, unsigned) = nullptr;             // FMOD_Channel_GetPosition (optional)
  std::string sfxParam;
};
static FmodApi g_fm;
static bool FmodReady() {
  if (g_fm.ok || (g_fm.tried && !g_fm.lastTry)) return g_fm.ok;
  DWORD now = GetTickCount();
  if (g_fm.lastTry && now - g_fm.lastTry < 2000) return false;       // the RuntimeManager may not exist yet
  g_fm.lastTry = now;
  FmodApi& a = g_fm;
  if (!a.tried) {
    a.tried = true;
    HMODULE m = GetModuleHandleA("fmodstudio.dll");
    auto P = [&](const char* n) { return m ? (void*)GetProcAddress(m, n) : nullptr; };
    *(void**)&a.createSound = P("FMOD_System_CreateSound"); *(void**)&a.playSound = P("FMOD_System_PlaySound");
    *(void**)&a.set3D = P("FMOD_Channel_Set3DAttributes"); *(void**)&a.minMax = P("FMOD_Channel_Set3DMinMaxDistance");
    *(void**)&a.volume = P("FMOD_Channel_SetVolume"); *(void**)&a.pitch = P("FMOD_Channel_SetPitch"); *(void**)&a.paused = P("FMOD_Channel_SetPaused");
    *(void**)&a.stop = P("FMOD_Channel_Stop"); *(void**)&a.coreOf = P("FMOD_Studio_System_GetCoreSystem");
    *(void**)&a.studioListener = P("FMOD_Studio_System_GetListenerAttributes"); *(void**)&a.setListener = P("FMOD_System_Set3DListenerAttributes");
    *(void**)&a.param = P("FMOD_Studio_System_GetParameterByName"); *(void**)&a.position = P("FMOD_Channel_GetPosition");
    if (!m || !a.createSound || !a.playSound || !a.set3D || !a.minMax || !a.volume || !a.pitch || !a.paused || !a.stop || !a.coreOf || !a.studioListener || !a.setListener) {
      Log("chomper sound: fmodstudio.dll %p or one of its functions is missing; the chompers are silent", m); a.lastTry = 0; return false;
    }
  }
  void* rm = FindClass("FMODUnity", "RuntimeManager");
  void* fi = rm ? il2cpp_class_get_field_from_name(rm, "instance") : nullptr;
  void* fs = rm ? il2cpp_class_get_field_from_name(rm, "studioSystem") : nullptr;
  if (!fi || !fs) { Log("chomper sound: FMODUnity.RuntimeManager not found; the chompers are silent"); a.lastTry = 0; return false; }
  void* inst = nullptr; il2cpp_field_static_get_value(fi, &inst);   // read, never get_Instance: that would create one
  if (!inst) return false;
  a.studio = *(void**)((char*)inst + il2cpp_field_get_offset(fs));
  int rc = a.studio ? a.coreOf(a.studio, &a.core) : -1;
  if (rc != 0 || !a.core) { Log("chomper sound: no FMOD core system (result %d); the chompers are silent", rc); a.lastTry = 0; return false; }
  std::string dir = g_folder + "\\sounds\\";
  int r1 = a.createSound(a.core, (dir + "chomp.wav").c_str(), 0x10 | 0x2 | 0x100 | 0x400000, nullptr, &a.chomp);   // 3D, loop, sample, linear-square rolloff
  int r2 = a.createSound(a.core, (dir + "gulp.wav").c_str(), 0x10 | 0x1 | 0x100 | 0x400000, nullptr, &a.gulp);
  int r3 = a.createSound(a.core, (dir + "boom.wav").c_str(), 0x10 | 0x1 | 0x100 | 0x400000, nullptr, &a.boom);
  if (r1 != 0) a.chomp = nullptr;
  if (r2 != 0) a.gulp = nullptr;
  if (r3 != 0) a.boom = nullptr;
  void* sa = FindClass("Core", "SettingsAudio");                       // the effects slider's Studio parameter
  void* fsi = sa ? il2cpp_class_get_field_from_name(sa, "instance") : nullptr; void* fss = sa ? il2cpp_class_get_field_from_name(sa, "settings") : nullptr;
  void* sai = nullptr; if (fsi) il2cpp_field_static_get_value(fsi, &sai);
  void* arr = sai && fss ? *(void**)((char*)sai + il2cpp_field_get_offset(fss)) : nullptr;
  std::string names;
  if (arr) {
    int n = ArrLen(arr);
    for (int i = 0; i < n && i < 8; i++) {
      void* s = ArrAt(arr, i); if (!s) continue;
      void* nf = il2cpp_class_get_field_from_name(il2cpp_object_get_class(s), "name");
      std::string nm = nf ? Str(*(void**)((char*)s + il2cpp_field_get_offset(nf))) : "";
      float v = -1, fv = -1; if (a.param && !nm.empty()) a.param(a.studio, nm.c_str(), &v, &fv);
      char b[96]; snprintf(b, sizeof b, "%s'%s' %.2f/%.2f", names.empty() ? "" : ", ", nm.c_str(), v, fv); names += b;
      if (i == 1) a.sfxParam = nm;
    }
  }
  a.ok = a.chomp || a.gulp || a.boom;
  Log("chomper sound: %s (chomp.wav %s, gulp.wav %s, boom.wav %s; volume sliders %s; effects volume from '%s')", a.ok ? "ready" : "the sound files did not load",
      (r1 ? "FMOD error " + std::to_string(r1) : std::string("loaded")).c_str(), (r2 ? "FMOD error " + std::to_string(r2) : std::string("loaded")).c_str(),
      (r3 ? "FMOD error " + std::to_string(r3) : std::string("loaded")).c_str(),
      names.empty() ? "not found" : names.c_str(), a.sfxParam.empty() ? "nothing, full volume" : a.sfxParam.c_str());
  a.lastTry = 0;
  return a.ok;
}
static float FmodVolume() {
  float v = 1, fv = 1;
  if (!g_fm.sfxParam.empty() && g_fm.param && g_fm.param(g_fm.studio, g_fm.sfxParam.c_str(), &v, &fv) == 0) v = fv > 1.5f ? fv / 100 : fv;
  return (float)(v < 0 ? 0 : v > 1 ? 1 : v) * (float)g_chompVolume;
}
static void FmodListener() {
  Fmod3D l = {}; FmodVec atten = {};
  if (g_fm.studioListener(g_fm.studio, 0, &l, &atten) == 0) g_fm.setListener(g_fm.core, 0, &l.pos, &l.vel, &l.fwd, &l.up);
}
static void* FmodPlay(void* sound, V3 p, float vol) {
  void* ch = nullptr;
  if (!sound || g_fm.playSound(g_fm.core, sound, nullptr, 1, &ch) != 0 || !ch) return nullptr;
  FmodVec pos = { p.x, p.y, p.z }, vel = {};
  g_fm.minMax(ch, 40, 900); g_fm.set3D(ch, &pos, &vel); g_fm.volume(ch, vol); g_fm.paused(ch, 0);
  return ch;
}
// The kick guns' boom (GfxKick), on top of the gun's own sound; follows the effects slider but not chomp_volume.
static void PlayBoom(V3 p) {
  if (!FmodReady() || !g_fm.boom) return;
  FmodListener();
  float v = 1, fv = 1;
  if (!g_fm.sfxParam.empty() && g_fm.param && g_fm.param(g_fm.studio, g_fm.sfxParam.c_str(), &v, &fv) == 0) v = fv > 1.5f ? fv / 100 : fv;
  void* ch = FmodPlay(g_fm.boom, p, v < 0 ? 0 : v > 1 ? 1 : v);
  if (ch) g_fm.minMax(ch, 150, 3000);                                   // loud a long way off
}

// The model a found torpedo shows in place of its own (TorpRun.look): 1 Chomper, 2 Chompette (two jaws), 3 the Sneaking
// Eel's commando (six parts), 4 the Flying Fish's fish (four parts). Each part is a model copy under the torpedo's root
// with its pivot at its origin.
struct TorpLook { std::vector<void*> parts, ph; void *th = nullptr, *ch = nullptr; int kind = 0; bool shown = false; std::vector<void*> stock, sh; };   // sh: GC handles of stock
static std::unordered_map<void*, TorpLook> g_torpLooks;               // Torpedo -> its look (kept on the pooled torpedo once made)
static const float kChompR = 1.3f;                                    // the ball's radius in the .obj files
static const char* const kCrawlParts[] = { "commando_body", "commando_arm_l", "commando_arm_r", "commando_leg_l", "commando_leg_r", "commando_tails" };
// each part's pivot in the commando's frame (origin: his belly on the water line, +Z ahead), metres; printed by
// tools/make_commando_assets.py, which must agree
static const V3 kCrawlPivot[] = { { 0, 0, 0 }, { -0.44f, 0.33f, 0.88f }, { 0.44f, 0.33f, 0.88f }, { -0.22f, 0.22f, -0.66f }, { 0.22f, 0.22f, -0.66f }, { 0, 0.726f, 1.078f } };
static const char* const kFishParts[] = { "flyfish_body", "flyfish_wing_l", "flyfish_wing_r", "flyfish_tail" };
// each part's pivot in the fish's frame (origin: the middle of its body, on its axis, +Z ahead), metres; printed by
// tools/make_flyfish_assets.py, which must agree, as FOLD and DIHEDRAL there with TorpFishPart
static const V3 kFishPivot[] = { { 0, 0, 0 }, { -0.22f, 0.08f, 0.75f }, { 0.22f, 0.08f, 0.75f }, { 0, 0, -1.55f } };
static std::vector<std::string> TorpLookFiles(int kind) {
  if (kind == 3) return std::vector<std::string>(std::begin(kCrawlParts), std::end(kCrawlParts));
  if (kind == 4) return std::vector<std::string>(std::begin(kFishParts), std::end(kFishParts));
  if (kind == 5) return { "smoke_pot" };                               // tools/make_utility_models.py
  if (kind == 6) return { "raft_pack" };
  return { kind == 2 ? "chompette_upper" : "chomper_upper", "chomper_lower" };
}
static const char* TorpLookName(int kind) { return kind == 6 ? "packed raft look" : kind == 5 ? "smoke pot look" : kind == 4 ? "flying fish look" : kind == 3 ? "commando look" : "chomper look"; }
static const char* TorpLookModel(int kind) { return kind == 6 ? "packed raft" : kind == 5 ? "smoke pot" : kind == 4 ? "flying fish" : kind == 3 ? "commando" : "chomper"; }
static void TorpLookOff(void* t, TorpLook& L) {
  for (void* j : L.parts) if (Alive(j)) { bool off = false; void* a[1] = { &off }; Invoke(m_setActive, j, a); }
  for (void* r : L.stock) if (Alive(r)) { bool off = false; void* a[1] = { &off }; Invoke(g_mapi.forceOff, r, a); }
  if (L.ch && g_fm.ok) g_fm.stop(L.ch);
  L.ch = nullptr; L.shown = false;
}
static void TorpLookDrop(TorpLook& L) {
  for (void* j : L.parts) if (Alive(j) && g_mapi.destroy) { void* a[1] = { j }; Invoke(g_mapi.destroy, nullptr, a); }
  for (void* h : L.ph) Unpin(h);
  L.parts.clear(); L.ph.clear();
}
static bool TorpLookOn(void* t, TorpLook& L, int kind) {
  if (g_modelsOff || !ModelApiReady() || !g_setParentTwoArgs) return false;
  void* tT = Invoke(m_getTransform, t); if (!tT) return false;
  if (!L.parts.empty() && (L.kind != kind || std::any_of(L.parts.begin(), L.parts.end(), [](void* j) { return !Alive(j); })))
    TorpLookDrop(L);                                                   // the other look, or destroyed with a scene
  if (L.parts.empty()) {
    std::vector<std::string> files = TorpLookFiles(kind);
    for (std::string& f : files) {
      f = g_folder + "\\models\\" + f + ".obj";
      if (GetFileAttributesA(f.c_str()) == INVALID_FILE_ATTRIBUTES) {
        static std::set<int> told; if (told.insert(kind).second) Log("%s: %s not found; this torpedo keeps its own look", TorpLookName(kind), f.c_str());
        return false;
      }
    }
    if (L.stock.empty()) for (void* r : RenderersUnder(tT)) if (IsMeshRenderer(r)) { L.stock.push_back(r); L.sh.push_back(Pin(r)); }   // before the parts are under it; pinned, held across battles
    std::vector<void*> got;
    for (const std::string& f : files) { void* j = LoadModelCopy(f, TorpLookModel(kind)); if (!j) break; got.push_back(j); }
    if (got.size() != files.size()) { for (void* j : got) if (g_mapi.destroy) { void* a[1] = { j }; Invoke(g_mapi.destroy, nullptr, a); } return false; }
    V3 ls = GetV3(g_mapi.lossy, tT); V3 sc = { ls.x != 0 ? 1 / ls.x : 1, ls.y != 0 ? 1 / ls.y : 1, ls.z != 0 ? 1 / ls.z : 1 };
    for (void* j : got) {
      void* jT = Invoke(g_mapi.goTransform, j);
      bool keepWorld = false; void* pa[2] = { tT, &keepWorld }; Invoke(m_setParent, jT, pa);
      void* a3[1] = { &sc }; Invoke(m_setLocalScale, jT, a3);
      L.parts.push_back(j); L.ph.push_back(Pin(j));
    }
    L.kind = kind;
    if (!L.th) L.th = Pin(t);
    static std::set<int> ready;
    if (ready.insert(kind).second) Log("%s: ready (%d torpedo meshes hidden under it)", TorpLookName(kind), (int)L.stock.size());
  }
  for (void* r : L.stock) if (Alive(r)) { bool on = true; void* a[1] = { &on }; Invoke(g_mapi.forceOff, r, a); }
  for (void* j : L.parts) { bool on = true; void* a[1] = { &on }; Invoke(m_setActive, j, a); }
  L.shown = true;
  return true;
}
static Q4 QMul(Q4 a, Q4 b) {
  return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
           a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
}
// The water surface at the torpedo, smoothed (r.waterY): FloatPoint depth is how far below it the torpedo is.
static float TorpWater(void* t, TorpRun& r, V3 p, float dt) {
  void* fp = *(void**)((char*)t + g_tp.pFloat);
  float depth = fp ? *(float*)((char*)fp + g_tp.fpDepth) : -1;
  float water = depth > 0.3f && depth < 9.9f ? p.y + depth : 0.0f;
  if (!r.waterSet) { r.waterY = water; r.waterSet = true; } else r.waterY += (water - r.waterY) * (std::min)(1.0f, dt * 6);
  return r.waterY;
}
static Q4 QAxis(int axis, float a) {                                  // a turn of a radians about X (0), Y (1) or Z (2)
  float sn = sinf(a / 2), cs = cosf(a / 2);
  return axis == 0 ? Q4{ sn, 0, 0, cs } : axis == 1 ? Q4{ 0, sn, 0, cs } : Q4{ 0, 0, sn, cs };
}
static V3 QRotV(Q4 q, V3 v) {                                         // v turned by q
  V3 t = { 2 * (q.y * v.z - q.z * v.y), 2 * (q.z * v.x - q.x * v.z), 2 * (q.x * v.y - q.y * v.x) };
  return { v.x + q.w * t.x + (q.y * t.z - q.z * t.y), v.y + q.w * t.y + (q.z * t.x - q.x * t.z), v.z + q.w * t.z + (q.x * t.y - q.y * t.x) };
}
// The Sneaking Eel's commando crawls flat on the water: the arms reach ahead in turn (the elbow lifting on the way
// forward), the opposite knee drawn out to the side, the body swaying and the headband's tails fluttering. 1.2 strokes
// of each arm a game second, so the kill cam slows him too; tools/make_commando_assets.py draws the same pose.
// Part i's turn and its pivot's place in the commando's frame at stroke phase ph (radians).
static void TorpCrawlPart(size_t i, float ph, Q4& q, V3& o) {
  const float D = 0.0174533f, Pi = 3.14159265f;
  Q4 qb = QAxis(1, 5 * D * sinf(ph));
  Q4 qp = { 0, 0, 0, 1 };
  if (i == 1 || i == 2) {                                              // arms: left first, right half a stroke later
    float sd = i == 1 ? -1.0f : 1.0f, a = ph + (i == 1 ? 0 : Pi);
    qp = QMul(QAxis(1, -sd * 28 * D * sinf(a)), QAxis(2, sd * 22 * D * (std::max)(0.0f, -cosf(a))));
  } else if (i == 3 || i == 4) {                                       // legs: the knee opposite the reaching arm
    float sd = i == 3 ? -1.0f : 1.0f, w = 0.5f + 0.5f * sinf(ph + (i == 3 ? Pi : 0));
    qp = QAxis(1, -sd * 30 * D * w);
  } else if (i == 5) qp = QMul(QAxis(1, 14 * D * sinf(ph * 3.7f)), QAxis(0, 6 * D * sinf(ph * 5.3f)));
  q = QMul(qb, qp); o = QRotV(qb, kCrawlPivot[i]);
}
// The flying fish's part i in its own frame: the wings spread (1) or folded back along its sides (0), tipped up when
// spread, plus a flutter; the tail turned tailA radians.
static const float kFishFold = 45.0f;                                // degrees; FOLD in make_flyfish_assets.py
static void TorpFishPart(size_t i, float spread, float flutter, float tailA, Q4& q, V3& o) {
  const float D = 0.0174533f;
  o = kFishPivot[i]; q = { 0, 0, 0, 1 };
  if (i == 1 || i == 2) {
    float sd = i == 1 ? -1.0f : 1.0f;
    q = QMul(QAxis(1, sd * kFishFold * D * (1 - spread)), QAxis(2, sd * (8 * D * spread + flutter)));
  } else if (i == 3) q = QAxis(1, tailA);
}
static void TorpCrawlPose(TorpLook& L, V3 c, Q4 qh, float age) {
  const float ph = age * 1.2f * 2 * 3.14159265f;
  for (size_t i = 0; i < L.parts.size() && i < 6; i++) {
    Q4 qp; V3 op; TorpCrawlPart(i, ph, qp, op);
    Q4 q = QMul(qh, qp); V3 o = QRotV(qh, op); V3 w = { c.x + o.x, c.y + o.y, c.z + o.z };
    void* jT = Invoke(g_mapi.goTransform, L.parts[i]);
    if (jT) { void* a[2] = { &w, &q }; Invoke(g_tp.trSetPR, jT, a); }
  }
}
static void TorpLookPose(void* t, TorpLook& L, TorpRun& r, float dt) {
  void* tT = Invoke(m_getTransform, t); void* pr = tT ? Invoke(g_tp.trPos, tT) : nullptr; if (!pr) return;
  V3 p = *(V3*)((char*)pr + 0x10);
  TorpWater(t, r, p, dt);
  float age = TorpTime() - r.t0;
  float ride = L.kind == 4 ? r.waterY + 0.15f : L.kind == 3 ? r.waterY + 0.05f : r.waterY + 0.3f * kChompR + 0.08f * sinf(age * 8.2f);
  float y = (std::max)(p.y, ride);                                    // out of the tube it flies; in the water it rides on top
  V3 c = { p.x, y + r.leapY, p.z };                                    // of the Dolphin: the ball leaps from the surface
  V3 hd = r.heading;
  if (r.leapPitch != 0) { float cp = cosf(r.leapPitch); hd = { hd.x * cp, sinf(r.leapPitch), hd.z * cp }; }
  Q4 qh = LookRot(hd);
  if (L.kind >= 5) return;                                             // a dropped pot or raft: it rides on the charge, a child of it
  if (L.kind == 3) { TorpCrawlPose(L, c, qh, age); return; }
  if (L.kind == 4) {                                                   // the flying fish: wings out in the first half second,
    float sp = (std::min)(1.0f, age / 0.5f); sp = sp * sp * (3 - 2 * sp);   // the tail beating hard on the water, lightly aloft
    bool aloft = r.leapStart >= 0;
    r.tailAmp += ((aloft ? 0.09f : 0.38f) - r.tailAmp) * (std::min)(1.0f, dt * 8);
    float tailA = r.tailAmp * sinf(age * 6.0f * 6.2832f), flutter = aloft ? 0.05f * sinf(age * 9.0f * 6.2832f) : 0.0f;
    for (size_t i = 0; i < L.parts.size() && i < 4; i++) {
      Q4 qp; V3 op; TorpFishPart(i, sp, flutter, tailA, qp, op);
      Q4 q = QMul(qh, qp); V3 o = QRotV(qh, op); V3 w = { c.x + o.x, c.y + o.y, c.z + o.z };
      void* jT = Invoke(g_mapi.goTransform, L.parts[i]);
      if (jT) { void* a[2] = { &w, &q }; Invoke(g_tp.trSetPR, jT, a); }
    }
    return;
  }
  if (L.parts.size() < 2) return;
  // shut 3 times a game second; with the chomp loop playing, at its bites (its 2/3 s holds two), so the two never drift
  // apart when the pitch cannot follow the game speed (it stops at 0.3; the kill cam runs at 0.25)
  float ph = age * 3.0f;
  unsigned ms = 0;
  if (L.ch && g_fm.position && g_fm.position(L.ch, &ms, 1) == 0) ph = (ms % 667) / 333.33f;
  float a = 0.6981f * (0.5f - 0.5f * cosf(6.2832f * ph));             // up to 40 degrees each jaw
  Q4 qu = QMul(qh, { -sinf(a / 2), 0, 0, cosf(a / 2) }), ql = QMul(qh, { sinf(a / 2), 0, 0, cosf(a / 2) });   // +X pitches +Z down
  void* uT = Invoke(g_mapi.goTransform, L.parts[0]); void* lT = Invoke(g_mapi.goTransform, L.parts[1]);
  if (uT) { void* a1[2] = { &c, &qu }; Invoke(g_tp.trSetPR, uT, a1); }
  if (lT) { void* a2[2] = { &c, &ql }; Invoke(g_tp.trSetPR, lT, a2); }
  if (L.ch) {
    float ts = KcFloat(g_tp.timeScale);
    FmodVec pos = { c.x, c.y, c.z }, vel = {};
    g_fm.set3D(L.ch, &pos, &vel);
    g_fm.paused(L.ch, ts >= 0 && ts < 0.01f);
    if (ts >= 0.01f) g_fm.pitch(L.ch, ts < 0.3f ? 0.3f : ts > 2 ? 2 : ts);
    g_fm.volume(L.ch, FmodVolume());
  }
}
static void* g_torpEng = nullptr;
static bool TorpActive(void* t) {
  void* go = Invoke(m_getGameObject, t); void* ab = go ? Invoke(g_tp.goActive, go) : nullptr;
  return ab && *(bool*)((char*)ab + 0x10);
}

// ---- of the Dolphin: the model leaps out of the water ------------------------------------------------------------------
// The game holds a torpedo in the water at its running depth every physics step (it snaps it back up from below, see
// Torpedo.FixedUpdate in the torpedo notes), so a push upward would not last and a real jump would change its hits and
// co-op copies. The torpedo runs on as stock; only its model (the root's mesh children, moved after the interpolation,
// before the frame is drawn) or a chomper's ball jumps along a thrown arc, nose up then down, with a shell's water splash
// where it breaks the surface and where it goes back in.
struct TorpLeapLook { void* th = nullptr; std::vector<void*> parts, ph; std::vector<V3> lp; std::vector<Q4> lr; bool moved = false, none = false; };
static std::unordered_map<void*, TorpLeapLook> g_torpLeaps;          // Torpedo -> its model parts and their rest pose
static int g_torpLeapLogs = 0;
static void TorpLeapRest(TorpLeapLook& L) {
  for (size_t i = 0; i < L.parts.size(); i++) if (Alive(L.parts[i])) {
    void* a1[1] = { &L.lp[i] }; Invoke(m_setLocalPos, L.parts[i], a1);
    void* a2[1] = { &L.lr[i] }; Invoke(m_setLocalRot, L.parts[i], a2);
  }
  L.moved = false;
}
// The root's children that hold its meshes (the stock "Torpedo" child), found once per pooled torpedo; a look's parts
// (a chomper's jaws, the commando; also under the root, switched off) are left out.
static bool TorpLeapParts(void* t, TorpLeapLook& L) {
  if (!L.parts.empty()) return true;
  if (L.none) return false;
  void* tT = Invoke(m_getTransform, t); if (!tT) return false;
  std::vector<void*> skip;
  auto lf = g_torpLooks.find(t);
  if (lf != g_torpLooks.end()) for (void* j : lf->second.parts) if (Alive(j)) skip.push_back(Invoke(g_mapi.goTransform, j));
  std::string names;
  for (void* rd : RenderersUnder(tT)) {
    if (!IsMeshRenderer(rd)) continue;
    void* x = Invoke(m_getTransform, rd);
    for (int k = 0; x && k < 16; k++) { void* up = Invoke(m_getParent, x); if (!up || up == tT) break; x = up; }
    if (!x || x == tT || Invoke(m_getParent, x) != tT) continue;
    if (std::find(skip.begin(), skip.end(), x) != skip.end() || std::find(L.parts.begin(), L.parts.end(), x) != L.parts.end()) continue;
    void* rr = Invoke(m_getLocalRot, x);
    L.parts.push_back(x); L.ph.push_back(Pin(x)); L.lp.push_back(GetV3(m_getLocalPos, x)); L.lr.push_back(rr ? *(Q4*)((char*)rr + 0x10) : Q4{ 0, 0, 0, 1 });
    names += (names.empty() ? "'" : ", '") + NameOf(x) + "'";
  }
  if (!L.th) L.th = Pin(t);
  static bool told = false, toldNone = false;
  if (L.parts.empty()) { L.none = true; if (!toldNone) { toldNone = true; Log("dolphin torpedoes: no model part under '%s' to move; this torpedo does not leap", CrewClean(NameOf(t)).c_str()); } return false; }
  if (!told) { told = true; Log("dolphin torpedoes: ready (moving %s of '%s')", names.c_str(), CrewClean(NameOf(t)).c_str()); }
  return true;
}
static void TorpSplash(V3 at) {
  static void* prefab = nullptr; static DWORD tried = 0; static bool told = false;
  if (!g_tp.vfxSpawn || !g_tp.vfxInst || g_tp.ySplash < 0) return;
  if (!prefab && (!tried || GetTickCount() - tried > 30000)) {         // a stock shell's water splash (pinned: an asset)
    tried = GetTickCount();
    for (const char* g : { "40 mm Bofors", "3-inch Gun Player", "20 mm Oerlikon Twin", "5-inch 25 Gun Player" }) {
      void* pt = StockAmmo(g); void* pf = pt ? *(void**)((char*)pt + g_tp.ySplash) : nullptr;
      if (pf) { prefab = pf; Pin(pf); Log("dolphin torpedoes: the leaps splash like a %s shell ('%s')", g, NameOf(pf).c_str()); break; }
    }
    if (!prefab && !told) { told = true; Log("dolphin torpedoes: no stock shell splash found yet; the leaps make no splash"); }
  }
  if (!prefab) return;
  void* inst = nullptr; il2cpp_field_static_get_value(g_tp.vfxInst, &inst); if (!inst) return;
  // as the game's local-only VfxTrigger.trigger: force=true passes the co-op client's gate (bypassReasons), local=true
  // keeps it off the network; the spawn cap and distance cull still apply. 0.1 m up, as ProjectileManager.splashProjectile
  bool force = true, local = true;
  at.y += 0.1f;
  void* a[4] = { prefab, &at, &force, &local };
  Invoke(g_tp.vfxSpawn, inst, a);
}
// The leap's timing and pose (r.leapY, r.leapPitch), in game time so it slows with the kill cam. base: how far the model
// rests below the water (0 for a chomper's ball, which rides on top). The arc falls at 30 m/s^2, not 9.81: at a
// torpedo's 60-85 m/s real gravity makes a 2 s, 170 m skim; this one takes about 1.2 s, and the pitch is drawn three
// times the path's. No leap starts while a ship is within the ground
// it would cover (ahead, or alongside), so a torpedo is not up in the air when it goes off.
static void TorpLeapUpdate(void* t, TorpRun& r, V3 p, float base, float now) {
  if (r.leapStart < 0) {
    r.leapY = 0; r.leapPitch = 0;
    if (now < r.leapAt || *(float*)((char*)t + g_tp.pInWater) < 2.0f || r.speed < 5) return;
    float h = 2.2f + 0.6f * (float)((r.code + r.leaps) % 3);           // 2.2, 2.8 or 3.4 m above the water
    float amp = (std::max)(0.0f, (std::min)(base, 8.0f)) + h, dur = 2 * sqrtf(2 * amp / 30.0f);
    float reach = r.speed * dur + 60.0f;                               // the leap's run, plus a hull's half length
    for (const EnemyAt& e : LiveEnemies()) {
      if (!e.ship) continue;
      float dx = e.pos.x - p.x, dz = e.pos.z - p.z;
      if (dx * r.heading.x + dz * r.heading.z > -40.0f && dx * dx + dz * dz < reach * reach) { r.leapAt = now + 0.5f; return; }
    }
    r.leapAmp = amp; r.leapDur = dur; r.leapStart = now;
  }
  float s = (now - r.leapStart) / r.leapDur;
  if (s >= 1 || s < 0) {
    r.leapStart = -1; r.leapY = 0; r.leapPitch = 0; r.leaps++; g_torpStats[r.item].leaps++;
    r.leapAt = now + r.leap * (0.8f + 0.1f * (float)((r.code + r.leaps) % 5));
    if (g_torpLeapLogs++ < 8) Log("torpedoes: '%s' leapt %.1f m out of the water (%.1f s from going up to back at depth)", r.item.c_str(), r.leapAmp - (std::max)(0.0f, (std::min)(base, 8.0f)), r.leapDur);
    return;
  }
  r.leapY = r.leapAmp * 4 * s * (1 - s);
  float vy = r.leapAmp * 4 * (1 - 2 * s) / r.leapDur;
  float ease = (std::min)(1.0f, (std::min)(s, 1 - s) / 0.15f); ease = ease * ease * (3 - 2 * ease);   // no snap at either end
  r.leapPitch = ease * (std::max)(-0.6f, (std::min)(0.6f, 3.0f * atan2f(vy, (std::max)(r.speed, 1.0f))));
}
// Flying Fish: once the torpedo is in the water its model rises over 0.4 s from its running depth to the surface (base:
// how far below the water it rests) and rides there in skips, like a thrown stone: five of falling height (1.6 m down to
// 0.3 m, falling at 30 m/s^2 as the dolphin's leap), a splash at each touch, then a short glide and again. Within the
// ground a skip covers of a ship it glides flat, so it is never up in the air when it goes off. Game time throughout.
static void TorpSkimUpdate(void* t, TorpRun& r, V3 p, float base, float now) {
  static const float kSkipH[] = { 1.6f, 1.1f, 0.75f, 0.5f, 0.3f };
  float inWater = *(float*)((char*)t + g_tp.pInWater);
  if (inWater < 0.2f) { r.leapStart = -1; r.leapY = 0; r.leapPitch = 0; r.skimGlide = 0; return; }   // still out of the tube
  float rise = (std::min)(1.0f, (inWater - 0.2f) / 0.4f); rise = rise * rise * (3 - 2 * rise);
  float lift = rise * ((std::max)(0.0f, (std::min)(base, 8.0f)) + 0.05f);   // just breaking the surface
  float hop = 0, pitch = 0;
  if (r.leapStart < 0 && rise >= 1 && now >= r.skimGlide && r.speed >= 5) {
    float h = kSkipH[r.skips % 5], dur = 2 * sqrtf(2 * h / 30.0f);
    float reach = r.speed * dur + 60.0f;
    bool shipAhead = false;
    for (const EnemyAt& e : LiveEnemies()) {
      if (!e.ship) continue;
      float dx = e.pos.x - p.x, dz = e.pos.z - p.z;
      if (dx * r.heading.x + dz * r.heading.z > -40.0f && dx * dx + dz * dz < reach * reach) { shipAhead = true; break; }
    }
    if (shipAhead) r.skimGlide = now + 0.25f;
    else { r.leapAmp = h; r.leapDur = dur; r.leapStart = now; }
  }
  if (r.leapStart >= 0) {
    float s = (now - r.leapStart) / r.leapDur;
    if (s >= 1 || s < 0) {                                             // back down on the water: a splash, and the next
      r.leapStart = -1; r.skips++; g_torpStats[r.item].skips++;
      r.skimGlide = now + (r.skips % 5 == 0 ? 0.35f : 0.0f);
      TorpSplash({ p.x, r.waterY, p.z });
      if (g_torpLeapLogs++ < 8) Log("torpedoes: '%s' skipped %.1f m off the water (%.2f s)", r.item.c_str(), r.leapAmp, r.leapDur);
    } else {
      hop = r.leapAmp * 4 * s * (1 - s);
      float vy = r.leapAmp * 4 * (1 - 2 * s) / r.leapDur;
      float ease = (std::min)(1.0f, (std::min)(s, 1 - s) / 0.2f); ease = ease * ease * (3 - 2 * ease);
      pitch = ease * (std::max)(-0.25f, (std::min)(0.25f, 1.5f * atan2f(vy, (std::max)(r.speed, 1.0f))));
    }
  }
  r.leapY = lift + hop; r.leapPitch = pitch;
}
static void TorpLeapPose(TorpLeapLook& L, TorpRun& r) {
  TorpLeapRest(L);                                                     // back at rest, then lifted and pitched from there
  if ((r.leapStart < 0 && r.leapY == 0) || !g_tp.trRot) return;
  L.moved = true;                                                      // before the first write: a fault part way still gets rested
  V3 ax = { r.heading.z, 0, -r.heading.x };                            // LookRot's right; +X pitches +Z down, so nose up is negative
  float a = -r.leapPitch;
  Q4 qp = { ax.x * sinf(a / 2), 0, ax.z * sinf(a / 2), cosf(a / 2) };
  for (void* x : L.parts) {
    if (!Alive(x)) continue;
    V3 wp = GetV3(g_tp.trPos, x); void* rr = Invoke(g_tp.trRot, x); if (!rr) continue;
    Q4 q = QMul(qp, *(Q4*)((char*)rr + 0x10));
    wp.y += r.leapY;
    void* a1[2] = { &wp, &q }; Invoke(g_tp.trSetPR, x, a1);
  }
}
// Every frame, before the chomper look: a dolphin's leap, and a model left lifted put back.
static void TorpLeapTick(void* t, TorpRun& r, float dt) {
  auto lf = g_torpLeaps.find(t);
  bool want = (r.leap > 0 || r.skim > 0) && !r.ended && r.handle && TorpActive(t) && *(int*)((char*)t + g_tp.pCode) == r.code;
  if (!want) {
    if (lf != g_torpLeaps.end() && lf->second.moved) TorpLeapRest(lf->second);
    r.leapStart = -1; r.leapY = 0; r.leapPitch = 0; r.leapOut = false;
    return;
  }
  void* tT = Invoke(m_getTransform, t); void* pr = tT ? Invoke(g_tp.trPos, tT) : nullptr; if (!pr) return;
  V3 p = *(V3*)((char*)pr + 0x10);
  bool chomp = r.look > 0;                                             // a look rides on the water; TorpLookPose lifts it
  if (!chomp && !g_tp.trRot) { r.leap = 0; r.skim = 0; r.leapStart = -1; r.leapY = 0; r.leapPitch = 0; return; }
  float base = chomp ? 0.0f : TorpWater(t, r, p, dt) - p.y;
  if (r.skim > 0) TorpSkimUpdate(t, r, p, base, TorpTime());
  else {
    TorpLeapUpdate(t, r, p, base, TorpTime());
    bool out = r.leapStart >= 0 && r.leapY > base + 0.3f;
    if (out != r.leapOut) { r.leapOut = out; TorpSplash({ p.x, r.waterY, p.z }); }
  }
  if (chomp) return;
  TorpLeapLook& L = g_torpLeaps[t];
  if (!TorpLeapParts(t, L)) { r.leap = 0; r.skim = 0; r.leapStart = -1; r.leapY = 0; r.leapPitch = 0; return; }
  if (r.leapStart >= 0 || r.leapY != 0 || L.moved) TorpLeapPose(L, r);
}
static void TorpBattleSummary() {
  for (const auto& s : g_torpStats) {
    const Item* it = ItemByName(s.first);
    bool homes = s.second.locks > 0 || (it && ItemFx(*it, "torpSeek") > 0), leaps = s.second.leaps > 0 || (it && ItemFx(*it, "leap") > 0);
    bool skims = s.second.skips > 0 || (it && ItemFx(*it, "skim") > 0);
    std::string extra = std::string(homes ? ", homed on " + std::to_string(s.second.locks) + " ship(s)" : "") + (leaps ? ", leapt " + std::to_string(s.second.leaps) + " time(s)" : "") +
                        (skims ? ", skipped " + std::to_string(s.second.skips) + " time(s)" : "") +
                        (s.second.dives ? ", dived on " + std::to_string(s.second.dives) + " submerged target(s)" : "");
    Log("torpedoes, last battle: '%s' launched %d, went off %d%s, top speed %.1f m/s", s.first.c_str(), s.second.launched, s.second.exploded, extra.c_str(), s.second.topSpeed);
  }
}
static void TorpLooksAllOff() {
  for (auto& kv : g_torpLooks) if (kv.second.shown) TorpLookOff(kv.first, kv.second);
  for (auto& kv : g_torpLeaps) TorpLeapRest(kv.second);              // all: only the saved rest pose is written
}
static void SafeTorpLooksOff() { __try { TorpLooksAllOff(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
// Every frame, before the looks: found torpedoes that left the water are marked ended (a Chompette then stops counting
// their ship as taken), and at a battle change the battle's numbers are logged and reset. The game clears
// WorldEngagementManager.instance a few frames before the scene goes, so a torpedo still running keeps its run (it was
// counted and paid for once); runs of torpedoes gone from the water, and looks of destroyed torpedoes, are dropped.
static void TorpBattleTick() {
  for (auto& kv : g_torpRuns) {
    TorpRun& r = kv.second;
    if (r.ended || !r.handle) continue;
    if (!Alive(kv.first) || *(int*)((char*)kv.first + g_tp.pCode) != r.code || !TorpActive(kv.first)) { r.ended = true; TorpRunReport(r); }
  }
  void* eng = EngagementInstance();
  if (eng == g_torpEng) return;
  if (g_torpEng) TorpBattleSummary();
  g_torpStats.clear(); g_torpPaid.clear(); g_torpLogs = 0; g_torpLeapLogs = 0; g_torpCoopMiss = 0;
  for (auto it = g_torpRuns.begin(); it != g_torpRuns.end(); ) {
    if (it->second.handle && !it->second.ended) { ++it; continue; }
    Unpin(it->second.handle); it = g_torpRuns.erase(it);
  }
  for (auto it = g_torpOrders.begin(); it != g_torpOrders.end(); ) {   // one copy per launcher Awake, pinned for good
    auto ow = g_torpOrderOwner.find(it->first);
    void* launcher = ow != g_torpOrderOwner.end() ? ow->second.first : nullptr;
    if (ow == g_torpOrderOwner.end() || (launcher && Alive(launcher) && *(void**)((char*)launcher + g_offOrder) == it->first)) { ++it; continue; }
    Unpin(ow->second.second); g_torpOrderOwner.erase(ow); Unpin(it->second); it = g_torpOrders.erase(it);
  }
  for (auto it = g_torpLooks.begin(); it != g_torpLooks.end(); ) {
    if (Alive(it->first)) { if (it->second.shown && !g_torpRuns.count(it->first)) TorpLookOff(it->first, it->second); ++it; continue; }
    for (void* h : it->second.ph) Unpin(h);
    for (void* h : it->second.sh) Unpin(h);
    Unpin(it->second.th); it = g_torpLooks.erase(it);                 // destroyed with its scene, its look too
  }
  for (auto it = g_torpLeaps.begin(); it != g_torpLeaps.end(); ) {
    if (Alive(it->first)) { if (it->second.moved && !g_torpRuns.count(it->first)) TorpLeapRest(it->second); ++it; continue; }
    for (void* h : it->second.ph) Unpin(h);
    Unpin(it->second.th); it = g_torpLeaps.erase(it);
  }
  for (auto it = g_torpDives.begin(); it != g_torpDives.end(); ) {   // diving copies of torpedoes not still running
    auto run = g_torpRuns.find(it->first);
    if (Alive(it->first) && run != g_torpRuns.end() && run->second.handle && !run->second.ended) { ++it; continue; }
    TorpDiveDrop(it->first, it->second); it = g_torpDives.erase(it);
  }
  g_torpEng = eng;
}
// The torpedoes waiting on the boat: each is a model of its tier, TorpedoTier.dummies[i].GetChild(0), which the game
// hides on firing and shows on a refill; a found torpedo's tier shares the dummies of the stock tier it was copied from.
// So the look follows the launcher's selected tier: a holder under each model (cancelling its stretch) carries the
// look's parts at rest, and the model's meshes are switched off (forceRenderingOff), so the look hides, shows and moves
// with it; a tier without a look gets its meshes back. Checked when a launcher's choice changes (selectIndex) and, in
// battle, every 20 frames.
struct TubeLook { void* holder = nullptr; void* hh = nullptr; void* mh = nullptr; std::vector<void*> stock, sh; int kind = 0; };
static std::unordered_map<void*, TubeLook> g_tubeLooks;              // a tube's torpedo model (pinned) -> the look on it
static std::unordered_map<void*, std::pair<void*, int>> g_tubeSeen;   // launcher -> the tier and look all its models have
static std::unordered_map<void*, int> g_tubeFails;                    // a model whose look could not be made: tries so far
static int g_tubeFaults = 0, g_tubeLogs = 0;
// Each step on its own, so one bad object never leaves the rest of the meshes dark.
static void SafeForceOn(void* r) { __try { if (Alive(r)) { bool off = false; void* a[1] = { &off }; Invoke(g_mapi.forceOff, r, a); } } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void SafeDestroy(void* o) { __try { if (Alive(o) && g_mapi.destroy) { void* a[1] = { o }; Invoke(g_mapi.destroy, nullptr, a); } } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void TubeLookOff(TubeLook& T) {
  SafeDestroy(T.holder);
  for (void* r : T.stock) SafeForceOn(r);                              // pinned, so the address is still that renderer
  Unpin(T.hh); Unpin(T.mh); for (void* h : T.sh) Unpin(h);
  T = TubeLook();
}
static void TubeLooksAllOff() {
  for (auto& kv : g_tubeLooks) TubeLookOff(kv.second);
  g_tubeLooks.clear(); g_tubeSeen.clear(); g_tubeFails.clear();
}
static void SafeTubeLooksOff() { __try { TubeLooksAllOff(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static void* NewGameObject(const char* name) {
  static void* cls = FindClass("UnityEngine", "GameObject");
  static void* ctor = cls ? FindMethod(cls, ".ctor", 1, "System.String") : nullptr;
  if (!ctor) return nullptr;
  void* go = il2cpp_object_new(cls); if (!go) return nullptr;
  void* na[1] = { il2cpp_string_new(name) }; void* exc = nullptr;
  il2cpp_runtime_invoke(ctor, go, na, &exc);
  return exc ? nullptr : go;
}
static Q4 QInv(Q4 q) { return { -q.x, -q.y, -q.z, q.w }; }
// The look `kind` on one tube model (an active one: a fired tube's model is skipped until its refill); false (its own
// meshes kept) when a piece is missing.
static bool TubeLookOn(void* model, TubeLook& T, int kind) {
  if (Alive(T.holder) && T.kind == kind) return true;
  TubeLookOff(T);
  if (g_modelsOff || !ModelApiReady() || !g_setParentTwoArgs || !g_tp.trSetPR || !g_tp.trRot || !m_setLocalPos || !m_setLocalRot || !m_setLocalScale) return false;
  std::vector<std::string> files = TorpLookFiles(kind);
  for (std::string& f : files) { f = g_folder + "\\models\\" + f + ".obj"; if (GetFileAttributesA(f.c_str()) == INVALID_FILE_ATTRIBUTES) return false; }
  // its meshes, and the biggest one's box (world): the middle of the torpedo
  std::vector<void*> stock; V3 mid = {}, ext = {}; float best = -1; void* big = nullptr;
  for (void* r : RenderersUnder(model)) {
    if (!IsMeshRenderer(r)) continue;
    stock.push_back(r);
    void* b = Invoke(g_mapi.bounds, r); if (!b) continue;
    V3 e = *(V3*)((char*)b + 0x1C);
    if (e.x * e.x + e.y * e.y + e.z * e.z > best) { best = e.x * e.x + e.y * e.y + e.z * e.z; mid = *(V3*)((char*)b + 0x10); ext = e; big = r; }
  }
  if (stock.empty() || best <= 0) return false;
  void* rb = Invoke(g_tp.trRot, model); if (!rb) return false;
  Q4 qm = *(Q4*)((char*)rb + 0x10);
  // its long axis and length in the model's own frame, from the mesh's local box (Renderer.localBounds), so the boat's
  // heading cannot fool it; without localBounds, from the world box (right unless the boat points about 45 degrees off)
  static void* locB = nullptr; static bool locTried = false;
  if (!locTried) { locTried = true; void* rk = FindClass("UnityEngine", "Renderer"); locB = rk ? FindMethod(rk, "get_localBounds", 0) : nullptr; }
  V3 span = {};                                                        // half the extent along the model's X, Y, Z
  void* lb = locB ? Invoke(locB, big) : nullptr;
  void* rT = lb ? Invoke(m_getTransform, big) : nullptr;
  void* rr = rT ? Invoke(g_tp.trRot, rT) : nullptr;
  if (lb && rr) {
    V3 le = *(V3*)((char*)lb + 0x1C), rs = GetV3(g_mapi.lossy, rT);
    le = { fabsf(le.x * rs.x), fabsf(le.y * rs.y), fabsf(le.z * rs.z) };
    Q4 qr = QMul(QInv(qm), *(Q4*)((char*)rr + 0x10));
    V3 ax = QRotV(qr, { 1, 0, 0 }), ay = QRotV(qr, { 0, 1, 0 }), az = QRotV(qr, { 0, 0, 1 });
    span = { fabsf(ax.x) * le.x + fabsf(ay.x) * le.y + fabsf(az.x) * le.z, fabsf(ax.y) * le.x + fabsf(ay.y) * le.y + fabsf(az.y) * le.z,
             fabsf(ax.z) * le.x + fabsf(ay.z) * le.y + fabsf(az.z) * le.z };
  } else {
    auto sp = [&](V3 d) { return fabsf(d.x) * ext.x + fabsf(d.y) * ext.y + fabsf(d.z) * ext.z; };
    span = { sp(QRotV(qm, { 1, 0, 0 })), 0, sp(QRotV(qm, { 0, 0, 1 })) };
  }
  // the model's +Z (the torpedo mesh's, nose ahead) unless another axis is clearly longer
  int axis = 2; float len = 2 * span.z;
  if (span.x > 1.25f * span.z && span.x >= span.y) { axis = 0; len = 2 * span.x; }
  else if (span.y > 1.25f * span.z && span.y > span.x) { axis = 1; len = 2 * span.y; }
  Q4 qa = axis == 0 ? QAxis(1, 1.5707963f) : axis == 1 ? QAxis(0, -1.5707963f) : Q4{ 0, 0, 0, 1 };   // the look's +Z onto that axis
  // the commando (about 4.8 m from his feet to his hands) and the fish (4.3 m to its tail tip) at most their own size, fitted to the
  // torpedo's length; a chomper's ball (2.6 m across) at 40% of it, never over 1.8 m
  // the smoke pot and packed raft are about 0.6 m long, the Mk 6 charge they replace
  float sc = kind >= 5 ? (std::min)(2.0f, (std::max)(0.5f, len / 0.6f)) : kind == 3 ? (std::min)(1.0f, (std::max)(0.3f, len / 4.8f)) : kind == 4 ? (std::min)(1.0f, (std::max)(0.3f, len / 4.3f))
                       : (std::min)(0.7f, (std::max)(0.2f, (std::min)(1.8f, 0.4f * len) / (2 * kChompR)));
  void* holder = NewGameObject("WM tube look"); if (!holder) return false;
  void* hT = Invoke(g_mapi.goTransform, holder);
  { bool keepWorld = false; void* pa[2] = { model, &keepWorld }; Invoke(m_setParent, hT, pa); }
  V3 ls = GetV3(g_mapi.lossy, model);
  V3 hs = { ls.x != 0 ? sc / ls.x : sc, ls.y != 0 ? sc / ls.y : sc, ls.z != 0 ? sc / ls.z : sc };
  { void* a[1] = { &hs }; Invoke(m_setLocalScale, hT, a); }
  { void* a[2] = { &mid, &qm }; Invoke(g_tp.trSetPR, hT, a); }
  std::vector<void*> got;
  for (const std::string& f : files) { void* j = LoadModelCopy(f, TorpLookModel(kind)); if (!j) break; got.push_back(j); }
  if (got.size() != files.size()) { for (void* j : got) SafeDestroy(j); SafeDestroy(holder); return false; }
  const float jaw = 0.21f;                                             // a chomper's mouth: 12 degrees each jaw
  for (size_t i = 0; i < got.size(); i++) {
    void* jT = Invoke(g_mapi.goTransform, got[i]);
    bool keepWorld = false; void* pa[2] = { hT, &keepWorld }; Invoke(m_setParent, jT, pa);
    V3 one = { 1, 1, 1 }; void* a1[1] = { &one }; Invoke(m_setLocalScale, jT, a1);
    Q4 q; V3 o;
    // the commando centred on the torpedo: at rest his reach runs -2.76..2.0 m along him and his height -0.10..0.98 m
    if (kind == 3) { TorpCrawlPart(i, 0, q, o); o.y -= 0.44f; o.z += 0.38f; }
    else if (kind == 4) { TorpFishPart(i, 0, 0, 0, q, o); o.z += 0.45f; }   // wings folded; nose to tail tip runs -2.6..1.7 m
    else if (kind <= 2) { q = QAxis(0, i == 0 ? -jaw : jaw); o = { 0, 0, 0 }; }
    else { q = { 0, 0, 0, 1 }; o = { 0, 0, 0 }; }                      // one rigid part
    q = QMul(qa, q); o = QRotV(qa, o);
    void* a2[1] = { &o }; Invoke(m_setLocalPos, jT, a2);
    void* a3[1] = { &q }; Invoke(m_setLocalRot, jT, a3);
    bool on = true; void* a4[1] = { &on }; Invoke(m_setActive, got[i], a4);
  }
  T.holder = holder; T.hh = Pin(holder); T.mh = Pin(model); T.stock = stock; T.kind = kind;
  for (void* r : stock) T.sh.push_back(Pin(r));
  for (void* r : stock) { bool on = true; void* a[1] = { &on }; Invoke(g_mapi.forceOff, r, a); }
  DLog("torpedo tubes: %s on a %.1f m torpedo model (long axis %c%s, scale %.2f, %d mesh(es) hidden)", TorpLookName(kind), len, "xyz"[axis],
       lb ? "" : " from the world box", sc, (int)stock.size());
  return true;
}
static bool ModelShown(void* m) {
  void* go = Invoke(m_getGameObject, m); void* ab = go ? Invoke(g_tp.goActive, go) : nullptr;
  return ab && *(bool*)((char*)ab + 0x10);
}
// A launcher's tube models: the look of its selected tier on that tier's models, every other tier's models their own.
// The choice is remembered only once every model that wants the look has it; until then (a tube empty, waiting for its
// refill) the battle tick tries again. A look that fails to load is tried 3 times per model.
static void TubeSync(void* l) {
  if (!l || !Alive(l) || g_tp.tDummies < 0 || g_tp.lTorpedos < 0) return;
  void* list = *(void**)((char*)l + g_tp.lTorpedos);
  int n = ListCount(list), idx = *(int*)((char*)l + g_tp.lSelIndex);
  void* sel = idx >= 0 && idx < n ? ListAt(list, idx) : nullptr;
  int kind = 0; std::string item;
  if (sel) {
    const Item* it = ItemOfTorpType(TierType(sel));                    // another player's too (coop_torpedoes)
    if (it && !it->retired) { int u = UtilKind(*it, l); kind = u ? UtilLook(u) : TorpLookKind(*it); item = it->name; }
  }
  auto seen = g_tubeSeen.find(l);
  if (seen != g_tubeSeen.end() && seen->second.first == sel && seen->second.second == kind) {
    bool fine = true;                                                  // the same choice: still on, unless a scene took it
    if (kind > 0) for (auto& kv : g_tubeLooks) if (kv.second.kind > 0 && !Alive(kv.second.holder)) { fine = false; break; }
    if (fine) return;
  }
  std::vector<std::pair<void*, int>> want;                            // each model once: the selected tier's look wins
  for (int i = 0; i < n && i < 64; i++) {
    void* tier = ListAt(list, i); if (!tier) continue;
    void* dl = *(void**)((char*)tier + g_tp.tDummies);
    int dn = ListCount(dl);
    for (int k = 0; k < dn && k < 32; k++) {
      void* d = ListAt(dl, k); if (!Alive(d) || Unbox32(Invoke(g_mapi.childCount, d)) < 1) continue;
      int zero = 0; void* a[1] = { &zero }; void* m = Invoke(g_mapi.getChild, d, a); if (!m) continue;
      int wk = tier == sel ? kind : 0;
      auto w = std::find_if(want.begin(), want.end(), [&](const std::pair<void*, int>& p) { return p.first == m; });
      if (w == want.end()) want.push_back({ m, wk }); else if (wk > 0) w->second = wk;
    }
  }
  int shown = 0, wanting = 0; bool complete = true;
  for (auto& w : want) {
    auto f = g_tubeLooks.find(w.first);
    if (w.second <= 0) { if (f != g_tubeLooks.end()) { TubeLookOff(f->second); g_tubeLooks.erase(f); } continue; }
    wanting++;
    if (f != g_tubeLooks.end() && f->second.kind == w.second && Alive(f->second.holder)) { shown++; continue; }
    if (!ModelShown(w.first)) { complete = false; continue; }          // an empty tube: on its refill
    int& tries = g_tubeFails[w.first];
    if (tries >= 3) continue;
    TubeLook& T = g_tubeLooks[w.first];
    if (TubeLookOn(w.first, T, w.second)) { shown++; tries = 0; }
    else { tries++; complete = false; if (!T.holder) g_tubeLooks.erase(w.first); }
  }
  bool wasSeen = seen != g_tubeSeen.end();                            // read before the erase below invalidates it
  if (complete) g_tubeSeen[l] = { sel, kind }; else g_tubeSeen.erase(l);
  // only when the count changes: while a boat loads, the retry every 20 frames would log the same count again
  static std::map<void*, int> logged; int code = shown * 4096 + wanting * 2 + (complete ? 1 : 0);
  auto lg = logged.find(l); bool changed = lg == logged.end() || lg->second != code; logged[l] = code;
  if (kind > 0 && (!wasSeen || complete) && changed && g_tubeLogs++ < 20)
    Log("torpedo tubes: '%s' shows its %s on %d of %d torpedo model(s) in its slot%s", item.c_str(), TorpLookName(kind), shown, wanting, complete ? "" : " (the rest when they are loaded)");
}
// In battle, every 20 frames: the player's boat's launchers (a refilled tube, a choice made without selectIndex), and
// looks whose models went with their scene are forgotten.
static void TubeTick() {
  static int frame = 0; if (++frame % 20) return;
  for (auto it = g_tubeLooks.begin(); it != g_tubeLooks.end(); ) {
    if (Alive(it->first)) { ++it; continue; }                          // pinned: a dead one is still its own wrapper
    TubeLookOff(it->second); it = g_tubeLooks.erase(it);
  }
  for (auto it = g_tubeSeen.begin(); it != g_tubeSeen.end(); ) { if (Alive(it->first)) ++it; else it = g_tubeSeen.erase(it); }
  // only a dead model's tries are forgotten (they go with its scene), so the cap of 3 holds for the rest of the battle
  for (auto it = g_tubeFails.begin(); it != g_tubeFails.end(); ) { if (Alive(it->first)) ++it; else it = g_tubeFails.erase(it); }
  for (void* l : LaunchersOf(PlayerCraft())) TubeSync(l);
  if (g_coopTorps && !g_remoteLaunchers.empty()) {                     // other players' boats, with their found torpedoes
    std::vector<void*> ls; for (auto& e : g_remoteLaunchers) ls.push_back(e.first);
    for (void* l : ls) if (Alive(l)) SafeRemoteTubeSync(l);
    if (g_coopTorpFaults < 0) ClearRemoteTorps();
  }
}
// After a choice: the launcher and its mirror, which selectIndex sets too.
static void SafeTubeSync(void* l) {
  if (!g_ready || !g_tp.ok || g_tubeFaults < 0 || !l) return;
  __try {
    TubeSync(l);
    void* sym = g_tp.lSymmetric >= 0 ? *(void**)((char*)l + g_tp.lSymmetric) : nullptr;
    if (sym && sym != l) TubeSync(sym);
  }
  __except (FaultFilter("torpedo looks in the tubes", GetExceptionInformation())) { g_tubeFaults = -1; SafeTubeLooksOff(); }
}
// Another player's launcher: a fault turns off only the co-op torpedoes (g_coopTorpFaults), never the looks of our own
// tubes. The caller clears the remote maps (TubeTick, SafeEnsureRemoteTorps).
static void SafeRemoteTubeSync(void* l) {
  if (!g_ready || !g_tp.ok || g_tubeFaults < 0 || g_coopTorpFaults < 0 || !l) return;
  __try {
    TubeSync(l);
    void* sym = g_tp.lSymmetric >= 0 ? *(void**)((char*)l + g_tp.lSymmetric) : nullptr;
    if (sym && sym != l) TubeSync(sym);
  }
  __except (FaultFilter("co-op torpedo looks in the tubes", GetExceptionInformation())) { g_coopTorpFaults = -1; }
}
static int g_torpBattleFaults = 0;
// Every frame (LateUpdate, after the physics and the transforms' interpolation): looks on chompers and the commando, off
// everything else.
static void TorpLookTick() {
  if (g_torpRuns.empty()) return;
  static LARGE_INTEGER last = {}; LARGE_INTEGER now; QueryPerformanceCounter(&now);
  float dt = last.QuadPart ? (float)(PerfMs(last) / 1000.0) : 0; last = now; if (dt > 0.1f) dt = 0.1f;
  bool listener = false;
  for (auto& kv : g_torpRuns) {
    void* t = kv.first; TorpRun& r = kv.second;
    if (r.leap > 0 || r.skim > 0 || g_torpLeaps.count(t)) TorpLeapTick(t, r, dt);
    auto lf = g_torpLooks.find(t);
    if (r.look <= 0 && lf == g_torpLooks.end()) continue;
    if (r.chomp > 0 && r.exploded && !r.gulped) {                      // the hit: a gulp where it was
      r.gulped = true;
      if (g_chompVolume > 0 && FmodReady()) { if (!listener) { FmodListener(); listener = true; } FmodPlay(g_fm.gulp, { r.last.x, r.waterY, r.last.z }, FmodVolume()); }
    }
    bool active = r.handle && TorpActive(t);
    bool want = r.look > 0 && !r.ended && active && *(int*)((char*)t + g_tp.pCode) == r.code;
    if (!want && r.look > 0 && !active) { r.ended = true; TorpRunReport(r); }   // back in the pool without going off (ran out)
    if (!want) { if (lf != g_torpLooks.end() && lf->second.shown) TorpLookOff(t, lf->second); continue; }
    TorpLook& L = g_torpLooks[t];
    if (L.shown && L.kind != r.look) TorpLookOff(t, L);               // a pooled torpedo fired again as another look: its sound stops too
    if (!L.shown && !TorpLookOn(t, L, r.look)) { r.look = 0; r.chomp = 0; continue; }   // no look: no chomp sound or gulp either
    if (r.chomp > 0 && !L.ch && g_chompVolume > 0 && FmodReady() && g_fm.chomp) { if (!listener) { FmodListener(); listener = true; } L.ch = FmodPlay(g_fm.chomp, r.last, FmodVolume()); r.t0 = TorpTime(); }
    if (L.ch && !listener) { FmodListener(); listener = true; }
    TorpLookPose(t, L, r, dt);
  }
}

static bool TorpInstall() {
  if (!TorpSetup()) { g_torpOff = true; return false; }
  bool a = PatchPointer(g_tp.launcherCls, "Awake", 0, (void*)&Hooked_TorpAwake, (void**)&g_torpAwakeOrig) != nullptr;
  bool b = PatchVtableEntry(g_tp.launcherCls, g_tp.selectMI, (void*)&Hooked_TorpSelectIndex, (void**)&g_torpSelectOrig);
  bool c = PatchPointer(g_tp.torpCls, "FixedUpdate", 0, (void*)&Hooked_TorpFixed, (void**)&g_torpFixedOrig) != nullptr;
  if (!a || !c) { g_torpOff = true; Log("found torpedoes: hooks failed (Awake %d, FixedUpdate %d); torpedo drops are off", (int)a, (int)c); return false; }
  Log("found torpedoes: ready (TorpedoLauncher.Awake, selectIndex %s, Torpedo.FixedUpdate hooked)", b ? "hooked" : "NOT hooked: a saved found torpedo may load as an empty slot");
  return true;
}

