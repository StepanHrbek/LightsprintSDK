// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - GL canvas in main window.
// --------------------------------------------------------------------------

#include "SVCanvas.h"
#include "SVEntityIcons.h"
#include "SVRayLog.h"
#include "SVFrame.h"
#include "SVLightProperties.h"
#include "SVSceneTree.h" // for shortcuts that manipulate animations in scene tree
#include "SVLog.h"
#include "SVSaveLoad.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/PluginAccumulation.h"
#include "Lightsprint/GL/PluginBloom.h"
#include "Lightsprint/GL/PluginDOF.h"
#include "Lightsprint/GL/PluginFPS.h"
#include "Lightsprint/GL/PluginLensFlare.h"
#include "Lightsprint/GL/PluginOculus.h"
#include "Lightsprint/GL/PluginOpenVR.h"
#include "Lightsprint/GL/PluginPanorama.h"
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PluginShowDDI.h"
#include "Lightsprint/GL/PluginSky.h"
#include "Lightsprint/GL/PluginSSGI.h"
#include "Lightsprint/GL/PluginContours.h"
#include "Lightsprint/GL/PluginStereo.h"
#include "Lightsprint/GL/PluginToneMapping.h"
#include "Lightsprint/GL/PluginToneMappingAdjustment.h"
#include "Lightsprint/GL/PreserveState.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif
#include <cctype>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp> // extension()==".gsa"

//#define SUPPORT_GL_ES

namespace bf = boost::filesystem;

#define canvasWindow (svframe->m_canvasWindow)
#define canvasGetSize() canvasWindow->GetSize()

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

// used when IsDisplaySupported()=true
// Note that all attributes are ignored when one of them fails. It is usually clearly visible - image becomes aliased.
// This happens when stereo is requested, but GPU doesn't support it.
static int s_attribListQuad[] = {
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	WX_GL_SAMPLE_BUFFERS, GL_TRUE, // makes no difference in Windows, is necessary in OSX for antialiasing
	WX_GL_SAMPLES, 4, // antialiasing. can be later disabled by glDisable(GL_MULTISAMPLE), but it doesn't improve speed (tested on X1650). it must be disabled here (change 4 to 1) for higher fps
	WX_GL_DEPTH_SIZE, 24, // default is 16, explicit 24 should reduce z-fight. 32 fails on all cards tested including hd5870 and gf460 (falls back to default without antialiasing)
	WX_GL_STEREO, // required by SM_QUAD_BUFFERED, starts quad buffered stereo. some GPUs don't support it; in such case wxWidgets falls back to default attributes without antialiasing
	0, 0};

// used when IsDisplaySupported()=false
static int s_attribList[] = {
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	WX_GL_SAMPLE_BUFFERS, GL_TRUE,
	WX_GL_SAMPLES, 4,
	WX_GL_DEPTH_SIZE, 24,
	0, 0};

CanvasWindow::CanvasWindow(SVFrame* _svframe)
	: wxGLCanvas(_svframe, wxID_ANY, 
	(_svframe->userPreferences.stereoMode==rr::RRCamera::SM_QUAD_BUFFERED) ? s_attribListQuad : s_attribList,
		wxDefaultPosition, wxDefaultSize, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE|wxWANTS_CHARS, "GLCanvas")
{
}

SVCanvas::SVCanvas( SceneViewerStateEx& _svs, SVFrame *_svframe)
	: svs(_svs)
{
	renderEmptyFrames = UINT_MAX;
	context = nullptr;
	svframe = _svframe;
	solver = nullptr;
	selectedType = ST_CAMERA;
	winWidth = 0;
	winHeight = 0;
	windowCoord[0] = 0;
	windowCoord[1] = 0;
	windowCoord[2] = 800;
	windowCoord[3] = 600;
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
	mousePositionInWindow = rr::RRVec2(0);
	centerObject = UINT_MAX;
	centerTexel = UINT_MAX;
	centerTriangle = UINT_MAX;
	collisionHandler = nullptr;
	fontInited = false;

	helpLoadAttempted = false;
	helpImage = nullptr;


	vignetteLoadAttempted = false;
	vignetteImage = nullptr;

	logoLoadAttempted = false;
	logoImage = nullptr;

	lightField = nullptr;
	lightFieldQuadric = nullptr;
	lightFieldObjectIllumination = nullptr;

	textureRenderer = nullptr;

	entityIcons = nullptr;
	sunIconPosition = rr::RRVec3(0);
	renderedIcons.iconSize = 1;
	fullyCreated = false;

	skyboxBlendingInProgress = false;
	selectedTransformation = IC_MOVEMENT;

	previousLightIndirect = LI_NONE;

	pathTracedBuffer = nullptr;
	pathTracedAccumulator = 0;

	vrDevice = nullptr;
	vrDeviceType = rr::RRCamera::SM_MONO;
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
						glDebugMessageCallbackARB(debugCallback, nullptr);
						rr_gl::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
						glDebugMessageControlARB(GL_DONT_CARE,GL_DEBUG_TYPE_PERFORMANCE_ARB,GL_DONT_CARE,0,nullptr,GL_FALSE);
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
	context = new SVContext(canvasWindow,false,false,false);
	canvasWindow->SetCurrent(*context);

#ifdef REPORT_HEAP_STATISTICS
	s_oldAllocHook = _CrtSetAllocHook(newAllocHook);
#endif

	const char* error = rr_gl::initializeGL(true);
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
		rr_gl::glGetIntegerv(GL_VIEWPORT,viewport);
		winWidth = viewport[2];
		winHeight = viewport[3];
	}


	// init solver
	solver = new rr_gl::RRSolverGL(RR_WX2RR(svs.pathToShaders), RR_WX2RR(svs.pathToMaps));
	textureRenderer = solver->getRenderer()->getTextureRenderer();
	solver->setColorSpace(rr::RRColorSpace::create_sRGB());
	if (svs.initialInputSolver)
	{
		delete solver->getColorSpace();
		solver->setColorSpace(svs.initialInputSolver->getColorSpace());
		rr::RRReal envAngle0 = 0;
		rr::RRReal envAngle1 = 0;
		rr::RRBuffer* env0 = svs.initialInputSolver->getEnvironment(0,&envAngle0);
		rr::RRBuffer* env1 = svs.initialInputSolver->getEnvironment(1,&envAngle1);
		solver->setEnvironment(env0,env1,envAngle0,envAngle1);
		solver->setEnvironmentBlendFactor(svs.initialInputSolver->getEnvironmentBlendFactor());
		solver->setStaticObjects(svs.initialInputSolver->getStaticObjects(),nullptr,nullptr,rr::RRCollider::IT_BVH_FAST,svs.initialInputSolver); // smoothing and multiobject are taken from _solver
		solver->setDynamicObjects(svs.initialInputSolver->getDynamicObjects());
		solver->setLights(svs.initialInputSolver->getLights());
		solver->setDirectIllumination(svs.initialInputSolver->getDirectIllumination());
	}
	else
	if (!svs.sceneFilename.empty())
	{
		// load scene
		rr::RRScene* scene = svframe->loadScene(svs.sceneFilename,true);
		mergedScenes.push_back(scene);

		if (scene->cameras.size())
		{
			svs.camera = scene->cameras[0];
			svs.autodetectCamera = false; // camera was taken from scene
		}

		// send everything to solver
		solver->setEnvironment(mergedScenes[0]->environment,nullptr,svs.skyboxRotationRad,svs.skyboxRotationRad);
		solver->setStaticObjects(mergedScenes[0]->objects,nullptr);
		solver->setDynamicObjects(mergedScenes[0]->objects);
		solver->setLights(mergedScenes[0]->lights);
	}

	// warning: when rendering scene from initialInputSolver, original cube buffers are lost here
	reallocateBuffersForRealtimeGI(true);

	// load skybox from filename only if we don't have it from inputsolver or scene yet
	// (note that ME_ENV_RELOAD does not replace env immediately, it creates smooth transition from old one.
	//  so if old one is missing, it creates transition from missing texture stub)
	if (solver->getEnvironment())
		rr::RRReporter::report(rr::INF3,"skybox: solver.env[%s]=%s, svs.env=%s\n",solver->getEnvironment()->isStub()?"stub":"ok",solver->getEnvironment()->filename.c_str(),svs.skyboxFilename.c_str());
	else
		rr::RRReporter::report(rr::INF3,"skybox: solver.env=nullptr, svs.env=%s\n",svs.skyboxFilename.c_str());
	if ((!solver->getEnvironment() || solver->getEnvironment()->isStub()) && !svs.skyboxFilename.empty())
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
	lightFieldObjectIllumination->getLayer(svs.layerBakedEnvironment) = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,nullptr);
	entityIcons = new SVEntityIcons(svs.pathToMaps,solver->getUberProgram());
	recalculateIconSizeAndPosition();

	svframe->userPreferences.applySwapInterval();

	collisionHandler = solver->getMultiObject()->createCollisionHandlerFirstVisible();

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
	rr::RRObject* object = solver->getMultiObject();
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
					solver->setEnvironment(scene->environment,nullptr,svs.skyboxRotationRad);
			}
			else
			{
				for (unsigned i=objects.size();i--;)
					for (unsigned j=0;j<scene->objects.size();j++)
						if (objects[i]==scene->objects[j])
						{
							objects.erase(objects.begin()+i);
							break;
						}
				for (unsigned i=lights.size();i--;)
					for (unsigned j=0;j<scene->lights.size();j++)
						if (lights[i]==scene->lights[j])
						{
							lights.erase(lights.begin()+i);
							break;
						}
			}
		}
		objects.removeEmptyObjects();
		if (staticObjectsModified)
			solver->setStaticObjects(objects,nullptr);
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

	// fix dangling pointer in collisionHandler
	delete collisionHandler;
	collisionHandler = solver->getMultiObject()->createCollisionHandlerFirstVisible();

	// alloc rtgi buffers, otherwise new objects would have no realtime indirect
	// (it has to be called even when removing static objects, because multiobject is rebuilt and has no rtgi buffer;
	//  if we don't call it, optimized render that uses multiobject would have no indir from FB/architect, optimized render usually shows up after disabling cubes/mirrors)
	reallocateBuffersForRealtimeGI(true);

	recalculateIconSizeAndPosition();

	svframe->OnAnyChange(SVFrame::ES_MISC,nullptr,nullptr,0);
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

	rr::RR_SAFE_DELETE(vrDevice);
	vrDeviceType = rr::RRCamera::SM_MONO;

	if (!svs.releaseResources)
	{
		// user requested fast exit without releasing resources
		// save time and don't release scene
		return;
	}

	// pathtracer
	rr::RR_SAFE_DELETE(pathTracedBuffer);

	// logo
	rr::RR_SAFE_DELETE(logoImage);

	// vignette
	rr::RR_SAFE_DELETE(vignetteImage);

	// help
	rr::RR_SAFE_DELETE(helpImage);

	rr::RR_SAFE_DELETE(entityIcons);
	rr::RR_SAFE_DELETE(collisionHandler);

	rr_gl::deleteAllTextures();
	// delete objects referenced by solver
	if (solver)
	{
		// delete all lightmaps for realtime rendering
		for (unsigned i=0;i<solver->getStaticObjects().size()+solver->getDynamicObjects().size();i++)
		{
			rr::RR_SAFE_DELETE(solver->getObject(i)->illumination.getLayer(svs.layerRealtimeAmbient));
		}

		// delete colorSpace created for scene loaded from disk
		if (!svs.initialInputSolver)
		{
			delete solver->getColorSpace();
		}

		textureRenderer = nullptr;
	}
	rr::RR_SAFE_DELETE(solver);
	for (unsigned i=0;i<mergedScenes.size();i++) delete mergedScenes[i];
	rr::RR_SAFE_DELETE(lightField);
	rr::RR_SAFE_DELETE(lightFieldObjectIllumination);
	for (unsigned i=0;i<lightsToBeDeletedOnExit.size();i++) delete lightsToBeDeletedOnExit[i];
	lightsToBeDeletedOnExit.clear();
	gluDeleteQuadric(lightFieldQuadric);
	lightFieldQuadric = nullptr;
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
	canvasWindow->GetClientSize(&w, &h);
	if (context
		// do nothing when wx calls us with unchanged size
		// (why? when exiting SV with one panel floating and all other panels closed, partialy destroyed window calls us with unchanged size, SetCurrent would assert)
		&& (force || w!=winWidth || h!=winHeight))
	{
		canvasWindow->SetCurrent(*context);
		// [#63] for a moment, winWidth is whole canvas size
		winWidth = w;
		winHeight = h;
		// with Enhanced screenshot checked, viewport maintains the same aspect as screenshot
		// [#63] winWidth is reduced to active area of canvas
		if (svframe->userPreferences.sshotEnhanced)
		{
			if (w*svframe->userPreferences.sshotEnhancedHeight > h*svframe->userPreferences.sshotEnhancedWidth)
				winWidth = h*svframe->userPreferences.sshotEnhancedWidth/svframe->userPreferences.sshotEnhancedHeight;
			else
				winHeight = w*svframe->userPreferences.sshotEnhancedHeight/svframe->userPreferences.sshotEnhancedWidth;
		}
	}
}

