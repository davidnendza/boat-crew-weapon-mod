// Automated guns in the utility slot (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- Automated guns in the utility slot ------------------------------------------------------------------------------
// Every slot family has WeaponSystem.followTargetingMode=1 except 'Utility Weapon' (0), and EntityTargetingManager only
// hands targets to systems with it set; PlayerAuxiliaryWeaponController.weapons[] (system, fireMode, aimingMode
// none/forward/full) aims utility guns at the camera instead and fires them on the player's input.
// While a crewless gun is selected there in battle: followTargetingMode=1 and its aux entry aimingMode=none, both put
// back when a crewed gun is selected again. Every 30 frames; the aux list is logged once per battle.
static std::map<void*, bool> g_autoFollowOrig;                       // weapon system -> its followTargetingMode before we set it
static std::map<void*, int> g_autoAimOrig;                           // aux Weapon entry -> its aimingMode before we set it
static int g_autoFrames = 0, g_autoFaults = 0;
static const bool g_autoUtility = true;                              // Automated utility guns: aux aimingMode -> none while one is selected
static void* g_autoLoggedEng = nullptr;
static void* AuxController() {
  if (!InitSeatIcons()) return nullptr;                              // resolves Resources.FindObjectsOfTypeAll and the class pointers
  static void* type = nullptr;
  if (!type) { void* k = FindClass("Core.Player", "PlayerAuxiliaryWeaponController"); type = k ? il2cpp_type_get_object(il2cpp_class_get_type(k)) : nullptr; }
  if (!type) return nullptr;
  void* a[1] = { type };
  void* arr = Invoke(g_resFindAll, nullptr, a);
  int n = ArrLen(arr);
  // FindObjectsOfTypeAll also returns prefab copies; the live controller is the one that has found its ship (playerShip +0x28)
  for (int i = 0; i < n; i++) {
    void* c = ArrAt(arr, i);
    if (!c) continue;
    if (*(void**)((char*)c + 0x28)) return c;
  }
  return nullptr;                                                      // not live yet: never edit a prefab copy (it outlives the battle)
}
// Crewless guns are fired by a FireControlMaster (GunMount.shootAll from aimAtTargets; fillCasters checks get_maxRange).
// Its List<WeaponSystem> systems (+0x40) is filled in Awake when autoFillAllSystems (+0x34) is set. If the player's boat
// has one whose list lacks the utility slot, the slot is added and createTargetsList() rebuilds its parallel target
// list. Once per battle.
static void* g_fcmDoneEng = nullptr;
static void* g_playerFcm = nullptr; static void* g_playerFcmHandle = nullptr;
static void AutoUtilityFireControl(void* eng, void* craft, void* ws) {
  if (g_fcmDoneEng == eng) return;
  g_fcmDoneEng = eng;
  if (g_playerFcmHandle) { Unpin(g_playerFcmHandle); g_playerFcmHandle = nullptr; }
  g_playerFcm = nullptr;                                               // found again below for this battle
  void* k = FindClass("Core", "FireControlMaster");
  if (!k || !g_resFindAll) { DLog("auto utility: FireControlMaster class not found"); return; }
  int oSelf = FieldOffset(k, "self"), oAuto = FieldOffset(k, "autoFillAllSystems"), oSys = FieldOffset(k, "systems");
  void* createTargets = il2cpp_class_get_method_from_name(k, "createTargetsList", 0);
  void* type = il2cpp_type_get_object(il2cpp_class_get_type(k));
  void* a[1] = { type };
  void* arr = Invoke(g_resFindAll, nullptr, a);                      // once per battle
  int n = ArrLen(arr);
  int onPlayer = 0;
  for (int i = 0; i < n; i++) {
    void* f = ArrAt(arr, i); if (!f) continue;
    void* self = oSelf >= 0 ? *(void**)((char*)f + oSelf) : nullptr;
    if (self != craft) continue;
    onPlayer++;
    if (g_playerFcmHandle) Unpin(g_playerFcmHandle);
    g_playerFcm = f; g_playerFcmHandle = Pin(f);
    void* list = oSys >= 0 ? *(void**)((char*)f + oSys) : nullptr;
    std::string names; bool has = false;
    for (int j = 0; j < ListCount(list); j++) { void* s2 = ListAt(list, j); if (s2 == ws) has = true; names += (s2 ? NameOf(s2) : std::string("null")) + ", "; }
    Log("auto utility: player FireControlMaster %p autoFill=%d systems (%d): %s utility slot %s", f, oAuto >= 0 ? *(bool*)((char*)f + oAuto) : -1, ListCount(list), names.c_str(), has ? "INCLUDED" : "missing");
    if (!has && list) {
      if (ListAdd(list, ws)) {
        if (createTargets) Invoke(createTargets, f);
        Log("auto utility:   added the utility slot to this FireControlMaster and rebuilt its target list");
      } else Log("auto utility:   could not add the utility slot");
    }
  }
  DLog("auto utility: %d FireControlMaster objects in total, %d on the player's boat", n, onPlayer);
}

