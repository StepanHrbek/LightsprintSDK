// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <set>
#include <string>
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
// LayerParameters

RRBuffer* RRObject::LayerParameters::createBuffer(bool forceFloats) const
{
	RRBufferFormat f;
	switch (actualFormat)
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBF:
			f = forceFloats ? BF_RGBF : actualFormat;
			break;
		case BF_RGBA:
		case BF_RGBAF:
			f = forceFloats ? BF_RGBAF : actualFormat;
			break;
		case BF_DXT1:
			f = forceFloats ? BF_RGBF : BF_RGB;
			break;
		case BF_DXT3:
		case BF_DXT5:
			f = forceFloats ? BF_RGBAF : BF_RGBA;
			break;
		default:
			f = actualFormat;
			break;
	}
	return RRBuffer::create(actualType,actualWidth,actualHeight,1,f,actualScaled,NULL);
}


//////////////////////////////////////////////////////////////////////////////
//
// FaceGroups

bool RRObject::FaceGroups::containsEmittance() const
{
	for (unsigned g=0;g<size();g++)
	{
		rr::RRMaterial* material = (*this)[g].material;
		if (material)
		{
			if (material->diffuseEmittance.color!=RRVec3(0) || material->diffuseEmittance.texture)
				return true;
		}
	}
	return false;
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
}

RRObject::~RRObject()
{
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

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, RRPointMaterial& material, const RRScaler* scaler) const
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

const RRMatrix3x4& RRObject::getWorldMatrixRef() const
{
	const rr::RRMatrix3x4* wm = getWorldMatrix();
	static RRMatrix3x4 identity = {1,0,0,0, 0,1,0,0, 0,0,1,0};
	return wm?*wm:identity;
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

unsigned RRObject::checkConsistency(const char* _objectNumber) const
{
	unsigned numReports = 0;
	std::string objectName(name.c_str());
	if (!name.empty()) objectName += " ";
	if (_objectNumber) objectName += std::string("(")+_objectNumber+")";

	// collider, mesh
	if (!getCollider())
	{
		RRReporter::report(ERRO,"Object %s has getCollider()=NULL.\n",objectName.c_str());
		return 1;
	}
	if (!getCollider()->getMesh())
	{
		RRReporter::report(ERRO,"Object %s has getCollider()->getMesh()=NULL.\n",objectName.c_str());
		return 1;
	}

	// matrix
	const RRMatrix3x4* world = getWorldMatrix();
	if (world)
	{
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				if (!_finite(world->m[i][j]))
				{
					numReports++;
					RRReporter::report(ERRO,"Object %s has broken world transformation.\n",objectName.c_str());
					break;
				}
	}

	// facegroups
	unsigned numTrianglesInFacegroups = 0;
	for (unsigned g=0;g<faceGroups.size();g++)
		numTrianglesInFacegroups += faceGroups[g].numTriangles;
	if (numTrianglesInFacegroups!=getCollider()->getMesh()->getNumTriangles())
	{
		numReports++;
		RRReporter::report(ERRO,"Object %s faceGroups define materials for %d triangles out of %d.\n",objectName.c_str(),numTrianglesInFacegroups,getCollider()->getMesh()->getNumTriangles());
	}

	// materials
	for (unsigned g=0;g<faceGroups.size();g++)
	{
		RRMaterial* material = faceGroups[g].material;
		if (!material)
		{
			numReports++;
			RRReporter::report(ERRO,"Object %s has NULL material.\n",objectName.c_str());
		}
		else
		{
			// todo: do texcoords exist?
		}
	}

	// lightmapTexcoord
	unsigned lightmapTexcoord = UINT_MAX;
	for (unsigned g=0;g<faceGroups.size();g++)
	{
		if (faceGroups[g].material->lightmapTexcoord!=UINT_MAX)
		{
			if (lightmapTexcoord!=UINT_MAX && lightmapTexcoord!=faceGroups[g].material->lightmapTexcoord)
			{
				numReports++;
				RRReporter::report(WARN,"Object %s combines materials with different lightmapTexcoord (%d,%d..).\n",objectName.c_str(),lightmapTexcoord,faceGroups[g].material->lightmapTexcoord);
				break;
			}
			lightmapTexcoord = faceGroups[g].material->lightmapTexcoord;
		}
	}

	return numReports + getCollider()->getMesh()->checkConsistency(lightmapTexcoord,objectName.c_str());
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject instance factory

RRMesh* RRObject::createWorldSpaceMesh()
{
	return this ? getCollider()->getMesh()->createTransformed(getWorldMatrix()) : NULL;
}


RRObject* RRObject::createMultiObject(const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, unsigned speed, const char* cacheLocation)
{
	if (!objects || !objects->size())
		return NULL;
	switch(speed)
	{
		case 0: return RRObjectMultiSmall::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		case 1: return RRObjectMultiFast::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		default: return RRObjectMultiFast::create(&(*objects)[0],objects->size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,true,cacheLocation);
	}
		
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject recommends

static char *bp(const char *fmt, ...)
{
	static char msg[1000];
	va_list argptr;
	va_start (argptr,fmt);
	_vsnprintf (msg,999,fmt,argptr);
	msg[999] = 0;
	va_end (argptr);
	return msg;
}

// formats filename from prefix(path), object number and postfix(ext)
const char* formatFilename(const char* path, unsigned objectIndex, const char* ext, bool isVertexBuffer)
{
	char* tmp = NULL;
	const char* finalExt;
	if (isVertexBuffer)
	{
		if (!ext)
		{
			finalExt = "vbu";
		}
		else
		{
			// transforms ext "bent_normals.png" to finalExt "bent_normals.vbu"
			tmp = new char[strlen(ext)+5];
			strcpy(tmp,ext);
			unsigned i = (unsigned)strlen(ext);
			while (i && tmp[i-1]!='.') i--;
			strcpy(tmp+i,"vbu");
			finalExt = tmp;
		}
	}
	else
	{
		if (!ext)
		{
			finalExt = "png";
		}
		else
		{
			finalExt = ext;
		}
	}
	const char* result = bp("%s%05d.%s",path?path:"",objectIndex,finalExt);
	delete[] tmp;
	return result;
}

void RRObject::recommendLayerParameters(RRObject::LayerParameters& layerParameters) const
{
	layerParameters.actualWidth = layerParameters.suggestedMapSize ? layerParameters.suggestedMapSize : getCollider()->getMesh()->getNumVertices();
	layerParameters.actualHeight = layerParameters.suggestedMapSize ? layerParameters.suggestedMapSize : 1;
	layerParameters.actualType = layerParameters.suggestedMapSize ? BT_2D_TEXTURE : BT_VERTEX_BUFFER;
	layerParameters.actualFormat = layerParameters.suggestedMapSize ? BF_RGB : BF_RGBF;
	layerParameters.actualScaled = layerParameters.suggestedMapSize ? true : false;
	free(layerParameters.actualFilename);
	layerParameters.actualFilename = _strdup(formatFilename(layerParameters.suggestedPath,layerParameters.objectIndex,layerParameters.suggestedExt,layerParameters.actualType==BT_VERTEX_BUFFER));
}

// Moved to file with exceptions enabled:
// void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)

} // namespace
