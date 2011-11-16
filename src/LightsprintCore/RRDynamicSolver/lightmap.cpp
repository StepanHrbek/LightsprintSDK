// --------------------------------------------------------------------------
// Offline build of lightmaps and bent normal maps.
// Copyright (c) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cmath> // necessary for mingw gcc4.3
#include <cfloat>
#include <cstdio> // sprintf
#include <vector>
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"
#include "gather.h"

//#define ITERATE_MULTIMESH // older version with very small inefficiency

namespace rr
{

RRReal getArea(RRVec2 v0, RRVec2 v1, RRVec2 v2)
{
	RRReal a = (v1-v0).length();
	RRReal b = (v2-v1).length();
	RRReal c = (v0-v2).length();
	RRReal s = (a+b+c)*0.5f;
	RRReal area = sqrtf(s*(s-a)*(s-b)*(s-c));
	//RR_ASSERT(_finite(area)); this is legal, enumerateTexelsPartial tests whether getArea() is finite
	return area;
}

struct UnwrapStatistics
{
	// enumeration parameters (for gathering extra statistics)
	bool subtexelsInMapSpace; // makes enumerateTexels produce subtexels with unInTriangleSpace in map space

	// stats for missing
	unsigned numTrianglesWithoutUnwrap;
	unsigned numTrianglesWithUnwrap;

	// stats for out of range
	unsigned pass; // texels in lmaps over 512*512 are enumerated in several passes, to reduce memory footprint
	unsigned numTrianglesWithUnwrapOutOfRange;
	float areaOfAllTriangles; // sum(triangle's unwrap area), valid only if subtexelsInMapSpace
	float areaOfAllTrianglesInRange; // sum(triangle's unwrap area in range), valid only if subtexelsInMapSpace

	// stats for coverage
	unsigned numTexelsProcessed;

