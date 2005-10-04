#include "RRCollider.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"
#include <math.h>
#include <memory.h>
#include <new> // aligned new
#include <stdio.h>
#include <string.h>
#include <vector>


namespace rrCollider
{

//////////////////////////////////////////////////////////////////////////////
//
// RRCopyMeshImporter
//
// For creating fully owned copies of importer,
// saving to file and loading from file.

class RRCopyMeshImporter : public RRMeshImporter
{
public:
	RRCopyMeshImporter()
	{
	}

	// Save importer to disk in importer binary interchange format.
	// Check importer consistency during save so we don't have to store redundant data.
	bool save(const char* filename)
	{
		//!!!
		return false;
	}

	// Load importer from disk in importer binary interchange format.
	bool load(const char* filename)
	{
		//!!!
		return false;
	}

	// Create copy of another importer.
	bool load(RRMeshImporter* importer)
	{
		if(!importer) return false;

		// copy vertices
		unsigned numVertices = importer->getNumVertices();
		postImportVertices.resize(numVertices);
		for(unsigned i=0;i<numVertices;i++)
		{
			Vertex v;
			importer->getVertex(i,v);
			postImportVertices[i] = v;
		}

		// copy triangles
		unsigned numPreImportTriangles = 0;
		unsigned numTriangles = importer->getNumTriangles();
		postImportTriangles.resize(numTriangles);
		for(unsigned postImportTriangle=0;postImportTriangle<numTriangles;postImportTriangle++)
		{
			PostImportTriangle t;
			// copy getPreImportTriangle
			t.preImportTriangle = importer->getPreImportTriangle(postImportTriangle);
			assert(t.preImportTriangle!=UINT_MAX);
			// copy getTriangle
			importer->getTriangle(postImportTriangle,t.postImportTriangleVertices);
			// copy getPreImportVertex
			for(unsigned j=0;j<3;j++)
			{
				assert(t.postImportTriangleVertices[j]!=UINT_MAX);
				t.preImportTriangleVertices[j] = importer->getPreImportVertex(t.postImportTriangleVertices[j],postImportTriangle);
			}
			numPreImportTriangles = std::max(numPreImportTriangles,t.preImportTriangle+1);
			postImportTriangles[postImportTriangle] = t;
		}

		// copy triangleBodies
		for(unsigned i=0;i<numTriangles;i++)
		{
			TriangleBody t;
			importer->getTriangleBody(i,t);
			//!!! check that getTriangleBody returns numbers consistent with getVertex/getTriangle
		}

		// copy getPostImportTriangle
		pre2postImportTriangles.resize(numPreImportTriangles);
		for(unsigned preImportTriangle=0;preImportTriangle<numPreImportTriangles;preImportTriangle++)
		{
			unsigned postImportTriangle = importer->getPostImportTriangle(preImportTriangle);
			assert(postImportTriangle==UINT_MAX || postImportTriangle<postImportTriangles.size());
			pre2postImportTriangles[preImportTriangle] = postImportTriangle;
		}

		return true;
	}

	virtual ~RRCopyMeshImporter()
	{
	}

	// vertices
	virtual unsigned     getNumVerticesHere() const
	{
		return static_cast<unsigned>(postImportVertices.size());
	}
	virtual unsigned     getNumVertices() const
	{
		return getNumVerticesHere();
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		assert(v<postImportVertices.size());
		out = postImportVertices[v];
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return postImportTriangles.size();
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<postImportTriangles.size());
		out = postImportTriangles[t].postImportTriangleVertices;
	}

