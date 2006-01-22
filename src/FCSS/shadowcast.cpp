//!!! fudge factors
#define INCIDENT_FLUX_FACTOR 1150.0f
#define INDIRECT_RENDER_FACTOR 1//0.6f
// 
/*

udelat render do forced 2d
udelat slow detekjci, tohle nejde vyladit aby fungovalo vic scen naraz
compile gcc, prejit na glew
proc ma 3ds renderer kazdy face jinak nasviceny, nejsou zle normaly?

autodeteknout zda mam metry nebo centimetry
vypocet je dost pomaly, use profiler. zkusit nejaky meshcopy
prozkoumat slucoani blizkych ivertexu proc tady nic nezlepsi
stranka s vic demacema pohromade
nacitat jpg
dodelat podporu pro matice do 3ds2rr importeru
nemit 2 ruzny textury, 1 pro render, 1 pro capture primary
fast vyzaduje svetlo s korektnim utlumem.. co je 2x dal je 4x min svetly
 prozatim vyreseno linearnim utlumem
 zkusit fyzikalne korektni model s gammou na konci 
 1. umi pouze 1 svetlo
    out = 0
    pro kazdou instanci: out += viditelnost
    out = gamma(out/sqr(vzdalenost_svetla)+indirect)*diffusematerial
 2. umi vic svetel
    out = 0
    pro kazde svetlo:
      pro kazdou instanci: out += viditelnost/sqr(vzdalenost_svetla)
    out = gamma(out+indirect)*diffusematerial
kdyz uz by byl korektni model s gammou, pridat ovladani gammy

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce
 kdyz se vypne scaler(0.4) nebo indirect, primary vypada desne tmave
 az pri secteni s indirectem (scitani produkuje prilis velke cislo) zacne vypadat akorat
 

 #include <GL/glew.h>
 #include <GL/glut.h>
 ...
 glutInit(&argc, argv);
 glutCreateWindow("GLEW Test");
 GLenum err = glewInit();
 if (GLEW_OK != err)
 {
 // Problem: glewInit failed, something is seriously wrong.
fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
...
}
fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
*/

#include <assert.h>
//#include <crtdefs.h> // intptr_t
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glut.h>
#include <GL/glprocs.h>

#include <iostream>
#include "glsl/Light.hpp"
#include "glsl/Camera.hpp"
#include "glsl/GLSLProgram.hpp"
#include "glsl/GLSLShader.hpp"
#include "glsl/Texture.hpp"
#include "glsl/FrameRate.hpp"

using namespace std;

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "mgf2rr.h"
#include "rr2gl.h"
#include "matrix.h"   /* OpenGL-style 4x4 matrix manipulation routines */


/////////////////////////////////////////////////////////////////////////////
//
// 3DS

#define _3DS
#ifdef _3DS
#undef INCIDENT_FLUX_FACTOR
#define INCIDENT_FLUX_FACTOR 150.0f
#include "Model_3DS.h"
#include "3ds2rr.h"
Model_3DS m3ds;
//char *filename_3ds="data\\sponza\\sponza.3ds";
char *filename_3ds="data\\raist\\koupelna3.3ds";
#endif


/////////////////////////////////////////////////////////////////////////////
//
// RR

bool renderOnlyRr = !false;
RRCachingRenderer* renderer = NULL;
rrVision::RRAdditionalObjectImporter* rrobject = NULL;
rrVision::RRScene* rrscene = NULL;
rrVision::RRScaler* rrscaler = NULL;
float rrtimestep;
// rr endfunc callback
#include <time.h>
#define TIME    clock_t            
#define GETTIME clock()
#define PER_SEC CLOCKS_PER_SEC
static bool endByTime(void *context)
{
	return GETTIME>(TIME)(intptr_t)context;
}
rrVision::RRColor* indirectColors = NULL;
void updateIndirect()
{
	if(!rrobject || !rrscene) return;

	rrCollider::RRMeshImporter* mesh = rrobject->getCollider()->getImporter();
	unsigned numTriangles = mesh->getNumTriangles();
	unsigned numVertices = mesh->getNumVertices();
	delete[] indirectColors;
	indirectColors = new rrVision::RRColor[numVertices*3];

	// 3ds has indexed trilist
	// triangle0=indices0,1,2, triangle1=indices3,4,5...
	for(unsigned t=0;t<numTriangles;t++) // triangle
	{
		// get 3*index
		rrCollider::RRMeshImporter::Triangle triangle;
		mesh->getTriangle(t,triangle);
		for(unsigned v=0;v<3;v++) // vertex
		{
			// get 1*irradiance
			static rrVision::RRColor black = rrVision::RRColor(1);
			const rrVision::RRColor* indirect = rrscene->getTriangleIrradiance(0,t,v);//rrscene->getTriangleRadiantExitance(0,t,v);
			if(!indirect) indirect = &black;
			// write 1*irradiance
			assert(triangle[v]<numVertices);
			rrVision::RRColor tmp = *indirect*INDIRECT_RENDER_FACTOR;
			for(unsigned i=0;i<3;i++)
			{
				assert(_finite(tmp[i]));
				assert(tmp[i]>=0);
				assert(tmp[i]<1500000);
			}
			indirectColors[triangle[v]] = *indirect*INDIRECT_RENDER_FACTOR;
		}
	}

	unsigned faces,rays;
	float sourceExitingFlux,reflectedIncidentFlux;
	rrscene->getStats(&faces,&sourceExitingFlux,&rays,&reflectedIncidentFlux);
	printf("faces=%d sourceExitingFlux=%f rays=%d\n",faces,sourceExitingFlux,rays);
/*	puts("");
	for(unsigned i=0;i<4;i++)
	{
		char buf[1000];
		rrscene->getInfo(buf,i);
		puts(buf);
	}*/
}


