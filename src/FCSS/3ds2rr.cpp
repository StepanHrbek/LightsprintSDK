//
// 3ds to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <vector>
#include "3ds2rr.h"


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation

#define VERIFY
#ifdef VERIFY
void reporter(const char* msg, void* context)
{
	puts(msg);
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter class

class M3dsImporter : public rrVision::RRObjectImporter, rrCollider::RRMeshImporter
{
public:
	M3dsImporter(Model_3DS* model, unsigned objectIdx);

	virtual ~M3dsImporter();

	// RRMeshImporter
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
//	virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

	// RRObjectImporter
	virtual const rrCollider::RRCollider* getCollider() const;
	virtual unsigned                      getTriangleSurface(unsigned t) const;
	virtual const rrVision::RRSurface*    getSurface(unsigned s) const;
	virtual const rrVision::RRMatrix4x4*  getWorldMatrix();
	virtual const rrVision::RRMatrix4x4*  getInvWorldMatrix();

private:
	Model_3DS* model;
	Model_3DS::Object* object;

	// geometry
	struct TriangleInfo
	{
		unsigned s; // surface index
	};
	std::vector<TriangleInfo> triangles;

	// surfaces
	std::vector<rrVision::RRSurface> surfaces;
	
	// collider
	rrCollider::RRCollider*      collider;
};


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter load

static void fillSurface(rrVision::RRSurface* s,Model_3DS::Material* m)
{
	s->sides                = 1;
	s->diffuseReflectance   = (float)(m->color.r+m->color.g+m->color.b)/768.0f;
	s->diffuseReflectanceColor[0] = m->color.r/256.0f;
	s->diffuseReflectanceColor[1] = m->color.g/256.0f;
	s->diffuseReflectanceColor[2] = m->color.b/256.0f;
	s->diffuseTransmittance = 0;
	s->diffuseTransmittanceColor[0] = 0;
	s->diffuseTransmittanceColor[1] = 0;
	s->diffuseTransmittanceColor[2] = 0;
	s->diffuseEmittance     = 0;
	s->diffuseEmittanceColor[0] = 0;
	s->diffuseEmittanceColor[1] = 0;
	s->diffuseEmittanceColor[2] = 0;
	s->emittanceType        = rrVision::diffuseLight;
	//s->emittancePoint       =Point3(0,0,0);
	s->specularReflectance  = 0;
	s->specularTransmittance= 0;
	s->refractionReal       = 1;
	s->refractionImaginary  = 0;
	s->_rd                  = s->diffuseReflectance;
	s->_rdcx                = 0.33f;
	s->_rdcy                = 0.33f;
	s->_ed                  = 0;
}

M3dsImporter::M3dsImporter(Model_3DS* amodel, unsigned objectIdx)
{
	model = amodel;
	object = &model->Objects[objectIdx];

	assert((object->numFaces%3)==0);
	unsigned numTriangles = (unsigned)object->numFaces/3;
	for(unsigned i=0;i<numTriangles;i++)
	{
		TriangleInfo ti;
		ti.s = 0;
		triangles.push_back(ti);
	}

	for(unsigned i=0;i<(unsigned)object->numMatFaces;i++)
	{
		unsigned idx0 = object->MatFaces[i].subFaces - object->Faces;

		// hack to fix probably bad loader (od bad 3ds file?)
		if(idx0>=numTriangles) idx0 = 0;

		assert((object->MatFaces[i].numSubFaces%3)==0);
		unsigned numSubFaces = object->MatFaces[i].numSubFaces/3;

		for(unsigned j=idx0;j<(unsigned)(idx0+numSubFaces);j++)
		{
			assert(j<numTriangles);
			triangles[j].s = object->MatFaces[i].MatIndex;
		}
	}

	for(unsigned i=0;i<(unsigned)model->numMaterials;i++)
	{
		rrVision::RRSurface s;
		fillSurface(&s,&model->Materials[i]);
		surfaces.push_back(s);
	}

#ifdef VERIFY
	verify(reporter,NULL);
#endif

	// create collider
	collider = rrCollider::RRCollider::create(this,rrCollider::RRCollider::IT_BSP_FASTEST);
}


M3dsImporter::~M3dsImporter()
{
	delete collider;
}


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter implements RRMeshImporter

unsigned M3dsImporter::getNumVertices() const
{
	return object->numVerts;
}

void M3dsImporter::getVertex(unsigned v, Vertex& out) const
{
	assert(v<(unsigned)object->numVerts);
	out[0] = object->Vertexes[3*v];
	out[1] = object->Vertexes[3*v+1];
	out[2] = object->Vertexes[3*v+2];
}

unsigned M3dsImporter::getNumTriangles() const
{
	assert((object->numFaces%3)==0);
	return object->numFaces/3;
}

void M3dsImporter::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out.m[0] = object->Faces[3*t];
	out.m[1] = object->Faces[3*t+1];
	out.m[2] = object->Faces[3*t+2];
}


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter implements RRObjectImporter

const rrCollider::RRCollider* M3dsImporter::getCollider() const
{
	return collider;
}

unsigned M3dsImporter::getTriangleSurface(unsigned t) const
{
	if(t>=getNumTriangles())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<surfaces.size());
	return s;
}

const rrVision::RRSurface* M3dsImporter::getSurface(unsigned s) const
{
	if(s>=surfaces.size()) 
	{
		assert(0);
		return NULL;
	}
	return &surfaces[s];
}

const rrVision::RRMatrix4x4* M3dsImporter::getWorldMatrix()
{
	//!!!
	return NULL;
}

const rrVision::RRMatrix4x4* M3dsImporter::getInvWorldMatrix()
{
	//!!!
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// main

rrVision::RRObjectImporter* new_3ds_importer(Model_3DS* model, unsigned objectIdx)
{
	rrVision::RRObjectImporter* importer = new M3dsImporter(model, objectIdx);
#ifdef VERIFY
	importer->getCollider()->getImporter()->verify(reporter,NULL);
#endif
	return importer;
}

rrVision::RRObjectImporter* new_3ds_importer(Model_3DS* model)
{
	rrVision::RRObjectImporter** importers = new rrVision::RRObjectImporter*[model->numObjects];
	for(unsigned i=0;i<(unsigned)model->numObjects;i++)
	{
		importers[i] = new_3ds_importer(model, i);
	}
	rrVision::RRObjectImporter* multi = rrVision::RRObjectImporter::createMultiObject(importers,model->numObjects,rrCollider::RRCollider::IT_BSP_FASTEST,0,NULL);
	//!!! delete[] importers;
	return multi;
}
