#include "PlyMeshReader.h"
#include "SphereUnitVecPool.h"
#include "StopWatch.h"
#include "..\..\include\RRIntersect.h"
#include <math.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <stdio.h>
#include <tchar.h>
#include <time.h>

typedef rrIntersect::RRIndexedTriListImporter<int> inherited;
//typedef rrIntersect::RRLessVerticesImporter<RRCollider::RRIndexedTriListImporter<int>,int> inherited;
//typedef rrIntersect::RRLessTrianglesImporter<RRCollider::RRLessVerticesImporter<RRCollider::RRIndexedTriListImporter<int>,int>,int> inherited;

class PlyMeshImporter : public inherited
{
public:
	PlyMeshImporter(PlyMesh& mesh) 
		: inherited((char*)&*mesh.verts.begin(),(unsigned)mesh.verts.size(),sizeof(PlyMeshVert),(int*)&*mesh.tris.begin(),(unsigned)mesh.tris.size()*3)
	{
	};
};

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
rrIntersect::RRCollider* intersector; // object able to calculate intersections

bool castOneRay(rrIntersect::RRRay* ray)
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
#ifndef COLLIDER_INPUT_UNLIMITED_DISTANCE
	ray->rayLengthMax = size;
#endif

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
	PlyMeshImporter importer(mesh);
	intersector = rrIntersect::RRCollider::create(&importer,rrIntersect::RRCollider::IT_BSP_FASTEST);
	printf("vertices=%d tris=%d\n",importer.getNumVertices(),importer.getNumTriangles());

	// create one ray for each thread
#ifdef _OPENMP
	int MAX_THREADS = omp_get_max_threads();
	printf("Processors = %d, max threads = %d.\n",omp_get_num_procs(),MAX_THREADS);
#else
	int MAX_THREADS = 1;
	printf("OpenMP not supported by compiler.\n");
#endif
	rrIntersect::RRRay* ray = rrIntersect::RRRay::create(MAX_THREADS);
	for(int i=0;i<MAX_THREADS;i++)
	{
		ray[i].rayFlags = rrIntersect::RRRay::FILL_TRIANGLE | rrIntersect::RRRay::FILL_DISTANCE;
		ray[i].rayLengthMin = 0;
	}

	// start watch
	StopWatch* watch = new StopWatch();
	watch->Start();

	// cast all rays
	const unsigned NUM_ITERS = 8000000;
	int num_hits = 0; // number of rays that actually hit the model
#ifndef _OPENMP
	// 1-thread version
	for(int i=0;i<NUM_ITERS;++i) if(castOneRay(ray)) num_hits++;
#else
	// n-thread version
	int j,k;
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
	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;
	const double speed = NUM_ITERS/((double)(watch->usertime)*1e-7);
	const double speed2 = NUM_ITERS/((double)(watch->usertime+watch->kerneltime)*1e-7); // slower due to swapping etc
	printf("speed = %d k/s   fraction_hit = %f\n",(int)(speed/1000),fraction_hit);
	fgetc(stdin);

	// cleanup
	delete watch;
	delete[] ray;
	delete intersector;

	return 0;
}
