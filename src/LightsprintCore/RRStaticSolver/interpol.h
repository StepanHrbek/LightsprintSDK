// --------------------------------------------------------------------------
// Smoothing.
// Copyright (c) 2000-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef RRVISION_INTERPOL_H
#define RRVISION_INTERPOL_H

namespace rr
{

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
	real power;
};

class IVertex
{
public:
	IVertex();
	~IVertex();

	void    insert(Triangle* node,bool toplevel,real power);
	bool    contains(Triangle* node);
	unsigned splitTopLevelByAngleNew(RRVec3 *avertex, Object *obj, float maxSmoothAngle, bool& outOfMemory);
	Channels irradiance(RRRadiometricMeasure measure); // only direct+indirect is used

	const Corner& getCorner(unsigned c) const;
	Corner& getCorner(unsigned c);

	// used by: merge close ivertices
	void    fillInfo(Object* object, unsigned originalVertexIndex, struct IVertexInfo& info);
	void    absorb(IVertex* aivertex);
	unsigned getNumCorners() {return corners;}

	// used by: triangledata -> vertexdata smoothing
	RRVec3  getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride, class Triangle* triangles, unsigned numTriangles) const;

	private:
		friend class Scene;
		friend class Object; // previousAllocBlock

		U16      cacheTime          :5; // fix __frameNumber&0x1f if you change :5
		U16      cacheValid         :1;
		U16      cacheDirect        :1;
		U16      cacheIndirect      :1;
		U16      cornersAllocatedLn2:8;
		U16      corners; // static+dynamic
		unsigned cornersAllocated(); // static+dynamic
		enum
		{
			STATIC_CORNERS_LN2 = 3,
			STATIC_CORNERS     = 1<<STATIC_CORNERS_LN2,
		};
		Corner   staticCorner[STATIC_CORNERS];
		Corner*  dynamicCorner;
		Channels cache;	// cached irradiance

		union
		{
			real     powerTopLevel;
			IVertex* previousAllocBlock; // used by newIVertex() allocator 
		};

		unsigned packedIndex; // index ivertexu v PackedSmoothing
};

} // namespace

#endif

