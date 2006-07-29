#ifndef RRILLUMCALCULATOR_H
#define RRILLUMCALCULATOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumCalculator.h
//! \brief RRIllumCalculator - high level library for calculating global illumination
//! \version 2006.6.6
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <vector>
#include "RRVision.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
			#pragma comment(lib,"RRIllumCalculator_s.lib")
		#else
			#pragma comment(lib,"RRIllumCalculator_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_CALCULATOR
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"RRIllumCalculator.lib")
#	endif
#	endif
#endif

#include "RRIllumPrecalculated.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination, extended for editor.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIlluminationForEditor : public RRObjectIllumination
	{
	public:
		RRObjectIlluminationForEditor(unsigned anumPreImportVertices)
			: RRObjectIllumination(anumPreImportVertices)
		{
		}
		void createPixelBufferUnwrap(RRObject* object)
		{
			if(pixelBufferUnwrap)
				delete[] pixelBufferUnwrap;
			pixelBufferUnwrap = new RRVec2[numPreImportVertices];
			RRMesh* mesh = object->getCollider()->getMesh();
			unsigned numPostImportTriangles = mesh->getNumTriangles();
			for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
			{
				RRObject::TriangleMapping triangleMapping;
				object->getTriangleMapping(postImportTriangle,triangleMapping);
				RRMesh::Triangle triangle;
				mesh->getTriangle(postImportTriangle,triangle);
				for(unsigned v=0;v<3;v++)
				{
					//!!!
					// muj nouzovy primitivni unwrap vetsinou nejde prevest z trianglu do vertexBufferu,
					// protoze jeden vertex je casto pouzit vice triangly v meshi
					unsigned preImportVertex = mesh->getPreImportVertex(triangle[v],postImportTriangle);
					if(preImportVertex<numPreImportVertices)
						pixelBufferUnwrap[preImportVertex] = triangleMapping.uv[v];
					else
						assert(0);
				}
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRVisionApp
	//! Framework for Vision application.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRVisionApp
	{
	public:
		RRVisionApp();
		virtual ~RRVisionApp();

		//! Defines objects present in scene.
		typedef std::pair<RRObject*,RRObjectIlluminationForEditor*> Object;
		typedef std::vector<Object> Objects;
		void setObjects(Objects& objects, float stitchDistance = 0.01f);
		RRObject* getObject(unsigned i);
		RRObjectIlluminationForEditor* getIllumination(unsigned i);
		
		//! Selects channel for storing results, 0 is default.
		void setResultChannel(unsigned channelIndex);

		//! Calculates, improves indirect illumination on objects, stores into given channel.
		RRScene::Improvement calculate();

		//! Reports to framework that appearance of one or more materials has changed.
		void reportMaterialChange();
		//! Reports to framework that position/rotation/shape of one or more objects has changed.
		void reportGeometryChange();
		//! Reports to framework that position/rotation/shape of one or more lights has changed.
		void reportLightChange(bool strong);
		//! Reports to framework that illumination has been used (eg. for rendering). Without such report, illumination is updated less often, which saves time for calculations.
		void reportIlluminationUse();
		//! Reports to framework that user interacts and needs maximal responsiveness. Framework stops all actions for short time.
		void reportCriticalInteraction();
		//! Reports to framework that critical interactions end. Framework can immediately start its activities.
		void reportEndOfInteractions();

		//!!!
		RRObjectAdditionalIllumination* multiObject;
		RRScene*   scene;
	protected:
		//! Autodetects material properties of all materials present in scene. To be implemented by you.
		//! New values must appear in RRObjects already present in scene.
		virtual void detectMaterials() = 0;
		//! Autodetects direct illumination on all faces in scene. To be implemented by you.
		//! You may fail by returning false, you will get another chance next time.
		virtual bool detectDirectIllumination() = 0;

		//! Adjusts RRScene parameters each time new scene is created. May be reimplemented by you.
		virtual void adjustScene();
		//! Framework reports event to you. May be reimplemented by you.
		virtual void reportAction(const char* action) const;

		Objects    objects;
		RRSurface* surfaces;
		unsigned   numSurfaces;

	private:
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};
		// calculate
		float      stitchDistance;
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		long       lastIlluminationUseTime;
		long       lastCriticalInteractionTime;
		long       lastCalcEndTime;
		long       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRScene::Improvement calculateCore(float improveStep);
		// read results
		void       updateVertexLookupTable();
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table
		void       readVertexResults();
		void       readPixelResults();
		unsigned   resultChannelIndex;
	};

} // namespace

#endif
