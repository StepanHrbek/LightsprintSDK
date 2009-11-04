// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include <cassert>
#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMultiFast
//
// Merges multiple object importers together.
// Instructions for deleting
//   ObjectImporters typically get underlying Collider as constructor parameter.
//   They are thus not allowed to destroy it (see general rules).
//   MultiObjectImporter creates Collider internally. To behave as others, it doesn't destroy it.

class RRObjectMultiFast : public RRObject
{
public:
	static RRObject* create(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, bool accelerate, const char* cacheLocation)
	{
		if (!objects || !numObjects) return NULL;
		// only in top level of hierarchy: create multicollider
		const RRCollider* multiCollider = NULL;
		const RRMesh** transformedMeshes = NULL;
		bool vertexStitching = maxDistanceBetweenVerticesToStitch>=0 && maxRadiansBetweenNormalsToStitch>=0;
		if (numObjects>1 || vertexStitching || optimizeTriangles)
		{
			// create multimesh
			transformedMeshes = new const RRMesh*[numObjects+MI_MAX];
				//!!! pri getWorldMatrix()==NULL by se misto WorldSpaceMeshe mohl pouzit original a pak ho neuvolnovat
			for (unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			for (unsigned i=0;i<MI_MAX;i++) transformedMeshes[numObjects+i] = NULL;

			const RRMesh* oldMesh = transformedMeshes[0];
			const RRMesh* multiMesh = RRMesh::createMultiMesh(transformedMeshes,numObjects,true);
			if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_MULTI] = multiMesh; // remember for freeing time

			// NOW: multiMesh is unoptimized = concatenated meshes
			// stitch vertices
			if (vertexStitching && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createOptimizedVertices(maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch);
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_OPTI_VERTICES] = multiMesh; // remember for freeing time
			}
			// remove degenerated triangles
			if (optimizeTriangles && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createOptimizedTriangles();
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_OPTI_TRIANGLES] = multiMesh; // remember for freeing time
			}
			// accelerate (saves time but needs more memory)
			if (accelerate && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createAccelerated();
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_ACCELERATED] = multiMesh; // remember for freeing time
			}
			// NOW: multiMesh is optimized, object indexing must be optimized too via calls to unoptimizeTriangle()


			// create multicollider
			multiCollider = RRCollider::create(multiMesh,intersectTechnique,aborting,cacheLocation);

			if (!multiCollider)
			{
				// not enough memory
				for (unsigned i=0;i<numObjects+MI_MAX;i++) delete transformedMeshes[i];
				delete[] transformedMeshes;
				return NULL;
			}
		}

		// creates tree of objects
		return new RRObjectMultiFast(objects,numObjects,multiCollider,transformedMeshes);
	}

	virtual const RRCollider* getCollider() const
	{
		return multiCollider;
	}

	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		RRMesh::PreImportNumber mid = postImportToMidImportTriangle[t];
		return singles[mid.object].object->getTriangleMaterial(mid.index,light,receiver);
	}

	virtual void getPointMaterial(unsigned t, RRVec2 uv,RRMaterial& out, const RRScaler* scaler = NULL) const
	{
		RRMesh::PreImportNumber mid = postImportToMidImportTriangle[t];
		singles[mid.object].object->getPointMaterial(mid.index,uv,out,scaler);
	}

	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		RRMesh::PreImportNumber mid = postImportToMidImportTriangle[t];
		singles[mid.object].object->getTriangleLod(mid.index,out);
	}

	virtual ~RRObjectMultiFast()
	{
		//delete[] postImportToMidImportVertex;
		delete[] postImportToMidImportTriangle;
		delete multiCollider;
		for (unsigned i=0;i<numSingles+MI_MAX;i++) delete transformedMeshes[i];
		delete[] transformedMeshes;
	}

