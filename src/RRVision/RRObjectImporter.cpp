#include <assert.h>

#include "../MeshImporter/Filter.h"
#include "rrcore.h"
#include "RRVision.h"

namespace rrVision
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter

void RRObjectImporter::getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
{
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
}

void RRObjectImporter::getTriangleNormals(unsigned t, TriangleNormals& out)
{
	unsigned numTriangles = getCollider()->getImporter()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}
	rrCollider::RRMeshImporter::TriangleBody tb;
	getCollider()->getImporter()->getTriangleBody(t,tb);
	Vec3 norm = ortogonalTo(tb.side1,tb.side2);
	norm *= 1/size(norm);
	out.norm[0] = norm;
	out.norm[1] = norm;
	out.norm[2] = norm;
}

void RRObjectImporter::getTriangleMapping(unsigned t, TriangleMapping& out)
{
	unsigned numTriangles = getCollider()->getImporter()->getNumTriangles();
	if(t>=numTriangles)
	{
		assert(0);
		return;
	}
	out.uv[0][0] = 1.0f*t/numTriangles;
	out.uv[0][1] = 0;
	out.uv[1][0] = 1.0f*(t+1)/numTriangles;
	out.uv[1][1] = 0;
	out.uv[2][0] = 1.0f*t/numTriangles;
	out.uv[2][1] = 1;
}

const RRMatrix3x4* RRObjectImporter::getWorldMatrix()
{
	return NULL;
}

const RRMatrix3x4* RRObjectImporter::getInvWorldMatrix()
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshFilter : public rrCollider::RRMeshFilter
{
public:
	RRTransformedMeshFilter(RRMeshImporter* mesh, const RRMatrix3x4* matrix)
		: RRMeshFilter(mesh)
	{
		m = matrix;
	}
	/*RRTransformedMeshFilter(RRObjectImporter* object)
	: RRMeshFilter(object->getCollider()->getImporter())
	{
	m = object->getWorldMatrix();
	}*/

	virtual void getVertex(unsigned v, Vertex& out) const
	{
		importer->getVertex(v,out);
		if(m)
		{
			m->transformPosition(out);
		}
	}

private:
	const RRMatrix3x4* m;
};

//////////////////////////////////////////////////////////////////////////////
//
// Merges multiple object importers together.
// Instructions for deleting
//   ObjectImporters typically get underlying Collider as constructor parameter.
//   They are thus not allowed to destroy it (see general rules).
//   MultiObjectImporter creates Collider internally. To behave as others, it doesn't destroy it.
// Limitations:
//   All objects must share one surface numbering.

