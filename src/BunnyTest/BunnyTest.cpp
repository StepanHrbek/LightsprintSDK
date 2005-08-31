#include "PlyMeshReader.h"
#include "SphereUnitVecPool.h"
#include "..\..\include\RRIntersect.h"
#include <math.h>
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

int main(int argc, char** argv)
{                        
	PlyMesh mesh;
	PlyMeshReader reader;
	reader.readFile("bun_zipper.ply",mesh);
	PlyMeshImporter importer(mesh);
	rrIntersect::RRCollider* intersector = rrIntersect::RRCollider::create(&importer,rrIntersect::RRCollider::IT_BSP_FASTEST);
	printf("vertices=%d tris=%d\n",importer.getNumVertices(),importer.getNumTriangles());

	SphereUnitVecPool vecpool;//create pool of random points

	clock_t t0 = clock(); //start timer
	int num_hits = 0;//number of rays that actually hit the model

	const unsigned NUM_ITERS = 2000000*4;
	const PoolVec3 aabb_center = PoolVec3(-0.016840f, 0.110154f, -0.001537f);

	rrIntersect::RRRay* ray = rrIntersect::RRRay::create();
	ray->skipTriangle = UINT_MAX;
	ray->flags = rrIntersect::RRRay::FILL_TRIANGLE | rrIntersect::RRRay::FILL_DISTANCE;
	const float RADIUS = 0.2f;//radius of sphere

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
		ray->rayDir[0] = dir.x/size;
		ray->rayDir[1] = dir.y/size;
		ray->rayDir[2] = dir.z/size;
		ray->hitDistanceMin = 0;
		ray->hitDistanceMax = size;

		//do the trace
		if(intersector->intersect(ray))
			num_hits++;//count the hit.
	}

	delete ray;

	clock_t t1 = clock();
	const double speed = NUM_ITERS/((double)(t1-t0)/CLOCKS_PER_SEC);
	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;

	printf("speed = %d k/s   fraction_hit = %f\n",(int)(speed/1000),fraction_hit);
	fgetc(stdin);
	delete intersector;

	return 0;
}

