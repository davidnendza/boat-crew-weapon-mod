// co-op: core, another player's guns, elites, helpers, the session (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- co-op ---------------------------------------------------------------------------------------------------------
// What the game syncs (from the disassembly):
// - Shots: the owner's WeaponsSync.processShot sends (system index, list POSITION, ammo name, position, velocity). Every
//   other machine fires guns[(system, position, 0)] of its own copy of that boat (WeaponsSync.guns +0x108, filled once
//   by findObjects from every mount of every system) and clears the shell's networkOwner, so the copy deals no damage.
//   Unsynced, each machine adds its own found guns to every boat, so another player's boat shows (and fires) the wrong guns.
// - Damage: only the shooter's machine applies it (sendDamageToServer for objects it does not own); HP syncs from the host.
// - Spawns: what the host takes from GameManager.entityPooler is networked by BridgePoolables; a client's are not.
// - Another player's gun choice is applied with WeaponSystem.select(index, true, false), called directly from
//   WeaponsSync.UpdateMe (not through selectIndex).
// The mod sends its own data over Steam (ISteamNetworkingMessages on a channel FishySteamworks does not use), with the
// host in the middle: clients talk to the host, and the host passes their data on. Five parts, each a drops.ini switch:
//  coop_guns:    each player sends the mod's guns on their boat (positions and definitions); the others build those guns
//                at the same positions on that boat, so its shots and looks match.
//  coop_elites:  the host rolls Rare/Elite enemies and sends them; clients show them (size, tag, loot when they sink it).
//  coop_visuals: chain leaps and homing play on copies of other players' shots (see IsCopyShot). A shot the mod makes
//                from a copy is a copy too, so nothing hits twice; that part is always on.
//  coop_helpers: what a client's gun calls in is spawned by the host (so the game shares it) and sized for everyone.
//  coop_torpedoes: the found torpedoes in each player's launchers ride the guns message (L/Q lines); the others build
//                them at the same positions of that boat's launchers, so the copies they see fire as they are.
struct SteamIdentity { int type; int size; unsigned long long id; char rest[120]; };   // SteamNetworkingIdentity
static_assert(sizeof(SteamIdentity) == 136, "SteamNetworkingIdentity layout");
static void* (*s_snmGet)() = nullptr;
static int (*s_snmSend)(void*, const SteamIdentity*, const void*, unsigned, int, int) = nullptr;
static int (*s_snmRecv)(void*, int, void**, int) = nullptr;
static bool (*s_snmAccept)(void*, const SteamIdentity*) = nullptr;
static void (*s_snmRelease)(void*) = nullptr;
static const int kCoopChannel = 0x42435750;                           // 'BCWP'
static const char* kCoopBuild = __DATE__ " " __TIME__;               // two builds of one version are told apart in the log

struct CoopPeer { int id; unsigned long long steam; bool heard, mismatch; DWORD helloAt, gunsAt; unsigned long long gunsHash; bool elites, buildWarned; };   // elites: it sends its Rare/Elite rolls
static std::vector<CoopPeer> g_peers;                                 // host: every other player; client: the host
static int g_myId = -1;                                               // FishNet ClientId of this machine
static DWORD g_coopTickAt = 0, g_coopSessionAt = 0, g_coopGunsAt = 0, g_coopFixAt = 0, g_coopPendingAt = 0;
static int g_coopFaults = 0;
static unsigned long long g_myGunsHash = 0;
static void* g_coopEng = nullptr;

