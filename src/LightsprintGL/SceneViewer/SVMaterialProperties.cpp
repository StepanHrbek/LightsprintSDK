// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVMaterialProperties.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVCustomProperties.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// SVMaterialProperties

SVMaterialProperties::SVMaterialProperties(wxWindow* parent)
	: wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER )
{
	lastSolver = NULL;
	lastTriangle = UINT_MAX;
	lastPoint2d = rr::RRVec2(0);
	material = NULL;
	showPoint = false;
	showPhysical = false;
	shown = true;

	Append(propPoint = new wxBoolProperty(wxT("Pixel details"), wxPG_LABEL, showPoint));
	SetPropertyEditor(propPoint,wxPGEditor_CheckBox);
	Append(propPhysical = new wxBoolProperty(wxT("In physical scale"), wxPG_LABEL, showPhysical));
	SetPropertyEditor(propPhysical,wxPGEditor_CheckBox);

	Append(propName = new wxStringProperty(wxT("Name")));
	SetPropertyReadOnly(propName,true); // has no memory management for editing

	Append(propFront = new wxBoolProperty(wxT("Front")));
	SetPropertyEditor(propFront,wxPGEditor_CheckBox);
	Append(propBack = new wxBoolProperty(wxT("Back")));
	SetPropertyEditor(propBack,wxPGEditor_CheckBox);

	Append(propDiffuse = new wxStringProperty(wxT("Diffuse"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propDiffuse,new HDRColorProperty(wxT("color")));
	AppendIn(propDiffuse,new wxIntProperty(wxT("uv")));
	AppendIn(propDiffuse,new wxFileProperty(wxT("texture")));
	Collapse(propDiffuse);

	Append(propSpecular = new wxStringProperty(wxT("Specular"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propSpecular,new HDRColorProperty(wxT("color")));
	AppendIn(propSpecular,new wxIntProperty(wxT("uv")));
	AppendIn(propSpecular,new wxFileProperty(wxT("texture")));
	Collapse(propSpecular);

	Append(propEmissive = new wxStringProperty(wxT("Emissive"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propEmissive,new HDRColorProperty(wxT("color")));
	AppendIn(propEmissive,new wxIntProperty(wxT("uv")));
	AppendIn(propEmissive,new wxFileProperty(wxT("texture")));
	Collapse(propEmissive);

	Append(propTransparent = new wxStringProperty(wxT("Transparent"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propTransparent,new HDRColorProperty(wxT("color")));
	AppendIn(propTransparent,new wxIntProperty(wxT("uv")));
	AppendIn(propTransparent,new wxFileProperty(wxT("texture")));
	AppendIn(propTransparent,propTransparency1bit = new wxBoolProperty(wxT("1-bit")));
	SetPropertyEditor(propTransparency1bit,wxPGEditor_CheckBox);
	AppendIn(propTransparent,propTransparencyInAlpha = new wxBoolProperty(wxT("in alpha")));
	SetPropertyEditor(propTransparencyInAlpha,wxPGEditor_CheckBox);
	AppendIn(propTransparent,propRefraction = new wxFloatProperty(wxT("refraction index")));
	Collapse(propTransparent);

	Append(propLightmapTexcoord = new wxIntProperty(wxT("Lightmap uv")));
	Append(propQualityForPoints = new wxIntProperty(wxT("Quality for point materials")));

	setMaterial(NULL,UINT_MAX,rr::RRVec2(0)); // hides properties, they were not filled yet
}

void SVMaterialProperties::setMaterial(rr::RRDynamicSolver* solver, unsigned hitTriangle, rr::RRVec2 hitPoint2d)
{
	lastSolver = solver;
	lastTriangle = hitTriangle;
	lastPoint2d = hitPoint2d;

	if (hitTriangle==UINT_MAX)
	{
		material = NULL;
	}
	else
	if (showPoint)
	{
		if (showPhysical)
			solver->getMultiObjectPhysical()->getPointMaterial(hitTriangle,hitPoint2d,materialPoint);
		else
			solver->getMultiObjectCustom()->getPointMaterial(hitTriangle,hitPoint2d,materialPoint);
		material = &materialPoint;
	}
	else
	{
		if (showPhysical)
			material = solver->getMultiObjectPhysical()->getTriangleMaterial(hitTriangle,NULL,NULL);
		else
			material = solver->getMultiObjectCustom()->getTriangleMaterial(hitTriangle,NULL,NULL);
	}

	if ((material!=NULL)!=shown)
	{
		shown = material!=NULL;

		HideProperty(propName,!material);
		HideProperty(propFront,!material);
		HideProperty(propBack,!material);
		HideProperty(propDiffuse,!material);
		HideProperty(propSpecular,!material);
		HideProperty(propEmissive,!material);
		HideProperty(propTransparent,!material);
		HideProperty(propLightmapTexcoord,!material);
		HideProperty(propQualityForPoints,!material);
	}

	if (material)
	{
		updateString(propName,material->name?material->name:"");

		updateBool(propFront,material->sideBits[0].renderFrom);
		updateBool(propBack,material->sideBits[1].renderFrom);

		updateProperty(propDiffuse->GetPropertyByName("color"),material->diffuseReflectance.color);
		updateInt(propDiffuse->GetPropertyByName("uv"),material->diffuseReflectance.texcoord);
		updateString(propDiffuse->GetPropertyByName("texture"),material->diffuseReflectance.texture?"filename":"");

		updateProperty(propSpecular->GetPropertyByName("color"),material->specularReflectance.color);
		updateInt(propSpecular->GetPropertyByName("uv"),material->specularReflectance.texcoord);
		updateString(propSpecular->GetPropertyByName("texture"),material->specularReflectance.texture?"filename":"");

		updateProperty(propEmissive->GetPropertyByName("color"),material->diffuseEmittance.color);
		updateInt(propEmissive->GetPropertyByName("uv"),material->diffuseEmittance.texcoord);
		updateString(propEmissive->GetPropertyByName("texture"),material->diffuseEmittance.texture?"filename":"");

		updateProperty(propTransparent->GetPropertyByName("color"),material->specularTransmittance.color);
		updateInt(propTransparent->GetPropertyByName("uv"),material->specularTransmittance.texcoord);
		updateString(propTransparent->GetPropertyByName("texture"),material->specularTransmittance.texture?"filename":"");
		updateBool(propTransparency1bit,material->specularTransmittanceKeyed);
		updateBool(propTransparencyInAlpha,material->specularTransmittanceInAlpha);
		updateFloat(propRefraction,material->refractionIndex);

		updateInt(propLightmapTexcoord,material->lightmapTexcoord);
		updateInt(propQualityForPoints,material->minimalQualityForPointMaterials);

		updateReadOnly();
	}
}

void SVMaterialProperties::updateReadOnly()
{
	if (material)
	{
		SetPropertyReadOnly(propFront,showPoint);
		SetPropertyReadOnly(propBack,showPoint);
		SetPropertyReadOnly(propDiffuse,showPoint);
		SetPropertyReadOnly(propDiffuse->GetPropertyByName("color"),showPoint||material->diffuseReflectance.texture);
		SetPropertyReadOnly(propSpecular,showPoint);
		SetPropertyReadOnly(propSpecular->GetPropertyByName("color"),showPoint||material->specularReflectance.texture);
		SetPropertyReadOnly(propEmissive,showPoint);
		SetPropertyReadOnly(propEmissive->GetPropertyByName("color"),showPoint||material->diffuseEmittance.texture);
		SetPropertyReadOnly(propTransparent,showPoint);
		SetPropertyReadOnly(propTransparent->GetPropertyByName("color"),showPoint||material->specularTransmittance.texture);
		SetPropertyReadOnly(propLightmapTexcoord,showPoint);
		SetPropertyReadOnly(propQualityForPoints,showPoint);
	}
}

void SVMaterialProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propPoint)
	{
		showPoint = property->GetValue().GetBool();
		setMaterial(lastSolver,lastTriangle,lastPoint2d); // load different material
	}
	else
	if (property==propPhysical)
	{
		showPhysical = property->GetValue().GetBool();
		setMaterial(lastSolver,lastTriangle,lastPoint2d); // load different material
	}
	if (!material)
		return;

	/* free/strdup nesmim pokud do RRMaterialu neudelam deep copy name a uvolnovani
	if (property==propName)
	{
		free(material->name);
		material->name = strdup(property->GetValue().GetString().c_str());
	}
	else*/
	if (property==propFront)
	{
		rr::RRSideBits visible[2] = {{0,0,1,1,0,0,1},{1,1,1,1,1,1,1}};
		material->sideBits[0] = visible[property->GetValue().GetBool()?1:0];
	}
	else
	if (property==propBack)
	{
		rr::RRSideBits visible[2] = {{0,0,1,1,0,0,1},{1,0,1,1,1,1,1}};
		material->sideBits[1] = visible[property->GetValue().GetBool()?1:0];
	}
	else
	if (property==propDiffuse->GetPropertyByName("uv"))
	{
		material->diffuseReflectance.texcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propDiffuse->GetPropertyByName("texture"))
	{
		if (!material->diffuseReflectance.texture)
		{
			material->diffuseReflectance.texture = rr::RRBuffer::load(property->GetValue().GetString());
			//!!! poznamenat si a na konci uvolnit
		}
		else
			material->diffuseReflectance.texture->reload(property->GetValue().GetString(),NULL);
	}
	else
	if (property==propDiffuse->GetPropertyByName("color"))
	{
		material->diffuseReflectance.color << property->GetValue();
	}
	else

	if (property==propSpecular->GetPropertyByName("uv"))
	{
		material->specularReflectance.texcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propSpecular->GetPropertyByName("texture"))
	{
		if (!material->specularReflectance.texture)
		{
			material->specularReflectance.texture = rr::RRBuffer::load(property->GetValue().GetString());
			//!!! poznamenat si a na konci uvolnit
		}
		else
			material->specularReflectance.texture->reload(property->GetValue().GetString(),NULL);
	}
	else
	if (property==propSpecular->GetPropertyByName("color"))
	{
		material->specularReflectance.color << property->GetValue();
	}
	else

	if (property==propEmissive->GetPropertyByName("uv"))
	{
		material->diffuseEmittance.texcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propEmissive->GetPropertyByName("texture"))
	{
		if (!material->diffuseEmittance.texture)
		{
			material->diffuseEmittance.texture = rr::RRBuffer::load(property->GetValue().GetString());
			//!!! poznamenat si a na konci uvolnit
		}
		else
			material->diffuseEmittance.texture->reload(property->GetValue().GetString(),NULL);
	}
	else
	if (property==propEmissive->GetPropertyByName("color"))
	{
		material->diffuseEmittance.color << property->GetValue();
	}
	else

	if (property==propTransparent->GetPropertyByName("uv"))
	{
		material->specularTransmittance.texcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propTransparent->GetPropertyByName("texture"))
	{
		if (!material->specularTransmittance.texture)
		{
			material->specularTransmittance.texture = rr::RRBuffer::load(property->GetValue().GetString());
			//!!! poznamenat si a na konci uvolnit
		}
		else
			material->specularTransmittance.texture->reload(property->GetValue().GetString(),NULL);
	}
	else
	if (property==propTransparent->GetPropertyByName("color"))
	{
		material->specularTransmittance.color << property->GetValue();
	}
	else

	if (property==propTransparency1bit)
	{
		material->specularTransmittanceKeyed = property->GetValue().GetBool();
	}
	else
	if (property==propTransparencyInAlpha)
	{
		material->specularTransmittanceInAlpha = property->GetValue().GetBool();
	}
	else
	if (property==propRefraction)
	{
		material->refractionIndex = property->GetValue().GetDouble();
	}
	else
	if (property==propLightmapTexcoord)
	{
		material->lightmapTexcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propQualityForPoints)
	{
		material->minimalQualityForPointMaterials = property->GetValue().GetInteger();
	}
}

BEGIN_EVENT_TABLE(SVMaterialProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVMaterialProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
