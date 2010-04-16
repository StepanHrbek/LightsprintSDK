// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSaveLoad.h"

#if _MSC_VER==1500 // save/load depends on boost. we have it installed only in 2008. you can change this if arbitrarily

/////////////////////////////////////////////////////////////////////////////
//
// boost::serialization support

#include "SVApp.h"
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>


namespace boost
{
namespace serialization
{

//------------------------------ RRVec2 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec2& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
}

//------------------------------ RRVec3 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec3& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
}

//------------------------------ RRVec4 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec4& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
	ar & make_nvp("w",a.w);
}

//------------------------------ RRLight ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRLight& a, const unsigned int version)
{
	ar & make_nvp("name",std::string(a.name.c_str()));
	ar & make_nvp("type",a.type);
	ar & make_nvp("position",a.position);
	ar & make_nvp("direction",a.direction);
	ar & make_nvp("outerAngleRad",a.outerAngleRad);
	ar & make_nvp("radius",a.radius);
	ar & make_nvp("color",a.color);
	ar & make_nvp("distanceAttenuationType",a.distanceAttenuationType);
	ar & make_nvp("polynom",a.polynom);
	ar & make_nvp("fallOffExponent",a.fallOffExponent);
	ar & make_nvp("spotExponent",a.spotExponent);
	ar & make_nvp("fallOffAngleRad",a.fallOffAngleRad);
	ar & make_nvp("castShadows",a.castShadows);
	ar & make_nvp("rtProjectedTextureFilename", std::string(a.rtProjectedTexture?a.rtProjectedTexture->filename.c_str():""));
	ar & make_nvp("rtMaxShadowSize",a.rtMaxShadowSize);
	// skip customData;
}

template<class Archive>
void load(Archive & ar, rr::RRLight& a, const unsigned int version)
{
	if (version>0)
	{
		std::string name;
		ar & make_nvp("name", name);
		a.name = name.c_str();
	}
	ar & make_nvp("type",a.type);
	ar & make_nvp("position",a.position);
	ar & make_nvp("direction",a.direction);
	ar & make_nvp("outerAngleRad",a.outerAngleRad);
	ar & make_nvp("radius",a.radius);
	ar & make_nvp("color",a.color);
	ar & make_nvp("distanceAttenuationType",a.distanceAttenuationType);
	ar & make_nvp("polynom",a.polynom);
	ar & make_nvp("fallOffExponent",a.fallOffExponent);
	ar & make_nvp("spotExponent",a.spotExponent);
	ar & make_nvp("fallOffAngleRad",a.fallOffAngleRad);
	ar & make_nvp("castShadows",a.castShadows);
	{
		std::string rtProjectedTextureFilenameString;
		ar & make_nvp("rtProjectedTextureFilename", rtProjectedTextureFilenameString);
		a.rtProjectedTexture = rr::RRBuffer::load(rtProjectedTextureFilenameString.c_str());
	}
	ar & make_nvp("rtMaxShadowSize",a.rtMaxShadowSize);
	// skip customData;
}

//----------------------------- RRLights ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRLights& a, const unsigned int version)
{
	unsigned count = a.size();
	ar & make_nvp("count",count);
	for (unsigned i=0;i<count;i++)
	{
		RR_ASSERT(a[i]);
		ar & make_nvp("light",*a[i]);
	}
}

template<class Archive>
void load(Archive & ar, rr::RRLights& a, const unsigned int version)
{
	RR_ASSERT(!a.size());
	unsigned count;
	ar & make_nvp("count",count);
	while (count--)
	{
		rr::RRLight* light = new rr::RRLight;
		ar & make_nvp("light", *light);
		a.push_back(light);
	}
}

//------------------------------ Camera -------------------------------------

template<class Archive>
void save(Archive & ar, const rr_gl::Camera& a, const unsigned int version)
{
	ar & make_nvp("pos",a.pos);
	ar & make_nvp("angle",a.angle);
	ar & make_nvp("leanAngle",a.leanAngle);
	ar & make_nvp("angleX",a.angleX);
	{
		float aspect = a.getAspect();
		ar & make_nvp("aspect",aspect);
	}
	{
		float fieldOfViewVerticalDeg = a.getFieldOfViewVerticalDeg();
		ar & make_nvp("fieldOfViewVerticalDeg",fieldOfViewVerticalDeg);
	}
	{
		float anear = a.getNear();
		ar & make_nvp("near",anear);
	}
	{
		float afar = a.getFar();
		ar & make_nvp("far",afar);
	}
	ar & make_nvp("orthogonal",a.orthogonal);
	ar & make_nvp("orthoSize",a.orthoSize);
	ar & make_nvp("updateDirFromAngles",a.updateDirFromAngles);
	ar & make_nvp("dir",a.dir);
}

