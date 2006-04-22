#define _3DS
//#define SHOW_CAPTURED_TRIANGLES
//#define DEFAULT_SPONZA
//#define MAX_FILTERED_INSTANCES 12  // max number of light instances with shadow filtering enabled
#define MAX_INSTANCES          50  // max number of light instances aproximating one area light
#define MAX_INSTANCES_PER_PASS 10
unsigned INSTANCES_PER_PASS=7;
unsigned initialPasses=1;
#define AREA_SIZE 0.15f
int fullscreen = 1;
int shadowSamples = 4;
bool lightIndirectMap = 0;//!!!
/*
rr renderer: pridat lightmapu aby aspon nekde behal muj primitivni unwrap
rr renderer: pridat normaly aby to smoothovalo direct

pridat dalsi koupelny
ovladani jasu (global, indirect)
nacitat jpg
spatne pocita sponza podlahu

autodeteknout zda mam metry nebo centimetry
dodelat podporu pro matice do 3ds2rr importeru
kdyz uz by byl korektni model s gammou, pridat ovladani gammy

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce
 kdyz se vypne scaler(0.4) nebo indirect, primary vypada desne tmave
 az pri secteni s indirectem (scitani produkuje prilis velke cislo) zacne vypadat akorat
*/

#include <assert.h>
#include <float.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/glut.h>

#include "RRVisionApp.h"

//#include "glsl/Light.hpp"
#include "glsl/Camera.hpp"
#include "glsl/GLSLProgram.hpp"
#include "glsl/GLSLShader.hpp"
#include "glsl/Texture.hpp"
#include "glsl/FrameRate.hpp"
#include "mgf2rr.h"
#include "rr2gl.h"
#include "matrix.h"   /* OpenGL-style 4x4 matrix manipulation routines */

using namespace std;

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))


/////////////////////////////////////////////////////////////////////////////
//
// MGF

char *mgf_filename="data\\scene8.mgf";


/////////////////////////////////////////////////////////////////////////////
//
// 3DS

#ifdef _3DS
#include "Model_3DS.h"
#include "3ds2rr.h"
Model_3DS m3ds;
#ifdef DEFAULT_SPONZA
	char* filename_3ds="sponza\\sponza.3ds";
	float scale_3ds = 1;
#else
	//char* filename_3ds="veza\\veza.3ds";
	//float scale_3ds = 1;
	char* filename_3ds="koupelna4\\koupelna4.3ds";
	float scale_3ds = 0.03f;
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
//
// RR

bool renderDiffuseTexture = true;
bool renderOnlyRr = false;
bool renderSource = false;
RRCachingRenderer* renderer = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// GLSL

GLSLProgram *lightProg, *ambientProg;
GLSLProgramSet* ubershaderProgSet;
Texture *lightDirectMap;
FrameRate *counter;
unsigned int shadowTex[MAX_INSTANCES];
int currentWindowSize;
#define SHADOW_MAP_SIZE 512
int softLight = -1; // current instance number 0..199, -1 = hard shadows, use instance 0

void checkGlError()
{
	GLenum err = glGetError();
	if(err!=GL_NO_ERROR)
	{
		printf("glGetError=%x\n",err);
	}
}

void initShadowTex()
{
	glGenTextures(MAX_INSTANCES, shadowTex);

	for(unsigned i=0;i<MAX_INSTANCES;i++)
	{
		glBindTexture(GL_TEXTURE_2D, shadowTex[i]);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE,
			SHADOW_MAP_SIZE,0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		// for shadow2D() instead of texture2D()
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
	}
}

void updateShadowTex()
{
	glBindTexture(GL_TEXTURE_2D, shadowTex[(softLight>=0)?softLight:0]);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
}

void initShaders()
{
	ubershaderProgSet = new GLSLProgramSet("shaders\\ubershader.vp", "shaders\\ubershader.fp");
	lightProg = new GLSLProgram(NULL,"shaders\\shadowmap.vp");
	ambientProg = new GLSLProgram(NULL,"shaders\\ambient.vp", "shaders\\ambient.fp");
}

void glsl_init()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);

	glShadeModel(GL_SMOOTH);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);

	initShadowTex();
	initShaders();

	counter = new FrameRate("hello");
	lightDirectMap = new Texture("spot0.tga", GL_LINEAR,
		GL_LINEAR, GL_CLAMP, GL_CLAMP);
}

void activateTexture(unsigned int textureUnit, unsigned int textureType)
{
	glActiveTextureARB(textureUnit);
	glEnable(textureType);
}




#define MAX_SIZE 1024

/* Display lists. */
enum {
  DL_RESERVED = 0, /* zero is not a valid OpenGL display list ID */
  DL_SPHERE,       /* sphere display list */
};

/* Draw modes. */
enum {
  DM_LIGHT_VIEW,
  DM_EYE_VIEW_SHADOWED,
  DM_EYE_VIEW_SOFTSHADOWED,
};

/* Menu items. */
enum {
  ME_EYE_VIEW_SHADOWED,
  ME_EYE_VIEW_SOFTSHADOWED,
  ME_LIGHT_VIEW,
  ME_TOGGLE_GLOBAL_ILLUMINATION,
  ME_TOGGLE_WIRE_FRAME,
  ME_TOGGLE_LIGHT_FRUSTUM,
  ME_SWITCH_MOUSE_CONTROL,
  ME_EXIT,
};

bool forcerun = 0;
int vsync = 0;
int requestedDepthMapSize = 512;
int depthMapSize = 512;

GLenum depthMapPrecision = GL_UNSIGNED_BYTE;
GLenum depthMapFormat = GL_LUMINANCE;
GLenum depthMapInternalFormat = GL_INTENSITY8;

unsigned useLights = initialPasses*INSTANCES_PER_PASS;
int softWidth[MAX_INSTANCES],softHeight[MAX_INSTANCES],softPrecision[MAX_INSTANCES],softFiltering[MAX_INSTANCES];
int areaType = 0; // 0=linear, 1=square grid, 2=circle

