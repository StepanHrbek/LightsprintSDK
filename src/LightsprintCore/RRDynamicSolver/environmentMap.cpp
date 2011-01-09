// --------------------------------------------------------------------------
// Realtime update of cube reflection maps.
// Copyright (c) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <boost/unordered_map.hpp>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "Interpolator.h"
#include "report.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"

#define CENTER_GRANULARITY 0.01f // if envmap center moves less than granularity, it is considered unchanged. prevents updates when dynamic object rotates (=position slightly fluctuates)

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// cube connectivity

enum Edge {TOP=0, LEFT, RIGHT, BOTTOM};

struct CubeSide
{
	signed char dir[3];
	struct Neighbour
	{
		unsigned char side;
		unsigned char edge;
	};
	Neighbour neighbour[4]; // indexed by Edge

	RRVec3 getDir() const
	{
		return RRVec3(dir[0],dir[1],dir[2]);
	}

	// returns direction from cube center to cube texel center.
	// texel coordinates: -1..1
	RRVec3 getTexelDir(RRVec2 uv) const;

	// returns direction from cube center to cube texel center.
	// texel coordinates: 0..size-1
	RRVec3 getTexelDir(unsigned size, unsigned x, unsigned y) const
	{
		return getTexelDir(RRVec2( RRReal(int(1+2*x-size))/size , RRReal(int(1+2*y-size))/size ));
	}

	// as getTexelDir, but for edge/corner texels, returns dir to edge/corner of cube
	RRVec3 getTexelDirSticky(unsigned size, unsigned x, unsigned y) const
	{
		return getTexelDir(RRVec2(
			(x==0) ? -1 : ( (x>=size-1) ? 1 : RRReal(int(1+2*x-size))/size ),
			(y==0) ? -1 : ( (y>=size-1) ? 1 : RRReal(int(1+2*y-size))/size )
			));
	}

	// returns texel size in steradians. approximative
	// texel coordinates: 0..size-1
	static RRReal getTexelSize(unsigned size, unsigned x, unsigned y)
	{
		RRReal angle1 = dot(
			RRVec3(1,RRReal(int(1+2*x-size))/size,RRReal(int(  2*y-size))/size).normalized(),
			RRVec3(1,RRReal(int(1+2*x-size))/size,RRReal(int(2+2*y-size))/size).normalized() );
		RRReal angle2 = dot(
			RRVec3(1,RRReal(int(  2*x-size))/size,RRReal(int(1+2*y-size))/size).normalized(),
			RRVec3(1,RRReal(int(2+2*x-size))/size,RRReal(int(1+2*y-size))/size).normalized() );
		return acos(angle1)*acos(angle2);
	}

	// returns nearest texel index (in format side*size*size+j*size+i) to x,y inside side reached from this via edge
	int getNeighbourTexelIndex(unsigned size,Edge edge, unsigned x,unsigned y) const;
};

static const CubeSide cubeSide[6] =
{
	// describes cubemap sides in opengl order
	{{ 1, 0, 0}, {{2,RIGHT },{4,RIGHT },{5,LEFT  },{3,RIGHT }}},
	{{-1, 0, 0}, {{2,LEFT  },{5,RIGHT },{4,LEFT  },{3,LEFT  }}},
	{{ 0, 1, 0}, {{5,TOP   },{1,TOP   },{0,TOP   },{4,TOP   }}},
	{{ 0,-1, 0}, {{4,BOTTOM},{1,BOTTOM},{0,BOTTOM},{5,BOTTOM}}},
	{{ 0, 0, 1}, {{2,BOTTOM},{1,RIGHT },{0,LEFT  },{3,TOP   }}},
	{{ 0, 0,-1}, {{2,TOP   },{0,RIGHT },{1,LEFT  },{3,BOTTOM}}}
};

// direction from cube center to texel center
RRVec3 CubeSide::getTexelDir(RRVec2 uv) const
{
	return getDir()+cubeSide[neighbour[RIGHT].side].getDir()*uv[0]+cubeSide[neighbour[BOTTOM].side].getDir()*uv[1];
}

