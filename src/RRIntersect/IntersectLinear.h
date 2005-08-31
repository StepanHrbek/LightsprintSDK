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

	extern RRIntersectStats intersectStats;

	PRIVATE void update_hitPoint3d(RRRay* ray, real distance);
	PRIVATE void update_hitPlane(RRRay* ray, RRMeshImporter* importer);
	PRIVATE bool intersect_triangle(RRRay* ray, const RRMeshImporter::TriangleSRL* t);

	class IntersectLinear : public RRCollider, public RRAligned
	{
	public:
		static IntersectLinear* create(RRMeshImporter* aimporter) {return new IntersectLinear(aimporter);}
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual RRMeshImporter* getImporter() const {return importer;}
		virtual IntersectTechnique getTechnique() const {return IT_LINEAR;}
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectLinear(RRMeshImporter* aimporter);
		RRMeshImporter*   importer;
		unsigned          triangles;
		real              DELTA_BSP; // tolerance to numeric errors (absolute distance in scenespace)
		Box               box; // aligned + vtable(4) + importer(4) + triangles(4) + DELTA_BSP(4) = aligned
#ifdef USE_SPHERE
		Sphere            sphere;
#endif
	};

}

#endif
