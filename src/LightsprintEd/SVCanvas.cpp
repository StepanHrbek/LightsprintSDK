// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
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
#include "Lightsprint/GL/PreserveState.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif
#include <cctype>
#include <boost/filesystem.hpp> // extension()==".gsa"

//#define SUPPORT_GL_ES

namespace bf = boost::filesystem;

#if !wxUSE_GLCANVAS
    #error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
	#define REPORT_HEAP_STATISTICS // reports num allocations per frame
#endif

namespace rr_ed
{

bool s_es = false; // todo: access s_es from Shader.h


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
	WX_GL_SAMPLE_BUFFERS, GL_TRUE, // makes no difference in Windows, is necessary in OSX for antialiasing
	WX_GL_SAMPLES, 4, // antialiasing. can be later disabled by glDisable(GL_MULTISAMPLE), but it doesn't improve speed (tested on X1650). it must be disabled here (change 4 to 1) for higher fps
	WX_GL_DEPTH_SIZE, 24, // default is 16, explicit 24 should reduce z-fight. 32 fails on all cards tested including hd5870 and gf460 (falls back to default without antialiasing)
	0, 0};

SVCanvas::SVCanvas( SceneViewerStateEx& _svs, SVFrame *_svframe, wxSize _size)
	: wxGLCanvas(_svframe, wxID_ANY, s_attribList, wxDefaultPosition, _size, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE|wxWANTS_CHARS, "GLCanvas"), svs(_svs)
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

	ssgiLoadAttempted = false;
	ssgi = NULL;

	dofLoadAttempted = false;
	dof = NULL;

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
	selectedTransformation = IC_MOVEMENT;

	previousLightIndirect = LI_NONE;

}

class SVContext : public wxGLContext
{
public:
#ifdef SUPPORT_GL_ES
	// Es works only with WGL_EXT_create_context_es2_profile, i.e. Nvidia only.
	// For AMD, one has to link with AMD OpenGL ES SDK, not supported here.
#endif
	SVContext(wxGLCanvas* win, bool core, bool debug, bool es)
		: wxGLContext(win)
	{
		SetCurrent(*win);

#ifdef SUPPORT_GL_ES
#ifdef _WIN32
		// now that we have default context, we can try to modify it
		if (es || debug || core)
		if (glewInit()==GLEW_OK && WGL_ARB_create_context)
		{
			std::vector<int> attribList;
			if (core)
			{
				attribList.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
				attribList.push_back(3);
				attribList.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
				attribList.push_back(3);
				attribList.push_back(WGL_CONTEXT_FLAGS_ARB);
				attribList.push_back(WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);
				attribList.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
				attribList.push_back(WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
			}
			if (debug)
			{
				attribList.push_back(WGL_CONTEXT_FLAGS_ARB);
				attribList.push_back(WGL_CONTEXT_DEBUG_BIT_ARB);
			}
			if (es && WGL_EXT_create_context_es2_profile)
			{
				attribList.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
				attribList.push_back(2);
				attribList.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
				attribList.push_back(0);
				attribList.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
				attribList.push_back(WGL_CONTEXT_ES2_PROFILE_BIT_EXT); // overwrites 'core' request, can't be combined with ES
			}
			attribList.push_back(0);
			HGLRC betterContext = wglCreateContextAttribsARB(win->GetHDC(),m_glContext,&attribList[0]);
			if (betterContext)
			{
				wglMakeCurrent(win->GetHDC(),betterContext);
				wglDeleteContext(m_glContext);
				m_glContext = betterContext;
				if (debug && glewInit()==GLEW_OK)
				{
					if (glDebugMessageCallbackARB)
					{
						glDebugMessageCallbackARB(debugCallback, NULL);
						glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
						glDebugMessageControlARB(GL_DONT_CARE,GL_DEBUG_TYPE_PERFORMANCE_ARB,GL_DONT_CARE,0,NULL,GL_FALSE);
					}
					else
						rr::RRReporter::report(rr::WARN,"OpenGL debug messages not supported by driver.\n");
				}
			}
			else
				rr::RRReporter::report(rr::WARN,"Requested OpenGL context not supported by driver.\n");
		}
#endif
#endif // SUPPORT_GL_ES
	}
#ifdef SUPPORT_GL_ES
#ifdef _WIN32
	static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
	{
		rr::RRReporter::report(
			//(type==GL_DEBUG_TYPE_ERROR_ARB || type==GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB)?rr::ERRO:((type==GL_DEBUG_TYPE_PERFORMANCE_ARB||type==GL_DEBUG_TYPE_PORTABILITY_ARB)?rr::WARN:rr::INF2),
			(severity==GL_DEBUG_SEVERITY_HIGH_ARB)?rr::ERRO:((severity==GL_DEBUG_SEVERITY_MEDIUM_ARB)?rr::WARN:rr::INF2),
			"%s\n",message);
		if (severity==GL_DEBUG_SEVERITY_HIGH_ARB)
		{
			int i=1;
		}
	}
#endif
#endif // SUPPORT_GL_ES
};


void SVCanvas::createContextCore()
{
	context = new SVContext(this,false,false,false);
	SetCurrent(*context);

#ifdef REPORT_HEAP_STATISTICS
	s_oldAllocHook = _CrtSetAllocHook(newAllocHook);
#endif

	const char* error = rr_gl::initializeGL();
	if (error)
	{
		rr::RRReporter::report(rr::ERRO,error);
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


	{
		int viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);
		winWidth = viewport[2];
		winHeight = viewport[3];
	}


	// init solver
	solver = new rr_gl::RRDynamicSolverGL(RR_WX2RR(svs.pathToShaders));
	textureRenderer = solver->getRendererOfScene()->getTextureRenderer();
	solver->setScaler(rr::RRScaler::createRgbScaler());
	if (svs.initialInputSolver)
	{
		delete solver->getScaler();
		solver->setScaler(svs.initialInputSolver->getScaler());
		solver->setEnvironment(svs.initialInputSolver->getEnvironment(0),svs.initialInputSolver->getEnvironment(1),svs.skyboxRotationRad,svs.skyboxRotationRad);
		envToBeDeletedOnExit = false;
		solver->setStaticObjects(svs.initialInputSolver->getStaticObjects(),NULL,NULL,rr::RRCollider::IT_BSP_FASTER,svs.initialInputSolver); // smoothing and multiobject are taken from _solver
		solver->setDynamicObjects(svs.initialInputSolver->getDynamicObjects());
		solver->setLights(svs.initialInputSolver->getLights());
		solver->setDirectIllumination(svs.initialInputSolver->getDirectIllumination());
	}
	else
	if (!svs.sceneFilename.empty())
	{
		// load scene
		rr::RRScene* scene = svframe->loadScene(svs.sceneFilename,svframe->userPreferences.import.getUnitLength(svs.sceneFilename),svframe->userPreferences.import.getUpAxis(svs.sceneFilename),true);
		mergedScenes.push_back(scene);

		if (scene->cameras.size())
		{
			svs.camera = scene->cameras[0];
			svs.autodetectCamera = false; // camera was taken from scene
		}

		// send everything to solver
		solver->setEnvironment(mergedScenes[0]->environment,NULL,svs.skyboxRotationRad,svs.skyboxRotationRad);
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


	solver->observer = &svs.camera; // solver automatically updates lights that depend on camera

	// make unique object names, so that lightmaps are loaded from different files
	rr::RRObjects allObjects = solver->getObjects();
	allObjects.makeNamesUnique();

	if (solver->getStaticObjects().size())
	{
		rr::RRReportInterval report(rr::INF3,"Setting illumination type...\n");
		switch (svs.renderLightIndirect)
		{
			case LI_REALTIME_FIREBALL:
				// try to load FB. if not found, create it
				svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_FIREBALL);
				break;
			case LI_REALTIME_ARCHITECT:
				// create architect
				svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_ARCHITECT);
				break;
			default:
				// no action needed for other modes. just to avoid warning
				break;
		}
	}

