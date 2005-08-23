#ifndef _RRINTERSECT_H_
#define _RRINTERSECT_H_


//////////////////////////////////////////////////////////////////////////////
// RRIntersect - library for fast "ray x mesh" intersections
// version 2005.08.22
// http://dee.cz/rr
//
// - thread safe, you can calculate any number of intersections at the same time
// - you can select technique in range from maximal speed to zero memory allocated
// - up to 2^32 vertices and 2^30 triangles in mesh
// - builds helper-structures and stores them in cache on disk
//
// Copyright (C) Stepan Hrbek 1999-2005, Daniel Sykora 1999-2004
// This work is protected by copyright law, 
// using it without written permission from Stepan Hrbek is forbidden.
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRINTERSECT_EXPORT
		// build dll
		#define RRINTERSECT_API __declspec(dllexport)
	#elif RRINTERSECT_IMPORT
		// use dll
		#define RRINTERSECT_API __declspec(dllimport)
		#pragma comment(lib,"RRIntersect.lib")
	#else
		// use static library
		#define RRINTERSECT_API
		#pragma comment(lib,"RRIntersect.lib")
	#endif
#else
	// use static library
	#define RRINTERSECT_API
#endif

#include <assert.h>
#include <limits.h>
#include <new>

namespace rrIntersect
{

	typedef float RRReal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRMeshImporter - abstract class for importing your mesh data into RR.
	//
	// Derive to import YOUR geometry.
	// Derive from RRObjectImporter if you want to calculate also radiosity.
	// Data must not change during object lifetime, all results must be constant.

	class RRINTERSECT_API RRMeshImporter
	{
	public:
		RRMeshImporter() {}
		virtual ~RRMeshImporter() {}

		// vertices
		virtual unsigned     getNumVertices() const = 0;
		virtual RRReal*      getVertex(unsigned v) const = 0;

		// triangles
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const = 0;

		// optional for advanced importers
		virtual unsigned     getPreImportVertex(unsigned postImportVertex) const {return postImportVertex;}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex) const {return preImportVertex;}
		virtual unsigned     getPreImportTriangle(unsigned postImportTraingle) const {return postImportTraingle;}
		virtual unsigned     getPostImportTraingle(unsigned preImportTraingle) const {return preImportTraingle;}

