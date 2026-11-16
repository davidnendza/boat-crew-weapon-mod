"""Reads gun prefabs, meshes, materials and textures from the game's data.unity3d (read-only), for the model tools.

Frames: every prefab node has a 4x4 matrix in the prefab root's frame (Unity: left-handed, +Y up, +Z forward). Meshes come
out as triangles in any node's frame, with their UVs, so a new model can reuse the game's own meshes and materials.
Needs UnityPy (pip) and the game installed; run python WITHOUT -I (UnityPy is in the user site).
"""
import functools
import pathlib

import numpy as np

GAME = pathlib.Path(r'd:/steam/steamapps/common/Boat Crew/Boat Crew_Data/data.unity3d')


@functools.lru_cache(maxsize=1)
def env():
    import UnityPy
    return UnityPy.load(str(GAME))


@functools.lru_cache(maxsize=1)
def _gameobjects():
    """name -> [GameObject object readers] for every GameObject."""
    out = {}
    for obj in env().objects:
        if obj.type.name == 'GameObject':
            try:
                out.setdefault(obj.peek_name(), []).append(obj)
            except Exception:
                pass
    return out


def _comp(c):
    return c.component if hasattr(c, 'component') else c


def qmat(q):
    x, y, z, w = q.x, q.y, q.z, q.w
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def trs(tr):
    M = np.eye(4)
    M[:3, :3] = qmat(tr.m_LocalRotation) @ np.diag([tr.m_LocalScale.x, tr.m_LocalScale.y, tr.m_LocalScale.z])
    M[:3, 3] = [tr.m_LocalPosition.x, tr.m_LocalPosition.y, tr.m_LocalPosition.z]
    return M


