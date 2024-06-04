//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Generic buffer code, shared by all implementations.
// --------------------------------------------------------------------------

#include <climits> // UINT_MAX
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRObject.h" // UnwrapSeams
#include "../squish/squish.h"
#include "../RRSolver/gather.h" // TexelFlags
#include "RRBufferInMemory.h"

// ImageCache
#include <unordered_map>
#ifdef _MSC_VER
	#define NOMINMAX
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif

#include <filesystem>
namespace bf = std::filesystem;

namespace rr
{

RRBuffer::RRBuffer()
{
	customData = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer interface

unsigned RRBuffer::getNumElements() const
{
	return getWidth()*getHeight()*getDepth();
}

void RRBuffer::setElement(unsigned index, const RRVec4& element, const RRColorSpace* colorSpace)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::setElement() called.\n"));
}

RRVec4 RRBuffer::getElement(unsigned index, const RRColorSpace* colorSpace) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElementAtPosition(const RRVec3& coord, const RRColorSpace* colorSpace, bool interpolated) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElementAtPosition() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElementAtDirection(const RRVec3& dir, const RRColorSpace* colorSpace) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElementAtDirection() called.\n"));
	return RRVec4(0);
}

unsigned char* RRBuffer::lock(RRBufferLock lock)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::lock() called.\n"));
	return nullptr;
}

void RRBuffer::unlock()
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::unlock() called.\n"));
}


//////////////////////////////////////////////////////////////////////////////
//
// RRBuffer tools for creation/copying

RRBuffer* RRBuffer::createCopy()
{
	unsigned char* data = lock(BL_READ);
	RRBuffer* copy = RRBuffer::create(getType(),getWidth(),getHeight(),getDepth(),getFormat(),getScaled(),data);
	unlock();
	return copy;
}

RRBuffer* RRBuffer::createCopy(RRBufferFormat _format, bool _scaled, const RRColorSpace* _colorSpace) const
{
	RRBuffer* copy = RRBuffer::create(getType(),getWidth(),getHeight(),getDepth(),_format,_scaled,nullptr);
	copyElementsTo(copy,_colorSpace);
	return copy;
}

bool RRBuffer::copyElementsTo(RRBuffer* destination, const RRColorSpace* colorSpace) const
{
	const RRBuffer* source = this; 
	if (!source || !destination)
	{
		RR_ASSERT(0); // invalid inputs
		return false;
	}
	unsigned w = destination->getWidth();
	unsigned h = destination->getHeight();
	unsigned d = destination->getDepth();
	if (source->getWidth()!=w || source->getHeight()!=h || source->getDepth()!=d)
	{
		RR_ASSERT(0); // invalid inputs
		return false;
	}
	unsigned size = w*h*d;
	const RRColorSpace* toCust = (!source->getScaled() && destination->getScaled()) ? colorSpace : nullptr;
	const RRColorSpace* toPhys = (source->getScaled() && !destination->getScaled()) ? colorSpace : nullptr;
	for (unsigned i=0;i<size;i++)
	{
		RRVec4 color = source->getElement(i,nullptr);
		if (toCust) toCust->fromLinear(color); else
		if (toPhys) toPhys->toLinear(color);
		destination->setElement(i,color,nullptr);
	}
	return true;
}

RRBuffer* RRBuffer::create(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, bool _scaled, const unsigned char* _data)
{
	RRBuffer* buffer = new RRBufferInMemory();
	if (!buffer->reset(_type,_width,_height,_depth,_format,_scaled,_data))
		RR_SAFE_DELETE(buffer);
	return buffer;
}

RRBuffer* RRBuffer::createSky(const RRVec4& upper, const RRVec4& lower, bool scaled)
{
	if (upper==lower)
	{
		const RRVec4 data[6] = {upper,upper,upper,upper,upper,upper};
		return create(BT_CUBE_TEXTURE,1,1,6,BF_RGBAF,scaled,(const unsigned char*)data);
	}
	else
	{
		const RRVec4 data[24] = {
			upper,upper,lower,lower,
			upper,upper,lower,lower,
			upper,upper,upper,upper,
			lower,lower,lower,lower,//bottom
			upper,upper,lower,lower,
			upper,upper,lower,lower
		};
		return create(BT_CUBE_TEXTURE,2,2,6,BF_RGBAF,scaled,(const unsigned char*)data);
	}
}

