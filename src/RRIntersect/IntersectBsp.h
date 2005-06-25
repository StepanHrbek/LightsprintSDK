#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

#ifdef USE_BSP

namespace rrIntersect
{

	template <class Ofs, unsigned ofsBits, class TriInfo>
	struct BspTreeLo
	{	
		typedef const BspTreeLo<Ofs,ofsBits,TriInfo> This;
		Ofs               size:ofsBits-2;
		Ofs               front:1;
		Ofs               back:1;
		This*             getNext()        const {return (THIS*)((char *)this+size);}
		This*             getFrontAdr()    const {return this+1;}
		This*             getFront()       const {return front?getFrontAdr():0;}
		This*             getBackAdr()     const {return (THIS*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		This*             getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	};

	template <class Ofs, int ofsBits, class TriInfo, class Lo>
	struct BspTreeHi
	{	
		typedef const BspTreeHi<Ofs,ofsBits,TriInfo,Lo> This;
		Ofs               size:ofsBits-3;
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

	#define IBP <class BspTree, class Ofs, int ofsBits, class TriInfo>
	#define IBP2 <BspTree,Ofs,ofsBits,TriInfo>
	template IBP
	class IntersectBsp : public IntersectLinear
	{
	public:
		IntersectBsp(RRObjectImporter* aimporter, IntersectTechnique aintersectTechnique);
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
