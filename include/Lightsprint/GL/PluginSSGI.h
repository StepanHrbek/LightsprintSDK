//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginSSGI.h
//! \brief LightsprintGL | screen space global illumination postprocess
//----------------------------------------------------------------------------

#ifndef PLUGINSSGI_H
#define PLUGINSSGI_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// SSGI plugin

//! Adds screen space global illumination effect on top of scene render (next plugin).
//
//! SSGI simulates single short distance light bounce, with color bleeding.
//! \n It can be used with Fireball or Architect realtime GI solvers to improve realtime GI quality (GI solvers contribute low frequency GI, SSGI adds high frequencies).
//! \n It can be used with constant ambient to add at least some GI effects to non-GI render.
//! \n It is a realtime alternative to baked LDM.
//!
//! <table border=0><tr align=top><td>
//! \image html ssgi1on.jpg on
//! </td><td>
//! \image html ssgi1off.jpg off
//! </td></tr><tr><td>
//! \image html ssgi2on.jpg
//! </td><td>
//! \image html ssgi2off.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsSSGI : public PluginParams
{
public:
	//! Intensity of SSGI effect, 1 for default.
	float intensity;
	//! Radius (in meters) of SSGI effect.
	float radius;
	//! With angleBias 0, SSGI makes all edges visible. Increase to make only sharper edges visible. 0.1 is reasonable default.
	float angleBias;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsSSGI(const PluginParams* _next, float _intensity, float _radius, float _angleBias) : intensity(_intensity), radius(_radius), angleBias(_angleBias) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
