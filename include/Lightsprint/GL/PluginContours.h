//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginContours.h
//! \brief LightsprintGL | contours postprocess
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
//
//! <table border=0><tr align=top><td>
//! \image html contours1.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsContours : public PluginParams
{
public:
	//! Color and opacity of silhouettes.
	rr::RRVec4 silhouetteColor;

	//! Color and opacity of creases.
	rr::RRVec4 creaseColor;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsContours(const PluginParams* _next, const rr::RRVec4& _silhouetteColor, const rr::RRVec4& _creaseColor) : silhouetteColor(_silhouetteColor), creaseColor(_creaseColor) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
