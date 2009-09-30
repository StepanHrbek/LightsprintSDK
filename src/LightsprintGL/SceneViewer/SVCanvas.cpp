// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVCanvas.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVLightIcons.h"
#include "SVRayLog.h"
#include "SVSaveLoad.h"
#include "SVSolver.h"
#include "SVFrame.h"
#include "Lightsprint/GL/Timer.h"
#include "../tmpstr.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif

#if !wxUSE_GLCANVAS
    #error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// SVCanvas

SVCanvas::SVCanvas( SceneViewerStateEx& _svs, SVFrame *_parent, wxSize _size)
	: wxGLCanvas(_parent, wxID_ANY, NULL, wxDefaultPosition, _size, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE , _T("GLCanvas")), svs(_svs)
{
	context = NULL;
	parent = _parent;
	solver = NULL;
	manuallyOpenedScene = NULL;
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

}

void SVCanvas::createContext()
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
	int major, minor;
	if (sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
	{
		rr::RRReporter::report(rr::ERRO,"OpenGL 2.0 capable graphics card is required.\n");
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
	solver = new SVSolver(svs);
	if (svs.initialInputSolver)
	{
		solver->setScaler(svs.initialInputSolver->getScaler());
		solver->setEnvironment(svs.initialInputSolver->getEnvironment());
		envToBeDeletedOnExit = false;
		solver->setStaticObjects(svs.initialInputSolver->getStaticObjects(),NULL,NULL,rr::RRCollider::IT_BSP_FASTER,svs.initialInputSolver); // smoothing and multiobject are taken from _solver
		solver->setLights(svs.initialInputSolver->getLights());
	}
	else
	if (!svs.sceneFilename.empty())
	{
		manuallyOpenedScene = new rr::RRScene(svs.sceneFilename.c_str());
		solver->setScaler(rr::RRScaler::createRgbScaler());
		solver->setEnvironment(manuallyOpenedScene->getEnvironment());
		envToBeDeletedOnExit = false;
		if (manuallyOpenedScene->getObjects())
			solver->setStaticObjects(*manuallyOpenedScene->getObjects(),NULL);
		if (manuallyOpenedScene->getLights() && manuallyOpenedScene->getLights()->size())
			solver->setLights(*manuallyOpenedScene->getLights());
		svs.autodetectCamera = true; // new scene, camera is not set
	}

	if (!svs.skyboxFilename.empty())
	{
		parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_ENV_RELOAD));
	}

	solver->observer = &svs.eye; // solver automatically updates lights that depend on camera
	if (solver->getNumObjects())
	{
		switch (svs.renderLightIndirect)
		{
			case LI_REALTIME_FIREBALL_LDM: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL_LDM)); break;
			case LI_REALTIME_FIREBALL:     parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL    )); break;
			case LI_STATIC_LIGHTMAPS:      parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_STATIC      )); break;
		}
	}

	// init rest
	lv = new SVLightmapViewer(svs.pathToShaders);
	if (svs.selectedLightIndex>=solver->getLights().size()) svs.selectedLightIndex = 0;
	if (svs.selectedObjectIndex>=solver->getNumObjects()) svs.selectedObjectIndex = 0;
	lightFieldQuadric = gluNewQuadric();
	lightFieldObjectIllumination = new rr::RRObjectIllumination(0);
	lightFieldObjectIllumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGB,true,NULL);
	lightFieldObjectIllumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,NULL);

	water = new Water(svs.pathToShaders,true,true);
	toneMapping = new ToneMapping(svs.pathToShaders);
	ray = rr::RRRay::create();
	collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();

	exitRequested = false;

#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif
}

