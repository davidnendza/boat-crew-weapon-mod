// Mod Options, What's New, the after-action report window, MODDED on the title screen (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- mod options, the version and What's New ----
// Opened from buttons under the MODDED banner on the title screen, and from MOD OPTIONS next to INVENTORY on the loadout
// screen. Drawn in the inventory's 1920x1080 virtual space. Each change is written straight into drops.ini /
// enemies.ini (only that key's line; comments stay) and, where the mod reads the value as it goes, used at once; the
// rest say so. The game build has no scroll view and cannot measure text: long text is wrapped by character count and
// the changelog scrolled with the mouse wheel inside a clipped group.
// The Field Manual look: a manila sheet with tabs, inked switches and a note card that explains the row under the
// mouse. Options that need a restart say 'Game needs a restart to take effect', so it is clear the GAME must restart.
// The paper, shadow, stamp, card, X, arrows and dot are PNGs in models\ (tools\make_ui_assets.py); each has a flat
// fallback, so a missing file never breaks a window.
enum { kTabLoot, kTabArsenal, kTabVisuals, kTabEnemies, kTabCrew, kTabCoop, kOptTabs };
static const char* kOptTabName[kOptTabs] = { "LOOT", "ARSENAL", "VISUALS", "ENEMIES", "CREW", "CO-OP" };
static const char* kOptTabWord[kOptTabs] = { "DROPS", "YOUR GUNS", "WHAT YOU SEE", "RARE AND ELITE ENEMIES", "CREW WEAPONS AND LOOKS", "CO-OP  (EVERYONE NEEDS THE SAME VERSION AND SETTINGS)" };
// when: when a change applies, nullptr = at once. kOptRestart marks the restart-only switches (read once at launch).
static const char *kOptRestart = "game needs a restart", *kOptNewGuns = "for new guns", *kOptLoadout = "from the next loadout";
static const char* kOptPortrait = "from the next crew portrait";
// tab -1: the footer's switch. desc: the row's second line; help: the note card's text; def: the default, as shown
struct ModOpt { int tab; const char* file; const char* key; const char* label; const char* desc; const char* help; const char* def; char kind; double lo, hi, step; char fmt; bool* b; double* d; int* i; const char* when; const bool* live; };   // live: the value in use, for restart-only switches
static bool s_optTurret, s_optSide, s_optTracer, s_optElites, s_optCoopG, s_optCoopE, s_optCoopV, s_optCoopH, s_optCoopT;   // restart-only: what the file says now
static float g_newsScroll = 0;
static void* m_evDelta = nullptr;
static int s_optAutoScrap = 0;                                         // auto_scrap as 0 none .. 3 Rare (g_autoScrap + 1)
static int g_optTab = kTabLoot, g_optHot = -1;                         // the open tab (kept for the session) and the row the note card explains
static std::vector<ModOpt>& ModOpts() {
  static std::vector<ModOpt> v;
  if (!v.empty()) return v;
  const char* R = kOptRestart;
  // fmt: p = percent, q = percent with a decimal, x = times, s = seconds, n = number, r = auto_scrap's rarity names,
  // c = a whole number that already is a percent (female_crew).
  // Texts must match kDefaultDrops / kDefaultEnemies and the code that reads each key.
  s_optAutoScrap = g_autoScrap + 1;
  v = {
    { kTabLoot, "drops.ini", "chance", "Drop chance per RP crate burst", "How often a burst of RP crates gives a drop",
      "When you pick up a burst of RP crates (crates picked up within a few seconds of each other count as one burst), this is the chance it also gives a drop: a found gun, a torpedo or an unopened crate.",
      "40%", 'd', 0, 1, 0.05, 'p', nullptr, &g_dropChance },
    { kTabLoot, "drops.ini", "torpedo_share", "Torpedo share of drops", "Only on boats with torpedo slots",
      "Share of found guns and torpedoes (not unopened crates or Uniques) that are a torpedo, when your boat has torpedo slots. 2.5% is about 1 in 40 of those, or about 1 drop in 55 overall.",
      "2.5%", 'd', 0, 0.2, 0.005, 'q', nullptr, &g_torpShare },
    { kTabLoot, "drops.ini", "unique_chance", "Unique chance per drop", "The gold, hand-made guns and torpedoes",
      "Chance that a drop (other than an unopened crate) is a Unique: a hand-made gun or torpedo with its own name and effects, for a slot your boat has. Crates have their own odds (unique_crate_chance in drops.ini).",
      "4%", 'd', 0, 0.3, 0.01, 'p', nullptr, &g_uniqueChance },
    { kTabLoot, "drops.ini", "cosmetic_chance", "Cosmetic extra chance", "Tracer color, long tracers or gun size",
      "Share of found guns that also get one cosmetic extra: a tracer color, long tracers or a different gun size. Looks only.",
      "30%", 'd', 0, 1, 0.05, 'p', nullptr, &g_cosmeticChance },
    { kTabLoot, "drops.ini", "paint_chance", "Paint scheme chance", "Haze Grey, Navy Blue, Olive Drab, ...",
      "Share of found guns that come in a paint scheme of the period (Haze Grey, Measure 21 Navy Blue, Olive Drab, Kure Grey, ...), named on their Look: line. Looks only, never on torpedoes; Uniques are always gold.",
      "35%", 'd', 0, 1, 0.05, 'p', nullptr, &g_paintChance, nullptr, kOptNewGuns },
    { kTabLoot, "drops.ini", "auto_scrap", "Auto-scrap drops up to", "Never Uniques or torpedoes",
      "Drops of this rarity or lower go straight to materials instead of the loot grid. Uniques, torpedoes and guns from crates you open are never scrapped.",
      "OFF", 'i', 0, 3, 1, 'r', nullptr, nullptr, &s_optAutoScrap },
    { kTabArsenal, "drops.ini", "turret_guns", "Extra turret guns", "",
      "The mod's extra turret guns in the shop and the loot: Twin Vickers K, Twin Vickers .50, Twin Type 93, 2 cm Flak 38, Type 96 25 mm, M4 Barge Buster, Breda 20/65, Madsen 20 mm, Polsten 20 mm, Twin MG 34 and Twin DShK.",
      "ON", 'b', 0, 1, 1, 'n', &s_optTurret, nullptr, nullptr, R, &g_turretGuns },
    { kTabArsenal, "drops.ini", "side_grades", "Side-grades for other slots", "",
      "The mod's side-grades for the Heavy, Heavy+, Medium, Utility, Rocket, crew weapon and torpedo slots, from the QF 2-pdr pom-pom to the Thompson and the Long Lance torpedo, in the shop and the loot.",
      "ON", 'b', 0, 1, 1, 'n', &s_optSide, nullptr, nullptr, R, &g_sideGrades },
    { kTabArsenal, "drops.ini", "uses", "Missions per found gun", "0 = no gun wears out (yours too, at once)",
      "Missions a found gun lasts, +1 per extra effect (Uniques have their own count). It only wears in battles where it fires, then retires for its rarity's material. A new number is for guns found from now on, but 0 stops all your guns wearing out at once.",
      "4", 'i', 0, 20, 1, 'n', nullptr, nullptr, &g_usesBase, kOptNewGuns },
    { kTabArsenal, "drops.ini", "price_scale", "RP price of found guns", "Times the price of its base gun",
      "Found guns cost the RP price of the gun they are based on, times this. 0 makes them free.",
      "x1.00", 'd', 0, 3, 0.25, 'x', nullptr, &g_priceScale, nullptr, kOptLoadout },
    { kTabVisuals, "drops.ini", "tracer_rarity", "Rarity tracer colors", "",
      "Found Uncommon guns fire blue tracers, Rare guns gold and Uniques orange. Off = the stock tracers.",
      "ON", 'b', 0, 1, 1, 'n', &s_optTracer, nullptr, nullptr, R, &g_tracerAtLaunch },
    { kTabVisuals, "drops.ini", "effect_streaks", "Heal and chain streaks", "A streak on each heal and chain leap",
      "A green streak flies from the wreck to your boat when a kill heals your hull (Vampiric, Lazarus, ...), and a bright one jumps along each chain leap. Your screen only; in co-op others see your heal streaks if they have this on and you have Effects on others' shots on.",
      "ON", 'b', 0, 1, 1, 'n', &g_streaksOn },
    { kTabVisuals, "drops.ini", "barrel_smoke", "Smoking barrels", "Muzzle smoke after a long burst",
      "A gun that fires steadily for a few seconds keeps smoking from the muzzle for a while after it stops. Looks only, on this machine only. barrel_smoke_after and barrel_smoke_seconds in drops.ini set when it starts and how long it lasts.",
      "ON", 'b', 0, 1, 1, 'n', &g_barrelSmoke },
    { kTabVisuals, "drops.ini", "killcam", "Torpedo kill cam", "Single player only",
      "When one of your torpedoes is about to hit a ship, the camera follows it in slow motion, then waits before the next one can start. Single player only.",
      "ON", 'b', 0, 1, 1, 'n', &g_kcOn },
    { kTabVisuals, "drops.ini", "killcam_cooldown", "Wait between kill cams", "Seconds before the next one",
      "Seconds after a kill cam before the next one can start.",
      "60 s", 'd', 5, 300, 5, 's', nullptr, &g_kcCooldown },
    { kTabVisuals, "drops.ini", "killcam_slow", "Game speed during a kill cam", "25% is a quarter speed",
      "How fast the game runs while the kill cam follows a torpedo: 25% is a quarter of normal speed.",
      "25%", 'd', 0.1, 1, 0.05, 'p', nullptr, &g_kcSlow },
    { kTabVisuals, "drops.ini", "newsreel", "Battle newsreel GIF", "Off if battles run slower",
      "A second camera films the best moments of each battle (planes going down, ships sinking, your crew at their guns) and writes a black-and-white GIF to BoatCrewMod\\newsreels when the battle ends. It renders an extra view several times a second, so switch it off if battles run slower with it on.",
      "ON", 'b', 0, 1, 1, 'n', &g_newsreelOn },
    { kTabVisuals, "drops.ini", "newsreel_keep", "Newsreels to keep", "Oldest deleted; 0 = keep all",
      "How many newsreel GIFs to keep in BoatCrewMod\\newsreels (each can be up to about 80 MB). After a new one is written, the oldest beyond this are deleted. Only files named exactly like the mod's own (newsreel_<date>_<time>.gif) are ever deleted: rename or move a GIF to keep it. 0 keeps them all.",
      "10", 'i', 0, 200, 1, 'n', nullptr, nullptr, &g_newsreelKeep },
    { kTabVisuals, "drops.ini", "modded_banner", "MODDED on the title screen", "The red tag under the logo",
      "Shows the red MODDED tag under the BOAT CREW logo on the title screen, with the mod's version next to it. Off, the version stays above the title screen's mod buttons.",
      "ON", 'b', 0, 1, 1, 'n', &g_bannerOn },
    { kTabVisuals, "drops.ini", "kill_marks", "Kill markings on the hull", "A silhouette per kill, gold per ten",
      "Rows of small silhouettes on your boat's hull: planes shot down, ships sunk and shore targets destroyed, a gold one for every ten. Off hides them; the kills are still counted. A change shows at your next kill or the next time the boat is loaded.",
      "ON", 'b', 0, 1, 1, 'n', &g_killMarks },
    { kTabEnemies, "enemies.ini", "enabled", "Rare and Elite enemies", "",
      "Some enemy ships appear as Rare or Elite, with traits, a name tag, a guaranteed gun drop and bonus call-in progress.",
      "ON", 'b', 0, 1, 1, 'n', &s_optElites, nullptr, nullptr, R, &g_elitesAtLaunch },
    { kTabEnemies, "enemies.ini", "rare_chance", "Rare enemy chance", "Per enemy ship",
      "Chance that an enemy ship appears as a Rare.",
      "8%", 'd', 0, 0.5, 0.01, 'p', nullptr, &g_rareChance },
    { kTabEnemies, "enemies.ini", "elite_chance", "Elite enemy chance", "Per enemy ship",
      "Chance that an enemy ship appears as an Elite.",
      "2%", 'd', 0, 0.25, 0.01, 'p', nullptr, &g_eliteChance },
    { kTabEnemies, "enemies.ini", "tags", "Name tags over them", "Name and traits over the ship",
      "Shows a Rare or Elite enemy's name and traits over it.",
      "ON", 'b', 0, 1, 1, 'n', &g_eliteTags },
    { kTabCrew, "drops.ini", "crew_tracers", "Red tracers on crew weapons", "Easier to follow their fire",
      "Shots from your crew's handheld weapons fly red tracers. Rockets, rifle grenades and found guns with their own tracer color keep their look.",
      "ON", 'b', 0, 1, 1, 'n', &g_crewTracers },
    { kTabCrew, "drops.ini", "crew_marker", "Marker over an aiming crew member", "Bright when they fire",
      "A square over a crew member who is aiming, bright when they fire.",
      "ON", 'b', 0, 1, 1, 'n', &g_crewMarker },
    { kTabCrew, "drops.ini", "crew_aim_line", "Aim line from weapon to target", "Dashed while aiming, solid when ready",
      "A line from the crew member's weapon to their target: dashed while aiming, solid when ready, bright on each shot.",
      "ON", 'b', 0, 1, 1, 'n', &g_crewLine },
    { kTabCrew, "drops.ini", "crew_talk", "Crew speak up in battle", "Lines over their heads",
      "Your crew say short lines over their heads: when hit, after a kill, when a crewmate falls, and in quiet moments about home, their letters and the crew they lost. Each has a voice of their own.",
      "ON", 'b', 0, 1, 1, 'n', &g_talkOn },
    { kTabCrew, "drops.ini", "crew_chatter", "How much they talk", "0.25x rarely .. 2x a lot",
      "How often the crew speak up: in quiet moments and when something happens. Each crew member's own voice still decides who talks most.",
      "x1.00", 'd', 0.25, 2, 0.25, 'x', nullptr, &g_talkChatter },
    { kTabCrew, "drops.ini", "crew_stories", "After-action report", "Opens after each battle",
      "The report on how your crew fought: who was hurt, who was lost and how. OFF: it never opens; the crew files still keep each battle.",
      "ON", 'b', 0, 1, 1, 'n', &g_storiesOn },
    { kTabCrew, "drops.ini", "crew_letters", "Letters home and mail call", "Letters to the families, letters from home",
      "Letters home to the families of crew you lose, and mail call with letters from home. OFF: neither is offered, no new mail comes and the crew do not talk about their letters.",
      "ON", 'b', 0, 1, 1, 'n', &g_lettersOn },
    { kTabCrew, "drops.ini", "no_females", "I don't want females!", "No women at all; overrides the share below",
      "ON: every crew member, enemy crews too, looks stock and new crew get men's names, whatever Women in the crew says. Women stay women in the save and come back when this is OFF; they keep their names.",
      "OFF", 'b', 0, 1, 1, 'n', &g_noFemales, nullptr, nullptr, kOptPortrait },
    { kTabCrew, "drops.ini", "female_crew", "Women in the crew", "Share of crew; 0% = stock figures and hair",
      "Share of crew members who are women, enemy crews too: own hair, no beard or pipe, a female figure. New women get women's first names; crew you have keep their names and are picked once, when first seen. Each crew member's gender is stored in the save, so a change only affects new crew and crew not seen yet; co-op players with the mod see the owner's choice. 0% = stock looks, and stock names for new crew (beards and pipes taken from women stay off)",
      "25%", 'i', 0, 100, 10, 'c', nullptr, nullptr, &g_femalePct, kOptPortrait },
    { kTabCoop, "drops.ini", "coop_guns", "Share found guns", "",
      "Everyone sees each player's found guns on that player's boat, and their shots look right.",
      "ON", 'b', 0, 1, 1, 'n', &s_optCoopG, nullptr, nullptr, R, &g_coopGuns },
    { kTabCoop, "drops.ini", "coop_elites", "Same Rare/Elite enemies", "",
      "The host picks the Rare and Elite enemies, so every player sees the same ones.",
      "ON", 'b', 0, 1, 1, 'n', &s_optCoopE, nullptr, nullptr, R, &g_coopElites },
    { kTabCoop, "drops.ini", "coop_visuals", "Effects on others' shots", "",
      "Effects such as chaining and homing also play on other players' shots. Looks only, no extra damage.",
      "ON", 'b', 0, 1, 1, 'n', &s_optCoopV, nullptr, nullptr, R, &g_coopVisuals },
    { kTabCoop, "drops.ini", "coop_helpers", "Shared gun-spawned units", "",
      "Anything a gun calls in is made by the host, so every player sees it.",
      "ON", 'b', 0, 1, 1, 'n', &s_optCoopH, nullptr, nullptr, R, &g_coopHelpers },
    { kTabCoop, "drops.ini", "coop_torpedoes", "Share found torpedoes", "",
      "Everyone sees each player's found torpedoes as they are: look, speed and homing. Damage stays the firer's.",
      "ON", 'b', 0, 1, 1, 'n', &s_optCoopT, nullptr, nullptr, R, &g_coopTorps },
    { -1, "drops.ini", "debug", "Detailed log (for bug reports)", "", "", "OFF", 'b', 0, 1, 1, 'n', &g_debug },
  };
  return v;
}
// Sets key=value in [section] of a BoatCrewMod ini file: the key's line is replaced, or added under the section header.
static bool IniSet(const char* file, const char* key, const std::string& val) {
  std::string path = g_folder + "\\" + file, text; char buf[4096]; size_t n;
  FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  fclose(f);
  std::string nl = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::string sec = strcmp(file, "enemies.ini") ? "[drops]" : "[enemies]";
  size_t ss = text.find(sec); if (ss == std::string::npos) return false;
  size_t se = text.find(nl + "[", ss + sec.size()); if (se == std::string::npos) se = text.size();
  std::string k = nl + key + "=";
  size_t kp = text.find(k, ss);
  if (kp != std::string::npos && kp < se) { size_t ve = text.find(nl, kp + k.size()); if (ve == std::string::npos) ve = text.size(); text.replace(kp + k.size(), ve - kp - k.size(), val); }
  else text.insert(ss + sec.size(), nl + key + "=" + val);
  f = fopen((path + ".tmp").c_str(), "wb"); if (!f) return false;
  fwrite(text.data(), 1, text.size(), f);
  FinishSave(f, path);
  return true;
}
static std::string OptValueText(const ModOpt& o) {
  char b[48];
  if (o.kind == 'b') return *o.b ? "ON" : "OFF";
  double v = o.kind == 'i' ? *o.i : *o.d;
  if (o.fmt == 'r') return (int)v <= 0 ? std::string("OFF") : std::string(kAutoScrapNames[(int)v > 3 ? 3 : (int)v]);
  if (o.fmt == 'p') snprintf(b, sizeof b, "%.0f%%", v * 100);
  else if (o.fmt == 'q') snprintf(b, sizeof b, "%.1f%%", v * 100);
  else if (o.fmt == 'c') snprintf(b, sizeof b, "%.0f%%", v);
  else if (o.fmt == 'x') snprintf(b, sizeof b, "x%.2f", v);
  else if (o.fmt == 's') snprintf(b, sizeof b, "%.0f s", v);
  else snprintf(b, sizeof b, "%.0f", v);
  return b;
}
static void OptChange(const ModOpt& o, int dir) {                      // dir: -1, +1, or 0 = toggle
  std::string val; char b[48];
  if (o.kind == 'b') { *o.b = !*o.b; val = *o.b ? "1" : "0"; }
  else if (o.kind == 'i') {
    int x = *o.i + dir * (int)o.step; if (x < o.lo) x = (int)o.lo; if (x > o.hi) x = (int)o.hi; *o.i = x; val = std::to_string(x);
    if (o.i == &s_optAutoScrap) { g_autoScrap = x - 1; val = kAutoScrapNames[x]; }   // the file keeps the name
  }
  else { double x = std::round((*o.d + dir * o.step) / o.step) * o.step; if (x < o.lo) x = o.lo; if (x > o.hi) x = o.hi; *o.d = x; snprintf(b, sizeof b, "%g", x); val = b; }
  bool ok = IniSet(o.file, o.key, val);
  Log("options: %s %s=%s%s%s", o.file, o.key, val.c_str(), ok ? "" : " (COULD NOT WRITE the file)", o.when ? (std::string(" (") + o.when + ")").c_str() : "");
}
static bool OptPending(const ModOpt& o) { return o.live && o.b && *o.live != *o.b; }   // a restart-only switch changed since launch
// Rounded corners: a quarter disk texture per corner, tinted, and plain fills between them, so no
// piece overlaps another (the panel fill is not fully opaque).
static void* g_texCorner[4] = {};                                      // top-left, top-right, bottom-left, bottom-right
static void* CornerTex(int k) {
  if (!Alive(g_texCorner[k])) g_texCorner[k] = MakeTex(32, 32, [k](int x, int y, Col4* c) {
    float cx = (k & 1) ? 0.0f : 32.0f, cy = (k & 2) ? 0.0f : 32.0f, dx = x + 0.5f - cx, dy = y + 0.5f - cy;
    float a = 32.0f - sqrtf(dx * dx + dy * dy); *c = { 1, 1, 1, a < 0 ? 0 : (a > 1 ? 1 : a) }; return true; });
  return g_texCorner[k];
}
static void RoundFill(Rect4 r, float rad, Col4 c, bool top = true, bool bottom = true) {
  float t = top ? rad : 0, b = bottom ? rad : 0;
  GuiColor(c);
  Fill({ r.x + rad, r.y, r.w - 2 * rad, r.h });
  Fill({ r.x, r.y + t, rad, r.h - t - b }); Fill({ r.x + r.w - rad, r.y + t, rad, r.h - t - b });
  if (top) { DrawTex({ r.x, r.y, rad, rad }, CornerTex(0)); DrawTex({ r.x + r.w - rad, r.y, rad, rad }, CornerTex(1)); }
  if (bottom) { DrawTex({ r.x, r.y + r.h - rad, rad, rad }, CornerTex(2)); DrawTex({ r.x + r.w - rad, r.y + r.h - rad, rad, rad }, CornerTex(3)); }
}

