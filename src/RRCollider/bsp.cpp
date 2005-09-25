#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "bsp.h"
#include "IntersectBsp.h"
#include "IntersectBspFast.h"
#include "IntersectBspCompact.h"

//#define CHECK_KD // slow checks of kd build correctness

// more strict separation of faces into kd_front / kd_back
// reduces subtrees -> can only make traversal faster
// makes bunny (round shape) 0% faster, rr scenes (blocky) 5% slower(why???)
//#define STRICT_SEPARATION 

#ifdef STRICT_SEPARATION
#include "vec.h"
#include "pcube.h"
#endif

#define PLANE_PRIZE   (buildParams.prizePlane)   // 1
#define SPLIT_PRIZE   (buildParams.prizeSplit)   // 150
#define BALANCE_PRIZE (buildParams.prizeBalance) // 5
/*
worst situations
 worst balance (0 vs faces) has prize faces*BALLANCE_PRIZE -> use bsp or kd_leaf
 split of all faces is equally bad and has prize faces*SPLIT_PRIZE -> use kd_leaf

*/

namespace rrCollider
{


// first_nonvoid<T1,T2>.T is (T1!=void)?T1:T2
template <typename T1,typename T2>
struct first_nonvoid{
	typedef T1 T;
	static const bool value = false;
};

template <typename T2>
struct first_nonvoid<void,T2>{
	typedef T2 T;
	static const bool value = true;
}; 


typedef float VECTOR[3];

static void cross_product(VECTOR res, VECTOR a, VECTOR b)
{
	res[0]=b[1]*a[2]-a[1]*b[2];
	res[1]=b[2]*a[0]-a[2]*b[0];
	res[2]=b[0]*a[1]-a[0]*b[1];
}

float FACE::getArea() const
{
	VECTOR u,v,n;

	u[0]=vertex[1]->x-vertex[0]->x;
	u[1]=vertex[1]->y-vertex[0]->y;
	u[2]=vertex[1]->z-vertex[0]->z;

	v[0]=vertex[2]->x-vertex[0]->x;
	v[1]=vertex[2]->y-vertex[0]->y;
	v[2]=vertex[2]->z-vertex[0]->z;

	cross_product(n,v,u);

	return n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
}

void FACE::fillNormal()
{
	VECTOR u,v,n; float l;

	u[0]=vertex[1]->x-vertex[0]->x;
	u[1]=vertex[1]->y-vertex[0]->y;
	u[2]=vertex[1]->z-vertex[0]->z;

	v[0]=vertex[2]->x-vertex[0]->x;
	v[1]=vertex[2]->y-vertex[0]->y;
	v[2]=vertex[2]->z-vertex[0]->z;

	cross_product(n,v,u);

	l=(float)sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);

	n[0]/=l; n[1]/=l; n[2]/=l;
	if(!( IS_NUMBER(n[0]) && IS_NUMBER(n[1]) && IS_NUMBER(n[2]) )) { n[0]=0; n[1]=0; n[2]=0; }

	normal.a=n[0];
	normal.b=n[1];
	normal.c=n[2];
	normal.d=-(n[0]*vertex[0]->x+
		n[1]*vertex[0]->y+
		n[2]*vertex[0]->z);
}

void FACE::fillMinMax()
{
	for(unsigned i=0;i<3;i++)
	{
		min[i] = MIN3((*vertex[0])[i],(*vertex[1])[i],(*vertex[2])[i]);
		max[i] = MAX3((*vertex[0])[i],(*vertex[1])[i],(*vertex[2])[i]);
	}
}


