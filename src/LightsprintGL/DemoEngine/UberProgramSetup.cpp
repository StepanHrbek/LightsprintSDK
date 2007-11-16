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
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		SHADOW_MAPS,
		SHADOW_SAMPLES,
		SHADOW_BILINEAR?"#define SHADOW_BILINEAR\n":"",
		SHADOW_PENUMBRA?"#define SHADOW_PENUMBRA\n":"",
		LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
		LIGHT_DIRECT_COLOR?"#define LIGHT_DIRECT_COLOR\n":"",
		LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
		LIGHT_DISTANCE_PHYSICAL?"#define LIGHT_DISTANCE_PHYSICAL\n":"",
		LIGHT_DISTANCE_POLYNOMIAL?"#define LIGHT_DISTANCE_POLYNOMIAL\n":"",
		LIGHT_DISTANCE_EXPONENTIAL?"#define LIGHT_DISTANCE_EXPONENTIAL\n":"",
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
		ANIMATION_WAVE?"#define ANIMATION_WAVE\n":"",
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
		int tmp;
		if(sscanf(argv[argc],"penumbra%d",&tmp)==1 && tmp>=1 && tmp<=8) // accept only penumbra1..8
		{
			SHADOW_MAPS = tmp;
			if(tmp<1 || tmp>8 || !getProgram(uberProgram)) 
			{
				rr::RRReporter::report(rr::ERRO,"GPU is not able to produce given penumbra quality, set lower quality.\n");
				SHADOW_MAPS = 0;
				return 0;
			}
			return tmp;
		}
	}
	// try max 9 maps, we must fit all maps in ubershader to 16 (maximum allowed by ATI)
	// no, make it shorter, try max 8 maps, both AMD and NVIDIA high end GPUs can do only 8
	for(SHADOW_MAPS=1;SHADOW_MAPS<=8;SHADOW_MAPS++)
	{
		Program* program = getProgram(uberProgram);
		if(!program // stop when !compiled or !linked
			|| (LIGHT_DIRECT && !program->uniformExists("worldLightPos")) // stop when uniform missing, workaround for Nvidia bug
			) break;
	}
	unsigned instancesPerPassOrig = --SHADOW_MAPS;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if(renderer)
	{
		// find 4digit number
		unsigned number = 0;
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for(unsigned i=0;renderer[i];i++)
			if(!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}

		// workaround for Catalyst bug, observed on X300, X1250
		if( (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")) && !(number>=1300 && number<=4999) )
			if(SHADOW_MAPS>3) SHADOW_MAPS -= 2; // 5->3 (X300 in PenumbraShadows), 4->2 (X300 in Lightmaps)

		// workaround for ForceWare bug, observed on 5xxx, 6xxx, 7xxx, Go 7xxx
		//if( (strstr(renderer,"GeForce")||strstr(renderer,"GEFORCE")) && (number>=5000 && number<=7999) )
		//	if(SHADOW_MAPS) SHADOW_MAPS--; // 7->6 (6150 in PenumbraShadows)
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
/*
void UberProgramSetup::setLightDirect(const RealtimeLight* light, const Texture* lightDirectMap)
{
	SHADOW_MAPS = light ? light->getNumInstances() : 0;
	SHADOW_SAMPLES = light ? MAX(SHADOW_SAMPLES,1) : 0;
	SHADOW_PENUMBRA = light && light->areaType!=RealtimeLight::POINT;
	LIGHT_DIRECT = light ? true : false;
	LIGHT_DIRECT_COLOR = light && light->origin && light->origin->color!=rr::RRVec3(1);
	LIGHT_DIRECT_MAP = light && light->areaType!=RealtimeLight::POINT && lightDirectMap;
	LIGHT_DISTANCE_PHYSICAL = light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
	LIGHT_DISTANCE_POLYNOMIAL = light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
	LIGHT_DISTANCE_EXPONENTIAL = light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
}

void UberProgramSetup::setLightIndirect()
{
	LIGHT_INDIRECT_CONST = 0;
	LIGHT_INDIRECT_VCOLOR = 0;
	LIGHT_INDIRECT_VCOLOR2 = 0;
	LIGHT_INDIRECT_VCOLOR_PHYSICAL = 0;
	LIGHT_INDIRECT_MAP = 0;
	LIGHT_INDIRECT_MAP2 = 0;
	LIGHT_INDIRECT_ENV = 0;
	LIGHT_INDIRECT_auto = 0;
}

void UberProgramSetup::setMaterial(const rr::RRMaterial* material)
{
	MATERIAL_DIFFUSE = 0;
	MATERIAL_DIFFUSE_CONST = 0;
	MATERIAL_DIFFUSE_VCOLOR = 0;
	MATERIAL_DIFFUSE_MAP = 0;
	MATERIAL_SPECULAR = 0;
	MATERIAL_SPECULAR_CONST = 0;
	MATERIAL_SPECULAR_MAP = 0;
	MATERIAL_NORMAL_MAP = 0;
	MATERIAL_EMISSIVE_MAP = 0;
}

void UberProgramSetup::setPostprocess(const rr::RRVec4* brightness, float gamma)
{
	//POSTPROCESS_NORMALS = 0;
	POSTPROCESS_BRIGHTNESS = brightness && *brightness!=rr::RRVec4(1);
	POSTPROCESS_GAMMA = gamma!=1;
	//POSTPROCESS_BIGSCREEN = 0;
}*/

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, const RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma)
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
		if(!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		glActiveTexture(GL_TEXTURE0+i);
		// prepare samplers
		light->getShadowMap(firstInstance+i)->bindTexture();
		//samplers[i]=i; // for array of samplers (needs OpenGL 2.0 compliant card)
		char name[] = "shadowMap0"; // for individual samplers (works on buggy ATI)
		name[9] = '0'+i; // for individual samplers (works on buggy ATI)
		program->sendUniform(name, (int)i); // for individual samplers (works on buggy ATI)
		// prepare and send matrices
		Camera* lightInstance = light->getInstance(firstInstance+i,true);
		glLoadMatrixd(tmp);
		glMultMatrixd(lightInstance->frustumMatrix);
		glMultMatrixd(lightInstance->viewMatrix);
		delete lightInstance;
	}
	//myProg->sendUniform("shadowMap", instances, samplers); // for array of samplers (needs OpenGL 2.0 compliant card)
	glMatrixMode(GL_MODELVIEW);

	if(LIGHT_DIRECT)
	{
		if(!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		if(!light->getParent())
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->getParent()==NULL.\n");
			return false;
		}
		program->sendUniform("worldLightPos",light->getParent()->pos[0],light->getParent()->pos[1],light->getParent()->pos[2]);
	}

	if(LIGHT_DIRECT_COLOR)
	{
		if(!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		if(!light->origin)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->origin==NULL.\n");
			return false;
		}
		rr::RRVec3 color = light->origin->color;
		if(light->origin->distanceAttenuationType==rr::RRLight::NONE || light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL)
		{
			color[0] = pow(color[0],0.45f);
			color[1] = pow(color[1],0.45f);
			color[2] = pow(color[2],0.45f);
		}
		program->sendUniform("lightDirectColor",color[0],color[1],color[2],1.0f);
	}

	if(LIGHT_DIRECT_MAP)
	{
		if(!light->lightDirectMap)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: lightDirectMap==NULL (projected texture is missing).\n");
			return false;
		}
		int id=TEXTURE_2D_LIGHT_DIRECT;
		glActiveTexture(GL_TEXTURE0+id);
		light->lightDirectMap->bindTexture();
		program->sendUniform("lightDirectMap", id);
	}

	if(LIGHT_DISTANCE_PHYSICAL)
	{
		RR_ASSERT(light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL);
	}

	if(LIGHT_DISTANCE_POLYNOMIAL)
	{
		if(!light->origin)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->origin==NULL.\n");
			return false;
		}
		RR_ASSERT(light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL);
		program->sendUniform("lightDistancePolynom",light->origin->polynom.x,light->origin->polynom.y,light->origin->polynom.z);
	}

	if(LIGHT_DISTANCE_EXPONENTIAL)
	{
		if(!light->origin)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->origin==NULL.\n");
			return false;
		}
		RR_ASSERT(light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL);
		program->sendUniform("lightDistanceRadius",light->origin->radius);
		program->sendUniform("lightDistanceFallOffExponent",light->origin->fallOffExponent);
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
		program->sendUniform4fv("postprocessBrightness", &brightness->x);
	}

	if(POSTPROCESS_GAMMA)
	{
		program->sendUniform("postprocessGamma", gamma);
	}

	return program;
}

}; // namespace
