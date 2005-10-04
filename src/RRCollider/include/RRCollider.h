#ifndef _COLLIDER_H_
#define _COLLIDER_H_

//////////////////////////////////////////////////////////////////////////////
// RRCollider - library for fast "ray x mesh" intersections
// version 2005.10.4
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

//////////////////////////////////////////////////////////////////////////////
// General rules
//
// If not otherwise specified, all inputs must be finite numbers.
// With Inf or NaN, result is undefined.
//
// All parameters that need to be destructed are destructed by caller.
// So if you forget to destruct, there's risk of leak, never risk of double destruction.
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRCOLLIDER_EXPORT
		// build dll
		#define RRCOLLIDER_API __declspec(dllexport)
	#else
	#ifdef RRCOLLIDER_IMPORT
		// use dll
		#define RRCOLLIDER_API __declspec(dllimport)
		#pragma comment(lib,"RRCollider.lib")
	#else
		// use static library
		#define RRCOLLIDER_API
		#pragma comment(lib,"RRCollider.lib")
	#endif
	#endif
#else
	// use static library
	#define RRCOLLIDER_API
#endif

#include <assert.h>
#include <limits.h>
#include <memory.h>
#include <new>

namespace rrCollider
{

	typedef float RRReal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRMeshImporter - abstract class for importing your mesh data into RR.
	//
	// Derive to import YOUR triangle mesh.
	// All results from RRMeshImporter must be constant in time.

	class RRCOLLIDER_API RRMeshImporter
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRMeshImporter() {}

		// vertices
		struct Vertex        {RRReal m[3]; RRReal&operator[](int i){return m[i];} const RRReal&operator[](int i)const{return m[i];}};
		virtual unsigned     getNumVertices() const = 0;
		virtual void         getVertex(unsigned v, Vertex& out) const = 0;

