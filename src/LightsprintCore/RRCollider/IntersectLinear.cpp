// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - linear.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "IntersectLinear.h"
#include <assert.h>
#include <math.h>
#include <cstring>
#include <stdio.h>
#include <cstdlib> // exit, qsort

#define DBG(a) //a

namespace rr
{

PRIVATE bool update_rayDir(RRRay* ray)
{
#ifdef COLLIDER_INPUT_INVDIR
	ray->rayDir[0] = 1/ray->rayDirInv[0];
	ray->rayDir[1] = 1/ray->rayDirInv[1];
	ray->rayDir[2] = 1/ray->rayDirInv[2];
#else
#ifdef TRAVERSAL_INPUT_DIR_INVDIR
	ray->rayDirInv[0] = 1/ray->rayDir[0];
	ray->rayDirInv[1] = 1/ray->rayDir[1];
	ray->rayDirInv[2] = 1/ray->rayDir[2];
#endif
#endif
	return true;
}

PRIVATE void update_hitPoint3d(RRRay* ray, real distance)
{
	ray->hitPoint3d[0] = ray->rayOrigin[0] + ray->rayDir[0]*distance;
	ray->hitPoint3d[1] = ray->rayOrigin[1] + ray->rayDir[1]*distance;
	ray->hitPoint3d[2] = ray->rayOrigin[2] + ray->rayDir[2]*distance;
}

PRIVATE void update_hitPlane(RRRay* ray, const RRMesh* importer)
{
	RRMesh::TriangleBody t2;
	importer->getTriangleBody(ray->hitTriangle,t2);
	Vec3 n;
	n.x = t2.side1[1] * t2.side2[2] - t2.side1[2] * t2.side2[1];
	n.y = t2.side1[2] * t2.side2[0] - t2.side1[0] * t2.side2[2];
	n.z = t2.side1[0] * t2.side2[1] - t2.side1[1] * t2.side2[0];
	real siz = size(n);
	ray->hitPlane[0] = n.x/siz;
	ray->hitPlane[1] = n.y/siz;
	ray->hitPlane[2] = n.z/siz;
	ray->hitPlane[3] = -(t2.vertex0[0] * ray->hitPlane[0] + t2.vertex0[1] * ray->hitPlane[1] + t2.vertex0[2] * ray->hitPlane[2]);
}

PRIVATE bool intersect_triangle(RRRay* ray, const RRMesh::TriangleBody* t)
// input:                ray, t
// returns:              true if ray hits t
// modifies when hit:    hitDistance, hitPoint2D, hitFrontSide
// modifies when no hit: <nothing is changed>
{
	FILL_STATISTIC(intersectStats.intersect_triangle++);
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec = ortogonalTo(ray->rayDir,t->side2);
	real det = dot(t->side1,pvec);

	// cull test
	bool hitFrontSide = det>0;
	if(!hitFrontSide && (ray->rayFlags&RRRay::TEST_SINGLESIDED)) return false;

	// if determinant is near zero, ray lies in plane of triangle
	if(det==0) return false;
	//#define EPSILON 1e-10 // 1e-6 good for all except bunny, 1e-10 good for bunny
	//if(det>-EPSILON && det<EPSILON) return false;

	// calculate distance from vert0 to ray origin
	Vec3 tvec = ray->rayOrigin-t->vertex0;

	// calculate U parameter and test bounds
	real u = dot(tvec,pvec)/det;
	if(u<0 || u>1) return false;

	// prepare to test V parameter
	Vec3 qvec = ortogonalTo(tvec,t->side1);

	// calculate V parameter and test bounds
	real v = dot(ray->rayDir,qvec)/det;
	if(v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	real dist = dot(t->side2,qvec)/det;
	if(dist<ray->hitDistanceMin || dist>ray->hitDistanceMax) return false;

	ray->hitDistance = dist;
#ifdef FILL_HITPOINT2D
	ray->hitPoint2d[0] = u;
	ray->hitPoint2d[1] = v;
#endif
#ifdef FILL_HITSIDE
	ray->hitFrontSide = hitFrontSide;
#endif
	return true;
}

IntersectLinear::IntersectLinear(const RRMesh* aimporter)
{
#ifdef USE_SSE
	if((long long)(&box)%16) 
	{
		RRReporter::report(ERRO,"You created unaligned structure. Try static or heap if it's on stack now.\n");
		RR_ASSERT(!((long long)(&box)%16));
		exit(0);
	}
#endif
	importer = aimporter;
	triangles = importer->getNumTriangles();

	unsigned vertices = importer->getNumVertices();
	Vec3* vertex = new Vec3[vertices];
	for(unsigned i=0;i<vertices;i++)
	{
		RRMesh::Vertex v;
		importer->getVertex(i,v);
		vertex[i].x = v[0];
		vertex[i].y = v[1];
		vertex[i].z = v[2];
	}
	box.detect(vertex,vertices);
	delete[] vertex;

	// lower number = danger of numeric errors
	// higher number = slower intersection
	// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)
	//Vec3 tmp = box.max-box.min;
	//DELTA_BSP = (tmp.x+tmp.y+tmp.z)/4*1e-5f;
	RRReal tmpx = MAX(fabs(box.max.x),fabs(box.min.x));
	RRReal tmpy = MAX(fabs(box.max.y),fabs(box.min.y));
	RRReal tmpz = MAX(fabs(box.max.z),fabs(box.min.z));
	RRReal maxCoord = MAX(MAX(tmpx,tmpy),tmpz);
	RR_ASSERT(IS_NUMBER(maxCoord));
	if(maxCoord==0 || _isnan(maxCoord)) maxCoord = 1;
	DELTA_BSP = maxCoord*1e-5f;
}

unsigned IntersectLinear::getMemoryOccupied() const
{
	return sizeof(IntersectLinear);
}

bool IntersectLinear::isValidTriangle(unsigned i) const
{
	RRMesh::Triangle t;
	importer->getTriangle(i,t);
	return t[0]!=t[1] && t[0]!=t[2] && t[1]!=t[2];
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitFrontSide,real *hitDistance)
bool IntersectLinear::intersect(RRRay* ray) const
{
	DBG(printf("\n"));
	FILL_STATISTIC(intersectStats.intersect_mesh++);
	if(!importer) return false; // this shouldn't happen but linear is so slow that we can test it
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	ray->hitDistance = ray->hitDistanceMax;

#ifdef USE_EXPECT_HIT
	if(ray->rayFlags&RRRay::EXPECT_HIT) 
	{
		ray->hitDistanceMin = ray->rayLengthMin;
		ray->hitDistanceMax = ray->rayLengthMax;
	}
	else
#endif
	{
		if(!box.intersect(ray)) return false;
	}

	bool hit = false;
	update_rayDir(ray);
	RR_ASSERT(fabs(size2(ray->rayDir)-1)<0.001);//ocekava normalizovanej dir
	FILL_STATISTIC(intersectStats.intersect_linear++);
#ifdef COLLISION_HANDLER
	char backup[sizeof(RRRay)];
	if(ray->collisionHandler)
		ray->collisionHandler->init();
#endif
	for(unsigned t=0;t<triangles;t++)
	{
		RRMesh::TriangleBody t2;
		importer->getTriangleBody(t,t2);
		if(intersect_triangle(ray,&t2))
		{
			ray->hitTriangle = t;
#ifdef COLLISION_HANDLER
			if(ray->collisionHandler) 
			{
#ifdef FILL_HITPOINT3D
				if(ray->rayFlags&RRRay::FILL_POINT3D)
				{
					update_hitPoint3d(ray,ray->hitDistance);
				}
#endif
#ifdef FILL_HITPLANE
				if(ray->rayFlags&RRRay::FILL_PLANE)
				{
					update_hitPlane(ray,importer);
				}
#endif
				// hits are reported in random order
				if(ray->collisionHandler->collides(ray)) 
				{
					memcpy(backup,ray,sizeof(*ray)); // the best hit is stored, *ray may be overwritten by other faces that seems better until they get refused by collides
					ray->hitDistanceMax = ray->hitDistance;
					hit = true;
				}
			}
			else
#endif
			{
				ray->hitDistanceMax = ray->hitDistance;
				hit = true;
			}
		}
	}
#ifdef COLLISION_HANDLER
	if(ray->collisionHandler)
		hit = ray->collisionHandler->done();
#endif
	if(hit) 
	{
#ifdef COLLISION_HANDLER
		if(ray->collisionHandler)
		{
			memcpy(ray,backup,sizeof(*ray)); // the best hit is restored
		}
#endif
#ifdef FILL_HITPOINT3D
		if(ray->rayFlags&RRRay::FILL_POINT3D)
		{
			update_hitPoint3d(ray,ray->hitDistance);
		}
#endif
#ifdef FILL_HITPLANE
		if(ray->rayFlags&RRRay::FILL_PLANE)
		{
			update_hitPlane(ray,importer);
		}
#endif
		FILL_STATISTIC(intersectStats.hit_mesh++);
	}
	return hit;
}

IntersectLinear::~IntersectLinear()
{
}


////////////////////////////////////////////////////////////////////////////
//
// RayHitBackup

// RayHitBackup saves minimal set of information about ray hit and restores full hit information on request.
// saved: hitTriangle,hitDistance,hitPoint2d,hitFrontSide
// autogenerated: hitPoint3d, hitPlane
// Used by IntersectBspCompact, but Linear and Fast* can use it in future too.
class RayHitBackup
{
public:
	// creates backup of ray hit
	// (it's not necessary to have valid hitPoint3d and hitPlane, they are not saved anyway)
	void createBackupOf(const RRRay* ray)
	{
		hitTriangle = ray->hitTriangle;
		hitDistance = ray->hitDistance;
		#ifdef FILL_HITPOINT2D
			hitPoint2d = ray->hitPoint2d;
		#endif
		#ifdef FILL_HITSIDE
			hitFrontSide = ray->hitFrontSide;
		#endif
	}
	// restores ray hit from backup
	// (restores also hitPoint3d and hitPlane that are autogenerated)
	void restoreBackupTo(RRRay* ray, const RRMesh* mesh)
	{
		// some data are copied
		ray->hitTriangle = hitTriangle;
		ray->hitDistance = hitDistance;
		#ifdef FILL_HITPOINT2D
			ray->hitPoint2d = hitPoint2d;
		#endif
		#ifdef FILL_HITSIDE
			ray->hitFrontSide = hitFrontSide;
		#endif
		// some data are autogenerated
		#ifdef FILL_HITPOINT3D
			if(ray->rayFlags&RRRay::FILL_POINT3D)
			{
				update_hitPoint3d(ray,ray->hitDistance);
			}
		#endif
		#ifdef FILL_HITPLANE
			if(ray->rayFlags&RRRay::FILL_PLANE)
			{
				update_hitPlane(ray,mesh);
			}
		#endif
	}
	// helper when sorting ray hits by distance
	static int compareBackupDistance(const void* elem1, const void* elem2)
	{
		const RayHitBackup* ray1 = (RayHitBackup*)elem1;
		const RayHitBackup* ray2 = (RayHitBackup*)elem2;
		return (ray1->hitDistance<ray2->hitDistance) ? ((ray1->hitDistance==ray2->hitDistance) ? 0 : -1) : 1;
	}
	RRReal getHitDistance()
	{
		return hitDistance;
	}
private:
	unsigned hitTriangle;
	RRReal hitDistance;
	#ifdef FILL_HITPOINT2D
		RRVec2 hitPoint2d;
	#endif
	#ifdef FILL_HITSIDE
		bool hitFrontSide;
	#endif
};


////////////////////////////////////////////////////////////////////////////
//
// RayHits

RayHits::RayHits(unsigned maxNumHits)
{
	RR_ASSERT(maxNumHits);
	backupSlot = new RayHitBackup[maxNumHits];
	backupSlotsUsed = 0;
	theBestSlot = 0;
}

void RayHits::insertHitUnordered(RRRay* ray)
{
	RR_ASSERT(ray);
	backupSlot[backupSlotsUsed++].createBackupOf(ray);
	if(ray->hitDistance<backupSlot[theBestSlot].getHitDistance()) theBestSlot = backupSlotsUsed-1;
}

bool RayHits::getHitOrdered(RRRay* ray, const RRMesh* mesh)
{
	RR_ASSERT(ray);
	bool hit = false;
	if(backupSlotsUsed)
	{
		// restore the most close hit
		backupSlot[theBestSlot].restoreBackupTo(ray,mesh);

#ifdef COLLISION_HANDLER
		if(ray->collisionHandler)
		{
			// optimization: try theBestSlot first, before qsort
			if(ray->collisionHandler->collides(ray)) 
			{
				hit = true;
			}
			else
			{
				// theBestSlot was rejected, sort hits by distance
				qsort(backupSlot,backupSlotsUsed,sizeof(RayHitBackup),RayHitBackup::compareBackupDistance);

				// run collisionHandler on sorted hits
				// skip 0th element, it was theBestSlot before sort
				for(unsigned i=1;i<backupSlotsUsed;i++)
				{
					backupSlot[i].restoreBackupTo(ray,mesh);
					if(ray->collisionHandler->collides(ray)) 
					{
						hit = true;
						break;
					}
				}
			}
		}
		else
#endif
		{
			// simply keep the most close hit
			hit = true;
		}
	}
	return hit;
}

RayHits::~RayHits()
{
	delete[] backupSlot;
}

} // namespace
