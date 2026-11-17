// on-kill effects (creditKill hooks), gun effects, crates per round (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- on-kill effects (Salvager, Vampiric, Bounty) -------------------------------------------------------------------
// PlayerKillFeedback.killConfirmed calls IKillCredible.creditKill(killed, factor) on everything that damaged the dead
// entity, through interface dispatch (the class vtable). Who that is: ProjectileManager.endJobs registers, for a hit from
// GunMount g, the LastMan of every seat of g's slot (the gunners, CrewMember) and only falls back to the WeaponSystem when
// nobody manned it (first test 2026-10-25: a crewed gun gave 0 WeaponSystem credits). So both are patched: the
// WeaponSystem stub (crewless guns, gun = selected) and CrewMember.creditKill (gun = achievedGunMount, +0xF8). A kill
// counts once per gun even when two gunners are credited.
typedef void (*fn_creditKill)(void*, void*, float, void*);
static fn_creditKill g_creditKillOrig = nullptr;
static int g_killFaults = 0;
static void* g_lastKills[16][2]; static DWORD g_lastKillAt[16]; static int g_lastKillPos = 0;
static bool SeenKill(void* killed, void* mount) {                    // the same kill credited twice to one gun (two gunners)
  DWORD now = GetTickCount();
  for (int i = 0; i < 16; i++) if (g_lastKills[i][0] == killed && g_lastKills[i][1] == mount && now - g_lastKillAt[i] < 3000) return true;
  g_lastKills[g_lastKillPos][0] = killed; g_lastKills[g_lastKillPos][1] = mount; g_lastKillAt[g_lastKillPos] = now;
  g_lastKillPos = (g_lastKillPos + 1) % 16;
  return false;
}
// A hull's look is kept apart from its HP: CombatEntity.damageVisual (MeshDamageMultipoint) holds holes and burns as
// damage points, and only the crew's repair task shrinks them (CrewAI.TaskHandle.processScore, read in its disassembly
// 2026-10-28): addDamagePoint(part position, radius max(EntityType.estimatedLength, estimatedWidth) x 8 / 20,
// cut = burn = 1 - healthRatio, healTime = Time.time, healIntensity, applyToAllFound, minimizeDamage, useRadiusForClump).
// The mod's repairs (Vampiric, Lazarus) do the same, or the bar shows a sound hull that still looks shot to pieces
// (David 2026-10-28).
static void RepairVisual(void* craft, void* part) {
  static void *mAdd = nullptr, *mRatio = nullptr, *mTime = nullptr, *mPos = nullptr;
  static int offDv = -2, offParams = -1, offLen = -1, offWid = -1, offAllow = -1;
  if (offDv == -2) {
    void* ce = FindClass("Core", "CombatEntity"); void* et = FindClass("Core", "EntityType"); void* hp = FindClass("Core", "HealthPart");
    void* md = FindClass("Core", "MeshDamageMultipoint"); void* tm = FindClass("UnityEngine", "Time"); void* tr = FindClass("UnityEngine", "Transform");
    offDv = FieldOffset(ce, "damageVisual"); offParams = FieldOffset(ce, "parameters"); offLen = FieldOffset(et, "estimatedLength"); offWid = FieldOffset(et, "estimatedWidth"); offAllow = FieldOffset(hp, "allowDamageVisual");
    mAdd = md ? FindMethod(md, "addDamagePoint", 9) : nullptr; mRatio = hp ? FindMethod(hp, "healthRatio", 0) : nullptr;
    mTime = tm ? FindMethod(tm, "get_time", 0) : nullptr; mPos = tr ? FindMethod(tr, "get_position", 0) : nullptr;
    if (offDv < 0 || !mAdd || !mRatio || !mTime || !mPos) Log("repair visuals: lookups failed (damageVisual %d, addDamagePoint %p); the hull's look will not follow on-kill repairs", offDv, mAdd);
  }
  if (offDv < 0 || !mAdd || !mRatio || !mTime || !mPos || !craft || !part) return;
  if (offAllow >= 0 && !*(bool*)((char*)part + offAllow)) return;      // a part with no damage look
  void* dv = *(void**)((char*)craft + offDv); if (!dv) return;
  void* et = offParams >= 0 ? *(void**)((char*)craft + offParams) : nullptr;
  float len = et && offLen >= 0 ? *(float*)((char*)et + offLen) : 0, wid = et && offWid >= 0 ? *(float*)((char*)et + offWid) : 0;
  float radius = (len > wid ? len : wid) * 8.0f / 20.0f; if (radius <= 0) radius = 8.0f;
  void* rb = Invoke(mRatio, part); float dmg = 1.0f - (rb ? *(float*)((char*)rb + 0x10) : 1.0f); if (dmg < 0) dmg = 0;
  void* tb = Invoke(mTime, nullptr); float now = tb ? *(float*)((char*)tb + 0x10) : 0;
  void* tr = Invoke(m_getTransform, part); void* pb = tr ? Invoke(mPos, tr) : nullptr; if (!pb) return;
  V3 pos = *(V3*)((char*)pb + 0x10);
  float intensity = 1.0f; bool yes = true;
  void* a[9] = { &pos, &radius, &dmg, &dmg, &now, &intensity, &yes, &yes, &yes };
  Invoke(mAdd, dv, a);
  DLog("repair visuals: holes and burns within %.1f m of the repaired part shrunk to %.0f%%", radius, dmg * 100);
}
static void OnKillCredit(void* mount, void* killed, float factor) {
  if (!mount || IsRemoteMount(mount) || IsHiddenMount(mount)) return;   // co-op: their kill rewards are paid on their machine
  const Item* it = FindItem(StripTags(NameOf(mount)));
  if (!it) return;
  if (ItemFx(*it, "killAmmo") <= 0 && ItemFx(*it, "killRepair") <= 0 && ItemFx(*it, "killRP") <= 0) return;
  if (SeenKill(killed, mount)) return;
  double ammo = ItemFx(*it, "killAmmo"), repair = ItemFx(*it, "killRepair"), rp = ItemFx(*it, "killRP");
  if (ammo <= 0 && repair <= 0 && rp <= 0) return;
  // Bounty pays only for ships (Core.Ship or a subclass): planes come in waves and could be farmed (David, 2026-10-25)
  static void* shipClass = FindClass("Core", "Ship");
  static void* planeClass = FindClass("Core", "Plane");
  bool isShip = false, isPlane = false;
  const char* killedType = "?";
  if (killed) {
    void* k = il2cpp_object_get_class(killed);
    killedType = il2cpp_class_get_name(k);
    for (; k; k = il2cpp_class_get_parent(k)) { if (k == shipClass) isShip = true; if (k == planeClass) isPlane = true; }
  }
  if (!isShip) rp = 0;
  if (isPlane) { repair *= 0.5; ammo *= 0.5; }                        // planes come in waves (farmable); before the log line, so it shows what is paid
  Log("kill by '%s' (credit %.2f) of %s '%s'%s: ammo %.2f repair %.2f rp %.0f", it->name.c_str(), factor, killedType,
      killed ? NameOf(killed).c_str() : "?", isShip ? " (ship)" : "", ammo, repair, rp);
  if (ammo > 0) {
    static void* mFill = nullptr; if (!mFill) { void* gm = FindClass("Core", "GunMount"); mFill = gm ? FindMethod(gm, "munitionsFill", 1) : nullptr; }
    float f = (float)ammo; void* a[1] = { &f };
    if (mFill) { Invoke(mFill, mount, a); ShowMessage("Salvaged ammo: " + it->name); }
  }
  if (repair > 0) {
    static void *mGetHp = nullptr, *mSetHp = nullptr, *mMaxHp = nullptr, *mCraft = nullptr; static int offHull = -2;
    if (offHull == -2) {
      void* hp = FindClass("Core", "HealthPart"); void* ce = FindClass("Core", "CombatEntity"); void* pc = FindClass("Core.Player", "Player");
      mGetHp = hp ? FindMethod(hp, "get_HP", 0) : nullptr; mSetHp = hp ? FindMethod(hp, "set_HP", 1) : nullptr; mMaxHp = hp ? FindMethod(hp, "get_maxHP", 0) : nullptr;
      mCraft = pc ? FindMethod(pc, "get_playerCraft", 0) : nullptr;
      offHull = FieldOffset(ce, "hull");
    }
    void* craft = mCraft ? Invoke(mCraft, nullptr) : nullptr;
    void* hull = craft && offHull >= 0 ? *(void**)((char*)craft + offHull) : nullptr;
    void* worst = nullptr; float worstHp = 0, worstMax = 0, worstRatio = 1.0f;
    for (int i = 0; i < ListCount(hull) && mGetHp && mMaxHp && mSetHp; i++) {
      void* part = ListAt(hull, i); if (!part) continue;
      void* h = Invoke(mGetHp, part); void* m = Invoke(mMaxHp, part);
      float hpv = h ? *(float*)((char*)h + 0x10) : 0, mx = m ? *(float*)((char*)m + 0x10) : 0;
      if (mx <= 0 || hpv <= 0) continue;                               // destroyed parts are left to the crew
      if (hpv / mx < worstRatio) { worstRatio = hpv / mx; worst = part; worstHp = hpv; worstMax = mx; }
    }
    if (worst) {
      float nv = worstHp + (float)repair * worstMax; if (nv > worstMax) nv = worstMax;
      void* a[1] = { &nv }; Invoke(mSetHp, worst, a);
      RepairVisual(craft, worst);
      int pct = (int)std::lround((nv - worstHp) / worstMax * 100);
      ShowMessage("Hull repaired: +" + std::to_string(pct) + "%");
      SafeHealStreak(killed, craft, it->name.c_str(), killedType, pct);   // the streak from the wreck (effect_streaks)
    }
  }
  if (rp > 0) PayRp((int)std::lround(rp), "Bounty");
}
// CampaignState.currencyPayment(i, reason) adds i RP.
static void PayRp(int amount, const char* reason) {
  static void *mCur = nullptr, *mPay = nullptr;
  if (!mCur) { void* cs = FindClass("Core.Campaign", "CampaignState"); mCur = cs ? FindMethod(cs, "get_Current", 0) : nullptr; mPay = cs ? FindMethod(cs, "currencyPayment", 2) : nullptr; }
  void* cur = mCur ? Invoke(mCur, nullptr) : nullptr;
  if (!cur || !mPay) return;
  void* a[2] = { &amount, il2cpp_string_new(reason) };
  Invoke(mPay, cur, a);
  ShowMessage(std::string(reason) + ": +" + std::to_string(amount) + " RP");
}
static void SafeGfxKill(void* mount, void* killed, void* gunner);    // gun effects (0.9.3): Rallying, Veteran's
static void Hooked_CreditKill(void* self, void* killed, float factor, void* method) {
  g_creditKillOrig(self, killed, factor, method);
  if (g_ready) SafeGfxKill(*(void**)((char*)self + g_offWsSelected), killed, nullptr);
  if (g_ready && g_elitesOn && g_eliteFaults >= 0) { __try { EliteKilled(killed); } __except (FaultFilter("elite kill", GetExceptionInformation())) { g_eliteFaults = -1; } }
  if (g_ready && g_unuFaults >= 0) { __try { UnusualKill(self, *(void**)((char*)self + g_offWsSelected), killed); } __except (FaultFilter("extra effects (kill)", GetExceptionInformation())) { g_unuFaults = -1; } }
  if (g_ready && g_turretFaults >= 0) { __try { TurretKill(*(void**)((char*)self + g_offWsSelected), killed); } __except (FaultFilter("turret guns (kill)", GetExceptionInformation())) { g_turretFaults = -1; } }
  if (!g_ready || g_killFaults < 0) return;
  __try { OnKillCredit(*(void**)((char*)self + g_offWsSelected), killed, factor); }
  __except (FaultFilter("on-kill effects", GetExceptionInformation())) { g_killFaults = -1; }
}
static fn_creditKill g_crewCreditKillOrig = nullptr;
static int g_offCrewGun = -1;                                        // CrewMember.achievedGunMount
static void Hooked_CrewCreditKill(void* self, void* killed, float factor, void* method) {
  g_crewCreditKillOrig(self, killed, factor, method);                  // the game's own: gunner XP and so on
  if (g_ready) SafeCrewLogKill(self, killed);
  if (g_ready && g_offCrewGun >= 0) SafeGfxKill(*(void**)((char*)self + g_offCrewGun), killed, self);
  if (g_ready && g_elitesOn && g_eliteFaults >= 0) { __try { EliteKilled(killed); } __except (FaultFilter("elite kill", GetExceptionInformation())) { g_eliteFaults = -1; } }
  if (g_ready && g_unuFaults >= 0 && g_offCrewGun >= 0) {
    __try { void* mount = *(void**)((char*)self + g_offCrewGun); UnusualKill(mount ? *(void**)((char*)mount + 0x90) : nullptr, mount, killed); }
    __except (FaultFilter("extra effects (crew kill)", GetExceptionInformation())) { g_unuFaults = -1; }
  }
  if (g_ready && g_turretFaults >= 0 && g_offCrewGun >= 0) {
    __try { TurretKill(*(void**)((char*)self + g_offCrewGun), killed); }
    __except (FaultFilter("turret guns (crew kill)", GetExceptionInformation())) { g_turretFaults = -1; }
  }
  if (g_ready && g_crewOn && g_offCrewGun >= 0) {
    __try { CrewKill(*(void**)((char*)self + g_offCrewGun), killed); }
    __except (FaultFilter("crew weapon kills", GetExceptionInformation())) {}
  }
  if (!g_ready || g_killFaults < 0 || g_offCrewGun < 0) return;
  __try { OnKillCredit(*(void**)((char*)self + g_offCrewGun), killed, factor); }
  __except (FaultFilter("on-kill effects (crew)", GetExceptionInformation())) { g_killFaults = -1; }
}

