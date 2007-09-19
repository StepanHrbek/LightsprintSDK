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
// The only lightsource considered is skybox loaded from
// texture(s), and it is fully white by default.
// You can quickly change sky maps or add other light sources
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
//  left button = toggle bilinear interpolation
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
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
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
#include "Lightsprint/GL/RRDynamicSolverGL.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 6xxx\n - GeForce 7xxx\n - GeForce 8xxx\n - Radeon 9500-9800\n - Radeon Xxxx\n - Radeon X1xxx");
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
rr_gl::Camera              eye(0,1.741,-2.646, 12.230,0,0.050,1.3,70.0,0.3,300.0);
#endif
rr_gl::RRDynamicSolverGL* solver = NULL;
rr_gl::RendererOfScene* rendererOfScene = NULL;
float                   speedForward = 0;
float                   speedBack = 0;
float                   speedRight = 0;
float                   speedLeft = 0;
rr_gl::UberProgramSetup uberProgramSetup;
float                   brightness[4] = {1,1,1,1};
float                   gamma = 1;
#ifdef RR_DEVELOPMENT
bool                    shortLines = true;
unsigned                debugTexelNumber = -1;
#endif


/////////////////////////////////////////////////////////////////////////////
//
// only map creation is implemented here

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("../../data/shaders/") {}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
#ifdef TB
		RRObjectTB * objectTB = reinterpret_cast<RRObjectTB*>(object);
		return createIlluminationPixelBuffer( objectTB->GetMapSize(), objectTB->GetMapSize() );
#else
		// Decide how big ambient occlusion map you want for object.
		// In this sample, we pick res proportional to number of triangles in object.
		// Optimal res depends on quality of unwrap provided by object->getTriangleMapping.
		unsigned res = 16;
		unsigned sizeFactor = 5; // higher factor = higher map resolution
		while(res<2048 && res<sizeFactor*sqrtf(object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		return createIlluminationPixelBuffer(res,res);
#endif
	}
	virtual void detectMaterials() {}
	virtual bool detectDirectIllumination() {return false;}
	virtual void setupShader(unsigned objectNumber) {}
};


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
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		static bool nearest = false;
		nearest = !nearest;
		for(unsigned i=0;i<solver->getNumObjects();i++)
		{	
			if(solver->getIllumination(i)->getLayer(0)->pixelBuffer)
			{
				glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_2D_LIGHT_INDIRECT);
				solver->getIllumination(i)->getLayer(0)->pixelBuffer->bindTexture();
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearest?GL_NEAREST:GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest?GL_NEAREST:GL_LINEAR);
			}
		}
	}
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
		eye.angle -= 0.005*x;
		eye.angleX -= 0.005*y;
		CLAMP(eye.angleX,-M_PI*0.49f,M_PI*0.49f);
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	eye.update(0);
	eye.setupForRender();
	rendererOfScene->setParams(uberProgramSetup,NULL,NULL);
	rendererOfScene->useOriginalScene(0);
	rendererOfScene->setBrightnessGamma(brightness,gamma);
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
			for(unsigned i=0;i<4;i++) brightness[i] *= 1.2;
			break;
		case '-':
			for(unsigned i=0;i<4;i++) brightness[i] /= 1.2;
			break;
		case '*':
			gamma *= 1.2;
			break;
		case '/':
			gamma /= 1.2;
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
						solver->getIllumination(SELECTED_OBJECT_NUMBER)->getLayer(0)->pixelBuffer,
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
		seconds *= 10;
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

void calculatePerVertexAndSelectedPerPixel(rr_gl::RRDynamicSolverGL* solver, unsigned layerNumber)
{
	// calculate per vertex - all objects
	// it is faster and quality is good for some objects
	rr::RRDynamicSolver::UpdateParameters paramsBoth;
	paramsBoth.measure = RM_IRRADIANCE_PHYSICAL; // get vertex colors in HDR
	paramsBoth.quality = 5000;
	paramsBoth.applyCurrentSolution = false;
	paramsBoth.applyEnvironment = true;
	solver->updateVertexBuffers(0,-1,true,&paramsBoth,&paramsBoth); 

	// calculate per pixel - selected objects
	// it is slower, but some objects need it
	rr::RRDynamicSolver::UpdateParameters paramsDirectPixel;
	paramsDirectPixel.measure = RM_IRRADIANCE_CUSTOM; // get maps in sRGB
	paramsDirectPixel.quality = 2000;
	paramsDirectPixel.applyEnvironment = true;
	unsigned objectNumbers[] = {SELECTED_OBJECT_NUMBER};
	for(unsigned i=0;i<sizeof(objectNumbers)/sizeof(objectNumbers[0]);i++)
	{
		unsigned objectNumber = objectNumbers[i];
		if(solver->getObject(objectNumber))
		{
			solver->getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer = solver->createIlluminationPixelBuffer(256,256);
			solver->updateLightmap(objectNumber,solver->getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer,NULL,&paramsDirectPixel);
		}
	}
}

