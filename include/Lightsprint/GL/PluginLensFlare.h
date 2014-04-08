//----------------------------------------------------------------------------
//! \file PluginLensFlare.h
//! \brief LightsprintGL | lens flare postprocess
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINLENSFLARE_H
#define PLUGINLENSFLARE_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Lens flare plugin

//! Adds lens flare effect.
//
//! <table border=0><tr align=top><td>
//! \image html lensflare.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsLensFlare : public PluginParams
{
public:
	//! Relative size of flare, 1 for typical size.
	float flareSize;
	//! Various flare parameters are generated from this number.
	unsigned flareId;
	//! Collection of lights in scene.
	const rr::RRLights* lights;
	//! Object with all scene geometry, used for occlusion testing.
	const rr::RRObject* scene;
	//! Number of rays shot. Higher quality makes effect of gradual occlusion smoother.
	unsigned quality;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsLensFlare(const PluginParams* _next, float _flareSize, unsigned _flareId, const rr::RRLights* _lights, const rr::RRObject* _scene, unsigned _quality) : flareSize(_flareSize), flareId(_flareId), lights(_lights), scene(_scene), quality(_quality) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
