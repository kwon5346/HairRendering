#version 330 core

in vec3 gsFragPos;
in vec3 gsU;
in vec3 gsV;
in vec3 gsW;

in float gsSinThetaI;
in float gsSinThetaO;
in float gsCosThetaI;
in float gsCosThetaO;
in float gsCosPhiD; 

in float gsThickness;
in float gsTransparency;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float alphaScale;

uniform sampler2D marschnerTexture; 
uniform sampler2D NR_texture;
uniform sampler2D NTT_texture;
uniform sampler2D NTRT_texture;

uniform sampler2D prevDepth;      // 새로 추가
uniform vec2 screenSize;          // 새로 추가

const vec3 hairColor = vec3(0.32, 0.20, 0.09);
const float PI = 3.1415926535897932384626433832795;

void main(void) {
    // Depth Peeling discard 조건
    vec2 uv = gl_FragCoord.xy / screenSize;
    float prevZ = texture(prevDepth, uv).r;
    if (gl_FragCoord.z <= prevZ + 1e-5)
        discard;

    vec3 viewDir = normalize(viewPos - gsFragPos);
    float angularFade = pow(clamp(dot(viewDir, gsW), 0.0, 1.0), 2.0);
    float distanceFade = clamp(1.0 - length(viewPos - gsFragPos) * 0.15, 0.0, 1.0);
    float fade = max(angularFade * distanceFade, 0.2);
    float finalAlpha = clamp(gsTransparency * fade * 3.0, 0.0, 1.0);
    finalAlpha *= 0.3;
    vec2 texCoord1 = vec2(clamp((gsSinThetaI + 1.0) * 0.5, 0.0, 1.0), 
                          clamp((gsSinThetaO + 1.0) * 0.5, 0.0, 1.0));
    vec4 M_values = texture(marschnerTexture, texCoord1);
    float MR = M_values.r, MTT = M_values.g, MTRT = M_values.b, CosThetaD = M_values.a;

    vec2 texCoordAz = vec2(clamp((CosThetaD + 1.0) * 0.5, 0.0, 1.0), 
                           clamp((gsCosPhiD + 1.0) * 0.5, 0.0, 1.0));
    float NR = clamp(texture(NR_texture, texCoordAz).r, 0.0, 1.0);
    vec3 NTT = clamp(texture(NTT_texture, texCoordAz).rgb, 0.0, 1.0);
    vec3 NTRT = clamp(texture(NTRT_texture, texCoordAz).rgb, 0.0, 1.0);

    float cD2 = CosThetaD * CosThetaD;
    vec3 S = MR * NR * vec3(1.0) / cD2 
           + MTT * NTT  / cD2 
           + MTRT * NTRT / cD2;

    float widthFactor = clamp(gsThickness * 5.0, 0.5, 2.0);
    vec3 shadedColor = hairColor * S * widthFactor;

    FragColor = vec4(shadedColor, finalAlpha);
}
