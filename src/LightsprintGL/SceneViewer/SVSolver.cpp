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
	solutionVersionInRealtimeLayer = 0;
}

SVSolver::~SVSolver()
{
	delete rendererOfScene;
}

void SVSolver::setStaticObjects(const rr::RRObjects& _objects, const SmoothingParameters* _smoothing, const char* _cacheLocation, rr::RRCollider::IntersectTechnique _intersectTechnique, RRDynamicSolver* _copyFrom)
{
	RRDynamicSolverGL::setStaticObjects(_objects,_smoothing,_cacheLocation,_intersectTechnique,_copyFrom);

	// allocate vertex buffers and specular cubemaps, just in case we need them later
	// both is used only by realtime per-object illumination
	for (unsigned i=0;i<getNumObjects();i++)
	{
		if (getObject(i) && getIllumination(i))
		{
			unsigned numVertices = getObject(i)->getCollider()->getMesh()->getNumVertices();
			unsigned numTriangles = getObject(i)->getCollider()->getMesh()->getNumTriangles();
			if (numVertices && numTriangles)
			{
				// allocate vertex buffers for LIGHT_INDIRECT_VCOLOR
				// (this should be called also if svs.realtimeLayerNumber changes... for now it never changes during solver existence)
				if (!getIllumination(i)->getLayer(svs.realtimeLayerNumber))
					getIllumination(i)->getLayer(svs.realtimeLayerNumber) =
						rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,rr::BF_RGBF,false,NULL);
				// allocate specular cubes for LIGHT_INDIRECT_CUBE_SPECULAR
				if (!getIllumination(i)->specularEnvMap)
				{
					// measure object's specularity
					float maxDiffuse = 0;
					float maxSpecular = 0;
					const rr::RRMaterial* previousMaterial = NULL;
					for (unsigned t=0;t<numTriangles;t++)
					{
						const rr::RRMaterial* material = getObject(i)->getTriangleMaterial(t,NULL,NULL);
						if (material && material!=previousMaterial)
						{
							previousMaterial = material;
							maxDiffuse = RR_MAX(maxDiffuse,material->diffuseReflectance.color.avg());
							maxSpecular = RR_MAX(maxSpecular,material->specularReflectance.color.avg());
						}
					}
					// continue only for highly specular objects
					if (maxSpecular>RR_MAX(0.01f,maxDiffuse*0.5f))
					{
						// measure object's size
						rr::RRVec3 mini,maxi;
						getObject(i)->getCollider()->getMesh()->getAABB(&mini,&maxi,NULL);
						rr::RRVec3 size = maxi-mini;
						float sizeMidi = size.sum()-size.maxi()-size.mini();
						// continue only for non-planar objects, cubical reflection looks bad on plane
						// (size is in object's space, so this is not precise for non-uniform scale)
						if (size.mini()>0.3*sizeMidi)
						{
							// allocate specular cube map
							rr::RRVec3 center;
							getObject(i)->getCollider()->getMesh()->getAABB(NULL,NULL,&center);
							const rr::RRMatrix3x4* matrix = getObject(i)->getWorldMatrix();
							if (matrix) matrix->transformPosition(center);
							getIllumination(i)->envMapWorldCenter = center;
							getIllumination(i)->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGBF,true,NULL);
							updateEnvironmentMapCache(getIllumination(i));
						}
					}
				}
			}
		}
	}
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
			|| (svs.renderLDM && getStaticObjects().size()>1) // optimized render can't render LDM for more than 1 object
			|| (uberProgramSetup.MATERIAL_SPECULAR && getStaticObjects().size()>1) // optimized render with 1 specular cube for whole scene looks bad, use this path with specular cube per-object
			)
		{
			// render per object (properly sorting), with temporary realtime-indirect buffers filled here
			// - update buffers
			if (solutionVersionInRealtimeLayer!=getSolutionVersion())
			{
				solutionVersionInRealtimeLayer = getSolutionVersion();

				// - update 2d buffers
				updateLightmaps(svs.realtimeLayerNumber,-1,-1,NULL,NULL,NULL);

				// - update cube buffers
				if (svs.renderSpecular)
				{
					//rr::RRReportInterval report(rr::INF1,"Update spec cubes.\n");
					for (unsigned i=0;i<getNumObjects();i++)
					{
						if (getIllumination(i) && getIllumination(i)->specularEnvMap)
						{
							updateEnvironmentMap(getIllumination(i));
						}
					}
				}
			}
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
