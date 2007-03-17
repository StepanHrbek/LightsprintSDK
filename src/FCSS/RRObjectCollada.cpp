// --------------------------------------------------------------------------
// Imports Collada scene (FCDocument) into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to Collada meshes, objects, materials.
// You can replace Collada with your internal format and adapt this code
// so it works with your data.
//
// Wrappers don't allocate additional memory, values are read from Collada document.
//
// Instancing is supported, multiple objects may share one collider and mesh.

#if 1

#include <cassert>
#include <cmath>
#include <map>
#include "RRObjectCollada.h"
#include "DemoEngine/Texture.h"
#include "RRGPUOpenGL/RendererOfRRObject.h"

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

#define SCALE 0.02f // scale vertices


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

#define VERIFY


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

bool getTriangleVerticesData(const FCDGeometryMesh* mesh, FUDaeGeometryInput::Semantic semantic, unsigned floatsPerVertex, unsigned itemIndex, void* itemData, unsigned itemSize)
{
	const FCDGeometrySource* source = mesh->FindSourceByType(semantic);
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
			assert(polygons->IsTriangles());
			size_t relativeIndex = itemIndex - polygons->GetFaceOffset();
			if(relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
			{
				const UInt32List* indices = polygons->FindIndices(source);
				if(indices)
				{
					size_t s = indices->size();
					for(unsigned j=0;j<3;j++)
					{
						assert(s>relativeIndex*3+j);
						((float*)itemData)[j*floatsPerVertex+0] = data[(*indices)[relativeIndex*3+j]*3  ];
						((float*)itemData)[j*floatsPerVertex+1] = data[(*indices)[relativeIndex*3+j]*3+1];
						((float*)itemData)[j*floatsPerVertex+2] = data[(*indices)[relativeIndex*3+j]*3+2];
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
		/*case rr_gl::CHANNEL_TRIANGLE_DIF_TEX:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(de::Texture*);
			return;*/
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(RRVec2[3]);
			return;
		case RRObject::CHANNEL_TRIANGLE_VERTICES_NORMAL:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(RRVec3[3]);
			return;
		case RRObject::CHANNEL_TRIANGLE_VERTICES_UNWRAP:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(RRVec2[3]);
			return;
		default:
			LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRMeshCollada: Unsupported channel %x requested.\n",channelId)); // legal, but could be error
			if(numItems) *numItems = 0;
			if(itemSize) *itemSize = 0;
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
		/*
		case rr_gl::CHANNEL_TRIANGLE_DIF_TEX:
		{
			if(itemIndex>=(unsigned)materials.size())
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
			*out = materials[itemIndex].texture;
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
		{
			if(itemIndex>=RRMeshCollada::getNumTriangles())
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
		*/

		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,2,itemIndex,itemData,itemSize);

		case RRObject::CHANNEL_TRIANGLE_VERTICES_NORMAL:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::NORMAL,3,itemIndex,itemData,itemSize);

		case RRObject::CHANNEL_TRIANGLE_VERTICES_UNWRAP:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::UV,2,itemIndex,itemData,itemSize);

		default:
			LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRMeshCollada: Unsupported channel %x requested.\n",channelId)); // legal, but could be error
			return false;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRMesh

unsigned RRMeshCollada::getNumVertices() const
{
	return RRMeshCollada::getNumTriangles()*3;
}

void RRMeshCollada::getVertex(unsigned v, Vertex& out) const
{
	assert(v<RRMeshCollada::getNumVertices());
	RRVec3 tmp[3];
	if(getTriangleVerticesData(mesh,FUDaeGeometryInput::POSITION,3,v/3,tmp,sizeof(tmp)))
	{
		out = tmp[v%3]*SCALE;
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


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectCollada

// See RRObject documentation for details
// on individual member functions.

class RRObjectCollada : public RRObject
{
public:
	RRObjectCollada(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const RRCollider* acollider, const char* pathToTextures);
	RRObjectIllumination* getIllumination();
	virtual ~RRObjectCollada();

	// RRObject
	virtual const RRCollider*       getCollider() const;
	virtual const RRMaterial*       getTriangleMaterial(unsigned t) const;
	virtual void                    getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual void                    getTriangleMapping(unsigned t, TriangleMapping& out) const;
	virtual const RRMatrix3x4*      getWorldMatrix();
	virtual const RRMatrix3x4*      getInvWorldMatrix();

private:
	const FCDSceneNode* node;
	const FCDGeometryInstance* geometryInstance;

	// copy of object's material properties
	struct MaterialInfo
	{
		RRMaterial material;
		de::Texture* diffuseTexture;
	};
	typedef std::map<const FCDEffectStandard*,MaterialInfo> Cache;
	Cache cache;
	void updateMaterials();

	// collider for ray-mesh collisions
	const RRCollider* collider;

	// copy of object's transformation matrices
	RRMatrix3x4 worldMatrix;
	RRMatrix3x4 invWorldMatrix;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};

RRObjectCollada::RRObjectCollada(const FCDSceneNode* anode, const FCDGeometryInstance* ageometryInstance, const RRCollider* acollider, const char* pathToTextures)
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
			worldMatrix.m[i][j] = world.m[i][j];
	const FMMatrix44 inv = world.Inverted();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			invWorldMatrix.m[i][j] = world.m[i][j];

	// create material cache
	updateMaterials();
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
#ifdef VERIFY
				if(mi.material.validate())
					RRReporter::report(RRReporter::WARN,"RRObjectCollada: Material adjusted to physically valid.\n");
#endif
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
							if(!mi.diffuseTexture)
								RRReporter::report(RRReporter::ERRO,"RRObjectCollada: Image %s not found.\n",&filename[0]);
						}
					}
				}
				cache[effectStandard] = mi;
			}
		}
	}
}

