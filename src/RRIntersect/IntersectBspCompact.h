#ifndef RRINTERSECT_INTERSECTBSPCOMPACT_H
#define RRINTERSECT_INTERSECTBSPCOMPACT_H

#include "IntersectBsp.h"

namespace rrIntersect
{
	template IBP
	class IntersectBspCompact : public IntersectLinear
	{
	public:
		static IntersectBspCompact* create(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams) {return new IntersectBspCompact(aimporter,aintersectTechnique,ext,buildParams);}
		virtual ~IntersectBspCompact();
		virtual bool      intersect(RRRay* ray) const;
		virtual unsigned  getMemorySize() const;
		// must be public because it calls itself with different template params
		bool              intersect_bsp(RRRay* ray, const BspTree *t, real distanceMax) const; 
	protected:
		IntersectBspCompact(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams);
		BspTree*          tree;
	};

}

#endif
