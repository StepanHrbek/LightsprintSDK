// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Realtime update of cube reflection maps.
// --------------------------------------------------------------------------


#ifdef _OPENMP
#include <omp.h>
#endif

#include "report.h"
#include "Lightsprint/RRSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"

#define CENTER_GRANULARITY 0.01f // if envmap center moves less than granularity, it is considered unchanged. prevents updates when dynamic object rotates (=position slightly fluctuates)

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// cube connectivity

enum Edge {TOP=0, LEFT, RIGHT, BOTTOM};

struct CubeSide
{
	signed char dir[3];
	struct Neighbour
	{
		unsigned char side;
		unsigned char edge;
	};
	Neighbour neighbour[4]; // indexed by Edge

	RRVec3 getDir() const
	{
		return RRVec3(dir[0],dir[1],dir[2]);
	}

	// returns direction from cube center to cube texel center.
	// texel coordinates: -1..1
	RRVec3 getTexelDir(RRVec2 uv) const;

	// returns direction from cube center to cube texel center.
	// texel coordinates: 0..size-1
	RRVec3 getTexelDir(unsigned size, unsigned x, unsigned y) const
	{
		return getTexelDir(RRVec2( RRReal(int(1+2*x-size))/size , RRReal(int(1+2*y-size))/size ));
	}
};

static const CubeSide cubeSide[6] =
{
	// describes cubemap sides in opengl order
	{{ 1, 0, 0}, {{2,RIGHT },{4,RIGHT },{5,LEFT  },{3,RIGHT }}},
	{{-1, 0, 0}, {{2,LEFT  },{5,RIGHT },{4,LEFT  },{3,LEFT  }}},
	{{ 0, 1, 0}, {{5,TOP   },{1,TOP   },{0,TOP   },{4,TOP   }}},
	{{ 0,-1, 0}, {{4,BOTTOM},{1,BOTTOM},{0,BOTTOM},{5,BOTTOM}}},
	{{ 0, 0, 1}, {{2,BOTTOM},{1,RIGHT },{0,LEFT  },{3,TOP   }}},
	{{ 0, 0,-1}, {{2,TOP   },{0,RIGHT },{1,LEFT  },{3,BOTTOM}}}
};

// direction from cube center to texel center
RRVec3 CubeSide::getTexelDir(RRVec2 uv) const
{
	return getDir()+cubeSide[neighbour[RIGHT].side].getDir()*uv[0]+cubeSide[neighbour[BOTTOM].side].getDir()*uv[1];
}


/////////////////////////////////////////////////////////////////////////////
//
// collision handler

class ReflectionCubeCollisionHandler : public RRCollisionHandler
{
public:
	void setup(const RRObject* _multiObject, unsigned _singleObjectNumber, float _singleObjectWorldRadius)
	{
		multiObject = _multiObject;
		singleObjectNumber = _singleObjectNumber;
		singleObjectWorldRadius = _singleObjectWorldRadius;
	}
	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_DISTANCE;
		hitTriangleMemory = UINT_MAX;
		hitDistanceMemory = 0;
	}
	virtual bool collides(const RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&RRRay::FILL_SIDE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_DISTANCE);

		// don't collide with self
		RRMesh::PreImportNumber preImport = multiObject->getCollider()->getMesh()->getPreImportTriangle(ray->hitTriangle);
		if (preImport.object==singleObjectNumber)
		{
			// forget about intersections closer than this self-intersection
			if (hitDistanceMemory<ray->hitDistance)
			{
				hitTriangleMemory = UINT_MAX;
				hitDistanceMemory = 0;
			}
			return false;
		}

		// don't collide with transparent points
		RRMaterial* material = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
		if (!material->sideBits[ray->hitFrontSide?0:1].renderFrom || material->specularTransmittance.color==RRVec3(1))
			return false;
		//RRPointMaterial pointMaterial;
		//multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial,colorSpace);
		//if (!pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom || pointMaterial.specularTransmittance.color==RRVec3(1))
		//	return false;

		if (hitTriangleMemory==UINT_MAX || ray->hitDistance<hitDistanceMemory)
		{
			hitTriangleMemory = ray->hitTriangle;
			hitDistanceMemory = ray->hitDistance;
		}
		return ray->hitDistance>singleObjectWorldRadius;
	}
	virtual bool done()
	{
		return hitTriangleMemory!=UINT_MAX;
	}
	unsigned gethHitTriangle()
	{
		return hitTriangleMemory;
	}
private:
	const RRObject* multiObject;
	unsigned singleObjectNumber;
	float singleObjectWorldRadius;
	unsigned hitTriangleMemory;
	float hitDistanceMemory; // hitDistance for hitTriangleMemory
};


