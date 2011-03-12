// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVObjectProperties.h"
#include "SVCustomProperties.h"
#include "SVMaterialProperties.h"
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
	if (_object!=object)
	{
		object = _object;
		Clear();
		if (object)
		{
			const rr::RRMesh* mesh = object->getCollider()->getMesh();
			rr::RRVec3 mini,maxi;
			mesh->getAABB(&mini,&maxi,&localCenter);

			Append(propName = new wxStringProperty(_("Name"),wxPG_LABEL,RR2WX(object->name)));

			// location
			Append(propLocation = new wxStringProperty(_("Location"),wxPG_LABEL));
			EnableProperty(propLocation,false);
			const rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
			AppendIn(propLocation, propCenter = new RRVec3Property(_("World center"),_("Center of object in world space"),_precision,worldMatrix.transformedPosition(localCenter)));
			EnableProperty(propCenter,false);
			AppendIn(propLocation, propTranslation = new RRVec3Property(_("World translation"),_("Translation of object in world space"),_precision,worldMatrix.getTranslation()));
			AppendIn(propLocation, propScale = new RRVec3Property(_("Scale"),_("How many times object is bigger than mesh"),_precision,worldMatrix.getScale()));

			// illumination
			Append(propIllumination = new wxStringProperty(_("Illumination"),wxPG_LABEL));
			EnableProperty(propIllumination,false);
			AppendIn(propIllumination, propCubeDiffuse = new FloatProperty("Diffuse cube size",_("Size of realtime raytraced diffuse reflection cubemap"),object->illumination.diffuseEnvMap?object->illumination.diffuseEnvMap->getWidth():0,_precision,0,100000,1,false));
			EnableProperty(propCubeDiffuse,false);
			AppendIn(propIllumination, propCubeSpecular = new FloatProperty("Specular cube size",_("Size of realtime raytraced specular reflection cubemap"),object->illumination.specularEnvMap?object->illumination.specularEnvMap->getWidth():0,_precision,0,100000,1,false));
			EnableProperty(propCubeSpecular,false);

			// facegroups
			Append(propFacegroups = new wxIntProperty(_("Materials"),wxPG_LABEL,object->faceGroups.size()));
			EnableProperty(propFacegroups,false);
			for (unsigned fg=0;fg<object->faceGroups.size();fg++)
			{
				rr::RRObject::FaceGroup& faceGroup = object->faceGroups[fg];
				wxPGProperty* tmp = new wxStringProperty(
					wxString::Format("%d triangles",faceGroup.numTriangles),
					wxPG_LABEL,
					wxString::Format("%s 0x%x",wxString(faceGroup.material?RR2WX(faceGroup.material->name):""),(unsigned)(intptr_t)(faceGroup.material))
					);
				AppendIn(propFacegroups, tmp);
				EnableProperty(tmp,false);
			}

			// mesh
			const rr::RRMeshArrays* arrays = dynamic_cast<const rr::RRMeshArrays*>(mesh);
			Append(propMesh = new wxStringProperty(_("Mesh"), wxPG_LABEL,wxString::Format("%x",(int)(intptr_t)mesh)));
			AppendIn(propMesh, new wxIntProperty(_("#triangles"),wxPG_LABEL,mesh->getNumTriangles()));
			AppendIn(propMesh, new wxIntProperty(_("#vertices"),wxPG_LABEL,mesh->getNumVertices()));
			if (arrays)
			{
				wxString channels;
				for (unsigned i=0;i<arrays->texcoord.size();i++)
					if (arrays->texcoord[i])
						channels += wxString::Format("%d ",i);
				AppendIn(propMesh, new wxStringProperty(_("uv channels"),wxPG_LABEL,channels));
				AppendIn(propMesh, new wxBoolProperty(_("tangents"),wxPG_LABEL,arrays->tangent?true:false));
				AppendIn(propMesh, new wxIntProperty(_("version"),wxPG_LABEL,arrays->version));
			}
			AppendIn(propMesh, new RRVec3Property(_("Local size"),_("Mesh size in object space"),_precision,maxi-mini));
			AppendIn(propMesh, new RRVec3Property(_("Local min"),_("Mesh AABB min in object space"),_precision,mini));
			AppendIn(propMesh, new RRVec3Property(_("Local max"),_("Mesh AABB max in object space"),_precision,maxi));
			AppendIn(propMesh, new RRVec3Property(_("Local center"),_("Mesh center in object space"),_precision,localCenter));
			EnableProperty(propMesh,false);
		}
	}
}

void SVObjectProperties::updateProperties()
{
	if (!object)
		return;

	updateFloat(propCubeDiffuse,object->illumination.diffuseEnvMap?object->illumination.diffuseEnvMap->getWidth():0);
	updateFloat(propCubeSpecular,object->illumination.specularEnvMap?object->illumination.specularEnvMap->getWidth():0);
}

void SVObjectProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	if (!object)
		return;

	wxPGProperty *property = event.GetProperty();
	if (property==propName)
	{
		object->name = WX2RR(property->GetValue().GetString());
		svframe->updateSceneTree();
	}
	else
	if (property==propTranslation)
	{
		rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
		RRVec3 newTranslation;
		newTranslation << property->GetValue();
		worldMatrix.setTranslation(newTranslation);
		object->setWorldMatrix(&worldMatrix);
		object->illumination.envMapWorldCenter = worldMatrix.transformedPosition(localCenter);
	}
	else
	if (property==propScale)
	{
		rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
		RRVec3 newScale;
		newScale << property->GetValue();
		RRVec3 oldScale = worldMatrix.getScale();
		for (unsigned i=0;i<3;i++)
			if (newScale[i]==0) newScale[i] = oldScale[i];
		worldMatrix.setScale(newScale);
		object->setWorldMatrix(&worldMatrix);
	}
}

void SVObjectProperties::OnPropertySelect(wxPropertyGridEvent& event)
{
	if (!object)
		return;

	wxPGProperty *property = event.GetProperty();
	if (property && property->GetParent()==propFacegroups && svframe->m_materialProperties && object)
	{
		unsigned fg = property->GetIndexInParent();
		if (fg<object->faceGroups.size())
		{
			svframe->m_materialProperties->setMaterial(object->faceGroups[fg].material);
		}
	}
}

BEGIN_EVENT_TABLE(SVObjectProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVObjectProperties::OnPropertyChange)
	EVT_PG_SELECTED(-1,SVObjectProperties::OnPropertySelect)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
