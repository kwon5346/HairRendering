#version 330 core

uniform sampler2D depth_range_map;
uniform float near;
uniform float far;
uniform int width;
uniform int height;

layout(location = 0) out uvec4 occ_bits;

void main() {
    vec2 texcoord = gl_FragCoord.xy / vec2(float(width), float(height));
    vec2 range = texture(depth_range_map, texcoord).xy;

    // 현재 프래그먼트의 선형 depth 계산
    float z_ndc = gl_FragCoord.z * 2.0 - 1.0;
    float d = (2.0 * near * far) / (far + near - z_ndc * (far - near));

    float depth_range = max(range.y - range.x, 1e-6);
    float ratio = clamp((d - range.x) / depth_range, 0.0, 1.0);

    int depth_id = int(ratio * 128.0);
    uint slab_id = uint(depth_id / 32);
    uint frag_id = uint(depth_id % 32);
    uint value = 1u << frag_id;

    if (slab_id == 0u)
        occ_bits = uvec4(value, 0u, 0u, 0u);
    else if (slab_id == 1u)
        occ_bits = uvec4(0u, value, 0u, 0u);
    else if (slab_id == 2u)
        occ_bits = uvec4(0u, 0u, value, 0u);
    else
        occ_bits = uvec4(0u, 0u, 0u, value);
}
