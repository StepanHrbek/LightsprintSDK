// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - compact.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "IntersectBspCompact.h"

#define DBG(a) //a // enables log of tree traversal, for debugging individual rays

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace std; // necessary for isnan() on Mac

namespace rr
{

// slightly modified version from IntersectLinear.cpp:
//  instead of <ray->hitDistanceMin,ray->hitDistanceMax>,
//  it tests inside <ray->hitDistanceMin,distanceMax>,
//  which is required by kd-leaf
static bool intersect_triangle(RRRay* ray, const RRMesh::TriangleBody* t, RRReal distanceMax)
// input:                ray, t, distanceMax
// returns:              true if ray hits t
// modifies when hit:    ray->hitDistance, ray->hitPoint2D, ray->hitFrontSide
// modifies when no hit: <nothing is changed>
{
	FILL_STATISTIC(intersectStats.intersect_triangle++);
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec = orthogonalTo(ray->rayDir,t->side2);
	real det = dot(t->side1,pvec);

	// cull test
	bool hitFrontSide = det>0;
	if (!hitFrontSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;

	// if determinant is near zero, ray lies in plane of triangle
	if (det==0) return false;
	//#define EPSILON 1e-10 // 1e-6 good for all except bunny, 1e-10 good for bunny
	//if (det>-EPSILON && det<EPSILON) return false;

	// calculate distance from vert0 to ray origin
	Vec3 tvec = ray->rayOrigin-t->vertex0;

	// calculate U parameter and test bounds
	real u = dot(tvec,pvec)/det;
	if (u<0 || u>1) return false;

	// prepare to test V parameter
	Vec3 qvec = orthogonalTo(tvec,t->side1);

	// calculate V parameter and test bounds
	real v = dot(ray->rayDir,qvec)/det;
	if (v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	real dist = dot(t->side2,qvec)/det;
	if (dist<ray->hitDistanceMin || dist>distanceMax) return false;

	ray->hitDistance = dist;
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0] = u;
	ray->hitPoint2d[1] = v;
#endif
#ifdef FILL_HITSIDE
	ray->hitFrontSide = hitFrontSide;
#endif
	return true;
}

DBG(static int q1=0);

template IBP
bool IntersectBspCompact IBP2::intersect_bsp(RRRay* ray, const BspTree* t, real distanceMax) const
{
begin:
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// transition to sons with different size
	if (t->allows_transition && t->bsp.transition)
	{
		#define intersect_bsp_type_func(type,func,ray,tree,distanceMax) ((IntersectBspCompact<typename type>*)this)->func(ray,(typename type*)tree,distanceMax)
		#define intersect_bsp_type(type,ray,tree,distanceMax) intersect_bsp_type_func(type,intersect_bsp,ray,tree,distanceMax)
		return intersect_bsp_type(BspTree::Transitioneer,ray,t,distanceMax);
	}
	DBG(q1++;RRReporter::report(INF1,"intersect_bsp(%s%d) %d %f..%f\n",t->bsp.kd?"kd":"bsp",t->kd.splitAxis,q1,ray->hitDistanceMin,distanceMax));
	
	// KD
	if (t->bsp.kd)
	{
		//FILL_STATISTIC(intersectStats.intersect_kd++);
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
				RRMesh::TriangleBody srl;
				importer->getTriangleBody(triangle->getTriangleIndex(),srl);
				DBG(bool hit=false);
				if (intersect_triangle(ray,&srl,distanceMax))
				{
					DBG(hit=true);
					ray->hitTriangle = triangle->getTriangleIndex();
					rayHits.insertHitUnordered(ray);
				}
				DBG(RRReporter::report(INF1," kd leaf tri %d dist %f %s\n",triangle->getTriangleIndex(),ray->hitDistance,hit?"hit":""));
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
			if (pointMaxVal>=splitValue) 
			{
				DBG(RRReporter::report(INF1," kd front\n"));
				if (t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,t->kd.getFront(),distanceMax);
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			DBG(RRReporter::report(INF1," kd FRONT+back\n"));
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if (intersect_bsp_type(BspTree::Son,ray,t->kd.getFront(),distSplit+DELTA_BSP)) return true;
			DBG(RRReporter::report(INF1," kd front+BACK\n"));
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			if (t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,t->kd.getBack(),distanceMax);
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if (pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				if (t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,t->kd.getBack(),distanceMax);
				t = (BspTree*)t->kd.getBack();
				DBG(RRReporter::report(INF1," kd back\n"));
				goto begin;
			}
			// back and front
			DBG(RRReporter::report(INF1," kd BACK+front\n"));
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis]) DIVIDE_BY_RAYDIR[t->kd.splitAxis];
			if (intersect_bsp_type(BspTree::Son,ray,t->kd.getBack(),distSplit+DELTA_BSP)) return true;
			DBG(RRReporter::report(INF1," kd back+FRONT\n"));
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			if (t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,t->kd.getFront(),distanceMax);
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	// BSP
	FILL_STATISTIC(intersectStats.intersect_bsp++);

	typename BspTree::Son* front=(typename BspTree::Son*)(t+1);
	typename BspTree::Son* back=(typename BspTree::Son*)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	RR_ASSERT(triangle<t->getTrianglesEnd());

	RRMesh::TriangleBody t2;
	importer->getTriangleBody(triangle->getTriangleIndex(),t2);
	RRVec4 n;
	n.x = t2.side1[1] * t2.side2[2] - t2.side1[2] * t2.side2[1];
	n.y = t2.side1[2] * t2.side2[0] - t2.side1[0] * t2.side2[2];
	n.z = t2.side1[0] * t2.side2[1] - t2.side1[1] * t2.side2[0];
	n.w = -(t2.vertex0[0] * n.x + t2.vertex0[1] * n.y + t2.vertex0[2] * n.z);

	/* Reference. Old well tested code.
	float distanceMinLocation = // +=point at distanceMin is in front, -=back, 0=plane
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3];
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	bool frontback = (distanceMinLocation>0)  // point at distanceMin is in front
		|| (distanceMinLocation==0 && nonz<0); // point at distanceMin is in plane and rayDir is from front to back
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	*/
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
			if (t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
			t=(BspTree*)front;
		} else {
			DBG(RRReporter::report(INF1," bsp back\n"));
			if (!t->bsp.back) return false;
			DBG(RRReporter::report(INF1," bsp BACK\n"));
			if (t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
			t=(BspTree*)back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled below
	if (_isnan(distancePlane)/*nonz==0*/) 
	{
		distancePlane = ray->hitDistanceMin;
		// this sequence of tests follows:
		// front: hitDistanceMin..hitDistanceMin+DELTA_BSP
		// plane
		// back: hitDistanceMin-DELTA_BSP..hitDistanceMax
	}

	// test first half
	if (frontback)
	{
		DBG(RRReporter::report(INF1," bsp FRONT+back\n"));
		if (t->bsp.front && intersect_bsp_type(BspTree::Son,ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		DBG(RRReporter::report(INF1," bsp BACK+front\n"));
		if (t->bsp.back && intersect_bsp_type(BspTree::Son,ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	DBG(RRReporter::report(INF1," bsp plane\n"));
	void* trianglesEnd=t->getTrianglesEnd();
	while (triangle<trianglesEnd)
	{
		importer->getTriangleBody(triangle->getTriangleIndex(),t2);
		if (intersect_triangle(ray,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = triangle->getTriangleIndex();
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
#ifdef FILL_HITPLANE
			if (ray->rayFlags&RRRay::FILL_PLANE)
			{
				real siz = size(RRVec3(n));
				ray->hitPlane[0] = n.x/siz;
				ray->hitPlane[1] = n.y/siz;
				ray->hitPlane[2] = n.z/siz;
				ray->hitPlane[3] = n.w/siz;
			}
#endif
#ifdef FILL_HITPOINT3D
			if (ray->rayFlags&RRRay::FILL_POINT3D)
			{
				update_hitPoint3d(ray,distancePlane);
			}
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
		DBG(RRReporter::report(INF1," bsp front+BACK pokus\n"));
		if (!t->bsp.back) return false;
		DBG(RRReporter::report(INF1," bsp front+BACK go\n"));
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if (t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
		t=(BspTree*)back;
	} else {
		DBG(RRReporter::report(INF1," bsp back+FRONT pokus\n"));
		if (!t->bsp.front) return false;
		DBG(RRReporter::report(INF1," bsp back+FRONT go\n"));
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if (t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
		t=(BspTree*)front;
	}
	goto begin;
}

template IBP
IntersectBspCompact IBP2::IntersectBspCompact(const RRMesh* aimporter, IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, const char* ext, BuildParams* buildParams) : IntersectLinear(aimporter)
{
	tree = load IBP2(aimporter,aborting,cacheLocation,ext,buildParams,this);
	if (!tree) return;
}

template IBP
unsigned IntersectBspCompact IBP2::getMemoryOccupied() const
{
	return sizeof(IntersectBspCompact)
		+ (tree?tree->bsp.size:0);
}

template IBP
bool IntersectBspCompact IBP2::intersect(RRRay* ray) const
{
	RR_ASSERT(tree);
	FILL_STATISTIC(intersectStats.intersect_mesh++);
	bool hit = false;

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
		hit = intersect_bsp(ray,tree,ray->hitDistanceMax);
	}

#ifdef COLLISION_HANDLER
	if (ray->collisionHandler)
		hit = ray->collisionHandler->done();
#endif

	FILL_STATISTIC(if (hit) intersectStats.hit_mesh++);
	return hit;
}

template IBP
IntersectBspCompact IBP2::~IntersectBspCompact()
{
	free((void*)tree);
}

// explicit instantiation

// multi-level bsp
template class IntersectBspCompact< BspTree14>;
template class IntersectBspCompact<CBspTree24>;
template class IntersectBspCompact<CBspTree44>; // for 65537..2^32 triangles

template class IntersectBspCompact< BspTree12>;
template class IntersectBspCompact<CBspTree22>;
template class IntersectBspCompact<CBspTree42>; // for 257..65536 triangles

template class IntersectBspCompact< BspTree11>;
template class IntersectBspCompact<CBspTree21>; // for 0..256 triangles

} // namespace
