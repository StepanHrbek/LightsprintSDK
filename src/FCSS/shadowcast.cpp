//!!! fudge factors
#define INDIRECT_RENDER_FACTOR 3

#define _3DS
//#define SHOW_CAPTURED_TRIANGLES
//#define DEFAULT_SPONZA
#define MAX_INSTANCES 5  // max number of light instances aproximating one area light
#define START_INSTANCES 8 // initial number of instances

/*
bataky na koupelnu i sponzu
readme
web
stranka s vic demacema pohromade
napsat hernim firmam
nacitat jpg
spatne pocita sponza kolem degeneratu
spatne pocita sponza podlahu

proc je nutno *3? nekde se asi spatne pocita r+g+b misto (r+g+b)/3
autodeteknout zda mam metry nebo centimetry
dodelat podporu pro matice do 3ds2rr importeru
z mgf ze zahadnych duvodu zmizel color bleeding
kdyz uz by byl korektni model s gammou, pridat ovladani gammy

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce
 kdyz se vypne scaler(0.4) nebo indirect, primary vypada desne tmave
 az pri secteni s indirectem (scitani produkuje prilis velke cislo) zacne vypadat akorat
*/

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/glut.h>

#include <iostream>
#include "glsl/Light.hpp"
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
// time

#include <time.h>
#define TIME    clock_t            
#define GETTIME clock()
#define PER_SEC CLOCKS_PER_SEC

#define TIME_TO_START_IMPROVING 0.3f
TIME lastInteractionTime = 0;


void checkGlError()
{
	GLenum err = glGetError();
	if(err!=GL_NO_ERROR)
	{
		printf("glGetError=%x\n",err);
	}
}

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
	char* filename_3ds="koupelna\\koupelna3.3ds";
	float scale_3ds = 0.01f;
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
//
// RR

bool renderOnlyRr = false;
bool renderSource = false;
RRCachingRenderer* renderer = NULL;
rrVision::RRAdditionalObjectImporter* rrobject = NULL;
rrVision::RRScene* rrscene = NULL;
rrVision::RRScaler* rrscaler = NULL;
float rrtimestep;
// rr endfunc callback
static bool endByTime(void *context)
{
	return GETTIME>(TIME)(intptr_t)context;
}
rrVision::RRColor* indirectColors = NULL;
#ifdef _3DS
void updateIndirect()
{
	if(!rrobject || !rrscene) return;

	rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,renderSource?1:0);
	rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,renderSource?0:1);

	unsigned firstTriangleIdx[1000];//!!!
	unsigned firstVertexIdx[1000];
	unsigned triIdx = 0;
	unsigned vertIdx = 0;
	for(unsigned obj=0;obj<(unsigned)m3ds.numObjects;obj++)
	{
		firstTriangleIdx[obj] = triIdx;
		firstVertexIdx[obj] = vertIdx;
		for (int j = 0; j < m3ds.Objects[obj].numMatFaces; j ++)
		{
			unsigned numTriangles = m3ds.Objects[obj].MatFaces[j].numSubFaces/3;
			triIdx += numTriangles;
		}
		vertIdx += m3ds.Objects[obj].numVerts;
	}

	rrCollider::RRMeshImporter* mesh = rrobject->getCollider()->getImporter();
	unsigned numVertices = vertIdx;//mesh->getNumVertices();
	delete[] indirectColors;
	indirectColors = new rrVision::RRColor[numVertices];
	for(unsigned i=0;i<numVertices;i++)
		indirectColors[i] = rrVision::RRColor(0);//!!! mozna zbytecne

	struct PreImportNumber 
		// our structure of pre import number (it is independent for each implementation)
		// (on the other hand, postimport is always plain unsigned, 0..num-1)
		// underlying importers must use preImport values that fit into index, this is not runtime checked
	{
		unsigned index : sizeof(unsigned)*8-12; // 32bit: max 1M triangles/vertices in one object
		unsigned object : 12; // 32bit: max 4k objects
		PreImportNumber() {}
		PreImportNumber(unsigned i) {*(unsigned*)this = i;} // implicit unsigned -> PreImportNumber conversion
		operator unsigned () {return *(unsigned*)this;} // implicit PreImportNumber -> unsigned conversion
	};

	unsigned numTriangles = mesh->getNumTriangles();
	for(unsigned t=0;t<numTriangles;t++) // triangle
	{
		rrCollider::RRMeshImporter::Triangle triangle;
		mesh->getTriangle(t,triangle);
		for(unsigned v=0;v<3;v++) // vertex
		{
			// get 1*irradiance
			rrVision::RRColor indirect;
			rrscene->getTriangleMeasure(0,t,v,rrVision::RM_IRRADIANCE,indirect);// je pouzito vzdy pro 3ds vystup, toto se jeste prenasobi difusni texturou
			// write 1*irradiance
			indirect *= INDIRECT_RENDER_FACTOR;
			for(unsigned i=0;i<3;i++)
			{
				assert(_finite(indirect[i]));
				assert(indirect[i]>=0);
				assert(indirect[i]<1500000);
			}
			PreImportNumber pre = mesh->getPreImportVertex(triangle[v],t);
			unsigned preVertexIdx = firstVertexIdx[pre.object]+pre.index;
			assert(preVertexIdx<numVertices);
			indirectColors[preVertexIdx] = indirect;
		}
	}
}
#endif


