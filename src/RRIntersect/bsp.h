#ifndef RRINTERSECT_BSP_H
#define RRINTERSECT_BSP_H

namespace rrIntersect
{

	// msvc hack
	#define inline __inline

	#define byte  unsigned char
	#define word  unsigned short
	#ifndef dword
	#define dword unsigned long
	#endif
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
		int      face_num;
		int      vertex_num;
		FACE     *face;
		VERTEX   *vertex;
		BSP_TREE *bsp;
		KD_TREE  *kd;
		} OBJECT;

	extern void createAndSaveBsp(FILE *f, OBJECT *obj);

}

#endif
