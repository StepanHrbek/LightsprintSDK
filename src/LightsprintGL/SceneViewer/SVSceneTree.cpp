// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneTree.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
#include "SVLightProperties.h"
#include "SVLog.h"
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

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
			rr::RRLight* light = solver->getLights()[i];
			wxString name = RR_RR2WX(light->name);
			if (name.empty())
				name = ((light->type==rr::RRLight::SPOT)?_("spot light"):((light->type==rr::RRLight::POINT)?_("point light"):_("sun light"))) + wxString::Format(" %d",i);
			if (!light->enabled)
				name += " [" + _("off") + "]";
			AppendItem(lights,name,-1,-1,new ItemData(EntityId(ST_LIGHT,i)));
		}
	}

	// update static objects
	if (solver)
	{
		SetItemText(staticObjects,wxString::Format(_("%d static objects"),solver?solver->getStaticObjects().size():0));
		DeleteChildren(staticObjects);
		unsigned numStaticObjects = solver->getStaticObjects().size();
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
		unsigned numStaticObjects = solver->getStaticObjects().size();
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

// returns number of modified entities
// - nothing is modified by identity matrix
// - camera is not modified if it would overflow pitch from -90,90 range
unsigned SVSceneTree::manipulateEntity(EntityId entity, const rr::RRMatrix3x4& transformation, bool preTransform, bool rollChangeAllowed)
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
					if (preTransform)
					{
						// preTransform = contains scale only, apply scale before worldMatrix, but preserve world center
						rr::RRVec3 localCenter(0);
						object->getCollider()->getMesh()->getAABB(NULL,NULL,&localCenter);
						matrix = object->getWorldMatrixRef() * transformation.centeredAround(localCenter);
					}
					object->setWorldMatrix(&matrix);
					object->updateIlluminationEnvMapCenter();
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
				unsigned result = rtlight->getCamera()->manipulateViewBy(transformation,rollChangeAllowed,false)?1:0;
				solver->reportDirectIlluminationChange(entity.index,true,true,true);
				return result;
			}
			break;
		case ST_CAMERA:
			{
				return svs.eye.manipulateViewBy(transformation,rollChangeAllowed,false)?1:0;
			}
			break;
	}
	return 0;
}

