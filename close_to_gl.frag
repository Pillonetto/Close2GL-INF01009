#version 150 core

uniform sampler2D uRasterTexture;

in vec2 vTexCoord;
out vec4 fColor;

void main() {
    fColor = texture(uRasterTexture, vTexCoord);
}
