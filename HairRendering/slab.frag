#version 330 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D depth_range_map;
uniform sampler2D head_depth_map;  

uniform int width;
uniform int height;

void main() {
    vec2 texcoord = gl_FragCoord.xy / vec2(float(width), float(height));
    float head_depth = texture(head_depth_map, texcoord).r;

    // 머리카락이 head보다 뒤에 있으면 무시
    if (gl_FragCoord.z >= head_depth + 1e-4) discard;
  
    vec2 range = texture(depth_range_map, texcoord).xy;
    float minD = range.x;
    float maxD = range.y;
    float span = max(maxD - minD, 1e-6);

    float ratio = clamp((gl_FragCoord.z - minD) / span, 0.0, 1.0);

    int slab_id = int(ratio * 4.0);
    vec4 colors[4] = vec4[4](
        vec4(1, 0, 0, 0),  // red
        vec4(0, 1, 0, 0),  // green
        vec4(0, 0, 1, 0),  // blue
        vec4(0, 0, 0, 1)   // alpha
    );

    fragColor = colors[clamp(slab_id, 0, 3)];
}
