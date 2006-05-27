#include <assert.h>

#include "../../RRCollider/RRMesh/RRMeshFilter.h"
#include "../rrcore.h"
#include "RRVision.h"
#include "RRObjectFilter.h"
#include "RRMeshFilterTransformed.h"
#include "RRObjectFilterTransformed.h"
#include "RRObjectMulti.h"
#include "RRObjectFilterAdditionalMeasure.h"

namespace rrVision
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

void RRObject::getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
{
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
}

void RRObject::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	unsigned numTriangles = getCollider()->getImporter()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}
	rrCollider::RRMesh::TriangleBody tb;
	getCollider()->getImporter()->getTriangleBody(t,tb);
	Vec3 norm = ortogonalTo(tb.side1,tb.side2);
	norm *= 1/size(norm);
	out.norm[0] = norm;
	out.norm[1] = norm;
	out.norm[2] = norm;
}

void RRObject::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	unsigned numTriangles = getCollider()->getImporter()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}
	out.uv[0][0] = 1.0f*t/numTriangles;
	out.uv[0][1] = 0;
	out.uv[1][0] = 1.0f*(t+1)/numTriangles;
	out.uv[1][1] = 0;
	out.uv[2][0] = 1.0f*t/numTriangles;
	out.uv[2][1] = 1;
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

rrCollider::RRMesh* RRObject::createWorldSpaceMesh()
{
	//!!! az bude refcounting, muzu vracet getCollider()->getImporter()
	return new RRTransformedMeshFilter(getCollider()->getImporter(),getWorldMatrix());
}

RRObject* RRObject::createWorldSpaceObject(rrCollider::RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation)
{
	//!!! az bude refcounting, muzu vracet this
	return new RRTransformedObjectFilter(this,intersectTechnique);
}

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,maxStitchDistance,optimizeTriangles,cacheLocation);
}

RRObjectAdditionalIllumination* RRObject::createAdditionalIllumination()
{
	return new RRObjectAdditionalIlluminationImpl(this);
}

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectSurfaceImporter

class RRAcceptFirstVisibleSurfaceImporter : public rrCollider::RRMeshSurfaceImporter
{
public:
	RRAcceptFirstVisibleSurfaceImporter(RRObject* aobject)
	{
		object = aobject;
	}
	virtual bool acceptHit(const rrCollider::RRRay* ray)
	{
		const RRSurface* surface = object->getSurface(object->getTriangleSurface(ray->hitTriangle));
		return surface && surface->sideBits[ray->hitFrontSide?0:1].renderFrom;
	}
private:
	RRObject* object;
};

rrCollider::RRMeshSurfaceImporter* RRObject::createAcceptFirstVisibleSurfaceImporter()
{
	return new RRAcceptFirstVisibleSurfaceImporter(this);
}

} // namespace
