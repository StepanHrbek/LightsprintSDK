#ifndef RRINTERSECT_RRINTERSECT_H
#define RRINTERSECT_RRINTERSECT_H

//////////////////////////////////////////////////////////////////////////////
// RRIntersect - library for fast "ray x mesh" intersections
// version 2005.06.16
// http://dee.cz/rr
//
// - thread safe, you can calculate any number of intersections at the same time
// - builds helper-structures and stores them in cache on disk
//
// Copyright (C) Stepan Hrbek 1999-2005, Daniel Sykora 1999-2004
// This work is protected by copyright law, 
// using it without written permission from Stepan Hrbek is forbidden.
//////////////////////////////////////////////////////////////////////////////

#include <assert.h>

#ifdef _MSC_VER
#pragma comment(lib,"RRIntersect.lib")
#endif

namespace rrIntersect
{

	typedef float RRReal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRObjectImporter - abstract class for importing your data into RRObject.
	//
	// Derive to import YOUR geometry.
	// Derive from RRSceneObjectImporter if you want to calculate also radiosity.
	// Data must not change during object lifetime, all results must be constant.

	class RRObjectImporter
	{
	public:
		RRObjectImporter();
		virtual ~RRObjectImporter() {};

		// vertices
		virtual unsigned     getNumVertices() const = 0;
		virtual RRReal*      getVertex(unsigned v) const = 0;
		virtual unsigned     getPreImportVertex(unsigned postImportVertex) const {return postImportVertex;}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex) const {return preImportVertex;}

		// triangles
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const = 0;
		virtual unsigned     getPreImportTriangle(unsigned postImportTraingle) const {return postImportTraingle;}
		virtual unsigned     getPostImportTraingle(unsigned preImportTraingle) const {return preImportTraingle;}

		// optional for faster access
		bool                 fastN   :1; // set true if you implement fast getTriangleN -> slower but much lower memory footprint Intersect may be used
		bool                 fastSRL :1;
		bool                 fastSRLN:1;
		struct TriangleN     {RRReal n[4];};
		struct TriangleSRL   {RRReal s[3],r[3],l[3];};
		struct TriangleSRLN  {RRReal s[3],r[3],l[3],n[4];};
		virtual void         getTriangleN(unsigned i, TriangleN* t) const;
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t) const;
		virtual void         getTriangleSRLN(unsigned i, TriangleSRLN* t) const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.

	struct RRRay
	{
		RRReal          rayOrigin[3];   // i, ray origin
		RRReal          rayDir[3];      // i, ray direction, must be normalized
		RRReal          hitDistanceMin; // i, test hit in range <min,max>
		RRReal          hitDistanceMax; // i
		RRReal          hitDistance;    // o, hit -> hit distance, otherwise undefined
		RRReal          hitPoint3d[3];  // o, undefined
		RRReal          hitPoint2d[2];  // o, hit -> hit coordinate in triangle space
		unsigned        skipTriangle;   // i, postImportTriangle to be skipped, 
		unsigned        hitTriangle;    // o, hit -> postImportTriangle that was hit
		bool            hitOuterSide;   // o, hit -> false when object was hit from the inner side
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersect - single object able to calculate intersections.

	class RRIntersect
	{
	public:
		virtual bool intersect(RRRay* ray) const = 0;
		virtual ~RRIntersect() {};
	};

	RRIntersect* newIntersect(RRObjectImporter* importer);


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRIntersectStats
	{
	public:
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
		unsigned intersect_kd;
		unsigned intersect_linear;
		// branches, many times per call
		unsigned intersect_triangleSRLNP;
		unsigned intersect_triangleNP;
		unsigned intersect_triangleP;
		// helper
		void getInfo(char *buf, unsigned len, unsigned level) const;
	};

	extern RRIntersectStats intersectStats;


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

	class RRTriStripImporter : virtual public RRObjectImporter
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
		}

		virtual unsigned getNumTriangles() const
		{
			return Indices-2;
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t<Indices-2);
			v0 = IBuffer[t];         assert(v0<Vertices);
			v1 = IBuffer[t+1+(t%2)]; assert(v1<Vertices);
			v2 = IBuffer[t+2-(t%2)]; assert(v2<Vertices);
		}
		virtual void getTriangleSRL(unsigned t, TriangleSRL* tr) const
		{
			assert(t<Indices-2);
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
		}
		virtual unsigned getNumTriangles() const
		{
			return INHERITED::Indices/3;
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t*3<Indices);
			v0 = INHERITED::IBuffer[t*3+0]; assert(v0<INHERITED::Vertices);
			v1 = INHERITED::IBuffer[t*3+1]; assert(v1<INHERITED::Vertices);
			v2 = INHERITED::IBuffer[t*3+2]; assert(v2<INHERITED::Vertices);
		}
		virtual void getTriangleSRL(unsigned t, RRObjectImporter::TriangleSRL* tr) const
		{
			assert(t*3<Indices);
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
			for(unsigned i=0;i<INHERITED::Indices-2;i++)
			{
				unsigned v0,v1,v2;
				INHERITED::getTriangle(i,v0,v1,v2);
				if(!(v0==v1 || v0==v2 || v1==v2)) ValidIndices++;
			}
			ValidIndex = new INDEX[ValidIndices];
			ValidIndices = 0;
			for(unsigned i=0;i<INHERITED::Indices-2;i++)
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
		//virtual unsigned getPostImportTraingle(unsigned preImportTraingle) const 
		//{
		//}

	protected:
		INDEX*               ValidIndex;
		unsigned             ValidIndices;
	};

} // namespace

#endif
