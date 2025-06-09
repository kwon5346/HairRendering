#define GLEW_STATIC
#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <cmath>
#include "shader.h"
#include "stb_image.h"
#include "marschner_texture.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace std;
using namespace glm;

const int windowWidth = 2560;
const int windowHeight = 1600;

struct ObjVertex {
    vec3 position;
    vec3 normal;
};

struct OBJModel {
    std::vector<ObjVertex> vertices;
    std::vector<unsigned int> indices;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
};


OBJModel loadOBJ(const std::string& filepath) {
    OBJModel model;
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_OptimizeMeshes);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp failed to load OBJ file: " << importer.GetErrorString() << std::endl;
        exit(1);
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];

        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            ObjVertex vertex;
            vertex.position = vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
            vertex.normal = vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);

            model.vertices.push_back(vertex);
        }

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                model.indices.push_back(face.mIndices[k]);
            }
        }
    }

    return model;
}


void setupObjBuffers(OBJModel& model) {
    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);
    glGenBuffers(1, &model.ebo);

    glBindVertexArray(model.vao);

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(ObjVertex), model.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ObjVertex), (void*)offsetof(ObjVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ObjVertex), (void*)offsetof(ObjVertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}


void renderOBJ(GLuint shaderProgram, const mat4& MVP, const mat4& model, const OBJModel& modelData, const vec3& cameraPos, const vec3& lightPos) {
    glUseProgram(shaderProgram);

    GLuint MVPLoc = glGetUniformLocation(shaderProgram, "MVP");
    glUniformMatrix4fv(MVPLoc, 1, GL_FALSE, value_ptr(MVP));

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    __glewUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));

    GLuint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    glUniform3fv(lightPosLoc, 1, value_ptr(lightPos));

    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    glUniform3fv(viewPosLoc, 1, value_ptr(cameraPos));

    float gamma = 2.2f;
    GLuint gammaLoc = glGetUniformLocation(shaderProgram, "gamma");
    glUniform1f(gammaLoc, gamma);

    glBindVertexArray(modelData.vao);
    glDrawElements(GL_TRIANGLES, modelData.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

struct HairVertex {
    vec3 position;
    vec3 uDirections;
    vec3 vDirections;
    vec3 wDirections;
    float thickness;       
    float transparency;   
};

struct HairStrand {
    vector<HairVertex> vertices;
};

struct HairModel {
    vector<HairStrand> strands;
};

void calculateUVWdirection(HairStrand& hairstrand) {
    size_t n = hairstrand.vertices.size();
    if (n < 2) return;

    vec3 u(0.0f);

    for (size_t i = 0; i < n - 1; i++) {
        if (i == 0 && n > 1) {
            vec3 p1 = vec3(hairstrand.vertices[i].position);
            vec3 p2 = vec3(hairstrand.vertices[i + 1].position);
            u = normalize(p2 - p1);
        }
        else if (i == n - 1 && n > 1) {
            vec3 p1 = vec3(hairstrand.vertices[i - 1].position);
            vec3 p2 = vec3(hairstrand.vertices[i].position);
            u = normalize(p2 - p1);
        }
        else {
            vec3 p1 = vec3(hairstrand.vertices[i - 1].position);
            vec3 p2 = vec3(hairstrand.vertices[i + 1].position);
            u = normalize(p2 - p1);
        }
        hairstrand.vertices[i].uDirections = u;

        vec3 v;
        if (fabs(u.x) > fabs(u.z)) {
            v = normalize(vec3(-u.y, u.x, 0.0f));
        } else {
            v = normalize(vec3(0.0f, -u.z, u.y));  
        }
        vec3 w = normalize(cross(u, v));  

        hairstrand.vertices[i].vDirections = v;
        hairstrand.vertices[i].wDirections = w;
    }
}

HairModel loadHairFile(const string& path) {
    HairModel model;
    ifstream file(path, ios::binary);
    if (!file) {
        std::cerr << "Cannot open .hair file" << std::endl;
        return model;
    }

    // Header
    char magic[4];
    file.read(magic, 4);
    if (strncmp(magic, "HAIR", 4) != 0) {
        std::cerr << "Invalid .hair format" << std::endl;
        return model;
    }

	// numStrand 4-7, numPoints 8-11, bitflags 12-15
    uint32_t numStrands, numPoints, flags;
    file.read((char*)&numStrands, 4);
    file.read((char*)&numPoints, 4);
    file.read((char*)&flags, 4);

    bool hasSegments = flags & (1 << 0);
    bool hasPoints = flags & (1 << 1);
    bool hasThickness = flags & (1 << 2);
    bool hasTransparency = flags & (1 << 3);
    bool hasColor = flags & (1 << 4); // 일단 파싱만함. 사용 x

    uint32_t defaultSegments;
    float defaultThickness, defaultTransparency, defaultColor[3];
    file.read((char*)&defaultSegments, 4);
    file.read((char*)&defaultThickness, 4);
    file.read((char*)&defaultTransparency, 4);
    file.read((char*)defaultColor, sizeof(float) * 3);
    file.ignore(88); // 파일정보 88바이트 무시

    // Segments
    vector<uint16_t> segments(numStrands);
    if (hasSegments)
        file.read((char*)segments.data(), sizeof(uint16_t) * numStrands);
    else
        fill(segments.begin(), segments.end(), defaultSegments);

    // Points
    vector<vec3> positions(numPoints);
    if (hasPoints)
        file.read((char*)positions.data(), sizeof(float) * 3 * numPoints);
    else {
        cerr << "No point data in .hair file" << endl;
        return model;
    }

    // Thickness
    std::vector<float> thickness(numPoints, defaultThickness);
    if (hasThickness)
        file.read((char*)thickness.data(), sizeof(float) * numPoints);

    // Transparency 
    std::vector<float> transparency(numPoints, defaultTransparency);
    if (hasTransparency)
        file.read((char*)transparency.data(), sizeof(float) * numPoints);

    // Color 
    std::vector<vec3> colors(numPoints, vec3(defaultColor[0], defaultColor[1], defaultColor[2]));
    if (hasColor)
        file.read((char*)colors.data(), sizeof(float) * 3 * numPoints);

    // Build strands 
    size_t offset = 0;
    for (size_t i = 0; i < numStrands; ++i) {
        uint16_t segs = segments[i];
        HairStrand strand;

        for (size_t j = 0; j <= segs; ++j) {
            HairVertex v;
            v.position = positions[offset];
            v.thickness = thickness[offset];
            v.transparency = transparency[offset];
            // v.color = colors[offset];
            //v.tipRatio = float(j) / float(segs);
            strand.vertices.push_back(v);
            ++offset;
        }

        calculateUVWdirection(strand);
        model.strands.push_back(strand);
    }

    
    // Rotate model for alignment
    mat4 R = glm::rotate(mat4(1.0f), glm::radians(90.0f), vec3(1, 0, 0));
    R = glm::rotate(R, glm::radians(90.0f), vec3(0, 1, 0));
    for (auto& strand : model.strands) {
        for (auto& v : strand.vertices) {
            vec4 p = vec4(v.position, 1.0f);
            v.position = vec3(R * p);
        }
    }
    
    return model;
}
/*
GLuint objFBO, objColorTex, objDepthTex;
void initObjFBO(GLuint& objFBO, GLuint& objColorTex, GLuint& objDepthTex, int width, int height) {
    glGenFramebuffers(1, &objFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, objFBO);

    // Color texture
    glGenTextures(1, &objColorTex);
    glBindTexture(GL_TEXTURE_2D, objColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, objColorTex, 0);

    // Depth texture
    glGenTextures(1, &objDepthTex);
    glBindTexture(GL_TEXTURE_2D, objDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, objDepthTex, 0);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[initObjFBO] Framebuffer not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
*/
//
//GLuint hairFBO;
//GLuint hairColorTex;
//
//void initHairFBO(GLuint& hairFBO, GLuint& colorTex, int width, int height)
//{
//    glGenFramebuffers(1, &hairFBO);
//    glBindFramebuffer(GL_FRAMEBUFFER, hairFBO);
//
//    glGenTextures(1, &colorTex);
//    glBindTexture(GL_TEXTURE_2D, colorTex);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
//
//    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
//        std::cerr << "Hair FBO is not complete!" << std::endl;
//
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}

