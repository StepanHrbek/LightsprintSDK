#ifndef MATRIX_H
#define MATRIX_H

#include <GL/glew.h>

namespace rr_gl
{

extern GLboolean invertMatrix(GLdouble *out, const GLdouble *m);

extern void buildLookAtMatrix(GLdouble m[16],
                              GLdouble eyex, GLdouble eyey, GLdouble eyez,
                              GLdouble centerx, GLdouble centery, GLdouble centerz,
                              GLdouble upx, GLdouble upy, GLdouble upz);
}; // namespace

#endif
