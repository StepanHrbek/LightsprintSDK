// RRVision minimalistic example.
//
// Mesh with two triangles, one dark and one emitting light, is constructed.
// After quick calculation, we see that measures of illumination
// on both triangles are higher - light has been distributed.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006

#include "RRVision.h"
#include <cstdio>
#include <ctime>

using namespace rr;

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

// implements interface to position/rotation and material properties of our mesh
class MyObject : public RRObject
{
public:
	MyObject(RRCollider* acollider) 
	{
		collider = acollider;
	}
	virtual unsigned getTriangleSurface(unsigned t) const
	{
		// all triangles are from material 0
		return 0;
	}
	virtual const RRSurface* getSurface(unsigned si) const
	{
		// return surface 0 which is gray with diffuse reflectance 50%, no specular, no transmittance
		static RRSurface surface;
		surface.reset(false);
		return &surface;
	}
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		// return 0,0,0 for triangle 0 and 1,1,1 for triangle 1. this will make triangle 1 slightly shining
		out = RRColor(t?1.0f:0.0f);
	}
	virtual const RRCollider* getCollider() const 
	{
		// public access to underlying collider and mesh
		return collider;
	}
private:
	RRCollider* collider;
};

// prints illumination levels on our object
void printIllumination(RRScene* scene)
{
	for(unsigned triangle=0;triangle<2;triangle++)
	{
		RRColor exitance;
		// read exitance from object 0, triangle triangle, vertex 0
		scene->getTriangleMeasure(triangle,0,RM_EXITANCE_PHYSICAL,NULL,exitance);
		printf(" %f",exitance[0]);
	}
	printf("\n");
}

// callback for illuminationImprove, is it time to stop calculation?
bool endAtTime(void* context)
{
	return clock()>*(clock_t*)context;
}

int main()
{
	// provide licence information
	if(RRLicense::loadLicense("..\\..\\data\\licence_number")!=RRLicense::VALID)
		printf("Invalid licence, nothing will be computed.\n");

	// create mesh (contains only geometry)
	RRMesh* mesh = RRMesh::create(RRMesh::TRI_LIST,RRMesh::FLOAT32,vertexArray,6,3*sizeof(float));

	// create collider (able to find intersections with mesh)
	RRCollider* collider = RRCollider::create(mesh,RRCollider::IT_BSP_FAST);

	// create object (position/rotation and material properties attached to mesh)
	RRObject* object = new MyObject(collider);

	// create scene (able to calculate radiosity)
	RRScene* scene = new RRScene(object,NULL);

	// Print illumination levels for first time, should be 0.00, 1.00
	// First triangle is completely dark, while second one emits light.
	printf("Before calculation:");
	printIllumination(scene);
	
	// perform 0.1sec of calculations
	clock_t timeOfEnd = clock()+CLK_TCK/10;
	scene->illuminationImprove(endAtTime,&timeOfEnd);

	// Print illumination levels for second time, should be approximately 0.13, 1.02
	// First triangle is lit by second triangle (increase by 0.13).
	// Second triangle is slightly lit by its own light reflected by first triangle (increase by 0.02).
	printf("After calculation:");
	printIllumination(scene);
	printf("\nPress enter to end...");
	fgetc(stdin);

	// cleanup
	delete scene;
	delete object;
	delete collider;
	delete mesh;

	return 0;
}
