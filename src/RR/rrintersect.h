#ifndef _RRINTERSECT_H
#define _RRINTERSECT_H

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

	// may change during object lifetime
	virtual const float* getWorldMatrix() = 0;
	virtual const float* getInvWorldMatrix() = 0;
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
// RRObject - single object able to calculate intersections.

class RRObject
{
public:
	RRObject(RRObjectImporter* aimporter);
	~RRObject();
	
	bool            intersect(RRRay* ray, RRHit* hit);

private:
	void*           hook;
};

#endif
