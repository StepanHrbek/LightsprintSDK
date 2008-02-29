// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>

#include "../RRMesh/RRMeshFilter.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRObject.h"
#include "RRObjectFilter.h"
#include "RRObjectFilterTransformed.h"
#include "RRObjectMulti.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out) const
{
	LIMITED_TIMES(1,RRReporter::report(WARN,"Slow RRObject::getPointMaterial path used, but no additional details are available."));
	const RRMaterial* material = getTriangleMaterial(t,NULL,NULL);
	if(material)
	{
		out = *material;
	}
	else
	{
		LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial returned NULL."));
		out.reset(false);
		RR_ASSERT(0);
	}
}

void RRObject::getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRVec3& out) const
{
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
}

const RRMatrix3x4* RRObject::getWorldMatrix()
{
	return NULL;
}

const RRMatrix3x4* RRObject::getInvWorldMatrix()
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject instance factory

RRMesh* RRObject::createWorldSpaceMesh()
{
	return this ? getCollider()->getMesh()->createTransformed(getWorldMatrix()) : NULL;
}

RRObject* RRObject::createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation)
{
	//!!! az bude refcounting, muzu pri identite vracet this
	return new RRTransformedObjectFilter(this,negScaleMakesOuterInner,intersectTechnique,cacheLocation);
}

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float vertexWeldDistance, bool optimizeTriangles, bool accelerate, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,vertexWeldDistance,optimizeTriangles,accelerate,cacheLocation);
}

} // namespace
