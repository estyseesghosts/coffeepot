#version 460

layout (location = 0) in vec2 vUv;
layout (binding = 0) uniform sampler2D src;
layout (location = 0) out vec4 outColor;

// Swapping red and blue makes it obvious in a picture, and in a pixel check, that this pass ran.
void main()
{
    outColor = vec4(texture(src, vUv).bgr, 1.0);
}
