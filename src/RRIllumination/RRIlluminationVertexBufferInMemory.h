#include "RRIllumination.h"

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
			vertices = NULL;
			RRIlluminationVertexBufferInMemory::setSize(anumVertices);
		}
		virtual void setSize(unsigned anumVertices)
		{
			delete[] vertices;
			numVertices = anumVertices;
			vertices = new Color[numVertices];
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