class RRMultiObjectImporter : public RRObjectImporter
{
public:
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation)
	{
		if(!numObjects) return NULL;
		// only in top level of hierarchy: create multicollider
		rrCollider::RRCollider* multiCollider = NULL;
		rrCollider::RRMeshImporter** transformedMeshes = NULL;
		// optimalizace: multimesh z 1 objektu = objekt samotny
		// lze aplikovat jen pokud se nestitchuji vertexy
		// pokud se stitchuji, musi vse projit standardni multi-cestou
		if(numObjects>1 || maxStitchDistance>=0 || optimizeTriangles)
		{
			// create multimesh
			transformedMeshes = new rrCollider::RRMeshImporter*[numObjects+3];
				//!!! pri getWorldMatrix()==NULL by se misto WorldSpaceMeshe mohl pouzit original a pak ho neuvolnovat
			for(unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			transformedMeshes[numObjects] = NULL;
			transformedMeshes[numObjects+1] = NULL;
			transformedMeshes[numObjects+2] = NULL;
			rrCollider::RRMeshImporter* multiMesh = rrCollider::RRMeshImporter::createMultiMesh(transformedMeshes,numObjects);
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
			// create copy (faster access)
			// disabled because we know that current copy implementation always gives up
			// due to low efficiency
			if(0)
			{
				rrCollider::RRMeshImporter* tmp = multiMesh->createCopy();
				if(tmp)
				{
					transformedMeshes[numObjects+1] = multiMesh; // remember for freeing time
					multiMesh = tmp;
				}
			}

			// create multicollider
			multiCollider = rrCollider::RRCollider::create(multiMesh,intersectTechnique,cacheLocation);
		}

		// creates tree of objects
		return create(objects,numObjects,multiCollider,transformedMeshes);
	}

	virtual const rrCollider::RRCollider* getCollider() const
	{
		return multiCollider;
	}

	virtual unsigned     getTriangleSurface(unsigned t) const
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleSurface(t);
		return pack[1].getImporter()->getTriangleSurface(t-pack[0].getNumTriangles());
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		// assumption: all objects share the same surface library
		// -> this is not universal code
		return pack[0].getImporter()->getSurface(s);
	}

	virtual void getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure format, RRColor& out) const
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalMeasure(t,format,out);
		return pack[1].getImporter()->getTriangleAdditionalMeasure(t-pack[0].getNumTriangles(),format,out);
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
			delete multiCollider->getImporter();
			// Delete multiCollider created by us.
			delete multiCollider;
			// Delete transformers created by us.
			unsigned numObjects = pack[0].getNumObjects() + pack[1].getNumObjects();
			for(unsigned i=0;i<numObjects+3;i++) delete transformedMeshes[i];
			delete[] transformedMeshes;
		}
	}

private:
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider* multiCollider = NULL, rrCollider::RRMeshImporter** transformedMeshes = NULL)
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
				assert(objects[i]->getCollider()->getImporter());
				tris[(i<num1)?0:1] += objects[i]->getCollider()->getImporter()->getNumTriangles();
			}

			// create multiobject
			return new RRMultiObjectImporter(
				create(objects,num1),num1,tris[0],
				create(objects+num1,num2),num2,tris[1],
				multiCollider,transformedMeshes);
		}
	}

	RRMultiObjectImporter(RRObjectImporter* mesh1, unsigned mesh1Objects, unsigned mesh1Triangles, 
		RRObjectImporter* mesh2, unsigned mesh2Objects, unsigned mesh2Triangles,
		rrCollider::RRCollider* amultiCollider, rrCollider::RRMeshImporter** atransformedMeshes)
	{
		multiCollider = amultiCollider;
		transformedMeshes = atransformedMeshes;
		pack[0].init(mesh1,mesh1Objects,mesh1Triangles);
		pack[1].init(mesh2,mesh2Objects,mesh2Triangles);
	}

	struct ObjectPack
	{
		void init(RRObjectImporter* importer, unsigned objects, unsigned triangles)
		{
			assert(numObjects);
			packImporter = importer;
			numObjects = objects;
			numTriangles = triangles;
		}
		RRObjectImporter* getImporter() const {return packImporter;}
		unsigned          getNumObjects() const {return numObjects;}
		unsigned          getNumTriangles() const {return numTriangles;}
	private:
		RRObjectImporter* packImporter;
		unsigned          numObjects;
		unsigned          numTriangles;
	};

	ObjectPack        pack[2];
	rrCollider::RRCollider* multiCollider;
	rrCollider::RRMeshImporter** transformedMeshes;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter instance factory

rrCollider::RRMeshImporter* RRObjectImporter::createWorldSpaceMesh()
{
	return new RRTransformedMeshFilter(getCollider()->getImporter(),getWorldMatrix());
}

RRObjectImporter* RRObjectImporter::createMultiObject(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,maxStitchDistance,optimizeTriangles,cacheLocation);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRAdditionalObjectImporter
/*
vision zajima exiting flux
renderer s texturou zajima irradiance, renderer bez textury vyjimecne exitance

fast pocita incident flux
slow pocita exitance

nekonzistence vznika kdyz u cerneho materialu ulozim outgoing a ptam se na incoming
-> ukladat incoming
nekonzistance vznika kdyz u degenerata ulozim flux a ptam se na irradiance
-> ukladat irradiance

RRMyAdditionalObjectImporter tedy vse co dostane prevede na irradiance
*/

