// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSaveLoad.h"


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
	ar & make_nvp("enabled",a.enabled);
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
	ar & make_nvp("rtProjectedTextureFilename",a.rtProjectedTexture?bf::system_complete(a.rtProjectedTexture->filename.c_str()).file_string():""); // must be absolute, otherwise load may fail, load relocator would not have complete information
	ar & make_nvp("rtNumShadowmaps",a.rtNumShadowmaps);
	ar & make_nvp("rtShadowmapSize",a.rtShadowmapSize);
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
	if (version>2)
	{
		ar & make_nvp("enabled", a.enabled);
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
	if (version<2)
	{
		float rtMaxShadowSize;
		ar & make_nvp("rtMaxShadowSize",rtMaxShadowSize);
	}
	else
	{
		ar & make_nvp("rtNumShadowmaps",a.rtNumShadowmaps);
	}
	if (version>3)
	{
		ar & make_nvp("rtShadowmapSize",a.rtShadowmapSize);
	}
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

//------------------------------ CalculateParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRDynamicSolver::CalculateParameters& a, const unsigned int version)
{
	ar & make_nvp("materialEmittanceMultiplier",a.materialEmittanceMultiplier);
	ar & make_nvp("materialEmittanceStaticQuality",a.materialEmittanceStaticQuality);
	ar & make_nvp("materialEmittanceVideoQuality",a.materialEmittanceVideoQuality);
	ar & make_nvp("materialEmittanceUsePointMaterials",a.materialEmittanceUsePointMaterials);
	ar & make_nvp("materialTransmittanceStaticQuality",a.materialTransmittanceStaticQuality);
	ar & make_nvp("materialTransmittanceVideoQuality",a.materialTransmittanceVideoQuality);
	ar & make_nvp("environmentStaticQuality",a.environmentStaticQuality);
	ar & make_nvp("environmentVideoQuality",a.environmentVideoQuality);
	ar & make_nvp("qualityIndirectDynamic",a.qualityIndirectDynamic);
	ar & make_nvp("qualityIndirectStatic",a.qualityIndirectStatic);
	ar & make_nvp("secondsBetweenDDI",a.secondsBetweenDDI);
}

//------------------------------ UpdateParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRDynamicSolver::UpdateParameters& a, const unsigned int version)
{
	ar & make_nvp("applyLights",a.applyLights);
	ar & make_nvp("applyEnvironment",a.applyEnvironment);
	ar & make_nvp("applyCurrentSolution",a.applyCurrentSolution);
	ar & make_nvp("quality",a.quality);
	ar & make_nvp("qualityFactorRadiosity",a.qualityFactorRadiosity);
	ar & make_nvp("insideObjectsTreshold",a.insideObjectsTreshold);
	ar & make_nvp("rugDistance",a.rugDistance);
	ar & make_nvp("locality",a.locality);
	ar & make_nvp("lowDetailForLightDetailMap",a.lowDetailForLightDetailMap);
	ar & make_nvp("measure_internal",a.measure_internal);
}

//------------------------------ FilteringParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRDynamicSolver::FilteringParameters& a, const unsigned int version)
{
	ar & make_nvp("smoothingAmount",a.smoothingAmount);
	ar & make_nvp("spreadForegroundColor",a.spreadForegroundColor);
	ar & make_nvp("backgroundColor",a.backgroundColor);
	ar & make_nvp("wrap",a.wrap);
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
	ar & make_nvp("screenCenter",a.screenCenter);
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
	if (a.orthoSize<=0)
		a.orthoSize = 100; // cameras used to be initialized with orthoSize=0, fix it
	if (version>0)
		ar & make_nvp("screenCenter",a.screenCenter);
	ar & make_nvp("updateDirFromAngles",a.updateDirFromAngles);
	ar & make_nvp("dir",a.dir);
}


//------------------------------ tm -----------------------------------

template<class Archive>
void serialize(Archive & ar, tm& a, const unsigned int version)
{
	a.tm_year += 1900;
	a.tm_mon += 1;
	ar & make_nvp("year",a.tm_year);
	ar & make_nvp("month",a.tm_mon); // 1..12
	ar & make_nvp("day",a.tm_mday); // 1..31
	ar & make_nvp("hour",a.tm_hour);
	ar & make_nvp("minute",a.tm_min);
	ar & make_nvp("second",a.tm_sec);
	a.tm_mon -= 1;
	a.tm_year -= 1900;
}

//------------------------- SceneViewerStateEx ------------------------------

template<class Archive>
void save(Archive & ar, const rr_gl::SceneViewerStateEx& a, const unsigned int version)
{
	ar & make_nvp("eye",a.eye);

	ar & make_nvp("envSimulateSky",a.envSimulateSky);
	ar & make_nvp("envSimulateSun",a.envSimulateSun);
	ar & make_nvp("envLongitude",a.envLongitudeDeg);
	ar & make_nvp("envLatitude",a.envLatitudeDeg);
	ar & make_nvp("envDateTime",a.envDateTime);

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
	ar & make_nvp("renderBloom",a.renderBloom);
	ar & make_nvp("renderLensFlare",a.renderLensFlare);
	ar & make_nvp("lensFlareSize",a.lensFlareSize);
	ar & make_nvp("lensFlareId",a.lensFlareId);
	ar & make_nvp("renderVignette",a.renderVignette);
	ar & make_nvp("renderHelp",a.renderHelp);
	ar & make_nvp("renderLogo",a.renderLogo);
	ar & make_nvp("renderTonemapping",a.renderTonemapping);
	ar & make_nvp("adjustTonemapping",a.tonemappingAutomatic);
	ar & make_nvp("tonemappingAutomaticTarget",a.tonemappingAutomaticTarget);
	ar & make_nvp("tonemappingAutomaticSpeed",a.tonemappingAutomaticSpeed);
	ar & make_nvp("playVideos",a.playVideos);
	ar & make_nvp("emissiveMultiplier",a.emissiveMultiplier);
	ar & make_nvp("videoEmittanceAffectsGI",a.videoEmittanceAffectsGI);
	ar & make_nvp("videoEmittanceGIQuality",a.videoEmittanceGIQuality);
	ar & make_nvp("videoTransmittanceAffectsGI",a.videoTransmittanceAffectsGI);
	ar & make_nvp("videoTransmittanceAffectsGIFull",a.videoTransmittanceAffectsGIFull);
	ar & make_nvp("videoEnvironmentAffectsGI",a.videoEnvironmentAffectsGI);
	ar & make_nvp("videoEnvironmentGIQuality",a.videoEnvironmentGIQuality);
	ar & make_nvp("fireballQuality",a.fireballQuality);
	ar & make_nvp("raytracedCubesEnabled",a.raytracedCubesEnabled);
	ar & make_nvp("raytracedCubesDiffuseRes",a.raytracedCubesDiffuseRes);
	ar & make_nvp("raytracedCubesSpecularRes",a.raytracedCubesSpecularRes);
	ar & make_nvp("raytracedCubesMaxObjects",a.raytracedCubesMaxObjects);
	ar & make_nvp("lightmapFilteringParameters",a.lightmapFilteringParameters);
	ar & make_nvp("cameraDynamicNear",a.cameraDynamicNear);
	ar & make_nvp("cameraMetersPerSecond",a.cameraMetersPerSecond);
	ar & make_nvp("brightness",a.tonemappingBrightness);
	ar & make_nvp("gamma",a.tonemappingGamma);
	ar & make_nvp("waterLevel",a.waterLevel);
	ar & make_nvp("waterColor",a.waterColor);
	ar & make_nvp("renderGrid",a.renderGrid);
	ar & make_nvp("gridNumSegments",a.gridNumSegments);
	ar & make_nvp("gridSegmentSize",a.gridSegmentSize);
	// skip autodetectCamera
	// skip initialInputSolver;
	// skip pathToShaders;
	ar & make_nvp("sceneFilename",bf::system_complete(a.sceneFilename).file_string()); // must be absolute, otherwise load may fail, load relocator would not have complete information
	ar & make_nvp("skyboxFilename",bf::system_complete(a.skyboxFilename).file_string()); // must be absolute, otherwise load may fail, load relocator would not have complete information
}

template<class Archive>
void load(Archive& ar, rr_gl::SceneViewerStateEx& a, const unsigned int version)
{
	ar & make_nvp("eye",a.eye);
	if (version>9)
	{
		ar & make_nvp("envSimulateSky",a.envSimulateSky);
		ar & make_nvp("envSimulateSun",a.envSimulateSun);
		ar & make_nvp("envLongitude",a.envLongitudeDeg);
		ar & make_nvp("envLatitude",a.envLatitudeDeg);
		ar & make_nvp("envDateTime",a.envDateTime);
	}
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
	if (version>9)
	{
		ar & make_nvp("renderBloom",a.renderBloom);
		ar & make_nvp("renderLensFlare",a.renderLensFlare);
		ar & make_nvp("lensFlareSize",a.lensFlareSize);
		ar & make_nvp("lensFlareId",a.lensFlareId);
	}
	ar & make_nvp("renderVignette",a.renderVignette);
	ar & make_nvp("renderHelp",a.renderHelp);
	ar & make_nvp("renderLogo",a.renderLogo);
	if (version>0)
	{
		ar & make_nvp("renderTonemapping",a.renderTonemapping);
	}
	ar & make_nvp("adjustTonemapping",a.tonemappingAutomatic);
	if (version>4)
	{
		ar & make_nvp("tonemappingAutomaticTarget",a.tonemappingAutomaticTarget);
		ar & make_nvp("tonemappingAutomaticSpeed",a.tonemappingAutomaticSpeed);
	}
	if (version>1)
	{
		ar & make_nvp("playVideos",a.playVideos);
		ar & make_nvp("emissiveMultiplier",a.emissiveMultiplier);
		ar & make_nvp("videoEmittanceAffectsGI",a.videoEmittanceAffectsGI);
	}
	if (version>13)
	{
		ar & make_nvp("videoEmittanceGIQuality",a.videoEmittanceGIQuality);
	}
	if (version>1)
	{
		ar & make_nvp("videoTransmittanceAffectsGI",a.videoTransmittanceAffectsGI);
	}
	if (version>15)
	{
		ar & make_nvp("videoTransmittanceAffectsGIFull",a.videoTransmittanceAffectsGIFull);
	}
	if (version>13)
	{
		ar & make_nvp("videoEnvironmentAffectsGI",a.videoEnvironmentAffectsGI);
		ar & make_nvp("videoEnvironmentGIQuality",a.videoEnvironmentGIQuality);
	}
	if (version>11)
	{
		ar & make_nvp("fireballQuality",a.fireballQuality);
		ar & make_nvp("raytracedCubesEnabled",a.raytracedCubesEnabled);
		ar & make_nvp("raytracedCubesDiffuseRes",a.raytracedCubesDiffuseRes);
		ar & make_nvp("raytracedCubesSpecularRes",a.raytracedCubesSpecularRes);
		ar & make_nvp("raytracedCubesMaxObjects",a.raytracedCubesMaxObjects);
	}
	if (version>17)
	{
		ar & make_nvp("lightmapFilteringParameters",a.lightmapFilteringParameters);
	}
	ar & make_nvp("cameraDynamicNear",a.cameraDynamicNear);
	ar & make_nvp("cameraMetersPerSecond",a.cameraMetersPerSecond);
	ar & make_nvp("brightness",a.tonemappingBrightness);
	ar & make_nvp("gamma",a.tonemappingGamma);
	ar & make_nvp("waterLevel",a.waterLevel);
	if (version>0)
	{
		ar & make_nvp("waterColor",a.waterColor);
	}
	if (version>3)
	{
		ar & make_nvp("renderGrid",a.renderGrid);
		ar & make_nvp("gridNumSegments",a.gridNumSegments);
		ar & make_nvp("gridSegmentSize",a.gridSegmentSize);
	}
	// skip autodetectCamera
	// skip initialInputSolver;
	// skip pathToShaders;
	ar & make_nvp("sceneFilename",a.sceneFilename);
	ar & make_nvp("skyboxFilename",a.skyboxFilename);
	// skip releaseResources;
}

//------------------------- ImportParameters ------------------------------

template<class Archive>
void serialize(Archive & ar, rr_gl::ImportParameters& a, const unsigned int version)
{
	ar & make_nvp("unitEnum",a.unitEnum);
	ar & make_nvp("unitFloat",a.unitFloat);
	ar & make_nvp("unitForce",a.unitForce);
	ar & make_nvp("up",a.up);
	ar & make_nvp("upForce",a.upForce);
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
	// Versions 0..2 may have fewer panels, don't read them,
	//  it seems it's not safe to tell wx to restore old layout that had fewer panels
	//  (Douglas reports F11 not working after running old RL version).
	if (version<3)
		throw 1;

	ar & make_nvp("currentWindowLayout",a.currentWindowLayout);
	ar & make_nvp("windowLayout",a.windowLayout);
	if (version>3)
	{
		ar & make_nvp("import",a.import);
	}
	if (version>2)
	{
		ar & make_nvp("sshotFilename",a.sshotFilename);
		ar & make_nvp("sshotEnhanced",a.sshotEnhanced);
		ar & make_nvp("sshotEnhancedWidth",a.sshotEnhancedWidth);
		ar & make_nvp("sshotEnhancedHeight",a.sshotEnhancedHeight);
		ar & make_nvp("sshotEnhancedFSAA",a.sshotEnhancedFSAA);
		ar & make_nvp("sshotEnhancedShadowResolutionFactor",a.sshotEnhancedShadowResolutionFactor);
		ar & make_nvp("sshotEnhancedShadowSamples",a.sshotEnhancedShadowSamples);
	}
}

//---------------------------------------------------------------------------

} // namespace
} // namespace

