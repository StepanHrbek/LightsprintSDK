#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "bsp.h"

//#define CHECK_KD // slow checks of kd build correctness

#define PLANE_PRIZE   (buildParams.prizePlane)   // 1
#define SPLIT_PRIZE   (buildParams.prizeSplit)   // 150
#define BALANCE_PRIZE (buildParams.prizeBalance) // 5
/*
worst situations
 worst balance (0 vs faces) has prize faces*BALLANCE_PRIZE -> use bsp or kd_leaf
 split of all faces is equally bad and has prize faces*SPLIT_PRIZE -> use kd_leaf

*/

//#define BESTN_N 100
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX3(a,b,c) MAX(a,MAX(b,c))
#define MIN3(a,b,c) MIN(a,MIN(b,c))


namespace rrIntersect
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


typedef float VECTOR[3];

struct BBOX
{
	float hi[3];
	float lo[3];
};

struct BSP_TREE 
{
	BSP_TREE *front;
	BSP_TREE *back;
	unsigned kdnodes;// nodes in whole tree
	unsigned bspnodes;// nodes in whole tree
	unsigned faces;// face instances in whole tree
	// bsp
	FACE **plane;
	// kd
	VERTEX *kdroot;
	int axis;
	FACE **leaf;
};

#define CACHE_SIZE 1000
#define ZERO 0.00001
#define DELTA 0.001
#define MAXL 100
#define PLANE 0
#define FRONT 1
#define BACK -1
#define SPLIT 2

#define nALLOC(A,B) (A *)malloc((B)*sizeof(A))
#define ALLOC(A) nALLOC(A,1)

#define SQR(A) ((A)*(A))
#undef  ABS
#define ABS(x) (((x)>0)?(x):(-(x)))

unsigned    max_skip,nodes,faces;
BSP_TREE*   bsptree;
unsigned    bsptree_id;
BuildParams buildParams;

BspBuilder()
{
	bsptree = NULL;
	bsptree_id = CACHE_SIZE;
}

BSP_TREE *new_bsp()
{
	BSP_TREE* oldbsptree=bsptree;
	if (bsptree_id<CACHE_SIZE) return bsptree+bsptree_id++;
	bsptree=nALLOC(BSP_TREE,CACHE_SIZE+1); bsptree[CACHE_SIZE].front=oldbsptree; bsptree_id=1; return bsptree;
}

static void free_bsp(BSP_TREE* t)
{
	free(t->plane);
	if(t->front) free_bsp(t->front);
	if(t->back) free_bsp(t->back);
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

static void cross_product(VECTOR res, VECTOR a, VECTOR b)
{
	res[0]=b[1]*a[2]-a[1]*b[2];
	res[1]=b[2]*a[0]-a[2]*b[0];
	res[2]=b[0]*a[1]-a[0]*b[1];
}

static void create_normal(FACE *f)
{
	VECTOR u,v,n; float l;

	u[0]=f->vertex[1]->x-f->vertex[0]->x;
	u[1]=f->vertex[1]->y-f->vertex[0]->y;
	u[2]=f->vertex[1]->z-f->vertex[0]->z;

	v[0]=f->vertex[2]->x-f->vertex[0]->x;
	v[1]=f->vertex[2]->y-f->vertex[0]->y;
	v[2]=f->vertex[2]->z-f->vertex[0]->z;

	cross_product(n,v,u);

	l=(float)sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);

	if (l>ZERO) { n[0]/=l; n[1]/=l; n[2]/=l; }
	else { n[0]=0; n[1]=0; n[2]=0; }

	f->normal.a=n[0];
	f->normal.b=n[1];
	f->normal.c=n[2];
	f->normal.d=-(n[0]*f->vertex[0]->x+
		n[1]*f->vertex[0]->y+
		n[2]*f->vertex[0]->z);
}

static int locate_vertex_bsp(FACE *f, VERTEX *v)
{
	float r=f->normal.a*v->x+f->normal.b*v->y+f->normal.c*v->z+f->normal.d;
	if (ABS(r)<DELTA) return PLANE; if (r>0) return FRONT; return BACK;
}

