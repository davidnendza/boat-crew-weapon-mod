"""Proves the split moved text only: expands each '#include "wm_*.h"' line in src/weaponmod.cpp (dropping the
fragment's added first comment line) and compares the result byte for byte with weaponmod.cpp at a git revision.

  python tools/split_check.py            compare with HEAD (before the split is committed)
  python tools/split_check.py <rev>      compare with another revision, e.g. the commit before the split
Exit code 0 = identical.
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rev = sys.argv[1] if len(sys.argv) > 1 else 'HEAD'
want = subprocess.run(['git', 'show', rev + ':src/weaponmod.cpp'], cwd=ROOT, capture_output=True, check=True).stdout
if b'#include "wm_' in want: sys.exit('%s is already split; pass the revision before the split' % rev)

out = []
for line in open(os.path.join(ROOT, 'src', 'weaponmod.cpp'), 'rb').read().splitlines(keepends=True):
    m = re.match(rb'#include "(wm_\w+\.h)"\r\n$', line)
    if not m:
        out.append(line); continue
    frag = open(os.path.join(ROOT, 'src', m.group(1).decode()), 'rb').read()
    out.append(frag.split(b'\r\n', 1)[1])
got = b''.join(out)
if got == want:
    print('identical: %d bytes, %d lines' % (len(got), got.count(b'\n'))); sys.exit(0)
k = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
print('DIFFERENT at byte %d (line %d): %d vs %d bytes' % (k, want.count(b'\n', 0, k) + 1, len(got), len(want)))
sys.exit(1)
