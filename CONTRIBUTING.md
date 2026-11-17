# Contributing

Thanks for wanting to help. Bug fixes, new guns, effects, models and translations are all welcome. For anything
bigger than a small fix, open an issue first so we can talk it over. If you fork the mod or build on it, please credit
David Nendza with a link back to this repository.

How the code is laid out is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Build and test

- Build: run `build.bat` (needs the Visual Studio 2019 or 2022 C++ build tools, x64). It writes `out\version.dll`.
- Offline test of the crew report text: `tools\build_report_test.bat` must end with `ALL CHECKS PASSED`.
- In game: copy `out\version.dll` next to `Boat Crew.exe` (with the game closed), play at least one battle, and check
  `BoatCrewMod\weaponmod.log`. Paste the lines that show your change working in the pull request. `debug=1` in
  `BoatCrewMod\drops.ini` gives a much more detailed log.
- Say what you tested and what you did not (co-op, a fresh save, an old save).

## Rules that protect players' saves

Breaking one of these can make an existing save load the wrong gun or lose a loadout.

- **The gun tables are append only.** A save stores the position of the chosen gun, not its name. Add new entries to
  `kTurretGuns`, `kSideGrades`, `kUtilityGear` and `kPaint` at the end. To take one away, set `retired=1`.
- **Changed defaults need a migration.** New `[template]`, `[affix]` and `[unique]` blocks are added to existing
  `drops.ini` files on their own. A changed line in an `[affix]` or `[unique]` block needs a row in `kAffixLineUpdates`
  (`wm_drops_data.h`). A new or changed `[drops]` key needs a numbered `done < N` block in `MigrateDrops` and a higher
  `kMigrations` (`wm_drops.h`). Never renumber old migrations.
- **Co-op stays compatible.** Every player must run the same version. Don't change the message format in
  `wm_coop.h` (the `BCWM|` header with the version, the channel id) without bumping `kModVersion`.

## Code habits

- **Find game things by name.** Use `FindClass`, `FindMethod` and `FieldOffset(class, "field")`, which returns -1 when
  the field is missing. Never hard-code an offset you could look up. Read IL2CPP arrays with `ArrLen` / `ArrAt` and
  lists with `ListCount` / `ListAt`.
- **Fail safe.** If a lookup fails, log a `MISSING PIECES` line once and switch the feature off. Code that runs inside
  a game hook goes inside the existing `__try` guards (`FaultFilter`), so a game update turns a feature off instead of
  crashing the game. A function with a `__try` block must not hold C++ objects that need unwinding.
- **Hold game objects safely.** A Unity object you keep between frames is pinned with `Pin` and released with
  `Unpin`. Check `Alive()` before using one. Clear per-battle state when the battle changes, the way the features
  next to yours do.
- **Log every new effect.** Players can't see most effects in the chaos of a battle, so each one writes
  `weaponmod.log` lines: the first few events in full with `Log`, the rest with `DLog` (debug only), and one summary
  line per battle.
- **Keep it small.** Reuse the helpers that exist before adding new ones. No new dependencies.

## Comments and naming

- A comment says why, not what: an offset and the class field it belongs to, why a hook exists, a game quirk, an order
  that matters.
- Comments describe the code as it is now. No dates, names, review numbers or "was X, now Y". That history belongs in
  the commit message.
- One line for one fact. Keep lines under about 160 characters.
- A known limit gets a `shortcut:` comment that says the limit and when to fix it.
- Prefixes: `g_` for globals, `k` for constants, `g_off<Class><Field>` for field offsets. No one-letter names outside
  small loops.

## Player text

Player-facing text (descriptions, Mod Options rows, What's New, report wording) is plain, short English. Changes to
the crew report wording should pass `tools\build_report_test.bat`.

## The secret tier

The mod has one gun tier that is deliberately left out of the README and the changelog. Keep it that way: don't
list it in documentation or release notes.
