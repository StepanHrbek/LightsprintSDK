// --------------------------------------------------------------------------
// Offline build of lightmaps and bent normal maps.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cmath> // necessary for mingw gcc4.3
#include <cassert>
#include <cfloat>
#include <vector>
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "LightmapFilter.h"
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

bool enumerateTexelsPartial(const RRObject* multiObject, unsigned objectNumber,
		unsigned mapWidth, unsigned mapHeight,
		unsigned rectXMin, unsigned rectYMin, unsigned rectXMaxPlus1, unsigned rectYMaxPlus1, 
		ProcessTexelResult (callback)(const struct ProcessTexelParams& pti), const TexelContext& tc, RRReal minimalSafeDistance, int onlyTriangleNumber=-1)
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
	RRObject* singleObject = tc.solver->getObject(objectNumber);
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
			singleMesh->getTriangleMapping(singlePostImportTriangle,mapping);
			// remember any triangle in selected object, for relevantLights
			multiPostImportTriangleNumber = t;

			// rasterize triangle t
			//  find minimal bounding box
			unsigned xminu, xmaxu, yminu, ymaxu;
			{
				RRReal xmin = mapWidth  * MIN(mapping.uv[0][0],MIN(mapping.uv[1][0],mapping.uv[2][0]));
				RRReal xmax = mapWidth  * MAX(mapping.uv[0][0],MAX(mapping.uv[1][0],mapping.uv[2][0]));
				xminu = (unsigned)MAX(xmin,rectXMin); // !negative
				xmaxu = (unsigned)CLAMPED(xmax+1,0,rectXMaxPlus1); // !negative
				RRReal ymin = mapHeight * MIN(mapping.uv[0][1],MIN(mapping.uv[1][1],mapping.uv[2][1]));
				RRReal ymax = mapHeight * MAX(mapping.uv[0][1],MAX(mapping.uv[1][1],mapping.uv[2][1]));
				yminu = (unsigned)MAX(ymin,rectYMin); // !negative
				ymaxu = (unsigned)CLAMPED(ymax+1,0,rectYMaxPlus1); // !negative
				if (yminu>=ymaxu || xminu>=xmaxu) continue; // early exit from triangle outside our rectangle
				if (!(xmin>=0 && xmax<=mapWidth) || !(ymin>=0 && ymax<=mapHeight))
					LIMITED_TIMES(1,RRReporter::report(WARN,"Unwrap coordinates out of 0..1 range.\n"));
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
					if ((tc.params->debugTexel==UINT_MAX || tc.params->debugTexel==x+y*mapWidth) && !tc.solver->aborting) // process only texel selected for debugging
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
							SubTexel subTexel;
							subTexel.multiObjPostImportTriIndex = t;
							subTexel.uvInTriangleSpace[0] = polyVertexInTriangleSpace[0];
							for (unsigned i=0;i<polySize-2;i++)
							{
								subTexel.uvInTriangleSpace[1] = polyVertexInTriangleSpace[i+1];
								subTexel.uvInTriangleSpace[2] = polyVertexInTriangleSpace[i+2];
								RRReal subTexelAreaInTriangleSpace = getArea(subTexel.uvInTriangleSpace[0],subTexel.uvInTriangleSpace[1],subTexel.uvInTriangleSpace[2]);
								subTexel.areaInMapSpace = subTexelAreaInTriangleSpace * triangleAreaInMapSpace;
								if (_finite(subTexel.areaInMapSpace)) // skip subtexels of NaN area (they exist because of limited float precision)
									texelsRect[(x-rectXMin)+(y-rectYMin)*(rectXMaxPlus1-rectXMin)].push_back(subTexel
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

	// 3. preallocate rays
#ifdef _OPENMP
	int numThreads = omp_get_max_threads();
#else
	int numThreads = 1;
#endif
	RRRay* rays = RRRay::create(2*numThreads);

	// 4. preallocate and populate relevantLights
	unsigned numAllLights = tc.solver->getLights().size();
	unsigned numRelevantLights = 0;
	const RRLight** relevantLightsForObject = new const RRLight*[numAllLights*numThreads];
	for (unsigned i=0;i<numAllLights;i++)
	{
		RRLight* light = tc.solver->getLights()[i];
		if (multiObject->getTriangleMaterial(multiPostImportTriangleNumber,light,NULL))
		{
			for (int k=0;k<numThreads;k++)
				relevantLightsForObject[k*numAllLights+numRelevantLights] = light;
			numRelevantLights++;
		}
	}

	// 5. gather, shoot rays from texels
	#pragma omp parallel for schedule(dynamic)
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
				if ((tc.params->debugTexel==UINT_MAX || tc.params->debugTexel==i+j*mapWidth) && !tc.solver->aborting) // process only texel selected for debugging
				{
					ProcessTexelParams ptp(tc);
					ptp.uv[0] = i;
					ptp.uv[1] = j;
					ptp.subTexels = texelsRect+indexInRect;
					ptp.rays = rays+2*threadNum;
					ptp.rays[0].rayLengthMin = minimalSafeDistance;
					ptp.rays[1].rayLengthMin = minimalSafeDistance;
					ptp.relevantLights = relevantLightsForObject+numAllLights*threadNum;
					ptp.numRelevantLights = numRelevantLights;
					ptp.relevantLightsFilled = true;
					callback(ptp);
				}
			}
		}
	}

	// 6. cleanup
	delete[] relevantLightsForObject;
	delete[] texelsRect;
	delete[] rays;

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
bool enumerateTexelsFull(const RRObject* multiObject, unsigned objectNumber, unsigned mapWidth, unsigned mapHeight, ProcessTexelResult (callback)(const struct ProcessTexelParams& pti), const TexelContext& tc, RRReal minimalSafeDistance, int onlyTriangleNumber=-1)
{
	enum {MAX_TEXELS_PER_PASS=512*512};
	unsigned numTexelsInMap = mapWidth*mapHeight;
	unsigned numPasses = (numTexelsInMap+MAX_TEXELS_PER_PASS-1)/MAX_TEXELS_PER_PASS;
	if (numPasses==0)
		return false;
	else
	if (mapWidth<=mapHeight)
	{
		for (unsigned i=0;i<numPasses;i++)
		{
			if (!enumerateTexelsPartial(multiObject, objectNumber, mapWidth, mapHeight, 0,mapHeight*i/numPasses,mapWidth,mapHeight*(i+1)/numPasses, callback, tc, minimalSafeDistance, onlyTriangleNumber))
				return false;
		}
		return true;
	}
	else
	{
		for (unsigned i=0;i<numPasses;i++)
		{
			if (!enumerateTexelsPartial(multiObject, objectNumber, mapWidth, mapHeight, mapHeight*i/numPasses,0,mapWidth*(i+1)/numPasses,mapHeight, callback, tc, minimalSafeDistance, onlyTriangleNumber))
				return false;
		}
		return true;
	}
}