// ---- gun effects -----------------------------------------------------------------------------------------------------
// Read in the disassembly first (2026-11-06). Every hit on a hull part goes through HealthPart.hitFlash(hit, damage, flash,
// heat) (rva 0x97E0C0): it applies globalDamageMultiplier and the target's damage control, then lowers the part's HP (or,
// on a co-op client, sends the damage to the host). Two callers matter: ProjectileManager.endJobs for a shell's direct
// hit (call at rva 0x9F4E66; its frame holds the Projectile at [rsp+0x168] and the part at [rsp+0x160]) and
// SplashDamageManager.FixedUpdate for blast damage (call at rva 0x982B10; its copy of the splash is at [rsp+0x90]:
// killer = the firing WeaponSystem, networkOwner, projectileCode). hitFlash is detoured, and only calls from those two
// places, checked byte for byte at install, are touched; the host applying a co-op client's damage (RequestDamage) never
// is, or that damage would be scaled twice. A hit scaled here counts direct hits AND blast, unlike Deadeye's crits.
//  * Marking: a hit on a ship marks it for markTime s; every hit of the player's guns on a marked ship does +mark.
//  * Giant Slayer: x giantSlayer on Rare/Elite ships (g_elites; co-op clients have the host's).
//  * Opening Salvo: x salvo for each battle's first magazine: shots are counted until GunMount.reloading (+0xC4) is first
//    seen; their direct hits scale, and blasts from the gun while the salvo runs or up to 3 s after it.
//  * Close Quarters / Long Shot: x closeBonus under closeDist m, x longBonus over longDist m (gun to impact).
//  * Veteran's: x (1 + vetPerKill x kills, up to vetCap kills) while its bonded gunner (the first who kills with it)
//    mans it (GunMount.seats -> Seat.lastMan). Kills come from CrewMember.creditKill; the bond and the count are kept on
//    the item (vet_crew=, vet_kills=) and wiped when that crew member is Dead, Missing or Captured
//    (CrewCharacter.healthState 2/3/4) on two checks a second apart (set_HP can turn Dead back to Wounded).
//  * Overkill: when a hit takes a ship's last vital part to 0 HP (the ship's death rule), the damage beyond the part's HP
//    (lastEffectiveDamage - HP before) x overkill goes as a splash onto the nearest other enemy in the next frame,
//    credited to the gun's weapon system. The spill carries kSpillCode, so no gun effect touches it. Not on co-op clients
//    (a client's splash on ships it does not own is unproven), like Volatile.
//  * Brittle: hits on the player's own gun (WeaponSystem.hull, a HealthPart) do x brittle while that gun is selected.
//  * Sure-Footed: CrewMember.hitFlash(hit, d) (rva 0x9A06C0) is the only path from hits to throwOverboardImpact: when
//    hit and d > 3 it rolls ceil(d/15) times CharacterConstants.overboardChance (static, 0.02) against a WHOLE percent, so
//    scaling the chance does nothing. Instead, each hit a Sure-Footed gun lands adds a stack (up to steadyCap, one fades
//    every steadyDecay s), and a hit on the player's crew then has a stacks x steady chance to be braced: overboardChance
//    is 0 for that one call (the damage, flash and wounds still happen) and is put back right after, also on a throw.
//  * Rallying: a kill takes rally s off the bleed-out timer of every downed crew member of the player's boat
//    (CrewMember.downedTime counts up while hp <= 0; at CrewCharacter.AllowedDownedTime he dies). Not for one whose timer
//    is already past it (rollInstantDeath sets it there on purpose), and never under 0.1 (iterateDowned's own floor).
//  * Frugal: GunMount.shoot uses no round when the firing Barrel.skipConsumeAmmo (+0x38) is set. After each shot of a
//    Frugal gun its barrels' flags are rolled for the next one (only barrels whose stock flag is off), and put back at the
//    battle change. consumptionAvoidanceChance is a crew trait for medkits and repair kits, never read by shoot().
// All of it only for the player's own guns and crew; co-op players' shots are theirs (copies deal no damage anyway), and
// marks are per machine. Every effect logs its events (the first few per battle, then debug lines) and a summary line.
static const int kSpillCode = -2;
struct GunFx {
  bool item = false; std::string name;
  float mark = 0, markTime = 8, giant = 0, salvo = 0, closeB = 0, closeD = 400, longB = 0, longD = 1000, overkill = 0;
  float vetPer = 0; int vetCap = 10; float rally = 0, steady = 0; int steadyCap = 10; float steadyDecay = 3, saveAmmo = 0, brittle = 0;
};
static std::unordered_map<void*, GunFx> g_gfxCache;                   // mount -> its effects, per battle (stable references)
struct GfxStats { int marks = 0, markedHits = 0, giantHits = 0, salvoShots = 0, salvoHits = 0, closeHits = 0, longHits = 0, spills = 0, vetHits = 0, vetKills = 0,
                  rallyKills = 0, rallyCrew = 0, stacks = 0, freeShots = 0, shots = 0, brittleHits = 0, capHits = 0;
                  double capCut = 0, markDmg = 0, giantDmg = 0, salvoDmg = 0, closeDmg = 0, longDmg = 0, spillDmg = 0, vetDmg = 0, rallySecs = 0, brittleDmg = 0; };
