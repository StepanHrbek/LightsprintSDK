// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <vector>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "MultiPass.h"


namespace rr_gl
{

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
	void setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight, bool honourExpensiveLightingShadowingFlags);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(const rr::RRVec4* brightness, float gamma);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfRRDynamicSolver();

protected:
	struct Params
	{
		rr::RRDynamicSolver* solver;
		UberProgramSetup uberProgramSetup;
		const RealtimeLights* lights;
		const rr::RRLight* renderingFromThisLight;
		bool honourExpensiveLightingShadowingFlags;
		rr::RRVec4 brightness;
		float gamma;
		Params();
	};
	Params params;
	TextureRenderer* textureRenderer;
	UberProgram* uberProgram;
private:
	// 1 renderer for 1 scene
	Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
};


RendererOfRRDynamicSolver::Params::Params()
{
	solver = NULL;
	lights = NULL;
	renderingFromThisLight = NULL;
	honourExpensiveLightingShadowingFlags = false;
	brightness = rr::RRVec4(1);
	gamma = 1;
}

RendererOfRRDynamicSolver::RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver, const char* pathToShaders)
{
	params.solver = solver;
	textureRenderer = new TextureRenderer(pathToShaders);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%subershader.vs",pathToShaders);
	_snprintf(buf2,399,"%subershader.fs",pathToShaders);
	uberProgram = UberProgram::create(buf1,buf2);
	params.brightness = rr::RRVec4(1);
	params.gamma = 1;
	rendererNonCaching = NULL;
	rendererCaching = NULL;
}

RendererOfRRDynamicSolver::~RendererOfRRDynamicSolver()
{
	delete rendererCaching;
	delete rendererNonCaching;
	delete uberProgram;
	delete textureRenderer;
}

