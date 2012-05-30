// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdarg>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgramSetup.h"
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
	rr::RRReporter::report(rr::INF2,"Detection quality: %s%s.\n",(_detectionQuality==DDI_AUTO)?"auto->":"",(detectionQuality==DDI_4X4)?"low":"high");

	// pointer 1 sent to RRBuffer::create = buffer will NOT allocate memory (we promise we won't touch it)
	detectBigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X*faceSizeX,DDI_TRIANGLES_MAX_Y*faceSizeY,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
	detectSmallMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X,DDI_TRIANGLES_MAX_Y,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);

	scaleDownProgram = Program::create(
		tmpstr("#define SIZEX %d\n#define SIZEY %d\n",faceSizeX,faceSizeY),
		tmpstr("%sscaledown_filter.vs",pathToShaders),
		tmpstr("%sscaledown_filter.fs",pathToShaders));
	if (!scaleDownProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %sscaledown_filter.*\n",pathToShaders);

	lastDDITime.addSeconds(-1000000);
	detectedDirectSum = NULL;
	detectedNumTriangles = 0;

	observer = NULL;
	rendererOfScene = rr_gl::RendererOfScene::create(_pathToShaders);
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
	for (unsigned i=0;i<getLights().size();i++)
	{
		RR_ASSERT(getLights()[i]);
		realtimeLights.push_back(new RealtimeLight(*getLights()[i]));
	}

	// reset detected direct lighting
	if (detectedDirectSum) memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
}

void RRDynamicSolverGL::reportDirectIlluminationChange(int lightIndex, bool dirtyShadowmap, bool dirtyGI, bool dirtyRange)
{
	// do core work (call once with lightIndex=-1, it tells core that geometry did change, do not call many times with lightIndex=0,1,2...)
	RRDynamicSolver::reportDirectIlluminationChange(lightIndex,dirtyShadowmap,dirtyGI,dirtyRange);
	// do rr_gl work
	for (unsigned i=0;i<realtimeLights.size();i++)
		if (lightIndex==i || lightIndex==-1) // -1=geometry change, affects all lights
		{
			realtimeLights[i]->dirtyShadowmap |= dirtyShadowmap;
			realtimeLights[i]->dirtyGI |= dirtyGI;
			realtimeLights[i]->dirtyRange |= dirtyRange;
		}
}

