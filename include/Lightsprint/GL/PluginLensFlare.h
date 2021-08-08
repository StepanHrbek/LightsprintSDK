//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginLensFlare.h
//! \brief LightsprintGL | lens flare postprocess
//----------------------------------------------------------------------------

#ifndef PLUGINLENSFLARE_H
#define PLUGINLENSFLARE_H

#include "Plugin.h"
#include "Lightsprint/RRSolver.h"

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
	//! Collider used for occlusion testing.
	const rr::RRCollider* collider;
	//! Object with all scene geometry, used for occlusion testing.
	const rr::RRObject* scene;
	//! Number of rays shot. Higher quality makes effect of gradual occlusion smoother.
	unsigned quality;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsLensFlare(const PluginParams* _next, float _flareSize, unsigned _flareId, const rr::RRSolver* _solver, unsigned _quality) : flareSize(_flareSize), flareId(_flareId), lights(&_solver->getLights()), collider(_solver->getCollider()), scene(_solver->getMultiObject()), quality(_quality) {next=_next;}
	//! Convenience ctor, for setting plugin parameters.
	PluginParamsLensFlare(const PluginParams* _next, float _flareSize, unsigned _flareId, const rr::RRLights* _lights, const rr::RRCollider* _collider, const rr::RRObject* _scene, unsigned _quality) : flareSize(_flareSize), flareId(_flareId), lights(_lights), collider(_collider), scene(_scene), quality(_quality) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
