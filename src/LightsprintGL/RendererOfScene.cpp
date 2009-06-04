// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <algorithm> // sort
#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "ObjectBuffers.h" // USE_VBO
#include "Lightsprint/GL/MultiPass.h"
#include "tmpstr.h"


namespace rr_gl
{

static bool g_usingOptimizedScene = false;

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRDynamicSolver

//! OpenGL renderer of RRDynamicSolver internal scene.
//
//! Renders contents of solver, geometry and illumination.
//! Geometry may be slightly different from original scene, because of optional internal optimizations.
//! Illumination is taken directly from solver,
//! renderer doesn't use or modify precomputed illumination in layers.
class RendererOfRRDynamicSolver : public Renderer
{
public:
	RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver, const char* pathToShaders);

	//! Sets parameters of render related to shader and direct illumination.
	//
	//! \param uberProgramSetup
	//!  Set of features for rendering.
	//! \param lights
	//!  Set of lights, source of direct illumination in rendered scene.
	//! \param renderingFromThisLight
	//!  When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	void setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(const rr::RRVec4* brightness, float gamma);

	//! Specifies light detail map. Default = none.
	void setLDM(unsigned layerNumberLDM)
	{
		params.layerNumberLDM = layerNumberLDM;
	}

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	//! Does not clear before rendering.
	virtual void render();

	virtual ~RendererOfRRDynamicSolver();

protected:
	struct Params
	{
		rr::RRDynamicSolver* solver;
		UberProgramSetup uberProgramSetup;
		const RealtimeLights* lights;
		const rr::RRLight* renderingFromThisLight;
		rr::RRVec4 brightness;
		float gamma;
		unsigned layerNumberLDM;
		Params();
	};
	Params params;
	TextureRenderer* textureRenderer;
	UberProgram* uberProgram;
	void initSpecularReflection(Program* program); // bind TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR texture, set uniform worldEyePos
private:
	// 1 renderer for 1 scene
	Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
	// 1 specular refl map for all static faces
	const rr::RRBuffer* lastRenderSolverEnv; // solver environment
	rr::RRBuffer* lastRenderSpecularEnv; // downscaled for specular reflections
};


RendererOfRRDynamicSolver::Params::Params()
{
	solver = NULL;
	lights = NULL;
	renderingFromThisLight = NULL;
	brightness = rr::RRVec4(1);
	gamma = 1;
	layerNumberLDM = UINT_MAX; // UINT_MAX is usually empty, so it's like disabled ldm
}

RendererOfRRDynamicSolver::RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver, const char* pathToShaders)
{
	params.solver = solver;
	textureRenderer = new TextureRenderer(pathToShaders);
	uberProgram = UberProgram::create(
		tmpstr("%subershader.vs",pathToShaders),
		tmpstr("%subershader.fs",pathToShaders));
	params.brightness = rr::RRVec4(1);
	params.gamma = 1;
	rendererNonCaching = NULL;
	rendererCaching = NULL;
	lastRenderSolverEnv = NULL;
	lastRenderSpecularEnv = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGBA,true,NULL);
}

RendererOfRRDynamicSolver::~RendererOfRRDynamicSolver()
{
	delete lastRenderSpecularEnv;
	delete rendererCaching;
	delete rendererNonCaching;
	delete uberProgram;
	delete textureRenderer;
}

void RendererOfRRDynamicSolver::setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight)
{
	params.uberProgramSetup = uberProgramSetup;
	params.lights = lights;
	params.renderingFromThisLight = renderingFromThisLight;
}

const void* RendererOfRRDynamicSolver::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RendererOfRRDynamicSolver::setBrightnessGamma(const rr::RRVec4* brightness, float gamma)
{
	params.brightness = brightness ? *brightness : rr::RRVec4(1);
	params.gamma = gamma;
}

