#include <assert.h>

#include "../MeshImporter/Filter.h"
#include "rrcore.h"
#include "RRVision.h"

namespace rrVision
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter

void RRObjectImporter::getTriangleAdditionalPower(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
{
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
}

void RRObjectImporter::getTriangleNormals(unsigned t, TriangleNormals& out)
{
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
	out.uv[0][0] = 1.0f*t/numTriangles;
	out.uv[0][1] = 0;
	out.uv[1][0] = 1.0f*(t+1)/numTriangles;
	out.uv[1][1] = 0;
	out.uv[2][0] = 1.0f*t/numTriangles;
	out.uv[2][1] = 1;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshFilter : public rrCollider::RRMeshFilter
{
public:
	RRTransformedMeshFilter(RRMeshImporter* mesh, const RRMatrix4x4* matrix)
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
			Vertex tmp;
			tmp[0] = m->m[0][0] * out[0] + m->m[1][0] * out[1] + m->m[2][0] * out[2] + m->m[3][0];
			tmp[1] = m->m[0][1] * out[0] + m->m[1][1] * out[1] + m->m[2][1] * out[2] + m->m[3][1];
			tmp[2] = m->m[0][2] * out[0] + m->m[1][2] * out[1] + m->m[2][2] * out[2] + m->m[3][2];
			out = tmp;
		}
	}

private:
	const RRMatrix4x4* m;
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
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, char* cacheLocation)
	{
		// only in top level of hierarchy: create multicollider
		rrCollider::RRCollider* multiCollider = NULL;
		rrCollider::RRMeshImporter** transformedMeshes = NULL;
		if(numObjects>1)
		{
			// create multimesh
			transformedMeshes = new rrCollider::RRMeshImporter*[numObjects+2];
				//!!! pri getWorldMatrix()==NULL by se misto WorldSpaceMeshe mohl pouzit original a pak ho neuvolnovat
			for(unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			transformedMeshes[numObjects] = NULL;
			transformedMeshes[numObjects+1] = NULL;
			rrCollider::RRMeshImporter* multiMesh = rrCollider::RRMeshImporter::createMultiMesh(transformedMeshes,numObjects);
			// stitch vertices
			if(maxStitchDistance>=0) 
			{
				transformedMeshes[numObjects] = multiMesh; // remember for freeing time
				multiMesh = multiMesh->createOptimizedVertices(maxStitchDistance);
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

	virtual void getTriangleAdditionalPower(unsigned t, RRRadiometricMeasure format, RRColor& out) const
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalPower(t,format,out);
		return pack[1].getImporter()->getTriangleAdditionalPower(t-pack[0].getNumTriangles(),format,out);
	}

	virtual const RRMatrix4x4* getWorldMatrix()
	{
		return NULL;
	}
	virtual const RRMatrix4x4* getInvWorldMatrix()
	{
		return NULL;
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
			for(unsigned i=0;i<numObjects+2;i++) delete transformedMeshes[i];
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
			return objects[0];
		default: 
			assert(objects); 
			unsigned num1 = numObjects/2;
			unsigned num2 = numObjects-numObjects/2;
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

RRObjectImporter* RRObjectImporter::createMultiObject(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,maxStitchDistance,cacheLocation);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRAdditionalObjectImporter

class RRMyAdditionalObjectImporter : public RRAdditionalObjectImporter
{
public:
	RRMyAdditionalObjectImporter(RRObjectImporter* aoriginal)
	{
		original = aoriginal;
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
	virtual bool setTriangleAdditionalPower(unsigned t, RRRadiometricMeasure measure, RRColor power)
	{
		if(t>=numTriangles)
		{
			assert(0);
			return false;
		}
		switch(measure)
		{
		case RM_INCIDENT_FLUX:
			triangleInfo[t].irradiance = power / triangleInfo[t].area;
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
			triangleInfo[t].irradiance = power / triangleInfo[t].area / s->diffuseReflectance;
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
			triangleInfo[t].irradiance = power / s->diffuseReflectance;
			break;
			}
		default:
			assert(0);
			return false;
		}
		return true;
	}
	virtual void getTriangleAdditionalPower(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
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
	virtual const RRMatrix4x4* getWorldMatrix()
	{
		return original->getWorldMatrix();
	}
	virtual const RRMatrix4x4* getInvWorldMatrix()
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

RRAdditionalObjectImporter* RRObjectImporter::createAdditionalExitance()
{
	return new RRMyAdditionalObjectImporter(this);
}

} // namespace