// The sheet's head: form line, title, MODDED stamp, close box and the rule under them. Returns true when the close box
// is clicked (called on every event).
static bool ManualHeader(Rect4 S, const std::string& title, int type, Vec2 mouse) {
  Rect4 x = { S.x + S.w - 82, S.y + 20, 30, 30 };
  bool click = HitBox(x);
  if (type != 7) return click;
  Text({ S.x + 52, S.y + 6, 560, 20 }, Spaced("BOAT CREW WEAPON MOD") + "   \xC2\xB7   " + Spaced(std::string("FORM ") + kModVersion), 10, kMutedHex);
  Text({ S.x + 52, S.y + 22, 600, 44 }, title, 26, kInkHex, kLeft, true);
  if (!DrawUi({ S.x + S.w - 248, S.y + 7, 150, 56 }, "ui_stamp_modded.png")) {
    Rect4 st = { S.x + S.w - 236, S.y + 19, 126, 32 };
    Frame(st, 3, Hex(kStampRed, 0.85f)); Text(st, "M O D D E D", 18, kRedHex, kCenter, true);
  }
  if (Inside(mouse, x)) FillC(x, Hilite());
  Frame(x, 2, Hex(kInk));
  if (!DrawUi({ x.x + 6, x.y + 6, 18, 18 }, "ui_x_ink.png")) Text({ x.x, x.y - 2, 30, 34 }, "X", 15, kInkHex, kCenter, true);
  FillC({ S.x + 22, S.y + 70, S.w - 44, 2 }, Hex(kInk));
  return click;
}
// The note card for the row under the mouse: ui_notecard.png is 262x372 shown, the card's box x 6..256, y 22..362,
// text-safe x 26..236, y 52..342 (the contract with tools\make_ui_assets.py).
static void NoteCard(const ModOpt& o, float px, float py) {
  if (!DrawUi({ px, py, 262, 372 }, "ui_notecard.png")) { FillC({ px + 9, py + 26, 250, 340 }, Hex(0, 0.2f)); FillC({ px + 6, py + 22, 250, 340 }, Hex(kCard)); }
  float x = px + 26, w = 210, y = py + 52, bottom = py + 342;
  Text({ x, y - 1, w, 20 }, Spaced("NOTE"), 11, kMutedHex); y += 22;
  std::vector<std::string> head = Wrap(o.label, w, 18, true);
  for (size_t k = 0; k < head.size() && k < 2; k++) { Text({ x, y - 3, w, 30 }, head[k], 18, kInkHex, kLeft, true); y += 24; }
  y += 6;
  // the foot: a dashed rule, then where it is filed, its default and when a change applies (a third line if too long)
  std::vector<std::string> foot = { std::string(o.file) + kSep + o.key };
  std::string def = std::string("default ") + o.def, when = o.when ? o.when : "applies at once";
  if ((def.size() + 5 + when.size()) * CharPx(11) <= w) foot.push_back(def + kSep + when); else { foot.push_back(def); foot.push_back(when); }
  float footY = bottom - 10 - 17 * foot.size();
  DashedRule(x, footY, w, Hex(kInkMuted));
  for (size_t k = 0; k < foot.size(); k++) Text({ x, footY + 6 + 17 * k, w, 18 }, foot[k], 11, kMutedHex);
  // the long text between, 13 px, down to 11 px when it would run into the foot
  float avail = footY - 6 - y; int sz = 13; std::vector<std::string> lines;
  for (; sz >= 11; sz--) { lines = Wrap(o.help, w, sz); if (lines.size() * (sz + 6) <= avail) break; }
  if (sz < 11) sz = 11;
  float lh = (float)(sz + 6);
  for (const std::string& l : lines) { if (y + lh > footY - 2) break; Text({ x, y - 2, w, lh + 4 }, l, sz, kInkHex); y += lh; }
}

