#version 330 core


layout(location = 0) in vec3 iVert;
layout(location = 1) in vec2 iUV;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in ivec4 boneIndices;
layout(location = 4) in vec4 boneWeights;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 UVMatrix;

#include rigging.glh

uniform sampler2D mainTexture;

out vec2 UV;
out vec3 normal;

void main() {
	gl_Position = ApplyRig(vec4(iVert.xyz, 1), boneIndices, boneWeights) * MVP;
	normal = normalize(ApplyRigNormal(normalize(iNormal), boneIndices.xyzw, boneWeights) * transpose(inverse(mat3(M))));
	UV = (vec4(iUV.xy, 0, 1.0) * UVMatrix).xy;
}