// --------------------------------------------------------------------------
// Build of acceleration structure for ray-mesh intersections.
// Copyright (c) 2000-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_BSP_H
#define COLLIDER_BSP_H

#include "Lightsprint/RRCollider.h"

//#define TEST_BSP // robustness test

namespace rr
{
	struct VERTEX 
	{
		int id,side,used;
		float x,y,z,u,v;
		float operator [](unsigned i) const {return (&x)[i];}
	};

	struct NORMAL 
	{
		float a,b,c,d;
	};

	struct FACE 
	{
		int id;
		VERTEX *vertex[3];
		void fillNormal();
		void fillMinMax();
		float getArea() const;
		NORMAL normal;
		float min[3]; // bbox min/max
		float max[3];
	};

	struct OBJECT 
	{
		int      face_num;
		int      vertex_num;
		FACE     *face;
		VERTEX   *vertex;
	};

	struct BuildParams
	{
		unsigned size;              // size of this structure
		unsigned forceRebuild;      // force rebuild of tree
		unsigned prizeBalance;      // redox/dee heuristic: penalty for unbalanced tree
		unsigned prizeSplit;        // redox/dee heuristic: penalty for triangle splitted by plane
		unsigned prizePlane;        // redox/dee heuristic: penalty for triangle in plane
		unsigned bspMaxFacesInTree; // don't even try bsp on bigger tree
		unsigned bspBestN;          // preselect and fully test this many triangles for bsp root
		unsigned kdMinFacesInTree;  // don't even try kd on smaller tree
		unsigned kdHavran;          // allow havran's splitting heuristic for fastest tree to be tried
		unsigned kdLeaf;            // allow kd leaves in tree (supported only by compact intersector)
		BuildParams(RRCollider::IntersectTechnique technique)
		{
			size = sizeof(*this);
			forceRebuild = 0;
			prizeBalance = 5;
			prizeSplit = 40;
			prizePlane = 10;
			bspMaxFacesInTree = 40; //400->40: bunny 4.87->4.69, ctfhydrosis 691s->612s, cpulightmaps 18.1s->17s
			bspBestN = 150;
			kdMinFacesInTree = 5;
			kdHavran = 0;
			kdLeaf = 1; // necessary to avoid exponential growth in speedtree meshes
			switch(technique)
			{
				case RRCollider::IT_BSP_FASTEST:
					kdHavran = 1;
					break;
				case RRCollider::IT_BSP_FASTER:
				case RRCollider::IT_BSP_FAST:
					break;
				case RRCollider::IT_BSP_COMPACT:
					prizeSplit = 50;
					prizePlane = 1;
					bspMaxFacesInTree = 400; // makes build and intersections slower, but tree smaller
					kdMinFacesInTree = 10;
					break;
			}
		}
	};

	template <class BspTree>
	extern bool createAndSaveBsp(OBJECT *obj, bool& aborting, BuildParams* buildParams, FILE *f, void** m);
}

#endif
