// --------------------------------------------------------------------------
// Shader, OpenGL 2.0 object.
// Copyright (C) 2005-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include "Lightsprint/RRMemory.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Shader

//! GLSL shader.
//
//! GLSL is language used by OpenGL API 
//! for writing shaders.
class Shader
{
public:
	//! Creates GLSL shader by concatenating string defines and contents of file filename.
	//! \param defines
	//!  Text that is inserted before shader source code.
	//!  Usually used for "#define XXX\n" codes.
	//! \param filename
	//!  Source code of GLSL shader.
	//! \param shaderType
	//!  GL_FRAGMENT_SHADER for fragment shader, GL_VERTEX_SHADER for vertex shader.
	static Shader* create(const char* defines, const rr::RRString& filename, GLenum shaderType = GL_FRAGMENT_SHADER);
	~Shader();

	//! Compiles shader.  
	//! (When errors are encountered, program link will fail.)
	void compile(const rr::RRString& filenameDiagnosticOnly);
	//! Returns OpenGL handle to shader.
	GLuint getHandle() const;
private:
	Shader(const rr::RRString& filenameDiagnosticOnly, const GLchar** source, GLenum shaderType);
	GLuint handle;
};

extern bool s_es;

}; // namespace

#endif
