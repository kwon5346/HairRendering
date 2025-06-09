#version 450 core

layout(location = 0) out vec4 FragColor;

uniform usampler2D occupancyMap;
uniform int totalSlices;     // e.g. 128
uniform int slicesPerSlab;   // e.g. 32

void main() {
    ivec2 uv = ivec2(gl_FragCoord.xy);
    uvec4 occupancy = texelFetch(occupancyMap, uv, 0);

    // 각 RGBA 채널에 대해 bit 개수 세기
    float slab0 = float(bitCount(occupancy.r));
    float slab1 = float(bitCount(occupancy.g));
    float slab2 = float(bitCount(occupancy.b));
    float slab3 = float(bitCount(occupancy.a));

    FragColor = vec4(slab0, slab1, slab2, slab3);

}
