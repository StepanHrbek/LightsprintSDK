// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVMaterialProperties.h"
#include "SVCustomProperties.h"
#include "SVApp.h"
#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// SVMaterialProperties

SVMaterialProperties::SVMaterialProperties(wxWindow* _parent, const SceneViewerState& _svs)
	: wxPropertyGrid( _parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER ), svs(_svs)
{
	SV_SET_PG_COLORS;
	wxColour headerColor(230,230,230);

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

	Append(propDiffuse = new wxStringProperty(wxT("Diffuse")));
	AppendIn(propDiffuse,new HDRColorProperty(wxT("color"),wxPG_LABEL,_svs.precision));
	AppendIn(propDiffuse,new wxIntProperty(wxT("uv")));
	AppendIn(propDiffuse,new ImageFileProperty(wxT("texture")));
	SetPropertyBackgroundColour(propDiffuse,headerColor,false);
	Collapse(propDiffuse);

	Append(propSpecular = new wxStringProperty(wxT("Specular")));
	AppendIn(propSpecular,new HDRColorProperty(wxT("color"),wxPG_LABEL,_svs.precision));
	AppendIn(propSpecular,new wxIntProperty(wxT("uv")));
	AppendIn(propSpecular,new ImageFileProperty(wxT("texture")));
	SetPropertyBackgroundColour(propSpecular,headerColor,false);
	Collapse(propSpecular);

	Append(propEmissive = new wxStringProperty(wxT("Emissive")));
	AppendIn(propEmissive,new HDRColorProperty(wxT("color"),wxPG_LABEL,_svs.precision));
	AppendIn(propEmissive,new wxIntProperty(wxT("uv")));
	AppendIn(propEmissive,new ImageFileProperty(wxT("texture")));
	SetPropertyBackgroundColour(propEmissive,headerColor,false);
	Collapse(propEmissive);

	Append(propTransparent = new wxStringProperty(wxT("Transparent")));
	AppendIn(propTransparent,new HDRColorProperty(wxT("color"),wxPG_LABEL,_svs.precision));
	AppendIn(propTransparent,new wxIntProperty(wxT("uv")));
	AppendIn(propTransparent,new ImageFileProperty(wxT("texture")));
	AppendIn(propTransparent,propTransparency1bit = new wxBoolProperty(wxT("1-bit")));
	SetPropertyEditor(propTransparency1bit,wxPGEditor_CheckBox);
	AppendIn(propTransparent,propTransparencyInAlpha = new wxBoolProperty(wxT("in alpha")));
	SetPropertyEditor(propTransparencyInAlpha,wxPGEditor_CheckBox);
	AppendIn(propTransparent,propRefraction = new wxFloatProperty(wxT("refraction index")));
	SetPropertyBackgroundColour(propTransparent,headerColor,false);
	Collapse(propTransparent);

	Append(propLightmapTexcoord = new wxIntProperty(wxT("Lightmap uv")));
	Append(propQualityForPoints = new wxIntProperty(wxT("Quality for point materials")));

	setMaterial(NULL,UINT_MAX,rr::RRVec2(0)); // hides properties, they were not filled yet
}

// compose root property out of child properties
static void composeMaterialPropertyRoot(wxPGProperty* prop, rr::RRMaterial::Property& material)
{
	// update self
	if (material.color==rr::RRVec3(0) && !material.texture)
	{
		prop->SetValueImage(*(wxBitmap*)NULL);
		prop->SetValueFromString("none");
	}
	else
	{
		wxBitmap* bitmap = prop->GetPropertyByName("texture")->GetValueImage();
		if (!bitmap) bitmap = prop->GetPropertyByName("color")->GetValueImage();
		prop->SetValueImage(*bitmap);
		prop->SetValueFromString("");
	}
}

// copy part of material to propertygrid
static void setMaterialProperty(wxPGProperty* prop, rr::RRMaterial::Property& material)
{
	wxPGProperty* propColor = prop->GetPropertyByName("color");
	wxPGProperty* propUv = prop->GetPropertyByName("uv");
	ImageFileProperty* propTexture = (ImageFileProperty*)prop->GetPropertyByName("texture");

	// update children
	updateProperty(propColor,material.color);
	updateInt(propUv,material.texcoord);
	updateString(propTexture,getTextureDescription(material.texture));
	propTexture->updateIcon(material.texture);

	// update self
	composeMaterialPropertyRoot(prop,material);
}

