#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cnv2bsp.h"

#define OK 1
#define ERROR -1
#define read_NULL NULL
#define INT static int
#define _CHUNK_ FILE *f, long p
#define MAXL 64

#define BLOCK(X) if (fread(&X,sizeof(X),1,f)!=OK) return ERROR
#define ARRAY(X) if (fread(X,sizeof(X),1,f)!=OK) return ERROR
#define ASCIIZ(X) if (read_ASCIIZ(f,X)!=OK) return ERROR

WORLD *world; OBJECT *object; CAMERA *camera; MATERIAL *material;

word object_number;

INT set_Object(int world_id, int self_id, int parent_id)
{
 world->object[world_id].id=self_id;
 world->object[world_id].parent=parent_id;
 return OK;
}

INT get_Object(char *name)
{
 int i; for (i=0;i<world->object_num;i++)
 if (!strcmp(world->object[i].name,name)) return i;
 return ERROR;
}

INT get_Camera(char *name)
{
 int i; for (i=0;i<world->camera_num;i++)
 if (!strcmp(world->camera[i].name,name)) return i;
 return ERROR;
}

INT get_Material(char *name)
{
 int i; for (i=0;i<world->material_num;i++)
 if (!strcmp(world->material[i].name,name)) return i;
 return ERROR;
}

// ------------------------------------

#pragma pack(2)

typedef struct { word id; dword len; } TChunkHeader, *PChunkHeader;

#pragma pack()

enum {
  CHUNK_VER       = 0x0002,
  CHUNK_RGBF      = 0x0010,
  CHUNK_RGBB      = 0x0011,
  CHUNK_RGBB2     = 0x0012,
  CHUNK_AMOUNTOF  = 0x0030,
  CHUNK_AMOUNTOFF = 0x0031,
  CHUNK_MASTERSCALE=0x0100,
  CHUNK_PRJ       = 0xC23D,
  CHUNK_MLI       = 0x3DAA,
  CHUNK_MESHVER   = 0x3D3E,

  CHUNK_MAIN      = 0x4D4D,
    CHUNK_OBJMESH   = 0x3D3D,
      CHUNK_BKGCOLOR  = 0x1200,
      CHUNK_AMBCOLOR  = 0x2100,
      CHUNK_OBJBLOCK  = 0x4000,
        CHUNK_TRIMESH   = 0x4100,
          CHUNK_VERTLIST  = 0x4110,
          CHUNK_VERTFLAGS = 0x4111,
          CHUNK_FACELIST  = 0x4120,
          CHUNK_FACEMAT   = 0x4130,
          CHUNK_MAPLIST   = 0x4140,
          CHUNK_SMOOLIST  = 0x4150,
          CHUNK_TRMATRIX  = 0x4160,
          CHUNK_MESHCOLOR = 0x4165,
          CHUNK_TXTINFO   = 0x4170,
          CHUNK_BOXMAP    = 0x4190,
        CHUNK_LIGHT     = 0x4600,
          CHUNK_SPOTLIGHT = 0x4610,
        CHUNK_CAMERA    = 0x4700,
        CHUNK_HIERARCHY = 0x4F00,

    CHUNK_VIEWPORT  = 0x7001,

