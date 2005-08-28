#ifndef RRINTERSECT_INTERSECTBSPFAST_H
#define RRINTERSECT_INTERSECTBSPFAST_H

#include "IntersectBsp.h"

namespace rrIntersect
{

	/*struct TriangleP
	{
		real    intersectReal;  // precalculated number for intersections
		char    intersectByte;  // precalculated number for intersections, 0..8
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};*/

	struct TriangleNP
	{
		Plane   n3;             // triangle plane, first three components form normalised normal
		real    intersectReal;  // precalculated number for intersections
		char    intersectByte;  // precalculated number for intersections, 0..8
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};

	struct TriangleSRLNP : public RRAligned
	{
		// all vectors are aligned at 16byte boundaries for SSE
		Plane   n3;             // triangle plane, first three components form normalised normal
		Vec3    s3;             // vertex[0]
		real    intersectReal;  // precalculated number for intersections
		Vec3    r3;             // vertex[1]-vertex[0]
		char    intersectByte;  // precalculated number for intersections, 0..8
		char    pad1[3];
		Vec3    l3;             // vertex[2]-vertex[0]
		real    pad2;
		void    setGeometry(const Vec3* a, const Vec3* b, const Vec3* c);
	};

	template IBP
	class IntersectBspFast : public IntersectLinear
	{
	public:
		static IntersectBspFast* create(RRMeshImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams) {return new IntersectBspFast(aimporter,aintersectTechnique,ext,buildParams);}
		virtual ~IntersectBspFast();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual IntersectTechnique getTechnique() const {return intersectTechnique;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectBspFast(RRMeshImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams);
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
