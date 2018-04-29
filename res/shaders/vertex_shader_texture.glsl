#version 150

uniform mat4 PVM;

in vec3 vertex;
in vec2 texUV;
in vec4 color;

out vec4 colorOut; //out going
out vec2 texUV_out;
out float zAt;

void main() {
    gl_Position = PVM * vec4(vertex, 1);
    colorOut = color;
    texUV_out = texUV;
    zAt = gl_Position.z;
}