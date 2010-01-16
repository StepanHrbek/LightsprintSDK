// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVObjectProperties.h"
#include "SVCustomProperties.h"
#include "SVFrame.h" // updateSceneTree()

namespace rr_gl
{

SVObjectProperties::SVObjectProperties(wxWindow* parent)
	: wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER )
{
	SV_SET_PG_COLORS;
	object = NULL;
}

void SVObjectProperties::setObject(rr::RRObject* _object, int _precision)
{
	if (_object!=object)
	{
		object = _object;
		Clear();
		if (object)
		{
			const rr::RRMesh* mesh = object->getCollider()->getMesh();
			wxPGProperty* tmp;

			Append(propName = new wxStringProperty(wxT("Name"),wxPG_LABEL,object->name.c_str()));

			Append(tmp = new wxIntProperty(wxT("#triangles"),wxPG_LABEL,mesh->getNumTriangles()));
			SetPropertyReadOnly(tmp,true);

			Append(tmp = new wxIntProperty(wxT("#vertices"),wxPG_LABEL,mesh->getNumVertices()));
			SetPropertyReadOnly(tmp,true);


			rr::RRMatrix3x4 worldMatrix;
			const rr::RRMatrix3x4* worldMatrixPtr = object->getWorldMatrix();
			if (worldMatrixPtr)
				worldMatrix = *worldMatrixPtr;
			else
				worldMatrix.setIdentity();
			rr::RRVec3 mini,maxi,center;
			mesh->getAABB(&mini,&maxi,&center);

			Append(tmp = new RRVec3Property(wxT("Local size"),wxPG_LABEL,_precision,maxi-mini));
			SetPropertyReadOnly(tmp,true);
			Append(tmp = new RRVec3Property(wxT("Local min"),wxPG_LABEL,_precision,mini));
			SetPropertyReadOnly(tmp,true);
			Append(tmp = new RRVec3Property(wxT("Local max"),wxPG_LABEL,_precision,maxi));
			SetPropertyReadOnly(tmp,true);
			Append(tmp = new RRVec3Property(wxT("Local center"),wxPG_LABEL,_precision,center));
			SetPropertyReadOnly(tmp,true);
			Append(tmp = new RRVec3Property(wxT("World center"),wxPG_LABEL,_precision,worldMatrix.transformedPosition(center)));
			SetPropertyReadOnly(tmp,true);

			Append(propWTranslation = new RRVec3Property(wxT("World translation"),wxPG_LABEL,_precision,worldMatrix.getTranslation()));
		}
	}
}

void SVObjectProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();
	if (property==propName)
	{
		object->name = property->GetValue().GetString().c_str();
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		frame->updateSceneTree();
	}
	else
	if (property==propWTranslation)
	{
		rr::RRMatrix3x4 worldMatrix;
		const rr::RRMatrix3x4* worldMatrixPtr = object->getWorldMatrix();
		if (worldMatrixPtr)
			worldMatrix = *worldMatrixPtr;
		else
			worldMatrix.setIdentity();
		RRVec3 newTranslation;
		newTranslation << property->GetValue();
		worldMatrix.setTranslation(newTranslation);
		object->setWorldMatrix(&worldMatrix);
	}
}

BEGIN_EVENT_TABLE(SVObjectProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVObjectProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
