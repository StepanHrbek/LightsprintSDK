//----------------------------------------------------------------------------
//! \file RendererOfScene.h
//! \brief LightsprintGL | renders contents of RRDynamicSolver instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
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
	void setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight);

	//! Specifies data source - original meshes and illumination present in given layer.
	//
	//! Original meshes are exactly what you entered into solver (see RRDynamicSolver::setStaticObjects()).
	//! \n Direct illumination: lights passed to setParams().
	//! \n Indirect illumination is always taken from given layer.
	//!    Format of indirect illumination is affected by LIGHT_INDIRECT_XXX you send to setParams().
	//! \param layerNumber
	//!  Indirect illumination is taken from given layer.
	//! \param lightIndirectVersion
	//!  Version of illumination in layer, increment it only when illumination changes
	//!  to prevent unnecessary CPU-GPU data transfers.
	void useOriginalScene(unsigned layerNumber, unsigned lightIndirectVersion);

	//! Specifies data source - original meshes and illumination blend from given layers.
	void useOriginalSceneBlend(unsigned layerNumber1, unsigned layerNumber2, float layerBlend, unsigned layerNumberFallback, unsigned lightIndirectVersion);

	//! Specifies data source - optimized meshes and live illumination from solver.
	//
	//! Optimized scene may have fewer vertices and/or triangles because of optional vertex stitching
	//! and other optimizations.
	//! Optimized scene rendering is faster, but objects are not sorted,
	//! so MATERIAL_TRANSPARENCY_BLEND is not supported (scene is rendered incorrectly).
	//! \n Direct illumination: all lights from setParams().
	//! \n Indirect illumination is always taken directly from solver.
	//!    Format of indirect illumination is affected by LIGHT_INDIRECT_XXX you send to setParams().
	//!    Use LIGHT_INDIRECT_VCOLOR to select per-vertex format, LIGHT_INDIRECT_MAP is not supported.
	void useOptimizedScene();
	//! Diagnostic aid, returns whether optimized or original mesh was rendered last time.
	bool usingOptimizedScene();

	//! Specifies data source - live illumination from solver, any suitable meshes.
	//
	//! This is recommended for realtime GI rendering.
	//! Depending on parameters sent to setParams(), one of two internal paths is selected.
	//! Fast path is used whenever possible, it is equivalent to calling useOptimizedScene().
	//! Slower but more capable path is equivalent to updating GI buffers in layer and calling useOriginalScene().
	//! \n Direct illumination: all lights from setParams().
	//! \n Indirect illumination is always taken directly from solver.
	//!    Format of indirect illumination is affected by LIGHT_INDIRECT_XXX you send to setParams(),
	//!    use LIGHT_INDIRECT_auto to let renderer choose for you.
	void useRealtimeGI(unsigned layerNumber);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(const rr::RRVec4* brightness, float gamma);

	//! Specifies global clip plane used by following render() commands.
	void setClipPlane(float clipPlaneY);

	//! Specifies source of light detail maps.
	void setLDM(unsigned layerNumberLDM);

	//! Renders scene (sets shaders, feeds OpenGL with object's data selected by setParams()).
	//
	//! Use standard OpenGL way of setting camera projection and view matrices before calling render(),
	//! they are respected by render().
	//!
	//! Render target is not cleared automatically, so you may want to clear both color and depth before calling render().
	virtual void render();

	//! For internal use.
	void renderLines(bool shortened, int onlyIndex=-1);

	virtual ~RendererOfScene();

private:
	enum DataSource {DS_LAYER_MESHES,DS_REALTIME_MULTIMESH,DS_REALTIME_AUTO} dataSource;
	class RendererOfOriginalScene* renderer;
};

}; // namespace

#endif
