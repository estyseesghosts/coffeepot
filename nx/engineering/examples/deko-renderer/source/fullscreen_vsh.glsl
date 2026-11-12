#version 460

layout (location = 0) out vec2 vUv;

// One oversized triangle covers the screen; no vertex buffer is bound for this pass.
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