vec3 computeHairCenter(const HairModel& model) {
    vec3 sum(0.0f); int cnt = 0;
    for (const auto& s : model.strands)
        for (const auto& v : s.vertices) sum += v.position, cnt++;
    return cnt > 0 ? sum / float(cnt) : vec3(0);
}


GLuint hairVAO, hairVBO;
//GLuint hairShadowVAO, hairShadowVBO;
//GLuint hairAlphaVAO, hairAlphaVBO;

void setupHairBuffers(const HairModel& hairModel) {
    std::vector<float> hairVertexData;

    for (const auto& strand : hairModel.strands) {
        for (const auto& v : strand.vertices) {
            hairVertexData.push_back(v.position.x);
            hairVertexData.push_back(v.position.y);
            hairVertexData.push_back(v.position.z);

            hairVertexData.push_back(v.uDirections.x);
            hairVertexData.push_back(v.uDirections.y);
            hairVertexData.push_back(v.uDirections.z);

            hairVertexData.push_back(v.vDirections.x);
            hairVertexData.push_back(v.vDirections.y);
            hairVertexData.push_back(v.vDirections.z);

            hairVertexData.push_back(v.wDirections.x);
            hairVertexData.push_back(v.wDirections.y);
            hairVertexData.push_back(v.wDirections.z);

            hairVertexData.push_back(v.thickness);
            hairVertexData.push_back(v.transparency);
        }
    }

    glGenVertexArrays(1, &hairVAO);
    glGenBuffers(1, &hairVBO);

    glBindVertexArray(hairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hairVBO);
    glBufferData(GL_ARRAY_BUFFER, hairVertexData.size() * sizeof(float), hairVertexData.data(), GL_STATIC_DRAW);

    GLsizei stride = 14 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
    glEnableVertexAttribArray(0); // position

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1); // u

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2); // v

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3); // w

    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4); // thickness

    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(13 * sizeof(float)));
    glEnableVertexAttribArray(5); // transparency

    glBindVertexArray(0);
}

