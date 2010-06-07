// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVObjectProperties.h"
#include "SVCustomProperties.h"
#include "SVFrame.h" // updateSceneTree()
#include "../tmpstr.h"

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
	wxColour headerColor(230,230,230);

	if (_object!=object)
	{
		object = _object;
		Clear();
		if (object)
		{
			wxPGProperty* tmp;

			// mesh
			const rr::RRMesh* mesh = object->getCollider()->getMesh();
			rr::RRVec3 mini,maxi;
			mesh->getAABB(&mini,&maxi,&localCenter);

			// object
			Append(propName = new wxStringProperty(wxT("Name"),wxPG_LABEL,object->name.c_str()));
			const rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
			Append(tmp = new RRVec3Property(wxT("World center"),wxPG_LABEL,_precision,worldMatrix.transformedPosition(localCenter)));
			EnableProperty(tmp,false);
			Append(propWTranslation = new RRVec3Property(wxT("World translation"),wxPG_LABEL,_precision,worldMatrix.getTranslation()));

			// mesh
			const rr::RRMeshArrays* arrays = dynamic_cast<const rr::RRMeshArrays*>(mesh);
			Append(tmp = new wxStringProperty(wxT("Mesh"), wxPG_LABEL,tmpstr("%x",(int)(size_t)mesh)));
			AppendIn(tmp, new wxIntProperty(wxT("#triangles"),wxPG_LABEL,mesh->getNumTriangles()));
			AppendIn(tmp, new wxIntProperty(wxT("#vertices"),wxPG_LABEL,mesh->getNumVertices()));
			if (arrays)
			{
				wxString channels;
				for (unsigned i=0;i<arrays->texcoord.size();i++)
					if (arrays->texcoord[i])
						channels += tmpstr("%d ",i);
				AppendIn(tmp, new wxStringProperty(wxT("uv channels"),wxPG_LABEL,channels));
				AppendIn(tmp, new wxBoolProperty(wxT("tangents"),wxPG_LABEL,arrays->tangent?true:false));
				AppendIn(tmp, new wxIntProperty(wxT("version"),wxPG_LABEL,arrays->version));
			}
			AppendIn(tmp, new RRVec3Property(wxT("Local size"),wxPG_LABEL,_precision,maxi-mini));
			AppendIn(tmp, new RRVec3Property(wxT("Local min"),wxPG_LABEL,_precision,mini));
			AppendIn(tmp, new RRVec3Property(wxT("Local max"),wxPG_LABEL,_precision,maxi));
			AppendIn(tmp, new RRVec3Property(wxT("Local center"),wxPG_LABEL,_precision,localCenter));
			EnableProperty(tmp,false);
			SetPropertyBackgroundColour(tmp,headerColor,false);
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
		object->illumination.envMapWorldCenter = worldMatrix.transformedPosition(localCenter);
	}
}

BEGIN_EVENT_TABLE(SVObjectProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVObjectProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
