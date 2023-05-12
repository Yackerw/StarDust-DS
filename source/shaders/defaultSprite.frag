#version 330 core
out vec4 color;

uniform sampler2D mainTexture;

in vec2 UV;

void main() {
	color = texture(mainTexture, UV).rgba;
	if (color.a <= 0) {
		discard;
	}
}