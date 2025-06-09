#version 450 core
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

layout (location = 0) out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float alphaScale;
uniform int passIndex;

uniform sampler2D marschnerTexture; 
uniform sampler2D NR_texture;
uniform sampler2D NTT_texture;
uniform sampler2D NTRT_texture;

// ====== Self-shadowing uniforms ======
uniform sampler2D depthRangeMap_shadow;   // RG: (min, max)
uniform usampler2D occupancyMap_shadow;   // RGBA32UI
uniform sampler2D slabMap_shadow;         // RGBA32F (slab별 fragment 수)
uniform mat4 lightMVP;
uniform float shadowWeight;               // e.g., 0.02

//const vec3 hairColor = vec3(0.32, 0.20, 0.09);
const vec3 hairColor = vec3(0.32, 0.20, 0.09); // Dark brown color

const float PI = 3.1415926535897932384626433832795;

void main(void) {
    vec3 viewDir = normalize(viewPos - gsFragPos);
    float angularFade = pow(clamp(dot(viewDir, gsW), 0.0, 1.0), 2.0);
    float distanceFade = clamp(1.0 - length(viewPos - gsFragPos) * 0.15, 0.0, 1.0);
    float fade = max(angularFade * distanceFade, 0.2);
    float finalAlpha = clamp(gsTransparency * fade * 3.0, 0.0, 1.0);

    // Marschner scattering lookup
    vec2 texCoord1 = vec2(clamp((gsSinThetaI + 1.0) * 0.5, 0.0, 1.0), 
                          clamp((gsSinThetaO + 1.0) * 0.5, 0.0, 1.0));
    vec4 M_values = texture(marschnerTexture, texCoord1);
    float MR = M_values.r; 
    float MTT = M_values.g;
    float MTRT = M_values.b;
    float CosThetaD = M_values.a;

    vec2 texCoordAz = vec2(clamp((CosThetaD + 1.0) * 0.5, 0.0, 1.0), 
                           clamp((gsCosPhiD + 1.0) * 0.5, 0.0, 1.0));
    float NR = clamp(texture(NR_texture, texCoordAz).r, 0.0, 1.0);
    vec3 NTT = clamp(texture(NTT_texture, texCoordAz).rgb, 0.0, 1.0);
    vec3 NTRT = clamp(texture(NTRT_texture, texCoordAz).rgb, 0.0, 1.0);

    float cD2 = CosThetaD * CosThetaD;
    vec3 S = (MR * NR * vec3(1.0) 
            + MTT * NTT * 3.0
            + MTRT * NTRT) *3.0 / cD2 * 0.5;

    float widthFactor = clamp(gsThickness * 5.0, 0.5, 2.0);
    //vec3 shadedColor = gsColor * S * widthFactor;
    vec3 shadedColor = hairColor * S * widthFactor;
    FragColor = vec4(shadedColor, finalAlpha);
    //FragColor = vec4(hairColor * (hairColor * S) * 0.3, finalAlpha);
}
