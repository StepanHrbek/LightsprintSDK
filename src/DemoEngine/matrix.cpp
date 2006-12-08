// modified code from The Mesa 3D graphics library

#include <cmath>
#include <cstdio>
#include "matrix.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void __gluMakeIdentityd(GLdouble m[16])
{
    m[0+4*0] = 1; m[0+4*1] = 0; m[0+4*2] = 0; m[0+4*3] = 0;
    m[1+4*0] = 0; m[1+4*1] = 1; m[1+4*2] = 0; m[1+4*3] = 0;
    m[2+4*0] = 0; m[2+4*1] = 0; m[2+4*2] = 1; m[2+4*3] = 0;
    m[3+4*0] = 0; m[3+4*1] = 0; m[3+4*2] = 0; m[3+4*3] = 1;
}

GLboolean invertMatrix(GLdouble *inverse, const GLdouble *src)
{
	int i, j, k;
	double t;
	GLdouble temp[4][4];
	 
	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) {
		    temp[i][j] = src[i*4+j];
		}
	}
	__gluMakeIdentityd(inverse);
	
	for (i = 0; i < 4; i++) {
		if (temp[i][i] == 0.0f) {
		    /*
		    ** Look for non-zero element in column
		    */
		    for (j = i + 1; j < 4; j++) {
				if (temp[j][i] != 0.0f) {
				    break;
				}
		    }
		
		    if (j != 4) {
				/*
				 ** Swap rows.
				 */
				for (k = 0; k < 4; k++) {
				    t = temp[i][k];
				    temp[i][k] = temp[j][k];
				    temp[j][k] = t;
			
				    t = inverse[i*4+k];
				    inverse[i*4+k] = inverse[j*4+k];
				    inverse[j*4+k] = t;
				}
		    }
		    else {
				/*
				** No non-zero pivot.  The matrix is singular, which shouldn't
				** happen.  This means the user gave us a bad matrix.
				*/
				return GL_FALSE;
		    }
		}
		
		t = 1.0f / temp[i][i];
		for (k = 0; k < 4; k++) {
		    temp[i][k] *= t;
		    inverse[i*4+k] *= t;
		}
		for (j = 0; j < 4; j++) {
		    if (j != i) {
				t = temp[j][i];
				for (k = 0; k < 4; k++) {
					    temp[j][k] -= temp[i][k]*t;
					    inverse[j*4+k] -= inverse[i*4+k]*t;
				}
		    }
		}
	}
	return GL_TRUE;
}

void buildFrustumMatrix(GLdouble m[16],
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

void buildPerspectiveMatrix(GLdouble m[16],
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

void buildLookAtMatrix(GLdouble m[16],
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
