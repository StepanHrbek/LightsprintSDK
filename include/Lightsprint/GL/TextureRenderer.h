//----------------------------------------------------------------------------
//! \file TextureRenderer.h
//! \brief LightsprintGL | helper for rendering 2D or cube texture in OpenGL 2.0
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TEXTURERENDERER_H
#define TEXTURERENDERER_H

#include "Lightsprint/RRCamera.h"
#include "Texture.h"
#include "UberProgram.h"

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

	//! Renders cubemap, equirectangular 2d texture or blend of two such maps as if camera is in center.
	//
	//! Use blendFactor=0 to render only texture0, texture1 then may be NULL.
	//! For non-NULL color, texture color is multiplied by color.
	//! Color is finally gamma corrected by gamma, 1 = no correction.
	//! Angles rotate environments around top/down axis.
	bool renderEnvironment(const rr::RRCamera& camera, const Texture* texture0, float angleRad0, const Texture* texture1, float angleRad1, float blendFactor, const rr::RRVec4* brightness, float gamma, bool allowDepthTest);

	//! Renders 2d texture into rectangle, using current blending/alpha testing/depth testing/masking etc modes.
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
	//! \param z
	//!  Depth in 0..1 range to be assigned to all rendered fragments.
	//!  Additional effect of z in render2D() (not in render2dQuad()) is: "if (z<0) temporarily disable GL_DEPTH_TEST"
	//! \param extraDefines
	//!  Usually NULL, may be additional glsl code inserted at the beginning of shader, to enable special rendering paths.
	void render2D(const Texture* texture, const rr::RRVec4* color, float gamma, float x,float y,float w,float h,float z=-1, const char* extraDefines=NULL);

	//! Component of render2D(), initializes pipeline.
	bool render2dBegin(const rr::RRVec4* color, float gamma, const char* extraDefines=NULL);
	//! Component of render2D(), renders textured quad. May be called multiple times between render2dBegin() and render2dEnd().
	void render2dQuad(const Texture* texture, float x,float y,float w,float h,float z=-1);
	//! Component of render2D(), restores pipeline.
	void render2dEnd();

	//! sky.* uberprogram, only for reading, feel free to render with it directly.
	UberProgram* skyProgram;
	//! texture.* uberprogram, only for reading, feel free to render with it directly.
	UberProgram* twodProgram;

	//! Helper, renders quad, identical to glBegin();4x glVertex();glEnd(); with coordinates from -1,-1 to 1,1.
	static void renderQuad();

private:
	bool renderEnvironment(const rr::RRCamera& camera, const Texture* texture, float angleRad, const rr::RRVec3& brightness, float gamma);
};

}; // namespace

#endif
