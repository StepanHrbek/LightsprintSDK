// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRGPUOpenGL/RendererOfScene.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"
#include "Lightsprint/RRGPUOpenGL.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"

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
	RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver);

	//! Sets parameters of render related to shader and direct illumination.
	void setParams(const de::UberProgramSetup& uberProgramSetup, const de::AreaLight* areaLight, const de::Texture* lightDirectMap);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

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

RendererOfRRDynamicSolver::RendererOfRRDynamicSolver(rr::RRDynamicSolver* solver)
{
	params.solver = solver;
	textureRenderer = new de::TextureRenderer("..\\..\\data\\shaders\\");
	uberProgram = new de::UberProgram("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");
	rendererNonCaching = NULL;
	rendererCaching = NULL;
	solutionVersion = 0;
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

void RendererOfRRDynamicSolver::render()
{
	// create helper renderers
	if(!params.solver || !params.solver->getMultiObjectCustom())
	{
		RR_ASSERT(0);
		return;
	}
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
	if(params.uberProgramSetup.LIGHT_DIRECT && textureRenderer)
	{
		//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
		textureRenderer->renderEnvironmentBegin(NULL);
		params.solver->getEnvironment()->bindTexture();
		glBegin(GL_POLYGON);
		glVertex3f(-1,-1,1);
		glVertex3f(1,-1,1);
		glVertex3f(1,1,1);
		glVertex3f(-1,1,1);
		glEnd();
		textureRenderer->renderEnvironmentEnd();
	}

	// render static scene
	if(!params.uberProgramSetup.useProgram(uberProgram,params.areaLight,0,params.lightDirectMap,NULL,1))
	{
		rr::RRReporter::report(rr::RRReporter::ERRO,"Failed to compile or link GLSL program.\n");
		return;
	}
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //!!! do it in RendererOfRRObject according to RRMaterial sideBits
	rr_gl::RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = params.uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_VCOLOR = params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = params.uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_ENV = params.uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = params.uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = params.uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_EMISSIVE_MAP = params.uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.FORCE_2D_POSITION = params.uberProgramSetup.FORCE_2D_POSITION;
	rendererNonCaching->setRenderedChannels(renderedChannels);
	rendererNonCaching->setIndirectIlluminationFromSolver(params.solver->getSolutionVersion());
	if(params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
		rendererNonCaching->render(); // don't cache indirect illumination, it changes
	else
		rendererCaching->render(); // cache everything else, it's constant
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
	RendererOfOriginalScene(rr::RRDynamicSolver* solver);

	//! Sets source of indirect illumination. It is layer 0 by default.
	//! \param layerNumber
	//!  Indirect illumination will be taken from given layer.
	void setIndirectIlluminationSource(unsigned layerNumber);

	//! Renders object, sets shaders, feeds OpenGL with object's data selected by setParams().
	virtual void render();

	virtual ~RendererOfOriginalScene();

private:
	// n renderers for n objects
	unsigned layerNumber;
	std::vector<de::Renderer*> renderersCaching;
	std::vector<RendererOfRRObject*> renderersNonCaching;
};


RendererOfOriginalScene::RendererOfOriginalScene(rr::RRDynamicSolver* asolver) : RendererOfRRDynamicSolver(asolver)
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
	if(params.uberProgramSetup.LIGHT_DIRECT && textureRenderer)
	{
		//textureRenderer->renderEnvironment(params.solver->getEnvironment(),NULL);
		textureRenderer->renderEnvironmentBegin(NULL);
		params.solver->getEnvironment()->bindTexture();
		glBegin(GL_POLYGON);
		glVertex3f(-1,-1,1);
		glVertex3f(1,-1,1);
		glVertex3f(1,1,1);
		glVertex3f(-1,1,1);
		glEnd();
		textureRenderer->renderEnvironmentEnd();
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
	renderedChannels.LIGHT_DIRECT = params.uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_VCOLOR = params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = params.uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_ENV = params.uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = params.uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = params.uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_EMISSIVE_MAP = params.uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.FORCE_2D_POSITION = params.uberProgramSetup.FORCE_2D_POSITION;

	// working copy of params.uberProgramSetup
	de::UberProgramSetup uberProgramSetup = params.uberProgramSetup;

	unsigned numObjects = params.solver->getNumObjects();
	for(unsigned i=0;i<numObjects;i++)
	{
		// - set shader according to vbuf/pbuf presence
		rr::RRIlluminationVertexBuffer* vbuffer = params.solver->getIllumination(i)->getLayer(layerNumber)->vertexBuffer;
		rr::RRIlluminationPixelBuffer* pbuffer = params.solver->getIllumination(i)->getLayer(layerNumber)->pixelBuffer;
		if(i==0 || (uberProgramSetup.LIGHT_INDIRECT_auto && ((vbuffer?true:false)!=uberProgramSetup.LIGHT_INDIRECT_VCOLOR || (pbuffer?true:false)!=uberProgramSetup.LIGHT_INDIRECT_MAP)))
		{
			if(uberProgramSetup.LIGHT_INDIRECT_auto)
			{
				renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR = vbuffer && !pbuffer;
				renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP = pbuffer?true:false;
			}
			if(!uberProgramSetup.useProgram(uberProgram,params.areaLight,0,params.lightDirectMap,NULL,1))
			{
				rr::RRReporter::report(rr::RRReporter::ERRO,"Failed to compile or link GLSL program.\n");
				return;
			}
		}
		if(i>=renderersNonCaching.size())
		{
			renderersNonCaching.push_back(new rr_gl::RendererOfRRObject(params.solver->getObject(i),NULL,NULL,true));
		}
		if(i>=renderersCaching.size())
		{
			renderersCaching.push_back(renderersNonCaching[i]->createDisplayList());
		}
		renderersNonCaching[i]->setRenderedChannels(renderedChannels);
		renderersNonCaching[i]->setIndirectIlluminationBuffers(vbuffer,pbuffer);
		if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
			renderersNonCaching[i]->render(); // don't cache indirect illumination, it changes
		else
			renderersCaching[i]->render(); // cache everything else, it's constant
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

RendererOfScene::RendererOfScene(rr::RRDynamicSolver* solver)
{
	renderer = new RendererOfOriginalScene(solver);
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

const void* RendererOfScene::getParams(unsigned& length) const
{
	return renderer->getParams(length);
}

void RendererOfScene::useOriginalScene(unsigned layerNumber)
{
	useOptimized = false;
	renderer->setIndirectIlluminationSource(layerNumber);
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

}; // namespace
