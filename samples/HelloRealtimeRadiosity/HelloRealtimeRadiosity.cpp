// Hello Realtime Radiosity sample
//
// You should be familiar with GLUT and OpenGL.
// Controls: move mouse with left or right button pressed.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Camera.h"
#include "DemoEngine/UberProgram.h"
#include "DemoEngine/MultiLight.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/3ds2rr.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/RendererOfRRObject.h"
#include "DemoEngine/UberProgramSetup.h"

using namespace std;

#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMP(a,min,max) (a)=(((a)<(min))?min:(((a)>(max)?(max):(a))))


/////////////////////////////////////////////////////////////////////////////

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nTry upgrading drivers for your graphics card.\nIf it doesn't help, your graphics card may be too old.\nCards that should work: NVIDIA 6xxx/7xxx, ATI 95xx+/Xxxx/X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are required by GLUT design with callbacks

bool                updateRadiosityDuringLightMovement = 1;
Model_3DS           m3ds;
Camera              eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
Camera              light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
AreaLight*          areaLight = NULL;
Texture*            lightDirectMap = NULL;
UberProgram*        uberProgram = NULL;
int                 winWidth, winHeight;
bool                needDepthMapUpdate = 1;
RendererOfRRObject* rendererNonCaching = NULL;
RendererWithCache*  rendererCaching = NULL;
class MyApp*        app = NULL;
bool                modeMovingEye = true;
bool                needRedisplay = false;

/////////////////////////////////////////////////////////////////////////////
//
// integration with Realtime Radiosity

void renderScene(UberProgramSetup uberProgramSetup);
void updateShadowmap(unsigned mapIndex);

// generate uv coords for direct illumination capture
class CaptureUv : public VertexDataGenerator
{
public:
	virtual ~CaptureUv() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) // vertexIndex=0..2
	{
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/ymax)+((vertexIndex<2)?0:1)-xmax/2)/(xmax/2);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%ymax)+1-(vertexIndex%2)-ymax/2)/(ymax/2);
	}
	unsigned firstCapturedTriangle;
	unsigned xmax, ymax;
};

class MyApp : public rr::RRRealtimeRadiosity
{
protected:
	virtual void detectMaterials() {}
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!rendererCaching) return false;

		// first time illumination is detected, no shadowmap has been created yet
		if(needDepthMapUpdate)
		{
			updateShadowmap(0);
		}

		rr::RRMesh* mesh = multiObject->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		// adjust captured texture size so we don't waste pixels
		static CaptureUv captureUv;
		unsigned width1 = 4;
		unsigned height1 = 4;
		captureUv.firstCapturedTriangle = 0;
		captureUv.xmax = winWidth/width1;
		captureUv.ymax = winHeight/height1;
		while(captureUv.xmax && numTriangles/(captureUv.xmax*captureUv.ymax)==numTriangles/((captureUv.xmax-1)*captureUv.ymax)) captureUv.xmax--;
		unsigned width = captureUv.xmax*width1;
		unsigned height = captureUv.ymax*height1;

		// setup render states
		glClearColor(0,0,0,1);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);
		glViewport(0, 0, width, height);

		// Allocate the index buffer memory as necessary.
		GLuint* pixelBuffer = new GLuint[width * height];

		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			// clear
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_DIRECT_MAP = true;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
			uberProgramSetup.LIGHT_INDIRECT_MAP = false;
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.FORCE_2D_POSITION = true;
			rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup);
			rendererNonCaching->setCapture(NULL,0);

			// Read back the index buffer to memory.
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// accumulate triangle powers
			for(unsigned triangleIndex=captureUv.firstCapturedTriangle;triangleIndex<MIN(numTriangles,captureUv.firstCapturedTriangle+captureUv.xmax*captureUv.ymax);triangleIndex++)
			{
				// accumulate 1 triangle power
				unsigned sum[3] = {0,0,0};
				unsigned i = (triangleIndex-captureUv.firstCapturedTriangle)/(height/height1);
				unsigned j = triangleIndex%(height/height1);
				for(unsigned n=0;n<height1;n++)
					for(unsigned m=0;m<width1;m++)
					{
						unsigned pixel = width*(j*height1+n) + (i*width1+m);
						unsigned color = pixelBuffer[pixel] >> 8; // alpha was lost
						sum[0] += color>>16;
						sum[1] += (color>>8)&255;
						sum[2] += color&255;
					}
				// pass power to rrobject
				rr::RRColor avg = rr::RRColor(sum[0],sum[1],sum[2]) / (255*width1*height1/2);
				multiObject->setTriangleAdditionalMeasure(triangleIndex,rr::RM_IRRADIANCE,avg);
			}
		}

		delete[] pixelBuffer;

		// restore render states
		glViewport(0, 0, winWidth, winHeight);
		glDepthMask(1);
		glEnable(GL_DEPTH_TEST);
		return true;
	}
	virtual void reportAction(const char* action) const
	{
		printf(action);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(UberProgramSetup uberProgramSetup)
{
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap))
		error("Failed to compile or link GLSL program.\n",true);
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(uberProgramSetup.LIGHT_INDIRECT_COLOR?app:NULL,uberProgramSetup.LIGHT_INDIRECT_MAP);
		return;
	}
	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
	rendererNonCaching->setRenderedChannels(renderedChannels);
	rendererCaching->render();
}

