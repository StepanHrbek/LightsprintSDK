// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <vector>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "../DemoEngine/PreserveState.h"

#ifdef RR_DEVELOPMENT
#define DIAGNOSTIC_RAYS // render rays shot by solver (RR_DEVELOPMENT must be on)
#endif

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
	void setParams(const UberProgramSetup& uberProgramSetup, const rr::RRVector<RealtimeLight*>* lights);

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
		const rr::RRVector<RealtimeLight*>* lights;
		rr::RRVec4 brightness;
		float gamma;
		Params();
	};
	Params params;
	TextureRenderer* textureRenderer;
	UberProgram* uberProgram;
private:
	// 1 renderer for 1 scene
	unsigned solutionVersion;
	Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
};


RendererOfRRDynamicSolver::Params::Params()
{
	solver = NULL;
	lights = NULL;
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
	solutionVersion = 0;
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

void RendererOfRRDynamicSolver::setParams(const UberProgramSetup& uberProgramSetup, const rr::RRVector<RealtimeLight*>* lights)
{
	params.uberProgramSetup = uberProgramSetup;
	params.lights = lights;
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
	if(!params.solver || !params.solver->getMultiObjectCustom())
	{
		RR_ASSERT(0);
		return;
	}
	if(params.uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		rr::RRReporter::report(rr::WARN,"LIGHT_INDIRECT_MAP incompatible with useOptimizedScene().\n");
		return;
	}

	// create helper renderers
	if(!rendererNonCaching)
	{
		rendererNonCaching = new RendererOfRRObject(params.solver->getMultiObjectCustom(),params.solver,params.solver->getScaler(),true);
	}
	if(!rendererCaching && rendererNonCaching)
		rendererCaching = rendererNonCaching->createDisplayList();
	if(!rendererCaching)
	{
		RR_ASSERT(0);
		return;
	}

	// render skybox
	if(params.uberProgramSetup.LIGHT_DIRECT && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		if(textureRenderer && params.solver->getEnvironment())
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(&params.brightness[0]);
			params.solver->getEnvironment()->bindTexture();
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

	// render static scene
	// 0 lights -> render 1x
	// n lights -> render nx
	unsigned numLights = params.lights?params.lights->size():0;
	unsigned numPasses = MAX(1,numLights);
	UberProgramSetup uberProgramSetup = params.uberProgramSetup;
	PreserveBlend p1;
	for(unsigned lightIndex=0;lightIndex<numPasses;lightIndex++)
	{
		if(lightIndex<numLights)
		{
			// adjust program for n-th light
			rr_gl::RealtimeLight* light = (*params.lights)[lightIndex];
			RR_ASSERT(light);
			//uberProgramSetup.setLightDirect(light,params.lightDirectMap);
			uberProgramSetup.SHADOW_MAPS = params.uberProgramSetup.SHADOW_MAPS ? light->getNumInstances() : 0;
			uberProgramSetup.SHADOW_PENUMBRA = light->areaType!=RealtimeLight::POINT;
			uberProgramSetup.LIGHT_DIRECT_COLOR = params.uberProgramSetup.LIGHT_DIRECT_COLOR && light->origin && light->origin->color!=rr::RRVec3(1);
			uberProgramSetup.LIGHT_DIRECT_MAP = params.uberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light->areaType!=RealtimeLight::POINT;
			uberProgramSetup.LIGHT_DIRECTIONAL = light->getParent()->orthogonal;
			uberProgramSetup.LIGHT_DISTANCE_PHYSICAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
			uberProgramSetup.LIGHT_DISTANCE_POLYNOMIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
			uberProgramSetup.LIGHT_DISTANCE_EXPONENTIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		}
		else
		{
			// adjust program for render without lights
			//uberProgramSetup.setLightDirect(NULL,NULL);
			uberProgramSetup.SHADOW_MAPS = 0;
			uberProgramSetup.SHADOW_SAMPLES = 0;
			uberProgramSetup.LIGHT_DIRECT = 0;
			uberProgramSetup.LIGHT_DIRECT_COLOR = 0;
			uberProgramSetup.LIGHT_DIRECT_MAP = 0;
			uberProgramSetup.LIGHT_DIRECTIONAL = 0;
			uberProgramSetup.LIGHT_DISTANCE_PHYSICAL = 0;
			uberProgramSetup.LIGHT_DISTANCE_POLYNOMIAL = 0;
			uberProgramSetup.LIGHT_DISTANCE_EXPONENTIAL = 0;
		}
		if(lightIndex==1)
		{
			// additional passes add to framebuffer
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE,GL_ONE);
			// additional passes don't include indirect
			uberProgramSetup.LIGHT_INDIRECT_auto = 0;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = 0;
		}
		if(!uberProgramSetup.useProgram(uberProgram,(lightIndex<numLights)?(*params.lights)[lightIndex]:NULL,0,&params.brightness,params.gamma))
		{
			rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n");
			return;
		}

		rr_gl::RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.NORMALS = uberProgramSetup.LIGHT_DIRECT || uberProgramSetup.LIGHT_INDIRECT_ENV || uberProgramSetup.POSTPROCESS_NORMALS;
		renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
		renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
		renderedChannels.LIGHT_INDIRECT_VCOLOR2 = uberProgramSetup.LIGHT_INDIRECT_VCOLOR2;
		renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
		renderedChannels.LIGHT_INDIRECT_MAP2 = uberProgramSetup.LIGHT_INDIRECT_MAP2;
		renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
		renderedChannels.MATERIAL_DIFFUSE_VCOLOR = uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
		renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
		renderedChannels.MATERIAL_EMISSIVE_MAP = uberProgramSetup.MATERIAL_EMISSIVE_MAP;
		renderedChannels.MATERIAL_CULLING = (uberProgramSetup.MATERIAL_DIFFUSE || uberProgramSetup.MATERIAL_SPECULAR) && !uberProgramSetup.FORCE_2D_POSITION; // should be enabled for all except for shadowmaps and force_2d
		renderedChannels.MATERIAL_BLENDING = lightIndex==0; // material wishes are respected only in first pass, other passes use adding
		renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setIndirectIlluminationFromSolver(params.solver->getSolutionVersion());

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
};


RendererOfOriginalScene::RendererOfOriginalScene(rr::RRDynamicSolver* asolver, const char* pathToShaders) : RendererOfRRDynamicSolver(asolver,pathToShaders)
{
	layerNumber = 0;
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
	if((params.uberProgramSetup.LIGHT_DIRECT || params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR || params.uberProgramSetup.LIGHT_INDIRECT_MAP || params.uberProgramSetup.LIGHT_INDIRECT_auto) && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		if(textureRenderer && params.solver->getEnvironment())
		{
			//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
			textureRenderer->renderEnvironmentBegin(&params.brightness[0]);
			params.solver->getEnvironment()->bindTexture();
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
/*
	// update vertex buffer if it needs update
	if(params.solver->getSolutionVersion()!=solutionVersion)
	{
		solutionVersion = params.solver->getSolutionVersion();
		params.solver->updateVertexBuffers(layerNumber,true,RM_IRRADIANCE_PHYSICAL_INDIRECT);
	}
*/
	//!!! ignores lights, uses only first one

	// render static scene
	rr_gl::RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.NORMALS = params.uberProgramSetup.LIGHT_DIRECT || params.uberProgramSetup.LIGHT_INDIRECT_ENV || params.uberProgramSetup.POSTPROCESS_NORMALS;
	renderedChannels.LIGHT_DIRECT = params.uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_VCOLOR = params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
	renderedChannels.LIGHT_INDIRECT_VCOLOR2 = params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR2;
	renderedChannels.LIGHT_INDIRECT_MAP = params.uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_MAP2 = params.uberProgramSetup.LIGHT_INDIRECT_MAP2;
	renderedChannels.LIGHT_INDIRECT_ENV = params.uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = params.uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = params.uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_EMISSIVE_MAP = params.uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.MATERIAL_CULLING = (params.uberProgramSetup.MATERIAL_DIFFUSE || params.uberProgramSetup.MATERIAL_SPECULAR) && !params.uberProgramSetup.FORCE_2D_POSITION; // should be enabled for all except for shadowmaps and force_2d
	renderedChannels.MATERIAL_BLENDING = 1;//!!!lightIndex==0; // material wishes are respected only in first pass, other passes use adding
	renderedChannels.FORCE_2D_POSITION = params.uberProgramSetup.FORCE_2D_POSITION;

	UberProgramSetup uberProgramSetupPrevious;
	Program* program = NULL;
	unsigned numObjects = params.solver->getNumObjects();
	if(params.lights && params.lights->size()>1)
		rr::RRReporter::report(rr::WARN,"Renderer of original scene supports only 1 light (temporarily).\n");
	for(unsigned i=0;i<numObjects;i++)
	{
		// - working copy of params.uberProgramSetup
		UberProgramSetup uberProgramSetup = params.uberProgramSetup;
		uberProgramSetup.OBJECT_SPACE = true;
		// - set shader according to vbuf/pbuf presence
		rr::RRIlluminationVertexBuffer* vbuffer = params.solver->getIllumination(i)->getLayer(layerNumber)->vertexBuffer;
		rr::RRIlluminationPixelBuffer* pbuffer = params.solver->getIllumination(i)->getLayer(layerNumber)->pixelBuffer;
		//   - second
		rr::RRIlluminationVertexBuffer* vbuffer2 = params.solver->getIllumination(i)->getLayer(layerNumber2)->vertexBuffer;
		rr::RRIlluminationPixelBuffer* pbuffer2 = params.solver->getIllumination(i)->getLayer(layerNumber2)->pixelBuffer;
		//   - fallback when buffers are not available
		if(!vbuffer) vbuffer = params.solver->getIllumination(i)->getLayer(layerNumberFallback)->vertexBuffer;
		if(!pbuffer) pbuffer = params.solver->getIllumination(i)->getLayer(layerNumberFallback)->pixelBuffer;
		if(!vbuffer2) vbuffer2 = params.solver->getIllumination(i)->getLayer(layerNumberFallback)->vertexBuffer;
		if(!pbuffer2) pbuffer2 = params.solver->getIllumination(i)->getLayer(layerNumberFallback)->pixelBuffer;
		if(uberProgramSetup.LIGHT_INDIRECT_auto)
		{
			renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR = vbuffer && !pbuffer;
			renderedChannels.LIGHT_INDIRECT_VCOLOR2 = uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = layerBlend && uberProgramSetup.LIGHT_INDIRECT_VCOLOR && vbuffer2 && vbuffer2!=vbuffer && !pbuffer2;
			renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP = pbuffer?true:false;
			renderedChannels.LIGHT_INDIRECT_MAP2 = uberProgramSetup.LIGHT_INDIRECT_MAP2 = layerBlend && uberProgramSetup.LIGHT_INDIRECT_MAP && pbuffer2 && pbuffer2!=pbuffer;
		}
		uberProgramSetup.validate();
		if(i==0 || (uberProgramSetup.LIGHT_INDIRECT_auto && uberProgramSetup!=uberProgramSetupPrevious))
		{
			program = uberProgramSetup.useProgram(uberProgram,(params.lights&&params.lights->size())?(*params.lights)[0]:NULL,0,&params.brightness,params.gamma);
			if(!program)
			{
				rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n");
				return;
			}
			uberProgramSetupPrevious = uberProgramSetup;
		}

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
			renderersNonCaching.push_back(new rr_gl::RendererOfRRObject(params.solver->getObject(i),NULL,NULL,true));
		}
		if(i>=renderersCaching.size())
		{
			renderersCaching.push_back(renderersNonCaching[i]->createDisplayList());
		}
		// - render
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
		if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
			renderersNonCaching[i]->render(); // don't cache indirect illumination, it changes
		else
			renderersCaching[i]->render(); // cache everything else, it's constant
	}
}

void RendererOfOriginalScene::renderLines(bool shortened, int onlyIndex)
{
#ifdef RR_DEVELOPMENT
	if((params.uberProgramSetup.LIGHT_DIRECT || params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR || params.uberProgramSetup.LIGHT_INDIRECT_MAP || params.uberProgramSetup.LIGHT_INDIRECT_auto) && !params.uberProgramSetup.FORCE_2D_POSITION)
	{
		UberProgramSetup uberProgramSetupLines;
		uberProgramSetupLines.LIGHT_INDIRECT_VCOLOR = 1;
		uberProgramSetupLines.MATERIAL_DIFFUSE = 1;
		Program* program = uberProgramSetupLines.useProgram(uberProgram,NULL,0,NULL,NULL,1);
		rr::RRStaticSolverStatistics* stats = params.solver->getStaticSolver()->getSceneStatistics();
		glBegin(GL_LINES);
		for(unsigned i=0;i<stats->numLineSegments;i++) if(onlyIndex<0 || stats->lineSegments[i].index==onlyIndex)
		{
			if(stats->lineSegments[i].unreliable)
				glColor3ub(255,0,0);
			else
			if(stats->lineSegments[i].infinite)
				glColor3ub(0,0,255);
			else
				glColor3ub(0,255,0);
			glVertex3fv(&stats->lineSegments[i].point[0][0]);
			glColor3ub(0,0,0);
			if(shortened)
			{
				rr::RRVec3 tmp = stats->lineSegments[i].point[0] + (stats->lineSegments[i].point[1]-stats->lineSegments[i].point[0]).normalized()*0.01f;
				glVertex3fv(&tmp[0]);
			}
			else
			{
				glVertex3fv(&stats->lineSegments[i].point[1][0]);
			}
		}
		glEnd();
	}
#endif
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

void RendererOfScene::setParams(const UberProgramSetup& uberProgramSetup, const rr::RRVector<RealtimeLight*>* lights)
{
	renderer->setParams(uberProgramSetup,lights);
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
