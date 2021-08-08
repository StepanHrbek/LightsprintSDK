// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - save & load functions.
// --------------------------------------------------------------------------

#include "SVSaveLoad.h"


/////////////////////////////////////////////////////////////////////////////
//
// boost::serialization support

#include "SVApp.h"
#include "wx/wx.h"
#include "wx/stdpaths.h" // wxStandardPaths
#include <cstdio>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#ifdef _WIN32
	#include <GL/wglew.h> // wglSwapInterval
#endif

namespace bf = boost::filesystem;

namespace boost
{
namespace serialization
{

//------------------------------ wxString -----------------------------------

#define SERIALIZE_WXSTRING(name,wxstring,utf8) \
	{ \
		std::string s; \
		if (Archive::is_saving::value) s = wxstring.ToUTF8(); \
		ar & make_nvp(name,s); \
		if (Archive::is_loading::value) wxstring = utf8 ? wxString::FromUTF8(s.c_str()) : s; \
	}

#define SERIALIZE_RRSTRING(name,rrstring,utf8) \
	{ \
		wxString wxstring = RR_RR2WX(rrstring); \
		SERIALIZE_WXSTRING(name,wxstring,utf8); \
		if (Archive::is_loading::value) rrstring = RR_WX2RR(wxstring); \
	}

#define SERIALIZE_WXFILENAME(name,wxstring,utf8) \
	{ \
		SERIALIZE_WXSTRING(name,wxstring,utf8) \
		if (Archive::is_loading::value) \
		{ \
			rr::RRString rrstring = RR_WX2RR(wxstring); \
			fixPath(rrstring); \
			wxstring = RR_RR2WX(rrstring); \
		} \
	}

#define SERIALIZE_RRFILENAME(name,rrstring,utf8) \
	{ \
		SERIALIZE_RRSTRING(name,rrstring,utf8) \
		if (Archive::is_loading::value) \
			fixPath(rrstring); \
	}

//------------------------- ImportParameters ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_ed::ImportParameters& a, const unsigned int version)
{
	ar & make_nvp("unitEnum",a.unitEnum);
	ar & make_nvp("unitFloat",a.unitFloat);
	ar & make_nvp("unitForce",a.unitForce);
	ar & make_nvp("up",a.upEnum);
	ar & make_nvp("upForce",a.upForce);
	if (version>0)
	{
		ar & make_nvp("flipFrontBack",a.flipFrontBackEnum);
	}
	if (version>1)
	{
		ar & make_nvp("removeEmpty",a.removeEmpty);
		ar & make_nvp("unitEnabled",a.unitEnabled);
		ar & make_nvp("upEnabled",a.upEnabled);
		ar & make_nvp("flipFrontBackEnabled",a.flipFrontBackEnabled);
		ar & make_nvp("tangentsEnabled",a.tangentsEnabled);
	}
}

//------------------------- wxRect ------------------------------

template<class Archive>
void serialize(Archive & ar, wxRect& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("w",a.width);
	ar & make_nvp("h",a.height);
}

//------------------------- UserPreferences ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_ed::UserPreferences::WindowLayout& a, const unsigned int version)
{
	ar & make_nvp("fullscreen",a.fullscreen);
	ar & make_nvp("maximized",a.maximized);
	if (version>2)
		ar & make_nvp("rectangle",a.rectangle);
	SERIALIZE_WXSTRING("perspective",a.perspective,version>0);
}

template<class Archive>
void serialize(Archive & ar, rr_ed::UserPreferences& a, const unsigned int version)
{
	if (version>7)
	{
		ar & make_nvp("tooltips",a.tooltips);
	}
	if (version>16)
	{
		ar & make_nvp("swapInterval",a.swapInterval);
	}
	ar & make_nvp("stereoMode",a.stereoMode);
	ar & make_nvp("stereoSwap",a.stereoSwap);
	if (version>19)
	{
		ar & make_nvp("stereoSupersampling",a.stereoSupersampling);
	}
	ar & make_nvp("currentWindowLayout",a.currentWindowLayout);
	ar & make_nvp("windowLayout",a.windowLayout);
	if (version>3)
	{
		ar & make_nvp("import",a.import);
	}
	if (version>2)
	{
		SERIALIZE_WXSTRING("sshotFilename",a.sshotFilename,version>6);
		ar & make_nvp("sshotEnhanced",a.sshotEnhanced);
		ar & make_nvp("sshotEnhancedWidth",a.sshotEnhancedWidth);
		ar & make_nvp("sshotEnhancedHeight",a.sshotEnhancedHeight);
		ar & make_nvp("sshotEnhancedFSAA",a.sshotEnhancedFSAA);
		ar & make_nvp("sshotEnhancedShadowResolutionFactor",a.sshotEnhancedShadowResolutionFactor);
		ar & make_nvp("sshotEnhancedShadowSamples",a.sshotEnhancedShadowSamples);
	}
	if (version>18)
	{
		ar & make_nvp("unwrapResolution",a.unwrapResolution);
		ar & make_nvp("unwrapNumTriangles",a.unwrapNumTriangles);
	}
	if (version==9)
	{
		bool debugging;
		ar & make_nvp("debugging",debugging);
	}
	else
	if (version>9)
	{
		ar & make_nvp("testingLogShaders",a.testingLogShaders);
		ar & make_nvp("testingLogMore",a.testingLogMore);
		ar & make_nvp("testingBeta",a.testingBeta);
	}
}

//---------------------------------------------------------------------------

} // namespace
} // namespace

