#version 330 core


layout(location = 0) in vec3 iVert;
layout(location = 1) in vec2 iUV;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in ivec4 boneIndices;
layout(location = 4) in vec4 boneWeights;

uniform mat4 MVP;
uniform mat4 M;

#include rigging.glh

uniform sampler2D mainTexture;

out vec2 UV;
out vec3 normal;

void main() {
	gl_Position = MVP * ApplyRig(vec4(iVert.xyz, 1), boneIndices, boneWeights);
	normal = normalize(transpose(inverse(mat3(M))) * ApplyRigNormal(normalize(iNormal), boneIndices.xyzw, boneWeights));
	//normal = iNormal;
	UV = iUV;
}