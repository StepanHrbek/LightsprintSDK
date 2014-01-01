//----------------------------------------------------------------------------
//! \file PluginStereo.h
//! \brief LightsprintGL | stereo render
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINSTEREO_H
#define PLUGINSTEREO_H

#include "Plugin.h"

namespace rr_gl
{

enum StereoMode
{
	SM_MONO             =0, ///< common non-stereo mode
	SM_INTERLACED       =2, ///< interlaced, with top scanline visible by right eye
	SM_INTERLACED_SWAP  =3, ///< interlaced, with top scanline visible by left eye
	SM_SIDE_BY_SIDE     =4, ///< left half is left eye
	SM_SIDE_BY_SIDE_SWAP=5, ///< left half is right eye
	SM_TOP_DOWN         =6, ///< top half is left eye
	SM_TOP_DOWN_SWAP    =7, ///< top half is right eye
};

/////////////////////////////////////////////////////////////////////////////
//
// Stereo plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! <table border=0><tr align=top><td>
//! \image html stereo1.png
//! </td><td>
//! \image html stereo2.png
//! </td></tr></table>
class RR_GL_API PluginParamsStereo : public PluginParams
{
public:
	//! One of camera stereo modes, or SM_MONO for common non-stereo render.
	StereoMode stereoMode;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode) : stereoMode(_stereoMode) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
