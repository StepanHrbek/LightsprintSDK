//----------------------------------------------------------------------------
//! \file UberProgramSetup.h
//! \brief LightsprintGL | parameter management for our UberProgram in UberShader.vs/fs
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2013
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
	// Program::sendTexture() codes, not sent to OpenGL
	// (Program remembers dynamically allocated texture units in slots indexed by these codes)
	TEX_CODE_2D_MATERIAL_DIFFUSE          = 0, ///< Program::sendTexture() code used by our uberprogram for diffuse map.
	TEX_CODE_2D_MATERIAL_SPECULAR         = 1, ///< Program::sendTexture() code used by our uberprogram for specular map.
	TEX_CODE_2D_MATERIAL_TRANSPARENCY     = 2, ///< Program::sendTexture() code used by our uberprogram for rgb transparency map.
	TEX_CODE_2D_MATERIAL_EMISSIVE         = 3, ///< Program::sendTexture() code used by our uberprogram for emissive map.
	TEX_CODE_2D_MATERIAL_BUMP             = 4, ///< Program::sendTexture() code used by our uberprogram for normal map.
	TEX_CODE_2D_LIGHT_DIRECT              = 5, ///< Program::sendTexture() code used by our uberprogram for projected light map.
	TEX_CODE_2D_LIGHT_INDIRECT            = 6, ///< Program::sendTexture() code used by our uberprogram for lightmap/ambient map/light detail map.
	TEX_CODE_2D_LIGHT_INDIRECT_MIRROR     = 7, ///< Program::sendTexture() code used by our uberprogram for mirror map.
	TEX_CODE_CUBE_LIGHT_INDIRECT          = 8, ///< Program::sendTexture() code used by our uberprogram for environment map.

	// texcoords assigned to UberProgram, sent to OpenGL
	// these constants are hardcoded in shaders
	MULTITEXCOORD_MATERIAL_DIFFUSE       = 0, ///< Texcoord channel used by our uberprogram for diffuse map uv.
	MULTITEXCOORD_LIGHT_INDIRECT         = 1, ///< Texcoord channel used by our uberprogram for lightmap/ambient map/light detail map uv.
	MULTITEXCOORD_FORCED_2D              = 2, ///< Texcoord channel used by our uberprogram for forced projection space vertex coordinates.
	MULTITEXCOORD_MATERIAL_EMISSIVE      = 3, ///< Texcoord channel used by our uberprogram for emissive map uv.
	MULTITEXCOORD_MATERIAL_TRANSPARENCY  = 4, ///< Texcoord channel used by our uberprogram for rgb transparency map uv.
	MULTITEXCOORD_MATERIAL_BUMP_MAP      = 5, ///< Texcoord channel used by our uberprogram for normal map uv.
	MULTITEXCOORD_MATERIAL_SPECULAR      = 6, ///< Texcoord channel used by our uberprogram for specular map uv.
	MULTITEXCOORD_COUNT                  = 7, ///< If you increase it, see [#17]
};

//! Clipping plane values.
struct ClipPlanes
{
	rr::RRVec4 clipPlane; ///< Discards geometry with pos . clipPlane<=0
	float clipPlaneXA; ///< Discards geometry with pos.x>clipPlaneXA
	float clipPlaneXB; ///< Discards geometry with pos.x<clipPlaneXB
	float clipPlaneYA; ///< Discards geometry with pos.y>clipPlaneYA
	float clipPlaneYB; ///< Discards geometry with pos.y<clipPlaneYB
	float clipPlaneZA; ///< Discards geometry with pos.z>clipPlaneZA
	float clipPlaneZB; ///< Discards geometry with pos.z<clipPlaneZB
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
//! Call validate() to get the nearest supported combination.
//!
//! LIGHT_INDIRECT_XXX flags enable rendering of indirect illumination in various situations, using various techniques.
//! Shader accumulates illumination from all enabled techniques, so if you build it with multiple techniques enabled,
//! you can end up with higher than expected indirect illumination.
//! On the other hand, higher level functions like RendererOfScene::render() are designed to be called with multiple techniques enabled;
//! they automatically select optimal technique out of enabled ones and disable the rest before building shader
//! (selection is based on material and illumination buffers available in object).
struct RR_GL_API UberProgramSetup
{
	// Various direct shadowing options.
	unsigned char SHADOW_MAPS              :4; ///< Number of shadow maps processed in one pass. 0=no shadows, 1=hard shadows, more=soft shadows. Valid values: 0..detectMaxShadowmaps().
	unsigned char SHADOW_SAMPLES           :4; ///< Number of samples read from each shadowmap. 0=no shadows, 1=hard shadows, 2,4,8=soft shadows. Valid values: 0,1,2,4,8.
	bool     SHADOW_COLOR                  :1; ///< Enables colored semitransparent shadows.
	bool     SHADOW_PENUMBRA               :1; ///< Enables blend of all shadowmaps, used by penumbra shadows.
	bool     SHADOW_CASCADE                :1; ///< Enables cascading of all shadowmaps, used by cascaded shadowmapping.
	bool     SHADOW_ONLY                   :1; ///< Renders only direct shadows without direct illumination. Must be combined with indirect illumination, shadows are subtracted from indirect light. Has no visible effect if there's no indirect light.

