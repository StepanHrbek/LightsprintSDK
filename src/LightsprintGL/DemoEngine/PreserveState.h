// --------------------------------------------------------------------------
// DemoEngine
// Preserving GL state.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef PRESERVESTATE_H
#define PRESERVESTATE_H

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
//   if(done)
//     return; // <- viewport restored here
//   more stuff...
// } // <- viewport restored here

DECLARE_PRESERVE_STATE( PreserveViewport  ,int viewport[4]        ,glGetIntegerv(GL_VIEWPORT,viewport)         ,glViewport(viewport[0],viewport[1],viewport[2],viewport[3]));
DECLARE_PRESERVE_STATE( PreserveClearColor,float clearcolor[4]    ,glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor),glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]));
DECLARE_PRESERVE_STATE( PreserveDepthTest ,GLboolean depthTest    ,depthTest=glIsEnabled(GL_DEPTH_TEST)        ,if(depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST));
DECLARE_PRESERVE_STATE( PreserveDepthMask ,unsigned char depthMask,glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask),glDepthMask(depthMask));
DECLARE_PRESERVE_STATE( PreserveCullFace  ,GLboolean cullFace     ,cullFace=glIsEnabled(GL_CULL_FACE)          ,if(cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE));
DECLARE_PRESERVE_STATE( PreserveCullMode  ,int cullMode           ,glGetIntegerv(GL_CULL_FACE_MODE,&cullMode)  ,glCullFace(cullMode));
DECLARE_PRESERVE_STATE( PreserveBlend     ,GLboolean blend        ,blend=glIsEnabled(GL_BLEND)                 ,if(blend) glEnable(GL_BLEND); else glDisable(GL_BLEND));

#undef DECLARE_PRESERVE_STATE

#endif
