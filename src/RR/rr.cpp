#include <assert.h>
#include <io.h>
#include <limits.h>   //INT_MAX
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//#define SUPPORT_KAMIL
//#define SUPPORT_Z
//#define SUPPORT_REGEX

#ifdef SUPPORT_Z
 #include <zlib.h>
#endif

//#define NDEBUG       // no asserts, no debug code
//#define ONE          // rr+core+geometry+interpol+dynamic in ONE module (20% faster)
//#define RASTERGL     // raster+video via openGL
#define LOGARITMIC_TGA // v tga jsou zlogaritmovane hodnoty
//#define ARCTAN_COLOR // spocitany jas pred zobrazenim prozene pres arctan()
#define DBG(a) //a
#define WAIT //fgetc(stdin) // program ceka na stisk, pro ladeni spotreby pameti
#define MAX_UNINTERACT_TIME 2 // max waiting for response with glut/opengl 2sec

#ifdef RASTERGL
 #include "videogl.h"
 #include "rastergl.h"
 #include <GL/glut.h>
 #define GLMINUS(A) (-(A))
#else
 #include "video.h"
 #include "raster.h"
 #include "subglut.h"
 #define GLMINUS(A) (A)
#endif

#ifdef ONE
 #include "geometry.cpp"
 #include "intersections.cpp"
 #include "core.cpp"
  #include "interpol.cpp"
 #ifdef SUPPORT_DYNAMIC
  #include "dynamic.cpp"
 #endif
#else
 #include "geometry.h"
 #include "intersections.h"
 #include "core.h"
  #include "interpol.h"
 #ifdef SUPPORT_DYNAMIC
  #include "dynamic.h"
 #endif
#endif

#ifdef RASTERGL
#ifdef SUPPORT_LIGHTMAP
 #error accelerated lightmaps not supported
#endif
#endif

#include "ldmgf.h"
#include "ldbsp.h"
#include "misc.h"
#include "rrengine.h"
#include "surface.h"
#include "world2rrengine.h"

WORLD  *__world=NULL;
MATRIX  __identity;
int __obj=0,__mirror=0,*__mirrorOutput;

//////////////////////////////////////////////////////////////////////////////
//
// drawing
//

// global variables for drawing to speed up recursive calls

// c=calculation params
bool    c_useClusters=true;     // vyrobit a pouzit clustery?
bool    c_fightNeedles=false;   // specialne hackovat jehlovite triangly?
bool    c_staticReinit=false;   // pocita se kazdy snimek samostatne?
bool    c_dynamic=false;        // pocita dynamicke osvetleni misto statickeho?
real    c_frameTime=0.5;        // cas na jeden snimek (zvetsujici se pri nemenne geometrii a kamere)
real    c_dynamicFrameTime=0.5; // zvetsujici se cas na jeden dynamickej snimek

// d=draw params
#ifdef ARCTAN_COLOR
real    d_gamma=1;
real    d_bright=10;
#else
real    d_gamma=0.35;
real    d_bright=1.2;
#endif
bool    d_fast=false;           // max speed, low quality
bool    d_gouraud=true;
bool    d_gouraud3=false;
char    d_needle=0;             // 0=pink 1=retusovat jehly (pomale kresleni)
char    d_meshing=2;            // 0=source faces, 1=reflector meshing, 2=receiver meshing
real    d_details=256;
bool    d_pointers=false;       // jako barvu pouzit pointer na subtriangle
Node   *d_factors2=NULL;        // zobrazi faktory do nodu
byte    d_forceSides=0;         // 0 podle mgf, 1=vse zobrazi 1sided, 2=vse zobrazi 2sided
int     d_only_o=-1;            // draw only object o, -1=all

// p=play params
bool    p_flyingCamera=false;   // kamera jezi po draze
bool    p_flyingObjects=false;  // objekty jezdi po draze
bool    p_clock=false;          // frame se zvysuej podle hodin, ne o konstantu
int     p_ffPlay=0;             // vsechny snimky se loadujou 1=z 000, 2=z tga
bool    p_ffGrab=false;         // vsechny snimky se pocitaj a grabujou do p_ffName s priponou cislo snimku (napr .000)
char    p_ffName[256];          // jmeno souboru se scenou bez pripony (napr. bsp\jidelna)

real    p_3dsFrameStart=0;
real    p_3dsFrameEnd=100;
real    p_3dsFrameStep=2;       // prehravani jde po tolika 3ds framech
real    p_3dsFrame=0;           // cislo aktualniho framu podle 3ds

// g=grab params
int     g_tgaFrames=40;         // grabovani jde do tolika framu krat g_lights
int     g_tgaFrame=0;
int     g_lights=1;             // pocet svetel ve scene
bool    g_separLights=true;     // kazde svetlo grabovat zvlast (sestavit z nich sekvenci)
int     g_maxVertices=10000;    // nejvyse tolik nejdulezitejsich vertexu se uklada
int     g_batchGrab=0;          // !=0 nagrabuj vsechny snimky a skonci
int     g_batchGrabOne=-1;      // >=0 grabni jen tento frame (pro vsechny svetla)
int     g_batchMerge=0;         // !=0 spoj vsechny snimky do tga a skonci

// n=internal flags
bool    n_dirtyCamera=false;    // uzivatel pohnul kamerou
bool    n_dirtyObject=false;    // uzivatel pohnul nejakym objektem
bool    n_dirtyColor=true;      // zmenily se barvy ve scene
bool    n_dirtyGeometry=true;   // zmenila se geometrie 2d sceny

char *bp(char *fmt, ...)
{
 static char msg[1000];
 va_list argptr;
 va_start (argptr,fmt);
 vsprintf (msg,fmt,argptr);
 va_end (argptr);
 return msg;
}

void gzfprintf(void *f, char *fmt, ...)
{
#ifdef SUPPORT_Z
 static char msg[1024];
 va_list argptr;
 va_start (argptr,fmt);
 vsprintf (msg,fmt,argptr);
 va_end (argptr);
 gzwrite(f,msg,strlen(msg));
#endif
}

bool saving_tga=false; // flag ktery je nastaven behem ukladani tga
const int g_logScale=15; // rozumny kompromis, vejdou se i svetla nekolikrat jasnejsi nez 255

real getBrightness(real color) // converts 0..infinity radiosity to 0..1 value for display
{
  assert(IS_NUMBER(color));
  if(color<0) return 0; //dynamicky stin muze zpusobit ze prijde zaporna radiosita
  assert(color>=0);

  if(p_ffPlay==2)
  {
  #ifdef LOGARITMIC_TGA
    // pri prehravani tga hodnotu odlogaritmuje
    color=(exp(color*g_logScale)-1)/255;
  #endif
  }
  else // pri prehravani z tga nekoriguje jas protoze uz ulozil zkorigovany
  {
    // standardni korekce jasu
    assert(color>=0);
    if(d_gamma!=1) color=pow(color,d_gamma);
    color*=d_bright;
#ifdef ARCTAN_COLOR
    color=atan(color)/(M_PI/2);
#endif
  }

  if(saving_tga)
  {
  #ifdef LOGARITMIC_TGA
    // pri ukladani tga hodnotu zlogaritmuje
    color=log(color*255+1)/g_logScale;
  #endif
  }

  //if(!d_meshing) if(color<0.2 || color>0.95) return (rand()%256)/256.;
  if(color>1) return 1;
  if(color<0) return 0;
  assert(IS_NUMBER(color));
  return color;
}

static ColorTable __needle_ct=new unsigned[C_INDICES];

void SubTriangle::drawFlat(real ambient,int df)
{
  if(flags&FLAG_DIRTY_ALL_SUBNODES) df|=DF_REFRESHALL;
  if(!(df&DF_REFRESHALL) && !(flags&FLAG_DIRTY_NODE)) return;
  flags&=~FLAGS_DIRTY;
  bool is_destination=(this==d_factors2)
          || (d_factors2 && IS_CLUSTER(d_factors2) && IS_TRIANGLE(this) && CLUSTER(d_factors2)->contains(TRIANGLE(this)));
  real tmp=0;
  bool is_source=d_factors2 && shooter && (tmp=shooter->contains(d_factors2))!=-1;
  //if(is_source) printf("pwr=%f ",tmp);//!!!
  if(is_destination || is_source) ; else
  if((!d_fast || (df&DF_TOLIGHTMAP)) && sub[0] && ((d_meshing==1 && sub[0]->shooter) || d_meshing==2))
  {
     ambient+=(energyDirect+getEnergyDynamic()-sub[0]->energyDirect-sub[1]->energyDirect)/area;
     SUBTRIANGLE(sub[0])->drawFlat(ambient,df);
     SUBTRIANGLE(sub[1])->drawFlat(ambient,df);
     return;
  }
  if(d_factors2 && !is_source && !is_destination) return;

  real brightness=getBrightness(ambient+(energyDirect+getEnergyDynamic())/area);

#ifdef RASTERGL

  Point3 v[3];

  v[0]=to3d(0);
  v[1]=to3d(1);
  v[2]=to3d(2);

  raster_ZFlat(v,grandpa->surface->diffuseReflectanceColorTable,brightness);

#else

  raster_POINT p[3];
  raster_POLYGON p1,p2,p3;

  p1.point=&p[0]; p1.next=&p2;
  p2.point=&p[1]; p2.next=&p3;
  p3.point=&p[2]; p3.next=NULL;

 #ifdef SUPPORT_LIGHTMAP
  if(df&DF_TOLIGHTMAP)
  {
    assert(grandpa);
    int WIDTH=grandpa->lightmap.w;
    real zoom=grandpa->zoomToLightmap;

    real x1=uv[0].x*zoom;
    real y1=uv[0].y*zoom;
    real x2=uv[1].x*zoom;
    real y2=uv[1].y*zoom;
    real x3=uv[2].x*zoom;
    real y3=uv[2].y*zoom;

   #ifndef NDEBUG
    int HEIGHT=grandpa->lightmap.h;
    assert(x1>=0 && x1<WIDTH && y1>=0 && y1<HEIGHT);
    assert(x2>=0 && x2<WIDTH && y2>=0 && y2<HEIGHT);
    assert(x3>=0 && x3<WIDTH && y3>=0 && y3<HEIGHT);
   #endif

    p[0].sx=x1; p[0].sy=y1;
    p[1].sx=x2; p[1].sy=y2;
    p[2].sx=x3; p[2].sy=y3;

    raster_LFlat(&p1,WIDTH,grandpa->lightmap.bitmap,brightness);
  }
  else
 #endif

  {
    Vertex v;

    v=to3d(0); p[0].x=v.x; p[0].y=v.y; p[0].z=v.z;
    v=to3d(1); p[1].x=v.x; p[1].y=v.y; p[1].z=v.z;
    v=to3d(2); p[2].x=v.x; p[2].y=v.y; p[2].z=v.z;

    if(d_pointers)
    {
            unsigned col=(unsigned)this;
            raster_ZFlat(&p1,&col,0);
    }
    else
    if(is_destination)
    {
            unsigned col=255;
            raster_ZFlat(&p1,&col,0);
    }
    else
    if(is_source)
    {
            unsigned col=255<<16;
            raster_ZFlat(&p1,&col,0);
    }
    else
    raster_ZFlat(&p1,grandpa->surface->diffuseReflectanceColorTable,brightness);
  }

#endif
}


// ambient propaguje dolu jen kvuli gouraud3 at vi barvu stredu subtrianglu

