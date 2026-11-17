// seeking shots and chaining shots (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- seeking shots ---------------------------------------------------------------------------------------------------
// Projectile.iterate (run each physics step by ProjectileManager.projectileSim) moves a shot by reading its managed
// velocity (+0x38) every step, so an edited velocity sticks. The game's own homing in iterate (ptype.assistFactor +0x60
// toward Projectile.target +0x30) only acts on targets more than 20 m up, i.e. planes; ships need steering of our own.
// Each frame, every live shot from a gun with fx.set.seek=<degrees per second> turns its velocity toward its target (the
// gun's target when it fired) by at most seek x dt degrees, keeping its speed. A limited turn rate gives the arc.
static int g_offPjTarget = -2, g_offPjVel = -1, g_offPjPos = -1, g_offPjTypeSeek = -1, g_offPtTimeScale = -1;
static int g_offPjDead = -2;                                          // Projectile.dead, looked up by ChainScan
// A shot's ProjectileType.timeScale: Projectile.iterate moves it and applies gravity by dt x timeScale (slow shells).
static float ShotTimeScale(void* pj) {
  void* pt = g_offPjTypeSeek >= 0 ? *(void**)((char*)pj + g_offPjTypeSeek) : nullptr;
  float ts = pt && g_offPtTimeScale >= 0 ? *(float*)((char*)pt + g_offPtTimeScale) : 1.0f;
  return ts > 0.01f ? ts : 1.0f;
}
static std::unordered_map<void*, float> g_seekCache;                 // mount -> degrees per second (0 = none), per battle
static void* g_seekEng = nullptr;
static LARGE_INTEGER g_seekLast = {};
static int g_seekSteered = 0;
static void* NearestOtherEnemyShip(const float* at, void* exclude, float radius, V3* posOut, bool ground);

// What kind of entity: surface = a CombatEntity that is not a Plane (ships and ground targets). Cached per class.
struct EntityKind { bool surface, ship; };
static EntityKind KindOf(void* ent) {
  static void *ceClass = nullptr, *planeClass = nullptr, *shipClass = nullptr;
  static std::unordered_map<void*, EntityKind> kinds;
  if (!ceClass) { ceClass = FindClass("Core", "CombatEntity"); planeClass = FindClass("Core", "Plane"); shipClass = FindClass("Core", "Ship"); }
  void* k = ent ? il2cpp_object_get_class(ent) : nullptr; if (!k) return { false, false };
  auto s = kinds.find(k);
  if (s == kinds.end()) {
    bool ce = false, plane = false, ship = false;
    for (void* p = k; p; p = il2cpp_class_get_parent(p)) { if (p == ceClass) ce = true; if (p == planeClass) plane = true; if (p == shipClass) ship = true; }
    s = kinds.emplace(k, EntityKind{ ce && !plane, ship }).first;
  }
  return s->second;
}

// Every live enemy ship and ground target (AA gun, MG nest), from the game's own entity list:
// Where a HashSet<T> of objects keeps its elements, read once per set class: the field names differ between class library
// versions. In this game's: _slots +0x18 (Slot[], 16 bytes each: hashCode, next, value +8) and _lastIndex (HashSet.Contains
// at 0x181C2CF50). il2cpp_class_array_element_size wants the ELEMENT class: given the Slot[] array class it answers 8, the
// size of a pointer, and stepping 8 bytes through 16-byte slots read hashCode/next as objects (the co-op fault in
// NetBoats on host and client, 2026-10-31).
struct HashSetLayout { int slots = -1, count = -1, val = -1, elem = 0; bool ok() const { return slots >= 0 && val >= 0 && elem >= val + 8; } };
static HashSetLayout HashSetLayoutOf(void* k) {
  static std::unordered_map<void*, HashSetLayout> layouts;
  auto L = layouts.find(k);
  if (L != layouts.end()) return L->second;
  HashSetLayout l;
  auto fieldGetType = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_field_get_type");
  auto classFromType = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_from_type");
  auto elementClass = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_get_element_class");
  auto elementSize = (int (*)(void*))GetProcAddress(g_ga, "il2cpp_class_array_element_size");
  void *fs = nullptr, *fl = nullptr, *fv = nullptr;
  for (const char* n : { "_slots", "m_slots", "_entries" }) if (k && !fs) fs = il2cpp_class_get_field_from_name(k, n);
  for (const char* n : { "_lastIndex", "m_lastIndex", "_count" }) if (k && !fl) fl = il2cpp_class_get_field_from_name(k, n);
  void* ak = fs && fieldGetType && classFromType ? classFromType(fieldGetType(fs)) : nullptr;
  void* ek = ak && elementClass ? elementClass(ak) : nullptr;
  for (const char* n : { "value", "Value" }) if (ek && !fv) fv = il2cpp_class_get_field_from_name(ek, n);
  if (fs) l.slots = (int)il2cpp_field_get_offset(fs);
  if (fl) l.count = (int)il2cpp_field_get_offset(fl);
  if (fv) l.val = (int)il2cpp_field_get_offset(fv) - 0x10;             // a value type's field offsets count the object header
  l.elem = ek && elementSize ? elementSize(ek) : 0;
  Log("HashSet layout of %s: slots +0x%X count +0x%X entry %d bytes value +%d%s", k ? il2cpp_class_get_name(k) : "?", l.slots, l.count, l.elem, l.val,
      l.ok() ? "" : " (incomplete; this set is not read)");
  layouts.emplace(k, l);
  return l;
}

