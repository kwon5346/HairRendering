#ifndef MARSCHNER_TEXTURE_H
#define MARSCHNER_TEXTURE_H

#include <GL/glew.h>
#include <glm/glm.hpp>

using namespace glm;

// LUT Texture ID
extern GLuint marschnerTex;
extern GLuint NR_tex;
extern GLuint NTT_tex;
extern GLuint NTRT_tex;

float marschner_M(float cos_theta_i, float cos_theta_o, float beta, float alpha);

GLuint createMarschnerTexture(int size);
GLuint createNR_Texture(int size, float eta);
GLuint createNTT_Texture(int size, float eta, vec3 absorption);
GLuint createNTRT_Texture(int size, float eta, vec3 absorption);

float fresnelReflect(float cosPhiD, float F0);

void saveMarschnerTexture(GLuint textureID, int size, const char* filename);
void saveNR_Texture(GLuint textureID, int size, const char* filename);
void saveNTT_Texture(GLuint textureID, int size, const char* filename);
void saveNTRT_Texture(GLuint textureID, int size, const char* filename);

#endif