//void setupShadowHairBuffers(const HairModel& hairModel) {
//    std::vector<float> hairVertexData;
//
//    for (const auto& strand : hairModel.strands) {
//        for (const auto& v : strand.vertices) {
//            hairVertexData.push_back(v.position.x);
//            hairVertexData.push_back(v.position.y);
//            hairVertexData.push_back(v.position.z);
//        }
//    }
//
//    glGenVertexArrays(1, &hairShadowVAO);
//    glGenBuffers(1, &hairShadowVBO);
//
//    glBindVertexArray(hairShadowVAO);
//    glBindBuffer(GL_ARRAY_BUFFER, hairShadowVBO);
//    glBufferData(GL_ARRAY_BUFFER, hairVertexData.size() * sizeof(float), hairVertexData.data(), GL_STATIC_DRAW);
//
//    GLsizei stride = 3 * sizeof(float);
//
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
//    glEnableVertexAttribArray(0); 
//
//    glBindVertexArray(0);
//}
//
//void setupAlphaHairBuffers(const HairModel& hairModel) {
//    std::vector<float> hairVertexData;
//
//    for (const auto& strand : hairModel.strands) {
//        for (const auto& v : strand.vertices) {
//            hairVertexData.push_back(v.position.x);
//            hairVertexData.push_back(v.position.y);
//            hairVertexData.push_back(v.position.z);
//        }
//    }
//
//    glGenVertexArrays(1, &hairAlphaVAO);
//    glGenBuffers(1, &hairAlphaVBO);
//
//    glBindVertexArray(hairAlphaVAO);
//    glBindBuffer(GL_ARRAY_BUFFER, hairAlphaVBO);
//    glBufferData(GL_ARRAY_BUFFER, hairVertexData.size() * sizeof(float), hairVertexData.data(), GL_STATIC_DRAW);
//
//    GLsizei stride = 3 * sizeof(float);
//
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
//    glEnableVertexAttribArray(0); 
//
//    glBindVertexArray(0);
//}


void saveAsOBJ(const string& outPath, const vector<HairStrand>& strands) {
    ofstream out(outPath);
    if (!out.is_open()) {
        cerr << "Failed to write OBJ file." << endl;
        return;
    }

    int vertexOffset = 1;
    for (const auto& strand : strands) {
        for (const auto& v : strand.vertices) {
            out << "v " << v.position.x << " " << v.position.y << " " << v.position.z << endl;
        }

        out << "l";
        for (int i = 0; i < strand.vertices.size(); ++i) {
            out << " " << vertexOffset + i;
        }
        out << endl;

        vertexOffset += strand.vertices.size();
    }

    out.close();
}

HairModel convertStrandsToHairModel(const vector<HairStrand>& strands) {
    HairModel model;
    model.strands = strands;
    return model;
}

