# Adds IL2CPP method names to the GameAssembly.dll+0xRVA lines of a threadsnap output.
#   python symbolize.py snap.txt [path to bcdis.py, default C:/Temp/bcmod/bcdis.py] > snap_named.txt
# Needs the IL2CPP dump that bcdis.py loads (meta.json); run it from any folder.
import sys, os, re, bisect, importlib.util

src = sys.argv[1]
bcdis = sys.argv[2] if len(sys.argv) > 2 else 'C:/Temp/bcmod/bcdis.py'
here = os.getcwd()
os.chdir(os.path.dirname(bcdis))
spec = importlib.util.spec_from_file_location('bcdis_mod', bcdis)
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)
os.chdir(here)
starts, names = b.starts, b.names

pat = re.compile(r'GameAssembly\.dll\+0x([0-9A-Fa-f]+)')


def name(m):
    va = 0x180000000 + int(m.group(1), 16)
    j = bisect.bisect_right(starts, va) - 1
    if j < 0:
        return m.group(0)
    fn = starts[j]
    return '%s  [%s +0x%X]' % (m.group(0), names.get(fn, ('', '?'))[1], va - fn)


for line in open(src, encoding='utf-8', errors='replace'):
    sys.stdout.write(pat.sub(name, line))
