//#define RASTERGL     // raster+video via openGL
#define LOGARITMIC_TGA // v tga jsou zlogaritmovane hodnoty
//#define ARCTAN_COLOR // spocitany jas pred zobrazenim prozene pres arctan()
//#define SUPPORT_Z

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
#ifdef SUPPORT_Z
 #include <zlib.h>
#endif

#ifdef RASTERGL
#ifdef SUPPORT_LIGHTMAP
#error accelerated lightmaps not supported
#endif
#endif

#include "../RREngine/rrcore.h"//!!!
using namespace rrEngine;

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
char    d_engine=0;             // output via rrengine interface, no direct access
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

real getAvg(const real* rad)
{
	return (rad[0]+rad[1]+rad[2])/3;
}

#if CHANNELS == 3
real getBrightness(Channels rad)
{
	return getBrightness(sum(rad)/3);
}
#endif

void drawEngine(rrEngine::RRScene* scene, unsigned o, unsigned t, Triangle *f)
{
	Point3 v[3];
	real brightness[3];
	v[0]=f->to3d(0);
	v[1]=f->to3d(1);
	v[2]=f->to3d(2);
	brightness[0]=getBrightness(getAvg(scene->getTriangleRadiantExitance(o,t,0)));
	brightness[1]=getBrightness(getAvg(scene->getTriangleRadiantExitance(o,t,1)));
	brightness[2]=getBrightness(getAvg(scene->getTriangleRadiantExitance(o,t,2)));
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
		|| (d_factorsTo && IS_CLUSTER(d_factorsTo) && IS_TRIANGLE(this) && CLUSTER(d_factorsTo)->contains(TRIANGLE(this)));
	real tmp=0;
	bool is_source=d_factors2 && shooter && (tmp=shooter->contains(d_factorsTo))!=-1;
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

		raster_ZFlat(v,((Surface*)grandpa->surface)->diffuseReflectanceColorTable,brightness);

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

	raster_ZGouraud(v,((Surface*)grandpa->surface)->diffuseReflectanceColorTable,brightness);

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

		Vec3 v;

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

			raster_ZGouraud(&p1,(d_needle==0 && grandpa->isNeedle)?__needle_ct:((Surface*)grandpa->surface)->diffuseReflectanceColorTable);
		}
		else
		{
			v=to3d(uv[0]+(u2+v2)/3);
			p[0].x=v.x; p[0].y=v.y; p[0].z=v.z;
			p[0].u=getBrightness(ambient+(energyDirect+getEnergyDynamic())/area);

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

unsigned SubTriangle::printGouraud(void *f, IVertex **iv, real scale,Channels flatambient)
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

		if (flatambient!=Channels(0)) {
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
	raster_ZTexture(p,lightmap.w,lightmap.bitmap,((Surface*)surface)->diffuseReflectanceColorTable);
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

void inline draw_triangle(rrEngine::RRScene* scene, unsigned o, unsigned t, Triangle *f)
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
#ifdef SUPPORT_LIGHTMAP
	if(p_ffPlay) f->updateLightmapSize(false);
	if(f->lightmap.w)
		f->drawLightmap();
	else
#endif
	{
		Channels ambient=f->radiosityIndirect();
		if(d_gouraud
#ifndef SUPPORT_LIGHTMAP
			&& !d_fast
#endif
			) f->drawGouraud(ambient,f->topivertex,DF_REFRESHALL); else
			f->drawFlat(ambient,true);
	}
#endif
}

void render_object(rrEngine::RRScene* scene, unsigned o, Object* obj, MATRIX& im)
{
	//raster_BeginTriangles();
	for (unsigned j=0;j<obj->triangles;j++) if(obj->triangle[j].isValid){
		Normal n=obj->triangle[j].getN3();
		U8 fromOut=n.d+im[3][0]*n.x+im[3][1]*n.y+im[3][2]*n.z>0;
		if ((d_forceSides==0 && sideBits[obj->triangle[j].surface->sides][fromOut?0:1].renderFrom) ||
			(d_forceSides==1 && fromOut) ||
			(d_forceSides==2))
			draw_triangle(scene,o,j,&obj->triangle[j]);
	}
	//raster_EndTriangles();
}

#ifdef RASTERGL
void renderWorld_Lights0(WORLD *w, int camera_id)
{
	for (int i=0;i<w->object_num;i++) 
	{
		if(camera_id>=0) 
		{
			MATRIX cm,im,om;
			OBJECT *o=&w->object[i];
			matrix_Copy(w->camera[camera_id].inverse,cm);
			matrix_Copy(o->transform,om);
			matrix_Mul(cm,om);
			matrix_Invert(cm,im);
			raster_SetMatrix(&cm,&im);
		}
		mgf_draw_colored();
		return;
	}
}

void renderWorld_Lights1(WORLD *w, int camera_id)
{
	if (softLight<0) updateDepthMap();

	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	setupEyeView();
	glLightfv(GL_LIGHT0, GL_POSITION, lv);

	if (useShadowMapSupport) {
		drawHardwareShadowPass();
	} else {
		drawDualTextureShadowPasses();
		blendTexturedFloor();
	}

	drawLight();
	drawShadowMapFrustum();
}

void placeSoftLight(int n)
{
	softLight=n;
	static float oldLightAngle,oldLightHeight;
	if(n==-1) { // init, before all
		oldLightAngle=lightAngle;
		oldLightHeight=lightHeight;
		glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 1);
		glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 90); // no light behind spotlight
		glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01);
		return;
	}
	if(n==-2) { // done, after all
		lightAngle=oldLightAngle;
		lightHeight=oldLightHeight;
		updateMatrices();
		return;
	}
	// place one point light approximating part of area light
	if(useLights>1) {
		switch(areaType) {
      case 0: // linear
	      lightAngle=oldLightAngle+0.2*(n/(useLights-1.)-0.5);
	      lightHeight=oldLightHeight-0.4*n/useLights;
	      break;
      case 1: // rectangular
	      {int q=(int)sqrtf(useLights-1)+1;
	      lightAngle=oldLightAngle+0.1*(n/q/(q-1.)-0.5);
	      lightHeight=oldLightHeight+(n%q/(q-1.)-0.5);}
	      break;
      case 2: // circular
	      lightAngle=oldLightAngle+sin(n*2*3.14159/useLights)/20;
	      lightHeight=oldLightHeight+cos(n*2*3.14159/useLights)/2;
	      break;
		}
		updateMatrices();
	}
	GLfloat ld[3]={-lv[0],-lv[1],-lv[2]};
	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ld);
}

