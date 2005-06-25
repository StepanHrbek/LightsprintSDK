#include "IntersectBsp.h"

#ifdef USE_BSP

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <time.h>  //gate
#include "bsp.h"
#include "cache.h"

namespace rrIntersect
{

template IBP
BspTree* load(FILE *f)
{
	if(!f) return NULL;
	BspTree head;
	size_t readen = fread(&head,sizeof(head),1,f);
	if(!readen) return NULL;
	fseek(f,-(int)sizeof(head),SEEK_CUR);
	BspTree* tree = (BspTree*)malloc(head.size);
	readen = fread(tree,1,head.size,f);
	if(readen == head.size) return tree;
	free(tree);
	return NULL;
}

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

template IBP
bool IntersectBsp IBP2::intersect_bspSRLNP(RRRay* ray, BspTree *t, real distanceMax) const
// input:                t, rayOrigin, rayDir, skip, hitDistanceMin, hitDistanceMax
// returns:              true if ray hits t
// modifies when hit:    (distanceMin, hitPoint3d) hitPoint2d, hitOuterSide, hitDistance
// modifies when no hit: (distanceMin, hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	intersectStats.intersect_bspSRLNP++;
	assert(ray);
	assert(t);

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	BspTree::_TriInfo* triangle=(BspTree::_TriInfo*)((char*)back+(t->back?back->size:0));
	assert(triangleSRLNP);
	Plane& n=triangleSRLNP[triangle[0]].n3;
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	const void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=ray->skipTriangle && intersect_triangleSRLNP(ray,triangleSRLNP+*triangle))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBsp IBP2::intersect_bspNP(RRRay* ray, BspTree *t, real distanceMax) const
{
begin:
	intersectStats.intersect_bspNP++;
	assert(ray);
	assert(t);

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	BspTree::_TriInfo* triangle=(BspTree::_TriInfo*)((char*)back+(t->back?back->size:0));
	assert(triangleNP);
	Plane& n=triangleNP[triangle[0]].n3;
	DBGLINE
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	DBGLINE
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRObjectImporter::TriangleSRL t2;
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skipTriangle && intersect_triangleNP(ray,triangleNP+*triangle,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBsp IBP2::intersect_bsp(RRRay* ray, BspTree* t, real distanceMax) const
{
begin:
	intersectStats.intersect_bsp++;
	assert(ray);
	assert(t);

	BspTree* front=t+1;
	BspTree* back=(BspTree*)((char*)front+(t->front?front->size:0));
	BspTree::_TriInfo* triangle=(BspTree::_TriInfo*)((char*)back+(t->back?back->size:0));
	assert(triangle<t->getTrianglesEnd());

	RRObjectImporter::TriangleSRL t2;
	importer->getTriangleSRL(*triangle,&t2);
	Plane n;
	n.x = t2.r[1] * t2.l[2] - t2.r[2] * t2.l[1];
	n.y = t2.r[2] * t2.l[0] - t2.r[0] * t2.l[2];
	n.z = t2.r[0] * t2.l[1] - t2.r[1] * t2.l[0];
	n.d = -(t2.s[0] * n.x + t2.s[1] * n.y + t2.s[2] * n.z);

	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bsp(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bsp(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skipTriangle && intersect_triangle(ray,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
				return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
IntersectBsp IBP2::IntersectBsp(RRObjectImporter* aimporter, IntersectTechnique intersectTechnique, char* ext) : IntersectLinear(aimporter, intersectTechnique)
{
	tree = NULL;
	if(!triangles) return;
	bool retried = false;
	char name[300];
	getFileName(name,300,aimporter,ext);
	FILE* f = fopen(name,"rb");
	if(!f)
	{
		printf("'%s' not found.\n",name);
retry:
		OBJECT obj;
		assert(triangles);
		obj.face_num = triangles;
		obj.vertex_num = importer->getNumVertices();
		obj.face = new FACE[obj.face_num];
		obj.vertex = new VERTEX[obj.vertex_num];
		for(int i=0;i<obj.vertex_num;i++)
		{
			real* v = importer->getVertex(i);
			obj.vertex[i].x = v[0];
			obj.vertex[i].y = v[1];
			obj.vertex[i].z = v[2];
			obj.vertex[i].id = i;
			obj.vertex[i].side = 1;
			obj.vertex[i].used = 1;
		}
		unsigned ii=0;
		for(int i=0;i<obj.face_num;i++)
		{
			unsigned v[3];
			importer->getTriangle(i,v[0],v[1],v[2]);
			obj.face[ii].vertex[0] = &obj.vertex[v[0]];
			obj.face[ii].vertex[1] = &obj.vertex[v[1]];
			obj.face[ii].vertex[2] = &obj.vertex[v[2]];
			if(isValidTriangle(i)) obj.face[ii++].id=i;
		}
		assert(ii);
		obj.face_num = ii;
		f = fopen(name,"wb");
		if(f)
		{
			createAndSaveBsp IBP2(f,&obj);
			fclose(f);
		}
		delete[] obj.vertex;
		delete[] obj.face;
		f = fopen(name,"rb");
	}
	if(f)
	{
		printf("Loading '%s'.\n",name);
		tree = load IBP2(f);
		fclose(f);
		if(!tree && !retried)
		{
			printf("Invalid tree in cache (%s), trying to fix...\n",name);
			retried = true;
			goto retry;
		}
	}
	time_t t = time(NULL);
	if(t<1118815599 || t>1118814496+77*24*3599) tree = NULL;
}

template IBP
unsigned IntersectBsp IBP2::getMemorySize() const
{
	return IntersectLinear::getMemorySize()
		+(tree?tree->size:0)
		+sizeof(IntersectBsp)-sizeof(IntersectLinear);
}

template IBP
bool IntersectBsp IBP2::intersect(RRRay* ray) const
{
	// fallback when bspgen failed (run from readonly disk etc)
	if(!tree)
	{
		DBGLINE
		DBG(printf("Bsp fallback to linear."));
		return IntersectLinear::intersect(ray);
	}

	DBG(printf("\n"));
	intersectStats.intersects++;

	bool hit = false;
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	assert(tree);
	switch(intersectTechnique)
	{
		case IT_BSP_FASTEST:
			hit = intersect_bspSRLNP(ray,tree,ray->hitDistanceMax);
			break;
		case IT_BSP_FAST:
			hit = intersect_bspNP(ray,tree,ray->hitDistanceMax);
			break;
		case IT_BSP_COMPACT:
		case IT_BSP_MOST_COMPACT:
			hit = intersect_bsp(ray,tree,ray->hitDistanceMax);
			break;
		default:
			assert(0);
	}

	if(hit) intersectStats.hits++;
	return hit;
}

template IBP
IntersectBsp IBP2::~IntersectBsp()
{
	free(tree);
}

// explicit instantiation
template class IntersectBsp<BspTreeLo<unsigned,32,unsigned> >;
template class IntersectBsp<BspTreeLo<unsigned,32,unsigned short> >;
template class IntersectBsp<BspTreeLo<unsigned short,16,unsigned short> >;
template class IntersectBsp<BspTreeLo<unsigned short,16,unsigned char> >;

} // namespace

#endif
