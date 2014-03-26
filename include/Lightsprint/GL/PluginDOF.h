//----------------------------------------------------------------------------
//! \file PluginDOF.h
//! \brief LightsprintGL | depth of field postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINDOF_H
#define PLUGINDOF_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Depth of field plugin

//! Adds depth of field effect, using current camera settings (camera is passed to renderer via PluginParamsShared).
//
//! <table border=0><tr align=top><td>
//! \image html dof1.jpg
//! </td><td>
//! \image html dof2.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsDOF : public PluginParams
{
public:
	//! True = physically based DOF, requires PluginAccumulation later in plugin chain.
	//! False = faster DOF approximation
	bool accumulated;

	//! Filename of texture with aperture/bokeh shape. Only used if accumulated==true.
	rr::RRString apertureShapeFilename;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsDOF(const PluginParams* _next, bool _accumulated, const rr::RRString& _apertureShapeFilename) : accumulated(_accumulated), apertureShapeFilename(_apertureShapeFilename) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif