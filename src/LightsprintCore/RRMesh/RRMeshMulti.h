// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRMesh.h"
#include <cassert>


namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshMultiFast
//
// Merges multiple mesh adapters together.
// Space is not transformed here, underlying meshes must already share one space.
// Fast = uses 8byte/triangle lookup table.

class RRMeshMultiFast : public RRMesh
{
public:
	// creators
	static RRMesh* create(RRMesh* const* mesh, unsigned numMeshes)
		// all parameters (meshes, array of meshes) are destructed by caller, not by us
		// array of meshes must live during this call
		// meshes must live as long as created multimesh
	{
		if(!mesh)
		{
			RR_ASSERT(0);
			return NULL;
		}
		switch(numMeshes)
		{
			case 0: 
				return NULL;
			case 1: 
				return mesh[0];
			default:
				return new RRMeshMultiFast(mesh,numMeshes);
		}
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		// All objects have the same channels, so let's simply ask object[0].
		// Equality must be ensured by creator of multiobject.
		//!!! check equality at construction time
		unsigned itemSizeLocal = 0;
		singles[0].mesh->getChannelSize(channelId,numItems,&itemSizeLocal);
		if(itemSize) *itemSize = itemSizeLocal;
		// Now we know object[0] properties.
		// But whole multiobject has more objects, we must correct *numItems.
		// If channels exists...
		if(itemSizeLocal)
		{
			// ...let's skip adding all objects, use known sum.
			switch(channelId&0x7ffff000)
			{
				case RRMesh::INDEXED_BY_VERTEX:
					*numItems = numVerticesMulti;
					break;
				case RRMesh::INDEXED_BY_TRIANGLE:
					*numItems = numTrianglesMulti;
					break;
				case RRMesh::INDEXED_BY_OBJECT:
					*numItems = numSingles;
					break;
			}
		}
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		switch(channelId&0x7ffff000)
		{
			case INDEXED_BY_VERTEX:
				RR_ASSERT(itemIndex<numVerticesMulti);
				RR_ASSERT(postImportToMidImportVertex[itemIndex].object<numSingles);
				return singles[postImportToMidImportVertex[itemIndex].object].mesh->getChannelData(channelId,postImportToMidImportVertex[itemIndex].index,itemData,itemSize);
			case INDEXED_BY_TRIANGLE:
				RR_ASSERT(itemIndex<numTrianglesMulti);
				RR_ASSERT(postImportToMidImportTriangle[itemIndex].object<numSingles);
				return singles[postImportToMidImportTriangle[itemIndex].object].mesh->getChannelData(channelId,postImportToMidImportTriangle[itemIndex].index,itemData,itemSize);
			case INDEXED_BY_OBJECT:
				RR_ASSERT(itemIndex<numSingles);
				return singles[itemIndex].mesh->getChannelData(channelId,0,itemData,itemSize);
			default:
				return false;
		}
	}

	// vertices
	virtual unsigned     getNumVertices() const
	{
		return numVerticesMulti;
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		RR_ASSERT(v<numVerticesMulti);
		RR_ASSERT(postImportToMidImportVertex[v].object<numSingles);
		singles[postImportToMidImportVertex[v].object].mesh->getVertex(postImportToMidImportVertex[v].index,out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return numTrianglesMulti;
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		RR_ASSERT(t<numTrianglesMulti);
		RR_ASSERT(postImportToMidImportTriangle[t].object<numSingles);
		Single* single = &singles[postImportToMidImportTriangle[t].object];
		single->mesh->getTriangle(postImportToMidImportTriangle[t].index,out);
		out[0] += single->numVerticesBefore;
		out[1] += single->numVerticesBefore;
		out[2] += single->numVerticesBefore;
	}

	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		RR_ASSERT(t<numTrianglesMulti);
		RR_ASSERT(postImportToMidImportTriangle[t].object<numSingles);
		singles[postImportToMidImportTriangle[t].object].mesh->getTriangleNormals(postImportToMidImportTriangle[t].index,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		RR_ASSERT(t<numTrianglesMulti);
		RR_ASSERT(postImportToMidImportTriangle[t].object<numSingles);
		singles[postImportToMidImportTriangle[t].object].mesh->getTriangleMapping(postImportToMidImportTriangle[t].index,out);
		// warning: all mappings overlap
	}

	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		RR_ASSERT(postImportTriangle<numTrianglesMulti);
		RR_ASSERT(postImportVertex<numVerticesMulti);
		RR_ASSERT(postImportToMidImportVertex[postImportVertex].object<numSingles);
		PreImportNumber preImportSingle = singles[postImportToMidImportVertex[postImportVertex].object].mesh->getPreImportVertex(postImportToMidImportVertex[postImportVertex].index,postImportToMidImportTriangle[postImportTriangle].index);
		if(preImportSingle.index==UNDEFINED) return PreImportNumber(UNDEFINED,UNDEFINED);
		PreImportNumber preImportMulti;
		preImportMulti.object = postImportToMidImportVertex[postImportVertex].object;
		preImportMulti.index = preImportSingle.index;
		return preImportMulti;
	}
	virtual unsigned     getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const 
	{
		PreImportNumber preImportV = preImportVertex;
		PreImportNumber preImportT = preImportTriangle;
		RR_ASSERT(preImportV.object<numSingles);
		RR_ASSERT(preImportT.object<numSingles);
		unsigned midImportVertex = singles[preImportV.object].mesh->getPostImportVertex(PreImportNumber(0,preImportV.index),PreImportNumber(0,preImportT.index));
		if(midImportVertex==UNDEFINED) return UNDEFINED;
		return singles[preImportV.object].numVerticesBefore+midImportVertex;
	}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const
	{
		RR_ASSERT(postImportTriangle<numTrianglesMulti);
		RR_ASSERT(postImportToMidImportTriangle[postImportTriangle].object<numSingles);
		PreImportNumber preImportSingle = singles[postImportToMidImportTriangle[postImportTriangle].object].mesh->getPreImportTriangle(postImportToMidImportTriangle[postImportTriangle].index);
		if(preImportSingle.index==UNDEFINED) return PreImportNumber(UNDEFINED,UNDEFINED);
		PreImportNumber preImportMulti;
		preImportMulti.object = postImportToMidImportTriangle[postImportTriangle].object;
		preImportMulti.index = preImportSingle.index;
		return preImportMulti;
	}
	virtual unsigned     getPostImportTriangle(PreImportNumber preImportTriangle) const
	{
		PreImportNumber preImport = preImportTriangle;
		RR_ASSERT(preImport.object<numSingles);
		unsigned midImportTriangle = singles[preImport.object].mesh->getPostImportTriangle(PreImportNumber(0,preImport.index));
		if(midImportTriangle==UNDEFINED) return UNDEFINED;
		return singles[preImport.object].numTrianglesBefore+midImportTriangle;
	}

