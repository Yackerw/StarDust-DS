#version 330 core
out vec4 color;

uniform sampler2D mainTexture;
uniform vec4 diffColor;
uniform float stencil;
uniform vec3 emissive;
#include lighting.glh

in vec2 UV;

in vec3 normal;

void main() {
	// don't render anything if it's just stencil
	if (stencil == 1.0) {
		color = vec4(0,0,0,0);
		return;
	}
	color = texture(mainTexture, UV / textureSize(mainTexture, 0)).rgba;
	color.a *= diffColor.a;
	if (color.a <= 0) {
		discard;
	}
	color.rgb = color.rgb * clamp((diffColor.rgb * ApplyLighting(color.rgb, normalize(normal))) + emissive, 0, 1);
}