void SubTriangle::drawGouraud(real ambient,IVertex **iv,int df)
{
  if(flags&(FLAG_DIRTY_ALL_SUBNODES+FLAG_DIRTY_SOME_SUBIVERTICES)) df|=DF_REFRESHALL;
  if(!(df&DF_REFRESHALL) && !(flags&(FLAG_DIRTY_NODE+FLAG_DIRTY_IVERTEX))) return;
  flags&=~FLAGS_DIRTY;
  assert(iv[0]);
  assert(iv[1]);
  assert(iv[2]);
  assert(iv[0]->check());
  assert(iv[1]->check());
  assert(iv[2]->check());
  assert(iv[0]->check(to3d(0)));
  assert(iv[1]->check(to3d(1)));
  assert(iv[2]->check(to3d(2)));
  if((!d_fast || (df&DF_TOLIGHTMAP)) && sub[0] && ((d_meshing==1 && sub[0]->shooter) || d_meshing==2)
  && (p_ffPlay!=2 || subvertex->error<d_details)
  && (p_ffPlay==0 || subvertex->loaded))//kdyz ffPlayuje=loaduje z disku, loaduje jen nektery ivertexy, ostatni zustanou nevyplneny a kdybysme se jich ptali, sectou si a vratej nam energii z corneru kde ted zadna neni
  {
     ambient+=(energyDirect+getEnergyDynamic()-sub[0]->energyDirect-sub[1]->energyDirect)/area;
     IVertex *iv0[3];
     IVertex *iv1[3];
     bool rightleft=isRightLeft();
     int rot=getSplitVertex();
     assert(subvertex);
     assert(subvertex->check());
     iv0[0]=iv[rot];
     iv0[1]=iv[(rot+1)%3];
     iv0[2]=subvertex;
     iv1[0]=iv[rot];
     iv1[1]=subvertex;
     iv1[2]=iv[(rot+2)%3];
     SUBTRIANGLE(sub[rightleft?0:1])->drawGouraud(ambient,iv0,df);
     SUBTRIANGLE(sub[rightleft?1:0])->drawGouraud(ambient,iv1,df);
     return;
  }

#ifdef RASTERGL

  Point3 v[3];

  v[0]=to3d(0);
  v[1]=to3d(1);
  v[2]=to3d(2);

  real brightness[3];

  brightness[0]=getBrightness(iv[0]->radiosity());
  brightness[1]=getBrightness(iv[1]->radiosity());
  brightness[2]=getBrightness(iv[2]->radiosity());

  raster_ZGouraud(v,grandpa->surface->diffuseReflectanceColorTable,brightness);

#else

  raster_POINT p[4];
  raster_POLYGON p1,p2,p3;

  p1.next=&p2;
  p2.next=&p3;
  p3.next=NULL;

 #ifdef SUPPORT_LIGHTMAP
  if(df&DF_TOLIGHTMAP)
  {

    int WIDTH=grandpa->lightmap.w;
    real zoom=grandpa->zoomToLightmap;
    real x1=uv[0].x*zoom;
    real y1=uv[0].y*zoom;
    real x2=uv[1].x*zoom;
    real y2=uv[1].y*zoom;
    real x3=uv[2].x*zoom;
    real y3=uv[2].y*zoom;

  //  #define OMEZ(var,min,max) if(var<min) var=min; else if(var>=max) var=max-0.01
  //  OMEZ(x1,0,WIDTH);OMEZ(y1,0,HEIGHT);
  //  OMEZ(x2,0,WIDTH);OMEZ(y2,0,HEIGHT);
  //  OMEZ(x3,0,WIDTH);OMEZ(y3,0,HEIGHT);
  //  #undef OMEZ

   #ifndef NDEBUG
    int HEIGHT=grandpa->lightmap.h;
    assert(x1>=0 && x1<WIDTH && y1>=0 && y1<HEIGHT);
    assert(x2>=0 && x2<WIDTH && y2>=0 && y2<HEIGHT);
    assert(x3>=0 && x3<WIDTH && y3>=0 && y3<HEIGHT);
   #endif

    p[1].sx=x1; p[1].sy=y1; p[1].tz=1; p[1].u=getBrightness(iv[0]->radiosity());
    p[2].sx=x2; p[2].sy=y2; p[2].tz=1; p[2].u=getBrightness(iv[1]->radiosity());
    p[3].sx=x3; p[3].sy=y3; p[3].tz=1; p[3].u=getBrightness(iv[2]->radiosity());

    if(!d_gouraud3)
    {
      p1.point=&p[1];
      p2.point=&p[2];
      p3.point=&p[3];

      raster_LGouraud(&p1,WIDTH,grandpa->lightmap.bitmap);
    }
    else
    {
      real xc=(uv[0].x+(u2.x+v2.x)/3)*zoom;
      real yc=(uv[0].y+(u2.y+v2.y)/3)*zoom;

      p[0].sx=xc; p[0].sy=yc; p[0].tz=1;
      p[0].u=getBrightness(ambient+(energyDirect+getEnergyDynamic())/area);

      p1.point=&p[1];
      p2.point=&p[2];
      p3.point=&p[0];

      raster_LGouraud(&p1,WIDTH,grandpa->lightmap.bitmap);

      p1.point=&p[1];
      p2.point=&p[0];
      p3.point=&p[3];

      raster_LGouraud(&p1,WIDTH,grandpa->lightmap.bitmap);

      p1.point=&p[0];
      p2.point=&p[2];
      p3.point=&p[3];

      raster_LGouraud(&p1,WIDTH,grandpa->lightmap.bitmap);
    }

  }
  else
 #endif
  {

    Vertex v;

    v=to3d(0);
    p[1].x=v.x; p[1].y=v.y; p[1].z=v.z;
    p[1].u=getBrightness(iv[0]->radiosity());

    v=to3d(1);
    p[2].x=v.x; p[2].y=v.y; p[2].z=v.z;
    p[2].u=getBrightness(iv[1]->radiosity());

    v=to3d(2);
    p[3].x=v.x; p[3].y=v.y; p[3].z=v.z;
    p[3].u=getBrightness(iv[2]->radiosity());

    if(!d_gouraud3)
    {
      p1.point=&p[1];
      p2.point=&p[2];
      p3.point=&p[3];

      raster_ZGouraud(&p1,(d_needle==0 && grandpa->isNeedle)?__needle_ct:grandpa->surface->diffuseReflectanceColorTable);
    }
    else
    {
      v=to3d(uv[0]+(u2+v2)/3);
      p[0].x=v.x; p[0].y=v.y; p[0].z=v.z;
      p[0].u=getBrightness(ambient+(energyDirect+getEnergyDynamic())/area);

      p1.point=&p[1];
      p2.point=&p[2];
      p3.point=&p[0];

      raster_ZGouraud(&p1,grandpa->surface->diffuseReflectanceColorTable);

      p1.point=&p[1];
      p2.point=&p[0];
      p3.point=&p[3];

      raster_ZGouraud(&p1,grandpa->surface->diffuseReflectanceColorTable);

      p1.point=&p[0];
      p2.point=&p[2];
      p3.point=&p[3];

      raster_ZGouraud(&p1,grandpa->surface->diffuseReflectanceColorTable);

    }

  }

#endif
}

unsigned SubTriangle::printGouraud(void *f, IVertex **iv, real scale,real flatambient)
{
 if(sub[0])
 {
  IVertex *iv0[3];
  IVertex *iv1[3];
  bool rightleft=isRightLeft();
  int rot=getSplitVertex();
  assert(subvertex);
  assert(subvertex->check());
  iv0[0]=iv[rot];
  iv0[1]=iv[(rot+1)%3];
  iv0[2]=subvertex;
  iv1[0]=iv[rot];
  iv1[1]=subvertex;
  iv1[2]=iv[(rot+2)%3];
  flatambient+=(energyDirect+getEnergyDynamic()-sub[0]->energyDirect-sub[1]->energyDirect)/area;
  return SUBTRIANGLE(sub[rightleft?0:1])->printGouraud(f,iv0,scale,flatambient)+
         SUBTRIANGLE(sub[rightleft?1:0])->printGouraud(f,iv1,scale,flatambient);
 }

 if (f) {

 real u[3],v[3],b[3]; unsigned i[3];

 u[0]=uv[0].x*scale;
 v[0]=uv[0].y*scale;
 u[1]=uv[1].x*scale;
 v[1]=uv[1].y*scale;
 u[2]=uv[2].x*scale;
 v[2]=uv[2].y*scale;

 if (flatambient) {
    // flat
    b[0]=b[1]=b[2]=getBrightness(flatambient+(energyDirect+getEnergyDynamic())/area);
 }  else {
    // gouraud
    b[0]=getBrightness(iv[0]->radiosity());
    b[1]=getBrightness(iv[1]->radiosity());
    b[2]=getBrightness(iv[2]->radiosity());
 }

 i[0]=(3-grandpa->rotations)%3;
 i[1]=(4-grandpa->rotations)%3;
 i[2]=(5-grandpa->rotations)%3;

 gzfprintf(f,"        subFace\n");
 gzfprintf(f,"        {\n");
 gzfprintf(f,"          vertices [ %f %f 0, %f %f 0, %f %f 0]\n",u[i[0]],v[i[0]],u[i[1]],v[i[1]],u[i[2]],v[i[2]]);
 gzfprintf(f,"          intensities [ %f, %f, %f]\n",b[i[0]],b[i[1]],b[i[2]]);
 gzfprintf(f,"        }\n");

 }

 return 1;
}


#ifdef SUPPORT_LIGHTMAP

// aktualizuje velikost lightmapy

void Triangle::setLightmapSize(unsigned w)
{
  if(lightmap.w==w) return;
  if(w==0)
  {
    lightmap.setSize(0,0);
    return;
  }
  assert(uv[0].x==0 && uv[0].y==0);
  zoomToLightmap=(w-1)/MAX(u2.x,v2.x);
  //pokud vychazi h treba 5.9999,
  // u2,v2 v subtrianglu semtam nepatrne naroste a melo by se kreslit do 6,
  // proto musime udelat vysku 7 (tedy pricist 1+MAXERROR)
  int h=(int)(MAX(u2.y,v2.y)*zoomToLightmap+1.5);

  // kdyz by byla lightmapa moc rozplacla, neni potreba ji zvetsovat,
  // proste se vypusti a nakresli se to primo do screenu
  if(h<8)
  {
    lightmap.setSize(0,0);
    return;
  }

  lightmap.setSize(w,h);

  // spocita souradnice ktery se budou predavat texturemapperu
  lightmap.uv[0]=Point2(0,0);assert(uv[0].x==0);assert(uv[0].y==0);
  lightmap.uv[1]=uv[1]*zoomToLightmap;
  lightmap.uv[2]=uv[2]*zoomToLightmap;
/*
  // a ohackuje je
  lightmap.uv[0].y+=0.1;
  lightmap.uv[1].y+=0.1;
  lightmap.uv[2].y-=2.1;

  if(lightmap.uv[2].x<lightmap.uv[1].x*0.3) {lightmap.uv[0].x+=1;lightmap.uv[1].x-=1;lightmap.uv[2].x+=1;} else
  if(lightmap.uv[2].x>lightmap.uv[1].x*0.7) {lightmap.uv[0].x+=1;lightmap.uv[1].x-=1;lightmap.uv[2].x-=1;} else
                                  {lightmap.uv[0].x+=1;lightmap.uv[1].x-=1;}
*/
  // proveri ze nic nebude pretejkat
  assert(IS_VEC2(lightmap.uv[0]));
  assert(IS_VEC2(lightmap.uv[1]));
  assert(IS_VEC2(lightmap.uv[2]));
  assert(lightmap.uv[0].x>=0 && lightmap.uv[0].x<lightmap.w && lightmap.uv[0].y>=0 && lightmap.uv[0].y<lightmap.h);
  assert(lightmap.uv[1].x>=0 && lightmap.uv[1].x<lightmap.w && lightmap.uv[1].y>=0 && lightmap.uv[1].y<lightmap.h);
  assert(lightmap.uv[2].x>=0 && lightmap.uv[2].x<lightmap.w && lightmap.uv[2].y>=0 && lightmap.uv[2].y<lightmap.h);

  flags|=FLAG_DIRTY_ALL_SUBNODES;
}

// podle poctu subtrianglu nastavi vhodnou velikost lightmapy
// pri dost malem mnozstvi ji i zrusi
// problem: po reinitu je plno subtrianglu ale prazdnejch

void Triangle::updateLightmapSize(bool forExport)
{
//  if(d_fast) return;
#if 1
  const int lightmapSizes=6;
  const unsigned lightmapWidth[lightmapSizes]=  {0,16,32,64,128,256    };
  const unsigned maxSubsForWidth[lightmapSizes]={1,6,24,100,400,INT_MAX};
#else
  const int lightmapSizes=4;
  const unsigned lightmapWidth[lightmapSizes]=  { 0,64,128,256    };
  const unsigned maxSubsForWidth[lightmapSizes]={10,40,120,INT_MAX};
#endif
  // v prvnim poli jsou mozne sirky lightmapy
  // druhe pole rika kolik nejvys subtrianglu se do tak siroke lightmapy
  //  vejde (kdyz se nevejdou, nastavi se vetsi sirka)
  // dusledek: prvni prvek druheho pole rika pri kolika jeste subtrianglech
  //  se triangl kresli primo do screeny bez pouziti lightmapy
  // pri exportu se lightmapa generuje uz pri 2 subtrianglech
  for(int i=0;i<lightmapSizes;i++)
    if(subtriangles<= ((!i&&forExport)?1:maxSubsForWidth[i]) )
    {
      setLightmapSize(lightmapWidth[i]);
      break;
    }
}

// aktualizuje obsah lightmapy

void Triangle::updateLightmap()
{
  #ifdef SUPPORT_DYNAMIC
  if(c_dynamic && d_drawDynamicHits) setLightmapSize(64);
  #endif

//  if (lightmap.isClean || n_dirtyColor)
//     memset(lightmap.bitmap,0,lightmap.w*lightmap.h);

  real ambient=radiosityIndirect();
  int df=DF_TOLIGHTMAP+(n_dirtyColor?DF_REFRESHALL:0);
  if(d_gouraud) drawGouraud(ambient,topivertex,df); else
  drawFlat(ambient,df);

  #ifdef SUPPORT_DYNAMIC
  // naflaka do lightmapy hity
  if(c_dynamic && d_drawDynamicHits)
  {
    hits.convertDHitsToLightmap(&lightmap,zoomToLightmap);
  }
  #endif

/*tenhle hack nekdy lightmapu divne kazi, konkretne dyn.stiny
  if (lightmap.isClean || n_dirtyColor) {
     byte c;
     for (int y=0;y<lightmap.h;y++) {
         int yw=y*lightmap.w;
         for (int x=0;x<lightmap.w;x++) {
             if ((c=lightmap.bitmap[x+yw])) {
                for (int i=0;i<x;i++) lightmap.bitmap[i+yw]=c;
                break;
                }
             }
         for (int x=lightmap.w-1;x>=0;x--) {
             if ((c=lightmap.bitmap[x+yw])) {
                for (int i=lightmap.w-1;i>x;i--) lightmap.bitmap[i+yw]=c;
                break;
                }
             }
         }
     for (int x=0;x<lightmap.w;x++) {
         int w=lightmap.w;
         for (int y=0;y<lightmap.h;y++) {
             if ((c=lightmap.bitmap[x+y*w])) {
                for (int i=0;i<y;i++) lightmap.bitmap[x+i*w]=c;
                break;
                }
             }
         for (int y=lightmap.h-1;y>=0;y--) {
             if ((c=lightmap.bitmap[x+y*w])) {
                for (int i=lightmap.h-1;i>y;i--) lightmap.bitmap[x+i*w]=c;
                break;
                }
             }
         }
     lightmap.isClean=false;
     }*/
  lightmap.isClean=false;
}

