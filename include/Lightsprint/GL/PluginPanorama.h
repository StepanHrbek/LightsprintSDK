//----------------------------------------------------------------------------
//! \file PluginPanorama.h
//! \brief LightsprintGL | renders 360 degree panorama
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINPANORAMA_H
#define PLUGINPANORAMA_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// 360 degree panorama plugin

//! Renders scene (calls next plugin) into cubemap, then transforms it back to 2d.
//
//! It is not reentrant because we did not need it yet, don't start rendering panorama2
//! in the middle of rendering panorama1.
//!
//! <table border=0><tr align=top><td>
//! \image html pano-equirect.jpg
//! </td><td>
//! \image html pano-littleplanet.jpg
//! </td><td>
//! \image html pano-fisheye.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsPanorama : public PluginParams
{
public:
	//! Convenience ctor, for setting plugin parameters.
	PluginParamsPanorama(const PluginParams* _next) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
