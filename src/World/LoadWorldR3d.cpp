#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spline.h"
#include "World.h"

#define nALLOC(A,B) (A *)malloc((B)*sizeof(A))
#define ALLOC(A) nALLOC(A,1)
#define SIGNATURE "BSP\n"

#define byte  unsigned char
#define word  unsigned short

#define RU(A) fread(&Unsigned,sizeof(unsigned),1,f); A=Unsigned
#define RI(A) fread(&Integer,sizeof(int),1,f); A=Integer
#define RF(A) fread(&Float,sizeof(float),1,f); A=Float
#define RW(A) fread(&Word,sizeof(word),1,f); A=Word
#define RB(A) fread(&Byte,sizeof(byte),1,f); A=Byte

static byte Byte;
//static word Word;
static float Float;
static int Integer;
static unsigned Unsigned;

static void load_object(FILE *f, OBJECT *obj)
{
 int i,id,size; float s; spline_MATRIX m; KEY *k;

 RB(size);

 obj->name=nALLOC(char,size+1);
 fread(obj->name,size,1,f);
 obj->name[size]=0;

 fread(obj->matrix,sizeof(MATRIX),1,f);

 RU(obj->id); RI(obj->parent);

 RU(obj->pos.num);

 obj->pos.keys=nALLOC(KEY,obj->pos.num);

 for (i=0;i<obj->pos.num;i++) { k=&obj->pos.keys[i];

     RF(k->frame); RF(k->pos.x); RF(k->pos.y); RF(k->pos.z);

     k->pos.w=0; k->tens=0; k->cont=0; k->bias=0; k->et=0; k->ef=0;
     k->ds.x=0; k->ds.y=0; k->ds.z=0; k->dd.x=0; k->dd.y=0; k->dd.z=0;

     }

 RU(obj->rot.num);

 obj->rot.keys=nALLOC(KEY,obj->rot.num);

 for (i=0;i<obj->rot.num;i++) { k=&obj->rot.keys[i];

     RF(k->frame); RF(k->aa.w); RF(k->aa.x); RF(k->aa.y); RF(k->aa.z);

     k->aa.w=-k->aa.w; s=sin(k->aa.w/2); k->pos.w=cos(k->aa.w/2);
     k->pos.x=k->aa.x*s; k->pos.y=k->aa.y*s; k->pos.z=k->aa.z*s;

     k->tens=0; k->cont=0; k->bias=0; k->et=0; k->ef=0; k->ds.x=0;
     k->ds.y=0; k->ds.z=0; k->dd.x=0; k->dd.y=0; k->dd.z=0;

     }

 k=&obj->rot.keys[0];

 spline_Quat2invMatrix(k->pos,&m);

 k->aa.w=0; k->aa.x=0; k->aa.y=0; k->aa.z=0;

 k->aa.w=-k->aa.w; s=sin(k->aa.w/2); k->pos.w=cos(k->aa.w/2);
 k->pos.x=k->aa.x*s; k->pos.y=k->aa.y*s; k->pos.z=k->aa.z*s;

 obj->matrix[0][0] = m.xx; obj->matrix[1][0] = m.xy; obj->matrix[2][0] = m.xz;
 obj->matrix[0][1] = m.yx; obj->matrix[1][1] = m.yy; obj->matrix[2][1] = m.yz;
 obj->matrix[0][2] = m.zx; obj->matrix[1][2] = m.zy; obj->matrix[2][2] = m.zz;

 obj->matrix[0][3] = 0; obj->matrix[3][0] = 0; obj->matrix[1][3] = 0;
 obj->matrix[3][1] = 0; obj->matrix[2][3] = 0; obj->matrix[3][2] = 0;
 obj->matrix[3][3] = 1;

 obj->pos.loop=1;
 obj->rot.loop=1;
 obj->pos.repeat=1;
 obj->rot.repeat=1;

 spline_Init(&obj->pos,0);
 spline_Init(&obj->rot,1);

 RU(obj->vertex_num);

 obj->vertex=nALLOC(VERTEX,obj->vertex_num);

 for (i=0;i<obj->vertex_num;i++) {

     obj->vertex[i].id=i;

     RF(obj->vertex[i].x);
     RF(obj->vertex[i].y);
     RF(obj->vertex[i].z);
     RF(Float);//obj->vertex[i].u);
     RF(Float);//obj->vertex[i].v);
/*
     //!!! smazat
     for (int j=0;j<i;j++)
         if (obj->vertex[i].x==obj->vertex[j].x && obj->vertex[i].y==obj->vertex[j].y && obj->vertex[i].z==obj->vertex[j].z) {
            printf("V .bsp jsou identicky vertexy %d a %d [%f %f %f].",j,i,obj->vertex[i].x,obj->vertex[i].y,obj->vertex[i].z);
            exit(1);
         }
*/
     }

 RU(obj->face_num); obj->face=nALLOC(FACE,obj->face_num);

 for (i=0;i<obj->face_num;i++) {
     RU(obj->face[i].id);
     RU(obj->face[i].material);
     RU(id); if(id<0 || id>=obj->vertex_num) {id=0;printf("Bad bsp format(1).");} obj->face[i].vertex[0]=&obj->vertex[id];
     RU(id); if(id<0 || id>=obj->vertex_num) {id=0;printf("Bad bsp format(1).");} obj->face[i].vertex[1]=&obj->vertex[id];
     RU(id); if(id<0 || id>=obj->vertex_num) {id=0;printf("Bad bsp format(1).");} obj->face[i].vertex[2]=&obj->vertex[id];
     RF(Float);//obj->face[i].normal.a);
     RF(Float);//obj->face[i].normal.b);
     RF(Float);//obj->face[i].normal.c);
     RF(Float);//obj->face[i].normal.d);
     }
}

