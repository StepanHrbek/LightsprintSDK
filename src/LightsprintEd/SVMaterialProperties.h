// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - material properties window.
// --------------------------------------------------------------------------

#ifndef SVMATERIALPROPERTIES_H
#define SVMATERIALPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"

namespace rr_ed
{

	class SVMaterialProperties : public SVProperties
	{
	public:
		SVMaterialProperties(SVFrame* svframe);

		//! Copy material -> property (selected by clicking facegroup, honours physical flag, clears point flag).
		void setMaterial(rr::RRMaterial* material);

		//! Copy material -> property (selected by clicking pixel in viewport, honours point and physical flags).
		void setMaterial(rr::RRSolver* solver, rr::RRObject* object, unsigned hitTriangle, rr::RRVec2 hitPoint2d);

		//! Copy material -> property.
		void updateProperties();
		void updateHelp();

		//! Defocus.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> material.
		void OnPropertyChange(wxPropertyGridEvent& event);


		bool              locked; // when locked, setMaterial() is ignored
	private:
		void updateHide();
		void updateReadOnly();
		void updateIsInStaticScene(); // updates materialIsInStaticScene, called from setMaterial()

		rr::RRSolver* lastSolver;
		rr::RRObject*     lastObject;
		unsigned          lastTriangle;
		rr::RRVec2        lastPoint2d;

	public:
		rr::RRMaterial*   material; // materialCustom or materialPhysical or &materialPoint. public only for SVFrame's ME_MATERIAL_SAVE/LOAD
	private:
		rr::RRPointMaterial materialPoint;
		bool              materialIsInStaticScene; // optimization: we don't report material change if material is not used in static scene (GI does not need update)

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
		wxPGProperty*     propTransparencyThreshold;
		wxPGProperty*     propTransparencyInAlpha;
		wxPGProperty*     propTransparencyMapInverted;
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

#endif
