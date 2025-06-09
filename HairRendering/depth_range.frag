#version 330 core

in vec4 gl_FragCoord;
out vec4 frag_out;

void main() {
    float z = gl_FragCoord.z;
    frag_out = vec4(z, 0.0, 0.0, z); // R = min(z), A = max(z)
}