/////////////////////////////////////////////////////////////////////////////
//
// GLSL

#define MAX_INSTANCES 50 // max number of light instances aproximating one area light
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

/* Texture objects. */
enum {
  TO_RESERVED = 0,             /* zero is not a valid OpenGL texture object ID */
  TO_DEPTH_MAP,                /* high resolution depth map for shadow mapping */
  TO_HW_DEPTH_MAP,             /* high resolution hardware depth map for shadow
                                  mapping */
  TO_SOFT_DEPTH_MAP            /* TO_SOFT_DEPTH_MAP+n = depth map for n-th light */
};

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
  ME_TOGGLE_WIRE_FRAME,
  ME_TOGGLE_LIGHT_FRUSTUM,
  ME_TOGGLE_HW_SHADOW_MAPPING,
  ME_TOGGLE_DEPTH_MAP_CULLING,
  ME_TOGGLE_HW_SHADOW_FILTERING,
  ME_TOGGLE_HW_SHADOW_COPY_TEX_IMAGE,
  ME_TOGGLE_QUICK_LIGHT_MOVE,
  ME_SWITCH_MOUSE_CONTROL,
  ME_ON_LIGHT_FRUSTUM,
  ME_OFF_LIGHT_FRUSTUM,
  ME_DEPTH_MAP_64,
  ME_DEPTH_MAP_128,
  ME_DEPTH_MAP_256,
  ME_DEPTH_MAP_512,
  ME_DEPTH_MAP_1024,
  ME_PRECISION_HW_16BIT,
  ME_PRECISION_HW_24BIT,
  ME_EXIT,
};

int hwLittleEndian;
int littleEndian;

int vsync = 0;
int requestedDepthMapSize = 512;
int depthMapSize = 512;

int requestedDepthMapRectWidth = 350;
int requestedDepthMapRectHeight = 300;
int depthMapRectWidth = 350;
int depthMapRectHeight = 300;

GLenum depthMapPrecision = GL_UNSIGNED_BYTE;
GLenum depthMapFormat = GL_LUMINANCE;
GLenum depthMapInternalFormat = GL_INTENSITY8;

GLenum hwDepthMapPrecision = GL_UNSIGNED_SHORT;
GLenum hwDepthMapFiltering = GL_LINEAR;

int useCopyTexImage = 1;
int useTextureRectangle = 0;
int useLights = 1;
int softWidth[200],softHeight[200],softPrecision[200],softFiltering[200];
int areaType = 0; // 0=linear, 1=square grid, 2=circle
GLfloat eye_shift[3]={0,0,0}; // mimo provoz
GLfloat ed[3];
char *mgf_filename="data\\scene8.mgf";

int depthBias16 = 6;
int depthBias24 = 28;
int depthScale16, depthScale24;
GLfloat slopeScale = 3.0;

GLfloat textureLodBias = 0.0;

#define LIGHT_DIMMING 0.0

GLfloat zero[] = {0.0, 0.0, 0.0, 1.0};

GLfloat lv[4];  /* default light position */

void *font = GLUT_BITMAP_8_BY_13;

GLUquadricObj *q;
float eyeAngle = 1.0;
float eyeHeight = 10.0;
int xEyeBegin, yEyeBegin, movingEye = 0;
float lightAngle = 0.85;
float lightHeight = 8.0;
int xLightBegin, yLightBegin, movingLight = 0;
int wireFrame = 0;

/* By default, disable back face culling during depth map construction.
   See the comment in the code. */
int depthMapBackFaceCulling = 0;

int winWidth, winHeight;
int needMatrixUpdate = 1;
int needTitleUpdate = 1;
int needDepthMapUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
int objectConfiguration = OC_MGF;
int showHelp = 0;
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
int hasSeparateSpecularColor = 0;
int hasTextureBorderClamp = 0;
int hasTextureEdgeClamp = 0;

double winAspectRatio;

GLdouble eyeFieldOfView = 100.0;
GLdouble eyeNear = 0.3;
GLdouble eyeFar = 60.0;
GLdouble lightFieldOfView = 80.0;
GLdouble lightNear = 1.0;
GLdouble lightFar = 20.0;

GLdouble eyeViewMatrix[16];
GLdouble eyeFrustumMatrix[16];
GLdouble lightViewMatrix[16];
GLdouble lightInverseViewMatrix[16];
GLdouble lightFrustumMatrix[16];
GLdouble lightInverseFrustumMatrix[16];


/*** OPENGL INITIALIZATION AND CHECKS ***/

static int supports20(void)
{
	const char *version;
	int major, minor;

	version = (char *) glGetString(GL_VERSION);
	if (sscanf(version, "%d.%d", &major, &minor) == 2) {
		return major>=2;
	}
	return 0;            /* OpenGL version string malformed! */
}

/* updateTitle - update the window's title bar text based on the current
   program state. */