class BspBuilder
{
public:

struct BBOX
{
	float hi[3];
	float lo[3];
	float getSurfaceSize() const
	{
		float a=hi[0]-lo[0];
		float b=hi[1]-lo[1];
		float c=hi[2]-lo[2];
		assert(a>=0);
		assert(b>=0);
		assert(c>=0);
		return 2*(a*(b+c)+b*c);
	}
	float getEdgeSize() const
	{
		float a=hi[0]-lo[0];
		float b=hi[1]-lo[1];
		float c=hi[2]-lo[2];
		assert(a>=0);
		assert(b>=0);
		assert(c>=0);
		return a+b+c;
	}
};

struct BSP_TREE 
{
	BSP_TREE *front;
	BSP_TREE *back;
	unsigned kdnodes;// kd internal nodes in whole tree
	unsigned kdleaves;// kd leaves in whole tree
	unsigned bspnodes;// bsp nodes in whole tree
	unsigned faces;// face instances in whole tree
	// bsp
	const FACE **plane;
	// kd
	const VERTEX *kdroot;
	int axis;
	const FACE **leaf;
};

#define CACHE_SIZE 1000
float   DELTA_INSIDE_PLANE; // min distance from plane to be recognized as non-plane
#define DELTA_NORMALS_MATCH 0.001 // min distance of normals to be recognized as non-plane
#define PLANE 0 // 2d in splitting plane
#define FRONT 1 // 2d in front, 1d may be in plane
#define BACK -1 // 2d in back, 1d may be in plane
#define SPLIT 2 // 2d partially in front, partially in back
#define NONE  3 // 2d outside bbox, 1d may be in front or back

#define nALLOC(A,B) (A *)malloc((B)*sizeof(A))
#define ALLOC(A) nALLOC(A,1)

#define SQR(A) ((A)*(A))
#undef  ABS
#define ABS(x) (((x)>0)?(x):(-(x)))

unsigned    nodes,faces;
BSP_TREE*   bsptree;
unsigned    bsptree_id;
BuildParams buildParams;

BspBuilder() : buildParams(RRCollider::IT_BSP_FASTEST)
{
	bsptree = NULL;
	bsptree_id = CACHE_SIZE;
}

BSP_TREE *new_node()
{
	BSP_TREE* oldbsptree=bsptree;
	if (bsptree_id<CACHE_SIZE) return bsptree+bsptree_id++;
	bsptree=nALLOC(BSP_TREE,CACHE_SIZE+1); bsptree[CACHE_SIZE].front=oldbsptree; bsptree_id=1; return bsptree;
}

static void free_node(BSP_TREE* t)
{
	free(t->plane);
	if(t->front) free_node(t->front);
	if(t->back) free_node(t->back);
	free(t->leaf);
}

~BspBuilder()
{
	while(bsptree)
	{
		BSP_TREE* tmp = bsptree[CACHE_SIZE].front;
		free(bsptree);
		bsptree = tmp;
	}
}

static int locate_vertex_bsp(const FACE *f, const VERTEX *v, float DELTA_INSIDE_PLANE)
{
	float r = f->normal.a*v->x+f->normal.b*v->y+f->normal.c*v->z+f->normal.d;
	if(ABS(r)<DELTA_INSIDE_PLANE) return PLANE; 
	if(r>0) return FRONT; 
	return BACK;
}

static int locate_vertex_kd(float splitValue, int splitAxis, const VERTEX *v)
{
	assert(splitAxis>=0 && splitAxis<3);
	return ((*v)[splitAxis]>splitValue) ? FRONT : ( ((*v)[splitAxis]<splitValue) ? BACK : PLANE );
}

static int normals_match(const FACE *plane, const FACE *face)
{
	return
		fabs(plane->normal.a+face->normal.a)+fabs(plane->normal.b+face->normal.b)+fabs(plane->normal.c+face->normal.c)<DELTA_NORMALS_MATCH ||
		fabs(plane->normal.a-face->normal.a)+fabs(plane->normal.b-face->normal.b)+fabs(plane->normal.c-face->normal.c)<DELTA_NORMALS_MATCH;
}

static int locate_face_bsp(const FACE *plane, const FACE *face, float DELTA_INSIDE_PLANE)
{
	int i,f=0,b=0,p=0;

	for(i=0;i<3;i++)
		switch(locate_vertex_bsp(plane,face->vertex[i],DELTA_INSIDE_PLANE))
		{
			case BACK:b++;break;
			case FRONT:f++;break;
			case PLANE:f++;b++;p++;break;
		}

	if(plane==face)
	{
		assert(p==3);
		return PLANE;
	}
	if(p==3 && normals_match(plane,face)) return PLANE;
	if(f==3) return FRONT;
	if(b==3) return BACK;
	return SPLIT;
}

#ifdef STRICT_SEPARATION
static RRReal* get_polygon_normal(RRReal normal[3], int nverts, const RRReal verts[/* nverts */][3])
{
	int i;
	real tothis[3], toprev[3], cross[3];

	/*
	* Triangulate the polygon and sum up the nverts-2 triangle normals.
	*/
	ZEROVEC3(normal);
	VMV3(toprev, verts[1], verts[0]);  	/* 3 subtracts */
	for (i = 2; i <= nverts-1; ++i) {   /* n-2 times... */
		VMV3(tothis, verts[i], verts[0]);    /* 3 subtracts */
		VXV3(cross, toprev, tothis);         /* 3 subtracts, 6 multiplies */
		VPV3(normal, normal, cross);         /* 3 adds */
		SET3(toprev, tothis);
	}
	return normal;
}

static bool face_intersects_box(const FACE* face, BBOX* bbox)
{
	// find out transformation a*point+b from box to [-0.5,-0.5,-0.5]..[0.5,0.5,0.5] box
	Vec3 a = (*(Vec3*)bbox->hi-*(Vec3*)bbox->lo);
	a = Vec3(1/a.x,1/a.y,1/a.z);
	Vec3 b = (*(Vec3*)bbox->lo+*(Vec3*)bbox->hi)*a*-0.5;
	// transform face
	real tri[3][3]; 
	for(unsigned i=0; i<3; i++)
		for(unsigned j=0; j<3; j++)
			tri[i][j] = a[j]*(*face->vertex[i])[j]+b[j];
	// Inf correction
	assert(IS_VEC3(tri[0]) && IS_VEC3(tri[1]) && IS_VEC3(tri[2]));
	// transform normal
	real normal[3];
	get_polygon_normal(normal, 3, tri);
	// call Hatch+Green test
	return pcube::fast_polygon_intersects_cube(3,tri,normal,0,0)!=0;
}
#endif

int locate_face_kd(float splitValue, int axis, BBOX *bbox, const FACE *face)
// bbox = bbox of node
// splitValue/axis = where bbox is splited
// face = object that needs to be located
{
	int i,f=0,b=0,p=0;

	for (i=0;i<3;i++)
		switch (locate_vertex_kd(splitValue,axis,face->vertex[i])) 
		{
			case BACK:b++;break;
			case FRONT:f++;break;
			case PLANE:f++;b++;p++;break;
		}

	if (p==3) return PLANE;
	if (f==3) return FRONT;
	if (b==3) return BACK;
#ifdef STRICT_SEPARATION
	// new: verify that face really intersects both subboxes
	BBOX bbox2 = *bbox;
	RRReal dif = DELTA_INSIDE_PLANE;
	for(unsigned i=0;i<3;i++)
	{
		// tiny grow to hit also all tringles in faces of bbox 
		// (only some faces of box should be covered, but nevermind)
		bbox2.lo[i] -= dif;
		bbox2.hi[i] += dif;
	}
	float tmp = bbox2.lo[axis];
	bbox2.lo[axis] = splitValue;
	bool frontHit = face_intersects_box(face,&bbox2);
	bbox2.lo[axis] = tmp;
	bbox2.hi[axis] = splitValue;
	bool backHit = face_intersects_box(face,&bbox2);

	if(frontHit && backHit) return SPLIT;
	if(frontHit) return FRONT;
	if(backHit) return BACK;
	return NONE; // shouldn't happen
#else
	// old
	return SPLIT;
#endif
}

static int compare_face_minx_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->min[0]-(*(FACE**)p1)->min[0];
	return (f<0)?+1:((f>0)?-1:0);
}

