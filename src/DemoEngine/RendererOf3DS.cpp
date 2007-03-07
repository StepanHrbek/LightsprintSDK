// --------------------------------------------------------------------------
// DemoEngine
// RendererOf3DS, Renderer implementation that renders 3DS model.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include "RendererOf3DS.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// RendererOf3DS

RendererOf3DS::RendererOf3DS(const Model_3DS* amodel)
{
	model = amodel;
	assert(model);
}

const void* RendererOf3DS::getParams(unsigned& length) const
{
	length = sizeof(model);
	return &model;
}

void RendererOf3DS::render()
{
	if(model)
		model->Draw(NULL,true,true,NULL,NULL);
}

/////////////////////////////////////////////////////////////////////////////
//
// Renderer

Renderer* Renderer::create3DSRenderer(const Model_3DS* model)
{
	return new RendererOf3DS(model);
}

}; // namespace
