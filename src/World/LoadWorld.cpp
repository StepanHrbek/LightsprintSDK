#include <math.h>
#include <memory.h>
#include "World.h"

/*WORLD *load_world(char *name)
{
	return load_world_r3d(name);
}*/

//////////////////////////////////////////////////////////////////////////////
//
// MATRIX in 3d

void matrix_Copy(MATRIX s, MATRIX d)
{
	memcpy(d,s,sizeof(MATRIX));
}

void matrix_Invert(MATRIX s, MATRIX d)
{
	int i,j; float det;

	d[0][0] =   s[1][1] * s[2][2] - s[1][2] * s[2][1];
	d[0][1] = - s[0][1] * s[2][2] + s[0][2] * s[2][1];
	d[0][2] =   s[0][1] * s[1][2] - s[0][2] * s[1][1];

	det = d[0][0] * s[0][0] + d[0][1] * s[1][0] + d[0][2] * s[2][0];

	d[1][0] = - s[1][0] * s[2][2] + s[1][2] * s[2][0];
	d[1][1] =   s[0][0] * s[2][2] - s[0][2] * s[2][0];
	d[1][2] = - s[0][0] * s[1][2] + s[0][2] * s[1][0];

	d[2][0] =   s[1][0] * s[2][1] - s[1][1] * s[2][0];
	d[2][1] = - s[0][0] * s[2][1] + s[0][1] * s[2][0];
	d[2][2] =   s[0][0] * s[1][1] - s[0][1] * s[1][0];

	for (j=0;j<3;j++) for (i=0;i<3;i++) d[j][i]/=det;

	d[3][0] = - s[3][0]*d[0][0] - s[3][1]*d[1][0] - s[3][2]*d[2][0];
	d[3][1] = - s[3][0]*d[0][1] - s[3][1]*d[1][1] - s[3][2]*d[2][1];
	d[3][2] = - s[3][0]*d[0][2] - s[3][1]*d[1][2] - s[3][2]*d[2][2];

	d[0][3] = 0;
	d[1][3] = 0;
	d[2][3] = 0;
	d[3][3] = 1;
}

void matrix_Mul(MATRIX a, MATRIX b)
{
	MATRIX t; int i, j;

	for (i=0;i<4;i++) for (j=0;j<4;j++)
		t[i][j]=a[0][j]*b[i][0]+a[1][j]*b[i][1]+
		a[2][j]*b[i][2]+a[3][j]*b[i][3];

	for (i=0;i<4; i++) for (j=0;j<4;j++) a[i][j]=t[i][j];
}

void matrix_Init(MATRIX a)
{
	int i,j; for (j=0;j<4;j++) {for (i=0;i<4;i++) a[i][j]=0;a[j][j]=1;}
}

void matrix_Move(MATRIX m, float dx, float dy, float dz)
{
	MATRIX t; matrix_Init(t);
	t[3][0]=dx; t[3][1]=dy; t[3][2]=dz;
	matrix_Mul(m,t);
}

void matrix_Rotate(MATRIX m, float a, int axis)
{
	MATRIX t; matrix_Init(t);
	switch (axis) {
  case _X_: t[1][2] = sin(a);
	  t[1][1] = cos(a);
	  t[2][2] = cos(a);
	  t[2][1] =-sin(a);
	  break;
  case _Y_: t[0][0] = cos(a);
	  t[0][2] =-sin(a);
	  t[2][2] = cos(a);
	  t[2][0] = sin(a);
	  break;
  case _Z_: t[0][0]= cos(a);
	  t[0][1]= sin(a);
	  t[1][1]= cos(a);
	  t[1][0]=-sin(a);
	  break;
	}
	matrix_Mul(m,t);
}

void matrix_Hierarchy(HIERARCHY *h, MATRIX m, float t)
{
	if(!h) return;

	OBJECT *o=h->object; float px,py,pz;
	MATRIX sm,im,tm; spline_MATRIX qm; matrix_Copy(m,sm);

	if (h->brother) matrix_Hierarchy(h->brother,sm,t);

	spline_Interpolate(&o->pos,t,&px,&py,&pz,0);
	spline_Interpolate(&o->rot,t,0,0,0,&qm);

	tm[0][0] = qm.xx; tm[1][0] = qm.xy; tm[2][0] = qm.xz; tm[3][0] = px;
	tm[0][1] = qm.yx; tm[1][1] = qm.yy; tm[2][1] = qm.yz; tm[3][1] = py;
	tm[0][2] = qm.zx; tm[1][2] = qm.zy; tm[2][2] = qm.zz; tm[3][2] = pz;
	tm[0][3] = 0;     tm[1][3] = 0;     tm[2][3] = 0;     tm[3][3] = 1;

	matrix_Mul(tm,o->matrix);
	matrix_Mul(sm,tm);
	matrix_Invert(sm,im);
	matrix_Copy(sm,o->transform);
	matrix_Copy(im,o->inverse);

	if (h->child) matrix_Hierarchy(h->child,sm,t);
}

