//#define RASTERGL     // raster+video via openGL
#define LOGARITMIC_TGA // v tga jsou zlogaritmovane hodnoty
//#define ARCTAN_COLOR // spocitany jas pred zobrazenim prozene pres arctan()

#include <assert.h>
#include <math.h>
#include <string.h>
#include "ldmgf.h"
#include "render.h"
#include "surface.h"
#include "world.h"
#ifdef RASTERGL
 #include "ldmgf2.h"
 #include <GL/glut.h>
 #include "videogl.h"
 #include "rastergl.h"
#else
 #include "video.h"
 #include "raster.h"
#endif

#include "../RRVision/rrcore.h"//!!!
#include "../RRVision/clusters.h"//!!!
using namespace rr;

//////////////////////////////////////////////////////////////////////////////
//
// drawing
//

// global variables for drawing to speed up recursive calls

// d=draw params
#ifdef ARCTAN_COLOR
float   d_gamma=1;
float   d_bright=10;
#else
float   d_gamma=0.35;
float   d_bright=1.2;
#endif
bool    d_fast=false;           // max speed, low quality
bool    d_gouraud=true;
bool    d_gouraud3=false;
char    d_needle=0;             // 0=pink 1=retusovat jehly (pomale kresleni)
char    d_meshing=2;            // 0=source faces, 1=reflector meshing, 2=receiver meshing
char    d_engine=0;             // output via rrvision interface, no direct access
float   d_details=256;
bool    d_pointers=false;       // jako barvu pouzit pointer na subtriangle
void   *d_factors2=NULL;        // zobrazi faktory do nodu
char    d_forceSides=0;         // 0 podle mgf, 1=vse zobrazi 1sided, 2=vse zobrazi 2sided
int     d_only_o=-1;            // draw only object o, -1=all
bool    d_saving_tga=false;     // flag ktery je nastaven behem ukladani tga

char *__hidematerial1=NULL;
char *__hidematerial2=NULL;
char *__hidematerial3=NULL;
char *__exportmaterial="*";
char *__dontexportmaterial=NULL;
char  __infolevel=1;

const int g_logScale=15; // rozumny kompromis, vejdou se i svetla nekolikrat jasnejsi nez 255

