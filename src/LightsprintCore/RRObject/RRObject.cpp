// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../RRMesh/RRMeshFilter.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRObject.h"
#include "RRObjectFilter.h"
#include "RRObjectFilterTransformed.h"
#include "RRObjectMulti.h"
#include "../NumReports.h"

namespace rr
{

//! Returns formatted string (printf-like) for immediate use.
//
//! Fully static, no allocations.
//! Has slots for several strings, call to tmpstr() overwrites one of previously returned strings.
const char* tmpstr(const char* fmt, ...)
{
	enum
	{
		MAX_STRINGS=2, // checkConsistency() needs 2 strings
		MAX_STRING_SIZE=1000
	};
	static unsigned i = 0;
	static char bufs[MAX_STRINGS][MAX_STRING_SIZE+1];
	char* buf = bufs[++i%MAX_STRINGS];
	va_list argptr;
	va_start (argptr,fmt);
	_vsnprintf (buf,MAX_STRING_SIZE,fmt,argptr);
	buf[MAX_STRING_SIZE] = 0;
	va_end (argptr);
	return buf;
}


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
		material.diffuseEmittance.color = material.diffuseEmittance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0));
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
		material.specularReflectance.color = material.specularReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0));
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
		RRVec4 rgba = material.diffuseReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0));
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
			RRVec4 rgba = material.specularTransmittance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0));
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
			material.diffuseReflectance.color = RRVec3(material.diffuseReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0)))
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

extern const char* checkUnwrapConsistency(const RRObject* object); // our small helper in lightmap.cpp

unsigned RRObject::checkConsistency(const char* _objectNumber) const
{
	const char* objectName = tmpstr("%s%s%s%s%s",name.c_str(),name.empty()?"":" ",_objectNumber?"(":"",_objectNumber?_objectNumber:"",_objectNumber?")":"");
	NumReports numReports(objectName);

	// collider, mesh
	if (!getCollider())
	{
		RRReporter::report(ERRO,"getCollider()=NULL.\n");
		return 1;
	}
	if (!getCollider()->getMesh())
	{
		RRReporter::report(ERRO,"getCollider()->getMesh()=NULL.\n");
		return 1;
	}
	getCollider()->getMesh()->checkConsistency(UINT_MAX,objectName,&numReports);

	// matrix
	const RRMatrix3x4* world = getWorldMatrix();
	if (world)
	{
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				if (!_finite(world->m[i][j]))
				{
					numReports++;
					RRReporter::report(ERRO,"Broken world transformation.\n");
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
		RRReporter::report(ERRO,"faceGroups define materials for %d triangles out of %d.\n",numTrianglesInFacegroups,getCollider()->getMesh()->getNumTriangles());
	}

	// materials
	for (unsigned g=0;g<faceGroups.size();g++)
	{
		RRMaterial* material = faceGroups[g].material;
		if (!material)
		{
			numReports++;
			RRReporter::report(ERRO,"NULL material.\n");
		}
		else
		{
			// todo: do texcoords exist?
		}
	}

	// lightmapTexcoord
	unsigned noUnwrap = 0;
	for (unsigned g=0;g<faceGroups.size();g++)
		if (faceGroups[g].material->lightmapTexcoord==UINT_MAX)
			noUnwrap++;
	if (noUnwrap==faceGroups.size())
	{
			numReports++;
			RRReporter::report(WARN,"No unwrap.\n");
	}
	else
	if (noUnwrap)
	{
			numReports++;
			RRReporter::report(WARN,"Part of unwrap missing.\n");
	}
	for (unsigned g=1;g<faceGroups.size();g++)
	{
		if (faceGroups[g].material->lightmapTexcoord!=faceGroups[g-1].material->lightmapTexcoord)
		{
			numReports++;
			RRReporter::report(WARN,"Combines materials with different lightmapTexcoord (%d,%d..).\n",faceGroups[g-1].material->lightmapTexcoord,faceGroups[g].material->lightmapTexcoord);
			break;
		}
	}

	// unwrap
	if (!noUnwrap)
	{
		const char* unwrapWarning = checkUnwrapConsistency(this);
		if (unwrapWarning)
		{
			numReports++;
			RRReporter::report(WARN,"%s\n",unwrapWarning);
		}
	}

	return numReports;
}

unsigned RRObjects::checkConsistency(const char* objectType) const
{
	unsigned numReports = 0;
	for (unsigned i=0;i<size();i++)
	{
		const char* objectNumber = tmpstr("%s%sobject %d/%d",objectType?objectType:"",objectType?" ":"",i,size());
		numReports += (*this)[i]->checkConsistency(objectNumber);
	}
	return numReports;
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
	const char* result = tmpstr("%s%05d.%s",path?path:"",objectIndex,finalExt);
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