    CHUNK_MATERIAL  = 0xAFFF,
      CHUNK_MATNAME   = 0xA000,
      CHUNK_AMBIENT   = 0xA010,
      CHUNK_DIFFUSE   = 0xA020,
      CHUNK_SPECULAR  = 0xA030,
      CHUNK_SHININESS    = 0xA040,
      CHUNK_SHINSTRENGTH = 0xA041,
      CHUNK_TRANSPARENCY = 0xA050,
      CHUNK_TRANSFALLOFF = 0xA052,
      CHUNK_REFBLUR      = 0xA053,
      CHUNK_SELFILLUM    = 0xA084,
      CHUNK_TWOSIDED     = 0xA081,
      CHUNK_TRANSADD     = 0xA083,
      CHUNK_WIREON       = 0xA085,
      CHUNK_WIRESIZE     = 0xA087,
      CHUNK_SOFTEN       = 0xA08C,
      CHUNK_SHADING   = 0xA100,
      CHUNK_TEXTURE   = 0xA200,
      CHUNK_REFLECT   = 0xA220,
      CHUNK_BUMPMAP   = 0xA230,
      CHUNK_BUMPAMOUNT= 0xA252,
      CHUNK_MAPFHANDLE= 0xA300,
      CHUNK_MAPFLAGS  = 0xA351,
      CHUNK_MAPBLUR   = 0xA353,
      CHUNK_MAPUSCALE = 0xA354,
      CHUNK_MAPVSCALE = 0xA356,
      CHUNK_MAPUOFFSET= 0xA358,
      CHUNK_MAPVOFFSET= 0xA35A,
      CHUNK_MAPROTANGLE=0xA35C,

    CHUNK_KEYFRAMER = 0xB000,
      CHUNK_AMBIENTKEY   = 0xB001,
      CHUNK_TRACKINFO    = 0xB002,
      CHUNK_TRACKCAMERA  = 0xB003,
      CHUNK_TRACKCAMTGT  = 0xB004,
      CHUNK_TRACKLIGHT   = 0xB005,
      CHUNK_TRACKLIGTGT  = 0xB006,
      CHUNK_TRACKSPOTL   = 0xB007,
      CHUNK_FRAMES       = 0xB008,
      CHUNK_KFCURTIME    = 0xB009,
      CHUNK_KFHDR        = 0xB00A,

        CHUNK_TRACKOBJNAME = 0xB010,
        CHUNK_DUMMYNAME    = 0xB011,
        CHUNK_PRESCALE     = 0xB012,
        CHUNK_TRACKPIVOT   = 0xB013,
        CHUNK_BOUNDBOX     = 0xB014,
        CHUNK_MORPH_SMOOTH = 0xB015,
        CHUNK_TRACKPOS     = 0xB020,
        CHUNK_TRACKROTATE  = 0xB021,
        CHUNK_TRACKSCALE   = 0xB022,
        CHUNK_TRACKFOV     = 0xB023,
        CHUNK_TRACKROLL    = 0xB024,
        CHUNK_TRACKCOLOR   = 0xB025,
        CHUNK_TRACKMORPH   = 0xB026,
        CHUNK_TRACKHOTSPOT = 0xB027,
        CHUNK_TRACKFALLOFF = 0xB028,
        CHUNK_TRACKHIDE    = 0xB029,
        CHUNK_OBJNUMBER    = 0xB030,
};

// ------------------------------------

static word face_num;
static char name[MAXL];
static char track_name[MAXL];
static int init,track_type,camera_id,object_id,
           material_id,frame_id,element_id;

#define UNKNOWN         0
#define CAMERA_UNKNOWN  1
#define CAMERA_TARGET   2
#define CAMERA_POSITION 3
#define OBJECT_POSITION 4

static inline int read_ASCIIZ(FILE *f, char *name)
{
 int i=0,c; while ((c=fgetc(f))!=EOF&&c&&i<MAXL-1) name[i++]=c;
 name[i]=0; if (i==MAXL-1) return OK; return c==EOF ? ERROR : OK;
}

INT read_Float(_CHUNK_) { float a; BLOCK(a); return OK; }
INT read_Short(_CHUNK_) { short a; BLOCK(a); return OK; }
INT read_Word(_CHUNK_) { word a; BLOCK(a); return OK; }
INT read_Skip(_CHUNK_) { return OK; }
INT read_Chunk(_CHUNK_);

