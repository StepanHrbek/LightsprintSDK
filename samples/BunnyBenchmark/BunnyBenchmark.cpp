// --------------------------------------------------------------------------
// Bunny Benchmark of Lightsprint Collider.
//
// Stanford Bunny model with 69451 triangles is loaded from disk,
// intersections with random rays are detected, speed is measured.
//
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "plymeshreader.h"
#include "sphereunitvecpool.h"
#include "Lightsprint/RRCollider.h"
#include <math.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <stdio.h>
#include <time.h>

using namespace rr;

int main(int argc, char** argv)
{
#ifdef XBOX
	RRReporter* reporter = RRReporter::createFileReporter("game:\\results.txt");
#else
	RRReporter* reporter = RRReporter::createPrintfReporter();
#endif

	RRReporter::report(INF1,"Stanford Bunny Benchmark\n");

	// provide license information
	const char* licError = loadLicense("../../data/licence_number");
	if (licError)
		RRReporter::report(ERRO,"%s\n",licError);


	// load mesh from disk
	PlyMesh plyMesh;
	PlyMeshReader reader;
	try
	{
#ifdef XBOX
		reader.readFile("game:\\bun_zipper.ply",plyMesh);
#else
		reader.readFile("../../data/objects/bun_zipper.ply",plyMesh);
#endif
	}
	catch(PlyMeshReaderExcep* e)
	{
		RRReporter::report(INF1,"%s\n\nHit enter to terminate...",e->what().c_str());
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
	RRReporter::report(INF2,"Bunny loaded: vertices=%d, triangles=%d.\n",rrMesh->getNumVertices(),rrMesh->getNumTriangles());

	// create random ray generator
	SphereUnitVecPool vecpool;

	// create collider
	bool aborting = false;
	const RRCollider* collider = RRCollider::create(rrMesh,NULL,RRCollider::IT_BSP_FASTEST,aborting);

	// start watch
	RRTime time;

	// cast all rays
	const int NUM_RAYS = 25000000;
	int num_hits = 0; // number of rays that actually hit the model
#ifdef _OPENMP
	RRReporter::report(INF2,"Available processors = %d, max threads = %d.\n",omp_get_num_procs(),omp_get_max_threads());
	#pragma omp parallel reduction(+:num_hits) //num_threads(16) // 16 improves performance on Pentiums with hyperthreading but decreases on PowerPC in Xbox360
	{
		int num_threads = omp_get_num_threads();
#else
	RRReporter::report(INF2,"OpenMP not supported by compiler, expect low performance.\n");
	{
		int num_threads = 1;
#endif
		// prepare ray for intersection testing
		RRRay ray;
		ray.rayFlags = RRRay::FILL_TRIANGLE | RRRay::FILL_DISTANCE;
		ray.rayLengthMin = 0;
		// perform intersection tests
		for (int i=0; i<NUM_RAYS/num_threads; ++i)
		{
			static const PoolVec3 AABB_CENTER = PoolVec3(-0.016840f, 0.110154f, -0.001537f);
			static const float RADIUS = 0.2f;//radius of sphere

			PoolVec3 rayorigin = vecpool.getVec();//get random point on unit ball from pool
			PoolVec3 rayend = vecpool.getVec();//get random point on unit ball from pool
			PoolVec3 dir = rayend - rayorigin;
			float size = sqrtf((dir.x*dir.x)+(dir.y*dir.y)+(dir.z*dir.z));
			if (size==0) continue;

			//form the ray object
			ray.rayOrigin[0] = rayorigin.x*RADIUS+AABB_CENTER.x;
			ray.rayOrigin[1] = rayorigin.y*RADIUS+AABB_CENTER.y;
			ray.rayOrigin[2] = rayorigin.z*RADIUS+AABB_CENTER.z;
			ray.rayDir[0] = dir.x/size;
			ray.rayDir[1] = dir.y/size;
			ray.rayDir[2] = dir.z/size;
			ray.rayLengthMax = size;

			//do the trace
			if (collider->intersect(&ray))
				num_hits++;//count the hit.
		}
	}

	// report results
	RRReporter::report(INF1,"Detected speed: %d intersections per second (hit ratio=%f)\n",
		(int)(NUM_RAYS/time.secondsPassed()),
		(double)num_hits/NUM_RAYS
		);

	RRReporter::report(INF1,"Hit enter to close...");
	fgetc(stdin);

	// cleanup
	delete rrMesh;
	delete collider;
	delete reporter;

	return 0;
}
