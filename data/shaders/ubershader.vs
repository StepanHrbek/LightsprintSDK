// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Ubershader
//
// Options:
//  #define SHADOW_MAPS [1..10]
//  #define SHADOW_SAMPLES [1|2|4|8]
//  #define SHADOW_COLOR
//  #define SHADOW_PENUMBRA
//  #define SHADOW_CASCADE
//  #define SHADOW_ONLY
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_COLOR
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_DIRECTIONAL
//  #define LIGHT_DIRECT_ATT_SPOT
//  #define LIGHT_DIRECT_ATT_REALISTIC
//  #define LIGHT_DIRECT_ATT_POLYNOMIAL
//  #define LIGHT_DIRECT_ATT_EXPONENTIAL
//  #define LIGHT_INDIRECT_CONST
//  #define LIGHT_INDIRECT_VCOLOR
//  #define LIGHT_INDIRECT_VCOLOR2
//  #define LIGHT_INDIRECT_VCOLOR_LINEAR
//  #define LIGHT_INDIRECT_MAP
//  #define LIGHT_INDIRECT_MAP2
//  #define LIGHT_INDIRECT_DETAIL_MAP
//  #define LIGHT_INDIRECT_ENV_DIFFUSE
//  #define LIGHT_INDIRECT_ENV_SPECULAR
//  #define LIGHT_INDIRECT_ENV_REFRACT
//  #define LIGHT_INDIRECT_MIRROR_DIFFUSE
//  #define LIGHT_INDIRECT_MIRROR_SPECULAR
//  #define LIGHT_INDIRECT_MIRROR_MIPMAPS
//  #define MATERIAL_DIFFUSE
//  #define MATERIAL_DIFFUSE_X2
//  #define MATERIAL_DIFFUSE_CONST
//  #define MATERIAL_DIFFUSE_MAP
//  #define MATERIAL_SPECULAR
//  #define MATERIAL_SPECULAR_CONST
//  #define MATERIAL_SPECULAR_MAP
//  #define MATERIAL_EMISSIVE_CONST
//  #define MATERIAL_EMISSIVE_MAP
//  #define MATERIAL_TRANSPARENCY_CONST
//  #define MATERIAL_TRANSPARENCY_MAP
//  #define MATERIAL_TRANSPARENCY_IN_ALPHA
//  #define MATERIAL_TRANSPARENCY_BLEND
//  #define MATERIAL_TRANSPARENCY_TO_RGB
//  #define MATERIAL_TRANSPARENCY_FRESNEL
//  #define MATERIAL_BUMP_MAP
//  #define MATERIAL_BUMP_TYPE_HEIGHT
//  #define MATERIAL_NORMAL_MAP_FLOW
//  #define ANIMATION_WAVE
//  #define POSTPROCESS_NORMALS
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define POSTPROCESS_BIGSCREEN
//  #define OBJECT_SPACE
//  #define CLIP_PLANE_[X|Y|Z][A|B]
//  #define FORCE_2D_POSITION

#define sqr(a) ((a)*(a))

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

#ifdef OBJECT_SPACE
	uniform mat4 worldMatrix;
	uniform mat3 inverseWorldMatrix;
#endif

#if defined(SHADOW_MAPS)
#if SHADOW_MAPS>0
	uniform mat4 textureMatrix0;
	out vec4 shadowCoord0;
#endif
#if SHADOW_MAPS>1
	uniform mat4 textureMatrix1;
	out vec4 shadowCoord1;
#endif
#if SHADOW_MAPS>2
	uniform mat4 textureMatrix2;
	out vec4 shadowCoord2;
#endif
#if SHADOW_MAPS>3
	uniform mat4 textureMatrix3;
	out vec4 shadowCoord3;
#endif
#if SHADOW_MAPS>4
	uniform mat4 textureMatrix4;
	out vec4 shadowCoord4;