static std::map<std::string, GfxStats> g_gfxStats;                    // gun name -> this battle's counts
enum { GL_MARK, GL_GIANT, GL_SALVO, GL_RANGE, GL_SPILL, GL_VET, GL_RALLY, GL_STEADY, GL_FRUGAL, GL_BRITTLE, GL_CAP, GL_N };
static int g_gfxLines[GL_N];                                          // event lines this battle: the first 5 of each are Log lines
#define GFXLOG(kind, ...) do { if (g_gfxLines[kind]++ < 5) Log(__VA_ARGS__); else DLog(__VA_ARGS__); } while (0)
static bool g_gfxTried = false, g_gfxReady = false, g_gfxHitOn = false, g_gfxCrewOn = false, g_gfxInBattle = false;
static int g_gfxFaults = 0, g_gfxBadFrames = 0;
static uintptr_t g_raDirect = 0, g_raSplash = 0;                      // return addresses of the two hitFlash calls
static void* g_gfxCraft = nullptr; static int g_gfxTeam = -999, g_gfxEntityCode = 0;   // the player's boat, read each frame
static void *g_clsWs = nullptr, *m_allowedDowned = nullptr, *g_fOverboard = nullptr;
static void (*g_fieldStaticSet)(void*, void*) = nullptr;
static int g_oHpTeam = -1, g_oHpCe = -1, g_oHpHp = -1, g_oHpVital = -1, g_oHpLastEff = -1, g_oHpClient = -1, g_oPjPosi = -1;
static int g_oGmEntity = -1, g_oGmGunType = -1, g_oGtMag = -1, g_oGmReloading = -1, g_oGmBarrels = -1, g_oBarSkip = -1, g_oGmSeats = -1, g_oSeatMan = -1, g_oGmWs = -1, g_oWsHull = -1;
static int g_oLeTeam = -1, g_oLeCode = -1, g_oLeAlive = -1, g_oCeHull = -1, g_oCeCrew = -1, g_oMgrEnt = -1, g_oMgrCrew = -1, g_oCmMgr = -1, g_oCmChar = -1, g_oCmHp = -1, g_oCmDowned = -1;
static int g_oChName = -1, g_oChNameCustom = -1, g_oChHealth = -1;
// per-battle state
struct SalvoState { bool active = true; int shots = 0, mag = 4; DWORD endedAt = 0, liveAt = 0; float mult = 1; std::string name; };   // liveAt: last frame a salvo shell was in flight
static std::unordered_map<void*, SalvoState> g_salvo;                 // mount -> its opening salvo
static std::unordered_map<void*, void*> g_salvoShots;                 // shell -> mount, fired in an opening salvo (pooled: cleared per new life)
// Projectile.explode fires its fragments through Projectile.shoot with the parent's shotFrom, so they reach GfxShot as
// new shots of the same mount. A mount's shells are seen before their fragments: each shell's ProjectileType.fragment is
// remembered per mount, and a later shot of that type is a fragment (keeps the salvo bonus; no salvo round, no Frugal roll)
static std::unordered_map<void*, std::set<void*>> g_gfxFragTypes;     // mount -> its shells' fragment types, per battle
static int g_oPjType = -1, g_oPtFrag = -1;
struct BrittleGun { float factor; std::string name; };
static std::unordered_map<void*, BrittleGun> g_brittle;               // the player's gun hull (HealthPart) -> Brittle
static DWORD g_brittleAt = 0;
struct Spill { void* ce; void* mount; float damage; std::string gun, ship; };
static std::vector<Spill> g_spills;                                   // Overkill: queued in a hit, sent in the next frame
static std::unordered_map<void*, DWORD> g_spilled;                    // ship -> when it last spilled (once per death; pooled ships die again)
// handles: GC pins on the mount and its barrels, so the restore never writes into a collected and reused object
struct FrugalState { std::vector<void*> barrels, handles; bool armed = false, frameArmed = false, pinned = false; unsigned frame = 0; };
static std::unordered_map<void*, FrugalState> g_frugal;
static std::map<std::pair<void*, void*>, DWORD> g_gfxKillSeen;        // (killed, mount or gunner) -> when: one death counted once per gun (pooled entities die again)
static void* g_hitLastMount = nullptr; static void* g_hitLastCe = nullptr; static DWORD g_hitLastAt = 0;   // one blast = one hit
static float g_steadyStacks = 0, g_steadyPer = 0, g_steadyDecay = 3, g_steadyPeak = 0; static int g_steadyCap = 0, g_steadyBraced = 0, g_steadyHits = 0; static DWORD g_steadyAt = 0;
static std::set<std::string> g_vetDeadSeen;                           // bonded gunners seen dead at the last check
static DWORD g_vetCheckAt = 0; static bool g_vetDirty = false;
typedef void (*fn_hpHit)(void*, bool, float, bool, float, void*);
typedef void (*fn_crewHit)(void*, bool, float, void*);
static fn_hpHit g_hpHitOrig = nullptr;
static fn_crewHit g_crewHitOrig = nullptr;
static void Hooked_HpHit(void* self, bool hit, float dmg, bool flash, float heat, void* mi);
static void Hooked_CrewHit(void* self, bool hit, float d, void* mi);
static uintptr_t g_raCrewDirect = 0, g_raCrewSplash = 0;              // crew log: return addresses of CrewMember.hitFlash's two hit callers
static void CrewLogInit();