// WorldEngagementManager.enemiesNearby walks LiveEntity.register (a static HashSet<LiveEntity>, read in its disassembly).
// Planes and submerged entities are left out. The set's slot array is read directly; its field names are looked up at
// run time, and if they are not found the callers fall back to the old ships-only physics search and the log says so.
// Read at most once every 10 ms, shared by the Unusual guns, seeking shots and chain leaps. A physics overlap found
// ships only (the Orbital's ring never launched against shore guns, David 2026-10-28), and near a shore its 256-collider
// buffer filled up with terrain: hand-aimed Homing shots at MG nests locked on 34 times in 914 searches (2026-10-30).
struct EnemyAt { void* ent; V3 pos; bool ship; };
static int g_enemyListState = 0, g_offLeAlive = -1;                  // state: 0 not read yet, 1 ready, -1 missing pieces
static std::vector<EnemyAt> g_enemiesUnder;                          // the same read's ships below the water (LiveEntity.underwater)
// Live enemies on the surface (shells, call-ins, the kill cam); LiveEnemiesAndSubs() adds the submerged ones for torpedoes.
static const std::vector<EnemyAt>& LiveEnemies() {
  static std::vector<EnemyAt> list; static LARGE_INTEGER readAt = {};
  static int offSlots = -1, offLast = -1, offVal = -1, elemSize = 0, offTeam = -1, offUnder = -1;
  static void *fReg = nullptr, *mTr = nullptr, *mPos = nullptr;
  if (readAt.QuadPart && PerfMs(readAt) < 10.0) return list;
  QueryPerformanceCounter(&readAt); list.clear(); g_enemiesUnder.clear();
  if (g_enemyListState == 0) {
    void* le = FindClass("Core", "LiveEntity"); fReg = le ? il2cpp_class_get_field_from_name(le, "register") : nullptr;
    void* set = nullptr; if (fReg) il2cpp_field_static_get_value(fReg, &set);
    if (fReg && !set) return list;                                     // no entities yet: try again next time
    g_enemyListState = -1;
    HashSetLayout hl = set ? HashSetLayoutOf(il2cpp_object_get_class(set)) : HashSetLayout();
    if (hl.ok()) { offSlots = hl.slots; offLast = hl.count; offVal = hl.val; elemSize = hl.elem; }
    offTeam = FieldOffset(le, "teamCode"); g_offLeAlive = FieldOffset(le, "_alive"); offUnder = FieldOffset(le, "underwater");
    void* comp = FindClass("UnityEngine", "Component"); mTr = comp ? FindMethod(comp, "get_transform", 0) : nullptr;
    void* t = FindClass("UnityEngine", "Transform"); mPos = t ? FindMethod(t, "get_position", 0) : nullptr;
    bool ok = offSlots >= 0 && offVal >= 0 && elemSize >= 8 && offTeam >= 0 && g_offLeAlive >= 0 && FindClass("Core", "CombatEntity") && mTr && mPos;
    Log("enemy list: %s (LiveEntity.register slots +0x%X count +0x%X entry %d bytes value +%d)", ok ? "ready" : "MISSING PIECES, ships-only physics search", offSlots, offLast, elemSize, offVal);
    if (ok) g_enemyListState = 1;
  }
  if (g_enemyListState < 0) return list;
  void* set = nullptr; il2cpp_field_static_get_value(fReg, &set); if (!set) return list;
  void* arr = *(void**)((char*)set + offSlots); if (!arr) return list;
  int n = ArrLen(arr);
  if (offLast >= 0) { int l = *(int*)((char*)set + offLast); if (l >= 0 && l < n) n = l; }
  void* me = PlayerCraft(); if (!me) return list;
  int myTeam = *(int*)((char*)me + offTeam);
  for (int i = 0; i < n; i++) {
    void* ent = *(void**)((char*)arr + 0x20 + (size_t)i * elemSize + offVal);
    if (!ent || ent == me || *(int*)((char*)ent + offTeam) == myTeam || !*(bool*)((char*)ent + g_offLeAlive)) continue;
    bool under = offUnder >= 0 && *(bool*)((char*)ent + offUnder);
    EntityKind kd = KindOf(ent); if (!kd.surface || (under && !kd.ship)) continue;
    void* tr = Invoke(mTr, ent); void* pb = tr ? Invoke(mPos, tr) : nullptr; if (!pb) continue;
    V3 q = *(V3*)((char*)pb + 0x10); if (q.y < -5000) continue;
    (under ? g_enemiesUnder : list).push_back({ ent, q, kd.ship });
  }
  return list;
}
static const std::vector<EnemyAt>& LiveEnemiesAndSubs() {
  static std::vector<EnemyAt> all;
  const std::vector<EnemyAt>& top = LiveEnemies();
  all.assign(top.begin(), top.end()); all.insert(all.end(), g_enemiesUnder.begin(), g_enemiesUnder.end());
  return all;
}

// The nearest live enemy ship or ground target within radius, other than exclude.
static void* NearestEnemyTarget(const float* at, float radius, V3* posOut, void* exclude = nullptr) {
  const std::vector<EnemyAt>& es = LiveEnemies();
  if (g_enemyListState < 0) return NearestOtherEnemyShip(at, exclude, radius, posOut, true);
  void* best = nullptr; float bestD = radius * radius;
  for (const EnemyAt& e : es) {
    if (e.ent == exclude) continue;
    float d = (e.pos.x - at[0]) * (e.pos.x - at[0]) + (e.pos.y - at[1]) * (e.pos.y - at[1]) + (e.pos.z - at[2]) * (e.pos.z - at[2]);
    if (d < bestD) { bestD = d; best = e.ent; *posOut = e.pos; }
  }
  return best;
}