template<class Archive>
void load(Archive & ar, rr_gl::Camera& a, const unsigned int version)
{
	ar & make_nvp("pos",a.pos);
	ar & make_nvp("angle",a.angle);
	ar & make_nvp("leanAngle",a.leanAngle);
	ar & make_nvp("angleX",a.angleX);
	{
		float aspect;
		ar & make_nvp("aspect",aspect);
		a.setAspect(aspect);
	}
	{
		float fieldOfViewVerticalDeg;
		ar & make_nvp("fieldOfViewVerticalDeg",fieldOfViewVerticalDeg);
		a.setFieldOfViewVerticalDeg(fieldOfViewVerticalDeg);
	}
	{
		float anear;
		ar & make_nvp("near",anear);
		a.setNear(anear);
	}
	{
		float afar;
		ar & make_nvp("far",afar);
		a.setFar(afar);
	}
	ar & make_nvp("orthogonal",a.orthogonal);
	ar & make_nvp("orthoSize",a.orthoSize);
	ar & make_nvp("updateDirFromAngles",a.updateDirFromAngles);
	ar & make_nvp("dir",a.dir);
}


//------------------------- SceneViewerStateEx ------------------------------

template<class Archive>
void save(Archive & ar, const rr_gl::SceneViewerStateEx& a, const unsigned int version)
{
	ar & make_nvp("eye",a.eye);
	ar & make_nvp("staticLayerNumber",a.staticLayerNumber);
	ar & make_nvp("realtimeLayerNumber",a.realtimeLayerNumber);
	ar & make_nvp("ldmLayerNumber",a.ldmLayerNumber);
	ar & make_nvp("selectedLightIndex",a.selectedLightIndex);
	ar & make_nvp("selectedObjectIndex",a.selectedObjectIndex);
	ar & make_nvp("fullscreen",a.fullscreen);
	ar & make_nvp("renderLightDirect",a.renderLightDirect);
	ar & make_nvp("renderLightIndirect",a.renderLightIndirect);
	ar & make_nvp("renderLightmaps2d",a.renderLightmaps2d);
	ar & make_nvp("renderLightmapsBilinear",a.renderLightmapsBilinear);
	ar & make_nvp("renderMaterialDiffuse",a.renderMaterialDiffuse);
	ar & make_nvp("renderMaterialSpecular",a.renderMaterialSpecular);
	ar & make_nvp("renderMaterialEmission",a.renderMaterialEmission);
	ar & make_nvp("renderMaterialTransparency",a.renderMaterialTransparency);
	ar & make_nvp("renderMaterialTextures",a.renderMaterialTextures);
	ar & make_nvp("renderWater",a.renderWater);
	ar & make_nvp("renderWireframe",a.renderWireframe);
	ar & make_nvp("renderFPS",a.renderFPS);
	ar & make_nvp("renderIcons",a.renderIcons);
	ar & make_nvp("renderHelpers",a.renderHelpers);
	ar & make_nvp("renderVignette",a.renderVignette);
	ar & make_nvp("renderHelp",a.renderHelp);
	ar & make_nvp("renderLogo",a.renderLogo);
	ar & make_nvp("renderTonemapping",a.renderTonemapping);
	ar & make_nvp("adjustTonemapping",a.adjustTonemapping);
	ar & make_nvp("playVideos",a.playVideos);
	ar & make_nvp("emissiveMultiplier",a.emissiveMultiplier);
	ar & make_nvp("videoEmittanceAffectsGI",a.videoEmittanceAffectsGI);
	ar & make_nvp("videoTransmittanceAffectsGI",a.videoTransmittanceAffectsGI);
	ar & make_nvp("cameraDynamicNear",a.cameraDynamicNear);
	ar & make_nvp("cameraMetersPerSecond",a.cameraMetersPerSecond);
	ar & make_nvp("brightness",a.brightness);
	ar & make_nvp("gamma",a.gamma);
	ar & make_nvp("waterLevel",a.waterLevel);
	ar & make_nvp("waterColor",a.waterColor);
	// skip autodetectCamera
	// skip initialInputSolver;
	// skip pathToShaders;
	ar & make_nvp("sceneFilename",a.sceneFilename);
	ar & make_nvp("skyboxFilename",a.skyboxFilename);
	// skip releaseResources;
}

