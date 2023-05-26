import bpy
import bmesh
import struct
import os
import mathutils

bl_info = {
    "name": "Stardust model format (.sdm)",
    "author": "Yacker",
    "version": (0, 0),
    "blender": (2, 79, 0),
    "location": "File > Import-Export > Stardust model (.sdm) ",
    "description": "Export stardust",
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
    def __eq__(self, other):
        return self.x == other.x and self.y == other.y and self.z == other.z and self.nx == other.nx and self.ny == other.ny and self.nz == other.nz and self.u == other.u and self.v == other.v and self.rig == other.rig
class submesh:
    def __init__(self):
        self.verts = []
        self.vertsOffset = 0
        self.matOffset = 0
        self.material = None
        self.matString = None
        self.quad = False
        self.strip = False
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
        self.worldPos = mathutils.Vector((0, 0, 0))
        self.rot = mathutils.Quaternion((1.0, 0, 0, 0))
        self.worldRot = mathutils.Quaternion((1.0, 0, 0, 0))
        self.scalex = 0.0
        self.scaley = 0.0
        self.scalez = 0.0
        self.upvec = mathutils.Vector((0, 1.0, 0))
        self.inverseMatrix = None
        self.parent = -1
        self.blenderBone = None
            

def optimize_mesh(mesh, ob):
    retValue = optimized_obj()
    retValue.type = 0
    
    # append all materials
    for material in mesh.materials:
        # TODO: get texture, color, etc
        currSubMesh = submesh()
        currSubMesh.material = material
        currSubMesh.quad = True
        retValue.subMeshes.append(currSubMesh)
        # one more for triangle list
        currSubMesh = submesh()
        currSubMesh.material = material
        currSubMesh.quad = False
        retValue.subMeshes.append(currSubMesh)
        
    for tri in mesh.polygons:
        if (tri.loop_total == 4):
            for loop_ind in range(tri.loop_start, tri.loop_start + tri.loop_total):
                # just add each vertex to a list
                currVert = vertex()
                currBlendVert = mesh.vertices[mesh.loops[loop_ind].vertex_index]
                currVert.x = currBlendVert.co[0]
                currVert.y = currBlendVert.co[1]
                currVert.z = currBlendVert.co[2]
                currVert.nx = mesh.loops[loop_ind].normal[0]
                currVert.ny = mesh.loops[loop_ind].normal[1]
                currVert.nz = mesh.loops[loop_ind].normal[2]
                currVert.internalId = mesh.loops[loop_ind].vertex_index
                # weights
                maxRig = 0
                if (len(currBlendVert.groups) > 0):
                    for group in currBlendVert.groups:
                        if (group.weight > maxRig):
                            currVert.rig = group.group
                            maxRig = group.weight
                # UV
                if (mesh.uv_layers.active != None):
                    currVert.u = mesh.uv_layers.active.data[loop_ind].uv[0]
                    currVert.v = mesh.uv_layers.active.data[loop_ind].uv[1]
                retValue.subMeshes[tri.material_index * 2].verts.append(currVert)
        else:
            for loop_ind in range(tri.loop_start, tri.loop_start + 3):
                # just add each vertex to a list
                currVert = vertex()
                currBlendVert = mesh.vertices[mesh.loops[loop_ind].vertex_index]
                currVert.x = currBlendVert.co[0]
                currVert.y = currBlendVert.co[1]
                currVert.z = currBlendVert.co[2]
                currVert.nx = mesh.loops[loop_ind].normal[0]
                currVert.ny = mesh.loops[loop_ind].normal[1]
                currVert.nz = mesh.loops[loop_ind].normal[2]
                currVert.internalId = mesh.loops[loop_ind].vertex_index
                # weights
                maxRig = 0
                if (len(currBlendVert.groups) > 0):
                    for group in currBlendVert.groups:
                        if (group.weight > maxRig):
                            currVert.rig = group.group
                            maxRig = group.weight
                # UV
                if (mesh.uv_layers.active != None):
                    currVert.u = mesh.uv_layers.active.data[loop_ind].uv[0]
                    currVert.v = mesh.uv_layers.active.data[loop_ind].uv[1]
                retValue.subMeshes[(tri.material_index * 2) + 1].verts.append(currVert)
    
    # optimize uvs
    for subMesh in retValue.subMeshes:
        if (subMesh.material.active_texture != None and hasattr(subMesh.material.active_texture, 'image') and subMesh.material.active_texture.image != None):
            if (subMesh.quad):
                for i in range(0, len(subMesh.verts), 4):
                    # we also invert the y axis here
                    subMesh.verts[i].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i+1].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i+1].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i+2].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i+2].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i+3].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i+3].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i].v
                    subMesh.verts[i+1].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i+1].v
                    subMesh.verts[i+2].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i+2].v
                    subMesh.verts[i+3].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i+3].v
                    # re-center the triangles UVs onto the image
                    for j in range(0, 4):
                        # multiplying by 2 to account for mirrored overflow UVs
                        while (subMesh.verts[i+j].u > subMesh.material.active_texture.image.size[0]*2):
                            for k in range(0, 4):
                                subMesh.verts[i+k].u -= subMesh.material.active_texture.image.size[0]*2
                        while (subMesh.verts[i+j].u < -subMesh.material.active_texture.image.size[0]*2):
                            for k in range(0, 4):
                                subMesh.verts[i+k].u += subMesh.material.active_texture.image.size[0]*2
                        while (subMesh.verts[i+j].v > subMesh.material.active_texture.image.size[1]*2):
                            for k in range(0, 4):
                                subMesh.verts[i+k].v -= subMesh.material.active_texture.image.size[1]*2
                        while (subMesh.verts[i+j].v < -subMesh.material.active_texture.image.size[1]*2):
                            for k in range(0, 4):
                                subMesh.verts[i+k].v += subMesh.material.active_texture.image.size[1]*2
            else:
                for i in range(0, len(subMesh.verts), 3):
                    # we also invert the y axis here
                    subMesh.verts[i].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i+1].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i+1].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i+2].u *= subMesh.material.active_texture.image.size[0]
                    subMesh.verts[i+2].v *= subMesh.material.active_texture.image.size[1]
                    subMesh.verts[i].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i].v
                    subMesh.verts[i+1].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i+1].v
                    subMesh.verts[i+2].v = subMesh.material.active_texture.image.size[1] - subMesh.verts[i+2].v
                    # re-center the triangles UVs onto the image
                    for j in range(0, 3):
                        # multiplying by 2 to account for mirrored overflow UVs
                        while (subMesh.verts[i+j].u > subMesh.material.active_texture.image.size[0]*2):
                            for k in range(0, 3):
                                subMesh.verts[i+k].u -= subMesh.material.active_texture.image.size[0]*2
                        while (subMesh.verts[i+j].u < -subMesh.material.active_texture.image.size[0]*2):
                            for k in range(0, 3):
                                subMesh.verts[i+k].u += subMesh.material.active_texture.image.size[0]*2
                        while (subMesh.verts[i+j].v > subMesh.material.active_texture.image.size[1]*2):
                            for k in range(0, 3):
                                subMesh.verts[i+k].v -= subMesh.material.active_texture.image.size[1]*2
                        while (subMesh.verts[i+j].v < -subMesh.material.active_texture.image.size[1]*2):
                            for k in range(0, 3):
                                subMesh.verts[i+k].v += subMesh.material.active_texture.image.size[1]*2
    
    # generate quad strips as a start
    m = 0
    for subMesh in retValue.subMeshes:
        if (subMesh.quad and subMesh.strip == False):
            i = 0
            while i < len(subMesh.verts):
                if (i >= len(subMesh.verts)):
                    break
                quadVerts = []
                quadVerts.append(subMesh.verts[i])
                quadVerts.append(subMesh.verts[i+1])
                quadVerts.append(subMesh.verts[i+2])
                quadVerts.append(subMesh.verts[i+3])
                foundAnyQuad = False
                quadStripSubmesh = submesh()
                quadStripSubmesh.strip = True
                quadStripSubmesh.quad = True
                quadStripSubmesh.verts.append(quadVerts[0])
                quadStripSubmesh.verts.append(quadVerts[1])
                quadStripSubmesh.verts.append(quadVerts[3])
                quadStripSubmesh.verts.append(quadVerts[2])
                quadStripSubmesh.material = subMesh.material
                k = 0
                while (k < 4 and foundAnyQuad == False):
                    foundQuad = True
                    while (foundQuad):
                        foundQuad = False
                        j = i + 4
                        while (j < len(subMesh.verts)):
                            if (subMesh.verts[j] == quadVerts[3] and subMesh.verts[j + 1] == quadVerts[2]):
                                quadStripSubmesh.verts.append(subMesh.verts[j + 3])
                                quadStripSubmesh.verts.append(subMesh.verts[j + 2])
                                quadVerts[0] = quadVerts[3]
                                quadVerts[1] = quadVerts[2]
                                quadVerts[2] = subMesh.verts[j + 2]
                                quadVerts[3] = subMesh.verts[j + 3]
                                # pop out the 4 verts we just added
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                # re-iterate over all the polygons to find any that connect to the new quad
                                foundQuad = True
                                foundAnyQuad = True
                                break
                            # sigh
                            if (subMesh.verts[j+1] == quadVerts[3] and subMesh.verts[j+2] == quadVerts[2]):
                                quadStripSubmesh.verts.append(subMesh.verts[j])
                                quadStripSubmesh.verts.append(subMesh.verts[j + 3])
                                quadVerts[0] = quadVerts[3]
                                quadVerts[1] = quadVerts[2]
                                quadVerts[2] = subMesh.verts[j + 3]
                                quadVerts[3] = subMesh.verts[j]
                                # pop out the 4 verts we just added
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundQuad = True
                                foundAnyQuad = True
                                break
                            if (subMesh.verts[j+2] == quadVerts[3] and subMesh.verts[j+3] == quadVerts[2]):
                                quadStripSubmesh.verts.append(subMesh.verts[j + 1])
                                quadStripSubmesh.verts.append(subMesh.verts[j])
                                quadVerts[0] = quadVerts[3]
                                quadVerts[1] = quadVerts[2]
                                quadVerts[2] = subMesh.verts[j]
                                quadVerts[3] = subMesh.verts[j + 1]
                                # pop out the 4 verts we just added
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundQuad = True
                                foundAnyQuad = True
                                break
                            if (subMesh.verts[j+3] == quadVerts[3] and subMesh.verts[j] == quadVerts[2]):
                                quadStripSubmesh.verts.append(subMesh.verts[j + 2])
                                quadStripSubmesh.verts.append(subMesh.verts[j + 1])
                                quadVerts[0] = quadVerts[3]
                                quadVerts[1] = quadVerts[2]
                                quadVerts[2] = subMesh.verts[j + 1]
                                quadVerts[3] = subMesh.verts[j + 2]
                                # pop out the 4 verts we just added
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundQuad = True
                                foundAnyQuad = True
                                break
                            j = j + 4
                    k = k + 1
                    if (foundAnyQuad == False):
                        # push them all up one
                        tmpQuadVert = quadVerts[3]
                        quadVerts[3] = quadVerts[2]
                        quadVerts[2] = quadVerts[1]
                        quadVerts[1] = quadVerts[0]
                        quadVerts[0] = tmpQuadVert
                        # fix order in the submesh
                        quadStripSubmesh.verts[0] = quadVerts[0]
                        quadStripSubmesh.verts[1] = quadVerts[1]
                        quadStripSubmesh.verts[2] = quadVerts[3]
                        quadStripSubmesh.verts[3] = quadVerts[2]
                    
                if (foundAnyQuad):
                    # and pop out the 4 verts we started with since they're going into the quad strip array
                    subMesh.verts.pop(i)
                    subMesh.verts.pop(i)
                    subMesh.verts.pop(i)
                    subMesh.verts.pop(i)
                    # finally store the quad strip sub mesh
                    retValue.subMeshes.insert(m + 1, quadStripSubmesh)
                    i -= 4
                if (i >= len(subMesh.verts)):
                    break
                i = i + 4
        
        if (subMesh.quad == False and subMesh.strip == False):
            i = 0
            while (i < len(subMesh.verts)):
                triVerts = []
                triVerts.append(subMesh.verts[i])
                triVerts.append(subMesh.verts[i+1])
                triVerts.append(subMesh.verts[i+2])
                foundAnyTri = False
                triStripSubmesh = submesh()
                triStripSubmesh.strip = True
                triStripSubmesh.verts.append(triVerts[0])
                triStripSubmesh.verts.append(triVerts[1])
                triStripSubmesh.verts.append(triVerts[2])
                triStripSubmesh.material = subMesh.material
                triId = 1
                k = 0
                while (k < 4 and foundAnyTri == False):
                    foundTri = True
                    while (foundTri):
                        foundTri = False
                        j = i + 3
                        while (j < len(subMesh.verts)):
                            if (subMesh.verts[j] == triVerts[2] and subMesh.verts[j + 1] == triVerts[1]):
                                triStripSubmesh.verts.append(subMesh.verts[j + 2])
                                if (triId % 2 == 0):
                                    triVerts[0] = triVerts[1]
                                    triVerts[1] = triVerts[2]
                                    triVerts[2] = subMesh.verts[j + 2]
                                else:
                                    triVerts[0] = triVerts[1]
                                    triVerts[2] = triVerts[2]
                                    triVerts[1] = subMesh.verts[j + 2]
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundTri = True
                                foundAnyTri = True
                                triId = triId + 1
                                break
                            if (subMesh.verts[j+1] == triVerts[2] and subMesh.verts[j + 2] == triVerts[1]):
                                triStripSubmesh.verts.append(subMesh.verts[j])
                                if (triId % 2 == 0):
                                    triVerts[0] = triVerts[1]
                                    triVerts[1] = triVerts[2]
                                    triVerts[2] = subMesh.verts[j]
                                else:
                                    triVerts[0] = triVerts[1]
                                    triVerts[2] = triVerts[2]
                                    triVerts[1] = subMesh.verts[j]
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundTri = True
                                foundAnyTri = True
                                triId = triId + 1
                                break
                            if (subMesh.verts[j+2] == triVerts[2] and subMesh.verts[j] == triVerts[1]):
                                triStripSubmesh.verts.append(subMesh.verts[j + 1])
                                if (triId % 2 == 0):
                                    triVerts[0] = triVerts[1]
                                    triVerts[1] = triVerts[2]
                                    triVerts[2] = subMesh.verts[j + 1]
                                else:
                                    triVerts[0] = triVerts[1]
                                    triVerts[2] = triVerts[2]
                                    triVerts[1] = subMesh.verts[j + 1]
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                subMesh.verts.pop(j)
                                foundTri = True
                                foundAnyTri = True
                                triId = triId + 1
                                break
                            j = j + 3
                        
                    k = k + 1
                    if (foundAnyTri == False):
                        # push them all up one
                        tmpTriVert = triVerts[2]
                        triVerts[2] = triVerts[1]
                        triVerts[1] = triVerts[0]
                        triVerts[0] = tmpTriVert
                        # fix submesh order
                        triStripSubmesh.verts[0] = triVerts[0]
                        triStripSubmesh.verts[1] = triVerts[1]
                        triStripSubmesh.verts[2] = triVerts[2]
                if (foundAnyTri):
                    # pop out the 3 verts we started with
                    subMesh.verts.pop(i)
                    subMesh.verts.pop(i)
                    subMesh.verts.pop(i)
                    # finally store the tri strip sub mesh
                    retValue.subMeshes.insert(m + 1, triStripSubmesh)
                    i -= 3
                i = i + 3
        m = m + 1
    
    # generic object transforms
    retValue.parent = ob.parent
    retValue.blenderObj = ob

    return retValue

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
    if (intvalue > 32767):
        intvalue = 32767
    if (intvalue < -32768):
        intvalue = -32768
    file.write(struct.pack("<1h", int(intvalue)))