/*
GLuint fbo_depth_range;
GLuint tex_depth_range;

GLuint fbo_occupancy;
GLuint tex_occupancy;

GLuint fbo_slab;
GLuint tex_slab;

GLuint fbo_composite;
GLuint tex_composite;

void initFramebuffer(GLuint& fbo, GLuint& tex, GLenum internalFormat, GLenum format, GLenum type, int width, int height)
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer not complete: %d\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


GLuint quadVAO = 0, quadVBO = 0;
void renderFullscreenQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// self-shadow framebuffers
GLuint fbo_shadowDepthRange, tex_shadowDepthRange;
GLuint fbo_shadowOccupancy, tex_shadowOccupancy;
GLuint fbo_shadowSlab, tex_shadowSlab;

void initAllFramebuffers(int width, int height)
{
    // Shadow Depth Range Map
    initFramebuffer(fbo_shadowDepthRange, tex_shadowDepthRange, GL_RGBA32F, GL_RGBA, GL_FLOAT, width, height);

    // Shadow Occupancy Map
    initFramebuffer(fbo_shadowOccupancy, tex_shadowOccupancy, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, width, height);

    // Shadow Slab Map
    initFramebuffer(fbo_shadowSlab, tex_shadowSlab, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);



    // [1] Depth Range Map 
    initFramebuffer(fbo_depth_range, tex_depth_range, GL_RGBA32F, GL_RGBA, GL_FLOAT, width, height);

    // [2] Occupancy Map 
    initFramebuffer(fbo_occupancy, tex_occupancy, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, width, height);

    // [3] Slab Map 
    initFramebuffer(fbo_slab, tex_slab, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);

    // [4] Composite Buffer 
    initFramebuffer(fbo_composite, tex_composite, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, width, height);
}


// *****Rendering Functions*****
void renderShadowDepthMap(GLuint shader, const HairModel& hairModel, const mat4& MVP_light)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadowDepthRange);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(1.0f, 0.0f, 0.0f, 0.0f); // R: min에 쓰일 초기값 (1.0), A: max에 쓰일 초기값 (0.0)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);      
    glDepthMask(GL_FALSE);         
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);                          // 덮어쓰기 없이 값 유지
    glBlendEquationSeparate(GL_MIN, GL_MAX);              // R = min(z), A = max(z)

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "MVP_light"), 1, GL_FALSE, glm::value_ptr(MVP_light));

    glBindVertexArray(hairShadowVAO);

    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void renderShadowOccupancy(GLuint shader, const HairModel& hairModel, const mat4& MVP_light, GLuint depthRangeTex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadowOccupancy);
    glViewport(0, 0, windowWidth, windowHeight);
    GLuint zeros[4] = { 0, 0, 0, 0 };
    glClearBufferuiv(GL_COLOR, 0, zeros);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "MVP_light"), 1, GL_FALSE, glm::value_ptr(MVP_light));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_shadowDepthRange);
    glUniform1i(glGetUniformLocation(shader, "depthRangeMap"), 0);

    glBindVertexArray(hairShadowVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderShadowSlab(GLuint shader, const HairModel& hairModel, const mat4& MVP_light, GLuint occupancyTex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadowSlab); 
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //  additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // 누적

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "MVP_light"), 1, GL_FALSE, glm::value_ptr(MVP_light));
    glUniform1i(glGetUniformLocation(shader, "totalSlices"), 128);      // optional
    glUniform1i(glGetUniformLocation(shader, "slicesPerSlab"), 32);     // optional

    // occupancyMap 텍스처 바인딩
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, occupancyTex);
    glUniform1i(glGetUniformLocation(shader, "occupancyMap"), 0);

    glBindVertexArray(hairShadowVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint fbo_headDepth;
GLuint tex_headDepth;



void renderHeadDepthMap(GLuint depthOnlyShader, const OBJModel& headModel, const glm::mat4& MVP)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_headDepth);
    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);                    // 깊이 기록
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // 색상 기록 안함

    glUseProgram(depthOnlyShader);
    glUniformMatrix4fv(glGetUniformLocation(depthOnlyShader, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));

    glBindVertexArray(headModel.vao);
    glDrawElements(GL_TRIANGLES, headModel.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // 색상 기록 복구
}



void renderDepthRange(GLuint shader, const HairModel& hairModel, const glm::mat4& MVP)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_depth_range);
    glViewport(0, 0, windowWidth, windowHeight);

    glClearColor(1.0f, 0.0f, 0.0f, 0.0f); // R=min, A=max 초기화
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);      // 중요!!
    glDepthMask(GL_FALSE);        // z-buffer에 쓰지 않음 (다른 pass 영향 없게)

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquationSeparate(GL_MIN, GL_MAX);

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));

    glBindVertexArray(hairAlphaVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// PASS 2: Occupancy Map
void renderOccupancy(GLuint shaderOccupancy, const HairModel& hairModel, const mat4& MVP_auto, float near, float far, GLuint tex_depth_range)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_occupancy);
    glViewport(0, 0, windowWidth, windowHeight);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_OR);

    glUseProgram(shaderOccupancy);
    glUniformMatrix4fv(glGetUniformLocation(shaderOccupancy, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP_auto));
    glUniform1f(glGetUniformLocation(shaderOccupancy, "near"), near);
    glUniform1f(glGetUniformLocation(shaderOccupancy, "far"), far);
    glUniform1i(glGetUniformLocation(shaderOccupancy, "width"), windowWidth);
    glUniform1i(glGetUniformLocation(shaderOccupancy, "height"), windowHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_depth_range);
    glUniform1i(glGetUniformLocation(shaderOccupancy, "depth_range_map"), 0);

    glBindVertexArray(hairAlphaVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_COLOR_LOGIC_OP);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// PASS 3: Slab Map
void renderSlabMap(GLuint shaderSlab, const HairModel& hairModel, const mat4& MVP, float near, float far, GLuint tex_occupancy)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_slab);
    glViewport(0, 0, windowWidth, windowHeight);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glUseProgram(shaderSlab);
    glUniformMatrix4fv(glGetUniformLocation(shaderSlab, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform1f(glGetUniformLocation(shaderSlab, "near"), near);
    glUniform1f(glGetUniformLocation(shaderSlab, "far"), far);
    glUniform1i(glGetUniformLocation(shaderSlab, "depth_range_map"), 0);
    glUniform1i(glGetUniformLocation(shaderSlab, "headD"), 1);
    glUniform1i(glGetUniformLocation(shaderSlab, "width"), windowWidth);
    glUniform1i(glGetUniformLocation(shaderSlab, "height"), windowHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_occupancy); // depth_range_map
    glUniform1i(glGetUniformLocation(shaderSlab, "depth_range_map"), 0);


    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_headDepth);   // head_depth_map (optional, if used)
    glUniform1i(glGetUniformLocation(shaderSlab, "head_depth_map"), 0);


    glBindVertexArray(hairAlphaVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// PASS 6: Composite Pass
void renderComposite(GLuint shaderComposite)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to default framebuffer (screen)
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shaderComposite);

    // Set texture inputs
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, objColorTex);
    glUniform1i(glGetUniformLocation(shaderComposite, "mesh_map"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, hairColorTex);
    glUniform1i(glGetUniformLocation(shaderComposite, "strands_map"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_slab);
    glUniform1i(glGetUniformLocation(shaderComposite, "slab_map"), 2);

    // Draw fullscreen quad (assumes VAO is bound elsewhere)
    renderFullscreenQuad();

    glUseProgram(0);
    glDisable(GL_BLEND);
}
*/

