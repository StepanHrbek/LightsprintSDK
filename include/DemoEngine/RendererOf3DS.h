// --------------------------------------------------------------------------
// DemoEngine
// RendererOf3DS, Renderer implementation that renders 3DS.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RENDEREROF3DS_H
#define RENDEREROF3DS_H

#include "Renderer.h"
#include "Model_3DS.h"

namespace de
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOf3DS - basic OpenGL renderer implementation

//! OpenGL renderer of Model_3DS model.
class DE_API RendererOf3DS : public Renderer
{
public:
	//! Creates renderer of model.
	RendererOf3DS(const Model_3DS* model);
	//! Returns parameters that specify what render() does (pointer to pointer to model). 
	virtual const void* getParams(unsigned& length) const;
	//! Renders model using Model_3DS::Draw().
	virtual void render();
private:
	const Model_3DS* model;
};

}; // namespace

#endif
