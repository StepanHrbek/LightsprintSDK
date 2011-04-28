// --------------------------------------------------------------------------
// Lightsprint adapters for Lightsprint RR3 format.
// Copyright (C) 2010-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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

#include "RRObjectLightsprint.h"
#include "RRSerialization.h"
#include <fstream>
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

using namespace std;
using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneLightsprint


class RRSceneLightsprint : public RRScene
{
public:

	static RRScene* load(const char* filename, RRFileLocator* textureLocator, bool* aborting)
	{
		try
		{
			std::ifstream ifs(filename,std::ios::in|std::ios::binary);
			if (!ifs || ifs.bad())
			{
				rr::RRReporter::report(rr::WARN,"Scene %s can't be loaded, file does not exist.\n",filename);
				return NULL;
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

			g_textureLocator = textureLocator;
			std::string oldReference;
			ar & boost::serialization::make_nvp("filename", oldReference);
			if (g_textureLocator)
				g_textureLocator->setRelocation(true,RR_STD2RR(oldReference),filename);
			ar & boost::serialization::make_nvp("scene", *(RRScene*)scene);
			if (g_textureLocator)
				g_textureLocator->setRelocation(false,RR_STD2RR(oldReference),filename);
			g_textureLocator = NULL;

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
			rr::RRReporter::report(rr::ERRO,"Failed to load scene %s.\n",filename);
			return NULL;
		}
	}

	static bool save(const RRScene* scene, const char* filename)
	{
		if (!scene || !filename)
		{
			return false;
		}
		try
		{
			std::ofstream ofs(filename,std::ios::out|std::ios::binary|std::ios::trunc);
			if (!ofs || ofs.bad())
			{
				rr::RRReporter::report(rr::WARN,"File %s can't be created, scene not saved.\n",filename);
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
			std::string filenameStr(filename);
			ar & boost::serialization::make_nvp("filename", filenameStr);
			ar & boost::serialization::make_nvp("scene", *scene);

			return true;
		}
		catch(...)
		{
			rr::RRReporter::report(rr::ERRO,"Failed to save %s.\n",filename);
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
	boost::unordered_set<RRMaterial*> materials;
	//! Resources deleted at destruction time.
	boost::unordered_set<RRMesh*> meshes;
};


//////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderLightsprint()
{
	RRScene::registerLoader("*.rr3",RRSceneLightsprint::load);
	RRScene::registerSaver("*.rr3",RRSceneLightsprint::save);
}

#endif // SUPPORT_LIGHTSPRINT
