#include "IntersectKd.h"

#ifdef USE_KD

#include <assert.h>
#include <math.h>
#include <malloc.h>
#include <stdio.h>

namespace rrIntersect
{

#define DBG(a) //a
#define I_DBG(a) //a

struct KdTree
{
	U32       size:30;  //< size of this tree in bytes
	U32       axis:2;   //< splitting axis, 0=x, 1=y, 2=z, 3=no more splitting
	union {
		U32       splitVertexNum;     //< !isLeaf -> index into vertex array, vertex that defines splitting plane
		real      splitValue;         //< !isLeaf -> value readen from splitVertex for speed
		U32       leafTriangleNum[1]; //< isLeaf -> (size-sizeof(size))/sizeof(data[0]) indices into triangle array
		Triankle* leafTrianglePtr[1]; //< isLeaf -> leafTriangleNum converted to pointers for speed
	};
	bool      isLeaf()         {return axis==3;}
	KdTree*   next()           {return (KdTree *)((char *)this+size);}
	KdTree*   getFrontAdr()    {return this+1;}
	KdTree*   getFront()       {return isLeaf()?NULL:getFrontAdr();}
	KdTree*   getBackAdr()     {return (KdTree *)(isLeaf()?NULL:((char*)getFrontAdr()+getFrontAdr()->size));}
	KdTree*   getBack()        {return isLeaf()?NULL:getBackAdr();}
	Triankle**getTriangles()   {return leafTrianglePtr;}
	void*     getTrianglesEnd(){return (char*)this+size;}
};

#define EPSILON 0.000001

static inline bool intersect_triangle_kd(Triankle *t,real distanceMin,real distanceMax)
// input:                t, i_eye, i_direction, distMin, distMax
// returns:              true if ray hits t in distance <distMin,distMax>
// modifies when hit:    i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide, i_hitDistance
// modifies when no hit: <nothing is changed>
{
	assert(t!=i_skip);
	intersectStats.tri++;
	if(!t) return false; // when invalid triangle was removed at load-time, we may get NULL here
	assert(t);
	I_DBG(i_stat[0]++);
	//if(!t->surface) return false;
	I_DBG(i_stat[1]++);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec=ortogonalTo(i_direction,t->l3);
	real det = scalarMul(t->r3,pvec);

	// cull test
	bool hitOuterSide=det>0;
	//if (!sideBits[1/*t->surface->sides*/][hitOuterSide?0:1].catchFrom) return false;
	I_DBG(i_stat[2]++);

	// if determinant is near zero, ray lies in plane of triangle
	if (det>-EPSILON && det<EPSILON) return false;
	I_DBG(i_stat[3]++);

	// calculate distance from vert0 to ray origin
	Vec3 tvec=i_eye-t->s3;

	// calculate U parameter and test bounds
	real u=scalarMul(tvec,pvec)/det;
	if (u<0 || u>1) return false;
	I_DBG(i_stat[4]++);

	// prepare to test V parameter
	Vec3 qvec=ortogonalTo(tvec,t->r3);

	// calculate V parameter and test bounds
	real v=scalarMul(i_direction,qvec)/det;
	if (v<0 || u+v>1) return false;
	I_DBG(i_stat[5]++);

	// calculate distance where ray intersects triangle
	real dist=scalarMul(t->l3,qvec)/det;
	if(dist<distanceMin || dist>distanceMax) return false;
	I_DBG(i_stat[6]++);

	i_hitDistance=dist;
	i_hitU=u;
	i_hitV=v;
	i_hitOuterSide=hitOuterSide;
	i_hitTriangle=t;
	return true;
}

static bool intersect_kd(KdTree *t,real distanceMax)
// input:                t, i_eye, i_direction, i_skip, i_distanceMin=0, distanceMax
// returns:              true if ray hits t
// modifies when hit:    i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide, i_hitDistance
// modifies when no hit: <nothing is changed>
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	intersectStats.kd++;
	assert(ray);
	assert(t);
	assert(i_distanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

	// test leaf
	if(t->isLeaf()) 
	{
		void *trianglesEnd=t->getTrianglesEnd();
		bool hit=false;
		for(Triankle **triangle=t->getTriangles();triangle<trianglesEnd;triangle++)
		//if((*triangle)->intersectionTime!=__shot)
		if(*triangle!=i_skip)
		{
			//(*triangle)->intersectionTime=__shot;
			if(intersect_triangle_kd(*triangle,i_distanceMin,distanceMax)) 
			{
				distanceMax=MIN(distanceMax,i_hitDistance);
				hit=true;
			}
		}
		return hit;
	}

	// test subtrees
	real splitValue=t->splitValue;
	real pointMinVal=i_eye[t->axis]+i_direction[t->axis]*i_distanceMin;
	real pointMaxVal=i_eye[t->axis]+i_direction[t->axis]*distanceMax;
	if(pointMinVal>splitValue) {
		// front only
		if(pointMaxVal>=splitValue) {
			t=t->getFront();
			goto begin;
		}
		// front and back
		real distSplit=(splitValue-i_eye[t->axis])/i_direction[t->axis];
		if(intersect_kd(t->getFront(),distSplit)) return true;
		i_distanceMin=distSplit;
		t=t->getBack();
		goto begin;
	} else {
		// back only
		// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
		if(pointMaxVal<=splitValue) { // catches also i_direction[t->axis]==0 case
			t=t->getBack();
			goto begin;
		}
		// back and front
		real distSplit=(splitValue-i_eye[t->axis])/i_direction[t->axis];
		if(intersect_kd(t->getBack(),distSplit)) return true;
		i_distanceMin=distSplit;
		t=t->getFront();
		goto begin;
	}
}

if(kdTree)
{
	i_skip=skip;
	i_distanceMin=0;
	//real distanceMax=bbox.GetMaxDistance(i_eye);
	//distanceMax=MIN(distanceMax,*hitDistance);
	real distanceMax=1e5;
	hit=intersect_kd(kdTree,distanceMax);
}

} // namespace

#endif
