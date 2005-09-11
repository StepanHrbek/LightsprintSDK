#ifndef COLLIDER_INTERSECTVERIFICATION_H
#define COLLIDER_INTERSECTVERIFICATION_H

#include "IntersectLinear.h"

namespace rrCollider
{

	class IntersectVerification : public IntersectLinear
	{
	public:
		static IntersectVerification* create(RRMeshImporter* aimporter) {return new IntersectVerification(aimporter);}
		virtual ~IntersectVerification();
		virtual bool      intersect(RRRay* ray) const;
		virtual IntersectTechnique getTechnique() const {return IT_VERIFICATION;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectVerification(RRMeshImporter* aimporter);
		RRCollider*       collider[IT_VERIFICATION];
	};

}

#endif
