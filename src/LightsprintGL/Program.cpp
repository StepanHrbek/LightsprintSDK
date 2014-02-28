// --------------------------------------------------------------------------
// Program, OpenGL 2.0 object with optional vertex and fragment shaders.
// Copyright (C) 2005-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/GL/Texture.h"
#include "Shader.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

static bool s_showLog = false;

void Program::logMessages(bool enable)
{
	s_showLog = enable;
}

Program::Program(const char* defines, const rr::RRString& vertexShader, const rr::RRString& fragmentShader)
{
	handle = glCreateProgram();
	vertex = NULL;
	fragment = NULL;
	linked = 0;

	if (s_showLog)
	{
		rr::RRReporter::report(rr::INF2,"Building %ls + %ls\n",vertexShader.w_str(),fragmentShader.w_str());
		if (defines && defines[0]) rr::RRReporter::report(rr::INF2,"%s",defines);
	}

	if (!vertexShader.empty())
	{
		vertex = Shader::create(defines, vertexShader, GL_VERTEX_SHADER);
		if (!vertex) goto end;
		glAttachShader(handle, vertex->getHandle());
	}

	if (!fragmentShader.empty())
	{
		fragment = Shader::create(defines, fragmentShader, GL_FRAGMENT_SHADER);
		if (!fragment) goto end;
		glAttachShader(handle, fragment->getHandle());
	}

	// hardcoded locations for attributes used by shaders
	glBindAttribLocation(handle, VAA_POSITION, "vertexPosition");
	glBindAttribLocation(handle, VAA_NORMAL, "vertexNormal");
	glBindAttribLocation(handle, VAA_UV_MATERIAL_DIFFUSE, "vertexUvDiffuse");
	glBindAttribLocation(handle, VAA_UV_MATERIAL_SPECULAR, "vertexUvSpecular");
	glBindAttribLocation(handle, VAA_UV_MATERIAL_EMISSIVE, "vertexUvEmissive");
	glBindAttribLocation(handle, VAA_UV_MATERIAL_TRANSPARENT, "vertexUvTransparent");
	glBindAttribLocation(handle, VAA_UV_MATERIAL_BUMP, "vertexUvBump");
	glBindAttribLocation(handle, VAA_UV_UNWRAP, "vertexUvUnwrap");
	glBindAttribLocation(handle, VAA_UV_FORCED_2D, "vertexUvForced2D");
	glBindAttribLocation(handle, VAA_COLOR, "vertexColor");
	glBindAttribLocation(handle, VAA_TANGENT, "vertexTangent");
	glBindAttribLocation(handle, VAA_BITANGENT, "vertexBitangent");

	// link
	glLinkProgram(handle);
	GLint alinked;
	glGetProgramiv(handle,GL_LINK_STATUS,&alinked);
	// store result
	linked = alinked && logLooksSafe();

	if (s_showLog)
	{
		GLint debugLength;
		glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &debugLength);
		if (!alinked)
			rr::RRReporter::report(rr::INF2,"Link failed%c\n",(debugLength>2)?':':'.');
		if (debugLength>2)
		{
			GLchar* debug = new GLchar[debugLength];
			glGetProgramInfoLog(handle, debugLength, &debugLength, debug);
			rr::RRReporter::report(rr::INF2,"%s\n",debug);
			delete[] debug;
		}
	}
end:
	if (s_showLog)
	{
		rr::RRReporter::report(rr::INF2,"\n");
	}

}

Program::~Program()
{
	delete vertex;
	delete fragment;
	glDeleteProgram(handle);
}

Program* Program::create(const char* defines, const rr::RRString& vertexShader, const rr::RRString& fragmentShader)
{
	Program* res = new Program(defines,vertexShader,fragmentShader);
	if (!res->isLinked()) RR_SAFE_DELETE(res);
	return res;
}

bool Program::logLooksSafe()
{
	bool result = true;
	GLchar* debug;
	GLint debugLength;
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &debugLength);
	debug = new GLchar[debugLength];
	glGetProgramInfoLog(handle, debugLength, &debugLength, debug);
	// when log contains "software" but not "hardware",
	//  program probably can't run on GPU
	// it's better to fail than render first frame for 30 seconds
	// Radeon X1250 runs fine, log contains both software and hardware
	if (strstr(debug,"software") && !strstr(debug,"hardware"))
	{
		//rr::RRReporter::report(rr::WARN,"Sw shader detected.\n");
		result = false;
	}
	delete [] debug;
	return result;
}

