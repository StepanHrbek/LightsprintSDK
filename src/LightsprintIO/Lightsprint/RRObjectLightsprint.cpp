// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for .rr3, .rrbuffer, .rrmaterial formats.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_LIGHTSPRINT

#pragma warning(disable:4996) // this warning is very frequent in boost 1.42, not helpful

//#define USE_TEXT         // portable text archive
//#define USE_TEXT_ZLIB    // portable text zlib archive, boost must be compiled with zlib
//#define USE_XML          // portable xml archive
//#define USE_XML_ZLIB     // portable xml zlib archive, boost must be compiled with zlib
//#define USE_XML_BZIP2    // portable xml bzip2 archive, boost must be compiled with bzip2
//#define USE_BINARY       // nonportable binary archive
//#define USE_BINARY_ZLIB  // nonportable binary zlib archive, boost must be compiled with zlib
//#define USE_BINARY_BZIP2 // nonportable binary bzip2 archive, boost must be compiled with bzip2
//#define USE_PORTABLE_BINARY // portable binary archive
#define USE_PORTABLE_BINARY_ZLIB // portable binary zlib archive, boost must be compiled with zlib


// .rr3 code uses boost, http://boost.org.
// If you don't have boost, install it or comment out #define SUPPORT_LIGHTSPRINT.

// .rr3 import fails to compile with Visual Studio 2015.2.
// This is caused by Visual Studio 2015.2 bug, see e.g. https://connect.microsoft.com/VisualStudio/feedback/details/2563889/compile-error-c2996-recursive-function-template-definition-is-emitted-for-supposedly-compilable-code
// If you can't use different Visual Studio version, comment out #define SUPPORT_LIGHTSPRINT.

#include "RRObjectLightsprint.h"
#include "RRSerialization.h"
#ifdef USE_TEXT
	#include <boost/archive/text_iarchive.hpp>
	#include <boost/archive/text_oarchive.hpp>
#endif
#ifdef USE_TEXT_ZLIB
	#include <boost/iostreams/filtering_stream.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/zlib.hpp>
	#include <boost/archive/text_iarchive.hpp>
	#include <boost/archive/text_oarchive.hpp>
#endif
#ifdef USE_XML
	#include <boost/archive/xml_iarchive.hpp>
	#include <boost/archive/xml_oarchive.hpp>
#endif
#ifdef USE_XML_ZLIB
	#include <boost/iostreams/filtering_stream.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/zlib.hpp>
	#include <boost/archive/xml_iarchive.hpp>
	#include <boost/archive/xml_oarchive.hpp>
#endif
#ifdef USE_XML_BZIP2
	#include <boost/iostreams/filtering_stream.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/bzip2.hpp>
	#include <boost/archive/xml_iarchive.hpp>
	#include <boost/archive/xml_oarchive.hpp>
#endif
#ifdef USE_BINARY
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif
#ifdef USE_BINARY_ZLIB
	#include <boost/iostreams/filtering_streambuf.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/zlib.hpp>
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif
#ifdef USE_BINARY_BZIP2
	#include <boost/iostreams/filtering_streambuf.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/bzip2.hpp>
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif
#ifdef USE_PORTABLE_BINARY
	#include "portable_binary_oarchive.hpp"
	#include "portable_binary_iarchive.hpp"
#endif
#ifdef USE_PORTABLE_BINARY_ZLIB
	#include <boost/iostreams/filtering_stream.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/zlib.hpp>
	#include "portable_binary_iarchive.hpp"
	#include "portable_binary_oarchive.hpp"
#endif

// case insensitive comparison of extension (tolower)
#include <algorithm>
#include <string>
#include <filesystem>
#include <fstream>
namespace bf = std::filesystem;

using namespace std;
using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneLightsprint


class RRSceneLightsprint : public RRScene
{
public:

