import bpy
import numpy as np
from struct import pack

# adapted from https://github.com/MomentsInGraphics/vulkan_renderer/blob/dd70c75f38ddb2fd439ad3a337eeff6846f8046f/tools/io_export_vulkan_animated_blender28.py#L473


scene = bpy.data.scenes[0]

AUTHOR_FPS = scene.render.fps
GAME_FPS = 60
armature_obj = bpy.data.objects["Armature"]
active_action = armature_obj.animation_data.action

num_authored_frames = active_action.frame_end - active_action.frame_start
num_frames = int(num_authored_frames)
# num_frames = (num_authored_frames / AUTHOR_FPS) * GAME_FPS

if len(bpy.data.armatures) != 1:
    print("ERROR: the scene must have exactly one armature")
    exit(1)

armature = bpy.data.armatures[0]
bone_names = [bone.name for bone in armature.bones]

bones = armature.bones
pose_bones = armature_obj.pose.bones

MATRIX_ROWS = 4
MATRIX_COLS = 4

# ---------------------------------------------------------------------------------------
# inverse binding matrix
inverse_bind = np.zeros((len(bone_names), MATRIX_ROWS, MATRIX_COLS), dtype = np.float32)

for bone_idx, bone in enumerate(bones):
    inverse_bind[bone_idx, :, :] = np.linalg.inv(bone.matrix_local)[:MATRIX_ROWS, :MATRIX_COLS]

# ---------------------------------------------------------------------------------------
# animation transform matrices
bone_to_armature = np.zeros((num_frames, len(bone_names), MATRIX_ROWS, MATRIX_COLS), dtype = np.float32)

frame_times = np.linspace(active_action.frame_start, active_action.frame_end, num=num_frames, endpoint=True)

initial_frame = scene.frame_current
initial_subframe = scene.frame_subframe

for frame_idx, frame_time in enumerate(frame_times):
    frame = np.floor(frame_time)
    scene.frame_set(frame, subframe=frame_time-frame)
               
    for bone_idx, pose_bone in pose_bones:
        bone_to_armature[frame_idx, bone_idx, :, :] = np.asarray(pose_bone.matrix)[:MATRIX_ROWS, :MATRIX_COLS]

# reset the scene to its original state    
scene.frame_set(initial_frame, subframe=initial_subframe)

# ---------------------------------------------------------------------------------------
# write to file
with open(f"{active_action.name}.janim", "wb") as f:
    f.write(pack("I", num_frames))
    f.write(pack("I", len(bones)))
    f.write(pack(f"{inverse_bind.size()}f", *inverse_bind.flatten()))
    f.write(pack(f"{bone_to_armature.size()}f", *bone_to_armature.flatten()))
print(bone_to_armature)
print(inverse_bind)