void updateTitle(void)
{
	char title[256];
	char *modeName;
	int depthBits;

	/* Only update the title is needTitleUpdate is set. */
	if (needTitleUpdate) {
		switch (drawMode) {
	case DM_LIGHT_VIEW:
		modeName = "light view";
	case DM_EYE_VIEW_SHADOWED:
		modeName = "eye view with shadows";
		break;
	case DM_EYE_VIEW_SOFTSHADOWED:
		modeName = "eye view with soft shadows";
		break;
	default:
		assert(0);
		break;
		}
		glGetIntegerv(GL_DEPTH_BITS, &depthBits);
		sprintf(title,
			"shadowcast - %s (%dx%d %d-bit) depthBias=%d, slopeScale=%f, filter=%s",
			modeName, depthMapSize, depthMapSize, depthBits, depthBias24 * depthScale24, slopeScale,
			((hwDepthMapFiltering == GL_LINEAR)) ? "LINEAR" : "NEAREST");
		glutSetWindowTitle(title);
		needTitleUpdate = 0;
	}
}


/*** DRAW VARIOUS SCENES ***/

static void drawSphere(void)
{
	gluSphere(q, 0.55, 16, 16);
}


/* drawScene - multipass shadow algorithms may need to call this routine
   more than once per frame. */
void drawScene(RRObjectRenderer::ColorChannel cc)
{
#ifdef _3DS
	if(!renderOnlyRr && cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE)
	{
		m3ds.Draw(NULL);
		return;
	}
	if(!renderOnlyRr && cc==RRObjectRenderer::CC_REFLECTED_IRRADIANCE)
	{
		m3ds.Draw(&indirectColors[0].x);
		return;
	}
#endif
	renderer->render(cc);
}

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void drawLight(void)
{
	ambientProg->useIt();
	glPushMatrix();
	glTranslatef(lv[0], lv[1], lv[2]);
	glColor3f(1,1,0);
	gluSphere(q, 0.05, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	ed[0]=3*sin(eyeAngle);
	ed[1]=0.3*eyeHeight;
	ed[2]=3*cos(eyeAngle);
	buildLookAtMatrix(eyeViewMatrix,
		ed[0], ed[1], ed[2],
		0, 3, 0,
		0, 1, 0);
	//glTranslatef(eye_shift[0],eye_shift[1],eye_shift[2]);

	lv[0] = 2*sin(lightAngle);
	lv[1] = 0.15 * lightHeight + 3;
	lv[2] = 2*cos(lightAngle);
	lv[3] = 1.0;

	buildLookAtMatrix(lightViewMatrix,
		lv[0], lv[1], lv[2],
		0, 3, 0,
		0, 1, 0);

	buildPerspectiveMatrix(lightFrustumMatrix, 
		lightFieldOfView, 1.0, lightNear, lightFar);

	if (showLightViewFrustum) {
		invertMatrix(lightInverseViewMatrix, lightViewMatrix);
		invertMatrix(lightInverseFrustumMatrix, lightFrustumMatrix);
	}
}

void setupEyeView(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(eyeFrustumMatrix);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(eyeViewMatrix);
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
	glLoadMatrixd(lightViewMatrix);
}

void drawLightView(void)
{
	glLightfv(GL_LIGHT0, GL_POSITION, lv);
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

	lightProg->useIt();
	setupLightView(1);

	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glEnable(GL_POLYGON_OFFSET_FILL);

	drawScene(RRObjectRenderer::CC_NO_COLOR);
	updateShadowTex();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);

	if(softLight<0 || softLight==useLights-1) needDepthMapUpdate = 0;
}

void drawHardwareShadowPass(RRObjectRenderer::ColorChannel cc)
{

	GLSLProgramSet* myProgSet = shadowDifCProgSet;
#ifdef _3DS
	if(!renderOnlyRr) myProgSet = shadowDifMProgSet;
#endif
	static int qq=0;
	GLSLProgram* myProg = myProgSet->getVariant((cc==RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION)?"#define FORCE_2D_POSITION\n":NULL);
	myProg->useIt();

	activateTexture(GL_TEXTURE1_ARB, GL_TEXTURE_2D);
	lightTex->bindTexture();
	myProg->sendUniform("lightTex", 1);

	activateTexture(GL_TEXTURE0_ARB, GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, shadowTex[(softLight>=0)?softLight:0]);
	myProg->sendUniform("shadowMap", 0);

	glMatrixMode(GL_TEXTURE);
	glLoadMatrixd(lightFrustumMatrix);
	glMultMatrixd(lightViewMatrix);
	GLdouble eyeInverseViewMatrix[16];
	invertMatrix(eyeInverseViewMatrix,eyeViewMatrix);
	glMultMatrixd(eyeInverseViewMatrix);
	glMatrixMode(GL_MODELVIEW);

#ifdef _3DS
	if(!renderOnlyRr)
	{
		activateTexture(GL_TEXTURE2_ARB, GL_TEXTURE_2D);
		myProg->sendUniform("diffuseTex", 2);
	}
#endif

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
	glLightfv(GL_LIGHT0, GL_POSITION, lv);

	drawHardwareShadowPass(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE);

	drawLight();
	drawShadowMapFrustum();
}

