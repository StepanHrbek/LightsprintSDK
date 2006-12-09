#include <cassert>
#include "RRRealtimeRadiosity.h"

#define HDR

namespace rr
{

#ifdef HDR
	// podporuje custom scaler (i odlisny od sceny)
	// korektni filtrovani v physical space
	// nikde se neclampuje
	typedef RRColorRGBF CubeColor;
#else
	// fixed scaler ze sceny
	// filtrovani je nekorektni, v custom space
	// clampuje se z custom space do 8bit
	typedef RRColorRGBA8 CubeColor;
#endif

// navaznost hran v cubemape:
// side0top   = side2right
// side0left  = side4right
// side0right = side5left
// side0bot   = side3right
// side1top   = side2left
// side1left  = side5right
// side1right = side4left
// side1bot   = side3left
// side2top   = side5top
// side2left  = side1top
// side2right = side0top
// side2bot   = side4top
// side3top   = side4bot
// side3left  = side1bot
// side3right = side0bot
// side3bot   = side5bot
// side4top   = side2bot
// side4left  = side1right
// side4right = side0left
// side4bot   = side3top
// side5top   = side2top
// side5left  = side0right
// side5right = side1left
// side5bot   = side3bot

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

	// returns nearest texel index (in format side*size*size+j*size+i) to x,y inside side reached from this via edge
	int getNeighbourTexelIndex(unsigned size,Edge edge, unsigned x,unsigned y) const;
};

static const CubeSide cubeSide[6] =
{
	// describes cubemap sides in opengl
	{{ 1, 0, 0}, {{2,RIGHT },{4,RIGHT },{5,LEFT  },{3,RIGHT }}},
	{{-1, 0, 0}, {{2,LEFT  },{5,RIGHT },{4,LEFT  },{3,LEFT  }}},
	{{ 0, 1, 0}, {{5,TOP   },{1,TOP   },{0,TOP   },{4,TOP   }}},
	{{ 0,-1, 0}, {{4,BOTTOM},{1,BOTTOM},{0,BOTTOM},{5,BOTTOM}}},
	{{ 0, 0, 1}, {{2,BOTTOM},{1,RIGHT },{0,LEFT  },{3,TOP   }}},
	{{ 0, 0,-1}, {{2,TOP   },{0,RIGHT },{1,LEFT  },{3,BOTTOM}}}
};

RRVec3 CubeSide::getTexelDir(RRVec2 uv) const
{
	return getDir()+cubeSide[neighbour[RIGHT].side].getDir()*uv[1]+cubeSide[neighbour[BOTTOM].side].getDir()*uv[0];
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

// inputs:
//  iSize - input cube size
//  iIrradiance - array with 6*iSize*iSize pixels of cube. should be in physical space, otherwise filtering is incorrect
// outputs:
//  scale: custom using scaler; physical if no scaler provided
//  a)
//   oSize - new filtered cube size
//   oIrradiance - new array with filtered cube, to be deleted by caller (in the same space, should be physical)
//  b)
//   *iIrradiance - modified values in input array
//  c)
//   no changes
// thread safe: yes
static void cubeMapFilter(unsigned iSize, CubeColor* iIrradiance, unsigned& oSize, CubeColor*& oIrradiance, const RRScaler* scaler)
{
	unsigned iPixels = iSize*iSize*6;

	// 1 -> 2, prumeruje rohy
	if(iSize==1)
	{
		// pri zmene cubeSide[6] nutno zmenit i tuto tabulku
		static const signed char filteringTable[] = {
			// side0
			1,0,1,0,1,0,
			1,0,1,0,0,1,
			1,0,0,1,1,0,
			1,0,0,1,0,1,
			// side1
			0,1,1,0,0,1,
			0,1,1,0,1,0,
			0,1,0,1,0,1,
			0,1,0,1,1,0,
			// side2
			0,1,1,0,0,1,
			1,0,1,0,0,1,
			0,1,1,0,1,0,
			1,0,1,0,1,0,
			// side3
			0,1,0,1,1,0,
			1,0,0,1,1,0,
			0,1,0,1,0,1,
			1,0,0,1,0,1,
			// side4
			0,1,1,0,1,0,
			1,0,1,0,1,0,
			0,1,0,1,1,0,
			1,0,0,1,1,0,
			// side5
			1,0,1,0,0,1,
			0,1,1,0,0,1,
			1,0,0,1,0,1,
			0,1,0,1,0,1,
		};
		oSize = 2;
		unsigned oPixels = 6*oSize*oSize;
		oIrradiance = new CubeColor[oPixels];
		for(unsigned i=0;i<oPixels;i++)
		{
			RRColorRGBF sum = RRColorRGBF(0);
			unsigned points = 0;
			for(unsigned j=0;j<iPixels;j++)
			{
				sum += iIrradiance[j].toRRColorRGBF() * filteringTable[iPixels*i+j];
				points += filteringTable[iPixels*i+j];
			}
			assert(points);
			oIrradiance[i] = sum / (points?points:1);
#ifdef HDR
			if(scaler) scaler->getCustomScale(oIrradiance[i]);
#endif
		}
	}

	// 2 -> 2, prumeruje rohy
	if(iSize==2)
	{
		static signed char filteringTable[24][3];
		static bool inited = false;
		if(!inited)
		{
			for(unsigned side=0;side<6;side++)
			{
				for(unsigned i=0;i<2;i++)
					for(unsigned j=0;j<2;j++)
					{
						filteringTable[4*side+2*j+i][0] = 4*side+2*j+i;
						filteringTable[4*side+2*j+i][1] = cubeSide[side].getNeighbourTexelIndex(2,i?RIGHT:LEFT,i,j);
						filteringTable[4*side+2*j+i][2] = cubeSide[side].getNeighbourTexelIndex(2,j?BOTTOM:TOP,i,j);
					}
			}
			inited=true;
		}
		oSize = 2;
		unsigned oPixels = 6*oSize*oSize;
		oIrradiance = new CubeColor[oPixels];
		for(unsigned i=0;i<oPixels;i++)
		{
			oIrradiance[i] = (iIrradiance[filteringTable[i][0]].toRRColorRGBF()+iIrradiance[filteringTable[i][1]].toRRColorRGBF()+iIrradiance[filteringTable[i][2]].toRRColorRGBF()) * 0.333333f;
#ifdef HDR
			if(scaler) scaler->getCustomScale(oIrradiance[i]);
#endif
		}
	}

	// n -> n, prumeruje 1-pix hrany a rohy
	if(iSize>2)
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
						default: assert(0);
					}
					unsigned idx1 = side*iSize*iSize+j*iSize+i;
					unsigned idx2 = cubeSide[side].getNeighbourTexelIndex(iSize,(Edge)edge,i,j);
					if(a==0)
					{
						// process corner (a+b+c)/3
						unsigned idx3 = cubeSide[side].getNeighbourTexelIndex(iSize,(Edge)edge2,i,j);
						iIrradiance[idx1] = iIrradiance[idx2] = iIrradiance[idx3] = (iIrradiance[idx1].toRRColorRGBF()+iIrradiance[idx2].toRRColorRGBF()+iIrradiance[idx3].toRRColorRGBF())*0.333333f;
					}
					else
					{
						// process edge: (a+b)/2
						iIrradiance[idx1] = iIrradiance[idx2] = (iIrradiance[idx1].toRRColorRGBF()+iIrradiance[idx2].toRRColorRGBF())*0.5f;
					}
				}
			}
		}