INT read_RGBF(_CHUNK_) { float c[3]; ARRAY(c); return OK; }
INT read_RGBB(_CHUNK_) { byte c[3]; ARRAY(c); return OK; }
INT read_Light(_CHUNK_) { float c[3]; ARRAY(c); return OK; }
INT read_ObjBlock(_CHUNK_) { ASCIIZ(name); return OK; }
INT read_SpotLight(_CHUNK_) { float c[5]; ARRAY(c); return OK; }
INT read_MapFile(_CHUNK_) { char fn[MAXL]; ASCIIZ(fn); return OK; }

// ------------------------------------

INT read_ObjNumber(_CHUNK_)
{
 BLOCK(object_number);
 return OK;
}

INT read_TriMesh(_CHUNK_)
{
 if (init) world->object_num++; else {
    object=&world->object[object_id++];
    object->name=nALLOC(char,strlen(name)+1);
    strcpy(object->name,name);
    }
 return OK;
}

INT read_VertList(_CHUNK_)
{
 word nv; float c[3]; int i=0; BLOCK(nv);
 if (!init) { object->vertex_num=nv; object->vertex=nALLOC(VERTEX,nv); }
 while (i<nv) { ARRAY(c);
       if (!init) {
          object->vertex[i].x=c[0];
          object->vertex[i].y=c[2];
          object->vertex[i].z=c[1];
          } i++;
       }
 return OK;
}

INT read_VertFlags(_CHUNK_)
{
 word nv,fl; BLOCK(nv); while (nv-->0) { BLOCK(fl); } return OK;
}

INT read_FaceList(_CHUNK_)
{
 word nf,c[3],flags; int i=0; BLOCK(nf); face_num=nf;
 if (!init) { object->face_num=nf; object->face=nALLOC(FACE,nf); }
 while (i<nf) { ARRAY(c); BLOCK(flags);
       if (!init) {
          object->face[i].vertex[0]=&object->vertex[c[0]];
          object->face[i].vertex[1]=&object->vertex[c[2]];
          object->face[i].vertex[2]=&object->vertex[c[1]];
          } i++;
       }
 return OK;
}

INT read_FaceMat(_CHUNK_)
{
 char matname[MAXL]; word n,nf; int m,i=0;
 ASCIIZ(matname); BLOCK(n); if (!init) m=get_Material(matname);
 while (i<n) { BLOCK(nf); if (!init) object->face[nf].material=m; i++; }
 return OK;
}

INT read_MapList(_CHUNK_)
{
 word nv; float c[2]; int i=0; BLOCK(nv);
 while (i<nv) { ARRAY(c);
       if (!init) {
          object->vertex[i].u=c[0];
          object->vertex[i].v=1.0f-c[1];
          } i++;
       }
 return OK;
}

INT read_SmooList(_CHUNK_)
{
 dword s; word nv=face_num; while (nv-->0) { BLOCK(s); } return OK;
}

INT read_TrMatrix(_CHUNK_)
{
 float rot[9]; float trans[3]; BLOCK(rot); BLOCK(trans);
 if (!init) {
#define M_(A,B) object->matrix[B][A]
    M_(0,0) =  rot[0]; M_(0,1) =  rot[6]; M_(0,2) =  rot[3]; M_(0,3) = trans[0];
    M_(1,0) =  rot[2]; M_(1,1) =  rot[8]; M_(1,2) =  rot[5]; M_(1,3) = trans[2];
    M_(2,0) =  rot[1]; M_(2,1) =  rot[7]; M_(2,2) =  rot[4]; M_(2,3) = trans[1];
    M_(3,0) = 0;       M_(3,1) = 0;       M_(3,2) = 0;       M_(3,3) = 1;
    }
 return OK;
}

INT read_Camera(_CHUNK_)
{
 float c[8]; ARRAY(c);
 if (init) world->camera_num++; else {
    camera=&world->camera[camera_id++];
    camera->name=nALLOC(char,strlen(name)+1);
    strcpy(camera->name,name);
    }
 return OK;
}