	// try to load lightmaps
	if (!allObjects.layerExistsInMemory(svs.layerBakedLightmap))
		allObjects.loadLayer(svs.layerBakedLightmap,LAYER_PREFIX,LMAP_POSTFIX);

	// try to load ambient maps
	if (!allObjects.layerExistsInMemory(svs.layerBakedAmbient))
		allObjects.loadLayer(svs.layerBakedAmbient,LAYER_PREFIX,AMBIENT_POSTFIX);

	// try to load LDM. if not found, disable it
	if (!allObjects.layerExistsInMemory(svs.layerBakedLDM))
		if (!allObjects.loadLayer(svs.layerBakedLDM,LAYER_PREFIX,LDM_POSTFIX))
			svs.renderLDM = false;

	// try to load cubemaps
	if (!allObjects.layerExistsInMemory(svs.layerBakedEnvironment))
		allObjects.loadLayer(svs.layerBakedEnvironment,LAYER_PREFIX,ENV_POSTFIX);

	// init rest
	rr::RRReportInterval report(rr::INF3,"Initializing the rest...\n");
	if (svs.selectedLightIndex>=solver->getLights().size()) svs.selectedLightIndex = 0;
	if (svs.selectedObjectIndex>=solver->getStaticObjects().size()) svs.selectedObjectIndex = 0;
	lightFieldQuadric = gluNewQuadric();
	lightFieldObjectIllumination = new rr::RRObjectIllumination;
	lightFieldObjectIllumination->getLayer(svs.layerBakedEnvironment) = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,NULL);
	entityIcons = new SVEntityIcons(svs.pathToMaps,solver->getUberProgram());
	recalculateIconSizeAndPosition();

#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif

	toneMapping = new rr_gl::ToneMapping();
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

