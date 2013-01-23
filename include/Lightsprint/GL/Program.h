//----------------------------------------------------------------------------
//! \file Program.h
//! \brief LightsprintGL | OpenGL 2.0 object with optional vertex and fragment shaders
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PROGRAM_H
#define PROGRAM_H

#include "DemoEngine.h"
#include <GL/glew.h>

namespace rr_gl
{

// vertex attrib array indices
// Note: Nvidia is broken, small indices alias with fixed attributes like gl_Vertex (although spec explicitly states they don't alias).
// Therefore we use the highest indices possible (MAX_VERTEX_ATTRIBS must be at least 16),
// it is equally correct, and it works around Nvidia bug.
enum
{
	VAA_POSITION = 0,
	VAA_DIRECTION = 1,
	VAA_UV0 = 2,
	VAA_TANGENT = 14,
	VAA_BITANGENT = 15,
};

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

	//! Sets uniform texture (activates texture unit, binds texture), returns texture unit used.
	//
	//! First call after useIt() uses texture unit 0, next one uses unit 1 etc.
	//! If code in range 0..15 is entered, all textures with the same code are sent to the same unit.
	//!
	//! Following fragments of code do the same
	//! - sendTexture(name,tex);
	//! - sendTexture(name,NULL); tex->bindTexture();
	//! - unsigned unit = sendTexture(name,NULL); glActiveTexture(GL_TEXTURE0+unit); tex->bindTexture();
	//!
	//! When doing multiple renders that differ only in textures, use codes
	//! - useIt(); for (int i=0;i<100;i++) { sendTexture(name1,...,1); sendTexture(name2,...,2); draw(); }
	//! With codes, the same 2 texture units are used 100 times.
	//! Without codes, 200 texture units would be allocated.
	unsigned sendTexture(const char* name, const class Texture* t, int code = -1);

	//! Sets uniform of type float.
	void sendUniform(const char* name, float x);
	//! Sets uniform of type vec2.
	void sendUniform(const char* name, float x, float y);
	//! Sets uniform of type vec3.
	void sendUniform(const char* name, const rr::RRVec3& xyz);
	//! Sets uniform of type vec4.
	void sendUniform(const char* name, const rr::RRVec4& xyzw);
	//! Sets array of uniforms of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniform(const char* name, int count, const GLint* x);
	//! Sets uniform of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniform(const char* name, int x);
	//! Sets uniform of type int2.
	void sendUniform(const char* name, int x, int y);
	//! Sets uniform of type mat2, mat3 or mat4.
	void sendUniform(const char* name, const float *m, bool transp=false, int size=4);

	//! Does uniform exist in program? 
	bool uniformExists(const char* uniformName);

	//! Print OpenGL log to console. False by default. Set true for debugging.
	static void logMessages(bool enable);
private:
	Program(const char* defines, const char* vertexShader, const char* fragmentShader);
	int getLoc(const char* name);
	bool isLinked();
	bool logLooksSafe();

	class Shader *vertex, *fragment;
	unsigned handle;
	bool linked;
	unsigned nextTextureUnit; // texture unit to be used in next setTexture()
	signed char assignedTextureUnit[16]; // indexed by code
};

}; // namespace

#endif
