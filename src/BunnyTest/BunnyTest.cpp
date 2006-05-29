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

rr::RRMesh* createPlyMeshImporter(PlyMesh& mesh)
{
	return rr::RRMesh::createIndexed(
		rr::RRMesh::TRI_LIST,
		rr::RRMesh::FLOAT32,
		(void*)&*mesh.verts.begin(),
		(unsigned)mesh.verts.size(),
		sizeof(PlyMeshVert),
		rr::RRMesh::UINT32,
		(int*)&*mesh.tris.begin(),
		(unsigned)mesh.tris.size()*3
		);
}

/*struct Box
{
Vec3    min;
real    pad;
Vec3    max;
void    detect(const Vec3 *vertex,unsigned vertices);
bool    intersect(RRRay* ray) const;
};
Box box;*/

SphereUnitVecPool vecpool; // object able to generate random points
rr::RRCollider* intersector; // object able to calculate intersections

bool castOneRay(rr::RRRay* ray)
{
	static const PoolVec3 aabb_center = PoolVec3(-0.016840f, 0.110154f, -0.001537f);
	static const float RADIUS = 0.2f;//radius of sphere

	PoolVec3 rayorigin = vecpool.getVec();//get random point on unit ball from pool
	PoolVec3 rayend = vecpool.getVec();//get random point on unit ball from pool
	PoolVec3 dir = rayend - rayorigin;
	float size = sqrtf((dir.x*dir.x)+(dir.y*dir.y)+(dir.z*dir.z));
	if(size==0) return false;

	//form the ray object
	ray->rayOrigin[0] = rayorigin.x*RADIUS+aabb_center.x;
	ray->rayOrigin[1] = rayorigin.y*RADIUS+aabb_center.y;
	ray->rayOrigin[2] = rayorigin.z*RADIUS+aabb_center.z;
	ray->rayDirInv[0] = size/dir.x;
	ray->rayDirInv[1] = size/dir.y;
	ray->rayDirInv[2] = size/dir.z;
	ray->rayLengthMax = size;

	/*if(box.intersect(ray))
	{
	ray->rayDir[0] = dir.x/size;
	ray->rayDir[1] = dir.y/size;
	ray->rayDir[2] = dir.z/size;
	if(intersector->intersect(ray))
	num_hits++;//count the hit.
	}*/

	//do the trace
	return intersector->intersect(ray);
}