/////////////////////////////////////////////////////////////////////////////
//
// GLSL

GLSLProgram *shadowProg, *lightProg, *ambientProg, *ambientDifMProg;
GLSLProgramSet* shadowDifCProgSet;
GLSLProgramSet* shadowDifMProgSet;
Texture *lightTex;
FrameRate *counter;
unsigned int shadowTex[MAX_INSTANCES];
int currentWindowSize;
#define SHADOW_MAP_SIZE 512
int softLight = -1; // current instance number 0..199, -1 = hard shadows, use instance 0

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
	shadowProg = new GLSLProgram(NULL,"shaders\\shadow.vp", "shaders\\shadow.fp");
	shadowDifCProgSet = new GLSLProgramSet("shaders\\shadow_DifC.vp", "shaders\\shadow_DifC.fp");
	shadowDifMProgSet = new GLSLProgramSet("shaders\\shadow_DifM.vp", "shaders\\shadow_DifM.fp");

/*
	shadowDifMProg = new GLSLProgram();
	GLSLShader* shv1 = new GLSLShader("shaders\\shadow_DifM.vp",GL_VERTEX_SHADER_ARB);
	GLSLShader* shf = new GLSLShader("shaders\\shadow_DifM.fp",GL_FRAGMENT_SHADER_ARB);
	GLSLShader* shv2 = new GLSLShader("shaders\\overwrite_pos.vp",GL_VERTEX_SHADER_ARB);
	shadowDifMProg->attach(shv2);
	shadowDifMProg->attach(shv1);
	shadowDifMProg->attach(shf);
	shadowDifMProg->linkIt();
*/
	lightProg = new GLSLProgram(NULL,"shaders\\light.vp");
	ambientProg = new GLSLProgram(NULL,"shaders\\ambient.vp", "shaders\\ambient.fp");
	ambientDifMProg = new GLSLProgram(NULL,"shaders\\ambient_DifM.vp", "shaders\\ambient_DifM.fp");
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
	lightTex = new Texture("spot0.tga", GL_LINEAR,
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

/* Object configurations. */
enum {
  OC_MGF,
  NUM_OF_OCS,
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

int vsync = 0;
int requestedDepthMapSize = 512;
int depthMapSize = 512;

GLenum depthMapPrecision = GL_UNSIGNED_BYTE;
GLenum depthMapFormat = GL_LUMINANCE;
GLenum depthMapInternalFormat = GL_INTENSITY8;

int useCopyTexImage = 1;
int useLights = (MAX_INSTANCES>=START_INSTANCES)?8:MAX_INSTANCES;
int softWidth[MAX_INSTANCES],softHeight[MAX_INSTANCES],softPrecision[MAX_INSTANCES],softFiltering[MAX_INSTANCES];
int areaType = 0; // 0=linear, 1=square grid, 2=circle

int depthBias24 = 36;
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
int objectConfiguration = OC_MGF;
bool showHelp = 0;
int fullscreen = 0;
int useDisplayLists = 1;
int showLightViewFrustum = 1;
int eyeButton = GLUT_LEFT_BUTTON;
int lightButton = GLUT_MIDDLE_BUTTON;
int useDepth24 = 1;
int drawFront = 0;

int hasShadow = 0;
int hasSwapControl = 0;
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


/*** OPENGL INITIALIZATION AND CHECKS ***/

static int supports15(void)
{
	const char *version;
	int major, minor;

	version = (char *) glGetString(GL_VERSION);
	if (sscanf(version, "%d.%d", &major, &minor) == 2) {
		return major>=2 || (major==1 && minor>=5);
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


GLSLProgram* getProgram(RRObjectRenderer::ColorChannel cc)
{
	switch(cc)
	{
		case RRObjectRenderer::CC_NO_COLOR:
			return lightProg;
		case RRObjectRenderer::CC_TRIANGLE_INDEX:
			return ambientProg;
		case RRObjectRenderer::CC_DIFFUSE_REFLECTANCE:
			{
			GLSLProgramSet* progSet = shadowDifCProgSet;
#ifdef _3DS
			if(!renderOnlyRr) 
				progSet = shadowDifMProgSet;
#endif
			return progSet->getVariant(NULL);
			}
		case RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
			{
			GLSLProgramSet* progSet = shadowDifCProgSet;
#ifdef _3DS
			// na detekci pouzivam RRObjectRenderer, takze bez textur
			//if(!renderOnlyRr) 
			//	progSet = shadowDifMProgSet;
#endif
			return progSet->getVariant("#define FORCE_2D_POSITION\n");
			}
		case RRObjectRenderer::CC_SOURCE_IRRADIANCE:
		case RRObjectRenderer::CC_SOURCE_EXITANCE:
		case RRObjectRenderer::CC_REFLECTED_IRRADIANCE:
		case RRObjectRenderer::CC_REFLECTED_EXITANCE:
#ifdef _3DS
			if(!renderOnlyRr) return ambientDifMProg;
#endif
			return ambientProg;
		default:
			assert(0);
			return NULL;
	}
}

GLSLProgram* setProgram(RRObjectRenderer::ColorChannel cc)
{
	GLSLProgram* tmp = getProgram(cc);
	tmp->useIt();
	return tmp;
}

void drawScene(RRObjectRenderer::ColorChannel cc)
{
	checkGlError();
#ifdef _3DS
	if(!renderOnlyRr && cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE)
	{
		m3ds.Draw(NULL);
		return;
	}
	if(!renderOnlyRr && (cc==RRObjectRenderer::CC_SOURCE_IRRADIANCE || cc==RRObjectRenderer::CC_REFLECTED_IRRADIANCE))
	{
		m3ds.Draw(&indirectColors[0].x);
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
		cc = RRObjectRenderer::CC_REFLECTED_EXITANCE; // for colored output
#ifdef _3DS
		if(!renderOnlyRr)
			cc = RRObjectRenderer::CC_REFLECTED_IRRADIANCE; // for textured 3ds output
#endif
	}
	if(cc==RRObjectRenderer::CC_SOURCE_AUTO)
	{
		cc = RRObjectRenderer::CC_SOURCE_EXITANCE; // for colored output
#ifdef _3DS
		if(!renderOnlyRr)
			cc = RRObjectRenderer::CC_SOURCE_IRRADIANCE; // for textured 3ds output
#endif
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

void updateDepthMap(void)
{
	if(!needDepthMapUpdate) return;

	setupLightView(1);

	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glEnable(GL_POLYGON_OFFSET_FILL);

	setProgramAndDrawScene(RRObjectRenderer::CC_NO_COLOR);
	updateShadowTex();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);

	if(softLight<0 || softLight==useLights-1) needDepthMapUpdate = 0;
}

void drawHardwareShadowPass(RRObjectRenderer::ColorChannel cc)
{
	GLSLProgram* myProg = setProgram(cc);

	activateTexture(GL_TEXTURE1_ARB, GL_TEXTURE_2D);
	lightTex->bindTexture();
	myProg->sendUniform("lightTex", 1);

	activateTexture(GL_TEXTURE0_ARB, GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, shadowTex[(softLight>=0)?softLight:0]);
	myProg->sendUniform("shadowMap", 0);

	glMatrixMode(GL_TEXTURE);
	glLoadMatrixd(lightFrustumMatrix);
	glMultMatrixd(light.viewMatrix);
	glMatrixMode(GL_MODELVIEW);

#ifdef _3DS
	if(!renderOnlyRr && cc!=RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION) // kdyz detekuju source (->force 2d), pouzivam RRObjectRenderer, takze jedem bez difus textur
	{
		activateTexture(GL_TEXTURE2_ARB, GL_TEXTURE_2D);
		myProg->sendUniform("diffuseTex", 2);
	}
#endif

	// set light pos in object space
	myProg->sendUniform("lLightPos",light.pos[0],light.pos[1],light.pos[2]);

	drawScene(cc);

	glActiveTextureARB(GL_TEXTURE0_ARB);
}

void drawEyeViewShadowed()
{
	if (softLight<0) updateDepthMap();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	setupEyeView();

	drawHardwareShadowPass(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE);

	drawLight();
	drawShadowMapFrustum();
}

void placeSoftLight(int n)
{
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
		  light.angle=oldLightAngle+0.2*(n/(useLights-1.)-0.5);
		  light.height=oldLightHeight-0.4*n/useLights;
		  break;
	  case 1: // rectangular
		  {int q=(int)sqrtf(useLights-1)+1;
		  light.angle=oldLightAngle+0.1*(n/q/(q-1.)-0.5);
		  light.height=oldLightHeight+(n%q/(q-1.)-0.5);}
		  break;
	  case 2: // circular
		  light.angle=oldLightAngle+sin(n*2*3.14159/useLights)/20;
		  light.height=oldLightHeight+cos(n*2*3.14159/useLights)/2;
		  break;
		}
		updateMatrices();
	}
}

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

void drawEyeViewSoftShadowed(void)
{
	// add direct
	placeSoftLight(-1);
	for(int i=0;i<useLights;i++)
	{
		placeSoftLight(i);
		glClear(GL_DEPTH_BUFFER_BIT);
		updateDepthMap();
	}
	glClear(GL_ACCUM_BUFFER_BIT);
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	for(int i=0;i<useLights;i++)
	{
		placeSoftLight(i);
		drawEyeViewShadowed();
		glAccum(GL_ACCUM,1./useLights);
	}
	placeSoftLight(-2);

	// add indirect
	if(rrscene 
#ifdef _3DS
		&& (indirectColors || renderOnlyRr)
#endif
		)
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

void capturePrimary() // slow
{
	//!!! needs windows at least 256x256
	unsigned width1 = 4;
	unsigned height1 = 4;
	unsigned width = 512;
	unsigned height = 512;
	captureUv.firstCapturedTriangle = 0;
	captureUv.xmax = width/width1;
	captureUv.ymax = height/height1;

	/*
	jak uspesne napichnout renderer?
	1) na konci vertex shaderu prepsat 2d pozici
	2) k tomu je nutne renderovat neindexovane
	3) podle momentalniho stavu view matice mohou vychazet ruzne speculary
	4) pouzit halfovy buffer aby se detekovalo i presviceni

	ad 2)
	nejde tam dostat per-triangle informaci bez nabourani stavajiciho indexed trilist renderu 
	protoze vic trianglu pouziva stejny vertex
	je nutne za behu prekladat vse do streamu kde se vertexy neopakuji

	prozatim muzu pouzit vlastni renderer a nenapichovat 3ds
	protoze vse dulezite z 3ds (1 svetlo se stiny) lze prenest do rr2gl
	texturu z 3ds lze zanedbat
	*/

	// setup render states
	glClearColor(0,0,0,1);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glViewport(0, 0, width, height);

	// Allocate the index buffer memory as necessary.
	GLuint* pixelBuffer = (GLuint*)malloc(width * height * 4);

	rrCollider::RRMeshImporter* mesh = rrobject->getCollider()->getImporter();
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
			rrobject->setTriangleAdditionalPower(triangleIndex,rrVision::RM_EXITANCE,avg);

			// debug print
			//rrVision::RRColor tmp = rrVision::RRColor(0);
			//rrobject->getTriangleAdditionalMeasure(triangleIndex,rrVision::RM_EXITING_FLUX,tmp);
			//suma+=tmp;
		}
		//printf("sum = %f/%f/%f\n",suma[0],suma[1],suma[2]);
	}

	free(pixelBuffer);

	// prepare for new calculation
	rrscene->sceneResetStatic(false);
	rrtimestep = 0.1f;

	// restore render states
	glViewport(0, 0, winWidth, winHeight);
	glDepthMask(1);
	glEnable(GL_DEPTH_TEST);
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
		"Help information",
		"",
		"mouse+left button - manipulate camera",
		"mouse+mid button  - manipulate light",
		"right button      - menu",
		"arrows            - move camera or light (with middle mouse pressed)",
		"",
		"space - toggle global illumination",
		"'z'   - toggle zoom in and zoom out",
		"'+/-' - area light: increase/decrease number of lights",
		"'s'   - area light: cycle through linear, rectangular and circular area",
		"'w'   - toggle wire frame",
		"'f'   - toggle showing the depth map frustum in dashed lines",
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
	glColor4f(0.0,1.0,0.0,0.2);  /* 20% green. */

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
	if (needTitleUpdate) {
		updateTitle();
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (needMatrixUpdate) {
		updateMatrices();
	}

	switch (drawMode) {
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

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	drawHelpMessage(showHelp);

	if (!drawFront) {
		glutSwapBuffers();
	}

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
	printf("  TEX2D %dx%d:%d using %s\n",
		depthMapSize, depthMapSize, precision,
		useCopyTexImage ? "CopyTexSubImage" : "ReadPixels/TexSubImage");
	needDepthMapUpdate = 1;
}

void selectObjectConfig(int item)
{
	objectConfiguration = item;
	needDepthMapUpdate = 1;
	glutPostRedisplay();
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
}

void selectMenu(int item)
{
	lastInteractionTime = GETTIME;
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
	lastInteractionTime = GETTIME;
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
	if(movingLight) needDepthMapUpdate = 1;
	glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
	lastInteractionTime = GETTIME;
	switch (c) {
		case 27:
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
		case '+':
			if(useLights<MAX_INSTANCES) 
			{
				useLights++;
				needDepthMapUpdate = 1;
			}
			break;
		case '-':
			if(useLights>1) 
			{
				useLights--;
				needDepthMapUpdate = 1;
			}
			break;
		case 'g':
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
	if (hasSwapControl) {
		if (vsync) {
			wglSwapIntervalEXT(1);
		} else {
			wglSwapIntervalEXT(0);
		}
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
	lastInteractionTime = GETTIME;
	if (button == eyeButton && state == GLUT_DOWN) {
		movingEye = 1;
		xEyeBegin = x;
		yEyeBegin = y;
	}
	if (button == eyeButton && state == GLUT_UP) {
		lastInteractionTime = 0;
		movingEye = 0;
	}
	if (button == lightButton && state == GLUT_DOWN) {
		movingLight = 1;
		xLightBegin = x;
		yLightBegin = y;
	}
	if (button == lightButton && state == GLUT_UP) {
		lastInteractionTime = 0;
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
	lastInteractionTime = GETTIME;
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
	glutAddMenuEntry("[m] Switch mouse control", ME_SWITCH_MOUSE_CONTROL);
	glutAddMenuEntry("[w] Toggle wire frame", ME_TOGGLE_WIRE_FRAME);
	glutAddMenuEntry("[ESC] Quit", ME_EXIT);

	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void parseOptions(int argc, char **argv)
{
	int i;

	for (i=1; i<argc; i++) {
		if (!strcmp("-window", argv[i])) {
			fullscreen = 0;
		}
		if (strstr(argv[i], ".mgf")) {
			mgf_filename = argv[i];
		}
#ifdef _3DS
		if (strstr(argv[i], ".3ds")) {
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
	TIME now = GETTIME;
	if((now-lastInteractionTime)/(float)PER_SEC<TIME_TO_START_IMPROVING) return;

	static const float calcstep = 0.1f;
	rrscene->sceneImproveStatic(endByTime,(void*)(intptr_t)(GETTIME+calcstep*PER_SEC));
	static float calcsum = 0;
	calcsum += calcstep;
	if(calcsum>=rrtimestep)
	{
		calcsum = 0;
		if(rrtimestep<1.5f) rrtimestep*=1.1f;
#ifdef _3DS
		if(renderOnlyRr) 
		{
			renderer->setStatus(RRObjectRenderer::CC_SOURCE_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
			renderer->setStatus(RRObjectRenderer::CC_REFLECTED_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
		}
		else
			updateIndirect();
#else
		renderer->setStatus(RRObjectRenderer::CC_SOURCE_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
		renderer->setStatus(RRObjectRenderer::CC_REFLECTED_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
#endif
		glutPostRedisplay();
	}
}

int
main(int argc, char **argv)
{
	rrCollider::registerLicense("","");
	rrVision::registerLicense("","");

	glutInitWindowSize(800, 600);
	glutInit(&argc, argv);
	parseOptions(argc, argv);

	if (useDepth24) {
		glutInitDisplayString("depth~24 rgb double");
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

	if(!supports15())
	{
		puts("At least OpenGL 1.5 required.");
		return 0;
	}

	if (strstr(filename_3ds, "koupelna3")) {
		scale_3ds = 0.01f;
	}
	if (strstr(filename_3ds, "sponza"))
	{
		SimpleCamera sponza_eye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999};
		SimpleCamera sponza_light = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001};
		eye = sponza_eye;
		light = sponza_light;
	}
	printf("Loading and preprocessing scene (~20 sec)...");
#ifdef _3DS
	// load 3ds
	if(!m3ds.Load(filename_3ds,scale_3ds)) return 1;
	//m3ds.shownormals=1;
	rrobject = new_3ds_importer(&m3ds)->createAdditionalExitance();
#else
	// load mgf
	rrobject = new_mgf_importer(mgf_filename)->createAdditionalExitance();
#endif
	//if(rrobject) printf("vertices=%d triangles=%d\n",rrobject->getCollider()->getImporter()->getNumVertices(),rrobject->getCollider()->getImporter()->getNumTriangles());
	rrVision::RRSetState(rrVision::RRSSF_SUBDIVISION_SPEED,0);
	rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,0);
	rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,1);
	//rrVision::RRSetState(rrVision::RRSS_GET_SMOOTH,0);
	rrVision::RRSetStateF(rrVision::RRSSF_MIN_FEATURE_SIZE,0.2f);
	//rrVision::RRSetStateF(rrVision::RRSSF_MAX_SMOOTH_ANGLE,0.9f);
	rrscene = new rrVision::RRScene();
	rrscaler = rrVision::RRScaler::createGammaScaler(0.4f);
	rrscene->setScaler(rrscaler);
	rrscene->objectCreate(rrobject);
	glsl_init();
	checkGlError();
	renderer = new RRCachingRenderer(new RRGLObjectRenderer(rrobject,rrscene));
	printf("\n");

	glutMainLoop();
	return 0;
}