void SVCanvas::recalculateIconSizeAndPosition()
{
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

void SVCanvas::addOrRemoveScene(rr::RRScene* scene, bool add, bool staticObjectsModified)
{
	if (scene)
		for (unsigned i=0;i<scene->objects.size();i++)
			if (!scene->objects[i]->isDynamic)
				staticObjectsModified = true;

	if (scene || add)
	{
		// add or remove scene from solver, or update after objects in solver change
		rr::RRObjects objects = solver->getObjects();
		rr::RRLights lights = solver->getLights();
		if (scene)
		{
			if (add)
			{
				objects.insert(objects.end(),scene->objects.begin(),scene->objects.end());
				lights.insert(lights.end(),scene->lights.begin(),scene->lights.end());
				if (!solver->getEnvironment() && scene->environment)
					solver->setEnvironment(scene->environment,NULL,svs.skyboxRotationRad);
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
		objects.removeEmptyObjects();
		if (staticObjectsModified)
			solver->setStaticObjects(objects,NULL);
		solver->setDynamicObjects(objects);
		solver->setLights(lights);
	}
	else
	{
		solver->reportDirectIlluminationChange(-1,true,true,true);
	}

	// fix svs.renderLightIndirect, setStaticObjects() just silently switched solver to architect
	// must be changed if setStaticObjects() behaviour changes
	// we don't switch to architect, but rather to const ambient, because architect ignores environment, scenes without lights are black
	if (staticObjectsModified && svs.renderLightIndirect==LI_REALTIME_FIREBALL)
	{
		svs.renderLightIndirect = LI_CONSTANT;
	}

	// make unique object names, so that lightmaps are saved to different files
	solver->getObjects().makeNamesUnique(); // updateAllPanels() must follow, to refresh names in scene tree

	// object numbers did change, change also svs.selectedObjectIndex so that it points to object in objprops
	// and following updateAllPanels() does not change objprops
	svs.selectedObjectIndex = 0;
	for (unsigned i=solver->getStaticObjects().size()+solver->getDynamicObjects().size();i--;)
		if (solver->getObject(i)==svframe->m_objectProperties->object)
			svs.selectedObjectIndex = i;

	// fix dangling pointers in light properties, object properties etc
	svframe->updateAllPanels();
	// fix dangling pointer in material properties (materialPhysical points to freed memory after smoothAndStitch())
	// sideeffect: disables [x]point [x]phys
	// this is not part of updateAllPanels() because updateAllPanels() is called too often (e.g. after right click in viewport) and sideeffects hurt
	svframe->m_materialProperties->setMaterial(svframe->m_materialProperties->materialCustom);

	// fix dangling pointer in collisionHandler
	delete collisionHandler;
	collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();
	// now that we have new collisionHandler, fix dangling pointer in ray
	ray->collisionHandler = collisionHandler;

	// alloc rtgi buffers, otherwise new objects would have no realtime indirect
	// (it has to be called even when removing static objects, because multiobject is rebuilt and has no rtgi buffer;
	//  if we don't call it, optimized render that uses multiobject would have no indir from FB/architect, optimized render usually shows up after disabling cubes/mirrors)
	reallocateBuffersForRealtimeGI(true);

	recalculateIconSizeAndPosition();

}

void SVCanvas::reallocateBuffersForRealtimeGI(bool reallocateAlsoVbuffers)
{
	solver->allocateBuffersForRealtimeGI(
		reallocateAlsoVbuffers?svs.layerRealtimeAmbient:-1,
		svs.layerRealtimeEnvironment,
		4,svs.raytracedCubesRes,svs.raytracedCubesRes, // allocates and preserves all cubes, even if diffuse, specular or refraction are disabled at the moment (=possibly more cubes and slower updates, but simpler and less error prone)
		true,true,svs.raytracedCubesSpecularThreshold,svs.raytracedCubesDepthThreshold);
	svframe->m_objectProperties->updateProperties();
}

SVCanvas::~SVCanvas()
{
	// don't process events, if they are still coming
	fullyCreated = false;

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

	rr_gl::deleteAllTextures();
	// delete objects referenced by solver
	if (solver)
	{
		// delete all lightmaps for realtime rendering
		for (unsigned i=0;i<solver->getStaticObjects().size()+solver->getDynamicObjects().size();i++)
		{
			RR_SAFE_DELETE(solver->getObject(i)->illumination.getLayer(svs.layerRealtimeAmbient));
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
			if (svs.renderDDI)
			{
				svs.renderDDI = 0;
			}
			else
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

extern bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption);

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
	rr::RRVec2 hitPoint2d; // Click coordinate in triangle space defined so that vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1.
	rr::RRVec3 hitPoint3d;
	SVEntity clickedEntity;
	bool clickedEntityIsSelected;
};
// set by OnMouseEvent on click (but not set on click that closes menu), valid and constant during drag
// read also by OnPaint (paints crosshair)
static bool s_ciRelevant = false; // are we dragging and is s_ci describing click that started it?
static bool s_ciRenderCrosshair = false;
static ClickInfo s_ci;
static rr::RRVec3 s_accumulatedPanning(0); // accumulated worldspace movement in 'dragging' path, cleared at the end of dragging

// What triggers context menu:
// a) rightclick -> OnMouseEvent() -> creates context menu
// b) hotkey -> OnContextMenuCreate() -> OnMouseEvent(event with ID_CONTEXT_MENU) -> creates context menu
#define ID_CONTEXT_MENU 543210

void SVCanvas::OnMouseEvent(wxMouseEvent& event)
{
	if (exitRequested || !fullyCreated)
		return;

	// when clicking faster than 5Hz, wxWidgets 2.9.1 drops some ButtonDown events
	// let's keep an eye on unexpected ButtonUps and generate missing ButtonDowns 
	#define EYE_ON(button,BUTTON) \
		static bool button##Down = false; \
		if (event.button##Down()) button##Down = true; \
		if (event.button##Up() && !button##Down) {wxMouseEvent e(event);e.SetEventType(wxEVT_##BUTTON##_DOWN);OnMouseEvent(e);} \
		if (event.button##Up()) button##Down = false;
	EYE_ON(Left,LEFT);
	EYE_ON(Middle,MIDDLE);
	EYE_ON(Right,RIGHT);

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
	mousePositionInWindow = rr::RRVec2((newPosition.x*2.0f+winWidth-GetSize().x)/winWidth-1,(newPosition.y*2.0f+winHeight-GetSize().y)/winHeight-1); // in fact, it is mouse position in _viewport_ where viewport winWidth*winHeight is in center of window GetSize()

	if (!winWidth || !winHeight || !solver)
	{
		return;
	}
	if (svs.renderLightmaps2d)
	{
		lv.OnMouseEvent(event,GetSize());
		return;
	}

	const EntityIds& selectedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);

	const SVEntity* intersectedEntity = entityIcons->intersectIcons(renderedIcons,mousePositionInWindow);
	SetCursor(wxCursor(intersectedEntity?wxCursor(wxCURSOR_HAND):wxNullCursor));

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
		rr::RRCamera* cam = (event.LeftIsDown()
			 && selectedType==ST_LIGHT && svs.selectedLightIndex<solver->getLights().size()) ? solver->realtimeLights[svs.selectedLightIndex]->getCamera() : &svs.camera;
		s_ci.pos = svs.camera.getPosition();
		s_ci.rayOrigin = svs.camera.getRayOrigin(mousePositionInWindow);
		s_ci.rayDirection = svs.camera.getRayDirection(mousePositionInWindow);

		// find scene distance, adjust search range to look only for closer icons
		s_ci.hitDistance = svs.camera.getNear()*0.9f+svs.camera.getFar()*0.1f;
		ray->rayOrigin = s_ci.rayOrigin;
		rr::RRVec3 directionToMouse = s_ci.rayDirection;
		float directionToMouseLength = directionToMouse.length();
		ray->rayDir = directionToMouse/directionToMouseLength;
		ray->rayLengthMin = svs.camera.getNear()*directionToMouseLength;
		ray->rayLengthMax = svs.camera.getFar()*directionToMouseLength;
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
		if (intersectedEntity)
		{
			s_ci.clickedEntity = *intersectedEntity; // ST_FIRST for gizmo icons
			s_ci.hitTriangle = UINT_MAX; // must be cleared, otherwise we use it to look up point material
			s_ci.hitDistance = (intersectedEntity->position-svs.camera.getPosition()).length();
			s_ci.hitPoint2d = rr::RRVec2(0);
			s_ci.hitPoint3d = intersectedEntity->position;
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

		s_ci.clickedEntityIsSelected = selectedEntities.find(s_ci.clickedEntity)!=selectedEntities.end(); // false for gizmo
	}

	// handle clicking (mouse released in less than 0.2s in less than 20pix distance)
	bool clicking = s_ci.time.secondsPassed()<0.2f && abs(event.GetX()-s_ci.mouseX)<20 && abs(event.GetY()-s_ci.mouseY)<20;
	if (contextMenu)
		goto context_menu;
	if ((event.LeftUp() || event.RightUp()) && clicking)
	{
		if (event.LeftUp())
		{
			// left click = select
			if (s_ci.clickedEntity.iconCode==IC_MOVEMENT)
			{
				selectedTransformation = IC_ROTATION;
			}
			else
			if (s_ci.clickedEntity.iconCode==IC_ROTATION)
			{
				selectedTransformation = IC_SCALE;
			}
			else
			if (s_ci.clickedEntity.iconCode==IC_SCALE)
			{
				selectedTransformation = IC_MOVEMENT;
			}
			else
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
			context_menu:;
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
		bool manipulatingCamera = manipulatedEntities==svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_CAMERA);
		bool manipulatingSelection = s_ci.clickedEntityIsSelected && !manipulatingCamera;
		bool manipulatingSingleLight = selectedEntities.size()==1 && selectedEntities.begin()->type==ST_LIGHT;
		bool manipulatingGizmo = s_ci.clickedEntity.iconCode>=IC_MOVEMENT && s_ci.clickedEntity.iconCode<=IC_Z;
		if (event.LeftIsDown() && !manipulatingCamera && (manipulatingSelection || manipulatingGizmo))
		{
			// moving/rotating/scaling selection (gizmo)
			rr::RRMatrix3x4 transformation;
			bool preTransform = false;
			rr::RRVec3 pan = svs.camera.isOrthogonal()
				? svs.camera.getRayOrigin(mousePositionInWindow)-svs.camera.getRayOrigin(oldMousePositionInWindow)
				: (svs.camera.getRayDirection(mousePositionInWindow)-svs.camera.getRayDirection(oldMousePositionInWindow))*(s_ci.hitDistance/s_ci.rayDirection.length());
			rr::RRVec2 drag = RRVec2((newPosition.x-oldPosition.x)/(float)winWidth,(newPosition.y-oldPosition.y)/(float)winHeight);
			switch (selectedTransformation)
			{
				case IC_MOVEMENT:
					switch (s_ci.clickedEntity.iconCode)
					{
						case IC_X: pan.y = pan.z = 0; break;
						case IC_Y: pan.x = pan.z = 0; break;
						case IC_Z: pan.x = pan.y = 0; break;
						default: break; // just to prevent warning
					}
					s_accumulatedPanning += pan;
					transformation = rr::RRMatrix3x4::translation(pan);
					break;
				case IC_ROTATION:
					switch (s_ci.clickedEntity.iconCode)
					{
						case IC_X: transformation = rr::RRMatrix3x4::rotationByAxisAngle(RRVec3(1,0,0),-drag.x*5); break;
						case IC_Y: transformation = rr::RRMatrix3x4::rotationByAxisAngle(RRVec3(0,1,0),-drag.x*5); break;
						case IC_Z: transformation = rr::RRMatrix3x4::rotationByAxisAngle(RRVec3(0,0,1),-drag.x*5); break;
						default:
							transformation = rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getUp(),(manipulatingSingleLight?5:-5)*drag.x)
								*rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getRight(),(manipulatingSingleLight?5:-5)*drag.y);
							break;
					}
					break;
				case IC_SCALE:
					switch (s_ci.clickedEntity.iconCode)
					{
						case IC_X: pan.y = pan.z = 0; break;
						case IC_Y: pan.x = pan.z = 0; break;
						case IC_Z: pan.x = pan.y = 0; break;
						default: pan = RRVec3(pan.sum()); break;
					}
					for (unsigned i=0;i<3;i++) pan[i] = powf(1.6f,pan[i]);
					transformation = rr::RRMatrix3x4::scale(pan);
					if (manipulatedEntities.size()==1)
						preTransform = true;
					break;
				default:
					// should not get here, just to prevent warning
					break;
			}
			svframe->m_sceneTree->manipulateEntities(manipulatedEntities,preTransform?transformation:transformation.centeredAround(manipulatedCenter),preTransform,false);
		}
		else
		if (event.LeftIsDown())
		{
			// rotating camera
			float dragX = (newPosition.x-oldPosition.x)/(float)winWidth;
			float dragY = (newPosition.y-oldPosition.y)/(float)winHeight;
			while (!svframe->m_sceneTree->manipulateEntity(EntityId(ST_CAMERA,0),
				(rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,1,0),dragX*5*svs.camera.getFieldOfViewHorizontalDeg()/90)
				*rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getRight(),dragY*5*svs.camera.getFieldOfViewHorizontalDeg()/90))
				.centeredAround(svs.camera.getPosition()),
				false,false) && dragY)
			{
				// disable Y component and try rotation again, maybe this time pitch won't overflow 90/-90
				// this makes rotation smoother when looking straight up/down
				dragY = 0;
			}
		}
		else
		if (event.MiddleIsDown())
		{
			// panning
			//  drag clicked pixel so that it stays under mouse
			rr::RRVec3 pan;
			if (svs.camera.isOrthogonal())
			{
				rr::RRVec2 origMousePositionInWindow = rr::RRVec2((s_ci.mouseX*2.0f+winWidth-GetSize().x)/winWidth-1,(s_ci.mouseY*2.0f+winHeight-GetSize().y)/winHeight-1); // in fact, it is mouse position in _viewport_ where viewport winWidth*winHeight is in center of window GetSize()
				pan = svs.camera.getRayOrigin(origMousePositionInWindow)-svs.camera.getRayOrigin(mousePositionInWindow);
			}
			else
			{
				rr::RRVec3 newRayDirection = svs.camera.getRayDirection(mousePositionInWindow);
				pan = (s_ci.rayDirection-newRayDirection)*(s_ci.hitDistance/s_ci.rayDirection.length());
			}
			svs.camera.setPosition(s_ci.pos + pan);
			s_ciRenderCrosshair = true;
		}
		else
		if (event.RightIsDown() && s_ci.hitPoint3d!=rr::RRVec3(0))
		{
			// inspection
			//  rotate around clicked point, point does not move on screen
			float dragX = (newPosition.x-oldPosition.x)/(float)winWidth;
			float dragY = (newPosition.y-oldPosition.y)/(float)winHeight;
			while (!svframe->m_sceneTree->manipulateEntity(EntityId(ST_CAMERA,0),
				(rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,1,0),dragX*5)
				*rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getRight(),dragY*5))
				.centeredAround(s_ci.hitPoint3d),false,false) && dragY)
			{
				dragY = 0;
			}
			s_ciRenderCrosshair = true;
		}

	}
	if (!event.ButtonDown() && !event.Dragging())
	{
		// dragging ended, all s_xxx become invalid
		s_ciRelevant = false;
		s_ciRenderCrosshair = false;
		// handle special combos
		if (s_accumulatedPanning!=rr::RRVec3(0))
		{
			rr::RRVec3 accumulatedPanning = s_accumulatedPanning;
			s_accumulatedPanning = rr::RRVec3(0); // must be zeroed early, otherwise this block is reentered before it is left
			if (event.ShiftDown() || event.ControlDown())
			{
				const EntityIds& manipulatedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
				bool manipulatingCamera = manipulatedEntities==svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_CAMERA);
				bool manipulatingSelection = s_ci.clickedEntityIsSelected && !manipulatingCamera;
				bool manipulatingGizmo = s_ci.clickedEntity.iconCode>=IC_MOVEMENT && s_ci.clickedEntity.iconCode<=IC_Z;
				unsigned manipulatingLights = 0;
				unsigned manipulatingObjects = 0;
				bool selectionContainsObjectOrLight = false;
				for (EntityIds::const_iterator i=manipulatedEntities.begin();i!=manipulatedEntities.end();++i)
				{
					if (i->type==ST_LIGHT) manipulatingLights++;
					if (i->type==ST_OBJECT) manipulatingObjects++;
				}
				if ((manipulatingSelection || manipulatingGizmo) && manipulatingLights+manipulatingObjects)
				{
					// ask for number N
					static float numCopies = 5;
					if (event.ShiftDown() || getFactor(svframe,numCopies,_("How many times to multiply selected objects and lights?\n(You can unshare materials by minus sign.)"),_("Selection multiplier")))
					{
						bool explodeMaterials = numCopies<0;
						numCopies = event.ShiftDown() ? 2 : RR_MAX(1,(int)abs(numCopies));
						// multiply selection N times
						rr::RRLights lights = solver->getLights();
						rr::RRObjects objects = solver->getObjects();
						for (unsigned j=1;j<numCopies;j++)
						{
							for (EntityIds::const_iterator i=manipulatedEntities.begin();i!=manipulatedEntities.end();++i)
							{
								if (i->type==ST_LIGHT)
								{
									rr::RRLight* newLight = new rr::RRLight(*lights[i->index]);
									newLight->position -= accumulatedPanning*(j/(numCopies-1.0f));
									svframe->m_canvas->lightsToBeDeletedOnExit.push_back(newLight);
									lights.push_back(newLight);
								}
								else
								if (i->type==ST_OBJECT)
								{
									rr::RRObject* newObject = new rr::RRObject; // memleak, never deleted
									newObject->setCollider(objects[i->index]->getCollider());
									newObject->name = objects[i->index]->name;
									newObject->faceGroups = objects[i->index]->faceGroups;
									if (explodeMaterials)
									{
										for (unsigned g=0;g<newObject->faceGroups.size();g++)
											if (newObject->faceGroups[g].material)
											{
												// new material instance is created here because we need it for matlib template right now,
												// it might be better to not create new material in future, or make it optional
												rr::RRMaterial* newMaterial = new rr::RRMaterial; // memleak, never deleted
												newMaterial->copyFrom(*newObject->faceGroups[g].material);
												newObject->faceGroups[g].material = newMaterial;
											}
									}
									newObject->isDynamic = true;
									rr::RRMatrix3x4 matrix = rr::RRMatrix3x4::translation(accumulatedPanning*(j/(1.0f-numCopies))) * objects[i->index]->getWorldMatrixRef();
									newObject->setWorldMatrix(&matrix);
									objects.push_back(newObject);
								}
							}
						}
						solver->setLights(lights); // RealtimeLight in light props is deleted here
						objects.makeNamesUnique();
						reallocateBuffersForRealtimeGI(false);
						solver->setDynamicObjects(objects);
						svframe->updateAllPanels();
					}
				}
			}
		}
	}

	// handle wheel
	if (event.GetWheelRotation())
	{
		int spin = -1;
		if (event.ControlDown())
		{
			// move camera forward/backward
			svs.camera.setPosition(svs.camera.getPosition() - svs.camera.getDirection() * (event.GetWheelRotation()*spin*svs.cameraMetersPerSecond/event.GetWheelDelta()/3));
		}
		else
		{
			// zoom camera
			if (svs.camera.isOrthogonal())
			{
				if (event.GetWheelRotation()*spin<0)
					svs.camera.setOrthoSize(svs.camera.getOrthoSize()/1.4f);
				if (event.GetWheelRotation()*spin>0)
					svs.camera.setOrthoSize(svs.camera.getOrthoSize()*1.4f);
			}
			else
			{
				float fov = svs.camera.getFieldOfViewVerticalDeg();
				if (event.GetWheelRotation()*spin<0)
				{
					if (fov>13) fov -= 10; else fov /= 1.4f;
				}
				if (event.GetWheelRotation()*spin>0)
				{
					if (fov*1.4f<=3) fov *= 1.4f; else if (fov<170) fov += 10;
				}
				svs.camera.setFieldOfViewVerticalDeg(fov);
			}
		}
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
		rr::RRCamera* cam = (selectedType!=ST_LIGHT)?&svs.camera:solver->realtimeLights[svs.selectedLightIndex]->getCamera();

		{
			// yes -> respond to keyboard
			const EntityIds& entities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
			rr::RRVec3 center = svframe->m_sceneTree->getCenterOf(entities);
			svframe->m_sceneTree->manipulateEntities(entities,
				rr::RRMatrix3x4::translation(
					svs.camera.getDirection() * ((speedForward-speedBack)*meters) +
					svs.camera.getRight() * ((speedRight-speedLeft)*meters) +
					svs.camera.getUp() * ((speedUp-speedDown)*meters) +
					rr::RRVec3(0,speedY*meters,0))
				* rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getDirection(),-speedLean*seconds*0.5f).centeredAround(center),
				false, speedLean?true:false
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
	if (!fontInited && !s_es)
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
		{
			// since switching back from DateTime(tm+nanoseconds) to plain tm, we need to remember fractions of seconds here, otherwise sun won't move at slow speeds
			static float secondsToAdd = 0;
			secondsToAdd += seconds*svs.envSpeed;
			float secondsFloor = floor(secondsToAdd);
			secondsToAdd -= secondsFloor;
			tm_addSeconds(svs.envDateTime,secondsFloor);
		}
		svframe->simulateSun();
	}

	Paint(false,"");

	// done
	SwapBuffers();
}

