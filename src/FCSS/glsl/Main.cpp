#include <iostream>
#include <GL/glut.h>
#include "Scene.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "GLSLProgram.hpp"
#include "Texture.hpp"
#include "FrameRate.hpp"

using namespace std;

#define SHADOW_MAP_SIZE 512
#define myMin(a, b) ((a < b) ? a : b)

GLSLProgram *shadowProg, *lightProg;

unsigned int shadowTex;
int currentWindowSize;

Scene *scene;
Light *light;
Camera *cam;
Texture *lightTex;
FrameRate *counter;


void initShadowTex()
{
  glGenTextures(1, &shadowTex);
  
  glBindTexture(GL_TEXTURE_2D, shadowTex);
  glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE,
		SHADOW_MAP_SIZE,0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void updateShadowTex()
{
  glBindTexture(GL_TEXTURE_2D, shadowTex);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		      SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
}

void initShaders()
{
  shadowProg = new GLSLProgram("shadow.vp", "shadow.fp");
  lightProg = new GLSLProgram("light.vp");
}

void init()
{
  glClearColor(0.0, 0.0, 0.0, 0.0);
  
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glShadeModel(GL_SMOOTH);
    
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  
  initShadowTex();
  initShaders();
}

void computeProjectiveMatrix()
{
  glMatrixMode(GL_TEXTURE);
  glLoadMatrixf(light->getProjMatrix());
  glMultMatrixf(light->getViewMatrix());
  glMultMatrixd(cam->getInvViewMatrix());
  
  glMatrixMode(GL_MODELVIEW);
}

void activateTexture(unsigned int textureUnit, unsigned int textureType)
{
	glActiveTextureARB(textureUnit);
	glEnable(textureType);
}

void display()
{
  glClear(GL_DEPTH_BUFFER_BIT);
  
  lightProg->useIt();
  
  glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
  light->activateRendering();
    scene->draw();
  light->deactivateRendering();
  glViewport(0, 0, currentWindowSize, currentWindowSize);
  
  updateShadowTex();
  glClear(GL_DEPTH_BUFFER_BIT);
  
  shadowProg->useIt();
  
  activateTexture(GL_TEXTURE1_ARB, GL_TEXTURE_2D);
  lightTex->bindTexture();
  shadowProg->sendUniform("lightTex", 1);
  
  activateTexture(GL_TEXTURE0_ARB, GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, shadowTex);
  shadowProg->sendUniform("shadowMap", 0);
  
  computeProjectiveMatrix();
  scene->draw();

  glFlush();
  glutSwapBuffers();
}

void reshape(int w, int v)
{
  currentWindowSize = myMin(w, v);
  glViewport(0, 0, currentWindowSize, currentWindowSize);

  glClear(GL_COLOR_BUFFER_BIT);
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, 1.0, 1.0, 11.0);
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  cam->placeIt();
}

void cleanUp()
{
  delete scene;
  delete light;
  delete cam;
  delete counter;
  delete lightTex;
  delete shadowProg;
  delete lightProg;
}

void keyb(unsigned char c, int x, int y)
{
  if(c == '\x1B')
    {
      cleanUp();
      exit(0);
    }
}

void idle()
{
  light->update();
  
  glutPostRedisplay();
  
  counter->displayFrameRate();
}

int main(int argc, char **argv)
{
  char *defaultLightTex = "spot0.tga";
  char *windowName = "Shadow Mapping -- by (:Greg:)";

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(512, 512);
  glutCreateWindow(windowName);

  wglSwapIntervalEXT(0);
  
  init();
  
  scene = new Scene;
  light = new Light;
  cam = new Camera(2.0, 3.0, 3.0);
  counter = new FrameRate(windowName);
  lightTex = new Texture(((argc > 1) ? argv[1] : defaultLightTex), GL_LINEAR,
			 GL_LINEAR, GL_CLAMP, GL_CLAMP);
  
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyb);
  glutIdleFunc(idle);
  glutMainLoop();
  
  return 0;
}
