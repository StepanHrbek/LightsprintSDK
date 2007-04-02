// --------------------------------------------------------------------------
// Hello Realtime Radiosity sample
//
// Use of RealtimeRadiosity is demonstrated on .3ds scene viewer.
// You should be familiar with OpenGL and GLUT to read the code.
//
// This is HelloDemoEngine with Lightsprint engine integrated,
// see how the same scene looks better with global illumination.
//
// See #define AMBIENT_MAPS
//  to compute and save ambient maps or lightmaps.
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
// If defined AMBIENT_MAPS:
//  spacebar = compute higher quality ambient maps
//  s = Save maps to disk (alt-tab to console to see filenames)
//  l = Load maps from disk, stop realtime global illumination
//  r = return to Realtime global illumination
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRRealtimeRadiosity.h"
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"
#include "../../samples/Import3DS/RRObject3DS.h"
#include "DynamicObject.h"

//#define AMBIENT_MAPS
// By uncommenting, switch from vertex based global illumination to texture based one.
// - Ambient maps are GENERATED and rendered in REALTIME,
//   every frame new set of maps for all objects in scene.
// - Press spacebar to start high quality precalculation
//   and alt-tab to watch progress in console (takes several minutes).
//   Once precalculation is finished, you can walk through scene and review map.
// - You can turn this demo into ambient map precalculator by saving maps to disk.
//   Save & load are demonstrated on 's' and 'l' keys.
// - To increase ambient map quality,
//   1) provide unwrap uv for meshes (see getTriangleMapping in RRObject3DS.cpp)
//   2) call updateAmbientMap() with higher quality
//   3) increase ambient map resolution (see newPixelBuffer)
// - To generate maps 10x faster
//   1) provide unwrap uv for meshes (see getTriangleMapping in RRObject3DS.cpp)
//      and decrease map resolution (see newPixelBuffer)

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nTry upgrading drivers for your graphics card.\nIf it doesn't help, your graphics card may be too old.\nCards known to work: GEFORCE 6xxx/7xxx, RADEON 95xx+/Xxxx/X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

de::Model_3DS           m3ds;
de::Camera              eye = {{-1.416,1.741,-3.646},12.230,0,0.050,1.3,70.0,0.3,60.0};
de::Camera              light = {{-1.802,0.715,0.850},0.635,0,-5.800,1.0,70.0,1.0,20.0};
de::AreaLight*          areaLight = NULL;
de::Texture*            lightDirectMap = NULL;
de::UberProgram*        uberProgram = NULL;
rr_gl::RendererOfRRObject* rendererNonCaching = NULL;
de::Renderer*           rendererCaching = NULL;
rr_gl::RRRealtimeRadiosityGL* solver = NULL;
DynamicObject*          robot = NULL;
DynamicObject*          potato = NULL;
int                     winWidth = 0;
int                     winHeight = 0;
bool                    modeMovingEye = false;
float                   speedForward = 0;
float                   speedBack = 0;
float                   speedRight = 0;
float                   speedLeft = 0;
bool                    ambientMapsRealtimeUpdate = true;
bool                    environmentMapsRealtimeUpdate = true;
bool                    quadro = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

// callback that feeds 3ds renderer with our vertex illumination
const float* lockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRRealtimeRadiosity*)solver)->getIllumination(object)->getChannel(0)->vertexBuffer;
	return vertexBuffer ? &vertexBuffer->lock()->x : NULL;
}

// callback that cleans vertex illumination
void unlockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRRealtimeRadiosity*)solver)->getIllumination(object)->getChannel(0)->vertexBuffer;
	if(vertexBuffer) vertexBuffer->unlock();
}

void renderScene(de::UberProgramSetup uberProgramSetup)
{
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap))
		error("Failed to compile or link GLSL program.\n",true);