static Rect4 OptPanel(float w, float h) { return { g_virtW - w - 60 > 40 ? g_virtW - w - 60 : (g_virtW - w) / 2, g_virtH > h ? (g_virtH - h) / 2 : 0, w, h }; }   // right of the title screen's menu
// Mod Options: tabs over the sheet, the tab's rows on the left, the note card on the right, the footer below.
static void OptionsWindow(int type, Vec2 mouse) {
  if (!g_optInit) {
    g_optInit = true;
    s_optTurret = g_turretGuns; s_optSide = g_sideGrades; s_optTracer = g_tracerAtLaunch; s_optElites = g_elitesAtLaunch;
    s_optCoopG = g_coopGuns; s_optCoopE = g_coopElites; s_optCoopV = g_coopVisuals; s_optCoopH = g_coopHelpers; s_optCoopT = g_coopTorps;
  }
  std::vector<ModOpt>& opts = ModOpts();
  Rect4 P = OptPanel(960, 620), S = { P.x, P.y + 38, 960, 582 };       // 620 fits a 720-high screen
  int tab = g_optTab;                                                  // a tab clicked below switches from the next event on
  float colX = S.x + 52, colW = S.w - 52 - 26 - 250 - 52, top = S.y + 86, rowH = 52;
  { int rows = 0; for (const ModOpt& o : opts) if (o.tab == tab) rows++;   // 416 px between the first row and the footer: 8 rows of 52, more get closer
    if (rows > 8) rowH = (std::max)(40.0f, 416.0f / rows); }               // shortcut: 10 rows is the most a tab holds, an 11th needs a scroll or a new tab
  // the note card's subject: the row last hovered on this tab (it stays after the mouse leaves), else the tab's first row
  if (g_optHot < 0 || g_optHot >= (int)opts.size() || opts[g_optHot].tab != tab) {
    g_optHot = -1; for (size_t k = 0; k < opts.size(); k++) if (opts[k].tab == tab) { g_optHot = (int)k; break; }
  }
  { float y = top + 22; for (size_t k = 0; k < opts.size(); k++) if (opts[k].tab == tab) { if (Inside(mouse, { colX, y, colW, rowH })) g_optHot = (int)k; y += rowH; } }
  int pending = 0; bool tabDot[kOptTabs] = {};
  for (const ModOpt& o : opts) if (OptPending(o)) { pending++; if (o.tab >= 0) tabDot[o.tab] = true; }
  if (type == 7) PaperSheet(S);
  for (int t = 0; t < kOptTabs; t++) {                                 // the active tab joins the sheet
    bool on = t == tab;
    Rect4 r = { S.x + 24 + t * 120, on ? P.y : P.y + 6, 118, on ? S.y + 2 - P.y : S.y - P.y - 6 };
    if (HitBox(r) && !on) { g_optTab = t; g_optHot = -1; }
    if (type != 7) continue;
    std::string lab = Spaced(kOptTabName[t]);
    if (on) { RoundFill(r, 8, Hex(UiTex("ui_paper.png") ? kTabOn : kPaper), true, false); Text(r, lab, 13, kInkHex, kCenter, true); }
    else {
      RoundFill(r, 8, Inside(mouse, r) ? Mix(Hex(kTabOff), Hex(kPaper), 0.4f) : Hex(kTabOff), true, false);
      FillC({ r.x, r.y + r.h - 4, r.w, 4 }, Hex(0, 0.12f));
      Text(r, lab, 13, kTabTextHex, kCenter);
    }
    if (tabDot[t]) {                                                   // a restart-only switch on this tab changed since launch
      Rect4 d = { r.x + r.w / 2 + CapsWidth(lab, 13) / 2 + 5, r.y + r.h / 2 - 4, 8, 8 };
      if (!DrawUi(d, "ui_dot.png")) FillC({ d.x + 1, d.y + 1, 6, 6 }, Hex(kStampRed));
    }
  }
  if (ManualHeader(S, Spaced("ORDNANCE OPTIONS"), type, mouse)) g_optOpen = false;
  if (type == 7) Text({ colX, top - 1, colW, 20 }, "SECTION " + std::to_string(tab + 1) + "  \xE2\x80\x94  " + kOptTabWord[tab], 12, kMutedHex);
  float y = top + 22; int n = 0;
  for (size_t k = 0; k < opts.size(); k++) {
    const ModOpt& o = opts[k];
    if (o.tab != tab) continue;
    n++;
    Rect4 row = { colX, y, colW, rowH };
    float rx = row.x + row.w - 6, cy = row.y + rowH / 2;
    if (type == 7) {
      if ((int)k == g_optHot) FillC(row, Hilite());
      FillC({ row.x, row.y + rowH - 1, row.w, 1 }, Hex(kInk, 0.25f));
      Text({ row.x + 6, row.y, 30, rowH }, std::to_string(tab + 1) + "." + std::to_string(n), 12, kMutedHex);
      bool restart = o.when == kOptRestart, changed = restart && OptPending(o);
      Text({ row.x + 44, cy - 22, colW - 44 - 170, 26 }, o.label, 15, kInkHex);
      Text({ row.x + 44, cy + 1, colW - 44 - 170, 18 }, restart ? (changed ? "Changed: game needs a restart to take effect" : "Game needs a restart to take effect") : o.desc,
           11, changed ? kRedHex : kMutedHex, kLeft, changed);
    }
    if (o.kind == 'b') {                                               // [X] ON  [ ] OFF; clicking the marked one does nothing
      Rect4 offBox = { rx - 34 - 6 - 16, cy - 8, 16, 16 }, onBox = { offBox.x - 14 - 24 - 6 - 16, cy - 8, 16, 16 };
      bool on = *o.b;
      if (HitBox({ onBox.x - 4, cy - 14, 16 + 6 + 24 + 6, 28 }) && !on) OptChange(o, 0);
      if (HitBox({ offBox.x - 4, cy - 14, 16 + 6 + 34 + 6, 28 }) && on) OptChange(o, 0);
      if (type == 7) {
        InkCheck(onBox, *o.b); Text({ onBox.x + 22, cy - 11, 30, 22 }, "O N", 12, kInkHex);
        InkCheck(offBox, !*o.b); Text({ offBox.x + 22, cy - 11, 40, 22 }, "O F F", 12, kInkHex);
      }
    } else {                                                           // < value >, the value over an inked line
      double v = o.kind == 'i' ? *o.i : *o.d;
      Rect4 ar = { rx - 14, cy - 8, 14, 16 }, val = { ar.x - 8 - 90, cy - 14, 90, 26 }, al = { val.x - 8 - 14, cy - 8, 14, 16 };
      if (HitBox({ al.x - 5, cy - 14, 24, 28 })) OptChange(o, -1);
      if (HitBox({ ar.x - 5, cy - 14, 24, 28 })) OptChange(o, +1);
      if (type == 7) {
        InkArrow(al, false, v <= o.lo + 1e-9);
        std::string vt = OptValueText(o);
        Text(val, vt, vt.size() > 7 ? 13 : 16, kBlueHex, kCenter, true);   // "Uncommon" wraps in the 90 px box at 16
        FillC({ val.x, val.y + val.h, val.w, 2 }, Hex(kInk));
        InkArrow(ar, true, v >= o.hi - 1e-9);
      }
    }
    y += rowH;
  }
  if (type == 7 && g_optHot >= 0) NoteCard(opts[g_optHot], S.x + S.w - 52 - 250 - 6, top - 14);
  // footer: where the tab's options are filed, pending restarts, the debug log switch, WHAT'S NEW and DONE
  float fy = S.y + S.h - 58;
  if (type == 7) {
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    std::string filed = std::string("Filed to BoatCrewMod\\") + (tab == kTabEnemies ? "enemies.ini" : "drops.ini");
    if (!pending) Text({ S.x + 52, fy + 2, 240, 56 }, filed, 11, kMutedHex);
    else {
      Text({ S.x + 52, fy + 10, 240, 20 }, filed, 11, kMutedHex);
      Text({ S.x + 52, fy + 29, 240, 20 }, std::to_string(pending) + (pending == 1 ? " change waits" : " changes wait") + " for a game restart", 11, kRedHex);
    }
  }
  for (const ModOpt& o : opts) {
    if (o.tab != -1) continue;
    Rect4 box = { S.x + 300, fy + 22, 16, 16 };
    if (HitBox({ box.x - 4, fy + 14, 230, 32 })) OptChange(o, 0);
    if (type == 7) { InkCheck(box, *o.b); Text({ box.x + 24, fy + 2, 210, 56 }, o.label, 11, kInkHex); }
  }
  Rect4 done = { S.x + S.w - 52 - 84, fy + 14, 84, 32 }, news = { done.x - 14 - 124, fy + 14, 124, 32 };
  if (InkButton(news, "WHAT'S NEW", type, mouse, false)) { g_newsOpen = true; g_optOpen = false; g_newsScroll = 0; }
  if (InkButton(done, "DONE", type, mouse, true)) g_optOpen = false;
}