def write_short(file, value):
    file.write(struct.pack("<1h", value))
    
def write_byte(file, value):
    file.write(struct.pack("<1B", value))

def write_vertex_data(obj, f):
    # write position data first
    currMat = 0
    for submesh in obj.subMeshes:
        write_short(f, len(submesh.verts))
        if (submesh.strip):
            if (submesh.quad):
                write_byte(f, 6)
            else:
                write_byte(f, 2)
        else:
            if (submesh.quad):
                write_byte(f, 5)
            else:
                write_byte(f, 1)
        if (submesh.strip == False):
            write_byte(f, currMat)
            currMat += 1
        else:
            write_byte(f, currMat - 1)
        for vert in submesh.verts:
            write_vertex(f, vert.x)
            write_vertex(f, vert.y)
            write_vertex(f, vert.z)
            # bone
            write_byte(f, vert.rig)
            # padding
            write_byte(f, 0)
            # normal
            write_int(f, (int(vert.nx * 0x1FF) & 0x3FF) | ((int(vert.ny * 0x1FF) & 0x3FF) << 10) | ((int(vert.nz * 0x1FF) & 0x3FF) << 20))
            # uv
            write_uv(f, vert.u)
            write_uv(f, vert.v)

def reorder_bones(optObj, blendObj, skeleton, scale, avgx, avgy, avgz):
    optSkeleton = []
    for bone in skeleton.data.bones:
        if (bone.use_deform):
            optBone = optimized_bone()
            optBone.blenderBone = bone
            optSkeleton.append(optBone)
    i2 = 0
    for bone in optSkeleton:
        i = 0
        for parentBone in optSkeleton:
            if parentBone.blenderBone == bone.blenderBone.parent:
                bone.parent = i
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
        # inverse matrix
        #tmpRot = bone.worldRot#.inverted()
        #rotMtx = tmpRot.to_matrix()
        #rotMtx = rotMtx.to_4x4()
        #rotMtx[0].w = (bone.worldPos.x - avgx) / scale
        #rotMtx[1].w = (bone.worldPos.y - avgy) / scale
        #rotMtx[2].w = (bone.worldPos.z - avgz) / scale
        #bone.inverseMatrix = rotMtx.inverted()
        #bone.inverseMatrix = rotMtx * mathutils.Matrix.Translation(mathutils.Vector((-(bone.worldPos.x - avgx) / scale, -(bone.worldPos.y - avgy) / scale, -(bone.worldPos.z - avgz) / scale)))
        
        # re-order the rigs to match what we've generated
        for submesh in optObj.subMeshes:
            for vert in submesh.verts:
                if blendObj.vertex_groups[vert.rig].name == bone.blenderBone.name:
                    vert.rig = i2
        
        i2 += 1
    return optSkeleton
    