// kresli celou lightmapu

void Triangle::drawLightmap()
{
  assert(lightmap.w);

  if(!d_fast || lightmap.isClean) updateLightmap();

  raster_POINT p1,p2,p3;
  raster_VERTEX v1,v2,v3;
  raster_POLYGON *p;

  p1.x=vertex[0]->x; p2.x=vertex[1]->x; p3.x=vertex[2]->x;
  p1.y=vertex[0]->y; p2.y=vertex[1]->y; p3.y=vertex[2]->y;
  p1.z=vertex[0]->z; p2.z=vertex[1]->z; p3.z=vertex[2]->z;

  v1.point=&p1; v1.next=&v2;
  v2.point=&p2; v2.next=&v3;
  v3.point=&p3; v3.next=NULL; p=&v1;

  p1.u=lightmap.uv[0].x; p2.u=lightmap.uv[1].x; p3.u=lightmap.uv[2].x;
  p1.v=lightmap.uv[0].y; p2.v=lightmap.uv[1].y; p3.v=lightmap.uv[2].y;
  assert(p1.u>=0);
  assert(p1.u<lightmap.w);
  assert(p2.u>=0);
  assert(p2.u<lightmap.w);
  assert(p3.u>=0);
  assert(p3.u<lightmap.w);
  assert(p1.v>=0);
  assert(p1.v<lightmap.h);
  assert(p2.v>=0);
  assert(p2.v<lightmap.h);
  assert(p3.v>=0);
  assert(p3.v<lightmap.h);
  raster_ZTexture(p,lightmap.w,lightmap.bitmap,surface->diffuseReflectanceColorTable);
}

void save_lightmaps(WORLD *w)
{
 d_fast=false; int oldNeedle=d_needle; d_needle=1;
 video_WriteScreen("saving lightmaps...");

 for (int i=0;i<w->object_num;i++) { OBJECT *o=&w->object[i];

     for (int j=0;j<o->face_num;j++) { char obj_name[256],log_name[256];

         strcpy(obj_name,w->material[o->face[j].material].name+1);
         sprintf(log_name,"%s/lmap.log",obj_name);
         FILE *lightmap_log=fopen(log_name,"a");

         if (lightmap_log) { char face_name[256];

            Triangle *t=(Triangle *)o->face[j].source_triangle;

            if (!t) fprintf(lightmap_log,"%d INVALID!\n",o->face[j].id); else
            if (!t->surface) /*fprintf(lightmap_log,"%d hidden\n",o->face[j].id)*/; else {

               sprintf(face_name,"%s/f%05d.png",obj_name,o->face[j].id);

               t->updateLightmapSize(true);

               if (t->lightmap.w) {
                  t->updateLightmap();
                  fprintf(lightmap_log,"%d %f %f %f %f %f %f\n",o->face[j].id,
                    t->lightmap.uv[(3-t->rotations)%3].x,t->lightmap.uv[(3-t->rotations)%3].y, // (n-rots)%3 ... compensate rotations done by Triangle::setGeometry
                    t->lightmap.uv[(4-t->rotations)%3].x,t->lightmap.uv[(4-t->rotations)%3].y,
                    t->lightmap.uv[(5-t->rotations)%3].x,t->lightmap.uv[(5-t->rotations)%3].y); 
                  t->lightmap.Save(face_name);
                  t->lightmap.setSize(0,0);
                } else {
                  fprintf(lightmap_log,"%d %f 0 %f 0 %f 0\n",o->face[j].id,
                  getBrightness(t->topivertex[0]->radiosity()),
                  getBrightness(t->topivertex[1]->radiosity()),
                  getBrightness(t->topivertex[2]->radiosity()));
                  }
               }

            fclose(lightmap_log);

            }
         }
     }

 video_WriteScreen("done! ");
 d_needle=oldNeedle;
}

#endif //SUPPORT_LIGHTMAP

#ifdef SUPPORT_REGEX
// regular expression match
// http://publibn.boulder.ibm.com/doc_link/en_US/a_doc_lib/aixprggd/genprogc/internationalized_reg_expression_subr.htm
#include <regex.h>
bool expmatch(char *str,char *pattern,regex_t *re)
{
        int status=regcomp(re,pattern,REG_EXTENDED);
        if(status) return false;
        status=regexec(re,str,0,NULL,0);
        return status==0;
}
#else
// returns if str matches expression exp (exp may start or end with *, funk matches "funk" "*nk" "f*")
bool expmatch(char *str,char *exp)
{
        if(!exp) return !str;
        if(!str) return false;
        if(!strcmp(str,exp)) return true;
        if(!exp[0]) return false;
        if(exp[0]=='*') return strlen(str)>=strlen(exp)-1 && !strcmp(str+strlen(str)-strlen(exp)+1,exp+1);
        if(exp[strlen(exp)-1]=='*') return strlen(str)>=strlen(exp)-1 && !memcmp(str,exp,strlen(exp)-1);
        return false;
}
#endif

char *__hidematerial1=NULL;
char *__hidematerial2=NULL;
char *__hidematerial3=NULL;
char *__exportmaterial="*";
char *__dontexportmaterial=NULL;
byte  __infolevel=1;

void save_subtriangles(WORLD *w)
{
#ifdef SUPPORT_Z
 d_fast=false; int oldNeedle=d_needle; d_needle=1; 
 if(__infolevel) video_WriteScreen("saving sub-triangles...");

 void *f=gzopen(bp("%s.log",p_ffName),"wb9");

 for (int k=0;k<w->material_num;k++) { char *m=w->material[k].name;

     if(!expmatch(m,__exportmaterial)) continue;
     if(expmatch(m,__dontexportmaterial)) continue;

     gzfprintf(f,"Mesh\n");
     gzfprintf(f,"{\n");

     gzfprintf(f,"  name ""%s""\n",m+1); unsigned num=0; if(__infolevel) video_WriteScreen(m);

     for (int i=0;i<w->object_num;i++) { OBJECT *o=&w->object[i];
     for (int j=0;j<o->face_num;j++) if (o->face[j].material==k) num++; }

     gzfprintf(f,"  faceN %d\n",num);
     gzfprintf(f,"  faces\n");
     gzfprintf(f,"  [\n");

     for (int i=0;i<w->object_num;i++) { OBJECT *o=&w->object[i];
     for (int j=0;j<o->face_num;j++) if (o->face[j].material==k) {

         Triangle *t=(Triangle *)o->face[j].source_triangle;

         gzfprintf(f,"    face\n");
         gzfprintf(f,"    {\n");
         gzfprintf(f,"      index %d\n",o->face[j].id);
         
         if (!t) {

         gzfprintf(f,"      INVALID!\n");

         } else if (!t->surface) {

         //gzfprintf(f,"      hidden\n");

         } else {

         // normalize uv
         real scale=1/MAX(MAX(t->uv[1].x,t->uv[2].x),t->uv[2].y); // max(u,v)==1
         // real scale=1/MAX(t->uv[1].x,t->uv[2].x); // max(u)==1, max(v)<1.11
         assert(t->uv[1].y==0);

         gzfprintf(f,"      vertices [ %f %f 0, %f %f 0, %f %f 0]\n",
           t->uv[(3-t->rotations)%3].x*scale,t->uv[(3-t->rotations)%3].y*scale,
           t->uv[(4-t->rotations)%3].x*scale,t->uv[(4-t->rotations)%3].y*scale,
           t->uv[(5-t->rotations)%3].x*scale,t->uv[(5-t->rotations)%3].y*scale);

         num=t->printGouraud(0,t->topivertex,scale);

         gzfprintf(f,"      subFaceN %d\n",num);
         gzfprintf(f,"      subFaces\n");
         gzfprintf(f,"      [\n");
         
         t->printGouraud(f,t->topivertex,scale);

         gzfprintf(f,"      ]\n");

         }

         gzfprintf(f,"    }\n");

         }
         }

     gzfprintf(f,"  ]\n");
     gzfprintf(f,"}\n");
     
     }

 gzclose(f);

 if(__infolevel) video_WriteScreen("done! ");
 d_needle=oldNeedle;
#endif
}

// obecna fce na kresleni trianglu, pouzije tu metodu ktera je zrovna podporovana

void inline draw_triangle(Triangle *f)
{
 if(!f->surface) return;
#ifdef TEST_SCENE
 if (!f) return; // narazili jsme na vyrazeny triangl
#endif
#ifdef RASTERGL
 real ambient=f->radiosityIndirect();
 //teoreticky by do flagu stacilo dat (n_dirtyColor || n_dirtyGeometry)?DF_REFRESHALL:0
 //ale praktikcy tam jsou jeste nejaky hacky, takze radsi refreshneme vzdy vse
 if(d_gouraud) f->drawGouraud(ambient,f->topivertex,DF_REFRESHALL); else
 f->drawFlat(ambient,DF_REFRESHALL);
#else
 #ifdef SUPPORT_LIGHTMAP
 if(p_ffPlay) f->updateLightmapSize(false);
 if(f->lightmap.w)
   f->drawLightmap();
 else
 #endif
 {
   real ambient=f->radiosityIndirect();
   if(d_gouraud
     #ifndef SUPPORT_LIGHTMAP
     && !d_fast
     #endif
     ) f->drawGouraud(ambient,f->topivertex,DF_REFRESHALL); else
   f->drawFlat(ambient,true);
 }
#endif
}

void matrix_Hierarchy(HIERARCHY *h, MATRIX m, float t)
{
 if(!h) return;

 OBJECT *o=h->object; float px,py,pz;
 MATRIX sm,im,tm; spline_MATRIX qm; matrix_Copy(m,sm);

 if (h->brother) matrix_Hierarchy(h->brother,sm,t);

 spline_Interpolate(&o->pos,t,&px,&py,&pz,0);
 spline_Interpolate(&o->rot,t,0,0,0,&qm);

 tm[0][0] = qm.xx; tm[1][0] = qm.xy; tm[2][0] = qm.xz; tm[3][0] = px;
 tm[0][1] = qm.yx; tm[1][1] = qm.yy; tm[2][1] = qm.yz; tm[3][1] = py;
 tm[0][2] = qm.zx; tm[1][2] = qm.zy; tm[2][2] = qm.zz; tm[3][2] = pz;
 tm[0][3] = 0;     tm[1][3] = 0;     tm[2][3] = 0;     tm[3][3] = 1;

 matrix_Mul(tm,o->matrix);
 matrix_Mul(sm,tm);
 matrix_Invert(sm,im);
 matrix_Copy(sm,o->transform);
 matrix_Copy(im,o->inverse);

 if (h->child) matrix_Hierarchy(h->child,sm,t);
}

void render_world(WORLD *w, int camera_id, bool mirrorFrame)
{
// TIME t0=GETTIME;
 MATRIX cm,im,om;

 for (int i=0;i<w->object_num;i++) {//if(/*d_only_o<0 ||*/ (i!=336 /*|| i==279*/) /*&& (i%100)==d_only_o*/) { 
   
     OBJECT *o=&w->object[i];
     Object *obj=(Object *)o->obj;
     assert(obj);

     if (!mirrorFrame || strcmp(o->name,"Zrcadlo")) {

if(camera_id>=0) 
{
#ifndef SUPPORT_DYNAMIC
        // vsechny objekty jsou ve scenespacu, vsechny matice jsou identity
        // takze by to tady mohlo jit o neco rychlejc i bez transformaci
#endif
        matrix_Copy(w->camera[camera_id].inverse,cm);
        matrix_Copy(o->transform,om);
        if (mirrorFrame) {
           om[3][0]=-om[3][0];
           om[2][0]=-om[2][0];
           om[1][0]=-om[1][0];
           om[0][0]=-om[0][0];
           }
        matrix_Mul(cm,om);
        matrix_Invert(cm,im);

#ifndef RASTERGL
        cm[0][0]*=video_XFOV; cm[1][0]*=video_XFOV;
        cm[2][0]*=video_XFOV; cm[3][0]*=video_XFOV;
        cm[0][1]*=video_YFOV; cm[1][1]*=video_YFOV;
        cm[2][1]*=video_YFOV; cm[3][1]*=video_YFOV;
#endif
        raster_SetMatrix(&cm,&im);
}
        //raster_BeginTriangles();
        for (unsigned j=0;j<obj->triangles;j++) /*if(!(j&3))*/{
            Normal *n=&obj->triangle[j].n3;
            byte fromOut=n->d+im[3][0]*n->x+im[3][1]*n->y+im[3][2]*n->z>0;
            if ((d_forceSides==0 && sideBits[obj->triangle[j].surface->sides][fromOut?0:1].renderFrom) ||
                (d_forceSides==1 && fromOut) ||
                (d_forceSides==2))
                   draw_triangle(&obj->triangle[j]);
            }

        //raster_EndTriangles();
        }
     }
/*
#ifndef RASTERGL
 for(int x=0;x<video_XRES;x++)
 for(int y=0;y<video_YRES;y++)
 {
        / *unsigned color=video_GetPixel(x,y);
        if(color>>24)
        {
                color=123456;
                video_PutPixel(x,y,color);
        }* /
        unsigned color=123456;
        Vec3 direction=;
        Color c={1,1,1};
        rayTraceCamera(eye,direction,NULL,color);
        real b=getBrightness(i);
        video_PutPixel(x,y,color);
 }
#endif
 */
// printf("render=%dms\n",GETTIME-t0);
}

