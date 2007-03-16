#pragma once

#include "RRMesh.h"
#include <cassert>


namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshMulti
//
// Merges multiple mesh importers together.
// Space is not transformed here, underlying meshes must already share one space.

class RRMeshMulti : public RRMesh
{
public:
	// creators
	static RRMesh* create(RRMesh* const* mesh, unsigned numMeshes)
		// all parameters (meshes, array of meshes) are destructed by caller, not by us
		// array of meshes must live during this call
		// meshes must live as long as created multimesh
	{
		switch(numMeshes)
		{
		case 0: 
			return NULL;
		case 1: 
			assert(mesh);
			return mesh[0];
		default: 
			assert(mesh); 
			return new RRMeshMulti(
				create(mesh,numMeshes/2),numMeshes/2,
				create(mesh+numMeshes/2,numMeshes-numMeshes/2),numMeshes-numMeshes/2);
		}
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		// all objects have the same channels, so let's simply ask object[0].
		// equality must be ensured by creator of multiobject.
		//!!! check equality at construction time
		pack[0].getMesh()->getChannelSize(channelId,numItems,itemSize);
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		unsigned pack0Items = 0;
		switch(channelId&0x7ffff000)
		{
		case INDEXED_BY_VERTEX:
			pack0Items = pack[0].getNumVertices();
			break;
		case INDEXED_BY_TRIANGLE:
			pack0Items = pack[0].getNumTriangles();
			break;
		case INDEXED_BY_MATERIAL:
			//pack0Items = pack[0].getNumMaterials();
			//!!! assumption: all objects share the same material library
			pack0Items = UINT_MAX;
			break;
		case INDEXED_BY_OBJECT:
			pack0Items = pack[0].getNumObjects();
			break;
		default:
			return false;
		}
		if(itemIndex<pack0Items) return pack[0].getMesh()->getChannelData(channelId,itemIndex,itemData,itemSize);
		return pack[1].getMesh()->getChannelData(channelId,itemIndex-pack0Items,itemData,itemSize);
	}

	// vertices
	virtual unsigned     getNumVertices() const
	{
		return pack[0].getNumVertices()+pack[1].getNumVertices();
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		if(v<pack[0].getNumVertices()) 
			pack[0].getMesh()->getVertex(v,out);
		else
			pack[1].getMesh()->getVertex(v-pack[0].getNumVertices(),out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return pack[0].getNumTriangles()+pack[1].getNumTriangles();
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		if(t<pack[0].getNumTriangles()) 
			pack[0].getMesh()->getTriangle(t,out);
		else
		{
			pack[1].getMesh()->getTriangle(t-pack[0].getNumTriangles(),out);
			out[0] += pack[0].getNumVertices();
			out[1] += pack[0].getNumVertices();
			out[2] += pack[0].getNumVertices();
		}
	}

	// optional for faster access
	//!!! default is slow
	//virtual void         getTriangleBody(unsigned i, TriangleBody* t) const
	//{
	//}

	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
//		assert(0);//!!! just mark that this code was not tested
		if(postImportVertex<pack[0].getNumVertices()) 
		{
			return pack[0].getMesh()->getPreImportVertex(postImportVertex, postImportTriangle);
		} else {
			MultiMeshPreImportNumber preImport = pack[1].getMesh()->getPreImportVertex(postImportVertex-pack[0].getNumVertices(), postImportTriangle-pack[0].getNumTriangles());
			if(preImport==UNDEFINED) return UNDEFINED;
			if(preImport.object>=pack[1].getNumObjects())
			{
				assert(0); // internal error
				return UNDEFINED;
			}
			preImport.object += pack[0].getNumObjects();
			return preImport;
		}
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		assert(0);//!!! just mark that this code was not tested
		MultiMeshPreImportNumber preImportV = preImportVertex;
		MultiMeshPreImportNumber preImportT = preImportTriangle;
		if(preImportV.object<pack[0].getNumObjects()) 
		{
			return pack[0].getMesh()->getPostImportVertex(preImportV, preImportT);
		} else {
			preImportV.object -= pack[0].getNumObjects();
			preImportT.object -= pack[0].getNumObjects();
			if(preImportV.object>=pack[1].getNumObjects()) 
			{
				assert(0); // it is allowed by rules, but also interesting to know when it happens
				return UNDEFINED;
			}
			if(preImportT.object>=pack[1].getNumObjects()) 
			{
				assert(0); // it is allowed by rules, but also interesting to know when it happens
				return UNDEFINED;
			}
			unsigned tmp = pack[1].getMesh()->getPostImportVertex(preImportV, preImportT);
			if(tmp==UNDEFINED) return UNDEFINED;
			return pack[0].getNumVertices() + tmp;
		}
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle<pack[0].getNumTriangles()) 
		{
			return pack[0].getMesh()->getPreImportTriangle(postImportTriangle);
		} else {
			MultiMeshPreImportNumber preImport = pack[1].getMesh()->getPreImportTriangle(postImportTriangle-pack[0].getNumTriangles());
			if(preImport==UNDEFINED) return UNDEFINED;
			if(preImport.object>=pack[1].getNumObjects())
			{
				assert(0); // internal error
				return UNDEFINED;
			}
			preImport.object += pack[0].getNumObjects();
			return preImport;
		}
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		MultiMeshPreImportNumber preImport = preImportTriangle;
		if(preImport.object<pack[0].getNumObjects()) 
		{
			return pack[0].getMesh()->getPostImportTriangle(preImport);
		} else {
			preImport.object -= pack[0].getNumObjects();
			if(preImport.object>=pack[1].getNumObjects()) 
			{
				assert(0); // it is allowed by rules, but also interesting to know when it happens
				return UNDEFINED;
			}
			unsigned tmp = pack[1].getMesh()->getPostImportTriangle(preImport);
			if(tmp==UNDEFINED) 
			{
				return UNDEFINED;
			}
			return pack[0].getNumTriangles() + tmp;
		}
	}

	virtual ~RRMeshMulti()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if(pack[0].getNumObjects()>1) delete pack[0].getMesh();
		if(pack[1].getNumObjects()>1) delete pack[1].getMesh();
	}
private:
	RRMeshMulti(const RRMesh* mesh1, unsigned mesh1Objects, const RRMesh* mesh2, unsigned mesh2Objects)
	{
		pack[0].init(mesh1,mesh1Objects);
		pack[1].init(mesh2,mesh2Objects);
	}
	struct MeshPack
	{
		void init(const RRMesh* importer, unsigned objects)
		{
			packImporter = importer;
			numObjects = objects;
			assert(importer);
			numVertices = importer->getNumVertices();
			numTriangles = importer->getNumTriangles();
		}
		const RRMesh*   getMesh() const {return packImporter;}
		unsigned        getNumObjects() const {return numObjects;}
		unsigned        getNumVertices() const {return numVertices;}
		unsigned        getNumTriangles() const {return numTriangles;}
	private:
		const RRMesh*   packImporter;
		unsigned        numObjects;
		unsigned        numVertices;
		unsigned        numTriangles;
	};
	MeshPack        pack[2];
};

} //namespace