/*
void renderHair(GLuint shaderProgram, const HairModel& hairModel, const mat4& MVP, const mat4& model, const vec3& cameraPos, const vec3& lightPos)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // 머리카락은 투명도 기반 누적이므로 쓰지 않음

    glUseProgram(shaderProgram);

    // Uniforms
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    
    // Marschner LUTs
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, marschnerTex);
    glUniform1i(glGetUniformLocation(shaderProgram, "marschnerTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, NR_tex);
    glUniform1i(glGetUniformLocation(shaderProgram, "NR_texture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, NTT_tex);
    glUniform1i(glGetUniformLocation(shaderProgram, "NTT_texture"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, NTRT_tex);
    glUniform1i(glGetUniformLocation(shaderProgram, "NTRT_texture"), 3);

    /*
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, tex_shadowDepthRange);
    glUniform1i(glGetUniformLocation(shaderProgram, "depthRangeMap_shadow"), 4);

    glActiveTexture(GL_TEXTURE5); 
    glBindTexture(GL_TEXTURE_2D, tex_shadowOccupancy);
    glUniform1i(glGetUniformLocation(shaderProgram, "occupancyMap_shadow"), 5);

    glActiveTexture(GL_TEXTURE6); 
    glBindTexture(GL_TEXTURE_2D, tex_shadowSlab);
    glUniform1i(glGetUniformLocation(shaderProgram, "slabMap_shadow"), 6);

	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, tex_headDepth);
	glUniform1i(glGetUniformLocation(shaderProgram, "headDepthMap_shadow"), 7);

    //glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightMVP"), 1, GL_FALSE, glm::value_ptr(MVP_light));
    
    //glUniform1f(glGetUniformLocation(shaderProgram, "shadowWeight"), 0.002f); // 튜닝 가능
    //glUniform1f(glGetUniformLocation(shaderProgram, "alphaScale"), 3.0f); // 튜닝 가능

    // Draw hair strands
    glBindVertexArray(hairVAO);
    int offset = 0;
    for (const auto& strand : hairModel.strands) {
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    }
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

*/

void renderHair(GLuint shaderProgram, const mat4& MVP, const mat4& model, HairModel& hairmodel, const vec3& cameraPos, const vec3& lightPos) {
   
	//glEnable(GL_DEPTH_TEST);
    //glDepthMask(GL_TRUE); 
	//glDisable(GL_BLEND); 
    glUseProgram(shaderProgram); 
    GLuint MVPLoc = glGetUniformLocation(shaderProgram, "MVP"); 
    glUniformMatrix4fv(MVPLoc, 1, GL_FALSE, value_ptr(MVP)); 
    GLuint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos"); 
    glUniform3fv(lightPosLoc, 1, value_ptr(lightPos)); 
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    __glewUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model)); 
    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos"); 

    glUniform3fv(viewPosLoc, 1, value_ptr(cameraPos)); 
    glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, marschnerTex); 
    glUniform1i(glGetUniformLocation(shaderProgram, "marschnerTexture"), 0); 
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, NR_tex); 
    glUniform1i(glGetUniformLocation(shaderProgram, "NR_texture"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, NTT_tex); 
    glUniform1i(glGetUniformLocation(shaderProgram, "NTT_texture"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, NTRT_tex); 
    glUniform1i(glGetUniformLocation(shaderProgram, "NTRT_texture"), 3);



    glBindVertexArray(hairVAO); 
    int offset = 0; 
    for (const auto& strand : hairmodel.strands) { 
        glDrawArrays(GL_LINE_STRIP, offset, strand.vertices.size());
        offset += strand.vertices.size();
    } 

	//glDepthMask(GL_TRUE); // 깊이 버퍼 기록 활성화

}


float fov = 33.0f;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= static_cast<float>(yoffset);
    
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 120.0f)
        fov = 120.0f;
}

// vec3 cameraTarget(0.0f, 1.7f, 0.0f); 
vec3 cameraTarget(0.0f, 50.0f, 80.0f); 
float radius = 200.0f;                  
float camYaw = 90.0f;
float camPitch = 0.0f;

bool mouseLeftPressed = false;
double lastX = 0.0, lastY = 0.0;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouseLeftPressed = true;
            glfwGetCursorPos(window, &lastX, &lastY);
        }
        else if (action == GLFW_RELEASE) {
            mouseLeftPressed = false;
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (mouseLeftPressed) {
        double xoffset = xpos - lastX;
        double yoffset = ypos - lastY;
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.1f;
        camYaw += static_cast<float>(xoffset) * sensitivity;
        camPitch += static_cast<float>(yoffset) * sensitivity;  

        if (camPitch > 89.0f)
            camPitch = 89.0f;
        if (camPitch < -89.0f)
            camPitch = -89.0f;
    }
}

const vec3 absorption = vec3(0.44, 0.64, 0.9); // brown
//const vec3 absorption = vec3(0.2, 0.2, 0.2); // blond
vector<vec3> predefinedAbsorptions = {
    vec3(0.1f, 0.1f, 0.1f),   // 밝은 금발
    vec3(0.25f, 0.15f, 0.1f), // 갈색
    vec3(0.4f, 0.3f, 0.2f),   // 진한 갈색
    vec3(0.8f, 0.6f, 0.4f),   // 흑갈색
    vec3(1.2f, 1.0f, 0.8f)    // 흑발
};
const char* absorptionLabels[] = {
    "Light Blonde", "Brown", "Dark Brown", "Brunette", "Black"
};
int selectedAbsorptionIndex = 0;

