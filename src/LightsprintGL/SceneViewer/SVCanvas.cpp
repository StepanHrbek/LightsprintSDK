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
#include "SVSceneTree.h" // for shortcuts that manipulate animations in scene tree
#include "SVLog.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/Bloom.h"
#include "Lightsprint/GL/LensFlare.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Water.h"
#include "../PreserveState.h"
#include "../RendererOfMesh.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif
#include <cctype>
#include <boost/filesystem.hpp> // extension()==".gsa"

namespace bf = boost::filesystem;

#if !wxUSE_GLCANVAS
    #error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
	#define REPORT_HEAP_STATISTICS // reports num allocations per frame
#endif

namespace rr_gl
{


#ifdef REPORT_HEAP_STATISTICS
	_CRT_ALLOC_HOOK s_oldAllocHook;
	unsigned s_numAllocs = 0;
	unsigned s_numAllocs2 = 0;
	size_t s_bytesAllocated = 0;
	int newAllocHook( int allocType, void *userData, size_t size, int blockType, long requestNumber, const unsigned char *filename, int lineNumber)
	{
		s_numAllocs++;
		s_numAllocs2 = requestNumber;
		s_bytesAllocated += size;
		return s_oldAllocHook(allocType,userData,size,blockType,requestNumber,filename,lineNumber);
	}
#endif

/////////////////////////////////////////////////////////////////////////////
//
// SVCanvas

static int s_attribList[] = {
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	//WX_GL_SAMPLE_BUFFERS, GL_TRUE, // produces no visible improvement
#ifndef __WXMAC__ // multisampling on MacMini would use software renderer
	WX_GL_SAMPLES, 4, // antialiasing. can be later disabled by glDisable(GL_MULTISAMPLE), but it doesn't improve speed (tested on X1650). it must be disabled here (change 4 to 1) for higher fps
#endif
	WX_GL_DEPTH_SIZE, 24, // default is 16, explicit 24 should reduce z-fight. 32 fails on all cards tested including 5870 (falls back to default without antialiasing)
	0, 0};

SVCanvas::SVCanvas( SceneViewerStateEx& _svs, SVFrame *_svframe, wxSize _size)
	: wxGLCanvas(_svframe, wxID_ANY, s_attribList, wxDefaultPosition, _size, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE, "GLCanvas"), svs(_svs)
{
	renderEmptyFrames = false;
	context = NULL;
	svframe = _svframe;
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
	speedY = 0;
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
	renderedIcons.iconSize = 1;
	fullyCreated = false;

	skyboxBlendingInProgress = false;

}

void SVCanvas::createContextCore()
{
	context = new wxGLContext(this);
	SetCurrent(*context);

#ifdef REPORT_HEAP_STATISTICS
	s_oldAllocHook = _CrtSetAllocHook(newAllocHook);
#endif

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
		rr::RRReporter::report(rr::ERRO,"Your system does not support OpenGL 2.0. You can see it with GLview. Note: Some multi-display systems support 2.0 only on one display.\n");
		exitRequested = true;
		return;
	}
	int i1=0,i2=0,i3=0,i4=0,i5=0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&i1);
	glGetIntegerv(GL_MAX_TEXTURE_UNITS,&i2);
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&i3);
	glGetIntegerv(GL_MAX_TEXTURE_COORDS,&i4);
	glGetIntegerv(GL_MAX_VARYING_FLOATS,&i5);
	rr::RRReporter::report(rr::INF2,"  %d image units, %d units, %d combined, %d coords, %d varyings.\n",i1,i2,i3,i4,i5);
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
		rr::RRScene* scene = svframe->loadScene(svs.sceneFilename,svframe->userPreferences.import.getUnitLength(svs.sceneFilename),svframe->userPreferences.import.getUpAxis(svs.sceneFilename),true);
		mergedScenes.push_back(scene);

		// send everything to solver
		solver->setEnvironment(mergedScenes[0]->environment);
		envToBeDeletedOnExit = false;
		solver->setStaticObjects(mergedScenes[0]->objects,NULL);
		solver->setDynamicObjects(mergedScenes[0]->objects);
		solver->setLights(mergedScenes[0]->lights);
	}

	// warning: when rendering scene from initialInputSolver, original cube buffers are lost here
	reallocateBuffersForRealtimeGI(true);

	// load skybox from filename only if we don't have it from inputsolver or scene yet
	if (!solver->getEnvironment() && !svs.skyboxFilename.empty())
	{
		rr::RRReportInterval report(rr::INF3,"Loading skybox...\n");
		svframe->OnMenuEventCore(SVFrame::ME_ENV_RELOAD);
	}


	solver->observer = &svs.eye; // solver automatically updates lights that depend on camera
	if (solver->getStaticObjects().size())
	{
		rr::RRReportInterval report(rr::INF3,"Setting illumination type...\n");
		switch (svs.renderLightIndirect)
		{
			// try to load FB. if not found, create it
			case LI_REALTIME_FIREBALL:     svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_FIREBALL); break;
			// create architect
			case LI_REALTIME_ARCHITECT:    svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_ARCHITECT); break;
		}

		// try to load lightmaps
		for (unsigned i=0;i<solver->getStaticObjects().size();i++)
			if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber))
				goto lightmapFoundInRam;
		solver->getStaticObjects().loadLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);
		lightmapFoundInRam:

		// try to load LDM. if not found, disable it
		for (unsigned i=0;i<solver->getStaticObjects().size();i++)
			if (solver->getStaticObjects()[i]->illumination.getLayer(svs.ldmLayerNumber))
				goto ldmFoundInRam;
		if (!solver->getStaticObjects().loadLayer(svs.ldmLayerNumber,LDM_PREFIX,LDM_POSTFIX))
			svs.renderLDM = false;
		ldmFoundInRam:;
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

			//renderedIcons.iconSize = (sceneMax-sceneMin).avg()*0.04f;
			// better, insensitive to single triangle in 10km distance
			renderedIcons.iconSize = object->getCollider()->getMesh()->getAverageVertexDistance()*0.017f;

			sunIconPosition.y = sceneMax.y + 5*renderedIcons.iconSize;
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
	if (scene || add)
	{
		// add or remove scene from solver
		rr::RRObjects objects = solver->getStaticObjects();
		objects.insert(objects.end(),solver->getDynamicObjects().begin(),solver->getDynamicObjects().end());
		rr::RRLights lights = solver->getLights();
		if (scene)
		{
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
		}
		solver->setStaticObjects(objects,NULL);
		solver->setDynamicObjects(objects);
		solver->setLights(lights);
	}
	else
	{
		// add or remove object from solver
		solver->reportDirectIlluminationChange(-1,true,true,true);
	}

	// fix svs.renderLightIndirect, setStaticObjects() just silently switched solver to architect
	// must be changed if setStaticObjects() behaviour changes
	// we don't switch to architect, but rather to const ambient, because architect ignores environment, scenes without lights are black
	if (svs.renderLightIndirect==LI_REALTIME_FIREBALL)
	{
		svs.renderLightIndirect = LI_CONSTANT;
	}

	// object numbers did change, change also svs.selectedObjectIndex so that it points to object in objprops
	// and following updateAllPanels() does not change objprops
	svs.selectedObjectIndex = 0;
	for (unsigned i=solver->getStaticObjects().size()+solver->getDynamicObjects().size();i--;)
		if (solver->getObject(i)==svframe->m_objectProperties->object)
			svs.selectedObjectIndex = i;

	// fix dangling pointer in light properties pane
	svframe->updateAllPanels();

	// fix dangling pointer in collisionHandler
	delete collisionHandler;
	collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();

	// alloc rtgi buffers, otherwise new objects would have no realtime indirect
	if (add)
		reallocateBuffersForRealtimeGI(true);
}

