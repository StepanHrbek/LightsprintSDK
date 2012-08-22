// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVMATERIALPROPERTIES_H
#define SVMATERIALPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"

namespace rr_gl
{

	class SVMaterialProperties : public SVProperties
	{
	public:
		SVMaterialProperties(SVFrame* svframe);

		//! Copy material -> property (selected by clicking facegroup, honours physical flag, clears point flag).
		void setMaterial(rr::RRMaterial* material);

		//! Copy material -> property (selected by clicking pixel in viewport, honours point and physical flags).
		void setMaterial(rr::RRDynamicSolver* solver, rr::RRObject* object, unsigned hitTriangle, rr::RRVec2 hitPoint2d);

		//! Copy material -> property.
		void updateProperties();

		//! Defocus.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> material.
		void OnPropertyChange(wxPropertyGridEvent& event);

		bool              locked; // when locked, setMaterial() is ignored
	private:
		void updateHide();
		void updateReadOnly();

		rr::RRDynamicSolver* lastSolver;
		rr::RRObject*     lastObject;
		unsigned          lastTriangle;
		rr::RRVec2        lastPoint2d;

		rr::RRMaterial*   material; // materialCustom or materialPhysical or &materialPoint
	public:
		rr::RRMaterial*   materialCustom; // public only for SVFrame's ME_MATERIAL_SAVE/LOAD
	private:
		rr::RRMaterial*   materialPhysical;
		rr::RRPointMaterial materialPoint;

		bool              showPoint;
		bool              showPhysical;

		ButtonProperty*   propLoad;
		ButtonProperty*   propSave;
		ButtonProperty*   propSaveAll;
		wxPGProperty*     propPoint;
		wxPGProperty*     propPhysical;
		wxPGProperty*     propName;
		wxPGProperty*     propFront;
		wxPGProperty*     propBack;
		wxPGProperty*     propDiffuse;
		wxPGProperty*     propSpecular;
		wxPGProperty*     propSpecularModel;
		wxPGProperty*     propSpecularShininess;
		wxPGProperty*     propSpecularRoughness;
		wxPGProperty*     propEmissive;
		wxPGProperty*     propTransparent;
		wxPGProperty*     propTransparency1bit;
		wxPGProperty*     propTransparencyInAlpha;
		wxPGProperty*     propRefraction;
		wxPGProperty*     propBumpMap;
		wxPGProperty*     propBumpType;
		wxPGProperty*     propBumpMultiplier1;
		wxPGProperty*     propBumpMultiplier2;
		wxPGProperty*     propLightmapTexcoord;
		wxPGProperty*     propQualityForPoints;

		DECLARE_EVENT_TABLE()
	};


}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
