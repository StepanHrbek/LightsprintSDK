// --------------------------------------------------------------------------
// Lightsprint adapter for OpenCollada scenes.
// Copyright (C) 2010 Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_OPENCOLLADA

//
// Blackmoore! Tonight you sleep in hell!
//

#include <vector>
#include <stack>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <string>

#include "COLLADAFWPrerequisites.h"
#include "COLLADAFWIWriter.h"
#include "COLLADAFWRoot.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWScene.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWVisualScene.h"
#include "COLLADAFWLibraryNodes.h"
#include "COLLADAFWFileInfo.h"
#include "COLLADAFWMaterialBinding.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWEffectCommon.h"
#include "COLLADAFWLight.h"
#include "COLLADAFWTexture.h"
#include "COLLADASaxFWLIExtraDataCallbackHandler.h"
#include "COLLADASaxFWLLoader.h"

#include "COLLADASaxFWLColladaParserAutoGen14Attributes.h"

#include "RRObjectOpenCollada.h"

#include "Lightsprint/RRScene.h"

#include <boost/filesystem.hpp> // system_complete

#pragma warning(disable:4267) // there are too many size_t -> unsigned conversions

using namespace rr;

//////////////////////////////////////////////////////////////////////////////
//
// Extra tags Loaders

void trimString(std::string& str)
{
	size_t startpos = str.find_first_not_of(" \t\n\r");
	if( std::string::npos != startpos )
		str = str.substr( startpos );

	size_t endpos = str.find_last_not_of(" \t\n\r");
	if( std::string::npos != endpos )
		str = str.substr( 0, endpos+1 );
}

bool convertStringToFloat(const char* text, unsigned int length, float& retVal)
{
	std::string str;
	str.assign(text,length);
	trimString(str);

	std::istringstream i(str);

	float convVal;

	if (!(i >> convVal))
		return false;
	else
	{
		retVal = convVal;
		return true;
	}
}

class ExtraData
{
protected:
	float* currValue;
	float* defferedValue;
	bool   error;
public:
	virtual void elementBegin(const char* name, const char* currentProfile)=0;

	ExtraData()
	{
		currValue = NULL;
		defferedValue = NULL;
	}

	void elementData(const char* text, unsigned int length)
	{
		if(currValue != NULL)
			error = !convertStringToFloat(text,length,*currValue);
	}

	void elementEnd(const char* name)
	{
		if(currValue != NULL && error)
		{
			RRReporter::report(WARN,"Extra '%s' was not of a convertable float value.\n",name);
		}

		currValue = NULL;
	}
};

class ExtraDataLight : public ExtraData
{
public:
	float intensity;             // fcollada

	ExtraDataLight()
	{
		intensity = 1.f;
	}

	virtual void elementBegin(const char* name, const char* currentProfile)
	{
		currValue = NULL;

		if(strcmp(name,"intensity")==0)
			currValue = &intensity;
	}
};

class ExtraDataGeometry : public ExtraData
{
public:
	float double_sided;          // shared

	ExtraDataGeometry()
	{
		double_sided = 0.f;
	}

	virtual void elementBegin(const char* name, const char* currentProfile)
	{
		currValue = NULL;

		// shared
		if(strcmp(name,"double_sided")==0)
			currValue = &double_sided;
	}

};

class ExtraDataEffect : public ExtraData
{
public:
	// from effect
	float double_sided;              // shared

	// from technique
	float spec_level;                // fcollada
	float emission_level;            // fcollada

	ExtraDataEffect()
	{
		double_sided = 0.f;
		spec_level = 1.f;
		emission_level = 1.f;
	}

	virtual void elementBegin(const char* name, const char* currentProfile)
	{
		currValue = NULL;

		// shared
		if(strcmp(name,"double_sided")==0)
			currValue = &double_sided;
		else if(strcmp(name,"float")==0)
		{
			if(defferedValue != NULL)
				currValue = defferedValue;
			defferedValue = NULL;
		}
		// fcollada
		else if(strcmp(currentProfile,"FCOLLADA")==0)
		{
			if(strcmp(name,"spec_level")==0)
				defferedValue = &spec_level;
			else if(strcmp(name,"emission_level")==0)
				defferedValue = &emission_level;
		}
	}
};

typedef std::map<COLLADAFW::UniqueId, ExtraData*> MapUniqueExtra;

class ExtraDataCallbackHandler : public COLLADASaxFWL::IExtraDataCallbackHandler
{
	MapUniqueExtra mapExtra;
public:
	const char* currProfile;
	ExtraData* currExtraData;

	ExtraDataCallbackHandler() { }

	virtual ~ExtraDataCallbackHandler()
	{
		// cleanup
		for(MapUniqueExtra::iterator iter = mapExtra.begin(); iter != mapExtra.end(); iter++)
			delete iter->second;
	}

	ExtraData* getData(const COLLADAFW::UniqueId& id, bool supressAssert = false)
	{
		MapUniqueExtra::iterator extraIter = mapExtra.find( id );

		if(extraIter != mapExtra.end() )
		{
			return extraIter->second;
		}
		else
		{
			if(!supressAssert)
			{
				RR_ASSERT(false);
			}

			return NULL;
		}
	}

	template<class T>
	ExtraData* getOrInsertDefaultData(const COLLADAFW::UniqueId& id)
	{
		ExtraData* extraData = getData( id, true );

		if(extraData == NULL)
		{
			ExtraData* defaultData = new T();
			mapExtra.insert( std::make_pair( id, defaultData ) );
			return defaultData;
		}
		else
			return extraData;
	}

	virtual bool parseElement (
		const GeneratedSaxParser::ParserChar* profileName,
		const GeneratedSaxParser::StringHash& elementHash,
		const COLLADAFW::UniqueId& uniqueId )
	{
		// parse for extra elements that we have data structures for
		currExtraData = NULL;
		currProfile = profileName;

		// v1.4 and v1.5 hashes for the following elements are the same
		switch( elementHash )
		{
			case COLLADASaxFWL14::HASH_ELEMENT_LIGHT:
				if(strcmp(profileName,"FCOLLADA") == 0)
					currExtraData = getOrInsertDefaultData<ExtraDataLight>( uniqueId );
				break;
			case COLLADASaxFWL14::HASH_ELEMENT_EFFECT:
				currExtraData = getOrInsertDefaultData<ExtraDataEffect>( uniqueId );
				break;
			case COLLADASaxFWL14::HASH_ELEMENT_PROFILE_COMMON:
				currExtraData = getOrInsertDefaultData<ExtraDataEffect>( uniqueId );
				break;
			case COLLADASaxFWL14::HASH_ELEMENT_TECHNIQUE:
				currExtraData = getOrInsertDefaultData<ExtraDataEffect>( uniqueId );
				break;
			case COLLADASaxFWL14::HASH_ELEMENT_GEOMETRY:
				currExtraData = getOrInsertDefaultData<ExtraDataGeometry>( uniqueId );
				break;
		}

		return (currExtraData != NULL);
	}

	virtual bool elementBegin( const GeneratedSaxParser::ParserChar* elementName, const GeneratedSaxParser::xmlChar** attributes)
	{
		currExtraData->elementBegin(elementName, currProfile);
		return true;
	}

	virtual bool elementEnd(const GeneratedSaxParser::ParserChar* elementName )
	{
		currExtraData->elementEnd(elementName);
		return true;
	}

	virtual bool textData(const GeneratedSaxParser::ParserChar* text, size_t textLength)
	{
		currExtraData->elementData(text,textLength);
		return true;
	}

private:

	/** Disable default copy ctor. */
	ExtraDataCallbackHandler( const ExtraDataCallbackHandler& pre ) { }

	/** Disable default assignment operator. */
	const ExtraDataCallbackHandler& operator= ( const ExtraDataCallbackHandler& pre ) { }

};

//////////////////////////////////////////////////////////////////////////////
//
// RRLightsOpenCollada

class RRLightsOpenCollada : public RRLights
{
public:
	~RRLightsOpenCollada()
	{
		for (unsigned i=0;i<size();i++)
		{
			delete (*this)[i];
		}
	}
};

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectOpenCollada

class RRObjectOpenCollada : public RRObject
{
public:
	COLLADAFW::UniqueId instanceId;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsOpenCollada

class RRObjectsOpenCollada : public RRObjects
{
public:
	RRMaterial defaultMaterial;

	unsigned int numMaterials;
	RRMaterial** materials;

	RRMeshArrays* meshes;
	unsigned int nextMesh;
	unsigned int numMeshes;

	const RRCollider** colliders;

	RRObjectsOpenCollada():RRObjects()
	{
		meshes = NULL;
		materials = NULL;
		colliders = NULL;
		numMaterials = 0;
		numMeshes = 0;
		nextMesh = 0;
	}

