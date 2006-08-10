#ifndef RRILLUMPRECALCULATEDSURFACE_H
#define RRILLUMPRECALCULATEDSURFACE_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumPrecalculatedSurface.h
//! \brief RRIllumPrecalculated - library for accessing precalculated illumination
//! \version 2006.8
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <map>
#include "RRMath.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Float RGB - One of color formats for vertex and pixel buffers.
	//
	// All color formats have default constructor that sets black color.
	// This makes vertex and pixels buffers black after construction.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRColorRGBF : public RRVec3
	{
	public:
		RRColorRGBF()
		{
			x = y = z = 0;
		}
		RRColorRGBF(const RRVec3& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
		}
	};


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

	class RR_API RRIlluminationVertexBuffer
	{
	public:
		//! Sets size of buffer. Content may be lost.
		virtual void setSize(unsigned numVertices) = 0;
		//! Sets value of one element of buffer.
		virtual void setVertex(unsigned vertex, const RRColorRGBF& color) = 0;
		virtual ~RRIlluminationVertexBuffer() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in vertex buffer in system memory.
	//
	//! Template parameter Color specifies format of one element in vertex buffer.
	//! It can be RRColorRGBF, RRColorRGBA8, RRColorI8.
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
			delete[] vertices;
			numVertices = anumVertices;
			vertices = new Color[numVertices];
		}
		const Color* lock()
		{
			return vertices;
		}
		virtual void setVertex(unsigned vertex, const RRColorRGBF& color)
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
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Also unwrap for illumination maps may be stored here.
	//! Add one instance to each of your objects.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination
	{
	public:
		//! Enter PreImport number of vertices, length of vertex buffer for rendering.
		//! Vertex buffer will be created for PreImport vertices, so it is not suitable for MultiObject.
		RRObjectIllumination(unsigned anumPreImportVertices)
		{
			numPreImportVertices = anumPreImportVertices;
		}
		struct Channel
		{
			Channel(unsigned anumVertices)
			{
				vertexBuffer = new RRIlluminationVertexBufferInMemory<RRColorRGBF>(anumVertices);
			}
			RRIlluminationVertexBuffer* vertexBuffer;
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
		~RRObjectIllumination()
		{
		}
	protected:
		unsigned numPreImportVertices; ///< PreImport number of vertices, length of vertex buffer for rendering.
		std::map<unsigned,Channel*> channels; ///< Calculated illumination.
	};

} // namespace

#endif