class Prefab:
    """One prefab tree: nodes by path ('weapon/Weapon model/LOD0'), each with its matrix in the root frame."""

    def __init__(self, name):
        roots, nested = [], []                            # most gun prefabs sit under a holder object, so take a root
        for go_obj in _gameobjects().get(name, []):     # if there is one, else the first one in the shared assets
            if not go_obj.assets_file.name.startswith('sharedassets'):
                continue
            go = go_obj.read()
            tr = next((_comp(c) for c in go.m_Components if _comp(c).type.name == 'Transform'), None)
            if tr is None:
                continue
            t = tr.read()
            (roots if t.m_Father.path_id == 0 else nested).append(t)
        roots += nested
        if not roots:
            raise KeyError('no prefab named %r' % name)
        self.name = name
        self.nodes = {}                                   # path -> (transform, gameobject, matrix in root frame)
        self._walk(roots[0], np.eye(4), '', True)

    def _walk(self, tr, M, path, top=False):
        go = tr.m_GameObject.read()
        Ml = np.eye(4) if top else M @ trs(tr)
        p = '' if top else (path + '/' + go.m_Name if path else go.m_Name)
        self.nodes[p] = (tr, go, Ml)
        for ch in tr.m_Children:
            self._walk(ch.read(), Ml, p)

    def frame(self, path):
        return self.nodes[path][2]

    def find(self, suffix):
        """The one node whose path ends with suffix."""
        hits = [p for p in self.nodes if p == suffix or p.endswith('/' + suffix)]
        if len(hits) != 1:
            raise KeyError('%s: %d nodes match %r: %s' % (self.name, len(hits), suffix, hits[:6]))
        return hits[0]

    def mesh(self, path, frame='', submesh=None):
        """The node's mesh as (tris, material names): tris = [(material index, 3 points, 3 normals, 3 uvs)] in the
        frame of node `frame` ('' = prefab root)."""
        tr, go, M = self.nodes[path]
        mesh = None
        mats = []
        for c in go.m_Components:
            co = _comp(c)
            if co.type.name == 'MeshFilter':
                mf = co.read()
                if mf.m_Mesh and mf.m_Mesh.path_id:
                    mesh = mf.m_Mesh.read()
            elif co.type.name in ('MeshRenderer', 'SkinnedMeshRenderer'):
                r = co.read()
                for pp in r.m_Materials:
                    try:
                        mats.append(pp.read().m_Name)
                    except Exception:
                        mats.append('?')
        if mesh is None:
            raise KeyError('%s: node %r has no mesh' % (self.name, path))
        from UnityPy.helpers.MeshHelper import MeshHandler
        h = MeshHandler(mesh)
        h.process()
        n = h.m_VertexCount

        def channel(data, k):                             # UnityPy gives 3 or 4 floats per normal, depending on the mesh
            return np.array(data, float).reshape(n, -1)[:, :k] if data is not None and len(data) else None
        V = channel(h.m_Vertices, 3)
        N = channel(h.m_Normals, 3)
        N = np.zeros_like(V) if N is None else N
        UV = channel(h.m_UV0, 2)
        UV = np.zeros((n, 2)) if UV is None else UV
        F = np.linalg.inv(self.frame(frame)) @ M
        R, t = F[:3, :3], F[:3, 3]
        Ninv = np.linalg.inv(R).T
        Vf = V @ R.T + t
        Nf = N @ Ninv.T
        Nf /= np.maximum(np.linalg.norm(Nf, axis=1, keepdims=True), 1e-9)
        flip = np.linalg.det(R) < 0                       # a mirrored node turns the triangles inside out
        tris = []
        for si, tri_list in enumerate(h.get_triangles()):
            if submesh is not None and si != submesh:
                continue
            for a, b, c in tri_list:
                if flip:
                    b, c = c, b
                tris.append((si, Vf[[a, b, c]], Nf[[a, b, c]], UV[[a, b, c]]))
        return tris, mats


    def visible_nodes(self, under=''):
        """Paths of every mesh node shown in game (LOD0 only, active, no debug spheres or decor crew) under `under`."""
        out = []
        for path, (tr, go, M) in self.nodes.items():
            if under and not (path == under or path.startswith(under + '/')):
                continue
            if any(s in path for s in ('LOD1', 'LOD2', 'LOD3', 'Crew', 'direction')):
                continue
            if not all(self.nodes[q][1].m_IsActive for q in self._ancestors(path)):
                continue
            if not any(_comp(c).type.name == 'MeshFilter' for c in go.m_Components):
                continue
            try:
                _, mats = self.mesh(path, '')
            except KeyError:
                continue
            if any(m in ('Blue', 'Green', 'Red') for m in mats):
                continue
            out.append(path)
        return out

    def visible_meshes(self, frame='', under=''):
        out = []
        for path in self.visible_nodes(under):
            tris, mats = self.mesh(path, frame)
            out += [(mats[min(si, len(mats) - 1)], ps, ns, uv) for si, ps, ns, uv in tris]
        return out

    def _ancestors(self, path):
        parts = path.split('/') if path else []            # the root itself is left out: prefab roots are stored off
        return ['/'.join(parts[:i + 1]) for i in range(len(parts))]


@functools.lru_cache(maxsize=None)
def material(name):
    """Shader, colours, floats and main texture (PIL image or None) of a material, by name."""
    for obj in env().objects:
        if obj.type.name != 'Material':
            continue
        try:
            if obj.peek_name() != name:
                continue
        except Exception:
            continue
        m = obj.read()
        sp = m.m_SavedProperties
        cols = {k: (v.r, v.g, v.b) for k, v in sp.m_Colors}
        floats = dict(sp.m_Floats)
        tex = None
        scale = (1.0, 1.0)
        for k, v in sp.m_TexEnvs:
            if k == '_MainTex' and v.m_Texture and v.m_Texture.path_id:
                tex = v.m_Texture.read().image.convert('RGB')
                scale = (v.m_Scale.x, v.m_Scale.y)
        try:
            shader = m.m_Shader.read().m_ParsedForm.m_Name
        except Exception:
            shader = '?'
        return dict(name=name, shader=shader, colors=cols, floats=floats, tex=tex, scale=scale)
    raise KeyError('no material %r' % name)
