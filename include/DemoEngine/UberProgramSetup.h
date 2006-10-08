// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, parameter management for our UberProgram in UberShader.vp/fp.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef UBERPROGRAMSETUP_H
#define UBERPROGRAMSETUP_H

#include "UberProgram.h"
#include "MultiLight.h"


enum
{
	// textures assigned to UberProgram
	// 0..9 are reserved for shadowmaps
	TEXTURE_2D_LIGHT_DIRECT        = 10,
	TEXTURE_2D_MATERIAL_DIFFUSE    = 11,
	TEXTURE_2D_LIGHT_INDIRECT      = 12,
	TEXTURE_2D_NOISE               = 13,
	TEXTURE_CUBE_LIGHT_INDIRECT    = 14,

	// texcoords assigned to UberProgram
	// these constants are hardcoded in shaders
	MULTITEXCOORD_MATERIAL_DIFFUSE = 0,
	MULTITEXCOORD_LIGHT_INDIRECT   = 1,
	MULTITEXCOORD_FORCED_2D        = 7,
};


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vp+fp

struct RR_API UberProgramSetup
{
	// these values are passed to UberProgram which sets them to shader #defines
	// UberProgram + UberProgramSetup = Program
	unsigned SHADOW_MAPS            :8;
	unsigned SHADOW_SAMPLES         :8;
	bool     NOISE_MAP              :1;
	bool     LIGHT_DIRECT           :1;
	bool     LIGHT_DIRECT_MAP       :1;
	bool     LIGHT_INDIRECT_CONST   :1;
	bool     LIGHT_INDIRECT_COLOR   :1;
	bool     LIGHT_INDIRECT_MAP     :1;
	bool     LIGHT_INDIRECT_ENV     :1;
	bool     MATERIAL_DIFFUSE_COLOR :1;
	bool     MATERIAL_DIFFUSE_MAP   :1;
	bool     FORCE_2D_POSITION      :1;

	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
	}

	const char* getSetupString();
	bool operator ==(const UberProgramSetup& a) const;
	bool operator !=(const UberProgramSetup& a) const;
	Program* getProgram(UberProgram* uberProgram);
	static unsigned detectMaxShadowmaps(UberProgram* uberProgram, unsigned startWith=10);
	bool useProgram(UberProgram* uberProgram, AreaLight* areaLight, unsigned firstInstance, Texture* lightDirectMap, Texture* noiseMap);
};

#endif