RRBuffer* RRBuffer::createEquirectangular()
{
	if (!this)
		return nullptr;
	switch (getType())
	{
		case BT_CUBE_TEXTURE:
			{
				unsigned width = getWidth()*3;
				unsigned height = getHeight()*2;
				RRBuffer* b = RRBuffer::create(BT_2D_TEXTURE,width,height,1,getFormat(),getScaled(),nullptr);
				for (unsigned j=0;j<height;j++)
					for (unsigned i=0;i<width;i++)
					{
						// [#55] rotated so that createEquirectangular() of empty scene with equirectangular environment E is E
						RRVec3 direction;
						direction.y = sin(RR_PI*((j+0.5f)/height-0.5f));
						direction.x = sin(RR_PI*(-2.0f*(i+0.5f)/width+1.5f)) * sqrt(1-direction.y*direction.y);
						direction.z = sqrt(1-direction.x*direction.x-direction.y*direction.y);
						if (i*2<width)
							direction.z = -direction.z;
						b->setElement(j*width+i,getElementAtDirection(direction,nullptr),nullptr);
					}
				return b;
			}
		case BT_2D_TEXTURE:
			return createReference();
		default: //BT_VERTEX_BUFFER
			return nullptr;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer tools

void RRBuffer::setFormat(RRBufferFormat newFormat)
{
	if (newFormat==getFormat())
	{
		return;
	}
	if (getFormat()==BF_DXT1 || getFormat()==BF_DXT3 || getFormat()==BF_DXT5)
	{
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),BF_RGBA,getScaled(),nullptr);
		int flags;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
			default:      flags = 0; break;
		};
		// compressed copy -> decompressed this
		squish::DecompressImage(lock(BL_DISCARD_AND_WRITE),getWidth(),getHeight(),copy->lock(BL_READ),flags);
		unlock();
		copy->unlock();
		delete copy;
		setFormat(newFormat);
	}
	else
	if (newFormat==BF_DXT1 || newFormat==BF_DXT3 || newFormat==BF_DXT5)
	{
		setFormat(BF_RGBA);
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),nullptr);
		int flags;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
			default:      flags = 0; break;
		};
		// uncompressed copy -> compressed this
		squish::CompressImage(copy->lock(BL_READ),getWidth(),getHeight(),lock(BL_DISCARD_AND_WRITE),flags);
		unlock();
		copy->unlock();
		delete copy;
	}
	else
	{
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),nullptr);
		unsigned numElements = getNumElements();
		for (unsigned i=0;i<numElements;i++)
		{
			setElement(i,copy->getElement(i,nullptr),nullptr);
		}
		delete copy;
	}
}

void RRBuffer::setFormatFloats()
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
			setFormat(BF_RGBF);
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
		case BF_RGBA:
			setFormat(BF_RGBAF);
			break;
		case BF_LUMINANCE:
			setFormat(BF_LUMINANCEF);
			break;
		case BF_DEPTH:
			// implementation defined
			break;
		case BF_RGBF:
		case BF_RGBAF:
		case BF_LUMINANCEF:
			// already floats
			break;
	}
}

void RRBuffer::clear(RRVec4 clearColor)
{
	unsigned numElements = getNumElements();
	for (unsigned i=0;i<numElements;i++)
	{
		setElement(i,clearColor,nullptr);
	}
}

