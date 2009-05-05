// --------------------------------------------------------------------------
// UberProgramSetup, settings specific for our single UberProgram.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
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

void UberProgramSetup::recommendMaterialSetup(rr::RRObject* object)
{
	// update MATERIAL_* recommendations
	unsigned numTrianglesWithDifConst = 0;
	unsigned numTrianglesWithDifMap = 0;
	unsigned numTrianglesWithSpecConst = 0;
	unsigned numTrianglesWithSpecMap = 0;
	unsigned numTrianglesWithEmiConst = 0;
	unsigned numTrianglesWithEmiMap = 0;
	unsigned numTrianglesWithConstTransp = 0;
	unsigned numTrianglesWithMapRGBTransp = 0;
	unsigned numTrianglesWithDifMapATransp = 0;
	unsigned numTrianglesWithNonDifMapATransp = 0;
	unsigned numTrianglesWithTranspBlend = 0;
	unsigned numTrianglesWithTranspKeyed = 0;
	unsigned numTriangles01Sided = 0;
	unsigned numTriangles2Sided = 0;
	if (object)
	{
		unsigned numTrianglesMulti = object->getCollider()->getMesh()->getNumTriangles();
		for (unsigned t=0;t<numTrianglesMulti;t++)
		{
			const rr::RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
			if (material)
			{
				// dif
				if (material->diffuseReflectance.texture)
				{
					numTrianglesWithDifMap++;
				}
				else
				if (material->diffuseReflectance.color!=rr::RRVec3(0))
				{
					numTrianglesWithDifConst++;
				}

				// spec
				if (material->specularReflectance.texture)
				{
					numTrianglesWithSpecMap++;
				}
				else
				if (material->specularReflectance.color!=rr::RRVec3(0))
				{
					numTrianglesWithSpecConst++;
				}

				// emi
				if (material->diffuseEmittance.texture)
				{
					numTrianglesWithEmiMap++;
				}
				else
				if (material->diffuseEmittance.color!=rr::RRVec3(0))
				{
					numTrianglesWithEmiConst++;
				}

				// transp
				if (material->specularTransmittance.texture)
				{
					if (material->specularTransmittance.texture==material->diffuseReflectance.texture)
						numTrianglesWithDifMapATransp++;
					else 
					if (material->specularTransmittanceInAlpha)
						numTrianglesWithNonDifMapATransp++;
					else 
						numTrianglesWithMapRGBTransp++;
					if (material->specularTransmittanceKeyed)
						numTrianglesWithTranspKeyed++;
					else
						numTrianglesWithTranspBlend++;
				}
				else
				if (material->specularTransmittance.color!=rr::RRVec3(0))
				{
					numTrianglesWithConstTransp++;
					numTrianglesWithTranspBlend++;
				}

				// culling
				if (material->sideBits[0].renderFrom && material->sideBits[1].renderFrom)
					numTriangles2Sided++;
				else
					numTriangles01Sided++;
			}
		}
	}

	// dif
	MATERIAL_DIFFUSE_X2 = false;
	MATERIAL_DIFFUSE_CONST = numTrianglesWithDifConst>0 && numTrianglesWithDifMap==0;
	MATERIAL_DIFFUSE_MAP = numTrianglesWithDifMap>0;
	MATERIAL_DIFFUSE = MATERIAL_DIFFUSE_CONST || MATERIAL_DIFFUSE_MAP;

	// spec
	MATERIAL_SPECULAR_CONST = numTrianglesWithSpecConst>0;
	MATERIAL_SPECULAR_MAP = numTrianglesWithSpecMap>0;
	MATERIAL_SPECULAR = MATERIAL_SPECULAR_CONST || MATERIAL_SPECULAR_MAP;

	// emi
	MATERIAL_EMISSIVE_CONST = numTrianglesWithEmiConst>0 && numTrianglesWithEmiMap==0;
	MATERIAL_EMISSIVE_MAP = numTrianglesWithEmiMap>0;

	// transp
	if (numTrianglesWithMapRGBTransp>0 && numTrianglesWithDifMapATransp+numTrianglesWithNonDifMapATransp>0)
	{
		//LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Scene contains both alpha transparency maps and rgb transparency maps, realtime renderer might render incorrectly.\n");
	}
	if (numTrianglesWithMapRGBTransp>numTrianglesWithDifMapATransp+numTrianglesWithNonDifMapATransp)
	{
		// transparency mostly in rgb map
		MATERIAL_TRANSPARENCY_CONST = 0;
		MATERIAL_TRANSPARENCY_MAP = 1;
		MATERIAL_TRANSPARENCY_IN_ALPHA = 0;
	}
	else
	if (numTrianglesWithNonDifMapATransp>0)
	{
		// transparency mostly in a map
		MATERIAL_TRANSPARENCY_CONST = 0;
		MATERIAL_TRANSPARENCY_MAP = 1;
		MATERIAL_TRANSPARENCY_IN_ALPHA = 1;
	}
	else
	if (numTrianglesWithDifMapATransp>0)
	{
		// transparency mostly in a of diffuse map
		MATERIAL_TRANSPARENCY_CONST = 0;
		MATERIAL_TRANSPARENCY_MAP = 0;
		MATERIAL_TRANSPARENCY_IN_ALPHA = 1;
	}
	else
	if (numTrianglesWithConstTransp>0)
	{
		// transparency mostly constant
		MATERIAL_TRANSPARENCY_CONST = 1;
		MATERIAL_TRANSPARENCY_MAP = 0;
		MATERIAL_TRANSPARENCY_IN_ALPHA = 0;
	}
	else
	{
		// no transparency
		MATERIAL_TRANSPARENCY_CONST = 0;
		MATERIAL_TRANSPARENCY_MAP = 0;
		MATERIAL_TRANSPARENCY_IN_ALPHA = 0;
	}
	MATERIAL_TRANSPARENCY_BLEND = numTrianglesWithTranspBlend>numTrianglesWithTranspKeyed;

	// misc
	MATERIAL_NORMAL_MAP = false;
	MATERIAL_CULLING = numTriangles01Sided>0;
}

