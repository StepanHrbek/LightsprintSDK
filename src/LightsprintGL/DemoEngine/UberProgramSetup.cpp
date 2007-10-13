// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, settings specific for our single UberProgram.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vs/fs


const char* UberProgramSetup::getSetupString()
{
	static bool SHADOW_BILINEAR = true;
	LIMITED_TIMES(1,char* renderer = (char*)glGetString(GL_RENDERER);if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON"))) SHADOW_BILINEAR = false);

	static char setup[1000];
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		SHADOW_MAPS,
		SHADOW_SAMPLES,
		SHADOW_BILINEAR?"#define SHADOW_BILINEAR\n":"",
		LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
		LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
		LIGHT_INDIRECT_CONST?"#define LIGHT_INDIRECT_CONST\n":"",
		LIGHT_INDIRECT_VCOLOR?"#define LIGHT_INDIRECT_VCOLOR\n":"",
		LIGHT_INDIRECT_VCOLOR2?"#define LIGHT_INDIRECT_VCOLOR2\n":"",
		LIGHT_INDIRECT_VCOLOR_PHYSICAL?"#define LIGHT_INDIRECT_VCOLOR_PHYSICAL\n":"",
		LIGHT_INDIRECT_MAP?"#define LIGHT_INDIRECT_MAP\n":"",
		LIGHT_INDIRECT_MAP2?"#define LIGHT_INDIRECT_MAP2\n":"",
		LIGHT_INDIRECT_ENV?"#define LIGHT_INDIRECT_ENV\n":"",
		MATERIAL_DIFFUSE?"#define MATERIAL_DIFFUSE\n":"",
		MATERIAL_DIFFUSE_CONST?"#define MATERIAL_DIFFUSE_CONST\n":"",
		MATERIAL_DIFFUSE_VCOLOR?"#define MATERIAL_DIFFUSE_VCOLOR\n":"",
		MATERIAL_DIFFUSE_MAP?"#define MATERIAL_DIFFUSE_MAP\n":"",
		MATERIAL_SPECULAR?"#define MATERIAL_SPECULAR\n":"",
		MATERIAL_SPECULAR_CONST?"#define MATERIAL_SPECULAR_CONST\n":"",
		MATERIAL_SPECULAR_MAP?"#define MATERIAL_SPECULAR_MAP\n":"",
		MATERIAL_NORMAL_MAP?"#define MATERIAL_NORMAL_MAP\n":"",
		MATERIAL_EMISSIVE_MAP?"#define MATERIAL_EMISSIVE_MAP\n":"",
		POSTPROCESS_NORMALS?"#define POSTPROCESS_NORMALS\n":"",
		POSTPROCESS_BRIGHTNESS?"#define POSTPROCESS_BRIGHTNESS\n":"",
		POSTPROCESS_GAMMA?"#define POSTPROCESS_GAMMA\n":"",
		POSTPROCESS_BIGSCREEN?"#define POSTPROCESS_BIGSCREEN\n":"",
		OBJECT_SPACE?"#define OBJECT_SPACE\n":"",
		CLIPPING?"#define CLIPPING\n":"",
		FORCE_2D_POSITION?"#define FORCE_2D_POSITION\n":""
		);
	return setup;
}

bool UberProgramSetup::operator ==(const UberProgramSetup& a) const
{
	return memcmp(this,&a,sizeof(*this))==0;
}
bool UberProgramSetup::operator !=(const UberProgramSetup& a) const
{
	return memcmp(this,&a,sizeof(*this))!=0;
}

Program* UberProgramSetup::getProgram(UberProgram* uberProgram)
{
	return uberProgram->getProgram(getSetupString());
}

