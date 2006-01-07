#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <GL/glut.h>
#include <GL/glprocs.h>
#include <GL/tube.h>  /* the OpenGL Tubing and Extrusion library by
                         Linas Vepstas */

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "mgf2rr.h"
#include "rr2gl.h"
#include "tga.h"      /* TGA image file loader routines */
#include "matrix.h"   /* OpenGL-style 4x4 matrix manipulation routines */
#include "rc_debug.h" /* register combiners debugging routines */

#define MAX_SIZE 1024

/* Texture objects. */
enum {
  TO_RESERVED = 0,             /* zero is not a valid OpenGL texture object ID */
  TO_MAP_8BIT,                 /* 1D identity texture, s := [0,255] range */
  TO_MAP_16BIT,                /* 2D identity texture, s*256+t := [0,65535] range */
  TO_DEPTH_MAP,                /* high resolution depth map for shadow mapping */
  TO_LOWRES_DEPTH_MAP,         /* low resolution depth map for shadow mapping */
  TO_DEPTH_MAP_RECT,           /* high resolution depth map for shadow mapping */
  TO_LOWRES_DEPTH_MAP_RECT,    /* low resolution depth map for shadow mapping */
  TO_HW_DEPTH_MAP,             /* high resolution hardware depth map for shadow
                                  mapping */
  TO_LOWRES_HW_DEPTH_MAP,      /* low resolution hardware depth map for shadow
                                  mapping */
  TO_HW_DEPTH_MAP_RECT,        /* high resolution hardware rectangular depth map
                                  for shadow mapping */
  TO_LOWRES_HW_DEPTH_MAP_RECT, /* low resolution hardware rectangular depth map
                                  for shadow mapping */
  TO_SPOT,
  TO_SOFT_DEPTH_MAP            /* TO_SOFT_DEPTH_MAP+n = depth map for n-th light */
};

/* Display lists. */
enum {
  DL_RESERVED = 0, /* zero is not a valid OpenGL display list ID */
  DL_SPHERE,       /* sphere display list */
};

/* Draw modes. */
enum {
  DM_EYE_VIEW,
  DM_LIGHT_VIEW,
  DM_DEPTH_MAP,
  DM_EYE_VIEW_DEPTH_TEXTURED,
  DM_EYE_VIEW_SHADOWED,
  DM_EYE_VIEW_SOFTSHADOWED,
};

/* Object configurations. */
enum {
  OC_SPHERE_GRID,
  OC_NVIDIA_LOGO,
  OC_WEIRD_HELIX,
  OC_COMBO,
  OC_SIMPLE,
  OC_BLUE_PONY,
  OC_MGF,
  NUM_OF_OCS,
};

/* Menu items. */
enum {
  ME_EYE_VIEW,
  ME_EYE_VIEW_SHADOWED,
  ME_EYE_VIEW_SOFTSHADOWED,
  ME_LIGHT_VIEW,
  ME_DEPTH_MAP,
  ME_EYE_VIEW_TEXTURE_DEPTH_MAP,
  ME_EYE_VIEW_TEXTURE_LIGHT_Z,
  ME_TOGGLE_WIRE_FRAME,
  ME_TOGGLE_LIGHT_FRUSTUM,
  ME_TOGGLE_HW_SHADOW_MAPPING,
  ME_TOGGLE_DEPTH_MAP_CULLING,
  ME_TOGGLE_HW_SHADOW_FILTERING,
  ME_TOGGLE_HW_SHADOW_COPY_TEX_IMAGE,
  ME_TOGGLE_QUICK_LIGHT_MOVE,
  ME_SWITCH_MOUSE_CONTROL,
  ME_ON_LIGHT_FRUSTUM,
  ME_OFF_LIGHT_FRUSTUM,
  ME_DEPTH_MAP_64,
  ME_DEPTH_MAP_128,
  ME_DEPTH_MAP_256,
  ME_DEPTH_MAP_512,
  ME_DEPTH_MAP_1024,
  ME_PRECISION_DT_8BIT,
  ME_PRECISION_DT_16BIT,
  ME_PRECISION_HW_16BIT,
  ME_PRECISION_HW_24BIT,
  ME_EXIT,
};

int hwLittleEndian;
int littleEndian;

int vsync = 1;
int requestedDepthMapSize = 512;
int depthMapSize = 512;

int requestedDepthMapRectWidth = 350;
int requestedDepthMapRectHeight = 300;
int depthMapRectWidth = 350;
int depthMapRectHeight = 300;

GLenum depthMapPrecision = GL_UNSIGNED_BYTE;
GLenum depthMapFormat = GL_LUMINANCE;
GLenum depthMapInternalFormat = GL_INTENSITY8;

GLenum hwDepthMapPrecision = GL_UNSIGNED_SHORT;
GLenum hwDepthMapInternalFormat = GL_DEPTH_COMPONENT16_SGIX;
GLenum hwDepthMapFiltering = GL_LINEAR;

int useCopyTexImage = 1;
int useTextureRectangle = 0;
int mipmapShadowMap = 0;
int useAccum = 0;
int useLights = 1;
int useScissor = 1;
float globalIntensity = 1;
float ambientPower = 0;
int softLight = -1;
int softWidth[200],softHeight[200],softPrecision[200],softFiltering[200];
int areaType = 0; // 0=linear, 1=square grid, 2=circle
bool drawOnlyZ = false;
bool drawIndexed = false;
GLfloat eye_shift[3]={0,0,0};
GLfloat ed[3];
char *mgf_filename="data\\scene8.mgf";

int depthBias8 = 2;
int depthBias16 = 6;
int depthBias24 = 8;
int depthScale8, depthScale16, depthScale24;
GLfloat slopeScale = 1.8;

GLfloat textureLodBias = 0.0;

#define LIGHT_DIMMING 0.0

GLfloat zero[] = {0.0, 0.0, 0.0, 1.0};
GLfloat lightColor[] = {10.0, 10.0, 10.0, 1.0};
GLfloat lightDimColor[] = {LIGHT_DIMMING, LIGHT_DIMMING, LIGHT_DIMMING, 1.0};
GLfloat grayMaterial[] = {0.7, 0.7, 0.7, 1.0};
GLfloat redMaterial[] = {0.7, 0.1, 0.2, 1.0};
GLfloat greenMaterial[] = {0.1, 0.7, 0.4, 1.0};
GLfloat brightGreenMaterial[] = {0.1, 0.9, 0.1, 1.0};
GLfloat blueMaterial[] = {0.1, 0.2, 0.7, 1.0};
GLfloat whiteMaterial[] = {1.0, 1.0, 1.0, 1.0};

GLfloat lv[4];  /* default light position */

void *font = GLUT_BITMAP_8_BY_13;

GLUquadricObj *q;
float eyeAngle = 1.0;
float eyeHeight = 10.0;
int xEyeBegin, yEyeBegin, movingEye = 0;
float lightAngle = 0.85;
float lightHeight = 8.0;
int xLightBegin, yLightBegin, movingLight = 0;
int quickLightMove = 0;
int wireFrame = 0;

/* By default, disable back face culling during depth map construction.
   See the comment in the code. */
int depthMapBackFaceCulling = 0;

int winWidth, winHeight;
int needMatrixUpdate = 1;
int needTitleUpdate = 1;
int needDepthMapUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
int objectConfiguration = OC_MGF;
int rangeMap = 0;
int showHelp = 0;
int fullscreen = 0;
int forceEnvCombine = 0;
int useDisplayLists = 1;
int textureSpot = 0;
int showLightViewFrustum = 1;
int showDepthMapMSBs = 1;
int eyeButton = GLUT_LEFT_BUTTON;
int lightButton = GLUT_MIDDLE_BUTTON;
int useStencil = 0;
int useDepth24 = 1;
int drawFront = 0;
int rcDebug = 0;

int hasRegisterCombiners = 0;
int hasTextureEnvCombine = 0;
int hasShadow = 0;
int hasSwapControl = 0;
int hasDepthTexture = 0;
int hasShadowMapSupport = 0;
int useShadowMapSupport = 0;
int hasSeparateSpecularColor = 0;
int hasTextureRectangle = 0;
int hasTextureBorderClamp = 0;
int hasTextureEdgeClamp = 0;
int hasOpenGL12 = 0;

int maxRectangleTextureSize = 0;

double winAspectRatio;

GLdouble eyeFieldOfView = 40.0;
GLdouble eyeNear = 0.3;
GLdouble eyeFar = 60.0;
GLdouble lightFieldOfView = 50.0;
GLdouble lightNear = 2.0;
GLdouble lightFar = 24.0;

GLdouble eyeViewMatrix[16];
GLdouble eyeFrustumMatrix[16];
GLdouble lightViewMatrix[16];
GLdouble lightInverseViewMatrix[16];
GLdouble lightFrustumMatrix[16];
GLdouble lightInverseFrustumMatrix[16];

/* printGLmatrix - handy debugging routine */
static void
printGLmatrix(GLenum matrix, char *text)
{
  GLdouble m[16]; 

  glGetDoublev(matrix, m);
  printMatrix(text, m);

}

/*** OPENGL INITIALIZATION AND CHECKS ***/

/* supportsOneDotTwo - returns true if supports OpenGL 1.2 or higher. */
static int
supportsOneDotTwo(void)
{
  const char *version;
  int major, minor;

  version = (char *) glGetString(GL_VERSION);
  if (sscanf(version, "%d.%d", &major, &minor) == 2) {
    return major > 1 || minor >= 2;
  }
  return 0;            /* OpenGL version string malformed! */
}

#ifdef _WIN32
# define GET_PROC_ADDRESS(x) wglGetProcAddress(x)
#else
# ifdef macintosh
#  ifndef GL_MAC_GET_PROC_ADDRESS_NV
#   define GL_MAC_GET_PROC_ADDRESS_NV 0x84FC
#  endif
#  define GET_PROC_ADDRESS(x) macGetProcAddress(x)
void *(*macGetProcAddress)(char *string);
# else
#  define GET_PROC_ADDRESS(x) glXGetProcAddress(x)
# endif
#endif

/* initExtensions - Check if required extensions exist. */
void
initExtensions(void)
{
  int missingRequiredExtension = 0;  /* Start optimistic. */

  hasOpenGL12 = supportsOneDotTwo();

  if (!glutExtensionSupported("GL_ARB_multitexture")) {
    printf("I require the GL_ARB_multitexture OpenGL extension to work.\n");
    missingRequiredExtension = 1;
  }
  if (!glutExtensionSupported("GL_EXT_bgra")) {
    printf("I require the GL_EXT_bgra OpenGL extension to work.\n");
    missingRequiredExtension = 1;
  }
  if (glutExtensionSupported("GL_NV_register_combiners")) {
    hasRegisterCombiners = 1;
  }
  if (glutExtensionSupported("GL_EXT_texture_env_combine")) {
    hasTextureEnvCombine = 1;
  }
  if (glutExtensionSupported("GL_SGIX_shadow")) {
    hasShadow = 1;
  }
#ifdef _WIN32
  if (glutExtensionSupported("WGL_EXT_swap_control")) {
    hasSwapControl = 1;
  }
#endif
  if (glutExtensionSupported("GL_SGIX_depth_texture")) {
    hasDepthTexture = 1;
  }
  if (glutExtensionSupported("GL_ARB_texture_border_clamp")) {
    hasTextureBorderClamp = 1;
  }
  if (hasOpenGL12 || glutExtensionSupported("GL_EXT_texture_edge_clamp")) {
    hasTextureEdgeClamp = 1;
  }
  if (hasOpenGL12 || glutExtensionSupported("GL_EXT_separate_specular_color")) {
    hasSeparateSpecularColor = 1;
  }
  if (glutExtensionSupported("GL_NV_texture_rectangle")) {
    hasTextureRectangle = 1;
    glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &maxRectangleTextureSize);
  }

  if (forceEnvCombine) {
    printf("Forcing use of GL_EXT_texture_env_combine.\n");
    hasRegisterCombiners = 0;
    if (!hasTextureEnvCombine) {
      printf("I require the following OpenGL extension to work:\n"
             "  GL_EXT_texture_env_combine\n");
      missingRequiredExtension = 1;
    }
  } else {
    if (!hasRegisterCombiners && !hasTextureEnvCombine) {
      printf("I require one of the following OpenGL extensions to work:\n"
             "  GL_EXT_texture_env_combine\n"
             "  GL_NV_register_combiners\n");
      missingRequiredExtension = 1;
    }
  }
  if (missingRequiredExtension) {
    printf("\nshadowcast: "
           "Exiting because required OpenGL extensions are not supported.\n");
    exit(1);
  }
  if (hasShadow && hasDepthTexture &&
      hasRegisterCombiners && hasSeparateSpecularColor) {
    printf("shadowcast: has hardware shadow mapping support.\n");
    hasShadowMapSupport = 1;
    useShadowMapSupport = 1;
  }

#if defined(NEEDS_GET_PROC_ADDRESS)

# if defined(macintosh)
  glGetPointerv(GL_MAC_GET_PROC_ADDRESS_NV, (GLvoid*) &macGetProcAddress);
# endif

# if !defined(macintosh) 
  /* Retrieve some ARB_multitexture routines. */
  glMultiTexCoord2iARB =
    (PFNGLMULTITEXCOORD2IARBPROC)
    GET_PROC_ADDRESS("glMultiTexCoord2iARB");
  glMultiTexCoord2fvARB =
    (PFNGLMULTITEXCOORD2FVARBPROC)
    GET_PROC_ADDRESS("glMultiTexCoord2fvARB");
  glMultiTexCoord3fARB =
    (PFNGLMULTITEXCOORD3FARBPROC)
    GET_PROC_ADDRESS("glMultiTexCoord3fARB");
  glMultiTexCoord3fvARB =
    (PFNGLMULTITEXCOORD3FVARBPROC)
    GET_PROC_ADDRESS("glMultiTexCoord3fvARB");
  glActiveTextureARB =
    (PFNGLACTIVETEXTUREARBPROC)
    GET_PROC_ADDRESS("glActiveTextureARB");
