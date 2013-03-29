// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENEPROPERTIES_H
#define SVSCENEPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"
#include "SVCustomProperties.h"

namespace rr_gl
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
		wxPGProperty*     propCameraFocalLength;
		BoolRefProperty*  propCameraDof;
		BoolRefProperty*  propCameraDofAutomatic;
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

		wxPGProperty*     propRenderMaterials;
		BoolRefProperty*  propRenderMaterialDiffuse;
		BoolRefProperty*  propRenderMaterialSpecular;
		BoolRefProperty*  propRenderMaterialEmittance;
		wxPGProperty*     propRenderMaterialTransparency;
		BoolRefProperty*  propRenderMaterialTransparencyFresnel;
		BoolRefProperty*  propRenderMaterialTransparencyRefraction;
		BoolRefProperty*  propRenderMaterialBumpMaps;
		BoolRefProperty*  propRenderMaterialTextures;
		BoolRefProperty*  propRenderMaterialSidedness;

		wxPGProperty*     propRenderExtras;
		BoolRefProperty*  propRenderWireframe;
		BoolRefProperty*  propRenderHelpers;
		BoolRefProperty*  propRenderFPS;

		BoolRefProperty*  propLogo;

		BoolRefProperty*  propRenderBloom;

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

#endif // SUPPORT_SCENEVIEWER

#endif
