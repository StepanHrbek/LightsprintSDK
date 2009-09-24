// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVSceneTree.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVFrame.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// ItemData

class ItemData : public wxTreeItemData
{
public:
	ItemData(EntityId _id) : id(_id)
	{
	}
	EntityId id;
};

/////////////////////////////////////////////////////////////////////////////
//
// SVSceneTree

SVSceneTree::SVSceneTree(wxWindow* _parent, SceneViewerStateEx& _svse)
	: wxTreeCtrl( _parent, wxID_ANY, wxDefaultPosition, wxSize(250,400) ), svs(_svse)
{
	allowEvents = true;
	tc = this;

	wxTreeItemId root = tc->AddRoot("root");
	lights = tc->AppendItem(root,"lights");
	objects = tc->AppendItem(root,"objects");

	tc->Expand(root);
	tc->Expand(lights); // wxmsw ignores this because lights is empty
}

void SVSceneTree::updateContent(RRDynamicSolverGL* solver)
{
	// DeleteChildren generates EVT_TREE_SEL_CHANGED, breaking our stuff
	// (despite docs saying it does not generate events)
	// let's ignore all events temporarily
	allowEvents = false;

	#define SHORTER_STRING(str,maxlength) (str.size()>maxlength?std::string("...")+(str.c_str()+str.size()-maxlength):str)
	tc->SetItemText(tc->GetRootItem(),
		svs.sceneFilename.size()?SHORTER_STRING(svs.sceneFilename,40):"scene");

	tc->DeleteChildren(lights);
	for (unsigned i=0;i<solver->getLights().size();i++)
	{
		tc->AppendItem(lights,wxString("light ")<<i,-1,-1,new ItemData(EntityId(ST_LIGHT,i)));
	}
	tc->DeleteChildren(objects);
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
	{
		const char* objectName = (const char*)solver->getObject(i)->getCustomData("const char* objectName");
		tc->AppendItem(objects,objectName ? objectName : wxString("object ")<<i,-1,-1,new ItemData(EntityId(ST_OBJECT,i)));
	}

	allowEvents = true;
}

wxTreeItemId SVSceneTree::findItem(EntityId entity, bool& isOk) const
{
	wxTreeItemId item; // invalid
	if (entity.type==ST_LIGHT)
	{
		wxTreeItemIdValue cookie;
		wxTreeItemId item = tc->GetFirstChild(lights,cookie);
		for (unsigned i=0;i<entity.index;i++)
		{
			item = tc->GetNextChild(lights,cookie);
		}
		isOk = item.IsOk();
		return item;
	}
	if (entity.type==ST_OBJECT)
	{
		wxTreeItemIdValue cookie;
		wxTreeItemId item = tc->GetFirstChild(objects,cookie);
		for (unsigned i=0;i<entity.index;i++)
		{
			item = tc->GetNextChild(objects,cookie);
		}
		isOk = item.IsOk();
		return item;
	}
	isOk = false;
	return wxTreeItemId();
}

void SVSceneTree::selectItem(EntityId entity)
{
	bool isOk;
	wxTreeItemId item = findItem(entity,isOk);
	if (isOk)
	{
		tc->SelectItem(item,true);
		tc->EnsureVisible(item);
	}
}

void SVSceneTree::OnSelChanged(wxTreeEvent& event)
{
	if (allowEvents)
	{
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		ItemData* data = (ItemData*)tc->GetItemData(event.GetItem());

		if (data) // is non-NULL only in leaf nodes of tree
		{
			frame->selectEntity(data->id,false,SEA_SELECT);
		}
	}
}

void SVSceneTree::OnItemActivated(wxTreeEvent& event)
{
	if (allowEvents)
	{
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		ItemData* data = (ItemData*)tc->GetItemData(event.GetItem());

		if (data) // is non-NULL only in leaf nodes of tree
		{
			frame->selectEntity(data->id,false,SEA_ACTION);
		}
	}
}

void SVSceneTree::OnKeyDown(wxTreeEvent& event)
{
	switch (event.GetKeyCode())
	{
		case WXK_DELETE:
			// our parent must be frame
			GetParent()->ProcessWindowEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHT_DELETE));
			break;
	}
}

BEGIN_EVENT_TABLE(SVSceneTree, wxTreeCtrl)
	EVT_TREE_SEL_CHANGED(-1,SVSceneTree::OnSelChanged)
	EVT_TREE_ITEM_ACTIVATED(-1,SVSceneTree::OnItemActivated)
	EVT_TREE_KEY_DOWN(-1,SVSceneTree::OnKeyDown)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
