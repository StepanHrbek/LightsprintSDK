#include <cstdio>
#include <string>
#include "Lightsprint/GL/DynamicMeshDemo.h"
#include "Lightsprint/GL/MultiPass.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/ToneMapping.h"

namespace rr_gl
{


/////////////////////////////////////////////////////////////////////////////
//
// Demo, stuff shared by all scenes

char* s_pathToShaders = NULL;
UberProgram* s_uberProgram = NULL;
ToneMapping* s_toneMapping = NULL;
rr::RRBuffer* s_diffuseEnvMap = NULL;
rr::RRBuffer* s_specularEnvMap = NULL;

Demo::Demo(const char* pathToShaders)
{
	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		exit(0);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	// initialize stuff shared by all scenes
	s_pathToShaders = _strdup(pathToShaders);
	s_uberProgram = UberProgram::create((std::string(pathToShaders)+"ubershader.vs").c_str(),(std::string(pathToShaders)+"ubershader.fs").c_str());
	s_toneMapping = new ToneMapping(s_pathToShaders);
	s_diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGBA,true,NULL);
	s_specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGBA,true,NULL);
}

Demo::~Demo()
{
	RR_SAFE_DELETE(s_specularEnvMap);
	RR_SAFE_DELETE(s_diffuseEnvMap);
	RR_SAFE_DELETE(s_toneMapping);
	RR_SAFE_DELETE(s_uberProgram);
	RR_SAFE_FREE(s_pathToShaders);
}


/////////////////////////////////////////////////////////////////////////////
//
// Scene

Scene::Scene(const char* _sceneFilename, unsigned _quality, float _indirectIllumMultiplier)
: brightness(1), RRDynamicSolverGL(s_pathToShaders)
{
	scene = new rr::RRScene(_sceneFilename);
	setScaler(rr::RRScaler::createRgbScaler());
	if (scene->getObjects())
		setStaticObjects(*scene->getObjects(), NULL);
	if (scene->getLights())
		setLights(*scene->getLights());

	// .fireball is built in temp, can be changed to local directory and packed with demo
	loadFireball(NULL) || buildFireball(_quality,NULL);

	// light detail maps (LDM) not built yet, is unwrap available?

	setDirectIlluminationBoost(_indirectIllumMultiplier);
	rendererOfScene = new RendererOfScene(this,s_pathToShaders);
}

void Scene::setEnvironment(rr::RRBuffer* _skybox)
{
	RRDynamicSolver::setEnvironment(_skybox);
}