INT read_MatName(_CHUNK_)
{
 ASCIIZ(name);
 if (init) world->material_num++; else {
    material=&world->material[material_id++];
    material->name=nALLOC(char,strlen(name)+1);
    strcpy(material->name,name);
    }
 return OK;
}

// ------------------------------------

INT read_Frames(_CHUNK_)
{
 dword c[2]; ARRAY(c);
 world->start_frame=c[0];
 world->end_frame=c[1];
 return OK;
}

INT read_TrackObjName(_CHUNK_)
{
 word w[2]; short parent; // parent changed from word which caused problem, all objects had parent=65535 instead of expected -1, hierarchy was broken
 ASCIIZ(track_name); ARRAY(w); BLOCK(parent);
 if (init) return OK;
 if (track_type!=UNKNOWN) element_id=get_Camera(track_name);
    else { element_id=get_Object(track_name);
           if(element_id>=0) set_Object(element_id,object_number,parent);
           track_type=OBJECT_POSITION; }
 return OK;
}

INT read_PivotPoint(_CHUNK_)
{
 float pos[3]; ARRAY(pos);
//x_pivo=pos[0]; y_pivot=-pos[2]; z_pivot=-pos[1];
 return OK;
}

INT SplineFlagsReader(FILE *f, word *nf)
{
 unsigned int i; float tens,cont,bias; word unknown,flags,nff;
 BLOCK(nff); BLOCK(unknown); BLOCK(flags); *nf=nff;
 for (i=0;i<16;i++) if (flags&(1<<i))
     switch (i) {
            case 0: BLOCK(tens); break;
            case 1: BLOCK(cont); break;
            case 2: BLOCK(bias); break;
            }
  return OK;
}

INT TrackFlagsReader(FILE *f, word *n)
{
 word flags[7]; ARRAY(flags); *n=flags[5]; return OK;
}

INT read_TrackPos(_CHUNK_)
{
 word n,nf; float pos[3]; if (TrackFlagsReader(f,&n)!=OK) return ERROR;

 if (!init && element_id>=0) switch (track_type) {

    case OBJECT_POSITION:
         world->object[element_id].pos_num=n;
         world->object[element_id].pt=nALLOC(PATH,n);
         world->object[element_id].px=nALLOC(PATH,n);
         world->object[element_id].py=nALLOC(PATH,n);
         world->object[element_id].pz=nALLOC(PATH,n); frame_id=0;
         break;

    case CAMERA_TARGET:
         world->camera[element_id].target_num=n;
         world->camera[element_id].tt=nALLOC(PATH,n);
         world->camera[element_id].tx=nALLOC(PATH,n);
         world->camera[element_id].ty=nALLOC(PATH,n);
         world->camera[element_id].tz=nALLOC(PATH,n); frame_id=0;
         break;

    case CAMERA_POSITION:
         world->camera[element_id].position_num=n;
         world->camera[element_id].pt=nALLOC(PATH,n);
         world->camera[element_id].px=nALLOC(PATH,n);
         world->camera[element_id].py=nALLOC(PATH,n);
         world->camera[element_id].pz=nALLOC(PATH,n); frame_id=0;
         break;
    }

 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; ARRAY(pos);

       if (!init && element_id>=0) switch (track_type) {

          case CAMERA_POSITION:
               world->camera[element_id].pt[frame_id]=nf;
               world->camera[element_id].px[frame_id]=pos[0];
               world->camera[element_id].py[frame_id]=pos[2];
               world->camera[element_id].pz[frame_id]=pos[1];
               break;

          case CAMERA_TARGET:
               world->camera[element_id].tt[frame_id]=nf;
               world->camera[element_id].tx[frame_id]=pos[0];
               world->camera[element_id].ty[frame_id]=pos[2];
               world->camera[element_id].tz[frame_id]=pos[1];
               break;

          case OBJECT_POSITION:
               world->object[element_id].pt[frame_id]=nf;
               world->object[element_id].px[frame_id]=pos[0];
               world->object[element_id].py[frame_id]=pos[2];
               world->object[element_id].pz[frame_id]=pos[1];
               break;
          }

       frame_id++;

       }
 return OK;
}

