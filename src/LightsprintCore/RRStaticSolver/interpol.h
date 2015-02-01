// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Smoothing of per-triangle illumination to per-vertex.
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
	Channels irradiance(RRRadiometricMeasure measure); // only direct+indirect is used

	const Corner& getCorner(unsigned c) const;
	Corner& getCorner(unsigned c);

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

		real     powerTopLevel;

		unsigned packedIndex; // index ivertexu v PackedSmoothing
};

} // namespace

#endif