static int locate_vertex_kd(float splitValue, int splitAxis, VERTEX *v)
{
	return ((*v)[splitAxis]>splitValue) ? FRONT : ( ((*v)[splitAxis]<splitValue) ? BACK : PLANE );
}

static int normals_match(FACE *plane, FACE *face)
{
	return
		fabs(plane->normal.a+face->normal.a)+fabs(plane->normal.b+face->normal.b)+fabs(plane->normal.c+face->normal.c)<0.01 ||
		fabs(plane->normal.a-face->normal.a)+fabs(plane->normal.b-face->normal.b)+fabs(plane->normal.c-face->normal.c)<0.01;
}

static int locate_face_bsp(FACE *plane, FACE *face)
{
	int i,f=0,b=0,p=0;

	for (i=0;i<3;i++)
		switch (locate_vertex_bsp(plane,face->vertex[i])) 
		{
			case BACK:b++;break;
			case FRONT:f++;break;
			case PLANE:f++;b++;p++;break;
		}

	if (p==3 && normals_match(plane,face)) return PLANE;
	if (f==3) return FRONT;
	if (b==3) return BACK;
	return SPLIT;
}

static int locate_face_kd(float splitValue, int axis, FACE *face)
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
	return SPLIT;
}
/*
static FACE *find_best_root(FACE **list)
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

		prize=split*SPLIT_PRIZE+plane*PLANE_PRIZE+ABS(front-back)*BALANCE_PRIZE;

		if (prize<best_prize || best==NULL) { best_prize=prize; best=list[i]; }
	}

	return best;
}
*/

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
};

