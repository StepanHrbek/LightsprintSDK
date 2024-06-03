// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Object adapter.
// --------------------------------------------------------------------------

#include "../RRMesh/RRMeshFilter.h"
#include "../RRStaticSolver/rrcore.h"
#include "../RRCollider/RRCollisionHandler.h"
#include "RRObjectFilter.h"
#include "RRObjectFilterTransformed.h"
#include "../NumReports.h"
#include <cstdio> // vsnprintf

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
	if (vsnprintf(buf, MAX_STRING_SIZE, fmt, argptr) < 0)
	{
		// error formatting
		buf[0] = 0;
	};
	va_end (argptr);
	return buf;
}


//////////////////////////////////////////////////////////////////////////////
//
// LayerParameters

RRBuffer* RRObject::LayerParameters::createBuffer(bool forceFloats, bool forceAlpha, const wchar_t* insertBeforeExtension) const
{
	RRBufferFormat f;
	switch (actualFormat)
	{
		case BF_RGB:
		case BF_BGR:
			f = forceFloats ? (forceAlpha?BF_RGBAF:BF_RGBF) : (forceAlpha?BF_RGBA:actualFormat);
			break;
		case BF_RGBF:
			f = forceAlpha ? BF_RGBAF : actualFormat;
			break;
		case BF_RGBA:
		case BF_RGBAF:
			f = forceFloats ? BF_RGBAF : actualFormat;
			break;
		case BF_DXT1:
			f = forceFloats ? (forceAlpha?BF_RGBAF:BF_RGBF) : (forceAlpha?BF_RGBA:BF_RGB);
			break;
		case BF_DXT3:
		case BF_DXT5:
			f = forceFloats ? BF_RGBAF : BF_RGBA;
			break;
		case BF_LUMINANCE:
			f = forceFloats ? BF_LUMINANCEF : actualFormat;
			break;
		case BF_DEPTH:
		case BF_LUMINANCEF:
		default:
			f = actualFormat;
			break;
	}
	RRBuffer* buffer = RRBuffer::create(actualType,actualWidth,actualHeight,1,f,actualScaled,nullptr);
	if (buffer)
	{
		if (insertBeforeExtension)
		{
			// insert layer name before extension
			std::wstring filename = actualFilename.w_str();
			int ofs = (int)filename.rfind('.',-1);
			if (ofs>=0) filename.insert(ofs,insertBeforeExtension);
			buffer->filename = RR_STDW2RR(filename); // [#36] constructed name of empty lightmap, envmap etc
		}
		else
			buffer->filename = actualFilename; // [#36] constructed name of empty lightmap, envmap etc
	}
	return buffer;
}


//////////////////////////////////////////////////////////////////////////////
//
// FaceGroups

