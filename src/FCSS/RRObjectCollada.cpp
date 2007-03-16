// --------------------------------------------------------------------------
// Imports Collada scene (FCDocument) into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to Collada meshes, objects, materials.
// You can replace Collada with your internal format and adapt this code
// so it works with your data.
//
// RRChanneledData - the biggest part of this implementation, provides access to
// custom data via our custom identifiers CHANNEL_MATERIAL_DIF_TEX etc.
// It is used only by our renderer RendererOfRRObject
// (during render of scene with ambient maps),
// it is never accessed by radiosity solver.
// You may skip it in your implementation.

#if 0

#include <cassert>
#include <cmath>
#include <vector>
#include "RRObjectCollada.h"
#include "DemoEngine/Texture.h"
#include "RRGPUOpenGL/RendererOfRRObject.h"

#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectProfile.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDSceneNode.h"
#include "FUtils/FUFileManager.h"

#ifdef _DEBUG
	#pragma comment(lib,"FColladaSD.lib")
#else
	#pragma comment(lib,"FColladaSR.lib")
#endif

using namespace rr;


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
// RRObjectCollada implements RRChanneledData

bool getTriangleVerticesData(const FCDGeometryMesh* mesh, FUDaeGeometryInput::Semantic semantic, unsigned floatsPerVertex, unsigned itemIndex, void* itemData, unsigned itemSize)
{
	const FCDGeometrySource* source = mesh->FindSourceByType(semantic);
	if(!source)
	{
		assert(0);
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
/*		case rr_gl::CHANNEL_MATERIAL_DIF_TEX:
			if(numItems) *numItems = (unsigned)materials.size();
			if(itemSize) *itemSize = sizeof(de::Texture*);
			return;
		case RRObject::CHANNEL_TRIANGLE_MATERIAL_IDX:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(unsigned);
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
			assert(0); // legal, but shouldn't happen in well coded program
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
		case rr_gl::CHANNEL_MATERIAL_DIF_TEX:
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
		case CHANNEL_TRIANGLE_MATERIAL_IDX:
		{
			...
			return true;
		}*/

		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,2,itemIndex,itemData,itemSize);

		case RRObject::CHANNEL_TRIANGLE_VERTICES_NORMAL:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::NORMAL,3,itemIndex,itemData,itemSize);

		case RRObject::CHANNEL_TRIANGLE_VERTICES_UNWRAP:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::UV,2,itemIndex,itemData,itemSize);

		default:
			assert(0); // legal, but shouldn't happen in well coded program
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
		out = tmp[v%3];
	}
	else
	{
		assert(0);
	}
	/*
	FCDGeometrySource* positionSource = mesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	assert(positionSource);
	polygons->FindIndices();
	out[0] = positionSource->GetValue(v)[0];
	out[1] = positionSource->GetValue(v)[1];
	out[2] = positionSource->GetValue(v)[2];
	*/
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
	virtual unsigned                getTriangleMaterial(unsigned t) const;
	virtual const RRMaterial*        getMaterial(unsigned s) const;
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
		de::Texture* texture;
	};
	std::vector<MaterialInfo> materials;

	// collider for ray-mesh collisions
	const RRCollider* collider;

	RRMatrix3x4 worldMatrix;
	RRMatrix3x4 invWorldMatrix;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};

/*
// Inputs: m
// Outputs: t, s
static void fillMaterial(RRMaterial& s, de::Texture*& t, de::TTexture* m,const char* pathToTextures, de::Texture* fallback)
{
	enum {size = 8};

	// load texture
	char* exts[3]={".jpg",".png",".tga"};
	for(unsigned e=0;e<3;e++)
	{
		char buf[300];
		_snprintf(buf,299,"%s%s%s",pathToTextures,m->mName,exts[e]);
		buf[299]=0;
		t = de::Texture::load(buf,NULL,true,false);
		//if(t) {puts(buf);break;}
		if(t) break;
		//if(e==2) printf("Not found: %s\n",buf);
	}
	if(!t) t = fallback;

	// for diffuse textures provided by bsp,
	// it is sufficient to compute average texture color
	RRColor avg = RRColor(0);
	if(t)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				RRColor tmp;
				t->getPixel(i/(float)size,j/(float)size,&tmp[0]);
				avg += tmp;
			}
		avg /= size*size/2; // 2 stands for quake map boost
	}

	// set all properties to default
	s.reset(0);

	// set diffuse reflectance according to bsp material
	s.diffuseReflectance = avg;

#ifdef VERIFY
	if(s.validate())
		RRReporter::report(RRReporter::WARN,"Material adjusted to physically valid.\n");
#else
	s.validate();
#endif
}
*/

