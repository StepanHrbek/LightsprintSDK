#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "cnv2bsp.h"
#pragma comment(lib,"mgflib.lib")

FACE *face;
WORLD *world;
OBJECT *object;
VERTEX *vertex;
CAMERA *camera;
MATERIAL *material;

static int make_dir=0,skip_object=0;

unsigned face_cache=1000;
unsigned vertex_cache=1000;
unsigned object_cache=100;
unsigned material_cache=100;

unsigned mgf_vertices;
unsigned mgf_materials;
unsigned mgf_objects;
unsigned mgf_triangles;

unsigned mgf_vertex_id;
unsigned mgf_material_id;

unsigned mgf_face_id;
unsigned mgf_object_id;

unsigned obj_face_id;

void *(*_insert_vertex)(FLOAT *p,FLOAT *n);
void *(*_insert_surface)(C_MATERIAL *material);
void  (*_insert_polygon)(unsigned vertices,void **vertex,void *surface);
void  (*_begin_object)(char *name);
void  (*_end_object)(void);

#define xf_xid (xf_context?xf_context->xid:0)

static inline FACE *get_Face(int id)
{
 FACE *new_face;
 if (id<face_cache) return face+id;
 new_face=nALLOC(FACE,face_cache<<1);
 memcpy(new_face,face,sizeof(FACE)*face_cache);
 face_cache<<=1; face=new_face;
 return get_Face(id);
}

static inline VERTEX *get_Vertex(int id)
{
 VERTEX *new_vertex;
 if (id<vertex_cache) return vertex+id;
 new_vertex=nALLOC(VERTEX,vertex_cache<<1);
 memcpy(new_vertex,vertex,sizeof(VERTEX)*vertex_cache);
 vertex_cache<<=1; vertex=new_vertex;
 return get_Vertex(id);
}

static inline OBJECT *get_Object(int id)
{
 OBJECT *new_object;
 if (id<object_cache) return object+id;
 new_object=nALLOC(OBJECT,object_cache<<1);
 memcpy(new_object,object,sizeof(OBJECT)*object_cache);
 object_cache<<=1; object=new_object;
 return get_Object(id);
}

static inline MATERIAL *get_Material(int id)
{
 MATERIAL *new_material;
 if (id<material_cache) return material+id;
 new_material=nALLOC(MATERIAL,material_cache<<1);
 memcpy(new_material,material,sizeof(MATERIAL)*material_cache);
 material_cache<<=1; material=new_material;
 return get_Material(id);
}

static int vertex_comp(const void *a, const void *b)
{
 VERTEX *A=(VERTEX *)a,*B=(VERTEX *)b;

 if ((A->x<B->x) ||
     (A->x==B->x && A->y<B->y) ||
     (A->x==B->x && A->y==B->y && A->z<B->z)) return -1;

 if (A->x==B->x && A->y==B->y && A->z==B->z) return 0;

 return 1;
}

static int inline face_comp(const void *a, const void *b)
{
 FACE *A=(FACE *)a,*B=(FACE *)b;

 if ((A->vertex[0]->id<B->vertex[0]->id) ||
     (A->vertex[0]->id==B->vertex[0]->id &&
      A->vertex[1]->id<B->vertex[1]->id) ||
     (A->vertex[0]->id==B->vertex[0]->id &&
      A->vertex[1]->id==B->vertex[1]->id &&
      A->vertex[2]->id<B->vertex[2]->id)) return -1;

 if (A->vertex[0]->id==B->vertex[0]->id &&
     A->vertex[1]->id==B->vertex[1]->id &&
     A->vertex[2]->id==B->vertex[2]->id) return 0;

 return 1;
}

static inline int my_hface(int ac,char **av)
{
 void **vtx; int i;

 if(ac<4) return(MG_EARGC);
 vtx=nALLOC(void*,ac-1);

 for (i=1;i<ac;i++) {
     C_VERTEX *vp=c_getvert(av[i]);
     if (!vp) return(MG_EUNDEF);
     if (vp->clock!=-1-111*xf_xid && _insert_vertex) {
        FVECT vert; FVECT norm;
        xf_xfmpoint(vert, vp->p);
        xf_xfmvect(norm, vp->n); normalize(norm);
        vp->client_data=(char *)_insert_vertex(vert,norm);
        vp->clock=-1-111*xf_xid;
        }
     vtx[i-1]=vp->client_data;
     }

 assert(c_cmaterial);

 if (c_cmaterial->clock!=-1) { c_cmaterial->clock=-1;
    c_cmaterial->client_data=_insert_surface?
    (char *)_insert_surface(c_cmaterial):NULL;
    }

 if(_insert_polygon) _insert_polygon(ac-1,vtx,c_cmaterial->client_data);

 free(vtx); return(MG_OK);
}

