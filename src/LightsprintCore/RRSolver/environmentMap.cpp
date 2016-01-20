// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
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
	virtual void init(RRRay& ray)
	{
		ray.rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_DISTANCE;
		hitTriangleMemory = UINT_MAX;
		hitDistanceMemory = 0;
	}
	virtual bool collides(const RRRay& ray)
	{
		RR_ASSERT(ray.rayFlags&RRRay::FILL_SIDE);
		RR_ASSERT(ray.rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray.rayFlags&RRRay::FILL_DISTANCE);

		// don't collide with self
		RRMesh::PreImportNumber preImport = multiObject->getCollider()->getMesh()->getPreImportTriangle(ray.hitTriangle);
		if (preImport.object==singleObjectNumber)
		{
			// forget about intersections closer than this self-intersection
			if (hitDistanceMemory<ray.hitDistance)
			{
				hitTriangleMemory = UINT_MAX;
				hitDistanceMemory = 0;
			}
			return false;
		}

		// don't collide with transparent points
		RRMaterial* material = multiObject->getTriangleMaterial(ray.hitTriangle,nullptr,nullptr);
		if (!material->sideBits[ray.hitFrontSide?0:1].renderFrom || material->specularTransmittance.color==RRVec3(1))
			return false;
		//RRPointMaterial pointMaterial;
		//multiObject->getPointMaterial(ray.hitTriangle,ray.hitPoint2d,pointMaterial,colorSpace);
		//if (!pointMaterial.sideBits[ray.hitFrontSide?0:1].renderFrom || pointMaterial.specularTransmittance.color==RRVec3(1))
		//	return false;

		if (hitTriangleMemory==UINT_MAX || ray.hitDistance<hitDistanceMemory)
		{
			hitTriangleMemory = ray.hitTriangle;
			hitDistanceMemory = ray.hitDistance;
		}
		return ray.hitDistance>singleObjectWorldRadius;
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
//  - triangleNumbers, multiobj postImport numbers, UINT_MAX for skybox, may be nullptr
//  - exitanceHdr, float exitance in physical scale, may be nullptr
//  - false=exitanceHdr not filled, true=exitanceHdr filled
bool RRSolver::cubeMapGather(RRObjectIllumination* illumination, unsigned layerEnvironment)
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
	if (!blendFactor) environment1 = nullptr;

	// rather than adding 1 kit to every RRObjectIllumination, we added 10 to solver and pick one of them
	// if user doesn't call updateEnvironmentMap() in parallel, we always use the same first kit
	CubeGatheringKit* kit = nullptr;
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

	#pragma omp parallel for schedule(dynamic) if(gatherSize*gatherSize*6>=RR_OMP_MIN_ELEMENTS/10)
	for (int side=0;side<6;side++)
	{
		kit->handler6[side].setup(multiObject,objectNumber,illumination->envMapWorldRadius);
		RRRay& ray = kit->ray6[side];
		for (unsigned j=0;j<gatherSize;j++)
			for (unsigned i=0;i<gatherSize;i++)
			{
				unsigned ofs = i+(j+side*gatherSize)*gatherSize;
				RRVec3 dir = cubeSide[side].getTexelDir(gatherSize,i,j);
				unsigned face = UINT_MAX;
				if (multiObject)
				{
					// find face
					ray.rayOrigin = illumination->envMapWorldCenter;
					ray.rayDir = dir.normalized();
					ray.rayLengthMin = 0;
					ray.rayLengthMax = 10000; //!!! hard limit
					ray.rayFlags = RRRay::FILL_TRIANGLE;
					ray.hitObject = multiObject;
					if (ray.hitObject->getCollider()->intersect(ray))
					{
						face = kit->handler6[side].gethHitTriangle(); //ray.hitTriangle;
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
				if (1)
				{
					RRVec3 exitanceHdr;
					if (face==UINT_MAX)
					{
						// read exitance of sky
						if (!environment0)
						{
							// no environment
							exitanceHdr = RRVec3(0);
						}
						else
						if (!environment1)
						{
							// 1 environment
							exitanceHdr = environment0->getElementAtDirection(dir,colorSpaceForEnv);
						}
						else
						{
							// blend of 2 environments
							RRVec3 env0color = environment0->getElementAtDirection(dir,colorSpaceForEnv);
							RRVec3 env1color = environment1->getElementAtDirection(dir,colorSpaceForEnv);
							exitanceHdr = env0color*(1-blendFactor)+env1color*blendFactor;
						}
						RR_ASSERT(IS_VEC3(exitanceHdr));
					}
					else if (priv->packedSolver)
					{
						// read face exitance
						exitanceHdr = priv->packedSolver->getTriangleExitance(face);
						RR_ASSERT(IS_VEC3(exitanceHdr));
					}
					else if (priv->scene)
					{
						// read face exitance
						priv->scene->getTriangleMeasure(face,3,RM_RADIOSITY_LINEAR,nullptr,exitanceHdr);
						RR_ASSERT(IS_VEC3(exitanceHdr));
					}
#ifdef RR_DEVELOPMET
					else if (priv->customIrradianceRGBA8 && priv->customToPhysical && getMultiObject())
					{
						// no solver, return DDI
						RRMaterial* triangleMaterial = getMultiObject()->getTriangleMaterial(face,nullptr,nullptr);
						unsigned rgba8 = priv->customIrradianceRGBA8[face];
						RRVec3 physicalIrradiance(priv->customToPhysical[rgba8&0xff],priv->customToPhysical[(rgba8>>8)&0xff],priv->customToPhysical[(rgba8>>16)&0xff]);
						exitanceHdr = triangleMaterial ? triangleMaterial->diffuseReflectance.color*physicalIrradiance+triangleMaterial->diffuseEmittance.color : RRVec3(0);
					}
#endif
					else
					{
						// no solver, return darkness
						exitanceHdr = RRVec3(0);
					}
					reflectionEnvMap->setElement(ofs,RRVec4(exitanceHdr,0),colorSpaceForEnv);
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
static void cubeMapConvertTrianglesToExitances(const RRStaticSolver* scene, const RRPackedSolver* packedSolver, const RRBuffer* environment0, const RRBuffer* environment1, float blendFactor, const RRColorSpace* colorSpaceForEnv, unsigned size, unsigned* triangleNumbers, RRBuffer* cube)
#endif
{
	if (!scene && !packedSolver && !environment0)
	{
		// BTW environment without solver is nothing unusual, called from RendererOfRRSolver::initSpecularReflection()
		RR_ASSERT(0);
		return;
	}
	if (!triangleNumbers || !cube)
	{
		RR_ASSERT(0);
		return;
	}

	// simplify tests for blending from if(env1 && blendFactor) to if(env1)
	if (!blendFactor) environment1 = nullptr;

	#pragma omp parallel for schedule(static) if(size*size*6>=RR_OMP_MIN_ELEMENTS)
	for (int ofs=0;ofs<(int)(6*size*size);ofs++)
	{
		unsigned face = triangleNumbers[ofs];
		RRVec3 exitanceHdr;
		if (face==UINT_MAX)
		{
			// read exitance of sky
			if (!environment0)
			{
				// no environment
				exitanceHdr = RRVec3(0);
			}
			else
			if (!environment1)
			{
				// 1 environment
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				exitanceHdr = environment0->getElementAtDirection(dir,colorSpaceForEnv);
				RR_ASSERT(IS_VEC3(exitanceHdr));
			}
			else
			{
				// blend of 2 environments
				RRVec3 dir = cubeSide[ofs/(size*size)].getTexelDir(size,ofs%size,(ofs/size)%size);
				RRVec3 env0color = environment0->getElementAtDirection(dir,colorSpaceForEnv);
				RRVec3 env1color = environment1->getElementAtDirection(dir,colorSpaceForEnv);
				exitanceHdr = env0color*(1-blendFactor)+env1color*blendFactor;
				RR_ASSERT(IS_VEC3(exitanceHdr));
			}
		}
		else if (packedSolver)
		{
			// read face exitance
			exitanceHdr = packedSolver->getTriangleExitance(face);
			RR_ASSERT(IS_VEC3(exitanceHdr));
		}
		else if (scene)
		{
			// read face exitance
			scene->getTriangleMeasure(face,3,RM_RADIOSITY_LINEAR,nullptr,exitanceHdr);
			RR_ASSERT(IS_VEC3(exitanceHdr));
		}
#ifdef RR_DEVELOPMET
		else if (customIrradianceRGBA8 && customToPhysical && multiObject)
		{
			// no solver, return DDI
			RRMaterial* triangleMaterial = multiObject->getTriangleMaterial(face,nullptr,nullptr);
			unsigned rgba8 = customIrradianceRGBA8[face];
			RRVec3 physicalIrradiance(customToPhysical[rgba8&0xff],customToPhysical[(rgba8>>8)&0xff],customToPhysical[(rgba8>>16)&0xff]);
			exitanceHdr[ofs] = triangleMaterial ? triangleMaterial->diffuseReflectance.color*physicalIrradiance+triangleMaterial->diffuseEmittance.color : RRVec3(0);
		}
#endif
		else
		{
			// no solver, return darkness
			exitanceHdr = RRVec3(0);
		}
		cube->setElement(ofs,RRVec4(exitanceHdr,0),colorSpaceForEnv);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// main


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
	unsigned gatherSize = (reflectionEnvMap && reflectionEnvMap->getType()==BT_CUBE_TEXTURE && (centerMoved || (reflectionEnvMap->version>>16)!=(solutionVersion&65535))) ? reflectionEnvMap->getWidth() : 0;
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

	if (!cubeMapGather(illumination,layerEnvironment))
	{
#ifdef RR_DEVELOPMET
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getDirectIllumination(),priv->customToPhysical,getMultiObject(),getEnvironment(0),getEnvironment(1),getEnvironmentBlendFactor(),getColorSpace(),gatherSize,illumination->cachedTriangleNumbers,reflectionEnvMap);
#else
		cubeMapConvertTrianglesToExitances(priv->scene,priv->packedSolver,getEnvironment(0),getEnvironment(1),getEnvironmentBlendFactor(),getColorSpace(),gatherSize,illumination->cachedTriangleNumbers,reflectionEnvMap);
#endif
	}

	if (illumination->cachedGatherSize)
	{
		// setting version from solver is not enough, cube would not update if it only moves around scene
		// furthermore, setting version differently may lead to updating always, even if it is not necessary
		//buffer->version = version;
		reflectionEnvMap->version++;
		reflectionEnvMap->version = (solutionVersion<<16)+(reflectionEnvMap->version&65535);
		return 1;
	}
	return 0;
}


} // namespace

