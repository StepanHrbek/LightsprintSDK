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

enum PanoramaMode
{
	PM_OFF              =0, ///< common non-panorama mode
	PM_EQUIRECTANGULAR  =1, ///< 360 degree render in equirectangular projection \image html 360-equirect.jpg
	PM_LITTLE_PLANET    =2, ///< 360 degree render in stereographic (little planet) projection \image html 360-planet.jpg
};

/////////////////////////////////////////////////////////////////////////////
//
// 360 degree panorama plugin

//! Renders scene (calls next plugin) into cubemap, then transforms it back to 2d.
//
//! <table border=0><tr align=top><td>
//! \image html 360-equirect.jpg
//! </td><td>
//! \image html 360-planet.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsPanorama : public PluginParams
{
public:
	//! One of camera panorama modes, or PM_OFF for common non-panorama render.
	PanoramaMode panoramaMode;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsPanorama(const PluginParams* _next, PanoramaMode _panoramaMode) : panoramaMode(_panoramaMode) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
