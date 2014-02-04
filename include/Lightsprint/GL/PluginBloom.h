//----------------------------------------------------------------------------
//! \file PluginBloom.h
//! \brief LightsprintGL | bloom postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINBLOOM_H
#define PLUGINBLOOM_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Bloom plugin

//! Adds bloom effect on top of scene render (next plugin).
//
//! <table border=0><tr align=top><td>
//! \image html bloom.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsBloom : public PluginParams
{
public:
	//! Bloom effect is created only around pixels with intensity above this threshold. Default: 1. Lower values produce more bloom.
	float threshold;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsBloom(const PluginParams* _next, float _threshold) : threshold(_threshold) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