// Where a seeking shot aims on a ground target (MG nest, AA gun, shore battery). GroundEntity.TerrainPlacement puts its
// origin on the terrain (a raycast down, plus GroundEntity.height; read in the disassembly 2026-10-30), so the ships'
// 3 m up flew homing shells over the sandbags and around in circles (David 2026-10-30: "some of the homing things do
// weird things against.. sandbags?"). It aims at the middle of the target's solid colliders instead, measured once per
// target per battle (ground targets do not move), or 1 m up if that fails. Logged once per kind of target.
static std::unordered_map<void*, V3> g_aimOffset;                    // ground target -> aim point minus origin, per battle
// The world box around a target's enabled, solid colliders (lo, hi); returns how many colliders it took (0: none, or the
// lookup failed).
static int ColliderBox(void* ent, float lo[3], float hi[3]) {
  static void *mInChildren = nullptr, *colType = nullptr, *mBounds = nullptr, *mTrigger = nullptr, *mEnabled = nullptr; static bool init = false;
  if (!init) {
    init = true;
    // the game's build stripped every GetComponentsInChildren(Type, ...) overload (it never found one, 2026-10-31); the
    // internal GameObject.GetComponentsInternal they all call is there
    void* go = FindClass("UnityEngine", "GameObject"); mInChildren = go ? FindMethod(go, "GetComponentsInternal", 6) : nullptr;
    void* col = FindClass("UnityEngine", "Collider"); colType = col ? il2cpp_type_get_object(il2cpp_class_get_type(col)) : nullptr;
    mBounds = col ? FindMethod(col, "get_bounds", 0) : nullptr; mTrigger = col ? FindMethod(col, "get_isTrigger", 0) : nullptr; mEnabled = col ? FindMethod(col, "get_enabled", 0) : nullptr;
    if (!mInChildren || !colType || !mBounds) Log("seeking shots: collider lookup MISSING PIECES (GetComponentsInternal %p get_bounds %p); ground targets get 1 m up, ships 3 m", mInChildren, mBounds);
  }
  int used = 0;
  for (int k = 0; k < 3; k++) { lo[k] = 1e30f; hi[k] = -1e30f; }
  if (mInChildren && colType && mBounds) {
    // (type, a Collider[] back, recursive, includeInactive, reverse, resultList): what GetComponentsInChildren(type) passes
    bool typed = true, recursive = true, inactive = false, reverse = false; void* a[6] = { colType, &typed, &recursive, &inactive, &reverse, nullptr };
    void* goEnt = Invoke(m_getGameObject, ent);
    void* arr = goEnt ? Invoke(mInChildren, goEnt, a) : nullptr; int n = ArrLen(arr);
    for (int i = 0; i < n && i < 64; i++) {
      void* c = ArrAt(arr, i); if (!c) continue;
      void* b = mTrigger ? Invoke(mTrigger, c) : nullptr; if (b && *(bool*)((char*)b + 0x10)) continue;   // shots pass through triggers
      b = mEnabled ? Invoke(mEnabled, c) : nullptr; if (b && !*(bool*)((char*)b + 0x10)) continue;
      void* bb = Invoke(mBounds, c); if (!bb) continue;
      const float* ce = (const float*)((char*)bb + 0x10); const float* ex = ce + 3;   // Bounds: center, extents
      for (int k = 0; k < 3; k++) { if (ce[k] - ex[k] < lo[k]) lo[k] = ce[k] - ex[k]; if (ce[k] + ex[k] > hi[k]) hi[k] = ce[k] + ex[k]; }
      used++;
    }
  }
  return used;
}
static V3 GroundAimOffset(void* ent, V3 origin) {
  auto it = g_aimOffset.find(ent); if (it != g_aimOffset.end()) return it->second;
  static std::set<std::string> logged;
  V3 off = { 0, 1, 0 };
  float lo[3], hi[3]; int used = ColliderBox(ent, lo, hi);
  float sx = hi[0] - lo[0], sy = hi[1] - lo[1], sz = hi[2] - lo[2];
  bool ok = used > 0 && sx < 80 && sy < 80 && sz < 80;                 // a box that big is not the target itself
  if (ok) off = { (lo[0] + hi[0]) / 2 - origin.x, (lo[1] + hi[1]) / 2 - origin.y, (lo[2] + hi[2]) / 2 - origin.z };
  g_aimOffset[ent] = off;
  std::string nm = NameOf(ent);
  if (logged.insert(nm).second) {
    if (ok) Log("seeking shots: shots at '%s' aim at the middle of its colliders, %.1f m above its origin and %.1f m to the side (%d colliders, box %.1f x %.1f x %.1f m)",
                nm.c_str(), off.y, sqrtf(off.x * off.x + off.z * off.z), used, sx, sy, sz);
    else Log("seeking shots: '%s' has %s; shots aim 1 m above its origin", nm.c_str(), used ? "a collider box over 80 m" : "no solid colliders");
  }
  return off;
}
// How high above its origin a seeking shot aims on a ship. It was a fixed 3 m, which flew homing shells over small and
// shrunken hulls (David 2026-11-02: The Ferryman 'weren't able to hit downsized enemies'; Smol Elites are 0.55 tall,
// and its last battle had 21 shells circle out). Now 40% up the box of the ship's solid colliders, kept between 0.5 and
// 3 m. Only the height is used: ships turn, the box is measured in world space. Measured again after 20 s, because a
// pooled ship can come back resized by an Elite trait. 3 m if it has no colliders. Logged once per kind and size.
struct ShipAim { float up; DWORD at; };
static std::unordered_map<void*, ShipAim> g_shipAim;                 // per battle
static float ShipAimHeight(void* ent, V3 origin) {
  DWORD now = GetTickCount();
  auto it = g_shipAim.find(ent); if (it != g_shipAim.end() && now - it->second.at < 20000) return it->second.up;
  static std::set<std::string> logged;
  float lo[3], hi[3]; int used = ColliderBox(ent, lo, hi);
  float h = hi[1] - lo[1], up = 3.0f;
  bool ok = used > 0 && h > 0.1f && h < 120 && hi[0] - lo[0] < 400 && hi[2] - lo[2] < 400;
  if (ok) { up = lo[1] + 0.4f * h - origin.y; if (up < 0.5f) up = 0.5f; if (up > 3.0f) up = 3.0f; }
  g_shipAim[ent] = { up, now };
  char key[160]; snprintf(key, sizeof key, "%s|%.1f", NameOf(ent).c_str(), ok ? h : -1.0f);
  if (logged.insert(key).second) {
    if (ok) Log("seeking shots: shots at ship '%s' aim %.1f m above its origin (%d colliders, %.1f m from bottom to top, bottom %.1f m from the origin)",
                NameOf(ent).c_str(), up, used, h, lo[1] - origin.y);
    else Log("seeking shots: ship '%s' has %s; shots aim 3 m above its origin", NameOf(ent).c_str(), used ? "a collider box too big to be the ship" : "no solid colliders");
  }
  return up;
}

// Per seeking shot: its hand-aimed lock, and how far it has turned in all. A pooled shell fired again (another gun, or
// Projectile.alive higher than last seen) starts over.
// near: closest it came to its aim point; counted/ended: in the per-gun summary as steered, and as ended or circled out.
struct SeekShot { void* target; void* handle; void* from; float alive; unsigned lastTry; float turned; float nearest; bool counted, ended; bool climbing = false, topped = false; };
static std::unordered_map<void*, SeekShot> g_seekShot;               // projectile -> its state, per battle
// Per gun, per battle (2026-11-02, to see how often homing shells actually reach what they steer at): shells steered, how
// they ended (on something within 25 m of their aim point, or farther off), and how many circled out.
struct SeekGunStat { int steered = 0, nearHit = 0, farHit = 0, circled = 0, apex = 0; double missSum = 0; };   // apex: flew up to the top of their arc first
static std::map<std::string, SeekGunStat> g_seekGun;
static unsigned g_seekFrame = 0;
static int g_seekLocked = 0, g_seekLockedShore = 0, g_seekSearches = 0, g_seekLost = 0, g_seekCircled = 0, g_seekDiag = 0;
static void ClearSeekShots() { for (auto& e : g_seekShot) Unpin(e.second.handle); g_seekShot.clear(); }
static SeekShot& SeekStateOf(void* pj, void* mount) {
  float alive = g_offPjAlive >= 0 ? *(float*)((char*)pj + g_offPjAlive) : 0.0f;
  auto it = g_seekShot.find(pj);
  if (it != g_seekShot.end() && (it->second.from != mount || alive > it->second.alive + 1e-4f)) { Unpin(it->second.handle); g_seekShot.erase(it); it = g_seekShot.end(); }
  if (it == g_seekShot.end()) it = g_seekShot.emplace(pj, SeekShot{ nullptr, nullptr, mount, alive, 0, 0.0f, 1e9f, false, false }).first;
  it->second.alive = alive;
  return it->second;
}