BOOST_CLASS_VERSION(rr_ed::ImportParameters,2);
BOOST_CLASS_VERSION(rr_ed::UserPreferences::WindowLayout,3)
BOOST_CLASS_VERSION(rr_ed::UserPreferences,20)

//---------------------------------------------------------------------------

namespace rr_ed
{

/////////////////////////////////////////////////////////////////////////////
//
// ImportParameters

ImportParameters::ImportParameters()
{
	removeEmpty = true;
	
	unitEnabled = true;
	unitEnum = U_M;
	unitFloat = 1;
	unitForce = false;

	upEnabled = true;
	upEnum = 0;
	upForce = false;

	flipFrontBackEnabled = true;
	flipFrontBackEnum = 3;

	tangentsEnabled = true;
}

bool ImportParameters::knowsUnitLength(const wxString& filename) const
{
	wxString ext = filename.Right(4).Lower();
	return ext==".rr3" || ext==".dae" || ext==".ifc" || ext==".kmz" || ext==".skp" || ext==".fbx";
}

float ImportParameters::getUnitLength(const wxString& filename) const
{
	if (knowsUnitLength(filename) && !unitForce)
		return 1;
	switch (unitEnum)
	{
		case U_CUSTOM: return unitFloat;
		case U_M: return 1;
		case U_INCH: return 0.0254f;
		case U_CM: return 0.01f;
		case U_MM: return 0.001f;
		default: RR_ASSERT(0);return 1;
	}
}

unsigned ImportParameters::getUpAxis(const wxString& filename) const
{
	wxString ext = filename.Right(4).Lower();
	bool alreadyNormalized = ext!=".obj" && ext!=".stl";
	if (alreadyNormalized && !upForce)
		return 1;
	return upEnum;
}

/////////////////////////////////////////////////////////////////////////////
//
// UserPreferences save/load

static wxString suggestPreferencesDirectory()
{
	return wxStandardPaths::Get().GetUserDataDir()+"/Lightsprint";
}

static wxString suggestPreferencesFilename()
{
	return suggestPreferencesDirectory() + "/SceneViewer.prefs";
}

bool UserPreferences::save() const
{
	try
	{
		boost::system::error_code ec;
		bf::create_directories(RR_WX2PATH(suggestPreferencesDirectory()),ec);
		bf::ofstream ofs(RR_WX2PATH(suggestPreferencesFilename()));
		if (!ofs || ofs.bad())
		{
			rr::RRReporter::report(rr::WARN,"File %ls can't be created, preferences not saved.\n",RR_WX2WCHAR(suggestPreferencesFilename()));
			return false;
		}
		boost::archive::xml_oarchive ar(ofs);

		ar & boost::serialization::make_nvp("userPreferences", *this);
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save %ls.\n",RR_WX2WCHAR(suggestPreferencesFilename()));
		return false;
	}

	return true;
}

bool UserPreferences::load(const wxString& nonDefaultFilename)
{
	try
	{
		bf::ifstream ifs(RR_WX2PATH(nonDefaultFilename.size()?nonDefaultFilename:suggestPreferencesFilename()));
		if (!ifs || ifs.bad())
		{
			// don't warn, we attempt to load prefs each time, without knowing the file exists
			return false;
		}
		boost::archive::xml_iarchive ar(ifs);

		ar & boost::serialization::make_nvp("userPreferences", *this);
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to load preferences %ls.\n",RR_WX2WCHAR(nonDefaultFilename.size()?nonDefaultFilename:suggestPreferencesFilename()));
		return false;
	}

	return true;
}

void UserPreferences::applySwapInterval()
{
#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(swapInterval);
#endif
}

}; // namespace
