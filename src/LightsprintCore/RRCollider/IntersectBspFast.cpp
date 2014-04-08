// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - fast.
// Copyright (c) 2000-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "IntersectBspFast.h"

#define DBG(a) //a // enables log of tree traversal, for debugging individual rays

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace std; // necessary for isnan() on Mac

namespace rr
{

		#define TEST_RANGE(min,max,cond,tree)

/*void TriangleP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.numTrianglesLoaded++);

	// set s3,r3,l3
	Vec3 s3=*a;
	Vec3 r3=*b-*a;
	Vec3 l3=*c-*a;

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if (ABS(rxy)>=ABS(ryz))
	  if (ABS(rxy)>=ABS(rzx))
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
	  if (ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	if (ABS(r3.x)>=ABS(r3.y))
	{
	  if (ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if (ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
}*/

void TriangleNP::setGeometry(const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.numTrianglesLoaded++);

	// set s3,r3,l3
	Vec3 s3=*a;
	Vec3 r3=*b-*a;
	Vec3 l3=*c-*a;

	// set intersectByte,intersectReal,u3,v3,n3
	// set intersectByte,intersectReal
	real rxy=l3.y*r3.x-l3.x*r3.y;
	real ryz=l3.z*r3.y-l3.y*r3.z;
	real rzx=l3.x*r3.z-l3.z*r3.x;
	if (ABS(rxy)>=ABS(ryz))
	  if (ABS(rxy)>=ABS(rzx))
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
	  if (ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	if (ABS(r3.x)>=ABS(r3.y))
	{
	  if (ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if (ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
	// calculate normal
	n4=normalized(orthogonalTo(r3,l3));
	n4.w=-dot(s3,RRVec3(n4));

	if (!IS_VEC3(n4)) 
	{
		FILL_STATISTIC(intersectStats.numTrianglesInvalid++);
		intersectByte=10;  // throw out degenerated triangle
	}
}

void TriangleSRLNP::setGeometry(unsigned atriangleIdx, const Vec3* a, const Vec3* b, const Vec3* c)
{
	FILL_STATISTIC(intersectStats.numTrianglesLoaded++);


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
	if (ABS(rxy)>=ABS(ryz))
	  if (ABS(rxy)>=ABS(rzx))
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
	  if (ABS(ryz)>=ABS(rzx))
	  {
	    intersectByte=1;
	    intersectReal=1/ryz;
	  }
	  else
	  {
	    intersectByte=2;
	    intersectReal=1/rzx;
	  }
	if (ABS(r3.x)>=ABS(r3.y))
	{
	  if (ABS(r3.x)>=ABS(r3.z))
	    intersectByte+=3;//max=r3.x
	}
	else
	{
	  if (ABS(r3.y)>=ABS(r3.z))
	    intersectByte+=6;//max=r3.y
	}
	// calculate normal
	n4=normalized(orthogonalTo(r3,l3));
	n4.w=-dot(s3,RRVec3(n4));

	if (!IS_VEC3(n4)) 
	{
		FILL_STATISTIC(intersectStats.numTrianglesInvalid++);
		intersectByte=10;  // throw out degenerated triangle
	}
}

static bool intersect_triangleSRLNP(RRRay* ray, const TriangleSRLNP *t)
// do ray and triangle intersect?
// inputs:            t, ray->hitPoint3d (must be ray-triangleplane intersection), ray->rayFlags, ray->rayDir
// outputs if miss:   false 
// outputs if hit:    true, ray->hitFrontSide, ray->hitPoint2d, ray->hitPlane
{
	FILL_STATISTIC(intersectStats.intersect_triangleSRLNP++);
	RR_ASSERT(ray);
	RR_ASSERT(t);

	real u,v;
	#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t->s3[Y])*t->r3[X]-(ray->hitPoint3d[X]-t->s3[X])*t->r3[Y]) * t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t->s3[Z]-t->l3[Z]*v) * t->ir3[Z];
#if 1
	//RR_ASSERT(t->intersectByte<9); // degenerated triangles are 10, others shoud be in 0..8
	static const unsigned tablo[9+2][3] = {{0,1,2},{1,2,2},{2,0,2},{0,1,0},{1,2,0},{2,0,0},{0,1,1},{1,2,1},{2,0,1}
		,{2,0,1},{2,0,1}}; // covers intersectByte 10 of degenerated triangles
	int x=tablo[t->intersectByte][0], y=tablo[t->intersectByte][1], z=tablo[t->intersectByte][2];
	//static const unsigned tablo[3][9] = {{0,1,2,0,1,2,0,1,2},{1,2,0,1,2,0,1,2,0},{2,2,2,0,0,0,1,1,1}};
	//int x=tablo[0][t->intersectByte], y=tablo[1][t->intersectByte], z=tablo[2][t->intersectByte];
	CASE(x,y,z);
	if (t->intersectByte!=10)
	{
		// valid (not degenerated) triangle
		RR_ASSERT(IS_NUMBER(u));
		RR_ASSERT(IS_NUMBER(v));
	}
	if (u<0 || u+v>1) return false;
	//!!! degenerated triangles should be removed at bsp build time, not here
	if (t->intersectByte>8) return false; // throw out degenerated triangle that was hit
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
	if (u<0 || u+v>1) return false;
#endif
	#undef CASE

#ifdef FILL_HITSIDE
	if (ray->rayFlags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		//bool hitFrontSide=size2(ray->rayDir-t->n3)>2;
		bool hitFrontSide=dot(ray->rayDir,RRVec3(t->n4))<0;
		if (!hitFrontSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;
		ray->hitFrontSide=hitFrontSide;
	}
#endif

#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0]=u;
	ray->hitPoint2d[1]=v;
#endif
#ifdef FILL_HITPLANE
	ray->hitPlane=t->n4;
#endif
	return true;
}

static bool intersect_triangleNP(RRRay* ray, const TriangleNP *t, const RRMesh::TriangleBody* t2)
// do ray and triangle intersect?
// inputs:            t, t2, ray->hitPoint3d (must be ray-triangleplane intersection), ray->rayFlags, ray->rayDir
// outputs if miss:   false 
// outputs if hit:    true, ray->hitFrontSide, ray->hitPoint2d, ray->hitPlane
{
	FILL_STATISTIC(intersectStats.intersect_triangleNP++);
	RR_ASSERT(ray);
	RR_ASSERT(t);
	RR_ASSERT(t2);

	real u,v;
	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((ray->hitPoint3d[Y]-t2->vertex0[Y])*t2->side1[X]-(ray->hitPoint3d[X]-t2->vertex0[X])*t2->side1[Y]) * t->intersectReal;                if (v<0 || v>1) return false;                u=(ray->hitPoint3d[Z]-t2->vertex0[Z]-t2->side2[Z]*v)/t2->side1[Z];                break
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
	if (u<0 || u+v>1) return false;

#ifdef FILL_HITSIDE
	if (ray->rayFlags&(RRRay::FILL_SIDE|RRRay::TEST_SINGLESIDED))
	{
		//bool hitFrontSide=size2(ray->rayDir-t->n3)>2;
		bool hitFrontSide=dot(ray->rayDir,RRVec3(t->n4))<0;
		if (!hitFrontSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;
		ray->hitFrontSide=hitFrontSide;
	}
#endif
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0]=u;
	ray->hitPoint2d[1]=v;
#endif
#ifdef FILL_HITPLANE
	ray->hitPlane=t->n4;
#endif
	return true;
}


template IBP
bool IntersectBspFast IBP2::intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const
// input:                t, rayOrigin, rayDir, skip, hitDistanceMin, hitDistanceMax
// returns:              true if ray hits t
// modifies when hit:    (distanceMin, hitPoint3d) hitPoint2d, hitFrontSide, hitDistance
// modifies when no hit: (distanceMin, hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
#define MAX_SIZE 100000000 // max size of node, for runtime checks of consistency
begin:
	FILL_STATISTIC(intersectStats.intersect_bspSRLNP++);
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// KD
	if (t->bsp.kd)
	{
#ifdef SUPPORT_EMPTY_KDNODE
		// exit from empty kdnode
		// empty kdnodes are created only in FASTEST (when havran=1 && SUPPORT_EMPTY_KDNODE)
		if (t->allows_emptykd && sizeof(typename BspTree::_Ofs)==sizeof(unsigned)) // this is true only in FASTEST
			if (t->kd.size<=sizeof(BspTree)) // only empty kdnode is so small
				return false;
#endif

		//FILL_STATISTIC(intersectStats.intersect_kd++);

		// failuje, vysvetleni nahore u #define TEST_RANGE
		//RR_ASSERT(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if (t->kd.isLeaf()) 
		{
			// kd leaf contains bunch of unsorted triangles
			// RayHits gathers all hits, sorts them by distance and then calls collisionHandler in proper order

			// size of kd leaf
			typename BspTree::_TriInfo* trianglesBegin = t->kd.getTrianglesBegin();
			typename BspTree::_TriInfo* trianglesEnd = (typename BspTree::_TriInfo*)t->getTrianglesEnd();

			// container for all hits in kd leaf
			RayHits rayHits;

			// test all triangles in kd leaf for intersection
			for (typename BspTree::_TriInfo* triangle=trianglesBegin;triangle<trianglesEnd;triangle++)
			{
				TriangleSRLNP* currentTriangle = triangleSRLNP+triangle->getTriangleIndex();
				const RRVec4& n = currentTriangle->n4;
				real nDotDir = ray->rayDir[0]*n[0]+ray->rayDir[1]*n[1]+ray->rayDir[2]*n[2];
				real nDotOrigin = ray->rayOrigin[0]*n[0]+ray->rayOrigin[1]*n[1]+ray->rayOrigin[2]*n[2]+n[3];
				real distancePlane = -nDotOrigin / nDotDir;
				DBG(bool hit=false);
				if (distancePlane>=ray->hitDistanceMin && distancePlane<=distanceMax)
				{
					update_hitPoint3d(ray,distancePlane);
					if (intersect_triangleSRLNP(ray,currentTriangle))
					{
						DBG(hit=true);
						ray->hitTriangle = triangle->getTriangleIndex();
						ray->hitDistance = distancePlane;
						rayHits.insertHitUnordered(ray);
					}
				}
			}
			return rayHits.getHitOrdered(ray,importer);
		}

//#define DISTANCE_SPACE // pokus

		// test subtrees
		// verze pocitajici neco v distance, neco v objectspace
		// haze asserty kdyz daji vypocty ruzne vysledky
		// prekvapive rychlejsi, i kdyz ma vic vypoctu

		// all math is limited to kd.splitAxis axis
		// kd node is split at splitValue
		real splitValue = t->kd.getSplitValue();
		// our ray segment is pointMinVal..pointMaxVal
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		// kd splitting plane belongs to back(negative) son
		// when pointMinVal==splitValue, ray segment starts at splitting plane (back)
		// when pointMinVal>splitValue, ray segment starts in front
		if (pointMinVal>splitValue)
		{
			// front only
			if (pointMaxVal>splitValue) 
			// WAS:	if (pointMaxVal>=splitValue) ...mohlo preskocit face v plane (back)
			{
				RR_ASSERT(t->kd.getFront()->bsp.size<MAX_SIZE);
				t = t->kd.getFront();
				TEST_RANGE(ray->hitDistanceMin,distanceMax,1,t);
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getFront());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getBack());
			if (intersect_bspSRLNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) return true;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			RR_ASSERT(t->kd.getBack()->bsp.size<MAX_SIZE);
			t = t->kd.getBack();
			goto begin;
		}
		else
		// when pointMinVal<=splitValue, ray segment starts in back
		{
			// back only
			// if point1[axis]==point2[axis]==splitVertex[axis], testing only back should be sufficient
			if (pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				RR_ASSERT(t->kd.getBack()->bsp.size<MAX_SIZE);
				t = t->kd.getBack();
				TEST_RANGE(ray->hitDistanceMin,distanceMax,1,t);
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getBack());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getFront());
			if (intersect_bspSRLNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) return true;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			RR_ASSERT(t->kd.getFront()->bsp.size<MAX_SIZE);
			t = t->kd.getFront();
			goto begin;
		}


	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	RR_ASSERT(triangleSRLNP);
	const RRVec4& n=triangleSRLNP[triangle->getTriangleIndex()].n4;

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
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if (frontback)
		{
			TEST_RANGE(ray->hitDistanceMin,distanceMax,t->bsp.front,front);
			if (!t->bsp.front) return false;
			RR_ASSERT(front->bsp.size<MAX_SIZE);
			t=front;
		} else {
			TEST_RANGE(ray->hitDistanceMin,distanceMax,t->bsp.back,back);
			if (!t->bsp.back) return false;
			RR_ASSERT(back->bsp.size<MAX_SIZE);
			t=back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled here
	if (_isnan(distancePlane)/*nDotDir==0*/) 
	{
		distancePlane = ray->hitDistanceMin;
		// this sequence of tests follows:
		// front: hitDistanceMin..hitDistanceMin+DELTA_BSP
		// plane
		// back: hitDistanceMin..hitDistanceMax
	}

	// test first half
	if (frontback)
	{
		if (t->bsp.front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
		TEST_RANGE(ray->hitDistanceMin,distancePlane+DELTA_BSP,t->bsp.front,front);
	} else {
		if (t->bsp.back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
		TEST_RANGE(ray->hitDistanceMin,distancePlane+DELTA_BSP,t->bsp.back,back);
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	const void* trianglesEnd=t->getTrianglesEnd();
	while (triangle<trianglesEnd)
	{
		if (intersect_triangleSRLNP(ray,triangleSRLNP+triangle->getTriangleIndex()))
		{
			RR_ASSERT(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = triangle->getTriangleIndex();
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
#ifdef COLLISION_HANDLER
			if (!ray->collisionHandler || ray->collisionHandler->collides(ray)) 
#endif
				return true;
		}
		triangle++;
	}

	// test other half
	if (frontback)
	{
		TEST_RANGE(distancePlane-DELTA_BSP,distanceMax,t->bsp.back,back);
		if (!t->bsp.back) return false;
		RR_ASSERT(back->bsp.size<MAX_SIZE);
		t=back;
	} else {
		TEST_RANGE(distancePlane-DELTA_BSP,distanceMax,t->bsp.front,front);
		if (!t->bsp.front) return false;
		RR_ASSERT(front->bsp.size<MAX_SIZE);
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

DBG(static int q1=0);

template IBP
bool IntersectBspFast IBP2::intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const
{
begin:
	DBG(q1++;RRReporter::report(INF1,"intersect_bsp(%s%d) %d %f..%f\n",t->bsp.kd?"kd":"bsp",t->kd.splitAxis,q1,ray->hitDistanceMin,distanceMax));
	FILL_STATISTIC(intersectStats.intersect_bspNP++);
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// KD
	if (t->bsp.kd)
	{
		FILL_STATISTIC(intersectStats.intersect_kd++);
		RR_ASSERT(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if (t->kd.isLeaf()) 
		{
			DBG(RRReporter::report(INF1," kd leaf\n"));
			// kd leaf contains bunch of unsorted triangles
			// RayHits gathers all hits, sorts them by distance and then calls collisionHandler in proper order

			// size of kd leaf
			typename BspTree::_TriInfo* trianglesBegin = t->kd.getTrianglesBegin();
			typename BspTree::_TriInfo* trianglesEnd = (typename BspTree::_TriInfo*)t->getTrianglesEnd();

			// container for all hits in kd leaf
			RayHits rayHits;

			// test all triangles in kd leaf for intersection
			for (typename BspTree::_TriInfo* triangle=trianglesBegin;triangle<trianglesEnd;triangle++)
			{
				TriangleNP* currentTriangle = triangleNP+triangle->getTriangleIndex();
				const RRVec4& n = currentTriangle->n4;
				real nDotDir = ray->rayDir[0]*n[0]+ray->rayDir[1]*n[1]+ray->rayDir[2]*n[2];
				real nDotOrigin = ray->rayOrigin[0]*n[0]+ray->rayOrigin[1]*n[1]+ray->rayOrigin[2]*n[2]+n[3];
				real distancePlane = -nDotOrigin / nDotDir;
				DBG(bool hit=false);
				if (distancePlane>=ray->hitDistanceMin && distancePlane<=distanceMax)
				{
					RRMesh::TriangleBody srl;
					importer->getTriangleBody(triangle->getTriangleIndex(),srl);
					update_hitPoint3d(ray,distancePlane);
					if (intersect_triangleNP(ray,currentTriangle,&srl))
					{
						DBG(hit=true);
						ray->hitTriangle = triangle->getTriangleIndex();
						ray->hitDistance = distancePlane;
						rayHits.insertHitUnordered(ray);
					}
				}
				DBG(RRReporter::report(INF1," kd leaf tri %d dist %f %s\n",triangle->getTriangleIndex(),distancePlane,hit?"hit":""));
			}
			return rayHits.getHitOrdered(ray,importer);
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if (pointMinVal>splitValue) 
		{
			// front only
			if (pointMaxVal>splitValue) 
			// WAS: if (pointMaxVal>=splitValue) probably error
			{
				DBG(RRReporter::report(INF1," kd front\n"));
				t = t->kd.getFront();
				goto begin;
			}
			// front and back
			DBG(RRReporter::report(INF1," kd FRONT+back\n"));
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if (intersect_bspNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) return true;
			DBG(RRReporter::report(INF1," kd front+BACK\n"));
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			t = t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if (pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = t->kd.getBack();
				DBG(RRReporter::report(INF1," kd back\n"));
				goto begin;
			}
			// back and front
			DBG(RRReporter::report(INF1," kd BACK+front\n"));
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if (intersect_bspNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) return true;
			DBG(RRReporter::report(INF1," kd back+FRONT\n"));
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			t = t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	RR_ASSERT(triangleNP);

	const RRVec4& n=triangleNP[triangle->getTriangleIndex()].n4;
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
		if (frontback)
		{
			DBG(RRReporter::report(INF1," bsp front\n"));
			if (!t->bsp.front) return false;
			DBG(RRReporter::report(INF1," bsp FRONT\n"));
			t=front;
		} else {
			DBG(RRReporter::report(INF1," bsp back\n"));
			if (!t->bsp.back) return false;
			DBG(RRReporter::report(INF1," bsp BACK\n"));
			t=back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled here
	if (_isnan(distancePlane)) // nDotDir==0
	{
		distancePlane = ray->hitDistanceMin;
		// this sequence of tests follows:
		// front: hitDistanceMin..hitDistanceMin+DELTA_BSP
		// plane
		// back: hitDistanceMin..hitDistanceMax
	}

	// test first half
	if (frontback)
	{
		DBG(RRReporter::report(INF1," bsp FRONT+back\n"));
		if (t->bsp.front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		DBG(RRReporter::report(INF1," bsp BACK+front\n"));
		if (t->bsp.back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	DBG(RRReporter::report(INF1," bsp plane\n"));
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while (triangle<trianglesEnd)
	{
		RRMesh::TriangleBody t2;
		importer->getTriangleBody(triangle->getTriangleIndex(),t2);
		if (intersect_triangleNP(ray,triangleNP+triangle->getTriangleIndex(),&t2))
		{
			RR_ASSERT(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = triangle->getTriangleIndex();
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
#ifdef COLLISION_HANDLER
			if (!ray->collisionHandler || ray->collisionHandler->collides(ray)) 
#endif
				return true;
		}
		triangle++;
	}

	// test other half
	if (frontback)
	{
		DBG(RRReporter::report(INF1," bsp front+BACK\n"));
		if (!t->bsp.back) return false;
		DBG(RRReporter::report(INF1," bsp front+BACK go\n"));
		t=back;
	} else {
		DBG(RRReporter::report(INF1," bsp back+FRONT\n"));
		if (!t->bsp.front) return false;
		DBG(RRReporter::report(INF1," bsp back+FRONT go\n"));
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
IntersectBspFast IBP2::IntersectBspFast(const RRMesh* aimporter, IntersectTechnique aintersectTechnique, bool& aborting, const char* cacheLocation, const char* ext, BuildParams* buildParams) : IntersectLinear(aimporter)
{
	RRReportInterval report(INF3,"Building collider for %d triangles ...\n",aimporter?aimporter->getNumTriangles():0);

	triangleNP = NULL;
	triangleSRLNP = NULL;
	intersectTechnique = aintersectTechnique;

	switch(intersectTechnique)
	{
		case IT_BSP_FASTEST:
		case IT_BSP_FASTER:
			triangleSRLNP = new TriangleSRLNP[triangles];
			break;
		case IT_BSP_FAST:
			triangleNP = new TriangleNP[triangles];
			break;
		default:
			RR_ASSERT(0);
	}
	if (triangleNP||triangleSRLNP)
		for (unsigned i=0;i<triangles;i++)
		{
			RRMesh::Triangle t;
			importer->getTriangle(i,t);
			RRMesh::Vertex v[3];
			importer->getVertex(t[0],v[0]);
			importer->getVertex(t[1],v[1]);
			importer->getVertex(t[2],v[2]);
			if (triangleNP) triangleNP[i].setGeometry(&v[0],&v[1],&v[2]);
			if (triangleSRLNP) triangleSRLNP[i].setGeometry(i,&v[0],&v[1],&v[2]);
		}

	tree = load IBP2(aimporter,aborting,cacheLocation,ext,buildParams,this);
	if (!tree)
	{
		RR_SAFE_DELETE_ARRAY(triangleNP); // must be deleted -> getMemoryOccupied is low -> RRCollider::create sees failure and switches to IT_LINEAR
		RR_SAFE_DELETE_ARRAY(triangleSRLNP);
		intersectTechnique = IT_LINEAR;
		return;
	}
}

template IBP
bool IntersectBspFast IBP2::isValidTriangle(unsigned i) const
{
	if (triangleSRLNP) return triangleSRLNP[i].intersectByte<10;
	if (triangleNP) return triangleNP[i].intersectByte<10;
	return IntersectLinear::isValidTriangle(i);
}

template IBP
size_t IntersectBspFast IBP2::getMemoryOccupied() const
{
	return sizeof(IntersectBspFast) 
		+ (tree?tree->bsp.size:0) 
		+ (triangleNP?sizeof(TriangleNP)*triangles:0)
		+ (triangleSRLNP?sizeof(TriangleSRLNP)*triangles:0);
}

// debug ray
void (*g_logRay)(const RRRay* ray,bool hit);

template IBP
bool IntersectBspFast IBP2::intersect(RRRay* ray) const
{
#ifdef BUNNY_BENCHMARK_OPTIMIZATIONS
	RR_ASSERT(intersectTechnique==IT_BSP_FASTEST);
	return box.intersect(ray) &&
		update_rayDir(ray) &&
		intersect_bspSRLNP(ray,tree,ray->hitDistanceMax);
#endif

	FILL_STATISTIC(intersectStats.intersect_mesh++);

	bool hit = false;
	RR_ASSERT(tree);

#ifdef COLLISION_HANDLER
	if (ray->collisionHandler)
		ray->collisionHandler->init(ray);
#endif

	// collisionHandler->init/done must be called _always_, users depend on it,
	// having ray rejected early by box.intersect test is no excuse
	if (box.intersect(ray))
	{
		update_rayDir(ray);
		RR_ASSERT(fabs(size2(ray->rayDir)-1)<0.001);//ocekava normalizovanej dir
		switch(intersectTechnique)
		{
			case IT_BSP_FASTEST:
			case IT_BSP_FASTER:
				hit = intersect_bspSRLNP(ray,tree,ray->hitDistanceMax);
				break;
			case IT_BSP_FAST:
				hit = intersect_bspNP(ray,tree,ray->hitDistanceMax);
				break;
			default:
				RR_ASSERT(0);
		}
	}

#ifdef COLLISION_HANDLER
	if (ray->collisionHandler)
		hit = ray->collisionHandler->done();
#endif


	// debug ray
	if (g_logRay) g_logRay(ray,hit);

	FILL_STATISTIC(if (hit) intersectStats.hit_mesh++);
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
template class IntersectBspFast<BspTree44>;

} // namespace
