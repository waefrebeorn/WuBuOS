#version 450
/* A full-screen quad (two triangles) carrying UVs into the texture. */
layout(location = 0) out vec2 vUV;
void main() {
    vec2 pos[6] = vec2[](vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0, 1.0),
                         vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0, 1.0));
    vec2 p = pos[gl_VertexIndex];
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