int depthBias24 = 42;
int depthScale24;
GLfloat slopeScale = 4.0;

GLfloat textureLodBias = 0.0;

void *font = GLUT_BITMAP_8_BY_13;

struct SimpleCamera
{
	// inputs
	GLfloat  pos[3];
	float    angle;
	float    height;
	// products
	GLfloat  dir[4];
	GLdouble viewMatrix[16];
	// tools
	void updateViewMatrix(float back)
	{
		buildLookAtMatrix(viewMatrix,
			pos[0]-back*dir[0],pos[1]-back*dir[1],pos[2]-back*dir[2],
			pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
			0, 1, 0);
	}
};

// light and camera setup
//SimpleCamera eye = {{0,1,4},3,0};
//SimpleCamera light = {{0,3,0},0.85f,8};
SimpleCamera eye = {{0.000000,1.000000,4.000000},2.935000,-0.7500};
SimpleCamera light = {{-1.233688,3.022499,-0.542255},1.239998,6.649996};

GLUquadricObj *q;
int xEyeBegin, yEyeBegin, movingEye = 0;
int xLightBegin, yLightBegin, movingLight = 0;
int wireFrame = 0;

int winWidth, winHeight;
int needMatrixUpdate = 1;
int needTitleUpdate = 1;
int needDepthMapUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
bool showHelp = 0;
int useDisplayLists = 1;
int showLightViewFrustum = 1;
int eyeButton = GLUT_LEFT_BUTTON;
int lightButton = GLUT_MIDDLE_BUTTON;
int useDepth24 = 0;
int drawFront = 0;

int hasShadow = 0;
int hasDepthTexture = 0;
int hasTextureBorderClamp = 0;
int hasTextureEdgeClamp = 0;

double winAspectRatio;

GLdouble eyeFieldOfView = 100.0;
GLdouble eyeNear = 0.3;
GLdouble eyeFar = 60.0;
GLdouble lightFieldOfView = 70.0;
GLdouble lightNear = 1;
GLdouble lightFar = 20.0;

GLdouble eyeFrustumMatrix[16];
GLdouble lightInverseViewMatrix[16];
GLdouble lightFrustumMatrix[16];
GLdouble lightInverseFrustumMatrix[16];


/////////////////////////////////////////////////////////////////////////////
//
// MyApp

void drawHardwareShadowPass(RRObjectRenderer::ColorChannel cc);

// generuje uv coords pro capture
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

CaptureUv captureUv;

// external dependencies of MyApp:
// z m3ds detekuje materialy
// renderer je pouzit k captureDirect
class MyApp : public rrVision::RRVisionApp
{
protected:
	virtual void detectMaterials()
	{
		delete[] surfaces;
		numSurfaces = m3ds.numMaterials;
		surfaces = new rrVision::RRSurface[numSurfaces];
		for(unsigned i=0;i<numSurfaces;i++)
		{
			surfaces[i].reset(false);
			surfaces[i].diffuseReflectance = rrVision::RRColor(m3ds.Materials[i].color.r/255.0,m3ds.Materials[i].color.g/255.0,m3ds.Materials[i].color.b/255.0);
		}
	}
	virtual void detectDirectIllumination()
	{
		if(!renderer) return;

		//!!!
		/*
		for each object
		generate pos-override channel
		render with pos-override
		prubezne pri kazdem naplneni textury
		zmensi texturu na gpu
		zkopiruj texturu do cpu
		uloz vysledky do AdditionalObjectImporteru
		*/
		//!!! needs windows at least 512x512
		unsigned width1 = 4;
		unsigned height1 = 4;
		unsigned width = 512;
		unsigned height = 512;
		captureUv.firstCapturedTriangle = 0;
		captureUv.xmax = width/width1;
		captureUv.ymax = height/height1;

		// setup render states
		glClearColor(0,0,0,1);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);
		glViewport(0, 0, width, height);

		// Allocate the index buffer memory as necessary.
		GLuint* pixelBuffer = (GLuint*)malloc(width * height * 4);

		rrCollider::RRMeshImporter* mesh = multiObject->getCollider()->getImporter();
		unsigned numTriangles = mesh->getNumTriangles();

		//printf("%d %d\n",numTriangles,captureUv.xmax*captureUv.ymax);
		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			// clear
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene
			renderer->setStatus(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION,RRCachingRenderer::CS_NEVER_COMPILE);
			generateForcedUv = &captureUv;
			drawHardwareShadowPass(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION);
			generateForcedUv = NULL;

			// Read back the index buffer to memory.
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// dbg print
			//rrVision::RRColor suma = rrVision::RRColor(0);

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
					rrVision::RRColor avg = rrVision::RRColor(sum[0],sum[1],sum[2]) / (255*width1*height1/2);
					multiObject->setTriangleAdditionalMeasure(triangleIndex,rrVision::RM_EXITANCE,avg);

					// debug print
					//rrVision::RRColor tmp = rrVision::RRColor(0);
					//rrobject->getTriangleAdditionalMeasure(triangleIndex,rrVision::RM_EXITING_FLUX,tmp);
					//suma+=tmp;
			}
			//printf("sum = %f/%f/%f\n",suma[0],suma[1],suma[2]);
		}

		free(pixelBuffer);

		// restore render states
		glViewport(0, 0, winWidth, winHeight);
		glDepthMask(1);
		glEnable(GL_DEPTH_TEST);
	}
	virtual void reportAction(const char* action) const
	{
		printf("%s\n",action);
	}
};

MyApp* app = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// OPENGL INITIALIZATION AND CHECKS

static int supports20(void)
{
	const char *version;
	int major, minor;

	version = (char *) glGetString(GL_VERSION);
	if (sscanf(version, "%d.%d", &major, &minor) == 2) {
		return major>=2;// || (major==1 && minor>=5);
	}
	return 0;            /* OpenGL version string malformed! */
}

/* updateTitle - update the window's title bar text based on the current
   program state. */
