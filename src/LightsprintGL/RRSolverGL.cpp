// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// OpenGL implementation of RRSolver.
// --------------------------------------------------------------------------

#include <cstdarg>
#include <cstdio>
#include <vector>
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/PreserveState.h"
#include "Lightsprint/GL/PluginCube.h"
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PluginSky.h"
#include "RendererOfMesh.h" // DDI_TRIANGLES_X/Y
#include "Shader.h" // s_es
#include "Workaround.h"

#define REPORT(a) //a

#define CENTER_GRANULARITY 0.01f // if envmap center moves less than granularity, it is considered unchanged. prevents updates when dynamic object rotates (=position slightly fluctuates)

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RRSolverGL

RRSolverGL::RRSolverGL(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps, DDIQuality _detectionQuality)
{
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
	detectBigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X*faceSizeX,DDI_TRIANGLES_MAX_Y*faceSizeY,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	detectSmallMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,DDI_TRIANGLES_X,DDI_TRIANGLES_MAX_Y,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);

	scaleDownProgram = Program::create(
		rr::RRString(0,L"#define SIZEX %d\n#define SIZEY %d\n",faceSizeX,faceSizeY).c_str(),
		rr::RRString(0,L"%lsscaledown_filter.vs",pathToShaders.w_str()),
		rr::RRString(0,L"%lsscaledown_filter.fs",pathToShaders.w_str()));
	if (!scaleDownProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %lsscaledown_filter.*\n",pathToShaders.w_str());

	lastDDITime.addSeconds(-1000000);
	detectedDirectSum = nullptr;
	detectedNumTriangles = 0;

	observer = nullptr;
	renderer = rr_gl::Renderer::create(pathToShaders, pathToMaps);
	uberProgram1 = UberProgram::create(rr::RRString(0,L"%lsubershader.vs",pathToShaders.w_str()),rr::RRString(0,L"%lsubershader.fs",pathToShaders.w_str()));

	updatingEnvironmentMap = 0;
	depthTexture = Texture::createShadowmap(1,1,false);
}

RRSolverGL::~RRSolverGL()
{
	delete depthTexture;
	for (unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
	delete renderer;
	delete uberProgram1;
	delete[] detectedDirectSum;
	delete scaleDownProgram;
	if (detectBigMap) delete detectBigMap->getBuffer();
	delete detectBigMap;
	if (detectSmallMap) delete detectSmallMap->getBuffer();
	delete detectSmallMap;
}

void RRSolverGL::setLights(const rr::RRLights& _lights)
{
	// create realtime lights
	RRSolver::setLights(_lights);
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

void RRSolverGL::reportDirectIlluminationChange(int lightIndex, bool dirtyShadowmap, bool dirtyGI, bool dirtyRange)
{
	// do core work (call once with lightIndex=-1, it tells core that geometry did change, do not call many times with lightIndex=0,1,2...)
	RRSolver::reportDirectIlluminationChange(lightIndex,dirtyShadowmap,dirtyGI,dirtyRange);
	// do rr_gl work
	for (unsigned i=0;i<realtimeLights.size();i++)
		if (lightIndex==i || lightIndex==-1) // -1=geometry change, affects all lights
		{
			realtimeLights[i]->dirtyShadowmap |= dirtyShadowmap;
			realtimeLights[i]->dirtyGI |= dirtyGI;
			realtimeLights[i]->dirtyRange |= dirtyRange;
		}
}

void RRSolverGL::calculate(const CalculateParameters* _params)
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
		setDirectIllumination(nullptr);
	}

	if (getLights().size()!=realtimeLights.size())
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Inconsistency: getLights().size=%d realtimeLights.size()=%d. Don't insert/delete realtimeLights directly, use setLights().\n",getLights().size(),realtimeLights.size()));
	}

	RRSolver::calculate(_params);
}

