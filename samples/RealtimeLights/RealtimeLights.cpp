// --------------------------------------------------------------------------
// RealtimeLights sample
//
// Most suitable for: scene viewers, designers, editors.
//
// This is a viewer of Collada .DAE scenes with
// - realtime GI
// - all lights from file, no hard limit on number of lights
// - no precalculations
//
// Light types supported: point, spot (todo: directional)
// GPUs supported: GeForce 5000+, Radeon X1300+
//
// Controls:
//  1..9 = switch to n-th light
//  arrows = move camera or light
//  mouse = rotate camera or light
//  left button = switch between camera and light
//  + - = change brightness
//  * / = change contrast
//	wheel = change camera FOV
//  space = randomly move dynamic objects
//
// Hint: hold space to see dynamic object occluding light
// Hint: press 1 or 2 and left/right arrows to move lights
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#include "FCollada.h" // must be included before LightsprintGL because of fcollada SAFE_DELETE macro
#include "FCDocument/FCDocument.h"
#include "../../samples/ImportCollada/RRObjectCollada.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "../RealtimeRadiosity/DynamicObject.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon X1300-X1950, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

rr_gl::Camera              eye(-1.856,1.440,2.097,2.404,0.000,0.020,1.3,110.0,0.1,100.0);
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
rr_gl::Texture*            lightDirectMap = NULL;
rr_gl::UberProgram*        uberProgram = NULL;
class Solver*              solver = NULL;
rr_gl::RendererOfScene*    rendererOfScene = NULL;
DynamicObject*             robot = NULL;
DynamicObject*             potato = NULL;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = true;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
rr::RRVec4                 brightness(1);
float                      gamma = 1;
float                      rotation = 0;

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRVector<rr_gl::RealtimeLight*>* lights);

