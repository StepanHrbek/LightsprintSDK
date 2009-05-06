// --------------------------------------------------------------------------
// Scene viewer - solver for GI lighting.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVSolver.h"

namespace rr_gl
{

SVSolver::SVSolver(SceneViewerStateEx& _svs) : RRDynamicSolverGL(_svs.pathToShaders), svs(_svs)
{
	rendererOfScene = new RendererOfScene(this,svs.pathToShaders);
}

SVSolver::~SVSolver()
{
	delete rendererOfScene;
}

void SVSolver::resetRenderCache()
{
	delete rendererOfScene;
	rendererOfScene = new RendererOfScene(this,svs.pathToShaders);
}

void SVSolver::renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
{
	const RealtimeLights* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

	// render static scene
	rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight);
	if (svs.renderRealtime)
	{
		rendererOfScene->useRealtimeGI(svs.realtimeLayerNumber);
	}
	else
	{
		rendererOfScene->useOriginalScene(svs.staticLayerNumber);
	}
	rendererOfScene->setLDM(svs.renderLDM ? svs.ldmLayerNumber : UINT_MAX);
	rendererOfScene->setBrightnessGamma(&svs.brightness,svs.gamma);
	rendererOfScene->render();
}

void SVSolver::dirtyLights()
{
	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		reportDirectIlluminationChange(i,true,true);
	}
}

}; // namespace
