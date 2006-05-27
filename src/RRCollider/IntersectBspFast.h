#ifndef COLLIDER_INTERSECTBSPFAST_H
#define COLLIDER_INTERSECTBSPFAST_H

#include "IntersectBsp.h"

namespace rrCollider
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

	struct TriangleSRLNP
	{
		// all vectors are aligned at 16byte boundaries for SSE
		Plane   n3;             // ALIGN16 triangle plane, first three components form normalised normal
		Vec3    s3;             // ALIGN16 vertex[0]
		real    intersectReal;  // precalculated number for intersections
		Vec3    r3;             // ALIGN16 vertex[1]-vertex[0]
		Vec3    ir3;            // ALIGN16 1/r3
		Vec3    l3;             // ALIGN16 vertex[2]-vertex[0]
		char    intersectByte;  // precalculated number for intersections, 0..8
		void    setGeometry(unsigned atriangleIdx, const Vec3* a, const Vec3* b, const Vec3* c);
		unsigned getTriangleIndex() const 
		{
			assert(0);
			return 0;
		}
	};

	// single-level bsp (FAST, FASTEST)
	/*typedef BspTree1<unsigned short,unsigned char ,void> BspTree21;
	typedef BspTree1<unsigned short,unsigned short,void> BspTree22;
	typedef BspTree1<unsigned int  ,unsigned short,void> BspTree42;*/
	typedef BspTree1<unsigned int  ,TriIndex<unsigned int  >,void> BspTree44;

	template IBP
	class IntersectBspFast : public IntersectLinear
	{
	public:
		static IntersectBspFast* create(RRMesh* aimporter, IntersectTechnique aintersectTechnique, const char* cacheLocation, const char* ext, BuildParams* buildParams) {return new IntersectBspFast(aimporter,aintersectTechnique,cacheLocation,ext,buildParams);}
		virtual ~IntersectBspFast();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual IntersectTechnique getTechnique() const {return intersectTechnique;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectBspFast(RRMesh* aimporter, IntersectTechnique aintersectTechnique, const char* cacheLocation, const char* ext, BuildParams* buildParams);
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
