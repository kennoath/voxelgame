#version 460 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;

void main() {
    vec4 tex = texture(text, TexCoords);
    color = tex;
}