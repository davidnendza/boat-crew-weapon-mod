# Lists GameObjects that carry a GunMount MonoBehaviour in the game's data.unity3d (read-only).
import UnityPy, sys, pickle, collections
env = UnityPy.load(r'd:/steam/steamapps/common/Boat Crew/Boat Crew_Data/data.unity3d')
scripts = {}
mounts = []
types = collections.Counter()
for obj in env.objects:
    types[obj.type.name] += 1
    if obj.type.name == 'MonoBehaviour':
        try:
            mb = obj.read(check_read=False)
            sc = mb.m_Script.read() if mb.m_Script else None
            nm = sc.m_ClassName if sc else None
        except Exception as e:
            continue
        if nm == 'GunMount':
            go = mb.m_GameObject.read()
            mounts.append((go.m_Name, obj.path_id, mb.m_GameObject.path_id, obj.assets_file.name))
import json; json.dump(mounts, open("mounts.json","w"))
print(len(mounts), 'GunMounts')
c=collections.Counter(m[0] for m in mounts)
for k,v in sorted(c.items()): print(v, k)
