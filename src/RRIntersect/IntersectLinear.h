#ifndef RRINTERSECT_INTERSECTLINEAR_H
#define RRINTERSECT_INTERSECTLINEAR_H

#include "geometry.h"
#include "RRIntersect.h"

namespace rrIntersect
{

	struct TriangleP
	{
		real    intersectReal;  // precalculated number for intersections
		U8      intersectByte;  // precalculated number for intersections, 0..8
		void    setGeometry(Point3 *a,Point3 *b,Point3* c);
	};

	struct TriangleNP : public TriangleP
	{
		Normal  n3;             // normalised normal vector
		void    setGeometry(Point3 *a,Point3 *b,Point3* c);
	};

	struct TriangleSRLNP : public TriangleNP
	{
		Point3  s3;             // absolute position of start of base
		Vec3    r3,l3;          // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0]
		void    setGeometry(Point3 *a,Point3 *b,Point3* c);
	};

	void update_hitPoint3d(RRRay* ray, real distance);
	real intersect_plane_distance(RRRay* ray, Normal n);
	bool intersect_triangleSRLNP(RRRay* ray, TriangleSRLNP *t);
	bool intersect_triangleNP(RRRay* ray, TriangleNP *t, RRObjectImporter::TriangleSRL* t2);
	bool intersect_triangleP(RRRay* ray, TriangleP *t, RRObjectImporter::TriangleSRLN* t2);

	class IntersectLinear : public RRIntersect
	{
	public:
		IntersectLinear(RRObjectImporter* aimporter);
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray);
	protected:
		RRObjectImporter* importer;
		unsigned          triangles;
		TriangleP*        triangleP;
		TriangleNP*       triangleNP;
		TriangleSRLNP*    triangleSRLNP;
	};

}

#endif