	// Various direct illumination options.
	bool     LIGHT_DIRECT                  :1; ///< Enables direct light. All enabled LIGHT_DIRECT_XXX are multiplied.
	bool     LIGHT_DIRECT_COLOR            :1; ///< Enables modulation of direct light by constant color.
	bool     LIGHT_DIRECT_MAP              :1; ///< Enables modulation of direct light by color map. Projects texture.
	bool     LIGHT_DIRECTIONAL             :1; ///< Switches direct light from positional (with optional dist.atten.) to directional (no dist.atten.).
	bool     LIGHT_DIRECT_ATT_SPOT         :1; ///< Enables direct light spot attenuation. (You can get the same effect faster with LIGHT_DIRECT_MAP by projecting spot texture.)
	bool     LIGHT_DIRECT_ATT_PHYSICAL     :1; ///< Enables direct light physically correct distance attenuation.
	bool     LIGHT_DIRECT_ATT_POLYNOMIAL   :1; ///< Enables direct light polynomial distance attenuation.
	bool     LIGHT_DIRECT_ATT_EXPONENTIAL  :1; ///< Enables direct light exponential distance attenuation.

	// Various indirect illumination techniques.
	bool     LIGHT_INDIRECT_CONST          :1; ///< Illuminates material (both diffuse and specular components) by constant ambient light. Non-directional. Always available (does not need any data buffers), but the least realistic out of all LIGHT_INDIRECT_ options.
	bool     LIGHT_INDIRECT_VCOLOR         :1; ///< Illuminates material's diffuse component by vertex colors (any vertex buffer you provide, or realtime radiosity calculated one). Non-directional.
	bool     LIGHT_INDIRECT_VCOLOR2        :1; ///< Enables blend between two ambient vertex colors. Non-directional.
	bool     LIGHT_INDIRECT_VCOLOR_PHYSICAL:1; ///< If indirect illumination by vertex colors is used, it is expected in physical/linear scale, converted to sRGB in shader.
	bool     LIGHT_INDIRECT_MAP            :1; ///< Illuminates material's diffuse component by ambient map (any map you provide). Non-directional.
	bool     LIGHT_INDIRECT_MAP2           :1; ///< Enables blend between two ambient maps. Non-directional.
	bool     LIGHT_INDIRECT_DETAIL_MAP     :1; ///< Enables modulation of indirect light by light detail map. Non-directional.
	bool     LIGHT_INDIRECT_ENV_DIFFUSE    :1; ///< Illuminates material's diffuse component by environment map (any map you provide, or lowres realtime raytraced one). Affects only flat objects with environment map preallocated. Directional, works with normal maps. Recommended for all dynamic objects except for large planes, where LIGHT_INDIRECT_MIRROR_DIFFUSE produces better (but slower) results.
	bool     LIGHT_INDIRECT_ENV_SPECULAR   :1; ///< Illuminates material's specular component by environment map (any map you provide, or lowres realtime raytraced one). Affects only flat objects with environment map preallocated. Directional, works with normal maps. Recommended for all objects without LIGHT_INDIRECT_MIRROR_SPECULAR.
	bool     LIGHT_INDIRECT_MIRROR_DIFFUSE :1; ///< Illuminates material's diffuse component by realtime rasterized highres mirror reflection. Affects only flat objects without environment map. Directional, works with normal maps. Recommended for all large dynamic planes (as a slower but higher quality option). Small dynamic planes look better with LIGHT_INDIRECT_ENV_DIFFUSE, static planes look better with LIGHT_INDIRECT_VCOLOR(unless number of vertices is very low) or LIGHT_INDIRECT_MAP.
	bool     LIGHT_INDIRECT_MIRROR_SPECULAR:1; ///< Illuminates material's specular component by realtime rasterized highres mirror reflection. Affects only flat objects without environment map. Directional, works with normal maps. Recommended for all planes (as a slower but higher quality option), except for very small ones, where much faster LIGHT_INDIRECT_ENV_SPECULAR might be good enough. (If you don't see mirroring, is specularEnvMap NULL? Is volume of mesh AABB zero? Object can be arbitrarily rotated, but original mesh before rotation must be axis aligned.)
	bool     LIGHT_INDIRECT_MIRROR_MIPMAPS :1; ///< Enables higher quality mirroring (builds lightIndirectMirrorMap with mipmaps).

