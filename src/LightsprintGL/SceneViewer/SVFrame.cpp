// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
//
// include

#include "Lightsprint/RRScene.h"
#include "SVFrame.h"
#include "SVRayLog.h"
#include "SVSolver.h"
#include "SVSaveLoad.h"
#include "../tmpstr.h"

namespace rr_gl
{

// "123abc" -> 123
unsigned getUnsigned(const wxString& str)
{
	unsigned result = 0;
	for (unsigned i=0;i<str.size() && str[i]>='0' && str[i]<='9';i++)
	{
		result = 10*result + (char)str[i]-'0';
	}
	return result;
}

// true = valid answer
// false = dialog was escaped
// Future version may use incoming quality as default value.
static bool getQuality(wxWindow* parent, unsigned& quality)
{
	wxArrayString choices;
	choices.Add("1 - extremely low");
	choices.Add("10 - low");
	choices.Add("40");
	choices.Add("100 - medium");
	choices.Add("350");
	choices.Add("1000 - high");
	choices.Add("3000");
	choices.Add("10000 - extremely high");
	wxSingleChoiceDialog dialog(parent,"","Please select quality",choices);
	if (dialog.ShowModal()==wxID_OK)
	{
		quality = getUnsigned(dialog.GetStringSelection());
		return true;
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
// Future version may use incoming quality as default value.
static bool getResolution(wxWindow* parent, unsigned& resolution, bool offerPerVertex)
{
	wxArrayString choices;
	if (offerPerVertex)	choices.Add("per-vertex");
	choices.Add("8x8");
	choices.Add("16x16");
	choices.Add("32x32");
	choices.Add("64x64");
	choices.Add("128x128");
	choices.Add("256x256");
	choices.Add("512x512");
	choices.Add("1024x1024");
	choices.Add("2048x2048");
	choices.Add("4096x4096");
	wxSingleChoiceDialog dialog(parent,"","Please select texture resolution",choices);
	if (dialog.ShowModal()==wxID_OK)
	{
		resolution = getUnsigned(dialog.GetStringSelection());
		return true;
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
static bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption)
{
	char value[30];
	sprintf(value,"%f",factor);
	wxTextEntryDialog dialog(parent,message,caption,value);
	if (dialog.ShowModal()==wxID_OK)
	{
		double d = 1;
		if (dialog.GetValue().ToDouble(&d))
		{
			factor = (float)d;
			return true;
		}
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
static bool getSpeed(wxWindow* parent, float& speed)
{
	return getFactor(parent,speed,"Please adjust camera speed (m/s)","Camera speed");
}

// true = valid answer
// false = dialog was escaped
static bool getBrightness(wxWindow* parent, rr::RRVec4& brightness)
{
	float average = brightness.avg();
	if (getFactor(parent,average,"Please adjust brightness.","Brightness"))
	{
		brightness = rr::RRVec4(average);
		return true;
	}
	return false;
}

void SVFrame::UpdateEverything(rr::RRScene* newScene)
{
	// workaround for 2.8.9 problem fixed in 2.9.0
	// remember canvas size, only first canvas is sized automatically
	wxSize newSize = m_canvas ? m_canvas->GetSize() : wxDefaultSize;

	RR_SAFE_DELETE(m_lightProperties);
	RR_SAFE_DELETE(m_canvas);

	svs.manuallyOpenedScene = newScene;

	m_canvas = new SVCanvas( svs, this, &m_lightProperties, newSize);

	// must go after SVCanvas() otherwise canvas stays 16x16 pixels
	Show(true);

	UpdateMenuBar();

	// must go after Show() otherwise SetCurrent() in createContext() fails
	m_canvas->createContext( svs );
	
	// without SetFocus, keyboard events may be sent to frame instead of canvas
	m_canvas->SetFocus();


	if (svs.autodetectCamera && !(svs.initialInputSolver && svs.initialInputSolver->aborting)) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_CAMERA_GENERATE_RANDOM));
	if (svs.fullscreen) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_MAXIMIZE));
}

SVFrame* SVFrame::Create(SceneViewerStateEx& svs)
{
	wxString str = wxT("SceneViewer");

	// open at ~50% of screen size
	int x,y,width,height;
	::wxClientDisplayRect(&x,&y,&width,&height);
	const int border = (width+height)/25;
	SVFrame *frame = new SVFrame(NULL, str, wxPoint(x+2*border,y+border), wxSize(width-4*border,height-2*border), svs);

	frame->UpdateEverything(NULL);

	return frame;
}

void SVFrame::OnExit(wxCommandEvent& event)
{
	// true is to force the frame to close
	Close(true);
}

SVFrame::SVFrame(wxWindow* _parent, const wxString& _title, const wxPoint& _pos, const wxSize& _size, SceneViewerStateEx& _svs)
	: wxFrame(_parent, wxID_ANY, _title, _pos, _size, wxDEFAULT_FRAME_STYLE), svs(_svs)
{
	m_canvas = NULL;
	m_lightProperties = NULL;

	static const char * sample_xpm[] = {
	/* columns rows colors chars-per-pixel */
	"32 32 6 1",
	"  c black",
	". c navy",
	"X c red",
	"o c yellow",
	"O c gray100",
	"+ c None",
	/* pixels */
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++              ++++++++++",
	"++++++++ ............ ++++++++++",
	"++++++++ ............ ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......              ",
	"++++++++ .OO...... oooooooooooo ",
	"         .OO...... oooooooooooo ",
	" XXXXXXX .OO...... oOOooooooooo ",
	" XXXXXXX .OO...... oOOooooooooo ",
	" XOOXXXX ......... oOOooooooooo ",
	" XOOXXXX ......... oOOooooooooo ",
	" XOOXXXX           oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oooooooooooo ",
	" XOOXXXXXXXXX ++++ oooooooooooo ",
	" XXXXXXXXXXXX ++++              ",
	" XXXXXXXXXXXX ++++++++++++++++++",
	"              ++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++"
	};
	SetIcon(wxIcon(sample_xpm));
}

void SVFrame::UpdateMenuBar()
{
	//const SVSolver* solver = m_canvas->solver;
	//SceneViewerState& svs = m_canvas->svs;

	wxMenuBar *menuBar = new wxMenuBar;
	wxMenu *winMenu = NULL;

	// Select...
	//winMenu = new wxMenu;
	//char buf[100];
	//winMenu->Append(-1,_T("camera"));
	//for (unsigned i=0;i<solver->getLights().size();i++)
	//{
	//	sprintf(buf,"light %d",i);
	//	winMenu->Append(i,_T(buf));
	//}
	//for (unsigned i=0;i<solver->getStaticObjects().size();i++)
	//{
	//	sprintf(buf,"object %d",i);
	//	winMenu->Append(1000+i,_T(buf));
	//}
	//menuBar->Append(winMenu, _T("Select"));

	// File...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_FILE_OPEN_SCENE,_T("Open scene..."));
		menuBar->Append(winMenu, _T("File"));
	}

	// Camera...
	winMenu = new wxMenu;
	winMenu->Append(ME_CAMERA_GENERATE_RANDOM,_T("Set random camera"));
	winMenu->Append(ME_CAMERA_SPEED,_T("Set camera speed..."));
	menuBar->Append(winMenu, _T("Camera"));

	// Lights...
	winMenu = new wxMenu;
	winMenu->Append(ME_SHOW_LIGHT_PROPERTIES,_T("Show light properties"));
	winMenu->Append(ME_LIGHT_DIR,_T("Add dir light"));
	winMenu->Append(ME_LIGHT_SPOT,_T("Add spot light"));
	winMenu->Append(ME_LIGHT_POINT,_T("Add point light"));
	winMenu->Append(ME_LIGHT_DELETE,_T("Delete selected light"));
	winMenu->Append(ME_LIGHT_AMBIENT,svs.renderAmbient?_T("Delete constant ambient"):_T("Add constant ambient"));
	menuBar->Append(winMenu, _T("Lights"));


	// Environment...
	winMenu = new wxMenu;
	winMenu->Append(ME_ENV_WHITE,_T("Set white"));
	winMenu->Append(ME_ENV_BLACK,_T("Set black"));
	winMenu->Append(ME_ENV_WHITE_TOP,_T("Set white top"));
	menuBar->Append(winMenu, _T("Environment"));

	// Static lighting...
	winMenu = new wxMenu;
	winMenu->Append(ME_STATIC_3D,_T("Render static lighting"));
	winMenu->Append(ME_STATIC_2D,_T("Render static lighting in 2D"));
	winMenu->Append(ME_STATIC_BILINEAR,_T("Toggle lightmap bilinear interpolation"));
	winMenu->Append(ME_STATIC_BUILD,_T("Build lightmaps..."));
	winMenu->Append(ME_STATIC_BUILD_1OBJ,_T("Build lightmap for selected obj, only direct..."));
#ifdef DEBUG_TEXEL
	winMenu->Append(ME_STATIC_DIAGNOSE,_T("Diagnose texel..."));
#endif
	winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_2D,_T("Build 2d lightfield"));
	winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_3D,_T("Build 3d lightfield"));
	winMenu->Append(ME_STATIC_SAVE,_T("Save"));
	winMenu->Append(ME_STATIC_LOAD,_T("Load"));
	menuBar->Append(winMenu, _T("Static lighting"));

	// Realtime lighting...
	winMenu = new wxMenu;
	winMenu->Append(ME_REALTIME_ARCHITECT,_T("Render realtime GI: architect"));
	winMenu->Append(ME_REALTIME_FIREBALL,_T("Render realtime GI: fireball"));
	winMenu->Append(ME_REALTIME_FIREBALL_BUILD,_T("Build fireball..."));
	winMenu->Append(ME_REALTIME_LDM_BUILD,_T("Build light detail map..."));
	winMenu->Append(ME_REALTIME_LDM,svs.renderLDM?_T("Disable light detail map"):_T("Enable light detail map"));
	menuBar->Append(winMenu, _T("Realtime lighting"));

	// Render...
	winMenu = new wxMenu;
	winMenu->Append(ME_MAXIMIZE,svs.fullscreen?_T("Windowed"):_T("Fullscreen"));
	winMenu->Append(ME_RENDER_HELPERS,svs.renderHelpers?_T("Hide helpers"):_T("Show helpers"));
	winMenu->Append(ME_RENDER_DIFFUSE,svs.renderDiffuse?_T("Disable diffuse color"):_T("Enable diffuse color"));
	winMenu->Append(ME_RENDER_SPECULAR,svs.renderSpecular?_T("Disable specular reflection"):_T("Enable specular reflection"));
	winMenu->Append(ME_RENDER_EMISSION,svs.renderEmission?_T("Disable emissivity"):_T("Enable emissivity"));
	winMenu->Append(ME_RENDER_TRANSPARENT,svs.renderTransparent?_T("Disable transparency"):_T("Enable transparency"));
	winMenu->Append(ME_RENDER_WATER,svs.renderWater?_T("Disable water"):_T("Enable water"));
	winMenu->Append(ME_RENDER_TEXTURES,svs.renderTextures?_T("Disable textures"):_T("Enable textures"));
	winMenu->Append(ME_RENDER_WIREFRAME,svs.renderWireframe?_T("Disable wireframe"):_T("Wireframe"));
	winMenu->Append(ME_RENDER_TONEMAPPING,svs.adjustTonemapping?_T("Disable tone mapping"):_T("Enable tone mapping"));
	winMenu->Append(ME_RENDER_BRIGHTNESS,_T("Adjust brightness..."));
	menuBar->Append(winMenu, _T("Render"));

	winMenu = new wxMenu;
	winMenu->Append(ME_CHECK_SOLVER,_T("Log solver diagnose"));
	winMenu->Append(ME_CHECK_SCENE,_T("Log scene errors"));
	winMenu->Append(ME_HELP,_T("Help"));
	menuBar->Append(winMenu, _T("Misc"));


	wxMenuBar* oldMenuBar = GetMenuBar();
	SetMenuBar(menuBar);
	delete oldMenuBar;
}