def write_skeleton_data(optSkel, f):
    for bone in optSkel:
        # write inverse matrix; we set this up earlier along with everything else
        for i in bone.inverseMatrix:
            write_float(f, i.x)
            write_float(f, i.y)
            write_float(f, i.z)
            write_float(f, i.w)
        write_float(f, bone.x)
        write_float(f, bone.y)
        write_float(f, bone.z)
        write_float(f, bone.scalex)
        write_float(f, bone.scaley)
        write_float(f, bone.scalez)
        write_float(f, bone.rot.x)
        write_float(f, bone.rot.y)
        write_float(f, bone.rot.z)
        write_float(f, bone.rot.w)
        write_int(f, bone.parent)

def write_material_data(optObj, f, backfaceCulling):
    # just write default data for now
    for submesh in optObj.subMeshes:
        if (submesh.strip):
            continue
        write_int(f, int(pow(submesh.material.diffuse_color[0], 1/2.2) * 255) >> 3)
        write_int(f, int(pow(submesh.material.diffuse_color[1], 1/2.2) * 255) >> 3)
        write_int(f, int(pow(submesh.material.diffuse_color[2], 1/2.2) * 255) >> 3)
        write_int(f, int(submesh.material.alpha * 31))
        # texture; dummy 0
        write_int(f, 0)
        # backface culling
        if (backfaceCulling):
            write_byte(f, 1)
        else:
            write_byte(f, 0)
        # lit
        if (submesh.material.use_shadeless):
            write_byte(f, 0)
        else:
            write_byte(f, 1)
        # specular amount
        write_byte(f, int(submesh.material.specular_intensity * 255));
        # padding
        write_byte(f, 0)
        # texture offset and scale, for now 0 and 1
        write_float(f, 0)
        write_float(f, 0)
        write_float(f, 1)
        write_float(f, 1)
        # emissive color
        # todo: put something here?
        write_float(f, 0)
        write_float(f, 0)
        write_float(f, 0)
        

