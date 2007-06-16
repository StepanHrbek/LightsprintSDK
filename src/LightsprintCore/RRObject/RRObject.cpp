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

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,maxStitchDistance,optimizeTriangles,cacheLocation);
}

RRObjectWithIllumination* RRObject::createObjectWithIllumination(const RRScaler* scaler)
{
	return new RRObjectWithIlluminationImpl(this,scaler);
}

//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandler

class RRCollisionHandlerFirstVisible : public RRCollisionHandler
{
public:
	RRCollisionHandlerFirstVisible(RRObject* aobject)
	{
		object = aobject;
	}
	virtual void init()
	{
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		const RRMaterial* material = object->getTriangleMaterial(ray->hitTriangle);
		return material && material->sideBits[ray->hitFrontSide?0:1].renderFrom;
	}
	virtual bool done()
	{
		return result;
	}
private:
	bool result;
	RRObject* object;
};

RRCollisionHandler* RRObject::createCollisionHandlerFirstVisible()
{
	return new RRCollisionHandlerFirstVisible(this);
}

} // namespace