static int compare_face_maxx_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->max[0]-(*(FACE**)p1)->max[0];
	return (f<0)?+1:((f>0)?-1:0);
}

static int compare_face_miny_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->min[1]-(*(FACE**)p1)->min[1];
	return (f<0)?+1:((f>0)?-1:0);
}

static int compare_face_maxy_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->max[1]-(*(FACE**)p1)->max[1];
	return (f<0)?+1:((f>0)?-1:0);
}

static int compare_face_minz_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->min[2]-(*(FACE**)p1)->min[2];
	return (f<0)?+1:((f>0)?-1:0);
}

static int compare_face_maxz_asc( const void *p1, const void *p2 )
{
	float f=(*(FACE**)p2)->max[2]-(*(FACE**)p1)->max[2];
	return (f<0)?+1:((f>0)?-1:0);
}

struct ROOT_INFO
{
	unsigned prize;
	unsigned front;
	unsigned plane;
	unsigned back;
	unsigned split;
	// kd
	float    fprize;
	unsigned axis;
	float    value; // position of splitting plane in axis
	FACE*    face;
	bool     max;
};

VERTEX *find_best_root_kd(BBOX *bbox, const FACE **list, ROOT_INFO* bestinfo)
{
	ROOT_INFO info, /*info2,*/ best_dee, best_havran;

	best_dee.prize = UINT_MAX;
	best_dee.face = NULL;
	best_havran.fprize = 1e30f;
	best_havran.face = NULL;

	// prepare sortarrays
	unsigned faces=0;
	for(int i=0;list[i];i++) faces++;
	assert(faces);
	if(!faces) return NULL;
	FACE** minx = new FACE*[faces];
	FACE** maxx = new FACE*[faces];
	memcpy(minx,list,sizeof(FACE*)*faces);
	memcpy(maxx,list,sizeof(FACE*)*faces);

	for(info.axis=0;info.axis<3;info.axis++)
	{
		unsigned axis = info.axis;

		// sort sortarrays
		qsort(minx,faces,sizeof(FACE*),(axis==0)?compare_face_minx_asc:((axis==1)?compare_face_miny_asc:compare_face_minz_asc));
		qsort(maxx,faces,sizeof(FACE*),(axis==0)?compare_face_maxx_asc:((axis==1)?compare_face_maxy_asc:compare_face_maxz_asc));

		// select root
		unsigned minxi = 0;
		unsigned maxxi = 0;
		while(1)
		{
			info.max = false;
			info.face = minx[minxi];
			info.value = minx[minxi]->min[axis];

			// skip faces in plane splitValue
			info.plane = 0;
			while(maxxi<faces && info.value >= maxx[maxxi]->max[axis])
			{
				if(info.value==maxx[maxxi]->max[axis] && 
					maxx[maxxi]->min[axis]==maxx[maxxi]->max[axis]) info.plane++;

				/*/----
				// try to split also at upper bound of tri, not just at lower bound
				info2 = info;
				info2.max = true;
				info2.face = maxx[maxxi];
				info2.value = maxx[maxxi]->max[axis];
				if(info2.value<bbox->lo[axis]) goto info2_done;
				if(info2.value>bbox->hi[axis]) goto info2_done;
				{int split_num=0;
				int plane_num=0;
				int front_num=0;
				int back_num=0;
				for(int i=0;list[i];i++)
				{
					switch(locate_face_kd(info2.value,axis,list[i])) 
					{
					case BACK: back_num++; break;
					case PLANE: plane_num++; break;
					case FRONT: front_num++; break;
					case SPLIT: split_num++; break;
					}
				}
				info2.back = back_num;
				info2.front = front_num;
				info2.split = split_num;
				info2.plane = plane_num;
				info2.prize = 1 + info2.split*SPLIT_PRIZE + ABS((int)info2.front-(int)(info2.back+info2.plane))*BALANCE_PRIZE;
				}
				// backsurface
				{
					float tmp = bbox->hi[axis];
					bbox->hi[axis] = info2.value;
					float backsurface = bbox->getSurfaceSize();
					bbox->hi[axis] = tmp;
					// frontsurface
					tmp = bbox->lo[axis];
					bbox->lo[axis] = info2.value;
					float frontsurface = bbox->getSurfaceSize();
					bbox->lo[axis] = tmp;
					// prize
					info2.fprize = (info2.front+info2.split)*frontsurface + (info2.back+info2.plane+info2.split)*backsurface;
				}
				if(info2.prize<best_dee.prize) best_dee = info2;
				if(info2.fprize<best_havran.fprize) best_havran = info2;
info2_done:			//----
*/
				maxxi++;
			}

			// skip planes outside bbox
			if(info.value<bbox->lo[axis]) goto next;
			if(info.value>bbox->hi[axis]) goto next;

			// calculate prize of split plane at splitValue
			info.back = maxxi-info.plane;	// pocet facu nalevo od rezu v splitValue
			info.front = faces-minxi-info.plane; // pocet facu napravo od rezu v splitValue
			info.split = minxi-info.back; // pocet facu preseklych rezem v splitValue
			info.prize = 1 + info.split*SPLIT_PRIZE + ABS((int)info.front-(int)(info.back+info.plane))*BALANCE_PRIZE;

#ifdef SUPPORT_EMPTY_KDNODE
			// skip planes that only cut off <15% of empty space
			{
				real range = (bbox->hi[axis]-bbox->lo[axis]) * 0.15f;
				if(info.front+info.split==0 && info.value>bbox->hi[axis]-range) goto next;
				if(info.back+info.plane+info.split==0 && info.value<bbox->lo[axis]+range) goto next;
			}
#endif

			// backsurface
			{
			float tmp = bbox->hi[axis];
			bbox->hi[axis] = info.value;
			float backsurface = bbox->getSurfaceSize();
			bbox->hi[axis] = tmp;
			// frontsurface
			tmp = bbox->lo[axis];
			bbox->lo[axis] = info.value;
			float frontsurface = bbox->getSurfaceSize();
			bbox->lo[axis] = tmp;
			// prize
			info.fprize = (info.front+info.split)*frontsurface + (info.back+info.plane+info.split)*backsurface;
			}


//			if(info.front && (info.back+info.plane))
			{
				if(info.prize<best_dee.prize) best_dee = info;
				if(info.fprize<best_havran.fprize) best_havran = info;
			}

			// step forward to next plane
next:
			minxi++;
			if(minxi==faces) break;
			if(minx[minxi]->min[axis] == minx[minxi-1]->min[axis]) goto next; // another identical planes -> skip it
		}
	}

	// delete sortarrays
	delete[] minx;
	delete[] maxx;

	info = best_havran;
#ifdef SUPPORT_EMPTY_KDNODE
	if(buildParams.kdHavran)
	{
		if(!info.face) info = best_dee;
		if(!info.face) return NULL;
	} 
	else
#endif
	{
		if(!info.face || info.front==0 || info.back+info.plane==0) info = best_dee;
		//if(!info.face || info.front<=faces/100 || info.back+info.plane<=faces/100) info = best_dee;
		if(!info.face || info.front==0 || info.back+info.plane==0) return NULL;
	}

	FACE* f = info.face;
	assert(f);
	if(!f) return NULL;
	VERTEX* result;
	if(info.max)
	{
		if(f->max[info.axis]==(*f->vertex[0])[info.axis]) result = f->vertex[0]; else
		if(f->max[info.axis]==(*f->vertex[1])[info.axis]) result = f->vertex[1]; else
		{assert(f->max[info.axis]==(*f->vertex[2])[info.axis]);	result = f->vertex[2];}
	} else {
		if(f->min[info.axis]==(*f->vertex[0])[info.axis]) result = f->vertex[0]; else
		if(f->min[info.axis]==(*f->vertex[1])[info.axis]) result = f->vertex[1]; else
		{assert(f->min[info.axis]==(*f->vertex[2])[info.axis]);	result = f->vertex[2];}
	}
	*bestinfo = info;


	return result;
}
/*
FACE *find_best_root_bsp_all(const FACE **list)
{
	int i,j,prize,best_prize=0; FACE *best=NULL;

	for (i=0;list[i];i++) 
	{
		int front=0,back=0,plane=0,split=0;

		for (j=0;list[j];j++)
			switch (locate_face_bsp(list[i],list[j])) 
		{
			case PLANE:plane++;break;
			case FRONT:front++;break;
			case SPLIT:split++;break;
			case BACK:back++;break;
		}

		prize = split*SPLIT_PRIZE+plane*PLANE_PRIZE+ABS(front-back)*BALANCE_PRIZE;

		if (prize<best_prize || best==NULL) { best_prize=prize; best=list[i]; }
	}

	return best;
}
*/
static void mid_point(const FACE *f, float *x, float *y, float *z)
{
	*x=0; *y=0; *z=0;

	for (int i=0;i<3;i++) 
	{
		*x+=f->vertex[i]->x;
		*y+=f->vertex[i]->y;
		*z+=f->vertex[i]->z;
	}

	*x/=3; *y/=3; *z/=3;
}

