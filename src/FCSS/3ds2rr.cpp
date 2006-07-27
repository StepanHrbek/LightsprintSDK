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

//#define VERIFY
#ifdef VERIFY
void reporter(const char* msg, void* context)
{
	puts(msg);
}
#endif


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter

class M3dsImporter : public rr::RRObject, rr::RRMesh
{
public:
	M3dsImporter(Model_3DS* model, unsigned objectIdx);

	virtual ~M3dsImporter();

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
//	virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual unsigned                getTriangleSurface(unsigned t) const;
	virtual const rr::RRSurface*    getSurface(unsigned s) const;
	virtual void                    getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual const rr::RRMatrix3x4*  getWorldMatrix();
	virtual const rr::RRMatrix3x4*  getInvWorldMatrix();

private:
	Model_3DS* model;
	Model_3DS::Object* object;

	// geometry
	struct TriangleInfo
	{
		rr::RRMesh::Triangle t;
		unsigned s; // surface index
	};
	std::vector<TriangleInfo> triangles;

	// surfaces
	std::vector<rr::RRSurface> surfaces;
	
	// collider
	rr::RRCollider*      collider;
};


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter load

static void fillSurface(rr::RRSurface* s,Model_3DS::Material* m)
{
	enum {size = 8};

	// average texture color
	rr::RRColor avg = rr::RRColor(0);
	if(m->tex)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				rr::RRColor tmp;
				m->tex->getPixel(i/(float)size,j/(float)size,&tmp[0]);
				avg += tmp;
			}
		avg /= size*size;
	}
	else
	{
		avg[0] = m->color.r;
		avg[1] = m->color.g;
		avg[2] = m->color.b;
	}

	s->reset(0);
	s->diffuseReflectance = avg;
#ifdef VERIFY
	if(s->validate())
		reporter("Surface adjusted to physically valid.",NULL)
#else
	s->validate();
#endif
}

M3dsImporter::M3dsImporter(Model_3DS* amodel, unsigned objectIdx)
{
	model = amodel;
	object = &model->Objects[objectIdx];

	for(unsigned i=0;i<(unsigned)object->numMatFaces;i++)
	{
		for(unsigned j=0;j<(unsigned)object->MatFaces[i].numSubFaces/3;j++)
		{
			TriangleInfo ti;
			ti.t[0] = object->MatFaces[i].subFaces[3*j];
			ti.t[1] = object->MatFaces[i].subFaces[3*j+1];
			ti.t[2] = object->MatFaces[i].subFaces[3*j+2];
			ti.s = object->MatFaces[i].MatIndex;
			triangles.push_back(ti);
		}
	}

	for(unsigned i=0;i<(unsigned)model->numMaterials;i++)
	{
		rr::RRSurface s;
		fillSurface(&s,&model->Materials[i]);
		surfaces.push_back(s);
	}

#ifdef VERIFY
	verify(reporter,NULL);
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);
}


M3dsImporter::~M3dsImporter()
{
	delete collider;
}

//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRChanneledData

void M3dsImporter::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case CHANNEL_SURFACE_DIF_TEX:
			if(numItems) *numItems = model->numMaterials;
			if(itemSize) *itemSize = sizeof(Texture*);
			return;
		case CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			if(numItems) *numItems = M3dsImporter::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			assert(0); // legal, but shouldn't happen in well coded program
			if(numItems) *numItems = 0;
			if(itemSize) *itemSize = 0;
	}
}

bool M3dsImporter::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	switch(channelId)
	{
	case CHANNEL_SURFACE_DIF_TEX:
		{
			if(!itemData)
			{
				assert(0);
				return false;
			}
			if(itemIndex>=(unsigned)model->numMaterials)
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef Texture* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			*out = model->Materials[itemIndex].tex;
			return true;
		}
	case CHANNEL_TRIANGLE_VERTICES_DIF_UV:
		{
			if(!itemData)
			{
				assert(0);
				return false;
			}
			if(itemIndex>=M3dsImporter::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRVec2 Out[3];
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			Triangle triangle;
			M3dsImporter::getTriangle(itemIndex,triangle);
			for(unsigned v=0;v<3;v++)
			{
				(*out)[v][0] = object->TexCoords[2*triangle[v]];
				(*out)[v][1] = object->TexCoords[2*triangle[v]+1];
			}
			return true;
		}
	default:
		assert(0); // legal, but shouldn't happen in well coded program
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRMesh

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
	return triangles.size();
}

void M3dsImporter::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=M3dsImporter::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out = triangles[t].t;
}


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRObject

const rr::RRCollider* M3dsImporter::getCollider() const
{
	return collider;
}

unsigned M3dsImporter::getTriangleSurface(unsigned t) const
{
	if(t>=M3dsImporter::getNumTriangles())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<surfaces.size());
	return s;
}

const rr::RRSurface* M3dsImporter::getSurface(unsigned s) const
{
	if(s>=surfaces.size()) 
	{
		assert(0);
		return NULL;
	}
	return &surfaces[s];
}

void M3dsImporter::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if(t>=M3dsImporter::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	M3dsImporter::getTriangle(t,triangle);
	for(unsigned v=0;v<3;v++)
	{
		out.norm[v][0] = object->Normals[3*triangle[v]];
		out.norm[v][1] = object->Normals[3*triangle[v]+1];
		out.norm[v][2] = object->Normals[3*triangle[v]+2];
	}
}

const rr::RRMatrix3x4* M3dsImporter::getWorldMatrix()
{
	//!!!
	return NULL;
}

const rr::RRMatrix3x4* M3dsImporter::getInvWorldMatrix()
{
	//!!!
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObject* new_3ds_importer(Model_3DS* model, unsigned objectIdx)
{
	rr::RRObject* importer = new M3dsImporter(model, objectIdx);
#ifdef VERIFY
	importer->getCollider()->getMesh()->verify(reporter,NULL);
#endif
	return importer;
}

void new_3ds_importer(Model_3DS* model,rr::RRVisionApp* app)
{
	rr::RRVisionApp::Objects objects;
	for(unsigned i=0;i<(unsigned)model->numObjects;i++)
	{
		objects.push_back(rr::RRVisionApp::Object(new_3ds_importer(model,i),new rr::RRObjectIlluminationForEditor(model->Objects[i].numVerts)));
	}
	if(app) app->setObjects(objects);
}