static bool s_shiftDown = false;

// called on any keypress, e.g. each press in numeric property, not just on enter
// processed synchronously with other events, so it is only called after we finish rendering frame, it can't interrupt rendering [#47]
int SVCanvas::FilterEvent(wxKeyEvent& event)
{
	if (exitRequested || !fullyCreated)
		return wxApp::Event_Skip;

	bool needsRefresh = false;
	int evkey = event.GetKeyCode();
	if (event.GetModifiers()==wxMOD_CONTROL) switch (evkey)
	{
		case 'T': // ctrl-t
			svs.renderMaterialTextures = !svs.renderMaterialTextures;
			return wxApp::Event_Processed;
		case 'W': // ctrl-w
			svs.renderWireframe = !svs.renderWireframe;
			return wxApp::Event_Processed;
		case 'H': // ctrl-h
			svs.renderHelpers = !svs.renderHelpers;
			return wxApp::Event_Processed;
		case 'F': // ctrl-f
			svs.renderFPS = !svs.renderFPS;
			return wxApp::Event_Processed;
	}
	else if (event.GetModifiers()==wxMOD_ALT) switch (evkey)
	{
		case 'S': // alt-s
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_SPOT);
			return wxApp::Event_Processed;
		case 'O': // alt-o
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_POINT);
			return wxApp::Event_Processed;
		case 'F': // alt-f
			svframe->OnMenuEventCore(SVFrame::ME_LIGHT_FLASH);
			return wxApp::Event_Processed;
		case '1':
		case '2':
		case '3':
			svframe->OnMenuEventCore(SVFrame::ME_WINDOW_LAYOUT1+evkey-'1');
			return wxApp::Event_Processed;
	}
	//else switch (evkey)
	//{
	//}
	return wxApp::Event_Skip;
}

void SVCanvas::configureVideoPlayback(bool play, float secondFromStart)
{
	//rr::RRReporter::report(rr::INF2,"video %hs %f\n",play?"play":"stop",secondFromStart);
	rr::RRVector<rr::RRBuffer*> buffers;
	if (solver)
		solver->processBuffers(nullptr,[&buffers](rr::RRBuffer* buffer){buffers.push_back(buffer);});
	if (secondFromStart>=0)
		for (unsigned i=0;i<buffers.size();i++)
			buffers[i]->seek(secondFromStart);
	for (unsigned i=0;i<buffers.size();i++)
		if (play)
			buffers[i]->play();
		else
			buffers[i]->pause();
}


void SVCanvas::OnKeyDown(wxKeyEvent& event)
{
	s_shiftDown = event.ShiftDown();

	if (exitRequested || !fullyCreated)
		return;

	bool needsRefresh = false;
	long evkey = event.GetKeyCode();
	float speed = 1;
	switch(evkey)
	{
		case ' ':
			// toggle video playback
			svs.playVideos = !svs.playVideos;
			configureVideoPlayback(svs.playVideos);
			break;

		case WXK_F9: svframe->OnMenuEventCore(SVFrame::ME_FILE_SAVE_SCREENSHOT); break;
		case WXK_F11: svframe->OnMenuEventCore(SVFrame::ME_WINDOW_FULLSCREEN_META); break;
		case WXK_NUMPAD_ADD:
		case '+': svs.tonemapping.color *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_SUBTRACT:
		case '-': svs.tonemapping.color /= 1.2f; needsRefresh = true; break;

		//case '8': if (event.GetModifiers()==0) break; // ignore '8', but accept '8' + shift as '*', continue to next case
		case WXK_NUMPAD_MULTIPLY:
		case '*': svs.tonemapping.gamma *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_DIVIDE:
		case '/': svs.tonemapping.gamma /= 1.2f; needsRefresh = true; break;

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

	if (needsRefresh)
	{
		canvasWindow->Refresh(false);
	}
	svframe->OnAnyChange(SVFrame::ES_KEYBOARD_MID_MOVEMENT,nullptr,&event,0);
}

void SVCanvas::OnKeyUp(wxKeyEvent& event)
{
	s_shiftDown = event.ShiftDown();

	if (exitRequested || !fullyCreated)
		return;
	bool didMove = (speedForward-speedBack) || (speedRight-speedLeft) || (speedUp-speedDown) || speedY || speedLean;
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
	bool movesNow = (speedForward-speedBack) || (speedRight-speedLeft) || (speedUp-speedDown) || speedY || speedLean;
	if (didMove && !movesNow)
		svframe->OnAnyChange(SVFrame::ES_KEYBOARD_END,nullptr,&event,0);
}

extern bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption);

