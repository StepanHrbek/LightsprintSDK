#include <cassert>
#include <iostream>
#include "Program.h"

using namespace std;

// Public part :

Program::Program()
  :vertex(NULL), fragment(NULL)
{
	handle = glCreateProgram();
	linked = false;
}

Program::Program(const char* defines, const char *shader, unsigned int shaderType)
  :fragment(NULL)
{
	handle = glCreateProgram();
	vertex = new Shader(defines, shader, shaderType);

	attach(vertex);

	linkIt();
}

Program::Program(const char* defines, const char *vertexShader, const char *fragmentShader)
  :vertex(NULL), fragment(NULL)
{
	handle = glCreateProgram();

	vertex = new Shader(defines, vertexShader, GL_VERTEX_SHADER);
	fragment = new Shader(defines, fragmentShader, GL_FRAGMENT_SHADER);

	attach(vertex);
	attach(fragment);

	linkIt();
}

Program::~Program()
{
	if(vertex)
		delete vertex;
	if(fragment)
		delete fragment;
	glDeleteProgram(handle);
}

void Program::attach(Shader &shader)
{
	glAttachShader(handle, shader.getHandle());
}

void Program::attach(Shader *shader)
{
	attach(*shader);
}

void Program::linkIt()
{
	// link
	glLinkProgram(handle);
	GLint alinked;
	glGetProgramiv(handle,GL_LINK_STATUS,&alinked);
	// store result
	linked = alinked!=0;
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
	return valid!=0;
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

void Program::sendUniform(const char *name, float x, float y, float z,
							  float w)
{
	glUniform4f(getLoc(name), x, y, z, w);
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

void Program::sendUniform(const char *name, int x, int y, int z,
							  int w)
{
	glUniform4i(getLoc(name), x, y, z, w);
}

void Program::sendUniform(const char *name, float *matrix, bool transpose,
							  int size)
{
	int loc = getLoc(name);

	switch(size)
	{
	case '2':
		glUniformMatrix2fv(loc, 0, transpose, matrix);
		break;
	case '3':
		glUniformMatrix3fv(loc, 0, transpose, matrix);
		break;
	case '4':
		glUniformMatrix4fv(loc, 0, transpose, matrix);
		break;
	}
}

// Private part :

int Program::getLoc(const char *name)
{
	int loc = glGetUniformLocation(handle, name);
	if(loc == -1)
	{
		cout << name << " is not a valid uniform variable name.\n";
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
