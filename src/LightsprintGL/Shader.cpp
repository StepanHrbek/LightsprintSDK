// --------------------------------------------------------------------------
// Shader, OpenGL 2.0 object.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <stdlib.h>
#include "Shader.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

char* readShader(const char *filename)
{
	FILE* f = fopen(filename,"rb");
	if (!f) return NULL;
	fseek(f,0,SEEK_END);
	unsigned count = ftell(f);
	char *buf;
	buf = new char[count + 1];
	fseek(f,0,SEEK_SET);
	count = fread(buf,1,count,f);
	fclose(f);
	buf[count] = 0;
	return buf;
}

Shader* Shader::create(const char* defines, const char* filename, GLenum shaderType)
{

// HACK: is MESA library present? (PS3 Linux)
#if !defined(_WIN32) && defined(__PPC__)
#define MESA_VERSION
#endif

#ifdef MESA_VERSION

	// MESA preprocessor is buggy, therefore we use GCC preprocessor instead

	const char* shaderDefs = defines ? defines : "";

	FILE* f = fopen(filename, "rb");
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long shaderFileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	int shaderSrcSize = strlen(shaderDefs) + shaderFileSize + 2;
	char* shaderSrc = new char[shaderSrcSize];
	memset(shaderSrc, 0, shaderSrcSize);

	sprintf(shaderSrc, "%s\n", shaderDefs);
	fread(shaderSrc + strlen(shaderSrc), shaderFileSize, 1, f);
	fclose(f);

	if (!strncmp(shaderSrc, "#version", 8))
	{
		// remove "#version" tag which GCC preprocessor does not recognize
		char* p = shaderSrc;
		while (*p != 0x0A && *p != 0x00) *p++ = ' ';
	}

	char* srcName = "shader.s";
	char* parsedSrcName = "shader.i";
/*
	static int shaderNum = -1;
	shaderNum++;

	printf("SHADER %02d: %s\n%s\n", shaderNum, filename, shaderDefs);

	char srcName[256];
	sprintf(srcName, "ubershader.s%02d", shaderNum);

	char parsedSrcName[256];
	sprintf(parsedSrcName, "ubershader.p%02d", shaderNum);
*/
	f = fopen(srcName, "wb");
	fwrite(shaderSrc, strlen(shaderSrc), 1, f);
	fclose(f);

	delete [] shaderSrc;

	char shellCmd[256];
	sprintf(shellCmd, "gcc -E -P -x c %s > %s", srcName, parsedSrcName);
	system(shellCmd);

	// C parser doesn't recognize "#version" tag

	const char *source[3];
	source[0] = "#version 110\n";
	source[1] = "";
	source[2] = readShader(parsedSrcName);

	sprintf(shellCmd, "rm %s", parsedSrcName);
	system(shellCmd);

#else // MESA_VERSION

	const char *source[3];
	source[0] = "#version 110\n";
	source[1] = defines?defines:"";
	source[2] = readShader(filename);

#endif // MESA_VERSION

	if (!source[2])
	{
		rr::RRReporter::report(rr::ERRO,"Shader %s not found.\n",filename);
		return NULL;
	}
	return new Shader(filename,source,shaderType);
}

Shader::Shader(const char* filenameDiagnosticOnly, const GLchar** source, GLenum shaderType)
{
	handle = glCreateShader(shaderType);
	glShaderSource(handle, 3, (const GLchar**)source, NULL);
	compile(filenameDiagnosticOnly);
	delete[] source[2];
}

void Shader::compile(const char* filenameDiagnosticOnly)
{
	GLint compiled;

	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		rr::RRReporter::report(rr::ERRO,"%s compilation failed:\n",filenameDiagnosticOnly);

		GLchar *debug;
		GLint debugLength;
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &debugLength);

		debug = new GLchar[debugLength];
		glGetShaderInfoLog(handle, debugLength, &debugLength, debug);

		rr::RRReporter::report(rr::ERRO,debug);
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
