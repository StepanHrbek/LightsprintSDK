#ifndef CAMERA_H
#define CAMERA_H

#include <GL/glut.h>

class Camera
{
public:
  Camera(float posX, float posY, float posZ);
  ~Camera();

  void placeIt();
  GLdouble *getInvViewMatrix();
private:
  void computeInvViewMatrix();

  float pos[3];
  GLdouble invViewMatrix[16];
};

#endif //CAMERA_H
