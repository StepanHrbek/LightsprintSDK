// --------------------------------------------------------------------------
// Non-intrusive serialization of Lightsprint structures.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRSERIALIZATION_H
#define RRSERIALIZATION_H

// This is optional header-only extension of LightsprintCore library.
// You can use it to serialize Lightsprint structures, but only if you have boost, http://boost.org.
// If you don't have boost, simply don't include this header, e.g. by disabling #define SUPPORT_LIGHTSPRINT.

#include "Lightsprint/RRScene.h"
#include "RRRelocation.h"
#include <boost/serialization/binary_object.hpp> // either install boost or don't serialize
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <boost/unordered_set.hpp>

// Helper for relocating absolute paths.
// Must be set up before serialization.
// Global, don't serialize in multiple threads at the same time.
RRRelocator g_relocator;

namespace boost {
namespace serialization {

// switches between readable array and fast binary object
//#define make_array_or_binary(a,s) make_array(a,s)
#define make_array_or_binary(a,s) make_binary_object(a,(s)*sizeof(*(a)))

//------------------------------ RRVec2 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec2& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
}

//------------------------------ RRVec3 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec3& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
}

//------------------------------ RRVec4 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec4& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
	ar & make_nvp("w",a.w);
}

//------------------------------ RRMatrix3x4 -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMatrix3x4& a, const unsigned int version)
{
	save(ar,a.m);
}

template<class Archive>
void load(Archive & ar, rr::RRMatrix3x4& a, const unsigned int version)
{
	load(ar,a.m);
}

//------------------------------ RRString ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRString& a, const unsigned int version)
{
	save(ar,std::string(a.c_str()));
}

template<class Archive>
void load(Archive & ar, rr::RRString& a, const unsigned int version)
{
	std::string s;
	load(ar,s);
	a = s.c_str();
}

//------------------------------ RRBuffer -------------------------------------

// Instead of RRBuffer, we serialize dynamically created proxy object.
// It's necessary to circumvent boost limitations:
//  - no custom allocation (http://lists.boost.org/boost-users/2005/05/11773.php)
//  - all derived classes must be registered
// If two pointers point to the same buffer, only one proxy is created.
// We statically store set of proxy instances, they must be alive during load, free them by freeMemory() afterwards.
// Static storage is not thread safe, don't serialize two scenes at once
// (can be fixed by moving static proxy storage to Archive class).

class RRBufferProxy
{
public:
	rr::RRBuffer* buffer;

	RRBufferProxy()
	{
		instances.insert(this);
	}
	~RRBufferProxy()
	{
		delete buffer; // all unique buffers are created and deleted once
		instances.erase(this);
	}
	static void freeMemory()
	{
		while (!instances.empty())
			delete *instances.begin();
	}
private:
	static boost::unordered_set<RRBufferProxy*> instances; // not thread safe, don't serialize two scenes at once
};

boost::unordered_set<RRBufferProxy*> RRBufferProxy::instances;

#define prefix_buffer(b)          (*(RRBufferProxy**)&(b))
#define postfix_buffer(Archive,b) {if (Archive::is_loading() && b) b = prefix_buffer(b)->buffer?prefix_buffer(b)->buffer->createReference():NULL;} // all non-unique buffers get their own reference


template<class Archive>
void save(Archive & ar, const RRBufferProxy& aa, const unsigned int version)
{
	// only unique non-NULL buffers get here
	rr::RRBuffer& a = *(rr::RRBuffer*)&aa;
	ar & make_nvp("filename",a.filename); // should be absolute path
	if (a.filename.empty())
	{
		rr::RRBufferType type = a.getType();
		ar & make_nvp("type",type);
		unsigned width = a.getWidth();
		ar & make_nvp("width",width);
		unsigned height = a.getHeight();
		ar & make_nvp("height",height);
		unsigned depth = a.getDepth();
		ar & make_nvp("depth",depth);
		rr::RRBufferFormat format = a.getFormat();
		ar & make_nvp("format",format);
/*		enum Compression
		{
			C_NONE
		};
		Compression compression = C_NONE;
		ar & make_nvp("compression",compression);*/
		bool scaled = a.getScaled();
		ar & make_nvp("scaled",scaled);
		unsigned char* data = a.lock(rr::BL_READ); // if you call saves in parallel (why?), they may lock in parallel. locking is thread safe in our buffer implementations, but is allowed to be unsafe in other people's implementations.
		ar & make_nvp("data",binary_object(data,a.getBufferBytes()));
		a.unlock();
	}
}