unsigned SVSceneTree::manipulateEntities(const EntityIds& entityIds, const rr::RRMatrix3x4& transformation, bool preTransform, bool rollChangeAllowed)
{
	unsigned result = 0;
	for (EntityIds::const_iterator i=entityIds.begin();i!=entityIds.end();++i)
		result += manipulateEntity(*i,transformation,preTransform,rollChangeAllowed);
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
	selectedObjectRoot = false;
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
				selectedObjectRoot = true;
				unsigned numStaticObjects = svframe->m_canvas->solver->getStaticObjects().size();
				for (unsigned i=0;i<numStaticObjects;i++)
					selectedEntityIds.insert(EntityId(ST_OBJECT,i));
			}
			else
			if (selections[i]==dynamicObjects)
			{
				selectedObjectRoot = true;
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
				{
					const rr::RRLights& lights = svframe->m_canvas->solver->getLights();
					rr::RRVec3 lightPos = lights[i->index]->position;
					if (lights[i->index]->type==rr::RRLight::DIRECTIONAL)
					{
						lightPos = svframe->m_canvas->sunIconPosition;
						for (unsigned j=0;j<i->index;j++)
							if (lights[j]->type==rr::RRLight::DIRECTIONAL)
								lightPos.y += 4*svframe->m_canvas->renderedIcons.iconSize; // the same constant is in addLights()
					}
					selectedEntitiesCenter += lightPos;
					numCenters++;
				}
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
				selectedEntitiesCenter += svs.eye.getPosition();
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
	wxMenu menu; // is PopupMenu, does not have to be allocated on heap

	// add menu items that need uniform context
	const EntityIds& entityIds = getEntityIds(SVSceneTree::MEI_SELECTED);
	bool entityIdsUniform = true;
	unsigned numObjects = 0;
	unsigned numObjectsStatic = 0;
	unsigned numLights = 0;
	unsigned numOthers = 0;
	for (EntityIds::const_iterator i=entityIds.begin();i!=entityIds.end();++i)
	{
		if (i->type!=entityIds.begin()->type)
			entityIdsUniform = false;
		if (i->type==ST_OBJECT)
		{
			numObjects++;
			if (!svframe->m_canvas->solver->getObject(i->index)->isDynamic) numObjectsStatic++;
		}
		else
		if (i->type==ST_LIGHT)
			numLights++;
		else
			numOthers++;
	}
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
			if (!IS_SHOWN(svframe->m_lightProperties))
				menu.Append(SVFrame::ME_WINDOW_LIGHT_PROPERTIES, _("Properties..."));
		}
		if (temporaryContext==dynamicObjects)
		{
			wxMenu* submenu = new wxMenu; // not PopupMenu, must be allocated on heap
			submenu->Append(CM_OBJECTS_ADD_PLANE,_("Plane"),_("Adds very large plane to scene. You can turn it into water surface by changing material. Keep it dynamic object to preserve GI accuracy in smaller static objects."));
			submenu->Append(CM_OBJECTS_ADD_RECTANGLE,_("Rectangle"),_("Adds rectangle to scene."));
			submenu->Append(CM_OBJECTS_ADD_BOX,_("Box"),_("Adds box to scene."));
			submenu->Append(CM_OBJECTS_ADD_SPHERE,_("Sphere"),_("Adds sphere to scene."));
			menu.AppendSubMenu(submenu,_("Add..."),_("Adds newly created dynamic object to scene."));
		}
		if (entityIds.size()) // prevents assert when right clicking empty objects
		if (temporaryContext==staticObjects
			|| temporaryContext==dynamicObjects
			|| (temporaryContext.IsOk() && GetItemParent(temporaryContext)==staticObjects)
			|| (temporaryContext.IsOk() && GetItemParent(temporaryContext)==dynamicObjects))
		{
			bool selectedExactlyAllStaticObjects = entityIds.rbegin()->index+1==svframe->m_canvas->solver->getStaticObjects().size();
			bool selectedOnlyStaticObjects = entityIds.rbegin()->index<svframe->m_canvas->solver->getStaticObjects().size();
			bool selectedOnlyDynamicObjects = entityIds.rbegin()->index>=svframe->m_canvas->solver->getStaticObjects().size();
			if (selectedExactlyAllStaticObjects || svframe->userPreferences.testingBeta) // is safe only for all objects at once because lightmapTexcoord is in material, not in RRObject, we can't change only selected objects without duplicating materials
				menu.Append(CM_OBJECTS_UNWRAP,_("Build unwrap..."),_("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
			if (selectedOnlyStaticObjects)
			{
				menu.Append(CM_OBJECTS_BUILD_LMAPS,_("Bake lightmaps..."),_("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel requires unwrap."));
				menu.Append(CM_OBJECTS_BUILD_AMBIENT_MAPS,_("Bake ambient maps..."),_("(Re)builds per-vertex or per-pixel ambient maps. Per-pixel requires unwrap."));
				menu.Append(CM_OBJECTS_BUILD_LDMS,_("Build LDMs..."),_("(Re)builds LDMs, layer of additional per-pixel details. LDMs require unwrap."));
			}
			if (temporaryContext!=staticObjects && temporaryContextItems.size()==1)
				menu.Append(CM_OBJECT_INSPECT_UNWRAP,_("Inspect unwrap,lightmap,LDM..."),_("Shows unwrap and lightmap or LDM in 2D."));
			if (entityIds.size()>1)
				menu.Append(CM_OBJECTS_MERGE,_("Merge objects"),_("Merges objects together."));
			menu.Append(CM_OBJECTS_MERGE_BY_MATERIALS,_("Merge/split by material"),_("Merge selected objects and then split them by material, so that for each material one object is created."));
			menu.Append(CM_OBJECTS_SMOOTH,_("Smooth..."),_("Rebuild objects to have smooth normals."));
			//if (svframe->userPreferences.testingBeta)
				menu.Append(CM_OBJECTS_TANGENTS,_("Build tangents"),_("Rebuild objects to have tangents and bitangents."));
			if (temporaryContext!=staticObjects && !IS_SHOWN(svframe->m_objectProperties) && temporaryContextItems.size()==1)
				menu.Append(SVFrame::ME_WINDOW_OBJECT_PROPERTIES, _("Properties..."));
		}
	}

	// add menu items that work with non uniform context
	if (entityIds.size())
	{
		if (numObjects && numObjects>numObjectsStatic && !numLights && !numOthers)
			menu.Append(CM_OBJECTS_STATIC, _("Make static"));
		if (numObjectsStatic && !numLights && !numOthers)
			menu.Append(CM_OBJECTS_DYNAMIC, _("Make dynamic"));
		if (numObjects+numLights && !numOthers)
			menu.Append(CM_EXPORT, _("Export..."));
		if (numObjects && !numLights && !numOthers)
			menu.Append(CM_OBJECTS_DELETE_DIALOG,_("Purge..."),_("Deletes components within objects."));
		menu.Append(CM_DELETE, _("Delete")+" (del)");
	}

	PopupMenu(&menu, event.GetPoint());
}