# endif

  if (hasRegisterCombiners) {
    /* Retrieve all NV_register_combiners routines. */
    glCombinerParameterfvNV =
      (PFNGLCOMBINERPARAMETERFVNVPROC)
      GET_PROC_ADDRESS("glCombinerParameterfvNV");
    glCombinerParameterivNV =
      (PFNGLCOMBINERPARAMETERIVNVPROC)
      GET_PROC_ADDRESS("glCombinerParameterivNV");
    glCombinerParameterfNV =
      (PFNGLCOMBINERPARAMETERFNVPROC)
      GET_PROC_ADDRESS("glCombinerParameterfNV");
    glCombinerParameteriNV =
      (PFNGLCOMBINERPARAMETERINVPROC)
      GET_PROC_ADDRESS("glCombinerParameteriNV");
    glCombinerInputNV =
      (PFNGLCOMBINERINPUTNVPROC)
      GET_PROC_ADDRESS("glCombinerInputNV");
    glCombinerOutputNV =
      (PFNGLCOMBINEROUTPUTNVPROC)
      GET_PROC_ADDRESS("glCombinerOutputNV");
    glFinalCombinerInputNV =
      (PFNGLFINALCOMBINERINPUTNVPROC)
      GET_PROC_ADDRESS("glFinalCombinerInputNV");
    glGetCombinerInputParameterfvNV =
      (PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC)
      GET_PROC_ADDRESS("glGetCombinerInputParameterfvNV");
    glGetCombinerInputParameterivNV =
      (PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC)
      GET_PROC_ADDRESS("glGetCombinerInputParameterivNV");
    glGetCombinerOutputParameterfvNV =
      (PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC)
      GET_PROC_ADDRESS("glGetCombinerOutputParameterfvNV");
    glGetCombinerOutputParameterivNV =
      (PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC)
      GET_PROC_ADDRESS("glGetCombinerOutputParameterivNV");
    glGetFinalCombinerInputParameterfvNV =
      (PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC)
      GET_PROC_ADDRESS("glGetFinalCombinerInputParameterfvNV");
    glGetFinalCombinerInputParameterivNV =
      (PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC)
      GET_PROC_ADDRESS("glGetFinalCombinerInputParameterivNV");
  }

# ifdef _WIN32
  if (hasSwapControl) {
    /* Retrieve all WGL_EXT_swap_control routines. */
    wglSwapIntervalEXT = 
      (PFNWGLSWAPINTERVALEXTPROC)
      GET_PROC_ADDRESS("wglSwapIntervalEXT");
    wglGetSwapIntervalEXT = 
      (PFNWGLGETSWAPINTERVALEXTPROC)
      GET_PROC_ADDRESS("wglGetSwapIntervalEXT");
  }
# endif  /* _WIN32 */

#endif  /* NEEDS_GET_PROC_ADDRESS */
}

/* updateTitle - update the window's title bar text based on the current
   program state. */
void
updateTitle(void)
{
  char title[256];
  char *modeName;
  char *depthTextureType;
  int width, height, precision;
  int depthBias;

  /* Only update the title is needTitleUpdate is set. */
  if (needTitleUpdate) {
    switch (drawMode) {
    case DM_EYE_VIEW:
      modeName = "eye view without shadows";
      break;
    case DM_LIGHT_VIEW:
      modeName = "light view";
    case DM_DEPTH_MAP:
      if ((depthMapPrecision == GL_UNSIGNED_SHORT) && !useShadowMapSupport) {
        if (showDepthMapMSBs) {
          modeName = "view depth map MSBs";
        } else {
          modeName = "view depth map LSBs";
        }
      } else {
        modeName = "view depth map";
      }
      break;
    case DM_EYE_VIEW_DEPTH_TEXTURED:
      if (rangeMap) {
        if ((depthMapPrecision == GL_UNSIGNED_SHORT) && !useShadowMapSupport) {
          if (showDepthMapMSBs) {
            modeName = "light Z range textured eye view MSBs";
          } else {
            modeName = "light Z range textured eye view LSBs";
          }
        } else {
          modeName = "light Z range textured eye view";
        }
      } else {
        if ((depthMapPrecision == GL_UNSIGNED_SHORT) && !useShadowMapSupport) {
          if (showDepthMapMSBs) {
            modeName = "projected depth map eye view MSBs";
          } else {
            modeName = "projected depth map eye view LSBs";
          }
        } else {
          modeName = "projected depth map eye view";
        }
      }
      break;
    case DM_EYE_VIEW_SHADOWED:
      modeName = "eye view with shadows";
      break;
    case DM_EYE_VIEW_SOFTSHADOWED:
      modeName = "eye view with soft shadows";
      break;
    default:
      assert(0);
      break;
    }
    /* When the light is moving and the "quick light move" mode is enabled... */
    if (movingLight && quickLightMove) {
      /* Cut the area of the depth map by one fourth. */
      if (useTextureRectangle) {
        width = depthMapRectWidth >> 1;
        height = depthMapRectHeight >> 1;
      } else {
        width = depthMapSize >> 1;
        height = depthMapSize >> 1;
      }
    } else {
      /* Normal size depth map. */
      if (useTextureRectangle) {
        width = depthMapRectWidth;
        height = depthMapRectHeight;
      } else {
        width = depthMapSize;
        height = depthMapSize;
      }
    }
    if (useShadowMapSupport) {
      if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
        precision = 16;
        depthBias = depthBias16;
      } else {
        precision = 24;
        depthBias = depthBias24;
      }
      if (useCopyTexImage) {
        depthTextureType = " hw [CopyTexImage]";
      } else {
        depthTextureType = " hw [ReadPixels]";
      }
    } else {
      if (depthMapPrecision == GL_UNSIGNED_SHORT) {
        precision = 16;
        depthBias = depthBias16;
      } else {
        precision = 8;
        depthBias = depthBias8;
      }
      depthTextureType = "";
    }
    sprintf(title,
      "shadowcast - %s (%dx%d %d-bit%s) depthBias=%d, slopeScale=%f, filter=%s",
      modeName, width, height, precision, depthTextureType, depthBias, slopeScale,
      (useShadowMapSupport && (hwDepthMapFiltering == GL_LINEAR)) ? "LINEAR" : "NEAREST");
    glutSetWindowTitle(title);
    needTitleUpdate = 0;
  }
}

GLfloat Ka[4];
GLfloat globalAmbientIntensity[4] = { 0.2, 0.2, 0.2, 1.0 };

/* setAmbientAndDiffuseMaterial - update the ambient and diffuse
   material setting for per-vertex lighting, and if single-pass hardware
   shadow mapping is also supported, update a constant color in the
   NV_register_combiners with the global ambient. */
void
setAmbientAndDiffuseMaterial(GLfloat *material)
{
  int i;

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);

  if (hasShadow && hasRegisterCombiners) {
    for (i=0; i<4; i++) {
      Ka[i] = material[i] * globalAmbientIntensity[i] * ambientPower;
    }
    /* The single-pass hardware shadow mapping mode programs
       the register combiners to treat CONSTANT_COLOR1 as the
       global ambient. */
    glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, Ka);
  }
}


/*** DRAW VARIOUS SCENES ***/

static void
drawSphere(void)
{
  gluSphere(q, 0.55, 16, 16);
}

/* drawObjectConfiguration - draw one of the various supported object
   configurations */
void
drawObjectConfiguration(void)
{
  switch (objectConfiguration) {
  case OC_MGF:
  default:
    // although it doesn't make sense, on GF4 Ti 4200 
    // it's slightly faster when "if(drawOnlyZ) mgf_draw_onlyz(); else" is deleted
    if(drawOnlyZ) rr2gl_draw_onlyz();
		else if(drawIndexed) rr2gl_draw_indexed();
			else rr2gl_draw_colored();
    break;
  }
}

/* drawScene - multipass shadow algorithms may need to call this routine
   more than once per frame. */
void
drawScene(void)
{
  drawObjectConfiguration();
}

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void
drawLight(void)
{
  glPushMatrix();
    glTranslatef(lv[0], lv[1], lv[2]);
    glDisable(GL_LIGHTING);
    glColor3f(1,1,0);
    gluSphere(q, 0.05, 10, 10);
    glEnable(GL_LIGHTING);
  glPopMatrix();
}

void
updateMatrices(void)
{
  ed[0]=15*sin(eyeAngle);
  ed[1]=eyeHeight;
  ed[2]=15*cos(eyeAngle);
  buildLookAtMatrix(eyeViewMatrix,
    15*sin(eyeAngle), eyeHeight, 15*cos(eyeAngle),
    0, 3, 0,
    0, 1, 0);
  
  lv[0] = 4 * sin(lightAngle);
  lv[1] = 0.5 * lightHeight + 3;
  lv[2] = 4 * cos(lightAngle);
  lv[3] = 1.0;

  buildLookAtMatrix(lightViewMatrix,
    lv[0], lv[1], lv[2],
    0, 3, 0,
    0, 1, 0);

  buildPerspectiveMatrix(lightFrustumMatrix, 
    lightFieldOfView, 1.0, lightNear, lightFar);

  if (showLightViewFrustum) {
    invertMatrix(lightInverseViewMatrix, lightViewMatrix);
    invertMatrix(lightInverseFrustumMatrix, lightFrustumMatrix);
  }
}

void
setupEyeView(void)
{
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixd(eyeFrustumMatrix);

  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixd(eyeViewMatrix);
  glTranslatef(eye_shift[0],eye_shift[1],eye_shift[2]);
}

/* drawShadowMapFrustum - Draw dashed lines around the light's view
   frustum to help visualize the region captured by the depth map. */
void
drawShadowMapFrustum(void)
{
  /* Go from light clip space, ie. the cube [-1,1]^3, to world space by
     transforming each clip space cube corner by the inverse of the light
     frustum transform, then by the inverse of the light view transform.
     This results in world space coordinate that can then be transformed
     by the eye view transform and eye projection matrix and on to the
     screen. */
  if (showLightViewFrustum) {
    glEnable(GL_LINE_STIPPLE);
    glPushMatrix();
      glMultMatrixd(lightInverseViewMatrix);
      glMultMatrixd(lightInverseFrustumMatrix);
      glDisable(GL_LIGHTING);
      glColor3f(1,1,1);
      /* Draw a wire frame cube with vertices at the corners
         of clip space.  Draw the top square, drop down to the
         bottom square and finish it, then... */
      glBegin(GL_LINE_STRIP);
        glVertex3f(1,1,1);
        glVertex3f(1,1,-1);
        glVertex3f(-1,1,-1);
        glVertex3f(-1,1,1);
        glVertex3f(1,1,1);
        glVertex3f(1,-1,1);
        glVertex3f(-1,-1,1);
        glVertex3f(-1,-1,-1);
        glVertex3f(1,-1,-1);
        glVertex3f(1,-1,1);
      glEnd();
      /* Draw the final three line segments connecting the top
         and bottom squares. */
      glBegin(GL_LINES);
        glVertex3f(1,1,-1);
        glVertex3f(1,-1,-1);

        glVertex3f(-1,1,-1);
        glVertex3f(-1,-1,-1);

        glVertex3f(-1,1,1);
        glVertex3f(-1,-1,1);
      glEnd();
      glEnable(GL_LIGHTING);
    glPopMatrix();
    glDisable(GL_LINE_STIPPLE);
  }
}

/* drawUnshadowedEyeView - draw the scene without shadows.  This requires
   a single rendering pass. */
void
drawUnshadowedEyeView(void)
{
  glLightfv(GL_LIGHT0, GL_POSITION, lv);

  drawScene();
  drawLight();
  drawShadowMapFrustum();
}

