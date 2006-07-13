//#define SHOW_CAPTURED_TRIANGLES
//#define DEFAULT_SPONZA
//#define MAX_FILTERED_INSTANCES   12  // max number of light instances with shadow filtering enabled
#define MAX_INSTANCES              50  // max number of light instances aproximating one area light
#define MAX_INSTANCES_PER_PASS     10
unsigned INSTANCES_PER_PASS = 2;//!!!7
#define INITIAL_INSTANCES_PER_PASS INSTANCES_PER_PASS
#define INITIAL_PASSES             1
#define AREA_SIZE                  0.15f
int fullscreen = 0;
int shadowSamples = 1;
bool lightIndirectMap = 0;
bool renderOnlyRr = false;
bool renderDiffuseTexture = true;
/*
! ati: s texturami je spatne shadowmaps>1, bez textur je spatne shadowmaps=1
! msvc: kdyz hybu svetlem, na konci hybani se smer kam sviti trochu zarotuje doprava
! v gcc dela m3ds renderer spatny indirect na koulich (na zdi je ok, v msvc je ok, v rrrendereru je ok)
! rr renderer presvetluje kdyz po startu zmacknu t-
rr renderer: pridat lightmapu aby aspon nekde behal muj primitivni unwrap

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
#include <GL/glut.h>

#include "RRIllumCalculator.h"

#include "glsl/Camera.hpp"
#include "glsl/GLSLProgram.hpp"
#include "glsl/GLSLShader.hpp"
#include "glsl/Texture.hpp"
#include "rr2gl.h"
#include "matrix.h"   /* OpenGL-style 4x4 matrix manipulation routines */

using namespace std;

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}


/////////////////////////////////////////////////////////////////////////////
//
// 3DS

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


/////////////////////////////////////////////////////////////////////////////
//
// RR

bool renderSource = false;
RRGLObjectRenderer* rendererNonCaching = NULL;
RRGLCachingRenderer* rendererCaching = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// GLSL

int currentWindowSize;
int softLight = -1; // current instance number 0..199, -1 = hard shadows, use instance 0