SVCanvas::~SVCanvas()
{
	if (!svs.releaseResources)
	{
		// user requested fast exit without releasing resources
		// however, if scene is .gsa, we must ignore him and release resources (it shutdowns gamebryo)
		bool sceneMustBeReleased = false;
		if (svs.sceneFilename.size()>4)
		{
			const char* sceneExtension = svs.sceneFilename.c_str() + svs.sceneFilename.size()-4;
			if (!_stricmp(sceneExtension,".gsa"))
				sceneMustBeReleased = true;
		}
		if (!sceneMustBeReleased)
		{
			// save time and don't release scene
			return;
		}
	}

	// fps
	RR_SAFE_DELETE(fpsDisplay);
	RR_SAFE_DELETE(textureRenderer);

	// logo
	RR_SAFE_DELETE(logoImage);

	// vignette
	RR_SAFE_DELETE(vignetteImage);

	// help
	RR_SAFE_DELETE(helpImage);

	RR_SAFE_DELETE(collisionHandler);
	RR_SAFE_DELETE(ray);
	RR_SAFE_DELETE(toneMapping);
	RR_SAFE_DELETE(water);

	deleteAllTextures();
	// delete objects referenced by solver
	if (solver)
	{
		// delete all textures created by us
		if (solver->getEnvironment())
			((rr::RRBuffer*)solver->getEnvironment())->customData = NULL; //!!! customData is modified in const object
		for (unsigned i=0;i<solver->getNumObjects();i++)
			if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber))
				solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->customData = NULL;

		// delete all lightmaps for realtime rendering
		for (unsigned i=0;i<solver->getNumObjects();i++)
		{
			if (solver->getIllumination(i))
				RR_SAFE_DELETE(solver->getIllumination(i)->getLayer(svs.realtimeLayerNumber));
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
	RR_SAFE_DELETE(manuallyOpenedScene);
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
	// this is also necessary to update the context on some platforms
	// obsoleted in 2.9, probably no longer necessary
	//wxGLCanvas::OnSize(event);

	// set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);
	if (context)
	{
		SetCurrent(*context);
		glViewport(0, 0, (GLint) w, (GLint) h);
		winWidth = w;
		winHeight = h;
		svs.eye.setAspect( winWidth/(float)winHeight );
	}
}

void SVCanvas::OnKeyDown(wxKeyEvent& event)
{
	bool needsRefresh = false;
	long evkey = event.GetKeyCode();
	float speed = (event.GetModifiers()==wxMOD_SHIFT) ? 3 : 1;
	if (event.GetModifiers()==wxMOD_CONTROL) switch (evkey)
	{
		case 'T': // ctrl-t
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_RENDER_TEXTURES));
			break;
		case 'W': // ctrl-w
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_RENDER_WIREFRAME));
			break;
		case 'H': // ctrl-h
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_RENDER_HELPERS));
			break;
		case 'F': // ctrl-f
			parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_RENDER_FPS));
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
	}
	else switch(evkey)
	{
		case WXK_F11: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_WINDOW_FULLSCREEN)); break;

		case WXK_NUMPAD_ADD:
		case '+': svs.brightness *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_SUBTRACT:
		case '-': svs.brightness /= 1.2f; needsRefresh = true; break;

		//case '8': if (event.GetModifiers()==0) break; // ignore '8', but accept '8' + shift as '*', continue to next case
		case WXK_NUMPAD_MULTIPLY:
		case '*': svs.gamma *= 1.2f; needsRefresh = true; break;
		case WXK_NUMPAD_DIVIDE:
		case '/': svs.gamma /= 1.2f; needsRefresh = true; break;

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
			{
				exitRequested = true;
			}
			break;
//		default:
//			GetParent()->GetEventHandler()->ProcessEvent(event);
//			return;
	}

	solver->reportInteraction();
	if (needsRefresh)
	{
		Refresh(false);
	}
}