SubTriangle *locate_subtriangle(WORLD *w, int x,int y)
{
	raster_Clear();
	bool old_gouraud=d_gouraud; d_gouraud=false;
	char old_meshing=d_meshing; d_meshing=2;
	d_pointers=true;
	render_world(w,0,false);
	d_pointers=false;
	d_meshing=old_meshing;
	d_gouraud=old_gouraud;
	return (SubTriangle*)video_GetPixel(x,y);
}
/*
void render_from_light(WORLD *w)
{
	// nastavit pohled ze svetla
	Point3 tmp1=Point3(0,1,0);
	raster_SetMatrix(tmp1,tmp1);
	raster_SetFOV(60,60);

	// rict	ze budem rendrovat jen Z
	bool old_gouraud=d_gouraud; d_gouraud=false;
	char old_meshing=d_meshing; d_meshing=2;
	glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

	// vyrendrovat scenu poprve, +queries, +Z, -Color
	queries.Reset(true);
	render_world(w,-1,false);

	// nastrkat do faces celou scenu
	SubTriangles faces;

	// pro vsechny facy zjistit jak	moc jsou videt
	while(faces.count)
	{
	}

	// obnovit puvodni stav
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	d_meshing=old_meshing;
	d_gouraud=old_gouraud;
}
*/
void Scene::infoScene(char *buf)
{
  int t=0,v=0;
  for(unsigned o=0;o<objects;o++)
  {
    t+=object[o]->triangles;
    v+=object[o]->vertices;
  }
  sprintf(buf,"vertices=%d triangles=%d objects=%d",v,t,objects);
}

void infoStructs(char *buf)
{
   int no=sizeof(Node);
   int cl=sizeof(Cluster);
   int su=sizeof(SubTriangle);
   int tr=sizeof(Triangle);
   int hi=sizeof(Hit);
   int fa=sizeof(Factor);
   int iv=0;
   int co=0;
   int ed=0;
   iv=sizeof(IVertex);
   co=sizeof(Corner);
   ed=sizeof(Edge);
   sprintf(buf,"no=%i,cl=%i,su=%i,tr=%i  hi=%i,fa=%i  iv=%i,co=%i,ed=%i)",no,cl,su,tr, hi,fa, iv,co,ed);
}

void Scene::infoImprovement(char *buf)
{
   int kb=MEM_ALLOCATED;
   int hi=sizeof(Hit)*__hitsAllocated;
   int fa=sizeof(Factor)*__factorsAllocated;
   int su=sizeof(SubTriangle)*(__subtrianglesAllocated-__trianglesAllocated);
   int tr=sizeof(Triangle)*__trianglesAllocated;
   int cl=sizeof(Cluster)*__clustersAllocated;
   int iv=0;
   int co=0;
   int li=0;
   iv=sizeof(IVertex)*__iverticesAllocated;
   co=sizeof(Corner)*__cornersAllocated;
#ifdef SUPPORT_LIGHTMAP
   li=__lightmapsAllocated;
#endif
   int ot=kb-hi-fa-su-tr-cl-iv-co-li;
   buf[0]=0;
   if(p_ffGrab) sprintf(buf+strlen(buf),"grab(%i/%i) ",g_tgaFrame,g_tgaFrames*g_lights);
   if(__infolevel>1) sprintf(buf+strlen(buf),"hits(%i/%i) ",__hitsOuter,__hitsInner);
#ifdef SUPPORT_DYNAMIC
   if(__infolevel>1) sprintf(buf+strlen(buf),"dshots(%i->%i) ",__lightShotsPerDynamicFrame,__shadeShotsPerDynamicFrame);
#endif
   //sprintf(buf+strlen(buf),"kb=%i",kb/1024);
   if(__infolevel>1) sprintf(buf+strlen(buf),"(hi=%i,fa=%i,su=%i,tr=%i,cl=%i,iv=%i,co=%i,li=%i,ot=%i)",
     hi/1024,fa/1024,su/1024,tr/1024,cl/1024,iv/1024,co/1024,li/1024,ot/1024);
   sprintf(buf+strlen(buf),"gamma=%0.3f bright=%0.3f",d_gamma,d_bright);
  // sprintf(buf+strlen(buf),"ib=%f ",(double)improveBig);
  // sprintf(buf+strlen(buf),"ii=%f ",(double)improveInaccurate);
   sprintf(buf+strlen(buf)," meshes=%i/%i rays=(%i)%i",staticReflectors.nodes,__nodesAllocated,shotsTotal,shotsForFactorsTotal);
   if(improvingStatic) sprintf(buf+strlen(buf),"(%i/%i->%i)",shotsAccumulated,improvingStatic->shooter->shotsForFactors,shotsForNewFactors);
   assert((improvingStatic!=NULL) == (phase!=0));
}

void Scene::draw(real quality)
{
 float backgroundColor[3]={0,0,0};
  //spravne ma byt =(material ve kterem je kamera)->color;
 video_Clear(backgroundColor);

#ifndef RASTERGL
 if (__mirror) {
    int *o=raster_Output;
    raster_Output=__mirrorOutput;
    raster_Clear();
    render_world(__world,0,true);
    raster_Output=o;
    raster_Clear();
    render_world(__world,0,false);
    int *m=__mirrorOutput,*s=m+video_XRES*video_YRES;
    while (m<s) {
          U8 reflect_power=(*o)>>24;
          switch (reflect_power) {
            case 255: *o=*m;
            case 0: break;
            default:
              U32 direct=*o;
              U32 reflect=*m;
              U8 direct_power=255-reflect_power;
              #define COMPONENT(mask) ( ( ((direct&mask)*direct_power+(reflect&mask)*reflect_power) /255) & mask)
              *o=COMPONENT(0xff0000)+COMPONENT(0xff00)+COMPONENT(0xff);
          }
          m++; o++;
    }
  } else
#endif
  {
    raster_Clear();
    render_world(__world,0,false);
  }

 if(__infolevel && (g_batchGrabOne<0 || g_batchGrabOne==g_tgaFrame%g_tgaFrames))
 {
   char buf[400];
   infoImprovement(buf);
   video_WriteBuf(buf);
 }

 mouse_hide();
 video_Blit();
 mouse_show();
 n_dirtyGeometry=false;
 n_dirtyColor=false;

 // je-li treba, smaze vsechny hity (konkretne tem trianglum co byly zasazeny
 //  ale ne kresleny, takze jim hity nikdo nesmazal)
 if(!improvingStatic)
 {
   Triangle *hitTriangle;
   while((hitTriangle=hitTriangles.get())) hitTriangle->hits.reset();
 }
}

void matrix_Create(CAMERA *camera, float t)
{
 static float tx,ty,tz,px,py,pz;

 spline_Interpolate(&camera->pos,t,&px,&py,&pz,0);
 spline_Interpolate(&camera->tar,t,&tx,&ty,&tz,0);

 Vec3 world_up=Vec3(0,GLMINUS(1),0);
 Vec3 view_dir=normalized(Vec3(GLMINUS(tx-px),GLMINUS(ty-py),GLMINUS(tz-pz)));
 Vec3 right=normalized(ortogonalTo(view_dir,world_up));
 Vec3 up=normalized(ortogonalTo(view_dir,right));

 matrix_Init(camera->matrix);
 camera->matrix[0][0]=right.x;
 camera->matrix[0][1]=right.y;
 camera->matrix[0][2]=right.z;
 camera->matrix[1][0]=up.x;
 camera->matrix[1][1]=up.y;
 camera->matrix[1][2]=up.z;
 camera->matrix[2][0]=view_dir.x;
 camera->matrix[2][1]=view_dir.y;
 camera->matrix[2][2]=view_dir.z;
 camera->matrix[3][0]=px;
 camera->matrix[3][1]=py;
 camera->matrix[3][2]=pz;
}

//////////////////////////////////////////////////////////////////////////////
//
// mgf input
//

void fillColorTable(unsigned *ct,double cx,double cy,real rs)
{
 for(unsigned c=0;c<C_INDICES;c++)
 {
   real rgb[3];
   xy2rgb(cx,cy,c/255.,rgb);
   #define FLOAT2BYTE(f) ((unsigned)((f)<0?0:(f)>=1?255:256*(f)))
   ct[c]=(FLOAT2BYTE(rs)<<24) + (FLOAT2BYTE(rgb[0])<<16) + (FLOAT2BYTE(rgb[1])<<8) + FLOAT2BYTE(rgb[2]);
 }
}

