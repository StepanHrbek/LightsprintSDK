#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

#ifdef USE_BSP

namespace rrIntersect
{

	template <class Ofs, class TriInfo, class Lo>
	struct BspTree1
	{
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		typedef const BspTree1<Ofs,TriInfo,Lo> This;
		Ofs               size:sizeof(Ofs)*8-2;
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (This*)((char *)this+size);}
		This*             getFrontAdr()    const {return this+1;}
		This*             getFront()       const {return front?getFrontAdr():0;}
		This*             getBackAdr()     const {return (This*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		This*             getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	};

	template <class Ofs, class TriInfo, class Lo>
	struct BspTree2
	{	
		typedef Ofs _Ofs;
		typedef TriInfo _TriInfo;
		typedef Lo _Lo;
		typedef const BspTree2<Ofs,TriInfo,Lo> This;
		Ofs               size:sizeof(Ofs)*8-3;
		Ofs               transition:1; // last This in tree traversal, sons are Lo
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (This*)((char *)this+size);}
		This*             getFrontAdr()    const {return this+1;}
		This*             getFront()       const {return front?getFrontAdr():0;}
		This*             getBackAdr()     const {return (This*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		This*             getBack()        const {return back?getBackAdr():0;}
		const Lo*         getLoFrontAdr()  const {return (Lo*)(this+1);}
		const Lo*         getLoFront()     const {return front?getLoFrontAdr():0;}
		const Lo*         getLoBackAdr()   const {return (Lo*)((char*)getLoFrontAdr()+(front?getLoFrontAdr()->size:0));}
		const Lo*         getLoBack()      const {return back?getLoBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getLoBackAdr()+(back?getLoBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	};
	// single-level bsp
	typedef BspTree1<unsigned short,unsigned char ,void> BspTree21;
	typedef BspTree1<unsigned short,unsigned short,void> BspTree22;
	typedef BspTree1<unsigned int  ,unsigned short,void> BspTree42;
	typedef BspTree1<unsigned int  ,unsigned int  ,void> BspTree44;

	// multi-level bsp
	typedef BspTree2<unsigned char ,unsigned int  ,void      > CBspTree14;
	typedef BspTree2<unsigned short,unsigned int  ,CBspTree14> CBspTree24;
	typedef BspTree2<unsigned int  ,unsigned int  ,CBspTree24> CBspTree44;

	typedef BspTree2<unsigned char ,unsigned short,void      > CBspTree12;
	typedef BspTree2<unsigned short,unsigned short,CBspTree12> CBspTree22;
	typedef BspTree2<unsigned int  ,unsigned short,CBspTree22> CBspTree42;

	typedef BspTree2<unsigned char ,unsigned char ,void      > CBspTree11;
	typedef BspTree2<unsigned short,unsigned char ,CBspTree11> CBspTree21;
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
	protected:
		BspTree*          tree;
		bool              intersect_bspSRLNP(RRRay* ray, BspTree *t, real distanceMax) const;
		bool              intersect_bspNP(RRRay* ray, BspTree *t, real distanceMax) const;
		bool              intersect_bsp(RRRay* ray, BspTree *t, real distanceMax) const;
	};

}

#endif

#endif
