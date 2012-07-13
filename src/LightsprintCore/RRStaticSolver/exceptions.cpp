// --------------------------------------------------------------------------
// LightsprintCore fragments that need exceptions enabled.
// Copyright (c) 2000-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// RRMesh
#include "Lightsprint/RRMesh.h"
#include "../RRMathPrivate.h"
#include <boost/unordered_map.hpp>

// ImageCache
#include "Lightsprint/RRBuffer.h"
#include <string>
//#include <boost/unordered_map.hpp>
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif

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
		bool sixfiles = wcsstr(filename.w_str(),L"%s")!=NULL;
		bool exists = !sixfiles && bf::exists(RR_RR2PATH(filename));

		Cache::iterator i = cache.find(RR_RR2STDW(filename));
		if (i!=cache.end())
		{
			// image was found in cache
			if (i->second.buffer // we try again if previous load failed, perhaps file was created on background
				&& (i->second.buffer->getDuration() // always take videos from cache
					|| i->second.buffer->version==i->second.bufferVersionWhenLoaded) // take static content from cache only if version did not change
				&& (sixfiles
					|| !exists // for example c@pture is virtual file, it does not exist on disk, but still we cache it
					|| (i->second.fileTimeWhenLoaded==bf::last_write_time(filename.w_str()) && i->second.fileSizeWhenLoaded==bf::file_size(filename.w_str())))
				)
			{
				// detect and report possible error
				bool cached2dCross = i->second.buffer->getType()==BT_2D_TEXTURE && (i->second.buffer->getWidth()*3==i->second.buffer->getHeight()*4 || i->second.buffer->getWidth()*4==i->second.buffer->getHeight()*3);
				bool cachedCube = i->second.buffer->getType()==BT_CUBE_TEXTURE;
				if ((cached2dCross && cubeSideName)
					|| (cachedCube && !cubeSideName && bf::path(RR_RR2PATH(filename)).extension()!=".rrbuffer")) // .rrbuffer is the only format that can produce cube even with cubeSideName=NULL, exclude it from test here
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
			value.fileTimeWhenLoaded = exists ? bf::last_write_time(filename.w_str()) : 0;
			value.fileSizeWhenLoaded = exists ? bf::file_size(filename.w_str()) : 0;
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
		std::time_t fileTimeWhenLoaded;
		boost::uintmax_t fileSizeWhenLoaded;
	};
	typedef boost::unordered_map<std::wstring,Value> Cache;
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
		return NULL;
	}
#endif
}

void RRBuffer::deleteFromCache()
{
	s_imageCache.deleteFromCache(this);
}


//////////////////////////////////////////////////////////////////////////////
//
// FaceGroups

void RRObject::FaceGroups::getTexcoords(RRVector<unsigned>& _texcoords, bool _forUnwrap, bool _forDiffuse, bool _forSpecular, bool _forEmissive, bool _forTransparent) const
{
	std::set<unsigned> texcoords;
	for (unsigned fg=0; fg<size(); fg++)
	{
		RRMaterial* material = (*this)[fg].material;
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

void RRObjects::getAllMaterials(RRMaterials& materials) const
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

void RRObjects::makeNamesUnique() const
{
again:
	bool modified = false;
	typedef boost::unordered_multimap<std::wstring,RRObject*> Map;
	Map map;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		std::wstring filename;
		filename = object->name.w_str();
		for (unsigned i=0;i<filename.size();i++)
			if (wcschr(L"<>:\"/\\|?*",filename[i]))
				filename[i] = '_';
		map.insert(Map::value_type(filename,object));
	}
	for (Map::iterator i=map.begin();i!=map.end();)
	{
		Map::iterator j = i;
		unsigned numEquals = 0;
		while (j!=map.end() && i->first==j->first)
		{
			numEquals++;
			j++;
		}
		if (numEquals>1)
		{
			unsigned numDigits = 0;
			while (numEquals) {numDigits++; numEquals/=10;}
			modified = true;
			unsigned index = 1;
			for (j=i;j!=map.end() && i->first==j->first;++j)
				j->second->name.format(L"%s%s%0*d",j->second->name.w_str(),j->second->name.empty()?"":".",numDigits,index++);
		}
		i = j;
	}
	if (modified)
	{
		// "name" was changed to "name.1", but we did not yet check that "name.1" is free, let's run all checks again
		goto again;
	}
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
		RRReporter::report(INF2,"Loaded %d/%d buffers from %ls<object name>%ls to layer %d.\n",result,size(),path.w_str(),ext.w_str(),layerNumber);
	}
	return result;
}

unsigned RRObjects::saveLayer(int layerNumber, const RRString& path, const RRString& ext) const
{
	bool directoryCreated = false;
	unsigned numBuffers = 0;
	unsigned numSaved = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			RRBuffer* buffer = object->illumination.getLayer(layerNumber);
			if (buffer)
			{
				// create destination directories
				if (!directoryCreated)
				{
					bf::path prefix(RR_RR2PATH(path));
					prefix.remove_filename();
					bf::exists(prefix) || bf::create_directories(prefix);
					directoryCreated = true;
				}

				numBuffers++;
				RRObject::LayerParameters layerParameters;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = (buffer->getType()==BT_VERTEX_BUFFER) ? 0 : 256;
				object->recommendLayerParameters(layerParameters);
				if (buffer->save(layerParameters.actualFilename))
				{
					numSaved++;
					RRReporter::report(INF3,"Saved %ls.\n",layerParameters.actualFilename.w_str());
				}
				else
				if (!layerParameters.actualFilename.empty())
					RRReporter::report(WARN,"Not saved %ls.\n",layerParameters.actualFilename.w_str());
			}
		}
		if (!numBuffers)
			; // don't report saving empty layer
		else
		if (numSaved)
			RRReporter::report(INF2,"Saved %d/%d buffers into %ls<object name>%ls from layer %d.\n",numSaved,numBuffers,path.w_str(),ext.w_str(),layerNumber);
		else
			RRReporter::report(WARN,"Failed to save layer %d (%d buffers) into %ls.\n",layerNumber,numBuffers,path.w_str());
	}
	return numSaved;
}

unsigned RRObjects::layerExistsInMemory(int layerNumber) const
{
	unsigned result = 0;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		if ((*this)[objectIndex]->illumination.getLayer(layerNumber))
			result++;
	return result;
}

unsigned RRObjects::layerDeleteFromMemory(int layerNumber) const
{
	unsigned result = 0;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		if ((*this)[objectIndex]->illumination.getLayer(layerNumber))
		{
			RR_SAFE_DELETE((*this)[objectIndex]->illumination.getLayer(layerNumber));
			result++;
		}
	return result;
}

unsigned RRObjects::layerDeleteFromDisk(const RRString& path, const RRString& ext) const
{
	unsigned result = 0;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		rr::RRObject::LayerParameters layerParameters;
		layerParameters.suggestedPath = path;
		layerParameters.suggestedExt = ext;
		(*this)[objectIndex]->recommendLayerParameters(layerParameters);
		bf::path path(RR_RR2PATH(layerParameters.actualFilename));
		if (bf::exists(path))
		{
			path.remove_filename();
			result++;
		}
	}
	return result;
}

} // namespace
