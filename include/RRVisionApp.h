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
		//! For editor.
		RRObjectIllumination(RRObjectImporter* aimporter)
		{
			importer = aimporter->createAdditionalIllumination();
			numVertices = aimporter->getCollider()->getImporter()->getNumVertices();
		}
		//! For game.
		RRObjectIllumination(unsigned anumVertices)
		{
			importer = NULL;
			numVertices = anumVertices;
		}
		~RRObjectIllumination()
		{
			delete importer;
		}
		RRObjectImporter* getObjectImporter()
		{
			return importer;
		}
		enum Format
		{
			NONE,
			ARGB8,
			RGB32F,
		};
		struct VertexBuffer
		{
			VertexBuffer(Format aformat, unsigned anumVertices)
			{
				memset(this,0,sizeof(*this));
				format = aformat;
				stride = (format==ARGB8)?4:((format==RGB32F)?12:0);
				numVertices = anumVertices;
				vertices = stride ? new char[stride*numVertices] : NULL;
			}
			~VertexBuffer()
			{
				delete[] vertices;
			}
			Format   format; //! May be requested to change.
			unsigned stride; //! May be requested to change.
			unsigned numVertices; //! Never changes during lifetime.
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
			Channel(unsigned anumVertices)
				: vertexBuffer(RGB32F,anumVertices), pixelBuffer(NONE,0,0)
			{
			}
			VertexBuffer vertexBuffer;
			PixelBuffer pixelBuffer;
		};
		Channel* getChannel(unsigned channelIndex)
		{
			std::map<unsigned,Channel*>::iterator i = channels.find(channelIndex);
			if(i!=channels.end()) return i->second;
			Channel* tmp = new Channel(numVertices);
			channels[channelIndex] = tmp;
			return tmp;
		}
	private:
		unsigned numVertices;
		std::map<unsigned,Channel*> channels;
		//! Only in editor, not in final game.
		RRAdditionalObjectImporter* importer;
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
		typedef std::vector<RRObjectIllumination*> Objects;
		void setObjects(Objects& objects);
		//! Selects channel for storing results, 0 is default.
		void setResultChannel(unsigned channelIndex);

		//! Calculates, improves indirect illumination on objects, stores into given channel.
		void calculate();

		//! Reports to framework that appearance of one or more materials has changed.
		void reportMaterialChange();
		//! Reports to framework that position/rotation/shape of one or more objects has changed.
		void reportGeometryChange();
		//! Reports to framework that position/rotation/shape of one or more lights has changed.
		void reportLightChange();
		//! Reports to framework that user interacts.
		void reportInteraction();

	protected:
		//! Autodetects material properties of all materials present in scene. To be implemented by you.
		virtual void detectMaterials() = 0;
		//! Autodetects direct illumination on all faces in scene. To be implemented by you.
		virtual void detectDirectIllumination() = 0;

		//! Adjusts RRScene parameters each time new scene is created. May be reimplemented by you.
		virtual void adjustScene();
		//! Framework reports event to you. May be reimplemented by you.
		virtual void reportAction(const char* action) const;

		Objects objects;
		RRSurface* surfaces;
		RRScene* scene;
	private:
		void readResults();
		unsigned resultChannelIndex;
		bool dirtyMaterials;
		bool dirtyGeometry;
		bool dirtyLights;
		long lastInteractionTime;
		float readingResultsPeriod;
		float calcTimeSinceReadingResults;
	};

} // namespace

#endif
