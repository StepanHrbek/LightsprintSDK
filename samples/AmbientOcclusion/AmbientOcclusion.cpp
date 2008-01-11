// --------------------------------------------------------------------------
// Ambient Occlusion sample
//
// This sample computes Ambient Occlusion maps 
// including infinite light bounces and color bleeding.
//
// It loads Collada 1.4 .DAE scene, calculates Ambient Occlusion maps,
// saves them to disk and runs interactive map viewer.
//
// Scene wiewer renders only computed ambient occlusion maps,
// no material colors, so all colors rendered are from color bleeding.
// You can enable diffuse maps by 'd'.
//
// The only lightsource is white skybox.
// You can quickly change sky or add other light sources
// (lights, emissive materials), calculation times won't change.
//
// Viewer controls:
//  mouse = look around
//  wheel = zoom
//  arrows = move around
//  space = toggle 2D/3D viewer
//  +- = brightness
//  */ = gamma
//  d = toggle diffuse maps
//
// Remarks:
// - map quality depends on unwrap quality,
//   make sure you have good unwrap in your scenes
//   (save it as second TEXCOORD in Collada document, see RRObjectCollada.cpp)
// - light leaks below wall due to bug in scene geometry,
//   scene behind wall is bright, wall is infinitely thin and there's no seam
//   in mapping between bright and dark part of floor.
//   seam in floor mapping or thick wall would fix it
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets per-pixel AO, others get per-vertex AO
//#define TB

#ifdef TB
#include "../../samples/ImportTB/RRObjectTB.h"
#else
#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "../ImportCollada/RRObjectCollada.h"
#endif

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/LightmapViewer.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

#ifdef TB
rr_gl::Camera              eye(-1.416,130.741,-3.646, 12.230,0,0.050,1.3,70.0,0.3,300.0);
#else
rr_gl::Camera              eye(0,1.741f,-2.646f, 12.230f,0,0.05f,1.3f,70,0.3f,300);
#endif
rr::RRDynamicSolver*    solver = NULL;
rr_gl::RendererOfScene* rendererOfScene = NULL;
float                   speedForward = 0;
float                   speedBack = 0;
float                   speedRight = 0;
float                   speedLeft = 0;
rr_gl::UberProgramSetup uberProgramSetup;
rr::RRVec4              brightness(1);
float                   gamma = 1;
#ifdef RR_DEVELOPMENT
bool                    shortLines = true;
unsigned                debugTexelNumber = -1;
#endif


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	eye.aspect = w/(float)h;
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView<130) eye.fieldOfView+=10;
	}
}

void passive(int x, int y)
{
	unsigned winWidth = glutGet(GLUT_WINDOW_WIDTH);
	unsigned winHeight = glutGet(GLUT_WINDOW_HEIGHT);
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		eye.angle -= 0.005f*x;
		eye.angleX -= 0.005f*y;
		CLAMP(eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	eye.update();
	eye.setupForRender();
	rendererOfScene->setParams(uberProgramSetup,NULL,NULL,false);
	rendererOfScene->useOriginalScene(0);
	rendererOfScene->setBrightnessGamma(&brightness,gamma);
	rendererOfScene->render();
#ifdef RR_DEVELOPMENT
	rendererOfScene->renderLines(shortLines,debugTexelNumber);
#endif

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
		case '+':
			brightness *= 1.2f;
			break;
		case '-':
			brightness /= 1.2f;
			break;
		case '*':
			gamma *= 1.2f;
			break;
		case '/':
			gamma /= 1.2f;
			break;
		
#ifdef RR_DEVELOPMENT
		case 'n': shortLines = !shortLines; break;	
		case ',': debugTexelNumber--;break;
		case '.': debugTexelNumber++;break;
		case 'm':
			{
				rr::RRRay* ray = rr::RRRay::create();
				rr::RRVec3 dir = rr::RRVec3(eye.dir[0],eye.dir[1],eye.dir[2]).normalized();
				ray->rayOrigin = rr::RRVec3(eye.pos[0],eye.pos[1],eye.pos[2]);
				ray->rayDirInv[0] = 1/dir[0];
				ray->rayDirInv[1] = 1/dir[1];
				ray->rayDirInv[2] = 1/dir[2];
				ray->rayLengthMin = eye.anear;
				ray->rayLengthMax = eye.afar;
				ray->rayFlags = rr::RRRay::FILL_POINT3D;
				if(solver->getMultiObjectCustom()->getCollider()->intersect(ray))
				{
					rr::RRDynamicSolver::UpdateParameters paramsDirect;
					paramsDirect.quality = 100;
					printf("%d %x\n",ray->hitTriangle,solver->getMultiObjectCustom()->getCollider()->getMesh()->getPreImportTriangle(ray->hitTriangle));
					solver->updateTriangleLightmap(0,ray->hitTriangle,&paramsDirect);
				}
			}
			break;
		case 'M':
			{
				rr::RRRay* ray = rr::RRRay::create();
				rr::RRVec3 dir = rr::RRVec3(eye.dir[0],eye.dir[1],eye.dir[2]).normalized();
				ray->rayOrigin = rr::RRVec3(eye.pos[0],eye.pos[1],eye.pos[2]);
				ray->rayDirInv[0] = 1/dir[0];
				ray->rayDirInv[1] = 1/dir[1];
				ray->rayDirInv[2] = 1/dir[2];
				ray->rayLengthMin = eye.anear;
				ray->rayLengthMax = eye.afar;
				ray->rayFlags = rr::RRRay::FILL_POINT3D;
				if(solver->getMultiObjectCustom()->getCollider()->intersect(ray))
				{
					rr::RRDynamicSolver::UpdateParameters paramsDirect;
					paramsDirect.quality = 1000;
					solver->updateTriangleLightmap(0,ray->hitTriangle,&paramsDirect);
				}
			}
			break;

		case 'N': uberProgramSetup.POSTPROCESS_NORMALS = !uberProgramSetup.POSTPROCESS_NORMALS; break;
#endif

		case 'd':
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = !uberProgramSetup.MATERIAL_DIFFUSE_MAP;
			break;

		case ' ':
			{
				static rr_gl::LightmapViewer* lv = NULL;
				if(lv)
				{
					glutDisplayFunc(display);
					glutMouseFunc(mouse);
					glutPassiveMotionFunc(passive);
					SAFE_DELETE(lv);
				}
				else
				{
					lv = rr_gl::LightmapViewer::create(
						solver->getIllumination(SELECTED_OBJECT_NUMBER)->getLayer(0),
						solver->getObject(SELECTED_OBJECT_NUMBER)->getCollider()->getMesh(),"../../data/shaders/");
					if(lv)
					{
						glutMouseFunc(lv->mouse);
						glutPassiveMotionFunc(lv->passive);
						glutDisplayFunc(lv->display);
					}
				}
			}
			break;
		case 27:
			exit(0);
	}
}