bool Program::isLinked()
{
	return linked;
}

bool Program::isValid()
{
	// validate
	glValidateProgram(handle);
	GLint valid;
	glGetProgramiv(handle,GL_VALIDATE_STATUS,&valid);
	return valid && logLooksSafe();
}

void Program::useIt()
{
	glUseProgram(handle);
	nextTextureUnit = 0;
	memset(assignedTextureUnit,-1,sizeof(assignedTextureUnit));
}

unsigned Program::sendTexture(const char* name, const Texture* t, int code)
{
	unsigned oldNextTextureUnit = nextTextureUnit;
	unsigned textureUnit = (code>=0 && code<16)
		? ( (assignedTextureUnit[code]<0) ? assignedTextureUnit[code] = nextTextureUnit++ : assignedTextureUnit[code] )
		: nextTextureUnit++;
	if (nextTextureUnit!=oldNextTextureUnit) // set only what was not set yet
		glUniform1i(getLoc(name),textureUnit);
	glActiveTexture(GL_TEXTURE0+textureUnit);
	if (t) t->bindTexture();
	return textureUnit;
}

void Program::unsendTexture(int code)
{
	if (code<0 || code>=16)
	{
		rr::RRReporter::report(rr::WARN,"Calling unsendTexture(%d), but only 0..15 allowed as a parameter.\n",code);
		return;
	}
	if (assignedTextureUnit[code]<0)
	{
		// material was not set but we try to unset it
		// it happens when RendererOfMesh skips whole facegroup because all fragments would be dropped in alpha-test
		//rr::RRReporter::report(rr::WARN,"Calling unsendTexture(%d), but never called sendTexture(,,%d) before.\n",code,code);
		return;
	}
	glActiveTexture(GL_TEXTURE0+assignedTextureUnit[code]);
	glBindTexture(GL_TEXTURE_2D,0);
}

void Program::sendUniform(const char* name, float x)
{
	glUniform1f(getLoc(name), x);
}

void Program::sendUniform(const char* name, float x, float y)
{
	glUniform2f(getLoc(name), x, y);
}

void Program::sendUniform(const char* name, const rr::RRVec3& xyz)
{
	glUniform3fv(getLoc(name), 1, &xyz[0]);
}

void Program::sendUniform(const char* name, const rr::RRVec4& xyzw)
{
	glUniform4fv(getLoc(name), 1, &xyzw[0]);
}

void Program::sendUniform(const char* name, int x)
{
	glUniform1i(getLoc(name), x);
}

void Program::sendUniform(const char* name, int x, int y)
{
	glUniform2i(getLoc(name), x, y);
}

void Program::sendUniform(const char* name, const float *matrix, bool transpose, int size)
{
	int loc = getLoc(name);

	switch(size)
	{
	case 2:
		glUniformMatrix2fv(loc, 1, transpose, matrix);
		break;
	case 3:
		glUniformMatrix3fv(loc, 1, transpose, matrix);
		break;
	case 4:
		glUniformMatrix4fv(loc, 1, transpose, matrix);
		break;
	}
}

void Program::sendUniformArray(const char* name, int count, const GLint* x)
{
	glUniform1iv(getLoc(name), count, x);
}

void Program::sendUniformArray(const char* name, int count, const GLfloat* x)
{
	glUniform1fv(getLoc(name), count, x);
}

void Program::sendUniformArray(const char* name, int count, const rr::RRVec2* x)
{
	glUniform2fv(getLoc(name), count, (const GLfloat*)x);
}

void Program::sendUniformArray(const char* name, int count, const rr::RRVec3* x)
{
	glUniform3fv(getLoc(name), count, (const GLfloat*)x);
}

void Program::sendUniformArray(const char* name, int count, const rr::RRVec4* x)
{
	glUniform4fv(getLoc(name), count, (const GLfloat*)x);
}

int Program::getLoc(const char* name)
{
	int loc = glGetUniformLocation(handle, name);
	if (loc == -1)
	{
		RR_LIMITED_TIMES(10,rr::RRReporter::report(rr::ERRO,"Variable %s disappeared from shader. It is either driver error or renderer error.\n",name));
	}
	return loc;
}

bool Program::uniformExists(const char* uniformName)
{
	return glGetUniformLocation(handle, uniformName)!=-1;
}


}; // namespace
