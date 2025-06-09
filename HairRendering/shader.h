//
//  Created by Hyun Joon Shin on 13/03/2018.
//  Copyright ⓒ 2018 Hyun Joon Shin. All rights reserved.
//

#ifndef SHADER_H
#define SHADER_H

#ifdef WIN32
#include <Windows.h>
#define GLEW_STATIC
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#elif defined __APPLE__
#pragma clang diagnostic ignored "-Wdocumentation"
#include <OpenGL/gl3.h>
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <GL/glew.h>
#include <glm/glm.hpp>

#ifdef WIN32
typedef wchar_t CHAR_T;
#include <windows.h>
inline std::wstring utf82Unicode(const std::string& s) {
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr, len);
	wstr[len] = L'\0';
	std::wstring ws(L""); ws += wstr;
	delete wstr;
	return ws;
}
#else
typedef char CHAR_T;
#define utf82Unicode(X) (X)
#endif

inline std::string getFilenameFromAbsPath(const std::string& filename)
{
	size_t slashPos = filename.find_last_of('/');
	if (slashPos == std::string::npos) return filename;
	if (slashPos == filename.length() - 1) return "";
	return filename.substr(slashPos + 1);
}


inline std::string loadText(const std::string& filename) {
	std::ifstream t(utf82Unicode(filename));
	if (!t.is_open()) {
		std::cerr << "[ERROR] Text file: " << getFilenameFromAbsPath(filename) << " is not found\n";
		return "";
	}
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	return str;
}

static inline void printInfoProgramLog(GLuint obj)
{
	int infologLength = 0, charsWritten = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength <= 0) return;
	std::cerr << "Program Info:" << std::endl;
	char* infoLog = new char[infologLength];
	glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
	std::cerr << infoLog << std::endl;
	delete[] infoLog;
}
static inline void printInfoShaderLog(GLuint obj)
{
	int infologLength = 0, charsWritten = 0;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength <= 0) return;
	std::cerr << "Shader Info:" << std::endl;
	char* infoLog = new char[infologLength];
	glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
	std::cerr << infoLog << std::endl;
	delete[] infoLog;
}

inline GLuint loadShaders(const char* vsFilename, const char* fsFilename, const char* gsFilename = nullptr) {
	GLuint vertShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint geomShaderID = 0;
	GLuint programID = glCreateProgram();

	// Load vertex shader
	std::string vertCode = loadText(vsFilename);
	if (vertCode.empty()) {
		std::cerr << "[ERROR] Vertex shader code is not loaded properly" << std::endl;
		return 0;
	}
	const GLchar* vshaderCode = vertCode.c_str();
	glShaderSource(vertShaderID, 1, &vshaderCode, nullptr);
	glCompileShader(vertShaderID);
	printInfoShaderLog(vertShaderID);
	glAttachShader(programID, vertShaderID);

	// Load fragment shader
	std::string fragCode = loadText(fsFilename);
	if (fragCode.empty()) {
		std::cerr << "[ERROR] Fragment shader code is not loaded properly" << std::endl;
		return 0;
	}
	const GLchar* fshaderCode = fragCode.c_str();
	glShaderSource(fragShaderID, 1, &fshaderCode, nullptr);
	glCompileShader(fragShaderID);
	printInfoShaderLog(fragShaderID);
	glAttachShader(programID, fragShaderID);

	// (Optional) Load geometry shader
	if (gsFilename) {
		geomShaderID = glCreateShader(GL_GEOMETRY_SHADER);
		std::string geomCode = loadText(gsFilename);
		if (geomCode.empty()) {
			std::cerr << "[ERROR] Geometry shader code is not loaded properly" << std::endl;
			return 0;
		}
		const GLchar* gshaderCode = geomCode.c_str();
		glShaderSource(geomShaderID, 1, &gshaderCode, nullptr);
		glCompileShader(geomShaderID);
		printInfoShaderLog(geomShaderID);
		glAttachShader(programID, geomShaderID);
	}

	// Link shader program
	glLinkProgram(programID);
	glUseProgram(programID);
	printInfoProgramLog(programID);

	// Clean up
	glDeleteShader(vertShaderID);
	glDeleteShader(fragShaderID);
	if (gsFilename)
		glDeleteShader(geomShaderID);

	return programID;
}
inline GLuint createShaderProgram_Unlinked(const char* vsFilename, const char* fsFilename, const char* gsFilename = nullptr) {
	GLuint vertShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint geomShaderID = 0;
	GLuint programID = glCreateProgram();

	// Load vertex shader
	std::string vertCode = loadText(vsFilename);
	if (vertCode.empty()) {
		std::cerr << "[ERROR] Vertex shader code is not loaded properly" << std::endl;
		return 0;
	}
	const GLchar* vshaderCode = vertCode.c_str();
	glShaderSource(vertShaderID, 1, &vshaderCode, nullptr);
	glCompileShader(vertShaderID);
	printInfoShaderLog(vertShaderID);
	glAttachShader(programID, vertShaderID);

	// Load fragment shader
	std::string fragCode = loadText(fsFilename);
	if (fragCode.empty()) {
		std::cerr << "[ERROR] Fragment shader code is not loaded properly" << std::endl;
		return 0;
	}
	const GLchar* fshaderCode = fragCode.c_str();
	glShaderSource(fragShaderID, 1, &fshaderCode, nullptr);
	glCompileShader(fragShaderID);
	printInfoShaderLog(fragShaderID);
	glAttachShader(programID, fragShaderID);

	// Optional geometry shader
	if (gsFilename) {
		geomShaderID = glCreateShader(GL_GEOMETRY_SHADER);
		std::string geomCode = loadText(gsFilename);
		if (geomCode.empty()) {
			std::cerr << "[ERROR] Geometry shader code is not loaded properly" << std::endl;
			return 0;
		}
		const GLchar* gshaderCode = geomCode.c_str();
		glShaderSource(geomShaderID, 1, &gshaderCode, nullptr);
		glCompileShader(geomShaderID);
		printInfoShaderLog(geomShaderID);
		glAttachShader(programID, geomShaderID);
	}

	// ❌ NO glLinkProgram here

	return programID;
}

#endif