	static RRScene* load(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
	{
		try
		{
			std::ifstream ifs(RR_RR2PATH(filename),std::ios::in|std::ios::binary);
			if (!ifs || ifs.bad())
			{
				rr::RRReporter::report(rr::WARN,"Scene %ls can't be loaded, file does not exist.\n",filename.w_str());
				return nullptr;
			}

#ifdef USE_TEXT
			boost::archive::text_iarchive ar(ifs);
#endif
#ifdef USE_TEXT_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::input> in;
			in.push(boost::iostreams::zlib_decompressor());
			in.push(ifs);
			boost::archive::text_iarchive ar(in);
#endif
#ifdef USE_XML
			boost::archive::xml_iarchive ar(ifs);
#endif
#ifdef USE_XML_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::input> in;
			in.push(boost::iostreams::zlib_decompressor());
			in.push(ifs);
			boost::archive::xml_iarchive ar(in);
#endif
#ifdef USE_XML_BZIP2
			boost::iostreams::filtering_stream<boost::iostreams::input> in;
			in.push(boost::iostreams::bzip2_decompressor());
			in.push(ifs);
			boost::archive::xml_iarchive ar(in);
#endif
#ifdef USE_BINARY
			boost::archive::binary_iarchive ar(ifs);
#endif
#ifdef USE_BINARY_ZLIB
			boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
			in.push(boost::iostreams::zlib_decompressor());
			in.push(ifs);
			boost::archive::binary_iarchive ar(in);
#endif
#ifdef USE_BINARY_BZIP2
			boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
			in.push(boost::iostreams::bzip2_decompressor());
			in.push(ifs);
			boost::archive::binary_iarchive ar(in);
#endif
#ifdef USE_PORTABLE_BINARY
			portable_binary_iarchive ar(ifs);
#endif
#ifdef USE_PORTABLE_BINARY_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::input> in;
			in.push(boost::iostreams::zlib_decompressor());
			in.push(ifs);
			portable_binary_iarchive ar(in);
#endif
			RRSceneLightsprint* scene = new RRSceneLightsprint;

			SerializationRuntime serializationRuntime(textureLocator,"load_rr3");

			RRString oldReference;
			std::string filenameOrVersion;
			ar & boost::serialization::make_nvp("filename", filenameOrVersion);
			if (filenameOrVersion.size()>3)
			{
				// version 0, only local charset filename
				oldReference = RR_STD2RR(filenameOrVersion);
			}
			else
			{
				// version 1, unicode filename added
				ar & boost::serialization::make_nvp("filename", oldReference);
			}
			fixPath(oldReference);
			if (serializationRuntime.textureLocator)
				serializationRuntime.textureLocator->setRelocation(true,oldReference,filename);
			ar & boost::serialization::make_nvp("scene", *(RRScene*)scene);
			if (serializationRuntime.textureLocator)
				serializationRuntime.textureLocator->setRelocation(false,oldReference,filename);

			// remember materials and meshes created by boost, so we can free them in destructor
			// user is allowed to manipulate scene, add or remove parts, but we will still delete only what load() created
			for (unsigned o=0;o<scene->objects.size();o++)
			{
				scene->meshes.insert(const_cast<RRMesh*>(scene->objects[o]->getCollider()->getMesh()));
				for (unsigned fg=0;fg<scene->objects[o]->faceGroups.size();fg++)
					scene->materials.insert(scene->objects[o]->faceGroups[fg].material);
			}

			return scene;
		}
		catch(...)
		{
			rr::RRReporter::report(rr::ERRO,"Failed to load scene %ls.\n",filename.w_str());
			return nullptr;
		}
	}

	static bool save(const RRScene* scene, const RRString& filename)
	{
		if (!scene)
		{
			return false;
		}
		try
		{
			std::ofstream ofs(RR_RR2PATH(filename),std::ios::out|std::ios::binary|std::ios::trunc);
			if (!ofs || ofs.bad())
			{
				rr::RRReporter::report(rr::WARN,"File %ls can't be created, scene not saved.\n",filename.w_str());
				return false;
			}

#ifdef USE_TEXT
			boost::archive::text_oarchive ar(ofs);
#endif
#ifdef USE_TEXT_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::output> out;
			out.push(boost::iostreams::zlib_compressor());
			out.push(ofs);
			boost::archive::text_oarchive ar(out);
#endif
#ifdef USE_XML
			boost::archive::xml_oarchive ar(ofs);
#endif
#ifdef USE_XML_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::output> out;
			out.push(boost::iostreams::zlib_compressor());
			out.push(ofs);
			boost::archive::xml_oarchive ar(out);
#endif
#ifdef USE_XML_BZIP2
			boost::iostreams::filtering_stream<boost::iostreams::output> out;
			out.push(boost::iostreams::bzip2_compressor());
			out.push(ofs);
			boost::archive::xml_oarchive ar(out);
#endif
#ifdef USE_BINARY
			boost::archive::binary_oarchive ar(ofs);
#endif
#ifdef USE_BINARY_ZLIB
			boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
			out.push(boost::iostreams::zlib_compressor());
			out.push(ofs);
			boost::archive::binary_oarchive ar(out);
#endif
#ifdef USE_BINARY_BZIP2
			boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
			out.push(boost::iostreams::bzip2_compressor());
			out.push(ofs);
			boost::archive::binary_oarchive ar(out);
#endif
#ifdef USE_PORTABLE_BINARY
			portable_binary_oarchive ar(ofs);
#endif
#ifdef USE_PORTABLE_BINARY_ZLIB
			boost::iostreams::filtering_stream<boost::iostreams::output> out;
			out.push(boost::iostreams::zlib_compressor());
			out.push(ofs);
			portable_binary_oarchive ar(out);
#endif
			SerializationRuntime serializationRuntime(nullptr,"save_rr3");

			std::string filenameOrVersion;
			ar & boost::serialization::make_nvp("filename", filenameOrVersion); // former local charset filename, must be preserved, loader always tries to read it, we don't have any version number at this point in file
			ar & boost::serialization::make_nvp("filename", filename);
			ar & boost::serialization::make_nvp("scene", *scene);

			return true;
		}
		catch(...)
		{
			rr::RRReporter::report(rr::ERRO,"Failed to save %ls.\n",filename.w_str());
			return false;
		}
	}

	virtual ~RRSceneLightsprint()
	{
		// delete what boost loaded from disk
		// these are original vectors, user had no chance to modify them
		for (unsigned i=0;i<objects.size();i++)
		{
			delete objects[i]->getCollider();
			delete objects[i];
		}
		for (unsigned i=0;i<lights.size();i++)
			delete lights[i];
		while(!meshes.empty()) delete *meshes.begin(), meshes.erase(meshes.begin());
		while(!materials.empty()) delete *materials.begin(), materials.erase(materials.begin());
	}

private:
	//! Resources deleted at destruction time.
	std::unordered_set<RRMaterial*> materials;
	//! Resources deleted at destruction time.
	std::unordered_set<RRMesh*> meshes;
};


/////////////////////////////////////////////////////////////////////////////
//
// .rrbuffer load

static RRBuffer* loadBuffer(const RRString& filename)
{
	try
	{
		std::ifstream ifs(RR_RR2PATH(filename),std::ios::in|std::ios::binary);
		if (!ifs || ifs.bad())
		{
			//rr::RRReporter::report(rr::WARN,"Buffer %ls can't be loaded, file does not exist.\n",filename.w_str());
			return nullptr;
		}

		boost::iostreams::filtering_stream<boost::iostreams::input> in;
		in.push(boost::iostreams::zlib_decompressor());
		in.push(ifs);
		portable_binary_iarchive ar(in);

		SerializationRuntime serializationRuntime(nullptr,"load_rrbuffer");

		unsigned version;
		ar & boost::serialization::make_nvp("version", version);
		rr::RRBuffer* buffer = boost::serialization::loadBufferContents<portable_binary_iarchive>(ar,version);
		if (buffer)
			buffer->filename = filename; // [#36] actual filename opened
		return buffer;
	}
	catch(...)
	{
		// importers don't report failure, it is reported at the end of RRBuffer::load()
		//rr::RRReporter::report(rr::ERRO,"Failed to load buffer %ls.\n",filename.w_str());
		return nullptr;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// .rrbuffer save

static bool saveBuffer(RRBuffer* buffer, const RRString& filename, const RRBuffer::SaveParameters* saveParameters)
{
	if (!buffer || filename.empty())
	{
		return false;
	}
	std::wstring ext = bf::path(RR_RR2PATH(filename)).extension().wstring();
	std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
	if (ext!=L".rrbuffer")
	{
		// don't warn if extension is wrong, maybe next saver will succeed (our system of savers does not know what extensions saver supports)
		return false;
	}
	try
	{
		std::ofstream ofs(RR_RR2PATH(filename),std::ios::out|std::ios::binary|std::ios::trunc);
		if (!ofs || ofs.bad())
		{
			//rr::RRReporter::report(rr::WARN,"File %ls can't be created, buffer not saved.\n",filename.w_str());
			return false;
		}

		boost::iostreams::filtering_stream<boost::iostreams::output> out;
		out.push(boost::iostreams::zlib_compressor());
		out.push(ofs);
		portable_binary_oarchive ar(out);

		SerializationRuntime serializationRuntime(nullptr,"save_rrbuffer");

		unsigned version = 0;
		ar & boost::serialization::make_nvp("version", version);
		boost::serialization::saveBufferContents<portable_binary_oarchive>(ar,*buffer,version);

		return true;
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save buffer %ls.\n",filename.w_str());
		return false;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// .rrmaterial load

static RRMaterials* loadMaterial(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
{
	RRMaterials* materials = new RRMaterials;
	try
	{
		std::ifstream ifs(RR_RR2PATH(filename),std::ios::in|std::ios::binary);
		if (!ifs || ifs.bad())
		{
			//rr::RRReporter::report(rr::WARN,"Buffer %ls can't be loaded, file does not exist.\n",filename.w_str());
			return nullptr;
		}

		boost::iostreams::filtering_stream<boost::iostreams::input> in;
		in.push(boost::iostreams::zlib_decompressor());
		in.push(ifs);
		portable_binary_iarchive ar(in);

		SerializationRuntime serializationRuntime(textureLocator,"load_rrmaterial");

		RRString oldReference;
		ar & boost::serialization::make_nvp("filename", oldReference);
		//rr::RRReporter::report(rr::INF2,"originally saved as %ls.\n",oldReference.w_str());
		fixPath(oldReference);
		if (serializationRuntime.textureLocator)
			serializationRuntime.textureLocator->setRelocation(true,oldReference,filename);
		ar & boost::serialization::make_nvp("materials",*materials);
		if (serializationRuntime.textureLocator)
			serializationRuntime.textureLocator->setRelocation(false,oldReference,filename);

		return materials;
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to load material %ls.\n",filename.w_str());
		delete materials;
		return nullptr;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// .rrmaterial save

static bool saveMaterial(const RRMaterials* materials, const RRString& filename)
{
	if (!materials || filename.empty())
	{
		return false;
	}
	try
	{
		std::ofstream ofs(RR_RR2PATH(filename),std::ios::out|std::ios::binary|std::ios::trunc);
		if (!ofs || ofs.bad())
		{
			//rr::RRReporter::report(rr::WARN,"File %ls can't be created, buffer not saved.\n",filename.w_str());
			return false;
		}

		boost::iostreams::filtering_stream<boost::iostreams::output> out;
		out.push(boost::iostreams::zlib_compressor());
		out.push(ofs);
		portable_binary_oarchive ar(out);

		SerializationRuntime serializationRuntime(nullptr,"save_rrmaterial");

		ar & boost::serialization::make_nvp("filename", filename);
		ar & boost::serialization::make_nvp("materials",*materials);

		return true;
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save material %ls.\n",filename.w_str());
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderLightsprint()
{
	RRScene::registerLoader("*.rr3",RRSceneLightsprint::load);
	RRScene::registerSaver("*.rr3",RRSceneLightsprint::save);
	RRBuffer::registerLoader("*.rrbuffer",loadBuffer);
	RRBuffer::registerSaver("*.rrbuffer",saveBuffer);
	RRMaterials::registerLoader("*.rrmaterial",loadMaterial);
	RRMaterials::registerSaver("*.rrmaterial",saveMaterial);
}

#endif // SUPPORT_LIGHTSPRINT
