#ifndef RRINTERSECT_INTERSECTLINEAR_H
#define RRINTERSECT_INTERSECTLINEAR_H

#include "geometry.h"
#include "RRIntersect.h"

namespace rrIntersect
{

	struct TriangleP
	{
		real    intersectReal;  // precalculated number for intersections
		U8      intersectByte:4;// precalculated number for intersections, 0..8
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

	// global variables used only by intersections to speed up recursive calls
	// intersector input
	//extern RRRay     i_ray;
	extern Point3    i_eye;
	extern Vec3      i_direction;
	extern real      i_hitDistance;
	extern Point3    i_hitPoint3d;
	// intersector output
	//extern RRHit   i_hit;
	extern bool      i_hitOuterSide;
	extern real      i_hitU;
	extern real      i_hitV;
	real intersect_plane_distance(Normal n);
	bool intersect_triangleSRLNP(TriangleSRLNP *t);
	bool intersect_triangleNP(TriangleNP *t, RRObjectImporter::TriangleSRL* t2);
	bool intersect_triangleP(TriangleP *t, RRObjectImporter::TriangleSRLN* t2);

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
