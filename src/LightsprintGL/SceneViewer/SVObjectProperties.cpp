// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVObjectProperties.h"
#include "SVCustomProperties.h"
#include "SVFrame.h" // updateSceneTree()

namespace rr_gl
{

SVObjectProperties::SVObjectProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
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
			Append(propName = new wxStringProperty(_("Name"),wxPG_LABEL,object->name.c_str()));
			const rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
			Append(tmp = new RRVec3Property(_("World center"),_("Center of object in world space"),_precision,worldMatrix.transformedPosition(localCenter)));
			EnableProperty(tmp,false);
			Append(propWTranslation = new RRVec3Property(_("World translation"),_("Translation of object in world space"),_precision,worldMatrix.getTranslation()));
			Append(propCubeDiffuse = new FloatProperty("Diffuse cube size",_("Size of realtime raytraced diffuse reflection cubemap"),object->illumination.diffuseEnvMap?object->illumination.diffuseEnvMap->getWidth():0,_precision,0,100000,1,false));
			EnableProperty(propCubeDiffuse,false);
			Append(propCubeSpecular = new FloatProperty("Specular cube size",_("Size of realtime raytraced specular reflection cubemap"),object->illumination.specularEnvMap?object->illumination.specularEnvMap->getWidth():0,_precision,0,100000,1,false));
			EnableProperty(propCubeSpecular,false);
			Append(new wxIntProperty(_("#facegroups"),wxPG_LABEL,object->faceGroups.size()));

			// mesh
			const rr::RRMeshArrays* arrays = dynamic_cast<const rr::RRMeshArrays*>(mesh);
			Append(tmp = new wxStringProperty(_("Mesh"), wxPG_LABEL,wxString::Format("%x",(int)(size_t)mesh)));
			AppendIn(tmp, new wxIntProperty(_("#triangles"),wxPG_LABEL,mesh->getNumTriangles()));
			AppendIn(tmp, new wxIntProperty(_("#vertices"),wxPG_LABEL,mesh->getNumVertices()));
			if (arrays)
			{
				wxString channels;
				for (unsigned i=0;i<arrays->texcoord.size();i++)
					if (arrays->texcoord[i])
						channels += wxString::Format("%d ",i);
				AppendIn(tmp, new wxStringProperty(_("uv channels"),wxPG_LABEL,channels));
				AppendIn(tmp, new wxBoolProperty(_("tangents"),wxPG_LABEL,arrays->tangent?true:false));
				AppendIn(tmp, new wxIntProperty(_("version"),wxPG_LABEL,arrays->version));
			}
			AppendIn(tmp, new RRVec3Property(_("Local size"),_("Mesh size in object space"),_precision,maxi-mini));
			AppendIn(tmp, new RRVec3Property(_("Local min"),_("Mesh AABB min in object space"),_precision,mini));
			AppendIn(tmp, new RRVec3Property(_("Local max"),_("Mesh AABB max in object space"),_precision,maxi));
			AppendIn(tmp, new RRVec3Property(_("Local center"),_("Mesh center in object space"),_precision,localCenter));
			EnableProperty(tmp,false);
			SetPropertyBackgroundColour(tmp,headerColor,false);
		}
	}
}

void SVObjectProperties::updateProperties()
{
	if (object)
	{
		updateFloat(propCubeDiffuse,object->illumination.diffuseEnvMap?object->illumination.diffuseEnvMap->getWidth():0);
		updateFloat(propCubeSpecular,object->illumination.specularEnvMap?object->illumination.specularEnvMap->getWidth():0);
	}
}

void SVObjectProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();
	if (property==propName)
	{
		object->name = property->GetValue().GetString().c_str();
		svframe->updateSceneTree();
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
