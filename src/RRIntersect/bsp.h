#ifndef RRINTERSECT_BSP_H
#define RRINTERSECT_BSP_H

#include "IntersectBsp.h"

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
		int id,side;
		VERTEX *vertex[3];
		NORMAL normal;
		void fillMinMax();
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

	template IBP
	extern bool createAndSaveBsp(FILE *f, OBJECT *obj, BuildParams* buildParams);
}

#endif
