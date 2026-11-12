#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;

layout (std140, binding = 0) uniform Transform
{
    mat4 mvp;
};

layout (location = 0) out vec2 vUv;

void main()
{
    gl_Position = mvp * vec4(inPos, 1.0);
    vUv = inUv;
}
