// --------------------------------------------------------------------------
// Penumbra Shadows sample
//
// Shows area light with penumbra shadows
// ADDED TO 3RD PARTY 3DS VIEWER.
//
// There is no GI, indirect illumination is approximated by constant ambient.
// Dynamic objects reflect skybox (visible behind walls).
//
// This sample started by taking 3rd party 3ds viewer.
// Then we added Lightsprint's penumbra shadows, with only small changes
// to 3rd party code. It still uses 3rd party scene loader, data structures
// and OpenGL draw calls, we only modified it to use Lightsprint shaders.
// (Ok, we also replaced 3rd party Material class with our RRMaterial later, just to get rid of conversions.)
//
// If you wish to add realtime GI to your own OpenGL based engine,
// see more advanced RealtimeRadiosity samples.
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
#include <cstdlib>
#include <ctime>
#include "Lightsprint/GL/FBO.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDebug.h"
#include "DynamicObject.h"
#include "Lightsprint/IO/IO.h"
#ifdef __EMSCRIPTEN__
	#include <emscripten.h> // emscripten_set_main_loop
#endif
#include <GLFW/glfw3.h>
#pragma comment(lib,"glfw3.lib")


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
// variables accessed by GLFW callbacks

Model_3DS                  m3ds;
rr::RRCamera               eye(rr::RRVec3(-1.416f,1.741f,-3.646f), rr::RRVec3(9.09f,0.05f,0),1.3f,70,0.3f,60);
rr_gl::RealtimeLight*      realtimeLight = nullptr;
rr::RRBuffer*              environmentMap = nullptr;
rr_gl::TextureRenderer*    textureRenderer = nullptr;
rr_gl::UberProgram*        uberProgram = nullptr;
DynamicObject*             robot = nullptr;
DynamicObject*             potato = nullptr;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = false;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(const rr::RRCamera& camera, rr_gl::UberProgramSetup uberProgramSetup)
{
	// render skybox
	if (uberProgramSetup.LIGHT_DIRECT && environmentMap)
		textureRenderer->renderEnvironment(camera,rr_gl::getTexture(environmentMap),0,nullptr,0,0,nullptr,1,false);

	// render static scene
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,&camera,realtimeLight,0,1,nullptr,1,nullptr);
	if (!program)
		error("Failed to compile or link GLSL program.\n",true);
	rr_gl::glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	if (uberProgramSetup.MATERIAL_DIFFUSE_MAP)
		program->sendTexture("materialDiffuseMap",nullptr); // activate unit, Draw will bind textures
	m3ds.Draw(nullptr,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,nullptr,nullptr);

	// render dynamic objects
	// enable object space
	uberProgramSetup.OBJECT_SPACE = true;
	if (uberProgramSetup.SHADOW_MAPS) uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
	// move and rotate object freely, nothing is precomputed
	static float rotation = 0;
	if (!uberProgramSetup.LIGHT_DIRECT) rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*70;
	// render objects
	if (potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_SPECULAR = true;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = true;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
		}
		potato->render(uberProgram,uberProgramSetup,camera,realtimeLight,0,environmentMap,rotation/2);
	}
	if (robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_DIFFUSE = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.MATERIAL_SPECULAR = true;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
		}
		robot->render(uberProgram,uberProgramSetup,camera,realtimeLight,0,environmentMap,rotation);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	rr::RRCamera lightInstance;
	realtimeLight->getShadowmapCamera(mapIndex,lightInstance);
	rr_gl::glColorMask(0,0,0,0);
	rr_gl::Texture* shadowmap = realtimeLight->getShadowmap(mapIndex);
	rr_gl::glViewport(0, 0, shadowmap->getBuffer()->getWidth(), shadowmap->getBuffer()->getHeight());
	rr_gl::FBO oldFBOState = rr_gl::FBO::getState();
	rr_gl::FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowmap,oldFBOState);
	glClear(GL_DEPTH_BUFFER_BIT);
	rr_gl::glEnable(GL_POLYGON_OFFSET_FILL);
	rr_gl::UberProgramSetup uberProgramSetup; // default constructor sets nearly all off, perfect for shadowmap
	uberProgramSetup.MATERIAL_CULLING = 0;
	renderScene(lightInstance,uberProgramSetup);
	oldFBOState.restore();
	rr_gl::glDisable(GL_POLYGON_OFFSET_FILL);
	rr_gl::glViewport(0, 0, winWidth, winHeight);
	rr_gl::glColorMask(1,1,1,1);
}


/////////////////////////////////////////////////////////////////////////////
//
// GLFW callbacks

