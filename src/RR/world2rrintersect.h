#ifndef _WORLD2RRINTERSECT_H
#define _WORLD2RRINTERSECT_H

#include "ldbsp.h"
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
	virtual void         getVertex(unsigned i, float& x, float& y, float& z);
	virtual unsigned     getNumTriangles();
	virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2, unsigned& si);

protected:
	OBJECT* object;
};

#endif