void updateTitle(void)
{
	char title[256];

	/* Only update the title is needTitleUpdate is set. */
	if (needTitleUpdate) 
	{
		/*
		char *modeName;
		int depthBits;
		switch (drawMode) 
		{
		case DM_LIGHT_VIEW:
			modeName = "light view";
		case DM_EYE_VIEW_SHADOWED:
			modeName = "primary illumination";
			break;
		case DM_EYE_VIEW_SOFTSHADOWED:
			modeName = "global illumination";
			break;
		default:
			assert(0);
			modeName = "";
			break;
		}
		glGetIntegerv(GL_DEPTH_BITS, &depthBits);
		sprintf(title,
			"realtime radiosity viewer - %s, shadowmaps %dx%d %d-bit, depthBias=%d, slopeScale=%f",
			modeName, depthMapSize, depthMapSize, depthBits, depthBias24, slopeScale);
			*/
		sprintf(title,"realtime radiosity viewer - %s",filename_3ds);
		glutSetWindowTitle(title);
		needTitleUpdate = 0;
	}
}


/*** DRAW VARIOUS SCENES ***/

static void drawSphere(void)
{
	gluSphere(q, 0.55, 16, 16);
}


GLSLProgram* getProgramCore(RRObjectRenderer::ColorChannel cc)
{
	switch(cc)
	{
		case RRObjectRenderer::CC_NO_COLOR:
			return lightProg;
		case RRObjectRenderer::CC_TRIANGLE_INDEX:
			return ambientProg;
		case RRObjectRenderer::CC_DIFFUSE_REFLECTANCE:
		case RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
		case RRObjectRenderer::CC_REFLECTED_IRRADIANCE:
		case RRObjectRenderer::CC_REFLECTED_EXITANCE:
			{
			GLSLProgramSet* progSet = ubershaderProgSet;
			static char tmp[200];
			bool force2d = cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION;
			bool lightIndirect1 = cc==RRObjectRenderer::CC_REFLECTED_IRRADIANCE || cc==RRObjectRenderer::CC_REFLECTED_EXITANCE;
			bool lightIndirect2 = lightIndirect1 || (!force2d && softLight>=0 && useLights<=INSTANCES_PER_PASS);
			bool lightIndirectColor1 = lightIndirect2 && !lightIndirectMap;
			bool lightIndirectMap1 = lightIndirect2 && lightIndirectMap;
			bool hasDiffuseColor = cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION;
retry:
			sprintf(tmp,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s",
				(lightIndirect1 || force2d || softLight<0)?1:MIN(useLights,INSTANCES_PER_PASS),
				lightIndirect1?0:((force2d || softLight<0)?1:shadowSamples),
				lightIndirect1?"":"#define LIGHT_DIRECT\n#define LIGHT_DIRECT_MAP\n",
				lightIndirectColor1?"#define LIGHT_INDIRECT_COLOR\n":"",
				lightIndirectMap1?"#define LIGHT_INDIRECT_MAP\n":"",
				(renderDiffuseTexture && !hasDiffuseColor)?"#define MATERIAL_DIFFUSE_MAP\n":"",
				(hasDiffuseColor)?"#define MATERIAL_DIFFUSE_COLOR\n":"",
				(force2d)?"#define FORCE_2D_POSITION\n":""
				);
			GLSLProgram* prog = progSet->getVariant(tmp);
			if(!prog && INSTANCES_PER_PASS>1)
			{
				printf("MAPS_PER_PASS=%d too much, trying %d...\n",INSTANCES_PER_PASS,INSTANCES_PER_PASS-1);
				INSTANCES_PER_PASS--;
				useLights = initialPasses*INSTANCES_PER_PASS;
				needDepthMapUpdate = 1; // zmenilo se rozlozeni instanci v prostoru
				goto retry;
			}
			return prog;
			}
		default:
			assert(0);
			return NULL;
	}
}
GLSLProgram* getProgram(RRObjectRenderer::ColorChannel cc)
{
	checkGlError();
	GLSLProgram* tmp = getProgramCore(cc);
	checkGlError();
	if(!tmp)
	{
		printf("getProgram failed, your card and driver is not capable enough.\nTry to update driver or simplify program.");
		fgetc(stdin);
		exit(0);
	}
	return tmp;
}

GLSLProgram* setProgram(RRObjectRenderer::ColorChannel cc)
{
	GLSLProgram* tmp = getProgram(cc);
	checkGlError();
	tmp->useIt();
	checkGlError();
	return tmp;
}

void drawScene(RRObjectRenderer::ColorChannel cc)
{
	checkGlError();
#ifdef _3DS
	if(!renderOnlyRr && cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(NULL,(useLights<=INSTANCES_PER_PASS)?app:NULL,lightIndirectMap); // optimalizovay render->s ambientem, jinak bez
		return;
	}
	if(!renderOnlyRr && (cc==RRObjectRenderer::CC_SOURCE_IRRADIANCE || cc==RRObjectRenderer::CC_REFLECTED_IRRADIANCE))
	{
		m3ds.Draw(NULL,app,lightIndirectMap);
		return;
	}
#endif
	renderer->render(cc);
	checkGlError();
}

