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

// global variables used only by intersections to speed up recursive calls

static Triangle *i_skip;
static Point3    i_eye;
static Vec3      i_direction;
static real      i_distanceMin; // bsp: starts as 0, may only increase during bsp traversal
static Point3    i_eye2;        // bsp: precalculated i_eye+i_direction*i_distanceMin
static real      i_hitDistance;
static bool      i_hitOuterSide;
static Triangle *i_hitTriangle;
static real      i_hitU;
static real      i_hitV;
static Point3    i_hitPoint3d;

unsigned  i_stat[8]={0,0,0,0,0,0,0,0};
unsigned __shot=0,__bsp=0,__kd=0,__tri=0;

#define I_DBG(a)

void i_dbg_print()
{
	I_DBG(printf("intersect_bsp stat:"));
	I_DBG(for(int i=0;i<6;i++) if(i_stat[i]) printf(" %d%%",i_stat[i+1]*100/i_stat[i]));
	I_DBG(printf("\n"));
}

static inline bool intersect_triangle_bsp(Triangle *t)
// input:                t, i_hitPoint3d, i_direction
// returns:              true if i_hitPoint3d is inside t
//                       if i_hitPoint3d is outside t plane, resut is undefined
// modifies when hit:    i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide
// modifies when no hit: <nothing is changed>
{
	assert(t!=i_skip);
	__tri++;
	assert(t);
	if(!t->surface) return false;
	real u,v;

		/* bounding box test
#define LT(i,x,e) (i_hitPoint3d.x<t->vertex[i]->x+(e))
#define E 0.0001
#define TEST(x) {if((LT(0,x,-E) && LT(1,x,-E) && LT(0,x,-E)) || (!LT(0,x,+E) && !LT(1,x,+E) && !LT(0,x,+E))) return false;}
		TEST(x);
		TEST(y);
		TEST(z);*/

	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((i_hitPoint3d.Y-t->s3.Y)*t->r3.X-(i_hitPoint3d.X-t->s3.X)*t->r3.Y) / t->intersectReal;                if (v<0 || v>1) return false;                u=(i_hitPoint3d.Z-t->s3.Z-t->l3.Z*v)/t->r3.Z;                break
		case 0:CASE(x,y,z);
		case 3:CASE(x,y,x);
		case 6:CASE(x,y,y);
		case 1:CASE(y,z,z);
		case 4:CASE(y,z,x);
		case 7:CASE(y,z,y);
		case 2:CASE(z,x,z);
		case 5:CASE(z,x,x);
		case 8:CASE(z,x,y);
		default: assert(0); return false;
		#undef CASE
	}
	if (u<0 || u+v>1) return false;

	bool hitOuterSide=sizeSquare(i_direction-t->n3)>2;
	if (!sideBits[t->surface->sides][hitOuterSide?0:1].catchFrom) return false;
	i_hitOuterSide=hitOuterSide;
	i_hitTriangle=t;
	i_hitU=u;
	i_hitV=v;
	return true;
}


#define EPSILON 0.000001