void
setupLightView(int square)
{
  glMatrixMode(GL_PROJECTION);
  if (square) {
    glLoadMatrixd(lightFrustumMatrix);
  } else {
    glLoadIdentity();
    glScalef(winAspectRatio, 1, 1);
    glMultMatrixd(lightFrustumMatrix);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixd(lightViewMatrix);
}

void
drawLightView(void)
{
  glLightfv(GL_LIGHT0, GL_POSITION, lv);
  drawScene();
}

void
useBestShadowMapClamping(GLenum target)
{
  if (hasTextureBorderClamp) {
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB);
  } else {
    /* We really want "clamp to border", but this may be good enough. */
    if (hasTextureEdgeClamp) {
      glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
      /* A bad option, but still better than "repeat". */
      glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
  }
}

void
swapShorts(unsigned int size, GLubyte *depthBuffer)
{
  unsigned int i;
  GLubyte tmp;

  for (i=0; i<size; i++) {
    tmp = depthBuffer[2*i + 0];
    depthBuffer[2*i + 0] = depthBuffer[2*i + 1];
    depthBuffer[2*i + 1] = tmp;
  }
}

void
updateDepthMap(void)
{
  /* Lazily update the depth map only if there is some indication (ie,
     needDepthMapUpdate is set) that the scene has changed.  This
     allows the scene to be viewed from different orientations without
     regenerating the depth map.  This optimization is possible because
     shadows are viewer independent. */
  if (needDepthMapUpdate) {

    /* Buffer for reading the depth buffer into. */
    static int depthBufferSize = 0;
    static GLubyte *depthBuffer = NULL;

    static int lastLowResWidth = 0, lastHiResWidth = 0;
    static int lastLowResHwWidth = 0, lastHiResHwWidth = 0;

    static int lastLowResHeight = 0, lastHiResHeight = 0;
    static int lastLowResHwHeight = 0, lastHiResHwHeight = 0;

    static GLenum lastLowResPrecision = 0, lastHiResPrecision = 0;
    static GLenum lastLowResHwPrecision = 0, lastHiResHwPrecision = 0;
    static GLenum lastLowResHwFiltering = 0, lastHiResHwFiltering = 0;

    int bytesPerDepthValue;
    int requiredSize;

    int width, height, lowRes;
    GLenum target;  /* Either 2D or rectangular. */
    GLuint texobj;

    if (movingLight && quickLightMove) {
      lowRes = 1;
      if (useTextureRectangle) {
        width = depthMapRectWidth >> 1;
        height = depthMapRectHeight >> 1;
      } else {
        width = depthMapSize >> 1;
        height = depthMapSize >> 1;
      }
      if (width < 1) {
        /* Just to be safe. */
        width = 1;
      }
      if (height < 1) {
        /* Just to be safe. */
        height = 1;
      }
    } else {
      lowRes = 0;
      if (useTextureRectangle) {
        width = depthMapRectWidth;
        height = depthMapRectHeight;
      } else {
        width = depthMapSize;
        height = depthMapSize;
      }
    }

    glViewport(0, 0, width, height);

    /* Setup light view with a square aspect ratio
       since the texture is square. */
    setupLightView(1);

    if (!depthMapBackFaceCulling) {
      /* Often it works to enable back face culling during the depth map
         construction stage.  Back face culling has the standard
         performance advantage of not rasterizing back facing polygons.
         However, there are two reasons to avoid back face culling
         when constructing the depth map.  The first issue involves
         what happens when a closed object is intersected by the near
         clip plane.  In this case, back facing polygons are discarded
         leaving a "hole" in the shadow map.  The second issue is what
         happens when front facing polygons have extreme slopes yet back
         facing polygons do not.  When combined with polygon offset,
         this can cause situations where holes open up in depth map
         due to extreme polygon offsets due to the extreme front facing
         polygon slopes.  If the back facing polygons did not have such
         extreme slopes, the shadowing artifacts could be avoided.  */
      glDisable(GL_CULL_FACE);
    }
 
    /* Only need depth values so avoid writing colors for speed. */
    glColorMask(0,0,0,0);
    glDisable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_FILL);
    drawOnlyZ = true;

    if(useScissor) {
      glScissor(0,0,width,height);
      glEnable(GL_SCISSOR_TEST);
    }

    drawScene();

    if (useShadowMapSupport) {
      int alreadyMatches = 0;
 
      /* The first component is the border depth (at the far clip
         plane).  The other three values are ignored. */
      static const GLfloat hwBorderColor[4] = { 1.0, 0.0, 0.0, 0.0 };

      /* Hardware shadow mapping using SGIX_shadow and SGIX_depth_texture
         extensions. */

      if (!useCopyTexImage) {

        /* Allocate/reallocate the depth buffer memory as necessary. */
        if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
          bytesPerDepthValue = 2;
        } else {
          assert(hwDepthMapPrecision == GL_UNSIGNED_INT);
          bytesPerDepthValue = 4;
        }
        requiredSize = width * height * bytesPerDepthValue;
        if (requiredSize > depthBufferSize) {
          depthBuffer = (GLubyte *)realloc(depthBuffer, requiredSize);
          if (depthBuffer == NULL) {
            fprintf(stderr, "shadowcast: depth readback buffer realloc failed\n");
            exit(1);
          }
          depthBufferSize = requiredSize;
        }

        /* Read back the depth buffer to memory. */
        glReadPixels(0, 0, width, height,
          GL_DEPTH_COMPONENT, hwDepthMapPrecision, depthBuffer);
      }

      if (lowRes) {
        if (useTextureRectangle) {
          target = GL_TEXTURE_RECTANGLE_NV;
          texobj = TO_LOWRES_HW_DEPTH_MAP_RECT;
        } else {
          target = GL_TEXTURE_2D;
          texobj = TO_LOWRES_HW_DEPTH_MAP;
        }
        glBindTexture(target, texobj);
        if ((width != lastLowResHwWidth) ||
            (height != lastLowResHwHeight) ||
            (hwDepthMapPrecision != lastLowResHwPrecision) ||
            (hwDepthMapFiltering != lastLowResHwFiltering)) {
          glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, hwBorderColor);
          if (mipmapShadowMap && !useTextureRectangle) {
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
          } else {
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
              hwDepthMapFiltering);
          }
          glTexParameteri(target, GL_TEXTURE_MAG_FILTER,
            hwDepthMapFiltering);
          useBestShadowMapClamping(target);
          glTexParameteri(target,
            GL_TEXTURE_COMPARE_SGIX, GL_TRUE);
          glTexParameteri(target,
            GL_TEXTURE_COMPARE_OPERATOR_SGIX, GL_TEXTURE_LEQUAL_R_SGIX);
          if (useCopyTexImage) {
            glCopyTexImage2D(target, 0, hwDepthMapInternalFormat,
              0, 0, width, height, 0);
          } else {
            glTexImage2D(target, 0, hwDepthMapInternalFormat,
              width, height, 0, GL_DEPTH_COMPONENT,
              hwDepthMapPrecision, depthBuffer);
          }
          lastLowResHwWidth = width;
          lastLowResHwHeight = height;
          lastLowResHwPrecision = hwDepthMapPrecision;
          lastLowResHwFiltering = hwDepthMapFiltering;
        } else {
          /* Texture object dimensions already initialized. */
          alreadyMatches = 1;
          if (useCopyTexImage) {
            glCopyTexSubImage2D(target, 0, 0, 0, 
              0, 0, width, height);
          } else {
            glTexSubImage2D(target, 0,
              0, 0, width, height, GL_DEPTH_COMPONENT,
              hwDepthMapPrecision, depthBuffer);
          }
        }
      } else {
        /* Hi-res mode */
        if (softLight>=0) {
          target = GL_TEXTURE_2D;
          texobj = TO_SOFT_DEPTH_MAP+softLight;
        } else
        if (useTextureRectangle) {
          target = GL_TEXTURE_RECTANGLE_NV;
          texobj = TO_HW_DEPTH_MAP_RECT;
        } else {
          target = GL_TEXTURE_2D;
          texobj = TO_HW_DEPTH_MAP;
        }
        glBindTexture(target, texobj);
        if ( (width != ((softLight<0)?lastHiResHwWidth:softWidth[softLight])) ||
             (height != ((softLight<0)?lastHiResHwHeight:softHeight[softLight])) ||
             (hwDepthMapPrecision != ((softLight<0)?lastHiResHwPrecision:softPrecision[softLight])) ||
             (hwDepthMapFiltering != ((softLight<0)?lastHiResHwFiltering:softFiltering[softLight])) ) {
          glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, hwBorderColor);
          if (mipmapShadowMap && !useTextureRectangle) {
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
          } else {
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, hwDepthMapFiltering);
          }
          glTexParameteri(target, GL_TEXTURE_MAG_FILTER, hwDepthMapFiltering);
          useBestShadowMapClamping(target);
          glTexParameteri(target, GL_TEXTURE_COMPARE_SGIX, GL_TRUE);
          glTexParameteri(target, GL_TEXTURE_COMPARE_OPERATOR_SGIX, GL_TEXTURE_LEQUAL_R_SGIX);
          if (useCopyTexImage) {
            glCopyTexImage2D(target, 0, hwDepthMapInternalFormat,
              0, 0, width, height, 0);
          } else {
            glTexImage2D(target, 0, hwDepthMapInternalFormat,
              width, height, 0, GL_DEPTH_COMPONENT, hwDepthMapPrecision, depthBuffer);
          }
          if(softLight>=0) {
            softWidth[softLight] = width;
            softHeight[softLight] = height;
            softPrecision[softLight] = hwDepthMapPrecision;
            softFiltering[softLight] = hwDepthMapFiltering;
          } else {
            lastHiResHwWidth = width;
            lastHiResHwHeight = height;
            lastHiResHwPrecision = hwDepthMapPrecision;
            lastHiResHwFiltering = hwDepthMapFiltering;
          }
        } else {
          /* Texture object dimensions already initialized. */
          alreadyMatches = 1;
          if (useCopyTexImage) {
            glCopyTexSubImage2D(target, 0, 0, 0, 0, 0, width, height);
          } else {
            glTexSubImage2D(target, 0,
              0, 0, width, height, GL_DEPTH_COMPONENT, hwDepthMapPrecision, depthBuffer);
          }
        }
      }
      if (mipmapShadowMap && !useTextureRectangle) {
        int level = 1;

        while (width > 1 || height > 1) {
          width = width >> 1;
          if (width == 0) {
            width = 1;
          }
          height = height >> 1;
          if (height == 0) {
            height = 1;
          }
          glViewport(0, 0, width, height);
          glClear(GL_DEPTH_BUFFER_BIT);
          drawScene();
          if (useCopyTexImage) {
            if (alreadyMatches) {
              glCopyTexSubImage2D(target, level, 0, 0, 0, 0, width, height);
            } else {
              glCopyTexImage2D(target, level, hwDepthMapInternalFormat,
                0, 0, width, height, 0);
            }
          } else {
            /* Read back the depth buffer to memory. */
            glReadPixels(0, 0, width, height,
              GL_DEPTH_COMPONENT, hwDepthMapPrecision, depthBuffer);
            glTexImage2D(target, level, hwDepthMapInternalFormat,
              width, height, 0, GL_DEPTH_COMPONENT, hwDepthMapPrecision, depthBuffer);
          }
          level++;
        }
      }
    }

    /* Reset state back to normal. */
    drawOnlyZ = false;
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_LIGHTING);
    glColorMask(1,1,1,1);

    if (!depthMapBackFaceCulling) {
      glEnable(GL_CULL_FACE);
    }

    if(useScissor) {
      glDisable(GL_SCISSOR_TEST);
    }

    glViewport(0, 0, winWidth, winHeight);

    if(softLight<0 || softLight==useLights-1) needDepthMapUpdate = 0;
  }
}

void
drawDepth(void)
{
  GLenum target;
  GLuint texobj;

  updateDepthMap();

  glViewport(0, 0, winWidth, winHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glScalef(winAspectRatio, 1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glColor3f(1,1,1);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  if (useShadowMapSupport) {
    if (movingLight && quickLightMove) {
      if (useTextureRectangle) {
        target = GL_TEXTURE_RECTANGLE_NV;
        texobj = TO_LOWRES_HW_DEPTH_MAP_RECT;
      } else {
        target = GL_TEXTURE_2D;
        texobj = TO_LOWRES_HW_DEPTH_MAP;
      }
    } else {
      if (useTextureRectangle) {
        target = GL_TEXTURE_RECTANGLE_NV;
        texobj = TO_HW_DEPTH_MAP_RECT;
      } else {
        target = GL_TEXTURE_2D;
        texobj = TO_HW_DEPTH_MAP;
      }
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
    /* Turn off shadow mapping comparisons so we can "show" the depth
       values directly.  With GL_TEXTURE_COMPARE_SGIX set to GL_FALSE,
       a GL_DEPTH_COMPONENT texture behaves as a GL_LUMINANCE texture.
       However, while the depth components may be 16-bit or 24-bit values,
       be warned that viewing the depth components as luminance values
       will only show you the most significant bits. */
    glTexParameteri(target, GL_TEXTURE_COMPARE_SGIX, GL_FALSE);
  } else {
    if (movingLight && quickLightMove) {
      if (useTextureRectangle) {
        target = GL_TEXTURE_RECTANGLE_NV;
        texobj = TO_LOWRES_DEPTH_MAP;
      } else {
        target = GL_TEXTURE_2D;
        texobj = TO_LOWRES_DEPTH_MAP;
      }
    } else {
      if (useTextureRectangle) {
        target = GL_TEXTURE_RECTANGLE_NV;
        texobj = TO_DEPTH_MAP_RECT;
      } else {
        target = GL_TEXTURE_2D;
        texobj = TO_DEPTH_MAP;
      }
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
    if (showDepthMapMSBs) {
      if (littleEndian) {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_ALPHA);
      } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
      }
    } else {
      if (littleEndian) {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
      } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_ALPHA);
      }
    }
  }

  glEnable(target);
  glBindTexture(target, texobj);
  glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, textureLodBias);

  if (useTextureRectangle) {
    GLfloat width, height;

    if (movingLight && quickLightMove) {
      width = depthMapRectWidth >> 1;
      height = depthMapRectHeight >> 1;
    } else {
      width = depthMapRectWidth;
      height = depthMapRectHeight;
    }

    /* Rectangular textures use UN-normalized texture coordinates in the
       [0..width]x[0..height] range. */
    glBegin(GL_QUADS);
      glTexCoord2f(0,0);
      glVertex2f(-1,-1);
      glTexCoord2f(width,0);
      glVertex2f(1,-1);
      glTexCoord2f(width,height);
      glVertex2f(1,1);
      glTexCoord2f(0,height);
      glVertex2f(-1,1);
    glEnd();

  } else {
    /* Standard 2D textures use normalized texture coordinates in the
       [0..1]x[0..1] rnage. */
    glBegin(GL_QUADS);
      glTexCoord2f(0,0);
      glVertex2f(-1,-1);
      glTexCoord2f(1,0);
      glVertex2f(1,-1);
      glTexCoord2f(1,1);
      glVertex2f(1,1);
      glTexCoord2f(0,1);
      glVertex2f(-1,1);
    glEnd();
  }

  if (useShadowMapSupport) {
    /* Re-enable shadow map comparsions. */
    glTexParameteri(target, GL_TEXTURE_COMPARE_SGIX, GL_TRUE);
  }

  glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 0.0);
  glDisable(target);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
}