#ifndef AMBIENT_MAPS
	// 3ds renderer m3ds.Draw uses vertex buffers incompatible with our generated ambient map uv channel,
	//  so it doesn't render properly with ambient maps.
	//  could be fixed with better uv or simple geometry shader
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(solver,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.LIGHT_INDIRECT_VCOLOR?lockVertexIllum:NULL,unlockVertexIllum);
	}
	else
#endif
	{
		// RendererOfRRObject::render uses trilist -> slow, but no problem with added ambient map unwrap
		rr_gl::RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
		renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
		renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
		renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
		renderedChannels.MATERIAL_DIFFUSE_VCOLOR = uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
		renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
		renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setIndirectIllumination(solver->getIllumination(0)->getChannel(0)->vertexBuffer,solver->getIllumination(0)->getChannel(0)->pixelBuffer);
		if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
			rendererNonCaching->render(); // don't cache indirect illumination, it changes
		else
			rendererCaching->render(); // cache everything else, it's constant
	}

	// enable object space
	uberProgramSetup.OBJECT_SPACE = true;
	// when not rendering shadows, enable environment maps
	if(uberProgramSetup.LIGHT_DIRECT)
	{
		uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false; // stop using vertex illumination
		uberProgramSetup.LIGHT_INDIRECT_MAP = false; // stop using ambient map illumination
		uberProgramSetup.LIGHT_INDIRECT_ENV = true; // use indirect illumination from envmap
	}
	// move and rotate object freely, nothing is precomputed
	static float rotation = 0;
	if(!uberProgramSetup.LIGHT_DIRECT) rotation = (timeGetTime()%10000000)*0.07f;
	// render object1
	if(robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		robot->rotYZ = rr::RRVec2(rotation,0);
		robot->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV && environmentMapsRealtimeUpdate)
			robot->updateIllumination(solver);
		robot->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV && environmentMapsRealtimeUpdate)
			potato->updateIllumination(solver);
		potato->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	de::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	de::Texture* shadowmap = areaLight->getShadowMap(mapIndex);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	de::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
	renderScene(uberProgramSetup);
	shadowmap->renderingToEnd();
	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);
}


/////////////////////////////////////////////////////////////////////////////
//
// integration with Realtime Radiosity

class Solver : public rr_gl::RRRealtimeRadiosityGL
{
public:
	Solver() : RRRealtimeRadiosityGL("../../data/shaders/")
	{
	}
	virtual ~Solver()
	{
		// delete objects and illumination
		delete3dsFromRR(this);
	}
protected:
#ifdef AMBIENT_MAPS
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		// Decide how big ambient map you want for object. 
		// In this sample, we pick res proportional to number of triangles in object.
		// When seams appear, increase res.
		// Optimal res depends on quality of unwrap provided by object->getTriangleMapping.
		// This sample has bad unwrap -> high res map is needed.
		unsigned res = 16;
		while(res<2048 && res<20*sqrtf(object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		return createIlluminationPixelBuffer(res,res);
	}
#endif
	// skipped, material properties were already readen from .3ds and never change
	virtual void detectMaterials() {}
	// detects direct illumination irradiances on all faces in scene
	virtual bool detectDirectIllumination()
	{
		// renderer not yet ready, fail
		if(!::rendererCaching) return false;

		// shadowmap could be outdated, update it
		updateShadowmap(0);

		return RRRealtimeRadiosityGL::detectDirectIllumination();
	}
	// set shader so that direct light+shadows+emissivity are rendered, but no materials
	virtual void setupShader(unsigned objectNumber)
	{
			// render scene with forced 2d positions of all triangles
			de::UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_DIRECT_MAP = true;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.FORCE_2D_POSITION = true;
			if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap))
				error("Failed to compile or link GLSL program.\n",true);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window
	eye.update(0);
	light.update(0.3f);
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++) updateShadowmap(i);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
#ifdef AMBIENT_MAPS // here we say: render with ambient maps
	uberProgramSetup.LIGHT_INDIRECT_MAP = true;
	if(!solver->getIllumination(0)->getChannel(0)->pixelBuffer) // if ambient maps don't exist yet, create them
	{
		solver->calculate(rr::RRRealtimeRadiosity::FORCE_UPDATE_PIXEL_BUFFERS);
	}
