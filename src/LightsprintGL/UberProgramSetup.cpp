// --------------------------------------------------------------------------
// UberProgramSetup, settings specific for our single UberProgram.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
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

void UberProgramSetup::enableAllLights()
{
	// necessary minimum, the rest is enabled automatically according to RRLight processed
	SHADOW_MAPS = 1;
	LIGHT_DIRECT = true;
	LIGHT_DIRECT_COLOR = true;
	LIGHT_DIRECT_MAP = true;
	LIGHT_DIRECT_ATT_SPOT = true;

	// necessary minimum, the rest is enabled automatically according to illumination buffers available
	LIGHT_INDIRECT_CONST = true;
	LIGHT_INDIRECT_VCOLOR = true;
	LIGHT_INDIRECT_MAP = true;
	LIGHT_INDIRECT_DETAIL_MAP = true;
	LIGHT_INDIRECT_ENV_DIFFUSE = true;
	LIGHT_INDIRECT_ENV_SPECULAR = true;
	LIGHT_INDIRECT_MIRROR_DIFFUSE = true;
	LIGHT_INDIRECT_MIRROR_SPECULAR = true;
}

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
	MATERIAL_TRANSPARENCY_FRESNEL = true;
	MATERIAL_BUMP_MAP = true;
	MATERIAL_NORMAL_MAP_FLOW = true;
	MATERIAL_CULLING = true;
}

// true if both material and mesh have everything necessary for mapping
// NULL mesh is trusted to have everything necessary
static bool hasMap(const rr::RRMaterial::Property& property, const rr::RRMeshArrays* meshArrays)
{
	return property.texture && (!meshArrays || (meshArrays->texcoord.size()>property.texcoord && meshArrays->texcoord[property.texcoord]));
}

void UberProgramSetup::enableUsedMaterials(const rr::RRMaterial* material, const rr::RRMeshArrays* meshArrays)
{
	if (!material)
		return;

	// dif
	MATERIAL_DIFFUSE_X2 = false;
	MATERIAL_DIFFUSE = material->diffuseReflectance.color!=rr::RRVec3(0);
	MATERIAL_DIFFUSE_MAP = hasMap(material->diffuseReflectance, meshArrays);
	MATERIAL_DIFFUSE_CONST = !MATERIAL_DIFFUSE_MAP && material->diffuseReflectance.color!=rr::RRVec3(1);

	// spec
	MATERIAL_SPECULAR = material->specularReflectance.color!=rr::RRVec3(0);
	MATERIAL_SPECULAR_MAP = hasMap(material->specularReflectance,meshArrays);
	MATERIAL_SPECULAR_CONST = !MATERIAL_SPECULAR_MAP && material->specularReflectance.color!=rr::RRVec3(1);
	MATERIAL_SPECULAR_MODEL = material->specularModel;

	// emi
	MATERIAL_EMISSIVE_MAP = hasMap(material->diffuseEmittance,meshArrays);
	MATERIAL_EMISSIVE_CONST = !MATERIAL_EMISSIVE_MAP && material->diffuseEmittance.color!=rr::RRVec3(0);

	// transp
	MATERIAL_TRANSPARENCY_MAP = hasMap(material->specularTransmittance,meshArrays);
	MATERIAL_TRANSPARENCY_CONST = !MATERIAL_TRANSPARENCY_MAP && material->specularTransmittance.color!=rr::RRVec3(0);
	MATERIAL_TRANSPARENCY_IN_ALPHA = material->specularTransmittance.color!=rr::RRVec3(0) && material->specularTransmittanceInAlpha;
	MATERIAL_TRANSPARENCY_BLEND = material->specularTransmittance.color!=rr::RRVec3(0) && !material->specularTransmittanceKeyed;
	MATERIAL_TRANSPARENCY_TO_RGB = MATERIAL_TRANSPARENCY_BLEND;
	MATERIAL_TRANSPARENCY_FRESNEL = (MATERIAL_EMISSIVE_CONST || MATERIAL_TRANSPARENCY_BLEND) && material->refractionIndex!=1;

	// normal map
	MATERIAL_BUMP_MAP = hasMap(material->bumpMap,meshArrays); // [#11] we keep normal map enabled even without tangentspace. missing tangents are generated in vertex shader
	MATERIAL_NORMAL_MAP_FLOW = strstr(material->name.c_str(),"water")!=NULL;

	// misc
	MATERIAL_CULLING = material->sideBits[0].renderFrom != material->sideBits[1].renderFrom;
}

