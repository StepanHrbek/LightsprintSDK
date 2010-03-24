// --------------------------------------------------------------------------
// Radiosity solver - low level.
// Copyright (c) 2000-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdarg>
#include <cstdio>    // printf
#include <cstdlib>
#include <cstring>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "../LicGen.h"
#include "rrcore.h"

namespace rr
{

#define ALLOW_DEGENS                 // accept triangles degenerated by low numerical quality float operations
#define HOMOGENOUS_FILL              // shooting direction is not random but fractal, 0% space, -2% calc
#define SHOOT_FULL_RANGE     1       // 89/90=ignore 1 degree near surface, shoot only away from surface (to avoid occasional shots inside object)
#define REFRESH_FIRST          1     // first refresh has this number of photons (zahada: vyssi cislo rrbench zpomaluje misto zrychluje)
#define REFRESH_MULTIPLY       4     // next refresh has multiplied number of photons
#define MAX_REFRESH_DISBALANCE 5     // higher = faster, but more dangerous
#define DISTRIB_LEVEL_HIGH     0.0003 // higher fraction of scene energy found in one node starts distribution
#define DISTRIB_LEVEL_LOW      0.000003// lower fraction of scene energy found in one node is ignored

#define TWOSIDED_RECEIVE_FROM_BOTH_SIDES
#define TWOSIDED_EMIT_TO_BOTH_SIDES
#define ONESIDED_TRANSMIT_ENERGY


//////////////////////////////////////////////////////////////////////////////
//
// memory

#define SIMULATE_REALLOC

void* realloc(void* p,size_t oldsize,size_t newsize)
{
#ifdef SIMULATE_REALLOC
	//if (newsize>500000) return realloc(p,newsize);
	// this simulated realloc prevents real but buggy realloc from crashing rr (seen in DJGPP)
	// it is also faster (seen in MinGW)
	void *q=malloc(newsize);
	if (!q)
	{
		RRReporter::report(ERRO,"Out of memory, exiting!\n");
		exit(0);
	}
	if (p)
	{
		memcpy(q,p,RR_MIN(oldsize,newsize));
		free(p);
	}
	return q;
#else
	return ::realloc(p,newsize);
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// globals

unsigned  __frameNumber=1; // frame number increased after each draw

//////////////////////////////////////////////////////////////////////////////
//
// triangle

Triangle::Triangle()
{
	topivertex[0]=NULL;
	topivertex[1]=NULL;
	topivertex[2]=NULL;
	area=0;       // says that setGeometry wasn't called yet
	surface=NULL; // says that setSurface wasn't called yet
	reset(true);
	hits=0;
	isLod0=0;
}

void Triangle::reset(bool resetFactors)
{
	totalExitingFlux=Channels(0);
	totalIncidentFlux=Channels(0);
	isReflector=0;
	if (resetFactors)
	{
		factors.clear();
		shotsForFactors=0;
	}
	totalExitingFluxToDiffuse=Channels(0);
}

static real minAngle(real a,real b,real c) // delky stran
{
	real angleA = fast_acos((b*b+c*c-a*a)/(2*b*c));
	real angleB = fast_acos((a*a+c*c-b*b)/(2*a*c));
	real angleC = fast_acos((a*a+b*b-c*c)/(2*a*b));
	return RR_MIN3(angleA,angleB,angleC);
}

// calculates triangle area from triangle vertices
static real calculateArea(const RRMesh::TriangleBody& body)
{
	real a=size2(body.side1);
	real b=size2(body.side2);
	real c=size2(body.side2-body.side1);
	return sqrt(2*b*c+2*c*a+2*a*b-a*a-b*b-c*c)/4;
}

S8 Triangle::setGeometry(const RRMesh::TriangleBody& body,float ignoreSmallerAngle,float ignoreSmallerArea)
{
	RRVec3 qn3=normalized(orthogonalTo(body.side1,body.side2));
	//qn3.w=-dot(body.vertex0,qn3);
	if (!IS_VEC3(qn3)) return -3; // throw out degenerated triangle

	real rsize=size(body.side1);
	real lsize=size(body.side2);
	if (rsize<=0 || lsize<=0) return -1; // throw out degenerated triangle
	real psqr=size2(body.side1/rsize-(body.side2/lsize));// ctverec nad preponou pri jednotkovejch stranach
	#ifdef ALLOW_DEGENS
	if (psqr<=0) {psqr=0.0001f;RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Low numerical quality, fixing area=0 triangle.\n"));} else
	if (psqr>=4) {psqr=3.9999f;RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Low numerical quality, fixing area=0 triangle.\n"));}
	#endif
	real sina=sqrt(psqr*(1-psqr/4));//sin(fast_acos(1-psqr/2)); //first is probably faster
	area=sina/2*rsize*lsize;
	if (psqr<=0) return -7;
	if (psqr>=4) return -9;
	if (1-psqr/4<=0) return -8;
	if (sina<=0) return -6;
	if (area<=0) return -4;
	if (area<=ignoreSmallerArea) return -5;

	// premerit min angle v localspace (mohlo by byt i ve world)
	real minangle = minAngle(lsize,rsize,size(body.side2-body.side1));
	if (!IS_NUMBER(area)) return -13;
	if (minangle<=ignoreSmallerAngle) return -14;

	area = calculateArea(body);
	if (!IS_NUMBER(area)) return -11;
	if (area<=ignoreSmallerArea) return -12;

	return 0;
}

// resetPropagation = true
//  nastavi skoro vse na vychozi hodnoty zacatku vypoctu -> pouze primaries maji energie, jinde jsou nuly
//  u nekterych promennych predpoklada ze uz jsou vynulovane
// resetPropagation = false
//  spoleha na to ze promenne uz jsou naplnene probihajicim vypoctem
//  pouze zaktualizuje primary illum energie podle surfacu a additionalExitingFlux
// return new primary exiting radiant flux in watts

Channels Triangle::setSurface(const RRMaterial *s, const RRVec3& _sourceIrradiance, bool resetPropagation)
{
	RR_ASSERT(area!=0);//setGeometry must be called before setSurface
	//RR_ASSERT(s); problem was already reported one level up, let's try to survive

	// aby to necrashlo kdyz uzivatel neopravnene zada NULL
	static RRMaterial emergencyMaterial;
	static bool emergencyInited = false;
	if (!emergencyInited)
	{
		emergencyInited = true;
		emergencyMaterial.reset(false);
	}
	if (!s) s = &emergencyMaterial;

	surface=s;
#if CHANNELS == 1
	#error CHANNELS == 1 not supported here.
#else
	Channels newSourceIrradiance = _sourceIrradiance;
	Channels newSourceExitance = surface->diffuseEmittance.color + _sourceIrradiance * surface->diffuseReflectance.color;
	Channels newSourceIncidentFlux = newSourceIrradiance * area;
	Channels newSourceExitingFlux = newSourceExitance * area;
	RR_ASSERT(IS_VEC3(newSourceIncidentFlux));
	RR_ASSERT(IS_VEC3(newSourceExitingFlux));
#endif
	RR_ASSERT(surface->diffuseEmittance.color[0]>=0); // teoreticky by melo jit i se zapornou
	RR_ASSERT(surface->diffuseEmittance.color[1]>=0);
	RR_ASSERT(surface->diffuseEmittance.color[2]>=0);
	RR_ASSERT(area>=0);
	RR_ASSERT(_sourceIrradiance.x>=0); // teoreticky by melo jit i se zapornou
	RR_ASSERT(_sourceIrradiance.y>=0);
	RR_ASSERT(_sourceIrradiance.z>=0);
	// set this primary illum
	if (resetPropagation)
	{
		// set primary illum
		totalExitingFluxToDiffuse = newSourceExitingFlux;
		// load received energy accumulator
		totalExitingFlux = newSourceExitingFlux;
		totalIncidentFlux = newSourceIncidentFlux;
	}
	else
	{
		Channels oldSourceExitingFlux = getDirectExitingFlux();
		Channels addSourceExitingFlux = newSourceExitingFlux-oldSourceExitingFlux;
		Channels oldSourceIncidentFlux = getDirectIncidentFlux();
		Channels addSourceIncidentFlux = newSourceIncidentFlux-oldSourceIncidentFlux;
		// add primary illum
		totalExitingFluxToDiffuse += addSourceExitingFlux;
		// load received energy accumulator
		totalExitingFlux += addSourceExitingFlux;
		totalIncidentFlux += addSourceIncidentFlux;
	}
	directIncidentFlux = newSourceIncidentFlux;
	return newSourceExitingFlux;
}

//////////////////////////////////////////////////////////////////////////////
//
// reflectors (light sources and things that reflect light)

Reflectors::Reflectors()
{
	nodesAllocated=0;
	node=NULL;
	nodes=0;
	reset();
}

void Reflectors::reset()
{
	for (int i=nodes;i--;) node[i]->isReflector=0;
	nodes=0;
	bests=0;
}

void Reflectors::resetBest()
{
	bests=0;
}

bool Reflectors::insert(Triangle* anode)
{
	if (anode->isReflector || !anode->isLod0) return false;
	if (anode->totalExitingFlux==Channels(0) && anode->totalExitingFluxToDiffuse==Channels(0)) return false;
	if (!nodesAllocated)
	{
		nodesAllocated=1024;
		node=(Triangle**)malloc(nodesAllocated*sizeof(Triangle*));
	}
	else
	if (nodes==nodesAllocated)
	{
		size_t oldsize=nodesAllocated*sizeof(Triangle*);
		nodesAllocated*=4;
		node=(Triangle**)realloc(node,oldsize,nodesAllocated*sizeof(Triangle*));
	}
	anode->isReflector=1;
	node[nodes++]=anode;
	return true;
}

void Reflectors::insertObject(Object *o)
{
	for (unsigned i=0;i<o->triangles;i++) insert(&o->triangle[i]);
}


BestInfo Reflectors::best(real allEnergyInScene)
{
	STATISTIC_INC(numCallsBest);
	// if cache empty, fill cache
	if (!bests && nodes)
	{
		// start accumulating nodes for refresh
		bool refreshing=1;
restart:
		//RRReal unshot = 0; // accumulators for convergence update
		//RRReal shot = 0;
		// search reflector with low accuracy, high totalExitingFluxToDiffuse etc
		real bestQ[BESTS];
		for (unsigned i=0;i<nodes;i++)
		{
			// calculate q for node
			real q;
			real toDiffuse=sum(abs(node[i]->totalExitingFluxToDiffuse));
			// distributor found -> switch from accumulating refreshers to accumulating distributors
			if (refreshing && node[i]->factors.size() && toDiffuse>DISTRIB_LEVEL_HIGH*allEnergyInScene)
			{
				refreshing=0;
				bests=0;
				goto restart;
			}
			// calculate quality of distributor
			if (!refreshing)
			{
				if (!node[i]->factors.size()) continue;
				if (toDiffuse<DISTRIB_LEVEL_LOW*abs(allEnergyInScene)) continue;
				q=toDiffuse;
			}
			else
			// calculate quality of refresher
			{
				q = sum(abs(node[i]->totalExitingFlux)) / (node[i]->shotsForFactors+0.5f);
			}

			// sort [q,node] into best cache, bestQ[0] is highest
			unsigned pos=bests;
			while (pos>0 && bestQ[pos-1]<q)
			{
				if (pos<BESTS)
				{
					bestNode[pos]=bestNode[pos-1];
					bestQ[pos]=bestQ[pos-1];
				}
				pos--;
			}
			if (pos<BESTS)
			{
				bestNode[pos].node=node[i];
				bestNode[pos].shotsForNewFactors = refreshing ? (node[i]->shotsForFactors?REFRESH_MULTIPLY*node[i]->shotsForFactors:REFRESH_FIRST) : 0;
				bestQ[pos]=q;
				if (bests<BESTS) bests++;
			}
		}

		// throw out nodes too good for refreshing
		// 1.6% faster when deleted, but danger of infinitely unbalanced refreshing
		if (refreshing)
		{
			while (bests && bestQ[bests-1]*REFRESH_MULTIPLY*MAX_REFRESH_DISBALANCE<bestQ[0]) bests--;
		}

		// update convergence
		//convergence = shot/(shot+unshot);

		//printf(refreshing?"*%d ":">%d ",bests);
	}
	// get best from cache
	if (!bests)
	{
		BestInfo result = {NULL,0};
		return result;
	}
	BestInfo result = bestNode[0];
	bests--;
	for (unsigned i=0;i<bests;i++) bestNode[i]=bestNode[i+1];
	RR_ASSERT(result.node);
	return result;
}

struct NodeQ 
{
	Triangle* node; 
	real q;
};

int CompareNodeQ(const void* elem1, const void* elem2)
{
	return (((NodeQ*)elem2)->q < ((NodeQ*)elem1)->q) ? -1 : 1;
}

Reflectors::~Reflectors()
{
	if (node) free(node);
}

//////////////////////////////////////////////////////////////////////////////
//
// set of triangles

Triangles::Triangles()
{
	trianglesAllocated=8;
	triangle=(Triangle **)malloc(trianglesAllocated*sizeof(Triangle *));
	reset();
}

void Triangles::reset()
{
	triangles=0;
}

void Triangles::insert(Triangle *key)
{
	if (triangles==trianglesAllocated)
	{
		size_t oldsize=trianglesAllocated*sizeof(Triangle *);
		trianglesAllocated*=2;
		triangle=(Triangle **)realloc(triangle,oldsize,trianglesAllocated*sizeof(Triangle *));
	}
	triangle[triangles++]=key;
}

//removes triangle from set
Triangle *Triangles::get()
{
	if (!triangles) return NULL;
//        if (!trianglesAfterResurrection) trianglesAfterResurrection=triangles;
	triangles--;
	return triangle[triangles];
}

Triangles::~Triangles()
{
	free(triangle);
}

//////////////////////////////////////////////////////////////////////////////
//
// object, part of scene

Object::Object()
{
	vertices=0;
	triangles=0;
	triangle=NULL;
	objSourceExitingFlux=Channels(0);
	topivertexArray = NULL;
}

unsigned Object::getTriangleIndex(Triangle* t)
{
	unsigned idx = (unsigned)(t-triangle);
	return (idx<triangles)?idx:UINT_MAX;
}

Object::~Object()
{
	delete[] triangle;
	delete[] topivertexArray;
}

void Object::resetStaticIllumination(bool resetFactors, bool resetPropagation, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB)
{
	// zero accumulators. separated to three floats to satisfy openmp reduction rules
	//objSourceExitingFlux=Channels(0);
	RRReal tmpx = 0;
	RRReal tmpy = 0;
	RRReal tmpz = 0;
#pragma omp parallel for schedule(static,1) reduction(+:tmpx,tmpy,tmpz) // fastest: indifferent
	for (int t=0;(unsigned)t<triangles;t++) if (triangle[t].surface) 
	{
		// smaze akumulatory (ale necha jim flag zda jsou v reflectors)
		if (resetPropagation)
		{
			unsigned flag=triangle[t].isReflector;
			triangle[t].reset(resetFactors);
			triangle[t].isReflector=flag;
		}

		// nastavi akumulatory na pocatecni hodnoty
		RRVec3 directIrradiancePhysical(0);
		if (directIrradianceCustomRGBA8)
		{
			unsigned color = directIrradianceCustomRGBA8[t];
			directIrradiancePhysical = RRVec3(customToPhysical[(color>>24)&255],customToPhysical[(color>>16)&255],customToPhysical[(color>>8)&255]);
		}
		else
		if (directIrradiancePhysicalRGB)
		{
			directIrradiancePhysical = directIrradiancePhysicalRGB[t];
		}
		Channels tmp = abs(triangle[t].setSurface(triangle[t].surface,directIrradiancePhysical,resetPropagation));
		//objSourceExitingFlux += tmp;
		tmpx += tmp.x;
		tmpy += tmp.y;
		tmpz += tmp.z;
	}
	objSourceExitingFlux = Channels(tmpx,tmpy,tmpz);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerLod0
//
//! Collides with lod0 only.
//! Direct access to array of scene triangles.

class RRCollisionHandlerLod0 : public RRCollisionHandler
{
public:
	RRCollisionHandlerLod0(const Triangle* t)
	{
		triangle = t;
		shooterTriangleIndex = UINT_MAX; // set manually before intersect
	}

	void setShooterTriangle(unsigned t)
	{
		shooterTriangleIndex = t;
	}

	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_TRIANGLE;
		result = false;
	}

	virtual bool collides(const RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);

		// don't collide with shooter
		if (ray->hitTriangle==shooterTriangleIndex)
			return false;

		// don't collide with lod!0
		if (!triangle[ray->hitTriangle].isLod0)
			return false;

		// don't collide when object has NULL material (illegal input)
		//triangleMaterial = triangle[ray->hitTriangle].surface;
		//if (!triangleMaterial)
		//	return false;

		//if (!triangleMaterial->sideBits[ray->hitFrontSide?0:1].catchFrom)
		//	return false

		return result = true;
	}
	virtual bool done()
	{
		return result;
	}

private:
	unsigned shooterTriangleIndex;
	bool result;
	const Triangle* triangle; // access to triangles in scene
};


//////////////////////////////////////////////////////////////////////////////
//
// ShootingKernel

ShootingKernel::ShootingKernel()
{
	sceneRay = RRRay::create();
	sceneRay->rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE|RRRay::FILL_PLANE;
	sceneRay->rayLengthMin = SHOT_OFFSET; // offset 0.1mm resi situaci kdy jsou 2 facy ve stejne poloze, jen obracene zady k sobe. bez offsetu se vzajemne zasahuji.
	sceneRay->rayLengthMax = BIG_REAL;
	sceneRay->collisionHandler = collisionHandlerLod0 = NULL;
}

ShootingKernel::~ShootingKernel()
{
	delete collisionHandlerLod0;
	delete sceneRay;
}

ShootingKernels::ShootingKernels()
{
#ifdef _OPENMP
	numKernels = omp_get_max_threads();
#else
	numKernels = 1;
#endif
	shootingKernel = new ShootingKernel[numKernels];
}

void ShootingKernels::setGeometry(Triangle* sceneGeometry)
{
	for (unsigned i=0;i<numKernels;i++)
	{
		shootingKernel[i].sceneRay->collisionHandler = shootingKernel[i].collisionHandlerLod0 = new RRCollisionHandlerLod0(sceneGeometry);
	}
}

void ShootingKernels::reset(unsigned maxQueries)
{
	for (unsigned i=0;i<numKernels;i++)
	{
		shootingKernel[i].filler.Reset(i,numKernels,maxQueries);
		shootingKernel[i].russianRoulette.reset();
		shootingKernel[i].hitTriangles.reset();
	}
}

ShootingKernels::~ShootingKernels()
{
	delete[] shootingKernel;
}


//////////////////////////////////////////////////////////////////////////////
//
// scene

Scene::Scene()
{
	object=NULL;
	phase=0;
	improvingStatic.node=NULL;
	shotsForNewFactors=0;
	shotsAccumulated=0;
	shotsForFactorsTotal=0;
	shotsTotal=0;
	staticSourceExitingFlux=Channels(0);
	skyPatchHitsForAllTriangles = NULL;
	skyPatchHitsForCurrentTriangle = NULL;
}

Scene::~Scene()
{
	abortStaticImprovement();
	delete object;
}

void Scene::objInsertStatic(Object *o)
{
	RR_ASSERT(!object);
	object = o;
	staticReflectors.insertObject(o);
	staticSourceExitingFlux+=o->objSourceExitingFlux;
	shootingKernels.setGeometry(object->triangle);
}

RRStaticSolver::Improvement Scene::resetStaticIllumination(bool resetFactors, bool resetPropagation, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB)
{
	if (resetFactors)
		resetPropagation = true;

	abortStaticImprovement();

	if (resetFactors)
	{
		shotsForFactorsTotal=0;
		shotsTotal=0;
		// tell pool it can deallocate or mark everything as free, we promise we won't use previously allocated factors
		factorAllocator.reset();
	}
	staticSourceExitingFlux=Channels(0);

	if (resetPropagation)
	{
		staticReflectors.reset();
	}
	else
	{
		staticReflectors.resetBest();
	}

	object->resetStaticIllumination(resetFactors,resetPropagation,directIrradianceCustomRGBA8,customToPhysical,directIrradiancePhysicalRGB);

	staticReflectors.insertObject(object);

	staticSourceExitingFlux+=object->objSourceExitingFlux;

	return (staticSourceExitingFlux!=Channels(0)) ? RRStaticSolver::NOT_IMPROVED : RRStaticSolver::FINISHED;
}


//////////////////////////////////////////////////////////////////////////////
//
// trace ray, reflect from triangles and mark hitpoints
// return amount of power added to scene

RRVec3 refract(RRVec3 N,RRVec3 I,real r)
{
	real ndoti=dot(N,I);
	if (ndoti<0) r=1/r;
	real D2=1-r*r*(1-ndoti*ndoti);
	if (D2>=0)
	{
		real a;
		if (ndoti>=0) a=r*ndoti-sqrt(D2);
		else a=r*ndoti+sqrt(D2);
		return N*a-I*r;
	} else {
		// total internal reflection
		return N*(2*ndoti)-I;
	}
}

unsigned __shot=0;

#define LOG_RAY(aeye,adir,adist,hit) { \
	STATISTIC( \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[0]=aeye; \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[1]=(aeye)+(adir)*(adist); \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].infinite=!hit; \
	++RRStaticSolver::getSceneStatistics()->numLineSegments%=RRStaticSolver::getSceneStatistics()->MAX_LINES; ) }

HitChannels Scene::rayTracePhoton(ShootingKernel* shootingKernel, const RRVec3& eye, const RRVec3& direction, const Triangle *skip, HitChannels power)
// returns power which will be diffuse reflected (result<=power)
// side effects: inserts hits to diffuse surfaces
{
	RR_ASSERT(IS_VEC3(eye));
	RR_ASSERT(IS_VEC3(direction));
	RR_ASSERT(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	RRRay& ray = *shootingKernel->sceneRay;
	ray.rayOrigin = eye;
	ray.rayDirInv[0] = 1/direction[0];
	ray.rayDirInv[1] = 1/direction[1];
	ray.rayDirInv[2] = 1/direction[2];
	shootingKernel->collisionHandlerLod0->setShooterTriangle((unsigned)(skip-object->triangle));
	Triangle* hitTriangle = (object->triangles // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
		&& object->importer->getCollider()->intersect(&ray)) ? &object->triangle[ray.hitTriangle] : NULL;
	__shot++;
	//LOG_RAY(eye,direction,hitTriangle?ray.hitDistance:0.2f,hitTriangle);
	if (!hitTriangle || !hitTriangle->surface) // !hitTriangle is common, !hitTriangle->surface is error (bsp se generuje z meshe a surfacu(null=zahodit face), bsp hash se generuje jen z meshe. -> po zmene materialu nacte stary bsp a zasahne triangl ktery mel surface ok ale nyni ma NULL)
	{
		if (!hitTriangle && skyPatchHitsForCurrentTriangle)
		{
			// convert direction to patch index
			unsigned skyPatchIndex = PackedSkyTriangleFactor::getPatchIndex(direction);
			// store patch hit
			#pragma omp critical
			skyPatchHitsForCurrentTriangle->patches[skyPatchIndex][0] += power;
		}
		// ray left scene and vanished
		return HitChannels(0);
	}
	RR_ASSERT(IS_NUMBER(ray.hitDistance));
	//static unsigned s_depth = 0;
	//if (s_depth>25) 
	//{
	//	STATISTIC_INC(numDepthOverflows);
	//	return HitChannels(0);
	//}
	//s_depth++;
	if (ray.hitFrontSide) STATISTIC_INC(numRayTracePhotonFrontHits); else STATISTIC_INC(numRayTracePhotonBackHits);
	// otherwise surface with these properties was hit
	RRSideBits side=hitTriangle->surface->sideBits[ray.hitFrontSide?0:1];
	RR_ASSERT(side.catchFrom); // check that bad side was not hit
	// calculate power of diffuse surface hits
	HitChannels  hitPower=HitChannels(0);
	// stats
	//if (!side.receiveFrom) RRStaticSolver::getSceneStatistics()->numRayTracePhotonHitsNotReceived++;
	//RRStaticSolver::getSceneStatistics()->sumRayTracePhotonHitPower+=power;
	//RRStaticSolver::getSceneStatistics()->sumRayTracePhotonDifRefl+=hitTriangle->surface->diffuseReflectance;
	// diffuse reflection
	// no real reflection is done here, but energy is stored for further
	//  redistribution along existing or newly calculated form factors
	// hits with power below 1% are ignored to save a bit of time
	//  without visible loss of quality
	if (side.receiveFrom)
	{
		RRReal diffuseReflectPower = sum(abs(hitTriangle->surface->diffuseReflectance.color*power));
		if (diffuseReflectPower>0.01)
		{
			STATISTIC_INC(numRayTracePhotonHitsReceived);
			hitPower += diffuseReflectPower;
			#pragma omp critical
			{
				// cheap storage with accumulated power -> subdivision is not possible
				// put triangle among other hit triangles
				if (!hitTriangle->hits) shootingKernel->hitTriangles.insert(hitTriangle);
				// inform subtriangle where and how powerfully it was hit
				hitTriangle->hits += power;
			}
		}
	}

	bool specularReflect = false;
	bool specularTransmit = false;
	RRReal specularReflectPower;
	RRReal specularTransmitPower;
	if (side.reflect)
	{
		specularReflectPower = power*hitTriangle->surface->specularReflectance.color.avg();
		specularReflect = shootingKernel->russianRoulette.survived(specularReflectPower);
	}
	if (side.transmitFrom)
	{
		specularTransmitPower = power*hitTriangle->surface->specularTransmittance.color.avg();
		specularTransmit = shootingKernel->russianRoulette.survived(specularTransmitPower);
	}

	if (specularReflect || specularTransmit)
	{
		// calculate hitpoint
		RRVec3 hitPoint3d=eye+direction*ray.hitDistance;

		// calculate parameters of transmission in advance, recursive reflection destroys ray content
		// (hitTriangle->surface is safe, reflection won't change it)
		RRVec3 newDirectionTransmit;
		if (specularTransmit)
		{
			// calculate new direction after refraction
			newDirectionTransmit = -refract(ray.hitPlane,direction,hitTriangle->surface->refractionIndex);
		}

		// mirror reflection
		if (specularReflect)
		{
			STATISTIC_INC(numRayTracePhotonHitsReflected);
			// calculate new direction after ideal mirror reflection
			RRVec3 newDirectionReflect = RRVec3(ray.hitPlane)*(-2*dot(direction,RRVec3(ray.hitPlane))/size2(RRVec3(ray.hitPlane)))+direction;
			// recursively call this function
			hitPower += rayTracePhoton(shootingKernel,hitPoint3d,newDirectionReflect,hitTriangle);
		}

		// transmission
		if (specularTransmit)
		{
			STATISTIC_INC(numRayTracePhotonHitsTransmitted);
			// recursively call this function
			hitPower += rayTracePhoton(shootingKernel,hitPoint3d,newDirectionTransmit,hitTriangle);
		}
	}

	//s_depth--;
	return hitPower;
}

//////////////////////////////////////////////////////////////////////////////
//
// homogenous filling:
//   generates points that nearly homogenously (low density fluctuations) fill some 2d area

void HomogenousFiller::Reset(unsigned kernelNum, unsigned numKernels, unsigned maxQueries)
{
	num = maxQueries*kernelNum/numKernels
		*3/2; // compensate that some rays don't fit in circle and we increment num multiple times
}

real HomogenousFiller::GetCirclePoint(real *a,real *b)
{
	real dist;
	do GetTrianglePoint(a,b); while ((dist=*a**a+*b**b)>=SHOOT_FULL_RANGE);
	return dist;
}

void HomogenousFiller::GetTrianglePoint(real *a,real *b)
{
	unsigned n=num++;
	static const real dir[4][3]={{0,0,-0.5f},{0,1,0.5f},{0.86602540378444f,-0.5f,0.5f},{-0.86602540378444f,-0.5f,0.5f}};
	real x=0;
	real y=0;
	real dist=1;
	while (n)
	{
		x+=dist*dir[n&3][0];
		y+=dist*dir[n&3][1];
		dist*=dir[n&3][2];
		n>>=2;
	}
	*a=x;
	*b=y;
	//*a=rand()/(RAND_MAX*0.5)-1;
	//*b=rand()/(RAND_MAX*0.5)-1;
}

//////////////////////////////////////////////////////////////////////////////
//
// Russian roulette


bool RussianRoulette::survived(float survivalProbability)
{
	// this is not necessary, but it is expected
	RR_ASSERT(survivalProbability>=0);
	RR_ASSERT(survivalProbability<=1);

	accumulator += survivalProbability;
	if (accumulator>1)
	{
		accumulator -= 1;
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// random exiting ray

bool ShootingKernel::getRandomExitDir(const RRMesh::TangentBasis& basis, const RRSideBits* sideBits, RRVec3& exitDir)
// orthonormal basis
// returns random direction exitting diffuse surface with 1 or 2 sides and normal norm
{
#ifdef HOMOGENOUS_FILL
	real x,y;
	real cosa=sqrt(1-filler.GetCirclePoint(&x,&y));
#else
	// select random vector from srcPoint3 to one halfspace
	// power is assumed to be 1
	real tmp=(real)rand()/RAND_MAX*SHOOT_FULL_RANGE;
	real cosa=sqrt(1-tmp);
#endif
	// emit only inside?
	if (!sideBits[0].emitTo && sideBits[1].emitTo)
		cosa=-cosa;
	// emit to both sides?
	if (sideBits[0].emitTo && sideBits[1].emitTo)
		if ((rand()%2)) cosa=-cosa;
	// don't emit?
	if (!sideBits[0].emitTo && !sideBits[1].emitTo)
		return false;
#ifdef HOMOGENOUS_FILL
	exitDir = basis.normal*cosa + basis.tangent*x + basis.bitangent*y;
#else
	real sina=sqrt( tmp );                  // a = rotation angle from normal to side, sin(a) = distance from center of circle
	Angle b=rand()*2*RR_PI/RAND_MAX;         // b = rotation angle around normal
	exitDir = basis.normal*cosa + basis.tangent*(sina*cos(b)) + basis.bitangent*(sina*sin(b));
#endif
	RR_ASSERT(fabs(size2(exitDir)-1)<0.001);//ocekava normalizovanej dir
	return true;
}

Triangle* Scene::getRandomExitRay(ShootingKernel* shootingKernel, Triangle* source, RRVec3* src, RRVec3* dir)
// returns random point and direction exiting sourceNode
{
	// select random point in source subtriangle
	unsigned u=rand();
	unsigned v=rand();
	if (u+v>RAND_MAX)
	{
		u=RAND_MAX-u;
		v=RAND_MAX-v;
	}
	RRVec3 srcPoint3 = improvingBody.vertex0 + improvingBody.side1*(u/(real)RAND_MAX) + improvingBody.side2*(v/(real)RAND_MAX);

	RR_ASSERT(source->surface);
	RRVec3 rayVec3;
	if (!shootingKernel->getRandomExitDir(improvingBasisOrthonormal,source->surface->sideBits,rayVec3))
		return NULL;
	RR_ASSERT(IS_SIZE1(rayVec3));

	*src = srcPoint3;
	*dir = rayVec3;

	return source;
}

//////////////////////////////////////////////////////////////////////////////
//
// one shot from subtriangle to whole halfspace

void Scene::shotFromToHalfspace(ShootingKernel* shootingKernel,Triangle* sourceNode)
{
	RRVec3 srcPoint3,rayVec3;
	Triangle* tri=getRandomExitRay(shootingKernel,sourceNode,&srcPoint3,&rayVec3);
	// cast ray
	if (tri) rayTracePhoton(shootingKernel,srcPoint3,rayVec3,tri);
}

//////////////////////////////////////////////////////////////////////////////
//
// distribute energy via one factor

static void distributeEnergyViaFactor(const Factor& factor, Channels energy, Reflectors* staticReflectors)
{
	RR_ASSERT(IS_VEC3(energy));
	RR_ASSERT(factor.power>=0);

	// statistics
	STATISTIC_INC(numCallsDistribFactor);

	Triangle* destination = factor.destination;
	RR_ASSERT(destination);
	RR_ASSERT(destination->surface);
	RR_ASSERT(IS_VEC3(destination->totalIncidentFlux));
	RR_ASSERT(IS_VEC3(destination->totalExitingFlux));
	RR_ASSERT(IS_VEC3(destination->totalExitingFluxToDiffuse));
	RR_ASSERT(IS_VEC3(destination->surface->diffuseReflectance.color));

	energy *= factor.power;
	RR_ASSERT(IS_VEC3(energy));

	destination->totalIncidentFlux += energy;
	RR_ASSERT(IS_VEC3(destination->totalIncidentFlux));

	energy *= destination->surface->diffuseReflectance.color;
	RR_ASSERT(IS_VEC3(energy));

	destination->totalExitingFlux += energy;
	RR_ASSERT(IS_VEC3(destination->totalExitingFlux));

	destination->totalExitingFluxToDiffuse += energy;
	RR_ASSERT(IS_VEC3(destination->totalExitingFluxToDiffuse));

	staticReflectors->insert(destination);
}


//////////////////////////////////////////////////////////////////////////////
//
// refresh form factors from one source to all destinations that need it

void Scene::refreshFormFactorsFromUntil(BestInfo source,RRStaticSolver::EndFunc& endfunc)
{
	if (phase==0)
	{
		// prepare shooting
		shotsForNewFactors = source.shotsForNewFactors;
		RR_ASSERT(shotsAccumulated==0);
		for (unsigned kernelNum=0;kernelNum<shootingKernels.numKernels;kernelNum++)
		{
			RR_ASSERT(!shootingKernels.shootingKernel[kernelNum].hitTriangles.get());
		}
		shootingKernels.reset(shotsForNewFactors); // prepare homogenous shooting
		// prepare data for shooting
		unsigned triangleIndex = ARRAY_ELEMENT_TO_INDEX(object->triangle,source.node);
		object->importer->getCollider()->getMesh()->getTriangleBody(triangleIndex,improvingBody);
		improvingBasisOrthonormal.normal = orthogonalTo(improvingBody.side1,improvingBody.side2).normalized();
		improvingBasisOrthonormal.buildBasisFromNormal();
		phase=1;
	}
	if (phase==1)
	{
		if (endfunc.requestsRealtimeResponse())
		{
			// shoot in 1 thread, slower, can be aborted
			// this is used by real-time Architect solver
			while (shotsAccumulated<shotsForNewFactors
				)
			{
				shotFromToHalfspace(shootingKernels.shootingKernel,source.node);
				shotsAccumulated++;
				shotsTotal++;
				if (shotsTotal%10==0 && endfunc.requestsEnd()) return;
			}
		}
		else
		{
			// shoot in multiple threads, faster, can't be aborted
			// this is used by offline solver
			int shotsTodo = shotsForNewFactors-shotsAccumulated;
			#if defined(_MSC_VER) && (_MSC_VER<1500)
				#pragma omp parallel for // 2005 SP1 has broken if
			#else
				#pragma omp parallel for if(shotsTodo>30)
			#endif
			for (int i=0;i<shotsTodo;i++)
			{
				{
					#ifdef _OPENMP
						int threadNum = omp_get_thread_num();
					#else
						int threadNum = 0;
					#endif
					shotFromToHalfspace(shootingKernels.shootingKernel+threadNum,source.node);
				}
			}
			shotsAccumulated += shotsTodo;
			shotsTotal += shotsTodo;
		}
	
		phase=2;
	}
	if (phase==2)
	{
		// preallocate space for new factors. if it fails, keep old factors.
		// can be deleted, its purpose is only to ensure that illumination doesn't get worse after allocation failure
		{
			unsigned numFactorsToInsert = 0;
			for (unsigned kernelNum=0;kernelNum<shootingKernels.numKernels;kernelNum++)
			{
				numFactorsToInsert += shootingKernels.shootingKernel[kernelNum].hitTriangles.size();
			}
			if (!factorAllocator.reserve(numFactorsToInsert))
			{
				// alloc failed, keep old factors
				return;
			}
		}

		// remove old factors
		shotsForFactorsTotal-=source.node->shotsForFactors;
		Channels ch(source.node->totalExitingFluxToDiffuse-source.node->totalExitingFlux);
		for (ChunkList<Factor>::const_iterator i=source.node->factors.begin(); *i; ++i)
			distributeEnergyViaFactor(**i, ch, &staticReflectors);
		source.node->factors.clear();

		// insert new factors
		ChunkList<Factor>::InsertIterator i(source.node->factors,factorAllocator);
		Factor f;
		for (unsigned kernelNum=0;kernelNum<shootingKernels.numKernels;kernelNum++)
		{
			while ((f.destination=shootingKernels.shootingKernel[kernelNum].hitTriangles.get())
				)
			{
				f.power = f.destination->hits/shotsAccumulated;
				RR_ASSERT(f.power>0);
				//RR_ASSERT(f.power<=1); above 1 is ok in presence of specular reflectance/transmittance
				f.destination->hits = 0;
				if (!i.insert(f))
				{
					shotsForFactorsTotal = UINT_MAX-1; // stop improving, avgAccuracy() will return number high enough for everyone
					break;
				}
			}
			shootingKernels.shootingKernel[kernelNum].hitTriangles.reset();
		}
		source.node->totalExitingFluxToDiffuse=source.node->totalExitingFlux;
		source.node->shotsForFactors=shotsAccumulated;
		shotsAccumulated=0;
		shotsForFactorsTotal+=source.node->shotsForFactors;
		phase=0;
	}
}

real Scene::avgAccuracy()
{
	return (1+shotsForFactorsTotal)/(1.f+object->triangles);
}

//////////////////////////////////////////////////////////////////////////////
//
// distribute energy from source
// refresh form factors and split triangles and subtriangles if needed
// return if everything was distributed

// vraci true pri improved
bool Scene::energyFromDistributedUntil(BestInfo source,RRStaticSolver::EndFunc& endfunc)
{
	// refresh unaccurate form factors
	if (phase==0)
	{
		if (source.needsRefresh())
		{
			STATISTIC_INC(numCallsRefreshFactors);
		}
		else
		{
			STATISTIC_INC(numCallsDistribFactors);
		}
	}
	if (source.needsRefresh())
	{
		refreshFormFactorsFromUntil(source,endfunc);
	}
	if (phase==0)
	{
		// distribute energy via form factors
		for (ChunkList<Factor>::const_iterator i=source.node->factors.begin(); *i; ++i)
			distributeEnergyViaFactor(**i, source.node->totalExitingFluxToDiffuse, &staticReflectors);

		source.node->totalExitingFluxToDiffuse=Channels(0);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
//
// improve global illumination in scene by only distributing energy

bool Scene::distribute(real maxError)
{
	bool distributed=false;
	int steps=0;
	int rezerva=20;
	while (1)
	{
		BestInfo source=staticReflectors.best(sum(abs(staticSourceExitingFlux)));
		if (!source.node || ( sum(abs(source.node->totalExitingFluxToDiffuse))<sum(abs(staticSourceExitingFlux*maxError)) && !rezerva--)) break;

		for (ChunkList<Factor>::const_iterator i=source.node->factors.begin(); *i; ++i)
			distributeEnergyViaFactor(**i, source.node->totalExitingFluxToDiffuse, &staticReflectors);

		source.node->totalExitingFluxToDiffuse=Channels(0);
		steps++;
		distributed=true;
	}
	return distributed;
}

//////////////////////////////////////////////////////////////////////////////
//
// improve global illumination in scene

RRStaticSolver::Improvement Scene::improveStatic(RRStaticSolver::EndFunc& endfunc)
{
	if (!IS_CHANNELS(staticSourceExitingFlux))
		return RRStaticSolver::INTERNAL_ERROR; // invalid internal data
	STATISTIC_INC(numCallsImprove);
	RRStaticSolver::Improvement improved=RRStaticSolver::NOT_IMPROVED;

	do
	{
		if (improvingStatic.node==NULL)
			improvingStatic=staticReflectors.best(sum(abs(staticSourceExitingFlux)));
		if (improvingStatic.node==NULL) 
		{
			improved = RRStaticSolver::FINISHED;
			break;
		}
		if (energyFromDistributedUntil(improvingStatic,endfunc))
		{
			improvingStatic.node=NULL;
			improved=RRStaticSolver::IMPROVED;
		}
	}
	while (!endfunc.requestsEnd());

	return improved;
}

//////////////////////////////////////////////////////////////////////////////
//
// abort/shorten/finish previously started improvement

void Scene::abortStaticImprovement()
{
	if (improvingStatic.node)
	{
		Triangle *hitTriangle;
		for (unsigned kernelNum=0;kernelNum<shootingKernels.numKernels;kernelNum++)
		{
			while ((hitTriangle=shootingKernels.shootingKernel[kernelNum].hitTriangles.get())) hitTriangle->hits=0;
			shootingKernels.shootingKernel[kernelNum].hitTriangles.reset();
		}
		shotsAccumulated=0;
		phase=0;
		improvingStatic.node=NULL;
	}
}

// skonci pokud uz je hitu aspon tolikrat vic nez kolik bylo pouzito na stavajici kvalitu

bool Scene::shortenStaticImprovementIfBetterThan(real minimalImprovement)
{
	RR_ASSERT((improvingStatic!=NULL) == (phase!=0));
	if (improvingStatic.node)
	{
		// za techto podminek at uz dal nestrili
		if (phase==1 && shotsAccumulated>=minimalImprovement*improvingStatic.node->shotsForFactors) phase=2;
		// vraci uspech pokud uz nestrili ale jeste neskoncil
		return phase>=2;
	}
	return false;
}

class NeverEnd : public RRStaticSolver::EndFunc
{
public:
	virtual bool requestsEnd() {return false;}
	virtual bool requestsRealtimeResponse() {return false;}
};

bool Scene::finishStaticImprovement()
{
	RR_ASSERT((improvingStatic!=NULL) == (phase!=0));
	if (improvingStatic.node)
	{
		RR_ASSERT(phase>0);
		NeverEnd neverEnd;
		bool e=energyFromDistributedUntil(improvingStatic,neverEnd);
		RR_ASSERT(e); e=e;
		RR_ASSERT(phase==0);
		improvingStatic.node=NULL;
		return true;
	}
	return false;
}

} // namespace