bool RRObject::FaceGroups::containsEmittance() const
{
	for (unsigned g=0;g<size();g++)
	{
		RRMaterial* material = (*this)[g].material;
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
	collider = nullptr;
	worldMatrix = nullptr;
	enabled = true;
	isDynamic = false;
}

RRObject::~RRObject()
{
	delete worldMatrix;
}

void RRObject::setCollider(RRCollider* _collider)
{
	collider = _collider;
	if (!_collider)
		RRReporter::report(ERRO,"setCollider(nullptr) called, collider must never be nullptr.\n");
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
	return nullptr;
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
		RRMaterial* material2 = getTriangleMaterial(t,nullptr,nullptr);
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

// Expects material prefilled with getTriangleMaterial(), both color and colorLinear.
// Updates color and (if colorSpace!=nullptr) colorLinear for properties with texture.
static void updatePointMaterial(const rr::RRMesh* mesh, unsigned t, RRVec2 uv, const RRColorSpace* colorSpace, bool interpolated, RRPointMaterial& material)
{
	// Make color (and possibly also colorLinear) more accurate using textures.
	if (material.diffuseEmittance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		mesh->getTriangleMapping(t,triangleMapping,material.diffuseEmittance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		material.diffuseEmittance.colorLinear = material.diffuseEmittance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0),colorSpace,interpolated);
	}
	if (material.specularReflectance.texture)
	{
		RRMesh::TriangleMapping triangleMapping;
		mesh->getTriangleMapping(t,triangleMapping,material.specularReflectance.texcoord);
		RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		RRVec4 specColor = material.specularReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0),colorSpace,interpolated);
		material.specularReflectance.colorLinear = specColor;
		// shininess is modulated by specular map alpha. does nothing if specular map is RGB only [#18]
		if (material.specularModel==RRMaterial::PHONG || material.specularModel==RRMaterial::BLINN_PHONG)
		{
			RRReal specularShininessSqrt = powf(RR_CLAMPED(material.specularShininess,1,1e10f),0.01f); // [#20]
			specularShininessSqrt = 1+(specularShininessSqrt-1)*specColor.w;
			material.specularShininess = powf(specularShininessSqrt,100.0f);
		}
		else
			material.specularShininess *= specColor.w;
	}
	if (material.diffuseReflectance.texture && material.specularTransmittance.texture==material.diffuseReflectance.texture && material.specularTransmittanceInAlpha)
	{
		// optional optimized path: transmittance in diffuse map alpha
		RRMesh::TriangleMapping triangleMapping;
		mesh->getTriangleMapping(t,triangleMapping,material.diffuseReflectance.texcoord);
		RRVec2 materialUv= triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		RRVec4 rgba = material.diffuseReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0),nullptr,interpolated);
		RRReal specularTransmittance = 1-rgba[3];
		if (material.specularTransmittanceMapInverted)
			specularTransmittance = 1-specularTransmittance;
		if (material.specularTransmittanceKeyed)
			specularTransmittance = (specularTransmittance>=material.specularTransmittanceThreshold) ? 1.f : 0.f;
		if (specularTransmittance==1)
			material.sideBits[0].catchFrom = material.sideBits[1].catchFrom =
			material.sideBits[0].renderFrom = material.sideBits[1].renderFrom = 0;
		material.diffuseReflectance.colorLinear = material.diffuseReflectance.color = rgba;
		material.specularTransmittance.colorLinear = material.specularTransmittance.color = RRVec3(specularTransmittance);
		if (colorSpace)
		{
			colorSpace->toLinear(material.diffuseReflectance.colorLinear);
			colorSpace->toLinear(material.specularTransmittance.colorLinear);
		}
		// [#39] we multiply dif by opacity on the fly, because real world data are often in this format
		material.diffuseReflectance.color *= (RRVec3(1)-material.specularTransmittance.color); // multiply cust color in cust.scale - inaccurate, but result probably not used
		material.diffuseReflectance.colorLinear *= (RRVec3(1)-material.specularTransmittance.colorLinear); // multiply phys color in phys scale - accurate, used by pathtracer, makes cloud borders in clouds.rr3 white
	}
	else
	{
		// generic path
		if (material.diffuseReflectance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			mesh->getTriangleMapping(t,triangleMapping,material.diffuseReflectance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			material.diffuseReflectance.colorLinear = material.diffuseReflectance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0),colorSpace,interpolated);
		}
		if (material.specularTransmittance.texture)
		{
			RRMesh::TriangleMapping triangleMapping;
			mesh->getTriangleMapping(t,triangleMapping,material.specularTransmittance.texcoord);
			RRVec2 materialUv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
			RRVec4 rgba = material.specularTransmittance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0),nullptr,interpolated);
			material.specularTransmittance.color = material.specularTransmittanceInAlpha ? RRVec3(1-rgba[3]) : rgba;
			if (material.specularTransmittanceMapInverted)
				material.specularTransmittance.color = RRVec3(1)-material.specularTransmittance.color;
			if (material.specularTransmittanceKeyed)
				material.specularTransmittance.color = (material.specularTransmittance.color.avg()>=material.specularTransmittanceThreshold) ? RRVec3(1) : RRVec3(0);
			if (material.specularTransmittance.color==RRVec3(1))
				material.sideBits[0].catchFrom = material.sideBits[1].catchFrom =
				material.sideBits[0].renderFrom = material.sideBits[1].renderFrom = 0;
			material.specularTransmittance.colorLinear = material.specularTransmittance.color;
			if (colorSpace)
				colorSpace->toLinear(material.specularTransmittance.colorLinear);
			// [#39] we multiply dif by opacity on the fly, because real world data are often in this format
			material.diffuseReflectance.color *= (RRVec3(1)-material.specularTransmittance.color); // multiply cust color in cust.scale - inaccurate, but result probably not used
			material.diffuseReflectance.colorLinear *= (RRVec3(1)-material.specularTransmittance.colorLinear); // multiply phys color in phys scale - accurate, used by pathtracer, makes cloud borders in clouds.rr3 white
		}
	}
}