void RRDynamicSolverGL::calculate(CalculateParameters* _params)
{
	REPORT(rr::RRReportInterval report(rr::INF3,"calculate GL...\n"));

	CalculateParameters params = _params ? *_params : CalculateParameters();

	// set light dirty flags before updating shadowmaps
	calculateDirtyLights(_params);

	// update only dirty maps
	updateShadowmaps();

	// early exit if quality=0
	// used in "no radiosity" part of Lightsmark
	if (params.qualityIndirectDynamic==0) return;

	// detect only dirty lights
	if (realtimeLights.size())
	{
		bool dirtyGI = false;
		unsigned numLightsEnabled = 0;
		for (unsigned i=0;i<realtimeLights.size();i++)
			if (realtimeLights[i] && realtimeLights[i]->getRRLight().enabled)
			{
				numLightsEnabled++;
				dirtyGI |= realtimeLights[i]->dirtyGI && !realtimeLights[i]->shadowOnly;
			}
		// When user disables light (rrlight->enabled=false;rtlight->dirtyGI=true), enabled lights stay clean,
		// but DDI needs update. So we detect when number of enabled lights changes.
		if (numLightsEnabled!=lastDDINumLightsEnabled)
		{
			lastDDINumLightsEnabled = numLightsEnabled;
			lastDDITime.addSeconds(-1000000);
			dirtyGI = true;
		}
		if (dirtyGI)
		{
			rr::RRTime now;
			if (now.secondsSince(lastDDITime)>=params.secondsBetweenDDI) // limits frequency of DDI
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

	if (getLights().size()!=realtimeLights.size())
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Inconsistency: getLights().size=%d realtimeLights.size()=%d. Don't insert/delete realtimeLights directly, use setLights().\n",getLights().size(),realtimeLights.size()));
	}

	RRDynamicSolver::calculate(_params);
}

void RRDynamicSolverGL::updateShadowmaps()
{
	PreserveClearColor p1;
	PreserveViewport p2;
	PreserveMatrices p3;
	PreserveFBO p4; // must go after viewport, so that viewport is restored later

	// calculate max reasonable level of shadow transparency for materials
	//  using higher level would slow down rendering, without improving quality
	RealtimeLight::ShadowTransparency shadowTransparencyRequestedByMaterials = RealtimeLight::FULLY_OPAQUE_SHADOWS;
	const rr::RRObjects& dynobjects = getDynamicObjects();
	for (int i=-1;i<(int)dynobjects.size();i++)
	{
		rr::RRObject* object = (i<0)?getMultiObjectCustom():dynobjects[i];
		if (object)
		{
			for (unsigned g=0;g<object->faceGroups.size();g++)
			{
				const rr::RRMaterial* material = object->faceGroups[g].material;
				if (material && material->specularTransmittance.color!=rr::RRVec3(0))
				{
					if (!material->specularTransmittanceKeyed)
					{
						if (material->refractionIndex!=1)
							shadowTransparencyRequestedByMaterials = RealtimeLight::FRESNEL_SHADOWS;
						else
						if (shadowTransparencyRequestedByMaterials!=RealtimeLight::FRESNEL_SHADOWS)
							shadowTransparencyRequestedByMaterials = RealtimeLight::RGB_SHADOWS;
						goto done;
					}
					if (shadowTransparencyRequestedByMaterials==RealtimeLight::FULLY_OPAQUE_SHADOWS)
						shadowTransparencyRequestedByMaterials = RealtimeLight::ALPHA_KEYED_SHADOWS;
				}
			}
		}
	}
done:

	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		RealtimeLight* light = realtimeLights[i];
		if (!light)
		{
			RR_ASSERT(0);
			continue;
		}
		if (!light->getRRLight().enabled)
			continue;
		
		// this optimization keeps dirtyShadowmap on in lights without shadows
		if (!light->getRRLight().castShadows)
			continue;

		// adjust shadow transparency
		RealtimeLight::ShadowTransparency shadowTransparencyActual = RR_MIN(shadowTransparencyRequestedByMaterials,light->shadowTransparencyRequested);
		if (light->shadowTransparencyActual!=shadowTransparencyActual)
		{
			light->shadowTransparencyActual = shadowTransparencyActual;
			reportDirectIlluminationChange(i,true,true,false);
		}

		// update shadowmap[s]
		bool isDirtyOnlyBecauseObserverHasMoved = !light->dirtyShadowmap && observer && observer->getPosition()!=light->getObserverPos() && light->getCamera()->isOrthogonal() && light->getNumShadowmaps();
		if (light->dirtyShadowmap || isDirtyOnlyBecauseObserverHasMoved)
		{
			REPORT(rr::RRReportInterval report(rr::INF3,"Updating shadowmap (light %d)...\n",i));
			if (getMultiObjectCustom())
			{
				if (light->dirtyRange)
					light->setRangeDynamically(getMultiObjectCustom()->getCollider(),getMultiObjectCustom());
				light->configureCSM(observer,getMultiObjectCustom());
			}
			glEnable(GL_POLYGON_OFFSET_FILL);
			// Setup shader for rendering to SM.
			// Default constructor sets nearly all off, perfect for shadowmap.
			// MATERIAL_TRANSPARENCY_BLEND is off because it would trigger distance sorting which we don't need,
			//  not even for rgb shadows (they multiply each other, order doesn't matter).
			// MATERIAL_CULLING is off because rendering both sides reduces shadow bias problem. And 1-sided faces are often expected to cast shadow in both directions.
			//  Even if we put it on, honourOfflineFlags in RendererOfMesh will turn it off for virtually all materials.
			UberProgramSetup uberProgramSetup;
			switch(light->shadowTransparencyActual)
			{
				case RealtimeLight::FRESNEL_SHADOWS:
					// + waves from normalmapped water slightly visible in shadows (but still far prom proper caustic, in both intensity and shape)
					// + shadow of glass sphere slightly darker around border (but still far prom proper caustic, in both intensity and shape)
					// - reduced fps
					uberProgramSetup.comment = "// fresnel shadowmap pass\n";
					uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_FRESNEL = true;
					uberProgramSetup.MATERIAL_BUMP_MAP = true; // adds details to Fresnel
					break;
				case RealtimeLight::RGB_SHADOWS:
					uberProgramSetup.comment = "// RGB shadowmap pass\n";
					uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = true;
					break;
				case RealtimeLight::ALPHA_KEYED_SHADOWS:
					uberProgramSetup.comment = "// alpha keyed shadowmap pass\n";
					uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = true;
					uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
					uberProgramSetup.MATERIAL_DIFFUSE = true;
					uberProgramSetup.LIGHT_INDIRECT_CONST = 1; // without light, diffuse texture would be optimized away
					break;
				case RealtimeLight::FULLY_OPAQUE_SHADOWS:
					uberProgramSetup.comment = "// opaque shadowmap pass\n";
					break;
				default:
					RR_ASSERT(0);
			}
			for (unsigned i=isDirtyOnlyBecauseObserverHasMoved?1:0 // don't update instance 0 just because of observer movement
			     ;i<light->getNumShadowmaps();i++)
			{
				rr::RRCamera lightInstance;
				Texture* shadowmap = light->getShadowmap(i);
				if (!shadowmap)
				{
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (shadow=NULL).\n"));
				}
				else
				{
					// these settings hide all cases of shadow acne at cost of large shadow bias
					// decrease to reduce bias, while allowing occassional acne
					float slopeBias = (light->getNumShadowSamples(i)==1)?2.6f:8.f;
					float fixedBias = slopeBias*500;
					Workaround::needsIncreasedBias(slopeBias,fixedBias,light->getRRLight());
					glPolygonOffset(slopeBias,fixedBias);
					glViewport(0, 0, light->getRRLight().rtShadowmapSize, light->getRRLight().rtShadowmapSize);
					FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,shadowmap);
					if (light->shadowTransparencyActual==RealtimeLight::RGB_SHADOWS || light->shadowTransparencyActual==RealtimeLight::FRESNEL_SHADOWS)
					{
						Texture* colormap = light->getShadowmap(i,true);
						if (!colormap)
						{
							RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (color=NULL).\n"));
						}
						else
						{
							FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,colormap);
						}
					}
					if (!FBO::isOk())
					{
						// 8800GTS returns this in some near out of memory situations, perhaps with texture that already failed to initialize
						RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (FBO).\n"));
					}
					else
					{
						if (light->shadowTransparencyActual==RealtimeLight::RGB_SHADOWS || light->shadowTransparencyActual==RealtimeLight::FRESNEL_SHADOWS)
						{
							// we assert that color mask is 1, clear would be masked otherwise
							glClearColor(1,1,1,1);
							glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
						}
						else
						{
							glClear(GL_DEPTH_BUFFER_BIT);
						}
						bool depthClamp = light->getRRLight().type==rr::RRLight::DIRECTIONAL && i && Workaround::supportsDepthClamp();
						if (depthClamp) glEnable(GL_DEPTH_CLAMP);
						renderScene(uberProgramSetup,light->getShadowmapCamera(i,lightInstance),&light->getRRLight(),false,UINT_MAX,UINT_MAX,UINT_MAX,NULL,false,NULL,1);
						if (depthClamp) glDisable(GL_DEPTH_CLAMP);
					}
				}
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			light->dirtyShadowmap = false; // clear it now when all is done (if cleared sooner, getShadowmap() during update might set it)
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
		if (!light->getRRLight().enabled)
			continue;
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
				if (observer) light->positionOfLastDDI = observer->getPosition();
				updatedSmallMaps += detectDirectIlluminationTo(light,light->smallMapCPU,light->numTriangles);
			}
		}
	}

	// find the only enabled light
	RealtimeLight* theOnlyEnabledLight = NULL;
	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		if (realtimeLights[i] && realtimeLights[i]->getRRLight().enabled)
			if (!theOnlyEnabledLight)
			{
				// first enabled light found
				theOnlyEnabledLight = realtimeLights[i];
			}
			else
			{
				// second enabled light found
				// sum all enabled smallMapsCPU into detectedDirectSum
				if (updatedSmallMaps && realtimeLights.size()>1)
				{
					unsigned numLights = realtimeLights.size();
					#pragma omp parallel for
					for (int b=0;b<(int)numTriangles*4;b++)
					{
						unsigned sum = 0;
						for (unsigned l=0;l<numLights;l++)
							if (realtimeLights[l] && realtimeLights[l]->getRRLight().enabled)
								sum += ((unsigned char*)realtimeLights[l]->smallMapCPU)[b];
						((unsigned char*)detectedDirectSum)[b] = RR_CLAMPED(sum,0,255);
					}
				}
				// return sum
				return detectedDirectSum;
			}
	}
	if (theOnlyEnabledLight)
	{
		// return data for the only light, no summing
		return theOnlyEnabledLight->smallMapCPU;
	}
	// return NULL for no lights
	return NULL;
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
	PreserveFBO p6; // must go after viewport, so that viewport is restored later
	PreserveFlag p7(GL_FRAMEBUFFER_SRGB,false); // we need GL_FRAMEBUFFER_SRGB disabled

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
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,detectBigMap);

		// clear
		glViewport(0, 0, pixelWidth,pixelHeightInOnePass);
		glClear(GL_COLOR_BUFFER_BIT); // old pixels are overwritten, so clear is usually not needed, but individual light-triangle lighting may be disabled by getTriangleMaterial()=triangles are not rendered, and we need to detect 0 rather than uninitialized value

		// setup shader
		UberProgramSetup uberProgramSetup;
		uberProgramSetup.comment = "// DDI pass\n";
		uberProgramSetup.SHADOW_MAPS = (ddiLight->getRRLight().type==rr::RRLight::POINT)?ddiLight->getNumShadowmaps():(ddiLight->getNumShadowmaps()?1:0);
		uberProgramSetup.SHADOW_SAMPLES = uberProgramSetup.SHADOW_MAPS?1:0; // for 1-light render, won't be reset by MultiPass
		uberProgramSetup.SHADOW_COLOR = uberProgramSetup.SHADOW_MAPS && (ddiLight->shadowTransparencyActual==RealtimeLight::RGB_SHADOWS || ddiLight->shadowTransparencyActual==RealtimeLight::FRESNEL_SHADOWS);
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_COLOR = ddiLight->getRRLight().color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = uberProgramSetup.SHADOW_MAPS && ddiLight->getProjectedTexture();
		uberProgramSetup.LIGHT_DIRECTIONAL = ddiLight->getCamera()->isOrthogonal();
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = ddiLight->getRRLight().type==rr::RRLight::SPOT && !ddiLight->getProjectedTexture();
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_CULLING = false;
		uberProgramSetup.FORCE_2D_POSITION = true;
		Program* program = uberProgramSetup.useProgram(uberProgram1,NULL,ddiLight,0,NULL,1,NULL);
		if (!program)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"setupShader: Failed to compile or link GLSL program.\n"));
		}

		// render scene
		glDisable(GL_CULL_FACE);
		FaceGroupRange fgRange(0,0,0,firstCapturedTriangle,lastCapturedTrianglePlus1);
		rendererOfScene->getRendererOfMesh(getMultiObjectCustom()->getCollider()->getMesh())->renderMesh(
			program,
			getMultiObjectCustom(),
			&fgRange,
			1,
			uberProgramSetup,
			false,
			NULL,
			NULL);


		// downscale 10pixel triangles in 4x4 squares to single pixel values
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,detectSmallMap);
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
		REPORT(rr::RRReportInterval report(rr::INF3,"glReadPix %dx%d\n", triCountX, triCountYInThisPass));
		RR_ASSERT(_space+2047>=firstCapturedTriangle+triCountX*triCountYInThisPass);
		glReadPixels(0, 0, triCountX, triCountYInThisPass, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _results+firstCapturedTriangle);
	}

	return 1;
}