void SVCanvas::reallocateBuffersForRealtimeGI(bool reallocateAlsoVbuffers)
{
	solver->allocateBuffersForRealtimeGI(
		reallocateAlsoVbuffers?svs.realtimeLayerNumber:-1,
		svs.raytracedCubesDiffuseRes,svs.raytracedCubesSpecularRes,RR_MAX(svs.raytracedCubesDiffuseRes,svs.raytracedCubesSpecularRes),
		true,true,svs.raytracedCubesSpecularThreshold,svs.raytracedCubesDepthThreshold);
	svframe->m_objectProperties->updateProperties();
}

SVCanvas::~SVCanvas()
{
#ifdef REPORT_HEAP_STATISTICS
	_CrtSetAllocHook(s_oldAllocHook);
#endif
	if (!svs.releaseResources)
	{
		// user requested fast exit without releasing resources
		// however, if scene is .gsa, we must ignore him and release resources (it shutdowns gamebryo)
		bool sceneMustBeReleased = _stricmp(bf::path(RR_WX2PATH(svs.sceneFilename)).extension().string().c_str(),".gsa")==0;
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
		for (unsigned i=0;i<solver->getStaticObjects().size()+solver->getDynamicObjects().size();i++)
		{
			RR_SAFE_DELETE(solver->getObject(i)->illumination.getLayer(svs.realtimeLayerNumber));
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
	OnSizeCore(false);
}

void SVCanvas::OnSizeCore(bool force)
{
	// set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);
	if (context
		// do nothing when wx calls us with unchanged size
		// (why? when exiting SV with one panel floating and all other panels closed, partialy destroyed window calls us with unchanged size, SetCurrent would assert)
		&& (force || w!=winWidth || h!=winHeight))
	{
		SetCurrent(*context);
		winWidth = w;
		winHeight = h;
		// with Enhanced screenshot checked, viewport maintains the same aspect as screenshot
		if (svframe->userPreferences.sshotEnhanced)
		{
			if (w*svframe->userPreferences.sshotEnhancedHeight > h*svframe->userPreferences.sshotEnhancedWidth)
				winWidth = h*svframe->userPreferences.sshotEnhancedWidth/svframe->userPreferences.sshotEnhancedHeight;
			else
				winHeight = w*svframe->userPreferences.sshotEnhancedHeight/svframe->userPreferences.sshotEnhancedWidth;
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
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_SPOT);
			break;
		case 'O': // alt-o
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_POINT);
			break;
		case 'F': // alt-f
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_FLASH);
			break;
		case '1':
		case '2':
		case '3':
			svframe->OnMenuEventCore(SVFrame::ME_WINDOW_LAYOUT1+evkey-'1');
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

		case WXK_F8: svframe->OnMenuEventCore(SVFrame::ME_FILE_SAVE_SCREENSHOT); break;
		case WXK_F11: svframe->OnMenuEventCore(SVFrame::ME_WINDOW_FULLSCREEN_META); break;
		case WXK_NUMPAD_ADD:
		case '+': svs.tonemappingBrightness *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_SUBTRACT:
		case '-': svs.tonemappingBrightness /= 1.2f; needsRefresh = true; break;

		//case '8': if (event.GetModifiers()==0) break; // ignore '8', but accept '8' + shift as '*', continue to next case
		case WXK_NUMPAD_MULTIPLY:
		case '*': svs.tonemappingGamma *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_DIVIDE:
		case '/': svs.tonemappingGamma /= 1.2f; needsRefresh = true; break;

		case WXK_UP:
		case 'w':
		case 'W': speedForward = speed; break;

		case WXK_DOWN:
		case 's':
		case 'S': speedBack = speed; break;

		case WXK_RIGHT:
		case 'd':
		case 'D': speedRight = speed; break;

		case WXK_LEFT:
		case 'a':
		case 'A': speedLeft = speed; break;

		case 'q':
		case 'Q': speedUp = speed; break;
		case 'z':
		case 'Z': speedDown = speed; break;

		case WXK_PAGEUP: speedY = speed; break;
		case WXK_PAGEDOWN: speedY = -speed; break;

		case 'x':
		case 'X': speedLean = -speed; break;
		case 'c':
		case 'C': speedLean = +speed; break;

		case 'h':
		case 'H': svframe->OnMenuEventCore(SVFrame::ME_HELP); break;

		case WXK_DELETE: svframe->m_sceneTree->runContextMenuAction(CM_DELETE,svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED)); break;

		case 'L': svframe->OnMenuEventCore(SVFrame::ME_VIEW_LEFT); break;
		case 'R': svframe->OnMenuEventCore(SVFrame::ME_VIEW_RIGHT); break;
		case 'F': svframe->OnMenuEventCore(SVFrame::ME_VIEW_FRONT); break;
		case 'B': svframe->OnMenuEventCore(SVFrame::ME_VIEW_BACK); break;
		case 'T': svframe->OnMenuEventCore(SVFrame::ME_VIEW_TOP); break;


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
				svframe->OnMenuEventCore(SVFrame::ME_WINDOW_FULLSCREEN_META);
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
		case WXK_UP:
		case 'w':
		case 'W': speedForward = 0; break;
		case WXK_DOWN:
		case 's':
		case 'S': speedBack = 0; break;

		case WXK_RIGHT:
		case 'd':
		case 'D': speedRight = 0; break;
		case WXK_LEFT:
		case 'a':
		case 'A': speedLeft = 0; break;

		case 'q':
		case 'Q': speedUp = 0; break;
		case 'z':
		case 'Z': speedDown = 0; break;

		case WXK_PAGEUP:
		case WXK_PAGEDOWN: speedY = 0; break;

		case 'x':
		case 'X':
		case 'c':
		case 'C': speedLean = 0; break;
	}
	event.Skip();
}

