//#define SUPPORT_KAMIL
//#define SUPPORT_REGEX
//#define NDEBUG       // no asserts, no debug code
#define DBG(a) //a
#define WAIT //fgetc(stdin) // program ceka na stisk, pro ladeni spotreby pameti
#define MAX_UNINTERACT_TIME 2 // max waiting for response with glut/opengl 2sec

#include <assert.h>
#include <io.h>
#include <limits.h>   //INT_MAX
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef RASTERGL
 #include "ldmgf2.h"
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
#include "ldmgf.h"
#include "misc.h"
#include "render.h"
#include "RRVision.h"
#include "surface.h"
#include "World.h"
#include "world2rrvision.h"

#include "../RRVision/rrcore.h"//!!!
using namespace rrVision;

WORLD  *__world=NULL;
MATRIX  __identity;
int __obj=0,__mirror=0,*__mirrorOutput;


// c=calculation params
bool    c_fightNeedles=false;   // specialne hackovat jehlovite triangly?
bool    c_staticReinit=false;   // pocita se kazdy snimek samostatne?
bool    c_dynamic=false;        // pocita dynamicke osvetleni misto statickeho?
real    c_frameTime=0.5;        // cas na jeden snimek (zvetsujici se pri nemenne geometrii a kamere)
real    c_dynamicFrameTime=0.5; // zvetsujici se cas na jeden dynamickej snimek

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

SubTriangle *locate_subtriangle(WORLD *w, rrVision::RRScene* scene, int x,int y)
{
	raster_Clear();
	bool old_gouraud=d_gouraud; d_gouraud=false;
	char old_meshing=d_meshing; d_meshing=2;
	d_pointers=true;
	render_world(w,scene,0,false);
	d_pointers=false;
	d_meshing=old_meshing;
	d_gouraud=old_gouraud;
	return (SubTriangle*)video_GetPixel(x,y);
}

void infoMisc(char *buf)
{
	buf[0]=0;
	if(p_ffGrab) sprintf(buf+strlen(buf),"grab(%i/%i) ",g_tgaFrame,g_tgaFrames*g_lights);
	sprintf(buf+strlen(buf)," gamma=%0.3f bright=%0.3f",d_gamma,d_bright);
}

void Scene::draw(rrVision::RRScene* scene, real quality)
{
 float backgroundColor[3]={0,0,0};
  //spravne ma byt =(material ve kterem je kamera)->color;
 video_Clear(backgroundColor);

#ifndef RASTERGL
 if (__mirror) {
    int *o=raster_Output;
    raster_Output=__mirrorOutput;
    raster_Clear();
    render_world(__world,scene,0,true);
    raster_Output=o;
    raster_Clear();
    render_world(__world,scene,0,false);
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
    render_world(__world,scene,0,false);
  }

 if(__infolevel && (g_batchGrabOne<0 || g_batchGrabOne==g_tgaFrame%g_tgaFrames))
 {
   char buf[400];
   infoImprovement(buf,__infolevel);
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
     unsigned ndx=scene->objNdx((Object *)(__world->object[o].obj));
     if(ndx>=0 && ndx<scene->staticObjects) scene->objMakeDynamic(ndx);
   }
 #endif
}

//////////////////////////////////////////////////////////////////////////////
//
// misc
//

static bool endByTime(void *context)
{
 return GETTIME>(TIME)(intptr_t)context;
}

