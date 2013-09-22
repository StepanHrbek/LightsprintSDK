//----------------------------------------------------------------------------
//! \file DOF.h
//! \brief LightsprintGL | depth of field postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2012-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef DOF_H
#define DOF_H

#include "Program.h"
#include "Texture.h"
#include "Lightsprint/RRCamera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// DOF

//! Depth of Field postprocess.
//
//! This is how it looks:
//! \image html dof2.jpg
class RR_GL_API DOF
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	DOF(const char* pathToShaders);
	~DOF();

	//! Adds depth of field effect to image in current render target, to region of size w*h.
	void applyDOF(unsigned w, unsigned h, const rr::RRCamera& eye);

protected:
	Texture* smallColor1;
	Texture* smallColor2;
	Texture* smallColor3;
	Texture* bigColor;
	Texture* bigDepth;
	Program* dofProgram1;
	Program* dofProgram2;
	Program* dofProgram3;
};

}; // namespace

#endif