void RendererOfRRDynamicSolver::initSpecularReflection(Program* program)
{
	// set environment as specular reflection
	const rr::RRBuffer* env = params.solver->getEnvironment();
	if (env)
	{
		// reflect 16x16x6 reflection map
		if (env!=lastRenderSolverEnv)
		{
			lastRenderSolverEnv = env;
			// create small specular reflection map using temporary solver with environment only
			rr::RRDynamicSolver tmpSolver;
			tmpSolver.setEnvironment(env);
			rr::RRObjectIllumination tmpObjIllum(0);
			tmpObjIllum.specularEnvMap = lastRenderSpecularEnv;
			tmpSolver.updateEnvironmentMap(&tmpObjIllum);
			tmpObjIllum.specularEnvMap = NULL;
		}
		glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
		getTexture(lastRenderSpecularEnv,false,false)->bindTexture();

		// reflect original environment (might be too sharp)
		//glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
		//if (env->getWidth()>2)
		//	getTexture(env,true,false)->bindTexture(); // smooth
		//else
		//	getTexture(env,false,false,GL_NEAREST,GL_NEAREST)->bindTexture(); // used by 2x2 sky
	}
}

void RendererOfRRDynamicSolver::render()
{
	rr::RRReportInterval report(rr::INF3,"Rendering optimized scene...\n");
	g_usingOptimizedScene = true;
	if (!params.solver)
	{
		RR_ASSERT(0);
		return;
	}
	if (params.uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		rr::RRReporter::report(rr::WARN,"LIGHT_INDIRECT_MAP incompatible with useOptimizedScene().\n");
		return;
	}

	// render skybox
	if (!params.renderingFromThisLight && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env = params.solver->getEnvironment();
		if (textureRenderer && env)
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(params.brightness,true,!env->getScaled(),params.gamma); // depth test allowed so that skybox does not overwrite water plane
			// no mipmaps because gluBuild2DMipmaps works incorrectly for values >1 (integer part is removed)
			// no compression because artifacts are usually clearly visible
			if (env->getWidth()>2)
				getTexture(env,false,false)->bindTexture(); // smooth
			else
				getTexture(env,false,false,GL_NEAREST,GL_NEAREST)->bindTexture(); // used by 2x2 sky
			glBegin(GL_POLYGON);
				glVertex3f(-1,-1,1);
				glVertex3f(1,-1,1);
				glVertex3f(1,1,1);
				glVertex3f(-1,1,1);
			glEnd();
			textureRenderer->renderEnvironmentEnd();
		}
	}

	if (!params.solver->getMultiObjectCustom())
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Rendering empty static scene.\n"));
		return;
	}

	// create helper renderers
	if (!rendererNonCaching)
	{
		rendererNonCaching = RendererOfRRObject::create(params.solver->getMultiObjectCustom(),params.solver,params.solver->getScaler(),true);
	}
	if (!rendererNonCaching)
	{
		// probably empty scene
		return;
	}
#ifdef USE_VBO
	// if we already USE_VBO, wrapping it in display list would
	// + speed up Nvidia cards by ~2%
	// - AMD cards crash in driver (with 9.3, final driver for Radeons up to X2100)
	// better don't create display list
#else
	if (!rendererCaching && rendererNonCaching)
		rendererCaching = rendererNonCaching->createDisplayList();
