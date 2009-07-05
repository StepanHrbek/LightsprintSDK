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
	// verify that settings are legal
	RR_ASSERT((svs.renderLightDirect==LD_STATIC_LIGHTMAPS) == (svs.renderLightIndirect==LI_STATIC_LIGHTMAPS));

	const RealtimeLights* lights = (uberProgramSetup.LIGHT_DIRECT && svs.renderLightDirect==LD_REALTIME) ? &realtimeLights : NULL;

	// render static scene
	rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight);
	if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)
	{
		// changes in svs.staticLayerNumber illumination are not tracked,
		//  but getSolutionVersion() should work as it is incremented during lightmap build
		rendererOfScene->useOriginalScene(svs.staticLayerNumber,getSolutionVersion());
	}
	else
	{
		rendererOfScene->useRealtimeGI(svs.realtimeLayerNumber);
	}
	rendererOfScene->setLDM((svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM) ? svs.ldmLayerNumber : UINT_MAX);
	rendererOfScene->setBrightnessGamma(&svs.brightness,svs.gamma);
	rendererOfScene->setClipPlane(svs.waterLevel);
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
