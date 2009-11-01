// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>

#include "../RRMesh/RRMeshFilter.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRObject.h"
#include "RRObjectFilter.h"
#include "RRObjectFilterTransformed.h"
#include "RRObjectMulti.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObject::RRObject()
{
	illumination = NULL;
}

RRObject::~RRObject()
{
	delete illumination;
}

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& material, const RRScaler* scaler) const
{
	if (!scaler)
	{
		// Slow path for end users, material is undefined on input, we fill it in custom scale.
		// Start by reading per-triangle quality, adapters usually have it cached.
		const RRMaterial* perTriangleMaterial = getTriangleMaterial(t,NULL,NULL);
		if (perTriangleMaterial)
		{
			material = *perTriangleMaterial;
		}
		else
		{
			RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial returned NULL."));
			material.reset(false);
			RR_ASSERT(0);
		}
	}
	else
	{
		// Fast path for solver, material is prefilled with physical scale colors and custom scale textures
		// (RRObjectWithPhysicalMaterialsImpl::getPointMaterial() prefilled it, no one else is expected to do it).
		// We will only update selected colors in physical scale.
	}

	// Improve precision using textures.
	if (material.diffuseEmittance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material.diffuseEmittance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		material.diffuseEmittance.color = material.diffuseEmittance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
		if (scaler)
		{
			scaler->getPhysicalScale(material.diffuseEmittance.color);
		}
	}
	if (material.specularReflectance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material.specularReflectance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		material.specularReflectance.color = material.specularReflectance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
		if (scaler)
		{
			scaler->getPhysicalFactor(material.specularReflectance.color);
		}
	}
	if (material.diffuseReflectance.texture && material.specularTransmittance.texture==material.diffuseReflectance.texture && material.specularTransmittanceInAlpha)
	{
		// optional optimized path: transmittance in diffuse map alpha
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material.diffuseReflectance.texcoord);
		RRVec2 materialUv= triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		RRVec4 rgba = material.diffuseReflectance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
		material.diffuseReflectance.color = rgba * rgba[3];
		material.specularTransmittance.color = RRVec3(1-rgba[3]);
		if (rgba[3]==0)
			material.sideBits[0].catchFrom = material.sideBits[1].catchFrom = 0;
		if (scaler)
		{
			scaler->getPhysicalFactor(material.diffuseReflectance.color);
			scaler->getPhysicalFactor(material.specularTransmittance.color);
		}
	}
	else
	{
		// generic path
		if (material.specularTransmittance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material.specularTransmittance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			RRVec4 rgba = material.specularTransmittance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
			material.specularTransmittance.color = material.specularTransmittanceInAlpha ? RRVec3(1-rgba[3]) : rgba;
			if (material.specularTransmittance.color==RRVec3(1))
				material.sideBits[0].catchFrom = material.sideBits[1].catchFrom = 0;
			if (scaler)
			{
				scaler->getPhysicalFactor(material.specularTransmittance.color);
			}
		}
		if (material.diffuseReflectance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material.diffuseReflectance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			material.diffuseReflectance.color = RRVec3(material.diffuseReflectance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0)))
				// we multiply dif texture by opacity on the fly
				// because real world data are often in this format
				// (typically texture with RGB=dif, A=opacity, where dif is NOT premultiplied)
				* (RRVec3(1)-material.specularTransmittance.color);
			if (scaler)
			{
				scaler->getPhysicalFactor(material.diffuseReflectance.color);
			}
		}
	}
}

void RRObject::getTriangleLod(unsigned t, LodInfo& out) const
{
	out.base = (void*)this;
	out.level = 0;
	out.distanceMin = 0;
	out.distanceMax = 1e35f;
}

const RRMatrix3x4* RRObject::getWorldMatrix()
{
	return NULL;
}

void* RRObject::getCustomData(const char* name) const
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject instance factory

RRMesh* RRObject::createWorldSpaceMesh()
{
	return this ? getCollider()->getMesh()->createTransformed(getWorldMatrix()) : NULL;
}

RRObject* RRObject::createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation)
{
	return new RRTransformedObjectFilter(this,negScaleMakesOuterInner,intersectTechnique,aborting,cacheLocation);
}

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, unsigned speed, const char* cacheLocation)
{
	switch(speed)
	{
		case 0: return RRObjectMultiSmall::create(objects,numObjects,intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		case 1: return RRObjectMultiFast::create(objects,numObjects,intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		default: return RRObjectMultiFast::create(objects,numObjects,intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,true,cacheLocation);
	}
		
}

// Moved to file with exceptions enabled:
// void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)

} // namespace
