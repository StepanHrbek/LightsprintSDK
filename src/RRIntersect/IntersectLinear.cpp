#include "IntersectLinear.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define DBG(a) //a

namespace rrIntersect
{

void update_hitPoint3d(RRRay* ray, real distance)
{
	ray->hitPoint3d[0] = ray->rayOrigin[0] + ray->rayDir[0]*distance;
	ray->hitPoint3d[1] = ray->rayOrigin[1] + ray->rayDir[1]*distance;
	ray->hitPoint3d[2] = ray->rayOrigin[2] + ray->rayDir[2]*distance;
}

void update_hitPlane(RRRay* ray, RRObjectImporter* importer)
{
	RRObjectImporter::TriangleSRL t2;
	importer->getTriangleSRL(ray->hitTriangle,&t2);
	Vec3 n;
	n.x = t2.r[1] * t2.l[2] - t2.r[2] * t2.l[1];
	n.y = t2.r[2] * t2.l[0] - t2.r[0] * t2.l[2];
	n.z = t2.r[0] * t2.l[1] - t2.r[1] * t2.l[0];
	real siz = size(n);
	ray->hitPlane[0] = n.x/siz;
	ray->hitPlane[1] = n.y/siz;
	ray->hitPlane[2] = n.z/siz;
	ray->hitPlane[3] = -(t2.s[0] * ray->hitPlane[0] + t2.s[1] * ray->hitPlane[1] + t2.s[2] * ray->hitPlane[2]);
}

bool intersect_triangle(RRRay* ray, const RRObjectImporter::TriangleSRL* t)
// input:                ray, t
// returns:              true if ray hits t
// modifies when hit:    hitDistance, hitPoint2D, hitOuterSide
// modifies when no hit: <nothing is changed>
{
	intersectStats.intersect_triangle++;
	assert(ray);
	assert(t);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec = ortogonalTo(*(Vec3*)ray->rayDir,*(Vec3*)t->l);
	real det = dot(*(Vec3*)t->r,pvec);

	// cull test
	bool hitOuterSide = det>0;
	if(!hitOuterSide && (ray->flags&RRRay::TEST_SINGLESIDED)) return false;

	// if determinant is near zero, ray lies in plane of triangle
	if(det==0) return false;
	//#define EPSILON 1e-10 // 1e-6 good for all except bunny, 1e-10 good for bunny
	//if(det>-EPSILON && det<EPSILON) return false;

	// calculate distance from vert0 to ray origin
	Vec3 tvec = *(Vec3*)ray->rayOrigin-*(Vec3*)t->s;

	// calculate U parameter and test bounds
	real u = dot(tvec,pvec)/det;
	if(u<0 || u>1) return false;

	// prepare to test V parameter
	Vec3 qvec = ortogonalTo(tvec,*(Vec3*)t->r);

	// calculate V parameter and test bounds
	real v = dot(*(Vec3*)ray->rayDir,qvec)/det;
	if(v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	real dist = dot(*(Vec3*)t->l,qvec)/det;
	if(dist<ray->hitDistanceMin || dist>ray->hitDistanceMax) return false;

	ray->hitDistance = dist;
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0] = u;
	ray->hitPoint2d[1] = v;
#endif
#ifdef FILL_HITSIDE
	ray->hitOuterSide = hitOuterSide;
#endif
	return true;
}

IntersectLinear::IntersectLinear(RRObjectImporter* aimporter)
{
	importer = aimporter;
	triangles = importer->getNumTriangles();

	unsigned vertices = importer->getNumVertices();
	Vec3* vertex = new Vec3[vertices];
	for(unsigned i=0;i<vertices;i++)
	{
		real* v = importer->getVertex(i);
		vertex[i].x = v[0];
		vertex[i].y = v[1];
		vertex[i].z = v[2];
	}
	sphere.detect(vertex,vertices);
	box.detect(vertex,vertices);
	delete[] vertex;

	DELTA_BSP = sphere.radius*1e-5f;
}

unsigned IntersectLinear::getMemorySize() const
{
	return sizeof(IntersectLinear);
}

bool IntersectLinear::isValidTriangle(unsigned i) const
{
	unsigned v[3];
	importer->getTriangle(i,v[0],v[1],v[2]);
	return v[0]!=v[1] && v[0]!=v[2] && v[1]!=v[2];
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool IntersectLinear::intersect(RRRay* ray) const
{
	DBG(printf("\n"));
	intersectStats.intersects++;
	if(!importer) return false; // this shouldn't happen but linear is so slow that we can test it
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	ray->hitDistance = ray->hitDistanceMax;

	if(!(ray->flags&RRRay::SKIP_PRETESTS))
	if(!sphere.intersect(ray) || !box.intersect(ray)) return false;

	bool hit = false;
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	intersectStats.intersect_linear++;
	for(unsigned t=0;t<triangles;t++) if(t!=ray->skipTriangle)
	{
		RRObjectImporter::TriangleSRL t2;
		importer->getTriangleSRL(t,&t2);
		if(intersect_triangle(ray,&t2))
		{
			hit = true;
			ray->hitTriangle = t;
			ray->hitDistanceMax = ray->hitDistance;
		}
	}
	if(hit) 
	{
#ifdef FILL_HITPOINT3D
		if(ray->flags&RRRay::FILL_POINT3D)
		{
			update_hitPoint3d(ray,ray->hitDistance);
		}
#endif
#ifdef FILL_HITPLANE
		if(ray->flags&RRRay::FILL_PLANE)
		{
			update_hitPlane(ray,importer);
		}
#endif
		intersectStats.hits++;
	}
	return hit;
}

IntersectLinear::~IntersectLinear()
{
}

} // namespace
