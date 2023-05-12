#version 330 core


layout(location = 0) in vec3 iVert;
layout(location = 1) in vec2 iUV;

uniform sampler2D mainTexture;

out vec2 UV;

void main() {
	gl_Position.xy = iVert.xy;
	gl_Position.z = 0.0;
	gl_Position.w = 1.0;
	UV = iUV;
}