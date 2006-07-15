#include "Camera.h"
#include "matrix.h"
#include <math.h>

void Camera::update(float back)
{
	dir[0] = 3*sin(angle);
	dir[1] = -0.3*height;
	dir[2] = 3*cos(angle);
	dir[3] = 1.0;
	buildLookAtMatrix(viewMatrix,
		pos[0]-back*dir[0],pos[1]-back*dir[1],pos[2]-back*dir[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		0, 1, 0);
	buildPerspectiveMatrix(frustumMatrix, fieldOfView, aspect, anear, afar);
	invertMatrix(inverseViewMatrix, viewMatrix);
	invertMatrix(inverseFrustumMatrix, frustumMatrix);
}
void Camera::setupForRender()
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(frustumMatrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(viewMatrix);
}
