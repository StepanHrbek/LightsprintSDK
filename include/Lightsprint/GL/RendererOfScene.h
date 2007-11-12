//----------------------------------------------------------------------------
//! \file RendererOfScene.h
//! \brief LightsprintGL | renders contents of RRDynamicSolver instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/UberProgramSetup.h"

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
	//!  Vector of lights with realtime shadows and direct illumination.
	//! \param lightDirectMap
	//!  Texture projected by realtime area light.
	void setParams(const UberProgramSetup& uberProgramSetup, const rr::RRVector<AreaLight*>* lights, const Texture* lightDirectMap);

	//! Specifies data source - original scene geometry and illumination from given layer.
	//
	//! Original scene is exactly what you entered into solver (see RRDynamicSolver::setStaticObjects()).
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
	//! \n Indirect illumination is always taken directly from solver.
	//! \n Indirect illumination data types supported: LIGHT_INDIRECT_VCOLOR. Nothing is rendered
	//!  if you request LIGHT_INDIRECT_MAP (see uberProgramSetup in setParams()).
	void useOptimizedScene();

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(float brightness[4], float gamma);

	//! Clears screen and renders scene (sets shaders, feeds OpenGL with object's data selected by setParams()).
	//
	//! Use standard OpenGL way of setting camera matrices before calling render(), they will be respected
	//! by render().
	//!
	//! Note that although color buffer is cleared automatically here, depth buffer is not cleared.
	//! If you render with single realtime light with realtime shadows, clear depth buffer before rendering.
	//! If you render multiple lights into accumulation buffer, clear depth buffer only once,
	//! following render() calls will reuse it.
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
