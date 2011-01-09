// --------------------------------------------------------------------------
// Preserving GL state.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef PRESERVESTATE_H
#define PRESERVESTATE_H

#include <Lightsprint/GL/FBO.h>

namespace rr_gl
{

#define DECLARE_PRESERVE_STATE(name,storage,get,set) class name {public: storage; name(){get;} ~name(){set;} };

// Rules for GL states:
//
// - LightsprintGL modifies GL state.
// - Some states are modified but original value is preserved
//   and restored back automatically:
//   - matrices
//   - culling
//   - viewport
// - Some states are modified and left modified:
//   - vertex attributes: color, secondary color, normal, texcoords
//   - program
//   - textures

// Example of use:
// {
//   PreserveViewport pv; // <- backup viewport
//   temporary changes to viewport...
//   if (done)
//     return; // <- viewport restored here
//   more stuff...
// } // <- viewport restored here

DECLARE_PRESERVE_STATE( PreserveViewport  ,GLint viewport[4]      ,glGetIntegerv(GL_VIEWPORT,viewport)         ,glViewport(viewport[0],viewport[1],viewport[2],viewport[3]));
DECLARE_PRESERVE_STATE( PreserveClearColor,GLfloat clearcolor[4]  ,glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor),glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]));
DECLARE_PRESERVE_STATE( PreserveDepthTest ,GLboolean depthTest    ,depthTest=glIsEnabled(GL_DEPTH_TEST)        ,if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST));
DECLARE_PRESERVE_STATE( PreserveDepthMask ,GLboolean depthMask    ,glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask),glDepthMask(depthMask));
DECLARE_PRESERVE_STATE( PreserveColorMask ,GLboolean colorMask[4] ,glGetBooleanv(GL_COLOR_WRITEMASK,colorMask) ,glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]));
DECLARE_PRESERVE_STATE( PreserveCullFace  ,GLboolean cullFace     ,cullFace=glIsEnabled(GL_CULL_FACE)          ,if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE));
DECLARE_PRESERVE_STATE( PreserveCullMode  ,GLint cullMode         ,glGetIntegerv(GL_CULL_FACE_MODE,&cullMode)  ,glCullFace(cullMode));
DECLARE_PRESERVE_STATE( PreserveBlend     ,GLboolean blend        ,blend=glIsEnabled(GL_BLEND)                 ,if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND));
DECLARE_PRESERVE_STATE( PreserveBlendFunc ,GLint src;GLint dst    ,glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst) ,glBlendFunc(src,dst));
DECLARE_PRESERVE_STATE( PreserveAlphaTest ,GLboolean test         ,test=glIsEnabled(GL_ALPHA_TEST)             ,if (test) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST));
DECLARE_PRESERVE_STATE( PreserveAlphaFunc ,GLint func;GLfloat ref ,glGetIntegerv(GL_ALPHA_TEST_FUNC,&func);glGetFloatv(GL_ALPHA_TEST_REF,&ref) ,glAlphaFunc(func,ref));
// current matrix only
DECLARE_PRESERVE_STATE( PreserveMatrix    ,                       ,glPushMatrix()                              ,glPopMatrix(););
// projection and modelview matrices only
DECLARE_PRESERVE_STATE( PreserveMatrices  ,GLint matrixMode       ,glGetIntegerv(GL_MATRIX_MODE,&matrixMode);glMatrixMode(GL_PROJECTION);glPushMatrix();glMatrixMode(GL_MODELVIEW);glPushMatrix(), glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(matrixMode););
DECLARE_PRESERVE_STATE( PreserveFBO       ,FBO state              ,state=FBO::getState()                       ,state.restore());

#undef DECLARE_PRESERVE_STATE

}; // namespace

#endif
