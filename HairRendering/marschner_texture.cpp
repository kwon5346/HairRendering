#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "marschner_texture.h"
#include <vector>
#include <cmath>
#include "stb_image_write.h"
#include <glm/glm.hpp>
#include <complex>
#include <iostream>
using namespace std;
using namespace glm;

// Fiber properties
const float eta = 1.55f;

// Surface properties
const float alphaR = -radians(7.5);
const float alphaTT = -alphaR / 2.0;
const float alphaTRT = -3.0 * alphaR / 2.0;
const float betaR = radians(7.5); 
const float betaTT = betaR / 2.0; 
const float betaTRT = 2.0 * betaR; 

// constants
const float PI = 3.141592653589793238;

GLuint marschnerTex = 0;
GLuint NR_tex = 0;
GLuint NTT_tex = 0;
GLuint NTRT_tex = 0;

float computeEtaPrime(float eta, float thetaD) {
    float sinPowThetaD = sin(thetaD) * sin(thetaD);
    float etaPrime = (sqrt(eta * eta - sinPowThetaD)) / cos(thetaD);
    return etaPrime;
}

float computeEtaDoublePrime(float eta, float thetaD) {
    float sinPowThetaD = sin(thetaD) * sin(thetaD);
    float etaDoublePrime = eta * eta * cos(thetaD) / sqrt(eta * eta - sinPowThetaD);
    return etaDoublePrime;
}

vec3 compute_sigma_A_prime(float theta_t, vec3 absorption) {
    float cosThetaT = cos(theta_t);
    vec3 sigma_A_prime = absorption / cosThetaT;
    return sigma_A_prime;
}

// highlight shift 적용 Gaussian
float marschner_M(float theta_i, float theta_o, float beta, float alpha) {     
    float theta_h = (theta_i + theta_o) / 2.0f;
    float shifted_theta_h = theta_h - alpha;

    float normalization = 1.0 / sqrt(2.0 * PI * beta * beta);
    return normalization * exp(-pow(shifted_theta_h, 2) / (2.0 * beta * beta));
}

// Longitudinal scattering texture
GLuint createMarschnerTexture(int size) {
    vector<float> textureData(size * size * 4);

    for (int i = 0; i < size; i++) {
        float sin_theta_i = -1.0f + 2.0f * (float)i / (size - 1);
		float theta_i = asinf(sin_theta_i); 

        for (int j = 0; j < size; j++) {
            float sin_theta_o = -1.0f + 2.0f * (float)j / (size - 1);
            float theta_o = asinf(sin_theta_o);
			float cosThetaD = cos((theta_o -theta_i) / 2);

            float M_R = marschner_M(theta_i, theta_o, betaR, alphaR);
            float M_TT = marschner_M(theta_i, theta_o, betaTT, alphaTT);
            float M_TRT = marschner_M(theta_i, theta_o, betaTRT, alphaTRT);

            int index = (j * size + i) * 4;
            textureData[index] = M_R;    // R
            textureData[index + 1] = M_TT; // G
            textureData[index + 2] = M_TRT; // B
            textureData[index + 3] = cosThetaD; // A
        }
    }

    glGenTextures(1, &marschnerTex);
    glBindTexture(GL_TEXTURE_2D, marschnerTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return marschnerTex;
}

float gaussian(float x, float wc) {
    float norm = 1.0f / (wc * sqrt(2.0f * PI));
    return norm * exp(-(x * x) / (2.0f * wc * wc));
}



// Fresnel 반사율 계산
float FresnelReflectance(float gamma_i, float etaPrime, float etaDoublePrime) {
    complex<float> eta(etaPrime, etaDoublePrime); 
    complex<float> cosThetaI(gamma_i, 0.0f);

    // Snell's law
    complex<float> sinThetaI = sqrt(1.0f - cosThetaI * cosThetaI);
    complex<float> sinThetaT = sinThetaI / eta;
    complex<float> cosThetaT = sqrt(1.0f - (sinThetaT * sinThetaT));

    // Fresnel 반사율 계산
    complex<float> Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    complex<float> Rp = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);

    float F = 0.5f * (std::norm(Rs) + std::norm(Rp)); // norm()은 복소수의 절댓값 제곱
    return F;
}

const float pi = 3.14159265358979323846f;

float solveGammaT(float c, float gamma_i) {
    float a = (3 * c / PI) * gamma_i;
    float b = (4 * c / PI * PI * PI) * gamma_i * gamma_i * gamma_i;
    return a - b;
}