void setProgramAndDrawScene(RRObjectRenderer::ColorChannel cc)
{
	if(cc==RRObjectRenderer::CC_REFLECTED_AUTO)
	{
		if(renderDiffuseTexture)
			cc = RRObjectRenderer::CC_REFLECTED_IRRADIANCE; // for textured 3ds output
		else
			cc = RRObjectRenderer::CC_REFLECTED_EXITANCE; // for colored output
	}
	if(cc==RRObjectRenderer::CC_SOURCE_AUTO)
	{
		if(renderDiffuseTexture)
			cc = RRObjectRenderer::CC_SOURCE_IRRADIANCE; // for textured 3ds output
		else
			cc = RRObjectRenderer::CC_SOURCE_EXITANCE; // for colored output
	}
	setProgram(cc);
	drawScene(cc);
}

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void drawLight(void)
{
	ambientProg->useIt();
	glPushMatrix();
	glTranslatef(light.pos[0]-0.3*light.dir[0], light.pos[1]-0.3*light.dir[1], light.pos[2]-0.3*light.dir[2]);
	glColor3f(1,1,0);
	gluSphere(q, 0.05, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	eye.dir[0] = 3*sin(eye.angle);
	eye.dir[1] = -0.3*eye.height;
	eye.dir[2] = 3*cos(eye.angle);
	eye.updateViewMatrix(0);

	light.dir[0] = 3*sin(light.angle);
	light.dir[1] = -0.3*light.height;
	light.dir[2] = 3*cos(light.angle);
	light.dir[3] = 1.0;
	light.updateViewMatrix(0.3f);

	buildPerspectiveMatrix(lightFrustumMatrix, 
		lightFieldOfView, 1.0, lightNear, lightFar);

	if (showLightViewFrustum) {
		invertMatrix(lightInverseViewMatrix, light.viewMatrix);
		invertMatrix(lightInverseFrustumMatrix, lightFrustumMatrix);
	}
}

void setupEyeView(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(eyeFrustumMatrix);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(eye.viewMatrix);
}

/* drawShadowMapFrustum - Draw dashed lines around the light's view
   frustum to help visualize the region captured by the depth map. */
void drawShadowMapFrustum(void)
{
	/* Go from light clip space, ie. the cube [-1,1]^3, to world space by
	transforming each clip space cube corner by the inverse of the light
	frustum transform, then by the inverse of the light view transform.
	This results in world space coordinate that can then be transformed
	by the eye view transform and eye projection matrix and on to the
	screen. */

	ambientProg->useIt();

	if (showLightViewFrustum) {
		glEnable(GL_LINE_STIPPLE);
		glPushMatrix();
		glMultMatrixd(lightInverseViewMatrix);
		glMultMatrixd(lightInverseFrustumMatrix);
		glColor3f(1,1,1);
		/* Draw a wire frame cube with vertices at the corners
		of clip space.  Draw the top square, drop down to the
		bottom square and finish it, then... */
		glBegin(GL_LINE_STRIP);
		glVertex3f(1,1,1);
		glVertex3f(1,1,-1);
		glVertex3f(-1,1,-1);
		glVertex3f(-1,1,1);
		glVertex3f(1,1,1);
		glVertex3f(1,-1,1);
		glVertex3f(-1,-1,1);
		glVertex3f(-1,-1,-1);
		glVertex3f(1,-1,-1);
		glVertex3f(1,-1,1);
		glEnd();
		/* Draw the final three line segments connecting the top
		and bottom squares. */
		glBegin(GL_LINES);
		glVertex3f(1,1,-1);
		glVertex3f(1,-1,-1);

		glVertex3f(-1,1,-1);
		glVertex3f(-1,-1,-1);

		glVertex3f(-1,1,1);
		glVertex3f(-1,-1,1);
		glEnd();
		glPopMatrix();
		glDisable(GL_LINE_STIPPLE);
	}
}

void setupLightView(int square)
{
	glMatrixMode(GL_PROJECTION);
	if (square) {
		glLoadMatrixd(lightFrustumMatrix);
	} else {
		glLoadIdentity();
		glScalef(winAspectRatio, 1, 1);
		glMultMatrixd(lightFrustumMatrix);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(light.viewMatrix);
}

void drawLightView(void)
{
	drawScene(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE);
}

void useBestShadowMapClamping(GLenum target)
{
	if (hasTextureBorderClamp) {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB);
	} else {
		/* We really want "clamp to border", but this may be good enough. */
		if (hasTextureEdgeClamp) {
			glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} else {
			/* A bad option, but still better than "repeat". */
			glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
	}
}

void placeSoftLight(int n)
{
	//printf("%d ",n);
	softLight=n;
	static float oldLightAngle,oldLightHeight;
	if(n==-1) { // init, before all
		oldLightAngle=light.angle;
		oldLightHeight=light.height;
		return;
	}
	if(n==-2) { // done, after all
		light.angle=oldLightAngle;
		light.height=oldLightHeight;
		updateMatrices();
		return;
	}
	// place one point light approximating part of area light
	if(useLights>1) {
		switch(areaType) {
	  case 0: // linear
		  light.angle=oldLightAngle+2*AREA_SIZE*(n/(useLights-1.)-0.5);
		  light.height=oldLightHeight-0.4*n/useLights;
		  break;
	  case 1: // rectangular
		  {int q=(int)sqrtf(useLights-1)+1;
		  light.angle=oldLightAngle+AREA_SIZE*(n/q/(q-1.)-0.5);
		  light.height=oldLightHeight+(n%q/(q-1.)-0.5);}
		  break;
	  case 2: // circular
		  light.angle=oldLightAngle+sin(n*2*3.14159/useLights)*0.5*AREA_SIZE;
		  light.height=oldLightHeight+cos(n*2*3.14159/useLights)*0.5;
		  break;
		}
		updateMatrices();
	}
}

void updateDepthMap(int mapIndex)
{
	if(!needDepthMapUpdate) return;

	if(mapIndex>=0)
	{
		placeSoftLight(mapIndex);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	setupLightView(1);

	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glEnable(GL_POLYGON_OFFSET_FILL);

	setProgramAndDrawScene(RRObjectRenderer::CC_NO_COLOR);
	updateShadowTex();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);

	if(softLight<0 || softLight==(int)(useLights-1)) 
	{
		needDepthMapUpdate = 0;
	}
}

void drawHardwareShadowPass(RRObjectRenderer::ColorChannel cc)
{
	checkGlError();
	GLSLProgram* myProg = setProgram(cc);
	checkGlError();

	// shadowMap[], gl_TextureMatrix[]
	glMatrixMode(GL_TEXTURE);
	checkGlError();
	GLdouble tmp[16]={
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		1,1,1,2
	};
	GLint samplers[100];
	int softLightBase = softLight;
	int instances = (softLight>=0 && cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE)?MIN(useLights,INSTANCES_PER_PASS):1;
	checkGlError();
	for(int i=0;i<instances;i++)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB+i);
		// prepare samplers
		glBindTexture(GL_TEXTURE_2D, shadowTex[((softLight>=0)?softLightBase:0)+i]);
		samplers[i]=i;
		// prepare and send matrices
		if(i && softLight>=0)
			placeSoftLight(softLightBase+i); // calculate light position
		glLoadMatrixd(tmp);
		glMultMatrixd(lightFrustumMatrix);
		glMultMatrixd(light.viewMatrix);
	}
	checkGlError();
	myProg->sendUniform("shadowMap", instances, samplers);
	checkGlError();
	glMatrixMode(GL_MODELVIEW);

	// lightDirectPos (in object space)
	myProg->sendUniform("lightDirectPos",light.pos[0],light.pos[1],light.pos[2]);

	// lightDirectMap
	activateTexture(GL_TEXTURE10_ARB, GL_TEXTURE_2D);
	lightDirectMap->bindTexture();
	myProg->sendUniform("lightDirectMap", 10);

	// lightIndirectMap
	if(lightIndirectMap)
	{
		activateTexture(GL_TEXTURE12_ARB, GL_TEXTURE_2D);
		myProg->sendUniform("lightIndirectMap", 12);
	}

	// materialDiffuseMap (last before drawScene, must stay active)
	if(renderDiffuseTexture && cc!=RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION) // kdyz detekuju source (->force 2d), pouzivam RRObjectRenderer, takze jedem bez difus textur
	{
		activateTexture(GL_TEXTURE11_ARB, GL_TEXTURE_2D);
		myProg->sendUniform("materialDiffuseMap", 11);
	}

	drawScene(cc);

	glActiveTextureARB(GL_TEXTURE0_ARB);
}