void updateShadowmap(unsigned mapIndex)
{
	Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	areaLight->getShadowMap(mapIndex)->renderingToInit();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
	renderScene(uberProgramSetup);
	areaLight->getShadowMap(mapIndex)->renderingToDone();
	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);
}

void reportEyeMovement()
{
	app->reportCriticalInteraction();
	needRedisplay = true;
}

void reportLightMovement()
{
	if(updateRadiosityDuringLightMovement)
	{
		app->reportLightChange(app->getMultiObject()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
	}
	else
	{
		app->reportCriticalInteraction();
	}
	needDepthMapUpdate = 1;
	needRedisplay = true;
}


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window
	needRedisplay = false;
	app->reportIlluminationUse();
	eye.update(0);
	light.update(0.3f);
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++) updateShadowmap(i);
	needDepthMapUpdate = false;
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.FORCE_2D_POSITION = false;
	renderScene(uberProgramSetup);
	glutSwapBuffers();
}

void special(int c, int x, int y)
{
	if(modeMovingEye) app->reportCriticalInteraction();
	Camera* cam = modeMovingEye?&eye:&light;
	switch (c) 
	{
		case GLUT_KEY_UP:
			cam->moveForward(0.05f);
			break;
		case GLUT_KEY_DOWN:
			cam->moveBack(0.05f);
			break;
		case GLUT_KEY_LEFT:
			cam->moveLeft(0.05f);
			break;
		case GLUT_KEY_RIGHT:
			cam->moveRight(0.05f);
			break;
		default:
			return;
	}
	if(modeMovingEye) reportEyeMovement(); else reportLightMovement();
	glutPostRedisplay();
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
	GLint shadowDepthBits = TextureShadowMap::getDepthBits();
	glPolygonOffset(4, 42 * ( (shadowDepthBits>=24) ? 1 << (shadowDepthBits - 16) : 1 ));
	needDepthMapUpdate = 1;
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
			reportEyeMovement();
		}
		else
		{
			light.angle = light.angle - 0.005*x;
			light.height = light.height + 0.15*y;
			CLAMP(light.height,-13,13);
			reportLightMovement();
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	if(!winWidth) return; // can't work without window
	if(app->calculate()==rr::RRScene::IMPROVED || needRedisplay)
	{
		glutPostRedisplay();
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
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
	glClearDepth(0.999999); // prevents backprojection, tested on nvidia geforce 6600

	// init shaders
	uberProgram = new UberProgram("..\\..\\data\\shaders\\ubershader.vp", "..\\..\\data\\shaders\\ubershader.fp");
	unsigned shadowmapsPerPass = UberProgramSetup::detectMaxShadowmaps(uberProgram);
	if(!shadowmapsPerPass) error("",true);

	// init textures
	lightDirectMap = Texture::load("..\\..\\data\\maps\\spot0.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.tga not found or not supported (supported = truecolor .tga).\n",false);
	areaLight = new AreaLight(shadowmapsPerPass);
	areaLight->attachTo(&light);

	// init .3ds scene
	if(!m3ds.Load("..\\..\\data\\3ds\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init radiosity solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\license_number")!=rr::RRLicense::VALID)
		error("Problem with license number.\n", false);
	app = new MyApp();
	new_3ds_importer(&m3ds,app,0.01f);
	app->calculate();
	if(!app->getMultiObject())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new RendererOfRRObject(app->getMultiObject(),app->getScene());
	rendererCaching = new RendererWithCache(rendererNonCaching);

	glutMainLoop();
	return 0;
}
