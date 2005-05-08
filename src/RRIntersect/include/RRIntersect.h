#ifndef RRINTERSECT_RRINTERSECT_H
#define RRINTERSECT_RRINTERSECT_H

//////////////////////////////////////////////////////////////////////////////
// RRIntersect - library for fast "ray x mesh" intersections
//
// - thread safe, you can calculate any number of intersections at the same time
// - builds helper-structures and stores them in cache on disk
//////////////////////////////////////////////////////////////////////////////

#include <assert.h>

namespace rrIntersect
{
	#ifdef _MSC_VER
	#pragma comment(lib,"RRIntersect.lib")
	#endif

	#define USE_BSP
	//#define USE_KD

	typedef unsigned TRIANGLE_HANDLE;
	typedef float    RRreal;


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
		virtual RRreal*      getVertex(unsigned i) const = 0;

		// triangles
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2) const = 0;

		// optional for faster access
		bool                 fastN   :1; // set true if you implement fast getTriangleN -> slower but much lower memory footprint Intersect may be used
		bool                 fastSRL :1;
		bool                 fastSRLN:1;
		struct TriangleN     {RRreal n[4];};
		struct TriangleSRL   {RRreal s[3],r[3],l[3];};
		struct TriangleSRLN  {RRreal s[3],r[3],l[3],n[4];};
		virtual void         getTriangleN(unsigned i, TriangleN* t) const;
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t) const;
		virtual void         getTriangleSRLN(unsigned i, TriangleSRLN* t) const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.

	struct RRRay
	{
		RRreal          rayOrigin[3];
		RRreal          rayDir[3];
		RRreal          hitDistanceMin,hitDistanceMax;
		RRreal          hitDistance;   // changed even when no hit
		RRreal          hitPoint3d[3]; // returned wrong
		RRreal          hitPoint2d[2];
		TRIANGLE_HANDLE skip;
		TRIANGLE_HANDLE hitTriangle;
		bool            hitOuterSide;
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
	// Basic Importers
	// - RRIndexedTriStripImporter 
	// - RRDuplVertTriStripImporter
	// - RRSparseTriStripImporter

	class RRIndexedTriStripImporter : virtual public rrIntersect::RRObjectImporter
	{
	public:
		RRIndexedTriStripImporter(char* vbuffer, unsigned vertices, unsigned stride, unsigned short* ibuffer, unsigned indices)
			: VBuffer(vbuffer), Vertices(vertices), Stride(stride), IBuffer(ibuffer), Indices(indices)
		{
		}

		virtual unsigned getNumVertices() const
		{
			return Vertices;
		}
		virtual float* getVertex(unsigned v) const
		{
			assert(v<Vertices);
			return (float*)(VBuffer+v*Stride);
		}
		virtual unsigned getPostImportVertex(unsigned preImportVertex) const 
		{
			return preImportVertex;
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
			#define VERTEX(v) ((float*)(VBuffer+v*Stride))
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
		unsigned short*      IBuffer;
		unsigned             Indices;
	};

	//****************************************************************************

	#define inherited RRIndexedTriStripImporter

	class RRDuplVertTriStripImporter : public inherited
	{
	public:
		RRDuplVertTriStripImporter(char* vbuffer, unsigned vertices, unsigned stride, unsigned short* ibuffer, unsigned indices)
			: inherited(vbuffer,vertices,stride,ibuffer,indices) 
		{
			Dupl2Unique = new unsigned short[vertices];
			Unique2Dupl = new unsigned short[vertices];
			UniqueVertices = 0;
			for(unsigned d=0;d<vertices;d++)
			{
				float* dfl = inherited::getVertex(d);
				for(unsigned u=0;u<UniqueVertices;u++)
				{
					float* ufl = inherited::getVertex(Unique2Dupl[u]);
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
		~RRDuplVertTriStripImporter()
		{
			delete[] Unique2Dupl;
			delete[] Dupl2Unique;
		}

		virtual unsigned getNumVertices() const
		{
			return UniqueVertices;
		}
		virtual float* getVertex(unsigned v) const
		{
			assert(v<UniqueVertices);
			assert(Unique2Dupl[v]<Vertices);
			return (float*)(VBuffer+Unique2Dupl[v]*Stride);
		}
		virtual unsigned getPostImportVertex(unsigned preImportVertex) const
		{
			assert(preImportVertex<Vertices);
			return Dupl2Unique[preImportVertex];
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			inherited::getTriangle(t,v0,v1,v2);
			v0 = Dupl2Unique[v0]; assert(v0<UniqueVertices);
			v1 = Dupl2Unique[v1]; assert(v1<UniqueVertices);
			v2 = Dupl2Unique[v2]; assert(v2<UniqueVertices);
		}

	protected:
		unsigned short*      Unique2Dupl;
		unsigned short*      Dupl2Unique;
		unsigned             UniqueVertices;
	};

	#undef inherited

	//****************************************************************************

	#define inherited RRDuplVertTriStripImporter

	class RRSparseTriStripImporter : public inherited
	{
	public:
		RRSparseTriStripImporter(char* vbuffer, unsigned vertices, unsigned stride, unsigned short* ibuffer, unsigned indices)
			: inherited(vbuffer,vertices,stride,ibuffer,indices) 
		{
			ValidIndices = 0;
			for(unsigned i=0;i<Indices-2;i++)
			{
				unsigned v0,v1,v2;
				inherited::getTriangle(i,v0,v1,v2);
				if(!(v0==v1 || v0==v2 || v1==v2)) ValidIndices++;
			}
			ValidIndex = new unsigned short[ValidIndices];
			ValidIndices = 0;
			for(unsigned i=0;i<Indices-2;i++)
			{
				unsigned v0,v1,v2;
				inherited::getTriangle(i,v0,v1,v2);
				if(!(v0==v1 || v0==v2 || v1==v2)) ValidIndex[ValidIndices++] = i;
			}
		};
		~RRSparseTriStripImporter()
		{
			delete[] ValidIndex;
		}

		// must not change during object lifetime
		virtual unsigned getNumTriangles() const
		{
			return ValidIndices;
		}
		virtual void getTriangle(unsigned t, unsigned& v0, unsigned& v1, unsigned& v2) const
		{
			assert(t<ValidIndices);
			inherited::getTriangle(ValidIndex[t],v0,v1,v2);
		}

	protected:
		unsigned short*      ValidIndex;
		unsigned             ValidIndices;
	};

	#undef inherited

}

#endif