/*
ColorTable createColorTable(double cx,double cy,real rs)
{
 ColorTable ct=new unsigned[C_INDICES];
 fillColorTable(ct,cx,cy,rs);
 return ct;
}

void fillSurface(Surface *s,C_MATERIAL *m)
{
 s->sides                =(m->sided==1)?1:2;
 s->diffuseReflectance   =m->rd;
 xy2rgb(m->rd_c.cx,m->rd_c.cy,0.5,s->diffuseReflectanceColor);
 s->diffuseReflectanceColorTable=createColorTable(m->rd_c.cx,m->rd_c.cy,m->rs);
 s->diffuseTransmittance =m->td;
 xy2rgb(m->td_c.cx,m->td_c.cy,0.5,s->diffuseTransmittanceColor);
 s->diffuseEmittance     =m->ed/1000;
 xy2rgb(m->ed_c.cx,m->ed_c.cy,0.5,s->diffuseEmittanceColor);
 s->diffuseEmittanceType =areaLight;
 s->diffuseEmittancePoint=Point3(0,0,0);
 s->specularReflectance  =m->rs;
 s->specularTransmittance=m->ts;
 s->refractionReal       =m->nr;
 s->refractionImaginary  =m->ni;
 s->outside              =NULL;
 s->inside               =NULL;
 s->texture              =NULL;
 s->_rd                  =m->rd;
 s->_rdcx                =m->rd_c.cx;
 s->_rdcy                =m->rd_c.cy;
 s->_ed                  =m->ed/1000;
}

unsigned mgf_surfaces;
unsigned mgf_surfaces_loaded;
Scene   *mgf_scene;

void *add_surface(C_MATERIAL *m)
{
 #ifdef TEST_SCENE
 if(mgf_surfaces_loaded<0 || mgf_surfaces_loaded>=mgf_scene->surfaces) {mgf_surfaces_loaded++;return NULL;}
 #endif
 Surface *s=&mgf_scene->surface[mgf_surfaces_loaded++];
 fillSurface(s,m);
 //printf("Filled surface[%d] rs=%f\n",mgf_surfaces_loaded-1,s->specularReflectance);
 return s;
}

//////////////////////////////////////////////////////////////////////////////
//
// data conversion (WORLD -> Scene)
//

OBJECT *cbsp_o1;
Object *cbsp_o2;

bool convert_BspTree(BspTree *tree)
{
	assert(tree);
	assert(tree->size);

	unsigned endoffset=(unsigned)tree+tree->size;
	bool front=tree->front;
	bool back=tree->back;
	// subtrees
	tree++;
	if(front) 
	{
		if(!convert_BspTree(tree)) return false;
		tree=tree->next();
	}
	if(back)
	{
		if(!convert_BspTree(tree)) return false;
		tree=tree->next();
	}
	// leaf, change triangle index to triangle pointer
	while((unsigned)tree<endoffset)	
	{
		unsigned tri=*(unsigned *)tree;
		assert(tri>=0 && tri<(unsigned)cbsp_o1->face_num);
		#ifdef TEST_SCENE
		if(tri<0 || tri>=(unsigned)cbsp_o1->face_num) return false;
		#endif
		//*(Triangle **)tree=&cbsp_o2->triangle[tri]; nerespektuje jine poradi trianglu (pri SUPPORT_DYNAMIC) proti facum
		*(void **)tree=cbsp_o1->face[tri].source_triangle;
		tree++;
	}
	return true;
}

OBJECT *ckd_o1;
Object *ckd_o2;

bool convert_KdTree(KdTree *tree)
{
	assert(tree);
	assert(tree->size);

	int axis=tree->axis;
	if(axis!=3) 
	{
		// subtrees, change splitVertexNum to splitValue
		tree->splitValue=ckd_o2->vertex[tree->splitVertexNum][tree->axis];
		tree++;
		if(!convert_KdTree(tree)) return false;
		tree=tree->next();
		if(!convert_KdTree(tree)) return false;
		tree=tree->next();
	} else {
		// leaf, change triangle index to triangle pointer
		for(unsigned i=0;&(tree->leafTriangleNum[i])<tree->getTrianglesEnd();i++)	
		{
			unsigned tri=tree->leafTriangleNum[i];
			assert(tri>=0 && tri<(unsigned)ckd_o1->face_num);
			#ifdef TEST_SCENE
			if(tri<0 || tri>=(unsigned)ckd_o1->face_num) return false;
			#endif
			tree->leafTrianglePtr[i]=(Triangle*)ckd_o1->face[tri].source_triangle;
		}
	}
	return true;
}

bool convert_BspTree(OBJECT *o1,TObject *o2)
{
	o2->bspTree=(BspTree *)o1->bsp_tree;
	cbsp_o1=o1;
	cbsp_o2=o2;
	return convert_BspTree((BspTree *)o1->bsp_tree);
}

bool convert_KdTree(OBJECT *o1,TObject *o2)
{
	o2->kdTree=(KdTree *)o1->kd_tree;
	ckd_o1=o1;
	ckd_o2=o2;
	return convert_KdTree((KdTree *)o1->kd_tree);
}


#ifdef TEST_SCENE
int duplicit=0;
int verticesKilled=0;//unused
#endif

Scene *convert_world2scene(WORLD *w,char *material_mgf)
{
	WAIT;
	Scene *scene=new Scene;
	// load	surfaces from mgf
	DBG(printf("Loading surfaces...\n"));
	scene->surfaces=w->material_num;
	scene->surface=new Surface[scene->surfaces];
	for(unsigned i=0;i<scene->surfaces;i++)	scene->surface[i].sides=0;//mark that surface is not filled
	mgf_scene=scene;
	mgf_surfaces_loaded=0;
	ldmgf(material_mgf,NULL,add_surface,NULL,NULL,NULL);// nacte knihovnu materialu	(stejne_jmeno.mgf)
	//static Surface s_light={1,0.3,{1,1,1},createColorTable(0.3,0.3,0),9.5,{1,1,1},9.5,{1,1,1},areaLight,Vec3(0,0,0),0,{1,1,1},0.5,0,{1,1,1},0.5,1,0,NULL,NULL,NULL, 0.3,0.3,0.3,9.5};
	static Surface s_default;//={1,0.3,{1,1,1},createColorTable(0.3,0.3,0),0,{1,1,1},0,{1,1,1},areaLight,Vec3(0,0,0),0,{1,1,1},0.5,0,{1,1,1},0.5,1,0,NULL,NULL,NULL, 0.3,0.3,0.3,0};
	s_default.sides=2; // 1 if surface is 1-sided, 2 for 2-sided
	s_default.diffuseReflectance=0.3;
	s_default.diffuseReflectanceColor[0]=1;
	s_default.diffuseReflectanceColor[1]=1;
	s_default.diffuseReflectanceColor[2]=1;
	s_default.diffuseReflectanceColorTable=createColorTable(0.3,0.3,0);
	s_default.diffuseTransmittance=0;
	//s_default.diffuseTransmittanceColor={1,1,1};
	s_default.diffuseEmittance=0;
	s_default.diffuseEmittanceColor[0]=1;
	s_default.diffuseEmittanceColor[1]=1;
	s_default.diffuseEmittanceColor[2]=1;
	s_default.diffuseEmittanceType=areaLight;
	s_default.diffuseEmittancePoint=Vec3(0,0,0);
	s_default.specularReflectance=0;
	s_default.specularReflectanceColor[0]=1;
	s_default.specularReflectanceColor[1]=1;
	s_default.specularReflectanceColor[2]=1;
	s_default.specularReflectanceRoughness=0.5;
	s_default.specularTransmittance=0;
	//s_default.specularTransmittanceColor={1,1,1};
	s_default.specularTransmittanceRoughness=0.5;
	s_default.refractionReal=1;
	s_default.refractionImaginary=0;
	s_default.outside=NULL;
	s_default.inside=NULL;
	s_default.texture=NULL;
	s_default._rd=0.3;//needed when calculating different illumination for different components
	s_default._rdcx=0.3;
	s_default._rdcy=0.3;
	s_default._ed=0;//needed by turnLight

 // load geometry from world
 DBG(printf("Loading geometry..."));
 for (int o=0;o<w->object_num;o++) {
     DBG(printf("Object %s(%d,%d)\n",w->object[o].name,w->object[o].vertex_num,w->object[o].face_num));
     TObject *obj=new TObject(w->object[o].vertex_num,w->object[o].face_num);

//     matrix_Init(w->object[o].matrix);

     // prevede vertexy
     DBG(printf(" vertices...\n"));
     for (int v=0;v<w->object[o].vertex_num;v++) {
         w->object[o].vertex[v].id=v;
         obj->vertex[v].x=w->object[o].vertex[v].x;
         obj->vertex[v].y=w->object[o].vertex[v].y;
         obj->vertex[v].z=w->object[o].vertex[v].z;
         #ifdef TEST_SCENE
         //if(v<10000) // too slow for big scenes
         //  for (int i=0;i<v;i++)
         //    if (obj->vertex[v]==obj->vertex[i]) {duplicit++;break;}
         #endif
         }
     #ifndef SUPPORT_DYNAMIC
     // ve statickem modu prevadi vertexy do scenespacu
     for (int v=0;v<w->object[o].vertex_num;v++) {
         obj->vertex[v].transform(&w->object[o].transform);//=Vec3(w->object[o].vertex[v].x,w->object[o].vertex[v].y,w->object[o].vertex[v].z).transformed(&w->object[o].transform);
         w->object[o].vertex[v].x=obj->vertex[v].x;
         w->object[o].vertex[v].y=obj->vertex[v].y;
         w->object[o].vertex[v].z=obj->vertex[v].z;
     }
     // ve statickem modu dela z matic identity
     matrix_Copy(__identity,w->object[o].transform);
     matrix_Copy(__identity,w->object[o].inverse);
     #endif
     // prevede facy na triangly
     // od nuly nahoru insertuje emitory, od triangles-1 dolu ostatni
     DBG(printf(" triangles...\n"));
     int tbot=0;
#ifdef SUPPORT_DYNAMIC
     int ttop=obj->triangles-1;
#endif
     for (int fi=0;fi<w->object[o].face_num;fi++) {
         FACE *f=&w->object[o].face[fi];

         // zjisti a nastavi pouzity material
         Surface *s;
         #ifdef TEST_SCENE
         if(f->material<0 || f->material>=w->material_num)
         {
             printf("# Invalid material #%d (valid 0..%d).\n",f->material,w->material_num-1);
             //assert(0); neassertit aby neexistujici material prosel i v debug verzi
             s=&s_default;
         } else
         #endif
         {
             if(  (__hidematerial1 && expmatch(w->material[f->material].name,__hidematerial1)) 
               || (__hidematerial2 && expmatch(w->material[f->material].name,__hidematerial2))
               || (__hidematerial3 && expmatch(w->material[f->material].name,__hidematerial3)))
             {
               s=NULL;
             }
             else
             if(scene->surface[f->material].sides==0) // not filled yet
             {
                 C_MATERIAL *m=c_getmaterial(w->material[f->material].name);
                 #ifdef TEST_SCENE
                 if(!m) {
                   printf("# Unknown material %s.\n",w->material[f->material].name);
                   s=&s_default;
                 } else 
                 #endif
                 {
                   s=&scene->surface[f->material];
                   fillSurface(s,m);
                   //printf("Filled surface[%d]=%s rs=%f\n",f->material,w->material[f->material].name,s->specularReflectance);
                   // hacky pro diskosvetlo a dveresvetlo
                   if(!strcmp("LI_DI",w->material[f->material].name))
                   {
                     s->diffuseEmittanceType=nearLight;
                   }
                   if(!strcmp("LI_DV",w->material[f->material].name))
                   {
                     s->diffuseEmittanceType=nearLight2;//nahardcodovany dvere
                   }
                   // hack pro zrcadlo
                   //!if(s->specularReflectance>0) __mirror=1;
                   //printf(" news=%i",(int)s);
                 }
             }
             else
             {
                 s=&scene->surface[f->material];
             }
         }
         // rozhodne zda vlozit face dolu mezi emitory nebo nahoru mezi ostatni
         Triangle *t;
         #ifdef SUPPORT_DYNAMIC
         if(s && s->diffuseEmittance) t=&obj->triangle[tbot++]; else t=&obj->triangle[ttop--];
         #else
         t=&obj->triangle[tbot++];
         #endif
         assert(t>=obj->triangle && t<&obj->triangle[obj->triangles]);
         // vlozi ho, seridi geometrii atd
         Normal n;
         n.x=f->normal.a;
         n.y=f->normal.b;
         n.z=f->normal.c;
         n.d=f->normal.d;
         int geom=t->setGeometry(
            &obj->vertex[f->vertex[0]->id],
            &obj->vertex[f->vertex[1]->id],
            &obj->vertex[f->vertex[2]->id]
            //,&n
            );
         if(geom>=0)
         {
           // geometrie je v poradku
           if(s) obj->energyEmited+=fabs(t->setSurface(s));
           f->source_triangle=t;
#ifndef SUPPORT_DYNAMIC
           // ve STATIC modu ztransformuje ve WORLDu normaly
           f->normal.a=t->n3.x;
           f->normal.b=t->n3.y;
           f->normal.c=t->n3.z;
           f->normal.d=t->n3.d;
#endif
         } else {
           // geometrie je invalidni, trojuhelnik zahazujem
           printf("# Removing invalid triangle %d in object %d (reason %d), disabling BSP!\n",fi,o,geom);
           printf("  [%.2f %.2f %.2f] [%.2f %.2f %.2f] [%.2f %.2f %.2f]\n",w->object[o].vertex[f->vertex[0]->id].x,w->object[o].vertex[f->vertex[0]->id].y,w->object[o].vertex[f->vertex[0]->id].z,w->object[o].vertex[f->vertex[1]->id].x,w->object[o].vertex[f->vertex[1]->id].y,w->object[o].vertex[f->vertex[1]->id].z,w->object[o].vertex[f->vertex[2]->id].x,w->object[o].vertex[f->vertex[2]->id].y,w->object[o].vertex[f->vertex[2]->id].z);
           obj->bspTree=NULL; // invalid geometry -> invalid bspTree
           // kdTree is still valid, but is has to be parsed more carefully (slowly)
           f->source_triangle=NULL;
           --obj->triangles;
           #ifdef SUPPORT_DYNAMIC
           if(s->diffuseEmittance) tbot--;else ttop++;//undo insert
           byte tmp[sizeof(Triangle)];
           memcpy(tmp,&obj->triangle[obj->triangles],sizeof(Triangle));
           memcpy(&obj->triangle[obj->triangles],&obj->triangle[ttop],sizeof(Triangle));
           memcpy(&obj->triangle[ttop],tmp,sizeof(Triangle));
           ttop--;
           #else
           tbot--; //undo insert
           #endif
         }
     }
     // prevede bsp tree
     // nutno po nacteni trianglu (v dynamic je preradi) ale pred odalokovanim facu (obsahuji pointery na jinak razene triangly)
     DBG(printf(" bsp...\n"));
     if((unsigned)w->object[o].face_num==obj->triangles && !convert_BspTree(&w->object[o],obj))
     {
       printf("Invalid BSP tree, disabling BSP!\n");
       obj->bspTree=NULL;
     }
     DBG(printf(" kd...\n"));
     if(!convert_KdTree(&w->object[o],obj))
     {
       printf("Invalid KD tree, disabling KD!\n");
       obj->kdTree=NULL;
     }
     // smaze z worldu vertexy a facy
     //nemazat, facy pouziju v save_lightmaps
     //free(w->object[o].vertex); 
     //w->object[o].vertex=NULL;
     //free(w->object[o].face); 
     //w->object[o].face=NULL;

     #ifdef SUPPORT_DYNAMIC
     obj->trianglesEmiting=tbot;
     obj->transformMatrix=&w->object[o].transform;
     obj->inverseMatrix=&w->object[o].inverse;
     // vyzada si prvni transformaci
     obj->matrixDirty=true;
     #endif
     // preprocessuje objekt
     DBG(printf(" bounds...\n"));
     obj->detectBounds();
     #ifndef SUPPORT_INTERPOL
     if(c_useClusters) 
     #endif
     {
       DBG(printf(" edges...\n"));
       obj->buildEdges(); // build edges only for clusters and/or interpol
     }
     if(c_useClusters) 
     {
       DBG(printf(" clusters...\n"));
       obj->buildClusters(); 
       // clusters first, ivertices then (see comment in Cluster::insert)
     }
     #ifdef SUPPORT_INTERPOL
     DBG(printf(" ivertices...\n"));
     obj->buildTopIVertices();
     #endif
     // priradi objektu jednoznacny a pri kazdem spusteni stejny identifikator
     obj->id=o;
     obj->name=w->object[o].name;
     // vlozi objekt do sceny
     #ifdef SUPPORT_DYNAMIC
     if (w->object[o].pos.num!=1 || w->object[o].rot.num!=1) scene->objInsertDynamic(obj); else
     #endif
        scene->objInsertStatic(obj);
     w->object[o].obj=obj;
     }

 #ifdef TEST_SCENE
 bool buga=false;
 if(verticesKilled)
 {
     fprintf(stdout,"Nefatalni: polygony obsahuji celkem %i duplicitnich vertexu.\n",verticesKilled);
     buga=true;
 }
 if(duplicit)
 {
     fprintf(stdout,"Casto fatalni: scena obsahuje %i duplicitnich vertexu, to by nemela.\n",duplicit);
     fprintf(stdout," nektery moje algoritmy kolem interpolace to nerady.\n");
     buga=true;
 }
 if(__trianglesWithBadNormal)
 {
     fprintf(stdout,"Varovani: %i trianglu ma spatne nebo nepresne zadanou normalu.\n",__trianglesWithBadNormal);
     buga=true;
 }
 #endif

//printf("%i\n",w->camera[0].pos.keys[w->camera[0].pos.num-1].frame);
//printf("%i\n",__world->camera[0].pos.keys[__world->camera[0].pos.num-1].frame);
//    p_3dsFrameEnd=w->camera[0].pos.keys[w->camera[0].pos.num-1].frame;
//// scene_frames=world->camera[0].pos.keys[world->camera[0].pos.num-1].frame;
// p_3dsFrameEnd=w->camera->tar.keys[w->camera->tar.num-1].frame;
//printf("%i\n",p_3dsFrameEnd);

// scene->objRemoveStatic(336); //!!! hack kvuli fact_big.bsp
// scene->objRemoveStatic(279);
// for(int i=0;i<200;i++) scene->objRemoveStatic(i);

 WAIT;
 return scene;
}
*/

