#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "bsp.h"

//#define BESTN_N 100
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

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

enum
{
	BEST,
	MEAN,
	BIG,
	BESTN,
};

#define CACHE_SIZE 1000
#define ZERO 0.00001
#define DELTA 0.001
#define MAXL 100
#define PLANE 0
#define FRONT 1
#define BACK -1
#define SPLIT 2

#define PLANE_PRIZE 1
#define SPLIT_PRIZE 150
#define BALANCE_PRIZE 5

#define nALLOC(A,B) (A *)malloc((B)*sizeof(A))
#define ALLOC(A) nALLOC(A,1)

#define SQR(A) ((A)*(A))
#undef ABS
#define ABS(x) (((x)>0)?(x):(-(x)))

int quality,max_skip,nodes,faces,bestN;

BSP_TREE *bsptree;
unsigned bsptree_id;

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

static int locate_vertex_kd(VERTEX *splitVertex, int splitAxis, VERTEX *v)
{
	switch(splitAxis) 
	{
		#define TEST(x) return (v->x>splitVertex->x)?FRONT:((v->x<splitVertex->x)?BACK:PLANE)
		case 0: TEST(x);
		case 1: TEST(y);
		case 2: TEST(z);
		#undef TEST
	}
	assert(0);
	return PLANE;
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

static int locate_face_kd(VERTEX *vertex, int axis, FACE *face)
{
	int i,f=0,b=0,p=0;

	for (i=0;i<3;i++)
		switch (locate_vertex_kd(vertex,axis,face->vertex[i])) 
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

static VERTEX *find_best_root_kd(BBOX *bbox, FACE **list, int *bestaxis)
{
	int i,j,vert,axis; VERTEX *best=NULL; float front_area,back_area,prize,best_prize=0;

	for (axis=0;axis<3;axis++) 
	{
		float cut_peri=(bbox->hi[(axis+1)%3]-bbox->lo[(axis+1)%3]) + (bbox->hi[(axis+2)%3]-bbox->lo[(axis+2)%3]);
		float cut_area=(bbox->hi[(axis+1)%3]-bbox->lo[(axis+1)%3]) * (bbox->hi[(axis+2)%3]-bbox->lo[(axis+2)%3]);

		for (i=0;list[i];i++) for (vert=0;vert<3;vert++) 
		{
			int front=0,back=0,plane=0,split=0;

			for (j=0;list[j];j++)
				switch (locate_face_kd(list[i]->vertex[vert],axis,list[j])) 
				{
					case PLANE:plane++;break;
					case FRONT:front++;break;
					case SPLIT:split++;break;
					case BACK:back++;break;
				}

			//prize=split*SPLIT_PRIZE+ABS(front-(plane+back))*BALANCE_PRIZE;
			#define VERTEX_COMPONENT(vrtx,ax) ((ax==0)?(vrtx->x):((ax==1)?(vrtx->y):(vrtx->z)))
			front_area=(bbox->hi[axis]-VERTEX_COMPONENT(list[i]->vertex[vert],axis))*cut_peri+cut_area;
			back_area =(VERTEX_COMPONENT(list[i]->vertex[vert],axis)-bbox->lo[axis])*cut_peri+cut_area;
			prize=(front+split)*front_area+(split+plane+back)*back_area;

			if (prize<best_prize || best==NULL)
				if (front>0 && plane+back>0) { best_prize=prize; best=list[i]->vertex[vert]; *bestaxis=axis; }

		}
	}

	return best;
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

FACE *find_mean_root(FACE **list)
{
	int i; FACE *best=NULL;
	float dist,min=1e10,x,y,z,px=0,py=0,pz=0; int pn=0;

	for (i=0;list[i];i++) 
	{
		mid_point(list[i],&x,&y,&z);
		px+=x; py+=y; pz+=z; pn++; 
	}

	if (pn<max_skip) return NULL;

	px/=pn; py/=pn; pz/=pn; best=NULL;

	for (i=0;list[i];i++) { dist=dist_point(list[i],px,py,pz);
	if (dist<min || !best) { min=dist; best=list[i]; } }

	return best;
}

FACE *find_big_root(FACE **list)
{
	int i,pn=0; FACE *best=NULL; float size,max=0;

	for (i=0;list[i];i++) 
	{
		pn++;
		size=face_size(list[i]);
		if (size>max || !best) 
		{
			max=size; best=list[i];
		}
	}

	if (pn<max_skip) return NULL;

	return best;
}

struct FACE_Q 
{
        float q;
        FACE *f;
};

static int compare_face_q( const void *p1, const void *p2 )
{
	float f=((FACE_Q *)p2)->q-((FACE_Q *)p1)->q;
	return (f<0)?-1:((f>0)?1:0);
}

FACE *find_bestN_root(FACE **list)
{
	int i,j,prize,best_prize=0; FACE *best=NULL;
	float px=0,py=0,pz=0; int pn=0;
	FACE_Q *tmp;

	for (i=0;list[i];i++) 
	{
		float x,y,z;
		mid_point(list[i],&x,&y,&z);
		px+=x; py+=y; pz+=z; pn++; 
	}

	if (pn<max_skip) return NULL;

	px/=pn; py/=pn; pz/=pn;

	tmp=nALLOC(FACE_Q,i);

	for (i=0;list[i];i++) 
	{
		float dist=dist_point(list[i],px,py,pz);
		float size=face_size(list[i]);
		tmp[i].q=(float)(10*sqrt(size))-dist;
		tmp[i].f=list[i];
	}

	qsort(tmp,pn,sizeof(FACE_Q),compare_face_q);

	for (i=0;i<bestN && i<pn;i++) 
	{ 
		int front=0,back=0,plane=0,split=0;

		for (j=0;list[j];j++)
			switch (locate_face_bsp(tmp[i].f,list[j])) 
			{
				case PLANE:plane++;break;
				case FRONT:front++;break;
				case SPLIT:split++;break;
				case BACK:back++;break;
			}

		prize=split*SPLIT_PRIZE+plane*PLANE_PRIZE+ABS(front-back)*BALANCE_PRIZE;

		if (prize<best_prize || best==NULL) { best_prize=prize; best=tmp[i].f; }

	}

	free(tmp);

	return best;
}

BSP_TREE *create_bsp(FACE **space, BBOX *bbox, bool kd_allowed)
{
	int pn=0;
	for(int i=0;space[i];i++) pn++;

	// alloc node
	BSP_TREE *t=new_bsp();
	memset(t,0,sizeof(BSP_TREE));

	// select splitting plane
	FACE* bsproot = NULL;
	VERTEX* kdroot = NULL; 
	int axis = 3;
	switch(quality) 
	{
		case BIG:bsproot=find_big_root(space);break;
		case MEAN:bsproot=find_mean_root(space);break;
		case BEST:bsproot=find_best_root(space);break;
		case BESTN:
			if(!kd_allowed || pn<400) //!!! 400=temer nikdy kd, 10=temer vzdy kd
			{
				bsproot=find_bestN_root(space);
			} else {
				kdroot=find_best_root_kd(bbox,space,&axis);
				if(!kdroot)
					bsproot=find_bestN_root(space);
			}
			break;
	}

	// leaf -> exit
	if(!bsproot && !kdroot) 
	{
		if(pn>5000) printf("No split in %d faces, texniq=%d(%d) bestN=%d",pn,quality,BESTN,bestN);//!!!
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
		if(!kdroot && space[i]==bsproot) {plane_num++;continue;} // don't insert bsproot
		space[i]->side = kdroot ? locate_face_kd(kdroot,axis,space[i]) : locate_face_bsp(bsproot,space[i]);
		switch(space[i]->side) 
		{
			case BACK: back_num++; break;
			case PLANE: plane_num++; break;
			case FRONT: front_num++; break;
			case SPLIT: back_num++; front_num++; split_num++; break;
		}
	}
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
		bbox_front.lo[axis]=VERTEX_COMPONENT(kdroot,axis);
		bbox_back .hi[axis]=VERTEX_COMPONENT(kdroot,axis);
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

	quality=BESTN;
	//bestN=BESTN_N;
	max_skip=1;

	assert(obj->face_num>0); // pozor nastava

	for (int i=0;i<obj->vertex_num;i++) {

		obj->vertex[i].id=i;
		obj->vertex[i].used=0;

		/*printf("v[%d]: %f %f %f\n",i,obj->vertex[i].x,
		obj->vertex[i].y,
		obj->vertex[i].z);*/

		bbox.hi[0]=MAX(bbox.hi[0],obj->vertex[i].x);
		bbox.hi[1]=MAX(bbox.hi[1],obj->vertex[i].y);
		bbox.hi[2]=MAX(bbox.hi[2],obj->vertex[i].z);
		bbox.lo[0]=MIN(bbox.lo[0],obj->vertex[i].x);
		bbox.lo[1]=MIN(bbox.lo[1],obj->vertex[i].y);
		bbox.lo[2]=MIN(bbox.lo[2],obj->vertex[i].z);
	}

	for (int i=0;i<obj->face_num;i++) {

		create_normal(&obj->face[i]);

		obj->vertex[obj->face[i].vertex[0]->id].used++;
		obj->vertex[obj->face[i].vertex[1]->id].used++;
		obj->vertex[obj->face[i].vertex[2]->id].used++;

		/*printf("f[%d]: %d %d %d\n",i,obj->face[i].vertex[0]->id,
		obj->face[i].vertex[1]->id,
		obj->face[i].vertex[2]->id);*/
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
bool createAndSaveBsp(FILE *f, OBJECT *obj, int effort)
{
	BspBuilder* builder = new BspBuilder();
	builder->bestN = effort;
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
	template bool createAndSaveBsp    <BspTree>(FILE *f, OBJECT *obj, int effort)

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