string selectedHairFile = "../hairstyles/wCurly.hair";  // 기본 파일
bool reloadHair = true;
float lightPos[3] = {0.0f, 50.0f, 50.0f }; // 광원 초기 위치

void showGUI(HairModel& hairModel) {
    ImGui::Begin("Hair Rendering Controls");
    ImGui::SetWindowFontScale(2.0f);
    // LightPos 조정 슬라이더
    ImGui::Text("Light Position:");
    ImGui::SliderFloat("Light X", &lightPos[0], -100.0f, 100.0f);
    ImGui::SliderFloat("Light Y", &lightPos[1], 0.0f, 100.0f);
    ImGui::SliderFloat("Light Z", &lightPos[2], -100.0f, 100.0f);

    // hairstyle 선택, load
    ImGui::Text("Hair Data Selection:");
    static char fileInputBuffer[256] = "../hairstyles/wStraight.hair";
    ImGui::InputText("Hair File", fileInputBuffer, IM_ARRAYSIZE(fileInputBuffer));
    if (ImGui::Button("Load Hair Data")) {
        selectedHairFile = std::string(fileInputBuffer);
        reloadHair = true;
    }
    GLuint Hair_shaderProgram = loadShaders("hair_shader.vert", "hair_shader.frag", "hair_shader.geom");

    if (ImGui::Combo("Hair Absorption", &selectedAbsorptionIndex, absorptionLabels, IM_ARRAYSIZE(absorptionLabels))) {
        vec3 selectedAbsorption = predefinedAbsorptions[selectedAbsorptionIndex];

        // Re-generate the textures
        if (NTT_tex) glDeleteTextures(1, &NTT_tex);
        if (NTRT_tex) glDeleteTextures(1, &NTRT_tex);

        NTT_tex = createNTT_Texture(size, eta, selectedAbsorption);
        NTRT_tex = createNTRT_Texture(size, eta, selectedAbsorption);
    }

    //ImGui::Text("shadowDepthRange"); ImGui::Image((ImTextureID)(intptr_t)tex_shadowDepthRange, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));

    //ImGui::Text("shadowOccupancy"); ImGui::Image((ImTextureID)(intptr_t)tex_shadowOccupancy, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
    //ImGui::Text("tex_shadowSlab"); ImGui::Image((ImTextureID)(intptr_t)tex_shadowSlab, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));

    //ImGui::Text("objDepthTex"); ImGui::Image((ImTextureID)(intptr_t)tex_headDepth, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
    //ImGui::Text("DepthRange"); ImGui::Image((ImTextureID)(intptr_t)tex_depth_range, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));

    //ImGui::Text("occupancy map:"); ImGui::Image((ImTextureID)(intptr_t)tex_occupancy, ImVec2(256,256),ImVec2(0, 1), ImVec2(1, 0));
    //ImGui::Text("Slab map:"); ImGui::Image((ImTextureID)(intptr_t)tex_slab, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));

    ImGui::End();
}
void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error [" << error << "]: " << description << std::endl;
}

vec3 computeMeshCenter(const vector<ObjVertex>& vertices) {
	vec3 sum(0.0f);
	for (const auto& vertex : vertices) {
		sum += vertex.position;
	}
	return sum / static_cast<float>(vertices.size());
}

void SetDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // 전체 UI 크기
    ImGui::GetStyle().ScaleAllSizes(1.2f);

    style.FrameRounding = 6.0f;  // 박스의 모서리 둥글기
    style.WindowRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.8f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.4f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
}
/*
int main() 
{
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Hair Rendering", nullptr, nullptr);
    if (!window) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit(); 

    initObjFBO(objFBO, objColorTex, objDepthTex, windowWidth, windowHeight);
    initDepthPeelingBuffers(windowWidth, windowHeight);
	initHairFBO(hairFBO, hairColorTex, windowWidth, windowHeight);
	initAllFramebuffers(windowWidth, windowHeight);

    

    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    //ImGui 초기화
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    SetDarkTheme();
    ImGui::GetStyle().ScaleAllSizes(4.5f);

    // load shaders
	GLuint shadowDepthShader = loadShaders("depthrange_shadow.vert", "depthrange_shadow.frag");
	GLuint shadowOccupancyShader = loadShaders("depthrange_shadow.vert", "occupancy_shadow.frag");
    GLuint shadowSlabShader = loadShaders("depthrange_shadow.vert", "slab_shadow.frag");

    GLuint Obj_shaderProgram = loadShaders("obj_shader.vert", "obj_shader.frag");

    GLuint depthOnlyShader = loadShaders("depth_range.vert", "depth_only.frag");

	GLuint depthRangeShader = loadShaders("depth_range.vert", "depth_range.frag");
	GLuint occupancyShader = loadShaders("depth_range.vert", "occu.frag"); //vertex shader는 딴거 재활용해도 될듯?
	GLuint slabShader = loadShaders("depth_range.vert", "slab.frag");

	GLuint Hair_shaderProgram = loadShaders("hair_shader.vert", "hair_shader.frag", "hair_shader.geom");
    GLuint blendingShader = loadShaders("composite.vert", "composite.frag");

    marschnerTex = createMarschnerTexture(256);
    saveMarschnerTexture(marschnerTex, 256, "marschner_texture.png");

    NR_tex = createNR_Texture(256, 1.55f);
    saveNR_Texture(NR_tex, 256, "NR_texture.png");

    NTT_tex = createNTT_Texture(256, 1.55f, absorption);
    saveNTT_Texture(NTT_tex, 256, "NTT_texture.png");

    NTRT_tex = createNTRT_Texture(256, 1.55f, absorption);
    saveNTRT_Texture(NTRT_tex, 256, "NTRT_texture.png");

    OBJModel headModel = loadOBJ("../hairstyles/woman_head.ply");
    setupObjBuffers(headModel);
	vec3 headcenter = computeMeshCenter(headModel.vertices);
    cameraTarget = headcenter;

    HairModel hairModel;
    
    setupHairBuffers(hairModel);
	setupShadowHairBuffers(hairModel);
	setupAlphaHairBuffers(hairModel);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // [1] 모델 행렬 (월드 내 회전 정렬)
        mat4 model = mat4(1.0f);
        mat4 Hairmodel = glm::mat4(1.0f);

        model = rotate(model, radians(-90.0f), vec3(0.0f, 1.0f, 0.0f));
        model = rotate(model, radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));

        // [2] 카메라 위치 계산
        float radYaw = radians(camYaw);
        float radPitch = radians(camPitch);
        vec3 cameraPos;
        cameraPos.x = cameraTarget.x + radius * cos(radPitch) * cos(radYaw);
        cameraPos.y = cameraTarget.y + radius * sin(radPitch);
        cameraPos.z = cameraTarget.z + radius * cos(radPitch) * sin(radYaw);

        // [3] 뷰 / 투영 행렬 (카메라 기준)
        mat4 view = lookAt(cameraPos, cameraTarget, vec3(0.0f, 1.0f, 0.0f));
        float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
        float zNear = radius - 3.0f;
        float zFar = radius + 3.0f;
        mat4 proj = perspective(radians(fov), aspect, zNear, zFar);

        // [4] 카메라 기준 MVP
        mat4 MVP_camera = proj * view * model;

        // [5] 조정된 near/far 계산 (깊이 범위 기반 자동 보정)
        glGenFramebuffers(1, &fbo_headDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_headDepth);

        // 텍스처 생성
        glGenTextures(1, &tex_headDepth);
        glBindTexture(GL_TEXTURE_2D, tex_headDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // FBO에 depth attachment
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex_headDepth, 0);
        glDrawBuffer(GL_NONE); // color 안씀
        glReadBuffer(GL_NONE);

        renderDepthRange(depthRangeShader, hairModel, MVP_camera);
        float zValues[4];
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_depth_range);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, zValues);
        float minZ = zValues[0];
        float maxZ = zValues[3];


        float ndcNear = minZ * 2.0f - 1.0f;
        float ndcFar = maxZ * 2.0f - 1.0f;
        float camDist = length(cameraPos - cameraTarget);

        float auto_near = std::max(0.01f, camDist * (ndcNear + 1.0f) / 2.0f);
        float auto_far = camDist * (ndcFar + 1.0f) / 2.0f + 0.05f;

        mat4 proj_auto = perspective(radians(fov), aspect, auto_near, auto_far);
        mat4 MVP_auto = proj_auto * view * model;


        // [6] 라이트 기준 뷰 및 투영
        vec3 lightPosWorld = vec3(lightPos[0], lightPos[1], lightPos[2]);
        mat4 lightView = lookAt(lightPosWorld, cameraTarget, vec3(0.0f, 1.0f, 0.0f));
        mat4 lightProj = ortho(-60.0f, 60.0f, -60.0f, 60.0f, 0.1f, 300.0f);
        mat4 MVP_light = lightProj * lightView * model;

       
        // light view pass
		renderShadowDepthMap(shadowDepthShader, hairModel, MVP_light);
		renderShadowOccupancy(shadowOccupancyShader, hairModel, MVP_light, tex_shadowDepthRange);
		renderShadowSlab(shadowSlabShader, hairModel, MVP_light, tex_shadowOccupancy);

		renderHeadDepthMap(depthOnlyShader, headModel, MVP_light);

        // camera view pass
		// Depth Range Map
		renderDepthRange(depthRangeShader, hairModel, MVP_auto);
  
		// Occupancy Map
		renderOccupancy(occupancyShader, hairModel, MVP_auto, auto_near, auto_far, tex_depth_range);
		//  Slab Map
		renderSlabMap(slabShader, hairModel, MVP_auto, auto_near ,auto_far, tex_occupancy);
        mat4 projection_mesh = perspective(radians(fov), aspect, 0.1f, 1000.0f);
        mat4 MVP_mesh = projection_mesh * view * model;

		// Head Model Rendering
        glBindFramebuffer(GL_FRAMEBUFFER, objFBO);
        glViewport(0, 0, windowWidth, windowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
		renderOBJ(Obj_shaderProgram, MVP_mesh, model, headModel, cameraPos, lightPosWorld);

		//  Hair Rendering
        glBindFramebuffer(GL_FRAMEBUFFER, hairFBO);
        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, hairFBO);
		renderHair(Hair_shaderProgram, hairModel, MVP_auto, Hairmodel, cameraPos, lightPosWorld, MVP_light);

		// PASS 6: Composite Pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
		renderComposite(blendingShader);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        showGUI(hairModel);
        if (reloadHair) {
            hairModel = loadHairFile(selectedHairFile);
            setupHairBuffers(hairModel);
            setupShadowHairBuffers(hairModel);
			setupAlphaHairBuffers(hairModel);
			// cameraTarget = computeHairCenter(hairModel);
            reloadHair = false;
        }
    
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth, windowHeight);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteTextures(1, &marschnerTex);
    glDeleteTextures(1, &NR_tex);
    glDeleteTextures(1, &NTT_tex);
    glDeleteTextures(1, &NTRT_tex);
    
    glDeleteVertexArrays(1, &hairVAO);
    glDeleteBuffers(1, &hairVBO);
    glDeleteProgram(Obj_shaderProgram);
    glDeleteProgram(Hair_shaderProgram);

    glfwTerminate();
    return 0;
}
*/