// What's New: the README's change lists, shortened, newest first. Keep the secret tier out of it, as out of the README.
static const char* kChangelog =
  "## 0.9.11 - Faster loads and fixes\n"
  "- Slow save loads fixed: at every launch the game writes about 255 reference files, and a save loaded in the first minute or two could sit on a black screen for over a minute while they were written. The mod skips those files, so saves load in seconds.\n"
  "- Newsreel: deck shots of your gunners can now look past the gunner's own gun.\n"
  "- Helm view: the mouse cursor no longer flickers while you look around.\n"
  "- Fixes: the camo list can no longer hang; a crash guard in the female crew tidy-up; a hand-set enemy trait size in enemies.ini is no longer changed back at launch.\n"
  "- Co-op: 0.9.11 does not sync with 0.9.10, so everyone needs 0.9.11.\n"
  "\n"
  "## 0.9.10 - Helm view, camo and kill markings\n"
  "- Helm view: press Insert in a battle to ride along in the helmsman's head, and Insert again to leave. Move the mouse to look around. The helmsman keeps steering, WASD still drives the boat, and while you ride along the crew fire at every enemy in sight, shore guns included.\n"
  "- Ten new hull camo schemes in the boat's camo list: Measure 22 Graded, Measure 31 Jungle, Measure 32 Dazzle, Western Approaches, Admiralty 1944, Flaming Tiger, Wild Badger, Big Johnson, Pride and Stormy Night. A save loaded without the mod shows Plain.\n"
  "- Kill markings: rows of small plane, ship and shore-gun silhouettes on your hull, one per kill and a gold one per ten, kept with each save. Mod Options > Visuals hides them.\n"
  "- Co-op: 0.9.10 does not sync with 0.9.9, so everyone needs 0.9.10. The new camos are untested in co-op: wear a stock camo if a partner plays without this version.\n"
  "\n"
  "## 0.9.9 - Long Tom and torpedoes\n"
  "- New Heavy and Heavy+ side-grade on the 3-inch, by request: the 155 mm M1 Long Tom, the US Army's long-range field gun. One huge shell at a time with a massive blast and a very long reach, then a long reload. It is expensive and rare in the loot.\n"
  "- Its Unique, Longer Tom: a much, much bigger gun that hits twice as hard, and every shot shoves your boat back.\n"
  "- Four torpedo side-grades, so the acoustic Mk 24 and Mk 27 have rivals: the British 21-inch Mk VIII (Mk 15 punch, but a short run and an easy wake to spot), the Mk 14 Magnetic (a huge fuse, but about three in ten are duds), the 45 cm Whitehead (cheap, twice the Mk 24's warhead, but no homing; drawn smaller; also as a Whitehead x2, two to a tube) and the Type 93 Long Lance (a huge warhead and the longest run, hard to spot, very expensive; drawn bigger). The Whitehead also fits the sub chaser's side launchers and the Long Lance the PT-6's rear tubes. They are always in the torpedo slots' lists and drop as loot too.\n"
  "- Co-op: 0.9.9 does not sync with 0.9.8, so everyone needs 0.9.9.\n"
  "\n"
  "## 0.9.8 - Balance and options\n"
  "- New Crew options in Mod Options: After-action report (off: the report never opens; the crew files still keep each battle) and Letters home and mail call (off: no letters to the families, no mail call, and the crew do not talk about letters).\n"
  "- Balance pass. Toned down: QF 2-pdr Single, Pom-pom, T-34 Turret, Type 96 Twin and Triple (fewer rounds carried), Twin Type 93 (30-round boxes, less damage), M4 Barge Buster (smaller magazine). Crates now cost 2 Scrap Iron, 3 Brass or 4 Gunmetal. Your drops.ini updates on its own unless you changed those lines.\n"
  "- Shrapnel no longer rolls on the Oerlikon and the Elco. Chaining now also works from guns you aim yourself. Guided, Fast, Bold and Skipping no longer roll on mortars, where they did nothing. Deadeye no longer rolls on the howitzer.\n"
  "- Uniques last 8 missions (the ones you have get 2 more) and an Elite can now drop one. Rainmaker, The Mole and The Bonesetter reload faster instead of homing, which mortar shells cannot do.\n"
  "- Damage bonuses that depend on the situation (Giant Slayer, Opening Salvo, Close Quarters, Long Shot, Veteran's) now add up instead of multiplying, capped at +200% together.\n"
  "- Vampiric, Salvager and Lazarus give half their ammo and repair for plane kills.\n"
  "- Automated utility mortars now drop their shells on the ship they fire at, with some scatter. Before, every shell fell on one old aim point.\n"
  "- Fixes from a code review, and the source is now on GitHub (github.com/davidnendza/boat-crew-weapon-mod).\n"
  "- Co-op: 0.9.8 does not sync with 0.9.7, so everyone needs 0.9.8.\n"
  "\n"
  "## 0.9.7 - More Weapons\n"
  "- Requested on Discord, and only minimally tested: 28 new historical guns and 29 new Uniques.\n"
  "- (suggestion) The new guns were asked for on Discord, among them the 380mm RW 61 (the Sturmtiger's rocket mortar), as the 8.8 cm Flak 36 and the Flakvierling shield were in 0.9.6. Thanks to everyone who asked.\n"
  "- New Heavy and Heavy+ side-grades: T-34 Turret 76 mm (on the 3-inch), 1.1-inch Chicago Piano and Breda 37/54 Twin (on the Dual Bofors), QF 2-pdr Single and 3.7 cm SK C/30 (on the Bofors), and for Heavy+ only the 10.5 cm SK C/32 (on the 5-inch).\n"
  "- New Medium side-grade, the MG 151/20 Drilling (on the twin Oerlikon), and five new turret guns: Breda 20/65 Mod. 40, Madsen 20 mm and Polsten 20 mm (on the Oerlikon), Twin MG 34 (on the Lewis) and Twin DShK (on the M2).\n"
  "- New Utility side-grades: Holman Projector (on the 60 mm mortar), 7.5 cm Pak 40 and 380mm RW 61 (on the 75 mm howitzer; the Sturmtiger's 38 cm rocket mortar: 60 s reload, ten ammo crates per round), 45 mm 21-K, Type 1 47 mm AT and Blacker Bombard (on the 37 mm AT).\n"
  "- New Rocket side-grades: 16-M-8 Katyusha (on the X50), M-13 Katyusha, 4.5-inch Beach Barrage Rocket and 20 cm Naval Rocket (on the Mk 50), and the Hedgerow (on the Mousetrap).\n"
  "- The Hedgerow throws a 24-bomb salvo of Hedgehog-style spigot bombs (the 150 mm mortar's shell). The 380mm RW 61 fires a rocket-assisted shell: it crawls out of the barrel, then speeds away. The T-34 Turret's gun elevates only from 5 degrees down to 30 up, as the real F-34. For modders: new weapons.ini keys elevation=, crew_offset=, pivot_offset=, projectile_from= and crates_per_round= (see the README).\n"
  "- Model fixes from Discord: the Flakvierling shield is now two side panels like the real mount (the guns elevate between them), the 8.8 cm Flak 36 is the real gun's size, and the T-34 Turret has the 1942 turret's proportions and mantlet and sits centred on a round base.\n"
  "- New crew weapon side-grades: M2 Stinger (on the handheld M1919), M1 Garand and Bren Mk I (on the BAR), PIAT (on the Bazooka) and Type 89 Knee Mortar (on the M7 grenade launcher).\n"
  "- 29 new Uniques: one for each new side-grade and turret gun above and one for the 8.8 cm Flak 36 (Acht-Acht), so every turret gun and side-grade now has its own Unique. Among them Tankograd (T-34 Turret), Stalin's Organ (16-M-8 Katyusha), The Ping (M1 Garand) and Spud Gun (Holman Projector).\n"
  "- Old Reliable and Marching Home: their kill-refill effect did nothing on crew weapons (endless ammo); now +100% magazine and kills give downed crew 20 s more. Found copies update on their own.\n"
  "- Co-op: 0.9.7 does not sync with 0.9.6, so everyone needs 0.9.7.\n"
  "\n"
  "## 0.9.6\n"
  "- Battle newsreel: a second camera films the best moments of each battle and writes them as a black-and-white, old-film-style GIF (640x360, up to about 60 s) into BoatCrewMod\\newsreels when the battle ends. It films planes going down (a chase camera follows the fall), enemy ships sinking, planes swooping in close, and your own crew on deck while your guns fire: gunners over the shoulder, ammo runners from behind. Night battles get automatic exposure so the footage can be seen.\n"
  "- Newsreel settings: Battle newsreel GIF and Newsreels to keep in Mod Options, Visuals tab (newsreel and newsreel_keep in drops.ini). Switch it off if battles run slower with it; it also holds up to about 300 MB of film in memory during a battle. Each GIF can be up to about 80 MB and the newest 10 are kept by default. Only files named like the mod's own (newsreel_<date>_<time>.gif) are ever deleted, so rename or move one to keep it. Your drops.ini gets both at the next launch.\n"
  "- Newsreel TV: Newsreel TV.html next to the GIFs plays your newsreels on an old 1950s wooden TV set, with a channel knob, a TV listings card, a power knob and snow and hiss between channels (not tested in game yet).\n"
  "- Crew bonds: each crew member's closest mate is worked out from shared battles, manning the same gun, shared kills, sinkings and swims together. The closest mate is named in CREW FILES (Closest aboard), on the memorial and in the letter home. Bonds start counting from your first 0.9.6 battle.\n"
  "- Close mates talk to each other, in quiet moments and in battle chatter. When a man falls, his closest mate speaks first and grieves him most afterwards, in the roll call too.\n"
  "- Fewer repeated lines: a line naming a crewmate is used once a battle, and last battle's lines are only said again when nothing else fits, also after a game restart.\n"
  "- Nicknames: crew earn one of 13 nicknames from their record (Jonah, Lazarus, Lucky, Stitches, Slick, Plank Owner, Doc, Chips, Wheels, Mule, Scrounger, Trigger, Splinters). Offers show on the after-action report with KEEP and VETO, at most 3 waiting; a vetoed name is never offered to that man again. A kept name shows on the crew card, in CREW FILES and in the report from the next battle on, and is used in talk, the roll call, the memorial and the letter home.\n"
  "- The dead man's station: when a crew member lost in the last 3 battles is replaced at his gun or job, the after-action report tells it once, and the new man may speak about taking over his station (not tested in game yet).\n"
  "- After-action report: crew thrown into the water as the boat sank are told as part of the sinking, and men taken prisoner are told apart from the lost and missing. Letters home follow the same rule.\n"
  "- After-action report wording: kill tallies now add up, twin-mount kills are credited together, timing words are more accurate, shell bursts and plane rakes are told apart, 'half the crew' is only said when it is true, splinter and graze sentences repeat less, and ship names are corrected.\n"
  "- New side-grade, asked for on Discord: the 8.8 cm Flak 36, the famous 88, for the Heavy and Heavy+ slots. It is built on the 3-inch and has its own model on the 3-inch pedestal and shield. It has a heavier shell, a bigger blast and a 3600 m reach, but three rounds a load, fewer shells carried, a slower rate and 1.5x the price (not tested in game yet).\n"
  "- The 2 cm Flakvierling 38 now has a gun shield, asked for on Discord: a front plate with openings for the four barrels and the sight, and two angled side wings. It turns and elevates with the guns. Looks only, it stops nothing.\n"
  "- Loot share: the turret guns and side-grades of a slot type now share a fixed part of that slot's gun drops (mod_gun_share in drops.ini, default 0.25 = 1 in 4), split among them by their weight=. Adding mod guns now adds variety, not more mod guns. In practice turret guns drop less often than before (they were about 43% of Tub Turret drops), Medium side-grades about twice as often, Heavy, Utility and Rocket side-grades a little more often, and crew weapons and Heavy+ side-grades a little less. 0 goes back to each gun's own weight=. Your drops.ini gets the key at the next launch (not tested in game yet).\n"
  "- Going back to 0.9.5 forgets nicknames, crew bonds and dead man's station records.\n"
  "- Co-op: 0.9.6 does not sync with 0.9.5, so everyone needs 0.9.6.\n"
  "## 0.9.5\n"
  "- Crew attachments, initial version: your crew now remember their battles, have a past of their own, write and get letters, and talk. This is the initial version of the crew attachments feature; more is coming for it.\n"
  "- After-action report: after every battle a sheet on the after-battle screen tells the battle as a story, a summary and then a paragraph for each crew member of your boat: hits (where, by what shell, which enemy gun and ship), wounds, downs, overboard, pick-ups, deaths and kills. It opens by itself once per new report (REPORT tag to open it again); scroll with the mouse wheel, DONE or Esc closes it. The same text is added to crew-reports.txt in BoatCrewMod.\n"
  "- Crew files: a CREW FILES tag on the crew screen opens a file on every crew member with a battle on file: portrait, status, lifetime tallies and each battle's report, newest first. Each save keeps its own crew files, like the inventory; saves from before 0.9.5 and new campaigns start empty.\n"
  "- Crew past: each crew member on file gets a hometown, a job before the war, a keepsake, a plan for after the war and next of kin, steered by the game's own traits (Has Kids, Orphaned, Drunkard, ...). Nobody aboard shares a home, job, keepsake or plan. All of it is invented; the game has no such data.\n"
  "- The lost: THE LOST tab in the crew files lists the dead, missing and captured, each with a memorial (service, effects returned, what they were going to do) and a KIA, LOST or CAPTURED stamp on the photo. A missing or captured crew member who comes back leaves the list.\n"
  "- Letters home: when you lose crew, the letters open after the report (or from the LETTERS HOME tag, or WRITE TO THE FAMILY on the report) with a draft to each family. Pick THE TRUTH or SPARE THEM, OTHER LINES for other wording, then SIGN, or SIGN ALL when there are several. LATER leaves it for the crew files (WRITE THE LETTER). Signed letters stay in the file.\n"
  "- Mail call: every few battles MAIL CALL opens after the report with letters from home for 2 or 3 of the crew who fought, from family, sweethearts and friends whose stories go on over several letters. Mail for a lost crew member comes back marked. Letters are kept in the crew file.\n"
  "- Talking crew: crew say short lines over their heads when hit, after a kill, when a crewmate goes down, dies or goes overboard, at a fire, and in quiet moments about home, their letters and the crew they lost. Each has one of 8 personalities (joker, stoic, crazy, silent, hothead, worrier, professional, cheerful), steered by traits.\n"
  "- Battle chatter: during a gunfight someone speaks every 9 to 19 s at the normal setting, in line with their job, and the crew shout when a burst rakes the deck. In the first battle after you lose crew, those who were there hold a roll call, and the lost names are met with silence. Only crew who knew the dead mourn them.\n"
  "- Crew speak up in battle and How much they talk in Mod Options, Crew tab (crew_talk and crew_chatter in drops.ini), switchable while playing. Your drops.ini gets both at the next launch. Only the talk can be switched off; the report, crew files, letters and mail are always on.\n"
  "- Modders: reports.ini, life.ini and talk.ini in BoatCrewMod replace any built-in section of the report, crew past and talk wording; reports.default.ini, life.default.ini and talk.default.ini show the built-in text.\n"
  "- The PT-6's Depth Charge Slot now offers smoke pots, life rafts and the split loads (not tested in game yet).\n"
  "- Fix: found torpedoes now fit every torpedo slot type that offers their stock torpedo, as found guns already did (a Mk 8 found for a rear slot also shows in a front slot that takes the Mk 8).\n"
  "- Fix: Guided and Homing shots fired in a high arc (a Guided Mousetrap, ...) fly their own arc and only start steering near the top, so they no longer skim into the water. Shots climbing at planes steer as before.\n"
  "- Co-op: 0.9.5 does not sync with 0.9.4, so everyone needs 0.9.5. The crew features cover your own boat only and are untested in co-op.\n"
  "## 0.9.4\n"
  "- (suggestion) Smoke pots and life rafts, suggested by Reiver: new choices in the depth charge slots (PT boats, sub chaser K-guns and stern racks) and the PT boats' torpedo slots. They roll off like depth charges.\n"
  "- Smoke Pot (200 RP): floats where it lands and burns for 150 s (smoke_seconds in drops.ini), puffing the smoke artillery's clouds into a screen about 150 m across that most enemy ships cannot spot you through. Several pots at once share the game's clouds and come out thinner.\n"
  "- Life Raft (300 RP): heads for your nearest crew member in the water (raft_speed in drops.ini) and picks them up: safe, out of this battle, back with your crew after it.\n"
  "- Life Rafts L + Smoke Pots R (250 RP each): rafts on the left launcher, smoke pots on the right, so each fire button always drops the same thing; from 1 + 1 up to 8 + 8 depending on the slot.\n"
  "- Fix: found turret guns and side-grades (Veteran's Type 96, Homing Flak 38, ...) were also offered in Heavy, Heavy+ and Medium slots, where their model has no mount. They are now only offered where their gun fits. If one was mounted there, that slot loses it on the next load; the gun is still yours, so mount it in its own slot again.\n"
  "- Co-op: 0.9.4 does not sync with 0.9.3, so everyone needs 0.9.4.\n"
  "## 0.9.3\n"
  "- Female crew: women in the crew, with their own hair (bob, bun, ponytail or victory rolls), no beard or pipe, and a female figure that their clothes follow. Women in the crew in Mod Options (female_crew in drops.ini) sets the share. 0% gives the stock figures and hair, but beards and pipes taken from women stay off in the save.\n"
  "- Female crew: a quarter of the crew are women by default (Women in the crew, 25%). New recruits who are women get women's first names; surnames are as before. Crew you already have keep their names, and the share picks which of them are women. Enemy crews too. Co-op players need the mod to see the women.\n"
  "- Female crew: a woman with no shirt wears a light grey bra that follows her figure (off under the armored and assault vests and the flak jacket), and women's lips are a muted rose instead of a dark line.\n"
  "- Female crew: I don't want females! in Mod Options (no_females in drops.ini) turns women off completely, enemy crews too, whatever the share says: stock figures and hair for everyone; new crew get men's names; women you already have keep their first names. The share and the stored genders are kept, so switching it off again brings the same women back.\n"
  "- Female crew fix: changing a woman's gear in the crew loadout no longer flashes the stock men's clothes for a moment; she keeps her old look until the new one is ready.\n"
  "- Female crew: each crew member's gender is now decided once and stored with them in the save, so renaming a crew member never changes it. Changing Women in the crew later keeps everyone whose gender is already stored; only new crew and crew the mod has not seen yet follow the new share. At 0% everyone looks stock, but the stored genders stay in the save. In co-op, players with the mod see the owner's choice for each crew member.\n"
  "- New gun effects: Marking (hits mark a ship for 8 s, and all your guns do more damage to it; marked ships get a red tag), Giant Slayer (more damage to Rare and Elite ships), Opening Salvo (more damage for the first magazine of each battle), Close Quarters and Long Shot (more damage within 400 m, or beyond 1000 m).\n"
  "- New gun effects: Overkill (a kill's leftover damage hits the nearest other enemy), Veteran's (more damage with each kill by its gunner, lost if that gunner dies), Rallying (kills give your downed crew more time), Sure-Footed (its hits make your crew less likely to be thrown overboard), Frugal (a chance a shot uses no ammo) and Drum (a much bigger magazine, slower reloads).\n"
  "- New drawbacks: Wild (more damage, less accurate) and Brittle (the gun takes more damage). The new damage effects count blast damage too, not only direct hits.\n"
  "- Automated: a 25% wider spread instead of -20% accuracy (the penalty was much bigger than it read). Automated guns you already have are converted.\n"
  "- Co-op: 0.9.3 does not sync with 0.9.2, so everyone needs 0.9.3.\n"
  "## 0.9.2\n"
  "- New Unique Flying Fish: a torpedo that skips across the surface as a flying fish, very fast but seen by every ship, so they can steer away.\n"
  "- New Unique Sneaking Eel: a torpedo no ship ever spots, so none steer away. It crawls along the surface as a commando.\n"
  "- The Chomper, Chompette and Sneaking Eel now show their look in the tubes and racks too, before they are fired.\n"
  "- The Flying Fish now looks like one: a flying fish that spreads its wing fins as it skips, its tail beating on the water. In the tubes it rests with its fins folded.\n"
  "- Homing torpedoes now chase submarines below the water and dive onto them, as the game's own acoustic torpedoes do.\n"
  "- Co-op: everyone sees each player's found torpedoes as they are, with their look, speed and homing (Share found torpedoes in Mod Options). Damage stays the firer's.\n"
  "- Heal and chain streaks: a green streak flies from the wreck to your boat when a kill repairs your hull (Vampiric, Lazarus, ...), and a bright streak jumps to the next target on each chain leap. In co-op, the other players see your heal streaks while Effects on others' shots is on (each player's Heal and chain streaks switch decides what they see).\n"
  "- Smoking barrels: a gun that fires steadily for a few seconds smokes from its muzzle for a while after it stops (looks only; Smoking barrels in Mod Options, barrel_smoke_after and barrel_smoke_seconds in drops.ini).\n"
  "- Mod Options and What's New have a new look: a field manual with tabs for Loot, Arsenal, Visuals, Enemies, Crew and Co-op, each option explained, and switches that need a game restart say so.\n"
  "- The inventory window is now a plywood field crate: stencilled labels, dark compartments for the grids, the workshop as paper slips under your guns and an ISSUED stamp on equipped guns. The crate slot machine got the same look.\n"
  "- Kill cam fix: it now follows a torpedo that will really hit, judged against the ship's own torpedo hit box, and if that one still misses it switches once to another torpedo about to hit.\n"
  "- The kill cam no longer throws crew overboard when the view returns to your boat.\n"
  "- Co-op: 0.9.2 does not sync with 0.9.1, so everyone needs 0.9.2.\n"
  "## 0.9.1\n"
  "- Found torpedoes: torpedoes drop for the PT boats and the sub chaser, with their own effects: Heavy, of Thunder, Swift, of the Long Run, Proximity, Seeking (homing), of the Dolphin (leaps out of the water) and of Thrift. As in the game, they only hit the bigger ships; homing torpedoes and the kill cam leave patrol boats, gunboats, armored boats and landing craft alone.\n"
  "- New Uniques Chomper and Chompette: homing torpedoes that ride the waves as a chomping yellow ball (chomp_volume in drops.ini sets their volume, 0 = silent).\n"
  "- Torpedo kill cam: when one of your torpedoes is about to hit a ship, the camera follows it in slow motion, then waits a minute before the next one (single player only; Mod Options).\n"
  "- Crew weapons are easier to follow: red tracers (rockets, rifle grenades and guns with their own tracer color keep theirs), a marker over a crew member who is aiming and an aim line to their target. Each can be turned off in Mod Options.\n"
  "- Fewer, better drops: a burst of RP crates gives a drop 40% of the time (was 100%), and more found guns are Uncommon or Rare (effect_counts 20,50,30, was 35,45,20). Your drops.ini only changes where a line still has the old default.\n"
  "- On a boat with torpedo slots, about 1 drop in 40 is a torpedo (Torpedo share of drops in Mod Options).\n"
  "- Auto-scrap (Mod Options): new drops up to the rarity you pick go straight to materials. Uniques and torpedoes are never auto-scrapped.\n"
  "- SCRAP ALL in the loot grid turns every loot gun and torpedo into materials, except Uniques: click it, then CONFIRM within 3 s.\n"
  "- Paint schemes: about a third of found guns come in a paint scheme of the period (Haze Grey, Measure 21 Navy Blue, Olive Drab, Kure Grey, ...), named on their Look: line. Uniques are gold.\n"
  "- Found guns fit every slot type their stock gun goes in.\n"
  "- Uniques rebalanced: The Metronome (every 3rd shot x4, +50% fire rate and magazine), Hornet's Nest (80% chain leaps at 60% damage, +25% fire rate), The Ferryman (+250% damage, double blast) and Lazarus (6 missions, 50% repair, reloads twice as fast). Uniques you already have get the new numbers.\n"
  "- 28 new Uniques: one for every turret gun and side-grade, and new ones for rockets, crew weapons, the Meat Chopper, the 5-inch and the 107 mm mortar.\n"
  "- Homing shells aim at a ship's real height, so they stop flying over small and shrunken boats, and turn hard in the last stretch. Homing mortar rounds you aim yourself lock onto the enemy nearest where they come down.\n"
  "- Mod Options and this What's New window on the title and loadout screens, and the mod's version next to the MODDED tag.\n"
  "- Co-op: 0.9.1 does not sync with 0.9.0, so everyone needs 0.9.1. Found torpedoes are not shared yet.\n"
  "## 0.9.0\n"
  "- Seventeen side-grades for the Heavy, Heavy+, Medium, Utility, Rocket and crew weapon slots, from the QF 2-pdr pom-pom to the Thompson, each with its own model; they drop as loot too.\n"
  "- Co-op support (experimental): other players see your found guns and their effects, and everyone gets the same Rare and Elite enemies.\n"
  "- Guns only wear when they fire: a battle where a gun never shoots costs it nothing.\n"
  "- New effect All-Knowing: while you aim it, your crew fires at every enemy in sight.\n"
  "- Homing works against shore targets; rocket launchers roll Quick and Slow again; worn-out guns give their rarity's material; 8x8 loot grid; bigger size traits; the MODDED title tag.\n"
  "## 0.8.0\n"
  "- Six more turret guns (Twin Vickers K, Twin Vickers .50, Twin Type 93, 2 cm Flak 38, Type 96 25 mm, M4 Barge Buster), also as loot.\n"
  "- Each save has its own inventory.\n"
  "- Found guns are much stronger: more effects, bigger rolls, milder drawbacks; new effect of Cheapness; homing on guns you aim yourself; slow shells keep their range.\n"
  "## 0.7.0\n"
  "- Found guns cost RP like their stock gun. Automated utility guns fire on their own out to about 3 km.\n"
  "## 0.6.0\n"
  "- Rare and Elite enemies with traits, name tags, a guaranteed gun drop and bonus call-in progress.\n"
  "## 0.5.0\n"
  "- Rockets, mortars, the 75 mm howitzer and crew weapons can drop. Uniques. Homing, Chaining and Guided. Tracer colors and gun sizes. The armory inventory look. Custom gun models.\n"
  "## 0.4.0\n"
  "- Flak, Skipping and Shrapnel. Drops match your boat's slots. Heavy+ and Medium guns, including the 5-inch.\n"
  "## 0.3.0\n"
  "- Deadeye, Salvager, Vampiric and Bounty; blue and gold tracers for Uncommon and Rare guns.\n"
  "## 0.2.1\n"
  "- Drops are more common: the default drop chance per burst of RP crates is 33% (was 10%).\n"
  "## 0.2.0\n"
  "- First public test.\n";
