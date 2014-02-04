//----------------------------------------------------------------------------
//! \file PluginScene.h
//! \brief LightsprintGL | renders given objects and light
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINSCENE_H
#define PLUGINSCENE_H

#include "Plugin.h"
#include "RealtimeLight.h"
#include "UberProgramSetup.h"
#include "RRDynamicSolverGL.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Render scene plugin

//! Renders scene, set of objects and lights.
//
//! With all parameters at defaults, PluginScene renders contents of solver, all static and dynamic objects and lights.
//! If you modify objects and lights variables, you can render arbitrary objects and lights instead.
class RR_GL_API PluginParamsScene : public PluginParams
{
public:
	//! Source of static and dynamic objects and illumination. Direct lights from solver are ignored if you set #lights differently.
	rr::RRDynamicSolver* solver;

	//! Objects to be rendered, or NULL for all objects from solver.
	//! \n NULL: All objects from solver are rendered. You can request renderer to update illumination buffers with data from solver.
	//! \n non-NULL: Given objects are rendered. Possible request to update illumination buffers at render time is ignored.
	//! \n When rendering all objects from solver, it is recommended to pass NULL rather than collection of all objects, as it could be faster.
	const rr::RRObjects* objects;

	//!  Set of lights, source of direct illumination in rendered scene.
	const RealtimeLights* lights;

	//! Specifies shader properties to be allowed during render.
	//
	//! For rendering with all light and material features,
	//! use UberProgramSetup::enableAllLights() and UberProgramSetup::enableAllMaterials().
	//!
	//! For brightness/gamma correction, it is sufficient to set PluginParamsShared::brightness/gamma.
	//! If you don't set POSTPROCESS_BRIGHTNESS/POSTPROCESS_GAMMA, plugin sets them automatically according to 
	//! SharedPluginParams::brightness/gamma. This gives you top fps, but system might need to compile multiple shaders
	//! (depending on brightness/gamma values), so startup might be slower.
	//! If you set POSTPROCESS_BRIGHTNESS/POSTPROCESS_GAMMA, they stay set even if not necessary, so startup is faster,
	//! but shader executes few extra instructions even if they are not needed, so fps might be super tiny bit lower.
	UberProgramSetup uberProgramSetup;

	//! When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	const rr::RRLight* renderingFromThisLight;

	//! False = renders illumination as it is stored in buffers, without updating it.
	//! True = updates illumination in _layerLightmap and _layerEnvironment layers before rendering it. Updates only outdated buffers, only buffers being rendered.
	//! Note that renderer does not allocated or deleted buffers.
	//! You can allocate buffers in advance by calling RRDynamicSolver::allocateBuffersForRealtimeGI() once.
	bool updateLayers;

	//! Indirect illumination is taken from and possibly updated in given layer.
	//! Renderer touches only existing buffers, does not allocate new ones.
	unsigned layerLightmap;

	//! Environment is taken from and possibly updated in given layer.
	//! Renderer touches only existing buffers, does not allocate new ones.
	unsigned layerEnvironment;

	//! Specifies source of light detail maps. Renderer only reads them.
	unsigned layerLDM;

	//! For diagnostic use only. 0=automatic, 1=force rendering singleobjects, 2=force rendering multiobject.
	unsigned forceObjectType;

	//! Specifies time from start of animation. At the moment, only water shader uses it for animated waves.
	float animationTime;

	//! Specifies clipping of rendered geometry.
	ClipPlanes clipPlanes;

	//! True = renders polygons as wireframe.
	bool wireframe;

	//! Convenience ctor, for setting some plugin parameters. You still might want to change default values of some other parameters after ctor.
	PluginParamsScene(const PluginParams* _next, RRDynamicSolverGL* _solver)
	{
		next = _next;
		solver = _solver;
		objects = NULL;
		lights = _solver?&_solver->realtimeLights:NULL;
		renderingFromThisLight = NULL;
		updateLayers = false;
		layerLightmap = UINT_MAX;
		layerEnvironment = UINT_MAX;
		layerLDM = UINT_MAX;
		forceObjectType = 0;
		animationTime = 0;
		clipPlanes.clipPlane = rr::RRVec4(1,0,0,0);
		clipPlanes.clipPlaneXA = 0;
		clipPlanes.clipPlaneXB = 0;
		clipPlanes.clipPlaneYA = 0;
		clipPlanes.clipPlaneYB = 0;
		clipPlanes.clipPlaneZA = 0;
		clipPlanes.clipPlaneZB = 0;
		wireframe = false;
	}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
