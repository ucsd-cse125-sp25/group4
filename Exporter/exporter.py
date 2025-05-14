import bpy
import mathutils
from struct import pack
import numpy as np
from dataclasses import dataclass

VERTS_PER_TRI = 3
NORMAL_FLOATS_PER_VERT = 3
POS_FLOATS_PER_VERT = 3
UV_FLOATS_PER_VERT = 2

@dataclass
class Mesh:
    num_tris       : int
    vert_positions : np.array # (objs, tris, VERTS_PER_TRI, POS_FLOATS_PER_VERT)
    vert_shade     : np.array # (objs, tris, VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)
    material_ids   : np.array # (objs, tris)
    
    def merge(self, other):
        if other is None:
            return
        self.num_tris       = self.num_tris + other.num_tris
        self.vert_positions = np.concat((self.vert_positions, other.vert_positions))
        self.vert_shade     = np.concat((self.vert_shade    , other.vert_shade))
        self.material_ids   = np.concat((self.material_ids  , other.material_ids))

@dataclass
class Material:
    base_color : int
    metallic   : int
    roughness  : int
    normal     : int


material_names_to_indices : dict[str, int] = {mat.name : i for i, mat in enumerate(bpy.data.materials)}

consolidated_mesh : Mesh = None

# meshes        : list[Mesh] = []
materials     : list[Material] = []
texture_names : list[str] = []


for obj in bpy.data.objects:
    if obj.type != "MESH" or obj.name[:3] == "bb#":
        continue
    
    print(f"processing object {obj.name}")
    bmesh = obj.data

    # vertex positions
    verts = np.zeros(POS_FLOATS_PER_VERT * len(bmesh.vertices), dtype=np.float32) # (num_bmesh_verts * 3)
    bmesh.vertices.foreach_get("co", verts)

    indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.int32)
    bmesh.loop_triangles.foreach_get("vertices", indices)

    verts = verts.reshape(-1, POS_FLOATS_PER_VERT) # (num_bmesh_verts, 3)
    verts = verts[indices]
    verts = verts.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, POS_FLOATS_PER_VERT)
    # TODO: transform verts into world space

    # normals
    normals = np.zeros(NORMAL_FLOATS_PER_VERT * VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.float32)
    bmesh.loop_triangles.foreach_get("split_normals", normals)
    normals = normals.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)
    # TODO: transform normals by inverse transpose

    # texture coordinates
    loop_indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype = np.int32)
    bmesh.loop_triangles.foreach_get("loops", loop_indices)

    uv_layer = bmesh.uv_layers[0].uv
    loop_uvs = np.zeros(UV_FLOATS_PER_VERT * len(uv_layer), dtype = np.float32) # (num_bmesh_loops * 2)
    loop_uvs = loop_uvs.reshape(-1, UV_FLOATS_PER_VERT) # (num_bmesh_loops, 2)

    uv = loop_uvs[loop_indices]
    uv = uv.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, UV_FLOATS_PER_VERT)

    print("normals shape:", normals.shape)
    print("uv shape:", uv.shape)

    normal_uv_interleaved = np.concatenate((normals, uv), axis=-1)

    print("interleaved shape:", normal_uv_interleaved.shape)
    print("interleaved:", normal_uv_interleaved)


    # material ids    
    triangle_to_slot_index = np.zeros(len(bmesh.loop_triangles), dtype=np.int32)
    bmesh.loop_triangles.foreach_get("material_index", triangle_to_slot_index)
    slot_index_to_materialid = np.array([material_names_to_indices[slot.material.name] for slot in obj.material_slots])
    material_ids = slot_index_to_materialid[triangle_to_slot_index]

    # add batch dimension for easy concatenation with other meshes
    new_mesh = Mesh(
                       num_tris       = len(bmesh.loop_triangles),
                       vert_positions = verts[np.newaxis, :],
                       vert_shade     = normal_uv_interleaved[np.newaxis, :],
                       material_ids   = material_ids[np.newaxis, :])
    consolidated_mesh = new_mesh.merge(consolidated_mesh)


    

def pack_bytes(layout : str, array : np.array) -> bytes:
    return pack(layout * array.size, *array.flatten())


with open('scene.jj', 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 0))
    # number of triangles
    f.write(pack("I", consilidated_mesh.num_tris))
    # index of first triangle
    f.write(pack("I", 0)) # this is redundant

    # write vertex positions
    # f.write(pack("f" * verts.size, *verts.flatten()))
    f.write(pack_bytes("f", consolidated_mesh.vert_positions))

    # write shading data
    f.write(pack_bytes("f", consolidated_mesh.vert_shade))
print("File written successfully")
# use this to write "scene.jj" into your working directory
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" H:\CSE125\suzanne.blend --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
