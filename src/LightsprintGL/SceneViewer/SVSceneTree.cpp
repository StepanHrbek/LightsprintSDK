// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneTree.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
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
	: wxTreeCtrl(_svframe, wxID_ANY, wxDefaultPosition, wxSize(300,300), wxTR_HAS_BUTTONS|SV_SUBWINDOW_BORDER), svs(_svframe->svs)
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

	// update first 100000 static objects, more would be slow and difficult to control
	if (solver)
	{
		SetItemText(staticObjects,wxString::Format(_("%d static objects"),solver?solver->getStaticObjects().size():0));
		DeleteChildren(staticObjects);
		unsigned numStaticObjects = RR_MIN(solver->getStaticObjects().size(),100000);
		for (unsigned i=0;solver && i<numStaticObjects;i++)
		{
			wxString name = RR_RR2WX(solver->getStaticObjects()[i]->name);
			if (name.empty()) name = wxString::Format(_("object %d"),i);
			AppendItem(staticObjects,name,-1,-1,new ItemData(EntityId(ST_STATIC_OBJECT,i)));
		}
	}

	// update first 100000 dynamic objects
	if (solver)
	{
		SetItemText(dynamicObjects,wxString::Format(_("%d dynamic objects"),solver?solver->getDynamicObjects().size():0));
		DeleteChildren(dynamicObjects);
		unsigned numDynamicObjects = RR_MIN(solver->getDynamicObjects().size(),100000);
		for (unsigned i=0;solver && i<numDynamicObjects;i++)
		{
			wxString name = RR_RR2WX(solver->getDynamicObjects()[i]->name);
			if (name.empty()) name = wxString::Format(_("object %d"),i);
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
		if (temporaryContext==root)
		{
			wxMenu menu;
			menu.Append(CM_ROOT_SCALE, _("Normalize units..."),_("Makes scene n-times bigger."));
			menu.Append(CM_ROOT_AXES, _("Normalize up-axis"),_("Rotates scene by 90 degrees."));
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (temporaryContext==lights)
		{
			wxMenu menu;
			menu.Append(CM_LIGHT_DIR, _("Add Sun light"));
			menu.Append(CM_LIGHT_SPOT, _("Add spot light")+" (alt-s)");
			menu.Append(CM_LIGHT_POINT, _("Add point light")+" (alt-o)");
			menu.Append(CM_LIGHT_FLASH, _("Toggle flashlight")+" (alt-f)");
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (GetItemParent(temporaryContext)==lights)
		{
			wxMenu menu;
			menu.Append(CM_LIGHT_DELETE, _("Delete light")+" (del)");
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (temporaryContext==staticObjects)
		{
			wxMenu menu;
			menu.Append(CM_STATIC_OBJECTS_UNWRAP,_("Build unwrap..."),_("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
			menu.Append(CM_STATIC_OBJECTS_BUILD_LMAPS,_("Build lightmaps..."),_("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel requires unwrap."));
			menu.Append(CM_STATIC_OBJECTS_BUILD_LDMS,_("Build LDMs..."),_("(Re)builds LDMs, layer of additional per-pixel details. LDMs require unwrap."));
			menu.Append(CM_STATIC_OBJECTS_SMOOTH,_("Smooth..."),_("Rebuild objects to have smooth normals."));
			menu.Append(CM_STATIC_OBJECTS_MERGE,_("Merge objects"),_("Merges all objects together."));
			menu.Append(CM_STATIC_OBJECTS_TANGENTS,_("Build tangents"),_("Rebuild objects to have tangents and bitangents."));
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (GetItemParent(temporaryContext)==staticObjects)
		{
			wxMenu menu;
			menu.Append(CM_STATIC_OBJECT_UNWRAP,_("Build unwrap..."),_("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
			menu.Append(CM_STATIC_OBJECT_BUILD_LMAP,_("Build lightmap..."),_("(Re)builds per-vertex or per-pixel lightmap. Per-pixel requires unwrap."));
			menu.Append(CM_STATIC_OBJECT_BUILD_LDM,_("Build LDM..."),_("(Re)builds LDM, layer of additional per-pixel details. LDMs require unwrap."));
			menu.Append(CM_STATIC_OBJECT_SMOOTH,_("Smooth..."),_("Rebuild objects to have smooth normals."));
			menu.Append(CM_STATIC_OBJECT_TANGENTS,_("Build tangents"),_("Rebuild objects to have tangents and bitangents."));
			menu.Append(CM_STATIC_OBJECT_DELETE, _("Delete object")+" (del)");
			PopupMenu(&menu, event.GetPoint());
		}
		else
		if (GetItemParent(temporaryContext)==dynamicObjects)
		{
			wxMenu menu;
			menu.Append(CM_DYNAMIC_OBJECT_DELETE, _("Delete object")+" (del)");
			PopupMenu(&menu, event.GetPoint());
		}
	}
}

void SVSceneTree::OnContextMenuRun(wxCommandEvent& event)
{
	EntityId contextEntityId = itemIdToEntityId(temporaryContext);
	runContextMenuAction(event.GetId(),contextEntityId);
}

extern bool getQuality(wxString title, wxWindow* parent, unsigned& quality);
extern bool getResolution(wxString title, wxWindow* parent, unsigned& resolution, bool offerPerVertex);
extern bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption);

void SVSceneTree::runContextMenuAction(unsigned actionCode, EntityId contextEntityId)
{
	callDepth++;
	svframe->commitPropertyChanges();
	callDepth--;

	RRDynamicSolverGL* solver = svframe->m_canvas->solver;

	switch (actionCode)
	{
		case CM_ROOT_SCALE:
			{
				static float currentUnitLengthInMeters = 1;
				if (getFactor(this,currentUnitLengthInMeters,_("Enter current unit length in meters."),_("Normalize units")))
				{
					rr::RRScene scene;
					scene.objects = solver->getStaticObjects();
					scene.lights = solver->getLights();
					scene.normalizeUnits(currentUnitLengthInMeters);
					svframe->m_canvas->addOrRemoveScene(NULL,true);
				}
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything
		case CM_ROOT_AXES:
			{
				static unsigned currentUpAxis = 0;
				rr::RRScene scene;
				scene.objects = solver->getStaticObjects();
				scene.lights = solver->getLights();
				scene.normalizeUpAxis(currentUpAxis);
				svframe->m_canvas->addOrRemoveScene(NULL,true);
				currentUpAxis = 2-currentUpAxis;
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything
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
					// updateAllPanels() must follow, it deletes lightprops
				}
			}
			break;
		case CM_LIGHT_DELETE:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->realtimeLights.size())
			{
				rr::RRLights newList = solver->getLights();

				if (newList[contextEntityId.index]->rtProjectedTexture)
					newList[contextEntityId.index]->rtProjectedTexture->stop();

				newList.erase(contextEntityId.index);

				solver->setLights(newList); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
				// updateAllPanels() must follow, it deletes lightprops
			}
			break;

		case CM_STATIC_OBJECT_UNWRAP:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->getStaticObjects().size())
			// intentionaly no break
		case CM_STATIC_OBJECTS_UNWRAP:
			if (solver)
			{
				unsigned res = 256;
				if (getResolution(_("Unwrap build"),this,res,false))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building unwrap..."));

					if (actionCode==CM_STATIC_OBJECT_UNWRAP)
					{
						rr::RRObjects objects;
						objects.push_back(solver->getStaticObjects()[contextEntityId.index]);
						objects.buildUnwrap(res,true,solver->aborting);
					}
					else
						solver->getStaticObjects().buildUnwrap(res,true,solver->aborting);

					// static objects may be modified even after abort (unwrap is not atomic)
					// so it's better if following setStaticObjects is not aborted
					solver->aborting = false;

					svframe->m_canvas->addOrRemoveScene(NULL,true);
				}
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything

		case CM_STATIC_OBJECT_BUILD_LMAP:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->getStaticObjects().size())
			// intentionaly no break
		case CM_STATIC_OBJECTS_BUILD_LMAPS:
			if (solver)
			{
				unsigned res = 256; // 0=vertex buffers
				unsigned quality = 100;
				if (getResolution(_("Lightmap build"),this,res,true) && getQuality(_("Lightmap build"),this,quality))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building lightmaps..."));

					// allocate buffers
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
						if (actionCode==CM_STATIC_OBJECTS_BUILD_LDMS || i==contextEntityId.index)
							if (solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices())
							{
								delete solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber);
								solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber) = res
									? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBA,true,NULL) // A in RGBA is only for debugging
									: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
							}

					// calculate all
					solver->leaveFireball();
					svframe->m_canvas->fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					params.aoIntensity = svs.lightmapDirectParameters.aoIntensity;
					params.aoSize = svs.lightmapDirectParameters.aoSize;
					if (actionCode==CM_STATIC_OBJECTS_BUILD_LMAPS)
						solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,&svs.lightmapFilteringParameters);
					else
					{
						solver->updateLightmaps(-1,-1,-1,&params,&params,&svs.lightmapFilteringParameters);
						params.applyCurrentSolution = true;
						solver->updateLightmap(contextEntityId.index,solver->getStaticObjects()[contextEntityId.index]->illumination.getLayer(svs.staticLayerNumber),NULL,NULL,&params,&svs.lightmapFilteringParameters);
					}

					// save calculated lightmaps
					solver->getStaticObjects().saveLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);

					// make results visible
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
				}
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything

		case CM_STATIC_OBJECT_BUILD_LDM:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->getStaticObjects().size())
			// intentionaly no break
		case CM_STATIC_OBJECTS_BUILD_LDMS:
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

					// build ldm
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
						if (actionCode==CM_STATIC_OBJECTS_BUILD_LDMS || i==contextEntityId.index)
						{
							delete solver->getStaticObjects()[i]->illumination.getLayer(svs.ldmLayerNumber);
							solver->getStaticObjects()[i]->illumination.getLayer(svs.ldmLayerNumber) =
								rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
						}
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
					if (actionCode==CM_STATIC_OBJECTS_BUILD_LDMS)
						solver->updateLightmaps(svs.ldmLayerNumber,-1,-1,&paramsDirect,&paramsIndirect,&svs.lightmapFilteringParameters);
					else
					{
						solver->updateLightmaps(-1,-1,-1,&paramsDirect,&paramsIndirect,&svs.lightmapFilteringParameters);
						paramsDirect.applyCurrentSolution = true;
						solver->updateLightmap(contextEntityId.index,solver->getStaticObjects()[contextEntityId.index]->illumination.getLayer(svs.ldmLayerNumber),NULL,NULL,&paramsDirect,&svs.lightmapFilteringParameters);
					}
					solver->setEnvironment(oldEnv);
					delete newEnv;

					// save ldm to disk
					solver->getStaticObjects().saveLayer(svs.ldmLayerNumber,LDM_PREFIX,LDM_POSTFIX);

					// make results visible
					if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS)
						svs.renderLightDirect = LD_REALTIME;
					if (svs.renderLightIndirect==LI_NONE || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)
						svs.renderLightIndirect = LI_CONSTANT;
					svs.renderLDM = true;
				}
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything

		case CM_STATIC_OBJECT_SMOOTH:
		case CM_STATIC_OBJECTS_SMOOTH: // right now, it smooths also dynamic objects
			{
				if (svframe->smoothDlg.ShowModal()==wxID_OK)
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Smoothing..."));

					double d = 0;
					float weldDistance = svframe->smoothDlg.weldDistance->GetValue().ToDouble(&d) ? (float)d : 0;
					float smoothAngle = svframe->smoothDlg.smoothAngle->GetValue().ToDouble(&d) ? (float)d : 30;
					rr::RRObjects smoothObjects;
					for (unsigned pass=0;pass<2;pass++)
					{
						const rr::RRObjects& solverObjects = pass?solver->getDynamicObjects():solver->getStaticObjects();
						for (unsigned i=0;i<solverObjects.size();i++)
							if (actionCode==CM_STATIC_OBJECTS_SMOOTH || solverObjects[i]->getCollider()->getMesh()==svframe->m_objectProperties->object->getCollider()->getMesh())
								smoothObjects.push_back(solverObjects[i]);
					}
					if (smoothObjects.size())
					{
						smoothObjects.stitchAndSmooth(
							svframe->smoothDlg.splitVertices->GetValue(),
							svframe->smoothDlg.stitchVertices->GetValue(),
							true,true,
							svframe->smoothDlg.preserveUvs->GetValue(),
							weldDistance,
							RR_DEG2RAD(smoothAngle),true);
						svframe->m_canvas->addOrRemoveScene(NULL,true);
					}
				}
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything
		case CM_STATIC_OBJECTS_MERGE:
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Merging objects..."));

				rr::RRObject* oldObject = rr::RRObject::createMultiObject(&solver->getStaticObjects(),rr::RRCollider::IT_LINEAR,solver->aborting,-1,-1,false,0,NULL);

				// convert oldObject with Multi* into newObject with RRMeshArrays
				// if we don't do it
				// - solver->getMultiObjectCustom() preimport numbers would point to many 1objects, although there is only one 1object now
				// - attempt to smooth scene would fail, it needs arrays
				const rr::RRCollider* oldCollider = oldObject->getCollider();
				const rr::RRMesh* oldMesh = oldCollider->getMesh();
				rr::RRVector<unsigned> texcoords;
				oldMesh->getUvChannels(texcoords);
				rr::RRMeshArrays* newMesh = oldMesh->createArrays(true,texcoords);
				const rr::RRCollider* newCollider = rr::RRCollider::create(newMesh,rr::RRCollider::IT_LINEAR,solver->aborting);
				rr::RRObject* newObject = new rr::RRObject;
				newObject->faceGroups = oldObject->faceGroups;
				newObject->setCollider(newCollider);
				delete oldObject;

				rr::RRObjects objects;
				objects.push_back(newObject);
				solver->setStaticObjects(objects,NULL); // memleak, newObject is never freed
				svframe->m_canvas->addOrRemoveScene(NULL,false);
			}
			break;
		case CM_STATIC_OBJECT_TANGENTS:
		case CM_STATIC_OBJECTS_TANGENTS:
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Building tangents..."));

				for (unsigned i=0;i<solver->getStaticObjects().size();i++)
				{
					if (actionCode==CM_STATIC_OBJECTS_TANGENTS || i==contextEntityId.index)
					{
						rr::RRMeshArrays* arrays = dynamic_cast<rr::RRMeshArrays*>(const_cast<rr::RRMesh*>(solver->getStaticObjects()[i]->getCollider()->getMesh()));
						if (arrays)
							arrays->buildTangents();
					}
				}
				svframe->m_canvas->addOrRemoveScene(NULL,true);
			}
			return; // skip updateAllPanels() at the end of this function, we did not change anything

		case CM_STATIC_OBJECT_DELETE:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->getStaticObjects().size())
			{
				// display log window with 'abort' while this function runs
				LogWithAbort logWithAbort(this,solver,_("Deleting..."));

				if (svs.playVideos)
				{
					// stop videos
					wxKeyEvent event;
					event.m_keyCode = ' ';
					svframe->m_canvas->OnKeyDown(event);
				}
				rr::RRObjects newList = solver->getStaticObjects();
				newList.erase(contextEntityId.index);
				solver->setStaticObjects(newList,NULL);
				svframe->m_canvas->addOrRemoveScene(NULL,false); // updateAllPanels() is called from here
				return; // skip updateAllPanels() at the end of this function to prevent SceneTree from updating twice, it's terribly slow
			}
			break;
		case CM_DYNAMIC_OBJECT_DELETE:
			if (solver && contextEntityId.isOk() && contextEntityId.index<solver->getDynamicObjects().size())
			{
				if (svs.playVideos)
				{
					// stop videos
					wxKeyEvent event;
					event.m_keyCode = ' ';
					svframe->m_canvas->OnKeyDown(event);
				}
				rr::RRObjects newList = solver->getDynamicObjects();
				newList.erase(contextEntityId.index);
				solver->setDynamicObjects(newList); // RRObject in object props is deleted here, objectprops is temporarily unsafe
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
