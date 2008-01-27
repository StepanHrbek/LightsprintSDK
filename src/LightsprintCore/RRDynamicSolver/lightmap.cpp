// --------------------------------------------------------------------------
// Offline build of lightmaps and bent normal maps.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cmath> // necessary for mingw gcc4.3
#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "LightmapFilter.h"
#include "../RRMathPrivate.h"
#include "private.h"
#include "gather.h"

namespace rr
{

RRReal getArea(RRVec2 v0,RRVec2 v1,RRVec2 v2)
{
	RRReal a = (v1-v0).length();
	RRReal b = (v2-v1).length();
	RRReal c = (v0-v2).length();
	RRReal s = (a+b+c)*0.5f;
	return  sqrtf(s*(s-a)*(s-b)*(s-c));
}

//! Enumerates all texels on object's surface.
//
//! Default implementation runs callbacks on all CPUs/cores at once.
//! It is not strictly defined what texels are enumerated, but close match to
//! pixels visible on mapped object improves lightmap quality.
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
void enumerateTexels(const RRObject* multiObject, unsigned objectNumber, unsigned mapWidth, unsigned mapHeight, ProcessTexelResult (callback)(const struct ProcessTexelParams& pti), const TexelContext& tc, RRReal minimalSafeDistance, int onlyTriangleNumber=-1)
{
	// Iterate through all multimesh triangles (rather than single object's mesh triangles)
	// Advantages:
	//  + vertices and normals in world space
	//  + no degenerated faces
	//!!! Warning: mapping must be preserved by multiobject.
	//    Current multiobject lets object mappings overlap, but it could change in future.

	if(!multiObject)
	{
		RR_ASSERT(0);
		return;
	}
	RRMesh* multiMesh = multiObject->getCollider()->getMesh();
	unsigned numTriangles = multiMesh->getNumTriangles();

	int firstTriangle = 0;
	if(onlyTriangleNumber>=0)
	{
		firstTriangle = onlyTriangleNumber;
		numTriangles = 1;
	}

	// 1. allocate space for texels and rays
	int numTexelsInMap = mapWidth*mapHeight;
	TexelSubTexels* texels = new TexelSubTexels[numTexelsInMap];
#ifdef _OPENMP
	RRRay* rays = RRRay::create(2*omp_get_max_threads());
#else
	RRRay* rays = RRRay::create(2);
#endif

	// 2. populate texels with subtexels
	for(int tint=firstTriangle;tint<(int)(firstTriangle+numTriangles);tint++)
	{
		unsigned t = (unsigned)tint;
		RRMesh::MultiMeshPreImportNumber preImportNumber = multiMesh->getPreImportTriangle(t);
		if(preImportNumber.object==objectNumber)
		{
			// gather data about triangle t
			//  prepare mapspace -> trianglespace matrix
			RRMesh::TriangleMapping mapping;
			multiMesh->getTriangleMapping(t,mapping);
			RRReal m[3][3] = {
				{ mapping.uv[1][0]-mapping.uv[0][0], mapping.uv[2][0]-mapping.uv[0][0], mapping.uv[0][0] },
				{ mapping.uv[1][1]-mapping.uv[0][1], mapping.uv[2][1]-mapping.uv[0][1], mapping.uv[0][1] },
				{ 0,0,1 } };
			RRReal det = m[0][0]*m[1][1]*m[2][2]+m[0][1]*m[1][2]*m[2][0]+m[0][2]*m[1][0]*m[2][1]-m[0][0]*m[1][2]*m[2][1]-m[0][1]*m[1][0]*m[2][2]-m[0][2]*m[1][1]*m[2][0];
			if(!det) continue; // skip degenerated triangles
			RRReal invdet = 1/det;
			RRReal inv[2][3] = {
				{ (m[1][1]*m[2][2]-m[1][2]*m[2][1])*invdet/mapWidth, (m[0][2]*m[2][1]-m[0][1]*m[2][2])*invdet/mapHeight, (m[0][1]*m[1][2]-m[0][2]*m[1][1])*invdet },
				{ (m[1][2]*m[2][0]-m[1][0]*m[2][2])*invdet/mapWidth, (m[0][0]*m[2][2]-m[0][2]*m[2][0])*invdet/mapHeight, (m[0][2]*m[1][0]-m[0][0]*m[1][2])*invdet },
				//{ (m[1][0]*m[2][1]-m[1][1]*m[2][0])*invdet, (m[0][1]*m[2][0]-m[0][0]*m[2][1])*invdet, (m[0][0]*m[1][1]-m[0][1]*m[1][0])*invdet }
				};
			RRReal triangleAreaInMapSpace = getArea(mapping.uv[0],mapping.uv[1],mapping.uv[2]);

			// rasterize triangle t
			//  find minimal bounding box
			RRReal xmin = mapWidth  * MIN(mapping.uv[0][0],MIN(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal xmax = mapWidth  * MAX(mapping.uv[0][0],MAX(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal ymin = mapHeight * MIN(mapping.uv[0][1],MIN(mapping.uv[1][1],mapping.uv[2][1]));
			RRReal ymax = mapHeight * MAX(mapping.uv[0][1],MAX(mapping.uv[1][1],mapping.uv[2][1]));
			if(!(xmin>=0 && xmax<=mapWidth) || !(ymin>=0 && ymax<=mapHeight))
				LIMITED_TIMES(1,RRReporter::report(WARN,"Unwrap coordinates out of 0..1 range.\n"));
			//  for all texels in bounding box
			for(int y=MAX((int)ymin,0);y<(int)MIN((unsigned)ymax+1,mapHeight);y++)
			{
				for(int x=MAX((int)xmin,0);x<(int)MIN((unsigned)xmax+1,mapWidth);x++)
				{
					// start with full texel, 4 vertices
					unsigned polySize = 4;
					RRVec2 polyVertexInTriangleSpace[7] =
					{
						#define MAPSPACE_TO_TRIANGLESPACE(x,y) RRVec2( x*inv[0][0] + y*inv[0][1] + inv[0][2], x*inv[1][0] + y*inv[1][1] + inv[1][2] )
						MAPSPACE_TO_TRIANGLESPACE(x,y),
						MAPSPACE_TO_TRIANGLESPACE(x,y+1),
						MAPSPACE_TO_TRIANGLESPACE(x+1,y),
						MAPSPACE_TO_TRIANGLESPACE(x+1,y+1)
						#undef MAPSPACE_TO_TRIANGLESPACE
					};
					RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[0]));
					RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[1]));
					RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[2]));
					RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[3]));
					// calculate texel-triangle intersection (=polygon) in 2d 0..1 map space
					// cut it three times by triangle side
					for(unsigned triSide=0;triSide<3;triSide++)
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
						for(unsigned i=0;i<polySize;i++)
						{
							RRReal dist = POINT_LINE_DISTANCE_2D(polyVertexInTriangleSpace[i],triLineInTriangleSpace);
							if(dist<0)
							{
								inside[i] = OUTSIDE;
								numOutside++;
							}
							else
							if(dist>0)
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
						if(!numOutside) continue;
						// none INSIDE -> empty poly, go to next texel
						if(!numInside) {polySize = 0; break;}
						// part INSIDE, part OUTSIDE -> cut off all OUTSIDE and EDGE, add 2 new vertices
						unsigned firstPreserved = 1;
						while(inside[(firstPreserved-1)%polySize]==INSIDE || inside[firstPreserved%polySize]!=INSIDE) firstPreserved++;
						RRVec2 polyVertexInTriangleSpaceOrig[7];
						memcpy(polyVertexInTriangleSpaceOrig,polyVertexInTriangleSpace,sizeof(polyVertexInTriangleSpace));
						unsigned src = firstPreserved;
						unsigned dst = 0;
						while(inside[src%polySize]==INSIDE) polyVertexInTriangleSpace[dst++] = polyVertexInTriangleSpaceOrig[src++%polySize]; // copy preserved vertices
						#define INTERSECTION_POINTA_POINTB_LINE(pointA,pointB,line) \
							((pointA) - ((pointB)-(pointA)) * POINT_LINE_DISTANCE_2D(pointA,line) / ( (line)[0]*((pointB)[0]-(pointA)[0]) + (line)[1]*((pointB)[1]-(pointA)[1]) ) )
						polyVertexInTriangleSpace[dst++] =
							INTERSECTION_POINTA_POINTB_LINE(polyVertexInTriangleSpaceOrig[(src-1)%polySize],polyVertexInTriangleSpaceOrig[src%polySize],triLineInTriangleSpace); // append new vertex
						polyVertexInTriangleSpace[dst++] =
							INTERSECTION_POINTA_POINTB_LINE(polyVertexInTriangleSpaceOrig[(firstPreserved-1)%polySize],polyVertexInTriangleSpaceOrig[firstPreserved%polySize],triLineInTriangleSpace); // append new vertex
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[dst-2]));
						RR_ASSERT(IS_VEC2(polyVertexInTriangleSpace[dst-1]));
						polySize = dst;
					}
					// triangulate polygon into subtexels
					if(polySize)
					{
						SubTexel subTexel;
						subTexel.multiObjPostImportTriIndex = t;
						subTexel.uvInTriangleSpace[0] = polyVertexInTriangleSpace[0];
						subTexel.uvInTriangleSpace[1] = polyVertexInTriangleSpace[1];
						for(unsigned i=0;i<polySize-2;i++)
						{
							subTexel.uvInTriangleSpace[2] = polyVertexInTriangleSpace[i+2];
							RRReal subTexelAreaInTriangleSpace = getArea(subTexel.uvInTriangleSpace[0],subTexel.uvInTriangleSpace[1],subTexel.uvInTriangleSpace[2]);
							subTexel.areaInMapSpace = subTexelAreaInTriangleSpace * triangleAreaInMapSpace;
							texels[x+y*mapWidth].push_back(subTexel);
						}
					}
				}
			}
		}
	}

	// 3. gather, shoot rays from texels
	#pragma omp parallel for schedule(dynamic)
	for(int j=0;j<(int)mapHeight;j++)
	{
		for(int i=0;i<(int)mapWidth;i++)
		{
			if(texels[i+j*mapWidth].size())
			{
				ProcessTexelParams ptp(tc);
				ptp.uv[0] = i;
				ptp.uv[1] = j;
				ptp.subTexels = texels+i+j*mapWidth;
#ifdef _OPENMP
				ptp.rays = rays+2*omp_get_thread_num();
#else
				ptp.rays = rays;
#endif
				ptp.rays[0].rayLengthMin = minimalSafeDistance;
				ptp.rays[1].rayLengthMin = minimalSafeDistance;
				callback(ptp);
			}
		}
	}

	// 4. cleanup
	delete[] texels;
	delete[] rays;
}


