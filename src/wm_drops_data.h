// random drops: loot templates, affixes, kDefaultDrops, uniques (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- random drops ----------------------------------------------------------------------------------------------
// drops.ini holds the chance, the guns a drop can be made from (templates) and the affixes that give it random stats.
// When the player collects an RP crate the mod rolls the chance; on success it builds a new item, appends it to
// inventory.ini and positions.ini and shows a line in the kill feed. The gun appears in the base loadout screen the next
// time a boat is built (the loadout screen builds a fresh boat every time it opens).
struct DropTemplate { std::string label, family, base, builtFrom; double weight = 1; };   // builtFrom: one of kTurretGuns
struct AffixOp { std::string key, expr; };
struct Affix { std::string name, text, title; bool bad = false, suffix = false; double lo = 1, hi = 1, weight = 1; std::vector<AffixOp> ops; std::vector<std::string> excludes; int minSeats = 0; std::vector<std::string> notFamilies;
  std::vector<std::string> onlyBases;   // bases=: the effect only rolls on templates whose base gun name contains one of these
  std::vector<std::string> notBases;    // not_bases=: ... and never on these (rocket launchers have no tracers to color)
  std::vector<std::string> notBuiltFrom;   // not_built_from=: ... nor on templates whose built_from= contains one of these
  std::vector<std::pair<std::string, double>> familyWeights;   // family_weight=Utility Weapon:4: weight x4 on that slot family
  std::string fragmentFrom;             // fragment_from=: shells burst into that stock gun's ammo (see SetFragment)
  bool noScale = false;                 // no_scale=1: effect_scale leaves the roll alone (counts, not multipliers)
  bool cosmetic = false;                // kind=cosmetic: looks only; rolled separately (cosmetic_chance), no rarity, no name word
  int slot = 0;                         // slot=: 0 guns only (an affix without the key), 1 torpedo (found torpedoes only), 2 any
};   // excludes: affix names it never rolls with; title: the word used in the gun's name
static double g_dropChance = 0;

static int g_gridCols = 10, g_gridRows = 6;   // drops.ini grid_cols / grid_rows: the inventory grid, which is the inventory limit
static int g_lootCols = 8, g_lootRows = 8;   // drops.ini loot_cols / loot_rows: guns found this mission, before they are kept
// Materials and crates: scrapping a gun gives a material by its rarity, a worn-out gun gives Scrap
// Iron, materials craft crates, a drop can also be an unopened crate, and crate guns last longer. Tier 0/1/2 = Iron,
// Brass, Gunmetal. Both counts live at the top of inventory.ini.
static int g_crateCost[3] = { 2, 3, 4 };                            // drops.ini crate_cost: materials of the same tier per crate
static double g_crateChance = 0.25;                                 // drops.ini crate_chance: share of drops that are a crate instead of a gun
static double g_crateWeight[3] = { 70, 25, 5 };                     // drops.ini crate_weights: which crate a drop is
static int g_crateBonusUses = 2;                                    // drops.ini crate_bonus_uses: extra missions on guns from crates
static float g_uiScaleSet = 0.0f;                                   // drops.ini ui_scale: inventory window scale (0 = screen height / 1080)
static double g_effectScale = 1.0, g_sizePerEffect = 0.0;           // drops.ini effect_scale / size_per_effect (testing aids)
static double g_effectCounts[3] = { 20, 50, 30 };                   // drops.ini effect_counts: odds of 1, 2, 3 good effects
static double g_drawbackChance = 0.15;                              // drops.ini drawback_chance: one drawback on top of them
static int g_usesBase = 0;                                          // drops.ini uses: missions a found gun lasts (0 = unlimited)
static int g_burstMs = 8000;                                         // drops.ini burst_seconds: crates picked up this close together share one roll (0 = roll per crate)
static bool g_injectLive = true;                                     // drops.ini inject_live=0 turns off adding a new drop to boats that already exist
static bool g_dropMessage = true;                                   // drops.ini message=0 hides the kill-feed line
static std::vector<DropTemplate> g_templates;
static std::vector<Affix> g_affixes;
struct UniqueDef { std::string name, base, flavor, fragmentFrom, model, soundFrom, builtFrom; std::vector<std::string> lines, modelKeep; std::vector<AffixOp> ops; double weight = 1; int uses = 0; bool unusual = false; float modelScale = 1.0f; bool modelOwn = false; };
static std::vector<UniqueDef> g_uniques;
static double g_unusualChance = 0.01;                                // drops.ini unusual_chance (kept out of the default file)
static double g_uniqueChance = 0.04;                                 // drops.ini unique_chance: share of gun drops that are a Unique
static double g_uniqueCrate[3] = { 0.04, 0.10, 0.20 };               // drops.ini unique_crate_chance: per Iron, Brass, Gunmetal crate

