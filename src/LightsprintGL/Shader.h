// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// GLSL shader.
// --------------------------------------------------------------------------

#ifndef SHADER_H
#define SHADER_H

#include "Lightsprint/RRMemory.h"
#include "Lightsprint/GL/Program.h" // includes <glew.h>

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

#ifndef RR_GL_ES2
	extern bool s_es;
#endif

}; // namespace

#endif