void RRObject::getPointMaterial(unsigned t, RRVec2 uv, const RRColorSpace* colorSpace, bool interpolated, RRPointMaterial& material) const
{
	// Material is undefined on input, fill it with per-triangle quality first.
	const RRMaterial* perTriangleMaterial = getTriangleMaterial(t,nullptr,nullptr);
	if (perTriangleMaterial)
	{
		material = *perTriangleMaterial;
	}
	else
	{
		RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRObject::getTriangleMaterial returned nullptr.\n"));
		material.reset(false);
		RR_ASSERT(0);
	}

	// Improve precision using textures.
	if (material.minimalQualityForPointMaterials<UINT_MAX)
		updatePointMaterial(getCollider()->getMesh(),t,uv,colorSpace,interpolated,material);
}

void RRObject::getTriangleLod(unsigned t, LodInfo& out) const
{
	out.base = (void*)this;
	out.level = 0;
	out.distanceMin = 0;
	out.distanceMax = 1e35f;
}

void RRObject::setWorldMatrix(const RRMatrix3x4* _worldMatrix)
{
	if (_worldMatrix && !_worldMatrix->isIdentity())
	{
		if (!worldMatrix)
			worldMatrix = new RRMatrix3x4Ex;
		*worldMatrix = RRMatrix3x4Ex(*_worldMatrix);
	}
	else
	{
		RR_SAFE_DELETE(worldMatrix);
	}
}

const RRMatrix3x4Ex* RRObject::getWorldMatrix() const
{
	return worldMatrix;
}

const RRMatrix3x4Ex& RRObject::getWorldMatrixRef() const
{
	const RRMatrix3x4Ex* wm = getWorldMatrix();
	static RRMatrix3x4Ex identity = RRMatrix3x4Ex(RRMatrix3x4::identity());
	return wm?*wm:identity;
}

void* RRObject::getCustomData(const char* name) const
{
	return nullptr;
}

extern const char* checkUnwrapConsistency(const RRObject* object); // our small helper in lightmap.cpp

