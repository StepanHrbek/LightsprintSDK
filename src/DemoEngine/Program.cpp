// --------------------------------------------------------------------------
// DemoEngine
// Program, OpenGL 2.0 object with optional vertex and fragment shaders.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Lightsprint/DemoEngine/Program.h"
#include "Shader.h"

namespace de
{

bool Program::showLog = false;

Program::Program(const char* defines, const char *vertexShader, const char *fragmentShader)
  :vertex(NULL), fragment(NULL)
{
	handle = glCreateProgram();

	if(vertexShader)
	{
		vertex = new Shader(defines, vertexShader, GL_VERTEX_SHADER);
		glAttachShader(handle, vertex->getHandle());
	}

	if(fragmentShader)
	{
		fragment = new Shader(defines, fragmentShader, GL_FRAGMENT_SHADER);
		glAttachShader(handle, fragment->getHandle());
	}

	// link
	glLinkProgram(handle);
	GLint alinked;
	glGetProgramiv(handle,GL_LINK_STATUS,&alinked);
	// store result
	linked = alinked && logLooksSafe();

	if(showLog)
	{
		GLint debugLength;
		glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &debugLength);
		if(debugLength>2)
		{
			printf("Vertex: %s\n",vertexShader);
			printf("Fragment: %s\n",fragmentShader);
			if(defines && defines[0]) printf("Defines: %s",defines);
			GLchar *debug = new GLchar[debugLength];
			glGetProgramInfoLog(handle, debugLength, &debugLength, debug);
			printf("Log: %s\n\n",debug);
			delete[] debug;
		}
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
	if(!res->isLinked()) SAFE_DELETE(res);
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
	// when log contains "software", program probably can't run on GPU
	if(strstr(debug,"software"))
	{
		//printf("Sw shader refused.\n");
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
	if(loc == -1)
	{
		printf("\n%s is not a valid uniform variable name.\n",name);
		printf("This is usually caused by error in graphics card driver.\n");
		printf("Make sure you have the latest official driver for your graphics card.\n");
		printf("As a workaround, run application with -hard parameter on command line.\n");
		fgetc(stdin);
		exit(0);
	}
	return loc;
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
	for(unsigned i=0;i<sizeof(types)/sizeof(types[0]);i++)
		if(types[i]==type)
			return typeNames[i];
	return "?";
}

void Program::enumVariables()
{
	GLint count = 0;
	glGetProgramiv(handle,GL_ACTIVE_ATTRIBUTES,&count);
	for(int i=0;i<count;i++)
	{
		char name[64] = "";
		int size = 0;
		GLenum type = 0;
		glGetActiveAttrib(handle,i,50,NULL,&size,&type,name);
		printf(" attrib  %s %d %s\n",name,size,getTypeName(type));
	}
	glGetProgramiv(handle,GL_ACTIVE_UNIFORMS,&count);
	for(int i=0;i<count;i++)
	{
		char name[64] = "";
		int size = 0;
		GLenum type = 0;
		glGetActiveUniform(handle,i,50,NULL,&size,&type,name);
		printf(" uniform %s %d %s\n",name,size,getTypeName(type));
	}
}

}; // namespace
