#include "IntersectBsp.h"

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
	BspTree* tree = (BspTree*)malloc(head.bsp.size);
	readen = fread(tree,1,head.bsp.size,f);
	if(readen == head.bsp.size) return tree;
	free(tree);
	return NULL;
}

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

template IBP
bool IntersectBsp IBP2::intersect_bspSRLNP(RRRay* ray, const BspTree *t, real distanceMax) const
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

	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			assert(0);
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
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
			if(!t->bsp.front) return false;
			t=front;
		} else {
			if(!t->bsp.back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	const void* trianglesEnd=t->bsp.getTrianglesEnd();
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
		if(!t->bsp.back) return false;
		t=back;
	} else {
		if(!t->bsp.front) return false;
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBsp IBP2::intersect_bspNP(RRRay* ray, const BspTree *t, real distanceMax) const
{
begin:
	intersectStats.intersect_bspNP++;
	assert(ray);
	assert(t);

	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			assert(0);
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bspSRLNP(ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	const BspTree *front=t+1;
	const BspTree *back=(const BspTree *)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
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
			if(!t->bsp.front) return false;
			t=front;
		} else {
			if(!t->bsp.back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->bsp.getTrianglesEnd();
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
		if(!t->bsp.back) return false;
		t=back;
	} else {
		if(!t->bsp.front) return false;
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

template IBP
bool IntersectBsp IBP2::intersect_bsp(RRRay* ray, const BspTree* t, real distanceMax) const
{
begin:
	assert(ray);
	assert(t);

	// transition to sons with different size
	if(t->allows_transition && t->bsp.transition)
	{
		#define intersect_bsp_type_func(type,func,ray,tree,distanceMax) ((IntersectBsp<typename type>*)this)->func(ray,(typename type*)tree,distanceMax)
		#define intersect_bsp_type(type,ray,tree,distanceMax) intersect_bsp_type_func(type,intersect_bsp,ray,tree,distanceMax)
		return intersect_bsp_type(BspTree::Transitioneer,ray,t,distanceMax);
	}
	
	// KD
	if(t->bsp.kd)
	{
		intersectStats.intersect_kd++;
		assert(ray->hitDistanceMin<=distanceMax); // rovnost je pripustna, napr kdyz mame projit usecku <5,10> a synove jsou <5,5> a <5,10>

		// test leaf
		if(t->kd.isLeaf()) 
		{
			assert(0);
			/*void *trianglesEnd=t->getTrianglesEnd();
			bool hit=false;
			for(Triankle **triangle=t->getTriangles();triangle<trianglesEnd;triangle++)
				//if((*triangle)->intersectionTime!=__shot)
				if(*triangle!=i_skip)
				{
					//(*triangle)->intersectionTime=__shot;
					if(intersect_triangle_kd(*triangle,i_distanceMin,distanceMax)) 
					{
						distanceMax=MIN(distanceMax,i_hitDistance);
						hit=true;
					}
				}
			return hit;*/
		}

		// test subtrees
		real splitValue = t->kd.getSplitValue();
		real pointMinVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*ray->hitDistanceMin;
		real pointMaxVal = ray->rayOrigin[t->kd.splitAxis]+ray->rayDir[t->kd.splitAxis]*distanceMax;
		if(pointMinVal>splitValue) 
		{
			// front only
			if(pointMaxVal>=splitValue) 
			{
				if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distanceMax);
				t = (BspTree*)t->kd.getFront();
				goto begin;
			}
			// front and back
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distanceMax);
			t = (BspTree*)t->kd.getBack();
			goto begin;
		} else {
			// back only
			// btw if point1[axis]==point2[axis]==splitVertex[axis], testing only back may be sufficient
			if(pointMaxVal<=splitValue) // catches also i_direction[t->axis]==0 case
			{
				if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distanceMax);
				t = (BspTree*)t->kd.getBack();
				goto begin;
			}
			// back and front
			real distSplit = (splitValue-ray->rayOrigin[t->kd.splitAxis])/ray->rayDir[t->kd.splitAxis];
			if(intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getBack(),distSplit)) return true;
			ray->hitDistanceMin = distSplit;
			if(t->kd.transition) return intersect_bsp_type(BspTree::Son,ray,(BspTree*)t->kd.getFront(),distanceMax);
			t = (BspTree*)t->kd.getFront();
			goto begin;
		}
	}

	// BSP
	intersectStats.intersect_bsp++;

	typename BspTree::Son* front=(typename BspTree::Son*)(t+1);
	typename BspTree::Son* back=(typename BspTree::Son*)((char*)front+(t->bsp.front?front->bsp.size:0));
	typename BspTree::_TriInfo* triangle=(typename BspTree::_TriInfo*)((char*)back+(t->bsp.back?back->bsp.size:0));
	assert(triangle<t->bsp.getTrianglesEnd());

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
			if(!t->bsp.front) return false;
			if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
			t=(BspTree*)front;
		} else {
			if(!t->bsp.back) return false;
			if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
			t=(BspTree*)back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->bsp.front && intersect_bsp_type(BspTree::Son,ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->bsp.back && intersect_bsp_type(BspTree::Son,ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	void* trianglesEnd=t->bsp.getTrianglesEnd();
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
#ifdef FILL_HITPLANE
			if(ray->flags&RRRay::FILL_PLANE)
			{
				real siz = size(n);
				ray->hitPlane[0] = n.x/siz;
				ray->hitPlane[1] = n.y/siz;
				ray->hitPlane[2] = n.z/siz;
				ray->hitPlane[3] = n.d/siz;
			}
#endif
#ifdef FILL_HITPOINT3D
			if(ray->flags&RRRay::FILL_POINT3D)
			{
				update_hitPoint3d(ray,distancePlane);
			}
#endif
			DBGLINE
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->bsp.back) return false;
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,back,distanceMax);
		t=(BspTree*)back;
	} else {
		if(!t->bsp.front) return false;
		ray->hitDistanceMin=distancePlane-DELTA_BSP;
		if(t->bsp.transition) return intersect_bsp_type(BspTree::Son,ray,front,distanceMax);
		t=(BspTree*)front;
	}
	goto begin;
}


