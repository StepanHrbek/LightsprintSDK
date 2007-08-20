#include <cassert>

#include "../RRMesh/RRMeshFilter.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRObject.h"
#include "RRObjectFilter.h"
#include "RRMeshFilterTransformed.h"
#include "RRObjectFilterTransformed.h"
#include "RRObjectMulti.h"
#include "RRObjectWithIllumination.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out) const
{
	LIMITED_TIMES(1,RRReporter::report(WARN,"Slow RRObject::getPointMaterial path used, but no additional details are available."));
	const RRMaterial* material = getTriangleMaterial(t);
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

void RRObject::getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
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
	//!!! az bude refcounting, muzu pri identite vracet getCollider()->getMesh()
	return new RRTransformedMeshFilter(getCollider()->getMesh(),getWorldMatrix());
}

RRObject* RRObject::createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation)
{
	//!!! az bude refcounting, muzu pri identite vracet this
	return new RRTransformedObjectFilter(this,negScaleMakesOuterInner,intersectTechnique,cacheLocation);
}

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float vertexWeldDistance, bool optimizeTriangles, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,vertexWeldDistance,optimizeTriangles,cacheLocation);
}

RRObjectWithIllumination* RRObject::createObjectWithIllumination(const RRScaler* scaler)
{
	return new RRObjectWithIlluminationImpl(this,scaler);
}

} // namespace