// A hand-aimed shot (the utility slot, aimed with the mouse) carries no target, so Homing had nothing to steer for
// (David 2026-10-29: "the 37mm that has homing on it ... didn't look like it was homing"). Such a shot locks onto the
// enemy its unguided flight would pass closest to: for each live enemy ahead, the shot's ballistic height by the time
// it gets that far, against the enemy's middle. Sideways misses count in full, up and down at half, and the miss must
// be under 25 m plus 6% of the distance. It looks again every 15 frames until it finds one, at most 6 searches a frame.
// (Until 2026-10-30: the enemy nearest a point 250 m ahead, which could also pick the MG nest next to the one aimed at.)
// The lock goes into Projectile.target, so Chaining sees it too, and a GC handle holds the target while the shot lives.
static void* SeekAcquire(void* pj, SeekShot& S, const float* ps, const float* v, int* budget) {
  if (S.target) return S.target;
  if ((S.lastTry && g_seekFrame - S.lastTry < 15) || *budget <= 0) return nullptr;
  (*budget)--; S.lastTry = g_seekFrame; g_seekSearches++;
  float vh = sqrtf(v[0] * v[0] + v[2] * v[2]);
  // Where the shell comes down to the water is where the player aimed it (David: 'home near where the player targetted,
  // not whatever is closest to the boat'). The game does not lob mortar rounds: it drops them from about 4.5 km straight
  // above the spot they will hit, falling at 240 m/s (log 2026-11-03: shell at -4137,4503,3183 moving 0,-240,0), so the
  // point under the shell is the aim point and the old arc test (vh < 1: give up) never found a target. GunMount.lastTarget
  // is not the aim point (it was 1.1 km off). Projectile.iterate (read 2026-11-03) moves a shell by velocity x dt x
  // ProjectileType.timeScale and adds Physics.gravity x dt x timeScale to its velocity, no drag, so the landing point does
  // not depend on timeScale.
  float disc = v[1] * v[1] + 19.62f * (ps[1] > 0 ? ps[1] : 0.0f), tl = (v[1] + sqrtf(disc)) / 9.81f;
  if (tl < 0) tl = 0;
  V3 aim = { ps[0] + v[0] * tl, 0, ps[2] + v[2] * tl };
  float aimDist = vh * tl;                                             // how far out it lands
  bool steep = vh < fabsf(v[1]);                                       // steeper than 45 deg: judged by where it comes down
  float hx = vh >= 1 ? v[0] / vh : 0, hz = vh >= 1 ? v[2] / vh : 0;
  void* best = nullptr; bool ship = false;
  const std::vector<EnemyAt>& es = LiveEnemies();
  if (g_enemyListState < 0) {                                          // no enemy list: the nearest to the aim point, or 250 m ahead
    float sp = sqrtf(vh * vh + v[1] * v[1]); if (sp < 1) sp = 1;
    float at[3] = { ps[0] + v[0] / sp * 250.0f, ps[1] + v[1] / sp * 250.0f, ps[2] + v[2] / sp * 250.0f };
    if (steep) { at[0] = aim.x; at[1] = 0; at[2] = aim.z; }
    V3 q; best = NearestOtherEnemyShip(at, nullptr, steep ? 80.0f : 300.0f, &q, true); ship = best && KindOf(best).ship;
  } else {
    // A steep shell (mortars) scores an enemy by its distance from where the shell comes down; any shell with some
    // sideways speed also by how close its arc passes the enemy (flat fire, whose landing point lies beyond the target).
    float bestScore = 1.0f, diagAim = 1e9f, diagPass = 1e9f; void* diagAimE = nullptr;
    for (const EnemyAt& e : es) {
      float dx = e.pos.x - aim.x, dz = e.pos.z - aim.z, d = sqrtf(dx * dx + dz * dz);
      if (d < diagAim) { diagAim = d; diagAimE = e.ent; }
      float score = steep ? d / (80.0f + 0.04f * aimDist) : 1e9f;
      if (vh >= 1) {
        float rx = e.pos.x - ps[0], rz = e.pos.z - ps[2], along = rx * hx + rz * hz;
        if (along >= 5.0f && along <= 4000.0f) {
          float side = rx * hz - rz * hx, t = along / vh;
          float up = (ps[1] + v[1] * t - 4.905f * t * t - (e.pos.y + (e.ship ? 3.0f : 1.0f))) * 0.5f;
          float pass = sqrtf(side * side + up * up) / (25.0f + 0.06f * along);
          if (pass < diagPass) diagPass = pass;
          if (pass < score) score = pass;
        }
      }
      if (score < bestScore) { bestScore = score; best = e.ent; ship = e.ship; }
    }
    if (g_seekDiag < 4) {                                              // the first few searches of a battle, to see why one fails
      g_seekDiag++;
      DLog("seeking shots: search by '%s': shell at %.0f,%.0f,%.0f moving %.1f,%.1f,%.1f; comes down at %.0f,%.0f (%.0f m out); %d enemies, nearest to that point %s %.0f m off; best pass score %.2f; %s",
          StripTags(NameOf(S.from)).c_str(), ps[0], ps[1], ps[2], v[0], v[1], v[2], aim.x, aim.z, aimDist, (int)es.size(),
          diagAimE ? NameOf(diagAimE).c_str() : "none", diagAim, diagPass, best ? ("locked onto " + NameOf(best)).c_str() : "no lock");
    }
  }
  if (!best) return nullptr;
  S.target = best; S.handle = Pin(best);
  SetRef(pj, (char*)pj + g_offPjTarget, best);
  if (!ship) g_seekLockedShore++;
  if (g_seekLocked++ == 0) DLog("seeking shots: a hand-aimed shot from '%s' locked onto %s", StripTags(NameOf(S.from)).c_str(), NameOf(best).c_str());
  return best;
}

