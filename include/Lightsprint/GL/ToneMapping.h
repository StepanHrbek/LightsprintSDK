//----------------------------------------------------------------------------
//! \file ToneMapping.h
//! \brief LightsprintGL | tone mapping
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2008-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TONEMAPPING_H
#define TONEMAPPING_H

#include "Lightsprint/GL/TextureRenderer.h"

namespace rr_gl
{

//! ToneMapping support.
//
//! Automatically adjusts tone mapping operator to simulate eye response in time.
//! It's safe to use single ToneMapping instance even when rendering different content into multiple windows.
class RR_GL_API ToneMapping
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	ToneMapping(const char* pathToShaders);
	~ToneMapping();

	//! Adjusts tonemapping operator (brightness, contrast) for use in next frames.
	//
	//! To be called when image was already rendered, but not yet flipped from backbuffer to frontbuffer.
	//! Only operator is adjusted here, not texture, it's your responsibility
	//! to pass brightness and contrast to renderer.
	//! \param secondsSinceLastAdjustment
	//!  Number of seconds since last call to this function.
	//! \param brightness
	//!  Specified 'multiply screen by brightness' operator that will be adjusted.
	//! \param contrast
	//!  Not used for now, may be adjusted in future.
	//! \param targetIntensity
	//!  Average pixel intensity we want to see after tone mapping, in 0..1 range, e.g. 0.5.
	void adjustOperator(rr::RRReal secondsSinceLastAdjustment, rr::RRVec3& brightness, rr::RRReal contrast, rr::RRReal targetIntensity);
private:
	Texture* bigTexture;
	Texture* smallTexture;
	TextureRenderer* textureRenderer;
};

}; // namespace

#endif
