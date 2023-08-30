// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// GLSL shader.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <stdlib.h>
#include "Shader.h"
#include "Lightsprint/RRDebug.h"
#include <sstream>
#include <fstream>

namespace rr_gl
{

#define NUM_LINES 4

Shader* Shader::create(const char* defines, const rr::RRString& filename, GLenum shaderType)
{
	#define SHADOW_HACK "#version 300\n#define shadow2D(a,b) vec4(texture(a,b))\n#define shadow2DProj(a,b) vec4(textureProj(a,b))\n#define textureCube texture\n"
	//#define SHADOW_HACK "#extension GL_EXT_shadow_samplers : require\n#define shadow2D(a,b) vec4(shadow2DEXT(a,b))\n#define shadow2DProj(a,b) vec4(shadow2DProjEXT(a,b))\n"
	const char* source[NUM_LINES];
#ifdef RR_GL_ES2
	bool s_es = true;
#endif
	source[0] = (s_es && strstr(filename.c_str(),"ubershader") ) ? SHADOW_HACK : "";//"#version 110\n"; // 110 is default, should be supported by any gl2.0 implementation. we don't insert it here, so that shader can specify its own version (dof needs 120)
	source[1] = s_es ? "precision highp float;\n" : "";
	source[2] = defines?defines:"";
	try
	{
		std::ifstream ifs(RR_RR2STD(filename),std::ios::in|std::ios::binary);
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		std::string s = buffer.str();
		source[3] = s.c_str();
		if (*source[3])
			return new Shader(filename,source,shaderType);
	}
	catch(...)
	{
	}
	rr::RRReporter::report(rr::ERRO,"Shader %ls not found.\n",filename.w_str());
	return nullptr;
}

Shader::Shader(const rr::RRString& filenameDiagnosticOnly, const GLchar** source, GLenum shaderType)
{
	handle = glCreateShader(shaderType);
	glShaderSource(handle, NUM_LINES, source, nullptr);
	compile(filenameDiagnosticOnly);
}

void Shader::compile(const rr::RRString& filenameDiagnosticOnly)
{
	GLint compiled;

	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		rr::RRReporter::report(rr::ERRO,"%ls compilation failed:\n",filenameDiagnosticOnly.w_str());

		GLchar* debug;
		GLint debugLength;
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &debugLength);

		debug = new GLchar[debugLength];
		glGetShaderInfoLog(handle, debugLength, &debugLength, debug);

		rr::RRReporter::report(rr::ERRO,debug);
		delete [] debug;
	}
}

GLuint Shader::getHandle() const
{
	return handle;
}

Shader::~Shader()
{
	glDeleteShader(handle);
}

}; // namespace