void drawEyeViewShadowed()
{
	if(softLight<0) updateDepthMap(softLight);

	if(softLight<=0) glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	setupEyeView();

	drawHardwareShadowPass(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE);

	drawLight();
	drawShadowMapFrustum();
}

void drawEyeViewSoftShadowed(void)
{
	// optimized path without accum
	if(useLights<=INSTANCES_PER_PASS)
	{
		placeSoftLight(-1);
		for(unsigned i=0;i<useLights;i++)
		{
			updateDepthMap(i);
		}
		if (wireFrame) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		placeSoftLight(0);
		drawEyeViewShadowed();
		placeSoftLight(-2);
		return;
	}

	// add direct
	placeSoftLight(-1);
	for(unsigned i=0;i<useLights;i++)
	{
		updateDepthMap(i);
	}
	glClear(GL_ACCUM_BUFFER_BIT);
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	for(unsigned i=0;i<useLights;i+=INSTANCES_PER_PASS)
	{
		placeSoftLight(i);
		drawEyeViewShadowed();
		glAccum(GL_ACCUM,1./(useLights/INSTANCES_PER_PASS));
	}
	placeSoftLight(-2);

	// add indirect
	{
#ifdef SHOW_CAPTURED_TRIANGLES
		generateForcedUv = &captureUv;
		captureUv.firstCapturedTriangle = 0;
		setProgramAndDrawScene(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION); // pro color exitance, pro texturu irradiance
#else
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		setProgramAndDrawScene(RRObjectRenderer::CC_REFLECTED_AUTO); // pro color exitance, pro texturu irradiance
#endif

		glAccum(GL_ACCUM,1);
	}

	glAccum(GL_RETURN,1);
}

void capturePrimary()
{
	app->reportLightChange();
}

static void output(int x, int y, char *string)
{
	int len, i;

	glRasterPos2f(x, y);
	len = (int) strlen(string);
	for (i = 0; i < len; i++) {
		glutBitmapCharacter(font, string[i]);
	}
}

static void drawHelpMessage(bool big)
{
	static char *message[] = {
		"Realtime Radiosity Viewer",
		" Stepan Hrbek, http://dee.cz",
		" radiosity engine, http://lightsprint.com",
		"",
		"mouse+left button - manipulate camera",
		"mouse+mid button  - manipulate light",
		"right button      - menu",
		"arrows            - move camera or light (with middle mouse pressed)",
		"",
		"space - toggle global illumination",
		"'+ -' - increase/decrease penumbra (soft shadow) precision",
		"'* /' - increase/decrease penumbra (soft shadow) smoothness",
		"'a'   - cycle through linear, rectangular and circular area light",
		"'z'   - toggle zoom",
		"'w'   - toggle wire frame",
		"'f'   - toggle showing spotlight frustum",
		"'m'   - toggle whether the left or middle mouse buttons control the eye and",
		"        view positions (helpful for systems with only a two-button mouse)",
		"",
		"'p'   - narrow shadow frustum field of view",
		"'P'   - widen shadow frustum field of view",
		"'n'   - compress shadow frustum near clip plane",
		"'N'   - expand shadow frustum near clip plane",
		"'c'   - compress shadow frustum far clip plane",
		"'C'   - expand shadow frustum far clip plane",
		"'b'   - increment the depth bias for 1st pass glPolygonOffset",
		"'B'   - decrement the depth bias for 1st pass glPolygonOffset",
		"'q'   - increment depth slope for 1st pass glPolygonOffset",
		"'Q'   - increment depth slope for 1st pass glPolygonOffset",
		NULL,
		"h - help",
		NULL
	};
	int i;
	int x = 40, y= 42;

	ambientProg->useIt();

	glActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);

	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winWidth, winHeight, 0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glColor4f(0.0,0.0,0.0,0.6);

	/* Drawn clockwise because the flipped Y axis flips CCW and CW. */
	if(big) glRecti(winWidth - 30, 30, 30, winHeight - 30);

	glDisable(GL_BLEND);

	glColor3f(1,1,1);
	for(i=0; message[i] != NULL; i++) 
	{
		if(!big) i=sizeof(message)/sizeof(char*)-2;
		if (message[i][0] == '\0') {
			y += 7;
		} else {
			output(x, y, message[i]);
			y += 14;
		}
	}

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
}