// zmeni dynamicky objekty na staticky

void objMakeAllStatic(Scene *scene)
{
 #ifdef SUPPORT_DYNAMIC
 for(unsigned o=scene->staticObjects;o<scene->objects;o++) scene->objMakeStatic(o);
 #endif
}

// vrati to zpatky, zas budou dynamicky

void objReturnDynamic(Scene *scene)
{
 #ifdef SUPPORT_DYNAMIC
 for(unsigned o=0;o<scene->objects;o++)
   if(__world->object[o].pos.num!=1 || __world->object[o].rot.num!=1)
   {
     unsigned ndx=scene->objNdx((TObject *)(__world->object[o].obj));
     if(ndx>=0 && ndx<scene->staticObjects) scene->objMakeDynamic(ndx);
   }
 #endif
}

//////////////////////////////////////////////////////////////////////////////
//
// misc
//

static TIME   endTime;

static void endAfter(real seconds)
{
 endTime=(TIME)(GETTIME+seconds*PER_SEC);
}

static bool endByTime(Scene *scene)
{
 return GETTIME>endTime;
}

static bool endByTimeOrInput(Scene *scene)
{
 return kb_hit() || GETTIME>endTime || mouse_hit();
}

static real   endAccuracy=-1;

/*static void endSetAccuracy(real accuracy)
{
 endAccuracy=accuracy;
}*/

#ifdef SUPPORT_KAMIL
#include "counter.h"
static Counter *cnt=0;
#endif

static int oAccuracy=0;

static bool endByAccuracy(Scene *scene)
{
 int Accuracy=(int)(100.0*scene->avgAccuracy()/endAccuracy);
 
 if (oAccuracy+5<=Accuracy) { oAccuracy=Accuracy;
#ifdef SUPPORT_KAMIL
    if (cnt) cnt->Next(); else 
#endif
    { char info[256];
       sprintf(info,"%d%%",Accuracy);
       video_WriteScreen(info);
       }
    }

#ifdef SUPPORT_KAMIL
 if (cnt && Accuracy>=100) cnt->End();
#endif
 return Accuracy>=100;
}

/*static bool endByAccuracy(Scene *scene)
{
 return scene->avgAccuracy()>endAccuracy;
}*/

static void captureTgaAfter(Scene *scene,char *name,real seconds,real minimalImprovementToShorten)
{
 endAfter(seconds);
 scene->improveStatic(endByTime);
 if (scene->shortenStaticImprovementIfBetterThan(minimalImprovementToShorten))
    scene->finishStaticImprovement();
 scene->draw(0.4);
 video_Grab(name);
 __frameNumber++;
}

void setRrMode(Scene *scene,bool staticreinit,bool dynamic,int drawdynamichits)
{
 if(scene->shortenStaticImprovementIfBetterThan(1)) scene->finishStaticImprovement();
 else scene->abortStaticImprovement();
 c_staticReinit=staticreinit;
 if(staticreinit) objMakeAllStatic(scene); else objReturnDynamic(scene);
 c_dynamic=dynamic;
 #ifdef SUPPORT_DYNAMIC
 d_drawDynamicHits=drawdynamichits;
 #endif
 p_ffGrab=false;
 p_ffPlay=0;
}

void fakMerge(Scene *scene,unsigned frames,unsigned maxvertices)
{
 char info[100];
 // naakumuluje pro kazdy ivertex jeho error
 scene->iv_cleanErrors();
 for(unsigned frame=0;frame<frames;frame++)
 {
   // nacita chyby z komponent
   for(int i=0;i<4;i++)
   {
     // pokud byla hlasena nejak chyba v batch modu, rovnou to zabali
     if(g_batchMerge && __errors) exit(__errors);

     char name[256];
     sprintf(name,"%s.%c.%03i",p_ffName,scene->selectColorFilter(i),frame);
     sprintf(info,"(kb=%i) accumulating %s...  ",(int)MEM_ALLOCATED/1024,name);
     video_WriteScreen(info);
     scene->iv_loadRealFrame(name);
     DBGLINE
     scene->iv_addLoadedErrors();
     DBGLINE
   }
 }
 // necha jen maxvertices s nejvetsim errorem
 sprintf(info,"(kb=%i) optimizing...             ",(int)MEM_ALLOCATED/1024);
 video_WriteScreen(info);
 scene->iv_markImportants_saveMeshing(maxvertices,bp("%s.mes",p_ffName));

 // postupne spocita a ulozi do .tga jednotlivy barevny komponenty
    // 1. mozny pristup k tomu aby cervena stena odrazela jen cervene svetlo
    //    energyDirect [25 vyskytu] rozdelit na energyDirectR/G/B
    //    radiosity() rozdelit na radiosityR/G/B()
    //    + bezny vypocet by jel ve vyssi kvalite
    //    - pomalejsi, zere vic pameti
    //    - upravy by byly naporad, nejde to udelat optional
    // 2. mozny pristup [YES!]
    //    pocitat dal v grayscalu a tady jen s pouzitim spoctenych faktoru
    //     3x prepocitat odrazivosti, predistribuovat energii a ulozit .tga
    //    - bezny vypocet pojede stale v grayscale
    //    + plna rychlost a pametova nenarocnost
    //    + zbytek programu nedotcen komplikovanou multikomponentnosti,
    //      stacilo pridat __colorfilter a upravit setSurface()
    // 1. mozny pristup k ukladani prekalkulaci do souboru [YES!]
    //    .000 delat v r,g,b mutacich
    //    + vhodne pri postupnem generovani tri .tga
    //    + nezvysi pametove naroky
    // 2. mozny pristup
    //    do .000 ulozit za sebe w,r,g,b mutace
    // 3. mozny pristup
    //    do .000 ukladat misto 1 realu vzdy 3 realy
    //    + vhodne i pri generovani truecolor .tga a .jpg
    //    + pri playovani 'b' lze menit jas/kontrast
    //    - nutno roztrojit energyDirect, sezere vic pameti

 saving_tga=true;
 for(int component=0;component<4;component++)
 {
   // postupne ulozi data ze vsech ivertexu
   scene->iv_startSavingBytes(frames,bp("%s.%c.tga",p_ffName,scene->selectColorFilter(component)));
   for(unsigned frame=0;frame<frames;frame++)
   {
     char name[256];
     sprintf(name,"%s.%c.%03i",p_ffName,scene->selectColorFilter(component),frame);
     sprintf(info,"(kb=%i) storing data from %s...  ",(int)MEM_ALLOCATED/1024,name);
     video_WriteScreen(info);
     scene->iv_loadRealFrame(name);
     scene->iv_fillEmptyImportants();
     scene->iv_saveByteFrame();
   }
   scene->iv_savingBytesDone();
 }
 saving_tga=false;

 // pokud bezi batch, ted skonci
 if(g_batchMerge) exit(__errors);
}

void fakStartLoadingBytes(Scene *scene,bool ora_filling,bool ora_reading)
{
 char namemes[256];
 sprintf(namemes,"%s.mes",p_ffName);
 if(ora_filling) ora_filling_init();
 if(ora_reading) ora_reading_init(bp("%s.ora",p_ffName));
 g_tgaFrames=scene->iv_initLoadingBytes(namemes,bp("%s.%c.tga",p_ffName,'w'))/g_lights;
 if(ora_reading) ora_reading_done();
 if(ora_filling) ora_filling_done(bp("%s.ora",p_ffName));
 g_tgaFrame=0;
}

//////////////////////////////////////////////////////////////////////////////
//
// displayed frames
//

// provede vsechny nezbytne transformace pred vypoctem nebo loadem aktualniho frame

void frameSetup(Scene *scene)
{
 if(p_ffGrab)
 {
   // jedine kdyz grabuje, g_tgaFrame prebiji hodnotu v p_3dsFrame
   p_3dsFrame=p_3dsFrameStart+(p_3dsFrameEnd-p_3dsFrameStart)*(g_tgaFrame%g_tgaFrames)/g_tgaFrames;
 }
 #ifdef SUPPORT_DYNAMIC
 if(p_flyingObjects)
 {
   for(unsigned o=scene->staticObjects;o<scene->objects;o++) scene->object[o]->matrixDirty=true;
 }
 // vzhledem k tomu ze tx,ty,tz ve vertex nevydrzej do dalsiho framu,
 // musime ztransformovat vzdy znova vsechny objekty
 for(unsigned o=0;o<scene->objects;o++) scene->object[o]->matrixDirty=true;

 if(p_flyingObjects || n_dirtyObject)
 {
   matrix_Hierarchy(__world->hierarchy,__identity,p_3dsFrame);
   n_dirtyObject=false;
 }
 #endif
 if(p_flyingCamera || n_dirtyCamera)
 {
   matrix_Invert(__world->camera[0].matrix,__world->camera[0].inverse);
   //n_dirtyCamera=false; musime jeste nechat dirty at vynuti prekresleni obrazu
 }
 #ifdef SUPPORT_DYNAMIC
 scene->transformObjects();
 #endif
 if(c_staticReinit)
 {
   scene->resetStaticIllumination(false);
   n_dirtyColor=true;
 }
}

// spocita nebo loadne aktualni frame
// vraci jestli se neco zmenilo a ma smysl volat frameDraw

bool  preparing_capture=false;

bool frameCalculate(Scene *scene)
{
 if(p_ffPlay)
 {
   real tgaFrame=(p_3dsFrame)/(p_3dsFrameEnd-p_3dsFrameStart)*g_tgaFrames;
   if(p_ffPlay==1)
     scene->iv_loadRealFrame(bp("%s.%c.%03i",p_ffName,'w',(int)tgaFrame));
   else
     scene->iv_loadByteFrame(tgaFrame);
   n_dirtyColor=true;
   return true;
 }

 if(p_flyingObjects || p_flyingCamera || n_dirtyGeometry) c_dynamicFrameTime=c_frameTime;
#ifdef RASTERGL
 // od glut 3.7 / win32 se nedovime ze user poslal event, musime pocitat po malejch chvilkach aby probihal glut mainloop
 if(!c_dynamic && !p_flyingObjects && !p_flyingCamera && n_dirtyGeometry) c_dynamicFrameTime=0.05;
 if(c_dynamicFrameTime>MAX_UNINTERACT_TIME) c_dynamicFrameTime=MAX_UNINTERACT_TIME;
#endif
 endAfter(c_dynamicFrameTime);

#ifdef SUPPORT_DYNAMIC
 if(c_dynamic)
 {
   scene->improveDynamic(endByTimeOrInput);
   if(endByTime(scene)) c_dynamicFrameTime*=1.5; // increase time only when previous time really elapsed (don't increase after each hit)
   n_dirtyColor=true;
   return true;
 }
 else
#endif
 {
   if(!p_flyingCamera && !p_flyingObjects && (n_dirtyCamera || n_dirtyObject)) return true; // jednorazova zmena sceny klavesnici nebo mysi -> okamzity redraw
   bool change=false;
   if(!preparing_capture && (g_batchGrabOne<0 || g_batchGrabOne==g_tgaFrame%g_tgaFrames)) {
     change=scene->improveStatic(endByTimeOrInput);
     if(endByTime(scene)) c_dynamicFrameTime*=1.5; // increase time only when previous time really elapsed (don't increase after each hit)
   }
   return change || p_flyingCamera || p_flyingObjects || n_dirtyCamera || n_dirtyObject;
 }
}

// vykresli a pripadne grabne aktualni frame

void frameDraw(Scene *scene)
{
// d_fast=!p_flyingObjects && !p_flyingCamera && n_dirtyGeometry;
 scene->draw(0.4);
 n_dirtyCamera=false;
 if(p_ffGrab && (g_batchGrabOne<0 || g_batchGrabOne==g_tgaFrame%g_tgaFrames))
 {
   for(int component=0;component<4;component++)
   {
     // vybere komponentu kterou bude ukladat
     // nastavi odrazivosti
     // nastavi svitivosti
     char c=scene->selectColorFilter(component);

     // vynuluje energie
     // nastavi energie k distribuci
     scene->resetStaticIllumination(true/*preserveFactors*/);

     // rozdistribuuje energii
     scene->distribute(0.00001);

     // zvysi cislo snimku aby radiosity() volana pri ukladani vedela ze nema
     //  vracet cislo z cache (udaj pro predchozi komponentu)
     __frameNumber++;

     //ulozi
     scene->iv_saveRealFrame(bp("%s.%c.%03i",p_ffName,c,g_tgaFrame));
   }
 }
 __frameNumber++;

 //jen na nagrabovani do diplomky:
 //static int id=0;static int __id=0;char name[20];
 //if(__id++>=4 && p_flyingObjects){__id=0;sprintf(name,"rr%d.tga",id++); video_Grab(name);}
}

