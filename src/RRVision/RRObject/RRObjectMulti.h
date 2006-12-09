#pragma once

#include <assert.h>
#include "RRVision.h"
namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Merges multiple object importers together.
// Instructions for deleting
//   ObjectImporters typically get underlying Collider as constructor parameter.
//   They are thus not allowed to destroy it (see general rules).
//   MultiObjectImporter creates Collider internally. To behave as others, it doesn't destroy it.
// Limitations:
//   All objects must share one surface numbering.

class RRMultiObjectImporter : public RRObject
{
public:
	static RRObject* create(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation)
	{
		if(!numObjects) return NULL;
		// only in top level of hierarchy: create multicollider
		RRCollider* multiCollider = NULL;
		RRMesh** transformedMeshes = NULL;
		// optimalizace: multimesh z 1 objektu = objekt samotny
		// lze aplikovat jen pokud se nestitchuji vertexy
		// pokud se stitchuji, musi vse projit standardni multi-cestou
		if(numObjects>1 || maxStitchDistance>=0 || optimizeTriangles)
		{
			// create multimesh
			transformedMeshes = new RRMesh*[numObjects+3];
				//!!! pri getWorldMatrix()==NULL by se misto WorldSpaceMeshe mohl pouzit original a pak ho neuvolnovat
			for(unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			transformedMeshes[numObjects] = NULL;
			transformedMeshes[numObjects+1] = NULL;
			transformedMeshes[numObjects+2] = NULL;
			RRMesh* multiMesh = RRMesh::createMultiMesh(transformedMeshes,numObjects);

			// NOW: multiMesh is unoptimized = concatenated meshes
			// kdyz jsou zaple tyto optimalizace, pristup k objektu je pomalejsi,
			//  protoze je nutne preindexovavat analogicky k obecne optimalizaci v meshi
			//!!! kdyz jsou zaple tyto optimalizace, "fcss koupelna" gcc hodi assert u m3ds v getTriangleSurface,
			//    moc velky trianglIndex. kdyz ho ignoruju, crashne. v msvc se nepodarilo navodit.
			// stitch vertices
			if(maxStitchDistance>=0)
			{
				transformedMeshes[numObjects] = multiMesh; // remember for freeing time
				multiMesh = multiMesh->createOptimizedVertices(maxStitchDistance);
			}
			// remove degenerated triangles
			if(optimizeTriangles)
			{
				transformedMeshes[numObjects+2] = multiMesh; // remember for freeing time
				multiMesh = multiMesh->createOptimizedTriangles();
			}
			// NOW: multiMesh is optimized, object indexing must be optimized too via calls to unoptimizeTriangle()

			// create copy (faster access)
			// disabled because we know that current copy implementation always gives up
			// due to low efficiency
			if(0)
			{
				RRMesh* tmp = multiMesh->createCopy();
				if(tmp)
				{
					transformedMeshes[numObjects+1] = multiMesh; // remember for freeing time
					multiMesh = tmp;
				}
			}

			// create multicollider
			multiCollider = RRCollider::create(multiMesh,intersectTechnique,cacheLocation);
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
		if(!transformedMeshes) return;
		unsigned numObjects = pack[0].getNumObjects()+pack[1].getNumObjects();
		RRMesh* unoptimizedMesh;
		if(transformedMeshes[numObjects]) unoptimizedMesh = transformedMeshes[numObjects]; else
			if(transformedMeshes[numObjects+2]) unoptimizedMesh = transformedMeshes[numObjects+2]; else
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
		if(!transformedMeshes) return;
		unsigned numObjects = pack[0].getNumObjects()+pack[1].getNumObjects();
		RRMesh* unoptimizedMesh;
		if(transformedMeshes[numObjects]) unoptimizedMesh = transformedMeshes[numObjects]; else
			if(transformedMeshes[numObjects+2]) unoptimizedMesh = transformedMeshes[numObjects+2]; else
				return;
		// <unoptimized> is mesh after concatenation of multiple meshes/objects
		// <this> is after concatenation and optimizations
		// convert t from "post" to "pre"
		t = getCollider()->getMesh()->getPreImportTriangle(t);
		// convert t from "pre" to "mid"
		t = unoptimizedMesh->getPostImportTriangle(t);
	}

	virtual unsigned getTriangleSurface(unsigned t) const
	{
		unoptimizeTriangle(t);
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleSurface(t);
		return pack[1].getImporter()->getTriangleSurface(t-pack[0].getNumTriangles());
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		//!!! assumption: all objects share the same surface library
		return pack[0].getImporter()->getSurface(s);
	}

	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		unoptimizeTriangle(t);
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleNormals(t,out);
		return pack[1].getImporter()->getTriangleNormals(t-pack[0].getNumTriangles(),out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		unoptimizeTriangle(t);
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleMapping(t,out);
		return pack[1].getImporter()->getTriangleMapping(t-pack[0].getNumTriangles(),out);
	}

	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure format, RRColor& out) const
	{
		unoptimizeTriangle(t);
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleIllumination(t,format,out);
		return pack[1].getImporter()->getTriangleIllumination(t-pack[0].getNumTriangles(),format,out);
	}

	virtual ~RRMultiObjectImporter()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if(pack[0].getNumObjects()>1) delete pack[0].getImporter();
		if(pack[1].getNumObjects()>1) delete pack[1].getImporter();
		// Only for top level of tree:
		if(multiCollider) 
		{
			// Delete multiMesh importer created by us.
			delete multiCollider->getMesh();
			// Delete multiCollider created by us.
			delete multiCollider;
			// Delete transformers created by us.
			unsigned numObjects = pack[0].getNumObjects() + pack[1].getNumObjects();
			for(unsigned i=0;i<numObjects+3;i++) delete transformedMeshes[i];
			delete[] transformedMeshes;
		}
	}

private:
	static RRObject* create(RRObject* const* objects, unsigned numObjects, RRCollider* multiCollider = NULL, RRMesh** transformedMeshes = NULL)
		// All parameters (meshes, array of meshes) are destructed by caller, not by us.
		// Array of meshes must live during this call.
		// Meshes must live as long as created multimesh.
	{
		switch(numObjects)
		{
		case 0: 
			return NULL;
		case 1: 
			assert(objects);
			//  pokud nemame externe narizeny multiCollider, vratime hned jediny objekt, objects[0]
			if(!multiCollider) return objects[0]; 
		// pozor, return objects[0]; nestaci v pripade ze vytvarime multiObject z 1 objektu (objects[0])
		//  a mame externe narizeny multiCollider
		//  ignorovaly by se totiz zmeny provedene v objects[0] a nam dodane v multiCollideru (konkretne jde o vertex stitching)
		//  musime vracet vse jako object[0], ale misto jeho collideru pouzit multiCollider
		//  toto za nas s nepatrne snizenou efektivitou zaridi default vetev
		// zde umyslne neni break, pokracujeme do defaultu
		default: 
			assert(objects); 
			unsigned num1 = (numObjects+1)/2; // pokud numObjects==1, musi vyjit num1=1, num2=0 (num1 nikdy nesmi byt 0 kvuli getSurface)
			unsigned num2 = numObjects-num1;
			unsigned tris[2] = {0,0};
			for(unsigned i=0;i<numObjects;i++) 
			{
				assert(objects[i]);
				assert(objects[i]->getCollider());
				assert(objects[i]->getCollider()->getMesh());
				tris[(i<num1)?0:1] += objects[i]->getCollider()->getMesh()->getNumTriangles();
			}

			// create multiobject
			return new RRMultiObjectImporter(
				create(objects,num1),num1,tris[0],
				create(objects+num1,num2),num2,tris[1],
				multiCollider,transformedMeshes);
		}
	}

	RRMultiObjectImporter(RRObject* mesh1, unsigned mesh1Objects, unsigned mesh1Triangles, 
		RRObject* mesh2, unsigned mesh2Objects, unsigned mesh2Triangles,
		RRCollider* amultiCollider, RRMesh** atransformedMeshes)
	{
		multiCollider = amultiCollider;
		transformedMeshes = atransformedMeshes;
		pack[0].init(mesh1,mesh1Objects,mesh1Triangles);
		pack[1].init(mesh2,mesh2Objects,mesh2Triangles);
	}

	struct ObjectPack
	{
		void init(RRObject* importer, unsigned objects, unsigned triangles)
		{
			assert(numObjects);
			packImporter = importer;
			numObjects = objects;
			numTriangles = triangles;
		}
		RRObject* getImporter() const {return packImporter;}
		unsigned  getNumObjects() const {return numObjects;}
		unsigned  getNumTriangles() const {return numTriangles;}
	private:
		RRObject* packImporter;
		unsigned  numObjects;
		unsigned  numTriangles;
	};

	ObjectPack    pack[2];
	RRCollider*   multiCollider;
	RRMesh**      transformedMeshes;
};

}; // namespace