	UnwrapStatistics()
	{
		// enumeration parameters
		subtexelsInMapSpace = false;
		// stats for missing
		numTrianglesWithoutUnwrap = 0;
		numTrianglesWithUnwrap = 0;
		// stats for out of range
		pass = 0;
		numTrianglesWithUnwrapOutOfRange = 0;
		areaOfAllTriangles = 0;
		areaOfAllTrianglesInRange = 0;
		// stats for coverage
		numTexelsProcessed = 0;
	}		
};

bool enumerateTexelsPartial(const RRObject* multiObject, unsigned objectNumber,
		unsigned mapWidth, unsigned mapHeight,
		unsigned rectXMin, unsigned rectYMin, unsigned rectXMaxPlus1, unsigned rectYMaxPlus1, 
		ProcessTexelResult (callback)(const struct ProcessTexelParams& pti), const TexelContext& tc,
		RRReal minimalSafeDistance, UnwrapStatistics& unwrapStatistics, int onlyTriangleNumber=-1)
{
	if (!multiObject)
	{
		RR_ASSERT(0);
		return false;
	}
	const RRMesh* multiMesh = multiObject->getCollider()->getMesh();

	// 1. preallocate texels
	unsigned numTexelsInRect = (rectXMaxPlus1-rectXMin)*(rectYMaxPlus1-rectYMin);
	TexelSubTexels* texelsRect = NULL;
	try
	{
		texelsRect = new TexelSubTexels[numTexelsInRect];
	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, lightmap not updated(1).\n");
		return false;
	}
	TexelSubTexels::Allocator subTexelAllocator; // pool, memory is freed when it gets out of scope
	unsigned multiPostImportTriangleNumber = 0; // filled in next step

	// 2. populate texels with subtexels
	try
	{
	// iterate only triangles in singlemesh
	const RRObject* singleObject = tc.solver ? tc.solver->getStaticObjects()[objectNumber] : multiObject; // safe objectNumber, checked in updateLightmap()
	const RRMesh* singleMesh = singleObject->getCollider()->getMesh();
	unsigned numSinglePostImportTriangles = singleMesh->getNumTriangles();
	for (unsigned singlePostImportTriangle=0;singlePostImportTriangle<numSinglePostImportTriangles;singlePostImportTriangle++)
	{
		RRMesh::PreImportNumber multiPreImportTriangle;
		multiPreImportTriangle.object = objectNumber;
		multiPreImportTriangle.index = singleMesh->getPreImportTriangle(singlePostImportTriangle).index;
		unsigned multiPostImportTriangle = multiMesh->getPostImportTriangle(multiPreImportTriangle);
		if (multiPostImportTriangle!=UINT_MAX && (onlyTriangleNumber<0 || onlyTriangleNumber==multiPostImportTriangle))
		{
			unsigned t = multiPostImportTriangle;
			// gather data about triangle t
			RRMesh::TriangleMapping mapping;
			{
				const RRMaterial* material = singleObject->getTriangleMaterial(singlePostImportTriangle,NULL,NULL);
				unsigned lightmapTexcoord = material ? material->lightmapTexcoord : UINT_MAX;
				if (singleMesh->getTriangleMapping(singlePostImportTriangle,mapping,lightmapTexcoord))
				{
					unwrapStatistics.numTrianglesWithUnwrap++;
				}
				else
				{
					unwrapStatistics.numTrianglesWithoutUnwrap++;
					continue; // early exit from triangle without unwrap
				}
			}
			// remember any triangle in selected object, for relevantLights
			multiPostImportTriangleNumber = t;

			// rasterize triangle t
			//  find minimal bounding box
			unsigned xminu, xmaxu, yminu, ymaxu;
			{
				RRReal xmin = mapWidth  * RR_MIN(mapping.uv[0][0],RR_MIN(mapping.uv[1][0],mapping.uv[2][0]));
				RRReal xmax = mapWidth  * RR_MAX(mapping.uv[0][0],RR_MAX(mapping.uv[1][0],mapping.uv[2][0]));
				xminu = (unsigned)RR_MAX(xmin,rectXMin); // !negative
				xmaxu = (unsigned)RR_CLAMPED(xmax+1,0,rectXMaxPlus1); // !negative
				RRReal ymin = mapHeight * RR_MIN(mapping.uv[0][1],RR_MIN(mapping.uv[1][1],mapping.uv[2][1]));
				RRReal ymax = mapHeight * RR_MAX(mapping.uv[0][1],RR_MAX(mapping.uv[1][1],mapping.uv[2][1]));
				yminu = (unsigned)RR_MAX(ymin,rectYMin); // !negative
				ymaxu = (unsigned)RR_CLAMPED(ymax+1,0,rectYMaxPlus1); // !negative
				if (unwrapStatistics.pass==0)
				{
					if (unwrapStatistics.subtexelsInMapSpace)
					{
						RRReal triangleAreaInMapSpace = getArea(mapping.uv[0],mapping.uv[1],mapping.uv[2]);
						unwrapStatistics.areaOfAllTriangles += triangleAreaInMapSpace;
					}
					if (xmin<0 || xmax>mapWidth || ymin<0 || ymax>mapHeight)
						unwrapStatistics.numTrianglesWithUnwrapOutOfRange++;
				}
				if (yminu>=ymaxu || xminu>=xmaxu) continue; // early exit from triangle outside our rectangle
			}
			//  prepare mapspace -> trianglespace matrix
			RRReal m[3][3] = {
				{ mapping.uv[1][0]-mapping.uv[0][0], mapping.uv[2][0]-mapping.uv[0][0], mapping.uv[0][0] },
				{ mapping.uv[1][1]-mapping.uv[0][1], mapping.uv[2][1]-mapping.uv[0][1], mapping.uv[0][1] },
				{ 0,0,1 } };
			RRReal det = m[0][0]*m[1][1]*m[2][2]+m[0][1]*m[1][2]*m[2][0]+m[0][2]*m[1][0]*m[2][1]-m[0][0]*m[1][2]*m[2][1]-m[0][1]*m[1][0]*m[2][2]-m[0][2]*m[1][1]*m[2][0];
			if (!det) continue; // skip degenerated triangles
			RRReal invdet = 1/det;
			RRReal inv[2][3] = {
				{ (m[1][1]*m[2][2]-m[1][2]*m[2][1])*invdet/mapWidth, (m[0][2]*m[2][1]-m[0][1]*m[2][2])*invdet/mapHeight, (m[0][1]*m[1][2]-m[0][2]*m[1][1])*invdet },
				{ (m[1][2]*m[2][0]-m[1][0]*m[2][2])*invdet/mapWidth, (m[0][0]*m[2][2]-m[0][2]*m[2][0])*invdet/mapHeight, (m[0][2]*m[1][0]-m[0][0]*m[1][2])*invdet },
				//{ (m[1][0]*m[2][1]-m[1][1]*m[2][0])*invdet, (m[0][1]*m[2][0]-m[0][0]*m[2][1])*invdet, (m[0][0]*m[1][1]-m[0][1]*m[1][0])*invdet }
				};
			RRReal triangleAreaInMapSpace = getArea(mapping.uv[0],mapping.uv[1],mapping.uv[2]);
			//  for all texels in bounding box
			for (unsigned y=yminu;y<ymaxu;y++)
			{
				for (unsigned x=xminu;x<xmaxu;x++)
				{
					if (!tc.solver || ((tc.params->debugTexel==UINT_MAX || tc.params->debugTexel==x+y*mapWidth) && !tc.solver->aborting)) // process only texel selected for debugging
					{
						// start with full texel, 4 vertices
						unsigned polySize = 4;
						RRVec2 polyVertexInTriangleSpace[7] =
						{
							#define MAPSPACE_TO_TRIANGLESPACE(x,y) RRVec2( (x)*inv[0][0] + (y)*inv[0][1] + inv[0][2], (x)*inv[1][0] + (y)*inv[1][1] + inv[1][2] )
							MAPSPACE_TO_TRIANGLESPACE(x,y),
							MAPSPACE_TO_TRIANGLESPACE(x+1,y),
							MAPSPACE_TO_TRIANGLESPACE(x+1,y+1),
							MAPSPACE_TO_TRIANGLESPACE(x,y+1),
							#undef MAPSPACE_TO_TRIANGLESPACE
						};
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[0]));
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[1]));
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[2]));
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[3]));

						// look for NaN areas (they exist because of limited float precision)
						//for (unsigned i=0;i<polySize-2;i++)
						//	getArea(polyVertexInTriangleSpace[0],polyVertexInTriangleSpace[i+1],polyVertexInTriangleSpace[i+2]);

						// calculate texel-triangle intersection (=polygon) in 2d 0..1 map space
						// cut it three times by triangle side
						for (unsigned triSide=0;triSide<3;triSide++)
						{
							RRVec3 triLineInTriangleSpace = (triSide==0) ? RRVec3(0,1,0) : ((triSide==1) ? RRVec3(-1,-1,1) : RRVec3(1,0,0) );
							// are poly vertices inside or outside halfplane defined by triLine?
							enum InsideOut
							{
								INSIDE, // distance is +, potentially inside triangle
								EDGE, // distance is 0, potentially touches triangle
								OUTSIDE // distance is -, outside triangle and this is for sure
							};
							InsideOut inside[7];
							unsigned numInside = 0;
							unsigned numOutside = 0;
							for (unsigned i=0;i<polySize;i++)
							{
#define POINT_LINE_DISTANCE_2D(point,line) ((line)[0]*(point)[0]+(line)[1]*(point)[1]+(line)[2])
								RRReal dist = POINT_LINE_DISTANCE_2D(polyVertexInTriangleSpace[i],triLineInTriangleSpace);
								if (dist<0)
								{
									inside[i] = OUTSIDE;
									numOutside++;
								}
								else
								if (dist>0)
								{
									inside[i] = INSIDE;
									numInside++;
								}
								else
								{
									inside[i] = EDGE;
								}
							}
							// none OUTSIDE -> don't modify poly, go to next triangle side
							if (!numOutside) continue;
							// none INSIDE -> empty poly, go to next texel
							if (!numInside) {polySize = 0; break;}
							// part INSIDE, part OUTSIDE -> cut off all OUTSIDE and EDGE, add 2 new vertices
							unsigned firstPreserved = 1;
							while (inside[(firstPreserved-1)%polySize]==INSIDE || inside[firstPreserved%polySize]!=INSIDE) firstPreserved++;
							RRVec2 polyVertexInTriangleSpaceOrig[7];
							memcpy(polyVertexInTriangleSpaceOrig,polyVertexInTriangleSpace,sizeof(polyVertexInTriangleSpace));
							unsigned src = firstPreserved;
							unsigned dst = 0;
							while (inside[src%polySize]==INSIDE) polyVertexInTriangleSpace[dst++] = polyVertexInTriangleSpaceOrig[src++%polySize]; // copy preserved vertices
							#define INTERSECTION_POINTA_POINTB_LINE(pointA,pointB,line) \
								((pointA) - ((pointB)-(pointA)) * POINT_LINE_DISTANCE_2D(pointA,line) / ( (line)[0]*((pointB)[0]-(pointA)[0]) + (line)[1]*((pointB)[1]-(pointA)[1]) ) )
							polyVertexInTriangleSpace[dst++] =
								INTERSECTION_POINTA_POINTB_LINE(polyVertexInTriangleSpaceOrig[(src-1)%polySize],polyVertexInTriangleSpaceOrig[src%polySize],triLineInTriangleSpace); // append new vertex
							polyVertexInTriangleSpace[dst++] =
								INTERSECTION_POINTA_POINTB_LINE(polyVertexInTriangleSpaceOrig[(firstPreserved-1)%polySize],polyVertexInTriangleSpaceOrig[firstPreserved%polySize],triLineInTriangleSpace); // append new vertex
							RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[dst-2]));
							RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[dst-1]));
							polySize = dst;

							// look for NaN areas (they exist because of limited float precision)
							//for (unsigned i=0;i<polySize-2;i++)
							//	getArea(polyVertexInTriangleSpace[0],polyVertexInTriangleSpace[i+1],polyVertexInTriangleSpace[i+2]);
						}
						// triangulate polygon into subtexels
						if (polySize)
						{
							TexelSubTexels& texel = texelsRect[(x-rectXMin)+(y-rectYMin)*(rectXMaxPlus1-rectXMin)];

							#define TRIANGLESPACE_TO_MAPSPACE(xy) RRVec2( (xy.x)*m[0][0] + (xy.y)*m[0][1] + m[0][2], (xy.x)*m[1][0] + (xy.y)*m[1][1] + m[1][2] )
							#define MAPSPACE_TO_TEXELSPACE(xy) RRVec2( (xy.x)*mapWidth-x-0.5f, (xy.y)*mapHeight-y-0.5f )
							RRVec2 vertexInMapSpaceOld = TRIANGLESPACE_TO_MAPSPACE(polyVertexInTriangleSpace[polySize-1]);
							RRVec2 vertexInTexelSpaceOld = MAPSPACE_TO_TEXELSPACE(vertexInMapSpaceOld);
							for (unsigned i=0;i<polySize;i++)
							{
								RRVec2 vertexInMapSpace = TRIANGLESPACE_TO_MAPSPACE(polyVertexInTriangleSpace[i]);

								// checkUnwrapConsistency() asks us to return subtexels in lightmap space,
								// convert them
								// numerical stability is poor (vertices are converted to trispace and back), analysis must take tiny overlaps as false positives
								if (unwrapStatistics.subtexelsInMapSpace)
								{
									polyVertexInTriangleSpace[i] = vertexInMapSpace;
								}

								// set texel flags
								#define ERROR_IN_TEXELS 0.03f // we expect subtexel vertex to be off by this fraction of texel
								RRVec2 vertexInTexelSpace = MAPSPACE_TO_TEXELSPACE(vertexInMapSpace);
								if (vertexInTexelSpace.x<-0.5f+ERROR_IN_TEXELS) texel.texelFlags |= EDGE_X0;
								if (vertexInTexelSpace.x>0.5f-ERROR_IN_TEXELS) texel.texelFlags |= EDGE_X1;
								if (vertexInTexelSpace.y<-0.5f+ERROR_IN_TEXELS) texel.texelFlags |= EDGE_Y0;
								if (vertexInTexelSpace.y>0.5f-ERROR_IN_TEXELS) texel.texelFlags |= EDGE_Y1;
								if (vertexInTexelSpace.x<0 && vertexInTexelSpace.y<0) texel.texelFlags |= QUADRANT_X0Y0;
								if (vertexInTexelSpace.x>0 && vertexInTexelSpace.y<0) texel.texelFlags |= QUADRANT_X1Y0;
								if (vertexInTexelSpace.x<0 && vertexInTexelSpace.y>0) texel.texelFlags |= QUADRANT_X0Y1;
								if (vertexInTexelSpace.x>0 && vertexInTexelSpace.y>0) texel.texelFlags |= QUADRANT_X1Y1;
								// diagonal edge goes through 3 quadrants, one without vertex, we must set its flag here
								if (vertexInTexelSpaceOld.x<0 != vertexInTexelSpace.x<0 && vertexInTexelSpaceOld.y<0 != vertexInTexelSpace.y<0)
								{
									float a = (vertexInTexelSpace.y-vertexInTexelSpaceOld.y)/(vertexInTexelSpace.x-vertexInTexelSpaceOld.x);
									float b = vertexInTexelSpace.y-a*vertexInTexelSpace.x;
									// kde krizi osu y? v b. podle b pridat dva dolni nebo dva horni kvadranty
									texel.texelFlags |= (b<0) ? (QUADRANT_X0Y0|QUADRANT_X1Y0) : (QUADRANT_X0Y1|QUADRANT_X1Y1);
									// kde krizi osu x? v -b/a. podle -b/a pridat dva leve nebo dva prave kvadranty
									texel.texelFlags |= (-b/a<0) ? (QUADRANT_X0Y0|QUADRANT_X0Y1) : (QUADRANT_X1Y0|QUADRANT_X1Y1);
								}
								vertexInTexelSpaceOld = vertexInTexelSpace;
							}
							#undef MAPSPACE_TO_TEXELSPACE
							#undef TRIANGLESPACE_TO_MAPSPACE
							SubTexel subTexel;
							subTexel.multiObjPostImportTriIndex = t;
							subTexel.uvInTriangleSpace[0] = polyVertexInTriangleSpace[0];
							for (unsigned i=0;i<polySize-2;i++)
							{
								subTexel.uvInTriangleSpace[1] = polyVertexInTriangleSpace[i+1];
								subTexel.uvInTriangleSpace[2] = polyVertexInTriangleSpace[i+2];
								RRReal subTexelAreaInTriangleSpace = getArea(subTexel.uvInTriangleSpace[0],subTexel.uvInTriangleSpace[1],subTexel.uvInTriangleSpace[2]);
								unwrapStatistics.areaOfAllTrianglesInRange += subTexelAreaInTriangleSpace;
								subTexel.areaInMapSpace = subTexelAreaInTriangleSpace * triangleAreaInMapSpace;
								if (_finite(subTexel.areaInMapSpace)) // skip subtexels of NaN area (they exist because of limited float precision)
									texel.push_back(subTexel
										,subTexelAllocator
										);
							}
						}
					}
				}
			}
		}
	}
	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, lightmap not updated(2).\n");
		delete[] texelsRect;
		return false;
	}

	// 4. preallocate and populate relevantLights
