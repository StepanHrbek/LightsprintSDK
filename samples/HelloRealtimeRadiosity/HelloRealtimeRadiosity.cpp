// --------------------------------------------------------------------------
// Hello Realtime Radiosity sample
//
// Use of RealtimeRadiosity is demonstrated on .3ds scene viewer.
// You should be familiar with GLUT and OpenGL.
//
// This is HelloDemoEngine with Lightsprint engine integrated,
// see how the same scene looks sexy with global illumination.
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Soft shadow quality is reduced due to bug in ATI drivers.
// Improve it on NVIDIA by deleting lines with NVIDIA in comment.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "RRRealtimeRadiosity.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/Timer.h"
#include "3ds2rr.h"
#include "DynamicObject.h"
#include "RendererOfRRObject.h"
#include "RRIlluminationPixelBufferInOpenGL.h"

//#define AMBIENT_MAPS
// Turns on ambient maps.
// They are generated and rendered in realtime,
// every frame new set of maps for all objects in scene.
// You can turn this demo into ambient map precalculator by saving maps to disk.
// It is possible to improve ambient map quality 
// 1) by providing unwrap for meshes (see getTriangleMapping in 3ds2rr.cpp).
//    If you see pink pixels in ambient maps (e.g. on walls),
//    it's result of bad unwrap with too small distance between standalone triangles.
//    You can limit these artifacts by increasing ambient map resolution, see newPixelBuffer.
// 2) by calling calculate() multiple times before
//    final calculate(UPDATE_PIXEL_BUFFERS) and save of ambient maps.

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
de::Camera              eye = {{-1.416,1.741,-3.646},12.230,0.050,1.3,70.0,0.3,60.0};
de::Camera              light = {{-1.802,0.715,0.850},0.635,-5.800,1.0,70.0,1.0,20.0};
de::AreaLight*          areaLight = NULL;
de::Texture*            lightDirectMap = NULL;
de::UberProgram*        uberProgram = NULL;
RendererOfRRObject*     rendererNonCaching = NULL;
de::RendererWithCache*  rendererCaching = NULL;
rr::RRRealtimeRadiosity*solver = NULL;
DynamicObject*          robot = NULL;
DynamicObject*          potato = NULL;
int                     winWidth = 0;
int                     winHeight = 0;
bool                    modeMovingEye = false;
float                   speedForward = 0;
float                   speedBack = 0;
float                   speedRight = 0;
float                   speedLeft = 0;


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
	// m3ds.Draw uses indexed trilist incompatible with ambient map uv channel, doesn't render properly with ambient maps
	//  could be fixed with better uv or simple geometry shader
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(solver,uberProgramSetup.LIGHT_INDIRECT_COLOR?lockVertexIllum:NULL,unlockVertexIllum);
	}
	else
#endif
	{
		// RendererOfRRObject::render uses trilist -> slow, but no problem with added ambient map unwrap
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
		renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
		renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
		renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
		renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
		renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
		renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
		rendererNonCaching->setRenderedChannels(renderedChannels);
		if(uberProgramSetup.LIGHT_INDIRECT_COLOR)
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
		uberProgramSetup.LIGHT_INDIRECT_COLOR = false; // stop using vertex illumination
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
		robot->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,solver,eye,rotation);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,solver,eye,rotation/2);
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

// generates uv coords for direct illumination capture
class CaptureUv : public VertexDataGenerator
{
public:
	// generates uv coords for any triangle so that firstCapturedTriangle
	//  occupies 'top left' corner in texture, others follow below and
	//  then right, so that whole texture is filled
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) // vertexIndex=0..2
	{
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%xmax)+((vertexIndex==2)?1:0)-xmax*0.5f+0.1f)/(xmax*0.5f);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/xmax)+((vertexIndex==0)?1:0)-ymax*0.5f+0.1f)/(ymax*0.5f);
	}
	virtual unsigned getHash()
	{
		return firstCapturedTriangle+(xmax<<8)+(ymax<<16);
	}
	unsigned firstCapturedTriangle; // index of first captured triangle (for case that there are too many triangles for one texture)
	unsigned xmax, ymax; // max number of triangles in texture
};