	~RRObjectsOpenCollada()
	{
		for (unsigned i=0;i<size();i++)
		{
			delete (*this)[i];
		}

		for (unsigned i=0;i<numMeshes;i++)
		{
			if(colliders[i] != NULL)
				delete colliders[i];
		}

		if(colliders != NULL)
			delete [] colliders;

		if(meshes != NULL)
			delete [] meshes;

		for (unsigned i=0;i<numMaterials;i++)
		{
			if(materials[i] != NULL)
				delete materials[i];
		}

		if(materials != NULL)
			free(materials);
	}
};

class RRWriterOpenCollada;

//////////////////////////////////////////////////////////////////////////////
//
// RRSceneOpenCollada

class RRSceneOpenCollada : public RRScene
{
	friend class RRWriterOpenCollada;
public:
	static RRScene* load(const char* filename, bool* aborting, float emissiveMultiplier);
	static bool save141(const RRScene* scene, const char* filename);
	static bool save150(const RRScene* scene, const char* filename);

	virtual ~RRSceneOpenCollada() { }
};

///////////////              Importers              //////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// RRWriterOpenCollada

struct NodeInfo
{
	NodeInfo( COLLADABU::Math::Matrix4 _worldTransformation)
		: worldTransformation(_worldTransformation){}
	COLLADABU::Math::Matrix4 worldTransformation;
};

typedef std::vector<RRObjectOpenCollada*> VectorRRObjectOpenCollada;
typedef std::map<COLLADAFW::MaterialId, COLLADAFW::UniqueId> MapMaterialIdToUnique;
typedef std::map<COLLADAFW::MaterialId, int> MapMaterialIdToLocal;
typedef std::map<COLLADAFW::UniqueId, COLLADAFW::MaterialId> MapUniqueToMaterialId;

typedef std::map<unsigned int,std::string> MapUIntString;

// this is harsh, evaluate me!
typedef std::map<size_t,size_t> MapLocalToPhysicalUVSet;
typedef std::map<COLLADAFW::MaterialId, MapLocalToPhysicalUVSet> MapMaterialIdToUVSet;

struct MeshPlaceholder
{
	MeshPlaceholder()
	{
		instanced = false;
	}

	~MeshPlaceholder()
	{
		for(VectorRRObjectOpenCollada::iterator iter = objectsWithMesh.begin(); iter != objectsWithMesh.end(); iter++)
		{
			delete (*iter);
		}
	}

	VectorRRObjectOpenCollada  objectsWithMesh;
	MapUIntString              localMaterials;
	MapMaterialIdToUVSet       mapUV;
	std::string                meshName;
	bool                       instanced;
	COLLADAFW::UniqueId        uniqueId;
	size_t                     lastUVSet;
	size_t                     numPrimitives;
	size_t                     numTriangles;
	size_t                     numIndices;
};

struct FaceGroupPlaceholder
{
	COLLADAFW::MaterialId              materialId;
	std::string                        materialString;
	unsigned int                       triangleCount;
};

struct MaterialBindingPlaceholder
{
	MapMaterialIdToUnique              mapBinding;
	MapMaterialIdToLocal               mapLocal;

	COLLADAFW::MaterialBindingArray*   boundBindingArray;
	MapUniqueToMaterialId              unboundBindingArray;

	MeshPlaceholder*                   sourceMesh;
	std::string                        instanceName;
	COLLADAFW::UniqueId                uniqueId;

	MaterialBindingPlaceholder()
	{
		boundBindingArray = NULL;
	}
};

typedef const unsigned int cuint;

struct CachedMaterial
{
	unsigned int drC;
	unsigned int deC;
	unsigned int srC;
	unsigned int stC;
	unsigned int lmC;

	unsigned int localId;

	CachedMaterial(cuint& dr, cuint& de, cuint& sr, cuint& st, cuint& lm, cuint locid)
	{
		drC = dr;
		deC = de;
		srC = sr;
		stC = st;
		lmC = lm;

		localId = locid;
	}
};

struct IndexInfo
{
	IndexInfo()
	{
		insert = false;
	}

	unsigned int remap;
	bool insert;
};

typedef std::stack<NodeInfo>                                      StackNodeInfo;

typedef std::map<COLLADAFW::UniqueId, COLLADAFW::Material>        MapUniqueMaterial;
typedef std::map<COLLADAFW::UniqueId, COLLADAFW::Effect>          MapUniqueEffect;
typedef std::map<COLLADAFW::UniqueId, COLLADAFW::Image>           MapUniqueImage;
typedef std::map<COLLADAFW::UniqueId, COLLADAFW::Node>            MapUniqueNode;
typedef std::map<COLLADAFW::UniqueId, COLLADAFW::Light>           MapUniqueLight;
typedef std::map<COLLADAFW::UniqueId, MeshPlaceholder>            MapUniqueMesh;

typedef std::map<COLLADAFW::UniqueId, MaterialBindingPlaceholder> MapUniqueMaterialBinding;

typedef std::vector<COLLADAFW::VisualScene>                       VectorVisualScene;
typedef std::vector<COLLADAFW::LibraryNodes>                      VectorLibraryNodes;

typedef std::vector<FaceGroupPlaceholder>                         VectorFaceGroupPlaceholder;

typedef std::multimap<COLLADAFW::UniqueId, CachedMaterial>        MapUniqueCached;
typedef std::map<std::string, COLLADAFW::UniqueId>                MapStringUnique;

enum PARSE_RUN_TYPE
{
	RUN_COPY_ELEMENTS,
	RUN_GEOMETRY
};

// FIXME does this work everytime? chmm ...
inline std::string GetPathFromFilename( const std::string& filename )
{
	return filename.substr( 0, filename.find_last_of("/\\")+1 );
}

class RRWriterOpenCollada : public COLLADAFW::IWriter
{
	PARSE_RUN_TYPE                  parseStep;

	const char*                     filename;
	std::string                     filepath;

	RRObjectsOpenCollada*           objects;
	RRLightsOpenCollada*            lights;

	COLLADAFW::InstanceVisualScene* instanceVisualScene;

	StackNodeInfo                   nodeInfoStack;

	COLLADAFW::FileInfo::UpAxisType upAxis;

	double rescaleUnit;

	VectorVisualScene               visualSceneArray;
	VectorLibraryNodes              libraryNodesArray;

	MapUniqueMaterial               materialMap;
	MapUniqueEffect                 effectMap;
	MapUniqueImage                  imageMap;
	MapUniqueNode                   nodeMap;
	MapUniqueMesh                   meshMap;
	MapUniqueLight                  lightMap;

	MapStringUnique					materialStringMap;

	MapUniqueCached                 cachedMap;

	MapUniqueMaterialBinding        materialBindingMap;

	ExtraDataCallbackHandler        extraHandler;
public:
	RRWriterOpenCollada(RRObjectsOpenCollada* _objects, RRLightsOpenCollada* _lights, const char* _filename)
	{
		parseStep = RUN_COPY_ELEMENTS;
		objects = _objects;
		lights = _lights;
		filename = _filename;
		filepath = GetPathFromFilename(filename);
		instanceVisualScene = NULL;
		rescaleUnit = 1;
	}

	~RRWriterOpenCollada()
	{
		if(instanceVisualScene != NULL)
			delete instanceVisualScene;
	}

	bool parseDocument()
	{
		COLLADASaxFWL::Loader loader;
		COLLADAFW::Root root(&loader, this);

		loader.registerExtraDataCallbackHandler ( &extraHandler );

//		if ( !root.loadDocument(filename) )
		if ( !root.loadDocument(boost::filesystem::system_complete(filename).file_string()) ) // workaround for http://code.google.com/p/opencollada/issues/detail?id=122
			return false;

		return true;
	}

	void handleNodes( const COLLADAFW::NodePointerArray& nodes)
	{
		for ( size_t i = 0, count = nodes.getCount(); i < count; ++i)
		{
			handleNode(nodes[i]);
		}
	}

	RRMatrix3x4 convertMatrix(const COLLADABU::Math::Matrix4& matrix)
	{
		RRMatrix3x4 wm;
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				wm.m[i][j] = (rr::RRReal)matrix.getElement(i, j);
		return wm;
	}

