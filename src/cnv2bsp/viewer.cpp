#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")

#include "cnv2bsp.h"

#define M_PI 3.14159265358979323846
#define PI M_PI

#define MAX_KEYS 16

#define MOUSE_LEFT   256
#define MOUSE_RIGHT  257
#define MOUSE_MIDDLE 258
#define MOUSE_X      259
#define MOUSE_Y      260

#define KEY_TAB   9
#define KEY_LEFT  37
#define KEY_UP    38
#define KEY_RIGHT 39
#define KEY_DOWN  40
#define KEY_SPACE 32
#define KEY_ENTER 13
#define KEY_PGUP  33
#define KEY_PGDN  34

static int BPP=32;
static int ZBPP=24;
static int XRES=800;
static int YRES=600;
static int FULLSCREEN=0;

int console_XRES;
int console_YRES;
int console_Refresh;

int console_Fullscreen=1;
int console_Active=1;
int console_Key[261];

__int64 clock_freq;
__int64 clock_Start;
__int64 clock_Counter;
 double clock_Freq;
 double clock_Time;

HDC       console_hDC=NULL;
HGLRC     console_hRC=NULL;
HWND      console_hWnd=NULL;
HINSTANCE console_hInstance;

#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN 4
#endif

#define ERR(A) MessageBox(NULL,A,"Error",MB_OK | MB_ICONEXCLAMATION);

LRESULT CALLBACK WndProc(HWND console_hWnd, UINT uMsg, WPARAM  wParam, LPARAM lParam)
{
 switch (uMsg) {
        case WM_ACTIVATE:    console_Active=!HIWORD(wParam);
                             console_Refresh=1; return 0;
        case WM_SYSCOMMAND:  switch (wParam) {
                                    case SC_SCREENSAVE:
                                    case SC_MONITORPOWER: return 0;
                                    } break;
        case WM_CLOSE:       PostQuitMessage(0); return 0;
        case WM_KEYDOWN:     console_Key[wParam]=1; return 0;
        case WM_KEYUP:       console_Key[wParam]=0; return 0;
        case WM_MOUSEMOVE:   console_Key[MOUSE_X]=LOWORD(lParam);
                             console_Key[MOUSE_Y]=HIWORD(lParam); break;
        case WM_LBUTTONDOWN: console_Key[MOUSE_LEFT]=1; break;
        case WM_LBUTTONUP:   console_Key[MOUSE_LEFT]=0; break;
        case WM_RBUTTONDOWN: console_Key[MOUSE_RIGHT]=1; break;
        case WM_RBUTTONUP:   console_Key[MOUSE_RIGHT]=0; break;
        case WM_MBUTTONDOWN: console_Key[MOUSE_MIDDLE]=1; break;
        case WM_MBUTTONUP:   console_Key[MOUSE_MIDDLE]=0; break;
        case WM_SIZE:        console_XRES=LOWORD(lParam);
                             console_YRES=HIWORD(lParam);
                             glViewport(0,0,console_XRES,console_YRES);
                             console_Refresh=1;
                             return 0;
        }

 return DefWindowProc(console_hWnd,uMsg,wParam,lParam);
}

void console_Done()
{
 if (console_Fullscreen) { ChangeDisplaySettings(NULL,0); ShowCursor(TRUE); }

 if (console_hRC) {
    if (!wglMakeCurrent(NULL,NULL)) ERR("Release of DC and RC failed!");
    if (!wglDeleteContext(console_hRC)) { ERR("Release rendering context failed!");
       console_hRC=NULL; }
    }

 if (console_hDC && !ReleaseDC(console_hWnd,console_hDC))
    { ERR("Release device context failed!"); console_hDC=NULL; }

 if (console_hWnd && !DestroyWindow(console_hWnd))
    { ERR("Could not release hWnd!"); console_hWnd=NULL; }

 if (!UnregisterClass("OpenGL",console_hInstance))
    { ERR("Could not unregister class!"); console_hInstance=NULL; }
}

