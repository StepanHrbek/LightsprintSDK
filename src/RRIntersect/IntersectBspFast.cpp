#include "IntersectBspFast.h"

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#ifdef USE_LONGJMP
#include <setjmp.h>
#endif

namespace rrIntersect
{

#ifdef TEST
	bool     watch_tested = true;
	unsigned watch_triangle;
	real     watch_distance;
	Vec3     watch_point3d;
	#define TEST_RANGE(min,max,cond,tree) if(!watch_tested && min<=watch_distance && watch_distance<=max) \
		assert(cond && tree->contains(watch_triangle)); // assert when wanted triangle is to be thrown away from tests
		// If you see many diff_clear_miss in stats (when TEST is on),
		//  it means that triangle that should be very easily hit is missed,
		//  which means that something is wrong with bsp.
		// How does TEST work: 
		//  1. correct result is precalculated using LINEAR and 
		//     if it should be easily hit (not close to border), it's stored into watch_triangle
		//  2. each time subtree is thrown away during bsp traversal, watch_triangle is expected to not be inside
		//  3. if watch_triangle is inside and thus thrown away, it's error -> this assert is thrown
#else
	#define TEST_RANGE(min,max,cond,tree)
#endif

#define DBG(a) //a

/*void TriangleP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.loaded_triangles++);

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
	    intersectReal=1/rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
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
}*/

void TriangleNP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.loaded_triangles++);

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
	    intersectReal=1/rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
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
		FILL_STATISTIC(intersectStats.invalid_triangles++);
		intersectByte=10;  // throw out degenerated triangle
	}
}

void TriangleSRLNP::setGeometry(unsigned atriangleIdx, const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.loaded_triangles++);


	// set s3,r3,l3
	s3 = *a;
	r3 = *b-*a;
	l3 = *c-*a;
	ir3 = Vec3(1/r3.x,1/r3.y,1/r3.z);

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if(ABS(rxy)>=ABS(ryz))
	  if(ABS(rxy)>=ABS(rzx))
	  {
	    intersectByte=0;
	    intersectReal=1/rxy;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	else
	  if(ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
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
		FILL_STATISTIC(intersectStats.invalid_triangles++);
		intersectByte=10;  // throw out degenerated triangle
	}
}

static bool intersect_triangleSRLNP(RRRay* ray, const TriangleSRLNP *t)
// input:                t, hitPoint3d, rayDir
// returns:              true if hitPoint3d is inside t
//                       if hitPoint3d is outside t plane, resut is undefined
// modifies when hit:    hitPoint2d, hitOuterSide
// modifies when no hit: <nothing is changed>
{
	FILL_STATISTIC(intersectStats.intersect_triangleSRLNP++);
	assert(ray);
	assert(t);

	real u,v;
	#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t->s3[Y])*t->r3[X]-(ray->hitPoint3d[X]-t->s3[X])*t->r3[Y]) * t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t->s3[Z]-t->l3[Z]*v) * t->ir3[Z];
#if 1
	static const unsigned tablo[9][3] = {{0,1,2},{1,2,2},{2,0,2},{0,1,0},{1,2,0},{2,0,0},{0,1,1},{1,2,1},{2,0,1}};
	int x=tablo[t->intersectByte][0], y=tablo[t->intersectByte][1], z=tablo[t->intersectByte][2];
	//static const unsigned tablo[3][9] = {{0,1,2,0,1,2,0,1,2},{1,2,0,1,2,0,1,2,0},{2,2,2,0,0,0,1,1,1}};
	//int x=tablo[0][t->intersectByte], y=tablo[1][t->intersectByte], z=tablo[2][t->intersectByte];
	CASE(x,y,z);
#else
	switch(t->intersectByte)
	{
		case 0:CASE(0,1,2);break;
		case 1:CASE(1,2,2);break;
		case 2:CASE(2,0,2);break;
		case 3:CASE(0,1,0);break;
		case 4:CASE(1,2,0);break;
		case 5:CASE(2,0,0);break;
		case 6:CASE(0,1,1);break;
		case 7:CASE(1,2,1);break;
		case 8:CASE(2,0,1);break;
		default: return false;
	}
#endif
	#undef CASE
	if(u<0 || u+v>1) return false;

#ifdef FILL_HITSIDE
	if(ray->rayFlags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		//bool hitOuterSide=size2((*(Vec3*)(ray->rayDir))-t->n3)>2;
		bool hitOuterSide=dot(*(Vec3*)(ray->rayDir),t->n3)<0;
		if(!hitOuterSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;
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

static bool intersect_triangleNP(RRRay* ray, const TriangleNP *t, const RRMeshImporter::TriangleSRL* t2)
{
	FILL_STATISTIC(intersectStats.intersect_triangleNP++);
	assert(ray);
	assert(t);
	assert(t2);

	real u,v;
	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t2->s[Y])*t2->r[X]-(ray->hitPoint3d[X]-t2->s[X])*t2->r[Y]) * t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t2->s[Z]-t2->l[Z]*v)/t2->r[Z];                break
		case 0:CASE(0,1,2);
		case 1:CASE(1,2,2);
		case 2:CASE(2,0,2);
		case 3:CASE(0,1,0);
		case 4:CASE(1,2,0);
		case 5:CASE(2,0,0);
		case 6:CASE(0,1,1);
		case 7:CASE(1,2,1);
		case 8:CASE(2,0,1);
		default: return false;
		#undef CASE
	}
	if(u<0 || u+v>1) return false;

