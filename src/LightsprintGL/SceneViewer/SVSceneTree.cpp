// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneTree.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
#include "SVLightProperties.h"
#include "SVLog.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// ItemData

class ItemData : public wxTreeItemData
{
public:
	ItemData(EntityId _entityId) : entityId(_entityId)
	{
	}
	EntityId entityId;
};

/////////////////////////////////////////////////////////////////////////////
//
// SVSceneTree

SVSceneTree::SVSceneTree(SVFrame* _svframe)
	: wxTreeCtrl(_svframe, wxID_ANY, wxDefaultPosition, wxSize(300,300), wxTR_HAS_BUTTONS|wxTR_MULTIPLE|SV_SUBWINDOW_BORDER), svs(_svframe->svs)
{
	svframe = _svframe;
	callDepth = 0;
	needsUpdateContent = false;


	root = AddRoot(_("root"));


	lights = AppendItem(root,"");

	staticObjects = AppendItem(root,"");
	dynamicObjects = AppendItem(root,"");


	Expand(lights); // wxmsw ignores this because lights is empty
	Expand(root); // must go after appends, otherwise it does not expand
}

void SVSceneTree::updateContent(RRDynamicSolverGL* solver)
{
	if (callDepth)
	{
		needsUpdateContent = true;
		return;
	}
	callDepth++;

	//#define USE_IF_NONEMPTY_ELSE(str,maxlength) str.size() ? (str.size()>maxlength?std::string("...")+(str.c_str()+str.size()-maxlength):str) :
	#define USE_IF_NONEMPTY_ELSE(str,maxlength) str.size() ? str :
	SetItemText(GetRootItem(),
		USE_IF_NONEMPTY_ELSE(svs.sceneFilename,40)
		_("scene"));

	// update lights
	if (solver)
	{
		SetItemText(lights,wxString::Format(_("%d lights"),solver?solver->getLights().size():0));
		DeleteChildren(lights);
		for (unsigned i=0;solver && i<solver->getLights().size();i++)
		{
			wxString name = RR_RR2WX(solver->getLights()[i]->name);
			if (name.empty()) name = wxString::Format(_("light %d"),i);
			AppendItem(lights,name,-1,-1,new ItemData(EntityId(ST_LIGHT,i)));
		}
	}

	// update static objects
	unsigned numStaticObjects = solver->getStaticObjects().size();
	if (solver)
	{
		SetItemText(staticObjects,wxString::Format(_("%d static objects"),solver?solver->getStaticObjects().size():0));
		DeleteChildren(staticObjects);
		for (unsigned i=0;solver && i<numStaticObjects;i++)
		{
			wxString name = RR_RR2WX(solver->getStaticObjects()[i]->name);
			if (name.empty()) name = wxString::Format(_("object %d"),i);
			AppendItem(staticObjects,name,-1,-1,new ItemData(EntityId(ST_OBJECT,i)));
		}
	}

	// update dynamic objects
	if (solver)
	{
		SetItemText(dynamicObjects,wxString::Format(_("%d dynamic objects"),solver?solver->getDynamicObjects().size():0));
		DeleteChildren(dynamicObjects);
		unsigned numDynamicObjects = solver->getDynamicObjects().size();
		for (unsigned i=0;solver && i<numDynamicObjects;i++)
		{
			wxString name = RR_RR2WX(solver->getDynamicObjects()[i]->name);
			if (name.empty()) name = wxString::Format(_("object %d"),i);
			AppendItem(dynamicObjects,name,-1,-1,new ItemData(EntityId(ST_OBJECT,numStaticObjects+i)));
		}
	}


	// tree rebuild moved cursor. move it back
	selectEntityInTree(svframe->getSelectedEntity());

	updateSelectedEntityIds();

	needsUpdateContent = false;
	callDepth--;
}

wxTreeItemId SVSceneTree::entityIdToItemId(EntityId entity) const
{
	wxTreeItemId searchRoot;
	switch (entity.type)
	{
		case ST_LIGHT: searchRoot = lights; break;
		case ST_OBJECT:
			if (entity.index<svframe->m_canvas->solver->getStaticObjects().size())
				searchRoot = staticObjects;
			else
			{
				searchRoot = dynamicObjects;
				entity.index -= svframe->m_canvas->solver->getStaticObjects().size();
			}
			break;
		case ST_CAMERA: return wxTreeItemId();
	}
	if (!searchRoot.IsOk())
	{
		// requested entity does not exist
		return wxTreeItemId();
	}
	wxTreeItemIdValue cookie;
	wxTreeItemId item = GetFirstChild(searchRoot,cookie);
	for (unsigned i=0;i<entity.index;i++)
	{
		item = GetNextChild(searchRoot,cookie);
	}
	return item;
}

