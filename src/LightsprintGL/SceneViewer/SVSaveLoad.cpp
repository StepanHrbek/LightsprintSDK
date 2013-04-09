// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSaveLoad.h"


/////////////////////////////////////////////////////////////////////////////
//
// boost::serialization support

#include "SVApp.h"
#include "wx/wx.h"
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

#ifdef _WIN32
	#define SERIALIZE_FILENAME SERIALIZE_WXSTRING
#else
	#define SERIALIZE_FILENAME(name,wxstring,utf8) \
	{ \
		SERIALIZE_WXSTRING(name,wxstring,utf8) \
		if (Archive::is_loading::value) wxstring.Replace("\\","/",true); \
	}
#endif


//------------------------- ImportParameters ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_gl::ImportParameters& a, const unsigned int version)
{
	ar & make_nvp("unitEnum",a.unitEnum);
	ar & make_nvp("unitFloat",a.unitFloat);
	ar & make_nvp("unitForce",a.unitForce);
	ar & make_nvp("up",a.up);
	ar & make_nvp("upForce",a.upForce);
	if (version>0)
	{
		ar & make_nvp("flipFrontBack",a.flipFrontBack);
	}
}

//------------------------- UserPreferences ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_gl::UserPreferences::WindowLayout& a, const unsigned int version)
{
	ar & make_nvp("fullscreen",a.fullscreen);
	ar & make_nvp("maximized",a.maximized);
	SERIALIZE_WXSTRING("perspective",a.perspective,version>0);
}

enum LegacyStereoMode
{
	LSM_INTERLACED  =0, // top scanline is visible by right eye, correct at least for LG D2342P-PN
	LSM_SIDE_BY_SIDE=1, // left hals is left eye
	LSM_TOP_DOWN    =2, // top half is left eye
};

template<class Archive>
void	 serialize(Archive & ar, rr_gl::UserPreferences& a, const unsigned int version)
{
	if (version>7)
	{
		ar & make_nvp("tooltips",a.tooltips);
	}
	if (version>14)
	{
		ar & make_nvp("stereoMode",a.stereoMode);
	}
	else
	if (version>13)
	{
		LegacyStereoMode legacyStereoMode;
		bool legacyStereoSwap;
		ar & make_nvp("stereoMode",legacyStereoMode);
		ar & make_nvp("stereoSwap",legacyStereoSwap);
		a.stereoMode = (rr_gl::StereoMode)(legacyStereoMode*2+(legacyStereoSwap?3:2));
	}
	else
	if (version>11)
	{
		bool legacyStereoSwap;
		ar & make_nvp("stereoTopLineSeenByLeftEye",legacyStereoSwap);
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

BOOST_CLASS_VERSION(rr_gl::ImportParameters,1);
BOOST_CLASS_VERSION(rr_gl::UserPreferences::WindowLayout,2)
BOOST_CLASS_VERSION(rr_gl::UserPreferences,16)

//---------------------------------------------------------------------------

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// ImportParameters

ImportParameters::ImportParameters()
{
	unitEnum = U_M;
	unitFloat = 1;
	unitForce = false;
	up = 0;
	upForce = false;
	flipFrontBack = 3;
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
	return up;
}

/////////////////////////////////////////////////////////////////////////////
//
// UserPreferences save/load

static wxString suggestPreferencesDirectory()
{
#ifdef _WIN32
		#define APPDATA_SUBDIR "\\Lightsprint"
		// Vista, 7
		const wchar_t* appdata = _wgetenv(L"LOCALAPPDATA");
		if (appdata)
			return wxString(appdata) + APPDATA_SUBDIR;
		// XP
		const wchar_t* user = _wgetenv(L"USERPROFILE");
		if (user)
			return wxString(user) + "\\Local Settings\\Application Data" + APPDATA_SUBDIR;
		// unknown
		return APPDATA_SUBDIR;
#else
	// theoretically $HOME can be wrong, NSHomeDirectory() in OSX would be better
	const char* user = getenv("HOME");
	if (user)
		return wxString(user) + "/.Lightsprint";
	return ".Lightsprint";
#endif
}

static wxString suggestPreferencesFilename()
{
	return suggestPreferencesDirectory()
		+ "/SceneViewer.prefs";
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


}; // namespace

#endif // SUPPORT_SCENEVIEWER