#endif

	if (params.uberProgramSetup.LIGHT_INDIRECT_auto)
	{
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = false;
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = true;
		params.uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		params.uberProgramSetup.LIGHT_INDIRECT_MAP2 = false;
		params.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = params.solver->getStaticObjects().size()==1 && params.solver->getIllumination(0)->getLayer(params.layerNumberLDM);
		if (params.layerNumberLDM!=UINT_MAX && params.solver->getStaticObjects().size()>1) 
			LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"LDM not rendered, call useOriginalScene() rather than useOptimizedScene(). Original works only for scenes with 1 object.\n"));
		params.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
		params.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
	}

	PreserveBlend p1;
	PreserveBlendFunc p2;
	PreserveAlphaTest p3;
	PreserveAlphaFunc p4;
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for now, all materials with alpha use this blendmode
	glAlphaFunc(GL_GREATER,0.5f); // for now, all materials with alpha use this alpha test in alpha-keying
	MultiPass multiPass(params.lights,params.uberProgramSetup,uberProgram,&params.brightness,params.gamma);
	UberProgramSetup uberProgramSetup;
	RendererOfRRObject::RenderedChannels renderedChannels;
	RealtimeLight* light;
	Program* program;
	while (program=multiPass.getNextPass(uberProgramSetup,renderedChannels,light))
	{
		rendererNonCaching->setProgram(program);
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setIndirectIlluminationFromSolver(params.solver->getSolutionVersion());
		rendererNonCaching->setLDM(params.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? params.solver->getIllumination(0)->getLayer(params.layerNumberLDM) : NULL);
		rendererNonCaching->setLightingShadowingFlags(params.renderingFromThisLight,light?&light->getRRLight():NULL);

		if (uberProgramSetup.MATERIAL_SPECULAR)
			initSpecularReflection(program);

		// don't cache indirect illumination in vertices, it changes often
		if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR || !rendererCaching)
			rendererNonCaching->render();
		else
			// cache everything else, it's constant
			rendererCaching->render();
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfOriginalScene

//! OpenGL renderer of scene you entered into RRDynamicSolver.
//
//! Renders original scene and illumination from layers.
//! Geometry is exactly what you entered into the solver.
//! Illumination is taken from given layer, not from solver.
//! Solver is not const, because its vertex/pixel buffers may change(create/update) at render time.
class RendererOfOriginalScene : public RendererOfRRDynamicSolver
{
public:
	RendererOfOriginalScene(rr::RRDynamicSolver* solver, const char* pathToShaders);

	//! Sets source of indirect illumination. It is layer 0 by default.
	//! \param layerNumber
	//!  Indirect illumination will be taken from given layer.
	void setIndirectIlluminationSource(unsigned layerNumber, unsigned lightIndirectVersion);
	void setIndirectIlluminationSourceBlend(unsigned layerNumber1, unsigned layerNumber2, float transition, unsigned layerNumberFallback, unsigned lightIndirectVersion);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	//! Does not clear before rendering.
	virtual void render();
	void renderLines(bool shortened, int onlyIndex);

	//! Uses optimized render or updates buffers and uses this render.
	void renderRealtimeGI();

	virtual ~RendererOfOriginalScene();

private:
	// n renderers for n objects
	unsigned layerNumber;
	unsigned layerNumber2;
	float layerBlend; // 0..1, 0=layerNumber, 1=layerNumber2
	unsigned layerNumberFallback;
	unsigned lightIndirectVersion;
	struct PerObjectPermanent
	{
		rr::RRObject* object;
		rr::RRObjectIllumination* illumination;
		rr::RRVec3 objectCenter;
		Renderer* rendererCaching;
		RendererOfRRObject* rendererNonCaching;
		UberProgramSetup recommendedMaterialSetup;
		PerObjectPermanent()
		{
			object = NULL;
			illumination = NULL;
			objectCenter = rr::RRVec3(0);
			rendererCaching = NULL;
			rendererNonCaching = NULL;
		}
		void init(rr::RRObject* _object, rr::RRObjectIllumination* _illumination)
		{
			object = _object;
			illumination = _illumination;
			if (object)
			{
				rr::RRMesh* mesh = object->createWorldSpaceMesh();
				mesh->getAABB(NULL,NULL,&objectCenter);
				delete mesh;
				rendererNonCaching = RendererOfRRObject::create(object,NULL,NULL,true);
#ifdef USE_VBO
				// if we already USE_VBO, wrapping it in display list would
				// + speed up Nvidia cards by ~2%
				// - AMD cards crash in driver (with 9.3, final driver for Radeons up to X2100)
				// better don't create display list
#else
				rendererCaching = rendererNonCaching ? rendererNonCaching->createDisplayList() : NULL;
#endif
				recommendedMaterialSetup.recommendMaterialSetup(object);
			}
		}
		~PerObjectPermanent()
		{
			delete rendererCaching;
			delete rendererNonCaching;
		}
	};
	PerObjectPermanent* perObjectPermanent;
	struct PerObjectSorted
	{
		PerObjectPermanent* permanent;
		rr::RRReal distance;
		bool operator<(const PerObjectSorted& a) const {return distance<a.distance;}
		bool operator>(const PerObjectSorted& a) const {return distance>a.distance;}
	};
	PerObjectSorted* perObjectSorted; // filled and used by render(). might be made thread safe if render() allocates it locally, however GL is unsafe anyway
	void renderOriginalObject(const PerObjectPermanent* perObject, bool renderNonBlended, bool renderBlended);
};