	// preimport/postimport conversions
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		assert(postImportVertex<postImportVertices.size());
		assert(postImportTriangle<postImportTriangles.size());
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[0]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[0];
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[1]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[1];
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[2]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[2];
		assert(0);
		return UINT_MAX;
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		unsigned postImportTriangle = getPostImportTriangle(preImportTriangle);
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[0]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[0];
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[1]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[1];
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[2]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[2];
		assert(0);
		return UINT_MAX;
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const
	{
		assert(postImportTriangle<postImportTriangles.size());
		return postImportTriangles[postImportTriangle].preImportTriangle;
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		assert(preImportTriangle<pre2postImportTriangles.size());
		return pre2postImportTriangles[preImportTriangle];
	}

protected:
	std::vector<Vertex>   postImportVertices;
	struct PostImportTriangle
	{
		unsigned preImportTriangle;
		Triangle postImportTriangleVertices;
		Triangle preImportTriangleVertices;
	};
	std::vector<PostImportTriangle> postImportTriangles;
	std::vector<unsigned> pre2postImportTriangles; // sparse(inverse of preImportTriangles) -> vector is mostly ok, but multimesh probably needs map
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMultiMeshImporter
//
// Merges multiple mesh importers together.
// Space is not transformed here, underlying meshes must already share one space.
// Defines its own PreImportNumber, see below.

class RRMultiMeshImporter : public RRMeshImporter
{
public:
	// creators
	static RRMeshImporter* create(RRMeshImporter* const* mesh, unsigned numMeshes)
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
			return new RRMultiMeshImporter(
				create(mesh,numMeshes/2),numMeshes/2,
				create(mesh+numMeshes/2,numMeshes-numMeshes/2),numMeshes-numMeshes/2);
		}
	}

	// vertices
	virtual unsigned     getNumVertices() const
	{
		return pack[0].getNumVertices()+pack[1].getNumVertices();
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		if(v<pack[0].getNumVertices()) 
			pack[0].getImporter()->getVertex(v,out);
		else
			pack[1].getImporter()->getVertex(v-pack[0].getNumVertices(),out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return pack[0].getNumTriangles()+pack[1].getNumTriangles();
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		if(t<pack[0].getNumTriangles()) 
			pack[0].getImporter()->getTriangle(t,out);
		else
			pack[1].getImporter()->getTriangle(t-pack[0].getNumTriangles(),out);
	}

	// optional for faster access
	//!!! default is slow
	//virtual void         getTriangleBody(unsigned i, TriangleBody* t) const
	//{
	//}

	// preimport/postimport conversions
	struct PreImportNumber 
		// our structure of pre import number (it is independent for each implementation)
		// (on the other hand, postimport is always plain unsigned, 0..num-1)
		// underlying importers must use preImport values that fit into index, this is not runtime checked
	{
		unsigned index : sizeof(unsigned)*8-12; // 32bit: max 1M triangles/vertices in one object
		unsigned object : 12; // 32bit: max 4k objects
		PreImportNumber() {}
		PreImportNumber(unsigned i) {*(unsigned*)this = i;} // implicit unsigned -> PreImportNumber conversion
		operator unsigned () {return *(unsigned*)this;} // implicit PreImportNumber -> unsigned conversion
	};
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		if(postImportVertex<pack[0].getNumVertices()) 
		{
			return pack[0].getImporter()->getPreImportVertex(postImportVertex, postImportTriangle);
		} else {
			PreImportNumber preImport = pack[1].getImporter()->getPreImportVertex(postImportVertex-pack[0].getNumVertices(), postImportTriangle-pack[0].getNumTriangles());
			preImport.object += pack[0].getNumObjects();
			preImport.index += pack[0].getNumVertices();
			return preImport;
		}
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		PreImportNumber preImportV = preImportVertex;
		PreImportNumber preImportT = preImportTriangle;
		if(preImportV.object<pack[0].getNumObjects()) 
		{
			return pack[0].getImporter()->getPostImportVertex(preImportV, preImportT);
		} else {
			preImportV.object -= pack[0].getNumObjects();
			preImportV.index -= pack[0].getNumVertices();
			preImportT.object -= pack[0].getNumObjects();
			preImportT.index -= pack[0].getNumTriangles();
			return pack[1].getImporter()->getPostImportVertex(preImportV, preImportT);
		}
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle<pack[0].getNumTriangles()) 
		{
			return pack[0].getImporter()->getPreImportTriangle(postImportTriangle);
		} else {
			PreImportNumber preImport = pack[1].getImporter()->getPreImportTriangle(postImportTriangle-pack[0].getNumTriangles());
			preImport.object += pack[0].getNumObjects();
			preImport.index += pack[0].getNumTriangles();
			return preImport;
		}
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		PreImportNumber preImport = preImportTriangle;
		if(preImport.object<pack[0].getNumObjects()) 
		{
			return pack[0].getImporter()->getPostImportTriangle(preImport);
		} else {
			preImport.object -= pack[0].getNumObjects();
			preImport.index -= pack[0].getNumTriangles();
			return pack[1].getImporter()->getPostImportTriangle(preImport);
		}
	}

	virtual ~RRMultiMeshImporter()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if(pack[0].getNumObjects()>1) delete pack[0].getImporter();
		if(pack[1].getNumObjects()>1) delete pack[1].getImporter();
	}