int console_Init(char *title, int width, int height, int bits, int ff)
{
 DWORD dwExStyle,dwStyle;
 GLuint PixelFormat;
 RECT WindowRect;
 WNDCLASS wc;

 WindowRect.left=0; WindowRect.right=width;
 WindowRect.top=0; WindowRect.bottom=height;

 console_Fullscreen=ff; console_hInstance=GetModuleHandle(NULL);

 wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
 wc.lpfnWndProc   = (WNDPROC) WndProc;
 wc.cbClsExtra    = 0;
 wc.cbWndExtra    = 0;
 wc.hInstance     = console_hInstance;
 //wc.hIcon         = LoadIcon(console_hInstance, MAKEINTRESOURCE(ID_ICON));
 wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
 wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
 wc.hbrBackground = NULL;
 wc.lpszMenuName  = NULL;
 wc.lpszClassName = "OpenGL";

 if (!RegisterClass(&wc)) {
    ERR("Failed to register the window class!");
    return 0;
    }

 if (console_Fullscreen) {

    DEVMODE dmScreenSettings;

    memset(&dmScreenSettings,0,sizeof(dmScreenSettings));

    dmScreenSettings.dmFields     = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
    dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth  = width;
    dmScreenSettings.dmPelsHeight = height;
    dmScreenSettings.dmBitsPerPel = bits;

    if (ChangeDisplaySettings(&dmScreenSettings,
        CDS_FULLSCREEN)!=DISP_CHANGE_SUCCESSFUL)
       if (MessageBox(NULL,
          "Unable to set fullscreen mode.\n"
          "Use windowed mode instead?","Fullscreen",
          MB_YESNO|MB_ICONEXCLAMATION)==IDYES)
          console_Fullscreen=0; else return 0;

    dwExStyle=WS_EX_APPWINDOW;
    dwStyle=WS_POPUP;
    ShowCursor(FALSE);

    } else {

    dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    dwStyle=WS_OVERLAPPEDWINDOW;

    }

 AdjustWindowRectEx(&WindowRect,dwStyle,FALSE,dwExStyle);

 if (!(console_hWnd=CreateWindowEx(dwExStyle,"OpenGL",
    title, dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    CW_USEDEFAULT,CW_USEDEFAULT,WindowRect.right-WindowRect.left,
    WindowRect.bottom-WindowRect.top,NULL,NULL,console_hInstance,NULL)))
    { console_Done(); ERR("Window creation error!"); return 0; }

 static PIXELFORMATDESCRIPTOR pfd={ sizeof(PIXELFORMATDESCRIPTOR),1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,bits,0,0,0,0,0,0,0,0,0,0,0,0,0,
        ZBPP,0,0,PFD_MAIN_PLANE,0,0,0,0 };

 if (!(console_hDC=GetDC(console_hWnd))) { console_Done();
    ERR("Unable to create a GL device context!"); return 0; }

 if (!(PixelFormat=ChoosePixelFormat(console_hDC,&pfd))) { console_Done();
    ERR("Unable to find a suitable pixelformat!"); return 0; }

 if(!SetPixelFormat(console_hDC,PixelFormat,&pfd)) { console_Done();
    ERR("Unable to set the pixelformat!"); return 0; }

 if (!(console_hRC=wglCreateContext(console_hDC))) { console_Done();
    ERR("Unable to create a GL rendering context!"); return 0; }

 if (!wglMakeCurrent(console_hDC,console_hRC)) { console_Done();
    ERR("Unable to activate the GL rendering context!"); return 0; }

 ShowWindow(console_hWnd,SW_SHOW);
 SetForegroundWindow(console_hWnd);
 SetFocus(console_hWnd);

 glViewport(0,0,width,height);

 console_XRES=width;
 console_YRES=height;
 console_Refresh=1;
 
 glClearColor(0,0,0,0);

 glClear(GL_COLOR_BUFFER_BIT);
 glClear(GL_DEPTH_BUFFER_BIT);

 glFlush();
 glFinish();

 SwapBuffers(console_hDC);

 return 1;
}

