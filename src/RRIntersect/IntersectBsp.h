#ifndef RRINTERSECT_INTERSECTBSP_H
#define RRINTERSECT_INTERSECTBSP_H

#include "IntersectLinear.h"

#ifdef USE_BSP

namespace rrIntersect
{

	struct BspTree
	{
		unsigned  size:30;
		unsigned  front:1;
		unsigned  back:1;
		BspTree*  next()           {return (BspTree *)((char *)this+size);}
		BspTree*  getFrontAdr()    {return this+1;}
		BspTree*  getFront()       {return front?getFrontAdr():0;}
		BspTree*  getBackAdr()     {return (BspTree *)((char*)getFrontAdr()+(front?getFrontAdr()->size:0));}
		BspTree*  getBack()        {return back?getBackAdr():0;}
		TriangleP**getTriangles()  {return (TriangleP **)((char*)getBackAdr()+(back?getBackAdr()->size:0));}
		void*     getTrianglesEnd(){return (char*)this+size;}
	};

	class IntersectBsp : public IntersectLinear
	{
	public:
		IntersectBsp(RRObjectImporter* aimporter);
		~IntersectBsp();
		virtual bool      intersect(RRRay* ray);
	protected:
		bool              intersect_bspNP(BspTree *t,real distanceMax);
		BspTree*          tree;
	};

}

#endif

#endif
