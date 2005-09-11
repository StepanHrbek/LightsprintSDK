#ifndef _WORLD2COLLIDER_H
#define _WORLD2COLLIDER_H

#include "World.h"
#include "RREngine.h"

//////////////////////////////////////////////////////////////////////////////
//
// WorldMeshImporter

class WorldMeshImporter : virtual public rrCollider::RRMeshImporter
{
public:
	WorldMeshImporter(OBJECT* aobject);
	virtual ~WorldMeshImporter();
	
	// must not change during object lifetime
	virtual unsigned     getNumVertices() const;
	virtual float*       getVertex(unsigned i) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2) const;
	virtual void         getTriangleSRL(unsigned i, TriangleSRL* t) const;

protected:
	OBJECT* object;
};

#endif
