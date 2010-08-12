// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdarg>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "CameraObjectDistance.h"
#include "PreserveState.h"
#include "RendererOfMesh.h" // DDI_TRIANGLES_X/Y
#include "Workaround.h"
#include "tmpstr.h"

#define REPORT(a) //a

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

RRDynamicSolverGL::RRDynamicSolverGL(const char* _pathToShaders, DDIQuality _detectionQuality)
{
	strncpy(pathToShaders,_pathToShaders,299);
	pathToShaders[299]=0;

	switch(_detectionQuality)
	{
		case DDI_4X4:
		case DDI_8X8:
			detectionQuality = _detectionQuality;
			break;
		default:
			detectionQuality = Workaround::needsDDI4x4()?DDI_4X4:DDI_8X8;
			break;
	}
	unsigned faceSizeX = (detectionQuality==DDI_8X8)?8:4;
	unsigned faceSizeY = (detectionQuality==DDI_8X8)?8:4;
	rr::RRReporter::report(rr::INF1,"Detection quality: %s%s.\n",(_detectionQuality==DDI_AUTO)?"auto->":"",(detectionQuality==DDI_4X4)?"low":"high");

	// pointer 1 sent to RRBuffer::create = buffer will NOT allocate memory (we promise we won't touch it)
	detectBigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X*faceSizeX,DDI_TRIANGLES_MAX_Y*faceSizeY,1,rr::BF_RGBA,true,(unsigned char*)1),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
	detectSmallMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X,DDI_TRIANGLES_MAX_Y,1,rr::BF_RGBA,true,(unsigned char*)1),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);

	scaleDownProgram = Program::create(
		tmpstr("#define SIZEX %d\n#define SIZEY %d\n",faceSizeX,faceSizeY),
		tmpstr("%sscaledown_filter.vs",pathToShaders),
		tmpstr("%sscaledown_filter.fs",pathToShaders));
	if (!scaleDownProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %sscaledown_filter.*\n",pathToShaders);

	detectedDirectSum = NULL;
	detectedNumTriangles = 0;

	observer = NULL;
	oldObserverPos = rr::RRVec3(1e6);
	rendererOfScene = new rr_gl::RendererOfScene(_pathToShaders);
	uberProgram1 = UberProgram::create(tmpstr("%subershader.vs",pathToShaders),tmpstr("%subershader.fs",pathToShaders));
}

