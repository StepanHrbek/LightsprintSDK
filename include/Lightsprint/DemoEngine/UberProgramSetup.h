// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, parameter management for our UberProgram in UberShader.vs/fs.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef UBERPROGRAMSETUP_H
#define UBERPROGRAMSETUP_H

#include "UberProgram.h"
#include "AreaLight.h"

namespace de
{

enum
{
	// textures assigned to UberProgram
	// 0..8 are reserved for shadowmaps, 9..15 declared here, 16+ would fail on ATI
	TEXTURE_2D_LIGHT_DIRECT              = 9, ///< Sampler id used by our uberprogram for projected light map.
	TEXTURE_2D_MATERIAL_DIFFUSE          = 10, ///< Sampler id used by our uberprogram for diffuse map.
	TEXTURE_2D_MATERIAL_EMISSIVE         = 11, ///< Sampler id used by our uberprogram for emissive map.
	TEXTURE_2D_LIGHT_INDIRECT            = 12, ///< Sampler id used by our uberprogram for ambient map.
	TEXTURE_2D_LIGHT_INDIRECT2           = 13, ///< Sampler id used by our uberprogram for ambient map2.
	TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR = 14, ///< Sampler id used by our uberprogram for specular cube map.
	TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE  = 15, ///< Sampler id used by our uberprogram for diffuse cube map.

	// texcoords assigned to UberProgram
	// these constants are hardcoded in shaders
	MULTITEXCOORD_MATERIAL_DIFFUSE       = 0, ///< Texcoord channel used by our uberprogram for diffuse map uv.
	MULTITEXCOORD_LIGHT_INDIRECT         = 1, ///< Texcoord channel used by our uberprogram for ambient map uv.
	MULTITEXCOORD_FORCED_2D              = 2, ///< Texcoord channel used by our uberprogram for forced projection space vertex coordinates.
	MULTITEXCOORD_MATERIAL_EMISSIVE      = 3, ///< Texcoord channel used by our uberprogram for emissive map uv.
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
//! Some combinations of attributes are not supported and getProgram will return NULL.
struct DE_API UberProgramSetup
{
	unsigned SHADOW_MAPS            :8; ///< Number of shadow maps processed in one pass. 0=no shadows, 1=hard shadows, more=soft shadows. Valid values: 0..detectMaxShadowmaps().
	unsigned SHADOW_SAMPLES         :8; ///< Number of samples read from each shadowmap. 0=no shadows, 1=hard shadows, 2,4,8=soft shadows. Valid values: 0,1,2,4,8.
	bool     LIGHT_DIRECT           :1; ///< Enables direct spot light.
	bool     LIGHT_DIRECT_MAP       :1; ///< Enables modulation of direct light by map. Projects texture.
	bool     LIGHT_INDIRECT_CONST   :1; ///< Enables indirect light, constant.
	bool     LIGHT_INDIRECT_VCOLOR  :1; ///< Enables indirect light, set per vertex.
	bool     LIGHT_INDIRECT_VCOLOR2 :1; ///< Enables blend between two ambient vertex colors.
	bool     LIGHT_INDIRECT_VCOLOR_PHYSICAL :1; ///< If indirect light per vertex is used, it is expected in physical scale, converted to screen space in shader.
	bool     LIGHT_INDIRECT_MAP     :1; ///< Enables indirect light, set by ambient map.
	bool     LIGHT_INDIRECT_MAP2    :1; ///< Enables blend between two ambient maps.
	bool     LIGHT_INDIRECT_ENV     :1; ///< Enables indirect light, set by environment map.
	bool     LIGHT_INDIRECT_auto    :1; ///< Extension. Makes RendererOfScene automatically set LIGHT_INDIRECT_VCOLOR and LIGHT_INDIRECT_MAP according to available data. If both vertex and pixel buffers are available, pixel is used.
	bool     MATERIAL_DIFFUSE       :1; ///< Enables material's diffuse reflection.
	bool     MATERIAL_DIFFUSE_CONST :1; ///< Enables material's diffuse reflectance modulated by constant color.
	bool     MATERIAL_DIFFUSE_VCOLOR:1; ///< Enables material's diffuse reflectance modulated by color set per vertex.
	bool     MATERIAL_DIFFUSE_MAP   :1; ///< Enables material's diffuse reflectance modulated by diffuse map.
	bool     MATERIAL_SPECULAR      :1; ///< Enables material's specular reflectance.
	bool     MATERIAL_SPECULAR_CONST:1; ///< Enables material's specular reflectance modulated by constant color.
	bool     MATERIAL_SPECULAR_MAP  :1; ///< Enables specular map, each pixel gets 100% diffuse or 100% specular. Decision is based on contents of diffuse map.
	bool     MATERIAL_NORMAL_MAP    :1; ///< Enables normal map, each pixel's normal is modulated by contents of diffuse map.
	bool     MATERIAL_EMISSIVE_MAP  :1; ///< Enables material's emission stored in sRGB map.
	bool     POSTPROCESS_NORMALS    :1; ///< Renders normal values instead of colors.
	bool     POSTPROCESS_BRIGHTNESS :1; ///< Enables brightness correction of final color (before gamma).
	bool     POSTPROCESS_GAMMA      :1; ///< Enables gamma correction of final color (after brightness).
	bool     POSTPROCESS_BIGSCREEN  :1; ///< Simulates effect of party projected bigscreen with ambient light.
	bool     OBJECT_SPACE           :1; ///< Enables positions in object space, vertices are transformed by uniform worldMatrix.
	bool     CLIPPING               :1; ///< Enables clipping in world space. Not supported by ATI.
	bool     FORCE_2D_POSITION      :1; ///< Overrides projection space vertex coordinates with coordinates read from texcoord7 channel. Triangles are lit as if they stay on their original positions, but they are rendered to externally set positions in texture.

