#include "IntersectVerification.h"

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

namespace rrCollider
{

IntersectVerification::IntersectVerification(RRMeshImporter* importer) 
	: IntersectLinear(importer)
{
	for(unsigned i=0;i<IT_VERIFICATION;i++)
		collider[i] = RRCollider::create(importer,(RRCollider::IntersectTechnique)i);
	unsigned vertices = importer->getNumVertices();
}

IntersectVerification::~IntersectVerification()
{
	for(unsigned i=0;i<IT_VERIFICATION;i++)
		delete collider[i];
}

unsigned IntersectVerification::getMemoryOccupied() const
{
	unsigned sum = 0;
	for(unsigned i=0;i<IT_VERIFICATION;i++)
		sum += collider[i]->getMemoryOccupied();
	return sum;
}

bool IntersectVerification::intersect(RRRay* ray) const
{
	RRRay* rayArray = RRRay::create(IT_VERIFICATION);
	bool hit[IT_VERIFICATION];
	bool diffHit = false;
	bool diffTriangle = false;
	bool diffSide = false;
	bool diffDistance = false;
	bool diffPlane = false;
	bool diffPoint2d = false;
	bool diffPoint3d = false;
	bool diffAny = false;
	for(unsigned i=0;i<IT_VERIFICATION;i++)
	{
		rayArray[i] = *ray;
		hit[i] = collider[i]->intersect(rayArray+i);
		if(hit[i]!=hit[0]) diffHit = true;
		if(i!=0 && hit[i] && hit[0])
		{
			real delta;
#define DIFFERS(a,b) ( (a!=b) && fabs(a-b)>delta )
			if(ray->rayFlags&RRRay::FILL_TRIANGLE) if(rayArray[i].hitTriangle!=rayArray[0].hitTriangle) diffTriangle = true;
			if(ray->rayFlags&RRRay::FILL_SIDE)     if(rayArray[i].hitOuterSide!=rayArray[0].hitOuterSide) diffSide = true;
			if(ray->rayFlags&RRRay::FILL_DISTANCE) {delta=10*DELTA_BSP;if(DIFFERS(rayArray[i].hitDistance,rayArray[0].hitDistance)) diffDistance = true;}
			if(ray->rayFlags&RRRay::FILL_POINT2D)  {delta=0.001f;if(DIFFERS(rayArray[i].hitPoint2d[0],rayArray[0].hitPoint2d[0]) || DIFFERS(rayArray[i].hitPoint2d[1],rayArray[0].hitPoint2d[1])) diffPoint2d = true;}
			if(ray->rayFlags&RRRay::FILL_POINT3D)  {delta=10*DELTA_BSP;if(DIFFERS(rayArray[i].hitPoint3d[0],rayArray[0].hitPoint3d[0]) || DIFFERS(rayArray[i].hitPoint3d[1],rayArray[0].hitPoint3d[1]) || DIFFERS(rayArray[i].hitPoint3d[2],rayArray[0].hitPoint3d[2])) diffPoint3d = true;}
			if(ray->rayFlags&RRRay::FILL_PLANE)    {delta=0.001f;if(DIFFERS(rayArray[i].hitPlane[0],rayArray[0].hitPlane[0]) || DIFFERS(rayArray[i].hitPlane[1],rayArray[0].hitPlane[1]) || DIFFERS(rayArray[i].hitPlane[2],rayArray[0].hitPlane[2]) || DIFFERS(rayArray[i].hitPlane[3],rayArray[0].hitPlane[3])) diffPlane = true;}
#undef DIFFERS
		}
	}
	if(diffHit) {diffAny = true; printf("%c%c%c%c ",hit[0]?'+':'-',hit[1]?'+':'-',hit[2]?'+':'-',hit[3]?'+':'-');}
#define TEST_DIFF(diff,prefix,mask,expr) if(diff) {diffAny = true; printf(prefix); for(unsigned i=0;i<IT_VERIFICATION;i++) printf(mask "%c",expr,i<IT_VERIFICATION-1?'/':' ');}
	TEST_DIFF(diffTriangle,"tri=","%d",hit[i]?rayArray[i].hitTriangle:0);
	TEST_DIFF(diffDistance,"dist=","%f",hit[i]?rayArray[i].hitDistance:0);
	TEST_DIFF(diffSide,"side=","%c",hit[i]?(rayArray[i].hitOuterSide?'o':'i'):'-');
	for(unsigned j=0;j<2;j++) TEST_DIFF(diffPoint2d,"2d=","%f",hit[i]?rayArray[i].hitPoint2d[j]:0);
	for(unsigned j=0;j<3;j++) TEST_DIFF(diffPoint3d,"3d=","%f",hit[i]?rayArray[i].hitPoint3d[j]:0);
	for(unsigned j=0;j<4;j++) TEST_DIFF(diffPlane,"plane=","%f",hit[i]?rayArray[i].hitPlane[j]:0);
#undef TEST_DIFF
	if(diffAny) printf("\n");
	*ray = rayArray[0];
	delete[] rayArray;
	return hit[0];
}

} // namespace
