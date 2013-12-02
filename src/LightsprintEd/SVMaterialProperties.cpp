// --------------------------------------------------------------------------
// Scene viewer - material properties window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVMaterialProperties.h"
#include "SVCustomProperties.h"
#include "SVApp.h"
#include "Lightsprint/GL/Texture.h"

namespace rr_ed
{

//////////////////////////////////////////////////////////////////////////////
//
// SVMaterialProperties

SVMaterialProperties::SVMaterialProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
	locked = false;
	lastSolver = NULL;
	lastObject = NULL;
	lastTriangle = UINT_MAX;
	lastPoint2d = rr::RRVec2(0);
	material = NULL;
	materialIsInStaticScene = true;
	showPoint = false;
	showPhysical = false;

	propLoad = new ButtonProperty(_("Load"),_("Loads this material from file."),svframe,SVFrame::ME_MATERIAL_LOAD);
	Append(propLoad);
	propLoad->updateImage();
	propSave = new ButtonProperty(_("Save"),_("Saves this material to file."),svframe,SVFrame::ME_MATERIAL_SAVE);
	Append(propSave);
	propSave->updateImage();
	propSaveAll = new ButtonProperty(_("Save library"),_("Saves all materials in scene to file."),svframe,SVFrame::ME_MATERIAL_SAVE_ALL);
	Append(propSaveAll);
	propSaveAll->updateImage();

	Append(propPoint = new BoolRefProperty(_("Pixel details"),_("Clicking scene shows material properties of pixel rather than face."),showPoint));
	Append(propPhysical = new BoolRefProperty(_("In physical scale"),_("Displays values in linear physical scale, rather than in sRGB."),showPhysical));

	Append(propName = new wxStringProperty(_("Name")));
	propName->SetHelpString(_("If name contains \"water\" and normal map is set, we animate mapping to simulate flow."));

	Append(propFront = new wxBoolProperty(_("Front")));
	SetPropertyEditor(propFront,wxPGEditor_CheckBox);
	Append(propBack = new wxBoolProperty(_("Back")));
	SetPropertyEditor(propBack,wxPGEditor_CheckBox);

	Append(propDiffuse = new wxStringProperty(_("Diffuse")));
	AppendIn(propDiffuse,new HDRColorProperty(_("color"),_("If texture is set, color is calculated from texture and can't be edited."),svs.precision));
	AppendIn(propDiffuse,new wxIntProperty(_("uv")));
	AppendIn(propDiffuse,new ImageFileProperty(_("texture or video"),_("Diffuse texture or video. Type in c@pture to use live video input.")));
	SetPropertyBackgroundColour(propDiffuse,importantPropertyBackgroundColor,false);
	Collapse(propDiffuse);

	Append(propSpecular = new wxStringProperty(_("Specular")));
	AppendIn(propSpecular,new HDRColorProperty(_("color"),_("If texture is set, color is calculated from texture and can't be edited."),svs.precision));
	AppendIn(propSpecular,new wxIntProperty(_("uv")));
	AppendIn(propSpecular,new ImageFileProperty(_("texture or video"),_("Specular texture or video. Type in c@pture to use live video input. Alpha (if present) modulates shininess/roughness.")));
	{
		const wxChar* strings[] = {wxT("Phong"),wxT("Blinn-Phong"),wxT("Torrance-Sparrow (Gaussian)"),wxT("Blinn-Torrance-Sparrow"),NULL};
		const long values[] = {rr::RRMaterial::PHONG,rr::RRMaterial::BLINN_PHONG,rr::RRMaterial::TORRANCE_SPARROW,rr::RRMaterial::BLINN_TORRANCE_SPARROW};
		propSpecularModel = new wxEnumProperty(_("model"),wxPG_LABEL,strings,values);
		propSpecularModel->SetHelpString(_("Changes shape and intensity of specular highlights."));
		AppendIn(propSpecular,propSpecularModel);
	}
	AppendIn(propSpecularModel,propSpecularShininess = new FloatProperty(_("shininess"),_("Effect depends on model."),10,svs.precision,1,1000000,5,false));
	AppendIn(propSpecularModel,propSpecularRoughness = new FloatProperty(_("roughness"),_("Effect depends on model."),10,svs.precision,0,1,0.2f,false));
	SetPropertyBackgroundColour(propSpecular,importantPropertyBackgroundColor,false);
	Collapse(propSpecular);