static bool invertMatrix4x4(const double m[16], double inverse[16])
{
	double inv[16] =
	{
		m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10],
		-m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10],
		m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6],
		-m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6],
		-m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10],
		m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10],
		-m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6],
		m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6],
		m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9],
		-m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9],
		m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5],
		-m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5],
		-m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9],
		m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9],
		-m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5],
		m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5]
	};
	double det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
	if (!det)
		return false;
	det = 1/det;
	for (unsigned i=0;i<16;i++)
		inverse[i] = inv[i] * det;
	return true;
}

static void drawCamera(rr::RRCamera* camera)
{
	if (camera)
	{
		double inverseViewMatrix[16];
		double inverseProjectionMatrix[16];
		invertMatrix4x4(camera->getViewMatrix(), inverseViewMatrix);
		invertMatrix4x4(camera->getProjectionMatrix(), inverseProjectionMatrix);
		glPushMatrix();
		glMultMatrixd(inverseViewMatrix);
		glMultMatrixd(inverseProjectionMatrix);
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
				rr::RRCamera camera;
				drawCamera(&light->getShadowmapCamera(i,camera));
			}
		}
		else
		{
			// light without shadows has parent rendered
			drawCamera(light->getCamera());
		}
	}
}

void RRDynamicSolverGL::renderScene(
		const UberProgramSetup& _uberProgramSetup,
		const rr::RRCamera& _camera,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLayers,
		unsigned _layerLightmap,
		unsigned _layerEnvironment,
		unsigned _layerLDM,
		const ClipPlanes* _clipPlanes,
		bool _srgbCorrect,
		const rr::RRVec4* _brightness,
		float _gamma)
{
	rendererOfScene->render(
		this,
		_uberProgramSetup,
		_camera,
		_uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL,
		_renderingFromThisLight,
		_updateLayers,
		_layerLightmap,
		_layerEnvironment,
		_layerLDM,
		_clipPlanes,
		_srgbCorrect,
		_brightness,
		_gamma);
}

void RRDynamicSolverGL::renderLights(const rr::RRCamera& _camera)
{
	setupForRender(_camera);
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
	uberProgramSetup.MATERIAL_DIFFUSE = 1;
	Program* program = uberProgramSetup.useProgram(uberProgram1,NULL,NULL,0,NULL,1,NULL);
	for (unsigned i=0;i<getLights().size();i++)
	{
		drawRealtimeLight(realtimeLights[i]);
	}
}

}; // namespace