template<class Archive>
void load(Archive & ar, RRBufferProxy& a, const unsigned int version)
{
	// only unique non-NULL buffers get here
	rr::RRString filename;
	ar & make_nvp("filename",filename);
	if (filename.empty())
	{
		rr::RRBufferType type;
		ar & make_nvp("type",type);
		unsigned width,height,depth;
		ar & make_nvp("width",width);
		ar & make_nvp("height",height);
		ar & make_nvp("depth",depth);
		rr::RRBufferFormat format;
		ar & make_nvp("format",format);
		bool scaled;
		ar & make_nvp("scaled",scaled);
		a.buffer = rr::RRBuffer::create(type,width,height,depth,format,scaled,NULL);
		ar & make_nvp("data",binary_object(a.buffer->lock(rr::BL_DISCARD_AND_WRITE),a.buffer->getBufferBytes()));
		a.buffer->unlock();
	}
	else
	{
		// Disable reporter when trying different paths for textures.
		RRReporter* oldReporter = RRReporter::getReporter();
		RRReporter::setReporter(NULL);

		// Look for file at expected new location.
		std::string relocatedFilename = g_relocator.relocatedFilename(filename.c_str());
		a.buffer = rr::RRBuffer::load(relocatedFilename.c_str(),NULL);
		if (!a.buffer && relocatedFilename!=filename.c_str())
		{
			// Look for file at original location (where it was at save time).
			a.buffer = rr::RRBuffer::load(filename.c_str(),NULL);
		}

		RRReporter::setReporter(oldReporter);
	}
}

//------------------------------ RRSideBits -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRSideBits& a, const unsigned int version)
{
	bool renderFrom = a.renderFrom;
	ar & make_nvp("renderFrom",renderFrom);
	bool emitTo = a.emitTo;
	ar & make_nvp("emitTo",emitTo);
	bool catchFrom = a.catchFrom;
	ar & make_nvp("catchFrom",catchFrom);
	bool legal = a.legal;
	ar & make_nvp("legal",legal);
	bool receiveFrom = a.receiveFrom;
	ar & make_nvp("receiveFrom",receiveFrom);
	bool reflect = a.reflect;
	ar & make_nvp("reflect",reflect);
	bool transmitFrom = a.transmitFrom;
	ar & make_nvp("transmitFrom",transmitFrom);
}

template<class Archive>
void load(Archive & ar, rr::RRSideBits& a, const unsigned int version)
{
	bool renderFrom;
	ar & make_nvp("renderFrom",renderFrom);
	a.renderFrom = renderFrom;
	bool emitTo;
	ar & make_nvp("emitTo",emitTo);
	a.emitTo = emitTo;
	bool catchFrom;
	ar & make_nvp("catchFrom",catchFrom);
	a.catchFrom = catchFrom;
	bool legal;
	ar & make_nvp("legal",legal);
	a.legal = legal;
	bool receiveFrom;
	ar & make_nvp("receiveFrom",receiveFrom);
	a.receiveFrom = receiveFrom;
	bool reflect;
	ar & make_nvp("reflect",reflect);
	a.reflect = reflect;
	bool transmitFrom;
	ar & make_nvp("transmitFrom",transmitFrom);
	a.transmitFrom = transmitFrom;
}

//------------------------------ RRMaterial::Property -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRMaterial::Property& a, const unsigned int version)
{
	ar & make_nvp("color",a.color);
	ar & make_nvp("texture",prefix_buffer(a.texture)); postfix_buffer(Archive,a.texture);
	ar & make_nvp("texcoord",a.texcoord);
}

//------------------------------ RRMaterial -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRMaterial& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("sideBits",a.sideBits);
	ar & make_nvp("diffuseReflectance",a.diffuseReflectance);
	ar & make_nvp("diffuseEmittance",a.diffuseEmittance);
	ar & make_nvp("specularReflectance",a.specularReflectance);
	ar & make_nvp("specularTransmittance",a.specularTransmittance);
	ar & make_nvp("specularTransmittanceInAlpha",a.specularTransmittanceInAlpha);
	ar & make_nvp("specularTransmittanceKeyed",a.specularTransmittanceKeyed);
	ar & make_nvp("refractionIndex",a.refractionIndex);
	ar & make_nvp("lightmapTexcoord",a.lightmapTexcoord);
	ar & make_nvp("minimalQualityForPointMaterials",a.minimalQualityForPointMaterials);
}

//----------------------------- RRVector<C> ------------------------------------

template<class Archive, class C>
void save(Archive & ar, const rr::RRVector<C>& a, const unsigned int version)
{
	unsigned count = a.size();
	ar & make_nvp("count",count);
	for (unsigned i=0;i<count;i++)
	{
		ar & make_nvp("item",a[i]);
	}
}

template<class Archive, class C>
void load(Archive & ar, rr::RRVector<C>& a, const unsigned int version)
{
	RR_ASSERT(!a.size());
	a.clear();
	unsigned count;
	ar & make_nvp("count",count);
	while (count--)
	{
		C item;
		ar & make_nvp("item", item);
		a.push_back(item);
	}
}