unsigned RRObject::checkConsistency(const char* _objectNumber) const
{
	const char* objectName = tmpstr("%s%s%s%s%s",name.c_str(),name.empty()?"":" ",_objectNumber?"(":"",_objectNumber?_objectNumber:"",_objectNumber?")":"");
	NumReports numReports(objectName);

	// collider, mesh
	if (!getCollider())
	{
		RRReporter::report(ERRO,"getCollider()=nullptr.\n");
		return 1;
	}
	const rr::RRMesh* mesh = getCollider()->getMesh();
	if (!mesh)
	{
		RRReporter::report(ERRO,"getCollider()->getMesh()=nullptr.\n");
		return 1;
	}
	mesh->checkConsistency(UINT_MAX,objectName,&numReports);

	// matrix
	const RRMatrix3x4* world = getWorldMatrix();
	if (world)
	{
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				if (!std::isfinite(world->m[i][j]))
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
	if (numTrianglesInFacegroups!=mesh->getNumTriangles())
	{
		numReports++;
		RRReporter::report(ERRO,"faceGroups define materials for %d triangles out of %d.\n",numTrianglesInFacegroups,mesh->getNumTriangles());
	}

	// materials
	for (unsigned g=0;g<faceGroups.size();g++)
	{
		RRMaterial* material = faceGroups[g].material;
		if (!material)
		{
			numReports++;
			RRReporter::report(ERRO,"nullptr material.\n");
		}
		else
		{
			RRMesh::TriangleMapping tm;
			if (mesh->getNumTriangles())
				for (unsigned i=0;i<5;i++)
				{
					const RRMaterial::Property& prop =
						(i==0)?material->diffuseReflectance:(
						 (i==1)?material->diffuseEmittance:(
						  (i==2)?material->specularReflectance:(
						   (i==3)?material->specularTransmittance:
						    material->bumpMap
							// no need to test also lightmap
						)));
					if (prop.texture && !mesh->getTriangleMapping(0,tm,prop.texcoord))
					{
						numReports++;
						RRReporter::report(WARN,"Material %s wants uv channel %d, but mesh does not have it.\n",material->name.c_str(),prop.texcoord);
					}
				}
		}
	}

	// lightmap.texcoord
	unsigned noUnwrap = 0;
	for (unsigned g=0;g<faceGroups.size();g++)
		if (faceGroups[g].material->lightmap.texcoord==UINT_MAX)
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
		if (faceGroups[g].material->lightmap.texcoord!=faceGroups[g-1].material->lightmap.texcoord)
		{
			numReports++;
			RRReporter::report(WARN,"Combines materials with different lightmap.texcoord (%d,%d..).\n",faceGroups[g-1].material->lightmap.texcoord,faceGroups[g].material->lightmap.texcoord);
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
		const char* objectNumber = tmpstr("%s%sobject %d/%" RR_SIZE_T "d",objectType?objectType:"",objectType?" ":"",i,size());
		numReports += (*this)[i]->checkConsistency(objectNumber);
	}
	return numReports;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject illumination

void RRObject::updateIlluminationEnvMapCenter()
{
	RRVec3 mini,maxi,center;
	getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
	illumination.envMapWorldCenter = getWorldMatrixRef().getTransformedPosition(center);
	illumination.envMapWorldRadius = (maxi-mini).length()/2*getWorldMatrixRef().getScale().abs().avg();
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject collisions

RRCollisionHandler* RRObject::createCollisionHandlerFirstVisible() const
{
	return new RRCollisionHandlerFirstVisible(this,false);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject instance factory

RRMesh* RRObject::createWorldSpaceMesh() const
{
	return this ? getCollider()->getMesh()->createTransformed(getWorldMatrix()) : nullptr;
}



//////////////////////////////////////////////////////////////////////////////
//
// RRObject recommends

// formats filename from prefix(path), object number and postfix(ext)
inline void formatFilename(const RRString& path, const RRString& suggestedName, const RRString& objectName, const RRString& ext, bool isVertexBuffer, RRString& out)
{
	wchar_t* tmp = nullptr;
	const wchar_t* finalExt;
	if (isVertexBuffer)
	{
		if (ext.empty())
		{
			finalExt = L"rrbuffer";
		}
		else
		{
			// transforms ext "bent_normals.png" to finalExt "bent_normals.rrbuffer"
			// ("bent_normals.vbu" is transformed too, although .vbu is vertex buffer format)
			unsigned i = (unsigned)wcslen(ext.w_str());
			tmp = new wchar_t[i+10];
			memcpy(tmp,ext.w_str(),(i+1)*sizeof(wchar_t));
			while (i && tmp[i-1]!='.') i--;
			wcscpy(tmp+i,L"rrbuffer");
			finalExt = tmp;
		}
	}
	else
	{
		if (ext.empty())
		{
			finalExt = L"png";
		}
		else
		{
			finalExt = ext.w_str();
		}
	}
	if (!suggestedName.empty())
		out.format(L"%ls%ls.%ls",path.w_str(),suggestedName.w_str(),finalExt);
	else
	{
		std::wstring name = RR_RR2STDW(objectName);
		for (unsigned i=0;i<name.size();i++)
			if (wcschr(L"<>:\"/\\|?*",name[i]))
				name[i] = '_';
		out.format(L"%ls%ls.%ls",path.w_str(),name.c_str(),finalExt);
	}
	delete[] tmp;
}

void RRObject::recommendLayerParameters(RRObject::LayerParameters& layerParameters) const
{
	layerParameters.actualWidth  = layerParameters.suggestedMapWidth ? layerParameters.suggestedMapWidth : getCollider()->getMesh()->getNumVertices();
	layerParameters.actualHeight = layerParameters.suggestedMapWidth ? layerParameters.suggestedMapHeight : 1;
	layerParameters.actualType   = layerParameters.suggestedMapWidth ? BT_2D_TEXTURE : BT_VERTEX_BUFFER;
	layerParameters.actualFormat = layerParameters.suggestedMapWidth ? BF_RGB : BF_RGBF;
	layerParameters.actualScaled = layerParameters.suggestedMapWidth ? true : false;
	formatFilename(layerParameters.suggestedPath,layerParameters.suggestedName,name,layerParameters.suggestedExt,layerParameters.actualType==BT_VERTEX_BUFFER,layerParameters.actualFilename);
}

} // namespace