// The element class of an array field (Seat[] -> Seat): nested classes are not found by name.
static void* GfxElemClass(void* k, const char* field) {
  static auto elementClass = (void* (*)(void*))GetProcAddress(g_ga, "il2cpp_class_get_element_class");
  void* f = k ? il2cpp_class_get_field_from_name(k, field) : nullptr;
  void* ak = f ? il2cpp_class_from_type(il2cpp_field_get_type(f)) : nullptr;
  return ak && elementClass ? elementClass(ak) : nullptr;
}
static bool GfxCodeIs(uintptr_t rva, const unsigned char* b, size_t n) { return memcmp((char*)g_ga + rva, b, n) == 0; }
static bool GfxCallsTo(uintptr_t rva, void* fn) { unsigned char* p = (unsigned char*)g_ga + rva; return p[0] == 0xE8 && p + 5 + *(int32_t*)(p + 1) == (unsigned char*)fn; }
static std::string GfxCrewName(void* ch) {
  for (int off : { g_oChNameCustom, g_oChName }) {
    void* s = ch && off >= 0 ? *(void**)((char*)ch + off) : nullptr;
    if (s && *(int*)((char*)s + 0x10) > 0) return Str(s);
  }
  return std::string();
}
// Veteran's bond key: CrewCharacter.name first (a rename writes nameCustom and keeps name), as FemGName does; nameCustom
// only for a character with no name (review 2026-11-06: a renamed gunner locked his gun for good)
static std::string GfxCrewKey(void* ch) {
  for (int off : { g_oChName, g_oChNameCustom }) {
    void* s = ch && off >= 0 ? *(void**)((char*)ch + off) : nullptr;
    if (s && *(int*)((char*)s + 0x10) > 0) return Str(s);
  }
  return std::string();
}
static void* GfxCrewList() {
  void* mgr = g_gfxCraft && g_oCeCrew >= 0 ? *(void**)((char*)g_gfxCraft + g_oCeCrew) : nullptr;
  return mgr && g_oMgrCrew >= 0 ? *(void**)((char*)mgr + g_oMgrCrew) : nullptr;
}
static GfxStats& GfxStat(const std::string& gun) { return g_gfxStats[gun]; }