void SVCanvas::Paint(bool _takingSshot, const wxString& extraMessage)
{
#ifdef _MSC_VER
	__try
	{
#endif
		PaintCore(_takingSshot,extraMessage);
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


void SVCanvas::PaintCore(bool _takingSshot, const wxString& extraMessage)
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
	if (svs.renderLightmaps2d)
	{
		if (solver->getObject(svs.selectedObjectIndex))
			lv.setObject(
				solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(
					svs.selectedLayer ? svs.selectedLayer : (
						svs.renderLDMEnabled() ? svs.layerBakedLDM : ((svs.renderLightIndirect!=LI_BAKED || svs.renderLightDirect==LD_BAKED)?svs.layerBakedLightmap:svs.layerBakedAmbient))
						),
				solver->getObject(svs.selectedObjectIndex),
				svs.renderLightmapsBilinear);
		else
			lv.setObject(
				NULL,
				NULL,
				svs.renderLightmapsBilinear);
		lv.OnPaint(textureRenderer,GetSize());
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

		// attempt to render empty baked layer? fill it with data from realtime layer
		// (and save it to disk, users expect that what they see in "baked" mode is saved)
		if (previousLightIndirect!=svs.renderLightIndirect)
		{
			rr::RRObjects allObjects = solver->getObjects();
			if (svs.renderLightIndirect==LI_CONSTANT || svs.renderLightIndirect==LI_BAKED)
			{
				bool needsSave = false;
				for (unsigned i=0;i<allObjects.size();i++)
					if (!allObjects[i]->illumination.getLayer(svs.layerBakedEnvironment) && allObjects[i]->illumination.getLayer(svs.layerRealtimeEnvironment))
					{
						allObjects[i]->illumination.getLayer(svs.layerBakedEnvironment) = allObjects[i]->illumination.getLayer(svs.layerRealtimeEnvironment)->createReference();
						needsSave = true;
					}
				if (needsSave)
					allObjects.saveLayer(svs.layerBakedEnvironment,LAYER_PREFIX,ENV_POSTFIX);
			}
			if (svs.renderLightIndirect==LI_BAKED)
			{
				bool needsSave = false;
				for (unsigned i=0;i<allObjects.size();i++)
					if (!allObjects[i]->illumination.getLayer(svs.layerBakedAmbient) && allObjects[i]->illumination.getLayer(svs.layerRealtimeAmbient))
					{
						allObjects[i]->illumination.getLayer(svs.layerBakedAmbient) = allObjects[i]->illumination.getLayer(svs.layerRealtimeAmbient)->createReference();
						needsSave = true;
					}
				if (needsSave)
					allObjects.saveLayer(svs.layerBakedAmbient,LAYER_PREFIX,AMBIENT_POSTFIX);
			}
			previousLightIndirect = svs.renderLightIndirect;
		}


		// aspect needs update after
		// - OnSize()
		// - rendering enhanced screenshot
		// - RL: restored previously saved camera and window size differs
		// - RL: calculated camera from previously saved keyframes and window size differs
		svs.camera.setAspect(winWidth/(float)winHeight,0.5f);

		// move flashlight
		for (unsigned i=solver->getLights().size();i--;)
			if (solver->getLights()[i] && solver->getLights()[i]->enabled && solver->getLights()[i]->type==rr::RRLight::SPOT && solver->getLights()[i]->name=="Flashlight")
			{
				float viewportWidthCovered = 0.9f;
				rr::RRVec3 newPos = svs.camera.getPosition() + svs.camera.getUp()*svs.cameraMetersPerSecond*0.03f+svs.camera.getRight()*svs.cameraMetersPerSecond*0.03f;
				rr::RRVec3 newDir = svs.camera.getDirection();
				solver->realtimeLights[i]->getCamera()->setPosition(newPos);
				solver->realtimeLights[i]->getCamera()->setDirection(newDir);
				solver->getLights()[i]->outerAngleRad = svs.camera.getFieldOfViewHorizontalRad()*viewportWidthCovered*0.6f;
				solver->getLights()[i]->fallOffAngleRad = svs.camera.getFieldOfViewHorizontalRad()*viewportWidthCovered*0.4f;
				solver->realtimeLights[i]->getCamera()->setRange(svs.camera.getNear(),svs.camera.getFar());
				solver->realtimeLights[i]->updateAfterRRLightChanges(); // sets dirtyRange if pos/dir/fov did change
				solver->realtimeLights[i]->dirtyRange = false; // clear it, range already is good (dirty range would randomly disappear flashlight, reason unknown)
				solver->realtimeLights[i]->dirtyShadowmap = true;
			}

		if (svs.cameraDynamicNear && !svs.camera.isOrthogonal()) // don't change range in ortho, fixed range from setView() is better
		{
			// eye must already be updated here because next line depends on pos, up, right
			svs.camera.setRangeDynamically(solver);
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
				params.qualityIndirectDynamic = svs.fireballWorkPerFrame;
				params.qualityIndirectStatic = svs.fireballWorkTotal;
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

			rr_gl::RenderParameters rp;

			rp.camera = &svs.camera;

			rp.brightness = svs.renderTonemapping ? svs.tonemappingBrightness * pow(svs.tonemappingGamma,0.45f) : rr::RRVec4(1);
			rp.gamma = svs.renderTonemapping ?svs.tonemappingGamma : 1;

			rp.uberProgramSetup.SHADOW_MAPS = 1;
			rp.uberProgramSetup.LIGHT_DIRECT = svs.renderLightDirect==LD_REALTIME;
			rp.uberProgramSetup.LIGHT_DIRECT_COLOR = svs.renderLightDirect==LD_REALTIME;
			rp.uberProgramSetup.LIGHT_DIRECT_MAP = svs.renderLightDirect==LD_REALTIME;
			rp.uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = svs.renderLightDirect==LD_REALTIME;
			rp.uberProgramSetup.LIGHT_INDIRECT_CONST = svs.renderLightIndirect==LI_CONSTANT;
			rp.uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
			rp.uberProgramSetup.LIGHT_INDIRECT_MAP = svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			rp.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = svs.renderLDMEnabled();
			rp.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE =
			rp.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = svs.raytracedCubesEnabled && solver->getStaticObjects().size()+solver->getDynamicObjects().size()<svs.raytracedCubesMaxObjects;
			rp.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT = rp.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR && svs.renderMaterialTransparencyRefraction;
			rp.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = svs.mirrorsEnabled && svs.mirrorsDiffuse;
			rp.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = svs.mirrorsEnabled && svs.mirrorsSpecular;
			rp.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = svs.mirrorsEnabled && svs.mirrorsMipmaps;
			rp.uberProgramSetup.MATERIAL_DIFFUSE = true;
			rp.uberProgramSetup.MATERIAL_DIFFUSE_CONST = svs.renderMaterialDiffuse;
			rp.uberProgramSetup.MATERIAL_DIFFUSE_MAP = svs.renderMaterialDiffuse && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_SPECULAR = svs.renderMaterialSpecular;
			rp.uberProgramSetup.MATERIAL_SPECULAR_MAP = svs.renderMaterialSpecular && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_SPECULAR_CONST = svs.renderMaterialSpecular;
			rp.uberProgramSetup.MATERIAL_EMISSIVE_CONST = svs.renderMaterialEmission;
			rp.uberProgramSetup.MATERIAL_EMISSIVE_MAP = svs.renderMaterialEmission && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = svs.renderMaterialTransparency!=T_OPAQUE;
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = svs.renderMaterialTransparency!=T_OPAQUE && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = svs.renderMaterialTransparency!=T_OPAQUE;
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = svs.renderMaterialTransparency==T_ALPHA_BLEND || svs.renderMaterialTransparency==T_RGB_BLEND;
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = svs.renderMaterialTransparency==T_RGB_BLEND && !rp.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT; // renderer processes _TO_RGB as multipass, this is not necessary (and not working) with _REFRACT, therefore we disable _TO_RGB when _REFRACT
			rp.uberProgramSetup.MATERIAL_TRANSPARENCY_FRESNEL = svs.renderMaterialTransparencyFresnel;
			rp.uberProgramSetup.MATERIAL_BUMP_MAP = svs.renderMaterialBumpMaps && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_NORMAL_MAP_FLOW = svs.renderMaterialBumpMaps && svs.renderMaterialTextures;
			rp.uberProgramSetup.MATERIAL_CULLING = svs.renderMaterialSidedness;
			rp.uberProgramSetup.POSTPROCESS_BRIGHTNESS = rp.brightness!=rr::RRVec4(1);
			rp.uberProgramSetup.POSTPROCESS_GAMMA = rp.gamma!=1;
			// There was updateLayers=true by accident since rev 5221 (I think development version of mirrors needed it to update mirrors, and I forgot to revert it).
			// It was error, because "true" when rendering static lmap allows RendererOfScene [#12] to use multiobj.
			// And as lmap is always stored in 1obj, RendererOfScene can't find it in multiobj. Error was visible only with specular cubes disabled (they also enforce 1obj).
			rp.updateLayers = svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT;
			rp.layerLightmap = (svs.renderLightDirect==LD_BAKED)?svs.layerBakedLightmap:((svs.renderLightIndirect==LI_BAKED)?svs.layerBakedAmbient:svs.layerRealtimeAmbient);
			rp.layerEnvironment = svs.raytracedCubesEnabled?((svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)?svs.layerRealtimeEnvironment:svs.layerBakedEnvironment):UINT_MAX;
			rp.layerLDM = svs.renderLDMEnabled()?svs.layerBakedLDM:UINT_MAX;
			
			// in interlaced mode, check whether image starts on odd or even scanline
			if (svs.renderStereo)
			{
				rp.stereoMode = svframe->userPreferences.stereoMode;
				if (rp.stereoMode==rr_gl::SM_INTERLACED || rp.stereoMode==rr_gl::SM_INTERLACED_SWAP)
				{
					GLint viewport[4];
					glGetIntegerv(GL_VIEWPORT,viewport);
					int trueWinWidth, trueWinHeight;
					GetClientSize(&trueWinWidth, &trueWinHeight);
					if ((GetScreenPosition().y+trueWinHeight-viewport[1]-viewport[3])&1)
						rp.stereoMode = (rp.stereoMode==rr_gl::SM_INTERLACED)?rr_gl::SM_INTERLACED_SWAP:rr_gl::SM_INTERLACED;
				}
			}

			if (svs.renderPanorama)
			{
				rp.panoramaMode = svs.panoramaMode;
			}

			if (svs.renderDDI)
			{
				// allocate and fill vertex buffer with DDI illumination
				#define LAYER_DDI 1928374
				rr::RRObject* multiObject = solver->getMultiObjectCustom();
				if (multiObject)
				{
					const unsigned* ddi = ((svs.initialInputSolver&&svs.renderLightIndirect==LI_NONE)?svs.initialInputSolver:solver)->getDirectIllumination();
					if (ddi)
					{
						unsigned numTriangles = multiObject->getCollider()->getMesh()->getNumTriangles();
						rr::RRBuffer* vbuf = multiObject->illumination.getLayer(LAYER_DDI) = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,3*numTriangles,1,1,rr::BF_RGBA,true,NULL);
						unsigned* vbufData = (unsigned*)vbuf->lock(rr::BL_DISCARD_AND_WRITE);
						if (vbufData)
						{
							for (unsigned i=0;i<numTriangles;i++)
								vbufData[3*i+2] = vbufData[3*i+1] = vbufData[3*i] = ddi[i];
						}
						vbuf->unlock();
						// tell renderer to use our buffer and not to update it
						rp.updateLayers = false;
						rp.layerLightmap = LAYER_DDI;
						rp.layerEnvironment = UINT_MAX;
						rp.layerLDM = UINT_MAX;
						rp.forceObjectType = 2;
					}
					else
						svs.renderDDI = false;
				}
				else
					svs.renderDDI = false;
				// change rendermode (I think !indexed VBO doesn't even have uvs, so we must not use textures)
				rr_gl::UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
				uberProgramSetup.MATERIAL_DIFFUSE = true;
				uberProgramSetup.MATERIAL_CULLING = rp.uberProgramSetup.MATERIAL_CULLING;
				uberProgramSetup.POSTPROCESS_BRIGHTNESS = rp.uberProgramSetup.POSTPROCESS_BRIGHTNESS;
				uberProgramSetup.POSTPROCESS_GAMMA = rp.uberProgramSetup.POSTPROCESS_GAMMA;
				rp.uberProgramSetup = uberProgramSetup;
			}

			if (svs.renderWireframe) {glClear(GL_COLOR_BUFFER_BIT); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);}

			rp.animationTime = svs.referenceTime.secondsPassed();
			if (rp.animationTime>1000)
				svs.referenceTime.addSeconds(1000);

			rp.srgbCorrect = (svs.renderLightDirect==LD_REALTIME) && svs.srgbCorrect;

			solver->renderScene(rp);

			if (svs.renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			if (svs.renderDDI)
			{
				// free vertex buffer with DDI illumination
				rr::RRObject* multiObject = solver->getMultiObjectCustom();
				if (multiObject)
					RR_SAFE_DELETE(multiObject->illumination.getLayer(LAYER_DDI));
			}

			// adjust tonemapping
			if (svs.renderTonemapping && svs.tonemappingAutomatic
				&& !svs.renderWireframe
				&& ((svs.renderLightIndirect==LI_BAKED && solver->containsLightSource())
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
				const EntityIds& selectedEntityIds = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);
				if (selectedEntityIds.size())
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					rr_gl::UberProgramSetup uberProgramSetup;
					uberProgramSetup.OBJECT_SPACE = true;
					uberProgramSetup.POSTPROCESS_NORMALS = true;
					rp.uberProgramSetup = uberProgramSetup;
					rr::RRObjects selectedObjects;
					for (EntityIds::const_iterator i=selectedEntityIds.begin();i!=selectedEntityIds.end();++i)
						if ((*i).type==ST_OBJECT)
							selectedObjects.push_back(solver->getObject((*i).index));
					solver->getRendererOfScene()->render(solver, &selectedObjects, NULL, rp);
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
				bloom = new rr_gl::Bloom(RR_WX2RR(svs.pathToShaders));
			}
			if (bloom)
			{
				bloom->applyBloom(winWidth,winHeight,svs.bloomThreshold);
			}
		}

		// render SSGI, using own shader
		if (svs.renderLightIndirect!=LI_NONE && svs.ssgiEnabled)
		{
			if (!ssgiLoadAttempted)
			{
				ssgiLoadAttempted = true;
				RR_ASSERT(!ssgi);
				ssgi = new rr_gl::SSGI(RR_WX2RR(svs.pathToShaders));
			}
			if (ssgi)
			{
				ssgi->applySSGI(winWidth,winHeight,svs.camera,svs.ssgiIntensity,svs.ssgiRadius,svs.ssgiAngleBias,textureRenderer);
			}
		}

		// render DOF, using own shader
		if (svs.renderDof)
		{
			if (!dofLoadAttempted)
			{
				dofLoadAttempted = true;
				RR_ASSERT(!dof);
				dof = new rr_gl::DOF(RR_WX2RR(svs.pathToShaders));
			}
			if (dof)
			{
				if (svs.dofAutomatic)
				{
					ray->rayOrigin = svs.camera.getRayOrigin(svs.camera.getScreenCenter());
					ray->rayDir = svs.camera.getRayDirection(svs.camera.getScreenCenter()).normalized();
					ray->rayLengthMin = svs.camera.getNear();
					ray->rayLengthMax = svs.camera.getFar();
					ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
					ray->hitObject = solver->getMultiObjectCustom(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
					ray->collisionHandler = collisionHandler;
					float ratio = sqrtf(svs.camera.dofFar/svs.camera.dofNear);
					if (!_finite(ratio) || ratio<1)
						ratio = 1;
					if (solver->getCollider()->intersect(ray))
					{
						svs.camera.dofNear = ray->hitDistance/ratio;
						svs.camera.dofFar = ray->hitDistance*ratio;
					}
					else
					{
						svs.camera.dofNear = svs.camera.getFar()/ratio;
						svs.camera.dofFar = svs.camera.getFar()*ratio;
					}
				}
				dof->applyDOF(winWidth,winHeight,svs.camera);
			}
		}

		// render lens flare, using own shader
		if (svs.renderLensFlare && !svs.camera.isOrthogonal())
		{
			if (!lensFlareLoadAttempted)
			{
				lensFlareLoadAttempted = true;
				RR_ASSERT(!lensFlare);
				lensFlare = new rr_gl::LensFlare(RR_WX2RR(svs.pathToMaps));
			}
			if (lensFlare)
			{
				lensFlare->renderLensFlares(svs.lensFlareSize,svs.lensFlareId,textureRenderer,svs.camera,solver->getLights(),solver->getMultiObjectCustom(),64);
			}
		}


		// render light field, using own shader
		if (svs.renderHelpers && !_takingSshot && lightField)
		{
			// update cube
			lightFieldObjectIllumination->envMapWorldCenter = svs.camera.getPosition()+svs.camera.getDirection();
			rr::RRVec2 sphereShift = rr::RRVec2(svs.camera.getDirection()[2],-svs.camera.getDirection()[0]).normalized()*0.05f;
			lightField->updateEnvironmentMap(lightFieldObjectIllumination,svs.layerBakedEnvironment,0);

			// diffuse
			// set shader (no direct light)
			rr_gl::UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true;
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = svs.tonemappingBrightness!=rr::RRVec4(1);
			uberProgramSetup.POSTPROCESS_GAMMA = svs.tonemappingGamma!=1;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			rr_gl::Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,NULL,0,&svs.tonemappingBrightness,svs.tonemappingGamma,NULL);
			uberProgramSetup.useIlluminationEnvMap(program,lightFieldObjectIllumination->getLayer(svs.layerBakedEnvironment));
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
			program = uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,NULL,0,&svs.tonemappingBrightness,svs.tonemappingGamma,NULL);
			uberProgramSetup.useIlluminationEnvMap(program,lightFieldObjectIllumination->getLayer(svs.layerBakedEnvironment));
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
				vignetteImage = rr::RRBuffer::load(RR_WX2RR(svs.pathToMaps+"sv_vignette.png"));
			}
			if (vignetteImage && textureRenderer)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				textureRenderer->render2D(rr_gl::getTexture(vignetteImage,false,false),NULL,1,0,0,1,1);
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
					logoImage = rr::RRBuffer::load(RR_WX2RR(svs.pathToMaps+"sv_logo.png"));
				}
				if (logoImage)
				{
					float w = logoImage->getWidth()/(float)winWidth;
					float h = logoImage->getHeight()/(float)winHeight;
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					textureRenderer->render2D(rr_gl::getTexture(logoImage,false,false),NULL,1,1-w,1-h,w,h);
					glDisable(GL_BLEND);
				}
			}
		}


		if (svs.renderGrid || s_ciRenderCrosshair)
		{
			// set line shader
			{
				rr_gl::UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				uberProgramSetup.LEGACY_GL = 1;
				uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,NULL,0,NULL,1,NULL);
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
		renderedIcons.clear();
		if (entityIcons->isOk() && !_takingSshot)
		{
			if (IS_SHOWN(svframe->m_lightProperties))
				renderedIcons.addLights(solver->getLights(),sunIconPosition);
			const EntityIds& selectedEntityIds = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);
			const EntityIds& autoEntityIds = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
			renderedIcons.markSelected(selectedEntityIds);
			if (selectedEntityIds.size())
				renderedIcons.addXYZ(svframe->m_sceneTree->getCenterOf(selectedEntityIds),(&selectedEntityIds==&autoEntityIds)?selectedTransformation:IC_STATIC,svs.camera);
			entityIcons->renderIcons(renderedIcons,solver->getRendererOfScene()->getTextureRenderer(),svs.camera,solver->getCollider(),ray,svs);
		}
	}

	if ((svs.renderHelpers && !_takingSshot)
		)
	{
		// set line shader
		{
			rr_gl::UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
			uberProgramSetup.MATERIAL_DIFFUSE = 1;
			uberProgramSetup.LEGACY_GL = 1;
			uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,NULL,0,NULL,1,NULL);
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
		if (!svs.renderLightmaps2d)
		{
			// ray and collisionHandler are used in this block
			ray->rayOrigin = svs.camera.getRayOrigin(mousePositionInWindow);
			ray->rayDir = svs.camera.getRayDirection(mousePositionInWindow).normalized();
			ray->rayLengthMin = svs.camera.getNear();
			ray->rayLengthMax = svs.camera.getFar();
			ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			ray->hitObject = solver->getMultiObjectCustom(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
			ray->collisionHandler = collisionHandler;
			if (solver->getCollider()->intersect(ray))
			{
				ray->convertHitFromMultiToSingleObject(solver);
				selectedPointValid = true;
				selectedPointObject = ray->hitObject;
				selectedPointMesh = selectedPointObject->getCollider()->getMesh();
				const rr::RRMatrix3x4& hitMatrix = selectedPointObject->getWorldMatrixRef();
				const rr::RRMatrix3x4& inverseHitMatrix = selectedPointObject->getInverseWorldMatrixRef();
				selectedPointMesh->getTriangleBody(ray->hitTriangle,selectedTriangleBody);
				selectedPointMesh->getTriangleNormals(ray->hitTriangle,selectedTriangleNormals);
				hitMatrix.transformPosition(selectedTriangleBody.vertex0);
				hitMatrix.transformDirection(selectedTriangleBody.side1);
				hitMatrix.transformDirection(selectedTriangleBody.side2);
				for (unsigned i=0;i<3;i++)
				{
					inverseHitMatrix.transformNormal(selectedTriangleNormals.vertex[i].normal);
					inverseHitMatrix.transformNormal(selectedTriangleNormals.vertex[i].tangent);
					inverseHitMatrix.transformNormal(selectedTriangleNormals.vertex[i].bitangent);
				}
				selectedPointBasis.normal = selectedTriangleNormals.vertex[0].normal + (selectedTriangleNormals.vertex[1].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].normal-selectedTriangleNormals.vertex[0].normal)*ray->hitPoint2d[1];
				selectedPointBasis.tangent = selectedTriangleNormals.vertex[0].tangent + (selectedTriangleNormals.vertex[1].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].tangent-selectedTriangleNormals.vertex[0].tangent)*ray->hitPoint2d[1];
				selectedPointBasis.bitangent = selectedTriangleNormals.vertex[0].bitangent + (selectedTriangleNormals.vertex[1].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[0] + (selectedTriangleNormals.vertex[2].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray->hitPoint2d[1];
			}
		}

		// render debug rays, using previously set shader
		if (svs.renderHelpers && !_takingSshot && !svs.renderLightmaps2d && SVRayLog::size)
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
		if (svs.renderHelpers && !_takingSshot && fontInited)
		{
			centerObject = UINT_MAX; // reset pointer to texel in the center of screen, it will be set again ~100 lines below
			centerTexel = UINT_MAX;
			centerTriangle = UINT_MAX;
			glDisable(GL_DEPTH_TEST);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			gluOrtho2D(0, winWidth, winHeight, 0);
			{
				// set shader
				rr_gl::UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_CONST = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				uberProgramSetup.LEGACY_GL = 1;
				rr_gl::Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,NULL,0,NULL,1,NULL);
				program->sendUniform("lightIndirectConst",rr::RRVec4(1));
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
					case LD_BAKED: strDirect = "baked"; break;
					case LD_NONE: strDirect = "off"; break;
				}
				// what indirect
				const char* strIndirect = "?";
				switch (svs.renderLightIndirect)
				{
					case LI_REALTIME_FIREBALL: strIndirect = "fireball"; break;
					case LI_REALTIME_ARCHITECT: strIndirect = "architect"; break;
					case LI_BAKED: strIndirect = "baked"; break;
					case LI_CONSTANT: strIndirect = "constant"; break;
					case LI_NONE: strIndirect = "off"; break;
				}
				// how many lightmaps
				unsigned numVbufs = 0;
				unsigned numLmaps = 0;
				for (unsigned i=0;i<numObjects;i++)
				{
					if (solver->getObject(i)->illumination.getLayer(svs.layerBakedLightmap))
					{
						if (solver->getObject(i)->illumination.getLayer(svs.layerBakedLightmap)->getType()==rr::BT_VERTEX_BUFFER) numVbufs++; else
						if (solver->getObject(i)->illumination.getLayer(svs.layerBakedLightmap)->getType()==rr::BT_2D_TEXTURE) numLmaps++;
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
			if (!svs.renderLightmaps2d)
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
				if (numTrianglesSingle)
				{
					textOutput(x,y+=18,h,"world AABB: %f %f %f .. %f %f %f",bboxMinW[0],bboxMinW[1],bboxMinW[2],bboxMaxW[0],bboxMaxW[1],bboxMaxW[2]);
					textOutput(x,y+=18,h,"world center: %f %f %f",centerW[0],centerW[1],centerW[2]);
					textOutput(x,y+=18,h,"local AABB: %f %f %f .. %f %f %f",bboxMinL[0],bboxMinL[1],bboxMinL[2],bboxMaxL[0],bboxMaxL[1],bboxMaxL[2]);
					textOutput(x,y+=18,h,"local center: %f %f %f",centerL[0],centerL[1],centerL[2]);
				}
				if (solver->getObject(svs.selectedObjectIndex))
				{
					unsigned layerNumber[3] = {svs.layerBakedLightmap,svs.layerBakedAmbient,svs.layerBakedLDM};
					const char* layerName[3] = {"[lightmap]","[ambient map]","[ldm]"};
					for (unsigned i=0;i<3;i++)
					{
						rr::RRBuffer* bufferSelectedObj = solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(layerNumber[i]);
						if (bufferSelectedObj)
						{
							//if (svs.renderRealtime) glColor3f(0.5f,0.5f,0.5f);
							textOutput(x,y+=18,h,layerName[i]);
							textOutput(x,y+=18,h,"type: %s",(bufferSelectedObj->getType()==rr::BT_VERTEX_BUFFER)?"PER VERTEX":((bufferSelectedObj->getType()==rr::BT_2D_TEXTURE)?"PER PIXEL":"INVALID!"));
							textOutput(x,y+=18,h,"size: %d*%d*%d",bufferSelectedObj->getWidth(),bufferSelectedObj->getHeight(),bufferSelectedObj->getDepth());
							textOutput(x,y+=18,h,"format: %s",(bufferSelectedObj->getFormat()==rr::BF_RGB)?"RGB":((bufferSelectedObj->getFormat()==rr::BF_RGBA)?"RGBA":((bufferSelectedObj->getFormat()==rr::BF_RGBF)?"RGBF":((bufferSelectedObj->getFormat()==rr::BF_RGBAF)?"RGBAF":"INVALID!"))));
							textOutput(x,y+=18,h,"scale: %s",bufferSelectedObj->getScaled()?"custom(usually sRGB)":"physical(linear)");
							//glColor3f(1,1,1);
						}
					}
				}
			}
			if (multiMesh && !svs.renderLightmaps2d)
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
					rr::RRBuffer* selectedPointLightmap = selectedPointObject->illumination.getLayer(svs.layerBakedLightmap);
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
				}
				textOutput(x,y+=18*2,h,"numbers of casters/lights show potential, what is allowed");
			}
			if (multiMesh && svs.renderLightmaps2d)
			{
				rr::RRVec2 uv = lv.getCenterUv(GetSize());
				textOutput(x,y+=18*2,h,"[pointed by mouse]");
				textOutput(x,y+=18,h,"uv: %f %f",uv[0],uv[1]);
				if (solver->getObject(svs.selectedObjectIndex))
				{
					rr::RRBuffer* buffer = solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(svs.layerBakedLightmap);
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
			helpImage = rr::RRBuffer::load(RR_WX2RR(svs.pathToMaps+"sv_help.png"));
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
			textureRenderer->render2D(rr_gl::getTexture(helpImage,false,false),NULL,1,(1-w)*0.5f,(1-h)*0.5f,w,h);
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
			fpsDisplay = rr_gl::FpsDisplay::create(RR_WX2RR(svs.pathToMaps));
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