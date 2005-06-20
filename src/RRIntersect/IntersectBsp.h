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
		const TriangleP** getTriangles()   const {return (const TriangleP **)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*             getTrianglesEnd()const {return (char*)this+size;}
	};

	class IntersectBsp : public IntersectLinear
	{
	public:
		IntersectBsp(RRObjectImporter* aimporter);
		virtual ~IntersectBsp();
		virtual bool      intersect(RRRay* ray) const;
	protected:
		bool              intersect_bspSRLNP(RRRay* ray, BspTree *t, real distanceMax) const;
		bool              intersect_bspNP(RRRay* ray, BspTree *t, real distanceMax) const;
		BspTree*          tree;
	};

}

#endif

#endif