INT read_TrackColor(_CHUNK_)
{
 word n,nf; float pos[3]; if (TrackFlagsReader(f,&n)!=OK) return ERROR;
 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; ARRAY(pos);
       }
 return OK;
}

INT read_TrackRot(_CHUNK_)
{
 word n,nf; float pos[4]; if (TrackFlagsReader(f,&n)!=OK) return ERROR;

 if (!init && element_id>=0 && track_type==OBJECT_POSITION) {
    world->object[element_id].rot_num=n;
    world->object[element_id].qt=nALLOC(PATH,n);
    world->object[element_id].qw=nALLOC(PATH,n);
    world->object[element_id].qx=nALLOC(PATH,n);
    world->object[element_id].qy=nALLOC(PATH,n);
    world->object[element_id].qz=nALLOC(PATH,n); frame_id=0;
    }

 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; ARRAY(pos);
       if (!init && element_id>=0 && track_type==OBJECT_POSITION) {
          world->object[element_id].qt[frame_id]=nf;
          world->object[element_id].qw[frame_id]=pos[0];
          world->object[element_id].qx[frame_id]=pos[1];
          world->object[element_id].qy[frame_id]=pos[3];
          world->object[element_id].qz[frame_id]=pos[2];
          frame_id++;
          }
       }
 return OK;
}

INT read_TrackFOV(_CHUNK_)
{
 word n,nf; float pos; if (TrackFlagsReader(f,&n)!=OK) return ERROR;
 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; BLOCK(pos);
       }
 return OK;
}

INT read_TrackRoll(_CHUNK_)
{
 word n,nf; float pos; if (TrackFlagsReader(f,&n)!=OK) return ERROR;
 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; BLOCK(pos);
       }
 return OK;
}

INT read_TrackScale(_CHUNK_)
{
 word n,nf; float pos[3]; if (TrackFlagsReader(f,&n)!=OK) return ERROR;
 while (n-->0) {
       if (SplineFlagsReader(f,&nf)!=OK) return ERROR; ARRAY(pos);
       }
 return OK;
}

INT read_TrackCamera(_CHUNK_)
{
 track_type=CAMERA_POSITION; return read_Chunk(f,p);
}

INT read_TrackCamTgt(_CHUNK_)
{
 track_type=CAMERA_TARGET; return read_Chunk(f,p);
}

INT read_TrackInfo(_CHUNK_)
{
 track_type=UNKNOWN; return read_Chunk(f,p);
}

// ------------------------------------

struct {