private:
	RRMultiMeshImporter(const RRMeshImporter* mesh1, unsigned mesh1Objects, const RRMeshImporter* mesh2, unsigned mesh2Objects)
	{
		pack[0].init(mesh1,mesh1Objects);
		pack[1].init(mesh2,mesh2Objects);
	}
	struct MeshPack
	{
		void init(const RRMeshImporter* importer, unsigned objects)
		{
			packImporter = importer;
			numObjects = objects;
			assert(importer);
			numVertices = importer->getNumVertices();
			numTriangles = importer->getNumTriangles();
		}
		const RRMeshImporter* getImporter() const {return packImporter;}
		unsigned        getNumObjects() const {return numObjects;}
		unsigned        getNumVertices() const {return numVertices;}
		unsigned        getNumTriangles() const {return numTriangles;}
	private:
		const RRMeshImporter* packImporter;
		unsigned        numObjects;
		unsigned        numVertices;
		unsigned        numTriangles;
	};
	MeshPack        pack[2];
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshImporter instance factory

bool RRMeshImporter::save(char* filename)
{
	RRCopyMeshImporter* importer = new RRCopyMeshImporter();
	bool res = importer->load(this) && importer->save(filename);
	delete importer;
	return res;
}

RRMeshImporter* RRMeshImporter::load(char* filename)
{
	RRCopyMeshImporter* importer = new RRCopyMeshImporter();
	if(importer->load(filename)) return importer;
	delete importer;
	return NULL;
}

RRMeshImporter* RRMeshImporter::createCopy()
{
	RRCopyMeshImporter* importer = new RRCopyMeshImporter();
	if(importer->load(this)) return importer;
	delete importer;
	return NULL;
}

RRMeshImporter* RRMeshImporter::createMultiMesh(RRMeshImporter* const* meshes, unsigned numMeshes)
{
	return RRMultiMeshImporter::create(meshes,numMeshes);
}




#ifdef USE_SSE
void* AlignedMalloc(size_t size,int byteAlign)
{
	void *mallocPtr = malloc(size + byteAlign + sizeof(void*));
	size_t ptrInt = (size_t)mallocPtr;

	ptrInt = (ptrInt + byteAlign + sizeof(void*)) / byteAlign * byteAlign;
	*(((void**)ptrInt) - 1) = mallocPtr;

	return (void*)ptrInt;
}

void AlignedFree(void *ptr)
{
	free(*(((void**)ptr) - 1));
}
#endif

void* RRAligned::operator new(std::size_t n)
{
#ifdef USE_SSE
	return AlignedMalloc(n,16);
#else
	return ::operator new(n);
#endif
};

void* RRAligned::operator new[](std::size_t n)
{
#ifdef USE_SSE
	return AlignedMalloc(n,16);
#else
	return ::operator new(n);
#endif
};

void RRAligned::operator delete(void* p, std::size_t n)
{
#ifdef USE_SSE
	AlignedFree(p);
#else
	::delete(p);
#endif
};

