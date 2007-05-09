#include <cstdio>
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRIllumination.h"

namespace rr
{

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
			numVertices = anumVertices;
			vertices = new Color[numVertices];
		}
		virtual unsigned getNumVertices() const
		{
			return numVertices;
		}
		virtual void setVertex(unsigned vertex, const RRColorRGBF& color)
		{
			if(!vertices)
			{
				RR_ASSERT(0);
				return;
			}
			if(vertex>=numVertices)
			{
				RR_ASSERT(0);
				return;
			}
			vertices[vertex] = color;
		}
		bool load(const char* filename)
		{
			FILE* f = fopen(filename,"rb");
			if(!f)
				return false;
			unsigned read = (unsigned)fread(vertices,sizeof(Color),numVertices,f);
			fclose(f);
			return read == numVertices;
		}
		virtual bool save(const char* filename)
		{
			FILE* f = fopen(filename,"wb");
			if(!f)
				return false;
			unsigned written = (unsigned)fwrite(vertices,sizeof(Color),numVertices,f);
			fclose(f);
			return written == numVertices;
		}
		virtual ~RRIlluminationVertexBufferInMemory()
		{
			delete[] vertices;
		}
	protected:
		unsigned numVertices;
		Color* vertices;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Lockable illumination storage in vertex buffer in system memory.
	//
	//! Has fixed RRColorRGBF format.
	//! It is 3 floats per vertex.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationVertexBufferRGBFInMemory : public RRIlluminationVertexBufferInMemory<RRColorRGBF>
	{
	public:
		RRIlluminationVertexBufferRGBFInMemory(unsigned anumVertices)
			: RRIlluminationVertexBufferInMemory<RRColorRGBF>(anumVertices)
		{
		}
		virtual const RRColorRGBF* lock()
		{
			return vertices;
		}
	};

} // namespace