void checkGlError()
{
	/*
	GLenum err = glGetError();
	if(err!=GL_NO_ERROR)
	{
		printf("glGetError=%x\n",err);
	}
	*/
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture for shadow map

#define SHADOW_MAP_SIZE 512

class TextureShadowMap : public Texture
{
public:
	TextureShadowMap()
		: Texture(NULL, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, GL_DEPTH, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE,
			SHADOW_MAP_SIZE,0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
		channels = 1;
		// for shadow2D() instead of texture2D()
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
	}
	// sideeffect: binds texture
	void readFromBackbuffer()
	{
		//glGetIntegerv(GL_TEXTURE_2D,&oldTextureObject);
		bindTexture();
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// our OpenGL resources

GLUquadricObj *quadric;
TextureShadowMap* shadowMaps = NULL;
Texture *lightDirectMap;
GLSLProgram *lightProg, *ambientProg;
GLSLProgramSet* ubershaderProgSet;

void init_gl_resources()
{
	quadric = gluNewQuadric();

	shadowMaps = new TextureShadowMap[MAX_INSTANCES];
	lightDirectMap = new Texture("spot0.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);

	ubershaderProgSet = new GLSLProgramSet("shaders\\ubershader.vp", "shaders\\ubershader.fp");
	lightProg = new GLSLProgram(NULL,"shaders\\shadowmap.vp");
	ambientProg = new GLSLProgram(NULL,"shaders\\ambient.vp", "shaders\\ambient.fp");
}


/////////////////////////////////////////////////////////////////////////////


/* Draw modes. */
enum {
  DM_EYE_VIEW_SHADOWED,
  DM_EYE_VIEW_SOFTSHADOWED,
};

/* Menu items. */
enum {
  ME_TOGGLE_GLOBAL_ILLUMINATION,
  ME_TOGGLE_WIRE_FRAME,
  ME_TOGGLE_LIGHT_FRUSTUM,
  ME_SWITCH_MOUSE_CONTROL,
  ME_EXIT,
};

unsigned useLights = INITIAL_PASSES*INITIAL_INSTANCES_PER_PASS;
int softWidth[MAX_INSTANCES],softHeight[MAX_INSTANCES],softPrecision[MAX_INSTANCES],softFiltering[MAX_INSTANCES];
int areaType = 0; // 0=linear, 1=square grid, 2=circle

int depthBias24 = 42;
int depthScale24;
GLfloat slopeScale = 4.0;

GLfloat textureLodBias = 0.0;

void *font = GLUT_BITMAP_8_BY_13;

// light and camera setup
//Camera eye = {{0,1,4},3,0};
//Camera light = {{0,3,0},0.85f,8};
Camera eye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
Camera light = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,20.};

int xEyeBegin, yEyeBegin, movingEye = 0;
int xLightBegin, yLightBegin, movingLight = 0;
int wireFrame = 0;

int winWidth, winHeight;
int needMatrixUpdate = 1;
int needTitleUpdate = 1;
int needDepthMapUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
bool showHelp = 0;
int showLightViewFrustum = 1;
int eyeButton = GLUT_LEFT_BUTTON;
int lightButton = GLUT_MIDDLE_BUTTON;
int useDepth24 = 0;
int drawFront = 0;


/////////////////////////////////////////////////////////////////////////////
//
// MyApp

void drawHardwareShadowPass(RRGLObjectRenderer::ColorChannel cc);
void updateDepthMap(int mapIndex);

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
class MyApp : public rr::RRVisionApp
{
protected:
	virtual void detectMaterials()
	{
		delete[] surfaces;
		numSurfaces = m3ds.numMaterials;
		surfaces = new rr::RRSurface[numSurfaces];
		for(unsigned i=0;i<numSurfaces;i++)
		{
			surfaces[i].reset(false);
			surfaces[i].diffuseReflectance = rr::RRColor(m3ds.Materials[i].color.r/255.0,m3ds.Materials[i].color.g/255.0,m3ds.Materials[i].color.b/255.0);
		}
	}
	virtual void detectDirectIllumination()
	{
		if(!rendererCaching) return;

		/*/ prepare 1 depth map for faster hard-shadow capture
		zda se ze neni nutne, protoze drawHardwareShadowPass() pouzije pozici svetla 0, ktera koresponduje
		 s shadowmapou 0 a je dostatecne podobna prumerne shadowmape		 
		unsigned oldShadowMaps = useLights;
		useLights = 1;
		needDepthMapUpdate = 1;
		updateDepthMap(0);
		useLights = oldShadowMaps;*/
		
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

		rr::RRMesh* mesh = multiObject->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		//printf("%d %d\n",numTriangles,captureUv.xmax*captureUv.ymax);
		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			// clear
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene
			rendererNonCaching->setChannel(RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION);
			rendererCaching->setStatus(RRGLCachingRenderer::CS_NEVER_COMPILE);
			generateForcedUv = &captureUv;
			drawHardwareShadowPass(RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION);
			generateForcedUv = NULL;

			// Read back the index buffer to memory.
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// dbg print
			//rr::RRColor suma = rr::RRColor(0);

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
				multiObject->setTriangleAdditionalMeasure(triangleIndex,rr::RM_EXITANCE,avg);

				// debug print
				//rr::RRColor tmp = rr::RRColor(0);
				//rrobject->getTriangleAdditionalMeasure(triangleIndex,rr::RM_EXITING_FLUX,tmp);
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
	if (needTitleUpdate) 
	{
		char title[256];
		sprintf(title,"realtime radiosity viewer - %s",filename_3ds);
		glutSetWindowTitle(title);
		needTitleUpdate = 0;
	}
}


/*** DRAW VARIOUS SCENES ***/

static void drawSphere(void)
{
	gluSphere(quadric, 0.55, 16, 16);
}

/*
struct UbershaderDefines
{
	// these values are directly passed into ubershader defines
	unsigned SHADOW_MAPS            :8;
	unsigned SHADOW_SAMPLES         :8;
	bool     LIGHT_DIRECT           :1;
	bool     LIGHT_DIRECT_MAP       :1;
	bool     LIGHT_INDIRECT_COLOR   :1;
	bool     LIGHT_INDIRECT_MAP     :1;
	bool     MATERIAL_DIFFUSE_COLOR :1;
	bool     MATERIAL_DIFFUSE_MAP   :1;
	bool     FORCE_2D_POSITION      :1;

	UbershaderConfig()
	{
		memset(this,0,sizeof(*this));
	}
};
*/

bool define_lightIndirectXxx = false;
//v rendru bez textur se materialColor i indirectColor nacitaj z gl_Color
//je nutne (pri vyplych texturach) vypnout optimalizaci - slouceni primary a indirectu do 1 passu

GLSLProgram* getProgramCore(RRGLObjectRenderer::ColorChannel cc)
{
	switch(cc)
	{
		case RRGLObjectRenderer::CC_NO_COLOR:
			return lightProg;
		case RRGLObjectRenderer::CC_TRIANGLE_INDEX:
			return ambientProg;
		case RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE:
		case RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
		case RRGLObjectRenderer::CC_REFLECTED_IRRADIANCE:
		case RRGLObjectRenderer::CC_REFLECTED_EXITANCE:
			{
			GLSLProgramSet* progSet = ubershaderProgSet;
			static char tmp[200];
			bool force2d = cc==RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION;
			bool lightIndirect1 = cc==RRGLObjectRenderer::CC_REFLECTED_IRRADIANCE || cc==RRGLObjectRenderer::CC_REFLECTED_EXITANCE;
			bool lightIndirectColor1 = define_lightIndirectXxx && !lightIndirectMap;
			bool lightIndirectMap1 = define_lightIndirectXxx && lightIndirectMap;
			bool materialDiffuseColor = cc==RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION || (!lightIndirectColor1 && !renderDiffuseTexture);
			bool materialDiffuseMap = !materialDiffuseColor && renderDiffuseTexture;
			assert(!lightIndirectColor1 || !materialDiffuseColor);
retry:
			sprintf(tmp,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s",
				(lightIndirect1 || force2d || softLight<0)?1:MIN(useLights,INSTANCES_PER_PASS),
				lightIndirect1?0:((force2d || softLight<0)?1:shadowSamples),
				lightIndirect1?"":"#define LIGHT_DIRECT\n#define LIGHT_DIRECT_MAP\n",
				lightIndirectColor1?"#define LIGHT_INDIRECT_COLOR\n":"",
				lightIndirectMap1?"#define LIGHT_INDIRECT_MAP\n":"",
				materialDiffuseColor?"#define MATERIAL_DIFFUSE_COLOR\n":"",
				materialDiffuseMap?"#define MATERIAL_DIFFUSE_MAP\n":"",
				force2d?"#define FORCE_2D_POSITION\n":""
				);
			GLSLProgram* prog = progSet->getVariant(tmp);
			if(!prog && INSTANCES_PER_PASS>1)
			{
				printf("MAPS_PER_PASS=%d too much, trying %d...\n",INSTANCES_PER_PASS,INSTANCES_PER_PASS-1);
				INSTANCES_PER_PASS--;
				useLights = INITIAL_PASSES*INSTANCES_PER_PASS;
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
GLSLProgram* getProgram(RRGLObjectRenderer::ColorChannel cc)
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

GLSLProgram* setProgram(RRGLObjectRenderer::ColorChannel cc)
{
	GLSLProgram* tmp = getProgram(cc);
	checkGlError();
	tmp->useIt();
	checkGlError();
	return tmp;
}

void drawScene(RRGLObjectRenderer::ColorChannel cc)
{
	checkGlError();

	if(!renderOnlyRr && cc==RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(NULL,define_lightIndirectXxx?app:NULL,lightIndirectMap);
		return;
	}
	if(!renderOnlyRr && (cc==RRGLObjectRenderer::CC_SOURCE_IRRADIANCE || cc==RRGLObjectRenderer::CC_REFLECTED_IRRADIANCE))
	{
		m3ds.Draw(NULL,app,lightIndirectMap);
		return;
	}

	rendererNonCaching->setChannel(cc);
	rendererCaching->render();
	checkGlError();
}

void setProgramAndDrawScene(RRGLObjectRenderer::ColorChannel cc)
{
	if(cc==RRGLObjectRenderer::CC_REFLECTED_AUTO)
	{
		if(renderDiffuseTexture)
			cc = RRGLObjectRenderer::CC_REFLECTED_IRRADIANCE; // for textured 3ds output
		else
			cc = RRGLObjectRenderer::CC_REFLECTED_EXITANCE; // for colored output
	}
	if(cc==RRGLObjectRenderer::CC_SOURCE_AUTO)
	{
		if(renderDiffuseTexture)
			cc = RRGLObjectRenderer::CC_SOURCE_IRRADIANCE; // for textured 3ds output
		else
			cc = RRGLObjectRenderer::CC_SOURCE_EXITANCE; // for colored output
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
	gluSphere(quadric, 0.05, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	eye.update(0);
	light.update(0.3f);
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

	glEnable(GL_LINE_STIPPLE);
	glPushMatrix();
	glMultMatrixd(light.inverseViewMatrix);
	glMultMatrixd(light.inverseFrustumMatrix);
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

	light.setupForRender();

	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glEnable(GL_POLYGON_OFFSET_FILL);

	setProgramAndDrawScene(RRGLObjectRenderer::CC_NO_COLOR);
	shadowMaps[(softLight>=0)?softLight:0].readFromBackbuffer();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);

	if(softLight<0 || softLight==(int)(useLights-1)) 
	{
		needDepthMapUpdate = 0;
	}
}

void drawHardwareShadowPass(RRGLObjectRenderer::ColorChannel cc)
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
	int instances = (softLight>=0 && cc==RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE)?MIN(useLights,INSTANCES_PER_PASS):1;
	checkGlError();
	for(int i=0;i<instances;i++)
	{
		glActiveTexture(GL_TEXTURE0+i);
		glEnable(GL_TEXTURE_2D);
		// prepare samplers
		shadowMaps[((softLight>=0)?softLightBase:0)+i].bindTexture();
		samplers[i]=i;
		// prepare and send matrices
		if(i && softLight>=0)
			placeSoftLight(softLightBase+i); // calculate light position
		glLoadMatrixd(tmp);
		glMultMatrixd(light.frustumMatrix);
		glMultMatrixd(light.viewMatrix);
	}
	checkGlError();
	myProg->sendUniform("shadowMap", instances, samplers);
	checkGlError();
	glMatrixMode(GL_MODELVIEW);

	// lightDirectPos (in object space)
	myProg->sendUniform("lightDirectPos",light.pos[0],light.pos[1],light.pos[2]);

	// lightDirectMap
	glActiveTexture(GL_TEXTURE10);
	glEnable(GL_TEXTURE_2D);
	lightDirectMap->bindTexture();
	myProg->sendUniform("lightDirectMap", 10);

	// lightIndirectMap
	if(lightIndirectMap)
	{
		glActiveTexture(GL_TEXTURE12);
		glEnable(GL_TEXTURE_2D);
		myProg->sendUniform("lightIndirectMap", 12);
	}

	// materialDiffuseMap (last before drawScene, must stay active)
	if(renderDiffuseTexture && cc!=RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION) // kdyz detekuju source (->force 2d), pouzivam RRObjectRenderer, takze jedem bez difus textur
	{
		glActiveTexture(GL_TEXTURE11);
		glEnable(GL_TEXTURE_2D);
		myProg->sendUniform("materialDiffuseMap", 11);
	}

	drawScene(cc);

	// disable texturing
	for(unsigned i=0;i<13;i++)
	{
		glActiveTexture(GL_TEXTURE0+i);
		glDisable(GL_TEXTURE_2D);
	}

	glActiveTexture(GL_TEXTURE0);
}

void drawEyeViewShadowed()
{
	if(softLight<0) updateDepthMap(softLight);

	glClear(GL_COLOR_BUFFER_BIT);
	if(softLight<=0) glClear(GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	eye.setupForRender();

	drawHardwareShadowPass(RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE);

	drawLight();
	if (showLightViewFrustum) drawShadowMapFrustum();
}

void drawEyeViewSoftShadowed(void)
{
	// optimized path without accum, only for m3ds, rrrenderer can't render both materialColor and indirectColor
	if(useLights<=INSTANCES_PER_PASS && !renderOnlyRr)
	{
		define_lightIndirectXxx = false;
		placeSoftLight(-1);
		for(unsigned i=0;i<useLights;i++)
		{
			updateDepthMap(i);
		}
		if (wireFrame) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		placeSoftLight(0);
		define_lightIndirectXxx = true;
		drawEyeViewShadowed();
		placeSoftLight(-2);
		define_lightIndirectXxx = false;
		return;
	}

	// add direct
	define_lightIndirectXxx = false;
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
		define_lightIndirectXxx = true;
#ifdef SHOW_CAPTURED_TRIANGLES
		generateForcedUv = &captureUv;
		captureUv.firstCapturedTriangle = 0;
		setProgramAndDrawScene(RRGLObjectRenderer::CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION); // pro color exitance, pro texturu irradiance
#else
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		setProgramAndDrawScene(RRGLObjectRenderer::CC_REFLECTED_AUTO); // pro color exitance, pro texturu irradiance
#endif

		glAccum(GL_ACCUM,1);
		define_lightIndirectXxx = false;
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
		"        Stepan Hrbek, http://dee.cz",
		"        radiosity engine, http://lightsprint.com",
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

	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
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
		case DM_EYE_VIEW_SHADOWED:
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

	LIMITED_TIMES(1,capturePrimary());
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
	switch (item) 
	{
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
	Camera* cam = movingLight?&light:&eye;
	switch (c) 
	{
		case GLUT_KEY_F7:
			benchmark(1);
			return;
		case GLUT_KEY_F8:
			benchmark(0);
			return;
		case GLUT_KEY_F9:
			printf("\nCamera eye = {{%f,%f,%f},%f,%f};\n",eye.pos[0],eye.pos[1],eye.pos[2],eye.angle,eye.height);
			printf("Camera light = {{%f,%f,%f},%f,%f};\n",light.pos[0],light.pos[1],light.pos[2],light.angle,light.height);
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
	switch (c)
	{
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
			eye.fieldOfView = (eye.fieldOfView == 100.0) ? 50.0 : 100.0;
			needMatrixUpdate = true;
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
			light.anear *= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'N':
			light.anear /= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'c':
			light.afar *= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'C':
			light.afar /= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'p':
			light.fieldOfView -= 5.0;
			if (light.fieldOfView < 5.0) {
				light.fieldOfView = 5.0;
			}
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'P':
			light.fieldOfView += 5.0;
			if (light.fieldOfView > 160.0) {
				light.fieldOfView = 160.0;
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
				if(useLights==1 && useLights<INSTANCES_PER_PASS)
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
	eye.aspect = (double) winWidth / (double) winHeight;
	needMatrixUpdate = true;

	/* Perhaps there might have been a mode change so at window
	reshape time, redetermine the depth scale. */
	updateDepthScale();
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

void idle()
{
	if(app->calculate()==rr::RRScene::IMPROVED)
	{
		rendererNonCaching->setChannel(RRGLObjectRenderer::CC_REFLECTED_EXITANCE);
		rendererCaching->setStatus(RRGLCachingRenderer::CS_READY_TO_COMPILE);
		rendererNonCaching->setChannel(RRGLObjectRenderer::CC_REFLECTED_IRRADIANCE);
		rendererCaching->setStatus(RRGLCachingRenderer::CS_READY_TO_COMPILE);
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

void init_gl_states()
{
	GLint depthBits;

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
	glEnable(GL_NORMALIZE);

	updateDepthScale();
	updateDepthBias(0);  /* Update with no offset change. */

	if (drawFront) {
		glDrawBuffer(GL_FRONT);
		glReadBuffer(GL_FRONT);
	}

	glShadeModel(GL_SMOOTH);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
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
		if (!strcmp("-window", argv[i])) {
			fullscreen = 0;
		}
		if (strstr(argv[i], ".3ds") || strstr(argv[i], ".3DS")) {
			filename_3ds = argv[i];
			scale_3ds = 1;
		}
		if (!strcmp("-depth24", argv[i])) {
			useDepth24 = 1;
		}
		if (!strcmp("-front", argv[i])) {
			drawFront = 1;
		}
	}
}

int main(int argc, char **argv)
{
	rr::RRLicense::registerLicense("","");

	glutInitWindowSize(800, 600);
	glutInit(&argc, argv);
	parseOptions(argc, argv);

	if (useDepth24) {
		glutInitDisplayString("depth~24 rgb double accum");
	} else {
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
	}
	glutCreateWindow("shadowcast");
	if(fullscreen)
	{
		glutFullScreen();
	}

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
	glutMotionFunc(motion);
	glutIdleFunc(idle);

	/* Menu initialization depends on knowing what extensions are
	supported. */
	initMenus();

	if (strstr(filename_3ds, "koupelna3")) {
		scale_3ds = 0.01f;
	}
	if (strstr(filename_3ds, "koupelna4")) {
		scale_3ds = 0.03f;
		//Camera koupelna4_eye = {{0.032202,1.659255,1.598609},10.010005,-0.150000};
		//Camera koupelna4_light = {{-1.309976,0.709500,0.498725},3.544996,-10.000000};
		Camera koupelna4_eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
		Camera koupelna4_light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
		eye = koupelna4_eye;
		light = koupelna4_light;
	}
	if (strstr(filename_3ds, "sponza"))
	{
		Camera sponza_eye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999, 1.,100.,0.3,60.};
		Camera sponza_light = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001, 1.,70.,1.,20.};
		//Camera sponza_eye = {{-10.407576,1.605258,4.050256},7.859994,-0.050000};
		//Camera sponza_light = {{-7.109047,5.130751,-2.025017},0.404998,2.950001};
		//lightFieldOfView += 10.0;
		//eyeFieldOfView = 50.0;
		eye = sponza_eye;
		light = sponza_light;
	}

	//if(rrobject) printf("vertices=%d triangles=%d\n",rrobject->getCollider()->getMesh()->getNumVertices(),rrobject->getCollider()->getMesh()->getNumTriangles());
	rr::RRScene::setStateF(rr::RRScene::SUBDIVISION_SPEED,0);
	rr::RRScene::setState(rr::RRScene::GET_SOURCE,0);
	rr::RRScene::setState(rr::RRScene::GET_REFLECTED,1);
	//rr::RRScene::setState(rr::GET_SMOOTH,0);
	rr::RRScene::setStateF(rr::RRScene::MIN_FEATURE_SIZE,0.15f);
	//rr::RRScene::setStateF(rr::MAX_SMOOTH_ANGLE,0.4f);

	init_gl_states();
	init_gl_resources();

	if(!supports20())
	{
		puts("At least OpenGL 2.0 required.\n\nHit enter to close...");
		fgetc(stdin);
		exit(0);
	}

	printf("Loading and preprocessing scene (~15 sec)...");

	// load 3ds
	if(!m3ds.Load(filename_3ds,scale_3ds)) return 1;
	//m3ds.shownormals=1;
	//m3ds.numObjects=2;//!!!
	app = new MyApp();
	new_3ds_importer(&m3ds,app);

//	printf(app->getObject(0)->getCollider()->getMesh()->save("c:\\a")?"saved":"not saved");
//	printf(app->getObject(0)->getCollider()->getMesh()->load("c:\\a")?" / loaded":" / not loaded");
	printf("\n");
	app->calculate();
	rendererNonCaching = new RRGLObjectRenderer(app->multiObject,app->scene);
	rendererCaching = new RRGLCachingRenderer(rendererNonCaching);
	glutMainLoop();
	return 0;
}
