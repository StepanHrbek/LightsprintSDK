// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVLIGHTPROPERTIES_H
#define SVLIGHTPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_gl
{

	class SVLightProperties : public wxPropertyGrid
	{
	public:
		SVLightProperties(wxWindow* parent, const SceneViewerState& svs);

		//! Copy light -> property (all values, show/hide).
		void setLight(RealtimeLight* _rtlight, int _precision);

		//! Copy light -> property (show/hide).
		void updateHide();

		//! Copy light -> property (8 floats).
		//! Update pos+dir properties according to current light state.
		//! They may be changed outside dialog, this must be called to update values in dialog.
		void updatePosDir();

		//! Copy light -> property (8 floats).
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> light (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		const SceneViewerState& svs;

		RealtimeLight*    rtlight;

		wxPGProperty*     propName;
		wxPGProperty*     propType;
		wxPGProperty*     propPosition;
		wxPGProperty*     propDirection;
		wxPGProperty*     propOuterAngleRad;
		wxPGProperty*     propRadius;
		wxPGProperty*     propColor;
		wxPGProperty*     propTexture;
		wxPGProperty*     propTextureChangeAffectsGI;
		wxPGProperty*     propDistanceAttType;
		wxPGProperty*     propConstant;
		wxPGProperty*     propLinear;
		wxPGProperty*     propQuadratic;
		wxPGProperty*     propClamp;
		wxPGProperty*     propFallOffExponent;
		wxPGProperty*     propFallOffAngleRad;
		wxPGProperty*     propCastShadows;
		wxPGProperty*     propSpotExponent;
		wxPGProperty*     propShadowmapRes;
		wxPGProperty*     propShadowSamples;
		wxPGProperty*     propNear;
		wxPGProperty*     propFar;
		wxPGProperty*     propOrthoSize;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