#ifdef FILL_HITSIDE
	if(ray->rayFlags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		//bool hitOuterSide=size2((*(Vec3*)(ray->rayDir))-t->n3)>2;
		bool hitOuterSide=dot(*(Vec3*)(ray->rayDir),t->n3)<0;
		if(!hitOuterSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;
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


#ifdef USE_LONGJMP
	jmp_buf tmpMark;
	#define RETURN_SUCCESS longjmp(tmpMark,1); // probably not thread safe
#else
	#define RETURN_SUCCESS return true;
#endif

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
#define MAX_SIZE 10000000 // max size of node, for runtime checks of consistency
begin:
	FILL_STATISTIC(intersectStats.intersect_bspSRLNP++);
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
#ifdef SUPPORT_EMPTY_KDNODE
		// exit from empty kdnode
		// empty kdnodes are created only in FASTEST (when havran=1 && SUPPORT_EMPTY_KDNODE)
		if(t->allows_emptykd && sizeof(typename BspTree::_Ofs)==4) // this is true only in FASTEST
			if(t->kd.size<=sizeof(BspTree)) // only empty kdnode is so small
				return false;
#endif

		//FILL_STATISTIC(intersectStats.intersect_kd++);
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
				assert(t->kd.getFront()->bsp.size<MAX_SIZE);
				t = t->kd.getFront();
				TEST_RANGE(ray->hitDistanceMin,distanceMax,1,t);
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getFront());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getBack());
			if(intersect_bspSRLNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) RETURN_SUCCESS;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			assert(t->kd.getBack()->bsp.size<MAX_SIZE);
			t = t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				assert(t->kd.getBack()->bsp.size<MAX_SIZE);
				t = t->kd.getBack();
				TEST_RANGE(ray->hitDistanceMin,distanceMax,1,t);
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getBack());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getFront());
			if(intersect_bspSRLNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) RETURN_SUCCESS;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			assert(t->kd.getFront()->bsp.size<MAX_SIZE);
			t = t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangleSRLNP);
	Plane& n=triangleSRLNP[triangle->getTriangleIndex()].n3;

	real nDotDir = ray->rayDir[0]*n[0]+ray->rayDir[1]*n[1]+ray->rayDir[2]*n[2];
	real nDotOrigin = ray->rayOrigin[0]*n[0]+ray->rayOrigin[1]*n[1]+ray->rayOrigin[2]*n[2]+n[3];
	//real nDotDir = ray->rayDir[0]*n[0]+ray->rayDir[1]*n[1]+ray->rayDir[2]*n[2]+ray->rayDir[3]*n[3];
	//real nDotOrigin = ray->rayOrigin[0]*n[0]+ray->rayOrigin[1]*n[1]+ray->rayOrigin[2]*n[2]+ray->rayOrigin[3]*n[3];
	real distancePlane = -nDotOrigin / nDotDir;
	float distanceMinLocation = nDotOrigin + nDotDir * ray->hitDistanceMin; // +=point at distanceMin is in front, -=back, 0=plane
	bool frontback = (distanceMinLocation>0)  // point at distanceMin is in front
		|| (distanceMinLocation==0 && nDotDir<0); // point at distanceMin is in plane and rayDir is from front to back
	/*
	Reference: Old well behaving code.
	float distanceMinLocation = // +=point at distanceMin is in front, -=back, 0=plane
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3];
	real nDotDir = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	bool frontback = (distanceMinLocation>0)  // point at distanceMin is in front
		|| (distanceMinLocation==0 && nDotDir<0); // point at distanceMin is in plane and rayDir is from front to back
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nDotDir;
	*/

	// test only one half
	// distancePlane = 1/0 (ray parallel to plane) is handled here
	if(distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			TEST_RANGE(ray->hitDistanceMin,distanceMax,t->bsp.front,front);
			if(!t->bsp.front) return false;
			assert(front->bsp.size<MAX_SIZE);
			t=front;
		} else {
			TEST_RANGE(ray->hitDistanceMin,distanceMax,t->bsp.back,back);
			if(!t->bsp.back) return false;
			assert(back->bsp.size<MAX_SIZE);
			t=back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled here
	if(_isnan(distancePlane)/*nDotDir==0*/) 
	{
		distancePlane = ray->hitDistanceMin;
		// this sequence of tests follows:
		// front: hitDistanceMin..hitDistanceMin+DELTA_BSP
		// plane
		// back: hitDistanceMin..hitDistanceMax
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) RETURN_SUCCESS;
		TEST_RANGE(ray->hitDistanceMin,distancePlane+DELTA_BSP,t->bsp.front,front);
	} else {
		if(t->bsp.back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) RETURN_SUCCESS;
		TEST_RANGE(ray->hitDistanceMin,distancePlane+DELTA_BSP,t->bsp.back,back);
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	const void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
#ifdef TEST
		if(!watch_tested && *triangle == watch_triangle) 
		{
			watch_tested = true;
			watch_point3d = *(Vec3*)ray->hitPoint3d;
		}
#endif
		if(intersect_triangleSRLNP(ray,triangleSRLNP+triangle->getTriangleIndex()))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = triangle->getTriangleIndex();
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
#ifdef SURFACE_CALLBACK
			if(!ray->surfaceImporter || ray->surfaceImporter->acceptHit(ray)) 
#endif
				RETURN_SUCCESS;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		TEST_RANGE(distancePlane-DELTA_BSP,distanceMax,t->bsp.back,back);
		if(!t->bsp.back) return false;
		assert(back->bsp.size<MAX_SIZE);
		t=back;
	} else {
		TEST_RANGE(distancePlane-DELTA_BSP,distanceMax,t->bsp.front,front);
		if(!t->bsp.front) return false;
		assert(front->bsp.size<MAX_SIZE);
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBspFast IBP2::intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const
{
begin:
	FILL_STATISTIC(intersectStats.intersect_bspNP++);
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
		FILL_STATISTIC(intersectStats.intersect_kd++);
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
				t = t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if(intersect_bspNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) RETURN_SUCCESS;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			t = t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if(intersect_bspNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) RETURN_SUCCESS;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			t = t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangleNP);
	Plane& n=triangleNP[triangle->getTriangleIndex()].n3;

	real nDotDir = ray->rayDir[0]*n[0]+ray->rayDir[1]*n[1]+ray->rayDir[2]*n[2];
	real nDotOrigin = ray->rayOrigin[0]*n[0]+ray->rayOrigin[1]*n[1]+ray->rayOrigin[2]*n[2]+n[3];
	real distancePlane = -nDotOrigin / nDotDir;
	float distanceMinLocation = nDotOrigin + nDotDir * ray->hitDistanceMin; // +=point at distanceMin is in front, -=back, 0=plane
	bool frontback = (distanceMinLocation>0)  // point at distanceMin is in front
		|| (distanceMinLocation==0 && nDotDir<0); // point at distanceMin is in plane and rayDir is from front to back

	// test only one half
	// distancePlane = 1/0 (ray parallel to plane) is handled here
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

	// distancePlane = 0/0 (ray inside plane) is handled here
	if(_isnan(distancePlane)) // nDotDir==0
	{
		distancePlane = ray->hitDistanceMin;
		// this sequence of tests follows:
		// front: hitDistanceMin..hitDistanceMin+DELTA_BSP
		// plane
		// back: hitDistanceMin..hitDistanceMax
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) RETURN_SUCCESS;
	} else {
		if(t->bsp.back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) RETURN_SUCCESS;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRMeshImporter::TriangleSRL t2;
		importer->getTriangleSRL(triangle->getTriangleIndex(),&t2);
		if(intersect_triangleNP(ray,triangleNP+triangle->getTriangleIndex(),&t2))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = triangle->getTriangleIndex();
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
#ifdef SURFACE_CALLBACK
			if(!ray->surfaceImporter || ray->surfaceImporter->acceptHit(ray)) 
#endif
				RETURN_SUCCESS;
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
IntersectBspFast IBP2::IntersectBspFast(RRMeshImporter* aimporter, IntersectTechnique aintersectTechnique, const char* ext, BuildParams* buildParams) : IntersectLinear(aimporter)
{
#ifdef TEST
	test = new IntersectLinear(aimporter);
#endif
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
			if(triangleSRLNP) triangleSRLNP[i].setGeometry(i,(Vec3*)p0,(Vec3*)p1,(Vec3*)p2);
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
unsigned IntersectBspFast IBP2::getMemoryOccupied() const
{
	return sizeof(IntersectBspFast) 
		+ (tree?tree->bsp.size:0) 
		+ (triangleNP?sizeof(TriangleNP)*triangles:0)
		+ (triangleSRLNP?sizeof(TriangleSRLNP)*triangles:0);
}

template IBP
bool IntersectBspFast IBP2::intersect(RRRay* ray) const
{
#ifdef BUNNY_BENCHMARK_OPTIMIZATIONS
	return box.intersectFast(ray) &&
		update_rayDir(ray) &&
		(
#ifdef USE_LONGJMP
			setjmp(tmpMark) || 
#endif
			intersect_bspSRLNP(ray,tree,ray->hitDistanceMax)
		);
#endif

	DBG(printf("\n"));
	FILL_STATISTIC(intersectStats.intersects++);

#ifdef TEST
	ray->flags |= RRRay::FILL_PLANE + RRRay::FILL_DISTANCE;
	RRRay& rayOrig = *RRRay::create(); rayOrig = *ray;
	RRRay& ray2 = *RRRay::create(); ray2 = rayOrig;
	bool hit2 = test->intersect(&ray2);
#endif
	bool hit = false;
	assert(tree);

	if(ray->rayFlags&RRRay::EXPECT_HIT) 
	{
		ray->hitDistanceMin = ray->rayLengthMin;
		ray->hitDistanceMax = ray->rayLengthMax;
	}
	else 
	{
#ifdef USE_SPHERE
		if(!sphere.intersect(ray)) goto test_no;
#endif
		if(!box.intersect(ray)) goto test_no;
		// Zridka se muze stat, ze box.intersectFast vrati true a nastavi nahodne 
		// hitDistanceMin/Max i kdyz se s paprskem neprotina 
		// (nastava kdyz v nejake ose (origin==min || origin==max) && dir==0)
		// Pokud jsou nahodne Min/Max konecna cisla, vse vykryje tree traversal,
		// ktery bude sice nadbytecny, ale nakonec vrati false.
		// Pokud je Min/Max nekonecno, traversal vrati ?
		// Pokud je Min/Max NaN, traversal vrati ?
		// Pri korektnim prubehu by nekonecno ani NaN nastat nemelo
		//  (pokud je konecny mesh, vyjde konecny interval Min/Max).
		// Muzeme tedy nekonecno i NaN zachytit jako nasledek vyse popsane udalosti.
		//if(!_finite(ray->hitDistanceMin) || !_finite(ray->hitDistanceMax)) goto test_no;
	}
	update_rayDir(ray);
#ifdef USE_LONGJMP
	if(setjmp(tmpMark)) return true;
#endif
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
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
test_no:
#ifdef TEST
	if(hit!=hit2 || (hit && hit2 && ray->hitTriangle!=ray2.hitTriangle))
	{
		const float delta = 0.001f;
		watch_tested = true;
		if(hit && hit2 && ray2.hitDistance==ray->hitDistance)
		{
			FILL_STATISTIC(intersectStats.diff_overlap++);
			goto ok;
		}
		if(hit && (!hit2 || ray2.hitDistance>ray->hitDistance)) 
		{
			bool hitBorder = ray->hitPoint2d[0]<delta || ray->hitPoint2d[1]<delta || ray->hitPoint2d[0]+ray->hitPoint2d[1]>1-delta;
			real tmp = ray->rayDir[0]*ray->hitPlane[0]+ray->rayDir[1]*ray->hitPlane[1]+ray->rayDir[2]*ray->hitPlane[2];
			bool hitParallel = fabs(tmp)<delta;
			/*RRRay ray5 = rayOrig;
			update_hitPoint3d(&ray5,ray->hitDistance);
			TriangleSRL srl;
			importer->getTriangleSRL(ray->hitTriangle,&srl);
			bool linearAbleToHit = intersect_triangle(&ray5,&srl);
			if(!ableToHit)
			{
				// precalculated intersect_triangleSRLNP gives different result -> miss
				FILL_STATISTIC(intersectStats.diff_precalc_miss++);
				goto ok;
			}*/
			if(hitBorder)
			{
				FILL_STATISTIC(intersectStats.diff_tight_hit++);
				goto ok;
			}
			if(hitParallel)
			{
				FILL_STATISTIC(intersectStats.diff_parallel_hit++);
				goto ok;
			}
			{
				FILL_STATISTIC(intersectStats.diff_clear_hit++);
				goto bad;
			}
		}
		if(hit2 && (!hit || ray->hitDistance>ray2.hitDistance)) 
		{
			bool hit2Border = ray2.hitPoint2d[0]<delta || ray2.hitPoint2d[1]<delta || ray2.hitPoint2d[0]+ray2.hitPoint2d[1]>1-delta;
			real tmp = ray2.rayDir[0]*ray2.hitPlane[0]+ray2.rayDir[1]*ray2.hitPlane[1]+ray2.rayDir[2]*ray2.hitPlane[2];
			bool hit2Parallel = fabs(tmp)<delta;
			RRRay& ray5 = *RRRay::create(); ray5 = rayOrig;
			update_hitPoint3d(&ray5,ray2.hitDistance);
			bool bspAbleToHit = intersect_triangleSRLNP(&ray5,triangleSRLNP+ray2.hitTriangle);
			if(!bspAbleToHit)
			{
				// precalculated intersect_triangleSRLNP gives different result -> miss
				FILL_STATISTIC(intersectStats.diff_precalc_miss++);
				goto ok;
			}
			if(hit2Border)
			{
				// extremely small numerical inprecisions -> tight miss
				FILL_STATISTIC(intersectStats.diff_tight_miss++);
				goto ok;
			}
			if(hit2Parallel)
			{
				// ray is parallel to plane + extremely small numerical inprecisions -> miss
				FILL_STATISTIC(intersectStats.diff_parallel_miss++);
				goto ok;
			}
			{
				// unknown problem -> miss
				FILL_STATISTIC(intersectStats.diff_clear_miss_tested++);
				watch_tested = false;
				watch_triangle = ray2.hitTriangle;
				watch_distance = ray2.hitDistance;
				goto bad;
			}
			delete &ray5;
		}
		assert(0);
	bad:
		{RRRay& ray3 = *RRRay::create(); ray3 = rayOrig;
		intersect_bspSRLNP(&ray3,tree,ray3.hitDistanceMax);
		if(!watch_tested)
		{
			FILL_STATISTIC(intersectStats.diff_clear_miss_tested--);
			FILL_STATISTIC(intersectStats.diff_clear_miss_not_tested++);
			watch_tested = true;
		}
		RRRay& ray4 = *RRRay::create(); ray4 = rayOrig;
		test->intersect(&ray4);
		delete &ray3;
		delete &ray4;}
	ok:
		delete &rayOrig;
		delete &ray2;
	}
#endif

	FILL_STATISTIC(if(hit) intersectStats.hits++);
	return hit;
}

template IBP
IntersectBspFast IBP2::~IntersectBspFast()
{
	free((void*)tree);
	delete[] triangleNP;
	delete[] triangleSRLNP;
#ifdef TEST
	delete test;
#endif
}

// explicit instantiation

// single-level bsp
template class IntersectBspFast<BspTree44>;

} // namespace
