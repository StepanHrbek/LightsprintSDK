// --------------------------------------------------------------------------
// DemoEngine
// Program, OpenGL 2.0 object with optional vertex and fragment shaders.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef PROGRAM_H
#define PROGRAM_H

#include "DemoEngine.h"
#include "Shader.h"


/////////////////////////////////////////////////////////////////////////////
//
// Program

class RR_API Program
{
public:
	Program();
	Program(const char* defines, const char* shader, unsigned int shaderType=GL_VERTEX_SHADER);
	Program(const char* defines, const char* vertexShader, const char* fragmentShader);
	~Program();

	void attach(Shader &shader);
	void attach(Shader *shader);
	void linkIt();
	bool isLinked(); // true when successfully linked
	bool isValid(); // true when successfully linked and all variables correctly set
	void useIt(); // must be ready before use
	void enumVariables();

	void sendUniform(const char *name, float x);
	void sendUniform(const char *name, float x, float y);
	void sendUniform(const char *name, float x, float y, float z);
	void sendUniform(const char *name, float x, float y, float z, float w);
	void sendUniform(const char *name, int count, const GLint* x);
	void sendUniform(const char *name, int x);
	void sendUniform(const char *name, int x, int y);
	void sendUniform(const char *name, int x, int y, int z);
	void sendUniform(const char *name, int x, int y, int z, int w);
	void sendUniform(const char *name, float *m, bool transp=false, int size=4);

private:
	int getLoc(const char *name);

	Shader *vertex, *fragment;
	GLuint handle;
	bool linked;
};

#endif
