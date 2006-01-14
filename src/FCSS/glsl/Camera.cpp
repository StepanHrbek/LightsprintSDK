#include "Camera.hpp"
#include <GL/glut.h>
//#include <GL/gl.h>
//#include <GL/glu.h>
#include "matrix.h"

// Public part :

Camera::Camera(float posX, float posY, float posZ)
{
  pos[0] = posX;
  pos[1] = posY;
  pos[2] = posZ;

  computeInvViewMatrix();
}

Camera::~Camera()
{
}

void Camera::placeIt()
{
  gluLookAt(pos[0], pos[1], pos[2],  0.0, 0.0, 0.0,  0.0, 1.0, 0.0);
}

GLdouble *Camera::getInvViewMatrix()
{
  return invViewMatrix;
}

// Private part :

void Camera::computeInvViewMatrix()
{
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  placeIt();
  GLdouble viewMatrix[16];
  glGetDoublev(GL_TEXTURE_MATRIX, viewMatrix);
  glMatrixMode(GL_MODELVIEW);

  invertMatrix(invViewMatrix, viewMatrix);
}