template<class Archive, class C>
void save(Archive & ar, const rr::RRVector<C*>& a, const unsigned int version)
{
	unsigned count = 0;
	for (unsigned i=0;i<a.size();i++)
		if (a[i])
			count++;
	ar & make_nvp("count",count);
	for (unsigned i=0;i<a.size();i++)
		if (a[i]) // skip NULL items
			ar & make_nvp("item",*a[i]);
}

template<class Archive, class C>
void load(Archive & ar, rr::RRVector<C*>& a, const unsigned int version)
{
	RR_ASSERT(!a.size());
	a.clear();
	unsigned count;
	ar & make_nvp("count",count);
	while (count--)
	{
		C* item = new C;
		ar & make_nvp("item", *item);
		a.push_back(item);
	}
}

template<class Archive, class C>
void serialize(Archive & ar, rr::RRVector<C>& a, const unsigned int file_version)
{
	boost::serialization::split_free(ar, a, file_version);
}

//------------------------------ RRLight ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRLight& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("type",a.type);
	ar & make_nvp("position",a.position);
	ar & make_nvp("direction",a.direction);
	ar & make_nvp("outerAngleRad",a.outerAngleRad);
	ar & make_nvp("radius",a.radius);
	ar & make_nvp("color",a.color);
	ar & make_nvp("distanceAttenuationType",a.distanceAttenuationType);
	ar & make_nvp("polynom",a.polynom);
	ar & make_nvp("fallOffExponent",a.fallOffExponent);
	ar & make_nvp("spotExponent",a.spotExponent);
	ar & make_nvp("fallOffAngleRad",a.fallOffAngleRad);
	ar & make_nvp("castShadows",a.castShadows);
	ar & make_nvp("rtProjectedTexture",prefix_buffer(a.rtProjectedTexture)); postfix_buffer(Archive,a.rtProjectedTexture);
	ar & make_nvp("rtMaxShadowSize",a.rtMaxShadowSize);
	// skip customData;
}

//----------------------------- RRLights ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRLights& a, const unsigned int version)
{
	rr::RRVector<rr::RRLight*>& aa = a;
	serialize(ar,aa,version);
}

//------------------------------ RRMesh::Triangle -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMesh::Triangle& a, const unsigned int version)
{
	save(ar,a.m);
}

template<class Archive>
void load(Archive & ar, rr::RRMesh::Triangle& a, const unsigned int version)
{
	load(ar,a.m);
}

//------------------------------ RRMeshArrays ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMeshArrays& a, const unsigned int version)
{
	// sizes
	ar & make_nvp("numTriangles",a.numTriangles);
	ar & make_nvp("numVertices",a.numVertices);
	rr::RRVector<unsigned> texcoords;
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			texcoords.push_back(i);
	ar & make_nvp("texcoords",texcoords);
	bool tangents = a.tangent && a.bitangent;
	ar & make_nvp("tangents",tangents);

	// arrays
	ar & make_nvp("triangle",make_array_or_binary(a.triangle,a.numTriangles));
	ar & make_nvp("position",make_array_or_binary(a.position,a.numVertices));
	ar & make_nvp("normal",make_array_or_binary(a.normal,a.numVertices));
	if (tangents)
	{
		ar & make_nvp("tangent",make_array_or_binary(a.tangent,a.numVertices));
		ar & make_nvp("bitangent",make_array_or_binary(a.bitangent,a.numVertices));
	}
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			ar & make_nvp("texcoord",make_array_or_binary(a.texcoord[i],a.numVertices));
}

template<class Archive>
void load(Archive & ar, rr::RRMeshArrays& a, const unsigned int version)
{
	// sizes
	unsigned numTriangles;
	ar & make_nvp("numTriangles",numTriangles);
	unsigned numVertices;
	ar & make_nvp("numVertices",numVertices);
	rr::RRVector<unsigned> texcoords;
	ar & make_nvp("texcoords",texcoords);
	bool tangents;
	ar & make_nvp("tangents",tangents);
	a.resizeMesh(numTriangles,numVertices,&texcoords,tangents);

	// arrays
	ar & make_nvp("triangle",make_array_or_binary(a.triangle,numTriangles));
	ar & make_nvp("position",make_array_or_binary(a.position,numVertices));
	ar & make_nvp("normal",make_array_or_binary(a.normal,numVertices));
	if (tangents)
	{
		ar & make_nvp("tangent",make_array_or_binary(a.tangent,numVertices));
		ar & make_nvp("bitangent",make_array_or_binary(a.bitangent,numVertices));
	}
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			ar & make_nvp("texcoord",make_array_or_binary(a.texcoord[i],numVertices));
}

