#version 450 core

layout(location = 0) out uvec4 fragBits;

uniform sampler2D depthRangeMap;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(depthRangeMap, 0));
    vec2 depthRange = texture(depthRangeMap, uv).ra;  // R=min, A=max

    float z = gl_FragCoord.z;
    float norm = clamp((z - depthRange.x) / (depthRange.y - depthRange.x + 1e-5), 0.0, 0.9999);
    int slice = int(norm * 128.0);

    int slabIdx = slice / 32;
    int bitIdx  = slice % 32;

    uvec4 outMask = uvec4(0);
    outMask[slabIdx] = 1u << bitIdx;

    fragBits = outMask;
}
