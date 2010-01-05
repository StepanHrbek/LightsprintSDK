// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - verificator.
// Copyright (c) 2000-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_INTERSECTVERIFICATION_H
#define COLLIDER_INTERSECTVERIFICATION_H

#include "IntersectLinear.h"

namespace rr
{

	class IntersectVerification : public IntersectLinear
	{
	public:
		static IntersectVerification* create(const RRMesh* aimporter, bool& aborting) {return new IntersectVerification(aimporter,aborting);}
		virtual ~IntersectVerification();
		virtual bool      intersect(RRRay* ray) const;
		virtual IntersectTechnique getTechnique() const {return IT_VERIFICATION;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectVerification(const RRMesh* aimporter, bool& aborting);
		const RRCollider* collider[IT_VERIFICATION];
	};

}

#endif
