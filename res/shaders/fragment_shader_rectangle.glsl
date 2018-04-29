#version 150

in vec4 colorOut;
in vec2 texUV_out;

uniform float percentY;

out vec4 color;
void main (void) {
    
    vec4  finalColor = colorOut;
    float alpha = 1.0f;
    
    if(texUV_out.y < percentY) {
        alpha *= smoothstep(0, percentY, texUV_out.y);
        
    } else if(texUV_out.y > (1 - percentY)) {
        alpha *= 1 - smoothstep((1 - percentY), 1, texUV_out.y);
    }
    
    alpha = clamp(alpha, 0, 1);
    finalColor *= alpha;
    color = finalColor;
}