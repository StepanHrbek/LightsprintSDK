#ifndef RRINTERSECT_INTERSECTKD_H
#define RRINTERSECT_INTERSECTKD_H

#include "IntersectLinear.h"

#ifdef USE_KD

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

#endif
