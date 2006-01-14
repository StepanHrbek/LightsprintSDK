#ifndef COLLIDER_INTERSECTBSP_H
#define COLLIDER_INTERSECTBSP_H

#define IBP <class BspTree>
#define IBP2 <BspTree>

#define MIN_TRIANGLES_FOR_SAVE 362 // smaller objects are not saved to disk
#define SUPPORT_EMPTY_KDNODE // only FASTEST: typically tiny, rarely big speedup (bunny); typically a bit, rarely much slower and memory hungry build (soda)
//#define BAKED_TRIANGLE

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>

#include "bsp.h"
#include "cache.h"
#include "IntersectLinear.h"
#include "LicGenOpt.h"
#include "sha1.h"

namespace rrCollider
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
		void              setKd(bool k)         {assert(allows_kd || !k);kd.kd=k;}
		bool              contains(TriInfo tri) const
		{
			if(bsp.kd==1)
				return kd.getFront()->contains(tri) || kd.getBack()->contains(tri);
			for(const TriInfo* t=bsp.getTrianglesBegin();t<getTrianglesEnd();t++)
				if(*t==tri) return true;
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
		if(!f) return NULL;
		BspTree head;
		size_t readen = fread(&head,sizeof(head),1,f);
		if(!readen) return NULL;
		fseek(f,-(int)sizeof(head),SEEK_CUR);
		BspTree* tree = (BspTree*)malloc(head.bsp.size);
		readen = fread(tree,1,head.bsp.size,f);
		if(readen == head.bsp.size) return tree;
		free(tree);
		return NULL;
	}

	template IBP
	PRIVATE BspTree* load(RRMeshImporter* importer, const char* cacheLocation, const char* ext, BuildParams* buildParams, IntersectLinear* intersector)
	{
		if(!intersector) return NULL;
		if(!importer) return NULL;
		unsigned triangles = importer->getNumTriangles();
		if(!triangles) return NULL;
		if(!buildParams || buildParams->size<sizeof(BuildParams)) return NULL;
		BspTree* tree = NULL;
		bool retried = false;
		char name[300];
		getFileName(name,300,importer,cacheLocation,ext);


		FILE* f = buildParams->forceRebuild ? NULL : fopen(name,"rb");
		if(!f)
		{
			printf("'%s' not found.\n",name);
		retry:
			OBJECT obj;
			assert(triangles);
			obj.face_num = triangles;
			obj.vertex_num = importer->getNumVertices();
			obj.face = new FACE[obj.face_num];
			obj.vertex = new VERTEX[obj.vertex_num];
			for(int i=0;i<obj.vertex_num;i++)
			{
				RRMeshImporter::Vertex v;
				importer->getVertex(i,v);
				obj.vertex[i].x = v[0];
				obj.vertex[i].y = v[1];
				obj.vertex[i].z = v[2];
				obj.vertex[i].id = i;
				obj.vertex[i].side = 1;
				obj.vertex[i].used = 1;
			}
			unsigned ii=0;
			for(int i=0;i<obj.face_num;i++)
			{
				RRMeshImporter::Triangle v;
				importer->getTriangle(i,v);
				obj.face[ii].vertex[0] = &obj.vertex[v[0]];
				obj.face[ii].vertex[1] = &obj.vertex[v[1]];
				obj.face[ii].vertex[2] = &obj.vertex[v[2]];
				if(intersector->isValidTriangle(i)) obj.face[ii++].id=i;
			}
			assert(ii);
			obj.face_num = ii;

			if(obj.face_num<MIN_TRIANGLES_FOR_SAVE)
			{
				// pack tree directly to ram, no saving
				assert(!tree);
				bool ok = createAndSaveBsp IBP2(&obj,buildParams,NULL,(void**)&tree);
			}
			else
			{
				// pack tree to file
				f = fopen(name,"wb");
				if(f)
				{
					assert(!tree);
					bool ok = createAndSaveBsp IBP2(&obj,buildParams,f,NULL);
					fclose(f);
					if(!ok)
					{
						printf("Failed to write tree (%s)...\n",name);
						//remove(name);
						f = fopen(name,"wb");
						fclose(f);
						retried = true;
					}
				}
				f = fopen(name,"rb");
			}

			delete[] obj.vertex;
			delete[] obj.face;
		}
		if(f)
		{
			printf("Loading '%s'.\n",name);
			tree = load IBP2(f);
			fclose(f);
			if(!tree && !retried)
			{
				printf("Invalid tree in cache (%s), trying to fix...\n",name);
				retried = true;
				goto retry;
			}
		}

		return tree;
	}

} // namespace

#endif