const char* UberProgramSetup::getSetupString()
{
	static bool SHADOW_BILINEAR = true;
	LIMITED_TIMES(1,char* renderer = (char*)glGetString(GL_RENDERER);if (renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON"))) SHADOW_BILINEAR = false);

	RR_ASSERT(!MATERIAL_TRANSPARENCY_CONST || !MATERIAL_TRANSPARENCY_MAP); // engine does not support both together

	static char setup[2000];
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		SHADOW_MAPS,
		SHADOW_SAMPLES,
		SHADOW_BILINEAR?"#define SHADOW_BILINEAR\n":"",
		SHADOW_PENUMBRA?"#define SHADOW_PENUMBRA\n":"",
		SHADOW_CASCADE?"#define SHADOW_CASCADE\n":"",
		LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
		LIGHT_DIRECT_COLOR?"#define LIGHT_DIRECT_COLOR\n":"",
		LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
		LIGHT_DIRECTIONAL?"#define LIGHT_DIRECTIONAL\n":"",
		LIGHT_DIRECT_ATT_SPOT?"#define LIGHT_DIRECT_ATT_SPOT\n":"",
		LIGHT_DIRECT_ATT_PHYSICAL?"#define LIGHT_DIRECT_ATT_PHYSICAL\n":"",
		LIGHT_DIRECT_ATT_POLYNOMIAL?"#define LIGHT_DIRECT_ATT_POLYNOMIAL\n":"",
		LIGHT_DIRECT_ATT_EXPONENTIAL?"#define LIGHT_DIRECT_ATT_EXPONENTIAL\n":"",
		LIGHT_INDIRECT_CONST?"#define LIGHT_INDIRECT_CONST\n":"",
		LIGHT_INDIRECT_VCOLOR?"#define LIGHT_INDIRECT_VCOLOR\n":"",
		LIGHT_INDIRECT_VCOLOR2?"#define LIGHT_INDIRECT_VCOLOR2\n":"",
		LIGHT_INDIRECT_VCOLOR_PHYSICAL?"#define LIGHT_INDIRECT_VCOLOR_PHYSICAL\n":"",
		LIGHT_INDIRECT_MAP?"#define LIGHT_INDIRECT_MAP\n":"",
		LIGHT_INDIRECT_MAP2?"#define LIGHT_INDIRECT_MAP2\n":"",
		LIGHT_INDIRECT_DETAIL_MAP?"#define LIGHT_INDIRECT_DETAIL_MAP\n":"",
		LIGHT_INDIRECT_ENV_DIFFUSE?"#define LIGHT_INDIRECT_ENV_DIFFUSE\n":"",
		LIGHT_INDIRECT_ENV_SPECULAR?"#define LIGHT_INDIRECT_ENV_SPECULAR\n":"",
		MATERIAL_DIFFUSE?"#define MATERIAL_DIFFUSE\n":"",
		MATERIAL_DIFFUSE_X2?"#define MATERIAL_DIFFUSE_X2\n":"",
		MATERIAL_DIFFUSE_CONST?"#define MATERIAL_DIFFUSE_CONST\n":"",
		MATERIAL_DIFFUSE_MAP?"#define MATERIAL_DIFFUSE_MAP\n":"",
		MATERIAL_SPECULAR?"#define MATERIAL_SPECULAR\n":"",
		MATERIAL_SPECULAR_CONST?"#define MATERIAL_SPECULAR_CONST\n":"",
		MATERIAL_SPECULAR_MAP?"#define MATERIAL_SPECULAR_MAP\n":"",
		MATERIAL_EMISSIVE_CONST?"#define MATERIAL_EMISSIVE_CONST\n":"",
		MATERIAL_EMISSIVE_MAP?"#define MATERIAL_EMISSIVE_MAP\n":"",
		MATERIAL_TRANSPARENCY_CONST?"#define MATERIAL_TRANSPARENCY_CONST\n":"",
		MATERIAL_TRANSPARENCY_MAP?"#define MATERIAL_TRANSPARENCY_MAP\n":"",
		MATERIAL_TRANSPARENCY_IN_ALPHA?"#define MATERIAL_TRANSPARENCY_IN_ALPHA\n":"",
		MATERIAL_TRANSPARENCY_BLEND?"#define MATERIAL_TRANSPARENCY_BLEND\n":"",
		MATERIAL_NORMAL_MAP?"#define MATERIAL_NORMAL_MAP\n":"",
		ANIMATION_WAVE?"#define ANIMATION_WAVE\n":"",
		POSTPROCESS_NORMALS?"#define POSTPROCESS_NORMALS\n":"",
		POSTPROCESS_BRIGHTNESS?"#define POSTPROCESS_BRIGHTNESS\n":"",
		POSTPROCESS_GAMMA?"#define POSTPROCESS_GAMMA\n":"",
		POSTPROCESS_BIGSCREEN?"#define POSTPROCESS_BIGSCREEN\n":"",
		OBJECT_SPACE?"#define OBJECT_SPACE\n":"",
		CLIP_PLANE?"#define CLIP_PLANE\n":"",
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
	GLint maxTextureImageUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&maxTextureImageUnits);
	int maxShadowmapUnits = maxTextureImageUnits-TEXTURE_2D_SHADOWMAP_0; // should be 1 on mesa, 9 on radeon, 25 on geforce
	if (maxShadowmapUnits<1) return 0;

	while (argc--)
	{
		int tmp;
		if (sscanf(argv[argc],"penumbra%d",&tmp)==1 && tmp>=1 && tmp<=8) // accept only penumbra1..8
		{
			SHADOW_MAPS = tmp;
			if (tmp<1 || tmp>8 || tmp>maxShadowmapUnits || !getProgram(uberProgram)) 
			{
				rr::RRReporter::report(rr::ERRO,"GPU is not able to produce given penumbra quality, set lower quality.\n");
				SHADOW_MAPS = 0;
				return 0;
			}
			return tmp;
		}
	}
	// try max 8 maps, we must fit all maps in ubershader to 16 (maximum allowed by ATI)
	for (SHADOW_MAPS=1;SHADOW_MAPS<=(unsigned)RR_MIN(maxShadowmapUnits,8);SHADOW_MAPS++)
	{
		Program* program = getProgram(uberProgram);
		if (!program // stop when !compiled or !linked
			|| (LIGHT_DIRECT && !program->uniformExists("worldLightPos")) // stop when uniform missing, workaround for Nvidia bug
			) break;
	}
	unsigned instancesPerPassOrig = --SHADOW_MAPS;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if (renderer)
	{
		// find 4digit number
		unsigned number = 0;
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for (unsigned i=0;renderer[i];i++)
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}

		// workaround for Catalyst bug (driver crashes or outputs garbage on long shader)
		if ( strstr(renderer,"Radeon")||strstr(renderer,"RADEON") )
		{
			if ( (number>=1300 && number<=1999) )
			{
				// X1950 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
				// X1650 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
				SHADOW_MAPS = RR_MIN(SHADOW_MAPS,4);
			}
			else
			if ( (number>=9500 || number<=1299) )
			{
				// X300 in Lightsmark2008 5->2or1, otherwise reads garbage from last shadowmap
				SHADOW_MAPS = RR_MIN(SHADOW_MAPS,1);
			}
		}
	}
	// 2 is ugly, prefer 1
	if (SHADOW_MAPS==2) SHADOW_MAPS--;
	rr::RRReporter::report(rr::INF1,"Penumbra quality: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,renderer?renderer:"");
	return SHADOW_MAPS;
}

void UberProgramSetup::checkCapabilities()
{
	GLint maxTextureImageUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&maxTextureImageUnits);
	if (maxTextureImageUnits<16)
	{
		rr::RRReporter::report(rr::WARN,"Only %d textures per shader supported, some features will be disabled.\n",maxTextureImageUnits);
	}
}

