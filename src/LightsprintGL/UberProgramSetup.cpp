// --------------------------------------------------------------------------
// UberProgramSetup, settings specific for our single UberProgram.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <boost/unordered_map.hpp>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/RRDebug.h"
#include "Workaround.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// Helpers: 1x1 textures, temporaries for VBO update

class Buffers1x1
{
public:
	void bindPropertyTexture(const rr::RRMaterial::Property& property,unsigned index)
	{
		rr::RRBuffer* buffer = property.texture;
		if (buffer)
		{
			// shader expects texture and material provides it
			getTexture(buffer)->bindTexture();
		}
		else
		{
			// this is only safety net, we should ideally never get here (we still do)
			// getting here means shader expects texture but material provides only color
			// lets create 1x1 texture for given color
			unsigned color = RR_FLOAT2BYTE(property.color[0])+(RR_FLOAT2BYTE(property.color[1])<<8)+(RR_FLOAT2BYTE(property.color[2])<<16);
			Buffer1x1Cache::iterator i = buffers1x1.find(color);
			if (i!=buffers1x1.end())
			{
				buffer = i->second;
				getTexture(buffer)->bindTexture();
			}
			else
			{
				buffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,(index==2)?rr::BF_RGBA:rr::BF_RGB,true,NULL); // 2 = RGBA
				buffer->setElement(0,rr::RRVec4(property.color,1-property.color.avg()));
				getTexture(buffer,false,false);
				buffers1x1[color] = buffer;
			}
		}
	}
	void releaseAllBuffers1x1()
	{
		for (Buffer1x1Cache::const_iterator i=buffers1x1.begin();i!=buffers1x1.end();++i)
			delete i->second;
	}
private:
	typedef boost::unordered_map<unsigned,rr::RRBuffer*> Buffer1x1Cache;
	Buffer1x1Cache buffers1x1;
};

static Buffers1x1 s_buffers1x1;

void releaseAllBuffers1x1()
{
	s_buffers1x1.releaseAllBuffers1x1();
}


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vs/fs

void UberProgramSetup::enableAllMaterials()
{
	MATERIAL_DIFFUSE = true;
	MATERIAL_DIFFUSE_CONST = true;
	MATERIAL_DIFFUSE_MAP = true;
	MATERIAL_SPECULAR = true;
	MATERIAL_SPECULAR_CONST = true;
	MATERIAL_EMISSIVE_CONST = true;
	MATERIAL_EMISSIVE_MAP = true;
	MATERIAL_TRANSPARENCY_CONST = true;
	MATERIAL_TRANSPARENCY_MAP = true;
	MATERIAL_TRANSPARENCY_IN_ALPHA = true;
	MATERIAL_TRANSPARENCY_BLEND = true;
	MATERIAL_TRANSPARENCY_TO_RGB = true;
	MATERIAL_NORMAL_MAP = false;
	MATERIAL_CULLING = true;
}

void UberProgramSetup::enableUsedMaterials(const rr::RRMaterial* material)
{
	if (!material)
		return;

	// dif
	MATERIAL_DIFFUSE_X2 = false;
	MATERIAL_DIFFUSE = material->diffuseReflectance.color!=rr::RRVec3(0);
	MATERIAL_DIFFUSE_CONST = !material->diffuseReflectance.texture && material->diffuseReflectance.color!=rr::RRVec3(1);
	MATERIAL_DIFFUSE_MAP = material->diffuseReflectance.texture!=NULL;

	// spec
	MATERIAL_SPECULAR = material->specularReflectance.color!=rr::RRVec3(0);
	MATERIAL_SPECULAR_CONST = !material->specularReflectance.texture && material->specularReflectance.color!=rr::RRVec3(1);
	MATERIAL_SPECULAR_MAP = material->specularReflectance.texture!=NULL;
	MATERIAL_SPECULAR_MODEL = material->specularModel;

	// emi
	MATERIAL_EMISSIVE_CONST = material->diffuseEmittance.color!=rr::RRVec3(0) && !material->diffuseEmittance.texture;
	MATERIAL_EMISSIVE_MAP = material->diffuseEmittance.texture!=NULL;

	// transp
	MATERIAL_TRANSPARENCY_CONST = !material->specularTransmittance.texture && material->specularTransmittance.color!=rr::RRVec3(0);
	MATERIAL_TRANSPARENCY_MAP = material->specularTransmittance.texture!=NULL;
	MATERIAL_TRANSPARENCY_IN_ALPHA = material->specularTransmittance.color!=rr::RRVec3(0) && material->specularTransmittanceInAlpha;
	MATERIAL_TRANSPARENCY_BLEND = material->specularTransmittance.color!=rr::RRVec3(0) && !material->specularTransmittanceKeyed;
	MATERIAL_TRANSPARENCY_TO_RGB = MATERIAL_TRANSPARENCY_BLEND;

	// misc
	MATERIAL_NORMAL_MAP = false;
	MATERIAL_CULLING = material->sideBits[0].renderFrom != material->sideBits[1].renderFrom;
}