static float GameTimeScale();
static void SeekScan(void* pm) {
  if (g_offPjTarget == -2) {
    void* pk = FindClass("Core", "Projectile");
    g_offPjTarget = FieldOffset(pk, "target"); g_offPjVel = FieldOffset(pk, "velocity"); g_offPjPos = FieldOffset(pk, "posi"); g_offPjTypeSeek = FieldOffset(pk, "ptype");
    g_offPtTimeScale = FieldOffset("Core", "ProjectileType", "timeScale");
    void* le = FindClass("Core", "LiveEntity"); void* fa = le ? il2cpp_class_get_field_from_name(le, "_alive") : nullptr;
    if (fa) g_offLeAlive = (int)il2cpp_field_get_offset(fa);
    if (g_offPjTarget < 0 || g_offPjVel < 0 || g_offPjPos < 0 || g_offPjTypeSeek < 0 || g_offPtTimeScale < 0 || g_offLeAlive < 0)
      Log("seeking shots: MISSING PIECES (target +0x%X, velocity +0x%X, posi +0x%X, ptype +0x%X, timeScale +0x%X, target alive +0x%X)", g_offPjTarget, g_offPjVel, g_offPjPos, g_offPjTypeSeek, g_offPtTimeScale, g_offLeAlive);
    else DLog("seeking shots: target +0x%X, velocity +0x%X, posi +0x%X, ptype +0x%X, timeScale +0x%X, target alive +0x%X", g_offPjTarget, g_offPjVel, g_offPjPos, g_offPjTypeSeek, g_offPtTimeScale, g_offLeAlive);
    // once: which stock projectile types use the game's own plane homing (assistFactor), and how strongly
    void* ptk = FindClass("Core", "ProjectileType"); void* fa2 = ptk ? il2cpp_class_get_field_from_name(ptk, "assistFactor") : nullptr;
    void* res = FindClass("UnityEngine", "Resources"); void* findAll = res ? FindMethod(res, "FindObjectsOfTypeAll", 1, "System.Type") : nullptr;
    if (fa2 && findAll) {
      void* a[1] = { il2cpp_type_get_object(il2cpp_class_get_type(ptk)) };
      void* arr = Invoke(findAll, nullptr, a); int n = ArrLen(arr); std::string list;
      for (int i = 0; i < n; i++) { void* pt = ArrAt(arr, i); float af = pt ? *(float*)((char*)pt + il2cpp_field_get_offset(fa2)) : 0; if (af > 0) { char b[96]; snprintf(b, sizeof b, "%s=%.2f; ", NameOf(pt).c_str(), af); list += b; } }
      DLog("seeking shots: stock assistFactor values (%d projectile types): %s", n, list.empty() ? "none above 0" : list.c_str());
    }
  }
  if (g_offPjTarget < 0 || g_offPjVel < 0 || g_offPjPos < 0 || g_offPmLive < 0 || g_offPjFrom < 0) return;
  void* eng = EngagementInstance();
  if (eng != g_seekEng) {
    if (g_seekEng && (g_seekSteered || g_seekSearches))
      Log("seeking shots: shots were steered in %d frames last battle; hand-aimed shots: %d target searches, %d locked on (%d on shore targets), "
          "%d lost a target that sank; %d shots stopped steering after turning a full circle", g_seekSteered, g_seekSearches, g_seekLocked, g_seekLockedShore, g_seekLost, g_seekCircled);
    for (auto& g : g_seekGun)
      Log("seeking shots by gun, last battle: '%s' steered %d shells (%d of them only from the top of their arc): %d ended within 25 m of their aim point, %d farther off (%.0f m on average), %d circled out; the rest flew out of range or were still flying",
          g.first.c_str(), g.second.steered, g.second.apex, g.second.nearHit, g.second.farHit, g.second.farHit ? g.second.missSum / g.second.farHit : 0.0, g.second.circled);
    g_seekGun.clear(); g_shipAim.clear();
    g_seekEng = eng; g_seekCache.clear(); g_seekSteered = 0; g_seekSearches = g_seekLocked = g_seekLockedShore = g_seekLost = g_seekCircled = g_seekDiag = 0; ClearSeekShots(); g_aimOffset.clear();
  }
  g_seekFrame++;
  int searchBudget = 6;
  LARGE_INTEGER now; QueryPerformanceCounter(&now);
  double dt = g_seekLast.QuadPart ? PerfMs(g_seekLast) / 1000.0 : 0; g_seekLast = now;
  if (!eng || dt <= 0 || dt > 0.25) return;                            // first frame, or a long pause (loading)
  dt *= GameTimeScale();                                               // game time: the kill cam's slow motion slows the steering too
  if (dt <= 0) return;
  static void *mTransform = nullptr, *mPosition = nullptr;
  if (!mTransform) { void* c = FindClass("UnityEngine", "Component"); mTransform = c ? FindMethod(c, "get_transform", 0) : nullptr; void* t = FindClass("UnityEngine", "Transform"); mPosition = t ? FindMethod(t, "get_position", 0) : nullptr; }
  if (!mTransform || !mPosition) return;
  // aim where the target will be when the shot arrives, using LiveEntity.realtimeVelocity; slow homing shells otherwise
  // dropped into the water behind ships moving away (David)
  static int offTVel = -2;
  if (offTVel == -2) offTVel = FieldOffset("Core", "LiveEntity", "realtimeVelocity");
  void* live = *(void**)((char*)pm + g_offPmLive);
  std::unordered_map<void*, V3> aimAt;                                 // one aim point per target per frame
  bool steeredAny = false;
  for (int i = 0; i < ListCount(live); i++) {
    void* pj = ListAt(live, i); if (!pj) continue;
    void* mount = *(void**)((char*)pj + g_offPjFrom); if (!mount) continue;
    if (!g_coopVisuals && IsCopyShot(pj)) continue;                    // co-op: other players' shots fly as the game sent them
    auto c = g_seekCache.find(mount);
    if (c == g_seekCache.end()) { const Item* it = ItemOfMount(mount); c = g_seekCache.emplace(mount, it ? (float)ItemFx(*it, "seek") : 0.0f).first; }
    float rate = c->second;
    if (g_chainBoost.count(pj) && rate < 360.0f) rate = 360.0f;
    if (rate <= 0) continue;
    if (g_offPjDead >= 0 && *(bool*)((char*)pj + g_offPjDead)) {       // it has hit something
      auto ds = g_seekShot.find(pj);
      if (ds != g_seekShot.end() && ds->second.counted && !ds->second.ended && ds->second.from == mount) {
        ds->second.ended = true;
        if (const Item* it = ItemOfMount(mount)) {
          SeekGunStat& g = g_seekGun[it->name];
          if (ds->second.nearest < 25) g.nearHit++; else { g.farHit++; g.missSum += ds->second.nearest; }
        }
      }
      continue;
    }
    SeekShot& S = SeekStateOf(pj, mount);
    // A shot that missed kept chasing its target in circles until it timed out (a weird sight, David 2026-10-30). After
    // a full circle of turning it flies on: enough for a chain leap to come back around, or for one more pass.
    if (S.turned > 6.2832f) continue;
    float* v = (float*)((char*)pj + g_offPjVel); const float* ps = (const float*)((char*)pj + g_offPjPos);
    void* target = *(void**)((char*)pj + g_offPjTarget);
    if (!target) target = SeekAcquire(pj, S, ps, v, &searchBudget);
    if (!target) continue;
    if (g_offLeAlive >= 0 && !*(bool*)((char*)target + g_offLeAlive)) {  // sank or was destroyed on the way
      if (target == S.target) {                                        // a hand-aimed shot looks for another
        Unpin(S.handle); S.handle = nullptr; S.target = nullptr; S.lastTry = 0;
        SetRef(pj, (char*)pj + g_offPjTarget, nullptr); g_seekLost++;
      }
      continue;                                                        // a gun's shot flies on
    }
    auto tp = aimAt.find(target);
    if (tp == aimAt.end()) {
      void* tr = Invoke(mTransform, target); void* pos = tr ? Invoke(mPosition, tr) : nullptr;
      if (!pos) continue;                                              // target destroyed: fly on
      V3 o = *(V3*)((char*)pos + 0x10), a = o;
      EntityKind kd = KindOf(target);
      if (kd.surface && !kd.ship) { V3 d = GroundAimOffset(target, o); a.x += d.x; a.y += d.y; a.z += d.z; }
      else if (kd.ship) a.y += ShipAimHeight(target, o);               // measured from the hull, at most 3 m
      else a.y += 3.0f;                                                // planes: 3 m above the origin
      tp = aimAt.emplace(target, a).first;
    }
    float sp = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    float ts = ShotTimeScale(pj);                                      // slow shells cover sp x ts metres a second
    float ax = tp->second.x, ay = tp->second.y, az = tp->second.z;
    if (offTVel >= 0 && sp > 1) {
      const float* tv = (const float*)((char*)target + offTVel);
      float d0 = sqrtf((ax - ps[0]) * (ax - ps[0]) + (ay - ps[1]) * (ay - ps[1]) + (az - ps[2]) * (az - ps[2]));
      float lead = d0 / (sp * ts); if (lead > 8) lead = 8;
      ax += tv[0] * lead; az += tv[2] * lead;
    }
    float dx = ax - ps[0], dy = ay - ps[1], dz = az - ps[2];
    float dl = sqrtf(dx * dx + dy * dy + dz * dz);
    { float rx = tp->second.x - ps[0], ry = tp->second.y - ps[1], rz = tp->second.z - ps[2], rd = sqrtf(rx * rx + ry * ry + rz * rz);
      if (rd < S.nearest) S.nearest = rd; }
    if (dl < 0.5f || sp < 0.1f) continue;
    // A lobbed shell steers only from the top of its arc (David 2026-11-12, Guided Mousetrap): steering from the launcher
    // with gravity cancelled bent its steep climb into a low skim at the hull's aim point, 1-3 m up, and the waves took
    // many of them. While it still climbs steeper than 10 degrees it flies its own ballistic arc. Chain leaps (thrown up
    // and away on purpose) steer at once; flat shots never climb that steeply, so nothing changes for them. Only a
    // climb steeper than the line to the aim point is held: a homing AA shot climbing toward a plane steers as before.
    if (!S.topped && !g_chainBoost.count(pj)) {
      if (v[1] > 0.17f * sp && v[1] / sp > dy / dl + 0.05f) { S.climbing = true; continue; }
      S.topped = true;
      if (S.climbing) if (const Item* it = ItemOfMount(mount)) g_seekGun[it->name].apex++;
    }
    if (!S.counted) { S.counted = true; if (const Item* it = ItemOfMount(mount)) g_seekGun[it->name].steered++; }   // only shells that steer
    // Close in, a shell may turn hard: at The Ferryman's 90 deg/s and about 100 m/s a shell that just missed a small hull
    // needed a 65 m circle to come back, and gave up after one (2026-11-02). Within 60 m, or 0.6 s of flight for a fast
    // shell, it turns at up to 360 deg/s (as chain leaps always do), which tightens that circle to about 16 m.
    float closeIn = sp * ts * 0.6f; if (closeIn < 60) closeIn = 60;
    if (dl < closeIn && rate < 360.0f) rate = 360.0f;
    v[1] += 9.81f * (float)dt * ts;                                    // homing shots are powered: gravity is cancelled out
    float ux = v[0] / sp, uy = v[1] / sp, uz = v[2] / sp, tx = dx / dl, ty = dy / dl, tz = dz / dl;
    float dot = ux * tx + uy * ty + uz * tz; if (dot > 1) dot = 1; if (dot < -1) dot = -1;
    float ang = acosf(dot), maxA = rate * 3.14159265f / 180.0f * (float)dt;
    if (ang < 1e-4f) continue;
    float f = ang <= maxA ? 1.0f : maxA / ang;                         // fraction of the way to turn this frame
    // rotate u toward t by f*ang inside their plane: w = t - u*dot (perpendicular part), new = u cos(a) + w/|w| sin(a)
    float wx = tx - ux * dot, wy = ty - uy * dot, wz = tz - uz * dot, wl = sqrtf(wx * wx + wy * wy + wz * wz);
    if (wl < 1e-5f) continue;                                          // straight away from the target: no defined plane
    float a = f * ang, ca = cosf(a), sa = sinf(a);
    v[0] = (ux * ca + wx / wl * sa) * sp; v[1] = (uy * ca + wy / wl * sa) * sp; v[2] = (uz * ca + wz / wl * sa) * sp;
    S.turned += a;
    if (S.turned > 6.2832f) { g_seekCircled++; if (const Item* it = ItemOfMount(mount)) g_seekGun[it->name].circled++; S.ended = true; }
    steeredAny = true;
  }
  if (steeredAny) g_seekSteered++;
}
static int g_seekFaults = 0;

