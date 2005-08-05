#ifndef RRINTERSECT_INTERSECTBSPFAST_H
#define RRINTERSECT_INTERSECTBSPFAST_H

#include "IntersectBsp.h"

namespace rrIntersect
{

	struct TriangleP
	{
		real    intersectReal;  // precalculated number for intersections
		char    intersectByte;  // precalculated number for intersections, 0..8
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};

	struct TriangleNP : public TriangleP
	{
		Plane   n3;             // normalised normal vector
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};

	struct TriangleSRLNP : public TriangleNP
	{
		Vec3    s3;             // absolute position of start of base
		Vec3    r3,l3;          // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0]
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};

	template IBP
	class IntersectBspFast : public IntersectLinear
	{
	public:
		static IntersectBspFast* create(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams) {return new IntersectBspFast(aimporter,aintersectTechnique,ext,buildParams);}
		virtual ~IntersectBspFast();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual unsigned  getMemorySize() const;
	protected:
		IntersectBspFast(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams);
		bool              intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const;
		bool              intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const;
		BspTree*          tree;
		TriangleNP*       triangleNP;
		TriangleSRLNP*    triangleSRLNP;
		IntersectTechnique intersectTechnique;
#ifdef TEST
		IntersectLinear*  test;
#endif
	};

}

#endif
