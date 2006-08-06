
/* matrix.c - matrix routines for GeForce/Quadro shadow mapping demo */

/* Copyright NVIDIA Corporation, 2000. */

/* $Id: //sw/main/apps/OpenGL/mjk/shadowcast/matrix.c#2 $ */

#include <math.h>
#include <stdio.h>
#include <GL/glut.h>

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "DemoEngine/matrix.h"

/*** MATRIX MANIPULATION ***/

/*
 * Compute inverse of 4x4 transformation matrix.
 * Code contributed by Jacques Leroy <jle@star.be>
 * Code lifted from Brian Paul's Mesa freeware OpenGL implementation.
 * Return GL_TRUE for success, GL_FALSE for failure (singular matrix)
 */
GLboolean
invertMatrix(GLdouble *out, const GLdouble *m)
{
/* NB. OpenGL Matrices are COLUMN major. */
#define SWAP_ROWS(a, b) { GLdouble *_tmp = a; (a)=(b); (b)=_tmp; }
#define MAT(m,r,c) (m)[(c)*4+(r)]

 GLdouble wtmp[4][8];
 GLdouble m0, m1, m2, m3, s;
 GLdouble *r0, *r1, *r2, *r3;

 r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];

 r0[0] = MAT(m,0,0), r0[1] = MAT(m,0,1),
 r0[2] = MAT(m,0,2), r0[3] = MAT(m,0,3),
 r0[4] = 1.0, r0[5] = r0[6] = r0[7] = 0.0,

 r1[0] = MAT(m,1,0), r1[1] = MAT(m,1,1),
 r1[2] = MAT(m,1,2), r1[3] = MAT(m,1,3),
 r1[5] = 1.0, r1[4] = r1[6] = r1[7] = 0.0,

 r2[0] = MAT(m,2,0), r2[1] = MAT(m,2,1),
 r2[2] = MAT(m,2,2), r2[3] = MAT(m,2,3),
 r2[6] = 1.0, r2[4] = r2[5] = r2[7] = 0.0,

 r3[0] = MAT(m,3,0), r3[1] = MAT(m,3,1),
 r3[2] = MAT(m,3,2), r3[3] = MAT(m,3,3),
 r3[7] = 1.0, r3[4] = r3[5] = r3[6] = 0.0;

 /* choose pivot - or die */
 if (fabs(r3[0])>fabs(r2[0])) SWAP_ROWS(r3, r2);
 if (fabs(r2[0])>fabs(r1[0])) SWAP_ROWS(r2, r1);
 if (fabs(r1[0])>fabs(r0[0])) SWAP_ROWS(r1, r0);
 if (0.0 == r0[0]) {
  return GL_FALSE;
 }

 /* eliminate first variable     */
 m1 = r1[0]/r0[0]; m2 = r2[0]/r0[0]; m3 = r3[0]/r0[0];
 s = r0[1]; r1[1] -= m1 * s; r2[1] -= m2 * s; r3[1] -= m3 * s;
 s = r0[2]; r1[2] -= m1 * s; r2[2] -= m2 * s; r3[2] -= m3 * s;
 s = r0[3]; r1[3] -= m1 * s; r2[3] -= m2 * s; r3[3] -= m3 * s;
 s = r0[4];
 if (s != 0.0) { r1[4] -= m1 * s; r2[4] -= m2 * s; r3[4] -= m3 * s; }
 s = r0[5];
 if (s != 0.0) { r1[5] -= m1 * s; r2[5] -= m2 * s; r3[5] -= m3 * s; }
 s = r0[6];
 if (s != 0.0) { r1[6] -= m1 * s; r2[6] -= m2 * s; r3[6] -= m3 * s; }
 s = r0[7];
 if (s != 0.0) { r1[7] -= m1 * s; r2[7] -= m2 * s; r3[7] -= m3 * s; }

 /* choose pivot - or die */
 if (fabs(r3[1])>fabs(r2[1])) SWAP_ROWS(r3, r2);
 if (fabs(r2[1])>fabs(r1[1])) SWAP_ROWS(r2, r1);
 if (0.0 == r1[1]) {
  return GL_FALSE;
 }

 /* eliminate second variable */
 m2 = r2[1]/r1[1]; m3 = r3[1]/r1[1];
 r2[2] -= m2 * r1[2]; r3[2] -= m3 * r1[2];
 r2[3] -= m2 * r1[3]; r3[3] -= m3 * r1[3];
 s = r1[4]; if (0.0 != s) { r2[4] -= m2 * s; r3[4] -= m3 * s; }
 s = r1[5]; if (0.0 != s) { r2[5] -= m2 * s; r3[5] -= m3 * s; }
 s = r1[6]; if (0.0 != s) { r2[6] -= m2 * s; r3[6] -= m3 * s; }
 s = r1[7]; if (0.0 != s) { r2[7] -= m2 * s; r3[7] -= m3 * s; }

 /* choose pivot - or die */
 if (fabs(r3[2])>fabs(r2[2])) SWAP_ROWS(r3, r2);
 if (0.0 == r2[2]) {
  return GL_FALSE;
 }

 /* eliminate third variable */
 m3 = r3[2]/r2[2];
 r3[3] -= m3 * r2[3], r3[4] -= m3 * r2[4],
 r3[5] -= m3 * r2[5], r3[6] -= m3 * r2[6],
 r3[7] -= m3 * r2[7];

 /* last check */
 if (0.0 == r3[3]) {
  return GL_FALSE;
 }

 s = 1.0/r3[3];              /* now back substitute row 3 */
 r3[4] *= s; r3[5] *= s; r3[6] *= s; r3[7] *= s;

 m2 = r2[3];                 /* now back substitute row 2 */
 s  = 1.0/r2[2];
 r2[4] = s * (r2[4] - r3[4] * m2), r2[5] = s * (r2[5] - r3[5] * m2),
 r2[6] = s * (r2[6] - r3[6] * m2), r2[7] = s * (r2[7] - r3[7] * m2);
 m1 = r1[3];
 r1[4] -= r3[4] * m1, r1[5] -= r3[5] * m1,
 r1[6] -= r3[6] * m1, r1[7] -= r3[7] * m1;
 m0 = r0[3];
 r0[4] -= r3[4] * m0, r0[5] -= r3[5] * m0,
 r0[6] -= r3[6] * m0, r0[7] -= r3[7] * m0;

 m1 = r1[2];                 /* now back substitute row 1 */
 s  = 1.0/r1[1];
 r1[4] = s * (r1[4] - r2[4] * m1), r1[5] = s * (r1[5] - r2[5] * m1),
 r1[6] = s * (r1[6] - r2[6] * m1), r1[7] = s * (r1[7] - r2[7] * m1);
 m0 = r0[2];
 r0[4] -= r2[4] * m0, r0[5] -= r2[5] * m0,
 r0[6] -= r2[6] * m0, r0[7] -= r2[7] * m0;

 m0 = r0[1];                 /* now back substitute row 0 */
 s  = 1.0/r0[0];
 r0[4] = s * (r0[4] - r1[4] * m0), r0[5] = s * (r0[5] - r1[5] * m0),
 r0[6] = s * (r0[6] - r1[6] * m0), r0[7] = s * (r0[7] - r1[7] * m0);

 MAT(out,0,0) = r0[4]; MAT(out,0,1) = r0[5],
 MAT(out,0,2) = r0[6]; MAT(out,0,3) = r0[7],
 MAT(out,1,0) = r1[4]; MAT(out,1,1) = r1[5],
 MAT(out,1,2) = r1[6]; MAT(out,1,3) = r1[7],
 MAT(out,2,0) = r2[4]; MAT(out,2,1) = r2[5],
 MAT(out,2,2) = r2[6]; MAT(out,2,3) = r2[7],
 MAT(out,3,0) = r3[4]; MAT(out,3,1) = r3[5],
 MAT(out,3,2) = r3[6]; MAT(out,3,3) = r3[7]; 

 return GL_TRUE;