struct NewsLine { std::string text; bool head; };
static const std::vector<NewsLine>& NewsLines(int width) {             // wrapped once: about 7.3 px per character at size 13
  static std::vector<NewsLine> lines; static int madeFor = -1;
  if (madeFor == width) return lines;
  madeFor = width; lines.clear();
  size_t maxc = (size_t)(width / 7.3f);
  std::string all = kChangelog; size_t at = 0;
  while (at < all.size()) {
    size_t e = all.find('\n', at); if (e == std::string::npos) e = all.size();
    std::string l = all.substr(at, e - at); at = e + 1;
    if (l.compare(0, 3, "## ") == 0) { if (!lines.empty()) lines.push_back({ "", false }); lines.push_back({ l.substr(3), true }); continue; }
    std::vector<std::string> w; WrapChars(l, maxc, l.compare(0, 2, "- ") == 0 ? "   " : "", &w);
    for (std::string& s : w) lines.push_back({ s, false });
  }
  return lines;
}
static void NewsWindow(void* ev, int type, Vec2 mouse) {
  Rect4 S = OptPanel(900, 620), body = { S.x + 52, S.y + 86, S.w - 104 - 12, S.h - 86 - 58 - 12 };
  const std::vector<NewsLine>& lines = NewsLines((int)body.w);
  float lineH = 20, headH = 34, total = 0; for (const NewsLine& l : lines) total += l.head ? headH : lineH;
  float maxScroll = total - body.h > 0 ? total - body.h : 0;
  if (type == 6 && m_evDelta && Inside(mouse, S)) {                     // ScrollWheel
    void* d = Invoke(m_evDelta, ev); float dy = d ? ((float*)((char*)d + 0x10))[1] : 0;
    g_newsScroll += dy * 20; if (m_evUse) Invoke(m_evUse, ev);
  }
  if (g_newsScroll < 0) g_newsScroll = 0; if (g_newsScroll > maxScroll) g_newsScroll = maxScroll;
  if (type == 7) PaperSheet(S);
  if (ManualHeader(S, Spaced("WHAT'S NEW"), type, mouse)) g_newsOpen = false;
  float fy = S.y + S.h - 58;
  if (type == 7) {
    void* g[3] = { &body, g_guiContentNone, g_guiStyleNone };
    Invoke(m_guiBeginGroup, nullptr, g);
    float y = -g_newsScroll;
    for (const NewsLine& l : lines) {
      float h = l.head ? headH : lineH;
      if (y + h > 0 && y < body.h) {
        if (l.head) { Text({ 0, y + 3, body.w, 26 }, l.text, 15, kInkHex, kLeft, true); FillC({ 0, y + 29, body.w, 1 }, Hex(kInk)); }
        else Text({ 0, y, body.w, lineH }, l.text, 13, kInkHex, kLeft);
      }
      y += h;
    }
    Invoke(m_guiEndGroup, nullptr);
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    if (maxScroll > 0) {                                               // a thin inked scroll bar, for show
      float bh = body.h * body.h / total, by = body.y + (body.h - bh) * (g_newsScroll / maxScroll), bx = body.x + body.w + 9;
      FillC({ bx, body.y, 1, body.h }, Hex(kInk, 0.3f)); FillC({ bx - 1, by, 3, bh }, Hex(kInk));
      Text({ S.x + 52, fy + 2, 300, 56 }, "Scroll with the mouse wheel", 11, kMutedHex, kLeft);
    }
  }
  Rect4 done = { S.x + S.w - 52 - 84, fy + 14, 84, 32 }, opt = { done.x - 14 - 130, fy + 14, 130, 32 };
  if (InkButton(opt, "MOD OPTIONS", type, mouse, false)) { g_optOpen = true; g_newsOpen = false; }
  if (InkButton(done, "DONE", type, mouse, true)) g_newsOpen = false;
}
// ---- the after-action report window ----
// On the stock after-battle screen (CampaignTriggers.lastActivityRaw == 9):
// a REPORT tag, and the last battle's prose report (g_aarParas, rendered by crew_report.h at the battle change) in a
// sheet like What's New. Opens by itself once per new report.
static float g_aarScroll = 0;
struct AarLine { std::string text; int style; bool gap; };
// letters home (crew lives, crew_life.h): the window sits over the after-battle screen (9) or the crew files (2)
static bool g_letterOpen = false; static int g_letterAct = -1, g_letterOffered = 0;
// mail call (crew_mail.h): this battle's letters from home, shown on the after-battle screen after the report and letters
struct MailShown { std::string name; bool female; MailLetter l; };
static std::vector<MailShown> g_mailNow; static int g_mailBattle = 0, g_mailLast = 0; static bool g_mailOpen = false, g_mailUnseen = false;
static float g_mailScroll = 0; static int g_mailSeq = 0;
static int g_crewBattleNoShown = 0;                                    // the battle the after-battle report is of
static int LettersDue(int battle);
static void LetterOpen(int battle, uint32_t first, int act);
static std::vector<uint32_t> AarNickOffers();
static void AarNickRow(uint32_t id, Rect4 S, float oy, int type, Vec2 mouse);
static const std::vector<AarLine>& AarLines(int width) {              // wrapped once per width and report
  static std::vector<AarLine> lines; static int madeW = -1, madeSeq = -1;
  if (madeW == width && madeSeq == g_aarSeq) return lines;
  madeW = width; madeSeq = g_aarSeq; lines.clear();
  for (const RepPara& p : g_aarParas) {
    if (!lines.empty()) lines.push_back({ "", 0, true });
    float cw = p.style ? 8.0f : 7.3f;                                  // bold headings run wider
    std::vector<std::string> w; WrapChars(p.text, (size_t)(width / cw), "", &w);
    for (std::string& t : w) lines.push_back({ t, p.style, false });
  }
  return lines;
}
static void AarWindow(void* ev, int type, Vec2 mouse) {
  std::vector<uint32_t> offers = AarNickOffers();                      // nicknames waiting for KEEP or VETO: a row each above the footer
  Rect4 S = OptPanel(900, 620), body = { S.x + 52, S.y + 86, S.w - 104 - 12, S.h - 86 - 58 - 12 };
  body.h -= 48.0f * offers.size();
  const std::vector<AarLine>& lines = AarLines((int)body.w);
  auto lineH = [](const AarLine& l) { return l.gap ? 10.0f : l.style == 1 ? 28.0f : 20.0f; };
  float total = 0; for (const AarLine& l : lines) total += lineH(l);
  float maxScroll = total - body.h > 0 ? total - body.h : 0;
  if (type == 6 && m_evDelta && Inside(mouse, S)) {                     // ScrollWheel
    void* d = Invoke(m_evDelta, ev); float dy = d ? ((float*)((char*)d + 0x10))[1] : 0;
    g_aarScroll += dy * 20; if (m_evUse) Invoke(m_evUse, ev);
  }
  if (g_aarScroll < 0) g_aarScroll = 0; if (g_aarScroll > maxScroll) g_aarScroll = maxScroll;
  if (type == 7) PaperSheet(S);
  if (ManualHeader(S, Spaced("AFTER-ACTION REPORT"), type, mouse)) g_aarOpen = false;
  float fy = S.y + S.h - 58;
  if (type == 7) {
    void* g[3] = { &body, g_guiContentNone, g_guiStyleNone };
    Invoke(m_guiBeginGroup, nullptr, g);
    float y = -g_aarScroll;
    for (const AarLine& l : lines) {
      float h = lineH(l);
      if (!l.gap && y + h > 0 && y < body.h) {
        if (l.style == 1) { Text({ 0, y + 3, body.w, 24 }, l.text, 14, kInkHex, kLeft, true); FillC({ 0, y + 25, body.w, 1 }, Hex(kInk, 0.5f)); }
        else Text({ 0, y, body.w, h }, l.text, 13, l.style == 2 ? kInkHex : kInkHex, kLeft, l.style == 2);
      }
      y += h;
    }
    Invoke(m_guiEndGroup, nullptr);
    if (lines.empty()) Text(body, "No report for the last battle.", 13, kMutedHex, kLeft);
    FillC({ S.x + 22, fy, S.w - 44, 2 }, Hex(kInk));
    if (maxScroll > 0) {
      float bh = body.h * body.h / total, by = body.y + (body.h - bh) * (g_aarScroll / maxScroll), bx = body.x + body.w + 9;
      FillC({ bx, body.y, 1, body.h }, Hex(kInk, 0.3f)); FillC({ bx - 1, by, 3, bh }, Hex(kInk));
      Text({ S.x + 52, fy + 2, 420, 56 }, "Scroll with the mouse wheel. Also in BoatCrewMod\\crew-reports.txt", 11, kMutedHex, kLeft);
    }
  }
  for (size_t i = 0; i < offers.size(); i++) AarNickRow(offers[i], S, fy - 48.0f * (offers.size() - i), type, mouse);
  int due = g_lettersOn ? LettersDue(g_crewBattleNoShown) : 0;
  if (due && InkButton({ S.x + S.w - 52 - 84 - 14 - 200, fy + 14, 200, 32 }, due == 1 ? "WRITE TO THE FAMILY" : "WRITE TO THE FAMILIES", type, mouse, false)) LetterOpen(g_crewBattleNoShown, 0, 9);
  if (InkButton({ S.x + S.w - 52 - 84, fy + 14, 84, 32 }, "DONE", type, mouse, true)) g_aarOpen = false;   // AfterActionGui offers the letters and mail next
}
// The mod's windows, in the inventory's scaled space. Esc closes them. Returns true while one is open.
static bool g_fileOpen = false, g_fileOff = false;                     // the crew files window (FileCabinetGui)
static void CabWindow(void* ev, int type, Vec2 mouse);
static void LetterWindow(void* ev, int type, Vec2 mouse);
static void MailWindow(void* ev, int type, Vec2 mouse);
static bool ModMenus(void* ev, int type, Vec2 mouse) {
  if (g_aarOff) g_aarOpen = false;
  if (g_fileOff) g_fileOpen = false;
  if (!g_optOpen && !g_newsOpen && !g_aarOpen && !g_fileOpen && !g_letterOpen && !g_mailOpen) return false;
  if (type == 4 && Unbox(Invoke(m_evKey, ev)) == 27) {
    if (g_letterOpen) g_letterOpen = false;                            // Esc on a letter goes back to the window under it
    else if (g_mailOpen) g_mailOpen = false;
    else g_optOpen = g_newsOpen = g_aarOpen = g_fileOpen = false;
    if (m_evUse) Invoke(m_evUse, ev); return false;
  }
  if (g_letterOpen) LetterWindow(ev, type, mouse); else if (g_mailOpen) MailWindow(ev, type, mouse); else if (g_optOpen) OptionsWindow(type, mouse); else if (g_newsOpen) NewsWindow(ev, type, mouse); else if (g_fileOpen) CabWindow(ev, type, mouse); else AarWindow(ev, type, mouse);
  return true;
}

