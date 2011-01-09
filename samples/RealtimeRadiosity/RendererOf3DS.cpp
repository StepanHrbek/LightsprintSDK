// --------------------------------------------------------------------------
// RendererOf3DS, Renderer implementation that renders 3DS model.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RendererOf3DS.h"
#include "../../src/LightsprintIO/Import3DS/Model_3DS.h"

/////////////////////////////////////////////////////////////////////////////
//
// RendererOf3DS

RendererOf3DS::RendererOf3DS(const Model_3DS* amodel,bool alit,bool atexturedDiffuse,bool atexturedEmissive)
{
	model = amodel;
	RR_ASSERT(model);
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
	if (model)
	{
		// warning: setting for lit, diffuse map, no emissive map is selected here
		model->Draw(NULL,lit,texturedDiffuse,texturedEmissive,NULL,NULL);
	}
}
