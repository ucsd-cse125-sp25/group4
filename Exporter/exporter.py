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
    vert_positions : np.array # (tris, VERTS_PER_TRI, POS_FLOATS_PER_VERT)
    vert_shade     : np.array # (tris, VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)
    material_ids   : np.array # (tris)
    
    def merge(self, other):
        if other is not None:
            self.num_tris       = self.num_tris + other.num_tris
            self.vert_positions = np.concatenate((self.vert_positions, other.vert_positions))
            self.vert_shade     = np.concatenate((self.vert_shade    , other.vert_shade))
            self.material_ids   = np.concatenate((self.material_ids  , other.material_ids))
        return self

@dataclass
class Material:
    base_color : int
    metallic   : int
    roughness  : int
    normal     : int


material_names_to_indices : dict[str, int] = {"default_material" : 0} | {mat.name : i+1 for i, mat in enumerate(bpy.data.materials)}

consolidated_mesh : Mesh = None

# meshes        : list[Mesh] = []
materials     : list[Material] = []
texture_names : list[str] = []


for obj in bpy.data.objects:
    if obj.type != "MESH" or obj.name[:3] == "bb#":
        continue
    
    print(f"processing object {obj.name}")
    bmesh = obj.data
    model_to_world_translation   = np.array(obj.matrix_world.to_translation(), dtype=np.float32)
    model_to_world_linear        = np.array(obj.matrix_world.to_3x3(), dtype=np.float32)
    model_to_world_adj_transpose = np.array(obj.matrix_world.to_3x3().adjugated().transposed(), dtype=np.float32)


    print("model_to_world_adj:", model_to_world_adj_transpose)

    # vertex positions
    verts = np.zeros(POS_FLOATS_PER_VERT * len(bmesh.vertices), dtype=np.float32) # (num_bmesh_verts * 3)
    bmesh.vertices.foreach_get("co", verts)

    indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.int32)
    bmesh.loop_triangles.foreach_get("vertices", indices)

    verts = verts.reshape(-1, POS_FLOATS_PER_VERT) # (num_bmesh_verts, 3)
    verts = verts[indices]
    verts = np.dot(verts, model_to_world_linear.T)
    verts = verts + model_to_world_translation
    verts = verts.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, POS_FLOATS_PER_VERT)

    # normals
    normals = np.zeros(NORMAL_FLOATS_PER_VERT * VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.float32)
    bmesh.loop_triangles.foreach_get("split_normals", normals)
    normals = normals.reshape(-1, NORMAL_FLOATS_PER_VERT)
    EPS = 10e-6
    # assert(all(abs(np.linalg.norm(normals, axis=-1) - 1) < EPS))
    norm = np.linalg.norm(normals, axis=-1)
    normals[abs(norm-1) > EPS, :] = np.array([0, 0, 1])
    normals = np.dot(normals, model_to_world_adj_transpose.T)
    norm = np.linalg.norm(normals, axis=-1)[:, np.newaxis]
    normals /= norm
    assert(all(abs(np.linalg.norm(normals, axis=-1) - 1) < EPS))

    normals = normals.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)

    # texture coordinates
    if len(bmesh.uv_layers) == 0: # edge case: object has no UVs
        uv = np.full((len(bmesh.loop_triangles), VERTS_PER_TRI, UV_FLOATS_PER_VERT), 0.5) # sample from the center of the texture
    else:
        uv_layer = bmesh.uv_layers[0].uv
        loop_indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype = np.int32)
        bmesh.loop_triangles.foreach_get("loops", loop_indices)
        loop_uvs = np.zeros(UV_FLOATS_PER_VERT * len(uv_layer), dtype = np.float32) # (num_bmesh_loops * 2)
        uv_layer.foreach_get("vector", loop_uvs)
        loop_uvs = loop_uvs.reshape(-1, UV_FLOATS_PER_VERT) # (num_bmesh_loops, 2)
        uv = loop_uvs[loop_indices]
        uv = uv.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, UV_FLOATS_PER_VERT)
    
    # interleave normal and uv information
    normal_uv_interleaved = np.concatenate((normals, uv), axis=-1)

    # material ids
    if len(obj.material_slots) == 0: # edge case: object has no materials
        material_ids = np.zeros(len(bmesh.loop_triangles), dtype=np.int32)
    else:
        slot_index_to_materialid = np.array([material_names_to_indices[slot.material.name] for slot in obj.material_slots])
        triangle_to_slot_index = np.zeros(len(bmesh.loop_triangles), dtype=np.int32)
        bmesh.loop_triangles.foreach_get("material_index", triangle_to_slot_index)
        material_ids = slot_index_to_materialid[triangle_to_slot_index]
        

    # add batch dimension for easy concatenation with other meshes
    new_mesh = Mesh(
                       num_tris       = len(bmesh.loop_triangles),
                       vert_positions = verts,
                       vert_shade     = normal_uv_interleaved,
                       material_ids   = material_ids
    )
    consolidated_mesh = new_mesh.merge(consolidated_mesh)
    assert(consolidated_mesh is not None)


    

def pack_bytes(layout : str, array : np.array) -> bytes:
    return pack(layout * array.size, *array.flatten())


with open('scene.jj', 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 0))
    # number of triangles
    f.write(pack("I", consolidated_mesh.num_tris))
    # index of first triangle
    f.write(pack("I", 0)) # this is redundant

    # write vertex positions
    # f.write(pack("f" * verts.size, *verts.flatten()))
    f.write(pack_bytes("f", consolidated_mesh.vert_positions))

    # write shading data
    f.write(pack_bytes("f", consolidated_mesh.vert_shade))
print("File written successfully")
# use this to write "scene.jj" into your working directory
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" "C:\Users\eekgasit\Downloads\bedroomv4.blend" --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
