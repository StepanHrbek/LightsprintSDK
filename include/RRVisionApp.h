#ifndef RRVISION_RRVISIONAPP_H
#define RRVISION_RRVISIONAPP_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVisionApp.h
//! \brief RRVision - library for fast global illumination calculations
//! \version 2006.4.2
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4530) // exceptions thrown but disabled, may crash

#include <map>
#include <vector>
#include "RRVision.h"

namespace rrVision
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Add one instance to each of your objects.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRObjectIllumination
	{
	public:
		//! Enter PreImport number of vertices, length of vertex buffer for rendering.
		//! Vertex buffer will be created for PreImport vertices, so it is not suitable for MultiObject.
		RRObjectIllumination(unsigned anumPreImportVertices)
		{
			numPreImportVertices = anumPreImportVertices;
		}
		~RRObjectIllumination()
		{
		}
		enum Format
		{
			NONE,
			ARGB8,
			RGB32F,
		};
		struct VertexBuffer
		{
			VertexBuffer(Format aformat, unsigned anumPreImportVertices)
			{
				memset(this,0,sizeof(*this));
				format = aformat;
				stride = (format==ARGB8)?4:((format==RGB32F)?12:0);
				numPreImportVertices = anumPreImportVertices;
				vertices = stride ? new char[stride*numPreImportVertices] : NULL;
			}
			~VertexBuffer()
			{
				delete[] vertices;
			}
			Format   format; //! May be requested to change.
			unsigned stride; //! May be requested to change.
			unsigned numPreImportVertices; //! Never changes during lifetime.
			char*    vertices;
		};
		struct PixelBuffer
		{
			PixelBuffer(Format aformat, unsigned amapWidth, unsigned amapHeight)
			{
				memset(this,0,sizeof(*this));
				format = aformat;
				stride = (format==ARGB8)?4:((format==RGB32F)?12:0);
				mapWidth = amapWidth;
				mapHeight = amapHeight;
				pixels = stride ? new char[stride*mapWidth*mapHeight] : NULL;
			}
			~PixelBuffer()
			{
				delete[] pixels;
			}
			Format   format; //! May be requested to change.
			unsigned stride; //! May be requested to change.
			unsigned mapWidth; //! May be requested to change.
			unsigned mapHeight; //! May be requested to change.
			char*    pixels;
		};
		struct Channel
		{
			Channel(unsigned anumPreImportVertices)
				: vertexBuffer(RGB32F,anumPreImportVertices), pixelBuffer(NONE,0,0)
			{
			}
			VertexBuffer vertexBuffer;
			PixelBuffer pixelBuffer;
		};
		Channel* getChannel(unsigned channelIndex)
		{
			std::map<unsigned,Channel*>::iterator i = channels.find(channelIndex);
			if(i!=channels.end()) return i->second;
			Channel* tmp = new Channel(numPreImportVertices);
			channels[channelIndex] = tmp;
			return tmp;
		}
		unsigned getNumPreImportVertices()
		{
			return numPreImportVertices;
		}
	private:
		unsigned numPreImportVertices; ///< PreImport number of vertices, length of vertex buffer for rendering.
		std::map<unsigned,Channel*> channels; ///< Calculated illumination.
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRVisionApp
	//! Framework for Vision application.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRVisionApp
	{
	public:
		RRVisionApp();
		virtual ~RRVisionApp();

		//! Defines objects present in scene.
		typedef std::pair<RRObjectImporter*,RRObjectIllumination*> Object;
		typedef std::vector<Object> Objects;
		void setObjects(Objects& objects);
		RRObjectImporter* getObject(unsigned i);
		RRObjectIllumination* getIllumination(unsigned i);
		//! Selects channel for storing results, 0 is default.
		void setResultChannel(unsigned channelIndex);

		//! Calculates, improves indirect illumination on objects, stores into given channel.
		RRScene::Improvement calculate();

		//! Reports to framework that appearance of one or more materials has changed.
		void reportMaterialChange();
		//! Reports to framework that position/rotation/shape of one or more objects has changed.
		void reportGeometryChange();
		//! Reports to framework that position/rotation/shape of one or more lights has changed.
		void reportLightChange();
		//! Reports to framework that user interacts.
		void reportInteraction();

		//!!!
		RRAdditionalObjectImporter* multiObject;
		RRScene*   scene;
	protected:
		//! Autodetects material properties of all materials present in scene. To be implemented by you.
		virtual void detectMaterials() = 0;
		//! Autodetects direct illumination on all faces in scene. To be implemented by you.
		virtual void detectDirectIllumination() = 0;

		//! Adjusts RRScene parameters each time new scene is created. May be reimplemented by you.
		virtual void adjustScene();
		//! Framework reports event to you. May be reimplemented by you.
		virtual void reportAction(const char* action) const;

		Objects    objects;
		RRSurface* surfaces;
		unsigned   numSurfaces;

	private:
		void       readResults();
		unsigned   resultChannelIndex;
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		bool       dirtyLights;
		long       lastInteractionTime;
		float      readingResultsPeriod;
		float      calcTimeSinceReadingResults;
	};

} // namespace

#endif
