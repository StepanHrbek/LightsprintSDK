//----------------------------------------------------------------------------
//! \file PluginSky.h
//! \brief LightsprintGL | renders sky
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2013
//! All rights reserved
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
	rr::RRDynamicSolver* solver;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsSky(const PluginParams* _next, rr::RRDynamicSolver* _solver) : solver(_solver) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
