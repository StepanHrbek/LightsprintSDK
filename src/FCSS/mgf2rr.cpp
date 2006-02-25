//
// mgf to rrobject loader by Stepan Hrbek, dee@mail.cz
//

float SCALE  = 2; // scale loaded object

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <vector>
#include "ldmgf.h"
#include "mgf2rr.h"


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter class

class MgfImporter : public rrVision::RRObjectImporter, rrCollider::RRMeshImporter
{
public:
	MgfImporter(char* filename);

	virtual ~MgfImporter();

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
	// geometry
	struct VertexInfo
	{
		Vertex v; // vertex coordinates
		rrVision::RRVec3 n; // normal coordinates
	};
	struct TriangleInfo
	{
		Triangle t; // triangle vertices indices
		unsigned s; // surface index
	};
	static void* add_vertex(FLOAT *p,FLOAT *n);
	static void add_polygon(unsigned vertices,void **vertex,void *material);
	std::vector<VertexInfo> vertices;
	std::vector<TriangleInfo> triangles;

	// surfaces
	std::vector<rrVision::RRSurface> surfaces;
	static void* add_material(C_MATERIAL *m);
	
	// collider
	rrCollider::RRCollider*      collider;
};


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter load

// static pointer to current instance
// it is not thread safe to load multiple files simultaneously
// this is mgflib restriction
static MgfImporter* mgfImporter = NULL;

void* MgfImporter::add_vertex(FLOAT *p,FLOAT *n)
{
	if(!mgfImporter) return NULL;
	VertexInfo vi;
	vi.v[0] = (float)p[0]*SCALE;
	vi.v[1] = (float)p[1]*SCALE;
	vi.v[2] = (float)p[2]*SCALE;
	vi.n[0] = (float)n[0];
	vi.n[1] = (float)n[1];
	vi.n[2] = (float)n[2];
	mgfImporter->vertices.push_back(vi);
	return (void *)(mgfImporter->vertices.size()-1);
}

static void fillSurface(rrVision::RRSurface *s,C_MATERIAL *m)
{
	s->reset(m->sided==2);
	xy2rgb(m->rd_c.cx,m->rd_c.cy,0.5,&s->diffuseReflectance.x);
	for(unsigned c=0;c<3;c++)
	{
		s->diffuseReflectance[c] *= m->rd;
		if(s->diffuseReflectance[c]>0.95f) s->diffuseReflectance[c]=0.95f;
	}
	xy2rgb(m->ed_c.cx,m->ed_c.cy,0.5,&s->diffuseEmittance.x);
	s->diffuseEmittance     *=m->ed/1000;
	s->specularReflectance  =m->rs;
	s->specularTransmittance=m->ts;
	s->refractionReal       =m->nr;
}

void* MgfImporter::add_material(C_MATERIAL *m)
{
	if(!mgfImporter) return NULL;
	rrVision::RRSurface s;
	fillSurface(&s,m);
	mgfImporter->surfaces.push_back(s);
	return (void *)(mgfImporter->surfaces.size()-1);
}

void MgfImporter::add_polygon(unsigned vertices,void **vertex,void *material)
{
	for(unsigned i=2;i<vertices;i++)
	{
		TriangleInfo ti;
		ti.t[0] = (unsigned)vertex[0];
		ti.t[1] = (unsigned)vertex[i-1];
		ti.t[2] = (unsigned)vertex[i];
		ti.s = (unsigned)material;
		mgfImporter->triangles.push_back(ti);
	}
}

MgfImporter::MgfImporter(char* filename)
{
	// vsechno nacte
	mgfImporter = this;
	ldmgf(filename,add_vertex,add_material,add_polygon,NULL,NULL);
	mgfImporter = NULL;

	printf("vertices: %i\n",getNumVertices());
	printf("triangles: %i\n",getNumTriangles());
	//printf("materials: %i\n",getNumSurfaces());

	// create collider
	collider = rrCollider::RRCollider::create(this,rrCollider::RRCollider::IT_BSP_FASTEST);
}


MgfImporter::~MgfImporter()
{
	delete collider;
}


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter implements RRMeshImporter

unsigned MgfImporter::getNumVertices() const
{
	return vertices.size();
}

void MgfImporter::getVertex(unsigned v, Vertex& out) const
{
	assert(v<vertices.size());
	out = vertices[v].v;
}

unsigned MgfImporter::getNumTriangles() const
{
	return triangles.size();
}

void MgfImporter::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=triangles.size()) 
	{
		assert(t<triangles.size());
		return;
	}
	out = triangles[t].t;
}


//////////////////////////////////////////////////////////////////////////////
//
// MgfImporter implements RRObjectImporter

const rrCollider::RRCollider* MgfImporter::getCollider() const
{
	return collider;
}

unsigned MgfImporter::getTriangleSurface(unsigned t) const
{
	if(t>=triangles.size())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<surfaces.size());
	return s;
}

const rrVision::RRSurface* MgfImporter::getSurface(unsigned s) const
{
	if(s>=surfaces.size()) 
	{
		assert(0);
		return NULL;
	}
	return &surfaces[s];
}

const rrVision::RRMatrix4x4* MgfImporter::getWorldMatrix()
{
	return NULL;
}

const rrVision::RRMatrix4x4* MgfImporter::getInvWorldMatrix()
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// main

rrVision::RRObjectImporter* new_mgf_importer(char* filename)
{
	return new MgfImporter(filename);
}
