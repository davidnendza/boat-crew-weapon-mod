# For each named GunMount prefab, lists (in serialized order) which transforms of its own tree its fields point to. Read-only.
import UnityPy, json, struct, sys
env = UnityPy.load(r'd:/steam/steamapps/common/Boat Crew/Boat Crew_Data/data.unity3d')
mounts = json.load(open('mounts.json'))
byfile = {}
for f in env.files.values():
    for sf in (f.files.values() if hasattr(f, 'files') else [f]):
        if hasattr(sf, 'objects'): byfile[sf.name] = sf
for want in sys.argv[1:]:
    m = next((m for m in mounts if m[0] == want and m[3].startswith('sharedassets')), None)
    sf = byfile[m[3]]
    names = {}
    def walk(tr_ptr, path):
        tr = tr_ptr.read(); go = tr.m_GameObject.read()
        p = path + '/' + go.m_Name if path else go.m_Name
        names[tr_ptr.path_id] = p
        names[tr.m_GameObject.path_id] = p + ' (GO)'
        for ch in tr.m_Children: walk(ch, p)
    go = sf.objects[m[2]].read()
    trc = next(c for c in go.m_Components if (c.component if hasattr(c, 'component') else c).type.name == 'Transform')
    walk(trc.component if hasattr(trc, 'component') else trc, '')
    raw = sf.objects[m[1]].get_raw_data()
    print('=====', want, len(raw), 'bytes')
    for o in range(0, len(raw) - 12, 4):
        fid, pid = struct.unpack_from('<iq', raw, o)
        if fid == 0 and pid in names: print('  @%4d %s' % (o, names[pid]))
