//----------------------------------------------------------------------------
//! \file RendererOfScene.h
//! \brief LightsprintGL | renders contents of RRDynamicSolver instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

//! OpenGL renderer of scene in RRDynamicSolver.
//
//! Renders scene from solver.
//! Takes live illumination from solver or computed illumination from layer.
class RR_GL_API RendererOfScene
{
public:
	//! Creates renderer.
	//
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	RendererOfScene(const char* pathToShaders);
	~RendererOfScene();

	//! Renders scene from solver.
	//!
	//! Use standard OpenGL way of setting camera projection and view matrices before calling render(),
	//! they are respected by render(). You can also use rr_gl::Camera::setupBeforeRender() to do it.
	//!
	//! Render target is not cleared automatically, so you may want to clear both color and depth before calling render().
	//!
	//! \param _solver
	//!  Source of static and dynamic objects and environment. Direct lights from solver are ignored.
	//! \param _uberProgramSetup
	//!  Specifies shader properties, including light and material types supported.
	//!  For render with all direct lighting/shadowing features according to light properties,
	//!  enable at least SHADOW_MAPS,LIGHT_DIRECT,LIGHT_DIRECT_COLOR,LIGHT_DIRECT_MAP,LIGHT_DIRECT_ATT_SPOT;
	//!  if you disabled one of them, it will stay disabled for all lights.
	//! \param _lights
	//!  Set of lights, source of direct illumination in rendered scene.
	//! \param _renderingFromThisLight
	//!  When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	//! \param _updateLightIndirect
	//!  When rendering with LIGHT_INDIRECT_VCOLOR, updates vertex buffers with version different from getSolutionVersion().
	//!  Function touches only existing buffers, does not allocate new ones.
	//!  You can allocate buffers in advance by calling RRDynamicSolver::allocateBuffersForRealtimeGI() once.
	//! \param _lightIndirectLayer
	//!  Indirect illumination is taken from and possibly updated to given layer.
	//!  Function touches only existing buffers, does not allocate new ones.
	//! \param _lightDetailMapLayer
	//!  Specifies source of light detail maps.
	//! \param _clipPlanes
	//!  NULL or array of six floats specifying max x, min x, max y, min y, max z, min z coordinates of rendered geometry.
	//! \param _brightness
	//!  Specifies global brightness.
	//! \param _gamma
	//!  Specifies global gamma (contrast) factor.
	void render(
		rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect,
		unsigned _lightIndirectLayer,
		int _lightDetailMapLayer,
		float* _clipPlanes,
		const rr::RRVec4* _brightness,
		float _gamma);

	class RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh);
private:
	class RendererOfOriginalScene* renderer;
};

}; // namespace

#endif
