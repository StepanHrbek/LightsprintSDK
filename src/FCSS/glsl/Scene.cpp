#include "Scene.hpp"
#include <GL/glut.h>
//#include <GL/gl.h>
#include "Torus.hpp"

// Public part :

Scene::Scene()
{
  t = new Torus(0.2, 0.7);
  //compileScene();
  //delete t;
}

Scene::~Scene()
{
  delete t;
  //glDeleteLists(scene, 1);
}

void Scene::draw()
{
  //glCallList(scene);
  glPushMatrix();
    
    t->draw();
    
    glBegin(GL_QUADS);
      glNormal3f(0.0, 1.0, 0.0);
      glVertex3f(floorSize, -1.0, -floorSize);
      glVertex3f(-floorSize, -1.0, -floorSize);
      glVertex3f(-floorSize, -1.0, floorSize);
      glVertex3f(floorSize, -1.0, floorSize);
    glEnd();
      
  glPopMatrix();
}

// Private part :

const float Scene::floorSize = 5.0;

void Scene::compileScene()
{
  scene = glGenLists(1);
  glNewList(scene, GL_COMPILE);
    glPushMatrix();
    
      t->draw();
      
      glBegin(GL_QUADS);
        glNormal3f(0.0, 1.0, 0.0);
	glVertex3f(floorSize, -1.0, -floorSize);
	glVertex3f(-floorSize, -1.0, -floorSize);
	glVertex3f(-floorSize, -1.0, floorSize);
	glVertex3f(floorSize, -1.0, floorSize);
      glEnd();
      
    glPopMatrix();
  glEndList();
}