void RRSolverGL::updateShadowmaps()
{
	PreserveClearColor p1;
	PreserveViewport p2;
	PreserveFBO p4; // must go after viewport, so that viewport is restored later

	// calculate max reasonable level of shadow transparency for materials
	//  using higher level would slow down rendering, without improving quality
	RealtimeLight::ShadowTransparency shadowTransparencyRequestedByMaterials = RealtimeLight::FULLY_OPAQUE_SHADOWS;
	const rr::RRObjects& dynobjects = getDynamicObjects();
	for (int i=-1;i<(int)dynobjects.size();i++)
	{
		rr::RRObject* object = (i<0)?getMultiObject():dynobjects[i];
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
			if (light->dirtyRange && getMultiObject())
				light->setRangeDynamically(getCollider(),getMultiObject()); // getMultiObject()->getCollider() would makes it test static objects only
			light->configureCSM(observer,this);
			glEnable(GL_POLYGON_OFFSET_FILL);
			// Setup shader for rendering to SM.
			// Default constructor sets nearly all off, perfect for shadowmap.
			// MATERIAL_TRANSPARENCY_BLEND is off because it would trigger distance sorting which we don't need,
			//  not even for rgb shadows (they multiply each other, order doesn't matter).
			// [#45] MATERIAL_CULLING is off because rendering both sides reduces shadow bias problem. And 1-sided faces are often expected to cast shadow in both directions.
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
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (shadow=nullptr).\n"));
				}
				else
				{
					// these settings hide all cases of shadow acne at cost of large shadow bias
					// decrease to reduce bias, while allowing occasional acne
					float slopeBias = (light->getNumShadowSamples(i)==1)?2.6f:8.f;
					float fixedBias = slopeBias*500;
					Workaround::needsIncreasedBias(slopeBias,fixedBias,light->getRRLight());
					slopeBias *= light->getRRLight().rtShadowmapBias.x;
					fixedBias *= light->getRRLight().rtShadowmapBias.y;
					glPolygonOffset(slopeBias,fixedBias);
					glViewport(0, 0, light->getRRLight().rtShadowmapSize, light->getRRLight().rtShadowmapSize);
					FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowmap,p4.state);
					if (light->shadowTransparencyActual==RealtimeLight::RGB_SHADOWS || light->shadowTransparencyActual==RealtimeLight::FRESNEL_SHADOWS)
					{
						Texture* colormap = light->getShadowmap(i,true);
						if (!colormap)
						{
							RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Shadowmap update failed (color=nullptr).\n"));
						}
						else
						{
							FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colormap,p4.state);
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
							glClearColor(1,1,1,1); // [#49] unshadowed areas have a=1
							glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
						}
						else
						{
							glClear(GL_DEPTH_BUFFER_BIT);
						}
#ifndef RR_GL_ES2
						bool depthClamp = light->getRRLight().type==rr::RRLight::DIRECTIONAL && i && Workaround::supportsDepthClamp();
						if (depthClamp) glEnable(GL_DEPTH_CLAMP);
#endif
						PluginParamsScene ppScene(nullptr,this);
						ppScene.lights = nullptr;
						ppScene.uberProgramSetup = uberProgramSetup;
						ppScene.renderingFromThisLight = &light->getRRLight();
						PluginParamsShared ppShared;
						ppShared.camera = &light->getShadowmapCamera(i,lightInstance);
						ppShared.viewport[2] = light->getRRLight().rtShadowmapSize;
						ppShared.viewport[3] = light->getRRLight().rtShadowmapSize;
						renderer->render(&ppScene,ppShared);
#ifndef RR_GL_ES2
						if (depthClamp) glDisable(GL_DEPTH_CLAMP);
#endif
					}
				}
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			light->dirtyShadowmap = false; // clear it now when all is done (if cleared sooner, getShadowmap() during update might set it)
		}
	}
}

