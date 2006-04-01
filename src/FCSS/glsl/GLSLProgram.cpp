#include "GLSLProgram.hpp"
#include <iostream>
#include <GL/glut.h>
#include "GLSLShader.hpp"

using namespace std;

// Public part :

GLSLProgram::GLSLProgram()
  :vertex(NULL), fragment(NULL)
{
	handle = glCreateProgramObjectARB();
	linked = false;
}

GLSLProgram::GLSLProgram(const char* defines, const char *shader, unsigned int shaderType)
  :fragment(NULL)
{
	handle = glCreateProgramObjectARB();
	vertex = new GLSLShader(defines, shader, shaderType);

	attach(vertex);

	linked = linkIt();
}

extern void checkGlError();

GLSLProgram::GLSLProgram(const char* defines, const char *vertexShader, const char *fragmentShader)
  :vertex(NULL), fragment(NULL)
{
	checkGlError();
	handle = glCreateProgramObjectARB();

	vertex = new GLSLShader(defines, vertexShader, GL_VERTEX_SHADER_ARB);
	fragment = new GLSLShader(defines, fragmentShader, GL_FRAGMENT_SHADER_ARB);

	attach(vertex);
	attach(fragment);

	linked = linkIt();
}

GLSLProgram::~GLSLProgram()
{
	if(vertex)
		delete vertex;
	if(fragment)
		delete fragment;
}

void GLSLProgram::attach(GLSLShader &shader)
{
	glAttachObjectARB(handle, shader.getHandle());
}

void GLSLProgram::attach(GLSLShader *shader)
{
	attach(*shader);
}

bool GLSLProgram::linkIt()
{
	glLinkProgramARB(handle);
	GLint inked;
	glGetProgramiv(handle,GL_LINK_STATUS,&inked);
	/*if(!linked)
	{
		GLcharARB *debug;
		GLint debugLength;
		glGetProgramiv(handle,GL_INFO_LOG_LENGTH, &debugLength);

		debug = new GLcharARB[debugLength];
		glGetProgramInfoLog(handle,debugLength,&debugLength,debug);

		cout << debug;
		delete [] debug;
	}*/
	return inked!=0;
}

void GLSLProgram::useIt()
{
	glUseProgramObjectARB(handle);
}

void GLSLProgram::sendUniform(const char *name, float x)
{
	glUniform1fARB(getLoc(name), x);
}

void GLSLProgram::sendUniform(const char *name, float x, float y)
{
	glUniform2fARB(getLoc(name), x, y);
}

void GLSLProgram::sendUniform(const char *name, float x, float y, float z)
{
	glUniform3fARB(getLoc(name), x, y, z);
}

void GLSLProgram::sendUniform(const char *name, float x, float y, float z,
							  float w)
{
	glUniform4fARB(getLoc(name), x, y, z, w);
}

void GLSLProgram::sendUniform(const char *name, int count, const GLint* x)
{
	glUniform1ivARB(getLoc(name), count, x);
}

void GLSLProgram::sendUniform(const char *name, int x)
{
	glUniform1iARB(getLoc(name), x);
}

void GLSLProgram::sendUniform(const char *name, int x, int y)
{
	glUniform2iARB(getLoc(name), x, y);
}

void GLSLProgram::sendUniform(const char *name, int x, int y, int z)
{
	glUniform3iARB(getLoc(name), x, y, z);
}

void GLSLProgram::sendUniform(const char *name, int x, int y, int z,
							  int w)
{
	glUniform4iARB(getLoc(name), x, y, z, w);
}

void GLSLProgram::sendUniform(const char *name, float *matrix, bool transpose,
							  int size)
{
	int loc = getLoc(name);

	switch(size)
	{
	case '2':
		glUniformMatrix2fvARB(loc, 0, transpose, matrix);
		break;
	case '3':
		glUniformMatrix3fvARB(loc, 0, transpose, matrix);
		break;
	case '4':
		glUniformMatrix4fvARB(loc, 0, transpose, matrix);
		break;
	}
}

// Private part :

int GLSLProgram::getLoc(const char *name)
{
	int loc = glGetUniformLocationARB(handle, name);
	if(loc == -1)
	{
		cout << name << " is not a valid uniform variable name.\n";
		fgetc(stdin);
		exit(0);
	}
	return loc;
}

GLSLProgramSet::GLSLProgramSet(const char* avertexShaderFileName, const char* afragmentShaderFileName)
{
	vertexShaderFileName = avertexShaderFileName;
	fragmentShaderFileName = afragmentShaderFileName;
}

GLSLProgramSet::~GLSLProgramSet()
{
}

GLSLProgram* GLSLProgramSet::getVariant(const char* defines)
{
	unsigned hash = 0;
	for(const char* tmp = defines;tmp && *tmp;tmp++)
	{
		hash=(hash<<5)+(hash>>27)+*tmp;
	}
	map<unsigned,GLSLProgram*>::iterator i = cache.find(hash);
	if(i!=cache.end()) return i->second;
	GLSLProgram* tmp = new GLSLProgram(defines,vertexShaderFileName,fragmentShaderFileName);
	if(!tmp->linked)
	{
		delete tmp;
		tmp=NULL;
	}
	cache[hash] = tmp;
	//cache.insert(defines,tmp);
	return tmp;
}
