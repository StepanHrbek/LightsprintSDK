//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PreserveState.h
//! \brief LightsprintGL | helpers for preserving OpenGL state
//----------------------------------------------------------------------------

#ifndef PRESERVESTATE_H
#define PRESERVESTATE_H

#include <Lightsprint/GL/FBO.h>

namespace rr_gl
{

#define DECLARE_PRESERVE_STATE(name,storage,get,set) class name {public: storage; name(){get;} ~name(){set;} };

// Rules for GL states:
//
// - LightsprintGL functions modify GL state.
// - Some states are modified but original value is preserved
//   and restored back automatically:
//   - blending
//   - culling
//   - viewport
//   - scissors
//   - FBO etc
// - Some states are modified and left modified:
//   - program
//   - textures etc

// Example of use:
// {
//   PreserveViewport pv; // <- backup viewport
//   temporary changes to viewport...
//   if (done)
//     return; // <- viewport restored here
//   more stuff...
// } // <- viewport restored here

DECLARE_PRESERVE_STATE( PreserveViewport  ,GLint viewport[4]      ,glGetIntegerv(GL_VIEWPORT,viewport)         ,glViewport(viewport[0],viewport[1],viewport[2],viewport[3]));
DECLARE_PRESERVE_STATE( PreserveScissor   ,GLint scissor[4]       ,glGetIntegerv(GL_SCISSOR_BOX,scissor)       ,glScissor(scissor[0],scissor[1],scissor[2],scissor[3]));
DECLARE_PRESERVE_STATE( PreserveClearColor,GLfloat clearcolor[4]  ,glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor),glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]));
DECLARE_PRESERVE_STATE( PreserveDepthTest ,GLboolean depthTest    ,depthTest=glIsEnabled(GL_DEPTH_TEST)        ,if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST));
//DECLARE_PRESERVE_STATE( PreserveDepthFunc ,GLint depthFunc        ,glGetIntegerv(GL_DEPTH_FUNC,&depthFunc)     ,glDepthFunc(depthFunc));
DECLARE_PRESERVE_STATE( PreserveDepthMask ,GLboolean depthMask    ,glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask),glDepthMask(depthMask));
DECLARE_PRESERVE_STATE( PreserveColorMask ,GLboolean colorMask[4] ,glGetBooleanv(GL_COLOR_WRITEMASK,colorMask) ,glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]));
DECLARE_PRESERVE_STATE( PreserveCullFace  ,GLboolean cullFace     ,cullFace=glIsEnabled(GL_CULL_FACE)          ,if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE));
DECLARE_PRESERVE_STATE( PreserveCullMode  ,GLint cullMode         ,glGetIntegerv(GL_CULL_FACE_MODE,&cullMode)  ,glCullFace(cullMode));
//DECLARE_PRESERVE_STATE( PreserveFrontFace ,GLint frontFace        ,glGetIntegerv(GL_FRONT_FACE,&frontFace)     ,glFrontFace(frontFace));
DECLARE_PRESERVE_STATE( PreserveBlend     ,GLboolean blend        ,blend=glIsEnabled(GL_BLEND)                 ,if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND));
DECLARE_PRESERVE_STATE( PreserveBlendFunc ,GLint src;GLint dst    ,glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst) ,glBlendFunc(src,dst));
DECLARE_PRESERVE_STATE( PreserveFBO       ,FBO state              ,state=FBO::getState()                       ,state.restore());
#ifndef RR_GL_ES2
DECLARE_PRESERVE_STATE( PreserveAlphaTest ,GLboolean test         ,test=glIsEnabled(GL_ALPHA_TEST)             ,if (test) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST));
DECLARE_PRESERVE_STATE( PreserveAlphaFunc ,GLint func;GLfloat ref ,glGetIntegerv(GL_ALPHA_TEST_FUNC,&func);glGetFloatv(GL_ALPHA_TEST_REF,&ref) ,glAlphaFunc(func,ref));
//DECLARE_PRESERVE_STATE( PreserveFBSRGB    ,GLboolean enabled      ,enabled=glIsEnabled(GL_FRAMEBUFFER_SRGB)    ,if (enabled) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB));
#endif

#undef DECLARE_PRESERVE_STATE

//! Faster preservation of flag.
//
//! Requested value is set in ctor, old value is restored in dtor.
//! Restore works reliably only if you don't touch flag between ctor and dtor.
class PreserveFlag
{
	GLenum name;
	bool oldval;
	bool newval;
public:
	PreserveFlag(GLenum _name, bool _newval)
	{
		name = _name;
		newval = oldval = glIsEnabled(name)!=GL_FALSE;
		change(_newval);
	}
	void change(bool _newval)
	{
		if (_newval!=newval)
		{
			newval = _newval;
			if (newval)
				glEnable(name);
			else
				glDisable(name);
		}
	}
	~PreserveFlag()
	{
		change(oldval);
	}
};

}; // namespace

#endif