void RendererOfRRDynamicSolver::setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight, bool honourExpensiveLightingShadowingFlags)
{
	params.uberProgramSetup = uberProgramSetup;
	params.lights = lights;
	params.renderingFromThisLight = renderingFromThisLight;
	params.honourExpensiveLightingShadowingFlags = honourExpensiveLightingShadowingFlags;
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

void RendererOfRRDynamicSolver::render()
{
	rr::RRReportInterval report(rr::INF3,"Rendering optimized scene...\n");
	if(!params.solver)
	{
		RR_ASSERT(0);
		return;
	}
	if(params.uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		rr::RRReporter::report(rr::WARN,"LIGHT_INDIRECT_MAP incompatible with useOptimizedScene().\n");
		return;
	}

	// render skybox
	if((params.uberProgramSetup.LIGHT_DIRECT
		|| params.uberProgramSetup.LIGHT_INDIRECT_CONST || params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR || params.uberProgramSetup.LIGHT_INDIRECT_MAP || params.uberProgramSetup.LIGHT_INDIRECT_auto
		|| params.uberProgramSetup.MATERIAL_EMISSIVE_CONST || params.uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR || params.uberProgramSetup.MATERIAL_EMISSIVE_MAP
		) && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env = params.solver->getEnvironment();
		if(textureRenderer && env)
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(&params.brightness[0]);
			if(env->getWidth()>2)
				getTexture(env)->bindTexture(); // smooth
			else
				getTexture(env,false,GL_NEAREST,GL_NEAREST)->bindTexture(); // used by 2x2 sky
			glBegin(GL_POLYGON);
				glVertex3f(-1,-1,1);
				glVertex3f(1,-1,1);
				glVertex3f(1,1,1);
				glVertex3f(-1,1,1);
			glEnd();
			textureRenderer->renderEnvironmentEnd();
		}
		else
		{
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	if(!params.solver->getMultiObjectCustom())
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Rendering empty static scene.\n"));
		return;
	}

	// create helper renderers
	if(!rendererNonCaching)
	{
		rendererNonCaching = RendererOfRRObject::create(params.solver->getMultiObjectCustom(),params.solver,params.solver->getScaler(),true);
	}
	if(!rendererCaching && rendererNonCaching)
		rendererCaching = rendererNonCaching->createDisplayList();
	if(!rendererCaching)
	{
		RR_ASSERT(0);
		return;
	}

	if(params.uberProgramSetup.LIGHT_INDIRECT_auto)
	{
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = false;
		params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = true;
		params.uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		params.uberProgramSetup.LIGHT_INDIRECT_MAP2 = false;
		params.uberProgramSetup.LIGHT_INDIRECT_ENV = false;
	}

	PreserveBlend p1;
	PreserveBlendFunc p2;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // for now, all materials with alpha use this blendmode
	MultiPass multiPass(params.lights,params.uberProgramSetup,uberProgram,&params.brightness,params.gamma,params.honourExpensiveLightingShadowingFlags);
	UberProgramSetup uberProgramSetup;
	RendererOfRRObject::RenderedChannels renderedChannels;
	const RealtimeLight* light;
	Program* program;
	while(program=multiPass.getNextPass(uberProgramSetup,renderedChannels,light))
	{
		rendererNonCaching->setProgram(program);
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setIndirectIlluminationFromSolver(params.solver->getSolutionVersion());
		rendererNonCaching->setLightingShadowingFlags(params.renderingFromThisLight,light?light->origin:NULL,params.honourExpensiveLightingShadowingFlags);

		// don't cache indirect illumination in vertices, it changes often
		if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
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
	void setIndirectIlluminationSource(unsigned layerNumber);
	void setIndirectIlluminationSourceBlend(unsigned layerNumber1, unsigned layerNumber2, float transition, unsigned layerNumberFallback);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();
	void renderLines(bool shortened, int onlyIndex);

	virtual ~RendererOfOriginalScene();

private:
	// n renderers for n objects
	unsigned layerNumber;
	unsigned layerNumber2;
	float layerBlend; // 0..1, 0=layerNumber, 1=layerNumber2
	unsigned layerNumberFallback;
	std::vector<Renderer*> renderersCaching;
	std::vector<RendererOfRRObject*> renderersNonCaching;
	bool amdBugWorkaround;
};


RendererOfOriginalScene::RendererOfOriginalScene(rr::RRDynamicSolver* asolver, const char* pathToShaders) : RendererOfRRDynamicSolver(asolver,pathToShaders)
{
	layerNumber = 0;

	amdBugWorkaround = false;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if(renderer)
	{
		// find 4digit number
		unsigned number = 0;
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for(unsigned i=0;renderer[i];i++)
			if(!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}

		// workaround for Catalyst bug (object disappears), observed on X1950, HD2xxx, HD3xxx
		amdBugWorkaround = (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")) && (number>=1900 && number<=4999);
	}
}

RendererOfOriginalScene::~RendererOfOriginalScene()
{
	for(unsigned i=0;i<renderersCaching.size();i++) delete renderersCaching[i];
	for(unsigned i=0;i<renderersNonCaching.size();i++) delete renderersNonCaching[i];
}

void RendererOfOriginalScene::setIndirectIlluminationSource(unsigned alayerNumber)
{
	layerNumber = alayerNumber;
	layerNumber2 = 0;
	layerBlend = 0;
	layerNumberFallback = 0;
}

void RendererOfOriginalScene::setIndirectIlluminationSourceBlend(unsigned alayerNumber1, unsigned alayerNumber2, float alayerBlend, unsigned alayerNumberFallback)
{
	layerNumber = alayerNumber1;
	layerNumber2 = alayerNumber2;
	layerBlend = alayerBlend;
	layerNumberFallback = alayerNumberFallback;
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : NULL;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : NULL;
}

void RendererOfOriginalScene::render()
{
	rr::RRReportInterval report(rr::INF3,"Rendering original scene...\n");
	// create helper renderers
	if(!params.solver)
	{
		RR_ASSERT(0);
		return;
	}

	// render skybox
	if((params.uberProgramSetup.LIGHT_DIRECT
		|| params.uberProgramSetup.LIGHT_INDIRECT_CONST || params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR || params.uberProgramSetup.LIGHT_INDIRECT_MAP || params.uberProgramSetup.LIGHT_INDIRECT_auto
		|| params.uberProgramSetup.MATERIAL_EMISSIVE_CONST || params.uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR || params.uberProgramSetup.MATERIAL_EMISSIVE_MAP
		) && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env = params.solver->getEnvironment();
		if(textureRenderer && env)
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(&params.brightness[0]);
			if(env->getWidth()>2)
				getTexture(env)->bindTexture(); // smooth
			else
				getTexture(env,false,GL_NEAREST,GL_NEAREST)->bindTexture(); // used by 2x2 sky
			glBegin(GL_POLYGON);
			glVertex3f(-1,-1,1);
			glVertex3f(1,-1,1);
			glVertex3f(1,1,1);
			glVertex3f(-1,1,1);
			glEnd();
			textureRenderer->renderEnvironmentEnd();
		}
		else
		{
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	// How do we render multiple objects + multiple lights?
	// for each object
	//   for each light
	//     render
	// - bigger overdraw / bigger need for additional z-only pass
	// + easily supported by getNextPass()
	// - many shader changes, reusing shaders already set was removed
	// + open path for future multi-light passes
	for(unsigned i=0;i<params.solver->getNumObjects();i++) if(params.solver->getObject(i))
	{
		// - working copy of params.uberProgramSetup
		UberProgramSetup mainUberProgramSetup = params.uberProgramSetup;
		mainUberProgramSetup.OBJECT_SPACE = params.solver->getObject(i)->getWorldMatrix()!=NULL;
		// - set shader according to vbuf/pbuf presence
		rr::RRBuffer* vbuffer = onlyVbuf(params.solver->getIllumination(i)->getLayer(layerNumber));
		rr::RRBuffer* pbuffer = onlyLmap(params.solver->getIllumination(i)->getLayer(layerNumber));
		//   - second
		rr::RRBuffer* vbuffer2 = onlyVbuf(params.solver->getIllumination(i)->getLayer(layerNumber2));
		rr::RRBuffer* pbuffer2 = onlyLmap(params.solver->getIllumination(i)->getLayer(layerNumber2));
		//   - fallback when buffers are not available
		if(!vbuffer) vbuffer = onlyVbuf(params.solver->getIllumination(i)->getLayer(layerNumberFallback));
		if(!pbuffer) pbuffer = onlyLmap(params.solver->getIllumination(i)->getLayer(layerNumberFallback));
		if(!vbuffer2) vbuffer2 = onlyVbuf(params.solver->getIllumination(i)->getLayer(layerNumberFallback));
		if(!pbuffer2) pbuffer2 = onlyLmap(params.solver->getIllumination(i)->getLayer(layerNumberFallback));
		if(mainUberProgramSetup.LIGHT_INDIRECT_auto)
		{
			mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR = vbuffer && !pbuffer;
			mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = layerBlend && mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR && vbuffer2 && vbuffer2!=vbuffer && !pbuffer2;
			mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = vbuffer && !vbuffer->getScaled();
			mainUberProgramSetup.LIGHT_INDIRECT_MAP = pbuffer?true:false;
			mainUberProgramSetup.LIGHT_INDIRECT_MAP2 = layerBlend && mainUberProgramSetup.LIGHT_INDIRECT_MAP && pbuffer2 && pbuffer2!=pbuffer;
		}
		mainUberProgramSetup.validate();

		PreserveBlend p1;
		PreserveBlendFunc p2;
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // for now, all materials with alpha use this blendmode
		MultiPass multiPass(params.lights,mainUberProgramSetup,uberProgram,&params.brightness,params.gamma,params.honourExpensiveLightingShadowingFlags);
		UberProgramSetup uberProgramSetup;
		RendererOfRRObject::RenderedChannels renderedChannels;
		const RealtimeLight* light;
		Program* program;
		// here we cached uberProgramSetup and skipped shader+uniforms set when setup didn't change
		// it had to go, but if performance tests show regression, we will return it back somehow
		while(program = multiPass.getNextPass(uberProgramSetup,renderedChannels,light))
		{
			// - set transformation
			if(uberProgramSetup.OBJECT_SPACE)
			{
				rr::RRObject* object = params.solver->getObject(i);
				const rr::RRMatrix3x4* world = object->getWorldMatrix();
				if(world)
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

			// - create missing renderers
			if(i>=renderersNonCaching.size())
			{
				while(i>renderersNonCaching.size()) renderersNonCaching.push_back(NULL); // renderers with lower i missing? -> must be NULL objects, set NULL renderers
				renderersNonCaching.push_back(rr_gl::RendererOfRRObject::create(params.solver->getObject(i),NULL,NULL,true));
			}
			if(i>=renderersCaching.size())
			{
				while(i>renderersCaching.size()) renderersCaching.push_back(NULL); // renderers with lower i missing? -> must be NULL objects, set NULL renderers
				renderersCaching.push_back(renderersNonCaching[i]->createDisplayList());
			}
			// - render
			if(renderersNonCaching[i])
			{
				renderersNonCaching[i]->setProgram(program);
				renderersNonCaching[i]->setRenderedChannels(renderedChannels);
				if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 || uberProgramSetup.LIGHT_INDIRECT_MAP2)
				{
					renderersNonCaching[i]->setIndirectIlluminationBuffersBlend(vbuffer,pbuffer,vbuffer2,pbuffer2);
					program->sendUniform("lightIndirectBlend",layerBlend);
				}
				else
				{
					renderersNonCaching[i]->setIndirectIlluminationBuffers(vbuffer,pbuffer);
				}
				if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR || (uberProgramSetup.OBJECT_SPACE && amdBugWorkaround))
					renderersNonCaching[i]->render(); // don't cache indirect illumination, it changes often. don't cache on some radeons, they are buggy
				else
					renderersCaching[i]->render(); // cache everything else, it's constant
			}
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
	useOptimized = true;
}

RendererOfScene::~RendererOfScene()
{
	delete renderer;
}

void RendererOfScene::setParams(const UberProgramSetup& uberProgramSetup, const RealtimeLights* lights, const rr::RRLight* renderingFromThisLight, bool honourExpensiveLightingShadowingFlags)
{
	renderer->setParams(uberProgramSetup,lights,renderingFromThisLight,honourExpensiveLightingShadowingFlags);
}

void RendererOfScene::setBrightnessGamma(const rr::RRVec4* brightness, float gamma)
{
	renderer->setBrightnessGamma(brightness,gamma);
}

const void* RendererOfScene::getParams(unsigned& length) const
{
	return renderer->getParams(length);
}

void RendererOfScene::useOriginalScene(unsigned layerNumber)
{
	useOptimized = false;
	renderer->setIndirectIlluminationSource(layerNumber);
}

void RendererOfScene::useOriginalSceneBlend(unsigned layerNumber1, unsigned layerNumber2, float transition, unsigned layerNumberFallback)
{
	useOptimized = false;
	renderer->setIndirectIlluminationSourceBlend(layerNumber1,layerNumber2,transition,layerNumberFallback);
}

void RendererOfScene::useOptimizedScene()
{
	useOptimized = true;
}

void RendererOfScene::render()
{
	if(useOptimized)
		renderer->RendererOfRRDynamicSolver::render();
	else
		renderer->render();
}

void RendererOfScene::renderLines(bool shortened, int onlyIndex)
{
#ifdef DIAGNOSTIC_RAYS
	renderer->renderLines(shortened,onlyIndex);
#endif
}

}; // namespace