static const GunFx& GfxOf(void* mount) {
  auto c = g_gfxCache.find(mount);
  if (c != g_gfxCache.end()) return c->second;
  GunFx g; const Item* it = IsHiddenMount(mount) ? nullptr : ItemOfMount(mount);
  if (it) {
    auto v = [&](const char* k) { return (float)ItemFx(*it, k); };
    g.item = true; g.name = it->name;
    g.mark = v("mark"); if (v("markTime") > 0) g.markTime = v("markTime");
    g.giant = v("giantSlayer"); g.salvo = v("salvo");
    g.closeB = v("closeBonus"); if (v("closeDist") > 0) g.closeD = v("closeDist");
    g.longB = v("longBonus"); if (v("longDist") > 0) g.longD = v("longDist");
    g.overkill = v("overkill"); g.vetPer = v("vetPerKill"); if (v("vetCap") > 0) g.vetCap = (int)v("vetCap");
    g.rally = v("rally"); g.steady = v("steady"); if (v("steadyCap") > 0) g.steadyCap = (int)v("steadyCap"); if (v("steadyDecay") > 0) g.steadyDecay = v("steadyDecay");
    g.saveAmmo = v("saveAmmo"); g.brittle = v("brittle");
  } else {
    g.name = CrewClean(StripTags(NameOf(mount)));                      // a stock gun: only Marking's bonus reaches it
    std::string nm = StripTags(NameOf(mount)); static std::set<std::string> told;   // or a found gun purged at launch, still in its slot
    if (!IsHiddenMount(mount) && IsTombstone(nm) && told.insert(nm).second)
      Log("gun effects: mounted gun '%s' is not in the inventory (a retired gun; empty the slot on the loadout screen)", nm.c_str());
  }
  return g_gfxCache.emplace(mount, g).first->second;
}
static Item* GfxMutItem(const std::string& name) { for (Item& it : g_items) if (it.name == name && it.fromInventory) return &it; return nullptr; }

