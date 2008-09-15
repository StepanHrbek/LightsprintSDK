// --------------------------------------------------------------------------
// Acceleration structures for ray-mesh intersections.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_INTERSECTBSP_H
#define COLLIDER_INTERSECTBSP_H

#define TREE_VERSION 4 // version of tree format, increase when format changes

#define IBP <class BspTree>
#define IBP2 <BspTree>

#define MIN_BYTES_FOR_SAVE 1024 // smaller trees are not saved to disk, build is faster than disk operation
#define SUPPORT_EMPTY_KDNODE // only FASTEST: typically tiny, rarely big speedup (bunny); typically a bit, rarely much slower and memory hungry build (soda)
//#define BAKED_TRIANGLE

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

#include "bsp.h"
#include "cache.h"
#include "IntersectLinear.h"
#include "../LicGen.h"
#include "sha1.h"

namespace rr
{


	// single-level bps
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree1
	{
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=0, allows_kd=1, allows_emptykd=1};
		typedef const BspTree1<Ofs,TriInfo,Lo> This;
		typedef This Transitioneer;
		typedef This Son;
		struct BspData
		{
			enum {transition=0};
			Ofs               size:sizeof(Ofs)*8-3;
			Ofs               kd:1;
			Ofs               front:1;
			Ofs               back:1;
			Son*              getFrontAdr()    const {return (Son*)(this+1);}
			Son*              getFront()       const {return front?getFrontAdr():0;}
			Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->bsp.size:0));}
			Son*              getBack()        const {return back?getBackAdr():0;}
			const TriInfo*    getTrianglesBegin() const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->bsp.size:0));}
		};
		struct KdData
		{
			enum {transition=0};
			Ofs               size:sizeof(Ofs)*8-3;
			Ofs               kd:1;
			Ofs               splitAxis:2;
			bool              isLeaf()         const {return splitAxis==3;}
			Son*              getFront()       const {return (Son*)((char*)this+sizeof(*this)+sizeof(real));}
			Son*              getBack()        const {return getFront()->getNext();}
			real              getSplitValue()  const {return *(real*)(this+1);}
			void              setSplitValue(real r)  {*(real*)(this+1)=r;}
			TriInfo*          getTrianglesBegin() const {return (TriInfo*)(this+1);}
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
		void*             getTrianglesEnd()const {return (char*)this+bsp.size;}
		void              setTransition(bool t) {}
		void              setKd(bool k)         {RR_ASSERT(allows_kd || !k);kd.kd=k;}
		bool              contains(TriInfo tri) const
		{
			if (bsp.kd==1)
				return kd.getFront()->contains(tri) || kd.getBack()->contains(tri);
			for (const TriInfo* t=bsp.getTrianglesBegin();t<getTrianglesEnd();t++)
				if (*t==tri) return true;
			return (bsp.front && bsp.getFront()->contains(tri)) 
				|| (bsp.back && bsp.getBack()->contains(tri));
		}
	};

	// multi-level bsp (with transition in this node)
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2T
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=0, allows_kd=1, allows_emptykd=0};
		typedef const BspTree2T<Ofs,TriInfo,Lo> This;
		typedef const This Transitioneer;
		typedef const Lo Son;
		struct BspData
		{
			enum {transition=1};
			Ofs               size:sizeof(Ofs)*8-4;
			Ofs               kd:1;
			Ofs               _transition:1;
			Ofs               front:1;
			Ofs               back:1;
			//Son*              getFrontAdr()    const {return (Son*)(this+1);}
			//Son*              getFront()       const {return front?getFrontAdr():0;}
			//Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
			//Son*              getBack()        const {return back?getBackAdr():0;}
			//const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		};
		struct KdData
		{
			enum {transition=1};
			Ofs               size:sizeof(Ofs)*8-4;
			Ofs               kd:1;
			Ofs               _transition:1;
			Ofs               splitAxis:2;
			bool              isLeaf()         const {return splitAxis==3;}
			Son*              getFront()       const {return (Son*)((char*)this+sizeof(*this)+sizeof(real));}
			Son*              getBack()        const {return getFront()->getNext();}
			real              getSplitValue()  const {return *(real*)(this+1);}
			void              setSplitValue(real r)  {*(real*)(this+1)=r;}
			TriInfo*          getTrianglesBegin() const {return (TriInfo*)(this+1);}
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
		void*             getTrianglesEnd()const {return (char*)this+bsp.size;}
		void              setTransition(bool t) {}
		void              setKd(bool k)         {kd.kd=k;}
	};

	// multi-level bsp (without transition in this node)
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=1, allows_kd=1, allows_emptykd=0};
		typedef const BspTree2<Ofs,TriInfo,Lo> This;
		typedef const BspTree2T<Ofs,TriInfo,Lo> Transitioneer;
		typedef This Son;
		struct BspData
		{
			Ofs               size:sizeof(Ofs)*8-4;
			Ofs               kd:1;
			Ofs               transition:1;
			Ofs               front:1;
			Ofs               back:1;
			//Son*              getFrontAdr()    const {return (Son*)(this+1);}
			//Son*              getFront()       const {return front?getFrontAdr():0;}
			//Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
			//Son*              getBack()        const {return back?getBackAdr():0;}
			//const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		};
		struct KdData
		{
			Ofs               size:sizeof(Ofs)*8-4;
			Ofs               kd:1;
			Ofs               transition:1;
			Ofs               splitAxis:2;
			bool              isLeaf()         const {return splitAxis==3;}
			Son*              getFront()       const {return (Son*)((char*)this+sizeof(*this)+sizeof(real));}
			Son*              getBack()        const {return getFront()->getNext();}
			real              getSplitValue()  const {return *(real*)(this+1);}
			void              setSplitValue(real r)  {*(real*)(this+1)=r;}
			TriInfo*          getTrianglesBegin() const {return (TriInfo*)(this+1);}
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
		void*             getTrianglesEnd()const {return (char*)this+bsp.size;}
		void              setTransition(bool t) {bsp.transition=t;}
		void              setKd(bool k)         {kd.kd=k;}
	};

	// triangle info: only index is stored
	// there may exist other triangle info implementations that contain more information
	template <class Index>
	struct TriIndex
	{
		void     setGeometry(unsigned atriangleIdx, const Vec3* a, const Vec3* b, const Vec3* c) {triangleIdx = atriangleIdx;}
		Index    getTriangleIndex() const {return triangleIdx;}
		Index    triangleIdx;
	};
	
	template IBP
	PRIVATE BspTree* load(FILE *f)
	{
		if (!f) return NULL;
		BspTree head;
		size_t read = fread(&head,sizeof(head),1,f);
		if (!read) return NULL;
		fseek(f,-(int)sizeof(head),SEEK_CUR);
		BspTree* tree = (BspTree*)malloc(head.bsp.size);
		read = fread(tree,1,head.bsp.size,f);
		if (read == head.bsp.size) return tree;
		free(tree);
		return NULL;
	}

	template IBP
	PRIVATE BspTree* load(const RRMesh* importer, const char* cacheLocation, const char* ext, BuildParams* buildParams, IntersectLinear* intersector)
	{
		if (!intersector) return NULL;
		if (!importer) return NULL;
		unsigned triangles = importer->getNumTriangles();
		if (!triangles) return NULL;
		if (!buildParams || buildParams->size<sizeof(BuildParams)) return NULL;
		BspTree* tree = NULL;
		char name[300];
		getFileName(name,300,TREE_VERSION,importer,cacheLocation,ext);


		// try to load tree from disk
		FILE* f;
		if (!buildParams->forceRebuild && (f=fopen(name,"rb")))
		{
			tree = load IBP2(f);
			fclose(f);
			if (tree)
				return tree;
		}

		// create tree in memory
		{
			OBJECT obj;
			RR_ASSERT(triangles);
			obj.face_num = triangles;
			obj.vertex_num = importer->getNumVertices();
			obj.face = new FACE[obj.face_num];
			obj.vertex = new VERTEX[obj.vertex_num];
			for (int i=0;i<obj.vertex_num;i++)
			{
				RRMesh::Vertex v;
				importer->getVertex(i,v);
				obj.vertex[i].x = v[0];
				obj.vertex[i].y = v[1];
				obj.vertex[i].z = v[2];
				obj.vertex[i].id = i;
				obj.vertex[i].side = 1;
				obj.vertex[i].used = 1;
			}
			unsigned ii=0;
			for (int i=0;i<obj.face_num;i++)
			{
				RRMesh::Triangle v;
				importer->getTriangle(i,v);
				obj.face[ii].vertex[0] = &obj.vertex[v[0]];
				obj.face[ii].vertex[1] = &obj.vertex[v[1]];
				obj.face[ii].vertex[2] = &obj.vertex[v[2]];
				// invalid triangles are skipped, but triangle numbers (id) are preserved
				if (intersector->isValidTriangle(i)) obj.face[ii++].id=i;
			}
			if (ii)
			{
				if (obj.face_num-ii)
					RRReporter::report(INF2,"%d degenerated triangles removed form collider.\n",obj.face_num-ii);
				obj.face_num = ii;
				RR_ASSERT(!tree);
				createAndSaveBsp IBP2(&obj,buildParams,NULL,(void**)&tree); // failure -> tree stays NULL 
			}
			else
			{
				RRReporter::report(WARN,"All %d triangles in mesh degenerated, fast collider not created.\n",obj.face_num);
			}
			delete[] obj.vertex;
			delete[] obj.face;
		}

		// save tree to disk (gcc warning on following line is innocent but hard to prevent)
		if (tree && tree->bsp.size>=MIN_BYTES_FOR_SAVE)
		{
			f = fopen(name,"wb");
			if (f)
			{
				fwrite(tree,tree->bsp.size,1,f);
				fclose(f);
			}
		}

		return tree;
	}

} // namespace

#endif
