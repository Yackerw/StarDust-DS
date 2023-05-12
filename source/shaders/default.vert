#version 330 core


layout(location = 0) in vec3 iVert;
layout(location = 1) in vec2 iUV;
layout(location = 2) in vec3 iNormal;

uniform mat4 MVP;
uniform mat4 M;

uniform sampler2D mainTexture;

out vec2 UV;
out vec3 normal;

void main() {
	gl_Position = MVP * vec4(iVert.xyz, 1);
	normal = normalize(transpose(inverse(mat3(M))) * iNormal);
	//normal = iNormal;
	UV = iUV;
}