/////////////////////////////////////////////////////////////////////////////
//
// integration with RRDynamicSolver

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	rr::RRVector<rr_gl::RealtimeLight*> realtimeLights;

	Solver() : RRDynamicSolverGL("../../data/shaders/")
	{
		detectedDirectSum = NULL;
		detectedNumTriangles = 0;
	}
	virtual void setLights(const rr::RRLights& _lights)
	{
		// create realtime lights
		RRDynamicSolverGL::setLights(_lights);
		for(unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
		realtimeLights.clear();
		for(unsigned i=0;i<_lights.size();i++) realtimeLights.push_back(new rr_gl::RealtimeLight(*_lights[i]));
		// reset detected direct lighting
		if(detectedDirectSum) memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
	}
	void updateDirtyLights()
	{
		// alloc space for detected direct illum
		unsigned updated = 0;
		unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		if(numTriangles!=detectedNumTriangles)
		{
			delete[] detectedDirectSum;
			detectedDirectSum = new unsigned[detectedNumTriangles=numTriangles];
			memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
		}

		// update shadowmaps and smallMapsCPU
		for(unsigned i=0;i<realtimeLights.size();i++)
		{
			rr_gl::RealtimeLight* light = realtimeLights[i];
			if(!light || !uberProgram)
			{
				RR_ASSERT(0);
				continue;
			}
			if(numTriangles!=light->numTriangles)
			{
				delete[] light->smallMapCPU;
				light->smallMapCPU = new unsigned[light->numTriangles=numTriangles];
				light->dirty = 1;
			}
			if(!light->dirty) continue;
			light->dirty = 0;
			// update shadowmap[s]
			{
				glColorMask(0,0,0,0);
				glEnable(GL_POLYGON_OFFSET_FILL);
				rr_gl::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
				for(unsigned i=0;i<light->getNumInstances();i++)
				{
					rr_gl::Camera* lightInstance = light->getInstance(i);
					lightInstance->setupForRender();
					delete lightInstance;
					rr_gl::Texture* shadowmap = light->getShadowMap(i);
					glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
					shadowmap->renderingToBegin();
					glClear(GL_DEPTH_BUFFER_BIT);
					renderScene(uberProgramSetup,NULL);
				}
				glDisable(GL_POLYGON_OFFSET_FILL);
				glColorMask(1,1,1,1);
				if(light->getNumInstances())
					light->getShadowMap(0)->renderingToEnd();
				glViewport(0, 0, winWidth, winHeight);
			}
			// update smallmap
			setupShaderLight = light;
			memcpy(light->smallMapCPU,RRDynamicSolverGL::detectDirectIllumination(),4*numTriangles);
			updated++;
		}

		// sum smallMapsCPU into detectedDirectSum
		if(updated)
		{
			unsigned numLights = realtimeLights.size();
			#pragma omp parallel for
			for(int b=0;b<(int)numTriangles*4;b++)
			{
				unsigned sum = 0;
				for(unsigned l=0;l<numLights;l++)
					sum += ((unsigned char*)realtimeLights[l]->smallMapCPU)[b];
				((unsigned char*)detectedDirectSum)[b] = CLAMPED(sum,0,255);
			}
		}
	}

protected:
	unsigned* detectedDirectSum;
	unsigned detectedNumTriangles;
	// skipped, material properties were already readen from .dae and never change
	virtual void detectMaterials() {}
	// detects direct illumination irradiances on all faces in scene
	virtual unsigned* detectDirectIllumination()
	{
		if(!winWidth) return NULL;
		updateDirtyLights();
		return detectedDirectSum;
	}

	rr_gl::RealtimeLight* setupShaderLight;
	virtual void setupShader(unsigned objectNumber)
	{
		rr_gl::UberProgramSetup uberProgramSetup;
		uberProgramSetup.SHADOW_MAPS = setupShaderLight->getNumInstances(); //!!! radeons 9500-X1250 can't run 6 in one pass
		uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_COLOR = setupShaderLight->origin && setupShaderLight->origin->color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = setupShaderLight->areaType!=rr_gl::RealtimeLight::POINT;
		uberProgramSetup.LIGHT_DISTANCE_PHYSICAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DISTANCE_POLYNOMIAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DISTANCE_EXPONENTIAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.FORCE_2D_POSITION = true;
		if(!uberProgramSetup.useProgram(uberProgram,setupShaderLight,0,lightDirectMap,NULL,1))
		{
			error("Failed to compile or link GLSL program.\n",true);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRVector<rr_gl::RealtimeLight*>* lights)
{
	// render static scene
	rendererOfScene->setParams(uberProgramSetup,lights,lightDirectMap);
	rendererOfScene->useOptimizedScene();
	rendererOfScene->setBrightnessGamma(&brightness,gamma);
	rendererOfScene->render();

	// render dynamic objects
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
	// render objects
	if(robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		robot->rotYZ = rr::RRVec2(rotation,0);
		robot->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			solver->updateEnvironmentMap(robot->illumination);
		robot->render(uberProgram,uberProgramSetup,lights,0,lightDirectMap,eye,&brightness,gamma);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(0.4f*sin(rotation*0.05f)+1,1.0f,0.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			solver->updateEnvironmentMap(potato->illumination);
		potato->render(uberProgram,uberProgramSetup,lights,0,lightDirectMap,eye,&brightness,gamma);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

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
	switch (c)
	{
		case '+':
			brightness *= 1.2;
			break;
		case '-':
			brightness /= 1.2;
			break;
		case '*':
			gamma *= 1.2;
			break;
		case '/':
			gamma /= 1.2;
			break;
		case ' ':
			//printf("camera(%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f);\n",eye.pos[0],eye.pos[1],eye.pos[2],fmodf(eye.angle+100*3.14159265f,2*3.14159265f),eye.leanAngle,eye.angleX,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			rotation = (clock()%10000000)*0.07f;
			solver->reportDirectIlluminationChange(false);
			for(unsigned i=0;i<solver->realtimeLights.size();i++)
				solver->realtimeLights[i]->dirty = true;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			selectedLightIndex = MIN(c-'1',(int)solver->getLights().size()-1);
			if(solver->realtimeLights.size()) modeMovingEye = false;
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
	rr_gl::Texture* texture = rr_gl::Texture::createShadowmap(64,64);
	GLint shadowDepthBits = texture->getTexelBits();
	delete texture;
	glPolygonOffset(1, 12 << (shadowDepthBits-16) );
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && solver->realtimeLights.size())
		modeMovingEye = !modeMovingEye;
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView<130) eye.fieldOfView+=10;
	}
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
			eye.angle -= 0.005*x;
			eye.angleX -= 0.005*y;
			CLAMP(eye.angleX,-M_PI*0.49f,M_PI*0.49f);
		}
		else
		{
			rr_gl::Camera* light = solver->realtimeLights[selectedLightIndex]->getParent();
			light->angle -= 0.005*x;
			light->angleX -= 0.005*y;
			CLAMP(light->angleX,-M_PI*0.49f,M_PI*0.49f);
			solver->reportDirectIlluminationChange(true);
			solver->realtimeLights[selectedLightIndex]->dirty = true;
			// changes also position a bit, together with rotation
			light->pos += light->dir*0.3f;
			light->update();
			light->pos -= light->dir*0.3f;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	eye.update();

	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	renderScene(uberProgramSetup,&solver->realtimeLights);

	glutSwapBuffers();
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
		rr_gl::Camera* cam = modeMovingEye?&eye:solver->realtimeLights[selectedLightIndex]->getParent();
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam!=&eye) 
			{
				solver->reportDirectIlluminationChange(true);
				solver->realtimeLights[selectedLightIndex]->dirty = true;
				if(speedForward) cam->moveForward(speedForward*seconds);
			}
		}
	}
	prev = now;

	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate();

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
	//rr::RRReporter::setFilter(true,3,true);

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint RealtimeLights"); // for windowed mode
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
	uberProgram = rr_gl::UberProgram::create("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");
	
	// init textures
	lightDirectMap = rr_gl::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.png not found.\n",false);

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("..\\..\\data\\objects\\I_Robot_female.3ds",0.3f,material,16,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("..\\..\\data\\objects\\potato\\potato01.3ds",0.004f,material,16,16);

	// init static scene and solver and lights
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("..\\..\\data\\scenes\\koupelna\\koupelna4.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setStaticObjects(*adaptObjectsFromFCollada(collada),NULL);
	solver->setLights(*adaptLightsFromFCollada(collada));
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(solver->loadIlluminationEnvironmentMap("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true));
	rendererOfScene = new rr_gl::RendererOfScene(solver,"../../data/shaders/");
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);
	solver->calculate();

	glutMainLoop();
	return 0;
}
