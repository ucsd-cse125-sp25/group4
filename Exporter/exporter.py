import bpy
import mathutils
from struct import pack
import numpy as np
from dataclasses import dataclass
import os

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
    # float3 is compacted to 1 bit tag and then R10G11B10 (stored in int type)
    # floats are negated before compacting so there is a 1-bit tag
    
    base_color : int
    metallic   : int | float
    roughness  : int | float
    normal     : int

    # default material; same as default for Blender Principled
    def default():
        return Material(
        # 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
        # TT RR RR RR RR RR RR RR RR RR RR GG GG GG GG GG GG GG GG GG GG GG BB BB BB BB BB BB BB BB BB BB
        base_color = 1 << 31 | round(0.8*(2**10)) << 21 | round(0.8*(2**11)) << 10 | round(0.8 * (2**10)),
        metallic   = -0.0,
        roughness  = -0.5,
        normal     = 1 << 31)

    def serialize(self) -> bytes:
        print(type(self.base_color), self.base_color)
        print(type(self.metallic), self.metallic)
        print(type(self.roughness), self.roughness)
        print(type(self.normal), self.normal)
        metallic_format = "I" if isinstance(self.metallic, int) else "f"
        roughness_format = "I" if isinstance(self.roughness, int) else "f"

        return pack(f"I{metallic_format}{roughness_format}I", self.base_color, self.metallic, self.roughness, self.normal)

@dataclass
class MaterialData:
    base_color : bpy.types.Image | bpy.types.bpy_prop_array
    metallic   : bpy.types.Image | float
    roughness  : bpy.types.Image | float
    normal     : bpy.types.Image | bpy.types.bpy_prop_array

material_names_to_indices : dict[str, int] = {"default_material" : 0} | {mat.name : i+1 for i, mat in enumerate(bpy.data.materials)}
num_textures = 0

consolidated_mesh : Mesh = None

materials     : list[Material] = [Material.default()]
texture_paths : list[str] = []

def clamp(x : float, a : float, b : float) -> float:
    return max(a, min(x, b))

def get_node_input(input : bpy.types.NodeSocket)-> int | float | bpy.types.bpy_prop_array:
    if not input.is_linked:
        return input.default_value

    from_node = input.links[0].from_node
    if from_node.bl_idname == "ShaderNodeNormalMap":
        return from_node.inputs["Color"].links[0].from_node.image
    elif from_node.bl_idname == "ShaderNodeTexImage":
        return from_node.image
    else:
        return input.default_value
    
# process materials
for material in bpy.data.materials:
    no_users       : bool = material.users == 0
    only_fake_user : bool = material.users == 1 and material.use_fake_user
    if no_users or only_fake_user:
        continue
    
    output = material.node_tree.get_output_node("ALL")
    principled = output.inputs["Surface"].links[0].from_node

    material_name_file = material.name.replace(".", "_")

    d = {
        "base_color" :  get_node_input(principled.inputs["Base Color"]),
        "metallic"   :  get_node_input(principled.inputs["Metallic"]),
        "roughness"  :  get_node_input(principled.inputs["Roughness"]),
        "normal"     :  get_node_input(principled.inputs["Normal"]),
    }

    output = {
        "base_color" :  None,
        "metallic"   :  None,
        "roughness"  :  None,
        "normal"     :  None,
    }
    
    for input_type, input in d.items():
        # save images
        if isinstance(input, bpy.types.Image):
            filename = f"{material_name_file}_{input_type}"
            filepath_save = f"./textures_raw/{filename}.png"
            filepath_dds = f"./textures/{filename}.dds"
            
            if not os.path.exists(filepath_save):
                input.save(filepath=filepath_save, quality=100, save_copy=True)
            
            texture_paths.append(filepath_dds)
            output[input_type] = num_textures
            num_textures += 1
        elif isinstance(input, float):
            output[input_type] = -input
        else:
            assert(isinstance(input, bpy.types.bpy_prop_array))
            # simply use a tag to indicate default normal
            if input_type == "normal":
                output[input_type] = 1 << 31
                continue
            
            r = clamp(input[0], 0, 1)
            g = clamp(input[1], 0, 1)
            b = clamp(input[2], 0, 1)

            # pack into R10G11B10
            tag_packed         = 1 << 31
            r_packed           = round(r * 2**10) << (11 + 10)
            g_packed           = round(g * 2**11) << 10
            b_packed           = round(b * 2**10)
            packed             = tag_packed | r_packed | g_packed | b_packed
            output[input_type] = packed
            
    materials.append(Material(**output))
            

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
        # DirectX texture coordinates have a inverted u/y axis compared to Blender
        uv[:, :, 1] = 1 - uv[:, :, 1]
    
    # interleave normal and uv information
    normal_uv_interleaved = np.concatenate((normals, uv), axis=-1)

    # material ids
    if len(obj.material_slots) == 0: # edge case: object has no materials
        material_ids = np.zeros(len(bmesh.loop_triangles), dtype=np.uint16)
    else:
        slot_index_to_materialid = np.array([material_names_to_indices[slot.material.name] for slot in obj.material_slots])
        triangle_to_slot_index = np.zeros(len(bmesh.loop_triangles), dtype=np.uint16)
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
    return pack(f"{array.size}{layout}", *array.flatten())


with open('scene.jj', 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 2))
    # number of triangles
    f.write(pack("I", consolidated_mesh.num_tris))
    # number of materials
    f.write(pack("I", len(materials)))
    # number of textures
    f.write(pack("I", len(texture_paths)))

    # write vertex positions
    f.write(pack_bytes("f", consolidated_mesh.vert_positions))

    # write shading data
    f.write(pack_bytes("f", consolidated_mesh.vert_shade))

    # write material ids
    f.write(pack_bytes("H", consolidated_mesh.material_ids))

    # write materials
    for material in materials:
        f.write(material.serialize())
    # write texture names
    for path in texture_paths:
        # windows uses the wchar_t type which is utf_16_le
        path_bytes = path.encode("utf_16_le")
        # we use 256 byte strings and need room for 1 null-terminator
        assert(len(path_bytes) < 512)
        # missing bytes are padded with null terminators
        f.write(pack("512s", path_bytes))
        
print("File written successfully")
# use this to write "scene.jj" into your working directory
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" "C:\Users\eekgasit\Downloads\bedroomv4.blend" --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
