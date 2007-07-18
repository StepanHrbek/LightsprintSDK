// --------------------------------------------------------------------------
// DemoEngine
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

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
	bool renderEnvironmentBegin(float color[4]);

	//! Restores original render states after renderEnvironmentBegin().
	//! It is one component of renderEnvironment().
	void renderEnvironmentEnd();

	//! Renders 2d texture into rectangle.
	//! x/y/w/h are in 0..1 space, x/y is top left corner.
	//! For non-NULL color, texture color is multiplied by color.
	void render2D(const Texture* texture,float color[4], float x,float y,float w,float h);

private:
	class Program *skyProgram;
	class Program *twodProgram;
	unsigned char culling;
	unsigned char depthTest;
	unsigned char depthMask;
};

}; // namespace

#endif
