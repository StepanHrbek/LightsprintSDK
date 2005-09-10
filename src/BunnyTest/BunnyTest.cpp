#include "PlyMeshReader.h"
#include "SphereUnitVecPool.h"
#include "StopWatch.h"
#include "..\..\include\RRIntersect.h"
#include <math.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

// optimizations
//#define COLLIDER_INPUT_INVDIR
//#define COLLIDER_INPUT_UNLIMITED_DISTANCE

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

int main(int argc, char** argv)
{                        
	PlyMesh mesh;
	PlyMeshReader reader;
	reader.readFile("bun_zipper.ply",mesh);
	PlyMeshImporter importer(mesh);
	rrIntersect::RRCollider* intersector = rrIntersect::RRCollider::create(&importer,rrIntersect::RRCollider::IT_BSP_FASTEST);
	printf("vertices=%d tris=%d\n",importer.getNumVertices(),importer.getNumTriangles());

	SphereUnitVecPool vecpool;//create pool of random points
	int num_hits = 0;//number of rays that actually hit the model
	const unsigned NUM_ITERS = 2000000*4;
	const PoolVec3 aabb_center = PoolVec3(-0.016840f, 0.110154f, -0.001537f);
	const float RADIUS = 0.2f;//radius of sphere

	rrIntersect::RRRay* ray = rrIntersect::RRRay::create();
	ray->flags = rrIntersect::RRRay::FILL_TRIANGLE | rrIntersect::RRRay::FILL_DISTANCE;
	StopWatch* watch = new StopWatch();
	watch->Start();
	for(unsigned i=0; i<NUM_ITERS; ++i)
	{
		PoolVec3 rayorigin = vecpool.getVec();//get random point on unit ball from pool
		PoolVec3 rayend = vecpool.getVec();//get random point on unit ball from pool
		PoolVec3 dir = rayend - rayorigin;
		float size = sqrtf((dir.x*dir.x)+(dir.y*dir.y)+(dir.z*dir.z));
		if(size==0) continue;

		//form the ray object
		ray->rayOrigin[0] = rayorigin.x*RADIUS+aabb_center.x;
		ray->rayOrigin[1] = rayorigin.y*RADIUS+aabb_center.y;
		ray->rayOrigin[2] = rayorigin.z*RADIUS+aabb_center.z;
#ifdef COLLIDER_INPUT_INVDIR
		ray->rayDirInv[0] = size/dir.x;
		ray->rayDirInv[1] = size/dir.y;
		ray->rayDirInv[2] = size/dir.z;
#else
		ray->rayDir[0] = dir.x/size;
		ray->rayDir[1] = dir.y/size;
		ray->rayDir[2] = dir.z/size;
#endif
#ifndef COLLIDER_INPUT_UNLIMITED_DISTANCE
		ray->hitDistanceMin = 0;
		ray->hitDistanceMax = size;
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
		if(intersector->intersect(ray))
			num_hits++;//count the hit.
	}
	watch->Watch();

	delete ray;
	delete intersector;

	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;
	const double speed = NUM_ITERS/((double)(watch->usertime)*1e-7);
	const double speed2 = NUM_ITERS/((double)(watch->usertime+watch->kerneltime)*1e-7); // slower due to swapping etc
	printf("speed = %d k/s   fraction_hit = %f\n",(int)(speed/1000),fraction_hit);
	fgetc(stdin);

	return 0;
}