#endif
#if SHADOW_MAPS>5
	uniform mat4 textureMatrix5;
	out vec4 shadowCoord5;
#endif
#if SHADOW_MAPS>6
	uniform mat4 textureMatrix6;
	out vec4 shadowCoord6;
#endif
#if SHADOW_MAPS>7
	uniform mat4 textureMatrix7;
	out vec4 shadowCoord7;
#endif
#if SHADOW_MAPS>8
	uniform mat4 textureMatrix8;
	out vec4 shadowCoord8;
#endif
#if SHADOW_MAPS>9
	uniform mat4 textureMatrix9;
	out vec4 shadowCoord9;
#endif
#endif

#if defined(LIGHT_DIRECT_MAP) && !defined(SHADOW_MAPS)
	uniform mat4 textureMatrixL;
	out vec4 lightCoord;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR
	layout(location = 9) in vec4 vertexColor;
	out vec4 lightIndirectColor;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR2
	// FIXME: OpenGL 2.0 path did not define location with glBindAttribLocation, how did it even work?
	layout(location = ?) in vec4 vertexColor2;
	uniform float lightIndirectBlend;
#endif

#if defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_DETAIL_MAP)
	layout(location = 7) in vec2 vertexUvUnwrap;
	out vec2 lightIndirectCoord;
#endif

#if defined(LIGHT_INDIRECT_MIRROR_DIFFUSE) || defined(LIGHT_INDIRECT_MIRROR_SPECULAR)
	out vec4 lightIndirectMirrorCoord;
#endif

out vec3 worldPos;
out vec3 worldNormalSmooth;

#ifdef MATERIAL_DIFFUSE_MAP
	layout(location = 2) in vec2 vertexUvDiffuse;
	out vec2 materialDiffuseCoord;
#endif

#ifdef MATERIAL_SPECULAR_MAP
	layout(location = 3) in vec2 vertexUvSpecular;
	out vec2 materialSpecularCoord;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	layout(location = 4) in vec2 vertexUvEmissive;
	out vec2 materialEmissiveCoord;
#endif

#ifdef MATERIAL_TRANSPARENCY_MAP
	layout(location = 5) in vec2 vertexUvTransparent;
	out vec2 materialTransparencyCoord;
#endif

#ifdef MATERIAL_BUMP_MAP
	layout(location = 10) in vec3 vertexTangent;
	layout(location = 11) in vec3 vertexBitangent;
	layout(location = 6) in vec2 vertexUvBump;
	out vec3 worldTangent;
	out vec3 worldBitangent;
	out vec2 materialBumpMapCoord;
#endif

#ifdef FORCE_2D_POSITION
	layout(location = 8) in vec2 vertexUvForced2D;
#else
	uniform mat4 modelViewProjectionMatrix;
#endif

#ifdef ANIMATION_WAVE
	uniform float animationTime;
#endif

