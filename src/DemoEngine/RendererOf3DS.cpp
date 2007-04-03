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

RendererOf3DS::RendererOf3DS(const Model_3DS* amodel,bool alit,bool atexturedDiffuse,bool atexturedEmissive)
{
	model = amodel;
	assert(model);
	lit = alit;
	texturedDiffuse = atexturedDiffuse;
	texturedEmissive = atexturedEmissive;
}

const void* RendererOf3DS::getParams(unsigned& length) const
{
	length = sizeof(model);
	return &model;
}

void RendererOf3DS::render()
{
	if(model)
	{
		// warning: setting for lit, diffuse map, no emissive map is selected here
		model->Draw(NULL,lit,texturedDiffuse,texturedEmissive,NULL,NULL);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// Renderer

Renderer* Renderer::create3DSRenderer(const Model_3DS* model,bool lit,bool texturedDiffuse,bool texturedEmissive)
{
	return new RendererOf3DS(model,lit,texturedDiffuse,texturedEmissive);
}

}; // namespace