private:
	RRObjectMultiFast(RRObject* const* _objects, unsigned _numObjects, const RRCollider* _multiCollider = NULL, const RRMesh** _transformedMeshes = NULL)
		// All parameters (meshes, array of meshes) are destructed by caller, not by us.
		// Array of meshes must live during this call.
		// Meshes must live as long as created multimesh.
	{
		RR_ASSERT(_objects);
		RR_ASSERT(_numObjects);
		RR_ASSERT(_multiCollider);
		numSingles = _numObjects;
		singles = new Single[numSingles];
		for (unsigned i=0;i<numSingles;i++)
		{
			singles[i].object = _objects[i];
			//singles[i].numTrianglesBefore = numTrianglesMulti;
			//singles[i].numVerticesBefore = numVerticesMulti;
			//numTrianglesMulti += _meshes[i] ? _meshes[i]->getNumTriangles() : 0;
			//numVerticesMulti += _meshes[i] ? _meshes[i]->getNumVertices() : 0;
		}
		multiCollider = _multiCollider;
		transformedMeshes = _transformedMeshes;
		const RRMesh* multiMeshOptimized = multiCollider->getMesh();
		numTrianglesOptimized = multiMeshOptimized->getNumTriangles();
		numVerticesOptimized = multiMeshOptimized->getNumVertices();
		postImportToMidImportTriangle = new RRMesh::PreImportNumber[numTrianglesOptimized];
		for (unsigned t=0;t<numTrianglesOptimized;t++)
		{
			RRMesh::PreImportNumber preImportT = multiMeshOptimized->getPreImportTriangle(t);
			preImportT.index = transformedMeshes[preImportT.object]->getPostImportTriangle(RRMesh::PreImportNumber(0,preImportT.index));
			postImportToMidImportTriangle[t] = preImportT;
		}
		//postImportToMidImportVertex = new RRMesh::PreImportNumber[numVerticesOptimized];
		//for (unsigned v=0;v<numVerticesOptimized;v++)
		//{
		//	RRMesh::PreImportNumber preImportT = multiMeshOptimized->getPreImportTriangle(t);
		//	RRMesh::PreImportNumber preImportV = multiMeshOptimized->getPreImportVertex(v);
		//	preImportV.index = transformedMeshes[preImportV.object]->getPostImportVertex(preImportV.index);
		//	postImportToMidImportVertex[v] = preImportV;
		//}
	}

	enum MeshIndex // for access to helper meshes transformedMeshes[numObjects+MeshIndex]
	{
		MI_MULTI = 0,
		MI_OPTI_VERTICES,
		MI_OPTI_TRIANGLES,
		MI_ACCELERATED,
		MI_MAX
	};

	struct Single
	{
		RRObject* object;
	};
	Single* singles;
	unsigned numSingles;
	unsigned numTrianglesOptimized;
	unsigned numVerticesOptimized;
	RRMesh::PreImportNumber* postImportToMidImportTriangle;
	//RRMesh::PreImportNumber* postImportToMidImportVertex;

	const RRCollider* multiCollider;
	const RRMesh** transformedMeshes;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMultiSmall
//
// Instructions for deleting
//   ObjectImporters typically get underlying Collider as constructor parameter.
//   They are thus not allowed to destroy it (see general rules).
//   MultiObjectImporter creates Collider internally. To behave as others, it doesn't destroy it.

class RRObjectMultiSmall : public RRObject
{
public:
	static RRObject* create(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, bool accelerate, const char* cacheLocation)
	{
		if (!objects || !numObjects) return NULL;
		// only in top level of hierarchy: create multicollider
		const RRCollider* multiCollider = NULL;
		const RRMesh** transformedMeshes = NULL;
		bool vertexStitching = maxDistanceBetweenVerticesToStitch>=0 && maxRadiansBetweenNormalsToStitch>=0;
		if (numObjects>1 || vertexStitching || optimizeTriangles)
		{
			// create multimesh
			transformedMeshes = new const RRMesh*[numObjects+MI_MAX];
			for (unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			for (unsigned i=0;i<MI_MAX;i++) transformedMeshes[numObjects+i] = NULL;

			const RRMesh* oldMesh = transformedMeshes[0];
			const RRMesh* multiMesh = RRMesh::createMultiMesh(transformedMeshes,numObjects,false);
			if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_MULTI] = multiMesh; // remember for freeing time

			// NOW: multiMesh is unoptimized = concatenated meshes
			// stitch vertices
			if (vertexStitching && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createOptimizedVertices(maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch);
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_OPTI_VERTICES] = multiMesh; // remember for freeing time
			}
			// remove degenerated triangles
			if (optimizeTriangles && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createOptimizedTriangles();
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_OPTI_TRIANGLES] = multiMesh; // remember for freeing time
			}
			// accelerate (saves time but needs more memory)
			if (accelerate && !aborting)
			{
				oldMesh = multiMesh;
				multiMesh = multiMesh->createAccelerated();
				if (multiMesh!=oldMesh) transformedMeshes[numObjects+MI_ACCELERATED] = multiMesh; // remember for freeing time
			}
			// NOW: multiMesh is optimized, object indexing must be optimized too via calls to unoptimizeTriangle()

