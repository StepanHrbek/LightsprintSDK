// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVCanvas.h"
#include "SVEntityIcons.h"
#include "SVRayLog.h"
#include "SVSaveLoad.h"
#include "SVFrame.h"
#include "SVLightProperties.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/Bloom.h"
#include "Lightsprint/GL/LensFlare.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Water.h"
#include "../PreserveState.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif
#include <cctype>
#include <boost/filesystem.hpp> // extension()==".gsa"

namespace bf = boost::filesystem;

#if !wxUSE_GLCANVAS
    #error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

namespace rr_gl
{


/////////////////////////////////////////////////////////////////////////////
//
// SVCanvas

static int s_attribList[] = {
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	//WX_GL_SAMPLE_BUFFERS, GL_TRUE, // produces no visible improvement
	WX_GL_SAMPLES, 4, // antialiasing. can be later disabled by glDisable(GL_MULTISAMPLE), but it doesn't improve speed (tested on X1650). it must be disabled here (change 4 to 1) for higher fps
	WX_GL_DEPTH_SIZE, 24, // default is 16, explicit 24 should reduce z-fight. 32 fails on all cards tested including 5870 (falls back to default without antialiasing)
	0, 0};

SVCanvas::SVCanvas( SceneViewerStateEx& _svs, SVFrame *_parent, wxSize _size)
	: wxGLCanvas(_parent, wxID_ANY, s_attribList, wxDefaultPosition, _size, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE, "GLCanvas"), svs(_svs)
{
	renderEmptyFrames = false;
	context = NULL;
	parent = _parent;
	solver = NULL;
	selectedType = ST_CAMERA;
	winWidth = 0;
	winHeight = 0;
	windowCoord[0] = 0;
	windowCoord[1] = 0;
	windowCoord[2] = 800;
	windowCoord[3] = 600;
	water = NULL;
	toneMapping = NULL;
	fireballLoadAttempted = 0;
	speedForward = 0;
	speedBack = 0;
	speedRight = 0;
	speedLeft = 0;
	speedUp = 0;
	speedDown = 0;
	speedLean = 0;
	exitRequested = 0;
	menuHandle = 0;
	envToBeDeletedOnExit = false;
	lv = NULL;
	mousePositionInWindow = rr::RRVec2(0);
	centerObject = UINT_MAX;
	centerTexel = UINT_MAX;
	centerTriangle = UINT_MAX;
	ray = NULL;
	collisionHandler = NULL;
	fontInited = false;

	helpLoadAttempted = false;
	helpImage = NULL;

	bloomLoadAttempted = false;
	bloom = NULL;

	lensFlareLoadAttempted = false;
	lensFlare = NULL;

	vignetteLoadAttempted = false;
	vignetteImage = NULL;

	logoLoadAttempted = false;
	logoImage = NULL;

	lightField = NULL;
	lightFieldQuadric = NULL;
	lightFieldObjectIllumination = NULL;

	textureRenderer = NULL;
	fpsLoadAttempted = false;
	fpsDisplay = NULL;

	entityIcons = NULL;
	sunIconPosition = rr::RRVec3(0);
	iconSize = 1;
	fullyCreated = false;

	timeWhenSkyboxBlendingStarted = 0;

}

void SVCanvas::createContextCore()
{
	context = new wxGLContext(this);
	SetCurrent(*context);

	// init GLEW
	if (glewInit()!=GLEW_OK)
	{
		rr::RRReporter::report(rr::ERRO,"GLEW init failed (OpenGL 2.0 capable graphics card is required).\n");
		exitRequested = true;
		return;
	}

	// init GL
	rr::RRReporter::report(rr::INF2,"OpenGL %s by %s on %s.\n",glGetString(GL_VERSION),glGetString(GL_VENDOR),glGetString(GL_RENDERER));
	int major, minor;
	if (sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
	{
		rr::RRReporter::report(rr::ERRO,"Your system does not support OpenGL 2.0. Note: Some systems degrade OpenGL version in presence of secondary display.\n");
		exitRequested = true;
		return;
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);


	{
		int viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);
		winWidth = viewport[2];
		winHeight = viewport[3];
	}


	// init solver
	solver = new rr_gl::RRDynamicSolverGL(svs.pathToShaders);
	textureRenderer = solver->getRendererOfScene()->getTextureRenderer();
	solver->setScaler(rr::RRScaler::createRgbScaler());
	if (svs.initialInputSolver)
	{
		delete solver->getScaler();
		solver->setScaler(svs.initialInputSolver->getScaler());
		solver->setEnvironment(svs.initialInputSolver->getEnvironment());
		envToBeDeletedOnExit = false;
		solver->setStaticObjects(svs.initialInputSolver->getStaticObjects(),NULL,NULL,rr::RRCollider::IT_BSP_FASTER,svs.initialInputSolver); // smoothing and multiobject are taken from _solver
		solver->setDynamicObjects(svs.initialInputSolver->getDynamicObjects());
		solver->setLights(svs.initialInputSolver->getLights());
	}
	else
	if (!svs.sceneFilename.empty())
	{
		// load scene
		rr::RRScene* scene = parent->loadScene(svs.sceneFilename,parent->userPreferences.import.getUnitLength(svs.sceneFilename),parent->userPreferences.import.getUpAxis(svs.sceneFilename));
		mergedScenes.push_back(scene);

		// send everything to solver
		solver->setEnvironment(mergedScenes[0]->environment);
		envToBeDeletedOnExit = false;
		solver->setStaticObjects(mergedScenes[0]->objects,NULL);
		solver->setLights(mergedScenes[0]->lights);
		svs.autodetectCamera = true; // new scene, camera is not set
	}

	// warning: when rendering scene from initialInputSolver, original cube buffers are lost here
	reallocateBuffersForRealtimeGI(true);

	// load skybox from filename only if we don't have it from inputsolver or scene yet
	if (!solver->getEnvironment() && !svs.skyboxFilename.empty())
	{
		rr::RRReportInterval report(rr::INF3,"Loading skybox...\n");
		parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_ENV_RELOAD));
	}


	solver->observer = &svs.eye; // solver automatically updates lights that depend on camera
	if (solver->getStaticObjects().size())
	{
		rr::RRReportInterval report(rr::INF3,"Setting illumination type...\n");
		switch (svs.renderLightIndirect)
		{
			case LI_REALTIME_FIREBALL_LDM: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL_LDM)); break;
			case LI_REALTIME_FIREBALL:     parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL    )); break;
			case LI_STATIC_LIGHTMAPS:      parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_STATIC      )); break;
		}
	}

	// init rest
	rr::RRReportInterval report(rr::INF3,"Initializing the rest...\n");
	lv = new SVLightmapViewer(svs.pathToShaders);
	if (svs.selectedLightIndex>=solver->getLights().size()) svs.selectedLightIndex = 0;
	if (svs.selectedObjectIndex>=solver->getStaticObjects().size()) svs.selectedObjectIndex = 0;
	lightFieldQuadric = gluNewQuadric();
	lightFieldObjectIllumination = new rr::RRObjectIllumination;
	lightFieldObjectIllumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGB,true,NULL);
	lightFieldObjectIllumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,NULL);
	{
		entityIcons = new SVEntityIcons(wxString::Format("%s../maps/",svs.pathToShaders),solver->getUberProgram());
		rr::RRVec3 sceneMin,sceneMax;
		rr::RRObject* object = solver->getMultiObjectCustom();
		if (object)
		{
			object->getCollider()->getMesh()->getAABB(&sceneMin,&sceneMax,&sunIconPosition);

			//iconSize = (sceneMax-sceneMin).avg()*0.04f;
			// better, insensitive to single triangle in 10km distance
			iconSize = object->getCollider()->getMesh()->getAverageVertexDistance()*0.017f;

			sunIconPosition.y = sceneMax.y + 5*iconSize;
		}
	}

#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif

	water = new Water(svs.pathToShaders,true,true);
	toneMapping = new ToneMapping(svs.pathToShaders);
	ray = rr::RRRay::create();
	collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();
	exitRequested = false;
	fullyCreated = true;
}

void SVCanvas::createContext()
{
#ifdef _MSC_VER
	// this does not solve any real problem we found, it's just precaution
	__try
	{
		createContextCore();
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Scene initialization crashed.\n"));
		exit(1);
	}
#else
	createContextCore();
#endif
}

void SVCanvas::addOrRemoveScene(rr::RRScene* scene, bool add)
{
	if (scene)
	{
		// add or remove scene from solver
		rr::RRObjects objects = solver->getStaticObjects();
		rr::RRLights lights = solver->getLights();
		if (add)
		{
			objects.insert(objects.end(),scene->objects.begin(),scene->objects.end());
			lights.insert(lights.end(),scene->lights.begin(),scene->lights.end());
			if (!solver->getEnvironment() && scene->environment)
				solver->setEnvironment(scene->environment);
		}
		else
		{
			for (unsigned i=objects.size();i--;)
				for (unsigned j=0;j<scene->objects.size();j++)
					if (objects[i]==scene->objects[j])
					{
						objects.erase(i);
						break;
					}
			for (unsigned i=lights.size();i--;)
				for (unsigned j=0;j<scene->lights.size();j++)
					if (lights[i]==scene->lights[j])
					{
						lights.erase(i);
						break;
					}
		}
		solver->setStaticObjects(objects,NULL);
		solver->setLights(lights);

		// fix svs.renderLightIndirect, setStaticObjects() just silently switched solver to architect
		// must be changed if setStaticObjects() behaviour changes
		// we don't switch to architect, but rather to const ambient, because architect ignores environment, scenes without lights are black
		if (svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)
		{
			svs.renderLightIndirect = LI_CONSTANT;
		}

		// fix dangling pointer in light properties pane
		parent->updateAllPanels();

		// fix dangling pointer in collisionHandler
		delete collisionHandler;
		collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();

		// alloc rtgi buffers, otherwise new objects would have no realtime indirect
		if (add)
			reallocateBuffersForRealtimeGI(true);
	}
}

