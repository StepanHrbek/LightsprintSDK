//----------------------------------------------------------------------------
//! \file Renderer.h
//! \brief LightsprintGL | abstract renderer with access to its parameters
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDERER_H
#define RENDERER_H

#include "DemoEngine.h"

namespace rr_gl
{
//////////////////////////////////////////////////////////////////////////////
//
// Renderer - interface

//! Interface of renderer.
class RR_GL_API Renderer : public rr::RRUniformlyAllocated
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

}; // namespace

#endif
