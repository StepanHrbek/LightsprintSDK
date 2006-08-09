
/* matrix.h - matrix routines for GeForce/Quadro shadow mapping demo */

/* Copyright NVIDIA Corporation, 2000. */

/* $Id: //sw/main/apps/OpenGL/mjk/shadowcast/matrix.h#2 $ */

#ifndef __MATRIX_H__
#define __MATRIX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <GL/glew.h>

extern GLboolean invertMatrix(GLdouble *out, const GLdouble *m);
extern void transposeMatrix(GLdouble dst[16], GLdouble src[16]);
extern void copyMatrix(GLdouble dst[16], GLdouble src[16]);
extern void addMatrices(GLdouble dst[16], GLdouble a[16], GLdouble b[16]);
extern void multMatrices(GLdouble dst[16], const GLdouble a[16], const GLdouble b[16]);
extern void buildFrustumMatrix(GLdouble m[16],
                               GLdouble l, GLdouble r, GLdouble b, GLdouble t,
                               GLdouble n, GLdouble f);
extern void buildPerspectiveMatrix(GLdouble m[16],
                                   GLdouble fovy, GLdouble aspect,
                                   GLdouble zNear, GLdouble zFar);
extern void buildLookAtMatrix(GLdouble m[16],
                              GLdouble eyex, GLdouble eyey, GLdouble eyez,
                              GLdouble centerx, GLdouble centery, GLdouble centerz,
                              GLdouble upx, GLdouble upy, GLdouble upz);
extern void printMatrix(char *msg, GLdouble m[16]);

#ifdef __cplusplus
}
#endif

#endif  /* __MATRIX_H__ */
