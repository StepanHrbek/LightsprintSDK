#include "RRIntersect.h"
#include "IntersectLinear.h"
#include "IntersectBsp.h"
#include "IntersectKd.h"
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>

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
	real* v[3];
	v[0] = getVertex(v0);
	v[1] = getVertex(v1);
	v[2] = getVertex(v2);
	t->s[0]=v[0][0];
	t->s[1]=v[0][1];
	t->s[2]=v[0][2];
	t->r[0]=v[1][0]-v[0][0];
	t->r[1]=v[1][1]-v[0][1];
	t->r[2]=v[1][2]-v[0][2];
	t->l[0]=v[2][0]-v[0][0];
	t->l[1]=v[2][1]-v[0][1];
	t->l[2]=v[2][2]-v[0][2];
}

void RRObjectImporter::getTriangleSRLN(unsigned i, TriangleSRLN* t)
{
	unsigned v0,v1,v2,s;
	getTriangle(i,v0,v1,v2,s);
	real* v[3];
	v[0] = getVertex(v0);
	v[1] = getVertex(v1);
	v[2] = getVertex(v2);
	t->s[0]=v[0][0];
	t->s[1]=v[0][1];
	t->s[2]=v[0][2];
	t->r[0]=v[1][0]-v[0][0];
	t->r[1]=v[1][1]-v[0][1];
	t->r[2]=v[1][2]-v[0][2];
	t->l[0]=v[2][0]-v[0][0];
	t->l[1]=v[2][1]-v[0][1];
	t->l[2]=v[2][2]-v[0][2];
	real x=t->r[1] * t->l[2] - t->r[2] * t->l[1];
	real y=t->r[2] * t->l[0] - t->r[0] * t->l[2];
	real z=t->r[0] * t->l[1] - t->r[1] * t->l[0];
	real a=1/sqrtf(x*x+y*y+z*z);
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
	memset(this,0,sizeof(*this));
}

char* RRIntersectStats::getInfo(unsigned level)
{
	static char buf[1000];
	buf[0]=0;
	if(level>=1) _snprintf(buf+strlen(buf),999-strlen(buf),"Intersect stats:\n");
	if(level>=1) _snprintf(buf+strlen(buf),999-strlen(buf)," rays=%d missed=%d(%d)\n",intersects,intersects-hits,(intersects-hits)/(intersects/100));
	if(level>=1 && (intersect_bspSRLNP || intersect_triangleSRLNP)) _snprintf(buf+strlen(buf),999-strlen(buf)," bspSRLNP=%d(%d) triSRLNP=%d(%d)\n",intersect_bspSRLNP,intersect_bspSRLNP/intersects,intersect_triangleSRLNP,intersect_triangleSRLNP/intersects);
	if(level>=1 && (intersect_bspNP    || intersect_triangleNP   )) _snprintf(buf+strlen(buf),999-strlen(buf)," bspNP=%d(%d) triNP=%d(%d)\n",intersect_bspNP,intersect_bspNP/intersects,intersect_bspNP,intersect_bspNP/intersects);
	if(invalid_triangles) _snprintf(buf+strlen(buf),999-strlen(buf)," invalid_triangles=%d/%d\n",invalid_triangles,loaded_triangles);
	if(intersect_linear) _snprintf(buf+strlen(buf),999-strlen(buf)," intersect_linear=%d\n",intersect_linear);
	buf[999]=0;
	return buf;
}

} //namespace