void error_callback(int error, const char* description)
{
	rr::RRReporter::report(rr::ERRO, "%d %s\n", error, description);
}

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// update shadowmaps
	unsigned numInstances = realtimeLight->getNumShadowmaps();
	for (unsigned i=0;i<numInstances;i++) updateShadowmap(i);

	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for 3ds draw, won't be reset by MultiPass
	uberProgramSetup.SHADOW_PENUMBRA = true;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	renderScene(eye,uberProgramSetup);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	int speed = (action == GLFW_PRESS || action == GLFW_REPEAT) ? 1 : 0;
	switch (key)
	{
		case GLFW_KEY_UP: speedForward = speed; break;
		case GLFW_KEY_DOWN: speedBack = speed; break;
		case GLFW_KEY_LEFT: speedLeft = speed; break;
		case GLFW_KEY_RIGHT: speedRight = speed; break;
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
	}
}

void reshape_callback(GLFWwindow* window, int w, int h)
{
	winWidth = w;
	winHeight = h;
	rr_gl::glViewport(0, 0, w, h);
	eye.setAspect( winWidth/(float)winHeight );
	// the most simple bias setting sufficient for this sample
	// other samples use Lightsprint renderer with more robust bias setting
	glPolygonOffset(4,10000);
}

void mousebutton_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		modeMovingEye = !modeMovingEye;
}

void mouseposition_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (!winWidth || !winHeight) return;
	RR_LIMITED_TIMES(1, \
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); \
		glfwSetCursorPos(window, 0, 0); \
		return; );
	if (xpos || ypos)
	{
		const float mouseSensitivity = 0.005f;
		rr::RRCamera& cam = modeMovingEye ? eye : *realtimeLight->getCamera();
		rr::RRVec3 yawPitchRollRad = cam.getYawPitchRollRad()-rr::RRVec3(xpos,ypos,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		cam.setYawPitchRollRad(yawPitchRollRad);
		glfwSetCursorPos(window, 0, 0);
	}
}

void idle()
{
	if (!winWidth) return; // can't work without window

	// smooth keyboard movement
	static rr::RRTime time;
	float seconds = time.secondsSinceLastQuery();
	RR_CLAMP(seconds,0.001f,0.3f);
	rr::RRCamera* cam = modeMovingEye?&eye:realtimeLight->getCamera();
	cam->setPosition(cam->getPosition()
		+ cam->getDirection() * ((speedForward-speedBack)*seconds)
		+ cam->getRight() * ((speedRight-speedLeft)*seconds)
		);
}

void main_loop()
{
	idle();
	display();
	glfwPollEvents();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char** argv)
{
	// log messages to console
	rr::RRReporter::createPrintfReporter();

	rr_io::registerIO(argc,argv);

	// init GLFW
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		error("glfwInit() failed.", true);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Lightsprint Penumbra Shadows", NULL, NULL);
	if (!window)
		error("glfwCreateWindow() failed.", true);
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &winWidth, &winHeight);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mousebutton_callback);
#ifndef __EMSCRIPTEN__
	// glfw-emscripten did not yet implement cursor locking
	glfwSetCursorPosCallback(window, mouseposition_callback);
#endif
	glfwSetFramebufferSizeCallback(window, reshape_callback);

	// init GL
	const char* err = rr_gl::initializeGL(true);
	if (err)
		error(err,true);

	// init shaders
	uberProgram = rr_gl::UberProgram::create("../../data/shaders/ubershader.vs", "../../data/shaders/ubershader.fs");
	textureRenderer = new rr_gl::TextureRenderer("../../data/shaders/");
	// for correct soft shadows: maximal number of shadowmaps renderable in one pass is detected
	// set shadowmapsPerPass=1 for standard shadows
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for detectMaxShadowmaps, won't be reset by MultiPass
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	unsigned shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	if (!shadowmapsPerPass) error("",true);

	// init textures
	environmentMap = rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg");

	// init static .3ds scene
	if (!m3ds.Load("../../data/scenes/koupelna/koupelna4.3DS",nullptr,0.03f))
		error("",false);

	// init dynamic objects
	robot = DynamicObject::create("../../data/objects/I_Robot_female.3ds",0.3f);
	potato = DynamicObject::create("../../data/objects/potato/potato01.3ds",0.004f);

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f);
	rrlight->projectedTexture = rr::RRBuffer::load("../../data/maps/spot0.png");
	rrlight->rtShadowmapSize = 512;
	realtimeLight = new rr_gl::RealtimeLight(*rrlight);
	realtimeLight->numInstancesInArea = shadowmapsPerPass;

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(main_loop, 0, true);
#else
	while (!glfwWindowShouldClose(window))
	{
		main_loop();
		glfwSwapBuffers(window);
	}
#endif

	return 0;
}
