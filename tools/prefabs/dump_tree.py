# Prints the transform tree (local pos/rot/scale, mesh bounds) of one GunMount prefab per name. Read-only.
import UnityPy, json, math, sys
WANT = sys.argv[1:]
env = UnityPy.load(r'd:/steam/steamapps/common/Boat Crew/Boat Crew_Data/data.unity3d')
mounts = json.load(open('mounts.json'))
files = {f.name: f for f in env.assets} if hasattr(env, 'assets') else None
byfile = {}
for f in env.files.values():
    for sf in (f.files.values() if hasattr(f, 'files') else [f]):
        if hasattr(sf, 'objects'): byfile[sf.name] = sf
def euler(q):
    x, y, z, w = q.x, q.y, q.z, q.w
    # Unity ZXY order approximation, degrees
    sinp = 2 * (w * x - y * z); sinp = max(-1, min(1, sinp))
    ex = math.degrees(math.asin(sinp))
    ey = math.degrees(math.atan2(2 * (w * y + x * z), 1 - 2 * (x * x + y * y)))
    ez = math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (x * x + z * z)))
    return ex, ey, ez
def walk(tr, depth):
    go = tr.m_GameObject.read()
    lp, lr, ls = tr.m_LocalPosition, tr.m_LocalRotation, tr.m_LocalScale
    info = ''
    for c in go.m_Components:
        try:
            co = c.component.read() if hasattr(c, 'component') else c.read()
        except Exception: continue
        tn = co.object_reader.type.name if hasattr(co, 'object_reader') else type(co).__name__
        if tn == 'MeshFilter' and co.m_Mesh:
            try:
                m = co.m_Mesh.read(); b = m.m_LocalAABB
                info += ' mesh=%s c=(%.2f,%.2f,%.2f) e=(%.2f,%.2f,%.2f)' % (m.m_Name, b.m_Center.x, b.m_Center.y, b.m_Center.z, b.m_Extent.x, b.m_Extent.y, b.m_Extent.z)
            except Exception as e: info += ' mesh=?'
        elif tn in ('MeshRenderer', 'SkinnedMeshRenderer', 'ParticleSystem', 'LODGroup'):
            info += ' ' + tn
        elif tn == 'MonoBehaviour':
            try: info += ' [%s]' % co.m_Script.read().m_ClassName
            except Exception: pass
    e = euler(lr)
    print('%s%s  pos(%.2f,%.2f,%.2f) rot(%.0f,%.0f,%.0f) scl(%.2f,%.2f,%.2f)%s' % ('  ' * depth, go.m_Name, lp.x, lp.y, lp.z, e[0], e[1], e[2], ls.x, ls.y, ls.z, info))
    for ch in tr.m_Children:
        walk(ch.read(), depth + 1)
for want in WANT:
    m = next((m for m in mounts if m[0] == want and m[3].startswith('sharedassets')), None)
    if not m: print('NOT FOUND', want); continue
    sf = byfile.get(m[3])
    go = sf.objects[m[2]].read()
    tr = next(c for c in go.m_Components if (c.component if hasattr(c,'component') else c).type.name in ('Transform', 'RectTransform'))
    tr = (tr.component if hasattr(tr, 'component') else tr).read()
    print('=====', want, m[3], m[2])
    walk(tr, 0)