RendererOfOriginalScene::RendererOfOriginalScene(rr::RRDynamicSolver* _solver, const char* _pathToShaders) : RendererOfRRDynamicSolver(_solver,_pathToShaders)
{
	layerNumber = UINT_MAX; // disabled
	layerNumber2 = UINT_MAX; // disabled
	layerNumberFallback = UINT_MAX; // disabled
	lightIndirectVersion = UINT_MAX;
	perObjectPermanent = NULL;
	perObjectSorted = NULL;
}

RendererOfOriginalScene::~RendererOfOriginalScene()
{
	delete[] perObjectSorted;
	delete[] perObjectPermanent;
}

void RendererOfOriginalScene::setIndirectIlluminationSource(unsigned _layerNumber, unsigned _lightIndirectVersion)
{
	layerNumber = _layerNumber;
	layerNumber2 = UINT_MAX; // disabled
	layerBlend = 0;
	layerNumberFallback = UINT_MAX; // disabled
	lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfOriginalScene::setIndirectIlluminationSourceBlend(unsigned _layerNumber1, unsigned _layerNumber2, float _layerBlend, unsigned _layerNumberFallback, unsigned _lightIndirectVersion)
{
	layerNumber = _layerNumber1;
	layerNumber2 = _layerNumber2;
	layerBlend = _layerBlend;
	layerNumberFallback = _layerNumberFallback;
	lightIndirectVersion = _lightIndirectVersion;
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : NULL;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : NULL;
}

// renders single object, materials with blending in blending pass only
void RendererOfOriginalScene::renderOriginalObject(const PerObjectPermanent* perObject, bool renderNonBlended, bool renderBlended)
{
	// tell renderer whether we render blended / nonblended materials
	// early exit when no materials pass filter
	if (!perObject->rendererNonCaching || !perObject->rendererNonCaching->setMaterialFilter(renderNonBlended,renderBlended)) return;

	// How do we render multiple objects + multiple lights?
	// for each object
	//   for each light
	//     render
	// - bigger overdraw / bigger need for additional z-only pass
	// + easily supported by getNextPass()
	// - many shader changes, reusing shaders already set was removed
	// + open path for future multi-light passes

	// working copy of params.uberProgramSetup
	UberProgramSetup mainUberProgramSetup = params.uberProgramSetup;
	mainUberProgramSetup.OBJECT_SPACE = perObject->object->getWorldMatrix()!=NULL;
	// set shader according to vbuf/pbuf presence
	rr::RRBuffer* vbuffer = onlyVbuf(perObject->illumination->getLayer(layerNumber));
	rr::RRBuffer* pbuffer = onlyLmap(perObject->illumination->getLayer(layerNumber));
	rr::RRBuffer* vbuffer2 = onlyVbuf(perObject->illumination->getLayer(layerNumber2));
	rr::RRBuffer* pbuffer2 = onlyLmap(perObject->illumination->getLayer(layerNumber2));
	rr::RRBuffer* pbufferldm = onlyLmap(perObject->illumination->getLayer(params.layerNumberLDM));
	// fallback when buffers are not available
	if (!vbuffer) vbuffer = onlyVbuf(perObject->illumination->getLayer(layerNumberFallback));
	if (!pbuffer) pbuffer = onlyLmap(perObject->illumination->getLayer(layerNumberFallback));
	if (!vbuffer2) vbuffer2 = onlyVbuf(perObject->illumination->getLayer(layerNumberFallback));
	if (!pbuffer2) pbuffer2 = onlyLmap(perObject->illumination->getLayer(layerNumberFallback));
	if (mainUberProgramSetup.LIGHT_INDIRECT_auto)
	{
		mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR = vbuffer && !pbuffer;
		mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = layerBlend && mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR && vbuffer2 && vbuffer2!=vbuffer && !pbuffer2;
		mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = vbuffer && !vbuffer->getScaled();
		mainUberProgramSetup.LIGHT_INDIRECT_MAP = pbuffer?true:false;
		mainUberProgramSetup.LIGHT_INDIRECT_MAP2 = layerBlend && mainUberProgramSetup.LIGHT_INDIRECT_MAP && pbuffer2 && pbuffer2!=pbuffer;
		mainUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = pbufferldm?true:false;
		mainUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = perObject->illumination->specularEnvMap!=NULL; // enable if cube exists, reduceMaterialSetup will disable it if not needed
	}
	// removes all material settings not necessary for given object
	mainUberProgramSetup.reduceMaterialSetup(perObject->recommendedMaterialSetup);
	// final touch
	mainUberProgramSetup.validate();

	PreserveBlend p1;
	PreserveBlendFunc p2;
	PreserveAlphaTest p3;
	PreserveAlphaFunc p4;
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for now, all materials with alpha use this blendmode
	glAlphaFunc(GL_GREATER,0.5f); // for now, all materials with alpha use this alpha-keying
	MultiPass multiPass(params.lights,mainUberProgramSetup,uberProgram,&params.brightness,params.gamma);
	UberProgramSetup uberProgramSetup;
	RendererOfRRObject::RenderedChannels renderedChannels;
	RealtimeLight* light;
	Program* program;
	// here we cached uberProgramSetup and skipped shader+uniforms set when setup didn't change
	// it had to go, but if performance tests show regression, we will return it back somehow
	while (program = multiPass.getNextPass(uberProgramSetup,renderedChannels,light))
	{
		// set transformation
		if (uberProgramSetup.OBJECT_SPACE)
		{
			rr::RRObject* object = perObject->object;
			const rr::RRMatrix3x4* world = object->getWorldMatrix();
			if (world)
			{
				float worldMatrix[16] =
				{
					world->m[0][0],world->m[1][0],world->m[2][0],0,
					world->m[0][1],world->m[1][1],world->m[2][1],0,
					world->m[0][2],world->m[1][2],world->m[2][2],0,
					world->m[0][3],world->m[1][3],world->m[2][3],1
				};
				program->sendUniform("worldMatrix",worldMatrix,false,4);
			}
			else
			{
				float worldMatrix[16] =
				{
					1,0,0,0,
					0,1,0,0,
					0,0,1,0,
					0,0,0,1
				};
				program->sendUniform("worldMatrix",worldMatrix,false,4);
			}
		}

		// set envmaps
		uberProgramSetup.useIlluminationEnvMaps(program,perObject->illumination,false);

		// render
		perObject->rendererNonCaching->setProgram(program);
		perObject->rendererNonCaching->setRenderedChannels(renderedChannels);
		if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 || uberProgramSetup.LIGHT_INDIRECT_MAP2)
		{
			perObject->rendererNonCaching->setIndirectIlluminationBuffersBlend(vbuffer,pbuffer,vbuffer2,pbuffer2,lightIndirectVersion);
			program->sendUniform("lightIndirectBlend",layerBlend);
		}
		else
		{
			perObject->rendererNonCaching->setIndirectIlluminationBuffers(vbuffer,pbuffer,lightIndirectVersion);
		}
		perObject->rendererNonCaching->setLDM(pbufferldm);

		if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR || !perObject->rendererCaching)
			perObject->rendererNonCaching->render(); // don't cache indirect illumination, it changes often
		else
			perObject->rendererCaching->render(); // cache everything else, it's constant
	}
}

