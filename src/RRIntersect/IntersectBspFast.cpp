#include "IntersectBspFast.h"

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
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

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

template IBP
bool IntersectBspFast IBP2::intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const
// input:                t, rayOrigin, rayDir, skip, hitDistanceMin, hitDistanceMax
// returns:              true if ray hits t
// modifies when hit:    (distanceMin, hitPoint3d) hitPoint2d, hitOuterSide, hitDistance
// modifies when no hit: (distanceMin, hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	intersectStats.intersect_bspSRLNP++;
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			assert(0);
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangleSRLNP);
	Plane& n=triangleSRLNP[triangle[0]].n3;
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->bsp.front) return false;
			t=front;
		} else {
			if(!t->bsp.back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	const void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=ray->skipTriangle && intersect_triangleSRLNP(ray,triangleSRLNP+*triangle))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->bsp.back) return false;
		t=back;
	} else {
		if(!t->bsp.front) return false;
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBspFast IBP2::intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const
{
begin:
	intersectStats.intersect_bspNP++;
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			assert(0);
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspNP(ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspNP(ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangleNP);
	Plane& n=triangleNP[triangle[0]].n3;
	DBGLINE
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	DBGLINE
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->bsp.front) return false;
			t=front;
		} else {
			if(!t->bsp.back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRObjectImporter::TriangleSRL t2;
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skipTriangle && intersect_triangleNP(ray,triangleNP+*triangle,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->bsp.back) return false;
		t=back;
	} else {
		if(!t->bsp.front) return false;
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
IntersectBspFast IBP2::IntersectBspFast(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams) : IntersectLinear(aimporter, intersectTechnique)
{
	triangleNP = NULL;
	triangleSRLNP = NULL;
	intersectTechnique = aintersectTechnique;

	tree = load IBP2(aimporter,ext,buildParams,this);
	if(!tree) return;

	switch(intersectTechnique)
	{
	case IT_BSP_FASTEST:
		triangleSRLNP = new TriangleSRLNP[triangles];
		break;
	case IT_BSP_FAST:
		triangleNP = new TriangleNP[triangles];
		break;
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

template IBP
bool IntersectBspFast IBP2::isValidTriangle(unsigned i) const
{
	if(triangleSRLNP) return triangleSRLNP[i].intersectByte<10;
	if(triangleNP) return triangleNP[i].intersectByte<10;
	return IntersectLinear::isValidTriangle(i);
}

template IBP
unsigned IntersectBspFast IBP2::getMemorySize() const
{
	return sizeof(IntersectBspFast) 
		+ (tree?tree->bsp.size:0) 
		+ (triangleNP?sizeof(TriangleNP)*triangles:0)
		+ (triangleSRLNP?sizeof(TriangleSRLNP)*triangles:0);
}

template IBP
bool IntersectBspFast IBP2::intersect(RRRay* ray) const
{
	DBG(printf("\n"));
	intersectStats.intersects++;

	bool hit = false;
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	assert(tree);
	switch(intersectTechnique)
	{
		case IT_BSP_FASTEST:
			hit = intersect_bspSRLNP(ray,tree,ray->hitDistanceMax);
			break;
		case IT_BSP_FAST:
			hit = intersect_bspNP(ray,tree,ray->hitDistanceMax);
			break;
		default:
			assert(0);
	}

	if(hit) intersectStats.hits++;
	return hit;
}

template IBP
IntersectBspFast IBP2::~IntersectBspFast()
{
	free((void*)tree);
	delete[] triangleNP;
	delete[] triangleSRLNP;
}

// explicit instantiation

// single-level bsp
template class IntersectBspFast<BspTree44>; // for size 0..2^30-1         [65537..2^32 triangles]

} // namespace
