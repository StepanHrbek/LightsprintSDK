#ifndef RRVISION_RRVISIONAPP_H
#define RRVISION_RRVISIONAPP_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVisionApp.h
//! \brief RRVision - library for fast global illumination calculations
//! \version 2006.4.16
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4530) // exceptions thrown but disabled, may crash
#endif

#include <cassert>
#include <map>
#include <vector>
#include "RRVision.h"

namespace rrVision
{

// *******************************************************************************
// nezavisle na rrvision, mozno pouzit ve final hre
// *******************************************************************************

	//////////////////////////////////////////////////////////////////////////////
	//
	//! 8bit intensity - One of color formats for vertex and pixel buffers.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRColorI8
	{
		RRColorI8()
		{
			color = 0;
		}
		RRColorI8(RRReal r,RRReal g,RRReal b)
		{
			color = (unsigned char)(255/3*(r+g+b));
		}
		bool operator ==(const RRColorI8& a)
		{
			return color==a.color;
		}
		bool operator !=(const RRColorI8& a)
		{
			return color!=a.color;
		}
		unsigned char color;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! RGBA8, total 32bits - One of color formats for vertex and pixel buffers.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRColorRGBA8
	{
		RRColorRGBA8()
		{
			color = 0;
		}
		RRColorRGBA8(RRReal r,RRReal g,RRReal b)
		{
			color = ((unsigned char)(255*r)&255) + (((unsigned char)(255*g)&255)<<8) + (((unsigned char)(255*b)&255)<<16);
		}
		bool operator ==(const RRColorI8& a)
		{
			return color==a.color;
		}
		bool operator !=(const RRColorI8& a)
		{
			return color!=a.color;
		}
		unsigned color;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface - Illumination storage based on vertex buffer.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRIlluminationVertexBuffer
	{
	public:
		//! Sets size of buffer. Content may be lost.
		virtual void setSize(unsigned numVertices) = 0;
		//! Sets value of one element of buffer.
		virtual void setVertex(unsigned vertex, const RRColor& color) = 0;
		virtual ~RRIlluminationVertexBuffer() {};
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in vertex buffer in system memory.
	//
	//! Template parameter Color specifies format of one element in vertex buffer.
	//! It can be RRColor, RRColorI8.
	//
	//////////////////////////////////////////////////////////////////////////////

	template <class Color>
	class RRIlluminationVertexBufferInMemory : public RRIlluminationVertexBuffer
	{
	public:
		RRIlluminationVertexBufferInMemory(unsigned anumVertices)
		{
			vertices = NULL;
			RRIlluminationVertexBufferInMemory::setSize(anumVertices);
		}
		virtual void setSize(unsigned anumVertices)
		{
			delete vertices;
			numVertices = anumVertices;
			vertices = new Color[numVertices];
		}
		const Color* lock()
		{
			return vertices;
		}
		virtual void setVertex(unsigned vertex, const RRColor& color)
		{
			if(!vertices)
			{
				assert(0);
				return;
			}
			if(vertex>=numVertices)
			{
				assert(0);
				return;
			}
			vertices[vertex] = color;
		}
		virtual ~RRIlluminationVertexBufferInMemory()
		{
			delete[] vertices;
		}
	private:
		unsigned numVertices;
		Color* vertices;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface - Illumination storage based on pixel buffer.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRIlluminationPixelBuffer
	{
	public:
		//! Sets size of buffer. Content may be lost.
		virtual void setSize(unsigned width, unsigned height) = 0;
		//! Marks all pixels as unused. Content may be lost.
		virtual void markAllUnused() {};
		//! Renders one triangle into map. Marks all triangle pixels as used. All other pixels stay unchanged.
		virtual void renderTriangle(const RRScene::SubtriangleIllumination& si) = 0;
		//! Filters map so that unused pixels close to used pixels get their color (may be also marked as used). Used pixels stay unchanged.
		virtual void growUsed() {};
		virtual ~RRIlluminationPixelBuffer() {};
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in system memory.
	//
	//! Template parameter Color specifies format of one element in pixel buffer.
	//! It can be RRColor, RRColorI8.
	//
	//////////////////////////////////////////////////////////////////////////////

	template <class Color>
	class RRIlluminationPixelBufferInMemory : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInMemory(unsigned awidth, unsigned aheight)
		{
			pixels = NULL;
			RRIlluminationPixelBufferInMemory::setSize(awidth,aheight);
		}
		virtual void setSize(unsigned awidth, unsigned aheight)
		{
			delete pixels;
			width = awidth;
			height = aheight;
			pixels = new Color[width*height];
		}
		const Color* lock()
		{
			return pixels;
		}
		virtual void markAllUnused()
		{
			for(unsigned i=0;i<width*height;i++)
			{
				pixels[i] = Color(1,1,0);
			}
		}
		virtual void renderTriangle(const RRScene::SubtriangleIllumination& si);
		virtual void growUsed()
		{
			for(unsigned j=0;j<height-1;j++)
			for(unsigned i=0;i<width-1;i++)
			{
				if(pixels[width*j+i]==Color(1,1,0))
				{
					if(pixels[width*j+i+1]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*j+i+1]; else
					if(pixels[width*(j+1)+i]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*(j+1)+i];
				}
			}
			for(unsigned j=height;--j>0;)
			for(unsigned i=width;--i>0;)
			{
				if(pixels[width*j+i]==Color(1,1,0))
				{
					if(pixels[width*j+i-1]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*j+i-1]; else
					if(pixels[width*(j-1)+i]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*(j-1)+i];
				}
			}
		}
		~RRIlluminationPixelBufferInMemory()
		{
			delete[] pixels;
		}
	private:
		unsigned width;
		unsigned height;
		Color*   pixels;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in OpenGL texture. Plus texture coords.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationPixelBufferInOpenGL : public RRIlluminationPixelBufferInMemory<RRColorI8>
	{
	public:
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight, RRObject* object, unsigned anumPreImportVertices)
			: RRIlluminationPixelBufferInMemory<RRColorI8>(awidth,aheight)
		{
			texCoord = NULL;
			numVertices = anumPreImportVertices;
		}
		RRVec2* getTexCoord(bool update)
		{
			if(!texCoord)
			{
				texCoord = new RRVec2[numVertices];
				update=true;
			}
			return texCoord;
		}
		~RRIlluminationPixelBufferInOpenGL()
		{
			delete[] texCoord;
		}
	private:
		unsigned numVertices;
		RRVec2* texCoord;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Also unwrap for illumination maps may be stored here.
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
			pixelBufferUnwrap = NULL;
		}
		struct Channel
		{
			Channel(unsigned anumVertices)
			{
				vertexBuffer = new RRIlluminationVertexBufferInMemory<RRColor>(anumVertices);
				pixelBuffer = new RRIlluminationPixelBufferInMemory<RRColorI8>(256,256);
			}
			RRIlluminationVertexBuffer* vertexBuffer;
			RRIlluminationPixelBuffer* pixelBuffer;
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
		const RRVec2* getPixelBufferUnwrap()
		{
			return pixelBufferUnwrap;
		}
		~RRObjectIllumination()
		{
			delete[] pixelBufferUnwrap;
		}
	protected:
		unsigned numPreImportVertices; ///< PreImport number of vertices, length of vertex buffer for rendering.
		std::map<unsigned,Channel*> channels; ///< Calculated illumination.
		RRVec2* pixelBufferUnwrap; ///< Optional unwrap for illumination in pixel buffers.
	};


// *******************************************************************************
// zavisle na rrvision, pouze pro editor
// *******************************************************************************

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination, extended for editor.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRObjectIlluminationForEditor : public RRObjectIllumination
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
			rrCollider::RRMesh* mesh = object->getCollider()->getImporter();
			unsigned numPostImportTriangles = mesh->getNumTriangles();
			for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
			{
				RRObject::TriangleMapping triangleMapping;
				object->getTriangleMapping(postImportTriangle,triangleMapping);
				rrCollider::RRMesh::Triangle triangle;
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

	class RRVISION_API RRVisionApp
	{
	public:
		RRVisionApp();
		virtual ~RRVisionApp();

		//! Defines objects present in scene.
		typedef std::pair<RRObject*,RRObjectIlluminationForEditor*> Object;
		typedef std::vector<Object> Objects;
		void setObjects(Objects& objects);
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
		void reportLightChange();
		//! Reports to framework that user interacts.
		void reportInteraction();

		//!!!
		RRObjectAdditionalIllumination* multiObject;
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
		// calculate
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		bool       dirtyLights;
		long       lastInteractionTime;
		float      readingResultsPeriod;
		float      calcTimeSinceReadingResults;
		// read results
		void       updateVertexLookupTable();
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table
		void       readVertexResults();
		void       readPixelResults();
		unsigned   resultChannelIndex;
	};

} // namespace

#endif