#ifdef _OPENMP
	int numThreads = omp_get_max_threads();
#else
	int numThreads = 1;
#endif
	unsigned numAllLights = tc.solver ? tc.solver->getLights().size() : 0;
	unsigned numRelevantLights = 0;
	const RRLight** relevantLightsForObject = new const RRLight*[numAllLights*numThreads];
	for (unsigned i=0;i<numAllLights;i++)
	{
		RRLight* light = tc.solver->getLights()[i];
		if (light && light->enabled && multiObject->getTriangleMaterial(multiPostImportTriangleNumber,light,NULL))
		{
			for (int k=0;k<numThreads;k++)
				relevantLightsForObject[k*numAllLights+numRelevantLights] = light;
			numRelevantLights++;
		}
	}

	// 5. gather, shoot rays from texels
	unsigned numTexelsProcessed = 0;
	#pragma omp parallel for schedule(dynamic) reduction(+:numTexelsProcessed)
	for (int j=(int)rectYMin;j<(int)rectYMaxPlus1;j++)
	{
#ifdef _OPENMP
		int threadNum = omp_get_thread_num();
#else
		int threadNum = 0;
#endif
		for (int i=(int)rectXMin;i<(int)rectXMaxPlus1;i++)
		{
			unsigned indexInRect = (i-rectXMin)+(j-rectYMin)*(rectXMaxPlus1-rectXMin);
			if (texelsRect[indexInRect].size())
			{
				if (!tc.solver || ((tc.params->debugTexel==UINT_MAX || tc.params->debugTexel==i+j*mapWidth) && !tc.solver->aborting)) // process only texel selected for debugging
				{
					ProcessTexelParams ptp(tc);
					ptp.uv[0] = i;
					ptp.uv[1] = j;
					ptp.subTexels = texelsRect+indexInRect;
					ptp.rayLengthMin = minimalSafeDistance;
					ptp.relevantLights = relevantLightsForObject+numAllLights*threadNum;
					ptp.numRelevantLights = numRelevantLights;
					ptp.relevantLightsFilled = true;
					callback(ptp);
					numTexelsProcessed++;
				}
			}
		}
	}
	unwrapStatistics.numTexelsProcessed += numTexelsProcessed;

	// 6. cleanup
	delete[] relevantLightsForObject;
	delete[] texelsRect;

	return true;
}