void SVFrame::OnMenuEvent(wxCommandEvent& event)
{
	RR_ASSERT(m_canvas);
	SVSolver*& solver = m_canvas->solver;
	RR_ASSERT(solver);
	rr::RRLightField*& lightField = m_canvas->lightField;
	bool& fireballLoadAttempted = m_canvas->fireballLoadAttempted;
	unsigned& centerObject = m_canvas->centerObject;
	unsigned& centerTexel = m_canvas->centerTexel;
	int* windowCoord = m_canvas->windowCoord;
	bool& ourEnv = m_canvas->ourEnv;

	switch (event.GetId())
	{
		// Select...
		//if (item<0) selectedType = ST_CAMERA;
		//if (item>=0 && item<1000) {selectedType = ST_LIGHT; svs.selectedLightIndex = item;}
		//if (item>=1000) {selectedType = ST_OBJECT; svs.selectedObjectIndex = item-1000;}

		case ME_RENDER_DIFFUSE: svs.renderDiffuse = !svs.renderDiffuse; break;
		case ME_RENDER_SPECULAR: svs.renderSpecular = !svs.renderSpecular; break;
		case ME_RENDER_EMISSION: svs.renderEmission = !svs.renderEmission; break;
		case ME_RENDER_TRANSPARENT: svs.renderTransparent = !svs.renderTransparent; break;
		case ME_RENDER_WATER: svs.renderWater = !svs.renderWater; break;
		case ME_RENDER_TEXTURES: svs.renderTextures = !svs.renderTextures; break;
		case ME_RENDER_TONEMAPPING: svs.adjustTonemapping = !svs.adjustTonemapping; break;
		case ME_RENDER_WIREFRAME: svs.renderWireframe = !svs.renderWireframe; break;
		case ME_RENDER_HELPERS: svs.renderHelpers = !svs.renderHelpers; break;

		case ME_MAXIMIZE:
			svs.fullscreen = !svs.fullscreen;
			ShowFullScreen(svs.fullscreen,wxFULLSCREEN_ALL);
			GetPosition(windowCoord+0,windowCoord+1);
			GetSize(windowCoord+2,windowCoord+3);
			break;

		case ME_CHECK_SOLVER: solver->checkConsistency(); break;
		case ME_CHECK_SCENE: solver->getMultiObjectCustom()->getCollider()->getMesh()->checkConsistency(); break;

		case ME_STATIC_3D:
			svs.renderRealtime = 0;
			svs.render2d = 0;
			break;
		case ME_STATIC_2D:
			svs.renderRealtime = 0;
			svs.render2d = 1;
			break;
		case ME_STATIC_BILINEAR:
			svs.renderBilinear = !svs.renderBilinear;
			svs.renderRealtime = false;
			for (unsigned i=0;i<solver->getNumObjects();i++)
			{	
				if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
				{
					glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_2D_LIGHT_INDIRECT);
					rr_gl::getTexture(solver->getIllumination(i)->getLayer(svs.staticLayerNumber))->bindTexture();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, svs.renderBilinear?GL_LINEAR:GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, svs.renderBilinear?GL_LINEAR:GL_NEAREST);
				}
			}
			break;
		case ME_STATIC_LOAD:
			solver->getStaticObjects().loadLayer(svs.staticLayerNumber,"","png");
			svs.renderRealtime = false;
			break;
		case ME_STATIC_SAVE:
			solver->getStaticObjects().saveLayer(svs.staticLayerNumber,"","png");
			break;
		case ME_STATIC_BUILD_LIGHTFIELD_2D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRDynamicSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
				aabbMin.y = aabbMax.y = svs.eye.pos.y;
				aabbMin.w = aabbMax.w = 0;
				delete lightField;
				lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				lightField->captureLighting(solver,0);
			}
			break;
		case ME_STATIC_BUILD_LIGHTFIELD_3D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRDynamicSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
				aabbMin.w = aabbMax.w = 0;
				delete lightField;
				lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				lightField->captureLighting(solver,0);
			}
			break;
		case ME_STATIC_BUILD_1OBJ:
			{
				unsigned quality = 100;
				if (getQuality(this,quality))
				{
					// calculate 1 object, direct lighting
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmap(svs.selectedObjectIndex,solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber),NULL,NULL,&params,&filtering);
					svs.renderRealtime = false;
					// propagate computed data from buffers to textures
					if (solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber) && solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
						getTexture(solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber))->reset(true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					solver->resetRenderCache();
				}
			}
			break;
