// --------------------------------------------------------------------------
// Creates Lightsprint interface for MGF scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code loads MGF scene (geometry primitives, materials) into RRObjects.
//
// Unlike adapters for other formats, this one doesn't adapt 3rd party structure
// in memory, it loads scene directly form file.


#include <cmath>
#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "RRObjectMGF.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "ldmgf.h"


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new wrappers.
// Define VERIFY to enable verification of wrappers and data.
// Once your code/data are verified and don't emit messages via reporter(),
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

#define VERIFY

#ifdef VERIFY
void reporter(const char* msg, void* context)
{
	puts(msg);
}
#endif


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectMGF : public rr::RRObject, rr::RRMesh
{
public:
	RRObjectMGF(const char* filename);
	rr::RRObjectIllumination* getIllumination();
	virtual ~RRObjectMGF();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual const rr::RRMaterial*   getTriangleMaterial(unsigned t) const;

	// copy of object's vertices
	struct VertexInfo
	{
		rr::RRVec3 pos;
	};
	std::vector<VertexInfo> vertices;

	// copy of object's triangles
	struct TriangleInfo
	{
		rr::RRMesh::Triangle indices;
		unsigned material; // material index
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's materials
	std::vector<rr::RRMaterial> materials;
	
	// collider for ray-mesh collisions
	rr::RRCollider* collider;

	// indirect illumination (ambient maps etc)
	rr::RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF load

RRObjectMGF* g_scene; // global scene necessary for callbacks

void *add_vertex(FLOAT *p,FLOAT *n)
{
	RRObjectMGF::VertexInfo v;
	v.pos = rr::RRVec3(rr::RRReal(p[0]),rr::RRReal(p[1]),rr::RRReal(p[2]));
	g_scene->vertices.push_back(v);
	return (void *)(g_scene->vertices.size()-1);
}

void *add_material(C_MATERIAL *m)
{
	rr::RRMaterial mat;
	mat.reset(m->sided==0);
	mgf2rgb(&m->ed_c,m->ed/4000,mat.diffuseEmittance); //!!!
	mgf2rgb(&m->rd_c,m->rd,mat.diffuseReflectance);
	mat.specularReflectance = m->rs;
	mgf2rgb(&m->ts_c,m->ts,mat.specularTransmittance);
	mat.refractionIndex = m->nr;
#ifdef VERIFY
	if(mat.validate())
		reporter("Material adjusted to physically valid.",NULL);
#else
	mat.validate();
#endif
	g_scene->materials.push_back(mat);
	return (void *)(g_scene->materials.size()-1);
}

void add_polygon(unsigned vertices,void **vertex,void *material)
{
	for(unsigned i=2;i<vertices;i++)
	{
		RRObjectMGF::TriangleInfo t;
		t.indices[0] = (unsigned)vertex[0];
		t.indices[1] = (unsigned)vertex[i-1];
		t.indices[2] = (unsigned)vertex[i];
		t.material = (unsigned)material;
		g_scene->triangles.push_back(t);
	}
}

RRObjectMGF::RRObjectMGF(const char* filename)
{
	g_scene = this;
	ldmgf(filename,add_vertex,add_material,add_polygon,NULL,NULL);

#ifdef VERIFY
	verify();
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);

	// create illumination
	illumination = new rr::RRObjectIllumination(getNumVertices());
}

rr::RRObjectIllumination* RRObjectMGF::getIllumination()
{
	return illumination;
}

RRObjectMGF::~RRObjectMGF()
{
	delete illumination;
	delete collider;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF implements RRMesh

unsigned RRObjectMGF::getNumVertices() const
{
	return (unsigned)vertices.size();
}

void RRObjectMGF::getVertex(unsigned v, Vertex& out) const
{
	RR_ASSERT(v<(unsigned)vertices.size());
	out = vertices[v].pos;
}

unsigned RRObjectMGF::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObjectMGF::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=RRObjectMGF::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	out = triangles[t].indices;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF implements RRObject

const rr::RRCollider* RRObjectMGF::getCollider() const
{
	return collider;
}

const rr::RRMaterial* RRObjectMGF::getTriangleMaterial(unsigned t) const
{
	if(t>=RRObjectMGF::getNumTriangles())
	{
		RR_ASSERT(0);
		return NULL;
	}
	unsigned material = triangles[t].material;
	if(material>=materials.size())
	{
		RR_ASSERT(0);
		return NULL;
	}
	return &materials[material];
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromMGF

class ObjectsFromMGF : public rr::RRObjects
{
public:
	ObjectsFromMGF(const char* filename)
	{
		RRObjectMGF* object = new RRObjectMGF(filename);
		if(object->getNumTriangles())
			push_back(rr::RRIlluminatedObject(object,object->getIllumination()));
	}
	virtual ~ObjectsFromMGF()
	{
		delete (*this)[0].object;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObjects* adaptObjectsFromMGF(const char* filename)
{
	return new ObjectsFromMGF(filename);
}
