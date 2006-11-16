// --------------------------------------------------------------------------
// DemoEngine
// Renderer, abstract renderer with access to its parameters.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef RENDERER_H
#define RENDERER_H

#include "DemoEngine.h"


//////////////////////////////////////////////////////////////////////////////
//
// Renderer - interface

class DE_API Renderer
{
public:
	//! Renders.
	virtual void render() = 0;

	//! When renderer instance has parameters that modify output, this returns them.
	//! By default, renderer is expected to have no parameters and render always the same.
	virtual const void* getParams(unsigned& length) const
	{
		length = 0;
		return 0;
	}

	virtual ~Renderer() {};
};

#endif
