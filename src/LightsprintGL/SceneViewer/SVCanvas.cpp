// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "SVCanvas.h"
#include "SVLightIcons.h"
#include "SVRayLog.h"
#include "SVSolver.h"
#include "SVFrame.h"
#include "Lightsprint/GL/Timer.h"
#include "../tmpstr.h"

#if !wxUSE_GLCANVAS
    #error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

namespace rr_gl
{


/////////////////////////////////////////////////////////////////////////////
//
// SVCanvas

SVCanvas::SVCanvas( SceneViewerParameters& params, SVFrame *_parent, SVLightProperties** parentsLightProperties)
	: wxGLCanvas(_parent, (wxGLCanvas*)NULL, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxCLIP_SIBLINGS|wxFULL_REPAINT_ON_RESIZE , _T("GLCanvas")), svs(params.svs)
{
	parent = _parent;
	originalSolver = params.solver;
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
	fireballLoadAttempted = 1;
	speedForward = 0;
	speedBack = 0;
	speedRight = 0;
	speedLeft = 0;
	speedUp = 0;
	speedDown = 0;
	speedLean = 0;
	exitRequested = 0;
	menuHandle = 0;
	ourEnv = 0;
	lv = NULL;
	centerObject = UINT_MAX;
	centerTexel = UINT_MAX;
	centerTriangle = UINT_MAX;
	ray = NULL;
	collisionHandler = NULL;
	fontInited = false;

	lightField = NULL;
	lightFieldQuadric = NULL;
	lightFieldObjectIllumination = NULL;

	lightProperties = parentsLightProperties;

	SetCurrent();

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
	solver = new SVSolver(params.pathToShaders,params.svs);
	solver->setScaler(params.solver->getScaler());
	solver->setEnvironment(params.solver->getEnvironment());
	solver->setStaticObjects(params.solver->getStaticObjects(),NULL,NULL,rr::RRCollider::IT_BSP_FASTER,params.solver); // smoothing and multiobject are taken from _solver
	solver->setLights(params.solver->getLights());
	solver->observer = &svs.eye; // solver automatically updates lights that depend on camera
	solver->loadFireball(NULL); // if fireball file already exists in temp, use it
	fireballLoadAttempted = 1;

	// init rest
	lv = new SVLightmapViewer(params.pathToShaders);
	ourEnv = 0;
	if (svs.selectedLightIndex>params.solver->getLights().size()) svs.selectedLightIndex = 0;
	if (svs.selectedObjectIndex>=solver->getNumObjects()) svs.selectedObjectIndex = 0;
	lightFieldQuadric = gluNewQuadric();
	lightFieldObjectIllumination = new rr::RRObjectIllumination(0);
	lightFieldObjectIllumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGB,true,NULL);
	lightFieldObjectIllumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,NULL);
	svs.renderAmbient = solver->getLights().size()==0 && svs.renderRealtime && !solver->getMaterialsInStaticScene().MATERIAL_EMISSIVE_CONST && !solver->getMaterialsInStaticScene().MATERIAL_EMISSIVE_MAP;

	water = new Water(params.pathToShaders,true,false);
	toneMapping = new ToneMapping(params.pathToShaders);
	ray = rr::RRRay::create();
	collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();

	exitRequested = false;
}

SVCanvas::~SVCanvas()
{
	RR_SAFE_DELETE(collisionHandler);
	RR_SAFE_DELETE(ray);
	RR_SAFE_DELETE(toneMapping);
	RR_SAFE_DELETE(water);

	// delete all textures created by us
	deleteAllTextures();
	if (solver->getEnvironment())
		((rr::RRBuffer*)solver->getEnvironment())->customData = NULL; //!!! customData is modified in const object
	for (unsigned i=0;i<solver->getNumObjects();i++)
		if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber))
			solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->customData = NULL;

	// delete all lightmap for realtime rendering
	for (unsigned i=0;i<solver->getNumObjects();i++)
	{
		if (solver->getIllumination(i))
			RR_SAFE_DELETE(solver->getIllumination(i)->getLayer(svs.realtimeLayerNumber));
	}

	if (ourEnv)
		delete solver->getEnvironment();
	RR_SAFE_DELETE(solver);
	RR_SAFE_DELETE(lv);
	RR_SAFE_DELETE(lightField);
	RR_SAFE_DELETE(lightFieldObjectIllumination);
	for (unsigned i=0;i<lightsToBeDeletedOnExit.size();i++) delete lightsToBeDeletedOnExit[i];
	lightsToBeDeletedOnExit.clear();
	gluDeleteQuadric(lightFieldQuadric);
	lightFieldQuadric = NULL;
}

