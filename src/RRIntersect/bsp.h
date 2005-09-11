#ifndef COLLIDER_BSP_H
#define COLLIDER_BSP_H

#include "RRIntersect.h"

//#define TEST // robustness test

namespace rrCollider
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
			forceRebuild = 0;//!!! 0=ok
			prizeBalance = 5;
			prizeSplit = 40;
			prizePlane = 10;
			bspMaxFacesInTree = 400;
			bspBestN = 150;
			kdMinFacesInTree = 5;
			kdHavran = 0;
			kdLeaf = 0;
			switch(technique)
			{
				case RRCollider::IT_BSP_FASTEST:
					kdHavran = 1;
					break;
				case RRCollider::IT_BSP_FAST:
					break;
				case RRCollider::IT_BSP_COMPACT:
					prizeSplit = 50;
					prizePlane = 1;
					kdMinFacesInTree = 10;
					kdLeaf = 1;
					break;
			}
		}
	};

	template <class BspTree>
	extern bool createAndSaveBsp(FILE *f, OBJECT *obj, BuildParams* buildParams);
}

#endif