static bool endByTimeOrInput(void *context)
{
 return kb_hit() || GETTIME>(TIME)(intptr_t)context || mouse_hit();
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

static bool endByAccuracy(void *context)
{
 Scene* scene = (Scene*)context;
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

static void captureTgaAfter(Scene *scene,rrVision::RRScene* rrscene,char *name,real seconds,real minimalImprovementToShorten)
{
 scene->improveStatic(endByTime,(void*)(intptr_t)(GETTIME+seconds*PER_SEC));
 if (scene->shortenStaticImprovementIfBetterThan(minimalImprovementToShorten))
    scene->finishStaticImprovement();
 scene->draw(rrscene,0.4);
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
    //    exitance() rozdelit na radiosityR/G/B()
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

 d_saving_tga=true;
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
 d_saving_tga=false;

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
 #ifdef SUPPORT_TRANSFORMS
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

#ifdef SUPPORT_DYNAMIC
 if(c_dynamic)
 {
   TIME endTime=(GETTIME+c_dynamicFrameTime*PER_SEC);
   scene->improveDynamic(endByTimeOrInput,(void*)(intptr_t)endTime);
   if(GETTIME>endTime) c_dynamicFrameTime*=1.5; // increase time only when previous time really elapsed (don't increase after each hit)
   n_dirtyColor=true;
   return true;
 }
 else
#endif
 {
   if(!p_flyingCamera && !p_flyingObjects && (n_dirtyCamera || n_dirtyObject)) return true; // jednorazova zmena sceny klavesnici nebo mysi -> okamzity redraw
   bool change=false;
   if(!preparing_capture && (g_batchGrabOne<0 || g_batchGrabOne==g_tgaFrame%g_tgaFrames)) {
     TIME endTime=(TIME)(GETTIME+c_dynamicFrameTime*PER_SEC);
     change=scene->improveStatic(endByTimeOrInput,(void*)(intptr_t)endTime)==rrVision::RRScene::IMPROVED;
     if(GETTIME>endTime) c_dynamicFrameTime*=1.5; // increase time only when previous time really elapsed (don't increase after each hit)
   }
   return change || p_flyingCamera || p_flyingObjects || n_dirtyCamera || n_dirtyObject;
 }
}

// vykresli a pripadne grabne aktualni frame

void frameDraw(Scene *scene, rrVision::RRScene* rrscene)
{
// d_fast=!p_flyingObjects && !p_flyingCamera && n_dirtyGeometry;
 scene->draw(rrscene,0.4);
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

     // zvysi cislo snimku aby exitance() volana pri ukladani vedela ze nema
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
rrVision::RRScene *rrscene;

void displayFunc(void)
{
 frameSetup(scene);
 if(frameCalculate(scene)) frameDraw(scene,rrscene);
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
              captureTgaAfter(scene,rrscene,"d01.tga",0,2);
              captureTgaAfter(scene,rrscene,"d1.tga",0.9,2);
              captureTgaAfter(scene,rrscene,"d10.tga",9,2);
              captureTgaAfter(scene,rrscene,"d100.tga",90,1);
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

  case '(': if(d_factors2 && ((Node*)d_factors2)->sub[0]) d_factors2=((Node*)d_factors2)->sub[0]; n_dirtyColor=true;break;
  case ')': if(d_factors2 && ((Node*)d_factors2)->parent) d_factors2=((Node*)d_factors2)->parent; n_dirtyColor=true;break;
  
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
 #ifdef SUPPORT_TRANSFORMS
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
 #ifdef SUPPORT_TRANSFORMS
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
 printf(" -itN         ...intersect technique, 0=most_compact..4=fastest\n");
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
 rrCollider::RRCollider::IntersectTechnique intersectTechnique = rrCollider::RRCollider::IT_BSP_FASTEST;

 assert(sizeof(U8)==1);
 assert(sizeof(U16)==2);
 assert(sizeof(U32)==4);
 assert(sizeof(U64)==8);
 MEM_INIT;
 kb_init();
 glutInit(&argc,argv);
 render_init();
 rrCollider::registerLicense(
	 "Illusion Softworks, a.s.",
	 "DDEFMGDEFFBFFHJOCLBCFPMNHKENKPJNHDJFGKLCGEJFEOBMDC"
	 "ICNMHGEJJHJACNCFBOGJKGKEKJBAJNDCFNBGIHMIBODFGMHJFI"
	 "NJLGPKGNGOFFLLOGEIJMPBEADBJBJHGLJKGGFKOLDNIBIFENEK"
	 "AJCOKCOALBDEEBIFIBJECMJDBPJMKOIJPCJGIOCCHGEGCJDGCD"
	 "JDPKJEOJGMIEKNKNAOEENGMEHNCPPABBLLKGNCAPLNPAPNLCKM"
	 "AGOBKPOMJK");
 rrVision::LicenseStatus status = rrVision::registerLicense(
	 "Illusion Softworks, a.s.",
	 "DDEFMGDEFFBFFHJOCLBCFPMNHKENKPJNHDJFGKLCGEJFEOBMDC"
	 "ICNMHGEJJHJACNCFBOGJKGKEKJBAJNDCFNBGIHMIBODFGMHJFI"
	 "NJLGPKGNGOFFLLOGEIJMPBEADBJBJHGLJKGGFKOLDNIBIFENEK"
	 "AJCOKCOALBDEEBIFIBJECMJDBPJMKOIJPCJGIOCCHGEGCJDGCD"
	 "JDPKJEOJGMIEKNKNAOEENGMEHNCPPABBLLKGNCAPLNPAPNLCKM"
	 "AGOBKPOMJK");
 switch(status)
 {
	case VALID:       break;
	case EXPIRED:     printf("License expired!\n"); break;
	case WRONG:       printf("Wrong license!\n"); break;
	case NO_INET:     printf("No internet connection to verify license!\n"); break;
	case UNAVAILABLE: printf("Temporarily unable to verify license, quit and try later.\n"); break;
 }
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
        RRSetState(RRSS_USE_CLUSTERS,0);
     else
     if (!strncmp(argv[i],"-gamma",6))
        {float tmp;if(sscanf(argv[i],"-gamma%f",&tmp)==1) d_gamma=tmp; else goto badarg;}
     else
     if (!strncmp(argv[i],"-bright",7))
        {float tmp;if(sscanf(argv[i],"-bright%f",&tmp)==1) d_bright=tmp; else goto badarg;}
     else
     if (!strncmp(argv[i],"-it",2))
        {int tmp;if(sscanf(argv[i],"-it%i",&tmp)==1) intersectTechnique = (rrCollider::RRCollider::IntersectTechnique)tmp; else goto badarg;}
     else
     if (!strncmp(argv[i],"-smooth",7))
        {float tmp;if(sscanf(argv[i],"-smooth%f",&tmp)==1) RRSetStateF(RRSSF_MAX_SMOOTH_ANGLE,tmp); else goto badarg;}
     else
     if (!strcmp(argv[i],"-j"))
        c_fightNeedles=true;
     else
     if (!strncmp(argv[i],"-sides",6))
        {int side,onoff;char inout[30],bit[30];
         if(sscanf(argv[i],"-sides%d%[^:]:%[^=]=%d",&side,inout,bit,&onoff)!=4) goto badarg;
         if(side<1 || side>2 || onoff<0 || onoff>1) goto badarg;
         RRSideBits *b;
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
 //!!!RRSetStateF(RRSSF_SUBDIVISION_SPEED,0.0001f);

 // nacte world
 DBG(printf("Loading bsp...\n"));
 __world=load_world(scenename);
 if(!__world) help();

 // nastavi matice, nutno pred world2scene
 matrix_Init(__identity);
 matrix_Create(&__world->camera[0],0);
 matrix_Hierarchy(__world->hierarchy,__identity,0);
// matrix_Move(__world->camera[0].matrix, 0,0,GLMINUS( 900));//!!! hack na cube / gl
 matrix_Invert(__world->camera[0].matrix,__world->camera[0].inverse);

 // zkonvertuje world na scene
 strcpy(p_ffName,scenename);
 p_ffName[strlen(p_ffName)-4]=0;// do p_ffName da jmeno sceny bez pripony
 rrscene=convert_world2scene(__world,bp("%s.mgf",p_ffName),intersectTechnique);
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
#ifdef RASTERGL
 mgf_load(bp("%s.mgf",p_ffName));
 mgf_compile();
#endif

 if (__mirror) __mirrorOutput=new int[video_XRES*video_YRES];

 if(preparing_capture) scene->improveStatic(endByTime,(void*)(intptr_t)(GETTIME+0.1*PER_SEC));

 if(endAccuracy>=0) {
#ifdef SUPPORT_KAMIL
   if (kamil) cnt=new Counter(20);
#endif
   scene->improveStatic(endByAccuracy,scene);
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
     scene->improveStatic(endByTimeOrInput,(void*)(intptr_t)(GETTIME+step*PER_SEC));
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
     scene->improveStatic(endByTime,(void*)(intptr_t)(GETTIME+5*PER_SEC));
     scene->infoImprovement(buf,__infolevel); puts(buf);
     //printf("kshot=%d kbsp=%d ktri=%d hak1=%d hak2=%d hak3=%d hak4=%d\n",__shot/1000,__bsp/1000,__tri/1000,__hak1/1000,__hak2/1000,__hak3/1000,__hak4/1000);
     rrCollider::RRIntersectStats::getInstance()->getInfo(buf,400,1); puts(buf);
     fgetc(stdin);
     return 0;
   }
   if(p_test==4) // superstrucny vypis pouze automaticky benchmark
   {
	   char buf[400];
	   scene->improveStatic(endByTime,(void*)(intptr_t)(GETTIME+5*PER_SEC));
	   scene->infoImprovement(buf,__infolevel); puts(buf);
	   const rrCollider::RRCollider* collider = scene->object[0]->importer->getCollider();
	   const rrCollider::RRMeshImporter* importer = collider->getImporter();
	   sprintf(buf,"%5d/%03d",rrCollider::RRIntersectStats::getInstance()->intersects/1000,collider->getMemoryOccupied()/importer->getNumTriangles());
	   fprintf(stderr,buf);
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
