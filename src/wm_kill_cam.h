// torpedo kill cam, hit prediction, the crew guard (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- torpedo kill cam ------------------------------------------------------------------------------------------------
// When one of the player's torpedoes is about to hit an enemy ship, the camera follows it from behind while the game
// slows to killcam_slow, holds on the blast, then cuts back and eases the speed back up. After that nothing for
// killcam_cooldown seconds (David: 'make sure that it has a cooldown so we aren't spamming slow mo kills one after the
// other'). Single player only: the game's speed is one synced value for every player.
// Game speed (read in the disassembly 2026-11-02): GameStateSync.Update -> setupTimeScale copies the synced timeScale
// into GameManager.targetTimeScale every frame; BlurModulator.Update moves its own timeScale (+0x48) toward that and
// writes Time.timeScale whenever the two differ (it also scales the SFX volume by it, and stops physics at 0). So the
// kill cam hooks BlurModulator.Update and sets Time.timeScale = its value x the kill cam factor right after it: pause
// and the speed buttons keep working, and the sound is not turned down. Time.fixedDeltaTime (set by nothing else after
// launch) is scaled the same way so physics and shells stay smooth, and put back afterwards.
// Camera: PlayerCameraControlManager.LateUpdate is hooked as in the first-person mod: the game's camera update runs,
// then the kill cam places Camera.main. When the kill cam lets go, the game's own camera state takes over again.
// Torpedoes: Core.Torpedo.register (static HashSet<Torpedo>); body (Rigidbody), launchedFrom (TorpedoLauncher, .entity),
// armingTime, elapsedTime, explodedAfterLaunch.
struct KcApi {
  bool ok = false, tried = false;
  void *regField = nullptr, *rbPos = nullptr, *rbVel = nullptr, *timeGet = nullptr, *timeSet = nullptr, *fdtGet = nullptr, *fdtSet = nullptr;
  void *camMain = nullptr, *compTr = nullptr, *trSetPR = nullptr;
  int body = -1, from = -1, arm = -1, elapsed = -1, exploded = -1, launcherEntity = -1, leVel = -1, blurTs = -1;
  HashSetLayout hl;
};
static KcApi g_kc;
static bool KcInit() {
  if (g_kc.tried) return g_kc.ok;
  static DWORD lastTry = 0; DWORD now = GetTickCount();                // the register is empty until a torpedo exists: look every 2 s
  if (lastTry && now - lastTry < 2000) return false;
  lastTry = now;
  KcApi& k = g_kc; k.tried = true;
  void* tc = FindClass("Core", "Torpedo"); void* lc = FindClass("Core", "TorpedoLauncher"); void* le = FindClass("Core", "LiveEntity");
  void* rb = FindClass("UnityEngine", "Rigidbody"); void* tm = FindClass("UnityEngine", "Time"); void* cam = FindClass("UnityEngine", "Camera");
  void* comp = FindClass("UnityEngine", "Component"); void* tr = FindClass("UnityEngine", "Transform"); void* bm = FindClass("Core", "BlurModulator");
  k.regField = tc ? il2cpp_class_get_field_from_name(tc, "register") : nullptr;
  k.body = FieldOffset(tc, "body"); k.from = FieldOffset(tc, "launchedFrom"); k.arm = FieldOffset(tc, "armingTime"); k.elapsed = FieldOffset(tc, "elapsedTime"); k.exploded = FieldOffset(tc, "explodedAfterLaunch");
  k.launcherEntity = FieldOffset(lc, "entity"); k.leVel = FieldOffset(le, "realtimeVelocity"); k.blurTs = FieldOffset(bm, "timeScale");
  k.rbPos = rb ? FindMethod(rb, "get_position", 0) : nullptr; k.rbVel = rb ? FindMethod(rb, "get_linearVelocity", 0) : nullptr;
  k.timeGet = tm ? FindMethod(tm, "get_timeScale", 0) : nullptr; k.timeSet = tm ? FindMethod(tm, "set_timeScale", 1) : nullptr;
  k.fdtGet = tm ? FindMethod(tm, "get_fixedDeltaTime", 0) : nullptr; k.fdtSet = tm ? FindMethod(tm, "set_fixedDeltaTime", 1) : nullptr;
  k.camMain = cam ? FindMethod(cam, "get_main", 0) : nullptr; k.compTr = comp ? FindMethod(comp, "get_transform", 0) : nullptr;
  k.trSetPR = tr ? FindMethod(tr, "SetPositionAndRotation", 2) : nullptr;
  void* set = nullptr; if (k.regField) il2cpp_field_static_get_value(k.regField, &set);
  if (set) k.hl = HashSetLayoutOf(il2cpp_object_get_class(set));
  else if (k.regField) { k.tried = false; return false; }               // no torpedo yet this session: try again later
  k.ok = k.regField && k.hl.ok() && k.body >= 0 && k.from >= 0 && k.arm >= 0 && k.elapsed >= 0 && k.exploded >= 0 && k.launcherEntity >= 0 && k.leVel >= 0 &&
         k.blurTs >= 0 && k.rbPos && k.rbVel && k.timeGet && k.timeSet && k.fdtGet && k.fdtSet && k.camMain && k.compTr && k.trSetPR;
  Log("kill cam: %s", k.ok ? "ready" : "MISSING PIECES, off");
  (k.ok ? DLog : Log)("kill cam: torpedo register %p body +0x%X launchedFrom +0x%X armingTime +0x%X elapsedTime +0x%X exploded +0x%X launcher entity +0x%X "
      "velocity +0x%X BlurModulator.timeScale +0x%X; Rigidbody %p/%p Time %p/%p/%p/%p camera %p/%p/%p",
      k.regField, k.body, k.from, k.arm, k.elapsed, k.exploded, k.launcherEntity, k.leVel, k.blurTs, k.rbPos, k.rbVel, k.timeGet, k.timeSet, k.fdtGet, k.fdtSet,
      k.camMain, k.compTr, k.trSetPR);
  return k.ok;
}
static float KcFloat(void* method) { void* r = method ? Invoke(method, nullptr) : nullptr; return r ? *(float*)((char*)r + 0x10) : -1.0f; }
static void KcSetFloat(void* method, float v) { void* a[1] = { &v }; Invoke(method, nullptr, a); }
// The game's speed this frame as the mod should use it: Time.timeScale (the kill cam's slow motion included). 1 if unknown.
static float GameTimeScale() {
  static void* m = nullptr; static bool tried = false;
  if (!tried) { tried = true; void* tm = FindClass("UnityEngine", "Time"); m = tm ? FindMethod(tm, "get_timeScale", 0) : nullptr; }
  float t = KcFloat(m); return t < 0 ? 1.0f : t;
}