/////////////////////////////////////////////////////////////////////////////
//
// gathering kit

CubeGatheringKit::CubeGatheringKit()
{
	inUse = false;
	ray6 = RRRay::create(6);
	handler6 = new ReflectionCubeCollisionHandler[6];
	for (unsigned i=0;i<6;i++)
		ray6[i].collisionHandler = handler6+i;
}

CubeGatheringKit::~CubeGatheringKit()
{
	delete[] handler6;
	delete[] ray6;
}


/////////////////////////////////////////////////////////////////////////////
//
// gather

// OMP parallel inside
// thread safe: yes
// outputs:
//  - triangleNumbers, multiobj postImport numbers, UINT_MAX for skybox, may be NULL
//  - exitanceHdr, float exitance in physical scale, may be NULL
//  - false=exitanceHdr not filled, true=exitanceHdr filled
bool RRSolver::cubeMapGather(RRObjectIllumination* illumination, unsigned layerEnvironment, RRVec3* exitanceHdr)
{
	if (!illumination)
	{
		RR_ASSERT(0);
		return false;
	}
	RRBuffer* reflectionEnvMap = illumination->getLayer(layerEnvironment);
	if (!reflectionEnvMap)
	{
		RR_ASSERT(0);
		return false;
	}
	const RRObject* multiObject = getMultiObject();
	const RRBuffer* environment0 = getEnvironment(0);
	const RRBuffer* environment1 = getEnvironment(1);
	float blendFactor = getEnvironmentBlendFactor();
	const RRColorSpace* colorSpaceForEnv = getColorSpace();
	unsigned gatherSize = reflectionEnvMap->getWidth();
	unsigned numTriangles = getMultiObject() ? getMultiObject()->getCollider()->getMesh()->getNumTriangles() : 0;

	if (gatherSize==illumination->cachedGatherSize
		&& (unsigned short)(numTriangles)==illumination->cachedNumTriangles
		&& (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()<=CENTER_GRANULARITY)
		return false;

	if (illumination->cachedGatherSize!=gatherSize)
	{
		RR_SAFE_DELETE_ARRAY(illumination->cachedTriangleNumbers);
	}
	if (!illumination->cachedTriangleNumbers)
	{
		illumination->cachedTriangleNumbers = new unsigned[6*gatherSize*gatherSize];
	}
	if ((!priv->scene && !priv->packedSolver) || !multiObject)
	{
		// this is legal, renderer asks us to build small cubemap from solver with big environment and 0 objects
		//RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Updating envmap, but lighting is not computed yet, call setStaticObjects() and calculate() first.\n"));
	}

	// simplify tests for blending from if(env1 && blendFactor) to if(env1)
	if (!blendFactor) environment1 = NULL;

	// rather than adding 1 kit to every RRObjectIllumination, we added 10 to solver and pick one of them
	// if user doesn't call updateEnvironmentMap() in parallel, we always use the same first kit
	CubeGatheringKit* kit = NULL;
	#pragma omp critical(cubeGatheringKits)
	{
		for (unsigned i=0;i<10;i++)
			if (!priv->cubeGatheringKits[i].inUse)
				{
					kit = priv->cubeGatheringKits+i;
					kit->inUse = true;
					break;
				}
	}
	if (!kit)
	{
		RRReporter::report(ERRO,"Why are you calling cubeMapGather() for single solver 11 times in parallel? 11th call ignored.\n");
		return false;
	}

	// find out our object number
	unsigned objectNumber;
	{
		const RRObjects& staticObjects = getStaticObjects();
		for (objectNumber=0;objectNumber<staticObjects.size() && &staticObjects[objectNumber]->illumination!=illumination;objectNumber++) ;
	}

	#pragma omp parallel for schedule(dynamic) // fastest: dynamic, static
	for (int side=0;side<6;side++)
	{
		kit->handler6[side].setup(multiObject,objectNumber,illumination->envMapWorldRadius);
		RRRay* ray = kit->ray6+side;
		for (unsigned j=0;j<gatherSize;j++)
			for (unsigned i=0;i<gatherSize;i++)
			{
				unsigned ofs = i+(j+side*gatherSize)*gatherSize;
				RRVec3 dir = cubeSide[side].getTexelDir(gatherSize,i,j);
				unsigned face = UINT_MAX;
				if (multiObject)
				{
					// find face
					ray->rayOrigin = illumination->envMapWorldCenter;
					ray->rayDir = dir.normalized();
					ray->rayLengthMin = 0;
					ray->rayLengthMax = 10000; //!!! hard limit
					ray->rayFlags = RRRay::FILL_TRIANGLE;
					ray->hitObject = multiObject;
					if (ray->hitObject->getCollider()->intersect(ray))
					{
						face = kit->handler6[side].gethHitTriangle(); //ray->hitTriangle;
						if (face>=numTriangles)
						{
							RR_ASSERT(0);
							face = UINT_MAX;
						}
					}
				}
				if (illumination->cachedTriangleNumbers)
				{
					illumination->cachedTriangleNumbers[ofs] = face;
				}
				if (exitanceHdr)
				{
					if (face==UINT_MAX)
					{
						// read exitance of sky
						if (!environment0)
						{
							// no environment
							exitanceHdr[ofs] = RRVec3(0);
						}
						else
						if (!environment1)
						{
							// 1 environment
							exitanceHdr[ofs] = environment0->getElementAtDirection(dir,colorSpaceForEnv);
						}
						else
						{
							// blend of 2 environments
							RRVec3 env0color = environment0->getElementAtDirection(dir,colorSpaceForEnv);
							RRVec3 env1color = environment1->getElementAtDirection(dir,colorSpaceForEnv);
							exitanceHdr[ofs] = env0color*(1-blendFactor)+env1color*blendFactor;
						}
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
					else if (priv->packedSolver)
					{
						// read face exitance
						exitanceHdr[ofs] = priv->packedSolver->getTriangleExitance(face);
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
					else if (priv->scene)
					{
						// read face exitance
						priv->scene->getTriangleMeasure(face,3,RM_RADIOSITY_LINEAR,NULL,exitanceHdr[ofs]);
						RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
					}
#ifdef RR_DEVELOPMET
					else if (priv->customIrradianceRGBA8 && priv->customToPhysical && getMultiObject())
					{
						// no solver, return DDI
						RRMaterial* triangleMaterial = getMultiObject()->getTriangleMaterial(face,NULL,NULL);
						unsigned rgba8 = priv->customIrradianceRGBA8[face];
						RRVec3 physicalIrradiance(priv->customToPhysical[rgba8&0xff],priv->customToPhysical[(rgba8>>8)&0xff],priv->customToPhysical[(rgba8>>16)&0xff]);
						exitanceHdr[ofs] = triangleMaterial ? triangleMaterial->diffuseReflectance.color*physicalIrradiance+triangleMaterial->diffuseEmittance.color : RRVec3(0);
					}
#endif
					else
					{
						// no solver, return darkness
						exitanceHdr[ofs] = RRVec3(0);
					}
				}
			}
	}
	kit->inUse = false;
	illumination->cachedGatherSize = gatherSize;
	illumination->cachedNumTriangles = numTriangles;
	illumination->cachedCenter = illumination->envMapWorldCenter;
	return true;
}


// thread safe: yes
// converts triangle numbers to float exitance in physical scale
#ifdef RR_DEVELOPMET
static void cubeMapConvertTrianglesToExitances(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const unsigned* customIrradianceRGBA8, const RRReal* customToPhysical, const RRObject* multiObject, const RRBuffer* environment0, const RRBuffer* environment1, float blendFactor, const RRColorSpace* colorSpaceForEnv, unsigned size, unsigned* triangleNumbers, RRVec3* exitanceHdr)
#else
static void cubeMapConvertTrianglesToExitances(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const RRBuffer* environment0, const RRBuffer* environment1, float blendFactor, const RRColorSpace* colorSpaceForEnv, unsigned size, unsigned* triangleNumbers, RRVec3* exitanceHdr)
#endif
{
	if (!scene && !packedSolver && !environment0)
	{
		// BTW environment without solver is nothing unusual, called from RendererOfRRSolver::initSpecularReflection()
		RR_ASSERT(0);
		return;
	}
	if (!triangleNumbers || !exitanceHdr)
	{
		RR_ASSERT(0);
		return;
	}

	// simplify tests for blending from if(env1 && blendFactor) to if(env1)
	if (!blendFactor) environment1 = NULL;

#pragma omp parallel for schedule(static)
	for (int ofs=0;ofs<(int)(6*size*size);ofs++)
	{
		unsigned face = triangleNumbers[ofs];
		if (face==UINT_MAX)
		{
			// read exitance of sky
			if (!environment0)
			{
				// no environment
				exitanceHdr[ofs] = RRVec3(0);
			}
			else
			if (!environment1)
			{
				// 1 environment
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				exitanceHdr[ofs] = environment0->getElementAtDirection(dir,colorSpaceForEnv);
				RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
			}
			else
			{
				// blend of 2 environments
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				RRVec3 env0color = environment0->getElementAtDirection(dir,colorSpaceForEnv);
				RRVec3 env1color = environment1->getElementAtDirection(dir,colorSpaceForEnv);
				exitanceHdr[ofs] = env0color*(1-blendFactor)+env1color*blendFactor;
				RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
			}
		}
		else if (packedSolver)
		{
			// read face exitance
			exitanceHdr[ofs] = packedSolver->getTriangleExitance(face);
			RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
		}
		else if (scene)
		{
			// read face exitance
			scene->getTriangleMeasure(face,3,RM_RADIOSITY_LINEAR,NULL,exitanceHdr[ofs]);
			RR_ASSERT(IS_VEC3(exitanceHdr[ofs]));
		}
#ifdef RR_DEVELOPMET
		else if (customIrradianceRGBA8 && customToPhysical && multiObject)
		{
			// no solver, return DDI
			RRMaterial* triangleMaterial = multiObject->getTriangleMaterial(face,NULL,NULL);
			unsigned rgba8 = customIrradianceRGBA8[face];
			RRVec3 physicalIrradiance(customToPhysical[rgba8&0xff],customToPhysical[(rgba8>>8)&0xff],customToPhysical[(rgba8>>16)&0xff]);
			exitanceHdr[ofs] = triangleMaterial ? triangleMaterial->diffuseReflectance.color*physicalIrradiance+triangleMaterial->diffuseEmittance.color : RRVec3(0);
		}
#endif
		else
		{
			// no solver, return darkness
			exitanceHdr[ofs] = RRVec3(0);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// main

// returns number of buffers updated
static unsigned filterToBuffer(unsigned version, RRVec3* gatheredExitance, const RRColorSpace* colorSpace, RRBuffer* buffer)
{
	RR_ASSERT(gatheredExitance);
	if (!gatheredExitance || !buffer || buffer->getType()!=BT_CUBE_TEXTURE) return 0;
	unsigned numElements = buffer->getNumElements();

	for (unsigned i=0;i<numElements;i++)
	{
		buffer->setElement(i,RRVec4(gatheredExitance[i],0),colorSpace);
	}
	// faster but works only for specularEnvMap BF_RGBF,!scaled
	//illumination->specularEnvMap->reset(BT_CUBE_TEXTURE,specularSize,specularSize,6,illumination->specularEnvMap->getFormat(),false,(unsigned char*)gatheredExitance);

	// setting version from solver is not enough, cube would not update if it only moves around scene
	// furthermore, setting version differently may lead to updating always, even if it is not necessary
	//buffer->version = version;
	buffer->version++;
	buffer->version = (version<<16)+(buffer->version&65535);
	return 1;
}


unsigned RRSolver::updateEnvironmentMap(RRObjectIllumination* illumination, unsigned layerEnvironment, unsigned layerLightmap, unsigned layerAmbientMap)
{
	if (!illumination)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned solutionVersion = getSolutionVersion();
	bool centerMoved = (illumination->envMapWorldCenter-illumination->cachedCenter).abs().sum()>CENTER_GRANULARITY;
	RRBuffer* reflectionEnvMap = illumination->getLayer(layerEnvironment);
	unsigned gatherSize = (reflectionEnvMap && (centerMoved || (reflectionEnvMap->version>>16)!=(solutionVersion&65535))) ? reflectionEnvMap->getWidth() : 0;
	if (!gatherSize)
	{
		return 0;
	}

	if (!priv->scene && !priv->packedSolver && !getEnvironment())
	{
		// BTW environment without solver is nothing unusual, called from RendererOfRRSolver::initSpecularReflection()
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"No solver, no environment, reflection map will be updated to black, call calculate().\n"));
	}
	if (!priv->scene && priv->packedSolver && !priv->customIrradianceRGBA8)
	{
		// this is legal, scene may be lit by skybox
		//RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Fireball lights not detected yet, reflection map will be updated to black, call calculate().\n"));
	}

	// alloc temp space
	RRVec3* gatheredExitance = new RRVec3[6*gatherSize*gatherSize];

	if (!cubeMapGather(illumination,layerEnvironment,gatheredExitance))
	{
#ifdef RR_DEVELOPMET
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getDirectIllumination(),priv->customToPhysical,getMultiObject(),getEnvironment(0),getEnvironment(1),getEnvironmentBlendFactor(),getColorSpace(),gatherSize,illumination->cachedTriangleNumbers,gatheredExitance);
#else
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getEnvironment(0),getEnvironment(1),getEnvironmentBlendFactor(),getColorSpace(),gatherSize,illumination->cachedTriangleNumbers,gatheredExitance);
#endif
	}

	unsigned updatedMaps = 0;
	if (illumination->cachedGatherSize)
	{
		// fill envmap
		updatedMaps += filterToBuffer(solutionVersion,gatheredExitance,priv->colorSpace,reflectionEnvMap);
	}

	// cleanup
	delete[] gatheredExitance;

	return updatedMaps;
}


} // namespace