// Base guns that fire rockets or rifle grenades (ProjectileStyle Rocket / Rocket_ASW): no tracer to recolor or stretch.
#define ROCKET_BASES "Rocket Launcher,Mousetrap,Modded Launcher,Bazooka,Grenade Launcher"
// the edits behind 'shells burst near the target' (as the Flak affix) and 'bursts into N fragments' (as Shrapnel)
#define UQ_PROX "proj.set.proximity=1\nproj.set.proximityMaxErrorSeconds=0.03\nproj.set.proximityOffsetSeconds=0\nproj.set.SplashDamage=1\n"
#define UQ_FRAG "fragment_from=M2 Browning\nproj.set.fragmentInheritSourceVelocity=0.3\n"
static const char* kDefaultDrops =
  "# BoatCrewWeaponMod: random gun drops.\n"
  "#\n"
  "# A burst of RP crates rolls 'chance' (see burst_seconds). A drop picks one [template], then 1 to 3 good [affix]\n"
  "# entries (effect_counts), sometimes one kind=bad drawback on top (drawback_chance), and sometimes one kind=cosmetic\n"
  "# extra (cosmetic_chance). Each affix rolls a value r between its lo and hi (roll=lo-hi). Edits can use $r, 1/$r,\n"
  "# 2.5*$r-1.5 (2-$r still works, for old files) or a number, for example mul.roundsPerMinute=$r. In text, {pct} becomes the\n"
  "# signed percent change (r=1.3 gives +30), {abs} the unsigned one (r=0.7 gives 30), {val} r as a percent (r=0.25 gives\n"
  "# 25), {int} r rounded (r=4.6 gives 5) and {spread} Wild's spread percent. mul.Inaccuracy scales a gun's spread\n"
  "# (100 - Accuracy; Wild and Automated use it).\n"
  "# Templates use the slot family and base gun names listed in weaponmod.log.\n"
  "# position=prefix|suffix and title= decide where the effect's word goes in the name ('Automated Bofors of Ruin').\n"
  "# not_families= keeps an affix off slot families; family_weight=Family:N multiplies its weight on one (Utility Weapon:4).\n"
  "# Edits: mul./set. on the gun, proj. on its projectiles, mount. on the gun mount, sphere. on its targeting sphere,\n"
  "# fx. for the mod's own effects (fx.set.seek, fx.set.critChance, ...). slot=torpedo puts an affix on found torpedoes\n"
  "# instead of guns; its mul./set. edits then change the torpedo (mul.damage, mul.thrust, mul.despawnDistance, ...).\n"
  "# bases= / not_bases= keep an affix on or off base guns whose name contains a word; excludes= names affixes it never\n"
  "# shares a gun with. not_built_from= keeps an affix off templates whose built_from= contains a word.\n"
  "# built_from=<turret gun> on a template: found guns start from one of the mod's turret guns (Twin Vickers K, ...).\n"
  "\n"
  "[drops]\n# chance: odds that a burst of RP crates also gives a drop (a found gun, a torpedo or an unopened crate). Crates picked up\n# within burst_seconds of each other count as one burst\nchance=0.4\nburst_seconds=8\n# message=0 hides the 'Found: ...' line in the kill feed\nmessage=1\n# inject_live=0 stops a new drop from being added to boats that already exist (it is then only on boats built later)\ninject_live=1\n# grid_cols, grid_rows: size of the inventory grid. Found guns take cells by type (MG 2x1, 20/37 mm 3x1, Bofors, twin\n# mounts, rocket launchers, the 107 mm mortar and the 75 mm howitzer 3x2, Dual Bofors, 3-inch and 5-inch 4x2,\n# torpedoes 4x1). size=WxH on a gun in inventory.ini overrides it\ngrid_cols=10\ngrid_rows=6\n# loot_cols, loot_rows: the loot grid. Drops land there; drag the ones to keep into the inventory grid on the loadout\n# screen. Loot stays until the grid is full; then the oldest loot is left behind to make room for a new drop\nloot_cols=8\nloot_rows=8\n# torpedo_share: share of drops that are a torpedo, when your boat has torpedo slots (0.025 = 1 in 40)\ntorpedo_share=0.025\n# auto_scrap: drops of this rarity or lower go straight to materials instead of the loot grid: none, Common, Uncommon\n# or Rare. Uniques, torpedoes and guns from crates you open are never scrapped\nauto_scrap=none\n# Crates. Scrapping a gun gives Scrap Iron (Common), Brass (Uncommon) or Gunmetal (Rare, 2 for a Unique); a worn-out\n# gun gives the same.\n# crate_cost: materials per Iron, Brass, Gunmetal crate. Iron crates roll normally, Brass at least Uncommon, Gunmetal Rare.\ncrate_cost=2,3,4\n# crate_chance: share of drops that are an unopened crate instead of a gun; crate_weights: Iron, Brass, Gunmetal odds\ncrate_chance=0.25\ncrate_weights=70,25,5\n# crate_bonus_uses: extra missions on guns from crates\ncrate_bonus_uses=2\n# unique_chance: share of gun drops that are a Unique (see [unique] below); unique_crate_chance: the same for Iron, Brass, Gunmetal crates\nunique_chance=0.04\nunique_crate_chance=0.04,0.10,0.20\n# price_scale: found guns cost the RP price of the gun they are based on (a stock gun, or one of the turret guns), times this (0 = free)\nprice_scale=1\n# turret_guns=0 hides the mod's extra turret guns (Vickers K, Vickers .50, Type 93, Flak 38, Type 96, M4 Barge Buster, Breda, Madsen,\n# Polsten, MG 34, DShK)\nturret_guns=1\n# side_grades=0 hides the mod's side-grades for the other slots (pom-pom, Molins 6-pdr, 81 mm mortar, Thompson, ...)\nside_grades=1\n# mod_gun_share: share of each slot's drops that are the mod's own guns (turret guns and side-grades), split\n# between them by their weight=, so adding more of them adds variety, not more of them. 0.25 = 1 drop in 4;\n# 0 = each one's own weight=, as before 0.9.6\nmod_gun_share=0.25\n# tracer_rarity: found Uncommon guns fire blue tracers, Rare guns gold, Uniques orange (0 = stock tracers)\ntracer_rarity=1\n# cosmetic_chance: share of found guns that also get one cosmetic extra (tracer color, long tracers, gun size); looks only\ncosmetic_chance=0.3\n# paint_chance: share of found guns that come in a paint scheme (Haze Grey, Measure 21 Navy Blue, Olive Drab, ...); looks only, not on torpedoes. Uniques are always gold\npaint_chance=0.35\n# effect_counts: odds of 1, 2 and 3 effects on a found gun (Common, Uncommon, Rare)\neffect_counts=20,50,30\n# drawback_chance: odds of one drawback ON TOP of the effects; the good effects then roll in the top half of their range\ndrawback_chance=0.15\n# uses: missions a found gun lasts, plus 1 for each extra effect. It is retired when they run out (0 = unlimited)\nuses=4\n# effect_scale: exaggerates rolled effects around 1 (2 turns +30% into +60%). Use it to make changes obvious while testing; 1 = normal\neffect_scale=1\n# size_per_effect: the mounted gun grows by this much per effect beyond the first, so rarer guns look bigger (0 = no change)\nsize_per_effect=0\n# color_text: rarity and effect colors in names and descriptions; set 0 if the game ever shows the color tags as plain text\ncolor_text=1\n# killcam: when one of your torpedoes is about to hit a ship, the camera follows it in slow motion (single player only;\n# 0 = off). killcam_cooldown: seconds before it can happen again. killcam_slow: game speed meanwhile (0.25 = a quarter)\nkillcam=1\nkillcam_cooldown=60\nkillcam_slow=0.25\n# chomp_volume: the Chomper and Chompette sounds (0 = silent, 1 = normal, up to 2)\nchomp_volume=1\n# smoke_seconds: how long a smoke pot keeps puffing smoke (10-600)\nsmoke_seconds=150\n# raft_speed: a life raft's speed on the water, m/s (1-60)\nraft_speed=8\n# crew_tracers: shots from your crew's handheld weapons fly red tracers (rockets, rifle grenades and found guns with\n# their own tracer color keep their look).\n# crew_marker: a square over a crew member who is aiming, bright when they fire. crew_aim_line: a line from the crew\n# member's weapon to their target, dashed while aiming, solid when ready, bright on each shot. 0 = off\ncrew_tracers=1\ncrew_marker=1\ncrew_aim_line=1\n# crew_talk: your crew speak up in battle: short lines over their heads when hit, after a kill, when a crewmate\n# falls, and in quiet moments about home, their letters from home and the crew they lost. 0 = off\ncrew_talk=1\n# crew_chatter: how often they talk (0.25 = rarely, 1 = normal, 2 = a lot)\ncrew_chatter=1\n# crew_stories: the after-action report that opens after each battle (how your crew fought, who was hurt, who was lost).\n# 0 = it never opens (the crew files still keep each battle). crew_letters: letters home to the families of crew you\n# lose, and mail call (letters from home). 0 = neither is offered and no new mail comes; the crew do not talk about letters\ncrew_stories=1\ncrew_letters=1\n# effect_streaks: a green streak flies from the wreck to your boat when a kill repairs your hull (Vampiric, Lazarus, ...),\n# and a bright streak jumps to the next target on each chain leap. 0 = off\neffect_streaks=1\n# barrel_smoke: your guns' barrels smoke for a while after a long burst (looks only, this machine only; 0 = off).\n# barrel_smoke_after: seconds of steady fire before a gun smokes (1-15); barrel_smoke_seconds: how long it smokes (1-15)\nbarrel_smoke=1\nbarrel_smoke_after=4\nbarrel_smoke_seconds=4\n# female_crew: percent of crew members who are women: their own hair, no beard or pipe and a female figure that the\n# clothes follow. New women get women's first names. Crew you already have keep their names; each is picked once, when\n# first seen. Enemy crews too. 0 = stock figures and hair, and stock names for new crew (beards and pipes taken from\n# women stay off in the save). A crew member's gender is decided once and stored in the save, so a later change only affects new crew and\n# crew not seen yet. Co-op players with the mod see the owner's choice\nfemale_crew=25\n# no_females=1: no women at all, whatever female_crew says: stock figures and hair for everyone; new crew get men's\n# names; women you already have keep their first names. Crew who were women keep that in the save and are women again\n# when this is 0\nno_females=0\n# newsreel: a second camera films the best moments of each battle (planes going down, ships sinking, your crew at\n# their guns) into a black-and-white GIF in BoatCrewMod\\newsreels. It renders an extra view several times a second, so\n# switch it off (0) if battles run slower with it\nnewsreel=1\n# newsreel_keep: how many newsreel GIFs to keep (each can be up to about 80 MB); after a new one is written the oldest go. Only\n# files named exactly like the mod's own (newsreel_<date>_<time>.gif) are ever deleted: rename or move one to keep it.\n# 0 = keep them all\nnewsreel_keep=10\n# kill_marks: tally silhouettes on your hull for planes shot down, ships sunk and shore targets destroyed (a gold\n# one per ten). 0 hides them; the kills are still counted\nkill_marks=1\n# Co-op (every player needs this same version of the mod; see weaponmod.log for what was synced). 0 turns a part off:\n# coop_guns: everyone sees each player's found guns on that player's boat, and their shots look right\n# coop_elites: the host picks the Rare and Elite enemies, so every player sees the same ones\n# coop_visuals: effects such as chaining and homing also play on other players' shots (looks only, no extra damage)\n# coop_helpers: anything a gun calls in is made by the host, so every player sees it\ncoop_guns=1\ncoop_elites=1\ncoop_visuals=1\ncoop_helpers=1\n# coop_torpedoes: everyone sees each player's found torpedoes as they are (look, speed, homing); damage stays the firer's\ncoop_torpedoes=1\n# debug=1 writes a much more detailed weaponmod.log (every loadout change, timings); for bug reports\ndebug=0\n"
  "\n"
  "[template]\nlabel=Lewis\nfamily=Tub Turret\nbase=30 Cal Dual MG (Lewis)\n\n"
  "[template]\nlabel=M1919\nfamily=Tub Turret\nbase=M1919 Browning\n\n"
  "[template]\nlabel=Browning\nfamily=Tub Turret\nbase=M2 Browning\n\n"
  "[template]\nlabel=Tub 20mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\n\n"
  // The mod's turret guns in the loot. base= is the stock gun under each (for effect filters and grid
  // sizes); built_from= gives a found one that turret gun's stats, model, sound and price under its rolled effects.
  // Weight 0.5 each, so turret guns as a whole drop only a little more often than with the stock four alone.
  "[template]\nlabel=Vickers K\nfamily=Tub Turret\nbase=30 Cal Dual MG (Lewis)\nbuilt_from=Twin Vickers K\nweight=0.5\n\n"
  "[template]\nlabel=Vickers .50\nfamily=Tub Turret\nbase=M2 Browning\nbuilt_from=Twin Vickers .50\nweight=0.5\n\n"
  "[template]\nlabel=Type 93\nfamily=Tub Turret\nbase=M2 Browning\nbuilt_from=Twin Type 93\nweight=0.5\n\n"
  "[template]\nlabel=Flak 38\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=2 cm Flak 38\nweight=0.5\n\n"
  "[template]\nlabel=Type 96\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=Type 96 25 mm\nweight=0.5\n\n"
  "[template]\nlabel=Barge Buster\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=M4 Barge Buster\nweight=0.5\n\n"
  "[template]\nlabel=Breda 20mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=Breda 20/65 Mod. 40\nweight=0.3\n"
  "[template]\nlabel=Madsen 20mm\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=Madsen 20 mm\nweight=0.3\n"
  "[template]\nlabel=Polsten\nfamily=Tub Turret\nbase=20 mm Oerlikon\nbuilt_from=Polsten 20 mm\nweight=0.3\n"
  "[template]\nlabel=MG 34\nfamily=Tub Turret\nbase=30 Cal Dual MG (Lewis)\nbuilt_from=Twin MG 34\nweight=0.3\n"
  "[template]\nlabel=DShK\nfamily=Tub Turret\nbase=M2 Browning\nbuilt_from=Twin DShK\nweight=0.3\n"
  "[template]\nlabel=37mm AT\nfamily=Utility Weapon\nbase=M3 37 mm AT\n\n"
  "[template]\nlabel=37mm Auto\nfamily=Utility Weapon\nbase=M4 37 mm Autocannon\n\n"
  "[template]\nlabel=Oerlikon\nfamily=Heavy Weapon\nbase=20 mm Oerlikon\n\n"
  "[template]\nlabel=Twin Oerlikon\nfamily=Heavy Weapon\nbase=20 mm Oerlikon Twin\n\n"
  "[template]\nlabel=Bofors\nfamily=Heavy Weapon\nbase=40 mm Bofors\n\n"
  "[template]\nlabel=Dual Bofors\nfamily=Heavy Weapon\nbase=40 mm Dual Bofors\n\n"
  "[template]\nlabel=Chopper\nfamily=Heavy Weapon\nbase=M45 Meat Chopper\n\n"
  "[template]\nlabel=Elco\nfamily=Heavy Weapon\nbase=20 mm Elco Thunderbolt\n\n"
  "[template]\nlabel=3-inch\nfamily=Heavy Weapon\nbase=3-inch Gun Player\n\n"
  "[template]\nlabel=Oerlikon\nfamily=Heavy+ Weapon\nbase=20 mm Oerlikon\n\n"
  "[template]\nlabel=Twin Oerlikon\nfamily=Heavy+ Weapon\nbase=20 mm Oerlikon Twin\n\n"
  "[template]\nlabel=Bofors\nfamily=Heavy+ Weapon\nbase=40 mm Bofors\n\n"
  "[template]\nlabel=Dual Bofors\nfamily=Heavy+ Weapon\nbase=40 mm Dual Bofors\n\n"
  "[template]\nlabel=Chopper\nfamily=Heavy+ Weapon\nbase=M45 Meat Chopper\n\n"
  "[template]\nlabel=Elco\nfamily=Heavy+ Weapon\nbase=20 mm Elco Thunderbolt\n\n"
  "[template]\nlabel=3-inch\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\n\n"
  "[template]\nlabel=5-inch\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nweight=0.4\n\n"
  "[template]\nlabel=Oerlikon\nfamily=Medium Weapon\nbase=20 mm Oerlikon\n\n"
  "[template]\nlabel=Twin Oerlikon\nfamily=Medium Weapon\nbase=20 mm Oerlikon Twin\n\n"
  "[template]\nlabel=Chopper\nfamily=Medium Weapon\nbase=M45 Meat Chopper\n\n"
  "[template]\nlabel=Elco\nfamily=Medium Weapon\nbase=20 mm Elco Thunderbolt\n\n"
  // rocket launchers, mortars and crew weapons are GunMounts in slots like any gun
  "[template]\nlabel=Mousetrap\nfamily=Rockets\nbase=Mousetrap Mk20\nweight=0.5\n\n"
  "[template]\nlabel=X50 Rockets\nfamily=Rockets\nbase=X50 Rocket Launcher\nweight=0.5\n\n"
  "[template]\nlabel=Mk50 Rockets\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nweight=0.5\n\n"
  "[template]\nlabel=Mk50 Modded\nfamily=Rockets\nbase=Mk50 Modded Launcher\nweight=0.5\n\n"
  "[template]\nlabel=60mm Mortar\nfamily=Utility Weapon\nbase=M2 60 mm Mortar\nweight=0.6\n\n"
  "[template]\nlabel=75mm Howitzer\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nweight=0.6\n\n"
  "[template]\nlabel=107mm Mortar\nfamily=Utility Weapon\nbase=M2 107 mm Mortar\nweight=0.6\n\n"
  "[template]\nlabel=BAR\nfamily=Crew Weapons\nbase=M1918 BAR\nweight=0.5\n\n"
  "[template]\nlabel=Bazooka\nfamily=Crew Weapons\nbase=Bazooka\nweight=0.5\n\n"
  "[template]\nlabel=Handheld M1919\nfamily=Crew Weapons\nbase=M1919 Browning Handheld\nweight=0.5\n\n"
  "[template]\nlabel=Grenade Launcher\nfamily=Crew Weapons\nbase=M7 Grenade Launcher\nweight=0.5\n\n"
  // The side-grades in the loot, the same way: one template per slot family they sit in, weight 0.3 each
  "[template]\nlabel=Pom-pom\nfamily=Heavy Weapon\nbase=40 mm Bofors\nbuilt_from=QF 2-pdr Pom-pom\nweight=0.3\n\n"
  "[template]\nlabel=Pom-pom\nfamily=Heavy+ Weapon\nbase=40 mm Bofors\nbuilt_from=QF 2-pdr Pom-pom\nweight=0.3\n\n"
  "[template]\nlabel=Flak M42\nfamily=Heavy Weapon\nbase=40 mm Bofors\nbuilt_from=3.7 cm Flak M42\nweight=0.3\n\n"
  "[template]\nlabel=Flak M42\nfamily=Heavy+ Weapon\nbase=40 mm Bofors\nbuilt_from=3.7 cm Flak M42\nweight=0.3\n\n"
  "[template]\nlabel=Molins 6-pdr\nfamily=Heavy Weapon\nbase=3-inch Gun Player\nbuilt_from=QF 6-pdr Molins\nweight=0.3\n\n"
  "[template]\nlabel=Molins 6-pdr\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nbuilt_from=QF 6-pdr Molins\nweight=0.3\n\n"
  "[template]\nlabel=Flak 88\nfamily=Heavy Weapon\nbase=3-inch Gun Player\nbuilt_from=8.8 cm Flak 36\nweight=0.3\n\n"
  "[template]\nlabel=Flak 88\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nbuilt_from=8.8 cm Flak 36\nweight=0.3\n\n"
  "[template]\nlabel=Type 96 Triple\nfamily=Heavy Weapon\nbase=20 mm Oerlikon Twin\nbuilt_from=Type 96 25 mm Triple\nweight=0.3\n\n"
  "[template]\nlabel=Type 96 Triple\nfamily=Heavy+ Weapon\nbase=20 mm Oerlikon Twin\nbuilt_from=Type 96 25 mm Triple\nweight=0.3\n\n"
  "[template]\nlabel=Flakvierling\nfamily=Heavy Weapon\nbase=20 mm Elco Thunderbolt\nbuilt_from=2 cm Flakvierling 38\nweight=0.3\n\n"
  "[template]\nlabel=Flakvierling\nfamily=Heavy+ Weapon\nbase=20 mm Elco Thunderbolt\nbuilt_from=2 cm Flakvierling 38\nweight=0.3\n\n"
  "[template]\nlabel=Flakvierling\nfamily=Medium Weapon\nbase=20 mm Elco Thunderbolt\nbuilt_from=2 cm Flakvierling 38\nweight=0.3\n\n"
  "[template]\nlabel=8 cm Type 3\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nbuilt_from=8 cm Type 3\nweight=0.3\n\n"
  "[template]\nlabel=Type 98 Twin\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nbuilt_from=10 cm Type 98 Twin\nweight=0.3\n\n"
  "[template]\nlabel=4.7-inch\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nbuilt_from=QF 4.7-inch Mk IX\nweight=0.3\n\n"
  "[template]\nlabel=Type 96 Twin\nfamily=Medium Weapon\nbase=20 mm Oerlikon Twin\nbuilt_from=Type 96 25 mm Twin\nweight=0.3\n\n"
  "[template]\nlabel=81mm Mortar\nfamily=Utility Weapon\nbase=M2 60 mm Mortar\nbuilt_from=M1 81 mm Mortar\nweight=0.3\n\n"
  "[template]\nlabel=57mm Recoilless\nfamily=Utility Weapon\nbase=M3 37 mm AT\nbuilt_from=M18 57 mm Recoilless\nweight=0.3\n\n"
  "[template]\nlabel=70mm Type 92\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nbuilt_from=Type 92 70 mm\nweight=0.3\n\n"
  "[template]\nlabel=Woofus\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nbuilt_from=7.2-inch Woofus\nweight=0.3\n\n"
  "[template]\nlabel=Land Mattress\nfamily=Rockets\nbase=Mk50 Modded Launcher\nbuilt_from=Land Mattress\nweight=0.3\n\n"
  "[template]\nlabel=Thompson\nfamily=Crew Weapons\nbase=M1918 BAR\nbuilt_from=Thompson M1928\nweight=0.3\n\n"
  "[template]\nlabel=Johnson\nfamily=Crew Weapons\nbase=M1918 BAR\nbuilt_from=Johnson M1941 LMG\nweight=0.3\n\n"
  "[template]\nlabel=Boys\nfamily=Crew Weapons\nbase=M1918 BAR\nbuilt_from=Boys AT Rifle\nweight=0.3\n\n"
  // side-grades, second set
  "[template]\nlabel=T-34 Turret\nfamily=Heavy Weapon\nbase=3-inch Gun Player\nbuilt_from=T-34 Turret 76 mm\nweight=0.3\n"
  "[template]\nlabel=T-34 Turret\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nbuilt_from=T-34 Turret 76 mm\nweight=0.3\n"
  "[template]\nlabel=1.1-inch Quad\nfamily=Heavy Weapon\nbase=40 mm Dual Bofors\nbuilt_from=1.1-inch Chicago Piano\nweight=0.3\n"
  "[template]\nlabel=1.1-inch Quad\nfamily=Heavy+ Weapon\nbase=40 mm Dual Bofors\nbuilt_from=1.1-inch Chicago Piano\nweight=0.3\n"
  "[template]\nlabel=2-pdr Single\nfamily=Heavy Weapon\nbase=40 mm Bofors\nbuilt_from=QF 2-pdr Single\nweight=0.3\n"
  "[template]\nlabel=2-pdr Single\nfamily=Heavy+ Weapon\nbase=40 mm Bofors\nbuilt_from=QF 2-pdr Single\nweight=0.3\n"
  "[template]\nlabel=SK C/30\nfamily=Heavy Weapon\nbase=40 mm Bofors\nbuilt_from=3.7 cm SK C/30\nweight=0.3\n"
  "[template]\nlabel=SK C/30\nfamily=Heavy+ Weapon\nbase=40 mm Bofors\nbuilt_from=3.7 cm SK C/30\nweight=0.3\n"
  "[template]\nlabel=Breda 37/54\nfamily=Heavy Weapon\nbase=40 mm Dual Bofors\nbuilt_from=Breda 37/54 Twin\nweight=0.3\n"
  "[template]\nlabel=Breda 37/54\nfamily=Heavy+ Weapon\nbase=40 mm Dual Bofors\nbuilt_from=Breda 37/54 Twin\nweight=0.3\n"
  "[template]\nlabel=SK C/32\nfamily=Heavy+ Weapon\nbase=5-inch 25 Gun Player\nbuilt_from=10.5 cm SK C/32\nweight=0.3\n"
  "[template]\nlabel=Long Tom\nfamily=Heavy Weapon\nbase=3-inch Gun Player\nbuilt_from=155 mm M1 Long Tom\nweight=0.2\n"
  "[template]\nlabel=Long Tom\nfamily=Heavy+ Weapon\nbase=3-inch Gun Player\nbuilt_from=155 mm M1 Long Tom\nweight=0.2\n"
  "[template]\nlabel=Drilling\nfamily=Medium Weapon\nbase=20 mm Oerlikon Twin\nbuilt_from=MG 151/20 Drilling\nweight=0.3\n"
  "[template]\nlabel=Holman\nfamily=Utility Weapon\nbase=M2 60 mm Mortar\nbuilt_from=Holman Projector\nweight=0.3\n"
  "[template]\nlabel=Pak 40\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nbuilt_from=7.5 cm Pak 40\nweight=0.3\n"
  "[template]\nlabel=45mm 21-K\nfamily=Utility Weapon\nbase=M3 37 mm AT\nbuilt_from=45 mm 21-K\nweight=0.3\n"
  "[template]\nlabel=47mm Type 1\nfamily=Utility Weapon\nbase=M3 37 mm AT\nbuilt_from=Type 1 47 mm AT\nweight=0.3\n"
  "[template]\nlabel=Blacker Bombard\nfamily=Utility Weapon\nbase=M3 37 mm AT\nbuilt_from=Blacker Bombard\nweight=0.3\n"
  "[template]\nlabel=380mm RW 61\nfamily=Utility Weapon\nbase=M1A1 75 mm Howitzer\nbuilt_from=380mm RW 61\nweight=0.3\n"
  "[template]\nlabel=M-8 Katyusha\nfamily=Rockets\nbase=X50 Rocket Launcher\nbuilt_from=16-M-8 Katyusha\nweight=0.3\n"
  "[template]\nlabel=M-13 Katyusha\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nbuilt_from=M-13 Katyusha\nweight=0.3\n"
  "[template]\nlabel=Beach Barrage\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nbuilt_from=4.5-inch Beach Barrage Rocket\nweight=0.3\n"
  "[template]\nlabel=20 cm Rocket\nfamily=Rockets\nbase=Mk50 Rocket Launcher\nbuilt_from=20 cm Naval Rocket\nweight=0.3\n"
  "[template]\nlabel=Hedgerow\nfamily=Rockets\nbase=Mousetrap Mk20\nbuilt_from=Hedgerow\nweight=0.3\n"
  "[template]\nlabel=Stinger\nfamily=Crew Weapons\nbase=M1919 Browning Handheld\nbuilt_from=M2 Stinger\nweight=0.3\n"
  "[template]\nlabel=Garand\nfamily=Crew Weapons\nbase=M1918 BAR\nbuilt_from=M1 Garand\nweight=0.3\n"
  "[template]\nlabel=Bren\nfamily=Crew Weapons\nbase=M1918 BAR\nbuilt_from=Bren Mk I\nweight=0.3\n"
  "[template]\nlabel=PIAT\nfamily=Crew Weapons\nbase=Bazooka\nbuilt_from=PIAT\nweight=0.3\n"
  "[template]\nlabel=Type 89\nfamily=Crew Weapons\nbase=M7 Grenade Launcher\nbuilt_from=Type 89 Knee Mortar\nweight=0.3\n"
  // Found torpedoes: copies of the stock torpedo choices of the PT boats' and the sub chaser's torpedo slots
  // (base= is the choice as weaponmod.log lists it under 'torpedo slot ... offers'). Effects with slot=torpedo roll on
  // them; the gun effects do not.
  "[template]\nlabel=Mk 13 Torpedo\nfamily=New Standard Torpedo Slot\nbase=Mk 13 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 14 Torpedo\nfamily=New Standard Torpedo Slot\nbase=Mk 14 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 18 Torpedo\nfamily=New Standard Torpedo Slot\nbase=Mk 18 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 24 Torpedo\nfamily=New Standard Torpedo Slot\nbase=Mk 24 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 8 Torpedo\nfamily=Rear Torpedo Slot\nbase=Mk 8 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 15 Torpedo\nfamily=Rear Torpedo Slot\nbase=Mk 15 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 24 Torpedo\nfamily=Rear Torpedo Slot\nbase=Mk 24 Torpedo x1\nweight=0.6\n\n"
  "[template]\nlabel=Mk 24 Torpedo Pair\nfamily=Sub Chaser Side Slot\nbase=Mk 24 Torpedo x2\nweight=0.6\n\n"
  // the torpedo side-grades (kSideGrades), as the gun side-grades: one template per slot family, weight 0.3
  "[template]\nlabel=Mk VIII\nfamily=New Standard Torpedo Slot\nbase=Mk 14 Torpedo x1\nbuilt_from=21-inch Mk VIII\nweight=0.3\n\n"
  "[template]\nlabel=Mk 14 Magnetic\nfamily=New Standard Torpedo Slot\nbase=Mk 14 Torpedo x1\nbuilt_from=Mk 14 Magnetic\nweight=0.3\n\n"
  "[template]\nlabel=Whitehead\nfamily=New Standard Torpedo Slot\nbase=Mk 24 Torpedo x1\nbuilt_from=45 cm Whitehead\nweight=0.3\n\n"
  "[template]\nlabel=Whitehead\nfamily=Sub Chaser Side Slot\nbase=Mk 24 Torpedo x2\nbuilt_from=45 cm Whitehead\nweight=0.3\n\n"
  "[template]\nlabel=Whitehead x2\nfamily=New Standard Torpedo Slot\nbase=Mk 24 Torpedo x2\nbuilt_from=45 cm Whitehead x2\nweight=0.3\n\n"
  "[template]\nlabel=Long Lance\nfamily=New Standard Torpedo Slot\nbase=Mk 14 Torpedo x1\nbuilt_from=Type 93 Long Lance\nweight=0.3\n\n"
  "[template]\nlabel=Long Lance\nfamily=Rear Torpedo Slot\nbase=Mk 15 Torpedo x1\nbuilt_from=Type 93 Long Lance\nweight=0.3\n\n"
  "[affix]\nname=Rapid\nkind=good\n# rocket launchers have no fire rate\nnot_families=Rockets\nroll=1.25-1.75\ntext={pct}% fire rate\nmul.roundsPerMinute=$r\nmul.shotCooldown=1/$r\n\n"
  "[affix]\nname=Heavy\nkind=good\nroll=1.3-2.0\ntext={pct}% damage\nproj.mul.BaseDamage=$r\nproj.mul.SplashDamageBase=$r\n\n"
  "[affix]\nname=Blast\nkind=good\nposition=suffix\ntitle=of Ruin\nroll=1.5-2.5\ntext={pct}% blast radius\nproj.mul.SplashDamageRange=$r\n\n"
  "[affix]\nname=Deep\nkind=good\nposition=suffix\ntitle=of Plenty\nroll=1.5-2.5\ntext={pct}% magazine\nmul.MagSize=$r\nmul.MaxAmmo=$r\n\n"
  "[affix]\nname=Quick\nkind=good\nroll=1.5-2.5\ntext=reload {abs}% faster\nmul.reloadCooldown=1/$r\n\n"
  "[affix]\nname=Fast\nnot_bases=Mortar\nkind=good\nroll=1.25-1.75\ntext={pct}% shell speed\nproj.mul.MuzzleVelocity=$r\nproj.mul.MuzzleVelocityRealistic=$r\n\n"
  "[affix]\nname=Jammy\nkind=bad\n# rocket launchers have no fire rate\nnot_families=Rockets\nroll=0.75-0.9\ntext={pct}% fire rate\nmul.roundsPerMinute=$r\nmul.shotCooldown=1/$r\n\n"
  "[affix]\nname=Weak\nkind=bad\nroll=0.75-0.9\ntext={pct}% damage\nproj.mul.BaseDamage=$r\nproj.mul.SplashDamageBase=$r\n\n"
  "[affix]\nname=Slow\nkind=bad\nroll=1.15-1.4\ntext=reload {abs}% slower\nmul.reloadCooldown=$r\n\n"
  "[affix]\nname=Tiny\nkind=bad\nroll=0.65-0.85\ntext={pct}% magazine\nmul.MagSize=$r\nmul.MaxAmmo=$r\n\n"
  "[affix]\nname=Bold\nnot_bases=Mortar\nkind=good\nposition=suffix\ntitle=of Fire\nroll=2.0-4.0\ntext={pct}% shot size (easier hits)\nproj.mul.projectileWidth=$r\nproj.mul.projectileLength=$r\n\n"
  "[affix]\nname=Short-handed\nkind=good\ntitle=Lean\nweight=0.8\nmin_seats=2\nroll=1-1\ntext=needs 1 fewer gunner, -15% fire rate\nexcludes=Automated\nmount.set.seatsRemoved=1\nmul.roundsPerMinute=0.85\nmul.shotCooldown=1.176\n\n"
  "[affix]\nname=Deadeye\nnot_bases=Howitzer\nkind=good\nweight=1\nroll=0.12-0.25\ntext={val}% chance of a x3 critical hit\nfx.set.critChance=$r\nfx.set.critMult=3\n\n[affix]\nname=Salvager\nkind=good\nposition=suffix\ntitle=of Salvage\nweight=1\nroll=0.30-0.60\ntext=kills refill {val}% of its ammo\nfx.set.killAmmo=$r\n\n[affix]\nname=Vampiric\nkind=good\nweight=1\nroll=0.08-0.20\ntext=kills repair {val}% of your most damaged hull part\nfx.set.killRepair=$r\n\n[affix]\nname=Bounty\nkind=good\nposition=suffix\ntitle=of Bounty\nweight=1\nroll=100-300\ntext=ship kills pay +{int} RP\nfx.set.killRP=$r\n\n"
  "[affix]\nname=Flak\nkind=good\nweight=1\nbases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco,Mortar,Howitzer\nroll=1.5-2.2\ntext=shells burst near the target, {pct}% blast radius\nproj.set.proximity=1\nproj.set.proximityMaxErrorSeconds=0.03\nproj.set.proximityOffsetSeconds=0\nproj.set.SplashDamage=1\nproj.mul.SplashDamageRange=$r\n\n[affix]\nname=Skipping\nnot_bases=Mortar\nkind=good\nweight=0.8\nroll=1-1\ntext=shots that hit the water skip off it once\nproj.set.deflect=1\n\n[affix]\nname=Shrapnel\nkind=good\nposition=suffix\ntitle=of Shrapnel\nweight=1\nbases=Bofors,37 mm,3-inch,5-inch,Mortar,Howitzer," ROCKET_BASES "\nno_scale=1\nfragment_from=M2 Browning\nroll=8-16\ntext=exploding shells burst into {int} fragments\nproj.set.fragmentCount=$r\nproj.set.fragmentInheritSourceVelocity=0.3\n\n"
  "[affix]\nname=Green Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=green tracers\nfx.set.tracerColor=1\n\n[affix]\nname=Purple Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=purple tracers\nfx.set.tracerColor=2\n\n[affix]\nname=Cyan Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=cyan tracers\nfx.set.tracerColor=3\n\n[affix]\nname=Orange Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=orange tracers\nfx.set.tracerColor=4\n\n[affix]\nname=Pink Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=pink tracers\nfx.set.tracerColor=5\n\n[affix]\nname=White Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=1-1\ntext=white tracers\nfx.set.tracerColor=6\n\n[affix]\nname=Long Tracers\nkind=cosmetic\nno_scale=1\nnot_bases=" ROCKET_BASES "\nroll=2-3\ntext=long tracers\nproj.mul.projectileLength=$r\n\n[affix]\nname=Oversized\nkind=cosmetic\nno_scale=1\nroll=1.3-1.6\ntext=oversized gun ({pct}% size)\nfx.set.gunScale=$r\n\n[affix]\nname=Compact\nkind=cosmetic\nno_scale=1\nroll=0.7-0.85\ntext=compact gun ({pct}% size)\nfx.set.gunScale=$r\n\n"
  "[affix]\nname=Homing\nkind=good\nweight=0.8\nno_scale=1\nnot_bases=Mortar,Howitzer," ROCKET_BASES "\nroll=0.45-0.60\ntext={pct}% shell speed, shells home in\nproj.mul.timeScale=$r\nproj.mul.FrameLifeTime=1/$r\nfx.set.seek=90\n\n"
  "[affix]\nname=Chaining\nkind=good\nposition=suffix\ntitle=of Chains\nweight=0.8\nno_scale=1\nroll=0.50-0.75\ntext={val}% chance a hit leaps to another ship, 30% less likely with each further leap (50% damage per leap)\nfx.set.chain=6\nfx.set.chainChance=$r\nfx.set.chainDamage=0.5\nfx.set.chainDecay=0.7\n\n"
  "[affix]\nname=Automated\nkind=good\nweight=0.6\nroll=1-1\ntext=fires on its own with no gunner, 25% wider spread\n# rockets and crew weapons already need no gunner\nnot_families=Rockets,Crew Weapons\n# 4x as likely on utility guns, where it also takes over the aiming\nfamily_weight=Utility Weapon:4\nmount.set.requiresCrew=0\nsphere.set.constrainTargets=0\nmul.Inaccuracy=1.25\n\n"
  // All-Knowing: only on guns the player aims by hand (utility guns, rockets); not with Automated,
  // whose utility gun aims itself
  "[affix]\nname=All-Knowing\nkind=good\nweight=0.8\nno_scale=1\nnot_families=Tub Turret,Heavy Weapon,Heavy+ Weapon,Medium Weapon,Crew Weapons\nexcludes=Automated\nroll=1-1\ntext=while you aim it, your crew fires at every enemy in sight, marked or not\nfx.set.allKnowing=1\n\n"
  // Homing slows a shell to a crawl, too slow for a 140-240 m/s rocket or mortar round; these just steer
  "[affix]\nname=Guided\nkind=good\nweight=1\nbases=Howitzer," ROCKET_BASES "\nno_scale=1\nroll=1-1\ntext=shots steer onto their target\nfx.set.seek=60\n\n"
  // of Cheapness: a cheaper gun that wears out sooner. usesCut is applied when the gun is rolled
  "[affix]\nname=Cheap\nkind=good\nposition=suffix\ntitle=of Cheapness\nweight=0.8\nroll=0.40-0.70\ntext=costs {val}% less RP, but lasts a third fewer missions\nfx.set.priceCut=$r\nfx.set.usesCut=0.333\n\n"
  // Gun effects (Marking, Giant Slayer and the rest). The damage ones
  // scale a hit where the game applies it (HealthPart.hitFlash, see the 'gun effects' section), so blast damage counts
  // too; Drum and Wild are plain stat edits, Frugal and Sure-Footed have their own code there.
  "[affix]\nname=Marking\nkind=good\nweight=0.8\nno_scale=1\nroll=0.15-0.30\ntext=hits mark a ship for 8 s: all your guns do +{val}% damage to it\nfx.set.mark=$r\nfx.set.markTime=8\n\n"
  "[affix]\nname=Giant Slayer\nkind=good\nposition=suffix\ntitle=of Giants\nweight=0.8\nroll=1.4-1.8\ntext={pct}% damage to Rare and Elite ships\nfx.set.giantSlayer=$r\n\n"
  "[affix]\nname=Opening Salvo\nkind=good\ntitle=Ambushing\nweight=0.8\nroll=1.6-2.2\ntext={pct}% damage for its first magazine (at least 6 shots) each battle\nfx.set.salvo=$r\n\n"
  "[affix]\nname=Close Quarters\nkind=good\ntitle=Brawling\nweight=0.8\n# crew weapons only fire within 270 m: there it would be a flat bonus\nnot_families=Crew Weapons\nnot_bases=Mortar\nexcludes=Long Shot\nroll=1.3-1.6\ntext={pct}% damage on hits within 400 m\nfx.set.closeBonus=$r\nfx.set.closeDist=400\n\n"
  "[affix]\nname=Long Shot\nkind=good\nposition=suffix\ntitle=of the Long Shot\nweight=0.8\nnot_families=Crew Weapons\n# short-range guns: tub MGs engage within 700-800 m, the Mousetrap is a close-in ASW rocket\nnot_bases=Lewis,M1919,Mousetrap\nexcludes=Close Quarters\nroll=1.3-1.7\ntext={pct}% damage on hits beyond 1000 m\nfx.set.longBonus=$r\nfx.set.longDist=1000\n\n"
  "[affix]\nname=Overkill\nkind=good\nposition=suffix\ntitle=of Overkill\nweight=0.8\nno_scale=1\nroll=0.5-1.0\ntext=a killing hit spills {val}% of its leftover damage onto the nearest enemy\nfx.set.overkill=$r\n\n"
  "[affix]\nname=Veteran's\nkind=good\nweight=0.8\nno_scale=1\nnot_families=Rockets,Crew Weapons\nexcludes=Automated\nroll=0.03-0.06\ntext=+{val}% damage per kill by its gunner (up to 10 kills), lost if that gunner dies\nfx.set.vetPerKill=$r\nfx.set.vetCap=10\n\n"
  "[affix]\nname=Rallying\nkind=good\nweight=0.8\nno_scale=1\nroll=5-12\ntext=kills give your downed crew {int} s more before they bleed out\nfx.set.rally=$r\n\n"
  "[affix]\nname=Sure-Footed\nkind=good\nweight=0.8\nno_scale=1\nroll=0.04-0.08\ntext=each hit steadies your crew: {val}% less chance a hit on your boat throws one overboard (up to 10 hits, fading)\nfx.set.steady=$r\nfx.set.steadyCap=10\nfx.set.steadyDecay=3\n\n"
  "[affix]\nname=Drum\nkind=good\nposition=suffix\ntitle=of the Drum\nweight=0.6\nbases=Bofors,Oerlikon,Elco,Chopper,37 mm Autocannon,Browning,BAR,M1919,Lewis\n# the Boys is a single-shot AT rifle built from the BAR\nnot_built_from=Boys\nroll=2.0-3.0\ntext={pct}% magazine, but reloads 30% slower\nmul.MagSize=$r\nmul.MaxAmmo=$r\nmul.reloadCooldown=1.3\n\n"
  "[affix]\nname=Frugal\nkind=good\nposition=suffix\ntitle=of Economy\nweight=0.8\nno_scale=1\nnot_families=Crew Weapons\nroll=0.15-0.35\ntext={val}% chance a shot uses no ammo\nfx.set.saveAmmo=$r\n\n"
  "[affix]\nname=Wild\nkind=bad\nroll=1.2-1.4\n# seeking shells steer out the spread, and mortars and howitzers have none to widen\nexcludes=Homing,Guided\nnot_bases=Mortar,Howitzer\ntext={pct}% damage, {spread}% wider spread\nproj.mul.BaseDamage=$r\nproj.mul.SplashDamageBase=$r\nmul.Inaccuracy=2.5*$r-1.5\n\n"
  "[affix]\nname=Brittle\nkind=bad\nroll=1.5-2.0\ntext=the gun takes {abs}% more damage when hit\nfx.set.brittle=$r\n\n"
  "[affix]\nname=Heavy Warhead\nkind=good\nslot=torpedo\ntitle=Heavy\nroll=1.4-2.0\ntext={pct}% warhead damage\nmul.damage=$r\n\n"
  "[affix]\nname=Wide Blast\nkind=good\nslot=torpedo\nposition=suffix\ntitle=of Thunder\nroll=1.4-2.0\ntext={pct}% blast radius\nmul.damageRange=$r\n\n"
  "[affix]\nname=Hot Engine\nkind=good\nslot=torpedo\ntitle=Swift\nroll=1.25-1.6\ntext={pct}% engine thrust, runs faster\nmul.thrust=$r\nmul.terminalVelocity=$r\n\n"
  "[affix]\nname=Long Runner\nkind=good\nslot=torpedo\nposition=suffix\ntitle=of the Long Run\nroll=1.5-2.5\ntext={pct}% range\nmul.despawnDistance=$r\n\n"
  "[affix]\nname=Proximity Fuse\nkind=good\nslot=torpedo\ntitle=Proximity\nroll=1.6-2.4\ntext=goes off {abs}% farther from a hull\nmul.hitboxRadius=$r\n\n"
  "[affix]\nname=Seeking\nkind=good\nslot=torpedo\ntitle=Seeking\nweight=0.8\nno_scale=1\nroll=20-35\ntext=steers onto the nearest big ship ahead ({int} degrees a second)\nfx.set.torpSeek=$r\n\n"
  // of the Dolphin: the model leaps out of the water now and then, the torpedo
  // itself runs on at its depth (see TorpLeapUpdate). Not on depth charges (bases=Torpedo).
  "[affix]\nname=Dolphin\nkind=good\nslot=torpedo\nposition=suffix\ntitle=of the Dolphin\nweight=0.8\nno_scale=1\nbases=Torpedo\nroll=4-7\ntext=leaps out of the water like a dolphin about every {int} seconds, +20% engine thrust\nfx.set.leap=$r\nmul.thrust=1.2\nmul.terminalVelocity=1.2\n\n"
  "[affix]\nname=Thrifty\nkind=good\nslot=torpedo\nposition=suffix\ntitle=of Thrift\nweight=0.6\nroll=0.40-0.70\ntext=costs {val}% less RP, but lasts a third fewer missions\nfx.set.priceCut=$r\nfx.set.usesCut=0.333\n\n"
  "[affix]\nname=Sluggish\nkind=bad\nslot=torpedo\nroll=0.7-0.85\ntext={pct}% engine thrust\nmul.thrust=$r\n\n"
  "[affix]\nname=Short Run\nkind=bad\nslot=torpedo\nroll=0.6-0.8\ntext={pct}% range\nmul.despawnDistance=$r\n\n"
  "[affix]\nname=Dud Warhead\nkind=bad\nslot=torpedo\nroll=0.75-0.85\ntext={pct}% warhead damage\nmul.damage=$r\n\n"
  // Uniques (after Diablo 4's unique items): hand-made guns with a fixed name, fixed effects and a flavour line. base=
  // is a stock gun some [template] uses; the gun takes that template's slot family. built_from= puts it on one of the
  // mod's own guns instead, through that gun's loot templates.
  // Rule: a Unique's main effect beats the best roll of the matching affix, plus one or two extras.
  "# Uniques: rare hand-made guns and torpedoes. Each has a fixed name, base and effects (same edit keys as an affix,\n"
  "# plain numbers).\n"
  "# line= is one line of its description (as many as you like), flavor= the closing quote, uses= its missions. They drop\n"
  "# at unique_chance and from crates at unique_crate_chance, and only for slot families your boat has. built_from=<mod gun>\n"
  "# instead of base= puts one on a turret gun or side-grade. Guns you already found follow edits made here.\n"
  "# base= may list several bases, comma separated: the Unique then drops on whichever of them your boat can carry.\n"
  "[unique]\nname=Hornet's Nest\nbase=M2 Browning\nuses=8\nline=shots seek their target\nline=80% chance a hit leaps to another ship, 20% less likely with each further leap (60% damage per leap)\nline=+25% fire rate\nflavor=They never stop coming.\nfx.set.seek=180\nfx.set.chain=6\nfx.set.chainChance=0.8\nfx.set.chainDamage=0.6\nfx.set.chainDecay=0.8\nmul.roundsPerMinute=1.25\nmul.shotCooldown=0.8\n\n"
  "[unique]\nname=Lazarus\nbase=M3 37 mm AT\nuses=8\nline=kills refill all of its ammo\nline=kills repair 50% of your most damaged hull part\nline=reloads twice as fast\nflavor=Every kill buys you another minute.\nfx.set.killAmmo=1\nfx.set.killRepair=0.5\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=The Ferryman\nbase=40 mm Bofors\nuses=8\nline=-88% shell speed, shells home in and keep their range\nline=+250% damage, +100% blast radius\nflavor=He is slow. He always arrives.\nproj.mul.timeScale=0.12\nproj.mul.FrameLifeTime=8.33\nfx.set.seek=90\nproj.mul.BaseDamage=3.5\nproj.mul.SplashDamageBase=3.5\nproj.mul.SplashDamageRange=2\n\n"
  "[unique]\nname=The Metronome\nbase=20 mm Oerlikon Twin\nuses=8\nline=every 3rd shot is a x4 critical hit\nline=+50% fire rate\nline=+50% magazine\nflavor=Tick, tick, tick, BOOM.\nfx.set.nthShot=3\nfx.set.nthMult=4\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.MagSize=1.5\nmul.MaxAmmo=1.5\n\n"
  // Rockets and Crew Weapons, and more for Medium, Heavy+ and Utility
  "[unique]\nname=Hailstorm\nbase=Mk50 Rocket Launcher\nuses=8\nline=rockets steer onto their target\nline=each rocket bursts into 16 fragments\nline=+100% rockets carried\nflavor=Look up.\nfx.set.seek=60\n" UQ_FRAG "proj.set.fragmentCount=16\nmul.MagSize=2\nmul.MaxAmmo=2\n\n"
  "[unique]\nname=Old Reliable\nbase=M1918 BAR\nuses=8\nline=+80% damage\nline=35% chance of a x3 critical hit\nline=+100% magazine\nflavor=Never once let me down.\nproj.mul.BaseDamage=1.8\nproj.mul.SplashDamageBase=1.8\nfx.set.critChance=0.35\nfx.set.critMult=3\nmul.MagSize=2\n\n"
  "[unique]\nname=Buzzsaw\nbase=M45 Meat Chopper\nuses=8\nline=+60% fire rate\nline=+100% magazine\nline=+200% shot size (easier hits)\nline=shots that hit the water skip off it once\nflavor=It doesn't aim. It edits.\nmul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nmul.MagSize=2\nmul.MaxAmmo=2\nproj.mul.projectileWidth=3\nproj.mul.projectileLength=3\nproj.set.deflect=1\n\n"
  "[unique]\nname=The Verdict\nbase=5-inch 25 Gun Player\nuses=8\nline=every 4th shell is a x5 critical hit\nline=+100% blast radius\nline=+50% damage\nflavor=Court is in session.\nfx.set.nthShot=4\nfx.set.nthMult=5\nproj.mul.SplashDamageRange=2\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\n\n"
  "[unique]\nname=Rainmaker\nbase=M2 107 mm Mortar\nuses=8\nline=reloads 30% faster\nline=every 3rd round is a x4 critical hit\nline=+100% blast radius\nflavor=Forecast: heavy, with scattered ships.\nmul.reloadCooldown=0.7\nfx.set.nthShot=3\nfx.set.nthMult=10\nproj.mul.SplashDamageRange=2\n\n"
  // one on each turret gun and side-grade
  "[unique]\nname=Chatterbox\nbuilt_from=Twin Vickers K\nuses=8\nline=+60% fire rate\nline=+150% magazine\nline=shots that hit the water skip off it once\nflavor=Never shuts up. Never runs dry.\nmul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nmul.MagSize=2.5\nmul.MaxAmmo=2.5\nproj.set.deflect=1\n\n"
  "[unique]\nname=Tall Order\nbuilt_from=Twin Vickers .50\nuses=8\nline=+80% shell speed\nline=+50% damage\nline=30% chance of a x3 critical hit\nflavor=Further than you think.\nproj.mul.MuzzleVelocity=1.8\nproj.mul.MuzzleVelocityRealistic=1.8\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\nfx.set.critChance=0.3\nfx.set.critMult=3\n\n"
  "[unique]\nname=Kurogane\nbuilt_from=Twin Type 93\nuses=8\nline=+120% damage\nline=kills refill all of its ammo\nline=kills repair 25% of your most damaged hull part\nflavor=Black iron does not bend.\nproj.mul.BaseDamage=2.2\nproj.mul.SplashDamageBase=2.2\nfx.set.killAmmo=1\nfx.set.killRepair=0.25\n\n"
  "[unique]\nname=Flak Curtain\nbuilt_from=2 cm Flak 38\nuses=8\nline=shells burst near the target, +150% blast radius\nline=+40% fire rate\nflavor=Nothing gets through.\n" UQ_PROX "proj.mul.SplashDamageRange=2.5\nmul.roundsPerMinute=1.4\nmul.shotCooldown=0.714\n\n"
  "[unique]\nname=Kamaitachi\nbuilt_from=Type 96 25 mm\nuses=8\nline=exploding shells burst into 20 fragments\nline=+60% blast radius\nline=+40% damage\nflavor=You never see the cut.\n" UQ_FRAG "proj.set.fragmentCount=20\nproj.mul.SplashDamageRange=1.6\nproj.mul.BaseDamage=1.4\nproj.mul.SplashDamageBase=1.4\n\n"
  "[unique]\nname=Salvage Rights\nbuilt_from=M4 Barge Buster\nuses=8\nline=ship kills pay +500 RP\nline=kills refill all of its ammo\nline=+60% damage\nflavor=Finders keepers.\nfx.set.killRP=500\nfx.set.killAmmo=1\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\n\n"
  "[unique]\nname=Chicago Piano\nbuilt_from=QF 2-pdr Pom-pom\nuses=8\nline=+50% fire rate\nline=+100% belt\nline=exploding shells burst into 10 fragments\nflavor=The whole band plays at once.\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.MagSize=2\nmul.MaxAmmo=2\n" UQ_FRAG "proj.set.fragmentCount=10\n\n"
  "[unique]\nname=Adlerauge\nbuilt_from=3.7 cm Flak M42\nuses=8\nline=shells home in gently, at full speed\nline=+60% shell speed\nline=40% chance of a x3 critical hit\nflavor=It saw you first.\nfx.set.seek=30\nproj.mul.MuzzleVelocity=1.6\nproj.mul.MuzzleVelocityRealistic=1.6\nfx.set.critChance=0.4\nfx.set.critMult=3\n\n"
  "[unique]\nname=Tsetse\nbuilt_from=QF 6-pdr Molins\nuses=8\nline=+100% damage\nline=+50% fire rate\nline=reloads 3x as fast\nflavor=One sting. Then another. Then another.\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.reloadCooldown=0.333\n\n"
  "[unique]\nname=Three Sisters\nbuilt_from=Type 96 25 mm Triple\nuses=8\nline=shots seek their target, at full speed\nline=+60% damage\nline=+50% magazine\nflavor=Three of them, and none of them miss.\nfx.set.seek=120\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\nmul.MagSize=1.5\nmul.MaxAmmo=1.5\n\n"
  "[unique]\nname=Hydra\nbuilt_from=2 cm Flakvierling 38\nuses=8\nline=75% chance a hit leaps to another ship, 20% less likely with each further leap (50% damage per leap)\nline=+30% fire rate\nflavor=Cut off one head...\nfx.set.chain=6\nfx.set.chainChance=0.75\nfx.set.chainDamage=0.5\nfx.set.chainDecay=0.8\nmul.roundsPerMinute=1.3\nmul.shotCooldown=0.769\n\n"
  "[unique]\nname=Hayate\nbuilt_from=8 cm Type 3\nuses=8\nline=+100% shell speed\nline=+80% damage\nline=30% chance of a x3 critical hit\nflavor=Gone before the sound arrives.\nproj.mul.MuzzleVelocity=2\nproj.mul.MuzzleVelocityRealistic=2\nproj.mul.BaseDamage=1.8\nproj.mul.SplashDamageBase=1.8\nfx.set.critChance=0.3\nfx.set.critMult=3\n\n"
  "[unique]\nname=Akizuki's Watch\nbuilt_from=10 cm Type 98 Twin\nuses=8\nline=shells burst near the target, +150% blast radius\nline=+60% damage\nline=reloads twice as fast\nflavor=The sky was never safe.\n" UQ_PROX "proj.mul.SplashDamageRange=2.5\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=The Admiralty\nbuilt_from=QF 4.7-inch Mk IX\nuses=8\nline=+100% magazine\nline=reloads 2.5x as fast\nline=+50% damage\nline=shots that hit the water skip off it once\nflavor=England expects.\nmul.MagSize=2\nmul.MaxAmmo=2\nmul.reloadCooldown=0.4\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\nproj.set.deflect=1\n\n"
  "[unique]\nname=Twin Dragons\nbuilt_from=Type 96 25 mm Twin\nuses=8\nline=kills repair 40% of your most damaged hull part\nline=+50% fire rate\nline=+50% damage\nflavor=One bites, one heals.\nfx.set.killRepair=0.4\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\n\n"
  "[unique]\nname=The Mole\nbuilt_from=M1 81 mm Mortar\nuses=8\nline=exploding rounds burst into 16 fragments\nline=+100% blast radius\nline=reloads 3x as fast\nflavor=It always finds a way in.\n" UQ_FRAG "proj.set.fragmentCount=16\nproj.mul.SplashDamageRange=2\nmul.reloadCooldown=0.333\n\n"
  "[unique]\nname=Doorknocker\nbuilt_from=M18 57 mm Recoilless\nuses=8\nline=+200% damage\nline=shots steer onto their target\nline=kills refill all of its ammo\nflavor=Knock knock.\nproj.mul.BaseDamage=3\nproj.mul.SplashDamageBase=3\nfx.set.seek=60\nfx.set.killAmmo=1\n\n"
  "[unique]\nname=Ronin\nbuilt_from=Type 92 70 mm\nuses=8\nline=while you aim it, your crew fires at every enemy in sight, marked or not\nline=rounds steer onto their target\nline=+100% blast radius\nflavor=No master. No mercy.\nfx.set.allKnowing=1\nfx.set.seek=60\nproj.mul.SplashDamageRange=2\n\n"
  "[unique]\nname=Big Woof\nbuilt_from=7.2-inch Woofus\nuses=8\nline=rockets steer onto their target\nline=+100% damage\nline=+100% blast radius\nflavor=Its bark is the bite.\nfx.set.seek=60\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\nproj.mul.SplashDamageRange=2\n\n"
  "[unique]\nname=Bedtime Story\nbuilt_from=Land Mattress\nuses=8\nline=+100% rockets carried\nline=+100% blast radius\nline=+50% damage\nline=reloads twice as fast\nflavor=Lights out.\nmul.MagSize=2\nmul.MaxAmmo=2\nproj.mul.SplashDamageRange=2\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Chicago Typewriter\nbuilt_from=Thompson M1928\nuses=8\nline=+50% fire rate\nline=+100% drum\nline=30% chance of a x3 critical hit\nflavor=Types fast. Never makes a typo.\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.MagSize=2\nmul.MaxAmmo=2\nfx.set.critChance=0.3\nfx.set.critMult=3\n\n"
  "[unique]\nname=Marching Home\nbuilt_from=Johnson M1941 LMG\nuses=8\nline=kills give your downed crew 20 s more before they bleed out\nline=+80% damage\nline=reloads 3x as fast\nflavor=Johnny comes marching home.\nfx.set.rally=20\nproj.mul.BaseDamage=1.8\nproj.mul.SplashDamageBase=1.8\nmul.reloadCooldown=0.333\n\n"
  "[unique]\nname=Elephant Gun\nbuilt_from=Boys AT Rifle\nuses=8\nline=+150% damage\nline=shots seek their target, at full speed\nline=kills repair 25% of your most damaged hull part\nflavor=Overkill is underrated.\nproj.mul.BaseDamage=2.5\nproj.mul.SplashDamageBase=2.5\nfx.set.seek=90\nfx.set.killRepair=0.25\n\n"
  // one for each side-grade of the second set, and the Flak 36
  "[unique]\nname=Tankograd\nbuilt_from=T-34 Turret 76 mm\nuses=8\nline=+100% damage on hits within 600 m\nline=+40% damage\nline=+50% fire rate\nflavor=Straight off the factory floor and into the river.\nfx.set.closeBonus=2\nfx.set.closeDist=600\nproj.mul.BaseDamage=1.4\nproj.mul.SplashDamageBase=1.4\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\n\n"
  "[unique]\nname=Ragtime\nbuilt_from=1.1-inch Chicago Piano\nuses=8\nline=+100% fire rate\nline=30% chance a shot uses no ammo\nline=+40% damage\nflavor=Four hands on the keys, and nobody misses a beat.\nmul.roundsPerMinute=2\nmul.shotCooldown=0.5\nfx.set.saveAmmo=0.3\nproj.mul.BaseDamage=1.4\nproj.mul.SplashDamageBase=1.4\n\n"
  "[unique]\nname=Dawn Patrol\nbuilt_from=QF 2-pdr Single\nuses=8\nline=+150% damage for its first magazine (at least 6 shots) each battle\nline=+50% fire rate\nline=reloads twice as fast\nflavor=First in, at first light.\nfx.set.salvo=2.5\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Meisterschuss\nbuilt_from=3.7 cm SK C/30\nuses=8\nline=50% chance of a x3 critical hit\nline=+50% damage on hits beyond 1000 m\nline=+50% shell speed\nflavor=One round, one hole.\nfx.set.critChance=0.5\nfx.set.critMult=3\nfx.set.longBonus=1.5\nfx.set.longDist=1000\nproj.mul.MuzzleVelocity=1.5\nproj.mul.MuzzleVelocityRealistic=1.5\n\n"
  "[unique]\nname=Gemelli\nbuilt_from=Breda 37/54 Twin\nuses=8\nline=hits mark a ship for 10 s: all your guns do +50% damage to it\nline=+50% fire rate\nline=+50% damage\nflavor=One points. The other one agrees.\nfx.set.mark=0.5\nfx.set.markTime=10\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\n\n"
  "[unique]\nname=Grey Wolves\nbuilt_from=10.5 cm SK C/32\nuses=8\nline=+150% damage to Rare and Elite ships\nline=+60% damage\nline=ship kills pay +300 RP\nflavor=Every hull a number in the logbook.\nfx.set.giantSlayer=2.5\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\nfx.set.killRP=300\n\n"
  // gunScale 2.5 scales the whole mount copy, seats included; kick is a velocity change on your boat per shot (GfxKick)
  "[unique]\nname=Longer Tom\nbuilt_from=155 mm M1 Long Tom\nmodel=longer_tom\nmodel_keep=Stand,US_3in_base,Bofors Hand Crank\nuses=8\nline=a much, much bigger gun\nline=+100% damage, +60% blast radius\nline=every shot shoves your boat back hard\nline=WARNING: NOT FOR BOATING USE\nflavor=Someone measured the deck afterwards.\nfx.set.gunScale=2.5\nfx.set.kick=5\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\nproj.mul.SplashDamageRange=1.6\n\n"
  "[unique]\nname=Acht-Acht\nbuilt_from=8.8 cm Flak 36\nuses=8\nline=+120% damage on hits beyond 1000 m\nline=+60% shell speed\nline=+100% magazine\nline=reloads twice as fast\nflavor=Pointed at the sky. Aimed at whatever comes.\nfx.set.longBonus=2.2\nfx.set.longDist=1000\nproj.mul.MuzzleVelocity=1.6\nproj.mul.MuzzleVelocityRealistic=1.6\nmul.MagSize=2\nmul.MaxAmmo=2\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Minengeschoss\nbuilt_from=MG 151/20 Drilling\nuses=8\nline=+200% blast radius\nline=+50% damage\nline=reloads 3x as fast\nflavor=Thin walls, big bang.\nproj.mul.SplashDamageRange=3\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\nmul.reloadCooldown=0.333\n\n"
  "[unique]\nname=Bersagliere\nbuilt_from=Breda 20/65 Mod. 40\nuses=8\nline=+10% damage per kill by its gunner (up to 10 kills), lost if that gunner dies\nline=+50% fire rate\nline=reloads twice as fast\nflavor=Feathers in the cap, one for every kill.\nfx.set.vetPerKill=0.1\nfx.set.vetCap=10\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Great Dane\nbuilt_from=Madsen 20 mm\nuses=8\nline=kills give your downed crew 20 s more before they bleed out\nline=each hit steadies your crew: 10% less chance a hit on your boat throws one overboard (up to 9 hits, fading)\nline=+60% damage\nline=+40% fire rate\nflavor=Big, loyal, and it never leaves anyone behind.\nfx.set.rally=20\nfx.set.steady=0.1\nfx.set.steadyCap=9\nfx.set.steadyDecay=3\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\nmul.roundsPerMinute=1.4\nmul.shotCooldown=0.714\n\n"
  "[unique]\nname=Warsaw Concerto\nbuilt_from=Polsten 20 mm\nuses=8\nline=50% chance a shot uses no ammo\nline=+50% fire rate\nline=reloads twice as fast\nflavor=Fewer parts, more music.\nfx.set.saveAmmo=0.5\nmul.roundsPerMinute=1.5\nmul.shotCooldown=0.667\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Doppeltrommel\nbuilt_from=Twin MG 34\nuses=8\nline=+200% magazine\nline=+60% fire rate\nline=+150% shot size (easier hits)\nflavor=Two drums, two barrels, no pause.\nmul.MagSize=3\nmul.MaxAmmo=3\nmul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nproj.mul.projectileWidth=2.5\nproj.mul.projectileLength=2.5\n\n"
  "[unique]\nname=Dushka\nbuilt_from=Twin DShK\nuses=8\nline=a killing hit spills 150% of its leftover damage onto the nearest enemy\nline=+50% damage\nline=+100% magazine\nflavor=Sweetheart, they called her. Not to her face.\nfx.set.overkill=1.5\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\nmul.MagSize=2\nmul.MaxAmmo=2\n\n"
  "[unique]\nname=Spud Gun\nbuilt_from=Holman Projector\nuses=8\nline=each bomb bursts into 20 fragments\nline=50% chance a shot uses no ammo\nline=+100% damage\nflavor=Officially for Mills bombs. Mostly for potatoes.\n" UQ_FRAG "proj.set.fragmentCount=20\nfx.set.saveAmmo=0.5\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\n\n"
  "[unique]\nname=Panzerknacker\nbuilt_from=7.5 cm Pak 40\nuses=8\nline=+150% damage to Rare and Elite ships\nline=35% chance of a x3 critical hit\nline=reloads twice as fast\nflavor=Armour is only a suggestion.\nfx.set.giantSlayer=2.5\nfx.set.critChance=0.35\nfx.set.critMult=3\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Okhotnik\nbuilt_from=45 mm 21-K\nuses=8\nline=hits mark a ship for 10 s: all your guns do +50% damage to it\nline=+100% blast radius\nline=reloads twice as fast\nflavor=The small hunter finds them. The rest of you finish them.\nfx.set.mark=0.5\nfx.set.markTime=10\nproj.mul.SplashDamageRange=2\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Machibuse\nbuilt_from=Type 1 47 mm AT\nuses=8\nline=+120% damage on hits within 400 m\nline=+100% damage for its first magazine (at least 6 shots) each battle\nline=+50% damage\nflavor=Wait. Wait. Now.\nfx.set.closeBonus=2.2\nfx.set.closeDist=400\nfx.set.salvo=2\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\n\n"
  "[unique]\nname=Last Ditch\nbuilt_from=Blacker Bombard\nuses=8\nline=bombs steer onto their target\nline=kills give your downed crew 20 s more before they bleed out\nline=+100% damage\nflavor=We shall never surrender. We shall improvise.\nfx.set.seek=60\nfx.set.rally=20\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\n\n"
  "[unique]\nname=Big Bada Boom\nbuilt_from=380mm RW 61\nuses=8\nline=ship kills pay +1000 RP\nline=a killing hit spills 150% of its leftover damage onto the nearest enemy\nline=+100% damage to Rare and Elite ships\nflavor=Big bada boom.\nfx.set.killRP=1000\nfx.set.overkill=1.5\nfx.set.giantSlayer=2\n\n"
  "[unique]\nname=Stalin's Organ\nbuilt_from=16-M-8 Katyusha\nuses=8\nline=+200% damage for its first magazine (at least 6 shots) each battle\nline=+100% rockets carried\nline=+50% damage\nflavor=You hear it before you see it. Then you see nothing.\nfx.set.salvo=3\nmul.MagSize=2\nmul.MaxAmmo=2\nproj.mul.BaseDamage=1.5\nproj.mul.SplashDamageBase=1.5\n\n"
  "[unique]\nname=Raisa Sergeyevna\nbuilt_from=M-13 Katyusha\nuses=8\nline=+100% damage on hits beyond 1000 m\nline=reloads twice as fast\nline=rockets steer onto their target\nflavor=She writes from very far away.\nfx.set.longBonus=2\nfx.set.longDist=1000\nmul.reloadCooldown=0.5\nfx.set.seek=60\n\n"
  "[unique]\nname=Mighty Midget\nbuilt_from=4.5-inch Beach Barrage Rocket\nuses=8\nline=+120% damage on hits within 400 m\nline=+100% rockets carried\nline=reloads twice as fast\nflavor=Small ship. Big ripple.\nfx.set.closeBonus=2.2\nfx.set.closeDist=400\nmul.MagSize=2\nmul.MaxAmmo=2\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Ikazuchi\nbuilt_from=20 cm Naval Rocket\nuses=8\nline=rockets steer onto their target\nline=40% chance of a x3 critical hit\nline=+100% blast radius\nflavor=The thunder chooses where it falls.\nfx.set.seek=60\nfx.set.critChance=0.4\nfx.set.critMult=3\nproj.mul.SplashDamageRange=2\n\n"
  "[unique]\nname=Bocage\nbuilt_from=Hedgerow\nuses=8\nline=hits mark a ship for 10 s: all your guns do +50% damage to it\nline=+100% damage\nline=+100% blast radius\nflavor=Clear a path. The rest will follow.\nfx.set.mark=0.5\nfx.set.markTime=10\nproj.mul.BaseDamage=2\nproj.mul.SplashDamageBase=2\nproj.mul.SplashDamageRange=2\n\n"
  "[unique]\nname=Black Sand\nbuilt_from=M2 Stinger\nuses=8\nline=reloads 3x as fast\nline=+100% belt\nline=+60% damage\nline=+200% shot size (easier hits)\nflavor=Eight trips back to the beach for ammo.\nmul.reloadCooldown=0.333\nmul.MagSize=2\nmul.MaxAmmo=2\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\nproj.mul.projectileWidth=3\nproj.mul.projectileLength=3\n\n"
  "[unique]\nname=The Ping\nbuilt_from=M1 Garand\nuses=8\nline=every 8th shot is a x8 critical hit\nline=+60% fire rate\nline=reloads twice as fast\nflavor=Eight rounds, one ping, one new clip.\nfx.set.nthShot=8\nfx.set.nthMult=8\nmul.roundsPerMinute=1.6\nmul.shotCooldown=0.625\nmul.reloadCooldown=0.5\n\n"
  "[unique]\nname=Steady On\nbuilt_from=Bren Mk I\nuses=8\nline=each hit steadies your crew: 10% less chance a hit on your boat throws one overboard (up to 10 hits, fading)\nline=100-round pan magazine\nline=+60% damage\nflavor=Short bursts, lads. Steady.\nfx.set.steady=0.1\nfx.set.steadyCap=10\nfx.set.steadyDecay=3\nset.MagSize=100\nproj.mul.BaseDamage=1.6\nproj.mul.SplashDamageBase=1.6\n\n"
  "[unique]\nname=Jack-in-the-Box\nbuilt_from=PIAT\nuses=8\nline=+150% damage\nline=a killing hit spills 150% of its leftover damage onto the nearest enemy\nline=bombs steer onto their target\nflavor=Wound tight. Let go.\nproj.mul.BaseDamage=2.5\nproj.mul.SplashDamageBase=2.5\nfx.set.overkill=1.5\nfx.set.seek=60\n\n"
  "[unique]\nname=The Bonesetter\nbuilt_from=Type 89 Knee Mortar\nuses=8\nline=exploding rounds burst into 20 fragments\nline=reloads 3x as fast\nline=+60% blast radius\nflavor=Fire it off the deck. Never the thigh.\n" UQ_FRAG "proj.set.fragmentCount=20\nmul.reloadCooldown=0.333\nproj.mul.SplashDamageRange=1.6\n\n"
  // The chomper torpedoes: they home in, chomping on the way; an original look, a yellow ball with eyes (and a bow),
  // not the arcade character
  "[unique]\nname=Chomper\nbase=Mk 14 Torpedo x1,Mk 24 Torpedo x2\nuses=8\nline=rides the waves as a hungry yellow ball, chomping all the way\nline=homes in hard on the nearest big ship ahead\nline=+100% warhead damage\nline=+50% blast radius\nflavor=Always hungry. Never full.\nfx.set.chomp=1\nfx.set.torpSeek=60\nmul.damage=2\nmul.damageRange=1.5\n\n"
  "[unique]\nname=Chompette\nbase=Mk 18 Torpedo x1,Mk 24 Torpedo x2\nuses=8\nline=rides the waves as a hungry yellow ball with a bow\nline=homes in, preferring a ship no other homing torpedo is after\nline=+60% engine thrust\nline=+100% warhead damage\nflavor=Saving room for dessert.\nfx.set.chomp=2\nfx.set.torpSeek=60\nmul.thrust=1.6\nmul.terminalVelocity=1.6\nmul.damage=2\n\n"
  // Flying Fish (fully visible, very fast, skips along the surface like a stone): the torpedo runs on at its depth as
  // stock (see TorpSkimUpdate), its
  // model skips on the surface; the game's own spotting multiplier is set to the highest stock value and above
  "[unique]\nname=Flying Fish\nbase=Mk 13 Torpedo x1,Mk 24 Torpedo x2\nuses=8\nline=skips across the surface as a flying fish, never below the water\nline=+80% engine thrust and top speed\nline=+25% warhead damage\nline=impossible to miss: every ship spots it coming and can steer away\nflavor=Not built for stealth.\nfx.set.skim=1\nmul.thrust=1.8\nmul.terminalVelocity=1.8\nmul.damage=1.25\nset.detectionByEntityChanceMultiplier=10\n\n"
  // Sneaking Eel (undetectable, its look a generic commando in a crawl pose): the game's own spotting multiplier at 0,
  // so no ship ever spots it and none
  // steer away; slower, as a crawl should be
  "[unique]\nname=Sneaking Eel\nbase=Mk 14 Torpedo x1,Mk 24 Torpedo x2\nuses=8\nline=crawls along the surface like a commando behind enemy lines\nline=never spotted: no ship sees it coming, so none steer away\nline=+50% warhead damage\nline=-15% engine thrust and top speed\nflavor=Nobody saw a thing.\nfx.set.crawl=1\nmul.damage=1.5\nmul.thrust=0.85\nmul.terminalVelocity=0.85\nset.detectionByEntityChanceMultiplier=0\n";