BOOST_SERIALIZATION_SPLIT_FREE(rr::RRLight)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRLights)
BOOST_SERIALIZATION_SPLIT_FREE(rr_gl::Camera)
BOOST_SERIALIZATION_SPLIT_FREE(rr_gl::SceneViewerStateEx)

BOOST_CLASS_VERSION(rr::RRLight, 4)
BOOST_CLASS_VERSION(rr_gl::Camera, 1)
BOOST_CLASS_VERSION(rr_gl::UserPreferences, 4) // must be increased also each time panel is added/removed
BOOST_CLASS_VERSION(rr_gl::SceneViewerStateEx, 19)

//---------------------------------------------------------------------------

#include "wx/wx.h"
#include "../tmpstr.h"
#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#ifdef _WIN32
	#include <shlobj.h> // SHGetSpecialFolderPath
#endif
namespace bf = boost::filesystem;

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
}

float ImportParameters::getUnitLength(const char* filename) const
{
	wxString ext = wxString(filename).Right(4).Lower();
	bool alreadyNormalized = ext==".rr3" || ext==".dae" || ext==".kmz";
	if (alreadyNormalized && !unitForce)
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

unsigned ImportParameters::getUpAxis(const char* filename) const
{
	wxString ext = wxString(filename).Right(4).Lower();
	bool alreadyNormalized = ext!=".obj" && ext!=".stl";
	if (alreadyNormalized && !upForce)
		return 1;
	return up;
}

/////////////////////////////////////////////////////////////////////////////
//
// UserPreferences save/load

static std::wstring suggestPreferencesDirectory()
{
#ifdef _WIN32
		#define APPDATA_SUBDIR L"\\Lightsprint"
		// Vista, 7
		const wchar_t* appdata = _wgetenv(L"LOCALAPPDATA");
		if (appdata)
			return std::wstring(appdata) + APPDATA_SUBDIR;
		// XP
		const wchar_t* user = _wgetenv(L"USERPROFILE");
		if (user)
			return std::wstring(user) + L"\\Local Settings\\Application Data" + APPDATA_SUBDIR;
		// unknown
		return APPDATA_SUBDIR;
#else
	return L".";
#endif
}

static std::wstring suggestPreferencesFilename()
{
	return suggestPreferencesDirectory() + L"\\SceneViewer.prefs";
}

bool UserPreferences::save() const
{
#if defined(_MSC_VER) && _MSC_VER<1400
	return false; // Visual Studio 2003 doesn't accept std::ofstream(const wchar_t*)
#else
	try
	{
		bf::create_directories(suggestPreferencesDirectory());
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
		rr::RRReporter::report(rr::ERRO,"Failed to save %S.\n",suggestPreferencesFilename().c_str());
		return false;
	}

	return true;
#endif
}

bool UserPreferences::load()
{
#if defined(_MSC_VER) && _MSC_VER<1400
	return false; // Visual Studio 2003 doesn't accept std::ofstream(const wchar_t*)
#else
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
		rr::RRReporter::report(rr::ERRO,"Failed to load %S.\n",suggestPreferencesFilename().c_str());
		return false;
	}

	return true;
#endif
}


}; // namespace

#endif // SUPPORT_SCENEVIEWER