const char* UberProgramSetup::getSetupString()
{
	bool SHADOW_BILINEAR = !Workaround::needsUnfilteredShadowmaps();

	RR_ASSERT(!MATERIAL_TRANSPARENCY_CONST || !MATERIAL_TRANSPARENCY_MAP); // engine does not support both together

	static char setup[2000];
	sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
		"#define MATERIAL_SPECULAR_MODEL %d\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		SHADOW_MAPS,
		SHADOW_SAMPLES,
		SHADOW_COLOR?"#define SHADOW_COLOR\n":"",
		SHADOW_BILINEAR?"#define SHADOW_BILINEAR\n":"",
		SHADOW_PENUMBRA?"#define SHADOW_PENUMBRA\n":"",
		SHADOW_CASCADE?"#define SHADOW_CASCADE\n":"",
		SHADOW_ONLY?"#define SHADOW_ONLY\n":"",
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
		MATERIAL_SPECULAR_MODEL,
		MATERIAL_EMISSIVE_CONST?"#define MATERIAL_EMISSIVE_CONST\n":"",
		MATERIAL_EMISSIVE_MAP?"#define MATERIAL_EMISSIVE_MAP\n":"",
		MATERIAL_TRANSPARENCY_CONST?"#define MATERIAL_TRANSPARENCY_CONST\n":"",
		MATERIAL_TRANSPARENCY_MAP?"#define MATERIAL_TRANSPARENCY_MAP\n":"",
		MATERIAL_TRANSPARENCY_IN_ALPHA?"#define MATERIAL_TRANSPARENCY_IN_ALPHA\n":"",
		MATERIAL_TRANSPARENCY_BLEND?"#define MATERIAL_TRANSPARENCY_BLEND\n":"",
		MATERIAL_TRANSPARENCY_TO_RGB?"#define MATERIAL_TRANSPARENCY_TO_RGB\n":"",
		MATERIAL_NORMAL_MAP?"#define MATERIAL_NORMAL_MAP\n":"",
		ANIMATION_WAVE?"#define ANIMATION_WAVE\n":"",
		POSTPROCESS_NORMALS?"#define POSTPROCESS_NORMALS\n":"",
		POSTPROCESS_BRIGHTNESS?"#define POSTPROCESS_BRIGHTNESS\n":"",
		POSTPROCESS_GAMMA?"#define POSTPROCESS_GAMMA\n":"",
		POSTPROCESS_BIGSCREEN?"#define POSTPROCESS_BIGSCREEN\n":"",
		OBJECT_SPACE?"#define OBJECT_SPACE\n":"",
		CLIP_PLANE_XA?"#define CLIP_PLANE_XA\n":"",
		CLIP_PLANE_XB?"#define CLIP_PLANE_XB\n":"",
		CLIP_PLANE_YA?"#define CLIP_PLANE_YA\n":"",
		CLIP_PLANE_YB?"#define CLIP_PLANE_YB\n":"",
		CLIP_PLANE_ZA?"#define CLIP_PLANE_ZA\n":"",
		CLIP_PLANE_ZB?"#define CLIP_PLANE_ZB\n":"",
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
bool UberProgramSetup::operator <(const UberProgramSetup& a) const
{
	return memcmp(this,&a,sizeof(*this))<0;
}

Program* UberProgramSetup::getProgram(UberProgram* uberProgram)
{
	return uberProgram->getProgram(getSetupString());
}

unsigned UberProgramSetup::detectMaxShadowmaps(UberProgram* uberProgram, int argc, const char*const*argv)
{
	GLint maxTextureImageUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&maxTextureImageUnits);
	int maxShadowmapUnits = maxTextureImageUnits-4; // reserve 4 units for other textures
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
	SHADOW_MAPS = Workaround::needsReducedQualityPenumbra(SHADOW_MAPS-1);
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

void UberProgramSetup::reduceMaterials(const UberProgramSetup& fullMaterial)
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
	MATERIAL_TRANSPARENCY_TO_RGB   &= fullMaterial.MATERIAL_TRANSPARENCY_TO_RGB;
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
		SHADOW_COLOR = 0;
		SHADOW_PENUMBRA = 0;
		SHADOW_CASCADE = 0;
		SHADOW_ONLY = 0;
		LIGHT_DIRECT_COLOR = 0;
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
	if (!LIGHT_DIRECT
		//&& !LIGHT_INDIRECT_CONST ...why was it here, bug? constant indirect does not affect specular
		//&& !LIGHT_INDIRECT_ENV_DIFFUSE // env diffuse needs normals, but it doesn't affect specular
		&& !LIGHT_INDIRECT_ENV_SPECULAR)
	{
		MATERIAL_SPECULAR = 0; // specular reflection requested, but there's no suitable light
	}
	if (!MATERIAL_DIFFUSE)
	{
		MATERIAL_DIFFUSE_X2 = 0;
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_MAP = 0;
		LIGHT_INDIRECT_MAP = 0; // lightmap is used only for diffuse reflection, alpha is ignored
		LIGHT_INDIRECT_DETAIL_MAP = 0; // LDM is used only for diffuse reflection, alpha is ignored
		LIGHT_INDIRECT_ENV_DIFFUSE = 0;
	}
	if (!MATERIAL_SPECULAR)
	{
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
		LIGHT_INDIRECT_ENV_SPECULAR = 0;
	}
	if (MATERIAL_TRANSPARENCY_BLEND)
	{
		// do this - and semitransparent pixels don't receive colored shadows
		// don't - and semitransparent pixels not in full shadow cast colored shadows on themselves
		//         (so 0.8 specular 0.1 transparency glass behind fence has strong specular only in fence shadow,
		//         unshadowed glass has 10x weaker specular and is darker, visible in 2010-01-eiranranta, looks very wrong)
		// both is wrong, but first evil is smaller
		SHADOW_COLOR = 0;
	}
	bool incomingLight = LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR;
	bool reflectsLight = MATERIAL_DIFFUSE || MATERIAL_SPECULAR;
	if (!incomingLight || !reflectsLight)
	{
		// no light -> disable reflection
		// no reflection -> disable light
		SHADOW_MAPS = 0;
		SHADOW_SAMPLES = 0;
		SHADOW_COLOR = 0;
		SHADOW_PENUMBRA = 0;
		SHADOW_CASCADE = 0;
		SHADOW_ONLY = 0;
		LIGHT_DIRECT = 0;
		LIGHT_DIRECT_COLOR = 0;
		LIGHT_DIRECT_MAP = 0;
		LIGHT_DIRECTIONAL = 0;
		LIGHT_DIRECT_ATT_SPOT = 0;
		LIGHT_DIRECT_ATT_PHYSICAL = 0;
		LIGHT_DIRECT_ATT_POLYNOMIAL = 0;
		LIGHT_DIRECT_ATT_EXPONENTIAL = 0;
		LIGHT_INDIRECT_CONST = 0;
		LIGHT_INDIRECT_VCOLOR = 0;
		LIGHT_INDIRECT_VCOLOR2 = 0;
		LIGHT_INDIRECT_VCOLOR_PHYSICAL = 0;
		LIGHT_INDIRECT_MAP = 0;
		LIGHT_INDIRECT_MAP2 = 0;
		LIGHT_INDIRECT_DETAIL_MAP = 0;
		LIGHT_INDIRECT_ENV_DIFFUSE = 0;
		LIGHT_INDIRECT_ENV_SPECULAR = 0;
		LIGHT_INDIRECT_auto = 0;
		MATERIAL_DIFFUSE = 0;
		MATERIAL_DIFFUSE_X2 = 0;
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_MAP = 0;
		MATERIAL_SPECULAR = 0;
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
		MATERIAL_NORMAL_MAP = 0;
		if (!MATERIAL_EMISSIVE_CONST && !MATERIAL_EMISSIVE_MAP)
		{
			POSTPROCESS_BRIGHTNESS = 0;
			POSTPROCESS_GAMMA = 0;
		}
	}
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, float* clipPlanes)
{
	RR_LIMITED_TIMES(1,checkCapabilities());

	Program* program = getProgram(uberProgram);
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useProgram: failed to compile or link GLSL shader.\n"));
		return NULL;
	}
	program->useIt();

	// shadowMapN, textureMatrixN, shadowColorN
	for (unsigned i=0;i<SHADOW_MAPS;i++)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set (SHADOW_MAPS>0).\n");
			return false;
		}
		Texture* shadowmap = light->getShadowmap(firstInstance+i);
		if (shadowmap)
		{
			// set depth texture
			char name[] = "shadowMap0";
			name[9] = '0'+i;
			program->sendTexture(name,shadowmap);
			// set matrix
			Camera* lightInstance = light->getShadowmapCamera(firstInstance+i,true);
			double m1[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 1,1,1,2};
			double m2[16];
			float m3[16];
#define MULT_MATRIX(a,b,c,ctype) { for (unsigned i=0;i<4;i++) for (unsigned j=0;j<4;j++) c[4*i+j] = (ctype)( b[4*i]*a[j] + b[4*i+1]*a[4+j] + b[4*i+2]*a[8+j] + b[4*i+3]*a[12+j] ); }
			MULT_MATRIX(m1,lightInstance->frustumMatrix,m2,double);
			MULT_MATRIX(m2,lightInstance->viewMatrix,m3,float);
			delete lightInstance;
			char name2[] = "textureMatrix0";
			name2[13] = '0'+i;
			program->sendUniform(name2,m3,false,4);
		}
	}
	if (SHADOW_COLOR)
	for (unsigned i=0;i<SHADOW_MAPS;i++)
	{
		Texture* shadowmap = light->getShadowmap(firstInstance+i,true);
		if (shadowmap)
		{
			// set color texture
			char name[] = "shadowColorMap0";
			name[14] = '0'+i;
			program->sendTexture(name,shadowmap);
		}
	}

	if (SHADOW_SAMPLES>1)
	{
		unsigned shadowmapSize = light->getRRLight().rtShadowmapSize;
		program->sendUniform("shadowBlurWidth",2.5f/shadowmapSize,-2.5f/shadowmapSize,0.0f,1.5f/shadowmapSize);
	}

	if (LIGHT_DIRECT)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set (LIGHT_DIRECT set).\n");
			return false;
		}
		if (!light->getParent())
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->getParent()==NULL.\n");
			return false;
		}

		if (LIGHT_DIRECT_MAP && !SHADOW_MAPS)
		{
			double m1[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 1,1,1,2};
			double m2[16];
			float m3[16];
			MULT_MATRIX(m1,light->getParent()->frustumMatrix,m2,double);
			MULT_MATRIX(m2,light->getParent()->viewMatrix,m3,float);
			program->sendUniform("textureMatrixL",m3,false,4);
		}
		if (LIGHT_DIRECTIONAL || LIGHT_DIRECT_ATT_SPOT)
		{
			program->sendUniform("worldLightDir",light->getParent()->dir[0],light->getParent()->dir[1],light->getParent()->dir[2]);
		}
		if (!LIGHT_DIRECTIONAL)
		{
			program->sendUniform("worldLightPos",light->getParent()->pos[0],light->getParent()->pos[1],light->getParent()->pos[2]);
		}
	}

	if (LIGHT_DIRECT_COLOR)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set (LIGHT_DIRECT_COLOR set).\n");
			return false;
		}
		rr::RRVec3 color = light->getRRLight().color;
		if (light->getRRLight().distanceAttenuationType!=rr::RRLight::POLYNOMIAL)
		{
			color[0] = color[0]<0?-pow(-color[0],0.45f):pow(color[0],0.45f);
			color[1] = color[1]<0?-pow(-color[1],0.45f):pow(color[1],0.45f);
			color[2] = color[2]<0?-pow(-color[2],0.45f):pow(color[2],0.45f);
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
		program->sendTexture("lightDirectMap", light->getProjectedTexture(), TEX_CODE_2D_LIGHT_DIRECT);
	}

	if (LIGHT_DIRECT_ATT_SPOT)
	{
		program->sendUniform("lightDirectSpotOuterAngleRad",light->getRRLight().outerAngleRad);
		program->sendUniform("lightDirectSpotFallOffAngleRad",light->getRRLight().fallOffAngleRad);
		program->sendUniform("lightDirectSpotExponent",RR_MAX(light->getRRLight().spotExponent,0.00000001f));
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

	if (POSTPROCESS_BRIGHTNESS
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		rr::RRVec4 correctedBrightness(brightness?*brightness:rr::RRVec4(1.0f));
		program->sendUniform4fv("postprocessBrightness", &correctedBrightness.x);
	}

	if (POSTPROCESS_GAMMA
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		program->sendUniform("postprocessGamma", gamma);
	}

	if (MATERIAL_SPECULAR && (LIGHT_DIRECT || LIGHT_INDIRECT_ENV_SPECULAR))
	{
		const Camera* camera = getRenderCamera();
		if (camera)
		{
			program->sendUniform("worldEyePos",camera->pos[0],camera->pos[1],camera->pos[2]);
		}
		else
		{
			RR_ASSERT(0);
		}
	}

	if (CLIP_PLANE_XA)
	{
		program->sendUniform("clipPlaneXA",clipPlanes?clipPlanes[0]:0);
	}
	if (CLIP_PLANE_XB)
	{
		program->sendUniform("clipPlaneXB",clipPlanes?clipPlanes[1]:0);
	}
	if (CLIP_PLANE_YA)
	{
		program->sendUniform("clipPlaneYA",clipPlanes?clipPlanes[2]:0);
	}
	if (CLIP_PLANE_YB)
	{
		program->sendUniform("clipPlaneYB",clipPlanes?clipPlanes[3]:0);
	}
	if (CLIP_PLANE_ZA)
	{
		program->sendUniform("clipPlaneZA",clipPlanes?clipPlanes[4]:0);
	}
	if (CLIP_PLANE_ZB)
	{
		program->sendUniform("clipPlaneZB",clipPlanes?clipPlanes[5]:0);
	}

	return program;
}

