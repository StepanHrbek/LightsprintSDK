#ifdef SUPPORT_DYNAMIC

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "core.h"
#include "dynamic.h"


byte d_drawDynamicHits=0;// 0=normal energy, 1=hits->pixels, 2=all shots->pixels

//////////////////////////////////////////////////////////////////////////////

void Hits::insert(Hit ahit,void *extension)
{
	assert(isDynamic || !hits);
	rawInsert(ahit);
	isDynamic=true;
	if(hits==hitsAllocated)
	{
		size_t oldsize=hitsAllocated*sizeof(Hit);
		__hitsAllocated+=3*hitsAllocated;
		hitsAllocated*=4;
		hit=(Hit *)realloc(hit,oldsize,hitsAllocated*sizeof(Hit));
	}
	hit[hits++].setExtensionP(extension);
}

real Hits::convertDHitsToHits()
{
	assert(isDynamic || !hits);
	assert((hits&1)==0);
	real maxEnergyInHit=SMALL_REAL;
	for(unsigned i=0;i<hits;i+=2)
	{
		ReflToDynobj *r2d=(ReflToDynobj *)(hit[i+1].getExtensionP());
		assert(r2d);
		assert(r2d->lightShots);
		assert(r2d->energyR>=0);
		assert(r2d->powerR2DSum>=0);
		assert(r2d->lightShots>0);
		assert(r2d->powerR2DSum/r2d->lightShots<=1);
		// energie vyletajici z reflektoru ke kouli
		real energyToSphere=r2d->energyR*(r2d->powerR2DSum/r2d->lightShots);
		// jak velkou jeji cast nese tento zasah
		real powerInHit;
		if(hit[i].getPower()>0)
			powerInHit=hit[i].getPower()/r2d->lightShotsP;
		else    powerInHit=hit[i].getPower()/r2d->shadeShotsP;
		// energie v jednom zasahu
		real energyInHit=energyToSphere*powerInHit;
		// ulozi ji na chvili do extension at to nemusi pocitat dvakrat
		hit[i+1].setExtensionR(energyInHit);
		// a jeste hleda meze
		maxEnergyInHit=MAX(fabs(energyInHit),maxEnergyInHit);
	}
	// smaze vsechny extended hity a znova je nainsertuje jako normalni
	unsigned myHits=hits/2;
	reset();
	for(unsigned i=0;i<myHits;i++)
	{
		// kondenzuje zasahy a predelava jim power
		// tak aby nejvyssi power(=1) znacila maxEnergyInHit
		Hit h=hit[2*i];
		h.setPower(hit[2*i+1].getExtensionR()/maxEnergyInHit);
		insert(h);
	}
	// returns value used later as powerToEnergyMultiplier
	return maxEnergyInHit;
}

#ifdef SUPPORT_LIGHTMAP

void Hits::convertDHitsToLightmap(Lightmap *l,real zoomToLightmap)
{
	assert(l);
	assert(l->bitmap);
	assert(isDynamic || !hits);
	assert((hits&1)==0);
	for(unsigned i=0;i<hits;i+=2)
	{
		ReflToDynobj *r2d=(ReflToDynobj *)(hit[i+1].getExtensionP());
		assert(r2d);
		assert(r2d->lightShots);
		assert(r2d->energyR>=0);
		assert(r2d->powerR2DSum>=0);
		assert(r2d->lightShots>0);
		assert(r2d->powerR2DSum/r2d->lightShots<=1);
		// energie vyletajici z reflektoru ke kouli
		real energyToSphere=r2d->energyR*(r2d->powerR2DSum/r2d->lightShots);
		// jak velkou jeji cast nese tento zasah
		real powerInHit;
		if(hit[i].getPower()>0)
			powerInHit=hit[i].getPower()/r2d->lightShotsP;
		else    powerInHit=hit[i].getPower()/r2d->shadeShotsP;
		// energie v jednom zasahu
		real energyInHit=energyToSphere*powerInHit;
		// zakresli hit do lightmapy
#ifdef HIT_12BIT
 #error only float hits supported here
#endif
#ifdef HIT_WORD
 #error only float hits supported here
#endif
		unsigned x=(unsigned)(hit[i].u*zoomToLightmap);
		unsigned y=(unsigned)(hit[i].v*zoomToLightmap);
		assert(x<l->w);
		assert(y<l->h);
		real limit=200;
		if(energyInHit<-limit) energyInHit=-limit;
		else if(energyInHit>limit) energyInHit=limit;
		l->bitmap[x+y*l->w]=(U8)((energyInHit/limit+1)*127.9);
	}
}

#endif

//////////////////////////////////////////////////////////////////////////////

#define SMALL_OBJECT_SIZE 1 // size of small object
 // not much important constant, wrong value leads to slightly lower quality

void Node::addEnergyDynamic(real e)
{
	if(__frameNumber!=energyDynamicFrameTime)
	{
		energyDynamicFrame=0;
		energyDynamicFrameTime=__frameNumber;
	}
	assert(IS_NUMBER(e));
	energyDynamicFrame+=e;
}

#define SMOOTHING 0.5

real Node::getEnergyDynamic()
{
	if(__frameNumber!=energyDynamicSmoothTime)
	{
		// adjust frame-energy for this frame
		if(__frameNumber!=energyDynamicFrameTime)
		{
			energyDynamicFrame*=pow(SMOOTHING,(U8)(__frameNumber-energyDynamicFrameTime));
			energyDynamicFrameTime=__frameNumber;
		}
		// adjust smooth-energy for this frame
		energyDynamicSmooth=pow(SMOOTHING,(U8)(__frameNumber-energyDynamicSmoothTime))*energyDynamicSmooth+(1-SMOOTHING)*energyDynamicFrame;
		energyDynamicSmoothTime=__frameNumber;
	}
	return energyDynamicSmooth;
}

real Node::importanceForDynamicShadows()
{
	assert(shooter);
	// importance of reflector when dyn.object size/distance is unknown
	// big energy -> high importance
	// small area (energy more concentrated) -> higher importance
	return fabs(shooter->energyDiffused+shooter->energyToDiffuse)
	       /(sqrt(area)+SMALL_OBJECT_SIZE);
}

