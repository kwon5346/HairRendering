#version 330 core

layout(lines) in;
layout(line_strip, max_vertices = 2) out;

in vec3 vFragPos[];
in vec3 vFragNormal[];
in vec3 vU[];
in vec3 vV[];
in vec3 vW[];
in float vSinThetaI[];
in float vSinThetaO[];
in float vCosThetaI[];
in float vCosThetaO[];
in float vCosPhiD[];
in float vPhiI[];
in float vPhiO[];
in float vThickness[];
in float vTransparency[];
in vec3 vColor[];

out vec3 gsFragPos;
out vec3 gsFragNormal;
out vec3 gsU;
out vec3 gsV;
out vec3 gsW;
out float gsSinThetaI;
out float gsSinThetaO;
out float gsCosThetaI;
out float gsCosThetaO;
out float gsCosPhiD;
out float gsPhiI;
out float gsPhiO;
out float gsThickness;
out float gsTransparency;
out vec3 gsColor;

uniform mat4 MVP;

void main() {
    for (int i = 0; i < 2; i += 1) {
        gl_Position = MVP * vec4(vFragPos[i], 1.0);

        gsFragPos = vFragPos[i];
        gsFragNormal = vFragNormal[i];
        gsU = vU[i];
        gsV = vV[i];
        gsW = vW[i];
        gsSinThetaI = vSinThetaI[i];
        gsSinThetaO = vSinThetaO[i];
        gsCosThetaI = vCosThetaI[i];
        gsCosThetaO = vCosThetaO[i];
        gsCosPhiD = vCosPhiD[i];
        gsPhiI = vPhiI[i];
        gsPhiO = vPhiO[i];
        gsThickness = vThickness[i];
        gsTransparency = vTransparency[i];
        gsColor = vColor[i];

        EmitVertex();
    }
    EndPrimitive();
}