static inline int my_hobject(int ac,char **av)
{
 if(ac==2) if(_begin_object) _begin_object(av[1]);
 if(ac==1) if(_end_object) _end_object();
 return obj_handler(ac,av);
}

int ldmgf(char *filename,
          void *insert_vertex(FLOAT *p,FLOAT *n),
          void *insert_surface(C_MATERIAL *material),
          void  insert_polygon(unsigned vertices,void **vertex,void *surface),
          void  begin_object(char *name),
          void  end_object(void))
{
 static int loaded=0; if(loaded) mg_clear(); loaded=1;
 mg_ehand[MG_E_COLOR]    = c_hcolor;     /* they get color */
 mg_ehand[MG_E_CMIX]     = c_hcolor;     /* they mix colors */
 mg_ehand[MG_E_CSPEC]    = c_hcolor;     /* they get spectra */
 mg_ehand[MG_E_CXY]      = c_hcolor;     /* they get chromaticities */
 mg_ehand[MG_E_CCT]      = c_hcolor;     /* they get color temp's */
 mg_ehand[MG_E_ED]       = c_hmaterial;  /* they get emission */
 mg_ehand[MG_E_FACE]     = my_hface;     /* we do faces */
 mg_ehand[MG_E_MATERIAL] = c_hmaterial;  /* they get materials */
 mg_ehand[MG_E_NORMAL]   = c_hvertex;    /* they get normals */
 mg_ehand[MG_E_OBJECT]   = my_hobject;   /* we track object names */
 mg_ehand[MG_E_POINT]    = c_hvertex;    /* they get points */
 mg_ehand[MG_E_RD]       = c_hmaterial;  /* they get diffuse refl. */
 mg_ehand[MG_E_RS]       = c_hmaterial;  /* they get specular refl. */
 mg_ehand[MG_E_SIDES]    = c_hmaterial;  /* they get # sides */
 mg_ehand[MG_E_TD]       = c_hmaterial;  /* they get diffuse trans. */
 mg_ehand[MG_E_TS]       = c_hmaterial;  /* they get specular trans. */
 mg_ehand[MG_E_VERTEX]   = c_hvertex;    /* they get vertices */
 mg_ehand[MG_E_POINT]    = c_hvertex;
 mg_ehand[MG_E_NORMAL]   = c_hvertex;
 mg_ehand[MG_E_XF]       = xf_handler;   /* they track transforms */
 mg_init();
 _insert_vertex=insert_vertex;
 _insert_surface=insert_surface;
 _insert_polygon=insert_polygon;
 _begin_object=begin_object;
 _end_object=end_object;
 return mg_load(filename);
}

static void insert_object(char *name)
{
 OBJECT *o=get_Object(mgf_object_id); int i,j; obj_face_id=0;

 if (make_dir && strlen(name)>2)
    if (name[0]=='l' && name[1]=='m' && name[2]=='.') mkdir(name);

 if (skip_object) { printf("adding[%s]\n",name); return; }

 printf("object[%s]: ",name); mgf_face_id=0;

 for (j=0;j<4;j++) { for (i=0;i<4;i++) o->matrix[i][j]=0; o->matrix[j][j]=1; }

 o->name=nALLOC(char,strlen(name)+1); strcpy(o->name,name);
 o->pt=nALLOC(PATH,1); o->pt[0]=0;
 o->px=nALLOC(PATH,1); o->px[0]=0;
 o->py=nALLOC(PATH,1); o->py[0]=0;
 o->pz=nALLOC(PATH,1); o->pz[0]=0;
 o->qt=nALLOC(PATH,1); o->qt[0]=0;
 o->qw=nALLOC(PATH,1); o->qw[0]=0;
 o->qx=nALLOC(PATH,1); o->qx[0]=0;
 o->qy=nALLOC(PATH,1); o->qy[0]=0;
 o->qz=nALLOC(PATH,1); o->qz[0]=0;
 o->id=mgf_object_id;
 o->rot_num=1;
 o->pos_num=1;
 o->parent=-1;
}