			// create copy (faster access)
			// disabled because we know that current copy implementation always gives up
			// due to low efficiency
			/*if (0)
			{
				RRMesh* tmp = multiMesh->createCopy();
				if (tmp)
				{
					transformedMeshes[numObjects+x] = multiMesh; // remember for freeing time
					multiMesh = tmp;
				}
			}*/

			// create multicollider
			multiCollider = RRCollider::create(multiMesh,intersectTechnique,aborting,cacheLocation);

			if (!multiCollider)
			{
				// not enough memory
				for (unsigned i=0;i<numObjects+MI_MAX;i++) delete transformedMeshes[i];
				delete[] transformedMeshes;
				return NULL;
			}
		}

		// creates tree of objects
		return create(objects,numObjects,multiCollider,transformedMeshes);
	}

	virtual const RRCollider* getCollider() const
	{
		return multiCollider;
	}

	/*void unoptimizeVertex(unsigned& v) const
	{
		if (!transformedMeshes) return;
		unsigned numObjects = pack[0].getNumObjects()+pack[1].getNumObjects();
		RRMesh* unoptimizedMesh;
		if (transformedMeshes[numObjects+1]) unoptimizedMesh = transformedMeshes[numObjects+1]; else
			if (transformedMeshes[numObjects+2]) unoptimizedMesh = transformedMeshes[numObjects+2]; else
				return;
		// <unoptimized> is mesh after concatenation of multiple meshes/objects
		// <this> is after concatenation and optimizations
		v = getCollider()->getMesh()->getPreImportVertex(v,?);
		v = unoptimizedMesh->getPostImportVertex(v,?);
	}*/

	// converts t from "post" to "mid"
	// (from "post" after optimizations to "post" of unoptimized mesh)
	void unoptimizeTriangle(unsigned& t) const
	{
		if (!transformedMeshes) return;
		unsigned numObjects = pack[0].getNumObjects()+pack[1].getNumObjects();
		const RRMesh* unoptimizedMesh;
		if (transformedMeshes[numObjects+MI_OPTI_VERTICES]) unoptimizedMesh = transformedMeshes[numObjects+MI_OPTI_VERTICES]; else
			if (transformedMeshes[numObjects+MI_OPTI_VERTICES]) unoptimizedMesh = transformedMeshes[numObjects+MI_OPTI_VERTICES]; else
				if (transformedMeshes[numObjects+MI_ACCELERATED]) unoptimizedMesh = transformedMeshes[numObjects+MI_ACCELERATED]; else
					return;
		// <unoptimized> is mesh after concatenation of multiple meshes/objects
		// <this> is after concatenation and optimizations
		// convert t from "post" to "pre"
		RRMesh::PreImportNumber preImportTriangle = getCollider()->getMesh()->getPreImportTriangle(t);
		// convert t from "pre" to "mid"
		t = unoptimizedMesh->getPostImportTriangle(preImportTriangle);
	}

	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		unoptimizeTriangle(t);
		if (t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleMaterial(t,light,receiver);
		return pack[1].getImporter()->getTriangleMaterial(t-pack[0].getNumTriangles(),light,receiver);
	}

	virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out, const RRScaler* scaler = NULL) const
	{
		unoptimizeTriangle(t);
		if (t<pack[0].getNumTriangles())
			pack[0].getImporter()->getPointMaterial(t,uv,out,scaler);
		else
			pack[1].getImporter()->getPointMaterial(t-pack[0].getNumTriangles(),uv,out,scaler);
	}


	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		unoptimizeTriangle(t);
		if (t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleLod(t,out);
		return pack[1].getImporter()->getTriangleLod(t-pack[0].getNumTriangles(),out);
	}

	virtual ~RRObjectMultiSmall()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if (pack[0].getNumObjects()>1) delete pack[0].getImporter();
		if (pack[1].getNumObjects()>1) delete pack[1].getImporter();
		// Only for top level of tree:
		if (multiCollider) 
		{
			// Delete multiCollider created by us.
			delete multiCollider;
			// Delete transformers created by us.
			unsigned numObjects = pack[0].getNumObjects() + pack[1].getNumObjects();
			RR_ASSERT(transformedMeshes[0]!=transformedMeshes[1]);
			for (unsigned i=0;i<numObjects+MI_MAX;i++) delete transformedMeshes[i];
			delete[] transformedMeshes;
		}
	}