static HIERARCHY *make_hierarchy(WORLD *w, int id)
{
 HIERARCHY *h; int i;

 for (i=0;i<w->object_num;i++) if (w->object[i].parent==id) break;

 if (i==w->object_num) return NULL;

 h=ALLOC(HIERARCHY); h->object=&w->object[i]; w->object[i].parent=-2;
 h->child=make_hierarchy(w,w->object[i].id);
 h->brother=make_hierarchy(w,id);

 return h;
}

extern WORLD *load_world(char *name)
{
 WORLD *world; FILE *f; int i,j,size; const char *sig=SIGNATURE;

 if(!name) return NULL;
 f=fopen(name,"rb");
 if (!f) return NULL;
 fseek(f,(long)strlen(sig),SEEK_SET);

 world=ALLOC(WORLD);

 RU(world->start_frame);
 RU(world->end_frame);

 RU(world->material_num);
 world->material=nALLOC(MATERIAL,world->material_num);

 for (i=0;i<world->material_num;i++) { RB(size);
     world->material[i].name=nALLOC(char,size+1);
     fread(world->material[i].name,size,1,f);
     world->material[i].name[size]=0;
     }

 RU(world->camera_num);
 world->camera=nALLOC(CAMERA,world->camera_num);

 for (i=0;i<world->camera_num;i++) { RB(size);

     world->camera[i].name=nALLOC(char,size+1);
     fread(world->camera[i].name,size,1,f);
     world->camera[i].name[size]=0;

     RU(world->camera[i].tar.num);
     RU(world->camera[i].pos.num);

     world->camera[i].tar.keys=nALLOC(KEY,world->camera[i].tar.num);
     world->camera[i].pos.keys=nALLOC(KEY,world->camera[i].pos.num);

     for (j=0;j<world->camera[i].tar.num;j++) {
         KEY *k=&world->camera[i].tar.keys[j];
         RF(k->frame); RF(k->pos.x); RF(k->pos.y); RF(k->pos.z);
         k->pos.w=0; k->tens=0; k->cont=0; k->bias=0; k->et=0; k->ef=0;
         k->ds.x=0; k->ds.y=0; k->ds.z=0; k->dd.x=0; k->dd.y=0; k->dd.z=0;
         }

     for (j=0;j<world->camera[i].pos.num;j++) {
         KEY *k=&world->camera[i].pos.keys[j];
         RF(k->frame); RF(k->pos.x); RF(k->pos.y); RF(k->pos.z);
         k->pos.w=0; k->tens=0; k->cont=0; k->bias=0; k->et=0; k->ef=0;
         k->ds.x=0; k->ds.y=0; k->ds.z=0; k->dd.x=0; k->dd.y=0; k->dd.z=0;
         }

     world->camera[i].pos.loop=1;
     world->camera[i].tar.loop=1;
     world->camera[i].pos.repeat=1;
     world->camera[i].tar.repeat=1;

     spline_Init(&world->camera[i].tar,0);
     spline_Init(&world->camera[i].pos,0);
     }

 RU(world->object_num); world->object=nALLOC(OBJECT,world->object_num);

 for (i=0;i<world->object_num;i++) load_object(f,&world->object[i]);

 fclose(f); world->hierarchy=make_hierarchy(world,-1); return world;
}
