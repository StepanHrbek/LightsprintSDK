// --------------------------------------------------------------------------
// DemoEngine
// Shader, OpenGL 2.0 object.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include "Shader.h"

namespace rr_gl
{

char* readShader(const char *filename)
{
	FILE* f = fopen(filename,"rb");
	if(!f) return NULL;
	fseek(f,0,SEEK_END);
	unsigned count = ftell(f);
	char *buf;
	buf = new char[count + 1];
	fseek(f,0,SEEK_SET);
	fread(buf,1,count,f);
	fclose(f);
	buf[count] = 0;
	return buf;
}

Shader::Shader(const char* defines, const char* filename, GLenum shaderType)
{
	const char *source[3];
	handle = glCreateShader(shaderType);

	source[0] = "#version 110\n";
	source[1] = defines?defines:"";
	source[2] = readShader(filename);
	if(!source[2])
	{
		printf("Shader %s not found.\nPress enter to end...",filename);
		fgetc(stdin);
		exit(1);
	}
	glShaderSource(handle, 3, (const GLchar**)source, NULL);
  
	compile();
  
	delete[] source[2];
}

void Shader::compile()
{
	GLint compiled;

	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);

	if(!compiled)
	{
		GLchar *debug;
		GLint debugLength;
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &debugLength);

		debug = new GLchar[debugLength];
		glGetShaderInfoLog(handle, debugLength, &debugLength, debug);

		printf(debug);
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
