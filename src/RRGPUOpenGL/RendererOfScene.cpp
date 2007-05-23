// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRGPUOpenGL.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfScene.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"


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
class RendererOfRRDynamicSolver : public de::Renderer
{
public:
	RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver, const char* pathToShaders);

	//! Sets parameters of render related to shader and direct illumination.
	void setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Specifies global brightness and gamma factors used by following render() commands.
	void setBrightnessGamma(float brightness[4], float gamma);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfRRDynamicSolver();

protected:
	struct Params
	{
		rr::RRDynamicSolver* solver;
		de::UberProgramSetup uberProgramSetup;
		const de::AreaLight* areaLight;
		const de::Texture* lightDirectMap;
		float brightness[4];
		float gamma;
		Params();
	};
	Params params;
	de::TextureRenderer* textureRenderer;
	de::UberProgram* uberProgram;
private:
	// 1 renderer for 1 scene
	unsigned solutionVersion;
	de::Renderer* rendererCaching;
	RendererOfRRObject* rendererNonCaching;
};


RendererOfRRDynamicSolver::Params::Params()
{
	solver = NULL;
	areaLight = NULL;
	lightDirectMap = NULL;
}

RendererOfRRDynamicSolver::RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver, const char* pathToShaders)
{
	params.solver = solver;
	textureRenderer = new de::TextureRenderer(pathToShaders);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%subershader.vs",pathToShaders);
	_snprintf(buf2,399,"%subershader.fs",pathToShaders);
	uberProgram = new de::UberProgram(buf1,buf2);
	params.brightness[0] = 1;
	params.brightness[1] = 1;
	params.brightness[2] = 1;
	params.brightness[3] = 1;
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

void RendererOfRRDynamicSolver::setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap)
{
	params.uberProgramSetup = uberProgramSetup;
	params.areaLight = areaLight;
	params.lightDirectMap = lightDirectMap;
}

const void* RendererOfRRDynamicSolver::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RendererOfRRDynamicSolver::setBrightnessGamma(float brightness[4], float gamma)
{
	params.brightness[0] = brightness ? brightness[0] : 1;
	params.brightness[1] = brightness ? brightness[1] : 1;
	params.brightness[2] = brightness ? brightness[2] : 1;
	params.brightness[3] = brightness ? brightness[3] : 1;
	params.gamma = gamma;
}

void RendererOfRRDynamicSolver::render()
{
	if(!params.solver || !params.solver->getMultiObjectCustom())
	{
		RR_ASSERT(0);
		return;
	}
	if(params.uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		rr::RRReporter::report(rr::RRReporter::WARN,"LIGHT_INDIRECT_MAP incompatible with useOptimizedScene().\n");
		return;
	}

	// create helper renderers
	if(!rendererNonCaching)
		rendererNonCaching = new rr_gl::RendererOfRRObject(params.solver->getMultiObjectCustom(),params.solver->getStaticSolver(),params.solver->getScaler(),true);
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
			textureRenderer->renderEnvironmentBegin(params.brightness);
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
	if(!params.uberProgramSetup.useProgram(uberProgram,params.areaLight,0,params.lightDirectMap,params.brightness,params.gamma))
	{
		rr::RRReporter::report(rr::RRReporter::ERRO,"Failed to compile or link GLSL program.\n");
		return;
	}
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //!!! do it in RendererOfRRObject according to RRMaterial sideBits
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
	renderedChannels.FORCE_2D_POSITION = params.uberProgramSetup.FORCE_2D_POSITION;
	rendererNonCaching->setRenderedChannels(renderedChannels);
	rendererNonCaching->setIndirectIlluminationFromSolver(params.solver->getSolutionVersion());

	// don't cache indirect illumination in vertices, it changes often
	if(params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
		rendererNonCaching->render();
	else
	// cache everything else, it's constant
		rendererCaching->render();
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
	std::vector<de::Renderer*> renderersCaching;
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
			textureRenderer->renderEnvironmentBegin(params.brightness);
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
	// render static scene
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //!!! do it in RendererOfRRObject according to RRMaterial sideBits

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
	renderedChannels.FORCE_2D_POSITION = params.uberProgramSetup.FORCE_2D_POSITION;

	// - working copy of params.uberProgramSetup
	de::UberProgramSetup uberProgramSetup = params.uberProgramSetup;
	de::UberProgramSetup uberProgramSetupPrevious;
	uberProgramSetup.OBJECT_SPACE = true;

	de::Program* program = NULL;
	unsigned numObjects = params.solver->getNumObjects();
	for(unsigned i=0;i<numObjects;i++)
	{
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
		if(i==0 || (uberProgramSetup.LIGHT_INDIRECT_auto && uberProgramSetup!=uberProgramSetupPrevious))
		{
			program = uberProgramSetup.useProgram(uberProgram,params.areaLight,0,params.lightDirectMap,params.brightness,params.gamma);
			if(!program)
			{
				rr::RRReporter::report(rr::RRReporter::ERRO,"Failed to compile or link GLSL program.\n");
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

void RendererOfScene::setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap)
{
	renderer->setParams(uberProgramSetup,areaLight,lightDirectMap);
}

void RendererOfScene::setBrightnessGamma(float brightness[4], float gamma)
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
