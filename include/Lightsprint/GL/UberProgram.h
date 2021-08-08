//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file UberProgram.h
//! \brief LightsprintGL | generic GLSL program parametrized using \#defines
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
	static UberProgram* create(const rr::RRString& vertexShaderFileName, const rr::RRString& fragmentShaderFileName);

protected:
	UberProgram() {};
};

}; // namespace

#endif //UBERPROGRAM_H
