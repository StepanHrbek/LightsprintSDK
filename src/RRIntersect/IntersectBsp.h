#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

#ifdef USE_BSP

namespace rrIntersect
{

	struct BspTree
	{			  
		unsigned          size:30;
		unsigned          front:1;
		unsigned          back:1;
		const BspTree*    getNext()        const {return (BspTree *)((char *)this+size);}
		const BspTree*    getFrontAdr()    const {return this+1;}
		const BspTree*    getFront()       const {return front?getFrontAdr():0;}
		const BspTree*    getBackAdr()     const {return (BspTree *)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		const BspTree*    getBack()        const {return back?getBackAdr():0;}
		const unsigned*   getTriangles()   const {return (const unsigned*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	};

	/*template <class TriIdx>
	struct BspTri
	{
		TriIdx            triIdx;
	};*/

	template <class Ofs, int OfsBits, class TriInfo, class Lo>
	struct BspTreeHi
	{	
	#define THIS BspTreeHi<Ofs,OfsBits,TriInfo,Lo>
		Ofs               size:OfsBits-3;
		Ofs               transition:1; // last THIS in tree traversal, sons are Lo
		Ofs               front:1;
		Ofs               back:1;
		const THIS*       getNext()        const {return (THIS*)((char *)this+size);}
		const THIS*       getFrontAdr()    const {return this+1;}
		const THIS*       getFront()       const {return front?getFrontAdr():0;}
		const THIS*       getBackAdr()     const {return (THIS*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		const THIS*       getBack()        const {return back?getBackAdr():0;}
		const Lo*         getLoFrontAdr()  const {return this+1;}
		const Lo*         getLoFront()     const {return front?getLoFrontAdr():0;}
		const Lo*         getLoBackAdr()   const {return (THIS*)((char*)getLoFrontAdr()+(front?getLoFrontAdr()->size:0));}
		const Lo*         getLoBack()      const {return back?getLoBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getLoBackAdr()+(back?getLoBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	#undef THIS
	};

	template <class Ofs, unsigned OfsBits, class TriInfo>
	struct BspTreeLo
	{	
	#define THIS BspTreeLo<Ofs,OfsBits,TriInfo>
		Ofs               size:OfsBits-2;
		Ofs               front:1;
		Ofs               back:1;
		const THIS*       getNext()        const {return (THIS*)((char *)this+size);}
		const THIS*       getFrontAdr()    const {return this+1;}
		const THIS*       getFront()       const {return front?getFrontAdr():0;}
		const THIS*       getBackAdr()     const {return (THIS*)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		const THIS*       getBack()        const {return back?getBackAdr():0;}
		const TriInfo*    getTriangles()   const {return (const TriInfo*)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	#undef THIS
	};

	/*#define U32 unsigned
	#define U16 short
	extern BspTreeHi<U32,32,U32,BspTreeLo<U16,16,U32> > t;*/

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
