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
	PM_DOME             =3, ///< 180 degree render in dome projection \image html 180-dome.jpg
};

enum PanoramaCoverage
{
	PC_FULL_STRETCH     =0, ///< panorama covers whole output area
	PC_FULL             =1, ///< panorama covers as large output area as possible while preserving aspect ratio
	PC_TRUNCATE_BOTTOM  =2, ///< panorama covers full output width, going from top down, with aspect preserved. bottom might be cropped or empty, depending on output size
	PC_TRUNCATE_TOP     =3, ///< panorama covers full output width, going from bottom up, with aspect preserved. upper part might be cropped or empty, depending on output size
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
//! </td><td>
//! \image html 180-dome.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsPanorama : public PluginParams
{
public:
	//! One of camera panorama modes, or PM_OFF for common non-panorama render.
	PanoramaMode panoramaMode;
	//! One of panorama coverage modes.
	PanoramaCoverage panoramaCoverage;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsPanorama(const PluginParams* _next, PanoramaMode _panoramaMode, PanoramaCoverage _panoramaCoverage) : panoramaMode(_panoramaMode), panoramaCoverage(_panoramaCoverage) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