	// Various material features.
	bool     MATERIAL_DIFFUSE              :1; ///< Enables material's diffuse reflection. All enabled MATERIAL_DIFFUSE_XXX are multiplied. When only MATERIAL_DIFFUSE is enabled, diffuse color is 1 (white).
	bool     MATERIAL_DIFFUSE_X2           :1; ///< Enables material's diffuse reflectance multiplied by 2. (used by Quake3 engine scenes)
	bool     MATERIAL_DIFFUSE_CONST        :1; ///< Enables material's diffuse reflectance modulated by constant color.
	bool     MATERIAL_DIFFUSE_MAP          :1; ///< Enables material's diffuse reflectance modulated by diffuse map.

	bool     MATERIAL_SPECULAR             :1; ///< Enables material's specular reflectance. All enabled MATERIAL_SPECULAR_XXX are multiplied. When only MATERIAL_SPECULAR is enabled, specular color is 1 (white).
	bool     MATERIAL_SPECULAR_CONST       :1; ///< Enables material's specular reflectance modulated by constant color.
	bool     MATERIAL_SPECULAR_MAP         :1; ///< Enables material's specular reflectance and shininess modulated by specular map (reflectance is read from RGB, material's shininess is modulated by A).
	unsigned char MATERIAL_SPECULAR_MODEL  :2; ///< Copy of specularModel from material.

	bool     MATERIAL_EMISSIVE_CONST       :1; ///< Enables material's emission stored in constant. All enabled MATERIAL_EMISSIVE_XXX are accumulated.
	bool     MATERIAL_EMISSIVE_MAP         :1; ///< Enables material's emission stored in sRGB map.

	bool     MATERIAL_TRANSPARENCY_CONST   :1; ///< Enables materials's specular transmittance modulated by constant.
	bool     MATERIAL_TRANSPARENCY_MAP     :1; ///< Enables materials's specular transmittance modulated by texture (rgb or alpha). Optimization: with diffuse map enabled and transparency in its alpha, set only MATERIAL_TRANSPARENCY_IN_ALPHA, not MATERIAL_TRANSPARENCY_MAP.
	bool     MATERIAL_TRANSPARENCY_IN_ALPHA:1; ///< If(!MATERIAL_TRANSPARENCY_CONST && !MATERIAL_TRANSPARENCY_MAP), enables materials's specular transmittance modulated by diffuse alpha (0=transparent), otherwise makes transparency read from alpha (0=transparent) rather than from rgb (1=transparent). 
	bool     MATERIAL_TRANSPARENCY_BLEND   :1; ///< When rendering transparency, uses blending (for semitransparency) rather than alpha keying (0% or 100%).
	bool     MATERIAL_TRANSPARENCY_TO_RGB  :1; ///< When blending, uses more realistic RGB blending rather than usual alpha blending. When not blending, transparency color is just sent to RGB instead of A (this mode must not be combined with diffuse/specular/emis because they also write to RGB).
	bool     MATERIAL_TRANSPARENCY_FRESNEL :1; ///< Turns small fraction of specular transmittance and MATERIAL_EMISSIVE_CONST into specular reflectance, decreasing transmittance+emittance, increasing reflectance. Including emittance seems wrong, but materials of deep water surface simulate transmittance+scattering(some photons going back) with constant emittance. If you don't see effects in realtime, check: 1) RRMaterial::refractionIndex!=1 (for refractionIndex converging to 1, images with Fresnel converge to images without Fresnel, this is correctly simulated). 2) MATERIAL_TRANSPARENCY_BLEND enabled (small transparency change would be hardly visible with 1-bit keying). 3) For increase in reflectance to be visible, MATERIAL_SPECULAR should be enabled (otherwise you see only reduced transmittance/emittance). 4) Also for reflectance to be visible, LIGHT_INDIRECT_ENV_SPECULAR is recommended. Then environment map for LIGHT_INDIRECT_ENV_SPECULAR must be available. If you allocate it with allocateBuffersForRealtimeGI() (rather than manually), make sure that allocation does not fails because of thresholds.

	bool     MATERIAL_BUMP_MAP             :1; ///< Enables normal map or height map, each pixel's normal is modulated by contents of map.
	bool     MATERIAL_BUMP_TYPE_HEIGHT     :1; ///< Switches bump mapping code from default normal maps to height maps (height maps need additional code to calculate normals).
	bool     MATERIAL_NORMAL_MAP_FLOW      :1; ///< When using normal map, enables flow of normal map over geometry, simulating flow of waves on large water surface.

