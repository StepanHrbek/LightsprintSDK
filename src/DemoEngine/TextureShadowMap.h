// --------------------------------------------------------------------------
// DemoEngine
// TextureShadowMap, Texture that can hold shadowmap, uses FBO when available.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef TEXTURESHADOWMAP_H
#define TEXTURESHADOWMAP_H

#include "TextureGL.h"


/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

class TextureShadowMap : public TextureGL
{
public:
	TextureShadowMap(unsigned awidth, unsigned aheight);
	virtual void renderingToBegin();
	virtual void renderingToEnd();
	virtual unsigned getDepthBits(); // number of bits in texture depth channel
	virtual ~TextureShadowMap();
private:
	static unsigned numInstances;
};


#endif
