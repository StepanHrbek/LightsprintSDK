#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "core.h"
#include "intersections.h"

#define USE_BSP
//#define USE_KD
#define DBG(a) //a

//////////////////////////////////////////////////////////////////////////////
//
// intersections
//
unsigned __shot=0;

#ifdef SUPPORT_DYNAMIC
 #ifdef TRANSFORM_SHOTS
  #define SUPPORT_DYNAMIC_AND_TRANSFORM_SHOTS
 #endif
#endif

// return first intersection with object
// but not with *skip and not more far than *hitDistance

bool Object::intersection(Point3 eye,Vec3 direction,Triangle *skip,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{

	DBG(printf("\n"));
	__shot++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
#ifdef SUPPORT_DYNAMIC_AND_TRANSFORM_SHOTS
	// transform from scenespace to objectspace
	i_eye            =eye.transformed(inverseMatrix);
	i_direction      =(eye+direction).transformed(inverseMatrix)-i_eye;
#else
	//i_eye            =eye;
	//i_direction      =direction;
#endif
	assert(fabs(sizeSquare(direction)-1)<0.001);//ocekava normalizovanej dir


	RRRay ray;
	RRHit hit;
	ray.ex = eye.x;
	ray.ey = eye.y;
	ray.ez = eye.z;
	ray.dx = direction.x;
	ray.dy = direction.y;
	ray.dz = direction.z;
#ifdef _MSC_VER
	ray.skip = ((unsigned)((U64)skip-(U64)triangle))/sizeof(Triangle);
#else
	ray.skip = ((unsigned)((U32)skip-(U32)triangle))/sizeof(Triangle);
#endif
	ray.distanceMin = 0;
	ray.distanceMax = *hitDistance;
	assert(intersector);
	bool res = intersector->intersect(&ray,&hit);
	if(res)
	{
		assert(hit.triangle>=0 && hit.triangle<triangles);
		*hitTriangle = &triangle[hit.triangle];
		// compenasate for our rotations
		switch((*hitTriangle)->rotations)
		{
			case 0:
				break;
			case 1:
				{float u=hit.u;
				float v=hit.v;
				hit.u=v;
				hit.v=1-u-v;}
				break;
			case 2:
				{float u=hit.u;
				float v=hit.v;
				hit.u=1-u-v;
				hit.v=u;}
				break;
			default:
				assert(0);
		}
		assert((*hitTriangle)->u2.y==0);
#ifdef HITS_FIXED
		hitPoint2d->u=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitU);
		hitPoint2d->v=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitV);
#else
		// prepocet u,v ze souradnic (rightside,leftside)
		//  do *hitPoint2d s ortonormalni bazi (u3,v3)
		assert(hit.triangle>=0 && hit.triangle<triangles);
		hit.u=hit.u*triangle[hit.triangle].u2.x+hit.v*triangle[hit.triangle].v2.x;
		hit.v=hit.v*triangle[hit.triangle].v2.y;
#endif
		hitPoint2d->u = hit.u;
		hitPoint2d->v = hit.v;
		*hitOuterSide = hit.outerSide;
		*hitDistance = hit.distance;
	}
	return res;
}

// return first intersection with "scene minus *skip minus dynamic objects"

bool Scene::intersectionStatic(Point3 eye,Vec3 direction,Triangle *skip,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{
	assert(fabs(sizeSquare(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv

	bool hit=false;
	for(unsigned o=0;o<staticObjects;o++)
		if(object[o]->bound.intersect(eye,direction,*hitDistance))
			if(object[o]->intersection(eye,direction,skip,hitTriangle,hitPoint2d,hitOuterSide,hitDistance))
				 hit=true;
	return hit;
}

#ifdef SUPPORT_DYNAMIC

// return if intersection with object-skip is closer than hitDistance

bool Object::intersectionCloserThan(Point3 eye,Vec3 direction,Triangle *skip,real hitDistance)
{
	#ifdef SUPPORT_DYNAMIC_AND_TRANSFORM_SHOTS
	// transform from scenespace to objectspace
	i_eye            =eye.transformed(inverseMatrix);
	i_direction      =(eye+direction).transformed(inverseMatrix)-i_eye;
	#else
	i_eye            =eye;
	i_direction      =direction;
	#endif
	assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir

#ifdef USE_KD
	if(kdTree)
	{
		i_skip=skip;
		i_distanceMin=0;
		if(intersect_kd(kdTree,hitDistance)) return true;
	}
	else
#endif
#ifdef USE_BSP
	if(bspTree)
	{
		i_skip=skip;
		i_distanceMin=0;
		i_eye2=i_eye;
		if(intersect_bsp(bspTree,hitDistance)) return true;
	}
	else
#endif
	{
		for(unsigned t=0;t<triangles;t++)
		  if(&triangle[t]!=skip)
		  {
		    real distance=intersect_plane_distance(triangle[t].n3);
		    if(distance>0 && distance<hitDistance)
		    {
		      i_hitPoint3d=i_eye+i_direction*distance;
		      if(intersect_triangle(&triangle[t])) return true;
		    }
		  }
	}

	return false;
}

// return first intersection with scene
// but only when that intersection is with dynobj
// sideeffect: inserts hit to triangle and triangle to hitTriangles

bool Scene::intersectionDynobj(Point3 eye,Vec3 direction,Triangle *skip,TObject *dynobj,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{
	assert(fabs(sizeSquare(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv

	if(!dynobj->intersection(eye,direction,skip,hitTriangle,hitPoint2d,hitOuterSide,hitDistance))
		return false;
	for(unsigned o=0;o<objects;o++)
		if(object[o]!=dynobj && object[o]->bound.intersect(eye,direction,*hitDistance))
			if(object[o]->intersectionCloserThan(eye,direction,skip,*hitDistance))
				return false;

	// inserts hit triangle to hitTriangles
	if(!(*hitTriangle)->hits.hits) hitTriangles.insert(*hitTriangle);
	return true;
}

#endif