static float dist_point(const FACE *f, float x, float y, float z)
{
	float dist=0; int i;

	for (i=0;i<3;i++)
		dist+=SQR(x-f->vertex[i]->x)+
		SQR(y-f->vertex[i]->y)+
		SQR(z-f->vertex[i]->z);

	return dist/3;
}
struct FACE_Q 
{
	float q;
	const FACE *f;
};

static int compare_face_q_desc( const void *p1, const void *p2 )
{
	float f=((FACE_Q *)p2)->q-((FACE_Q *)p1)->q;
	return (f<0)?-1:((f>0)?1:0);
}

const FACE *find_best_root_bsp(const FACE **list, ROOT_INFO* bestinfo)
{
	int best_prize=0; 
	const FACE *best=NULL;
	float px=0,py=0,pz=0;
	unsigned pn=0;
	FACE_Q *tmp;

	for(int i=0;list[i];i++) 
	{
		float x,y,z;
		mid_point(list[i],&x,&y,&z);
		px+=x; py+=y; pz+=z; pn++; 
	}

	if(!pn) return NULL;

	px/=pn; py/=pn; pz/=pn;

	tmp=nALLOC(FACE_Q,pn);

	for(int i=0;list[i];i++) 
	{
		float dist=dist_point(list[i],px,py,pz);
		float size=list[i]->getArea();
		tmp[i].q=(float)(10*sqrt(size))-dist;
		tmp[i].f=list[i];
	}

	qsort(tmp,pn,sizeof(FACE_Q),compare_face_q_desc);

	for(unsigned i=0;i<buildParams.bspBestN && i<pn;i++) 
	{ 
		int front=0,back=0,plane=0,split=0;

		for(int j=0;list[j];j++)
			switch(locate_face_bsp(tmp[i].f,list[j],DELTA_INSIDE_PLANE)) 
			{
				case PLANE:plane++;break;
				case FRONT:front++;break;
				case SPLIT:split++;break;
				case BACK:back++;break;
			}

		int prize=split*SPLIT_PRIZE+plane*PLANE_PRIZE+ABS(front-back)*BALANCE_PRIZE;

		if(prize<best_prize || best==NULL) 
		{ 
			best_prize=prize; 
			bestinfo->back = back;
			bestinfo->front = front;
			bestinfo->plane = plane;
			bestinfo->split = split;
			bestinfo->prize = prize;
			best=tmp[i].f; 
			assert(plane); // at least best(triangle that makes plane) must be in plane
		}

	}

	free(tmp);

	assert(bestinfo->plane>=1);
	return best;
}

