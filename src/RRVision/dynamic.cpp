#ifdef SUPPORT_DYNAMIC

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "rrcore.h"
#include "dynamic.h"

namespace rr
{


U8 d_drawDynamicHits=0;// 0=normal energy, 1=hits->pixels, 2=all shots->pixels

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

#define SMOOTHING 0.5f

real Node::getEnergyDynamic()
{
	if(__frameNumber!=energyDynamicSmoothTime)
	{
		// adjust frame-energy for this frame
		if(__frameNumber!=energyDynamicFrameTime)
		{
			energyDynamicFrame*=powf(SMOOTHING,(U8)(__frameNumber-energyDynamicFrameTime));
			energyDynamicFrameTime=__frameNumber;
		}
		// adjust smooth-energy for this frame
		energyDynamicSmooth=powf(SMOOTHING,(U8)(__frameNumber-energyDynamicSmoothTime))*energyDynamicSmooth+(1-SMOOTHING)*energyDynamicFrame;
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
	return fabs(shooter->totalExitingFluxDiffused+shooter->totalExitingFluxToDiffuse)
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
	if(IS_CLUSTER(this)) point=CLUSTER(this)->randomTriangle()->getS3();else
	 if(IS_SUBTRIANGLE(this)) point=SUBTRIANGLE(this)->to3d(0);else
	  point=TRIANGLE(this)->getS3();
	return fabs(shooter->totalExitingFluxDiffused+shooter->totalExitingFluxToDiffuse)
	       /(sqrt(area)+SMALL_OBJECT_SIZE)
	       *(objectBound->radius+SMALL_OBJECT_SIZE)
	       /(size2(objectBound->center-point)+SMALL_OBJECT_SIZE);
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
	energyR=refl->shooter->totalExitingFluxDiffused+refl->shooter->totalExitingFluxToDiffuse;
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
	rawAccuracy=lightShots/(fabs(reflector->totalExitingFluxDiffused+reflector->totalExitingFluxToDiffuse)+SMALL_ENERGY);
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
// set of reflectors (light sources and things that reflect light)
// dynamic version - first go nodesImportant, then others
//                 - gimmeLinks selects most important links for dyn.shooting

// importance of some reflectors is checked each frame and they may be moved
//  from unimportant reflectors to important or vice versa
#define REFLS_PART_PER_UPDATE 0.1 // this part of all reflectors is checked each update
#define ADD_REFLS_PER_UPDATE 10 // # of additional reflectors that are checked each update

#define MAX_IMPORTANCE_DECREASE_PER_UPDATE 0.98f

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
	importanceLevelToAccept=0.1f;
	importanceLevelToDeny=0.05f;
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
 unsigned (shootFunc1)(Scene *scene,Node *refl,Object *dynobj,ReflToDynobj* r2d,unsigned shots),
 unsigned (shootFunc2)(Scene *scene,Node *refl,unsigned shots),
 bool endfunc(void *),void *context)
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
		if(endfunc(context)) break;
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
 0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.001002f,0.000905f,0.000862f,0.000775f,0.000689f,0.000671f,0.000616f,0.000592f,0.000492f,0.000449f,0.000360f,0.000321f,0.000252f,0.000200f,0.000124f,0.000067f,0.000007f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.003863f,0.003638f,0.003406f,0.003218f,0.002918f,0.002643f,0.002388f,0.002209f,0.002001f,0.001746f,0.001488f,0.001194f,0.001024f,0.000720f,0.000506f,0.000234f,0.000049f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.008941f,0.008161f,0.007793f,0.007031f,0.006438f,0.005866f,0.005494f,0.004916f,0.004416f,0.003937f,0.003311f,0.002719f,0.002190f,0.001570f,0.001147f,0.000559f,0.000184f,0.000011f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.015558f,0.014569f,0.013621f,0.012912f,0.011670f,0.010732f,0.009670f,0.008766f,0.007900f,0.006637f,0.005761f,0.004900f,0.003871f,0.003063f,0.002016f,0.001065f,0.000415f,0.000082f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.024548f,0.022957f,0.021337f,0.019575f,0.018183f,0.016895f,0.015280f,0.013680f,0.012170f,0.010642f,0.009125f,0.007758f,0.006067f,0.004617f,0.003088f,0.001804f,0.000847f,0.000260f,0.000020f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.035361f,0.032793f,0.030819f,0.028445f,0.026398f,0.024254f,0.022122f,0.019633f,0.017557f,0.015107f,0.013180f,0.011047f,0.008771f,0.006645f,0.004515f,0.002723f,0.001407f,0.000555f,0.000099f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.047722f,0.044808f,0.041671f,0.038909f,0.035527f,0.032642f,0.029826f,0.027045f,0.023784f,0.020884f,0.017812f,0.014996f,0.011916f,0.008832f,0.006316f,0.004010f,0.002257f,0.001049f,0.000286f,0.000018f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.062514f,0.058816f,0.054377f,0.050857f,0.046790f,0.042994f,0.039047f,0.034957f,0.031658f,0.027262f,0.023443f,0.019528f,0.015662f,0.011749f,0.008376f,0.005697f,0.003315f,0.001688f,0.000666f,0.000111f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.078970f,0.074239f,0.069209f,0.064572f,0.059101f,0.054157f,0.049075f,0.044639f,0.039887f,0.034429f,0.029583f,0.024962f,0.019784f,0.015294f,0.011042f,0.007620f,0.004874f,0.002694f,0.001237f,0.000375f,0.000021f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.097796f,0.091512f,0.085400f,0.079668f,0.072803f,0.067151f,0.060985f,0.054758f,0.048594f,0.042559f,0.036935f,0.030431f,0.024808f,0.018990f,0.014130f,0.010102f,0.006639f,0.003921f,0.002066f,0.000752f,0.000139f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.118212f,0.110572f,0.103982f,0.095841f,0.088478f,0.081571f,0.073827f,0.066368f,0.059199f,0.051915f,0.044561f,0.037000f,0.029984f,0.023514f,0.017851f,0.012756f,0.009109f,0.005697f,0.003200f,0.001453f,0.000373f,0.000025f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.140303f,0.132088f,0.123163f,0.114351f,0.105377f,0.096763f,0.088074f,0.079073f,0.070181f,0.061894f,0.052581f,0.044360f,0.035833f,0.028552f,0.022262f,0.016676f,0.011569f,0.007663f,0.004566f,0.002403f,0.000882f,0.000161f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.164868f,0.154474f,0.143956f,0.133157f,0.123656f,0.113390f,0.103258f,0.093144f,0.082806f,0.072355f,0.062116f,0.051976f,0.043141f,0.034701f,0.027085f,0.020638f,0.015003f,0.010171f,0.006491f,0.003746f,0.001650f,0.000526f,0.000030f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.191438f,0.179870f,0.167863f,0.154745f,0.143928f,0.131309f,0.119462f,0.107750f,0.095037f,0.083584f,0.071568f,0.060930f,0.050652f,0.041238f,0.032940f,0.025295f,0.018748f,0.013625f,0.009024f,0.005481f,0.002629f,0.001110f,0.000212f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.220315f,0.206208f,0.191983f,0.177801f,0.165081f,0.151608f,0.137174f,0.123823f,0.109905f,0.096435f,0.082796f,0.070405f,0.058872f,0.048862f,0.039157f,0.030803f,0.023404f,0.017244f,0.012014f,0.007607f,0.004180f,0.002018f,0.000589f,0.000040f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.250044f,0.234399f,0.218197f,0.202859f,0.186948f,0.171991f,0.156755f,0.141233f,0.125128f,0.109455f,0.095019f,0.081213f,0.068559f,0.057191f,0.046275f,0.037544f,0.028657f,0.021839f,0.015275f,0.010220f,0.006142f,0.003252f,0.001181f,0.000235f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.282517f,0.265047f,0.247288f,0.229125f,0.210918f,0.194607f,0.176186f,0.159283f,0.140746f,0.124168f,0.108730f,0.093469f,0.079503f,0.066348f,0.054990f,0.044137f,0.034646f,0.026879f,0.019739f,0.013631f,0.008802f,0.005093f,0.002309f,0.000684f,0.000055f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.316096f,0.296509f,0.276965f,0.257235f,0.237247f,0.217435f,0.197798f,0.178274f,0.157977f,0.140272f,0.122623f,0.106548f,0.091081f,0.077364f,0.064500f,0.052625f,0.042060f,0.032664f,0.024765f,0.017767f,0.012027f,0.007357f,0.003738f,0.001468f,0.000289f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.352957f,0.330432f,0.308362f,0.286842f,0.264371f,0.242430f,0.220448f,0.198728f,0.176980f,0.156881f,0.137519f,0.120350f,0.103827f,0.089104f,0.074762f,0.062193f,0.050312f,0.040150f,0.031010f,0.022552f,0.015734f,0.010253f,0.005631f,0.002714f,0.000827f,0.000053f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.390896f,0.366220f,0.341464f,0.317138f,0.293225f,0.268007f,0.244421f,0.219554f,0.197725f,0.175444f,0.155022f,0.135602f,0.118124f,0.101754f,0.087033f,0.072264f,0.059826f,0.048134f,0.037625f,0.028210f,0.020641f,0.013898f,0.008426f,0.004582f,0.001718f,0.000349f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.430059f,0.403211f,0.377070f,0.349595f,0.323774f,0.295495f,0.269648f,0.243072f,0.217836f,0.194987f,0.173453f,0.153760f,0.134145f,0.116459f,0.099476f,0.084577f,0.070210f,0.057580f,0.046047f,0.035440f,0.026516f,0.018752f,0.012090f,0.007102f,0.003176f,0.001036f,0.000081f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.472808f,0.442312f,0.413118f,0.383930f,0.354687f,0.325123f,0.295952f,0.268809f,0.241718f,0.217552f,0.193563f,0.172837f,0.151405f,0.132554f,0.114052f,0.097717f,0.081868f,0.068286f,0.055353f,0.043815f,0.033144f,0.024172f,0.016698f,0.010231f,0.005579f,0.002314f,0.000431f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.517155f,0.483868f,0.452120f,0.419480f,0.389063f,0.354618f,0.323803f,0.294614f,0.267322f,0.240181f,0.215942f,0.192849f,0.171437f,0.149820f,0.130978f,0.113305f,0.096367f,0.081016f,0.066274f,0.053672f,0.041809f,0.031313f,0.022285f,0.014633f,0.008614f,0.004018f,0.001340f,0.000106f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.562278f,0.526963f,0.492509f,0.457002f,0.422014f,0.387508f,0.355000f,0.323736f,0.294126f,0.266362f,0.239859f,0.215132f,0.192117f,0.170445f,0.149666f,0.129760f,0.112304f,0.094458f,0.079736f,0.065366f,0.051979f,0.039590f,0.029356f,0.020144f,0.012909f,0.007101f,0.002827f,0.000619f,0.000000f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.609843f,0.571987f,0.533459f,0.495251f,0.457895f,0.421030f,0.387416f,0.354827f,0.323681f,0.294290f,0.267726f,0.240965f,0.216308f,0.193180f,0.170767f,0.150675f,0.130533f,0.111823f,0.094393f,0.078240f,0.063797f,0.050164f,0.038272f,0.027392f,0.018227f,0.011121f,0.005404f,0.001820f,0.000109f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.660531f,0.618853f,0.577766f,0.536600f,0.495578f,0.457478f,0.421175f,0.389352f,0.356586f,0.325464f,0.296385f,0.269738f,0.243473f,0.217713f,0.194439f,0.172211f,0.151007f,0.130385f,0.112597f,0.094651f,0.078325f,0.063043f,0.048998f,0.036657f,0.025956f,0.016665f,0.009342f,0.003946f,0.000779f,0.000000f,0.000000f,0.000000f,0.000000f,
 0.711871f,0.667518f,0.623595f,0.578724f,0.536229f,0.496981f,0.460704f,0.424688f,0.392214f,0.360393f,0.329053f,0.300369f,0.273712f,0.247602f,0.222195f,0.198128f,0.175256f,0.153573f,0.133101f,0.114022f,0.095396f,0.078710f,0.062868f,0.048455f,0.035781f,0.024478f,0.015046f,0.007755f,0.002501f,0.000210f,0.000000f,0.000000f,0.000000f,
 0.766099f,0.716762f,0.669590f,0.623409f,0.580612f,0.539848f,0.502549f,0.466244f,0.431436f,0.398217f,0.367864f,0.337021f,0.308862f,0.281238f,0.254447f,0.229192f,0.204291f,0.180849f,0.157749f,0.137015f,0.117083f,0.098413f,0.080291f,0.064144f,0.048639f,0.035235f,0.023401f,0.013518f,0.006149f,0.001386f,0.000000f,0.000000f,0.000000f,
 0.821831f,0.770804f,0.719293f,0.672058f,0.627572f,0.588148f,0.548142f,0.511127f,0.477253f,0.442731f,0.412023f,0.379406f,0.349163f,0.319915f,0.292321f,0.265206f,0.239046f,0.213811f,0.190343f,0.166132f,0.144293f,0.122630f,0.102953f,0.084345f,0.066805f,0.050063f,0.035221f,0.022644f,0.012153f,0.004398f,0.000442f,0.000000f,0.000000f,
 0.879179f,0.824267f,0.771475f,0.725090f,0.682030f,0.641276f,0.603935f,0.565544f,0.529920f,0.496160f,0.462449f,0.430029f,0.399041f,0.369218f,0.339675f,0.311000f,0.283150f,0.256918f,0.229535f,0.203691f,0.179414f,0.156050f,0.133158f,0.111440f,0.090404f,0.070989f,0.053811f,0.037474f,0.022974f,0.011038f,0.003010f,0.000000f,0.000000f,
 0.938403f,0.881014f,0.832187f,0.788517f,0.747049f,0.707570f,0.670312f,0.634757f,0.598363f,0.564208f,0.530539f,0.497276f,0.466862f,0.434372f,0.404048f,0.373046f,0.342721f,0.314665f,0.285917f,0.258302f,0.231523f,0.205100f,0.178891f,0.153803f,0.129753f,0.106018f,0.084060f,0.063029f,0.043492f,0.025499f,0.011008f,0.001458f,0.000000f,
 1.000000f,0.968518f,0.937163f,0.906535f,0.874761f,0.843931f,0.813314f,0.781227f,0.749733f,0.718285f,0.687276f,0.656042f,0.625004f,0.594260f,0.562244f,0.531578f,0.498883f,0.468271f,0.437781f,0.405738f,0.375448f,0.343814f,0.312189f,0.281199f,0.249667f,0.218313f,0.187118f,0.156768f,0.125090f,0.093185f,0.062393f,0.031447f,0.000000f
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

static unsigned staticToDynobjShotFunc(Scene *scene,Node *refl,Object *dynobj,ReflToDynobj* r2d,unsigned shotsAllowed)
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
	real u=(real)rand();
	real v=(real)rand();
	if(u+v>RAND_MAX)
	{
		u=RAND_MAX-u;
		v=RAND_MAX-v;
	}

	Point2 srcPoint2=source->uv[0]+source->u2*(u/RAND_MAX)+source->v2*(v/RAND_MAX);
	Point3 srcPoint3=source->grandpa->to3d(srcPoint2);

	// transform from shooter's objectspace to scenespace
	Point3 srcPoint3o=srcPoint3;
	srcPoint3.transformPosition(source->grandpa->object->transformMatrix);

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
		rayVec3=source->grandpa->getN3()*cos(a)
		       +source->grandpa->getU3()*(sina*cos(b))
		       +source->grandpa->getV3()*(sina*sin(b));
		shotPower=1;
	}
	else
	{
		// source outside sphere -> 0..1 of energy goes to sphere
		// sphereSizeCode=sin(uhel stredkoule-oko-tecnakoule)
		real sphereSizeCode=dynobj->bound.radius/toSphereCenterSize;
		// spherePositionCode=ctverec rozdilu normaly a smeru ke kouli
		real spherePositionCode=size2(source->grandpa->getN3()-toSphereCenter/toSphereCenterSize);
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
		assert(fabs(size2(rayVec3)-1)<0.001);
		// shotPower=cos(uhel mezi n3 a rayVec3)=1-sqr(rozdil tech vektoru)/2;
		shotPower=1-size2(rayVec3-source->grandpa->getN3())/2;
		// osetri pripad kdy miri dovnitr zarice=moc daleko od normaly
		if(shotPower<=0.001)
		{
			if(tries++<15) goto retry;
			break; // too hard to hit, continuing has no sense
		}
	}