void SVCanvas::reallocateBuffersForRealtimeGI(bool reallocateAlsoVbuffers)
{
	solver->allocateBuffersForRealtimeGI(
		reallocateAlsoVbuffers?svs.realtimeLayerNumber:-1,
		svs.raytracedCubesDiffuseRes,svs.raytracedCubesSpecularRes,RR_MAX(svs.raytracedCubesDiffuseRes,svs.raytracedCubesSpecularRes));
	parent->m_objectProperties->updateProperties();
}

SVCanvas::~SVCanvas()
{
	if (!svs.releaseResources)
	{
		// user requested fast exit without releasing resources
		// however, if scene is .gsa, we must ignore him and release resources (it shutdowns gamebryo)
		bool sceneMustBeReleased = _stricmp(bf::path(WX2PATH(svs.sceneFilename)).extension().string().c_str(),".gsa")==0;
		if (!sceneMustBeReleased)
		{
			// save time and don't release scene
			return;
		}
	}

	// fps
	RR_SAFE_DELETE(fpsDisplay);
	textureRenderer = NULL;

	// logo
	RR_SAFE_DELETE(logoImage);

	// vignette
	RR_SAFE_DELETE(vignetteImage);

	// lens flare
	RR_SAFE_DELETE(lensFlare);

	// bloom
	RR_SAFE_DELETE(bloom);

	// help
	RR_SAFE_DELETE(helpImage);

	RR_SAFE_DELETE(entityIcons);
	RR_SAFE_DELETE(collisionHandler);
	RR_SAFE_DELETE(ray);
	RR_SAFE_DELETE(toneMapping);
	RR_SAFE_DELETE(water);

	deleteAllTextures();
	// delete objects referenced by solver
	if (solver)
	{
		// delete all lightmaps for realtime rendering
		for (unsigned i=0;i<solver->getStaticObjects().size();i++)
		{
			RR_SAFE_DELETE(solver->getStaticObjects()[i]->illumination.getLayer(svs.realtimeLayerNumber));
		}

		// delete env manually loaded by user
		if (envToBeDeletedOnExit)
		{
			delete solver->getEnvironment();
		}

		// delete scaler created for scene loaded from disk
		if (!svs.initialInputSolver)
		{
			delete solver->getScaler();
		}
	}
	RR_SAFE_DELETE(solver);
	for (unsigned i=0;i<mergedScenes.size();i++) delete mergedScenes[i];
	RR_SAFE_DELETE(lv);
	RR_SAFE_DELETE(lightField);
	RR_SAFE_DELETE(lightFieldObjectIllumination);
	for (unsigned i=0;i<lightsToBeDeletedOnExit.size();i++) delete lightsToBeDeletedOnExit[i];
	lightsToBeDeletedOnExit.clear();
	gluDeleteQuadric(lightFieldQuadric);
	lightFieldQuadric = NULL;
	delete context;
}

void SVCanvas::OnSize(wxSizeEvent& event)
{
	// set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);
	if (context)
	{
		SetCurrent(*context);
		winWidth = w;
		winHeight = h;
		// with Enhanced screenshot checked, viewport maintains the same aspect as screenshot
		if (parent->userPreferences.sshotEnhanced)
		{
			if (w*parent->userPreferences.sshotEnhancedHeight > h*parent->userPreferences.sshotEnhancedWidth)
				winWidth = h*parent->userPreferences.sshotEnhancedWidth/parent->userPreferences.sshotEnhancedHeight;
			else
				winHeight = w*parent->userPreferences.sshotEnhancedHeight/parent->userPreferences.sshotEnhancedWidth;
		}
		glViewport((w-winWidth)/2,(h-winHeight)/2,winWidth,winHeight);
	}
}

void SVCanvas::OnKeyDown(wxKeyEvent& event)
{
	if (exitRequested || !fullyCreated)
		return;

	bool needsRefresh = false;
	long evkey = event.GetKeyCode();
	float speed = (event.GetModifiers()==wxMOD_SHIFT) ? 3 : 1;
	if (event.GetModifiers()==wxMOD_CONTROL) switch (evkey)
	{
		case 'T': // ctrl-t
			svs.renderMaterialTextures = !svs.renderMaterialTextures;
			break;
		case 'W': // ctrl-w
			svs.renderWireframe = !svs.renderWireframe;
			break;
		case 'H': // ctrl-h
			svs.renderHelpers = !svs.renderHelpers;
			break;
		case 'F': // ctrl-f
			svs.renderFPS = !svs.renderFPS;
			break;
	}
	else if (event.GetModifiers()==wxMOD_ALT) switch (evkey)
	{
		case 'S': // alt-s
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHT_SPOT));
			break;
		case 'O': // alt-o
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHT_POINT));
			break;
		case 'F': // alt-f
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHT_FLASH));
			break;
		case '1':
		case '2':
		case '3':
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_WINDOW_LAYOUT1+evkey-'1'));
			break;
	}
	else switch(evkey)
	{
		case ' ':
			// pause/play videos
			if (solver)
			{
				rr::RRVector<rr::RRBuffer*> buffers;
				solver->getAllBuffers(buffers,NULL);
				svs.playVideos = !svs.playVideos;
				for (unsigned i=0;i<buffers.size();i++)
					if (svs.playVideos)
						buffers[i]->play();
					else
						buffers[i]->pause();
			}
			break;

		case WXK_F8: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_FILE_SAVE_SCREENSHOT)); break;
		case WXK_F11: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_WINDOW_FULLSCREEN_META)); break;
		case WXK_NUMPAD_ADD:
		case '+': svs.tonemappingBrightness *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_SUBTRACT:
		case '-': svs.tonemappingBrightness /= 1.2f; needsRefresh = true; break;

		//case '8': if (event.GetModifiers()==0) break; // ignore '8', but accept '8' + shift as '*', continue to next case
		case WXK_NUMPAD_MULTIPLY:
		case '*': svs.tonemappingGamma *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_DIVIDE:
		case '/': svs.tonemappingGamma /= 1.2f; needsRefresh = true; break;

		case WXK_LEFT:
		case 'a':
		case 'A': speedLeft = speed; break;

		case WXK_DOWN:
		case 's':
		case 'S': speedBack = speed; break;

		case WXK_RIGHT:
		case 'd':
		case 'D': speedRight = speed; break;

		case WXK_UP:
		case 'w':
		case 'W': speedForward = speed; break;

		case WXK_PAGEUP:
		case 'q':
		case 'Q': speedUp = speed; break;

		case WXK_PAGEDOWN:
		case 'z':
		case 'Z': speedDown = speed; break;

		case 'x':
		case 'X': speedLean = -speed; break;

		case 'c':
		case 'C': speedLean = +speed; break;

		case 'h':
		case 'H': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_HELP)); break;

		case WXK_DELETE:
			if (selectedType==ST_LIGHT)
				parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHT_DELETE));
			break;

		case 'L': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_VIEW_LEFT)); break;
		case 'R': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_VIEW_RIGHT)); break;
		case 'F': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_VIEW_FRONT)); break;
		case 'B': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_VIEW_BACK)); break;
		case 'T': parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_VIEW_TOP)); break;


		case 27:
			if (svs.renderLightmaps2d)
			{
				svs.renderLightmaps2d = 0;
			}
			else
			if (svs.renderHelp)
			{
				svs.renderHelp = false;
			}
			else
			if (svs.fullscreen)
			{
				parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_WINDOW_FULLSCREEN_META));
			}
			else
			{
				exitRequested = true;
			}
			break;
//		default:
//			GetParent()->GetEventHandler()->ProcessEvent(event);
//			return;
	}

	if (solver)
		solver->reportInteraction();
	if (needsRefresh)
	{
		Refresh(false);
	}
}

void SVCanvas::OnKeyUp(wxKeyEvent& event)
{
	if (exitRequested || !fullyCreated)
		return;

	long evkey = event.GetKeyCode();
	switch(evkey)
	{
		case WXK_LEFT:
		case 'a':
		case 'A': speedLeft = 0; break;

		case WXK_DOWN:
		case 's':
		case 'S': speedBack = 0; break;

		case WXK_RIGHT:
		case 'd':
		case 'D': speedRight = 0; break;

		case WXK_UP:
		case 'w':
		case 'W': speedForward = 0; break;

		case WXK_PAGEUP:
		case 'q':
		case 'Q': speedUp = 0; break;

		case WXK_PAGEDOWN:
		case 'z':
		case 'Z': speedDown = 0; break;

		case 'x':
		case 'X': speedLean = 0; break;

		case 'c':
		case 'C': speedLean = 0; break;
	}
	event.Skip();
}

