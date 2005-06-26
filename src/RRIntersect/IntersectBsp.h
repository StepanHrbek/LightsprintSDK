#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

#ifdef USE_BSP

namespace rrIntersect
{

	// single-level bps
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree1
	{
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {transition=0, allows_transition=0};
		typedef const BspTree1<Ofs,TriInfo,Lo> This;
		typedef This Transitioneer;
		typedef This Son;
		Ofs               size:sizeof(Ofs)*8-2;
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (This*)((char *)this+size);}
		Son*              getFrontAdr()    const {return (Son*)(this+1);}
		Son*              getFront()       const {return front?getFrontAdr():0;}
		Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		Son*              getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
		void              setTransition(bool t) {}
	};

	// multi-level bsp (with transition in this node)
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2T
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {transition=1, allows_transition=0};
		typedef const BspTree2T<Ofs,TriInfo,Lo> This;
		typedef const This Transitioneer;
		typedef const Lo Son;
		Ofs               size:sizeof(Ofs)*8-3;
		Ofs               _transition:1;
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (This*)((char *)this+size);}
		Son*              getFrontAdr()    const {return (Son*)(this+1);}
		Son*              getFront()       const {return front?getFrontAdr():0;}
		Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		Son*              getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
		void              setTransition(bool t) {}
	};

	// multi-level bsp (without transition in this node)
	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		enum {allows_transition=1};
		typedef const BspTree2<Ofs,TriInfo,Lo> This;
		typedef const BspTree2T<Ofs,TriInfo,Lo> Transitioneer;
		typedef This Son;
		Ofs               size:sizeof(Ofs)*8-3;
		Ofs               transition:1; // last This in tree traversal, sons are Lo
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (This*)((char *)this+size);}
		Son*              getFrontAdr()    const {return (Son*)(this+1);}
		Son*              getFront()       const {return front?getFrontAdr():0;}
		Son*              getBackAdr()     const {return (Son*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		Son*              getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
		void              setTransition(bool t) {transition=t;}
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
		IntersectBsp(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique, char* ext);
		virtual ~IntersectBsp();
		virtual bool      intersect(RRRay* ray) const;
		virtual unsigned  getMemorySize() const;
		bool              intersect_bsp(RRRay* ray, const BspTree *t, real distanceMax) const;//!!!
	protected:
		BspTree*          tree;
		bool              intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const;
		bool              intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const;
	};

}

#endif

#endif