// Filled on click, original data acquired at click time.
struct ClickInfo
{
	rr::RRTime time;
	int mouseX; // click coord in canvas, 0..n, n might be >=winWidth
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
static bool s_draggingObjectOrLight = false;

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
		canvasWindow->SetFocus();
	}

	// coords from previous frame, inherited to current frame if new coords are not available
	static wxPoint oldPosition(-1,-1);
	static rr::RRVec2 oldMousePositionInWindow(0);
	bool contextMenu = event.GetId()==ID_CONTEXT_MENU;
	wxPoint newPosition = (event.GetPosition()==wxPoint(-1,-1)) ? oldPosition : event.GetPosition(); // use previous coords for event that does not come with its own
	mousePositionInWindow = rr::RRVec2((newPosition.x*2.0f+winWidth-canvasGetSize().x)/winWidth-1,1-(newPosition.y*2.0f+winHeight-canvasGetSize().y)/winHeight); // in fact, it is mouse position in _viewport_ where viewport winWidth*winHeight is in center of window GetSize()

	if (!winWidth || !winHeight || !solver)
	{
		return;
	}
	if (svs.renderLightmaps2d)
	{
		lv.OnMouseEvent(event,canvasGetSize());
		return;
	}

	const EntityIds& selectedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);

	const SVEntity* intersectedEntity = entityIcons->intersectIcons(renderedIcons,mousePositionInWindow);
	canvasWindow->SetCursor(wxCursor(intersectedEntity?wxCursor(wxCURSOR_HAND):wxNullCursor));

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
		svs.camera.getRay(mousePositionInWindow,s_ci.rayOrigin,s_ci.rayDirection);

		// find scene distance, adjust search range to look only for closer icons
		s_ci.hitDistance = svs.camera.getNear()*0.9f+svs.camera.getFar()*0.1f;
		rr::RRRay ray;
		ray.rayOrigin = s_ci.rayOrigin;
		rr::RRVec3 directionToMouse = s_ci.rayDirection;
		float directionToMouseLength = directionToMouse.length();
		ray.rayDir = directionToMouse/directionToMouseLength;
		ray.rayLengthMin = svs.camera.getNear()*directionToMouseLength;
		ray.rayLengthMax = svs.camera.getFar()*directionToMouseLength;
		ray.rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_TRIANGLE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D;
		ray.hitObject = solver->getMultiObject(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
		ray.collisionHandler = collisionHandler;
		if (solver->getCollider()->intersect(ray))
		{
			// in next step, look only for closer lights
			ray.rayLengthMax = ray.hitDistance;
			s_ci.hitTriangle = ray.hitTriangle;
			s_ci.hitDistance = ray.hitDistance;
			s_ci.hitPoint2d = ray.hitPoint2d;
			s_ci.hitPoint3d = ray.hitPoint3d;
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
			if (ray.hitObject==nullptr || ray.hitObject==solver->getMultiObject())
			{
				if (s_ci.hitTriangle==UINT_MAX)
				{
					s_ci.clickedEntity.index = 0;
				}
				else
				{
					rr::RRMesh::PreImportNumber pre = solver->getMultiObject()->getCollider()->getMesh()->getPreImportTriangle(s_ci.hitTriangle);
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
					if (solver->getStaticObjects()[i]==ray.hitObject)
						s_ci.clickedEntity.index = i;
				for (unsigned i=0;i<numDynamicObjects;i++)
					if (solver->getDynamicObjects()[i]==ray.hitObject)
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
					// static object: pass nullptr + triangle in multiobject, m_materialProperties will show custom+physical versions
					// m_materialProperties has extra 'object=nullptr' path for looking up material in multiobjCustom and multiObjPhysical
					rr::RRMesh::PreImportNumber pre;
					pre.object = s_ci.clickedEntity.index;
					pre.index = s_ci.hitTriangle;
					unsigned post = solver->getMultiObject()->getCollider()->getMesh()->getPostImportTriangle(pre);
					svframe->m_materialProperties->setMaterial(solver,nullptr,post,s_ci.hitPoint2d);
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

	// handle double clicking sky
	if (event.LeftDClick() && s_ci.hitTriangle==UINT_MAX && s_ci.clickedEntity.iconCode==IC_LAST) // IC_LAST means no icon clicked
	{
		svframe->OnMenuEventCore(SVFrame::ME_WINDOW_FULLSCREEN_META);
	}

	// handle dragging
	if (event.Dragging() && s_ciRelevant && newPosition!=oldPosition && !clicking)
	{
		const EntityIds& manipulatedEntities = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_AUTO);
		rr::RRVec3 manipulationMini,manipulationMaxi,manipulationPivot;
		svframe->m_sceneTree->getAABBOf(manipulatedEntities,PP_CENTER,manipulationMini,manipulationMaxi,manipulationPivot);
		bool manipulatingCamera = manipulatedEntities==svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_CAMERA);
		bool manipulatingSelection = s_ci.clickedEntityIsSelected && !manipulatingCamera;
		bool manipulatingSingleLight = selectedEntities.size()==1 && selectedEntities.begin()->type==ST_LIGHT;
		bool manipulatingGizmo = s_ci.clickedEntity.iconCode>=IC_MOVEMENT && s_ci.clickedEntity.iconCode<=IC_Z;
		s_draggingObjectOrLight = manipulatingGizmo && !manipulatingCamera;
		if (event.LeftIsDown() && !manipulatingCamera && (manipulatingSelection || manipulatingGizmo))
		{
			// moving/rotating/scaling selection (gizmo)
			rr::RRMatrix3x4 transformation;
			bool preTransform = false;
			rr::RRVec3 rayOriginOld,rayDirectionOld; svs.camera.getRay(oldMousePositionInWindow,rayOriginOld,rayDirectionOld);
			rr::RRVec3 rayOriginNow,rayDirectionNow; svs.camera.getRay(mousePositionInWindow,rayOriginNow,rayDirectionNow);
			rr::RRVec3 pan = svs.camera.isOrthogonal()
				? rayOriginNow-rayOriginOld
				: (rayDirectionNow-rayDirectionOld)*(s_ci.hitDistance/s_ci.rayDirection.length());
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
					pan = rr::RRVec3(drag.x); // make pan independent of world scale
					switch (s_ci.clickedEntity.iconCode)
					{
						case IC_X: pan.y = pan.z = 0; break;
						case IC_Y: pan.x = pan.z = 0; break;
						case IC_Z: pan.x = pan.y = 0; break;
						default: pan = RRVec3(pan.avg()); break;
					}
					for (unsigned i=0;i<3;i++) pan[i] = powf(6.f,pan[i]);
					transformation = rr::RRMatrix3x4::scale(pan);
					if (manipulatedEntities.size()==1)
						preTransform = true;
					break;
				default:
					// should not get here, just to prevent warning
					break;
			}
			svframe->m_sceneTree->manipulateEntities(manipulatedEntities,preTransform?transformation:transformation.centeredAround(manipulationPivot),preTransform,false);
		}
		else
		if (event.LeftIsDown())
		{
			// rotating camera
			goto rotating_camera;
		}
		else
		if (event.MiddleIsDown())
		{
			// panning
			//  drag clicked pixel so that it stays under mouse
			rr::RRVec2 origMousePositionInWindow = rr::RRVec2((s_ci.mouseX*2.0f+winWidth-canvasGetSize().x)/winWidth-1,1-(s_ci.mouseY*2.0f+winHeight-canvasGetSize().y)/winHeight); // in fact, it is mouse position in _viewport_ where viewport winWidth*winHeight is in center of window GetSize()
			rr::RRVec3 rayOriginOld,rayDirectionOld; svs.camera.getRay(origMousePositionInWindow,rayOriginOld,rayDirectionOld);
			rr::RRVec3 rayOriginNow,rayDirectionNow; svs.camera.getRay(mousePositionInWindow,rayOriginNow,rayDirectionNow);
			rr::RRVec3 pan = svs.camera.isOrthogonal()
				? rayOriginOld-rayOriginNow
				: (s_ci.rayDirection-rayDirectionNow)*(s_ci.hitDistance/s_ci.rayDirection.length());
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
		svframe->OnAnyChange(SVFrame::ES_MOUSE_MID_MOVEMENT,nullptr,&event,0);
	}

	// when nothing happens, just some passive mouse movement event, don't return (7421 was wrong, 7426 fix is no longer sufficient)
	// 1) above, mousePositionInWindow needs to be filled, renderHelpers needs it
	// 2) below, camera can rotate on passive mouse movement
	// 3) below, some actions (dragging) end by first event without buttons

	// passive movement: possibly rotate camera
	if (event.Moving() || event.Entering() || event.Leaving())
	{
		if (svframe->vrActive() && svs.fullscreen)
		{
			// avoid camera discontinuity when mouse enters viewport
			if (event.Entering())
				oldPosition = newPosition;

			// rotating camera
		rotating_camera:
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
			svframe->OnAnyChange(SVFrame::ES_MOUSE_MID_MOVEMENT,nullptr,&event,0);
		}
	}

	// passive movement: end dragging etc
	if (!event.ButtonDown() && !event.Dragging()
		&& !event.LeftIsDown() && !event.MiddleIsDown() && !event.RightIsDown()) // when dragging leaves window, event arrives with LeftIsDown() && !Dragging(). this makes dragging survive such event
	{
		if (s_ciRelevant)
			svframe->OnAnyChange(SVFrame::ES_MOUSE_END,nullptr,&event,0);
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
						numCopies = event.ShiftDown() ? 2 : RR_MAX(1,(int)std::abs(numCopies));
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
						solver->setDynamicObjects(objects);
						reallocateBuffersForRealtimeGI(false);
						svframe->updateAllPanels();
					}
				}
			}
		}
		if (s_draggingObjectOrLight)
		{
			s_draggingObjectOrLight = false;
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
		svframe->OnAnyChange(SVFrame::ES_MOUSE_END,nullptr,&event,0);
	}


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
	float seconds = prevIdleTime.secondsSinceLastQuery();
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
			rr::RRVec3 mini,maxi,pivot;
			svframe->m_sceneTree->getAABBOf(entities,PP_CENTER,mini,maxi,pivot);
			float speed = s_shiftDown ? 3 : 1;
			svframe->m_sceneTree->manipulateEntities(entities,
				rr::RRMatrix3x4::translation( (
					svs.camera.getDirection() * ((speedForward-speedBack)*meters) +
					svs.camera.getRight() * ((speedRight-speedLeft)*meters) +
					svs.camera.getUp() * ((speedUp-speedDown)*meters) +
					rr::RRVec3(0,speedY*meters,0)) *speed)
				* rr::RRMatrix3x4::rotationByAxisAngle(svs.camera.getDirection(),-speedLean*seconds*0.5f).centeredAround(pivot),
				false, speedLean?true:false
				);
			bool movesNow = (speedForward-speedBack) || (speedRight-speedLeft) || (speedUp-speedDown) || speedY || speedLean;
			if (movesNow)
				svframe->OnAnyChange(SVFrame::ES_KEYBOARD_MID_MOVEMENT,nullptr,&event,0);
		}
	}

	canvasWindow->Refresh(false);
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
	if (exitRequested || !winWidth || !winHeight)
		return;

	wxPaintDC dc(canvasWindow);

	if (!context) return;
	canvasWindow->SetCurrent(*context);

	if (renderEmptyFrames || !fullyCreated)
	{
		rr_gl::glClearColor(0.31f,0.31f,0.31f,0);
		glClear(GL_COLOR_BUFFER_BIT);
		rr_gl::glClearColor(0,0,0,0);
		if (renderEmptyFrames)
			renderEmptyFrames--;
		canvasWindow->SwapBuffers();
		return;
	}

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

	bool swapBuffersAlreadyCalled = Paint(false,"");
	if (!swapBuffersAlreadyCalled)
		canvasWindow->SwapBuffers();
}