// Once, on the game thread (ProjectileManager.Update), so neither hitFlash runs while it is patched.
// ---- crates per round (crates_per_round=) ---------------------------------------------------------------------------
// The 380mm RW 61 (a Discord player's idea, David wants it): ten ammo crates make one round. Read in the disassembly
// first: a loader's delivery is CrewAI.TaskHandle.processScore (task kind 3), which inlines GunMount.munitionsFill:
// set_Munitions(munitions +0x130 + GunType.ammoPerFill +0x58 x multiplier), call at rva 0x992290, then lastAimed = 3 and
// enabled = true. munitionsFill itself (0x971D60) has no native caller (only the mod's Salvager), so it is left alone,
// as are reset/clearAmmo/setPayload and FixedUpdateMe's infinite-ammo set. set_Munitions clamps to EffectiveMaxAmmo.
// A crate that does not finish a round is consumed (the trip is done) but the value stays the same; munitionsReloadable
// (munitions + ammoPerFill <= max) stays true, so the loaders keep coming. Own boat only: other players' mounts are skipped.
typedef void (*fn_setMun)(void*, int, void*);
static fn_setMun g_setMunOrig = nullptr;
static uintptr_t g_raCrateFill = 0;
static int g_oGmMunitions = -1, g_crateFaults = 0;
struct CrateCount { std::string name; int per = 0, crates = 0, rounds = 0; };
static std::unordered_map<void*, CrateCount> g_crateCounts;           // mount -> this battle's deliveries
static void CrateBattleChange(bool hadBattle) {
  if (hadBattle) for (auto& e : g_crateCounts) {
    const CrateCount& c = e.second;
    Log("crates per round, last battle: '%s' took %d crate(s), %d made a round (%d of %d toward the next)", c.name.c_str(), c.crates, c.rounds, c.crates % c.per, c.per);
  }
  g_crateCounts.clear();
}
static int CrateGate(void* self, int value) {                         // the value set_Munitions gets
  if (!OnPlayerBoat(self) || IsRemoteMount(self)) return value;
  const Item* it = ItemOfMount(self);
  if (!it) return value;
  Item buf; const Item& x = ExpandBuiltFrom(*it, &buf);
  int cur = *(int*)((char*)self + g_oGmMunitions);
  if (x.cratesPerRound < 2 || value <= cur) return value;
  CrateCount& c = g_crateCounts[self];
  if (c.per != x.cratesPerRound) { c = CrateCount(); c.name = x.name; c.per = x.cratesPerRound; }
  int n = ++c.crates % c.per;
  bool full = c.crates <= 2 * c.per;                                    // the first two rounds in full, the rest with debug=1 (the battle summary has the totals)
  if (n) { if (full) Log("%s: crate %d of %d for the next round", c.name.c_str(), n, c.per); else DLog("%s: crate %d of %d for the next round", c.name.c_str(), n, c.per); return cur; }
  c.rounds++;
  if (full) Log("%s: crate %d of %d: a round is ready (%d this battle)", c.name.c_str(), c.per, c.per, c.rounds);
  else DLog("%s: crate %d of %d: a round is ready (%d this battle)", c.name.c_str(), c.per, c.per, c.rounds);
  return value;
}
static int CrateFilter(EXCEPTION_POINTERS* ep) { g_crateFaults = -1; return FaultFilter("crates per round", ep); }
static void Hooked_SetMunitions(void* self, int value, void* mi) {
  if ((uintptr_t)_ReturnAddress() == g_raCrateFill && g_crateFaults >= 0 && self) {
    __try { value = CrateGate(self, value); }
    __except (CrateFilter(GetExceptionInformation())) {}
  }
  g_setMunOrig(self, value, mi);
}
static void CratesInit() {
  void* gm = FindClass("Core", "GunMount");
  g_oGmMunitions = FieldOffset(gm, "munitions");
  void* m = gm ? FindMethod(gm, "set_Munitions", 1) : nullptr;
  void* p = m ? *(void**)m : nullptr;
  // set_Munitions: mov [rsp+10h],rbx; mov [rsp+18h],rsi; push rdi; sub rsp,20h (15 bytes, no rip-relative)
  static const unsigned char kSetMun[15] = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20 };
  if (!p || g_oGmMunitions < 0 || !GfxCallsTo(0x992290, p)) {
    Log("crates per round: MISSING PIECES (set_Munitions %p, munitions +0x%X, loader delivery call %s); crates_per_round is off, every crate adds its rounds",
        p, g_oGmMunitions, p && GfxCallsTo(0x992290, p) ? "found" : "not found");
    return;
  }
  g_setMunOrig = (fn_setMun)Detour(p, (void*)&Hooked_SetMunitions, kSetMun, 15, "GunMount.set_Munitions");
  if (!g_setMunOrig) { Log("crates per round: MISSING PIECES (set_Munitions could not be hooked); crates_per_round is off"); return; }
  g_raCrateFill = (uintptr_t)g_ga + 0x992295;
  Log("crates per round: ready (GunMount.set_Munitions detoured, loader delivery at +0x992290, munitions +0x%X)", g_oGmMunitions);
}

