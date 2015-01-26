// --------------------------------------------------------------------------
// LightsprintCore fragments that need exceptions enabled.
// Copyright (c) 2000-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// RRMesh
#include "Lightsprint/RRMesh.h"
#include "../RRMathPrivate.h"
#include <unordered_map>

// ImageCache
#include "Lightsprint/RRBuffer.h"
#include <string>
//#include <unordered_map>
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif

// FaceGroups
#include "Lightsprint/RRObject.h"
#include <set>

// RRObjects
//#include "Lightsprint/RRObject.h"
#include <string> // std::to_wstring
#include <unordered_set>
#ifdef RR_LINKS_BOOST
	#include <boost/filesystem.hpp>
	namespace bf = boost::filesystem;
#else
	#include "Lightsprint/RRFileLocator.h"
#endif

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
		typedef std::unordered_map<RRReal,RRReal> YToArea;
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
#ifdef RR_LINKS_BOOST
		boost::system::error_code ec;
		bool exists = !sixfiles && bf::exists(RR_RR2PATH(filename),ec);
#else
		RRFileLocator fl;
		bool exists = !sixfiles && fl.exists(filename);
#endif

		Cache::iterator i = cache.find(RR_RR2STDW(filename));
		if (i!=cache.end())
		{
			// image was found in cache
			if (i->second.buffer // we try again if previous load failed, perhaps file was created on background
				&& (i->second.buffer->getDuration() // always take videos from cache
					|| i->second.buffer->version==i->second.bufferVersionWhenLoaded) // take static content from cache only if version did not change
				&& (sixfiles
					|| !exists // for example c@pture is virtual file, it does not exist on disk, but still we cache it
#ifdef RR_LINKS_BOOST
					|| (i->second.fileTimeWhenLoaded==bf::last_write_time(filename.w_str(),ec) && i->second.fileSizeWhenLoaded==bf::file_size(filename.w_str()))
#endif
					)
				)
			{
				// detect and report possible error
				bool cached2dCross = i->second.buffer->getType()==BT_2D_TEXTURE && (i->second.buffer->getWidth()*3==i->second.buffer->getHeight()*4 || i->second.buffer->getWidth()*4==i->second.buffer->getHeight()*3);
				bool cachedCube = i->second.buffer->getType()==BT_CUBE_TEXTURE;
				if ((cached2dCross && cubeSideName)
#ifdef RR_LINKS_BOOST
					|| (cachedCube && !cubeSideName && bf::path(RR_RR2PATH(filename)).extension()!=".rrbuffer")) // .rrbuffer is the only format that can produce cube even with cubeSideName=NULL, exclude it from test here
#else
					|| (cachedCube && !cubeSideName))
#endif
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
#ifdef RR_LINKS_BOOST
			value.fileTimeWhenLoaded = exists ? bf::last_write_time(filename.w_str(),ec) : 0;
			value.fileSizeWhenLoaded = exists ? bf::file_size(filename.w_str(),ec) : 0;
#endif
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
#ifdef RR_LINKS_BOOST
		std::time_t fileTimeWhenLoaded;
		boost::uintmax_t fileSizeWhenLoaded;
#endif
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
// RRObjects

void RRObjects::getAllMaterials(RRMaterials& materials) const
{
	typedef std::unordered_set<RRMaterial*> Set;
	Set set;
	// fill set
	for (unsigned i=materials.size();i--;) // iteration from 0 would flip order of materials in RRMaterials. we don't promise to preserve order, but at least we try to, this mostly works (although it probably depends on implementation of unordered_set)
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

void RRObjects::updateColorPhysical(const RRColorSpace* scaler) const
{
	typedef std::unordered_set<RRMaterial*> Set;
	Set set;
	// gather all materials (it has to be fast, using getAllMaterials would be slow)
	for (unsigned i=0;i<size();i++)
	{
		RRObject::FaceGroups& faceGroups = (*this)[i]->faceGroups;
		for (unsigned g=0;g<faceGroups.size();g++)
		{
			set.insert(faceGroups[g].material);
		}
	}
	// color->colorLinear
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			(*i)->convertToLinear(scaler);
}

static std::wstring filenamized(RRString& name)
{
	std::wstring filename;
	filename = name.w_str();
	for (unsigned i=0;i<filename.size();i++)
		if (wcschr(L"<>:\"/\\|?*",filename[i]))
			filename[i] = '_';
	return filename;
}

void splitName(std::wstring& prefix, unsigned long long& number)
{
	number = 0;
	unsigned long long base = 1;
	while (prefix.size() && prefix[prefix.size()-1]>='0' && prefix[prefix.size()-1]<='9')
	{
		number += base * (prefix[prefix.size()-1]-'0');
		prefix.pop_back();
		base *= 10;
	}
}

std::wstring mergedName(const std::wstring& prefix, unsigned long long number)
{
	return prefix + ((number<10)?L"0":L"") + std::to_wstring(number);
}

void RRObjects::makeNamesUnique() const
{
	// naming scheme 1
	// turns names xxx,xxx,xxx,xxx5,xxx5 into xxx,xxx01,xxx02,xxx5,xxx06
	typedef std::unordered_set<std::wstring> Set;
	typedef std::unordered_map<std::wstring,unsigned long long> Map;
	Map filenamizedOriginals; // value = the highest number so far assigned to this filename, we skip lower numbers when searching for unused one
	Set filenamizedAccepted;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		filenamizedOriginals[filenamized(object->name)] = 0;
	}
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		std::wstring filenamizedOriginal = filenamized(object->name);
		std::wstring filenamizedCandidate = filenamizedOriginal;
		if (filenamizedAccepted.find(filenamizedCandidate)==filenamizedAccepted.end())
		{
			// 1. accept "so far" unique names
			filenamizedAccepted.insert(filenamizedCandidate);
		}
		else
		{
			// 2. split non-unique name to prefix + number
			std::wstring filenamizedPrefix = filenamizedCandidate;
			unsigned long long number;
			splitName(filenamizedPrefix,number);
			// optimization step1: skip numbers that are definitely not free
			if (filenamizedOriginals[filenamizedOriginal])
				number = filenamizedOriginals[filenamizedOriginal];
			// 3. try increasing number until it becomes "globally" unique
			do
			{
				number++;
				filenamizedCandidate = mergedName(filenamizedPrefix,number);
			}
			while (filenamizedAccepted.find(filenamizedCandidate)!=filenamizedAccepted.end() || filenamizedOriginals.find(filenamizedCandidate)!=filenamizedOriginals.end());
			filenamizedAccepted.insert(filenamizedCandidate);
			// optimization step2: remember numbers that are definitely not free
			filenamizedOriginals[filenamizedOriginal] = number;
			// 4. construct final non-filenamized name
			std::wstring rawPrefix = RR_RR2STDW(object->name);
			unsigned long long rawNumber;
			splitName(rawPrefix,rawNumber);
			object->name = RR_STDW2RR(mergedName(rawPrefix,number));
		}
	}
#if 0
	// naming scheme 2
	// turns names xxx,xxx,xxx,xxx.3,xxx.3 into xxx,xxx.2,xxx.4,xxx.3,xxx.3.2
again:
	bool modified = false;
	typedef std::unordered_multimap<std::wstring,RRObject*> Map;
	Map map;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		map.insert(Map::value_type(filenamized(object->name),object));
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
			{
				if (index>1)
				{
					// 1: try to format new name
					// if it is already taken, increase index and goto 1
					try_index:
					RRString newNameCandidate;
					newNameCandidate.format(L"%ls%hs%0*d",j->second->name.w_str(),j->second->name.empty()?"":".",numDigits,index);
					if (map.find(filenamized(newNameCandidate))!=map.end())
					{
						index++;
						goto try_index;
					}
					j->second->name = newNameCandidate;
				}
				index++;
			}
		}
		i = j;
	}
	if (modified)
	{
		// "name" was changed to "name.1", but we did not yet check that "name.1" is free, let's run all checks again
		goto again;
	}