RRDynamicSolverGL::~RRDynamicSolverGL()
{
	for (unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
	delete rendererOfScene;
	delete uberProgram1;
	delete[] detectedDirectSum;
	delete scaleDownProgram;
	if (detectBigMap) delete detectBigMap->getBuffer();
	delete detectBigMap;
	if (detectSmallMap) delete detectSmallMap->getBuffer();
	delete detectSmallMap;
}

void RRDynamicSolverGL::setLights(const rr::RRLights& _lights)
{
	// create realtime lights
	RRDynamicSolver::setLights(_lights);
	for (unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
	realtimeLights.clear();
	for (unsigned i=0;i<_lights.size();i++)
	{
		RR_ASSERT(_lights[i]);
		realtimeLights.push_back(new RealtimeLight(*_lights[i]));
	}

	// adjust near/far to better match current scene
#pragma omp parallel for schedule(dynamic)
	for (int i=0;i<(int)_lights.size();i++)
	{
		const rr::RRLight* light = _lights[i];
		RR_ASSERT(light);
		RealtimeLight* realtimeLight = realtimeLights[i];
		RR_ASSERT(realtimeLight);

		if (light->type!=rr::RRLight::DIRECTIONAL)
		{
			CameraObjectDistance cod(getMultiObjectCustom());
			if (light->type==rr::RRLight::POINT)
			{
				// POINT
				cod.addPoint(light->position);
			}
			else
			{
				// SPOT
				for (unsigned i=0;i<realtimeLight->getNumShadowmaps();i++)
				{
					Camera* camera = realtimeLight->getShadowmapCamera(i);
					cod.addCamera(camera);
					delete camera;
				}
			}
			if (cod.getDistanceMax()>=cod.getDistanceMin()
				// better keep old range if detected distance is 0 (camera in wall?)
				&& cod.getDistanceMax()>0)
			{
				realtimeLight->getParent()->setRange(cod.getDistanceMin()*0.9f,cod.getDistanceMax()*5);
			}
		}
	}

	// reset detected direct lighting
	if (detectedDirectSum) memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
}

void RRDynamicSolverGL::reportDirectIlluminationChange(unsigned lightIndex, bool dirtyShadowmap, bool dirtyGI)
{
	RRDynamicSolver::reportDirectIlluminationChange(lightIndex,dirtyShadowmap,dirtyGI);
	if (lightIndex<realtimeLights.size())
	{
		realtimeLights[lightIndex]->dirtyShadowmap |= dirtyShadowmap;
		realtimeLights[lightIndex]->dirtyGI |= dirtyGI;
	}
}

void RRDynamicSolverGL::calculate(CalculateParameters* _params)
{
	REPORT(rr::RRReportInterval report(rr::INF3,"calculate...\n"));

	CalculateParameters params = _params ? *_params : CalculateParameters();

	// update only dirty maps
	updateShadowmaps();

	// early exit if quality=0
	// used in "no radiosity" part of Lightsmark
	if (params.qualityIndirectDynamic==0) return;

	// detect only dirty lights
	if (realtimeLights.size())
	{
		bool dirtyGI = false;
		for (unsigned i=0;i<realtimeLights.size();i++) dirtyGI |= realtimeLights[i]->dirtyGI && !realtimeLights[i]->shadowOnly;
		if (dirtyGI)
		{
			double now = GETSEC;
			if (fabs(now-lastDDITime)>=params.secondsBetweenDDI) // limits frequency of DDI
			{
				lastDDITime = now;
				setDirectIllumination(detectDirectIllumination());
			}
		}
	}
	else
	{
		// must be called at least once after all lights are removed
		// it's no performance problem to call it many times in row
		setDirectIllumination(NULL);
	}

	RRDynamicSolver::calculate(_params);
}

void RRDynamicSolverGL::updateShadowmaps()
{
	if (!getMultiObjectCustom()) return;

	PreserveViewport p1;
	PreserveMatrices p2;

	// after camera change, dirty travelling lights
	if (observer && observer->pos!=oldObserverPos)
	{
		//rr::RRReporter::report(rr::INF2,"Dirty directional lights.\n");
		oldObserverPos = observer->pos;
		for (unsigned i=0;i<realtimeLights.size();i++)
			if (realtimeLights[i]->getParent()->orthogonal && realtimeLights[i]->getNumShadowmaps())
			{
				// dirty shadowmap always
				realtimeLights[i]->dirtyShadowmap = true;
				// dirty GI only when observer moved too far
				// (direct illum is detected and indirect calculated only in orthoSize*orthoSize range around observer,
				//  so when observer moves far enough, we redetect direct illum automatically)
				if ((observer->pos-realtimeLights[i]->positionOfLastDDI).length()>realtimeLights[i]->getParent()->orthoSize/4)
				{
					realtimeLights[i]->dirtyGI = true;
				}
			}
	}

	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		RealtimeLight* light = realtimeLights[i];
		if (!light)
		{
			RR_ASSERT(0);
			continue;
		}

		// update non-dirlight position (this is probably redundant operation)
		//if (!light->getParent()->orthogonal || !light->getNumShadowmaps())
		//{
		//	light->getParent()->update();
		//}

		// update shadowmap[s]
		if (light->dirtyShadowmap)
		{
			// update dirlight position, it moves with observer camera
			//  but only if shadowmap changes, to prevent this error scenario:
			//   eye direction changes -> eye far automatically changes -> light far would change -> old CSM would render incorerectly
			if (light->getParent()->orthogonal && light->getNumShadowmaps())
			{
				light->getParent()->update(observer);
			}

			REPORT(rr::RRReportInterval report(rr::INF3,"Updating shadowmap (light %d)...\n",i));
			light->dirtyShadowmap = false;
			glColorMask(0,0,0,0);
			glEnable(GL_POLYGON_OFFSET_FILL);
			UberProgramSetup uberProgramSetup; // default constructor sets nearly all off, perfect for shadowmap
			switch(light->transparentMaterialShadows)
			{
				case RealtimeLight::RGB_SHADOWS:
					// not yet implemented
					break;
				case RealtimeLight::ALPHA_KEYED_SHADOWS:
					uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
					uberProgramSetup.MATERIAL_DIFFUSE = 1;
					uberProgramSetup.LIGHT_INDIRECT_CONST = 1; // without light, diffuse texture would be optimized away
					break;
				case RealtimeLight::FULLY_OPAQUE_SHADOWS:
					uberProgramSetup.MATERIAL_CULLING = 0;
					break;
				default:
					RR_ASSERT(0);
			}
			for (unsigned i=0;i<light->getNumShadowmaps();i++)
			{
				Camera* lightInstance = light->getShadowmapCamera(i);
				lightInstance->setupForRender();
				delete lightInstance;
				Texture* shadowmap = light->getShadowmap(i);
				if (!shadowmap)
				{
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (texture=NULL).\n"));
				}
				else
				{
					float slopeBias = (light->getNumShadowSamples(i)==1)?1.f:4.f;
					float fixedBias = (light->getRRLight().type==rr::RRLight::POINT)?slopeBias*4:slopeBias;
					Workaround::needsIncreasedBias(slopeBias,fixedBias,light->getRRLight());
					glPolygonOffset(slopeBias,fixedBias);
					if (!shadowmap->getBuffer())
					{
						RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (buffer=NULL).\n"));
					}
					else
					{
						glViewport(0, 0, shadowmap->getBuffer()->getWidth(), shadowmap->getBuffer()->getHeight());
						if (!shadowmap->renderingToBegin())
						{
							// 8800GTS returns this in some near out of memory situations, perhaps with texture that already failed to initialize
							RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (FBO).\n"));
						}
						else
						{
							glClear(GL_DEPTH_BUFFER_BIT);
							renderScene(uberProgramSetup,&light->getRRLight(),false,-1,-1,0,NULL,1);
						}
					}
				}
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			glColorMask(1,1,1,1);
			if (light->getNumShadowmaps())
				if (light->getShadowmap(0)) // shadowmap must exist. FBO shutdown skipped if shadowmap does not exist (because someone set shadowmap size 0)
					light->getShadowmap(0)->renderingToEnd();
		}
	}
}

const unsigned* RRDynamicSolverGL::detectDirectIllumination()
{
	if (!getMultiObjectCustom()) return NULL;
	unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
	if (!numTriangles) return NULL;

	PreserveViewport p1;
	PreserveMatrices p2;

	// alloc space for detected direct illum
	if (numTriangles!=detectedNumTriangles)
	{
		delete[] detectedDirectSum;
		detectedNumTriangles = numTriangles;
		detectedDirectSum = new unsigned[detectedNumTriangles];
		memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
	}

	// update smallMapsCPU
	unsigned updatedSmallMaps = 0;
	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		RealtimeLight* light = realtimeLights[i];
		if (!light)
		{
			RR_ASSERT(0);
			continue;
		}
		if (numTriangles!=light->numTriangles)
		{
			delete[] light->smallMapCPU;
			light->numTriangles = numTriangles;
			light->smallMapCPU = new unsigned[numTriangles+2047]; // allocate more so we can read complete last line from GPU, including unused values
			memset(light->smallMapCPU,0,sizeof(unsigned)*(numTriangles+2047)); // shadowOnly is not updated, but it is accumulated
			light->dirtyGI = true;
		}

		// update smallmap
		if (light->dirtyGI)
		{
			light->dirtyGI = false;
			if (!light->shadowOnly)
			{
				if (observer) light->positionOfLastDDI = observer->pos;
				updatedSmallMaps += detectDirectIlluminationTo(light,light->smallMapCPU,light->numTriangles);
			}
		}
	}

	// sum smallMapsCPU into detectedDirectSum
	if (updatedSmallMaps && realtimeLights.size()>1)
	{
		unsigned numLights = realtimeLights.size();
#pragma omp parallel for
		for (int b=0;b<(int)numTriangles*4;b++)
		{
			unsigned sum = 0;
			for (unsigned l=0;l<numLights;l++)
				sum += ((unsigned char*)realtimeLights[l]->smallMapCPU)[b];
			((unsigned char*)detectedDirectSum)[b] = RR_CLAMPED(sum,0,255);
		}
	}

	if (realtimeLights.size()==1) return realtimeLights[0]->smallMapCPU;
	return detectedDirectSum;
}

unsigned RRDynamicSolverGL::detectDirectIlluminationTo(RealtimeLight* ddiLight, unsigned* _results, unsigned _space)
{
	if (!scaleDownProgram || !_results)
	{
		RR_ASSERT(0);
		return 0;
	}

	REPORT(rr::RRReportInterval report(rr::INF1,"detectDirectIllumination()\n"));

	if (!getMultiObjectCustom())
		return 0;

	const rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	if (!numTriangles)
	{
		RR_ASSERT(0); // legal, but should not happen in well coded program
		return 0;
	}

	// preserve render states, any changes will be restored at return from this function
	PreserveViewport p1;
	PreserveClearColor p2;
	PreserveCullFace p3;
	PreserveDepthMask p4;
	PreserveDepthTest p5;

	// setup render states
	glClearColor(0,0,0,1);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

	// adjust captured texture size so we don't waste pixels
	unsigned faceSizeX = (detectionQuality==DDI_8X8)?8:4;
	unsigned faceSizeY = (detectionQuality==DDI_8X8)?8:4;

	// calculate mapping for FORCED_2D
	//  We have working space for 256x256 tringles, so scene with 80k triangles must be split to two passes.
	//  We do 256x157,256x157 passes, but 256x256,256x57 would work too.
	//  This calculation is repeated in MeshArraysVBOs where we render to texture, here we read texture back.
	unsigned triCountX = DDI_TRIANGLES_X;
	unsigned triCountYTotal = (numTriangles+DDI_TRIANGLES_X-1)/DDI_TRIANGLES_X;
	unsigned numPasses = (triCountYTotal+DDI_TRIANGLES_MAX_Y-1)/DDI_TRIANGLES_MAX_Y;
	unsigned triCountYInOnePass = (triCountYTotal+numPasses-1)/numPasses;
	unsigned pixelWidth = triCountX*faceSizeX;
	unsigned pixelHeightInOnePass = triCountYInOnePass*faceSizeY;

	// for each set of triangles (if all triangles don't fit into one texture)
	for (unsigned firstCapturedTriangle=0;firstCapturedTriangle<numTriangles;firstCapturedTriangle+=triCountX*triCountYInOnePass)
	{
		unsigned lastCapturedTrianglePlus1 = RR_MIN(numTriangles,firstCapturedTriangle+triCountX*triCountYInOnePass);
		unsigned triCountYInThisPass = (lastCapturedTrianglePlus1-firstCapturedTriangle+triCountX-1)/triCountX; // may be bit lower in last pass of multipass, this prevents writing too far beyond end of _results

		// prepare for scaling down -> render to texture
		detectBigMap->renderingToBegin();

		// clear
		glViewport(0, 0, pixelWidth,pixelHeightInOnePass);
		glClear(GL_COLOR_BUFFER_BIT); // old pixels are overwritten, so clear is usually not needed, but individual light-triangle lighting may be disabled by getTriangleMaterial()=triangles are not rendered, and we need to detect 0 rather than uninitialized value

		// setup shader
		UberProgramSetup uberProgramSetup;
		uberProgramSetup.SHADOW_MAPS = (ddiLight->getRRLight().type==rr::RRLight::POINT)?ddiLight->getNumShadowmaps():(ddiLight->getNumShadowmaps()?1:0);
		uberProgramSetup.SHADOW_SAMPLES = uberProgramSetup.SHADOW_MAPS?1:0; // for 1-light render, won't be reset by MultiPass
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_COLOR = ddiLight->getRRLight().color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = uberProgramSetup.SHADOW_MAPS && ddiLight->getProjectedTexture();
		uberProgramSetup.LIGHT_DIRECTIONAL = ddiLight->getParent()->orthogonal;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = ddiLight->getRRLight().type==rr::RRLight::SPOT && !ddiLight->getProjectedTexture();
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_CULLING = false;
		uberProgramSetup.FORCE_2D_POSITION = true;
		Program* program = uberProgramSetup.useProgram(uberProgram1,ddiLight,0,NULL,1,NULL);
		if (!program)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"setupShader: Failed to compile or link GLSL program.\n"));
		}

		// render scene
		glDisable(GL_CULL_FACE);
		rendererOfScene->getRendererOfMesh(getMultiObjectCustom()->getCollider()->getMesh())->render(
			program,
			getMultiObjectCustom(),
			&FaceGroupRange(0,0,0,firstCapturedTriangle,lastCapturedTrianglePlus1),
			1,
			uberProgramSetup,
			NULL,
			NULL);

		// downscale 10pixel triangles in 4x4 squares to single pixel values
		detectSmallMap->renderingToBegin();
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/detectBigMap->getBuffer()->getWidth(),1.0f/detectBigMap->getBuffer()->getHeight());
		glViewport(0,0,DDI_TRIANGLES_X,DDI_TRIANGLES_MAX_Y);
		glActiveTexture(GL_TEXTURE0);
		detectBigMap->bindTexture();
		glDisable(GL_CULL_FACE);
		glBegin(GL_POLYGON);
			glMultiTexCoord2f(GL_TEXTURE0,0,0);
			glVertex2f(-1,-1);
			float fractionOfBigMapUsed = 1.0f*triCountYInOnePass/DDI_TRIANGLES_MAX_Y; // we downscale only part of big map that is used
			glMultiTexCoord2f(GL_TEXTURE0,0,fractionOfBigMapUsed);
			glVertex2f(-1,fractionOfBigMapUsed*2-1);
			glMultiTexCoord2f(GL_TEXTURE0,1,fractionOfBigMapUsed);
			glVertex2f(1,fractionOfBigMapUsed*2-1);
			glMultiTexCoord2f(GL_TEXTURE0,1,0);
			glVertex2f(1,-1);
		glEnd();

		// read downscaled image to memory
		REPORT(rr::RRReportInterval report(rr::INF3,"glReadPix %dx%d\n", triCountX, triCountY));
		RR_ASSERT(_space+2047>=firstCapturedTriangle+triCountX*triCountYInThisPass);
		glReadPixels(0, 0, triCountX, triCountYInThisPass, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _results+firstCapturedTriangle);
		detectSmallMap->renderingToEnd();
	}

	return 1;
}