int CubeSide::getNeighbourTexelIndex(unsigned size,Edge edge, unsigned x,unsigned y) const
{
	// input texel x,y is in this, close to edge
	// looking for output neighbour texel i,j in neighbour[edge].side, close to neighbour[edge].edge
	// 4 possible ways how i,j is derived from x,y:
	//  x,y -> size-1-x,y (TOP-TOP, BOTTOM-BOTTOM, LEFT-RIGHT, RIGHT-LEFT)
	//  x,y -> x,size-1-y (LEFT-LEFT, RIGHT-RIGHT, TOP-BOTTOM, BOTTOM-TOP)
	//  x,y -> y,x (TOP-LEFT, BOTTOM-RIGHT, RIGHT-BOTTOM, LEFT-TOP)
	//  x,y -> size-1-y,size-1-x (TOP-RIGHT, BOTTOM-LEFT, RIGHT-TOP, LEFT-BOTTOM)
	unsigned i,j;
	#define TWO_EDGES(edge1,edge2) ((edge1)*4+(edge2))
	switch(TWO_EDGES(edge,neighbour[edge].edge))
	{
		case TWO_EDGES(TOP,TOP):
		case TWO_EDGES(BOTTOM,BOTTOM):
		case TWO_EDGES(LEFT,RIGHT):
		case TWO_EDGES(RIGHT,LEFT):
			i = size-1-x;
			j = y;
			break;
		case TWO_EDGES(TOP,BOTTOM):
		case TWO_EDGES(BOTTOM,TOP):
		case TWO_EDGES(LEFT,LEFT):
		case TWO_EDGES(RIGHT,RIGHT):
			i = x;
			j = size-1-y;
			break;
		case TWO_EDGES(TOP,LEFT):
		case TWO_EDGES(BOTTOM,RIGHT):
		case TWO_EDGES(LEFT,TOP):
		case TWO_EDGES(RIGHT,BOTTOM):
			i = y;
			j = x;
			break;
		case TWO_EDGES(TOP,RIGHT):
		case TWO_EDGES(BOTTOM,LEFT):
		case TWO_EDGES(LEFT,BOTTOM):
		case TWO_EDGES(RIGHT,TOP):
			i = size-1-y;
			j = size-1-x;
			break;
		default:
			RR_ASSERT(0);
	}
	#undef TWO_EDGES
	return neighbour[edge].side*size*size+j*size+i;
}


/////////////////////////////////////////////////////////////////////////////
//
// collision handler

class ReflectionCubeCollisionHandler : public RRCollisionHandler
{
public:
	void setup(const RRObject* _multiObject, unsigned _singleObjectNumber, float _singleObjectWorldRadius)
	{
		multiObject = _multiObject;
		singleObjectNumber = _singleObjectNumber;
		singleObjectWorldRadius = _singleObjectWorldRadius;
	}
	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_DISTANCE;
		hitTriangleMemory = UINT_MAX;
	}
	virtual bool collides(const RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&RRRay::FILL_SIDE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_DISTANCE);

		// don't collide with self
		RRMesh::PreImportNumber preImport = multiObject->getCollider()->getMesh()->getPreImportTriangle(ray->hitTriangle);
		if (preImport.object==singleObjectNumber)
		{
			hitTriangleMemory = UINT_MAX;
			return false;
		}

		// don't collide with transparent points
		RRMaterial* material = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
		if (!material->sideBits[ray->hitFrontSide?0:1].renderFrom || material->specularTransmittance.color==RRVec3(1))
			return false;
		//RRPointMaterial pointMaterial;
		//multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial,scaler);
		//if (!pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom || pointMaterial.specularTransmittance.color==RRVec3(1))
		//	return false;

		// kdyz zasahnu jiny obj Y uvnitr AABB X: { pokud je pamet M prazdna, zapamatovat si Y. pokracovat }
		// kdyz zasahnu jiny obj Y mimo AABB X: { skoncit s M?M:Z }
		if (hitTriangleMemory==UINT_MAX)
			hitTriangleMemory = ray->hitTriangle;
		return ray->hitDistance>singleObjectWorldRadius;
	}
	virtual bool done()
	{
		return hitTriangleMemory!=UINT_MAX;
	}
	unsigned gethHitTriangle()
	{
		return hitTriangleMemory;
	}
private:
	const RRObject* multiObject;
	unsigned singleObjectNumber;
	float singleObjectWorldRadius;
	unsigned hitTriangleMemory;
};


/////////////////////////////////////////////////////////////////////////////
//
// gathering kit

CubeGatheringKit::CubeGatheringKit()
{
	inUse = false;
	ray6 = RRRay::create(6);
	handler6 = new ReflectionCubeCollisionHandler[6];
	for (unsigned i=0;i<6;i++)
		ray6[i].collisionHandler = handler6+i;
}