def write_some_data(context, filepath, auto_transform, backfaceCulling, polygons_type):
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
    selObj = bpy.context.selected_objects[0]
    modifierToRemove = newObj.modifiers.get("Armature")
    if (modifierToRemove != None):
        newObj.modifiers.remove(modifierToRemove)
    if (polygons_type == 'Quads'):
        bpy.context.scene.objects.link(newObj)
        # deselect all objects
        for o in bpy.context.scene.objects:
            o.select = False
        newObj.select = True
        bpy.context.scene.objects.active = newObj
        bpy.ops.object.mode_set(mode = 'EDIT')
        # select all n-gons
        bpy.ops.mesh.select_face_by_sides(number=4, type='GREATER', extend=False)
        # triangulate them
        bpy.ops.mesh.quads_convert_to_tris()
        bpy.ops.object.mode_set(mode = 'OBJECT')
    mesh = newObj.to_mesh(scene=bpy.context.scene, apply_modifiers=True, settings='PREVIEW')
    if (polygons_type == 'Triangles'):
        bm = bmesh.new()
        bm.from_mesh(mesh)
        # ?? python syntax is fucked
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.to_mesh(mesh)
        bm.free()
    if (polygons_type == 'Quads'):
        bpy.ops.object.delete()
    mesh.calc_normals_split()
    
    boundsx = 0
    boundsy = 0
    boundsz = 0
    avgvertx = 0
    avgverty = 0
    avgvertz = 0
    modelBoundsx = 0
    modelBoundsy = 0
    modelBoundsz = 0
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
        modelBoundsx = max(abs(vert.co[0] - avgvertx), modelBoundsx)
        modelBoundsy = max(abs(vert.co[1] - avgverty), modelBoundsy)
        modelBoundsz = max(abs(vert.co[2] - avgvertz), modelBoundsz)
    
    if auto_transform:
        maxbounds = max(boundsx, max(boundsy, boundsz))
        maxbounds /= (32767.0 / 4096.0)
        maxbounds = max(maxbounds, 1.0)
        for vert in mesh.vertices:
            vert.co[0] /= maxbounds
            vert.co[1] /= maxbounds
            vert.co[2] /= maxbounds
        boundsx /= maxbounds
        boundsy /= maxbounds
        boundsz /= maxbounds
        scale = maxbounds
        
    # generate bounds more properly
    boundsMinx = 10000000
    boundsMaxx = -10000000
    boundsMiny = 10000000
    boundsMaxy = -10000000
    boundsMinz = 10000000
    boundsMaxz = -10000000
    for vert in mesh.vertices:
        boundsMinx = min(boundsMinx, vert.co[0])
        boundsMiny = min(boundsMiny, vert.co[1])
        boundsMinz = min(boundsMinz, vert.co[2])
        boundsMaxx = max(boundsMaxx, vert.co[0])
        boundsMaxy = max(boundsMaxy, vert.co[1])
        boundsMaxz = max(boundsMaxz, vert.co[2])
    
    
    optObj = optimize_mesh(mesh, selObj)
    bpy.data.meshes.remove(mesh)
    
    skeleton = None;
    for modifier in selObj.modifiers:
        if modifier.type == 'ARMATURE':
            skeleton = modifier.object
    
    optSkeleton = None
    if skeleton != None:
        optSkeleton = reorder_bones(optObj, selObj, skeleton, scale, offsx, offsy, offsz)
    
    # get base offset for object data...
    baseOffs = 0x4C
    
    # write the object header
    write_int(f, len(optObj.subMeshes))
    write_int(f, baseOffs)
    for submesh in optObj.subMeshes:
        baseOffs += 4
        baseOffs += 16 * len(submesh.verts)
        # materials
    optObjCount = 0
    for submesh in optObj.subMeshes:
        if submesh.strip == False:
            optObjCount = optObjCount + 1
    write_int(f, optObjCount)
    write_int(f, baseOffs)
    baseOffs += 0x34 * optObjCount
    # setup BONE ZONE
    skeletonOffs = baseOffs
    if (optSkeleton != None):
        baseOffs += len(optSkeleton) * 0x6C
    
    # names
    write_int(f, baseOffs)
    # uh oh gotta get texture names
    for submesh in optObj.subMeshes:
        if (submesh.strip == False):
            if (submesh.material.active_texture != None and hasattr(submesh.material.active_texture, 'image') and submesh.material.active_texture.image != None):
                baseOffs += 1 + len(submesh.material.active_texture.image.name)
            else:
                baseOffs += 1 + len("white.sdi")
    # skeleton now...
    if (optSkeleton != None):
        write_int(f, len(optSkeleton))
    else:
        write_int(f, 0)
    write_int(f, skeletonOffs)
    # default offset, scale
    write_float(f, offsx * selObj.scale.x)
    write_float(f, offsy * selObj.scale.x)
    write_float(f, offsz * selObj.scale.x)
    write_float(f, scale * selObj.scale.x)
    
    # bounds
    write_float(f, boundsMinx)
    write_float(f, boundsMiny)
    write_float(f, boundsMinz)
    write_float(f, boundsMaxx)
    write_float(f, boundsMaxy)
    write_float(f, boundsMaxz)
    
    # 0x4: native model ptr
    write_int(f, 0)
    
    # write data now
    write_vertex_data(optObj, f)
    
    # materials
    write_material_data(optObj, f, backfaceCulling)
    
    # skeleton
    if (optSkeleton != None):
        write_skeleton_data(optSkeleton, f)
    
    # texture names
    for submesh in optObj.subMeshes:
        if (submesh.strip == False):
            if (submesh.material.active_texture != None and hasattr(submesh.material.active_texture, 'image') and submesh.material.active_texture.image != None):
                f.write(submesh.material.active_texture.image.name[0:len(submesh.material.active_texture.image.name)-3].encode('ascii'))
                f.write("sdi".encode('ascii'))
            else:
                f.write("white.sdi".encode('ascii'))
            write_byte(f, 0)
    
    # todo: write bounding box data
    
    f.close()
    return {'FINISHED'}


# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator


class ExportSomeData(Operator, ExportHelper):
    """Exports a model to .SDM format for use with Stardust"""
    bl_idname = "stardust_export.export_model"  # important since its how bpy.ops.import_test.some_data is constructed
    bl_label = "Export"

    # ExportHelper mixin class uses this
    filename_ext = ".sdm"

    filter_glob = StringProperty(
        default="*.sdm",
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
    
    backface_culling = BoolProperty(
        name="Backface Culling",
        description="Toggles backface culling for this mesh",
        default=True,
    )

    polygons_type = EnumProperty(
        name="Polygon export type",
        description="Quads are more efficient to render. May also result in strange deformation for rigged models.",
        items=(
            ('Triangles', "Triangles", "Export whole model as triangles"),
            ('Quads', "Quads", "Export triangles & Quads"),
        ),
        default='Quads',
    )

    def execute(self, context):
        return write_some_data(context, self.filepath, self.auto_transform, self.backface_culling, self.polygons_type)


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ExportSomeData.bl_idname, text="Stardust model export (.sdm)")
    


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
    bpy.ops.stardust_export.export_model('INVOKE_DEFAULT')