void UberProgramSetup::reduceMaterialSetup(const UberProgramSetup& fullMaterial)
{
	MATERIAL_DIFFUSE               &= fullMaterial.MATERIAL_DIFFUSE;
	MATERIAL_DIFFUSE_X2            &= fullMaterial.MATERIAL_DIFFUSE_X2;
	//MATERIAL_DIFFUSE_CONST       &= fullMaterial.MATERIAL_DIFFUSE_CONST;
	MATERIAL_DIFFUSE_CONST          = (MATERIAL_DIFFUSE_CONST && fullMaterial.MATERIAL_DIFFUSE_CONST)
		|| (MATERIAL_DIFFUSE_CONST && fullMaterial.MATERIAL_DIFFUSE_MAP && !MATERIAL_DIFFUSE_MAP)
		|| (fullMaterial.MATERIAL_DIFFUSE_CONST && MATERIAL_DIFFUSE_MAP && !fullMaterial.MATERIAL_DIFFUSE_MAP);
	MATERIAL_DIFFUSE_MAP           &= fullMaterial.MATERIAL_DIFFUSE_MAP;
	MATERIAL_SPECULAR              &= fullMaterial.MATERIAL_SPECULAR;
	//MATERIAL_SPECULAR_CONST      &= fullMaterial.MATERIAL_SPECULAR_CONST;
	MATERIAL_SPECULAR_CONST         = (MATERIAL_SPECULAR_CONST && fullMaterial.MATERIAL_SPECULAR_CONST)
		|| (MATERIAL_SPECULAR_CONST && fullMaterial.MATERIAL_SPECULAR_MAP && !MATERIAL_SPECULAR_MAP)
		|| (fullMaterial.MATERIAL_SPECULAR_CONST && MATERIAL_SPECULAR_MAP && !fullMaterial.MATERIAL_SPECULAR_MAP);
	MATERIAL_SPECULAR_MAP          &= fullMaterial.MATERIAL_SPECULAR_MAP;
	MATERIAL_EMISSIVE_CONST         = (MATERIAL_EMISSIVE_CONST && fullMaterial.MATERIAL_EMISSIVE_CONST)
		|| (MATERIAL_EMISSIVE_CONST && fullMaterial.MATERIAL_EMISSIVE_MAP && !MATERIAL_EMISSIVE_MAP)
		|| (fullMaterial.MATERIAL_EMISSIVE_CONST && MATERIAL_EMISSIVE_MAP && !fullMaterial.MATERIAL_EMISSIVE_MAP);
	MATERIAL_EMISSIVE_MAP          &= fullMaterial.MATERIAL_EMISSIVE_MAP;
	//MATERIAL_TRANSPARENCY_CONST  &= fullMaterial.MATERIAL_TRANSPARENCY_CONST;
	MATERIAL_TRANSPARENCY_CONST     = (MATERIAL_TRANSPARENCY_CONST && fullMaterial.MATERIAL_TRANSPARENCY_CONST)
		|| (MATERIAL_TRANSPARENCY_CONST && fullMaterial.MATERIAL_TRANSPARENCY_MAP && !MATERIAL_TRANSPARENCY_MAP)
		|| (fullMaterial.MATERIAL_TRANSPARENCY_CONST && MATERIAL_TRANSPARENCY_MAP && !fullMaterial.MATERIAL_TRANSPARENCY_MAP);
	MATERIAL_TRANSPARENCY_MAP      &= fullMaterial.MATERIAL_TRANSPARENCY_MAP;
	MATERIAL_TRANSPARENCY_IN_ALPHA &= fullMaterial.MATERIAL_TRANSPARENCY_IN_ALPHA;
	MATERIAL_TRANSPARENCY_BLEND    &= fullMaterial.MATERIAL_TRANSPARENCY_BLEND;
	MATERIAL_NORMAL_MAP            &= fullMaterial.MATERIAL_NORMAL_MAP;
	MATERIAL_CULLING               &= fullMaterial.MATERIAL_CULLING;
}