void SVSceneTree::OnContextMenuRun(wxCommandEvent& event)
{
	runContextMenuAction(event.GetId(),getEntityIds(SVSceneTree::MEI_SELECTED));
}

rr::RRObject* SVSceneTree::addMesh(rr::RRMesh* mesh, wxString name, bool inFrontOfCamera)
{
	// this leaks memory, but it is not called often = not serious
	bool aborting = false;
	rr::RRCollider* collider = rr::RRCollider::create(mesh,NULL,rr::RRCollider::IT_LINEAR,aborting);
	rr::RRObject* object = new rr::RRObject;
	object->setCollider(collider);
	rr::RRMaterial* material = new rr::RRMaterial;
	material->reset(false);
	rr::RRObject::FaceGroup fg;
	fg.numTriangles = mesh->getNumTriangles();
	fg.material = material;
	object->faceGroups.push_back(fg);
	if (inFrontOfCamera)
	{
		rr::RRMatrix3x4 m = rr::RRMatrix3x4::translation(svs.eye.getPosition()+svs.eye.getDirection()*3-svs.eye.getUp()*0.5f);
		object->setWorldMatrix(&m);
	}
	object->isDynamic = true;
	object->name = RR_WX2RR(name);
	object->updateIlluminationEnvMapCenter();
	rr::RRScene scene;
	scene.objects.push_back(object);
	svframe->m_canvas->addOrRemoveScene(&scene,true,false); // calls svframe->updateAllPanels();
	return object;
}

extern bool getQuality(wxString title, wxWindow* parent, unsigned& quality);
extern bool getResolution(wxString title, wxWindow* parent, unsigned& resolution, bool offerPerVertex);
extern bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption);

static bool containsStaticObject(rr::RRObjects& objects)
{
	for (unsigned i=0;i<objects.size();i++)
		if (!objects[i]->isDynamic)
			return true;
	return false;
}

void SVSceneTree::runContextMenuAction(unsigned actionCode, const EntityIds contextEntityIds)
{
	callDepth++;
	svframe->commitPropertyChanges();
	callDepth--;

	RRDynamicSolverGL* solver = svframe->m_canvas->solver;

	// what objects to process, code shared by many actions
	rr::RRObjects allObjects = solver->getObjects();
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

	// what lights to process, code shared by many actions
	rr::RRLights selectedLights;
	for (unsigned lightIndex=0;lightIndex<solver->getLights().size();lightIndex++)
		if (contextEntityIds.find(EntityId(ST_LIGHT,lightIndex))!=contextEntityIds.end())
				selectedLights.push_back(solver->getLights()[lightIndex]);

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
					svframe->m_canvas->addOrRemoveScene(NULL,true,true);
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
				svframe->m_canvas->addOrRemoveScene(NULL,true,true);
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
					case CM_LIGHT_SPOT: newLight = rr::RRLight::createSpotLight(svs.eye.getPosition(),rr::RRVec3(1),svs.eye.getDirection(),svs.eye.getFieldOfViewVerticalRad()/2,svs.eye.getFieldOfViewVerticalRad()/4); break;
					case CM_LIGHT_POINT: newLight = rr::RRLight::createPointLight(svs.eye.getPosition(),rr::RRVec3(1)); break;
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
				static unsigned res = 256;
				if (getResolution(_("Unwrap build"),this,res,false))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building unwrap..."));


					// 1) merge identical vertices so that two unwraps in a row work with the same data (possibly ordered differently). this must be done after deleting old unwrap
					// 2) remove degens, unwrapper crashes on them
					selectedObjectsAndInstances.deleteComponents(false,true,true,false); // remove old unwrap etc
					selectedObjectsAndInstances.smoothAndStitch(false,true,true,true,true,false,0,0,0,false); // then merge identical vertices
					selectedObjectsAndInstances.buildUnwrap(res,0,solver->aborting);

					// static objects may be modified even after abort (unwrap is not atomic)
					// so it's better if following setStaticObjects is not aborted
					solver->aborting = false;

					svframe->m_canvas->addOrRemoveScene(NULL,true,containsStaticObject(selectedObjectsAndInstances)); // calls svframe->updateAllPanels();
				}
			}
			break;


		case CM_OBJECTS_BUILD_LMAPS:
		case CM_OBJECTS_BUILD_AMBIENT_MAPS:
			if (solver)
			{
				bool ambient = actionCode==CM_OBJECTS_BUILD_AMBIENT_MAPS;
				static unsigned resLmap = 256;
				static unsigned resAmb = 0; // 0=vertex buffers
				unsigned& res = ambient?resAmb:resLmap;
				static unsigned quality = 100;
				if (getResolution(ambient?_("Ambient map build"):_("Lightmap build"),this,res,true) && getQuality(ambient?_("Ambient map build"):_("Lightmap build"),this,quality))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,ambient?_("Building ambient maps..."):_("Building lightmaps..."));


					// allocate buffers in temp layer
					unsigned tmpLayer = 74529457;
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->getCollider()->getMesh()->getNumVertices())
							selectedObjects[i]->illumination.getLayer(tmpLayer) = res
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBF,true,NULL) // A is only for debugging, F prevents clamping to 1 in very bright regions
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,selectedObjects[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);

					// update everything in temp layer
					solver->leaveFireball();
					svframe->m_canvas->fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters updateParameters(quality);
					updateParameters.aoIntensity = svs.lightmapDirectParameters.aoIntensity;
					updateParameters.aoSize = svs.lightmapDirectParameters.aoSize;