private:
	static RRObject* create(RRObject* const* objects, unsigned numObjects, const RRCollider* multiCollider = NULL, const RRMesh** transformedMeshes = NULL)
		// All parameters (meshes, array of meshes) are destructed by caller, not by us.
		// Array of meshes must live during this call.
		// Meshes must live as long as created multimesh.
	{
		switch(numObjects)
		{
		case 0: 
			return NULL;
		case 1: 
			RR_ASSERT(objects);
			if (!multiCollider) return objects[0]; 
		// intentionally no break, continue to default
		default: 
			RR_ASSERT(objects); 
			unsigned num1 = (numObjects+1)/2;
			unsigned num2 = numObjects-num1;
			unsigned tris[2] = {0,0};
			for (unsigned i=0;i<numObjects;i++) 
			{
				RR_ASSERT(!objects[i] || (objects[i]->getCollider() && objects[i]->getCollider()->getMesh()));
				tris[(i<num1)?0:1] += objects[i] ? objects[i]->getCollider()->getMesh()->getNumTriangles() : 0;
			}

			// create multiobject
			return new RRObjectMultiSmall(
				create(objects,num1),num1,tris[0],
				create(objects+num1,num2),num2,tris[1],
				multiCollider,transformedMeshes);
		}
	}

	RRObjectMultiSmall(RRObject* mesh1, unsigned mesh1Objects, unsigned mesh1Triangles, 
		RRObject* mesh2, unsigned mesh2Objects, unsigned mesh2Triangles,
		const RRCollider* amultiCollider, const RRMesh** atransformedMeshes)
	{
		multiCollider = amultiCollider;
		transformedMeshes = atransformedMeshes;
		pack[0].init(mesh1,mesh1Objects,mesh1Triangles);
		pack[1].init(mesh2,mesh2Objects,mesh2Triangles);
	}

	struct ObjectPack
	{
		void init(RRObject* _importer, unsigned _numObjects, unsigned _numTriangles)
		{
			//RR_ASSERT(_numObjects); this is legal in case of multiobject from 1 object, with vertex weld enabled
			packImporter = _importer;
			numObjects = _numObjects;
			numTriangles = _numTriangles;
		}
		RRObject* getImporter() const {return packImporter;}
		unsigned  getNumObjects() const {return numObjects;}
		unsigned  getNumTriangles() const {return numTriangles;}
	private:
		RRObject* packImporter;
		unsigned  numObjects;
		unsigned  numTriangles;
	};

	enum MeshIndex // for access to helper meshes transformedMeshes[numObjects+MeshIndex]
	{
		MI_MULTI = 0,
		MI_OPTI_VERTICES,
		MI_OPTI_TRIANGLES,
		MI_ACCELERATED,
		MI_MAX
	};

	ObjectPack    pack[2];
	const RRCollider* multiCollider;
	const RRMesh** transformedMeshes;
};

}; // namespace