// Built-in affixes taken out of the random pool. A drops.ini written by an older version still has them; one whose text
// is unchanged from the old default is switched off at load (weight 0), one the player edited is left alone.
// Endless: 'infinite ammo, -75% damage' reads as a bad trade.
static const char* kRetiredAffixes[][2] = { { "Endless", "infinite ammo, but -75% damage" } };
// Lines of built-in affixes that changed after a release. A line still exactly as an older version wrote it is replaced
// (an empty newLine removes it); oldLine null adds newLine under name= when the affix has no line with that key yet.
// Lines the player edited stay.
// 2026-10-26: rockets, mortars and crew weapons joined the drops.
struct AffixLineUpdate { const char* name; const char* oldLine; const char* newLine; int since = 0; };   // since: migration number, see MigrateDrops
static const AffixLineUpdate kAffixLineUpdates[] = {
  { "Flying Fish", "line=skips across the surface like a thrown stone, never below the water", "line=skips across the surface as a flying fish, never below the water" },   // 2026-11-05: its fish look
  { "Automated", "not_families=Utility Weapon", "not_families=Rockets,Crew Weapons" },
  // 2026-11-15: crew guns have endless ammo, so killAmmo does nothing on these two
  { "Old Reliable", "line=kills refill all of its ammo", "line=+100% magazine" },
  { "Old Reliable", "fx.set.killAmmo=1", "mul.MagSize=2" },
  { "Marching Home", "line=kills refill all of its ammo", "line=kills give your downed crew 20 s more before they bleed out" },
  { "Marching Home", "fx.set.killAmmo=1", "fx.set.rally=20" },
  { "Automated", "not_families=Utility Weapon,Rockets,Crew Weapons", "not_families=Rockets,Crew Weapons" },   // 2026-10-28: utility guns fire on their own
  // Automated utility guns would be 1 drop in about 110 on a PT boat; x4 there makes it about 1 in 30
  { "Automated", nullptr, "family_weight=Utility Weapon:4", 1 },
  // the 2026-10-29 buff: bigger good rolls, milder drawbacks, the special effects far more often
  { "Rapid", "roll=1.15-1.5", "roll=1.25-1.75", 1 },
  { "Heavy", "roll=1.2-1.7", "roll=1.3-2.0", 1 },
  { "Blast", "roll=1.25-2.0", "roll=1.5-2.5", 1 },
  { "Deep", "roll=1.25-2.0", "roll=1.5-2.5", 1 },
  { "Quick", "roll=1.3-2.0", "roll=1.5-2.5", 1 },
  { "Fast", "roll=1.15-1.5", "roll=1.25-1.75", 1 },
  { "Jammy", "roll=0.6-0.85", "roll=0.75-0.9", 1 },
  { "Weak", "roll=0.6-0.85", "roll=0.75-0.9", 1 },
  { "Slow", "roll=1.3-1.8", "roll=1.15-1.4", 1 },
  { "Tiny", "roll=0.5-0.8", "roll=0.65-0.85", 1 },
  { "Short-handed", "weight=0.6", "weight=0.8", 1 },
  { "Deadeye", "weight=0.6", "weight=1", 1 },
  { "Deadeye", "roll=0.08-0.16", "roll=0.12-0.25", 1 },
  { "Salvager", "weight=0.6", "weight=1", 1 },
  { "Salvager", "roll=0.15-0.40", "roll=0.30-0.60", 1 },
  { "Vampiric", "weight=0.6", "weight=1", 1 },
  { "Vampiric", "roll=0.04-0.12", "roll=0.08-0.20", 1 },
  { "Bounty", "weight=0.6", "weight=1", 1 },
  { "Bounty", "roll=50-200", "roll=100-300", 1 },
  { "Flak", "weight=0.5", "weight=1", 1 },
  { "Flak", "roll=1.3-1.8", "roll=1.5-2.2", 1 },
  { "Skipping", "weight=0.5", "weight=0.8", 1 },
  { "Shrapnel", "weight=0.5", "weight=1", 1 },
  { "Shrapnel", "roll=6-12", "roll=8-16", 1 },
  { "Homing", "weight=0.5", "weight=0.8", 1 },
  { "Homing", "roll=0.40-0.55", "roll=0.45-0.60", 1 },
  { "Chaining", "weight=0.25", "weight=0.8", 1 },
  { "Chaining", "roll=0.20-0.35", "roll=0.30-0.50", 1 },
  { "Automated", "weight=0.5", "weight=0.6", 1 },
  { "Guided", "weight=0.6", "weight=1", 1 },
  { "Flak", "bases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco", "bases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco,Mortar,Howitzer", 1 },
  { "Shrapnel", "bases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco", "bases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco,Mortar,Howitzer," ROCKET_BASES },
  { "Homing", nullptr, "not_bases=Mortar,Howitzer," ROCKET_BASES },
  // fire rate and reload effects do nothing on rocket launchers: their rate is 0 and they reload at most once before
  // resupply (Mousetrap never)
  { "Rapid", nullptr, "not_families=Rockets", 1 },
  { "Jammy", nullptr, "not_families=Rockets", 1 },
  { "Quick", nullptr, "not_families=Rockets", 1 },
  { "Slow", nullptr, "not_families=Rockets", 1 },
  // at most -60% shell speed: GunMount.maxRange is the ballistic v^2/g, so a Homing 3-inch (400 m/s) at -88% has
  // about 230 m and aims but never fires
  { "Homing", "roll=0.10-0.15", "roll=0.40-0.55", 1 },
  // Slow shells keep their range: ProjectileType.timeScale slows a shell's whole flight (Projectile.iterate moves it and
  // applies gravity by dt x timeScale), so it flies the stock arc, only slower, and
  // GunMount.maxRange and the aim (both from MuzzleVelocity) stay the stock gun's. FrameLifeTime x 1/r makes up for
  // Projectile.alive counting real time. Homing steers, so the later arrival does not spoil its aim.
  { "Homing", "proj.mul.MuzzleVelocity=$r", "proj.mul.timeScale=$r", 1 },
  { "Homing", "proj.mul.MuzzleVelocityRealistic=$r", "", 1 },
  { "The Ferryman", "proj.mul.MuzzleVelocity=0.12", "proj.mul.timeScale=0.12", 1 },
  { "The Ferryman", "proj.mul.MuzzleVelocityRealistic=0.12", "", 1 },
  { "Green Tracers", nullptr, "not_bases=" ROCKET_BASES }, { "Purple Tracers", nullptr, "not_bases=" ROCKET_BASES },
  { "Cyan Tracers", nullptr, "not_bases=" ROCKET_BASES }, { "Orange Tracers", nullptr, "not_bases=" ROCKET_BASES },
  { "Pink Tracers", nullptr, "not_bases=" ROCKET_BASES }, { "White Tracers", nullptr, "not_bases=" ROCKET_BASES },
  { "Long Tracers", nullptr, "not_bases=" ROCKET_BASES },
  // Chaining: the first leap 50-75%, each further leap 30% less likely
  { "Chaining", "roll=0.30-0.50", "roll=0.50-0.75", 2 },
  { "Chaining", "text={val}% chance a hit leaps to another ship, and again at less each time (50% damage per leap)", "text={val}% chance a hit leaps to another ship, 30% less likely with each further leap (50% damage per leap)", 2 },
  // rocket launchers do reload between resupplies: Quick and Slow roll on
  // them again. Rapid and Jammy stay off: every launcher's roundsPerMinute and shotCooldown are 0 in the dump.
  { "Quick", "# rocket launchers reload at most once before they need resupply", "", 4 },
  { "Quick", "not_families=Rockets", "", 4 },
  { "Slow", "# rocket launchers reload at most once before they need resupply", "", 4 },
  { "Slow", "not_families=Rockets", "", 4 },
  // 2026-11-04 wording audit: lines still exactly as the mod wrote them get the clearer text (safe every launch)
  { "Vampiric", "text=kills repair {val}% of a damaged hull part", "text=kills repair {val}% of your most damaged hull part" },
  { "Salvager", "text=kills refill {val}% ammo", "text=kills refill {val}% of its ammo" },
  { "Proximity Fuse", "text=goes off {pct}% farther from a hull", "text=goes off {abs}% farther from a hull" },
  { "Seeking", "text=steers onto the nearest ship ahead ({int} degrees a second)", "text=steers onto the nearest big ship ahead ({int} degrees a second)" },
  { "Dolphin", "text=leaps out of the water like a dolphin every {int} seconds, +20% engine thrust", "text=leaps out of the water like a dolphin about every {int} seconds, +20% engine thrust" },
  // 2026-11-06: accuracy drawbacks scale the spread (100 - Accuracy), not the score; Wild costs nothing on seeking
  // shells or on mortars and howitzers (no spread); Opening Salvo has a 6-shot floor; Brawling is a flat bonus on crew
  // weapons (they fire within 270 m); Drum reaches the Meat Chopper and the 37 mm autocannon, not the Boys
  { "Automated", "text=fires on its own with no gunner, -20% accuracy", "text=fires on its own with no gunner, 25% wider spread", 18 },
  { "Automated", "mul.Accuracy=0.8", "mul.Inaccuracy=1.25", 18 },
  { "Wild", "text={pct}% damage, {abs}% less accurate", "text={pct}% damage, {spread}% wider spread", 18 },
  { "Wild", "mul.Accuracy=2-$r", "mul.Inaccuracy=2.5*$r-1.5", 18 },
  { "Wild", nullptr, "excludes=Homing,Guided", 18 },
  { "Wild", nullptr, "not_bases=Mortar,Howitzer", 18 },
  { "Opening Salvo", "text={pct}% damage for its first magazine each battle", "text={pct}% damage for its first magazine (at least 6 shots) each battle", 18 },
  { "Close Quarters", nullptr, "not_families=Crew Weapons", 18 },
  { "Drum", "bases=Bofors,Oerlikon,Elco,Browning,BAR,M1919,Lewis", "bases=Bofors,Oerlikon,Elco,Chopper,37 mm Autocannon,Browning,BAR,M1919,Lewis", 18 },
  { "Drum", nullptr, "not_built_from=Boys", 18 },
  // 2026-11-16 balance pass: mortar shells land within a frame of appearing (sky shots), so speed, steering and
  // skipping do nothing on them; crits scale only direct damage (the howitzer's is 0); 20 mm shrapnel swamped the gun
  { "Fast", nullptr, "not_bases=Mortar", 24 }, { "Bold", nullptr, "not_bases=Mortar", 24 }, { "Skipping", nullptr, "not_bases=Mortar", 24 },
  { "Deadeye", nullptr, "not_bases=Howitzer", 24 },
  { "Guided", "bases=Mortar,Howitzer," ROCKET_BASES, "bases=Howitzer," ROCKET_BASES, 24 },
  { "Shrapnel", "bases=Bofors,37 mm,3-inch,5-inch,Oerlikon,Elco,Mortar,Howitzer," ROCKET_BASES, "bases=Bofors,37 mm,3-inch,5-inch,Mortar,Howitzer," ROCKET_BASES, 24 },
  { "Rainmaker", "line=rounds steer onto their target", "line=reloads 30% faster", 24 }, { "Rainmaker", "fx.set.seek=60", "mul.reloadCooldown=0.7", 24 },
  { "Rainmaker", "fx.set.nthMult=4", "fx.set.nthMult=10", 24 },   // x10 on the 800 direct of its 800/1600 shell: about x4 on the whole
  { "The Mole", "line=rounds steer onto their target", "", 24 }, { "The Mole", "fx.set.seek=60", "", 24 },
  { "The Mole", "line=reloads twice as fast", "line=reloads 3x as fast", 24 }, { "The Mole", "mul.reloadCooldown=0.5", "mul.reloadCooldown=0.333", 24 },
  { "The Bonesetter", "line=rounds steer onto their target", "", 24 }, { "The Bonesetter", "fx.set.seek=60", "", 24 },
  { "The Bonesetter", "line=reloads twice as fast", "line=reloads 3x as fast", 24 }, { "The Bonesetter", "mul.reloadCooldown=0.5", "mul.reloadCooldown=0.333", 24 },
};