/* Scale and bias [-1,1]^3 clip space into [0,1]^3 texture space. */
GLdouble Smatrix[16] = {
    0.5, 0,   0,   0,
    0,   0.5, 0,   0,
    0,   0,   0.5, 0,
    0.5, 0.5, 0.5, 1.0
};
/* Scale and bias [-1,1]^3 clip space into [0,1]^3 texture space, and then
   move LightSpace Z to S, move 256 * LightSpace Z to T, zero out R, and 
   leave LightSpace Q alone. */
GLdouble RSmatrix[16] = {
    0,   0,   0,   0,
    0,   0,   0,   0,
    0.5, 128, 0,   0,
    0.5, 128, 0,   1.0
};

#define MAG16 65535.0f
#define MAG24 16777215.0f

void
configTexgen(int rangeMap, int bits)
{
  GLfloat p[4];
  GLfloat wrapScale;
  GLdouble m1[16], m2[16];

  if (rangeMap) {
    /* Generate the depth map space planar distance as the S/Q texture
       coordinate. */

    if ((depthMapPrecision == GL_UNSIGNED_SHORT) && !useShadowMapSupport) {
      glBindTexture(GL_TEXTURE_2D, TO_MAP_16BIT);
      glEnable(GL_TEXTURE_2D);
      if (hasTextureRectangle) {
        glDisable(GL_TEXTURE_RECTANGLE_NV);
      }
    } else {
      glBindTexture(GL_TEXTURE_1D, TO_MAP_8BIT);
      glEnable(GL_TEXTURE_1D);
      glDisable(GL_TEXTURE_2D);
      if (hasTextureRectangle) {
        glDisable(GL_TEXTURE_RECTANGLE_NV);
      }
    }

    copyMatrix(m1, RSmatrix);
    wrapScale = (1 << bits);
    m1[0] *= wrapScale;
    m1[4] *= wrapScale;
    m1[8] *= wrapScale;
    m1[12] *= wrapScale;
  } else {
    GLenum target;
    GLuint texobj;

    copyMatrix(m1, Smatrix);
    
    if (useTextureRectangle) {
      GLfloat width, height;

      /* Texture rectangle textures require unnormalized
         texture coordinates in the [0,width]x[0,height] range
         rather than the typical [0,1]x[0,1] for 2D textures.
         For this reason, scale the S row of the matrix by
         width and the T row of the matrix by height. */
      if (movingLight && quickLightMove) {
        width = depthMapRectWidth >> 1;
        height = depthMapRectHeight >> 1;
      } else {
        width = depthMapRectWidth;
        height = depthMapRectHeight;
      }
      /* Scale S row to be [ .5*width, 0, 0, .5*width] */
      m1[0] *= width;
      m1[12] *= width;
      /* Scale T row to be [ 0 .5*height, 0, 0, .5*height] */
      m1[5] *= height;
      m1[13] *= height;

      /* Pick the rectangular target and texture object. */
      target = GL_TEXTURE_RECTANGLE_NV;
      if (useShadowMapSupport) {
        if (movingLight && quickLightMove) {
          texobj = TO_LOWRES_HW_DEPTH_MAP_RECT;
        } else {
          texobj = TO_HW_DEPTH_MAP_RECT;
        }
      } else {
        if (movingLight && quickLightMove) {
          texobj = TO_LOWRES_DEPTH_MAP_RECT;
        } else {
          texobj = TO_DEPTH_MAP_RECT;
        }
      }
    } else {
      /* Pick the 2D target and texture object. */
      target = GL_TEXTURE_2D;
      if (useShadowMapSupport) {
        if (movingLight && quickLightMove) {
          texobj = TO_LOWRES_HW_DEPTH_MAP;
        } else {
          texobj = TO_HW_DEPTH_MAP;
        }
      } else {
        if (movingLight && quickLightMove) {
          texobj = TO_LOWRES_DEPTH_MAP;
        } else {
          texobj = TO_DEPTH_MAP;
        }
      }
    }
    if (softLight>=0) {
      texobj = TO_SOFT_DEPTH_MAP+softLight;
    }
    glBindTexture(target, texobj);
    glEnable(target);
  }

  /* For performance reasons, it is good to only enable the
     particular texture coordinates required.  This saves the
     driver or hardware possibly having to compute dot products
     for unused texture coordinates.

     Here are the rules:

       1D texturing only use S and Q.

       2D texturing only uses S, T, and Q.

       2D shadow mapping uses S, T, R, and Q.

     For completeness:

       3D texturing uses S, T, R, and Q.

       Cube map texturing uses S, T, and R.

     The other thing to avoid for performance reasons is combining the
     texture matrix with eye (or object) linear texgen.  That is the
     rationale for the two matrix pre-multiplies below. Rather than
     simply generating identity eye coordinates for S, T, R, and Q and
     then having the texture matrix apply the appropriate 4x4 transform,
     we "bake" the right terms into the eye linear texgen planes. */

  multMatrices(m2, m1, lightFrustumMatrix);
  multMatrices(m1, m2, lightViewMatrix);

  /* S and Q are always needed */
  p[0] = m1[0];
  p[1] = m1[4];
  p[2] = m1[8];
  p[3] = m1[12];
  glTexGenfv(GL_S, GL_EYE_PLANE, p);
  p[0] = m1[3];
  p[1] = m1[7];
  p[2] = m1[11];
  p[3] = m1[15];
  glTexGenfv(GL_Q, GL_EYE_PLANE, p);

  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_Q);

  if (rangeMap && (bits == 0)) {
    /* rangeMap is a 1D mode so it does not need to generate T or R. */
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);
  } else {
    if (useShadowMapSupport) {
      /* Hardware shadow mapping is a 2D texturing mode (uses S and T) that
         also compares with R so S, T, and R must be generated. */
      p[0] = m1[1];
      p[1] = m1[5];
      p[2] = m1[9];
      p[3] = m1[13];
      glTexGenfv(GL_T, GL_EYE_PLANE, p);

      p[0] = m1[2];
      p[1] = m1[6];
      p[2] = m1[10];
      p[3] = m1[14];
      glTexGenfv(GL_R, GL_EYE_PLANE, p);

      glEnable(GL_TEXTURE_GEN_S);
      glEnable(GL_TEXTURE_GEN_T);
      glEnable(GL_TEXTURE_GEN_R);
    } else {
      /* Standard 2D texture mapping uses S, T, and Q, but does not use R. */
      p[0] = m1[1];
      p[1] = m1[5];
      p[2] = m1[9];
      p[3] = m1[13];
      glTexGenfv(GL_T, GL_EYE_PLANE, p);

      glEnable(GL_TEXTURE_GEN_S);
      glEnable(GL_TEXTURE_GEN_T);
      glDisable(GL_TEXTURE_GEN_R);
    }
  }
}

void
disableTexgen(void)
{
  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
  glDisable(GL_TEXTURE_GEN_R);
  glDisable(GL_TEXTURE_GEN_Q);
  if (hasTextureRectangle) {
    glDisable(GL_TEXTURE_RECTANGLE_NV);
  }
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_1D);
}

void
drawEyeViewDepthTextured(void)
{
  GLenum target;

  updateDepthMap();

  if (useStencil) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  } else {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  if (wireFrame) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }

  setupEyeView();
  if (showDepthMapMSBs) {
    configTexgen(rangeMap, 0);
  } else {
    configTexgen(rangeMap, 8);
  }

  /* Do not waste time lighting; rely on drawLight to re-enable
     lighting. */
  glDisable(GL_LIGHTING);

  if (useTextureRectangle) {
    target = GL_TEXTURE_RECTANGLE_NV;
  } else {
    target = GL_TEXTURE_2D;
  }

  if (useShadowMapSupport) {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
    glTexParameteri(target, GL_TEXTURE_COMPARE_SGIX, GL_FALSE);
  } else {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
    if (showDepthMapMSBs) {
      if (rangeMap || littleEndian) {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_ALPHA);
      } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
      }
    } else {
      if (rangeMap || littleEndian) {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
      } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_ALPHA);
      }
    }
  }

  drawScene();

  if (useShadowMapSupport) {
    glTexParameteri(target, GL_TEXTURE_COMPARE_SGIX, GL_TRUE);
  }

  disableTexgen();

  drawLight();
  drawShadowMapFrustum();
}

/* How to do a 16-bit shadow map comparison in three GeForce/Quadro GPU
   rendering passes...

   Here's what we want to compute:

     if (depthMap > zRange) {
       return UNshadowed;
     } else {
       return SHADOWED;
     }

   But consider what happens when depthMap and zRange are both 16-bit
   quantities and the largest quantity that the GPU can operate on
   per-pixel is an 8-bit quantity.

   For inspiration, consider how you compare 2-digit decimal numbers.
   If you compare 43 to 76, you can compare the two most sigificant
   digits, 4 and 7.  If these two most sigificant digits are not equal,
   you know the result of the comparison without even considering the
   two least significant digits.  But if you compare 53 and 57, the two
   most sigificant digits are both 5 so you must compare the two least
   sigificant digits.  You only need to be able to compare 1-digit
   numbers, to compare 2 (and more) digit numbers.

   Think of a 16-bit number such as 0xfe4a as being made of two
   8-bit digits, 0xfe (the most sigificant digit) and 0x4a (the least
   sigifnicant digit).  You can compare 16-bit numbers by comparing their
   consituent digits just like you would for decimal multi-digit numbers.

   We can split depthMap and zRange into two 8-bit "digits":

     depthMap = (depthMapMSB << 8) + depthMapLSB
     zRange   = (zRangeMSB   << 8) + zRangeLSB

   The MSB and LSB suffixes stand for Most Significant and Least
   Significant Bits.

   Now the shadow map comparison can be computed using only 8-bit
   operations as:

     if (depthMapMSB > zRangeMSB) {
       return UNshadowed;
     } else {
       if (depthMapMSB < zRangeMSB) {
         return SHADOWED;
       } else {
         // MSBs must be equal since not greater than and not less than
         if (depthMapLSB > zRangeLSB) {
           return UNshadowed;
         } else {
           return SHADOWED;
         }
       }
     }

   Unfortunately, things are a bit more complicated.  The above expression
   cannot be directly expressed as a single GeForce or Quadro GPU
   rendering pass.  But we can obtain a functionally identical result
   by re-writing the above expression as a set of GPU rendering passes.

   The first step to converting the above expression into a set of
   GPU rendering passes is re-writing the comparisons in the form
   of conditional operations supported by OpenGL.  These are the
   standard alpha test and the NV_register_combiner extension's MUX
   operation.

   The alpha test allows a fragment's alpha value to be compared to
   a constant.  Instead of comparing "A > B", this can be expressed as
   "A - B > 0".  If "A - B" is computed by the register combiners, then
   alpha testing can be used to determine if the result is greater
   than zero.

   The register combiner MUX operation compares the alpha value in the
   SPARE0 register to 0.5.  If the SPARE0 alpha value is greater than
   0.5, the "A*B" product is output and otherwise the "C*D" product is
   output.  The comparsion "A > B" can be re-written as "A + (1-B) -
   0.5 > 0.5".  The expression "A + (1-B) - 0.5" clamped to the [0,1] range
   can be computed in the register combiners and is effectively "A - B +
   0.5" (called a "signed add" by the EXT_texture_env_combine extension).
   Using the signed add and the MUX operation's comparison with 0.5, we
   can also perform the "A > B" comparison within the register combiners.

   The shadow comparison using only 8-bit operations can be re-written
   using the alpha test and register combiners MUX operation comparisons
   as:

     if (depthMapMSB - zRangeMSB > 0.0) {
       return UNshadowed;
     } else {
       if (zRangeMSB - (1 - depthMapMSB) - 0.5 > 0.5) {
         return SHADOWED;
       } else {
         // MSBs must be equal since not greater than and not less than
         if (depthMapLSB - zRangeLSB > 0.0) {
           return UNshadowed;
         } else {
           return SHADOWED;
         }
       }
     }

   Even though this matches the forms of comparisons available in GPUs,
   the above expression is still too complex for a single rendering pass.

   We can break the above expression up into three distinct passes.  We
   rely on stencil testing to match the correct precedence of the 8-bit
   shadow map comparisons.

   pass #1:

     fragment.rgb = SHADOWED

     if (fragment.z <= pixel.z) {
       pixel.Z   = fragment.z
       pixel.rgb = fragment.rgb
     }

   pass #2:

     texture0.a   = depthMapMSB
     texture1.a   = zRangeMSB
     fragment.rgb = UNshadowed
     fragment.a   = max(0, texture0.a - texture1.a)

     if (fragment.a > 0.0) {
       if (fragment.z == pixel.z) {
         pixel.s   = (pixel.s & ~0x1) | 0x1
         pixel.rgb = fragment.rgb
       }
     }

   pass #3:

     texture0.rgb = depthMapLSB
     texture0.a   = depthMapMSB
     texture1.rgb = zRangeLSB
     texture1.a   = zRangeMSB
     fragment.rgb = UNshadowed
     fragment.a   = (texture1.a + (1 - texture0.a) - 0.5 > 0.5) ?
                    0 : texture0.rgb - texture1.rgb

     if (fragment.a > 0.0) {
       if (pixel.s & 0x1 == 0x0) {
         if (fragment.z == pixel.z) {
           pixel.rgb = fragment.rgb
         }
       }
     }

   Note that SHADOWED pixels rendered in the first pass can be re-written
   as UNshadowed pixels in the subsequent two passes.  Also stencil
   testing in the second and third passes keeps an UNshadowed pixel
   written in the second pass from being over-written in the third pass.

   This re-writing is quite a bit more involved than the the original
   shadow map comparison, but the advantage of this re-writing is that
   now a higher precision shadow map comparison can be preformed entirely
   with operations accelerated by GPUs. */

