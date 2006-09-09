#include <cstdio>
#include <fstream>
#include <iostream>
#include "DemoEngine/Shader.h"

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

// Public part :

Shader::Shader(const char* defines, const char* filename, GLenum shaderType)
{
  const char *source[2];
  handle = glCreateShader(shaderType);

  source[0] = defines?defines:"";
  source[1] = readShader(filename);
  if(!source[1])
  {
	  printf("Shader %s not found.\nPress enter to end...",filename);
	  fgetc(stdin);
	  exit(1);
  }
  glShaderSource(handle, 2, (const GLcharARB**)source, NULL);
  
  compileIt();
  
  delete [] source[1];
}

void Shader::compileIt()
{
	GLint compiled;

	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);

	if(!compiled)
	{
		GLcharARB *debug;
		GLint debugLength;
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &debugLength);

		debug = new GLcharARB[debugLength];
		glGetShaderInfoLog(handle, debugLength, &debugLength, debug);

		std::cout << debug;
		delete [] debug;
	}
}

// Private part :

GLuint Shader::getHandle()
{
	return handle;
}

Shader::~Shader()
{
	glDeleteShader(handle);
}

