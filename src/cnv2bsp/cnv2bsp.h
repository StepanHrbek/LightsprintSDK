#ifndef _3DS2BSP_H_
#define _3DS2BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

// msvc hack
#define inline __inline

#define byte  unsigned char
#define word  unsigned short
#define dword unsigned long
#define PATH float

#define SCALE 1000
#define SQR(A) ((A)*(A))

#define BEST  0
#define MEAN  1
#define BIG   2
#define BESTN 3

#define nALLOC(A,B) (A *)malloc((B)*sizeof(A))
#define ALLOC(A) nALLOC(A,1)

typedef float VECTOR[3];

typedef float MATRIX[4][4];

typedef struct {
        int id,side,used;
        float x,y,z,u,v;
        } VERTEX;

typedef struct {
        float a,b,c,d;
        } NORMAL;

typedef struct {
        int id,side,material;
        VERTEX *vertex[3];
        NORMAL normal;
        } FACE;

typedef struct {
        float hi[3];
        float lo[3];
        } BBOX;

typedef struct _BSP_TREE {
        struct _BSP_TREE *front;
        struct _BSP_TREE *back;
        FACE **plane;
        } BSP_TREE;

typedef struct _KD_TREE {
        struct _KD_TREE *front;
        struct _KD_TREE *back;
        FACE **leaf;
        int axis;
        VERTEX *root;
        } KD_TREE;

typedef struct {
        int      id;
        char     *name;
        int      parent;
        int      pos_num;
        int      rot_num;
        int      face_num;
        int      vertex_num;
        PATH     *pt,*px,*py,*pz;
        PATH     *qt,*qw,*qx,*qy,*qz;
        FACE     *face;
        VERTEX   *vertex;
        MATRIX   matrix;
        BSP_TREE *bsp;
        KD_TREE  *kd;
        } OBJECT;

typedef struct { char *name; } MATERIAL;

typedef struct {
        char *name;
        int target_num;
        int position_num;
        PATH *tt,*tx,*ty,*tz;
        PATH *pt,*px,*py,*pz;
        } CAMERA;

typedef struct {
        int        start_frame;
        int        end_frame;
        int        material_num;
        int        camera_num;
        int        object_num;
        OBJECT     *object;
        CAMERA     *camera;
        MATERIAL   *material;
        } WORLD;

extern WORLD *load_3DS(char *fn);
extern WORLD *load_MGF(char *fn, int mode, int dir);
extern WORLD *load_Scene(char *in_name, char *out_name, int dir, int mode, int max, int root, int num);

#ifdef __cplusplus
}
#endif

#endif