 word id; const char *name; int sub; int (*func)(_CHUNK_);

} ChunkNames[] = {

// Chunk ID,       Chunk Description,   1=Sub-chunks,  Chunk Reader

  {CHUNK_VER,         "Version",                0, read_Short},
  {CHUNK_RGBF,        "RGB float",              0, read_RGBF},
  {CHUNK_RGBB,        "RGB byte",               0, read_RGBB},
  {CHUNK_RGBB2,       "RGB byte 2",             0, read_RGBB},
  {CHUNK_AMOUNTOF,    "Amount",                 0, read_Word},
  {CHUNK_MASTERSCALE, "Master scale",           0, read_Float},
  {CHUNK_PRJ,         "Project",                1, read_NULL},
  {CHUNK_MLI,         "Material Library",       1, read_NULL},
  {CHUNK_MESHVER,     "Mesh version",           0, read_Short},
  {CHUNK_MAIN,        "Main",                   1, read_NULL},
  {CHUNK_OBJMESH,     "Object Mesh",            1, read_NULL},
  {CHUNK_BKGCOLOR,    "Background color",       1, read_NULL},
  {CHUNK_AMBCOLOR,    "Ambient color",          1, read_NULL},
  {CHUNK_OBJBLOCK,    "Object Block",           1, read_ObjBlock},
  {CHUNK_TRIMESH,     "Tri-Mesh",               1, read_TriMesh},
  {CHUNK_VERTLIST,    "Vertex list",            0, read_VertList},
  {CHUNK_VERTFLAGS,   "Vertex flags",           0, read_VertFlags},
  {CHUNK_FACELIST,    "Face list",              1, read_FaceList},
  {CHUNK_FACEMAT,     "Face material",          0, read_FaceMat},
  {CHUNK_MAPLIST,     "Mappings list",          0, read_MapList},
  {CHUNK_SMOOLIST,    "Smoothings",             0, read_SmooList},
  {CHUNK_TRMATRIX,    "Matrix",                 0, read_TrMatrix},
  {CHUNK_MESHCOLOR,   "Mesh Color",             0, read_Skip},
  {CHUNK_TXTINFO,     "TxtInfo",                0, read_Skip},
  {CHUNK_BOXMAP,      "BoxMap",                 0, read_NULL},
  {CHUNK_LIGHT,       "Light",                  1, read_Light},
  {CHUNK_SPOTLIGHT,   "Spotlight",              0, read_SpotLight},
  {CHUNK_CAMERA,      "Camera",                 0, read_Camera},
  {CHUNK_HIERARCHY,   "Hierarchy",              1, read_NULL},
  {CHUNK_VIEWPORT,    "Viewport info",          0, read_Skip},
  {CHUNK_MATERIAL,    "Material",               1, read_NULL},
  {CHUNK_MATNAME,     "Material name",          0, read_MatName},
  {CHUNK_AMBIENT,     "Ambient color",          1, read_NULL},
  {CHUNK_DIFFUSE,     "Diffuse color",          1, read_NULL},
  {CHUNK_SPECULAR,    "Specular color",         1, read_NULL},
  {CHUNK_SHININESS,   "Shiness",                1, read_NULL},
  {CHUNK_SHINSTRENGTH,"Shiness strength",       1, read_NULL},
  {CHUNK_TRANSPARENCY,"Transparency",           1, read_NULL},
  {CHUNK_TRANSFALLOFF,"Falloff",                1, read_NULL},
  {CHUNK_REFBLUR,     "Reflection blur",        1, read_NULL},
  {CHUNK_SELFILLUM,   "Self illumination",      1, read_NULL},
  {CHUNK_TWOSIDED,    "Twosided",               0, read_Skip},
  {CHUNK_TRANSADD,    "TransAdd",               0, read_Skip},
  {CHUNK_WIREON,      "Wire On",                0, read_Skip},
  {CHUNK_WIRESIZE,    "Wire Size",              0, read_Float},
  {CHUNK_SOFTEN,      "Soften",                 0, read_Skip},
  {CHUNK_SHADING,     "Shading",                0, read_Word},
  {CHUNK_TEXTURE,     "Texture map",            1, read_Skip},
  {CHUNK_REFLECT,     "Reflection map",         1, read_Skip},
  {CHUNK_BUMPMAP,     "Bump map",               1, read_Skip},
  {CHUNK_BUMPAMOUNT,  "Bump amount",            0, read_Word},
  {CHUNK_MAPFHANDLE,  "Map filename",           0, read_MapFile},
  {CHUNK_MAPFLAGS,    "Map flags",              0, read_Word},
  {CHUNK_MAPBLUR,     "Map blurring",           0, read_Float},
  {CHUNK_MAPUSCALE,   "Map U scale",            0, read_Float},
  {CHUNK_MAPVSCALE,   "Map V scale",            0, read_Float},
  {CHUNK_MAPUOFFSET,  "Map U offset",           0, read_Float},
  {CHUNK_MAPVOFFSET,  "Map V offset",           0, read_Float},
  {CHUNK_MAPROTANGLE, "Map rot angle",          0, read_Float},
  {CHUNK_KEYFRAMER,   "Keyframer data",         1, read_NULL},
  {CHUNK_AMBIENTKEY,  "Ambient key",            1, read_NULL},
  {CHUNK_TRACKINFO,   "Track info",             1, read_TrackInfo},
  {CHUNK_FRAMES,      "Frames",                 0, read_Frames},
  {CHUNK_TRACKOBJNAME,"Track obj. name",        0, read_TrackObjName},
  {CHUNK_DUMMYNAME,   "Dummy name",             0, read_NULL},
  {CHUNK_TRACKPIVOT,  "Pivot point",            0, read_PivotPoint},
  {CHUNK_TRACKPOS,    "Position keys",          0, read_TrackPos},
  {CHUNK_TRACKCOLOR,  "Color track",            0, read_TrackColor},
  {CHUNK_TRACKROTATE, "Rotation keys",          0, read_TrackRot},
  {CHUNK_TRACKSCALE,  "Scale keys",             0, read_TrackScale},
  {CHUNK_TRACKMORPH,  "Morph track",            0, read_NULL},
  {CHUNK_TRACKHIDE,   "Hide track",             0, read_NULL},
  {CHUNK_OBJNUMBER,   "Object number",          0, read_ObjNumber},
  {CHUNK_TRACKCAMERA, "Camera track",           1, read_TrackCamera},
  {CHUNK_TRACKCAMTGT, "Camera target track",    1, read_TrackCamTgt},
  {CHUNK_TRACKLIGHT,  "Pointlight track",       1, read_NULL},
  {CHUNK_TRACKLIGTGT, "Pointlight target track",1, read_NULL},
  {CHUNK_TRACKSPOTL,  "Spotlight track",        1, read_NULL},
  {CHUNK_TRACKFOV,    "FOV track",              0, read_TrackFOV},
  {CHUNK_TRACKROLL,   "Roll track",             0, read_TrackRoll},
};

