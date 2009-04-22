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
		if ( (!renderingFromThisLight && uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND) // optimized render can't sort
			|| (svs.renderLDM && getStaticObjects().size()>1) // optimized render can't render AO for more than 1 object
			)
		{
			// render per object (properly sorting), with temporary realtime-indirect buffers filled here
			// - allocate buffers
			for (unsigned i=0;i<getNumObjects();i++)
			{
				if (getIllumination(i) && getObject(i) && getObject(i)->getCollider()->getMesh()->getNumVertices())
				{
					if (getIllumination(i)->getLayer(svs.realtimeLayerNumber))
					{
						// break from cycle as early as possible to avoid checking everything in each frame
						// but don't skip creating buffers TOO early, this is right moment
						// previously we skipped creation in case object(0) had buffer (caused memory leak if object 0 had 0 triangles)
						break;
					}
					getIllumination(i)->getLayer(svs.realtimeLayerNumber) =
						rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
				}
			}
			// - update buffers
			updateLightmaps(svs.realtimeLayerNumber,-1,-1,NULL,NULL,NULL);
			// - render scene with buffers
			rendererOfScene->useOriginalScene(svs.realtimeLayerNumber);
		}
		else
		{
			// render whole scene at once (no sort)
			// (whole scene is blended or keyed -> scene with trees and windows renders incorrectly)
			rendererOfScene->useOptimizedScene();
		}
	}
	else
	{
		// render per object (properly sorting), with static lighting buffers
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
