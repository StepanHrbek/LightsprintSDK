// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneTree.h"
#include "SVFrame.h"
#include "../tmpstr.h"

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
	: wxTreeCtrl(_svframe, wxID_ANY, wxDefaultPosition, wxSize(300,300), wxTR_HAS_BUTTONS|SV_SUBWINDOW_BORDER), svs(_svframe->svs)
{
	svframe = _svframe;
	callDepth = 0;
	needsUpdateContent = false;

	wxTreeItemId root = AddRoot("root");


	lights = AppendItem(root,"0 lights");

	staticObjects = AppendItem(root,"0 static objects");
	dynamicObjects = AppendItem(root,"0 dynamic objects");


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
		"scene");

	// update lights
	if (solver)
	{
		SetItemText(lights,tmpstr("%d lights",solver?solver->getLights().size():0));
		DeleteChildren(lights);
		for (unsigned i=0;solver && i<solver->getLights().size();i++)
		{
			wxString name = solver->getLights()[i]->name.c_str();
			if (name.empty()) name = wxString("light ")<<i;
			AppendItem(lights,name,-1,-1,new ItemData(EntityId(ST_LIGHT,i)));
		}
	}

	// update first 1000 static objects, more would be slow and difficult to control
	if (solver)
	{
		SetItemText(staticObjects,tmpstr("%d static objects",solver?solver->getStaticObjects().size():0));
		DeleteChildren(staticObjects);
		unsigned numStaticObjects = RR_MIN(solver->getStaticObjects().size(),1000);
		for (unsigned i=0;solver && i<numStaticObjects;i++)
		{
			wxString name = solver->getStaticObjects()[i]->name.c_str();
			if (name.empty()) name = wxString("object ")<<i;
			AppendItem(staticObjects,name,-1,-1,new ItemData(EntityId(ST_STATIC_OBJECT,i)));
		}
	}

	// update first 1000 dynamic objects
	if (solver)
	{
		SetItemText(dynamicObjects,tmpstr("%d dynamic objects",solver?solver->getDynamicObjects().size():0));
		DeleteChildren(dynamicObjects);
		unsigned numDynamicObjects = RR_MIN(solver->getDynamicObjects().size(),1000);
		for (unsigned i=0;solver && i<numDynamicObjects;i++)
		{
			wxString name = solver->getDynamicObjects()[i]->name.c_str();
			if (name.empty()) name = wxString("object ")<<i;
			AppendItem(dynamicObjects,name,-1,-1,new ItemData(EntityId(ST_DYNAMIC_OBJECT,i)));
		}
	}


	// tree rebuild moved cursor. move it back
	selectEntityInTree(svframe->getSelectedEntity());

	needsUpdateContent = false;
	callDepth--;
}

wxTreeItemId SVSceneTree::entityIdToItemId(EntityId entity) const
{
	wxTreeItemId searchRoot;
	switch (entity.type)
	{
		case ST_LIGHT: searchRoot = lights; break;
		case ST_STATIC_OBJECT: searchRoot = staticObjects; break;
		case ST_DYNAMIC_OBJECT: searchRoot = dynamicObjects; break;
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
	ItemData* data = (ItemData*)GetItemData(item);
	return data ? data->entityId : EntityId();
}

void SVSceneTree::selectEntityInTree(EntityId entity)
{
	wxTreeItemId item = entityIdToItemId(entity);
	if (item.IsOk())
	{
		SelectItem(item,true);
		EnsureVisible(item);
	}
}

void SVSceneTree::OnSelChanged(wxTreeEvent& event)
{
	if (callDepth)
		return;
	callDepth++;

	EntityId entityId = itemIdToEntityId(event.GetItem());
	svframe->selectEntityInTreeAndUpdatePanel(entityId,SEA_SELECT);

	callDepth--;	
	if (needsUpdateContent)
	{
		updateContent(NULL);
	}
}

void SVSceneTree::OnItemActivated(wxTreeEvent& event)
{
	if (callDepth)
		return;
	callDepth++;

	EntityId entityId = itemIdToEntityId(event.GetItem());
	svframe->selectEntityInTreeAndUpdatePanel(entityId,SEA_ACTION);

	callDepth--;
}

void SVSceneTree::OnContextMenuCreate(wxTreeEvent& event)
{
	temporaryContext = event.GetItem();
	if (temporaryContext.IsOk())
	{
		if (temporaryContext==lights)
		{
			wxMenu menu;
			menu.Append(CM_LIGHT_SPOT, wxT("Add spot light (alt-s)"));
			menu.Append(CM_LIGHT_POINT, wxT("Add point light (alt-o)"));
			menu.Append(CM_LIGHT_DIR, wxT("Add directional light"));
			menu.Append(CM_LIGHT_FLASH, wxT("Toggle flashlight (alt-f)"));
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (GetItemParent(temporaryContext)==lights)
		{
			wxMenu menu;
			menu.Append(CM_LIGHT_DELETE, wxT("Delete light (del)"));
			PopupMenu(&menu, event.GetPoint());
		}
	}
}

void SVSceneTree::OnContextMenuRun(wxCommandEvent& event)
{
	EntityId contextEntityId = itemIdToEntityId(temporaryContext);
	runContextMenuAction(event.GetId(),contextEntityId);
}


void SVSceneTree::runContextMenuAction(unsigned actionCode, EntityId contextEntityId)
{
	callDepth++;
	svframe->commitPropertyChanges();
	callDepth--;

	switch (actionCode)
	{
		case CM_LIGHT_SPOT:
		case CM_LIGHT_POINT:
		case CM_LIGHT_DIR:
			if (svframe->m_canvas->solver)
			{
				rr::RRLights newList = svframe->m_canvas->solver->getLights();
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
					svframe->m_canvas->solver->setLights(newList); // RealtimeLight in light props is deleted here
					if (actionCode==CM_LIGHT_DIR)
						svframe->simulateSun(); // when inserting sun, move it to simulated direction (it would be better to simulate only when inserting first dirlight, because simulation affects only first dirlight)
				}
			}
			break;
		case CM_LIGHT_FLASH:
			if (svframe->m_canvas->solver)
			{
				bool containsFlashlight[2] = {false,false};
				rr::RRLights lights = svframe->m_canvas->solver->getLights();
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
					svframe->m_canvas->solver->setLights(lights); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
					// updateAllPanels() must follow, it deletes lightprops
				}
			}
			break;
		case CM_LIGHT_DELETE:
			if (svframe->m_canvas->solver && contextEntityId.isOk() && contextEntityId.index<svframe->m_canvas->solver->realtimeLights.size())
			{
				rr::RRLights newList = svframe->m_canvas->solver->getLights();

				if (newList[contextEntityId.index]->rtProjectedTexture)
					newList[contextEntityId.index]->rtProjectedTexture->stop();

				newList.erase(contextEntityId.index);

				svframe->m_canvas->solver->setLights(newList); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
				// updateAllPanels() must follow, it deletes lightprops
			}
			break;

	}
	// validate all svs.selectedXxxIndex (some may be out of range after delete)
	// update all property panels (some may point to deleted item)
	// update scene tree panel
	svframe->updateAllPanels();
}


void SVSceneTree::OnKeyDown(wxTreeEvent& event)
{
	wxKeyEvent keyEvent = event.GetKeyEvent();
	long code = keyEvent.GetKeyCode();
	if (code==WXK_LEFT||code==WXK_RIGHT||code==WXK_DOWN||code==WXK_UP||code==WXK_PAGEUP||code==WXK_PAGEDOWN||code==WXK_HOME||code==WXK_END)
	{
		// only for treectrl, don't forward
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
