// --------------------------------------------------------------------------
// Imports Collada scene (FCDocument) into RRDynamicSolver
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to Collada meshes,
// objects, materials, loaded by FCollada library.
// You can replace Collada with your internal format and adapt this code
// so it works with your data.
//
// Wrappers don't allocate additional memory, values are read from 
// Collada document.
//
// Instancing is supported, multiple instances with different
// positions and materials share one collider and mesh.
//
// TEXCOORD 0 channel from document is assigned to diffuse maps.
// If TEXCOORD 1 channel is present, it is assigned to lightmaps,
// otherwise automatic unwrap for lightmaps is generated.
// You can easily tweak this setup, see TEXCOORD,0 and TEXCOORD,1.

#if 1 // 0 disables Collada support, 1 enables

#include <cassert>
#include <cmath>
#include <map>

#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectProfile.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDEffectTechnique.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTexture.h"
#include "FUtils/FUFileManager.h"

#include "RRObjectCollada.h"
#include "Lightsprint/DemoEngine/Texture.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"

#ifdef _DEBUG
	#ifdef RR_STATIC
		#pragma comment(lib,"FColladaSD.lib")
	#else
		#pragma comment(lib,"FColladaD.lib")
	#endif
#else
	#ifdef RR_STATIC
		#pragma comment(lib,"FColladaSR.lib")
	#else
		#pragma comment(lib,"FCollada.lib")
	#endif
#endif

using namespace rr;

#define SCALE 1 // scale vertices
#define SWAP_YZ


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new wrappers.
// Define VERIFY to enable verification of wrappers and data.
// Display system messages using RRReporter.
// Once your code/data are verified and don't emit messages via RRReporter,
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada

// See RRMesh documentation for details
// on individual member functions.

class RRMeshCollada : public RRMesh
{
public:
	RRMeshCollada(const FCDGeometryMesh* mesh);

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual void         getTriangleMapping(unsigned t, TriangleMapping& out) const;

private:
	const FCDGeometryMesh* mesh;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada load

// Creates complete internal copy of mesh geometry, so it doesn't depend on original collada mesh.
RRMeshCollada::RRMeshCollada(const FCDGeometryMesh* amesh)
{
	mesh = amesh;

#ifdef VERIFY
	verify();
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRChanneledData

bool getTriangleVerticesData(const FCDGeometryMesh* mesh, FUDaeGeometryInput::Semantic semantic, unsigned index, unsigned floatsPerVertex, unsigned itemIndex, void* itemData, unsigned itemSize)
{
	assert(itemSize==12*floatsPerVertex);
	//const FCDGeometrySource* source = mesh->FindSourceByType(semantic);
	FCDGeometrySourceConstList sources;
	mesh->FindSourcesByType(semantic,sources);
	if(sources.size()<=index)
	{
		return false;
	}
	const FCDGeometrySource* source = sources[index];

	if(!source)
	{
		return false;
	}
	const FloatList& data = source->GetSourceData();
	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			LIMITED_TIMES(10,assert(polygons->IsTriangles())); // this is expensive check, do it only few times
			size_t relativeIndex = itemIndex - polygons->GetFaceOffset();
			if(relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
			{
				const UInt32List* indices = polygons->FindIndices(source);
				if(indices)
				{
					float* out = (float*)itemData;
					size_t s = indices->size();
					for(unsigned j=0;j<3;j++)
					{
						assert(relativeIndex*3+j<s);
						for(unsigned k=0;k<floatsPerVertex;k++)
						{
							*out++ = data[(*indices)[relativeIndex*3+j]*3+k];
						}
					}
					return true;
				}
			}
		}
	}
	assert(0);
	return false;
}

void RRMeshCollada::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(RRVec2[3]);
			return;

		default:
			// unsupported channel
			RRMesh::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRMeshCollada::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		assert(0);
		return false;
	}
	switch(channelId)
	{
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,0,2,itemIndex,itemData,itemSize);

		default:
			// unsupported channel
			return RRMesh::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRMesh

unsigned RRMeshCollada::getNumVertices() const
{
	return RRMeshCollada::getNumTriangles()*3;
}

void swapAxes(RRVec3& a)
{
#ifdef SWAP_YZ
	RRReal tmp = a[1];
	a[1] = a[2];
	a[2] = -tmp;
#endif
}

void RRMeshCollada::getVertex(unsigned v, Vertex& out) const
{
	assert(v<RRMeshCollada::getNumVertices());
	RRVec3 tmp[3];
	if(getTriangleVerticesData(mesh,FUDaeGeometryInput::POSITION,0,3,v/3,tmp,sizeof(tmp)))
	{
		out = tmp[v%3]*SCALE;
		swapAxes(out);
	}
	else
	{
		LIMITED_TIMES(1,RRReporter::report(RRReporter::ERRO,"RRMeshCollada: No vertex positions.\n"));
	}
}

