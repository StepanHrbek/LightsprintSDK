#include "IntersectLinear.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

namespace rrIntersect
{

#define DBG(a) //a

void TriangleP::setGeometry(Point3 *a,Point3 *b,Point3* c)
{
	// set s3,r3,l3
	Point3 s3=*a;
	Point3 r3=*b-*a;
	Point3 l3=*c-*a;

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

void TriangleNP::setGeometry(Point3 *a,Point3 *b,Point3* c)
{
	// set s3,r3,l3
	Point3 s3=*a;
	Point3 r3=*b-*a;
	Point3 l3=*c-*a;

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
	n3.d=-scalarMul(s3,n3);

	#ifdef TEST_SCENE
	if(!IS_VEC3(n3)) {
	  return -3; // throw out degenerated triangle
	}
	if(!IS_VEC3(v3)) {
	  return -10; // throw out degenerated triangle
	}
	#endif
}

void TriangleSRLNP::setGeometry(Point3 *a,Point3 *b,Point3* c)
{
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
	n3.d=-scalarMul(s3,n3);

	#ifdef TEST_SCENE
	if(!IS_VEC3(n3)) {
	  return -3; // throw out degenerated triangle
	}
	if(!IS_VEC3(v3)) {
	  return -10; // throw out degenerated triangle
	}
	#endif
}


// global variables used only by intersections to speed up recursive calls
Point3    i_eye;
Vec3      i_direction;
real      i_hitDistance;
bool      i_hitOuterSide;
real      i_hitU;
real      i_hitV;
Point3    i_hitPoint3d;

real intersect_plane_distance(Normal n)
{
	// return -normalValueIn(n,i_eye)/scalarMul(n,i_direction);
	// compilers don't tend to inline scalarMul, we must do it ourselves (-1.5%cpu)
	return -normalValueIn(n,i_eye)/(n.x*i_direction.x+n.y*i_direction.y+n.z*i_direction.z);
}

bool intersect_triangleSRLNP(TriangleSRLNP *t)
// input:                t, i_hitPoint3d, i_direction
// returns:              true if i_hitPoint3d is inside t
//                       if i_hitPoint3d is outside t plane, resut is undefined
// modifies when hit:    i_hitU, i_hitV, i_hitOuterSide
// modifies when no hit: <nothing is changed>
{
	intersectStats.tri++;
	assert(t);
	//if(!t->surface) return false;
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
	//if (!sideBits[1/*t->surface->sides*/][hitOuterSide?0:1].catchFrom) return false;
	i_hitOuterSide=hitOuterSide;
	i_hitU=u;
	i_hitV=v;
	return true;
}

bool intersect_triangleNP(TriangleNP *t, RRObjectImporter::TriangleSRL* t2)
{
	intersectStats.tri++;
	assert(t);
	//if(!t->surface) return false;
	real u,v;

	//RRObjectImporter::TriangleSRL t2;
	//getTriangleSRL(i,&t2);

	switch(t->intersectByte)
	{
		#define CASE(X,Y,Z) v=((i_hitPoint3d[Y]-t2->s[Y])*t2->r[X]-(i_hitPoint3d[X]-t2->s[X])*t2->r[Y]) / t->intersectReal;                if (v<0 || v>1) return false;                u=(i_hitPoint3d[Z]-t2->s[Z]-t2->l[Z]*v)/t2->r[Z];                break
		case 0:CASE(0,1,2);
		case 3:CASE(0,1,0);
		case 6:CASE(0,1,1);
		case 1:CASE(1,2,2);
		case 4:CASE(1,2,0);
		case 7:CASE(1,2,1);
		case 2:CASE(2,0,2);
		case 5:CASE(2,0,0);
		case 8:CASE(2,0,1);
		default: assert(0); return false;
		#undef CASE
	}
	if (u<0 || u+v>1) return false;

	bool hitOuterSide=sizeSquare(i_direction-t->n3)>2;
	//if (!sideBits[1/*t->surface->sides*/][hitOuterSide?0:1].catchFrom) return false;
	i_hitOuterSide=hitOuterSide;
	i_hitU=u;
	i_hitV=v;
	return true;
}

bool intersect_triangleP(TriangleP *t, RRObjectImporter::TriangleSRLN* t2)
{
	intersectStats.tri++;
	assert(t);
	//if(!t->surface) return false;
	real u,v;

	//RRObjectImporter::TriangleSRLN t2;
	//getTriangleSRLN(i,&t2);

	switch(t->intersectByte)
	{
#define CASE(X,Y,Z) v=((i_hitPoint3d[Y]-t2->s[Y])*t2->r[X]-(i_hitPoint3d[X]-t2->s[X])*t2->r[Y]) / t->intersectReal;                if (v<0 || v>1) return false;                u=(i_hitPoint3d[Z]-t2->s[Z]-t2->l[Z]*v)/t2->r[Z];                break
		case 0:CASE(0,1,2);
		case 3:CASE(0,1,0);
		case 6:CASE(0,1,1);
		case 1:CASE(1,2,2);
		case 4:CASE(1,2,0);
		case 7:CASE(1,2,1);
		case 2:CASE(2,0,2);
		case 5:CASE(2,0,0);
		case 8:CASE(2,0,1);
		default: assert(0); return false;
#undef CASE
	}
	if (u<0 || u+v>1) return false;

	bool hitOuterSide=sizeSquare(i_direction-*(Vec3*)(t2->n))>2;
	//if (!sideBits[1/*t->surface->sides*/][hitOuterSide?0:1].catchFrom) return false;
	i_hitOuterSide=hitOuterSide;
	i_hitU=u;
	i_hitV=v;
	return true;
}

IntersectLinear::IntersectLinear(RRObjectImporter* aimporter)
{
	importer = aimporter;
	triangles = importer->getNumTriangles();
	triangleP = NULL;
	triangleNP = NULL;
	triangleSRLNP = NULL;
	if(importer->fastSRLN) triangleP = new TriangleP[triangles]; else
	if(importer->fastSRL) triangleNP = new TriangleNP[triangles]; else
		triangleSRLNP = new TriangleSRLNP[triangles];
	for(unsigned i=0;i<triangles;i++)
	{
		unsigned v0,v1,v2,s;
		importer->getTriangle(i,v0,v1,v2,s);
		Point3 p0,p1,p2;
		importer->getVertex(v0,p0.x,p0.y,p0.z);
		importer->getVertex(v1,p1.x,p1.y,p1.z);
		importer->getVertex(v2,p2.x,p2.y,p2.z);
		if(triangleP) triangleP[i].setGeometry(&p0,&p1,&p2);
		if(triangleNP) triangleNP[i].setGeometry(&p0,&p1,&p2);
		if(triangleSRLNP) triangleSRLNP[i].setGeometry(&p0,&p1,&p2);
	}
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool IntersectLinear::intersect(RRRay* ray)
{
	DBG(printf("\n"));
	intersectStats.shots++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	i_eye         = *(Point3*)(ray->rayOrigin);
	i_direction   = *((Point3*)(ray->rayDir));
	i_hitDistance = ray->hitDistanceMax;

	bool hit = false;
	assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir
	for(unsigned t=0;t<triangles;t++) if(t!=ray->skip)
	{
		// 100% speed using no-precalc intersect
		//hit|=intersect_triangle_kd(&triangle[t],0,i_hitDistance);
		// 170% speed using with-precalc intersect
		if(importer->fastSRLN)
		{
			RRObjectImporter::TriangleSRLN t2;
			importer->getTriangleSRLN(t,&t2);
			real distance=intersect_plane_distance(*(Normal*)t2.n);
			if(distance>0 && distance<i_hitDistance)
			{
				DBG(printf("."));
				i_hitPoint3d=i_eye+i_direction*distance;
				if(intersect_triangleP(&triangleP[t],&t2))
				{
					hit = true;
					ray->hitTriangle = t;
					ray->hitDistance = distance;
					DBG(printf("%d",t));
				}
			}			
		} else
		if(importer->fastSRL) 
		{
			real distance=intersect_plane_distance(triangleNP[t].n3);
			if(distance>0 && distance<i_hitDistance)
			{
				DBG(printf("."));
				i_hitPoint3d=i_eye+i_direction*distance;
				RRObjectImporter::TriangleSRL t2;
				importer->getTriangleSRL(t,&t2);
				if(intersect_triangleNP(&triangleNP[t],&t2))
				{
					hit = true;
					ray->hitTriangle = t;
					ray->hitDistance = distance;
					DBG(printf("%d",t));
				}
			}			
		} else {
			real distance=intersect_plane_distance(triangleSRLNP[t].n3);
			if(distance>0 && distance<i_hitDistance)
			{
				DBG(printf("."));
				i_hitPoint3d=i_eye+i_direction*distance;
				if(intersect_triangleSRLNP(&triangleSRLNP[t]))
				{
					hit = true;
					ray->hitTriangle = t;
					ray->hitDistance = distance;
					DBG(printf("%d",t));
				}
			}			
		}
	}
	DBG(printf(hit?"\n":"NOHIT\n"));

	if(hit)
	{
		ray->hitU = i_hitU;
		ray->hitV = i_hitV;
		ray->hitOuterSide = i_hitOuterSide;
	}

	return hit;
}

IntersectLinear::~IntersectLinear()
{
	delete[] triangleP;
	delete[] triangleNP;
	delete[] triangleSRLNP;
}

}