void UberProgramSetup::useMaterial(Program* program, const rr::RRMaterial* material) const
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useMaterial(): program=NULL\n"));
		return;
	}
	if (!material)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useMaterial(): material=NULL\n"));
		rr::RRMaterial s_material;
		RR_LIMITED_TIMES(1,s_material.reset(false));
		material = &s_material;
	}
	if (MATERIAL_DIFFUSE_CONST)
	{
		program->sendUniform("materialDiffuseConst",material->diffuseReflectance.color[0],material->diffuseReflectance.color[1],material->diffuseReflectance.color[2],1.0f);
	}

	if (MATERIAL_SPECULAR && LIGHT_DIRECT)
	{
		float shininess = material->specularShininess;
		switch (MATERIAL_SPECULAR_MODEL)
		{
			case rr::RRMaterial::PHONG:
			case rr::RRMaterial::BLINN_PHONG:
				shininess = RR_CLAMPED(material->specularShininess,1,1e10f);
				break;
			case rr::RRMaterial::TORRANCE_SPARROW:
				shininess = RR_CLAMPED(material->specularShininess*material->specularShininess,0.001f,1);
				break;
			case rr::RRMaterial::BLINN_TORRANCE_SPARROW:
				shininess = RR_CLAMPED(material->specularShininess*material->specularShininess,0,1);
				break;
		}
		program->sendUniform("materialSpecularShininess",shininess);
	}

	if (MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform("materialSpecularConst",material->specularReflectance.color[0],material->specularReflectance.color[1],material->specularReflectance.color[2],1.0f);
	}

	if (MATERIAL_EMISSIVE_CONST)
	{
		program->sendUniform("materialEmissiveConst",material->diffuseEmittance.color[0],material->diffuseEmittance.color[1],material->diffuseEmittance.color[2],0.0f);
	}

	if (MATERIAL_TRANSPARENCY_CONST)
	{
		program->sendUniform("materialTransparencyConst",material->specularTransmittance.color[0],material->specularTransmittance.color[1],material->specularTransmittance.color[2],1-material->specularTransmittance.color.avg());
	}

	if (MATERIAL_DIFFUSE_MAP)
	{
		program->sendTexture("materialDiffuseMap",NULL,TEX_CODE_2D_MATERIAL_DIFFUSE);
		s_buffers1x1.bindPropertyTexture(material->diffuseReflectance,0);
	}

	if (MATERIAL_EMISSIVE_MAP)
	{
		program->sendTexture("materialEmissiveMap",NULL,TEX_CODE_2D_MATERIAL_EMISSIVE);
		s_buffers1x1.bindPropertyTexture(material->diffuseEmittance,1);
	}

	if (MATERIAL_TRANSPARENCY_MAP)
	{
		program->sendTexture("materialTransparencyMap",NULL,TEX_CODE_2D_MATERIAL_TRANSPARENCY);
		s_buffers1x1.bindPropertyTexture(material->specularTransmittance,2); // 2 = RGBA
	}
}

