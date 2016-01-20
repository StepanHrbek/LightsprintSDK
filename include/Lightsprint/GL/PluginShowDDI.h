//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginShowDDI.h
//! \brief LightsprintGL | modifies PluginScene parameters to show detected direct illumination
//----------------------------------------------------------------------------

#ifndef PLUGINSHOWDDI_H
#define PLUGINSHOWDDI_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Render DDI plugin

//! Helper for visualizing per-triangle data sent to RRSolver::setDirectIllumination().
//
//! This plugin works by locating PluginScene in plugin chain and changing its parameters.
//! It has no effect if no such plugin is found.
//!
//! <table border=0><tr align=top><td>
//! \image html ddi1.jpg
//! </td><td>
//! \image html ddi2.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsShowDDI : public PluginParams
{
public:
	//! Solver with DDI (detected direct illumination) you want to visualize.
	rr::RRSolver* solver;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsShowDDI(const PluginParams* _next, rr::RRSolver* _solver) : solver(_solver) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
