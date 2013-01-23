// --------------------------------------------------------------------------
// Ray-multimesh collision example.
//
// Multimesh is set of meshes glued together without data duplication.
// Simple multimesh is constructed and collision with one ray detected.
//
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRCollider.h"
#include <stdio.h>

using namespace rr;

int main()
{
	// provide licence information
	const char* licError = loadLicense("../../data/licence_number");
	if (licError)
		puts(licError);

	// array of meshes in our scene. all meshes use the same coordinate space
	RRMesh* meshes[2];

	// simple mesh used for demonstration
	float vertexArray[18] =
	{
		// triangle 0
		-1,0,0, // vertex 0
		1,0,0, // vertex 1
		0,1,0, // vertex 2
		// triangle 1
		1,0,0, // vertex 1
		-1,0,0, // vertex 0
		0,0,1, // vertex 3
	};

	// create mesh[0] from array
	meshes[0] = RRMesh::create(RRMesh::TRI_LIST,RRMesh::FLOAT32,vertexArray,6,3*sizeof(float));

	// create mesh[1] as another instance of mesh[0], but slightly translated. data are not duplicated
	RRMatrix3x4 matrix(
		1,0,0,0,
		0,1,0,-0.1f,
		0,0,1,0);
	meshes[1] = meshes[0]->createTransformed(&matrix);

	// create multimesh, glue array of meshes together without duplicating data
	const RRMesh* multiMesh = RRMesh::createMultiMesh(meshes,2,false);

	// you can add more filters, e.g.
	//multiMesh2 = multiMesh->createOptimizedVertices(0.1f); // vertex weld, distance 0.1
	//multiMesh3 = multiMesh2->createOptimizedTriangles(); // remove degenerated triangles (e.g. from vertex weld)

	printf("Triangles in multimesh = %d\n",multiMesh->getNumTriangles()); // 2+2
	printf("Vertices in multimesh = %d\n\n",multiMesh->getNumVertices()); // 6+6

	// create collider (able to find ray x mesh intersections)
	bool aborting = false;
	const RRCollider* collider = RRCollider::create(multiMesh,NULL,RRCollider::IT_BSP_FAST,aborting);

	// create ray (contains both ray and intersection results)
	RRRay* ray = RRRay::create();
	ray->rayOrigin = RRVec3(0,-1,0.3f);
	ray->rayDir = RRVec3(0,1,0);
	ray->rayLengthMin = 0;
	ray->rayLengthMax = 1000;

	// find intersection
	bool hit = collider->intersect(ray);

	// print results
	if (hit)
	{
		// get original mesh/triangle numbers
		// it works even when filters (e.g. vertex weld) run on mesh or multimesh
		RRMesh::PreImportNumber original = multiMesh->getPreImportTriangle(ray->hitTriangle);

		printf("Intersection was detected.\n\n");
		printf(" distance                    = %f\n",ray->hitDistance);
		printf(" position in object space    = %f %f %f\n",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
		printf(" position in triangle space  = %f %f\n",ray->hitPoint2d[0],ray->hitPoint2d[1]);
		printf(" triangle in multimesh       = %d\n",ray->hitTriangle);
		printf("   original mesh number      = %d\n",original.object); // we had 2 meshes, so this number is in 0..1 range
		printf("   original triangle number  = %d\n",original.index); // we hit mesh with 2 triangles, so this number is in 0..1 range
		printf(" triangle side               = %s\n",ray->hitFrontSide?"front":"back");
		printf(" triangle plane              = %f %f %f %f\n",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2],ray->hitPlane[3]);
	}
	else
	{
		printf("No intersection was detected.\n");
	}
	printf("\nPress enter to end...");
	fgetc(stdin);

	// cleanup
	delete ray;
	delete collider;
	delete multiMesh;
	delete meshes[1];
	delete meshes[0];

	return 0;
}
