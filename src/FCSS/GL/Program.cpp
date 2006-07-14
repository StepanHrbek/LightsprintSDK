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