EntityId SVSceneTree::itemIdToEntityId(wxTreeItemId item) const
{
	ItemData* data = item.IsOk() ? (ItemData*)GetItemData(item) : NULL;
	return data ? data->entityId : EntityId();
}

// returns number of transformed entities
static unsigned manipulateCamera(Camera& camera, const rr::RRMatrix3x4& transformation, bool rollChangeAllowed)
{
	float oldRoll = camera.yawPitchRollRad[2];
	rr::RRMatrix3x4 matrix(camera.inverseViewMatrix,true);
	matrix = transformation * matrix;
	RRVec3 newRot = matrix.getYawPitchRoll();
	if (abs(abs(camera.yawPitchRollRad.x-newRot.x)-RR_PI)<RR_PI/2) // rot.x change is closer to -180 or 180 than to 0 or 360. this happens when rot.y overflows 90 or -90
		return 0;
	camera.pos = matrix.getTranslation();
	camera.yawPitchRollRad = newRot;
	if (!rollChangeAllowed) // prevent unwanted roll distortion (yawpitch changes would accumulate rounding errors in roll)
		camera.yawPitchRollRad[2] = oldRoll;
	camera.update();
	return 1;
}

// returns number of modified entities
// - nothing is modified by identity matrix
// - camera is not modified if it would overflow pitch from -90,90 range
unsigned SVSceneTree::manipulateEntity(EntityId entity, const rr::RRMatrix3x4& transformation, bool rollChangeAllowed)
{
	if (!svframe->m_canvas)
		return 0;
	if (transformation.isIdentity()) // without this, we would transform camera by identity in every frame. such transformation converts euler angles to matrix and back = float errors accumulate over time, "front" view stops being exactly front etc
		return 0;
	RRDynamicSolverGL* solver = svframe->m_canvas->solver;
	switch(entity.type)
	{
		case ST_OBJECT:
			{
				rr::RRObject* object = solver->getObject(entity.index);
				if (object->isDynamic)
				{
					rr::RRMatrix3x4 matrix = transformation * object->getWorldMatrixRef();
					object->setWorldMatrix(&matrix);
					rr::RRVec3 center;
					object->getCollider()->getMesh()->getAABB(NULL,NULL,&center);
					matrix.transformPosition(center);
					object->illumination.envMapWorldCenter = center;
					solver->reportDirectIlluminationChange(-1,true,false,false);
					if (svframe->m_objectProperties->object==object)
						svframe->m_objectProperties->updateProperties();
				}
				return 1;
			}
			break;
		case ST_LIGHT:
			{
				RealtimeLight* rtlight = solver->realtimeLights[entity.index];
				manipulateCamera(*rtlight->getParent(),transformation,rollChangeAllowed);
				rtlight->updateAfterRealtimeLightChanges();
				solver->reportDirectIlluminationChange(entity.index,true,true,true);
				return 1;
			}
			break;
		case ST_CAMERA:
			{
				return manipulateCamera(svs.eye,transformation,rollChangeAllowed);
			}
			break;
	}
	return 0;
}

unsigned SVSceneTree::manipulateEntities(const EntityIds& entityIds, const rr::RRMatrix3x4& transformation, bool rollChangeAllowed)
{
	unsigned result = 0;
	for (EntityIds::const_iterator i=entityIds.begin();i!=entityIds.end();++i)
		result += manipulateEntity(*i,transformation,rollChangeAllowed);
	return result;
}

void SVSceneTree::selectEntityInTree(EntityId entity)
{
	wxTreeItemId item = entityIdToItemId(entity);
	UnselectAll();
	if (item.IsOk())
		SelectItem(item,true);
}

