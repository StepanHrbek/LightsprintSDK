//----------------------------------------------------------------------------
//! \file SSGI.h
//! \brief LightsprintGL | screen space ambient occlusion postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2012-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SSGI_H
#define SSGI_H

#include "UberProgram.h"
#include "Texture.h"
#include "TextureRenderer.h"
#include "Lightsprint/RRCamera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// SSGI

//! Screen space global illumination postprocess.
//
//! This is how it looks:
//! \image html ssgi1.jpg
class RR_GL_API SSGI
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	SSGI(const rr::RRString& pathToShaders);
	~SSGI();

	//! Adds screen space ambient occlusion effect to image in current render target, to region of size w*h.
	void applySSGI(unsigned w, unsigned h, const rr::RRCamera& eye, float intensity, float radius, float angleBias, TextureRenderer* debugTextureRenderer = NULL);

protected:
	Texture* bigColor;
	Texture* bigColor2;
	Texture* bigColor3;
	Texture* bigDepth;
	UberProgram* ssgiUberProgram;
	Program* ssgiProgram1;
	Program* ssgiProgram2;
};

}; // namespace

#endif