class Solver : public rr::RRRealtimeRadiosity
{
public:
	Solver()
	{
		bigMap = de::Texture::create(NULL,BIG_MAP_SIZE,BIG_MAP_SIZE,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
		scaleDownProgram = new de::Program(NULL,"../../data/shaders/scaledown_filter.vp", "../../data/shaders/scaledown_filter.fp");
		smallMap = new GLuint[BIG_MAP_SIZE*BIG_MAP_SIZE/16];
	}
	virtual ~Solver()
	{
		delete[] smallMap;
		delete scaleDownProgram;
		delete bigMap;
		// delete objects and illumination
		delete3dsFromRR(this);
	}
protected:
#ifdef AMBIENT_MAPS
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		// Decide how big ambient map you want for object. 
		// When seams appear, increase res.
		// Depends on quality of unwrap provided by object->getTriangleMapping.
		// This demo has bad unwrap -> high res map.
		return new rr::RRIlluminationPixelBufferInOpenGL(1024,1024,"../../data/shaders/");
	}
#endif
	// skipped, material properties were already readen from .3ds and never change
	virtual void detectMaterials() {}
	// detects direct illumination irradiances on all faces in scene
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!rendererCaching) return false;

		// shadowmap could be outdated, update it
		updateShadowmap(0);

		rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		// adjust captured texture size so we don't waste pixels
		captureUv.xmax = BIG_MAP_SIZE/4; // number of triangles in one row
		captureUv.ymax = BIG_MAP_SIZE/4; // number of triangles in one column
		while(captureUv.ymax && numTriangles/(captureUv.xmax*captureUv.ymax)==numTriangles/(captureUv.xmax*(captureUv.ymax-1))) captureUv.ymax--;
		unsigned width = BIG_MAP_SIZE; // used width in pixels
		unsigned height = captureUv.ymax*4; // used height in pixels

		// setup render states
		glClearColor(0,0,0,1);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);
		glViewport(0, 0, width, height);

		// for each set of triangles (if all triangles don't fit into one texture)
		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			unsigned lastCapturedTriangle = MIN(numTriangles,captureUv.firstCapturedTriangle+captureUv.xmax*captureUv.ymax)-1;

			// prepare for scaling down -> render to texture
			bigMap->renderingToBegin();

			// clear
			glViewport(0, 0, width,height);
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene with forced 2d positions of all triangles
			de::UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_DIRECT_MAP = true;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.FORCE_2D_POSITION = true;
			rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle,lastCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup);
			rendererNonCaching->setCapture(NULL,0,numTriangles-1);

			// downscale 10pixel triangles in 4x4 squares to single pixel values
			bigMap->renderingToEnd();
			scaleDownProgram->useIt();
			scaleDownProgram->sendUniform("lightmap",0);
			scaleDownProgram->sendUniform("pixelDistance",1.0f/BIG_MAP_SIZE,1.0f/BIG_MAP_SIZE);
			glViewport(0,0,BIG_MAP_SIZE/4,BIG_MAP_SIZE/4);//!!! needs at least 256x256 backbuffer
			// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
			glClearColor(1,0,1,0);
			glClear(GL_COLOR_BUFFER_BIT);
			// setup pipeline
			glActiveTexture(GL_TEXTURE0);
			glDisable(GL_CULL_FACE);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			bigMap->bindTexture();
			glBegin(GL_POLYGON);
			glMultiTexCoord2f(0,0,0);
			glVertex2f(-1,-1);
			glMultiTexCoord2f(0,0,1);
			glVertex2f(-1,1);
			glMultiTexCoord2f(0,1,1);
			glVertex2f(1,1);
			glMultiTexCoord2f(0,1,0);
			glVertex2f(1,-1);
			glEnd();
			glClearColor(0,0,0,0);
			glEnable(GL_CULL_FACE);

			// read downscaled image to memory
			glReadPixels(0, 0, captureUv.xmax, captureUv.ymax, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, smallMap);

			// send triangle irradiances to solver
			for(unsigned triangleIndex=captureUv.firstCapturedTriangle;triangleIndex<=lastCapturedTriangle;triangleIndex++)
			{
				unsigned color = smallMap[triangleIndex-captureUv.firstCapturedTriangle];
				getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(
					triangleIndex,
					rr::RM_IRRADIANCE_CUSTOM,
					rr::RRColor((color>>24)&255,(color>>16)&255,(color>>8)&255) / 255.0f);
			}
		}

		// restore render states
		glViewport(0, 0, winWidth, winHeight);
		glDepthMask(1);
		glEnable(GL_DEPTH_TEST);
		return true;
	}
private:
	CaptureUv    captureUv;           // uv generator for rendering into bigMap
	de::Texture* bigMap;              // map with 10pixel irradiances per triangle
	de::Program* scaleDownProgram;    // program for downscaling bigMap into smallMap
	GLuint*      smallMap;            // map with 1pixel irradiances per triangle
	enum         {BIG_MAP_SIZE=1024}; // size of bigMap
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
		solver->calculate(rr::RRRealtimeRadiosity::UPDATE_PIXEL_BUFFERS);
	}
#else // here we say: render with indirect illumination per-vertex
	uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
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
	switch (c)
	{
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
			solver->reportLightChange(true);
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
			if(cam==&light) solver->reportLightChange(true);
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

	// init GLUT
	glutInitWindowSize(800, 600);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutGameModeString("800x600:32");
	glutEnterGameMode();
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
	unsigned shadowmapsPerPass = de::UberProgramSetup::detectMaxShadowmaps(uberProgram);
	if(shadowmapsPerPass>1) shadowmapsPerPass--; // needed because of bug in ATI drivers. delete to improve quality on NVIDIA.
	if(shadowmapsPerPass>1) shadowmapsPerPass--; // needed because of bug in ATI drivers. delete to improve quality on NVIDIA.
	if(!shadowmapsPerPass) error("",true);
	
	// init textures
	lightDirectMap = de::Texture::load("..\\..\\data\\maps\\spot0.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.tga not found.\n",false);
	areaLight = new de::AreaLight(&light,shadowmapsPerPass,512);

	// init static .3ds scene
	if(!m3ds.Load("..\\..\\data\\3ds\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init dynamic objects
	de::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("..\\..\\data\\3ds\\characters\\I Robot female.3ds",0.3f,material,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("..\\..\\data\\3ds\\characters\\potato\\potato01.3ds",0.004f,material,16);

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
	rendererNonCaching = new RendererOfRRObject(solver->getMultiObjectCustom(),solver->getScene(),solver->getScaler());
	rendererCaching = new de::RendererWithCache(rendererNonCaching);

	glutMainLoop();
	return 0;
}