// Filled on click, original data acquired at click time.
struct ClickInfo
{
	rr::RRTime time;
	int mouseX;
	int mouseY;
	bool mouseLeft;
	bool mouseRight;
	bool mouseMiddle;
	rr::RRVec3 pos;
	rr::RRVec3 rayOrigin;
	rr::RRVec3 rayDirection;
	unsigned hitTriangle;
	float hitDistance;
	rr::RRVec2 hitPoint2d;
	rr::RRVec3 hitPoint3d;
	SVEntity clickedEntity;
	bool clickedEntityIsSelected;
};
// set by OnMouseEvent on click (but not set on click that closes menu), valid and constant during drag
// read also by OnPaint (paints crosshair)
static bool s_ciRelevant = false; // are we dragging and is s_ci describing click that started it?
static bool s_ciRenderCrosshair = false;
static ClickInfo s_ci;

// What triggers context menu:
// a) rightclick -> OnMouseEvent() -> creates context menu
// b) hotkey -> OnContextMenuCreate() -> OnMouseEvent(event with ID_CONTEXT_MENU) -> creates context menu
#define ID_CONTEXT_MENU 543210

void SVCanvas::OnMouseEvent(wxMouseEvent& event)
{
	if (exitRequested || !fullyCreated)
		return;

	if (event.IsButton())
	{
		// regain focus, innocent actions like clicking menu take it away
		SetFocus();
	}

	// coords from previous frame, inherited to current frame if new coords are not available
	static wxPoint oldPosition(-1,-1);
	static rr::RRVec2 oldMousePositionInWindow(0);
	bool contextMenu = event.GetId()==ID_CONTEXT_MENU;
	wxPoint newPosition = (event.GetPosition()==wxPoint(-1,-1)) ? oldPosition : event.GetPosition(); // use previous coords for event that does not come with its own
	mousePositionInWindow = rr::RRVec2(newPosition.x*2.0f/GetSize().x-1,newPosition.y*2.0f/GetSize().y-1);

	if (!winWidth || !winHeight || !solver)
	{
		return;
	}
	if (svs.renderLightmaps2d && lv)
	{
		lv->OnMouseEvent(event,GetSize());
		return;
	}

	const EntityIds& selectedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);

	// scene clicked: fill s_xxx
	if (event.ButtonDown() || contextMenu)
	{
		// init static variables when dragging starts
		s_ciRelevant = true;
		s_ci.mouseX = newPosition.x;
		s_ci.mouseY = newPosition.y;
		s_ci.time.setNow();
		s_ci.mouseLeft = event.LeftIsDown();
		s_ci.mouseMiddle = event.MiddleIsDown();
		s_ci.mouseRight = event.RightIsDown();
		Camera* cam = (event.LeftIsDown()
			 && selectedType==ST_LIGHT && svs.selectedLightIndex<solver->getLights().size()) ? solver->realtimeLights[svs.selectedLightIndex]->getParent() : &svs.eye;
		s_ci.pos = svs.eye.pos;
		s_ci.rayOrigin = svs.eye.getRayOrigin(mousePositionInWindow);
		s_ci.rayDirection = svs.eye.getRayDirection(mousePositionInWindow);

		// find scene distance, adjust search range to look only for closer icons
		s_ci.hitDistance = svs.eye.getNear()*0.9f+svs.eye.getFar()*0.1f;
		ray->rayOrigin = s_ci.rayOrigin;
		rr::RRVec3 directionToMouse = s_ci.rayDirection;
		float directionToMouseLength = directionToMouse.length();
		ray->rayDir = directionToMouse/directionToMouseLength;
		ray->rayLengthMin = svs.eye.getNear()*directionToMouseLength;
		ray->rayLengthMax = svs.eye.getFar()*directionToMouseLength;
		ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_TRIANGLE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D;
		ray->hitObject = solver->getMultiObjectCustom(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
		ray->collisionHandler = collisionHandler;
		if (solver->getCollider()->intersect(ray))
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
		if (entityIcons->intersectIcons(renderedIcons,ray))
		{
			s_ci.clickedEntity = renderedIcons[ray->hitTriangle];
			s_ci.hitDistance = ray->hitDistance;
			s_ci.hitPoint2d = rr::RRVec2(0);
			s_ci.hitPoint3d = ray->rayOrigin + ray->rayDir*ray->hitDistance;
		}
		else
		{
			s_ci.clickedEntity.type = (s_ci.hitTriangle==UINT_MAX) ? ST_CAMERA : ST_OBJECT;
			if (ray->hitObject==NULL || ray->hitObject==solver->getMultiObjectCustom())
			{
				if (s_ci.hitTriangle==UINT_MAX)
				{
					s_ci.clickedEntity.index = 0;
				}
				else
				{
					rr::RRMesh::PreImportNumber pre = solver->getMultiObjectCustom()->getCollider()->getMesh()->getPreImportTriangle(s_ci.hitTriangle);
					s_ci.clickedEntity.index =  pre.object;
					s_ci.hitTriangle = pre.index;
				}
			}
			else
			{
				s_ci.clickedEntity.index = 0;
				unsigned numStaticObjects = solver->getStaticObjects().size();
				unsigned numDynamicObjects = solver->getDynamicObjects().size();
				for (unsigned i=0;i<numStaticObjects;i++)
					if (solver->getStaticObjects()[i]==ray->hitObject)
						s_ci.clickedEntity.index = i;
				for (unsigned i=0;i<numDynamicObjects;i++)
					if (solver->getDynamicObjects()[i]==ray->hitObject)
						s_ci.clickedEntity.index = numStaticObjects+i;
			}
			s_ci.clickedEntity.iconCode = IC_LAST;
		}

		s_ci.clickedEntityIsSelected = selectedEntities.find(s_ci.clickedEntity)!=selectedEntities.end();

		if (contextMenu)
			goto context_menu;
	}

	// handle clicking (mouse released in less than 0.2s in less than 20pix distance)
	bool clicking = s_ci.time.secondsPassed()<0.2f && abs(event.GetX()-s_ci.mouseX)<20 && abs(event.GetY()-s_ci.mouseY)<20;
	if ((event.LeftUp() || event.RightUp()) && clicking)
	{
		if (event.LeftUp())
		{
			// left click = select
			if ((selectedEntities.size()==1 && s_ci.clickedEntityIsSelected) || event.ControlDown())
			{
				// toggle selection (clicked with ctrl)
				wxTreeItemId item = svframe->m_sceneTree->entityIdToItemId(s_ci.clickedEntity);
				if (item.IsOk())
					svframe->m_sceneTree->ToggleItemSelection(item);
			}
			else
			{
				// select single entity (clicked without ctrl)
				svframe->m_materialProperties->locked = true; // selectEntityInTreeAndUpdatePanel calls setMaterial, material panel must ignore it (set is slow and it clears [x] point, [x] phys)
				svframe->selectEntityInTreeAndUpdatePanel(s_ci.clickedEntity,SEA_SELECT);
				svframe->m_materialProperties->locked = false;
			}
			if (s_ci.hitTriangle!=UINT_MAX)
			{
				if (s_ci.clickedEntity.index>=solver->getStaticObjects().size())
				{
					// dynamic object: pass 1object + triangle in 1object, m_materialProperties will show only custom version
					// if we do it also for static object, it won't have physical version
					svframe->m_materialProperties->setMaterial(solver,solver->getObject(s_ci.clickedEntity.index),s_ci.hitTriangle,s_ci.hitPoint2d);
				}
				else
				{
					// static object: pass NULL + triangle in multiobject, m_materialProperties will show custom+physical versions
					// m_materialProperties has extra 'object=NULL' path for looking up material in multiobjCustom and multiObjPhysical
					rr::RRMesh::PreImportNumber pre;
					pre.object = s_ci.clickedEntity.index;
					pre.index = s_ci.hitTriangle;
					unsigned post = solver->getMultiObjectCustom()->getCollider()->getMesh()->getPostImportTriangle(pre);
					svframe->m_materialProperties->setMaterial(solver,NULL,post,s_ci.hitPoint2d);
				}
			}
		}
		else
		{
			// right click = context menu
			context_menu:

			// when right clicking something not yet selected, select it, unselect everything else
			if (!s_ci.clickedEntityIsSelected)
			{
				svframe->m_sceneTree->UnselectAll();
				wxTreeItemId itemId = svframe->m_sceneTree->entityIdToItemId(s_ci.clickedEntity);
				if (itemId.IsOk())
					svframe->m_sceneTree->SelectItem(itemId);
			}

			wxTreeEvent event2;
			event2.SetPoint(wxPoint(-1,-1));
			svframe->m_sceneTree->OnContextMenuCreate(event2);
		}
	}

	// handle double clicking
	if (event.LeftDClick() && s_ci.hitTriangle==UINT_MAX)
	{
		svframe->OnMenuEventCore(SVFrame::ME_WINDOW_FULLSCREEN_META);
	}

	// handle dragging
	if (event.Dragging() && s_ciRelevant && newPosition!=oldPosition && !clicking)
	{
		const EntityIds& manipulatedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
		rr::RRVec3 manipulatedCenter = svframe->m_sceneTree->getCenterOf(manipulatedEntities);

		if (event.LeftIsDown() && s_ci.clickedEntityIsSelected)
		{
			// moving selection
			rr::RRVec3 pan;
			if (svs.eye.orthogonal)
			{
				pan = svs.eye.getRayOrigin(mousePositionInWindow)-svs.eye.getRayOrigin(oldMousePositionInWindow);
			}
			else
			{
				pan = (svs.eye.getRayDirection(mousePositionInWindow)-svs.eye.getRayDirection(oldMousePositionInWindow))*(s_ci.hitDistance/s_ci.rayDirection.length());
			}
			if (event.ShiftDown() || event.ControlDown() || event.AltDown())
			{
				if (!event.ControlDown()) pan.x = 0;
				if (!event.ShiftDown()) pan.y = 0;
				if (!event.AltDown()) pan.z = 0;
			}
			svframe->m_sceneTree->manipulateEntities(manipulatedEntities,rr::RRMatrix3x4::translation(pan));
		}
		else
		if (event.RightIsDown() && s_ci.clickedEntityIsSelected)
		{
			// rotating selection
			float dragX = (newPosition.x-oldPosition.x)/(float)winWidth;
			float dragY = (newPosition.y-oldPosition.y)/(float)winHeight;
			svframe->m_sceneTree->manipulateEntities(manipulatedEntities,(rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,1,0),dragX*5)*rr::RRMatrix3x4::rotationByAxisAngle(svs.eye.right,dragY*5)).centeredAround(manipulatedCenter));
			solver->reportInteraction();

			s_ci.hitPoint3d = manipulatedCenter;
			s_ci.hitDistance = (s_ci.hitPoint3d-svs.eye.pos).length();
			s_ciRenderCrosshair = true;
		}
		else
		if (event.LeftIsDown())
		{
			// rotating camera
			float dragX = (newPosition.x-oldPosition.x)/(float)winWidth;
			float dragY = (newPosition.y-oldPosition.y)/(float)winHeight;
			svframe->m_sceneTree->manipulateEntity(EntityId(ST_CAMERA,0),(rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,1,0),dragX*5*svs.eye.getFieldOfViewHorizontalDeg()/90)*rr::RRMatrix3x4::rotationByAxisAngle(svs.eye.right,dragY*5*svs.eye.getFieldOfViewHorizontalDeg()/90)).centeredAround(svs.eye.pos));
			solver->reportInteraction();
		}
		else
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
			s_ciRenderCrosshair = true;
		}
		else
		if (event.RightIsDown())
		{
			// inspection
			//  rotate around clicked point, point does not move on screen
			float dragX = (newPosition.x-oldPosition.x)/(float)winWidth;
			float dragY = (newPosition.y-oldPosition.y)/(float)winHeight;
			svframe->m_sceneTree->manipulateEntity(EntityId(ST_CAMERA,0),(rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,1,0),dragX*5)*rr::RRMatrix3x4::rotationByAxisAngle(svs.eye.right,dragY*5)).centeredAround(s_ci.hitPoint3d));
			solver->reportInteraction();
			s_ciRenderCrosshair = true;
		}

	}
	if (!event.ButtonDown() && !event.Dragging())
	{
		// dragging ended, all s_xxx become invalid
		s_ciRelevant = false;
		s_ciRenderCrosshair = false;
	}

	// handle wheel
	if (event.GetWheelRotation())
	{
		if (svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED).size())
		{
			const EntityIds& manipulatedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);
			rr::RRVec3 manipulatedCenter = svframe->m_sceneTree->getCenterOf(manipulatedEntities);

			svframe->m_sceneTree->manipulateEntities(manipulatedEntities,rr::RRMatrix3x4::scale(rr::RRVec3(expf(event.GetWheelRotation()*0.1f/event.GetWheelDelta()))).centeredAround(manipulatedCenter));
			solver->reportInteraction();
		}
		else
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
		svs.eye.update(); // without this, some eye changes are ignored
	}

	if (svs.selectedLightIndex<solver->realtimeLights.size())
	{
		solver->realtimeLights[svs.selectedLightIndex]->updateAfterRealtimeLightChanges();
	}
	solver->reportInteraction();
	oldPosition = newPosition;
	oldMousePositionInWindow = mousePositionInWindow;
}

