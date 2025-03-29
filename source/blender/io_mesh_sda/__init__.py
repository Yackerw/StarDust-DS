import bpy
import bmesh
import struct
import os
import mathutils

bl_info = {
    "name": "Stardust animation format (.sda)",
    "author": "Yacker",
    "version": (0, 0),
    "blender": (2, 79, 0),
    "location": "File > Import-Export > Stardust animation (.sda) ",
    "description": "Export stardust animation",
    "warning": "",
    "category": "Import-Export",
}

class vertex:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.nx = 0.0
        self.ny = 0.0
        self.nz = 0.0
        self.u = 0.0
        self.v = 0.0
        self.rig = 0
class submesh:
    def __init__(self):
        self.verts = []
        self.vertsOffset = 0
        self.matOffset = 0
        self.material = None
        self.matString = None
class optimized_obj:
    def __init__(self):
        self.type = -1
        self.subMeshes = []
        self.bones = []
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.scalex = 0.0
        self.scaley = 0.0
        self.scalez = 0.0
        self.blenderObj = None
class optimized_bone:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.rot = mathutils.Quaternion((0, 0, 0, 1.0))
        self.worldRot = mathutils.Quaternion((0, 0, 0, 1.0))
        self.scalex = 0.0
        self.scaley = 0.0
        self.scalez = 0.0
        self.upvec = mathutils.Vector((0, 1.0, 0))
        self.inverseMatrix = None
        self.parent = -1
        self.blenderBone = None
class keyframe:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.z = 0
        self.w = 0
        self.frame = 0
class keyframe_set:
    def __init__(self):
        self.target = 0
        self.keyType = 0
        self.keyFrames = []

def write_int(file, value):
    # little endian
    file.write(struct.pack("<1i", value))

def write_float(file, value):
    # little endian
    intvalue = value * 4096
    file.write(struct.pack("<1i", int(intvalue)))

def write_vertex(file, value):
    intvalue = value * 4096
    file.write(struct.pack("<1h", int(intvalue)))

def write_uv(file, value):
    intvalue = value * 16
    file.write(struct.pack("<1h", int(intvalue)))

def write_short(file, value):
    file.write(struct.pack("<1h", value))
    
def write_byte(file, value):
    file.write(struct.pack("<1b", value))