void clock_Run()
{
 QueryPerformanceFrequency((LARGE_INTEGER*)&clock_freq);
 QueryPerformanceCounter((LARGE_INTEGER*)&clock_Start);

 clock_Freq=clock_freq;
}

double clock_Get()
{
 QueryPerformanceCounter((LARGE_INTEGER*)&clock_Counter);
 clock_Time=clock_Counter-clock_Start; clock_Time/=clock_Freq;
 return clock_Time;
}

void console_Blit()
{
 glFlush(); glFinish(); SwapBuffers(console_hDC);
}

int bsp_depth=0;
char bsp_traverse[1024];
GLfloat bsp_col[4]={1,1,1,1};
GLfloat bsp_white[4]={1,1,1,1};

void draw_bsp(BSP_TREE *t, int depth)
{
 int i;

 if (depth==bsp_depth) glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,bsp_white);

 if (depth>=bsp_depth) for (i=0;t->plane[i];i++) {

    VERTEX **v=t->plane[i]->vertex;
    NORMAL *n=&t->plane[i]->normal;

    glBegin(GL_TRIANGLES);

    glNormal3f(n->a,n->b,n->c);

    glVertex3f(v[0]->x,v[0]->y,v[0]->z);
    glVertex3f(v[1]->x,v[1]->y,v[1]->z);
    glVertex3f(v[2]->x,v[2]->y,v[2]->z);

    glEnd();

    }

 if (depth==bsp_depth) {
    bsp_col[0]=.5; bsp_col[1]=1; bsp_col[2]=.2;
    glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,bsp_col);
    }

 if (depth<bsp_depth) { if (bsp_traverse[depth]=='f')
    if (t->front) draw_bsp(t->front,depth+1); } else
    if (t->front) draw_bsp(t->front,depth+1);

 if (depth==bsp_depth) {
    bsp_col[0]=.5; bsp_col[1]=.2; bsp_col[2]=1;
    glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,bsp_col);
    }

 if (depth<bsp_depth) { if (bsp_traverse[depth]=='b')
    if (t->back) draw_bsp(t->back,depth+1); } else
    if (t->back) draw_bsp(t->back,depth+1);
}

void draw_Scene(WORLD *w, float px, float py, float pz)
{
 int i,j; GLfloat pos[4],col[4]={1,1,1,1};
 
 glShadeModel(GL_FLAT);

 pos[0]=px; pos[1]=py; pos[2]=pz;

 glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
 glLightfv(GL_LIGHT0,GL_POSITION,pos);
 glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,col);
/*
 for (j=0;j<w->object_num;j++) { OBJECT *o=&w->object[j];

     for (i=0;i<o->face_num;i++) { o->face[i].id=i;

         VERTEX **v=o->face[i].vertex;
         NORMAL *n=&o->face[i].normal;

         glBegin(GL_TRIANGLES);

         glNormal3f(n->a,n->b,n->c);

         glVertex3f(v[0]->x,v[0]->y,v[0]->z);
         glVertex3f(v[1]->x,v[1]->y,v[1]->z);
         glVertex3f(v[2]->x,v[2]->y,v[2]->z);

         glEnd();
         
         }
     }
*/
 for (j=0;j<w->object_num;j++) draw_bsp(w->object[j].bsp,0);
}

void run_Scene(WORLD *w, float rho, float alpha, float aspect, float fov)
{
 float px,py,pz;

 glEnable(GL_DEPTH_TEST);
 glEnable(GL_CULL_FACE);

 glClear(GL_COLOR_BUFFER_BIT);
 glClear(GL_DEPTH_BUFFER_BIT);

 glMatrixMode(GL_PROJECTION); glLoadIdentity();
 gluPerspective(fov,aspect,0.01,2*SCALE);
 
 px=.5*SCALE*cos(rho);
 py=.5*SCALE*sin(alpha);
 pz=.5*SCALE*sin(rho);

 glMatrixMode(GL_MODELVIEW); glLoadIdentity();
 gluLookAt(px,py,pz,0,0,0,0,1,0);
 
 draw_Scene(w,px,py,pz);
}