void
configCombiners(int pass)
{
  if (hasRegisterCombiners) {
    if (pass == 1) {
      assert(depthMapPrecision == GL_UNSIGNED_SHORT ||
             depthMapPrecision == GL_UNSIGNED_BYTE);
      
      glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);

      /* Discard entire RGB combiner stage 0 */
      glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV,
        GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

      /* Aa = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
      /* Ba = texture0a = depth map MSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV,
        GL_TEXTURE0_ARB, GL_SIGNED_IDENTITY_NV,
        littleEndian ? GL_ALPHA : GL_BLUE);
      /* Ca = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
      /* Da = texture1a = range MSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV,
        GL_TEXTURE1_ARB, GL_SIGNED_NEGATE_NV, GL_ALPHA);

      /* spare0a = 1 * tex0a + 1 * -tex1a = tex0a - tex1a */
      glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
        GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

    } else {
      assert(pass == 2);
      assert(depthMapPrecision == GL_UNSIGNED_SHORT);

      glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);

      /* Argb = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
      /* Brgb = texture0rgb = depth map LSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV,
        GL_TEXTURE0_ARB, GL_SIGNED_IDENTITY_NV,
        littleEndian ? GL_RGB : GL_ALPHA);
      /* Crgb = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
      /* Drgb = texture1rgb = range LSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV,
        GL_TEXTURE1_ARB, GL_SIGNED_NEGATE_NV, GL_RGB);

      /* spare0rgb = 1 * tex0rgb + 1 * -tex1rgb = tex0rgb - tex1rgb */
      glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
        GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

      /* Aa = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
      /* Ba = texture1a = range MSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV,
        GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
      /* Ca = one */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
      /* Da = texture0a = depth map MSB */
      glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV,
        GL_TEXTURE0_ARB, GL_UNSIGNED_INVERT_NV,
        littleEndian ? GL_ALPHA : GL_BLUE);

      /* spare0a = 1 * tex1a + 1 * (1-tex0a) - 0.5 = tex1a - tex0a + 0.5 */
      glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
        GL_NONE, GL_BIAS_BY_NEGATIVE_ONE_HALF_NV,
        GL_FALSE, GL_FALSE, GL_FALSE);

      /* Discard entire RGB combiner stage 1 */
      glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV,
        GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

      /* Aa = one */
      glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_A_NV,
        GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
      /* Ba = spare0b = tex0b - tex1b */
      glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_B_NV,
        GL_SPARE0_NV, GL_SIGNED_IDENTITY_NV, GL_BLUE);
      /* Ca = zero */
      glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_C_NV,
        GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
      /* Da = zero */
      glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_D_NV,
        GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

      /* spare0a = (tex1 - tex0 + 0.5) > 0.5 ? 0 : tex0b - tex1b */
      glCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA,
        GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
        GL_NONE, GL_NONE, GL_FALSE, GL_FALSE,
        GL_TRUE);  /* Use the MUX operation. */
    }

    /* A = one */
    glFinalCombinerInputNV(GL_VARIABLE_A_NV,
      GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
    /* B = primary color */
    glFinalCombinerInputNV(GL_VARIABLE_B_NV,
      GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
    /* C = zero */
    glFinalCombinerInputNV(GL_VARIABLE_C_NV,
      GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
    /* D = zero */
    glFinalCombinerInputNV(GL_VARIABLE_D_NV,
      GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
    /* RGB = A*B + (1-A)*C + D = primary color */

    /* G = spare0a = max(0, tex0 - tex1) */
    glFinalCombinerInputNV(GL_VARIABLE_G_NV,
      GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
    /* Alpha = max(0, tex0 - tex1) */

    glEnable(GL_REGISTER_COMBINERS_NV);
  } else {
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PRIMARY_COLOR_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);

    glActiveTextureARB(GL_TEXTURE1_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD_SIGNED_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PREVIOUS_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_ONE_MINUS_SRC_ALPHA);
  }
  if (rcDebug) {
    printRegisterCombinersState();
  }
}

void
configCombinersForHardwareShadowPass(int withTexture)
{
  glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);
  glCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, lightDimColor);
  glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, Ka);

  /* Argb = one */
  glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV,
    GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
  /* Brgb = constant0rgb = light dimming */
  glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV,
    GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* Crgb = 1.0 - constant0rgb = 1.0 - light dimming */
  glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV,
    GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_INVERT_NV, GL_RGB);
  /* Drgb = texture0rgb = unshadowed percentage */
  glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV,
    GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

  /* spare0rgb = light dimming + (1.0 - light dimming) * unshadowed percentage */
  glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB,
    GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
    GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

  /* Discard entire Alpha combiner stage 0 */
  glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA,
    GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV,
    GL_NONE, GL_NONE,
    GL_FALSE, GL_FALSE, GL_FALSE);

  /* Argb = spare0rgb = light dimming + (1.0 - light dimming) * unshadowed percentage */
  glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV,
    GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* Brgb = primary color = light's diffuse contribution */
  glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV,
    GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* Crgb = constant 1 = Ka = ambient contribution */
  glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV,
    GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* Drgb = 1 */
  glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV,
    GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);

  /* spare0rgb =
     (light dimming + (1.0 - light dimming) * unshadowed percentage) * diffuse +
     ambient */
  glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB,
    GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV,
    GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

  /* Discard entire Alpha combiner stage 1 */
  glCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA,
    GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV,
    GL_NONE, GL_NONE, GL_FALSE, GL_FALSE,
    GL_TRUE);  /* Use the MUX operation. */
  
  if (withTexture==1) {
    /* A = texture1 = decal */
    glFinalCombinerInputNV(GL_VARIABLE_A_NV,
      GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  } else if (withTexture==2) {
    /* A = texture2 = spotmap */
    glFinalCombinerInputNV(GL_VARIABLE_A_NV,
      GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  } else {
    /* A = one */
    glFinalCombinerInputNV(GL_VARIABLE_A_NV,
      GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
  }
  /* B = spare0 */
  glFinalCombinerInputNV(GL_VARIABLE_B_NV,
    GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* C = zero */
  glFinalCombinerInputNV(GL_VARIABLE_C_NV,
    GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* E = unshadowed percentage */
  glFinalCombinerInputNV(GL_VARIABLE_E_NV,
    GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* F = specular */
  glFinalCombinerInputNV(GL_VARIABLE_F_NV,
    GL_SECONDARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* D = EF product = unshadowed percentage * specular contribution */
  glFinalCombinerInputNV(GL_VARIABLE_D_NV,
    GL_E_TIMES_F_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
  /* RGB = A*B + (1-A)*C + D = (texture1 OR 1) * space0 + specular * unshadowed */

  /* G = 1 */
  glFinalCombinerInputNV(GL_VARIABLE_G_NV,
    GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
  /* Alpha = 1 */

  glEnable(GL_REGISTER_COMBINERS_NV);

  if (rcDebug) {
    printRegisterCombinersState();
  }
}

void
disableCombiners(void)
{
  if (hasRegisterCombiners) {
    glDisable(GL_REGISTER_COMBINERS_NV);
  }
}

void
setLightIntensity()
{
  GLfloat col[4]={lightColor[0]*globalIntensity,lightColor[1]*globalIntensity,lightColor[2]*globalIntensity,lightColor[3]*globalIntensity};
  glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
  glLightfv(GL_LIGHT0, GL_SPECULAR, col);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, col);
}

void
drawHardwareShadowPass(void)
{
  glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);

  setLightIntensity();

  /* Texture 2 is spot map. */
  if(textureSpot) {
    glActiveTextureARB(GL_TEXTURE2_ARB);
    configTexgen(0, 0);
  }
  /* Texture 0 is depth map. */
  glActiveTextureARB(GL_TEXTURE0_ARB);
  configTexgen(0, 0);

  configCombinersForHardwareShadowPass(0);

  if (textureSpot) {
    configCombinersForHardwareShadowPass(2);
    glActiveTextureARB(GL_TEXTURE2_ARB);
    glBindTexture(GL_TEXTURE_2D, TO_SPOT);
    glEnable(GL_TEXTURE_2D);

    drawObjectConfiguration();

    glDisable(GL_TEXTURE_2D);
  } else 
    drawObjectConfiguration();
    
  disableCombiners();

  glActiveTextureARB(GL_TEXTURE0_ARB);
  disableTexgen();
}

void
drawEyeViewShadowed(int clear)
{
  if (softLight<0) updateDepthMap();

  if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (wireFrame) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }

  setupEyeView();
  glLightfv(GL_LIGHT0, GL_POSITION, lv);

  drawHardwareShadowPass();

  drawLight();
  drawShadowMapFrustum();
}

void
capturePrimary()
{
	//!!! needs windows at least 256x256
	unsigned width = 256;
	unsigned height = 256;

	// Setup light view with a square aspect ratio since the texture is square.
	setupLightView(1);

	glViewport(0, 0, width, height);
	glDisable(GL_LIGHTING);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	unsigned numTriangles = rr2gl_draw_indexed();

	// Allocate the index buffer memory as necessary.
	GLuint* indexBuffer = (GLuint*)malloc(width * height * 4);
	float* trianglePower = (float*)malloc(numTriangles*sizeof(float));

	// Read back the index buffer to memory.
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, indexBuffer);

	// accumulate triangle powers into trianglePower
	for(unsigned i=0;i<numTriangles;i++) trianglePower[i]=0;
	unsigned pixel = 0;
	for(unsigned j=0;j<height;j++)
	for(unsigned i=0;i<width;i++)
	{
		unsigned index = indexBuffer[pixel] >> 8; // alpha was lost
		if(index<numTriangles)
		{
			// modulate by spotmap
			float pixelPower=1; //!!!
			trianglePower[index] += pixelPower;
		}
		else
		{
			assert(0);
		}
		pixel++;
	}

	// debug print
	printf("\n\n");
	for(unsigned i=0;i<numTriangles;i++) printf("%d ",(int)trianglePower[i]);

	// calculate 1 second
	//!!!

	free(trianglePower);
	free(indexBuffer);
	glEnable(GL_LIGHTING);
	glViewport(0, 0, winWidth, winHeight);
}

void
placeSoftLight(int n)
{
  softLight=n;
  static float oldLightAngle,oldLightHeight;
  if(n==-1) { // init, before all
    oldLightAngle=lightAngle;
    oldLightHeight=lightHeight;
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 1);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 90); // no light behind spotlight
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01);
    return;
  }
  if(n==-2) { // done, after all
    lightAngle=oldLightAngle;
    lightHeight=oldLightHeight;
    updateMatrices();
    return;
  }
  // place one point light approximating part of area light
  if(useLights>1) {
    switch(areaType) {
      case 0: // linear
        lightAngle=oldLightAngle+0.2*(n/(useLights-1.)-0.5);
        lightHeight=oldLightHeight-0.4*n/useLights;
        break;
      case 1: // rectangular
        {int q=(int)sqrtf(useLights-1)+1;
        lightAngle=oldLightAngle+0.1*(n/q/(q-1.)-0.5);
        lightHeight=oldLightHeight+(n%q/(q-1.)-0.5);}
        break;
      case 2: // circular
        lightAngle=oldLightAngle+sin(n*2*3.14159/useLights)/20;
        lightHeight=oldLightHeight+cos(n*2*3.14159/useLights)/2;
        break;
    }
    updateMatrices();
  }
  GLfloat ld[3]={-lv[0],-lv[1],-lv[2]};
  glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ld);
}

void
drawEyeViewSoftShadowed(void)
{
  int i;
  int oldAmbientPower=ambientPower;
  placeSoftLight(-1);
  for(i=0;i<useLights;i++)
  {
    placeSoftLight(i);
    glClear(GL_DEPTH_BUFFER_BIT);
    updateDepthMap();
  }
  if(useAccum) {
    glClear(GL_ACCUM_BUFFER_BIT);
  } else {
    globalIntensity=0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupEyeView();
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    setLightIntensity();
    drawScene();
    globalIntensity=1./useLights;
    ambientPower=0;
    glBlendFunc(GL_ONE,GL_ONE);
    glEnable(GL_BLEND);
  }
  for(i=0;i<useLights;i++)
  {
    placeSoftLight(i);
    drawEyeViewShadowed(useAccum);
    if(useAccum) {
      glAccum(GL_ACCUM,1./useLights);
    }
  }
  placeSoftLight(-2);
  if(useAccum) {
    glAccum(GL_RETURN,1);
  } else {  
    glDisable(GL_BLEND);
    globalIntensity=1;
    ambientPower=oldAmbientPower;
  }
}

static void
output(int x, int y, char *string)
{
  int len, i;

  glRasterPos2f(x, y);
  len = (int) strlen(string);
  for (i = 0; i < len; i++) {
    glutBitmapCharacter(font, string[i]);
  }
}