void SVSceneTree::updateSelectedEntityIds()
{
	selectedEntityIds.clear();
	wxArrayTreeItemIds selections;
	GetSelections(selections);
	for (unsigned i=0;i<selections.size();i++)
		if (selections[i].IsOk())
		{
			if (selections[i]==lights)
			{
				unsigned numLights = svframe->m_canvas->solver->getLights().size();
				for (unsigned i=0;i<numLights;i++)
					selectedEntityIds.insert(EntityId(ST_LIGHT,i));
			}
			else
			if (selections[i]==staticObjects)
			{
				unsigned numStaticObjects = svframe->m_canvas->solver->getStaticObjects().size();
				for (unsigned i=0;i<numStaticObjects;i++)
					selectedEntityIds.insert(EntityId(ST_OBJECT,i));
			}
			else
			if (selections[i]==dynamicObjects)
			{
				unsigned numStaticObjects = svframe->m_canvas->solver->getStaticObjects().size();
				unsigned numDynamicObjects = svframe->m_canvas->solver->getDynamicObjects().size();
				for (unsigned i=0;i<numDynamicObjects;i++)
					selectedEntityIds.insert(EntityId(ST_OBJECT,numStaticObjects+i));
			}
			else
			{
				EntityId entityId = itemIdToEntityId(selections[i]);
				selectedEntityIds.insert(entityId);
			}
		}
}

const EntityIds& SVSceneTree::getEntityIds(SVSceneTree::ManipulatedEntityIds preference) const
{
	static EntityIds cameraEntityIds = EntityId(ST_CAMERA,0);
	switch (preference)
	{
		case MEI_CAMERA:
			return cameraEntityIds;
		case MEI_SELECTED:
			return selectedEntityIds;
	}
	bool atLeastOneMovableSelected = false;
	for (EntityIds::const_iterator i=selectedEntityIds.begin();i!=selectedEntityIds.end();++i)
	{
		atLeastOneMovableSelected |= i->type==ST_LIGHT || (i->type==ST_OBJECT && i->index>=svframe->m_canvas->solver->getStaticObjects().size());
	}
	return atLeastOneMovableSelected ? selectedEntityIds : cameraEntityIds;
}

rr::RRVec3 SVSceneTree::getCenterOf(const EntityIds& entityIds) const
{
	rr::RRVec3 selectedEntitiesCenter(0);
	unsigned numCenters = 0;
	for (EntityIds::const_iterator i=entityIds.begin();i!=entityIds.end();++i)
	{
		switch (i->type)
		{
			case ST_LIGHT:
				selectedEntitiesCenter += svframe->m_canvas->solver->getLights()[i->index]->position;
				numCenters++;
				break;
			case ST_OBJECT:
				{
					rr::RRObject* object = svframe->m_canvas->solver->getObject(i->index);
					rr::RRVec3 center;
					object->getCollider()->getMesh()->getAABB(NULL,NULL,&center);
					selectedEntitiesCenter += object->getWorldMatrixRef().getTransformedPosition(center);
					numCenters++;
				}
				break;
			case ST_CAMERA:
				selectedEntitiesCenter += svs.eye.pos;
				numCenters++;
				break;
		}
	}
	if (numCenters)
		selectedEntitiesCenter /= numCenters;
	return selectedEntitiesCenter;
}

void SVSceneTree::OnSelChanged(wxTreeEvent& event)
{
	if (callDepth)
		return;
	callDepth++;

	updateSelectedEntityIds();

	if (event.GetItem().IsOk() && IsSelected(event.GetItem())) // don't update panels when deselecting (we especially don't want to update material props, because it would uncheck [x]physical, [x]point)
	{
		EntityId entityId = itemIdToEntityId(event.GetItem());
		svframe->selectEntityInTreeAndUpdatePanel(entityId,SEA_NOTHING); // it's already selected, don't change selection, just update panels
	}

	callDepth--;	
	if (needsUpdateContent)
	{
//		updateContent(NULL);
	}
}

// activated by doubleclick, space
void SVSceneTree::OnItemActivated(wxTreeEvent& event)
{
	ToggleItemSelection(event.GetItem());
}

