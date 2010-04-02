// --------------------------------------------------------------------------
// Lightsprint adapters for Lightsprint RR3 format.
// Copyright (C) 2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_LIGHTSPRINT

#pragma warning(disable:4996) // this warning is very frequent in boost 1.42, not helpful

#define USE_XML // portable xml archive
//#define USE_BZIP2 // nonportable bzip2 archive, boost must be compiled with bzip2
//#define USE_ZLIB // nonportable zlib archive, boost must be compiled with zlib
//#define USE_BINARY // nonportable binary archive
/*
        bez_obrazku     s_obrazky
        save load size  save load  size
  dae                         610  1000(400dae,600jpg)
  bz2     82   13   53   730  340  2225
  zip     23    4   58   460   87  2815
  bin      6    1  337    18    9  4458
*/

// .rr3 format depends on boost, http://boost.org.
// If you don't have boost, install it or comment out #define SUPPORT_LIGHTSPRINT.

#include "RRObjectLightsprint.h"
#include "RRSerialization.h"
#include <fstream>
//#include <iostream>
#ifdef USE_XML
	#include <boost/archive/xml_iarchive.hpp>
	#include <boost/archive/xml_oarchive.hpp>
#endif
#ifdef USE_BZIP2
	#include <boost/iostreams/filtering_streambuf.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/bzip2.hpp>
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif
#ifdef USE_ZLIB
	#include <boost/iostreams/filtering_streambuf.hpp>
	#include <boost/iostreams/copy.hpp>
	#include <boost/iostreams/filter/zlib.hpp>
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif
#ifdef USE_BINARY
	#include <boost/archive/binary_iarchive.hpp>
	#include <boost/archive/binary_oarchive.hpp>
#endif

using namespace std;
using namespace rr;

//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new adapters.
// Define VERIFY to enable verification of adapters and data.
// RRReporter will be used to warn about detected data inconsistencies.
// Once your code/data are verified and don't emit messages via RRReporter,
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneLightsprint

/*
Q: jak serializovat i dynobj?
a) RRScene rozsirit o dynobjs a save()
	-slozitejsi save v SV, musel bych pred savem data zkopirovat ze solveru do RRScene
	-dynobjs zmatou uzivatele, zadny jiny fileformat dynobj nepodporuje
b) serializovat solver, solver->save()/load()
	-nezapada do koncepce rr_io RRScene(filename)
	-slozitejsi load v SV, musel bych pridat treti cestu (solver,file,rr3file)

Q: rr_io se builduje moc dlouho, navic rr3 muze zaviset na boostu, co s tim?
a) YES
   sirit binarni rr_io, nejaky loader zverejnit na ukazku v samplu Loader (po loadu pusti scene viewer)
   -nutno vyrobit novy sampl a 2 ukazkove loadery, do arrays a do custom meshe
      *v ukazce pouzit 3ds, procistit kod
      *v src nechat jeste rrobjectassimp bez assimpu, rrobjectobj a rrobjectfcollada bez fcollady, jen na ukazku
   +src muzu nabizet za priplatek
   +zmensi include (o fcolladu,freeimage)
   *do build.bat a .lst pridat rr_io_dd.dll
b) rr3 a opencolladu dat do core
   -nesystemove umisteni

Q: jak na xrefy?
+dostatecne obecne xrefy jednou ranou vyresi meshe i textury (nekdy odkaz, nekdy pripakovat primo data)
*pro mesh i texturu: do streamu vlozit odkaz na soubor nebo primo data
*vic odkazu na tentyz mesh/tex: ohlidat at se loadne jen jednou
*pro mesh i texturu: podle filename se rozhodovat zda je to xref nebo builtin (""=builtin, !""=xref) (v SV jde rucne zmenit)
*mozne UI: do "import scene" pridat dialog
 1(a) open as new scene
  (b) insert into existing scene

 2(a) pridat do sceny jako xref, zmeny se neulozi
      +3(a) = vlozit multiobj s name="XREF:filename"
              rozbije blendovani pokud je tam mnoho oken
      +3(b) = vlozit jako mnoho obj s name="xref:name", pridat specialni prazdny obj s name="XREF:filename"
              pri savu "xref:*" ignorovat, "XREF:*" ulozit
  (b) vlozit primo data, mnoho meshu, bude ulozeno do rr3

 3(a) keep individual meshes
  (b) load as one big mesh (may break sorting of semitransparent meshes)

 4(a) static   \
  (b) dynamic  / neptat se pokud jde o .rr3

  [ 1.0 ] scale

Q: kdo uvolni objekty nahrane boostem z disku?

*/


class RRSceneLightsprint : public RRScene
{
public:

	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		try
		{
			std::ifstream ifs(filename,std::ios::in|std::ios::binary);
			if (!ifs || ifs.bad())
			{
				rr::RRReporter::report(rr::WARN,"Scene %s can't be loaded, file does not exist.\n",filename);
				return NULL;
			}

#ifdef USE_XML
			boost::archive::xml_iarchive ar(ifs);
#endif
#ifdef USE_BZIP2
			boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
			in.push(boost::iostreams::bzip2_decompressor());
			in.push(ifs);
			boost::archive::binary_iarchive ar(in);
#endif
#ifdef USE_ZLIB
			boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
			in.push(boost::iostreams::zlib_decompressor());
			in.push(ifs);
			boost::archive::binary_iarchive ar(in);
#endif
#ifdef USE_BINARY
			boost::archive::binary_iarchive ar(ifs);
#endif
//			char* pathToTextures = _strdup(filename);
//			char* tmp = RR_MAX(strrchr(pathToTextures,'\\'),strrchr(pathToTextures,'/'));
//			if (tmp) tmp[1] = 0;
			RRSceneLightsprint* scene = new RRSceneLightsprint;

			ar & boost::serialization::make_nvp("scene", *(RRScene*)scene);

//			free(pathToTextures);

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
			rr::RRReporter::report(rr::ERRO,"Failed to load %s.\n",filename);
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

#ifdef USE_XML
			boost::archive::xml_oarchive ar(ofs);
#endif
#ifdef USE_BZIP2
			boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
			out.push(boost::iostreams::bzip2_compressor());
			out.push(ofs);
			boost::archive::binary_oarchive ar(out);
#endif
#ifdef USE_ZLIB
			boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
			out.push(boost::iostreams::zlib_compressor());
			out.push(ofs);
			boost::archive::binary_oarchive ar(out);
#endif
#ifdef USE_BINARY
			boost::archive::binary_oarchive ar(ofs);
#endif
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