void calculatePerPixel(rr_gl::RRDynamicSolverGL* solver, unsigned layerNumber)
{
	// calculate per pixel - all objects
	rr::RRDynamicSolver::UpdateParameters paramsDirect;
	paramsDirect.measure.scaled = false;
	paramsDirect.quality = 1000;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = true;
	rr::RRDynamicSolver::UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = true;
	solver->updateLightmaps(layerNumber,-1,true,&paramsDirect,&paramsIndirect); 
}

void saveAmbientOcclusionToDisk(rr_gl::RRDynamicSolverGL* solver, unsigned layerNumber)
{
	for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
	{
		char filename[1000];

		// save vertex buffer
		rr::RRIlluminationVertexBuffer* vbuf = solver->getIllumination(objectIndex)->getLayer(layerNumber)->vertexBuffer;
		if(vbuf)
		{
#ifdef TB
			RRObjectTB * object = reinterpret_cast<RRObjectTB*>(solver->getObject(objectIndex));
			sprintf(filename,"../../data/export/%s.vbu",object->GetMapName() );
#else
			sprintf(filename,"../../data/export/%d.vbu",objectIndex );
#endif
			bool saved = vbuf->save(filename);
			rr::RRReporter::report(rr::INF1,saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
		}

		// save pixel buffer
		rr::RRIlluminationPixelBuffer* map = solver->getIllumination(objectIndex)->getLayer(layerNumber)->pixelBuffer;
		if(map)
		{
#ifdef TB
			RRObjectTB * object = reinterpret_cast<RRObjectTB*>(solver->getObject(objectIndex));
			sprintf(filename,"../../data/export/%s.tga",object->GetMapName() );
#else
			sprintf(filename,"../../data/export/%d.png",objectIndex );
#endif
			bool saved = map->save(filename);
			rr::RRReporter::report(rr::INF1,saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
		}
	}
}

void loadAmbientOcclusionFromDisk(rr_gl::RRDynamicSolverGL* solver, unsigned layerNumber)
{
	for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
	{
		char filename[1000];

		// load vertex buffer
#ifdef TB
		RRObjectTB * objectTB = reinterpret_cast<RRObjectTB*>(solver->getObject(objectIndex));
		sprintf(filename,"../../data/export/%s.vbu",objectTB->GetMapName() );
#else
		sprintf(filename,"../../data/export/%d.vbu",objectIndex );
#endif
		solver->getIllumination(objectIndex)->getLayer(layerNumber)->vertexBuffer
			= rr::RRIlluminationVertexBuffer::load(filename,solver->getObject(objectIndex)->getCollider()->getMesh()->getNumVertices());

		// load pixel buffer
#ifdef TB
		sprintf(filename,"../../data/export/%s.tga",objectTB->GetMapName() );
#else
		sprintf(filename,"../../data/export/%d.png",objectIndex );
#endif
		solver->getIllumination(objectIndex)->getLayer(layerNumber)->pixelBuffer
			= solver->loadIlluminationPixelBuffer(filename);
	}
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

	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	rr_gl::Texture* environmentMap = rr_gl::Texture::load("..\\..\\data\\maps\\whitebox\\whitebox_%s.png",cubeSideNames,true,true,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
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
	solver->setObjects( *adaptObjectsFromTB( mapNames ), NULL );
#else
	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("../../data/scenes/koupelna/koupelna4-windows.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setObjects( *adaptObjectsFromFCollada( collada ), NULL );
#endif
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);
	solver->setEnvironment( solver->adaptIlluminationEnvironmentMap( environmentMap ) );
	rendererOfScene = new rr_gl::RendererOfScene(solver,"../../data/shaders/");

	{
		rr::RRReportInterval report(rr::INF1,"Calculating global ambient occlusion ...\n");

		// calculate and save it
		calculatePerVertexAndSelectedPerPixel(solver,0); // calculatePerPixel(solver,0);
		saveAmbientOcclusionToDisk(solver,0);

		// or load it
		//loadAmbientOcclusionFromDisk(solver,0);
	}

	uberProgramSetup.LIGHT_INDIRECT_auto = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;

	// run glut based scene viewer/map viewer
	glutMainLoop();
	return 0;
}