int main() {
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    glfwSetErrorCallback(glfwErrorCallback);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Hair Rendering", nullptr, nullptr);

    if (!window) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();

    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);


    //ImGui 초기화
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    SetDarkTheme();
    ImGui::GetStyle().ScaleAllSizes(4.5f);

    GLuint Obj_shaderProgram = loadShaders("obj_shader.vert", "obj_shader.frag");
    GLuint Hair_shaderProgram = loadShaders("hair_shader.vert", "hair_shader.frag", "hair_shader.geom");

    marschnerTex = createMarschnerTexture(256);
    saveMarschnerTexture(marschnerTex, 256, "marschner_texture.png");

    NR_tex = createNR_Texture(256, 1.55f);
    saveNR_Texture(NR_tex, 256, "NR_texture.png");

    NTT_tex = createNTT_Texture(256, 1.55f, absorption);
    saveNTT_Texture(NTT_tex, 256, "NTT_texture.png");

    NTRT_tex = createNTRT_Texture(256, 1.55f, absorption);

    saveNTRT_Texture(NTRT_tex, 256, "NTRT_texture.png");
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Version: " << version << std::endl;
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "GLSL Version: " << glslVersion << std::endl;

    OBJModel headModel = loadOBJ("../hairstyles/woman_head.ply");
    setupObjBuffers(headModel);
    vec3 headcenter = computeMeshCenter(headModel.vertices);
    cameraTarget = headcenter;

    HairModel hairModel;
    setupHairBuffers(hairModel);
    glEnable(GL_DEPTH_TEST); //이게문제 
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    while (!glfwWindowShouldClose(window)) {

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 model = glm::mat4(1.0f);
        
		//mat4 Hairmodel = glm::mat4(1.0f);

        model = glm::rotate(model, radians(-90.0f), vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));
        
        float radYaw = radians(camYaw);
        float radPitch = radians(camPitch);

        vec3 cameraPos;

        cameraPos.x = cameraTarget.x + radius * cos(radPitch) * cos(radYaw);
        cameraPos.y = cameraTarget.y + radius * sin(radPitch);
        cameraPos.z = cameraTarget.z + radius * cos(radPitch) * sin(radYaw);

        mat4 view = lookAt(cameraPos, cameraTarget, vec3(0.0f, 1.0f, 0.0f));
        float aspect = 800.0f / 600.0f;
        float near = 1.0f;
        float far = 1000.0f;
        mat4 projection = perspective(radians(fov), aspect, near, far);
        mat4 MVP = projection * view * model;
        vec3 updatedLightPos(lightPos[0], lightPos[1], lightPos[2]);
        //void renderHair(GLuint shaderProgram, const mat4& MVP, const mat4& model, HairModel& hairmodel, const vec3& cameraPos, const vec3& lightPos) {

        renderOBJ(Obj_shaderProgram, MVP, model, headModel, cameraPos, updatedLightPos);

        renderHair(Hair_shaderProgram, MVP, model, hairModel, cameraPos, updatedLightPos);
        // 이후 다른 렌더링을 위해 상태 복원
        glDepthMask(GL_TRUE);
       
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        showGUI(hairModel);

        if (reloadHair) {
            hairModel = loadHairFile(selectedHairFile);
            setupHairBuffers(hairModel);
            // cameraTarget = computeHairCenter(hairModel);
            reloadHair = false;

        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glDeleteTextures(1, &marschnerTex);
    glDeleteTextures(1, &NR_tex);
    glDeleteTextures(1, &NTT_tex);
    glDeleteTextures(1, &NTRT_tex);

    //glDeleteVertexArrays(1,);

    //glDeleteBuffers(1, &VBO);

    //glDeleteBuffers(1, &EBO);

    glDeleteVertexArrays(1, &hairVAO);
    glDeleteBuffers(1, &hairVBO);
    glDeleteProgram(Obj_shaderProgram);
    glDeleteProgram(Hair_shaderProgram);

    glfwTerminate();

    return 0;
}