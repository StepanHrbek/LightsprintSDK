#ifndef _WORLD2RRINTERSECT_H
#define _WORLD2RRINTERSECT_H

#include "World.h"
#include "RREngine.h"

//////////////////////////////////////////////////////////////////////////////
//
// WorldObjectImporter

class WorldObjectImporter : virtual public rrIntersect::RRObjectImporter
{
public:
	WorldObjectImporter(OBJECT* aobject);
	virtual ~WorldObjectImporter();
	
	// must not change during object lifetime
	virtual unsigned     getNumVertices();
	virtual float*       getVertex(unsigned i);
	virtual unsigned     getNumTriangles();
	virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2, unsigned& si);
	virtual void         getTriangleSRL(unsigned i, TriangleSRL* t);

protected:
	OBJECT* object;
};

#endif
