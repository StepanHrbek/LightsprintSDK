// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - with multiple objects.
// Copyright (c) 2000-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "IntersectLinear.h"
#include "Lightsprint/RRObject.h"
#include <vector>

namespace rr
{

class StoreCollisionHandler : public RRCollisionHandler
{
public:
	StoreCollisionHandler(RayHits& _rayHits)
		: rayHits(_rayHits)
	{
	}
	virtual void init(RRRay* ray)
	{
	}
	virtual bool collides(const RRRay* ray)
	{
		RRReal oldDistance = ray->hitDistance;
		const_cast<RRRay*>(ray)->hitDistance /= scale; // we change hitDistance temporarily, return it back two lines below, so no harm is done
		rayHits.insertHitUnordered(ray);
		const_cast<RRRay*>(ray)->hitDistance = oldDistance;
		return false;
	}
	virtual bool done()
	{
		return false;
	}
	RRReal scale;
private:
	RayHits& rayHits;
};

// Stores inverse matrices, needs rebuild each time object moves (only to update inverse matrices).
// Q: How about storing inverse matrices in objects?
// A: + supercollider would work without rebuild even if objects move (simplification, most likely small speedup)
//    - by not rebuilding supercollider, we would close path to more clever collider with kd
class RRColliderMulti : public RRCollider
{
public:
	RRColliderMulti(const RRObjects& _objects, IntersectTechnique _technique, bool& _aborting)//, const char* cacheLocation, void* buildParams)
	{
		objects = _objects;
		inverseMatrix = new RRMatrix3x4[_objects.size()];

		bool disabledReporting = false;
		bool b1,b3;
		unsigned b2;

#pragma omp parallel for schedule(dynamic)
		for (int i=0;i<(int)objects.size();i++)
		{
			if (objects[i]->getCollider()->getTechnique()==RRCollider::IT_LINEAR)
			{
				// disable reporting when working with thousands of small colliders, especially windowed reporter is too slow
				if (!disabledReporting && objects.size()>100)
				{
					disabledReporting = true;
					RRReporter::report(INF2,"Updating multicollider for %d objects...\n",objects.size());
					RRReporter::getFilter(b1,b2,b3);
					RRReporter::setFilter(true,1,false);
				}
				objects[i]->getCollider()->setTechnique(_technique,_aborting);//,cacheLocation,buildParams);
			}
			objects[i]->getWorldMatrixRef().invertedTo(inverseMatrix[i]);
		}

		// restore reporting
		if (disabledReporting)
		{
			RRReporter::setFilter(b1,b2,b3);
		}
	}

	virtual bool intersect(RRRay* ray) const
	{
		RRCollisionHandler* oldCollisionHandler = ray->collisionHandler;
		if (oldCollisionHandler)
			oldCollisionHandler->init(ray);
		RayHits rayHits;
		RRVec3 rayOrigin = ray->rayOrigin;
		RRVec3 rayDir = ray->rayDir;
		RRReal rayLengthMin = ray->rayLengthMin;
		RRReal rayLengthMax = ray->rayLengthMax;
		StoreCollisionHandler storeCollisionHandler(rayHits);
		ray->collisionHandler = &storeCollisionHandler;
		for (unsigned i=0;i<objects.size();i++)
		{
			ray->hitObject = objects[i];
			const RRMatrix3x4* m = ray->hitObject->getWorldMatrix();
			if (m)
			{
				// slower: inverts matrices in every ray
				//RRMatrix3x4 inverseMatrix;
				//if (!m->invertedTo(inverseMatrix)) continue;
				// faster: uses matrices preinverted in ctor

				inverseMatrix[i].transformPosition(ray->rayOrigin);
				inverseMatrix[i].transformDirection(ray->rayDir);
				storeCollisionHandler.scale = ray->rayDir.length();
				ray->rayDir /= storeCollisionHandler.scale;
				ray->rayLengthMin *= storeCollisionHandler.scale;
				ray->rayLengthMax *= storeCollisionHandler.scale;
			}
			else
				storeCollisionHandler.scale = 1;
			ray->hitObject->getCollider()->intersect(ray);
			if (m)
			{
				ray->rayOrigin = rayOrigin;
				ray->rayDir = rayDir;
				ray->rayLengthMin = rayLengthMin;
				ray->rayLengthMax = rayLengthMax;
			}
		}
		ray->collisionHandler = oldCollisionHandler;
		bool hitAcceptedByHandler = rayHits.getHitOrdered(ray,NULL);
		if (oldCollisionHandler)
			hitAcceptedByHandler = oldCollisionHandler->done();
		return hitAcceptedByHandler;
	}

	virtual const RRMesh* getMesh() const
	{
		return NULL;
	}

	virtual IntersectTechnique getTechnique() const
	{
		return objects.size() ? objects[0]->getCollider()->getTechnique() : IT_LINEAR;
	}

	virtual unsigned getMemoryOccupied() const
	{
		return sizeof(*this);
	}

	virtual ~RRColliderMulti()
	{
		delete[] inverseMatrix;
	}
private:
	RRObjects objects;
	RRMatrix3x4* inverseMatrix;
};

RRCollider* createMultiCollider(const RRObjects& objects, RRCollider::IntersectTechnique technique, bool& aborting)
{
	return new RRColliderMulti(objects,technique,aborting);
}

} // namespace