// posune se na dalsi frame

void frameAdvance(Scene *scene)
{
 // kdyz dograboval posledni, prepne na play
 if(p_ffGrab && g_tgaFrame%g_tgaFrames==g_tgaFrames-1)
 {
   // pokud ale grabuje postupne pro kazdy svetlo zvlast,
   int light=g_tgaFrame/g_tgaFrames;
   if(light<g_lights-1)
   {
     // neni-li u posledniho, prepne na dalsi svetlo a projede sekvenci znova
     scene->turnLight(light,0);
     scene->turnLight(light+1,1);
     // energie svetlum nastavi resetStaticIllumination v frameSetup
   }
   else
   {
     // je-li u posledniho, rozsviti svetla
     for(int i=0;i<g_lights;i++) scene->turnLight(i,1);
     // a prepne na play
     p_ffGrab=false;
     p_ffPlay=1;

     // pokud bezi batch a mel jen grabovat, ted skonci
     if(g_batchGrab && !g_batchMerge) exit(__errors);

     // uplne nakonec nasimuluje ze uzivatel po 'g' stiskl i 'h' (vzdy jsem to delal)
     // spoji .?.000 soubory do .mes, .?.tga
     fakMerge(scene,g_tgaFrames*g_lights,g_maxVertices);
   }
 }
 // kdyz litaj objekty, posune je o nakej kus dal
 if(p_flyingObjects)
 {
   // kdyz grabuje, rozhodujici je tgaFrame, jinak 3dsFrame
   if(p_ffGrab) g_tgaFrame++;else
   if(p_clock) p_3dsFrame=(int)(27*GETTIME/PER_SEC);
     else p_3dsFrame=fmod(p_3dsFrame+p_3dsFrameStep-p_3dsFrameStart,p_3dsFrameEnd-p_3dsFrameStart)+p_3dsFrameStart;
 }
 // kdyz lita kamera, posune ji
 if(p_flyingCamera)
 {
   static real camframe=0;
   if(p_clock) camframe=5*GETTIME/PER_SEC;
     else camframe+=p_3dsFrameStep;
   matrix_Create(&__world->camera[0],camframe);
 }
}

//////////////////////////////////////////////////////////////////////////////
//
// glut callbacks
//

char  name[20];
int   id=0;
Scene *scene;

void displayFunc(void)
{
 frameSetup(scene);
 if(frameCalculate(scene)) frameDraw(scene);
 frameAdvance(scene);
}

void idleFunc(void)
{
 displayFunc();
}

void reshapeFunc(int width, int height)
{
}

void keyboardFunc(unsigned char key, int x, int y)
{
 //printf("key %d\n",key);
 switch(key) {
  case 27 : /*delete scene;*/ exit(g_batchGrab || g_batchMerge); // escapnuta davka je chyba, musime ukoncit makefile
  case ' ': if(preparing_capture)
            {
              preparing_capture=false;
              captureTgaAfter(scene,"d01.tga",0,2);
              captureTgaAfter(scene,"d1.tga",0.9,2);
              captureTgaAfter(scene,"d10.tga",9,2);
              captureTgaAfter(scene,"d100.tga",90,1);
            }
            break;
  case 'i': __infolevel=(__infolevel+1)%3;n_dirtyGeometry=true;break;
  case '`': sprintf(name,"rr%d.tga",id++); video_Grab(name);break;
  case ']': video_XFOV+=5;video_YFOV-=5;raster_SetFOV(video_XFOV,video_YFOV);n_dirtyGeometry=true;break;
  case '[': video_XFOV-=5;video_YFOV+=5;raster_SetFOV(video_XFOV,video_YFOV);n_dirtyGeometry=true;break;
  case '}': __obj++;__obj%=__world->object_num;break;
  case '{': if(__obj--) __obj=__world->object_num;break;
  case 'm': ++d_meshing%=3;                        n_dirtyColor=true;break;
  case 'j': ++d_needle%=2;                         n_dirtyColor=true;break;
  case '1': d_gouraud=false;                       n_dirtyColor=true;break;
  case '2': d_gouraud=true;d_gouraud3=false;       n_dirtyColor=true;break;
  case '3': d_gouraud=true;d_gouraud3=true;        n_dirtyColor=true;break;
  case '+': d_bright*=1.1;                         n_dirtyColor=true;break;
  case '-': d_bright/=1.1;                         n_dirtyColor=true;break;
  case '*': d_gamma*=1.1;d_bright*=1.15;           n_dirtyColor=true;break;
  case '/': d_gamma/=1.1;d_bright/=1.15;           n_dirtyColor=true;break;
  case '!': ++d_forceSides%=3;                     n_dirtyColor=true;break;

  case 'o': p_flyingObjects=!p_flyingObjects;break;
  case 'p': p_flyingCamera=!p_flyingCamera;break;
  case 'f': p_clock=!p_clock;break;

  case '=': c_frameTime/=2;break;
  case '\\':c_frameTime*=2;break;
  case '_': p_3dsFrameStep/=2;break;
  case '|': p_3dsFrameStep*=2;break;

  case '(': if(d_factors2 && d_factors2->sub[0]) d_factors2=d_factors2->sub[0]; n_dirtyColor=true;break;
  case ')': if(d_factors2 && d_factors2->parent) d_factors2=d_factors2->parent; n_dirtyColor=true;break;
  
#ifdef SUPPORT_LIGHTMAP
  case '@': save_lightmaps(__world);break;
#else
  case '@': save_subtriangles(__world);break;
#endif

  case '5': setRrMode(scene,false,false,0);p_flyingObjects=false;break;
  case '6': setRrMode(scene,true ,false,0);p_flyingObjects=true;break;
  case '7': setRrMode(scene,false,true ,0);p_flyingObjects=true;break;
  case '8': setRrMode(scene,false,true ,1);break;
  case '9': setRrMode(scene,false,true ,2);break;
  case 'g': // nagrabuje sekvenci do .?.000 souboru
            if(g_separLights) // grabuje pro kazde svetlo zvlast, takze sekvence je pocet_svetel krat delsi
              for(int i=1;i<g_lights;i++) scene->turnLight(i,0);// necha svitit jen prvni svetlo
            setRrMode(scene,true ,false,0);p_flyingObjects=true;
            p_ffGrab=true;
            //g_tgaFrames=20;//zeptat se do kolika framu to natipat
            g_tgaFrame=0;//tolikatym framem zacit (pokud uz jsou treba predchozi spocteny)
            //c_frameTime=10;//zeptat se kolik sekund na snimek
            p_clock=false;
            break;
  case 'h': // spoji .?.000 soubory do .mes, .?.tga
            fakMerge(scene,g_tgaFrames*g_lights,g_maxVertices);
            break;
  case 'b': // prehrava .w.000 soubory
            setRrMode(scene,false,false,0);p_flyingObjects=true;
            p_ffPlay=1;
            p_3dsFrame=0;
            //ocekava g_tgaFrames framu nagrabovanejch na disku
            p_clock=false;
            break;
  case 'n': // prehrava .w.tga
            setRrMode(scene,false,false,0);p_flyingObjects=true;
            p_ffPlay=2;
            p_3dsFrame=0;
            p_clock=false;
            fakStartLoadingBytes(scene,false,false);
            break;
//  case 't': scene->iv_dumpTree("tree");

  case 'a': matrix_Move  (__world->camera[0].matrix, 0,GLMINUS(-5),0  );n_dirtyGeometry=true;break;
  case 'z': matrix_Move  (__world->camera[0].matrix, 0,GLMINUS( 5),0  );n_dirtyGeometry=true;break;
  case 'e': matrix_Rotate(__world->camera[0].matrix, 0.02,_Z_);n_dirtyGeometry=true;break;
  case 'q': matrix_Rotate(__world->camera[0].matrix,-0.02,_Z_);n_dirtyGeometry=true;break;
  case 's': matrix_Rotate(__world->camera[0].matrix,-0.02,_X_);n_dirtyGeometry=true;break;
  case 'w': matrix_Rotate(__world->camera[0].matrix, 0.02,_X_);n_dirtyGeometry=true;break;
  case 'c': matrix_Rotate(__world->camera[0].matrix,GLMINUS(-0.02),_Y_);n_dirtyGeometry=true;break;
  case 'x': matrix_Rotate(__world->camera[0].matrix,GLMINUS(0.02),_Y_);n_dirtyGeometry=true;break;

  case 'A': matrix_Move  (__world->object[__obj].matrix, 0,-5,0  );n_dirtyGeometry=true;break;
  case 'Z': matrix_Move  (__world->object[__obj].matrix, 0, 5,0  );n_dirtyGeometry=true;break;
  case 'E': matrix_Rotate(__world->object[__obj].matrix, 0.02,_Z_);n_dirtyGeometry=true;break;
  case 'Q': matrix_Rotate(__world->object[__obj].matrix,-0.02,_Z_);n_dirtyGeometry=true;break;
  case 'S': matrix_Rotate(__world->object[__obj].matrix,-0.02,_X_);n_dirtyGeometry=true;break;
  case 'W': matrix_Rotate(__world->object[__obj].matrix, 0.02,_X_);n_dirtyGeometry=true;break;
  case 'C': matrix_Rotate(__world->object[__obj].matrix,-0.02,_Y_);n_dirtyGeometry=true;break;
  case 'X': matrix_Rotate(__world->object[__obj].matrix, 0.02,_Y_);n_dirtyGeometry=true;break;

  case '"': matrix_Move(__world->object[__obj].matrix, 0,0, 5);n_dirtyGeometry=true;break;
  case '?': matrix_Move(__world->object[__obj].matrix, 0,0,-5);n_dirtyGeometry=true;break;
  case '<': matrix_Move(__world->object[__obj].matrix, 5,0, 0);n_dirtyGeometry=true;break;
  case '>': matrix_Move(__world->object[__obj].matrix,-5,0, 0);n_dirtyGeometry=true;break;

  case '.': d_only_o++;break;
 }

 // vynuti si pozdejsi transformaci objektu se kterym se hnulo
 #ifdef SUPPORT_DYNAMIC
 scene->object[__obj]->matrixDirty=true;
 n_dirtyObject=true;
 #endif
 // vynuti si pozdejsi transformaci kamery
 n_dirtyCamera=true;
}

void specialFunc(int key,int x,int y)
{
 //printf("spec key %d\n",key);
 switch (key) {
  case GLUT_KEY_UP:    matrix_Move(__world->camera[0].matrix, 0,0,GLMINUS( 5));n_dirtyGeometry=true;break;
  case GLUT_KEY_DOWN:  matrix_Move(__world->camera[0].matrix, 0,0,GLMINUS(-5));n_dirtyGeometry=true;break;
  case GLUT_KEY_LEFT:  matrix_Move(__world->camera[0].matrix, 5,0, 0);n_dirtyGeometry=true;break;
  case GLUT_KEY_RIGHT: matrix_Move(__world->camera[0].matrix,-5,0, 0);n_dirtyGeometry=true;break;
  }

 // vynuti si pozdejsi transformaci objektu se kterym se hnulo
 #ifdef SUPPORT_DYNAMIC
 scene->object[__obj]->matrixDirty=true;
 n_dirtyObject=true;
 #endif
 // vynuti si pozdejsi transformaci kamery
 n_dirtyCamera=true;

// scene->eye.x = scene->invcam[3][0];
// scene->eye.y = scene->invcam[3][1];
// scene->eye.z = scene->invcam[3][2];
}

void mouseFunc(int button, int state, int x, int y)
{
/*
 //printf("mouse %d %d b%d=%d\n",x,y,button,state);
 if(!state) return;
 Node *tmp=locate_subtriangle(__world,x,y);
 d_factors2=(tmp==d_factors2)?NULL:tmp;
 scene->staticReflectors.findFactorsTo(d_factors2); //!!! ladici vypis
 n_dirtyColor=true;
 n_dirtyCamera=true;//vynuti refresh, dirtyColor kvuli zrychleni neprekresluje hned
*/
}

void motionFunc(int x, int y)
{
}

void passiveMotionFunc(int x, int y)
{
}

void entryFunc(int state)
{
}

void visibilityFunc(int state)
{
}

//////////////////////////////////////////////////////////////////////////////
//
// main
//

