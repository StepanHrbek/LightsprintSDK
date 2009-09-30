// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENEPROPERTIES_H
#define SVSCENEPROPERTIES_H

#include "SVApp.h"

#ifdef SUPPORT_SCENEVIEWER

#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"

namespace rr_gl
{

	class SVSceneProperties : public wxPropertyGrid
	{
	public:
		SVSceneProperties(wxWindow* parent, SceneViewerStateEx& svs);

		//! Copy svs -> property.
		void updateAllProperties();

		//! Copy volatile svs -> property.
		void updateVolatileProperties();

		//! Copy volatile svs -> property.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> svs.
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		//! Copy svs -> hide/show property.
		void updateHide();

		SceneViewerStateEx& svs;
		wxPGProperty*     propWater;
		wxPGProperty*     propWaterColor;
		wxPGProperty*     propWaterLevel;
		wxPGProperty*     propToneMapping;
		wxPGProperty*     propToneMappingBrightness;
		wxPGProperty*     propToneMappingContrast;
		wxPGProperty*     propToneMappingAutomatic;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