real getBrightness(real color) // converts 0..infinity radiosity to 0..1 value for display
{
	assert(IS_NUMBER(color));
	if(color<0) return 0;
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

	if(d_saving_tga)
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

real getAvg(const RRColor* rad)
{
	return avg(*rad);
}

#if CHANNELS == 3
real getBrightness(Channels rad)
{
	return getBrightness(avg(rad));
}
#endif

void drawEngine(rr::RRScene* scene, unsigned o, unsigned t, Triangle *f)
{
	Point3 v[3];
	real brightness[3];
	v[0]=f->to3d(0);
	v[1]=f->to3d(1);
	v[2]=f->to3d(2);
	RRColor tmp;
	scene->getTriangleMeasure(o,t,0,RM_IRRADIANCE_ALL,tmp); brightness[0]=getBrightness(getAvg(&tmp));
	scene->getTriangleMeasure(o,t,1,RM_IRRADIANCE_ALL,tmp); brightness[1]=getBrightness(getAvg(&tmp));
	scene->getTriangleMeasure(o,t,2,RM_IRRADIANCE_ALL,tmp); brightness[2]=getBrightness(getAvg(&tmp));
#ifdef RASTERGL
	raster_ZGouraud(v,((Surface*)f->grandpa->surface)->diffuseReflectanceColorTable,brightness);
#else
	raster_POINT p[4];
	raster_POLYGON p1,p2,p3;
	p1.point=&p[1]; p1.next=&p2; 
	p2.point=&p[2]; p2.next=&p3;
	p3.point=&p[3]; p3.next=NULL;
	p[1].x=v[0].x; p[1].y=v[0].y; p[1].z=v[0].z; p[1].u=brightness[0];
	p[2].x=v[1].x; p[2].y=v[1].y; p[2].z=v[1].z; p[2].u=brightness[1];
	p[3].x=v[2].x; p[3].y=v[2].y; p[3].z=v[2].z; p[3].u=brightness[2];
	raster_ZGouraud(&p1,((Surface*)f->surface)->diffuseReflectanceColorTable);
#endif
}

static ColorTable __needle_ct=new unsigned[C_INDICES];

void SubTriangle::drawFlat(Channels ambient,int df)
{
	if(flags&FLAG_DIRTY_ALL_SUBNODES) df|=DF_REFRESHALL;
	if(!(df&DF_REFRESHALL) && !(flags&FLAG_DIRTY_NODE)) return;
	flags&=~FLAGS_DIRTY;
	Node* d_factorsTo = (Node*)d_factors2;
	bool is_destination=(this==d_factors2)
		;
	real tmp=0;
	bool is_source=d_factors2 && shooter && (tmp=shooter->contains(d_factorsTo))!=-1;
	//if(is_source) printf("pwr=%f ",tmp);//!!!
	if(is_destination || is_source) ; else
		if((!d_fast) && sub[0] && ((d_meshing==1 && sub[0]->shooter) || d_meshing==2))
		{
			ambient+=(totalExitingFlux-sub[0]->totalExitingFlux-sub[1]->totalExitingFlux)/area;
			SUBTRIANGLE(sub[0])->drawFlat(ambient,df);
			SUBTRIANGLE(sub[1])->drawFlat(ambient,df);
			return;
		}
		if(d_factors2 && !is_source && !is_destination) return;

		real brightness=getBrightness(ambient+totalExitingFlux/area);

#ifdef RASTERGL

		Point3 v[3];

		v[0]=to3d(0);
		v[1]=to3d(1);
		v[2]=to3d(2);

		raster_ZFlat(v,((Surface*)grandpa->surface)->diffuseReflectanceColorTable,brightness);

#else

		raster_POINT p[3];
		raster_POLYGON p1,p2,p3;

		p1.point=&p[0]; p1.next=&p2;
		p2.point=&p[1]; p2.next=&p3;
		p3.point=&p[2]; p3.next=NULL;

		{
			Vec3 v;

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
						raster_ZFlat(&p1,((Surface*)grandpa->surface)->diffuseReflectanceColorTable,brightness);
		}

#endif
}


// ambient propaguje dolu jen kvuli gouraud3 at vi barvu stredu subtrianglu

void SubTriangle::drawGouraud(Channels ambient,IVertex **iv,int df)
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
	if((!d_fast) && sub[0] && ((d_meshing==1 && sub[0]->shooter) || d_meshing==2)
		&& (p_ffPlay!=2 || subvertex->error<d_details)
		&& (p_ffPlay==0 || subvertex->loaded))//kdyz ffPlayuje=loaduje z disku, loaduje jen nektery ivertexy, ostatni zustanou nevyplneny a kdybysme se jich ptali, sectou si a vratej nam energii z corneru kde ted zadna neni
	{
		ambient+=(totalExitingFlux-sub[0]->totalExitingFlux-sub[1]->totalExitingFlux)/area;
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

	brightness[0]=getBrightness(iv[0]->exitance());
	brightness[1]=getBrightness(iv[1]->exitance());
	brightness[2]=getBrightness(iv[2]->exitance());

	raster_ZGouraud(v,((Surface*)grandpa->surface)->diffuseReflectanceColorTable,brightness);

#else

	raster_POINT p[4];
	raster_POLYGON p1,p2,p3;

	p1.next=&p2;
	p2.next=&p3;
	p3.next=NULL;

	{

		Vec3 v;

		v=to3d(0);
		p[1].x=v.x; p[1].y=v.y; p[1].z=v.z;
		p[1].u=getBrightness(iv[0]->exitance(this));

		v=to3d(1);
		p[2].x=v.x; p[2].y=v.y; p[2].z=v.z;
		p[2].u=getBrightness(iv[1]->exitance(this));

		v=to3d(2);
		p[3].x=v.x; p[3].y=v.y; p[3].z=v.z;
		p[3].u=getBrightness(iv[2]->exitance(this));

		if(!d_gouraud3)
		{
			p1.point=&p[1];
			p2.point=&p[2];
			p3.point=&p[3];

			raster_ZGouraud(&p1,
				((Surface*)grandpa->surface)->diffuseReflectanceColorTable);
		}
		else
		{
			v=to3d(uv[0]+(u2+v2)/3);
			p[0].x=v.x; p[0].y=v.y; p[0].z=v.z;
			p[0].u=getBrightness(ambient+totalExitingFlux/area);

			p1.point=&p[1];
			p2.point=&p[2];
			p3.point=&p[0];

			raster_ZGouraud(&p1,((Surface*)grandpa->surface)->diffuseReflectanceColorTable);

			p1.point=&p[1];
			p2.point=&p[0];
			p3.point=&p[3];

			raster_ZGouraud(&p1,((Surface*)grandpa->surface)->diffuseReflectanceColorTable);

			p1.point=&p[0];
			p2.point=&p[2];
			p3.point=&p[3];

			raster_ZGouraud(&p1,((Surface*)grandpa->surface)->diffuseReflectanceColorTable);

		}

	}

#endif
}



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

// obecna fce na kresleni trianglu, pouzije tu metodu ktera je zrovna podporovana

void inline draw_triangle(rr::RRScene* scene, unsigned o, unsigned t, Triangle *f)
{
	if(!f->surface) return;
#ifdef TEST_SCENE
	if (!f) return; // narazili jsme na vyrazeny triangl
#endif
	if(d_engine) {drawEngine(scene,o,t,f); return;}
#ifdef RASTERGL
	real ambient=f->radiosityIndirect();
	//teoreticky by do flagu stacilo dat (n_dirtyColor || n_dirtyGeometry)?DF_REFRESHALL:0
	//ale praktikcy tam jsou jeste nejaky hacky, takze radsi refreshneme vzdy vse
	if(d_gouraud) f->drawGouraud(ambient,f->topivertex,DF_REFRESHALL); else
		f->drawFlat(ambient,DF_REFRESHALL);
#else
	{
		Channels ambient=f->radiosityIndirect();
		if(d_gouraud && !d_fast) f->drawGouraud(ambient,f->topivertex,DF_REFRESHALL); else
			f->drawFlat(ambient,true);
	}
#endif
}

void render_object(rr::RRScene* scene, unsigned o, Object* obj, MATRIX& im)
{
	//raster_BeginTriangles();
	for (unsigned j=0;j<obj->triangles;j++) if(obj->triangle[j].isValid){
		Normal n=obj->triangle[j].getN3();
		U8 fromOut=n.w+im[3][0]*n.x+im[3][1]*n.y+im[3][2]*n.z>0;
		if ((d_forceSides==0 && obj->triangle[j].surface->sideBits[fromOut?0:1].renderFrom) ||
			(d_forceSides==1 && fromOut) ||
			(d_forceSides==2))
			draw_triangle(scene,o,j,&obj->triangle[j]);
	}
	//raster_EndTriangles();
}

void render_world(WORLD *w, rr::RRScene* scene, int camera_id, bool mirrorFrame)
{
	// TIME t0=GETTIME;
	MATRIX cm,im,om;

	for (int i=0;i<w->object_num;i++) {//if(/*d_only_o<0 ||*/ (i!=336 /*|| i==279*/) /*&& (i%100)==d_only_o*/) { 

		OBJECT *o=&w->object[i];
		Scene* tmp = *(Scene**)scene; // pozor, predpokladame ze prvni polozka v RRScene je pointer na Scene
		Object *obj=tmp->object[o->objectHandle];
		assert(obj);

		if (!mirrorFrame || strcmp(o->name,"Zrcadlo")) {

			if(camera_id>=0) 
			{
				// vsechny objekty jsou ve scenespacu, vsechny matice jsou identity
				// takze by to tady mohlo jit o neco rychlejc i bez transformaci
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
			render_object(scene,i,obj,im);
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
	rayTraceGather(eye,direction,NULL,color);
	real b=getBrightness(i);
	video_PutPixel(x,y,color);
	}
	#endif
	*/
	// printf("render=%dms\n",GETTIME-t0);
}

void fillColorTable(unsigned *ct,double cx,double cy,real rs)
{
	for(unsigned c=0;c<C_INDICES;c++)
	{
		RRColor rgb;
		xy2rgb(cx,cy,c/255.,rgb);
		#define FLOAT2BYTE(f) ((unsigned)((f)<0?0:(f)>=1?255:256*(f)))
		ct[c]=(FLOAT2BYTE(rs)<<24) + (FLOAT2BYTE(rgb[0])<<16) + (FLOAT2BYTE(rgb[1])<<8) + FLOAT2BYTE(rgb[2]);
	}
}

void render_init()
{
	fillColorTable(__needle_ct,.5,.1,.8);
}
