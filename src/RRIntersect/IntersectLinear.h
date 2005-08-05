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

	void update_hitPoint3d(RRRay* ray, real distance);
	void update_hitPlane(RRRay* ray, RRObjectImporter* importer);
	bool intersect_triangle(RRRay* ray, const RRObjectImporter::TriangleSRL* t);

	class IntersectLinear : public RRIntersect, public RRAligned
	{
	public:
		static IntersectLinear* create(RRObjectImporter* aimporter) {return new IntersectLinear(aimporter);}
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual unsigned  getMemorySize() const;
	protected:
		IntersectLinear(RRObjectImporter* aimporter);
		RRObjectImporter* importer;
		unsigned          triangles;
		real              DELTA_BSP; // tolerance to numeric errors (absolute distance in scenespace)
		Box               box; // aligned + vtable(4) + importer(4) + triangles(4) + DELTA_BSP(4) = aligned
#ifdef USE_SPHERE
		Sphere            sphere;
#endif
	};

}

#endif