// The selected utility mount: logged when handed over, and enabled if it is not (GunMount.shoot and aimAt enable a
// disabled mount themselves when enabledOnShootAndAim +0x21 is set; a disabled mount runs no FixedUpdateMe, which is
// where a crewless gun is marked crewed). Offsets from the dump: enabledOnShootAndAim +0x21, crewed +0x169.
static void AutoUtilityMountCheck(void* ws) {
  void* mount = ws ? *(void**)((char*)ws + g_offWsSelected) : nullptr; if (!mount) return;
  static void *mGetEn = nullptr, *mSetEn = nullptr; static int offEngage = -2;
  if (!mGetEn) { void* b = FindClass("UnityEngine", "Behaviour"); mGetEn = b ? FindMethod(b, "get_enabled", 0) : nullptr; mSetEn = b ? FindMethod(b, "set_enabled", 1) : nullptr; }
  if (offEngage == -2) offEngage = FieldOffset("Core", "GunType", "engagementDistance");
  void* r = mGetEn ? Invoke(mGetEn, mount) : nullptr; bool en = r && *(bool*)((char*)r + 0x10);
  void* gt = *(void**)((char*)mount + g_offGunType);
  Log("auto utility:   mount '%s' enabled %d enabledOnShootAndAim %d requiresCrew %d crewed %d engagementDistance %.0f", StripTags(NameOf(mount)).c_str(), en,
       *(bool*)((char*)mount + 0x21), *(bool*)((char*)mount + g_offReqCrew), *(bool*)((char*)mount + 0x169), gt && offEngage >= 0 ? *(float*)((char*)gt + offEngage) : -1.0f);
  if (r && !en && mSetEn) { bool t = true; void* a[1] = { &t }; Invoke(mSetEn, mount, a); Log("auto utility:   mount was disabled: enabled it"); }
}
// The fire control sees ships only inside its search sphere (FireControlMaster.search +0x78, EntitySphereOverlap.Range =
// EntityType.fireControlSystemSearchDistance x weather visibility, clamped by the weather). The player's boat has 2500,
// short of a crewless gun's kAutoRange, so while one is selected the boat's value is raised to kAutoRange
// and put back when a crewed gun is selected or a new battle starts. Fog still shortens it.
static void* g_searchEt = nullptr; static void* g_searchEtHandle = nullptr; static float g_searchOrig = 0;
static int g_offSearchDist = -2, g_offCraftParams = -1;
static void AutoUtilitySearch(void* craft, bool on) {
  if (g_offSearchDist == -2) {
    g_offSearchDist = FieldOffset("Core", "EntityType", "fireControlSystemSearchDistance"); g_offCraftParams = FieldOffset("Core", "CombatEntity", "parameters");
  }
  if (!on) {
    if (g_searchEt && g_offSearchDist >= 0) { *(float*)((char*)g_searchEt + g_offSearchDist) = g_searchOrig; DLog("auto utility:   fire control search distance put back to %.0f", g_searchOrig); }
    Unpin(g_searchEtHandle); g_searchEt = g_searchEtHandle = nullptr;
    return;
  }
  void* et = craft && g_offCraftParams >= 0 ? *(void**)((char*)craft + g_offCraftParams) : nullptr;
  if (!et || g_offSearchDist < 0 || g_searchEt) return;
  float v = *(float*)((char*)et + g_offSearchDist);
  if (v >= kAutoRange) return;
  g_searchEt = et; g_searchEtHandle = Pin(et); g_searchOrig = v;
  *(float*)((char*)et + g_offSearchDist) = kAutoRange;
  Log("auto utility:   fire control search distance %.0f -> %.0f (times weather visibility)", v, kAutoRange);
}
// Every 15 s (8 times per battle at most) while a crewless utility gun is selected: how far it gets through the boat's
// FireControlMaster. Casters by obstruction code (0 clear, 1 rotation, 2 elevation, 3 obstruction, 4 range, 5 engagement
// distance, 6 waiting for the raycast, 7 dead, 8 underwater) with the distance from the gun to each caster's ship, the
// search range, its TurretTargets' target count and fireAtWill, whether it is in validTargeters (aimed and fired this
// cycle), and the gun's ammo (Automated has none of its own: loaders bring it). Offsets from the dump: FireControlMaster
// systemCasters +0x48, systemTargets +0x50, visible +0x58, search +0x78, validTargeters +0xB8; TurretCaster system +0x10,
// obstruction +0x1C, targetEntity +0x28; TurretTargets system +0x10, targets +0x18, fireAtWill +0x20.
// Logged without debug so a crewless gun that never fires still leaves a trace. The fire control sees only marked
// enemies while constrainToAssignedTargets is on (see All-Knowing): 'marked-only 1, 0 in sight' means nothing marked.
static DWORD g_autoDiagAt = 0; static int g_autoDiagN = 0; static void* g_autoDiagEng = nullptr;
static void AutoUtilityDiag(void* eng, void* ws) {
  if (eng != g_autoDiagEng) { g_autoDiagEng = eng; g_autoDiagN = 0; }
  DWORD now = GetTickCount();
  if (!g_playerFcm || g_autoDiagN >= 8 || now - g_autoDiagAt < 15000) return;
  void* f = g_playerFcm;
  static void *mTransform = nullptr, *mPosition = nullptr, *mRange = nullptr;
  static int offMarked = -1, offMun = -1, offMag = -1;
  if (!mTransform) {
    void* c = FindClass("UnityEngine", "Component"); mTransform = c ? FindMethod(c, "get_transform", 0) : nullptr;
    void* t = FindClass("UnityEngine", "Transform"); mPosition = t ? FindMethod(t, "get_position", 0) : nullptr;
    void* so = FindClass("Core", "EntitySphereOverlap"); mRange = so ? FindMethod(so, "get_Range", 0) : nullptr;
    offMarked = FieldOffset("Core", "FireControlMaster", "constrainToAssignedTargets"); offMun = FieldOffset("Core", "GunMount", "munitions"); offMag = FieldOffset("Core", "GunMount", "inMag");
  }
  auto posOf = [&](void* obj, V3& p) {
    void* tr = obj && mTransform ? Invoke(mTransform, obj) : nullptr; void* pb = tr && mPosition ? Invoke(mPosition, tr) : nullptr;
    if (pb) p = *(V3*)((char*)pb + 0x10);
    return pb != nullptr;
  };
  void* cl = *(void**)((char*)f + 0x48);
  int casters = 0;
  for (int i = 0; i < ListCount(cl); i++) { void* c = ListAt(cl, i); if (c && *(void**)((char*)c + 0x10) == ws) casters++; }
  g_autoDiagAt = now; g_autoDiagN++;
  void* mount = *(void**)((char*)ws + g_offWsSelected);
  V3 mp; bool haveMp = posOf(mount, mp);
  int codes[9] = {}; std::string dist;
  for (int i = 0; i < ListCount(cl); i++) {
    void* c = ListAt(cl, i); if (!c || *(void**)((char*)c + 0x10) != ws) continue;
    int o = *(int*)((char*)c + 0x1C); if (o >= 0 && o < 9) codes[o]++;
    V3 tp;
    if (haveMp && dist.size() < 160 && posOf(*(void**)((char*)c + 0x28), tp)) {
      char b[24]; snprintf(b, sizeof b, " %d@%.0fm", o, sqrtf((tp.x - mp.x) * (tp.x - mp.x) + (tp.y - mp.y) * (tp.y - mp.y) + (tp.z - mp.z) * (tp.z - mp.z)));
      dist += b;
    }
  }
  void* tl = *(void**)((char*)f + 0x50); void* tt = nullptr;
  for (int i = 0; i < ListCount(tl); i++) { void* t = ListAt(tl, i); if (t && *(void**)((char*)t + 0x10) == ws) { tt = t; break; } }
  void* vl = *(void**)((char*)f + 0xB8); bool valid = false;
  for (int i = 0; i < ListCount(vl); i++) if (ListAt(vl, i) == tt && tt) valid = true;
  void* search = *(void**)((char*)f + 0x78);
  void* rr = search && mRange ? Invoke(mRange, search) : nullptr;
  Log("auto utility: '%s': fire control marked-only %d, %d in sight, search range %.0f m; for this gun %d (clear %d rot %d elev %d obstr %d range %d engage %d pending %d dead %d under %d); targets %d fireAtWill %d; firing %d; ammo %d loaded + %d at the gun; code@distance:%s",
      mount ? StripTags(NameOf(mount)).c_str() : "?", offMarked >= 0 ? *(bool*)((char*)f + offMarked) : -1, ListCount(*(void**)((char*)f + 0x58)), rr ? *(float*)((char*)rr + 0x10) : -1.0f,
      casters, codes[0], codes[1], codes[2], codes[3], codes[4], codes[5], codes[6], codes[7], codes[8],
      tt ? ListCount(*(void**)((char*)tt + 0x18)) : -1, tt ? *(bool*)((char*)tt + 0x20) : -1, valid,
      mount && offMag >= 0 ? *(int*)((char*)mount + offMag) : -1, mount && offMun >= 0 ? *(int*)((char*)mount + offMun) : -1, dist.c_str());
}