#ifdef OLD_SIMPLE_GI
					solver->updateLightmaps(tmpLayer,-1,-1,&updateParameters,&updateParameters,&svs.lightmapFilteringParameters);
#else
					float directLightMultiplier = ambient ? 0 : 1;
					float indirectLightMultiplier = svs.renderLightIndirectMultiplier; // affects baked solution only

					// apply indirect light multiplier
					std::vector<rr::RRVec3> lightColors;
					for (unsigned i=0;i<solver->getLights().size();i++)
					{
						lightColors.push_back(solver->getLights()[i]->color); // save original colors, just in case indirectLightMultiplier is 0
						solver->getLights()[i]->color *= indirectLightMultiplier;
					}

					// calculate indirect illumination in solver
					solver->updateLightmaps(-1,-1,-1,NULL,&updateParameters,NULL);

					// apply direct light multiplier
					for (unsigned i=0;i<solver->getLights().size();i++)
						solver->getLights()[i]->color = lightColors[i]*directLightMultiplier;

					// build direct illumination
					updateParameters.applyCurrentSolution = true;
					solver->updateLightmaps(tmpLayer,-1,-1,&updateParameters,NULL,&svs.lightmapFilteringParameters);

					// restore light intensities
					for (unsigned i=0;i<solver->getLights().size();i++)
						solver->getLights()[i]->color = lightColors[i];