RRObjectCollada::RRObjectCollada(const FCDSceneNode* anode, const FCDGeometryInstance* ageometryInstance, const RRCollider* acollider, const char* pathToTextures)
{
	node = anode;
	geometryInstance = ageometryInstance;
	collider = acollider;

	const FMMatrix44 world = node->CalculateWorldTransform();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			worldMatrix.m[i][j] = world.m[i][j];
	const FMMatrix44 inv = world.Inverted();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			invWorldMatrix.m[i][j] = world.m[i][j];

	//!!!

	/*for(unsigned i=0;i<(unsigned)model->mTextures.size();i++)
	{
		MaterialInfo si;
		fillMaterial(si.material,si.texture,&model->mTextures[i],pathToTextures);
		materials.push_back(si);
	}*/
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
	return false;
}

unsigned RRObjectCollada::getTriangleMaterial(unsigned t) const
{
	// lets have some abstract fun

	if(!geometryInstance)
	{
		return UINT_MAX;
	}

	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return UINT_MAX;
	}

	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return UINT_MAX;
	}

	const fstring symbol = getTriangleMaterialSymbol(mesh,t);

	const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
	if(!materialInstance)
	{
		return UINT_MAX;
	}

	const FCDMaterial* material = static_cast<const FCDMaterial*>(materialInstance->GetEntity());
	if(!material)
	{
		return UINT_MAX;
	}

	const FCDEffect* effect = material->GetEffect();
	if(!effect)
	{
		return UINT_MAX;
	}

	const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
	if(!effectProfile)
	{
		return UINT_MAX;
	}

	const FCDEffectParameter* effectParameter = effectProfile->FindParameterByReference("diffuse");
/*
	unsigned result = UINT_MAX;
	collider->getMesh()->getChannelData(CHANNEL_TRIANGLE_MATERIAL_IDX,t,&result,sizeof(unsigned));
	return result;
	/*
	if(t>=collider->getMesh()->getNumTriangles())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<materials.size());
	return s;
	*/
}

const RRMaterial* RRObjectCollada::getMaterial(unsigned s) const
{
	if(s>=materials.size()) 
	{
		assert(0);
		return NULL;
	}
	return &materials[s].material;
}

void RRObjectCollada::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	// if collada contains no normals,
	if(!collider->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_NORMAL,t,&out,sizeof(out)))
		// fallback to autogenerated ones
		RRObject::getTriangleNormals(t,out);
}

void RRObjectCollada::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	// if collada contains no unwrap,
	if(!collider->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_UNWRAP,t,&out,sizeof(out)))
		// fallback to autogenerated one
		RRObject::getTriangleMapping(t,out);
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
	delete collider->getMesh();
	delete collider;
}


//////////////////////////////////////////////////////////////////////////////
//
// ColladaToRealtimeRadiosity

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
RRCollider* ColladaToRealtimeRadiosity::newColliderCached(const FCDGeometryMesh* mesh)
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
RRObjectCollada* ColladaToRealtimeRadiosity::newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const char* pathToTextures)
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
void ColladaToRealtimeRadiosity::addNode(const FCDSceneNode* node, const char* pathToTextures)
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
			addNode(child,pathToTextures);
		}
	}
}

// Create meshes, colliders, objects and insert them into solver.
ColladaToRealtimeRadiosity::ColladaToRealtimeRadiosity(FCDocument* adocument,const char* pathToTextures,RRRealtimeRadiosity* asolver,const RRScene::SmoothingParameters* smoothing)
{
	document = adocument;
	solver = asolver;

	// triangulate all polygons
	FCDGeometryLibrary* geometryLibrary = adocument->GetGeometryLibrary();
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
		const FCDSceneNode* root = document->GetVisualSceneRoot();
		addNode(root,pathToTextures);
		solver->setObjects(objects,smoothing);
	}
}

ColladaToRealtimeRadiosity::~ColladaToRealtimeRadiosity()
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

#endif