float dphidh(int p, float c, float gamma_i, float h) {
    float a = ((6.0f * p * c / PI) - 2.0f) - (3.0f * 8.0f * p * c / (PI * PI * PI) * gamma_i * gamma_i);
    float b = sqrt(1.0f - h * h);
    return a / b;
}

float evaluate_phi_hat(int p, float gamma_i, float c) {
    float term1 = ((6.0f * p * c) / pi - 2.0f) * gamma_i;
    float term2 = (8.0f * p * c) / (pi * pi * pi) * pow(gamma_i, 3.0f);
    return term1 - term2 + p * pi;
}

vector<complex<float>> solveCubic(float a, float b, float c) {
    using cf = complex<float>;

    if (fabs(a) < 1e-6f) {
        if (fabs(b) < 1e-6f) return {}; 
        return { cf(-c / b, 0.0f) };
    }

    float p = b / a;
    float q = c / a;
    float delta = pow(q / 2.0f, 2) + pow(p / 3.0f, 3);

    cf sqrt_delta = sqrt(cf(delta, 0.0f));
    cf u_cubed = -q / 2.0f + sqrt_delta;
    cf v_cubed = -q / 2.0f - sqrt_delta;

    cf u = pow(u_cubed, 1.0f / 3.0f);
    cf v = pow(v_cubed, 1.0f / 3.0f);

    cf omega1(-0.5f, sqrt(3) / 2.0f);
    cf omega2(-0.5f, -sqrt(3) / 2.0f);

    cf x1 = u + v;
    cf x2 = u * omega1 + v * omega2;
    cf x3 = u * omega2 + v * omega1;

    return { x1, x2, x3 };
}

float solveGammaI(float phi_d, int p, float c) {
    float a = (8.0f * p * c) / pow(pi, 3.0f);
    float b = -((6.0f * p * c) / pi - 2.0f);
    float cc = phi_d - p * pi;

    auto roots = solveCubic(a, b, cc);

    float best = NAN;
    float min_err = 1e10f;
    float min_gamma_magnitude = 100.0f; 

    for (auto& r : roots) {
        if (fabs(r.imag()) < 1e-2f) { 
            float realRoot = r.real();
            if (fabs(realRoot) > 3.0f) continue;

            float phi_hat = evaluate_phi_hat(p, realRoot, c);
            float err = fabs(phi_hat - phi_d);

            if (p == 2) { 
                if (err < 0.3f && fabs(realRoot) < min_gamma_magnitude) {
                    best = realRoot;
                    min_err = err;
                    min_gamma_magnitude = fabs(realRoot);
                }
            }
            else {
                if (err < min_err) {
                    best = realRoot;
                    min_err = err;
                }
            }
        }
    }
    
    return best;
}


/*
float phi_p(float gamma_i, int p, float c) {
    float a = (6.0f * p * c / PI) - 2.0f;
    float b = -8.0f * p * c / (PI * PI * PI);
    return a * gamma_i + b * gamma_i * gamma_i * gamma_i + p * PI;
}
*/

/* 이분
float solveGammaI(float phi_d, int p, float eta, float c) {
    float lower = -c;
    float upper = c;
    float mid;
    float epsilon = 1e-4f;
    int max_iter = 50;

    for (int i = 0; i < max_iter; ++i) { 
        mid = 0.5f * (lower + upper);
        float phi_mid = phi_p(mid, p, c);
        float f_mid = phi_mid - phi_d;

        if (fabs(f_mid) < epsilon)
            return mid;

        float phi_lower = phi_p(lower, p, c);
        float f_lower = phi_lower - phi_d;

        if (f_mid * f_lower < 0)
            upper = mid;
        else
            lower = mid;
    }

    return mid; 
}
*/

