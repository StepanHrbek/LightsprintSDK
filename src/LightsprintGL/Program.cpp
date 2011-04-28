// --------------------------------------------------------------------------
// Program, OpenGL 2.0 object with optional vertex and fragment shaders.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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

Program::Program(const char* defines, const char *vertexShader, const char *fragmentShader)
{
	handle = glCreateProgram();
	vertex = NULL;
	fragment = NULL;
	linked = 0;

	if (s_showLog)
	{
		rr::RRReporter::report(rr::INF2,"Building %s + %s\n",vertexShader,fragmentShader);
		if (defines && defines[0]) rr::RRReporter::report(rr::INF2,"%s",defines);
	}

	if (vertexShader)
	{
		vertex = Shader::create(defines, vertexShader, GL_VERTEX_SHADER);
		if (!vertex) goto end;
		glAttachShader(handle, vertex->getHandle());
	}

	if (fragmentShader)
	{
		fragment = Shader::create(defines, fragmentShader, GL_FRAGMENT_SHADER);
		if (!fragment) goto end;
		glAttachShader(handle, fragment->getHandle());
	}

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
			GLchar *debug = new GLchar[debugLength];
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

Program* Program::create(const char* defines, const char* vertexShader, const char* fragmentShader)
{
	Program* res = new Program(defines,vertexShader,fragmentShader);
	if (!res->isLinked()) RR_SAFE_DELETE(res);
	return res;
}

bool Program::logLooksSafe()
{
	bool result = true;
	GLchar *debug;
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

unsigned Program::sendTexture(const char *name, const Texture* t, int code)
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

void Program::sendUniform(const char *name, float x)
{
	glUniform1f(getLoc(name), x);
}

void Program::sendUniform(const char *name, float x, float y)
{
	glUniform2f(getLoc(name), x, y);
}

void Program::sendUniform(const char *name, float x, float y, float z)
{
	glUniform3f(getLoc(name), x, y, z);
}

void Program::sendUniform3fv(const char *name, const float xyz[3])
{
	glUniform3fv(getLoc(name), 1, xyz);
}

void Program::sendUniform(const char *name, float x, float y, float z, float w)
{
	glUniform4f(getLoc(name), x, y, z, w);
}

void Program::sendUniform4fv(const char *name, const float xyzw[4])
{
	glUniform4fv(getLoc(name), 1, xyzw);
}

void Program::sendUniform(const char *name, int count, const GLint* x)
{
	glUniform1iv(getLoc(name), count, x);
}

void Program::sendUniform(const char *name, int x)
{
	glUniform1i(getLoc(name), x);
}

void Program::sendUniform(const char *name, int x, int y)
{
	glUniform2i(getLoc(name), x, y);
}

void Program::sendUniform(const char *name, int x, int y, int z)
{
	glUniform3i(getLoc(name), x, y, z);
}

void Program::sendUniform(const char *name, int x, int y, int z, int w)
{
	glUniform4i(getLoc(name), x, y, z, w);
}

void Program::sendUniform(const char *name, float *matrix, bool transpose, int size)
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

int Program::getLoc(const char *name)
{
	int loc = glGetUniformLocation(handle, name);
	if (loc == -1)
	{
		RR_LIMITED_TIMES(10,rr::RRReporter::report(rr::ERRO,"Variable %s disappeared from shader. This is usually graphics card driver error.\n",name));
	}
	return loc;
}

bool Program::uniformExists(const char *uniformName)
{
	return glGetUniformLocation(handle, uniformName)!=-1;
}

const char* getTypeName(unsigned type)
{
	unsigned types[] = {GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT_VEC4, GL_INT, GL_INT_VEC2, GL_INT_VEC3, GL_INT_VEC4,
		GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4, GL_FLOAT_MAT2, GL_FLOAT_MAT3,
		GL_FLOAT_MAT4, GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE,
		GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW };
	const char* typeNames[] = {"FLOAT", "FLOAT_VEC2", "FLOAT_VEC3", "FLOAT_VEC4", "INT", "INT_VEC2", "INT_VEC3", "INT_VEC4",
		"BOOL", "BOOL_VEC2", "BOOL_VEC3", "BOOL_VEC4", "FLOAT_MAT2", "FLOAT_MAT3",
		"FLOAT_MAT4", "SAMPLER_1D", "SAMPLER_2D", "SAMPLER_3D", "SAMPLER_CUBE",
		"SAMPLER_1D_SHADOW", "SAMPLER_2D_SHADOW"};
	for (unsigned i=0;i<sizeof(types)/sizeof(types[0]);i++)
		if (types[i]==type)
			return typeNames[i];
	return "?";
}

void Program::enumVariables()
{
	GLint count = 0;
	glGetProgramiv(handle,GL_ACTIVE_ATTRIBUTES,&count);
	for (int i=0;i<count;i++)
	{
		char name[64] = "";
		GLint size = 0;
		GLenum type = 0;
		glGetActiveAttrib(handle,i,50,NULL,&size,&type,name);
		printf(" attrib  %s %d %s\n",name,(int)size,getTypeName(type));
	}
	glGetProgramiv(handle,GL_ACTIVE_UNIFORMS,&count);
	for (int i=0;i<count;i++)
	{
		char name[64] = "";
		GLint size = 0;
		GLenum type = 0;
		glGetActiveUniform(handle,i,50,NULL,&size,&type,name);
		printf(" uniform %s %d %s\n",name,(int)size,getTypeName(type));
	}
}

}; // namespace
