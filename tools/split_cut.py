"""Cuts src/weaponmod.cpp into src/wm_<feature>.h fragments (a unity build: weaponmod.cpp includes them in order).

Run once from the repo root:  python tools/split_cut.py
Each fragment starts at a line found by its text (the '// ----' section markers, a few function lines), so edits
above a cut do not move it. Every cut must sit at brace depth 0 and #if depth 0, or nothing is written.
Each fragment gets one added first line (a comment saying what it holds); tools/split_check.py drops that line,
expands the includes again and compares the result byte for byte with the pre-split weaponmod.cpp.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'weaponmod.cpp')

# (fragment name or None = stays in weaponmod.cpp, start line prefix, what it holds). Order = file order.
CUTS = [
    ('wm_il2cpp',             '// ---- IL2CPP C API',                       'IL2CPP C API: exports, class/field/method lookups, string and array helpers'),
    ('wm_items',              '// ---- items from weapons.ini',             'items from weapons.ini, rich-text colors, the mod\'s turret guns, side-grades and utility gear, LoadItems'),
    ('wm_unity',              '// ---- resolved once',                      'Unity lookups resolved once (Setup) and object cloning'),
    ('wm_gun_models',         '// ---- custom gun models',                  'custom gun models (OBJ meshes on found guns)'),
    ('wm_gun_paint',          '// ---- paint schemes on found guns',        'paint schemes on found guns'),
    ('wm_inject',             '// Copy one mount (and its GunType)',        'mount building: AddCloneMount, order entries, the slot catalogue'),
    ('wm_positions',          '// ---- stable positions',                   'stable positions (positions.ini) and the one-time render-style dump'),
    ('wm_drops_data',         '// ---- random drops',                       'random drops: loot templates, affixes, kDefaultDrops, uniques'),
    ('wm_drops',              'static void ClampCrateSettings() {',         'drops.ini: MigrateDrops, LoadDrops, the roll engine, inventory save'),
    ('wm_grid_pickups',       '// ---- inventory grid (Diablo 2 style)',    'inventory grid limit, boats that already exist, the RP crate pickup watcher'),
    ('wm_loadout_screen',     '// ---- diagnostics: what the loadout screen', 'loadout screen: selection diagnostics, seat icons, WeaponSystem.selectIndex'),
    ('wm_inventory_window',   '// ---- inventory cap and uses per gun',     'inventory cap, uses per gun, the inventory window (IMGUI grid)'),
    ('wm_per_save',           '// ---- per-save inventory',                 'per-save inventory sidecars, the stall watch, save/load hooks'),
    ('wm_ui_kit',             '// ---- the window\'s look: "Armory stencil"', 'window look ("Armory stencil") and the Field Manual kit (paper, ink, stamps, PNGs)'),
    ('wm_crates',             '// ---- the field crate look for the inventory', 'field crate look, the workshop strip and the crate opening reel'),
    ('wm_windows',            '// ---- mod options, the version and What\'s New', 'Mod Options, What\'s New, the after-action report window, MODDED on the title screen'),
    ('wm_crew_files',         '// ---- crew files ----',                    'crew files, crew lives, the crew files / mail call / letter windows'),
    ('wm_gui_host',           'static void OnGuiTick(int i) {',             'OnGUI hosts: OnGuiTick, GUI_HOOK, InstallInventoryGui'),
    ('wm_tracers',            '// ---- tracer color:',                      'tracer color and rarity tracers'),
    ('wm_auto_utility',       '// ---- Automated guns in the utility slot', 'Automated guns in the utility slot'),
    ('wm_crits_smoke',        '// ---- critical hits (\'Deadeye\')',        'critical hits (Deadeye), smoking barrels, CritScan (the per-frame shot scan)'),
    ('wm_seek_chain',         '// ---- seeking shots',                      'seeking shots and chaining shots'),
    ('wm_elites',             '// ---- Rare and Elite enemies ----',        'Rare and Elite enemies'),
    ('wm_crew_looks',         '// ---- crew weapon looks',                  'crew weapon looks'),
    ('wm_female_crew',        '// ---- female crew ----',                   'female crew (looks, stored gender, gear flash, names) and crew id (MOD_CREWID)'),
    ('wm_unusual',            '// ---- Unusual guns',                       'Unusual guns and heal/chain streaks'),
    ('wm_helpers',            '// ---- Big Brother',                        'Big Brother, helpers\' looks, Kamikaze Gull steering'),
    ('wm_home_run',           '// ---- Home Run:',                          'Home Run, UnusualTick and Hooked_PMUpdate'),
    ('wm_gun_effects',        '// ---- on-kill effects',                    'on-kill effects (creditKill hooks), gun effects, crates per round'),
    ('wm_fire_balloon',       '// ---- Fire Mission:',                      'Fire Mission and Up, Up and Away (balloon)'),
    ('wm_crew_log',           '// ---- crew battle log',                    'crew battle log, speech bubbles, the talking crew'),
    ('wm_crew_hits',          '// ---- where a hit landed and who fired it', 'crew hits: where a hit landed and who fired it'),
    ('wm_prose_report',       '// ---- the prose report',                   'the prose after-action report, crew files record, battle summary'),
    ('wm_gun_effects_shots',  '// Each own new shot (CritScan',             'gun effects per shot, kill, tick and battle change (GfxShot to GfxBattleChange)'),
    ('wm_all_knowing',        '// ---- All-Knowing',                        'All-Knowing'),
    ('wm_kill_cam',           '// ---- torpedo kill cam',                   'torpedo kill cam, hit prediction, the crew guard'),
    ('wm_torpedoes',          '// ---- found torpedoes ----',               'found torpedoes: launchers, tiers, torpedoes in the water'),
    ('wm_utility_gear',       '// ---- utility gear on the water',          'utility gear on the water: smoke pots, life rafts, Dave'),
    ('wm_torp_looks',         '// ---- the chomper look and sound',         'torpedo looks: chomper + FMOD sound, the Dolphin leap, tube looks, TorpInstall'),
    (None,                    '#include "newsreel.h"',                      None),
    ('wm_frame_hooks',        'static void (*g_pmLateUpdateOriginal)',      'Hooked_PMLateUpdate (the per-frame dispatcher) and ItemElementUI.Awake'),
    ('wm_coop',               '// ---- co-op ----',                         'co-op: core, another player\'s guns, elites, helpers, the session'),
    (None,                    '// ---- hook: WeaponSystem.Awake',           None),
]


def line_states(text):
    """(brace depth, #if depth) at the start of each line, 0-based. Skips comments, strings and raw strings."""
    n = len(text); i = 0; depth = 0; pp = 0; atbol = True
    starts = [(0, 0)]
    while i < n:
        c = text[i]
        if atbol and c == '#':
            m = re.match(r'#\s*(\w+)', text[i:i + 40]); d = m.group(1) if m else ''
            if d in ('if', 'ifdef', 'ifndef'): pp += 1
            elif d == 'endif': pp -= 1
            k = i
            while k < n and not (text[k] == '\n' and text[k - 1] != '\\'): k += 1
            starts.extend([(depth, pp)] * text.count('\n', i, k)); i = k; atbol = False; continue
        if text.startswith('//', i):
            k = text.find('\n', i); i = n if k < 0 else k; atbol = False; continue
        if text.startswith('/*', i):
            k = text.find('*/', i + 2); k = n if k < 0 else k + 2
            starts.extend([(depth, pp)] * text.count('\n', i, k)); i = k; atbol = False; continue
        if c == 'R' and text.startswith('R"', i) and not (text[i - 1].isalnum() or text[i - 1] == '_'):
            m = re.match(r'R"([^(\s]{0,16})\(', text[i:i + 20])
            if m:
                end = ')' + m.group(1) + '"'; k = text.find(end, i + len(m.group(0))) + len(end)
                starts.extend([(depth, pp)] * text.count('\n', i, k)); i = k; atbol = False; continue
        if c in '"\'':
            k = i + 1
            while k < n and text[k] != c and text[k] != '\n':
                k += 2 if text[k] == '\\' else 1
            i = k if k < n and text[k] == '\n' else k + 1; atbol = False; continue
        if c == '{': depth += 1
        elif c == '}': depth -= 1
        if c == '\n':
            starts.append((depth, pp)); atbol = True
        elif c not in ' \t\r':
            atbol = False
        i += 1
    return starts, (depth, pp)


def main():
    raw = open(SRC, 'rb').read()
    if b'#include "wm_' in raw: sys.exit('weaponmod.cpp is already split')
    lines = raw.splitlines(keepends=True)
    text = raw.decode('latin-1').replace('\r\n', '\n')      # so a '\' line continuation sits right before '\n'
    starts, final = line_states(text)
    if len(starts) != text.count('\n') + 1: sys.exit('line count mismatch in the depth scan')
    if final != (0, 0): sys.exit('file does not end at depth 0/0: %r' % (final,))
    idx = []
    for name, prefix, _ in CUTS:
        hits = [k for k, l in enumerate(lines) if l.decode('latin-1').startswith(prefix)]
        if len(hits) != 1: sys.exit('anchor %r found %d times' % (prefix, len(hits)))
        k = hits[0]
        if idx and k <= idx[-1]: sys.exit('anchor %r is out of order' % prefix)
        if starts[k] != (0, 0): sys.exit('anchor %r (line %d) is at depth %r' % (prefix, k + 1, starts[k]))
        idx.append(k)
    idx.append(len(lines))
    out = lines[:idx[0]]
    for j, (name, prefix, what) in enumerate(CUTS):
        part = lines[idx[j]:idx[j + 1]]
        if name is None:
            out += part; continue
        path = os.path.join(ROOT, 'src', name + '.h')
        if os.path.exists(path): sys.exit(path + ' already exists')
        head = ('// %s (a fragment of weaponmod.cpp, included in order, not standalone)\r\n' % what).encode('ascii')
        with open(path, 'wb') as f: f.write(head + b''.join(part))
        out.append(('#include "%s.h"\r\n' % name).encode('ascii'))
        print('%-26s %5d lines  (from line %d)' % (name + '.h', len(part) + 1, idx[j] + 1))
    with open(SRC, 'wb') as f: f.write(b''.join(out))
    print('weaponmod.cpp now %d lines' % len(out))


if __name__ == '__main__':
    main()
