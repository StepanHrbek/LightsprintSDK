#include "RRCollider.h"
#include "RRFilteredMeshImporter.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <new> // aligned new
#ifdef _MSC_VER
	#include "stdint.h"
#else
	#include <stdint.h>
#endif
#include <stdio.h>
#include <string.h>
#include <vector>


namespace rrCollider
{



//////////////////////////////////////////////////////////////////////////////
//
// Importers from vertex/index buffers
//
// support indices of any size, vertex positions float[3]
//
// RRTriStripImporter               - triangle strip
// RRTriListImporter                - triangle list
// RRIndexedTriStripImporter<INDEX> - indexed triangle strip 
// RRIndexedTriListImporter<INDEX>  - indexed triangle list

class RRTriStripImporter : public RRMeshImporter
{
public:
	RRTriStripImporter(char* vbuffer, unsigned vertices, unsigned stride)
		: VBuffer(vbuffer), Vertices(vertices), Stride(stride)
	{
	}

	virtual unsigned getNumVertices() const
	{
		return Vertices;
	}
	virtual void getVertex(unsigned v, Vertex& out) const
	{
		assert(v<Vertices);
		assert(VBuffer);
		out = *(Vertex*)(VBuffer+v*Stride);
	}
	virtual unsigned getNumTriangles() const
	{
		return Vertices-2;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<Vertices-2);
		out[0] = t+0;
		out[1] = t+1;
		out[2] = t+2;
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		assert(t<Vertices-2);
		assert(VBuffer);
		unsigned v0,v1,v2;
		v0 = t+0;
		v1 = t+1;
		v2 = t+2;
#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
		out.vertex0[0] = VERTEX(v0)[0];
		out.vertex0[1] = VERTEX(v0)[1];
		out.vertex0[2] = VERTEX(v0)[2];
		out.side1[0] = VERTEX(v1)[0]-out.vertex0[0];
		out.side1[1] = VERTEX(v1)[1]-out.vertex0[1];
		out.side1[2] = VERTEX(v1)[2]-out.vertex0[2];
		out.side2[0] = VERTEX(v2)[0]-out.vertex0[0];
		out.side2[1] = VERTEX(v2)[1]-out.vertex0[1];
		out.side2[2] = VERTEX(v2)[2]-out.vertex0[2];
#undef VERTEX
	}

protected:
	char*                VBuffer;
	unsigned             Vertices;
	unsigned             Stride;
};

//////////////////////////////////////////////////////////////////////////////

class RRTriListImporter : public RRTriStripImporter
{
public:
	RRTriListImporter(char* vbuffer, unsigned vertices, unsigned stride)
		: RRTriStripImporter(vbuffer,vertices,stride)
	{
	}
	virtual unsigned getNumTriangles() const
	{
		return Vertices/3;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		assert(t*3<Vertices);
		out[0] = t*3+0;
		out[1] = t*3+1;
		out[2] = t*3+2;
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		assert(t*3<Vertices);
		assert(VBuffer);
		unsigned v0,v1,v2;
		v0 = t*3+0;
		v1 = t*3+1;
		v2 = t*3+2;
#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
		out.vertex0[0] = VERTEX(v0)[0];
		out.vertex0[1] = VERTEX(v0)[1];
		out.vertex0[2] = VERTEX(v0)[2];
		out.side1[0] = VERTEX(v1)[0]-out.vertex0[0];
		out.side1[1] = VERTEX(v1)[1]-out.vertex0[1];
		out.side1[2] = VERTEX(v1)[2]-out.vertex0[2];
		out.side2[0] = VERTEX(v2)[0]-out.vertex0[0];
		out.side2[1] = VERTEX(v2)[1]-out.vertex0[1];
		out.side2[2] = VERTEX(v2)[2]-out.vertex0[2];
#undef VERTEX
	}
};

//////////////////////////////////////////////////////////////////////////////

template <class INDEX>
class RRIndexedTriStripImporter : public RRTriStripImporter
{
public:
	RRIndexedTriStripImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: RRTriStripImporter(vbuffer,vertices,stride), IBuffer(ibuffer), Indices(indices)
	{
		INDEX tmp = vertices;
		tmp = tmp;
		assert(tmp==vertices);
	}

