// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVSceneTree.h"

#ifdef SUPPORT_SCENEVIEWER

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
	ItemData(EntityId _id) : id(_id)
	{
	}
	EntityId id;
};

/////////////////////////////////////////////////////////////////////////////
//
// SVSceneTree

SVSceneTree::SVSceneTree(wxWindow* _parent, SceneViewerStateEx& _svse)
	: wxTreeCtrl( _parent, wxID_ANY, wxDefaultPosition, wxSize(250,400), wxTR_HAS_BUTTONS|wxBORDER_SIMPLE ), svs(_svse)
{
	allowEvents = true;

	wxTreeItemId root = AddRoot("root");

	lights = AppendItem(root,"lights");

	objects = AppendItem(root,"objects");


	Expand(lights); // wxmsw ignores this because lights is empty
	Expand(root); // must go after appends, otherwise it does not expand
}

void SVSceneTree::updateContent(RRDynamicSolverGL* solver)
{
	// DeleteChildren generates EVT_TREE_SEL_CHANGED, breaking our stuff
	// (despite docs saying it does not generate events)
	// let's ignore all events temporarily
	allowEvents = false;

	#define USE_IF_NONEMPTY_ELSE(str,maxlength) str.size() ? (str.size()>maxlength?std::string("...")+(str.c_str()+str.size()-maxlength):str) :
	SetItemText(GetRootItem(),
		USE_IF_NONEMPTY_ELSE(svs.sceneFilename,40)
		"scene");

	SetItemText(lights,tmpstr("%d lights",solver?solver->getLights().size():0));
	DeleteChildren(lights);
	for (unsigned i=0;solver && i<solver->getLights().size();i++)
	{
		wxString name = solver->getLights()[i]->name.c_str();
		if (name.empty()) name = wxString("light ")<<i;
		AppendItem(lights,name,-1,-1,new ItemData(EntityId(ST_LIGHT,i)));
	}

	SetItemText(objects,tmpstr("%d objects",solver?solver->getStaticObjects().size():0));
	DeleteChildren(objects);
	for (unsigned i=0;solver && i<solver->getStaticObjects().size();i++)
	{
		wxString name = solver->getStaticObjects()[i]->name.c_str();
		if (name.empty()) name = wxString("object ")<<i;
		AppendItem(objects,name,-1,-1,new ItemData(EntityId(ST_OBJECT,i)));
	}


	allowEvents = true;
}

wxTreeItemId SVSceneTree::findItem(EntityId entity, bool& isOk) const
{
	wxTreeItemId searchRoot;
	switch (entity.type)
	{
		case ST_LIGHT: searchRoot = lights; break;
		case ST_OBJECT: searchRoot = objects; break;
		case ST_CAMERA: isOk = false; return wxTreeItemId();
	}
	wxTreeItemIdValue cookie;
	wxTreeItemId item = GetFirstChild(searchRoot,cookie);
	for (unsigned i=0;i<entity.index;i++)
	{
		item = GetNextChild(searchRoot,cookie);
	}
	isOk = item.IsOk();
	return item;
}

void SVSceneTree::selectItem(EntityId entity)
{
	bool isOk;
	wxTreeItemId item = findItem(entity,isOk);
	if (isOk)
	{
		SelectItem(item,true);
		EnsureVisible(item);
	}
}

void SVSceneTree::OnSelChanged(wxTreeEvent& event)
{
	if (allowEvents)
	{
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		ItemData* data = (ItemData*)GetItemData(event.GetItem());

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
		ItemData* data = (ItemData*)GetItemData(event.GetItem());

		if (data) // is non-NULL only in leaf nodes of tree
		{
			frame->selectEntity(data->id,false,SEA_ACTION);
		}
	}
}

void SVSceneTree::OnKeyDown(wxTreeEvent& event)
{
	// our parent must be frame
	SVFrame* frame = (SVFrame*)GetParent();
	wxKeyEvent keyEvent = event.GetKeyEvent();
	frame->m_canvas->OnKeyDown(keyEvent);
}

BEGIN_EVENT_TABLE(SVSceneTree, wxTreeCtrl)
	EVT_TREE_SEL_CHANGED(-1,SVSceneTree::OnSelChanged)
	EVT_TREE_ITEM_ACTIVATED(-1,SVSceneTree::OnItemActivated)
	EVT_TREE_KEY_DOWN(-1,SVSceneTree::OnKeyDown)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