//! Enumerates all texels on object's surface.
//
//! Default implementation runs callbacks on all CPUs/cores at once.
//! Callback receives list of all triangles intersecting texel.
//! It is not called for texels with empty list.
//! \n Enumeration order is not defined.
//! \param objectNumber
//!  Number of object in this scene.
//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
//! \param mapWidth
//!  Width of map that wraps object's surface in texels.
//!  No map is used here, but coordinates are generated for map of this size.
//! \param mapHeight
//!  Height of map that wraps object's surface in texels.
//!  No map is used here, but coordinates are generated for map of this size.
//! \param callback
//!  Function called for each enumerated texel. Must be thread safe.
//! \param context
//!  Context is passed unchanged to callback.
//! \param unwrapStatistics
//!  Values in this structure are incremented.
//! \return False if failed to enumerate all texels.
bool enumerateTexelsFull(const RRObject* multiObject, unsigned objectNumber, unsigned mapWidth, unsigned mapHeight, ProcessTexelResult (callback)(const struct ProcessTexelParams& pti), const TexelContext& tc, RRReal minimalSafeDistance, UnwrapStatistics& unwrapStatistics, int onlyTriangleNumber=-1)
{
	enum {MAX_TEXELS_PER_PASS=512*512};
	unsigned numTexelsInMap = mapWidth*mapHeight;
	unsigned numPasses = (numTexelsInMap+MAX_TEXELS_PER_PASS-1)/MAX_TEXELS_PER_PASS;
	if (numPasses==0)
		return false;
	if (mapWidth<=mapHeight)
	{
		for (unsigned i=0;i<numPasses;i++)
		{
			unwrapStatistics.pass = i;
			if (!enumerateTexelsPartial(multiObject, objectNumber, mapWidth, mapHeight, 0,mapHeight*i/numPasses,mapWidth,mapHeight*(i+1)/numPasses, callback, tc, minimalSafeDistance, unwrapStatistics, onlyTriangleNumber))
				return false;
		}
	}
	else
	{
		for (unsigned i=0;i<numPasses;i++)
		{
			unwrapStatistics.pass = i;
			if (!enumerateTexelsPartial(multiObject, objectNumber, mapWidth, mapHeight, mapHeight*i/numPasses,0,mapWidth*(i+1)/numPasses,mapHeight, callback, tc, minimalSafeDistance, unwrapStatistics, onlyTriangleNumber))
				return false;
		}
	}
	return true;
}

// helper for RRObject::checkConsistency()
const char* checkUnwrapConsistency(const RRObject* object)
{
	enum
	{
		MAP_WIDTH = 64,
	};
	struct UnwrapStatisticsEx : public UnwrapStatistics
	{
		unsigned numTexels;
		unsigned numTexelsWithOverlap;
		UnwrapStatisticsEx()
		{
			numTexels = MAP_WIDTH*MAP_WIDTH;
			numTexelsWithOverlap = 0;
		}
		static bool isPointInsideTriangle(const RRVec2& point, const RRVec2* triangle)
		{
			RRVec2 s = triangle[0];
			RRVec2 r = triangle[1]-triangle[0];
			RRVec2 l = triangle[2]-triangle[0];
			float b = ((point[1]-s[1])*r[0] - (point[0]-s[0])*r[1]);
			float a = ((point[0]-s[0])*l[1] - (point[1]-s[1])*l[0]);
			float d = l[1]*r[0] - l[0]*r[1];
			float epsilon = d*0.001f;
			if (d>0)
				return a>epsilon && b>epsilon && d-a-b>epsilon;
			else
				return a<epsilon && b<epsilon && d-a-b<epsilon;
		}
		static ProcessTexelResult callback(const struct ProcessTexelParams& pti)
		{
			UnwrapStatisticsEx* us = reinterpret_cast<UnwrapStatisticsEx*>(pti.context.singleObjectReceiver);

			// do subtexels overlap?
			if (pti.subTexels->size()>1)
			{
				// measure area of subtexels, ignoring overlap
				float sumOfTriangleAreasInMap = 0;
				for (TexelSubTexels::const_iterator i=pti.subTexels->begin();i!=pti.subTexels->end();++i)
				{
					sumOfTriangleAreasInMap += i->areaInMapSpace;
				}

				if (sumOfTriangleAreasInMap*us->numTexels<1.05f)
				{
					// sample 40 random points in subtexels
					TexelSubTexels::const_iterator subTexelIterator = pti.subTexels->begin();
					RRReal areaAccu = -pti.subTexels->begin()->areaInMapSpace;
					unsigned numSamples = 20;
					RRReal areaStep = sumOfTriangleAreasInMap/(numSamples+0.91f);
					for (unsigned seriesShooterNum=0;seriesShooterNum<numSamples;seriesShooterNum++)
					{
						// select random subtexel
						areaAccu += areaStep;
						while (areaAccu>0)
						{
							++subTexelIterator;
							if (subTexelIterator==pti.subTexels->end())
								subTexelIterator = pti.subTexels->begin();
							areaAccu -= subTexelIterator->areaInMapSpace;
						}
						const SubTexel* subTexel = *subTexelIterator;
						// select random point in subtexel
						unsigned u=rand();
						unsigned v=rand();
						if (u+v>RAND_MAX)
						{
							u=RAND_MAX-u;
							v=RAND_MAX-v;
						}
						RRVec2 point(subTexel->uvInTriangleSpace[0] + (subTexel->uvInTriangleSpace[1]-subTexel->uvInTriangleSpace[0])*(u+0.5f)/(RAND_MAX+1.f) + (subTexel->uvInTriangleSpace[2]-subTexel->uvInTriangleSpace[0])*(v+0.5f)/(RAND_MAX+1.f));
						// test point against all other triangles
						for (TexelSubTexels::const_iterator subTexel2=pti.subTexels->begin();subTexel2!=pti.subTexels->end();++subTexel2)
						{
							if (*subTexel2!=subTexel && isPointInsideTriangle(point,subTexel2->uvInTriangleSpace))
							{
								goto overlap_found;
							}
						}
					}
				}
				else
				{
					// sum of subtexels is too large to fit in texel, must overlap
					overlap_found:
					// multiple threads modify us at once
					// we could ensure atomicity by #pragma omp atomic but we don't need absolutely exact result, speed is better
					us->numTexelsWithOverlap++;
				}
			}

			// return anything, it is not used
			return ProcessTexelResult();
		}
	};
	unsigned numTriangles = object->getCollider()->getMesh()->getNumTriangles();
	if (numTriangles)
	{
		UnwrapStatisticsEx us;
		us.subtexelsInMapSpace = true;
		TexelContext tc;
		memset(&tc,0,sizeof(tc));
		tc.singleObjectReceiver = reinterpret_cast<RRObject*>(&us);
		enumerateTexelsFull(object,-1,MAP_WIDTH,MAP_WIDTH,us.callback,tc,0,us);
		float missing = us.numTrianglesWithoutUnwrap/(float)numTriangles;
		bool missingBad = missing>0.02f;
		bool missingPoor = missing>0;
		float outofrangeT = us.numTrianglesWithUnwrapOutOfRange/(float)numTriangles;
		float outofrangeA = (outofrangeT && us.areaOfAllTriangles) ? (us.areaOfAllTriangles-us.areaOfAllTrianglesInRange)/us.areaOfAllTriangles : 0;
		bool outofrangeBad = outofrangeA>0.02f;
		bool outofrangePoor = outofrangeT!=0;
		float overlap = us.numTexelsWithOverlap/(float)(us.numTexelsProcessed?us.numTexelsProcessed:1);
		bool overlapBad = overlap>0.02f;
		bool overlapPoor = overlap>0.01f;
		float coverage = us.numTexelsProcessed/(float)us.numTexels;
		bool coverageBad = us.numTexelsProcessed<=1;
		bool coveragePoor = coverage<0.05f;
		bool badUnwrap =  missingBad || outofrangeBad || overlapBad || coverageBad;
		bool poorUnwrap =  missingPoor || outofrangePoor || overlapPoor || coveragePoor;
		if (poorUnwrap)
		{
			static char buf[150];
			sprintf(buf,badUnwrap?"Bad unwrap":"Imperfect unwrap");
			if (missingPoor) sprintf(strchr(buf,0),", %d%% missing",(int)(missing*100));
			if (outofrangeBad) sprintf(strchr(buf,0),", %d%% out of range(%d%% triangles)",(int)(outofrangeA*100),(int)(outofrangeT*100));
			if (overlapPoor) sprintf(strchr(buf,0),", %d%% overlap",(int)(overlap*100));
			if (coveragePoor) sprintf(strchr(buf,0),", %d%% coverage",(int)(coverage*100));
			if (outofrangePoor && !outofrangeBad) sprintf(strchr(buf,0),", %d%% triangles slightly out of range",(int)(outofrangeT*100));
			return buf;
		}
	}
	return NULL;
}

