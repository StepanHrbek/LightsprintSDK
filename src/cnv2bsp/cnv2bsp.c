#include <assert.h>
#include <conio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cnv2bsp.h"

#define SIGNATURE "BSP\n"
#define CACHE_SIZE 1000
#define ZERO 0.00001
#define DELTA 0.001
#define MAXL 100

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

static inline void cross_product(VECTOR res, VECTOR a, VECTOR b)
{
 res[0]=b[1]*a[2]-a[1]*b[2];
 res[1]=b[2]*a[0]-a[2]*b[0];
 res[2]=b[0]*a[1]-a[0]*b[1];
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
 int i; MATRIX om,im; BBOX bbox={-1e10,-1e10,-1e10,1e10,1e10,1e10};

 //assert(f);
 //assert(obj->face_num>0); pozor, nastava

 WB((byte)strlen(obj->name));

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
     WF(Float);//!!!obj->vertex[i].u);
     WF(Float);//!!!obj->vertex[i].v);

     #define MAX(a,b) (((a)>(b))?(a):(b))
     #define MIN(a,b) (((a)<(b))?(a):(b))
     bbox.hi[0]=MAX(bbox.hi[0],obj->vertex[i].x);
     bbox.hi[1]=MAX(bbox.hi[1],obj->vertex[i].y);
     bbox.hi[2]=MAX(bbox.hi[2],obj->vertex[i].z);
     bbox.lo[0]=MIN(bbox.lo[0],obj->vertex[i].x);
     bbox.lo[1]=MIN(bbox.lo[1],obj->vertex[i].y);
     bbox.lo[2]=MIN(bbox.lo[2],obj->vertex[i].z);
     }

 WU(obj->face_num); //printf("face:%d(%d)\n",obj->face_num,ftell(f));

 for (i=0;i<obj->face_num;i++) {

     WU(obj->face[i].id); obj->face[i].id=i;
     WU(obj->face[i].material);
     WU(obj->face[i].vertex[0]->id); obj->vertex[obj->face[i].vertex[0]->id].used++;
     WU(obj->face[i].vertex[1]->id); obj->vertex[obj->face[i].vertex[1]->id].used++;
     WU(obj->face[i].vertex[2]->id); obj->vertex[obj->face[i].vertex[2]->id].used++;

     /*printf("f[%d]: %d %d %d\n",i,obj->face[i].vertex[0]->id,
                                  obj->face[i].vertex[1]->id,
                                  obj->face[i].vertex[2]->id);*/
     WF(Float); //!!! faked normal
     WF(Float);
     WF(Float);
     WF(Float);

     }

 for (i=0;i<obj->vertex_num;i++)
     if (!obj->vertex[i].used) printf("unused_vertex: %d\n",i);

 rooted=0; name=obj->name; printf("%s -> ",name); 
}

extern WORLD *load_Scene(char *in_name, char *out_name, int dir, int mode)
{
 FILE *f; int i,j,faces,verts; char *sig=SIGNATURE;

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
     WB((byte)strlen(world->material[i].name));
     fwrite(world->material[i].name,
     strlen(world->material[i].name),1,f);
     }

 WU(world->camera_num);

 for (i=0;i<world->camera_num;i++) {

     WB((byte)strlen(world->camera[i].name));
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