unsigned RRMeshCollada::getNumTriangles() const
{
	return (unsigned)mesh->GetFaceCount();
}

void RRMeshCollada::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=RRMeshCollada::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out[0] = t*3;
	out[1] = t*3+1;
	out[2] = t*3+2;
}

void RRMeshCollada::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	// if collada contains no normals,
	if(!getTriangleVerticesData(mesh,FUDaeGeometryInput::NORMAL,0,3,t,&out,sizeof(out)))
	{
		// fallback to autogenerated ones
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRObjectCollada: No normals, fallback to automatic.\n"));
		RRMesh::getTriangleNormals(t,out);
	}
	else
	{
		for(unsigned i=0;i<3;i++)
			swapAxes(out.norm[i]);
	}
}

void RRMeshCollada::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	// if collada contains no unwrap,
	if(!getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,1,2,t,&out,sizeof(out)))
	{
		// fallback to autogenerated one
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRObjectCollada: No unwrap in TEXCOORD 1, fallback to automatic.\n"));
		RRMesh::getTriangleMapping(t,out);
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectCollada

// See RRObject documentation for details
// on individual member functions.

class RRObjectCollada : public RRObject
{
public:
	RRObjectCollada(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const RRCollider* acollider);
	RRObjectIllumination*      getIllumination();
	virtual ~RRObjectCollada();

	// RRChanneledData
	virtual void               getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool               getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRObject
	virtual const RRCollider*  getCollider() const;
	virtual const RRMaterial*  getTriangleMaterial(unsigned t) const;
	virtual const RRMatrix3x4* getWorldMatrix();
	virtual const RRMatrix3x4* getInvWorldMatrix();

private:
	const FCDSceneNode*        node;
	const FCDGeometryInstance* geometryInstance;

	// copy of object's material properties
	struct MaterialInfo
	{
		RRMaterial             material;
		de::Texture*           diffuseTexture;
	};
	typedef std::map<const FCDEffectStandard*,MaterialInfo> Cache;
	Cache                      cache;
	void                       updateMaterials();
	const MaterialInfo*        getTriangleMaterialInfo(unsigned t) const;

	// collider for ray-mesh collisions
	const RRCollider*          collider;

	// copy of object's transformation matrices
	RRMatrix3x4                worldMatrix;
	RRMatrix3x4                invWorldMatrix;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination*      illumination;
};

RRObjectCollada::RRObjectCollada(const FCDSceneNode* anode, const FCDGeometryInstance* ageometryInstance, const RRCollider* acollider)
{
	node = anode;
	geometryInstance = ageometryInstance;
	collider = acollider;

	// create illumination
	illumination = new rr::RRObjectIllumination(collider->getMesh()->getNumVertices());

	// create transformation matrices
	const FMMatrix44 world = node->CalculateWorldTransform();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			worldMatrix.m[i][j] = world.m[j][i];
	const FMMatrix44 inv = world.Inverted();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			invWorldMatrix.m[i][j] = world.m[j][i];

	// create material cache
	updateMaterials();
}

void RRObjectCollada::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
			if(numItems) *numItems = getCollider()->getMesh()->getNumTriangles();
			if(itemSize) *itemSize = sizeof(de::Texture*);
			return;

		default:
			// unsupported channel
			RRObject::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRObjectCollada::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		assert(0);
		return false;
	}
	switch(channelId)
	{
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
			{
			if(itemIndex>=getCollider()->getMesh()->getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef de::Texture* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			const MaterialInfo* mi = getTriangleMaterialInfo(itemIndex);
			*out = mi ? mi->diffuseTexture : NULL;
			return true;
			}

		case rr_gl::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
			{
			if(itemIndex>=getCollider()->getMesh()->getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef RRObjectIllumination* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			*out = illumination;
			return true;
			}

		default:
			// unsupported channel
			return RRObject::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
}

const RRCollider* RRObjectCollada::getCollider() const
{
	return collider;
}

fstring getTriangleMaterialSymbol(const FCDGeometryMesh* mesh, unsigned triangle)
{
	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			size_t relativeIndex = triangle - polygons->GetFaceOffset();
			if(relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
			{
				return polygons->GetMaterialSemantic();
			}
		}
	}
	assert(0);
	return NULL;
}

RRColor colorToColor(FMVector4 color)
{
	return RRColor(color.x,color.y,color.z);
}

RRReal colorToFloat(FMVector4 color)
{
	return (color.x+color.y+color.z)*0.333f;
}

void RRObjectCollada::updateMaterials()
{
	if(!geometryInstance)
	{
		return;
	}

	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return;
	}

	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return;
	}

	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			const fstring symbol = polygons->GetMaterialSemantic();

			const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
			if(!materialInstance)
			{
				continue;
			}

			const FCDMaterial* material = materialInstance->GetMaterial();
			if(!material)
			{
				continue;
			}

			const FCDEffect* effect = material->GetEffect();
			if(!effect)
			{
				continue;
			}

			const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
			if(!effectProfile)
			{
				continue;
			}
			const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);

			// Write RRMaterials into cache.
			// Why?
			// 1) RRObject requires that we return always the same RRMaterial for the same triangle
			//    and that pointer stays valid for whole RRObject life.
			// 2) getTriangleMaterial() is const and thread safe,
			//    so cache can't be filled lazily, it must be filled in constructor.
			Cache::const_iterator i = cache.find(effectStandard);
			if(i==cache.end())
			{
				MaterialInfo mi;
				mi.material.reset(false);
				mi.material.diffuseReflectance = colorToColor(effectStandard->GetDiffuseColor());
				mi.material.diffuseEmittance = colorToColor(effectStandard->GetEmissionFactor() * effectStandard->GetEmissionColor());
				mi.material.specularReflectance = colorToFloat(effectStandard->GetSpecularFactor() * effectStandard->GetSpecularColor());
				mi.material.specularTransmittance = colorToFloat(effectStandard->GetTranslucencyFactor() * effectStandard->GetTranslucencyColor());
				mi.material.refractionIndex = effectStandard->GetIndexOfRefraction();
				mi.diffuseTexture = NULL;
				if(effectStandard->GetTextureCount(FUDaeTextureChannel::DIFFUSE))
				{
					const FCDTexture* diffuseTexture = effectStandard->GetTexture(FUDaeTextureChannel::DIFFUSE,0);
					if(diffuseTexture)
					{
						const FCDImage* diffuseImage = diffuseTexture->GetImage();
						if(diffuseImage)
						{
							const fstring& filename = diffuseImage->GetFilename();
							mi.diffuseTexture = de::Texture::load(&filename[0],NULL);
							if(mi.diffuseTexture)
							{
								// compute average diffuse texture color
								enum {size = 8};
								rr::RRColor avg = rr::RRColor(0);
								for(unsigned i=0;i<size;i++)
									for(unsigned j=0;j<size;j++)
									{
										float tmp[4];
										mi.diffuseTexture->getPixel(i/(float)size,j/(float)size,0,tmp);
										avg += rr::RRVec3(tmp[0],tmp[1],tmp[2]);
									}
								mi.material.diffuseReflectance = avg / (size*size);
							}
							else
							{
								RRReporter::report(RRReporter::ERRO,"RRObjectCollada: Image %s not found.\n",&filename[0]);
							}
						}
					}
				}
#ifdef VERIFY
				if(mi.material.validate())
					RRReporter::report(RRReporter::WARN,"RRObjectCollada: Material adjusted to physically valid.\n");
#endif
				cache[effectStandard] = mi;
			}
		}
	}
}

