//----------------------------------------------------------------------------
//! \file Bloom.h
//! \brief LightsprintGL | bloom postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef BLOOM_H
#define BLOOM_H

#include "Program.h"
#include "Texture.h"
#include "Camera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Bloom

//! Bloom postprocess.
//
//! This is how it looks:
//! \image html bloom.jpg
class RR_GL_API Bloom
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	Bloom(const char* pathToShaders);
	~Bloom();

	//! Adds bloom effect to image in current render target, to region of size w*h.
	void applyBloom(unsigned w, unsigned h);

protected:
	Texture* bigMap;
	Texture* smallMap1;
	Texture* smallMap2;
	Program* scaleDownProgram;
	Program* blurProgram;
	Program* blendProgram;
};

}; // namespace

#endif
