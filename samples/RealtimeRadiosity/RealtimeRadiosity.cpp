// --------------------------------------------------------------------------
// RealtimeRadiosity sample
//
// Shows realtime GI + area light with penumbra shadows + projected video
// ADDED TO 3RD PARTY 3DS VIEWER.
//
// This sample started by taking 3rd party 3ds viewer.
// Then we added Lightsprint's advanced lighting, with only small changes
// to 3rd party code. It still uses 3rd party scene loader, data structures
// and OpenGL draw calls, we only modified it to use Lightsprint shaders.
// (Ok, we also replaced 3rd party Material class with our RRMaterial later,
//  just to get rid of conversions and to further simplify code.)
//
// If you wish to add only penumbra shadows to your own OpenGL based engine,
// see simpler PenumbraShadows sample.
//
// However, if we ditch 3rd party code and use Lightsprint SDK functions instead, we get
// - 5x shorter sample source code
// - higher quality (translucency, bump maps etc)
// - higher FPS
// - see Lightmaps or RealtimeLights samples
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRSolver.h"
#include "../src/LightsprintIO/Import3DS/Model_3DS.h"
#include "../src/LightsprintIO/Import3DS/RRObject3DS.h"
#include "DynamicObject.h"
#include "Lightsprint/IO/IO.h"
#ifdef __APPLE__
	#include <GLUT/glut.h>
	#include <ApplicationServices/ApplicationServices.h>
#else
	#include <GL/glut.h>
#endif


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported GPUs:\n - those that support OpenGL 2.0 or OpenGL ES 2.0 or higher\n - i.e. nearly all from ATI/AMD since 2002, NVIDIA since 2003, Intel since 2006");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

Model_3DS                  m3ds;
rr::RRCamera               eye(rr::RRVec3(-1.416f,1.741f,-3.646f), rr::RRVec3(9.09f,0.05f,0),1.3f,70,0.3f,60);
rr_gl::RealtimeLight*      realtimeLight = NULL;
rr::RRBuffer*              environmentMap = NULL;
rr_gl::TextureRenderer*    textureRenderer = NULL;
rr_gl::UberProgram*        uberProgram = NULL;
rr_gl::RRSolverGL*  solver = NULL;
DynamicObject*             robot = NULL;
DynamicObject*             potato = NULL;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = false;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene via small custom 3ds renderer

// callback that feeds 3ds renderer with our vertex illumination in RGBF format
const float* lockVertexIllum(void* solver,unsigned object)
{
	rr::RRBuffer* vertexBuffer = ((rr::RRSolver*)solver)->getStaticObjects()[object]->illumination.getLayer(LAYER_AMBIENT_MAP);
	return vertexBuffer && (vertexBuffer->getFormat()==rr::BF_RGBF) ? (float*)(vertexBuffer->lock(rr::BL_READ)) : NULL;
}

// callback that cleans vertex illumination
void unlockVertexIllum(void* solver,unsigned object)
{
	rr::RRBuffer* vertexBuffer = ((rr::RRSolver*)solver)->getStaticObjects()[object]->illumination.getLayer(LAYER_AMBIENT_MAP);
	if (vertexBuffer) vertexBuffer->unlock();
}

