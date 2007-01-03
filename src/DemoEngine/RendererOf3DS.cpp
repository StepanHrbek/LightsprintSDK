// --------------------------------------------------------------------------
// DemoEngine
// RendererOf3DS, Renderer implementation that renders 3DS model.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#include <cassert>
#include "DemoEngine/RendererOf3DS.h"

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
		model->Draw(NULL,NULL,NULL);
}