real Node::importanceForDynamicShadows(Bound *objectBound)
{
	assert(shooter);
	// importance of reflector when dyn.object size/distance is known
	// near small object often makes no shadow -> 0
	// near samesize object makes shadow -> 1
	// near big object makes big shadow -> 2
	// distant object makes weak shadow -> /=distance^2
	Point3 point;
	if(IS_CLUSTER(this)) point=CLUSTER(this)->randomTriangle()->s3;else
	 if(IS_SUBTRIANGLE(this)) point=SUBTRIANGLE(this)->to3d(0);else
	  point=TRIANGLE(this)->s3;
	return fabs(shooter->energyDiffused+shooter->energyToDiffuse)
	       /(sqrt(area)+SMALL_OBJECT_SIZE)
	       *(objectBound->radius+SMALL_OBJECT_SIZE)
	       /(sizeSquare(objectBound->center-point)+SMALL_OBJECT_SIZE);
}

//////////////////////////////////////////////////////////////////////////////
//
// triangle, part of cluster and object

void Triangle::updateGeometryMoverot()
{
	// update s3,r3,l3
	s3=vertex[0]->transformedFromCache();
	r3=vertex[1]->transformedFromCache()-vertex[0]->transformedFromCache();
	l3=vertex[2]->transformedFromCache()-vertex[0]->transformedFromCache();

	// set intersectByte,intersectReal,u3,v3,n3
	setGeometryCore();

#ifndef NDEBUG
	// checks
	// s2,u2,v2 should stay unchanged in case of move/rotation
	real rsize=size(r3);
	real lsize=size(l3);
	assert(rsize>0);// we don't like degenerated triangles
	assert(lsize>0);
	real psqr=sizeSquare(u3-(l3/lsize));// ctverec nad preponou pri jednotkovejch stranach
	real cosa=1-psqr/2;
	real sina=/*sqrt(psqr*(1-psqr/4));*/sin(acos(cosa));
	assert(uv[0].x==0 && uv[0].y==0);
	assert(IS_EQ(u2.x,rsize) && IS_EQ(u2.y,0));
	assert(IS_EQ(v2.x,cosa*lsize) && IS_EQ(v2.y,sina*lsize));
	assert(IS_EQ(sqrt(area),sqrt(sina/2*rsize*lsize)));
	//assert(size(SubTriangle::to3d(2)-*vertex[2])<0.001);
/*        assert(cosa>=0);
	assert(v2.y<=MAX(u2.x,v2.x));
	assert(v3.x<=v2.x);
	assert(u2.x>=0);
	assert(u2.y==0);
	assert(v2.x>=0);
	assert(v2.y>=0);*/
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// struct describing one "reflector -> dynamic object" link

#define DYN_STABILITY 1 // higher=slower but safer against random ignoring some important shadow
#define PREFER_LIGHT 1 // importance of lighting dynamic objects against importance of casting their shadows

// initFrame se musi volat pred zacatkem kazdeho snimku

void ReflToDynobj::initFrame(Node *refl)
{
	// poznamena si kolik energie celkem leti z reflektoru
	assert(refl);
	assert(refl->shooter);
	energyR=refl->shooter->energyDiffused+refl->shooter->energyToDiffuse;
	// ostatni nuluje
	powerR2DSum=0;
	lightShots=0;
	lightHits=0;
	shadeShots=0;
	lightShotsP=0;
	lightHitsP=0;
	shadeShotsP=0;
	shadeHitsP=0;
	rawAccuracy=0;
	lightAccuracy=0;
}

void ReflToDynobj::updateLightAccuracy(Shooter *reflector)
{
	assert(reflector);
	rawAccuracy=lightShots/(fabs(reflector->energyDiffused+reflector->energyToDiffuse)+SMALL_ENERGY);
	real importance=( (visible?PREFER_LIGHT:0) + shadeHitsP/(shadeShotsP+DYN_STABILITY) )/(PREFER_LIGHT+1);
	lightAccuracy=rawAccuracy* lightShotsP/( lightHitsP*importance+DYN_STABILITY );
}

real ReflToDynobj::shadeAccuracy()
{
	return rawAccuracy* shadeShotsP/(shadeHitsP+DYN_STABILITY);
}

// neni zasazitelnej obj:           lightAcc=raw*MAX, shadeAcc=raw*0
// neni videt obj, neni videt stin: lightAcc=raw*MAX, shadeAcc=raw*MAX
// je videt obj, neni videt stin:   lightAcc=raw*2, shadeAcc=raw*MAX
// neni videt obj, je videt stin:   lightAcc=raw*2, shadeAcc=raw*1
// je videt obj, je videt stin:     lightAcc=raw*1, shadeAcc=raw*1

void ReflToDynobj::setImportance(real imp)
{
	importance=imp;
}

real ReflToDynobj::getImportance()
{
	return importance;
}

//////////////////////////////////////////////////////////////////////////////
//
// object, part of scene

DObject::DObject(int avertices,int atriangles) : Object(avertices,atriangles)
{
	trianglesEmiting=0;
	for(unsigned t=0;t<triangles;t++) triangle[t].object=this;
	transformMatrix=NULL;
	inverseMatrix=NULL;
	matrixDirty=false;
}

// transformuje boundingsphere z objectspace do scenespace

void DObject::transformBound()
{
	bound.center=bound.centerBeforeTransformation.transformed(transformMatrix);
}

// transformuje objekt z objectspace do scenespace
// paprsky se pak nemusi neustale transformovat
// zatim jsou mozne pouze posuny a rotace objektu

void DObject::transformAll(bool freeTransform)
{
	transformBound();
	// transformuje vertexy z objectspace do scenespace
	for(unsigned v=0;v<vertices;v++) vertex[v].transformToCache(transformMatrix);
	// aktualizuje podle nich triangly
	for(unsigned t=0;t<triangles;t++) triangle[t].updateGeometryMoverot();
	// pri deformaci aktualizuje prakticky vsechno
	assert(!freeTransform);
	/*if(freeTransform)
	{
		for(unsigned t=0;t<triangles;t++) triangle[t].deleteSubs();
		destroyEdges/Clusters...
		detectBounds();
		buildEdges();
		buildClusters();
		#ifdef SUPPORT_INTERPOL
		obj->buildTopIVertices();
		#endif
	}*/
}

DObject::~DObject()
{
}

//////////////////////////////////////////////////////////////////////////////
//
// set of reflectors (light sources and things that reflect light)
// dynamic version - first go nodesImportant, then others
//                 - gimmeLinks selects most important links for dyn.shooting

// importance of some reflectors is checked each frame and they may be moved
//  from unimportant reflectors to important or vice versa
#define REFLS_PART_PER_UPDATE 0.1 // this part of all reflectors is checked each update
#define ADD_REFLS_PER_UPDATE 10 // # of additional reflectors that are checked each update

#define MAX_IMPORTANCE_DECREASE_PER_UPDATE 0.98

DReflectors::DReflectors(Scene *ascene)
{
	assert(ascene);
	scene=ascene;
	r2d=NULL;
	r2dAllocated=0;
	reset();
}

void DReflectors::reset()
{
	nodesImportant=0;
	scanpos1=0;
	scanpos2=0;
	maxImportance=0;
	importanceLevelToAccept=0.1;
	importanceLevelToDeny=0.05;
	Reflectors::reset();
}

real DReflectors::importance(unsigned n)
{
	real imp=node[n]->importanceForDynamicShadows();
	maxImportance=MAX(maxImportance,imp);
	return imp;
}

void DReflectors::swapReflectors(unsigned n1,unsigned n2)
{
	Node *tmp=node[n1];
	node[n1]=node[n2];
	node[n2]=tmp;
}

// checks if node[n] is still unimportant and fixes it if necessary

void DReflectors::updateUnimportant(unsigned n)
{
	assert(n>=nodesImportant);
	assert(n<nodes);
	if(importance(n)>=maxImportance*importanceLevelToAccept)
	{
		swapReflectors(nodesImportant++,n);
	}
}

// checks if node[n] is still important and fixes it if necessary

void DReflectors::updateImportant(unsigned n)
{
	assert(n<nodesImportant);
	if(importance(n)<maxImportance*importanceLevelToDeny)
	{
		swapReflectors(n,--nodesImportant);
	}
}

// pretezuje insert ze statickych Reflectors
// - hned otestuje zda nepatri mezi important

bool DReflectors::insert(Node *anode)
{
	if(!Reflectors::insert(anode)) return false;
	updateUnimportant(nodes-1);
	return true;
}

// pretezuje remove ze statickych Reflectors
// - pokud je potreba, odstrani node z important

void DReflectors::remove(unsigned n)
{
	if(n<nodesImportant)
	{
		swapReflectors(n,--nodesImportant);
		n=nodesImportant;
	}
	Reflectors::remove(n);
}

void DReflectors::removeObject(Object *o)
{
	for(int i=nodes-1;i>=0;i--)
		if(o->contains(node[i])) remove(i);
}

void DReflectors::removeSubtriangles()
{
	for(int i=nodes-1;i>=0;i--)
		if(IS_SUBTRIANGLE(node[i])) remove(i);
}

void DReflectors::updateListForDynamicShadows()
{
	// slightly decrease maxImportance each time
	maxImportance*=MAX_IMPORTANCE_DECREASE_PER_UPDATE;
	// remove obsolete important nodes
	unsigned k=(unsigned)ceil(nodesImportant*REFLS_PART_PER_UPDATE)+ADD_REFLS_PER_UPDATE;
	k=MIN(k,nodesImportant);
	while(k--)
	{
		if(scanpos1>=nodesImportant) scanpos1=0;
		updateImportant(scanpos1);
		scanpos1++;
	}
	// add new important nodes
	k=(unsigned)ceil((nodes-nodesImportant)*REFLS_PART_PER_UPDATE)+ADD_REFLS_PER_UPDATE;
	k=MIN(k,(nodes-nodesImportant));
	while(k--)
	{
		if(scanpos2<nodesImportant || scanpos2>=nodes) scanpos2=nodesImportant;
		updateUnimportant(scanpos2);
		scanpos2++;
	}
}

#define MIN_DYNAMIC_SHOTS 100 // # of dynamic shots when there is no time
  // even in most huge scenes, 100 shots is fast enough for 70fps

#define MULT_SHOTS_EACH_PASS 2 // how many times increase # of shots each pass

#define PREFER_DYN_LIGHT 2 // how many times more important is light from
  // dyn.reflectors to scene than from static reflectors to dyn.objects

unsigned DReflectors::gimmeLinks(
 unsigned (shootFunc1)(Scene *scene,Node *refl,DObject *dynobj,ReflToDynobj* r2d,unsigned shots),
 unsigned (shootFunc2)(Scene *scene,Node *refl,unsigned shots),
 bool endfunc(Scene *))
{
	// this function is called once per frame
	// realloc r2d if not big enough
	unsigned r2dNeeded=(scene->objects-scene->staticObjects)*nodesImportant;
	if(r2dAllocated<r2dNeeded)
	{
		free(r2d);
		r2dAllocated=r2dNeeded+100;
		r2d=(ReflToDynobj *)malloc(r2dAllocated*sizeof(ReflToDynobj));
	}
	// init r2d slots and count sum of all importances
	real importanceSum=SMALL_REAL;
	for(unsigned o=scene->staticObjects;o<scene->objects;o++)
	{
		unsigned ndx=(o-scene->staticObjects)*nodesImportant;
		for(unsigned n=0;n<nodesImportant;n++)
		{
			real imp=node[n]->importanceForDynamicShadows(&scene->object[o]->bound);
			// init slot
			r2d[ndx+n].initFrame(node[n]);
			// set slot importance
			r2d[ndx+n].setImportance(imp);
			importanceSum+=imp;
		}
	}

	// energy to shoot from static lightsources
	// unfair speedup: counts only energy from primary emitors
	//  which is bad in case of big secondary emission in static scene
	real energyToShootFromStatics=scene->energyEmitedByStatics;
	// energy to shoot from dynamic lightsources
	// (dynamic objects have only primary emitors, no secondary)
	real energyToShootFromDynamics=scene->energyEmitedByDynamics;
	  //for(unsigned o=scene->staticObjects;o<scene->objects;o++) energyToShootFromDynamics+=fabs(scene->object[o]->energyEmited);
	// normalized demand for static/dynamic shots (sum of both=1)
	real staticDemand=energyToShootFromStatics/(energyToShootFromStatics+energyToShootFromDynamics*PREFER_DYN_LIGHT+SMALL_REAL);
	real dynamicDemand=energyToShootFromDynamics*PREFER_DYN_LIGHT/(energyToShootFromStatics+energyToShootFromDynamics*PREFER_DYN_LIGHT+SMALL_REAL);
	// repeat pass through all links with increasing shots per importance
	// until time elapses
	real shotsPerRound=MIN_DYNAMIC_SHOTS;
	unsigned shotsDoneTotalSum=0;//strel behem vsech kol
	unsigned prevShotsDoneSum=MIN_DYNAMIC_SHOTS/MULT_SHOTS_EACH_PASS;
	while(1)
	{
		unsigned shotsAllowedSum=0;
		unsigned shotsDoneSum=0;//strel behem jednoho kola
		// for each dyn.object
		for(unsigned o=scene->staticObjects;o<scene->objects;o++)
		{
			unsigned ndx=(o-scene->staticObjects)*nodesImportant;
			// static reflectors shoot to this object
			for(unsigned n=0;n<nodesImportant;n++)
			{
				unsigned shotsAllowed=(unsigned)(r2d[ndx+n].getImportance()/(importanceSum+SMALL_REAL)*shotsPerRound*staticDemand);
				shotsAllowedSum+=shotsAllowed;
				if(shotsAllowed) shotsDoneSum+=shootFunc1(scene,node[n],scene->object[o],&r2d[ndx+n],shotsAllowed);
			}
			// dynamic reflectors shoot from this object
			for(unsigned t=0;t<scene->object[o]->trianglesEmiting;t++)
			{
				real triangleEmits=scene->object[o]->triangle[t].surface->diffuseEmittance*scene->object[o]->triangle[t].area;
				unsigned shotsAllowed=(unsigned)(fabs(triangleEmits)/(energyToShootFromDynamics+SMALL_REAL)*shotsPerRound*dynamicDemand);
				shotsAllowedSum+=shotsAllowed;
				if(shotsAllowed) shotsDoneSum+=shootFunc2(scene,&scene->object[o]->triangle[t],shotsAllowed);
			}
		}
		shotsDoneTotalSum+=shotsDoneSum;
		if(!shotsAllowedSum) break;
		if(endfunc(scene)) break;
		// change shotsPerImportance to double shotsDoneSum
		shotsPerRound*=MULT_SHOTS_EACH_PASS*MULT_SHOTS_EACH_PASS*prevShotsDoneSum/(shotsDoneSum+MIN_DYNAMIC_SHOTS/10.f);//+neco aby nedelil nulou pri !shotsDoneSum
		prevShotsDoneSum=shotsDoneSum;
	}
	return shotsDoneTotalSum;
}

DReflectors::~DReflectors()
{
	free(r2d);
}

//////////////////////////////////////////////////////////////////////////////

static real powerToSphere(real sphereSizeCode,real spherePositionCode)
{
	// returns which part of energy diffuses to sphere=0..1
	// sphereSizeCode=sin(uhel stredkoule-oko-tecnakoule)=0..1
	// spherePositionCode=ctverec rozdilu normaly a smeru ke kouli=0..4
	assert(sphereSizeCode>=0);
	assert(spherePositionCode>=0 && spherePositionCode<=4);
	if(sphereSizeCode>1) return 1;
	const int STEPS=32;   // jak jemna je tabulka
	const float table[(STEPS+1)*(STEPS+1)]={ // vygeneroval maketab.cpp
 0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.001002,0.000905,0.000862,0.000775,0.000689,0.000671,0.000616,0.000592,0.000492,0.000449,0.000360,0.000321,0.000252,0.000200,0.000124,0.000067,0.000007,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.003863,0.003638,0.003406,0.003218,0.002918,0.002643,0.002388,0.002209,0.002001,0.001746,0.001488,0.001194,0.001024,0.000720,0.000506,0.000234,0.000049,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.008941,0.008161,0.007793,0.007031,0.006438,0.005866,0.005494,0.004916,0.004416,0.003937,0.003311,0.002719,0.002190,0.001570,0.001147,0.000559,0.000184,0.000011,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.015558,0.014569,0.013621,0.012912,0.011670,0.010732,0.009670,0.008766,0.007900,0.006637,0.005761,0.004900,0.003871,0.003063,0.002016,0.001065,0.000415,0.000082,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.024548,0.022957,0.021337,0.019575,0.018183,0.016895,0.015280,0.013680,0.012170,0.010642,0.009125,0.007758,0.006067,0.004617,0.003088,0.001804,0.000847,0.000260,0.000020,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.035361,0.032793,0.030819,0.028445,0.026398,0.024254,0.022122,0.019633,0.017557,0.015107,0.013180,0.011047,0.008771,0.006645,0.004515,0.002723,0.001407,0.000555,0.000099,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.047722,0.044808,0.041671,0.038909,0.035527,0.032642,0.029826,0.027045,0.023784,0.020884,0.017812,0.014996,0.011916,0.008832,0.006316,0.004010,0.002257,0.001049,0.000286,0.000018,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.062514,0.058816,0.054377,0.050857,0.046790,0.042994,0.039047,0.034957,0.031658,0.027262,0.023443,0.019528,0.015662,0.011749,0.008376,0.005697,0.003315,0.001688,0.000666,0.000111,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.078970,0.074239,0.069209,0.064572,0.059101,0.054157,0.049075,0.044639,0.039887,0.034429,0.029583,0.024962,0.019784,0.015294,0.011042,0.007620,0.004874,0.002694,0.001237,0.000375,0.000021,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.097796,0.091512,0.085400,0.079668,0.072803,0.067151,0.060985,0.054758,0.048594,0.042559,0.036935,0.030431,0.024808,0.018990,0.014130,0.010102,0.006639,0.003921,0.002066,0.000752,0.000139,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.118212,0.110572,0.103982,0.095841,0.088478,0.081571,0.073827,0.066368,0.059199,0.051915,0.044561,0.037000,0.029984,0.023514,0.017851,0.012756,0.009109,0.005697,0.003200,0.001453,0.000373,0.000025,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.140303,0.132088,0.123163,0.114351,0.105377,0.096763,0.088074,0.079073,0.070181,0.061894,0.052581,0.044360,0.035833,0.028552,0.022262,0.016676,0.011569,0.007663,0.004566,0.002403,0.000882,0.000161,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.164868,0.154474,0.143956,0.133157,0.123656,0.113390,0.103258,0.093144,0.082806,0.072355,0.062116,0.051976,0.043141,0.034701,0.027085,0.020638,0.015003,0.010171,0.006491,0.003746,0.001650,0.000526,0.000030,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.191438,0.179870,0.167863,0.154745,0.143928,0.131309,0.119462,0.107750,0.095037,0.083584,0.071568,0.060930,0.050652,0.041238,0.032940,0.025295,0.018748,0.013625,0.009024,0.005481,0.002629,0.001110,0.000212,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.220315,0.206208,0.191983,0.177801,0.165081,0.151608,0.137174,0.123823,0.109905,0.096435,0.082796,0.070405,0.058872,0.048862,0.039157,0.030803,0.023404,0.017244,0.012014,0.007607,0.004180,0.002018,0.000589,0.000040,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.250044,0.234399,0.218197,0.202859,0.186948,0.171991,0.156755,0.141233,0.125128,0.109455,0.095019,0.081213,0.068559,0.057191,0.046275,0.037544,0.028657,0.021839,0.015275,0.010220,0.006142,0.003252,0.001181,0.000235,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.282517,0.265047,0.247288,0.229125,0.210918,0.194607,0.176186,0.159283,0.140746,0.124168,0.108730,0.093469,0.079503,0.066348,0.054990,0.044137,0.034646,0.026879,0.019739,0.013631,0.008802,0.005093,0.002309,0.000684,0.000055,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.316096,0.296509,0.276965,0.257235,0.237247,0.217435,0.197798,0.178274,0.157977,0.140272,0.122623,0.106548,0.091081,0.077364,0.064500,0.052625,0.042060,0.032664,0.024765,0.017767,0.012027,0.007357,0.003738,0.001468,0.000289,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.352957,0.330432,0.308362,0.286842,0.264371,0.242430,0.220448,0.198728,0.176980,0.156881,0.137519,0.120350,0.103827,0.089104,0.074762,0.062193,0.050312,0.040150,0.031010,0.022552,0.015734,0.010253,0.005631,0.002714,0.000827,0.000053,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.390896,0.366220,0.341464,0.317138,0.293225,0.268007,0.244421,0.219554,0.197725,0.175444,0.155022,0.135602,0.118124,0.101754,0.087033,0.072264,0.059826,0.048134,0.037625,0.028210,0.020641,0.013898,0.008426,0.004582,0.001718,0.000349,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.430059,0.403211,0.377070,0.349595,0.323774,0.295495,0.269648,0.243072,0.217836,0.194987,0.173453,0.153760,0.134145,0.116459,0.099476,0.084577,0.070210,0.057580,0.046047,0.035440,0.026516,0.018752,0.012090,0.007102,0.003176,0.001036,0.000081,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.472808,0.442312,0.413118,0.383930,0.354687,0.325123,0.295952,0.268809,0.241718,0.217552,0.193563,0.172837,0.151405,0.132554,0.114052,0.097717,0.081868,0.068286,0.055353,0.043815,0.033144,0.024172,0.016698,0.010231,0.005579,0.002314,0.000431,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.517155,0.483868,0.452120,0.419480,0.389063,0.354618,0.323803,0.294614,0.267322,0.240181,0.215942,0.192849,0.171437,0.149820,0.130978,0.113305,0.096367,0.081016,0.066274,0.053672,0.041809,0.031313,0.022285,0.014633,0.008614,0.004018,0.001340,0.000106,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.562278,0.526963,0.492509,0.457002,0.422014,0.387508,0.355000,0.323736,0.294126,0.266362,0.239859,0.215132,0.192117,0.170445,0.149666,0.129760,0.112304,0.094458,0.079736,0.065366,0.051979,0.039590,0.029356,0.020144,0.012909,0.007101,0.002827,0.000619,0.000000,0.000000,0.000000,0.000000,0.000000,
 0.609843,0.571987,0.533459,0.495251,0.457895,0.421030,0.387416,0.354827,0.323681,0.294290,0.267726,0.240965,0.216308,0.193180,0.170767,0.150675,0.130533,0.111823,0.094393,0.078240,0.063797,0.050164,0.038272,0.027392,0.018227,0.011121,0.005404,0.001820,0.000109,0.000000,0.000000,0.000000,0.000000,
 0.660531,0.618853,0.577766,0.536600,0.495578,0.457478,0.421175,0.389352,0.356586,0.325464,0.296385,0.269738,0.243473,0.217713,0.194439,0.172211,0.151007,0.130385,0.112597,0.094651,0.078325,0.063043,0.048998,0.036657,0.025956,0.016665,0.009342,0.003946,0.000779,0.000000,0.000000,0.000000,0.000000,
 0.711871,0.667518,0.623595,0.578724,0.536229,0.496981,0.460704,0.424688,0.392214,0.360393,0.329053,0.300369,0.273712,0.247602,0.222195,0.198128,0.175256,0.153573,0.133101,0.114022,0.095396,0.078710,0.062868,0.048455,0.035781,0.024478,0.015046,0.007755,0.002501,0.000210,0.000000,0.000000,0.000000,
 0.766099,0.716762,0.669590,0.623409,0.580612,0.539848,0.502549,0.466244,0.431436,0.398217,0.367864,0.337021,0.308862,0.281238,0.254447,0.229192,0.204291,0.180849,0.157749,0.137015,0.117083,0.098413,0.080291,0.064144,0.048639,0.035235,0.023401,0.013518,0.006149,0.001386,0.000000,0.000000,0.000000,
 0.821831,0.770804,0.719293,0.672058,0.627572,0.588148,0.548142,0.511127,0.477253,0.442731,0.412023,0.379406,0.349163,0.319915,0.292321,0.265206,0.239046,0.213811,0.190343,0.166132,0.144293,0.122630,0.102953,0.084345,0.066805,0.050063,0.035221,0.022644,0.012153,0.004398,0.000442,0.000000,0.000000,
 0.879179,0.824267,0.771475,0.725090,0.682030,0.641276,0.603935,0.565544,0.529920,0.496160,0.462449,0.430029,0.399041,0.369218,0.339675,0.311000,0.283150,0.256918,0.229535,0.203691,0.179414,0.156050,0.133158,0.111440,0.090404,0.070989,0.053811,0.037474,0.022974,0.011038,0.003010,0.000000,0.000000,
 0.938403,0.881014,0.832187,0.788517,0.747049,0.707570,0.670312,0.634757,0.598363,0.564208,0.530539,0.497276,0.466862,0.434372,0.404048,0.373046,0.342721,0.314665,0.285917,0.258302,0.231523,0.205100,0.178891,0.153803,0.129753,0.106018,0.084060,0.063029,0.043492,0.025499,0.011008,0.001458,0.000000,
 1.000000,0.968518,0.937163,0.906535,0.874761,0.843931,0.813314,0.781227,0.749733,0.718285,0.687276,0.656042,0.625004,0.594260,0.562244,0.531578,0.498883,0.468271,0.437781,0.405738,0.375448,0.343814,0.312189,0.281199,0.249667,0.218313,0.187118,0.156768,0.125090,0.093185,0.062393,0.031447,0.000000
	};
	// normalizes inputs to fit table
	sphereSizeCode*=STEPS;
	spherePositionCode*=STEPS/4;

	int sizf=(int)floor(sphereSizeCode);
	if(sizf==STEPS) sizf--;
	int posf=(int)floor(spherePositionCode);
	if(posf==STEPS) posf--;
	return (sizf+1-sphereSizeCode)*(posf+1-spherePositionCode)*table[sizf*(STEPS+1)+posf]+
	       (sizf+1-sphereSizeCode)*(spherePositionCode-posf)*table[sizf*(STEPS+1)+posf+1]+
	       (sphereSizeCode-sizf)*(posf+1-spherePositionCode)*table[(sizf+1)*(STEPS+1)+posf]+
	       (sphereSizeCode-sizf)*(spherePositionCode-posf)*table[(sizf+1)*(STEPS+1)+posf+1];
}

static unsigned staticToDynobjShotFunc(Scene *scene,Node *refl,DObject *dynobj,ReflToDynobj* r2d,unsigned shotsAllowed)
{
	assert(r2d->powerR2DSum>=0);

	// vystrili zadanej pocet naboju
	unsigned shotsDone=0;
	assert(shotsAllowed);

	// pokud je lightHits o moc mensi nez lightShots, nema moc cenu
	//  strilet dalsi
	// ...

	while(shotsAllowed--)
	{


	// select source subtriangle
	SubTriangle *source;
	if(IS_CLUSTER(refl)) source=CLUSTER(refl)->randomTriangle();
	else source=SUBTRIANGLE(refl);

	// select random point in source subtriangle
	real u=rand();
	real v=rand();
	if(u+v>RAND_MAX)
	{
		u=RAND_MAX-u;
		v=RAND_MAX-v;
	}

	Point2 srcPoint2=source->uv[0]+source->u2*(u/RAND_MAX)+source->v2*(v/RAND_MAX);
	Point3 srcPoint3=source->grandpa->to3d(srcPoint2);

#ifdef TRANSFORM_SHOTS
	// transform from shooter's objectspace to scenespace
	Point3 srcPoint3o=srcPoint3;
	srcPoint3.transform(source->grandpa->object->transformMatrix);
#endif

	// vyber nahodneho vektoru ze srcPoint3 ke kouli,
	// urceni jeho sily,
	// aktualizace udaju k pozdejsimu prepoctu power v hitu na energii
	// (jak moc energie tomu paprsku nalozit?
	// nalozit mu power=1 a pointer na r2d odkud pozdejc pujde
	// prepocitat z relativni power na absolutni energy)
	Vec3 toSphereCenter=dynobj->bound.center-srcPoint3;
	real toSphereCenterSize=size(toSphereCenter);
	real powerR2D;
	Vec3 rayVec3;
	real shotPower;
	if(dynobj->bound.radius>toSphereCenterSize)
	{
		// source inside sphere -> all energy goes to sphere
		powerR2D=1;

		// shoot to whole halfspace
		real sina=sqrt( (real)rand()/RAND_MAX );
		Angle a=asin(sina);                     // rotation angle a from normal to side
		Angle b=rand()*2*M_PI/RAND_MAX;         // rotation angle b around normal
		rayVec3=source->grandpa->n3*cos(a)
		       +source->grandpa->u3*(sina*cos(b))
		       +source->grandpa->v3*(sina*sin(b));
		shotPower=1;
	}
	else
	{
		// source outside sphere -> 0..1 of energy goes to sphere
		// sphereSizeCode=sin(uhel stredkoule-oko-tecnakoule)
		real sphereSizeCode=dynobj->bound.radius/toSphereCenterSize;
		// spherePositionCode=ctverec rozdilu normaly a smeru ke kouli
		real spherePositionCode=sizeSquare(source->grandpa->n3-toSphereCenter/toSphereCenterSize);
		powerR2D=powerToSphere(sphereSizeCode,spherePositionCode);

		// shoot to sphere but not under source surface
		Vec3 toSphereCenterNorm=toSphereCenter/toSphereCenterSize;
		Vec3 toSphereRight=normalized(ortogonalTo(toSphereCenterNorm));
		Vec3 toSphereFront=ortogonalTo(toSphereCenterNorm,toSphereRight);
		int tries=0;
	      retry:
		// select random vector from srcPoint3 to dynobj boundingsphere
		// predstava: boundingsphere obtisknu na polokouli kolem
		//  strelce. cela potistena plocha bude mit stejnou pravdepodobnost
		//  zasahu, ale to vejs nad obzorem pak dostane vetsi power,
		//  to pod obzorem se zahodi a zkusi vygenerovat znova.
		real maxa=asin(sphereSizeCode);
		assert(IS_NUMBER(maxa));
		// zamirim do stredu a urcim o kolik odklonit do strany (a)
		//  a o kolik zarotovat (b)
		// chci A z intervalu 0..maxa a aby 2x vetsi A bylo skoro
		//  2x pravdepodobnejsi (ta kruznice v obtisku na povrchu
		//  polokoule je skoro 2x delsi)
		// varianta 1: ma tendenci strilet ke stredu objektu, vic hitu
		//Angle a=maxa*rand()/RAND_MAX;
		// varianta 2: ma tendenci strilet ke krajum objektu
		Angle a=maxa*sqrt((real)rand()/RAND_MAX);
		Angle b=2*M_PI*rand()/RAND_MAX;
		rayVec3=toSphereCenterNorm*cos(a)
		       +toSphereRight*(sin(a)*cos(b))
		       +toSphereFront*(sin(a)*sin(b));
		assert(fabs(sizeSquare(rayVec3)-1)<0.001);
		// shotPower=cos(uhel mezi n3 a rayVec3)=1-sqr(rozdil tech vektoru)/2;
		shotPower=1-sizeSquare(rayVec3-source->grandpa->n3)/2;
		// osetri pripad kdy miri dovnitr zarice=moc daleko od normaly
		if(shotPower<=0.001)
		{
			if(tries++<15) goto retry;
			break; // too hard to hit, continuing has no sense
		}
	}

#ifdef TRANSFORM_SHOTS
	// transform from shooter's objectspace to scenespace
	rayVec3=(srcPoint3o+rayVec3).transformed(source->grandpa->object->transformMatrix)-srcPoint3;
#endif

	assert(r2d->powerR2DSum>=0);
	assert(powerR2D>=0 && powerR2D<=1);
	r2d->powerR2DSum+=powerR2D;
	// find intersection with dynamic object
	// and when successful, insert hitTriangle to hitTriangles
	Triangle *hitTriangle;
	Hit      hitPoint2d;
	bool     hitOuterSide;
	real     hitDistance=size(srcPoint3-dynobj->bound.center)+dynobj->bound.radius;
	bool hit=scene->intersectionDynobj(srcPoint3,rayVec3,source->grandpa,dynobj,&hitTriangle,&hitPoint2d,&hitOuterSide,&hitDistance);
	r2d->lightShots++;
	r2d->lightShotsP+=shotPower;
	__lightShotsPerDynamicFrame++;
	shotsDone++;

	// when dynobj was hit...
	if(hit)
	{

		assert(dynobj->contains(hitTriangle));
		r2d->lightHits++;
		r2d->lightHitsP+=shotPower;

		// insert extended lighthit to hitTriangle
		hitPoint2d.setPower(shotPower);
		hitTriangle->hits.insert(hitPoint2d,r2d);
		// odraz a prunik svetla skrz dynobj zde ignoruje

//...                if(r2d->shadeAccuracy()<accuracy)
		{
			r2d->shadeShots++;
			r2d->shadeShotsP+=shotPower;
			__shadeShotsPerDynamicFrame++;
			// calculate hitpoint
			Point3 hitPoint3d=srcPoint3+rayVec3*hitDistance;
			//object.rayTrace musi power nasobit dulezitosti zasazenejch ploch
			// zasazeny staticky triangly jdou do hitTriangles,
			// pozdejc ale jeste pred zobrazenim se z nich
			// skrz jejich staticky faktory rozsirej dynamicky stiny.
			// power=1 ale k hitum se uklada i pointer r2d ze kteryho jde
			// spocitat skutecny mnozstvi preneseny energie.
			// (odecita zapornou energii aby se shadeHits zvysilo)
			r2d->shadeHitsP-=scene->rayTracePhoton(hitPoint3d,rayVec3,hitTriangle,r2d,-shotPower);
		}
	}
	else
	// kresli pixely i kdyz se do dynobj netrefi
	if(d_drawDynamicHits==2) scene->rayTracePhoton(srcPoint3,rayVec3,source->grandpa,r2d,shotPower);

	r2d->updateLightAccuracy(source->shooter);


	}
	return shotsDone;
}

static unsigned dynlightShotFunc(Scene *scene,Node *refl,unsigned shotsAllowed)
{
	//...
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// read and clear hits, possibly split to subtriangles, add dynamic energy

static void convertHitsToEnergyDynamic(SubTriangle *destination,Hits *phits,real powerToEnergyMultiplier)
{
	assert(destination);
	assert(phits);
	// na rozhodovani o deleni stacej relativni udaje o energii
	if(phits->hits==0) return;
	Point2 triCentre=destination->uv[0]+(destination->u2+destination->v2)/3;
	real perimeter=destination->perimeter();
	bool doSplit=phits->doSplit(triCentre,perimeter,destination->grandpa);
	if(doSplit)
	{
		// i na deleni stacej relativni udaje o energii
		destination->splitGeometry(NULL);
		Hits *phits2=allocHitsLevel();
		destination->splitHits(phits,phits2);
		convertHitsToEnergyDynamic(SUBTRIANGLE(destination->sub[0]),phits2,powerToEnergyMultiplier);
		freeHitsLevel();
		convertHitsToEnergyDynamic(SUBTRIANGLE(destination->sub[1]),phits,powerToEnergyMultiplier);
	}
	else
	{
		// az ve finale je potreba absolutni udaj o mnozstvi energie
		real e=phits->totalPower()*powerToEnergyMultiplier;
		assert(IS_NUMBER(e));
		destination->addEnergyDynamic(e);//e is premultiplied by destination->grandpa->surface->diffuseReflectance
		phits->reset();
	}
}
/*
static void distributeEnergyDynamicViaFactor(Factor *factor,va_list ap)
{
	double energy=va_arg(ap,double);

	Node *destination=factor->destination;
	assert(destination);
	assert(factor->power>=0);
	assert(factor->power<=1);
	energy*=factor->power;
	// preda energii
	destination->addEnergyDynamic(energy);
	// nastavi nektery dirty
	//...mozna by jednou stacilo DIRTY_ALL_SUBNODES na cluster a nedistribuovat to dolu do trianglu
	if(IS_CLUSTER(destination))
		CLUSTER(destination)->makeDirty();
	else
		SUBTRIANGLE(destination)->makeDirty();
	// pak leze nahoru az k trianglu, do clusteru neni treba
	// a nastavuje dalsi dirty flagy
	do
	{
		destination->flags|=FLAG_DIRTY_NODE;
		destination=destination->parent;
	}
	while(destination && !IS_CLUSTER(destination));
}
*/
// improveDynamic je jednorazova akce predchazejici zobrazeni snimku,
// nema smysl volat pred zobrazenim vickrat
// ...todo: dokud se nic nepohne, melo by jit improvovat porad dal
//          a prubezne zobrazovat

unsigned __lightShotsPerDynamicFrame=0;
unsigned __shadeShotsPerDynamicFrame=0;

void Scene::improveDynamic(bool endfunc(Scene *))
{
	// dynamic shadows use the same structures as static, only one may
	//  run at time
	if(shortenStaticImprovementIfBetterThan(1)) finishStaticImprovement();
	else abortStaticImprovement();
	assert(!improvingStatic);
	// dynamic shooting fills hits
	staticReflectors.updateListForDynamicShadows();
	__lightShotsPerDynamicFrame=0;
	__shadeShotsPerDynamicFrame=0;
	staticReflectors.gimmeLinks(staticToDynobjShotFunc,dynlightShotFunc,endfunc);
	// no hits, no dynamic light
	if(!__lightShotsPerDynamicFrame) return;
	// drawing hits = no need to convert them to energyDynamic
	if(d_drawDynamicHits) return;
	// convert hits to energyDynamic
	Triangle *hitTriangle;
	while((hitTriangle=hitTriangles.get()))
	{
		// convert triangle hits from extended to plain
		real powerToEnergyMultiplier=hitTriangle->hits.convertDHitsToHits();
		// premultiply it by surface reflectance
		real e=powerToEnergyMultiplier*hitTriangle->surface->diffuseReflectance;
		// convert triangle hits to energyDynamic in subtriangles
		// (clustering skipped here)
		convertHitsToEnergyDynamic(hitTriangle,&hitTriangle->hits,e);
#ifdef SUPPORT_LIGHTMAP
		hitTriangle->updateLightmapSize(false);
#endif
/*
		// distribute energyDynamic via static factors
		// (in other words - apply one diffuse reflection)
		// (further diffuse reflections are ignored as probably weak)
	     // chyba: tohle by distribuovalo jen ze zasazenejch trianglu
	     //  ale ne ze subtrianglu kam se mohla energie propadnout
		if(hitTriangle->energyDynamic>...)
		{
			assert(hitTriangle->shooter);
			real ed=hitTriangle->getEnergyDynamic();
			hitTriangle->shooter->forEach(distributeEnergyDynamicViaFactor,ed);
		}
*/
	}
	hitTriangles.reset();//pro jistotu at nemuze resurrectnout
}

void Scene::objInsertDynamic(TObject *o)
{
	if(objects==allocatedObjects)
	{
		size_t oldsize=allocatedObjects*sizeof(TObject *);
		allocatedObjects*=4;
		object=(TObject **)realloc(object,oldsize,allocatedObjects*sizeof(TObject *));
	}
	object[objects++]=o;

	energyEmitedByDynamics+=o->energyEmited;
}

void Scene::objRemoveDynamic(unsigned o)
{
	assert(o>=staticObjects && o<objects);

	energyEmitedByDynamics-=object[o]->energyEmited;

	object[o]=object[--objects];
}

void Scene::objMakeStatic(unsigned o)
{
	assert(o>=staticObjects && o<objects);
	TObject *tmp=object[o];
	object[o]=object[staticObjects];
	object[staticObjects++]=tmp;

	energyEmitedByDynamics-=tmp->energyEmited;
	energyEmitedByStatics+=tmp->energyEmited;

	staticReflectors.insertObject(tmp);
}

void Scene::objMakeDynamic(unsigned o)
{
	assert(o<staticObjects);
	DObject *tmp=object[o];
	object[o]=object[--staticObjects];
	object[staticObjects]=tmp;

	energyEmitedByStatics-=tmp->energyEmited;
	energyEmitedByDynamics+=tmp->energyEmited;

	staticReflectors.removeObject(tmp);
}

void Scene::transformObjects()
{
	for(unsigned o=0;o<objects;o++)
	    // transformuje jen kdyz se matice od minule zmenila
	    if(object[o]->matrixDirty)
	    {
		#ifdef TRANSFORM_SHOTS
		// transformuje ted jen sphere a pak vsechny paprsky
		object[o]->transformBound();
		#else
		// transformuje ted i vsechny vertexy a pak nemusi paprsky
		//...potrebuje tenhle objekt freeTransform?
		bool freeTransform=false;
		object[o]->transformAll(freeTransform);
		#endif
		object[o]->matrixDirty=false;
	    }
}

/*
buga v interpolaci na vrsku 8stennyho bodaku
pocitat pri gouraudu i energyDirect z clusteru
pocitat pri flatu i energyDirect z clusteru
reflector meshing je pomalej protoze pulreflektory nemaj energii na strileni, musi se odebrat reflektoru a dat jim.
Factor *factor alokovat az pri prvnim insertu
co pujde z Shooter presunout do Scene

optimalizace dynashadingu:
-vedet predem ktery facy jsou jak moc videt a pro neviditelny nic nepocitat
-sledovat esli je lepsi testovat nejdriv zda vubec zasahuje dynobj
 nebo rovnou hledat nejblizsi prusecik ne vzdalenejsi nez konec dynobj
-vedet jaky pary dynobj-refl byly minule nejvytizenejsi a venovat se prednostne jim
-v kazdym nodu sledovat kolisavost dynalightu a kdyz je mensi, zvetsit setrvacnost
*/

#endif