void idle()
{
	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		CLAMP(seconds,0.001f,0.3f);
		seconds *= 3;
		rr_gl::Camera* cam = &eye;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
	}
	prev = now;

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

void calculate(rr::RRDynamicSolver* solver, unsigned layerNumber)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		rr::RRMesh* mesh = solver->getObject(i)->getCollider()->getMesh();
		if(i==SELECTED_OBJECT_NUMBER)
		{
			// allocate lightmaps for selected object
			unsigned res = 16;
			unsigned sizeFactor = 5; // 5 is ok for scenes with unwrap (20 is ok for scenes without unwrap)
			while(res<2048 && (float)res<sizeFactor*sqrtf((float)(mesh->getNumTriangles()))) res*=2;
			solver->getIllumination(i)->getLayer(layerNumber) =
				rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
		}
		else
		{
			// allocate vertex buffers for other objects
			solver->getIllumination(i)->getLayer(layerNumber) =
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// calculate ambient occlusion
	rr::RRDynamicSolver::UpdateParameters params(1000);
	solver->updateLightmaps(layerNumber,-1,&params,&params,NULL); 
}

int main(int argc, char **argv)
{
	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800,600);
	glutCreateWindow("Lightsprint Ambient Occlusion");
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

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new rr::RRDynamicSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
#ifdef TB
	// these are our peice chunk AO textures names to match on
	std::vector< std::string > mapNames;
	//mapNames.push_back( "1_0" );
	mapNames.push_back( "1_1" );
	//mapNames.push_back( "1_2" );
	//mapNames.push_back( "1_3" );
	//mapNames.push_back( "1_4" );
	//mapNames.push_back( "1_5" );
	//mapNames.push_back( "1_6" );
	//mapNames.push_back( "1_7" );
	//mapNames.push_back( "1_8" );
	//mapNames.push_back( "1_9" ); //missing
	//mapNames.push_back( "1_10" ); //missing
	//mapNames.push_back( "1_11" );
	//mapNames.push_back( "1_12" );
	//mapNames.push_back( "1_13" );
	//mapNames.push_back( "1_14" );
	solver->setStaticObjects( *adaptObjectsFromTB( mapNames ), NULL );
#else
	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("../../data/scenes/koupelna/koupelna4-windows.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setStaticObjects( *adaptObjectsFromFCollada( collada ), NULL );
#endif
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);
	
	// set white environment
	solver->setEnvironment( rr::RRBuffer::createSky() );
	
	rendererOfScene = new rr_gl::RendererOfScene(solver,"../../data/shaders/");

	{
		rr::RRReportInterval report(rr::INF1,"Calculating global ambient occlusion ...\n");

		// decrease priority, so that this task runs on background using only free CPU cycles
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);

		// calculate and save it
		calculate(solver,0);
		solver->getStaticObjects().saveIllumination("../../data/export/",0);

		// or load it
		//solver->getObjects()->loadIllumination("../../data/export/",0);

		// restore priority
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
	}

	uberProgramSetup.LIGHT_INDIRECT_auto = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;

	// run glut based scene viewer/map viewer
	glutMainLoop();
	return 0;
}