// creates menu for selected entities (roughly what's in GetSelections(), event.GetItem() is ignored)
void SVSceneTree::OnContextMenuCreate(wxTreeEvent& event)
{
	wxMenu menu;

	// add menu items that need uniform context
	const EntityIds& entityIds = getEntityIds(SVSceneTree::MEI_SELECTED);
	bool entityIdsUniform = true;
	for (EntityIds::const_iterator i=entityIds.begin();i!=entityIds.end();++i)
		if (i->type!=entityIds.begin()->type)
			entityIdsUniform = false;
	if (entityIdsUniform)
	{
		wxArrayTreeItemIds temporaryContextItems;
		wxTreeItemId temporaryContext;
		if (GetSelections(temporaryContextItems))
			temporaryContext = temporaryContextItems[0];
		bool contextIsSky = !temporaryContext.IsOk();
		if (temporaryContext==root || contextIsSky)
		{
			menu.Append(CM_ENV_CUSTOM,_("Change environment..."),_("Changes environment texture."));
			menu.Append(CM_ENV_WHITE,_("White environment"),_("Sets white environment."));
			menu.Append(CM_ENV_BLACK,_("Black environment"),_("Sets black environment."));
			menu.AppendSeparator();
			menu.Append(CM_ROOT_SCALE, _("Normalize units..."),_("Makes scene n-times bigger."));
			menu.Append(CM_ROOT_AXES, _("Normalize up-axis"),_("Rotates scene by 90 degrees."));
			if (contextIsSky)
				menu.AppendSeparator();
		}
		if (temporaryContext==lights)
		{
			menu.Append(CM_LIGHT_DIR, _("Add Sun light"));
			menu.Append(CM_LIGHT_SPOT, _("Add spot light")+" (alt-s)");
			menu.Append(CM_LIGHT_POINT, _("Add point light")+" (alt-o)");
			menu.Append(CM_LIGHT_FLASH, _("Toggle flashlight")+" (alt-f)");
		}
		if (temporaryContext.IsOk() && GetItemParent(temporaryContext)==lights)
		{
			if (!svframe->m_lightProperties->IsShown())
				menu.Append(SVFrame::ME_WINDOW_LIGHT_PROPERTIES, _("Properties..."));
		}
		if (entityIds.size()) // prevents assert when right clicking empty objects
		if (temporaryContext==staticObjects
			|| temporaryContext==dynamicObjects
			|| (temporaryContext.IsOk() && GetItemParent(temporaryContext)==staticObjects)
			|| (temporaryContext.IsOk() && GetItemParent(temporaryContext)==dynamicObjects))
		{
			bool selectedExactlyAllStaticObjects = entityIds.rbegin()->index+1==svframe->m_canvas->solver->getStaticObjects().size();
			bool selectedOnlyStaticObjects = entityIds.rbegin()->index<svframe->m_canvas->solver->getStaticObjects().size();
			if (selectedExactlyAllStaticObjects || svframe->userPreferences.testingBeta) // is safe only for all objects at once because lightmapTexcoord is in material, not in RRObject, we can't change only selected objects without duplicating materials
				menu.Append(CM_OBJECTS_UNWRAP,_("Build unwrap..."),_("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
			if (selectedOnlyStaticObjects)
			{
				menu.Append(CM_OBJECTS_BUILD_LMAPS,_("Build lightmaps..."),_("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel requires unwrap."));
				menu.Append(CM_OBJECTS_BUILD_LDMS,_("Build LDMs..."),_("(Re)builds LDMs, layer of additional per-pixel details. LDMs require unwrap."));
			}
			if (temporaryContext!=staticObjects && temporaryContextItems.size()==1)
				menu.Append(CM_OBJECT_INSPECT_UNWRAP,_("Inspect unwrap,lightmap,LDM..."),_("Shows unwrap and lightmap or LDM in 2D."));
			if (temporaryContextItems.size()>1)
				menu.Append(CM_OBJECTS_MERGE,_("Merge objects"),_("Merges objects together."));
			menu.Append(CM_OBJECTS_SMOOTH,_("Smooth..."),_("Rebuild objects to have smooth normals."));
			if (svframe->userPreferences.testingBeta) // is in beta because tangents have no effect
				menu.Append(CM_OBJECTS_TANGENTS,_("Build tangents"),_("Rebuild objects to have tangents and bitangents."));
			menu.Append(CM_OBJECTS_DELETE_DIALOG,_("Delete components..."),_("Deletes components within objects."));
			if (temporaryContext!=staticObjects && !svframe->m_objectProperties->IsShown() && temporaryContextItems.size()==1)
				menu.Append(SVFrame::ME_WINDOW_OBJECT_PROPERTIES, _("Properties..."));
		}
	}

	// add menu items that work with non uniform context
	if (entityIds.size())
		menu.Append(CM_DELETE, _("Delete")+" (del)");

	PopupMenu(&menu, event.GetPoint());
}

void SVSceneTree::OnContextMenuRun(wxCommandEvent& event)
{
	runContextMenuAction(event.GetId(),getEntityIds(SVSceneTree::MEI_SELECTED));
}

extern bool getQuality(wxString title, wxWindow* parent, unsigned& quality);
extern bool getResolution(wxString title, wxWindow* parent, unsigned& resolution, bool offerPerVertex);
extern bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption);

void SVSceneTree::runContextMenuAction(unsigned actionCode, const EntityIds contextEntityIds)
{
	callDepth++;
	svframe->commitPropertyChanges();
	callDepth--;

	RRDynamicSolverGL* solver = svframe->m_canvas->solver;

	// what objects to process, code shared by many actions
	rr::RRObjects allObjects = solver->getStaticObjects();
	allObjects.insert(allObjects.end(),solver->getDynamicObjects().begin(),solver->getDynamicObjects().end());
	rr::RRObjects selectedObjects;
	rr::RRObjects selectedObjectsAndInstances; // some tasks have to process instances of selected objects too
	for (unsigned i=0;i<allObjects.size();i++)
	{
		if (contextEntityIds.find(EntityId(ST_OBJECT,i))!=contextEntityIds.end())
			selectedObjects.push_back(allObjects[i]);
		for (EntityIds::const_iterator j=contextEntityIds.begin();j!=contextEntityIds.end();++j)
			if (j->type==ST_OBJECT && allObjects[j->index]->getCollider()->getMesh()==allObjects[i]->getCollider()->getMesh())
			{
				selectedObjectsAndInstances.push_back(allObjects[i]);
				break;
			}
	}

	if (actionCode>=SVFrame::ME_FIRST)
	{
		svframe->OnMenuEventCore2(actionCode);
	}
	else
	switch (actionCode)
	{
		case CM_ROOT_SCALE:
			{
				static float currentUnitLengthInMeters = 1;
				if (getFactor(this,currentUnitLengthInMeters,_("Enter current unit length in meters."),_("Normalize units")))
				{
					rr::RRScene scene;
					scene.objects = allObjects;
					scene.lights = solver->getLights();
					scene.normalizeUnits(currentUnitLengthInMeters);
					svframe->m_canvas->addOrRemoveScene(NULL,true);
				}
			}
			break;
		case CM_ROOT_AXES:
			{
				static unsigned currentUpAxis = 0;
				rr::RRScene scene;
				scene.objects = allObjects;
				scene.lights = solver->getLights();
				scene.normalizeUpAxis(currentUpAxis);
				svframe->m_canvas->addOrRemoveScene(NULL,true);
				currentUpAxis = 2-currentUpAxis;
			}
			break;

		case CM_ENV_CUSTOM: svframe->OnMenuEventCore2(SVFrame::ME_ENV_OPEN); return;
		case CM_ENV_WHITE: svframe->OnMenuEventCore2(SVFrame::ME_ENV_WHITE); return;
		case CM_ENV_BLACK: svframe->OnMenuEventCore2(SVFrame::ME_ENV_BLACK); return;

		case CM_LIGHT_SPOT:
		case CM_LIGHT_POINT:
		case CM_LIGHT_DIR:
			if (solver)
			{
				rr::RRLights newList = solver->getLights();
				rr::RRLight* newLight = NULL;
				switch (actionCode)
				{
					case CM_LIGHT_DIR: newLight = rr::RRLight::createDirectionalLight(rr::RRVec3(-1),rr::RRVec3(1),true); newLight->name = "Sun"; break;
					case CM_LIGHT_SPOT: newLight = rr::RRLight::createSpotLight(svs.eye.pos,rr::RRVec3(1),svs.eye.dir,svs.eye.getFieldOfViewVerticalRad()/2,svs.eye.getFieldOfViewVerticalRad()/4); break;
					case CM_LIGHT_POINT: newLight = rr::RRLight::createPointLight(svs.eye.pos,rr::RRVec3(1)); break;
				}
				if (newLight)
				{
					svframe->m_canvas->lightsToBeDeletedOnExit.push_back(newLight);
					newList.push_back(newLight);
					solver->setLights(newList); // RealtimeLight in light props is deleted here
					if (actionCode==CM_LIGHT_DIR)
						svframe->simulateSun(); // when inserting sun, move it to simulated direction (it would be better to simulate only when inserting first dirlight, because simulation affects only first dirlight)
					svframe->updateAllPanels();
				}
			}
			break;
		case CM_LIGHT_FLASH:
			if (solver)
			{
				bool containsFlashlight[2] = {false,false};
				rr::RRLights lights = solver->getLights();
				for (unsigned i=lights.size();i--;)
					if (lights[i] && lights[i]->type==rr::RRLight::SPOT && lights[i]->name=="Flashlight")
						containsFlashlight[lights[i]->enabled?1:0] = true;
				if (containsFlashlight[0] || containsFlashlight[1])
				{
					// toggle all flashlights
					for (unsigned i=lights.size();i--;)
						if (lights[i] && lights[i]->type==rr::RRLight::SPOT && lights[i]->name=="Flashlight")
							lights[i]->enabled = !lights[i]->enabled;
				}
				else
				{
					// insert one flashlight
					rr::RRLight* newLight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(0),rr::RRVec3(1),rr::RRVec3(1),0.5f,0.1f);
					newLight->name = "Flashlight";
					lights.push_back(newLight);
					solver->setLights(lights); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
					svframe->updateAllPanels();
				}
			}
			break;

		case CM_OBJECTS_UNWRAP:
			if (solver)
			{
				unsigned res = 256;
				if (getResolution(_("Unwrap build"),this,res,false))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building unwrap..."));

					selectedObjectsAndInstances.smoothAndStitch(false,false,true,false,0,0,0,false); // remove degens, unwrapper crashes on them
					selectedObjectsAndInstances.deleteComponents(false,true,true,false);
					selectedObjectsAndInstances.buildUnwrap(res,solver->aborting);

					// static objects may be modified even after abort (unwrap is not atomic)
					// so it's better if following setStaticObjects is not aborted
					solver->aborting = false;

					svframe->m_canvas->addOrRemoveScene(NULL,true); // calls svframe->updateAllPanels();
				}
			}
			break;

		case CM_OBJECTS_BUILD_LMAPS:
			if (solver)
			{
				unsigned res = 256; // 0=vertex buffers
				unsigned quality = 100;
				if (getResolution(_("Lightmap build"),this,res,true) && getQuality(_("Lightmap build"),this,quality))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building lightmaps..."));

					// allocate buffers in temp layer
					unsigned tmpLayer = 74529457;
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->getCollider()->getMesh()->getNumVertices())
							selectedObjects[i]->illumination.getLayer(tmpLayer) = res
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBA,true,NULL) // A in RGBA is only for debugging
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,selectedObjects[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);

					// update everything in temp layer
					solver->leaveFireball();
					svframe->m_canvas->fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					params.aoIntensity = svs.lightmapDirectParameters.aoIntensity;
					params.aoSize = svs.lightmapDirectParameters.aoSize;
					solver->updateLightmaps(tmpLayer,-1,-1,&params,&params,&svs.lightmapFilteringParameters);

					// save temp layer
					allObjects.saveLayer(tmpLayer,LMAP_PREFIX,LMAP_POSTFIX);

					// move buffers from temp to final layer
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->getCollider()->getMesh()->getNumVertices())
						{
							delete selectedObjects[i]->illumination.getLayer(svs.staticLayerNumber);
							selectedObjects[i]->illumination.getLayer(svs.staticLayerNumber) = selectedObjects[i]->illumination.getLayer(tmpLayer);
							selectedObjects[i]->illumination.getLayer(tmpLayer) = NULL;
						}

					// make results visible
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
				}
			}
			break;

		case CM_OBJECTS_BUILD_LDMS:
			if (solver)
			{
				unsigned res = 256;
				unsigned quality = 100;
				if (getQuality("LDM build",this,quality) && getResolution("LDM build",this,res,false))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building LDM..."));

					// if in fireball mode, leave it, otherwise updateLightmaps() below fails
					svframe->m_canvas->fireballLoadAttempted = false;
					solver->leaveFireball();

					// allocate buffers in temp layer
					unsigned tmpLayer = 74529457;
					for (unsigned i=0;i<selectedObjects.size();i++)
						selectedObjects[i]->illumination.getLayer(tmpLayer) = 
							rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);

					// update everything in temp layer
					rr::RRDynamicSolver::UpdateParameters paramsDirect(quality);
					paramsDirect.applyLights = 0;
					paramsDirect.aoIntensity = svs.lightmapDirectParameters.aoIntensity*2;
					paramsDirect.aoSize = svs.lightmapDirectParameters.aoSize;
					rr::RRDynamicSolver::UpdateParameters paramsIndirect(quality);
					paramsIndirect.applyLights = 0;
					paramsIndirect.locality = -1;
					paramsIndirect.qualityFactorRadiosity = 0;
					rr::RRBuffer* oldEnv = solver->getEnvironment();
					rr::RRBuffer* newEnv = rr::RRBuffer::createSky(rr::RRVec4(0.65f),rr::RRVec4(0.65f)); // 0.65*typical materials = average color in LDM around 0.5
					solver->setEnvironment(newEnv);
					solver->updateLightmaps(tmpLayer,-1,-1,&paramsDirect,&paramsIndirect,&svs.lightmapFilteringParameters);
					solver->setEnvironment(oldEnv);
					delete newEnv;

					// save temp layer
					allObjects.saveLayer(tmpLayer,LDM_PREFIX,LDM_POSTFIX);

					// move buffers from temp to final layer
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->getCollider()->getMesh()->getNumVertices())
						{
							delete selectedObjects[i]->illumination.getLayer(svs.ldmLayerNumber);
							selectedObjects[i]->illumination.getLayer(svs.ldmLayerNumber) = selectedObjects[i]->illumination.getLayer(tmpLayer);
							selectedObjects[i]->illumination.getLayer(tmpLayer) = NULL;
						}

					// make results visible
					if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS)
						svs.renderLightDirect = LD_REALTIME;
					if (svs.renderLightIndirect==LI_NONE || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)
						svs.renderLightIndirect = LI_CONSTANT;
					svs.renderLDM = true;
				}
			}
			break;

		case CM_OBJECT_INSPECT_UNWRAP:
			svframe->m_canvas->selectedType = ST_OBJECT;
			svs.selectedObjectIndex = contextEntityIds.begin()->index;
			svs.renderLightmaps2d = 1;
			// a) slower, rebuilds tree, triggers wx bug: stealing focus (select obj or light, focus to canvas, select inspect from context menu (tree is rebuilt here), focus goes to tree)
			//svframe->updateAllPanels();
			// b) faster, avoids wx bug (focus in canvas stays in canvas)
			svframe->selectEntityInTreeAndUpdatePanel(EntityId(ST_OBJECT,svs.selectedObjectIndex),SEA_SELECT);
			break;

		case CM_OBJECTS_SMOOTH: // right now, it smooths also dynamic objects
			{
				if (svframe->smoothDlg.ShowModal()==wxID_OK)
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Smoothing..."));

					double d = 0;
					float weldDistance = svframe->smoothDlg.weldDistance->GetValue().ToDouble(&d) ? (float)d : 0;
					float smoothAngle = svframe->smoothDlg.smoothAngle->GetValue().ToDouble(&d) ? (float)d : 30;
					float uvDistance = svframe->smoothDlg.uvDistance->GetValue().ToDouble(&d) ? (float)d : 0;
					if (selectedObjectsAndInstances.size())
					{
						selectedObjectsAndInstances.smoothAndStitch(
							svframe->smoothDlg.splitVertices->GetValue(),
							svframe->smoothDlg.stitchVertices->GetValue(),
							true,true,
							weldDistance,
							RR_DEG2RAD(smoothAngle),
							uvDistance,
							true);
						svframe->m_canvas->addOrRemoveScene(NULL,true); // calls svframe->updateAllPanels();
					}
				}
			}
			break;
		case CM_OBJECTS_MERGE:
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Merging objects..."));

				// don't merge tangents if sources clearly have no tangents
				bool tangents = false;
				{
					for (unsigned i=0;i<selectedObjects.size();i++)
					{
						const rr::RRObject* object = selectedObjects[i];
						const rr::RRMeshArrays* meshArrays = dynamic_cast<const rr::RRMeshArrays*>(object->getCollider()->getMesh());
						if (!meshArrays || meshArrays->tangent)
							tangents = true;
					}
				}

				rr::RRObject* oldObject = rr::RRObject::createMultiObject(&selectedObjects,rr::RRCollider::IT_LINEAR,solver->aborting,-1,-1,false,0,NULL);

				// convert oldObject with Multi* into newObject with RRMeshArrays
				// if we don't do it
				// - solver->getMultiObjectCustom() preimport numbers would point to many 1objects, although there is only one 1object now
				// - attempt to smooth scene would fail, it needs arrays
				const rr::RRCollider* oldCollider = oldObject->getCollider();
				const rr::RRMesh* oldMesh = oldCollider->getMesh();
				rr::RRVector<unsigned> texcoords;
				oldMesh->getUvChannels(texcoords);
				rr::RRMeshArrays* newMesh = oldMesh->createArrays(true,texcoords,tangents);
				rr::RRCollider* newCollider = rr::RRCollider::create(newMesh,NULL,rr::RRCollider::IT_LINEAR,solver->aborting);
				rr::RRObject* newObject = new rr::RRObject;
				newObject->faceGroups = oldObject->faceGroups;
				newObject->setCollider(newCollider);
				delete oldObject;

				rr::RRObjects newList;
				newList.push_back(newObject); // memleak, newObject is never freed
				for (unsigned objectIndex=0;objectIndex<allObjects.size();objectIndex++)
					if (contextEntityIds.find(EntityId(ST_OBJECT,objectIndex))==contextEntityIds.end())
						newList.push_back(allObjects[objectIndex]);
				solver->setStaticObjects(newList,NULL);
				solver->setDynamicObjects(newList);
				svframe->m_canvas->addOrRemoveScene(NULL,false); // calls svframe->updateAllPanels();
			}
			break;
		case CM_OBJECTS_TANGENTS:
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Building tangents..."));

				for (unsigned i=0;i<selectedObjects.size();i++)
				{
					rr::RRMeshArrays* arrays = dynamic_cast<rr::RRMeshArrays*>(const_cast<rr::RRMesh*>(selectedObjects[i]->getCollider()->getMesh()));
					if (arrays)
						arrays->buildTangents();
				}
				svframe->m_canvas->addOrRemoveScene(NULL,true); // calls svframe->updateAllPanels();
			}
			break;

		case CM_OBJECTS_DELETE_DIALOG:
			if (solver)
			if (svframe->deleteDlg.ShowModal()==wxID_OK)
			{
				selectedObjectsAndInstances.deleteComponents(svframe->deleteDlg.tangents->GetValue(),svframe->deleteDlg.unwrap->GetValue(),svframe->deleteDlg.unusedUvChannels->GetValue(),svframe->deleteDlg.emptyFacegroups->GetValue());
				svframe->m_canvas->addOrRemoveScene(NULL,true); // calls svframe->updateAllPanels(); // necessary at least after deleting empty facegroups
			}
			break;

		case CM_DELETE:
			if (solver)
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Deleting..."));

				// delete lights
				{
					rr::RRLights newList;
					for (unsigned lightIndex=0;lightIndex<solver->getLights().size();lightIndex++)
						if (contextEntityIds.find(EntityId(ST_LIGHT,lightIndex))==contextEntityIds.end())
						{
							// not to be deleted, copy it to new list
							newList.push_back(solver->getLights()[lightIndex]);
						}
						else
						{
							// to be deleted, stop video playback
							if (solver->getLights()[lightIndex]->rtProjectedTexture)
								solver->getLights()[lightIndex]->rtProjectedTexture->stop();
						}
					if (newList.size()!=solver->getLights().size())
					{
						solver->setLights(newList); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
					}
				}

				// delete objects
				{
					rr::RRObjects newList;
					for (unsigned objectIndex=0;objectIndex<allObjects.size();objectIndex++)
						if (contextEntityIds.find(EntityId(ST_OBJECT,objectIndex))==contextEntityIds.end())
							newList.push_back(allObjects[objectIndex]);
					if (newList.size()!=allObjects.size())
					{
						if (svs.playVideos)
						{
							// stop videos
							wxKeyEvent event;
							event.m_keyCode = ' ';
							svframe->m_canvas->OnKeyDown(event);
						}
						solver->setStaticObjects(newList,NULL);
						solver->setDynamicObjects(newList);
						svframe->m_canvas->addOrRemoveScene(NULL,false); // calls svframe->updateAllPanels();
					}
				}


				svframe->updateAllPanels();
			}
			break;
	}
}