#ifdef HDR
		if(scaler)
		{
			for(unsigned i=0;i<iPixels;i++)
			{
				scaler->getCustomScale(iIrradiance[i]);
			}
		}
#endif
	}
}

// thread safe: yes
static void cubeMapGather(const RRScene* scene, const RRObject* object, const RRScaler* scaler, RRVec3 center, unsigned size, CubeColor* irradiance)
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
	if(!irradiance)
	{
		assert(0);
		return;
	}
	RRRay* ray = RRRay::create();
	for(unsigned side=0;side<6;side++)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
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
				// read irradiance on sky
				if(face==UINT_MAX)
				{
					*irradiance = RRVec3(0); //!!! add sky
				}
				else
				// read cube irradiance as face exitance
				{
#ifdef HDR
					scene->getTriangleMeasure(face,3,RM_EXITANCE_PHYSICAL,scaler,*irradiance);
#else
					RRVec3 irrad;
					scene->getTriangleMeasure(face,3,RM_EXITANCE_CUSTOM,scaler,irrad);
					*irradiance = irrad;
#endif
					// na pokusy: misto irradiance bere barvu materialu
					//const RRSurface* surface = object->getSurface(object->getTriangleSurface(face));
					//if(surface) *irradiance = surface->diffuseReflectance;
				}
				// go to next pixel
				irradiance++;
			}
	}
	delete ray;
}

// thread safe: yes
void RRRealtimeRadiosity::updateEnvironmentMap(RRIlluminationEnvironmentMap* environmentMap, unsigned maxSize, RRVec3 objectCenterWorld)
{
	if(!environmentMap) return;
	if(!scene)
	{
		assert(0);
		return;
	}
	if(!getMultiObject())
	{
		assert(0);
		return;
	}

	// gather irradiances
	const unsigned iSize = maxSize?maxSize:1;
	CubeColor* iIrradiance = new CubeColor[6*iSize*iSize];
	cubeMapGather(scene,getMultiObject(),getScaler(),objectCenterWorld,iSize,iIrradiance);

	// filter cubemap
	unsigned oSize = 0;
	CubeColor* oIrradiance = NULL;
	cubeMapFilter(iSize,iIrradiance,oSize,oIrradiance,getScaler());

	// pass cubemap to client
	environmentMap->setValues(oSize?oSize:iSize,oIrradiance?oIrradiance:iIrradiance);

	// cleanup
	delete oIrradiance;
	delete iIrradiance;
}

} // namespace