const char* UberProgramSetup::getSetupString()
{
	RR_ASSERT(!MATERIAL_TRANSPARENCY_CONST || !MATERIAL_TRANSPARENCY_MAP); // engine does not support both together

	char shadowMaps[50];
	char shadowSamples[50];
	char specularModel[50];
	sprintf(shadowMaps,"#define SHADOW_MAPS %d\n",(int)SHADOW_MAPS);
	sprintf(shadowSamples,"#define SHADOW_SAMPLES %d\n",(int)SHADOW_SAMPLES);
	sprintf(specularModel,"#define MATERIAL_SPECULAR_MODEL %d\n",(int)MATERIAL_SPECULAR_MODEL);

	static char setup[2000];
	sprintf(setup,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		comment?comment:"",
		SHADOW_MAPS?shadowMaps:"",
		SHADOW_SAMPLES?shadowSamples:"",
		SHADOW_COLOR?"#define SHADOW_COLOR\n":"",
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
		LIGHT_INDIRECT_MIRROR_DIFFUSE?"#define LIGHT_INDIRECT_MIRROR_DIFFUSE\n":"",
		LIGHT_INDIRECT_MIRROR_SPECULAR?"#define LIGHT_INDIRECT_MIRROR_SPECULAR\n":"",
		MATERIAL_DIFFUSE?"#define MATERIAL_DIFFUSE\n":"",
		MATERIAL_DIFFUSE_X2?"#define MATERIAL_DIFFUSE_X2\n":"",
		MATERIAL_DIFFUSE_CONST?"#define MATERIAL_DIFFUSE_CONST\n":"",
		MATERIAL_DIFFUSE_MAP?"#define MATERIAL_DIFFUSE_MAP\n":"",
		MATERIAL_SPECULAR?"#define MATERIAL_SPECULAR\n":"",
		MATERIAL_SPECULAR_CONST?"#define MATERIAL_SPECULAR_CONST\n":"",
		MATERIAL_SPECULAR_MAP?"#define MATERIAL_SPECULAR_MAP\n":"",
		MATERIAL_SPECULAR?specularModel:"",
		MATERIAL_EMISSIVE_CONST?"#define MATERIAL_EMISSIVE_CONST\n":"",
		MATERIAL_EMISSIVE_MAP?"#define MATERIAL_EMISSIVE_MAP\n":"",
		MATERIAL_TRANSPARENCY_CONST?"#define MATERIAL_TRANSPARENCY_CONST\n":"",
		MATERIAL_TRANSPARENCY_MAP?"#define MATERIAL_TRANSPARENCY_MAP\n":"",
		MATERIAL_TRANSPARENCY_IN_ALPHA?"#define MATERIAL_TRANSPARENCY_IN_ALPHA\n":"",
		MATERIAL_TRANSPARENCY_BLEND?"#define MATERIAL_TRANSPARENCY_BLEND\n":"",
		MATERIAL_TRANSPARENCY_TO_RGB?"#define MATERIAL_TRANSPARENCY_TO_RGB\n":"",
		MATERIAL_TRANSPARENCY_FRESNEL?"#define MATERIAL_TRANSPARENCY_FRESNEL\n":"",
		MATERIAL_BUMP_MAP?"#define MATERIAL_BUMP_MAP\n":"",
		MATERIAL_NORMAL_MAP_FLOW?"#define MATERIAL_NORMAL_MAP_FLOW\n":"",
		ANIMATION_WAVE?"#define ANIMATION_WAVE\n":"",
		POSTPROCESS_NORMALS?"#define POSTPROCESS_NORMALS\n":"",
		POSTPROCESS_BRIGHTNESS?"#define POSTPROCESS_BRIGHTNESS\n":"",
		POSTPROCESS_GAMMA?"#define POSTPROCESS_GAMMA\n":"",
		POSTPROCESS_BIGSCREEN?"#define POSTPROCESS_BIGSCREEN\n":"",
		OBJECT_SPACE?"#define OBJECT_SPACE\n":"",
		CLIP_PLANE?"#define CLIP_PLANE\n":"",
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
	MATERIAL_TRANSPARENCY_FRESNEL  &= fullMaterial.MATERIAL_TRANSPARENCY_FRESNEL;
	MATERIAL_BUMP_MAP              &= fullMaterial.MATERIAL_BUMP_MAP;
	MATERIAL_NORMAL_MAP_FLOW       &= fullMaterial.MATERIAL_NORMAL_MAP_FLOW;
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
	if (LIGHT_INDIRECT_CONST && !(MATERIAL_DIFFUSE || (MATERIAL_SPECULAR && !LIGHT_INDIRECT_ENV_SPECULAR && !LIGHT_INDIRECT_MIRROR_SPECULAR)))
	{
		// diffuse component always applies LIGHT_INDIRECT_CONST, specular component ignores it when rendering LIGHT_INDIRECT_ENV/MIRROR_SPECULAR
		LIGHT_INDIRECT_CONST = false;
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
	if (!LIGHT_DIRECT && !LIGHT_INDIRECT_CONST && !LIGHT_INDIRECT_VCOLOR && !LIGHT_INDIRECT_MAP && !LIGHT_INDIRECT_MAP2 && !LIGHT_INDIRECT_ENV_DIFFUSE && !LIGHT_INDIRECT_MIRROR_DIFFUSE)
	{
		LIGHT_INDIRECT_DETAIL_MAP = 0;
		MATERIAL_DIFFUSE = 0; // diffuse reflection requested, but there's no suitable light
	}
	if (!LIGHT_DIRECT
		//&& !LIGHT_INDIRECT_CONST ...why was it here, bug? constant indirect does not affect specular
		//&& !LIGHT_INDIRECT_ENV_DIFFUSE // env diffuse needs normals, but it doesn't affect specular
		&& !LIGHT_INDIRECT_ENV_SPECULAR
		&& !LIGHT_INDIRECT_MIRROR_SPECULAR)
	{
		MATERIAL_SPECULAR = 0; // specular reflection requested, but there's no suitable light
	}
	if ((LIGHT_INDIRECT_VCOLOR && !MATERIAL_BUMP_MAP) || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_MAP2)
	{
		// when there are two sources of indirect illumination, disable one of them
		//  (unless it is LIGHT_INDIRECT_VCOLOR with normal maps enabled, then we render average of both sources, VCOLOR does not respond to normal)
		// this must be done before "if (!LIGHT_INDIRECT_ENV_DIFFUSE) MATERIAL_BUMP_MAP = 0"
		LIGHT_INDIRECT_ENV_DIFFUSE = 0;
	}
	if (!MATERIAL_TRANSPARENCY_BLEND // keeps Fresnel in final renders with blending
		&& !MATERIAL_EMISSIVE_CONST // keeps Fresnel on deep water surface
		&& !MATERIAL_TRANSPARENCY_TO_RGB) // keeps Fresnel in Fresnel shadows
	//if (!MATERIAL_TRANSPARENCY_CONST && !MATERIAL_TRANSPARENCY_MAP && !MATERIAL_TRANSPARENCY_IN_ALPHA) // fresnel would work with keyed transparency, but difference would be hardly visible
	{
		// _FRESNEL turns a bit of transparency into specular
		// it can't work without transparency (e.g. with specular only)
		MATERIAL_TRANSPARENCY_FRESNEL = 0;
	}
	if (!MATERIAL_SPECULAR
		&& !LIGHT_INDIRECT_ENV_DIFFUSE // env diffuse can use normal maps, even if specular is disabled
		&& !LIGHT_INDIRECT_MIRROR_DIFFUSE // mirror diffuse can use normal maps, even if specular is disabled
		&& !MATERIAL_TRANSPARENCY_FRESNEL) // fresnel can use normal maps, even if specular is disabled
	{
		MATERIAL_BUMP_MAP = 0; // no use for normal map
	}
	if (!MATERIAL_DIFFUSE)
	{
		MATERIAL_DIFFUSE_X2 = 0;
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_MAP = 0;
		LIGHT_INDIRECT_MAP = 0; // lightmap is used only for diffuse reflection, alpha is ignored
		LIGHT_INDIRECT_DETAIL_MAP = 0; // LDM is used only for diffuse reflection, alpha is ignored
		LIGHT_INDIRECT_ENV_DIFFUSE = 0;
		LIGHT_INDIRECT_MIRROR_DIFFUSE = 0;
	}
	if (!MATERIAL_SPECULAR)
	{
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
		LIGHT_INDIRECT_ENV_SPECULAR = 0;
		LIGHT_INDIRECT_MIRROR_SPECULAR = 0;
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
	bool incomingLight = LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || LIGHT_INDIRECT_MIRROR_DIFFUSE || LIGHT_INDIRECT_MIRROR_SPECULAR;
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
		LIGHT_INDIRECT_MIRROR_DIFFUSE = 0;
		LIGHT_INDIRECT_MIRROR_SPECULAR = 0;
		MATERIAL_DIFFUSE = 0;
		MATERIAL_DIFFUSE_X2 = 0;
		MATERIAL_DIFFUSE_CONST = 0;
		MATERIAL_DIFFUSE_MAP = 0;
		MATERIAL_SPECULAR = 0;
		MATERIAL_SPECULAR_CONST = 0;
		MATERIAL_SPECULAR_MAP = 0;
		if (!MATERIAL_TRANSPARENCY_FRESNEL)
			MATERIAL_BUMP_MAP = 0;
		if (!MATERIAL_EMISSIVE_CONST && !MATERIAL_EMISSIVE_MAP)
		{
			POSTPROCESS_BRIGHTNESS = 0;
			POSTPROCESS_GAMMA = 0;
		}
	}
	if (!MATERIAL_BUMP_MAP)
		MATERIAL_NORMAL_MAP_FLOW = false;
	if (!LIGHT_INDIRECT_VCOLOR)
		LIGHT_INDIRECT_VCOLOR_PHYSICAL = false;
	if (!SHADOW_MAPS) // SHADOW_SAMPLES does not have to be set, sometimes we call validate() after enableAllLights() (so SHADOW_MAPS is set), but before getNextPass() (so SHADOW_SAMPLES is not yet set)
	{
		SHADOW_MAPS = 0;
		SHADOW_SAMPLES = 0;
		SHADOW_COLOR = 0;
		SHADOW_PENUMBRA = 0;
		SHADOW_CASCADE = 0;
		SHADOW_ONLY = 0;
	}
}

Program* UberProgramSetup::useProgram(UberProgram* uberProgram, const rr::RRCamera* camera, RealtimeLight* light, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, const ClipPlanes* clipPlanes)
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
			// set depth border
			// this makes Sun overshoot, illuminate geometry outside its shadowmap range
			rr::RRVec4 depthBorder((LIGHT_DIRECTIONAL && i==0)?1.0f:0);
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &depthBorder.x);
			// set matrix
			rr::RRCamera lightInstance;
			light->getShadowmapCamera(firstInstance+i,lightInstance);
			double m1[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 1,1,1,2};
			double m2[16];
			float m3[16];
#define MULT_MATRIX(a,_b,c,ctype) { const double* b = _b; for (unsigned i=0;i<4;i++) for (unsigned j=0;j<4;j++) c[4*i+j] = (ctype)( b[4*i]*a[j] + b[4*i+1]*a[4+j] + b[4*i+2]*a[8+j] + b[4*i+3]*a[12+j] ); }
			MULT_MATRIX(m1,lightInstance.getProjectionMatrix(),m2,double);
			MULT_MATRIX(m2,lightInstance.getViewMatrix(),m3,float);
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
		program->sendUniform("shadowBlurWidth",rr::RRVec4(1.5f,1.5f,2.5f,-2.5f)/shadowmapSize);
	}

	if (LIGHT_DIRECT)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set (LIGHT_DIRECT set).\n");
			return false;
		}
		if (!light->getCamera())
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: light->getCamera()==NULL.\n");
			return false;
		}

		if (LIGHT_DIRECT_MAP && !SHADOW_MAPS)
		{
			double m1[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 1,1,1,2};
			double m2[16];
			float m3[16];
			MULT_MATRIX(m1,light->getCamera()->getProjectionMatrix(),m2,double);
			MULT_MATRIX(m2,light->getCamera()->getViewMatrix(),m3,float);
			program->sendUniform("textureMatrixL",m3,false,4);
		}
		if (LIGHT_DIRECTIONAL || LIGHT_DIRECT_ATT_SPOT)
		{
			program->sendUniform("worldLightDir",light->getCamera()->getDirection());
		}
		if (!LIGHT_DIRECTIONAL)
		{
			program->sendUniform("worldLightPos",light->getCamera()->getPosition());
		}
	}

	if (LIGHT_DIRECT_COLOR)
	{
		if (!light)
		{
			rr::RRReporter::report(rr::ERRO,"useProgram: no light set (LIGHT_DIRECT_COLOR set).\n");
			return false;
		}
		rr::RRVec4 color(light->getRRLight().color,1);
		if (light->getRRLight().distanceAttenuationType!=rr::RRLight::POLYNOMIAL)
		{
			color[0] = color[0]<0?-pow(-color[0],0.45f):pow(color[0],0.45f);
			color[1] = color[1]<0?-pow(-color[1],0.45f):pow(color[1],0.45f);
			color[2] = color[2]<0?-pow(-color[2],0.45f):pow(color[2],0.45f);
		}
		program->sendUniform("lightDirectColor",color);
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
		program->sendUniform("lightDistancePolynom",light->getRRLight().polynom);
	}

	if (LIGHT_DIRECT_ATT_EXPONENTIAL)
	{
		RR_ASSERT(light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL);
		program->sendUniform("lightDistanceRadius",light->getRRLight().radius);
		program->sendUniform("lightDistanceFallOffExponent",light->getRRLight().fallOffExponent);
	}

	if (LIGHT_INDIRECT_CONST && (MATERIAL_DIFFUSE || (MATERIAL_SPECULAR && !LIGHT_INDIRECT_ENV_SPECULAR && !LIGHT_INDIRECT_MIRROR_SPECULAR))) // shader ignores LIGHT_INDIRECT_CONST when rendering LIGHT_INDIRECT_ENV/MIRROR_SPECULAR
	{
		program->sendUniform("lightIndirectConst",rr::RRVec4(0.2f,0.2f,0.2f,1.0f));
	}

	if (POSTPROCESS_BRIGHTNESS
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || LIGHT_INDIRECT_MIRROR_DIFFUSE || LIGHT_INDIRECT_MIRROR_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		program->sendUniform("postprocessBrightness", brightness?*brightness:rr::RRVec4(1.0f));
	}

	if (POSTPROCESS_GAMMA
		// sendUniform is crybaby, don't call it if uniform doesn't exist
		// uniform is unused (and usually removed by shader compiler) when there is no light
		&& (LIGHT_DIRECT || LIGHT_INDIRECT_CONST || LIGHT_INDIRECT_VCOLOR || LIGHT_INDIRECT_MAP || LIGHT_INDIRECT_ENV_DIFFUSE || LIGHT_INDIRECT_ENV_SPECULAR || LIGHT_INDIRECT_MIRROR_DIFFUSE || LIGHT_INDIRECT_MIRROR_SPECULAR || MATERIAL_EMISSIVE_CONST || MATERIAL_EMISSIVE_MAP))
	{
		program->sendUniform("postprocessGamma", gamma);
	}

	if ((MATERIAL_SPECULAR && (LIGHT_DIRECT || LIGHT_INDIRECT_ENV_SPECULAR)) || MATERIAL_TRANSPARENCY_FRESNEL)
	{
		if (camera)
		{
			// for othogonal camera, move position way back, so that view directions are roughly the same
			// it is not worth creating another ubershader option
			program->sendUniform("worldEyePos",camera->isOrthogonal()?camera->getPosition()-camera->getDirection()*camera->getFar()*1000:camera->getPosition());
		}
		else
		{
			RR_ASSERT(0);
		}
	}

	if (CLIP_PLANE)
	{
		program->sendUniform("clipPlane",clipPlanes?clipPlanes->clipPlane:rr::RRVec4(1));
	}
	if (CLIP_PLANE_XA)
	{
		program->sendUniform("clipPlaneXA",clipPlanes?clipPlanes->clipPlaneXA:0);
	}
	if (CLIP_PLANE_XB)
	{
		program->sendUniform("clipPlaneXB",clipPlanes?clipPlanes->clipPlaneXB:0);
	}
	if (CLIP_PLANE_YA)
	{
		program->sendUniform("clipPlaneYA",clipPlanes?clipPlanes->clipPlaneYA:0);
	}
	if (CLIP_PLANE_YB)
	{
		program->sendUniform("clipPlaneYB",clipPlanes?clipPlanes->clipPlaneYB:0);
	}
	if (CLIP_PLANE_ZA)
	{
		program->sendUniform("clipPlaneZA",clipPlanes?clipPlanes->clipPlaneZA:0);
	}
	if (CLIP_PLANE_ZB)
	{
		program->sendUniform("clipPlaneZB",clipPlanes?clipPlanes->clipPlaneZB:0);
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
		program->sendUniform("materialDiffuseConst",rr::RRVec4(material->diffuseReflectance.color,1.0f));
	}

	if (MATERIAL_SPECULAR && (LIGHT_DIRECT || LIGHT_INDIRECT_ENV_SPECULAR || LIGHT_INDIRECT_MIRROR_SPECULAR))
	{
		float shininess = material->specularShininess;
		float spreadAngle; // how far from reflection angle reflection intensity is 0.5 (with intensity averaged over hemisphere 1)
		float miplevel; // miplevel 0=sample from 1x1x6, miplevel 1=2x2x6, miplevel 2=4x4x6...
		switch (MATERIAL_SPECULAR_MODEL)
		{
			case rr::RRMaterial::PHONG:
				shininess = RR_CLAMPED(material->specularShininess,1,1e10f);
				spreadAngle = acos(pow(0.5f/(shininess+1),1/shininess));
				miplevel = (spreadAngle<=0) ? 15.f : log(spreadAngle/RR_DEG2RAD(45))/log(0.5f); // theoretically correct for 45->0, 22->1, 11->2 ...
				break;
			case rr::RRMaterial::BLINN_PHONG:
				shininess = RR_CLAMPED(material->specularShininess,1,1e10f);
				spreadAngle = acos(pow(1/(shininess+1),1/shininess))*2; // estimate: 2x higher than PHONG
				miplevel = (spreadAngle<=0) ? 15.f : log(spreadAngle/RR_DEG2RAD(45))/log(0.5f);
				break;
			case rr::RRMaterial::TORRANCE_SPARROW:
				shininess = RR_CLAMPED(material->specularShininess*material->specularShininess,0.001f,1);
				miplevel = (1-shininess)*6;
				break;
			case rr::RRMaterial::BLINN_TORRANCE_SPARROW:
				shininess = RR_CLAMPED(material->specularShininess*material->specularShininess,0,1);
				miplevel = (1-shininess)*6;
				break;
		}
		program->sendUniform("materialSpecularShininessData",shininess,miplevel);
	}

	if (MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform("materialSpecularConst",rr::RRVec4(material->specularReflectance.color,1.0f));
	}

	if (MATERIAL_EMISSIVE_CONST)
	{
		program->sendUniform("materialEmissiveConst",rr::RRVec4(material->diffuseEmittance.color,0.0f));
	}

	if (MATERIAL_TRANSPARENCY_CONST)
	{
		program->sendUniform("materialTransparencyConst",rr::RRVec4(material->specularTransmittance.color,1-material->specularTransmittance.color.avg()));
	}

	if (MATERIAL_TRANSPARENCY_FRESNEL)
	{
		program->sendUniform("materialRefractionIndex",material->refractionIndex);
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

	if (MATERIAL_BUMP_MAP)
	{
		program->sendTexture("materialNormalMap",NULL,TEX_CODE_2D_MATERIAL_NORMAL);
		getTexture(material->bumpMap.texture,true,false);
		s_buffers1x1.bindPropertyTexture(material->bumpMap,1);
		if (MATERIAL_NORMAL_MAP_FLOW)
		{
			static rr::RRTime time;
			float secondsPassed = time.secondsPassed();
			if (secondsPassed>1000)
				time.addSeconds(1000);
			program->sendUniform("seconds",secondsPassed);
		}
	}
}