bool SVCanvas::Paint(bool _takingSshot, const wxString& extraMessage)
{
	bool result = false;
#ifdef _MSC_VER
	__try
	{
#endif
		result = PaintCore(_takingSshot,extraMessage);
#ifdef _MSC_VER
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"3D renderer crashed, you'll see no image. The rest of application (menus, windows, hotkeys) may still work.\n"));
		rr_gl::glClearColor(1,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
		rr_gl::glClearColor(0,0,0,0);
	}
#endif
	return result;
}


// [#47] called from renderer to detect user interaction (so that very slow rendering can be aborted)
rr::RRTime g_lastRawInteractionTime;
bool isKeyOrButtonPressed()
{
	// a) return wxAppConsole::GetInstance()->HasPendingEvents(); does not work, it is always false
	// b) test state of the most important buttons
	wxMouseState ms = wxGetMouseState();
	bool m = ms.LeftIsDown() || ms.RightIsDown() || ms.MiddleIsDown();
	bool k = wxGetKeyState(wxKeyCode('w')) || wxGetKeyState(wxKeyCode('s')) || wxGetKeyState(wxKeyCode('a')) || wxGetKeyState(wxKeyCode('d'))
		|| wxGetKeyState(wxKeyCode('q')) || wxGetKeyState(wxKeyCode('z'))
		|| wxGetKeyState(WXK_LEFT) || wxGetKeyState(WXK_RIGHT) || wxGetKeyState(WXK_UP) || wxGetKeyState(WXK_DOWN)
		|| wxGetKeyState(WXK_ESCAPE) || wxGetKeyState(WXK_F4);
	if (m || k)
	{
		// we need to make difference between lastInteractionTime (wx event processed by us), and raw interaction possibly with other program
		g_lastRawInteractionTime.setNow();
		// definitely don't call OnAnyChange, we are not in main thread
		//wxAppConsole::GetInstance()->OnAnyChange(SVFrame::ES_MOUSE,nullptr,nullptr,0);
	}
	return m || k;
}