	Append(propEmissive = new wxStringProperty(_("Emissive")));
	AppendIn(propEmissive,new HDRColorProperty(_("color"),_("If texture is set, color is calculated from texture and can't be edited."),svs.precision));
	AppendIn(propEmissive,new wxIntProperty(_("uv")));
	AppendIn(propEmissive,new ImageFileProperty(_("texture or video"),_("Emissive texture or video. Type in c@pture to use live video input.")));
	SetPropertyBackgroundColour(propEmissive,importantPropertyBackgroundColor,false);
	Collapse(propEmissive);

	Append(propTransparent = new wxStringProperty(_("Transparency")));
	AppendIn(propTransparent,new HDRColorProperty(_("color"),_("If texture is set, color is calculated from texture and can't be edited."),svs.precision));
	AppendIn(propTransparent,new wxIntProperty(_("uv")));
	AppendIn(propTransparent,new ImageFileProperty(_("texture or video"),_("Opacity texture or video. Type in c@pture to use live video input.")));
	AppendIn(propTransparent,propTransparency1bit = new wxBoolProperty(_("1-bit")));
	SetPropertyEditor(propTransparency1bit,wxPGEditor_CheckBox);
	propTransparency1bit->SetHelpString(_("Makes opacity either 0% or 100%."));
	AppendIn(propTransparency1bit,propTransparencyThreshold = new FloatProperty(_("threshold"),_("Transmittance in 0..1 range is tested against this threshold. Values above threshold are considered fully transparent, values below threshold are considered fully opaque."),0,svs.precision,0,1,0.1f,false));
	AppendIn(propTransparent,propTransparencyInAlpha = new wxBoolProperty(_("in alpha")));
	SetPropertyEditor(propTransparencyInAlpha,wxPGEditor_CheckBox);
	propTransparencyInAlpha->SetHelpString(_("Reads opacity from alpha rather than from rgb."));
	AppendIn(propTransparent,propTransparencyMapInverted = new wxBoolProperty(_("inverted")));
	SetPropertyEditor(propTransparencyMapInverted,wxPGEditor_CheckBox);
	propTransparencyMapInverted->SetHelpString(_("Inverts how transparency texture is interpreted."));
	AppendIn(propTransparent,propRefraction = new FloatProperty("refraction index",_("Index of refraction when light hits surface from front side."),1,svs.precision,0.25f,4,0.1f,false));
	SetPropertyBackgroundColour(propTransparent,importantPropertyBackgroundColor,false);
	Collapse(propTransparent);

	Append(propBumpMap = new wxStringProperty(_("Bump map")));
	AppendIn(propBumpMap,new wxIntProperty(_("uv")));
	AppendIn(propBumpMap,new ImageFileProperty(_("texture or video"),_("Normal map (rgb) or height map (grayscale), texture or video. Type in c@pture to use live video input.")));
	AppendIn(propBumpMap,propBumpType = new wxBoolProperty(_("height map")));
	SetPropertyEditor(propBumpType,wxPGEditor_CheckBox);
	propBumpType->SetHelpString(_("Use the map as a height map rather than normal map."));
	AppendIn(propBumpMap,propBumpMultiplier1 = new FloatProperty("normal multiplier",_("Multiplies bump effect on normals."),1,svs.precision,-1000,1000,0.1f,false));
	AppendIn(propBumpMap,propBumpMultiplier2 = new FloatProperty("parallax multiplier",_("Multiplies parallax mapping effect."),1,svs.precision,-1000,1000,0.1f,false));
	SetPropertyBackgroundColour(propBumpMap,importantPropertyBackgroundColor,false);
	Collapse(propBumpMap);