int APIENTRY WinMain(HINSTANCE hCurrentInst,
                     HINSTANCE hPreviousInst,
                     LPSTR lpszCmdLine, int nCmdShow)
{
 int argc=0,speed=500,progress=1,view=0,root=BIG,dir=0,one=0,max=1,bestN=0,i;
 char *argv[256],s[256],*sp=s; WORLD *w; MSG msg;
 float rho=0,alpha=0,fov=55;

 strcpy(s,lpszCmdLine); argv[++argc]=sp;

 printf("\n -=[ BSP generator v2.01 ]=- by ReDox/MovSD (C) 2004\n\n");

 while (*sp) if (*sp==' ') { while (*sp==' ') *sp++=0;
    if (*sp) argv[++argc]=sp; else break; } else sp++;

 if (argc<2) {
    printf(" usage: cnv2bsp <*.[3ds|mgf]> <*.bsp> [dir|one|view|big|mean|best|{max}] \n\n"
           " dir -> create sub-directory for each object to store lightmaps\n"
           " one -> produce one big BSP tree by taking all objects together\n"
           " mean -> BSP root is the triangle near the middle of space O(N)\n"
           " big -> BSP root is the triangle with the biggest area O(N)\n"
           " best -> BSP root is the best triangle in the space O(N^2)\n"
           " bestN -> BSP root is the best from N most promising triangles O(NlogN)\n"
           " view -> show final BSP tree ([F1]=front, [F2]=back, [ESC]=quit,\n"
           "         [BACKSPACE]=go back, [ARROWS]=rotate, [+/-]=field-of-view)\n"
           " {max} -> maximal number of faces stored in leaves of BSP tree\n\n"
           " example: cnv2bsp big.mgf big.bsp one best view 16\n"); return 0; }

 for (i=3;i<=argc;i++) {
     if (!strcmp(argv[i],"dir")) dir=1;
     if (!strcmp(argv[i],"one")) one=1;
     if (!strcmp(argv[i],"view")) view=1;
     if (!strcmp(argv[i],"big")) root=BIG;
     if (!strcmp(argv[i],"mean")) root=MEAN;
     if (!strcmp(argv[i],"best")) root=BEST;
     if (!strncmp(argv[i],"best",4)) { sscanf(argv[i],"best%d",&bestN); if (bestN) root=BESTN; }
     if ('0'<=argv[i][0] && argv[i][0]<='9') max=atoi(argv[i]);
     }

 w=load_Scene(argv[1],argv[2],dir,one,max,root,bestN); if (!view) return 0;

 if (!console_Init("BSP",XRES,YRES,BPP,FULLSCREEN)) return 0; clock_Run();

 while (progress) {

       while (progress &&
             !PeekMessage(&msg,console_hWnd,0,0,PM_NOREMOVE)) {

             if (console_Key[VK_F1]) { bsp_traverse[bsp_depth++]='f'; Sleep(200); }
             if (console_Key[VK_F2]) { bsp_traverse[bsp_depth++]='b'; Sleep(200); }
             if (console_Key[VK_BACK]) { if (--bsp_depth<0) bsp_depth=0; Sleep(200); }

             if (console_Key[VK_UP]) alpha-=PI/360;
             if (console_Key[VK_DOWN]) alpha+=PI/360;

             if (console_Key[VK_LEFT]) rho+=PI/360;
             if (console_Key[VK_RIGHT]) rho-=PI/360;

             if (console_Key[VK_ADD]) fov++;
             if (console_Key[VK_SUBTRACT]) fov--;
             if (console_Key[VK_ESCAPE]) progress=0;

             if (speed<0) speed=0;

             if (progress) {

                float aspect=(float)XRES/YRES;

                run_Scene(w,rho,alpha,aspect,fov);

                console_Blit();
                console_Refresh=0;

                }
             }

       if (!progress) SendMessage(console_hWnd,WM_CLOSE,0,0);
       if (GetMessage(&msg,console_hWnd,0,0)!=TRUE) break;

       TranslateMessage(&msg);
       DispatchMessage(&msg);

       }

 console_Done();

 return msg.wParam;
}