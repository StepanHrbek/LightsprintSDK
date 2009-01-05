// --------------------------------------------------------------------------
// Shader, OpenGL 2.0 object.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include "Lightsprint/GL/DemoEngine.h"

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
	static Shader* create(const char* defines, const char* filename, GLenum shaderType = GL_FRAGMENT_SHADER);
	~Shader();

	//! Compiles shader.  
	//! (When errors are encountered, program link will fail.)
	void compile(const char* filenameDiagnosticOnly);
	//! Returns OpenGL handle to shader.
	GLuint getHandle() const;
private:
	Shader(const char* filenameDiagnosticOnly, const GLchar** source, GLenum shaderType);
	GLuint handle;
};

}; // namespace

#endif