static void GfxInit() {
  g_gfxTried = true;
  CratesInit();
  EliteLookups();                                                      // Transform/Camera methods for the mark tags
  void* hp = FindClass("Core", "HealthPart"); void* pj = FindClass("Core", "Projectile"); void* gm = FindClass("Core", "GunMount");
  void* gt = FindClass("Core", "GunType"); void* le = FindClass("Core", "LiveEntity"); void* ce = FindClass("Core", "CombatEntity");
  void* cm = FindClass("Core", "CrewMember"); void* mg = FindClass("Core", "CrewManager"); void* ch = FindClass("Core", "CrewCharacter");
  void* cc = FindClass("Core", "CharacterConstants"); g_clsWs = FindClass("Core", "WeaponSystem");
  g_oHpTeam = FieldOffset(hp, "teamCode"); g_oHpCe = FieldOffset(hp, "combatEntity"); g_oHpHp = FieldOffset(hp, "_hp"); g_oHpVital = FieldOffset(hp, "vital");
  g_oHpLastEff = FieldOffset(hp, "lastEffectiveDamage"); g_oHpClient = FieldOffset(hp, "clientOnlyMode"); g_oPjPosi = FieldOffset(pj, "posi");
  // this runs before CritScan's own one-time lookup on the first frame (review 2026-11-06): the two Projectile fields here too
  if (g_offPjFrom < 0) g_offPjFrom = FieldOffset(pj, "shotFrom");
  if (g_offPjNetOwner < 0) g_offPjNetOwner = FieldOffset(pj, "networkOwner");
  g_oPjType = FieldOffset(pj, "ptype"); g_oPtFrag = FieldOffset(FindClass("Core", "ProjectileType"), "fragment");
  g_oGmEntity = FieldOffset(gm, "entity"); g_oGmGunType = FieldOffset(gm, "gunType"); g_oGtMag = FieldOffset(gt, "MagSize"); g_oGmReloading = FieldOffset(gm, "reloading");
  g_oGmBarrels = FieldOffset(gm, "barrels"); g_oBarSkip = FieldOffset(GfxElemClass(gm, "barrels"), "skipConsumeAmmo");
  g_oGmSeats = FieldOffset(gm, "seats"); g_oSeatMan = FieldOffset(GfxElemClass(gm, "seats"), "lastMan"); g_oGmWs = FieldOffset(gm, "weaponSystem"); g_oWsHull = FieldOffset(g_clsWs, "hull");
  g_oLeTeam = FieldOffset(le, "teamCode"); g_oLeCode = FieldOffset(le, "entityCode"); g_oLeAlive = FieldOffset(le, "_alive"); g_oCeHull = FieldOffset(ce, "hull"); g_oCeCrew = FieldOffset(ce, "crewManager");
  g_oMgrEnt = FieldOffset(mg, "entity"); g_oMgrCrew = FieldOffset(mg, "crew"); g_oCmMgr = FieldOffset(cm, "crewManager"); g_oCmChar = FieldOffset(cm, "_character");
  g_oCmHp = FieldOffset(cm, "hp"); g_oCmDowned = FieldOffset(cm, "downedTime");
  g_oChName = FieldOffset(ch, "name"); g_oChNameCustom = FieldOffset(ch, "nameCustom"); g_oChHealth = FieldOffset(ch, "healthState");
  m_allowedDowned = ch ? FindMethod(ch, "get_AllowedDownedTime", 0) : nullptr;
  g_fOverboard = cc ? il2cpp_class_get_field_from_name(cc, "overboardChance") : nullptr;
  g_fieldStaticSet = (void (*)(void*, void*))GetProcAddress(g_ga, "il2cpp_field_static_set_value");
  DLog("gun effects: HealthPart team +0x%X entity +0x%X _hp +0x%X vital +0x%X lastEffectiveDamage +0x%X clientOnlyMode +0x%X; Projectile.posi +0x%X; GunMount entity +0x%X gunType +0x%X "
       "reloading +0x%X barrels +0x%X (skipConsumeAmmo +0x%X) seats +0x%X (lastMan +0x%X) weaponSystem +0x%X; WeaponSystem.hull +0x%X; MagSize +0x%X",
       g_oHpTeam, g_oHpCe, g_oHpHp, g_oHpVital, g_oHpLastEff, g_oHpClient, g_oPjPosi, g_oGmEntity, g_oGmGunType, g_oGmReloading, g_oGmBarrels, g_oBarSkip, g_oGmSeats, g_oSeatMan, g_oGmWs, g_oWsHull, g_oGtMag);
  DLog("gun effects: crew: CombatEntity.crewManager +0x%X, CrewManager entity +0x%X crew +0x%X, CrewMember crewManager +0x%X _character +0x%X hp +0x%X downedTime +0x%X, "
       "CrewCharacter name +0x%X nameCustom +0x%X healthState +0x%X, AllowedDownedTime %p, overboardChance %p",
       g_oCeCrew, g_oMgrEnt, g_oMgrCrew, g_oCmMgr, g_oCmChar, g_oCmHp, g_oCmDowned, g_oChName, g_oChNameCustom, g_oChHealth, m_allowedDowned, g_fOverboard);
  if (g_oGmEntity < 0 || g_oLeTeam < 0 || g_oLeCode < 0 || g_offPjFrom < 0 || g_offWsSelected < 0) { Log("gun effects: a lookup failed (GunMount.entity %d, teamCode %d, shotFrom %d, selected %d); the 0.9.3 gun effects are off", g_oGmEntity, g_oLeTeam, g_offPjFrom, g_offWsSelected); return; }
  g_gfxReady = true;
  // HealthPart.hitFlash: mov rax,rsp; mov [rax+18h],rbx; mov [rax+20h],rsi; mov [rax+10h],dl (14 bytes, no rip-relative)
  static const unsigned char kHpHit[14] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x48, 0x89, 0x70, 0x20, 0x88, 0x50, 0x10 };
  // the two callers' frames as read in the disassembly: endJobs sub rsp,310h; mov [rsp+160h],r12 (the part);
  // mov [rsp+168h],rax (the shell); SplashDamageManager.FixedUpdate sub rsp,5B0h; movaps [rsp+90h],xmm13 (the splash copy);
  // cmp byte ptr [rsp+0D4h],0 (its networkOwner)
  static const unsigned char kEjSub[7] = { 0x48, 0x81, 0xEC, 0x10, 0x03, 0x00, 0x00 }, kEjPart[8] = { 0x4C, 0x89, 0xA4, 0x24, 0x60, 0x01, 0x00, 0x00 },
                             kEjShot[8] = { 0x48, 0x89, 0x84, 0x24, 0x68, 0x01, 0x00, 0x00 }, kSdSub[7] = { 0x48, 0x81, 0xEC, 0xB0, 0x05, 0x00, 0x00 },
                             kSdCopy[9] = { 0x44, 0x0F, 0x29, 0xAC, 0x24, 0x90, 0x00, 0x00, 0x00 }, kSdOwner[8] = { 0x80, 0xBC, 0x24, 0xD4, 0x00, 0x00, 0x00, 0x00 };
  void* mHit = hp ? il2cpp_class_get_method_from_name(hp, "hitFlash", 4) : nullptr;
  void* pHit = mHit ? *(void**)mHit : nullptr;
  bool frames = pHit && GfxCallsTo(0x9F4E66, pHit) && GfxCallsTo(0x982B10, pHit) && GfxCodeIs(0x9F46D7, kEjSub, 7) && GfxCodeIs(0x9F4B2C, kEjPart, 8) && GfxCodeIs(0x9F4BCB, kEjShot, 8)
             && GfxCodeIs(0x9816D2, kSdSub, 7) && GfxCodeIs(0x981A20, kSdCopy, 9) && GfxCodeIs(0x98290A, kSdOwner, 8);
  bool fields = g_oHpTeam >= 0 && g_oHpCe >= 0 && g_oHpHp >= 0 && g_oHpVital >= 0 && g_oHpLastEff >= 0 && g_oHpClient >= 0 && g_oPjPosi >= 0 && g_clsWs && g_oGmWs >= 0;
  if (!frames || !fields) Log("gun effects: %s; Marking, Giant Slayer, Opening Salvo, Close Quarters, Long Shot, Overkill, Veteran's, Brittle and Sure-Footed's stacks are off",
                              !pHit ? "HealthPart.hitFlash not found" : !fields ? "a HealthPart/Projectile field is missing" : "the code around HealthPart.hitFlash is not what this version expects (game updated?)");
  else {
    g_hpHitOrig = (fn_hpHit)Detour(pHit, (void*)&Hooked_HpHit, kHpHit, 14, "HealthPart.hitFlash");
    g_raDirect = (uintptr_t)g_ga + 0x9F4E6B; g_raSplash = (uintptr_t)g_ga + 0x982B15;
    g_gfxHitOn = g_hpHitOrig != nullptr;
    if (g_gfxHitOn) Log("gun effects: ready (HealthPart.hitFlash detoured: direct hits and blasts of your guns)");
  }
  // CrewMember.hitFlash: mov rax,rsp; movss [rax+18h],xmm2; mov [rax+8],rcx; push rbx; push rsi; push rdi (15 bytes)
  static const unsigned char kCrewHit[15] = { 0x48, 0x8B, 0xC4, 0xF3, 0x0F, 0x11, 0x50, 0x18, 0x48, 0x89, 0x48, 0x08, 0x53, 0x56, 0x57 };
  void* mCrew = cm ? il2cpp_class_get_method_from_name(cm, "hitFlash", 2) : nullptr;
  if (!mCrew || !g_fOverboard || !g_fieldStaticSet || g_oCmMgr < 0 || g_oMgrEnt < 0) Log("gun effects: Sure-Footed is off (CrewMember.hitFlash %p, overboardChance %p, static set %p)", mCrew, g_fOverboard, (void*)g_fieldStaticSet);
  else {
    g_crewHitOrig = (fn_crewHit)Detour(*(void**)mCrew, (void*)&Hooked_CrewHit, kCrewHit, 15, "CrewMember.hitFlash");
    g_gfxCrewOn = g_crewHitOrig != nullptr;
    if (g_gfxCrewOn) Log("gun effects: Sure-Footed ready (CrewMember.hitFlash detoured)");
    // crew log: the two hit callers (call bytes checked; the blast frame only when SplashDamageManager's frame matched above)
    void* pCrew = *(void**)mCrew;
    if (GfxCallsTo(0x9F5B1D, pCrew)) g_raCrewDirect = (uintptr_t)g_ga + 0x9F5B22;
    if (frames && GfxCallsTo(0x983810, pCrew)) g_raCrewSplash = (uintptr_t)g_ga + 0x983815;
  }
  CrewLogInit();
}