int main(int argc, char** argv)
{                        
	// create one collider
	PlyMesh mesh;
	PlyMeshReader reader;
	reader.readFile("bun_zipper.ply",mesh);
	intersector = rr::RRCollider::create(createPlyMeshImporter(mesh),rr::RRCollider::IT_BSP_FASTEST);
	printf("vertices=%d tris=%d\n",intersector->getImporter()->getNumVertices(),intersector->getImporter()->getNumTriangles());

	// create one ray for each thread
#ifdef _OPENMP
	int MAX_THREADS = omp_get_max_threads();
	printf("Processors = %d, max threads = %d.\n",omp_get_num_procs(),MAX_THREADS);
#else
	int MAX_THREADS = 1;
	printf("OpenMP not supported by compiler.\n");
#endif
	const int MAX_THREADS_INCL_FORCE = (MAX_THREADS<4)?4:MAX_THREADS;
	rr::RRRay* ray = rr::RRRay::create(MAX_THREADS_INCL_FORCE);
	for(int i=0;i<MAX_THREADS_INCL_FORCE;i++)
	{
		ray[i].rayFlags = rr::RRRay::FILL_TRIANGLE | rr::RRRay::FILL_DISTANCE;
		ray[i].rayLengthMin = 0;
	}

	StopWatch* watch = new StopWatch();
	const unsigned NUM_ITERS = 8000000;
	int num_hits;
#ifdef _OPENMP
	int j,k;
#endif

	///////////////////////////////////////////////////////////////////////////

	// start watch
	watch->Start();

	// cast all rays
	num_hits = 0; // number of rays that actually hit the model
#ifndef _OPENMP
	// 1-thread version
	for(unsigned i=0;i<NUM_ITERS;++i) if(castOneRay(ray)) num_hits++;
#else
	// n-thread version
	#pragma omp parallel for private(j,k)
	for(int i=0; i<NUM_ITERS/10000; ++i)
	{
		k = omp_get_thread_num(); // call once per 10000 rays to hide overhead
		//printf("%d ",k);
		for(j=0; j<10000; ++j)
		{
			if(castOneRay(ray+k))
				#pragma omp atomic
				num_hits++;//count the hit.
		}
	}
#endif

	// stop watch
	watch->Watch();

	// report results
	printf("auto: wallspd=%d 1cpuspd=%d user=%f kernel=%f hits=%f\n",
		(int)(NUM_ITERS/(watch->realtime)/1000),
		(int)(NUM_ITERS/(watch->usertime+watch->kerneltime)/1000),
		watch->usertime/watch->realtime,
		watch->kerneltime/watch->realtime,
		(double)num_hits / NUM_ITERS);

	///////////////////////////////////////////////////////////////////////////
#ifdef _OPENMP

	// start watch
	watch->Start();

	// cast all rays
	num_hits = 0; // number of rays that actually hit the model
	for(int i=0;i<NUM_ITERS;++i) if(castOneRay(ray)) num_hits++;

	// stop watch
	watch->Watch();

	// report results
	printf("1thr: wallspd=%d 1cpuspd=%d user=%f kernel=%f hits=%f\n",
		(int)(NUM_ITERS/(watch->realtime)/1000),
		(int)(NUM_ITERS/(watch->usertime+watch->kerneltime)/1000),
		watch->usertime/watch->realtime,
		watch->kerneltime/watch->realtime,
		(double)num_hits / NUM_ITERS);

	///////////////////////////////////////////////////////////////////////////

	// start watch
	watch->Start();

	// cast all rays
	num_hits = 0; // number of rays that actually hit the model
	#pragma omp parallel for private(j,k) num_threads(2)
	for(int i=0; i<NUM_ITERS/10000; ++i)
	{
		k = omp_get_thread_num(); // call once per 10000 rays to hide overhead
		//printf("%d ",k);
		for(j=0; j<10000; ++j)
		{
			if(castOneRay(ray+k))
				#pragma omp atomic
				num_hits++;//count the hit.
		}
	}

	// stop watch
	watch->Watch();

	// report results
	printf("2thr: wallspd=%d 1cpuspd=%d user=%f kernel=%f hits=%f\n",
		(int)(NUM_ITERS/(watch->realtime)/1000),
		(int)(NUM_ITERS/(watch->usertime+watch->kerneltime)/1000),
		watch->usertime/watch->realtime,
		watch->kerneltime/watch->realtime,
		(double)num_hits / NUM_ITERS);

	///////////////////////////////////////////////////////////////////////////

	// start watch
	watch->Start();

	// cast all rays
	num_hits = 0; // number of rays that actually hit the model
	#pragma omp parallel for private(j,k) num_threads(4)
	for(int i=0; i<NUM_ITERS/10000; ++i)
	{
		k = omp_get_thread_num(); // call once per 10000 rays to hide overhead
		//printf("%d ",k);
		for(j=0; j<10000; ++j)
		{
			if(castOneRay(ray+k))
				#pragma omp atomic
				num_hits++;//count the hit.
		}
	}

	// stop watch
	watch->Watch();

	// report results
	printf("4thr: wallspd=%d 1cpuspd=%d user=%f kernel=%f hits=%f\n",
		(int)(NUM_ITERS/(watch->realtime)/1000),
		(int)(NUM_ITERS/(watch->usertime+watch->kerneltime)/1000),
		watch->usertime/watch->realtime,
		watch->kerneltime/watch->realtime,
		(double)num_hits / NUM_ITERS);
#endif
	///////////////////////////////////////////////////////////////////////////

	// cleanup
	delete watch;
	delete[] ray;
	delete intersector;

	fgetc(stdin);
	return 0;
}