static inline bool intersect_triangle_kd(Triangle *t,real distanceMin,real distanceMax)
// input:                t, i_eye, i_direction, distMin, distMax
// returns:              true if ray hits t in distance <distMin,distMax>
// modifies when hit:    i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide, i_hitDistance
// modifies when no hit: <nothing is changed>
{
	assert(t!=i_skip);
	__tri++;
	if(!t) return false; // when invalid triangle was removed at load-time, we may get NULL here
	assert(t);
	I_DBG(i_stat[0]++);
	if(!t->surface) return false;
	I_DBG(i_stat[1]++);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec=ortogonalTo(i_direction,t->l3);
	real det = scalarMul(t->r3,pvec);

	// cull test
	bool hitOuterSide=det>0;
	if (!sideBits[t->surface->sides][hitOuterSide?0:1].catchFrom) return false;
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


static inline real intersect_plane_distance(Normal n)
{
	// return -normalValueIn(n,i_eye)/scalarMul(n,i_direction);
	// compilers don't tend to inline scalarMul, we must do it ourselves (-1.5%cpu)
	return -normalValueIn(n,i_eye)/(n.x*i_direction.x+n.y*i_direction.y+n.z*i_direction.z);
}

#ifdef USE_BSP

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

static bool intersect_bsp(BspTree *t,real distanceMax)
// input:                t, i_eye, i_eye2=i_eye, i_direction, i_skip, i_distanceMin=0, distanceMax
// returns:              true if ray hits t
// modifies when hit:    (i_eye2, i_distanceMin, i_hitPoint3d) i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide, i_hitDistance
// modifies when no hit: (i_eye2, i_distanceMin, i_hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	__bsp++;
	assert(t);
	#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
	#endif

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	Triangle **triangle=(Triangle **)((char*)back+(t->back?back->size:0));
	assert(*triangle);
	#ifdef TEST_SCENE
	if (!*triangle) return false; // rovina zacina vyrazenym polygonem (nema zadne triangly)
	#endif
	Normal n=triangle[0]->n3;
	real distancePlane=intersect_plane_distance(n);
	bool frontback=normalValueIn(n,i_eye2)>0;

	// test only one half
	if (distancePlane<i_distanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bsp(front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bsp(back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	i_hitPoint3d=i_eye+i_direction*distancePlane;
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=i_skip /*&& (*triangle)->intersectionTime!=__shot*/ && intersect_triangle_bsp(*triangle))
		{
			i_hitDistance=distancePlane;
			return true;
		}
		//(*triangle)->intersectionTime=__shot;
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	i_distanceMin=distancePlane-DELTA_BSP;
	i_eye2=i_eye+i_direction*i_distanceMin; // precalculation helps -0.4%cpu
	goto begin;
}

#endif //USE_BSP

#ifdef USE_KD

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
	__kd++;
	assert(t);
	#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
	#endif
	assert(i_distanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

	// test leaf
	if(t->isLeaf()) {
		void *trianglesEnd=t->getTrianglesEnd();
		bool hit=false;
		for(Triangle **triangle=t->getTriangles();triangle<trianglesEnd;triangle++)
            //if((*triangle)->intersectionTime!=__shot)
			if(*triangle!=i_skip)
            {
        		//(*triangle)->intersectionTime=__shot;
				if(intersect_triangle_kd(*triangle,i_distanceMin,distanceMax)) {
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

#endif //USE_KD

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
#ifdef USE_RRINTERSECT
	RRHit hit2;
	bool res2;
	{RRRay ray;
	RRHit hit;
	ray.ex = eye.x;
	ray.ey = eye.y;
	ray.ez = eye.z;
	ray.dx = direction.x;
	ray.dy = direction.y;
	ray.dz = direction.z;
	ray.skip = ((unsigned)((U64)skip-(U64)triangle))/sizeof(Triangle);
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
#ifndef HITS_FIXED
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
	hit2=hit;res2=res;}
	//!!!
#endif // USE_RRINTERSECT

	DBG(printf("\n"));
	__shot++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
	#ifdef SUPPORT_DYNAMIC_AND_TRANSFORM_SHOTS
	// transform from scenespace to objectspace
	i_eye            =eye.transformed(inverseMatrix);
	i_direction      =(eye+direction).transformed(inverseMatrix)-i_eye;
	#else
	i_eye            =eye;
	i_direction      =direction;
	#endif

	bool hit=false;

	assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir

#ifdef USE_KD
	if(kdTree)
	{
		i_skip=skip;
		i_distanceMin=0;
		//real distanceMax=bbox.GetMaxDistance(i_eye);
		//distanceMax=MIN(distanceMax,*hitDistance);
		real distanceMax=1e5;
		hit=intersect_kd(kdTree,distanceMax);
	}
	else
#endif
#ifdef USE_BSP
	if(bspTree)
	{
		i_skip=skip;
		i_distanceMin=0;
		i_eye2=i_eye;
		hit=intersect_bsp(bspTree,*hitDistance);
	}
	else
#endif
	{
		i_hitDistance=*hitDistance;
		for(unsigned t=0;t<triangles;t++)
		  if(&triangle[t]!=skip)
		  {
			// 100% speed using no-precalc intersect
			//hit|=intersect_triangle_kd(&triangle[t],0,i_hitDistance);
			// 170% speed using with-precalc intersect
		    real distance=intersect_plane_distance(triangle[t].n3);
		    if(distance>0 && distance<i_hitDistance)
		    {
		      DBG(printf("."));
		      i_hitPoint3d=i_eye+i_direction*distance;
		      if(intersect_triangle_bsp(&triangle[t]))
		      {
		        hit=true;
		        i_hitDistance=distance;
		        DBG(printf("%d",t));
		      }
		    }
			
		  }
		  DBG(printf(hit?"\n":"NOHIT\n"));
	}

	if(hit)
	{
		assert(i_hitTriangle->u2.y==0);
#ifdef HITS_FIXED
		hitPoint2d->u=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitU);
		hitPoint2d->v=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitV);
#else
		// prepocet u,v ze souradnic (rightside,leftside)
		//  do *hitPoint2d s ortonormalni bazi (u3,v3)
		hitPoint2d->u=i_hitU*i_hitTriangle->u2.x+i_hitV*i_hitTriangle->v2.x;
		hitPoint2d->v=i_hitV*i_hitTriangle->v2.y;
#endif
		assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir
		*hitOuterSide=i_hitOuterSide;
		*hitTriangle =i_hitTriangle;
		*hitDistance =i_hitDistance;
	}

#ifdef USE_RRINTERSECT
	static unsigned qw=0;
	if(res2!=hit) qw++;
	assert(qw<10);
#endif
	return hit;
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