void UberProgramSetup::useIlluminationEnvMap(Program* program, const rr::RRBuffer* reflectionEnvMap)
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationEnvMaps(program=NULL).\n"));
		return;
	}
	if ((LIGHT_INDIRECT_ENV_DIFFUSE && MATERIAL_DIFFUSE) || (LIGHT_INDIRECT_ENV_SPECULAR && MATERIAL_SPECULAR))
	{
		if (!reflectionEnvMap)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useIlluminationEnvMaps: reflectionEnvMap==NULL.\n"));
			return;
		}
		program->sendTexture("lightIndirectEnvMap",getTexture(reflectionEnvMap,true,false),TEX_CODE_CUBE_LIGHT_INDIRECT);
		unsigned w = reflectionEnvMap->getWidth();
		unsigned numLevels = 1;
		while (w>1) {w = w/2; numLevels++;}
		program->sendUniform("lightIndirectEnvMapNumLods",(float)numLevels);
	}
}

void UberProgramSetup::useIlluminationMirror(Program* program, const rr::RRBuffer* mirrorMap)
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useIlluminationMirror(program=NULL).\n"));
		return;
	}
	if ((MATERIAL_DIFFUSE && LIGHT_INDIRECT_MIRROR_DIFFUSE) || (MATERIAL_SPECULAR && LIGHT_INDIRECT_MIRROR_SPECULAR))
	{
		if (!mirrorMap)
		{
			// don't warn, this happens when we skip occluded mirror (but it already has mirroring enabled in material)
			//RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useIlluminationMirror: mirrorMap==NULL.\n"));
			return;
		}
		program->sendTexture("lightIndirectMirrorMap",getTexture(mirrorMap,true,false),TEX_CODE_2D_LIGHT_INDIRECT_MIRROR);
	}
	if (MATERIAL_SPECULAR && LIGHT_INDIRECT_MIRROR_SPECULAR)
	{
		float numLevels = logf((float)(mirrorMap->getWidth()+mirrorMap->getHeight()))/logf(2)-4;
		program->sendUniform("lightIndirectMirrorData",rr::RRVec3(1.f/mirrorMap->getWidth(),1.f/mirrorMap->getHeight(),numLevels));
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
