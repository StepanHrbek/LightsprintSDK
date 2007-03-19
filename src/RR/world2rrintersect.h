#ifndef _WORLD2COLLIDER_H
#define _WORLD2COLLIDER_H

#include "World.h"
#include "Lightsprint/RRVision.h"

//////////////////////////////////////////////////////////////////////////////
//
// WorldMeshImporter

class WorldMeshImporter : virtual public rr::RRMesh
{
public:
	WorldMeshImporter(OBJECT* aobject);
	virtual ~WorldMeshImporter();
	
	// must not change during object lifetime
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned i, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned i, Triangle& out) const;
	virtual void         getTriangleBody(unsigned i, TriangleBody& out) const;

protected:
	OBJECT* object;
};

#endif
