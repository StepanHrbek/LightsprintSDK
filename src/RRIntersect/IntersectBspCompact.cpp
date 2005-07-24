#include "IntersectBspCompact.h"

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

namespace rrIntersect
{

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

template IBP
bool IntersectBspCompact IBP2::intersect_bsp(RRRay* ray, const BspTree* t, real distanceMax) const
{
begin:
	assert(ray);
	assert(t);

	// transition to sons with different size
	if(t->allows_transition && t->bsp.transition)
	{
		#define intersect_bsp_type_func(type,func,ray,tree,distanceMax) ((IntersectBspCompact<typename type>*)this)->func(ray,(typename type*)tree,distanceMax)
		#define intersect_bsp_type(type,ray,tree,distanceMax) intersect_bsp_type_func(type,intersect_bsp,ray,tree,distanceMax)
		return intersect_bsp_type(BspTree::Transitioneer,ray,t,distanceMax);
	}
	
	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			void *trianglesEnd=t->getTrianglesEnd();
			bool hit=false;
			for(typename BspTree::_TriInfo* triangle=t->kd.getTrianglesBegin();triangle<trianglesEnd;triangle++) if(*triangle!=ray->skipTriangle)
			{
				RRObjectImporter::TriangleSRL srl;
				importer->getTriangleSRL(*triangle,&srl);
				if(intersect_triangle(ray,&srl))
				{
					hit=true;
					ray->hitTriangle = *triangle;
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

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distanceMax);
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distanceMax);
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distanceMax);
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distanceMax);
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	// BSP
	intersectStats.intersect_bsp++;

	typename BspTree::Son* front=(typename BspTree::Son*)(t+1);
	typename BspTree::Son* back=(typename BspTree::Son*)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangle<t->getTrianglesEnd());

	RRObjectImporter::TriangleSRL t2;
	importer->getTriangleSRL(*triangle,&t2);
	Plane n;
	n.x = t2.r[1] * t2.l[2] - t2.r[2] * t2.l[1];
	n.y = t2.r[2] * t2.l[0] - t2.r[0] * t2.l[2];
	n.z = t2.r[0] * t2.l[1] - t2.r[1] * t2.l[0];
	n.d = -(t2.s[0] * n.x + t2.s[1] * n.y + t2.s[2] * n.z);

	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;

	// test only one half
	// distancePlane = 1/0 (ray parallel to plane) is handled here
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->bsp.front) return false;
			if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
			t=(BspTree*)front;
		} else {
			if(!t->bsp.back) return false;
			if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
			t=(BspTree*)back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled below
	if(_isnan(distancePlane)/*nonz==0*/) distancePlane = 0;

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bsp_type(BspTree::Son,ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bsp_type(BspTree::Son,ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skipTriangle && intersect_triangle(ray,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
#ifdef FILL_HITPLANE
			if(ray->flags&RRRay::FILL_PLANE)
			{
				real siz = size(n);
				ray->hitPlane[0] = n.x/siz;
				ray->hitPlane[1] = n.y/siz;
				ray->hitPlane[2] = n.z/siz;
				ray->hitPlane[3] = n.d/siz;
			}
#endif
#ifdef FILL_HITPOINT3D
			if(ray->flags&RRRay::FILL_POINT3D)
			{
				update_hitPoint3d(ray,distancePlane);
			}
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
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
		t=(BspTree*)back;
	} else {
		if(!t->bsp.front) return false;
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
		t=(BspTree*)front;
	}
	goto begin;
}

template IBP
IntersectBspCompact IBP2::IntersectBspCompact(RRObjectImporter* aimporter, IntersectTechnique intersectTechnique, const char* ext, BuildParams* buildParams) : IntersectLinear(aimporter)
{
	tree = load IBP2(aimporter,ext,buildParams,this);
	if(!tree) return;
}

template IBP
unsigned IntersectBspCompact IBP2::getMemorySize() const
{
	return sizeof(IntersectBspCompact)
		+ (tree?tree->bsp.size:0);
}

template IBP
bool IntersectBspCompact IBP2::intersect(RRRay* ray) const
{
	DBG(printf("\n"));
	intersectStats.intersects++;

	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	assert(tree);
	bool hit = sphere.intersect(ray) && box.intersect(ray) && intersect_bsp(ray,tree,ray->hitDistanceMax);
	if(hit) intersectStats.hits++;
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
