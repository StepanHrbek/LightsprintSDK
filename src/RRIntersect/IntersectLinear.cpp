#include "IntersectLinear.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define DBG(a) //a

void Triankle::setGeometry(Point3 *a,Point3 *b,Point3* c)
{
	Point3* vertex[3];
	vertex[0]=a;
	vertex[1]=b;
	vertex[2]=c;

	// set s3,r3,l3
	s3=*vertex[0];
	r3=*vertex[1]-*vertex[0];
	l3=*vertex[2]-*vertex[0];

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
Triankle *i_skip;
Point3    i_eye;
Vec3      i_direction;
real      i_distanceMin; // bsp: starts as 0, may only increase during bsp traversal
Point3    i_eye2;        // bsp: precalculated i_eye+i_direction*i_distanceMin
real      i_hitDistance;
bool      i_hitOuterSide;
Triankle *i_hitTriangle;
real      i_hitU;
real      i_hitV;
Point3    i_hitPoint3d;

real intersect_plane_distance(Normal n)
{
	// return -normalValueIn(n,i_eye)/scalarMul(n,i_direction);
	// compilers don't tend to inline scalarMul, we must do it ourselves (-1.5%cpu)
	return -normalValueIn(n,i_eye)/(n.x*i_direction.x+n.y*i_direction.y+n.z*i_direction.z);
}

bool intersect_triangle_bsp(Triankle *t)
// input:                t, i_hitPoint3d, i_direction
// returns:              true if i_hitPoint3d is inside t
//                       if i_hitPoint3d is outside t plane, resut is undefined
// modifies when hit:    i_hitTriangle, i_hitU, i_hitV, i_hitOuterSide
// modifies when no hit: <nothing is changed>
{
	assert(t!=i_skip);
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
	i_hitTriangle=t;
	i_hitU=u;
	i_hitV=v;
	return true;
}

IntersectLinear::IntersectLinear(RRObjectImporter* aimporter)
{
	importer = aimporter;
	triangles = importer->getNumTriangles();
	triangle = new Triankle[triangles];
	for(unsigned i=0;i<triangles;i++)
	{
		unsigned v0,v1,v2,s;
		importer->getTriangle(i,v0,v1,v2,s);
		Point3 p0,p1,p2;
		importer->getVertex(v0,p0.x,p0.y,p0.z);
		importer->getVertex(v1,p1.x,p1.y,p1.z);
		importer->getVertex(v2,p2.x,p2.y,p2.z);
		triangle[i].setGeometry(&p0,&p1,&p2);
	}
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triankle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool IntersectLinear::intersect(RRRay* ray, RRHit* hit)
{
	Point3 eye = *(Point3*)(&ray->ex);
	Vec3 direction = *((Point3*)(&ray->dx));

	DBG(printf("\n"));
	intersectStats.shots++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
#ifdef TRANSFORM_SHOTS
	// transform from scenespace to objectspace
	i_eye            =eye.transformed(inverseMatrix);
	i_direction      =(eye+direction).transformed(inverseMatrix)-i_eye;
#else
	i_eye            =eye;
	i_direction      =direction;
#endif

	bool result=false;
	assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir
	i_hitDistance=ray->distanceMax;
	for(unsigned t=0;t<triangles;t++)
		if(t!=ray->skip)
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
					result=true;
					i_hitDistance=distance;
					DBG(printf("%d",t));
				}
			}			
		}
		DBG(printf(hit?"\n":"NOHIT\n"));

	if(result)
	{
		//!!!assert(i_hitTriangle->u2.y==0);
#ifdef HITS_FIXED
		hitPoint2d->u=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitU);
		hitPoint2d->v=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitV);
#else
		// prepocet u,v ze souradnic (rightside,leftside)
		//  do *hitPoint2d s ortonormalni bazi (u3,v3)
		//!!!hitPoint2d->u=i_hitU*i_hitTriangle->u2.x+i_hitV*i_hitTriangle->v2.x;
		//hitPoint2d->v=i_hitV*i_hitTriangle->v2.y;
		hit->u = i_hitU;
		hit->v = i_hitV;
#endif
		assert(fabs(sizeSquare(i_direction)-1)<0.001);//ocekava normalizovanej dir
		hit->outerSide = i_hitOuterSide;
		hit->triangle = (unsigned)(((U64)i_hitTriangle-(U64)triangle))/sizeof(Triankle);
		hit->distance = i_hitDistance;
	}

	return result;
}

IntersectLinear::~IntersectLinear()
{
	delete[] triangle;
}