BSP_TREE *create_bsp(const FACE **space, BBOX *bbox, bool kd_allowed)
{
	unsigned pn=0;
	for(int i=0;space[i];i++) pn++;

	// alloc node
	BSP_TREE *t=new_node();
	memset(t,0,sizeof(BSP_TREE));

	// select splitting plane
	const FACE* bsproot = NULL;
	const VERTEX* kdroot = NULL; 
	ROOT_INFO info_bsp, info_kd;
	if(!kd_allowed || pn<buildParams.kdMinFacesInTree)
	{
		bsproot=find_best_root_bsp(space,&info_bsp);
		if(bsproot) assert(info_bsp.plane>=1);
	} else {
		kdroot = find_best_root_kd(bbox,space,&info_kd);
#ifdef SUPPORT_EMPTY_KDNODE
		if(buildParams.kdHavran)
		{
			if(kdroot)
			{
				if(info_kd.value<=bbox->lo[info_kd.axis] || info_kd.value>=bbox->hi[info_kd.axis]) 
					kdroot = NULL; // split je na kraji boxu, tj. jeden ze synu je stejne velky jako otec, jisty nekonecny rozvoj -> nebrat
			}
		}
		else
#endif
		{
			if(info_kd.front==0 || info_kd.back+info_kd.plane==0) 
				kdroot = NULL; // jeden ze synu bude obsahovat stejne trianglu jako otec, nebezpeci nekonecneho rozvoje -> nebrat
		}
		if((buildParams.kdHavran==0 && pn<buildParams.bspMaxFacesInTree) || !kdroot)
		{
			bsproot = find_best_root_bsp(space,&info_bsp);
			if(bsproot) assert(info_bsp.plane>=1);
			if(buildParams.kdHavran==0 && kdroot && info_kd.prize<info_bsp.prize+pn)
				bsproot=NULL; 
			else 
				kdroot=NULL;
		}
	}
	
	// create kd leaf
	if(buildParams.kdLeaf && bsproot && info_bsp.front==0 && info_bsp.back==0 && info_bsp.plane<3)
	{
		if(pn>7) printf("*%d",pn);
		t->plane  =NULL;
		t->leaf   =space;
		t->front  =NULL;
		t->back   =NULL; 
		t->axis   =3;
		t->kdroot =NULL; 
		return t; 
	}

	// leaf -> exit
	if(!bsproot && !kdroot) 
	{
		if(pn>5000) printf("No split in %d faces, bestN=%d",pn,buildParams.bspBestN);//!!!
		t->plane  =space;
		t->leaf   =NULL;
		t->front  =NULL;
		t->back   =NULL; 
		t->axis   =3; 
		t->kdroot =NULL; 
		return t; 
	}

	if(kdroot)
	{
		assert((*kdroot)[info_kd.axis]==info_kd.value);
	}

	// calculate sizes of front/back/plane
	int split_num=0;
	int plane_num=0;
	int front_num=0;
	int back_num=0;
	int none_num=0;
	for(int i=0;space[i];i++)
	{
		//if(!kdroot && space[i]==bsproot) {plane_num++;continue;} // insert bsproot into plane
		int side = kdroot ? locate_face_kd((*kdroot)[info_kd.axis],info_kd.axis,bbox,space[i]) : locate_face_bsp(bsproot,space[i],DELTA_INSIDE_PLANE);
		switch(side) 
		{
			case BACK: back_num++; break;
			case PLANE: plane_num++; break;
			case FRONT: front_num++; break;
			case SPLIT: split_num++; break;
			case NONE: none_num++; break;
		}
	}
	if(kdroot)
	{
#ifndef STRICT_SEPARATION
		assert(back_num == info_kd.back);
		assert(front_num == info_kd.front);
		assert(plane_num == info_kd.plane);
		assert(split_num == info_kd.split);
#endif
	} else {
		assert(back_num == info_bsp.back);
		assert(front_num == info_bsp.front);
		assert(plane_num == info_bsp.plane);
		assert(split_num == info_bsp.split);
	}
	if(front_num>1000 || back_num>1000)
		printf("[%d|%d/%d|%d]",front_num,plane_num,split_num,back_num);
	back_num += split_num;
	front_num += split_num;

	// alloc front/back/plane
	const FACE **plane=NULL;
	const FACE **front=NULL;
	const FACE **back=NULL;
	if(front_num>0) { front=nALLOC(const FACE*,front_num+1); front[front_num]=NULL; }
	if(kdroot)
	{
		if(back_num+plane_num>0) { back=nALLOC(const FACE*,back_num+plane_num+1); back[back_num+plane_num]=NULL; }
#ifdef SUPPORT_EMPTY_KDNODE
		if(!buildParams.kdHavran)
#endif
		{
			assert(front_num); // v top-level-only kd nesmi byt leaf -> musi byt front i back
			assert(back_num+plane_num);
			assert(front && back);
		}
	} else {
		if(back_num>0) { back=nALLOC(const FACE*,back_num+1); back[back_num]=NULL; }
		if(plane_num>0) { plane=nALLOC(const FACE*,plane_num+1); plane[plane_num]=NULL; }
	}

	// fill front/back/plane
	int plane_id=0;
	int front_id=0;
	int back_id=0;
	for(int i=0;space[i];i++) if(kdroot || space[i]!=bsproot) 
	{
		int side = kdroot ? locate_face_kd((*kdroot)[info_kd.axis],info_kd.axis,bbox,space[i]) : locate_face_bsp(bsproot,space[i],DELTA_INSIDE_PLANE);
		switch(side) 
		{
			case PLANE: if(!kdroot) {plane[plane_id++]=space[i]; break;}
				// intentionally no break for kd, plane goes into back
			case BACK: back[back_id++]=space[i]; break;
			case FRONT: front[front_id++]=space[i]; break;
			case SPLIT: front[front_id++]=space[i]; back[back_id++]=space[i]; break;
		}
	}
	if(!kdroot) plane[plane_id++] = bsproot;
	assert(front_id==front_num);
	if(!kdroot) assert(back_id==back_num);
	if(!kdroot) assert(plane_id==plane_num);
	if(kdroot) assert(back_id+plane_id==back_num+plane_num);

	free(space);

	// create sons
	BBOX bbox_front=*bbox;
	BBOX bbox_back =*bbox;
	if(kdroot) 
	{
		bbox_front.lo[info_kd.axis]=(*kdroot)[info_kd.axis];
		bbox_back .hi[info_kd.axis]=(*kdroot)[info_kd.axis];
		t->leaf = NULL;
		t->axis = info_kd.axis;
		t->kdroot = kdroot;
	} else {
		t->plane = plane;
	}

#ifdef TEST
	printf(kdroot?"KD":"BSP");
	if(plane) for(int i=0;plane[i];i++) printf(" %d",plane[i]->id);
	printf(" Front:");
	if(front) for(int i=0;front[i];i++) printf(" %d",front[i]->id);
	printf(" Back:");
	if(back) for(int i=0;back[i];i++) printf(" %d",back[i]->id);
	printf("\n");
#endif

	// uncomment to disable havran with wrong bbox
	// let it commented, havan is fastest even with wrong bbox
		// havran is not good below bsp node because of wrong bbox
		//unsigned kdHavranOld = buildParams.kdHavran;
		//if(!kdroot) buildParams.kdHavran = 0;

	t->front = front ? create_bsp(front,&bbox_front,kd_allowed) : NULL;
	t->back = back ? create_bsp(back,&bbox_back,kd_allowed) : NULL;

		//buildParams.kdHavran = kdHavranOld;

#ifdef SUPPORT_EMPTY_KDNODE
	if(!buildParams.kdHavran)
#endif
	{
		if(t->kdroot) assert(t->front && t->back); // v top-level-only kd musi byt front i back
	}

	return t;
}