	virtual ~RRMeshMultiFast()
	{
		delete[] postImportToMidImportVertex;
		delete[] postImportToMidImportTriangle;
		delete[] singles;
	}
private:
	RRMeshMultiFast(RRMesh* const* _meshes, unsigned _numMeshes)
	{
		RR_ASSERT(_meshes);
		RR_ASSERT(_numMeshes);
		numSingles = _numMeshes;
		singles = new Single[numSingles];
		numTrianglesMulti = 0;
		numVerticesMulti = 0;
		for(unsigned i=0;i<_numMeshes;i++)
		{
			singles[i].mesh = _meshes[i];
			singles[i].numTrianglesBefore = numTrianglesMulti;
			singles[i].numVerticesBefore = numVerticesMulti;
			numTrianglesMulti += _meshes[i] ? _meshes[i]->getNumTriangles() : 0;
			numVerticesMulti += _meshes[i] ? _meshes[i]->getNumVertices() : 0;
		}
		postImportToMidImportTriangle = new PreImportNumber[numTrianglesMulti];
		postImportToMidImportVertex = new PreImportNumber[numVerticesMulti];
		unsigned numTrianglesInserted = 0;
		unsigned numVerticesInserted = 0;
		for(unsigned i=0;i<_numMeshes;i++)
		{
			if(singles[i].mesh)
			{
				unsigned numTrianglesSingle = singles[i].mesh->getNumTriangles();
				for(unsigned j=0;j<numTrianglesSingle;j++)
				{
					postImportToMidImportTriangle[numTrianglesInserted].object = i;
					postImportToMidImportTriangle[numTrianglesInserted].index = j;
					numTrianglesInserted++;
				}
				unsigned numVerticesSingle = singles[i].mesh->getNumVertices();
				for(unsigned j=0;j<numVerticesSingle;j++)
				{
					postImportToMidImportVertex[numVerticesInserted].object = i;
					postImportToMidImportVertex[numVerticesInserted].index = j;
					numVerticesInserted++;
				}
			}
		}
	}
	struct Single
	{
		RRMesh* mesh;
		unsigned numTrianglesBefore;
		unsigned numVerticesBefore;
	};
	Single* singles;
	unsigned numSingles;
	unsigned numTrianglesMulti;
	unsigned numVerticesMulti;
	PreImportNumber* postImportToMidImportTriangle;
	PreImportNumber* postImportToMidImportVertex;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshMultiSmall
//
// Merges multiple mesh adapters together.
// Space is not transformed here, underlying meshes must already share one space.
// Small = doesn't allocate any per-triangle table.

class RRMeshMultiSmall : public RRMesh
{
public:
	// creators
	static RRMesh* create(RRMesh* const* mesh, unsigned numMeshes)
		// all parameters (meshes, array of meshes) are destructed by caller, not by us
		// array of meshes must live during this call
		// meshes must live as long as created multimesh
	{
		if(!mesh)
		{
			RR_ASSERT(0);
			return NULL;
		}
		switch(numMeshes)
		{
			case 0: 
				return NULL;
			case 1: 
				return mesh[0];
			default:
				return new RRMeshMultiSmall(
					create(mesh,numMeshes/2),numMeshes/2,
					create(mesh+numMeshes/2,numMeshes-numMeshes/2),numMeshes-numMeshes/2);
		}
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		// All objects have the same channels, so let's simply ask object[0].
		// Equality must be ensured by creator of multiobject.
		//!!! check equality at construction time
		unsigned itemSizeLocal = 0;
		pack[0].getMesh()->getChannelSize(channelId,numItems,&itemSizeLocal);
		if(itemSize) *itemSize = itemSizeLocal;
		// Now we know object[0] properties.
		// But whole multiobject has more objects, we must correct *numItems.
		// If channels exists...
		if(itemSizeLocal)
		{
			// ...let's skip adding all objects, use known sum.
			switch(channelId&0x7ffff000)
			{
				case RRMesh::INDEXED_BY_VERTEX:
					*numItems = RRMeshMultiSmall::getNumVertices();
					break;
				case RRMesh::INDEXED_BY_TRIANGLE:
					*numItems = RRMeshMultiSmall::getNumTriangles();
					break;
			}
		}
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
			case INDEXED_BY_OBJECT:
				pack0Items = pack[0].getNumObjects();
				break;
			default:
				return false;
		}
		if(itemIndex<pack0Items)
			return pack[0].getMesh()->getChannelData(channelId,itemIndex,itemData,itemSize);
		else
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

	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if(t<pack[0].getNumTriangles()) 
			pack[0].getMesh()->getTriangleNormals(t,out);
		else
			pack[1].getMesh()->getTriangleNormals(t-pack[0].getNumTriangles(),out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		// warning: all mappings overlap
		if(t<pack[0].getNumTriangles()) 
			pack[0].getMesh()->getTriangleMapping(t,out);
		else
			pack[1].getMesh()->getTriangleMapping(t-pack[0].getNumTriangles(),out);
	}