void SVCanvas::OnContextMenuCreate(wxContextMenuEvent& _event)
{
	wxMouseEvent event;
	event.SetPosition(_event.GetPosition());
	event.SetId(ID_CONTEXT_MENU);
	OnMouseEvent(event);
}


void SVCanvas::OnIdle(wxIdleEvent& event)
{
	if ((svs.initialInputSolver && svs.initialInputSolver->aborting) || (fullyCreated && !solver) || (solver && solver->aborting) || exitRequested)
	{
		svframe->Close(true);
		return;
	}
	if (!fullyCreated)
		return;

	// displays queued (asynchronous) reports
	// otherwise they would display before next synchronous report, possibly much later
	if (svframe->m_log)
		svframe->m_log->flushQueue();

	// camera/light movement
	static rr::RRTime prevTime;
	float seconds = prevTime.secondsSinceLastQuery();
	if (seconds>0)
	{
		// ray is used in this block


		// camera/light keyboard move
		RR_CLAMP(seconds,0.001f,0.3f);
		float meters = seconds * svs.cameraMetersPerSecond;
		Camera* cam = (selectedType!=ST_LIGHT)?&svs.eye:solver->realtimeLights[svs.selectedLightIndex]->getParent();

		{
			// yes -> respond to keyboard
			const EntityIds& entities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
			rr::RRVec3 center = svframe->m_sceneTree->getCenterOf(entities);
			svframe->m_sceneTree->manipulateEntities(entities,
				rr::RRMatrix3x4::translation(
					svs.eye.dir * ((speedForward-speedBack)*meters) +
					svs.eye.right * ((speedRight-speedLeft)*meters) +
					svs.eye.up * ((speedUp-speedDown)*meters) +
					rr::RRVec3(0,speedY*meters,0))
				* rr::RRMatrix3x4::rotationByAxisAngle(svs.eye.dir,speedLean*seconds*0.5f).centeredAround(center)
				);
		}
	}

	Refresh(false);
}