unsigned UberProgramSetup::detectMaxShadowmaps(UberProgram* uberProgram, int argc, const char*const*argv)
{
	while(argc--)
	{
		if(!strcmp(argv[argc],"-hard")) return 1;
		int tmp;
		if(sscanf(argv[argc],"penumbra%d",&tmp)==1)
		{
			SHADOW_MAPS = tmp;
			if(!getProgram(uberProgram)) 
			{
				rr::RRReporter::report(rr::ERRO,"GPU is not able to produce given penumbra quality, set lower quality.\n");
				SHADOW_MAPS = 0;
				return 0;
			}
			return tmp;
		}
	}
	// try max 9 maps, we must fit all maps in ubershader to 16 (maximum allowed by ATI)
#ifdef DESCEND
	for(SHADOW_MAPS=9;SHADOW_MAPS;SHADOW_MAPS--)
	#define FAIL continue
	#define SUCCESS break
	#define RESULT SHADOW_MAPS
#else
	for(SHADOW_MAPS=1;SHADOW_MAPS<=9;SHADOW_MAPS++)
	#define FAIL break
	#define SUCCESS
	#define RESULT SHADOW_MAPS-1
#endif
	{
		if(!getProgram(uberProgram)) FAIL;
		SUCCESS;
	}
	unsigned instancesPerPassOrig = SHADOW_MAPS = RESULT;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")))
	{
		const char* buggy[] = {"9500","9550","9600","9700","9800","X300","X550","X600","X700","X740","X800","X850","X1050"};
		for(unsigned i=0;i<sizeof(buggy)/sizeof(char*);i++)
			if(strstr(renderer,buggy[i]))
			{
				if(SHADOW_MAPS>3) SHADOW_MAPS -= 2; // 5->3 (X300 in PenumbraShadows), 4->2 (X300 in Lightmaps)
				break;
			}
		// with bilinear filter ignored by Radeon, 3 is ugly, prefer 1
		if(SHADOW_MAPS==3) SHADOW_MAPS--;
	}
	if(renderer && (strstr(renderer,"GeForce 6")||strstr(renderer,"GeForce 7"))) // 7->6 (6150 in PenumbraShadows)
	{
		if(SHADOW_MAPS) SHADOW_MAPS--;
	}
	// 2 is ugly, prefer 1
	if(SHADOW_MAPS==2) SHADOW_MAPS--;
	rr::RRReporter::report(rr::INF2,"Penumbra quality: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,renderer?renderer:"");
	return SHADOW_MAPS;
}

void UberProgramSetup::validate()
{
	if(!LIGHT_DIRECT)
	{
		SHADOW_MAPS = 0;
		SHADOW_SAMPLES = 0;
		LIGHT_DIRECT_MAP = 0;
	}
	if(!LIGHT_INDIRECT_VCOLOR)
	{
		LIGHT_INDIRECT_VCOLOR2 = 0;
	}
	if(!LIGHT_INDIRECT_MAP)
	{
		LIGHT_INDIRECT_MAP2 = 0;
	}
	if(!MATERIAL_DIFFUSE)
	{
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_VCOLOR = 0;
		MATERIAL_DIFFUSE_MAP = 0;
	}
	if(!MATERIAL_SPECULAR)
	{
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
	}
	bool black = !(LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV);
	if(black)
	{
		UberProgramSetup uberProgramSetupBlack;
		uberProgramSetupBlack.OBJECT_SPACE = OBJECT_SPACE;
		uberProgramSetupBlack.CLIPPING = CLIPPING;
		uberProgramSetupBlack.FORCE_2D_POSITION = FORCE_2D_POSITION;
		*this = uberProgramSetupBlack;
	}
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, const AreaLight* areaLight, unsigned firstInstance, const Texture* lightDirectMap, const float brightness[4], float gamma)
{
	Program* program = getProgram(uberProgram);
	if(!program)
	{
		rr::RRReporter::report(rr::ERRO,"useProgram: failed to compile or link GLSL shader.\n");
		return NULL;
	}
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
		if(!areaLight)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: areaLight==NULL.\n");
			return false;
		}
		glActiveTexture(GL_TEXTURE0+i);
		// prepare samplers
		areaLight->getShadowMap(firstInstance+i)->bindTexture();
		//samplers[i]=i; // for array of samplers (needs OpenGL 2.0 compliant card)
		char name[] = "shadowMap0"; // for individual samplers (works on buggy ATI)
		name[9] = '0'+i; // for individual samplers (works on buggy ATI)
		program->sendUniform(name, (int)i); // for individual samplers (works on buggy ATI)
		// prepare and send matrices
		Camera* lightInstance = areaLight->getInstance(firstInstance+i,true);
		glLoadMatrixd(tmp);
		glMultMatrixd(lightInstance->frustumMatrix);
		glMultMatrixd(lightInstance->viewMatrix);
		delete lightInstance;
	}
	//myProg->sendUniform("shadowMap", instances, samplers); // for array of samplers (needs OpenGL 2.0 compliant card)
	glMatrixMode(GL_MODELVIEW);

	if(LIGHT_DIRECT)
	{
		if(!areaLight)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: areaLight==NULL.\n");
			return false;
		}
		const Camera* light = areaLight->getParent();
		if(!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: areaLight->getParent()==NULL.\n");
			return false;
		}
		program->sendUniform("worldLightPos",light->pos[0]-0.3f*light->dir[0],light->pos[1]-0.3f*light->dir[1],light->pos[2]-0.3f*light->dir[2]);//!!!
	}

	if(LIGHT_DIRECT_MAP)
	{
		if(!lightDirectMap)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: lightDirectMap==NULL (projected texture is missing).\n");
			return false;
		}
		int id=TEXTURE_2D_LIGHT_DIRECT;
		glActiveTexture(GL_TEXTURE0+id);
		lightDirectMap->bindTexture();
		program->sendUniform("lightDirectMap", id);
	}

	if(LIGHT_INDIRECT_CONST)
	{
		program->sendUniform("lightIndirectConst",0.2f,0.2f,0.2f,0.0f);
	}

	if(LIGHT_INDIRECT_MAP)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectMap", id);
	}

	if(LIGHT_INDIRECT_MAP2)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT2;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectMap2", id);
	}

	if(LIGHT_INDIRECT_ENV)
	{
		if(MATERIAL_DIFFUSE)
		{
			int id=TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE;
			//glActiveTexture(GL_TEXTURE0+id);
			program->sendUniform("lightIndirectDiffuseEnvMap", id);
		}
		if(MATERIAL_SPECULAR)
		{
			int id=TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR;
			//glActiveTexture(GL_TEXTURE0+id);
			program->sendUniform("lightIndirectSpecularEnvMap", id);
		}
	}

	if(MATERIAL_DIFFUSE_CONST)
	{
		// set default value, caller may override it by additional sendUniform call
		program->sendUniform("materialDiffuseConst",2.0f,2.0f,2.0f,1.0f);
	}

	if(MATERIAL_DIFFUSE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_DIFFUSE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active
		program->sendUniform("materialDiffuseMap", id);
	}

	if(MATERIAL_SPECULAR_CONST)
	{
		// set default value, caller may override it by additional sendUniform call
		program->sendUniform("materialSpecularConst",.5f,.5f,.5f,1.0f);
	}

	if(MATERIAL_EMISSIVE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_EMISSIVE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active (EMISSIVE is typically used without DIFFUSE)
		program->sendUniform("materialEmissiveMap", id);
	}

	if(POSTPROCESS_BRIGHTNESS)
	{
		if(!brightness)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: brightness==NULL.\n");
			return false;
		}
		program->sendUniform4fv("postprocessBrightness", brightness);
	}

	if(POSTPROCESS_GAMMA)
	{
		program->sendUniform("postprocessGamma", gamma);
	}

	return program;
}