class RRMyAdditionalObjectImporter : public RRAdditionalObjectImporter
{
public:
	RRMyAdditionalObjectImporter(RRObjectImporter* aoriginal)
	{
		original = aoriginal;
		assert(original);
		assert(getCollider());
		assert(getCollider()->getImporter());
		numTriangles = getCollider()->getImporter()->getNumTriangles();
		triangleInfo = new TriangleInfo[numTriangles];
		for(unsigned i=0;i<numTriangles;i++)
		{
			triangleInfo[i].irradiance[0] = 0;
			triangleInfo[i].irradiance[1] = 0;
			triangleInfo[i].irradiance[2] = 0;
			triangleInfo[i].area = getCollider()->getImporter()->getTriangleArea(i);
		}
	}
	virtual ~RRMyAdditionalObjectImporter() 
	{
		delete[] triangleInfo;
	}
	virtual bool setTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor power)
	{
		if(t>=numTriangles)
		{
			assert(0);
			return false;
		}
		switch(measure)
		{
		case RM_INCIDENT_FLUX:
			triangleInfo[t].irradiance = triangleInfo[t].area ? power / triangleInfo[t].area : RRColor(0);
			break;
		case RM_IRRADIANCE:
			triangleInfo[t].irradiance = power;
			break;
		case RM_EXITING_FLUX:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				return false;
			}
			//triangleInfo[t].irradiance = power / triangleInfo[t].area / s->diffuseReflectance;
			for(unsigned c=0;c<3;c++)
				triangleInfo[t].irradiance[c] = (triangleInfo[t].area && s->diffuseReflectance[c]) ? power[c] / triangleInfo[t].area / s->diffuseReflectance[c] : 0;
			break;
			}
		case RM_EXITANCE:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				return false;
			}
			//triangleInfo[t].irradiance = power / s->diffuseReflectance;
			for(unsigned c=0;c<3;c++)
				triangleInfo[t].irradiance[c] = (s->diffuseReflectance[c]) ? power[c] / s->diffuseReflectance[c] : 0;
			break;
			}
		default:
			assert(0);
			return false;
		}
		return true;
	}
	virtual void getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		if(t>=numTriangles)
		{
			assert(0);
			out = RRColor(0);
			return;
		}
		switch(measure)
		{
		case RM_INCIDENT_FLUX:
			out = triangleInfo[t].irradiance * triangleInfo[t].area;
			break;
		case RM_IRRADIANCE:
			out = triangleInfo[t].irradiance;
			break;
		case RM_EXITING_FLUX:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				out = RRColor(0);
				return;
			}
			out = triangleInfo[t].irradiance * triangleInfo[t].area * s->diffuseReflectance;
			break;
			}
		case RM_EXITANCE:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				out = RRColor(0);
				return;
			}
			out = triangleInfo[t].irradiance * s->diffuseReflectance;
			break;
			}
		default:
			assert(0);
		}
	}

	// filter
	virtual const rrCollider::RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual unsigned getTriangleSurface(unsigned t) const
	{
		return original->getTriangleSurface(t);
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		return original->getSurface(s);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out)
	{
		return original->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out)
	{
		return original->getTriangleMapping(t,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return original->getWorldMatrix();
	}
	virtual const RRMatrix3x4* getInvWorldMatrix()
	{
		return original->getInvWorldMatrix();
	}

private:
	struct TriangleInfo
	{
		RRColor irradiance;
		RRReal area;
	};
	RRObjectImporter* original;
	unsigned numTriangles;
	TriangleInfo* triangleInfo;
};

RRAdditionalObjectImporter* RRObjectImporter::createAdditionalIllumination()
{
	return new RRMyAdditionalObjectImporter(this);
}

} // namespace
