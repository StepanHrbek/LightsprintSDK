#include "rrintersect.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
//#include "core.h"
#include "geometry.h"

//#define USE_BSP
//#define USE_KD
#define DBG(a) //a
//#define TRANSFORM_SHOTS


//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	//!!!memset(this,0,sizeof(*this));
}

unsigned RRIntersectStats::shots;
unsigned RRIntersectStats::bsp;
unsigned RRIntersectStats::kd;
unsigned RRIntersectStats::tri;

unsigned  _i_stat[8]={0,0,0,0,0,0,0,0};


//////////////////////////////////////////////////////////////////////////////
//
// intersections
//

struct Triankle
{
	Triankle() {}
	Point3  s3;             // absolute position of start of base
	Vec3    r3,l3;          // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0]
	Normal  n3;             // normalised normal vector
	byte    intersectByte:4;// precalculated number for intersections, 0..8
	real    intersectReal;  // precalculated number for intersections
	void    setGeometry(Point3 *a,Point3 *b,Point3* c);
};

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

static Triankle *i_skip;
static Point3    i_eye;
static Vec3      i_direction;
static real      i_distanceMin; // bsp: starts as 0, may only increase during bsp traversal
static Point3    i_eye2;        // bsp: precalculated i_eye+i_direction*i_distanceMin
static real      i_hitDistance;
static bool      i_hitOuterSide;
static Triankle *i_hitTriangle;
static real      i_hitU;
static real      i_hitV;
static Point3    i_hitPoint3d;

#define I_DBG(a)

void _i_dbg_print()
{
	I_DBG(printf("intersect_bsp stat:"));
	I_DBG(for(int i=0;i<6;i++) if(i_stat[i]) printf(" %d%%",_i_stat[i+1]*100/_i_stat[i]));
	I_DBG(printf("\n"));
}

struct BspTree
{
	unsigned  size:30;
	unsigned  front:1;
	unsigned  back:1;
	BspTree*  next()           {return (BspTree *)((char *)this+size);}
	BspTree*  getFrontAdr()    {return this+1;}
	BspTree*  getFront()       {return front?getFrontAdr():NULL;}
	BspTree*  getBackAdr()     {return (BspTree *)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
	BspTree*  getBack()        {return back?getBackAdr():NULL;}
	Triankle**getTriangles()   {return (Triankle **)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
	void*     getTrianglesEnd(){return (char*)this+size;}
};

static inline bool intersect_triangle_bsp(Triankle *t)
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
	intersectStats.bsp++;
	assert(t);
	#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
	#endif

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	Triankle **triangle=(Triankle **)((char*)back+(t->back?back->size:0));
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
	intersectStats.kd++;
	assert(t);
	#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
	#endif
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

#endif //USE_KD


//////////////////////////////////////////////////////////////////////////////
//
// Intersector
//
// TRIANGLE_HANDLE is index into imported array of triangles

class Intersector
{
public:
	Intersector(RRObjectImporter* aimporter);
	~Intersector();

	bool            intersect(RRRay* ray, RRHit* hit);
	
	RRObjectImporter* importer;

	// information sucked from importer
	unsigned          triangles;
	Triankle*         triangle;
	BspTree*          bspTree;
	KdTree*           kdTree;
};

Intersector::Intersector(RRObjectImporter* aimporter)
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
	bspTree = NULL;
	kdTree = NULL;
}

Intersector::~Intersector()
{
	delete[] triangle;
}


// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triankle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool Intersector::intersect(RRRay* ray, RRHit* hit)
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

#ifdef USE_KD
	if(kdTree)
	{
		i_skip=&triangle[ray->skip];
		i_distanceMin=0;
		//real distanceMax=bbox.GetMaxDistance(i_eye);
		//distanceMax=MIN(distanceMax,*hitDistance);
		real distanceMax=1e5;
		result=intersect_kd(kdTree,distanceMax);
	}
	else
#endif
#ifdef USE_BSP
	if(bspTree)
	{
		i_skip=&triangle[ray->skip];
		i_distanceMin=0;
		i_eye2=i_eye;
		result=intersect_bsp(bspTree,ray->distanceMax);
	}
	else
#endif
	{
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
	}

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


//////////////////////////////////////////////////////////////////////////////
//
// RRObject
//

RRObject::RRObject(RRObjectImporter* importer)
{
	hook = new Intersector(importer);
}

RRObject::~RRObject()
{
	delete hook;
}

bool RRObject::intersect(RRRay* ray, RRHit* hit)
{
	return ((Intersector*)hook)->intersect(ray,hit);
}

