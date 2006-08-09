#ifndef CAMERA_H
#define CAMERA_H

#include "DemoEngine.h"
#include <GL/glew.h>


/////////////////////////////////////////////////////////////////////////////
//
// Camera

class RR_API Camera
{
public:
	// inputs
	GLfloat  pos[3];
	float    angle;
	float    height;
	GLdouble aspect;
	GLdouble fieldOfView;
	GLdouble anear;
	GLdouble afar;

	// outputs
	GLfloat  dir[4];
	GLdouble viewMatrix[16];
	GLdouble inverseViewMatrix[16];
	GLdouble frustumMatrix[16];
	GLdouble inverseFrustumMatrix[16];

	// tools
	void update(float back); // converts inputs to outputs
	void setupForRender();
};

#endif //CAMERA_H