void RendererOfOriginalScene::renderRealtimeGI()
{
	lightIndirectVersion = params.solver?params.solver->getSolutionVersion():0;
	// renders realtime GI (on meshes or multimes, what's better)
	if (
		// optimized render is faster and supports rendering into shadowmaps (this will go away with colored shadows)
		!params.renderingFromThisLight
		// optimized render is faster and supports everything in scenes with 1 object
		&& params.solver->getStaticObjects().size()>1
		&& (
			// optimized render can't sort
			params.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND
			// optimized render can't render LDM for more than 1 object
			|| ((params.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP || params.uberProgramSetup.LIGHT_INDIRECT_auto) && params.layerNumberLDM!=UINT_MAX)
			// optimized render looks bad with single specular cube per-scene
			|| (params.uberProgramSetup.MATERIAL_SPECULAR && (params.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR || params.uberProgramSetup.LIGHT_INDIRECT_auto))
		)
		)
	{
		// render per object (properly sorting), with temporary realtime-indirect buffers filled here
		// - update buffers
		params.solver->updateBuffersForRealtimeGI(layerNumber,params.uberProgramSetup.MATERIAL_SPECULAR);
		// - render scene with buffers
		render();
	}
	else
	{
		// render whole scene at once, blended or keyed, but without sort
		RendererOfRRDynamicSolver::render();
	}
}

