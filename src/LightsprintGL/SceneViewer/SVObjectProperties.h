// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVOBJECTPROPERTIES_H
#define SVOBJECTPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"

namespace rr_gl
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
		wxPGProperty*     propDynamic;
		wxPGProperty*     propLocation;
		wxPGProperty*     propCenter;
		wxPGProperty*     propTranslation;
		wxPGProperty*     propRotation;
		wxPGProperty*     propScale;
		wxPGProperty*     propIllumination;
		wxPGProperty*     propRealtimeEnvRes;
		wxPGProperty*     propBakedEnvRes;
		wxPGProperty*     propFacegroups;
		wxPGProperty*     propMesh;
		wxPGProperty*     propMeshUvs;
		wxPGProperty*     propMeshTangents;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
