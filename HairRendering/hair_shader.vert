#version 330 core

layout (location = 0) in vec3 aPos;    
layout (location = 1) in vec3 aUDirection;
layout (location = 2) in vec3 aVDirection;
layout (location = 3) in vec3 aWDirection;
layout (location = 4) in float aThickness;
layout (location = 5) in float aTransparency;


out vec3 vFragPos; 
out vec3 vU;
out vec3 vV;
out vec3 vW;
out vec3 vFragNormal;
out float vSinThetaI;
out float vSinThetaO;
out float vCosThetaI;
out float vCosThetaO;
out float vCosPhiD; 

out float vThickness;
out float vTransparency;

uniform mat4 MVP;
uniform mat4 model;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    vFragPos = vec3(model * vec4(aPos, 1.0));

    vec3 lightDir = normalize(lightPos - vFragPos);
    vec3 viewDir = normalize(viewPos - vFragPos);

    vU = normalize(mat3(model) * aUDirection);
    vV = normalize(mat3(model) * aVDirection);
    vW = normalize(mat3(model) * aWDirection);

    vSinThetaI = dot(lightDir, vU);
    vSinThetaO = dot(viewDir, vU);
    vCosThetaI = dot(lightDir, vW);
    vCosThetaO = dot(viewDir, vW);

    vec3 lightPerp = lightDir - vSinThetaI * vU; 
    vec3 eyePerp = viewDir - vSinThetaO * vU;
    vCosPhiD = pow(dot(eyePerp, lightPerp) * dot(eyePerp, eyePerp) * dot(lightPerp, lightPerp), 0.5);

    vThickness = aThickness;
    vTransparency = aTransparency;

    gl_Position = MVP * vec4(vFragPos, 1.0);
}
