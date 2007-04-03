// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, parameter management for our UberProgram in UberShader.vp/fp.
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
	// 0..9 are reserved for shadowmaps
	TEXTURE_2D_LIGHT_DIRECT              = 10, ///< Sampler id used by our uberprogram for projected light map.
	TEXTURE_2D_MATERIAL_DIFFUSE          = 11, ///< Sampler id used by our uberprogram for diffuse map.
	TEXTURE_2D_MATERIAL_EMISSIVE         = 12, ///< Sampler id used by our uberprogram for emissive map.
	TEXTURE_2D_LIGHT_INDIRECT            = 13, ///< Sampler id used by our uberprogram for ambient map.
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
// UberProgramSetup - options for UberShader.vp+fp

//! Options for DemoEngine's ubershader UberShader.vp and UberShader.fp.
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
	bool     LIGHT_INDIRECT_MAP     :1; ///< Enables indirect light, set by ambient map.
	bool     LIGHT_INDIRECT_ENV     :1; ///< Enables indirect light, set by environment map.
	bool     MATERIAL_DIFFUSE       :1; ///< Enables material's diffuse reflection.
	bool     MATERIAL_DIFFUSE_CONST :1; ///< Enables material's diffuse reflectance modulated by constant color.
	bool     MATERIAL_DIFFUSE_VCOLOR:1; ///< Enables material's diffuse reflectance modulated by color set per vertex.
	bool     MATERIAL_DIFFUSE_MAP   :1; ///< Enables material's diffuse reflectance modulated by diffuse map.
	bool     MATERIAL_SPECULAR      :1; ///< Enables material's specular reflectance.
	bool     MATERIAL_SPECULAR_MAP  :1; ///< Enables specular map, each pixel gets 100% diffuse or 100% specular. Decision is based on contents of diffuse map.
	bool     MATERIAL_NORMAL_MAP    :1; ///< Enables normal map, each pixel's normal is modulated by contents of diffuse map.
	bool     MATERIAL_EMISSIVE_MAP  :1; ///< Enables material's emission stored in sRGB map.
	bool     OBJECT_SPACE           :1; ///< Enables object space, vertices are transformed by uniform worldMatrix.
	bool     FORCE_2D_POSITION      :1; ///< Overrides projection space vertex coordinates with coordinates read from texcoord7 channel. Triangles are lit as if they stay on their original positions, but they are rendered to externally set positions in texture.


	//! Creates UberProgramSetup with everything turned off by default.
	//! It is suitable for rendering into shadowmap, no color is produced, only depth.
	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
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
	//! Returns the highest number of shadowmas that may be processed in one pass
	//! under standard conditions of diffuse material texture, projected light texture, indirect illumination per vertex.
	//! If ambientMaps is true, indirect illumination in ambient maps is taken into account.
	static unsigned detectMaxShadowmaps(UberProgram* uberProgram, bool ambientMaps);
	//! Sets rendering pipeline so that following primitives are rendered using
	//! our program.
	Program* useProgram(UberProgram* uberProgram, AreaLight* areaLight, unsigned firstInstance, Texture* lightDirectMap);
};

}; // namespace

#endif
