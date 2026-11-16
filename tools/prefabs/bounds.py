# For each named GunMount prefab: every LOD0/plain mesh's bounds expressed in the frame of root, pivot ('weapon') and
# its barrel, using the real quaternions. Read-only.
import UnityPy, json, sys
import numpy as np
env = UnityPy.load(r'd:/steam/steamapps/common/Boat Crew/Boat Crew_Data/data.unity3d')
mounts = json.load(open('mounts.json'))
byfile = {}
for f in env.files.values():
    for sf in (f.files.values() if hasattr(f, 'files') else [f]):
        if hasattr(sf, 'objects'): byfile[sf.name] = sf
def qmat(q):
    x, y, z, w = q.x, q.y, q.z, q.w
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)], [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)], [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
def trs(tr):
    M = np.eye(4); M[:3, :3] = qmat(tr.m_LocalRotation) @ np.diag([tr.m_LocalScale.x, tr.m_LocalScale.y, tr.m_LocalScale.z])
    M[:3, 3] = [tr.m_LocalPosition.x, tr.m_LocalPosition.y, tr.m_LocalPosition.z]; return M
for want in sys.argv[1:]:
    m = next(m for m in mounts if m[0] == want and m[3].startswith('sharedassets'))
    sf = byfile[m[3]]; go = sf.objects[m[2]].read()
    rootc = next(c for c in go.m_Components if (c.component if hasattr(c, 'component') else c).type.name == 'Transform')
    print('=====', want)
    frames = {}
    def walk(trp, M, path, top=False):
        tr = trp.read(); g = tr.m_GameObject.read(); Ml = np.eye(4) if top else M @ trs(tr)
        p = '' if top else path + '/' + g.m_Name
        frames[p] = Ml
        for c in g.m_Components:
            co = (c.component if hasattr(c, 'component') else c)
            if co.type.name != 'MeshFilter': continue
            mf = co.read()
            if not mf.m_Mesh: continue
            if any(s in g.m_Name for s in ('LOD1', 'LOD2', 'LOD3', '(1)', '(2)', '(3)')) and 'Package' not in p: pass
            mesh = mf.m_Mesh.read(); b = mesh.m_LocalAABB
            c0 = np.array([b.m_Center.x, b.m_Center.y, b.m_Center.z]); e = np.array([b.m_Extent.x, b.m_Extent.y, b.m_Extent.z])
            corners = np.array([[c0[0]+sx*e[0], c0[1]+sy*e[1], c0[2]+sz*e[2], 1] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
            out = []
            for fname in ('', '/weapon'):
                base = next((k for k in frames if k == fname), None)
                if base is None: continue
                Minv = np.linalg.inv(frames[base]) @ Ml
                w = (Minv @ corners.T).T[:, :3]
                out.append('%s[%s]' % ('root' if fname == '' else 'pivot', ' '.join('%.2f..%.2f' % (w[:, i].min(), w[:, i].max()) for i in range(3))))
            if 'LOD1' in mesh.m_Name or 'LOD2' in mesh.m_Name or 'LOD3' in mesh.m_Name: continue
            print('  %-60s %-28s %s' % (p[-60:], mesh.m_Name[:28], '  '.join(out)))
        for ch in tr.m_Children: walk(ch, Ml, p)
    walk(rootc.component if hasattr(rootc, 'component') else rootc, np.eye(4), '', True)
