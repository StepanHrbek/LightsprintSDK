// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - compact.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_INTERSECTBSPCOMPACT_H
#define COLLIDER_INTERSECTBSPCOMPACT_H

#include "IntersectBsp.h"

namespace rr
{
	template IBP
	class IntersectBspCompact : public IntersectLinear
	{
	public:
		static IntersectBspCompact* create(const RRMesh* aimporter, IntersectTechnique aintersectTechnique, bool& aborting, const char* cacheLocation, const char* ext, BuildParams* buildParams) {return new IntersectBspCompact(aimporter,aintersectTechnique,aborting,cacheLocation,ext,buildParams);}
		virtual ~IntersectBspCompact();
		virtual bool      intersect(RRRay* ray) const;
		virtual IntersectTechnique getTechnique() const {return IT_BSP_COMPACT;}
		virtual unsigned  getMemoryOccupied() const;
		// must be public because it calls itself with different template params
		bool              intersect_bsp(RRRay* ray, const BspTree *t, real distanceMax) const; 
	protected:
		IntersectBspCompact(const RRMesh* aimporter, IntersectTechnique aintersectTechnique, bool& aborting, const char* cacheLocation, const char* ext, BuildParams* buildParams);
		BspTree*          tree;
	};

	// multi-level bsp (COMPACT)
	typedef BspTree1<unsigned char ,TriIndex<unsigned int  >,void      >  BspTree14;
	typedef BspTree2<unsigned short,TriIndex<unsigned int  >, BspTree14> CBspTree24;
	typedef BspTree2<unsigned int  ,TriIndex<unsigned int  >,CBspTree24> CBspTree44;

	typedef BspTree1<unsigned char ,TriIndex<unsigned short>,void      >  BspTree12;
	typedef BspTree2<unsigned short,TriIndex<unsigned short>, BspTree12> CBspTree22;
	typedef BspTree2<unsigned int  ,TriIndex<unsigned short>,CBspTree22> CBspTree42;

	typedef BspTree1<unsigned char ,TriIndex<unsigned char >,void      >  BspTree11;
	typedef BspTree2<unsigned short,TriIndex<unsigned char >, BspTree11> CBspTree21;

}

#endif
