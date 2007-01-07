// --------------------------------------------------------------------------
// DemoEngine
// RendererOf3DS, Renderer implementation that renders 3DS.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RENDEREROF3DS_H
#define RENDEREROF3DS_H

#include "Renderer.h"
#include "Model_3DS.h"


//////////////////////////////////////////////////////////////////////////////
//
// RendererOf3DS - basic OpenGL renderer implementation

class DE_API RendererOf3DS : public Renderer
{
public:
	RendererOf3DS(const Model_3DS* model);
	virtual const void* getParams(unsigned& length) const;
	virtual void render();
private:
	const Model_3DS* model;
};

#endif