struct KcTorp { void* t; V3 p, v; bool armed, exploded; };
static std::vector<KcTorp> KcPlayerTorpedoes() {
  std::vector<KcTorp> out;
  void* set = nullptr; il2cpp_field_static_get_value(g_kc.regField, &set); if (!set) return out;
  void* arr = *(void**)((char*)set + g_kc.hl.slots); if (!arr) return out;
  int n = ArrLen(arr);
  if (g_kc.hl.count >= 0) { int l = *(int*)((char*)set + g_kc.hl.count); if (l >= 0 && l < n) n = l; }
  void* me = PlayerCraft(); if (!me) return out;
  for (int i = 0; i < n; i++) {
    void* t = *(void**)((char*)arr + 0x20 + (size_t)i * g_kc.hl.elem + g_kc.hl.val); if (!t) continue;
    void* lf = *(void**)((char*)t + g_kc.from); if (!lf || *(void**)((char*)lf + g_kc.launcherEntity) != me) continue;
    void* body = *(void**)((char*)t + g_kc.body); if (!body) continue;
    void* rp = Invoke(g_kc.rbPos, body); void* rv = Invoke(g_kc.rbVel, body); if (!rp || !rv) continue;
    KcTorp k; k.t = t; k.p = *(V3*)((char*)rp + 0x10); k.v = *(V3*)((char*)rv + 0x10);
    k.armed = *(float*)((char*)t + g_kc.elapsed) >= *(float*)((char*)t + g_kc.arm);
    k.exploded = *(bool*)((char*)t + g_kc.exploded);
    out.push_back(k);
  }
  return out;
}
// Rotation looking along f with the world up (Unity's Quaternion.LookRotation).
static Q4 LookRot(V3 f) {
  float l = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z); if (l < 1e-5f) return { 0, 0, 0, 1 };
  f = { f.x / l, f.y / l, f.z / l };
  V3 r = { f.z, 0, -f.x }; float rl = sqrtf(r.x * r.x + r.z * r.z); if (rl < 1e-5f) r = { 1, 0, 0 }; else { r.x /= rl; r.z /= rl; }
  V3 u = { f.y * r.z - f.z * r.y, f.z * r.x - f.x * r.z, f.x * r.y - f.y * r.x };
  float m00 = r.x, m01 = u.x, m02 = f.x, m10 = r.y, m11 = u.y, m12 = f.y, m20 = r.z, m21 = u.z, m22 = f.z, tr = m00 + m11 + m22; Q4 q;
  if (tr > 0) { float s = sqrtf(tr + 1) * 2; q = { (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s }; }
  else if (m00 > m11 && m00 > m22) { float s = sqrtf(1 + m00 - m11 - m22) * 2; q = { 0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s }; }
  else if (m11 > m22) { float s = sqrtf(1 + m11 - m00 - m22) * 2; q = { (m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s }; }
  else { float s = sqrtf(1 + m22 - m00 - m11) * 2; q = { (m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s }; }
  return q;
}

// Can a torpedo hit this ship? The game's torpedo radar only takes ships whose NavalUnitType has
// underwaterRadarSusceptible set (Torpedo.FixedUpdate 0x1809dade5), and exactly those carry the layer-21 'Torpedo
// Trigger' collider the hit check casts against (mask 0x200000). Patrol boats, gunboats, landing craft and the PT boats
// have neither, so a torpedo runs straight through them (David 2026-11-04: 'in the game, torpedoes only hit the bigger
// ships'; the Chomper Test Rack circled an IJN Patrol T1 at 0 m until it ran out of range). Cached per unit type.
static bool TorpHittable(void* ent) {
  static int offParams = -2, offFlag = -1; static void* nut = nullptr;
  if (offParams == -2) {
    void* ce = FindClass("Core", "CombatEntity"); nut = FindClass("Core", "NavalUnitType");
    offParams = FieldOffset(ce, "parameters");
    offFlag = FieldOffset(nut, "underwaterRadarSusceptible");
    if (offParams < 0 || offFlag < 0) Log("torpedoes: the hit check lookup failed (parameters %d, underwaterRadarSusceptible %d); homing takes every ship", offParams, offFlag);
  }
  if (offParams < 0 || offFlag < 0 || !ent) return true;
  void* p = *(void**)((char*)ent + offParams); if (!p) return false;
  static std::unordered_map<void*, bool> cache;
  auto c = cache.find(p); if (c != cache.end()) return c->second;
  bool isNut = false;
  for (void* k = il2cpp_object_get_class(p); k; k = il2cpp_class_get_parent(k)) if (k == nut) { isNut = true; break; }
  bool ok = isNut && *(bool*)((char*)p + offFlag);
  cache[p] = ok;
  if (!ok) Log("torpedoes: '%s' is too small for torpedoes (the game's own rule); homing torpedoes and the kill cam leave it alone", CrewClean(NameOf(p)).c_str());
  return ok;
}

// ---- kill cam hit prediction ----
// Until then the kill cam took the torpedo with the earliest closest pass to a ship's MIDDLE and called it a hit when
// that pass was within 0.35 x the long side of the ship's collider AABB: a circle. Against a Long Boi tanker (twice as
// long) a PT salvo had one torpedo running 20 m off the side ('passes 20.0 m from its middle', taken as a hit, it missed)
// while another really struck; and the AABB of a ship lying diagonally is inflated besides. Now the straight track is
// tested against the hull's real shape: the layer-21 'Misc/Torpedo Trigger' BoxCollider the game's own hit check casts
// for (SphereCastAll(pos, r, vel.normalized, detectionDistance, mask 0x200000); r = 4 m while arming, hitboxRadius once
// armed). The track is relative to the ship (its realtimeVelocity); the box's XZ
// rectangle comes from the collider transform's localToWorldMatrix at every check (ships turn, Elite traits rescale);
// the collider list, centre and size are cached per ship for the battle. Separate from TorpSteer and the seeking code.
struct KcBox { float cx, cz, ux, uz, hx, vx, vz, hz, bottom, circle; };    // circle > 0: the old circle test, no box
struct KcHullCache { std::vector<void*> cols, hs; std::vector<V3> ctr, size; DWORD at = 0; void* entH = nullptr; };   // entH pins the ship so its address is not reused while cached
static struct { bool tried = false, ok = false; void *inCh = nullptr, *boxType = nullptr, *center = nullptr, *size = nullptr, *enabled = nullptr, *layer = nullptr, *l2w = nullptr;
                int ttype = -1, hbr = -1, det = -1; } g_kh;
static std::unordered_map<void*, KcHullCache> g_khCache; static void* g_khEng = nullptr;
static bool KhInit() {
  if (g_kh.tried) return g_kh.ok;
  g_kh.tried = true;
  void* go = FindClass("UnityEngine", "GameObject"); void* bc = FindClass("UnityEngine", "BoxCollider"); void* col = FindClass("UnityEngine", "Collider");
  void* tr = FindClass("UnityEngine", "Transform"); void* tc = FindClass("Core", "Torpedo"); void* tt = FindClass("Core", "TorpedoType");
  g_kh.inCh = go ? FindMethod(go, "GetComponentsInternal", 6) : nullptr; g_kh.layer = go ? FindMethod(go, "get_layer", 0) : nullptr;
  g_kh.boxType = bc ? il2cpp_type_get_object(il2cpp_class_get_type(bc)) : nullptr;
  g_kh.center = bc ? FindMethod(bc, "get_center", 0) : nullptr; g_kh.size = bc ? FindMethod(bc, "get_size", 0) : nullptr;
  g_kh.enabled = col ? FindMethod(col, "get_enabled", 0) : nullptr; g_kh.l2w = tr ? FindMethod(tr, "get_localToWorldMatrix", 0) : nullptr;
  g_kh.ttype = FieldOffset(tc, "torpedoType"); g_kh.hbr = FieldOffset(tt, "hitboxRadius"); g_kh.det = FieldOffset(tt, "detectionDistance");
  g_kh.ok = g_kh.inCh && g_kh.layer && g_kh.boxType && g_kh.center && g_kh.size && g_kh.l2w && m_getGameObject && m_getTransform;
  if (g_kh.ok) Log("kill cam: hit prediction uses the ships' torpedo triggers (the layer-21 box the game's hit check casts for)");
  else Log("kill cam: hit prediction falls back to a hull box estimated from the ship's colliders, or the old circle test (a lookup is missing: "
           "GetComponentsInternal %p get_layer %p BoxCollider %p get_center %p get_size %p get_localToWorldMatrix %p)", g_kh.inCh, g_kh.layer, g_kh.boxType, g_kh.center, g_kh.size, g_kh.l2w);
  if (g_kh.ttype < 0 || g_kh.hbr < 0 || g_kh.det < 0) Log("kill cam: torpedo hitboxRadius/detectionDistance not found (%d %d %d); predicting with 5 m and 7 m", g_kh.ttype, g_kh.hbr, g_kh.det);
  return g_kh.ok;
}
// localToWorldMatrix of a transform, boxed Matrix4x4 at +0x10, column-major (m[col*4 + row]); false if it failed.
static bool KhMatrix(void* tr, float m[16]) { void* r = tr && g_kh.l2w ? Invoke(g_kh.l2w, tr) : nullptr; if (!r) return false; memcpy(m, (char*)r + 0x10, 64); return true; }
// The ship's hull as boxes in XZ: 1 its torpedo triggers, 2 a box along its forward axis from its solid colliders (no
// trigger found), 3 the old circle (nothing better). Returns the method, or 0 if none.
static int KcHullBoxes(void* ent, V3 pos, void* eng, std::vector<KcBox>& out) {
  out.clear(); DWORD now = GetTickCount();
  if (eng != g_khEng) { for (auto& p : g_khCache) { for (void* h : p.second.hs) Unpin(h); Unpin(p.second.entH); } g_khCache.clear(); g_khEng = eng; }
  if (KhInit()) {
    auto it = g_khCache.find(ent);
    if (it == g_khCache.end() || (it->second.cols.empty() && now - it->second.at > 10000)) {   // none found: look again after 10 s
      KcHullCache& c = g_khCache[ent]; for (void* h : c.hs) Unpin(h); Unpin(c.entH); c = KcHullCache(); c.at = now; c.entH = Pin(ent);
      bool typed = true, recursive = true, inactive = false, reverse = false; void* a[6] = { g_kh.boxType, &typed, &recursive, &inactive, &reverse, nullptr };
      void* goEnt = Invoke(m_getGameObject, ent); void* arr = goEnt ? Invoke(g_kh.inCh, goEnt, a) : nullptr; int n = ArrLen(arr);
      for (int i = 0; i < n && i < 64; i++) {
        void* b = ArrAt(arr, i); if (!b) continue;
        void* g = Invoke(m_getGameObject, b); void* l = g ? Invoke(g_kh.layer, g) : nullptr; if (!l || *(int*)((char*)l + 0x10) != 21) continue;
        void* ce = Invoke(g_kh.center, b); void* sz = Invoke(g_kh.size, b); if (!ce || !sz) continue;
        c.cols.push_back(b); c.hs.push_back(Pin(b)); c.ctr.push_back(*(V3*)((char*)ce + 0x10)); c.size.push_back(*(V3*)((char*)sz + 0x10));
      }
      static std::set<std::string> logged; std::string nm = CrewClean(NameOf(ent));
      if (logged.insert(nm + (c.cols.empty() ? "|0" : "|1")).second) {
        if (c.cols.empty()) Log("kill cam: '%s' has no torpedo trigger box; its hits are predicted from a box along its colliders", nm.c_str());
        else { const V3& s = c.size[0]; DLog("kill cam: '%s' torpedo trigger: %d box(es), the first %.1f x %.1f x %.1f m (unscaled)", nm.c_str(), (int)c.cols.size(), s.x, s.y, s.z); }
      }
      it = g_khCache.find(ent);
    }
    const KcHullCache& c = it->second;
    for (size_t i = 0; i < c.cols.size(); i++) {
      void* b = c.cols[i];
      if (g_kh.enabled) { void* e = Invoke(g_kh.enabled, b); if (!e || !*(bool*)((char*)e + 0x10)) continue; }   // null: destroyed
      float m[16]; if (!KhMatrix(Invoke(m_getTransform, b), m)) continue;
      const V3& lc = c.ctr[i]; const V3& ls = c.size[i];
      float wx = m[0] * lc.x + m[4] * lc.y + m[8] * lc.z + m[12], wy = m[1] * lc.x + m[5] * lc.y + m[9] * lc.z + m[13], wz = m[2] * lc.x + m[6] * lc.y + m[10] * lc.z + m[14];
      float ax = m[0] * ls.x / 2, ay = m[1] * ls.x / 2, az = m[2] * ls.x / 2, yy = m[5] * ls.y / 2, bx = m[8] * ls.z / 2, by = m[9] * ls.z / 2, bz = m[10] * ls.z / 2;
      float hx = sqrtf(ax * ax + az * az), hz = sqrtf(bx * bx + bz * bz); if (hx < 0.05f || hz < 0.05f) continue;
      out.push_back({ wx, wz, ax / hx, az / hx, hx, bx / hz, bz / hz, hz, wy - fabsf(ay) - fabsf(yy) - fabsf(by), 0 });
    }
    if (!out.empty()) return 1;
  }
  // No trigger: a box along the ship's forward axis. The collider AABB is a world box, so the hull's length L and width
  // W are solved from it with the heading (sx = L|fx| + W|fz|, sz = L|fz| + W|fx|); near 45 degrees, where that is ill
  // conditioned, W = 0.2 L is assumed.
  float lo[3], hi[3], m[16];
  if (ColliderBox(ent, lo, hi) > 0 && hi[0] - lo[0] < 400 && hi[2] - lo[2] < 400 && KhMatrix(Invoke(m_getTransform, ent), m)) {
    float fx = m[8], fz = m[10], fl = sqrtf(fx * fx + fz * fz);
    if (fl > 0.01f) {
      fx /= fl; fz /= fl; float cx = fabsf(fx), cz = fabsf(fz), sx = hi[0] - lo[0], sz = hi[2] - lo[2], det = cx * cx - cz * cz, L, W;
      if (fabsf(det) > 0.3f) { L = (sx * cx - sz * cz) / det; W = (sz * cx - sx * cz) / det; } else { L = (std::max)(sx, sz) / ((std::max)(cx, cz) + 0.2f * (std::min)(cx, cz)); W = 0.2f * L; }
      if (L > 2) { W = (std::max)(2.0f, (std::min)(W, L));
        out.push_back({ (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2, fx, fz, L / 2, fz, -fx, W / 2, lo[1], 0 }); return 2; }
    }
  }
  float r = 10;                                                        // the old test: about half the hull's length around its middle
  if (ColliderBox(ent, lo, hi) > 0 && hi[0] - lo[0] < 400 && hi[2] - lo[2] < 400) { r = 0.35f * (std::max)(hi[0] - lo[0], hi[2] - lo[2]); if (r < 4) r = 4; if (r > 45) r = 45; }
  out.push_back({ pos.x, pos.z, 1, 0, 0, 0, 1, 0, -1e9f, r }); return 3;
}
// The game's cast for this torpedo: radius (4 m while arming, hitboxRadius once armed) and how far ahead it reaches.
static void KcCast(const KcTorp& k, float& r, float& reach) {
  r = k.armed ? 5.0f : 4.0f; reach = 7.0f;
  void* ty = g_kh.ttype >= 0 ? *(void**)((char*)k.t + g_kh.ttype) : nullptr; if (!ty) return;
  if (k.armed && g_kh.hbr >= 0) { float v = *(float*)((char*)ty + g_kh.hbr); if (v > 0 && v < 100) r = v; }
  if (g_kh.det >= 0) { float v = *(float*)((char*)ty + g_kh.det); if (v >= 0 && v < 100) reach = v; }
}
// First game time in [0, tMax] at which the torpedo's cast (a ball of radius r swept reach metres ahead along its own
// heading) touches one of the boxes, on its straight track relative to the ship; -1 if none. Every 0.02 game s (~1.5 m).
static float KcEntry(const KcTorp& k, const float* sv, const std::vector<KcBox>& bs, float tMax) {
  float r, reach; KcCast(k, r, reach);
  float wx = k.v.x - sv[0], wz = k.v.z - sv[2], hl = sqrtf(k.v.x * k.v.x + k.v.z * k.v.z), dx = hl > 0.1f ? k.v.x / hl : 0, dz = hl > 0.1f ? k.v.z / hl : 0;
  for (float t = 0; t <= tMax + 1e-4f; t += 0.02f) {
    float px = k.p.x + wx * t, pz = k.p.z + wz * t;
    for (const KcBox& b : bs) {
      if (b.circle > 0) { float mx = px - b.cx, mz = pz - b.cz; if (mx * mx + mz * mz <= b.circle * b.circle) return t; continue; }
      if (k.p.y < b.bottom - r) continue;                              // runs under the box
      for (int j = 0; j < 3; j++) {
        float qx = px + dx * reach * 0.5f * j - b.cx, qz = pz + dz * reach * 0.5f * j - b.cz;
        float ex = (std::max)(0.0f, fabsf(qx * b.ux + qz * b.uz) - b.hx), ez = (std::max)(0.0f, fabsf(qx * b.vx + qz * b.vz) - b.hz);
        if (ex * ex + ez * ez <= r * r) return t;
      }
    }
  }
  return -1;
}
// Among the player's torpedoes (other than skip), the one that enters a hittable enemy hull first, between tMin and tMax
// game s. hitsOut (if given): how many of the torpedoes in the water (not deep, not exploded) enter one in 3 game s.
struct KcPick { const KcTorp* t = nullptr; void* ship = nullptr; float at = 1e9f; int method = 0; };
static KcPick KcPickHit(const std::vector<KcTorp>& ts, const std::vector<EnemyAt>& es, float tMin, float tMax, void* skip, void* eng, int* hitsOut = nullptr) {
  KcPick best; std::unordered_map<void*, std::pair<int, std::vector<KcBox>>> hull;   // per call: each ship's boxes, built when first needed
  auto boxes = [&](const EnemyAt& e) -> std::pair<int, std::vector<KcBox>>& {
    auto it = hull.find(e.ent); if (it != hull.end()) return it->second;
    auto& h = hull[e.ent]; h.first = KcHullBoxes(e.ent, e.pos, eng, h.second); return h; };
  int hits = 0;
  for (const KcTorp& k : ts) {
    if (k.exploded || k.p.y < -10.0f || k.v.x * k.v.x + k.v.z * k.v.z < 9) continue;   // deep: diving on a sub, under any hull
    bool candidate = k.armed && k.t != skip, counted = false;
    if (!candidate && !hitsOut) continue;
    for (const EnemyAt& e : es) {
      if (!e.ship || !TorpHittable(e.ent)) continue;                   // no torpedo can hit it: no kill cam for a pass under it
      const float* ev = (const float*)((char*)e.ent + g_kc.leVel);
      float rx = e.pos.x - k.p.x, rz = e.pos.z - k.p.z, wx = ev[0] - k.v.x, wz = ev[2] - k.v.z, ww = wx * wx + wz * wz;
      if (ww < 1 || rx * rx + rz * rz > (sqrtf(ww) * 3 + 250) * (sqrtf(ww) * 3 + 250)) continue;   // too far to reach it in 3 s
      auto& h = boxes(e); if (!h.first) continue;
      float t = KcEntry(k, ev, h.second, hitsOut ? 3.0f : tMax);
      if (t < 0) continue;
      if (!counted) { counted = true; hits++; }
      if (candidate && t >= tMin && t <= tMax && t < best.at) { best.t = &k; best.ship = e.ent; best.at = t; best.method = h.first; }
    }
  }
  if (hitsOut) *hitsOut = hits;
  return best;
}

static int g_kcState = 0;                                           // 0 watching, 1 following the torpedo, 2 holding on the blast, 3 easing back
static float g_kcFactor = 1.0f, g_kcBaseFdt = 0;                     // factor: read by the BlurModulator hook
static void *g_kcTorp = nullptr, *g_kcTorpH = nullptr, *g_kcTarget = nullptr, *g_kcTargetH = nullptr, *g_kcEng = nullptr;
static double g_kcT = 0, g_kcTotal = 0, g_kcGameT = 0, g_kcPred = 0;  // real seconds in this state and in all; game seconds following, and the predicted time to the hit
static double g_kcFollowT = 0;                                       // real seconds following the current torpedo (the 20 s limit; restarts on a switch)
static DWORD g_kcEndedAt = 0; static int g_kcHow = 0;                // how the torpedo ended: 0 not yet / ran too long, 1 exploded, 2 left the game
static bool g_kcHadEnd = false, g_kcHit = false, g_kcCamSet = false, g_kcBlurSeen = false;
static V3 g_kcCam = {}, g_kcLook = {}, g_kcBlast = {};
static LARGE_INTEGER g_kcLast = {};
static int g_kcFaults = 0, g_kcShown = 0, g_kcFrame = 0;
static bool g_kcSwitched = false;                                    // switched once to another torpedo after a miss (at most one per kill cam)
struct KcOther { void* t; void* h; int how; };                       // the player's other torpedoes in the water at the start; how: 0 still going, 1 exploded, 2 left the game
static std::vector<KcOther> g_kcOthers;
static void KcOthersClear() { for (KcOther& o : g_kcOthers) Unpin(o.h); g_kcOthers.clear(); }
static void KcPlaceCamera(V3 pos, V3 look) {
  void* cam = Invoke(g_kc.camMain, nullptr); void* t = cam ? Invoke(g_kc.compTr, cam) : nullptr; if (!t) return;
  Q4 q = LookRot({ look.x - pos.x, look.y - pos.y, look.z - pos.z });
  void* a[2] = { &pos, &q }; Invoke(g_kc.trSetPR, t, a);
}
static void KcStop(const char* why) {
  if (g_kcBaseFdt > 0) KcSetFloat(g_kc.fdtSet, g_kcBaseFdt);
  bool sunk = g_kcTarget && g_offLeAlive >= 0 && !*(bool*)((char*)g_kcTarget + g_offLeAlive);
  Log("kill cam: over after %.1f s (%s; torpedo %s; '%s' %s); the next one can come in %.0f s at the earliest", g_kcTotal, why, g_kcHow == 1 ? "exploded" : g_kcHow == 2 ? "disappeared (hit, most likely)" : g_kcHow == 3 ? "went past it" : "never arrived",
      g_kcTarget ? CrewClean(NameOf(g_kcTarget)).c_str() : "?", sunk ? "sank" : "still afloat", g_kcCooldown);
  { int ex = 0, left = 0; for (const KcOther& o : g_kcOthers) { if (o.how == 1) ex++; else if (o.how == 2) left++; }
    if (ex) Log("kill cam: another of your torpedoes went off meanwhile (%d of the %d others exploded%s)", ex, (int)g_kcOthers.size(), left ? ", more ran out or left the game" : "");
    else if (left) Log("kill cam: %d of your other %d torpedoes left the game meanwhile without going off", left, (int)g_kcOthers.size());
    else if (!g_kcOthers.empty()) Log("kill cam: none of your other %d torpedoes went off meanwhile", (int)g_kcOthers.size()); }
  KcOthersClear();
  Unpin(g_kcTorpH); Unpin(g_kcTargetH); g_kcTorpH = g_kcTargetH = nullptr; g_kcTorp = g_kcTarget = nullptr;
  g_kcState = 0; g_kcFactor = 1.0f; g_kcBaseFdt = 0; g_kcCamSet = false; g_kcEndedAt = GetTickCount(); g_kcHadEnd = true;
}

// ---- the kill cam's crew guard ----
// After the kill cam cut back to the boat, crew members could fall off it.
// Traced 2026-11-05 in the disassembly: CrewMember.FixedUpdateMe (rva 0x99EE30) gives a crew member's float point the
// real Crest depth only while the camera is within CrestManager.simDistance, else depth 0. Then, if the depth is over
// 0.1 and smoothChestPos.y under 1.25 (or transform.up.y under 0.4), framesUntilUnderwater (float) -= 1 per fixed STEP,
// else it is reset to 18..24 (timeScale <= 1.25); UpdateMe throws the crew member overboard once it is below 0.
// CrestManager.sampleSequence also blends every sample toward flat water by camera distance, so while the kill cam is
// kilometres away the boat floats on flat water, and the real waves come back in one step when the camera does: the low
// PT deck can dip under for a moment. The camera used to come back during the slow-motion ease-back, with fixedDeltaTime
// scaled down, so the 18-24 step grace was only a fraction of its game time. Now the ease-back keeps the kill camera
// until full speed, and this detour keeps the counter of the player's crew at 18 or more during the kill cam and for 2
// real seconds after it hands the camera back (and in slow motion it counts game time, not steps, for every crew).
// Hits do not use the counter (hitFlash -> throwOverboardImpact), so they still throw crew.
typedef void (*fn_crewFixed)(void*, float, void*);
static fn_crewFixed g_crewFixedOrig = nullptr;
static bool g_cgOn = false, g_cgTried = false, g_cgPending = false;
static int g_cgOffMgr = -1, g_cgOffFp = -1, g_cgOffChest = -1, g_cgOffCount = -1, g_cgOffEnt = -1, g_cgOffDepth = -1;
static void* g_cgCraft = nullptr;                                     // the player's boat, taken when a kill cam starts
static DWORD g_cgUntil = 0;                                           // GetTickCount: the guard's window after the camera came back
static int g_kcRun = 0, g_cgRun = -1, g_cgKept = 0, g_cgSteps = 0, g_cgLines = 0;
struct CgCrew { void* c; float virt; bool fell; };                    // virt: the counter as it would have run without the guard
static std::vector<CgCrew> g_cgCrew;
static const float kCgMark = 1000.0f;                                 // put in before the original: 999 after it means it counted down
static bool CgWindow() { return g_kcState != 0 || (g_cgUntil && (int)(g_cgUntil - GetTickCount()) > 0); }
static void CgReport() {
  if (!g_cgPending) return;
  g_cgPending = false;
  if (g_cgKept) Log("kill cam: kept %d crew aboard while the waves came back", g_cgKept);
  DLog("kill cam: crew guard: %d of your crew watched, %d would have gone over, %d fixed steps held them", (int)g_cgCrew.size(), g_cgKept, g_cgSteps);
  g_cgCrew.clear();
}
static bool CgPos(void* obj, V3* out) {
  void* t = obj && g_kc.compTr ? Invoke(g_kc.compTr, obj) : nullptr; void* p = t && m_trGetPos ? Invoke(m_trGetPos, t) : nullptr;
  if (!p) return false;
  *out = *(V3*)((char*)p + 0x10); return true;
}
static int CgBefore(void* c, float* before, bool* mine) {
  void* mgr = *(void**)((char*)c + g_cgOffMgr);
  *mine = g_cgCraft && mgr && *(void**)((char*)mgr + g_cgOffEnt) == g_cgCraft;
  if (!*mine && !(g_kcFactor < 0.999f && g_kcBaseFdt > 0)) return 0;
  float* n = (float*)((char*)c + g_cgOffCount);
  *before = *n > 24.5f ? 18.0f : *n;                                  // above any reset value: a stale marker, start again from 18
  *n = kCgMark; return 1;
}
static void CgAfter(void* c, float fdt, float before, bool mine) {
  float* n = (float*)((char*)c + g_cgOffCount); float after = *n;
  if (after == kCgMark) { *n = before; return; }                     // it returned early (blocked, a replay): unchanged
  bool down = after == kCgMark - 1.0f;
  float step = 1.0f;                                                  // slow motion: count the step's share of a normal step
  if (down && g_kcFactor < 0.999f && g_kcBaseFdt > 0) step = (std::min)(1.0f, (std::max)(0.0f, fdt / g_kcBaseFdt));
  float v = down ? before - step : after;
  if (mine) {
    if (g_cgRun != g_kcRun) { CgReport(); g_cgRun = g_kcRun; g_cgKept = g_cgSteps = g_cgLines = 0; g_cgPending = true; }
    CgCrew* e = nullptr; for (CgCrew& x : g_cgCrew) if (x.c == c) { e = &x; break; }
    if (!e) { g_cgCrew.push_back({ c, before, false }); e = &g_cgCrew.back(); }
    e->virt = down ? e->virt - step : after;
    if (v < 18.0f) {
      if (e->virt < 0) { g_cgSteps++; if (!e->fell) { e->fell = true; g_cgKept++; } }
      v = 18.0f;
    }
    if (down && g_debug && g_cgLines < 200) {
      g_cgLines++;
      void* fp = *(void**)((char*)c + g_cgOffFp); float depth = fp ? *(float*)((char*)fp + g_cgOffDepth) : -1;
      float chest = *(float*)((char*)c + g_cgOffChest + 4);
      V3 cp, mp; void* cam = g_kc.camMain ? Invoke(g_kc.camMain, nullptr) : nullptr;
      float dist = cam && CgPos(cam, &cp) && CgPos(c, &mp) ? sqrtf((cp.x - mp.x) * (cp.x - mp.x) + (cp.y - mp.y) * (cp.y - mp.y) + (cp.z - mp.z) * (cp.z - mp.z)) : -1;
      DLog("kill cam: crew guard: '%s' under water: counter %.2f (unguarded %.2f), depth %.2f, chest y %.2f, camera %.0f m away, state %d x%.2f",
          CrewClean(NameOf(c)).c_str(), v, e->virt, depth, chest, dist, g_kcState, g_kcFactor);
    }
  }
  *n = v;
}
static void SafeCgRestore(void* c, float before) { __try { *(float*)((char*)c + g_cgOffCount) = before; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static int CgRestoreFilter(void* c, float before) { SafeCgRestore(c, before); return EXCEPTION_CONTINUE_SEARCH; }
static void Hooked_CrewFixed(void* self, float fdt, void* method) {
  if (!g_cgOn || !self || !CgWindow()) { g_crewFixedOrig(self, fdt, method); return; }
  float before = 0; bool mine = false; int mode = 0;
  __try { mode = CgBefore(self, &before, &mine); }
  __except (FaultFilter("kill cam crew guard", GetExceptionInformation())) { g_cgOn = false; mode = 0; }
  if (mode) { __try { g_crewFixedOrig(self, fdt, method); } __except (CgRestoreFilter(self, before)) {} }  // a throw from the original: put the counter back, let it carry on
  else g_crewFixedOrig(self, fdt, method);
  if (!mode) return;
  __try { CgAfter(self, fdt, before, mine); }
  __except (FaultFilter("kill cam crew guard", GetExceptionInformation())) { g_cgOn = false; SafeCgRestore(self, before); }
}
// On the game thread (the camera's LateUpdate), so FixedUpdateMe is not running while it is patched.
static void CgInstall() {
  g_cgTried = true;
  void* cm = FindClass("Core", "CrewMember"); void* mg = FindClass("Core", "CrewManager"); void* fc = FindClass("Core", "FloatPoint");
  g_cgOffMgr = FieldOffset(cm, "crewManager"); g_cgOffFp = FieldOffset(cm, "floatPoint"); g_cgOffChest = FieldOffset(cm, "smoothChestPos"); g_cgOffCount = FieldOffset(cm, "framesUntilUnderwater");
  g_cgOffEnt = FieldOffset(mg, "entity"); g_cgOffDepth = FieldOffset(fc, "_depth");
  void* mi = cm ? il2cpp_class_get_method_from_name(cm, "FixedUpdateMe", 1) : nullptr;
  if (!mi || g_cgOffMgr < 0 || g_cgOffFp < 0 || g_cgOffChest < 0 || g_cgOffCount < 0 || g_cgOffEnt < 0 || g_cgOffDepth < 0) {
    Log("kill cam: no crew guard (CrewMember.FixedUpdateMe %p, crewManager +0x%X floatPoint +0x%X smoothChestPos +0x%X framesUntilUnderwater +0x%X entity +0x%X _depth +0x%X); "
        "the kill cam still works", mi, g_cgOffMgr, g_cgOffFp, g_cgOffChest, g_cgOffCount, g_cgOffEnt, g_cgOffDepth);
    return;
  }
  // push rbx; sub rsp,0x60; cmp byte ptr [rip+disp],0 (rip-relative: the trampoline does it through rax); mov rbx,rcx
  static const unsigned char kFixed[16] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x60, 0x80, 0x3D, 0xBC, 0xAC, 0x84, 0x03, 0x00, 0x48, 0x8B, 0xD9 };
  unsigned char* p = (unsigned char*)*(void**)mi;
  uint64_t flag = (uint64_t)p + 13 + (int64_t)*(int32_t*)(kFixed + 8);
  unsigned char body[24] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x60,       // push rbx; sub rsp,0x60
                             0x50, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, // push rax; mov rax,flag
                             0x80, 0x38, 0x00, 0x58,                    // cmp byte ptr [rax],0; pop rax (the flags stay)
                             0x48, 0x8B, 0xD9 };                        // mov rbx,rcx; then back to +16 (movaps ...; jne)
  memcpy(body + 9, &flag, 8);
  g_crewFixedOrig = (fn_crewFixed)Detour(p, (void*)&Hooked_CrewFixed, kFixed, 16, "CrewMember.FixedUpdateMe", body, sizeof(body));
  if (!g_crewFixedOrig) { Log("kill cam: no crew guard (CrewMember.FixedUpdateMe could not be detoured); the kill cam still works"); return; }
  g_cgOn = true;
  Log("kill cam: crew guard ready (CrewMember.FixedUpdateMe detoured)");
  DLog("kill cam: crew guard: rva 0x%llX, framesUntilUnderwater +0x%X, crewManager +0x%X -> entity +0x%X, floatPoint +0x%X -> _depth +0x%X, smoothChestPos +0x%X",
       (unsigned long long)((uintptr_t)p - (uintptr_t)g_ga), g_cgOffCount, g_cgOffMgr, g_cgOffEnt, g_cgOffFp, g_cgOffDepth, g_cgOffChest);
}
static void KillCamTick() {
  LARGE_INTEGER now; QueryPerformanceCounter(&now);
  double dt = g_kcLast.QuadPart ? PerfMs(g_kcLast) / 1000.0 : 0; g_kcLast = now; if (dt > 0.1) dt = 0.1;
  void* eng = EngagementInstance();
  if (!g_kcState && (g_cgUntil || g_cgPending) && !CgWindow()) { g_cgUntil = 0; CgReport(); }
  if (g_kcState && (!eng || eng != g_kcEng || g_coopRole)) { KcStop(!eng || eng != g_kcEng ? "the battle ended" : "co-op"); return; }
  if (!g_kcState) {
    if (!g_kcOn || g_coopRole || !eng || (++g_kcFrame & 1)) return;
    if (g_kcHadEnd && GetTickCount() - g_kcEndedAt < (DWORD)(g_kcCooldown * 1000)) return;   // the cooldown
    if (!KcInit()) return;
    std::vector<KcTorp> ts = KcPlayerTorpedoes(); if (ts.empty()) return;
    const std::vector<EnemyAt>& es = LiveEnemies();
    // The torpedo that really enters a hull first, 0.3..1.2 game s ahead (1.2 game s is about 4.5 real s at quarter speed;
    // 2.5 felt far too early). Until 2026-11-05 this was the earliest closest pass to a ship's middle (see KcHullBoxes).
    KcPick pk = KcPickHit(ts, es, 0.3f, 1.2f, nullptr, eng);
    if (!pk.t) return;
    const KcTorp* best = pk.t; void* bestShip = pk.ship; float bestT = pk.at;
    int inWater = 0, hits = 0; for (const KcTorp& k : ts) if (!k.exploded) inWater++;
    KcPickHit(ts, es, 0, 3.0f, nullptr, eng, &hits);
    KcOthersClear(); for (const KcTorp& k : ts) if (k.t != best->t && !k.exploded) g_kcOthers.push_back({ k.t, Pin(k.t), 0 });
    g_kcTorp = best->t; g_kcTorpH = Pin(best->t); g_kcTarget = bestShip; g_kcTargetH = Pin(bestShip); g_kcEng = eng;
    g_kcBaseFdt = KcFloat(g_kc.fdtGet); if (g_kcBaseFdt <= 0) g_kcBaseFdt = 0;
    g_kcPred = bestT; g_kcGameT = 0; g_kcFollowT = 0; g_kcSwitched = false;
    CgReport(); g_kcRun++; g_cgUntil = 0; g_cgCraft = PlayerCraft();
    g_kcState = 1; g_kcT = g_kcTotal = 0; g_kcHit = false; g_kcHow = 0; g_kcCamSet = false; g_kcBlast = best->p;
    Log("kill cam: torpedo about to hit '%s': %s in %.1f game s; slow motion x%.2f; %d of your torpedoes in the water, %d predicted to hit a ship within 3 game s",
        CrewClean(NameOf(bestShip)).c_str(), pk.method == 1 ? "enters its hull" : pk.method == 2 ? "enters its estimated hull (no torpedo trigger box)" : "comes within the old circle around its middle",
        bestT, g_kcSlow, inWater, hits);
    if (g_kcShown++ < 1 && !g_kcBlurSeen) Log("kill cam: BlurModulator.Update has not run yet, so the game speed may not change");
  }
  float blurTs = 1;                                                    // paused: hold everything where it is
  { float t = KcFloat(g_kc.timeGet); if (t >= 0 && t < 0.01f) blurTs = 0; }
  if (blurTs > 0) { g_kcT += dt; g_kcTotal += dt; float gt = KcFloat(g_kc.timeGet); if (g_kcState == 1) g_kcFollowT += dt; if (g_kcState == 1 && gt > 0) g_kcGameT += dt * gt; }
  V3 tgt = g_kcBlast;                                                  // the target's middle, 2 m up
  { void* tr = Invoke(g_kc.compTr, g_kcTarget); void* p = tr && m_trGetPos ? Invoke(m_trGetPos, tr) : nullptr; if (p) { tgt = *(V3*)((char*)p + 0x10); tgt.y += 2; } }
  std::vector<KcTorp> ts;                                              // following or holding: the player's torpedoes now
  if (g_kcState == 1 || g_kcState == 2) {
    ts = KcPlayerTorpedoes();
    for (KcOther& o : g_kcOthers) {                                    // did another one go off meanwhile? (the repool takes it out of the
      if (o.how) continue;                                             // register first, then sets explodedAfterLaunch: read it after)
      const KcTorp* f = nullptr; for (const KcTorp& k : ts) if (k.t == o.t) { f = &k; break; }
      if (f && f->exploded) o.how = 1; else if (!f) o.how = *(bool*)((char*)o.t + g_kc.exploded) ? 1 : 2;
    }
  }
  if (g_kcState == 1) {
    const KcTorp* me = nullptr;
    for (const KcTorp& k : ts) if (k.t == g_kcTorp) { me = &k; break; }
    if (!me || me->exploded) { g_kcHow = me ? 1 : 2; g_kcHit = true; g_kcState = 2; g_kcT = 0; }
    // Until 2026-11-02 the limit was 6 REAL seconds, which at quarter speed cut away 1.5 game s in, before a 2.5 s hit
    // (David saw it cut before the torpedo arrived). Now: the predicted time plus 1.5 game s, 20 real s at most.
    else if (g_kcGameT > g_kcPred + 1.5 || g_kcFollowT > 20) { g_kcState = 3; g_kcT = 0; }
    else if (g_kcGameT > 0.3 && [&] {                                  // it has gone past the target's middle: a miss
      const float* ev = (const float*)((char*)g_kcTarget + g_kc.leVel);
      float rx = tgt.x - me->p.x, rz = tgt.z - me->p.z, wx = ev[0] - me->v.x, wz = ev[2] - me->v.z, ww = wx * wx + wz * wz;
      if (!(ww > 1 && -(rx * wx + rz * wz) / ww < -0.5f && sqrtf(rx * rx + rz * rz) > 25)) return false;
      std::vector<KcBox> bs;                                           // past the middle, but still heading into the stern or bow: not yet
      return !KcHullBoxes(g_kcTarget, tgt, eng, bs) || KcEntry(*me, ev, bs, 0.5f) < 0; }()) {
      // A miss. With a salvo another torpedo may be about to hit: follow that one instead, once (David 2026-11-05).
      KcPick pk; if (!g_kcSwitched) pk = KcPickHit(ts, LiveEnemies(), 0.3f, 1.5f, g_kcTorp, eng);
      if (pk.t) {
        Log("kill cam: that one missed; switching to another torpedo about to hit '%s' in %.1f game s", CrewClean(NameOf(pk.ship)).c_str(), pk.at);
        Unpin(g_kcTorpH); Unpin(g_kcTargetH);
        g_kcTorp = pk.t->t; g_kcTorpH = Pin(pk.t->t); g_kcTarget = pk.ship; g_kcTargetH = Pin(pk.ship);
        g_kcPred = pk.at; g_kcGameT = 0; g_kcFollowT = 0; g_kcSwitched = true; g_kcBlast = pk.t->p;   // slow motion and the camera smoothing carry on
        for (size_t i = 0; i < g_kcOthers.size(); i++) if (g_kcOthers[i].t == g_kcTorp) { Unpin(g_kcOthers[i].h); g_kcOthers.erase(g_kcOthers.begin() + i); break; }
      }
      else { g_kcHow = 3; g_kcState = 3; g_kcT = 0; }
    }
    else {
      g_kcBlast = me->p;
      float hl = sqrtf(me->v.x * me->v.x + me->v.z * me->v.z); V3 d = hl > 0.1f ? V3{ me->v.x / hl, 0, me->v.z / hl } : V3{ 0, 0, 1 };
      V3 want = { me->p.x - d.x * 11 + d.z * 3, (std::max)(me->p.y + 4.0f, 2.5f), me->p.z - d.z * 11 - d.x * 3 };
      V3 look = { me->p.x + (tgt.x - me->p.x) * 0.6f, me->p.y + (tgt.y - me->p.y) * 0.6f, me->p.z + (tgt.z - me->p.z) * 0.6f };
      if (!g_kcCamSet) { g_kcCam = want; g_kcLook = look; g_kcCamSet = true; }
      else { float a = 1 - expf(-(float)dt * 6); g_kcCam = { g_kcCam.x + (want.x - g_kcCam.x) * a, g_kcCam.y + (want.y - g_kcCam.y) * a, g_kcCam.z + (want.z - g_kcCam.z) * a };
             g_kcLook = { g_kcLook.x + (look.x - g_kcLook.x) * a, g_kcLook.y + (look.y - g_kcLook.y) * a, g_kcLook.z + (look.z - g_kcLook.z) * a }; }
      g_kcFactor = (float)(1 + (g_kcSlow - 1) * (std::min)(1.0, g_kcT / 0.25));
      KcPlaceCamera(g_kcCam, g_kcLook);
    }
  }
  if (g_kcState == 2) {                                                // the blast: keep the camera, turn toward it
    g_kcFactor = (float)g_kcSlow;
    if (g_kcCamSet) { float a = 1 - expf(-(float)dt * 4); V3 b = { (g_kcBlast.x + tgt.x) / 2, (g_kcBlast.y + tgt.y) / 2 + 1, (g_kcBlast.z + tgt.z) / 2 };
                      g_kcLook = { g_kcLook.x + (b.x - g_kcLook.x) * a, g_kcLook.y + (b.y - g_kcLook.y) * a, g_kcLook.z + (b.z - g_kcLook.z) * a };
                      KcPlaceCamera(g_kcCam, g_kcLook); }
    if (g_kcT > 1.5) { g_kcState = 3; g_kcT = 0; }
  }
  if (g_kcState == 3) {                                                // the speed eases back over half a second with the kill camera still out
    g_kcFactor = (float)(g_kcSlow + (1 - g_kcSlow) * (std::min)(1.0, g_kcT / 0.5));   // here; then the game takes the camera back at full speed
    if (g_kcT >= 0.5) {                                                // (until 2026-11-05 it came back in slow motion: crew went overboard)
      KcStop(g_kcHit ? "done" : "the torpedo missed or ran out");
      g_cgUntil = GetTickCount() + 2000; if (!g_cgUntil) g_cgUntil = 1;  // the crew guard, 2 real s more
    }
    else if (g_kcCamSet) KcPlaceCamera(g_kcCam, g_kcLook);
  }
}
static void SafeKillCamOff() {
  __try { if (g_kcBaseFdt > 0 && g_kc.fdtSet) KcSetFloat(g_kc.fdtSet, g_kcBaseFdt); } __except (EXCEPTION_EXECUTE_HANDLER) {}
  g_kcState = 0; g_kcFactor = 1.0f; g_kcBaseFdt = 0; g_kcOn = false;
}
static void (*g_camLateOriginal)(void*, void*) = nullptr;
static void Hooked_CamLateUpdate(void* self, void* method) {
  g_camLateOriginal(self, method);
  if (!g_ready || g_kcFaults < 0 || (!g_kcOn && !g_kcState)) return;
  if (!g_cgTried) { __try { CgInstall(); } __except (FaultFilter("kill cam crew guard", GetExceptionInformation())) { g_cgOn = false; } }
  __try { KillCamTick(); }
  __except (FaultFilter("kill cam", GetExceptionInformation())) { g_kcFaults = -1; SafeKillCamOff(); }
}
static void (*g_blurUpdateOriginal)(void*, void*) = nullptr;
static void Hooked_BlurUpdate(void* self, void* method) {
  g_blurUpdateOriginal(self, method);
  g_kcBlurSeen = true;
  if (!g_ready || g_kcFactor >= 0.999f || g_kcFaults < 0 || !g_kc.ok) return;
  __try {
    float ts = *(float*)((char*)self + g_kc.blurTs);                   // what the game wants (0 when paused)
    if (ts > 0.01f) { KcSetFloat(g_kc.timeSet, ts * g_kcFactor); if (g_kcBaseFdt > 0) KcSetFloat(g_kc.fdtSet, (std::max)(0.002f, g_kcBaseFdt * g_kcFactor)); }
  }
  __except (FaultFilter("kill cam (game speed)", GetExceptionInformation())) { g_kcFaults = -1; SafeKillCamOff(); }
}

