#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "bsp.h"

namespace rrIntersect
{

class BspBuilder
{
public:


typedef float VECTOR[3];

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
#define ABS(x) (((x)>0)?(x):(-(x)))

#define WU(A) Unsigned=A; fwrite(&Unsigned,sizeof(unsigned),1,f)

int quality,max_skip,nodes,faces,bestN;

BSP_TREE *bsptree;
unsigned bsptree_id;
KD_TREE *kdtree;
unsigned kdtree_id;

BspBuilder()
{
	bsptree = NULL;
	bsptree_id = CACHE_SIZE;
	kdtree = NULL;
	kdtree_id = CACHE_SIZE;
}

BSP_TREE *new_bsp()
{
	BSP_TREE* oldbsptree=bsptree;
	if (bsptree_id<CACHE_SIZE) return bsptree+bsptree_id++;
	bsptree=nALLOC(BSP_TREE,CACHE_SIZE+1); bsptree[CACHE_SIZE].front=oldbsptree; bsptree_id=1; return bsptree;
}

KD_TREE *new_kd()
{
	if (kdtree_id<CACHE_SIZE) return kdtree+kdtree_id++;
	kdtree=nALLOC(KD_TREE,CACHE_SIZE); kdtree_id=1; return kdtree;
}

static free_bsp(BSP_TREE* t)
{
	free(t->plane);
	if(t->front) free_bsp(t->front);
	if(t->back) free_bsp(t->back);
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
 switch(splitAxis) {
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
     switch (locate_vertex_bsp(plane,face->vertex[i])) {
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
     switch (locate_vertex_kd(vertex,axis,face->vertex[i])) {
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

 for (i=0;list[i];i++) { int front=0,back=0,plane=0,split=0;

     for (j=0;list[j];j++)
         switch (locate_face_bsp(list[i],list[j])) {
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

 for (axis=0;axis<3;axis++) {

     float cut_peri=(bbox->hi[(axis+1)%3]-bbox->lo[(axis+1)%3]) + (bbox->hi[(axis+2)%3]-bbox->lo[(axis+2)%3]);
     float cut_area=(bbox->hi[(axis+1)%3]-bbox->lo[(axis+1)%3]) * (bbox->hi[(axis+2)%3]-bbox->lo[(axis+2)%3]);

     for (i=0;list[i];i++) for (vert=0;vert<3;vert++) {

         int front=0,back=0,plane=0,split=0;

         for (j=0;list[j];j++)
             switch (locate_face_kd(list[i]->vertex[vert],axis,list[j])) {
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
 int i; *x=0; *y=0; *z=0;

 for (i=0;i<3;i++) {
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

 for (i=0;list[i];i++) {
     mid_point(list[i],&x,&y,&z);
     px+=x; py+=y; pz+=z; pn++; }

 if (pn<max_skip) return NULL;

 px/=pn; py/=pn; pz/=pn; best=NULL;

 for (i=0;list[i];i++) { dist=dist_point(list[i],px,py,pz);
     if (dist<min || !best) { min=dist; best=list[i]; } }

 return best;
}

FACE *find_big_root(FACE **list)
{
 int i,pn=0; FACE *best=NULL; float size,max=0;

 for (i=0;list[i];i++) { pn++;
     size=face_size(list[i]);
     if (size>max || !best) {
        max=size; best=list[i];
        }
     }

 if (pn<max_skip) return NULL;

 return best;
}

typedef struct {
        float q;
        FACE *f;
        } FACE_Q;

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

 for (i=0;list[i];i++) {
     float x,y,z;
     mid_point(list[i],&x,&y,&z);
     px+=x; py+=y; pz+=z; pn++; }

 if (pn<max_skip) return NULL;

 px/=pn; py/=pn; pz/=pn;

 tmp=nALLOC(FACE_Q,i);

 for (i=0;list[i];i++) {
     float dist=dist_point(list[i],px,py,pz);
     float size=face_size(list[i]);
     tmp[i].q=(float)(10*sqrt(size))-dist;
     tmp[i].f=list[i];
     }

 qsort(tmp,pn,sizeof(FACE_Q),compare_face_q);

 for (i=0;i<bestN && i<pn;i++) { int front=0,back=0,plane=0,split=0;

     for (j=0;list[j];j++)
         switch (locate_face_bsp(tmp[i].f,list[j])) {
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

BSP_TREE *create_bsp(FACE **space)
{
 BSP_TREE *t=new_bsp();
 int plane_id=0,front_id=0,back_id=0;
 int split_num=0,plane_num=0,front_num=0,back_num=0,i;
 FACE **plane=NULL,**front=NULL,**back=NULL,*root;

 switch (quality) {
  case BIG:root=find_big_root(space);break;
  case MEAN:root=find_mean_root(space);break;
  case BEST:root=find_best_root(space);break;
  case BESTN:root=find_bestN_root(space);break;
  }

 int pn=0;
 for (int i=0;space[i];i++) pn++;
 if(!root && pn>5000) printf("No split in %d faces, texniq=%d(%d) bestN=%d",pn,quality,BESTN,bestN);//!!!

 if (root) plane_num++; else { t->plane=space; t->front=NULL; t->back=NULL; return t; }

 for (i=0;space[i];i++) if (space[i]!=root) {
     space[i]->side=locate_face_bsp(root,space[i]);
     switch (space[i]->side) {
            case BACK: back_num++; break;
            case PLANE: plane_num++; break;
            case FRONT: front_num++; break;
            case SPLIT: back_num++;
                        front_num++;
                        split_num++; break;
            }
      }

 if (back_num>0) { back=nALLOC(FACE*,back_num+1); back[back_num]=NULL; }
 if (front_num>0) { front=nALLOC(FACE*,front_num+1); front[front_num]=NULL; }
 if (plane_num>0) { plane=nALLOC(FACE*,plane_num+1); plane[plane_num]=NULL; }

 for (i=0;space[i];i++)
     if (space[i]!=root) switch (space[i]->side) {
     case BACK: back[back_id++]=space[i]; break;
     case PLANE: plane[plane_id++]=space[i]; break;
     case FRONT: front[front_id++]=space[i]; break;
     case SPLIT: front[front_id++]=space[i];
                 back[back_id++]=space[i]; break;
     }

 if (front_num>500 && back_num>500)
    printf("[%d|%d/%d|%d]",front_num,plane_num,split_num,back_num);

 plane[plane_id++]=root; free(space);

 t->plane=plane;
 t->front=front ? create_bsp(front) : NULL;
 t->back =back  ? create_bsp(back)  : NULL;

 return t;
}

KD_TREE *create_kd(BBOX *bbox, FACE **space)
{
 KD_TREE *t=new_kd();
 int front_id=0,back_id=0;
 int split_num=0,plane_num=0,front_num=0,back_num=0,i;
 FACE **front=NULL,**back=NULL;
 VERTEX *root=NULL; int axis=3;
 BBOX bbox_front,bbox_back;

 switch (quality) {
  case BIG:
  case MEAN:
  case BESTN:
  case BEST:root=find_best_root_kd(bbox,space,&axis);break;
  }

 if (!root) {
    t->leaf =space;
    t->front=NULL;
    t->back =NULL; 
    t->axis =3; 
    t->root =NULL; 
    return t; 
    }

 for (i=0;space[i];i++) {
     space[i]->side=locate_face_kd(root,axis,space[i]);
     switch (space[i]->side) {
            case BACK: back_num++; break;
            case FRONT: front_num++; break;
            case PLANE: plane_num++; break;
            case SPLIT: back_num++;
                        front_num++;
                        split_num++; break;
            }
      }

 if (back_num+plane_num>0) { back=nALLOC(FACE*,back_num+plane_num+1); back[back_num+plane_num]=NULL; }
 if (front_num>0) { front=nALLOC(FACE*,front_num+1); front[front_num]=NULL; }

 for (i=0;space[i];i++)
     switch (space[i]->side) {
     case PLANE:
     case BACK: back[back_id++]=space[i]; break;
     case FRONT: front[front_id++]=space[i]; break;
     case SPLIT: front[front_id++]=space[i];
                 back[back_id++]=space[i]; break;
     }

 free(space);

 if (front_num>500 && back_num+plane_num>500)
    printf("[%d|%d/%d|%d]",front_num,plane_num,split_num,back_num);

 bbox_front=*bbox; bbox_front.lo[axis]=VERTEX_COMPONENT(root,axis);
 bbox_back =*bbox; bbox_back .hi[axis]=VERTEX_COMPONENT(root,axis);

 t->leaf =NULL;
 t->front=front ? create_kd(&bbox_front,front) : NULL;
 t->back =back  ? create_kd(&bbox_back,back)  : NULL;
 t->axis =axis;
 t->root =root;
 return t;
}

typedef struct { unsigned size:30;
                 unsigned front:1;
                 unsigned back:1; } BSP_NODE;

void save_bsp(FILE *f, BSP_TREE *t)
{
 int i,n=0,pos1,pos2; BSP_NODE node; nodes++;

 for (i=0;t->plane[i];i++) n++; faces+=n;

 pos1=ftell(f);

 unsigned Unsigned;
 WU(0); // empty space

 if (t->front) save_bsp(f,t->front);
 if (t->back) save_bsp(f,t->back);

 for (i=n;i--;) { WU(t->plane[i]->id); }

 // write back into empty space
 pos2=ftell(f);
 node.size=pos2-pos1;
 node.back=t->back?1:0;
 node.front=t->front?1:0;
 fseek(f,pos1,SEEK_SET);
 fwrite(&node,sizeof(node),1,f);
 fseek(f,pos2,SEEK_SET);
}

typedef struct { unsigned size:30;
                 unsigned axis:2; } KD_NODE;

void save_kd(FILE *f, KD_TREE *t)
{
 int i,n=0,pos1,pos2; KD_NODE node; nodes++;

 assert(t);

 for (i=0;t->leaf && t->leaf[i];i++) n++; faces+=n;

 pos1=ftell(f);

 unsigned Unsigned;
 WU(0); // empty space

 if (t->axis==3) {
    // save leaf
    for (i=n;i--;) { WU(t->leaf[i]->id); }
 }  else {
    // save non-leaf
    WU(t->root->id);
    save_kd(f,t->front);
    save_kd(f,t->back);
 }

 // write back into empty space
 pos2=ftell(f);
 node.size=pos2-pos1;
 node.axis=t->axis;
 fseek(f,pos1,SEEK_SET);
 fwrite(&node,sizeof(node),1,f);
 fseek(f,pos2,SEEK_SET);
}

static FACE **make_list(OBJECT *o)
{
 FACE **l=nALLOC(FACE*,o->face_num+1); int i;
 for (i=0;i<o->face_num;i++) l[i]=o->face+i;
 l[o->face_num]=NULL; return l;
}

void createAndSaveBsp(FILE *f, OBJECT *obj)
{
 int i,j; BBOX bbox={-1e10,-1e10,-1e10,1e10,1e10,1e10};

 quality=BESTN;
 bestN=100;
 max_skip=1;

 assert(f);
 assert(obj->face_num>0); // pozor nastava

 for (i=0;i<obj->vertex_num;i++) {

     obj->vertex[i].id=i;
     obj->vertex[i].used=0;

     /*printf("v[%d]: %f %f %f\n",i,obj->vertex[i].x,
                                   obj->vertex[i].y,
                                   obj->vertex[i].z);*/

     #define MAX(a,b) (((a)>(b))?(a):(b))
     #define MIN(a,b) (((a)<(b))?(a):(b))
     bbox.hi[0]=MAX(bbox.hi[0],obj->vertex[i].x);
     bbox.hi[1]=MAX(bbox.hi[1],obj->vertex[i].y);
     bbox.hi[2]=MAX(bbox.hi[2],obj->vertex[i].z);
     bbox.lo[0]=MIN(bbox.lo[0],obj->vertex[i].x);
     bbox.lo[1]=MIN(bbox.lo[1],obj->vertex[i].y);
     bbox.lo[2]=MIN(bbox.lo[2],obj->vertex[i].z);
     }

 for (i=0;i<obj->face_num;i++) {

     create_normal(&obj->face[i]);

     obj->vertex[obj->face[i].vertex[0]->id].used++;
     obj->vertex[obj->face[i].vertex[1]->id].used++;
     obj->vertex[obj->face[i].vertex[2]->id].used++;

     /*printf("f[%d]: %d %d %d\n",i,obj->face[i].vertex[0]->id,
                                  obj->face[i].vertex[1]->id,
                                  obj->face[i].vertex[2]->id);*/
     }

 for (i=0;i<obj->vertex_num;i++)
     if (!obj->vertex[i].used) printf("unused_vertex: %d\n",i);

 BSP_TREE* bsp=create_bsp(make_list(obj));
 nodes=0; faces=0;
 i=ftell(f);
 save_bsp(f,bsp); 
 j=ftell(f);
 if(!obj->face_num) printf("\nBSP: No faces.\n"); else
 printf("\nBSP nodes: %d(%d) size: %d(%d)\n",nodes,faces/obj->face_num,j-i,(j-i)/obj->face_num);
 free_bsp(bsp);
/*
 KD_TREE* kd=create_kd(&bbox,make_list(obj));
 nodes=0; faces=0;
 save_kd(f,kd); 
 i=ftell(f);
 if(!obj->face_num) printf("\nKD: No faces.\n"); else
 printf("\nKD nodes: %d(%d) size: %d(%d)\n",nodes,faces/obj->face_num,i-j,(i-j)/obj->face_num);*/
}

}; // BspBuilder

void createAndSaveBsp(FILE *f, OBJECT *obj)
{
	BspBuilder* builder = new BspBuilder();
	builder->createAndSaveBsp(f, obj);
	delete builder;
}

} // namespace
