#include <stdio.h>
#include <math.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include "cnv2bsp.h"

#define SIGNATURE "BSP\n"
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

#define SQR(x) ((x)*(x))
#define ABS(x) (((x)>0)?(x):(-(x)))

#define WB(A) Byte=A; fwrite(&Byte,sizeof(byte),1,f)
#define WW(A) Word=A; fwrite(&Word,sizeof(word),1,f)
#define WF(A) Float=A; fwrite(&Float,sizeof(float),1,f)
#define WI(A) Integer=A; fwrite(&Integer,sizeof(int),1,f)
#define WU(A) Unsigned=A; fwrite(&Unsigned,sizeof(unsigned),1,f)

static byte Byte;
static word Word;
static float Float;
static int Integer;
static unsigned Unsigned;

static int quality,max_skip,nodes,faces,rooted,bestN;

static WORLD *world;
static char *name;

static BSP_TREE *tree=NULL;
static unsigned tree_id=CACHE_SIZE;

static inline BSP_TREE *get_Tree()
{
 if (tree_id<CACHE_SIZE) return tree+tree_id++;
 tree=nALLOC(BSP_TREE,CACHE_SIZE); tree_id=1; return tree;
}

static inline void cross_product(VECTOR res, VECTOR a, VECTOR b)
{
 res[0]=b[1]*a[2]-a[1]*b[2];
 res[1]=b[2]*a[0]-a[2]*b[0];
 res[2]=b[0]*a[1]-a[0]*b[1];
}

static inline void create_normal(FACE *f)
{
 VECTOR u,v,n; float l;

 u[0]=f->vertex[1]->x-f->vertex[0]->x;
 u[1]=f->vertex[1]->y-f->vertex[0]->y;
 u[2]=f->vertex[1]->z-f->vertex[0]->z;

 v[0]=f->vertex[2]->x-f->vertex[0]->x;
 v[1]=f->vertex[2]->y-f->vertex[0]->y;
 v[2]=f->vertex[2]->z-f->vertex[0]->z;

 cross_product(n,v,u);

 l=sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);

 if (l>ZERO) { n[0]/=l; n[1]/=l; n[2]/=l; }
        else { n[0]=0; n[1]=0; n[2]=0; }

 f->normal.a=n[0];
 f->normal.b=n[1];
 f->normal.c=n[2];
 f->normal.d=-(n[0]*f->vertex[0]->x+
               n[1]*f->vertex[0]->y+
               n[2]*f->vertex[0]->z);
}

static inline int locate_vertex(FACE *f, VERTEX *v)
{
 float r=f->normal.a*v->x+f->normal.b*v->y+f->normal.c*v->z+f->normal.d;
 if (ABS(r)<DELTA) return PLANE; if (r>0) return FRONT; return BACK;
}

int normals_match(FACE *plane, FACE *face)
{
 return
  fabs(plane->normal.a+face->normal.a)+fabs(plane->normal.b+face->normal.b)+fabs(plane->normal.c+face->normal.c)<0.01 ||
  fabs(plane->normal.a-face->normal.a)+fabs(plane->normal.b-face->normal.b)+fabs(plane->normal.c-face->normal.c)<0.01;
}

static inline int locate_face(FACE *plane, FACE *face)
{
 int i,f=0,b=0,p=0;

 for (i=0;i<3;i++)
     switch (locate_vertex(plane,face->vertex[i])) {
       case BACK:b++;break;
       case FRONT:f++;break;
       case PLANE:f++;b++;p++;break;
       }

 if (p==3 && normals_match(plane,face)) return PLANE;
 if (f==3) return FRONT;
 if (b==3) return BACK;
 return SPLIT;
}