void display(void)
{
	if(needTitleUpdate)
		updateTitle();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(needMatrixUpdate)
		updateMatrices();

	switch(drawMode)
	{
		case DM_LIGHT_VIEW:
			setupLightView(0);
			if (wireFrame) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
			drawLightView();
			break;
		case DM_EYE_VIEW_SHADOWED:
			/* Wire frame handled internal to this routine. */
			drawEyeViewShadowed();
			break;
		case DM_EYE_VIEW_SOFTSHADOWED:
			drawEyeViewSoftShadowed();
			break;
		default:
			assert(0);
			break;
	}

	if(wireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	drawHelpMessage(showHelp);

	if(!drawFront)
		glutSwapBuffers();

	static bool captured=false; if(!captured) {captured=true;capturePrimary();}
}

static void benchmark(int perFrameDepthMapUpdate)
{
	const int numFrames = 150;
	int precision = 0;
	int start, stop;
	float time;
	int i;

	needDepthMapUpdate = 1;
	display();

	printf("starting benchmark...\n");
	glFinish();

	start = glutGet(GLUT_ELAPSED_TIME);
	for (i=0; i<numFrames; i++) {
		if (perFrameDepthMapUpdate) {
			needDepthMapUpdate = 1;
		}
		display();
	}
	glFinish();
	stop = glutGet(GLUT_ELAPSED_TIME);

	time = (stop - start)/1000.0;

	printf("  perFrameDepthMapUpdate=%d, time = %f secs, fps = %f\n",
		perFrameDepthMapUpdate, time, numFrames/time);
	printf("  TEX2D %dx%d:%d\n",
		depthMapSize, depthMapSize, precision);
	needDepthMapUpdate = 1;
}

void switchMouseControl(void)
{
	if (eyeButton == GLUT_LEFT_BUTTON) {
		eyeButton = GLUT_MIDDLE_BUTTON;
		lightButton = GLUT_LEFT_BUTTON;
	} else {
		lightButton = GLUT_MIDDLE_BUTTON;
		eyeButton = GLUT_LEFT_BUTTON;
	}
	movingEye = 0;
	movingLight = 0;
}

void toggleWireFrame(void)
{
	wireFrame = !wireFrame;
	if (wireFrame) {
		glClearColor(0.1,0.2,0.2,0);
	} else {
		glClearColor(0,0,0,0);
	}
	glutPostRedisplay();
}

void updateDepthBias(int delta)
{
	depthBias24 += delta;
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
}

void toggleGlobalIllumination()
{
	if(drawMode == DM_EYE_VIEW_SHADOWED)
		drawMode = DM_EYE_VIEW_SOFTSHADOWED;
	else
		drawMode = DM_EYE_VIEW_SHADOWED;
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
}

void toggleTextures()
{
	renderOnlyRr = !renderOnlyRr;
	renderDiffuseTexture = !renderDiffuseTexture;
}

void selectMenu(int item)
{
	app->reportInteraction();
	switch (item) {
  case ME_EYE_VIEW_SHADOWED:
	  drawMode = DM_EYE_VIEW_SHADOWED;
	  needTitleUpdate = 1;
	  break;
  case ME_EYE_VIEW_SOFTSHADOWED:
	  drawMode = DM_EYE_VIEW_SOFTSHADOWED;
	  needTitleUpdate = 1;
	  break;
  case ME_LIGHT_VIEW:
	  drawMode = DM_LIGHT_VIEW;
	  needTitleUpdate = 1;
	  break;
  case ME_SWITCH_MOUSE_CONTROL:
	  switchMouseControl();
	  return;  /* No redisplay needed. */

  case ME_TOGGLE_GLOBAL_ILLUMINATION:
	  toggleGlobalIllumination();
	  return;
  case ME_TOGGLE_WIRE_FRAME:
	  toggleWireFrame();
	  return;
  case ME_TOGGLE_LIGHT_FRUSTUM:
	  showLightViewFrustum = !showLightViewFrustum;
	  if (showLightViewFrustum) {
		  needMatrixUpdate = 1;
	  }
	  break;
  case ME_EXIT:
	  exit(0);
	  break;
  default:
	  assert(0);
	  break;
	}
	glutPostRedisplay();
}

void special(int c, int x, int y)
{
	app->reportInteraction();
	SimpleCamera* cam = movingLight?&light:&eye;
	switch (c) 
	{
		case GLUT_KEY_F7:
			benchmark(1);
			return;
		case GLUT_KEY_F8:
			benchmark(0);
			return;
		case GLUT_KEY_F9:
			printf("\nSimpleCamera eye = {{%f,%f,%f},%f,%f};\n",eye.pos[0],eye.pos[1],eye.pos[2],eye.angle,eye.height);
			printf("SimpleCamera light = {{%f,%f,%f},%f,%f};\n",light.pos[0],light.pos[1],light.pos[2],light.angle,light.height);
			return;

		case GLUT_KEY_UP:
			for(int i=0;i<3;i++) cam->pos[i]+=cam->dir[i]/20;
			break;
		case GLUT_KEY_DOWN:
			for(int i=0;i<3;i++) cam->pos[i]-=cam->dir[i]/20;
			break;
		case GLUT_KEY_LEFT:
			cam->pos[0]+=cam->dir[2]/20;
			cam->pos[2]-=cam->dir[0]/20;
			break;
		case GLUT_KEY_RIGHT:
			cam->pos[0]-=cam->dir[2]/20;
			cam->pos[2]+=cam->dir[0]/20;
			break;

		default:
			return;
	}
	/*if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
	switch (c) {
	case GLUT_KEY_UP:
	break;
	case GLUT_KEY_DOWN:
	break;
	case GLUT_KEY_LEFT:
	break;
	case GLUT_KEY_RIGHT:
	break;
	}
	return;
	}*/
	needMatrixUpdate = 1;
	if(movingLight) 
	{
		needDepthMapUpdate = 1;
	}
	glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
	app->reportInteraction();
	switch (c) {
		case 27:
			//delete app; throws asser in freeing node from ivertex
			exit(0);
			break;
		case 'b':
			updateDepthBias(+1);
			break;
		case 'B':
			updateDepthBias(-1);
			break;
		case 'l':
		case 'L':
			drawMode = DM_LIGHT_VIEW;
			needTitleUpdate = 1;
			break;
		case 'm':
			switchMouseControl();
			break;
		case 'h':
		case 'H':
			showHelp = !showHelp;
			break;
		case 'f':
		case 'F':
			showLightViewFrustum = !showLightViewFrustum;
			if (showLightViewFrustum) {
				needMatrixUpdate = 1;
			}
			break;
		case 'z':
		case 'Z':
			eyeFieldOfView = (eyeFieldOfView == 100.0) ? 50.0 : 100.0;
			buildPerspectiveMatrix(eyeFrustumMatrix,
				eyeFieldOfView, 1.0/winAspectRatio, eyeNear, eyeFar);
			break;
		case 'q':
			slopeScale += 0.1;
			needDepthMapUpdate = 1;
			needTitleUpdate = 1;
			updateDepthBias(0);
			break;
		case 'Q':
			slopeScale -= 0.1;
			if (slopeScale < 0.0) {
				slopeScale = 0.0;
			}
			needDepthMapUpdate = 1;
			needTitleUpdate = 1;
			updateDepthBias(0);
			break;
		case '>':
			textureLodBias += 0.2;
			break;
		case '<':
			textureLodBias -= 0.2;
			break;
		case 'w':
		case 'W':
			toggleWireFrame();
			return;
		case 'n':
			lightNear *= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'N':
			lightNear /= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'c':
			lightFar *= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'C':
			lightFar /= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'p':
			lightFieldOfView -= 5.0;
			if (lightFieldOfView < 5.0) {
				lightFieldOfView = 5.0;
			}
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'P':
			lightFieldOfView += 5.0;
			if (lightFieldOfView > 160.0) {
				lightFieldOfView = 160.0;
			}
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case ' ':
			toggleGlobalIllumination();
			break;
		case 't':
			toggleTextures();
			break;
		case '*':
			if(shadowSamples<4) shadowSamples *= 2;
			break;
		case '/':
			if(shadowSamples>1) shadowSamples /= 2;
			break;
		case '+':
			if(useLights+INSTANCES_PER_PASS<=MAX_INSTANCES) 
			{
				if(useLights==1) 
					useLights = INSTANCES_PER_PASS;
				else
					useLights += INSTANCES_PER_PASS;
				needDepthMapUpdate = 1;
			}
			break;
		case '-':
			if(useLights>1) 
			{
				if(useLights>INSTANCES_PER_PASS) 
					useLights -= INSTANCES_PER_PASS;
				else
					useLights = 1;
				needDepthMapUpdate = 1;
			}
			break;
		case 'a':
			++areaType%=3;
			needDepthMapUpdate = 1;
			break;
		default:
			return;
	}
	glutPostRedisplay();
}

void updateDepthScale(void)
{
	GLint depthBits;

	glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	if (depthBits < 24) {
		depthScale24 = 1;
	} else {
		depthScale24 = 1 << (depthBits - 24+8); // "+8" hack: on gf6600, 24bit depth seems to need +8 scale
	}
	needDepthMapUpdate = 1;
}

void reshape(int w, int h)
{
	if(w<512 || h<512)
		printf("Window size<512 not supported in this demo, expect incorrect render!\n");
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	winAspectRatio = (double) winHeight / (double) winWidth;
	buildPerspectiveMatrix(eyeFrustumMatrix,
		eyeFieldOfView, 1.0/winAspectRatio, eyeNear, eyeFar);

	/* Perhaps there might have been a mode change so at window
	reshape time, redetermine the depth scale. */
	updateDepthScale();
}

void initGL(void)
{
	GLint depthBits;

#if defined(_WIN32)
	if (vsync) {
		wglSwapIntervalEXT(1);
	} else {
		wglSwapIntervalEXT(0);
	}
#endif

	glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	//printf("depth buffer precision = %d\n", depthBits);

	glClearColor(0,0,0,0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	/* GL_LEQUAL ensures that when fragments with equal depth are
	generated within a single rendering pass, the last fragment
	results. */
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	glLineStipple(1, 0xf0f0);

	glEnable(GL_CULL_FACE);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
#if 000
	/* This would work too. */
	glEnable(GL_RESCALE_NORMAL_EXT);
#else
	glEnable(GL_NORMALIZE);
#endif

	q = gluNewQuadric();
	if (useDisplayLists) {

		/* Make a sphere display list. */
		glNewList(DL_SPHERE, GL_COMPILE);
		drawSphere();
		glEndList();
	}

	updateDepthScale();
	updateDepthBias(0);  /* Update with no offset change. */

	if (drawFront) {
		glDrawBuffer(GL_FRONT);
		glReadBuffer(GL_FRONT);
	}
}

void mouse(int button, int state, int x, int y)
{
	app->reportInteraction();
	if (button == eyeButton && state == GLUT_DOWN) {
		movingEye = 1;
		xEyeBegin = x;
		yEyeBegin = y;
	}
	if (button == eyeButton && state == GLUT_UP) {
		//!!!lastInteractionTime = 0;
		movingEye = 0;
	}
	if (button == lightButton && state == GLUT_DOWN) {
		movingLight = 1;
		xLightBegin = x;
		yLightBegin = y;
	}
	if (button == lightButton && state == GLUT_UP) {
		//!!!lastInteractionTime = 0;
		movingLight = 0;
		needDepthMapUpdate = 1;
		capturePrimary();
		glutPostRedisplay();
	}
}

void passivemotion(int x, int y)
{
}

void motion(int x, int y)
{
	app->reportInteraction();
	if (movingEye) {
		eye.angle = eye.angle - 0.005*(x - xEyeBegin);
		eye.height = eye.height + 0.15*(y - yEyeBegin);
		if (eye.height > 10.0) eye.height = 10.0;
		if (eye.height < -10.0) eye.height = -10.0;
		xEyeBegin = x;
		yEyeBegin = y;
		needMatrixUpdate = 1;
		glutPostRedisplay();
	}
	if (movingLight) {
		light.angle = light.angle - 0.005*(x - xLightBegin);
		light.height = light.height + 0.15*(y - yLightBegin);
		if (light.height > 10.0) light.height = 10.0;
		if (light.height < -10.0) light.height = -10.0;
		xLightBegin = x;
		yLightBegin = y;
		needMatrixUpdate = 1;
		needDepthMapUpdate = 1;
		glutPostRedisplay();
	}
}

void depthBiasSelect(int depthBiasOption)
{
	depthBias24 = depthBiasOption;
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
	glutPostRedisplay();
}

void initMenus(void)
{
	glutCreateMenu(selectMenu);
	glutAddMenuEntry("[ ] Toggle global illumination", ME_TOGGLE_GLOBAL_ILLUMINATION);
	glutAddMenuEntry("[f] Toggle light frustum", ME_TOGGLE_LIGHT_FRUSTUM);
	glutAddMenuEntry("[w] Toggle wire frame", ME_TOGGLE_WIRE_FRAME);
	glutAddMenuEntry("[m] Switch mouse buttons", ME_SWITCH_MOUSE_CONTROL);
	glutAddMenuEntry("[ESC] Quit", ME_EXIT);

	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void parseOptions(int argc, char **argv)
{
	int i;

	for (i=1; i<argc; i++) 
	{
		{
			int tmp;
			if(sscanf(argv[i],"%d",&tmp)==1)
			{
				if(tmp>=1 && tmp<=MAX_INSTANCES_PER_PASS) 
				{
					INSTANCES_PER_PASS = tmp;
					useLights = INSTANCES_PER_PASS;
				}
				else
					printf("Out of range, 1 to 10 allowed.\n");
			}
		}
		if (!strcmp("-forcerun", argv[i])) {
			forcerun = 1;
		}
		if (!strcmp("-window", argv[i])) {
			fullscreen = 0;
		}
		if (strstr(argv[i], ".mgf")) {
			mgf_filename = argv[i];
		}
#ifdef _3DS
		if (strstr(argv[i], ".3ds") || strstr(argv[i], ".3DS")) {
			filename_3ds = argv[i];
			scale_3ds = 1;
		}
#endif
		if (!strcmp("-depth24", argv[i])) {
			useDepth24 = 1;
		}
		if (!strcmp("-vsync", argv[i])) {
			vsync = 1;
		}
		if (!strcmp("-front", argv[i])) {
			drawFront = 1;
		}
	}
}

void idle()
{
	if(app->calculate()==rrVision::RRScene::IMPROVED)
		glutPostRedisplay();
}

int main(int argc, char **argv)
{
	rrVision::RRLicense::registerLicense("","");

	glutInitWindowSize(800, 600);
	glutInit(&argc, argv);
	parseOptions(argc, argv);

	if (useDepth24) {
		glutInitDisplayString("depth~24 rgb double accum");
	} else {
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
	}
	glutCreateWindow("shadowcast");
	if (fullscreen) glutFullScreen();

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		// Problem: glewInit failed, something is seriously wrong.
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		return 1;
	}
	//fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passivemotion);
	glutMotionFunc(motion);
	glutIdleFunc(idle);

	/* Menu initialization depends on knowing what extensions are
	supported. */
	initMenus();

	initGL();

	if(!forcerun && !supports20())
	{
		puts("At least OpenGL 2.0 required.");
		return 0;
	}

	if (strstr(filename_3ds, "koupelna3")) {
		scale_3ds = 0.01f;
	}
	if (strstr(filename_3ds, "koupelna4")) {
		scale_3ds = 0.03f;
		eyeFieldOfView = 50.0;
		//SimpleCamera koupelna4_eye = {{0.032202,1.659255,1.598609},10.010005,-0.150000};
		//SimpleCamera koupelna4_light = {{-1.309976,0.709500,0.498725},3.544996,-10.000000};
		SimpleCamera koupelna4_eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003};
		SimpleCamera koupelna4_light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996};		eye = koupelna4_eye;
		light = koupelna4_light;
	}
	if (strstr(filename_3ds, "sponza"))
	{
		SimpleCamera sponza_eye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999};
		SimpleCamera sponza_light = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001};
		//SimpleCamera sponza_eye = {{-10.407576,1.605258,4.050256},7.859994,-0.050000};
		//SimpleCamera sponza_light = {{-7.109047,5.130751,-2.025017},0.404998,2.950001};
		//lightFieldOfView += 10.0;
		//eyeFieldOfView = 50.0;
		eye = sponza_eye;
		light = sponza_light;
	}

	//if(rrobject) printf("vertices=%d triangles=%d\n",rrobject->getCollider()->getImporter()->getNumVertices(),rrobject->getCollider()->getImporter()->getNumTriangles());
	rrVision::RRScene::setStateF(rrVision::RRScene::SUBDIVISION_SPEED,0);
	rrVision::RRScene::setState(rrVision::RRScene::GET_SOURCE,0);
	rrVision::RRScene::setState(rrVision::RRScene::GET_REFLECTED,1);
	//rrVision::RRScene::setState(rrVision::GET_SMOOTH,0);
	rrVision::RRScene::setStateF(rrVision::RRScene::MIN_FEATURE_SIZE,0.15f);
	//rrVision::RRScene::setStateF(rrVision::MAX_SMOOTH_ANGLE,0.4f);

	printf("Loading and preprocessing scene (~15 sec)...");
#ifdef _3DS
	// load 3ds
	if(!m3ds.Load(filename_3ds,scale_3ds)) return 1;
	//m3ds.shownormals=1;
	//m3ds.numObjects=2;//!!!
	app = new MyApp();
	new_3ds_importer(&m3ds,app);
#else
	// load mgf
	rrobject = new_mgf_importer(mgf_filename)->createAdditionalIllumination();
#endif
	printf("\n");
	glsl_init();
	checkGlError();
	renderer = NULL;
	app->calculate();
	renderer = new RRCachingRenderer(new RRGLObjectRenderer(app->multiObject,app->scene));

	glutMainLoop();
	return 0;
}
