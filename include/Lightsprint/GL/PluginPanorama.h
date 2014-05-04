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
	PM_EQUIRECTANGULAR  =1, ///< 360 degree render in equirectangular projection \image html pano-equirect.jpg
	PM_LITTLE_PLANET    =2, ///< 360 degree render in stereographic (little planet) projection \image html pano-littleplanet.jpg
	PM_FISHEYE          =3, ///< variable FOV fisheye render; suitable for dome projection with spherical mirror in center \image html pano-fisheye.jpg
};

enum PanoramaCoverage
{
	PC_FULL_STRETCH     =0, ///< panorama covers whole output area \image html cover-stretch.jpg
	PC_FULL             =1, ///< panorama covers as large output area as possible while preserving aspect ratio \image html cover-full.jpg
	PC_TRUNCATE_BOTTOM  =2, ///< panorama covers full output width, going from top down, with aspect preserved. bottom might be cropped or empty, depending on output size \image html cover-trunc-bottom.jpg
	PC_TRUNCATE_TOP     =3, ///< panorama covers full output width, going from bottom up, with aspect preserved. upper part might be cropped or empty, depending on output size \image html cover-trunc-top.jpg
};

/////////////////////////////////////////////////////////////////////////////
//
// 360 degree panorama plugin

//! Renders scene (calls next plugin) into cubemap, then transforms it back to 2d.
//
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
	//! One of camera panorama modes, or PM_OFF for common non-panorama render.
	PanoramaMode panoramaMode;
	//! One of panorama coverage modes.
	PanoramaCoverage panoramaCoverage;
	//! Panorama scale, 1 for normal size, 2 for bigger etc. Note that you can shift panorama with RRCamera::setScreenCenter().
	float scale;
	//! Fisheye field of view, 360 for full sphere, 180 for hemisphere.
	float fisheyeFovDeg;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsPanorama(const PluginParams* _next, PanoramaMode _panoramaMode, PanoramaCoverage _panoramaCoverage, float _scale, float _fisheyeFovDeg) : panoramaMode(_panoramaMode), panoramaCoverage(_panoramaCoverage), scale(_scale), fisheyeFovDeg(_fisheyeFovDeg) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
