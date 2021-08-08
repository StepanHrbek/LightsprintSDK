// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - scene properties window.
// --------------------------------------------------------------------------

#ifndef SVSCENEPROPERTIES_H
#define SVSCENEPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"
#include "SVCustomProperties.h"

namespace rr_ed
{

	class SVSceneProperties : public SVProperties
	{
	public:
		SVSceneProperties(SVFrame* svframe);

		//! Copy svs -> property.
		void updateProperties();

		//! Copy svs -> property, defocus.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> svs.
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		//! Copy svs -> hide/show property.
		void updateHide();

		wxPGProperty*     propCamera;
		BoolRefProperty*  propCameraStereo;
		wxPGProperty*     propCameraEyeSeparation;
		wxPGProperty*     propCameraDisplayDistance;
		BoolRefProperty*  propCameraPanorama;
		wxPGProperty*     propCameraPanoramaMode;
		FloatProperty*    propCameraPanoramaFovDeg;
		wxPGProperty*     propCameraPanoramaCoverage;
		FloatProperty*    propCameraPanoramaScale;
		BoolRefProperty*  propCameraDof;
		BoolRefProperty*  propCameraDofAccumulated;
		FloatProperty*    propCameraDofApertureDiameter;
		ImageFileProperty*propCameraDofApertureShape;
		wxPGProperty*     propCameraDofFocusDistance;
		BoolRefProperty*  propCameraDofAutomaticFocusDistance;
		FloatProperty*    propCameraDofNear;
		FloatProperty*    propCameraDofFar;
		wxPGProperty*     propCameraView;
		wxPGProperty*     propCameraSpeed;
		wxPGProperty*     propCameraPosition;
		wxPGProperty*     propCameraDirection;
		wxPGProperty*     propCameraAngles;
		wxPGProperty*     propCameraOrtho;
		wxPGProperty*     propCameraOrthoSize;
		wxPGProperty*     propCameraFov;
		wxPGProperty*     propCameraNear;
		wxPGProperty*     propCameraFar;
		BoolRefProperty*  propCameraRangeAutomatic;
		wxPGProperty*     propCameraRangeAutomaticNumRays;
		wxPGProperty*     propCameraCenter;

		wxPGProperty*     propEnv;
		//BoolRefProperty*  propEnvSimulateSky;
		BoolRefProperty*  propEnvSimulateSun;
		ImageFileProperty*propEnvMap;
		FloatProperty*    propEnvMapAngleDeg;
		LocationProperty* propEnvLocation;
		wxDateProperty*   propEnvDate;
		FloatProperty*    propEnvTime;
		FloatProperty*    propEnvSpeed;

		BoolRefProperty*  propToneMapping;
		BoolRefProperty*  propToneMappingAutomatic;
		wxPGProperty*     propToneMappingAutomaticTarget;
		wxPGProperty*     propToneMappingAutomaticSpeed;
		wxPGProperty*     propToneMappingBrightness;
		wxPGProperty*     propToneMappingContrast;
		wxPGProperty*     propToneMappingSaturation;
		wxPGProperty*     propToneMappingHueShift;
		wxPGProperty*     propToneMappingSteps;
		wxPGProperty*     propToneMappingColor;

		wxPGProperty*     propRenderMaterials;
		BoolRefProperty*  propRenderMaterialDiffuse;
		BoolRefProperty*  propRenderMaterialDiffuseColor;
		BoolRefProperty*  propRenderMaterialSpecular;
		BoolRefProperty*  propRenderMaterialEmittance;
		wxPGProperty*     propRenderMaterialTransparency;
		BoolRefProperty*  propRenderMaterialTransparencyNoise;
		BoolRefProperty*  propRenderMaterialTransparencyFresnel;
		BoolRefProperty*  propRenderMaterialTransparencyRefraction;
		BoolRefProperty*  propRenderMaterialBumpMaps;
		BoolRefProperty*  propRenderMaterialTextures;
		BoolRefProperty*  propRenderMaterialSidedness;

		wxPGProperty*     propRenderExtras;
		BoolRefProperty*  propRenderContours;
		HDRColorProperty* propRenderContoursSilhouette;
		HDRColorProperty* propRenderContoursCrease;
		BoolRefProperty*  propRenderWireframe;
		BoolRefProperty*  propRenderHelpers;
		BoolRefProperty*  propRenderFPS;

		BoolRefProperty*  propLogo;

		BoolRefProperty*  propBloom;
		FloatProperty*    propBloomThreshold;

		BoolRefProperty*  propLensFlare;
		FloatProperty*    propLensFlareSize;
		FloatProperty*    propLensFlareId;

		BoolRefProperty*  propVignette;

		BoolRefProperty*  propGrid;
		wxPGProperty*     propGridNumSegments;
		wxPGProperty*     propGridSegmentSize;


		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