#undef MAT
#undef SWAP_ROWS
}

/* dst = transpose(src) */
void
transposeMatrix(GLdouble dst[16], GLdouble src[16])
{
  dst[0] = src[0];
  dst[1] = src[4];
  dst[2] = src[8];
  dst[3] = src[12];

  dst[4] = src[1];
  dst[5] = src[5];
  dst[6] = src[9];
  dst[7] = src[13];

  dst[8] = src[2];
  dst[9] = src[6];
  dst[10] = src[10];
  dst[11] = src[14];

  dst[12] = src[3];
  dst[13] = src[7];
  dst[14] = src[11];
  dst[15] = src[15];
}

/* dst = a + b */
void
addMatrices(GLdouble dst[16], GLdouble a[16], GLdouble b[16])
{
  dst[0] = a[0] + b[0];
  dst[1] = a[1] + b[1];
  dst[2] = a[2] + b[2];
  dst[3] = a[3] + b[3];

  dst[4] = a[4] + b[4];
  dst[5] = a[5] + b[5];
  dst[6] = a[6] + b[6];
  dst[7] = a[7] + b[7];

  dst[8] = a[8] + b[8];
  dst[9] = a[9] + b[9];
  dst[10] = a[10] + b[10];
  dst[11] = a[11] + b[11];

  dst[12] = a[12] + b[12];
  dst[13] = a[13] + b[13];
  dst[14] = a[14] + b[14];
  dst[15] = a[15] + b[15];
}

/* dst = a * b */
void
multMatrices(GLdouble dst[16], const GLdouble a[16], const GLdouble b[16])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      dst[i * 4 + j] =
        b[i * 4 + 0] * a[0 * 4 + j] +
        b[i * 4 + 1] * a[1 * 4 + j] +
        b[i * 4 + 2] * a[2 * 4 + j] +
        b[i * 4 + 3] * a[3 * 4 + j];
    }
  }
}