void RendererOfOriginalScene::render()
{
	rr::RRReportInterval report(rr::INF3,"Rendering original scene...\n");
	g_usingOptimizedScene = false;
	if (!params.solver)
	{
		RR_ASSERT(0);
		return;
	}

	// Create object renderers
	if (!perObjectPermanent)
	{
		perObjectPermanent = new PerObjectPermanent[params.solver->getNumObjects()];
		perObjectSorted = new PerObjectSorted[params.solver->getNumObjects()];
		for (unsigned i=0;i<params.solver->getNumObjects();i++)
		{
			perObjectPermanent[i].init(params.solver->getObject(i),params.solver->getIllumination(i));
			perObjectSorted[i].permanent = perObjectPermanent+i;
			perObjectSorted[i].distance = 0;
		}
	}

	// Sort objects
	const Camera* eye = Camera::getRenderCamera();
	if (!eye)
	{
		RR_ASSERT(0); // eye not set
		return;
	}
	unsigned numObjectsToRender = 0;
	for (unsigned i=0;i<params.solver->getNumObjects();i++) if (params.solver->getObject(i))
	{
		perObjectSorted[numObjectsToRender].permanent = perObjectPermanent+i;
		perObjectSorted[numObjectsToRender].distance = (perObjectPermanent[i].objectCenter-eye->pos).length2();
		numObjectsToRender++;
	}
	std::sort(perObjectSorted,perObjectSorted+numObjectsToRender);

	if (params.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
	{
		// Render opaque objects from 0 to inf
		//  Do it with disabled blend to 1) improve speed 2) avoid MultiPass to prerender Z for opaque object, because it is both useless and dangerous. For some reason it renders opaque objects lit by more than 2 lights as lit only by last light.
		params.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = false;
		for (unsigned i=0;i<numObjectsToRender;i++)
		{
			renderOriginalObject(perObjectSorted[i].permanent,true,false);
		}
		params.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = true;
	}
	else
	{
		// Render all objects from 0 to inf
		for (unsigned i=0;i<numObjectsToRender;i++)
		{
			renderOriginalObject(perObjectSorted[i].permanent,true,true);
		}
	}

	// Render skybox
	if (!params.renderingFromThisLight && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env = params.solver->getEnvironment();
		if (textureRenderer && env)
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(params.brightness,true,!env->getScaled(),params.gamma);
			if (env->getWidth()>2)
				getTexture(env,true,false)->bindTexture(); // smooth
			else
				getTexture(env,false,false,GL_NEAREST,GL_NEAREST)->bindTexture(); // used by 2x2 sky
			glBegin(GL_POLYGON);
				const GLfloat depth = 1;
				glVertex3f(-1,-1,depth);
				glVertex3f(1,-1,depth);
				glVertex3f(1,1,depth);
				glVertex3f(-1,1,depth);
			glEnd();
			textureRenderer->renderEnvironmentEnd();
		}
	}

	if (params.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
	{
		// Render blended objects from inf to 0
		for (unsigned i=numObjectsToRender;i--;)
		{
			renderOriginalObject(perObjectSorted[i].permanent,false,true);
		}
	}
}

void RendererOfOriginalScene::renderLines(bool shortened, int onlyIndex)
{
}

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

RendererOfScene::RendererOfScene(rr::RRDynamicSolver* solver, const char* pathToShaders)
{
	renderer = new RendererOfOriginalScene(solver,pathToShaders);
	dataSource = DS_REALTIME_MULTIMESH;
}

RendererOfScene::~RendererOfScene()
{
	delete renderer;
}

void RendererOfScene::setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight)
{
	renderer->setParams(uberProgramSetup,lights,renderingFromThisLight);
}