static void textOutput(int x, int y, int h, const char* format, ...)
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

void SVCanvas::OnPaint(wxPaintEvent& event)
{
	if (exitRequested || !fullyCreated || !winWidth || !winHeight)
		return;

	wxPaintDC dc(this);

	if (!context) return;
	SetCurrent(*context);

	svframe->AfterPaneOpenClose();

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

#ifdef REPORT_HEAP_STATISTICS
	// report num allocation per frame
	static _CrtMemState state;
	_CrtMemCheckpoint(&state);
	static unsigned oldNumAllocs = 0;
	static unsigned oldNumAllocs2 = 0;
	static size_t oldBytesAllocated = 0;
	static size_t oldBytesAllocated2 = 0;
	rr::RRReporter::report(rr::INF3,"Num allocations +%d +%d, bytes allocated: +%llu +%llu\n",s_numAllocs-oldNumAllocs,s_numAllocs2-oldNumAllocs2,(long long)s_bytesAllocated-oldBytesAllocated,(long long)state.lTotalCount-oldBytesAllocated2);
	oldNumAllocs = s_numAllocs;
	oldNumAllocs2 = s_numAllocs2;
	oldBytesAllocated = s_bytesAllocated;
	oldBytesAllocated2 = state.lTotalCount;
#endif

	if (svs.envSimulateSun && svs.envSpeed)
	{
		static rr::RRTime time;
		float seconds = time.secondsSinceLastQuery();
		if (seconds>0 && seconds<1)
			svs.envDateTime.addSeconds(seconds*svs.envSpeed);
		svframe->simulateSun();
	}

	Paint(false);

	// done
	SwapBuffers();
}

