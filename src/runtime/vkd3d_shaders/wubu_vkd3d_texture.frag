#version 450
/* A COMBINED image sampler -- which is what D3D12's SRV + static sampler really
 * is.  NOTE: writing `texture(sampler2D(tex, samp), uv)` with SEPARATE texture2D
 * and sampler bindings does NOT keep them separate -- glslang folds it into one
 * OpTypeSampledImage and deletes the sampler binding, so the pipeline layout
 * must declare COMBINED_IMAGE_SAMPLER or the draw silently samples with a
 * default NEAREST sampler. */
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main() { outColor = texture(tex, vUV); }