const unsigned* RRSolverGL::detectDirectIllumination()
{
	if (!getMultiObject()) return nullptr;
	unsigned numTriangles = getMultiObject()->getCollider()->getMesh()->getNumTriangles();
	if (!numTriangles) return nullptr;

	PreserveViewport p1;

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
	RealtimeLight* theOnlyEnabledLight = nullptr;
	for (unsigned i=0;i<realtimeLights.size();i++)
	{
		if (realtimeLights[i] && realtimeLights[i]->getRRLight().enabled)
		{
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
					#pragma omp parallel for schedule(static) if(numTriangles>RR_OMP_MIN_ELEMENTS)
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
	}
	if (theOnlyEnabledLight)
	{
		// return data for the only light, no summing
		return theOnlyEnabledLight->smallMapCPU;
	}
	// return nullptr for no lights
	return nullptr;
}

unsigned RRSolverGL::detectDirectIlluminationTo(RealtimeLight* ddiLight, unsigned* _results, unsigned _space)
{
	if (!scaleDownProgram || !_results)
	{
		RR_ASSERT(0);
		return 0;
	}

	REPORT(rr::RRReportInterval report(rr::INF1,"detectDirectIllumination()\n"));

	if (!getMultiObject())
		return 0;

	const rr::RRMesh* mesh = getMultiObject()->getCollider()->getMesh();
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
#ifndef RR_GL_ES2
	PreserveFlag p7(GL_FRAMEBUFFER_SRGB,false); // we need GL_FRAMEBUFFER_SRGB disabled
#endif

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
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,detectBigMap,p6.state);

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
		uberProgramSetup.LIGHT_DIRECT_ATT_REALISTIC = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::REALISTIC;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = ddiLight->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_CULLING = false;
		uberProgramSetup.FORCE_2D_POSITION = true;
		Program* program = uberProgramSetup.useProgram(uberProgram1,nullptr,ddiLight,0,1,nullptr,1,nullptr);
		if (!program)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"setupShader: Failed to compile or link GLSL program.\n"));
		}

		// render scene
		glDisable(GL_CULL_FACE);
		FaceGroupRange fgRange(0,0,0,firstCapturedTriangle,lastCapturedTrianglePlus1);
		renderer->getMeshRenderer(getMultiObject()->getCollider()->getMesh())->renderMesh(
			program,
			getMultiObject(),
			&fgRange,
			1,
			uberProgramSetup,
			false,
			nullptr,
			nullptr,
			1,
			0);


		// downscale 10pixel triangles in 4x4 squares to single pixel values
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,detectSmallMap,p6.state);
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/detectBigMap->getBuffer()->getWidth(),1.0f/detectBigMap->getBuffer()->getHeight());
		glViewport(0,0,DDI_TRIANGLES_X,DDI_TRIANGLES_MAX_Y);
		glActiveTexture(GL_TEXTURE0);
		detectBigMap->bindTexture();
		glDisable(GL_CULL_FACE);
		float fractionOfBigMapUsed = 1.0f*triCountYInOnePass/DDI_TRIANGLES_MAX_Y; // we downscale only part of big map that is used
		float position[] = {0,0, 0,fractionOfBigMapUsed, 1,fractionOfBigMapUsed, 1,0};
		glEnableVertexAttribArray(VAA_POSITION);
		glVertexAttribPointer(VAA_POSITION, 2, GL_FLOAT, 0, 0, position);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glDisableVertexAttribArray(VAA_POSITION);

		// read downscaled image to memory
		REPORT(rr::RRReportInterval report(rr::INF3,"glReadPix %dx%d\n", triCountX, triCountYInThisPass));
		RR_ASSERT(_space+2047>=firstCapturedTriangle+triCountX*triCountYInThisPass);
		glReadPixels(0, 0, triCountX, triCountYInThisPass, GL_RGBA, GL_UNSIGNED_BYTE, _results+firstCapturedTriangle); // GL_RGBA+GL_UNSIGNED_BYTE is the only fixed combination supported by OpenGL ES 2.0
	}

	return 1;
}

#ifdef RR_GL_ES2

void RRSolverGL::renderLights(const rr::RRCamera& _camera)
{
	RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"renderLights() not implemented in OpenGL ES.\n"));
}

#else //!RR_GL_ES2

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

/*void RRSolverGL::renderScene(const RenderParameters& _renderParameters)
{
	renderer->render(this, nullptr, _renderParameters.uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : nullptr, _renderParameters);
}*/

void RRSolverGL::renderLights(const rr::RRCamera& _camera)
{
	if (s_es)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"renderLights() not implemented in OpenGL ES.\n"));
		return;
	}
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
	uberProgramSetup.MATERIAL_DIFFUSE = 1;
	uberProgramSetup.LEGACY_GL = 1;
	Program* program = uberProgramSetup.useProgram(uberProgram1,nullptr,nullptr,0,1,nullptr,1,nullptr);
	uberProgramSetup.useCamera(program,&_camera);
	for (unsigned i=0;i<getLights().size();i++)
	{
		drawRealtimeLight(realtimeLights[i]);
	}
}

#endif //!RR_GL_ES2

// gcc 4.2 can't instantiate std::vector if this struct is declared locally
struct ObjectInfo
{
	rr::RRObject* object;
	rr::RRObject::FaceGroups faceGroups;
};

