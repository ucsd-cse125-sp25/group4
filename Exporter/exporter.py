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

verts = verts.reshape(-1, 3)
verts = verts[indices]
verts = verts.flatten()

with open('scene.jj', 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 0))
    # number of triangles
    f.write(pack("I", len(mesh.loop_triangles) * 3))
    # index of first triangle
    f.write(pack("I", 0))

    # write data
    f.write(pack("f" * len(verts), *verts))

print("File written successfully")
# use this to write "scene.jj" into your working directory
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" H:\CSE125\suzanne.blend --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\ClientApp\exporter.py"