template IBP
IntersectBsp IBP2::IntersectBsp(RRObjectImporter* aimporter, IntersectTechnique intersectTechnique, char* ext, int effort) : IntersectLinear(aimporter, intersectTechnique)
{
	tree = NULL;
	if(!triangles) return;
	bool retried = false;
	char name[300];
	getFileName(name,300,aimporter,ext);
	FILE* f = (effort<=0) ? NULL : fopen(name,"rb");
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
			bool ok = createAndSaveBsp IBP2(f,&obj,abs(effort));
			fclose(f);
			if(!ok)
			{
				printf("Failed to write tree (%s)...\n",name);
				//remove(name);
				f = fopen(name,"wb");
				fclose(f);
				retried = true;
			}
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
		+(tree?tree->bsp.size:0)
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
	free((void*)tree);
}

// explicit instantiation

// single-level bsp
template class IntersectBsp<BspTree44>; // for size 0..2^30-1         [65537..2^32 triangles]
template class IntersectBsp<BspTree42>; // for 257..65536 triangles   [size 0..2^30-1]
template class IntersectBsp<BspTree22>; // for size 0..16383          [257..65536 triangles]
template class IntersectBsp<BspTree21>; // for 0..256 triangles       [size 0..16383]

// multi-level bsp
template class IntersectBsp< BspTree14>;
template class IntersectBsp<CBspTree24>;
template class IntersectBsp<CBspTree44>; // for 65537..2^32 triangles

template class IntersectBsp< BspTree12>;
template class IntersectBsp<CBspTree22>;
template class IntersectBsp<CBspTree42>; // for 257..65536 triangles

template class IntersectBsp< BspTree11>;
template class IntersectBsp<CBspTree21>;
template class IntersectBsp<CBspTree41>; // for 0..256 triangles

} // namespace