void SVSceneTree::OnKeyDown(wxTreeEvent& event)
{
	wxKeyEvent keyEvent = event.GetKeyEvent();
	long code = keyEvent.GetKeyCode();
	if (code==WXK_LEFT||code==WXK_RIGHT||code==WXK_DOWN||code==WXK_UP||code==WXK_PAGEUP||code==WXK_PAGEDOWN||code==WXK_HOME||code==WXK_END)
	{
		// only for treectrl
		event.Skip();
	}
	else
	{
		// useless for treectrl, forward to canvas
		svframe->m_canvas->OnKeyDown(keyEvent);
	}
}

void SVSceneTree::OnKeyUp(wxKeyEvent& event)
{
	svframe->m_canvas->OnKeyUp(event);
}

BEGIN_EVENT_TABLE(SVSceneTree, wxTreeCtrl)
	EVT_TREE_SEL_CHANGED(-1,SVSceneTree::OnSelChanged)
	EVT_TREE_ITEM_ACTIVATED(-1,SVSceneTree::OnItemActivated)
	EVT_TREE_ITEM_MENU(-1,SVSceneTree::OnContextMenuCreate)
	EVT_MENU(-1,SVSceneTree::OnContextMenuRun)
	EVT_TREE_KEY_DOWN(-1,SVSceneTree::OnKeyDown)
	EVT_KEY_UP(SVSceneTree::OnKeyUp)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