#else // here we say: render with indirect illumination per-vertex
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
#endif
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	renderScene(uberProgramSetup);
	glutSwapBuffers();
}

void special(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 1; break;
		case GLUT_KEY_DOWN: speedBack = 1; break;
		case GLUT_KEY_LEFT: speedLeft = 1; break;
		case GLUT_KEY_RIGHT: speedRight = 1; break;
	}
}

void specialUp(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 0; break;
		case GLUT_KEY_DOWN: speedBack = 0; break;
		case GLUT_KEY_LEFT: speedLeft = 0; break;
		case GLUT_KEY_RIGHT: speedRight = 0; break;
	}
}

void keyboard(unsigned char c, int x, int y)
{
	const char* cubeSideNames[6] = {"x+","x-","y+","y-","z+","z-"};
	switch (c)
	{
#ifdef AMBIENT_MAPS
		case ' ':
			// Updates ambient maps (indirect illumination) in high quality.
			// If you need lightmaps (direct illum), call updateLightmap() instead of updateAmbientMap().
			// More lightmap examples soon.
			for(unsigned i=0;i<solver->getNumObjects();i++)
			{
				printf("Updating ambient map, object %d/%d, res %d*%d ...",i+1,solver->getNumObjects(),
					solver->getIllumination(i)->getChannel(0)->pixelBuffer->getWidth(),solver->getIllumination(i)->getChannel(0)->pixelBuffer->getHeight());
				rr::RRRealtimeRadiosity::UpdateLightmapParameters params;
				params.quality = 1000;
				params.insideObjectsTreshold = 0.1f;
				solver->updateLightmap(i,NULL,&params);
				printf(" done.\n");
			}
			// stop updating maps in realtime, stay with what we computed here
			ambientMapsRealtimeUpdate = false;
			modeMovingEye = true;
			break;
#endif

		case 's':
			// save current indirect illumination (static snapshot) to disk
			{
				static unsigned captureIndex = 0;
				char filename[100];
				// save all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
				{
					rr::RRIlluminationPixelBuffer* map = solver->getIllumination(objectIndex)->getChannel(0)->pixelBuffer;
					if(map)
					{
						sprintf(filename,"../../data/export/cap%02d_statobj%d.png",captureIndex,objectIndex);
						bool saved = map->save(filename);
						printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
					}
				}
				// save robot's specular environment map (dynamic object)
				// note: you can do the same with diffuseMap and with other dynamic objects
				if(robot->specularMap)
				{
					sprintf(filename,"../../data/export/cap%02d_dynobj%d_spec_%cs.png",captureIndex,'%');
					bool saved = robot->specularMap->save(filename,cubeSideNames);
					printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
				}
				captureIndex++;
				break;
			}

		case 'l':
			// load static snapshot of indirect illumination from disk, stop realtime updates
			{
				unsigned captureIndex = 0;
				char filename[100];
				// load all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
				{
					sprintf(filename,"../../data/export/cap%02d_statobj%d.png",captureIndex,objectIndex);
					rr::RRObjectIllumination::Channel* illum = solver->getIllumination(objectIndex)->getChannel(0);
					rr::RRIlluminationPixelBuffer* loaded = solver->loadIlluminationPixelBuffer(filename);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete illum->pixelBuffer;
						illum->pixelBuffer = loaded;
					}
				}
				// load robot's specular environment map (dynamic object)
				// note: you can do the same with diffuseMap and with other dynamic objects
				sprintf(filename,"../../data/export/cap%02d_robot_spec_%cs.png",captureIndex,'%');
				rr::RRIlluminationEnvironmentMap* loaded = solver->loadIlluminationEnvironmentMap(filename,cubeSideNames);
				printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
				if(loaded)
				{
					delete robot->specularMap;
					robot->specularMap = loaded;
				}

				modeMovingEye = true;
				// stop realtime illumination, so you can review loaded static one
				// (you can restart realtime by 'r')
				ambientMapsRealtimeUpdate = false;
				environmentMapsRealtimeUpdate = false;
				break;
			}

		case 'r':
			// realtime global illumination on (if it was turned off by load)
			ambientMapsRealtimeUpdate = true;
			environmentMapsRealtimeUpdate = true;
			modeMovingEye = false;
			break;

		case 27:
			exit(0);
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = (double) winWidth / (double) winHeight;
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getTexelBits();
	glPolygonOffset(4, 42 << (shadowDepthBits-16) );
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
}

