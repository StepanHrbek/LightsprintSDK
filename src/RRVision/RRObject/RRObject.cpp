#include <cassert>

#include "../../RRMesh/RRMeshFilter.h"
#include "../rrcore.h"
#include "RRVision.h"
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

void RRObject::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	unsigned numTriangles = getCollider()->getMesh()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}
	RRMesh::TriangleBody tb;
	getCollider()->getMesh()->getTriangleBody(t,tb);
	Vec3 norm = ortogonalTo(tb.side1,tb.side2);
	norm *= 1/size(norm);
	out.norm[0] = norm;
	out.norm[1] = norm;
	out.norm[2] = norm;
}

void RRObject::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	unsigned numTriangles = getCollider()->getMesh()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}

	// row with 50% fill |/|/|/|/ 1234
	//out.uv[0][0] = 1.0f*t/numTriangles;
	//out.uv[0][1] = 0;
	//out.uv[1][0] = 1.0f*(t+1)/numTriangles;
	//out.uv[1][1] = 0;
	//out.uv[2][0] = 1.0f*t/numTriangles;
	//out.uv[2][1] = 1;

	// matrix with 50% fill  |/|/|/|/  1234
	//                       |/|/|/|/  5678
	//unsigned w = (unsigned)sqrtf((float)(numTriangles-1))+1;
	//unsigned h = (numTriangles-1)/w+1;
	//unsigned x = t%w;
	//unsigned y = t/w;
	//out.uv[0][0] = 1.0f*x/w;
	//out.uv[0][1] = 1.0f*y/h;
	//out.uv[1][0] = 1.0f*(x+1)/w;
	//out.uv[1][1] = 1.0f*y/h;
	//out.uv[2][0] = 1.0f*x/w;
	//out.uv[2][1] = 1.0f*(y+1)/h;

	// matrix with 90% fill |//||//||//||//| 12345678
	//                      |//||//||//||//| 9abcdefg
	unsigned spin = t&1;
	t /= 2;
	unsigned w = (unsigned)sqrtf((float)((numTriangles+1)/2-1))+1;
	unsigned h = ((numTriangles+1)/2-1)/w+1;
	unsigned x = t%w;
	unsigned y = t/w;
	static const float border = 0.1f;
	if(!spin)
	{
		out.uv[0][0] = (x+border)/w;
		out.uv[0][1] = (y+border)/h;
		out.uv[1][0] = (x+1-2.4f*border)/w;
		out.uv[1][1] = (y+border)/h;
		out.uv[2][0] = (x+border)/w;
		out.uv[2][1] = (y+1-2.4f*border)/h;
	}
	else
	{
		out.uv[0][0] = (x+1-border)/w;
		out.uv[0][1] = (y+1-border)/h;
		out.uv[1][0] = (x+2.4f*border)/w;
		out.uv[1][1] = (y+1-border)/h;
		out.uv[2][0] = (x+1-border)/w;
		out.uv[2][1] = (y+2.4f*border)/h;
	}
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
