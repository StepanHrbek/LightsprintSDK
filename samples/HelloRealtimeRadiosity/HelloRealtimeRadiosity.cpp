// Hello Realtime Radiosity sample
//
// Use of RealtimeRadiosity is demonstrated on .3ds scene viewer.
// You should be familiar with GLUT and OpenGL.
//
// Controls:
//  move = look around
//  arrows = move around
//  left button = switch between camera and light
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
#include "DemoEngine/MultiLight.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/3ds2rr.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/RendererOfRRObject.h"
#include "DemoEngine/UberProgramSetup.h"


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

Model_3DS           m3ds;
Camera              eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
Camera              light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
AreaLight*          areaLight = NULL;
Texture*            lightDirectMap = NULL;
UberProgram*        uberProgram = NULL;
RendererOfRRObject* rendererNonCaching = NULL;
RendererWithCache*  rendererCaching = NULL;
rr::RRRealtimeRadiosity* solver = NULL;
int                 winWidth = 0;
int                 winHeight = 0;
bool                modeMovingEye = true;
bool                needDepthMapUpdate = true;
bool                needRedisplay = true;


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
		m3ds.Draw(uberProgramSetup.LIGHT_INDIRECT_COLOR?solver:NULL,uberProgramSetup.LIGHT_INDIRECT_MAP);
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
	Texture* shadowmap = areaLight->getShadowMap(mapIndex);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
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
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/ymax)+((vertexIndex<2)?0:1)-xmax*0.5f)/(xmax*0.5f);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%ymax)+1-(vertexIndex%2)-ymax*0.5f)/(ymax*0.5f);
	}
	unsigned firstCapturedTriangle; // index of first captured triangle (for case that there are too many triangles for one texture)
	unsigned xmax, ymax; // max number of triangles in texture
};

class Solver : public rr::RRRealtimeRadiosity
{
protected:
	// skipped, material properties were already readen from .3ds and never change
	virtual void detectMaterials() {}
	// detects direct illumination level on all faces in scene
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!rendererCaching) return false;

		// shadowmap is not ready, update it
		if(needDepthMapUpdate)
		{
			updateShadowmap(0);
		}

		rr::RRMesh* mesh = multiObject->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		// adjust captured texture size so we don't waste pixels
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

		// allocate the texture memory as necessary
		GLuint* pixelBuffer = new GLuint[width * height];

		// for each set of triangles (if all triangles doesn't fit into one texture)
		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			unsigned lastCapturedTriangle = MIN(numTriangles,captureUv.firstCapturedTriangle+captureUv.xmax*captureUv.ymax)-1;

			// clear
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene with forced 2d positions of all triangles
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
			rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle,lastCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup);
			rendererNonCaching->setCapture(NULL,0,numTriangles-1);

			// read back the index buffer to memory
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// accumulate triangle irradiances
			for(unsigned triangleIndex=captureUv.firstCapturedTriangle;triangleIndex<=lastCapturedTriangle;triangleIndex++)
			{
				// accumulate 1 triangle power from square region in texture
				// (square coordinate calculation is in match with CaptureUv uv generator)
				unsigned sum[3] = {0,0,0};
				unsigned i = (triangleIndex-captureUv.firstCapturedTriangle)/captureUv.ymax;
				unsigned j = (triangleIndex-captureUv.firstCapturedTriangle)%captureUv.ymax;
				for(unsigned n=0;n<height1;n++)
					for(unsigned m=0;m<width1;m++)
					{
						unsigned pixel = width*(j*height1+n) + (i*width1+m);
						unsigned color = pixelBuffer[pixel] >> 8; // alpha was lost
						sum[0] += color>>16;
						sum[1] += (color>>8)&255;
						sum[2] += color&255;
					}
				// pass irradiance to rrobject
				// (default implementation of adjustScene() has globally switched unit from W/m^2 to screen rgb)
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
private:
	CaptureUv captureUv;
};

// called each time eye moves
void reportEyeMovement()
{
	// solver->reportCriticalInteraction(); // improves responsiveness on singlecore machine
	needRedisplay = true;
}

// called each time light moves
void reportLightMovement()
{
	// reports light change with hint on how big the change was. it's better to report big change for big scene
	solver->reportLightChange(solver->getMultiObject()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
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
	solver->reportIlluminationUse();
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
	if(modeMovingEye) solver->reportCriticalInteraction();
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
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getDepthBits();
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
	if(solver->calculate()==rr::RRScene::IMPROVED || needRedisplay)
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
	areaLight = new AreaLight(shadowmapsPerPass,512);
	areaLight->attachTo(&light);

	// init .3ds scene
	if(!m3ds.Load("..\\..\\data\\3ds\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init realtime radiosity solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\license_number")!=rr::RRLicense::VALID)
		error("Problem with license number.\n", false);
	solver = new Solver();
	provideObjectsFrom3dsToRR(&m3ds,solver,0.01f);
	solver->calculate();
	if(!solver->getMultiObject())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new RendererOfRRObject(solver->getMultiObject(),solver->getScene());
	rendererCaching = new RendererWithCache(rendererNonCaching);

	glutMainLoop();
	return 0;
}
