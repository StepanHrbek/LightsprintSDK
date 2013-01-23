//----------------------------------------------------------------------------
//! \file UberProgram.h
//! \brief LightsprintGL | general Program parametrized using \#defines
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef UBERPROGRAM_H
#define UBERPROGRAM_H

#include "Program.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

//! GLSL program with preprocessor parameters changeable at runtime.
//
//! GLSL is language used by OpenGL API
//! for writing shaders.
class RR_GL_API UberProgram : public rr::RRUniformlyAllocated
{
public:
	// interface

	//! Returns program for given set of defines.
	virtual Program* getProgram(const char* defines) = 0;
	virtual ~UberProgram() {};

	// tools

	//! Creates UberProgram from vertex and fragment shaders stored in text files.
	//! \param vertexShaderFileName
	//!  File name of GLSL vertex shader source code.
	//! \param fragmentShaderFileName
	//!  File name of GLSL fragment shader source code.
	static UberProgram* create(const char* vertexShaderFileName, const char* fragmentShaderFileName);

protected:
	UberProgram() {};
};

}; // namespace

#endif //UBERPROGRAM_H
