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
// RendererOfScene

//! OpenGL renderer of RRDynamicSolver.
//! Solver is not const, because its vertex/pixel buffers may change(create/update) at render time.
class RR_API RendererOfScene : public de::Renderer
{
public:
	RendererOfScene(rr::RRDynamicSolver* solver);

	void setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfScene();

private:
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
	de::Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
	de::UberProgram* uberProgram;
	unsigned solutionVersion;
};

}; // namespace

#endif