// ground: also ground targets (AA guns, MG nests): any CombatEntity that is not a plane
static void* NearestOtherEnemyShip(const float* at, void* exclude, float radius, V3* posOut, bool ground = false) {
  static void *mOverlap = nullptr, *mInParent = nullptr, *ceType = nullptr, *shipClass = nullptr, *planeClass = nullptr, *mCraft = nullptr, *mTr = nullptr, *mPos = nullptr, *buf = nullptr;
  static int offTeam = -1, offAlive = -1;
  if (!mOverlap) {
    void* ph = FindClass("UnityEngine", "Physics"); mOverlap = ph ? FindMethod(ph, "OverlapSphereNonAlloc", 3, "UnityEngine.Vector3", "System.Single") : nullptr;
    void* comp = FindClass("UnityEngine", "Component"); mInParent = comp ? FindMethod(comp, "GetComponentInParent", 2, "System.Type", "System.Boolean") : nullptr;
    mTr = comp ? FindMethod(comp, "get_transform", 0) : nullptr;
    void* t = FindClass("UnityEngine", "Transform"); mPos = t ? FindMethod(t, "get_position", 0) : nullptr;
    void* ce = FindClass("Core", "CombatEntity"); ceType = ce ? il2cpp_type_get_object(il2cpp_class_get_type(ce)) : nullptr;
    shipClass = FindClass("Core", "Ship"); planeClass = FindClass("Core", "Plane");
    void* le = FindClass("Core", "LiveEntity");
    offTeam = FieldOffset(le, "teamCode"); offAlive = FieldOffset(le, "_alive");
    void* pc = FindClass("Core.Player", "Player"); mCraft = pc ? FindMethod(pc, "get_playerCraft", 0) : nullptr;
    void* col = FindClass("UnityEngine", "Collider");
    static void* (*arrayNew)(void*, size_t) = (void* (*)(void*, size_t))GetProcAddress(g_ga, "il2cpp_array_new");
    buf = col && arrayNew ? arrayNew(col, 256) : nullptr; Pin(buf);
    Log("chaining shots: target search %s (OverlapSphereNonAlloc %p GetComponentInParent %p team +0x%X alive +0x%X)",
        mOverlap && mInParent && ceType && buf && offTeam >= 0 ? "ready" : "MISSING PIECES", mOverlap, mInParent, offTeam, offAlive);
  }
  if (!mOverlap || !mInParent || !ceType || !buf || offTeam < 0 || !mCraft) return nullptr;
  void* me = Invoke(mCraft, nullptr); if (!me) return nullptr;
  int myTeam = *(int*)((char*)me + offTeam);
  V3 c = { at[0], at[1], at[2] };
  void* a[3] = { &c, &radius, buf };
  void* r = Invoke(mOverlap, nullptr, a);
  int n = r ? *(int*)((char*)r + 0x10) : 0; if (n > 256) n = 256;
  void* best = nullptr; float bestD = 1e30f;
  std::vector<void*> checked;
  for (int i = 0; i < n; i++) {
    void* coll = ArrAt(buf, i); if (!coll) continue;
    bool inactive = false; void* g[2] = { ceType, &inactive };
    void* ent = Invoke(mInParent, coll, g);
    if (!ent || ent == exclude || ent == me || std::find(checked.begin(), checked.end(), ent) != checked.end()) continue;
    checked.push_back(ent);
    if (*(int*)((char*)ent + offTeam) == myTeam) continue;
    if (offAlive >= 0 && !*(bool*)((char*)ent + offAlive)) continue;
    bool ship = false, plane = false;
    for (void* k = il2cpp_object_get_class(ent); k; k = il2cpp_class_get_parent(k)) { if (k == shipClass) ship = true; if (k == planeClass) plane = true; }
    if (ground ? plane : !ship) continue;                              // the overlap only finds CombatEntities
    void* tr = Invoke(mTr, ent); void* ps = tr ? Invoke(mPos, tr) : nullptr; if (!ps) continue;
    V3 q = *(V3*)((char*)ps + 0x10);
    float d = (q.x - at[0]) * (q.x - at[0]) + (q.y - at[1]) * (q.y - at[1]) + (q.z - at[2]) * (q.z - at[2]);
    if (d < bestD) { bestD = d; best = ent; *posOut = q; }
  }
  return best;
}