static inline FACE *find_best_root(FACE **list)
{
 int i,j,prize,best_prize=0; FACE *best=NULL;

 for (i=0;list[i];i++) { int front=0,back=0,plane=0,split=0;

     for (j=0;list[j];j++)
         switch (locate_face(list[i],list[j])) {
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

static inline float face_size(FACE *f)
{
 VECTOR u,v,n; float l;

 u[0]=f->vertex[1]->x-f->vertex[0]->x;
 u[1]=f->vertex[1]->y-f->vertex[0]->y;
 u[2]=f->vertex[1]->z-f->vertex[0]->z;

 v[0]=f->vertex[2]->x-f->vertex[0]->x;
 v[1]=f->vertex[2]->y-f->vertex[0]->y;
 v[2]=f->vertex[2]->z-f->vertex[0]->z;

 cross_product(n,v,u);

 return n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
}

static inline void mid_point(FACE *f, float *x, float *y, float *z)
{
 int i; *x=0; *y=0; *z=0;

 for (i=0;i<3;i++) {
     *x+=f->vertex[i]->x;
     *y+=f->vertex[i]->y;
     *z+=f->vertex[i]->z;
     }

 *x/=3; *y/=3; *z/=3;
}

static inline float dist_point(FACE *f, float x, float y, float z)
{
 float dist=0; int i;

 for (i=0;i<3;i++)
     dist+=SQR(x-f->vertex[i]->x)+
           SQR(y-f->vertex[i]->y)+
           SQR(z-f->vertex[i]->z);

 return dist/3;
}

static inline FACE *find_mean_root(FACE **list)
{
 int i; FACE *best=NULL;
 float dist,min,px=0,py=0,pz=0,pn=0,x,y,z;

 for (i=0;list[i];i++) {
     mid_point(list[i],&x,&y,&z);
     px+=x; py+=y; pz+=z; pn++; }

 if (pn<max_skip) return NULL;

 px/=pn; py/=pn; pz/=pn; best=NULL;

 for (i=0;list[i];i++) { dist=dist_point(list[i],px,py,pz);
     if (dist<min || !best) { min=dist; best=list[i]; } }

 return best;
}

static inline FACE *find_big_root(FACE **list)
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

int compare_face_q( const void *p1, const void *p2 )
{
 return ((FACE_Q *)p2)->q-((FACE_Q *)p1)->q;
}

static inline FACE *find_bestN_root(FACE **list)
{
 int i,j,prize,best_prize=0; FACE *best=NULL;
 float px=0,py=0,pz=0,pn=0;
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
     tmp[i].q=10*sqrt(size)-dist;
     tmp[i].f=list[i];
     }

 qsort(tmp,pn,sizeof(FACE_Q),compare_face_q);

 for (i=0;i<bestN && i<pn;i++) { int front=0,back=0,plane=0,split=0;

     for (j=0;list[j];j++)
         switch (locate_face(tmp[i].f,list[j])) {
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

static BSP_TREE *create_bsp(FACE **space)
{
 BSP_TREE *t=get_Tree();
 int plane_id=0,front_id=0,back_id=0;
 int split_num=0,plane_num=0,front_num=0,back_num=0,i;
 FACE **plane=NULL,**front=NULL,**back=NULL,*root;

 switch (quality) {
  case BIG:root=find_big_root(space);break;
  case MEAN:root=find_mean_root(space);break;
  case BEST:root=find_best_root(space);break;
  case BESTN:root=find_bestN_root(space);break;
  }

 if (root) plane_num++; else { t->plane=space; t->front=NULL; t->back=NULL; return t; }

 for (i=0;space[i];i++) if (space[i]!=root) {
     space[i]->side=locate_face(root,space[i]);
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

typedef struct { unsigned size:30;
                 unsigned front:1;
                 unsigned back:1; } BSP_NODE;

static void _save_bsp(FILE *f, BSP_TREE *t)
{
 int i,n=0,pos1,pos2; BSP_NODE node; nodes++;

 for (i=0;t->plane[i];i++) n++; faces+=n;

 pos1=ftell(f);

 WU(0); // empty space

 if (t->front) _save_bsp(f,t->front);
 if (t->back) _save_bsp(f,t->back);

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

static void save_bsp(FILE *f, BSP_TREE *t)
{
 int i,n=0; nodes++;

 for (i=0;t->plane[i];i++) n++; faces+=n;

 WU(n);

 for (i=n;i--;) { WU(t->plane[i]->id); }

 WB(t->front ? 1 : 0);
 WB(t->back ? 1 : 0);

 if (t->front) save_bsp(f,t->front);
 if (t->back) save_bsp(f,t->back);
}

FACE **make_list(OBJECT *o)
{
 FACE **l=nALLOC(FACE*,o->face_num+1); int i;
 for (i=0;i<o->face_num;i++) l[i]=o->face+i;
 l[o->face_num]=NULL; return l;
}

static void invert_matrix(MATRIX s, MATRIX d)
{
 int i,j; float det;

 d[0][0]= s[1][1]*s[2][2]-s[1][2]*s[2][1];
 d[0][1]=-s[0][1]*s[2][2]+s[0][2]*s[2][1];
 d[0][2]= s[0][1]*s[1][2]-s[0][2]*s[1][1];

 det=d[0][0]*s[0][0]+d[0][1]*s[1][0]+d[0][2]*s[2][0];

 d[1][0]=-s[1][0]*s[2][2]+s[1][2]*s[2][0];
 d[1][1]= s[0][0]*s[2][2]-s[0][2]*s[2][0];
 d[1][2]=-s[0][0]*s[1][2]+s[0][2]*s[1][0];

 d[2][0]= s[1][0]*s[2][1]-s[1][1]*s[2][0];
 d[2][1]=-s[0][0]*s[2][1]+s[0][1]*s[2][0];
 d[2][2]= s[0][0]*s[1][1]-s[0][1]*s[1][0];

 for (j=0;j<3;j++) for (i=0;i<3;i++) d[j][i]/=det;

 d[3][0]=-s[3][0]*d[0][0]-s[3][1]*d[1][0]-s[3][2]*d[2][0];
 d[3][1]=-s[3][0]*d[0][1]-s[3][1]*d[1][1]-s[3][2]*d[2][1];
 d[3][2]=-s[3][0]*d[0][2]-s[3][1]*d[1][2]-s[3][2]*d[2][2];

 d[0][3]=0;
 d[1][3]=0;
 d[2][3]=0;
 d[3][3]=1;
}

static void save_object(FILE *f, OBJECT *obj)
{
 int i; MATRIX om,im;
 
 WB(strlen(obj->name));

 fwrite(obj->name,strlen(obj->name),1,f);
 fwrite(obj->matrix,sizeof(MATRIX),1,f);

 //printf("obj[%s] ------\n",obj_name);

 memcpy(om,obj->matrix,sizeof(MATRIX));

 om[3][0]=0; om[3][1]=0; om[3][2]=0;

 invert_matrix(om,im);

 WU(obj->id); WI(obj->parent);

 WU(obj->pos_num);

 for (i=0;i<obj->pos_num;i++) {
     WF(obj->pt[i]);
     WF(obj->px[i]);
     WF(obj->py[i]);
     WF(obj->pz[i]);
     }

 WU(obj->rot_num);

 for (i=0;i<obj->rot_num;i++) {
     WF(obj->qt[i]);
     WF(obj->qw[i]);
     WF(obj->qx[i]);
     WF(obj->qy[i]);
     WF(obj->qz[i]);
     }

 WU(obj->vertex_num);

 for (i=0;i<obj->vertex_num;i++) { float x,y,z;

     x=obj->vertex[i].x-=obj->matrix[3][0];
     y=obj->vertex[i].y-=obj->matrix[3][1];
     z=obj->vertex[i].z-=obj->matrix[3][2];

     obj->vertex[i].x=x*im[0][0]+y*im[1][0]+z*im[2][0]+im[3][0];
     obj->vertex[i].y=x*im[0][1]+y*im[1][1]+z*im[2][1]+im[3][1];
     obj->vertex[i].z=x*im[0][2]+y*im[1][2]+z*im[2][2]+im[3][2];

     obj->vertex[i].id=i;
     obj->vertex[i].used=0;

     /*printf("v[%d]: %f %f %f\n",i,obj->vertex[i].x,
                                   obj->vertex[i].y,
                                   obj->vertex[i].z);*/
     WF(obj->vertex[i].x);
     WF(obj->vertex[i].y);
     WF(obj->vertex[i].z);
     WF(obj->vertex[i].u);
     WF(obj->vertex[i].v);
     }

 WU(obj->face_num); //printf("face:%d(%d)\n",obj->face_num,ftell(f));

 for (i=0;i<obj->face_num;i++) {

     create_normal(&obj->face[i]);

     WU(obj->face[i].id); obj->face[i].id=i;
     WU(obj->face[i].material);
     WU(obj->face[i].vertex[0]->id); obj->vertex[obj->face[i].vertex[0]->id].used++;
     WU(obj->face[i].vertex[1]->id); obj->vertex[obj->face[i].vertex[1]->id].used++;
     WU(obj->face[i].vertex[2]->id); obj->vertex[obj->face[i].vertex[2]->id].used++;

     /*printf("f[%d]: %d %d %d\n",i,obj->face[i].vertex[0]->id,
                                  obj->face[i].vertex[1]->id,
                                  obj->face[i].vertex[2]->id);*/
     WF(obj->face[i].normal.a);
     WF(obj->face[i].normal.b);
     WF(obj->face[i].normal.c);
     WF(obj->face[i].normal.d);

     }

 for (i=0;i<obj->vertex_num;i++)
     if (!obj->vertex[i].used) printf("unused_vertex: %d\n",i);

 rooted=0; name=obj->name; printf("%s -> ",name); obj->bsp=create_bsp(make_list(obj));
 nodes=0; faces=0; _save_bsp(f,obj->bsp); printf("\nBSP nodes: %d(%d)\n",nodes,faces/obj->face_num);
}

extern WORLD *load_Scene(char *in_name, char *out_name, int dir, int mode, int max, int root, int num)
{
 FILE *f; int i,j,faces,verts; char *sig=SIGNATURE; max_skip=max; quality=root; bestN=num;

 world=strstr(in_name,".3") ? load_3DS(in_name) : load_MGF(in_name,mode,dir);

 if (!world) { printf("Error: loader failed!"); return NULL; } faces=0; verts=0;

 for (i=0;i<world->object_num;i++) {
     faces+=world->object[i].face_num;
     verts+=world->object[i].vertex_num;
     }

 printf("world[%s]: objects: %d, materials: %d, faces: %d, vertices: %d\n",
        in_name,world->object_num,world->material_num,faces,verts);

 f=fopen(out_name,"wb"); fwrite(sig,1,strlen(sig),f);

 WU(world->start_frame);
 WU(world->end_frame);

 WU(world->material_num);

 for (i=0;i<world->material_num;i++) {
     WB(strlen(world->material[i].name));
     fwrite(world->material[i].name,
     strlen(world->material[i].name),1,f);
     }

 WU(world->camera_num);

 for (i=0;i<world->camera_num;i++) {

     WB(strlen(world->camera[i].name));
     fwrite(world->camera[i].name,
     strlen(world->camera[i].name),1,f);

     WU(world->camera[i].target_num);
     WU(world->camera[i].position_num);

     for (j=0;j<world->camera[i].target_num;j++) {
         WF(world->camera[i].tt[j]);
         WF(world->camera[i].tx[j]);
         WF(world->camera[i].ty[j]);
         WF(world->camera[i].tz[j]);
         }

     for (j=0;j<world->camera[i].position_num;j++) {
         WF(world->camera[i].pt[j]);
         WF(world->camera[i].px[j]);
         WF(world->camera[i].py[j]);
         WF(world->camera[i].pz[j]);
         }
     }

 WU(world->object_num);

 for (i=0;i<world->object_num;i++)
     save_object(f,&world->object[i]);

 fclose(f);

 return world;
}