#endif
					// save temp layer to .exr
					bool hdr = svs.lightmapFloats;
					svs.lightmapFloats = true;
					selectedObjects.saveLayer(tmpLayer,LAYER_PREFIX,ambient?AMBIENT_POSTFIX:LMAP_POSTFIX);
					// save temp layer to .png
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->illumination.getLayer(tmpLayer))
							selectedObjects[i]->illumination.getLayer(tmpLayer)->setFormat(rr::BF_RGB);
					svs.lightmapFloats = false;
					selectedObjects.saveLayer(tmpLayer,LAYER_PREFIX,ambient?AMBIENT_POSTFIX:LMAP_POSTFIX);
					svs.lightmapFloats = hdr;

					// delete temp layer
					selectedObjects.layerDeleteFromMemory(tmpLayer);

					// load final layer from disk
					// (this can be optimized away if we create copy of HDR)
					selectedObjects.loadLayer(ambient?svs.layerBakedAmbient:svs.layerBakedLightmap,LAYER_PREFIX,ambient?AMBIENT_POSTFIX:LMAP_POSTFIX);

					// make results visible
					svs.renderLightDirect = ambient?LD_REALTIME:LD_BAKED;
					svs.renderLightIndirect = LI_BAKED;

					// bake also cubemaps
					goto bake_cubemaps;
				}
			}
			break;

		case CM_OBJECTS_BUILD_CUBES:
			bake_cubemaps:
			if (solver)
			{
				// when baking all static objects, bake also dynamic objects
				// (alternative approach would be to expose baking in context menu also for dynamic objects, user would have to explicitly bake also dynamic)
				if (selectedObjectRoot || selectedObjects.size()==solver->getStaticObjects().size())
					selectedObjects = allObjects;

				// allocate cubes
				selectedObjects.allocateBuffersForRealtimeGI(-1,svs.layerBakedEnvironment,4,2*svs.raytracedCubesRes,2*svs.raytracedCubesRes,true,true,svs.raytracedCubesSpecularThreshold,svs.raytracedCubesDepthThreshold);

				// delete cubes from disk
				// if we always keep all files, it won't be possible to get rid of once baked cube
				//  (e.g. when cube treshold is increased to have fewer cubes and more mirrors, we will continue using old cube baked before change)
				// if we always delete older files, it won't be possible to bake objects incrementally, one by one
				// so let's delete older files only when user requests baking them, and they have no buffer
				selectedObjects.layerDeleteFromDisk(LAYER_PREFIX,ENV_POSTFIX);

				// update cubes
				for (unsigned i=0;i<selectedObjects.size();i++)
					solver->updateEnvironmentMap(&selectedObjects[i]->illumination,svs.layerBakedEnvironment);

				// save cubes
				selectedObjects.saveLayer(svs.layerBakedEnvironment,LAYER_PREFIX,ENV_POSTFIX);
			}
			break;

		case CM_OBJECTS_BUILD_LDMS:
			if (solver)
			{
				static unsigned res = 256;
				static unsigned quality = 100;
				if (getResolution("LDM build",this,res,false) && getQuality("LDM build",this,quality))
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
					solver->setEnvironment(oldEnv,NULL,svs.skyboxRotationRad);
					delete newEnv;

					// save temp layer
					allObjects.saveLayer(tmpLayer,LAYER_PREFIX,LDM_POSTFIX);

					// move buffers from temp to final layer
					for (unsigned i=0;i<selectedObjects.size();i++)
						if (selectedObjects[i]->getCollider()->getMesh()->getNumVertices())
						{
							delete selectedObjects[i]->illumination.getLayer(svs.layerBakedLDM);
							selectedObjects[i]->illumination.getLayer(svs.layerBakedLDM) = selectedObjects[i]->illumination.getLayer(tmpLayer);
							selectedObjects[i]->illumination.getLayer(tmpLayer) = NULL;
						}

					// make results visible
					if (svs.renderLightDirect==LD_BAKED)
						svs.renderLightDirect = LD_REALTIME;
					if (svs.renderLightIndirect==LI_NONE || svs.renderLightIndirect==LI_BAKED)
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
					float stitchDistance = svframe->smoothDlg.stitchDistance->GetValue().ToDouble(&d) ? (float)d : 0;
					float smoothAngle = svframe->smoothDlg.smoothAngle->GetValue().ToDouble(&d) ? (float)d : 30;
					float uvDistance = svframe->smoothDlg.uvDistance->GetValue().ToDouble(&d) ? (float)d : 0;
					if (selectedObjectsAndInstances.size())
					{
						selectedObjectsAndInstances.smoothAndStitch(
							svframe->smoothDlg.splitVertices->GetValue(),
							svframe->smoothDlg.mergeVertices->GetValue(),
							true,
							svframe->smoothDlg.stitchPositions->GetValue(),
							svframe->smoothDlg.stitchNormals->GetValue(),
							svframe->smoothDlg.generateNormals->GetValue(),
							stitchDistance,
							RR_DEG2RAD(smoothAngle),
							uvDistance,
							true);

						svframe->m_canvas->addOrRemoveScene(NULL,true,containsStaticObject(selectedObjectsAndInstances)); // calls svframe->updateAllPanels();
					}
				}
			}
			break;
		case CM_OBJECTS_MERGE:
		case CM_OBJECTS_MERGE_BY_MATERIALS:
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

				rr::RRReporter::report(rr::INF2,"Merging...\n");
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
				newObject->isDynamic = !containsStaticObject(selectedObjects);
				delete oldObject;

				rr::RRObjects newList;
				newList.push_back(newObject); // memleak, newObject is never freed
				// ensure that there is one facegroup per material
				rr::RRReporter::report(rr::INF2,"Optimizing...\n");
				newList.optimizeFaceGroups(newObject); // we know there are no other instances of newObject's mesh

				if (actionCode==CM_OBJECTS_MERGE_BY_MATERIALS)
				{
					rr::RRReporter::report(rr::INF2,"Splitting...\n");
					// split by facegroups=materials
					newList.clear();
					unsigned firstTriangleIndex = 0;
					for (unsigned f=0;f<newObject->faceGroups.size();f++)
					{
						// create splitObject from newObject->faceGroups[f]
						rr::RRMeshArrays* splitMesh = NULL;
						{
							// temporarily hide triangles with other materials
							unsigned tmpNumTriangles = newMesh->numTriangles;
							newMesh->numTriangles = newObject->faceGroups[f].numTriangles;
							newMesh->triangle += firstTriangleIndex;
							// filter out unused vertices
							const rr::RRMesh* optimizedMesh = newMesh->createOptimizedVertices(0,0,0,&texcoords);
							// read result into new mesh
							splitMesh = optimizedMesh->createArrays(true,texcoords,tangents);
							// delete temporaries
							delete optimizedMesh;
							// unhide triangles
							newMesh->triangle -= firstTriangleIndex;
							newMesh->numTriangles = tmpNumTriangles;
						}
						rr::RRCollider* splitCollider = rr::RRCollider::create(splitMesh,NULL,rr::RRCollider::IT_LINEAR,solver->aborting);
						rr::RRObject* splitObject = new rr::RRObject;
						splitObject->faceGroups.push_back(newObject->faceGroups[f]);
						splitObject->setCollider(splitCollider);
						splitObject->isDynamic = newObject->isDynamic;
						splitObject->name = splitObject->faceGroups[0].material->name; // name object by material
						newList.push_back(splitObject); // memleak, splitObject is never freed
						firstTriangleIndex += newObject->faceGroups[f].numTriangles;
					}
				}

				rr::RRReporter::report(rr::INF2,"Updating objects in solver...\n");
				for (unsigned objectIndex=0;objectIndex<allObjects.size();objectIndex++)
					if (contextEntityIds.find(EntityId(ST_OBJECT,objectIndex))==contextEntityIds.end())
						newList.push_back(allObjects[objectIndex]);
				solver->setStaticObjects(newList,NULL);
				solver->setDynamicObjects(newList);

				svframe->m_canvas->addOrRemoveScene(NULL,false,containsStaticObject(selectedObjects)); // calls svframe->updateAllPanels();
			}
			break;
		case CM_OBJECTS_TANGENTS:
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Building tangents..."));

				for (unsigned i=0;i<selectedObjects.size();i++)
				{
					unsigned uvChannel = 0;
					const rr::RRObject::FaceGroups& fgs = selectedObjects[i]->faceGroups;
					for (unsigned fg=0;fg<fgs.size();fg++)
						if (fgs[fg].material)
						{
							uvChannel = fgs[fg].material->bumpMap.texcoord;
							if (fgs[fg].material->bumpMap.texture)
								break;
						}
					rr::RRMeshArrays* arrays = dynamic_cast<rr::RRMeshArrays*>(const_cast<rr::RRMesh*>(selectedObjects[i]->getCollider()->getMesh()));
					if (arrays)
						arrays->buildTangents(uvChannel);
				}

				svframe->m_canvas->addOrRemoveScene(NULL,true,containsStaticObject(selectedObjects)); // calls svframe->updateAllPanels();
			}
			break;

		case CM_OBJECTS_STATIC:
		case CM_OBJECTS_DYNAMIC:
			{
				for (unsigned i=0;i<selectedObjects.size();i++)
					selectedObjects[i]->isDynamic = actionCode==CM_OBJECTS_DYNAMIC;
				svframe->m_canvas->addOrRemoveScene(NULL,true,true);
			}
			break;

		case CM_OBJECTS_ADD_PLANE:
			{
				enum {H=6,TRIANGLES=1+6*H,VERTICES=3+3*H};
				rr::RRMeshArrays* arrays = new rr::RRMeshArrays;
				rr::RRVector<unsigned> texcoords;
				texcoords.push_back(0);
				arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
				arrays->triangle[0][0] = 0;
				arrays->triangle[0][1] = 1;
				arrays->triangle[0][2] = 2;
				for (unsigned i=0;i<TRIANGLES-1;i+=2)
				{
					arrays->triangle[i+1][0] = i/6*3+((2+((i/2)%3))%3);
					arrays->triangle[i+1][1] = i/6*3+((1+((i/2)%3))%3);
					arrays->triangle[i+1][2] = i/6*3+((0+((i/2)%3))%3)+3;
					arrays->triangle[i+2][0] = i/6*3+((2+((i/2)%3))%3);
					arrays->triangle[i+2][1] = i/6*3+((0+((i/2)%3))%3)+3;
					arrays->triangle[i+2][2] = i/6*3+((1+((i/2)%3))%3)+3;
				}
				float v[] = {1.732f,-1, -1.732f,-1, 0,2};
				for (unsigned i=0;i<VERTICES;i++)
				{
					arrays->position[i] = rr::RRVec3(v[(i%3)*2],0,v[(i%3)*2+1])*powf(4,i/3)*((i/3)%2?1:-1);
					arrays->normal[i] = rr::RRVec3(0,1,0);
					arrays->texcoord[0][i] = rr::RRVec2(arrays->position[i].z,arrays->position[i].x);
				}
				addMesh(arrays,_("plane"));
			}
			break;
		case CM_OBJECTS_ADD_RECTANGLE:
			{
				enum {W=10,H=10,TRIANGLES=W*H*2,VERTICES=(W+1)*(H+1)};
				rr::RRMeshArrays* arrays = new rr::RRMeshArrays;
				rr::RRVector<unsigned> texcoords;
				texcoords.push_back(0);
				arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
				for (unsigned j=0;j<H;j++)
				for (unsigned i=0;i<W;i++)
				{
					arrays->triangle[2*(i+W*j)  ][0] = i  + j   *(W+1);
					arrays->triangle[2*(i+W*j)  ][1] = i  +(j+1)*(W+1);
					arrays->triangle[2*(i+W*j)  ][2] = i+1+(j+1)*(W+1);
					arrays->triangle[2*(i+W*j)+1][0] = i  + j   *(W+1);
					arrays->triangle[2*(i+W*j)+1][1] = i+1+(j+1)*(W+1);
					arrays->triangle[2*(i+W*j)+1][2] = i+1+ j   *(W+1);
				}
				for (unsigned i=0;i<VERTICES;i++)
				{
					arrays->position[i] = rr::RRVec3((i%(W+1))/(float)W-0.5f,0,i/(W+1)/(float)H-0.5f);
					arrays->normal[i] = rr::RRVec3(0,1,0);
					arrays->texcoord[0][i] = rr::RRVec2(i/(W+1)/(float)H,(i%(W+1))/(float)W);
				}
				addMesh(arrays,_("rectangle"));
			}
			break;
		case CM_OBJECTS_ADD_BOX:
			{
				enum {TRIANGLES=12,VERTICES=24};
				const unsigned char triangles[3*TRIANGLES] = {0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20};
				const unsigned char positions[3*VERTICES] = {
					0,1,0, 0,1,1, 1,1,1, 1,1,0,
					0,1,1, 0,0,1, 1,0,1, 1,1,1,
					1,1,1, 1,0,1, 1,0,0, 1,1,0,
					1,1,0, 1,0,0, 0,0,0, 0,1,0,
					0,1,0, 0,0,0, 0,0,1, 0,1,1,
					1,0,0, 1,0,1, 0,0,1, 0,0,0};
				const unsigned char uvs[2*VERTICES] = {0,1, 0,0, 1,0, 1,1};
				rr::RRMeshArrays* arrays = new rr::RRMeshArrays;
				rr::RRVector<unsigned> texcoords;
				texcoords.push_back(0);
				arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
				for (unsigned i=0;i<TRIANGLES;i++)
					for (unsigned j=0;j<3;j++)
						// commented out code can replace constant arrays, but 2 cube sides have mapping rotated
						//arrays->triangle[i][j] = i*2+((i%2)?1-(((i/2)%2)?2-j:j):(((i/2)%2)?2-j:j));
						arrays->triangle[i][j] = triangles[3*i+j];
				for (unsigned i=0;i<VERTICES;i++)
				{
					for (unsigned j=0;j<3;j++)
						//arrays->position[i][j] = ((j==((i/8)%3)) ? ((i/4)%2) : ((j==((1+i/8)%3)) ? ((i/2)%2) : (i%2))) - 0.5f;
						arrays->position[i][j] = positions[3*i+j]-0.5f;
					//arrays->texcoord[0][i] = ((i/4)%2) ? rr::RRVec2((i/2)%2,i%2) : rr::RRVec2(1-(i/2)%2,(i%2));
					for (unsigned j=0;j<2;j++)
						arrays->texcoord[0][i][j] = uvs[2*(i%4)+j];
				}
				arrays->buildNormals();
				addMesh(arrays,_("box"));
			}
			break;
		case CM_OBJECTS_ADD_SPHERE:
			{
				enum {W=30,H=15,TRIANGLES=W*H*2,VERTICES=(W+1)*(H+1)};
				rr::RRMeshArrays* arrays = new rr::RRMeshArrays;
				rr::RRVector<unsigned> texcoords;
				texcoords.push_back(0);
				arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
				for (unsigned j=0;j<H;j++)
				for (unsigned i=0;i<W;i++)
				{
					arrays->triangle[2*(i+W*j)  ][0] = i  + j   *(W+1);
					arrays->triangle[2*(i+W*j)  ][1] = i+1+(j+1)*(W+1);
					arrays->triangle[2*(i+W*j)  ][2] = i  +(j+1)*(W+1);
					arrays->triangle[2*(i+W*j)+1][0] = i  + j   *(W+1);
					arrays->triangle[2*(i+W*j)+1][1] = i+1+ j   *(W+1);
					arrays->triangle[2*(i+W*j)+1][2] = i+1+(j+1)*(W+1);
				}
				for (unsigned i=0;i<VERTICES;i++)
				{
					arrays->normal[i] = rr::RRVec3(cos(2*RR_PI*(i%(W+1))/W)*sin(i/(W+1)*RR_PI/H),cos(i/(W+1)*RR_PI/H),sin(2*RR_PI*(i%(W+1))/W)*sin(i/(W+1)*RR_PI/H));
					if (i/(W+1)==H)
					{
						// sin(0)==0 so north pole is centered; sin(RR_PI)!=0 so south pole is slightly off; also center from getAABB() is slightly off zero
						// this centers south pole
						// with uncentered south pole, RL's spherical mapping would look bad around pole (all south pole vertices would stitched)
						arrays->normal[i] = rr::RRVec3(0,-1,0);
					}
					arrays->position[i] = arrays->normal[i]/2;
					arrays->texcoord[0][i] = rr::RRVec2(1-(i%(W+1))/(float)W,1-i/(W+1)/(float)H);
				}
				addMesh(arrays,_("sphere"));
			}
			break;

		case CM_OBJECTS_DELETE_DIALOG:
			if (solver)
			if (svframe->deleteDlg.ShowModal()==wxID_OK)
			{
				selectedObjectsAndInstances.deleteComponents(svframe->deleteDlg.tangents->GetValue(),svframe->deleteDlg.unwrap->GetValue(),svframe->deleteDlg.unusedUvChannels->GetValue(),svframe->deleteDlg.emptyFacegroups->GetValue());

				svframe->m_canvas->addOrRemoveScene(NULL,true,containsStaticObject(selectedObjectsAndInstances)); // calls svframe->updateAllPanels(); // necessary at least after deleting empty facegroups
			}
			break;

		case CM_EXPORT:
			if (solver)
			{
				wxString selectedFilename = svs.sceneFilename;
				if (svframe->chooseSceneFilename(_("Export as"),selectedFilename))
				{
					rr::RRScene scene;
					scene.objects = selectedObjects;
					scene.lights = selectedLights;
					scene.cameras.push_back(svs.eye);
					scene.environment = solver->getEnvironment();
					scene.save(RR_WX2RR(selectedFilename));
					scene.environment = NULL; // would be deleted in destructor otherwise
				}
			}
			break;
		case CM_DELETE:
			if (solver)
			{
				// display log window with 'abort' while this function runs
				bool containsObjects = false;
				for (EntityIds::const_iterator i=contextEntityIds.begin();i!=contextEntityIds.end();++i)
					containsObjects |= i->type==ST_OBJECT;
				bool skipLog = !LogWithAbort::logIsOn && !containsObjects;
				if (skipLog) LogWithAbort::logIsOn = true;
				LogWithAbort logWithAbort(this,solver,_("Deleting..."));
				if (skipLog) LogWithAbort::logIsOn = false;

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

						svframe->m_canvas->addOrRemoveScene(NULL,false,containsStaticObject(selectedObjects)); // calls svframe->updateAllPanels();
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
