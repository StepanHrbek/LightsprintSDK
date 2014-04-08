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
	SM_INTERLACED       =2, ///< interlaced, with top scanline visible by right eye, for passive displays \image html stereo_interlaced.png
	SM_INTERLACED_SWAP  =3, ///< interlaced, with top scanline visible by left eye
	SM_SIDE_BY_SIDE     =4, ///< left half is left eye \image html stereo_sidebyside.jpg
	SM_SIDE_BY_SIDE_SWAP=5, ///< left half is right eye
	SM_TOP_DOWN         =6, ///< top half is left eye \image html stereo_topdown.jpg
	SM_TOP_DOWN_SWAP    =7, ///< top half is right eye
	SM_OCULUS_RIFT      =8, ///< for Oculus Rift \image html stereo_oculus.jpg
	SM_OCULUS_RIFT_SWAP =9, ///< for Oculus Rift with swapped eyes
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

	//! For Oculus Rift only, HMDInfo.DistortionK.
	rr::RRVec4 oculusDistortionK;
	//! For Oculus Rift only, HMDInfo.ChromaAbCorrection.
	rr::RRVec4 oculusChromaAbCorrection;
	//! For Oculus Rift only, 1-2.0*HMDInfo.LensSeparationDistance/HMDInfo.HScreenSize
	float oculusLensShift;

	//! Convenience ctor, for setting plugin parameters. Additional Oculus Rift parameters are set to defaults.
	PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode) : stereoMode(_stereoMode), oculusDistortionK(1,0.22f,0.24f,0), oculusChromaAbCorrection(0.996f,-0.004f,1.014f,0), oculusLensShift(0.152f) {next=_next;}

	//! Convenience ctor, for setting plugin parameters, including Oculus Rift ones.
	//
	//! Example:
	//! rr_gl::PluginParamsStereo ppStereo(pluginChain,SM_OCULUS_RIFT,oculusHMDInfo.DistortionK,oculusHMDInfo.ChromaAbCorrection,1-2.f*oculusHMDInfo.LensSeparationDistance/oculusHMDInfo.HScreenSize);
	PluginParamsStereo(const PluginParams* _next, StereoMode _stereoMode, const float(&_oculusDistortionK)[4], const float(&_oculusChromaAbCorrection)[4], float _oculusLensShift) : stereoMode(_stereoMode), oculusDistortionK(_oculusDistortionK[0],_oculusDistortionK[1],_oculusDistortionK[2],_oculusDistortionK[3]), oculusChromaAbCorrection(_oculusChromaAbCorrection[0],_oculusChromaAbCorrection[1],_oculusChromaAbCorrection[2],_oculusChromaAbCorrection[3]), oculusLensShift(_oculusLensShift) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
