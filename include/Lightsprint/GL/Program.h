//----------------------------------------------------------------------------
//! \file Program.h
//! \brief LightsprintGL | OpenGL 2.0 object with optional vertex and fragment shaders
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PROGRAM_H
#define PROGRAM_H

#include "DemoEngine.h"
#include <GL/glew.h>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Program

//! GLSL program.
//
//! GLSL is language used by OpenGL API for writing shaders.
//! Program is one or more shaders linked together, see OpenGL for more details.
class RR_GL_API Program : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates GLSL program by linking one vertex and one fragment shader together.
	//! Shaders are created by concatenating string defines
	//! and contents of file vertexShader / fragmentShader.
	//! \param defines
	//!  String inserted before compilation at the beginning of shader source code. May be NULL.
	//! \param vertexShader
	//!  Name of text file with GLSL vertex shader. May be NULL for fixed pipeline.
	//! \param fragmentShader
	//!  Name of text file with GLSL fragment shader. May be NULL for fixed pipeline.
	static Program* create(const char* defines, const char* vertexShader, const char* fragmentShader);

	~Program();

	//! Validates program and returns true only when it was successful,
	//! see OpenGL for more details on validation.
	//! Validation is optional process used mainly for debugging.
	bool isValid();
	//! Sets rendering pipeline so that program will be used for
	//! following primitives rendered.
	//! Program must be linked before use.
	void useIt();
	//! Prints variables found in linked program to standard output.
	//! Used for debugging.
	void enumVariables();

	//! Sets uniform of type float.
	void sendUniform(const char *name, float x);
	//! Sets uniform of type vec2.
	void sendUniform(const char *name, float x, float y);
	//! Sets uniform of type vec3.
	void sendUniform(const char *name, float x, float y, float z);
	//! Sets uniform of type vec3.
	void sendUniform3fv(const char *name, const float xyz[3]);
	//! Sets uniform of type vec4.
	void sendUniform(const char *name, float x, float y, float z, float w);
	//! Sets uniform of type vec4.
	void sendUniform4fv(const char *name, const float xyzw[4]);
	//! Sets array of uniforms of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniform(const char *name, int count, const GLint* x);
	//! Sets uniform of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniform(const char *name, int x);
	//! Sets uniform of type int2.
	void sendUniform(const char *name, int x, int y);
	//! Sets uniform of type int3.
	void sendUniform(const char *name, int x, int y, int z);
	//! Sets uniform of type int4.
	void sendUniform(const char *name, int x, int y, int z, int w);
	//! Sets uniform of type mat2, mat3 or mat4.
	void sendUniform(const char *name, float *m, bool transp=false, int size=4);

	//! Returns location of uniform.
	//! Used to detect Nvidia driver bug.
	bool uniformExists(const char* uniformName);

	//! Print OpenGL log to console. False by default. Set true for debugging.
	static void logMessages(bool enable);
private:
	Program(const char* defines, const char* vertexShader, const char* fragmentShader);
	int getLoc(const char *name);
	bool isLinked();
	bool logLooksSafe();

	class Shader *vertex, *fragment;
	unsigned handle;
	bool linked;
};

}; // namespace

#endif
