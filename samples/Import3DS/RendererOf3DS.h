// --------------------------------------------------------------------------
// RendererOf3DS, Renderer implementation that renders 3DS.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RENDEREROF3DS_H
#define RENDEREROF3DS_H

#include "Lightsprint/GL/Renderer.h"
#include "Model_3DS.h"

//////////////////////////////////////////////////////////////////////////////
//
// RendererOf3DS - basic OpenGL renderer implementation

//! OpenGL renderer of Model_3DS model.
class RendererOf3DS : public rr_gl::Renderer
{
public:
	//! Creates renderer of model.
	RendererOf3DS(const Model_3DS* model,bool lit,bool texturedDiffuse,bool texturedEmissive);
	//! Returns parameters that specify what render() does (pointer to pointer to model). 
	virtual const void* getParams(unsigned& length) const;
	//! Renders model using Model_3DS::Draw().
	virtual void render();
private:
	const Model_3DS* model;
	bool lit;
	bool texturedDiffuse;
	bool texturedEmissive;
};

#endif
