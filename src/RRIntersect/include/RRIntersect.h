#ifndef RRINTERSECT_RRINTERSECT_H
#define RRINTERSECT_RRINTERSECT_H

namespace rrIntersect
{

	#ifdef _MSC_VER
	#pragma comment(lib,"RRIntersect.lib")
	#endif

	#define USE_BSP
	//#define USE_KD

	typedef unsigned TRIANGLE_HANDLE;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRObjectImporter - abstract class for importing your data into RRObject.
	//
	// Derive to import YOUR geometry.
	// Derive from RRSceneObjectImporter if you want to calculate also radiosity.

	class RRObjectImporter
	{
	public:
		// must not change during object lifetime
		virtual unsigned     getNumVertices() = 0;
		virtual void         getVertex(unsigned i, float& x, float& y, float& z) = 0;
		virtual unsigned     getNumTriangles() = 0;
		virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2, unsigned& s) = 0;

		// optional
		// if you implement fast getTriangleXXX, set fastXXX
		// -> slower but much lower memory footprint Intersect may be used
		RRObjectImporter();
		bool                 fastN   :1;
		bool                 fastSRL :1;
		bool                 fastSRLN:1;
		struct TriangleN     {float n[4];};
		struct TriangleSRL   {float s[3],r[3],l[3];};
		struct TriangleSRLN  {float s[3],r[3],l[3],n[4];};
		virtual void         getTriangleN(unsigned i, TriangleN* t);
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t);
		virtual void         getTriangleSRLN(unsigned i, TriangleSRLN* t);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.

	struct RRRay
	{
		float           rayOrigin[3];
		float           rayDir[3];
		float           hitDistanceMin,hitDistanceMax;
		float           hitPoint[3];
		float           hitU,hitV;
		float           hitDistance;
		TRIANGLE_HANDLE skip;
		TRIANGLE_HANDLE hitTriangle;
		bool            hitOuterSide;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersect - single object able to calculate intersections.

	class RRIntersect
	{
	public:
		//virtual ~Intersect() = 0;
		virtual bool intersect(RRRay* ray) = 0;
	};

	RRIntersect* newIntersect(RRObjectImporter* importer);


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRIntersectStats
	{
	public:
		RRIntersectStats();
		static unsigned shots;
		static unsigned bsp;
		static unsigned kd;
		static unsigned tri;
	};

	extern RRIntersectStats intersectStats;

}

#endif
