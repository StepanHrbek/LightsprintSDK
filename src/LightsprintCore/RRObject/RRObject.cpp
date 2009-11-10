// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <set>
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
// FaceGroups

void RRObject::FaceGroups::getBlending(bool& containsBlended, bool& containsNonBlended) const
{
	containsBlended = false;
	containsNonBlended = false;
	for (unsigned g=0;g<size();g++)
	{
		rr::RRMaterial* material = (*this)[g].material;
		if (material)
		{
			if (material->needsBlending())
			{
				containsBlended = true;
			}
			else
			{
				containsNonBlended = true;
			}
		}
	}
}

void RRObject::FaceGroups::getTexcoords(RRVector<unsigned>& _texcoords, bool _forUnwrap, bool _forDiffuse, bool _forSpecular, bool _forEmissive, bool _forTransparent) const
{
	std::set<unsigned> texcoords;
	for (unsigned fg=0; fg<size(); fg++)
	{
		rr::RRMaterial* material = (*this)[fg].material;
		if (material)
		{
			if (_forUnwrap)
				texcoords.insert(material->lightmapTexcoord);
			if (_forDiffuse && material->diffuseReflectance.texture)
				texcoords.insert(material->diffuseReflectance.texcoord);
			if (_forSpecular && material->specularReflectance.texture)
				texcoords.insert(material->specularReflectance.texcoord);
			if (_forEmissive && material->diffuseEmittance.texture)
				texcoords.insert(material->diffuseEmittance.texcoord);
			if (_forTransparent && material->specularTransmittance.texture)
				texcoords.insert(material->specularTransmittance.texcoord);
		}
	}
	_texcoords.clear();
	for (std::set<unsigned>::const_iterator i=texcoords.begin();i!=texcoords.end();++i)
		_texcoords.push_back(*i);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObject::RRObject()
{
	collider = NULL;
	worldMatrix = NULL;
	illumination = NULL;
}

RRObject::~RRObject()
{
	delete illumination;
	delete worldMatrix;
}

void RRObject::setCollider(const RRCollider* _collider)
{
	collider = _collider;
	if (!_collider)
		RRReporter::report(WARN,"setCollider(NULL) called, collider must never be NULL.\n");
}

RRMaterial* RRObject::getTriangleMaterial(unsigned t, const class RRLight* light, const RRObject* receiver) const
{
	unsigned tt = t;
	for (unsigned g=0;g<faceGroups.size();g++)
	{
		if (tt<faceGroups[g].numTriangles)
			return faceGroups[g].material;
		tt -= faceGroups[g].numTriangles;
	}
	unsigned numTriangles = getCollider()->getMesh()->getNumTriangles();
	if (t>numTriangles)
	{
		RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial(%d) out of range, mesh has %d triangles.\n",t,numTriangles));
	}
	else
	if (faceGroups.size())
	{
		RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::faceGroups incomplete, only %d triangles, mesh has %d.\n",t-tt,numTriangles));
	}
	else
	{
		RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::faceGroups empty.\n"));
	}
	return NULL;
}

void RRObject::updateFaceGroupsFromTriangleMaterials()
{
	// How do I test whether getTriangleMaterial() is the same as RRObject::getTriangleMaterial() ?
	//if (&getTriangleMaterial==&RRObject::getTriangleMaterial)
	//{
	//	RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"Illegal updateFaceGroupsFromTriangleMaterials() call, fill faceGroups manually or reimplement getTriangleMaterial().\n"));
	//	return;
	//}
	faceGroups.clear();
	RRMaterial* material = 0;
	unsigned numTriangles = getCollider()->getMesh()->getNumTriangles();
	for (unsigned t=0;t<numTriangles;t++)
	{
		RRMaterial* material2 = getTriangleMaterial(t,NULL,NULL);
		if (!t || material2!=material)
		{
			faceGroups.push_back(FaceGroup(material=material2,1));
		}
		else
		{
			faceGroups[faceGroups.size()-1].numTriangles++;
		}
	}
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
			RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial returned NULL.\n"));
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

const RRMatrix3x4* RRObject::getWorldMatrix() const
{
	return worldMatrix;
}

void RRObject::setWorldMatrix(const RRMatrix3x4* _worldMatrix)
{
	if (_worldMatrix && !_worldMatrix->isIdentity())
	{
		if (!worldMatrix)
			worldMatrix = new RRMatrix3x4;
		*worldMatrix = *_worldMatrix;
	}
	else
	{
		RR_SAFE_DELETE(worldMatrix);
	}
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

RRObject* RRObject::createMultiObject(const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, unsigned speed, const char* cacheLocation)
{
	if (!objects)
		return NULL;
	switch(speed)
	{
		case 0: return RRObjectMultiSmall::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		case 1: return RRObjectMultiFast::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		default: return RRObjectMultiFast::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,true,cacheLocation);
	}
		
}

// Moved to file with exceptions enabled:
// void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)

} // namespace