static void
drawHelpMessage(void)
{
  static char *message[] = {
    "Help information",
    "'h'  - shows and dismisses this message",
    "'e'  - show eye view without shadows",
    "'s'  - show eye view WITH shadows",
    "'S'  - show eye view WITH SOFT shadows",
    "'+/-'- soft: increase/decrease number of points",
    "arrow- soft: move camera",
    "'A'  - soft: toggle accumulation buffer",
    "'g'  - soft: cycle through linear, rectangular and circular light",
    "'t'  - show textured view",
    "'w'  - toggle wire frame",
    "'r'  - in textured view, toggle between light z range and projected depth map",
    "'d'  - show the depth map texture",
    "'a'  - toggle between showing MSBs and LSBs in 16-bit mode for",
    "       depth map and textured view",
    "'m'  - toggle whether the left or middle mouse buttons control the eye and",
    "       view positions (helpful for systems with only a two-button mouse)",
    "'o'  - increment object configurations",
    "'O'  - decrement object configurations",
    "'f'  - toggle showing the depth map frustum in dashed lines",
    "'p'  - narrow shadow frustum field of view",
    "'P'  - widen shadow frustum field of view",
    "'n'  - compress shadow frustum near clip plane",
    "'N'  - expand shadow frustum near clip plane",
    "'c'  - compress shadow frustum far clip plane",
    "'C'  - expand shadow frustum far clip plane",
    "'b'  - increment the depth bias for 1st pass glPolygonOffset",
    "'B'  - decrement the depth bias for 1st pass glPolygonOffset",
    "'q'  - increment depth slope for 1st pass glPolygonOffset",
    "'Q'  - increment depth slope for 1st pass glPolygonOffset",
    "",
    "'1' through '5' - use 64x64, 128x128, 256x256, 512x512, or 1024x1024 depth map",
    "",
    "'8'  - toggle between 8-bit and 16-bit depth map precision",
    "      (only works on GeForce, Quadro, and later NVIDIA GPUs)",
    "'9'  - toggle 16-bit and 24-bit depth map precison for hardware shadow mapping",
    "'z'  - toggle zoom in and zoom out",
    "'F1' - toggle hardware shadow mapping",
    "'F2' - toggle quick light move (quater size depth map during light moves)",
    "'F3' - toggle back face culling during depth map construction",
    "'F4' - toggle linear/nearest hardware depth map filtering",
    "'F5' - toggle CopyTexSubImage versus ReadPixels/TexSubImage",
    "'F6' - toggle square versus rectangular texture",
    NULL
  };
  int i;
  int x = 40, y= 42;

  glActiveTextureARB(GL_TEXTURE1_ARB);
  glDisable(GL_TEXTURE_2D);
  glActiveTextureARB(GL_TEXTURE0_ARB);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winWidth, winHeight, 0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glColor4f(0.0,1.0,0.0,0.2);  /* 20% green. */

    /* Drawn clockwise because the flipped Y axis flips CCW and CW. */
    glRecti(winWidth - 30, 30, 30, winHeight - 30);

    glDisable(GL_BLEND);

    glColor3f(1,1,1);
    for(i=0; message[i] != NULL; i++) {
      if (message[i][0] == '\0') {
        y += 7;
      } else {
        output(x, y, message[i]);
        y += 14;
      }
    }

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
}

void
display(void)
{
  if (needTitleUpdate) {
    updateTitle();
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (needMatrixUpdate) {
    updateMatrices();
  }

  switch (drawMode) {
  case DM_EYE_VIEW:
    setupEyeView();
    if (wireFrame) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    drawUnshadowedEyeView();
    break;
  case DM_LIGHT_VIEW:
    setupLightView(0);
    if (wireFrame) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    drawLightView();
    break;
  case DM_DEPTH_MAP:
    /* Wire frame does not apply. */
    drawDepth();
    break;
  case DM_EYE_VIEW_DEPTH_TEXTURED:
    /* Wire frame handled internal to this routine. */
    drawEyeViewDepthTextured();
    break;
  case DM_EYE_VIEW_SHADOWED:
    /* Wire frame handled internal to this routine. */
    drawEyeViewShadowed(1);
    break;
  case DM_EYE_VIEW_SOFTSHADOWED:
    drawEyeViewSoftShadowed();
    break;
  default:
    assert(0);
    break;
  }

  if (wireFrame) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  if (showHelp) {
    drawHelpMessage();
  }
  
  if (!drawFront) {
    glutSwapBuffers();
  }
}

static void
benchmark(int perFrameDepthMapUpdate)
{
  const int numFrames = 150;
  int precision;
  int start, stop;
  float time;
  int i;

  needDepthMapUpdate = 1;
  display();

  printf("starting benchmark...\n");
  glFinish();

  start = glutGet(GLUT_ELAPSED_TIME);
  for (i=0; i<numFrames; i++) {
    if (perFrameDepthMapUpdate) {
      needDepthMapUpdate = 1;
    }
    display();
  }
  glFinish();
  stop = glutGet(GLUT_ELAPSED_TIME);

  time = (stop - start)/1000.0;

  printf("  perFrameDepthMapUpdate=%d, time = %f secs, fps = %f\n",
    perFrameDepthMapUpdate, time, numFrames/time);
  if (useShadowMapSupport) {
    if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
      precision = 16;
    } else {
      precision = 24;
    }
  } else {
    if (depthMapPrecision == GL_UNSIGNED_SHORT) {
      precision = 16;
    } else {
      precision = 8;
    }
  }
  if (useTextureRectangle) {
    printf("  RECT %dx%d:%d using %s\n",
      depthMapRectWidth, depthMapRectHeight, precision,
      useCopyTexImage ? "CopyTexSubImage" : "ReadPixels/TexSubImage");
  } else {
    printf("  TEX2D %dx%d:%d using %s\n",
      depthMapSize, depthMapSize, precision,
      useCopyTexImage ? "CopyTexSubImage" : "ReadPixels/TexSubImage");
  }
  needDepthMapUpdate = 1;
}

void
selectObjectConfig(int item)
{
  objectConfiguration = item;
  needDepthMapUpdate = 1;
  glutPostRedisplay();
}

void
switchMouseControl(void)
{
  if (eyeButton == GLUT_LEFT_BUTTON) {
    eyeButton = GLUT_MIDDLE_BUTTON;
    lightButton = GLUT_LEFT_BUTTON;
  } else {
    lightButton = GLUT_MIDDLE_BUTTON;
    eyeButton = GLUT_LEFT_BUTTON;
  }
  movingEye = 0;
  movingLight = 0;
}

void
toggleWireFrame(void)
{
  wireFrame = !wireFrame;
  if (wireFrame) {
    glClearColor(0.1,0.2,0.2,0);
  } else {
    glClearColor(0,0,0,0);
  }
  glutPostRedisplay();
}

void
toggleHwShadowMapping(void)
{
  if (hasShadowMapSupport) {
    useShadowMapSupport = !useShadowMapSupport;
    needDepthMapUpdate = 1;
    needTitleUpdate = 1;
    glutPostRedisplay();
  }
}

void
toggleQuickLightMove(void)
{
  quickLightMove = !quickLightMove;
  needDepthMapUpdate = 1;
  glutPostRedisplay();
}

void
toggleDepthMapCulling(void)
{
  depthMapBackFaceCulling = !depthMapBackFaceCulling;
  needDepthMapUpdate = 1;
  glutPostRedisplay();
}

void
toggleHwShadowFiltering(void)
{
  if (hwDepthMapFiltering == GL_LINEAR) {
    hwDepthMapFiltering = GL_NEAREST;
  } else {
    hwDepthMapFiltering = GL_LINEAR;
  }
  needTitleUpdate = 1;
  needDepthMapUpdate = 1;
  glutPostRedisplay();
}

void
toggleHwShadowCopyTexImage(void)
{
  if (hasShadowMapSupport) {
    useCopyTexImage = !useCopyTexImage;
    needTitleUpdate = 1;
    needDepthMapUpdate = 1;
    glutPostRedisplay();
  }
}

void
toggleTextureRectangle(void)
{
  if (hasTextureRectangle) {
    useTextureRectangle = !useTextureRectangle;
    needTitleUpdate = 1;
    needDepthMapUpdate = 1;
    glutPostRedisplay();
  }
}


void
updateDepthBias(int delta)
{
  GLfloat scale, bias;

  if (useShadowMapSupport) {
    if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
      depthBias16 += delta;
      scale = slopeScale;
      bias = depthBias16 * depthScale16;
    } else {
      depthBias24 += delta;
      scale = slopeScale;
      bias = depthBias24 * depthScale24;
    }
  } else {
    if (depthMapPrecision == GL_UNSIGNED_SHORT) {
      depthBias16 += delta;
      scale = slopeScale;
      bias = depthBias16 * depthScale16;
    } else {
      depthBias8 += delta;
      scale = slopeScale;
      bias = depthBias8 * depthScale8;
    }
  }
  glPolygonOffset(scale, bias);
  needTitleUpdate = 1;
  needDepthMapUpdate = 1;
}

void
updateDepthMapSize(void)
{
  int oldDepthMapSize = depthMapSize;

  depthMapSize = requestedDepthMapSize;
  while ((winWidth < depthMapSize) || (winHeight < depthMapSize)) {
    depthMapSize >>= 1;  // Half the depth map size
  }
  if (depthMapSize < 1) {
    /* Just in case. */
    depthMapSize = 1;
  }
  if (depthMapSize != requestedDepthMapSize) {
    printf("shadowcast: reducing depth map from %d to %d based on window size %dx%d\n",
      requestedDepthMapSize, depthMapSize, winWidth, winHeight);
  }
  if (!useTextureRectangle) {
    if (oldDepthMapSize != depthMapSize) {
      needDepthMapUpdate = 1;
      needTitleUpdate = 1;
      glutPostRedisplay();
    }
  }
}

void
updateDepthMapRectDimensions(void)
{
  int oldDepthMapWidth = depthMapRectWidth;
  int oldDepthMapHeight = depthMapRectHeight;

  depthMapRectWidth = requestedDepthMapRectWidth;
  if (winWidth < depthMapRectWidth) {
    depthMapRectWidth = winWidth;
  }
  if (depthMapRectWidth < 1) {
    /* Just in case. */
    depthMapRectWidth = 1;
  }
  if (depthMapRectWidth != requestedDepthMapRectWidth) {
    printf("shadowcast: reducing depth map width from %d to %d based on window size %dx%d\n",
      requestedDepthMapRectWidth, depthMapRectWidth, winWidth, winHeight);
  }

  depthMapRectHeight = requestedDepthMapRectHeight;
  if (winHeight < depthMapRectHeight) {
    depthMapRectHeight = winHeight;
  }
  if (depthMapRectHeight < 1) {
    /* Just in case. */
    depthMapRectHeight = 1;
  }
  if (depthMapRectHeight != requestedDepthMapRectHeight) {
    printf("shadowcast: reducing depth map height from %d to %d based on window size %dx%d\n",
      requestedDepthMapRectHeight, depthMapRectHeight, winWidth, winHeight);
  }

  if (useTextureRectangle) {
    if ((oldDepthMapWidth != depthMapRectWidth) ||
        (oldDepthMapHeight != depthMapRectHeight)) {
      needDepthMapUpdate = 1;
      needTitleUpdate = 1;
      glutPostRedisplay();
    }
  }
}

void
selectMenu(int item)
{
  switch (item) {
  case ME_EYE_VIEW:
    drawMode = DM_EYE_VIEW;
    needTitleUpdate = 1;
    break;
  case ME_EYE_VIEW_SHADOWED:
    drawMode = DM_EYE_VIEW_SHADOWED;
    needTitleUpdate = 1;
    break;
  case ME_EYE_VIEW_SOFTSHADOWED:
    drawMode = DM_EYE_VIEW_SOFTSHADOWED;
    needTitleUpdate = 1;
    break;
  case ME_LIGHT_VIEW:
    drawMode = DM_LIGHT_VIEW;
    needTitleUpdate = 1;
    break;
  case ME_DEPTH_MAP:
    drawMode = DM_DEPTH_MAP;
    needTitleUpdate = 1;
    break;
  case ME_EYE_VIEW_TEXTURE_DEPTH_MAP:
    drawMode = DM_EYE_VIEW_DEPTH_TEXTURED;
    rangeMap = 0;
    needTitleUpdate = 1;
    break;
  case ME_EYE_VIEW_TEXTURE_LIGHT_Z:
    drawMode = DM_EYE_VIEW_DEPTH_TEXTURED;
    rangeMap = 1;
    needTitleUpdate = 1;
    break;
  case ME_SWITCH_MOUSE_CONTROL:
    switchMouseControl();
    return;  /* No redisplay needed. */
   
  case ME_TOGGLE_WIRE_FRAME:
    toggleWireFrame();
    return;
  case ME_TOGGLE_HW_SHADOW_MAPPING:
    toggleHwShadowMapping();
    return;
  case ME_TOGGLE_DEPTH_MAP_CULLING:
    toggleDepthMapCulling();
    return;
  case ME_TOGGLE_HW_SHADOW_FILTERING:
    toggleHwShadowFiltering();
    return;
  case ME_TOGGLE_HW_SHADOW_COPY_TEX_IMAGE:
    toggleHwShadowCopyTexImage();
    return;
  case ME_TOGGLE_QUICK_LIGHT_MOVE:
    toggleQuickLightMove();
    return;

  case ME_TOGGLE_LIGHT_FRUSTUM:
    showLightViewFrustum = !showLightViewFrustum;
    if (showLightViewFrustum) {
      needMatrixUpdate = 1;
    }
    break;
  case ME_ON_LIGHT_FRUSTUM:
    if (!showLightViewFrustum) {
      needMatrixUpdate = 1;
    }
    showLightViewFrustum = 1;
    break;
  case ME_OFF_LIGHT_FRUSTUM:
    showLightViewFrustum = 0;
    break;
  case ME_DEPTH_MAP_64:
    requestedDepthMapSize = 64;
    updateDepthMapSize();
    return;
  case ME_DEPTH_MAP_128:
    requestedDepthMapSize = 128;
    updateDepthMapSize();
    return;
  case ME_DEPTH_MAP_256:
    requestedDepthMapSize = 256;
    updateDepthMapSize();
    return;
  case ME_DEPTH_MAP_512:
    requestedDepthMapSize = 512;
    updateDepthMapSize();
    return;
  case ME_DEPTH_MAP_1024:
    requestedDepthMapSize = 1024;
    updateDepthMapSize();
    return;
  case ME_PRECISION_HW_16BIT:
    if (hasShadowMapSupport) {
      hwDepthMapInternalFormat = GL_DEPTH_COMPONENT16_SGIX;
      hwDepthMapPrecision = GL_UNSIGNED_SHORT;
      updateDepthBias(0);
    }
    break;
  case ME_PRECISION_HW_24BIT:
    if (hasShadowMapSupport) {
      hwDepthMapInternalFormat = GL_DEPTH_COMPONENT24_SGIX;
      hwDepthMapPrecision = GL_UNSIGNED_INT;
      updateDepthBias(0);
    }
    break;
  case ME_PRECISION_DT_8BIT:
    depthMapFormat = GL_LUMINANCE;
    depthMapInternalFormat = GL_INTENSITY8;
    depthMapPrecision = GL_UNSIGNED_BYTE;
    updateDepthBias(0);
    break;
  case ME_PRECISION_DT_16BIT:
    if (hasRegisterCombiners) {
      depthMapFormat = GL_LUMINANCE_ALPHA;
      depthMapInternalFormat = GL_LUMINANCE8_ALPHA8;
      depthMapPrecision = GL_UNSIGNED_SHORT;
      updateDepthBias(0);
    } else {
      printf("shadowcast: "
        "16-bit precision depth map requires NV_register_combiners\n");
    }
    break;
  case ME_EXIT:
    exit(0);
    break;
  default:
    assert(0);
    break;
  }
  glutPostRedisplay();
}

