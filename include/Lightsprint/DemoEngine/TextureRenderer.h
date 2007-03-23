// --------------------------------------------------------------------------
// DemoEngine
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef TEXTURERENDERER_H
#define TEXTURERENDERER_H

#include "Texture.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer
//

//! Quickly renders skybox or any other cube map.
//
//! Could be extended to support 2D textures.
//! It handles resource (shader) allocation/freeing.
//! Needs sky.vp/fp and texture.vp/fp shaders on disk.
class DE_API TextureRenderer
{
public:
	//! Initializes renderer, loading sky.vp/fp shaders from disk.
	TextureRenderer(const char* pathToShaders);

	//! Shutdowns renderer, freeing shaders.
	~TextureRenderer();

	//! Renders cube texture as sky around camera.
	void renderEnvironment(Texture* texture);

	//! Renders 2d texture into rectangle.
	//! x/y/w/h are in 0..1 space, x/y is top left corner.
	//! For non-NULL color, texture is multiplied by color.
	void render2D(Texture* texture,float color[4], float x,float y,float w,float h);

private:
	class Program *skyProgram;
	class Program *twodProgram;
};

}; // namespace

#endif