void drawCamera(Camera* camera)
{
	if (camera)
	{
		glPushMatrix();
		glMultMatrixd(camera->inverseViewMatrix);
		glMultMatrixd(camera->inverseFrustumMatrix);
		glBegin(GL_LINE_STRIP);
		glColor3f(0,0,0);
		glVertex3f(1,1,1);
		glColor3f(1,1,1);
		glVertex3f(1,1,-1);
		glVertex3f(-1,1,-1);
		glColor3f(0,0,0);
		glVertex3f(-1,1,1);
		glVertex3f(1,1,1);
		glVertex3f(1,-1,1);
		glVertex3f(-1,-1,1);
		glColor3f(1,1,1);
		glVertex3f(-1,-1,-1);
		glVertex3f(1,-1,-1);
		glColor3f(0,0,0);
		glVertex3f(1,-1,1);
		glEnd();
		glBegin(GL_LINES);
		glColor3f(1,1,1);
		glVertex3f(1,1,-1);
		glVertex3f(1,-1,-1);
		glVertex3f(-1,1,-1);
		glVertex3f(-1,-1,-1);
		glColor3f(0,0,0);
		glVertex3f(-1,1,1);
		glVertex3f(-1,-1,1);
		glEnd();
		glPopMatrix();
	}
}

void drawRealtimeLight(RealtimeLight* light)
{
	if (light)
	{
		if (light->getNumShadowmaps())
		{
			// light with shadows has all instances rendered
			for (unsigned i=0;i<light->getNumShadowmaps();i++)
			{
				Camera* camera = light->getShadowmapCamera(i);
				drawCamera(camera);
				delete camera;
			}
		}
		else
		{
			// light without shadows has parent rendered
			drawCamera(light->getParent());
		}
	}
}

void RRDynamicSolverGL::renderScene(
		const UberProgramSetup& _uberProgramSetup,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect,
		unsigned _lightIndirectLayer,
		int _lightDetailMapLayer,
		float* _clipPlanes,
		const rr::RRVec4* _brightness,
		float _gamma)
{
	rendererOfScene->render(
		this,
		_uberProgramSetup,
		_uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL,
		_renderingFromThisLight,
		_updateLightIndirect,
		_lightIndirectLayer,
		_lightDetailMapLayer,
		_clipPlanes,
		_brightness,
		_gamma);
}

void RRDynamicSolverGL::renderLights()
{
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
	uberProgramSetup.MATERIAL_DIFFUSE = 1;
	Program* program = uberProgramSetup.useProgram(uberProgram1,NULL,0,NULL,1,NULL);
	for (unsigned i=0;i<getLights().size();i++)
	{
		drawRealtimeLight(realtimeLights[i]);
	}
}

}; // namespace