void
special(int c, int x, int y)
{
  switch (c) {
  case GLUT_KEY_F1:
    toggleHwShadowMapping();
    return;
  case GLUT_KEY_F2:
    toggleQuickLightMove();
    return;
  case GLUT_KEY_F3:
    toggleDepthMapCulling();
    return;
  case GLUT_KEY_F4:
    toggleHwShadowFiltering();
    return;
  case GLUT_KEY_F5:
    toggleHwShadowCopyTexImage();
    return;
  case GLUT_KEY_F6:
    toggleTextureRectangle();
    return;
  case GLUT_KEY_F7:
    benchmark(1);
    return;
  case GLUT_KEY_F8:
    benchmark(0);
    return;
  case GLUT_KEY_F9:
    capturePrimary();
    return;
  }
  if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
    switch (c) {
    case GLUT_KEY_UP:
      requestedDepthMapRectHeight += 15;
      if (requestedDepthMapRectHeight > maxRectangleTextureSize) {
        requestedDepthMapRectHeight = maxRectangleTextureSize;
      }
      updateDepthMapRectDimensions();
      break;
    case GLUT_KEY_DOWN:
      requestedDepthMapRectHeight -= 15;
      if (requestedDepthMapRectHeight < 1) {
        requestedDepthMapRectHeight = 1;
      }
      updateDepthMapRectDimensions();
      break;
    case GLUT_KEY_LEFT:
      requestedDepthMapRectWidth -= 15;
      if (requestedDepthMapRectWidth < 1) {
        requestedDepthMapRectWidth = 1;
      }
      updateDepthMapRectDimensions();
      break;
    case GLUT_KEY_RIGHT:
      requestedDepthMapRectWidth += 15;
      if (requestedDepthMapRectWidth > maxRectangleTextureSize) {
        requestedDepthMapRectWidth = maxRectangleTextureSize;
      }
      updateDepthMapRectDimensions();
      break;
    }
    return;
  }
  switch (objectConfiguration) {
  case OC_MGF:
    switch (c) {
    case GLUT_KEY_UP:
      for(int i=0;i<3;i++) eye_shift[i]+=ed[i]/20;
      break;
    case GLUT_KEY_DOWN:
      for(int i=0;i<3;i++) eye_shift[i]-=ed[i]/20;
      break;
    case GLUT_KEY_LEFT:
      eye_shift[0]+=ed[2]/20;
      eye_shift[2]-=ed[0]/20;
      //eye_shift[2]+=ed[1]/20;
      break;
    case GLUT_KEY_RIGHT:
      eye_shift[0]-=ed[2]/20;
      eye_shift[2]+=ed[0]/20;
      //eye_shift[2]+=ed[1]/20;
      break;
    default:
      return;
    }
    break;
  default:
    return;
  }
  glutPostRedisplay();
}

void
keyboard(unsigned char c, int x, int y)
{
  switch (c) {
  case 27:
    exit(0);
    break;
  case 'b':
    updateDepthBias(+1);
    break;
  case 'B':
    updateDepthBias(-1);
    break;
  case 'e':
  case 'E':
    drawMode = DM_EYE_VIEW;
    needTitleUpdate = 1;
    break;
  case 'l':
  case 'L':
    drawMode = DM_LIGHT_VIEW;
    needTitleUpdate = 1;
    break;
  case 'm':
    switchMouseControl();
    break;
  case 'h':
  case 'H':
    showHelp = !showHelp;
    break;
  case 'f':
  case 'F':
    showLightViewFrustum = !showLightViewFrustum;
    if (showLightViewFrustum) {
      needMatrixUpdate = 1;
    }
    break;
  case 'z':
  case 'Z':
    eyeFieldOfView = (eyeFieldOfView == 40.0) ? 20.0 : 40.0;
    buildPerspectiveMatrix(eyeFrustumMatrix,
      eyeFieldOfView, 1.0/winAspectRatio, eyeNear, eyeFar);
    break;
  case 'a':
    showDepthMapMSBs = !showDepthMapMSBs;
    needTitleUpdate = 1;
    break;
  case 'A':
    useAccum=1-useAccum;
    break;
  case 'q':
    slopeScale += 0.1;
    needDepthMapUpdate = 1;
    needTitleUpdate = 1;
    updateDepthBias(0);
    break;
  case 'Q':
    slopeScale -= 0.1;
    if (slopeScale < 0.0) {
      slopeScale = 0.0;
    }
    needDepthMapUpdate = 1;
    needTitleUpdate = 1;
    updateDepthBias(0);
    break;
  case 'd':
    drawMode = DM_DEPTH_MAP;
    needTitleUpdate = 1;
    break;
  case 'M':
    mipmapShadowMap = !mipmapShadowMap;
    break;
  case '>':
    textureLodBias += 0.2;
    break;
  case '<':
    textureLodBias -= 0.2;
    break;
  case 'D':
    if (lightDimColor[0] == 0.0) {
      lightDimColor[0] = LIGHT_DIMMING;
      lightDimColor[1] = LIGHT_DIMMING;
      lightDimColor[2] = LIGHT_DIMMING;
      lightDimColor[3] = LIGHT_DIMMING;
    } else {
      lightDimColor[0] = 0.0;
      lightDimColor[1] = 0.0;
      lightDimColor[2] = 0.0;
      lightDimColor[3] = 0.0;
    }
    break;
  case 'w':
  case 'W':
    toggleWireFrame();
    return;
  case 't':
  case 'T':
    drawMode = DM_EYE_VIEW_DEPTH_TEXTURED;
    needTitleUpdate = 1;
    break;
  case 'n':
    lightNear *= 0.8;
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 'N':
    lightNear /= 0.8;
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 'c':
    lightFar *= 1.2;
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 'C':
    lightFar /= 1.2;
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 'p':
    lightFieldOfView -= 5.0;
    if (lightFieldOfView < 5.0) {
      lightFieldOfView = 5.0;
    }
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 'P':
    lightFieldOfView += 5.0;
    if (lightFieldOfView > 160.0) {
      lightFieldOfView = 160.0;
    }
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    break;
  case 's':
    drawMode = DM_EYE_VIEW_SHADOWED;
    needTitleUpdate = 1;
    break;
  case 'S':
    drawMode = DM_EYE_VIEW_SOFTSHADOWED;
    needTitleUpdate = 1;
    break;
  case 'r':
  case 'R':
    rangeMap = !rangeMap;
    needTitleUpdate = 1;
    break;
  case 'o':
    objectConfiguration = (objectConfiguration + 1) % NUM_OF_OCS;
    glutIdleFunc(NULL);
    needDepthMapUpdate = 1;
    break;
  case 'O':
    objectConfiguration = objectConfiguration - 1;
    if (objectConfiguration < 0) {
      objectConfiguration = NUM_OF_OCS-1;
    }
    glutIdleFunc(NULL);
    needDepthMapUpdate = 1;
    break;
  case '+':
    useLights++;
    needDepthMapUpdate = 1;
    break;
  case '-':
    if(useLights>1) {
      useLights--;
      needDepthMapUpdate = 1;
    }
    break;
  case 'g':
    ++areaType%=3;
    needDepthMapUpdate = 1;
    break;
  case '1':
    requestedDepthMapSize = 64;
    updateDepthMapSize();
    return;
  case '2':
    requestedDepthMapSize = 128;
    updateDepthMapSize();
    return;
  case '3':
    requestedDepthMapSize = 256;
    updateDepthMapSize();
    return;
  case '4':
    requestedDepthMapSize = 512;
    updateDepthMapSize();
    return;
  case '5':
    requestedDepthMapSize = 1024;
    updateDepthMapSize();
    return;
  case '8':
    if (hasRegisterCombiners) {
      if (depthMapPrecision == GL_UNSIGNED_SHORT) {
        depthMapFormat = GL_LUMINANCE;
        depthMapInternalFormat = GL_INTENSITY8;
        depthMapPrecision = GL_UNSIGNED_BYTE;
      } else {
        depthMapFormat = GL_LUMINANCE_ALPHA;
        depthMapInternalFormat = GL_LUMINANCE8_ALPHA8;
        depthMapPrecision = GL_UNSIGNED_SHORT;
      }
      updateDepthBias(0);
    } else {
      printf("shadowcast: "
             "16-bit precision depth map requires NV_register_combiners\n");
    }
    break;
  case '9':
    if (hasShadowMapSupport) {
      if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
        hwDepthMapInternalFormat = GL_DEPTH_COMPONENT24_SGIX;
        hwDepthMapPrecision = GL_UNSIGNED_INT;
      } else {
        hwDepthMapInternalFormat = GL_DEPTH_COMPONENT16_SGIX;
        hwDepthMapPrecision = GL_UNSIGNED_SHORT;
      }
      updateDepthBias(0);
    } else {
      printf("shadowcast: "
             "hardware shadow mapping requires SGIX_shadow and SGIX_depth_texture\n");
    }
    break;
  default:
    return;
  }
  glutPostRedisplay();
}

void
updateDepthScale(void)
{
  GLint depthBits;

  glGetIntegerv(GL_DEPTH_BITS, &depthBits);
  if (depthBits < 24) {
    depthScale24 = 1;
  } else {
    depthScale24 = 1 << (depthBits - 24);
  }
  if (depthBits < 16) {
    depthScale16 = 1;
  } else {
    depthScale16 = 1 << (depthBits - 16);
  }
  if (depthBits < 8) {
    depthScale8 = 1;
  } else {
    depthScale8 = 1 << (depthBits - 8);
  }
  needDepthMapUpdate = 1;
}

void
reshape(int w, int h)
{
  winWidth = w;
  winHeight = h;
  glViewport(0, 0, w, h);
  winAspectRatio = (double) winHeight / (double) winWidth;
  buildPerspectiveMatrix(eyeFrustumMatrix,
    eyeFieldOfView, 1.0/winAspectRatio, eyeNear, eyeFar);

  /* Perhaps there might have been a mode change so at window
     reshape time, redetermine the depth scale. */
  updateDepthScale();

  if (useTextureRectangle) {
    updateDepthMapRectDimensions();
  } else {
    updateDepthMapSize();
  }
}