void RendererOfScene::setBrightnessGamma(const rr::RRVec4* brightness, float gamma)
{
	renderer->setBrightnessGamma(brightness,gamma);
}

void RendererOfScene::setLDM(unsigned layerNumberLDM)
{
	renderer->setLDM(layerNumberLDM);
}

const void* RendererOfScene::getParams(unsigned& length) const
{
	return renderer->getParams(length);
}

void RendererOfScene::useOriginalScene(unsigned layerNumber, unsigned lightIndirectVersion)
{
	dataSource = DS_LAYER_MESHES;
	renderer->setIndirectIlluminationSource(layerNumber,lightIndirectVersion);
}

void RendererOfScene::useOriginalSceneBlend(unsigned layerNumber1, unsigned layerNumber2, float transition, unsigned layerNumberFallback, unsigned lightIndirectVersion)
{
	dataSource = DS_LAYER_MESHES;
	renderer->setIndirectIlluminationSourceBlend(layerNumber1,layerNumber2,transition,layerNumberFallback,lightIndirectVersion);
}

void RendererOfScene::useOptimizedScene()
{
	dataSource = DS_REALTIME_MULTIMESH;
}

bool RendererOfScene::usingOptimizedScene()
{
	return g_usingOptimizedScene;
}

void RendererOfScene::useRealtimeGI(unsigned layerNumber)
{
	dataSource = DS_REALTIME_AUTO;
	renderer->setIndirectIlluminationSource(layerNumber,0); // 0 is arbitrary number, it won't be used
}

void RendererOfScene::render()
{
	switch (dataSource)
	{
		case DS_LAYER_MESHES:
			// renders GI from layer on meshes
			renderer->render();
			break;
		case DS_REALTIME_MULTIMESH:
			// renders realtime GI on multimesh
			renderer->RendererOfRRDynamicSolver::render();
			break;
		case DS_REALTIME_AUTO:
			// renders realtime GI (on meshes or multimes, what's better)
			renderer->renderRealtimeGI();
			break;
	}
}

void RendererOfScene::renderLines(bool shortened, int onlyIndex)
{
#ifdef DIAGNOSTIC_RAYS
	renderer->renderLines(shortened,onlyIndex);
#endif
}

}; // namespace
