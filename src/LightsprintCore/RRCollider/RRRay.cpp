//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Ray for ray-mesh intersections.
// --------------------------------------------------------------------------

#include "Lightsprint/RRCollider.h"
#include "config.h" // USE_SSE
#include "Lightsprint/RRSolver.h"

#include <cstring>

namespace rr
{

RRRay::RRRay()
{
	memset(this,0,sizeof(RRRay)); // no virtuals in RRRay -> no pointer to virtual function table overwritten
	//memset(((char*)this)+4,0,sizeof(RRRay)-4); // virtuals in RRRay -> avoid overwriting pointer to virtual function table
	rayOrigin[3] = 1;
	rayFlags = FILL_DISTANCE | FILL_POINT3D | FILL_POINT2D | FILL_PLANE | FILL_TRIANGLE | FILL_SIDE;
}

RRRay* RRRay::create()
{
	return new RRRay();
}

RRRay* RRRay::create(unsigned n)
{
#ifdef USE_SSE
	RR_ASSERT(!(sizeof(RRRay)%16));
#endif
	return new RRRay[n]();
}

bool RRRay::convertHitFromMultiToSingleObject(RRSolver* solver)
{
	if (!solver || !hitObject || hitObject!=solver->getMultiObject() || hitTriangle==UINT_MAX)
		return false;
	if (!(rayFlags&FILL_TRIANGLE))
	{
		RRReporter::report(WARN,"convertHitFromMultiToSingleObject: FILL_TRAINGLE in rayFlags missing.\n");
		return false;
	}
	RRMesh::PreImportNumber preImport = hitObject->getCollider()->getMesh()->getPreImportTriangle(hitTriangle);
	if (preImport.object>=solver->getStaticObjects().size())
	{
		RRReporter::report(ERRO,"convertHitFromMultiToSingleObject: invalid inputs.\n");
		return false;
	}
	hitTriangle = preImport.index;
	hitObject = solver->getStaticObjects()[preImport.object];
	return true;
}

} //namespace
