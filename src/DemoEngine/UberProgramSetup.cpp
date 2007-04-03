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
	static char setup[300];
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
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
		MATERIAL_SPECULAR_MAP?"#define MATERIAL_SPECULAR_MAP\n":"",
		MATERIAL_NORMAL_MAP?"#define MATERIAL_NORMAL_MAP\n":"",
		MATERIAL_EMISSIVE_MAP?"#define MATERIAL_EMISSIVE_MAP\n":"",
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

unsigned UberProgramSetup::detectMaxShadowmaps(UberProgram* uberProgram, bool ambientMaps)
{
	unsigned instancesPerPass;
#ifdef DESCEND
	for(instancesPerPass=10;instancesPerPass;instancesPerPass--)
	#define FAIL continue
	#define SUCCESS break
#else
	for(instancesPerPass=1;instancesPerPass<10;instancesPerPass++)
	#define FAIL {instancesPerPass--;break;}
	#define SUCCESS
#endif
	{
		// static object with light_indirect in vertex colors
		UberProgramSetup uberProgramSetup;
		uberProgramSetup.SHADOW_MAPS = instancesPerPass;
		uberProgramSetup.SHADOW_SAMPLES = 4;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_MAP = true;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = false;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
		uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = false;
		if(!uberProgramSetup.getProgram(uberProgram)) FAIL;
		// static object with light_indirect in ambient maps
		if(ambientMaps)
		{
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
			uberProgramSetup.LIGHT_INDIRECT_MAP = true;
			if(!uberProgramSetup.getProgram(uberProgram)) FAIL;
		}
		/*/ dynamic object
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = true;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = false;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
		uberProgramSetup.MATERIAL_SPECULAR = true;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = true;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
		uberProgramSetup.OBJECT_SPACE = true;
		if(!uberProgramSetup.getProgram(uberProgram)) FAIL;*/
		// all shaders ok
		SUCCESS;
	}
	unsigned instancesPerPassOrig = instancesPerPass;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")))
	{
		const char* buggy[] = {"9500","9550","9600","9700","9800","X300","X550","X600","X700","X740","X800","X850","X1050"};
		for(unsigned i=0;i<sizeof(buggy)/sizeof(char*);i++)
			if(strstr(renderer,buggy[i]))
			{
				if(instancesPerPass>3) instancesPerPass = MAX(3,instancesPerPass-2);
				break;
			}
	}
	// 2 is ugly, prefer 1
	if(instancesPerPass==2) instancesPerPass--;
	printf("Penumbra shadows: %d/%d on %s.\n",instancesPerPass,instancesPerPassOrig,renderer?renderer:"");
	return instancesPerPass;
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, AreaLight* areaLight, unsigned firstInstance, Texture* lightDirectMap)
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

	// lightDirectPos (in object space)
	if(LIGHT_DIRECT)
	{
		if(!areaLight) return false;
		const Camera* light = areaLight->getParent();
		if(!light) return false;
		program->sendUniform("worldLightPos",light->pos[0]-0.3f*light->dir[0],light->pos[1]-0.3f*light->dir[1],light->pos[2]-0.3f*light->dir[2]);//!!!
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

	if(LIGHT_INDIRECT_CONST)
	{
		program->sendUniform("lightIndirectConst",0.2f,0.2f,0.2f,0.0f);
	}

	// lightIndirectMap
	if(LIGHT_INDIRECT_MAP)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectMap", id);
	}

	// lightIndirectEnvMap
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

	// materialDiffuseMap
	if(MATERIAL_DIFFUSE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_DIFFUSE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active
		program->sendUniform("materialDiffuseMap", id);
	}

	// materialEmissiveMap
	if(MATERIAL_EMISSIVE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_EMISSIVE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active (EMISSIVE is typically used without DIFFUSE)
		program->sendUniform("materialEmissiveMap", id);
	}

	return program;
}

}; // namespace
