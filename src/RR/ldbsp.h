#ifndef _LDBSP_H
#define _LDBSP_H

#include "spline.h"

#define byte  unsigned char
#define word  unsigned short

#define PLANE 0
#define FRONT 1
#define BACK -1
#define SPLIT 2

typedef float MATRIX[4][4];

typedef struct {
        int id,sx,sy,t,s;
        float x,y,z,u,v;
        float tx,ty,tz;
        } VERTEX;

typedef struct {
        float a,b,c,d;
        float ta,tb,tc,td;
        } NORMAL;

typedef struct _FACE {
        int id,material;
        NORMAL normal;
        VERTEX *vertex[3];
        struct _FACE *source_face;
        void *source_triangle;
        } FACE;

typedef struct {
        int      id;
        char     *name;
        int      parent;
        int      vertex_num;
        int      face_num;
        int      bsp_num;
        MATRIX   matrix;
        MATRIX   transform;
        MATRIX   inverse;
        TRACK    pos,rot;
        FACE     *face;
        VERTEX   *vertex;
        void     *obj;
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

WORLD *load_world(char *name);

#endif