void SVCanvas::Paint(bool _takingSshot)
{
#ifdef _MSC_VER
	__try
	{
#endif
		PaintCore(_takingSshot);
#ifdef _MSC_VER
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"3D renderer crashed, you'll see no image. The rest of application (menus, windows, hotkeys) may still work.\n"));
		glClearColor(1,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0,0,0,0);
	}
#endif
}


void SVCanvas::PaintCore(bool _takingSshot)
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
		if (solver->getObject(svs.selectedObjectIndex))
			lv->setObject(
				solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(svs.renderLDMEnabled()?svs.ldmLayerNumber:svs.staticLayerNumber),
				solver->getObject(svs.selectedObjectIndex),
				svs.renderLightmapsBilinear);
		else
			lv->setObject(
				NULL,
				NULL,
				svs.renderLightmapsBilinear);
		lv->OnPaint(GetSize());
	}
	else
	{
		if (exitRequested || !winWidth || !winHeight) return; // can't display without window

		// blend skyboxes
		if (skyboxBlendingInProgress)
		{
			float blend = skyboxBlendingStartTime.secondsPassed()/3;
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
				skyboxBlendingInProgress = false;
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
				solver->realtimeLights[i]->updateAfterRRLightChanges(); // sets dirtyRange if pos/dir/fov did change
				solver->realtimeLights[i]->dirtyRange = false; // clear it, range already is good (dirty range would randomly disappear flashlight, reason unknown)
			}

		if (svs.cameraDynamicNear && !svs.eye.orthogonal) // don't change range in ortho, fixed range from setView() is better
		{
			// eye must already be updated here because next line depends on up, right
			svs.eye.setRangeDynamically(solver->getMultiObjectCustom(),svs.renderWater,svs.waterLevel);
			svs.eye.update();
		}

		if (svs.renderLightDirect==LD_REALTIME || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			for (unsigned i=0;i<solver->realtimeLights.size();i++)
				solver->realtimeLights[i]->shadowTransparencyRequested = svs.shadowTransparency;
			rr::RRDynamicSolver::CalculateParameters params;
			if (svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
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
			uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = svs.renderLDMEnabled();
			uberProgramSetup.LIGHT_INDIRECT_auto = svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE =
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = svs.raytracedCubesEnabled && solver->getStaticObjects().size()+solver->getDynamicObjects().size()<svs.raytracedCubesMaxObjects && svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
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
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					svs.renderLDMEnabled()?svs.ldmLayerNumber:UINT_MAX,
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
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					svs.renderLDMEnabled()?svs.ldmLayerNumber:UINT_MAX,
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
					svs.renderLightIndirect==LI_REALTIME_ARCHITECT || svs.renderLightIndirect==LI_REALTIME_FIREBALL,
					(svs.renderLightDirect==LD_STATIC_LIGHTMAPS || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)?svs.staticLayerNumber:svs.realtimeLayerNumber,
					svs.renderLDMEnabled()?svs.ldmLayerNumber:UINT_MAX,
					clipPlanes,
					svs.srgbCorrect,
					&brightness,
					gamma);
			}
			if (svs.renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// adjust tonemapping
			if (svs.renderTonemapping && svs.tonemappingAutomatic
				&& !svs.renderWireframe
				&& ((svs.renderLightIndirect==LI_STATIC_LIGHTMAPS && solver->containsLightSource())
					|| ((svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT) && solver->containsRealtimeGILightSource())
					|| svs.renderLightIndirect==LI_CONSTANT
					))
			{
				static rr::RRTime time;
				float secondsSinceLastFrame = time.secondsSinceLastQuery();
				if (secondsSinceLastFrame>0 && secondsSinceLastFrame<10)
				{
					toneMapping->adjustOperator(textureRenderer,secondsSinceLastFrame*svs.tonemappingAutomaticSpeed,svs.tonemappingBrightness,svs.tonemappingGamma,svs.tonemappingAutomaticTarget);
				}
			}

			// render selected
			if (!_takingSshot)
			{
				wxArrayTreeItemIds selections;
				if (svframe->m_sceneTree && svframe->m_sceneTree->GetSelections(selections)>0)
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					UberProgramSetup uberProgramSetup;
					uberProgramSetup.OBJECT_SPACE = true;
					uberProgramSetup.POSTPROCESS_NORMALS = true;
					Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,NULL);
					for (unsigned i=0;i<selections.size();i++)
					{
						EntityId entity = svframe->m_sceneTree->itemIdToEntityId(selections[i]);
						if (entity.type==ST_OBJECT)
						{
							const rr::RRObject* object = solver->getObject(entity.index);
							if (object && object->faceGroups.size())
							{
								uberProgramSetup.useWorldMatrix(program,object);
								const rr::RRMesh* mesh = object->getCollider()->getMesh();
								FaceGroupRange fgRange(0,0,object->faceGroups.size()-1,0,mesh->getNumTriangles());
								RendererOfMesh* rendererOfMesh = solver->getRendererOfScene()->getRendererOfMesh(mesh);
								rendererOfMesh->render(program,object,&fgRange,1,uberProgramSetup,NULL,NULL);
							}
						}
					}
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}
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
		if (svs.renderHelpers && !_takingSshot && lightField)
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
				vignetteImage = rr::RRBuffer::load(RR_WX2RR(wxString::Format("%s../maps/sv_vignette.png",svs.pathToShaders)));
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
					logoImage = rr::RRBuffer::load(RR_WX2RR(wxString::Format("%s../maps/sv_logo.png",svs.pathToShaders)));
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


		if (svs.renderGrid || s_ciRenderCrosshair)
		{
			// set line shader
			{
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,NULL);
			}

			// render crosshair, using previously set shader
			if (s_ciRenderCrosshair)
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
		if (entityIcons->isOk() && !_takingSshot)
		{
			renderedIcons.clear();
			if (svframe->m_lightProperties->IsShown())
				renderedIcons.addLights(solver->getLights(),sunIconPosition);
			renderedIcons.markSelected(svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED));
			entityIcons->renderIcons(renderedIcons,svs.eye);
		}
	}

	if ((svs.renderHelpers && !_takingSshot)
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
		static rr::RRTime time;
		bool displayPhysicalMaterials = fmod(time.secondsPassed(),8)>4;
		const rr::RRObject* multiObject = displayPhysicalMaterials ? solver->getMultiObjectPhysical() : solver->getMultiObjectCustom();
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;

		// gather information about selected object
		rr::RRObject* singleObject = solver->getObject(svs.selectedObjectIndex);
		const rr::RRMesh* singleMesh = singleObject ? singleObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesSingle = singleMesh ? singleMesh->getNumTriangles() : 0;

		// gather information about selected point and triangle (pointed by mouse)
		bool                        selectedPointValid = false; // true = all selectedXxx below are valid
		const rr::RRObject*         selectedPointObject = NULL;
		const rr::RRMesh*           selectedPointMesh = NULL;
		rr::RRMesh::TangentBasis    selectedPointBasis;
		rr::RRMesh::TriangleBody    selectedTriangleBody;
		rr::RRMesh::TriangleNormals selectedTriangleNormals;
		if (multiMesh && (!svs.renderLightmaps2d || !lv))
		{
			// ray and collisionHandler are used in this block
			ray->rayOrigin = svs.eye.getRayOrigin(mousePositionInWindow);
			ray->rayDir = svs.eye.getRayDirection(mousePositionInWindow).normalized();
			ray->rayLengthMin = svs.eye.getNear();
			ray->rayLengthMax = svs.eye.getFar();
			ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			ray->hitObject = solver->getMultiObjectCustom(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
			ray->collisionHandler = collisionHandler;
			if (solver->getCollider()->intersect(ray))
			{
				selectedPointValid = true;
				selectedPointObject = ray->hitObject;
				selectedPointMesh = selectedPointObject->getCollider()->getMesh();
				const rr::RRMatrix3x4& hitMatrix = selectedPointObject->getWorldMatrixRef();
				selectedPointMesh->getTriangleBody(ray->hitTriangle,selectedTriangleBody);
				selectedPointMesh->getTriangleNormals(ray->hitTriangle,selectedTriangleNormals);
				hitMatrix.transformPosition(selectedTriangleBody.vertex0);
				hitMatrix.transformDirection(selectedTriangleBody.side1);
				hitMatrix.transformDirection(selectedTriangleBody.side2);
				for (unsigned i=0;i<3;i++)
				{
					hitMatrix.transformDirection(selectedTriangleNormals.vertex[i].normal);
					hitMatrix.transformDirection(selectedTriangleNormals.vertex[i].tangent);
					hitMatrix.transformDirection(selectedTriangleNormals.vertex[i].bitangent);
				}
				selectedPointBasis.normal = selectedTriangleNormals.vertex[0].normal + (selectedTriangleNormals.vertex[1].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[1];
				selectedPointBasis.tangent = selectedTriangleNormals.vertex[0].tangent + (selectedTriangleNormals.vertex[1].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[1];
				selectedPointBasis.bitangent = selectedTriangleNormals.vertex[0].bitangent + (selectedTriangleNormals.vertex[1].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[1];
			}
		}

		// render debug rays, using previously set shader
		if (svs.renderHelpers && !_takingSshot && (!svs.renderLightmaps2d || !lv) && SVRayLog::size)
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
		if (svs.renderHelpers && !_takingSshot && !svs.renderLightmaps2d)
		{
			drawTangentBasis(selectedTriangleBody.vertex0,selectedTriangleNormals.vertex[0]);
			drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side1,selectedTriangleNormals.vertex[1]);
			drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side2,selectedTriangleNormals.vertex[2]);
			drawTangentBasis(ray->hitPoint3d,selectedPointBasis);
			drawTriangle(selectedTriangleBody);
		}


		// render helper text, using own shader (because text output ignores color passed to line shader)
		if (svs.renderHelpers && !_takingSshot)
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
			unsigned numObjects = solver->getStaticObjects().size()+solver->getDynamicObjects().size();
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
					if (solver->getObject(i)->illumination.getLayer(svs.staticLayerNumber))
					{
						if (solver->getObject(i)->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_VERTEX_BUFFER) numVbufs++; else
						if (solver->getObject(i)->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE) numLmaps++;
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
				textOutput(x,y+=18,h,"lighting direct=%s indirect=%s%s lightmaps=(%dx vbuf %dx lmap %dx none) solver=%s",
					strDirect,strIndirect,svs.renderLDMEnabled()?"+LDM":"",numVbufs,numLmaps,numObjects-numVbufs-numLmaps,strSolver);
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
				// analyzes data from rarely used feature: lighting and shadowing only selected objects/combinations of objects
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
								if (multiObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowCasters++;
							}
						}
					}
					textOutput(x,y+=18*2,h,"[light %d/%d]",svs.selectedLightIndex,solver->realtimeLights.size());
					textOutput(x,y+=18,h,"triangles lit: %d/%d",numLightReceivers,numTrianglesMulti);
					textOutput(x,y+=18,h,"triangles casting shadow: %f/%d",numShadowCasters/float(numObjects),numTrianglesMulti);
				}
			}
			if (singleMesh)
			{
				textOutput(x,y+=18*2,h,"[object %d/%d]",svs.selectedObjectIndex,numObjects);
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
								if (singleObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowsCast++;
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
				if (solver->getObject(svs.selectedObjectIndex))
				{
					rr::RRBuffer* bufferSelectedObj = solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(svs.staticLayerNumber);
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
					rr::RRMesh::PreImportNumber preTriangle = selectedPointMesh->getPreImportTriangle(ray->hitTriangle);
					const rr::RRMaterial* selectedTriangleMaterial = selectedPointObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
					const rr::RRMaterial* material = selectedTriangleMaterial;
					rr::RRPointMaterial selectedPointMaterial;
					if (material && material->minimalQualityForPointMaterials<10000)
					{
						selectedPointObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,selectedPointMaterial);
						material = &selectedPointMaterial;
					}
					rr::RRMesh::TriangleMapping triangleMapping;
					selectedPointMesh->getTriangleMapping(ray->hitTriangle,triangleMapping,material?material->lightmapTexcoord:0);
					rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray->hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray->hitPoint2d[1];
					textOutput(x,y+=18*2,h,"[pointed by mouse]");
					if (selectedPointObject->isDynamic)
						textOutput(x,y+=18,h,"dynamic object: %ls",selectedPointObject->name.w_str());
					else
						textOutput(x,y+=18,h,"static object: %d/%d",preTriangle.object,solver->getStaticObjects().size());
					rr::RRBuffer* selectedPointLightmap = selectedPointObject->illumination.getLayer(svs.staticLayerNumber);
					textOutput(x,y+=18,h,"offline lightmap: %s %dx%d",selectedPointLightmap?(selectedPointLightmap->getType()==rr::BT_2D_TEXTURE?"per-pixel":"per-vertex"):"none",selectedPointLightmap?selectedPointLightmap->getWidth():0,selectedPointLightmap?selectedPointLightmap->getHeight():0);
					if (selectedPointObject->isDynamic)
					{
						textOutput(x,y+=18,h,"triangle in object: %d/%d",ray->hitTriangle,selectedPointMesh->getNumTriangles());
						textOutput(x,y+=18,h,"triangle in static scene:");
					}
					else
					{
						textOutput(x,y+=18,h,"triangle in object: %d/%d",preTriangle.index,solver->getObject(preTriangle.object)->getCollider()->getMesh()->getNumTriangles());
						textOutput(x,y+=18,h,"triangle in static scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
					}
					textOutput(x,y+=18,h,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
					textOutput(x,y+=18,h,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
					if (selectedPointLightmap && selectedPointLightmap->getType()==rr::BT_2D_TEXTURE)
					{
						int i = int(uvInLightmap[0]*selectedPointLightmap->getWidth());
						int j = int(uvInLightmap[1]*selectedPointLightmap->getHeight());
						textOutput(x,y+=18,h,"ij in lightmap: %d %d",i,j);
						if (i>=0 && i<(int)selectedPointLightmap->getWidth() && j>=0 && j<(int)selectedPointLightmap->getHeight())
						{
							// diagnose texel
							centerObject = preTriangle.object;
							centerTexel = i + j*selectedPointLightmap->getWidth();
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
							if (multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,solver->getObject(j))) numShadowsCast++;
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
				if (solver->getObject(svs.selectedObjectIndex))
				{
					rr::RRBuffer* buffer = solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(svs.staticLayerNumber);
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
			helpImage = rr::RRBuffer::load(RR_WX2RR(wxString::Format("%s../maps/sv_help.png",svs.pathToShaders)));
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


BEGIN_EVENT_TABLE(SVCanvas, wxGLCanvas)
    EVT_SIZE(SVCanvas::OnSize)
    EVT_PAINT(SVCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(SVCanvas::OnEraseBackground)
    EVT_KEY_DOWN(SVCanvas::OnKeyDown)
    EVT_KEY_UP(SVCanvas::OnKeyUp)
    EVT_MOUSE_EVENTS(SVCanvas::OnMouseEvent)
	EVT_CONTEXT_MENU(SVCanvas::OnContextMenuCreate)
    EVT_IDLE(SVCanvas::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
