//----------------------------------------------------------------------------
//! \file RendererOfScene.h
//! \brief LightsprintGL | renders contents of RRDynamicSolver instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

//! OpenGL renderer of scene in RRDynamicSolver.
//
//! Renders original scene or optimized scene from solver.
//! Takes live illumination from solver or computed illumination from layer.
class RR_GL_API RendererOfScene : public Renderer
{
public:
	//! Creates renderer of scene in solver.
	//
	//! When renderer is created, default data source is optimized scene,
	//! as if useOptimizedScene() was called.
	//! \param solver
	//!  Adding/removing objects in solver while this renderer exists is not allowed,
	//!  results would be undefined.
	//! \param pathToShaders
	//!  Path to texture, sky and ubershader shaders, with trailing slash (if not empty or NULL).
	RendererOfScene(rr::RRDynamicSolver* solver, const char* pathToShaders);

	//! Sets parameters of render related to shader and direct illumination.
	//
	//! \param uberProgramSetup
	//!  Specifies shader properties, including type of indirect illumination,
	//!  vertex colors, ambient maps or none.
	//!  OBJECT_SPACE may be always cleared, it is set automatically when required.
	//! \param lights
	//!  Set of lights, source of direct illumination in rendered scene.
	//! \param renderingFromThisLight
	//!  When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	//! \param honourExpensiveLightingShadowingFlags
	//!  Enables slower rendering path that supports lighting or shadowing disabled via RRObject::getTriangleMaterial().
	void setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight, bool honourExpensiveLightingShadowingFlags);

	//! Specifies data source - original scene geometry and illumination from given layer.
	//
	//! Original scene is exactly what you entered into solver (see RRDynamicSolver::setStaticObjects()).
	//! \n Direct illumination: first light from lights passed to setParams().
	//! \n Indirect illumination is always taken from given layer.
	//! \n Indirect illumination data types supported: LIGHT_INDIRECT_VCOLOR, LIGHT_INDIRECT_MAP.
	//!  If both types are availabale (vertex buffer and texture), texture is used. 
	//!  Partially textured scenes are supported (some objects look better with ambient map,
	//!  some objects are good enough with vertex colors).
	//! \param layerNumber
	//!  Indirect illumination will be taken from given layer.
	void useOriginalScene(unsigned layerNumber);

	//! Specifies data source - original scene geometry and blended of illumination from given layers.
	void useOriginalSceneBlend(unsigned layerNumber1, unsigned layerNumber2, float layerBlend, unsigned layerNumberFallback);

	//! Specifies data source - optimized internal scene in solver and live illumination in solver.
	//
	//! Optimized scene may have fewer vertices and/or triangles because of optional vertex stitching
	//! and other optimizations.
	//! \n Direct illumination: all lights from setParams().
	//! \n Indirect illumination is always taken directly from solver.
	//!    Only LIGHT_INDIRECT_VCOLOR is supported. Nothing is rendered
	//!    if you request LIGHT_INDIRECT_MAP (see uberProgramSetup in setParams()).
	//! \n Maps (diffuse etc) are not supported.
	void useOptimizedScene();

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(const rr::RRVec4* brightness, float gamma);

	//! Clears screen and renders scene (sets shaders, feeds OpenGL with object's data selected by setParams()).
	//
	//! Use standard OpenGL way of setting camera projection and view matrices before calling render(),
	//! they are respected by render().
	//!
	//! Note that although color buffer is cleared automatically here, depth buffer is not cleared.
	//! You can simply clear it before render(), but you can also prerender depth
	//! to reduce overdraw in render().
	//! (Prerender of depth is not automatic because you may want to render also dynamic objects.
	//! We have access only to static objects.)
	virtual void render();

	//! For internal use.
	void renderLines(bool shortened, int onlyIndex=-1);

	virtual ~RendererOfScene();

private:
	bool  useOptimized;
	class RendererOfOriginalScene* renderer;
};

}; // namespace

#endif
