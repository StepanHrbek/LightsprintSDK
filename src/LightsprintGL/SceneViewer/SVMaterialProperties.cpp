// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVMaterialProperties.h"
#include "SVCustomProperties.h"
#include "SVApp.h"
#include "Lightsprint/GL/Texture.h"
#include "../tmpstr.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// SVMaterialProperties

SVMaterialProperties::SVMaterialProperties(wxWindow* parent, int _precision)
	: wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER )
{
	SV_SET_PG_COLORS;
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

	Append(propFront = new wxBoolProperty(wxT("Front")));
	SetPropertyEditor(propFront,wxPGEditor_CheckBox);
	Append(propBack = new wxBoolProperty(wxT("Back")));
	SetPropertyEditor(propBack,wxPGEditor_CheckBox);

	Append(propDiffuse = new wxStringProperty(wxT("Diffuse"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propDiffuse,new HDRColorProperty(wxT("color"),wxPG_LABEL,_precision));
	AppendIn(propDiffuse,new wxIntProperty(wxT("uv")));
	AppendIn(propDiffuse,new wxFileProperty(wxT("texture")));
	Collapse(propDiffuse);

	Append(propSpecular = new wxStringProperty(wxT("Specular"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propSpecular,new HDRColorProperty(wxT("color"),wxPG_LABEL,_precision));
	AppendIn(propSpecular,new wxIntProperty(wxT("uv")));
	AppendIn(propSpecular,new wxFileProperty(wxT("texture")));
	Collapse(propSpecular);

	Append(propEmissive = new wxStringProperty(wxT("Emissive"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propEmissive,new HDRColorProperty(wxT("color"),wxPG_LABEL,_precision));
	AppendIn(propEmissive,new wxIntProperty(wxT("uv")));
	AppendIn(propEmissive,new wxFileProperty(wxT("texture")));
	Collapse(propEmissive);

	Append(propTransparent = new wxStringProperty(wxT("Transparent"),wxPG_LABEL,wxT("<composed>")));
	AppendIn(propTransparent,new HDRColorProperty(wxT("color"),wxPG_LABEL,_precision));
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

wxString getTextureDescription(rr::RRBuffer* buffer)
{
	return buffer
		? (buffer->filename.empty()
			?tmpstr("<%d*%d generated>",buffer->getWidth(),buffer->getHeight())
			:buffer->filename.c_str())
		:"<no texture>";
}

static void setMaterialProperty(wxPGProperty* wxproperty, rr::RRMaterial::Property& rrproperty)
{
	updateProperty(wxproperty->GetPropertyByName("color"),rrproperty.color);
	updateInt(wxproperty->GetPropertyByName("uv"),rrproperty.texcoord);
	updateString(wxproperty->GetPropertyByName("texture"),getTextureDescription(rrproperty.texture));
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
		materialPhysical = solver->getMultiObjectPhysical()->getTriangleMaterial(hitTriangle,NULL,NULL);
		materialCustom = solver->getMultiObjectCustom()->getTriangleMaterial(hitTriangle,NULL,NULL);
		material = showPhysical ? materialPhysical : materialCustom;
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
		updateString(propName,material->name.c_str());

		updateBool(propFront,material->sideBits[0].renderFrom);
		updateBool(propBack,material->sideBits[1].renderFrom);

		setMaterialProperty(propDiffuse,material->diffuseReflectance);
		setMaterialProperty(propSpecular,material->specularReflectance);
		setMaterialProperty(propEmissive,material->diffuseEmittance);
		setMaterialProperty(propTransparent,material->specularTransmittance);

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
	bool diffuseChanged = false;
	bool specularChanged = false;
	bool emittanceChanged = false;
	bool textureChanged = false;

	wxPGProperty *property = event.GetProperty();

	// propagate change from wx to material
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

	if (property==propName)
	{
		material->name = property->GetValue().GetString().c_str();
	}
	else
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

	// - diffuseReflectance
	if (property==propDiffuse->GetPropertyByName("uv"))
	{
		material->diffuseReflectance.texcoord = property->GetValue().GetInteger();
		diffuseChanged = true;
	}
	else
	if (property==propDiffuse->GetPropertyByName("texture"))
	{
		delete material->diffuseReflectance.texture;
		material->diffuseReflectance.texture = rr::RRBuffer::load(property->GetValue().GetString());
		diffuseChanged = true;
		textureChanged = true;
	}
	else
	if (property==propDiffuse->GetPropertyByName("color"))
	{
		material->diffuseReflectance.color << property->GetValue();
		diffuseChanged = true;
	}
	else

	// - specularReflectance
	if (property==propSpecular->GetPropertyByName("uv"))
	{
		material->specularReflectance.texcoord = property->GetValue().GetInteger();
		specularChanged = true;
	}
	else
	if (property==propSpecular->GetPropertyByName("texture"))
	{
		delete material->specularReflectance.texture;
		material->specularReflectance.texture = rr::RRBuffer::load(property->GetValue().GetString());
		specularChanged = true;
		textureChanged = true;
	}
	else
	if (property==propSpecular->GetPropertyByName("color"))
	{
		material->specularReflectance.color << property->GetValue();
		specularChanged = true;
	}
	else

	// - diffuseEmittance
	if (property==propEmissive->GetPropertyByName("uv"))
	{
		material->diffuseEmittance.texcoord = property->GetValue().GetInteger();
		emittanceChanged = true;
	}
	else
	if (property==propEmissive->GetPropertyByName("texture"))
	{
		delete material->diffuseEmittance.texture;
		material->diffuseEmittance.texture = rr::RRBuffer::load(property->GetValue().GetString());
		emittanceChanged = true;
		textureChanged = true;
	}
	else
	if (property==propEmissive->GetPropertyByName("color"))
	{
		material->diffuseEmittance.color << property->GetValue();
		emittanceChanged = true;
	}
	else

	// - specularTransmittance
	if (property==propTransparent->GetPropertyByName("uv"))
	{
		material->specularTransmittance.texcoord = property->GetValue().GetInteger();
	}
	else
	if (property==propTransparent->GetPropertyByName("texture"))
	{
		delete material->specularTransmittance.texture;
		material->specularTransmittance.texture = rr::RRBuffer::load(property->GetValue().GetString());
		textureChanged = true;
	}
	else
	if (property==propTransparent->GetPropertyByName("color"))
	{
		material->specularTransmittance.color << property->GetValue();
	}
	else

	// - the rest
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

	// propagate change from physical material to custom and vice versa
	if (!showPoint && materialPhysical && materialCustom)
	{
		if (showPhysical)
		{
			materialCustom->copyFrom(*materialPhysical);
			materialCustom->convertToCustomScale(lastSolver->getScaler());
		}
		else
		{
			materialPhysical->copyFrom(*materialCustom);
			materialPhysical->convertToCustomScale(lastSolver->getScaler());
		}
	}

	// add/remove cubemaps after change in diffuse or specular
	if (diffuseChanged || specularChanged)
	{
		// diffuse sphere doesn't have specular cube
		// when we change material to specular, specular cube is allocated here
		// warning: also deallocates if no longer necessary, may deallocate custom cubes
		lastSolver->allocateBuffersForRealtimeGI(-1);
	}

	// update solver after change in emittance
	if (emittanceChanged)
	{
		lastSolver->setEmittance(1,16,true);
	}

	// after adding/deleting texture, readonly changes
	if (textureChanged)
	{
		updateReadOnly();
	}
}

BEGIN_EVENT_TABLE(SVMaterialProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVMaterialProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