static void end_object()
{
 OBJECT *o=get_Object(mgf_object_id); int i,j,vertex_id;

 if (skip_object) return;

 o->vertex_num=0;
 o->face_num=mgf_face_id;

 for (i=0;i<mgf_face_id;i++) for (j=0;j<3;j++) face[i].vertex[j]->used=0;
 for (i=0;i<mgf_face_id;i++) for (j=0;j<3;j++)
     if (!face[i].vertex[j]->used) {
          face[i].vertex[j]->used=1; o->vertex_num++; }

 o->face=nALLOC(FACE,o->face_num);
 o->vertex=nALLOC(VERTEX,o->vertex_num);

 vertex_id=0;

 for (i=0;i<mgf_face_id;i++) for (j=0;j<3;j++) face[i].vertex[j]->id=-1;
 for (i=0;i<mgf_face_id;i++) { memcpy(&o->face[i],&face[i],sizeof(FACE));
     for (j=0;j<3;j++) if (face[i].vertex[j]->id>=0)
         o->face[i].vertex[j]=&o->vertex[face[i].vertex[j]->id]; else {
         memcpy(&o->vertex[vertex_id],face[i].vertex[j],sizeof(VERTEX));
         o->face[i].vertex[j]=&o->vertex[vertex_id];
         face[i].vertex[j]->id=vertex_id++;
         }
     }

 printf("faces: %d + vertices: %d\n",o->face_num,o->vertex_num);

 if (o->face_num>0) mgf_object_id++;
}

static void *insert_vertex(FLOAT *p, FLOAT *n)
{
 VERTEX *v=get_Vertex(mgf_vertex_id);
 //printf("v[%d]: %f %f %f\n",mgf_vertex_id,p[0],p[1],p[2]);
 v->x=p[0]; v->y=p[1]; v->z=p[2];
 return (void *)(mgf_vertex_id++);
}

static void *insert_material(C_MATERIAL *cm)
{
 MATERIAL *m;
 m=get_Material(mgf_material_id);
 if (!c_cmname) {
    m->name=nALLOC(char,9);
    strcpy(m->name,"Untitled");
    } else {
    m->name=nALLOC(char,strlen(c_cmname)+1);
    strcpy(m->name,c_cmname);
    }
 return (void *)(mgf_material_id++);
}

static void insert_polygon(unsigned vertices, void **vertex_id, void *material)
{
 FACE *f,*o,*n; int i; f=get_Face(mgf_face_id++); n=f;

 //printf("f[%d]: %d %d %d %d\n",mgf_face_id,
 //vertex_id[0],vertex_id[1],vertex_id[2],vertex_id[3]);

 f->vertex[0]=&vertex[(int)vertex_id[0]];
 f->vertex[1]=&vertex[(int)vertex_id[1]];
 f->vertex[2]=&vertex[(int)vertex_id[2]];
 f->material=(int)material;
 f->id=obj_face_id++;

 for (i=3;i<vertices;i++) { o=n;
     n=get_Face(mgf_face_id++);
     n->id=obj_face_id++;
     n->material=(int)material;
     n->vertex[0]=o->vertex[0];
     n->vertex[1]=o->vertex[2];
     n->vertex[2]=&vertex[(int)vertex_id[i]];
     }
}

static inline CAMERA *get_Camera()
{
 CAMERA *c=ALLOC(CAMERA); memset(c,0,sizeof(CAMERA));

 c->name=nALLOC(char,11); strcpy(c->name,"MGF_Camera");
 c->position_num=1;
 c->target_num=1;

 c->tt=nALLOC(PATH,1); c->tt[0]=0;
 c->tx=nALLOC(PATH,1); c->tx[0]=0;
 c->ty=nALLOC(PATH,1); c->ty[0]=0;
 c->tz=nALLOC(PATH,1); c->tz[0]=0;

 c->pt=nALLOC(PATH,1); c->pt[0]=0;
 c->px=nALLOC(PATH,1); c->px[0]=0;
 c->py=nALLOC(PATH,1); c->py[0]=0;
 c->pz=nALLOC(PATH,1); c->pz[0]=SCALE;

 return c;
}

static VERTEX *find_vertex(VERTEX *v, VERTEX *V, int left, int right)
{
 int id=(left+right)/2; VERTEX *m=V+id;

 if (right-left<6) {
    for (m=V+left;m<=V+right;m++)
        if (v->x==m->x && v->y==m->y && v->z==m->z) return m;
    printf("Error: vertex not found!\n"); exit(1);
    }

 if ((v->x<m->x) ||
     (v->x==m->x && v->y<m->y) ||
     (v->x==m->x && v->y==m->y && v->z<m->z))
     return find_vertex(v,V,left,id); else
     return find_vertex(v,V,id,right);
}