void Scene::renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
{
	// Render static objects.
	rendererOfScene->setParams(uberProgramSetup,&realtimeLights,renderingFromThisLight);
	rendererOfScene->useRealtimeGI(0);
	rendererOfScene->setBrightnessGamma(&brightness,1);
	rendererOfScene->render();

	// Render dynamic objects.
	bool lit = uberProgramSetup.LIGHT_INDIRECT_auto;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnable(GL_CULL_FACE);
	if (lit)
	{
		// Render with lighting.
		for (unsigned i=0;i<numDynamicMeshes;i++)
		{
		/*
		UberProgramSetup uberProgramSetup;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.useProgram(s_uberProgram,NULL,0,NULL,1,0);
		glColor3ub(0,255,255);
		glVertexPointer(3, GL_FLOAT, 0, dynamicMeshes[i].vertices);
		glDrawElements(GL_TRIANGLES, dynamicMeshes[i].numIndices, GL_UNSIGNED_SHORT, dynamicMeshes[i].indices);
		*/
			bool diffuse = dynamicMeshes[i].material->diffuseReflectance.color!=rr::RRVec3(0);
			bool specular = dynamicMeshes[i].material->specularReflectance.color!=rr::RRVec3(0);
			// - update envmaps
			rr::RRObjectIllumination illumination(dynamicMeshes[i].numVertices);
			rr::RRVec3 sum(0);
			for (unsigned j=0;j<dynamicMeshes[i].numVertices;j++)
				sum += rr::RRVec3(dynamicMeshes[i].vertices[j].position[0],dynamicMeshes[i].vertices[j].position[1],dynamicMeshes[i].vertices[j].position[2]);
			illumination.envMapWorldCenter = sum/dynamicMeshes[i].numVertices;
			illumination.gatherEnvMapSize = 16;
			illumination.diffuseEnvMap = diffuse ? s_diffuseEnvMap : NULL;
			illumination.specularEnvMap = specular ? s_specularEnvMap : NULL;
			updateEnvironmentMap(&illumination);
			// - set shader
			UberProgramSetup dynamicUberProgramSetup;
			dynamicUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = diffuse;
			dynamicUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = specular;
			dynamicUberProgramSetup.MATERIAL_DIFFUSE = diffuse;
			dynamicUberProgramSetup.MATERIAL_DIFFUSE_CONST = diffuse;
			dynamicUberProgramSetup.MATERIAL_SPECULAR = specular;
			dynamicUberProgramSetup.MATERIAL_SPECULAR_CONST = specular;
			dynamicUberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
			MultiPass multiPass(&realtimeLights,dynamicUberProgramSetup,s_uberProgram,&brightness,1,0);
			UberProgramSetup outUberProgramSetup;
			RendererOfRRObject::RenderedChannels outRenderedChannels;
			RealtimeLight* outLight;
			Program* outProgram;
			// - set shader for one light per pass
			while (outProgram = multiPass.getNextPass(outUberProgramSetup,outRenderedChannels,outLight))
			{
				// - send illumination to shader
				outUberProgramSetup.useIlluminationEnvMaps(outProgram,&illumination,true);
				// - send material to shader
				outRenderedChannels.useMaterial(outProgram,dynamicMeshes[i].material);
				// - send positions and normals to shader
				glVertexPointer(3, GL_FLOAT, sizeof(DynamicVertex), dynamicMeshes[i].vertices[0].position);
				if (outRenderedChannels.NORMALS)
				{
					glEnableClientState(GL_NORMAL_ARRAY);
					glNormalPointer(GL_FLOAT, sizeof(DynamicVertex), dynamicMeshes[i].vertices[0].normal);
				}
				// - render
				glDrawElements(GL_TRIANGLES, dynamicMeshes[i].numIndices, GL_UNSIGNED_SHORT, dynamicMeshes[i].indices);
				if (outRenderedChannels.NORMALS)
				{
					glDisableClientState(GL_NORMAL_ARRAY);
				}
			}
			// - keep envmaps, don't let illumination destructor delete them
			illumination.diffuseEnvMap = NULL;
			illumination.specularEnvMap = NULL;
		}
	}
	else
	{
		// Render into shadowmap, Z only.
		// - set shader
		UberProgramSetup uberProgramSetup;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.useProgram(s_uberProgram,NULL,0,NULL,1,0);
		// - disable color writes
		glColorMask(0,0,0,0);
		// - render
		for (unsigned i=0;i<numDynamicMeshes;i++)
		{
			glVertexPointer(3, GL_FLOAT, sizeof(DynamicVertex), dynamicMeshes[i].vertices[0].position);
			glDrawElements(GL_TRIANGLES, dynamicMeshes[i].numIndices, GL_UNSIGNED_SHORT, dynamicMeshes[i].indices);
		}
		glColorMask(1,1,1,1);
	}
	glDisableClientState(GL_VERTEX_ARRAY);
}

void Scene::render(Camera& _camera, unsigned _numDynamicMeshes, DynamicMesh* _dynamicMeshes)
{
	//! Remember render settings, we need them in renderScene() but we can't pass them.
	numDynamicMeshes = _numDynamicMeshes;
	dynamicMeshes = _dynamicMeshes;

	// Updates shadowmaps and indirect in solver.
	calculate();

	// Set camera.
	_camera.update();
	_camera.setupForRender();

	// Render.
	UberProgramSetup uberProgramSetup = getMaterialsInStaticScene();
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_auto = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	renderScene(uberProgramSetup,NULL);

	// Adjust tone mapping operator.
	static TIME oldTime = 0;
	TIME newTime = GETTIME;
	float secondsSinceLastFrame = (newTime-oldTime)/float(PER_SEC);
	if (secondsSinceLastFrame>0 && secondsSinceLastFrame<10 && oldTime)
		s_toneMapping->adjustOperator(secondsSinceLastFrame,brightness,1,0.5f);
	oldTime = newTime;

	//! Forget dynamic objects.
	numDynamicMeshes = 0;
	dynamicMeshes = NULL;
}

void Scene::debugger()
{
	sceneViewer(this,NULL,NULL,s_pathToShaders,NULL,true);
}

Scene::~Scene()
{
	delete rendererOfScene;
	delete getScaler();
	delete scene;
}

}; // namespace
