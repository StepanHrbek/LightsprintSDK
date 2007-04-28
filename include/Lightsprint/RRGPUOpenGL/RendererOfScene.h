// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/DemoEngine/Renderer.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

//! OpenGL renderer of scene in RRDynamicSolver.
//
//! Renders original scene or optimized scene from solver.
//! Takes live illumination from solver or computed illumination from channel.
//! Optionally updates illumination in channel.
class RR_API RendererOfScene : public de::Renderer
{
public:
	//! Creates renderer of scene in solver.
	//
	//! Adding/removing objects in solver while this renderer exists is not allowed,
	//! results would be undefined.
	RendererOfScene(rr::RRDynamicSolver* solver);

	//! Sets parameters of render related to shader and direct illumination.
	//
	//! \param uberProgramSetup
	//!  Specifies shader properties, including type of indirect illumination,
	//!  vertex colors, ambient maps or none.
	//! \param areaLight
	//!  Area light with realtime shadows and direct illumination.
	//!  \n \n To render scene with multiple realtime area lights with realtime shadows, 
	//!  render scene several times, each time with different light, and accumulate
	//!  results in accumulation buffer. You may use single renderer for rendering with
	//!  multiple area lights.
	//!  \n \n It is also possible to tweak AreaLight to render multiple spotlights
	//!  with realtime shadows in one pass (see AreaLight::instanceMakeup()).
	//! \param lightDirectMap
	//!  Texture projected by realtime area light.
	void setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap);

	//! Specifies data source - original scene geometry and illumination from given channel.
	//
	//! Original scene is exactly what you entered into solver (see RRDynamicSolver::setObjects()).
	//! \n Indirect illumination is always taken from given channel.
	//! \n Indirect illumination data types supported: LIGHT_INDIRECT_VCOLOR, LIGHT_INDIRECT_MAP.
	//! \param channelNumber
	//!  Indirect illumination will be taken from given channel.
	void useOriginalScene(unsigned channelNumber);

	//! Specifies data source - optimized internal scene in solver and live illumination in solver.
	//
	//! Optimized scene may have fewer vertices and/or triangles because of optional vertex stitching
	//! and other optimizations.
	//! \n Indirect illumination is always taken directly from solver.
	//! \n Indirect illumination data types supported: LIGHT_INDIRECT_VCOLOR.
	void useOptimizedScene();

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfScene();

private:
	bool  useOptimized;
	class RendererOfOriginalScene* renderer;
};

}; // namespace

#endif
