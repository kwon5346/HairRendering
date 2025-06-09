#version 330 core

layout(location = 0) out vec4 fragColor;

void main() {
    float z = gl_FragCoord.z; // light-space depth in [0, 1]
    fragColor = vec4(z, 0.0, 0.0, z); // R: min depth, A: max depth


}

