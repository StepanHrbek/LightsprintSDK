//----------------------------------------------------------------------------
//! \file UberProgramSetup.h
//! \brief LightsprintGL | parameter management for our UberProgram in UberShader.vs/fs
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2011
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef UBERPROGRAMSETUP_H
#define UBERPROGRAMSETUP_H

#include <cstring>
#include "UberProgram.h"
#include "RealtimeLight.h"

namespace rr_gl
{

enum
{
	// textures assigned to UberProgram
	// GeForce supports 0..31, ok
	// Radeon supports 0..15, ok
	// Mesa supports only 0..7, ok if you use at most 1 shadowmap and 1 lightmap
	TEXTURE_2D_MATERIAL_DIFFUSE          = 0, ///< Texture image unit used by our uberprogram for diffuse map.
	TEXTURE_2D_LIGHT_DIRECT              = 1, ///< Texture image unit used by our uberprogram for projected light map.
	TEXTURE_2D_LIGHT_INDIRECT            = 2, ///< Texture image unit used by our uberprogram for lightmap/ambient map/light detail map.
	TEXTURE_2D_MATERIAL_TRANSPARENCY     = 3, ///< Texture image unit used by our uberprogram for rgb transparency map.
	TEXTURE_2D_MATERIAL_EMISSIVE         = 4, ///< Texture image unit used by our uberprogram for emissive map.
	TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE  = 5, ///< Texture image unit used by our uberprogram for diffuse cube map.
	TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR = 6, ///< Texture image unit used by our uberprogram for specular cube map.
	TEXTURE_2D_SHADOWMAP_0               = 7, ///< Texture image unit used by our shadowmap 0. For more shadowmaps in single shader, use up to TEXTURE_2D_SHADOWMAP_0+7. If there are 6 shadowmaps and 6 color maps, color maps use TEXTURE_2D_SHADOWMAP_0+6..11. Conflict with TEXTURE_2D_LIGHT_INDIRECT2 is no issue, we don't use the latter now. High number of texture image units used may be an issue, needs additional testing.
	TEXTURE_2D_LIGHT_INDIRECT2           = 15, ///< Texture image unit used by our uberprogram for ambient map2.

	// texcoords assigned to UberProgram
	// these constants are hardcoded in shaders
	MULTITEXCOORD_MATERIAL_DIFFUSE       = 0, ///< Texcoord channel used by our uberprogram for diffuse map uv.
	MULTITEXCOORD_LIGHT_INDIRECT         = 1, ///< Texcoord channel used by our uberprogram for lightmap/ambient map/light detail map uv.
	MULTITEXCOORD_FORCED_2D              = 2, ///< Texcoord channel used by our uberprogram for forced projection space vertex coordinates.
	MULTITEXCOORD_MATERIAL_EMISSIVE      = 3, ///< Texcoord channel used by our uberprogram for emissive map uv.
	MULTITEXCOORD_MATERIAL_TRANSPARENCY  = 4, ///< Texcoord channel used by our uberprogram for rgb transparency map uv.
};


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vs/fs

//! Options that change code of UberProgram made of UberShader.vs/fs.
//
//! UberProgram + UberProgramSetup = Program
//!
//! All attribute values are converted to "#define ATTRIBUTE [value]" format
//! and inserted at the beginning of ubershader.
//!
//! Some combinations of attributes are not supported and getProgram will return NULL.
//! Call validate() to get nearest supported combination.
struct RR_GL_API UberProgramSetup
{
	unsigned char SHADOW_MAPS              :4; ///< Number of shadow maps processed in one pass. 0=no shadows, 1=hard shadows, more=soft shadows. Valid values: 0..detectMaxShadowmaps().
	unsigned char SHADOW_SAMPLES           :4; ///< Number of samples read from each shadowmap. 0=no shadows, 1=hard shadows, 2,4,8=soft shadows. Valid values: 0,1,2,4,8.
	bool     SHADOW_COLOR                  :1; ///< Enables colored semitransparent shadows.
	bool     SHADOW_PENUMBRA               :1; ///< Enables blend of all shadowmaps, used by penumbra shadows.
	bool     SHADOW_CASCADE                :1; ///< Enables cascading of all shadowmaps, used by cascaded shadowmapping.
	bool     SHADOW_ONLY                   :1; ///< Renders only direct shadows without direct illumination. Must be combined with indirect illumination, shadows are subtracted from indirect light. Has no visible effect if there's no indirect light.

	bool     LIGHT_DIRECT                  :1; ///< Enables direct light. All enabled LIGHT_DIRECT_XXX are multiplied.
	bool     LIGHT_DIRECT_COLOR            :1; ///< Enables modulation of direct light by constant color.
	bool     LIGHT_DIRECT_MAP              :1; ///< Enables modulation of direct light by color map. Projects texture.
	bool     LIGHT_DIRECTIONAL             :1; ///< Switches direct light from positional (with optional dist.atten.) to directional (no dist.atten.).
	bool     LIGHT_DIRECT_ATT_SPOT         :1; ///< Enables direct light spot attenuation. (You can get the same effect faster with LIGHT_DIRECT_MAP by projecting spot texture.)
	bool     LIGHT_DIRECT_ATT_PHYSICAL     :1; ///< Enables direct light physically correct distance attenuation.
	bool     LIGHT_DIRECT_ATT_POLYNOMIAL   :1; ///< Enables direct light polynomial distance attenuation.
	bool     LIGHT_DIRECT_ATT_EXPONENTIAL  :1; ///< Enables direct light exponential distance attenuation.

