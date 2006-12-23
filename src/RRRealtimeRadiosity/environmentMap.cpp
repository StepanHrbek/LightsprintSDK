#include <cassert>
#include <map>
#include "Interpolator.h"
#include "RRRealtimeRadiosity.h"
#include "../src/RRMath/RRMathPrivate.h"
#include <windows.h>

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
			assert(0);
	}
	#undef TWO_EDGES
	return neighbour[edge].side*size*size+j*size+i;
}


/////////////////////////////////////////////////////////////////////////////
//
// gather

// thread safe: yes
static void cubeMapGather(const RRScene* scene, const RRObject* object, const RRScaler* scaler, RRVec3 center, unsigned size, RRColorRGBA8* irradianceLdr, RRColorRGBF* irradianceHdr)
{
	if(!scene)
	{
		assert(0);
		return;
	}
	if(!object)
	{
		assert(0);
		return;
	}
	if(!irradianceLdr && !irradianceHdr)
	{
		assert(0);
		return;
	}
	RRRay* ray = RRRay::create();
	for(unsigned side=0;side<6;side++)
	{
		for(unsigned j=0;j<size;j++)
			for(unsigned i=0;i<size;i++)
			{
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
				if(irradianceHdr)
				{
					if(face==UINT_MAX)
					{
						// read irradiance on sky
						*irradianceHdr = RRVec3(0); //!!! add sky
					}
					else
					{
						// read cube irradiance as face exitance
						scene->getTriangleMeasure(face,3,RM_EXITANCE_PHYSICAL,scaler,*irradianceHdr);
					}
					irradianceHdr++;
				}
				else
				{
					if(face==UINT_MAX)
					{
						*irradianceLdr = 0;
					}
					else
					{
						RRVec3 irrad;
						scene->getTriangleMeasure(face,3,RM_EXITANCE_CUSTOM,scaler,irrad);
						*irradianceLdr = irrad;
					}
					irradianceLdr++;
				}
			}
	}
	delete ray;
}


/////////////////////////////////////////////////////////////////////////////
//
// filter

void buildCubeFilter(unsigned iSize, unsigned oSize, float radius, Interpolator* interpolator)
{
	//!!! zapisovat vysledek na 1/2/3 mista
	for(unsigned oside=0;oside<6;oside++)
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
								assert(contrib*iTexelSize>0);
								sum += contrib*iTexelSize;
							}
						}
				}
				assert(sum>0);
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

// thread safe: yes
class InterpolatorCache
{
public:
	InterpolatorCache()
	{
		InitializeCriticalSection(&criticalSection);
	}
	const Interpolator* getInterpolator(unsigned iSize, unsigned oSize, RRReal radius)
	{
		Key key;
		key.iSize = iSize;
		key.oSize = oSize;
		key.radius = radius;
		EnterCriticalSection(&criticalSection);
		Map::const_iterator i = interpolators.find(key);
		Interpolator* result;
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
		LeaveCriticalSection(&criticalSection);
		return result;
	}
	~InterpolatorCache()
	{
		for(Map::iterator i=interpolators.begin();i!=interpolators.end();i++)
		{
			delete i->second;
		}
		DeleteCriticalSection(&criticalSection);
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
	CRITICAL_SECTION criticalSection;
};

static InterpolatorCache cache;


/////////////////////////////////////////////////////////////////////////////
//
// main

// thread safe: yes
void RRRealtimeRadiosity::updateEnvironmentMaps(RRVec3 objectCenter, unsigned gatherSize, unsigned specularSize, RRIlluminationEnvironmentMap* specularMap, unsigned diffuseSize, RRIlluminationEnvironmentMap* diffuseMap, bool HDR)
{
	if(!gatherSize)
	{
		assert(0);
		return;
	}
	if(!specularMap)
		specularSize = 0;
	if(!diffuseMap)
		diffuseSize = 0;
	if(!specularMap && !diffuseMap)
	{
		assert(0);
		return;
	}
	if(!scene)
	{
		assert(0);
		return;
	}
	if(!getMultiObjectCustom())
	{
		assert(0);
		return;
	}

#define FILL_CUBEMAP(filteredSize, radius, map) \
	if(map) \
	{ \
		const Interpolator* interpolator = cache.getInterpolator(gatherSize,filteredSize,radius); \
		interpolator->interpolate(gatheredIrradiance,filteredIrradiance,scaler); \
		map->setValues(filteredSize,filteredIrradiance); \
	}

	if(HDR)
	{
		// alloc temp space
		RRColorRGBF* gatheredIrradiance = new RRColorRGBF[6*gatherSize*gatherSize + 6*specularSize*specularSize + 6*diffuseSize*diffuseSize];
		RRColorRGBF* filteredIrradiance = gatheredIrradiance + 6*gatherSize*gatherSize;

		// gather irradiances
		cubeMapGather(scene,getMultiObjectCustom(),getScaler(),objectCenter,gatherSize,NULL,gatheredIrradiance);

		// fill cubemaps
		FILL_CUBEMAP(specularSize,0.1f,specularMap);
		FILL_CUBEMAP(diffuseSize,0.8f,diffuseMap);

		// cleanup
		delete[] gatheredIrradiance;
	}
	else
	{
		// alloc temp space
		RRColorRGBA8* gatheredIrradiance = new RRColorRGBA8[6*gatherSize*gatherSize + 6*specularSize*specularSize + 6*diffuseSize*diffuseSize];
		RRColorRGBA8* filteredIrradiance = gatheredIrradiance + 6*gatherSize*gatherSize;

		// gather irradiances
		cubeMapGather(scene,getMultiObjectCustom(),getScaler(),objectCenter,gatherSize,gatheredIrradiance,NULL);

		// fill cubemaps
		FILL_CUBEMAP(specularSize,0.1f,specularMap);
		FILL_CUBEMAP(diffuseSize,0.8f,diffuseMap);

		// cleanup
		delete[] gatheredIrradiance;
	}
}

} // namespace
