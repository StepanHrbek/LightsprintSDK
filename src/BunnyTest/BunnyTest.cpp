// BunnyTest.cpp : Defines the entry point for the console application.
//

#include "PlyMeshReader.h"
#include "SphereUnitVecPool.h"
#include "..\..\include\RRIntersect.h"
#include <math.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

typedef rrIntersect::RRIndexedTriListImporter<int> inherited;
//typedef rrIntersect::RRLessVerticesImporter<rrIntersect::RRIndexedTriListImporter<int>,int> inherited;
//typedef rrIntersect::RRLessTrianglesImporter<rrIntersect::RRLessVerticesImporter<rrIntersect::RRIndexedTriListImporter<int>,int>,int> inherited;

class PlyMeshImporter : public inherited
{
public:
	PlyMeshImporter(PlyMesh& mesh) 
		: inherited((char*)&*mesh.verts.begin(),(unsigned)mesh.verts.size(),sizeof(PlyMeshVert),(int*)&*mesh.tris.begin(),(unsigned)mesh.tris.size()*3)
	{
	};
};

int _tmain(int argc, _TCHAR* argv[])
{                        
	PlyMesh mesh;
	PlyMeshReader reader;
	reader.readFile("bun_zipper.ply",mesh);
	PlyMeshImporter importer(mesh);
	rrIntersect::RRIntersect* intersector = rrIntersect::RRIntersect::newIntersect(&importer,rrIntersect::RRIntersect::IT_BSP_COMPACT);
	printf("vertices=%d tris=%d\n",importer.getNumVertices(),importer.getNumTriangles());

	SphereUnitVecPool vecpool;//create pool of random points

	clock_t t0 = clock(); //start timer
	int num_hits = 0;//number of rays that actually hit the model

	const unsigned NUM_ITERS = 200000;
	const PoolVec3 aabb_center = PoolVec3(-0.016840f, 0.110154f, -0.001537f);

	for(int i=0; i<NUM_ITERS; ++i)
	{
		const float RADIUS = 0.2f;//radius of sphere

		///generate ray origin///
		PoolVec3 rayorigin = vecpool.getVec();//get random point on unit ball from pool

		rayorigin *= RADIUS;
		rayorigin += aabb_center;//aabb_center is (-0.016840, 0.110154, -0.001537)

		///generate other point on ray path///
		PoolVec3 rayend = vecpool.getVec();//get random point on unit ball from pool

		rayend *= RADIUS;
		rayend += aabb_center;

		PoolVec3 dir = rayend - rayorigin;
		float size = sqrtf((dir.x*dir.x)+(dir.y*dir.y)+(dir.z*dir.z));
		if(size==0) continue;

		//form the ray object
		rrIntersect::RRRay ray;
		ray.rayOrigin[0] = rayorigin.x;
		ray.rayOrigin[1] = rayorigin.y;
		ray.rayOrigin[2] = rayorigin.z;
		ray.rayDir[0] = dir.x/size;
		ray.rayDir[1] = dir.y/size;
		ray.rayDir[2] = dir.z/size;
		ray.hitDistanceMin = 0;
		ray.hitDistanceMax = size;
		ray.skipTriangle = UINT_MAX;
		ray.flags = rrIntersect::RRRay::FILL_TRIANGLE | rrIntersect::RRRay::FILL_DISTANCE;

		//do the trace
		if(intersector->intersect(&ray))
			num_hits++;//count the hit.
	}

	clock_t t1 = clock();
	const double speed = NUM_ITERS/((double)(t1-t0)/CLOCKS_PER_SEC);
	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;

	printf("speed = %d k/s   fraction_hit = %.3f\n",(int)(speed/1000),fraction_hit);
	fgetc(stdin);
	delete intersector;

	return 0;
}