	bool     LIGHT_INDIRECT_CONST          :1; ///< Enables indirect light, constant. All enabled LIGHT_INDIRECT_XXX are accumulated.
	bool     LIGHT_INDIRECT_VCOLOR         :1; ///< Enables indirect light, set per vertex.
	bool     LIGHT_INDIRECT_VCOLOR2        :1; ///< Enables blend between two ambient vertex colors.
	bool     LIGHT_INDIRECT_VCOLOR_PHYSICAL:1; ///< If indirect light per vertex is used, it is expected in physical/linear scale, converted to sRGB in shader.
	bool     LIGHT_INDIRECT_MAP            :1; ///< Enables indirect light, set by ambient map.
	bool     LIGHT_INDIRECT_MAP2           :1; ///< Enables blend between two ambient maps.
	bool     LIGHT_INDIRECT_DETAIL_MAP     :1; ///< Enables modulation of indirect light by light detail map.
	bool     LIGHT_INDIRECT_ENV_DIFFUSE    :1; ///< Enables indirect light, set by diffuse reflection environment map.
	bool     LIGHT_INDIRECT_ENV_SPECULAR   :1; ///< Enables indirect light, set by specular reflection environment map.
	bool     LIGHT_INDIRECT_auto           :1; ///< Extension. Makes renderer set LIGHT_INDIRECT_[VCOLOR*|MAP*|DETAIL_MAP] flags automatically according to available data.

	bool     MATERIAL_DIFFUSE              :1; ///< Enables material's diffuse reflection. All enabled MATERIAL_DIFFUSE_XXX are multiplied. When only MATERIAL_DIFFUSE is enabled, diffuse color is 1 (white).
	bool     MATERIAL_DIFFUSE_X2           :1; ///< Enables material's diffuse reflectance multiplied by 2. (used by Quake3 engine scenes)
	bool     MATERIAL_DIFFUSE_CONST        :1; ///< Enables material's diffuse reflectance modulated by constant color.
	bool     MATERIAL_DIFFUSE_MAP          :1; ///< Enables material's diffuse reflectance modulated by diffuse map.

	bool     MATERIAL_SPECULAR             :1; ///< Enables material's specular reflectance. All enabled MATERIAL_SPECULAR_XXX are multiplied. When only MATERIAL_SPECULAR is enabled, specular color is 1 (white).
	bool     MATERIAL_SPECULAR_CONST       :1; ///< Enables material's specular reflectance modulated by constant color.
	bool     MATERIAL_SPECULAR_MAP         :1; ///< Enables specular map, each pixel gets 100% diffuse or 100% specular. Decision is based on contents of diffuse map.

	bool     MATERIAL_EMISSIVE_CONST       :1; ///< Enables material's emission stored in constant. All enabled MATERIAL_EMISSIVE_XXX are accumulated.
	bool     MATERIAL_EMISSIVE_MAP         :1; ///< Enables material's emission stored in sRGB map.

	bool     MATERIAL_TRANSPARENCY_CONST   :1; ///< Enables materials's specular transmittance modulated by constant.
	bool     MATERIAL_TRANSPARENCY_MAP     :1; ///< Enables materials's specular transmittance modulated by texture (rgb or alpha). Optimization: with diffuse map enabled and transparency in its alpha, set only MATERIAL_TRANSPARENCY_IN_ALPHA, not MATERIAL_TRANSPARENCY_MAP.
	bool     MATERIAL_TRANSPARENCY_IN_ALPHA:1; ///< If(!MATERIAL_TRANSPARENCY_CONST && !MATERIAL_TRANSPARENCY_MAP), enables materials's specular transmittance modulated by diffuse alpha (0=transparent), otherwise makes transparency read from alpha (0=transparent) rather than from rgb (1=transparent). 
	bool     MATERIAL_TRANSPARENCY_BLEND   :1; ///< When rendering transparency, uses blending (for semitransparency) rather than alpha keying (0% or 100%).
	bool     MATERIAL_TRANSPARENCY_TO_RGB  :1; ///< When blending, uses more realistic RGB blending rather than usual alpha blending. When not blending, transparency color is just sent to RGB instead of A (this mode must not be combined with diffuse/specular/emis because they also write to RGB).

	bool     MATERIAL_NORMAL_MAP           :1; ///< Enables normal map, each pixel's normal is modulated by contents of diffuse map.
	bool     MATERIAL_CULLING              :1; ///< Enables materials's n-sided property (culling is enabled/disabled according to material, no change in shader).

