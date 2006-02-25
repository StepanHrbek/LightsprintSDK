#ifndef _WORLD_H
#define _WORLD_H

#ifdef _MSC_VER
	#ifdef NDEBUG
		#pragma comment(lib,"World_s.lib")
	#else
		#pragma comment(lib,"World_sd.lib")
	#endif
#endif

#include "spline.h"

#define _X_ 0
#define _Y_ 1
#define _Z_ 2

typedef float MATRIX[4][4];

typedef struct {
        int id;
        float x,y,z;//,u,v;
        } VERTEX;

typedef struct {
        float a,b,c,d;
        float ta,tb,tc,td;
        } NORMAL;

typedef struct _FACE {
        int id,material;
        //NORMAL normal;
        VERTEX *vertex[3];
        } FACE;

typedef struct {
        int      id;
        char     *name;
        int      parent;
        int      vertex_num;
        int      face_num;
        MATRIX   matrix;
        MATRIX   transform;
        MATRIX   inverse;
        TRACK    pos,rot;
        FACE     *face;
        VERTEX   *vertex;
        unsigned objectHandle; // RRScene::ObjectHandle
        } OBJECT;

typedef struct { char *name; } MATERIAL;

typedef struct {
        char *name;
        TRACK pos,tar;
        MATRIX matrix,inverse;
        } CAMERA;

typedef struct _HIERARCHY {
        OBJECT *object;
        struct _HIERARCHY *child;
        struct _HIERARCHY *brother;
        } HIERARCHY;

typedef struct {
        int       start_frame;
        int       end_frame;
        int       material_num;
        int       camera_num;
        int       object_num;
        MATRIX    matrix;
        OBJECT    *object;
        CAMERA    *camera;
        MATERIAL  *material;
        HIERARCHY *hierarchy;
        } WORLD;

void   matrix_Invert(MATRIX s, MATRIX d);
void   matrix_Mul(MATRIX a, MATRIX b);
void   matrix_Init(MATRIX a);
void   matrix_Copy(MATRIX s, MATRIX d);
void   matrix_Move(MATRIX m, float dx, float dy, float dz);
void   matrix_Rotate(MATRIX m, float a, int axis);
void   matrix_Hierarchy(HIERARCHY *h, MATRIX m, float t);

WORLD *load_world(char *name);

#endif
