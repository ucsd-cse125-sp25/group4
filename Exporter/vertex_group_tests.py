import bpy
import numpy as np

armature = bpy.data.armatures[0]
bone_name_to_index = {bone.name: i for i, bone in enumerate(armature.bones)}
print("armature bones:")
print(bone_name_to_index)

max_bone_index_length = 0

for obj in bpy.data.objects:
    if obj.type != "MESH":
        continue
    mesh = obj.data

    bone_indices = np.zeros((len(mesh.vertices), 4), dtype=np.int32)
    bone_weights = np.zeros((len(mesh.vertices), 4), dtype=np.float32)
    
    if obj.parent_type == "BONE":
        bone_indices[:, 0] = bone_name_to_index[obj.parent_bone]
        bone_weights[:, 0] = 1

    if obj.find_armature() is not None:
        
        # map object vertex groups to bone indices
        vertex_group_names = [group.name for group in obj.vertex_groups]
        vertex_group_to_bone = np.array([bone_name_to_index[vertex_group_name] for vertex_group_name in vertex_group_names], dtype=np.int32)
        
        for i, vertex in enumerate(mesh.vertices):
            max_bone_index_length = max(max_bone_index_length, len(vertex.groups))

            num_written = min(len(vertex.groups), 4)
            vertex_groups = np.zeros(len(vertex.groups), dtype=np.int32)
            vertex_weights = np.zeros(len(vertex.groups), dtype=np.float32)
            vertex.groups.foreach_get("group", vertex_groups)
            vertex.groups.foreach_get("weight", vertex_weights)

            if len(vertex.groups) > 4:
                top_selection = np.argpartition(vertex_weights, -4)[-4:]
                vertex_weights = vertex_weights[top_selection]
                vertex_groups = vertex_groups[top_selection]

            vertex_weights /= np.linalg.norm(vertex_weights)

            bone_indices[i, :num_written] = vertex_group_to_bone[vertex_groups]
            assert(all(vertex_group_to_bone[vertex_groups] < len(bone_name_to_index)))
            bone_weights[i, :num_written] = vertex_weights

            # print(vertex_groups)
            # print(vertex_weights)

print(obj.name)
print(max_bone_index_length)
