// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out) const
{
	// Fill all material properties using material averages.
	const RRMaterial* material = getTriangleMaterial(t,NULL,NULL);
	if (material)
	{
		out = *material;
	}
	else
	{
		LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial returned NULL."));
		out.reset(false);
		RR_ASSERT(0);
	}

	// Improve precision using textures.
	if (material->diffuseEmittance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material->diffuseEmittance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		out.diffuseEmittance.color = material->diffuseEmittance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
	}
	if (material->specularReflectance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material->specularReflectance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		out.specularReflectance.color = material->specularReflectance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
	}
	if (material->diffuseReflectance.texture && material->specularTransmittance.texture==material->diffuseReflectance.texture && material->specularTransmittanceInAlpha)
	{
		// optional optimized path: transmittance in diffuse map alpha
		RRMesh::TriangleMapping triangleMapping;
		getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material->diffuseReflectance.texcoord);
		RRVec2 materialUv= triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		rr::RRVec4 rgba = material->diffuseReflectance.texture->getElement(rr::RRVec3(materialUv[0],materialUv[1],0));
		out.diffuseReflectance.color = rgba * rgba[3];
		out.specularTransmittance.color = rr::RRVec3(1-rgba[3]);
		if (rgba[3]==0)
			out.sideBits[0].catchFrom = out.sideBits[1].catchFrom = 0;
	}
	else
	{
		// generic path
		if (material->specularTransmittance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material->specularTransmittance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			RRVec4 rgba = material->specularTransmittance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0));
			out.specularTransmittance.color = material->specularTransmittanceInAlpha ? RRVec3(1-rgba[3]) : rgba;
			if (out.specularTransmittance.color==RRVec3(1))
				out.sideBits[0].catchFrom = out.sideBits[1].catchFrom = 0;
		}
		if (material->diffuseReflectance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,material->diffuseReflectance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			out.diffuseReflectance.color = RRVec3(material->diffuseReflectance.texture->getElement(RRVec3(materialUv[0],materialUv[1],0)))
				// we multiply dif texture by opacity on the fly
				// because real world data are often in this format
				// (typically texture with RGB=dif, A=opacity, where dif is NOT premultiplied)
				* (RRVec3(1)-out.specularTransmittance.color);
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

RRObject* RRObject::createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float vertexWeldDistance, bool optimizeTriangles, unsigned speed, const char* cacheLocation)
{
	switch(speed)
	{
		case 0: return RRObjectMultiSmall::create(objects,numObjects,intersectTechnique,aborting,vertexWeldDistance,optimizeTriangles,false,cacheLocation);
		case 1: return RRObjectMultiFast::create(objects,numObjects,intersectTechnique,aborting,vertexWeldDistance,optimizeTriangles,false,cacheLocation);
		default: return RRObjectMultiFast::create(objects,numObjects,intersectTechnique,aborting,vertexWeldDistance,optimizeTriangles,true,cacheLocation);
	}
		
}

// Moved to file with exceptions enabled:
// void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)

} // namespace
