// --------------------------------------------------------------------------
// DemoEngine
// TextureShadowMap, Texture that can hold shadowmap, uses FBO when available.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef TEXTURESHADOWMAP_H
#define TEXTURESHADOWMAP_H

#include "TextureGL.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

class TextureShadowMap : public TextureGL
{
public:
	TextureShadowMap(unsigned awidth, unsigned aheight);
	virtual void setSize(unsigned width, unsigned height);
	virtual bool renderingToBegin(unsigned side);
	virtual void renderingToEnd();
	virtual unsigned getTexelBits(); // number of bits in texture depth channel
	virtual ~TextureShadowMap();
private:
	static unsigned numInstances;
};

}; // namespace

#endif