void UberProgramSetup::validate()
{
	// removes unused bits
	// why?
	// because shader optimizes some parameters away
	// we use bits to send parameters to shader
	// sending parameter that is not present in shader wastes CPU time so we report it as error

	if (!LIGHT_DIRECT)
	{
		SHADOW_MAPS = 0;
		SHADOW_SAMPLES = 0;
		LIGHT_DIRECT_MAP = 0;
		LIGHT_DIRECTIONAL = 0;
		LIGHT_DIRECT_ATT_SPOT = 0;
		LIGHT_DIRECT_ATT_PHYSICAL = 0;
		LIGHT_DIRECT_ATT_POLYNOMIAL = 0;
		LIGHT_DIRECT_ATT_EXPONENTIAL = 0;
	}
	if (!LIGHT_INDIRECT_VCOLOR)
	{
		LIGHT_INDIRECT_VCOLOR2 = 0;
	}
	if (!LIGHT_INDIRECT_MAP)
	{
		LIGHT_INDIRECT_MAP2 = 0;
	}
	if (LIGHT_INDIRECT_MAP && LIGHT_INDIRECT_DETAIL_MAP)
	{
		LIGHT_INDIRECT_DETAIL_MAP = 0; // LIGHT_INDIRECT_DETAIL_MAP information is already baked in LIGHT_INDIRECT_MAP
	}
	if (!LIGHT_DIRECT && !LIGHT_INDIRECT_CONST && !LIGHT_INDIRECT_VCOLOR && !LIGHT_INDIRECT_MAP && !LIGHT_INDIRECT_MAP2 && !LIGHT_INDIRECT_ENV_DIFFUSE)
	{
		LIGHT_INDIRECT_DETAIL_MAP = 0;
		MATERIAL_DIFFUSE = 0; // diffuse reflection requested, but there's no suitable light
	}
	if (!LIGHT_DIRECT && !LIGHT_INDIRECT_CONST && !LIGHT_INDIRECT_ENV_SPECULAR)
	{
		MATERIAL_SPECULAR = 0; // specular reflection requested, but there's no suitable light
	}
	if (!MATERIAL_DIFFUSE)
	{
		MATERIAL_DIFFUSE_X2 = 0;
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_MAP = 0;
		LIGHT_INDIRECT_ENV_DIFFUSE = 0;
	}
	if (!MATERIAL_SPECULAR)
	{
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
		LIGHT_INDIRECT_ENV_SPECULAR = 0;
	}
	bool incomingLight = LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR;
	bool reflectsLight = MATERIAL_DIFFUSE || MATERIAL_SPECULAR;
	if (!incomingLight || !reflectsLight)
	{
		// no light -> disable reflection
		// no reflection -> disable light
		UberProgramSetup uberProgramSetupBlack;
		uberProgramSetupBlack.MATERIAL_EMISSIVE_CONST = MATERIAL_EMISSIVE_CONST;
		uberProgramSetupBlack.MATERIAL_EMISSIVE_MAP = MATERIAL_EMISSIVE_MAP;
		uberProgramSetupBlack.MATERIAL_TRANSPARENCY_CONST = MATERIAL_TRANSPARENCY_CONST;
		uberProgramSetupBlack.MATERIAL_TRANSPARENCY_MAP = MATERIAL_TRANSPARENCY_MAP;
		uberProgramSetupBlack.MATERIAL_TRANSPARENCY_BLEND = MATERIAL_TRANSPARENCY_BLEND; // if not preserved, breaks Z-only pre rendering of >50% transparent objects
		uberProgramSetupBlack.OBJECT_SPACE = OBJECT_SPACE;
		uberProgramSetupBlack.CLIP_PLANE = CLIP_PLANE;
		uberProgramSetupBlack.FORCE_2D_POSITION = FORCE_2D_POSITION;
		*this = uberProgramSetupBlack;
	}
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma)
{
	LIMITED_TIMES(1,checkCapabilities());

	if (LIGHT_DIRECT_MAP && !SHADOW_MAPS)
	{
		// late correction. we rarely get here, see early correction
		LIGHT_DIRECT_MAP = 0;
		LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Projecting texture without shadows not supported yet.\n"));
	}

	Program* program = getProgram(uberProgram);
	if (!program)
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useProgram: failed to compile or link GLSL shader.\n"));
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
	for (unsigned i=0;i<SHADOW_MAPS;i++)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		Texture* shadowmap = light->getShadowmap(firstInstance+i);
		if (shadowmap)
		{
			glActiveTexture(GL_TEXTURE0+TEXTURE_2D_SHADOWMAP_0+i); // for binding "shadowmapN" texture
			// prepare samplers
			shadowmap->bindTexture();
			//samplers[i]=i; // for array of samplers (needs OpenGL 2.0 compliant card)
			char name[] = "shadowMap0"; // for individual samplers
			name[9] = '0'+i; // for individual samplers
			program->sendUniform(name, (int)(TEXTURE_2D_SHADOWMAP_0+i)); // for individual samplers
			// prepare and send matrices
			Camera* lightInstance = light->getShadowmapCamera(firstInstance+i,true);
			glActiveTexture(GL_TEXTURE0+i); // for feeding gl_TextureMatrix[0..maps-1]
			glLoadMatrixd(tmp);
			glMultMatrixd(lightInstance->frustumMatrix);
			glMultMatrixd(lightInstance->viewMatrix);
			delete lightInstance;
		}
	}
	//myProg->sendUniform("shadowMap", instances, samplers); // for array of samplers (needs OpenGL 2.0 compliant card)
	glMatrixMode(GL_MODELVIEW);

	if (SHADOW_SAMPLES>1)
	{
		rr::RRBuffer* buffer = light->getShadowmap(firstInstance)->getBuffer();
		unsigned shadowmapSize = buffer->getWidth()+buffer->getHeight();
		program->sendUniform("shadowBlurWidth",6.f/shadowmapSize,-6.f/shadowmapSize,0.0f,3.f/shadowmapSize);
	}

	if (LIGHT_DIRECT)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		if (!light->getParent())
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->getParent()==NULL.\n");
			return false;
		}

		if (LIGHT_DIRECTIONAL || LIGHT_DIRECT_ATT_SPOT)
		{
			program->sendUniform("worldLightDir",light->getParent()->dir[0],light->getParent()->dir[1],light->getParent()->dir[2]);
		}
		if (!LIGHT_DIRECTIONAL)
		{
			if (!program->uniformExists("worldLightPos"))
			{
				LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Miscompiled shader, this is known driver bug.\n"));
				return NULL;
			}
			program->sendUniform("worldLightPos",light->getParent()->pos[0],light->getParent()->pos[1],light->getParent()->pos[2]);
		}
	}

	if (LIGHT_DIRECT_COLOR)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set.\n");
			return false;
		}
		rr::RRVec3 color = light->getRRLight().color;
		if (light->getRRLight().distanceAttenuationType!=rr::RRLight::POLYNOMIAL)
		{
			color[0] = pow(color[0],0.45f);
			color[1] = pow(color[1],0.45f);
			color[2] = pow(color[2],0.45f);
		}
		program->sendUniform("lightDirectColor",color[0],color[1],color[2],1.0f);
	}

	if (LIGHT_DIRECT_MAP)
	{
		if (!light->getProjectedTexture())
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: LIGHT_DIRECT_MAP set, but getProjectedTexture()==NULL.\n");
			return false;
		}
		int id=TEXTURE_2D_LIGHT_DIRECT;
		glActiveTexture(GL_TEXTURE0+id);
		light->getProjectedTexture()->bindTexture();
		program->sendUniform("lightDirectMap", id);
	}

	if (LIGHT_DIRECT_ATT_SPOT)
	{
		program->sendUniform("lightDirectSpotOuterAngleRad",light->getRRLight().outerAngleRad);
		program->sendUniform("lightDirectSpotFallOffAngleRad",light->getRRLight().fallOffAngleRad);
	}

	if (LIGHT_DIRECT_ATT_PHYSICAL)
	{
		RR_ASSERT(light->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL);
	}

	if (LIGHT_DIRECT_ATT_POLYNOMIAL)
	{
		RR_ASSERT(light->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL);
		program->sendUniform("lightDistancePolynom",light->getRRLight().polynom.x,light->getRRLight().polynom.y,light->getRRLight().polynom.z,light->getRRLight().polynom.w);
	}

	if (LIGHT_DIRECT_ATT_EXPONENTIAL)
	{
		RR_ASSERT(light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL);
		program->sendUniform("lightDistanceRadius",light->getRRLight().radius);
		program->sendUniform("lightDistanceFallOffExponent",light->getRRLight().fallOffExponent);
	}

	if (LIGHT_INDIRECT_CONST)
	{
		program->sendUniform("lightIndirectConst",0.2f,0.2f,0.2f,1.0f);
	}

	if (LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_DETAIL_MAP)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectMap", id);
	}

	if (LIGHT_INDIRECT_MAP2)
	{
		int id=TEXTURE_2D_LIGHT_INDIRECT2;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectMap2", id);
	}

	if (LIGHT_INDIRECT_ENV_DIFFUSE && MATERIAL_DIFFUSE)
	{
		int id=TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectDiffuseEnvMap", id);
	}

	if (LIGHT_INDIRECT_ENV_SPECULAR && MATERIAL_SPECULAR)
	{
		int id=TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR;
		//glActiveTexture(GL_TEXTURE0+id);
		program->sendUniform("lightIndirectSpecularEnvMap", id);
	}

	if (MATERIAL_DIFFUSE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_DIFFUSE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active
		program->sendUniform("materialDiffuseMap", id);
	}

	if (MATERIAL_EMISSIVE_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_EMISSIVE;
		glActiveTexture(GL_TEXTURE0+id); // last before drawScene, must stay active (EMISSIVE is typically used without DIFFUSE)
		program->sendUniform("materialEmissiveMap", id);
	}

	if (MATERIAL_TRANSPARENCY_MAP)
	{
		int id=TEXTURE_2D_MATERIAL_TRANSPARENCY;
		program->sendUniform("materialTransparencyMap", id);
	}

	if (POSTPROCESS_BRIGHTNESS
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		if (!brightness)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: brightness==NULL.\n");
			return false;
		}
		program->sendUniform4fv("postprocessBrightness", &brightness->x);
	}

	if (POSTPROCESS_GAMMA
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		program->sendUniform("postprocessGamma", gamma);
	}

	if ((LIGHT_DIRECT || LIGHT_INDIRECT_ENV_SPECULAR) && !FORCE_2D_POSITION)
	{
		const Camera* camera = Camera::getRenderCamera();
		if (camera)
		{
			program->sendUniform("worldEyePos",camera->pos[0],camera->pos[1],camera->pos[2]);
		}
		else
		{
			RR_ASSERT(0);
		}
	}

	return program;
}

void UberProgramSetup::useIlluminationEnvMaps(Program* program, rr::RRObjectIllumination* illumination, bool updateTexturesFromBuffers)
{
	if (!program)
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationEnvMaps(program=NULL).\n"));
		return;
	}
	if (!illumination)
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationEnvMaps(illumination=NULL).\n"));
		return;
	}
	if (LIGHT_INDIRECT_ENV_DIFFUSE && MATERIAL_DIFFUSE)
	{
		if (illumination->diffuseEnvMap)
		{
			glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
			if (updateTexturesFromBuffers)
				getTexture(illumination->diffuseEnvMap,false,false)->reset(false,false);
			getTexture(illumination->diffuseEnvMap,false,false)->bindTexture();
		}
	}
	if (LIGHT_INDIRECT_ENV_SPECULAR && MATERIAL_SPECULAR)
	{
		if (illumination->specularEnvMap)
		{
			glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
			if (updateTexturesFromBuffers)
				getTexture(illumination->specularEnvMap,false,false)->reset(false,false);
			getTexture(illumination->specularEnvMap,false,false)->bindTexture();
		}
	}
}

}; // namespace