void main()
{
	#ifdef OBJECT_SPACE
		vec4 worldPos4 = worldMatrix * vec4(vertexPosition,1.0);
		worldNormalSmooth = normalize( inverseWorldMatrix * vertexNormal );
	#else
		vec4 worldPos4 = vec4(vertexPosition,1.0);
		worldNormalSmooth = normalize( vertexNormal );
	#endif
	#ifdef ANIMATION_WAVE
		worldPos4.z += 0.1*sin(animationTime+5.0*worldPos4.y); // arbitrary procedural deformation
		float tga = -0.5*cos(animationTime+5.0*worldPos4.y); // its derivation
		float cosa = 1.0/sqrt(tga*tga+1.0);
		float sina = tga*cosa;
		worldNormalSmooth.yz = vec2(
			sina*worldNormalSmooth.z+cosa*worldNormalSmooth.y,
			cosa*worldNormalSmooth.z-sina*worldNormalSmooth.y);
	#endif
	worldPos = worldPos4.xyz;

	#ifdef LIGHT_INDIRECT_VCOLOR
		#ifdef LIGHT_INDIRECT_VCOLOR2
			lightIndirectColor = vertexColor*(1.0-lightIndirectBlend)+vertexColor2*lightIndirectBlend;
		#else
			lightIndirectColor = vertexColor;
		#endif
		#ifdef LIGHT_INDIRECT_VCOLOR_LINEAR
			lightIndirectColor = pow(lightIndirectColor,vec4(0.45,0.45,0.45,0.45));
		#endif
	#endif

	#if defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_DETAIL_MAP)
		lightIndirectCoord = vertexUvUnwrap;
	#endif

	#ifdef MATERIAL_DIFFUSE_MAP
		materialDiffuseCoord = vertexUvDiffuse;
	#endif

	#ifdef MATERIAL_SPECULAR_MAP
		materialSpecularCoord = vertexUvSpecular;
	#endif

	#ifdef MATERIAL_EMISSIVE_MAP
		materialEmissiveCoord = vertexUvEmissive;
	#endif

	#ifdef MATERIAL_TRANSPARENCY_MAP
		materialTransparencyCoord = vertexUvTransparent;
	#endif

	#ifdef MATERIAL_BUMP_MAP
		materialBumpMapCoord = vertexUvBump;
		vec3 tangent2, bitangent2;
		if (vertexTangent==vec3(0.0,0.0,0.0))
		{
			// when mesh lacks tangent space, generate something
			vec3 a = worldNormalSmooth;
			vec3 b = tangent2 = (a.x==0.0 && a.y==0.0) ? vec3(0.0,1.0,0.0) : vec3(-a.y,a.x,0.0);
			bitangent2 = vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
		}
		else
		{
			tangent2 = vertexTangent;
			bitangent2 = vertexBitangent;
		}
		#ifdef OBJECT_SPACE
			worldTangent = normalize( inverseWorldMatrix * tangent2 );
			worldBitangent = normalize( inverseWorldMatrix * bitangent2 );
		#else
			worldTangent = normalize(tangent2);
			worldBitangent = normalize(bitangent2);
		#endif
	#endif

	#if defined(SHADOW_MAPS)
	#if SHADOW_MAPS>0
		shadowCoord0 = textureMatrix0 * worldPos4;
	#endif
	#if SHADOW_MAPS>1
		shadowCoord1 = textureMatrix1 * worldPos4;
	#endif
	#if SHADOW_MAPS>2
		shadowCoord2 = textureMatrix2 * worldPos4;
	#endif
	#if SHADOW_MAPS>3
		shadowCoord3 = textureMatrix3 * worldPos4;
	#endif
	#if SHADOW_MAPS>4
		shadowCoord4 = textureMatrix4 * worldPos4;
	#endif
	#if SHADOW_MAPS>5
		shadowCoord5 = textureMatrix5 * worldPos4;
	#endif
	#if SHADOW_MAPS>6
		shadowCoord6 = textureMatrix6 * worldPos4;
	#endif
	#if SHADOW_MAPS>7
		shadowCoord7 = textureMatrix7 * worldPos4;
	#endif
	#if SHADOW_MAPS>8
		shadowCoord8 = textureMatrix8 * worldPos4;
	#endif
	#if SHADOW_MAPS>9
		shadowCoord9 = textureMatrix9 * worldPos4;
	#endif
	#endif

	#if defined(LIGHT_DIRECT_MAP) && !defined(SHADOW_MAPS)
		lightCoord = textureMatrixL * worldPos4;
	#endif
  
	#ifdef FORCE_2D_POSITION
		gl_Position = vec4(vertexUvForced2D,0.5,1.0);
	#else
		gl_Position = modelViewProjectionMatrix * worldPos4;
	#endif

	#if defined(LIGHT_INDIRECT_MIRROR_DIFFUSE) || defined(LIGHT_INDIRECT_MIRROR_SPECULAR)
		lightIndirectMirrorCoord = gl_Position;
	#endif
}