extern WORLD *load_MGF(char *fn, int mode, int dir)
{
 int i,j,k,res; VERTEX center,min,max; float diam;

 make_dir=dir;

 mgf_face_id=0;
 mgf_vertex_id=0;
 mgf_object_id=0;
 mgf_material_id=0;

 material=nALLOC(MATERIAL,material_cache);
 memset(material,0,sizeof(MATERIAL)*material_cache);

 object=nALLOC(OBJECT,object_cache);
 memset(object,0,sizeof(OBJECT)*object_cache);

 vertex=nALLOC(VERTEX,vertex_cache);
 memset(vertex,0,sizeof(VERTEX)*vertex_cache);

 face=nALLOC(FACE,face_cache);
 memset(face,0,sizeof(FACE)*face_cache);

 if (mode) { insert_object("test"); skip_object=1; }

 res=ldmgf(fn,insert_vertex,insert_material,
              insert_polygon,insert_object,end_object);

 if (mode) { skip_object=0; end_object(); }

 if (res!=MG_OK) {
    free(object); free(vertex);
    free(material); free(face);
    printf("Error: %s\n",mg_err[res]); return NULL; }

 min.x=object[0].vertex[0].x;
 min.y=object[0].vertex[0].y;
 min.z=object[0].vertex[0].z;

 max.x=object[0].vertex[0].x;
 max.y=object[0].vertex[0].y;
 max.z=object[0].vertex[0].z;

 for (i=0;i<mgf_object_id;i++) {

     OBJECT *o=object+i; VERTEX *uniq_vertex; FACE *uniq_face;

     memcpy(vertex,o->vertex,o->vertex_num*sizeof(VERTEX));

     qsort(vertex,o->vertex_num,sizeof(VERTEX),vertex_comp);

     uniq_vertex=nALLOC(VERTEX,o->vertex_num);

     uniq_vertex[0].id=0;
     uniq_vertex[0].x=vertex[0].x;
     uniq_vertex[0].y=vertex[0].y;
     uniq_vertex[0].z=vertex[0].z;

     for (k=1,j=0;k<o->vertex_num;k++)
         if (uniq_vertex[j].x!=vertex[k].x ||
             uniq_vertex[j].y!=vertex[k].y ||
             uniq_vertex[j].z!=vertex[k].z) { j++;
             uniq_vertex[j].id=j;
             uniq_vertex[j].x=vertex[k].x;
             uniq_vertex[j].y=vertex[k].y;
             uniq_vertex[j].z=vertex[k].z;
             }

     for (k=0;k<o->face_num;k++) {

         o->face[k].vertex[0]=find_vertex(o->face[k].vertex[0],uniq_vertex,0,j);
         o->face[k].vertex[1]=find_vertex(o->face[k].vertex[1],uniq_vertex,0,j);
         o->face[k].vertex[2]=find_vertex(o->face[k].vertex[2],uniq_vertex,0,j);

         }

     printf("object[%s]: vertex reduction: %d/%d\n",o->name,j+1,o->vertex_num);

     free(o->vertex); o->vertex=uniq_vertex; o->vertex_num=j+1;

     /*
     
     qsort(o->face,o->face_num,sizeof(FACE),face_comp);

     uniq_face=nALLOC(FACE,o->face_num);

     memcpy(uniq_face,o->face,sizeof(FACE));

     for (k=1,j=0;k<o->face_num;k++)
         if (uniq_face[j].vertex[0]!=o->face[k].vertex[0] ||
             uniq_face[j].vertex[1]!=o->face[k].vertex[1] ||
             uniq_face[j].vertex[2]!=o->face[k].vertex[2]) {
             j++; memcpy(uniq_face+j,o->face+k,sizeof(FACE)); }

     printf("object[%s]: face reduction: %d/%d\n",o->name,j+1,o->face_num);

     free(o->face); o->face=uniq_face; o->face_num=j+1;
     
     */
     
     for (k=0;k<o->vertex_num;k++) {

         if (min.x>o->vertex[k].x) min.x=o->vertex[k].x;
         if (min.y>o->vertex[k].y) min.y=o->vertex[k].y;
         if (min.z>o->vertex[k].z) min.z=o->vertex[k].z;

         if (max.x<o->vertex[k].x) max.x=o->vertex[k].x;
         if (max.y<o->vertex[k].y) max.y=o->vertex[k].y;
         if (max.z<o->vertex[k].z) max.z=o->vertex[k].z;

         }
     }

 center.x=(max.x+min.x)/2;
 center.y=(max.y+min.y)/2;
 center.z=(max.z+min.z)/2;

 diam=sqrt(SQR(max.x-min.x)+SQR(max.y-min.y)+SQR(max.z-min.z));

 for (i=0;i<mgf_object_id;i++) { OBJECT *o=object+i;

     for (k=0;k<o->vertex_num;k++) {

         o->vertex[k].x=SCALE*(o->vertex[k].x-center.x)/diam;
         o->vertex[k].y=SCALE*(o->vertex[k].y-center.y)/diam;
         o->vertex[k].z=SCALE*(o->vertex[k].z-center.z)/diam;

         }
     }

 world=ALLOC(WORLD);
 world->camera_num=1;
 world->object_num=mgf_object_id;
 world->material_num=mgf_material_id;
 world->start_frame=0;
 world->end_frame=0;

 world->object=object;
 world->material=material;
 world->camera=get_Camera();

 free(vertex); free(face); return world;
}