#ifdef DEBUG_TEXEL
		case ME_STATIC_DIAGNOSE:
			{
				if (centerObject!=UINT_MAX)
				{
					solver->leaveFireball();
					fireballLoadAttempted = false;
					unsigned quality = 100;
					if (getQuality(this,quality))
					{
						rr::RRDynamicSolver::UpdateParameters params(quality);
						params.debugObject = centerObject;
						params.debugTexel = centerTexel;
						params.debugTriangle = UINT_MAX;//centerTriangle;
						params.debugRay = SVRayLog::push_back;
						SVRayLog::size = 0;
						solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,NULL);
					}
				}
			}
			break;
#endif
		case ME_STATIC_BUILD:
			{
				unsigned res = 256; // 0=vertex buffers
				unsigned quality = 100;
				if (getResolution(this,res,true) && getQuality(this,quality))
				{
					// allocate buffers
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if (solver->getIllumination(i) && solver->getObject(i)->getCollider()->getMesh()->getNumVertices())
						{
							delete solver->getIllumination(i)->getLayer(svs.staticLayerNumber);
							solver->getIllumination(i)->getLayer(svs.staticLayerNumber) = res
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL)
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
						}
					}

					// calculate all
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,&filtering);
					svs.renderRealtime = false;
					// propagate computed data from buffers to textures
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
							getTexture(solver->getIllumination(i)->getLayer(svs.staticLayerNumber))->reset(true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
					}

					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					if (res)
					{
						solver->resetRenderCache();
					}
				}
			}
			break;

		case ME_REALTIME_FIREBALL:
			svs.renderRealtime = 1;
			svs.render2d = 0;
			solver->dirtyLights();
			if (!fireballLoadAttempted) 
			{
				fireballLoadAttempted = true;
				solver->loadFireball(NULL);
			}
			break;
		case ME_REALTIME_ARCHITECT:
			svs.renderRealtime = 1;
			svs.render2d = 0;
			solver->dirtyLights();
			fireballLoadAttempted = false;
			solver->leaveFireball();
			break;
		case ME_REALTIME_LDM:
			svs.renderRealtime = 1;
			solver->dirtyLights();
			svs.renderLDM = !svs.renderLDM;
			break;
		case ME_REALTIME_LDM_BUILD:
			{
				unsigned res = 256;
				unsigned quality = 100;
				if (getQuality(this,quality) && getResolution(this,res,false))
				{
					svs.renderRealtime = 1;
					svs.render2d = 0;
					solver->dirtyLights();
					svs.renderLDM = 1;

					// if in fireball mode, leave it, otherwise updateLightmaps() below fails
					fireballLoadAttempted = false;
					solver->leaveFireball();

					for (unsigned i=0;i<solver->getNumObjects();i++)
						solver->getIllumination(i)->getLayer(svs.ldmLayerNumber) =
							rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
					rr::RRDynamicSolver::UpdateParameters paramsDirect(quality);
					paramsDirect.applyLights = 0;
					rr::RRDynamicSolver::UpdateParameters paramsIndirect(quality);
					paramsIndirect.applyLights = 0;
					paramsIndirect.locality = 1;
					const rr::RRBuffer* oldEnv = solver->getEnvironment();
					rr::RRBuffer* newEnv = rr::RRBuffer::createSky(rr::RRVec4(0.5f),rr::RRVec4(0.5f)); // higher sky color would decrease effect of emissive materials
					solver->setEnvironment(newEnv);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.backgroundColor = rr::RRVec4(0.5f);
					filtering.wrap = false;
					filtering.smoothBackground = true;
					solver->updateLightmaps(svs.ldmLayerNumber,-1,-1,&paramsDirect,&paramsIndirect,&filtering); 
					solver->setEnvironment(oldEnv);
					delete newEnv;
				}
			}
			break;
		case ME_REALTIME_FIREBALL_BUILD:
			{
				unsigned quality = 100;
				if (getQuality(this,quality))
				{
					svs.renderRealtime = 1;
					svs.render2d = 0;
					solver->buildFireball(quality,NULL);
					solver->dirtyLights();
					fireballLoadAttempted = true;
				}
			}
			break;

		case ME_CAMERA_GENERATE_RANDOM:
			svs.eye.setPosDirRangeRandomly(solver->getMultiObjectCustom());
			svs.cameraMetersPerSecond = svs.eye.getFar()*0.08f;
			break;
		case ME_RENDER_BRIGHTNESS:
			getBrightness(this,svs.brightness);
			break;
		case ME_CAMERA_SPEED:
			getSpeed(this,svs.cameraMetersPerSecond);
			break;
			

		case ME_ENV_WHITE:
		case ME_ENV_BLACK:
		case ME_ENV_WHITE_TOP:
			if (ourEnv)
			{
				delete solver->getEnvironment();
			}
			ourEnv = true;
			switch (event.GetId())
			{
				case ME_ENV_WHITE: solver->setEnvironment(rr::RRBuffer::createSky()); break;
				case ME_ENV_BLACK: solver->setEnvironment(NULL); break;
				case ME_ENV_WHITE_TOP: solver->setEnvironment(rr::RRBuffer::createSky(rr::RRVec4(1),rr::RRVec4(0))); break;
			}
			break;


		case ME_LIGHT_DIR:
		case ME_LIGHT_SPOT:
		case ME_LIGHT_POINT:
			{
				rr::RRLights newList = solver->getLights();
				rr::RRLight* newLight = NULL;
				switch (event.GetId())
				{
					case ME_LIGHT_DIR: newLight = rr::RRLight::createDirectionalLight(rr::RRVec3(-1),rr::RRVec3(1),true); break;
					case ME_LIGHT_SPOT: newLight = rr::RRLight::createSpotLight(svs.eye.pos,rr::RRVec3(1),svs.eye.dir,svs.eye.getFieldOfViewVerticalRad()/2,svs.eye.getFieldOfViewVerticalRad()/4); break;
					case ME_LIGHT_POINT: newLight = rr::RRLight::createPointLight(svs.eye.pos,rr::RRVec3(1)); break;
				}
				m_canvas->lightsToBeDeletedOnExit.push_back(newLight);
				if (!newList.size()) svs.renderAmbient = 0; // disable ambient when adding first light
				newList.push_back(newLight);
				solver->setLights(newList); // RealtimeLight in light props is deleted here
				svs.selectedLightIndex = newList.size()?newList.size()-1:0; // select new light
				if (m_lightProperties)
				{
					m_lightProperties->setLight(solver->realtimeLights[svs.selectedLightIndex]); // light props is updated to new light
				}
			}
			break;
		case ME_LIGHT_DELETE:
			if (svs.selectedLightIndex<solver->realtimeLights.size())
			{
				rr::RRLights newList = solver->getLights();
				newList.erase(svs.selectedLightIndex);
				if (svs.selectedLightIndex && svs.selectedLightIndex==newList.size())
					svs.selectedLightIndex--;
				if (!newList.size())
				{
					m_canvas->selectedType = SVCanvas::ST_CAMERA;
					// enable ambient when deleting last light
					//  but only when scene doesn't contain emissive materials
					svs.renderAmbient = !solver->getMaterialsInStaticScene().MATERIAL_EMISSIVE_CONST && !solver->getMaterialsInStaticScene().MATERIAL_EMISSIVE_MAP;
				}
				RR_SAFE_DELETE(m_lightProperties); // delete light props
				solver->setLights(newList); // RealtimeLight in light props is deleted here
			}
			break;
		case ME_LIGHT_AMBIENT: svs.renderAmbient = !svs.renderAmbient; break;
		case ME_FILE_OPEN_SCENE:
			{
				wxFileDialog dialog(this,"Choose a 3d scene","","","*.*",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					wxString newSceneFilename = dialog.GetPath();
					rr::RRScene* newScene = new rr::RRScene(newSceneFilename);
					if (newScene->getObjects())
					{
						strncpy(svs.sceneFilename,newSceneFilename.c_str(),svs.MAX_FILENAME_LENGTH);
						svs.sceneFilename[svs.MAX_FILENAME_LENGTH] = 0;
						svs.autodetectCamera = true;
						UpdateEverything(newScene);
					}
					else
					{
						delete newScene;
					}
				}
			}
			break;
		case ME_SHOW_LIGHT_PROPERTIES:
			if (svs.selectedLightIndex<solver->getLights().size())
			{
				if (!m_lightProperties)
				{
					m_lightProperties = new SVLightProperties( this );
				}
				m_lightProperties->setLight(solver->realtimeLights[svs.selectedLightIndex]);
				m_lightProperties->Show(true);
			}
			break;
		case ME_HELP:
			wxMessageBox("To LOOK, move mouse with right button pressed.\n"
				"To MOVE, use arrows or wsadqzxc.\n"
				"To ZOOM, use wheel.\n"
				"\n"
				"To change lights or lighting techniques, use menu.",
				"Controls");
			break;
	}

	UpdateMenuBar();
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
END_EVENT_TABLE()

}; // namespace