	Append(propLightmapTexcoord = new wxIntProperty(_("Lightmap uv")));
	Append(propQualityForPoints = new wxIntProperty(_("Quality for point materials")));

	setMaterial(NULL,NULL,UINT_MAX,rr::RRVec2(0)); // hides properties, they were not filled yet
}

// compose root property out of child properties
static void composeMaterialPropertyRoot(wxPGProperty* prop, rr::RRMaterial::Property& material)
{
	// update self
	wxPGProperty* propColor = prop->GetPropertyByName(_("color"));
	if ((!propColor || material.color==rr::RRVec3(0)) && !material.texture) // !propColor catches bumpMap, it does not have color, needs either texture or none
	{
		prop->SetValueImage(*(wxBitmap*)NULL);
		prop->SetValueFromString(_("none"));
	}
	else
	{
		wxBitmap* bitmap = prop->GetPropertyByName(_("texture or video"))->GetValueImage();
		if (!bitmap)
		{
			wxVariant v;
			if (propColor) // it does not have to exist (propBumpMap)
			{
				propColor->ValueToString(v); // updates value image
				bitmap = propColor->GetValueImage();
			}
		}
		if (bitmap)
		{
			prop->SetValueImage(*bitmap);
			prop->SetValueFromString("");
		}
	}
}

// copy part of material to propertygrid
static void setMaterialProperty(wxPGProperty* prop, rr::RRMaterial::Property& material)
{
	wxPGProperty* propColor = prop->GetPropertyByName(_("color"));
	wxPGProperty* propUv = prop->GetPropertyByName(_("uv"));
	ImageFileProperty* propTexture = (ImageFileProperty*)prop->GetPropertyByName(_("texture or video"));

	// update children
	if (propColor) // it does not have to exist (propBumpMap)
		updateProperty(propColor,material.color);
	updateInt(propUv,material.texcoord);
	updateString(propTexture,getTextureDescription(material.texture));
	propTexture->updateIcon(material.texture);

	// update self
	composeMaterialPropertyRoot(prop,material);
}

void SVMaterialProperties::updateHide()
{
	{
		HideProperty(propLoad,!material);
		HideProperty(propSave,!material);
		HideProperty(propName,!material);
		HideProperty(propFront,!material);
		HideProperty(propBack,!material);
		HideProperty(propDiffuse,!material);
		HideProperty(propSpecular,!material);
		HideProperty(propSpecularShininess,!material || material->specularModel==rr::RRMaterial::TORRANCE_SPARROW || material->specularModel==rr::RRMaterial::BLINN_TORRANCE_SPARROW);
		HideProperty(propSpecularRoughness,!material || material->specularModel==rr::RRMaterial::PHONG || material->specularModel==rr::RRMaterial::BLINN_PHONG);
		HideProperty(propEmissive,!material);
		HideProperty(propTransparent,!material);
		HideProperty(propTransparencyThreshold,!material || !material->specularTransmittanceKeyed);
		HideProperty(propBumpMap,!material);
		HideProperty(propBumpType,!material || !material->bumpMap.texture);
		HideProperty(propBumpMultiplier1,!material || !material->bumpMap.texture);
		HideProperty(propBumpMultiplier2,!material || !material->bumpMap.texture || !material->bumpMapTypeHeight);
		HideProperty(propLightmapTexcoord,!material);
		HideProperty(propQualityForPoints,!material);
	}
}