void
initGL(void)
{
  GLfloat globalAmbient[] = {0.5, 0.5, 0.5, 1.0};
  GLubyte texmap[256*256*2];
  unsigned int i, j;
  GLint depthBits;

#if defined(_WIN32)
  if (hasSwapControl) {
    if (vsync) {
      wglSwapIntervalEXT(1);
    } else {
      wglSwapIntervalEXT(0);
    }
  }
#endif

  glGetIntegerv(GL_DEPTH_BITS, &depthBits);
  printf("depth buffer precision = %d\n", depthBits);
  if (depthBits >= 24) {
    hwDepthMapPrecision = GL_UNSIGNED_INT;
    hwDepthMapInternalFormat = GL_DEPTH_COMPONENT24_SGIX;
  }

  glClearStencil(0);
  glClearColor(0,0,0,0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);

  glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
  glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
  glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
  glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

  /* Make 8-bit identity texture that maps (s)=(z) to [0,255]/255. */
  for (i=0; i<256; i++) {
    texmap[i] = i;
  }
  glBindTexture(GL_TEXTURE_1D, TO_MAP_8BIT);
  glTexImage1D(GL_TEXTURE_1D, 0, GL_INTENSITY8,
    256, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, texmap);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  /* Make 16-bit identity texture that maps (s,t)=(z,z*256) to [0,65535]/65535. */
  for (j=0; j<256; j++) {
    for (i=0; i<256; i++) {
      texmap[j*512+i*2+0] = j;  /* Luminance has least sigificant bits. */
      texmap[j*512+i*2+1] = i;  /* Alpha has most sigificant bits. */
    }
  }
  glBindTexture(GL_TEXTURE_2D, TO_MAP_16BIT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8_ALPHA8,
    256, 256, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, texmap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  /* GL_LEQUAL ensures that when fragments with equal depth are
     generated within a single rendering pass, the last fragment
     results. */
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  setLightIntensity();

  glLineStipple(1, 0xf0f0);

  glMaterialf(GL_FRONT, GL_SHININESS, 30.0);

  glEnable(GL_CULL_FACE);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
#if 000
  /* This would work too. */
  glEnable(GL_RESCALE_NORMAL_EXT);
#else
  glEnable(GL_NORMALIZE);
#endif

  q = gluNewQuadric();
  if (useDisplayLists) {

    /* Make a sphere display list. */
    glNewList(DL_SPHERE, GL_COMPILE);
    drawSphere();
    glEndList();
  }

  if (hasRegisterCombiners) {
    depthMapFormat = GL_LUMINANCE_ALPHA;
    depthMapInternalFormat = GL_LUMINANCE8_ALPHA8;
    depthMapPrecision = GL_UNSIGNED_SHORT;
  }
  updateDepthScale();
  updateDepthBias(0);  /* Update with no offset change. */

  if (drawFront) {
    glDrawBuffer(GL_FRONT);
    glReadBuffer(GL_FRONT);
  }
}

/*** LOAD TEXTURE IMAGES ***/

gliGenericImage *
readImage(char *filename)
{
  FILE *file;
  gliGenericImage *image;

  file = fopen(filename, "rb");
  if (file == NULL) {
    printf("shadowcast: could not open \"%s\"\n", filename);
    return NULL;
  }
  image = gliReadTGA(file, filename);
  fclose(file);
  if (image == NULL) {
    printf("shadowcast: could not decode file format of \"%s\"\n", filename);
    return NULL;
  }
  return image;
}

int
loadTextureDecalImage(char *filename, int mipmaps)
{
  gliGenericImage *image;

  image = readImage(filename);
  if (image == NULL) {
    printf("shadowcast: failed to read \"%s\" texture.\n", filename);
    return 0;
  }
  if (image->format == GL_COLOR_INDEX) {
    /* Rambo 8-bit color index into luminance. */
    image->format = GL_LUMINANCE;
  }
  if (mipmaps) {
    gluBuild2DMipmaps(GL_TEXTURE_2D, image->components,
      image->width, image->height,
      image->format, GL_UNSIGNED_BYTE, image->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
      GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, image->components,
      image->width, image->height, 0,
      image->format, GL_UNSIGNED_BYTE, image->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  return 1;
}

void
loadTextures(void)
{
  /* Assume tightly packed textures. */
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glBindTexture(GL_TEXTURE_2D, TO_SPOT);
  textureSpot = loadTextureDecalImage("spot0.tga", 1);
  useBestShadowMapClamping(GL_TEXTURE_2D);
}

void
mouse(int button, int state, int x, int y)
{
  if (button == eyeButton && state == GLUT_DOWN) {
    movingEye = 1;
    xEyeBegin = x;
    yEyeBegin = y;
  }
  if (button == eyeButton && state == GLUT_UP) {
    movingEye = 0;
  }
  if (button == lightButton && state == GLUT_DOWN) {
    movingLight = 1;
    xLightBegin = x;
    yLightBegin = y;
  }
  if (button == lightButton && state == GLUT_UP) {
    movingLight = 0;
    needDepthMapUpdate = 1;
    glutPostRedisplay();
  }
}

void
motion(int x, int y)
{
  if (movingEye) {
    eyeAngle = eyeAngle - 0.005*(x - xEyeBegin);
    eyeHeight = eyeHeight + 0.15*(y - yEyeBegin);
    if (eyeHeight > 20.0) eyeHeight = 20.0;
    if (eyeHeight < -12.0) eyeHeight = -12.0;
    xEyeBegin = x;
    yEyeBegin = y;
    needMatrixUpdate = 1;
    glutPostRedisplay();
  }
  if (movingLight) {
    lightAngle = lightAngle + 0.005*(x - xLightBegin);
    lightHeight = lightHeight - 0.15*(y - yLightBegin);
    if (lightHeight > 12.0) lightHeight = 12.0;
    if (lightHeight < -4.0) lightHeight = -4.0;
    xLightBegin = x;
    yLightBegin = y;
    needMatrixUpdate = 1;
    needDepthMapUpdate = 1;
    glutPostRedisplay();
  }
}

void
depthBiasSelect(int depthBiasOption)
{
  GLfloat scale, bias;

  if (useShadowMapSupport) {
    if (hwDepthMapPrecision == GL_UNSIGNED_SHORT) {
      depthBias16 = depthBiasOption;
      scale = slopeScale;
      bias = depthBias16 * depthScale16;
    } else {
      depthBias24 = depthBiasOption;
      scale = slopeScale;
      bias = depthBias24 * depthScale24;
    }
  } else {
    if (depthMapPrecision == GL_UNSIGNED_SHORT) {
      depthBias16 = depthBiasOption;
      scale = slopeScale;
      bias = depthBias16 * depthScale16;
    } else {
      depthBias8 = depthBiasOption;
      scale = slopeScale;
      bias = depthBias8 * depthScale8;
    }
  }
  glPolygonOffset(scale, bias);
  needTitleUpdate = 1;
  needDepthMapUpdate = 1;
  glutPostRedisplay();
}

void
initMenus(void)
{
  int viewMenu, frustumMenu, objectConfigMenu,
      dualTexturePrecisionMenu, hardwarePrecisionMenu,
      depthMapMenu, depthBiasMenu;

  if (!fullscreen) {
    viewMenu = glutCreateMenu(selectMenu);
    glutAddMenuEntry("[s] Eye view with shadows", ME_EYE_VIEW_SHADOWED);
    glutAddMenuEntry("[S] Eye view with soft shadows", ME_EYE_VIEW_SOFTSHADOWED);
    glutAddMenuEntry("[e] Eye view, no shadows", ME_EYE_VIEW);
    glutAddMenuEntry("[l] Light view", ME_LIGHT_VIEW);
    glutAddMenuEntry("[d] Show depth map", ME_DEPTH_MAP);
    glutAddMenuEntry("Eye view with projected depth map",
      ME_EYE_VIEW_TEXTURE_DEPTH_MAP);
    glutAddMenuEntry("Eye view with projected light Z",
      ME_EYE_VIEW_TEXTURE_LIGHT_Z);

    frustumMenu = glutCreateMenu(selectMenu);
    glutAddMenuEntry("[f] Toggle", ME_TOGGLE_LIGHT_FRUSTUM);
    glutAddMenuEntry("Show", ME_ON_LIGHT_FRUSTUM);
    glutAddMenuEntry("Hide", ME_OFF_LIGHT_FRUSTUM);

    objectConfigMenu = glutCreateMenu(selectObjectConfig);
    glutAddMenuEntry("3D grid of spheres", OC_SPHERE_GRID);
    glutAddMenuEntry("NVIDIA logo", OC_NVIDIA_LOGO);
    glutAddMenuEntry("Weird helix", OC_WEIRD_HELIX);
    glutAddMenuEntry("Logo and helix", OC_COMBO);
    glutAddMenuEntry("Simple", OC_SIMPLE);
    glutAddMenuEntry("Blue pony with simple", OC_BLUE_PONY);
    glutAddMenuEntry("MGF", OC_MGF);

    dualTexturePrecisionMenu = glutCreateMenu(selectMenu);
    glutAddMenuEntry("8-bit", ME_PRECISION_DT_8BIT);
    if (hasRegisterCombiners) {
      glutAddMenuEntry("16-bit", ME_PRECISION_DT_16BIT);
    }

    if (hasShadowMapSupport) {
      hardwarePrecisionMenu = glutCreateMenu(selectMenu);
      glutAddMenuEntry("16-bit", ME_PRECISION_HW_16BIT);
      glutAddMenuEntry("24-bit", ME_PRECISION_HW_24BIT);
    }

    depthMapMenu = glutCreateMenu(selectMenu);
    glutAddMenuEntry("[1] 64x64", ME_DEPTH_MAP_64);
    glutAddMenuEntry("[2] 128x128", ME_DEPTH_MAP_128);
    glutAddMenuEntry("[3] 256x256", ME_DEPTH_MAP_256);
    glutAddMenuEntry("[4] 512x512", ME_DEPTH_MAP_512);
    glutAddMenuEntry("[5] 1024x1024", ME_DEPTH_MAP_1024);

    depthBiasMenu = glutCreateMenu(depthBiasSelect);
    glutAddMenuEntry("2", 2);
    glutAddMenuEntry("4", 4);
    glutAddMenuEntry("6", 6);
    glutAddMenuEntry("8", 8);
    glutAddMenuEntry("10", 10);
    glutAddMenuEntry("100", 100);
    glutAddMenuEntry("512", 512);
    glutAddMenuEntry("0", 0);

    glutCreateMenu(selectMenu);
    glutAddSubMenu("View", viewMenu);
    glutAddSubMenu("Object configuration", objectConfigMenu);
    glutAddSubMenu("Light frustum", frustumMenu);
    glutAddSubMenu("Depth map resolution", depthMapMenu);
    glutAddSubMenu("Dual-texture depth map precision", dualTexturePrecisionMenu);
    if (hasShadowMapSupport) {
      glutAddSubMenu("Hardware depth map precision", hardwarePrecisionMenu);
    }
    glutAddSubMenu("Shadow depth bias", depthBiasMenu);
    glutAddMenuEntry("[m] Switch mouse control", ME_SWITCH_MOUSE_CONTROL);
    glutAddMenuEntry("[w] Toggle wire frame", ME_TOGGLE_WIRE_FRAME);
    if (hasShadowMapSupport) {
      glutAddMenuEntry("[F1] Toggle hardware shadow mapping", ME_TOGGLE_HW_SHADOW_MAPPING);
    }
    glutAddMenuEntry("[F2] Toggle quick light move", ME_TOGGLE_QUICK_LIGHT_MOVE);
    glutAddMenuEntry("[F3] Toggle depth map back face culling", ME_TOGGLE_DEPTH_MAP_CULLING);
    if (hasShadowMapSupport) {
      glutAddMenuEntry("[F4] Toggle hardware shadow filtering", ME_TOGGLE_HW_SHADOW_FILTERING);
      glutAddMenuEntry("[F5] Toggle hardware shadow CopyTexImage", ME_TOGGLE_HW_SHADOW_COPY_TEX_IMAGE);
    }
    glutAddMenuEntry("[ESC] Quit", ME_EXIT);

    glutAttachMenu(GLUT_RIGHT_BUTTON);
  }
}

void
parseOptions(int argc, char **argv)
{
  int i;

  for (i=1; i<argc; i++) {
    if (!strcmp("-fullscreen", argv[i])) {
      fullscreen = 1;
    }
    if (!strcmp("-force_envcombine", argv[i])) {
      forceEnvCombine = 1;
    }
    if (!strcmp("-nodlist", argv[i])) {
      useDisplayLists = 0;
    }
    if (strstr(argv[i], ".mgf")) {
      mgf_filename = argv[i];
    }
    // -- end of ldmgf2.cpp params --
    if (!strcmp("-depth24", argv[i])) {
      useDepth24 = 1;
    }
    if (!strcmp("-stencil", argv[i])) {
      useStencil = 1;
    }
    if (!strcmp("-novsync", argv[i])) {
      vsync = 0;
    }
    if (!strcmp("-rc_debug", argv[i])) {
      rcDebug = 1;
    }
    if (!strcmp("-swapendian", argv[i])) {
      littleEndian = !littleEndian;
      printf("swap endian requested, assuming %s-endian\n",
        littleEndian ? "little" : "big");
    }
    if (!strcmp("-front", argv[i])) {
      drawFront = 1;
    }
    if (!strcmp("-v", argv[i])) {
      gliVerbose(1);
    }
  }
}

void
setLittleEndian(void)
{
  unsigned short shortValue = 258;
  unsigned char *byteVersion = (unsigned char*) &shortValue;

  if (byteVersion[1] == 1) {
    assert(byteVersion[0] == 2);
    hwLittleEndian = 1;
  } else {
    assert(byteVersion[0] == 1);
    assert(byteVersion[1] == 2);
    hwLittleEndian = 0;
  }
  littleEndian = hwLittleEndian;
}

int
main(int argc, char **argv)
{
  setLittleEndian();

  glutInitWindowSize(800, 600);
  glutInit(&argc, argv);
  parseOptions(argc, argv);

  // load mgf
  rrVision::RRObjectImporter* rrobject = new_mgf_importer(mgf_filename);

  if (useStencil) {
    if (useDepth24) {
      glutInitDisplayString("depth~24 rgb stencil double");
    } else {
      glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM | GLUT_STENCIL);
    }
  } else {
    if (useDepth24) {
      glutInitDisplayString("depth~24 rgb double");
    } else {
      glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
    }
  }
  if (fullscreen) {
    glutGameModeString("800x600:32");
    glutEnterGameMode();
  } else {
    glutCreateWindow("shadowcast");
  }
  glutDisplayFunc(display);
  glutKeyboardFunc(keyboard);
  glutSpecialFunc(special);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);

  initExtensions();

  /* Menu initialization depends on knowing what extensions are
     supported. */
  initMenus();

  initGL();
  loadTextures();
  rr2gl_load(rrobject);

  glutMainLoop();
  return 0;
}