	bool     ANIMATION_WAVE                :1; ///< Enables simple procedural deformation, only to demonstrate that lighting supports animations.
	bool     POSTPROCESS_NORMALS           :1; ///< Renders normal values instead of colors.
	bool     POSTPROCESS_BRIGHTNESS        :1; ///< Enables brightness correction of final color (before gamma).
	bool     POSTPROCESS_GAMMA             :1; ///< Enables gamma correction of final color (after brightness).
	bool     POSTPROCESS_BIGSCREEN         :1; ///< Simulates effect of party projected bigscreen with ambient light.
	bool     OBJECT_SPACE                  :1; ///< Enables positions in object space, vertices are transformed by uniform worldMatrix. Without OBJECT_SPACE, objects are rendered in their local spaces, you may get all objects stacked in world center.
	bool     CLIP_PLANE_XA                 :1; ///< Discards geometry with x<clipPlaneXA.
	bool     CLIP_PLANE_XB                 :1; ///< Discards geometry with x<clipPlaneXB.
	bool     CLIP_PLANE_YA                 :1; ///< Discards geometry with y<clipPlaneYA.
	bool     CLIP_PLANE_YB                 :1; ///< Discards geometry with y<clipPlaneYB. This is used to clip parts of scene below water level.
	bool     CLIP_PLANE_ZA                 :1; ///< Discards geometry with z<clipPlaneZA.
	bool     CLIP_PLANE_ZB                 :1; ///< Discards geometry with z<clipPlaneZB.
	bool     FORCE_2D_POSITION             :1; ///< Overrides projection space vertex coordinates with coordinates read from texcoord7 channel. Triangles are lit as if they stay on their original positions, but they are rendered to externally set positions in texture.
	bool     SKYBOX                        :1; ///< Enables rendering of skybox. This is unrelated to lighting by skybox.

	//! Creates UberProgramSetup with everything turned off by default.
	//! It is suitable for rendering into shadowmap, no color is produced, only depth.
	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
		LIGHT_INDIRECT_VCOLOR_PHYSICAL = true; // this doesn't turn on LIGHT_INDIRECT_VCOLOR, but if YOU turn it on, it will be in physical scale rather than custom
		MATERIAL_CULLING = true;
	}

	//! Enables all MATERIAL_XXX except for MATERIAL_NORMAL_MAP.
	//
	//! You can freely enable and disable MATERIAL_XXX features to get desired render,
	//! this is helper for those who want everything, all material features enabled.
	//! It is usually used before final render of scene.
	void enableAllMaterials();

	//! Enables only MATERIAL_XXX required by given material, disables unused MATERIAL_XXX.
	void enableUsedMaterials(const rr::RRMaterial* material);

	//! Reduces material setup, removes properties not present in fullMaterial.
	//
	//! Note: MATERIAL_XXX_MAP is considered superset of MATERIAL_XXX_CONST, so
	//! mixing MATERIAL_XXX_CONST in this or fullMaterial, and MATERIAL_XXX_MAP in the other input,
	//! leads to MATERIAL_XXX_CONST set.
	void reduceMaterials(const UberProgramSetup& fullMaterial);

	//! Returns our attribute values in format suitable for our uberprogram.
	//! Example: "#define SHADOW_MAPS 2\n#define SHADOW_SAMPLES 4\n#define MATERIAL_DIFFUSE\n".
	const char* getSetupString();

	// Operators for unordered_map in renderer.
	bool operator ==(const UberProgramSetup& a) const;
	bool operator !=(const UberProgramSetup& a) const;
	bool operator <(const UberProgramSetup& a) const;
	operator size_t() const {return *(unsigned*)(((char*)this)+3);}

	//! Returns uberProgram with parameter values defined by our attributes.
	//! UberProgram with parameter values defined is Program.
	Program* getProgram(UberProgram* uberProgram);
	//! Returns the highest number of shadowmaps,
	//! that may be processed in one pass with this setup (material & lighting).
	//! The same number is set to SHADOW_MAPS.
	//! \n\n If one of arguments in argv is penumbraX for X=1..8, X is returned.
	unsigned detectMaxShadowmaps(UberProgram* uberProgram, int argc = 0, const char*const*argv = NULL);
	//! Check whether number of texture image units supported by platform is high enough, warns if it isn't.
	static void checkCapabilities();
	//! Change invalid settings to closest valid settings.
	void validate();
	//! Sets rendering pipeline so that following primitives are rendered by our program.
	//! Camera::setupForRender() should precede this call.
	//! Some shader parameters are left uninitialized, useIlluminationEnvMaps() and useMaterial() should follow to set them.
	Program* useProgram(UberProgram* uberProgram, RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, float* clipPlanes);
	//! Sets shader uniform parameters to match given material, should be called after useProgram() or getNextPass().
	//! You can call expensive useProgram() once and cheaper useMaterial() multiple times.
	void useMaterial(Program* program, const rr::RRMaterial* material) const;
	//! Sets shader illumination environment maps, should be called after useProgram() or getNextPass().
	//! You can call expensive useProgram() once and cheaper useIlluminationEnvMaps() multiple times.
	void useIlluminationEnvMaps(Program* program, rr::RRObjectIllumination* illumination);
	//! Sets world matrix for given object.
	void useWorldMatrix(Program* program, rr::RRObject* object);
};

}; // namespace

#endif