// render scene using very simple custom 3ds renderer, see source code in m3ds.Draw()
void renderScene(const rr::RRCamera& camera, rr_gl::UberProgramSetup uberProgramSetup)
{
	// render skybox
	if (uberProgramSetup.LIGHT_DIRECT && environmentMap)
		textureRenderer->renderEnvironment(camera,rr_gl::getTexture(environmentMap),0,NULL,0,0,NULL,1,false);

	// render static scene
	rr::RRVec4 brightness(2);// render static scene
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,&camera,realtimeLight,0,1,uberProgramSetup.POSTPROCESS_BRIGHTNESS?&brightness:NULL,1,NULL);
	if (!program)
		error("Failed to compile or link GLSL program.\n",true);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	if (uberProgramSetup.MATERIAL_DIFFUSE_MAP)
		program->sendTexture("materialDiffuseMap",NULL); // calls glActiveTexture(), Draw will bind textures
	m3ds.Draw(solver,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,uberProgramSetup.LIGHT_INDIRECT_VCOLOR?lockVertexIllum:NULL,unlockVertexIllum);

	// render dynamic objects
	// enable object space
	uberProgramSetup.OBJECT_SPACE = true;
	// when not rendering shadows, enable environment maps
	if (uberProgramSetup.LIGHT_DIRECT)
	{
		uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = false; // stop using vertex illumination
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true; // use indirect illumination from envmap
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true; // use indirect illumination from envmap
	}
	// move and rotate object freely, nothing is precomputed
	float rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*70.f;
	if (potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			solver->RRSolver::updateEnvironmentMap(potato->illumination,LAYER_ENVIRONMENT,UINT_MAX,LAYER_AMBIENT_MAP);
		potato->render(uberProgram,uberProgramSetup,camera,&solver->realtimeLights,0,NULL,1);
	}
	if (robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		//robot->rotYZ = rr::RRVec2(rotation,0); // rotate
		robot->rotYZ = rr::RRVec2(55,0); robot->animationTime = rotation*0.01f; // wave
		robot->updatePosition();
		if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			solver->RRSolver::updateEnvironmentMap(robot->illumination,LAYER_ENVIRONMENT,UINT_MAX,LAYER_AMBIENT_MAP);
		robot->render(uberProgram,uberProgramSetup,camera,&solver->realtimeLights,0,NULL,1);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// GI solver
//
// Shadowmaps are updated by solver, therefore to obtain robot shadows, we must
// either send robot to solver                         <-- this is shown in RealtimeLights, Lightmaps etc
// or overload solver->renderScene() to render robot   <-- this is shown here

class Solver : public rr_gl::RRSolverGL
{
public:
	Solver() : RRSolverGL("../../data/shaders/","../../data/maps/")
	{
	}
	// Called from RRSolverGL to update shadowmaps.
	// If we delete this function, sample still works, but dynamic objects have no shadows.
	// It's because default renderScene() implemetation renders only objects we sent to solver.
	// In this sample, dynamic objects are in third party format, and third party code renders them,
	// so we can't send them to solver. The only thing solver needs is ability to render them on request.
	// This function satisfies it.
	/*virtual void renderScene(const rr_gl::RenderParameters& _renderParameters)
	{
		// disable all material properties not supported by custom 3ds renderer
		rr_gl::UberProgramSetup uberProgramSetup = _renderParameters.uberProgramSetup;
		//uberProgramSetup.MATERIAL_DIFFUSE
		uberProgramSetup.MATERIAL_DIFFUSE_X2 = false;
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP
		//uberProgramSetup.MATERIAL_SPECULAR = false;
		//uberProgramSetup.MATERIAL_SPECULAR_CONST = false;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		uberProgramSetup.MATERIAL_EMISSIVE_CONST = false;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = false;
		uberProgramSetup.MATERIAL_BUMP_MAP = false;
		uberProgramSetup.MATERIAL_BUMP_TYPE_HEIGHT = false;
		uberProgramSetup.MATERIAL_CULLING = false;

		// call custom 3ds renderer
		::renderScene(*_renderParameters.camera,uberProgramSetup);
	}*/
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// this would print diagnostic messages from solver internals
	//solver->checkConsistency();

	// update shadowmaps, lightmaps
	solver->reportDirectIlluminationChange(0,true,false,false);
	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	rr::RRSolver::CalculateParameters params;
	params.lightMultiplier = 2;
	solver->calculate(&params);
	static unsigned solutionVersion = 0;
	if (solver->getSolutionVersion()!=solutionVersion)
	{
		solutionVersion = solver->getSolutionVersion();
		solver->updateLightmaps(LAYER_AMBIENT_MAP,-1,-1,NULL,NULL);
	}

	// render
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = realtimeLight->getNumShadowmaps();
	uberProgramSetup.SHADOW_SAMPLES = 4; // for 3ds draw, won't be reset by MultiPass
	uberProgramSetup.SHADOW_PENUMBRA = true;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = realtimeLight->getProjectedTexture()?true:false;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	glClear(GL_DEPTH_BUFFER_BIT);
	renderScene(eye,uberProgramSetup);

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
	switch (c)
	{
		case 27:
			// immediate exit without freeing memory, leaks may be reported
			// see e.g. RealtimeLights sample for freeing memory before exit
			exit(0);
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.setAspect( winWidth/(float)winHeight );
}

void mouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
}

void passive(int x, int y)
{
	if (!winWidth || !winHeight) return;
	RR_LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;

	if (x || y)
	{
#if defined(LINUX) || defined(linux)
		const float mouseSensitivity = 0.0002f;
#else
		const float mouseSensitivity = 0.005f;
#endif
		rr::RRCamera& cam = modeMovingEye ? eye : *realtimeLight->getCamera();
		rr::RRVec3 yawPitchRollRad = cam.getYawPitchRollRad()-rr::RRVec3(x,y,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		cam.setYawPitchRollRad(yawPitchRollRad);
		if (!modeMovingEye)
			solver->reportDirectIlluminationChange(0,true,true,false);
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	// smooth keyboard movement
	static rr::RRTime time;
	float seconds = time.secondsSinceLastQuery();
	RR_CLAMP(seconds,0.001f,0.3f);
	rr::RRCamera* cam = modeMovingEye?&eye:realtimeLight->getCamera();
	if (speedForward || speedBack || speedRight || speedLeft)
	{
		cam->setPosition(cam->getPosition()
			+ cam->getDirection() * ((speedForward-speedBack)*seconds)
			+ cam->getRight() * ((speedRight-speedLeft)*seconds)
			);
		if (cam!=&eye) solver->reportDirectIlluminationChange(0,true,true,false);
	}

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char** argv)
{
	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::createPrintfReporter();
	//rr::RRReporter::setFilter(true,3,true); // log more
	//rr_gl::Program::logMessages(true); // log also results of shader compiler

	rr_io::registerLoaders(argc,argv);

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Realtime Radiosity"); // for windowed mode
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutSpecialUpFunc(specialUp);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);
#ifdef __APPLE__
//	OSX kills events ruthlessly
//	see http://stackoverflow.com/questions/728049/glutpassivemotionfunc-and-glutwarpmousepointer
	CGSetLocalEventsSuppressionInterval(0.0);
#endif

	// init GL
	const char* err = rr_gl::initializeGL();
	if (err)
		error(err,true);

	// init shaders
	uberProgram = rr_gl::UberProgram::create("../../data/shaders/ubershader.vs", "../../data/shaders/ubershader.fs");
	textureRenderer = new rr_gl::TextureRenderer("../../data/shaders/");
	// for soft shadows, keep shadowmapsPerPass=1
	unsigned shadowmapsPerPass = 1;
	// for penumbra shadows: increase it to maximal number of shadowmaps renderable in one pass
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for detectMaxShadowmaps, won't be reset by MultiPass
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	if (!shadowmapsPerPass) error("",true);
	
	// uncomment to load and render and calculate with skybox (not visible from closed scene)
	//environmentMap = rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg");

	// init static .3ds scene
	if (!m3ds.Load("../../data/scenes/koupelna/koupelna4.3DS",NULL,0.03f))
		error("",false);

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	material.ANIMATION_WAVE = true;
	robot = DynamicObject::create("../../data/objects/I_Robot_female.3ds",0.3f,material,16);
	material.ANIMATION_WAVE = false;
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("../../data/objects/potato/potato01.3ds",0.004f,material,16);

	// init realtime radiosity solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setColorSpace(rr::RRColorSpace::create_sRGB());
	solver->setStaticObjects(*adaptObjectsFrom3DS(&m3ds),NULL);
	solver->setEnvironment(environmentMap);
	if (!solver->getMultiObject())
		error("No objects in scene.",false);

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
		if (solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices())
			solver->getStaticObjects()[i]->illumination.getLayer(LAYER_AMBIENT_MAP) =
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(0.4f,0.2f,1),RR_DEG2RAD(30),0.1f);
	// project texture
	//rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/maps/spot0.png");
	// Project video. You can do this in any other sample or context, simply use video instead of image, then call play().
	rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/video/Televisi1960.avi");
	rrlight->rtShadowmapSize = 512;
	rr::RRLights rrlights;
	rrlights.push_back(rrlight);
	solver->setLights(rrlights);
	realtimeLight = solver->realtimeLights[0];
	realtimeLight->numInstancesInArea = shadowmapsPerPass;
	realtimeLight->getCamera()->setNear(0.5f); // adjusts shadowmapping near plane

	// Enable Fireball - faster, higher quality, smaller realtime global illumination solver for games.
	// You can safely skip it to stay with fully dynamic solver that doesn't need any precalculations.
	solver->loadFireball("",true) || solver->buildFireball(5000,"");

	// start playing video
	if (rrlight->rtProjectedTexture)
		rrlight->rtProjectedTexture->play();

	rr::RRReporter::report(rr::INF2,
		"\n"
		"This sample is all about adding lighting to 3rd party codebase.\n"
		"We can get better results without 3rd party code, see Lightmaps sample.\n"
		"For complete description, see comment at the beginning of sample source code.\n"
		"\n"
		"Controls:\n"
		"  mouse = look around\n"
		"  arrows = move around\n"
		"  left button = switch between camera and light\n"
		"\n");

	glutMainLoop();
	return 0;
}