void
copyMatrix(GLdouble dst[16], GLdouble src[16])
{
  int i;

  for (i=0; i<16; i++) {
    dst[i] = src[i];
  }
}

void
buildFrustumMatrix(GLdouble m[16],
                   GLdouble l, GLdouble r, GLdouble b, GLdouble t,
                   GLdouble n, GLdouble f)
{
  m[0] = (2.0*n) / (r-l);
  m[1] = 0.0;
  m[2] = 0.0;
  m[3] = 0.0;

  m[4] = 0.0;
  m[5] = (2.0*n) / (t-b);
  m[6] = 0.0;
  m[7] = 0.0;

  m[8] = (r+l) / (r-l);
  m[9] = (t+b) / (t-b);
  m[10] = -(f+n) / (f-n);
  m[11] = -1.0;

  m[12] = 0.0;
  m[13] = 0.0;
  m[14] = -(2.0*f*n) / (f-n);
  m[15] = 0.0;
}

void
buildPerspectiveMatrix(GLdouble m[16],
                       GLdouble fovy, GLdouble aspect,
                       GLdouble zNear, GLdouble zFar)
{
  GLdouble xmin, xmax, ymin, ymax;

  ymax = zNear * tan(fovy * M_PI / 360.0);
  ymin = -ymax;

  xmin = ymin * aspect;
  xmax = ymax * aspect;

  buildFrustumMatrix(m, xmin, xmax, ymin, ymax, zNear, zFar);
}

/* Build a 4x4 matrix transform based on the parameters for gluLookAt.
 * Code lifted from Brian Paul's MesaGLU.
 */
void
buildLookAtMatrix(GLdouble m[16],
                  GLdouble eyex, GLdouble eyey, GLdouble eyez,
                  GLdouble centerx, GLdouble centery, GLdouble centerz,
                  GLdouble upx, GLdouble upy, GLdouble upz)
{
   GLdouble x[3], y[3], z[3];
   GLdouble mag;

   /* Make rotation matrix */

   /* Z vector */
   z[0] = eyex - centerx;
   z[1] = eyey - centery;
   z[2] = eyez - centerz;
   mag = sqrt( z[0]*z[0] + z[1]*z[1] + z[2]*z[2] );
   if (mag) {  /* mpichler, 19950515 */
      z[0] /= mag;
      z[1] /= mag;
      z[2] /= mag;
   }

   /* Y vector */
   y[0] = upx;
   y[1] = upy;
   y[2] = upz;

   /* X vector = Y cross Z */
   x[0] =  y[1]*z[2] - y[2]*z[1];
   x[1] = -y[0]*z[2] + y[2]*z[0];
   x[2] =  y[0]*z[1] - y[1]*z[0];

   /* Recompute Y = Z cross X */
   y[0] =  z[1]*x[2] - z[2]*x[1];
   y[1] = -z[0]*x[2] + z[2]*x[0];
   y[2] =  z[0]*x[1] - z[1]*x[0];

   /* mpichler, 19950515 */
   /* cross product gives area of parallelogram, which is < 1.0 for
    * non-perpendicular unit-length vectors; so normalize x, y here
    */

   mag = sqrt( x[0]*x[0] + x[1]*x[1] + x[2]*x[2] );
   if (mag) {
      x[0] /= mag;
      x[1] /= mag;
      x[2] /= mag;
   }

   mag = sqrt( y[0]*y[0] + y[1]*y[1] + y[2]*y[2] );
   if (mag) {
      y[0] /= mag;
      y[1] /= mag;
      y[2] /= mag;
   }

#define M(row,col)  m[col*4+row]
   M(0,0) = x[0];  M(0,1) = x[1];  M(0,2) = x[2];  M(0,3) = -x[0]*eyex + -x[1]*eyey + -x[2]*eyez;
   M(1,0) = y[0];  M(1,1) = y[1];  M(1,2) = y[2];  M(1,3) = -y[0]*eyex + -y[1]*eyey + -y[2]*eyez;
   M(2,0) = z[0];  M(2,1) = z[1];  M(2,2) = z[2];  M(2,3) = -z[0]*eyex + -z[1]*eyey + -z[2]*eyez;
   M(3,0) = 0.0;   M(3,1) = 0.0;   M(3,2) = 0.0;   M(3,3) = 1.0;
#undef M
}

/* For debugging purposes. */
void
printMatrix(char *msg, GLdouble m[16])
{
  printf("%s\n", msg);
  printf(" [ %f %f %f %f ]\n", m[0], m[4], m[8], m[12]);
  printf(" [ %f %f %f %f ]\n", m[1], m[5], m[9], m[13]);
  printf(" [ %f %f %f %f ]\n", m[2], m[6], m[10], m[14]);
  printf(" [ %f %f %f %f ]\n\n", m[3], m[7], m[11], m[15]);
}

