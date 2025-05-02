import bpy
import mathutils

scene = bpy.data.scenes[0]

cube = scene.objects.get("Suzanne")
cubemesh = cube.data
verts = cubemesh.vertices
scenevertices = []
for tri in cubemesh.loop_triangles:
    for vidx in tri.vertices:
        vertex = verts[vidx]
        position_local = mathutils.Vector((vertex.co.x, vertex.co.y, vertex.co.z, 1)) 
        position_global =  cube.matrix_world @ position_local
        scenevertices += [position_global.x, position_global.y, position_global.z]
from array import array
output_file = open('scene.jj', 'wb')
float_array = array('d', scenevertices)
float_array.tofile(output_file)
output_file.close()

print(scenevertices)

# "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe"
#  & "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe" suzanne.blend --background --python "C:\Users\eekgasit\source\repos\ucsd-cse125-sp25\group4\ClientApp\exporter.py"^C
