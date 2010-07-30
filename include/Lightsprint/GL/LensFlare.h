//----------------------------------------------------------------------------
//! \file LensFlare.h
//! \brief LightsprintGL | lens flare effect
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2010
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef LENSFLARE_H
#define LENSFLARE_H

#include "Texture.h"
#include "Camera.h"
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRObject.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// LensFlare

//! Lens flare effect.
//! Images by Michael Tanczos, published at http://www.gamedev.net/reference/articles/article874.asp.
//! To use your own images, save them over existing data/maps/flare*.png.
class RR_GL_API LensFlare
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	//!  Textures are loaded from paths relative to shaders, e.g. ../maps/flare1.png.
	LensFlare(const char* pathToShaders);
	~LensFlare();

	//! Renders lens flares for single light source.
	//
	//! \param flareSize
	//!  Relative size of flare, 1 for typical size.
	//! \param flareId
	//!  Various flare parameters are generated from this number.
	//! \param textureRenderer
	//!  Pointer to caller's TextureRenderer instance, we save time by not creating local instance.
	//! \param aspect
	//!  Camera aspect.
	//! \param lightPositionInWindow
	//!  0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	void renderLensFlare(float flareSize, unsigned flareId, class TextureRenderer* textureRenderer, float aspect, rr::RRVec2 lightPositionInWindow);
	//! Renders lens flares for set of light sources, skips occluded lights.
	//
	//! \param flareSize
	//!  Relative size of flare, 1 for typical size.
	//! \param flareId
	//!  Various flare parameters are generated from this number.
	//! \param textureRenderer
	//!  Pointer to caller's TextureRenderer instance, we save time by not creating local instance.
	//! \param ray
	//!  Pointer to caller's RRRay instance, we save time by not creating local instance.
	//!  Its collisionHandler is used, it should ideally be scene->createCollisionHandlerFirstVisible()
	//!  so that light is not occluded and flares not blocked by transparent pixels in alpha keyed textures.
	//! \param eye
	//!  Current camera.
	//! \param lights
	//!  Collection of lights in scene.
	//! \param scene
	//!  Object with all scene geometry, used for occlusion testing.
	void renderLensFlares(float flareSize, unsigned flareId, class TextureRenderer* textureRenderer, rr::RRRay* ray, const Camera& eye, const rr::RRLights& lights, const rr::RRObject* scene);

protected:
	TextureRenderer* textureRenderer;
	enum {NUM_PRIMARY_MAPS=3,NUM_SECONDARY_MAPS=3};
	rr::RRBuffer* primaryMap[NUM_PRIMARY_MAPS];
	rr::RRBuffer* secondaryMap[NUM_SECONDARY_MAPS];
};

}; // namespace

#endif