void SVCanvas::OnKeyUp(wxKeyEvent& event)
{
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

void SVCanvas::OnMouseEvent(wxMouseEvent& event)
{
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
	if (!solver)
	{
		return;
	}
	if (svs.renderLightmaps2d && lv)
	{
		lv->OnMouseEvent(event,GetSize());
		return;
	}
	static int prevX = 0;
	static int prevY = 0;
	if (event.LeftDown())
	{
		if (solver->realtimeLights.size())
		{
			if (selectedType!=ST_CAMERA)
			{
				selectedType = ST_CAMERA;
			}
			else
			{
				selectedType = ST_LIGHT;
			}
		}
	}
	else if (event.GetWheelRotation())
	{
		float fov = svs.eye.getFieldOfViewVerticalDeg();
		if (event.GetWheelRotation()<0)
		{
			if (fov>13) fov -= 10; else fov /= 1.4f;
		}
		if (event.GetWheelRotation()>0)
		{
			if (fov*1.4f<=3) fov *= 1.4f; else if (fov<130) fov += 10;
		}
		svs.eye.setFieldOfViewVerticalDeg(fov);
	}
	else if (event.RightDown())
	{
		prevX = event.GetX();
		prevY = event.GetY();
	}
	else if (event.Dragging() && event.RightIsDown())
	{
		if (!winWidth || !winHeight) return;
		int x = event.GetX() - prevX;
		int y = event.GetY() - prevY;
		prevX = event.GetX();
		prevY = event.GetY();
		if (x || y)
		{
#if defined(LINUX) || defined(linux)
			const float mouseSensitivity = 0.0002f;
#else
			const float mouseSensitivity = 0.005f;
#endif
			if (selectedType==ST_CAMERA || selectedType==ST_OBJECT)
			{
				svs.eye.angle -= mouseSensitivity*x*(svs.eye.getFieldOfViewVerticalDeg()/90);
				svs.eye.angleX -= mouseSensitivity*y*(svs.eye.getFieldOfViewVerticalDeg()/90);
				RR_CLAMP(svs.eye.angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			}
			else
			if (selectedType==ST_LIGHT)
			{
				if (svs.selectedLightIndex<solver->getLights().size())
				{
					solver->reportDirectIlluminationChange(svs.selectedLightIndex,true,true);
					Camera* light = solver->realtimeLights[svs.selectedLightIndex]->getParent();
					light->angle -= mouseSensitivity*x;
					light->angleX -= mouseSensitivity*y;
					RR_CLAMP(light->angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
					// changes position a bit, together with rotation
					// if we don't call it, solver updates light in a standard way, without position change
					light->pos += light->dir*0.3f;
					light->update();
					light->pos -= light->dir*0.3f;
				}
			}
			solver->reportInteraction();
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
	if (!winWidth) return; // can't work without window

	if ((svs.initialInputSolver && svs.initialInputSolver->aborting) || solver->aborting || exitRequested)
	{
		parent->Close(true);
		return;
	}

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
			if (speedForward) cam->moveForward(speedForward*meters);
			if (speedBack) cam->moveBack(speedBack*meters);
			if (speedRight) cam->moveRight(speedRight*meters);
			if (speedLeft) cam->moveLeft(speedLeft*meters);
			if (speedUp) cam->moveUp(speedUp*meters);
			if (speedDown) cam->moveDown(speedDown*meters);
			if (speedLean) cam->lean(speedLean*seconds*0.5f);
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

void SVCanvas::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);

	if (!context) return;
	SetCurrent(*context);

#ifdef _WIN32
	// init font for text outputs
	if (!fontInited)
	{
		fontInited = true;
		HFONT font = CreateFont(
			-15,							// Height Of Font
			0,								// Width Of Font
			0,								// Angle Of Escapement
			0,								// Orientation Angle
			FW_THIN,						// Font Weight
			FALSE,							// Italic
			FALSE,							// Underline
			FALSE,							// Strikeout
			ANSI_CHARSET,					// Character Set Identifier
			OUT_TT_PRECIS,					// Output Precision
			CLIP_DEFAULT_PRECIS,			// Clipping Precision
			ANTIALIASED_QUALITY,			// Output Quality
			FF_DONTCARE|FIXED_PITCH,		// Family And Pitch
			_T("System"));					// Font Name
		SelectObject(wglGetCurrentDC(), font);
		wglUseFontBitmaps(wglGetCurrentDC(), 0, 127, 1000);
		glListBase(1000);
	}
#endif

	rr::RRReportInterval report(rr::INF3,"display...\n");
	if (svs.renderLightmaps2d && lv)
	{
		lv->setObject(solver->getIllumination(svs.selectedObjectIndex)->getLayer((svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM)?svs.ldmLayerNumber:svs.staticLayerNumber),solver->getObject(svs.selectedObjectIndex),svs.renderLightmapsBilinear);
		lv->OnPaint(event,GetSize());
	}
	else
	{
		if (exitRequested || !winWidth || !winHeight) return; // can't display without window

		svs.eye.update();

		if (svs.cameraDynamicNear)
		{
			// eye must already be updated here because next line depends on up, right
			svs.eye.setRangeDynamically(solver->getMultiObjectCustom());
			svs.eye.update();
		}

		if (svs.renderLightDirect==LD_REALTIME || svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			rr::RRDynamicSolver::CalculateParameters params;
			if (svs.renderLightIndirect==LI_REALTIME_FIREBALL_LDM || svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
			{
				// rendering indirect -> calculate will update shadowmaps and improve indirect
				//params.qualityIndirectDynamic = 6;
				params.qualityIndirectStatic = 10000;
			}
			else
			{
				// rendering direct only -> calculate will update shadowmaps only
				params.qualityIndirectDynamic = 0;
				params.qualityIndirectStatic = 0;
			}
			solver->calculate(&params);
		}

		{
			rr::RRReportInterval report(rr::INF3,"render scene...\n");
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			svs.eye.setupForRender();

			UberProgramSetup miss = solver->getMaterialsInStaticScene();
			bool hasDif = miss.MATERIAL_DIFFUSE_CONST||miss.MATERIAL_DIFFUSE_MAP;
			bool hasEmi = miss.MATERIAL_EMISSIVE_CONST||miss.MATERIAL_EMISSIVE_MAP;
			bool hasTra = miss.MATERIAL_TRANSPARENCY_CONST||miss.MATERIAL_TRANSPARENCY_MAP||miss.MATERIAL_TRANSPARENCY_IN_ALPHA;

			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_COLOR = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_MAP = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = svs.renderLightDirect==LD_REALTIME;
			uberProgramSetup.LIGHT_INDIRECT_CONST = svs.renderLightIndirect==LI_CONSTANT;
			uberProgramSetup.LIGHT_INDIRECT_auto = svs.renderLightIndirect!=LI_CONSTANT && svs.renderLightIndirect!=LI_NONE;
			uberProgramSetup.MATERIAL_DIFFUSE = miss.MATERIAL_DIFFUSE; // "&& renderMaterialDiffuse" would disable diffuse refl completely. Current code only makes diffuse color white - I think this is what user usually expects.
			uberProgramSetup.MATERIAL_DIFFUSE_CONST = svs.renderMaterialDiffuse && !svs.renderMaterialTextures && hasDif;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = svs.renderMaterialDiffuse && svs.renderMaterialTextures && hasDif;
			uberProgramSetup.MATERIAL_SPECULAR = svs.renderMaterialSpecular && miss.MATERIAL_SPECULAR;
			uberProgramSetup.MATERIAL_SPECULAR_CONST = svs.renderMaterialSpecular && miss.MATERIAL_SPECULAR; // even when mixing only 0% and 100% specular, this must be enabled, otherwise all will have 100%
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = svs.renderMaterialEmission && !svs.renderMaterialTextures && hasEmi;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = svs.renderMaterialEmission && svs.renderMaterialTextures && hasEmi;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = svs.renderMaterialTransparency && hasTra && (miss.MATERIAL_TRANSPARENCY_CONST || !svs.renderMaterialTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = svs.renderMaterialTransparency && hasTra && (miss.MATERIAL_TRANSPARENCY_MAP && svs.renderMaterialTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = svs.renderMaterialTransparency && hasTra && (miss.MATERIAL_TRANSPARENCY_IN_ALPHA && svs.renderMaterialTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = svs.renderMaterialTransparency && hasTra && (miss.MATERIAL_TRANSPARENCY_BLEND
				// here we intentionally enable blend if alpha-keyed textures are disabled.
				// why? imagine alpha keyed tree, now disable transparency map. tree is 70% transparent, not blended, so whole tree disappears. better enable blend
				|| !svs.renderMaterialTextures);
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
			uberProgramSetup.POSTPROCESS_GAMMA = true;
			if (svs.renderWireframe) {glClear(GL_COLOR_BUFFER_BIT); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);}
			if (svs.renderWater && water && !svs.renderWireframe)
			{
				water->updateReflectionInit(winWidth/4,winHeight/4,&svs.eye,svs.waterLevel);
				glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
				uberProgramSetup.CLIP_PLANE = true;
				solver->renderScene(uberProgramSetup,NULL);
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
						water->render(svs.eye.getFar()*2,svs.eye.pos,svs.waterColor,light->direction,light->color);
						goto rendered;
					}
				}
				water->render(svs.eye.getFar()*2,svs.eye.pos,svs.waterColor,rr::RRVec3(0),rr::RRVec3(0));
rendered:
				solver->renderScene(uberProgramSetup,NULL);
				svs.eye.setFar(oldFar);
			}
			else
			{
				solver->renderScene(uberProgramSetup,NULL);
			}
			if (svs.renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			if (svs.adjustTonemapping
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
					toneMapping->adjustOperator(secondsSinceLastFrame,svs.brightness,svs.gamma,0.6f);
				oldTime = newTime;
			}
		}

		if (svs.renderHelpers)
		{
			rr::RRReportInterval report(rr::INF3,"render helpers 1...\n");
			// render light field
			if (lightField)
			{
				// update cube
				lightFieldObjectIllumination->envMapWorldCenter = rr::RRVec3(svs.eye.pos[0]+svs.eye.dir[0],svs.eye.pos[1]+svs.eye.dir[1],svs.eye.pos[2]+svs.eye.dir[2]);
				rr::RRVec2 sphereShift = rr::RRVec2(svs.eye.dir[2],-svs.eye.dir[0]).normalized()*0.05f;
				lightField->updateEnvironmentMap(lightFieldObjectIllumination,0);

				// diffuse
				// set shader (no direct light)
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true;
				uberProgramSetup.POSTPROCESS_BRIGHTNESS = svs.brightness!=rr::RRVec4(1);
				uberProgramSetup.POSTPROCESS_GAMMA = svs.gamma!=1;
				uberProgramSetup.MATERIAL_DIFFUSE = true;
				Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.brightness,svs.gamma,svs.waterLevel);
				uberProgramSetup.useIlluminationEnvMaps(program,lightFieldObjectIllumination,true);
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
				program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.brightness,svs.gamma,svs.waterLevel);
				uberProgramSetup.useIlluminationEnvMaps(program,lightFieldObjectIllumination,true);
				// render
				float worldMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, lightFieldObjectIllumination->envMapWorldCenter[0]+sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]+sphereShift[1],1};
				program->sendUniform("worldMatrix",worldMatrix,false,4);
				gluSphere(lightFieldQuadric, 0.05f, 16, 16);
			}
		}

		if (svs.renderIcons)
		{

			// render light frames
			solver->renderLights();
		}

		if (svs.renderHelpers
			)
		{
			// set shader
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
			uberProgramSetup.MATERIAL_DIFFUSE = 1;
			uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,0);
		}
		if (svs.renderHelpers)
		{
			// render lines
			glBegin(GL_LINES);
			enum {LINES=100, SIZE=100};
			for (unsigned i=0;i<LINES+1;i++)
			{
				if (i==LINES/2)
				{
					glColor3f(0,0,1);
					glVertex3f(0,-0.5*SIZE,0);
					glVertex3f(0,+0.5*SIZE,0);
				}
				else
					glColor3f(0,0,0.3f);
				float q = (i/float(LINES)-0.5f)*SIZE;
				glVertex3f(q,0,-0.5*SIZE);
				glVertex3f(q,0,+0.5*SIZE);
				glVertex3f(-0.5*SIZE,0,q);
				glVertex3f(+0.5*SIZE,0,q);
			}
			glEnd();
		}
	}

	if (svs.renderHelpers
		)
	{
		rr::RRReportInterval report(rr::INF3,"render helpers 2...\n");

		// gather information about scene
		unsigned numLights = solver->getLights().size();
		bool displayPhysicalMaterials = fmod(GETSEC,8)>4;
		const rr::RRObject* multiObject = displayPhysicalMaterials ? solver->getMultiObjectPhysical() : solver->getMultiObjectCustom();
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;

		// gather information about selected object
		rr::RRObject* singleObject = solver->getObject(svs.selectedObjectIndex);
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
			rr::RRVec3 dir = svs.eye.getDirection(mousePositionInWindow).normalized();
			ray->rayOrigin = svs.eye.pos;
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


		if (svs.renderHelpers)
		{
			// render debug rays, using previously set shader
			if ((!svs.renderLightmaps2d || !lv) && SVRayLog::size)
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
			if (!svs.renderLightmaps2d)
			{
				drawTangentBasis(selectedTriangleBody.vertex0,selectedTriangleNormals.vertex[0]);
				drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side1,selectedTriangleNormals.vertex[1]);
				drawTangentBasis(selectedTriangleBody.vertex0+selectedTriangleBody.side2,selectedTriangleNormals.vertex[2]);
				drawTangentBasis(ray->hitPoint3d,selectedPointBasis);
				drawTriangle(selectedTriangleBody);
			}

			// render text, using custom shader (because text output ignores color passed to previous shader)
			centerObject = UINT_MAX; // reset pointer to texel in the center of screen, it will be set again ~100 lines below
			centerTexel = UINT_MAX;
			centerTriangle = UINT_MAX;
			glDisable(GL_DEPTH_TEST);
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			gluOrtho2D(0, winWidth, winHeight, 0);
			{
				// set shader
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_CONST = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1,0);
				program->sendUniform("lightIndirectConst",1.0f,1.0f,1.0f,1.0f);
			}
			int x = 10;
			int y = 10;
			int h = GetSize().y;
			unsigned numObjects = solver->getNumObjects();
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
					if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber))
					{
						if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_VERTEX_BUFFER) numVbufs++; else
						if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE) numLmaps++;
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
				// what renderer
				const char* strRenderer = solver->usingOptimizedScene() ? "optimized" : "per-object";
				// print it
				textOutput(x,y+=18,h,"lighting direct=%s indirect=%s lightmaps=(%dx vbuf %dx lmap %dx none) solver=%s render=%s",
					strDirect,strIndirect,numVbufs,numLmaps,numObjects-numVbufs-numLmaps,strSolver,strRenderer);
			}
			if (!svs.renderLightmaps2d || !lv)
			{
				textOutput(x,y+=18*2,h,"[camera]");
				textOutput(x,y+=18,h,"pos: %f %f %f",svs.eye.pos[0],svs.eye.pos[1],svs.eye.pos[2]);
				textOutput(x,y+=18,h,"dir: %f %f %f",svs.eye.dir[0],svs.eye.dir[1],svs.eye.dir[2]);
				//textOutput(x,y+=18,h,"rig: %f %f %f",svs.eye.right[0],svs.eye.right[1],svs.eye.right[2]);
				//textOutput(x,y+=18,h,"up : %f %f %f",svs.eye.up[0],svs.eye.up[1],svs.eye.up[2]);
				//textOutput(x,y+=18,h,"FOV deg: x=%f y=%f asp=%f",svs.eye.getFieldOfViewHorizontalDeg(),svs.eye.getFieldOfViewVerticalDeg(),svs.eye.getAspect());
				GLint depthBits;
				glGetIntegerv(GL_DEPTH_BITS, &depthBits);
				textOutput(x,y+=18,h,"range: %f to %f (%dbit Z)",svs.eye.getNear(),svs.eye.getFar(),depthBits);
			}
			if (!svs.renderLightmaps2d || !lv) if (svs.selectedLightIndex<solver->realtimeLights.size())
			{
				RealtimeLight* rtlight = solver->realtimeLights[svs.selectedLightIndex];
				const rr::RRLight* rrlight = &rtlight->getRRLight();
				Camera* light = rtlight->getParent();
				textOutput(x,y+=18*2,h,"[light %d/%d]",svs.selectedLightIndex,solver->realtimeLights.size());
				textOutput(x,y+=18,h,"type: %s",(rrlight->type==rr::RRLight::POINT)?"point":((rrlight->type==rr::RRLight::SPOT)?"spot":"dir"));
				textOutput(x,y+=18,h,"pos: %f %f %f",light->pos[0],light->pos[1],light->pos[2]);
				textOutput(x,y+=18,h,"dir: %f %f %f",light->dir[0],light->dir[1],light->dir[2]);
				textOutput(x,y+=18,h,"color: %f %f %f",rrlight->color[0],rrlight->color[1],rrlight->color[2]);
				switch(rrlight->distanceAttenuationType)			
				{
					case rr::RRLight::NONE:        textOutput(x,y+=18,h,"dist att: none"); break;
					case rr::RRLight::PHYSICAL:    textOutput(x,y+=18,h,"dist att: physically correct"); break;
					case rr::RRLight::POLYNOMIAL:  textOutput(x,y+=18,h,"dist att: 1/(%f+%f*d+%f*d^2)",rrlight->polynom[0],rrlight->polynom[1],rrlight->polynom[2]); break;
					case rr::RRLight::EXPONENTIAL: textOutput(x,y+=18,h,"dist att: max(0,1-(distance/%f)^2)^%f",rrlight->radius,rrlight->fallOffExponent); break;
				}
				if (numTrianglesMulti<100000) // skip this expensive step for big scenes
				{
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
					textOutput(x,y+=18,h,"triangles lit: %d/%d",numLightReceivers,numTrianglesMulti);
					textOutput(x,y+=18,h,"triangles casting shadow: %f/%d",numShadowCasters/float(numObjects),numTrianglesMulti);
				}
			}
			if (singleMesh && svs.selectedObjectIndex<solver->getNumObjects())
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
				rr::RRBuffer* bufferSelectedObj = solver->getIllumination(svs.selectedObjectIndex) ? solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber) : NULL;
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
			if (multiMesh && (!svs.renderLightmaps2d || !lv))
			{
				if (selectedPointValid)
				{
					rr::RRMesh::PreImportNumber preTriangle = multiMesh->getPreImportTriangle(ray->hitTriangle);
					const rr::RRMaterial* triangleMaterial = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
					const rr::RRMaterial* material = triangleMaterial;
					rr::RRMaterial pointMaterial;
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
					rr::RRBuffer* objectsLightmap = solver->getIllumination(preTriangle.object)->getLayer(svs.staticLayerNumber);
					textOutput(x,y+=18,h,"object's lightmap: %s %dx%d",objectsLightmap?(objectsLightmap->getType()==rr::BT_2D_TEXTURE?"per-pixel":"per-vertex"):"none",objectsLightmap?objectsLightmap->getWidth():0,objectsLightmap?objectsLightmap->getHeight():0);
					textOutput(x,y+=18,h,"triangle in object: %d/%d",preTriangle.index,solver->getObject(preTriangle.object)->getCollider()->getMesh()->getNumTriangles());
					textOutput(x,y+=18,h,"triangle in scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
					textOutput(x,y+=18,h,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
					textOutput(x,y+=18,h,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
					rr::RRBuffer* bufferCenter = solver->getIllumination(preTriangle.object) ? solver->getIllumination(preTriangle.object)->getLayer(svs.staticLayerNumber) : NULL;
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
					if (material)
					{
						textOutput(x,y+=18,h,"material: %s [%s]",material->name?material->name:"",displayPhysicalMaterials?"physical":"sRGB");
						textOutput(x,y+=18,h," sides: %s %s",material->sideBits[0].renderFrom?"front":"",material->sideBits[1].renderFrom?"back":"");
						textOutputMaterialProperty(x,y+=18,h," diff",triangleMaterial->diffuseReflectance   ,material->diffuseReflectance   ,ray,multiMesh);
						textOutputMaterialProperty(x,y+=18,h," spec",triangleMaterial->specularReflectance  ,material->specularReflectance  ,ray,multiMesh);
						textOutputMaterialProperty(x,y+=18,h," emit",triangleMaterial->diffuseEmittance     ,material->diffuseEmittance     ,ray,multiMesh);
						textOutputMaterialProperty(x,y+=18,h," tran",triangleMaterial->specularTransmittance,material->specularTransmittance,ray,multiMesh);
						textOutput(x,y+=18,h," transparency: %s %s",triangleMaterial->specularTransmittanceInAlpha?"ALPHA":"RGB",triangleMaterial->specularTransmittanceKeyed?"1bit-keyed":"smooth-blended");
						textOutput(x,y+=18,h," refraction index: %f",material->refractionIndex);
						textOutput(x,y+=18,h," lightmap uv: %d",material->lightmapTexcoord);
						textOutput(x,y+=18,h," minimalQualityForPointMaterials: %d",material->minimalQualityForPointMaterials);
					}
					else
					{
						textOutput(x,y+=18,h,"material=NULL!!!");
					}
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
				rr::RRBuffer* buffer = solver->getIllumination(svs.selectedObjectIndex) ? solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber) : NULL;
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
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			glEnable(GL_DEPTH_TEST);
		}
	}


	// vignette
	if (svs.renderVignette)
	{
		if (!vignetteLoadAttempted)
		{
			vignetteLoadAttempted = true;
			if (!textureRenderer)
			{
				textureRenderer = new TextureRenderer(svs.pathToShaders);
			}
			RR_ASSERT(!vignetteImage);
			vignetteImage = rr::RRBuffer::load(tmpstr("%s../maps/vignette.png",svs.pathToShaders));
		}
		if (vignetteImage)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			textureRenderer->render2D(getTexture(vignetteImage,false,false),NULL,0,0,1,1);
			glDisable(GL_BLEND);
		}
	}

	// help
	if (svs.renderHelp)
	{
		if (!helpLoadAttempted)
		{
			helpLoadAttempted = true;
			if (!textureRenderer)
			{
				textureRenderer = new TextureRenderer(svs.pathToShaders);
			}
			RR_ASSERT(!helpImage);
			helpImage = rr::RRBuffer::load(tmpstr("%s../maps/sv_help.png",svs.pathToShaders));
			if (!helpImage)
			{
				wxMessageBox("To LOOK, move mouse with right button pressed.\n"
					"To MOVE, use arrows or wsadqzxc.\n"
					"To ZOOM, use wheel.\n"
					"To switch light/camera, left click.\n"
					"\n"
					"To change scene, skybox, lights, lighting techniques... use menu.",
					"Controls");
			}
		}
		if (helpImage)
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

	// logo
	if (svs.renderLogo)
	{
		if (!logoLoadAttempted)
		{
			logoLoadAttempted = true;
			if (!textureRenderer)
			{
				textureRenderer = new TextureRenderer(svs.pathToShaders);
			}
			RR_ASSERT(!logoImage);
			logoImage = rr::RRBuffer::load(tmpstr("%s../maps/sv_logo.png",svs.pathToShaders));
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

	// fps
	unsigned fps = fpsCounter.getFps();
	if (svs.renderFPS)
	{
		if (!fpsLoadAttempted)
		{
			fpsLoadAttempted = true;
			if (!textureRenderer)
			{
				textureRenderer = new TextureRenderer(svs.pathToShaders);
			}
			RR_ASSERT(!fpsDisplay);
			fpsDisplay = FpsDisplay::create(tmpstr("%s../maps/",svs.pathToShaders));
		}
		if (fpsDisplay)
		{
			fpsDisplay->render(textureRenderer,fps,winWidth,winHeight);
		}
	}

	// done
	SwapBuffers();
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
