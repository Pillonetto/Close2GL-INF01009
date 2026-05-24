#version 150 core

in vec2 vPosition;
in vec2 vNormal;

out vec2 vTexCoord;

void main() {
    gl_Position = vec4(vPosition, 0.0, 1.0);
    vTexCoord = vNormal;
}
