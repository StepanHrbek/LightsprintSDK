// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVLIGHTPROPERTIES_H
#define SVLIGHTPROPERTIES_H

#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/GL/SceneViewer.h"

namespace rr_gl
{

	class SVLightProperties : public wxDialog
	{
	public:
		SVLightProperties( wxWindow* parent );

		//! Copy light -> property (all values, show/hide).
		void setLight(RealtimeLight* _rtlight);

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

		//! When user closes lightprops dialog, this deletes it (default behaviour=hide it=troubles).
		void OnClose(wxCloseEvent& event);

	private:
		RealtimeLight*    rtlight;
		wxPropertyGrid*   pg;
		wxPGProperty*     propType;
		wxPGProperty*     propPosition;
		wxPGProperty*     propDirection;
		wxPGProperty*     propOuterAngleRad;
		wxPGProperty*     propRadius;
		wxPGProperty*     propColor;
		wxPGProperty*     propTexture;
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
		wxPGProperty*     propNear;
		wxPGProperty*     propFar;
		wxPGProperty*     propOrthoSize;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
