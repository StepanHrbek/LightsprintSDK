#include "RRIntersect.h"
#include "IntersectLinear.h"
#include "IntersectBsp.h"
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>

namespace rrIntersect
{

void RRObjectImporter::getTriangleSRL(unsigned i, TriangleSRL* t) const
{
	unsigned v0,v1,v2;
	getTriangle(i,v0,v1,v2);
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

RRIntersect* RRIntersect::newIntersect(RRObjectImporter* importer, IntersectTechnique intersectTechnique, void* buildParams)
{
	BuildParams bp;
	if(!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBsp.cpp and bsp.cpp
		case IT_BSP_COMPACT:
			if(importer->getNumTriangles()<=256)
				return new IntersectBsp<CBspTree21>(importer,intersectTechnique,".m21",(BuildParams*)buildParams);
			if(importer->getNumTriangles()<=65536)
				return new IntersectBsp<CBspTree42>(importer,intersectTechnique,".m42",(BuildParams*)buildParams);
			return new IntersectBsp<CBspTree44>(importer,intersectTechnique,".m44",(BuildParams*)buildParams);
		case IT_BSP_FASTEST:
		case IT_BSP_FAST:
			return new IntersectBsp<BspTree44>(importer,intersectTechnique,".big",(BuildParams*)buildParams);
		case IT_LINEAR: 
		default:
			return new IntersectLinear(importer,intersectTechnique);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	memset(this,0,sizeof(*this));
}

void RRIntersectStats::getInfo(char *buf, unsigned len, unsigned level) const
{
	buf[0]=0;
	len--;
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf),"Intersect stats:\n");
	if(intersects>100) {
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf)," rays=%d missed=%d(%d)\n",intersects,intersects-hits,(intersects-hits)/(intersects/100));
	if(level>=1 && (intersect_bspSRLNP || intersect_triangleSRLNP)) _snprintf(buf+strlen(buf),len-strlen(buf)," bspSRLNP=%d(%d) triSRLNP=%d(%d)\n",intersect_bspSRLNP,intersect_bspSRLNP/intersects,intersect_triangleSRLNP,intersect_triangleSRLNP/intersects);
	if(level>=1 && (intersect_bspNP    || intersect_triangleNP   )) _snprintf(buf+strlen(buf),len-strlen(buf)," bspNP=%d(%d) triNP=%d(%d)\n",intersect_bspNP,intersect_bspNP/intersects,intersect_bspNP,intersect_bspNP/intersects);
	}
	if(invalid_triangles) _snprintf(buf+strlen(buf),len-strlen(buf)," invalid_triangles=%d/%d\n",invalid_triangles,loaded_triangles);
	if(intersect_linear) _snprintf(buf+strlen(buf),len-strlen(buf)," intersect_linear=%d\n",intersect_linear);
	buf[len]=0;
}

} //namespace