void guess_size_bsp(BSP_TREE *t)
{
	unsigned n = 0;
	if(t->plane) for(unsigned i=0;t->plane[i];i++) n++; 
	if(t->leaf) for(unsigned i=0;t->leaf[i];i++) n++;
	if(t->front) guess_size_bsp(t->front);
	if(t->back) guess_size_bsp(t->back);
	t->bspnodes = ((!t->kdroot && !t->leaf)?1:0) + (t->front?t->front->bspnodes:0) + (t->back?t->back->bspnodes:0);
	t->kdnodes = (t->kdroot?1:0) + (t->front?t->front->kdnodes:0) + (t->back?t->back->kdnodes:0);
	t->kdleaves = (t->leaf?1:0) + (t->front?t->front->kdleaves:0) + (t->back?t->back->kdleaves:0);
	t->faces = n + (t->front?t->front->faces:0) + (t->back?t->back->faces:0);
}

template IBP
bool save_bsp(FILE *f, BSP_TREE *t)
{
	unsigned n = 0; 
	if(t->plane) for(unsigned i=0;t->plane[i];i++) n++; 
	nodes++;
	faces += n;

	unsigned pos1 = ftell(f);

	BspTree node;
	node.bsp.size=-1;
	fwrite(&node,sizeof(node),1,f);

	if(t->kdroot && (t->front || t->back)) // pro prazdny kdnode uz nezapisuje splitValue
	{
		assert(t->axis>=0 && t->axis<=2);
		real splitValue = (&t->kdroot->x)[t->axis];
		fwrite(&splitValue,sizeof(splitValue),1,f);
#ifdef SUPPORT_EMPTY_KDNODE
		if(!buildParams.kdHavran)
#endif
		{
			assert(t->front);
			assert(t->back);
		}
	}
	if(t->leaf)
	{
		assert(t->leaf[0]);
		for(unsigned i=0;t->leaf[i];i++) 
		{
			typename BspTree::_TriInfo tri;
			tri.setGeometry(t->leaf[i]->id,(Vec3*)&t->leaf[i]->vertex[0]->x,(Vec3*)&t->leaf[i]->vertex[1]->x,(Vec3*)&t->leaf[i]->vertex[2]->x);
			assert(tri.getTriangleIndex()==t->leaf[i]->id);
			fwrite(&tri,sizeof(tri),1,f);
			faces++;
		}
	}

	bool transition = false;
	if(BspTree::allows_transition)
	{
		#define TREE_SIZE(tree,nodeSize,triSize) (tree ? tree->bspnodes*nodeSize + tree->kdnodes*(nodeSize+sizeof(real)) + tree->kdleaves*nodeSize + tree->faces*triSize : 0)
		typename first_nonvoid<typename BspTree::_Lo,BspTree>::T smallerBspTree,smallerBspTree2;
		typename first_nonvoid<typename BspTree::_Lo,BspTree>::T::_TriInfo smallerTriInfo;
		//unsigned frontSizeMax = TREE_SIZE(t->front,sizeof(BspTree),sizeof(typename BspTree::_TriInfo));
		unsigned frontSizeMin = TREE_SIZE(t->front,sizeof(smallerBspTree),sizeof(smallerTriInfo));
		//unsigned backSizeMax = TREE_SIZE(t->back,sizeof(BspTree),sizeof(typename BspTree::_TriInfo));
		unsigned backSizeMin = TREE_SIZE(t->back,sizeof(smallerBspTree),sizeof(smallerTriInfo));
		smallerBspTree.bsp.size=frontSizeMin;
		smallerBspTree2.bsp.size=backSizeMin;
		transition = smallerBspTree.bsp.size==frontSizeMin && smallerBspTree2.bsp.size==backSizeMin;
	}

#ifdef SUPPORT_EMPTY_KDNODE
	static BSP_TREE empty = {};
	empty.kdroot = (VERTEX*)1;
	if(t->kdroot && t!=&empty) assert(t->front || t->back);
	if(t->front || t->back)
#endif
	if(transition)
	{
		if(t->front) if(!save_bsp<typename first_nonvoid<typename BspTree::_Lo,BspTree>::T>(f,t->front)) return false;
		if(t->back) if(!save_bsp<typename first_nonvoid<typename BspTree::_Lo,BspTree>::T>(f,t->back)) return false;
		// havran(->empty kdnodes) + transition not implemented
		// luckily we use havran only in FASTEST which uses no transitions
	} else {
		if(t->front) if(!save_bsp IBP2(f,t->front)) return false;
#ifdef SUPPORT_EMPTY_KDNODE
		if(t->kdroot && !t->front) if(!save_bsp IBP2(f,&empty)) return false;
#endif
		if(t->back) if(!save_bsp IBP2(f,t->back)) return false;
#ifdef SUPPORT_EMPTY_KDNODE
		if(t->kdroot && !t->back) if(!save_bsp IBP2(f,&empty)) return false;
#endif
	}
#ifdef SUPPORT_EMPTY_KDNODE
	if(!t->front && !t->back) assert(n || t->leaf || t==&empty); // n=bsp leaf, leaf=kd leaf, empty=havran's kd branch
#else
	if(!t->front && !t->back) assert(n || t->leaf); // n=bsp leaf, leaf=kd leaf
#endif
		

	/*
	// sorting triangles by area (biggest first) should help in some scenes and not hurt in others
	// but surprisingly it's 2% slower on average
	if(!t->kdroot)
	{
		// sort by area
		FACE_Q* tmp = new FACE_Q[n];
		for(unsigned i=n;i--;) 
		{
			tmp[i].f = t->plane[i];
			tmp[i].q = tmp[i].f->getArea();
		}
		if(n>2) qsort(tmp+1,n-1,sizeof(FACE_Q),compare_face_q_desc); // first one defines plane, don't sort it away
		//if(n>1) qsort(tmp,n,sizeof(FACE_Q),compare_face_q_desc);
		// write to file
		for(unsigned i=n;i--;) // worst
	//	for(unsigned i=0;i<n;i++) // best
		{
			typename BspTree::_TriInfo info = tmp[i].f->id;
			if(info!=tmp[i].f->id) {assert(0);return false;}
			fwrite(&info,sizeof(info),1,f);
		}
		delete tmp;
	}*/
	if(!t->kdroot) for(unsigned i=n;i--;)
	{
		typename BspTree::_TriInfo info;
		info.setGeometry(t->plane[i]->id,(Vec3*)&t->plane[i]->vertex[0]->x,(Vec3*)&t->plane[i]->vertex[1]->x,(Vec3*)&t->plane[i]->vertex[2]->x);
		if(info.getTriangleIndex()!=t->plane[i]->id) {assert(0);return false;}
		fwrite(&info,sizeof(info),1,f);
	}

	// write back into empty space
	unsigned pos2 = ftell(f);
	node.bsp.size = pos2-pos1;
	if(node.bsp.size!=pos2-pos1) {assert(0);return false;}
	node.setTransition(transition);
	node.setKd(t->kdroot || t->leaf);
	if(t->kdroot || t->leaf)
	{
		assert(t->axis>=0 && t->axis<=3);
		node.kd.splitAxis = t->axis;
	} else {
		node.bsp.back = t->back?1:0;
		node.bsp.front = t->front?1:0;
	}
	fseek(f,pos1,SEEK_SET);
	fwrite(&node,sizeof(node),1,f);
	fseek(f,pos2,SEEK_SET);
	return true;
}