INT FindChunk (word id)
{
 int i;
 for (i=0;i<(sizeof(ChunkNames)/sizeof(ChunkNames[0]));i++)
     if (id==ChunkNames[i].id) return i;
 return ERROR;
}

INT read_Chunk(_CHUNK_)
{
 TChunkHeader h; long pc; int n,res;

 while (ftell(f)<p) {
       pc=ftell(f); BLOCK(h);
       if (!h.len) return ERROR; n=FindChunk(h.id);
       if (n<0) fseek(f,pc+h.len,SEEK_SET); else { pc+=h.len;
          if (ChunkNames[n].func) {
             res=ChunkNames[n].func(f,pc);
             if (res!=OK) return res;
             }
          if (ChunkNames[n].sub) {
             res=read_Chunk(f,pc);
             if (res!=OK) return res;
             }
          fseek(f,pc,SEEK_SET);
          }
       if (ferror(f)) return ERROR;
       }

 return OK;
}

// ------------------------------------

extern WORLD *load_3DS(char *fn)
{
 long p; int i; FILE *f;

 world=ALLOC(WORLD);
 world->material_num=0;
 world->camera_num=0;
 world->object_num=0;

 #define READ f=fopen(fn,"rb"); if (!f) return NULL; \
              fseek(f,0,SEEK_END); p=ftell(f); \
              fseek(f,0,SEEK_SET); i=read_Chunk(f,p); \
              fclose (f); if (i!=OK) return NULL;

 init=1; READ

 world->material=nALLOC(MATERIAL,world->material_num);
 world->object=nALLOC(OBJECT,world->object_num);
 world->camera=nALLOC(CAMERA,world->camera_num);

 memset(world->material,0,sizeof(MATERIAL)*world->material_num);
 memset(world->object,0,sizeof(OBJECT)*world->object_num);
 memset(world->camera,0,sizeof(CAMERA)*world->camera_num);

 init=0; object_id=0; camera_id=0; material_id=0; READ

 return world;
}