#endif
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
#ifdef RR_LINKS_BOOST
			boost::system::error_code ec;
			if ( !bf::exists(RR_RR2PATH(layerParameters.actualFilename),ec) || !(buffer=RRBuffer::load(layerParameters.actualFilename,NULL)) )
#else
			RRFileLocator fl;
			if ( !fl.exists(layerParameters.actualFilename) || !(buffer=RRBuffer::load(layerParameters.actualFilename,NULL)) )
#endif
			{
				// if it fails, try to load per-vertex format
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = 0;
				object->recommendLayerParameters(layerParameters);
#ifdef RR_LINKS_BOOST
				if (bf::exists(RR_RR2PATH(layerParameters.actualFilename),ec))
#else
				if (fl.exists(layerParameters.actualFilename))
#endif
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
#ifdef RR_LINKS_BOOST
				if (!directoryCreated)
				{
					bf::path prefix(RR_RR2PATH(path));
					prefix.remove_filename();
					boost::system::error_code ec;
					bf::exists(prefix,ec) || bf::create_directories(prefix,ec);
					directoryCreated = true;
				}
#endif

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
			RRReporter::report(INF2,"Saved %d/%d buffers into %ls<object name>.%ls from layer %d.\n",numSaved,numBuffers,path.w_str(),ext.w_str(),layerNumber);
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
#ifdef RR_LINKS_BOOST
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		rr::RRObject::LayerParameters layerParameters;
		layerParameters.suggestedPath = path;
		layerParameters.suggestedExt = ext;
		(*this)[objectIndex]->recommendLayerParameters(layerParameters);
		bf::path path(RR_RR2PATH(layerParameters.actualFilename));
		boost::system::error_code ec;
		if (bf::exists(path,ec))
		{
			if (bf::remove(path,ec))
				result++;
		}
	}
#endif
	return result;
}

} // namespace
