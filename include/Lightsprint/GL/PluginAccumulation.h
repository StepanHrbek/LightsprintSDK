//----------------------------------------------------------------------------
//! \file PluginAccumulation.h
//! \brief LightsprintGL | Accumulates frames, outputs average
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINACCUMULATION_H
#define PLUGINACCUMULATION_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Accumulation plugin

//! Accumulates frames in time, outputs average of previous frames.
class RR_GL_API PluginParamsAccumulation : public PluginParams
{
public:
	//! When set, old images are lost, accumulation is restarted.
	bool restart;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsAccumulation(const PluginParams* _next, bool _restart) : restart(_restart) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
