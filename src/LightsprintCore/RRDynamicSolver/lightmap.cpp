#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"
#include "gather.h"

//#define DIAGNOSTIC // sleduje texel overlapy, vypisuje histogram velikosti trianglu

namespace rr
{

struct RenderSubtriangleContext
{
	RRIlluminationPixelBuffer* pixelBuffer;
	RRMesh::TriangleMapping triangleMapping;
};

void renderSubtriangle(const RRStaticSolver::SubtriangleIllumination& si, void* context)
{
	RenderSubtriangleContext* context2 = (RenderSubtriangleContext*)context;
	RRIlluminationPixelBuffer::IlluminatedTriangle si2;
	for(unsigned i=0;i<3;i++)
	{
		si2.iv[i].measure = si.measure[i];
		RR_ASSERT(context2->triangleMapping.uv[0][0]>=0 && context2->triangleMapping.uv[0][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[0][1]>=0 && context2->triangleMapping.uv[0][1]<=1);
		RR_ASSERT(context2->triangleMapping.uv[1][0]>=0 && context2->triangleMapping.uv[1][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[1][1]>=0 && context2->triangleMapping.uv[1][1]<=1);
		RR_ASSERT(context2->triangleMapping.uv[2][0]>=0 && context2->triangleMapping.uv[2][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[2][1]>=0 && context2->triangleMapping.uv[2][1]<=1);
		RR_ASSERT(si.texCoord[i][0]>=0 && si.texCoord[i][0]<=1);
		RR_ASSERT(si.texCoord[i][1]>=0 && si.texCoord[i][1]<=1);
		// si.texCoord 0,0 prevest na context2->triangleMapping.uv[0]
		// si.texCoord 1,0 prevest na context2->triangleMapping.uv[1]
		// si.texCoord 0,1 prevest na context2->triangleMapping.uv[2]
		si2.iv[i].texCoord = context2->triangleMapping.uv[0] + (context2->triangleMapping.uv[1]-context2->triangleMapping.uv[0])*si.texCoord[i][0] + (context2->triangleMapping.uv[2]-context2->triangleMapping.uv[0])*si.texCoord[i][1];
		RR_ASSERT(si2.iv[i].texCoord[0]>=0 && si2.iv[i].texCoord[0]<=1);
		RR_ASSERT(si2.iv[i].texCoord[1]>=0 && si2.iv[i].texCoord[1]<=1);
		for(unsigned j=0;j<3;j++)
		{
			RR_ASSERT(_finite(si2.iv[i].measure[j]));
			RR_ASSERT(si2.iv[i].measure[j]>=0);
			RR_ASSERT(si2.iv[i].measure[j]<1500000);
		}
	}
	context2->pixelBuffer->renderTriangle(si2);
}

RRIlluminationPixelBuffer* RRDynamicSolver::newPixelBuffer(RRObject* object)
{
	return NULL;
}


#ifdef DIAGNOSTIC
unsigned hist[100];
void logReset()
{
	memset(hist,0,sizeof(hist));
}
void logRasterizedTriangle(unsigned numTexels)
{
	if(numTexels>=100) numTexels = 99;
	hist[numTexels]++;
}
void logPrint()
{
	printf("Numbers of triangles with 0/1/2/...97/98/more texels:\n");
	for(unsigned i=0;i<100;i++)
		printf("%d ",hist[i]);
}
#endif

const int RightHandSide        = -1;
const int LeftHandSide         = +1;
const int CollinearOrientation =  0;

template<typename T>
inline int orientation(const T& x1, const T& y1,
					   const T& x2, const T& y2,
					   const T& px, const T& py)
{
	T orin = (x2 - x1) * (py - y1) - (px - x1) * (y2 - y1);

	if (orin > 0.0)      return LeftHandSide;         /* Orientaion is to the left-hand side  */
	else if (orin < 0.0) return RightHandSide;        /* Orientaion is to the right-hand side */
	else                 return CollinearOrientation; /* Orientaion is neutral aka collinear  */
}

template<typename T>
inline void closest_point_on_segment_from_point(const T& x1, const T& y1,
												const T& x2, const T& y2,
												const T& px, const T& py,
												T& nx,       T& ny)
{
	T vx = x2 - x1;
	T vy = y2 - y1;
	T wx = px - x1;
	T wy = py - y1;

	T c1 = vx * wx + vy * wy;

	if (c1 <= T(0.0))
	{
		nx = x1;
		ny = y1;
		return;
	}

	T c2 = vx * vx + vy * vy;

	if (c2 <= c1)
	{
		nx = x2;
		ny = y2;
		return;
	}

	T ratio = c1 / c2;

	nx = x1 + ratio * vx;
	ny = y1 + ratio * vy;
}

template<typename T>
inline T distance(const T& x1, const T& y1, const T& x2, const T& y2)
{
	T dx = (x1 - x2);
	T dy = (y1 - y2);
	return sqrt(dx * dx + dy * dy);
}

template<typename T>
inline void closest_point_on_triangle_from_point(const T& x1, const T& y1,
												 const T& x2, const T& y2,
												 const T& x3, const T& y3,
												 const T& px, const T& py,
												 T& nx,       T& ny)
{
	if (orientation(x1,y1,x2,y2,px,py) != orientation(x1,y1,x2,y2,x3,y3))
	{
		closest_point_on_segment_from_point(x1,y1,x2,y2,px,py,nx,ny);
		if (orientation(x2,y2,x3,y3,px,py) != orientation(x2,y2,x3,y3,x1,y1))
		{
			RRVec2 m;
			closest_point_on_segment_from_point(x2,y2,x3,y3,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		else
		if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
		{
			RRVec2 m;
			closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		return;
	}

	if (orientation(x2,y2,x3,y3,px,py) != orientation(x2,y2,x3,y3,x1,y1))
	{
		closest_point_on_segment_from_point(x2,y2,x3,y3,px,py,nx,ny);
		RRVec2 m;
		if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
		{
			closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		return;
	}

	if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
	{
		closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,nx,ny);
		return;
	}
	nx = px;
	ny = py;
}

//! Enumerates all texels on object's surface.
//
//! Default implementation runs callbacks on all CPUs/cores at once.
//! It is not strictly defined what texels are enumerated, but close match to
//! pixels visible on mapped object improves lightmap quality.
//! \n Enumeration order is not defined.
//! \param objectNumber
//!  Number of object in this scene.
//!  Object numbers are defined by order in which you pass objects to setObjects().
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

	// flag all texels as not yet enumerated
	//char* enumerated = new char[mapWidth*mapHeight];
	//memset(enumerated,0,mapWidth*mapHeight);

	int firstTriangle = 0;
	if(onlyTriangleNumber>=0)
	{
		firstTriangle = onlyTriangleNumber;
		numTriangles = 1;
	}

	// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
	RRRay* rays = RRRay::create(omp_get_max_threads());
#else
	RRRay* rays = RRRay::create(1);
#endif

#ifndef DIAGNOSTIC_RAYS
	#pragma omp parallel for schedule(dynamic) // fastest: dynamic, static,1, static
#endif
	for(int tt=firstTriangle;tt<(int)(firstTriangle+numTriangles);tt++)
	{
		unsigned t = (unsigned)tt;
		RRMesh::MultiMeshPreImportNumber preImportNumber = multiMesh->getPreImportTriangle(t);
		if(preImportNumber.object==objectNumber)
		{
#ifdef DIAGNOSTIC_RAYS
			logTexelIndex = 0;
#endif
			// gather data about triangle t
			ProcessTexelParams pti(tc);
			pti.tri.triangleIndex = t;
			multiMesh->getTriangleBody(t,pti.tri.triangleBody);
			RRMesh::TriangleNormals normals;
			multiMesh->getTriangleNormals(t,normals);
			RRMesh::TriangleMapping mapping;
			multiMesh->getTriangleMapping(t,mapping);
#ifdef _OPENMP
			pti.ray = rays+omp_get_thread_num();
#else
			pti.ray = rays;
#endif
			pti.ray->rayLengthMin = minimalSafeDistance;
			// rasterize triangle t
			//  find minimal bounding box
			RRReal xmin = mapWidth  * MIN(mapping.uv[0][0],MIN(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal xmax = mapWidth  * MAX(mapping.uv[0][0],MAX(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal ymin = mapHeight * MIN(mapping.uv[0][1],MIN(mapping.uv[1][1],mapping.uv[2][1]));
			RRReal ymax = mapHeight * MAX(mapping.uv[0][1],MAX(mapping.uv[1][1],mapping.uv[2][1]));
			RR_ASSERT(xmin>=0 && xmax<=mapWidth);
			RR_ASSERT(ymin>=0 && ymax<=mapHeight);
			//  precompute mapping[0]..mapping[1] line and mapping[0]..mapping[2] line equations in 2d map space
			#define LINE_EQUATION(lineEquation,lineDirection,pointInDistance0,pointInDistance1) \
				lineEquation = RRVec3((lineDirection)[1],-(lineDirection)[0],0); \
				lineEquation[2] = -POINT_LINE_DISTANCE_2D(pointInDistance0,lineEquation); \
				lineEquation *= 1/POINT_LINE_DISTANCE_2D(pointInDistance1,lineEquation);
			LINE_EQUATION(pti.tri.line1InMap,mapping.uv[1]-mapping.uv[0],mapping.uv[0],mapping.uv[2]);
			LINE_EQUATION(pti.tri.line2InMap,mapping.uv[2]-mapping.uv[0],mapping.uv[0],mapping.uv[1]);
			//  for all texels in bounding box
			unsigned numTexels = 0;
			const int overlap = 0; // 0=only texels that contain part of triangle, 1=all texels that touch them by edge or corner
			for(int y=MAX((int)ymin-overlap,0);y<(int)MIN((unsigned)ymax+1+overlap,mapHeight);y++)
			{
				for(int x=MAX((int)xmin-overlap,0);x<(int)MIN((unsigned)xmax+1+overlap,mapWidth);x++)
				{
					// compute uv in triangle
					//  xy = mapSize*mapping[0] -> uvInTriangle = 0,0
					//  xy = mapSize*mapping[1] -> uvInTriangle = 1,0
					//  xy = mapSize*mapping[2] -> uvInTriangle = 0,1
					// do it for 4 corners of texel
					// do it with 1 texel overlap, so that filtering is not necessary
					RRVec2 uvInMapF0 = RRVec2(RRReal(x-overlap)/mapWidth,RRReal(y-overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF1 = RRVec2(RRReal(x-overlap)/mapWidth,RRReal(y+1+overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF2 = RRVec2(RRReal(x+1+overlap)/mapWidth,RRReal(y-overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF3 = RRVec2(RRReal(x+1+overlap)/mapWidth,RRReal(y+1+overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInTriangle0 = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF0,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF0,pti.tri.line1InMap));
					RRVec2 uvInTriangle1 = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF1,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF1,pti.tri.line1InMap));
					RRVec2 uvInTriangle2 = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF2,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF2,pti.tri.line1InMap));
					RRVec2 uvInTriangle3 = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF3,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF3,pti.tri.line1InMap));
					// process only texels at least partially inside triangle
					if((uvInTriangle0[0]>=0 || uvInTriangle1[0]>=0 || uvInTriangle2[0]>=0 || uvInTriangle3[0]>=0)
						&& (uvInTriangle0[1]>=0 || uvInTriangle1[1]>=0 || uvInTriangle2[1]>=0 || uvInTriangle3[1]>=0)
						&& (uvInTriangle0[0]+uvInTriangle0[1]<=1 || uvInTriangle1[0]+uvInTriangle1[1]<=1 || uvInTriangle2[0]+uvInTriangle2[1]<=1 || uvInTriangle3[0]+uvInTriangle3[1]<=1)
						) // <= >= makes small overlap
					{
						// find nearest position inside triangle
						// - center of texel
						RRVec2 uvInMapF = RRVec2((x+0.5f)/mapWidth,(y+0.5f)/mapHeight); // in 0..1 map space
						// - clamp to inside triangle (so that rays are shot from reasonable shooter)
						//   1. clamp to one of 3 lines of triangle - could stay outside triangle
						RRVec2 uvInTriangle = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF,pti.tri.line1InMap));
						if(uvInTriangle[0]<0 || uvInTriangle[1]<0 || uvInTriangle[0]+uvInTriangle[1]>1)
						{
							// skip texels outside border, that were already enumerated before
							//if(enumerated[x+y*mapWidth])
							//	continue;

							// clamp to triangle edge
							RRVec2 uvInMapF2;
							closest_point_on_triangle_from_point<RRReal>(
								mapping.uv[0][0],mapping.uv[0][1],
								mapping.uv[1][0],mapping.uv[1][1],
								mapping.uv[2][0],mapping.uv[2][1],
								uvInMapF[0],uvInMapF[1],
								uvInMapF2[0],uvInMapF2[1]);
							uvInTriangle = RRVec2(POINT_LINE_DISTANCE_2D(uvInMapF2,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMapF2,pti.tri.line1InMap));

							// hide precision errors (coords could be still outside triangle)
							const float depthInTriangle = 0.1f; // shooters on the edge often produce unreliable values
							if(uvInTriangle[0]<depthInTriangle) uvInTriangle[0] = depthInTriangle;
							if(uvInTriangle[1]<depthInTriangle) uvInTriangle[1] = depthInTriangle;
							RRReal uvInTriangleSum = uvInTriangle[0]+uvInTriangle[1];
							if(uvInTriangleSum>1-depthInTriangle) uvInTriangle *= (1-depthInTriangle)/uvInTriangleSum;
						}

						// compute uv in map and pos/norm in worldspace
						// enumerate texel
						//enumerated[x+y*mapWidth] = 1;
						pti.uv[0] = x;
						pti.uv[1] = y;
						pti.tri.pos3d = pti.tri.triangleBody.vertex0 + pti.tri.triangleBody.side1*uvInTriangle[0] + pti.tri.triangleBody.side2*uvInTriangle[1];
						pti.tri.normal = normals.norm[0] + (normals.norm[1]-normals.norm[0])*uvInTriangle[0] + (normals.norm[2]-normals.norm[0])*uvInTriangle[1];
						callback(pti);
#ifdef DIAGNOSTIC_RAYS
						logTexelIndex++;
#endif
						numTexels++;
					}
				}
			}
#ifdef DIAGNOSTIC
			logRasterizedTriangle(numTexels);
#endif
		}
	}
	delete[] rays;
	//delete[] enumerated;
}


unsigned RRDynamicSolver::updateLightmap(unsigned objectNumber, RRIlluminationPixelBuffer* pixelBuffer, RRIlluminationPixelBuffer* bentNormalsPerPixel, const UpdateParameters* aparams)
{
	// validate params
	UpdateParameters params;
	if(aparams) params = *aparams;

	// optimize params
	if(params.applyLights && !getLights().size())
		params.applyLights = false;
	if(params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;

	bool useGathering = params.applyLights || params.applyEnvironment || (params.applyCurrentSolution && params.quality);
	unsigned width = pixelBuffer ? pixelBuffer->getWidth() : bentNormalsPerPixel->getWidth();
	unsigned height = pixelBuffer ? pixelBuffer->getHeight() : bentNormalsPerPixel->getHeight();
	RRReportInterval report(useGathering?INF1:INF3,"Updating lightmap, object %d of %d, res %d*%d ...\n",objectNumber,getNumObjects(),width,height);

	if(!pixelBuffer && !bentNormalsPerPixel)
	{
		RRReporter::report(WARN,"No map, pixelBuffer=bentNormalsPerPixel=NULL.\n");
		RR_ASSERT(0); // no work, probably error
		return 0;
	}
	if(!getMultiObjectCustom() || !getStaticSolver() || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !getStaticSolver() || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RRReporter::report(WARN,"Empty scene.\n");
			RR_ASSERT(0);
			return 0;
		}
	}
	if(objectNumber>=getNumObjects())
	{
		RRReporter::report(WARN,"Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getNumObjects()-1);
		RR_ASSERT(0);
		return 0;
	}
	const RRObject* object = getMultiObjectCustom();
	RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numPostImportTriangles = mesh->getNumTriangles();

#ifdef DIAGNOSTIC
	logReset();
#endif
	if(pixelBuffer) pixelBuffer->renderBegin();
	if(bentNormalsPerPixel) bentNormalsPerPixel->renderBegin();
	if(useGathering)
	{
		// check that map sizes match
		if(pixelBuffer && bentNormalsPerPixel)
		{
			if(pixelBuffer->getWidth() != bentNormalsPerPixel->getWidth()
				|| pixelBuffer->getHeight() != bentNormalsPerPixel->getHeight())
			{
				RRReporter::report(ERRO,"Sizes don't match, lightmap=%dx%d, bentnormalmap=%dx%d.\n",pixelBuffer->getWidth(),pixelBuffer->getHeight(),bentNormalsPerPixel->getWidth(),bentNormalsPerPixel->getHeight());
				RR_ASSERT(0);
				return 0;
			}
		}

		TexelContext tc;
		tc.solver = this;
		tc.pixelBuffer = pixelBuffer;
		tc.params = &params;
		tc.bentNormalsPerPixel = bentNormalsPerPixel;
		// preallocate lightmap buffer before going parallel
		unsigned uv[2]={0,0};
		if(pixelBuffer) pixelBuffer->renderTexel(uv,RRColorRGBAF(0));
		if(bentNormalsPerPixel) bentNormalsPerPixel->renderTexel(uv,RRColorRGBAF(0));
		// continue with all texels, possibly in multiple threads
		enumerateTexels(getMultiObjectCustom(),objectNumber,width,height,processTexel,tc,priv->minimalSafeDistance);
		if(pixelBuffer) pixelBuffer->renderEnd(true);
		if(bentNormalsPerPixel) bentNormalsPerPixel->renderEnd(true);
#ifdef DIAGNOSTIC
		logPrint();
#endif
	}
	else
	if(params.applyCurrentSolution)
	{
		if(bentNormalsPerPixel)
		{
			RRReporter::report(WARN,"Bent normals won't be updated in 'realtime' mode.\n");
			RR_ASSERT(0);
			bentNormalsPerPixel->renderEnd(false);
			bentNormalsPerPixel = NULL; // necessary for correct return value (1 instead of 2)
			if(!pixelBuffer) return 0; // no work
		}
		// for each triangle in multimesh
		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			// process only triangles belonging to objectHandle
			RRMesh::MultiMeshPreImportNumber preImportTriangle = mesh->getPreImportTriangle(postImportTriangle);
			if(preImportTriangle.object==objectNumber)
			{
				// multiObject must preserve mapping (all objects overlap in one map)
				//!!! this is satisfied now, but it may change in future
				RenderSubtriangleContext rsc;
				rsc.pixelBuffer = pixelBuffer;
				mesh->getTriangleMapping(postImportTriangle,rsc.triangleMapping);
				// render all subtriangles into pixelBuffer using object's unwrap
				priv->scene->getSubtriangleMeasure(postImportTriangle,params.measure,getScaler(),renderSubtriangle,&rsc);
			}
		}
		pixelBuffer->renderEnd(false);
#ifdef DIAGNOSTIC
		logPrint();
#endif
	}
	else
	{
		RRReporter::report(WARN,"No lightsources.\n");
		pixelBuffer->renderEnd(false);
		if(bentNormalsPerPixel) bentNormalsPerPixel->renderEnd(false);
		RR_ASSERT(0);
	}
	return bentNormalsPerPixel ? 2 : 1;
}

unsigned RRDynamicSolver::updateLightmaps(int layerNumberLighting, int layerNumberBentNormals, bool createMissingBuffers, const UpdateParameters* aparamsDirect, const UpdateParameters* aparamsIndirect)
{
	UpdateParameters paramsIndirect;
	UpdateParameters paramsDirect;
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;
	if(aparamsDirect) paramsDirect = *aparamsDirect;

	RRReportInterval report(INF1,"Updating lightmaps (%d,%d,DIRECT(%s%s%s%s%s),INDIRECT(%s%s%s%s%s)).\n",
		layerNumberLighting,layerNumberBentNormals,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.direct)?"D":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.indirect)?"I":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.direct)?"D":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.indirect)?"I":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	// 0. create missing buffers
	if(createMissingBuffers)
	{
		for(unsigned object=0;object<getNumObjects();object++)
		{
			if(layerNumberLighting>=0)
			{
				RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumberLighting);
				if(layer && !layer->pixelBuffer) layer->pixelBuffer = newPixelBuffer(getObject(object));
			}
			if(layerNumberBentNormals>=0)
			{
				RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumberBentNormals);
				if(layer && !layer->pixelBuffer) layer->pixelBuffer = newPixelBuffer(getObject(object));
			}
		}
	}

	if(paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		RRReporter::report(WARN,"paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
	}

	if(aparamsIndirect)
	{
		// auto quality for first gather
		// shoot 10x less indirect rays than direct
		unsigned numTexels = 0;
		for(unsigned object=0;object<getNumObjects();object++)
		{
			RRIlluminationPixelBuffer* lmap = getIllumination(object)->getLayer(layerNumberLighting)->pixelBuffer;
			if(lmap) numTexels += lmap->getWidth()*lmap->getHeight();
		}
		unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		paramsIndirect.quality = (unsigned)(paramsDirect.quality*0.1f*numTexels/(numTriangles+1))+1;

		// 1. first gather: solver.direct+indirect+lights+env -> solver.direct
		// 2. propagate: solver.direct -> solver.indirect
		if(!updateSolverIndirectIllumination(&paramsIndirect,numTexels,paramsDirect.quality))
			return 0;

		paramsDirect.applyCurrentSolution = true; // set solution generated here to be gathered in final gather
		paramsDirect.measure.direct = true; // it is stored in direct+indirect (after calculate)
		paramsDirect.measure.indirect = true;
		// musim gathernout direct i indirect.
		// indirect je jasny, jedine v nem je vysledek spocteny v calculate().
		// ovsem direct musim tez, jedine z nej dostanu prime osvetleni emisivnim facem, prvni odraz od spotlight, lights a env
	}

	// 3. final gather into buffers (solver not nodified)
	unsigned updatedBuffers = 0;
	for(unsigned object=0;object<getNumObjects();object++)
	{
		RRIlluminationPixelBuffer* lightmap = (layerNumberLighting<0) ? NULL : getIllumination(object)->getLayer(layerNumberLighting)->pixelBuffer;
		RRIlluminationPixelBuffer* bentNormals = (layerNumberBentNormals<0) ? NULL : getIllumination(object)->getLayer(layerNumberBentNormals)->pixelBuffer;
		if(lightmap || bentNormals)
		{
			updatedBuffers += updateLightmap(object,lightmap,bentNormals,&paramsDirect);
		}
	}
	return updatedBuffers;
}

} // namespace
