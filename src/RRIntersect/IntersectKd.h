#ifndef _INTERSECTKD_H
#define _INTERSECTKD_H

#include "IntersectLinear.h"

namespace rrIntersect
{

	class IntersectKd : public IntersectLinear
	{
	public:
		IntersectKd(RRObjectImporter* aimporter);
		~IntersectKd();
		virtual bool      intersect(RRRay* ray, RRHit* hit);
	protected:
		void*             tree;
	};

}

#endif
