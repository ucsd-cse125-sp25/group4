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
BONES_PER_VERT = 4

@dataclass
class Mesh:
    num_tris       : int
    vert_positions : np.array # (tris, VERTS_PER_TRI, POS_FLOATS_PER_VERT)
    vert_shade     : np.array # (tris, VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)
    material_ids   : np.array # (tris)
    vert_bone_indices : np.array = None # (verts, BONES_PER_VERT)
    vert_bone_weights : np.array = None # (verts, BONES_PER_VERT)
    
    def merge(self, other):
        if other is not None:
            self.num_tris       = self.num_tris + other.num_tris
            self.vert_positions = np.concatenate((self.vert_positions, other.vert_positions))
            self.vert_shade     = np.concatenate((self.vert_shade    , other.vert_shade))
            self.material_ids   = np.concatenate((self.material_ids  , other.material_ids))

            if self.vert_bone_indices is not None:
                assert(self.vert_bone_weights) is not None
                assert(other.vert_bone_indices) is not None
                assert(other.vert_bone_weights) is not None

                self.vert_bone_indices = np.concatenate((self.vert_bone_indices, other.vert_bone_indices))
                self.vert_bone_weights = np.concatenate((self.vert_bone_weights, other.vert_bone_weights))
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
        metallic_format = "I" if isinstance(self.metallic, int) else "f"
        roughness_format = "I" if isinstance(self.roughness, int) else "f"

        return pack(f"I{metallic_format}{roughness_format}I", self.base_color, self.metallic, self.roughness, self.normal)

@dataclass
class MaterialData:
    base_color : bpy.types.Image | bpy.types.bpy_prop_array
    metallic   : bpy.types.Image | float
    roughness  : bpy.types.Image | float
    normal     : bpy.types.Image | bpy.types.bpy_prop_array


used_materials : list[bpy.types.Material]= []
for material in bpy.data.materials:
    no_users       : bool = material.users == 0
    only_fake_user : bool = material.users == 1 and material.use_fake_user
    if no_users or only_fake_user:
        continue
    used_materials.append(material)
    
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
    
material_names_to_indices : dict[str, int] = {"default_material" : 0}
# process materials
i = 1
for material in used_materials:
    if material.name in material_names_to_indices:
        continue
    print("processing material:", material.name)
    material_names_to_indices |= {material.name : i}
    i += 1
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

SCENE = bpy.data.scenes[0]

armature = None
if len(bpy.data.armatures) > 1:
    print("ERROR: scene has more than one armature")
    exit(1)
elif len(bpy.data.armatures) == 1:
    armature = bpy.data.armatures[0]
    bone_name_to_index = {bone.name : i for i, bone in enumerate(armature.bones)}
            
