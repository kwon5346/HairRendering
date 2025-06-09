#version 330 core

layout(location = 0) in vec3 inPosition;

uniform mat4 MVP_light; 

void main() {
    gl_Position = MVP_light * vec4(inPosition, 1.0);
}
