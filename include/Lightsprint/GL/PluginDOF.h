//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginDOF.h
//! \brief LightsprintGL | depth of field postprocess
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
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
