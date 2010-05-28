// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENEPROPERTIES_H
#define SVSCENEPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVApp.h"
#include "SVCustomProperties.h"
#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"

namespace rr_gl
{

	class SVSceneProperties : public wxPropertyGrid
	{
	public:
		SVSceneProperties(wxWindow* parent, SceneViewerStateEx& svs);

		//! Copy svs -> property.
		void updateProperties();

		//! Copy svs -> property.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> svs.
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		//! Copy svs -> hide/show property.
		void updateHide();

		SceneViewerStateEx& svs;
		wxPGProperty*     propCamera;
		wxPGProperty*     propCameraSpeed;
		wxPGProperty*     propCameraPosition;
		wxPGProperty*     propCameraDirection;
		wxPGProperty*     propCameraAngles;
		wxPGProperty*     propCameraFov;
		wxPGProperty*     propCameraNear;
		wxPGProperty*     propCameraFar;

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
		BoolRefProperty*  propRenderMaterialTransparency;
		BoolRefProperty*  propRenderMaterialTextures;

		wxPGProperty*     propRenderExtras;
		BoolRefProperty*  propRenderWireframe;
		BoolRefProperty*  propRenderHelpers;
		BoolRefProperty*  propRenderFPS;
		BoolRefProperty*  propRenderLogo;
		BoolRefProperty*  propRenderVignettation;

		BoolRefProperty*  propWater;
		wxPGProperty*     propWaterColor;
		wxPGProperty*     propWaterLevel;

		BoolRefProperty*  propGrid;
		wxPGProperty*     propGridNumSegments;
		wxPGProperty*     propGridSegmentSize;

		wxPGProperty*     propGI;
		wxPGProperty*     propGIEmisMultiplier;
		BoolRefProperty*  propGIEmisVideoAffectsGI;
		BoolRefProperty*  propGITranspVideoAffectsGI;

		/*
		wxPGProperty*     propSizes;
		wxPGProperty*     propSizesSize;
		wxPGProperty*     propSizesMin;
		wxPGProperty*     propSizesMax;
		*/

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
