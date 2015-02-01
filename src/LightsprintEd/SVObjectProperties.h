// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - object properties window.
// --------------------------------------------------------------------------

#ifndef SVOBJECTPROPERTIES_H
#define SVOBJECTPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"

namespace rr_ed
{

	class SVObjectProperties : public SVProperties
	{
	public:
		SVObjectProperties(SVFrame* svframe);

		//! Copy object -> property (all values, show/hide).
		void setObject(rr::RRObject* object, int precision);

		// Copy object -> 
		void updateHide();

		//! Copy object -> property (cube sizes).
		void updateProperties();

		//! Copy property -> object (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

		//! Select material in mat.props when facegroup is clicked.
		void OnPropertySelect(wxPropertyGridEvent& event);

		rr::RRObject*     object;
	private:
		unsigned          numFacegroups;
		rr::RRVec3        localCenter;

		wxPGProperty*     propName;
		BoolRefProperty*  propEnabled;
		wxPGProperty*     propDynamic;
		wxPGProperty*     propLocation;
		wxPGProperty*     propCenter;
		wxPGProperty*     propTranslation;
		wxPGProperty*     propRotation;
		wxPGProperty*     propScale;
		wxPGProperty*     propIllumination;
		wxPGProperty*     propIlluminationBakedLightmap;
		wxPGProperty*     propIlluminationBakedAmbient;
		wxPGProperty*     propIlluminationRealtimeAmbient;
		wxPGProperty*     propIlluminationBakedEnv;
		wxPGProperty*     propIlluminationRealtimeEnv;
		wxPGProperty*     propIlluminationBakedLDM;
		wxPGProperty*     propFacegroups;
		wxPGProperty*     propMesh;
		wxPGProperty*     propMeshTris;
		wxPGProperty*     propMeshVerts;
		wxPGProperty*     propMeshUvs;
		wxPGProperty*     propMeshTangents;
		wxPGProperty*     propMeshUnwrapChannel;
		wxPGProperty*     propMeshUnwrapSize;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