	void handleInstanceLights( const COLLADAFW::Node* node, const COLLADABU::Math::Matrix4& matrix )
	{
		const COLLADAFW::InstanceLightPointerArray& instanceLights = node->getInstanceLights();

		for ( size_t i = 0, count = instanceLights.getCount(); i < count; ++i)
		{
			COLLADAFW::InstanceLight* instanceLight = instanceLights[i];
			COLLADAFW::UniqueId lightId = instanceLight->getInstanciatedObjectId();

			// find source light
			MapUniqueLight::iterator iter = lightMap.find(lightId);

			if(iter == lightMap.end())
			{
				RRReporter::report(WARN,"Light instance without parent light.\n");
				continue;
			}

			COLLADAFW::Light& light = iter->second;

			rr::RRMatrix3x4 worldMatrix = convertMatrix(matrix);
			RRVec3 position = worldMatrix.transformedPosition(RRVec3(0));
			RRVec3 direction = worldMatrix.transformedDirection(RRVec3(0,0,-1));

			ExtraDataLight* extraLight = (ExtraDataLight*)extraHandler.getData( light.getUniqueId() );

			// create RRLight
			RRVec3 color = RRVec3((rr::RRReal)light.getColor().getRed(),(rr::RRReal)light.getColor().getGreen(),(rr::RRReal)light.getColor().getBlue()) * extraLight->intensity;
			RRVec4 polynom((rr::RRReal)light.getConstantAttenuation(),(rr::RRReal)(light.getLinearAttenuation()/rescaleUnit),(rr::RRReal)(light.getQuadraticAttenuation()/rescaleUnit/rescaleUnit),(rr::RRReal)0.0001f);

			RRLight* rrLight = NULL;

			switch( light.getLightType() )
			{
			case COLLADAFW::Light::POINT_LIGHT:
				{
					rrLight = RRLight::createPointLightPoly(position,color,polynom);
				}
				break;
			case COLLADAFW::Light::SPOT_LIGHT:
				{
					// FIXME opencollada does not have any outerangle?
					rrLight = RRLight::createSpotLightPoly(position,color,polynom,direction,(rr::RRReal)light.getFallOffAngle(),(rr::RRReal)light.getFallOffAngle(),(rr::RRReal)light.getFallOffExponent());
				}
				break;
			case COLLADAFW::Light::DIRECTIONAL_LIGHT:
				{
					rrLight = RRLight::createDirectionalLight(direction,color,false);
				}
				break;
			}

			if(rrLight != NULL)
			{
				std::string name = instanceLight->getName().c_str();
				if(name  == "")
					name  = node->getName();
				rrLight->name = name.c_str();

				lights->push_back(rrLight);
			}
		}
	}

	void handleInstanceGeometries( const COLLADAFW::Node* node, const COLLADABU::Math::Matrix4& matrix )
	{
		const COLLADAFW::InstanceGeometryPointerArray& instanceGeometries = node->getInstanceGeometries();

		for ( size_t i = 0, count = instanceGeometries.getCount(); i < count; ++i)
		{
			COLLADAFW::InstanceGeometry* instanceGeometry = instanceGeometries[i];
			COLLADAFW::UniqueId meshId = instanceGeometry->getInstanciatedObjectId();

			// find source geometry
			MapUniqueMesh::iterator iter = meshMap.find(meshId);

			if(iter == meshMap.end())
			{
				RRReporter::report(WARN,"Geometry instance without parent geometry.\n");
				continue;
			}

			MeshPlaceholder &ph = iter->second;

			std::string name = instanceGeometry->getName();
			if(name == "")
			{
				// NOTE change geometry instance name here to avoid copying names from parent nodes
				name = node->getName();
			}

			// add RRObject
			RRObjectOpenCollada* object = new RRObjectOpenCollada;
			object->instanceId = instanceGeometry->getUniqueId();

			// add bindings for this object if not already done
			MapUniqueMaterialBinding::iterator iterBinding = materialBindingMap.find( object->instanceId );

			if(iterBinding == materialBindingMap.end())
			{
				MaterialBindingPlaceholder mbp;
				mbp.boundBindingArray = new COLLADAFW::MaterialBindingArray();
				instanceGeometry->getMaterialBindings().cloneArray(*mbp.boundBindingArray);

				for ( size_t j = 0, count = mbp.boundBindingArray->getCount(); j < count; ++j)
				{
					const COLLADAFW::MaterialBinding& materialBinding = (*mbp.boundBindingArray)[j];
					mbp.mapBinding.insert( std::make_pair( materialBinding.getMaterialId(), materialBinding.getReferencedMaterial() ) );
				}

				mbp.sourceMesh = &(*iter).second;
				mbp.instanceName = name;
				mbp.uniqueId = instanceGeometry->getUniqueId();

				// iterate through primitives to find the ones that do not have material binding
				// add them to the binding list with a material unique ID found with the local name
				// (some exporters do it like this)
				for(MapUIntString::iterator locMatIter = ph.localMaterials.begin(); locMatIter != ph.localMaterials.end(); locMatIter++)
				{
					MapMaterialIdToUnique::iterator uniqueIter = mbp.mapBinding.find( locMatIter->first );
					if(uniqueIter == mbp.mapBinding.end())
					{
						// this local material has no binding, let's try to find it by name
						MapStringUnique::iterator stringMatIter = materialStringMap.find( locMatIter->second );
						if(stringMatIter != materialStringMap.end())
						{
							// insert new binding for this unbound material found by matching string names with defined materials
							mbp.mapBinding.insert( std::make_pair( locMatIter->first, stringMatIter->second ) );
							mbp.unboundBindingArray.insert( std::make_pair( stringMatIter->second, locMatIter->first ) );
						}
					}
				}

				materialBindingMap.insert( std::make_pair( object->instanceId, mbp ) );
			}

			object->setWorldMatrix( &convertMatrix(matrix) );
			object->name = name.c_str();

			(*iter).second.instanced = true;
			(*iter).second.objectsWithMesh.push_back(object);
		}
	}

	void handleInstanceNodes( const COLLADAFW::InstanceNodePointerArray& instanceNodes)
	{
		for ( size_t i = 0, count = instanceNodes.getCount(); i < count; ++i)
		{
			const COLLADAFW::InstanceNode* instanceNode = instanceNodes[i];
			const COLLADAFW::UniqueId& referencedNodeUniqueId = instanceNode->getInstanciatedObjectId();

			// find referenced node
			MapUniqueNode::iterator iter = nodeMap.find(referencedNodeUniqueId);

			if(iter == nodeMap.end())
			{
				RRReporter::report(WARN,"Node instance without parent node.\n");
				continue;
			}

			handleNode( &iter->second );
		}
	}

	void handleNode(const COLLADAFW::Node* node)
	{
		const NodeInfo& parentNodeInfo = nodeInfoStack.top();
		const COLLADABU::Math::Matrix4& parentWorldMatrix = parentNodeInfo.worldTransformation;
		COLLADABU::Math::Matrix4 worldMatrix = parentWorldMatrix * node->getTransformationMatrix();
		NodeInfo nodeInfo( worldMatrix );
		nodeInfoStack.push(nodeInfo);

		handleInstanceGeometries( node, worldMatrix );
		handleInstanceLights( node, worldMatrix );
		handleNodes( node->getChildNodes() );
		handleInstanceNodes( node->getInstanceNodes() );

		nodeInfoStack.pop();
	}

	void importNodes( const COLLADAFW::NodePointerArray& nodes)
	{
		for ( size_t i = 0, count = nodes.getCount(); i < count; ++i)
		{
			importNode(nodes[i]);
		}
	}

	void importNode(const COLLADAFW::Node* node)
	{
		nodeMap.insert( std::make_pair( node->getUniqueId(), *node ) );

		const COLLADAFW::NodePointerArray& childNodes = node->getChildNodes();
		for ( size_t i = 0, count = childNodes.getCount(); i < count; ++i)
		{
			importNode( childNodes[i] );
		}
	}

	virtual void cancel(const COLLADAFW::String& errorMessage)
	{
		return;
	}

	virtual void start()
	{
		return;
	}