// [unique] blocks whose built-in text changed: one still exactly as an older version wrote it is replaced whole by the
// current default block of that name (an edited one stays). since = migration number, as above.
struct UniqueBlockUpdate { const char* name; const char* oldBlock; int since; };
static const UniqueBlockUpdate kUniqueBlockUpdates[] = {
  // 2026-11-02 rebalance: all four had fallen behind the 2026-10-29 affix buff
  { "Hornet's Nest", "[unique]\nname=Hornet's Nest\nbase=M2 Browning\nuses=6\nline=shots seek their target\nline=half of all hits leap to another ship, and on (35% damage per leap)\nflavor=They never stop coming.\nfx.set.seek=180\nfx.set.chain=4\nfx.set.chainChance=0.5\nfx.set.chainDamage=0.35\nfx.set.chainDecay=0.6\n", 7 },
  { "Lazarus", "[unique]\nname=Lazarus\nbase=M3 37 mm AT\nuses=3\nline=kills refill all of its ammo\nline=kills repair 30% of your most damaged hull part\nline=but only lasts 3 missions\nflavor=Every kill buys you another minute.\nfx.set.killAmmo=1\nfx.set.killRepair=0.3\n", 7 },
  { "The Ferryman", "[unique]\nname=The Ferryman\nbase=40 mm Bofors\nuses=6\nline=-88% shell speed, shells home in and keep their range\nline=+150% damage\nflavor=He is slow. He always arrives.\nproj.mul.timeScale=0.12\nproj.mul.FrameLifeTime=8.33\nfx.set.seek=90\nproj.mul.BaseDamage=2.5\nproj.mul.SplashDamageBase=2.5\n", 7 },
  { "The Metronome", "[unique]\nname=The Metronome\nbase=20 mm Oerlikon Twin\nuses=6\nline=every 5th shot is a x3 critical hit\nline=+20% fire rate\nflavor=Tick, tick, tick, BOOM.\nfx.set.nthShot=5\nfx.set.nthMult=3\nmul.roundsPerMinute=1.2\nmul.shotCooldown=0.833\n", 7 },
  // 2026-11-04: the chompers also drop as the sub chaser's Mk 24 pair
  { "Chomper", "[unique]\nname=Chomper\nbase=Mk 14 Torpedo x1\nuses=6\nline=rides the waves as a hungry yellow ball, chomping all the way\nline=homes in hard on the nearest ship ahead\nline=+100% warhead damage\nline=+50% blast radius\nflavor=Always hungry. Never full.\nfx.set.chomp=1\nfx.set.torpSeek=60\nmul.damage=2\nmul.damageRange=1.5\n", 9 },
  { "Chompette", "[unique]\nname=Chompette\nbase=Mk 18 Torpedo x1\nuses=6\nline=rides the waves as a hungry yellow ball with a bow\nline=homes in on a ship no other chomper is after\nline=+60% engine thrust\nline=+100% warhead damage\nflavor=Saving room for dessert.\nfx.set.chomp=2\nfx.set.torpSeek=60\nmul.thrust=1.6\nmul.terminalVelocity=1.6\nmul.damage=2\n", 9 },
  // 2026-11-04 wording audit: the chompers only home on ships a torpedo can hit
  { "Chomper", "[unique]\nname=Chomper\nbase=Mk 14 Torpedo x1,Mk 24 Torpedo x2\nuses=6\nline=rides the waves as a hungry yellow ball, chomping all the way\nline=homes in hard on the nearest ship ahead\nline=+100% warhead damage\nline=+50% blast radius\nflavor=Always hungry. Never full.\nfx.set.chomp=1\nfx.set.torpSeek=60\nmul.damage=2\nmul.damageRange=1.5\n", 13 },
  { "Chompette", "[unique]\nname=Chompette\nbase=Mk 18 Torpedo x1,Mk 24 Torpedo x2\nuses=6\nline=rides the waves as a hungry yellow ball with a bow\nline=homes in on a ship no other chomper is after\nline=+60% engine thrust\nline=+100% warhead damage\nflavor=Saving room for dessert.\nfx.set.chomp=2\nfx.set.torpSeek=60\nmul.thrust=1.6\nmul.terminalVelocity=1.6\nmul.damage=2\n", 13 },
};