GLuint createNR_Texture(int size, float eta) {
    vector<float> textureData(size * size);

    for (int i = 0; i < size; i++) {
        float cos_theta_d = -1.0f + 2.0f * (float)i / (size - 1);
        float theta_d = acosf(cos_theta_d); 
        float etaPrime = computeEtaPrime(eta, theta_d);
        float etaDoublePrime = computeEtaDoublePrime(eta, theta_d);
        float c = asinf(1.0f / etaPrime);

        for (int j = 0; j < size; j++) {
            float cos_phi_d = -1.0f + 2.0f * (float)j / (size - 1);
            float phi_d = acosf(cos_phi_d); 

            float gamma_i = solveGammaI(phi_d, 0, c);
            float h = sinf(gamma_i);
            float F = FresnelReflectance(gamma_i, etaPrime, etaDoublePrime);

            float N_R = F;
            int index = j * size + i;
            textureData[index] = N_R;
        }
    }
    glGenTextures(1, &NR_tex);
    glBindTexture(GL_TEXTURE_2D, NR_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, size, size, 0, GL_RED, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return NR_tex;
}



GLuint createNTT_Texture(int size, float eta, vec3 absorption) {
    vector<float> textureData(size * size * 3);

    for (int i = 0; i < size; i++) {
        float cos_theta_d = -1.0f + 2.0f * (float)i / (size - 1);
		float theta_d = acosf(cos_theta_d); 
        float theta_t = asin(sin(theta_d) / eta); 

        float etaPrime = computeEtaPrime(eta, theta_d);
        float etaDoublePrime = computeEtaDoublePrime(eta, theta_d);
        float c = asin(1.0f / etaPrime);
        vec3 sigma_A_prime = compute_sigma_A_prime(theta_t, absorption);

        for (int j = 0; j < size; j++) {
            float cos_phi_d = -1.0f + 2.0f * (float)j / (size - 1);
			float phi_d = acos(cos_phi_d); 

            float gamma_i = solveGammaI(phi_d, 1, c);
            float h = sinf(gamma_i);
			float gamma_t = asinf(h / etaPrime); 

            vec3 T = exp(-2.0f * sigma_A_prime * (1.0f + cos(2.0f * gamma_t)));
            float N_R = FresnelReflectance(gamma_i, etaPrime, etaDoublePrime);
            
            float inverse_double_dphidh = 1.0f / fabs(2.0f * dphidh(1, c, gamma_i, h));
            vec3 N_TT = pow(1.0f - N_R, 2.0f) * T * inverse_double_dphidh;

            int index = (j * size + i) * 3;
			textureData[index] = N_TT.r;
			textureData[index + 1] = N_TT.g;
			textureData[index + 2] = N_TT.b;
        }
    }
    glGenTextures(1, &NTT_tex);
    glBindTexture(GL_TEXTURE_2D, NTT_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return NTT_tex;
}

GLuint createNTRT_Texture(int size, float eta, vec3 absorption) {
    vector<float> textureData(size * size * 3);

    for (int i = 0; i < size; i++) {
        float cos_theta_d = -1.0f + 2.0f * (float)i / (size - 1);
		float theta_d = acosf(cos_theta_d); 
        float theta_t = asin(sin(theta_d) / eta);// snell's law

        float etaPrime = computeEtaPrime(eta, theta_d);
        float etaDoublePrime = computeEtaDoublePrime(eta, theta_d);
        float c = asinf((1 / etaPrime));
        vec3 sigma_A_prime = compute_sigma_A_prime(theta_t, absorption);

        for (int j = 0; j < size; j++) {
            float cos_phi_d = -1.0f + 2.0f * (float)j / (size - 1);
			float phi_d = acosf(cos_phi_d); 

			float gamma_i = solveGammaI(phi_d, 1, c);
            float h = sinf(gamma_i);
            float gamma_t = asinf(h / etaPrime);

            vec3 T = exp(-2.0f * sigma_A_prime * (1.0f + cos(2.0f * gamma_t)));

            float N_R = FresnelReflectance(gamma_i, etaPrime, etaDoublePrime);
            float inverse_double_dphidh = 1.0f / fabs(2.0f * dphidh(2, c, gamma_i, h));
            vec3 N_TRT = pow(1.0f - N_R, 2.0f) * FresnelReflectance(gamma_t, 1.0f / etaPrime, 1.0f / etaDoublePrime) * T * T * inverse_double_dphidh;

            int index = (j * size + i) * 3;

			textureData[index] = N_TRT.r;
			textureData[index + 1] = N_TRT.g;
			textureData[index + 2] = N_TRT.b;
        }
    }

    glGenTextures(1, &NTRT_tex);
    glBindTexture(GL_TEXTURE_2D, NTRT_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, textureData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return NTRT_tex;
}


void saveMarschnerTexture(GLuint textureID, int size, const char* filename) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    vector<unsigned char> textureData(size * size * 4);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());

    stbi_write_png(filename, size, size, 4, textureData.data(), size * 4);
}

void saveNR_Texture(GLuint textureID, int size, const char* filename) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    vector<unsigned char> textureData(size * size);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

    stbi_write_png(filename, size, size, 1, textureData.data(), size);
}

void saveNTT_Texture(GLuint textureID, int size, const char* filename) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    vector<unsigned char> textureData(size * size * 3);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());

    stbi_write_png(filename, size, size, 3, textureData.data(), size * 3);
}


void saveNTRT_Texture(GLuint textureID, int size, const char* filename) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    vector<unsigned char> textureData(size * size * 3);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());

    stbi_write_png(filename, size, size, 3, textureData.data(), size * 3);
}