RRBuffer* onlyVbuf(RRBuffer* buffer)
{
	return (buffer && buffer->getType()==BT_VERTEX_BUFFER) ? buffer : NULL;
}
RRBuffer* onlyLmap(RRBuffer* buffer)
{
	return (buffer && buffer->getType()==BT_2D_TEXTURE) ? buffer : NULL;
}

unsigned RRDynamicSolver::updateLightmap(int objectNumber, RRBuffer* buffer, RRBuffer* directionalLightmaps[3], RRBuffer* bentNormals, const UpdateParameters* _params, const FilteringParameters* _filtering)
{
	if (aborting)
		return 0;
/*
	static unsigned s_updates = 0; s_updates++;
	static unsigned s_sec = 5;
	static double s_start = 0;
	double now = omp_get_wtime();
	if (!s_start) s_start = now;
	if (now-s_start>s_sec)
	{
		RRReporter::report(INF1,"%ds: %.1f updates/s\n",s_sec,(float)(s_updates/(now-s_start)));
		s_sec+=5;
	}
*/
	bool realtime = buffer && buffer->getType()==BT_VERTEX_BUFFER && !bentNormals && (!_params || (!_params->applyLights && !_params->applyEnvironment && !_params->quality));
	RRReportInterval report(realtime?INF3:INF2,"Updating object %d/%d '%s', %s %d*%d, directional %d*%d, bent normals %d*%d...\n",
		objectNumber,getStaticObjects().size(),((unsigned)objectNumber<getStaticObjects().size())?getStaticObjects()[objectNumber]->name.c_str():"",
		(buffer && buffer->getType()==BT_VERTEX_BUFFER)?"vertex buffer":"lightmap",
		buffer?buffer->getWidth():0,buffer?buffer->getHeight():0,
		(directionalLightmaps&&directionalLightmaps[0])?directionalLightmaps[0]->getWidth():0,
		(directionalLightmaps&&directionalLightmaps[0])?directionalLightmaps[0]->getHeight():0,
		// other two directional sizes not reported to make report shorter
		//(directionalLightmaps&&directionalLightmaps[1])?directionalLightmaps[1]->getWidth():0,
		//(directionalLightmaps&&directionalLightmaps[1])?directionalLightmaps[1]->getHeight():0,
		//(directionalLightmaps&&directionalLightmaps[2])?directionalLightmaps[2]->getWidth():0,
		//(directionalLightmaps&&directionalLightmaps[2])?directionalLightmaps[2]->getHeight():0,
		bentNormals?bentNormals->getWidth():0,bentNormals?bentNormals->getHeight():0);
	
	// init params
	UpdateParameters params;
	if (_params) params = *_params;
	if (params.applyLights && !getLights().size())
		params.applyLights = false;
	if (params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;
	bool paramsAllowRealtime = !params.applyLights && !params.applyEnvironment && params.applyCurrentSolution && !params.quality;

	// init solver
	if ((!priv->scene
		&& !priv->packedSolver
		) || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0);
		if ( (!priv->scene
			&& !priv->packedSolver
			) || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"RRDynamicSolver::updateLightmap: Empty scene.\n"));
			return 0;
		}
	}

	// init buffers
	unsigned numVertexBuffers = 0;
	unsigned numPixelBuffers = 0;
	RRBuffer* allVertexBuffers[NUM_BUFFERS];
	RRBuffer* allPixelBuffers[NUM_BUFFERS];
	unsigned pixelBufferWidth = 0;
	unsigned pixelBufferHeight = 0;
	unsigned vertexBufferWidth = 0;
	{
		if (paramsAllowRealtime && objectNumber==-1)
		{
			vertexBufferWidth = getMultiObjectCustom()->getCollider()->getMesh()->getNumVertices(); // [multiobj indir is indexed]
		}
		else
		{
			if (objectNumber>=(int)getStaticObjects().size() || objectNumber<0)
			{
				RRReporter::report(WARN,"Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getStaticObjects().size()-1);
				return 0;
			}
			vertexBufferWidth = getStaticObjects()[objectNumber]->getCollider()->getMesh()->getNumVertices();
		}

		RRBuffer* allBuffers[NUM_BUFFERS];
		allBuffers[LS_LIGHTMAP] = buffer;
		allBuffers[LS_DIRECTION1] = directionalLightmaps?directionalLightmaps[0]:NULL;
		allBuffers[LS_DIRECTION2] = directionalLightmaps?directionalLightmaps[1]:NULL;
		allBuffers[LS_DIRECTION3] = directionalLightmaps?directionalLightmaps[2]:NULL;
		allBuffers[LS_BENT_NORMALS] = bentNormals;

		for (unsigned i=0;i<NUM_BUFFERS;i++)
		{
			allVertexBuffers[i] = onlyVbuf(allBuffers[i]); if (allVertexBuffers[i]) numVertexBuffers++;
			allPixelBuffers[i] = onlyLmap(allBuffers[i]); if (allPixelBuffers[i]) numPixelBuffers++;
		}
		if (numVertexBuffers+numPixelBuffers==0)
		{
			RRReporter::report(WARN,"No output buffers, no work to do.\n");
			return 0;
		}
		for (unsigned i=0;i<NUM_BUFFERS;i++)
		{
			if (allVertexBuffers[i])
			{
				if (allBuffers[i]->getWidth()!=vertexBufferWidth)
				{
					RRReporter::report(WARN,"Wrong vertex buffer size %d, should be %d.\n",allBuffers[i]->getWidth(),vertexBufferWidth);
					return 0;
				}
			}
		}
		bool filled = false;
		for (unsigned i=0;i<NUM_BUFFERS;i++)
		{
			if (allPixelBuffers[i])
			{
				if (!filled)
				{
					filled = true;
					pixelBufferWidth = allBuffers[i]->getWidth();
					pixelBufferHeight = allBuffers[i]->getHeight();
				}
				else
				{
					if (pixelBufferWidth!=allBuffers[i]->getWidth() || pixelBufferHeight!=allBuffers[i]->getHeight())
					{
						RRReporter::report(WARN,"Pixel buffer sizes don't match, %dx%d != %dx%d.\n",pixelBufferWidth,pixelBufferHeight,allBuffers[i]->getWidth(),allBuffers[i]->getHeight());
						return 0;
					}
				}
			}
		}
	}

	unsigned updatedBuffers = 0;

	// PER-VERTEX
	if (numVertexBuffers)
	{
		// REALTIME
		if (allVertexBuffers[0] && paramsAllowRealtime)
		{
			updatedBuffers += updateVertexBufferFromSolver(objectNumber,allVertexBuffers[0],_params);
		}
		else
		// NON-REALTIME
		{
			// final gather: solver.direct+indirect+lights+env -> tmparray
			// for each triangle
			// future optimization: gather only triangles necessary for selected object
			unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
			GatheredPerTriangleData* finalGatherPhysical = GatheredPerTriangleData::create(numTriangles,allVertexBuffers[LS_LIGHTMAP]?1:0,allVertexBuffers[LS_DIRECTION1]||allVertexBuffers[LS_DIRECTION2]||allVertexBuffers[LS_DIRECTION3],allVertexBuffers[LS_BENT_NORMALS]?1:0);
			if (!finalGatherPhysical)
			{
				RRReporter::report(ERRO,"Allocation failed, vertex buffer not updated.\n");
			}
			else
			{
				gatherPerTrianglePhysical(&params,finalGatherPhysical,numTriangles,getMultiObjectCustom()->faceGroups.containsEmittance()); // this is final gather -> gather emissive materials

				// interpolate: tmparray -> buffer
				for (unsigned i=0;i<NUM_BUFFERS;i++)
				{
					if (allVertexBuffers[i] && !aborting)
					{
						updatedBuffers += updateVertexBufferFromPerTriangleDataPhysical(objectNumber,allVertexBuffers[i],finalGatherPhysical->data[i],sizeof(*finalGatherPhysical->data[i]),i!=LS_BENT_NORMALS);
					}
				}
				delete finalGatherPhysical;
			}
		}
	}

	// PER-PIXEL (NON-REALTIME)
	if (numPixelBuffers)
	{
		TexelContext tc;
		tc.solver = this;
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			tc.pixelBuffers[i] = NULL;
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			if (allPixelBuffers[i])
			{
				tc.pixelBuffers[i] = RRBuffer::create(BT_2D_TEXTURE,pixelBufferWidth,pixelBufferHeight,1,BF_RGBAF,false,NULL);
				if (!tc.pixelBuffers[i])
				{
					for (unsigned i=0;i<NUM_BUFFERS;i++)
						delete tc.pixelBuffers[i];
					RRReporter::report(ERRO,"Allocation failed, lightmap not updated(0).\n");
					return updatedBuffers;
				}
				tc.pixelBuffers[i]->clear();
			}
		tc.params = &params;
		tc.singleObjectReceiver = getStaticObjects()[objectNumber]; // safe objectNumber, checked in updateLightmap()
		tc.gatherDirectEmitors = getMultiObjectCustom()->faceGroups.containsEmittance(); // this is final gather -> gather from emitors
		tc.gatherAllDirections = allPixelBuffers[LS_DIRECTION1] || allPixelBuffers[LS_DIRECTION2] || allPixelBuffers[LS_DIRECTION3];
		tc.staticSceneContainsLods = priv->staticSceneContainsLods;
		UnwrapStatistics us;
		bool gathered = enumerateTexelsFull(getMultiObjectCustom(),objectNumber,pixelBufferWidth,pixelBufferHeight,processTexel,tc,priv->minimalSafeDistance,us);

		// report unwrap errors
		if (gathered && (us.numTrianglesWithoutUnwrap || us.numTrianglesWithUnwrapOutOfRange))
		{
			if (us.numTrianglesWithoutUnwrap)
			{
				if (!us.numTrianglesWithUnwrap)
				{
					RRReporter::report(WARN,"No unwrap. Build unwrap or bake GI per-vertex.\n");
					gathered = false;
				}
				else
				{
					unsigned numTriangles = us.numTrianglesWithUnwrap+us.numTrianglesWithoutUnwrap;
					RRReporter::report(WARN,"Bad unwrap, %d%% missing. Fix it or bake GI per-vertex.\n",
						(100*us.numTrianglesWithoutUnwrap+numTriangles-1)/numTriangles
						);
				}
			}
			else
			{
				unsigned numTriangles = us.numTrianglesWithUnwrap+us.numTrianglesWithoutUnwrap;
				RRReporter::report(WARN,"Imperfect unwrap, %d%% triangles reach out of range.\n",
					(100*us.numTrianglesWithUnwrapOutOfRange+numTriangles-1)/numTriangles
					);
			}
		}

		const FilteringParameters filteringLocal;
		if (!_filtering)
			_filtering = &filteringLocal;
		unsigned numBuffersEmpty = 0;
		unsigned numBuffersFull = 0;
		for (unsigned b=0;b<NUM_BUFFERS;b++)
		{
			if (tc.pixelBuffers[b])
			{
				if (allPixelBuffers[b]
					&& params.debugTexel==UINT_MAX // skip texture update when debugging texel
					&& gathered)
				{
					tc.pixelBuffers[b]->lightmapSmooth(_filtering->smoothingAmount,_filtering->wrap,getStaticObjects()[objectNumber]); // safe objectNumber, was already checked
					if (tc.pixelBuffers[b]->lightmapGrowForBilinearInterpolation(_filtering->wrap))
						numBuffersFull++;
					else
						numBuffersEmpty++;
					tc.pixelBuffers[b]->lightmapGrow(_filtering->spreadForegroundColor,_filtering->wrap);
					tc.pixelBuffers[b]->lightmapFillBackground(_filtering->backgroundColor);
					tc.pixelBuffers[b]->copyElementsTo(allPixelBuffers[b],(b==LS_BENT_NORMALS)?NULL:priv->scaler);
					allPixelBuffers[b]->version = getSolutionVersion();
					updatedBuffers++;
				}
				delete tc.pixelBuffers[b];
			}
		}
		if (numBuffersEmpty && !aborting)
		{
			// We are in trouble, no pixels were rendered into buffer.
			// Let's try to be helpful, can we log also _why_ did it happen?

			// Uv index is defined by material.
			// Object may have multiple materials, but they should use the same uv index. Let's check it.
			unsigned uvIndex = 0;
			bool uvIndexSet = false;
			bool multipleUvIndicesUsed = false;
			const RRObject* object = getStaticObjects()[objectNumber]; // safe objectNumber, checked in updateLightmap()
			const RRMesh* mesh = object->getCollider()->getMesh();
			unsigned numTriangles = mesh->getNumTriangles();
			unsigned numVertices = mesh->getNumVertices();
			for (unsigned t=0;t<numTriangles;t++)
			{
				const RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
				if (material)
				{
					if (uvIndexSet && material->lightmapTexcoord!=uvIndex)
						multipleUvIndicesUsed = true;
					uvIndex = material->lightmapTexcoord;
					uvIndexSet = true;
				}
			}

			// Reason is unknown, just guessing.
			const char* hint = "bad unwrap or wrong uv index?";
			// We found something unrelated but important to say.
			if (multipleUvIndicesUsed) hint = "is it intentional that materials in object use different uv indices for lightmap?";
			// This is probably unrelated, but serious problem that must be fixed.
			if (uvIndexSet==false) hint = "all materials are NULL!";
			// We found the reasons.
			if (pixelBufferWidth*pixelBufferHeight==0) hint = "map size is 0!";
			if (numTriangles==0) hint = "mesh has 0 triangles!";

			RRReporter::report(WARN,
				"No texels rendered into map, %s (object=%d/%d numTriangles=%d numVertices=%d emptyBuffers=%d/%d resolution=%dx%d uvIndex=%d)\n",
				hint,
				objectNumber,getStaticObjects().size(),
				numTriangles,numVertices,
				numBuffersEmpty,numBuffersFull+numBuffersEmpty,
				pixelBufferWidth,pixelBufferHeight,
				uvIndex);
		}
	}

	return updatedBuffers;
}