		// triangles
		struct Triangle      {unsigned m[3]; unsigned&operator[](int i){return m[i];} const unsigned&operator[](int i)const{return m[i];}};
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned t, Triangle& out) const = 0;

		// optional for faster access
		struct TriangleBody  {Vertex vertex0,side1,side2;};
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

		// optional for advanced importers
		//  post import number is always plain unsigned, 0..num-1
		//  pre import number is implementation defined
		//  all numbers in interface are post import, except for following preImportXxx:
		virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return postImportVertex;}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const {return preImportVertex;}
		virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const {return postImportTriangle;}
		virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const {return preImportTriangle;}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		bool                   save(char* filename);
		static RRMeshImporter* load(char* filename);
		static RRMeshImporter* createMultiMesh(RRMeshImporter* const* meshes, unsigned numMeshes);
		RRMeshImporter*        createCopy();
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRMeshSurfaceImporter - defines non-trivial behaviour of mesh surfaces
	//
	// Derive to define behaviour of YOUR surfaces.
	// You need it only when you want to
	// - intersect mesh that contains both singlesided and twosided faces
	// - iterate over multiple intersections with mesh
	//
	// It's intentionally not part of RRMeshImporter, so you can easily combine
	// different surface behaviours with one geometry.

	class RRCOLLIDER_API RRMeshSurfaceImporter
	{
	public:
		virtual ~RRMeshSurfaceImporter() {}

		// acceptHit is called at each intersection with mesh
		// return false to continue to next intersection, true to end
		// for IT_BSP techniques, intersections are reported in order from the nearest one
		// for IT_LINEAR technique, intersections go unsorted
		virtual bool         acceptHit(const class RRRay* ray) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRAligned
	//
	// On some platforms, some structures need to be specially aligned in memory.
	// This helper base class helps to align them.

	struct RRCOLLIDER_API RRAligned
	{
		void* operator new(std::size_t n);
		void* operator new[](std::size_t n);
		void operator delete(void* p, std::size_t n);
		void operator delete[](void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.
	//
	// Contains all inputs and outputs for RRCollider::intersect()
	// All fields of at least 3 floats are aligned.

	class RRCOLLIDER_API RRRay : public RRAligned
	{
	public:
		// create ray
		static RRRay* create(); // all is zeroed, all FILL flags on
		static RRRay* create(unsigned n); // creates array
		// inputs (never modified by collider)
		enum Flags
		{ 
			FILL_DISTANCE   =(1<<0), // which outputs to fill
			FILL_POINT3D    =(1<<1), // note: some outputs might be filled even when not requested by flag.
			FILL_POINT2D    =(1<<2),
			FILL_PLANE      =(1<<3),
			FILL_TRIANGLE   =(1<<4),
			FILL_SIDE       =(1<<5),
			TEST_SINGLESIDED=(1<<6), // detect collision only against outer side. default is to test both sides
		};
		RRReal          rayOrigin[4];   // i, (-Inf,Inf), ray origin. never modify last component, must stay 1
		RRReal          rayDirInv[4];   // i, <-Inf,Inf>, 1/ray direction. direction must be normalized
		RRReal          rayLengthMin;   // i, <0,Inf), test intersection in range <min,max>
		RRReal          rayLengthMax;   // i, <0,Inf), test intersection in range <min,max>
		unsigned        rayFlags;       // i, flags that specify the action
		RRMeshSurfaceImporter* surfaceImporter; // i, optional surface importer for user-defined surface behaviours
		// outputs (valid after positive test, undefined otherwise)
		RRReal          hitDistance;    // o, hit distance in object space
		unsigned        hitTriangle;    // o, triangle (postImport) that was hit
		RRReal          hitPoint2d[2];  // o, hit coordinate in triangle space (vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1)
		RRReal          hitPlane[4];    // o, plane of hitTriangle in object space, [0..2] is normal
		RRReal          hitPoint3d[3];  // o, hit coordinate in object space
		bool            hitOuterSide;   // o, true = object was hit from the outer (common) side
		RRReal          hitPadding[8];  // o, undefined, never modify
	private:
		RRRay(); // intentionally private so one can't accidentally create unaligned instance
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRCollider - single object able to calculate ray x trimesh intersections.
	//
	// Mesh is defined by importer passed to create().
	// All results from importer must be constant in time, 
	// otherwise collision results are undefined.

	class RRCOLLIDER_API RRCollider
	{
	public:
		// create
		enum IntersectTechnique
		{
			IT_LINEAR,          // speed   1%, size  0
			IT_BSP_COMPACT,     // speed 100%, size  5 bytes per triangle
			IT_BSP_FAST,        // speed 175%, size 31
			IT_BSP_FASTEST,     // speed 200%, size 58
			IT_VERIFICATION,    // test using all techniques and compare
		};
		static RRCollider*   create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation=NULL, void* buildParams=0);

		// calculate intersections
		// (When intersection is detected, ray outputs are filled and true returned.
		// When no intersection is detected, ray outputs are undefined and false returned.)
		virtual bool         intersect(RRRay* ray) const = 0;

		// helpers
		virtual RRMeshImporter*    getImporter() const = 0; // identical to importer from create()
		virtual IntersectTechnique getTechnique() const = 0; // my differ from technique requested in create()
		virtual unsigned           getMemoryOccupied() const = 0; // total amount of system memory occupied by collider
		virtual ~RRCollider() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRCOLLIDER_API RRIntersectStats
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
				Vertex dfl;
				INHERITED::getVertex(d,dfl);
				for(unsigned u=0;u<UniqueVertices;u++)
				{
					Vertex ufl;
					INHERITED::getVertex(Unique2Dupl[u],ufl);
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
		virtual void getVertex(unsigned v, RRMeshImporter::Vertex& out) const
		{
			assert(v<UniqueVertices);
			assert(Unique2Dupl[v]<INHERITED::Vertices);
			assert(INHERITED::VBuffer);
			out = *(Vertex*)(INHERITED::VBuffer+Unique2Dupl[v]*INHERITED::Stride);
		}
		virtual unsigned getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
		{
			assert(postImportVertex<UniqueVertices);

			// exact version
			// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
			// use postImportTriangle to fully specify which one preImportVertex to return
			unsigned preImportTriangle = getPreImportTriangle(postImportTriangle);
			Triangle preImportVertices;
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
				RRMeshImporter::Triangle t;
				INHERITED::getTriangle(i,t);
				if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndices++;
			}
			ValidIndex = new INDEX[ValidIndices];
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
			for(unsigned post=0;post<ValidIndices-2;post++)
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
		INDEX*               ValidIndex;
		unsigned             ValidIndices;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// Base class for mesh import filters.

	class RRFilteredMeshImporter : public RRMeshImporter
	{
	public:
		RRFilteredMeshImporter(const RRMeshImporter* mesh)
		{
			importer = mesh;
			numVertices = importer ? importer->getNumVertices() : 0;
			numTriangles = importer ? importer->getNumTriangles() : 0;
		}
		virtual ~RRFilteredMeshImporter()
		{
			// Delete only what was created by us, not params.
			// So don't delete importer.
		}

		// vertices
		virtual unsigned     getNumVertices() const
		{
			return numVertices;
		}
		virtual void         getVertex(unsigned v, Vertex& out) const
		{
			importer->getVertex(v,out);
		}

		// triangles
		virtual unsigned     getNumTriangles() const
		{
			return numTriangles;
		}
		virtual void         getTriangle(unsigned t, Triangle& out) const
		{
			importer->getTriangle(t,out);
		}

		// preimport/postimport conversions
		virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
		{
			return importer->getPreImportVertex(postImportVertex, postImportTriangle);
		}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
		{
			return importer->getPostImportVertex(preImportVertex, preImportTriangle);
		}
		virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
		{
			return importer->getPreImportTriangle(postImportTriangle);
		}
		virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
		{
			return importer->getPostImportTriangle(preImportTriangle);
		}

	protected:
		const RRMeshImporter* importer;
		unsigned        numVertices;
		unsigned        numTriangles;
	};

} // namespace

#endif
