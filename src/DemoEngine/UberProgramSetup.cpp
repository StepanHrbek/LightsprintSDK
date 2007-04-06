// --------------------------------------------------------------------------
// DemoEngine
// UberProgramSetup, settings specific for our single UberProgram.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include "Lightsprint/DemoEngine/UberProgramSetup.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vp+fp


const char* UberProgramSetup::getSetupString()
{
	static char setup[500];
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		SHADOW_MAPS,
		SHADOW_SAMPLES,
		LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
		LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
		LIGHT_INDIRECT_CONST?"#define LIGHT_INDIRECT_CONST\n":"",
		LIGHT_INDIRECT_VCOLOR?"#define LIGHT_INDIRECT_VCOLOR\n":"",
		LIGHT_INDIRECT_MAP?"#define LIGHT_INDIRECT_MAP\n":"",
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
		POSTPROCESS_BRIGHTNESS?"#define POSTPROCESS_BRIGHTNESS\n":"",
		POSTPROCESS_GAMMA?"#define POSTPROCESS_GAMMA\n":"",
		POSTPROCESS_BIGSCREEN?"#define POSTPROCESS_BIGSCREEN\n":"",
		OBJECT_SPACE?"#define OBJECT_SPACE\n":"",
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

unsigned UberProgramSetup::detectMaxShadowmaps(UberProgram* uberProgram)
{
#ifdef DESCEND
	for(SHADOW_MAPS=10;SHADOW_MAPS;SHADOW_MAPS--)
	#define FAIL continue
	#define SUCCESS break
#else
	for(SHADOW_MAPS=1;SHADOW_MAPS<10;SHADOW_MAPS++)
	#define FAIL {SHADOW_MAPS--;break;}
	#define SUCCESS
#endif
	{
		if(!getProgram(uberProgram)) FAIL;
		SUCCESS;
	}
	unsigned instancesPerPassOrig = SHADOW_MAPS;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")))
	{
		const char* buggy[] = {"9500","9550","9600","9700","9800","X300","X550","X600","X700","X740","X800","X850","X1050"};
		for(unsigned i=0;i<sizeof(buggy)/sizeof(char*);i++)
			if(strstr(renderer,buggy[i]))
			{
				if(SHADOW_MAPS>3) SHADOW_MAPS = MAX(3,SHADOW_MAPS-2);
				break;
			}
	}
	// 2 is ugly, prefer 1
	if(SHADOW_MAPS==2) SHADOW_MAPS--;
	printf("Penumbra shadows: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,renderer?renderer:"");
	return SHADOW_MAPS;
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, AreaLight* areaLight, unsigned firstInstance, const Texture* lightDirectMap, const float brightness[4], float gamma)
{
	Program* program = getProgram(uberProgram);
	if(!program) return NULL;
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

	if(LIGHT_DIRECT)
	{
		if(!areaLight) return false;
		const Camera* light = areaLight->getParent();
		if(!light) return false;
		program->sendUniform("worldLightPos",light->pos[0]-0.3f*light->dir[0],light->pos[1]-0.3f*light->dir[1],light->pos[2]-0.3f*light->dir[2]);//!!!
	}

	if(LIGHT_DIRECT_MAP)
	{
		if(!lightDirectMap) return false;
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
		if(!brightness)	return false;
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
