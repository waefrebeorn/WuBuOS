#version 450
/* A minimal vertex shader: a hardcoded triangle, no vertex buffers.
 * gl_VertexIndex drives it, which is exactly how D3D12's
 * SV_VertexID / a root-constant vertex buffer would be fed. */
layout(location = 0) out vec3 vColor;
void main() {
    vec2 pos[3] = vec2[](vec2(-0.55, -0.45),
                         vec2( 0.55, -0.45),
                         vec2( 0.00,  0.55));
    vColor = vec3(0.25, 0.65, 1.0);
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
