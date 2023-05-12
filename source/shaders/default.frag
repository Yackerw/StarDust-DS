#version 330 core
out vec4 color;

uniform sampler2D mainTexture;
uniform vec4 diffColor;
#include lighting.glh

in vec2 UV;

in vec3 normal;

void main() {
	color = texture(mainTexture, UV).rgba;
	color *= diffColor;
	if (color.a <= 0) {
		discard;
	}
	color.rgb = ApplyLighting(color.rgb, normalize(normal));
}