	// transform from shooter's objectspace to scenespace
	rayVec3=(srcPoint3o+rayVec3).transformedPosition(source->grandpa->object->transformMatrix)-srcPoint3;

	assert(r2d->powerR2DSum>=0);
	assert(powerR2D>=0 && powerR2D<=1);
	r2d->powerR2DSum+=powerR2D;
	// find intersection with dynamic object
	// and when successful, insert hitTriangle to hitTriangles
	Triangle *hitTriangle;
	Hit      hitPoint2d;
	bool     hitFrontSide;
	real     hitDistance=size(srcPoint3-dynobj->bound.center)+dynobj->bound.radius;
	bool hit=scene->intersectionDynobj(srcPoint3,rayVec3,source->grandpa,dynobj,&hitTriangle,&hitPoint2d,&hitFrontSide,&hitDistance);
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
	bool doSplit=phits->doSplit(triCentre,perimeter,destination->grandpa,MESHING);
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

void Scene::improveDynamic(bool endfunc(void *),void *context)
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
	staticReflectors.gimmeLinks(staticToDynobjShotFunc,dynlightShotFunc,endfunc,context);
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

void Scene::objInsertDynamic(Object *o)
{
	if(objects==allocatedObjects)
	{
		size_t oldsize=allocatedObjects*sizeof(Object *);
		allocatedObjects*=4;
		object=(Object **)realloc(object,oldsize,allocatedObjects*sizeof(Object *));
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
	Object *tmp=object[o];
	object[o]=object[staticObjects];
	object[staticObjects++]=tmp;

	energyEmitedByDynamics-=tmp->energyEmited;
	energyEmitedByStatics+=tmp->energyEmited;

	staticReflectors.insertObject(tmp);
}

void Scene::objMakeDynamic(unsigned o)
{
	assert(o<staticObjects);
	Object *tmp=object[o];
	object[o]=object[--staticObjects];
	object[staticObjects]=tmp;

	energyEmitedByStatics-=tmp->energyEmited;
	energyEmitedByDynamics+=tmp->energyEmited;

	staticReflectors.removeObject(tmp);
}

/*
buga v interpolaci na vrsku 8stennyho bodaku
pocitat pri gouraudu i totalExitingFlux z clusteru
pocitat pri flatu i totalExitingFlux z clusteru
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

} // namespace

#endif
