#ifndef RRINTERSECT_BSP_H
#define RRINTERSECT_BSP_H

#include "RRIntersect.h"

namespace rrIntersect
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
		unsigned size;
		unsigned forceRebuild;
		unsigned prizeBalance;
		unsigned prizeSplit;
		unsigned prizePlane;
		unsigned bspMaxFacesInTree;
		unsigned bspBestN;
		unsigned kdMinFacesInTree;
		unsigned kdHavran;
		BuildParams(RRIntersect::IntersectTechnique technique)
		{
			size = sizeof(*this);
			forceRebuild = 0;//!!!
			prizeBalance = 5;
			prizeSplit = 40;
			prizePlane = 10;
			bspMaxFacesInTree = 400;
			bspBestN = 150;
			kdMinFacesInTree = 5;
			kdHavran = 0;
			switch(technique)
			{
				case RRIntersect::IT_BSP_FASTEST:
					kdHavran = 1;
					break;
				case RRIntersect::IT_BSP_FAST:
					break;
				case RRIntersect::IT_BSP_COMPACT:
					prizeSplit = 50;
					prizePlane = 1;
					kdMinFacesInTree = 10;
					break;
			}
		}
	};

	template <class BspTree>
	extern bool createAndSaveBsp(FILE *f, OBJECT *obj, BuildParams* buildParams);
}

#endif