	//!!! default is slow
	//virtual void         getTriangleBody(unsigned i, TriangleBody* t) const
	//{
	//}

	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		if(postImportVertex<pack[0].getNumVertices()) 
		{
			return pack[0].getMesh()->getPreImportVertex(postImportVertex, postImportTriangle);
		} else {
			PreImportNumber preImport = pack[1].getMesh()->getPreImportVertex(postImportVertex-pack[0].getNumVertices(), postImportTriangle-pack[0].getNumTriangles());
			if(preImport.index==UNDEFINED) return PreImportNumber(UNDEFINED,UNDEFINED);
			if(preImport.object>=pack[1].getNumObjects())
			{
				RR_ASSERT(0); // internal error
				return PreImportNumber(UNDEFINED,UNDEFINED);
			}
			preImport.object += pack[0].getNumObjects();
			return preImport;
		}
	}
	virtual unsigned     getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const 
	{
		PreImportNumber preImportV = preImportVertex;
		PreImportNumber preImportT = preImportTriangle;
		if(preImportV.object<pack[0].getNumObjects()) 
		{
			return pack[0].getMesh()->getPostImportVertex(preImportV, preImportT);
		} else {
			preImportV.object -= pack[0].getNumObjects();
			preImportT.object -= pack[0].getNumObjects();
			if(preImportV.object>=pack[1].getNumObjects()) 
			{
				RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
				return UNDEFINED;
			}
			if(preImportT.object>=pack[1].getNumObjects()) 
			{
				RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
				return UNDEFINED;
			}
			unsigned tmp = pack[1].getMesh()->getPostImportVertex(preImportV, preImportT);
			if(tmp==UNDEFINED) return UNDEFINED;
			return pack[0].getNumVertices() + tmp;
		}
	}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle<pack[0].getNumTriangles()) 
		{
			return pack[0].getMesh()->getPreImportTriangle(postImportTriangle);
		} else {
			PreImportNumber preImport = pack[1].getMesh()->getPreImportTriangle(postImportTriangle-pack[0].getNumTriangles());
			if(preImport.index==UNDEFINED) return PreImportNumber(UNDEFINED,UNDEFINED);
			if(preImport.object>=pack[1].getNumObjects())
			{
				RR_ASSERT(0); // internal error
				return PreImportNumber(UNDEFINED,UNDEFINED);
			}
			preImport.object += pack[0].getNumObjects();
			return preImport;
		}
	}
	virtual unsigned     getPostImportTriangle(PreImportNumber preImportTriangle) const 
	{
		PreImportNumber preImport = preImportTriangle;
		if(preImport.object<pack[0].getNumObjects()) 
		{
			return pack[0].getMesh()->getPostImportTriangle(preImport);
		} else {
			preImport.object -= pack[0].getNumObjects();
			if(preImport.object>=pack[1].getNumObjects()) 
			{
				RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
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

	virtual ~RRMeshMultiSmall()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if(pack[0].getNumObjects()>1) delete pack[0].getMesh();
		if(pack[1].getNumObjects()>1) delete pack[1].getMesh();
	}
private:
	RRMeshMultiSmall(const RRMesh* mesh1, unsigned mesh1Objects, const RRMesh* mesh2, unsigned mesh2Objects)
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
			numVertices = importer ? importer->getNumVertices() : 0;
			numTriangles = importer ? importer->getNumTriangles() : 0;
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
