//----------------------------------------------------------------------------
//! \file TextureRenderer.h
//! \brief LightsprintGL | helper for rendering 2D or cube texture in OpenGL 2.0
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TEXTURERENDERER_H
#define TEXTURERENDERER_H

#include "Texture.h"

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
	TextureRenderer(const char* pathToShaders);

	//! Shutdowns renderer, freeing shaders.
	~TextureRenderer();

	//! Renders cubemap as if camera is inside the cube.
	//! Current OpenGL transformation matrices are used.
	//! For non-NULL color, texture color is multiplied by color.
	bool renderEnvironment(const Texture* texture, float color[4]);

	//! Initializes shader and render states for rendering cubemap.
	//! It is one component of renderEnvironment().
	//! For non-NULL color, texture is multiplied by color.
	bool renderEnvironmentBegin(float color[4], bool allowDepthTest);

	//! Restores original render states after renderEnvironmentBegin().
	//! It is one component of renderEnvironment().
	void renderEnvironmentEnd();

	//! Renders 2d texture into rectangle.
	//
	//! x/y/w/h are in 0..1 space, x/y is top left corner.
	//! For non-NULL color, texture color is multiplied by color.
	//! \n render2D() internally calls render2dBegin() + render2dQuad() + render2dEnd().
	//! For rendering N textures, it's possible to simply call render2D() N times,
	//! but render2dBegin() + N*render2dQuad() + render2dEnd() is slightly faster.
	void render2D(const Texture* texture,float color[4], float x,float y,float w,float h);

	//! Component of render2D(), initializes pipeline.
	bool render2dBegin(float color[4]);
	//! Component of render2D(), renders textured quad. May be called multiple times between render2dBegin() and render2dEnd().
	void render2dQuad(const Texture* texture, float x,float y,float w,float h);
	//! Component of render2D(), restores pipeline.
	void render2dEnd();

private:
	class Program *skyProgram;
	class Program *twodProgram;
	unsigned char culling;
	unsigned char depthTest;
	unsigned char depthMask;
	const class Camera* oldCamera;
};

}; // namespace

#endif
