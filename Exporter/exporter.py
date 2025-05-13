import bpy
import mathutils
from struct import pack
import numpy as np

VERTS_PER_TRI = 3
NORMAL_FLOATS_PER_VERT = 3
POS_FLOATS_PER_VERT = 3
UV_FLOATS_PER_VERT = 2

scene = bpy.data.scenes[0]

obj = scene.objects.get("Suzanne")
mesh = obj.data

verts = np.zeros(POS_FLOATS_PER_VERT * len(mesh.vertices), dtype=np.float32) # (num_mesh_verts * 3)
mesh.vertices.foreach_get("co", verts)

indices = np.zeros(VERTS_PER_TRI * len(mesh.loop_triangles), dtype=np.int32)
mesh.loop_triangles.foreach_get("vertices", indices)

verts = verts.reshape(-1, POS_FLOATS_PER_VERT) # (num_mesh_verts, 3)
verts = verts[indices]
# verts = verts.flatten()

normals = np.zeros(NORMAL_FLOATS_PER_VERT * VERTS_PER_TRI * len(mesh.loop_triangles), dtype=np.float32)
mesh.loop_triangles.foreach_get("split_normals", normals)
normals = normals.reshape(len(mesh.loop_triangles),VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)
for i in range(normals.shape[0]):
    for j in range(normals.shape[1]):
        if (normals[i, j] == np.zeros(3, dtype=np.float32)).all():
            print(f"zero'd normal at {i}, {j}")

loop_indices = np.zeros(VERTS_PER_TRI * len(mesh.loop_triangles), dtype = np.int32)
mesh.loop_triangles.foreach_get("loops", loop_indices)

uv_layer = mesh.uv_layers[0].uv
loop_uvs = np.zeros(UV_FLOATS_PER_VERT * len(uv_layer), dtype = np.float32) # (num_mesh_loops * 2)
# uv_layer.foreach_get("vector", loop_uvs)
loop_uvs = loop_uvs.reshape(-1, UV_FLOATS_PER_VERT) # (num_mesh_loops, 2)

uv = loop_uvs[loop_indices]
uv = uv.reshape(len(mesh.loop_triangles), VERTS_PER_TRI, UV_FLOATS_PER_VERT)

print("normals shape:", normals.shape)
print("uv shape:", uv.shape)

NORMAL_UV_STRIDE = UV_FLOATS_PER_VERT + NORMAL_FLOATS_PER_VERT
# normal_uv_interleaved = np.zeros((len(mesh.loop_triangles), VERTS_PER_TRI, NORMAL_UV_STRIDE), dtype=np.float32)
# normal_uv_interleaved[..., ::NORMAL_UV_STRIDE] = normals
# normal_uv_interleaved[..., NORMAL_FLOATS_PER_VERT::NORMAL_UV_STRIDE] = uv
normal_uv_interleaved = np.concatenate((normals, uv), axis=-1)

print("interleaved shape:", normal_uv_interleaved.shape)
print("interleaved:", normal_uv_interleaved)


def pack_bytes(layout : str, array : np.array):
    return pack(layout * array.size, *array.flatten())

with open('scene.jj', 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 0))
    # number of triangles
    f.write(pack("I", len(mesh.loop_triangles)))
    # index of first triangle
    f.write(pack("I", 0)) # this is redundant

    # write vertex positions
    # f.write(pack("f" * verts.size, *verts.flatten()))
    f.write(pack_bytes("f", verts))

    # write interleaved normal and uv
    f.write(pack("f" * normal_uv_interleaved.size, *normal_uv_interleaved.flatten()))
print("File written successfully")
# use this to write "scene.jj" into your working directory
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" H:\CSE125\suzanne.blend --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