	virtual unsigned getNumTriangles() const
	{
		return Indices-2;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<Indices-2);
		assert(IBuffer);
		out[0] = IBuffer[t];         assert(out[0]<Vertices);
		out[1] = IBuffer[t+1+(t%2)]; assert(out[1]<Vertices);
		out[2] = IBuffer[t+2-(t%2)]; assert(out[2]<Vertices);
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		assert(t<Indices-2);
		assert(VBuffer);
		assert(IBuffer);
		unsigned v0,v1,v2;
		v0 = IBuffer[t];         assert(v0<Vertices);
		v1 = IBuffer[t+1+(t%2)]; assert(v1<Vertices);
		v2 = IBuffer[t+2-(t%2)]; assert(v2<Vertices);
#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
		out.vertex0[0] = VERTEX(v0)[0];
		out.vertex0[1] = VERTEX(v0)[1];
		out.vertex0[2] = VERTEX(v0)[2];
		out.side1[0] = VERTEX(v1)[0]-out.vertex0[0];
		out.side1[1] = VERTEX(v1)[1]-out.vertex0[1];
		out.side1[2] = VERTEX(v1)[2]-out.vertex0[2];
		out.side2[0] = VERTEX(v2)[0]-out.vertex0[0];
		out.side2[1] = VERTEX(v2)[1]-out.vertex0[1];
		out.side2[2] = VERTEX(v2)[2]-out.vertex0[2];
#undef VERTEX
	}

protected:
	INDEX*               IBuffer;
	unsigned             Indices;
};

//////////////////////////////////////////////////////////////////////////////

template <class INDEX>
#define INHERITED RRIndexedTriStripImporter<INDEX>
class RRIndexedTriListImporter : public INHERITED
{
public:
	RRIndexedTriListImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: RRIndexedTriStripImporter<INDEX>(vbuffer,vertices,stride,ibuffer,indices)
	{
		INDEX tmp = vertices;
		tmp = tmp;
		assert(tmp==vertices);
	}
	virtual unsigned getNumTriangles() const
	{
		return INHERITED::Indices/3;
	}
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
	{
		assert(t*3<INHERITED::Indices);
		assert(INHERITED::IBuffer);
		out[0] = INHERITED::IBuffer[t*3+0]; assert(out[0]<INHERITED::Vertices);
		out[1] = INHERITED::IBuffer[t*3+1]; assert(out[1]<INHERITED::Vertices);
		out[2] = INHERITED::IBuffer[t*3+2]; assert(out[2]<INHERITED::Vertices);
	}
	virtual void getTriangleBody(unsigned t, RRMeshImporter::TriangleBody& out) const
	{
		assert(t*3<INHERITED::Indices);
		assert(INHERITED::VBuffer);
		assert(INHERITED::IBuffer);
		unsigned v0,v1,v2;
		v0 = INHERITED::IBuffer[t*3+0]; assert(v0<INHERITED::Vertices);
		v1 = INHERITED::IBuffer[t*3+1]; assert(v1<INHERITED::Vertices);
		v2 = INHERITED::IBuffer[t*3+2]; assert(v2<INHERITED::Vertices);
#define VERTEX(v) ((RRReal*)(INHERITED::VBuffer+v*INHERITED::Stride))
		out.vertex0[0] = VERTEX(v0)[0];
		out.vertex0[1] = VERTEX(v0)[1];
		out.vertex0[2] = VERTEX(v0)[2];
		out.side1[0] = VERTEX(v1)[0]-out.vertex0[0];
		out.side1[1] = VERTEX(v1)[1]-out.vertex0[1];
		out.side1[2] = VERTEX(v1)[2]-out.vertex0[2];
		out.side2[0] = VERTEX(v2)[0]-out.vertex0[0];
		out.side2[1] = VERTEX(v2)[1]-out.vertex0[1];
		out.side2[2] = VERTEX(v2)[2]-out.vertex0[2];
#undef VERTEX
	}
};
#undef INHERITED


//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessVerticesImporter<IMPORTER,INDEX>  - importer filter that removes duplicate vertices
// RRLessTrianglesImporter<IMPORTER,INDEX> - importer filter that removes degenerate triangles