def reorder_bones(blendObj, skeleton, scale, avgx, avgy, avgz):
    optSkeleton = []
    for bone in skeleton.data.bones:
        if (bone.use_deform):
            # ignore bones that aren't actually a part of vertex groups
            isInGroups = False
            for x in blendObj.vertex_groups:
                if (x.name == bone.name):
                    isInGroups = True
                    break
            if (isInGroups == False):
                continue
            optBone = optimized_bone()
            optBone.blenderBone = bone
            optSkeleton.append(optBone)
    i2 = 0
    for bone in optSkeleton:
        i = 0
        for parentBone in optSkeleton:
            if parentBone.blenderBone == bone.blenderBone.parent:
                bone.parent = i
                print(i)
            i += 1
        if (bone.blenderBone.parent != None):
            tmpWorldPos = bone.blenderBone.head_local#bone.blenderBone.head + optSkeleton[bone.parent].worldPos + (bone.blenderBone.parent.tail - bone.blenderBone.parent.head)
            bone.worldPos = tmpWorldPos
            #bone.x = ((bone.blenderBone.head.x - avgx) - (bone.blenderBone.parent.head.x - avgx))
            #bone.y = ((bone.blenderBone.head.y - avgy) - (bone.blenderBone.parent.head.y - avgy))
            #bone.z = ((bone.blenderBone.head.z - avgz) - (bone.blenderBone.parent.head.z - avgz))
            bone.x = (tmpWorldPos.x - (optSkeleton[bone.parent].worldPos.x - avgx)) - avgx
            bone.y = (tmpWorldPos.y - (optSkeleton[bone.parent].worldPos.y - avgy)) - avgy
            bone.z = (tmpWorldPos.z - (optSkeleton[bone.parent].worldPos.z - avgz)) - avgz
            # convert to local space
            tmpVec = optSkeleton[bone.parent].worldRot.inverted() * mathutils.Vector((bone.x, bone.y, bone.z))
            bone.x = tmpVec[0]
            bone.y = tmpVec[1]
            bone.z = tmpVec[2]
            bone.x /= scale
            bone.y /= scale
            bone.z /= scale
        else:
            bone.x = (bone.blenderBone.head.x - avgx) / scale
            bone.y = (bone.blenderBone.head.y - avgy) / scale
            bone.z = (bone.blenderBone.head.z - avgz) / scale
            bone.worldPos = bone.blenderBone.head_local
        #bone.x /= scale
        #bone.y /= scale
        #bone.z /= scale
        # ?? just assume 1 i guess
        bone.scalex = 1.0
        bone.scaley = 1.0
        bone.scalez = 1.0
        # once again, blender has no default rotation. so we wanna generate our own
        bone.upvec = mathutils.Vector((bone.blenderBone.tail.x, bone.blenderBone.tail.y, bone.blenderBone.tail.z)) - mathutils.Vector((bone.blenderBone.head.x, bone.blenderBone.head.y, bone.blenderBone.head.z))
        bone.upvec = bone.upvec.normalized()
        tmpMtx2 = None
        if (bone.parent != -1):
            bone.rot = bone.blenderBone.parent.matrix_local.to_quaternion().inverted() * bone.blenderBone.matrix_local.to_quaternion()
            bone.worldRot = bone.blenderBone.matrix_local.to_quaternion()
            tmpMtx = optSkeleton[bone.parent].inverseMatrix.inverted()
            tmpMtx2 = bone.rot.to_matrix()
            tmpMtx2 = tmpMtx2.to_4x4()
            tmpMtx2[0].w = bone.x
            tmpMtx2[1].w = bone.y
            tmpMtx2[2].w = bone.z
            tmpMtx2 = tmpMtx * tmpMtx2
            bone.inverseMatrix = tmpMtx2.inverted()
            #tmpMtx2 = bone.worldRot.to_matrix()
            #tmpMtx2 = tmpMtx2.to_4x4()
            #tmpMtx2 = mathutils.Matrix.Translation(mathutils.Vector(((bone.worldPos.x - avgx) / scale, (bone.worldPos.y - avgy) / scale, (bone.worldPos.z - avgz) / scale))) * tmpMtx2
            #print(tmpMtx * mathutils.Vector((bone.x, bone.y, bone.z)))
        else:
            bone.rot = bone.blenderBone.matrix.to_quaternion()
            bone.worldRot = bone.rot
            tmpRot = bone.worldRot#.inverted()
            rotMtx = tmpRot.to_matrix()
            rotMtx = rotMtx.to_4x4()
            rotMtx[0].w = bone.x
            rotMtx[1].w = bone.y
            rotMtx[2].w = bone.z
            bone.inverseMatrix = rotMtx.inverted()        
    return optSkeleton