void RRAligned::operator delete[](void* p, std::size_t n)
{
#ifdef USE_SSE
	AlignedFree(p);
#else
	::delete(p);
#endif
};

RRRay::RRRay()
{
	memset(this,0,sizeof(RRRay)); // no virtuals in RRRay -> no pointer to virtual function table overwritten
	rayOrigin[3] = 1;
	rayFlags = FILL_DISTANCE | FILL_POINT3D | FILL_POINT2D | FILL_PLANE | FILL_TRIANGLE | FILL_SIDE;
}

RRRay* RRRay::create()
{
	return new RRRay();
}

RRRay* RRRay::create(unsigned n)
{
	assert(!(sizeof(RRRay)%16));
	return new RRRay[n]();
}

void RRMeshImporter::getTriangleBody(unsigned i, TriangleBody& out) const
{
	Triangle t;
	getTriangle(i,t);
	Vertex v[3];
	getVertex(t[0],v[0]);
	getVertex(t[1],v[1]);
	getVertex(t[2],v[2]);
	out.vertex0[0]=v[0][0];
	out.vertex0[1]=v[0][1];
	out.vertex0[2]=v[0][2];
	out.side1[0]=v[1][0]-v[0][0];
	out.side1[1]=v[1][1]-v[0][1];
	out.side1[2]=v[1][2]-v[0][2];
	out.side2[0]=v[2][0]-v[0][0];
	out.side2[1]=v[2][1]-v[0][1];
	out.side2[2]=v[2][2]-v[0][2];
}

RRCollider* RRCollider::create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation, void* buildParams)
{
	if(!importer) return NULL;
	BuildParams bp(intersectTechnique);
	if(!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case IT_BSP_COMPACT:
			if(importer->getNumTriangles()<=256)
			{
				typedef IntersectBspCompact<CBspTree21> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			if(importer->getNumTriangles()<=65536)
			{
				typedef IntersectBspCompact<CBspTree42> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			{
				typedef IntersectBspCompact<CBspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_BSP_FASTEST:
		case IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,(intersectTechnique==IT_BSP_FAST)?".fast":".fastest",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_VERIFICATION:
			{
				return IntersectVerification::create(importer);
			}
		case IT_LINEAR: 
		default:
		linear:
			assert(importer);
			if(!importer) return NULL;
			return IntersectLinear::create(importer);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	memset(this,0,sizeof(*this));
}

RRIntersectStats* RRIntersectStats::getInstance()
{
	return &intersectStats;
}

void RRIntersectStats::getInfo(char *buf, unsigned len, unsigned level) const
{
	buf[0]=0;
	len--;
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf),"Intersect stats:\n");
	if(intersects>100) {
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf)," rays=%d missed=%d(%d)\n",intersects,intersects-hits,(intersects-hits)/(intersects/100));
	if(level>=1 && (intersect_bspSRLNP || intersect_triangleSRLNP)) _snprintf(buf+strlen(buf),len-strlen(buf)," bspSRLNP=%d(%d) triSRLNP=%d(%d)\n",intersect_bspSRLNP,intersect_bspSRLNP/intersects,intersect_triangleSRLNP,intersect_triangleSRLNP/intersects);
	if(level>=1 && (intersect_bspNP    || intersect_triangleNP   )) _snprintf(buf+strlen(buf),len-strlen(buf)," bspNP=%d(%d) triNP=%d(%d)\n",intersect_bspNP,intersect_bspNP/intersects,intersect_bspNP,intersect_bspNP/intersects);
	}
	if(invalid_triangles) _snprintf(buf+strlen(buf),len-strlen(buf)," invalid_triangles=%d/%d\n",invalid_triangles,loaded_triangles);
	if(intersect_linear) _snprintf(buf+strlen(buf),len-strlen(buf)," intersect_linear=%d\n",intersect_linear);
	buf[len]=0;
}

} //namespace