void help()
{
 printf(" Realtime Radiosity                                   Dee & ReDox (C) 1999-2004\n");
 printf(" -------------------------------[ calculation ]--------------------------------\n");
 printf(" scene.bsp    ...load scene\n");
 printf(" -smoothRAD   ...smoothen edges between surfaces in plane +-RAD (0.3)\n");
 printf(" -hide:NAME   ...hide all faces from material NAME ()\n");
 printf(" -j           ...fight needles ('j' toggles needles: highlighted, masked)\n");
 printf(" -c           ...don't use clusters\n");
 printf("\n ---------------------------------[ display ]----------------------------------\n");
 printf(" -rXRESxYRES  ...set gfx display resolution (800x600)\n");
 printf(" -nogfx       ...no gfx display, stay in console\n");
 printf(" -gammaN      ...set gamma correction (0.35)\n");
 printf(" -brightN     ...set brightness (1.2)\n");
#ifdef SUPPORT_KAMIL
 printf(" -kamil       ...Kamilos hyper progress bar\n");
#endif
 printf(" -verboseLEVEL...0=less prints, 2=more prints (1)\n");
 printf("\n ----------------------------------[ export ]----------------------------------\n");
#ifdef SUPPORT_LIGHTMAP
 printf(" -lightmapsQ  ...calculate static scene to quality Q and export lightmaps\n");
#else
 printf(" -qualityQ    ...calculate static scene to quality Q and export subtriangles\n");
#endif
 printf(" -export:NAME ...export only faces from material NAME (*)\n");
 printf(" -dontexport:NAME...but dont export faces from material NAME ()\n");
 printf("\n ----------------------------[ animation precalc ]-----------------------------\n");
 printf(" -fFRAMES     ...split animation for each light into this number of FRAMES\n");
 printf(" -3dsFRAMES   ...use this number of 3ds frames\n");
 printf(" -lLIGHTS     ...>1 grab all lights separately, 1=grab all together\n");
 printf(" -sSECONDS    ...seconds of calculation per frame\n");
 printf(" -vVERTICES   ...save no more than this number of vertices (to tga)\n");
 printf(" -gFRAME      ...grab this FRAME (0..FRAMES-1) and terminate\n");
 printf(" -g           ...grab all frames (if -s specified), build tga (if -v) and exit\n");
 printf("\n -----------------------------------[ test ]-----------------------------------\n");
 printf(" -sides[1|2][outer|inner]:[render|emit|catch|receive|reflect|transmit]=[0|1]\n");
 printf(" -cap         ...capture scene after 0.1, 1, 10 and 100 sec, start by ' '\n");
 printf(" -tN          ...test speed (0=w/o .ora,1=&build .ora,2=test w/.ora,3=shooting)\n");
 exit(0);
}

int main(int argc, char **argv)
{
 int xres=800,yres=600;
 char *scenename=NULL;
 int p_test=0;
 bool gfx=true;//set gfx mode?,could be turned off by -nogfx
#ifdef SUPPORT_KAMIL
 bool kamil=false;
#endif

 MEM_INIT;
 kb_init();
 core_Init();
 glutInit(&argc,argv);
 fillColorTable(__needle_ct,.5,.1,.8);

 //char buf[400]; infoStructs(buf); puts(buf);

 for (int i=1;i<argc;i++)
 {
     if (!strcmp(argv[i],"-h"))
        help();
     else
     if (!strncmp(argv[i],"-hide:",6))
       (__hidematerial1?(__hidematerial2?__hidematerial3:__hidematerial2):__hidematerial1)=argv[i]+6;
     else
     if (!strncmp(argv[i],"-export:",8))
       __exportmaterial=argv[i]+8;
     else
     if (!strncmp(argv[i],"-dontexport:",12))
       __dontexportmaterial=argv[i]+12;
     else
     if (!strncmp(argv[i],"-verbose",8))
        {int tmp;if(sscanf(argv[i],"-verbose%d",&tmp)==1) __infolevel=tmp; else goto badarg;}
     else
     if (!strcmp(argv[i],"-c"))
        c_useClusters=false;
     else
     if (!strncmp(argv[i],"-gamma",6))
        {float tmp;if(sscanf(argv[i],"-gamma%f",&tmp)==1) d_gamma=tmp; else goto badarg;}
     else
     if (!strncmp(argv[i],"-bright",7))
        {float tmp;if(sscanf(argv[i],"-bright%f",&tmp)==1) d_bright=tmp; else goto badarg;}
     else
     if (!strncmp(argv[i],"-smooth",7))
        {float tmp;if(sscanf(argv[i],"-smooth%f",&tmp)==1) MAX_INTERPOL_ANGLE=tmp; else goto badarg;}
     else
     if (!strcmp(argv[i],"-j"))
        c_fightNeedles=true;
     else
     if (!strncmp(argv[i],"-sides",6))
        {int side,onoff;char inout[30],bit[30];
         if(sscanf(argv[i],"-sides%d%[^:]:%[^=]=%d",&side,inout,bit,&onoff)!=4) goto badarg;
         if(side<1 || side>2 || onoff<0 || onoff>1) goto badarg;
         SideBits *b;
         if(!strcmp(inout,"inner")) b=&sideBits[side][1]; else
         if(!strcmp(inout,"outer")) b=&sideBits[side][0]; else goto badarg;
         if(!strcmp(bit,"render")) b->renderFrom=onoff; else
         if(!strcmp(bit,"emit")) b->emitTo=onoff; else
         if(!strcmp(bit,"catch")) b->catchFrom=onoff; else
         if(!strcmp(bit,"receive")) b->receiveFrom=onoff; else
         if(!strcmp(bit,"reflect")) b->reflect=onoff; else
         if(!strcmp(bit,"transmit")) b->transmitFrom=onoff; else goto badarg;
     }
     else

     if (!strcmp(argv[i],"-cap"))
        preparing_capture=true;
     else
     if (!strncmp(argv[i],"-r",2))
        {if(sscanf(argv[i],"-r%dx%d",&xres,&yres)!=2) goto badarg;}
     else
     if (!strncmp(argv[i],"-f",2))
        {if(sscanf(argv[i],"-f%i",&g_tgaFrames)!=1) goto badarg;}
     else
     if (!strncmp(argv[i],"-3ds",2))
        {float tmp;if(sscanf(argv[i],"-3ds%f",&tmp)==1) {p_3dsFrameEnd=tmp;printf("using only first %f 3ds frames\n",p_3dsFrameEnd);} else goto badarg;}
     else
#ifdef SUPPORT_LIGHTMAP
     if (!strncmp(argv[i],"-lightmaps%f",10))
        {float tmp;if(sscanf(argv[i],"-lightmaps%f",&tmp)==1) endAccuracy=tmp; else goto badarg;}
     else
#else
     if (!strncmp(argv[i],"-quality%f",8))
        {float tmp;if(sscanf(argv[i],"-quality%f",&tmp)==1) endAccuracy=tmp; else goto badarg;}
     else
#endif
     if (!strncmp(argv[i],"-l",2))
        {int li;if(sscanf(argv[i],"-l%i",&li)==1) g_separLights=li!=1; else goto badarg;}
     else
     if (!strcmp(argv[i],"-g"))
        {g_batchGrab++;g_batchMerge++;}
     else
     if (!strncmp(argv[i],"-g",2))
        {if(sscanf(argv[i],"-g%i",&g_batchGrabOne)==1) g_batchGrab++; else goto badarg;}
     else
     if (!strncmp(argv[i],"-s",2))
        {float tmp;if(sscanf(argv[i],"-s%f",&tmp)==1) {c_frameTime=tmp;g_batchGrab++;} else goto badarg;}
     else
     if (!strncmp(argv[i],"-v",2))
        {if(sscanf(argv[i],"-v%i",&g_maxVertices)==1) g_batchMerge++; else goto badarg;}
     else
     if (!strncmp(argv[i],"-t",2))
        {p_test=-1;if(sscanf(argv[i],"-t%i",&p_test)!=1) goto badarg;}
     else
     if (!strncmp(argv[i],"-d",2))
        {float tmp;if(sscanf(argv[i],"-d%f",&tmp)==1) d_details=tmp; else goto badarg;}
     else
     if (!strcmp(argv[i],"-nogfx"))
        gfx=false;
     else
#ifdef SUPPORT_KAMIL
     if (!strcmp(argv[i],"-kamil"))
        kamil=true;
     else
#endif
     if (!strcmp(argv[i]+strlen(argv[i])-4,".bsp") || !strcmp(argv[i]+strlen(argv[i])-4,".BSP") || !strcmp(argv[i]+strlen(argv[i])-4,".r3d") || !strcmp(argv[i]+strlen(argv[i])-4,".R3D"))
        scenename=argv[i];
     else
        badarg:printf("unknown param: %s\n",argv[i]);
 }
 g_batchGrab/=2; // zas to vynuluje pokud nebylo inkremetovano u -s i -g
 g_batchMerge/=2;// zas to vynuluje pokud nebylo inkremetovano u -v i -g

 // nacte world
 DBG(printf("Loading bsp...\n"));
 __world=load_world(scenename);
 if(!__world) help();

 // nastavi matice, nutno pred world2scene
 matrix_Init(__identity);
 /*if(!__world->camera_num) {
	 __world->camera_num=1;
	 __world->camera=new CAMERA;
	 FILE* f=fopen("rr.cam","rb");
	 fread(&__world->camera[0],1,sizeof(__world->camera[0]),f);
	 fclose(f);
 }
 /*FILE* f=fopen("rr.cam2","wb");
 fwrite(&__world->camera[0],1,sizeof(__world->camera[0]),f);
 fclose(f);*/
 matrix_Create(&__world->camera[0],0);
 matrix_Hierarchy(__world->hierarchy,__identity,0);
 matrix_Invert(__world->camera[0].matrix,__world->camera[0].inverse);

 // zkonvertuje world na scene
 strcpy(p_ffName,scenename);
 p_ffName[strlen(p_ffName)-4]=0;// do p_ffName da jmeno sceny bez pripony
 //scene=convert_world2scene(__world,bp("%s.mgf",p_ffName));
 RRScene* rrscene=convert_world2scene(__world,bp("%s.mgf",p_ffName));
 if(!rrscene) help();
 scene=(Scene*)rrscene->getScene();
 if(!scene) help();

 g_lights=g_separLights?scene->turnLight(-1,0):1;// zjisti kolik je ve scene svetel (sviticich materialu), pokud je nema separovat tak necha jedno

 bool video_inited=false;
 if(gfx)
 {
  #ifdef RASTERGL
   video_inited=video_Init(xres,yres);
  #else
   video_inited=(raster_Output=video_Init(xres,yres));
  #endif
   if (!video_inited) {
      printf(" Nepovedlo se nastavit graficky rezim!\n");
      return 1;
      }
   if(!p_test) // shozeni grafiky dame na atexit (kdyz testuje rychlost, shodi si grafiku sam)
     atexit(video_Done);
 }

 raster_Init(video_XRES,video_YRES);
 raster_SetFOV(video_XFOV,video_YFOV);

 if (__mirror) __mirrorOutput=new int[video_XRES*video_YRES];

 endAfter(0.1);
 if(preparing_capture) scene->improveStatic(endByTime);

 if(endAccuracy>=0) {
#ifdef SUPPORT_KAMIL
   if (kamil) cnt=new Counter(20);
#endif
   scene->improveStatic(endByAccuracy);
#ifdef SUPPORT_LIGHTMAP
   save_lightmaps(__world);
#else
   save_subtriangles(__world);
#endif
   if(!gfx) return 0;
 }

 if(g_batchGrab) kb_put('g');//{displayFunc();keyboardFunc('g',0,0);}
 else
 if(g_batchMerge) kb_put('h');//{displayFunc();keyboardFunc('h',0,0);}

/*
 if(p_ffGrab)
 {
   // grabne jeden snimek a skonci
   // behem vypoctu ho kazdych 5 sekund zobrazi
   setRrMode(scene,true,false,0);
   p_ffGrab=true;
   p_flyingObjects=true;
   frameSetup(scene);
   real step=5;
   while(c_frameTime>step && !kb_hit())
   {
     endAfter(step);
     scene->improveStatic(endByTimeOrInput);
     scene->draw(0.4);
     c_frameTime-=step;
     __frameNumber++;
   }
   // ulozi ho jen kdyz nebyl prerusen
   if(!kb_hit())
   {
     frameCalculate(scene);
     frameDraw(scene);
   }
 }
 else
*/
 if(p_test)
 {
   if(p_test==3)
   {
     char buf[400];
     scene->infoScene(buf); puts(buf);
     endAfter(5);
     scene->improveStatic(endByTime);
     scene->infoImprovement(buf); puts(buf);
     //printf("kshot=%d kbsp=%d ktri=%d hak1=%d hak2=%d hak3=%d hak4=%d\n",__shot/1000,__bsp/1000,__tri/1000,__hak1/1000,__hak2/1000,__hak3/1000,__hak4/1000);
     extern void i_dbg_print();
     i_dbg_print();
     return 0;
   }
   //prehraje 5 snimku
   setRrMode(scene,false,false,0);p_flyingObjects=true;
   p_ffPlay=2;
   p_3dsFrame=0;
   p_clock=false;
   TIME t0=GETTIME;
   fakStartLoadingBytes(scene,p_test==1/*-t1=create .ora*/,p_test==2/*-t2=read .ora*/);
   TIME t1=GETTIME;
   if(video_inited)
   {
     unsigned frames=5;
     for(unsigned i=0;i<frames;i++) displayFunc();
     TIME t2=GETTIME;
     video_Done();
     printf("Drawing frame: %f sec\n",(float)(t2-t1)/PER_SEC/frames);
   }
   printf("Loading scene: %f sec\n",(float)(t1-t0)/PER_SEC);
   return 0;
 }
 else
 {
   glutDisplayFunc(displayFunc);
  // glutReshapeFunc(reshapeFunc);
   glutKeyboardFunc(keyboardFunc);
   glutSpecialFunc(specialFunc);
   glutMouseFunc(mouseFunc);
  // glutMotionFunc(motionFunc);
  // glutPassiveMotionFunc(passiveMotionFunc);
  // glutEntryFunc(entryFunc);
  // glutVisibilityFunc(visibilityFunc);
   glutIdleFunc(idleFunc);

   glutMainLoop();
 }

 return 0;
}