void setLightIntensity(float* col)
{
	float zero[4]={1,0,0,1};
	float col[4];
	glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
	glLightfv(GL_LIGHT0, GL_SPECULAR, col);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, col);
}

void renderWorld_LightsN(WORLD *w, int camera_id, unsigned useLights, bool useAccum)
{
	int i;
	placeSoftLight(-1);
	for(i=0;i<useLights;i++)
	{
		placeSoftLight(i);
		glClear(GL_DEPTH_BUFFER_BIT);
		renderWorld_Depth(i);
	}
	if(useAccum) {
		glClear(GL_ACCUM_BUFFER_BIT);
	} else {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		setupEyeView();
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
		glDisable(GL_LIGHTING)
		renderWorld_Lights0(w,camera_id);
		glEnable(GL_LIGHTING)
		globalIntensity=1./useLights;
		glBlendFunc(GL_ONE,GL_ONE);
		glEnable(GL_BLEND);
	}
	for(i=0;i<useLights;i++)
	{
		placeSoftLight(i);
		renderWorld_Lights1(useAccum);
		if(useAccum) {
			glAccum(GL_ACCUM,1./useLights);
		}
	}
	placeSoftLight(-2);
	if(useAccum) {
		glAccum(GL_RETURN,1);
	} else {  
		glDisable(GL_BLEND);
		globalIntensity=1;
	}
}
#endif

void render_world(WORLD *w, rrEngine::RRScene* scene, int camera_id, bool mirrorFrame)
{
#ifdef RASTERGL
	renderWorldInstantRadiosity(w,camera_id); return;

	float col[4]={1,1,1,1};
	setLightIntensity(col);
	glLightfv(GL_LIGHT0, GL_POSITION, col);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	renderWorldGeometry(w,camera_id);
	return;
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//setupEyeView();
	//drawShadowMapFrustum();
#endif

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
	rayTraceCamera(eye,direction,NULL,color);
	real b=getBrightness(i);
	video_PutPixel(x,y,color);
	}
	#endif
	*/
	// printf("render=%dms\n",GETTIME-t0);
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

void render_init()
{
	fillColorTable(__needle_ct,.5,.1,.8);
}