void RRBuffer::invert()
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
		case BF_LUMINANCE:
		case BF_LUMINANCEF:
			{
				unsigned numElements = getNumElements();
				for (unsigned i=0;i<numElements;i++)
				{
					RRVec4 color = getElement(i,nullptr);
					color = RRVec4(1)-color;
					setElement(i,color,nullptr);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"invert() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::multiplyAdd(RRVec4 multiplier, RRVec4 addend)
{
	if (multiplier==RRVec4(1) && addend==RRVec4(0))
	{
		return;
	}
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
		case BF_LUMINANCE:
		case BF_LUMINANCEF:
			{
				unsigned numElements = getNumElements();
				for (unsigned i=0;i<numElements;i++)
				{
					setElement(i,getElement(i,nullptr)*multiplier+addend,nullptr);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"multiplyAdd() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::flip(bool flipX, bool flipY, bool flipZ)
{
	if (!flipX && !flipY && !flipZ)
	{
		return;
	}
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
		case BF_LUMINANCE:
		case BF_LUMINANCEF:
			{
				// slow getElement path, faster path can be written using lock and direct access
				unsigned xmax = getWidth();
				unsigned ymax = getHeight();
				unsigned zmax = getDepth();
				for (unsigned x=0;x<xmax;x++)
				for (unsigned y=0;y<ymax;y++)
				for (unsigned z=0;z<zmax;z++)
				{
					unsigned e1 = x+xmax*(y+ymax*z);
					unsigned e2 = (flipX?xmax-1-x:x)+xmax*((flipY?ymax-1-y:y)+ymax*(flipZ?zmax-1-z:z));
					if (e1<e2)
					{
						RRVec4 color1 = getElement(e1,nullptr);
						RRVec4 color2 = getElement(e2,nullptr);
						setElement(e1,color2,nullptr);
						setElement(e2,color1,nullptr);
					}
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"flip() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::rotate(int degrees, unsigned depthLayer)
{
	// not 0, 90, 180, 270...
	if ((degrees/90)*90!=degrees)
	{
		RR_ASSERT(0);
		return;
	}
	// 0
	if ((degrees/360)*360==degrees)
	{
		return;
	}
	// 90, 180, 270...
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
		case BF_LUMINANCE:
		case BF_LUMINANCEF:
			{
				// slow getElement path, faster path can be written using lock and direct access
				unsigned xmax = getWidth();
				unsigned ymax = getHeight();
				unsigned zmax = getDepth();
				unsigned offset = RR_CLAMPED(depthLayer,0,zmax-1)*xmax*ymax;
				if ((degrees/180)*180==degrees)
				{
					// 180
					// works like flip(true,true,false), but only for selected depthLayer
					for (unsigned x=0;x<xmax;x++)
					for (unsigned y=0;y<ymax;y++)
					{
						unsigned e1 = x+xmax*y;
						unsigned e2 = xmax-1-x+xmax*(ymax-1-y);
						if (e1<e2)
						{
							RRVec4 color1 = getElement(e1+offset,nullptr);
							RRVec4 color2 = getElement(e2+offset,nullptr);
							setElement(e1+offset,color2,nullptr);
							setElement(e2+offset,color1,nullptr);
						}
					}
				}
				else
				{
					// 90, 270
					unsigned direction = ((degrees/90)%4)+4;
					for (unsigned x=0;x<xmax;x++)
					for (unsigned y=0;y<ymax;y++)
					{
						unsigned e[4] =
						{
							x+xmax*y,
							ymax-1-y+xmax*x,
							xmax-1-x+xmax*(ymax-1-y),
							y+xmax*(xmax-1-x)
						};
						if (e[0]<e[1] && e[0]<e[2] && e[0]<e[3])
						{
							RRVec4 color[4] =
							{
								getElement(e[0]+offset,nullptr),
								getElement(e[1]+offset,nullptr),
								getElement(e[2]+offset,nullptr),
								getElement(e[3]+offset,nullptr)
							};
							for (unsigned i=0;i<4;i++)
								setElement(e[i]+offset,color[(i+direction)%4],nullptr);
						}
					}
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"rotate() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::brightnessGamma(RRVec4 brightness, RRVec4 gamma)
{
	if (brightness==RRVec4(1) && gamma==RRVec4(1))
	{
		return;
	}
	// slow getElement path, faster path can be written using lock and direct access
	int numElements = getNumElements();
	#pragma omp parallel for schedule(static) if(numElements>RR_OMP_MIN_ELEMENTS)
	for (int i=0;i<numElements;i++)
	{
		RRVec4 element = getElement(i,nullptr);
		for (unsigned j=0;j<4;j++)
			element[j] = pow(element[j]*brightness[j],gamma[j]);
		setElement(i,element,nullptr);
	}
}

void RRBuffer::getMinMax(RRVec4* _mini, RRVec4* _maxi)
{
	// slow getElement path, faster path can be written using lock and direct access
	unsigned numElements = getNumElements();
	RRVec4 mini(1e20f);
	RRVec4 maxi(-1e20f);
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i,nullptr);
		for (unsigned j=0;j<4;j++)
		{
			mini[j] = RR_MIN(mini[j],color[j]);
			maxi[j] = RR_MAX(maxi[j],color[j]);
		}
	}
	if (_mini) *_mini = mini;
	if (_maxi) *_maxi = maxi;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRBuffer tools for lightmaps

struct UnwrapSeam
{
	RRVec2 edge1[2]; // triangle 1 edge coordinates in texture space
	RRVec2 edge2[2]; // triangle 2 edge coordinates in texture space
};

class UnwrapSeams : public std::vector<UnwrapSeam>
{
public:
	UnwrapSeams(const RRObject* object)
	{
		// check that we have object, mesh, unwrap...
		if (!object)
			return;
		if (!object->faceGroups.size())
			return;
		RRMaterial* material = object->faceGroups[0].material;
		if (!material)
			return;
		const RRMesh* mesh0 = object->getCollider()->getMesh();
		if (!mesh0)
			return;
		{
			RRMesh::TriangleMapping tm;
			if (!mesh0->getTriangleMapping(0,tm,material->lightmap.texcoord))
				return;
		}

		// stitch vertices with the same position + nearly the same normal (ignore unwrap differences)
		// this makes looking for shared edges easier
		const RRMesh* mesh = mesh0->createOptimizedVertices(0,RR_DEG2RAD(1),0,nullptr);
		if (!mesh)
		{
			RR_LIMITED_TIMES(10,RRReporter::report(WARN,"Unwrap seams won't be filtered.\n"));
			return;
		}

		// find triangles in vertices
		unsigned numVertices = mesh->getNumVertices();
		unsigned numTriangles = mesh->getNumTriangles();
		std::vector<std::vector<unsigned> > trianglesInVertex;
		{
			trianglesInVertex.resize(numVertices);
			for (unsigned t=0;t<numTriangles;t++)
			{
				RRMesh::Triangle tri;
				mesh->getTriangle(t,tri);
				for (unsigned v=0;v<3;v++)
					trianglesInVertex[tri[v]].push_back(t);
			}
		}

		// find v1-v2 edges shared by t1,t2 triangles, where both triangles have different unwrap
		// t=triangle v=vertex tv=triangle_vertices tm=triangle_mapping tvi==triangle_vertices_index(0,1,2)
		for (unsigned t1=0;t1<numTriangles;t1++)
		{
			RRMesh::Triangle tv1;
			mesh->getTriangle(t1,tv1);
			RRMesh::TriangleMapping tm1;
			mesh->getTriangleMapping(t1,tm1,material->lightmap.texcoord);
			// for all 3 edges of t1
			for (unsigned e=0;e<3;e++)
			{
				// edge v1-v2
				unsigned v1 = tv1[e];
				unsigned v2 = tv1[(e+1)%3];
				// get first edge coords in texture space
				UnwrapSeam seam;
				seam.edge1[0] = tm1.uv[e];
				seam.edge1[1] = tm1.uv[(e+1)%3];
				// is there triangle t2 sharing edge v1-v2?
				unsigned i1=0;
				for (unsigned i2=0;i2<trianglesInVertex[v2].size();i2++)
					if (trianglesInVertex[v2][i2]>t1) // t2>t1 ensures that edge is not processed twice
					{
						while (i1+1<trianglesInVertex[v1].size() && trianglesInVertex[v1][i1]<trianglesInVertex[v2][i2]) i1++;
						if (trianglesInVertex[v1][i1]==trianglesInVertex[v2][i2])
						{
							// we found t2
							unsigned t2 = trianglesInVertex[v1][i1];
							// get second edge coords in texture space
							RRMesh::Triangle tv2;
							mesh->getTriangle(t2,tv2);
							RRMesh::TriangleMapping tm2;
							mesh->getTriangleMapping(t2,tm2,material->lightmap.texcoord);
							seam.edge2[0] = tm2.uv[(tv2[0]==tv1[e])?0:((tv2[1]==tv1[e])?1:2)];
							seam.edge2[1] = tm2.uv[(tv2[0]==tv1[(e+1)%3])?0:((tv2[1]==tv1[(e+1)%3])?1:2)];
							// do edge coords differ?
							if (seam.edge1[0]!=seam.edge2[0] || seam.edge1[1]!=seam.edge2[1])
							{
								// we found a seam, save it
								push_back(seam);
							}
						}
					}
			}
		}
		if (mesh!=mesh0)
			delete mesh;
	}
};

bool RRBuffer::lightmapSmooth(float _sigma, bool _wrap, const RRObject* _object)
{
	const bool emphasizeUnwrapSeams = false;
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (_sigma<0)
		return false;
	if (getType()!=BT_2D_TEXTURE)
		return false;
	if (_sigma==0)
		return true;

	// copy image from buffer to temp
	unsigned width = getWidth();
	unsigned height = getHeight();
	unsigned size = width*height;
	void* buf;
	buf = malloc(size*(sizeof(RRVec4)*2+1));
	if (!buf)
	{
		RR_LIMITED_TIMES(10,RRReporter::report(WARN,"Allocation of %s failed in lightmapSmooth().\n",RRReporter::bytesToString(size*(sizeof(RRVec4)*2+1))));
		return false;
	}
	RRVec4* source = (RRVec4*)buf;
	RRVec4* destination = source+size;
	for (unsigned i=0;i<size;i++)
	{
		source[i] = getElement(i,nullptr);
	}

	// fill blurFlags, what neighbors to blur with
	// 8bits per texel = should it blur with 8 neighbors?
	// bits:
	//  ^ j
	//  |
	//  5 6 7
	//  3   4
	//  0 1 2  --> i
	unsigned char* blurFlags = (unsigned char*)(destination+size);
	for (unsigned j=0;j<height;j++)
	for (unsigned i=0;i<width;i++)
	{
		unsigned k = i+j*width;
		unsigned centerFlags = FLOAT_TO_TEXELFLAGS(source[k][3]);
		unsigned blurBit = 1;
		unsigned blurFlagsK = 0;
		for (int jj=-1;jj<2;jj++)
		for (int ii=-1;ii<2;ii++)
			if (ii||jj)
			{
				// calculate neighbor coordinates x,y
				int x = i+ii;
				int y = j+jj;
				if (_wrap)
				{
					x = (x+width)%width;
					y = (y+height)%height;
				}
				// if in range, set good neighbor flags
				if (x>=0 && x<(int)width && y>=0 && y<(int)height)
				{
					unsigned neighborFlags = FLOAT_TO_TEXELFLAGS(source[x+y*width][3]);
					if (1
						&& (ii>=0 || (centerFlags&EDGE_X0 && neighborFlags&EDGE_X1))
						&& (ii<=0 || (centerFlags&EDGE_X1 && neighborFlags&EDGE_X0))
						&& (jj>=0 || (centerFlags&EDGE_Y0 && neighborFlags&EDGE_Y1))
						&& (jj<=0 || (centerFlags&EDGE_Y1 && neighborFlags&EDGE_Y0))
						)
						blurFlagsK |= blurBit;
				}
				blurBit *= 2;
			}
		blurFlags[k] = blurFlagsK;
	}

	// alpha=1 in temp
	for (unsigned i=0;i<size;i++)
	{
		source[i][3] = (source[i][3]>0) ? 1.f : 0.f;
	}

	// prepare for smoothing unwrap seams
	UnwrapSeams seams(_object);

	// blur temp
	if (_sigma>10)
		_sigma = 10;
	float sigma2sum = _sigma*_sigma;
	while (sigma2sum>0)
	{
		// calculate amount of blur for this pass
		float sigma2 = RR_MIN(sigma2sum,0.34f);
		RRVec4 kernel(1,expf(-0.5f/sigma2),expf(-1/sigma2),expf(-2/sigma2));
		sigma2sum -= sigma2;

		// dst=blur(src)
		#pragma omp parallel for schedule(static) if(size>RR_OMP_MIN_ELEMENTS)
		for (int j=0;j<(int)height;j++)
		for (int i=0;i<(int)width;i++)
		{
			unsigned k = i+j*width;
			RRVec4 c = source[k];
			if (c[3]!=0)
			{
				unsigned flags = blurFlags[k];
				c = c * kernel[0]
					+ source[((i-1+width)%width)+((j-1+height)%height)*width] * ((flags&1  )?kernel[2]:0)
					+ source[( i               )+((j-1+height)%height)*width] * ((flags&2  )?kernel[1]:0)
					+ source[((i+1      )%width)+((j-1+height)%height)*width] * ((flags&4  )?kernel[2]:0)
					+ source[((i-1+width)%width)+( j                 )*width] * ((flags&8  )?kernel[1]:0)
					+ source[((i+1      )%width)+( j                 )*width] * ((flags&16 )?kernel[1]:0)
					+ source[((i-1+width)%width)+((j+1       )%height)*width] * ((flags&32 )?kernel[2]:0)
					+ source[( i               )+((j+1       )%height)*width] * ((flags&64 )?kernel[1]:0)
					+ source[((i+1      )%width)+((j+1       )%height)*width] * ((flags&128)?kernel[2]:0)
					;
				c /= c[3];
			}
			destination[k] = c;
		}

		// smooth seams
		for (unsigned s=0;s<seams.size();s++)
		{
			const UnwrapSeam& seam = seams[s];
			unsigned lasti1=UINT_MAX,lasti2=UINT_MAX;
			RRVec4 accu1,accu2;
			for (unsigned i=0;i<=1000;i++)
			{
				RRVec2 uv1 = seam.edge1[0]*(1-i*0.001f)+seam.edge1[1]*(i*0.001f);
				RRVec2 uv2 = seam.edge2[0]*(1-i*0.001f)+seam.edge2[1]*(i*0.001f);
				if (uv1[0]>=0 && uv1[0]<1 && uv1[1]>=0 && uv1[1]<1)
				if (uv2[0]>=0 && uv2[0]<1 && uv2[1]>=0 && uv2[1]<1)
				{
					unsigned i1 = (unsigned)(uv1[0]*width) + (unsigned)(uv1[1]*height)*width;
					unsigned i2 = (unsigned)(uv2[0]*width) + (unsigned)(uv2[1]*height)*width;
					if (destination[i1][3] && destination[i2][3]) // don't touch unused texels. all edge texels should already be used, but we could fall outside due to rounding errors
					{
						if (i1!=lasti1)
						{
							if (lasti1!=UINT_MAX)
								destination[lasti1] = emphasizeUnwrapSeams ? RRVec3(1,0,0) : accu1/accu1[3];
							lasti1 = i1;
							accu1 = RRVec4(0);
						}
						if (i2!=lasti2)
						{
							if (lasti2!=UINT_MAX)
								destination[lasti2] = emphasizeUnwrapSeams ? RRVec3(0,0,1) :  accu2/accu2[3];
							lasti2 = i2;
							accu2 = RRVec4(0);
						}
						accu1 += destination[i1]+destination[i2];
						accu2 += destination[i1]+destination[i2];
					}
				}
			}
			if (lasti1!=UINT_MAX)
				destination[lasti1] = emphasizeUnwrapSeams ? RRVec3(1,0,0) : accu1/accu1[3];
			if (lasti2!=UINT_MAX)
				destination[lasti2] = emphasizeUnwrapSeams ? RRVec3(0,0,1) : accu2/accu2[3];
		}

		// src=dst
		RRVec4* temp = source;
		source = destination;
		destination = temp;
	}

	// copy temp back to buffer, preserve alpha
	for (unsigned i=0;i<size;i++)
		if (source[i][3]>0)
			setElement(i,RRVec4(source[i][0],source[i][1],source[i][2],getElement(i,nullptr)[3]),nullptr);
	free(buf);
	return true;
}

bool RRBuffer::lightmapGrowForBilinearInterpolation(bool _wrap)
{
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (getType()!=BT_2D_TEXTURE)
		return false;
	bool notEmpty = false;
	unsigned width = getWidth();
	unsigned height = getHeight();
	for (unsigned j=0;j<height;j++)
	for (unsigned i=0;i<width;i++)
	{
		// we are processing texel i,j
		if (getElement(i+j*width,nullptr)[3]>0.002f)
		{
			// not empty, keep it unchanged
			notEmpty = true;
		}
		else
		{
			// empty, change it to average of good neighbors
			RRVec4 sum(0);
			for (int jj=-1;jj<2;jj++)
			for (int ii=-1;ii<2;ii++)
			{
				// calculate neighbor coordinates x,y
				int x = i+ii;
				int y = j+jj;
				if (_wrap)
				{
					x = (x+width)%width;
					y = (y+height)%height;
				}
				else
				{
					if (x<0 || x>=(int)width || y<0 || y>=(int)height)
						continue;
				}
				// read neighbor
				RRVec4 c = getElement(x+y*width,nullptr);
				unsigned texelFlags = FLOAT_TO_TEXELFLAGS(c[3]);
				// is it good one?
				if (0
					|| (ii<+1 && jj<+1 && (texelFlags&QUADRANT_X1Y1))
					|| (ii>-1 && jj>-1 && (texelFlags&QUADRANT_X0Y0))
					|| (ii<+1 && jj>-1 && (texelFlags&QUADRANT_X1Y0))
					|| (ii>-1 && jj<+1 && (texelFlags&QUADRANT_X0Y1))
					)
				{
					// sum it
					float weight = (ii && jj)?1:1.6f;
					sum += RRVec4(c[0],c[1],c[2],1)*weight;
				}
			}
			if (sum[3])
				setElement(i+j*width,RRVec4(sum[0]/sum[3],sum[1]/sum[3],sum[2]/sum[3],
					0.001f // small enough to be invisible for texelFlags, but big enough to be >0, to prevent growForeground() and fillBackground() from overwriting this texel
					),nullptr);
		}
	}
	return notEmpty;
}

bool RRBuffer::lightmapGrow(unsigned _numSteps, bool _wrap, bool& _aborting)
{
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (getType()!=BT_2D_TEXTURE)
		return false;
	if (_numSteps==0)
		return true;
	if (getFormat()==BF_DXT1 || getFormat()==BF_DXT3 || getFormat()==BF_DXT5)
	{
		// can't manipulate compressed buffer
		return false;
	}
	if (getFormat()!=BF_RGBA && getFormat()!=BF_RGBAF)
	{
		// no alpha -> everything is foreground -> no space to grow
		return true;
	}

	// copy image from buffer to temp
	unsigned width = getWidth();
	unsigned height = getHeight();
	unsigned size = width*height;
	RRVec4* buf;
	buf = new (std::nothrow) RRVec4[size*2];
	if (!buf)
	{
		RR_LIMITED_TIMES(10,RRReporter::report(WARN,"Allocation of %s failed in lightmapGrow().\n",RRReporter::bytesToString(size*2*sizeof(RRVec4))));
		return false;
	}
	RRVec4* source = buf;
	RRVec4* destination = buf+size;
	for (unsigned i=0;i<size;i++)
	{
		RRVec4 c = getElement(i,nullptr);
		source[i] = c[3]>0 ? RRVec4(c[0],c[1],c[2],1) : RRVec4(0);
	}

	// grow temp
	while (!_aborting && _numSteps--)
	{
		bool changed = false;
		if (_wrap)
		{
			// faster version with wrap
			#pragma omp parallel for schedule(static) if(size>RR_OMP_MIN_ELEMENTS)
			for (int i=0;i<(int)size;i++)
			{
				RRVec4 c = source[i];
				if (c[3]==0)
				{
					RRVec4 d = (source[(i+1)%size] + source[(i-1)%size] + source[(i+width)%size] + source[(i-width)%size]) * 1.6f
						+ (source[(i+1+width)%size] + source[(i+1-width)%size] + source[(i-1+width)%size] + source[(i-1-width)%size]);
					if (d[3]!=0)
					{
						c = d/d[3];
						changed = true;
					}
				}
				destination[i] = c;
			}
		}
		else
		{
			// slower version without wrap
			#pragma omp parallel for schedule(static) if(size>RR_OMP_MIN_ELEMENTS)
			for (int i=0;i<(int)size;i++)
			{
				RRVec4 c = source[i];
				if (c[3]==0)
				{
					int x = i%width;
					int y = i/width;
					int w = (int)width;
					int h = (int)height;
					RRVec4 d = (source[RR_MIN(x+1,w-1) + y*w] + source[RR_MAX(x-1,0) + y*w] + source[x + RR_MIN(y+1,h-1)*w] + source[x + RR_MAX(y-1,0)*w]) * 1.6f
						+ (source[RR_MIN(x+1,w-1) + RR_MIN(y+1,h-1)*w] + source[RR_MIN(x+1,w-1) + RR_MAX(y-1,0)*w] + source[RR_MAX(x-1,0) + RR_MIN(y+1,h-1)*w] + source[RR_MAX(x-1,0) + RR_MAX(y-1,0)*w]);
					if (d[3]!=0)
					{
						c = d/d[3];
						changed = true;
					}
				}
				destination[i] = c;
			}
		}
		if (!changed)
			_numSteps = 0; // stop growing if it has no effect
		RRVec4* temp = source;
		source = destination;
		destination = temp;
	}

	// copy temp back to buffer
	for (unsigned i=0;i<size;i++)
		if (source[i][3]>0 && getElement(i,nullptr)[3]==0)
			setElement(i,RRVec4(source[i][0],source[i][1],source[i][2],0.001f),nullptr);
	delete[] buf;
	return true;
}

bool RRBuffer::lightmapFillBackground(RRVec4 backgroundColor)
{
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (getType()!=BT_2D_TEXTURE)
		return false;
	unsigned numElements = getNumElements();
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i,nullptr);
		setElement(i,(color[3]<=0)?backgroundColor:color,nullptr);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageCache

extern RRBuffer* load_noncached(const RRString& _filename, const char* _cubeSideName[6]);


class ImageCache
{
public:
	RRBuffer* load_cached(const RRString& filename, const char* cubeSideName[6])
	{
		bool sixfiles = wcsstr(filename.w_str(),L"%s")!=nullptr;
		std::error_code ec;
		bool exists = !sixfiles && bf::exists(RR_RR2PATH(filename),ec);

		Cache::iterator i = cache.find(RR_RR2STDW(filename));
		if (i!=cache.end())
		{
			// image was found in cache
			if (i->second.buffer // we try again if previous load failed, perhaps file was created on background
				&& (i->second.buffer->getDuration() // always take videos from cache
					|| i->second.buffer->version==i->second.bufferVersionWhenLoaded) // take static content from cache only if version did not change
				&& (sixfiles
					|| !exists // for example c@pture is virtual file, it does not exist on disk, but still we cache it
					|| (i->second.fileTimeWhenLoaded==bf::last_write_time(filename.w_str(),ec) && i->second.fileSizeWhenLoaded==bf::file_size(filename.w_str(),ec))
					)
				)
			{
				// detect and report possible error
				bool cached2dCross = i->second.buffer->getType()==BT_2D_TEXTURE && (i->second.buffer->getWidth()*3==i->second.buffer->getHeight()*4 || i->second.buffer->getWidth()*4==i->second.buffer->getHeight()*3);
				bool cachedCube = i->second.buffer->getType()==BT_CUBE_TEXTURE;
				if ((cached2dCross && cubeSideName)
					|| (cachedCube && !cubeSideName && bf::path(RR_RR2PATH(filename)).extension()!=".rrbuffer")) // .rrbuffer is the only format that can produce cube even with cubeSideName=nullptr, exclude it from test here
					RRReporter::report(WARN,"You broke image cache by loading %ls as both 2d and cube.\n",filename.w_str());

				// image is already in memory and it was not modified since load, use it
				return i->second.buffer->createReference(); // add one ref for user
			}
			// modified (in memory or on disk) after load, delete it from cache, we can't use it anymore
			deleteFromCache(i);
		}
		// load new file into cache
		Value& value = cache[RR_RR2STDW(filename)];
		value.buffer = load_noncached(filename,cubeSideName);
		if (value.buffer)
		{
			value.buffer->createReference(); // keep initial ref for us, add one ref for user
			value.bufferVersionWhenLoaded = value.buffer->version;
			value.fileTimeWhenLoaded = exists ? bf::last_write_time(filename.w_str(),ec) : bf::file_time_type::min();
			value.fileSizeWhenLoaded = exists ? bf::file_size(filename.w_str(),ec) : 0;
		}
		return value.buffer;
	}
	size_t getMemoryOccupied()
	{
		size_t memoryOccupied = 0;
		for (Cache::iterator i=cache.begin();i!=cache.end();i++)
		{
			if (i->second.buffer)
				memoryOccupied += i->second.buffer->getBufferBytes();
		}
		return memoryOccupied;
	}
	void deleteFromCache(RRBuffer* b)
	{
		if (b)
			deleteFromCache(cache.find(RR_RR2STDW(b->filename)));
	}
	~ImageCache()
	{
		while (cache.begin()!=cache.end())
		{
#ifdef _DEBUG
			// If users deleted their buffers, refcount should be down at 1 and this delete is final
			// Don't report in release, some samples knowingly leak, to make code simpler
			RRBuffer* b = cache.begin()->second.buffer;
			if (b && b->getReferenceCount()!=1)
				RRReporter::report(WARN,"Memory leak, image %ls not deleted (%dx).\n",b->filename.w_str(),b->getReferenceCount()-1);
#endif
			deleteFromCache(cache.begin());
		}
	}
protected:
	struct Value
	{
		RRBuffer* buffer;
		unsigned bufferVersionWhenLoaded;
		// attributes critical for Toolbench plugin, "Bake from cache" must not load images from cache if they did change on disk.
		bf::file_time_type fileTimeWhenLoaded;
		std::uintmax_t fileSizeWhenLoaded;
	};
	typedef std::unordered_map<std::wstring,Value> Cache;
	Cache cache;

	void deleteFromCache(Cache::iterator i)
	{
		if (i!=cache.end())
		{
			RRBuffer* b = i->second.buffer;
			cache.erase(i);
			// delete calls deleteFromCache(), but we have just erased it from cache, so it won't find it, it won't delete it again
			delete b;
		}
	}
};

// Single cache works better than individual cache instances in scenes,
// especially if user loads many models that share textures.
ImageCache s_imageCache;

RRBuffer* load_cached(const RRString& filename, const char* cubeSideName[6])
{
#ifdef _MSC_VER
	__try
	{
#endif
		return s_imageCache.load_cached(filename,cubeSideName);
#ifdef _MSC_VER
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"RRBuffer import crashed.\n"));
		return nullptr;
	}
#endif
}

void RRBuffer::deleteFromCache()
{
	s_imageCache.deleteFromCache(this);
}


/////////////////////////////////////////////////////////////////////////////
//
// save & load

static RRBuffer* loadStub(const RRString& _filename, const RRString& _stubname)
{
	if (!_stubname.empty())
	{
		// ":" is a name of file that definitely does not exist. when used for stub, don't even try loading, don't warn
		RRBuffer* stub = (_stubname==":") ? nullptr : load_cached(_stubname,nullptr);
		RRBuffer* result;
		if (stub)
		{
			// by returning copy, we preserve stub intact in cache, next time we won't have to load it from disk
			result = stub->createCopy();
			delete stub;
		}
		else
		{
			unsigned char data[3*16*16];
			for (unsigned i=0;i<16;i++)
			for (unsigned j=0;j<16;j++)
				data[3*(i+16*j)+2] = data[3*(i+16*j)+1] = data[3*(i+16*j)] = (((i/2^j/2)%2)?0:255);
			result = RRBuffer::create(BT_2D_TEXTURE,16,16,1,BF_RGB,true,(unsigned char*)data);
		}
		result->filename = _filename; // [#36] stub contains original filename (of file that was not located)
		RRBufferInMemory* bufferInMemory = dynamic_cast<RRBufferInMemory*>(result);
		if (bufferInMemory)
			bufferInMemory->stub = true;
		return result;
	}
	// load with fileLocator failed, and there's no stub
	// load_noncached ensures that all failures were already reported
	//RRReporter::report(WARN,"Failed to find or load %ls.\n",_filename.w_str());
	return nullptr;
}

RRBuffer* RRBuffer::load(const RRString& _filename, const char* _cubeSideName[6], const RRFileLocator* _fileLocator)
{
	if (_filename.empty())
	{
		return nullptr;
	}

	if (_fileLocator)
	{
		// locate only
		RRString stubname = _fileLocator->getLocation(_filename,RRFileLocator::ATTEMPT_LOCATE_ONLY); // note: search probably fails if filename contains %s
		if (!stubname.empty())
		{
			RRString locatedFilename = _fileLocator->getLocation(_filename, "");
			if (!locatedFilename.empty())
			{
				RRBuffer* result = loadStub(_filename,stubname);
				if (result)
					result->filename = locatedFilename;
				return result;
			}
		}
		else
		// load with fileLocator
		for (unsigned attempt=0;attempt<UINT_MAX;attempt++)
		{
			RRString location = _fileLocator->getLocation(_filename,attempt);
			if (location.empty())
				break;
			std::wstring location_buf = RR_RR2STDW(location);
			int ofs = (int)location_buf.find(L"%s");
			if (ofs>=0)
			{
				if (_cubeSideName && _cubeSideName[0])
					location_buf.replace(ofs,2,RRString(_cubeSideName[0]).w_str());
				else
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Texture filename %ls contains %%s, but cubeSideNames is nullptr.\n",location.w_str()));
			}
			bool exists = _fileLocator->exists(RR_STDW2RR(location_buf));
			RRReporter::report(INF3,"%d%c %ls\n",attempt,exists?'+':'-',location.w_str());
			if (exists)
			{
				RRBuffer* result = load_cached(location,_cubeSideName);
				if (result)
					return result;
			}
		}
		// we don't know whether file is missing (this is the most common case, and it was not yet reported)
		// or file failed to load (it was already reported from load_cached())
		// better report twice in rare cases than not report in common case
		RRReporter::report(WARN,"%ls not loaded.\n",_filename.w_str());
		// not found or load failed, return stub
		stubname = _fileLocator->getLocation(_filename,RRFileLocator::ATTEMPT_STUB);
		return loadStub(_filename,stubname);
	}
	else
	{
		RRBuffer* result = load_cached(_filename,_cubeSideName);
		RRReporter::report(INF3,"@%c %ls\n",result?'+':'-',_filename.w_str());
		if (!result)
		{
			// load_noncached ensures that all failures were already reported
			//RRReporter::report(WARN,"Failed to load %ls.\n",_filename.w_str());
		}
		return result;
	}
}

bool RRBuffer::reload(const RRString& _filename, const char* _cubeSideName[6], const RRFileLocator* _fileLocator)
{
	if (_filename.empty())
	{
		return false;
	}
	if (!this)
	{
		RRReporter::report(WARN,"Attempted nullptr->reload().\n");
		return false;
	}

	// load into new buffer
	RRBuffer* loaded = load(_filename,_cubeSideName,_fileLocator);
	if (!loaded) return false;

	if (getDuration() || loaded->getDuration())
		RRReporter::report(WARN,"Default reload() does not support video/animation.\n");

	// reload into existing buffer
	const unsigned char* loadedData = loaded->lock(BL_READ);
	reset(loaded->getType(),loaded->getWidth(),loaded->getHeight(),loaded->getDepth(),loaded->getFormat(),loaded->getScaled(),loadedData);
	filename = loaded->filename; // [#36] reload() just tmp->load()s and then copies all, including filename
	if (dynamic_cast<RRBufferInMemory*>(this))
		dynamic_cast<RRBufferInMemory*>(this)->stub = loaded->isStub();
	if (loadedData)
	{
		loaded->unlock();
	}
	else
	{
		unsigned pixels = loaded->getNumElements();
		for (unsigned i=0;i<pixels;i++)
			setElement(i,loaded->getElement(i,nullptr),nullptr);
	}
	delete loaded;
	return true;
}

// sideeffect: plants %s into _filename
static const char** selectCubeSideNames(std::wstring& _filename)
{
	const unsigned numConventions = 3;
	static const char* cubeSideNames[numConventions][6] =
	{
		{"bk","ft","up","dn","rt","lf"}, // Quake
		{"BK","FT","UP","DN","RT","LF"}, // Quake
		{"negative_x","positive_x","positive_y","negative_y","positive_z","negative_z"} // codemonsters.de
	};
	// inserts %s if it is one of 6 images
	for (unsigned c=0;c<numConventions;c++)
	{
		for (unsigned s=0;s<6;s++)
		{
			RRString suffix = cubeSideNames[c][s];
			size_t suffixLen = strlen(cubeSideNames[c][s]);
			size_t suffixOfs = _filename.size()-4-suffixLen;
			if (suffixOfs>=0 && suffixOfs<1000 && _filename.substr(suffixOfs,suffixLen)==suffix.w_str())
			{
				_filename.replace(suffixOfs,suffixLen,L"%s");
				return cubeSideNames[c];
			}
		}
	}
	return cubeSideNames[0];
}

RRBuffer* RRBuffer::loadCube(const RRString& _filename, const RRFileLocator* _fileLocator)
{
	if (_filename.empty())
	{
		return nullptr;
	}
	std::wstring filename = RR_RR2STDW(_filename);
	const char** cubeSideNames = selectCubeSideNames(filename);
	return load(RR_STDW2RR(filename),cubeSideNames,_fileLocator);
}

}; // namespace
