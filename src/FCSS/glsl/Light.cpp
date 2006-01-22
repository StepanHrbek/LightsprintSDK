#include "Light.hpp"
#include <math.h>
#include <GL/glut.h>

// Public part :

Light::Light()
  :lastUpdate(glutGet(GLUT_ELAPSED_TIME)), speed(defaultSpeed), angle(0.0),
   radius(defaultRadius), height(defaultHeight)
{
  pos[3] = 1.0;
  initProjMatrix();
  update();
}

Light::~Light()
{
}

void Light::update()
{
  float currentTime = glutGet(GLUT_ELAPSED_TIME);
  
  angle += (currentTime - lastUpdate) * speed;
  if(angle > 360.0)
    angle -= 360.0;
  
  pos[0] = radius * cos(angle);
  pos[1] = height;
  pos[2] = radius * sin(angle);
  
  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  lastUpdate = currentTime;
  
  updateViewMatrix();
}

void Light::activateRendering()
{
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  
  glCullFace(GL_FRONT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
    glLoadMatrixf(projMatrix);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
      glLoadMatrixf(viewMatrix);
}

void Light::deactivateRendering()
{
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glCullFace(GL_BACK);
}

float *Light::getProjMatrix()
{
  return projMatrix;
}

float *Light::getViewMatrix()
{
  return viewMatrix;
}

// Private part :

const float Light::defaultSpeed = 0.001;
const float Light::defaultRadius = 2.0;
const float Light::defaultHeight = 2.0;

void Light::initProjMatrix()
{
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
    glLoadIdentity();  
    gluPerspective(60.0, 1.0, 1.0, 10.0);
    glGetFloatv(GL_MODELVIEW_MATRIX, projMatrix);
  glPopMatrix();
}

void Light::updateViewMatrix()
{
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
    glLoadIdentity();
    gluLookAt(pos[0], pos[1], pos[2],  0.0, 0.0, 0.0,  0.0, 1.0, 0.0);
    glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
  glPopMatrix();
}