def convert_animation(optSkel, skel, scale, avgx, avgy, avgz, frameSkip):
    sce = bpy.context.scene
    # initially set up the key frame sets; one for position, scale, and rotation for each bone
    keyframeSets = []
    for i in range(0, len(optSkel)):
        newSet = keyframe_set()
        newSet.target = i
        newSet.keyType = 0
        keyframeSets.append(newSet)
        newSet = keyframe_set()
        newSet.target = i
        newSet.keyType = 1
        keyframeSets.append(newSet)
        newSet = keyframe_set()
        newSet.target = i
        newSet.keyType = 2
        keyframeSets.append(newSet)
    
    f = sce.frame_start
    while f < sce.frame_end + 1:
        if (f % frameSkip == 0 or f == sce.frame_end or f == sce.frame_start):
            sce.frame_set(f)
            for bone in skel.pose.bones:
                if bone.bone.use_deform:
                    i = 0
                    blendBone = bone.bone
                    while (blendBone != optSkel[i].blenderBone):
                        i += 1
                    optBone = optSkel[i]
                    # generate rotation key frame
                    rotkey = keyframe()
                    rotkey.frame = f - sce.frame_start
                    newQuat = None
                    if (bone.parent != None):
                        newQuat = (bone.parent.matrix.to_quaternion().inverted() * bone.matrix.to_quaternion())#bone.rotation_quaternion
                    else:
                        newQuat = bone.matrix.to_quaternion()
                    rotkey.x = newQuat.x
                    rotkey.y = newQuat.y
                    rotkey.z = newQuat.z
                    rotkey.w = newQuat.w
                    keyframeSets[i * 3].keyFrames.append(rotkey)
                    # position
                    transKey = keyframe()
                    transKey.frame = f - sce.frame_start
                    # we're just gonna do this
                    tmpVec = (optBone.worldRot * (bone.location / scale)) + mathutils.Vector((optBone.x, optBone.y, optBone.z))
                    transKey.x = tmpVec.x
                    transKey.y = tmpVec.y
                    transKey.z = tmpVec.z
                    keyframeSets[(i * 3) + 1].keyFrames.append(transKey)
                    # scale
                    scaleKey = keyframe()
                    scaleKey.frame = f - sce.frame_start
                    scaleKey.x = bone.scale[0]
                    scaleKey.y = bone.scale[1]
                    scaleKey.z = bone.scale[2]
                    keyframeSets[(i * 3) + 2].keyFrames.append(scaleKey)
        f += 1
    return keyframeSets

def write_some_data(context, filepath, auto_transform, keyFrameCount, encodeRot, encodePos, encodeScale):
    print("running write_some_data...")
    f = open(filepath, 'wb')
    # version
    write_int(f, 0)
    
    # get all objects
    objectCount = 0
    
    if (bpy.context.selected_objects[0].type != 'MESH'):
        f.close()
        return
    # sigh, copy the object and mesh so we can apply any modifiers BUT armature
    newObj = bpy.context.selected_objects[0].copy()
    newObj.data = bpy.context.selected_objects[0].data.copy()
    modifierToRemove = newObj.modifiers.get("Armature")
    if (modifierToRemove != None):
        newObj.modifiers.remove(modifierToRemove)
    mesh = newObj.to_mesh(scene=bpy.context.scene, apply_modifiers=True, settings='PREVIEW')
    bm = bmesh.new()
    bm.from_mesh(mesh)
    # ?? python syntax is fucked
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(mesh)
    bm.free()
    mesh.calc_normals_split()
    
    boundsx = 0
    boundsy = 0
    boundsz = 0
    avgvertx = 0
    avgverty = 0
    avgvertz = 0
    offsx = 0
    offsy = 0
    offsz = 0
    scale = 1
    
    # get average vertex (note: should probably be inside if auto_transform: but idk right now)
    # maybe todo if anyone wants to at some point; change it to use the extremes rather than the average to produce better results
    for vert in mesh.vertices:
        avgvertx += vert.co[0]
        avgverty += vert.co[1]
        avgvertz += vert.co[2]
    avgvertx /= len(mesh.vertices)
    avgverty /= len(mesh.vertices)
    avgvertz /= len(mesh.vertices)
    
    # translate to center the verts
    if auto_transform:
        for vert in mesh.vertices:
            vert.co[0] -= avgvertx
            vert.co[1] -= avgverty
            vert.co[2] -= avgvertz
        offsx = avgvertx
        offsy = avgverty
        offsz = avgvertz
        avgvertx = 0
        avgverty = 0
        avgvertz = 0
    
    # generate bounding box
    # definite todo later: make this more accurate
    for vert in mesh.vertices:
        boundsx = max(abs(vert.co[0]), boundsx)
        boundsy = max(abs(vert.co[1]), boundsy)
        boundsz = max(abs(vert.co[2]), boundsz)
    
    maxbounds = 1.0
    
    if auto_transform:
        maxbounds = max(boundsx, max(boundsy, boundsz))
        maxbounds /= (32767.0 / 4096.0)
        maxbounds = max(maxbounds, 1.0)
    
    skeleton = None;
    for modifier in bpy.context.selected_objects[0].modifiers:
        if modifier.type == 'ARMATURE':
            skeleton = modifier.object
            
    optSkeleton = None
    if skeleton != None:
        optSkeleton = reorder_bones(bpy.context.selected_objects[0], skeleton, maxbounds, offsx, offsy, offsz)
    else:
        f.close()
        return {'NO ARMATURE FOUND'}
    
    # convert to a more convenient format
    frameRate = 1
    if (keyFrameCount == '30'):
        frameRate = 2
    if (keyFrameCount == '15'):
        frameRate = 4
    keyframeSets = convert_animation(optSkeleton, skeleton, maxbounds, offsx, offsy, offsz, frameRate)
    
    # remove any we're not supposed to encode
    i = 0
    while i < len(keyframeSets):
        if (encodeRot == False and i >= 0 and keyframeSets[i].keyType == 0):
            keyframeSets.pop(i)
            i -= 1
        if (encodePos == False and i >= 0 and keyframeSets[i].keyType == 1):
            keyframeSets.pop(i)
            i -= 1
        if (encodeScale == False and i >= 0 and keyframeSets[i].keyType == 2):
            keyframeSets.pop(i)
            i -= 1
        i += 1
    
    # write the key frames now
    baseOffs = 0xC
    write_int(f, len(keyframeSets))
    write_int(f, (bpy.context.scene.frame_end - 1) * 4096)
    # initial offset
    baseOffs += 4 * len(keyframeSets)
    for keySet in keyframeSets:
        write_int(f, baseOffs)
        baseOffs += 0xC + (0x14 * len(keySet.keyFrames))
    # write the sets now
    for keySet in keyframeSets:
        write_int(f, keySet.target)
        write_int(f, keySet.keyType)
        write_int(f, len(keySet.keyFrames))
        for keyframe in keySet.keyFrames:
            write_int(f, keyframe.frame * 4096)
            write_float(f, keyframe.x)
            write_float(f, keyframe.y)
            write_float(f, keyframe.z)
            write_float(f, keyframe.w)
    
    return {'FINISHED'}


# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator


class ExportSomeData(Operator, ExportHelper):
    """Exports a model to .SDA format for use with Stardust"""
    bl_idname = "stardust_export.export_anim"  # important since its how bpy.ops.import_test.some_data is constructed
    bl_label = "Export"

    # ExportHelper mixin class uses this
    filename_ext = ".sda"

    filter_glob = StringProperty(
        default="*.sda",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    auto_transform = BoolProperty(
        name="Auto transform",
        description="Automatically transforms the mesh to be optimized for the DS. Disable for rigged meshes or meshes where you wish to preserve the origin",
        default=True,
    )
    
    encode_rotation = BoolProperty(
        name="Encode rotation",
        description="Encodes rotation in the animation",
        default=True,
    )
    
    encode_position = BoolProperty(
        name="Encode position",
        description="Encodes position in the animation",
        default=True,
    )
    
    encode_scale = BoolProperty(
        name="Encode scale",
        description="Encodes scale in the animation",
        default=True,
    )

    keyFramesPerSecond = EnumProperty(
        name="Keyframes per second",
        description="Choose how many key frames to encode per second",
        items=(
            ('15', "15", "15 keyframes"),
            ('30', "30", "30 keyframes"),
            ('60', "60", "60 keyframes"),
        ),
        default='15',
    )

    def execute(self, context):
        return write_some_data(context, self.filepath, self.auto_transform, self.keyFramesPerSecond, self.encode_rotation, self.encode_position, self.encode_scale)


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ExportSomeData.bl_idname, text="Stardust anim export (.sda)")


# Register and add to the "file selector" menu (required to use F3 search "Text Export Operator" for quick access).
def register():
    bpy.utils.register_class(ExportSomeData)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportSomeData)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    bpy.ops.stardust_export.export_anim('INVOKE_DEFAULT')