void UberProgramSetup::useIlluminationEnvMaps(Program* program, rr::RRObjectIllumination* illumination)
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationEnvMaps(program=NULL).\n"));
		return;
	}
	if (!illumination)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationEnvMaps(illumination=NULL).\n"));
		return;
	}
	if (LIGHT_INDIRECT_ENV_DIFFUSE && MATERIAL_DIFFUSE)
	{
		if (!illumination->diffuseEnvMap)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useIlluminationEnvMaps: diffuseEnvMap==NULL.\n"));
		}
		program->sendTexture("lightIndirectDiffuseEnvMap",getTexture(illumination->diffuseEnvMap,false,false),TEX_CODE_CUBE_LIGHT_INDIRECT_DIFFUSE);
	}
	if (LIGHT_INDIRECT_ENV_SPECULAR && MATERIAL_SPECULAR)
	{
		if (!illumination->specularEnvMap)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useIlluminationEnvMaps: specularEnvMap==NULL.\n"));
		}
		program->sendTexture("lightIndirectSpecularEnvMap",getTexture(illumination->specularEnvMap,false,false),TEX_CODE_CUBE_LIGHT_INDIRECT_SPECULAR);
	}
}

void UberProgramSetup::useWorldMatrix(Program* program, const rr::RRObject* object)
{
	if (OBJECT_SPACE && object)
	{
		const rr::RRMatrix3x4* world = object->getWorldMatrix();
		if (world)
		{
			float worldMatrix[16] =
			{
				world->m[0][0],world->m[1][0],world->m[2][0],0,
				world->m[0][1],world->m[1][1],world->m[2][1],0,
				world->m[0][2],world->m[1][2],world->m[2][2],0,
				world->m[0][3],world->m[1][3],world->m[2][3],1
			};
			program->sendUniform("worldMatrix",worldMatrix,false,4);
		}
		else
		{
			float worldMatrix[16] =
			{
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1
			};
			program->sendUniform("worldMatrix",worldMatrix,false,4);
		}
	}
}

}; // namespace
