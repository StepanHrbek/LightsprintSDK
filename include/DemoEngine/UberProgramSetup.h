// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, parameter management for our UberProgram in UberShader.vp/fp.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef UBERPROGRAMSETUP_H
#define UBERPROGRAMSETUP_H

#include "UberProgram.h"
#include "MultiLight.h"


enum
{
	// textures assigned for UberProgram
	// 0..9 are reserved for shadowmaps
	TEXTURE_2D_LIGHT_DIRECT        = 10,
	TEXTURE_2D_MATERIAL_DIFFUSE    = 11,
	TEXTURE_2D_LIGHT_INDIRECT      = 12,
	TEXTURE_2D_NOISE               = 13,

	// texcoords assigned for UberProgram
	// these constants are hardcoded in shaders
	MULTITEXCOORD_MATERIAL_DIFFUSE = 0,
	MULTITEXCOORD_LIGHT_INDIRECT   = 1,
	MULTITEXCOORD_FORCED_2D        = 7,
};


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vp+fp

struct UberProgramSetup
{
	// these values are passed to UberProgram which sets them to shader #defines
	// UberProgram + UberProgramSetup = Program
	unsigned SHADOW_MAPS            :8;
	unsigned SHADOW_SAMPLES         :8;
	bool     NOISE_MAP              :1;
	bool     LIGHT_DIRECT           :1;
	bool     LIGHT_DIRECT_MAP       :1;
	bool     LIGHT_INDIRECT_COLOR   :1;
	bool     LIGHT_INDIRECT_MAP     :1;
	bool     MATERIAL_DIFFUSE_COLOR :1;
	bool     MATERIAL_DIFFUSE_MAP   :1;
	bool     FORCE_2D_POSITION      :1;

	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
	}

	const char* getSetupString()
	{
		static char setup[300];
		sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s",
			SHADOW_MAPS,
			SHADOW_SAMPLES,
			NOISE_MAP?"#define NOISE_MAP\n":"",
			LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
			LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
			LIGHT_INDIRECT_COLOR?"#define LIGHT_INDIRECT_COLOR\n":"",
			LIGHT_INDIRECT_MAP?"#define LIGHT_INDIRECT_MAP\n":"",
			MATERIAL_DIFFUSE_COLOR?"#define MATERIAL_DIFFUSE_COLOR\n":"",
			MATERIAL_DIFFUSE_MAP?"#define MATERIAL_DIFFUSE_MAP\n":"",
			FORCE_2D_POSITION?"#define FORCE_2D_POSITION\n":""
			);
		return setup;
	}

	bool operator ==(const UberProgramSetup& a) const
	{
		return memcmp(this,&a,sizeof(*this))==0;
	}
	bool operator !=(const UberProgramSetup& a) const
	{
		return memcmp(this,&a,sizeof(*this))!=0;
	}

	Program* getProgram(UberProgram* uberProgram)
	{
		return uberProgram->getProgram(getSetupString());
	}

	static unsigned detectMaxShadowmaps(UberProgram* uberProgram, unsigned startWith=10)
	{
		unsigned instancesPerPass;
		for(instancesPerPass=startWith;instancesPerPass;instancesPerPass--)
		{
			// maximize use of samplers
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = instancesPerPass;
			uberProgramSetup.SHADOW_SAMPLES = 4;
			uberProgramSetup.NOISE_MAP = true;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_DIRECT_MAP = true;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
			uberProgramSetup.LIGHT_INDIRECT_MAP = false;
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = true;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.FORCE_2D_POSITION = false;
			if(!uberProgramSetup.getProgram(uberProgram)) continue;
			// maximize use of interpolators
			uberProgramSetup.NOISE_MAP = false;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
			uberProgramSetup.LIGHT_INDIRECT_MAP = true;
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
			if(uberProgramSetup.getProgram(uberProgram)) break;
		}
//		if(instancesPerPass>1) instancesPerPass--;
//		if(instancesPerPass>1) instancesPerPass--;
		return instancesPerPass;
	}

	bool useProgram(UberProgram* uberProgram, AreaLight* areaLight, unsigned firstInstance, Texture* lightDirectMap, Texture* noiseMap)
	{
		Program* program = getProgram(uberProgram);
		if(!program) return false;
		program->useIt();

		// shadowMap[], gl_TextureMatrix[]
		glMatrixMode(GL_TEXTURE);
		GLdouble tmp[16]={
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			1,1,1,2
		};
		//GLint samplers[100]; // for array of samplers (needs OpenGL 2.0 compliant card)
		for(unsigned i=0;i<SHADOW_MAPS;i++)
		{
			if(!areaLight) return false;
			glActiveTexture(GL_TEXTURE0+i);
			// prepare samplers
			areaLight->getShadowMap(firstInstance+i)->bindTexture();
			//samplers[i]=i; // for array of samplers (needs OpenGL 2.0 compliant card)
			char name[] = "shadowMap0"; // for individual samplers (works on buggy ATI)
			name[9] = '0'+i; // for individual samplers (works on buggy ATI)
			program->sendUniform(name, (int)i); // for individual samplers (works on buggy ATI)
			// prepare and send matrices
			Camera* lightInstance = areaLight->getInstance(firstInstance+i);
			glLoadMatrixd(tmp);
			glMultMatrixd(lightInstance->frustumMatrix);
			glMultMatrixd(lightInstance->viewMatrix);
			delete lightInstance;
		}
		//myProg->sendUniform("shadowMap", instances, samplers); // for array of samplers (needs OpenGL 2.0 compliant card)
		glMatrixMode(GL_MODELVIEW);

		if(NOISE_MAP)
		{
			if(!noiseMap) return false;
			int id=TEXTURE_2D_NOISE;
			glActiveTexture(GL_TEXTURE0+id);
			noiseMap->bindTexture();
			program->sendUniform("noiseMap",id);
		}

		// lightDirectPos (in object space)
		if(LIGHT_DIRECT)
		{
			if(!areaLight) return false;
			const Camera* light = areaLight->getParent();
			if(!light) return false;
			program->sendUniform("lightDirectPos",light->pos[0]-0.3f*light->dir[0],light->pos[1]-0.3f*light->dir[1],light->pos[2]-0.3f*light->dir[2]);//!!!
		}

		// lightDirectMap
		if(LIGHT_DIRECT_MAP)
		{
			if(!lightDirectMap) return false;
			int id=TEXTURE_2D_LIGHT_DIRECT;
			glActiveTexture(GL_TEXTURE0+id);
			lightDirectMap->bindTexture();
			program->sendUniform("lightDirectMap", id);
		}

		// lightIndirectMap
		if(LIGHT_INDIRECT_MAP)
		{
			int id=TEXTURE_2D_LIGHT_INDIRECT;
			//glActiveTexture(GL_TEXTURE0+id);
			program->sendUniform("lightIndirectMap", id);
		}

		// materialDiffuseMap
		if(MATERIAL_DIFFUSE_MAP)
		{
			int id=TEXTURE_2D_MATERIAL_DIFFUSE;
			glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active
			program->sendUniform("materialDiffuseMap", id);
		}

		return true;
	}
};

#endif