static const FACE **make_list(OBJECT *o)
{
	const FACE **l=nALLOC(const FACE*,o->face_num+1); int i;
	for(i=0;i<o->face_num;i++) l[i]=o->face+i;
	l[o->face_num]=NULL; return l;
}

BSP_TREE* create_bsp(OBJECT *obj,bool kd_allowed)
{
	BBOX bbox={-1e10,-1e10,-1e10,1e10,1e10,1e10};

	assert(obj->face_num>0); // pozor nastava

	for(int i=0;i<obj->vertex_num;i++) 
	{
		obj->vertex[i].id=i;
		obj->vertex[i].used=0;

		if(!IS_VEC3(obj->vertex[i])) 
			printf("warning: invalid vertex %d\n",i);

		bbox.hi[0]=MAX(bbox.hi[0],obj->vertex[i].x);
		bbox.hi[1]=MAX(bbox.hi[1],obj->vertex[i].y);
		bbox.hi[2]=MAX(bbox.hi[2],obj->vertex[i].z);
		bbox.lo[0]=MIN(bbox.lo[0],obj->vertex[i].x);
		bbox.lo[1]=MIN(bbox.lo[1],obj->vertex[i].y);
		bbox.lo[2]=MIN(bbox.lo[2],obj->vertex[i].z);
	}

	for(int i=0;i<obj->face_num;i++) 
	{
		obj->face[i].fillMinMax();
		obj->face[i].fillNormal();

		obj->vertex[obj->face[i].vertex[0]->id].used++;
		obj->vertex[obj->face[i].vertex[1]->id].used++;
		obj->vertex[obj->face[i].vertex[2]->id].used++;
	}

	// pri 1e-6 se vyskytl triangl, ktery udajne nebyl uvnitr vlastni roviny
	// nicmene pak to vzdy spadlo, oboje mohlo byt nasledek jineho rozkladu
	DELTA_INSIDE_PLANE = bbox.getEdgeSize() * 1e-6f;

	for(int i=0;i<obj->vertex_num;i++)
		if(!obj->vertex[i].used) printf("warning: unused vertex %d\n",i);

	return create_bsp(make_list(obj),&bbox,kd_allowed);
}

