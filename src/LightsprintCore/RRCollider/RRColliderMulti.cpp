//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Ray-mesh intersection traversal - with multiple objects.
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
	virtual void init(RRRay& ray)
	{
	}
	virtual bool collides(const RRRay& ray)
	{
		RRReal oldDistance = ray.hitDistance;
		const_cast<RRRay*>(&ray)->hitDistance /= scale; // we change hitDistance temporarily, return it back two lines below, so no harm is done
		rayHits.insertHitUnordered(ray);
		const_cast<RRRay*>(&ray)->hitDistance = oldDistance;
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

// Reads matrices from objects, does not need rebuild each time object moves.
// (However, more clever supercollider with KD would need rebuilding supercollider [#21])
class RRColliderMulti : public RRCollider
{
public:
	RRColliderMulti(const RRObjects& _objects, IntersectTechnique _technique, bool& _aborting)//, const char* cacheLocation, void* buildParams)
	{
		objects = _objects;

		bool disabledReporting = false;
		bool b1,b3;
		unsigned b2;

		RRTime start;
// embree must not be called in parallel, it would crash in glut based samples with probability=0.15 (surprisingly i did not see it crash in sceneviewer)
// (embree is parallelized internally, #pragma would speed up only IT_BSP_ colliders but no one uses them, right?)
//#pragma omp parallel for schedule(dynamic)
		for (int i=0;i<(int)objects.size();i++)
		{
			if (objects[i]->getCollider()->getTechnique()==RRCollider::IT_LINEAR)
			{
				// disable reporting when working with thousands of small colliders, especially windowed reporter is too slow
				if (!disabledReporting && objects.size()>100)
				{
					disabledReporting = true;
					RRReporter::report(INF2,"Updating multicollider for %" RR_SIZE_T "d objects...\n",objects.size());
					RRReporter::getFilter(b1,b2,b3);
					RRReporter::setFilter(true,1,false);
				}
				objects[i]->getCollider()->setTechnique(_technique,_aborting);//,cacheLocation,buildParams);
			}
		}

		// restore reporting
		if (disabledReporting)
		{
			RRReporter::setFilter(b1,b2,b3);
			RRReporter::report(TIMI,"  done in %.1fs\n",start.secondsPassed());
		}
	}

	virtual bool intersect(RRRay& ray) const
	{
		RRCollisionHandler* oldCollisionHandler = ray.collisionHandler;
		if (oldCollisionHandler)
			oldCollisionHandler->init(ray);
		RayHits rayHits;
		RRVec3 rayOrigin = ray.rayOrigin;
		RRVec3 rayDir = ray.rayDir;
		RRReal rayLengthMin = ray.rayLengthMin;
		RRReal rayLengthMax = ray.rayLengthMax;
		StoreCollisionHandler storeCollisionHandler(rayHits);
		ray.collisionHandler = &storeCollisionHandler;
		for (unsigned i=0;i<objects.size();i++)
		{
			if (objects[i]->enabled)
			{
				ray.hitObject = objects[i];
				const RRMatrix3x4Ex* m = ray.hitObject->getWorldMatrix();
				if (m)
				{
					m->inverse.transformPosition(ray.rayOrigin);
					m->inverse.transformDirection(ray.rayDir);
					storeCollisionHandler.scale = ray.rayDir.length();
					ray.rayDir /= storeCollisionHandler.scale;
					ray.rayLengthMin *= storeCollisionHandler.scale;
					ray.rayLengthMax *= storeCollisionHandler.scale;
				}
				else
					storeCollisionHandler.scale = 1;
				ray.hitObject->getCollider()->intersect(ray);
				if (m)
				{
					ray.rayOrigin = rayOrigin;
					ray.rayDir = rayDir;
					ray.rayLengthMin = rayLengthMin;
					ray.rayLengthMax = rayLengthMax;
				}
			}
		}
		ray.collisionHandler = oldCollisionHandler;
		bool hitAcceptedByHandler = rayHits.getHitOrdered(ray,nullptr);
		if (oldCollisionHandler)
			hitAcceptedByHandler = oldCollisionHandler->done();
		return hitAcceptedByHandler;
	}

	virtual const RRMesh* getMesh() const
	{
		return nullptr;
	}

	virtual IntersectTechnique getTechnique() const
	{
		return objects.size() ? objects[0]->getCollider()->getTechnique() : IT_LINEAR;
	}

	virtual size_t getMemoryOccupied() const
	{
		return sizeof(*this);
	}
private:
	RRObjects objects;
};

RRCollider* createMultiCollider(const RRObjects& objects, RRCollider::IntersectTechnique technique, bool& aborting)
{
	return new RRColliderMulti(objects,technique,aborting);
}

} // namespace
