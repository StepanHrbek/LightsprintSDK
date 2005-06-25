#include "IntersectLinear.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

namespace rrIntersect
{

#define DBG(a) //a

void TriangleP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	intersectStats.loaded_triangles++;

	// set s3,r3,l3
	Vec3 s3=*a;
	Vec3 r3=*b-*a;
	Vec3 l3=*c-*a;

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if(ABS(rxy)>=ABS(ryz))
	  if(ABS(rxy)>=ABS(rzx))
	  {
	    intersectByte=0;
	    intersectReal=rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	if(ABS(r3.x)>=ABS(r3.y))
	{
	  if(ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if(ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
}

void TriangleNP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	intersectStats.loaded_triangles++;

	// set s3,r3,l3
	Vec3 s3=*a;
	Vec3 r3=*b-*a;
	Vec3 l3=*c-*a;

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if(ABS(rxy)>=ABS(ryz))
	  if(ABS(rxy)>=ABS(rzx))
	  {
	    intersectByte=0;
	    intersectReal=rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	if(ABS(r3.x)>=ABS(r3.y))
	{
	  if(ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if(ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
	// calculate normal
	n3=normalized(ortogonalTo(r3,l3));
	n3.d=-dot(s3,n3);

	if(!IS_VEC3(n3)) 
	{
		intersectStats.invalid_triangles++;
		intersectByte=10;  // throw out degenerated triangle
	}
}

void TriangleSRLNP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	intersectStats.loaded_triangles++;

	// set s3,r3,l3
	s3=*a;
	r3=*b-*a;
	l3=*c-*a;

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if(ABS(rxy)>=ABS(ryz))
	  if(ABS(rxy)>=ABS(rzx))
	  {
	    intersectByte=0;
	    intersectReal=rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=rzx;
	  }
	if(ABS(r3.x)>=ABS(r3.y))
	{
	  if(ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if(ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
	// calculate normal
	n3=normalized(ortogonalTo(r3,l3));
	n3.d=-dot(s3,n3);

	if(!IS_VEC3(n3)) 
	{
		intersectStats.invalid_triangles++;
		intersectByte=10;  // throw out degenerated triangle
	}
}

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

bool intersect_triangleSRLNP(RRRay* ray, const TriangleSRLNP *t)
// input:                t, hitPoint3d, rayDir
// returns:              true if hitPoint3d is inside t
//                       if hitPoint3d is outside t plane, resut is undefined
// modifies when hit:    hitPoint2d, hitOuterSide
// modifies when no hit: <nothing is changed>
{
	intersectStats.intersect_triangleSRLNP++;
	assert(ray);
	assert(t);

	real u,v;
	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t->s3[Y])*t->r3[X]-(ray->hitPoint3d[X]-t->s3[X])*t->r3[Y]) / t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t->s3[Z]-t->l3[Z]*v)/t->r3[Z];                break
		case 0:CASE(0,1,2);
		case 3:CASE(0,1,0);
		case 6:CASE(0,1,1);
		case 1:CASE(1,2,2);
		case 4:CASE(1,2,0);
		case 7:CASE(1,2,1);
		case 2:CASE(2,0,2);
		case 5:CASE(2,0,0);
		case 8:CASE(2,0,1);
		default: return false;
		#undef CASE
	}
	if (u<0 || u+v>1) return false;

#ifdef FILL_HITSIDE
	if(ray->flags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		bool hitOuterSide=size2((*(Vec3*)(ray->rayDir))-t->n3)>2;
		if(!hitOuterSide && (ray->flags&RRRay::TEST_SINGLESIDED)) return false;
		ray->hitOuterSide=hitOuterSide;
	}
#endif
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0]=u;
	ray->hitPoint2d[1]=v;
#endif
#ifdef FILL_HITPLANE
	*(Plane*)ray->hitPlane=t->n3;
#endif
	return true;
}

bool intersect_triangleNP(RRRay* ray, const TriangleNP *t, const RRObjectImporter::TriangleSRL* t2)
{
	intersectStats.intersect_triangleNP++;
	assert(ray);
	assert(t);
	assert(t2);

	real u,v;
	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t2->s[Y])*t2->r[X]-(ray->hitPoint3d[X]-t2->s[X])*t2->r[Y]) / t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t2->s[Z]-t2->l[Z]*v)/t2->r[Z];                break
		case 0:CASE(0,1,2);
		case 3:CASE(0,1,0);
		case 6:CASE(0,1,1);
		case 1:CASE(1,2,2);
		case 4:CASE(1,2,0);
		case 7:CASE(1,2,1);
		case 2:CASE(2,0,2);
		case 5:CASE(2,0,0);
		case 8:CASE(2,0,1);
		default: return false;
		#undef CASE
	}
	if (u<0 || u+v>1) return false;

#ifdef FILL_HITSIDE
	if(ray->flags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		bool hitOuterSide=size2((*(Vec3*)(ray->rayDir))-t->n3)>2;
		if(!hitOuterSide && (ray->flags&RRRay::TEST_SINGLESIDED)) return false;
		ray->hitOuterSide=hitOuterSide;
	}
#endif
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0]=u;
	ray->hitPoint2d[1]=v;
