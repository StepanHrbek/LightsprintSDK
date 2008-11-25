// --------------------------------------------------------------------------
// Scene viewer - solver for GI lighting.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "SVSolver.h"

namespace rr_gl
{

SVSolver::SVSolver(const char* _pathToShaders, SceneViewerState& _svs) : RRDynamicSolverGL(_pathToShaders), svs(_svs)
{
	pathToShaders = _strdup(_pathToShaders);
	rendererOfScene = new RendererOfScene(this,pathToShaders);
}

SVSolver::~SVSolver()
{
	free(pathToShaders);
	delete rendererOfScene;
}

void SVSolver::resetRenderCache()
{
	delete rendererOfScene;
	rendererOfScene = new RendererOfScene(this,pathToShaders);
}

void SVSolver::renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
{
	const RealtimeLights* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

	// render static scene
	rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight);

	if (svs.renderRealtime)
	{
		if ( (!renderingFromThisLight && getMaterialsInStaticScene().MATERIAL_TRANSPARENCY_BLEND) // optimized render can't sort
			|| (svs.renderLDM && getStaticObjects().size()>1) // optimized render can't render AO for more than 1 object
			)
		{
			// render per object (properly sorting), with temporary buffers filled here
			if (!getIllumination(0)->getLayer(svs.realtimeLayerNumber))
			{
				// allocate lightmap-buffers for realtime rendering
				for (unsigned i=0;i<getNumObjects();i++)
				{
					if (getIllumination(i) && getObject(i) && getObject(i)->getCollider()->getMesh()->getNumVertices())
						getIllumination(i)->getLayer(svs.realtimeLayerNumber) =
							rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
				}
			}
			updateLightmaps(svs.realtimeLayerNumber,-1,-1,NULL,NULL,NULL);
			rendererOfScene->useOriginalScene(svs.realtimeLayerNumber);
		}
		else
		{
			// render whole scene at once (no sorting -> semitranslucency would render incorrectly)
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