// helps detect shared buffers
struct SortedBuffer
{
	RRBuffer* buffer;
	unsigned objectIndex;
	unsigned lightmapIndex;
	bool operator >(const SortedBuffer& a) const {return buffer>a.buffer;}
	bool operator <(const SortedBuffer& a) const {return buffer<a.buffer;}
};


unsigned RRDynamicSolver::updateLightmaps(int layerNumberLighting, int layerNumberDirectionalLighting, int layerNumberBentNormals, const UpdateParameters* _paramsDirect, const UpdateParameters* _paramsIndirect, const FilteringParameters* _filtering)
{
	UpdateParameters paramsDirect;
	UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	if (_paramsDirect) paramsDirect = *_paramsDirect;
	if (_paramsIndirect) paramsIndirect = *_paramsIndirect;

	// when direct=NULL, copy quality from indirect otherwise final gather would shoot only 1 ray per texel to gather indirect
	if (!_paramsDirect && _paramsIndirect) paramsDirect.quality = paramsIndirect.quality;

	// clear applyXxx that can be cleared
	{
		bool envFound = false;
		RRBuffer* env = getEnvironment();
		if (env)
		{
			unsigned numElements = env->getNumElements();
			for (unsigned i=0;i<numElements;i++)
				envFound |= env->getElement(i)!=RRVec4(0);
		}
		if (!envFound)
		{
			paramsDirect.applyEnvironment = false;
			paramsIndirect.applyEnvironment = false;
		}

		bool lightsFound = false;
		for (unsigned i=0;i<getLights().size();i++)
			lightsFound |= getLights()[i]->enabled && getLights()[i]->color!=RRVec3(0);
		if (!lightsFound)
		{
			paramsDirect.applyLights = false;
			paramsIndirect.applyLights = false;
		}

		if (paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
		{
			if (_paramsDirect) // don't report if direct is NULL, silently disable it
				RRReporter::report(WARN,"paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
			paramsDirect.applyCurrentSolution = false;
		}
	}

	int allLayers[NUM_BUFFERS];
	allLayers[LS_LIGHTMAP] = layerNumberLighting;
	allLayers[LS_DIRECTION1] = layerNumberDirectionalLighting;
	allLayers[LS_DIRECTION2] = layerNumberDirectionalLighting+((layerNumberDirectionalLighting>=0)?1:0);
	allLayers[LS_DIRECTION3] = layerNumberDirectionalLighting+((layerNumberDirectionalLighting>=0)?2:0);
	allLayers[LS_BENT_NORMALS] = layerNumberBentNormals;

	std::vector<SortedBuffer> bufferSharing;

	bool containsFirstGather = _paramsIndirect && ((paramsIndirect.applyCurrentSolution && paramsIndirect.quality) || paramsIndirect.applyLights || paramsIndirect.applyEnvironment);
	bool containsRealtime = !paramsDirect.applyLights && !paramsDirect.applyEnvironment && paramsDirect.applyCurrentSolution && !paramsDirect.quality;
	bool containsVertexBuffers = false;
	bool containsPixelBuffers = false;
	bool containsVertexBuffer[NUM_BUFFERS] = {0,0,0,0,0};
	unsigned sizeOfAllBuffers = 0;
	for (unsigned object=0;object<getStaticObjects().size();object++)
	{
		for (unsigned i=0;i<NUM_BUFFERS;i++)
		{
			RRBuffer* buffer = getStaticObjects()[object]->illumination.getLayer(allLayers[i]);
			if (buffer)
			{
				sizeOfAllBuffers += buffer->getBufferBytes();
				containsVertexBuffers |= buffer->getType()==BT_VERTEX_BUFFER;
				containsPixelBuffers  |= buffer->getType()==BT_2D_TEXTURE;
				containsVertexBuffer[i] |= buffer->getType()==BT_VERTEX_BUFFER;

				SortedBuffer sb;
				sb.buffer = buffer;
				sb.lightmapIndex = i;
				sb.objectIndex = object;
				bufferSharing.push_back(sb);
			}
		}
	}

	// detect buffers shared by multiple objects
	sort(bufferSharing.begin(),bufferSharing.end());
	for (unsigned i=1;i<bufferSharing.size();i++)
	{
		if (bufferSharing[i].buffer==bufferSharing[i-1].buffer) // sharing
		{
			if (bufferSharing[i].lightmapIndex!=bufferSharing[i-1].lightmapIndex)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Single buffer can't be used for multiple content types (eg lightmap and bent normals).\n"));
			}
			else
			/*if (bufferSharing[i].buffer->getType()!=BT_2D_TEXTURE)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Per-vertex lightmap can't be shared by multiple objects.\n"));
			}
			else*/
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Buffer can't be shared by multiple objects.\n"));
			}
		}
	}

	RRReportInterval report((containsFirstGather||containsPixelBuffers||!containsRealtime)?INF1:INF3,"Updating %s (%d,%d,%d,DIRECT(%s%s%s),INDIRECT(%s%s%s)) with %d objects, %d lights...\n",
		sizeOfAllBuffers?"lightmaps":"indirect illumination",
		layerNumberLighting,layerNumberDirectionalLighting,layerNumberBentNormals,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		paramsIndirect.applyCurrentSolution?"cur ":"",
		getStaticObjects().size(),getLights().size());
	
	if (sizeOfAllBuffers>10000000 && (containsFirstGather||containsPixelBuffers||!containsRealtime))
		RRReporter::report(INF1,"Memory taken by lightmaps: %dMB\n",sizeOfAllBuffers/1024/1024);

	// 1. first gather: solver+lights+env -> solver.direct
	// 2. propagate: solver.direct -> solver.indirect
	if (containsFirstGather)
	{
		// 1. first gather: solver.direct+indirect+lights+env -> solver.direct
		// 2. propagate: solver.direct -> solver.indirect
		if (!updateSolverIndirectIllumination(&paramsIndirect))
			return 0;

		paramsDirect.applyCurrentSolution = true; // set solution generated here to be gathered in final gather
		paramsDirect.measure_internal.direct = true; // it is stored in direct+indirect (after calculate)
		paramsDirect.measure_internal.indirect = true;
		// musim gathernout direct i indirect.
		// indirect je jasny, jedine v nem je vysledek spocteny v calculate().
		// ovsem direct musim tez, jedine z nej dostanu prime osvetleni emisivnim facem, prvni odraz od spotlight, lights a env
	}

	unsigned updatedBuffers = 0;

	if (!paramsDirect.applyLights && !paramsDirect.applyEnvironment && !paramsDirect.applyCurrentSolution)
	{
		RRReporter::report(WARN,"No light sources enabled.\n");
	}

	// 3. vertex: realtime copy into buffers (solver not modified)
	if (containsVertexBuffers && containsRealtime)
	{
		for (int objectHandle=0;objectHandle<(int)getStaticObjects().size();objectHandle++) if (!aborting)
		{
			for (unsigned i=0;i<NUM_BUFFERS;i++)
			{
				if (allLayers[i]>=0)
				{
					RRBuffer* vertexBuffer = onlyVbuf( getStaticObjects()[objectHandle]->illumination.getLayer(allLayers[i]) );
					if (vertexBuffer)
					{
						if (i==LS_LIGHTMAP)
						{
							updatedBuffers += updateVertexBufferFromSolver(objectHandle,vertexBuffer,&paramsDirect);
						}
						else
						{
							RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Directional buffers not updated in 'realtime' mode (quality=0, applyCurrentSolution only).\n"));
						}
					}
				}
			}
		}
	}

	// 4+5. vertex: final gather into vertex buffers (solver not modified)
	if (containsVertexBuffers && !containsRealtime)
	if (!(paramsDirect.debugTexel!=UINT_MAX && paramsDirect.debugTriangle==UINT_MAX)) // skip triangle-gathering when debugging texel
	{
			// 4. final gather: solver.direct+indirect+lights+env -> tmparray
			// for each triangle
			unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
			const GatheredPerTriangleData* finalGatherPhysical = GatheredPerTriangleData::create(numTriangles,containsVertexBuffer[LS_LIGHTMAP],containsVertexBuffer[LS_DIRECTION1]||containsVertexBuffer[LS_DIRECTION2]||containsVertexBuffer[LS_DIRECTION3],containsVertexBuffer[LS_BENT_NORMALS]);
			if (!finalGatherPhysical)
			{
				RRReporter::report(ERRO,"Not enough memory, vertex buffers not updated.\n");
			}
			else
			{
				gatherPerTrianglePhysical(&paramsDirect,finalGatherPhysical,numTriangles,getMultiObjectCustom()->faceGroups.containsEmittance()); // this is final gather -> gather emissive materials

				// 5. interpolate: tmparray -> buffer
				// for each object with vertex buffer
				if (paramsDirect.debugObject==UINT_MAX) // skip update when debugging
				{
					for (unsigned objectHandle=0;objectHandle<getStaticObjects().size();objectHandle++)
					{
						for (unsigned i=0;i<NUM_BUFFERS;i++)
						{
							if (allLayers[i]>=0 && !aborting)
							{
								RRBuffer* vertexBuffer = onlyVbuf( getStaticObjects()[objectHandle]->illumination.getLayer(allLayers[i]) );
								if (vertexBuffer)
									updatedBuffers += updateVertexBufferFromPerTriangleDataPhysical(objectHandle,vertexBuffer,finalGatherPhysical->data[i],sizeof(*finalGatherPhysical->data[i]),i!=LS_BENT_NORMALS);
							}
						}
					}
				}
				delete finalGatherPhysical;
			}
	}

	// 6. pixel: final gather into pixel buffers (solver not modified)
	if (containsPixelBuffers)
	if (!(paramsDirect.debugTexel==UINT_MAX && paramsDirect.debugTriangle!=UINT_MAX)) // skip pixel-gathering when debugging triangle
	{
		for (unsigned object=0;object<getStaticObjects().size();object++)
		{
			if ((paramsDirect.debugObject==UINT_MAX || paramsDirect.debugObject==object) && !aborting) // skip objects when debugging texel
			{
				RRBuffer* allPixelBuffers[NUM_BUFFERS];
				unsigned numPixelBuffers = 0;
				for (unsigned i=0;i<NUM_BUFFERS;i++)
				{
					allPixelBuffers[i] = (allLayers[i]>=0) ? onlyLmap(getStaticObjects()[object]->illumination.getLayer(allLayers[i])) : NULL;
					if (allPixelBuffers[i]) numPixelBuffers++;
				}

				if (numPixelBuffers)
				{
					updatedBuffers += updateLightmap(object,allPixelBuffers[LS_LIGHTMAP],allPixelBuffers+LS_DIRECTION1,allPixelBuffers[LS_BENT_NORMALS],&paramsDirect,_filtering);
				}
			}
		}
	}

	return updatedBuffers;
}

} // namespace

