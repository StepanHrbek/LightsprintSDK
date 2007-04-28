// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/DemoEngine/Renderer.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"
#include "Lightsprint/RRGPUOpenGL.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRDynamicSolver

//! OpenGL renderer of RRDynamicSolver internal scene.
//
//! Renders contents of solver, geometry and illumination.
//! Geometry may be slightly different from original scene, because of optional internal optimizations.
//! Illumination is taken directly from solver,
//! renderer doesn't use or modify precomputed illumination in channels.
class RR_API RendererOfRRDynamicSolver : public de::Renderer
{
public:
	RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver);

	//! Sets parameters of render related to shader and direct illumination.
	void setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfRRDynamicSolver();

protected:
	struct Params
	{
		rr::RRDynamicSolver* solver;
		de::UberProgramSetup uberProgramSetup;
		const de::AreaLight* areaLight;
		const de::Texture* lightDirectMap;
		Params();
	};
	Params params;
	de::TextureRenderer* textureRenderer;
	de::UberProgram* uberProgram;
private:
	// 1 renderer for 1 scene
	unsigned solutionVersion;
	de::Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

//! OpenGL renderer of scene you entered into RRDynamicSolver.
//
//! Renders original scene and illumination from channels.
//! Geometry is exactly what you entered into the solver.
//! Illumination is taken from given channel, not from solver.
//! Solver is not const, because its vertex/pixel buffers may change(create/update) at render time.
class RR_API RendererOfScene : public RendererOfRRDynamicSolver
{
public:
	RendererOfScene(rr::RRDynamicSolver* solver);

	//! Sets source of indirect illumination. It is channel 0 by default.
	//! \param channelNumber
	//!  Indirect illumination will be taken from given channel.
	void setIndirectIlluminationSource(unsigned channelNumber);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	//! setParams()
	virtual void render();

	virtual ~RendererOfScene();

private:
	// n renderers for n objects
	unsigned channelNumber;
	std::vector<de::Renderer*> renderersCaching;
	std::vector<RendererOfRRObject*> renderersNonCaching;
};

}; // namespace

#endif