static bool SalvoOn(void* mount, bool direct, void* pj) {
  auto s = g_salvo.find(mount); if (s == g_salvo.end()) return false;
  if (direct) return g_salvoShots.count(pj) > 0;
  // a blast counts while the salvo is being fired, or while one of its shells is still in flight (+0.5 s for the blast
  // to land): not a fixed time after the reload, which missed slow shells and caught the next magazine's
  return s->second.active || (s->second.liveAt && GetTickCount() - s->second.liveAt < 500);
}
static void SteadyDecay() {
  DWORD now = GetTickCount();
  if (g_steadyStacks > 0 && g_steadyDecay > 0) { g_steadyStacks -= (now - g_steadyAt) / 1000.0f / g_steadyDecay; if (g_steadyStacks < 0) g_steadyStacks = 0; }
  g_steadyAt = now;
}
static int GunnerKillsOn(void* mount, const Item* it) {               // Veteran's: the bonded gunner's kills while that gunner mans it
  if (!it || it->vetCrew.empty() || g_oGmSeats < 0 || g_oSeatMan < 0 || g_oCmChar < 0) return 0;
  void* seats = *(void**)((char*)mount + g_oGmSeats); if (!seats) return 0;
  int n = ArrLen(seats);
  for (int i = 0; i < n && i < 8; i++) {
    void* seat = ArrAt(seats, i); void* man = seat ? *(void**)((char*)seat + g_oSeatMan) : nullptr;
    void* ch = man ? *(void**)((char*)man + g_oCmChar) : nullptr;
    if (ch && GfxCrewKey(ch) == it->vetCrew) return it->vetKills;
  }
  return 0;
}
