// --------------------------------------------------------------------------
// Renderer base class.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RENDERER_H
#define RENDERER_H

#include "Lightsprint/GL/DemoEngine.h"

//////////////////////////////////////////////////////////////////////////////
//
// Renderer - interface
//
//! This used to be base class for all renderers; and all renderers used to support display lists.
//! Since we switched built-in renderer from display lists to VBOs, this class
//! and display lists remain only in RealtimeRadiosity sample.

class Renderer : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	/////////////////////////////////////////////////////////////////////////
	// interface
	/////////////////////////////////////////////////////////////////////////

	//! Renders contents derived from implementation defined parameters.
	//! Example1: Parameters are Scene* scene, render() renders scene.
	//! Example2: Parameters are Renderer* ren, render() saves ren's output into display list and replays it next time.
	virtual void render() = 0;

	//! Returns pointer to block of parameters that specify what is rendered by render().
	//! By default, renderer is expected to have no parameters and renders always the same.
	virtual const void* getParams(unsigned& length) const
	{
		length = 0;
		return 0;
	}

	virtual ~Renderer() {};


	/////////////////////////////////////////////////////////////////////////
	// tools
	/////////////////////////////////////////////////////////////////////////

	//! Creates and returns renderer that caches OpenGL rendering commands in display list.
	//! Caching takes into account possible parameters of underlying renderer
	//! and manages multiple display lists.
	Renderer* createDisplayList();
};

#endif