for obj in bpy.data.objects:
    if obj.type != "MESH" or obj.name[:3] == "bb#":
        continue
    
    print(f"processing object {obj.name}")
    # apply modifiers
    # from https://blenderartists.org/t/alternative-to-bpy-ops-object-convert-target-mesh-command/1177790/3
    dependencies_graph = bpy.context.evaluated_depsgraph_get()
    bmesh = obj.evaluated_get(dependencies_graph).data.copy()
    
    model_to_world_translation   = np.array(obj.matrix_world.to_translation(), dtype=np.float32)
    model_to_world_linear        = np.array(obj.matrix_world.to_3x3(), dtype=np.float32)
    model_to_world_adj_transpose = np.array(obj.matrix_world.to_3x3().adjugated().transposed(), dtype=np.float32)



    # vertex positions
    verts = np.zeros((len(bmesh.vertices), POS_FLOATS_PER_VERT), dtype=np.float32) # (num_bmesh_verts, 3)
    bmesh.vertices.foreach_get("co", verts.ravel())

    triangle_vert_indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.int32)
    bmesh.loop_triangles.foreach_get("vertices", triangle_vert_indices)

    verts = np.dot(verts, model_to_world_linear.T)
    verts = verts + model_to_world_translation
    verts = verts[triangle_vert_indices]
    verts = verts.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, POS_FLOATS_PER_VERT)

    # normals
    loop_normals = np.zeros((len(bmesh.loops), NORMAL_FLOATS_PER_VERT), dtype = np.float32)
    bmesh.loops.foreach_get("normal", loop_normals.ravel())
    loop_normals = np.dot(loop_normals, model_to_world_adj_transpose.T)
    loop_indices = np.zeros(VERTS_PER_TRI * len(bmesh.loop_triangles), dtype=np.int32)
    bmesh.loop_triangles.foreach_get("loops", loop_indices)
    normals = loop_normals[loop_indices]
    EPS = 10e-6
    norm = np.linalg.norm(normals, axis=-1)[:, np.newaxis]
    normals /= norm

    normals = normals.reshape(len(bmesh.loop_triangles), VERTS_PER_TRI, NORMAL_FLOATS_PER_VERT)

    # texture coordinates
    if len(bmesh.uv_layers) == 0: # edge case: object has no UVs
        uv = np.full((len(bmesh.loop_triangles), VERTS_PER_TRI, UV_FLOATS_PER_VERT), 0.5) # sample from the center of the texture
    else:
        uv_layer = bmesh.uv_layers[0].uv # WARNING: must be changed later for lighmaps
        loop_uvs = np.zeros((len(uv_layer), UV_FLOATS_PER_VERT), dtype = np.float32) # (num_bmesh_loops * 2)
        uv_layer.foreach_get("vector", loop_uvs.ravel())
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
        

    # vertex groups for skinning
    bone_indices =  None
    bone_weights = None
    if armature is not None:
        bone_indices = np.zeros((len(bmesh.vertices), BONES_PER_VERT), dtype=np.int32)
        bone_weights = np.zeros((len(bmesh.vertices), BONES_PER_VERT), dtype=np.float32)
        if obj.parent_type == "BONE":
            bone_indices[:, 0] = bone_name_to_index[obj.parent_bone]
            bone_weights[:, 0] = 1
        elif obj.find_armature() is not None:
            # map object vertex groups to bone indices
            vertex_group_names = [group.name for group in obj.vertex_groups]
            vertex_group_to_bone = np.array([bone_name_to_index.get(vertex_group_name, 0) for vertex_group_name in vertex_group_names], dtype=np.int32)
        
            for i, vertex in enumerate(bmesh.vertices):

                num_written = min(len(vertex.groups), BONES_PER_VERT)
                vertex_groups = np.zeros(len(vertex.groups), dtype=np.int32)
                vertex_weights = np.zeros(len(vertex.groups), dtype=np.float32)
                vertex.groups.foreach_get("group", vertex_groups)
                vertex.groups.foreach_get("weight", vertex_weights)

                if len(vertex.groups) > BONES_PER_VERT:
                    # get the indices of the most influential BONES_PER_VERT weights/group indices
                    top_selection = np.argpartition(vertex_weights, -BONES_PER_VERT)[-BONES_PER_VERT:]
                    vertex_weights = vertex_weights[top_selection]
                    vertex_groups = vertex_groups[top_selection]

                # normalize vertex weights to add up to 1
                vertex_weights /= np.sum(vertex_weights)

                # convert indices of vertex groups in object to indices of bones in armature
                vertex_bone_indices = vertex_group_to_bone[vertex_groups]

                
                bone_indices[i, :num_written] = vertex_bone_indices
                assert(all(vertex_bone_indices < len(bone_name_to_index)))
                bone_weights[i, :num_written] = vertex_weights
        bone_indices = bone_indices[triangle_vert_indices]
        bone_weights = bone_weights[triangle_vert_indices]
            
            
    
    new_mesh = Mesh(
                       num_tris            = len(bmesh.loop_triangles),
                       vert_positions      = verts,
                       vert_shade          = normal_uv_interleaved,
                       material_ids        = material_ids,
                       vert_bone_indices   = bone_indices,
                       vert_bone_weights   = bone_weights,
    )
    consolidated_mesh = new_mesh.merge(consolidated_mesh)
    assert(consolidated_mesh is not None)



def pack_bytes(layout : str, array : np.array) -> bytes:
    return pack(f"{array.size}{layout}", *array.flatten())

print(texture_paths)
filename = bpy.path.basename(bpy.data.filepath).split(".")[0]
dynamic : bool = armature is not None
with open(f"{filename}.jj", 'wb') as f:
    # write header 
    # version
    f.write(pack("I", 3))
    # number of triangles
    f.write(pack("I", consolidated_mesh.num_tris))
    # number of materials
    f.write(pack("I", len(materials)))
    # number of textures
    f.write(pack("I", len(texture_paths)))
    # whether there are vertex weights
    f.write(pack("?", dynamic))
    f.write(pack("3x")) # floats align to 4 bytes

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
    
    if dynamic:    
        # write vertex groups
        f.write(pack_bytes("I", consolidated_mesh.vert_bone_indices))
        f.write(pack_bytes("f", consolidated_mesh.vert_bone_weights))
        
print(f"{filename}.jj written successfully")
# use this to write "scene.jj" into your working directory
# & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" "C:\Users\eekgasit\Downloads\bedroomv4.blend" --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
# & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" "C:\Users\eekgasit\Downloads\monsterv2.blend" --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\Exporter\exporter.py"
