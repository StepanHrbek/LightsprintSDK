#ifndef _RRINTERSECT_H
#define _RRINTERSECT_H

#pragma comment(lib,"RRIntersect.lib")

#define USE_BSP
//#define USE_KD

typedef unsigned OBJECT_HANDLE;
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
	// when fastXXX is set, slower but much lower memory footprint Intersect may be used
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
	float           ex,ey,ez;
	float           dx,dy,dz;
	float           distanceMin,distanceMax;
	TRIANGLE_HANDLE skip;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRRay - hit from intersection.

struct RRHit
{
	float           x,y,z;
	float           u,v;
	float           distance;
	bool            outerSide;
	TRIANGLE_HANDLE triangle;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRIntersect - single object able to calculate intersections.

class RRIntersect
{
public:
	//virtual ~Intersect() = 0;
	virtual bool intersect(RRRay* ray, RRHit* hit) = 0;
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

#endif
