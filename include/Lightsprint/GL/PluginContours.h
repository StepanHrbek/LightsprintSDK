//----------------------------------------------------------------------------
//! \file PluginContours.h
//! \brief LightsprintGL | contours postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2012-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINCONTOURS_H
#define PLUGINCONTOURS_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Contours plugin

//! Adds contours on top of scene render (next plugin).
class RR_GL_API PluginParamsContours : public PluginParams
{
public:
	//! Color of silhouettes.
	rr::RRVec3 silhouetteColor;

	//! Color of creases.
	rr::RRVec3 creaseColor;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsContours(const PluginParams* _next, const rr::RRVec3& _silhouetteColor, const rr::RRVec3& _creaseColor) : silhouetteColor(_silhouetteColor), creaseColor(_creaseColor) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