void SVMaterialProperties::updateReadOnly()
{
	if (material)
	{
		EnableProperty(propName,!showPoint);
		EnableProperty(propFront,!showPoint);
		EnableProperty(propBack,!showPoint);
		EnableProperty(propDiffuse,!showPoint);
		SetPropertyReadOnly(propDiffuse,true,0);
		EnableProperty(propDiffuse->GetPropertyByName(_("color")),!(showPoint||material->diffuseReflectance.texture));
		EnableProperty(propSpecular,!showPoint);
		SetPropertyReadOnly(propSpecular,true,0);
		EnableProperty(propSpecular->GetPropertyByName(_("color")),!(showPoint||material->specularReflectance.texture));
		EnableProperty(propEmissive,!showPoint);
		SetPropertyReadOnly(propEmissive,true,0);
		EnableProperty(propEmissive->GetPropertyByName(_("color")),!(showPoint||material->diffuseEmittance.texture));
		EnableProperty(propTransparent,!showPoint);
		SetPropertyReadOnly(propTransparent,true,0);
		EnableProperty(propTransparent->GetPropertyByName(_("color")),!(showPoint||material->specularTransmittance.texture));
		EnableProperty(propBumpMap,!showPoint);
		SetPropertyReadOnly(propBumpMap,true,0);
		EnableProperty(propLightmapTexcoord,!showPoint);
		EnableProperty(propQualityForPoints,!showPoint);
	}
}

void SVMaterialProperties::updateProperties()
{
	if (material)
	{
		updateString(propName,RR_RR2WX(material->name));

		updateBool(propFront,material->sideBits[0].renderFrom);
		updateBool(propBack,material->sideBits[1].renderFrom);

		setMaterialProperty(propDiffuse,material->diffuseReflectance);
		setMaterialProperty(propSpecular,material->specularReflectance);
		setMaterialProperty(propEmissive,material->diffuseEmittance);
		setMaterialProperty(propTransparent,material->specularTransmittance);
		setMaterialProperty(propBumpMap,material->bumpMap);

		updateInt(propSpecularModel,material->specularModel);
		updateFloat(propSpecularShininess,material->specularShininess);
		updateFloat(propSpecularRoughness,material->specularShininess);
		updateBool(propTransparency1bit,material->specularTransmittanceKeyed);
		updateFloat(propTransparencyThreshold,material->specularTransmittanceThreshold);
		updateBool(propTransparencyInAlpha,material->specularTransmittanceInAlpha);
		updateBool(propTransparencyMapInverted,material->specularTransmittanceMapInverted);
		updateFloat(propRefraction,material->refractionIndex);
		updateBool(propBumpType,material->bumpMapTypeHeight);
		updateFloat(propBumpMultiplier1,material->bumpMap.color.x);
		updateFloat(propBumpMultiplier2,material->bumpMap.color.y);

		updateInt(propLightmapTexcoord,material->lightmapTexcoord);
		updateInt(propQualityForPoints,material->minimalQualityForPointMaterials);
	}
	updateHide();
	updateReadOnly();
}

void SVMaterialProperties::updateIsInStaticScene()
{
	materialIsInStaticScene = false;
	if (svframe && svframe->m_canvas && svframe->m_canvas->solver)
	{
		const rr::RRObject* multiobject = svframe->m_canvas->solver->getMultiObjectCustom();
		if (multiobject)
			for (unsigned fg=0;fg<multiobject->faceGroups.size();fg++)
				if (multiobject->faceGroups[fg].material==materialCustom)
					materialIsInStaticScene = true;
	}
}

//! Copy material -> property (selected by clicking facegroup, clears physical flag, clears point flag).
void SVMaterialProperties::setMaterial(rr::RRMaterial* _material)
{
	if (locked) return;

	EnableProperty(propPoint,false);
	EnableProperty(propPhysical,false);

	updateBool(propPoint,showPoint = false);
	updateBool(propPhysical,showPhysical = false);

	materialPhysical = NULL;
	materialCustom = _material;
	material = _material;

	updateProperties();

	updateIsInStaticScene();
}