const RRMaterial* RRObjectCollada::getTriangleMaterial(unsigned t) const
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
	return &i->second.material;
}

void RRObjectCollada::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	// if collada contains no normals,
	if(!collider->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_NORMAL,t,&out,sizeof(out)))
	{
		// fallback to autogenerated ones
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRObjectCollada: No normals, fallback to automatic.\n"));
		RRObject::getTriangleNormals(t,out);
	}
}

void RRObjectCollada::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	// if collada contains no unwrap,
	if(!collider->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_UNWRAP,t,&out,sizeof(out)))
	{
		// fallback to autogenerated one
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRObjectCollada: No unwrap, fallback to automatic.\n"));
		RRObject::getTriangleMapping(t,out);
	}
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
// ColladaToRealtimeRadiosityImpl

class ColladaToRealtimeRadiosityImpl
{
public:
	//! Imports FCollada scene contents to RRRealtimeRadiosity solver.
	ColladaToRealtimeRadiosityImpl(FCDocument* document,const char* pathToTextures,RRRealtimeRadiosity* solver,const RRScene::SmoothingParameters* smoothing);

	//! Removes FCollada scene contents from RRRealtimeRadiosity solver.
	~ColladaToRealtimeRadiosityImpl();

private:
	RRCollider*                newColliderCached(const FCDGeometryMesh* mesh);
	class RRObjectCollada*     newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const char* pathToTextures);
	void                       addNode(RRRealtimeRadiosity::Objects& objects, const FCDSceneNode* node, const char* pathToTextures);

	// solver filled by our objects, colliders and meshes
	RRRealtimeRadiosity*       solver;

	// collider and mesh cache, for instancing
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
RRCollider* ColladaToRealtimeRadiosityImpl::newColliderCached(const FCDGeometryMesh* mesh)
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
RRObjectCollada* ColladaToRealtimeRadiosityImpl::newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const char* pathToTextures)
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
	return new RRObjectCollada(node,geometryInstance,collider,pathToTextures);
}

// Adds all instances from node and his subnodes to 'objects'.
void ColladaToRealtimeRadiosityImpl::addNode(RRRealtimeRadiosity::Objects& objects, const FCDSceneNode* node, const char* pathToTextures)
{
	// add instances from node
	for(size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if(entityInstance->GetEntityType()==FCDEntity::GEOMETRY)
		{
			const FCDGeometryInstance* geometryInstance = static_cast<const FCDGeometryInstance*>(entityInstance);
			RRObjectCollada* object = newObject(node,geometryInstance,pathToTextures);
			if(object)
			{
				objects.push_back(RRRealtimeRadiosity::Object(object,object->getIllumination()));
			}
		}
	}
	// add children
	for(size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if(child)
		{
			addNode(objects,child,pathToTextures);
		}
	}
}

// Create meshes, colliders, objects and insert them into solver.
ColladaToRealtimeRadiosityImpl::ColladaToRealtimeRadiosityImpl(FCDocument* document,const char* pathToTextures,RRRealtimeRadiosity* asolver,const RRScene::SmoothingParameters* smoothing)
{
	solver = asolver;

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
	if(document && solver)
	{
		RRRealtimeRadiosity::Objects objects;
		const FCDSceneNode* root = document->GetVisualSceneRoot();
		addNode(objects,root,pathToTextures);
		solver->setObjects(objects,smoothing);
	}
}

ColladaToRealtimeRadiosityImpl::~ColladaToRealtimeRadiosityImpl()
{
	// delete objects (stored in solver)
	if(solver)
	{
		for(unsigned i=0;i<solver->getNumObjects();i++)
		{
			// no need to delete illumination separately, we created it as part of object
			//delete app->getIllumination(i);
			delete solver->getObject(i);
		}
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
// ColladaToRealtimeRadiosity

ColladaToRealtimeRadiosity::ColladaToRealtimeRadiosity(FCDocument* document,const char* pathToTextures,RRRealtimeRadiosity* solver,const RRScene::SmoothingParameters* smoothing)
{
	impl = new ColladaToRealtimeRadiosityImpl(document,pathToTextures,solver,smoothing);
}

ColladaToRealtimeRadiosity::~ColladaToRealtimeRadiosity()
{
	delete impl;
}

#endif