/*
/////////////////////////////////////////////////////////////////////////////
//
// UberProgramData - data for UberProgram

UberProgramData::UberProgramData()
{
	memset(this,0,sizeof(*this));
	materialDiffuseConst[0] = 1;
	materialDiffuseConst[1] = 1;
	materialDiffuseConst[2] = 1;
	materialDiffuseConst[3] = 1;
	materialSpecularConst[0] = 1;
	materialSpecularConst[1] = 1;
	materialSpecularConst[2] = 1;
	materialSpecularConst[3] = 1;
	postprocessBrightness[0] = 1;
	postprocessBrightness[1] = 1;
	postprocessBrightness[2] = 1;
	postprocessBrightness[3] = 1;
	postprocessGamma = 1;
	worldMatrix[0] = 1;
	worldMatrix[5] = 1;
	worldMatrix[10] = 1;
	worldMatrix[15] = 1;
}

bool UberProgramData::feedProgram(const UberProgramSetup& uberProgramSetup, Program* program)
{
	// shadowMap[], gl_TextureMatrix[]
	unsigned firstInstance = 0;
	glMatrixMode(GL_TEXTURE);
	GLdouble tmp[16]={
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		1,1,1,2
	};
	//GLint samplers[100]; // for array of samplers (needs OpenGL 2.0 compliant card)
	for(unsigned i=0;i<uberProgramSetup.SHADOW_MAPS;i++)
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

	if(uberProgramSetup.LIGHT_DIRECT)
	{
		if(!areaLight) return false;
		const Camera* light = areaLight->getParent();
		if(!light) return false;
		program->sendUniform("worldLightPos",light->pos[0]-0.3f*light->dir[0],light->pos[1]-0.3f*light->dir[1],light->pos[2]-0.3f*light->dir[2]);//!!!
	}

	if(uberProgramSetup.LIGHT_DIRECT_MAP)
	{
		if(!lightDirectMap) return false;
		int id=TEXTURE_2D_LIGHT_DIRECT;
		glActiveTexture(GL_TEXTURE0+id);
		lightDirectMap->bindTexture();
		program->sendUniform("lightDirectMap", id);
	}

	if(uberProgramSetup.LIGHT_INDIRECT_CONST)
	{
		program->sendUniform4fv("lightIndirectConst",lightIndirectConst);
	}

	if(uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT;
		glActiveTexture(GL_TEXTURE0+id);
		lightIndirectMap->bindTexture();
	}

	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		if(uberProgramSetup.MATERIAL_DIFFUSE)
		{
			int id=TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE;
			glActiveTexture(GL_TEXTURE0+id);
			lightIndirectDiffuseEnvMap->bindTexture();
		}
		if(uberProgramSetup.MATERIAL_SPECULAR)
		{
			int id=TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR;
			glActiveTexture(GL_TEXTURE0+id);
			lightIndirectSpecularEnvMap->bindTexture();
		}
	}

	if(uberProgramSetup.MATERIAL_DIFFUSE_CONST)
	{
		program->sendUniform4fv("materialDiffuseConst",materialDiffuseConst);
	}

	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_DIFFUSE;
		glActiveTexture(GL_TEXTURE0+id);
		materialDiffuseMap->bindTexture();
	}

	if(uberProgramSetup.MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform4fv("materialSpecularConst",materialSpecularConst);
	}

	if(uberProgramSetup.MATERIAL_EMISSIVE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_EMISSIVE;
		glActiveTexture(GL_TEXTURE0+id);
		materialEmissiveMap->bindTexture();
	}

	return true;
}
*/
}; // namespace
