// --------------------------------------------------------------------------
// RRCollider minimalistic example.
//
// Simple mesh is constructed and collision with one ray detected.
//
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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

	// create mesh (geometry)
	RRMesh* mesh = RRMesh::create(RRMesh::TRI_LIST,RRMesh::FLOAT32,vertexArray,6,3*sizeof(float));

	// create collider (able to find ray x mesh intersections)
	bool aborting = false;
	const RRCollider* collider = RRCollider::create(mesh,NULL,RRCollider::IT_BSP_FAST,aborting);

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
		printf("Intersection was detected.\n\n");
		printf(" distance = %f\n",ray->hitDistance);
		printf(" position in object space = %f %f %f\n",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
		printf(" position in triangle space = %f %f\n",ray->hitPoint2d[0],ray->hitPoint2d[1]);
		printf(" triangle index in mesh = %d\n",ray->hitTriangle);
		printf(" triangle index before import = %d\n",mesh->getPreImportTriangle(ray->hitTriangle).index);
		printf(" triangle side = %s\n",ray->hitFrontSide?"front":"back");
		printf(" triangle plane = %f %f %f %f\n",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2],ray->hitPlane[3]);
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
	delete mesh;

	return 0;
}