	bool     MATERIAL_CULLING              :1; ///< Enables materials's n-sided property (culling is enabled/disabled according to material). false = treat everything as 2sided.

	// Misc other options.
	bool     ANIMATION_WAVE                :1; ///< Enables simple procedural deformation, only to demonstrate that lighting supports animations.
	bool     POSTPROCESS_NORMALS           :1; ///< Renders normal values instead of colors.
	bool     POSTPROCESS_BRIGHTNESS        :1; ///< Enables brightness correction of final color (before gamma).
	bool     POSTPROCESS_GAMMA             :1; ///< Enables gamma correction of final color (after brightness).
	bool     POSTPROCESS_BIGSCREEN         :1; ///< Simulates effect of party projected bigscreen with ambient light.
	bool     OBJECT_SPACE                  :1; ///< Enables positions in object space, vertices are transformed by uniform worldMatrix. Without OBJECT_SPACE, objects are rendered in their local spaces, you may get all objects stacked in world center.
	bool     CLIP_PLANE                    :1; ///< Discards geometry with pos . clipPlane<=0.
	bool     CLIP_PLANE_XA                 :1; ///< Discards geometry with x>clipPlaneXA.
	bool     CLIP_PLANE_XB                 :1; ///< Discards geometry with x<clipPlaneXB.
	bool     CLIP_PLANE_YA                 :1; ///< Discards geometry with y>clipPlaneYA.
	bool     CLIP_PLANE_YB                 :1; ///< Discards geometry with y<clipPlaneYB.
	bool     CLIP_PLANE_ZA                 :1; ///< Discards geometry with z>clipPlaneZA.
	bool     CLIP_PLANE_ZB                 :1; ///< Discards geometry with z<clipPlaneZB.
	bool     FORCE_2D_POSITION             :1; ///< Overrides projection space vertex coordinates with coordinates read from texcoord7 channel. Triangles are lit as if they stay on their original positions, but they are rendered to externally set positions in texture.

	const char* comment;                       ///< Comment added to shader. Must start with //. Not freed.

	//! Creates UberProgramSetup with everything turned off by default.
	//! It is suitable for rendering opaque faces into shadowmap, no color is produced, only depth.
	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
	}

	//! Prepares for rendering with all lighting features, enables all SHADOW_XXX and LIGHT_XXX relevant for RendererOfScene.
	//
	//! Certain shadow and light flags are enabled, you can disable individual flags to prevent renderer from using given features.
	//! Some flags are not touched, they depend on light properties and RendererOfScene sets them automatically.
	void enableAllLights();

	//! Prepares for rendering with all material features, enables all MATERIAL_XXX relevant for RendererOfScene.
	//
	//! Nearly all material flags are enabled,
	//! you can disable individual flags to prevent renderer from using given features.
	void enableAllMaterials();

	//! Enables only MATERIAL_XXX required by given material, disables unused MATERIAL_XXX.
	//
	//! With meshArrays provided, enables only features supported by mesh,
	//! e.g. MATERIAL_BUMP_MAP only if tangents, bitangents and selected uv channel are present.
	//! With meshArrays NULL, works as if mesh contains all uv channels necessary, but no tangents (so normal maps will be disabled).
	void enableUsedMaterials(const rr::RRMaterial* material, const rr::RRMeshArrays* meshArrays);

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
	//! Some shader parameters are left uninitialized, useIlluminationEnvMaps(), useIlluminationMirror() and useMaterial() should follow to set them.
	Program* useProgram(UberProgram* uberProgram, const rr::RRCamera* camera, RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, const ClipPlanes* clipPlanes);
	//! Sets shader uniform parameters to match given material, should be called after useProgram() or getNextPass().
	//! You can call expensive useProgram() once and cheaper useMaterial() multiple times.
	void useMaterial(Program* program, const rr::RRMaterial* material) const;
	//! Sets shader illumination environment map, should be called after useProgram() or getNextPass().
	//! You can call expensive useProgram() once and cheaper useIlluminationEnvMap() multiple times.
	void useIlluminationEnvMap(Program* program, const rr::RRBuffer* environment);
	//! Sets shader illumination mirror map, should be called after useProgram() or getNextPass().
	//! You can call expensive useProgram() once and cheaper useIlluminationMirror() multiple times.
	void useIlluminationMirror(Program* program, const rr::RRBuffer* mirrorMap);
	//! Sets world matrix for given object.
	void useWorldMatrix(Program* program, const rr::RRObject* object);
};

}; // namespace

#endif