#endif
#ifdef FILL_HITPLANE
	*(Plane*)ray->hitPlane=t->n3;
#endif
	return true;
}

#define EPSILON 0.000001

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
	if (det>-EPSILON && det<EPSILON) return false;

	// calculate distance from vert0 to ray origin
	Vec3 tvec = *(Vec3*)ray->rayOrigin-*(Vec3*)t->s;

	// calculate U parameter and test bounds
	real u = dot(tvec,pvec)/det;
	if (u<0 || u>1) return false;

	// prepare to test V parameter
	Vec3 qvec = ortogonalTo(tvec,*(Vec3*)t->r);

	// calculate V parameter and test bounds
	real v = dot(*(Vec3*)ray->rayDir,qvec)/det;
	if (v<0 || u+v>1) return false;

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

IntersectLinear::IntersectLinear(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique)
{
	importer = aimporter;
	intersectTechnique = aintersectTechnique;
	triangles = importer->getNumTriangles();
	triangleNP = NULL;
	triangleSRLNP = NULL;
	switch(intersectTechnique)
	{
		case IT_BSP_FASTEST:
			triangleSRLNP = new TriangleSRLNP[triangles];
			break;
		case IT_BSP_FAST:
			triangleNP = new TriangleNP[triangles];
			break;
		case IT_BSP_COMPACT:
		case IT_BSP_MOST_COMPACT:
		case IT_LINEAR:
			break;
		//case IT_BSP2_COMPACT:
		default:
			assert(0);
	}
	if(triangleNP||triangleSRLNP)
		for(unsigned i=0;i<triangles;i++)
		{
			unsigned v0,v1,v2;
			importer->getTriangle(i,v0,v1,v2);
			real* p0 = importer->getVertex(v0);
			real* p1 = importer->getVertex(v1);
			real* p2 = importer->getVertex(v2);
			if(triangleNP) triangleNP[i].setGeometry((Vec3*)p0,(Vec3*)p1,(Vec3*)p2);
			if(triangleSRLNP) triangleSRLNP[i].setGeometry((Vec3*)p0,(Vec3*)p1,(Vec3*)p2);
		}
}

unsigned IntersectLinear::getMemorySize() const
{
	return (triangleNP?sizeof(TriangleNP)*triangles:0)
		+(triangleSRLNP?sizeof(TriangleSRLNP)*triangles:0)
		+sizeof(IntersectLinear);
}

bool IntersectLinear::isValidTriangle(unsigned i) const
{
	if(triangleSRLNP) return triangleSRLNP[i].intersectByte<10;
	if(triangleNP) return triangleNP[i].intersectByte<10;
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
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	ray->hitDistance = ray->hitDistanceMax;

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
	delete[] triangleNP;
	delete[] triangleSRLNP;
}

} // namespace
