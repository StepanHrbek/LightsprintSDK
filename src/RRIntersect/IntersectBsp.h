#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#define IBP <class BspTree>
#define IBP2 <BspTree>

#include "IntersectLinear.h"

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <time.h>  //gate
#include "bsp.h"
#include "cache.h"

namespace rrIntersect
{

	// single-level bps
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree1
	{
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=0, allows_kd=1};
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
			//Son*              getFrontAdr()    const {return (Son*)(this+1);}
			//Son*              getFront()       const {return front?getFrontAdr():0;}
			//Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
			//Son*              getBack()        const {return back?getBackAdr():0;}
			//const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
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
	};

	// multi-level bsp (with transition in this node)
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2T
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=0, allows_kd=1};
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
		enum {allows_transition=1, allows_kd=1};
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

	// single-level bsp
	typedef BspTree1<unsigned short,unsigned char ,void> BspTree21;
	typedef BspTree1<unsigned short,unsigned short,void> BspTree22;
	typedef BspTree1<unsigned int  ,unsigned short,void> BspTree42;
	typedef BspTree1<unsigned int  ,unsigned int  ,void> BspTree44;

	// multi-level bsp
	typedef BspTree1<unsigned char ,unsigned int  ,void      >  BspTree14;
	typedef BspTree2<unsigned short,unsigned int  , BspTree14> CBspTree24;
	typedef BspTree2<unsigned int  ,unsigned int  ,CBspTree24> CBspTree44;

	typedef BspTree1<unsigned char ,unsigned short,void      >  BspTree12;
	typedef BspTree2<unsigned short,unsigned short, BspTree12> CBspTree22;
	typedef BspTree2<unsigned int  ,unsigned short,CBspTree22> CBspTree42;

	typedef BspTree1<unsigned char ,unsigned char ,void      >  BspTree11;
	typedef BspTree2<unsigned short,unsigned char , BspTree11> CBspTree21;


	template IBP
	BspTree* load(FILE *f)
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
	BspTree* load(RRObjectImporter* importer, const char* ext, BuildParams* buildParams, IntersectLinear* intersector)
	{
		if(!intersector) return NULL;
		if(!importer) return NULL;
		unsigned triangles = importer->getNumTriangles();
		if(!triangles) return NULL;
		if(!buildParams || buildParams->size<sizeof(BuildParams)) return NULL;
		BspTree* tree = NULL;
		bool retried = false;
		char name[300];
		getFileName(name,300,importer,ext);
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
				real* v = importer->getVertex(i);
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
				unsigned v[3];
				importer->getTriangle(i,v[0],v[1],v[2]);
				obj.face[ii].vertex[0] = &obj.vertex[v[0]];
				obj.face[ii].vertex[1] = &obj.vertex[v[1]];
				obj.face[ii].vertex[2] = &obj.vertex[v[2]];
				if(intersector->isValidTriangle(i)) obj.face[ii++].id=i;
			}
			assert(ii);
			obj.face_num = ii;
			f = fopen(name,"wb");
			if(f)
			{
				bool ok = createAndSaveBsp IBP2(f,&obj,buildParams);
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
			delete[] obj.vertex;
			delete[] obj.face;
			f = fopen(name,"rb");
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
		time_t t = time(NULL);
		if(t<1120681678 || t>1120681678+77*24*3599) {free(tree); tree = NULL;} // 6.7.2005
		return tree;
	}

}

#endif
