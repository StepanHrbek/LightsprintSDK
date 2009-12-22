// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVMATERIALPROPERTIES_H
#define SVMATERIALPROPERTIES_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"

namespace rr_gl
{

	class SVMaterialProperties : public wxPropertyGrid
	{
	public:
		SVMaterialProperties(wxWindow* parent, int precision);

		//! Copy material -> property.
		void setMaterial(rr::RRDynamicSolver* solver, unsigned hitTriangle, rr::RRVec2 hitPoint2d);

		//! Copy property -> material.
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		void updateReadOnly();

		rr::RRDynamicSolver* lastSolver;
		unsigned          lastTriangle;
		rr::RRVec2        lastPoint2d;

		rr::RRMaterial*   material; // materialCustom or materialPhysical or &materialPoint
		rr::RRMaterial*   materialCustom;
		rr::RRMaterial*   materialPhysical;
		rr::RRPointMaterial materialPoint;

		bool              showPoint;
		bool              showPhysical;
		bool              shown;

		wxPGProperty*     propPoint;
		wxPGProperty*     propPhysical;
		wxPGProperty*     propName;
		wxPGProperty*     propFront;
		wxPGProperty*     propBack;
		wxPGProperty*     propDiffuse;
		wxPGProperty*     propSpecular;
		wxPGProperty*     propEmissive;
		wxPGProperty*     propTransparent;
		wxPGProperty*     propTransparency1bit;
		wxPGProperty*     propTransparencyInAlpha;
		wxPGProperty*     propRefraction;
		wxPGProperty*     propLightmapTexcoord;
		wxPGProperty*     propQualityForPoints;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