// copy material to propertygrid
// if object!=NULL, show custom data for hitTriangle in object, show no physical data
// if object==NULL, show custom/physical data for hitTriangle in getMultiObject[Custom|Physical]()
void SVMaterialProperties::setMaterial(rr::RRDynamicSolver* solver, rr::RRObject* object, unsigned hitTriangle, rr::RRVec2 hitPoint2d)
{
	if (locked) return;

	EnableProperty(propPoint,true);
	EnableProperty(propPhysical,true);

	lastSolver = solver;
	lastObject = object;
	lastTriangle = hitTriangle;
	lastPoint2d = hitPoint2d;

	if (hitTriangle==UINT_MAX || !solver || (object && showPhysical))
	{
		material = NULL;
		materialPhysical = NULL;
		materialCustom = NULL;
	}
	else
	if (showPoint)
	{
		if (showPhysical)
			solver->getMultiObjectPhysical()->getPointMaterial(hitTriangle,hitPoint2d,materialPoint);
		else
			(object?object:solver->getMultiObjectCustom())->getPointMaterial(hitTriangle,hitPoint2d,materialPoint);
		material = &materialPoint;
	}
	else
	{
		materialPhysical = object?NULL:solver->getMultiObjectPhysical()->getTriangleMaterial(hitTriangle,NULL,NULL);
		materialCustom = (object?object:solver->getMultiObjectCustom())->getTriangleMaterial(hitTriangle,NULL,NULL);
		material = showPhysical ? materialPhysical : materialCustom;
	}

	updateProperties();

	updateIsInStaticScene();
}

void SVMaterialProperties::OnIdle(wxIdleEvent& event)
{
	if (IS_SHOWN(this))
	{
		defocusButtonEditor();
	}
}

void SVMaterialProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	bool diffuseChanged = false;
	bool specularChanged = false;
	bool emittanceChanged = false;
	bool transmittanceChanged = false;
	bool textureChanged = false;

	wxPGProperty *property = event.GetProperty();

	// propagate change from wx to material
	if (property==propPoint || property==propPhysical)
	{
		setMaterial(lastSolver,lastObject,lastTriangle,lastPoint2d); // load different material
	}
	if (!material)
		return;

	if (property==propName)
	{
		material->name = RR_WX2RR(property->GetValue().GetString());
	}
	else
	if (property==propFront)
	{
		rr::RRSideBits visible[3] = {
			{1,1,1,1,1,1,1}, // checked
			{0,0,1,1,0,0,1}, // unchecked (the other side is checked), catchFrom is set to preserve opacity
			{0,0,0,1,0,0,0}}; // unchecked (the other side is unchecked), catchFrom is disabled to enforce 100% transparency
		material->sideBits[0] = visible[property->GetValue().GetBool()?0:(propBack->GetValue().GetBool()?1:2)];
		transmittanceChanged = true;
	}
	else
	if (property==propBack)
	{
		rr::RRSideBits visible[3] = {
			{1,0,1,1,1,1,1}, // checked
			{0,0,1,1,0,0,1}, // unchecked (the other side is checked), catchFrom is set to preserve opacity
			{0,0,0,1,0,0,0}}; // unchecked (the other side is unchecked), catchFrom is disabled to enforce 100% transparency
		material->sideBits[1] = visible[property->GetValue().GetBool()?0:(propFront->GetValue().GetBool()?1:2)];
		transmittanceChanged = true;
	}
	else

	// - diffuseReflectance
	if (property==propDiffuse->GetPropertyByName(_("uv")))
	{
		material->diffuseReflectance.texcoord = property->GetValue().GetInteger();
		diffuseChanged = true;
	}
	else
	if (property==propDiffuse->GetPropertyByName(_("texture or video")))
	{
		((ImageFileProperty*)property)->updateBufferAndIcon(material->diffuseReflectance.texture,svs.playVideos);
		material->diffuseReflectance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP,false);
		updateProperty(propDiffuse->GetPropertyByName(_("color")),material->diffuseReflectance.color);
		composeMaterialPropertyRoot(propDiffuse,material->diffuseReflectance);
		diffuseChanged = true;
		textureChanged = true;
	}
	else
	if (property==propDiffuse->GetPropertyByName(_("color")))
	{
		material->diffuseReflectance.color << property->GetValue();
		composeMaterialPropertyRoot(propDiffuse,material->diffuseReflectance);
		diffuseChanged = true;
	}
	else

	// - specularReflectance
	if (property==propSpecular->GetPropertyByName(_("uv")))
	{
		material->specularReflectance.texcoord = property->GetValue().GetInteger();
		specularChanged = true;
	}
	else
	if (property==propSpecular->GetPropertyByName(_("texture or video")))
	{
		((ImageFileProperty*)property)->updateBufferAndIcon(material->specularReflectance.texture,svs.playVideos);
		material->specularReflectance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP,false);
		updateProperty(propSpecular->GetPropertyByName(_("color")),material->specularReflectance.color);
		composeMaterialPropertyRoot(propSpecular,material->specularReflectance);
		specularChanged = true;
		textureChanged = true;
	}
	else
	if (property==propSpecular->GetPropertyByName(_("color")))
	{
		material->specularReflectance.color << property->GetValue();
		composeMaterialPropertyRoot(propSpecular,material->specularReflectance);
		specularChanged = true;
	}
	else
	if (property==propSpecularModel)
	{
		material->specularModel = (rr::RRMaterial::SpecularModel)(property->GetValue().GetInteger());
		switch (material->specularModel)
		{
			case rr::RRMaterial::PHONG:
			case rr::RRMaterial::BLINN_PHONG:
				if (material->specularShininess<=0)
					material->specularShininess = 1000000;
				else if (material->specularShininess<1)
					material->specularShininess = 1/material->specularShininess;
				updateFloat(propSpecularShininess,material->specularShininess);
				break;
			case rr::RRMaterial::TORRANCE_SPARROW:
			case rr::RRMaterial::BLINN_TORRANCE_SPARROW:
				if (material->specularShininess>1)
					material->specularShininess = 1/material->specularShininess;
				updateFloat(propSpecularRoughness,material->specularShininess);
				break;
		}
		updateHide();
	}
	else
	if (property==propSpecularShininess || property==propSpecularRoughness)
	{
		material->specularShininess = property->GetValue().GetDouble();
	}
	else

	// - diffuseEmittance
	if (property==propEmissive->GetPropertyByName(_("uv")))
	{
		material->diffuseEmittance.texcoord = property->GetValue().GetInteger();
		emittanceChanged = true;
	}
	else
	if (property==propEmissive->GetPropertyByName(_("texture or video")))
	{
		((ImageFileProperty*)property)->updateBufferAndIcon(material->diffuseEmittance.texture,svs.playVideos);
		material->diffuseEmittance.updateColorFromTexture(NULL,false,rr::RRMaterial::UTA_KEEP,false);
		updateProperty(propEmissive->GetPropertyByName(_("color")),material->diffuseEmittance.color);
		composeMaterialPropertyRoot(propEmissive,material->diffuseEmittance);
		emittanceChanged = true;
		textureChanged = true;
	}
	else
	if (property==propEmissive->GetPropertyByName(_("color")))
	{
		material->diffuseEmittance.color << property->GetValue();
		composeMaterialPropertyRoot(propEmissive,material->diffuseEmittance);
		emittanceChanged = true;
	}
	else

	// - specularTransmittance
	if (property==propTransparent->GetPropertyByName(_("uv")))
	{
		material->specularTransmittance.texcoord = property->GetValue().GetInteger();
		transmittanceChanged = true;
	}
	else
	if (property==propTransparent->GetPropertyByName(_("texture or video")))
	{
		((ImageFileProperty*)property)->updateBufferAndIcon(material->specularTransmittance.texture,svs.playVideos);
		if (material->specularTransmittance.texture && material->specularTransmittance.texture->getFormat()!=rr::BF_RGBA && material->specularTransmittance.texture->getFormat()!=rr::BF_RGBAF)
			material->specularTransmittanceInAlpha = false;
		material->specularTransmittance.updateColorFromTexture(NULL,material->specularTransmittanceInAlpha,rr::RRMaterial::UTA_KEEP,false);
		updateProperty(propTransparent->GetPropertyByName(_("color")),material->specularTransmittance.color);
		composeMaterialPropertyRoot(propTransparent,material->specularTransmittance);
		material->updateKeyingFromTransmittance();
		propTransparency1bit->SetValue(material->specularTransmittanceKeyed);
		transmittanceChanged = true;
		textureChanged = true;
	}
	else
	if (property==propTransparent->GetPropertyByName(_("color")))
	{
		material->specularTransmittance.color << property->GetValue();
		composeMaterialPropertyRoot(propTransparent,material->specularTransmittance);
		propTransparency1bit->SetValue(material->specularTransmittanceKeyed = material->specularTransmittance.color==rr::RRVec3(0) || material->specularTransmittance.color==rr::RRVec3(1));
		transmittanceChanged = true;
	}
	else

	// - bumpMap
	if (property==propBumpMap->GetPropertyByName(_("uv")))
	{
		material->bumpMap.texcoord = property->GetValue().GetInteger();
		transmittanceChanged = true; // update fresnel shadows
	}
	else
	if (property==propBumpMap->GetPropertyByName(_("texture or video")))
	{
		((ImageFileProperty*)property)->updateBufferAndIcon(material->bumpMap.texture,svs.playVideos);
		composeMaterialPropertyRoot(propBumpMap,material->bumpMap);
		material->updateBumpMapType();
		updateBool(propBumpType,material->bumpMapTypeHeight);
		textureChanged = true;
		transmittanceChanged = true; // update fresnel shadows
		updateHide();
	}
	else
	if (property==propBumpType)
	{
		material->bumpMapTypeHeight = property->GetValue().GetBool();
		transmittanceChanged = true; // update fresnel shadows
		updateHide();
	}
	else
	if (property==propBumpMultiplier1)
	{
		material->bumpMap.color.x = property->GetValue().GetDouble();
		transmittanceChanged = true; // update fresnel shadows
	}
	else
	if (property==propBumpMultiplier2)
	{
		material->bumpMap.color.y = property->GetValue().GetDouble();
	}
	else

	// - the rest
	if (property==propTransparency1bit)
	{
		material->specularTransmittanceKeyed = property->GetValue().GetBool();
		transmittanceChanged = true;
		updateHide();
	}
	else
	if (property==propTransparencyThreshold)
	{
		material->specularTransmittanceThreshold = property->GetValue().GetDouble();
		transmittanceChanged = true; // update shadows
	}
	else
	if (property==propTransparencyInAlpha)
	{
		material->specularTransmittanceInAlpha = property->GetValue().GetBool();
		transmittanceChanged = true;
	}
	else
	if (property==propTransparencyMapInverted)
	{
		material->specularTransmittanceMapInverted = property->GetValue().GetBool();
		if (material->specularTransmittance.texture)
			material->specularTransmittance.color = rr::RRVec3(1)-material->specularTransmittance.color;
		transmittanceChanged = true;
	}
	else
	if (property==propRefraction)
	{
		material->refractionIndex = property->GetValue().GetDouble();
		transmittanceChanged = true; // update fresnel shadows
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
	if (!showPoint && materialPhysical && materialCustom && lastSolver)
	{
		if (showPhysical)
		{
			materialCustom->copyFrom(*materialPhysical);
			materialCustom->convertToCustomScale(lastSolver->getScaler());
		}
		else
		{
			materialPhysical->copyFrom(*materialCustom);
			materialPhysical->convertToPhysicalScale(lastSolver->getScaler());
		}
	}

	// add/remove cubemaps after change in diffuse or specular
	if (diffuseChanged || specularChanged)
	{
		// diffuse sphere doesn't have specular cube
		// when we change material to specular, specular cube is allocated here
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
	}

	// after adding/deleting texture, readonly changes
	if (textureChanged)
	{
		updateReadOnly();
	}

	if (lastSolver)
	{
		// only changes in static scene need to be reported
		if (materialIsInStaticScene)
			lastSolver->reportMaterialChange(transmittanceChanged,true);
	}

}

BEGIN_EVENT_TABLE(SVMaterialProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVMaterialProperties::OnPropertyChange)
	EVT_IDLE(SVMaterialProperties::OnIdle)
END_EVENT_TABLE()



}; // namespace
