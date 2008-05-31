//----------------------------------------------------------------------------
//! \file ToneMapping.h
//! \brief LightsprintGL | tone mapping
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TONEMAPPING_H
#define TONEMAPPING_H

#include "Lightsprint/GL/TextureRenderer.h"

namespace rr_gl
{

//! ToneMapping support.
//! It's safe to use single ToneMapping instance even when rendering different content into multiple windows.
class RR_GL_API ToneMapping
{
public:
	ToneMapping(const char* pathToShaders);
	~ToneMapping();
	//! Adjusts tonemapping operator (brightness, contrast) for use in next frames.
	//! To be called when image was already rendered, but not yet flipped from backbuffer to frontbuffer.
	//! Only operator is adjusted here, not texture, it's your responsibility
	//! to pass brightness and contrast to renderer.
	void adjustOperator(rr::RRReal secondsSinceLastAdjustment, rr::RRVec3& brightness, rr::RRReal contrast);
private:
	Texture* bigTexture;
	Texture* smallTexture;
	TextureRenderer* textureRenderer;
};

}; // namespace

#endif