// Starts a scaled IMGUI pass like InventoryGui: returns the event, its type and the mouse in virtual pixels.
static void* ModGuiBegin(int* type, Vec2* mouse) {
  void* ev = Invoke(m_evCurrent, nullptr); if (!ev) return nullptr;
  float sw = (float)Unbox(Invoke(m_scrW, nullptr)), sh = (float)Unbox(Invoke(m_scrH, nullptr));
  g_uiScale = g_uiScaleSet > 0 ? g_uiScaleSet : (sh > 1080 ? sh / 1080.0f : 1.0f);
  if (sw > 0 && sh > 0) { g_virtW = sw / g_uiScale; g_virtH = sh / g_uiScale; }
  GuiScale(g_uiScale);                                                 // first: the mouse position follows GUI.matrix
  *type = Unbox(Invoke(m_evType, ev));
  void* mp = Invoke(m_evMouse, ev); *mouse = mp ? *(Vec2*)((char*)mp + 0x10) : Vec2{ -1, -1 };
  GuiThemeBegin(); GuiColor({ 1, 1, 1, 1 });
  return ev;
}
static void ModGuiEnd() { GuiThemeEnd(); GuiColor({ 1, 1, 1, 1 }); GuiScale(1.0f); }

// ---- MODDED on the title screen ----
// models\modded_banner.png (made by tools\make_modded_banner.py; any PNG will do) is read once with
// ImageConversion.LoadImage, which the game kept, and drawn from TitleScreenManagerUI's OnGUI: the game's own method is
// empty and runs only while the title screen is up. The title screen's left panel scales with the screen height, so the
// place is a share of it: drops.ini modded_banner_place=<center x>,<top>,<width>,<squash>, squash being its height as a
// share of the picture's own; modded_banner=0 hides it.
static void* g_bannerTex = nullptr;
static void TitleBanner() {
  static bool tried = false;
  if (!tried) {
    tried = true;
    std::string path = g_folder + "\\models\\modded_banner.png", det; void* tex = nullptr;
    int r = LoadPngTex(path, &tex, &det);
    if (r == 1) { Log("title screen: %s not found; no MODDED banner", path.c_str()); return; }
    if (r == 2) { Log("title screen: a lookup failed (%s); no MODDED banner", det.c_str()); return; }
    if (r == 3) { Log("title screen: the game could not read %s as an image; no MODDED banner", path.c_str()); return; }
    g_bannerTex = tex;
    Log("title screen: MODDED banner loaded (%dx%d), drawn at %.3f,%.3f width %.3f of the screen height, squashed to %.2f", Unbox(Invoke(m_texW, tex)), Unbox(Invoke(m_texH, tex)),
        g_bannerPlace[0], g_bannerPlace[1], g_bannerPlace[2], g_bannerPlace[3]);
  }
  if (!Alive(g_bannerTex)) return;
  void* ev = Invoke(m_evCurrent, nullptr);
  if (!ev || Unbox(Invoke(m_evType, ev)) != 7) return;                 // Repaint only
  float sh = (float)Unbox(Invoke(m_scrH, nullptr));
  float tw = (float)Unbox(Invoke(m_texW, g_bannerTex)), th = (float)Unbox(Invoke(m_texH, g_bannerTex));
  float w = sh * (float)g_bannerPlace[2], h = tw > 0 ? w * th / tw * (float)g_bannerPlace[3] : 0;
  GuiColor({ 1, 1, 1, 1 });
  DrawTex({ sh * (float)g_bannerPlace[0] - w / 2, sh * (float)g_bannerPlace[1], w, h }, g_bannerTex);
}
// MOD OPTIONS and WHAT'S NEW on the title screen as paper tags in the Field Manual look: paper, an inked
// frame and letter-spaced ink, highlighter on hover.
static bool PaperTag(Rect4 r, const char* label, int type, Vec2 mouse) {
  bool click = HitBox(r);
  if (type == 7) {
    FillC(r, Hex(kPaper)); if (Inside(mouse, r)) FillC(r, Hilite());
    Frame(r, 2, Hex(kInk)); Text(r, Spaced(label), 11, kInkHex, kCenter);
  }
  return click;
}
// Under the banner (or where it would be): the mod's version, MOD OPTIONS and WHAT'S NEW, and the two windows.
static void TitleMenu() {
  int type = 0; Vec2 mouse = {};
  void* ev = ModGuiBegin(&type, &mouse); if (!ev) return;
  float sh = g_virtH, tw = 886, th = 300;
  if (Alive(g_bannerTex)) { tw = (float)Unbox(Invoke(m_texW, g_bannerTex)); th = (float)Unbox(Invoke(m_texH, g_bannerTex)); }
  float w = sh * (float)g_bannerPlace[2], h = g_bannerOn && tw > 0 ? w * th / tw * (float)g_bannerPlace[3] : 0;
  // the version right of MODDED and the buttons under Quit, the left panel's last entry, clear of the game's version line
  // and Changelog button (shares of the screen height, like the banner)
  float cx = sh * (float)g_bannerPlace[0], y = sh * 0.935f;
  if (type == 7) {
    std::string v = std::string("v") + kModVersion;
    if (h > 0) Text({ cx + w / 2 + 6, sh * (float)g_bannerPlace[1] + h - 26, 160, 24 }, v, 11, "#d9d9d9", kLeft);
    else Text({ cx - 160, y - 24, 320, 22 }, "weapon mod " + v, 11, "#d9d9d9", kCenter);
  }
  if (PaperTag({ cx - 152, y, 148, 26 }, "MOD OPTIONS", type, mouse)) { g_optOpen = !g_optOpen; g_newsOpen = false; }
  if (PaperTag({ cx + 4, y, 148, 26 }, "WHAT'S NEW", type, mouse)) { g_newsOpen = !g_newsOpen; g_optOpen = false; g_newsScroll = 0; }
  ModMenus(ev, type, mouse);
  ModGuiEnd();
}
static void SafeTitleMenu() {
  static bool off = false;
  if (off || !g_invReady) return;
  __try { TitleMenu(); } __except (FaultFilter("title screen menu", GetExceptionInformation())) { off = true; GuiScale(1.0f); }
}
static void SafeTitleBanner() {
  if (!g_bannerOn || !m_guiDrawTex || !m_evCurrent || !m_scrH || !m_texW) return;
  __try { TitleBanner(); } __except (FaultFilter("title banner", GetExceptionInformation())) { g_bannerOn = false; }
}

static std::string FemUtf8(void* s);
static std::string FemGShown(void* c);
static uint32_t CrewIdOf(void* c);
