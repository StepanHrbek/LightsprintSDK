// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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

	#define USE_IF_NONEMPTY_ELSE(str,maxlength) str.size() ? (str.size()>maxlength?std::string("...")+(str.c_str()+str.size()-maxlength):str) :
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
	EVT_TREE_KEY_DOWN(-1,SVSceneTree::OnKeyDown)
	EVT_KEY_UP(SVSceneTree::OnKeyUp)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
