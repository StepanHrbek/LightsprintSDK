#include <fstream>
#include <iostream>
//#include <stdio.h>
#include "GLSLShader.hpp"

using namespace std;

// Public part :

GLSLShader::GLSLShader(const char* defines, const char* filename, GLenum shaderType)
{
  const char *source[2];
  handle = glCreateShaderObjectARB(shaderType);

  source[0] = defines?defines:"";
  source[1] = readShader(filename);
  glShaderSourceARB(handle, 2, (const GLcharARB**)source, NULL);
  
  compileIt();
  
  delete [] source[1];
}

void GLSLShader::compileIt()
{
	GLint compiled;

	glCompileShaderARB(handle);
	getParameteri(GL_OBJECT_COMPILE_STATUS_ARB, &compiled);

	if(!compiled)
	{
		GLcharARB *debug;
		GLint debugLength;
		getParameteri(GL_OBJECT_INFO_LOG_LENGTH_ARB, &debugLength);

		debug = new GLcharARB[debugLength];
		glGetInfoLogARB(handle, debugLength, &debugLength, debug);

		cout << debug;
		delete [] debug;
	}
}

// Private part :

char *GLSLShader::readShader(const char *filename)
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
/*
  ifstream temp(filename);
  unsigned count = 0;
  char *buf;
  
  temp.seekg(0, ios::end);
  count = temp.tellg();
  
  buf = new char[count + 1];
  for(unsigned i=0;i<count+1;i++) buf[i]=0; // necessary in release run outside ide or debugger
  temp.seekg(0, ios::beg);
  temp.read(buf, count);
  temp.close();
  buf[count] = 0;

  cout << "[BEGIN:" << filename << "]" << buf << "[END]";
  return buf;*/
}