	unsigned int findTextureChannelIndex(COLLADAFW::ColorOrTexture& cot, const COLLADAFW::MaterialBinding* colladaBinding, COLLADAFW::MaterialId materialId, MapMaterialIdToUVSet& mapUV)
	{
		if(!cot.isTexture())
			return UINT_MAX;

		COLLADAFW::Texture& texture = cot.getTexture();

		if(colladaBinding != NULL)
		{
			for(unsigned textbind = 0; textbind < colladaBinding->getTextureCoordinateBindingArray().getCount(); textbind++)
			{
				if( colladaBinding->getTextureCoordinateBindingArray()[textbind].getTextureMapId() == texture.getTextureMapId() )
				{
					size_t localSet = colladaBinding->getTextureCoordinateBindingArray()[textbind].getSetIndex();
					MapMaterialIdToUVSet::iterator localToPhysIter = mapUV.find(  materialId );
					if( localToPhysIter != mapUV.end() )
					{
						MapLocalToPhysicalUVSet::iterator physIter = localToPhysIter->second.find( localSet );
						if( physIter != localToPhysIter->second.end() )
						{
							// got it!
							return physIter->second;
						}
						else
							return UINT_MAX;
					}
					else
						return UINT_MAX;
				}
			}
		}

		// use first valid uv set, otherwise return max
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"No input set bound with bind_vertex_input.\n"));
		MapMaterialIdToUVSet::iterator localToPhysIter = mapUV.find(  materialId );
		if(localToPhysIter->second.size()>0)
			return localToPhysIter->second.begin()->second;
		else
			return UINT_MAX;
	}

	void applyColorOrTexture(rr::RRMaterial::Property& prop, COLLADAFW::ColorOrTexture& cot, unsigned int uvChannel, COLLADAFW::EffectCommon* common, float multiFactor = 1.0f, float defaultGrey = 0.5f, bool zeroBased = true)
	{
		if(cot.isColor())
		{
			if(zeroBased)
			{
				prop.color.x = (float)cot.getColor().getRed() * multiFactor;
				prop.color.y = (float)cot.getColor().getGreen() * multiFactor;
				prop.color.z = (float)cot.getColor().getBlue() * multiFactor;
			}
			else
			{
				prop.color.x = 1.0f - (float)cot.getColor().getRed() * multiFactor;
				prop.color.y = 1.0f - (float)cot.getColor().getGreen() * multiFactor;
				prop.color.z = 1.0f - (float)cot.getColor().getBlue() * multiFactor;
			}
		}
		else if(cot.isTexture())
		{
			COLLADAFW::Texture& texture = cot.getTexture();
			COLLADAFW::Sampler* sampler = common->getSamplerPointerArray()[ cot.getTexture().getSamplerId() ];

			MapUniqueImage::iterator imageIter = imageMap.find( sampler->getSourceImage() );

			if(imageIter != imageMap.end())
			{
				COLLADAFW::Image& image = imageIter->second;
				const COLLADABU::URI& uri = image.getImageURI();
				COLLADABU::String imagePath = COLLADABU::URI::uriDecode( uri.toNativePath() );
				// load image filename by hand, getPathFile does not work properly everytime
				std::string imageFilePath = imagePath;
				size_t pos = imageFilePath.find_last_of("/\\");
				if (pos!=-1) imageFilePath.erase(0,pos+1);

				RRBuffer* buffer;

				// disable reporter when trying different paths for textures
				RRReporter* oldReporter = RRReporter::getReporter();
				RRReporter::setReporter(NULL);

				// try absolute image path
				buffer = rr::RRBuffer::load(imagePath.c_str(),NULL);
				if(buffer == NULL)
				{
					// try joining scenefile path with image path
					std::string joinedPath = filepath + imagePath;
					buffer = rr::RRBuffer::load(joinedPath.c_str(),NULL);
					if(buffer == NULL)
					{
						// try stripping path from the image and joining it with scenefilepath
						std::string strippedPath = filepath + imageFilePath;
						buffer = rr::RRBuffer::load(strippedPath.c_str(),NULL);
					}
				}
				RRReporter::setReporter(oldReporter);

				if(buffer != NULL)
				{
					prop.texcoord = uvChannel;

					if( uvChannel != UINT_MAX )
					{	
						prop.texture = buffer;
						return;
					}
					else
					{
						RRReporter::report(WARN,"Texture bound to a mesh without a proper uv channel index.\n");
						prop.texture = buffer;
					}
				}
				else
				{
					RRReporter::report(WARN,"Can't load texture %s\n",imageFilePath.c_str());

					prop.color.x = defaultGrey;
					prop.color.y = defaultGrey;
					prop.color.z = defaultGrey;
				}
			}
		}
	}

	void handleMaterialBinding(MaterialBindingPlaceholder& bindingPlaceholder, const COLLADAFW::MaterialBinding* colladaBinding, const COLLADAFW::UniqueId& materialUniqueId, const COLLADAFW::MaterialId materialId, const bool& transparencyInverted, unsigned int& nextMaterial)
	{
		COLLADAFW::Material& colladaMaterial = materialMap.find( materialUniqueId )->second;
		MapUniqueEffect::iterator effectIter = effectMap.find( colladaMaterial.getInstantiatedEffect() );

		if( effectIter != effectMap.end() )
		{
			COLLADAFW::Effect& effect = effectIter->second;
			COLLADAFW::EffectCommon* common = effect.getCommonEffects()[0]; // NOTE all sample importers takes only the first one, is that ok?

			// get all uv set indices first (saves -1 for color only or no binding found)
			MapMaterialIdToUVSet& mapUV = bindingPlaceholder.sourceMesh->mapUV;

			unsigned int drC = findTextureChannelIndex(common->getDiffuse(),colladaBinding,materialId,mapUV);
			unsigned int deC = findTextureChannelIndex(common->getEmission(),colladaBinding,materialId,mapUV);
			unsigned int srC = findTextureChannelIndex(common->getSpecular(),colladaBinding,materialId,mapUV);
			unsigned int stC = findTextureChannelIndex(common->getOpacity(),colladaBinding,materialId,mapUV);
			unsigned int lmC = findTextureChannelIndex(common->getAmbient(),colladaBinding,materialId,mapUV);

			// see if we can find this material in cache with the same textcoord parameters
			bool identicalCached = false, changedCached = false;
			std::pair<MapUniqueCached::iterator,MapUniqueCached::iterator> range = cachedMap.equal_range( materialUniqueId );

			for(MapUniqueCached::iterator cachedIter = range.first; cachedIter != range.second; cachedIter++)
			{
				// material is cached, now let's see if it is really the same with all uv set indices
				CachedMaterial& cache = cachedIter->second;
				MapUniqueCached::iterator nextIter = cachedIter; nextIter++;

				if( cache.drC == drC && cache.deC == deC && cache.srC == srC &&
					cache.stC == stC && cache.lmC == lmC
					)
				{
					// indices are the same
					RRReporter::report(INF3,"Found cached material.\n");
					bindingPlaceholder.mapLocal.insert( std::make_pair( materialId, cache.localId ) );
					identicalCached = true;
					break;
				}
				else
				{
					// indices are different
					if( nextIter == range.second )
					{
						// this is the last set of indices, so we can safely say that
						// there is no completely same set of indices already cached for this material
						// hence, create a new material as a copy of the cached one and alter the indices
						objects->materials[ nextMaterial ] = new RRMaterial();
						RRMaterial& material = *objects->materials[ nextMaterial ];
						material.copyFrom( *objects->materials[ cache.localId ] );

						// NOTE change material name here
						// get number of previous elements e.g. with cachedMap.count( colladaBinding.getReferencedMaterial() ) 

						material.diffuseReflectance.texcoord    = drC;
						material.diffuseEmittance.texcoord      = deC;
						material.specularReflectance.texcoord   = srC;
						material.specularTransmittance.texcoord = stC;
						material.lightmapTexcoord               = lmC;

						RRReporter::report(INF3,"Found cached material with different indices.\n");
						changedCached = true;
						break;
					}
				}
			}

			if(identicalCached)
			{
				return;
			}

			bindingPlaceholder.mapLocal.insert( std::make_pair( materialId, nextMaterial ) );
			CachedMaterial cm(drC,deC,srC,stC,lmC,nextMaterial);
			cachedMap.insert( std::make_pair( materialUniqueId, cm ) );
			if(!changedCached) objects->materials[ nextMaterial ] = new RRMaterial();
			RRMaterial& material = *objects->materials[ nextMaterial ];
			nextMaterial++;

			if(changedCached)
			{
				return;
			}

			// material was not cached, let's create it from scratch from the connected effect (only COMMON) and extras
			ExtraDataEffect* extraEffect = (ExtraDataEffect*)extraHandler.getData( effect.getUniqueId() );
			ExtraDataGeometry* extraGeometry = (ExtraDataGeometry*)extraHandler.getData( bindingPlaceholder.sourceMesh->uniqueId );

			// init material
			material.reset( extraEffect->double_sided == 1.f || extraGeometry->double_sided == 1.f );
			material.lightmapTexcoord = bindingPlaceholder.sourceMesh->lastUVSet;

			// basic material properties
			applyColorOrTexture(material.diffuseReflectance, common->getDiffuse(), drC, common);
			applyColorOrTexture(material.diffuseEmittance, common->getEmission(), deC, common, extraEffect->emission_level);
			applyColorOrTexture(material.specularReflectance, common->getSpecular(), srC, common, extraEffect->spec_level);
			applyColorOrTexture(material.specularTransmittance,common->getOpacity(), stC, common, 1.0f, 0.5f, transparencyInverted);

			if(common->getIndexOfRefraction().getType() == COLLADAFW::FloatOrParam::FLOAT)
				material.refractionIndex = common->getIndexOfRefraction().getFloatValue();

			// opencollada have default -1 for refraction
			if(material.refractionIndex == -1.f)
				material.refractionIndex = 1.f;


			if( lmC != UINT_MAX )
				material.lightmapTexcoord = lmC;

			// transparency in diffuse texture
			if(material.diffuseReflectance.texture != NULL)
			{
				if( material.diffuseReflectance.texture->getFormat() == BF_RGBA )
				{
					RRReporter::report(WARN,"Transparency in diffuse map. Enabling workaround.\n");
					material.specularTransmittance.texture  = material.diffuseReflectance.texture->createReference();
					material.specularTransmittanceInAlpha   = true;
					material.specularTransmittance.texcoord = material.specularTransmittance.texcoord;
				}
			}

			material.name = colladaMaterial.getName().c_str();

			// get average colors from textures
			RRScaler* scaler = RRScaler::createRgbScaler();
			material.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
			delete scaler;

			// autodetect keying
			material.updateKeyingFromTransmittance();

			// optimize material flags
			material.updateSideBitsFromColors();
		}
		else
		{
			bindingPlaceholder.mapLocal.insert( std::make_pair( materialId, -1 ) );
		}
	}

	/** This method is called after the last write* method. No other methods will be called after this.*/
	virtual void finish()
	{
		if(parseStep == RUN_COPY_ELEMENTS)
		{
			RRReporter::report(INF3,"Finished with first pass\n");

			if(instanceVisualScene == NULL)
			{
				RRReporter::report(WARN,"No visual scene instance found in the file.\n");
				return;
			}

			// get active visual scene
			const COLLADAFW::VisualScene  *activeScene = NULL;
			for ( VectorVisualScene::iterator iter = visualSceneArray.begin(); iter != visualSceneArray.end(); iter++)
			{
				if ( instanceVisualScene->getInstanciatedObjectId() == (*iter).getUniqueId() )
				{
					activeScene = &(*iter);
					break;
				}
			}

			if(activeScene == NULL)
			{
				RRReporter::report(WARN,"Instanced visual scene not found in <library_visual_scenes>.\n");
				return;
			}

			// import all nodes from visual scene + library nodes
			importNodes( activeScene->getRootNodes() );
			for( VectorLibraryNodes::iterator iter = libraryNodesArray.begin(); iter != libraryNodesArray.end(); iter++ )
			{
				importNodes( (*iter).getNodes() );
			}

			// now handle all nodes and all instances and such from the root of active visual scene
			COLLADABU::Math::Matrix4 baseMatrix = COLLADABU::Math::Matrix4::IDENTITY;

			switch(upAxis)
			{
			case COLLADAFW::FileInfo::Z_UP:
				baseMatrix.setElement(1,1,0.0f);
				baseMatrix.setElement(1,2,1.0f);
				baseMatrix.setElement(2,1,-1.0f);
				baseMatrix.setElement(2,2,0.0f);
				break;

			/*case COLLADAFW::FileInfo::X_UP:
				baseMatrix.setElement(0,0,0.0f);
				baseMatrix.setElement(0,1,1.0f);
				baseMatrix.setElement(1,0,-1.0f);
				baseMatrix.setElement(1,1,0.0f);
				break;*/
			}

			// set rescale matrix according to linear unit in the scene
			COLLADABU::Math::Matrix4 scaleMatrix = COLLADABU::Math::Matrix4::IDENTITY;
			scaleMatrix.setElement(0,0,rescaleUnit);
			scaleMatrix.setElement(1,1,rescaleUnit);
			scaleMatrix.setElement(2,2,rescaleUnit);

			baseMatrix = baseMatrix * scaleMatrix;

			NodeInfo nodeInfo( baseMatrix );
			nodeInfoStack.push( nodeInfo );

			handleNodes( activeScene->getRootNodes() );

			RRReporter::report(INF3,"Nodes handled\n");

			// get the number of instantiated parent meshes
			int numberOfMeshes = 0;
			for(MapUniqueMesh::iterator iter = meshMap.begin(); iter != meshMap.end(); iter++)
			{
				MeshPlaceholder& ph = (*iter).second;

				if(ph.instanced && ph.numPrimitives != 0 && ph.numTriangles != 0 )
					numberOfMeshes++;
			}
			objects->numMeshes = numberOfMeshes;
			objects->meshes = new RRMeshArrays[numberOfMeshes];
			objects->colliders = new const RRCollider*[numberOfMeshes];

			for(int i=0; i<numberOfMeshes; i++)
				objects->colliders[i] = NULL;

			// create array for all possible material instances
			int numberOfMaterialInstances = 0;
			for(MapUniqueMaterialBinding::iterator iter = materialBindingMap.begin(); iter != materialBindingMap.end(); iter++)
			{
				numberOfMaterialInstances += iter->second.boundBindingArray->getCount();
				numberOfMaterialInstances += iter->second.unboundBindingArray.size();
			}
			objects->materials = (RRMaterial**)malloc(sizeof(RRMaterial*)*numberOfMaterialInstances);

			for(int i=0; i<numberOfMaterialInstances; i++)
				objects->materials[i] = NULL;

			// NOTE because of possible transparency bug, let's find out which is the prevalent transparency default
			// let's assume that one would not want most of the objects to be completely transparent
			// would not work for a scene with no 0 or 1 transparency values :( goddamn you, opencollada, for not letting me
			// know the default value ...
			bool transparencyInverted = false;
			unsigned transValue[2]; transValue[0] = 0; transValue[1] = 0;
			for(MapUniqueMaterialBinding::iterator iter = materialBindingMap.begin(); iter != materialBindingMap.end(); iter++)
			{
				MaterialBindingPlaceholder& bindingPlaceholder = iter->second;
				for(unsigned i = 0; i < bindingPlaceholder.boundBindingArray->getCount(); i++)
				{
					COLLADAFW::MaterialBinding& colladaBinding = (*bindingPlaceholder.boundBindingArray)[i];
					COLLADAFW::Material& colladaMaterial = materialMap.find( colladaBinding.getReferencedMaterial() )->second;
					MapUniqueEffect::iterator effectIter = effectMap.find( colladaMaterial.getInstantiatedEffect() );

					if( effectIter != effectMap.end() )
					{
						COLLADAFW::Effect& effect = effectIter->second;
						COLLADAFW::EffectCommon* common = effect.getCommonEffects()[0];

						if(common->getOpacity().isColor())
						{
							COLLADAFW::Color& color = common->getOpacity().getColor();
							double all = color.getRed() * color.getBlue() * color.getGreen();
							if(all == 0.0)
								transValue[0]++;
							else if(all == 1.0)
								transValue[1]++;
						}
					}
				}
			}

			if(transValue[0] > transValue[1])
			{
				RRReporter::report(WARN,"Transparency looks inverted. Enabling workaround.\n");
				transparencyInverted = true;
			}

			// handle all loaded material instances (material bindings)
			unsigned nextMaterial = 0;
			for(MapUniqueMaterialBinding::iterator iter = materialBindingMap.begin(); iter != materialBindingMap.end(); iter++)
			{
				MaterialBindingPlaceholder& bindingPlaceholder = iter->second;

				// handle all with defined bindings
				for(unsigned i = 0; i < bindingPlaceholder.boundBindingArray->getCount(); i++)
				{
					COLLADAFW::MaterialBinding& colladaBinding = (*bindingPlaceholder.boundBindingArray)[i];
					handleMaterialBinding(bindingPlaceholder, &colladaBinding, colladaBinding.getReferencedMaterial(), colladaBinding.getMaterialId(), transparencyInverted, nextMaterial);
				}

				// handle all without defined bindings, for which a material was found with the local string
				for(MapUniqueToMaterialId::iterator unboundIter = bindingPlaceholder.unboundBindingArray.begin(); unboundIter != bindingPlaceholder.unboundBindingArray.end(); unboundIter++ )
				{
					handleMaterialBinding(bindingPlaceholder, NULL, unboundIter->first, unboundIter->second, transparencyInverted, nextMaterial);
				}
			}

			objects->numMaterials = nextMaterial;
			objects->materials = (RRMaterial**)realloc(objects->materials,sizeof(RRMaterial*)*objects->numMaterials);

			RRReporter::report(INF3,"Finished with materials\n");

			objects->defaultMaterial.reset(false);
			objects->defaultMaterial.lightmapTexcoord = UINT_MAX;
			objects->defaultMaterial.updateKeyingFromTransmittance();
			objects->defaultMaterial.updateSideBitsFromColors();

			// parse the file again, now creating all neccessary geometry and pairing it to instances
			RRReporter::report(INF3,"Starting second pass\n");
			parseStep = RUN_GEOMETRY;
			parseDocument();

			// clean material binding nodes
			for(MapUniqueMaterialBinding::iterator iter = materialBindingMap.begin(); iter != materialBindingMap.end(); iter++)
			{
				delete (COLLADAFW::MaterialBindingArray*)iter->second.boundBindingArray;
			}
		}
		else
		{
			RRReporter::report(INF3,"Finished with second pass\n");
		}
	}

	/** When this method is called, the writer must write the global document asset.
	@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGlobalAsset ( const COLLADAFW::FileInfo* asset )
	{
		upAxis = asset->getUpAxisType();
		rescaleUnit = asset->getUnit().getLinearUnitMeter();
		return true;
	}

	/** Writes the entire visual scene.
	@return True on succeeded, false otherwise.*/
	virtual bool writeVisualScene ( const COLLADAFW::VisualScene* visualScene )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		visualSceneArray.push_back( *visualScene );
		return true;
	}

	/** Writes the scene.
	@return True on succeeded, false otherwise.*/
	virtual bool writeScene ( const COLLADAFW::Scene* scene )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		if(scene->getInstanceVisualScene() != NULL)
			instanceVisualScene = scene->getInstanceVisualScene ()->clone ();

		return true;
	}

	/** Handles all nodes in the library nodes.
	@return True on succeeded, false otherwise.*/
	virtual bool writeLibraryNodes( const COLLADAFW::LibraryNodes* libraryNodes )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		libraryNodesArray.push_back( *libraryNodes );
		return true;
	}

	struct UniqueData
	{
		unsigned int uniqueIndex;
		unsigned int* data;

		UniqueData()
		{
			data = NULL;
		}

		void cleanup()
		{
			if(data != NULL)
				delete [] data;
		}
	};

	typedef std::multimap<unsigned int, UniqueData> MapIntToUniqueData;


	template<typename T>
	void assignWithCheck(rr::RRReal& assigned, const COLLADAFW::ArrayPrimitiveType<T>* assignee, const unsigned int& position, const unsigned int& maximum)
	{
		if(position < maximum)
			assigned = (rr::RRReal)(*assignee)[position];
		else
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Vertex index out of bounds.\n"));
			assigned = 0.0f;
		}
	}

	/** Writes the geometry.
	@return True on succeeded, false otherwise.*/
	virtual bool writeGeometry ( const COLLADAFW::Geometry* geometry )
	{
		if(geometry->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH)
			return true;

		if(parseStep == RUN_COPY_ELEMENTS)
		{
			COLLADAFW::Mesh* colladaMesh = (COLLADAFW::Mesh*)geometry;

			MeshPlaceholder ph;
			ph.meshName = colladaMesh->getName();
			ph.uniqueId = colladaMesh->getUniqueId();

			extraHandler.getOrInsertDefaultData<ExtraDataGeometry>( colladaMesh->getUniqueId() );

			// run through primitives to get textcoord channels mapping + map local material ids to material name string
			ph.lastUVSet = 0;
			const COLLADAFW::MeshPrimitiveArray& primitiveElementsArray = colladaMesh->getMeshPrimitives();
			for(size_t prim = 0; prim < primitiveElementsArray.getCount(); prim++)
			{
				COLLADAFW::MeshPrimitive* primitiveElement = primitiveElementsArray [ prim ];

				ph.localMaterials.insert( std::make_pair( primitiveElement->getMaterialId(), primitiveElement->getMaterial() ) );

				MapLocalToPhysicalUVSet locToPhys;

				for(size_t uv=0; uv<primitiveElement->getUVCoordIndicesArray().getCount(); ++uv)
				{
					COLLADAFW::IndexList* uvChannel = primitiveElement->getUVCoordIndices(uv);
					size_t uvPhysicalSet = colladaMesh->getUVSetIndexByName(uvChannel->getName());
					size_t uvLocalSet = uvChannel->getSetIndex();

					if(ph.lastUVSet < uvPhysicalSet)
						ph.lastUVSet = uvPhysicalSet;

					locToPhys.insert( std::make_pair( uvLocalSet, uvPhysicalSet ) );
				}

				COLLADAFW::MaterialId matId = primitiveElement->getMaterialId();
				ph.mapUV.insert( std::make_pair( matId, locToPhys ) );
			}

			// run through primitives to get number of triangles (also for triangulable surfaces)
			ph.numPrimitives = primitiveElementsArray.getCount();

			size_t& numPrimitives = ph.numPrimitives;
			size_t& numTriangles = ph.numTriangles;
			size_t& numIndices = ph.numIndices;

			// base: triangles
			numTriangles = colladaMesh->getTrianglesTriangleCount();
			numIndices = colladaMesh->getTrianglesTriangleCount()*3;

			// polygons/polylists/trifans/tristrips
			for(size_t prim = 0; prim<numPrimitives; prim++)
			{
				COLLADAFW::MeshPrimitive* primitiveElement = primitiveElementsArray [ prim ];
				const COLLADAFW::MeshPrimitive::PrimitiveType& primitiveType = primitiveElement->getPrimitiveType();

				if(primitiveType != COLLADAFW::MeshPrimitive::POLYLIST &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_FANS &&
					primitiveType != COLLADAFW::MeshPrimitive::POLYGONS
					)
					continue;

				size_t numGroups = primitiveElement->getGroupedVertexElementsCount();

				for(size_t ve = 0; ve<numGroups; ve++)
				{
					size_t numVertices = primitiveElement->getGroupedVerticesVertexCount(ve);

					if(numVertices >= 3)
					{
						numTriangles += numVertices - 2;
					}

					numIndices += numVertices;
				}
			}

			meshMap.insert( std::make_pair( geometry->getUniqueId(), ph ) );
		}

		if(parseStep == RUN_GEOMETRY)
		{
			MapUniqueMesh::iterator meshPlaceholderIter = meshMap.find( geometry->getUniqueId() );
			if(meshPlaceholderIter == meshMap.end() || !(*meshPlaceholderIter).second.instanced)
				return true;

			MeshPlaceholder& meshPlaceholder = (*meshPlaceholderIter).second;

			COLLADAFW::Mesh* colladaMesh = (COLLADAFW::Mesh*)geometry;
			const COLLADAFW::String meshName = colladaMesh->getName();
			const COLLADAFW::MeshPrimitiveArray& primitiveElementsArray = colladaMesh->getMeshPrimitives();
			size_t numPrimitives = meshPlaceholder.numPrimitives;

			if(numPrimitives == 0)
				return true;

			size_t numTriangles  = meshPlaceholder.numTriangles;
			size_t numIndices    = meshPlaceholder.numIndices;

			if(numTriangles == 0)
				return true;

			IndexInfo* remapInfo = new IndexInfo[ numIndices ];
			size_t currIndexInMesh = 0;

			unsigned int meshNumUvChannels = colladaMesh->getUVCoords().getNumInputInfos();
			unsigned int meshPosNormChannels = 1 + colladaMesh->hasNormals();
			unsigned int meshNumChannels = meshPosNormChannels + meshNumUvChannels;

			MapIntToUniqueData uniqueMap;
			size_t currUniqueIndex = 0;

			// find out the number of unique combinations of indices accross all primitives
			for(size_t prim = 0; prim<numPrimitives; prim++)
			{
				COLLADAFW::MeshPrimitive* primitiveElement = primitiveElementsArray [ prim ];
				const COLLADAFW::MeshPrimitive::PrimitiveType& primitiveType = primitiveElement->getPrimitiveType();

				// skip all nontriangulable primitives
				if(primitiveType != COLLADAFW::MeshPrimitive::TRIANGLES &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_FANS &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS &&
					primitiveType != COLLADAFW::MeshPrimitive::POLYLIST &&
					primitiveType != COLLADAFW::MeshPrimitive::POLYGONS
					)
					continue;

				// get primitive number of channels = positions + normals + uvsets
				int primitiveNumChannels = 1 + primitiveElement->hasNormalIndices() + primitiveElement->getUVCoordIndicesArray().getCount();

				unsigned int* indexArray = new unsigned int[meshNumChannels];

				// go through all indexes, find unique combinations and create a remap
				// NOTE for objects without normals all vertices are unique
				if(primitiveElement->hasNormalIndices())
				{
					for(size_t vert=0; vert<primitiveElement->getPositionIndices().getCount(); ++vert)
					{
						// reset all indexes (some may not be set by this vertex)
						for(unsigned cl = 0; cl < meshNumChannels; cl++)
							indexArray[cl] = -1;

						// create array for hashing
						indexArray[0] = primitiveElement->getPositionIndices()[vert];

						if(primitiveElement->hasNormalIndices())
							indexArray[1] = primitiveElement->getNormalIndices()[vert];

						for(size_t uv=0; uv<primitiveElement->getUVCoordIndicesArray().getCount(); ++uv)
						{
							COLLADAFW::IndexList* uvChannel = primitiveElement->getUVCoordIndices(uv);
							int set = colladaMesh->getUVSetIndexByName( uvChannel->getName() );
							RR_ASSERT(set >= 0);
							indexArray[meshPosNormChannels + set] = uvChannel->getIndex(vert);
						}

						// hash to get ~uniqueness of each set of indices
						unsigned int hash = 0;
						for (size_t i=0; i<meshNumChannels; ++i)
						{
							hash = hash * 131 + (indexArray[i]+1);
						}

						std::pair<MapIntToUniqueData::iterator,MapIntToUniqueData::iterator> range = uniqueMap.equal_range( hash );

						bool insertNew = true;

						for(MapIntToUniqueData::iterator iter = range.first; iter != range.second; iter++)
						{
							// check if it is really the same (hash can be colliding)
							bool same = true;
							for(unsigned cp = 0; cp < meshNumChannels; cp++)
							{
								if(iter->second.data[cp] != indexArray[cp])
								{
									same = false;
									break;
								}
							}

							if(same)
							{
								// this set of indices is really already in the vertex buffer
								RR_ASSERT(currIndexInMesh < numIndices);
								remapInfo[ currIndexInMesh ].remap	= iter->second.uniqueIndex;
								remapInfo[ currIndexInMesh ].insert	= false;
								insertNew = false;
								break;
							}
						}
						
						if(insertNew)
						{
							// will need to be inserted
							RR_ASSERT(currIndexInMesh < numIndices);
							remapInfo[ currIndexInMesh ].remap	= currUniqueIndex;
							remapInfo[ currIndexInMesh ].insert	= true;

							// copy indices for future check 
							UniqueData ud;
							ud.data = new unsigned int[meshNumChannels];
							ud.uniqueIndex = currUniqueIndex;

							for(unsigned cp = 0; cp < meshNumChannels; cp++)
							{
								ud.data[cp] = indexArray[cp];
							}

							uniqueMap.insert( std::make_pair( hash, ud ) );

							currUniqueIndex++;
						}

						currIndexInMesh++;
					}
				}
				else
				{
					// no normals, we will generate them, thus every vertex is unique
					for(size_t vert=0; vert<primitiveElement->getPositionIndices().getCount(); ++vert)
					{
						remapInfo[ currIndexInMesh ].remap = currUniqueIndex;
						remapInfo[ currIndexInMesh ].insert = true;

						currUniqueIndex++;
						currIndexInMesh++;
					}
				}

				delete [] indexArray;
			}

			// clean up unique data buffers
			for(MapIntToUniqueData::iterator iter = uniqueMap.begin(); iter != uniqueMap.end(); iter++)
			{
				iter->second.cleanup();
			}

			// resize collada mesh
			size_t numVertices = currUniqueIndex;
			size_t numUVSets = colladaMesh->getUVCoords().getNumInputInfos();

			RRVector<unsigned> texcoords;

			for(unsigned i=0;i<numUVSets;i++)
			{
				COLLADAFW::String setName = colladaMesh->getUVCoords().getName(i);
				size_t setIndex = colladaMesh->getUVSetIndexByName( setName );
				texcoords.push_back(setIndex);
			}

			RRMeshArrays* mesh = &objects->meshes[ objects->nextMesh ];
			mesh->resizeMesh(numTriangles,numVertices,&texcoords,false);

			size_t currTriangle = 0;
			currIndexInMesh = 0;

			VectorFaceGroupPlaceholder faceGroupArray;

			// fill lightsprint mesh with remapped data
			for(size_t prim = 0; prim<numPrimitives; prim++)
			{
				COLLADAFW::MeshPrimitive* primitiveElement = primitiveElementsArray [ prim ];
				const COLLADAFW::MeshPrimitive::PrimitiveType& primitiveType = primitiveElement->getPrimitiveType();

				// skip all nontriangulable primitives
				if(primitiveType != COLLADAFW::MeshPrimitive::TRIANGLES &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_FANS &&
					primitiveType != COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS &&
					primitiveType != COLLADAFW::MeshPrimitive::POLYLIST &&
					primitiveType != COLLADAFW::MeshPrimitive::POLYGONS
					)
					continue;

				// get primitive number of channels = positions + normals + uvsets
				int primitiveNumChannels = 1 + primitiveElement->hasNormalIndices() + primitiveElement->getUVCoordIndicesArray().getCount();

				size_t currVertex = 0;
				size_t currPrimitiveTriangles = 0;

				size_t currTriangleInGroup = 0;
				size_t currVertexInGroup = 0;
				size_t currGroup = 0;
				size_t currGroupStartIndex = 0;

				size_t positionsCount = colladaMesh->getPositions().getValuesCount();
				size_t normalsCount = colladaMesh->getNormals().getValuesCount();
				size_t uvsCount = colladaMesh->getUVCoords().getValuesCount();

				// go through all indexes and insert data as necessary
				for(size_t vert=0; vert<primitiveElement->getPositionIndices().getCount(); ++vert)
				{
					if( remapInfo[ currIndexInMesh ].insert )
					{
						unsigned int remappedIndex = remapInfo[ currIndexInMesh ].remap;

						// add position
						unsigned int positionIndex = primitiveElement->getPositionIndices()[vert] * 3;
						switch(colladaMesh->getPositions().getType())
						{
						case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
							{
								const COLLADAFW::ArrayPrimitiveType<float>* values = colladaMesh->getPositions().getFloatValues();
								RR_ASSERT(values->getCount() == positionsCount);
								assignWithCheck<float>(mesh->position[remappedIndex].x,values,positionIndex,positionsCount);
								assignWithCheck<float>(mesh->position[remappedIndex].y,values,positionIndex+1,positionsCount);
								assignWithCheck<float>(mesh->position[remappedIndex].z,values,positionIndex+2,positionsCount);
								break;
							}

						case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
							{
								const COLLADAFW::ArrayPrimitiveType<double>* values = colladaMesh->getPositions().getDoubleValues();
								RR_ASSERT(values->getCount() == positionsCount);
								assignWithCheck<double>(mesh->position[remappedIndex].x,values,positionIndex,positionsCount);
								assignWithCheck<double>(mesh->position[remappedIndex].y,values,positionIndex+1,positionsCount);
								assignWithCheck<double>(mesh->position[remappedIndex].z,values,positionIndex+2,positionsCount);
								break;
							}

						default:
							RR_ASSERT(false);
							break;
						}

						// add normal
						if(colladaMesh->hasNormals())
						{
							if(primitiveElement->hasNormalIndices())
							{
								unsigned int normalIndex = primitiveElement->getNormalIndices()[vert] * 3;
								switch(colladaMesh->getNormals().getType())
								{
								case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
									{
										const COLLADAFW::ArrayPrimitiveType<float>* values = colladaMesh->getNormals().getFloatValues();
										RR_ASSERT(values->getCount() == normalsCount);
										assignWithCheck<float>(mesh->normal[remappedIndex].x,values,normalIndex,normalsCount);
										assignWithCheck<float>(mesh->normal[remappedIndex].y,values,normalIndex+1,normalsCount);
										assignWithCheck<float>(mesh->normal[remappedIndex].z,values,normalIndex+2,normalsCount);
										break;
									}

								case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
									{
										const COLLADAFW::ArrayPrimitiveType<double>* values = colladaMesh->getNormals().getDoubleValues();
										RR_ASSERT(values->getCount() == normalsCount);
										assignWithCheck<double>(mesh->normal[remappedIndex].x,values,normalIndex,normalsCount);
										assignWithCheck<double>(mesh->normal[remappedIndex].y,values,normalIndex+1,normalsCount);
										assignWithCheck<double>(mesh->normal[remappedIndex].z,values,normalIndex+2,normalsCount);
										break;
									}

								default:
									RR_ASSERT(false);
									break;
								}
							}
							else
							{
								// we will generate those when the whole triangle is loaded
							}
						}

						// add uv channels
						if(meshNumUvChannels != 0)
						{
							for(size_t uvSet=0; uvSet<meshNumUvChannels; uvSet++)
							{
								mesh->texcoord[uvSet][remappedIndex] = RRVec2(0, 0);
							}

							for(size_t uv=0; uv<primitiveElement->getUVCoordIndicesArray().getCount(); ++uv)
							{
								//continue;
								COLLADAFW::IndexList* uvChannel = primitiveElement->getUVCoordIndices(uv);

								unsigned int uvIndex	= uvChannel->getIndex(vert);
								unsigned int uvStride	= uvChannel->getStride();
								unsigned int uvStart	= uvChannel->getInitialIndex();

								// physical set index for the whole mesh
								size_t uvSet = colladaMesh->getUVSetIndexByName(uvChannel->getName());

								uvIndex	= uvStride * uvIndex;

								switch(colladaMesh->getUVCoords().getType())
								{
								case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
									{
										const COLLADAFW::ArrayPrimitiveType<float>* values = colladaMesh->getUVCoords().getFloatValues();
										RR_ASSERT(values->getCount() == uvsCount);
										if(uvIndex+1 < uvsCount)
											mesh->texcoord[uvSet][remappedIndex] = RRVec2((*values)[uvIndex], (*values)[uvIndex+1]);
										else
										{
											RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Vertex UV index out of bounds.\n"));
											mesh->texcoord[uvSet][remappedIndex] = RRVec2(0.0f,0.0f);
										}
										break;
									}

								case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
									{
										const COLLADAFW::ArrayPrimitiveType<double>* values = colladaMesh->getUVCoords().getDoubleValues();
										RR_ASSERT(values->getCount() == uvsCount);
										if(uvIndex+1 < uvsCount)
											mesh->texcoord[uvSet][remappedIndex] = RRVec2((float)(*values)[uvIndex], (float)(*values)[uvIndex+1]);
										else
										{
											RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Vertex UV index out of bounds.\n"));
											mesh->texcoord[uvSet][remappedIndex] = RRVec2(0.0f,0.0f);
										}
										break;
									}

								default:
									RR_ASSERT(false);
									break;
								}
							}
						}
					}

					// triangulate when neccessary
					if(primitiveType == COLLADAFW::MeshPrimitive::TRIANGLES)
					{
						mesh->triangle[currTriangle][currVertex] = remapInfo[ currIndexInMesh ].remap;
						currVertex++;
					}
					// opencollada returns polylists as polygons
					else if(primitiveType == COLLADAFW::MeshPrimitive::POLYLIST ||
						primitiveType == COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS ||
						primitiveType == COLLADAFW::MeshPrimitive::TRIANGLE_FANS ||
						primitiveType == COLLADAFW::MeshPrimitive::POLYGONS
						)
					{
						if( primitiveType == COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS )
						{
							if(currVertexInGroup >= 2)
							{
								if ((currTriangleInGroup & 0x1) == 0x0)
								{
									mesh->triangle[currTriangle][0] = remapInfo[ currIndexInMesh - 2 ].remap;
									mesh->triangle[currTriangle][1] = remapInfo[ currIndexInMesh - 1 ].remap;
									mesh->triangle[currTriangle][2] = remapInfo[ currIndexInMesh ].remap;
								}
								else
								{
									mesh->triangle[currTriangle][0] = remapInfo[ currIndexInMesh - 2 ].remap;
									mesh->triangle[currTriangle][1] = remapInfo[ currIndexInMesh ].remap;
									mesh->triangle[currTriangle][2] = remapInfo[ currIndexInMesh - 1 ].remap;
								}
								currVertex = 3;
							}
						}
						else
						{
							if(currVertexInGroup < 3)
							{
								if(currVertexInGroup == 0)
									currGroupStartIndex = currIndexInMesh;

								mesh->triangle[currTriangle][currVertex] = remapInfo[ currIndexInMesh ].remap;
								currVertex++;
							} 
							else
							{
								mesh->triangle[currTriangle][0] = remapInfo[ currGroupStartIndex ].remap;
								mesh->triangle[currTriangle][1] = remapInfo[ currIndexInMesh - 1 ].remap;
								mesh->triangle[currTriangle][2] = remapInfo[ currIndexInMesh ].remap;
								currVertex = 3;
							}
						}

						// move in grouped vertices
						currVertexInGroup++;
						int cvig = primitiveElement->getGroupedVerticesVertexCount( currGroup );
						if( (int)currVertexInGroup >= primitiveElement->getGroupedVerticesVertexCount( currGroup ) )
						{
							currTriangleInGroup = 0;
							currVertexInGroup = 0;
							currGroup++;
						}
					}

					if(currVertex >= 3)
					{
						if(!colladaMesh->hasNormals())
						{
							// generate normals for the whole triangle
							rr::RRVec3* v[3];

							for(int i=0;i<3;i++)
								v[i] = &mesh->position[ mesh->triangle[currTriangle][i] ];

							rr::RRVec3 normal = (*v[1] - *v[0]).cross( *v[2] - *v[0] );
							normal.normalize();

							for(int i=0;i<3;i++)
								mesh->normal[ mesh->triangle[currTriangle][i] ] = normal;
						}

						// jump to next triangle; don't add if just restarted
						if(currVertexInGroup != 0)
							currTriangleInGroup++;

						currPrimitiveTriangles++;
						currTriangle++;
						currVertex = 0;
					}

					currIndexInMesh++;
				}

				FaceGroupPlaceholder faceGroup;
				faceGroup.materialString = primitiveElement->getMaterial();
				faceGroup.materialId = primitiveElement->getMaterialId();
				faceGroup.triangleCount = currPrimitiveTriangles;
				faceGroupArray.push_back(faceGroup);

				if(currVertex != 0)
					RRReporter::report(WARN,"Primitive in '%s' does not have correct number of vertex indices (correct = 3*triangle count).\n",meshName.c_str());
			}

			delete [] remapInfo;

			RR_ASSERT(mesh->numTriangles == currTriangle);
			mesh->numTriangles = currTriangle;

			// create collider
			bool aborting = false;
			objects->colliders[ objects->nextMesh ] = RRCollider::create(mesh,RRCollider::IT_LINEAR,aborting);

			// set all assigned instances
			for(VectorRRObjectOpenCollada::iterator iter = meshPlaceholder.objectsWithMesh.begin(); iter != meshPlaceholder.objectsWithMesh.end(); iter++)
			{
				RRObjectOpenCollada* object = (*iter);
				MapUniqueMaterialBinding::iterator mbiter = materialBindingMap.find( object->instanceId );
				bool notBound = ( mbiter == materialBindingMap.end() );

				object->setCollider( objects->colliders[ objects->nextMesh ] );

				for(VectorFaceGroupPlaceholder::iterator fgiter = faceGroupArray.begin(); fgiter != faceGroupArray.end(); fgiter++)
				{
					rr::RRMaterial* mat;

					if(notBound)
					{
						mat = &objects->defaultMaterial;
					}
					else
					{
						MapMaterialIdToLocal::iterator localIter = mbiter->second.mapLocal.find( fgiter->materialId );

						if(localIter != mbiter->second.mapLocal.end())
						{
							int materialId = localIter->second;

							if(materialId != -1)
							{
								mat = objects->materials[ materialId ];
							}
							else
								mat = &objects->defaultMaterial;
						}
						else
						{
							mat = &objects->defaultMaterial;
							// There is no material bound to this geometry instance, or there's no material of that name present in the file
							RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Geometry instance does not have correctly bound material.\n",meshName.c_str()));
						}
					}

					object->faceGroups.push_back(
						RRObject::FaceGroup( mat , fgiter->triangleCount )
						);
				}

				objects->push_back(object);
			}

			// clean up
			meshPlaceholder.objectsWithMesh.clear();

			objects->nextMesh++;
		}

		return true;
	}

	/** Writes the material.
	@return True on succeeded, false otherwise.*/
	virtual bool writeMaterial( const COLLADAFW::Material* material )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		materialMap.insert( std::make_pair( material->getUniqueId(), *material ) );

		if(material->getName() != "")
			materialStringMap.insert( std::make_pair( material->getName(), material->getUniqueId() ) );

		return true;
	}

	/** Writes the effect.
	@return True on succeeded, false otherwise.*/
	virtual bool writeEffect( const COLLADAFW::Effect* effect )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		effectMap.insert( std::make_pair( effect->getUniqueId(), *effect ) );
		extraHandler.getOrInsertDefaultData<ExtraDataEffect>( effect->getUniqueId() );
		return true;
	}

	/** Writes the camera.
	@return True on succeeded, false otherwise.*/
	virtual bool writeCamera( const COLLADAFW::Camera* camera )
	{
		return true;
	}

	/** Writes the image.
	@return True on succeeded, false otherwise.*/
	virtual bool writeImage( const COLLADAFW::Image* image )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		imageMap.insert( std::make_pair( image->getUniqueId(), *image ) );
		return true;
	}

	/** Writes the light.
	@return True on succeeded, false otherwise.*/
	virtual bool writeLight( const COLLADAFW::Light* light )
	{
		if(parseStep != RUN_COPY_ELEMENTS)
			return true;

		lightMap.insert( std::make_pair( light->getUniqueId(), *light ) );
		extraHandler.getOrInsertDefaultData<ExtraDataLight>( light->getUniqueId() );
		return true;
	}

	/** Writes the animation.
	@return True on succeeded, false otherwise.*/
	virtual bool writeAnimation( const COLLADAFW::Animation* animation )
	{
		return true;
	}

	/** Writes the animation.
	@return True on succeeded, false otherwise.*/
	virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList )
	{
		return true;
	}

	/** Writes the skin controller data.
	@return True on succeeded, false otherwise.*/
	virtual bool writeSkinControllerData( const COLLADAFW::SkinControllerData* skinControllerData )
	{
		return true;
	}

	/** Writes the controller.
	@return True on succeeded, false otherwise.*/
	virtual bool writeController( const COLLADAFW::Controller* Controller )
	{
		return true;
	}

	/** When this method is called, the writer must write the formulas. All the formulas of the entire
	COLLADA file are contained in @a formulas.
	@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeFormulas( const COLLADAFW::Formulas* formulas )
	{
		return true;
	}

	/** When this method is called, the writer must write the kinematics scene.
	@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeKinematicsScene( const COLLADAFW::KinematicsScene* kinematicsScene )
	{
		return true;
	};

private:
	/** Disable default copy ctor. */
	RRWriterOpenCollada( const RRWriterOpenCollada& pre );
	/** Disable default assignment operator. */
	const RRWriterOpenCollada& operator= ( const RRWriterOpenCollada& pre );
};

