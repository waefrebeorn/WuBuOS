#version 450
/* D3D12-style binding: a root constant (push constant) scales a CBV colour.
 * Neither value is hardcoded -- both come from the API. */
layout(push_constant) uniform PC { float scale; } pc;
layout(binding = 0) uniform UBO { vec3 base; } ubo;
layout(location = 0) out vec3 vColor;
void main() {
    vec2 pos[3] = vec2[](vec2(-0.55, -0.45),
                         vec2( 0.55, -0.45),
                         vec2( 0.00,  0.55));
    vColor = ubo.base * pc.scale;
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
