#include "IntersectBsp.h"

#ifdef USE_BSP

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include "bsp.h"
#include "cache.h"

namespace rrIntersect
{

BspTree* load(FILE *f)
{
	if(!f) return NULL;
	unsigned size;
	size_t readen = fread(&size,sizeof(size),1,f);
	if(!readen) return NULL;
	size &= 0x3fffffff;
	fseek(f,-(int)sizeof(unsigned),SEEK_CUR);
	BspTree* tree = (BspTree*)malloc(size);
	readen = fread(tree,1,size,f);
	if(readen == size) return tree;
	free(tree);
	return NULL;
}

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

TriangleSRLNP*  i_triangleSRLNP;
real            i_distanceMin; // bsp: starts as 0, may only increase during bsp traversal
Point3          i_eye2;        // bsp: precalculated i_eye+i_direction*i_distanceMin
TRIANGLE_HANDLE i_skip;
TRIANGLE_HANDLE i_hitTriangle;

static bool intersect_bspSRLNP(BspTree *t,real distanceMax)
// input:                t, i_eye, i_eye2=i_eye, i_direction, i_skip, i_distanceMin=0, distanceMax
// returns:              true if ray hits t
// modifies when hit:    (i_eye2, i_distanceMin, i_hitPoint3d) i_hitU, i_hitV, i_hitOuterSide, i_hitDistance
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
	TRIANGLE_HANDLE* triangle=(TRIANGLE_HANDLE*)((char*)back+(t->back?back->size:0));
	assert(i_triangleSRLNP);
	Normal n=i_triangleSRLNP[triangle[0]].n3;
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
		if(t->front && intersect_bspSRLNP(front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspSRLNP(back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	i_hitPoint3d=i_eye+i_direction*distancePlane;
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=i_skip && intersect_triangleSRLNP(i_triangleSRLNP+*triangle))
		{
			i_hitTriangle=*triangle;
			i_hitDistance=distancePlane;
			return true;
		}
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

bool IntersectBsp::intersect_bspNP(BspTree *t,real distanceMax)
{
begin:
	intersectStats.bsp++;
	assert(t);
#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
#endif

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	TRIANGLE_HANDLE* triangle=(TRIANGLE_HANDLE*)((char*)back+(t->back?back->size:0));
	assert(triangleNP);
	Normal n=triangleNP[triangle[0]].n3;
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
		if(t->front && intersect_bspNP(front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspNP(back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	i_hitPoint3d=i_eye+i_direction*distancePlane;
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRObjectImporter::TriangleSRL t2;
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=i_skip && intersect_triangleNP(triangleNP+*triangle,&t2))
		{
			i_hitTriangle=*triangle;
			i_hitDistance=distancePlane;
			return true;
		}
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

IntersectBsp::IntersectBsp(RRObjectImporter* aimporter) : IntersectLinear(aimporter)
{
	tree = NULL;
	const char* name = getFileName(aimporter,".bsp");
	FILE* f = fopen(name,"rb");
	if(!f)
	{
		OBJECT obj;
		obj.face_num = triangles;
		obj.vertex_num = importer->getNumVertices();
		obj.face = new FACE[obj.face_num];
		obj.vertex = new VERTEX[obj.vertex_num];
		for(int i=0;i<obj.vertex_num;i++)
		{
			importer->getVertex(i,obj.vertex[i].x,obj.vertex[i].y,obj.vertex[i].z);
			obj.vertex[i].id = i;
			obj.vertex[i].side = 1;
			obj.vertex[i].used = 1;
		}
		for(int i=0;i<obj.face_num;i++)
		{
			unsigned v0,v1,v2,s;
			importer->getTriangle(i,v0,v1,v2,s);
			obj.face[i].vertex[0] = &obj.vertex[v0];
			obj.face[i].vertex[1] = &obj.vertex[v1];
			obj.face[i].vertex[2] = &obj.vertex[v2];
			obj.face[i].material = s;
		}
		f = fopen(name,"wb");
		if(f)
		{
			createAndSaveBsp(f,&obj);
			fclose(f);
		}
		f = fopen(name,"rb");
	}
	if(f)
	{
		tree = load(f);
	}
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triankle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool IntersectBsp::intersect(RRRay* ray, RRHit* hit)
{
	// fallback when bspgen failed (run from readonly disk etc)
	if(!tree) 
	{
		DBG(printf("Bsp fallback to linear."));
		return IntersectLinear::intersect(ray,hit);
	}

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

	assert(tree);
	if(triangleSRLNP)
	{
		i_skip=ray->skip;
		i_distanceMin=0;
		i_eye2=i_eye;
		i_triangleSRLNP=triangleSRLNP;
		result=intersect_bspSRLNP(tree,ray->distanceMax);
	} else 
	if(triangleNP)
	{
		i_skip=ray->skip;
		i_distanceMin=0;
		i_eye2=i_eye;
		result=intersect_bspNP(tree,ray->distanceMax);
	} else {
		assert(0);
		result = false;
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
		hit->triangle = i_hitTriangle;
		hit->distance = i_hitDistance;
	}

	return result;
}

IntersectBsp::~IntersectBsp()
{
	free(tree);
}

} // namespace

#endif