const RRObjectCollada::MaterialInfo* RRObjectCollada::getTriangleMaterialInfo(unsigned t) const
{
	// lets have some abstract layered fun

	if(!geometryInstance)
	{
		return NULL;
	}

	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return NULL;
	}

	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return NULL;
	}

	const fstring symbol = getTriangleMaterialSymbol(mesh,t);

	const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
	if(!materialInstance)
	{
		return NULL;
	}

	const FCDMaterial* material = materialInstance->GetMaterial();
	if(!material)
	{
		return NULL;
	}

	const FCDEffect* effect = material->GetEffect();
	if(!effect)
	{
		return NULL;
	}

	const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
	if(!effectProfile)
	{
		return NULL;
	}
	const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);

	Cache::const_iterator i = cache.find(effectStandard);
	if(i==cache.end())
	{
		assert(0);
		return NULL;
	}
	return &i->second;
}

const RRMaterial* RRObjectCollada::getTriangleMaterial(unsigned t) const
{
	return &getTriangleMaterialInfo(t)->material;
}

const RRMatrix3x4* RRObjectCollada::getWorldMatrix()
{
	return &worldMatrix;
}

const RRMatrix3x4* RRObjectCollada::getInvWorldMatrix()
{
	return &invWorldMatrix;
}

RRObjectIllumination* RRObjectCollada::getIllumination()
{
	return illumination;
}

