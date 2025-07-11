#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
    tex_coord = aTexCoord;
}