//------------------------------ RRMeshProxy -------------------------------------

// Instead of RRMesh, we serialize dynamically created proxy object.
// It's necessary to circumvent boost limitations:
//  - no custom allocation (http://lists.boost.org/boost-users/2005/05/11773.php)
//  - all derived classes must be registered
// We statically store set of proxy instances, they must be alive during load, free them by freeMemory() afterwards.
// Static storage is not thread safe, don't serialize two scenes at once
// (can be fixed by moving static proxy storage to Archive class).

class RRMeshProxy
{
public:
	rr::RRMeshArrays* mesh;

	RRMeshProxy()
	{
		instances.insert(this);
	}
	~RRMeshProxy()
	{
		instances.erase(this);
	}
	static void freeMemory()
	{
		while (!instances.empty())
			delete *instances.begin();
	}
private:
	static boost::unordered_set<RRMeshProxy*> instances; // not thread safe, don't serialize two scenes at once
};

boost::unordered_set<RRMeshProxy*> RRMeshProxy::instances;

template<class Archive>
void save(Archive & ar, const RRMeshProxy& a, const unsigned int version)
{
	// here we get only unique non-NULL meshes
	// we can't save any mesh, only RRMeshArrays, so conversion might be necessary
	rr::RRMesh* mesh = (rr::RRMesh*)&a;
	rr::RRMeshArrays* meshArrays = dynamic_cast<rr::RRMeshArrays*>(mesh);
	if (!meshArrays)
	{
		rr::RRVector<unsigned> texcoords;
		rr::RRMesh::TriangleMapping tm;
		for (unsigned i=0;i<100;i++)
			if (mesh->getTriangleMapping(0,tm,i))
				texcoords.push_back(i);
		meshArrays = mesh->createArrays(true,texcoords);
	}
	save(ar,*meshArrays,version);
	if (meshArrays!=mesh)
		delete meshArrays;
}

template<class Archive>
void load(Archive & ar, RRMeshProxy& a, const unsigned int version)
{
	// here we get only unique non-NULL meshes
	a.mesh = new RRMeshArrays;
	load(ar,*a.mesh,version);
}

//------------------------------ RRObject::FaceGroup -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObject::FaceGroup& a, const unsigned int version)
{
	ar & make_nvp("numTriangles",a.numTriangles);
	ar & make_nvp("material",a.material);
}

//----------------------------- RRObject::FaceGroups ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObject::FaceGroups& a, const unsigned int version)
{
	rr::RRVector<rr::RRObject::FaceGroup>& aa = a;
	serialize(ar,aa,version);
}

//------------------------------ RRObject ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRObject& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("faceGroups",a.faceGroups);
	ar & make_nvp("worldMatrix",a.getWorldMatrixRef());
	{
		RRMeshProxy* proxy = (RRMeshProxy*)a.getCollider()->getMesh();
		if (!proxy)
			RRReporter::report(ERRO,"Saved NULL mesh.\n");
		ar & make_nvp("mesh",proxy);
	}
}

template<class Archive>
void load(Archive & ar, rr::RRObject& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("faceGroups",a.faceGroups);
	{
		RRMatrix3x4 worldMatrix;
		ar & make_nvp("worldMatrix",worldMatrix);
		a.setWorldMatrix(&worldMatrix);
	}
	{
		RRMeshProxy* proxy = NULL;
		ar & make_nvp("mesh",proxy);
		if (proxy && proxy->mesh)
		{
			bool aborting = false;
			a.setCollider(rr::RRCollider::create(proxy->mesh,rr::RRCollider::IT_LINEAR,aborting));
		}
		else
		{
			RRReporter::report(ERRO,"Loaded NULL mesh.\n");
		}
	}
}

//----------------------------- RRObjects ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObjects& a, const unsigned int version)
{
	rr::RRVector<rr::RRObject*>& aa = a;
	serialize(ar,aa,version);
}

//------------------------------ RRScene ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRScene& a, const unsigned int version)
{
	ar & make_nvp("objects",a.objects);
	ar & make_nvp("lights",a.lights);
	ar & make_nvp("environment",prefix_buffer(a.environment)); postfix_buffer(Archive,a.environment);
	// must be called after load. there's nothing to free after save
	RRBufferProxy::freeMemory();
	RRMeshProxy::freeMemory();
}

//---------------------------------------------------------------------------

} // namespace
} // namespace

BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMatrix3x4)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRString)
BOOST_SERIALIZATION_SPLIT_FREE(RRBufferProxy)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMesh::Triangle)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMeshArrays)
BOOST_SERIALIZATION_SPLIT_FREE(RRMeshProxy)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRSideBits)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRObject)

#endif