template<class Archive>
void load(Archive& ar, rr_gl::SceneViewerStateEx& a, const unsigned int version)
{
	ar & make_nvp("eye",a.eye);
	ar & make_nvp("staticLayerNumber",a.staticLayerNumber);
	ar & make_nvp("realtimeLayerNumber",a.realtimeLayerNumber);
	ar & make_nvp("ldmLayerNumber",a.ldmLayerNumber);
	ar & make_nvp("selectedLightIndex",a.selectedLightIndex);
	ar & make_nvp("selectedObjectIndex",a.selectedObjectIndex);
	ar & make_nvp("fullscreen",a.fullscreen);
	ar & make_nvp("renderLightDirect",a.renderLightDirect);
	ar & make_nvp("renderLightIndirect",a.renderLightIndirect);
	ar & make_nvp("renderLightmaps2d",a.renderLightmaps2d);
	ar & make_nvp("renderLightmapsBilinear",a.renderLightmapsBilinear);
	ar & make_nvp("renderMaterialDiffuse",a.renderMaterialDiffuse);
	ar & make_nvp("renderMaterialSpecular",a.renderMaterialSpecular);
	ar & make_nvp("renderMaterialEmission",a.renderMaterialEmission);
	ar & make_nvp("renderMaterialTransparency",a.renderMaterialTransparency);
	ar & make_nvp("renderMaterialTextures",a.renderMaterialTextures);
	ar & make_nvp("renderWater",a.renderWater);
	ar & make_nvp("renderWireframe",a.renderWireframe);
	ar & make_nvp("renderFPS",a.renderFPS);
	ar & make_nvp("renderIcons",a.renderIcons);
	ar & make_nvp("renderHelpers",a.renderHelpers);
	ar & make_nvp("renderVignette",a.renderVignette);
	ar & make_nvp("renderHelp",a.renderHelp);
	ar & make_nvp("renderLogo",a.renderLogo);
	if (version>0) ar & make_nvp("renderTonemapping",a.renderTonemapping);
	ar & make_nvp("adjustTonemapping",a.adjustTonemapping);
	if (version>1)
	{
		ar & make_nvp("playVideos",a.playVideos);
		ar & make_nvp("emissiveMultiplier",a.emissiveMultiplier);
		ar & make_nvp("videoEmittanceAffectsGI",a.videoEmittanceAffectsGI);
		ar & make_nvp("videoTransmittanceAffectsGI",a.videoTransmittanceAffectsGI);
	}
	ar & make_nvp("cameraDynamicNear",a.cameraDynamicNear);
	ar & make_nvp("cameraMetersPerSecond",a.cameraMetersPerSecond);
	ar & make_nvp("brightness",a.brightness);
	ar & make_nvp("gamma",a.gamma);
	ar & make_nvp("waterLevel",a.waterLevel);
	if (version>0) ar & make_nvp("waterColor",a.waterColor);
	// skip autodetectCamera
	// skip initialInputSolver;
	// skip pathToShaders;
	ar & make_nvp("sceneFilename",a.sceneFilename);
	ar & make_nvp("skyboxFilename",a.skyboxFilename);
	// skip releaseResources;
}

//------------------------- UserPreferences ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_gl::UserPreferences::WindowLayout& a, const unsigned int version)
{
	ar & make_nvp("fullscreen",a.fullscreen);
	ar & make_nvp("maximized",a.maximized);
	ar & make_nvp("perspective",a.perspective);
}

template<class Archive>
void serialize(Archive & ar, rr_gl::UserPreferences& a, const unsigned int version)
{
	ar & make_nvp("currentWindowLayout",a.currentWindowLayout);
	ar & make_nvp("windowLayout",a.windowLayout);
}

//---------------------------------------------------------------------------

} // namespace
} // namespace

BOOST_SERIALIZATION_SPLIT_FREE(rr::RRLight)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRLights)
BOOST_SERIALIZATION_SPLIT_FREE(rr_gl::Camera)
BOOST_SERIALIZATION_SPLIT_FREE(rr_gl::SceneViewerStateEx)

BOOST_CLASS_VERSION(rr::RRLight, 1)
BOOST_CLASS_VERSION(rr_gl::SceneViewerStateEx, 3)

//---------------------------------------------------------------------------

#include "wx/wx.h"
#include "../tmpstr.h"
#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
namespace bf = boost::filesystem;


namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// UserPreferences save/load

static std::string suggestPreferencesDirectory()
{
	return std::string(getenv("LOCALAPPDATA")) + "\\Lightsprint";
}

static std::string suggestPreferencesFilename()
{
	return suggestPreferencesDirectory() + "\\SceneViewer.prefs";
}

bool UserPreferences::save() const
{
	try
	{
		_mkdir(suggestPreferencesDirectory().c_str());
		std::ofstream ofs(suggestPreferencesFilename().c_str());
		if (!ofs || ofs.bad())
		{
			rr::RRReporter::report(rr::WARN,"File %s can't be created, preferences not saved.\n",suggestPreferencesFilename().c_str());
			return false;
		}
		boost::archive::xml_oarchive ar(ofs);

		ar & boost::serialization::make_nvp("userPreferences", *this);
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save %s.\n",suggestPreferencesFilename().c_str());
		return false;
	}

	return true;
}

bool UserPreferences::load()
{
	try
	{
		std::ifstream ifs(suggestPreferencesFilename().c_str());
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
		rr::RRReporter::report(rr::ERRO,"Failed to load %s.\n",suggestPreferencesFilename().c_str());
		return false;
	}

	return true;
}


}; // namespace

#else // _MSC_VER==1500

// empty implementation for platforms with disabled boost
namespace rr_gl
{
	bool UserPreferences::save() const
	{
		return false;
	}
	bool UserPreferences::load()
	{
		return false;
	}
}

#endif // _MSC_VER==1500
#endif // SUPPORT_SCENEVIEWER
