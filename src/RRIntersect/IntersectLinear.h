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
	bool intersect_triangle(RRRay* ray, const RRObjectImporter::TriangleSRL* t);

	class IntersectLinear : public RRIntersect
	{
	public:
		IntersectLinear(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique);
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual unsigned  getMemorySize() const;
	protected:
		RRObjectImporter* importer;
		unsigned          triangles;
	};

}

#endif
