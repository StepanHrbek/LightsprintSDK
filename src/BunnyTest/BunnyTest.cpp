// Bunny Benchmark of Lightsprint Collider.
//
// Stanford Bunny model with 69451 triangles is loaded from disk,
// intersections with random rays are detected, speed is measured.
//
// Lightsprint, 2005-2006

#include "PlyMeshReader.h"
#include "SphereUnitVecPool.h"
#include "StopWatch.h"
#include "RRCollider.h"
#include <math.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <stdio.h>
#include <tchar.h>
#include <time.h>

using namespace rr;

int main(int argc, char** argv)
{                        
	printf("Stanford Bunny Benchmark\n\n");

	// load mesh from disk
	PlyMesh plyMesh;
	PlyMeshReader reader;
	try
	{
		reader.readFile("bun_zipper.ply",plyMesh);
	}
	catch(PlyMeshReaderExcep* e)
	{
		printf("%s\n\nHit enter to terminate...",e->what().c_str());
		fgetc(stdin);
		return 0;
	}
	RRMesh* rrMesh = RRMesh::createIndexed(
		RRMesh::TRI_LIST,
		RRMesh::FLOAT32,
		(void*)&*plyMesh.verts.begin(),
		(unsigned)plyMesh.verts.size(),
		sizeof(PlyMeshVert),
		RRMesh::UINT32,
		(int*)&*plyMesh.tris.begin(),
		(unsigned)plyMesh.tris.size()*3
		);
	printf("Bunny loaded: vertices=%d, triangles=%d.\n",rrMesh->getNumVertices(),rrMesh->getNumTriangles());

	// create random ray generator
	SphereUnitVecPool vecpool;

	// create collider
	RRCollider* collider = RRCollider::create(rrMesh,RRCollider::IT_BSP_FASTEST);

	// start watch
	StopWatch* watch = new StopWatch();
	watch->Start();

	// cast all rays
	const int NUM_ITERS = 3000000;
	int num_hits = 0; // number of rays that actually hit the model
#ifdef _OPENMP
	//printf("Available processors = %d, max threads = %d.\n",omp_get_num_procs(),omp_get_max_threads());
	#pragma omp parallel reduction(+:num_hits) num_threads(16)
	{
		int num_threads = omp_get_num_threads();
#else
	printf("OpenMP not supported by compiler, expect low performance.\n");
	{
		int num_threads = 1;
#endif
		// create ray for intersection testing
		RRRay* ray = RRRay::create();
		ray->rayFlags = RRRay::FILL_TRIANGLE | RRRay::FILL_DISTANCE;
		ray->rayLengthMin = 0;
		// perform intersection tests
		for(int i=0; i<NUM_ITERS/num_threads; ++i)
		{
			static const PoolVec3 aabb_center = PoolVec3(-0.016840f, 0.110154f, -0.001537f);
			static const float RADIUS = 0.2f;//radius of sphere

			PoolVec3 rayorigin = vecpool.getVec();//get random point on unit ball from pool
			PoolVec3 rayend = vecpool.getVec();//get random point on unit ball from pool
			PoolVec3 dir = rayend - rayorigin;
			float size = sqrtf((dir.x*dir.x)+(dir.y*dir.y)+(dir.z*dir.z));
			if(size==0) continue;

			//form the ray object
			ray->rayOrigin[0] = rayorigin.x*RADIUS+aabb_center.x;
			ray->rayOrigin[1] = rayorigin.y*RADIUS+aabb_center.y;
			ray->rayOrigin[2] = rayorigin.z*RADIUS+aabb_center.z;
			ray->rayDirInv[0] = size/dir.x;
			ray->rayDirInv[1] = size/dir.y;
			ray->rayDirInv[2] = size/dir.z;
			ray->rayLengthMax = size;

			//do the trace
			if(collider->intersect(ray))
				num_hits++;//count the hit.
		}
		// cleanup
		delete ray;
	}

	// stop watch
	watch->Watch();

	// report results
	printf("\nDetected speed: %d intersections per second (hit ratio=%f)\n",
		(int)(NUM_ITERS/(watch->realtime)),
		(double)num_hits / NUM_ITERS
		);
	printf("Note that statically linked version is faster by 15%%.\n");
	/*printf("\nMeasured speed: wallspd=%d 1cpuspd=%d user=%f kernel=%f hits=%f\n",
		(int)(NUM_ITERS/(watch->realtime)/1000),
		(int)(NUM_ITERS/(watch->usertime+watch->kerneltime)/1000),
		watch->usertime/watch->realtime,
		watch->kerneltime/watch->realtime,
		(double)num_hits / NUM_ITERS);*/

	// cleanup
	delete watch;
	delete rrMesh;
	delete collider;

	printf("\nHit enter to close...");
	fgetc(stdin);
	return 0;
}