void flush(RRBuffer* destBuffer, RRVec4* srcData, const RRScaler* scaler)
{
	if(!srcData || !destBuffer)
	{
		RR_ASSERT(0); // invalid inputs
		return;
	}
	if(!destBuffer->getScaled()) scaler = NULL;
	unsigned numElements = destBuffer->getWidth()*destBuffer->getHeight();
	for(unsigned i=0;i<numElements;i++)
	{
		if(scaler) scaler->getCustomScale(srcData[i]);
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

unsigned RRDynamicSolver::updateLightmap(int objectNumber, RRBuffer* buffer, RRBuffer* bentNormals, const UpdateParameters* _params, const FilteringParameters* filtering)
{
	bool realtime = buffer && buffer->getType()==BT_VERTEX_BUFFER && !bentNormals && (!_params || (!_params->applyLights && !_params->applyEnvironment && !_params->quality));
	RRReportInterval report(realtime?INF3:INF1,"Updating object %d/%d, %s %d*%d, bentNormal %s %d*%d...\n",
		objectNumber,getNumObjects(),
		(buffer && buffer->getType()==BT_VERTEX_BUFFER)?"vertex buffer":"lightmap",
		buffer?buffer->getWidth():0,buffer?buffer->getHeight():0,
		(bentNormals && bentNormals->getType()==BT_VERTEX_BUFFER)?"vertex buffer":"map",
		bentNormals?bentNormals->getWidth():0,bentNormals?bentNormals->getHeight():0);

	// validate params
	UpdateParameters params;
	if(_params) params = *_params;
	if(!buffer && !bentNormals)
	{
		RRReporter::report(WARN,"Both output buffers are NULL, no work.\n");
		return 0;
	}
	if(buffer && bentNormals && buffer->getType()==bentNormals->getType())
	{
		if(buffer->getWidth() != bentNormals->getWidth()
			|| buffer->getHeight() != bentNormals->getHeight())
		{
			RRReporter::report(ERRO,"Buffer sizes don't match, buffer=%dx%d, bentNormals=%dx%d.\n",buffer->getWidth(),buffer->getHeight(),bentNormals->getWidth(),bentNormals->getHeight());
			RR_ASSERT(0);
			return 0;
		}
	}
	if(!priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0);
		if( (!priv->scene
			&& !priv->packedSolver
			) || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RRReporter::report(WARN,"RRDynamicSolver::updateLightmap: Empty scene.\n");
			return 0;
		}
	}
	
	// optimize params
	if(params.applyLights && !getLights().size())
		params.applyLights = false;
	if(params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;

	unsigned updatedBuffers = 0;
	RRBuffer* vertexBuffer = onlyVbuf(buffer);
	RRBuffer* pixelBuffer = onlyLmap(buffer);
	RRBuffer* bentNormalsPerVertex = onlyVbuf(bentNormals);
	RRBuffer* bentNormalsPerPixel = onlyLmap(bentNormals);

	// PER-VERTEX
	if(vertexBuffer || bentNormalsPerVertex)
	{
		// REALTIME
		if(vertexBuffer && !params.applyLights && !params.applyEnvironment && params.applyCurrentSolution && !params.quality)
		{
			updatedBuffers += updateVertexBufferFromSolver(objectNumber,vertexBuffer,_params);
		}
		else
		// NON-REALTIME
		{
			// final gather: solver.direct+indirect+lights+env -> tmparray
			// for each triangle
			// future optimization: gather only triangles necessary for selected object
			unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
			ProcessTexelResult* finalGather = new ProcessTexelResult[numTriangles];
			gatherPerTriangle(&params,finalGather,numTriangles,priv->staticObjectsContainEmissiveMaterials); // this is final gather -> gather emissive materials

			// interpolate: tmparray -> buffer
			if(vertexBuffer)
			{
				updatedBuffers += updateVertexBufferFromPerTriangleData(objectNumber,vertexBuffer,&finalGather[0].irradiance,sizeof(finalGather[0]));
			}
			if(bentNormalsPerVertex)
			{
				updatedBuffers += updateVertexBufferFromPerTriangleData(objectNumber,bentNormalsPerVertex,&finalGather[0].bentNormal,sizeof(finalGather[0]));
			}
			delete[] finalGather;
		}
	}

	// PER-PIXEL (NON-REALTIME)
	if(pixelBuffer || bentNormalsPerPixel)
	{
		// do per-pixel part
		const RRObject* object = getMultiObjectCustom();
		RRMesh* mesh = object->getCollider()->getMesh();
		unsigned numPostImportTriangles = mesh->getNumTriangles();

		if(objectNumber>=(int)getNumObjects() || objectNumber<0)
		{
			RRReporter::report(WARN,"Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getNumObjects()-1);
			RR_ASSERT(0);
			return 0;
		}

		TexelContext tc;
		tc.solver = this;
		tc.pixelBuffer = pixelBuffer?new LightmapFilter(pixelBuffer->getWidth(),pixelBuffer->getHeight()):NULL;
		tc.params = &params;
		tc.bentNormalsPerPixel = bentNormalsPerPixel?new LightmapFilter(bentNormalsPerPixel->getWidth(),bentNormalsPerPixel->getHeight()):NULL;
		tc.singleObjectReceiver = getObject(objectNumber);
		tc.gatherDirectEmitors = priv->staticObjectsContainEmissiveMaterials; // this is final gather -> gather from emitors
		RRBuffer* tmp = pixelBuffer?pixelBuffer:bentNormals;
		enumerateTexels(getMultiObjectCustom(),objectNumber,tmp->getWidth(),tmp->getHeight(),processTexel,tc,priv->minimalSafeDistance);

		if(tc.pixelBuffer)
		{
			flush(pixelBuffer,tc.pixelBuffer->getFiltered(filtering),priv->scaler);
			delete tc.pixelBuffer;
			updatedBuffers++;
		}
		if(tc.bentNormalsPerPixel)
		{
			flush(bentNormalsPerPixel,tc.bentNormalsPerPixel->getFiltered(filtering),priv->scaler);
			delete tc.bentNormalsPerPixel;
			updatedBuffers++;
		}

	}

	return updatedBuffers;
}

//ve scene je emisivni material:
// rt update:                  [externi calculate(): OK emis se propagne] OK na vystup se posle jen propagly (ne src)
// direct light(final gather): OK(slaby vykon) emis se gatherne
// GI(gather+propag+gather):   OK emis se negatherne, OK emis se propagne, OK(slaby vykon) emis i propagly se gatherne

unsigned RRDynamicSolver::updateLightmaps(int layerNumberLighting, int layerNumberBentNormals, const UpdateParameters* _paramsDirect, const UpdateParameters* _paramsIndirect, const FilteringParameters* _filtering)
{
	UpdateParameters paramsDirect;
	UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	if(_paramsDirect) paramsDirect = *_paramsDirect;
	if(_paramsIndirect) paramsIndirect = *_paramsIndirect;

	if(paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		if(_paramsDirect) // don't report if direct is NULL, silently disable it
			RRReporter::report(WARN,"paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
	}

	bool containsFirstGather = _paramsIndirect && (paramsIndirect.applyCurrentSolution || paramsIndirect.applyLights || paramsIndirect.applyEnvironment);
	bool containsRealtime = !paramsDirect.applyLights && !paramsDirect.applyEnvironment && paramsDirect.applyCurrentSolution && !paramsDirect.quality;
	bool containsVertexBuffers = false;
	bool containsLightmaps = false;
	for(unsigned object=0;object<getNumObjects();object++)
	{
		containsVertexBuffers |= getIllumination(object) && getIllumination(object)->getLayer(layerNumberLighting) && getIllumination(object)->getLayer(layerNumberLighting)->getType()==BT_VERTEX_BUFFER;
		containsVertexBuffers |= getIllumination(object) && getIllumination(object)->getLayer(layerNumberBentNormals) && getIllumination(object)->getLayer(layerNumberBentNormals)->getType()==BT_VERTEX_BUFFER;
		containsLightmaps     |= getIllumination(object) && getIllumination(object)->getLayer(layerNumberLighting) && getIllumination(object)->getLayer(layerNumberLighting)->getType()==BT_2D_TEXTURE;
		containsLightmaps     |= getIllumination(object) && getIllumination(object)->getLayer(layerNumberBentNormals) && getIllumination(object)->getLayer(layerNumberBentNormals)->getType()==BT_2D_TEXTURE;
	}

	RRReportInterval report((containsFirstGather||containsLightmaps||!containsRealtime)?INF1:INF3,"Updating lightmaps (%d,%d,DIRECT(%s%s%s),INDIRECT(%s%s%s)).\n",
		layerNumberLighting,layerNumberBentNormals,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	// 1. first gather: solver+lights+env -> solver.direct
	// 2. propagate: solver.direct -> solver.indirect
	if(containsFirstGather)
	{
		// auto quality for first gather
		unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();

		// shoot 4x less indirect rays than direct
		// (but only if direct.quality was specified)
		if(_paramsDirect) paramsIndirect.quality = paramsDirect.quality/4;
		unsigned benchTexels = numTriangles;

		// 1. first gather: solver.direct+indirect+lights+env -> solver.direct
		// 2. propagate: solver.direct -> solver.indirect
		if(!updateSolverIndirectIllumination(&paramsIndirect,benchTexels,paramsDirect.quality))
			return 0;

		paramsDirect.applyCurrentSolution = true; // set solution generated here to be gathered in final gather
		paramsDirect.measure_internal.direct = true; // it is stored in direct+indirect (after calculate)
		paramsDirect.measure_internal.indirect = true;
		// musim gathernout direct i indirect.
		// indirect je jasny, jedine v nem je vysledek spocteny v calculate().
		// ovsem direct musim tez, jedine z nej dostanu prime osvetleni emisivnim facem, prvni odraz od spotlight, lights a env
	}

	unsigned updatedBuffers = 0;

	if(!paramsDirect.applyLights && !paramsDirect.applyEnvironment && !paramsDirect.applyCurrentSolution)
	{
		RRReporter::report(WARN,"No light sources enabled.\n");
	}

	// 3. vertex: realtime copy into buffers (solver not modified)
	if(containsVertexBuffers && containsRealtime)
	{
		for(int objectHandle=0;objectHandle<(int)priv->objects.size();objectHandle++)
		{
			if(layerNumberLighting>=0)
			{
				RRBuffer* vertexColors = getIllumination(objectHandle)->getLayer(layerNumberLighting);
				if(vertexColors && vertexColors->getType()==BT_VERTEX_BUFFER)
					updatedBuffers += updateVertexBufferFromSolver(objectHandle,vertexColors,&paramsDirect);
				//if(vertexColors && vertexColors->getType()!=BT_VERTEX_BUFFER)
				//	LIMITED_TIMES(1,RRReporter::report(WARN,"Lightmaps not updated in 'realtime' mode (quality=0, applyCurrentSolution only).\n"));
			}
			if(layerNumberBentNormals>=0)
			{
				RRBuffer* bentNormals = getIllumination(objectHandle)->getLayer(layerNumberBentNormals);
				if(bentNormals)
					LIMITED_TIMES(1,RRReporter::report(WARN,"Bent normals not updated in 'realtime' mode (quality=0, applyCurrentSolution only).\n"));
			}
		}
	}

	// 4+5. vertex: final gather into vertex buffers (solver not modified)
	if(containsVertexBuffers && !containsRealtime)
	{
		if(containsVertexBuffers && (paramsDirect.applyLights || paramsDirect.applyEnvironment || (paramsDirect.applyCurrentSolution && paramsDirect.quality)))
		{
			// 4. final gather: solver.direct+indirect+lights+env -> tmparray
			// for each triangle
			unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
			ProcessTexelResult* finalGather = new ProcessTexelResult[numTriangles];
			gatherPerTriangle(&paramsDirect,finalGather,numTriangles,priv->staticObjectsContainEmissiveMaterials); // this is final gather -> gather emissive materials

			// 5. interpolate: tmparray -> buffer
			// for each object with vertex buffer
			for(unsigned objectHandle=0;objectHandle<priv->objects.size();objectHandle++)
			{
				if(layerNumberLighting>=0)
				{
					RRBuffer* vertexColors = getIllumination(objectHandle)->getLayer(layerNumberLighting);
					if(vertexColors && vertexColors->getType()==BT_VERTEX_BUFFER)
						updatedBuffers += updateVertexBufferFromPerTriangleData(objectHandle,vertexColors,&finalGather[0].irradiance,sizeof(finalGather[0]));
				}
				if(layerNumberBentNormals>=0)
				{
					RRBuffer* bentNormals = getIllumination(objectHandle)->getLayer(layerNumberBentNormals);
					if(bentNormals && bentNormals->getType()==BT_VERTEX_BUFFER)
						updatedBuffers += updateVertexBufferFromPerTriangleData(objectHandle,bentNormals,&finalGather[0].bentNormal,sizeof(finalGather[0]));
				}
			}
			delete[] finalGather;
		}

	}

	// 6. pixel: final gather into pixel buffers (solver not modified)
	if(containsLightmaps)
	{
		for(unsigned object=0;object<getNumObjects();object++)
		{
			RRBuffer* lightmap = (layerNumberLighting>=0 && getIllumination(object)) ? getIllumination(object)->getLayer(layerNumberLighting) : NULL;
			if(lightmap && lightmap->getType()!=BT_2D_TEXTURE) lightmap = NULL;
			RRBuffer* bentNormals = (layerNumberBentNormals>=0 && getIllumination(object)) ? getIllumination(object)->getLayer(layerNumberBentNormals) : NULL;
			if(bentNormals && bentNormals->getType()!=BT_2D_TEXTURE) bentNormals = NULL;
			if(lightmap || bentNormals)
			{
				updatedBuffers += updateLightmap(object,lightmap,bentNormals,&paramsDirect,_filtering);
			}
		}
	}

	return updatedBuffers;
}

} // namespace