CubeGatheringKit::~CubeGatheringKit()
{
	delete[] handler6;
	delete[] ray6;
}


/////////////////////////////////////////////////////////////////////////////
//
// gather

// OMP parallel inside
// thread safe: yes
// outputs:
//  - triangleNumbers, multiobj postImport numbers, UINT_MAX for skybox, may be NULL
//  - exitanceHdr, float exitance in physical scale, may be NULL
bool RRDynamicSolver::cubeMapGather(RRObjectIllumination* illumination, RRVec3* exitanceHdr)
{
	const RRObject* multiObject = getMultiObjectCustom();
	const RRBuffer* environment0 = getEnvironment(0);
	const RRBuffer* environment1 = getEnvironment(1);
	float blendFactor = getEnvironmentBlendFactor();
	const RRScaler* scalerForReadingEnv = getScaler();
	unsigned gatherSize = illumination->gatherEnvMapSize;

	if (gatherSize==illumination->cachedGatherSize && (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()<=CENTER_GRANULARITY)
		return false;

	if (illumination->cachedGatherSize!=gatherSize)
	{
		RR_SAFE_DELETE_ARRAY(illumination->cachedTriangleNumbers);
	}
	if (!illumination->cachedTriangleNumbers)
	{
		illumination->cachedTriangleNumbers = new unsigned[6*gatherSize*gatherSize];
	}
	if ((!priv->scene && !priv->packedSolver) || !multiObject)
	{
		// this is legal, renderer asks us to build small cubemap from solver with big environment and 0 objects
		//RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Updating envmap, but lighting is not computed yet, call setStaticObjects() and calculate() first.\n"));
	}

	// simplify tests for blending from if(env1 && blendFactor) to if(env1)
	if (!blendFactor) environment1 = NULL;

	// simplify tests for scaling from if(env0 && env0->getScaled() && scalerForReadingEnv0) to if(scalerForReadingEnv0)
	const RRScaler* scalerForReadingEnv0 = (environment0 && environment0->getScaled()) ? scalerForReadingEnv : NULL;
	const RRScaler* scalerForReadingEnv1 = (environment1 && environment1->getScaled()) ? scalerForReadingEnv : NULL;
	unsigned size = illumination->gatherEnvMapSize;

	// rather than adding 1 kit to every RRObjectIllumination, we added 10 to solver and pick one of them
	// if user doesn't call updateEnvironmentMap() in parallel, we always use the same first kit
	CubeGatheringKit* kit = NULL;
	#pragma omp critical(cubeGatheringKits)
	{
		unsigned i=0;
		while (priv->cubeGatheringKits[i].inUse) ++i%=10;
		kit = priv->cubeGatheringKits+i;
		kit->inUse = true;
	}

	#pragma omp parallel for schedule(dynamic) // fastest: dynamic, static
	for (int side=0;side<6;side++)
	{
		kit->handler6[side].setup(multiObject,illumination->envMapObjectNumber,illumination->envMapWorldRadius);
		RRRay* ray = kit->ray6+side;
		for (unsigned j=0;j<size;j++)
			for (unsigned i=0;i<size;i++)
			{
				unsigned ofs = i+(j+side*size)*size;
				RRVec3 dir = cubeSide[side].getTexelDir(size,i,j);
				unsigned face = UINT_MAX;
				if (multiObject)
				{
					RRReal dirsize = dir.length();
					// find face
					ray->rayOrigin = illumination->envMapWorldCenter;
					ray->rayDirInv[0] = dirsize/dir[0];
					ray->rayDirInv[1] = dirsize/dir[1];
					ray->rayDirInv[2] = dirsize/dir[2];
					ray->rayLengthMin = 0;
					ray->rayLengthMax = 10000; //!!! hard limit
					ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::TEST_SINGLESIDED;
					if (multiObject->getCollider()->intersect(ray))
						face = kit->handler6[side].gethHitTriangle(); //ray->hitTriangle;
				}
				if (illumination->cachedTriangleNumbers)
				{
					illumination->cachedTriangleNumbers[ofs] = face;
				}
				if (exitanceHdr)
				{
					if (face==UINT_MAX)
					{
						// read exitance of sky
						if (!environment0)
						{
							// no environment
							exitanceHdr[ofs] = RRVec3(0);
						}
						else
						if (!environment1)
						{
							// 1 environment
							exitanceHdr[ofs] = environment0->getElementAtDirection(dir);
							if (scalerForReadingEnv0) scalerForReadingEnv0->getPhysicalScale(exitanceHdr[ofs]);
						}
						else
						{
							// blend of 2 environments
							RRVec3 env0color = environment0->getElementAtDirection(dir);
							if (scalerForReadingEnv0) scalerForReadingEnv0->getPhysicalScale(env0color);
							RRVec3 env1color = environment1->getElementAtDirection(dir);
							if (scalerForReadingEnv1) scalerForReadingEnv1->getPhysicalScale(env1color);
							exitanceHdr[ofs] = env0color*(1-blendFactor)+env1color*blendFactor;
						}
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
					else if (priv->packedSolver)
					{
						// read face exitance
						exitanceHdr[ofs] = priv->packedSolver->getTriangleExitance(face);
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
					else if (priv->scene)
					{
						// read face exitance
						priv->scene->getTriangleMeasure(face,3,RM_EXITANCE_PHYSICAL,NULL,exitanceHdr[ofs]);
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
				}
			}
	}
	kit->inUse = false;
	illumination->cachedGatherSize = gatherSize;
	illumination->cachedCenter = illumination->envMapWorldCenter;
	return true;
}


// thread safe: yes
// converts triangle numbers to float exitance in physical scale
static void cubeMapConvertTrianglesToExitances(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const RRBuffer* environment0, const RRBuffer* environment1, float blendFactor, const RRScaler* scalerForReadingEnv, unsigned size, unsigned* triangleNumbers, RRVec3* exitanceHdr)
{
	if (!scene && !packedSolver)
	{
		RR_ASSERT(0);
		return;
	}
	if (!triangleNumbers || !exitanceHdr)
	{
		RR_ASSERT(0);
		return;
	}

	// simplify tests for blending from if(env1 && blendFactor) to if(env1)
	if (!blendFactor) environment1 = NULL;

	// simplify tests for scaling from if(env0->getScaled() && scalerForReadingEnv) to if(scalerForReadingEnv0)
	const RRScaler* scalerForReadingEnv0 = (environment0 && environment0->getScaled()) ? scalerForReadingEnv : NULL;
	const RRScaler* scalerForReadingEnv1 = (environment1 && environment1->getScaled()) ? scalerForReadingEnv : NULL;

#pragma omp parallel for schedule(static)
	for (int ofs=0;ofs<(int)(6*size*size);ofs++)
	{
		unsigned face = triangleNumbers[ofs];
		if (face==UINT_MAX)
		{
			// read exitance of sky
			if (!environment0)
			{
				// no environment
				exitanceHdr[ofs] = RRVec3(0);
			}
			else
			if (!environment1)
			{
				// 1 environment
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				exitanceHdr[ofs] = environment0->getElementAtDirection(dir);
				if (scalerForReadingEnv0) scalerForReadingEnv0->getPhysicalScale(exitanceHdr[ofs]);
				RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
			}
			else
			{
				// blend of 2 environments
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				RRVec3 env0color = environment0->getElementAtDirection(dir);
				if (scalerForReadingEnv0) scalerForReadingEnv0->getPhysicalScale(env0color);
				RRVec3 env1color = environment1->getElementAtDirection(dir);
				if (scalerForReadingEnv1) scalerForReadingEnv1->getPhysicalScale(env1color);
				exitanceHdr[ofs] = env0color*(1-blendFactor)+env1color*blendFactor;
				RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
			}
		}
		else if (packedSolver)
		{
			// read face exitance
			exitanceHdr[ofs] = packedSolver->getTriangleExitance(face);
			RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
		}
		else
		{
			// read face exitance
			scene->getTriangleMeasure(face,3,RM_EXITANCE_PHYSICAL,NULL,exitanceHdr[ofs]);
			RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// general filter

// thread safe: yes
// radius = 1-minDot (dot=cos(angle) musi byt aspon tak velky jako minDot, jinak jde o prilis vzdaleny texel)
void buildCubeFilter(unsigned iSize, unsigned oSize, float radius, Interpolator* interpolator)
{
	for (int oside=0;oside<6;oside++)
	{
		for (unsigned oj=0;oj<oSize;oj++)
			for (unsigned oi=0;oi<oSize;oi++)
			{
				RRVec3 odir = cubeSide[oside].getTexelDirSticky(oSize,oi,oj).normalized();
				interpolator->learnDestinationBegin();
				RRReal sum = 0;
				for (unsigned iside=0;iside<6;iside++)
				{
					for (unsigned ij=0;ij<iSize;ij++)
						for (unsigned ii=0;ii<iSize;ii++)
						{
							RRVec3 idir = cubeSide[iside].getTexelDir(iSize,ii,ij).normalized(); //!!! lze taky predpocitat
							RRReal contrib = dot(odir,idir)+radius-1;
							if (contrib>0)
							{
								RRReal iTexelSize = cubeSide[iside].getTexelSize(iSize,ii,ij); //!!! lze predpocitat pro vsechny texely
								RR_ASSERT(contrib*iTexelSize>0);
								sum += contrib*iTexelSize;
							}
						}
				}
				RR_ASSERT(sum>0);
				for (unsigned iside=0;iside<6;iside++)
				{
					for (unsigned ij=0;ij<iSize;ij++)
						for (unsigned ii=0;ii<iSize;ii++)
						{
							RRVec3 idir = cubeSide[iside].getTexelDir(iSize,ii,ij).normalized();
							RRReal contrib = dot(odir,idir)+radius-1;
							if (contrib>0)
							{
								RRReal iTexelSize = cubeSide[iside].getTexelSize(iSize,ii,ij); //!!! lze predpocitat pro vsechny texely
								RRReal finalContrib = contrib*iTexelSize/sum;
								interpolator->learnSource(iSize*iSize*iside+iSize*ij+ii,finalContrib);
							}
						}
				}
				interpolator->learnDestinationEnd(oSize*oSize*oside+oSize*oj+oi);
			}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// general filter cache

// thread safe: yes
class InterpolatorCache
{
public:
	InterpolatorCache()
	{
	}
	const Interpolator* getInterpolator(unsigned iSize, unsigned oSize, RRReal radius)
	{
		// this is just hash, but good enough for us
		// (there was problem with proper struct Key in VS2010, reducing Key to float solved it)
		float key = radius + iSize + oSize*3.378f;

		Interpolator* result;
		#pragma omp critical(interpolators)
		{
			Map::const_iterator i = interpolators.find(key);
			if (i!=interpolators.end())
			{
				// found in cache
				result = i->second;
			}
			else
			{
				// not found in cache
				result = interpolators[key] = new Interpolator();
				buildCubeFilter(iSize,oSize,radius,result);
			}
		}
		return result;
	}
	~InterpolatorCache()
	{
		for (Map::iterator i=interpolators.begin();i!=interpolators.end();i++)
		{
			delete i->second;
		}
	}
private:
	typedef boost::unordered_map<float,Interpolator*> Map;
	Map interpolators;
};

static InterpolatorCache cache;


/////////////////////////////////////////////////////////////////////////////
//
// hardcoded filter

// thread safe: yes
template<class CubeColor>
static void filterEdges(unsigned iSize, CubeColor* iExitance)
{
	// process only x+/x- corners&edges and y+/y- edges
	for (unsigned side=0;side<4;side++)
	{
		// process all 4 edges			
		for (unsigned edge=0;edge<4;edge++)
		{
			for (unsigned a=(side<2)?0:1;a<iSize-1;a++)
			{
				unsigned i,j,edge2; // edge2: hrana stykajici se s edge v rohu, proti smeru hod.rucicek
				switch(edge)
				{
					case TOP: i=a;j=0;edge2=LEFT;break;
					case RIGHT: i=iSize-1;j=a;edge2=TOP;break;
					case BOTTOM: i=iSize-1-a;j=iSize-1;edge2=RIGHT;break;
					case LEFT: i=0;j=iSize-1-a;edge2=BOTTOM;break;
					default: RR_ASSERT(0);
				}
				unsigned idx1 = side*iSize*iSize+j*iSize+i;
				unsigned idx2 = cubeSide[side].getNeighbourTexelIndex(iSize,(Edge)edge,i,j);
				if (a==0)
				{
					// process corner (a+b+c)/3
					unsigned idx3 = cubeSide[side].getNeighbourTexelIndex(iSize,(Edge)edge2,i,j);
					iExitance[idx1] = iExitance[idx2] = iExitance[idx3] = (iExitance[idx1].toRRColorRGBF()+iExitance[idx2].toRRColorRGBF()+iExitance[idx3].toRRColorRGBF())*0.333333f;
				}
				else
				{
					// process edge: (a+b)/2
					iExitance[idx1] = iExitance[idx2] = (iExitance[idx1].toRRColorRGBF()+iExitance[idx2].toRRColorRGBF())*0.5f;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// main

// returns number of buffers updated
static unsigned filterToBuffer(unsigned version, RRVec3* gatheredExitance, unsigned gatherSize, const RRScaler* scaler, RRReal filterRadius, RRBuffer* buffer)
{
	RR_ASSERT(gatheredExitance);
	RR_ASSERT(gatherSize);
	RR_ASSERT(filterRadius);
	if (!buffer || buffer->getType()!=BT_CUBE_TEXTURE) return 0;
	const Interpolator* interpolator = cache.getInterpolator(gatherSize,buffer->getWidth(),filterRadius);
	interpolator->interpolate(gatheredExitance,buffer,scaler);
	// setting version from solver is not enough, cube would not update if it only moves around scene
	//buffer->version = version;
	buffer->version++;
	buffer->version = (version<<16)+(buffer->version&65535);
	return 1;
}

void RRDynamicSolver::updateEnvironmentMapCache(RRObjectIllumination* illumination)
{
	if (!illumination || !illumination->gatherEnvMapSize || (!illumination->diffuseEnvMap && !illumination->specularEnvMap))
	{
		RR_ASSERT(0);
		return;
	}
	unsigned specularSize = illumination->specularEnvMap ? illumination->specularEnvMap->getWidth() : 0;
	unsigned diffuseSize = illumination->diffuseEnvMap ? illumination->diffuseEnvMap->getWidth() : 0;
	unsigned gatherSize = (specularSize+diffuseSize) ? illumination->gatherEnvMapSize : 0;
	if (!gatherSize)
	{
		RR_ASSERT(0);
		return;
	}
	cubeMapGather(illumination,NULL);
}

unsigned RRDynamicSolver::updateEnvironmentMap(RRObjectIllumination* illumination)
{
	if (!illumination)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned solutionVersion = getSolutionVersion();
	bool centerMoved = (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()>CENTER_GRANULARITY;
	unsigned diffuseSize = (illumination->diffuseEnvMap && (centerMoved || (illumination->diffuseEnvMap->version>>16)!=(solutionVersion&65535))) ? illumination->diffuseEnvMap->getWidth() : 0;
	unsigned specularSize = (illumination->specularEnvMap && (centerMoved || (illumination->specularEnvMap->version>>16)!=(solutionVersion&65535))) ? illumination->specularEnvMap->getWidth() : 0;
	unsigned gatherSize = (specularSize+diffuseSize) ? illumination->gatherEnvMapSize : 0;
	if (!gatherSize)
	{
		return 0;
	}

	if (!priv->scene && !priv->packedSolver && !getEnvironment())
	{
		// BTW environment without solver is nothing unusual, called from RendererOfRRDynamicSolver::initSpecularReflection()
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"No solver, no environment, reflection map will be updated to black, call calculate().\n"));
	}
	if (!priv->scene && priv->packedSolver && !priv->customIrradianceRGBA8)
	{
		// this is legal, scene may be lit by skybox
		//RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Fireball lights not detected yet, reflection map will be updated to black, call calculate().\n"));
	}

	// alloc temp space
	RRVec3* gatheredExitance = new RRVec3[6*gatherSize*gatherSize];

	if (!cubeMapGather(illumination,gatheredExitance))
	{
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getEnvironment(0),getEnvironment(1),getEnvironmentBlendFactor(),getScaler(),gatherSize,illumination->cachedTriangleNumbers,gatheredExitance);
	}

	unsigned updatedMaps = 0;
	if (illumination->cachedGatherSize)
	{
		// fill envmaps
		if (diffuseSize)
			updatedMaps += filterToBuffer(solutionVersion,gatheredExitance,gatherSize,priv->scaler,0.9f,illumination->diffuseEnvMap);
		if (specularSize)
		{
			unsigned minSize = RR_MIN(gatherSize,specularSize);
			RRReal filterRadius = 1-minSize*sqrtf(1.0f/(3+minSize*minSize));
			//RRReal filterRadius = 0.25f/minSize;
			updatedMaps += filterToBuffer(solutionVersion,gatheredExitance,gatherSize,priv->scaler,filterRadius,illumination->specularEnvMap);
		}
	}

	// cleanup
	delete[] gatheredExitance;

	return updatedMaps;
}


} // namespace