void scaleAndFlushToBuffer(RRBuffer* destBuffer, RRVec4* srcData, const RRScaler* scaler)
{
	if (!srcData || !destBuffer)
	{
		RR_ASSERT(0); // invalid inputs
		return;
	}
	if (!destBuffer->getScaled()) scaler = NULL;
	unsigned numElements = destBuffer->getWidth()*destBuffer->getHeight();
	for (unsigned i=0;i<numElements;i++)
	{
		if (scaler) scaler->getCustomScale(srcData[i]);
		destBuffer->setElement(i,srcData[i]);
	}
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : NULL;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : NULL;
}

unsigned RRDynamicSolver::updateLightmap(int objectNumber, RRBuffer* buffer, RRBuffer* directionalLightmaps[3], RRBuffer* bentNormals, const UpdateParameters* _params, const FilteringParameters* filtering)
{
	bool realtime = buffer && buffer->getType()==BT_VERTEX_BUFFER && !bentNormals && (!_params || (!_params->applyLights && !_params->applyEnvironment && !_params->quality));
	RRReportInterval report(realtime?INF3:INF2,"Updating object %d/%d, %s %d*%d, directional %d*%d, bent normals %d*%d...\n",
		objectNumber,getNumObjects(),
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
			RRReporter::report(WARN,"RRDynamicSolver::updateLightmap: Empty scene.\n");
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
			// for multiobject, we use non-indexed render with numTriangles*3 vertices in vbuf
			vertexBufferWidth = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()*3;
		}
		else
		{
			if (objectNumber>=(int)getNumObjects() || objectNumber<0)
			{
				RRReporter::report(WARN,"Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getNumObjects()-1);
				return 0;
			}
			if (!getIllumination(objectNumber))
			{
				RRReporter::report(WARN,"getIllumination(%d) is NULL.\n",objectNumber);
				return 0;
			}
			vertexBufferWidth = getIllumination(objectNumber)->getNumPreImportVertices();
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
				if (allBuffers[i]->getWidth()<vertexBufferWidth) // only smaller buffer is problem, bigger buffer is sometimes created by ObjectBuffers
				{
					RRReporter::report(WARN,"Insufficient vertex buffer size %d, should be %d.\n",allBuffers[i]->getWidth(),vertexBufferWidth);
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
				RRReporter::report(ERRO,"Not enough memory, vertex buffer not updated.\n");
			}
			else
			{
				gatherPerTrianglePhysical(&params,finalGatherPhysical,numTriangles,priv->staticSceneContainsEmissiveMaterials); // this is final gather -> gather emissive materials

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
		try
		{
			for (unsigned i=0;i<NUM_BUFFERS;i++)
				tc.pixelBuffers[i] = NULL;
			for (unsigned i=0;i<NUM_BUFFERS;i++)
				tc.pixelBuffers[i] = allPixelBuffers[i]?new LightmapFilter(pixelBufferWidth,pixelBufferHeight):NULL;
		}
		catch(std::bad_alloc)
		{
			for (unsigned i=0;i<NUM_BUFFERS;i++)
				delete tc.pixelBuffers[i];
			RRReporter::report(ERRO,"Not enough memory, lightmap not updated(0).\n");
			return updatedBuffers;
		}
		tc.params = &params;
		tc.singleObjectReceiver = getObject(objectNumber);
		tc.gatherDirectEmitors = priv->staticSceneContainsEmissiveMaterials; // this is final gather -> gather from emitors
		tc.gatherAllDirections = allPixelBuffers[LS_DIRECTION1] || allPixelBuffers[LS_DIRECTION2] || allPixelBuffers[LS_DIRECTION3];
		tc.staticSceneContainsLods = priv->staticSceneContainsLods;
		bool gathered = enumerateTexelsFull(getMultiObjectCustom(),objectNumber,pixelBufferWidth,pixelBufferHeight,processTexel,tc,priv->minimalSafeDistance);

		for (unsigned i=0;i<NUM_BUFFERS;i++)
		{
			if (tc.pixelBuffers[i])
			{
				if (allPixelBuffers[i]
					&& params.debugTexel==UINT_MAX // skip texture update when debugging texel
					&& gathered)
				{
					scaleAndFlushToBuffer(allPixelBuffers[i],tc.pixelBuffers[i]->getFilteredPhysical(filtering),(i==LS_BENT_NORMALS || (i==LS_LIGHTMAP && params.lowDetailForLightDetailMap))?NULL:priv->scaler);
					updatedBuffers++;
				}
				delete tc.pixelBuffers[i];
			}
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

	if (paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		if (_paramsDirect) // don't report if direct is NULL, silently disable it
			RRReporter::report(WARN,"paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
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
	for (unsigned object=0;object<getNumObjects();object++)
	{
		if (getIllumination(object))
		{
			for (unsigned i=0;i<NUM_BUFFERS;i++)
			{
				RRBuffer* buffer = getIllumination(object)->getLayer(allLayers[i]);
				if (buffer)
				{
					sizeOfAllBuffers += buffer->getMemoryOccupied();
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
	}

	// detect buffers shared by multiple objects
	sort(bufferSharing.begin(),bufferSharing.end());
	for (unsigned i=1;i<bufferSharing.size();i++)
	{
		if (bufferSharing[i].buffer==bufferSharing[i-1].buffer) // sharing
		{
			if (bufferSharing[i].lightmapIndex!=bufferSharing[i-1].lightmapIndex)
			{
				LIMITED_TIMES(1,RRReporter::report(WARN,"Single buffer can't be used for multiple content types (eg lightmap and bent normals).\n"));
			}
			else
			/*if (bufferSharing[i].buffer->getType()!=BT_2D_TEXTURE)
			{
				LIMITED_TIMES(1,RRReporter::report(WARN,"Per-vertex lightmap can't be shared by multiple objects.\n"));
			}
			else*/
			{
				LIMITED_TIMES(1,RRReporter::report(WARN,"Buffer can't be shared by multiple objects.\n"));
			}
		}
	}

	RRReportInterval report((containsFirstGather||containsPixelBuffers||!containsRealtime)?INF1:INF3,"Updating lightmaps (%d,%d,%d,DIRECT(%s%s%s),INDIRECT(%s%s%s)).\n",
		layerNumberLighting,layerNumberDirectionalLighting,layerNumberBentNormals,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");
	
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
		for (int objectHandle=0;objectHandle<(int)priv->objects.size();objectHandle++) if (!aborting)
		{
			for (unsigned i=0;i<NUM_BUFFERS;i++)
			{
				if (allLayers[i]>=0)
				{
					RRBuffer* vertexBuffer = onlyVbuf( getIllumination(objectHandle) ? getIllumination(objectHandle)->getLayer(allLayers[i]) : NULL );
					if (vertexBuffer)
					{
						if (i==LS_LIGHTMAP)
						{
							updatedBuffers += updateVertexBufferFromSolver(objectHandle,vertexBuffer,&paramsDirect);
						}
						else
						{
							LIMITED_TIMES(1,RRReporter::report(WARN,"Directional buffers not updated in 'realtime' mode (quality=0, applyCurrentSolution only).\n"));
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
				gatherPerTrianglePhysical(&paramsDirect,finalGatherPhysical,numTriangles,priv->staticSceneContainsEmissiveMaterials); // this is final gather -> gather emissive materials

				// 5. interpolate: tmparray -> buffer
				// for each object with vertex buffer
				if (paramsDirect.debugObject==UINT_MAX) // skip update when debugging
				{
					for (unsigned objectHandle=0;objectHandle<priv->objects.size();objectHandle++)
					{
						for (unsigned i=0;i<NUM_BUFFERS;i++)
						{
							if (allLayers[i]>=0 && !aborting)
							{
								RRBuffer* vertexBuffer = onlyVbuf( getIllumination(objectHandle) ? getIllumination(objectHandle)->getLayer(allLayers[i]) : NULL );
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
		for (unsigned object=0;object<getNumObjects();object++)
		{
			if ((paramsDirect.debugObject==UINT_MAX || paramsDirect.debugObject==object) && !aborting) // skip objects when debugging texel
			{
				RRBuffer* allPixelBuffers[NUM_BUFFERS];
				unsigned numPixelBuffers = 0;
				for (unsigned i=0;i<NUM_BUFFERS;i++)
				{
					allPixelBuffers[i] = (allLayers[i]>=0 && getIllumination(object)) ? onlyLmap(getIllumination(object)->getLayer(allLayers[i])) : NULL;
					if (allPixelBuffers[i]) numPixelBuffers++;
				}

				if (numPixelBuffers)
				{
					if (allPixelBuffers[LS_LIGHTMAP] && paramsDirect.locality>99999 && paramsIndirect.locality<99 && !paramsDirect.applyLights)
					{
						// light detail map
						RRReportInterval report(INF2,"Creating light detail map...\n");
						RRBuffer* lowDetail = RRBuffer::create(BT_VERTEX_BUFFER,getIllumination(object)->getNumPreImportVertices(),1,1,BF_RGBF,true,NULL);
						updateLightmap(object,lowDetail,NULL,NULL,&paramsDirect,_filtering);
						paramsDirect.lowDetailForLightDetailMap = lowDetail;
						updatedBuffers += updateLightmap(object,allPixelBuffers[LS_LIGHTMAP],allPixelBuffers+LS_DIRECTION1,allPixelBuffers[LS_BENT_NORMALS],&paramsDirect,_filtering);
						paramsDirect.lowDetailForLightDetailMap = NULL;
						delete lowDetail;
					}
					else
					{
						// other maps
						updatedBuffers += updateLightmap(object,allPixelBuffers[LS_LIGHTMAP],allPixelBuffers+LS_DIRECTION1,allPixelBuffers[LS_BENT_NORMALS],&paramsDirect,_filtering);
					}
				}
			}
		}
	}

	return updatedBuffers;
}

} // namespace