struct TurretStats;
static TurretStats* TurretOf(void* mount);
// Every frame while an automated sky-shot gun (GunType.skyShot +0x70, the player mortars) is selected: its aim point
// GunMount.lastTarget (+0x190, written by aimAt) goes into skyShotPosition (+0x1C0), where GunMount.shoot puts the shell.
static V3 PosOf(void* component);
// The ship each automated sky-shot mount last fired at (pinned), for AutoSkyAim: lastTarget is the gun's ballistic aim
// point, 100-300 m up and off the ship, so it is no place to drop a shell.
static std::map<void*, std::pair<void*, void*>> g_skyTarget;         // mount -> (LiveEntity, GC handle)
static void SkyRemember(void* mount, void* tgt) {
  auto it = g_skyTarget.find(mount);
  if (it != g_skyTarget.end() && it->second.first == tgt) return;
  if (it != g_skyTarget.end()) Unpin(it->second.second);
  g_skyTarget[mount] = { tgt, Pin(tgt) };
}
static void SkyForgetAll() { for (auto& kv : g_skyTarget) Unpin(kv.second.second); g_skyTarget.clear(); }
// Frames aimed by: 0 aim point, 1 forceTarget, 2 last shell's ship; per mount, for that gun's battle summary line.
static std::unordered_map<void*, std::array<long long, 3>> g_skySrcBy;
// Scatter, since a shell dropped on the ship's centre would never miss: each automated sky shell lands at a random
// point in a disk of radius kSkyScatter x range x (1 + (100 - Accuracy) / 50), so Inaccuracy affixes widen it; a new
// point is rolled after every shot (SkyShotFired), not every frame.
static const float kSkyScatter = 0.04f;
static std::unordered_map<void*, std::pair<float, float>> g_skyDisk;  // mount -> unit-disk point for its next shell
static void SkyShotFired(void* mount) {
  float r = sqrtf((float)Rand01()), a = (float)(Rand01() * 6.2831853);
  g_skyDisk[mount] = { r * cosf(a), r * sinf(a) };
}
static void AutoSkyAim() {
  static int offSky = -2, offLastT = -1, offSkyPos = -1, offForce = -1, offAlive = -1;
  if (offSky == -2) {
    offSky = FieldOffset("Core", "GunType", "skyShot"); offLastT = FieldOffset("Core", "GunMount", "lastTarget"); offSkyPos = FieldOffset("Core", "GunMount", "skyShotPosition");
    offForce = FieldOffset("Core", "GunMount", "forceTarget"); offAlive = FieldOffset("Core", "LiveEntity", "_alive");
    if (offSky < 0 || offLastT < 0 || offSkyPos < 0) Log("auto utility: sky aim MISSING PIECES (skyShot +0x%X lastTarget +0x%X skyShotPosition +0x%X)", offSky, offLastT, offSkyPos);
  }
  if (offSky < 0 || offLastT < 0 || offSkyPos < 0 || g_autoFollowOrig.empty()) return;
  void* craft = PlayerCraft(); if (!craft) return;                      // only systems on the boat right now, not those of a sunk or rebuilt boat
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  int nSys = ArrLen(systems);
  for (int i = 0; i < nSys && i < 32; i++) {
    void* ws = ArrAt(systems, i);
    if (!ws || !g_autoFollowOrig.count(ws)) continue;                   // the slots switched to automated this battle
    void* mount = *(void**)((char*)ws + g_offWsSelected); if (!mount || !Alive(mount)) continue;
    void* gt = *(void**)((char*)mount + g_offGunType);
    if (!gt || !*(bool*)((char*)gt + offSky)) continue;
    // the fire control's target (forceTarget, set when aimAt gets one), else the ship the last shell went at; the shell
    // appears 20 m above it (falls in ~0.1 s); with neither, the aim point (first shell of an engagement)
    void* tgt = offForce >= 0 ? *(void**)((char*)mount + offForce) : nullptr;
    int src = 0;
    if (tgt && Alive(tgt) && (offAlive < 0 || *(bool*)((char*)tgt + offAlive))) src = 1;
    else {
      auto it = g_skyTarget.find(mount);
      tgt = it != g_skyTarget.end() ? it->second.first : nullptr;
      if (tgt && Alive(tgt) && (offAlive < 0 || *(bool*)((char*)tgt + offAlive))) src = 2; else tgt = nullptr;
    }
    V3 p = tgt ? PosOf(tgt) : V3{ 0, -10000, 0 };
    if (tgt && p.y > -9000) {
      V3 me = PosOf(mount);
      float range = sqrtf((p.x - me.x) * (p.x - me.x) + (p.z - me.z) * (p.z - me.z));
      double acc = NumField(gt, "Accuracy"); if (acc < 0) acc = 0; if (acc > 100) acc = 100;
      float rad = kSkyScatter * range * (float)(1 + (100 - acc) / 50);
      auto d = g_skyDisk.find(mount); if (d == g_skyDisk.end()) { SkyShotFired(mount); d = g_skyDisk.find(mount); }
      p.x += d->second.first * rad; p.z += d->second.second * rad; p.y += 20;
      memcpy((char*)mount + offSkyPos, &p, 12);
    }
    else { memcpy((char*)mount + offSkyPos, (char*)mount + offLastT, 12); src = 0; }
    g_skySrcBy[mount][src]++;
  }
}
static void AutoUtilityTick() {
  void* eng = EngagementInstance();
  if (eng && eng == g_autoLoggedEng) AutoSkyAim();                     // only once this battle's tick has forgotten last battle's slots
  if (!eng || ++g_autoFrames % 30 != 0) return;
  static int offFollow = -2, offAuxWeapons = -1, offAwSystem = -1, offAwFire = -1, offAwAim = -1, offAwExt = -1;
  if (offFollow == -2) {
    offFollow = FieldOffset("Core", "WeaponSystem", "followTargetingMode");
    offAuxWeapons = FieldOffset("Core.Player", "PlayerAuxiliaryWeaponController", "weapons");
    void* ctl = FindClass("Core.Player", "PlayerAuxiliaryWeaponController");
    void* iter = nullptr; void* nested; void* wk = nullptr;
    while (ctl && (nested = il2cpp_class_get_nested_types(ctl, &iter))) if (!strcmp(il2cpp_class_get_name(nested), "Weapon")) wk = nested;
    offAwSystem = FieldOffset(wk, "system"); offAwFire = FieldOffset(wk, "fireMode"); offAwAim = FieldOffset(wk, "aimingMode"); offAwExt = FieldOffset(wk, "externalInput");
    DLog("auto utility: offsets follow %d aux.weapons %d Weapon.system %d fireMode %d aimingMode %d externalInput %d", offFollow, offAuxWeapons, offAwSystem, offAwFire, offAwAim, offAwExt);
  }
  if (offFollow < 0) return;
  void* craft = PlayerCraft();
  if (!craft) return;
  void* wm = *(void**)((char*)craft + g_offCraftWM);
  void* systems = wm ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  static void* ctl = nullptr; static void* ctlEng = nullptr; static void* ctlHandle = nullptr;
  if (eng != ctlEng || !ctl) {                                         // once per battle (asked again until it is live), held with a GC handle
    if (ctlHandle && il2cpp_gchandle_free) il2cpp_gchandle_free(ctlHandle);
    ctlHandle = nullptr;
    ctl = AuxController();
    if (ctl) { ctlEng = eng; if (il2cpp_gchandle_new) ctlHandle = il2cpp_gchandle_new(ctl, false); }
  }
  void* aux = ctl && offAuxWeapons >= 0 ? *(void**)((char*)ctl + offAuxWeapons) : nullptr;   // Weapon[]
  int nAux = ArrLen(aux);
  bool logNow = eng != g_autoLoggedEng;
  int nLive = ArrLen(systems);
  if (logNow && (!g_autoFollowOrig.empty() || !g_autoAimOrig.empty())) {
    for (int i = 0; i < nLive && i < 32; i++) {                        // only objects that are on the boat right now are written
      void* ws = ArrAt(systems, i);
      auto f = g_autoFollowOrig.find(ws);
      if (ws && f != g_autoFollowOrig.end()) *(bool*)((char*)ws + offFollow) = f->second;
    }
    for (int j = 0; j < nAux; j++) {
      void* w = ArrAt(aux, j);
      auto f = g_autoAimOrig.find(w);
      if (w && f != g_autoAimOrig.end() && offAwAim >= 0) *(int*)((char*)w + offAwAim) = f->second;
    }
    g_autoFollowOrig.clear(); g_autoAimOrig.clear();
    DLog("auto utility: new battle, earlier changes put back and forgotten");
  }
  if (logNow) AutoUtilitySearch(nullptr, false);
  if (logNow) {
    g_autoLoggedEng = eng;
    DLog("auto utility: aux controller %p with %d weapons", ctl, nAux);
    for (int j = 0; j < nAux; j++) {
      void* w = ArrAt(aux, j); if (!w) continue;
      void* sys = offAwSystem >= 0 ? *(void**)((char*)w + offAwSystem) : nullptr;
      DLog("auto utility:   aux[%d] system %p '%s' fireMode %d aimingMode %d externalInput %d", j, sys, sys ? NameOf(sys).c_str() : "-",
          offAwFire >= 0 ? *(int*)((char*)w + offAwFire) : -1, offAwAim >= 0 ? *(int*)((char*)w + offAwAim) : -1, offAwExt >= 0 ? *(bool*)((char*)w + offAwExt) : -1);
    }
  }
  int nSys = ArrLen(systems);          // WeaponManager.systems is WeaponSystem[]
  for (int i = 0; i < nSys && i < 32; i++) {
    void* ws = ArrAt(systems, i); if (!ws) continue;
    int c = CrewlessMount(ws);
    // the empty 'None' entry has no seats and no crew flag: not a gun
    if (c == 1 && SeatCount(*(void**)((char*)ws + g_offWsSelected)) == 0) c = -1;
    if (c == 1) TurretOf(*(void**)((char*)ws + g_offWsSelected));       // so the battle summary has it even if it never fires
    bool* follow = (bool*)((char*)ws + offFollow);
    auto orig = g_autoFollowOrig.find(ws);
    if (c == 1 && orig == g_autoFollowOrig.end() && !*follow) {
      g_autoFollowOrig[ws] = *follow; *follow = true;
      Log("auto utility: slot '%s' (system %p) has a crewless gun selected: followTargetingMode 0 -> 1", NameOf(ws).c_str(), ws);
      AutoUtilityFireControl(eng, craft, ws);
      AutoUtilityMountCheck(ws);
      AutoUtilitySearch(craft, true);
      for (int j = 0; j < nAux; j++) {
        void* w = ArrAt(aux, j);
        if (!w || offAwSystem < 0 || offAwAim < 0) continue;
        void* sys = *(void**)((char*)w + offAwSystem);
        // the aux entry can point at another object named 'Utility Weapon'; fall back to the name
        if (sys != ws && !(sys && NameOf(sys) == NameOf(ws))) continue;
        if (sys != ws) DLog("auto utility:   aux[%d] system %p is not this slot %p but has its name; using it", j, sys, ws);
        g_autoAimOrig[w] = *(int*)((char*)w + offAwAim); *(int*)((char*)w + offAwAim) = 0;
        Log("auto utility:   aux[%d] aimingMode %d -> 0 (none)", j, g_autoAimOrig[w]);
      }
    } else if (c == 1 && orig != g_autoFollowOrig.end()) {
      AutoUtilityDiag(eng, ws);
    } else if (c == 0 && orig != g_autoFollowOrig.end()) {
      *follow = orig->second; g_autoFollowOrig.erase(orig);
      for (auto it = g_autoAimOrig.begin(); it != g_autoAimOrig.end(); ) {
        void* w = it->first;
        void* sys = offAwSystem >= 0 ? *(void**)((char*)w + offAwSystem) : nullptr;   // matched by name too, as when it was set
        if (sys && (sys == ws || NameOf(sys) == NameOf(ws))) { *(int*)((char*)w + offAwAim) = it->second; it = g_autoAimOrig.erase(it); } else ++it;
      }
      AutoUtilitySearch(nullptr, false);
      Log("auto utility: slot '%s' has a crewed gun again: settings restored", NameOf(ws).c_str());
    }
  }
}

