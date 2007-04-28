// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRGPUOpenGL/RendererOfScene.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRDynamicSolver

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
// RendererOfScene

RendererOfScene::RendererOfScene(rr::RRDynamicSolver* asolver) : RendererOfRRDynamicSolver(asolver)
{
	channelNumber = 0;
}

RendererOfScene::~RendererOfScene()
{
	for(unsigned i=0;i<renderersCaching.size();i++) delete renderersCaching[i];
	for(unsigned i=0;i<renderersNonCaching.size();i++) delete renderersNonCaching[i];
}

void RendererOfScene::setIndirectIlluminationSource(unsigned achannelNumber)
{
	channelNumber = achannelNumber;
}

void RendererOfScene::render()
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
		params.solver->updateVertexBuffers(channelNumber,true,RM_IRRADIANCE_PHYSICAL_INDIRECT);
	}
*/
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

	unsigned numObjects = params.solver->getNumObjects();
	for(unsigned i=0;i<numObjects;i++)
	{
		if(i>=renderersNonCaching.size())
		{
			renderersNonCaching.push_back(new rr_gl::RendererOfRRObject(params.solver->getMultiObjectCustom(),params.solver->getStaticSolver(),params.solver->getScaler(),true));
		}
		if(i>=renderersCaching.size())
		{
			renderersCaching.push_back(renderersNonCaching[i]->createDisplayList());
		}
		renderersNonCaching[i]->setRenderedChannels(renderedChannels);
		renderersNonCaching[i]->setIndirectIlluminationBuffers(params.solver->getIllumination(i)->getChannel(channelNumber)->vertexBuffer,params.solver->getIllumination(i)->getChannel(channelNumber)->pixelBuffer);
		if(params.uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
			renderersNonCaching[i]->render(); // don't cache indirect illumination, it changes
		else
			renderersCaching[i]->render(); // cache everything else, it's constant
	}
}

}; // namespace
