#include "RRIntersect.h"
#include "IntersectLinear.h"
#include "IntersectBsp.h"
#include "IntersectKd.h"
#include <math.h>

namespace rrIntersect
{

RRObjectImporter::RRObjectImporter()
{
	fastN = false;
	fastSRL = false;
	fastSRLN = false;
}

void RRObjectImporter::getTriangleN(unsigned i, TriangleN* t)
{
	TriangleSRLN tmp;
	getTriangleSRLN(i,&tmp);
	t->n[0]=tmp.n[0];
	t->n[1]=tmp.n[1];
	t->n[2]=tmp.n[2];
	t->n[3]=tmp.n[3];
}

void RRObjectImporter::getTriangleSRL(unsigned i, TriangleSRL* t)
{
	unsigned v0,v1,v2,s;
	getTriangle(i,v0,v1,v2,s);
	getVertex(v0,t->s[0],t->s[1],t->s[2]);
	getVertex(v1,t->r[0],t->r[1],t->r[2]);
	getVertex(v2,t->l[0],t->l[1],t->l[2]);
	t->r[0]-=t->s[0];
	t->r[1]-=t->s[1];
	t->r[2]-=t->s[2];
	t->l[0]-=t->s[0];
	t->l[1]-=t->s[1];
	t->l[2]-=t->s[2];
}

void RRObjectImporter::getTriangleSRLN(unsigned i, TriangleSRLN* t)
{
	unsigned v0,v1,v2,s;
	getTriangle(i,v0,v1,v2,s);
	getVertex(v0,t->s[0],t->s[1],t->s[2]);
	getVertex(v1,t->r[0],t->r[1],t->r[2]);
	getVertex(v2,t->l[0],t->l[1],t->l[2]);
	t->r[0]-=t->s[0];
	t->r[1]-=t->s[1];
	t->r[2]-=t->s[2];
	t->l[0]-=t->s[0];
	t->l[1]-=t->s[1];
	t->l[2]-=t->s[2];
	float x=t->r[1] * t->l[2] - t->r[2] * t->l[1];
	float y=t->r[2] * t->l[0] - t->r[0] * t->l[2];
	float z=t->r[0] * t->l[1] - t->r[1] * t->l[0];
	float a=1/sqrtf(x*x+y*y+z*z);
	t->n[0]=x*a;
	t->n[1]=y*a;
	t->n[2]=z*a;
	t->n[3]=-(t->s[0] * t->n[0] + t->s[1] * t->n[1] + t->s[2] * t->n[2]);
}

RRIntersect* newIntersect(RRObjectImporter* importer)
{
#ifdef USE_KD
	return new IntersectKd(importer);
#else
#ifdef USE_BSP
	return new IntersectBsp(importer);
#else
	return new IntersectLinear(importer);
#endif
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	//!!!memset(this,0,sizeof(*this));
}

unsigned RRIntersectStats::shots;
unsigned RRIntersectStats::bsp;
unsigned RRIntersectStats::kd;
unsigned RRIntersectStats::tri;

} //namespace
