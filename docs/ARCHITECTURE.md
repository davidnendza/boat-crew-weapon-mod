# How the code is laid out

The mod is one C++ file, `src/weaponmod.cpp`, built into `version.dll`. Unity's player loads `version.dll` from the
game folder, so the game loads the mod's copy. It forwards the real Windows exports and starts one thread. From then
on everything is found by name through the IL2CPP C API (`il2cpp_class_from_name`, `il2cpp_class_get_field_from_name`
and so on), so a game update usually only breaks what was renamed.

`weaponmod.cpp` itself holds only the start and the end: the includes, the export forwards, the log and `kModVersion`
at the top; the `WeaponSystem.Awake` hook, the hook installer, the load watch (debug=1), `InitThread` and `DllMain` at the bottom. Everything in
between is `#include`d from `src/wm_*.h` in a fixed order. It is still one translation unit (a "unity build"), so the
fragments share the file's `static` functions and globals and must stay in that order. They are not standalone
headers. `tools/split_check.py` puts them back together to compare with an older single-file version.

How to contribute, and the rules for saves, migrations and comments, are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## How it runs

- **Start.** `InitThread` reads the ini files in `BoatCrewMod\` (`weapons.ini`, `inventory.ini`, `drops.ini`,
  `enemies.ini`, `positions.ini`), then waits for the IL2CPP domain, resolves the API and installs the hooks. The save
  and load hooks come later (`TryInstallPerSave`), normally from a game thread.
- **Guns in the loadout.** `Hooked_Awake` runs after `WeaponSystem.Awake`. It appends the mod's guns to the slot's
  mounts list in a fixed order and adds matching entries to a private copy of the slot's `SelectionOrder`. A save
  stores the position of the chosen gun, which is why the gun tables are append only.
- **Every frame.** Two hooks on the game's `ProjectileManager` drive most battle features:
  - `Hooked_PMUpdate` (`wm_home_run.h`): the shot scan `CritScan` (crits, uses, shot counts), seeking shells
    (`SeekScan`), barrel smoke and the gun effects tick (`SafeGfxTick`), which also polls the crew battle log and
    talk. `CritScan` notices battle changes and runs the per-battle summaries and resets.
  - `Hooked_PMLateUpdate` (`wm_frame_hooks.h`): the dispatcher for everything else that ticks: tracer colors,
    counting missions, chaining, Rare and Elite enemies, automated utility guns, All-Knowing, torpedoes and their
    looks, utility gear, crew weapon looks, streaks and the newsreel.
- **Windows.** The mod draws its own windows with Unity's IMGUI from `OnGUI` hooks on a few game objects
  (`wm_gui_host.h`): the inventory, Mod Options, crates, the after-action report, the crew files, letters and mail.
  Elite name tags, the crew marker and the co-op tick run from there too.
- **Hits and kills.** Detours on `HealthPart.hitFlash` and `CrewMember.hitFlash` see every hit; hooks on the game's
  `creditKill` methods see every kill (`wm_gun_effects.h`). Gun effects and the crew log are built on these.
- **Saves.** Hooks on the game's save and load (`wm_per_save.h`) keep a separate inventory and crew files per save.
- **Co-op.** `wm_coop.h` sends the mod's state between players over a Steam side channel.

## The files

In include order.

| File | What is in it |
|---|---|
| `wm_il2cpp.h` | The IL2CPP C API, `FindClass`, `FindMethod`, `FieldOffset`, `ArrLen`/`ArrAt`, `ListCount`/`ListAt`, `Pin`/`Unpin`, `Invoke`, `Str` |
| `wm_items.h` | The item model (`Item`, `Op`, `ItemFx`), the `weapons.ini` parser, text colors, the mod's turret guns (`kTurretGuns`), side-grades (`kSideGrades`) and utility gear, `LoadItems` |
| `wm_unity.h` | Unity classes and methods resolved once (`Setup`), object cloning |
| `wm_gun_models.h` | Custom gun models: `.obj` meshes on found guns |
| `wm_gun_paint.h` | Paint schemes on found guns (`kPaint`) |
| `wm_inject.h` | Building a gun's mount (`AddCloneMount`), the slot's order entries, the slot catalogue |
| `wm_positions.h` | Stable positions (`positions.ini`), so saved positions keep pointing at the same gun |
| `wm_drops_data.h` | The default `drops.ini` text (`kDefaultDrops`): loot templates, effects (affixes), Uniques, `kAffixLineUpdates` |
| `wm_drops.h` | `MigrateDrops` (updating old `drops.ini` files), `LoadDrops`, rolling a drop, saving the inventory |
| `wm_grid_pickups.h` | The inventory grid limit, adding guns to boats that already exist, the RP crate pickup watcher |
| `wm_loadout_screen.h` | The loadout screen: seat icons, `WeaponSystem.selectIndex`, emptying worn-out guns; `FaultFilter`, `PatchVtableEntry` |
| `wm_inventory_window.h` | Uses per gun and the inventory window |
| `wm_per_save.h` | Per-save inventory files, the slow-load watch, the save and load hooks |
| `wm_ui_kit.h` | The look shared by the mod's windows (paper, ink, stamps) |
| `wm_crates.h` | Crates, materials and the crate opening reel |
| `wm_windows.h` | Mod Options (`ModOpts`), What's New and the changelog, the after-action report window, the title screen tag |
| `wm_crew_files.h` | Crew files, the lost, letters home and mail call, and their windows |
| `wm_gui_host.h` | The `OnGUI` hosts that draw all the windows |
| `wm_tracers.h` | Tracer colors and rarity tracers |
| `wm_auto_utility.h` | Automated guns in the utility slot, including mortar sky shots |
| `wm_crits_smoke.h` | Critical hits, smoking barrels and `CritScan`, the per-frame shot scan |
| `wm_seek_chain.h` | Seeking (homing) shells and chaining shots |
| `wm_elites.h` | Rare and Elite enemies (`kDefaultEnemies`), their traits and name tags |
| `wm_crew_looks.h` | Crew weapon tracers, the aim marker and the aim line |
| `wm_female_crew.h` | Female crew (looks, names, gender stored in the save) and the crew id |
| `wm_unusual.h`, `wm_helpers.h`, `wm_home_run.h` | The secret gun tier and the friendly units guns call in. `wm_home_run.h` also holds `Hooked_PMUpdate` |
| `wm_gun_effects.h` | On-kill effects, the hit and kill hooks, crates per round |
| `wm_fire_balloon.h` | More of the secret gun tier |
| `wm_crew_log.h` | The crew battle log, speech bubbles and the talking crew |
| `wm_crew_hits.h` | Where each crew hit landed and who fired it |
| `wm_prose_report.h` | The written after-action report and recording a battle into the crew files |
| `wm_gun_effects_shots.h` | Gun effects per shot, per kill, per tick and at a battle change |
| `wm_all_knowing.h` | All-Knowing: the crew fires at unmarked enemies while you aim a hand-aimed gun |
| `wm_kill_cam.h` | The torpedo kill cam, its hit prediction and the crew guard |
| `wm_torpedoes.h` | Found torpedoes: launchers, tiers, torpedoes in the water |
| `wm_utility_gear.h` | Utility gear on the water: smoke pots and life rafts |
| `wm_torp_looks.h` | Torpedo looks and sounds (Chomper and others), tube looks, `TorpInstall` |
| `wm_frame_hooks.h` | `Hooked_PMLateUpdate`, the per-frame dispatcher |
| `wm_coop.h` | Co-op: the channel, other players' guns, Elites, called-in units, the session |

Other headers. The `crew_*.h` files are included at the top of `weaponmod.cpp`, `newsreel.h` just before
`wm_frame_hooks.h`, the rest by those:

| File | What is in it |
|---|---|
| `crew_report.h` | Turns a battle into the after-action report text. Pure C++, also built offline by `tools/crew_report_test.cpp` |
| `crew_talk.h`, `talk_lines.h` | The crew's battle chatter and its lines |
| `crew_life.h`, `crew_mail.h`, `mail_threads.h` | Crew members' pasts, the memorial, letters home, mail call and its letter threads |
| `crew_bonds.h`, `crew_nick.h` | Closest mates and earned nicknames |
| `newsreel.h`, `gif_writer.h`, `newsreel_tv.h` | The battle newsreel: filming, the GIF writer and the old-TV page |

## Where to start for common changes

- **A new turret gun or side-grade:** add an `[item]` block at the end of `kTurretGuns` or `kSideGrades`
  (`wm_items.h`). Models go in `models/`, generated by the scripts in `tools/`.
- **A new effect (affix):** add an `[affix]` block in `kDefaultDrops` (`wm_drops_data.h`) with an `fx.set.<key>`.
  Read it where the effect happens with `ItemFx(item, "<key>")`. Most per-shot and kill effects live in
  `wm_gun_effects_shots.h` (`GfxShot`, `GfxKill`); crits are in `wm_crits_smoke.h`, the simple on-kill effects
  (ammo, repair, RP) in `wm_gun_effects.h`.
- **A new Unique:** add a `[unique]` block in `kDefaultDrops`. It reaches existing `drops.ini` files on its own.
- **A changed default value:** see the migration rules in [CONTRIBUTING.md](../CONTRIBUTING.md).
- **A new Mod Options row:** add it to `ModOpts()` in `wm_windows.h`, next to rows of the same kind. The row names its
  file and key and points at the global the key is parsed into (the `[drops]` keys are parsed in `wm_drops.h`).
- **A new window:** draw it from the `OnGUI` hosts in `wm_gui_host.h`, with the look from `wm_ui_kit.h`.
- **Something that ticks every frame in battle:** call it from `Hooked_PMLateUpdate` (`wm_frame_hooks.h`) inside
  its own `__try` guard. Reset its state at the battle change, from `CritScan`'s battle change (`wm_crits_smoke.h`)
  or by watching `EngagementInstance()` in the tick, as the crew weapon looks do.