unsigned RRSolverGL::updateEnvironmentMap(rr::RRObjectIllumination* illumination, unsigned layerEnvironment, unsigned layerLightmap, unsigned layerAmbientMap)
{
	if (!illumination)
	{
		RR_ASSERT(0);
		return 0;
	}
	rr::RRBuffer* cube = illumination->getLayer(layerEnvironment);
	if (!cube)
	{
		return 0;
	}
	// we used to automatically switch to RRSolver's implementation,
	//  but it fills cube with black if internal solver does not exist, some users prefer our implementation that works always
	//if (cube->getWidth()<ENVMAP_RES_RASTERIZED)
	//{
	//	return this->RRSolver::updateEnvironmentMap(illumination,layerEnvironment,layerLightmap,layerAmbientMap);
	//}
	//else
	{
		// check version numbers, is cube out of date?
		unsigned solutionVersion = getSolutionVersion();
		bool centerMoved = (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()>CENTER_GRANULARITY;
		if (!centerMoved && (cube->version>>16)==(solutionVersion&65535))
		{
			return 0;
		}

		updatingEnvironmentMap++;
		if (updatingEnvironmentMap>1)
			rr::RRReporter::report(rr::WARN,"updateEnvironmentMap() called from updateEnvironmentMap(), not cool.\n");
	
		// find out scene size
		rr::RRReal size = 1;
		if (getMultiObject())
		{
			rr::RRVec3 mini,maxi;
			getMultiObject()->getCollider()->getMesh()->getAABB(&mini,&maxi,nullptr);
			size = (maxi-mini).length();
		}

		// hide objects with current illumination
		// (we hide it only in 1objects, it stays visible in multiobject. rendering with all lights and materials usually uses 1objects.
		//  if it uses multiobject, object reflects itself)
		std::vector<ObjectInfo> infos;
		for (unsigned i=0;i<2;i++)
		{
			const rr::RRObjects& objects = i ? getDynamicObjects() : getStaticObjects();
			for (unsigned j=0;j<objects.size();j++)
			{
				rr::RRObject* object = objects[j];
				if (&object->illumination==illumination)
				{
					ObjectInfo info;
					info.object = object;
#ifdef RR_SUPPORTS_RVALUE_REFERENCES
					info.faceGroups = std::move(object->faceGroups);
#else
					info.faceGroups = object->faceGroups;
					object->faceGroups.clear();
#endif
					infos.push_back(info);
				}
			}
		}

		// update cube
		PluginParamsSky ppSky(nullptr,this,1);
		PluginParamsScene ppScene(&ppSky,this);
		ppScene.uberProgramSetup.enableAllMaterials();
		ppScene.uberProgramSetup.enableAllLights();
		ppScene.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false; // no mirrors, prevents render() from calling another render()
		ppScene.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
		ppScene.updateLayerEnvironment = false; // no environment updates,  prevents render() from calling another updateEnvironmentMap(), we are not reentrant because of depthTexture
		ppScene.layerEnvironment = layerEnvironment; // use the same envmaps we are rendering into (object being updated is hidden, so there should be no conflict from using texture we render into)
		if (layerAmbientMap!=UINT_MAX)
		{
			// realtime lights + ambient maps
			ppScene.layerLightmap = layerAmbientMap;
		}
		else
		if (layerLightmap!=UINT_MAX)
		{
			// lightmaps
			ppScene.layerLightmap = layerLightmap;
			ppScene.lightmapsContainAlsoDirectIllumination = true;
		}
		else
		{
			// realtime lights + constant ambient
		}
		bool srgbCorrect = !true; // we don't know whether final render is srgb correct or not, let's request more realistic version (but renderToCube might ignore us)
			// something is off here, cube has wrong intensities, srgb incorrect one is bit better so it is used for now
			// (srgb is probably more wrong here because we receive linear BT_RGB cubes to update, they should be scaled for srgb)
		Texture* cubeTexture = getTexture(cube,false,false);
		cubeTexture->reset(false,false,srgbCorrect);
		PluginParamsCube ppCube(&ppScene,cubeTexture,depthTexture,360);
		rr::RRCamera camera;
		camera.setPosition(illumination->envMapWorldCenter);
		camera.setRange(illumination->envMapWorldRadius,size);
		PluginParamsShared ppShared;
		ppShared.camera = &camera;
		ppShared.viewport[2] = cube->getWidth();
		ppShared.viewport[3] = cube->getHeight();
		ppShared.srgbCorrect = srgbCorrect;
		ppShared.gamma = srgbCorrect?0.45f:1.f;
		renderer->render(&ppCube,ppShared);
		cubeTexture->bindTexture();
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // without glTexParameteri all env reflections would have max shininess
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP); // exists in GL2+ARB_framebuffer_object, GL3, ES2

		// unhide objects with current illumination
		for (unsigned i=0;i<infos.size();i++)
#ifdef RR_SUPPORTS_RVALUE_REFERENCES
			infos[i].object->faceGroups = std::move(infos[i].faceGroups);
#else
			infos[i].object->faceGroups = infos[i].faceGroups;
#endif

		// copy texture to buffer (only necessary before save, can be skipped otherwise)
		cubeTexture->copyTextureToBuffer();
		// encode solutionVersion into cube version
		cube->version = (solutionVersion<<16)+(cube->version&65535);
		cubeTexture->version = cube->version;

		illumination->cachedNumTriangles = getMultiObject() ? getMultiObject()->getCollider()->getMesh()->getNumTriangles() : 0;
		illumination->cachedCenter = illumination->envMapWorldCenter;

		updatingEnvironmentMap--;
		return 1;
	}
}

}; // namespace
