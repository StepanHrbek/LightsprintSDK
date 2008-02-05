// --------------------------------------------------------------------------
// Smoothing.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef RRVISION_INTERPOL_H
#define RRVISION_INTERPOL_H

namespace rr
{

/*
INTERPOL_BETWEEN tells if it's good idea to interpolate between two triangles
 when to not interpol?
   obsoleted: different surface
     was good in time when exitances were interpolated. now we interpolate irradiances
   obsoleted: (angle too big) * (areas of too different size)
     had following explanation:
     "If we interpolate between areas of too different size, small dark tri + large lit tri would
     go both to grey which makes scene much darker than it should be."
     Now problem is solved by power*=node->area which means that smaller face makes smaller 
	 contribution to ivertex color.
   used: (angle too big)
*/
// ver1 #define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=MAX_INTERPOL_ANGLE && t1->grandpa->surface==t2->grandpa->surface)
// ver2 #define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=(MIN(t1->area,t2->area)/(t1->area+t2->area)*2+0.2f)*MAX_INTERPOL_ANGLE)
// all #define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=/*(MIN(t1->area,t2->area)/(t1->area+t2->area)*2+0.2f)**/MAX_INTERPOL_ANGLE /*&& t1->grandpa->surface==t2->grandpa->surface*/)
#define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=MAX_INTERPOL_ANGLE)
#define INTERPOL_BETWEEN(t1,t2)         INTERPOL_BETWEEN_A(t1,t2,angleBetweenNormalized(t1->getN3(),t2->getN3()))

#define MAX_INTERPOL_ANGLE maxSmoothAngle // max angle between interpolated neighbours


//////////////////////////////////////////////////////////////////////////////
//
// interpolated vertex

extern unsigned __iverticesAllocated;
extern unsigned __cornersAllocated;

class Triangle;
class Object;

struct Corner
{
	Triangle *node;
	real power; // libovolne cislo, vaha corneru (soucet vah neni 1, je ulozen v powerTopLevel)
	// nikdo nesmi zaviset na absolutni hodnote power, vzdy je to jen vaha vuci osttanim cornerum
	// ciste pro zajimavost ale absolutni hodnoty jsou:
	//   subdivisionSpeed==0 -> power je velikost uhlu * node->area     (lepe interpoluje, ale neumi subtriangly)
	//   subdivisionSpeed!=0 -> power je velikost uhlu
};

class IVertex
{
public:
	IVertex();
	~IVertex();

	void    insert(Triangle* node,bool toplevel,real power,Point3 apoint=Point3(0,0,0));
	bool    contains(Triangle* node);
	unsigned splitTopLevelByAngleOld(RRVec3 *avertex, Object *obj, float maxSmoothAngle);
	unsigned splitTopLevelByAngleNew(RRVec3 *avertex, Object *obj, float maxSmoothAngle);
	unsigned splitTopLevelByNormals(RRVec3 *avertex, Object *obj);
	Channels irradiance(RRRadiometricMeasure measure); // only direct+indirect is used

	// used by: merge close ivertices
	void    fillInfo(Object* object, unsigned originalVertexIndex, struct IVertexInfo& info);
	void    absorb(IVertex* aivertex);
	unsigned getNumCorners() {return corners;}

	// used by: triangledata -> vertexdata smoothing
	RRVec3  getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride, class Triangle* triangles, unsigned numTriangles) const;

	private:
		friend class Scene;
		friend class Object; // previousAllocBlock

		U8       cacheTime:5; // fix __frameNumber&0x1f if you change :5
		U8       cacheValid:1;
		U8       cacheDirect:1;
		U8       cacheIndirect:1;
		U8       cornersAllocatedLn2;
		U16      corners;
		unsigned cornersAllocated();
		Corner   *corner; // pole corneru tvoricich tento ivertex
		Channels cache;	// cached irradiance

		union
		{
			real     powerTopLevel;
			IVertex* previousAllocBlock; // used by newIVertex() allocator, must be 
		};

		unsigned packedIndex; // night edition: index ivertexu v PackedSmoothing
};

} // namespace

#endif