void SVCanvas::OnSize(wxSizeEvent& event)
{
	// this is also necessary to update the context on some platforms
	wxGLCanvas::OnSize(event);

	// set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);
	#ifndef __WXMOTIF__
	if (GetContext())
	#endif
	{
		SetCurrent();
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
	switch(evkey)
	{
		case WXK_F11: parent->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_MAXIMIZE)); break;

		case '+': svs.brightness *= 1.2f; needsRefresh = true; break;
		case '-': svs.brightness /= 1.2f; needsRefresh = true; break;
		case '*': svs.gamma *= 1.2f; needsRefresh = true; break;
		case '/': svs.gamma /= 1.2f; needsRefresh = true; break;

		case '[': if (solver->getNumObjects()) svs.selectedObjectIndex = (svs.selectedObjectIndex+solver->getNumObjects()-1)%solver->getNumObjects(); needsRefresh = true; break;
		case ']': if (solver->getNumObjects()) svs.selectedObjectIndex = (svs.selectedObjectIndex+1)%solver->getNumObjects(); needsRefresh = true; break;

		case WXK_LEFT:
		case 'a':
		case 'A': speedLeft = 1; break;

		case WXK_DOWN:
		case 's':
		case 'S': speedBack = 1; break;

		case WXK_RIGHT:
		case 'd':
		case 'D': speedRight = 1; break;

		case WXK_UP:
		case 'w':
		case 'W': speedForward = 1; break;

		case WXK_PAGEUP:
		case 'q':
		case 'Q': speedUp = 1; break;

		case WXK_PAGEDOWN:
		case 'z':
		case 'Z': speedDown = 1; break;

		case 'x':
		case 'X': speedLean = -1; break;

		case 'c':
		case 'C': speedLean = +1; break;


		case 27:
			if (svs.render2d)
			{
				svs.render2d = 0;
			}
			else if (svs.fullscreen)
			{
				GetParent()->ProcessEvent(event);
			}
			else
			{
				exitRequested = true;
			}
			break;
		default:
			GetParent()->ProcessEvent(event);
			return;
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
	if (svs.render2d && lv)
	{
		lv->OnMouseEvent(event,GetSize());
		return;
	}
	static int prevX = 0;
	static int prevY = 0;
	if (event.LeftDown() && solver->realtimeLights.size())
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
				CLAMP(svs.eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
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
					CLAMP(light->angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
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

	solver->reportInteraction();
}

void SVCanvas::OnIdle(wxIdleEvent& event)
{
	if (!winWidth) return; // can't work without window

	if (originalSolver->aborting || solver->aborting || exitRequested)
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
		CLAMP(seconds,0.001f,0.3f);
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
			if (speedLean) cam->lean(speedLean*meters);
		}

		// light change report
		if (speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
		{
			if (cam!=&svs.eye) 
			{
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

void SVCanvas::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);

	#ifndef __WXMOTIF__
	if (!GetContext()) return;
	#endif

	SetCurrent();

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
			FF_DONTCARE|DEFAULT_PITCH,		// Family And Pitch
			"Arial");					// Font Name
		SelectObject(wglGetCurrentDC(), font); 
		wglUseFontBitmaps(wglGetCurrentDC(), 0, 127, 1000); 
		glListBase(1000);
	}
#endif

	rr::RRReportInterval report(rr::INF3,"display...\n");
	if (svs.render2d && lv)
	{
		lv->setObject(solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.renderLDM?svs.ldmLayerNumber:svs.staticLayerNumber),solver->getObject(svs.selectedObjectIndex)->getCollider()->getMesh(),svs.renderBilinear);
		lv->OnPaint(event,GetSize());
	}
	else
	{
		if (exitRequested || !winWidth || !winHeight) return; // can't display without window

		svs.eye.update();

		if (svs.cameraDynamicNear)
		{
			svs.eye.setNearDynamically(solver->getMultiObjectCustom());
			svs.eye.update();
		}

		if (svs.renderRealtime)
		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			rr::RRDynamicSolver::CalculateParameters params;
			//params.qualityIndirectDynamic = 6;
			params.qualityIndirectStatic = 10000;
			solver->calculate(&params);
		}

		{
			rr::RRReportInterval report(rr::INF3,"render scene...\n");
			glClear(GL_DEPTH_BUFFER_BIT);
			svs.eye.setupForRender();

			UberProgramSetup miss = solver->getMaterialsInStaticScene();
			bool hasDif = miss.MATERIAL_DIFFUSE_CONST||miss.MATERIAL_DIFFUSE_MAP;
			bool hasEmi = miss.MATERIAL_EMISSIVE_CONST||miss.MATERIAL_EMISSIVE_MAP;
			bool hasTra = miss.MATERIAL_TRANSPARENCY_CONST||miss.MATERIAL_TRANSPARENCY_MAP||miss.MATERIAL_TRANSPARENCY_IN_ALPHA;

			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = svs.renderRealtime;
			uberProgramSetup.LIGHT_DIRECT_COLOR = svs.renderRealtime;
			uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = svs.renderRealtime;
			uberProgramSetup.LIGHT_INDIRECT_CONST = svs.renderAmbient;
			uberProgramSetup.LIGHT_INDIRECT_auto = true;
			uberProgramSetup.MATERIAL_DIFFUSE = miss.MATERIAL_DIFFUSE; // "&& renderDiffuse" would disable diffuse refl completely. Current code only makes diffuse color white - I think this is what user usually expects.
			uberProgramSetup.MATERIAL_DIFFUSE_CONST = svs.renderDiffuse && !svs.renderTextures && hasDif;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = svs.renderDiffuse && svs.renderTextures && hasDif;
			uberProgramSetup.MATERIAL_SPECULAR = svs.renderSpecular && miss.MATERIAL_SPECULAR;
			uberProgramSetup.MATERIAL_SPECULAR_CONST = svs.renderSpecular && miss.MATERIAL_SPECULAR; // even when mixing only 0% and 100% specular, this must be enabled, otherwise all will have 100%
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = svs.renderEmission && !svs.renderTextures && hasEmi;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = svs.renderEmission && svs.renderTextures && hasEmi;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = svs.renderTransparent && hasTra && (miss.MATERIAL_TRANSPARENCY_CONST || !svs.renderTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = svs.renderTransparent && hasTra && (miss.MATERIAL_TRANSPARENCY_MAP && svs.renderTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = svs.renderTransparent && hasTra && (miss.MATERIAL_TRANSPARENCY_IN_ALPHA && svs.renderTextures);
			uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = svs.renderTransparent && hasTra && miss.MATERIAL_TRANSPARENCY_BLEND;
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
			uberProgramSetup.POSTPROCESS_GAMMA = true;
			if (svs.renderWireframe) {glClear(GL_COLOR_BUFFER_BIT); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);}
			if (svs.renderWater && water && !svs.renderWireframe)
			{
				water->updateReflectionInit(winWidth/4,winHeight/4,&svs.eye,0);
				glClear(GL_DEPTH_BUFFER_BIT);
				uberProgramSetup.CLIP_PLANE = true;
				solver->renderScene(uberProgramSetup,NULL);
				water->updateReflectionDone();
			}
			solver->renderScene(uberProgramSetup,NULL);
			if (svs.renderWater && water && !svs.renderWireframe) water->render(svs.eye.getFar()*2,svs.eye.pos);
			if (svs.renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			if (svs.adjustTonemapping && !svs.renderWireframe && (solver->getLights().size() || uberProgramSetup.LIGHT_INDIRECT_CONST || hasEmi)) // disable adjustment in completely dark scene
			{
				static TIME oldTime = 0;
				TIME newTime = GETTIME;
				float secondsSinceLastFrame = (newTime-oldTime)/float(PER_SEC);
				if (secondsSinceLastFrame>0 && secondsSinceLastFrame<10 && oldTime)
					toneMapping->adjustOperator(secondsSinceLastFrame,svs.brightness,svs.gamma);
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
				uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.brightness,svs.gamma);
				glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
				getTexture(lightFieldObjectIllumination->diffuseEnvMap,false,false)->reset(false,false);
				getTexture(lightFieldObjectIllumination->diffuseEnvMap,false,false)->bindTexture();
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
				Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&svs.brightness,svs.gamma);
				program->sendUniform("worldEyePos",svs.eye.pos[0],svs.eye.pos[1],svs.eye.pos[2]);
				glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
				getTexture(lightFieldObjectIllumination->specularEnvMap,false,false)->reset(false,false);
				getTexture(lightFieldObjectIllumination->specularEnvMap,false,false)->bindTexture();
				// render
				float worldMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, lightFieldObjectIllumination->envMapWorldCenter[0]+sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]+sphereShift[1],1};
				program->sendUniform("worldMatrix",worldMatrix,false,4);
				gluSphere(lightFieldQuadric, 0.05f, 16, 16);
			}


			// render lines
			{
				// set shader
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
				uberProgramSetup.MATERIAL_DIFFUSE = 1;
				uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,NULL,1);
			}
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

			// render light frames
			solver->renderLights();
		}
	}

	if (svs.renderHelpers)
	{
		rr::RRReportInterval report(rr::INF3,"render helpers 2...\n");
		// render properties
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
		glColor3f(1,1,1);
		glRasterPos2i(winWidth/2-4,winHeight/2+1);
		int x = 10;
		int y = 10;
		int h = GetSize().y;
		unsigned numObjects = solver->getNumObjects();
		{
			if (svs.renderRealtime)
			{
				const char* solverType = "";
				switch(solver->getInternalSolverType())
				{
					case rr::RRDynamicSolver::NONE: solverType = "no solver"; break;
					case rr::RRDynamicSolver::ARCHITECT: solverType = "Architect solver"; break;
					case rr::RRDynamicSolver::FIREBALL: solverType = "Fireball solver"; break;
					case rr::RRDynamicSolver::BOTH: solverType = "both solvers"; break;
				}
				textOutput(x,y+=18,h,"realtime GI lighting, %s, light detail map %s",solverType,svs.renderLDM?"on":"off");
			}
			else
			{
				unsigned vbuf = 0;
				unsigned lmap = 0;
				for (unsigned i=0;i<numObjects;i++)
				{
					if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber))
					{
						if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_VERTEX_BUFFER) vbuf++; else
						if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE) lmap++;
					}
				}
				textOutput(x,y+=18,h,"static lighting (%dx vbuf, %dx lmap, %dx none)",vbuf,lmap,numObjects-vbuf-lmap);
			}
		}
		if (!svs.render2d || !lv)
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
		unsigned numLights = solver->getLights().size();
		const rr::RRObject* multiObject = solver->getMultiObjectCustom();
		rr::RRObject* singleObject = solver->getObject(svs.selectedObjectIndex);
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : NULL;
		const rr::RRMesh* singleMesh = singleObject ? singleObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;
		unsigned numTrianglesSingle = singleMesh ? singleMesh->getNumTriangles() : 0;
		if (!svs.render2d || !lv) if (svs.selectedLightIndex<solver->realtimeLights.size())
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
			textOutput(x,y+=18,h,"lit: %s",svs.renderRealtime?"realtime":"static");
			rr::RRBuffer* bufferSelectedObj = solver->getIllumination(svs.selectedObjectIndex) ? solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber) : NULL;
			if (bufferSelectedObj)
			{
				if (svs.renderRealtime) glColor3f(0.5f,0.5f,0.5f);
				textOutput(x,y+=18,h,"[lightmap]");
				textOutput(x,y+=18,h,"type: %s",(bufferSelectedObj->getType()==rr::BT_VERTEX_BUFFER)?"PER VERTEX":((bufferSelectedObj->getType()==rr::BT_2D_TEXTURE)?"PER PIXEL":"INVALID!"));
				textOutput(x,y+=18,h,"size: %d*%d*%d",bufferSelectedObj->getWidth(),bufferSelectedObj->getHeight(),bufferSelectedObj->getDepth());
				textOutput(x,y+=18,h,"format: %s",(bufferSelectedObj->getFormat()==rr::BF_RGB)?"RGB":((bufferSelectedObj->getFormat()==rr::BF_RGBA)?"RGBA":((bufferSelectedObj->getFormat()==rr::BF_RGBF)?"RGBF":((bufferSelectedObj->getFormat()==rr::BF_RGBAF)?"RGBAF":"INVALID!"))));
				textOutput(x,y+=18,h,"scale: %s",bufferSelectedObj->getScaled()?"custom(usually sRGB)":"physical(linear)");
				glColor3f(1,1,1);
			}
		}
		if (multiMesh && (!svs.render2d || !lv))
		{
			// ray and collisionHandler are used in this block
			rr::RRVec3 dir = svs.eye.dir.RRVec3::normalized();
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
				rr::RRMesh::PreImportNumber preTriangle = multiMesh->getPreImportTriangle(ray->hitTriangle);
				const rr::RRMaterial* material = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
				rr::RRMaterial pointMaterial;
				if (material && material->sideBits[ray->hitFrontSide?0:1].pointDetails)
				{
					multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
					material = &pointMaterial;
				}
				rr::RRMesh::TriangleMapping triangleMapping;
				multiMesh->getTriangleMapping(ray->hitTriangle,triangleMapping);
				rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray->hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray->hitPoint2d[1];
				rr::RRMesh::TriangleNormals triangleNormals;
				multiMesh->getTriangleNormals(ray->hitTriangle,triangleNormals);
				rr::RRVec3 normal = triangleNormals.vertex[0].normal + (triangleNormals.vertex[1].normal-triangleNormals.vertex[0].normal)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].normal-triangleNormals.vertex[0].normal)*ray->hitPoint2d[1];
				rr::RRVec3 tangent = triangleNormals.vertex[0].tangent + (triangleNormals.vertex[1].tangent-triangleNormals.vertex[0].tangent)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].tangent-triangleNormals.vertex[0].tangent)*ray->hitPoint2d[1];
				rr::RRVec3 bitangent = triangleNormals.vertex[0].bitangent + (triangleNormals.vertex[1].bitangent-triangleNormals.vertex[0].bitangent)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].bitangent-triangleNormals.vertex[0].bitangent)*ray->hitPoint2d[1];
				textOutput(x,y+=18*2,h,"[point in the middle of viewport]");
				textOutput(x,y+=18,h,"object: %d/%d",preTriangle.object,numObjects);
				textOutput(x,y+=18,h,"object lit: %s",svs.renderRealtime?"reatime GI":(solver->getIllumination(preTriangle.object)->getLayer(svs.staticLayerNumber)?(solver->getIllumination(preTriangle.object)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE?"static lightmap":"static per-vertex"):"not"));
				textOutput(x,y+=18,h,"triangle in object: %d/%d",preTriangle.index,solver->getObject(preTriangle.object)->getCollider()->getMesh()->getNumTriangles());
				textOutput(x,y+=18,h,"triangle in scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
				textOutput(x,y+=18,h,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
				textOutput(x,y+=18,h,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
				rr::RRBuffer* bufferCenter = solver->getIllumination(preTriangle.object) ? solver->getIllumination(preTriangle.object)->getLayer(svs.staticLayerNumber) : NULL;
				if (bufferCenter)
				{
					int i = int(uvInLightmap[0]*bufferCenter->getWidth());
					int j = int(uvInLightmap[1]*bufferCenter->getHeight());
					textOutput(x,y+=18,h,"ij in lightmap: %d %d",i,j);
					if (i>=0 && i<(int)bufferCenter->getWidth() && j>=0 && j<(int)bufferCenter->getHeight())
					{
						centerObject = preTriangle.object;
						centerTexel = i + j*bufferCenter->getWidth();
						centerTriangle = ray->hitTriangle;
					}
				}
				textOutput(x,y+=18,h,"distance: %f",ray->hitDistance);
				textOutput(x,y+=18,h,"pos: %f %f %f",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
				textOutput(x,y+=18,h,"plane:  %f %f %f %f",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2],ray->hitPlane[3]);
				textOutput(x,y+=18,h,"normal: %f %f %f",normal[0],normal[1],normal[2]);
				textOutput(x,y+=18,h,"tangent: %f %f %f",tangent[0],tangent[1],tangent[2]);
				textOutput(x,y+=18,h,"bitangent: %f %f %f",bitangent[0],bitangent[1],bitangent[2]);
				textOutput(x,y+=18,h,"side: %s",ray->hitFrontSide?"front":"back");
				textOutput(x,y+=18,h,"material: %s",material?((material!=&pointMaterial)?"per-triangle":"per-pixel"):"NULL!!!");
				if (material)
				{
					if (material->name)
						textOutput(x,y+=18,h,"name: %s",material->name);
					textOutput(x,y+=18,h,"diffuse refl: %f %f %f",material->diffuseReflectance.color[0],material->diffuseReflectance.color[1],material->diffuseReflectance.color[2]);
					textOutput(x,y+=18,h,"specular refl: %f",material->specularReflectance);
					textOutput(x,y+=18,h,"transmittance: %f %f %f",material->specularTransmittance.color[0],material->specularTransmittance.color[1],material->specularTransmittance.color[2]);
					textOutput(x,y+=18,h,"refraction index: %f",material->refractionIndex);
					textOutput(x,y+=18,h,"dif.emittance: %f %f %f",material->diffuseEmittance.color[0],material->diffuseEmittance.color[1],material->diffuseEmittance.color[2]);
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
		if (multiMesh && svs.render2d && lv)
		{
			rr::RRVec2 uv = lv->getCenterUv(GetSize());
			textOutput(x,y+=18*2,h,"[point in the middle of viewport]");
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
		if ((!svs.render2d || !lv) && SVRayLog::size)
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
	}


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