static bool SteamReady() {
  static int state = 0;
  if (state) return state > 0;
  HMODULE h = GetModuleHandleA("steam_api64.dll");
  if (!h) return false;                                                // not loaded yet
  s_snmGet = (void* (*)())GetProcAddress(h, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
  s_snmSend = (int (*)(void*, const SteamIdentity*, const void*, unsigned, int, int))GetProcAddress(h, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
  s_snmRecv = (int (*)(void*, int, void**, int))GetProcAddress(h, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
  s_snmAccept = (bool (*)(void*, const SteamIdentity*))GetProcAddress(h, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
  s_snmRelease = (void (*)(void*))GetProcAddress(h, "SteamAPI_SteamNetworkingMessage_t_Release");
  state = s_snmGet && s_snmSend && s_snmRecv && s_snmAccept && s_snmRelease ? 1 : -1;
  if (state < 0) Log("co-op: steam_api64.dll has no networking messages functions; nothing is synced in co-op");
  return state > 0;
}
static SteamIdentity SteamUser(unsigned long long id) { SteamIdentity s = {}; s.type = 16; s.size = 8; s.id = id; return s; }   // k_ESteamNetworkingIdentityType_SteamID
static std::string CoopHeader(const char* type, int origin) { return std::string("BCWM|") + kModVersion + "|" + type + "|" + std::to_string(origin) + "|" + kCoopBuild + "\n"; }
static bool CoopSend(unsigned long long to, const std::string& msg) {
  void* snm = s_snmGet ? s_snmGet() : nullptr;
  if (!snm || !to) return false;
  SteamIdentity id = SteamUser(to);
  int r = s_snmSend(snm, &id, msg.data(), (unsigned)msg.size(), 8 | 32, kCoopChannel);   // reliable, restart a broken session
  if (r != 1) { static int fails = 0; if (++fails <= 5) Log("co-op: sending %u bytes to Steam user %llu failed (result %d)", (unsigned)msg.size(), to, r); return false; }
  g_cc.msgsOut++; g_cc.bytesOut += (int)msg.size();
  return true;
}
static int CoopSendAll(const std::string& msg, unsigned long long except = 0) {
  int n = 0;
  for (const CoopPeer& p : g_peers) if (p.heard && !p.mismatch && p.steam != except && CoopSend(p.steam, msg)) n++;
  return n;
}
// Heal streaks in co-op (coop_visuals): a kill that repairs your hull sends the wreck and boat positions (HEAL), and the
// others draw the same streak (fixed ends: their copy of your boat is not followed). Looks only.
static void CoopSendHeal(const V3& w, const V3& me) {
  if (!g_coopRole || !g_coopVisuals) return;
  char b[128]; snprintf(b, sizeof b, "%.1f %.1f %.1f %.1f %.1f %.1f\n", w.x, w.y, w.z, me.x, me.y, me.z);
  if (CoopSendAll(CoopHeader("HEAL", g_myId) + b) > 0) g_cc.healsSent++;
}
static void CoopHealIn(int origin, const std::string& body) {
  float v[6]; const char* p = body.c_str();
  for (int i = 0; i < 6; i++) {
    char* e = nullptr; v[i] = strtof(p, &e);
    if (e == p || !std::isfinite(v[i]) || fabsf(v[i]) > 1e6f) { DLog("co-op: a HEAL message from player %d did not parse; ignored", origin); return; }
    p = e;
  }
  g_cc.healsIn++;
  V3 w{ v[0], v[1], v[2] }, me{ v[3], v[4], v[5] };
  if (!StreakAdd(0, w, me, nullptr)) return;
  g_streakHealIn++;
  if (g_streakLogsH < 10) { g_streakLogsH++; Log("effect streaks: heal streak from player %d's kill, %.0f m", origin, CrewDist(w, me)); }
}
static unsigned long long HashText(const std::string& s) { unsigned long long h = 1469598103934665603ULL; for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } return h; }

// InstanceFinder.NetworkManager writes "NetworkManager not found in any open scenes." to Player.log on every call where
// there is none, so whether a game is hosted or joined is read from the Steam transport itself (FishySteamworks).
static void *m_fsLocalState = nullptr, *m_ifClientManager = nullptr, *g_fWsReg = nullptr, *g_fOsReg = nullptr, *m_nbIsSpawned = nullptr;
static void *m_nbOwnerId = nullptr, *m_nbObjectId = nullptr, *m_fsState = nullptr, *m_fsAddress = nullptr, *g_clsFishy = nullptr, *m_wsSelectMI = nullptr;
static int g_offNcId = -1, g_offWsWM = -1, g_offWsGuns = -1, g_offOsEntity = -1, g_offFsClient = -1, g_offFsServer = -1;
static bool CoopLookups() {
  static int state = 0;
  if (state) return state > 0;
  state = -1;
  void* inf = FindClass("FishNet", "InstanceFinder");
  m_ifClientManager = inf ? FindMethod(inf, "get_ClientManager", 0) : nullptr;   // only read once a co-op game is running
  void* wsk = FindClass("Core.Multiplayer", "WeaponsSync"); void* osk = FindClass("Core.Multiplayer", "ObjectSync");
  g_fWsReg = wsk ? il2cpp_class_get_field_from_name(wsk, "register") : nullptr; g_fOsReg = osk ? il2cpp_class_get_field_from_name(osk, "register") : nullptr;
  g_offWsWM = FieldOffset(wsk, "weaponManager"); g_offWsGuns = FieldOffset(wsk, "guns"); g_offOsEntity = FieldOffset(osk, "targetLiveEntity");
  void* nb = FindClass("FishNet.Object", "NetworkBehaviour");
  m_nbOwnerId = nb ? FindMethod(nb, "get_OwnerId", 0) : nullptr; m_nbObjectId = nb ? FindMethod(nb, "get_ObjectId", 0) : nullptr;
  m_nbIsSpawned = nb ? FindMethod(nb, "get_IsSpawned", 0) : nullptr;
  g_offNcId = FieldOffset(FindClass("FishNet.Connection", "NetworkConnection"), "ClientId");
  g_clsFishy = FindClass("FishySteamworks", "FishySteamworks");
  m_fsState = g_clsFishy ? FindMethod(g_clsFishy, "GetConnectionState", 1, "System.Int32") : nullptr;   // a client: RemoteConnectionState
  m_fsLocalState = g_clsFishy ? FindMethod(g_clsFishy, "GetConnectionState", 1, "System.Boolean") : nullptr;   // this machine: server or client side
  m_fsAddress = g_clsFishy ? FindMethod(g_clsFishy, "GetConnectionAddress", 1) : nullptr;
  g_offFsClient = FieldOffset(g_clsFishy, "_client"); g_offFsServer = FieldOffset(g_clsFishy, "_server");
  void* wsc = FindClass("Core", "WeaponSystem"); m_wsSelectMI = wsc ? FindMethod(wsc, "select", 3, "System.Int32") : nullptr;
  bool ok = m_fsLocalState && m_ifClientManager && g_fWsReg && g_fOsReg && g_offWsWM >= 0 && g_offWsGuns >= 0 && g_offOsEntity >= 0 && m_nbIsSpawned &&
            m_nbOwnerId && m_nbObjectId && g_offNcId >= 0 && g_clsFishy && m_fsState && m_fsAddress && g_offFsClient >= 0 && g_offFsServer >= 0 && m_wsSelectMI;
  if (!ok) {
    Log("co-op: a lookup failed (local state %p clients %p WeaponsSync %p ObjectSync %p wm %d guns %d entity %d spawned %p owner %p object %p ClientId %d Fishy %p state %p address %p client %d server %d select %p); nothing is synced in co-op",
        m_fsLocalState, m_ifClientManager, g_fWsReg, g_fOsReg, g_offWsWM, g_offWsGuns, g_offOsEntity, m_nbIsSpawned, m_nbOwnerId, m_nbObjectId, g_offNcId, g_clsFishy, m_fsState, m_fsAddress, g_offFsClient, g_offFsServer, m_wsSelectMI);
    return false;
  }
  state = 1;
  return true;
}

// Every element of a HashSet<T> of objects (layout: HashSetLayoutOf).
static void SetItems(void* set, std::vector<void*>& out) {
  out.clear();
  if (!set) return;
  HashSetLayout l = HashSetLayoutOf(il2cpp_object_get_class(set));
  if (!l.ok()) return;
  void* arr = *(void**)((char*)set + l.slots);
  if (!arr) return;
  int n = ArrLen(arr);
  if (l.count >= 0) { int c = *(int*)((char*)set + l.count); if (c >= 0 && c < n) n = c; }
  for (int i = 0; i < n; i++) { void* e = *(void**)((char*)arr + 0x20 + (size_t)i * l.elem + l.val); if (e) out.push_back(e); }
}

// Every player boat the game syncs (a WeaponsSync), with its owner's ClientId and its WeaponManager.
struct NetBoat { void* sync; int owner; void* wm; };
static std::vector<NetBoat> NetBoats() {
  std::vector<NetBoat> out; std::vector<void*> all; void* set = nullptr;
  il2cpp_field_static_get_value(g_fWsReg, &set);
  SetItems(set, all);
  for (void* s : all) if (Alive(s)) out.push_back({ s, BoxedInt(Invoke(m_nbOwnerId, s)), *(void**)((char*)s + g_offWsWM) });
  return out;
}
static int MyClientId() {
  void* cm = Invoke(m_ifClientManager, nullptr);
  static int offConn = -2;
  if (offConn == -2 && cm) { offConn = FieldOffset(il2cpp_object_get_class(cm), "Connection"); }
  void* nc = cm && offConn >= 0 ? *(void**)((char*)cm + offConn) : nullptr;
  return nc ? *(int*)((char*)nc + g_offNcId) : -1;
}
// A network object's id, or -1 while it is not spawned: a pooled one keeps its last id, and FishNet's unset id is 65535.
static int SpawnedObjectId(void* os) {
  void* sp = Invoke(m_nbIsSpawned, os);
  if (!sp || !*(bool*)((char*)sp + 0x10)) return -1;
  int id = BoxedInt(Invoke(m_nbObjectId, os));
  return id < 0 || id >= 65535 ? -1 : id;
}
static int ObjectIdOf(void* ent) {                                    // the id of the network object that shares ent, or -1
  std::vector<void*> all; void* set = nullptr;
  il2cpp_field_static_get_value(g_fOsReg, &set);
  SetItems(set, all);
  for (void* os : all) if (*(void**)((char*)os + g_offOsEntity) == ent) { int id = SpawnedObjectId(os); if (id >= 0) return id; }
  return -1;
}
static void ObjectIdMap(std::unordered_map<int, void*>& out) {         // network object id -> the entity it shares here
  out.clear(); std::vector<void*> all; void* set = nullptr;
  il2cpp_field_static_get_value(g_fOsReg, &set);
  SetItems(set, all);
  for (void* os : all) {
    void* ent = *(void**)((char*)os + g_offOsEntity); if (!ent) continue;
    int id = SpawnedObjectId(os); if (id < 0) continue;
    auto e = out.find(id);                                             // two with one id: the living entity wins
    if (e == out.end() || (g_offLeAlive >= 0 && !*(bool*)((char*)e->second + g_offLeAlive) && *(bool*)((char*)ent + g_offLeAlive))) out[id] = ent;
  }
}

static void* g_fishy = nullptr; static void* g_fishyHandle = nullptr; static DWORD g_fishyAt = 0;
static void* Fishy() {                                                // the FishySteamworks transport (Steam P2P)
  if (g_fishy && Alive(g_fishy)) return g_fishy;
  if (g_fishyAt && GetTickCount() - g_fishyAt < 5000) return nullptr;
  g_fishyAt = GetTickCount();
  Unpin(g_fishyHandle); g_fishy = g_fishyHandle = nullptr;
  static void* mFind = nullptr;
  if (!mFind) { void* o = FindClass("UnityEngine", "Object"); mFind = o ? FindMethod(o, "FindObjectsByType", 3, "System.Type") : nullptr; }
  if (!mFind) return nullptr;
  int inactive = 0, sort = 0; void* a[3] = { il2cpp_type_get_object(il2cpp_class_get_type(g_clsFishy)), &inactive, &sort };
  void* arr = Invoke(mFind, nullptr, a);
  if (arr && ArrLen(arr) > 0) { g_fishy = ArrAt(arr, 0); g_fishyHandle = Pin(g_fishy); }
  return g_fishy;
}

// ---- another player's guns (coop_guns) ----
struct RemoteSlot { std::string family; std::map<int, std::string> pos; };   // position -> mod gun name
struct RemoteBoat { std::map<int, RemoteSlot> slots; std::map<int, RemoteSlot> launchers; std::vector<Item> items; std::map<std::string, unsigned long long> stamps; unsigned long long hash = 0; std::string raw; };
static std::map<int, RemoteBoat> g_remote;                           // owner ClientId -> what they sent
// Both maps hold GC handles on their objects, so a destroyed boat's address is never reused while it is a key here.
struct RemoteWs { void* sync; int sys; int owner; void* hWs; void* hSync; };
static std::unordered_map<void*, RemoteWs> g_remoteWs;               // weapon system of another player's boat -> where it is
static DWORD g_remoteWsAt = 0;
struct RemoteMount { int owner; std::string name; unsigned long long stamp; Item item; bool hidden; void* handle; };
static std::unordered_map<void*, RemoteMount> g_remoteMounts;        // guns this mod built for other players' boats
static std::set<std::pair<void*, int>> g_remoteFailed;               // (system, position) that could not be built: not retried
static int g_coopGunLogs = 0;
static void ClearRemoteWs() { for (auto& e : g_remoteWs) { Unpin(e.second.hWs); Unpin(e.second.hSync); } g_remoteWs.clear(); }
static void ClearRemoteMounts() { for (auto& e : g_remoteMounts) Unpin(e.second.handle); g_remoteMounts.clear(); g_remoteFailed.clear(); }
static void ForgetRemoteMount(void* m) { auto r = g_remoteMounts.find(m); if (r != g_remoteMounts.end()) { Unpin(r->second.handle); g_remoteMounts.erase(r); } }

static const Item* ItemOfMount(void* mount) {
  if (!g_remoteMounts.empty()) { auto r = g_remoteMounts.find(mount); if (r != g_remoteMounts.end()) return r->second.hidden ? nullptr : &r->second.item; }
  if (IsHiddenMount(mount)) return nullptr;                            // a placeholder is not its gun
  return FindItem(StripTags(NameOf(mount)));
}
static bool IsRemoteMount(void* mount) { return !g_remoteMounts.empty() && g_remoteMounts.count(mount) > 0; }

static void RefreshRemoteWs() {
  ClearRemoteWs(); ClearRemoteLaunchers(); g_remoteWsAt = GetTickCount();
  if (g_myId < 0 || g_offWMSystems < 0) { ClearRemoteMounts(); ClearRemoteTorps(); return; }   // without our own id every boat would look like theirs
  std::unordered_set<void*> live;
  void* myCraft = PlayerCraft(); void* myWm = myCraft && g_offCraftWM >= 0 ? *(void**)((char*)myCraft + g_offCraftWM) : nullptr;
  for (const NetBoat& b : NetBoats()) {
    if (b.owner < 0 || b.owner == g_myId || !b.wm || !Alive(b.wm) || b.wm == myWm) continue;   // never this player's own boat
    void* arr = *(void**)((char*)b.wm + g_offWMSystems);
    for (int i = 0; arr && i < ArrLen(arr) && i < 32; i++) {
      void* ws = ArrAt(arr, i); if (!ws || !Alive(ws)) continue;
      g_remoteWs[ws] = { b.sync, i, b.owner, Pin(ws), Pin(b.sync) };
      void* mounts = *(void**)((char*)ws + g_offMounts);
      for (int j = 0; j < ListCount(mounts); j++) live.insert(ListAt(mounts, j));
    }
    void* la = g_coopTorps && g_tp.ok ? *(void**)((char*)b.wm + g_tp.wmLaunchers) : nullptr;   // index i as UpdateMe and createTorpedo use it
    for (int i = 0; la && i < ArrLen(la) && i < 32; i++) {
      void* l = ArrAt(la, i); if (!l || !Alive(l)) continue;
      if (!g_remoteLaunchers.count(l)) g_remoteLaunchers[l] = { b.owner, i, Pin(l) };
      void* list = *(void**)((char*)l + g_tp.lTorpedos);
      for (int j = 0; j < ListCount(list); j++) live.insert(ListAt(list, j));
    }
  }
  for (auto it = g_remoteMounts.begin(); it != g_remoteMounts.end(); ) {   // boat gone
    if (!live.count(it->first)) { Unpin(it->second.handle); it = g_remoteMounts.erase(it); } else ++it;
  }
  for (auto it = g_remoteTiers.begin(); it != g_remoteTiers.end(); ) {     // launcher gone, or the tier left its list
    if (!live.count(it->first)) { Unpin(it->second.h); it = g_remoteTiers.erase(it); } else ++it;
  }
}

// My boat's mod guns, as the others need them: "S <system> <family>" and "P <position> <name>" lines, then ITEMS and
// the definitions. The boat is the one the game syncs for this machine (its WeaponsSync), else Player.playerCraft.
// Found torpedoes (coop_torpedoes) follow as "L <launcher> <family>" and "Q <position> <name>" lines: new letters, so an
// older parser skips them.
static std::string MyGunsText(int* positions, int* guns, int* tpos, int* torps) {
  void* wm = nullptr;
  for (const NetBoat& b : NetBoats()) if (g_myId >= 0 && b.owner == g_myId && b.wm) wm = b.wm;
  if (!wm) { void* craft = PlayerCraft(); wm = craft && g_offCraftWM >= 0 ? *(void**)((char*)craft + g_offCraftWM) : nullptr; }
  void* arr = wm && g_offWMSystems >= 0 ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  std::string text, defs; std::set<std::string> described;
  *positions = *guns = *tpos = *torps = 0;
  for (int i = 0; g_coopGuns && arr && i < ArrLen(arr) && i < 32; i++) {
    void* ws = ArrAt(arr, i); if (!ws) continue;
    void* order = *(void**)((char*)ws + g_offOrder); void* mounts = *(void**)((char*)ws + g_offMounts);
    if (!order || !mounts) continue;
    std::string family = FamilyOf(order), lines;
    for (int j = 1; j < ListCount(mounts) && j < 512; j++) {
      void* m = ListAt(mounts, j); if (!m) continue;
      std::string nm = StripTags(NameOf(m));
      const Item* it = FindItemIn(nm, family);
      if (!it || it->held || it->retired) continue;
      lines += "P " + std::to_string(j) + " " + nm + "\n"; (*positions)++;
      if (described.insert(nm).second) { defs += ItemText(*it, true); (*guns)++; }
    }
    if (!lines.empty()) text += "S " + std::to_string(i) + " " + family + "\n" + lines;
  }
  void* la = g_coopTorps && g_tp.ok && wm ? *(void**)((char*)wm + g_tp.wmLaunchers) : nullptr;
  for (int i = 0; la && i < ArrLen(la) && i < 32; i++) {
    void* l = ArrAt(la, i); if (!l) continue;
    void* order = *(void**)((char*)l + g_offOrder); if (!order) continue;
    void* list = *(void**)((char*)l + g_tp.lTorpedos);
    std::string family = FamilyOf(order), lines;
    for (int j = 1; j < ListCount(list) && j < 512; j++) {
      void* tier = ListAt(list, j);
      if (!tier || !TierBase(tier).empty()) continue;                 // stock: the same on every machine
      std::string nm = TierName(tier);
      const Item* it = IsHiddenTier(tier) ? nullptr : FindItemIn(nm, family);
      // a placeholder, a held or missing item, and one worn out in an earlier battle fire as the stock torpedo here
      // (TorpBegin): "Q <position> -" says so, so the other machine puts a placeholder there and not its own found
      // torpedo (InjectTorpedoes may have filled their copy of this launcher before it was known to be ours). One worn
      // out this battle keeps firing as itself.
      if (!it || it->held || (it->retired && !std::any_of(g_torpPaid.begin(), g_torpPaid.end(), [&](const std::pair<std::string, void*>& p) { return p.first == nm; }))) {
        std::string base;                                              // the stock it fires as: "Q <position> - <base>"
        if (void* ty = TierType(tier)) {
          auto oc = g_torpOfType.find(ty);
          void* st = oc != g_torpOfType.end() && oc->second ? oc->second->stock : ty;
          for (int k = 0; k < ListCount(list) && base.empty(); k++) { void* s2 = ListAt(list, k); if (s2 && !TierBase(s2).empty() && TierType(s2) == st) base = TierBase(s2); }
        }
        lines += "Q " + std::to_string(j) + " -" + (base.empty() ? std::string() : " " + base) + "\n"; continue;
      }
      lines += "Q " + std::to_string(j) + " " + nm + "\n"; (*tpos)++;
      if (described.insert(nm).second) { Item c = *it; c.retired = false; defs += ItemText(c, true); (*torps)++; }
    }
    if (!lines.empty()) text += "L " + std::to_string(i) + " " + family + "\n" + lines;
  }
  return text + "ITEMS\n" + defs;
}
static void CoopSendGuns() {
  if (g_myId < 0) return;                                               // which boat is ours is not known yet
  int positions = 0, guns = 0, tpos = 0, torps = 0;
  std::string body = MyGunsText(&positions, &guns, &tpos, &torps);
  unsigned long long h = HashText(body);
  bool changed = h != g_myGunsHash; g_myGunsHash = h;
  DWORD now = GetTickCount(); int sent = 0;
  std::string msg = CoopHeader("GUNS", g_myId) + body;
  for (CoopPeer& p : g_peers) {
    if (!p.heard || p.mismatch || (p.gunsHash == h && now - p.gunsAt < 60000)) continue;
    if (CoopSend(p.steam, msg)) { p.gunsHash = h; p.gunsAt = now; sent++; }
  }
  if (sent) g_cc.gunsSent++;
  if (sent && changed) Log("co-op guns: sent your boat's guns to %d player(s): %d positions, %d guns described; %d found torpedoes in %d launcher positions (%u bytes)",
                           sent, positions, guns, torps, tpos, (unsigned)msg.size());
}
// Another player's boat, as they sent it.
static void CoopGunsIn(int origin, const std::string& body, const std::string& raw) {
  if (origin < 0 || origin == g_myId) return;
  size_t ip = body.compare(0, 6, "ITEMS\n") == 0 ? 0 : body.find("\nITEMS\n");   // a line of its own, not a gun name ending in ITEMS
  if (ip == std::string::npos) { Log("co-op guns: a message from player %d had no ITEMS line; ignored", origin); return; }
  if (ip > 0) ip++;
  RemoteBoat rb; rb.raw = raw; rb.hash = HashText(body);
  auto old = g_remote.find(origin);
  if (old != g_remote.end() && old->second.hash == rb.hash) return;     // the same as last time
  RemoteSlot* cur = nullptr; RemoteSlot* curL = nullptr; int positions = 0, tposIn = 0;   // curL: the launcher of L/Q lines
  for (size_t at = 0; at < ip; ) {
    size_t nl = body.find('\n', at); if (nl == std::string::npos || nl > ip) nl = ip;
    std::string l = body.substr(at, nl - at); at = nl + 1;
    if (l.size() > 2 && l[0] == 'S' && l[1] == ' ') {
      int sys = atoi(l.c_str() + 2); size_t sp = l.find(' ', 2); curL = nullptr;
      if (sys < 0 || sys >= 32 || sp == std::string::npos) { cur = nullptr; continue; }
      cur = &rb.slots[sys]; cur->family = l.substr(sp + 1);
    } else if (cur && l.size() > 2 && l[0] == 'P' && l[1] == ' ') {
      int pos = atoi(l.c_str() + 2); size_t sp = l.find(' ', 2);
      if (pos <= 0 || pos >= 512 || sp == std::string::npos) continue;
      cur->pos[pos] = l.substr(sp + 1); positions++;
    } else if (g_coopTorps && l.size() > 2 && l[0] == 'L' && l[1] == ' ') {
      int li = atoi(l.c_str() + 2); size_t sp = l.find(' ', 2); cur = nullptr;
      if (li < 0 || li >= 32 || sp == std::string::npos || sp + 1 >= l.size()) { curL = nullptr; continue; }
      curL = &rb.launchers[li]; curL->family = l.substr(sp + 1);
    } else if (curL && l.size() > 2 && l[0] == 'Q' && l[1] == ' ') {
      int pos = atoi(l.c_str() + 2); size_t sp = l.find(' ', 2);
      if (pos <= 0 || pos >= 512 || sp == std::string::npos) continue;
      std::string v = l.substr(sp + 1);
      if (v.size() > 2 && v[0] == '-' && v[1] == ' ') v = "-|" + v.substr(2);   // a placeholder and the stock base it fires as
      curL->pos[pos] = v; tposIn++;
    }
  }
  ParseItemsInto(rb.items, body.substr(ip + 6), "co-op guns", false);
  // another machine's numbers, bounded to what this mod could roll: a garbled or edited value must not wreck this one
  // (huge gun scale, a chain that never stops leaping, shells that never expire)
  for (Item& it : rb.items) {
    for (size_t k = 0; k < it.ops.size(); ) {
      Op& o = it.ops[k];
      if (!std::isfinite(o.value)) { it.ops.erase(it.ops.begin() + k); continue; }
      auto lim = [&](const char* f, double lo, double hi) { if (o.field == f) o.value = o.value < lo ? lo : (o.value > hi ? hi : o.value); };
      if (o.fx) { lim("gunScale", 0.3, 3); lim("chain", 0, 8); lim("chainChance", 0, 0.95); lim("chainDecay", 0, 0.95); lim("seek", 0, 360); lim("paint", 0, kPaintN - 1);
                  lim("torpSeek", 0, 360); lim("chomp", 0, 2); lim("leap", 0, 30); lim("skim", 0, 1); lim("crawl", 0, 1); lim("priceCut", 0, 1); lim("smoke", 0, 1); lim("raft", 0, 1); lim("dud", 0, 1); lim("torpScale", 0.5, 2);
                  lim("crewOffsetX", -3, 3); lim("crewOffsetY", -3, 3); lim("crewOffsetZ", -3, 3);
                  lim("pivotOffsetX", -3, 3); lim("pivotOffsetY", -3, 3); lim("pivotOffsetZ", -3, 3); }
      else if (o.mul) { lim("FrameLifeTime", 0.05, 10); lim("projectileWidth", 0.05, 6); lim("projectileLength", 0.05, 6); lim("roundsPerMinute", 0.01, 10);
                        lim("damage", 0.1, 10); lim("damageRange", 0.1, 5); lim("thrust", 0.1, 5); lim("terminalVelocity", 0.1, 5); lim("despawnDistance", 0.1, 5); lim("hitboxRadius", 0.1, 5); }
      else {                                                           // the torpedoes' set. values; a set. on a field this mod
        if (!o.proj && !o.mount && !o.sphere && (o.field == "damage" || o.field == "damageRange" || o.field == "thrust" || o.field == "terminalVelocity" ||
            o.field == "despawnDistance" || o.field == "hitboxRadius")) { it.ops.erase(it.ops.begin() + k); continue; }   // only rolls as mul. is dropped
        lim("detectionByEntityChanceMultiplier", 0, 10); lim("idleDepth", 0.3, 20); lim("gunHappy", -90, 90); lim("gunSad", -90, 90);
      }
      k++;
    }
  }
  for (const Item& it : rb.items) rb.stamps[it.name] = HashText(ItemText(it, true));
  std::string owners; for (const NetBoat& b : NetBoats()) owners += std::to_string(b.owner) + " ";
  int torpDefs = 0; for (const Item& it : rb.items) if (IsTorpFamily(it.family)) torpDefs++;
  Log("co-op guns: player %d's boat: %d positions in %d slots, %d guns described; %d torpedo positions in %d launchers, %d torpedoes described (boats here belong to players %s)",
      origin, positions, (int)rb.slots.size(), (int)rb.items.size() - torpDefs, tposIn, (int)rb.launchers.size(), torpDefs, owners.c_str());
  if (!rb.launchers.empty()) g_cc.torpsIn++;
  g_remote[origin] = std::move(rb);
  g_cc.gunsIn++;
  g_coopFixAt = 0;                                                     // put them on the boat now
}

// Puts the owner's gun at position j of a weapon system on their boat, if it is not there yet. True if it changed.
// The mounts list only ever gets the mod's guns replaced (a stock gun is never touched), positions the owner has but
// this machine lacks are filled with hidden placeholders first, and the shot sync's guns entry is pointed at the new gun.
static bool EnsureRemoteGun(void* ws, int j) {
  auto rw = g_remoteWs.find(ws);
  if (rw == g_remoteWs.end() || !Alive(ws) || !Alive(rw->second.sync) || g_remoteFailed.count({ ws, j })) return false;
  int owner = rw->second.owner, sys = rw->second.sys;
  void* syncObj = rw->second.sync;                                     // copied: the managed calls below could refresh g_remoteWs
  auto rbi = g_remote.find(owner);
  if (rbi == g_remote.end()) return false;
  RemoteBoat& rb = rbi->second;
  auto sl = rb.slots.find(sys);
  if (sl == rb.slots.end()) return false;
  auto ps = sl->second.pos.find(j);
  if (ps == sl->second.pos.end()) return false;                       // no gun of the mod's there on their boat
  void* order = *(void**)((char*)ws + g_offOrder); void* mounts = *(void**)((char*)ws + g_offMounts);
  if (!order || !mounts) return false;
  std::string family = FamilyOf(order);
  if (family != sl->second.family) {
    static std::set<std::string> told;
    if (told.insert(std::to_string(owner) + "|" + family).second) Log("co-op guns: player %d's slot %d is '%s' here but '%s' for them; left alone", owner, sys, family.c_str(), sl->second.family.c_str());
    return false;
  }
  const std::string& name = ps->second;
  const Item* def = nullptr;
  for (const Item& it : rb.items) if (it.name == name) { def = &it; break; }
  unsigned long long stamp = def ? rb.stamps[name] : 0;
  int cnt = ListCount(mounts);
  void* cur = j < cnt ? ListAt(mounts, j) : nullptr;
  std::string was = cur ? StripTags(NameOf(cur)) : std::string("(nothing)");
  if (cur) {
    auto rm = g_remoteMounts.find(cur);
    void* cgt = *(void**)((char*)cur + g_offGunType);                  // the mod's guns carry an overrideTag of WM_<name>
    bool mods = cgt && g_offOverrideTag >= 0 && strncmp(Str(*(void**)((char*)cgt + g_offOverrideTag)), "WM_", 3) == 0;
    if (rm != g_remoteMounts.end()) { if (rm->second.owner == owner && rm->second.name == name && rm->second.stamp == stamp) return false; }   // already theirs
    else if (!mods) {                                                  // a stock gun at a position they use: never replaced
      static std::set<std::string> told;
      if (told.insert(std::to_string(owner) + "|" + family + "|" + std::to_string(j)).second) Log("co-op guns: position %d of player %d's slot '%s' holds the stock '%s' here; left alone", j, owner, family.c_str(), was.c_str());
      return false;
    }
  }
  while (ListCount(mounts) < j) {                                      // they have more guns in this slot than this machine
    Item gap; gap.name = "co-op placeholder"; gap.retired = true;
    void* src = FirstRealGunMount(mounts); void* g = src ? AddCloneMount(ws, mounts, src, gap) : nullptr;
    if (!g) return false;
    g_remoteMounts[g] = { owner, gap.name, 0, Item(), true, Pin(g) };
  }
  void* source = nullptr;
  for (int i = 0; def && i < ListCount(mounts) && !source; i++) {     // their gun's stock base on this boat (stock guns come first)
    void* m = ListAt(mounts, i); void* gt = m ? *(void**)((char*)m + g_offGunType) : nullptr;
    if (gt && NameOf(gt) == def->base) source = m;
  }
  Item hidden; hidden.name = name; hidden.retired = true;
  if (!source) {
    if (def) Log("co-op guns: player %d's '%s' is based on '%s', which slot '%s' does not have here; a hidden placeholder keeps the position", owner, name.c_str(), def->base.c_str(), family.c_str());
    def = nullptr; source = FirstRealGunMount(mounts);
  }
  if (!source) return false;
  Item use = def ? *def : hidden;                                       // one definition per gun name is sent, so a side-grade
  use.family = family;                                                 // equipped in two slot families takes this slot's family
  void* m2 = AddCloneMount(ws, mounts, source, use);
  if (!m2) return false;
  int last = ListCount(mounts) - 1;
  if (j < last) {                                                      // it was added at the end: move it to position j
    if (!ListSetAt(mounts, j, m2) || !ListRemoveAt(mounts, last)) {     // given up on: retrying every second would only add copies
      if (ListCount(mounts) == last + 1 && ListAt(mounts, last) == m2) ListRemoveAt(mounts, last);
      g_remoteFailed.insert({ ws, j });
      Log("co-op guns: could not move '%s' to position %d of player %d's slot '%s'; that position keeps this machine's gun", name.c_str(), j, owner, family.c_str());
      return false;
    }
    if (cur) {                                                         // the gun it replaces leaves the list; select() no longer hides it
      void* det = g_offDetailables >= 0 ? *(void**)((char*)ws + g_offDetailables) : nullptr;   // nor the slot's detail list (as RealizePlaceholder)
      for (int i = ListCount(det) - 1; i >= 0; i--) if (ListAt(det, i) == cur) { ListRemoveAt(det, i); break; }
      void* go = Invoke(m_getGameObject, cur); bool off = false; void* a[1] = { &off };
      if (go) Invoke(m_setActive, go, a);
      ForgetRemoteMount(cur);
    }
  }
  void* guns = *(void**)((char*)syncObj + g_offWsGuns);                 // the shot sync finds a gun by (system, position, 0)
  void* setItem = guns ? il2cpp_class_get_method_from_name(il2cpp_object_get_class(guns), "set_Item", 2) : nullptr;
  if (setItem) { int key[3] = { sys, j, 0 }; void* a[2] = { key, m2 }; Invoke(setItem, guns, a); }
  else Log("co-op guns: no shot table on player %d's boat; their shots from '%s' may not show", owner, name.c_str());
  g_remoteMounts[m2] = { owner, name, stamp, def ? use : Item(), def == nullptr, Pin(m2) };
  g_cc.gunsBuilt++;
  if (++g_coopGunLogs <= 40) Log("co-op guns: player %d's slot '%s' position %d is now '%s'%s (was '%s')", owner, family.c_str(), j, name.c_str(), def ? "" : " (hidden)", was.c_str());
  return true;
}
// The game applies another player's choice by calling select directly (WeaponsSync.UpdateMe), so select itself is
// hooked: the 14 bytes at its start (four plain moves into the home slots) become a jump here.
typedef void* (*fn_wsSelect)(void*, int, bool, bool, void*);
static fn_wsSelect g_wsSelectOrig = nullptr;
static void CoopBeforeSelect(void* ws, int index) {
  if (!g_remoteWs.count(ws)) {
    if (GetTickCount() - g_remoteWsAt < 250) return;                  // most selects are on this machine's own or AI boats
    RefreshRemoteWs();
    if (!g_remoteWs.count(ws)) return;
  }
  EnsureRemoteGun(ws, index);
}
static void* Hooked_WsSelect(void* ws, int index, bool first, bool force, void* method) {
  if (g_coopRole && g_coopGuns && g_coopFaults >= 0 && index > 0 && g_ready) {
    __try { CoopBeforeSelect(ws, index); }
    __except (FaultFilter("co-op guns", GetExceptionInformation())) { g_coopFaults = -1; g_coopRole = 0; ClearRemoteWs(); ClearRemoteMounts(); ClearRemoteTorps(); }
  }
  return g_wsSelectOrig(ws, index, first, force, method);
}
static void InstallCoopSelect() {
  static bool tried = false;
  if (tried) return;
  tried = true;
  static const unsigned char kSelect[14] = { 0x48, 0x8B, 0xC4, 0x44, 0x88, 0x48, 0x20, 0x44, 0x88, 0x40, 0x18, 0x89, 0x50, 0x10 };
  g_wsSelectOrig = (fn_wsSelect)Detour(*(void**)m_wsSelectMI, (void*)&Hooked_WsSelect, kSelect, 14, "WeaponSystem.select");
  if (g_wsSelectOrig) DLog("co-op guns: hooked WeaponSystem.select (rva 0x%llX)", (unsigned long long)((uintptr_t)*(void**)m_wsSelectMI - (uintptr_t)g_ga));
  else Log("co-op guns: other players' guns are fixed about a second after they pick them (the select hook failed)");
}
// Once a second: a gun another player has selected that is not theirs yet is built and selected again, so it shows.
static void CoopFixSelected() {
  std::vector<void*> all; for (auto& e : g_remoteWs) all.push_back(e.first);   // select can reach the symmetric slot and refresh the map
  for (void* ws : all) {
    if (!Alive(ws) || g_offWsSelIndex < 0) continue;
    int sel = *(int*)((char*)ws + g_offWsSelIndex);
    if (sel <= 0 || !EnsureRemoteGun(ws, sel)) continue;
    bool first = true, force = false; void* a[3] = { &sel, &first, &force };
    Invoke(m_wsSelectMI, ws, a);
  }
}
// Once a second (coop_torpedoes): each launcher of another player's boat gets their found torpedoes at the positions
// they have them, as TorpedoTiers built here (MakeTier with their definition). As for guns, a stock tier is never
// replaced and positions this machine lacks are padded with hidden placeholders. No select detour is needed: the game's
// WeaponsSync.UpdateMe keeps calling select while their index is beyond this list, so their pick takes on its next try;
// a position already selected is selected again so it fires and shows the new tier.
static void EnsureRemoteTorps() {
  if (!g_tp.ok) return;
  std::vector<void*> all; for (auto& e : g_remoteLaunchers) all.push_back(e.first);   // select's slot-conflict code can reach a
  for (void* l : all) {                                                // WeaponSystem.select, whose hook refreshes the maps
    auto rl = g_remoteLaunchers.find(l);
    if (rl == g_remoteLaunchers.end() || !Alive(l)) continue;
    int owner = rl->second.owner, index = rl->second.index;            // copied before any managed call
    auto rbi = g_remote.find(owner);
    if (rbi == g_remote.end()) continue;
    auto sl = rbi->second.launchers.find(index);
    if (sl == rbi->second.launchers.end()) continue;                   // no found torpedoes in it on their boat
    RemoteSlot slot = sl->second;
    std::vector<Item> defs; std::map<std::string, unsigned long long> stamps;
    for (const auto& ps : slot.pos)
      for (const Item& it : rbi->second.items) if (it.name == ps.second) { defs.push_back(it); stamps[it.name] = rbi->second.stamps[it.name]; break; }
    void* order = *(void**)((char*)l + g_offOrder);
    void* list = *(void**)((char*)l + g_tp.lTorpedos);
    if (!order || ListCount(list) < 2) continue;
    std::string family = FamilyOf(order);
    if (family != slot.family) {
      static std::set<std::string> told;
      if (told.insert(std::to_string(owner) + "|" + std::to_string(index)).second)
        Log("co-op torpedoes: player %d's launcher %d is '%s' here but '%s' for them; left alone", owner, index, family.c_str(), slot.family.c_str());
      continue;
    }
    std::unordered_map<std::string, void*> byBase;                     // as InjectTorpedoes: the stock tiers by their base= name
    void* firstReal = nullptr;
    for (int i = 0; i < ListCount(list); i++) {
      void* t = ListAt(list, i); if (!t) continue;
      std::string b = TierBase(t);
      if (!b.empty()) { byBase.emplace(b, t); if (!firstReal && i > 0 && TierType(t)) firstReal = t; }
    }
    if (!firstReal) continue;
    for (const auto& ps : slot.pos) {
      int j = ps.first; const std::string& name = ps.second;
      const Item* def = nullptr;
      for (const Item& it : defs) if (it.name == name) { def = &it; break; }
      unsigned long long stamp = def ? stamps[name] : 0;
      int cnt = ListCount(list);
      void* cur = j < cnt ? ListAt(list, j) : nullptr;
      std::string was = cur ? TierName(cur) : std::string("(nothing)");
      auto rt = cur ? g_remoteTiers.find(cur) : g_remoteTiers.end();
      if (rt != g_remoteTiers.end() && rt->second.owner == owner && rt->second.name == name && rt->second.stamp == stamp) continue;   // already theirs
      bool ours = rt != g_remoteTiers.end();                            // looked up again below: inserts may rehash the map
      if (cur && !TierBase(cur).empty()) {                             // a stock torpedo at a position they use: never replaced
        static std::set<std::string> told;
        if (told.insert(std::to_string(owner) + "|" + std::to_string(index) + "|" + std::to_string(j)).second)
          Log("co-op torpedoes: position %d of player %d's launcher %d holds the stock '%s' here; left alone", j, owner, index, was.c_str());
        continue;
      }
      std::string fkey = std::to_string(owner) + "|" + std::to_string(index) + "|" + std::to_string(j) + "|";
      if (g_remoteTorpFailed.count(fkey + "pad")) break;               // padding failed before: nothing past it either
      fkey += name + "|" + std::to_string(stamp);
      if (g_remoteTorpFailed.count(fkey)) continue;
      bool padFailed = false;
      while (ListCount(list) < j) {                                    // they have more torpedoes in this launcher than this machine
        Item gap; gap.name = "co-op placeholder"; gap.retired = true;
        void* t = MakeTier(firstReal, gap, owner, 0);
        if (!t || !ListAdd(list, t)) { padFailed = true; break; }
        g_remoteTiers[t] = { owner, gap.name, 0, true, Pin(t) };
      }
      if (padFailed) {
        g_remoteTorpFailed.insert(std::to_string(owner) + "|" + std::to_string(index) + "|" + std::to_string(j) + "|pad");
        Log("co-op torpedoes: could not pad player %d's launcher %d ('%s') to position %d; left as it is this battle", owner, index, family.c_str(), j);
        break;
      }
      bool stockThere = name == "-" || name.compare(0, 2, "-|") == 0;   // "-" or "-|<base>": stock on their machine
      Item hidden; hidden.name = stockThere ? "co-op placeholder" : name; hidden.retired = true;
      const Item* use = def;
      auto bb = def ? byBase.find(def->base) : byBase.end();
      void* src = bb != byBase.end() ? bb->second : nullptr;
      // a built-in is built from this machine's own copy for this family (every player runs the same version). Two reasons:
      // their text describes one copy per name, whose base may be another family's (SplitFamilies), and CoopGunsIn's bounds
      // check drops the set. edits a torpedo side-grade is made of. retired is cleared so it shows even if side_grades=0 hid it.
      Item own;
      if (def && def->builtIn) {
        const Item* loc = FindItemIn(name, family);
        if (loc && loc->builtIn && (bb = byBase.find(loc->base)) != byBase.end()) { src = bb->second; own = *loc; own.retired = false; use = &own; }
      }
      if (!src) {
        static std::set<std::string> told;
        if (def && told.insert(std::to_string(owner) + "|" + name + "|" + family).second)
          Log("co-op torpedoes: player %d's '%s' is based on '%s', which launcher '%s' does not have here; a hidden placeholder keeps the position", owner, name.c_str(), def->base.c_str(), family.c_str());
        use = &hidden; src = firstReal;
        if (name.size() > 2 && name.compare(0, 2, "-|") == 0) {        // the stock they fire there, when this launcher has it
          auto pb = byBase.find(name.substr(2));
          if (pb != byBase.end() && TierType(pb->second)) src = pb->second;
        }
      }
      void* t2 = MakeTier(src, *use, owner, use == &hidden ? 0 : stamp);
      bool put = t2 && (j < ListCount(list) ? ListSetAt(list, j, t2) : ListAdd(list, t2));
      if (!put) {
        g_remoteTorpFailed.insert(fkey);
        Log("co-op torpedoes: could not %s '%s' for position %d of player %d's launcher %d ('%s'); left as it is this battle",
            t2 ? "place" : "build", use->name.c_str(), j, owner, index, family.c_str());
        continue;
      }
      if (ours) { auto e = g_remoteTiers.find(cur); if (e != g_remoteTiers.end()) { Unpin(e->second.h); g_remoteTiers.erase(e); } }
      g_remoteTiers[t2] = { owner, name, stamp, use == &hidden, Pin(t2) };
      g_cc.torpTiersBuilt++;
      if (++g_coopGunLogs <= 40) Log("co-op torpedoes: player %d's launcher %d ('%s') position %d is now '%s'%s (was '%s')", owner, index, family.c_str(), j, use->name.c_str(), use == &hidden ? " (hidden)" : "", was.c_str());
      if (*(int*)((char*)l + g_tp.lSelIndex) == j) {                    // selected already: again, so it fires and shows the new tier
        int idx = j; void* a[1] = { &idx };
        Invoke(g_tp.selectMI, l, a);                                   // selectIndex = select(idx, true, false); it sets the twin too
        SafeRemoteTubeSync(l);
        if (g_coopTorpFaults < 0) return;                              // it faulted: SafeEnsureRemoteTorps clears the maps
        Log("co-op torpedoes: player %d's launcher %d had position %d selected; selected it again so it fires and shows '%s'", owner, index, j, use->name.c_str());
      }
    }
  }
}
static void SafeEnsureRemoteTorps() {                                 // a fault here turns off only the co-op torpedoes
  if (g_coopTorpFaults < 0) return;
  __try { EnsureRemoteTorps(); if (g_coopTorpFaults < 0) ClearRemoteTorps(); }
  __except (FaultFilter("co-op torpedoes", GetExceptionInformation())) { g_coopTorpFaults = -1; ClearRemoteTorps(); }
}

// ---- Rare and Elite enemies (coop_elites) ----
struct EliteOut { void* ce; void* handle; std::string text, name; DWORD at; };
static std::vector<EliteOut> g_eliteOut;                             // host: rolled, waiting for its network object
struct EliteIn { int id; EliteRoll roll; EliteInfo info; DWORD at; };
static std::vector<EliteIn> g_eliteIn;                               // client: sent by the host, waiting for the ship here
static void CoopEliteMade(void* ce, const EliteRoll& r, const EliteInfo& info, const std::string& shown) {
  if (!g_coopElites) return;
  char b[256];
  snprintf(b, sizeof b, "%d %.4f %.4f %.4f %.4f %.4f %d %.1f %.1f\n", info.tier, r.hp, info.damage, r.sx, r.sy, r.sz, info.rp, info.blastDamage, info.blastRadius);
  g_eliteOut.push_back({ ce, Pin(ce), std::string(b) + info.name + "\n" + info.ship + "\n" + shown + "\n", info.name, GetTickCount() });
}
static void CoopEliteIn(const std::string& body) {
  std::vector<std::string> l; for (size_t at = 0; at < body.size(); ) { size_t nl = body.find('\n', at); if (nl == std::string::npos) nl = body.size(); l.push_back(body.substr(at, nl - at)); at = nl + 1; }
  if (l.size() < 5) return;
  EliteIn e = {}; e.id = atoi(l[0].c_str()); e.at = GetTickCount();
  double hp = 1, dmg = 1, sx = 1, sy = 1, sz = 1, bd = 0, br = 0; int tier = 1, rp = 0;
  if (sscanf(l[1].c_str(), "%d %lf %lf %lf %lf %lf %d %lf %lf", &tier, &hp, &dmg, &sx, &sy, &sz, &rp, &bd, &br) != 9) return;
  auto clampd = [](double v, double lo, double hi) { return !std::isfinite(v) ? 1.0 : v < lo ? lo : (v > hi ? hi : v); };   // %lf reads nan too
  e.roll = { clampd(hp, 0.1, 20), 1, clampd(sx, 0.2, 5), clampd(sy, 0.2, 5), clampd(sz, 0.2, 5) };
  e.info.tier = tier == 2 ? 2 : 1; e.info.damage = (float)clampd(dmg, 0.1, 20);   // each machine pays its own player (KILL messages share the kills)
  e.info.rp = rp < 0 ? 0 : (rp > 5000 ? 5000 : rp);                    // another machine's numbers are bounded before they pay RP
  e.info.blastDamage = (float)(std::isfinite(bd) ? clampd(bd, 0, 5000) : 0); e.info.blastRadius = (float)(std::isfinite(br) ? clampd(br, 0, 200) : 0);
  e.info.name = l[2]; e.info.ship = l[3]; e.info.traits = l[4];
  g_eliteIn.push_back(e);
  DLog("co-op elites: the host sent %s '%s' (object %d)", e.info.tier == 2 ? "Elite" : "Rare", e.info.name.c_str(), e.id);
}

// ---- helpers (coop_helpers) ----
struct LookOut { void* ent; void* handle; V3 scale; DWORD at; int owner; int kind; };
static std::vector<LookOut> g_lookOut;                               // host: a helper's size, waiting for its network object
struct LookIn { int id; V3 scale; DWORD at; int kind; };          // kind: 0 fighter, 1 PT boat, 2 gull (-1 from an older build)
static std::vector<LookIn> g_lookIn;                                 // client: a helper's size from the host
struct LookRestore { void* ent; void* handle; V3 scale; };
static std::vector<LookRestore> g_lookRestore;                       // client: pooled entities the mod resized
static bool MountPlace(void* mount, int* sys, int* pos) {             // where a gun sits on this machine's own boat
  void* wm = nullptr;
  for (const NetBoat& b : NetBoats()) if (g_myId >= 0 && b.owner == g_myId && b.wm) wm = b.wm;
  if (!wm) { void* craft = PlayerCraft(); wm = craft && g_offCraftWM >= 0 ? *(void**)((char*)craft + g_offCraftWM) : nullptr; }
  void* arr = wm && g_offWMSystems >= 0 ? *(void**)((char*)wm + g_offWMSystems) : nullptr;
  for (int i = 0; arr && i < ArrLen(arr) && i < 32; i++) {
    void* ws = ArrAt(arr, i); void* mounts = ws ? *(void**)((char*)ws + g_offMounts) : nullptr;
    for (int j = 0; j < ListCount(mounts); j++) if (ListAt(mounts, j) == mount) { *sys = i; *pos = j; return true; }
  }
  return false;
}
static bool CoopAskHelper(const SpawnReq& r) {
  int sys = -1, pos = -1;
  if (g_myId < 0 || g_peers.empty() || !g_peers[0].heard || g_peers[0].mismatch) { DLog("co-op helpers: the host has not answered yet; no %s", kMinionWord[r.kind]); return false; }
  if (!MountPlace(r.mount, &sys, &pos)) { Log("co-op helpers: the gun is not on your boat; no %s", kMinionWord[r.kind]); return false; }
  V3 boat = PosOf(PlayerCraft());
  char b[256];
  snprintf(b, sizeof b, "%d %d %d %d %.2f %.2f %.2f %.4f %.4f %.4f %.2f %.2f %.2f\n", r.kind, sys, pos, r.shot, r.pos.x, r.pos.y, r.pos.z, r.dir.x, r.dir.y, r.dir.z, boat.x, boat.y, boat.z);
  if (!CoopSend(g_peers[0].steam, CoopHeader("SPAWN", g_myId) + b)) return false;
  g_cc.helpersAsked++;
  Log("co-op helpers: asked the host for a %s (shot %d of slot %d position %d)", kMinionWord[r.kind], r.shot, sys, pos);
  return true;
}
static void CoopSpawnIn(int from, const std::string& body) {
  int kind = -1, sys = -1, pos = -1, shot = 0; V3 p, d, boat;
  if (sscanf(body.c_str(), "%d %d %d %d %f %f %f %f %f %f %f %f %f", &kind, &sys, &pos, &shot, &p.x, &p.y, &p.z, &d.x, &d.y, &d.z, &boat.x, &boat.y, &boat.z) != 13 || kind < 0 || kind > 3) return;
  for (float v : { p.x, p.y, p.z, d.x, d.y, d.z, boat.x, boat.y, boat.z }) if (!std::isfinite(v) || fabsf(v) > 1e6f) return;
  if (!EngagementInstance()) { DLog("co-op helpers: player %d asked for a %s outside a battle; ignored", from, kMinionWord[kind]); return; }
  void* mount = nullptr;
  for (const NetBoat& b : NetBoats()) {
    if (b.owner != from || !b.wm) continue;
    void* arr = *(void**)((char*)b.wm + g_offWMSystems);
    void* ws = arr && sys >= 0 && sys < ArrLen(arr) ? ArrAt(arr, sys) : nullptr;
    void* mounts = ws ? *(void**)((char*)ws + g_offMounts) : nullptr;
    if (mounts && pos >= 0 && pos < ListCount(mounts)) mount = ListAt(mounts, pos);
  }
  if (!mount) { Log("co-op helpers: player %d asked for a %s from slot %d position %d, which is not on their boat here; ignored", from, kMinionWord[kind], sys, pos); return; }
  SpawnReq q{ kind, mount, p, d, shot }; q.owner = from; q.boat = boat;
  g_spawnQueue.push_back(q);
}
static void CoopHelperMade(void* ent, int owner, int kind) {
  if (!g_coopHelpers) return;
  void* tr = Invoke(m_getTransform, ent); void* b = tr ? Invoke(m_getLocalScale, tr) : nullptr;
  if (!b) return;
  g_lookOut.push_back({ ent, Pin(ent), *(V3*)((char*)b + 0x10), GetTickCount(), owner, kind });
  if (owner >= 0) { g_cc.helpersMade++; Log("co-op helpers: spawned a helper for player %d", owner); }
}
static void CoopLookIn(const std::string& body) {
  LookIn l = {}; l.at = GetTickCount(); l.kind = -1;
  if (sscanf(body.c_str(), "%d %f %f %f %d", &l.id, &l.scale.x, &l.scale.y, &l.scale.z, &l.kind) < 4) return;
  if (!(l.scale.x > 0.01f && l.scale.x < 50 && l.scale.y > 0.01f && l.scale.y < 50 && l.scale.z > 0.01f && l.scale.z < 50)) return;
  g_lookIn.push_back(l);
}
static void RestoreLooks() {
  for (LookRestore& r : g_lookRestore) { void* tr = Invoke(m_getTransform, r.ent); if (tr) { void* a[1] = { &r.scale }; Invoke(m_setLocalScale, tr, a); } Unpin(r.handle); }
  g_lookRestore.clear();
}

// Host: rolled Elites and new helpers go out once the game has given them a network object. Client: what the host sent
// is applied once the ship or helper exists here (an Elite only after EliteScan has seen this life of the ship).
static void CoopPending() {
  DWORD now = GetTickCount();
  for (size_t i = 0; i < g_eliteOut.size(); ) {
    EliteOut& e = g_eliteOut[i];
    int id = ObjectIdOf(e.ce);
    if (id >= 0) {
      int n = CoopSendAll(CoopHeader("ELITE", g_myId) + std::to_string(id) + "\n" + e.text); g_cc.elitesSent++; Log("co-op elites: sent '%s' to %d player(s) (object %d)", e.name.c_str(), n, id);
      auto ge = g_elites.find(e.ce); if (ge != g_elites.end()) ge->second.netId = id;   // its KILL message names this object
    }
    else if (now - e.at < 15000) { i++; continue; }
    else Log("co-op elites: '%s' got no network object within 15 s; the other players see a normal ship", e.name.c_str());
    Unpin(e.handle); g_eliteOut.erase(g_eliteOut.begin() + i);
  }
  for (size_t i = 0; i < g_lookOut.size(); ) {
    LookOut& l = g_lookOut[i];
    int id = ObjectIdOf(l.ent);
    if (id >= 0) { char b[96]; snprintf(b, sizeof b, "%d %.4f %.4f %.4f %d\n", id, l.scale.x, l.scale.y, l.scale.z, l.kind); CoopSendAll(CoopHeader("LOOK", g_myId) + b); g_cc.looksSent++; }
    else if (now - l.at < 10000) { i++; continue; }
    else DLog("co-op helpers: a helper got no network object within 10 s; the others see it full size");
    Unpin(l.handle); g_lookOut.erase(g_lookOut.begin() + i);
  }
  if (g_eliteIn.empty() && g_lookIn.empty()) return;
  std::unordered_map<int, void*> ents; ObjectIdMap(ents);
  for (size_t i = 0; i < g_eliteIn.size(); ) {
    EliteIn& e = g_eliteIn[i];
    auto f = ents.find(e.id); void* ce = f != ents.end() ? f->second : nullptr;
    bool ready = ce && EliteLookups() && *(bool*)((char*)ce + g_offLeAlive) && g_seenAlive.count(ce);
    if (ready && !g_elites.count(ce)) { e.info.netId = e.id; ApplyElite(ce, e.roll, e.info, true, e.info.traits); g_cc.elitesApplied++; }
    else if (!ready && now - e.at < 20000) { i++; continue; }
    else if (!ready) Log("co-op elites: '%s' (object %d) never showed up here; it looks normal on this screen", e.info.name.c_str(), e.id);
    g_eliteIn.erase(g_eliteIn.begin() + i);
  }
  for (size_t i = 0; i < g_lookIn.size(); ) {
    LookIn& l = g_lookIn[i];
    auto f = ents.find(l.id); void* ent = f != ents.end() ? f->second : nullptr;
    if (ent && (!EliteLookups() || !*(bool*)((char*)ent + g_offLeAlive))) ent = nullptr;   // not spawned here yet: CoopLooksBack would undo it
    void* tr = ent ? Invoke(m_getTransform, ent) : nullptr; void* b = tr ? Invoke(m_getLocalScale, tr) : nullptr;
    if (b) {
      bool listed = false; for (auto& r : g_lookRestore) if (r.ent == ent) listed = true;   // keep the first, real size: a second
      if (!listed) g_lookRestore.push_back({ ent, Pin(ent), *(V3*)((char*)b + 0x10) });       // LOOK would record the resized one
      void* a[1] = { &l.scale }; Invoke(m_setLocalScale, tr, a); g_cc.looksApplied++;
      if (l.kind == 2) AddHelperLook(ent, V3{ 0, 0, 0 }, false, true);   // a gull: its look here too (the size is CoopLooksBack's)
    } else if (now - l.at < 10000) { i++; continue; }
    g_lookIn.erase(g_lookIn.begin() + i);
  }
}
static void CoopLooksBack() {                                         // a resized helper that went back to its pool gets its size back
  for (size_t i = 0; i < g_lookRestore.size(); ) {
    LookRestore& r = g_lookRestore[i];
    if (g_offLeAlive >= 0 && *(bool*)((char*)r.ent + g_offLeAlive)) { i++; continue; }
    void* tr = Invoke(m_getTransform, r.ent); if (tr) { void* a[1] = { &r.scale }; Invoke(m_setLocalScale, tr, a); }
    Unpin(r.handle); g_lookRestore.erase(g_lookRestore.begin() + i);
  }
}

// A Rare/Elite kill credited on this machine (to anyone: this player, another player's boat or helper, an ally) pays this
// player in EliteKilled and is announced here, so every other player gets their own reward too. The host passes a
// client's KILL on to the other clients. A ship sunk with no kill credit on any machine pays nobody, as in single player.
static void CoopKillOut(const EliteInfo& info) {
  if (!g_coopRole || !g_coopElites || info.netId < 0) return;
  int n = CoopSendAll(CoopHeader("KILL", g_myId) + std::to_string(info.netId) + "\n");
  g_cc.killsSent++;
  Log("co-op elites: told %d player(s) that '%s' sank (object %d), so they get their own reward", n, info.name.c_str(), info.netId);
}
static void CoopKillIn(int from, const std::string& body) {
  char* end = nullptr; long lid = strtol(body.c_str(), &end, 10);
  if (end == body.c_str() || (*end && *end != '\n' && *end != '\r') || lid < 0 || lid > INT_MAX) return;   // not a plain object id (-1 would match unsent Elites)
  int id = (int)lid;
  void* ce = nullptr; bool paid = false;
  for (auto& e : g_elites) if (e.second.netId == id) { ce = e.first; paid = e.second.rewarded; }
  for (auto& d : g_eliteDead) if (!ce && d.second.first.netId == id) { ce = d.first; paid = d.second.first.rewarded; }
  if (!ce || paid) { DLog("co-op elites: player %d says object %d sank; %s", from, id, ce ? "already paid here" : "no Rare/Elite here has that object"); return; }
  g_cc.killsIn++;
  EliteKilled(ce, true);
}
static bool CoopHostElites() {                                         // client: the host answered and said it sends its rolls
  return g_coopRole == 2 && g_coopElites && !g_peers.empty() && g_peers[0].heard && !g_peers[0].mismatch && g_peers[0].elites;
}

// ---- the session ----
static CoopPeer* PeerBySteam(unsigned long long s) { for (CoopPeer& p : g_peers) if (p.steam == s) return &p; return nullptr; }
static void CoopEnd(const char* why) {
  Log("co-op: %s; co-op sync stops", why);
  g_peers.clear(); g_remote.clear(); ClearRemoteWs(); ClearRemoteMounts(); ClearRemoteTorps(); g_myGunsHash = 0;
  for (auto& e : g_eliteOut) Unpin(e.handle);
  for (auto& l : g_lookOut) Unpin(l.handle);
  g_eliteOut.clear(); g_eliteIn.clear(); g_lookOut.clear(); g_lookIn.clear();
  RestoreLooks();
}
// Who this machine talks to: as host every Steam connection the transport has, as a client the host.
static void CoopSession() {
  void* fs = Fishy();
  // This game's FishNet has LocalConnectionState as an int flags enum: Stopped 1, Stopping 2, Starting 4, Started 8.
  // Started is 8 here, not 2 as in a byte enum.
  auto state = [&](bool serverSide) {
    void* a[1] = { &serverSide }; void* r = fs ? Invoke(m_fsLocalState, fs, a) : nullptr;
    return r ? *(int*)((char*)r + 0x10) : 0;
  };
  int ss = state(true), cs = state(false);
  static int lastSs = -1, lastCs = -1;
  if (ss != lastSs || cs != lastCs) {                                  // changes only on hosting, joining and leaving
    lastSs = ss; lastCs = cs;
    Log("co-op: Steam transport %s; server state %d, client state %d (8 = started)", fs ? "found" : "not found", ss, cs);
  }
  bool server = ss == 8, clientOnly = !server && cs == 8;
  std::vector<std::pair<int, unsigned long long>> now;
  if (fs && server) {
    void* srv = *(void**)((char*)fs + g_offFsServer);
    static int offNext = -2;
    if (offNext == -2 && srv) { offNext = FieldOffset(il2cpp_object_get_class(srv), "_nextConnectionId"); }
    int next = srv && offNext >= 0 ? *(int*)((char*)srv + offNext) : 0;
    for (int id = 0; id < next && id < 64; id++) {
      void* a[1] = { &id }; void* st = Invoke(m_fsState, fs, a);
      if (!st || *(unsigned char*)((char*)st + 0x10) != 2) continue;      // RemoteConnectionState.Started
      void* addr = Invoke(m_fsAddress, fs, a);
      unsigned long long steam = addr ? _strtoui64(Str(addr), nullptr, 10) : 0;
      if (!steam) { static std::set<int> told; if (told.insert(id).second) Log("co-op: player %d is not connected through Steam ('%s'); nothing is synced with them", id, addr ? Str(addr) : "?"); continue; }
      now.push_back({ id, steam });
    }
  } else if (fs && clientOnly) {
    void* cl = *(void**)((char*)fs + g_offFsClient);
    static int offHost = -2;
    if (offHost == -2 && cl) { offHost = FieldOffset(il2cpp_object_get_class(cl), "_hostSteamID"); }
    unsigned long long steam = cl && offHost >= 0 ? *(unsigned long long*)((char*)cl + offHost) : 0;
    if (steam) now.push_back({ -1, steam });                           // the host's ClientId comes with its messages
  }
  int role = now.empty() ? 0 : (server ? 1 : 2);
  if (role != g_coopRole) {
    if (g_coopRole) CoopEnd(role ? "the co-op game changed" : "the co-op game ended");
    g_coopRole = role;
    if (role) {
      g_myId = MyClientId();
      Log("co-op: %s (you are player %d); syncing guns %d, Rare/Elite enemies %d, effects on their shots %d, called-in helpers %d, found torpedoes %d", role == 1 ? "hosting a game with other players" : "joined a game",
          g_myId, (int)g_coopGuns, (int)g_coopElites, (int)g_coopVisuals, (int)g_coopHelpers, (int)g_coopTorps);
      if (g_coopGuns) InstallCoopSelect();
    }
  }
  if (!role) return;
  if (g_myId < 0) g_myId = MyClientId();
  for (auto& n : now) {
    CoopPeer* p = PeerBySteam(n.second);
    if (!p) {
      g_peers.push_back({ n.first, n.second, false, false, 0, 0, 0 });
      if (role == 1) Log("co-op: Steam user %llu joined as player %d", n.second, n.first); else Log("co-op: the host is Steam user %llu", n.second);
    }                                                                  // an id the player sent in HELLO is kept
  }
  for (size_t i = 0; i < g_peers.size(); ) {
    bool still = false; for (auto& n : now) if (n.second == g_peers[i].steam) still = true;
    if (still) { i++; continue; }
    Log("co-op: player %d (Steam user %llu) left", g_peers[i].id, g_peers[i].steam);
    g_remote.erase(g_peers[i].id);
    g_peers.erase(g_peers.begin() + i);
  }
  void* snm = s_snmGet();
  DWORD t = GetTickCount();
  for (CoopPeer& p : g_peers) {                                       // sending implicitly accepts their session too
    SteamIdentity id = SteamUser(p.steam);
    if (snm) s_snmAccept(snm, &id);
    if (!p.helloAt || t - p.helloAt > (p.heard ? 20000u : 2000u)) { p.helloAt = t; CoopSend(p.steam, CoopHeader("HELLO", g_myId) + (g_coopElites ? "elites\n" : "")); }
  }
}
static void CoopHandle(unsigned long long from, const std::string& text) {
  size_t nl = text.find('\n');
  std::string head = text.substr(0, nl == std::string::npos ? text.size() : nl), body = nl == std::string::npos ? std::string() : text.substr(nl + 1);
  std::vector<std::string> f; for (size_t at = 0; at <= head.size(); ) { size_t b = head.find('|', at); if (b == std::string::npos) b = head.size(); f.push_back(head.substr(at, b - at)); at = b + 1; }
  if (f.size() < 5 || f[0] != "BCWM") return;
  CoopPeer* p = PeerBySteam(from);
  if (!p) { static int told = 0; if (++told <= 3) Log("co-op: a message from Steam user %llu, who is not in this game; ignored", from); return; }
  if (f[1] != kModVersion) {
    if (!p->mismatch) { p->mismatch = true; Log("co-op: player %d runs mod %s and you run %s; nothing is synced with them (you both need the same version)", p->id, f[1].c_str(), kModVersion);
                        ShowMessage("Co-op: your friend runs weapon mod " + f[1] + ", you run " + kModVersion + ". Nothing is synced."); }
    return;
  }
  // Same version, another build (a -dev version changes between builds): synced anyway, with a warning.
  if (f[4] != kCoopBuild && !p->buildWarned) {
    p->buildWarned = true;
    Log("co-op: player %d runs build '%s' of mod %s and you run build '%s'; syncing anyway, but the two may behave differently", p->id, f[4].c_str(), kModVersion, kCoopBuild);
    ShowMessage("Co-op: your friend's weapon mod build differs from yours (" + f[4] + " vs " + kCoopBuild + "). Use the same zip.");
  }
  int origin = atoi(f[3].c_str());
  if (f[2] == "HELLO" && origin >= 0 && origin != p->id) {             // its own FishNet ClientId: the owner id of its boat
    if (p->id >= 0) DLog("co-op: player %d calls itself player %d; using that", p->id, origin);
    p->id = origin;
  }
  if (g_coopRole == 1) origin = p->id;                                 // a client only ever speaks for itself
  if (!p->heard) {
    p->heard = true; p->gunsHash = 0;
    Log("co-op: player %d answered (mod %s%s); syncing with them", p->id, f[1].c_str(), f[4] == kCoopBuild ? "" : (", a different build: " + f[4] + ", yours " + kCoopBuild).c_str());
    if (g_coopRole == 1) for (auto& r : g_remote) if (r.first != p->id && !r.second.raw.empty()) CoopSend(p->steam, r.second.raw);   // the guns of everyone already here
  }
  const std::string& type = f[2];
  if (type == "HELLO") p->elites = body.compare(0, 6, "elites") == 0;
  if (type == "GUNS") {
    if (!g_coopTorps && (body.compare(0, 2, "L ") == 0 || body.find("\nL ") != std::string::npos)) {
      static std::set<int> told;                                         // their torpedoes stay this machine's (InjectTorpedoes)
      if (told.insert(origin).second) Log("co-op torpedoes: player %d sent found torpedoes, but coop_torpedoes=0 here; their torpedoes show as this machine's", origin);
    }
    if (!g_coopGuns && !g_coopTorps) return;
    std::string raw = g_coopRole == 1 ? CoopHeader("GUNS", origin) + body : text;
    CoopGunsIn(origin, body, raw);
    if (g_coopRole == 1) CoopSendAll(raw, from);                       // pass it on to the other clients
  }
  else if (type == "ELITE" && g_coopRole == 2 && g_coopElites) CoopEliteIn(body);
  else if (type == "KILL" && g_coopElites) {
    CoopKillIn(origin, body);
    if (g_coopRole == 1) CoopSendAll(CoopHeader("KILL", origin) + body, from);   // the host passes a client's kill on to the others
  }
  else if (type == "LOOK" && g_coopRole == 2 && g_coopHelpers) CoopLookIn(body);
  else if (type == "SPAWN" && g_coopRole == 1 && g_coopHelpers) CoopSpawnIn(origin, body);
  else if (type == "HEAL" && g_coopVisuals) {                          // an older build ignores the type
    CoopHealIn(origin, body);
    if (g_coopRole == 1) CoopSendAll(CoopHeader("HEAL", origin) + body, from);   // the host passes it on to the other clients
  }
}
static void CoopReceive() {
  void* snm = s_snmGet();
  if (!snm) return;
  void* msgs[32];
  for (int round = 0; round < 8; round++) {
    int n = s_snmRecv(snm, kCoopChannel, msgs, 32);
    for (int i = 0; i < n; i++) {
      char* m = (char*)msgs[i];
      const char* data = *(const char**)m; int size = *(int*)(m + 8);
      unsigned long long from = *(int*)(m + 16) == 16 ? *(unsigned long long*)(m + 24) : 0;   // m_identityPeer: a SteamID
      std::string text = data && size > 0 && size < (4 << 20) ? std::string(data, (size_t)size) : std::string();
      s_snmRelease(m);
      g_cc.msgsIn++; g_cc.bytesIn += size;
      if (from && !text.empty()) CoopHandle(from, text);
    }
    if (n < 32) break;
  }
}
static void CoopBattleWatch() {
  void* eng = EngagementInstance();
  if (eng == g_coopEng) return;
  if (g_coopEng && (g_cc.msgsOut || g_cc.msgsIn || g_copyShots))
    Log("co-op summary (battle over): %d messages sent (%d KB), %d received (%d KB); guns: sent %d times, %d boats received, %d guns built on other players' boats; "
        "Rare/Elite: %d sent, %d applied, %d kills announced, %d rewards from other players' kills; other players' shots with mod effects: %d, their chain leaps shown: %d; "
        "helpers: %d asked for, %d made for others, %d sizes sent, %d applied; "
        "found torpedoes: %d boats' lists received, %d tiers built on other players' launchers, %d of their found torpedoes seen in the water; "
        "heal streaks: %d sent, %d received",
        g_cc.msgsOut, g_cc.bytesOut / 1024, g_cc.msgsIn, g_cc.bytesIn / 1024, g_cc.gunsSent, g_cc.gunsIn, g_cc.gunsBuilt, g_cc.elitesSent, g_cc.elitesApplied,
        g_cc.killsSent, g_cc.killsIn, g_copyShots, g_copyChains, g_cc.helpersAsked, g_cc.helpersMade, g_cc.looksSent, g_cc.looksApplied,
        g_cc.torpsIn, g_cc.torpTiersBuilt, g_cc.torpCopies, g_cc.healsSent, g_cc.healsIn);
  g_cc = {}; g_copyShots = g_copyChains = 0;
  if (g_coopEng) { RestoreLooks(); ClearRemoteWs(); ClearRemoteMounts(); ClearRemoteTorps(); }   // a battle ended: its boats go away
  g_coopEng = eng;
}
static void CoopTick() {
  DWORD now = GetTickCount();
  if (now - g_coopTickAt < 50) return;
  g_coopTickAt = now;
  if (!g_coopGuns && !g_coopElites && !g_coopVisuals && !g_coopHelpers && !g_coopTorps) return;
  if (!SteamReady() || !CoopLookups()) return;
  if (now - g_coopSessionAt >= 2000) { g_coopSessionAt = now; CoopSession(); }
  CoopBattleWatch();
  if (!g_coopRole) return;
  CoopReceive();
  if ((g_coopGuns || g_coopTorps) && now - g_coopGunsAt >= 2000) { g_coopGunsAt = now; CoopSendGuns(); }
  if ((g_coopGuns || g_coopTorps) && now - g_coopFixAt >= 1000) {
    g_coopFixAt = now; RefreshRemoteWs();                              // also maps their launchers (coop_torpedoes)
    if (g_coopGuns) CoopFixSelected();
    if (g_coopTorps) SafeEnsureRemoteTorps();
  }
  if (now - g_coopPendingAt >= 500) { g_coopPendingAt = now; CoopPending(); CoopLooksBack(); }
}
static void SafeCoopTick() {
  if (g_coopFaults < 0) return;
  __try { CoopTick(); }
  __except (FaultFilter("co-op", GetExceptionInformation())) { g_coopFaults = -1; g_coopRole = 0; ClearRemoteWs(); ClearRemoteMounts(); ClearRemoteTorps(); }
}