void passive(int x, int y)
{
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		if(modeMovingEye)
		{
			eye.angle = eye.angle - 0.005*x;
			eye.height = eye.height + 0.15*y;
			CLAMP(eye.height,-13,13);
		}
		else
		{
			light.angle = light.angle - 0.005*x;
			light.height = light.height + 0.15*y;
			CLAMP(light.height,-13,13);
			solver->reportDirectIlluminationChange(true);
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	if(!winWidth) return; // can't work without window

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		CLAMP(seconds,0.001f,0.3f);
		de::Camera* cam = modeMovingEye?&eye:&light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam==&light) solver->reportDirectIlluminationChange(true);
		}
	}
	prev = now;

	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate(
#ifdef AMBIENT_MAPS
		ambientMapsRealtimeUpdate ? rr::RRRealtimeRadiosity::AUTO_UPDATE_PIXEL_BUFFERS : 0
#else
		rr::RRRealtimeRadiosity::AUTO_UPDATE_VERTEX_BUFFERS
#endif
		);

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("HelloRR"); // for windowed mode
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutSpecialUpFunc(specialUp);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);

	// init GLEW
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(0.9999); // prevents backprojection

	// init shaders
	uberProgram = new de::UberProgram("..\\..\\data\\shaders\\ubershader.vp", "..\\..\\data\\shaders\\ubershader.fp");
	// for correct soft shadows: maximal number of shadowmaps renderable in one pass is detected
	// for usual soft shadows, simply set shadowmapsPerPass=1
	unsigned shadowmapsPerPass = 1;
	if(!quadro)
	{
#ifdef AMBIENT_MAPS
		shadowmapsPerPass = de::UberProgramSetup::detectMaxShadowmaps(uberProgram,true);
#else
		shadowmapsPerPass = de::UberProgramSetup::detectMaxShadowmaps(uberProgram,false);
#endif
		if(!shadowmapsPerPass) error("",true);
	}
	
	// init textures
	lightDirectMap = de::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.png not found.\n",false);
	areaLight = new de::AreaLight(&light,shadowmapsPerPass,512);

	// init static .3ds scene
	if(!m3ds.Load("..\\..\\data\\3ds\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init dynamic objects
	if(!quadro)
	{
		de::UberProgramSetup material;
		material.MATERIAL_SPECULAR = true;
		robot = DynamicObject::create("..\\..\\data\\3ds\\characters\\I_Robot_female.3ds",0.3f,material,16);
		material.MATERIAL_DIFFUSE = true;
		material.MATERIAL_DIFFUSE_MAP = true;
		material.MATERIAL_SPECULAR_MAP = true;
		potato = DynamicObject::create("..\\..\\data\\3ds\\characters\\potato\\potato01.3ds",0.004f,material,16);
	}

	// init realtime radiosity solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	insert3dsToRR(&m3ds,solver,NULL);
	solver->calculate();
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new rr_gl::RendererOfRRObject(solver->getMultiObjectCustom(),solver->getScene(),solver->getScaler(),true);
	rendererCaching = rendererNonCaching->createDisplayList();

	glutMainLoop();
	return 0;
}