		// optional for faster access
		struct TriangleSRL   {RRReal s[3],r[3],l[3];};
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t) const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRAligned
	//
	// On some platforms (x86+SSE), some structures need to be specially aligned in memory.
	// This helper base class helps to align them.

	struct RRINTERSECT_API RRAligned
	{
		RRAligned();
		void* operator new(std::size_t n);
		void operator delete(void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.

	struct RRINTERSECT_API RRRay : public RRAligned
	{
		// create ray
		static RRRay* create();
		// inputs
		enum Flags
		{ 
			FILL_DISTANCE   =(1<<0), // what outputs to fill
			FILL_POINT3D    =(1<<1), // note: some outputs might be filled even when not requested by flag.
			FILL_POINT2D    =(1<<2),
			FILL_PLANE      =(1<<3),
			FILL_TRIANGLE   =(1<<4),
			FILL_SIDE       =(1<<5),
			TEST_SINGLESIDED=(1<<6), // detect collision only against outer side. default is to test both sides
			SKIP_PRETESTS   =(1<<7), // skip bounding volume pretests
		};
		RRReal          rayOrigin[3];   // i, ray origin [ALIGN16]
		unsigned        skipTriangle;   // i, postImportTriangle to be skipped, not tested
		RRReal          rayDir[3];      // i, ray direction, must be normalized [ALIGN16]
		unsigned        flags;          // i, flags that specify the action
		RRReal          hitDistanceMin; // io, test hit in range <min,max>, undefined after test
		RRReal          hitDistanceMax; // io, test hit in range <min,max>, undefined after test
		// outputs
		RRReal          hitDistance;    // o, hit -> hit distance, !hit -> undefined
		RRReal          hitPoint3d[3];  // o, hit -> hit coordinate in object space; !hit -> undefined
		RRReal          hitPoint2d[2];  // o, hit -> hit coordinate in triangle space
		RRReal          hitPlane[4];    // o, hit -> plane of hitTriangle, [0..2] is normal
		unsigned        hitTriangle;    // o, hit -> postImportTriangle that was hit
		bool            hitOuterSide;   // o, hit -> false when object was hit from the inner side
	private:
		RRRay() {} // intentionally private so no one is able to create unaligned instance
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersect - single object able to calculate intersections.

	class RRINTERSECT_API RRIntersect
	{
	public:
		// create
		enum IntersectTechnique
		{
			IT_LINEAR,          // speed   1%, size  0
			IT_BSP_COMPACT,     // speed 100%, size  5 bytes per triangle
			IT_BSP_FAST,        // speed 175%, size 31
			IT_BSP_FASTEST,     // speed 200%, size 58
		};
		static RRIntersect*  create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, void* buildParams=0);

		// calculate intersections
		virtual bool         intersect(RRRay* ray) const = 0;

		// helpers
		virtual RRMeshImporter*  getImporter() const = 0;
		virtual IntersectTechnique getTechnique() const = 0;
		virtual unsigned           getMemoryOccupied() const = 0;
		virtual ~RRIntersect() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRINTERSECT_API RRIntersectStats
	{
	public:
		static RRIntersectStats* getInstance();
		RRIntersectStats();
		// data
		unsigned loaded_triangles;
		unsigned invalid_triangles;
		// calls
		unsigned intersects;
		unsigned hits;
		// branches, once per call
		unsigned intersect_bspSRLNP;
		unsigned intersect_bspNP;
		unsigned intersect_bsp;
		unsigned intersect_kd;
		unsigned intersect_linear;
		// branches, many times per call
		unsigned intersect_triangleSRLNP;
		unsigned intersect_triangleNP;
		unsigned intersect_triangle;
		// robustness
		unsigned diff_overlap;
		unsigned diff_precalc_miss;
		unsigned diff_tight_hit;
		unsigned diff_tight_miss;
		unsigned diff_parallel_hit;
		unsigned diff_parallel_miss;
		unsigned diff_clear_hit;
		unsigned diff_clear_miss_tested;
		unsigned diff_clear_miss_not_tested;
		// helper
		void getInfo(char *buf, unsigned len, unsigned level) const;
	};


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

	class RRTriStripImporter : virtual public RRMeshImporter
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
		virtual RRReal* getVertex(unsigned v) const
		{
			assert(v<Vertices);
			assert(VBuffer);
			return (RRReal*)(VBuffer+v*Stride);
		}
		virtual unsigned getNumTriangles() const
		{
			return Vertices-2;
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t<Vertices-2);
			v0 = t+0;
			v1 = t+1;
			v2 = t+2;
		}
		virtual void getTriangleSRL(unsigned t, TriangleSRL* tr) const
		{
			assert(t<Vertices-2);
			assert(VBuffer);
			unsigned v0,v1,v2;
			v0 = t+0;
			v1 = t+1;
			v2 = t+2;
			#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
			tr->s[0] = VERTEX(v0)[0];
			tr->s[1] = VERTEX(v0)[1];
			tr->s[2] = VERTEX(v0)[2];
			tr->r[0] = VERTEX(v1)[0]-tr->s[0];
			tr->r[1] = VERTEX(v1)[1]-tr->s[1];
			tr->r[2] = VERTEX(v1)[2]-tr->s[2];
			tr->l[0] = VERTEX(v2)[0]-tr->s[0];
			tr->l[1] = VERTEX(v2)[1]-tr->s[1];
			tr->l[2] = VERTEX(v2)[2]-tr->s[2];
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
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t*3<Vertices);
			v0 = t*3+0;
			v1 = t*3+1;
			v2 = t*3+2;
		}
		virtual void getTriangleSRL(unsigned t, TriangleSRL* tr) const
		{
			assert(t*3<Vertices);
			assert(VBuffer);
			unsigned v0,v1,v2;
			v0 = t*3+0;
			v1 = t*3+1;
			v2 = t*3+2;
			#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
			tr->s[0] = VERTEX(v0)[0];
			tr->s[1] = VERTEX(v0)[1];
			tr->s[2] = VERTEX(v0)[2];
			tr->r[0] = VERTEX(v1)[0]-tr->s[0];
			tr->r[1] = VERTEX(v1)[1]-tr->s[1];
			tr->r[2] = VERTEX(v1)[2]-tr->s[2];
			tr->l[0] = VERTEX(v2)[0]-tr->s[0];
			tr->l[1] = VERTEX(v2)[1]-tr->s[1];
			tr->l[2] = VERTEX(v2)[2]-tr->s[2];
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
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t<Indices-2);
			assert(IBuffer);
			v0 = IBuffer[t];         assert(v0<Vertices);
			v1 = IBuffer[t+1+(t%2)]; assert(v1<Vertices);
			v2 = IBuffer[t+2-(t%2)]; assert(v2<Vertices);
		}
		virtual void getTriangleSRL(unsigned t, TriangleSRL* tr) const
		{
			assert(t<Indices-2);
			assert(VBuffer);
			assert(IBuffer);
			unsigned v0,v1,v2;
			v0 = IBuffer[t];         assert(v0<Vertices);
			v1 = IBuffer[t+1+(t%2)]; assert(v1<Vertices);
			v2 = IBuffer[t+2-(t%2)]; assert(v2<Vertices);
			#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
			tr->s[0] = VERTEX(v0)[0];
			tr->s[1] = VERTEX(v0)[1];
			tr->s[2] = VERTEX(v0)[2];
			tr->r[0] = VERTEX(v1)[0]-tr->s[0];
			tr->r[1] = VERTEX(v1)[1]-tr->s[1];
			tr->r[2] = VERTEX(v1)[2]-tr->s[2];
			tr->l[0] = VERTEX(v2)[0]-tr->s[0];
			tr->l[1] = VERTEX(v2)[1]-tr->s[1];
			tr->l[2] = VERTEX(v2)[2]-tr->s[2];
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
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t*3<INHERITED::Indices);
			assert(INHERITED::IBuffer);
			v0 = INHERITED::IBuffer[t*3+0]; assert(v0<INHERITED::Vertices);
			v1 = INHERITED::IBuffer[t*3+1]; assert(v1<INHERITED::Vertices);
			v2 = INHERITED::IBuffer[t*3+2]; assert(v2<INHERITED::Vertices);
		}
		virtual void getTriangleSRL(unsigned t, RRMeshImporter::TriangleSRL* tr) const
		{
			assert(t*3<INHERITED::Indices);
			assert(INHERITED::VBuffer);
			assert(INHERITED::IBuffer);
			unsigned v0,v1,v2;
			v0 = INHERITED::IBuffer[t*3+0]; assert(v0<INHERITED::Vertices);
			v1 = INHERITED::IBuffer[t*3+1]; assert(v1<INHERITED::Vertices);
			v2 = INHERITED::IBuffer[t*3+2]; assert(v2<INHERITED::Vertices);
			#define VERTEX(v) ((RRReal*)(INHERITED::VBuffer+v*INHERITED::Stride))
			tr->s[0] = VERTEX(v0)[0];
			tr->s[1] = VERTEX(v0)[1];
			tr->s[2] = VERTEX(v0)[2];
			tr->r[0] = VERTEX(v1)[0]-tr->s[0];
			tr->r[1] = VERTEX(v1)[1]-tr->s[1];
			tr->r[2] = VERTEX(v1)[2]-tr->s[2];
			tr->l[0] = VERTEX(v2)[0]-tr->s[0];
			tr->l[1] = VERTEX(v2)[1]-tr->s[1];
			tr->l[2] = VERTEX(v2)[2]-tr->s[2];
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
		RRLessVerticesImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
			: INHERITED(vbuffer,vertices,stride,ibuffer,indices)
		{
			INDEX tmp = vertices;
			assert(tmp==vertices);
			Dupl2Unique = new INDEX[vertices];
			Unique2Dupl = new INDEX[vertices];
			UniqueVertices = 0;
			for(unsigned d=0;d<vertices;d++)
			{
				RRReal* dfl = INHERITED::getVertex(d);
				for(unsigned u=0;u<UniqueVertices;u++)
				{
					RRReal* ufl = INHERITED::getVertex(Unique2Dupl[u]);
					if(dfl[0]==ufl[0] && dfl[1]==ufl[1] && dfl[2]==ufl[2]) 
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
		virtual RRReal* getVertex(unsigned v) const
		{
			assert(v<UniqueVertices);
			assert(Unique2Dupl[v]<INHERITED::Vertices);
			assert(INHERITED::VBuffer);
			return (RRReal*)(INHERITED::VBuffer+Unique2Dupl[v]*INHERITED::Stride);
		}
		virtual unsigned getPreImportVertex(unsigned postImportVertex) const
		{
			assert(postImportVertex<UniqueVertices);
			return Unique2Dupl[postImportVertex];
		}
		virtual unsigned getPostImportVertex(unsigned preImportVertex) const
		{
			assert(preImportVertex<INHERITED::Vertices);
			return Dupl2Unique[preImportVertex];
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			INHERITED::getTriangle(t,v0,v1,v2);
			v0 = Dupl2Unique[v0]; assert(v0<UniqueVertices);
			v1 = Dupl2Unique[v1]; assert(v1<UniqueVertices);
			v2 = Dupl2Unique[v2]; assert(v2<UniqueVertices);
		}

	protected:
		INDEX*               Unique2Dupl;
		INDEX*               Dupl2Unique;
		unsigned             UniqueVertices;
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
				unsigned v0,v1,v2;
				INHERITED::getTriangle(i,v0,v1,v2);
				if(!(v0==v1 || v0==v2 || v1==v2)) ValidIndices++;
			}
			ValidIndex = new INDEX[ValidIndices];
			ValidIndices = 0;
			for(unsigned i=0;i<numAllTriangles;i++)
			{
				unsigned v0,v1,v2;
				INHERITED::getTriangle(i,v0,v1,v2);
				if(!(v0==v1 || v0==v2 || v1==v2)) ValidIndex[ValidIndices++] = i;
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
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t<ValidIndices);
			INHERITED::getTriangle(ValidIndex[t],v0,v1,v2);
		}
		virtual unsigned getPreImportTriangle(unsigned postImportTraingle) const 
		{
			return ValidIndex[postImportTraingle];
		}
		virtual unsigned getPostImportTraingle(unsigned preImportTriangle) const 
		{
			// efficient implementation would require another translation array
			for(unsigned post=0;post<ValidIndices-2;post++)
				if(ValidIndex[post]==preImportTriangle)
					return post;
			return UINT_MAX;
		}
		virtual void getTriangleSRL(unsigned t, RRMeshImporter::TriangleSRL* tr) const
		{
			assert(t<ValidIndices);
			INHERITED::getTriangleSRL(ValidIndex[t],tr);
		}

	protected:
		INDEX*               ValidIndex;
		unsigned             ValidIndices;
	};

} // namespace

#endif