void placeSoftLight(int n)
{
	softLight=n;
	static float oldLightAngle,oldLightHeight;
	if(n==-1) { // init, before all
		oldLightAngle=lightAngle;
		oldLightHeight=lightHeight;
		glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 1);
		glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 90); // no light behind spotlight
		//glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01);
		glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 1.5);
		return;
	}
	if(n==-2) { // done, after all
		lightAngle=oldLightAngle;
		lightHeight=oldLightHeight;
		updateMatrices();
		return;
	}
	// place one point light approximating part of area light
	if(useLights>1) {
		switch(areaType) {
	  case 0: // linear
		  lightAngle=oldLightAngle+0.2*(n/(useLights-1.)-0.5);
		  lightHeight=oldLightHeight-0.4*n/useLights;
		  break;
	  case 1: // rectangular
		  {int q=(int)sqrtf(useLights-1)+1;
		  lightAngle=oldLightAngle+0.1*(n/q/(q-1.)-0.5);
		  lightHeight=oldLightHeight+(n%q/(q-1.)-0.5);}
		  break;
	  case 2: // circular
		  lightAngle=oldLightAngle+sin(n*2*3.14159/useLights)/20;
		  lightHeight=oldLightHeight+cos(n*2*3.14159/useLights)/2;
		  break;
		}
		updateMatrices();
	}
	GLfloat ld[3]={-lv[0],-lv[1],-lv[2]};
	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ld);
}

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
#ifdef _3DS
		if(renderOnlyRr)
			ambientProg->useIt();
		else
			ambientDifMProg->useIt();
#else
		ambientProg->useIt();
#endif
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		drawScene(RRObjectRenderer::CC_REFLECTED_EXITANCE); // pro color exitance, pro texturu irradiance
		glAccum(GL_ACCUM,1);
	}

	glAccum(GL_RETURN,1);
}

void capturePrimaryFast()
{
	//!!! needs windows at least 256x256
	unsigned width = 256;
	unsigned height = 256;

	// Setup light view with a square aspect ratio since the texture is square.
	setupLightView(1);

	glViewport(0, 0, width, height);

	glClearColor(1,1,1,1); // avoid background to contribute to triangle 0
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glClearColor(0,0,0,1);
	ambientProg->useIt();
	renderer->render(RRObjectRenderer::CC_TRIANGLE_INDEX);
	unsigned numTriangles = rrobject->getCollider()->getImporter()->getNumTriangles();

	// Allocate the index buffer memory as necessary.
	GLuint* indexBuffer = (GLuint*)malloc(width * height * 4);
	rrVision::RRColor* trianglePower = (rrVision::RRColor*)malloc(numTriangles*sizeof(rrVision::RRColor));

	// Read back the index buffer to memory.
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, indexBuffer);

	// accumulate triangle powers into trianglePower
	for(unsigned i=0;i<numTriangles;i++) 
		for(unsigned c=0;c<3;c++)
			trianglePower[i][c]=0;
	unsigned pixel = 0;
	for(unsigned j=0;j<height;j++)
		for(unsigned i=0;i<width;i++)
		{
			unsigned index = indexBuffer[pixel] >> 8; // alpha was lost
			if(index<numTriangles)
			{
				rrVision::RRColor pixelPower = rrVision::RRColor(1,1,1);
				// modulate by spotmap
				if(lightTex)
				{
					float rgb[3];
					lightTex->getPixel((float)i / width,(float)j / height,rgb);
					for(unsigned c=0;c<3;c++)
						pixelPower[c] *= rgb[c];
				}

				for(unsigned c=0;c<3;c++)
					trianglePower[index][c] += pixelPower[c];
			}
			else
			{
				//assert(0);
			}
			pixel++;
		}

	// copy data to object
	float mult = INCIDENT_FLUX_FACTOR/width/height;
	for(unsigned t=0;t<numTriangles;t++)
	{
		rrVision::RRColor color = trianglePower[t] * mult;
		rrobject->setTriangleAdditionalPower(t,rrVision::RM_INCIDENT_FLUX,color);
	}

	// debug print
	//printf("\n\n");
	//for(unsigned i=0;i<numTriangles;i++) printf("%d ",(int)trianglePower[i][0]);

	// prepare for new calculation
	rrscene->sceneResetStatic(true);
	rrtimestep = 0.1f;

	free(trianglePower);
	free(indexBuffer);
	glViewport(0, 0, winWidth, winHeight);
}

