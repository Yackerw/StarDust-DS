#version 330 core


layout(location = 0) in vec3 iVert;
layout(location = 1) in vec2 iUV;
layout(location = 2) in vec3 iNormal;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 UVMatrix;

uniform sampler2D mainTexture;

out vec2 UV;
out vec3 normal;

void main() {
	gl_Position = vec4(iVert.xyz, 1) * MVP;
	normal = normalize(iNormal * transpose(inverse(mat3(M))));
	UV = (vec4(iUV.xy, 0, 1.0) * UVMatrix).xy;
}