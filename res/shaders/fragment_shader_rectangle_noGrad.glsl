#version 150

in vec4 colorOut;
in vec2 texUV_out;
uniform float lenRatio;

out vec4 color;
void main (void) {
    vec2 tex = texUV_out;
    float c = lenRatio; 
    color = colorOut;
}

