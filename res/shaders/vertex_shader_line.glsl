#version 150

uniform mat4 PVM;

in vec3 vertex;
in vec2 texUV;
in vec4 color;
in float lengthRatioIn;
in float percentY_;

out vec4 colorOut; //out going
out vec2 texUV_out;
out float lengthRatio;
out float percentY;

void main() {
    gl_Position = PVM * vec4(vertex, 1);
    colorOut = color;
    texUV_out = texUV;
    lengthRatio = lengthRatioIn;
    percentY = percentY_;
}

