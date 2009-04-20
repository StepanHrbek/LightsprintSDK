#include "Lightsprint/GL/DynamicMeshDemo.h"
#include "Lightsprint/IO/ImportScene.h"
#include <cstdio>
#include <GL/glew.h>
#include <GL/glut.h>


/////////////////////////////////////////////////////////////////////////////
//
// globals, required by GLUT design with callbacks

rr_gl::Scene*    scene1 = NULL;
rr_gl::Scene*    scene2 = NULL;
rr::RRBuffer*    skybox1 = NULL;
rr::RRBuffer*    skybox2 = NULL;
rr_gl::Camera    camera(-1.416f,1.741f,-3.646f, 7.0f,0,0.05f,1.3f,110,0.1f,100);


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void keyboard(unsigned char c, int x, int y)
{
	exit(0);
}

void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	camera.setAspect( w/(float)h );
}

void display(void)
{
	glClear(GL_DEPTH_BUFFER_BIT);

	static float f = 0;
	f += 0.005f;

	// Pick random scene to see that swapping works.
	rr_gl::Scene* scene = (int(f)%8)<4?scene1:scene2;

	// Pick random skybox to see that swapping works.
	if (rand()%50==0) scene->setEnvironment((rand()%2)?skybox1:skybox2);

	// Move camera to see that it works.
	camera.pos.z += 0.001f;

	// Move lights to see that it works.
	scene->realtimeLights[0]->getRRLight().position.z = -2*cos(f);
	scene->realtimeLights[0]->getRRLight().direction.x = sin(f);
	scene->realtimeLights[0]->getRRLight().direction.y = -0.4f;
	scene->realtimeLights[0]->getRRLight().direction.z = cos(f);
	scene->realtimeLights[0]->getRRLight().direction.normalize();
	scene->realtimeLights[0]->updateAfterRRLightChanges();

	// Generate dynamic meshes to see that it works.
	// - make up some geometry
	const unsigned numIndices = 3+6;
	unsigned short indices[] = {0,1,2, 3,4,5,3,5,6};
	const unsigned numVertices = 3+4;
	static rr_gl::DynamicVertex vertices[] = {
		-1,1,-0, 0,0,0,
		-2,1,-0, 0,0,0,
		-1,2,-0, 0,0,0,
		-1,1,-0, 0,0,0,
		-1,1, 1, 0,0,0,
		 0,1, 1, 0,0,0,
		 0,1,-0, 0,0,0};
	for (unsigned i=0;i<numVertices;i++)
	{
		vertices[i].position[0] += ((rand()%10000)-4800)/300000.0f;
		vertices[i].position[1] += ((rand()%10000)-4900)/500000.0f;
		vertices[i].position[2] += ((rand()%10000)-4800)/300000.0f;
	}
	// - generate normals
	for (unsigned i=0;i<numIndices;i+=3)
	{
		rr::RRVec3& p0 = *(rr::RRVec3*)vertices[indices[i]].position;
		rr::RRVec3& p1 = *(rr::RRVec3*)vertices[indices[i+1]].position;
		rr::RRVec3& p2 = *(rr::RRVec3*)vertices[indices[i+2]].position;
		rr::RRVec3& n0 = *(rr::RRVec3*)vertices[indices[i]].normal;
		rr::RRVec3& n1 = *(rr::RRVec3*)vertices[indices[i+1]].normal;
		rr::RRVec3& n2 = *(rr::RRVec3*)vertices[indices[i+2]].normal;
		n0 = n1 = n2 = (p1-p0).cross(p2-p0).normalized();
	}
	// - make up some materials
	rr::RRMaterial material1;
	material1.reset(false);
	material1.diffuseReflectance.color = rr::RRVec3(1);
	material1.specularReflectance.color = rr::RRVec3(0);
	rr::RRMaterial material2;
	material2.reset(false);
	material2.diffuseReflectance.color = rr::RRVec3(0);
	material2.specularReflectance.color = rr::RRVec3(1);
	// - fill structures
	const unsigned numMeshes = 2;
	rr_gl::DynamicMesh meshes[numMeshes];
	meshes[0].vertices = vertices;
	meshes[0].numVertices = 3;
	meshes[0].indices = indices;
	meshes[0].numIndices = 3;
	meshes[0].material = &material1;
	meshes[1].vertices = vertices;
	meshes[1].numVertices = 4;
	meshes[1].indices = indices+3;
	meshes[1].numIndices = 6;
	meshes[1].material = &material2;

	// Calculate GI and render.
	scene->render(camera,numMeshes,meshes);

	// Render 2d overlays with naked girls...

	// Flush contents of backbuffer to screen.
	glutSwapBuffers();
}

void idle()
{
	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// Init GLUT.
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("RdxDemo"); // for windowed mode
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);

	// Init GLEW.
	if (glewInit()!=GLEW_OK)
	{
		printf("We can't continue, GLEW init failed.\n");
		exit(0);
	}

	// Init GL.
	int major, minor;
	if (sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
	{
		printf("We can't continue, graphics drivers don't support OpenGL 2.0.\n");
		exit(0);
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	// Initialize demo library. One instance must exist.
	rr_gl::Demo demo("../../data/shaders/");
	rr_io::registerLoaders();

	// Load scenes from disk.
	// If you change quality, delete .fireball file otherwise old quality will still be used.
	scene1 = new rr_gl::Scene("../../data/scenes/koupelna/koupelna4-windows.dae",5000,2);
	scene2 = new rr_gl::Scene("../../data/scenes/koule.dae",5000,2);

	// Load skyboxes from disk.
	skybox1 = rr::RRBuffer::loadCube("../../data/maps/panorama.jpg");
	skybox2 = rr::RRBuffer::loadCube("../../data/maps/skybox_gradient.jpg");

	// Set initial skybox (can be changed later).
	scene1->setEnvironment(skybox1);
	scene2->setEnvironment(skybox2);

	// Modify initial lights (can be changed later).
	//  - Initially lights loaded from scene file are present.
	//  - You can insert/remove lights now or later by calling setLights() with new set.
	//    Note that it's too slow to do in every frame.
	//  - You can modify existing lights in scene by directly accessing their public properties
	//    via scene->getLights() and scene->realtimeLights.
	//    Call scene->realtimeLights[]->updateAfterXxx() afterwards to let system know of your changes.
	rr::RRLights lights; // temporary container used only to pass multiple lights at once
	lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(-0.802f,1.7f,0.850f),rr::RRVec3(1),rr::RRVec3(1,-0.2f,1),RR_DEG2RAD(40),0.1f));
	lights[0]->rtProjectedTextureFilename = _strdup("../../data/maps/spot0.png");
	lights[0]->color *= 5;
	scene1->setLights(lights);
	scene2->setLights(lights);

	// Runs interactive scene viewer/debugger.
	// Bug: it may be called only here, before first render() call. It crashes if called later.
	//scene1->debugger();

	// To make scene-swapping smooth, invisibly pre-render one frame from each scene.
	// Scene's first frame is always slow because data are sent to GPU.
	scene1->render(camera,0,NULL);
	scene2->render(camera,0,NULL);

	// Run glut mainloop. It never returns.
	glutMainLoop();
	return 0;
}