	//! Creates UberProgramSetup with everything turned off by default.
	//! It is suitable for rendering into shadowmap, no color is produced, only depth.
	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
		LIGHT_INDIRECT_VCOLOR_PHYSICAL = true; // this doesn't turn on LIGHT_INDIRECT_VCOLOR, but if YOU turn it on, it will be in physical scale rather than custom
	}

	//! Returns our attribute values in format suitable for our uberprogram.
	//! Example: "#define SHADOW_MAPS 2\n#define SHADOW_SAMPLES 4\n#define MATERIAL_DIFFUSE\n".
	const char* getSetupString();
	//! Compares two instances.
	bool operator ==(const UberProgramSetup& a) const;
	//! Compares two instances.
	bool operator !=(const UberProgramSetup& a) const;
	//! Returns uberProgram with parameter values defined by our attributes.
	//! UberProgram with parameter values defined is Program.
	Program* getProgram(UberProgram* uberProgram);
	//! Returns the highest number of shadowmaps,
	//! that may be processed in one pass with this setup (material & lighting).
	//! The same number is set to SHADOW_MAPS.
	//! \n\n If commandline contains -hard parameter, 1 is returned.
	unsigned detectMaxShadowmaps(UberProgram* uberProgram, int argc = 0, char **argv = NULL);
	//! Sets rendering pipeline so that following primitives are rendered using
	//! our program.
	Program* useProgram(UberProgram* uberProgram, const AreaLight* areaLight, unsigned firstInstance, const Texture* lightDirectMap, const float brightness[4], float gamma);
};


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramData - data for UberProgram
/*
//! Data for UberProgram, options that don't change code.
struct DE_API UberProgramData
{
	AreaLight*areaLight;                 ///< Area light with shadowmaps.
	Texture* lightDirectMap;             ///< Multiplies direct light. This texture is projected by light.
	float    lightIndirectConst[4];      ///< Adds indirect light.
	Texture* lightIndirectMap;           ///< Adds indirect light.
	Texture* lightIndirectDiffuseEnvMap; ///< Adds indirect light.
	Texture* lightIndirectSpecularEnvMap;///< Adds indirect light.
	float    materialDiffuseConst[4];    ///< Multiplies material's diffuse reflectance.
	Texture* materialDiffuseMap;         ///< Multiplies material's diffuse reflectance.
	float    materialSpecularConst[4];   ///< Multiplies material's specular reflectance.
	Texture* materialSpecularMap;        ///< Multiplies material's specular reflectance.
	Texture* materialEmissiveMap;        ///< Adds emission of light.
	float    postprocessBrightness[4];   ///< Multiplies final color (before gamma).
	float    postprocessGamma;           ///< Gamma corrects final color (after brightness).
	float    worldMatrix[16];            ///< Transforms objects from object to world space.

	//! Creates UberProgramData with default values.
	UberProgramData();

	//! Loads light data into program.
	//
	//! Returns false on failure to acquire and feed the data into GPU.
	//! If rendering follows anyway, results are undefined.
	bool feedProgram(const UberProgramSetup& uberProgramSetup, Program* program);
	//bool feedLights(const UberProgramSetup& uberProgramSetup, Program* program);

	//! Loads material data into program.
	//
	//! Returns false on failure to acquire and feed the data into GPU.
	//! If rendering follows anyway, results are undefined.
	//bool feedMaterial(const UberProgramSetup& uberProgramSetup, Program* program);
};
*/

}; // namespace

#endif
