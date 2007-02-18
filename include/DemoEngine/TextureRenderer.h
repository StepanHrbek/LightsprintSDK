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

class DE_API TextureRenderer
{
public:
	TextureRenderer(const char* pathToShaders);
	~TextureRenderer();
	void renderEnvironment(Texture* texture);
private:
	class Program *skyProgram;
};

}; // namespace

#endif
