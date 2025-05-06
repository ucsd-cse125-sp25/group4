import bpy
import mathutils
from struct import pack
import numpy as np

scene = bpy.data.scenes[0]

obj = scene.objects.get("Suzanne")
mesh = obj.data

verts = np.zeros(3 * len(mesh.vertices), dtype=np.float32)
mesh.vertices.foreach_get("co", verts)

indices = np.zeros(3 * len(mesh.loop_triangles), dtype=np.int32)
mesh.loop_triangles.foreach_get("vertices", indices)

verts = verts[indices]

with open('scene.jj', 'wb') as f:
    # write header 
    f.write(pack("I", 0))
    f.write(pack("I", len(mesh.vertices)))
    f.write(pack("I", 3)) # 3 byte offset
    f.write(pack("f" * len(verts), *verts))

print("File written successfully")
# "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe"
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" suzanne.blend --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\ClientApp\exporter.py"
