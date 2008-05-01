// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - verificator.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_INTERSECTVERIFICATION_H
#define COLLIDER_INTERSECTVERIFICATION_H

#include "IntersectLinear.h"

namespace rr
{

	class IntersectVerification : public IntersectLinear
	{
	public:
		static IntersectVerification* create(const RRMesh* aimporter) {return new IntersectVerification(aimporter);}
		virtual ~IntersectVerification();
		virtual bool      intersect(RRRay* ray) const;
		virtual IntersectTechnique getTechnique() const {return IT_VERIFICATION;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectVerification(const RRMesh* aimporter);
		const RRCollider* collider[IT_VERIFICATION];
	};

}

#endif
