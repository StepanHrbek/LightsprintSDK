#ifndef RRINTERSECT_BSP_H
#define RRINTERSECT_BSP_H

namespace rrIntersect
{
	struct VERTEX {
		int id,side,used;
		float x,y,z,u,v;
		};

	struct NORMAL {
		float a,b,c,d;
		};

	struct FACE {
		int id,side;
		VERTEX *vertex[3];
		NORMAL normal;
		};

	struct OBJECT {
		int      face_num;
		int      vertex_num;
		FACE     *face;
		VERTEX   *vertex;
		};

	extern void createAndSaveBsp(FILE *f, OBJECT *obj);
}

#endif