// ---- chaining shots: a shot that hits comes back for more -------------------------------------------------------------
// fx.set.chain=N: when a shot from such a gun dies within 30 m of its target, a new shot of the same type is fired from
// the impact point, at 70% of its damage, up to N times. If the gun's shots are also seeking another target, the new
// shot pierces on toward that one; otherwise it is thrown up and away and its seek steering brings it back around into
// the same target. Read first (2026-10-26): ProjectileManager.fetchProjectile takes a shot from the pool and adds it to
// projectiles_live; Projectile.shoot(type, start, velocity, gun) calls reset() (clears target +0x30, dead +0x68) but not
// damageMultiplier (+0x84), so that is set here. A dying shot's target is cleared by the game, so each chain shot's
// target, position and velocity are remembered while it lives. Hits are resolved in ProjectileManager.Update; new shots
// are fired in LateUpdate, after that and before the next physics step schedules its sphere casts.
// copy: from another player's shot (co-op), so its leaps are copies too: shown, never dealing damage
// leap: 0 for the gun's own shot, 1 for its first leap, and so on (the effect streaks' log)
struct ChainState { void* target; void* mount; void* ptype; float pos[3], vel[3]; float mult; int hops; float chance, dmg, decay; float alive; unsigned frame; bool copy; int leap; };   // alive: Projectile.alive last seen (see ShotSig)
static void SafeChainStreak(const ChainState& cs, void* next, const V3& np);   // the leap's streak (effect_streaks), with its own fault guard
struct ChainCfg { int hops; float chance, dmg, decay; };              // hops = safety cap; chance x decay for each further hop
static std::unordered_map<void*, ChainState> g_chain;                // live chain shot -> state
static std::unordered_map<void*, ChainCfg> g_chainCache;             // mount -> chain settings (hops 0 = none), per battle
static void* g_chainEng = nullptr;
static int g_chainFired = 0, g_chainFaults = 0, g_offPjType = -1, g_chainAimed = 0, g_chainFragSkips = 0;
static std::unordered_map<void*, std::set<void*>> g_chainFragTypes;  // mount -> its shells' fragment types, per battle
// A ship's size in metres: max(EntityType.estimatedLength, estimatedWidth) of CombatEntity.parameters, 0 if unknown.
static float EntitySize(void* ce) {
  static int offParams = -2, offLen = -1, offWid = -1; static void* ceClass = nullptr;
  if (offParams == -2) {
    ceClass = FindClass("Core", "CombatEntity"); void* et = FindClass("Core", "EntityType");
    offParams = FieldOffset(ceClass, "parameters"); offLen = FieldOffset(et, "estimatedLength"); offWid = FieldOffset(et, "estimatedWidth");
  }
  bool combat = false;                                                 // a shot's target can be any LiveEntity
  for (void* k = ce ? il2cpp_object_get_class(ce) : nullptr; k && !combat; k = il2cpp_class_get_parent(k)) combat = k == ceClass;
  void* et = combat && offParams >= 0 ? *(void**)((char*)ce + offParams) : nullptr;
  if (!et || offLen < 0 || offWid < 0) return 0;
  float l = *(float*)((char*)et + offLen), w = *(float*)((char*)et + offWid);
  return l > w ? l : w;
}
static void ChainScan(void* pm) {
  if (g_offPjDead == -2) {
    void* pk = FindClass("Core", "Projectile");
    g_offPjDead = FieldOffset(pk, "dead"); g_offPjType = FieldOffset(pk, "ptype");
  }
  static int offFrag = -2;
  if (offFrag == -2) offFrag = FieldOffset("Core", "ProjectileType", "fragment");
  if (g_offPjDead < 0 || g_offPjType < 0 || g_offPjTarget < 0 || g_offPjVel < 0 || g_offPjPos < 0 || g_offPmLive < 0 || g_offPjFrom < 0 || g_offPjDmg < 0) return;
  void* eng = EngagementInstance();
  if (eng != g_chainEng) {
    if (g_chainEng && (g_chainFired || g_chainFragSkips)) Log("chaining shots: %d follow-up shots fired last battle (%d from hand-aimed shots; %d fragments not chained)", g_chainFired, g_chainAimed, g_chainFragSkips);
    g_chainEng = eng; g_chain.clear(); g_chainBoost.clear(); g_chainCache.clear(); g_chainFragTypes.clear(); g_chainFired = g_chainAimed = g_chainFragSkips = 0;
  }
  if (!eng) return;
  static void *mFetch = nullptr, *mShoot = nullptr, *mTransform = nullptr, *mPosition = nullptr;
  if (!mFetch) {
    void* pmk = FindClass("Core", "ProjectileManager"); void* pk = FindClass("Core", "Projectile");
    mFetch = pmk ? FindMethod(pmk, "fetchProjectile", 0) : nullptr; mShoot = pk ? FindMethod(pk, "shoot", 4) : nullptr;
    void* c = FindClass("UnityEngine", "Component"); mTransform = c ? FindMethod(c, "get_transform", 0) : nullptr;
    void* t = FindClass("UnityEngine", "Transform"); mPosition = t ? FindMethod(t, "get_position", 0) : nullptr;
    Log("chaining shots: fetchProjectile %p, Projectile.shoot %p", mFetch, mShoot);
  }
  if (!mFetch || !mShoot || !mTransform || !mPosition) return;
  auto posOf = [&](void* ent, V3* out) { void* tr = ent ? Invoke(mTransform, ent) : nullptr; void* ps = tr ? Invoke(mPosition, tr) : nullptr; if (!ps) return false; *out = *(V3*)((char*)ps + 0x10); return true; };
  void* live = *(void**)((char*)pm + g_offPmLive);
  std::vector<void*> targets;                                           // what the chain guns are shooting at right now
  std::vector<ChainState> spawn;
  static unsigned chainFrame = 0; ++chainFrame;                      // tracked shots are stamped, not every live shell collected
  for (int i = 0; i < ListCount(live); i++) {
    void* pj = ListAt(live, i); if (!pj) continue;
    bool dead = *(bool*)((char*)pj + g_offPjDead);
    auto st = g_chain.find(pj);
    float alive = g_offPjAlive >= 0 ? *(float*)((char*)pj + g_offPjAlive) : 0.0f;
    if (st != g_chain.end() && !SameShot({ st->second.mount, st->second.alive }, *(void**)((char*)pj + g_offPjFrom), alive)) {
      g_chainBoost.erase(pj); g_chain.erase(st); st = g_chain.end();   // a pooled shell fired again: not the tracked shot
    }
    if (dead) {
      if (st == g_chain.end()) continue;
      ChainState cs = st->second; g_chain.erase(st); g_chainBoost.erase(pj);
      const float* dp = (const float*)((char*)pj + g_offPjPos);
      bool aimed = false;                                               // a hand-aimed shot (no Projectile.target): the enemy it died next to
      if (!cs.target) { V3 q; cs.target = NearestEnemyTarget(dp, 30.0f, &q); aimed = cs.target != nullptr; }
      V3 tp; if (!cs.target || !posOf(cs.target, &tp)) continue;       // target gone: the chain ends
      float dx = tp.x - dp[0], dy = tp.y - dp[1], dz = tp.z - dp[2];
      // died far from its target: a miss, no chain. The distance is to the ship's centre, so a long ship hit at the bow
      // or stern counts too: half its length plus 10 m, at least 30 m (a flat 30 m missed hits on big ships)
      float reach = EntitySize(cs.target) / 2 + 10.0f; if (reach < 30.0f) reach = 30.0f;
      if (dx * dx + dy * dy + dz * dz > reach * reach) continue;
      if (cs.hops <= 0 || Rand01() >= cs.chance) continue;             // only some hits chain
      if (aimed) { if (g_chainAimed++ < 3) Log("chaining shots: a hand-aimed shot died near '%s'; it chains from there", NameOf(cs.target).c_str()); else DLog("chaining shots: hand-aimed chain root %d", g_chainAimed); }
      cs.pos[0] = dp[0]; cs.pos[1] = dp[1]; cs.pos[2] = dp[2];
      spawn.push_back(cs);
      continue;
    }
    void* mount = *(void**)((char*)pj + g_offPjFrom);
    if (st == g_chain.end()) {                                         // a live shot not tracked yet: from a chain gun?
      if (!mount) continue;
      bool copy = IsCopyShot(pj);
      if (copy && !g_coopVisuals) continue;
      auto c = g_chainCache.find(mount);
      if (c == g_chainCache.end()) { const Item* it = ItemOfMount(mount); ChainCfg cfg = { 0, 1.0f, 0.5f, 1.0f };
        if (it) { cfg.hops = (int)ItemFx(*it, "chain"); double ch = ItemFx(*it, "chainChance"), dm = ItemFx(*it, "chainDamage"), dc = ItemFx(*it, "chainDecay");
                  if (ch > 0) cfg.chance = (float)ch; if (dm > 0) cfg.dmg = (float)dm; if (dc > 0) cfg.decay = (float)dc; }
        c = g_chainCache.emplace(mount, cfg).first; }
      if (c->second.hops <= 0) continue;
      void* pt = *(void**)((char*)pj + g_offPjType);                   // fragments come from the parent's mount: never new chain roots
      std::set<void*>& ft = g_chainFragTypes[mount];
      if (pt && ft.count(pt)) { g_chainFragSkips++; continue; }
      if (pt && offFrag >= 0) { void* fr = *(void**)((char*)pt + offFrag); if (fr && fr != pt) ft.insert(fr); }
      ChainState cs = {}; cs.mount = mount; cs.hops = c->second.hops; cs.mult = 1.0f; cs.chance = c->second.chance; cs.dmg = c->second.dmg; cs.decay = c->second.decay; cs.copy = copy;
      st = g_chain.emplace(pj, cs).first;
    }
    ChainState& cs = st->second;
    cs.alive = alive; cs.frame = chainFrame;
    void* t = *(void**)((char*)pj + g_offPjTarget); if (t) cs.target = t;
    cs.ptype = *(void**)((char*)pj + g_offPjType);
    memcpy(cs.vel, (char*)pj + g_offPjVel, sizeof cs.vel);
    cs.mult = *(float*)((char*)pj + g_offPjDmg);
    if (cs.target && std::find(targets.begin(), targets.end(), cs.target) == targets.end()) targets.push_back(cs.target);
  }
  for (auto it = g_chain.begin(); it != g_chain.end(); ) { if (it->second.frame != chainFrame) { g_chainBoost.erase(it->first); it = g_chain.erase(it); } else ++it; }   // repooled
  for (auto it = g_chainBoost.begin(); it != g_chainBoost.end(); ) { if (!g_chain.count(*it)) it = g_chainBoost.erase(it); else ++it; }   // never boost a reused shot
  for (const ChainState& cs : spawn) {
    if (!cs.ptype || !cs.mount) continue;
    float sp = sqrtf(cs.vel[0] * cs.vel[0] + cs.vel[1] * cs.vel[1] + cs.vel[2] * cs.vel[2]); if (sp < 1) continue;
    float dir[3] = { cs.vel[0] / sp, cs.vel[1] / sp, cs.vel[2] / sp };
    void* next = cs.target; V3 np = {};
    { V3 q; void* other = NearestEnemyTarget(cs.pos, 400.0f, &q, cs.target); if (other) { next = other; np = q; } }   // another ship or shore target near: jump to it
    if (next == cs.target && g_offLeAlive >= 0 && !*(bool*)((char*)next + g_offLeAlive)) continue;   // it sank and nothing else is near: the chain ends
    bool jump = next != cs.target;
    V3 start, vel;
    if (next != cs.target) {
      float dx = np.x - cs.pos[0], dy = np.y - cs.pos[1], dz = np.z - cs.pos[2], dl = sqrtf(dx * dx + dy * dy + dz * dz); if (dl < 1) continue;
      start = { cs.pos[0] + dir[0] * 4, cs.pos[1] + dir[1] * 4 + 1, cs.pos[2] + dir[2] * 4 };
      vel = { dx / dl * sp, dy / dl * sp, dz / dl * sp };
    } else {                                                           // same target: up and away, seek brings it back
      float ux = dir[0] * 0.35f, uy = 1.0f, uz = dir[2] * 0.35f, ul = sqrtf(ux * ux + uy * uy + uz * uz);
      start = { cs.pos[0] + dir[0] * 4, cs.pos[1] + 3, cs.pos[2] + dir[2] * 4 };
      float ls = sp * 0.8f < 150.0f ? sp * 0.8f : 150.0f;              // slow enough to loop back (radius about v / 360 deg/s)
      vel = { ux / ul * ls, uy / ul * ls, uz / ul * ls };
    }
    void* pj = Invoke(mFetch, pm);
    if (!pj) continue;
    void* a[4] = { cs.ptype, &start, &vel, cs.mount };
    Invoke(mShoot, pj, a);
    if (cs.copy) { *(bool*)((char*)pj + g_offPjNetOwner) = false; g_copyChains++; }   // shown only: the shooter's own leap does the damage
    if (jump) SafeChainStreak(cs, next, np);                           // no streak for the loop back onto the same target
    SetRef(pj, (char*)pj + g_offPjTarget, next);
    *(float*)((char*)pj + g_offPjDmg) = cs.mult * cs.dmg;
    ChainState n = cs; n.target = next; n.hops = cs.hops - 1; n.leap = cs.leap + 1; n.mult = cs.mult * cs.dmg; n.chance = cs.chance * cs.decay; memcpy(n.vel, &vel, sizeof n.vel);
    n.alive = g_offPjAlive >= 0 ? *(float*)((char*)pj + g_offPjAlive) : 0.0f;
    g_chain[pj] = n;
    g_chainBoost.insert(pj);
    g_chainFired++;
  }
}

