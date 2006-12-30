// Hello Realtime Radiosity sample
//
// Use of RealtimeRadiosity is demonstrated on .3ds scene viewer.
// You should be familiar with GLUT and OpenGL.
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Soft shadow quality is reduced due to bug in ATI drivers.
// Improve it on NVIDIA by deleting two lines with NVIDIA in comment.
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
#include "DemoEngine/RRIlluminationPixelBufferInOpenGL.h"
#include "DemoEngine/Timer.h"

//#define AMBIENT_MAPS
// Makes indirect illumination stored into and rendered from ambient maps (instead of vertex buffers).
// With ambient maps render is slower because:
// - generating ambient maps is more expensive than generating vertex buffer
// - unwrap generated by us is not compatible with tristrip in .3ds,
//   so we fallback from fast tristrip renderer to emergency trilist renderer.
//   you can easily avoid this problem by baking unwraps into your meshes.

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
float               speedForward = 0;
float               speedBack = 0;
float               speedRight = 0;
float               speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(UberProgramSetup uberProgramSetup)
{
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap))
		error("Failed to compile or link GLSL program.\n",true);
#ifndef AMBIENT_MAPS
	// m3ds.Draw uses tristrips incompatible with ambient map uv channel, doesn't render properly with ambient maps
	//  could be fixed with better uv or simple geometry shader
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(uberProgramSetup.LIGHT_INDIRECT_COLOR?solver:NULL);
		return;
	}
#endif
	// RendererOfRRObject::render uses trilist -> slow, but no problem with added ambient map unwrap
	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.OBJECT_SPACE = uberProgramSetup.OBJECT_SPACE;
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
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%xmax)+((vertexIndex==2)?1:0)-xmax*0.5f)/(xmax*0.5f);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/xmax)+((vertexIndex==0)?1:0)-ymax*0.5f)/(ymax*0.5f);
	}
	unsigned firstCapturedTriangle; // index of first captured triangle (for case that there are too many triangles for one texture)
	unsigned xmax, ymax; // max number of triangles in texture
};

class Solver : public rr::RRRealtimeRadiosity
{
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

		rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		// adjust captured texture size so we don't waste pixels
		unsigned width1 = 4;
		unsigned height1 = 4;
		captureUv.xmax = winWidth/width1;
		captureUv.ymax = winHeight/height1;
		while(captureUv.ymax && numTriangles/(captureUv.xmax*captureUv.ymax)==numTriangles/(captureUv.xmax*(captureUv.ymax-1))) captureUv.ymax--;
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
			uberProgramSetup.LIGHT_INDIRECT_CONST = false;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
			uberProgramSetup.LIGHT_INDIRECT_MAP = false;
			uberProgramSetup.LIGHT_INDIRECT_ENV = false;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.MATERIAL_SPECULAR = false;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
			uberProgramSetup.MATERIAL_NORMAL_MAP = false;
			uberProgramSetup.OBJECT_SPACE = false;
			uberProgramSetup.FORCE_2D_POSITION = true;
			rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle,lastCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup);
			rendererNonCaching->setCapture(NULL,0,numTriangles-1);

			// read back rendered image to memory
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// accumulate triangle irradiances
			for(unsigned triangleIndex=captureUv.firstCapturedTriangle;triangleIndex<=lastCapturedTriangle;triangleIndex++)
			{
				// accumulate 1 triangle power from square region in texture
				// (square coordinate calculation is in match with CaptureUv uv generator)
				unsigned sum[3] = {0,0,0};
				unsigned i = (triangleIndex-captureUv.firstCapturedTriangle)%captureUv.xmax;
				unsigned j = (triangleIndex-captureUv.firstCapturedTriangle)/captureUv.xmax;
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
				rr::RRColor avg = rr::RRColor(sum[0],sum[1],sum[2]) / (255*width1*height1/2);
				getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(triangleIndex,rr::RM_IRRADIANCE_CUSTOM,avg);
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
	solver->reportInteraction();
	needRedisplay = true;
}

// called each time light moves
void reportLightMovement()
{
	// reports light change with hint on how big the change was. it's better to report big change for big scene
	solver->reportLightChange(solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
	solver->reportInteraction();
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
	uberProgramSetup.LIGHT_INDIRECT_CONST = false;
#ifdef AMBIENT_MAPS // here we say: render with ambient maps
	uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
	uberProgramSetup.LIGHT_INDIRECT_MAP = true;
	if(!solver->getIllumination(0)->getChannel(0)->pixelBuffer) // if ambient maps don't exist yet, create them
	{
		solver->calculate(rr::RRRealtimeRadiosity::UPDATE_PIXEL_BUFFERS);
	}
#else // here we say: render with indirect illumination per-vertex
	uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
#endif
	uberProgramSetup.LIGHT_INDIRECT_ENV = false;
	uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.MATERIAL_SPECULAR = false;
	uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
	uberProgramSetup.MATERIAL_NORMAL_MAP = false;
	uberProgramSetup.OBJECT_SPACE = false;
	uberProgramSetup.FORCE_2D_POSITION = false;
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

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		CLAMP(seconds,0.001f,0.3f);
		Camera* cam = modeMovingEye?&eye:&light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam==&light) reportLightMovement(); else reportEyeMovement();
		}
	}
	prev = now;

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
	uberProgram = new UberProgram("..\\..\\data\\shaders\\ubershader.vp", "..\\..\\data\\shaders\\ubershader.fp");
	unsigned shadowmapsPerPass = UberProgramSetup::detectMaxShadowmaps(uberProgram);
	if(shadowmapsPerPass) shadowmapsPerPass--; // needed because of bug in ATI drivers. delete to improve quality on NVIDIA.
	if(!shadowmapsPerPass) error("",true);
	
	// init textures
	lightDirectMap = Texture::load("..\\..\\data\\maps\\spot0.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.tga not found.\n",false);
	areaLight = new AreaLight(shadowmapsPerPass,512);
	areaLight->attachTo(&light);

	// init .3ds scene
	if(!m3ds.Load("..\\..\\data\\3ds\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init realtime radiosity solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	provideObjectsFrom3dsToRR(&m3ds,solver,NULL);
	solver->calculate();
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new RendererOfRRObject(solver->getMultiObjectCustom(),solver->getScene(),solver->getScaler());
	rendererCaching = new RendererWithCache(rendererNonCaching);

	glutMainLoop();
	return 0;
}