RRObjectCollada::~RRObjectCollada()
{
	delete illumination;
	// don't delete collider and mesh, we haven't created them
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromFCollada

class ObjectsFromFCollada : public rr::RRObjects
{
public:
	ObjectsFromFCollada(FCDocument* document);
	virtual ~ObjectsFromFCollada();

private:
	RRCollider*                newColliderCached(const FCDGeometryMesh* mesh);
	RRObjectCollada*           newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance);
	void                       addNode(const FCDSceneNode* node);

	// collider and mesh cache, for instancing
	// every object is cached exactly once, so destruction is simple,
	//  no need for our data injected into document, no need for reference counting
	typedef std::map<const FCDGeometryMesh*,RRCollider*> Cache;
	Cache                      cache;
};

// Creates new RRMesh from FCDGeometryMesh.
// Always creates, no caching.
static RRMesh* newMesh(const FCDGeometryMesh* mesh)
{
	if(!mesh)
	{
		assert(0);
		return NULL;
	}
	return new RRMeshCollada(mesh);
}

// Creates new RRCollider from FCDGeometryMesh.
// Always creates, no caching.
static RRCollider* newCollider(const FCDGeometryMesh* amesh)
{
	RRMesh* mesh = newMesh(amesh);
	return RRCollider::create(mesh,RRCollider::IT_LINEAR);
}


// Creates new RRCollider from FCDGeometryMesh.
// Caching on, first query creates collider, second query reads it from cache.
RRCollider* ObjectsFromFCollada::newColliderCached(const FCDGeometryMesh* mesh)
{
	if(!mesh)
	{
		assert(0);
		return NULL;
	}
	Cache::iterator i = cache.find(mesh);
	if(i!=cache.end())
	{
		return (*i).second;
	}
	else
	{
		return cache[mesh] = newCollider(mesh);
	}
}

// Creates new RRObject from FCDEntityInstance.
// Always creates, no caching (only internal caching of colliders).
RRObjectCollada* ObjectsFromFCollada::newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance)
{
	if(!geometryInstance)
	{
		return NULL;
	}
	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return NULL;
	}
	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return NULL;
	}
	RRCollider* collider = newColliderCached(mesh);
	if(!collider)
	{
		return NULL;
	}
	return new RRObjectCollada(node,geometryInstance,collider);
}

// Adds all instances from node and his subnodes to 'objects'.
void ObjectsFromFCollada::addNode(const FCDSceneNode* node)
{
	// add instances from node
	for(size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if(entityInstance->GetEntityType()==FCDEntity::GEOMETRY)
		{
			const FCDGeometryInstance* geometryInstance = static_cast<const FCDGeometryInstance*>(entityInstance);
			RRObjectCollada* object = newObject(node,geometryInstance);
			if(object)
			{
				push_back(RRIlluminatedObject(object,object->getIllumination()));
			}
		}
	}
	// add children
	for(size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if(child)
		{
			addNode(child);
		}
	}
}

ObjectsFromFCollada::ObjectsFromFCollada(FCDocument* document)
{
	if(!document)
		return;

	/*/ normalize geometry
	bool swapYZ = false;
	RRReal scale = 1;
	FCDAsset* asset = document->GetAsset();
	if(asset)
	{
	scale = asset->GetUnitConversionFactor();
	FMVector3 up = asset->GetUpAxis();
	swapYZ = up==FMVector3(0,0,1);
	}*/

	// triangulate all polygons
	FCDGeometryLibrary* geometryLibrary = document->GetGeometryLibrary();
	for(size_t i=0;i<geometryLibrary->GetEntityCount();i++)
	{
		FCDGeometry* geometry = static_cast<FCDGeometry*>(geometryLibrary->GetEntity(i));
		FCDGeometryMesh* mesh = geometry->GetMesh();
		FCDGeometryPolygonsTools::Triangulate(mesh);
		FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh);
	}

	// import data
	const FCDSceneNode* root = document->GetVisualSceneRoot();
	addNode(root);
}

ObjectsFromFCollada::~ObjectsFromFCollada()
{
	// delete objects
	for(unsigned i=0;i<size();i++)
	{
		// no need to delete illumination separately, we created it as part of object
		//delete (*this)[i].illumination;
		delete (*this)[i].object;
	}
	// delete meshes and colliders (stored in cache)
	for(Cache::iterator i = cache.begin(); i!=cache.end(); i++)
	{
		RRCollider* collider = (*i).second;
		delete collider->getMesh();
		delete collider;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObjects* adaptObjectsFromFCollada(FCDocument* document)
{
	return new ObjectsFromFCollada(document);
}

#else

// stub - for quickly disabled collada support
#include "RRObjectCollada.h"
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document)
{
	return NULL;
}

#endif