void capturePrimarySlow()
{
	//!!! needs windows at least 256x256
	unsigned width1 = 8;
	unsigned height1 = 4;
	unsigned width = 256;
	unsigned height = 256;

	// clear
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT);

	// prepare texcoords (for trilist, uvuvuv per triangle)
	rrCollider::RRMeshImporter* mesh = rrobject->getCollider()->getImporter();
	unsigned numTriangles = mesh->getNumTriangles();
	unsigned numVertices = mesh->getNumVertices();
	GLfloat* texcoords = new GLfloat[6*numTriangles];
	for(unsigned i=0;i<numTriangles;i++)
	{
		//texcoords
	}
	/*
	jak uspesne napichnout renderer?
	1) na konci vertex shaderu prepsat 2d pozici
	2) k tomu je nutne renderovat neindexovane
	3) podle momentalniho stavu view matice mohou vychazet ruzne speculary

	ad 2)
	nejde tam dostat per-triangle informaci bez nabourani stavajiciho indexed trilist renderu 
	protoze vic trianglu pouziva stejny vertex
	je nutne za behu prekladat vse do streamu kde se vertexy neopakuji

	prozatim muzu pouzit vlastni renderer a nenapichovat 3ds
	protoze vse dulezite z 3ds (1 svetlo se stiny) lze prenest do rr2gl
	texturu z 3ds lze zanedbat
	*/

	// setup render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glViewport(0, 0, width, height);

	// render scene
	drawHardwareShadowPass(RRObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION);

	// Allocate the index buffer memory as necessary.
	GLuint* indexBuffer = (GLuint*)malloc(width * height * 4);
	rrVision::RRColor* trianglePower = (rrVision::RRColor*)malloc(numTriangles*sizeof(rrVision::RRColor));

	// Read back the index buffer to memory.
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, indexBuffer);

	// accumulate triangle powers into trianglePower
	for(unsigned i=0;i<numTriangles;i++) 
		for(unsigned c=0;c<3;c++)
			trianglePower[i][c]=0;
	unsigned pixel = 0;
	for(unsigned j=0;j<height;j++)
		for(unsigned i=0;i<width;i++)
		{
			unsigned index = indexBuffer[pixel] >> 8; // alpha was lost
			if(index<numTriangles)
			{
				rrVision::RRColor pixelPower = rrVision::RRColor(1,1,1);
				// modulate by spotmap
				if(lightTex)
				{
					float rgb[3];
					lightTex->getPixel((float)i / width,(float)j / height,rgb);
					for(unsigned c=0;c<3;c++)
						pixelPower[c] *= rgb[c];
				}

				for(unsigned c=0;c<3;c++)
					trianglePower[index][c] += pixelPower[c];
			}
			else
			{
				//assert(0);
			}
			pixel++;
		}

	// copy data to object
	float mult = INCIDENT_FLUX_FACTOR/width/height;
	for(unsigned t=0;t<numTriangles;t++)
	{
		rrVision::RRColor color = trianglePower[t] * mult;
		rrobject->setTriangleAdditionalPower(t,rrVision::RM_INCIDENT_FLUX,color);
	}

	// debug print
	//printf("\n\n");
	//for(unsigned i=0;i<numTriangles;i++) printf("%d ",(int)trianglePower[i][0]);

	// prepare for new calculation
	rrscene->sceneResetStatic(true);
	rrtimestep = 0.1f;

	free(trianglePower);
	free(indexBuffer);
	delete[] texcoords;

	// restore render states
	glViewport(0, 0, winWidth, winHeight);
	glDepthMask(1);
	glEnable(GL_DEPTH_TEST);
}