template <class INHERITED, class INDEX>
class RRLessVerticesImporter : public INHERITED
{
public:
	RRLessVerticesImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices, float MAX_STITCH_DISTANCE)
		: INHERITED(vbuffer,vertices,stride,ibuffer,indices)
	{
		INDEX tmp = vertices;
		assert(tmp==vertices);
		Dupl2Unique = new INDEX[vertices];
		Unique2Dupl = new INDEX[vertices];
		UniqueVertices = 0;
		for(unsigned d=0;d<vertices;d++)
		{
			RRMeshImporter::Vertex dfl;
			INHERITED::getVertex(d,dfl);
			for(unsigned u=0;u<UniqueVertices;u++)
			{
				RRMeshImporter::Vertex ufl;
				INHERITED::getVertex(Unique2Dupl[u],ufl);
//#define CLOSE(a,b) ((a)==(b))
#define CLOSE(a,b) (fabs((a)-(b))<=MAX_STITCH_DISTANCE)
				if(CLOSE(dfl[0],ufl[0]) && CLOSE(dfl[1],ufl[1]) && CLOSE(dfl[2],ufl[2])) 
				{
					Dupl2Unique[d] = u;
					goto dupl;
				}
			}
			Unique2Dupl[UniqueVertices] = d;
			Dupl2Unique[d] = UniqueVertices++;
dupl:;
		}
	}
	~RRLessVerticesImporter()
	{
		delete[] Unique2Dupl;
		delete[] Dupl2Unique;
	}

	virtual unsigned getNumVertices() const
	{
		return UniqueVertices;
	}
	virtual void getVertex(unsigned v, RRMeshImporter::Vertex& out) const
	{
		assert(v<UniqueVertices);
		assert(Unique2Dupl[v]<INHERITED::Vertices);
		assert(INHERITED::VBuffer);
		out = *(RRMeshImporter::Vertex*)(INHERITED::VBuffer+Unique2Dupl[v]*INHERITED::Stride);
	}
	virtual unsigned getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		assert(postImportVertex<UniqueVertices);

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned preImportTriangle = RRMeshImporter::getPreImportTriangle(postImportTriangle);
		RRMeshImporter::Triangle preImportVertices;
		INHERITED::getTriangle(preImportTriangle,preImportVertices);
		if(Dupl2Unique[preImportVertices[0]]==postImportVertex) return preImportVertices[0];
		if(Dupl2Unique[preImportVertices[1]]==postImportVertex) return preImportVertices[1];
		if(Dupl2Unique[preImportVertices[2]]==postImportVertex) return preImportVertices[2];
		assert(0);

		// fast version
		return Unique2Dupl[postImportVertex];
	}
	virtual unsigned getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		assert(preImportVertex<INHERITED::Vertices);
		return Dupl2Unique[preImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
	{
		INHERITED::getTriangle(t,out);
		out[0] = Dupl2Unique[out[0]]; assert(out[0]<UniqueVertices);
		out[1] = Dupl2Unique[out[1]]; assert(out[1]<UniqueVertices);
		out[2] = Dupl2Unique[out[2]]; assert(out[2]<UniqueVertices);
	}

protected:
	INDEX*               Unique2Dupl;    // small -> big number translation, one small number translates to one of many suitable big numbers
	INDEX*               Dupl2Unique;    // big -> small number translation
	unsigned             UniqueVertices; // small number of unique vertices, UniqueVertices<=INHERITED.getNumVertices()
};

//////////////////////////////////////////////////////////////////////////////

template <class INHERITED, class INDEX>
class RRLessTrianglesImporter : public INHERITED
{
public:
	RRLessTrianglesImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: INHERITED(vbuffer,vertices,stride,ibuffer,indices) 
	{
		ValidIndices = 0;
		unsigned numAllTriangles = INHERITED::getNumTriangles();
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMeshImporter::Triangle t;
			INHERITED::getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMeshImporter::Triangle t;
			INHERITED::getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndex[ValidIndices++] = i;
		}
	};
	~RRLessTrianglesImporter()
	{
		delete[] ValidIndex;
	}

	virtual unsigned getNumTriangles() const
	{
		return ValidIndices;
	}
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangle(ValidIndex[t],out);
	}
	virtual unsigned getPreImportTriangle(unsigned postImportTriangle) const 
	{
		return ValidIndex[postImportTriangle];
	}
	virtual unsigned getPostImportTriangle(unsigned preImportTriangle) const 
	{
		// efficient implementation would require another translation array
		for(unsigned post=0;post<ValidIndices;post++)
			if(ValidIndex[post]==preImportTriangle)
				return post;
		return UINT_MAX;
	}
	virtual void getTriangleBody(unsigned t, RRMeshImporter::TriangleBody& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangleBody(ValidIndex[t],out);
	}

