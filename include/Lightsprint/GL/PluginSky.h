//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginSky.h
//! \brief LightsprintGL | renders sky
//----------------------------------------------------------------------------

#ifndef PLUGINSKY_H
#define PLUGINSKY_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Render sky plugin

//! Renders background or blend of backgrounds as they are set in given solver.
class RR_GL_API PluginParamsSky : public PluginParams
{
public:
	//! Solver with environment you want to render.
	rr::RRSolver* solver;
	//! Multiplier to use for sky rendering.
	float skyMultiplier;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsSky(const PluginParams* _next, rr::RRSolver* _solver, float _skyMultiplier) : solver(_solver), skyMultiplier(_skyMultiplier) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
