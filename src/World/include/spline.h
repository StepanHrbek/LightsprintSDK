#ifndef _SPLINE_H
#define _SPLINE_H

#ifdef _MSC_VER
#pragma comment(lib,"World.lib")
#endif

typedef struct { float w,x,y,z; } QUAT;
typedef struct { float xx,xy,xz,yx,yy,yz,zx,zy,zz; } spline_MATRIX;
typedef struct { QUAT pos,ds,dd,aa; float frame,tens,cont,bias,et,ef; } KEY;
typedef struct { KEY *keys; int num,loop,repeat; } TRACK;

void spline_Init(TRACK *track, int quat);
void spline_Quat2invMatrix(QUAT q, spline_MATRIX *m);
void spline_Interpolate(TRACK *track, float time,
                        float *x, float *y, float *z,
                        spline_MATRIX *m);
#endif
