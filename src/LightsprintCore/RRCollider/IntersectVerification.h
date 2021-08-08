//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Ray-mesh intersection traversal - verificator.
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
		virtual void      update();
		virtual ~IntersectVerification();
		virtual bool      intersect(RRRay& ray) const;
		virtual IntersectTechnique getTechnique() const {return IT_VERIFICATION;}
		virtual size_t    getMemoryOccupied() const;
	protected:
		IntersectVerification(const RRMesh* aimporter, bool& aborting);
		RRCollider* collider[IT_VERIFICATION];
	};

}

#endif
