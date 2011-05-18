// --------------------------------------------------------------------------
// LightsprintCore fragments that need exceptions enabled.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// RRMesh
#include "Lightsprint/RRMesh.h"
#include "../RRMathPrivate.h"
#include <boost/unordered_map.hpp>

// ImageCache
#include "Lightsprint/RRBuffer.h"
#include <string>
//#include <boost/unordered_map.hpp>

// FaceGroups
#include "Lightsprint/RRObject.h"
#include <set>

// RRObjects
//#include "Lightsprint/RRObject.h"
#include <boost/unordered_set.hpp>
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

// calculates triangle area from triangle vertices
static RRReal calculateArea(RRVec3 v0, RRVec3 v1, RRVec3 v2)
{
	RRReal a = size2(v1-v0);
	RRReal b = size2(v2-v0);
	RRReal c = size2(v2-v1);
	//return sqrt(2*b*c+2*c*a+2*a*b-a*a-b*b-c*c)/4;
	RRReal d = 2*b*c+2*c*a+2*a*b-a*a-b*b-c*c;
	return (d>0) ? sqrt(d)*0.25f : 0; // protection against evil rounding error
}

// this function logically belongs to RRMesh.cpp,
// we put it here where exceptions are enabled
// enabling exceptions in RRMesh.cpp would cause slow down
RRReal RRMesh::findGroundLevel() const
{
	unsigned numTriangles = getNumTriangles();
	if (numTriangles)
	{
		typedef boost::unordered_map<RRReal,RRReal> YToArea;
		YToArea yToArea;
		for (unsigned t=0;t<numTriangles;t++)
		{
			RRMesh::TriangleBody tb;
			getTriangleBody(t,tb);
			if (tb.side1.y==0 && tb.side2.y==0 && orthogonalTo(tb.side1,tb.side2).y>0) // planar and facing up
			{
				RRReal area = calculateArea(tb.side1,tb.side2,tb.side2-tb.side1);
				YToArea::iterator i = yToArea.find(tb.vertex0.y);
				if (i==yToArea.end())
					yToArea[tb.vertex0.y]=area;
				else
					yToArea[tb.vertex0.y]+=area;
			}
		}
		YToArea::iterator best = yToArea.begin();
		for (YToArea::iterator i=yToArea.begin();i!=yToArea.end();i++)
		{
			if (i->second>best->second) best = i;
		}
		return best->first;
	}
	else
	{
		return 0;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// ImageCache

extern RRBuffer* load_noncached(const RRString& filename, const char* cubeSideName[6]);


class ImageCache
{
public:
	RRBuffer* load_cached(const RRString& filename, const char* cubeSideName[6])
	{
		Cache::iterator i = cache.find(RR_RR2STDW(filename));
		if (i!=cache.end())
		{
			// image was found in cache
			if (i->second.buffer // we try again if previous load failed, perhaps file was created on background
				&& (i->second.buffer->getDuration() // always take videos from cache
					|| i->second.buffer->version==i->second.bufferVersionWhenLoaded) // take static content from cache only if version did not change
				&& i->second.fileTimeWhenLoaded==bf::last_write_time(filename.w_str())
				)
			{
				// detect and report possible error
				bool cached2dCross = i->second.buffer->getType()==BT_2D_TEXTURE && (i->second.buffer->getWidth()*3==i->second.buffer->getHeight()*4 || i->second.buffer->getWidth()*4==i->second.buffer->getHeight()*3);
				bool cachedCube = i->second.buffer->getType()==BT_CUBE_TEXTURE;
				if ((cached2dCross && cubeSideName) || (cachedCube && !cubeSideName))
					RRReporter::report(WARN,"You broke image cache by loading %ls as both 2d and cube.\n",filename.w_str());

				// image is already in memory and it was not modified since load, use it
				return i->second.buffer->createReference(); // add one ref for user
			}
			// modified (in memory or on disk) after load, delete it from cache, we can't use it anymore
			delete i->second.buffer;
			cache.erase(i);
		}
		// load new file into cache
		Value& value = cache[RR_RR2STDW(filename)];
		value.buffer = load_noncached(filename,cubeSideName);
		if (value.buffer)
		{
			value.buffer->createReference(); // keep initial ref for us, add one ref for user
			value.bufferVersionWhenLoaded = value.buffer->version;
			value.fileTimeWhenLoaded = bf::last_write_time(filename.w_str());
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
	~ImageCache()
	{
		for (Cache::iterator i=cache.begin();i!=cache.end();i++)
		{
#ifdef _DEBUG
			// If users deleted their buffers, refcount should be down at 1 and this delete is final
			// Don't report in release, some samples knowingly leak, to make code simpler
			if (i->second.buffer && i->second.buffer->getReferenceCount()!=1)
				RRReporter::report(WARN,"Memory leak, image %ls not deleted (%dx).\n",i->second.buffer->filename.w_str(),i->second.buffer->getReferenceCount()-1);
#endif
			delete i->second.buffer;
		}
	}
protected:
	struct Value
	{
		RRBuffer* buffer;
		unsigned bufferVersionWhenLoaded;
		std::time_t fileTimeWhenLoaded; // Critical for Toolbench plugin, "Bake from cache" must not load images from cache if they did change on disk.
	};
	typedef boost::unordered_map<std::wstring,Value> Cache;
	Cache cache;
};

// Single cache works better than individual cache instances in scenes,
// especially if user loads many models that share textures.
ImageCache s_imageCache;

RRBuffer* load_cached(const RRString& filename, const char* cubeSideName[6])
{
	return s_imageCache.load_cached(filename,cubeSideName);
}


//////////////////////////////////////////////////////////////////////////////
//
// FaceGroups

void RRObject::FaceGroups::getTexcoords(RRVector<unsigned>& _texcoords, bool _forUnwrap, bool _forDiffuse, bool _forSpecular, bool _forEmissive, bool _forTransparent) const
{
	std::set<unsigned> texcoords;
	for (unsigned fg=0; fg<size(); fg++)
	{
		rr::RRMaterial* material = (*this)[fg].material;
		if (material)
		{
			if (_forUnwrap)
				texcoords.insert(material->lightmapTexcoord);
			if (_forDiffuse && material->diffuseReflectance.texture)
				texcoords.insert(material->diffuseReflectance.texcoord);
			if (_forSpecular && material->specularReflectance.texture)
				texcoords.insert(material->specularReflectance.texcoord);
			if (_forEmissive && material->diffuseEmittance.texture)
				texcoords.insert(material->diffuseEmittance.texcoord);
			if (_forTransparent && material->specularTransmittance.texture)
				texcoords.insert(material->specularTransmittance.texcoord);
		}
	}
	_texcoords.clear();
	for (std::set<unsigned>::const_iterator i=texcoords.begin();i!=texcoords.end();++i)
		_texcoords.push_back(*i);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjects

void RRObjects::getAllMaterials(RRVector<RRMaterial*>& materials) const
{
	typedef boost::unordered_set<RRMaterial*> Set;
	Set set;
	// fill set
	for (unsigned i=0;i<materials.size();i++)
		set.insert(materials[i]);
	for (unsigned i=0;i<size();i++)
	{
		RRObject::FaceGroups& faceGroups = (*this)[i]->faceGroups;
		for (unsigned g=0;g<faceGroups.size();g++)
		{
			set.insert(faceGroups[g].material);
		}
	}
	// copy set to buffers
	materials.clear();
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			materials.push_back(*i);
}


unsigned RRObjects::loadLayer(int layerNumber, const RRString& path, const RRString& ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			// first try to load per-pixel format
			RRBuffer* buffer = NULL;
			RRObject::LayerParameters layerParameters;
			layerParameters.objectIndex = objectIndex;
			layerParameters.suggestedPath = path;
			layerParameters.suggestedExt = ext;
			object->recommendLayerParameters(layerParameters);
			if ( !bf::exists(RR_RR2PATH(layerParameters.actualFilename)) || !(buffer=RRBuffer::load(layerParameters.actualFilename,NULL)) )
			{
				// if it fails, try to load per-vertex format
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = 0;
				object->recommendLayerParameters(layerParameters);
				if (bf::exists(RR_RR2PATH(layerParameters.actualFilename)))
					buffer = RRBuffer::load(layerParameters.actualFilename.c_str());
			}
			if (buffer && buffer->getType()==BT_VERTEX_BUFFER && buffer->getWidth()!=object->getCollider()->getMesh()->getNumVertices())
			{
				RR_LIMITED_TIMES(5,RRReporter::report(ERRO,"%ls has wrong size.\n",layerParameters.actualFilename.w_str()));
				RR_SAFE_DELETE(buffer);
			}
			if (buffer)
			{
				delete object->illumination.getLayer(layerNumber);
				object->illumination.getLayer(layerNumber) = buffer;
				result++;
				RRReporter::report(INF3,"Loaded %ls.\n",layerParameters.actualFilename.w_str());
			}
			else
			{
				RRReporter::report(INF3,"Not loaded %ls.\n",layerParameters.actualFilename.w_str());
			}
		}
		RRReporter::report(INF2,"Loaded layer %d, %d/%d buffers into %ls.\n",layerNumber,result,size(),path.w_str());
	}
	return result;
}

unsigned RRObjects::saveLayer(int layerNumber, const RRString& path, const RRString& ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		// create destination directories
		{
			bf::path prefix(RR_RR2PATH(path));
			prefix.remove_filename();
			bf::exists(prefix) || bf::create_directories(prefix);
		}

		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			RRBuffer* buffer = object->illumination.getLayer(layerNumber);
			if (buffer)
			{
				RRObject::LayerParameters layerParameters;
				layerParameters.objectIndex = objectIndex;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = (buffer->getType()==BT_VERTEX_BUFFER) ? 0 : 256;
				object->recommendLayerParameters(layerParameters);
				if (buffer->save(layerParameters.actualFilename))
				{
					result++;
					RRReporter::report(INF3,"Saved %ls.\n",layerParameters.actualFilename.w_str());
				}
				else
				if (!layerParameters.actualFilename.empty())
					RRReporter::report(WARN,"Not saved %ls.\n",layerParameters.actualFilename.w_str());
			}
		}
		if (result)
			RRReporter::report(INF2,"Saved layer %d, %d/%d buffers into %ls.\n",layerNumber,result,size(),path.w_str());
		else
			RRReporter::report(WARN,"Failed to save layer %d (%d buffers) into %ls.\n",layerNumber,size(),path.w_str());
	}
	return result;
}

} // namespace