VERTEX *find_best_root_kd(BBOX *bbox, FACE **list, int* bestaxis, ROOT_INFO* bestinfo)
{
	// prepare sortarrays
	unsigned faces=0;
	for(int i=0;list[i];i++) faces++;
	assert(faces);
	if(!faces) return NULL;
	FACE** minx = new FACE*[faces];
	FACE** maxx = new FACE*[faces];
	memcpy(minx,list,sizeof(FACE*)*faces);
	memcpy(maxx,list,sizeof(FACE*)*faces);

	unsigned best_prize = UINT_MAX;
	unsigned best_axis = 0;
	float best_value = 0;
	FACE* best_face = NULL;

	for (unsigned axis=0;axis<3;axis++) 
	{
		// sort sortarrays
		qsort(minx,faces,sizeof(FACE*),(axis==0)?compare_face_minx_asc:((axis==1)?compare_face_miny_asc:compare_face_minz_asc));
		qsort(maxx,faces,sizeof(FACE*),(axis==0)?compare_face_maxx_asc:((axis==1)?compare_face_maxy_asc:compare_face_maxz_asc));

		// select root
		unsigned minxi = 0;
		unsigned maxxi = 0;
		while(1)
		{
			// skip faces in plane minx[minxi].min[axis]
			unsigned plane = 0;
			while(maxxi<faces && minx[minxi]->min[axis] >= maxx[maxxi]->max[axis]) 
			{
				if(minx[minxi]->min[axis]==maxx[maxxi]->max[axis] && 
					maxx[maxxi]->min[axis]==maxx[maxxi]->max[axis]) plane++;
				maxxi++;
			}

			// calculate prize of split plane at minx[minxi].min[axis]
			unsigned back = maxxi-plane;	// pocet facu nalevo od rezu v minx[minxi].q
			unsigned front = faces-minxi-plane; // pocet facu napravo od rezu v minx[minxi].q
			unsigned split = minxi-back; // pocet facu preseklych rezem v minx[minxi].q
			unsigned prize = 1 + split*SPLIT_PRIZE + ABS((int)front-(int)(back+plane))*BALANCE_PRIZE;


			if(prize<best_prize)
			{
				best_prize = prize;
				best_face = minx[minxi];
				best_axis = axis;
				best_value = minx[minxi]->min[axis];
				bestinfo->back = back;
				bestinfo->front = front;
				bestinfo->plane = plane;
				bestinfo->split = split;
				bestinfo->prize = prize;
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

	FACE* f = best_face;
	*bestaxis = best_axis;
	VERTEX* result;
	if(f->min[best_axis]==(*f->vertex[0])[best_axis]) result = f->vertex[0]; else
	if(f->min[best_axis]==(*f->vertex[1])[best_axis]) result = f->vertex[1]; else
	{assert(f->min[best_axis]==(*f->vertex[2])[best_axis]);	result = f->vertex[2];}


	return result;
}

static float face_size(FACE *f)
{
	VECTOR u,v,n;

	u[0]=f->vertex[1]->x-f->vertex[0]->x;
	u[1]=f->vertex[1]->y-f->vertex[0]->y;
	u[2]=f->vertex[1]->z-f->vertex[0]->z;

	v[0]=f->vertex[2]->x-f->vertex[0]->x;
	v[1]=f->vertex[2]->y-f->vertex[0]->y;
	v[2]=f->vertex[2]->z-f->vertex[0]->z;

	cross_product(n,v,u);

	return n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
}

static void mid_point(FACE *f, float *x, float *y, float *z)
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

static float dist_point(FACE *f, float x, float y, float z)
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
	FACE *f;
};

static int compare_face_q_desc( const void *p1, const void *p2 )
{
	float f=((FACE_Q *)p2)->q-((FACE_Q *)p1)->q;
	return (f<0)?-1:((f>0)?1:0);
}

FACE *find_best_root_bsp(FACE **list, ROOT_INFO* bestinfo)
{
	int best_prize=0; FACE *best=NULL;
	float px=0,py=0,pz=0;
	unsigned pn=0;
	FACE_Q *tmp;

	for(int i=0;list[i];i++) 
	{
		float x,y,z;
		mid_point(list[i],&x,&y,&z);
		px+=x; py+=y; pz+=z; pn++; 
	}

	if(pn<max_skip) return NULL;

	px/=pn; py/=pn; pz/=pn;

	tmp=nALLOC(FACE_Q,pn);

	for(int i=0;list[i];i++) 
	{
		float dist=dist_point(list[i],px,py,pz);
		float size=face_size(list[i]);
		tmp[i].q=(float)(10*sqrt(size))-dist;
		tmp[i].f=list[i];
	}

	qsort(tmp,pn,sizeof(FACE_Q),compare_face_q_desc);

	for(unsigned i=0;i<buildParams.bspBestN && i<pn;i++) 
	{ 
		int front=0,back=0,plane=0,split=0;

		for(int j=0;list[j];j++)
			switch(locate_face_bsp(tmp[i].f,list[j])) 
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
		}

	}

	free(tmp);

	return best;
}

BSP_TREE *create_bsp(FACE **space, BBOX *bbox, bool kd_allowed)
{
	unsigned pn=0;
	for(int i=0;space[i];i++) pn++;

	// alloc node
	BSP_TREE *t=new_bsp();
	memset(t,0,sizeof(BSP_TREE));

	// select splitting plane
	FACE* bsproot = NULL;
	VERTEX* kdroot = NULL; 
	int axis = 3;
	ROOT_INFO info_bsp, info_kd;
	if(!kd_allowed || pn<buildParams.kdMinFacesInTree)
	{
		bsproot=find_best_root_bsp(space,&info_bsp);
	} else {
		kdroot = find_best_root_kd(bbox,space,&axis,&info_kd);
		if(pn<buildParams.bspMaxFacesInTree || !kdroot || info_kd.front==0 || info_kd.back+info_kd.plane==0)
		{
			bsproot = find_best_root_bsp(space,&info_bsp);
			if(kdroot && info_kd.prize<info_bsp.prize+pn && info_kd.front && info_kd.back+info_kd.plane) 
				bsproot=NULL; 
			else kdroot=NULL;
		}
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

	// calculate sizes of front/back/plane
	int split_num=0;
	int plane_num=0;
	int front_num=0;
	int back_num=0;
	for(int i=0;space[i];i++)
	{
		if(!kdroot && space[i]==bsproot) {plane_num++;continue;} // insert bsproot into plane
		space[i]->side = kdroot ? locate_face_kd((*kdroot)[axis],axis,space[i]) : locate_face_bsp(bsproot,space[i]);
		switch(space[i]->side) 
		{
			case BACK: back_num++; break;
			case PLANE: plane_num++; break;
			case FRONT: front_num++; break;
			case SPLIT: split_num++; break;
		}
	}
	if(kdroot)
	{
		assert(back_num == info_kd.back);
		assert(front_num == info_kd.front);
		assert(plane_num == info_kd.plane);
		assert(split_num == info_kd.split);
	} else {
		assert(back_num == info_bsp.back);
		assert(front_num == info_bsp.front);
		assert(plane_num == info_bsp.plane);
		assert(split_num == info_bsp.split);
	}
	back_num += split_num;
	front_num += split_num;
	if(front_num>500 && back_num>500)
		printf("[%d|%d/%d|%d]",front_num,plane_num,split_num,back_num);

	// alloc front/back/plane
	FACE **plane=NULL;
	FACE **front=NULL;
	FACE **back=NULL;
	if(front_num>0) { front=nALLOC(FACE*,front_num+1); front[front_num]=NULL; }
	if(kdroot)
	{
		if(back_num+plane_num>0) { back=nALLOC(FACE*,back_num+plane_num+1); back[back_num+plane_num]=NULL; }
		assert(front_num); // v top-level-only kd nesmi byt leaf -> musi byt front i back
		assert(back_num+plane_num);
		assert(front && back);
	} else {
		if(back_num>0) { back=nALLOC(FACE*,back_num+1); back[back_num]=NULL; }
		if(plane_num>0) { plane=nALLOC(FACE*,plane_num+1); plane[plane_num]=NULL; }
	}

	// fill front/back/plane
	int plane_id=0;
	int front_id=0;
	int back_id=0;
	for(int i=0;space[i];i++) if(kdroot || space[i]!=bsproot) 
		switch(space[i]->side) 
		{
			case PLANE: if(!kdroot) {plane[plane_id++]=space[i]; break;}
				// intentionally no break for kd, plane goes into back
			case BACK: back[back_id++]=space[i]; break;
			case FRONT: front[front_id++]=space[i]; break;
			case SPLIT: front[front_id++]=space[i]; back[back_id++]=space[i]; break;
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
		bbox_front.lo[axis]=(*kdroot)[axis];
		bbox_back .hi[axis]=(*kdroot)[axis];
		t->leaf = NULL;
		t->axis = axis;
		t->kdroot = kdroot;
	} else {
		t->plane = plane;
	}
	t->front = front ? create_bsp(front,bbox,kd_allowed) : NULL;
	t->back = back ? create_bsp(back,bbox,kd_allowed) : NULL;

	if(t->kdroot) assert(t->front && t->back); // v top-level-only kd musi byt front i back

	return t;
}

void guess_size_bsp(BSP_TREE *t)
{
	unsigned n = 0;
	if(t->plane) for(unsigned i=0;t->plane[i];i++) n++; 
	if(t->front) guess_size_bsp(t->front);
	if(t->back) guess_size_bsp(t->back);
	t->bspnodes = (t->kdroot?0:1) + (t->front?t->front->bspnodes:0) + (t->back?t->back->bspnodes:0);
	t->kdnodes = (t->kdroot?1:0) + (t->front?t->front->kdnodes:0) + (t->back?t->back->kdnodes:0);
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

	if(t->kdroot)
	{
		assert(t->axis>=0 && t->axis<=2);
		real splitValue = (&t->kdroot->x)[t->axis];
		fwrite(&splitValue,sizeof(splitValue),1,f);
		assert(t->front);
		assert(t->back);
	}

	bool transition = false;
	if(BspTree::allows_transition)
	{
		#define TREE_SIZE(tree,nodeSize,triSize) (tree ? tree->bspnodes*nodeSize + tree->kdnodes*(nodeSize+sizeof(real)) + tree->faces*triSize : 0)
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

	if(transition)
	{
		if(t->front) if(!save_bsp<typename first_nonvoid<typename BspTree::_Lo,BspTree>::T>(f,t->front)) return false;
		if(t->back) if(!save_bsp<typename first_nonvoid<typename BspTree::_Lo,BspTree>::T>(f,t->back)) return false;
	} else {
		if(t->front) if(!save_bsp IBP2(f,t->front)) return false;
		if(t->back) if(!save_bsp IBP2(f,t->back)) return false;
	}

	if(!t->front && !t->back) assert(n);

	if(!t->kdroot) for(unsigned i=n;i--;)
	{
		typename BspTree::_TriInfo info = t->plane[i]->id;
		if(info!=t->plane[i]->id) {assert(0);return false;}
		fwrite(&info,sizeof(info),1,f);
	}

	// write back into empty space
	unsigned pos2 = ftell(f);
	node.bsp.size = pos2-pos1;
	if(node.bsp.size!=pos2-pos1) {assert(0);return false;}
	node.setTransition(transition);
	node.setKd(t->kdroot!=NULL);
	if(t->kdroot)
	{
		assert(t->axis>=0 && t->axis<=2);
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

static FACE **make_list(OBJECT *o)
{
	FACE **l=nALLOC(FACE*,o->face_num+1); int i;
	for (i=0;i<o->face_num;i++) l[i]=o->face+i;
	l[o->face_num]=NULL; return l;
}

BSP_TREE* create_bsp(OBJECT *obj,bool kd_allowed)
{
	BBOX bbox={-1e10,-1e10,-1e10,1e10,1e10,1e10};

	max_skip=1;

	assert(obj->face_num>0); // pozor nastava

	for(int i=0;i<obj->vertex_num;i++) 
	{
		obj->vertex[i].id=i;
		obj->vertex[i].used=0;

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

		create_normal(&obj->face[i]);

		obj->vertex[obj->face[i].vertex[0]->id].used++;
		obj->vertex[obj->face[i].vertex[1]->id].used++;
		obj->vertex[obj->face[i].vertex[2]->id].used++;
	}

	for (int i=0;i<obj->vertex_num;i++)
		if (!obj->vertex[i].used) printf("unused_vertex: %d\n",i);

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
		printf("\nBSP nodes: %d+%d(%1.1f) size: %d(%1.1f)\n",bsp->kdnodes,bsp->bspnodes,bsp->faces/(float)obj->face_num,j-i,(j-i)/(float)obj->face_num);

	return ok;
}

}; // BspBuilder

template IBP
bool createAndSaveBsp(FILE *f, OBJECT *obj, BuildParams* buildParams)
{
	BspBuilder* builder = new BspBuilder();
	builder->buildParams = *buildParams;
	BspBuilder::BSP_TREE* bsp = builder->create_bsp(obj,BspTree::allows_kd);
	bool ok = builder->save_bsp IBP2(f, obj, bsp);
	builder->free_bsp(bsp);
	delete builder;
	return ok;
}

// explicit instantiation
#define INSTANTIATE(BspTree) \
	template bool BspBuilder::save_bsp<BspTree>(FILE *f, BSP_TREE *t);\
	template bool BspBuilder::save_bsp<BspTree>(FILE *f, OBJECT *obj, BSP_TREE* bsp);\
	template bool createAndSaveBsp    <BspTree>(FILE *f, OBJECT *obj, BuildParams* buildParams)

// single-level bsp
INSTANTIATE(BspTree21);
INSTANTIATE(BspTree22);
INSTANTIATE(BspTree42);
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
INSTANTIATE(CBspTree41);

} // namespace
