// --------------------------------------------------------------------------
// DemoEngine
// UberProgram, general Program parametrized using #defines.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef UBERPROGRAM_H
#define UBERPROGRAM_H

#include "Program.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

//! GLSL program with preprocessor parameters changeable at runtime.
//
//! GLSL is language used by OpenGL API
//! for writing shaders.
class DE_API UberProgram
{
public:
	// interface

	//! Returns program for given set of defines.
	virtual Program* getProgram(const char* defines) = 0;
	virtual ~UberProgram() {};

	// tools

	//! Creates UberProgram from vertex and fragment shaders stored in text files.
	//! \param avertexShaderFileName
	//!  File name of GLSL vertex shader source code.
	//! \param afragmentShaderFileName
	//!  File name of GLSL fragment shader source code.
	static UberProgram* create(const char* vertexShaderFileName, const char* fragmentShaderFileName);

protected:
	UberProgram() {};
};

}; // namespace

#endif //UBERPROGRAM_H
