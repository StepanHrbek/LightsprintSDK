#include <cassert>
#include <map>
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
		char side;
		char edge;
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
// gather

// OMP parallel inside
// thread safe: yes
// outputs:
//  - triangleNumbers, multiobj postImport numbers, UINT_MAX for skybox, may be NULL
//  - exitanceHdr, float exitance in physical scale, may be NULL
static bool cubeMapGather(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const RRObject* object, const RRBuffer* environment, const RRScaler* scalerForReadingEnv, RRVec3 center, unsigned size, RRRay* ray6, unsigned* triangleNumbers, RRVec3* exitanceHdr)
{
	if((!scene && !packedSolver) || !object || (!triangleNumbers && !exitanceHdr))
	{
		if(!scene && !packedSolver)
			{LIMITED_TIMES(5,RRReporter::report(WARN,"Updating envmap, but lighting is not computed yet, call setStaticObjects() and calculate() first.\n"));}
		else
			RR_ASSERT(0);
		return false;
	}
	if(environment && !environment->getScaled()) scalerForReadingEnv = NULL;
// vypnuto kdyz nas vola worker thread s nizkou prioritou (!exitanceHdr), omp paralelizace je nezadouci, je mozne ze by to rozdelil mezi dalsi thready s normalni prioritou
// zapnuto kdyz nas vola uzivatel (exitanceHdr)
//#pragma omp parallel for if(exitanceHdr!=NULL) schedule(dynamic) // fastest: dynamic, static
//!!! if je v vc2005 rozbity, zapnuto vzdy i kdyz nas vola worker thread, nevim co to udela
	#pragma omp parallel for schedule(dynamic) // fastest: dynamic, static
	for(int side=0;side<6;side++)
	{
		RRRay* ray = ray6+side;
		for(unsigned j=0;j<size;j++)
			for(unsigned i=0;i<size;i++)
			{
				unsigned ofs = i+(j+side*size)*size;
				RRVec3 dir = cubeSide[side].getTexelDir(size,i,j);
				RRReal dirsize = dir.length();
				// find face
				ray->rayOrigin = center;
				ray->rayDirInv[0] = dirsize/dir[0];
				ray->rayDirInv[1] = dirsize/dir[1];
				ray->rayDirInv[2] = dirsize/dir[2];
				ray->rayLengthMin = 0;
				ray->rayLengthMax = 10000; //!!! hard limit
				ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::TEST_SINGLESIDED;
				unsigned face = object->getCollider()->intersect(ray) ? ray->hitTriangle : UINT_MAX;
				if(triangleNumbers)
				{
					triangleNumbers[ofs] = face;
				}
				if(exitanceHdr)
				{
					if(face==UINT_MAX)
					{
						// read exitance of sky
						//exitanceHdr[ofs] = environment ? environment->getValue(dir) : RRVec3(0);
						if(!environment)
						{
							exitanceHdr[ofs] = RRVec3(0);
						}
						else
						{
							exitanceHdr[ofs] = environment->getElement(dir);
							if(scalerForReadingEnv) scalerForReadingEnv->getPhysicalScale(exitanceHdr[ofs]);
						}
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
					else if(packedSolver)
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
	}
	return true;
}


// thread safe: yes
// converts triangle numbers to float exitance in physical scale
static void cubeMapConvertTrianglesToExitances(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const RRBuffer* environment, unsigned size, unsigned* triangleNumbers, RRVec3* exitanceHdr)
{
	if(!scene && !packedSolver)
	{
		RR_ASSERT(0);
		return;
	}
	if(!triangleNumbers || !exitanceHdr)
	{
		RR_ASSERT(0);
		return;
	}
#pragma omp parallel for schedule(static)
	for(int ofs=0;ofs<(int)(6*size*size);ofs++)
	{
		unsigned face = triangleNumbers[ofs];
		if(face==UINT_MAX)
		{
			if(!environment)
				exitanceHdr[ofs] = RRVec3(0);
			else
			{
				// read exitance of sky
				exitanceHdr[ofs] = environment->getElement(cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size));
				RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
			}
		}
		else if(packedSolver)
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
	for(int oside=0;oside<6;oside++)
	{
		for(unsigned oj=0;oj<oSize;oj++)
			for(unsigned oi=0;oi<oSize;oi++)
			{
				RRVec3 odir = cubeSide[oside].getTexelDirSticky(oSize,oi,oj).normalized();
				interpolator->learnDestinationBegin();
				RRReal sum = 0;
				for(unsigned iside=0;iside<6;iside++)
				{
					for(unsigned ij=0;ij<iSize;ij++)
						for(unsigned ii=0;ii<iSize;ii++)
						{
							RRVec3 idir = cubeSide[iside].getTexelDir(iSize,ii,ij).normalized(); //!!! lze taky predpocitat
							RRReal contrib = dot(odir,idir)+radius-1;
							if(contrib>0)
							{
								RRReal iTexelSize = cubeSide[iside].getTexelSize(iSize,ii,ij); //!!! lze predpocitat pro vsechny texely
								RR_ASSERT(contrib*iTexelSize>0);
								sum += contrib*iTexelSize;
							}
						}
				}
				RR_ASSERT(sum>0);
				for(unsigned iside=0;iside<6;iside++)
				{
					for(unsigned ij=0;ij<iSize;ij++)
						for(unsigned ii=0;ii<iSize;ii++)
						{
							RRVec3 idir = cubeSide[iside].getTexelDir(iSize,ii,ij).normalized();
							RRReal contrib = dot(odir,idir)+radius-1;
							if(contrib>0)
							{
								RRReal iTexelSize = cubeSide[iside].getTexelSize(iSize,ii,ij); //!!! lze predpocitat pro vsechny texely
								RRReal finalContrib = contrib*iTexelSize/sum;
								interpolator->learnSource(iSize*iSize*iside+iSize*ij+ii,finalContrib);
							}
						}
				}
				interpolator->learnDestinationEnd(oSize*oSize*oside+oSize*oj+oi,oSize*oSize*oside+oSize*oj+oi,oSize*oSize*oside+oSize*oj+oi);
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
		Key key;
		key.iSize = iSize;
		key.oSize = oSize;
		key.radius = radius;
		Interpolator* result;
		#pragma omp critical
		{
			Map::const_iterator i = interpolators.find(key);
			if(i!=interpolators.end())
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
		for(Map::iterator i=interpolators.begin();i!=interpolators.end();i++)
		{
			delete i->second;
		}
	}
private:
	struct Key
	{
		unsigned iSize;
		unsigned oSize;
		RRReal radius;
		bool operator <(const Key& a) const
		{
			return memcmp(this,&a,sizeof(Key))<0;
		}
	};
	typedef std::map<Key,Interpolator*> Map;
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
	for(unsigned side=0;side<4;side++)
	{
		// process all 4 edges			
		for(unsigned edge=0;edge<4;edge++)
		{
			for(unsigned a=(side<2)?0:1;a<iSize-1;a++)
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
				if(a==0)
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

#define FILL_CUBEMAP(filteredSize, radius, map) \
	if(map) \
	{ \
		const Interpolator* interpolator = cache.getInterpolator(gatherSize,filteredSize,radius); \
		interpolator->interpolate(gatheredExitance,filteredExitance,priv->scaler); \
		map->reset(BT_CUBE_TEXTURE,filteredSize,filteredSize,6,BF_RGBF,priv->scaler?true:false,(const unsigned char*)filteredExitance); \
		updatedMaps++; \
	}


void RRDynamicSolver::updateEnvironmentMapCache(RRObjectIllumination* illumination)
{
	if(!illumination || !illumination->gatherEnvMapSize || (!illumination->diffuseEnvMap && !illumination->specularEnvMap))
	{
		RR_ASSERT(0);
		return;
	}
	unsigned specularSize = illumination->specularEnvMap ? illumination->specularEnvMapSize : 0;
	unsigned diffuseSize = illumination->diffuseEnvMap ? illumination->diffuseEnvMapSize : 0;
	unsigned gatherSize = (specularSize+diffuseSize) ? illumination->gatherEnvMapSize : 0;
	if(!gatherSize)
	{
		RR_ASSERT(0);
		return;
	}
	if(gatherSize!=illumination->cachedGatherSize || (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()>CENTER_GRANULARITY)
	{
		if(illumination->cachedGatherSize!=gatherSize)
		{
			SAFE_DELETE_ARRAY(illumination->cachedTriangleNumbers);
		}
		if(!illumination->cachedTriangleNumbers)
			illumination->cachedTriangleNumbers = new unsigned[6*gatherSize*gatherSize];
		if(cubeMapGather(priv->scene,priv->packedSolver,getMultiObjectCustom(),NULL,NULL,illumination->envMapWorldCenter,gatherSize,illumination->ray6,illumination->cachedTriangleNumbers,NULL))
		{
			// gather succeeded, mark cache as valid
			// (without taking care, we would cache invalid data in 1st frame [when solver is not created yet]
			//  and not fix them until object moves)
			illumination->cachedGatherSize = gatherSize;
			illumination->cachedCenter = illumination->envMapWorldCenter;
		}
		else
		{
			// gather failed, mark cache as invalid
			illumination->cachedGatherSize = 0;
		}
	}
}

unsigned RRDynamicSolver::updateEnvironmentMap(RRObjectIllumination* illumination)
{
	if(!illumination)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned specularSize = illumination->specularEnvMap ? illumination->specularEnvMapSize : 0;
	unsigned diffuseSize = illumination->diffuseEnvMap ? illumination->diffuseEnvMapSize : 0;
	unsigned gatherSize = (specularSize+diffuseSize) ? illumination->gatherEnvMapSize : 0;
	if(!gatherSize)
	{
		RR_ASSERT(0);
		return 0;
	}

	// alloc temp space
	RRVec3* gatheredExitance = new RRVec3[6*gatherSize*gatherSize + 6*specularSize*specularSize + 6*diffuseSize*diffuseSize];
	RRVec3* filteredExitance = gatheredExitance + 6*gatherSize*gatherSize;

	if(gatherSize!=illumination->cachedGatherSize || (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()>CENTER_GRANULARITY)
	{
		if(illumination->cachedGatherSize!=gatherSize)
		{
			SAFE_DELETE_ARRAY(illumination->cachedTriangleNumbers);
		}
		if(!illumination->cachedTriangleNumbers)
		{
			illumination->cachedTriangleNumbers = new unsigned[6*gatherSize*gatherSize];
		}
		if(cubeMapGather(priv->scene,priv->packedSolver,getMultiObjectCustom(),getEnvironment(),getScaler(),illumination->envMapWorldCenter,gatherSize,illumination->ray6,illumination->cachedTriangleNumbers,gatheredExitance))
		{
			// gather succeeded, mark cache as valid
			// (without taking care, we would cache invalid data in 1st frame [when solver is not created yet]
			//  and not fix them until object moves)
			illumination->cachedGatherSize = gatherSize;
			illumination->cachedCenter = illumination->envMapWorldCenter;
		}
		else
		{
			// gather failed, mark cache as invalid
			illumination->cachedGatherSize = 0;
		}
	}
	else
	{
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getEnvironment(),gatherSize,illumination->cachedTriangleNumbers,gatheredExitance);
	}

	// early exit if we don't have valid data
	if(!illumination->cachedGatherSize)
		return 0;

	// fill envmaps
	unsigned minSize = MIN(gatherSize,specularSize);
	RRReal minDot = minSize*sqrtf(1.0f/(3+minSize*minSize));
	unsigned updatedMaps = 0;
	FILL_CUBEMAP(diffuseSize,0.9f,illumination->diffuseEnvMap);
	FILL_CUBEMAP(specularSize,1-minDot,illumination->specularEnvMap);

	// cleanup
	delete[] gatheredExitance;

	return updatedMaps;
}


} // namespace