// copy material to propertygrid
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
		SetPropertyReadOnly(propDiffuse,true,0);
		SetPropertyReadOnly(propDiffuse->GetPropertyByName("color"),showPoint||material->diffuseReflectance.texture);
		SetPropertyReadOnly(propSpecular,showPoint);
		SetPropertyReadOnly(propSpecular,true,0);
		SetPropertyReadOnly(propSpecular->GetPropertyByName("color"),showPoint||material->specularReflectance.texture);
		SetPropertyReadOnly(propEmissive,showPoint);
		SetPropertyReadOnly(propEmissive,true,0);
		SetPropertyReadOnly(propEmissive->GetPropertyByName("color"),showPoint||material->diffuseEmittance.texture);
		SetPropertyReadOnly(propTransparent,showPoint);
		SetPropertyReadOnly(propTransparent,true,0);
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
		((ImageFileProperty*)property)->updateBufferAndIcon(material->diffuseReflectance.texture,svs.playVideos);
		material->diffuseReflectance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP);
		updateProperty(propDiffuse->GetPropertyByName("color"),material->diffuseReflectance.color);
		composeMaterialPropertyRoot(propDiffuse,material->diffuseReflectance);
		diffuseChanged = true;
		textureChanged = true;
	}
	else
	if (property==propDiffuse->GetPropertyByName("color"))
	{
		material->diffuseReflectance.color << property->GetValue();
		composeMaterialPropertyRoot(propDiffuse,material->diffuseReflectance);
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
		((ImageFileProperty*)property)->updateBufferAndIcon(material->specularReflectance.texture,svs.playVideos);
		material->specularReflectance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP);
		updateProperty(propSpecular->GetPropertyByName("color"),material->specularReflectance.color);
		composeMaterialPropertyRoot(propSpecular,material->specularReflectance);
		specularChanged = true;
		textureChanged = true;
	}
	else
	if (property==propSpecular->GetPropertyByName("color"))
	{
		material->specularReflectance.color << property->GetValue();
		composeMaterialPropertyRoot(propSpecular,material->specularReflectance);
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
		((ImageFileProperty*)property)->updateBufferAndIcon(material->diffuseEmittance.texture,svs.playVideos);
		material->diffuseEmittance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP);
		updateProperty(propEmissive->GetPropertyByName("color"),material->diffuseEmittance.color);
		composeMaterialPropertyRoot(propEmissive,material->diffuseEmittance);
		emittanceChanged = true;
		textureChanged = true;
	}
	else
	if (property==propEmissive->GetPropertyByName("color"))
	{
		material->diffuseEmittance.color << property->GetValue();
		composeMaterialPropertyRoot(propEmissive,material->diffuseEmittance);
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
		((ImageFileProperty*)property)->updateBufferAndIcon(material->specularTransmittance.texture,svs.playVideos);
		if (material->specularTransmittance.texture && material->specularTransmittance.texture->getFormat()!=rr::BF_RGBA && material->specularTransmittance.texture->getFormat()!=rr::BF_RGBAF)
			material->specularTransmittanceInAlpha = false;
		material->specularTransmittance.updateColorFromTexture(NULL,material->specularTransmittanceInAlpha,rr::RRMaterial::UTA_KEEP);
		updateProperty(propTransparent->GetPropertyByName("color"),material->specularTransmittance.color);
		composeMaterialPropertyRoot(propTransparent,material->specularTransmittance);
		textureChanged = true;
	}
	else
	if (property==propTransparent->GetPropertyByName("color"))
	{
		material->specularTransmittance.color << property->GetValue();
		composeMaterialPropertyRoot(propTransparent,material->specularTransmittance);
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
		lastSolver->setEmittance(svs.emissiveMultiplier,16,true);
	}

	// after adding/deleting texture, readonly changes
	if (textureChanged)
	{
		updateReadOnly();
	}

	if (svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL)
	{
		// fireball: skip reportMaterialChange(), it either switches to architect (current behaviour, fast, but surprising, possibly unwanted)
		//           or rebuilds fireball (not enabled, would be very slow)
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::INF2,"To make material change affect indirect light, switch to Architect solver or rebuild Fireball (both in Global Illumination menu).\n"));
	}
	else
	if (svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
	{
		// architect: reportMaterialChange() is cheap, call it
		lastSolver->reportMaterialChange();
	}
}

BEGIN_EVENT_TABLE(SVMaterialProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVMaterialProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