void capturePrimary()
{
	capturePrimaryFast();
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

static void drawHelpMessage(void)
{
	static char *message[] = {
		"Help information",
		"'h'  - shows and dismisses this message",
		"'s'  - show eye view WITH shadows",
		"'S'  - show eye view WITH SOFT shadows",
		"'+/-'- soft: increase/decrease number of points",
		"arrow- soft: move camera",
		"'g'  - soft: cycle through linear, rectangular and circular light",
		"'w'  - toggle wire frame",
		"'m'  - toggle whether the left or middle mouse buttons control the eye and",
		"       view positions (helpful for systems with only a two-button mouse)",
		"'o'  - increment object configurations",
		"'O'  - decrement object configurations",
		"'f'  - toggle showing the depth map frustum in dashed lines",
		"'p'  - narrow shadow frustum field of view",
		"'P'  - widen shadow frustum field of view",
		"'n'  - compress shadow frustum near clip plane",
		"'N'  - expand shadow frustum near clip plane",
		"'c'  - compress shadow frustum far clip plane",
		"'C'  - expand shadow frustum far clip plane",
		"'b'  - increment the depth bias for 1st pass glPolygonOffset",
		"'B'  - decrement the depth bias for 1st pass glPolygonOffset",
		"'q'  - increment depth slope for 1st pass glPolygonOffset",
		"'Q'  - increment depth slope for 1st pass glPolygonOffset",
		"",
		"'1' through '5' - use 64x64, 128x128, 256x256, 512x512, or 1024x1024 depth map",
		"",
		"'9'  - toggle 16-bit and 24-bit depth map precison for hardware shadow mapping",
		"'z'  - toggle zoom in and zoom out",
		"'F3' - toggle back face culling during depth map construction",
		"'F4' - toggle linear/nearest hardware depth map filtering",
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
	glRecti(winWidth - 30, 30, 30, winHeight - 30);

	glDisable(GL_BLEND);

	glColor3f(1,1,1);
	for(i=0; message[i] != NULL; i++) {
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

	if (showHelp) {
		drawHelpMessage();
	}

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
	if (useTextureRectangle) {
		printf("  RECT %dx%d:%d using %s\n",
			depthMapRectWidth, depthMapRectHeight, precision,
			useCopyTexImage ? "CopyTexSubImage" : "ReadPixels/TexSubImage");
	} else {
		printf("  TEX2D %dx%d:%d using %s\n",
			depthMapSize, depthMapSize, precision,
			useCopyTexImage ? "CopyTexSubImage" : "ReadPixels/TexSubImage");
	}
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

void toggleDepthMapCulling(void)
{
	depthMapBackFaceCulling = !depthMapBackFaceCulling;
	needDepthMapUpdate = 1;
	glutPostRedisplay();
}

void toggleHwShadowFiltering(void)
{
	if (hwDepthMapFiltering == GL_LINEAR) {
		hwDepthMapFiltering = GL_NEAREST;
	} else {
		hwDepthMapFiltering = GL_LINEAR;
	}
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
	glutPostRedisplay();
}


void updateDepthBias(int delta)
{
	GLfloat scale, bias;

	if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
		depthBias16 += delta;
		scale = slopeScale;
		bias = depthBias16 * depthScale16;
	} else {
		depthBias24 += delta;
		scale = slopeScale;
		bias = depthBias24 * depthScale24;
	}
	glPolygonOffset(scale, bias);
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
}

void updateDepthMapSize(void)
{
	int oldDepthMapSize = depthMapSize;

	depthMapSize = requestedDepthMapSize;
	while ((winWidth < depthMapSize) || (winHeight < depthMapSize)) {
		depthMapSize >>= 1;  // Half the depth map size
	}
	if (depthMapSize < 1) {
		/* Just in case. */
		depthMapSize = 1;
	}
	if (depthMapSize != requestedDepthMapSize) {
		printf("shadowcast: reducing depth map from %d to %d based on window size %dx%d\n",
			requestedDepthMapSize, depthMapSize, winWidth, winHeight);
	}
	if (!useTextureRectangle) {
		if (oldDepthMapSize != depthMapSize) {
			needDepthMapUpdate = 1;
			needTitleUpdate = 1;
			glutPostRedisplay();
		}
	}
}

void selectMenu(int item)
{
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

  case ME_TOGGLE_WIRE_FRAME:
	  toggleWireFrame();
	  return;
  case ME_TOGGLE_DEPTH_MAP_CULLING:
	  toggleDepthMapCulling();
	  return;
  case ME_TOGGLE_HW_SHADOW_FILTERING:
	  toggleHwShadowFiltering();
	  return;

  case ME_TOGGLE_LIGHT_FRUSTUM:
	  showLightViewFrustum = !showLightViewFrustum;
	  if (showLightViewFrustum) {
		  needMatrixUpdate = 1;
	  }
	  break;
  case ME_ON_LIGHT_FRUSTUM:
	  if (!showLightViewFrustum) {
		  needMatrixUpdate = 1;
	  }
	  showLightViewFrustum = 1;
	  break;
  case ME_OFF_LIGHT_FRUSTUM:
	  showLightViewFrustum = 0;
	  break;
  case ME_DEPTH_MAP_64:
	  requestedDepthMapSize = 64;
	  updateDepthMapSize();
	  return;
  case ME_DEPTH_MAP_128:
	  requestedDepthMapSize = 128;
	  updateDepthMapSize();
	  return;
  case ME_DEPTH_MAP_256:
	  requestedDepthMapSize = 256;
	  updateDepthMapSize();
	  return;
  case ME_DEPTH_MAP_512:
	  requestedDepthMapSize = 512;
	  updateDepthMapSize();
	  return;
  case ME_DEPTH_MAP_1024:
	  requestedDepthMapSize = 1024;
	  updateDepthMapSize();
	  return;
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
	switch (c) {
  case GLUT_KEY_F3:
	  toggleDepthMapCulling();
	  return;
  case GLUT_KEY_F4:
	  toggleHwShadowFiltering();
	  return;
  case GLUT_KEY_F7:
	  benchmark(1);
	  return;
  case GLUT_KEY_F8:
	  benchmark(0);
	  return;
  case GLUT_KEY_F9:
	  capturePrimary();
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
	switch (objectConfiguration) {
  case OC_MGF:
	  switch (c) {
  case GLUT_KEY_UP:
	  for(int i=0;i<3;i++) eye_shift[i]+=ed[i]/20;
	  break;
  case GLUT_KEY_DOWN:
	  for(int i=0;i<3;i++) eye_shift[i]-=ed[i]/20;
	  break;
  case GLUT_KEY_LEFT:
	  eye_shift[0]+=ed[2]/20;
	  eye_shift[2]-=ed[0]/20;
	  //eye_shift[2]+=ed[1]/20;
	  break;
  case GLUT_KEY_RIGHT:
	  eye_shift[0]-=ed[2]/20;
	  eye_shift[2]+=ed[0]/20;
	  //eye_shift[2]+=ed[1]/20;
	  break;
  default:
	  return;
	  }
	  break;
  default:
	  return;
	}
	glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
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
  case 's':
	  drawMode = DM_EYE_VIEW_SHADOWED;
	  needTitleUpdate = 1;
	  break;
  case 'S':
	  drawMode = DM_EYE_VIEW_SOFTSHADOWED;
	  needTitleUpdate = 1;
	  break;
  case 'o':
	  objectConfiguration = (objectConfiguration + 1) % NUM_OF_OCS;
	  needDepthMapUpdate = 1;
	  break;
  case 'O':
	  objectConfiguration = objectConfiguration - 1;
	  if (objectConfiguration < 0) {
		  objectConfiguration = NUM_OF_OCS-1;
	  }
	  needDepthMapUpdate = 1;
	  break;
  case '+':
	  useLights++;
	  needDepthMapUpdate = 1;
	  break;
  case '-':
	  if(useLights>1) {
		  useLights--;
		  needDepthMapUpdate = 1;
	  }
	  break;
  case 'g':
	  ++areaType%=3;
	  needDepthMapUpdate = 1;
	  break;
  case '1':
	  requestedDepthMapSize = 64;
	  updateDepthMapSize();
	  return;
  case '2':
	  requestedDepthMapSize = 128;
	  updateDepthMapSize();
	  return;
  case '3':
	  requestedDepthMapSize = 256;
	  updateDepthMapSize();
	  return;
  case '4':
	  requestedDepthMapSize = 512;
	  updateDepthMapSize();
	  return;
  case '5':
	  requestedDepthMapSize = 1024;
	  updateDepthMapSize();
	  return;
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
	if (depthBits < 16) {
		depthScale16 = 1;
	} else {
		depthScale16 = 1 << (depthBits - 16);
	}
	needDepthMapUpdate = 1;
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	winAspectRatio = (double) winHeight / (double) winWidth;
	buildPerspectiveMatrix(eyeFrustumMatrix,
		eyeFieldOfView, 1.0/winAspectRatio, eyeNear, eyeFar);

	/* Perhaps there might have been a mode change so at window
	reshape time, redetermine the depth scale. */
	updateDepthScale();

	updateDepthMapSize();
}

void initGL(void)
{
	GLfloat globalAmbient[] = {0.5, 0.5, 0.5, 1.0};
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
	printf("depth buffer precision = %d\n", depthBits);
	if (depthBits >= 24) {
		hwDepthMapPrecision = GL_UNSIGNED_INT;
	}

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
	if (button == eyeButton && state == GLUT_DOWN) {
		movingEye = 1;
		xEyeBegin = x;
		yEyeBegin = y;
	}
	if (button == eyeButton && state == GLUT_UP) {
		movingEye = 0;
	}
	if (button == lightButton && state == GLUT_DOWN) {
		movingLight = 1;
		xLightBegin = x;
		yLightBegin = y;
	}
	if (button == lightButton && state == GLUT_UP) {
		movingLight = 0;
		needDepthMapUpdate = 1;
		capturePrimary();
		glutPostRedisplay();
	}
}

void motion(int x, int y)
{
	if (movingEye) {
		eyeAngle = eyeAngle - 0.005*(x - xEyeBegin);
		eyeHeight = eyeHeight + 0.15*(y - yEyeBegin);
		if (eyeHeight > 20.0) eyeHeight = 20.0;
		if (eyeHeight < -0.0) eyeHeight = -0.0;
		xEyeBegin = x;
		yEyeBegin = y;
		needMatrixUpdate = 1;
		glutPostRedisplay();
	}
	if (movingLight) {
		lightAngle = lightAngle + 0.005*(x - xLightBegin);
		lightHeight = lightHeight - 0.15*(y - yLightBegin);
		if (lightHeight > 12.0) lightHeight = 12.0;
		if (lightHeight < -4.0) lightHeight = -4.0;
		xLightBegin = x;
		yLightBegin = y;
		needMatrixUpdate = 1;
		needDepthMapUpdate = 1;
		glutPostRedisplay();
	}
}

void depthBiasSelect(int depthBiasOption)
{
	GLfloat scale, bias;

	if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
		depthBias16 = depthBiasOption;
		scale = slopeScale;
		bias = depthBias16 * depthScale16;
	} else {
		depthBias24 = depthBiasOption;
		scale = slopeScale;
		bias = depthBias24 * depthScale24;
	}
	glPolygonOffset(scale, bias);
	needTitleUpdate = 1;
	needDepthMapUpdate = 1;
	glutPostRedisplay();
}

void initMenus(void)
{
	int viewMenu, frustumMenu, objectConfigMenu,
		depthMapMenu, depthBiasMenu;

	viewMenu = glutCreateMenu(selectMenu);
	glutAddMenuEntry("[s] Eye view with shadows", ME_EYE_VIEW_SHADOWED);
	glutAddMenuEntry("[S] Eye view with soft shadows", ME_EYE_VIEW_SOFTSHADOWED);
	glutAddMenuEntry("[l] Light view", ME_LIGHT_VIEW);

	frustumMenu = glutCreateMenu(selectMenu);
	glutAddMenuEntry("[f] Toggle", ME_TOGGLE_LIGHT_FRUSTUM);
	glutAddMenuEntry("Show", ME_ON_LIGHT_FRUSTUM);
	glutAddMenuEntry("Hide", ME_OFF_LIGHT_FRUSTUM);

	objectConfigMenu = glutCreateMenu(selectObjectConfig);
	glutAddMenuEntry("MGF", OC_MGF);

	depthMapMenu = glutCreateMenu(selectMenu);
	glutAddMenuEntry("[1] 64x64", ME_DEPTH_MAP_64);
	glutAddMenuEntry("[2] 128x128", ME_DEPTH_MAP_128);
	glutAddMenuEntry("[3] 256x256", ME_DEPTH_MAP_256);
	glutAddMenuEntry("[4] 512x512", ME_DEPTH_MAP_512);
	glutAddMenuEntry("[5] 1024x1024", ME_DEPTH_MAP_1024);

	depthBiasMenu = glutCreateMenu(depthBiasSelect);
	glutAddMenuEntry("2", 2);
	glutAddMenuEntry("4", 4);
	glutAddMenuEntry("6", 6);
	glutAddMenuEntry("8", 8);
	glutAddMenuEntry("10", 10);
	glutAddMenuEntry("100", 100);
	glutAddMenuEntry("512", 512);
	glutAddMenuEntry("0", 0);

	glutCreateMenu(selectMenu);
	glutAddSubMenu("View", viewMenu);
	glutAddSubMenu("Object configuration", objectConfigMenu);
	glutAddSubMenu("Light frustum", frustumMenu);
	glutAddSubMenu("Depth map resolution", depthMapMenu);
	glutAddSubMenu("Shadow depth bias", depthBiasMenu);
	glutAddMenuEntry("[m] Switch mouse control", ME_SWITCH_MOUSE_CONTROL);
	glutAddMenuEntry("[w] Toggle wire frame", ME_TOGGLE_WIRE_FRAME);
	glutAddMenuEntry("[F3] Toggle depth map back face culling", ME_TOGGLE_DEPTH_MAP_CULLING);
	glutAddMenuEntry("[F4] Toggle hardware shadow filtering", ME_TOGGLE_HW_SHADOW_FILTERING);
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
		if (!strcmp("-nodlist", argv[i])) {
			useDisplayLists = 0;
		}
		if (strstr(argv[i], ".mgf")) {
			mgf_filename = argv[i];
		}
		if (!strcmp("-depth24", argv[i])) {
			useDepth24 = 1;
		}
		if (!strcmp("-vsync", argv[i])) {
			vsync = 1;
		}
		if (!strcmp("-swapendian", argv[i])) {
			littleEndian = !littleEndian;
			printf("swap endian requested, assuming %s-endian\n",
				littleEndian ? "little" : "big");
		}
		if (!strcmp("-front", argv[i])) {
			drawFront = 1;
		}
	}
}

void setLittleEndian(void)
{
	unsigned short shortValue = 258;
	unsigned char *byteVersion = (unsigned char*) &shortValue;

	if (byteVersion[1] == 1) {
		assert(byteVersion[0] == 2);
		hwLittleEndian = 1;
	} else {
		assert(byteVersion[0] == 1);
		assert(byteVersion[1] == 2);
		hwLittleEndian = 0;
	}
	littleEndian = hwLittleEndian;
}

void idle()
{
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
			renderer->setStatus(RRObjectRenderer::CC_REFLECTED_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
		else
			updateIndirect();
#else
		renderer->setStatus(RRObjectRenderer::CC_REFLECTED_EXITANCE,RRCachingRenderer::CS_READY_TO_COMPILE);
#endif
		glutPostRedisplay();
	}
}

int
main(int argc, char **argv)
{
	setLittleEndian();

	rrCollider::registerLicense(
		"Illusion Softworks, a.s.",
		"DDEFMGDEFFBFFHJOCLBCFPMNHKENKPJNHDJFGKLCGEJFEOBMDC"
		"ICNMHGEJJHJACNCFBOGJKGKEKJBAJNDCFNBGIHMIBODFGMHJFI"
		"NJLGPKGNGOFFLLOGEIJMPBEADBJBJHGLJKGGFKOLDNIBIFENEK"
		"AJCOKCOALBDEEBIFIBJECMJDBPJMKOIJPCJGIOCCHGEGCJDGCD"
		"JDPKJEOJGMIEKNKNAOEENGMEHNCPPABBLLKGNCAPLNPAPNLCKM"
		"AGOBKPOMJK");
	rrVision::LicenseStatus status = rrVision::registerLicense(
		"Illusion Softworks, a.s.",
		"DDEFMGDEFFBFFHJOCLBCFPMNHKENKPJNHDJFGKLCGEJFEOBMDC"
		"ICNMHGEJJHJACNCFBOGJKGKEKJBAJNDCFNBGIHMIBODFGMHJFI"
		"NJLGPKGNGOFFLLOGEIJMPBEADBJBJHGLJKGGFKOLDNIBIFENEK"
		"AJCOKCOALBDEEBIFIBJECMJDBPJMKOIJPCJGIOCCHGEGCJDGCD"
		"JDPKJEOJGMIEKNKNAOEENGMEHNCPPABBLLKGNCAPLNPAPNLCKM"
		"AGOBKPOMJK");

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
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutIdleFunc(idle);

	/* Menu initialization depends on knowing what extensions are
	supported. */
	initMenus();

	initGL();

	if(!supports20())
	{
		puts("At least OpenGL 2.0 required.");
		return 0;
	}

#ifdef _3DS
	// load 3ds
	if(!m3ds.Load(filename_3ds)) return 1;
	rrobject = new_3ds_importer(&m3ds)->createAdditionalExitance();
#else
	// load mgf
	rrobject = new_mgf_importer(mgf_filename)->createAdditionalExitance();
#endif
	if(rrobject) printf("vertices=%d triangles=%d\n",rrobject->getCollider()->getImporter()->getNumVertices(),rrobject->getCollider()->getImporter()->getNumTriangles());
	rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,0);
	rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,1);
	rrVision::RRSetState(rrVision::RRSSF_SUBDIVISION_SPEED,0);
	//rrVision::RRSetState(rrVision::RRSSF_MIN_FEATURE_SIZE,1.0f);
	//rrVision::RRSetState(rrVision::RRSSF_MAX_SMOOTH_ANGLE,0.5f);
	rrscene = new rrVision::RRScene();
	rrscaler = rrVision::RRScaler::createGammaScaler(0.4f);
	rrscene->setScaler(rrscaler);
	rrscene->objectCreate(rrobject);
	renderer = new RRCachingRenderer(new RRGLObjectRenderer(rrobject,rrscene));
	//rr2gl_compile(rrobject,rrscene);
	glsl_init();

	glutMainLoop();
	return 0;
}