//////////////////////////////////////////////////////////////////////////////
//
// RRSceneOpenCollada

RRScene* RRSceneOpenCollada::load(const char* filename, bool* aborting, float emissiveMultiplier)
{
	//return NULL;
	RRReportInterval report(INF2,"Importing with OpenCollada\n");

	RRSceneOpenCollada* scene = new RRSceneOpenCollada;

	RRObjectsOpenCollada* objects = new RRObjectsOpenCollada;
	RRLightsOpenCollada* lights = new RRLightsOpenCollada;

	RRWriterOpenCollada writer(objects, lights, filename);

	if( !writer.parseDocument() )
		return false;

	scene->protectedObjects = objects;
	scene->protectedLights = lights;

	return scene;
}

bool RRSceneOpenCollada::save141(const RRScene* scene, const char* filename)
{
	RRReportInterval report(INF2,"Exporting Collada 1.4.1\n");
	return false;
}

bool RRSceneOpenCollada::save150(const RRScene* scene, const char* filename)
{
	RRReportInterval report(INF2,"Exporting Collada 1.5.0\n");
	return false;
}

//////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderOpenCollada()
{
	RRScene::registerLoader("*.dae",RRSceneOpenCollada::load);
//	RRScene::registerSaver("*.1.5.0.dae",RRSceneOpenCollada::save150);
//	RRScene::registerSaver("*.1.4.1.dae",RRSceneOpenCollada::save141);
}

#endif // SUPPORT_OPENCOLLADA
