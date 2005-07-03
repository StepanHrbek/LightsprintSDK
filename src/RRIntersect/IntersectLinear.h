#ifndef RRINTERSECT_INTERSECTLINEAR_H
#define RRINTERSECT_INTERSECTLINEAR_H

#include "geometry.h"
#include "RRIntersect.h"

#define FILL_HITDISTANCE
#define FILL_HITPOINT3D
#define FILL_HITPOINT2D
#define FILL_HITPLANE
#define FILL_HITTRIANGLE
#define FILL_HITSIDE

#define DBGLINE
//#define DBGLINE printf("- %s %i\n",__FILE__, __LINE__);//fgetc(stdin);

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

	void update_hitPoint3d(RRRay* ray, real distance);
	bool intersect_triangleSRLNP(RRRay* ray, const TriangleSRLNP *t);
	bool intersect_triangleNP(RRRay* ray, const TriangleNP *t, const RRObjectImporter::TriangleSRL* t2);
	bool intersect_triangle(RRRay* ray, const RRObjectImporter::TriangleSRL* t);

	class IntersectLinear : public RRIntersect
	{
	public:
		IntersectLinear(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique);
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray) const;
		virtual unsigned  getMemorySize() const;
		bool              isValidTriangle(unsigned i) const;
	protected:
		RRObjectImporter* importer;
		IntersectTechnique intersectTechnique;
		unsigned          triangles;
		TriangleNP*       triangleNP;
		TriangleSRLNP*    triangleSRLNP;
	};

}

#endif