template IBP
bool save_bsp(FILE *f, OBJECT *obj, BSP_TREE* bsp)
{
	assert(f);
	nodes=0; faces=0;

	int i=ftell(f);
	guess_size_bsp(bsp);
	bool ok = save_bsp IBP2(f,bsp); 
	assert(faces==bsp->faces);
	int j=ftell(f);
	if(!obj->face_num) printf("\nBSP: No faces.\n"); else
		printf("\nBSP nodes: %d+%d+%d(%1.1f) size: %d(%1.1f)\n",bsp->kdnodes,bsp->bspnodes,bsp->kdleaves,bsp->faces/(float)obj->face_num,j-i,(j-i)/(float)obj->face_num);

	return ok;
}

}; // BspBuilder

template IBP
bool createAndSaveBsp(FILE *f, OBJECT *obj, BuildParams* buildParams)
{
	BspBuilder* builder = new BspBuilder();
	assert(buildParams);
	builder->buildParams = *buildParams;
	BspBuilder::BSP_TREE* bsp = builder->create_bsp(obj,BspTree::allows_kd);
	bool ok = builder->save_bsp IBP2(f, obj, bsp);
	builder->free_node(bsp);
	delete builder;
	return ok;
}

// explicit instantiation
#define INSTANTIATE(BspTree) \
	template bool BspBuilder::save_bsp<BspTree>(FILE *f, BSP_TREE *t);\
	template bool BspBuilder::save_bsp<BspTree>(FILE *f, OBJECT *obj, BSP_TREE* bsp);\
	template bool createAndSaveBsp    <BspTree>(FILE *f, OBJECT *obj, BuildParams* buildParams)

// single-level bsp
INSTANTIATE(BspTree44);

// multi-level bsp
INSTANTIATE( BspTree14);
INSTANTIATE(CBspTree24);
INSTANTIATE(CBspTree44);

INSTANTIATE( BspTree12);
INSTANTIATE(CBspTree22);
INSTANTIATE(CBspTree42);

INSTANTIATE( BspTree11);
INSTANTIATE(CBspTree21);

} // namespace
