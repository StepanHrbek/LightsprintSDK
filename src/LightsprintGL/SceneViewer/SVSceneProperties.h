// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENEPROPERTIES_H
#define SVSCENEPROPERTIES_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVApp.h"
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
		wxPGProperty*     propToneMapping;
		wxPGProperty*     propToneMappingAutomatic;
		wxPGProperty*     propToneMappingBrightness;
		wxPGProperty*     propToneMappingContrast;
		wxPGProperty*     propWater;
		wxPGProperty*     propWaterColor;
		wxPGProperty*     propWaterLevel;
		wxPGProperty*     propSizes;
		wxPGProperty*     propSizesSize;
		wxPGProperty*     propSizesMin;
		wxPGProperty*     propSizesMax;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