// Filled on click, original data acquired at click time.
struct ClickInfo
{
	TIME time;
	int mouseX;
	int mouseY;
	bool mouseLeft;
	bool mouseRight;
	bool mouseMiddle;
	float angle;
	float angleX;
	rr::RRVec3 pos;
	rr::RRVec3 rayOrigin;
	rr::RRVec3 rayDirection;
	unsigned hitTriangle;
	float hitDistance;
	rr::RRVec2 hitPoint2d;
	rr::RRVec3 hitPoint3d;
	EntityId clickedEntity;
};
// set by OnMouseEvent on click (but not set on click that closes menu), valid and constant during drag
// read also by OnPaint (paints crosshair)
static bool s_ciRelevant = false; // are we dragging and is s_ci describing click that started it?
static ClickInfo s_ci;

void SVCanvas::OnMouseEvent(wxMouseEvent& event)
{
	if (exitRequested || !fullyCreated)
		return;

	if (event.IsButton())
	{
		// regain focus, innocent actions like clicking menu take it away
		SetFocus();
	}
	{
		wxPoint pixelPos = event.GetPosition();
		wxSize pixelSize = GetSize();
		mousePositionInWindow = rr::RRVec2(pixelPos.x*2.0f/pixelSize.x-1,pixelPos.y*2.0f/pixelSize.y-1);
	}
	if (!winWidth || !winHeight || !solver)
	{
		return;
	}
	if (svs.renderLightmaps2d && lv)
	{
		lv->OnMouseEvent(event,GetSize());
		return;
	}

	// set on every frame of dragging
	static int s_prevX;
	static int s_prevY;

	// scene clicked: fill s_xxx
	if (event.ButtonDown())
	{
		// init static variables when dragging starts
		s_ciRelevant = true;
		s_prevX = s_ci.mouseX = event.GetX();
		s_prevY = s_ci.mouseY = event.GetY();
		s_ci.time = GETTIME;
		s_ci.mouseLeft = event.LeftIsDown();
		s_ci.mouseMiddle = event.MiddleIsDown();
		s_ci.mouseRight = event.RightIsDown();
		Camera* cam = (event.LeftIsDown()
			 && selectedType==ST_LIGHT && svs.selectedLightIndex<solver->getLights().size()) ? solver->realtimeLights[svs.selectedLightIndex]->getParent() : &svs.eye;
		s_ci.angle = cam->angle; // when rotating light, init with light angles rather than camera angles
		s_ci.angleX = cam->angleX;
		s_ci.pos = svs.eye.pos;
		s_ci.rayOrigin = svs.eye.getRayOrigin(mousePositionInWindow);
		s_ci.rayDirection = svs.eye.getRayDirection(mousePositionInWindow);

		// find scene distance, adjust search range to look only for closer icons
		s_ci.hitDistance = svs.eye.getNear()*0.9f+svs.eye.getFar()*0.1f;
		ray->rayOrigin = s_ci.rayOrigin;
		rr::RRVec3 directionToMouse = s_ci.rayDirection;
		float directionToMouseLength = directionToMouse.length();
		ray->rayDirInv = rr::RRVec3(directionToMouseLength)/directionToMouse;
		ray->rayLengthMin = svs.eye.getNear()*directionToMouseLength;
		ray->rayLengthMax = svs.eye.getFar()*directionToMouseLength;
		ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_TRIANGLE|rr::RRRay::FILL_POINT2D;
		ray->collisionHandler = collisionHandler;
		if (solver->getMultiObjectCustom() && solver->getMultiObjectCustom()->getCollider()->intersect(ray))
		{
			// in next step, look only for closer lights
			ray->rayLengthMax = ray->hitDistance;
			s_ci.hitTriangle = ray->hitTriangle;
			s_ci.hitDistance = ray->hitDistance;
			s_ci.hitPoint2d = ray->hitPoint2d;
			s_ci.hitPoint3d = ray->hitPoint3d;
		}
		else
		{
			s_ci.hitTriangle = UINT_MAX;
			s_ci.hitDistance = 0;
			s_ci.hitPoint2d = rr::RRVec2(0);
			s_ci.hitPoint3d = rr::RRVec3(0);
		}

		// find icon distance
		SVEntities entities;
		if (parent->m_lightProperties->IsShown())
			entities.addLights(solver->getLights(),sunIconPosition);
		if (entityIcons->intersectIcons(entities,ray,iconSize))
		{
			s_ci.clickedEntity = EntityId(entities[ray->hitTriangle].type,entities[ray->hitTriangle].index);
			s_ci.hitDistance = ray->hitDistance;
			s_ci.hitPoint2d = rr::RRVec2(0);
			s_ci.hitPoint3d = ray->rayOrigin + rr::RRVec3(1)/ray->rayDirInv*ray->hitDistance;
		}
		else
		{
			s_ci.clickedEntity = EntityId(ST_CAMERA,0);
		}
	}

	// handle clicking (mouse released in less than 0.2s in less than 20pix distance)
	if (event.LeftUp() && (GETTIME-s_ci.time)<0.2f*PER_SEC && abs(event.GetX()-s_ci.mouseX)<20 && abs(event.GetY()-s_ci.mouseY)<20)
	{
		// selection
		if (s_ci.clickedEntity.type!=ST_CAMERA)
		{
			// clicked icon
			parent->selectEntityInTreeAndUpdatePanel(s_ci.clickedEntity,event.LeftDClick()?SEA_ACTION:SEA_ACTION_IF_ALREADY_SELECTED);
		}
		else
		{
			// clicked scene
			selectedType = ST_CAMERA;
			rr::RRMesh::PreImportNumber selectedPreImportTriangle(0,0);
			rr::RRObject* selectedObject = NULL;
			if (s_ci.hitTriangle!=UINT_MAX)
			{
				selectedPreImportTriangle = solver->getMultiObjectCustom()->getCollider()->getMesh()->getPreImportTriangle(s_ci.hitTriangle);
				selectedObject = solver->getStaticObjects()[selectedPreImportTriangle.object];
			}		
			parent->m_objectProperties->setObject(selectedObject,svs.precision);
			parent->m_materialProperties->setMaterial(solver,s_ci.hitTriangle,s_ci.hitPoint2d);
		}
	}

	// handle double clicking
	if (event.LeftDClick() && s_ci.hitTriangle==UINT_MAX)
	{
		parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_WINDOW_FULLSCREEN_META));
	}

	// handle dragging
	if (event.Dragging() && s_ciRelevant && (event.GetX()!=s_prevX || event.GetY()!=s_prevY))
	{
		float dragX = (event.GetX()-s_ci.mouseX)/(float)winWidth;
		float dragY = (event.GetY()-s_ci.mouseY)/(float)winHeight;
		s_prevX = event.GetX();
		s_prevY = event.GetY();

		if (event.LeftIsDown())
		{
			// looking
			//  rotate around [eye|light].pos
			if (selectedType==ST_LIGHT)
			{
				if (svs.selectedLightIndex<solver->getLights().size())
				{
					solver->reportDirectIlluminationChange(svs.selectedLightIndex,true,true);
					Camera* light = solver->realtimeLights[svs.selectedLightIndex]->getParent();
					light->angle = s_ci.angle-5*dragX;
					light->angleX = s_ci.angleX-5*dragY;
					RR_CLAMP(light->angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
					// changes position a bit, together with rotation
					// if we don't call it, solver updates light in a standard way, without position change
					light->pos += light->dir*0.3f;
					light->update();
					light->pos -= light->dir*0.3f;
				}
			}
			else
			{
				svs.eye.angle = s_ci.angle-dragX*5*svs.eye.getFieldOfViewHorizontalDeg()/90;
				svs.eye.angleX = s_ci.angleX-dragY*5*svs.eye.getFieldOfViewVerticalDeg()/90;
				RR_CLAMP(svs.eye.angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			}
			solver->reportInteraction();
		}
		if (event.MiddleIsDown())
		{
			// panning
			//  drag clicked pixel so that it stays under mouse
			rr::RRVec3 pan;
			if (svs.eye.orthogonal)
			{
				rr::RRVec2 origMousePositionInWindow = rr::RRVec2(s_ci.mouseX*2.0f/winWidth-1,s_ci.mouseY*2.0f/winHeight-1);
				pan = svs.eye.getRayOrigin(origMousePositionInWindow)-svs.eye.getRayOrigin(mousePositionInWindow);
			}
			else
			{
				rr::RRVec3 newRayDirection = svs.eye.getRayDirection(mousePositionInWindow);
				pan = (s_ci.rayDirection-newRayDirection)*(s_ci.hitDistance/s_ci.rayDirection.length());
			}
			svs.eye.pos = s_ci.pos + pan;
			solver->reportInteraction();
		}
		if (event.RightIsDown())
		{
			// inspection
			//  rotate around clicked point, point does not move on screen
			svs.eye.angle = s_ci.angle-dragX*8;
			svs.eye.angleX = s_ci.angleX-dragY*8;
			RR_CLAMP(svs.eye.angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			svs.eye.update();
			rr::RRVec2 origMousePositionInWindow = rr::RRVec2(s_ci.mouseX*2.0f/winWidth-1,s_ci.mouseY*2.0f/winHeight-1);
			rr::RRVec3 newRayDirection = svs.eye.getRayDirection(origMousePositionInWindow);
			svs.eye.pos = s_ci.pos + (s_ci.rayDirection-newRayDirection)*(s_ci.hitDistance/s_ci.rayDirection.length());
			solver->reportInteraction();
		}

	}
	if (!event.ButtonDown() && !event.Dragging())
	{
		// dragging ended, all s_xxx become invalid
		s_ciRelevant = false;
	}

	// handle wheel
	if (event.GetWheelRotation())
	{
		if (event.ControlDown())
		{
			// move forward/backward
			svs.eye.pos -= svs.eye.dir * (event.GetWheelRotation()*svs.cameraMetersPerSecond/event.GetWheelDelta()/3);
		}
		else
		{
			// zoom
			if (svs.eye.orthogonal)
			{
				if (event.GetWheelRotation()<0)
					svs.eye.orthoSize /= 1.4f;
				if (event.GetWheelRotation()>0)
					svs.eye.orthoSize *= 1.4f;
			}
			else
			{
				float fov = svs.eye.getFieldOfViewVerticalDeg();
				if (event.GetWheelRotation()<0)
				{
					if (fov>13) fov -= 10; else fov /= 1.4f;
				}
				if (event.GetWheelRotation()>0)
				{
					if (fov*1.4f<=3) fov *= 1.4f; else if (fov<170) fov += 10;
				}
				svs.eye.setFieldOfViewVerticalDeg(fov);
			}
		}
	}

	if (svs.selectedLightIndex<solver->realtimeLights.size())
	{
		solver->realtimeLights[svs.selectedLightIndex]->updateAfterRealtimeLightChanges();
	}
	solver->reportInteraction();
}


void SVCanvas::OnIdle(wxIdleEvent& event)
{
	if ((svs.initialInputSolver && svs.initialInputSolver->aborting) || (fullyCreated && !solver) || (solver && solver->aborting) || exitRequested)
	{
		parent->Close(true);
		return;
	}
	if (!fullyCreated)
		return;

	// camera/light movement
	static TIME prevTime = 0;
	TIME nowTime = GETTIME;
	if (prevTime && nowTime!=prevTime)
	{
		// ray is used in this block


		// camera/light keyboard move
		float seconds = (nowTime-prevTime)/(float)PER_SEC;
		RR_CLAMP(seconds,0.001f,0.3f);
		float meters = seconds * svs.cameraMetersPerSecond;
		Camera* cam = (selectedType!=ST_LIGHT)?&svs.eye:solver->realtimeLights[svs.selectedLightIndex]->getParent();

		{
			// yes -> respond to keyboard
			if (speedForward) cam->pos += cam->dir * (speedForward*meters);
			if (speedBack) cam->pos -= cam->dir * (speedBack*meters);
			if (speedRight) cam->pos += cam->right * (speedRight*meters);
			if (speedLeft) cam->pos -= cam->right * (speedLeft*meters);
			if (speedUp) cam->pos += cam->up * (speedUp*meters);
			if (speedDown) cam->pos -= cam->up * (speedDown*meters);
			if (speedLean) cam->leanAngle += speedLean*seconds*0.5f;
		}

		// light change report
		if (speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
		{
			if (cam!=&svs.eye) 
			{
				solver->realtimeLights[svs.selectedLightIndex]->updateAfterRealtimeLightChanges();
				solver->reportDirectIlluminationChange(svs.selectedLightIndex,true,true);
			}
		}
	}
	prevTime = nowTime;

	Refresh(false);
}

void SVCanvas::OnEnterWindow(wxMouseEvent& event)
{
	SetFocus();
}

static void textOutput(int x, int y, int h, const char *format, ...)
{
#ifdef _WIN32
	if (y>=h) return;
	char text[1000];
	va_list argptr;
	va_start (argptr,format);
	_vsnprintf (text,999,format,argptr);
	text[999] = 0;
	va_end (argptr);
	glRasterPos2i(x,y);
	glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
#endif
}

static void textOutputMaterialProperty(int x, int y, int h, const char* name, const rr::RRMaterial::Property& triangle, const rr::RRMaterial::Property& point, const rr::RRRay* ray, const rr::RRMesh* multiMesh)
{
	if (&triangle==&point || !triangle.texture)
	{
		// triangle equals point, no need to print it
		textOutput(x,y,h,"%s tri=%f %f %f",name,triangle.color[0],triangle.color[1],triangle.color[2]);
	}
	else
	{
		// point differs from triangle, print it
		rr::RRMesh::TriangleMapping triangleMapping;
		multiMesh->getTriangleMapping(ray->hitTriangle,triangleMapping,triangle.texcoord);
		rr::RRVec2 uvInMap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray->hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray->hitPoint2d[1];
		textOutput(x,y,h,"%s tri=%f %f %f point=%f %f %f uv[%d]=%f %f",name,
			triangle.color[0],triangle.color[1],triangle.color[2],
			point.color[0],point.color[1],point.color[2],
			triangle.texcoord,uvInMap[0],uvInMap[1]
		);
	}
}

static void drawTangentBasis(rr::RRVec3 point, rr::RRMesh::TangentBasis basis)
{
	glBegin(GL_LINES);
	glColor3f(1,0,0);
	glVertex3fv(&point[0]);
	glVertex3f(point[0]+basis.normal[0],point[1]+basis.normal[1],point[2]+basis.normal[2]);
	glColor3f(0,1,0);
	glVertex3fv(&point[0]);
	glVertex3f(point[0]+basis.tangent[0],point[1]+basis.tangent[1],point[2]+basis.tangent[2]);
	glColor3f(0,0,1);
	glVertex3fv(&point[0]);
	glVertex3f(point[0]+basis.bitangent[0],point[1]+basis.bitangent[1],point[2]+basis.bitangent[2]);
	glEnd();
}

static void drawTriangle(rr::RRMesh::TriangleBody body)
{
	glBegin(GL_LINE_LOOP);
	glColor3f(1,1,1);
	glVertex3fv(&body.vertex0[0]);
	glVertex3f(body.vertex0[0]+body.side1[0],body.vertex0[1]+body.side1[1],body.vertex0[2]+body.side1[2]);
	glVertex3f(body.vertex0[0]+body.side2[0],body.vertex0[1]+body.side2[1],body.vertex0[2]+body.side2[2]);
	glEnd();
}

void SVCanvas::OnPaintCore(wxPaintEvent& event)
{
	if (exitRequested || !fullyCreated || !winWidth || !winHeight)
		return;

	wxPaintDC dc(this);

	if (!context) return;
	SetCurrent(*context);

	parent->AfterPaneOpenClose();

#ifdef _WIN32
	// init font for text outputs
	if (!fontInited)
	{
		fontInited = true;
		HFONT font = CreateFont(-15,0,0,0,FW_THIN,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_DONTCARE|FIXED_PITCH,_T("System"));
		SelectObject(wglGetCurrentDC(), font);
		wglUseFontBitmaps(wglGetCurrentDC(),0,127,1000);
		glListBase(1000);
	}
#endif

	Paint(event);

	// done
	SwapBuffers();
}


void SVCanvas::Paint(wxPaintEvent& _event)
{
	rr::RRReportInterval report(rr::INF3,"display...\n");
	if (renderEmptyFrames)
	{
		glClearColor(0.31f,0.31f,0.31f,0);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0,0,0,0);
		SwapBuffers();
		return;
	}
	if (svs.renderLightmaps2d && lv)
	{
		if (svs.selectedObjectIndex<solver->getStaticObjects().size())
			lv->setObject(
				solver->getStaticObjects()[svs.selectedObjectIndex]->illumination.getLayer((svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)?svs.ldmLayerNumber:svs.staticLayerNumber),
				solver->getStaticObjects()[svs.selectedObjectIndex],
				svs.renderLightmapsBilinear);
		else
			lv->setObject(
				NULL,
				NULL,
				svs.renderLightmapsBilinear);
		lv->OnPaint(_event,GetSize());
	}
	else
	{
		if (exitRequested || !winWidth || !winHeight) return; // can't display without window

		// blend skyboxes
		if (timeWhenSkyboxBlendingStarted)
		{
			float blend = (float)(GETSEC-timeWhenSkyboxBlendingStarted)/3;
			// blending adds small CPU+GPU overhead, so don't blend if not necessary
			// blend only if 3sec period running && second texture is present && differs from first one
			if (blend>=0 && blend<=1 && solver->getEnvironment(1) && solver->getEnvironment(0)!=solver->getEnvironment(1))
			{
				// blend
				solver->setEnvironmentBlendFactor(1-blend);
			}
			else
			{
				// stop blending
				solver->setEnvironmentBlendFactor(0);
				timeWhenSkyboxBlendingStarted = 0;
			}
		}


		// aspect needs update after
		// - OnSize()
		// - rendering enhanced screenshot
		// - RL: restored previously saved camera and window size differs
		// - RL: calculated camera from previously saved keyframes and window size differs
		svs.eye.setAspect(winWidth/(float)winHeight,0.5f);
		svs.eye.update();

		// move flashlight
		for (unsigned i=solver->getLights().size();i--;)
			if (solver->getLights()[i] && solver->getLights()[i]->enabled && solver->getLights()[i]->type==rr::RRLight::SPOT && solver->getLights()[i]->name=="Flashlight")
			{
				// eye must already be updated otherwise flashlight will lag one frame
				solver->getLights()[i]->position = svs.eye.pos + svs.eye.up*svs.cameraMetersPerSecond*0.03f+svs.eye.right*svs.cameraMetersPerSecond*0.03f;
				solver->getLights()[i]->direction = svs.eye.dir;
				float viewportWidthCovered = 0.9f;
				solver->getLights()[i]->outerAngleRad = svs.eye.getFieldOfViewHorizontalRad()*viewportWidthCovered*0.6f;
				solver->getLights()[i]->fallOffAngleRad = svs.eye.getFieldOfViewHorizontalRad()*viewportWidthCovered*0.4f;
				solver->realtimeLights[i]->getParent()->setRange(svs.eye.getNear(),svs.eye.getFar());
				solver->realtimeLights[i]->updateAfterRRLightChanges();
			}

		if (svs.cameraDynamicNear && !svs.eye.orthogonal) // don't change range in ortho, fixed range from setView() is better
		{
			// eye must already be updated here because next line depends on up, right
			svs.eye.setRangeDynamically(solver->getMultiObjectCustom(),svs.renderWater,svs.waterLevel);
			svs.eye.update();
		}

		if (svs.renderLightDirect==LD_REALTIME || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			for (unsigned i=0;i<solver->realtimeLights.size();i++)
				solver->realtimeLights[i]->shadowTransparencyRequested = svs.shadowTransparency;
			rr::RRDynamicSolver::CalculateParameters params;
			if (svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
			{
				// rendering indirect -> calculate will update shadowmaps, possibly resample environment and emissive maps, improve indirect
				params.materialEmittanceMultiplier = svs.emissiveMultiplier;
				params.materialEmittanceStaticQuality = 17;
				params.materialEmittanceVideoQuality = svs.videoEmittanceAffectsGI?svs.videoEmittanceGIQuality+1:0;
				params.materialTransmittanceStaticQuality = 0; // don't check static textures in each frame, we manually reportMaterialChange() when they change
				params.materialTransmittanceVideoQuality = svs.videoTransmittanceAffectsGI?(svs.videoTransmittanceAffectsGIFull?2:1):0;
				params.environmentStaticQuality = 6000;
				params.environmentVideoQuality = svs.videoEnvironmentAffectsGI?svs.videoEnvironmentGIQuality+1:0;
				params.qualityIndirectDynamic = 3;
				params.qualityIndirectStatic = 10000;
			}
			else
			{
				// rendering direct only -> calculate will update shadowmaps only
				params.materialEmittanceStaticQuality = 0;
				params.materialEmittanceVideoQuality = 0;
				params.materialTransmittanceStaticQuality = 0;
				params.materialTransmittanceVideoQuality = svs.videoTransmittanceAffectsGI?1:0; // only shadows
				params.environmentStaticQuality = 0;
				params.environmentVideoQuality = 0;
				params.qualityIndirectDynamic = 0;
				params.qualityIndirectStatic = 0;
			}
			solver->calculate(&params);
		}

		{
			rr::RRReportInterval report(rr::INF3,"render scene...\n");
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			svs.eye.setupForRender();

			rr::RRVec4 brightness = svs.renderTonemapping ? svs.tonemappingBrightness * pow(svs.tonemappingGamma,0.45f) : rr::RRVec4(1);
			float gamma = svs.renderTonemapping ?svs.tonemappingGamma : 1;

			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_COLOR = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_MAP = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_INDIRECT_CONST = svs.renderLightIndirect==LI_CONSTANT;
			uberProgramSetup.LIGHT_INDIRECT_auto = svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE =
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = svs.raytracedCubesEnabled && solver->getStaticObjects().size()<svs.raytracedCubesMaxObjects && svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.MATERIAL_DIFFUSE_CONST = svs.renderMaterialDiffuse;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = svs.renderMaterialDiffuse && svs.renderMaterialTextures;
			uberProgramSetup.MATERIAL_SPECULAR = svs.renderMaterialSpecular;
			uberProgramSetup.MATERIAL_SPECULAR_CONST = svs.renderMaterialSpecular;
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = svs.renderMaterialEmission;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = svs.renderMaterialEmission && svs.renderMaterialTextures;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = svs.renderMaterialTransparency!=T_OPAQUE;
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = svs.renderMaterialTransparency!=T_OPAQUE && svs.renderMaterialTextures;
			uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = svs.renderMaterialTransparency!=T_OPAQUE;
			uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = svs.renderMaterialTransparency==T_ALPHA_BLEND || svs.renderMaterialTransparency==T_RGB_BLEND;
			uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = svs.renderMaterialTransparency==T_RGB_BLEND;
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = brightness!=rr::RRVec4(1);
			uberProgramSetup.POSTPROCESS_GAMMA = gamma!=1;
			float clipPlanes[6] = {0,0,0,0,0,0};
			if (svs.renderWireframe) {glClear(GL_COLOR_BUFFER_BIT); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);}
			if (svs.renderWater && water && !svs.renderWireframe)
			{
				if (uberProgramSetup.CLIP_PLANE_YB)
					clipPlanes[3] = RR_MAX(clipPlanes[3],svs.waterLevel);
				else
				{
					clipPlanes[3] = svs.waterLevel;
					uberProgramSetup.CLIP_PLANE_YB = true;
				}
				water->updateReflectionInit(winWidth/2,winHeight/2,&svs.eye,svs.waterLevel,svs.srgbCorrect);
				glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
				solver->renderScene(
					uberProgramSetup,
					NULL,
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					(svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)?svs.ldmLayerNumber:UINT_MAX,
					clipPlanes,
					svs.srgbCorrect,
					&brightness,
					gamma);
				water->updateReflectionDone();
				float oldFar = svs.eye.getFar();
				svs.eye.setFar(oldFar*5); // far is set to end right behind scene. water polygon continues behind scene, we need it visible -> increase far
				svs.eye.update();
				svs.eye.setupForRender();
				
				// find sun
				for (unsigned i=0;i<solver->getLights().size();i++)
				{
					const rr::RRLight* light = solver->getLights()[i];
					if (light->type==rr::RRLight::DIRECTIONAL)
					{
						water->render(svs.eye.getFar()*2,svs.eye.pos,rr::RRVec4(svs.waterColor,0.5f),light->direction,light->color);
						goto rendered;
					}
				}
				water->render(svs.eye.getFar()*4,svs.eye.pos,rr::RRVec4(svs.waterColor,0.5f),rr::RRVec3(0),rr::RRVec3(0)); // far*4 makes triangle end before far only in unusually wide aspects, error is nearly invisible. higher constant would increase float errors in shader
rendered:
				solver->renderScene(
					uberProgramSetup,
					NULL,
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					(svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)?svs.ldmLayerNumber:UINT_MAX,
					clipPlanes,
					svs.srgbCorrect,
					&brightness,
					gamma);
				svs.eye.setFar(oldFar);
			}
			else
			{
				solver->renderScene(
					uberProgramSetup,
					NULL,
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					(svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)?svs.ldmLayerNumber:UINT_MAX,
					clipPlanes,
					svs.srgbCorrect,
					&brightness,
					gamma);
			}
			if (svs.renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			if (svs.renderTonemapping && svs.tonemappingAutomatic
				&& !svs.renderWireframe
				&& ((svs.renderLightIndirect==LI_STATIC_LIGHTMAPS && solver->containsLightSource())
					|| ((svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT) && solver->containsRealtimeGILightSource())
					|| svs.renderLightIndirect==LI_CONSTANT
					))
			{
				static TIME oldTime = 0;
				TIME newTime = GETTIME;
				float secondsSinceLastFrame = (newTime-oldTime)/float(PER_SEC);
				if (secondsSinceLastFrame>0 && secondsSinceLastFrame<10 && oldTime)
				{
					toneMapping->adjustOperator(textureRenderer,secondsSinceLastFrame*svs.tonemappingAutomaticSpeed,svs.tonemappingBrightness,svs.tonemappingGamma,svs.tonemappingAutomaticTarget);
				}
				oldTime = newTime;
			}
		}

		// render bloom, using own shader
		if (svs.renderBloom)
		{
			if (!bloomLoadAttempted)
			{
				bloomLoadAttempted = true;
				RR_ASSERT(!bloom);
				bloom = new Bloom(svs.pathToShaders);
			}
			if (bloom)
			{
				bloom->applyBloom(winWidth,winHeight);
			}
		}

		// render lens flare, using own shader
		if (svs.renderLensFlare && !svs.eye.orthogonal)
		{
			if (!lensFlareLoadAttempted)
			{
				lensFlareLoadAttempted = true;
				RR_ASSERT(!lensFlare);
				lensFlare = new LensFlare(svs.pathToShaders);
			}
			if (lensFlare)
			{
				lensFlare->renderLensFlares(svs.lensFlareSize,svs.lensFlareId,textureRenderer,svs.eye,solver->getLights(),solver->getMultiObjectCustom(),64);
			}
		}


		// render light field, using own shader
		if (svs.renderHelpers && lightField)
		{
			// update cube
			lightFieldObjectIllumination->envMapWorldCenter = rr::RRVec3(svs.eye.pos[0]+svs.eye.dir[0],svs.eye.pos[1]+svs.eye.dir[1],svs.eye.pos[2]+svs.eye.dir[2]);
			rr::RRVec2 sphereShift = rr::RRVec2(svs.eye.dir[2],-svs.eye.dir[0]).normalized()*0.05f;
			lightField->updateEnvironmentMap(lightFieldObjectIllumination,0);

			// diffuse
			// set shader (no direct light)
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true;
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = svs.tonemappingBrightness!=rr::RRVec4(1);
			uberProgramSetup.POSTPROCESS_GAMMA = svs.tonemappingGamma!=1;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.tonemappingBrightness,svs.tonemappingGamma,NULL);
			uberProgramSetup.useIlluminationEnvMaps(program,lightFieldObjectIllumination);
			// render
			glPushMatrix();
			glTranslatef(lightFieldObjectIllumination->envMapWorldCenter[0]-sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]-sphereShift[1]);
			gluSphere(lightFieldQuadric, 0.05f, 16, 16);
			glPopMatrix();

			// specular
			// set shader (no direct light)
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
			uberProgramSetup.MATERIAL_DIFFUSE = false;
			uberProgramSetup.MATERIAL_SPECULAR = true;
			uberProgramSetup.OBJECT_SPACE = true;
			program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.tonemappingBrightness,svs.tonemappingGamma,NULL);
			uberProgramSetup.useIlluminationEnvMaps(program,lightFieldObjectIllumination);
			// render
			float worldMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, lightFieldObjectIllumination->envMapWorldCenter[0]+sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]+sphereShift[1],1};
			program->sendUniform("worldMatrix",worldMatrix,false,4);
			gluSphere(lightFieldQuadric, 0.05f, 16, 16);
		}

		// render vignette, using own shader (must go before video capture)
		if (svs.renderVignette)
		{
			if (!vignetteLoadAttempted)
			{
				vignetteLoadAttempted = true;
				RR_ASSERT(!vignetteImage);
				vignetteImage = rr::RRBuffer::load(wxString::Format("%s../maps/sv_vignette.png",svs.pathToShaders));
			}
			if (vignetteImage && textureRenderer)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				textureRenderer->render2D(getTexture(vignetteImage,false,false),NULL,0,0,1,1);
				glDisable(GL_BLEND);
			}
		}

		// render logo, using own shader (must go before video capture)
		if (textureRenderer)
		{
			if (svs.renderLogo)
			{
				if (!logoLoadAttempted)
				{
					logoLoadAttempted = true;
					RR_ASSERT(!logoImage);
					logoImage = rr::RRBuffer::load(wxString::Format("%s../maps/sv_logo.png",svs.pathToShaders));
				}
				if (logoImage)
				{
					float w = logoImage->getWidth()/(float)winWidth;
					float h = logoImage->getHeight()/(float)winHeight;
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					textureRenderer->render2D(getTexture(logoImage,false,false),NULL,1-w,1-h,w,h);
					glDisable(GL_BLEND);
				}
			}
		}


		bool renderCrosshair = s_ciRelevant && (s_ci.mouseMiddle || s_ci.mouseRight);
		if (svs.renderGrid || renderCrosshair)
		{
			// set line shader
			{
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,NULL);
			}

			// render crosshair, using previously set shader
			if (renderCrosshair)
			{
				rr::RRVec3 pos = s_ci.hitPoint3d;
				float size = s_ci.hitDistance/30;
				glColor3f(1,0,0);
				glLineWidth(3); // this makes crosshair disappear on Radeon HD2400 10.02 with grid or helpers, blink without grid and helpers
				glBegin(GL_LINES);
				glVertex3f(pos.x-size,pos.y,pos.z);
				glVertex3f(pos.x+size,pos.y,pos.z);
				glVertex3f(pos.x,pos.y-size,pos.z);
				glVertex3f(pos.x,pos.y+size,pos.z);
				glVertex3f(pos.x,pos.y,pos.z-size);
				glVertex3f(pos.x,pos.y,pos.z+size);
				glEnd();
				glLineWidth(1);
			}

			// render grid, using previously set shader
			if (svs.renderGrid)
			{
				glBegin(GL_LINES);
				for (unsigned i=0;i<RR_MIN(svs.gridNumSegments+1,10001);i++)
				{
					float SIZE = svs.gridNumSegments*svs.gridSegmentSize;
					if (i==svs.gridNumSegments/2)
					{
						glColor3f(0,0,1);
						glVertex3f(0,-0.5*SIZE,0);
						glVertex3f(0,+0.5*SIZE,0);
					}
					else
						glColor3f(0,0,0.3f);
					float q = (i/float(svs.gridNumSegments)-0.5f)*SIZE;
					glVertex3f(q,0,-0.5*SIZE);
					glVertex3f(q,0,+0.5*SIZE);
					glVertex3f(-0.5*SIZE,0,q);
					glVertex3f(+0.5*SIZE,0,q);
				}
				glEnd();
			}
		}


		// render icons, using own shader (must go after video capture)
		if (entityIcons->isOk())
		{
			SVEntities entities;
			if (parent->m_lightProperties->IsShown())
				entities.addLights(solver->getLights(),sunIconPosition);
			entityIcons->renderIcons(entities,svs.eye,(selectedType==ST_LIGHT)?svs.selectedLightIndex:UINT_MAX,iconSize);
		}
	}

	if (svs.renderHelpers
		)
	{
		// set line shader
		{
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
			uberProgramSetup.MATERIAL_DIFFUSE = 1;
			uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,NULL);
		}

		// gather information about scene
		unsigned numLights = solver->getLights().size();
		bool displayPhysicalMaterials = fmod(GETSEC,8)>4;
		const rr::RRObject* multiObject = displayPhysicalMaterials ? solver->getMultiObjectPhysical() : solver->getMultiObjectCustom();
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;

		// gather information about selected object
		rr::RRObject* singleObject = (svs.selectedObjectIndex<solver->getStaticObjects().size())?solver->getStaticObjects()[svs.selectedObjectIndex]:NULL;
		const rr::RRMesh* singleMesh = singleObject ? singleObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesSingle = singleMesh ? singleMesh->getNumTriangles() : 0;

		// gather information about selected point and triangle (pointed by mouse)
		bool selectedPointValid = false; // true = all selectedXxx below are valid
		rr::RRMesh::TangentBasis selectedPointBasis;
		rr::RRMesh::TriangleBody selectedTriangleBody;
		rr::RRMesh::TriangleNormals selectedTriangleNormals;
		if (multiMesh && (!svs.renderLightmaps2d || !lv))
		{
			// ray and collisionHandler are used in this block
			rr::RRVec3 dir = svs.eye.getRayDirection(mousePositionInWindow).normalized();
			ray->rayOrigin = svs.eye.getRayOrigin(mousePositionInWindow);
			ray->rayDirInv[0] = 1/dir[0];
			ray->rayDirInv[1] = 1/dir[1];
			ray->rayDirInv[2] = 1/dir[2];
			ray->rayLengthMin = svs.eye.getNear();
			ray->rayLengthMax = svs.eye.getFar();
			ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			ray->collisionHandler = collisionHandler;
			if (solver->getMultiObjectCustom()->getCollider()->intersect(ray))
			{
				selectedPointValid = true;
				multiMesh->getTriangleBody(ray->hitTriangle,selectedTriangleBody);
				multiMesh->getTriangleNormals(ray->hitTriangle,selectedTriangleNormals);
				selectedPointBasis.normal = selectedTriangleNormals.vertex[0].normal + (selectedTriangleNormals.vertex[1].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[1];
				selectedPointBasis.tangent = selectedTriangleNormals.vertex[0].tangent + (selectedTriangleNormals.vertex[1].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[1];
				selectedPointBasis.bitangent = selectedTriangleNormals.vertex[0].bitangent + (selectedTriangleNormals.vertex[1].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[1];
			}
		}

		// render debug rays, using previously set shader
		if (svs.renderHelpers && (!svs.renderLightmaps2d || !lv) && SVRayLog::size)
		{
			glBegin(GL_LINES);
			for (unsigned i=0;i<SVRayLog::size;i++)
			{
				if (SVRayLog::log[i].unreliable)
					glColor3ub(255,0,0);
				else
				if (SVRayLog::log[i].infinite)
					glColor3ub(0,0,255);
				else
					glColor3ub(0,255,0);
				glVertex3fv(&SVRayLog::log[i].begin[0]);
				glColor3ub(0,0,0);
				//glVertex3fv(&SVRayLog::log[i].end[0]);
				glVertex3f(SVRayLog::log[i].end[0]+rand()/(100.0f*RAND_MAX),SVRayLog::log[i].end[1]+rand()/(100.0f*RAND_MAX),SVRayLog::log[i].end[2]+rand()/(100.0f*RAND_MAX));
			}
			glEnd();
		}

		// render arrows, using previously set shader
		if (svs.renderHelpers && !svs.renderLightmaps2d)
		{
			drawTangentBasis(selectedTriangleBody.vertex0,selectedTriangleNormals.vertex[0]);
			drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side1,selectedTriangleNormals.vertex[1]);
			drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side2,selectedTriangleNormals.vertex[2]);
			drawTangentBasis(ray->hitPoint3d,selectedPointBasis);
			drawTriangle(selectedTriangleBody);
		}


		// render helper text, using own shader (because text output ignores color passed to line shader)
		if (svs.renderHelpers)
		{
			centerObject = UINT_MAX; // reset pointer to texel in the center of screen, it will be set again ~100 lines below
			centerTexel = UINT_MAX;
			centerTriangle = UINT_MAX;
			glDisable(GL_DEPTH_TEST);
			PreserveMatrices p1;
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			gluOrtho2D(0, winWidth, winHeight, 0);
			{
				// set shader
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_CONST = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,NULL);
				program->sendUniform("lightIndirectConst",1.0f,1.0f,1.0f,1.0f);
			}
			int x = 10;
			int y = 10;
			int h = GetSize().y;
			unsigned numObjects = solver->getStaticObjects().size();
			{
				// what direct
				const char* strDirect = "?";
				switch (svs.renderLightDirect)
				{
					case LD_REALTIME: strDirect = "realtime"; break;
					case LD_STATIC_LIGHTMAPS: strDirect = "lightmap"; break;
					case LD_NONE: strDirect = "off"; break;
				}
				// what indirect
				const char* strIndirect = "?";
				switch (svs.renderLightIndirect)
				{
					case LI_REALTIME_FIREBALL_LDM: strIndirect = "fireball+LDM"; break;
					case LI_REALTIME_FIREBALL: strIndirect = "fireball"; break;
					case LI_REALTIME_ARCHITECT: strIndirect = "architect"; break;
					case LI_STATIC_LIGHTMAPS: strIndirect = "lightmap"; break;
					case LI_CONSTANT: strIndirect = "constant"; break;
					case LI_NONE: strIndirect = "off"; break;
				}
				// how many lightmaps
				unsigned numVbufs = 0;
				unsigned numLmaps = 0;
				for (unsigned i=0;i<numObjects;i++)
				{
					if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber))
					{
						if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_VERTEX_BUFFER) numVbufs++; else
						if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE) numLmaps++;
					}
				}
				// what solver
				const char* strSolver = "?";
				switch(solver->getInternalSolverType())
				{
					case rr::RRDynamicSolver::NONE: strSolver = "none"; break;
					case rr::RRDynamicSolver::ARCHITECT: strSolver = "Architect"; break;
					case rr::RRDynamicSolver::FIREBALL: strSolver = "Fireball"; break;
					case rr::RRDynamicSolver::BOTH: strSolver = "both"; break;
				}
				// print it
				textOutput(x,y+=18,h,"lighting direct=%s indirect=%s lightmaps=(%dx vbuf %dx lmap %dx none) solver=%s",
					strDirect,strIndirect,numVbufs,numLmaps,numObjects-numVbufs-numLmaps,strSolver);
			}
			if (!svs.renderLightmaps2d || !lv)
			{
				textOutput(x,y+=18*2,h,"[viewport]");
				textOutput(x,y+=18,h,"size: %d*%d",winWidth,winHeight);
				GLint redBits,greenBits,blueBits,alphaBits,depthBits;
				glGetIntegerv(GL_RED_BITS, &redBits);
				glGetIntegerv(GL_GREEN_BITS, &greenBits);
				glGetIntegerv(GL_BLUE_BITS, &blueBits);
				glGetIntegerv(GL_ALPHA_BITS, &alphaBits);
				glGetIntegerv(GL_DEPTH_BITS, &depthBits);
				textOutput(x,y+=18,h,"r+g+b+a+z: %d+%d+%d+%d+%d",(int)redBits,(int)greenBits,(int)blueBits,(int)alphaBits,(int)depthBits);
			}
			if (!svs.renderLightmaps2d || !lv) if (svs.selectedLightIndex<solver->realtimeLights.size())
			{
				if (numTrianglesMulti<100000) // skip this expensive step for big scenes
				{
					RealtimeLight* rtlight = solver->realtimeLights[svs.selectedLightIndex];
					const rr::RRLight* rrlight = &rtlight->getRRLight();
					static RealtimeLight* lastLight = NULL;
					static unsigned numLightReceivers = 0;
					static unsigned numShadowCasters = 0;
					if (rtlight!=lastLight)
					{
						lastLight = rtlight;
						numLightReceivers = 0;
						numShadowCasters = 0;
						for (unsigned t=0;t<numTrianglesMulti;t++)
						{
							if (multiObject->getTriangleMaterial(t,rrlight,NULL)) numLightReceivers++;
							for (unsigned j=0;j<numObjects;j++)
							{
								if (multiObject->getTriangleMaterial(t,rrlight,solver->getStaticObjects()[j])) numShadowCasters++;
							}
						}
					}
					textOutput(x,y+=18*2,h,"[light %d/%d]",svs.selectedLightIndex,solver->realtimeLights.size());
					textOutput(x,y+=18,h,"triangles lit: %d/%d",numLightReceivers,numTrianglesMulti);
					textOutput(x,y+=18,h,"triangles casting shadow: %f/%d",numShadowCasters/float(numObjects),numTrianglesMulti);
				}
			}
			if (singleMesh && svs.selectedObjectIndex<solver->getStaticObjects().size())
			{
				textOutput(x,y+=18*2,h,"[static object %d/%d]",svs.selectedObjectIndex,numObjects);
				textOutput(x,y+=18,h,"triangles: %d/%d",numTrianglesSingle,numTrianglesMulti);
				textOutput(x,y+=18,h,"vertices: %d/%d",singleMesh->getNumVertices(),multiMesh?multiMesh->getNumVertices():0);
				static const rr::RRObject* lastObject = NULL;
				static rr::RRVec3 bboxMinL;
				static rr::RRVec3 bboxMaxL;
				static rr::RRVec3 centerL;
				static rr::RRVec3 bboxMinW;
				static rr::RRVec3 bboxMaxW;
				static rr::RRVec3 centerW;
				static unsigned numReceivedLights = 0;
				static unsigned numShadowsCast = 0;
				if (singleObject!=lastObject)
				{
					lastObject = singleObject;
					singleObject->getCollider()->getMesh()->getAABB(&bboxMinL,&bboxMaxL,&centerL);
					rr::RRMesh* singleWorldMesh = singleObject->createWorldSpaceMesh();
					singleWorldMesh->getAABB(&bboxMinW,&bboxMaxW,&centerW);
					delete singleWorldMesh;
					numReceivedLights = 0;
					numShadowsCast = 0;
					for (unsigned i=0;i<numLights;i++)
					{
						rr::RRLight* rrlight = solver->getLights()[i];
						for (unsigned t=0;t<numTrianglesSingle;t++)
						{
							if (singleObject->getTriangleMaterial(t,rrlight,NULL)) numReceivedLights++;
							for (unsigned j=0;j<numObjects;j++)
							{
								if (singleObject->getTriangleMaterial(t,rrlight,solver->getStaticObjects()[j])) numShadowsCast++;
							}
						}
					}
				}

				if (numTrianglesSingle)
				{
					textOutput(x,y+=18,h,"world AABB: %f %f %f .. %f %f %f",bboxMinW[0],bboxMinW[1],bboxMinW[2],bboxMaxW[0],bboxMaxW[1],bboxMaxW[2]);
					textOutput(x,y+=18,h,"world center: %f %f %f",centerW[0],centerW[1],centerW[2]);
					textOutput(x,y+=18,h,"local AABB: %f %f %f .. %f %f %f",bboxMinL[0],bboxMinL[1],bboxMinL[2],bboxMaxL[0],bboxMaxL[1],bboxMaxL[2]);
					textOutput(x,y+=18,h,"local center: %f %f %f",centerL[0],centerL[1],centerL[2]);
					textOutput(x,y+=18,h,"received lights: %f/%d",numReceivedLights/float(numTrianglesSingle),numLights);
					textOutput(x,y+=18,h,"shadows cast: %f/%d",numShadowsCast/float(numTrianglesSingle),numLights*numObjects);
				}
				if (svs.selectedObjectIndex<solver->getStaticObjects().size())
				{
					rr::RRBuffer* bufferSelectedObj = solver->getStaticObjects()[svs.selectedObjectIndex]->illumination.getLayer(svs.staticLayerNumber);
					if (bufferSelectedObj)
					{
						//if (svs.renderRealtime) glColor3f(0.5f,0.5f,0.5f);
						textOutput(x,y+=18,h,"[lightmap]");
						textOutput(x,y+=18,h,"type: %s",(bufferSelectedObj->getType()==rr::BT_VERTEX_BUFFER)?"PER VERTEX":((bufferSelectedObj->getType()==rr::BT_2D_TEXTURE)?"PER PIXEL":"INVALID!"));
						textOutput(x,y+=18,h,"size: %d*%d*%d",bufferSelectedObj->getWidth(),bufferSelectedObj->getHeight(),bufferSelectedObj->getDepth());
						textOutput(x,y+=18,h,"format: %s",(bufferSelectedObj->getFormat()==rr::BF_RGB)?"RGB":((bufferSelectedObj->getFormat()==rr::BF_RGBA)?"RGBA":((bufferSelectedObj->getFormat()==rr::BF_RGBF)?"RGBF":((bufferSelectedObj->getFormat()==rr::BF_RGBAF)?"RGBAF":"INVALID!"))));
						textOutput(x,y+=18,h,"scale: %s",bufferSelectedObj->getScaled()?"custom(usually sRGB)":"physical(linear)");
						//glColor3f(1,1,1);
					}
				}
			}
			if (multiMesh && (!svs.renderLightmaps2d || !lv))
			{
				if (selectedPointValid)
				{
					rr::RRMesh::PreImportNumber preTriangle = multiMesh->getPreImportTriangle(ray->hitTriangle);
					const rr::RRMaterial* triangleMaterial = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
					const rr::RRMaterial* material = triangleMaterial;
					rr::RRPointMaterial pointMaterial;
					if (material && material->minimalQualityForPointMaterials<10000)
					{
						multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
						material = &pointMaterial;
					}
					rr::RRMesh::TriangleMapping triangleMapping;
					multiMesh->getTriangleMapping(ray->hitTriangle,triangleMapping,material?material->lightmapTexcoord:0);
					rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray->hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray->hitPoint2d[1];
					textOutput(x,y+=18*2,h,"[pointed by mouse]");
					textOutput(x,y+=18,h,"object: %d/%d",preTriangle.object,numObjects);
					rr::RRBuffer* objectsLightmap = solver->getStaticObjects()[preTriangle.object]->illumination.getLayer(svs.staticLayerNumber);
					textOutput(x,y+=18,h,"object's lightmap: %s %dx%d",objectsLightmap?(objectsLightmap->getType()==rr::BT_2D_TEXTURE?"per-pixel":"per-vertex"):"none",objectsLightmap?objectsLightmap->getWidth():0,objectsLightmap?objectsLightmap->getHeight():0);
					textOutput(x,y+=18,h,"triangle in object: %d/%d",preTriangle.index,solver->getStaticObjects()[preTriangle.object]->getCollider()->getMesh()->getNumTriangles());
					textOutput(x,y+=18,h,"triangle in scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
					textOutput(x,y+=18,h,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
					textOutput(x,y+=18,h,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
					rr::RRBuffer* bufferCenter = solver->getStaticObjects()[preTriangle.object]->illumination.getLayer(svs.staticLayerNumber);
					if (bufferCenter && bufferCenter->getType()==rr::BT_2D_TEXTURE)
					{
						int i = int(uvInLightmap[0]*bufferCenter->getWidth());
						int j = int(uvInLightmap[1]*bufferCenter->getHeight());
						textOutput(x,y+=18,h,"ij in lightmap: %d %d",i,j);
						if (i>=0 && i<(int)bufferCenter->getWidth() && j>=0 && j<(int)bufferCenter->getHeight())
						{
							// diagnose texel
							centerObject = preTriangle.object;
							centerTexel = i + j*bufferCenter->getWidth();
							centerTriangle = ray->hitTriangle;
						}
					}
					if (centerObject==UINT_MAX)
					{
						// diagnose triangle
						centerObject = preTriangle.object;
						centerTriangle = ray->hitTriangle;
					}
					textOutput(x,y+=18,h,"distance: %f",ray->hitDistance);
					textOutput(x,y+=18,h,"pos: %f %f %f",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
					textOutput(x,y+=18,h,"plane:  %f %f %f %f",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2],ray->hitPlane[3]);
					textOutput(x,y+=18,h,"normal: %f %f %f",selectedPointBasis.normal[0],selectedPointBasis.normal[1],selectedPointBasis.normal[2]);
					textOutput(x,y+=18,h,"tangent: %f %f %f",selectedPointBasis.tangent[0],selectedPointBasis.tangent[1],selectedPointBasis.tangent[2]);
					textOutput(x,y+=18,h,"bitangent: %f %f %f",selectedPointBasis.bitangent[0],selectedPointBasis.bitangent[1],selectedPointBasis.bitangent[2]);
					textOutput(x,y+=18,h,"side: %s",ray->hitFrontSide?"front":"back");
					unsigned numReceivedLights = 0;
					unsigned numShadowsCast = 0;
					for (unsigned i=0;i<numLights;i++)
					{
						rr::RRLight* rrlight = solver->getLights()[i];
						if (multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,NULL)) numReceivedLights++;
						for (unsigned j=0;j<numObjects;j++)
						{
							if (multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,solver->getStaticObjects()[j])) numShadowsCast++;
						}
					}
					textOutput(x,y+=18,h,"received lights: %d/%d",numReceivedLights,numLights);
					textOutput(x,y+=18,h,"shadows cast: %d/%d",numShadowsCast,numLights*numObjects);
				}
				textOutput(x,y+=18*2,h,"numbers of casters/lights show potential, what is allowed");
			}
			if (multiMesh && svs.renderLightmaps2d && lv)
			{
				rr::RRVec2 uv = lv->getCenterUv(GetSize());
				textOutput(x,y+=18*2,h,"[pointed by mouse]");
				textOutput(x,y+=18,h,"uv: %f %f",uv[0],uv[1]);
				if (svs.selectedObjectIndex<solver->getStaticObjects().size())
				{
					rr::RRBuffer* buffer = solver->getStaticObjects()[svs.selectedObjectIndex]->illumination.getLayer(svs.staticLayerNumber);
					if (buffer && buffer->getType()==rr::BT_2D_TEXTURE)
					{
						int i = int(uv[0]*buffer->getWidth());
						int j = int(uv[1]*buffer->getHeight());
						textOutput(x,y+=18,h,"ij: %d %d",i,j);
						if (i>=0 && i<(int)buffer->getWidth() && j>=0 && j<(int)buffer->getHeight())
						{
							centerObject = svs.selectedObjectIndex;
							centerTexel = i + j*buffer->getWidth();
							//!!!centerTriangle = ?;
							rr::RRVec4 color = buffer->getElement(i+j*buffer->getWidth());
							textOutput(x,y+=18,h,"color: %f %f %f %f",color[0],color[1],color[2],color[3]);
						}
					}
				}
			}
			glEnable(GL_DEPTH_TEST);
		}
	}

	// render help, using own shader
	if (svs.renderHelp)
	{
		if (!helpLoadAttempted)
		{
			helpLoadAttempted = true;
			RR_ASSERT(!helpImage);
			helpImage = rr::RRBuffer::load(wxString::Format("%s../maps/sv_help.png",svs.pathToShaders));
			if (!helpImage)
			{
				wxMessageBox("To LOOK, move mouse with left button pressed.\n"
					"To MOVE, use arrows or wsadqzxc.\n"
					"To ZOOM, use wheel.\n"
					"To PAN, move mouse with middle button pressed.\n"
					"To INSPECT, move mouse with right button pressed.\n"
					"Edit everything in property panels.\n"
					"Run actions from menu.\n"
					"Run more actions from scene tree context menu (e.g. add lights).\n"
					"Play/pause videos with spacebar.\n"
					,"Controls");
			}
		}
		if (helpImage && textureRenderer)
		{
			float w = helpImage->getWidth()/(float)winWidth;
			float h = helpImage->getHeight()/(float)winHeight;
			if (w>h)
			{
				h /= w;
				w = 1;
			}
			else
			{
				w /= h;
				h = 1;
			}
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			textureRenderer->render2D(getTexture(helpImage,false,false),NULL,(1-w)*0.5f,(1-h)*0.5f,w,h);
			glDisable(GL_BLEND);
		}
	}


	// render fps, using own shader
	unsigned fps = fpsCounter.getFps();
	if (svs.renderFPS)
	{
		if (!fpsLoadAttempted)
		{
			fpsLoadAttempted = true;
			RR_ASSERT(!fpsDisplay);
			fpsDisplay = FpsDisplay::create(wxString::Format("%s../maps/",svs.pathToShaders));
		}
		if (fpsDisplay)
		{
			fpsDisplay->render(textureRenderer,fps,winWidth,winHeight);
		}
	}

}

void SVCanvas::OnPaint(wxPaintEvent& event)
{
#ifdef _MSC_VER
	__try
	{
		OnPaintCore(event);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"3D renderer crashed, you'll see no image. The rest of application (menus, windows, hotkeys) may still work.\n"));
		glClearColor(1,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0,0,0,0);
		SwapBuffers();
	}
#else
	OnPaintCore(event);
#endif
}


BEGIN_EVENT_TABLE(SVCanvas, wxGLCanvas)
    EVT_SIZE(SVCanvas::OnSize)
    EVT_PAINT(SVCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(SVCanvas::OnEraseBackground)
    EVT_KEY_DOWN(SVCanvas::OnKeyDown)
    EVT_KEY_UP(SVCanvas::OnKeyUp)
    EVT_MOUSE_EVENTS(SVCanvas::OnMouseEvent)
    EVT_IDLE(SVCanvas::OnIdle)
    EVT_ENTER_WINDOW(SVCanvas::OnEnterWindow)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
