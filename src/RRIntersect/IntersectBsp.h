#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

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
			const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
			void*             getTrianglesEnd()const {return (char*)this+size;}
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
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
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
			const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
			void*             getTrianglesEnd()const {return (char*)this+size;}
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
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
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
			const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
			void*             getTrianglesEnd()const {return (char*)this+size;}
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
		};
		union {
			BspData bsp;
			KdData  kd;
		};
		This*             getNext()        const {return (This*)((char *)this+bsp.size);}
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
	typedef BspTree2<unsigned int  ,unsigned char ,CBspTree21> CBspTree41;

	#define IBP <class BspTree>
	#define IBP2 <BspTree>
	template IBP
	class IntersectBsp : public IntersectLinear
	{
	public:
		IntersectBsp(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, char* ext, int effort);
		virtual ~IntersectBsp();
		virtual bool      intersect(RRRay* ray) const;
		virtual unsigned  getMemorySize() const;
		// must be public because it calls itself with different template params
		bool              intersect_bsp(RRRay* ray, const BspTree *t, real distanceMax) const; 
	protected:
		BspTree*          tree;
		bool              intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const;
		bool              intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const;
	};

}

#endif
