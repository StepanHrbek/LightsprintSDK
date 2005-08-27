#include "IntersectBspFast.h"

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

namespace rrIntersect
{

#ifdef TEST
	bool     watch_tested = true;
	unsigned watch_triangle;
	real     watch_distance;
	Vec3     watch_point3d;
	#define TEST_RANGE(min,max,cond,tree) if(!watch_tested && min<=watch_distance && watch_distance<=max) \
		assert(cond && tree->contains(watch_triangle)); // optimal triangle is to be thrown away
#else
	#define TEST_RANGE(min,max,cond,tree)
#endif

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
	if(u<0 || u+v>1) return false;

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

bool intersect_triangleNP(RRRay* ray, const TriangleNP *t, const RRMeshImporter::TriangleSRL* t2)
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
	if(u<0 || u+v>1) return false;

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
	intersectStats.intersect_bspSRLNP++;
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
#ifdef SUPPORT_EMPTY_KDNODE
		if(t->kd.size<=sizeof(BspTree)) 
			return false; // only empty kdnode is so small
#endif

		//intersectStats.intersect_kd++;
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
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getFront());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getBack());
			if(intersect_bspSRLNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) return true;
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
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			TEST_RANGE(ray->hitDistanceMin,distSplit+DELTA_BSP,1,t->kd.getBack());
			TEST_RANGE(distSplit-DELTA_BSP,distanceMax,1,t->kd.getFront());
			if(intersect_bspSRLNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) return true;
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
	Plane& n=triangleSRLNP[triangle[0]].n3;
#define OPLANE
#ifdef OPLANE
	// 0plane: part done with point in distance 0, part in distance plane
	float distanceMinLocation = // +=point at distanceMin is in front, -=back, 0=plane
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3];
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	bool frontback = (distanceMinLocation>0)  // point at distanceMin is in front
		|| (distanceMinLocation==0 && nonz<0); // point at distanceMin is in plane and rayDir is from front to back
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
#else
	// 0: all done with point in distance 0
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	real distancePlaneScaled = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d);
	real distancePlane = distancePlaneScaled/nonz;
	//bool frontback2 = ((ray->hitDistanceMin>distancePlane) == (nonz>0)) // point at distanceMin is in front
	//	|| (nonz==0); // point at distanceMin is in plane and rayDir is from front to back
	bool frontback = (ray->hitDistanceMin*nonz>distancePlaneScaled) // point at distanceMin is in front
		|| (nonz==0); // point at distanceMin is in plane and rayDir is from front to back
#endif
	// number of clear_miss during 5 sec in cube debug
	// (switch 0/0plane/plane here, switch kd/bsp/kdbsp in bsp.h)
	//           kd   bsp  kdbsp
	// 0		  143    63
	// 0plane           7     4
	// plane

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
	if(_isnan(distancePlane)/*nonz==0*/) 
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
		if(t->bsp.front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
		TEST_RANGE(ray->hitDistanceMin,distancePlane+DELTA_BSP,t->bsp.front,front);
	} else {
		if(t->bsp.back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
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
		if(*triangle!=ray->skipTriangle && intersect_triangleSRLNP(ray,triangleSRLNP+*triangle))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			if(!ray->surfaceImporter || ray->surfaceImporter->acceptHit(ray)) return true;
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
				t = t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspNP(ray,t->kd.getFront(),distSplit+DELTA_BSP)) return true;
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
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspNP(ray,t->kd.getBack(),distSplit+DELTA_BSP)) return true;
			ray->hitDistanceMin = distSplit-DELTA_BSP;
			t = t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangleNP);
	Plane& n=triangleNP[triangle[0]].n3;
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
			t=front;
		} else {
			if(!t->bsp.back) return false;
			t=back;
		}
		goto begin;
	}

	// distancePlane = 0/0 (ray inside plane) is handled here
	if(_isnan(distancePlane)/*nonz==0*/) 
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
		if(t->bsp.front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRMeshImporter::TriangleSRL t2;
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skipTriangle && intersect_triangleNP(ray,triangleNP+*triangle,&t2))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
			if(!ray->surfaceImporter || ray->surfaceImporter->acceptHit(ray)) return true;
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
	DBG(printf("\n"));
	intersectStats.intersects++;

#ifdef TEST
	ray->flags |= RRRay::FILL_PLANE + RRRay::FILL_DISTANCE;
	RRRay rayOrig = *ray;
	RRRay ray2 = rayOrig;
	bool hit2 = test->intersect(&ray2);
#endif
	bool hit = false;
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	assert(tree);

	if((ray->flags&RRRay::SKIP_PRETESTS) || (
#ifdef USE_SPHERE
		sphere.intersect(ray) &&
#endif
		box.intersect(ray)))
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
#ifdef TEST
	if(hit!=hit2 || (hit && hit2 && ray->hitTriangle!=ray2.hitTriangle))
	{
		const float delta = 0.001f;
		watch_tested = true;
		if(hit && hit2 && ray2.hitDistance==ray->hitDistance)
		{
			intersectStats.diff_overlap++;
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
				intersectStats.diff_precalc_miss++;
				goto ok;
			}*/
			if(hitBorder)
			{
				intersectStats.diff_tight_hit++;
				goto ok;
			}
			if(hitParallel)
			{
				intersectStats.diff_parallel_hit++;
				goto ok;
			}
			{
				intersectStats.diff_clear_hit++;
				goto bad;
			}
		}
		if(hit2 && (!hit || ray->hitDistance>ray2.hitDistance)) 
		{
			bool hit2Border = ray2.hitPoint2d[0]<delta || ray2.hitPoint2d[1]<delta || ray2.hitPoint2d[0]+ray2.hitPoint2d[1]>1-delta;
			real tmp = ray2.rayDir[0]*ray2.hitPlane[0]+ray2.rayDir[1]*ray2.hitPlane[1]+ray2.rayDir[2]*ray2.hitPlane[2];
			bool hit2Parallel = fabs(tmp)<delta;
			RRRay ray5 = rayOrig;
			update_hitPoint3d(&ray5,ray2.hitDistance);
			bool bspAbleToHit = intersect_triangleSRLNP(&ray5,triangleSRLNP+ray2.hitTriangle);
			if(!bspAbleToHit)
			{
				// precalculated intersect_triangleSRLNP gives different result -> miss
				intersectStats.diff_precalc_miss++;
				goto ok;
			}
			if(hit2Border)
			{
				// extremely small numerical inprecisions -> tight miss
				intersectStats.diff_tight_miss++;
				goto ok;
			}
			if(hit2Parallel)
			{
				// ray is parallel to plane + extremely small numerical inprecisions -> miss
				intersectStats.diff_parallel_miss++;
				goto ok;
			}
			{
				// unknown problem -> miss
				intersectStats.diff_clear_miss_tested++;
				watch_tested = false;
				watch_triangle = ray2.hitTriangle;
				watch_distance = ray2.hitDistance;
				goto bad;
			}
		}
		assert(0);
	bad:
		{RRRay ray3 = rayOrig;
		intersect_bspSRLNP(&ray3,tree,ray3.hitDistanceMax);
		if(!watch_tested)
		{
			intersectStats.diff_clear_miss_tested--;
			intersectStats.diff_clear_miss_not_tested++;
			watch_tested = true;
		}
		RRRay ray4 = rayOrig;
		test->intersect(&ray4);}
	ok:;
	}
#endif

	if(hit) intersectStats.hits++;
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
template class IntersectBspFast<BspTree44>; // for size 0..2^30-1         [65537..2^32 triangles]

} // namespace
