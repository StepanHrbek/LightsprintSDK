// --------------------------------------------------------------------------
// Viewer of scene.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/Timer.h"
#include <cstdio>
#include <GL/glew.h>
#include <GL/glut.h>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if(gfxRelated)
		rr::RRReporter::report(rr::INF1,"Please update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families\n");
	if(glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
		glutLeaveGameMode();
	else
		glutDestroyWindow(glutGetWindow());
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

class Solver*              solver = NULL;
Camera                     eye(-1.856f,1.440f,2.097f,2.404f,0,0.02f,1.3f,90,0.1f,1000);
enum SelectionType {ST_CAMERA, ST_LIGHT, ST_OBJECT};
SelectionType              selectedType = ST_CAMERA;
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
unsigned                   selectedObjectIndex = 0; // index into static objects
int                        winWidth = 0;
int                        winHeight = 0;
bool                       renderLights = 1;
bool                       renderAmbient = 0;
float                      speedGlobal = 1;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
float                      speedUp = 0;
float                      speedDown = 0;
float                      speedLean = 0;
rr::RRVec4                 brightness(1);
float                      gamma = 1;


/////////////////////////////////////////////////////////////////////////////
//
// GI solver and renderer

class Solver : public RRDynamicSolverGL
{
public:
	RendererOfScene* rendererOfScene;

	Solver(const char* pathToShaders) : RRDynamicSolverGL(pathToShaders)
	{
		rendererOfScene = new RendererOfScene(this,pathToShaders);
	}
	~Solver()
	{
		delete rendererOfScene;
	}
	virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
	{
		const rr::RRVector<RealtimeLight*>* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

		// render static scene
		rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight,honourExpensiveLightingShadowingFlags);
		rendererOfScene->useOptimizedScene();
		rendererOfScene->setBrightnessGamma(&brightness,gamma);
		rendererOfScene->render();
	}

protected:
	virtual unsigned* detectDirectIllumination()
	{
		if(!winWidth) return NULL;
		return RRDynamicSolverGL::detectDirectIllumination();
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// menu

class Menu
{
public:
	Menu(Solver* solver)
	{
		// select submenu
		int selectHandle = glutCreateMenu(selectCallback);
		char buf[100];
		glutAddMenuEntry("camera", -1);
		for(unsigned i=0;i<solver->getLights().size();i++)
		{
			sprintf(buf,"light %d",i);
			glutAddMenuEntry(buf, i);
		}
		for(unsigned i=0;i<solver->getStaticObjects().size();i++)
		{
			sprintf(buf,"object %d",i);
			glutAddMenuEntry(buf, 1000+i);
		}

		// speed submenu
		int speedHandle = glutCreateMenu(speedCallback);
		glutAddMenuEntry("1/256", 1);
		glutAddMenuEntry("1/64", 4);
		glutAddMenuEntry("1/16", 16);
		glutAddMenuEntry("1/4", 64);
		glutAddMenuEntry("1", 256);
		glutAddMenuEntry("4", 1024);
		glutAddMenuEntry("16", 4096);
		glutAddMenuEntry("64", 16384);
		glutAddMenuEntry("256", 65536);

		// main menu
		glutCreateMenu(mainCallback);
		glutAddSubMenu("Select...", selectHandle);
		glutAddSubMenu("Speed...", speedHandle);
		glutAddMenuEntry("Toggle render ambient", ME_RENDER_AMBIENT);
		glutAddMenuEntry("Toggle render helpers", ME_RENDER_HELPERS);
		glutAddMenuEntry("Toggle honour expensive flags", ME_HONOUR_FLAGS);
		glutAttachMenu(GLUT_RIGHT_BUTTON);
	}
	static void mainCallback(int item)
	{
		switch(item)
		{
			case ME_RENDER_AMBIENT: renderAmbient = !renderAmbient; break;
			case ME_RENDER_HELPERS: renderLights = !renderLights; break;
			case ME_HONOUR_FLAGS: solver->honourExpensiveLightingShadowingFlags = !solver->honourExpensiveLightingShadowingFlags;
				for(unsigned i=0;i<solver->realtimeLights.size();i++) solver->realtimeLights[i]->dirty = true; // update all for new flags
				break;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void selectCallback(int item)
	{
		if(item<0) selectedType = ST_CAMERA;
		if(item>=0 && item<1000) {selectedType = ST_LIGHT; selectedLightIndex = item;}
		if(item>=1000) {selectedType = ST_OBJECT; selectedObjectIndex = item-1000;}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void speedCallback(int item)
	{
		speedGlobal = item/256.f;
		glutWarpPointer(winWidth/2,winHeight/2);
	}
protected:
	enum
	{
		ME_RENDER_AMBIENT,
		ME_RENDER_HELPERS,
		ME_HONOUR_FLAGS,
	};
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void special(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 1; break;
		case GLUT_KEY_DOWN: speedBack = 1; break;
		case GLUT_KEY_LEFT: speedLeft = 1; break;
		case GLUT_KEY_RIGHT: speedRight = 1; break;
		case GLUT_KEY_PAGE_UP: speedUp = 1; break;
		case GLUT_KEY_PAGE_DOWN: speedDown = 1; break;
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
		case GLUT_KEY_PAGE_UP: speedUp = 0; break;
		case GLUT_KEY_PAGE_DOWN: speedDown = 0; break;
	}
}

void keyboard(unsigned char c, int x, int y)
{
	switch (c)
	{
		case '+': brightness *= 1.2f; break;
		case '-': brightness /= 1.2f; break;
		case '*': gamma *= 1.2f; break;
		case '/': gamma /= 1.2f; break;

		case 'a':
		case 'A': speedLeft = 1; break;
		case 's':
		case 'S': speedBack = 1; break;
		case 'd':
		case 'D': speedRight = 1; break;
		case 'w':
		case 'W': speedForward = 1; break;
		case 'q':
		case 'Q': speedUp = 1; break;
		case 'z':
		case 'Z': speedDown = 1; break;
		case 'x':
		case 'X': speedLean = -1; break;
		case 'c':
		case 'C': speedLean = +1; break;

		//case 27:
		//	delete solver;
		//	exit(0);
	}
	solver->reportInteraction();
}

void keyboardUp(unsigned char c, int x, int y)
{
	switch (c)
	{
		case 'a':
		case 'A': speedLeft = 0; break;
		case 's':
		case 'S': speedBack = 0; break;
		case 'd':
		case 'D': speedRight = 0; break;
		case 'w':
		case 'W': speedForward = 0; break;
		case 'q':
		case 'Q': speedUp = 0; break;
		case 'z':
		case 'Z': speedDown = 0; break;
		case 'x':
		case 'X': speedLean = 0; break;
		case 'c':
		case 'C': speedLean = 0; break;
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = winWidth/(float)winHeight;
	Texture* texture = Texture::createShadowmap(64,64);
	GLint shadowDepthBits = texture->getTexelBits();
	delete texture;
	glPolygonOffset(1,(GLfloat)(12<<(shadowDepthBits-16)));
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && solver->realtimeLights.size())
	{
		if(selectedType!=ST_CAMERA) selectedType = ST_CAMERA;
		else selectedType = ST_LIGHT;
	}
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView<130) eye.fieldOfView+=10;
	}
	solver->reportInteraction();
}

void passive(int x, int y)
{
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		if(selectedType==ST_CAMERA || selectedType==ST_OBJECT)
		{
			eye.angle -= 0.005f*x;
			eye.angleX -= 0.005f*y;
			CLAMP(eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
		}
		else
		if(selectedType==ST_LIGHT)
		{
			Camera* light = solver->realtimeLights[selectedLightIndex]->getParent();
			light->angle -= 0.005f*x;
			light->angleX -= 0.005f*y;
			CLAMP(light->angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
			solver->reportDirectIlluminationChange(true);
			solver->realtimeLights[selectedLightIndex]->dirty = true;
			// changes position a bit, together with rotation
			// if we don't call it, solver updates light in a standard way, without position change
			light->pos += light->dir*0.3f;
			light->update();
			light->pos -= light->dir*0.3f;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
		solver->reportInteraction();
	}
}

static void textOutput(int x, int y, const char *format, ...)
{
	char text[1000];
	va_list argptr;
	va_start (argptr,format);
	vsprintf (text,format,argptr);
	va_end (argptr);
	glRasterPos2i(x,y);
	int len = (int)strlen(text);
	for(int i=0;i<len;i++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15,text[i]);
}

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	eye.update();

	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = renderAmbient;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	solver->renderScene(uberProgramSetup,NULL);

	if(renderLights)
	{
		// render light frames
		solver->renderLights();

		// render lines
		glBegin(GL_LINES);
		enum {LINES=100, SIZE=100};
		for(unsigned i=0;i<LINES+1;i++)
		{
			if(i==LINES/2)
			{
				glColor3f(0,0,1);
				glVertex3f(0,-0.5*SIZE,0);
				glVertex3f(0,+0.5*SIZE,0);
			}
			else
				glColor3f(0,0,0.3f);
			float q = (i/float(LINES)-0.5f)*SIZE;
			glVertex3f(q,0,-0.5*SIZE);
			glVertex3f(q,0,+0.5*SIZE);
			glVertex3f(-0.5*SIZE,0,q);
			glVertex3f(+0.5*SIZE,0,q);
		}
		glEnd();

		// render properties
		winWidth = glutGet(GLUT_WINDOW_WIDTH);
		winHeight = glutGet(GLUT_WINDOW_HEIGHT);
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, winWidth, winHeight, 0);
		glColor3f(1,1,1);
		glRasterPos2i(winWidth/2-4,winHeight/2+1);
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15,'.');
		int x = 10;
		int y = 10;
		textOutput(x,y+=18,"right mouse button = menu");
		{
			textOutput(x,y+=18*2,"[camera]");
			textOutput(x,y+=18,"pos: %f %f %f",eye.pos[0],eye.pos[1],eye.pos[2]);
			textOutput(x,y+=18,"dir: %f %f %f",eye.dir[0],eye.dir[1],eye.dir[2]);
		}
		unsigned numObjects = solver->getNumObjects();
		unsigned numLights = solver->getLights().size();
		const rr::RRObject* multiObject = solver->getMultiObjectCustom();
		const rr::RRObject* singleObject = solver->getObject(selectedObjectIndex);
		const rr::RRMesh* multiMesh = multiObject->getCollider()->getMesh();
		const rr::RRMesh* singleMesh = singleObject->getCollider()->getMesh();
		unsigned numTrianglesMulti = multiMesh->getNumTriangles();
		unsigned numTrianglesSingle = singleMesh->getNumTriangles();
		if(selectedLightIndex<solver->realtimeLights.size())
		{
			RealtimeLight* rtlight = solver->realtimeLights[selectedLightIndex];
			const rr::RRLight* rrlight = rtlight->origin;
			Camera* light = rtlight->getParent();
			textOutput(x,y+=18*2,"[light %d/%d]",selectedLightIndex,solver->realtimeLights.size());
			textOutput(x,y+=18,"type: %s",(rrlight->type==rr::RRLight::POINT)?"point":((rrlight->type==rr::RRLight::SPOT)?"spot":"dir"));
			textOutput(x,y+=18,"pos: %f %f %f",light->pos[0],light->pos[1],light->pos[2]);
			textOutput(x,y+=18,"dir: %f %f %f",light->dir[0],light->dir[1],light->dir[2]);
			textOutput(x,y+=18,"color: %f %f %f",rrlight->color[0],rrlight->color[1],rrlight->color[2]);
			switch(rrlight->distanceAttenuationType)			
			{
				case rr::RRLight::NONE:        textOutput(x,y+=18,"dist att: none"); break;
				case rr::RRLight::PHYSICAL:    textOutput(x,y+=18,"dist att: physically correct"); break;
				case rr::RRLight::POLYNOMIAL:  textOutput(x,y+=18,"dist att: 1/(%f+%f*d+%f*d^2)",rrlight->polynom[0],rrlight->polynom[1],rrlight->polynom[2]); break;
				case rr::RRLight::EXPONENTIAL: textOutput(x,y+=18,"dist att: max(0,1-distance/%f)^%f)",rrlight->radius,rrlight->fallOffExponent); break;
			}
			static RealtimeLight* lastLight = NULL;
			static unsigned numLightReceivers = 0;
			static unsigned numShadowCasters = 0;
			if(rtlight!=lastLight)
			{
				lastLight = rtlight;
				numLightReceivers = 0;
				numShadowCasters = 0;
				for(unsigned t=0;t<numTrianglesMulti;t++)
				{
					if(multiObject->getTriangleMaterial(t,rrlight,NULL)) numLightReceivers++;
					for(unsigned j=0;j<numObjects;j++)
					{
						if(multiObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowCasters++;
					}
				}
			}
			textOutput(x,y+=18,"triangles lit: %d/%d",numLightReceivers,numTrianglesMulti);
			textOutput(x,y+=18,"triangles casting shadow: %f/%d",numShadowCasters/float(numObjects),numTrianglesMulti);
		}
		if(selectedObjectIndex<solver->getNumObjects())
		{
			textOutput(x,y+=18*2,"[static object %d/%d]",selectedObjectIndex,numObjects);
			textOutput(x,y+=18,"triangles: %d/%d",numTrianglesSingle,numTrianglesMulti);
			textOutput(x,y+=18,"vertices: %d/%d",singleMesh->getNumVertices(),multiMesh->getNumVertices());
			static const rr::RRObject* lastObject = NULL;
			static unsigned numReceivedLights = 0;
			static unsigned numShadowsCast = 0;
			if(singleObject!=lastObject)
			{
				lastObject = singleObject;
				numReceivedLights = 0;
				numShadowsCast = 0;
				for(unsigned i=0;i<numLights;i++)
				{
					rr::RRLight* rrlight = solver->getLights()[i];
					for(unsigned t=0;t<numTrianglesSingle;t++)
					{
						if(singleObject->getTriangleMaterial(t,rrlight,NULL)) numReceivedLights++;
						for(unsigned j=0;j<numObjects;j++)
						{
							if(singleObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowsCast++;
						}
					}
				}
			}
			textOutput(x,y+=18,"received lights: %f/%d",numReceivedLights/float(numTrianglesSingle),numLights);
			textOutput(x,y+=18,"shadows cast: %f/%d",numShadowsCast/float(numTrianglesSingle),numLights*numObjects);
		}
		{
			rr::RRRay* ray = rr::RRRay::create();
			rr::RRVec3 dir = eye.dir.RRVec3::normalized();
			ray->rayOrigin = eye.pos;
			ray->rayDirInv[0] = 1/dir[0];
			ray->rayDirInv[1] = 1/dir[1];
			ray->rayDirInv[2] = 1/dir[2];
			ray->rayLengthMin = 0;
			ray->rayLengthMax = 10000;
			ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			if(solver->getMultiObjectCustom()->getCollider()->intersect(ray))
			{
				rr::RRMesh::MultiMeshPreImportNumber preTriangle = multiMesh->getPreImportTriangle(ray->hitTriangle);
				const rr::RRMaterial* material = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
				rr::RRMaterial pointMaterial;
				if(material->sideBits[ray->hitFrontSide?0:1].pointDetails)
				{
					multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
					material = &pointMaterial;
				}
				rr::RRMesh::TriangleMapping triangleMapping;
				multiMesh->getTriangleMapping(ray->hitTriangle,triangleMapping);
				rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + triangleMapping.uv[1]*ray->hitPoint2d[0] + triangleMapping.uv[2]*ray->hitPoint2d[1];
				textOutput(x,y+=18*2,"[point in the middle of viewport]");
				textOutput(x,y+=18,"object: %d/%d",preTriangle.object,numObjects);
				textOutput(x,y+=18,"triangle in object: %d/%d",preTriangle.index,numTrianglesSingle);
				textOutput(x,y+=18,"triangle in scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
				textOutput(x,y+=18,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
				textOutput(x,y+=18,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
				textOutput(x,y+=18,"distance: %f",ray->hitDistance);
				textOutput(x,y+=18,"pos: %f %f %f",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
				textOutput(x,y+=18,"norm: %f %f %f",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2]);
				textOutput(x,y+=18,"side: %s",ray->hitFrontSide?"front":"back");
				textOutput(x,y+=18,"material: %s",(material!=&pointMaterial)?"per-triangle":"per-vertex");
				textOutput(x,y+=18,"diffuse refl: %f %f %f",material->diffuseReflectance[0],material->diffuseReflectance[1],material->diffuseReflectance[2]);
				textOutput(x,y+=18,"specular refl: %f",material->specularReflectance);
				textOutput(x,y+=18,"transmittance: %f %f %f",material->specularTransmittance[0],material->specularTransmittance[1],material->specularTransmittance[2]);
				textOutput(x,y+=18,"refraction index: %f",material->refractionIndex);
				textOutput(x,y+=18,"dif.emittance: %f %f %f",material->diffuseEmittance[0],material->diffuseEmittance[1],material->diffuseEmittance[2]);
				unsigned numReceivedLights = 0;
				unsigned numShadowsCast = 0;
				for(unsigned i=0;i<numLights;i++)
				{
					rr::RRLight* rrlight = solver->getLights()[i];
					if(multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,NULL)) numReceivedLights++;
					for(unsigned j=0;j<numObjects;j++)
					{
						if(multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,solver->getObject(j))) numShadowsCast++;
					}
				}
				textOutput(x,y+=18,"received lights: %d/%d",numReceivedLights,numLights);
				textOutput(x,y+=18,"shadows cast: %d/%d",numShadowsCast,numLights*numObjects);
			}
			textOutput(x,y+=18*2,"numbers of casters/lights show potential, what is allowed");
		}
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glEnable(GL_DEPTH_TEST);
	}

	glutSwapBuffers();
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
		seconds *= speedGlobal;
		Camera* cam = (selectedType!=ST_LIGHT)?&eye:solver->realtimeLights[selectedLightIndex]->getParent();
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedUp) cam->moveUp(speedUp*seconds);
		if(speedDown) cam->moveDown(speedDown*seconds);
		if(speedLean) cam->lean(speedLean*seconds);
		if(speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
		{
			if(cam!=&eye) 
			{
				solver->reportDirectIlluminationChange(true);
				solver->realtimeLights[selectedLightIndex]->dirty = true;
				if(speedForward) cam->moveForward(speedForward*seconds);
			}
		}
	}
	prev = now;

	glutPostRedisplay();
}

void sceneViewer(rr::RRDynamicSolver* _solver, const char* _pathToShaders, bool _honourExpensiveLightingShadowingFlags)
{
	// init GLUT
	int argc=1;
	char* argv[] = {"abc",NULL};
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	unsigned w = glutGet(GLUT_SCREEN_WIDTH);
	unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
	unsigned resolutionx = w-128;
	unsigned resolutiony = h-64;
	glutInitWindowSize(resolutionx,resolutiony);
	glutInitWindowPosition((w-resolutionx)/2,(h-resolutiony)/2);
	glutCreateWindow("Lightsprint Debug Console");
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);
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

	// init solver
	solver = new Solver(_pathToShaders);
	solver->honourExpensiveLightingShadowingFlags = _honourExpensiveLightingShadowingFlags;
	solver->setScaler(_solver->getScaler());
	solver->setEnvironment(_solver->getEnvironment());
	solver->setStaticObjects(_solver->getStaticObjects(),NULL);
	solver->setLights(_solver->getLights());
	char buf[1000];
	_snprintf(buf,999,"%s%s",_pathToShaders,"../maps/spot0.png");
	buf[999] = 0;
	Texture* lightDirectMap = Texture::load(buf, NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	for(unsigned i=0;i<solver->realtimeLights.size();i++)
		solver->realtimeLights[i]->lightDirectMap = lightDirectMap;
	solver->observer = &eye; // solver automatically updates lights that depend on camera
	//solver->loadFireball(NULL) || solver->buildFireball(5000,NULL);
	solver->calculate();

	Menu menu(solver);

	// run
	glutMainLoop();
}

}; // namespace