bool SVCanvas::PaintCore(bool _takingSshot, const wxString& extraMessage)
{
	rr::RRReportInterval report(rr::INF3,"display...\n");
	bool result = false;
	if (exitRequested || !fullyCreated || !winWidth || !winHeight) return result; // can't display without window
	if (svs.renderLightmaps2d)
	{
		if (solver->getObject(svs.selectedObjectIndex))
			lv.setObject(
				solver->getObject(svs.selectedObjectIndex)->illumination.getLayer(
					svs.selectedLayer ? svs.selectedLayer : (
						svs.renderLDMEnabled() ? svs.layerBakedLDM : ((svs.renderLightIndirect==LI_AMBIENTMAPS)?svs.layerBakedAmbient:svs.layerBakedLightmap))
						),
				solver->getObject(svs.selectedObjectIndex),
				svs.renderLightmapsBilinear);
		else
			lv.setObject(
				nullptr,
				nullptr,
				svs.renderLightmapsBilinear);
		lv.OnPaint(textureRenderer,canvasGetSize());
	}
	else
	{

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
				// restart pathtracer
				pathTracedAccumulator = 0;
			}
			else
			{
				// stop blending
				solver->setEnvironmentBlendFactor(0);
				skyboxBlendingInProgress = false;
			}
		}


		// override parts of svs.camera
		svs.camera.stereoMode = svs.renderStereo ? svframe->userPreferences.stereoMode : rr::RRCamera::SM_MONO;
		svs.camera.stereoSwap = svframe->userPreferences.stereoSwap;
		svs.camera.panoramaMode = svs.renderPanorama ? svs.panoramaMode : rr::RRCamera::PM_OFF;
		svs.camera.panoramaCoverage = svs.panoramaCoverage;
		svs.camera.panoramaScale = svs.panoramaScale;
		svs.camera.panoramaFisheyeFovDeg = svs.panoramaFovDeg;

		// create / switch vr device
		if (svs.camera.stereoMode==rr::RRCamera::SM_OCULUS_RIFT || svs.camera.stereoMode==rr::RRCamera::SM_OPENVR)
		if (svs.camera.stereoMode!=vrDeviceType)
		{
			rr::RR_SAFE_DELETE(vrDevice);
			vrDevice = (svs.camera.stereoMode==rr::RRCamera::SM_OCULUS_RIFT) ? rr_gl::createOculusDevice() : rr_gl::createOpenVRDevice();
			vrDeviceType = svs.camera.stereoMode;
		}

		// vr camera rotation+translation
		if (svframe->vrActive())
		{
			vrDevice->updateCamera(svs.camera);
			svframe->OnAnyChange(SVFrame::ES_RIFT,nullptr,nullptr,0);
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
				solver->realtimeLights[i]->getCamera()->setRange(svs.camera.getNear(),svs.camera.getFar()); // not good when camera goes very close to objects, flashlight needs different near
				solver->realtimeLights[i]->updateAfterRRLightChanges(); // sets dirtyRange if pos/dir/fov did change
				solver->realtimeLights[i]->dirtyRange = false; // clear it, range already is good (dirty range would randomly disappear flashlight, reason unknown)
				solver->realtimeLights[i]->dirtyShadowmap = true;
			}

		if (svs.cameraDynamicNear && !svs.camera.isOrthogonal()) // don't change range in ortho, fixed range from setView() is better
		{
			// eye must already be updated here because next line depends on pos, up, right
			svs.camera.setRangeDynamically(solver,false,svs.cameraDynamicNearNumRays);
		}

		// when sky changes, ask all cubemaps for update
		{
			static unsigned previousEnvVersion = 0;
			rr::RRBuffer* env = solver->getEnvironment(0);
			if (env && env->version!=previousEnvVersion)
			{
				previousEnvVersion = env->version;
				// ask all cubemaps for update
				for (unsigned j=0;j<2;j++)
				{
					const rr::RRObjects& objects = j?solver->getDynamicObjects():solver->getStaticObjects();
					for (unsigned k=0;k<objects.size();k++)
					{
						rr::RRObject* object = objects[k];
						rr::RRBuffer* cube = object->illumination.getLayer(svs.layerRealtimeEnvironment);
						if (cube)
							cube->version = rand();
					}
				}
			}
		}

		if (svs.renderLightDirectActive() || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			for (unsigned i=0;i<solver->realtimeLights.size();i++)
				solver->realtimeLights[i]->shadowTransparencyRequested = svs.shadowTransparency;
			rr::RRSolver::CalculateParameters params;
			params.rr::RRSolver::Multipliers::operator=(svs.getMultipliersIndirect());
			if (svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
			{
				// rendering indirect -> calculate will update shadowmaps, possibly resample environment and emissive maps, improve indirect
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

		// shared plugin data
		rr_gl::PluginParamsShared ppShared;
		rr::RRCamera ppSharedCamera = svs.camera; // we need to make small tweaks in camera before we send it to renderer, this is our local copy
		ppShared.camera = &ppSharedCamera;
		if (!svs.renderDof)
			ppSharedCamera.apertureDiameter = 0;
		int w, h;
		canvasWindow->GetClientSize(&w, &h);
		ppShared.viewport[0] = (_takingSshot && svframe->userPreferences.sshotEnhanced) ? 0 : (w-winWidth)/2; // [#63] choose active area in canvas
		ppShared.viewport[1] = (_takingSshot && svframe->userPreferences.sshotEnhanced) ? 0 : (h-winHeight)/2;
		ppShared.viewport[2] = winWidth;
		ppShared.viewport[3] = winHeight;
		rr_gl::glViewport(ppShared.viewport[0],ppShared.viewport[1],ppShared.viewport[2],ppShared.viewport[3]);
		ppShared.srgbCorrect = svs.srgbCorrect; // affects image even with direct lighting disabled (by adding dif+spec+emis correctly)
		ppShared.brightness = rr::RRVec4( svs.renderTonemapping ? svs.tonemapping.color.RRVec3::avg() * pow(svs.tonemapping.gamma,0.45f) : 1 );
		ppShared.gamma = svs.renderTonemapping ? svs.tonemapping.gamma : 1;

		// start chaining plugins
		const rr_gl::PluginParams* pluginChain = nullptr;
		if (svs.pathEnabled && svframe->lastInteractionTime.secondsPassed()>0.2f)
		{
			//
			// renderpath 1 - pathtracer
			//
			if (g_lastRawInteractionTime.secondsPassed()<0.3f) // at least 0.3s for drop-down list animation to finish
				return true; // pretend that SwapBuffers was already called
			rr::RRReportInterval report(rr::INF3,"pathtrace scene...\n");
			if (!pathTracedBuffer)
				pathTracedBuffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGBF,false,nullptr);
			unsigned pathTraceWidth = winWidth;
			unsigned pathTraceHeight = winHeight;
			if (pathTraceWidth!=pathTracedBuffer->getWidth() || pathTraceHeight!=pathTracedBuffer->getHeight())
			{
				pathTracedBuffer->reset(rr::BT_2D_TEXTURE,pathTraceWidth,pathTraceHeight,1,rr::BF_RGBF,false,nullptr); // embree accepts only RGB,RGBA,RGBF
				pathTracedAccumulator = 0;
			}
			ppSharedCamera.setAspect(pathTraceWidth/(float)pathTraceHeight); // [#37] not ,0.5
			rr::RRSolver::PathTracingParameters params;
			params.direct = svs.getMultipliersDirect();
			params.indirect = svs.getMultipliersIndirect();
			if (!svs.renderLightDirect)
				params.direct.lightMultiplier = 0;
			if (!svs.renderMaterialEmission)
				params.direct.materialEmittanceMultiplier = 0;
			params.brdfTypes = rr::RRMaterial::BrdfType( (svs.renderMaterialDiffuse?rr::RRMaterial::BRDF_DIFFUSE:0) + (svs.renderMaterialSpecular?rr::RRMaterial::BRDF_SPECULAR:0) + ((svs.renderMaterialTransparency!=T_OPAQUE)?rr::RRMaterial::BRDF_TRANSMIT:0) );
			unsigned shortcut = (unsigned)sqrtf((float)(pathTracedAccumulator/10)); // starts at 0, increases on frames 10, 40, 90, 160 etc
			params.useFlatNormalsSinceDepth = shortcut+2;
			params.useSolverDirectSinceDepth = svs.pathShortcut ? shortcut+1 : UINT_MAX;
			params.useSolverIndirectSinceDepth = svs.pathShortcut ? shortcut : UINT_MAX;

			// solver->pathTraceFrame() with decorations that make it abortable
			boost::thread t([](rr::RRSolver* solver)
			{
				while (!isKeyOrButtonPressed())
					boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
				solver->aborting = true;
			},solver);
			if (!pathTracedAccumulator) pathTracedBuffer->clear(); // if we interrupt pathtracing first frame, it would contains uninitialized pixels
			// danger: if pathTraceFrame() throws, we don't interrupt t and it writes to solver->aborting forever. when we delete solver, it crashes
			//         BOOST_SCOPE_EXIT here would fix it, but it makes code harder to read
			solver->pathTraceFrame(ppSharedCamera,pathTracedBuffer,pathTracedAccumulator,params);
			t.interrupt(); // boost::this_thread::sleep_for() is interruptible
			t.join(); // without join, release version terminates ocassionally when isKeyOrButtonPressed detects button click
			bool skipThisFrame = solver->aborting;
			solver->aborting = false;
			prevIdleTime.setNow(); // without this, OnIdle would think that we spent all time since last OnIdle pressing key
			if (skipThisFrame)
				return true; // pretend that SwapBuffers was already called

			rr_gl::ToneParameters tp;
			if (svs.renderTonemapping)
			{
				tp = svs.tonemapping;
				solver->getColorSpace()->toLinear(tp.color);
			}
			tp.gamma *= 0.45f;
			pathTracedAccumulator++;
			solver->getRenderer()->getTextureRenderer()->render2D(rr_gl::getTexture(pathTracedBuffer,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE),&tp,0,0,1,1);
			glClear(GL_DEPTH_BUFFER_BIT); // necessary for selection plugin

			// copy&paste-d plugins that make sense with pathtracer

			// selection plugin
			rr_gl::PluginParamsScene ppSelection(pluginChain,nullptr);
			const EntityIds& selectedEntityIds = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);
			rr::RRObjects selectedObjects;
			for (EntityIds::const_iterator i=selectedEntityIds.begin();i!=selectedEntityIds.end();++i)
				if ((*i).type==ST_OBJECT)
					selectedObjects.push_back(solver->getObject((*i).index));
			ppSelection.objects = &selectedObjects;
			ppSelection.uberProgramSetup.OBJECT_SPACE = true;
			ppSelection.uberProgramSetup.POSTPROCESS_NORMALS = true;
			ppSelection.wireframe = true;
			if (selectedObjects.size() && !_takingSshot && !svs.renderStereo && !svs.renderPanorama)
				pluginChain = &ppSelection;

			// lens flare plugin
			rr_gl::PluginParamsLensFlare ppLensFlare(pluginChain,svs.lensFlareSize,svs.lensFlareId,solver,64);
			if (svs.renderLensFlare && !svs.camera.isOrthogonal() && !svs.renderPanorama && !svs.renderStereo)
				pluginChain = &ppLensFlare;

			// FPS plugin
			rr_gl::PluginParamsFPS ppFPS(pluginChain,fpsCounter,false);
			if (svs.renderFPS)
				pluginChain = &ppFPS;

			// final render
			solver->getRenderer()->render(pluginChain,ppShared);
		}
		else
		{
			//
			// renderpath 2 - rasterizer
			//
			rr::RRReportInterval report(rr::INF3,"render scene...\n");

			// skybox plugin
			rr_gl::PluginParamsSky ppSky(pluginChain,solver,svs.getMultipliersDirect().environmentMultiplier);
			pluginChain = &ppSky;

			// selection plugin
			rr_gl::PluginParamsScene ppSelection(pluginChain,nullptr);
			const EntityIds& selectedEntityIds = svframe->m_sceneTree->getEntityIds(SVSceneTree::MEI_SELECTED);
			rr::RRObjects selectedObjects;
			for (EntityIds::const_iterator i=selectedEntityIds.begin();i!=selectedEntityIds.end();++i)
				if ((*i).type==ST_OBJECT)
					selectedObjects.push_back(solver->getObject((*i).index));
			ppSelection.objects = &selectedObjects;
			ppSelection.uberProgramSetup.OBJECT_SPACE = true;
			ppSelection.uberProgramSetup.POSTPROCESS_NORMALS = true;
			ppSelection.wireframe = true;
			if (selectedObjects.size() && !_takingSshot)
				pluginChain = &ppSelection;

			// scene plugin
			rr_gl::PluginParamsScene ppScene(pluginChain,solver);
			ppScene.multipliers = svs.getMultipliersDirect();
			ppScene.uberProgramSetup.SHADOW_MAPS = 1;
			ppScene.uberProgramSetup.LIGHT_DIRECT = svs.renderLightDirectActive();
			ppScene.uberProgramSetup.LIGHT_DIRECT_COLOR = svs.renderLightDirectActive();
			ppScene.uberProgramSetup.LIGHT_DIRECT_MAP = svs.renderLightDirectActive();
			ppScene.uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = svs.renderLightDirectActive();
			ppScene.uberProgramSetup.LIGHT_INDIRECT_CONST = svs.renderLightIndirect==LI_CONSTANT;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
			ppScene.uberProgramSetup.LIGHT_INDIRECT_MAP = svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = svs.renderLDMEnabled();
			ppScene.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE =
			ppScene.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = svs.raytracedCubesEnabled && solver->getStaticObjects().size()+solver->getDynamicObjects().size()<svs.raytracedCubesMaxObjects;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT = ppScene.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR && svs.renderMaterialTransparencyRefraction;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = svs.mirrorsEnabled && svs.mirrorsDiffuse;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = svs.mirrorsEnabled && svs.mirrorsSpecular;
			ppScene.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = svs.mirrorsEnabled && svs.mirrorsMipmaps;
			ppScene.uberProgramSetup.MATERIAL_DIFFUSE = svs.renderMaterialDiffuse;
			ppScene.uberProgramSetup.MATERIAL_DIFFUSE_CONST = svs.renderMaterialDiffuse && svs.renderMaterialDiffuseColor;
			ppScene.uberProgramSetup.MATERIAL_DIFFUSE_MAP = svs.renderMaterialDiffuse && svs.renderMaterialDiffuseColor && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_SPECULAR = svs.renderMaterialSpecular;
			ppScene.uberProgramSetup.MATERIAL_SPECULAR_MAP = svs.renderMaterialSpecular && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_SPECULAR_CONST = svs.renderMaterialSpecular;
			ppScene.uberProgramSetup.MATERIAL_EMISSIVE_CONST = svs.renderMaterialEmission;
			ppScene.uberProgramSetup.MATERIAL_EMISSIVE_MAP = svs.renderMaterialEmission && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = svs.renderMaterialTransparency!=T_OPAQUE;
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = svs.renderMaterialTransparency!=T_OPAQUE && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = svs.renderMaterialTransparency!=T_OPAQUE;
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = svs.renderMaterialTransparency==T_ALPHA_BLEND || svs.renderMaterialTransparency==T_RGB_BLEND;
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = svs.renderMaterialTransparency==T_RGB_BLEND && !ppScene.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT; // renderer processes _TO_RGB as multipass, this is not necessary (and not working) with _REFRACT, therefore we disable _TO_RGB when _REFRACT
			ppScene.uberProgramSetup.MATERIAL_TRANSPARENCY_FRESNEL = svs.renderMaterialTransparencyFresnel;
			ppScene.uberProgramSetup.MATERIAL_BUMP_MAP = svs.renderMaterialBumpMaps && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_NORMAL_MAP_FLOW = svs.renderMaterialBumpMaps && svs.renderMaterialTextures;
			ppScene.uberProgramSetup.MATERIAL_CULLING = svs.renderMaterialSidedness;
			// There was updateLayers=true by accident since rev 5221 (I think development version of mirrors needed it to update mirrors, and I forgot to revert it).
			// It was error, because "true" when rendering static lmap allows PluginScene [#12] to use multiobj.
			// And as lmap is always stored in 1obj, PluginScene can't find it in multiobj. Error was visible only with specular cubes disabled (they also enforce 1obj).
			ppScene.updateLayerLightmap = svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT;
			ppScene.updateLayerEnvironment = svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT;
			ppScene.layerLightmap = (svs.renderLightIndirect==LI_LIGHTMAPS)?svs.layerBakedLightmap:((svs.renderLightIndirect==LI_AMBIENTMAPS)?svs.layerBakedAmbient:svs.layerRealtimeAmbient);
			ppScene.layerEnvironment = svs.raytracedCubesEnabled?((svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)?svs.layerRealtimeEnvironment:svs.layerBakedEnvironment):UINT_MAX;
			ppScene.layerLDM = svs.renderLDMEnabled()?svs.layerBakedLDM:UINT_MAX;
			ppScene.wireframe = svs.renderWireframe;
			ppScene.mirrorOcclusionQuery = svs.mirrorsOcclusion;
			if (!svs.renderDof || !svs.dofAccumulated) // when accumulating dof, stop animations
				ppScene.animationTime = svs.referenceTime.secondsPassed();
			if (ppScene.animationTime>1000)
				svs.referenceTime.addSeconds(1000);
			pluginChain = &ppScene;

			// SSGI plugin
			rr_gl::PluginParamsSSGI ppSSGI(pluginChain,svs.ssgiIntensity,svs.ssgiRadius,svs.ssgiAngleBias);
			if (svs.ssgiEnabled)
				pluginChain = &ppSSGI;

			// Contours plugin
			rr_gl::PluginParamsContours ppContours(pluginChain,rr::RRVec4(svs.contoursSilhouetteColor,1-svs.contoursSilhouetteColor.avg()),rr::RRVec4(svs.contoursCreaseColor,1-svs.contoursCreaseColor.avg()));
			if (svs.contoursEnabled)
				pluginChain = &ppContours;

			// DOF plugin
			static unsigned oldStateVersion;
			bool resetAccumulation = svframe->stateVersion!=oldStateVersion;
			oldStateVersion = svframe->stateVersion;
			rr_gl::PluginParamsDOF ppDOF(pluginChain,svs.dofAccumulated,svs.dofApertureShapeFilename);
			if (svs.renderDof && !(svs.dofAccumulated && resetAccumulation))
			{
				pluginChain = &ppDOF;
				if (svs.dofAutomaticFocusDistance)
				{
					rr::RRRay ray;
					svs.camera.getRay(svs.camera.getScreenCenter(),ray.rayOrigin,ray.rayDir);
					ray.rayDir.normalize();
					ray.rayLengthMin = svs.camera.getNear();
					ray.rayLengthMax = svs.camera.getFar();
					ray.rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
					ray.hitObject = solver->getMultiObject(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
					ray.collisionHandler = collisionHandler;
					float ratio = sqrtf(svs.camera.dofFar/svs.camera.dofNear);
					if (!_finite(ratio) || ratio<1)
						ratio = 1;
					if (solver->getCollider()->intersect(ray))
					{
						svs.camera.dofNear = ray.hitDistance/ratio;
						svs.camera.dofFar = ray.hitDistance*ratio;
					}
					else
					{
						svs.camera.dofNear = svs.camera.getFar()/ratio;
						svs.camera.dofFar = svs.camera.getFar()*ratio;
					}
				}
			}

			// lens flare plugin
			rr_gl::PluginParamsLensFlare ppLensFlare(pluginChain,svs.lensFlareSize,svs.lensFlareId,solver,64);
			if (svs.renderLensFlare && !svs.camera.isOrthogonal() && !svs.renderPanorama && !svs.renderStereo)
				pluginChain = &ppLensFlare;

			// panorama plugin
			rr_gl::PluginParamsPanorama ppPanorama(pluginChain);
			if (svs.renderPanorama)
				pluginChain = &ppPanorama;

			// bloom plugin (should go after panorama, to avoid discontinuities)
			rr_gl::PluginParamsBloom ppBloom(pluginChain,svs.bloomThreshold);
			if (svs.renderBloom)
				pluginChain = &ppBloom;

			// stereo plugin
			rr_gl::PluginParamsStereo ppStereo(pluginChain,vrDevice);
			if (svs.renderStereo)
			{
				switch (ppShared.camera->stereoMode)
				{
					// in interlaced mode, check whether image starts on odd or even scanline
					case rr::RRCamera::SM_INTERLACED:
						{
							GLint viewport[4];
							rr_gl::glGetIntegerv(GL_VIEWPORT,viewport);
							int trueWinWidth, trueWinHeight;
							canvasWindow->GetClientSize(&trueWinWidth, &trueWinHeight);
							if ((canvasWindow->GetScreenPosition().y+trueWinHeight-viewport[1]-viewport[3])&1)
								ppSharedCamera.stereoSwap = !ppSharedCamera.stereoSwap;
						}
						break;
					default: // prevents clang warning
						break;
				}
				pluginChain =  &ppStereo;
			}

			// accumulation plugin
			rr_gl::PluginParamsAccumulation ppAccumulation(pluginChain,resetAccumulation);
			if (svs.renderDof && svs.dofAccumulated)
				pluginChain = &ppAccumulation;

			// tonemapping plugin
			rr_gl::ToneParameters tp = svs.tonemapping;
			tp.color = svs.tonemapping.color.RRVec3::avg() ? svs.tonemapping.color / svs.tonemapping.color.RRVec3::avg() : svs.tonemapping.color; // don't apply color's brightness, it was already applied by ppScene
			tp.gamma = 1; // don't apply gamma, it was already applied by ppScene
			rr_gl::PluginParamsToneMapping ppToneMapping(pluginChain,tp);
			if (svs.renderTonemapping)
				pluginChain = &ppToneMapping;

			// tonemapping adjustment plugin
			static rr::RRTime time;
			float secondsSinceLastFrame = time.secondsSinceLastQuery();
			bool adjustingTonemapping = (secondsSinceLastFrame>0 && secondsSinceLastFrame<10)
				&& svs.renderTonemapping
				&& svs.tonemappingAutomatic
				&& !svs.renderWireframe
				&& (((svs.renderLightIndirect==LI_LIGHTMAPS || svs.renderLightIndirect==LI_AMBIENTMAPS) && solver->containsLightSource())
					|| ((svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT) && solver->containsRealtimeGILightSource())
					|| svs.renderLightIndirect==LI_CONSTANT
					|| (solver->getEnvironment() && solver->getStaticObjects().size()+solver->getDynamicObjects().size()==0)
					);
			rr_gl::PluginParamsToneMappingAdjustment ppToneMappingAdjustment(pluginChain,svs.tonemapping.color,secondsSinceLastFrame*svs.tonemappingAutomaticSpeed,svs.tonemappingAutomaticTarget);
			if (adjustingTonemapping)
				pluginChain = &ppToneMappingAdjustment;

			// FPS plugin
			rr_gl::PluginParamsFPS ppFPS(pluginChain,fpsCounter,false);
			if (svs.renderFPS)
				pluginChain = &ppFPS;

			// showDDI plugin
			rr_gl::PluginParamsShowDDI ppShowDDI(pluginChain,(svs.initialInputSolver&&svs.renderLightIndirect==LI_NONE)?svs.initialInputSolver:solver);
			if (svs.renderDDI)
				pluginChain = &ppShowDDI;

			// final render
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			solver->getRenderer()->render(pluginChain,ppShared);


			// report statistics
			if (svframe->userPreferences.testingLogMore)
			{
				for (rr_gl::NamedCounter* counter = solver->getRenderer()->getCounters(); counter; counter=counter->next)
				{
					rr::RRReporter::report(rr::INF2,"%s %d\n",counter->name,counter->count);
					counter->count = 0;
				}
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
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			rr_gl::Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,nullptr,0,1,&svs.tonemapping.color,svs.tonemapping.gamma,nullptr);
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
			program = uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,nullptr,0,1,&svs.tonemapping.color,svs.tonemapping.gamma,nullptr);
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
				rr_gl::glEnable(GL_BLEND);
				rr_gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				textureRenderer->render2D(rr_gl::getTexture(vignetteImage,false,false),nullptr,0,0,1,1);
				rr_gl::glDisable(GL_BLEND);
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
					rr_gl::glEnable(GL_BLEND);
					rr_gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					textureRenderer->render2D(rr_gl::getTexture(logoImage,false,false),nullptr,1-w,1-h,w,h);
					rr_gl::glDisable(GL_BLEND);
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
				uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,nullptr,0,1,nullptr,1,nullptr);
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
			{
				rr::RRVec3 mini,maxi,pivot;
				svframe->m_sceneTree->getAABBOf(selectedEntityIds,PP_CENTER,mini,maxi,pivot);
				renderedIcons.addXYZ(pivot,(&selectedEntityIds==&autoEntityIds)?selectedTransformation:IC_STATIC,svs.camera);
			}
			entityIcons->renderIcons(renderedIcons,solver->getRenderer()->getTextureRenderer(),svs.camera,solver->getCollider(),collisionHandler,svs);
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
			uberProgramSetup.useProgram(solver->getUberProgram(),&svs.camera,nullptr,0,1,nullptr,1,nullptr);
		}

		// gather information about scene
		unsigned numLights = solver->getLights().size();
		static rr::RRTime time;
		const rr::RRObject* multiObject = solver->getMultiObject();
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : nullptr;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;

		// gather information about selected object
		rr::RRObject* singleObject = solver->getObject(svs.selectedObjectIndex);
		const rr::RRMesh* singleMesh = singleObject ? singleObject->getCollider()->getMesh() : nullptr;
		unsigned numTrianglesSingle = singleMesh ? singleMesh->getNumTriangles() : 0;

		// gather information about selected point and triangle (pointed by mouse)
		bool                        selectedPointValid = false; // true = all selectedXxx below are valid
		const rr::RRObject*         selectedPointObject = nullptr;
		const rr::RRMesh*           selectedPointMesh = nullptr;
		rr::RRMesh::TangentBasis    selectedPointBasis;
		rr::RRMesh::TriangleBody    selectedTriangleBody;
		rr::RRMesh::Triangle        selectedTriangle;
		rr::RRVec3                  selectedTriangleCoordsLocal[3];
		rr::RRVec3                  selectedTriangleCoordsWorld[3];
		rr::RRMesh::TriangleNormals selectedTriangleNormals;
		rr::RRRay ray; // filled only when !svs.renderLightmaps2d
		if (!svs.renderLightmaps2d)
		{
			// ray and collisionHandler are used in this block
			svs.camera.getRay(mousePositionInWindow,ray.rayOrigin,ray.rayDir);
			ray.rayDir.normalize();
			ray.rayLengthMin = svs.camera.getNear();
			ray.rayLengthMax = svs.camera.getFar();
			ray.rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			ray.hitObject = solver->getMultiObject(); // solver->getCollider()->intersect() usually sets hitObject, but sometimes it does not, we set it instead
			ray.collisionHandler = collisionHandler;
			if (solver->getCollider()->intersect(ray))
			{
				ray.convertHitFromMultiToSingleObject(solver);
				selectedPointValid = true;
				selectedPointObject = ray.hitObject;
				selectedPointMesh = selectedPointObject->getCollider()->getMesh();
				const rr::RRMatrix3x4Ex& hitMatrix = selectedPointObject->getWorldMatrixRef();
				selectedPointMesh->getTriangle(ray.hitTriangle,selectedTriangle);
				selectedPointMesh->getVertex(selectedTriangle[0],selectedTriangleCoordsLocal[0]);
				selectedPointMesh->getVertex(selectedTriangle[1],selectedTriangleCoordsLocal[1]);
				selectedPointMesh->getVertex(selectedTriangle[2],selectedTriangleCoordsLocal[2]);
				hitMatrix.transformPosition(selectedTriangleCoordsWorld[0] = selectedTriangleCoordsLocal[0]);
				hitMatrix.transformPosition(selectedTriangleCoordsWorld[1] = selectedTriangleCoordsLocal[1]);
				hitMatrix.transformPosition(selectedTriangleCoordsWorld[2] = selectedTriangleCoordsLocal[2]);
				selectedPointMesh->getTriangleBody(ray.hitTriangle,selectedTriangleBody);
				selectedPointMesh->getTriangleNormals(ray.hitTriangle,selectedTriangleNormals);
				hitMatrix.transformPosition(selectedTriangleBody.vertex0);
				hitMatrix.transformDirection(selectedTriangleBody.side1);
				hitMatrix.transformDirection(selectedTriangleBody.side2);
				for (unsigned i=0;i<3;i++)
				{
					hitMatrix.transformNormal(selectedTriangleNormals.vertex[i].normal);
					hitMatrix.transformNormal(selectedTriangleNormals.vertex[i].tangent);
					hitMatrix.transformNormal(selectedTriangleNormals.vertex[i].bitangent);
				}
				selectedPointBasis.normal = selectedTriangleNormals.vertex[0].normal + (selectedTriangleNormals.vertex[1].normal-selectedTriangleNormals.vertex[0].normal)*ray.hitPoint2d[0] + (selectedTriangleNormals.vertex[2].normal-selectedTriangleNormals.vertex[0].normal)*ray.hitPoint2d[1];
				selectedPointBasis.tangent = selectedTriangleNormals.vertex[0].tangent + (selectedTriangleNormals.vertex[1].tangent-selectedTriangleNormals.vertex[0].tangent)*ray.hitPoint2d[0] + (selectedTriangleNormals.vertex[2].tangent-selectedTriangleNormals.vertex[0].tangent)*ray.hitPoint2d[1];
				selectedPointBasis.bitangent = selectedTriangleNormals.vertex[0].bitangent + (selectedTriangleNormals.vertex[1].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray.hitPoint2d[0] + (selectedTriangleNormals.vertex[2].bitangent-selectedTriangleNormals.vertex[0].bitangent)*ray.hitPoint2d[1];
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
			drawTangentBasis(ray.hitPoint3d,selectedPointBasis);
			drawTriangle(selectedTriangleBody);
		}



		// render helper text, using own shader (because text output ignores color passed to line shader)
		if (svs.renderHelpers && !_takingSshot && fontInited)
		{
			centerObject = UINT_MAX; // reset pointer to texel in the center of screen, it will be set again ~100 lines below
			centerTexel = UINT_MAX;
			centerTriangle = UINT_MAX;
			rr_gl::glDisable(GL_DEPTH_TEST);
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
				rr_gl::Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),nullptr,nullptr,0,1,nullptr,1,nullptr);
				program->sendUniform("lightIndirectConst",rr::RRVec4(1));
			}
			int x = 10;
			int y = 10;
			int h = canvasGetSize().y;
			unsigned numObjects = solver->getStaticObjects().size()+solver->getDynamicObjects().size();
			{
				// what direct
				const char* strDirect = svs.renderLightDirectActive()?"realtime":"";
				// what indirect
				const char* strIndirect = "?";
				switch (svs.renderLightIndirect)
				{
					case LI_REALTIME_FIREBALL: strIndirect = "fireball"; break;
					case LI_REALTIME_ARCHITECT: strIndirect = "architect"; break;
					case LI_LIGHTMAPS: strIndirect = "lightmaps"; break;
					case LI_AMBIENTMAPS: strIndirect = "ambientmaps"; break;
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
					case rr::RRSolver::NONE: strSolver = "none"; break;
					case rr::RRSolver::ARCHITECT: strSolver = "Architect"; break;
					case rr::RRSolver::FIREBALL: strSolver = "Fireball"; break;
					case rr::RRSolver::BOTH: strSolver = "both"; break;
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
				static const rr::RRObject* lastObject = nullptr;
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
							textOutput(x,y+=18,h,"scale: %s",bufferSelectedObj->getScaled()?"custom(usually sRGB)":"linear");
							//glColor3f(1,1,1);
						}
					}
				}
			}
			if (!svs.renderLightmaps2d)
			{
				if (selectedPointValid)
				{
					rr::RRMesh::PreImportNumber preTriangle = selectedPointMesh->getPreImportTriangle(ray.hitTriangle);
					const rr::RRMaterial* selectedTriangleMaterial = selectedPointObject->getTriangleMaterial(ray.hitTriangle,nullptr,nullptr);
					const rr::RRMaterial* material = selectedTriangleMaterial;
					rr::RRPointMaterial selectedPointMaterial;
					if (material && material->minimalQualityForPointMaterials<10000)
					{
						selectedPointObject->getPointMaterial(ray.hitTriangle,ray.hitPoint2d,solver->getColorSpace(),true,selectedPointMaterial);
						material = &selectedPointMaterial;
					}
					rr::RRMesh::TriangleMapping triangleMapping;
					selectedPointMesh->getTriangleMapping(ray.hitTriangle,triangleMapping,material?material->lightmap.texcoord:0);
					rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray.hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray.hitPoint2d[1];
					textOutput(x,y+=18*2,h,"[pointed by mouse]");
					if (selectedPointObject->isDynamic)
						textOutput(x,y+=18,h,"dynamic object: %ls",selectedPointObject->name.w_str());
					else
						textOutput(x,y+=18,h,"static object: %d/%" RR_SIZE_T "d",preTriangle.object,solver->getStaticObjects().size());
					rr::RRBuffer* selectedPointLightmap = selectedPointObject->illumination.getLayer(svs.layerBakedLightmap);
					textOutput(x,y+=18,h,"offline lightmap: %s %dx%d",selectedPointLightmap?(selectedPointLightmap->getType()==rr::BT_2D_TEXTURE?"per-pixel":"per-vertex"):"none",selectedPointLightmap?selectedPointLightmap->getWidth():0,selectedPointLightmap?selectedPointLightmap->getHeight():0);
					if (selectedPointObject->isDynamic)
					{
						textOutput(x,y+=18,h,"triangle in object: %d/%d",ray.hitTriangle,selectedPointMesh->getNumTriangles());
						textOutput(x,y+=18,h,"triangle in static scene:");
					}
					else
					{
						textOutput(x,y+=18,h,"triangle in object: %d/%d",preTriangle.index,solver->getObject(preTriangle.object)->getCollider()->getMesh()->getNumTriangles());
						textOutput(x,y+=18,h,"triangle in static scene: %d/%d",ray.hitTriangle,numTrianglesMulti);
					}
					for (unsigned i=0;i<3;i++)
						textOutput(x,y+=18,h,"trilocal[%d]: %f %f %f",i,selectedTriangleCoordsLocal[i][0],selectedTriangleCoordsLocal[i][1],selectedTriangleCoordsLocal[i][2]);
					for (unsigned i=0;i<3;i++)
						textOutput(x,y+=18,h,"triworld[%d]: %f %f %f",i,selectedTriangleCoordsWorld[i][0],selectedTriangleCoordsWorld[i][1],selectedTriangleCoordsWorld[i][2]);
					textOutput(x,y+=18,h,"uv in triangle: %f %f",ray.hitPoint2d[0],ray.hitPoint2d[1]);
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
							centerTriangle = ray.hitTriangle;
						}
					}
					if (centerObject==UINT_MAX)
					{
						// diagnose triangle
						centerObject = preTriangle.object;
						centerTriangle = ray.hitTriangle;
					}
					textOutput(x,y+=18,h,"distance: %f",ray.hitDistance);
					textOutput(x,y+=18,h,"pos: %f %f %f",ray.hitPoint3d[0],ray.hitPoint3d[1],ray.hitPoint3d[2]);
					textOutput(x,y+=18,h,"plane:  %f %f %f %f",ray.hitPlane[0],ray.hitPlane[1],ray.hitPlane[2],ray.hitPlane[3]);
					textOutput(x,y+=18,h,"normal: %f %f %f",selectedPointBasis.normal[0],selectedPointBasis.normal[1],selectedPointBasis.normal[2]);
					textOutput(x,y+=18,h,"tangent: %f %f %f",selectedPointBasis.tangent[0],selectedPointBasis.tangent[1],selectedPointBasis.tangent[2]);
					textOutput(x,y+=18,h,"bitangent: %f %f %f",selectedPointBasis.bitangent[0],selectedPointBasis.bitangent[1],selectedPointBasis.bitangent[2]);
					textOutput(x,y+=18,h,"side: %s",ray.hitFrontSide?"front":"back");
				}
				textOutput(x,y+=18*2,h,"numbers of casters/lights show potential, what is allowed");
			}
			if (svs.renderLightmaps2d)
			{
				rr::RRVec2 uv = lv.getCenterUv(canvasGetSize());
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
							rr::RRVec4 color = buffer->getElement(i+j*buffer->getWidth(),nullptr);
							textOutput(x,y+=18,h,"color: %f %f %f %f",color[0],color[1],color[2],color[3]);
						}
					}
				}
			}
			rr_gl::glEnable(GL_DEPTH_TEST);
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
			rr_gl::glEnable(GL_BLEND);
			rr_gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			textureRenderer->render2D(rr_gl::getTexture(helpImage,false,false),nullptr,(1-w)*0.5f,(1-h)*0.5f,w,h);
			rr_gl::glDisable(GL_BLEND);
		}
	}

	fpsCounter.addFrame();
	return result;
}


BEGIN_EVENT_TABLE(CanvasWindow, wxGLCanvas)
	EVT_SIZE(CanvasWindow::OnSize)
	EVT_PAINT(CanvasWindow::OnPaint)
	EVT_KEY_DOWN(CanvasWindow::OnKeyDown)
	EVT_KEY_UP(CanvasWindow::OnKeyUp)
	EVT_MOUSE_EVENTS(CanvasWindow::OnMouseEvent)
	EVT_CONTEXT_MENU(CanvasWindow::OnContextMenuCreate)
	EVT_IDLE(CanvasWindow::OnIdle)
END_EVENT_TABLE()

 
}; // namespace
