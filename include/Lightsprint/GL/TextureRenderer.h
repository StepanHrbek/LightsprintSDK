//----------------------------------------------------------------------------
//! \file TextureRenderer.h
//! \brief LightsprintGL | helper for rendering 2D or cube texture in OpenGL 2.0
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2012
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TEXTURERENDERER_H
#define TEXTURERENDERER_H

#include "Texture.h"
#include "Lightsprint/RRCamera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer
//

//! Helper for render of 2D or cube (skybox) texture.
//
//! It handles resource (shader) allocation/freeing.
//! Needs sky.vs/fs and texture.vs/fs shaders on disk.
class RR_GL_API TextureRenderer
{
public:
	//! Initializes renderer, loading shaders from disk.
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	TextureRenderer(const char* pathToShaders);

	//! Shutdowns renderer, freeing shaders.
	~TextureRenderer();

	//! Renders cubemap or blend of two cubemaps as if camera is inside the cube.
	//! Use blendFactor=0 to render only texture0, texture1 then may be NULL.
	//! Current OpenGL transformation matrices are used.
	//! For non-NULL color, texture color is multiplied by color.
	//! Color is finally gamma corrected by gamma, 1 = no correction.
	bool renderEnvironment(const rr::RRCamera& camera, const Texture* texture0, const Texture* texture1, float blendFactor, const rr::RRVec4* brightness, float gamma, bool allowDepthTest);

	//! Renders 2d texture into rectangle, using current blending and alpha testing modes.
	//
	//! render2D() internally calls render2dBegin() + render2dQuad() + render2dEnd().
	//! For rendering N textures, it's possible to simply call render2D() N times,
	//! but render2dBegin() + N*render2dQuad() + render2dEnd() is slightly faster.
	//! \param texture
	//!  Texture to be rendered.
	//! \param color
	//!  For non-NULL color, output (texture color) is multiplied by color.
	//! \param gamma
	//!  Gamma correction, 1 for no change.
	//! \param x
	//!  Position of texture's left side in render target, 0=leftmost, 1=rightmost.
	//! \param y
	//!  Position of texture's bottom side in render target, 0=bottom, 1=top.
	//! \param w
	//!  x+w is position of texture's right side in render target, 0=leftmost, 1=rightmost. Negative w is supported.
	//! \param h
	//!  y+h is position of texture's top side in render target, 0=bottom, 1=top. Negative h is supported.
	void render2D(const Texture* texture, const rr::RRVec4* color, float gamma, float x,float y,float w,float h);

	//! Component of render2D(), initializes pipeline.
	bool render2dBegin(const rr::RRVec4* color, float gamma);
	//! Component of render2D(), renders textured quad. May be called multiple times between render2dBegin() and render2dEnd().
	void render2dQuad(const Texture* texture, float x,float y,float w,float h);
	//! Component of render2D(), restores pipeline.
	void render2dEnd();

private:
	bool renderEnvironment(const rr::RRCamera& camera, const Texture* texture, const rr::RRVec3& brightness, float gamma);

	class UberProgram* skyProgram;
	class UberProgram* twodProgram;
	unsigned char culling;
	unsigned char depthTest;
	unsigned char depthMask;
	const rr::RRCamera* oldCamera;
};

}; // namespace

#endif