protected:
	unsigned*            ValidIndex; // may be uint16_t when indices<64k
	unsigned             ValidIndices;
};


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

	// Save importer to file.
	// Check importer consistency during save so we don't have to store redundant data.
	bool save(const char* filename)
	{
		//!!!
		return false;
	}

	// Load importer from file.
	bool load(const char* filename)
	{
		//!!!
		return false;
	}

	// Load importer from another importer (create copy).
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
			numPreImportTriangles = MAX(numPreImportTriangles,t.preImportTriangle+1);
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
	virtual unsigned     getNumVertices() const
	{
		return static_cast<unsigned>(postImportVertices.size());
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		assert(v<postImportVertices.size());
		out = postImportVertices[v];
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return static_cast<unsigned>(postImportTriangles.size());
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
		{
			pack[1].getImporter()->getTriangle(t-pack[0].getNumTriangles(),out);
			out[0] += pack[0].getNumVertices();
			out[1] += pack[0].getNumVertices();
			out[2] += pack[0].getNumVertices();
		}
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
		assert(0);//!!! just mark that this code was not tested
		if(postImportVertex<pack[0].getNumVertices()) 
		{
			return pack[0].getImporter()->getPreImportVertex(postImportVertex, postImportTriangle);
		} else {
			PreImportNumber preImport = pack[1].getImporter()->getPreImportVertex(postImportVertex-pack[0].getNumVertices(), postImportTriangle-pack[0].getNumTriangles());
			assert(preImport.object<pack[1].getNumObjects());
			preImport.object += pack[0].getNumObjects();
			return preImport;
		}
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		assert(0);//!!! just mark that this code was not tested
		PreImportNumber preImportV = preImportVertex;
		PreImportNumber preImportT = preImportTriangle;
		if(preImportV.object<pack[0].getNumObjects()) 
		{
			return pack[0].getImporter()->getPostImportVertex(preImportV, preImportT);
		} else {
			preImportV.object -= pack[0].getNumObjects();
			preImportT.object -= pack[0].getNumObjects();
			assert(preImportV.object<pack[1].getNumObjects());
			assert(preImportT.object<pack[1].getNumObjects());
			return pack[0].getNumVertices() + pack[1].getImporter()->getPostImportVertex(preImportV, preImportT);
		}
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle<pack[0].getNumTriangles()) 
		{
			PreImportNumber preImport = pack[0].getImporter()->getPreImportTriangle(postImportTriangle);
			return preImport;
		} else {
			PreImportNumber preImport = pack[1].getImporter()->getPreImportTriangle(postImportTriangle-pack[0].getNumTriangles());
			assert(preImport.object<pack[1].getNumObjects());
			preImport.object += pack[0].getNumObjects();
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
			assert(preImport.object<pack[1].getNumObjects());
			return pack[0].getNumTriangles() + pack[1].getImporter()->getPostImportTriangle(preImport);
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

RRMeshImporter* RRMeshImporter::create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride)
{
	flags &= ~(OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES); // optimizations are not implemented for non-indexed
	switch(vertexFormat)
	{
		case FLOAT32:
			switch(flags)
			{
				case TRI_LIST: return new RRTriListImporter((char*)vertexBuffer,vertexCount,vertexStride);
				case TRI_STRIP: return new RRTriStripImporter((char*)vertexBuffer,vertexCount,vertexStride);
			}
			break;
	}
	return NULL;
}

RRMeshImporter* RRMeshImporter::createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance)
{
	unsigned triangleCount = (flags&TRI_STRIP)?vertexCount-2:(vertexCount/3);
	switch(vertexFormat)
	{
	case FLOAT32:
		switch(flags)
		{
			case TRI_LIST:
				switch(indexFormat)
				{
					case UINT8: return new RRIndexedTriListImporter<uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRIndexedTriListImporter<uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRIndexedTriListImporter<uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_STRIP:
				switch(indexFormat)
				{
					case UINT8: return new RRIndexedTriStripImporter<uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRIndexedTriStripImporter<uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRIndexedTriStripImporter<uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRIndexedTriListImporter<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRIndexedTriListImporter<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRIndexedTriListImporter<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRIndexedTriStripImporter<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRIndexedTriStripImporter<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRIndexedTriStripImporter<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_LIST|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRIndexedTriListImporter<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRIndexedTriListImporter<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRIndexedTriListImporter<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_STRIP|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRIndexedTriStripImporter<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRIndexedTriStripImporter<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRIndexedTriStripImporter<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriListImporter<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriListImporter<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriListImporter<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriStripImporter<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriStripImporter<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRIndexedTriStripImporter<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
		}
		break;
	}
	return NULL;
}

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

RRMeshImporter* RRMeshImporter::createOptimizedVertices(float vertexStitchMaxDistance)
{
	return this;//!!!new RRLessVerticesImporter<RRFilteredMeshImporter,unsigned>(this,vertexStitchMaxDistance);
}




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

void* RRAligned::operator new(std::size_t n)
{
	return AlignedMalloc(n,16);
};

void* RRAligned::operator new[](std::size_t n)
{
	return AlignedMalloc(n,16);
};

void RRAligned::operator delete(void* p, std::size_t n)
{
	AlignedFree(p);
};

void RRAligned::operator delete[](void* p, std::size_t n)
{
	AlignedFree(p);
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


//////////////////////////////////////////////////////////////////////////////
//
// License

void registerLicense(char* licenseOwner, char* licenseNumber)
{
}

} //namespace
