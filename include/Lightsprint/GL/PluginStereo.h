//----------------------------------------------------------------------------
//! \file PluginStereo.h
//! \brief LightsprintGL | stereo render
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGINSTEREO_H
#define PLUGINSTEREO_H

#include "Plugin.h"

namespace rr_gl
{

enum StereoMode
{
	SM_MONO             =0, ///< common non-stereo mode
	SM_INTERLACED       =1, ///< interlaced, with top scanline visible by right eye, for passive displays \image html stereo_interlaced.png
	SM_SIDE_BY_SIDE     =2, ///< left half is left eye \image html stereo_sidebyside.jpg
	SM_TOP_DOWN         =3, ///< top half is left eye \image html stereo_topdown.jpg
	SM_OCULUS_RIFT      =4, ///< for Oculus Rift with SDK 0.4+, later call Oculus SDK to distort image \image html stereo_oculus.jpg

	//! quad buffered stereo, GL_BACK_RIGHT+GL_BACK_LEFT
	//
	//! note that GPU vendors put various artificial restrictions on quad buffered stereo; as of writing this
	//! - Quadro and FirePro support quad buffered stereo
	//! - Radeon supports quad buffered stereo in fullscreen only, on HD6000 and newer
	//! - GeForce does NOT support quad buffered stereo
	//! - Intel does NOT support quad buffered stereo
	SM_QUAD_BUFFERED    =5,
};

/////////////////////////////////////////////////////////////////////////////
//
// Stereo plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! <table border=0><tr align=top><td>
//! \image html stereo_interlaced.png
//! </td><td>
//! \image html stereo_sidebyside.jpg
//! </td></tr><tr align=top><td>
//! \image html stereo_topdown.jpg
//! </td><td>
//! \image html stereo_oculus.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsStereo : public PluginParams
{
public:
	//! One of camera stereo modes, or SM_MONO for common non-stereo render.
	StereoMode stereoMode;
	bool       stereoSwap;

	// For Oculus Rift only, HMDInfo.DistortionK.
	//rr::RRVec4 oculusDistortionK;
	// For Oculus Rift only, HMDInfo.ChromaAbCorrection.
	//rr::RRVec4 oculusChromaAbCorrection;
	// For Oculus Rift only, 1-2.0*HMDInfo.LensSeparationDistance/HMDInfo.HScreenSize
	//float oculusLensShift;
	//! For Oculus Rift only, ovrFovPort DefaultEyeFov[2]. We type it to void* to avoid Oculus SDK dependency.
	const void* oculusTanHalfFov;

	//! Convenience ctor, for setting plugin parameters. Additional Oculus Rift parameters are set to defaults.
	PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode, bool _stereoSwap) : stereoMode(_stereoMode), stereoSwap(_stereoSwap), oculusTanHalfFov(NULL) {next=_next;}
	//PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode, bool _stereoSwap) : stereoMode(_stereoMode), stereoSwap(_stereoSwap), oculusDistortionK(1,0.22f,0.24f,0), oculusChromaAbCorrection(0.996f,-0.004f,1.014f,0), oculusLensShift(0.152f), oculusTanHalfFov(NULL) {next=_next;}

	// Convenience ctor, for setting plugin parameters, including Oculus Rift ones.
	//
	// Example:
	// rr_gl::PluginParamsStereo ppStereo(pluginChain,SM_OCULUS_RIFT,oculusHMDInfo.DistortionK,oculusHMDInfo.ChromaAbCorrection,1-2.f*oculusHMDInfo.LensSeparationDistance/oculusHMDInfo.HScreenSize);
	//PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode, bool _stereoSwap, const float(&_oculusDistortionK)[4], const float(&_oculusChromaAbCorrection)[4], float _oculusLensShift) : stereoMode(_stereoMode), stereoSwap(_stereoSwap), oculusDistortionK(_oculusDistortionK[0],_oculusDistortionK[1],_oculusDistortionK[2],_oculusDistortionK[3]), oculusChromaAbCorrection(_oculusChromaAbCorrection[0],_oculusChromaAbCorrection[1],_oculusChromaAbCorrection[2],_oculusChromaAbCorrection[3]), oculusLensShift(_oculusLensShift), oculusTanHalfFov(NULL) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
