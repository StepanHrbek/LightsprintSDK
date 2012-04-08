// LightsprintGL: vertex ubershader
// Copyright (C) 2006-2012 Stepan Hrbek, Lightsprint
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
//  #define LIGHT_DIRECT_ATT_PHYSICAL
//  #define LIGHT_DIRECT_ATT_POLYNOMIAL
//  #define LIGHT_DIRECT_ATT_EXPONENTIAL
//  #define LIGHT_INDIRECT_CONST
//  #define LIGHT_INDIRECT_VCOLOR
//  #define LIGHT_INDIRECT_VCOLOR2
//  #define LIGHT_INDIRECT_VCOLOR_PHYSICAL
//  #define LIGHT_INDIRECT_MAP
//  #define LIGHT_INDIRECT_MAP2
//  #define LIGHT_INDIRECT_DETAIL_MAP
//  #define LIGHT_INDIRECT_ENV_DIFFUSE
//  #define LIGHT_INDIRECT_ENV_SPECULAR
//  #define LIGHT_INDIRECT_MIRROR_DIFFUSE
//  #define LIGHT_INDIRECT_MIRROR_SPECULAR
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
//  #define MATERIAL_NORMAL_MAP
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

#ifdef OBJECT_SPACE
	uniform mat4 worldMatrix;
#endif

#if defined(SHADOW_MAPS)
#if SHADOW_MAPS>0
	uniform mat4 textureMatrix0;
	varying vec4 shadowCoord0;
#endif
#if SHADOW_MAPS>1
	uniform mat4 textureMatrix1;
	varying vec4 shadowCoord1;
#endif
#if SHADOW_MAPS>2
	uniform mat4 textureMatrix2;
	varying vec4 shadowCoord2;
#endif
#if SHADOW_MAPS>3
	uniform mat4 textureMatrix3;
	varying vec4 shadowCoord3;
#endif
#if SHADOW_MAPS>4
	uniform mat4 textureMatrix4;
	varying vec4 shadowCoord4;
#endif
#if SHADOW_MAPS>5
	uniform mat4 textureMatrix5;
	varying vec4 shadowCoord5;
#endif
#if SHADOW_MAPS>6
	uniform mat4 textureMatrix6;
	varying vec4 shadowCoord6;
#endif
#if SHADOW_MAPS>7
	uniform mat4 textureMatrix7;
	varying vec4 shadowCoord7;
#endif
#if SHADOW_MAPS>8
	uniform mat4 textureMatrix8;
	varying vec4 shadowCoord8;
#endif
#if SHADOW_MAPS>9
	uniform mat4 textureMatrix9;
	varying vec4 shadowCoord9;
#endif
#endif

#if defined(LIGHT_DIRECT_MAP) && !defined(SHADOW_MAPS)
	uniform mat4 textureMatrixL;
	varying vec4 lightCoord;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR
	varying vec4 lightIndirectColor;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR2
	uniform float lightIndirectBlend;
#endif

#if defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_DETAIL_MAP)
	varying vec2 lightIndirectCoord;
#endif

#if defined(LIGHT_INDIRECT_MIRROR_DIFFUSE) || defined(LIGHT_INDIRECT_MIRROR_SPECULAR)
	varying vec4 lightIndirectMirrorCoord;
#endif

varying vec3 worldPos;
varying vec3 worldNormalSmooth;

#ifdef MATERIAL_DIFFUSE_MAP
	varying vec2 materialDiffuseCoord;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	varying vec2 materialEmissiveCoord;
#endif

#ifdef MATERIAL_TRANSPARENCY_MAP
	varying vec2 materialTransparencyCoord;
#endif

#ifdef MATERIAL_NORMAL_MAP
	attribute vec3 tangent;
	attribute vec3 bitangent;
	varying vec3 worldTangent;
	varying vec3 worldBitangent;
	varying vec2 materialNormalMapCoord;
#endif

#ifdef ANIMATION_WAVE
	uniform float animationTime;
#endif

void main()
{
	#ifdef OBJECT_SPACE
		vec4 worldPos4 = worldMatrix * gl_Vertex;
		worldNormalSmooth = normalize( ( worldMatrix * vec4(gl_Normal,0.0) ).xyz );
	#else
		vec4 worldPos4 = gl_Vertex;
		worldNormalSmooth = normalize( gl_Normal );
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
			lightIndirectColor = gl_Color*(1.0-lightIndirectBlend)+gl_SecondaryColor*lightIndirectBlend;
		#else
			lightIndirectColor = gl_Color;
		#endif
		#ifdef LIGHT_INDIRECT_VCOLOR_PHYSICAL
			lightIndirectColor = pow(lightIndirectColor,vec4(0.45,0.45,0.45,0.45));
		#endif
	#endif

	#if defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_DETAIL_MAP)
		lightIndirectCoord = gl_MultiTexCoord1.xy; // 1 = MULTITEXCOORD_LIGHT_INDIRECT
	#endif

	#ifdef MATERIAL_DIFFUSE_MAP
		materialDiffuseCoord = gl_MultiTexCoord0.xy; // 0 = MULTITEXCOORD_MATERIAL_DIFFUSE
	#endif

	#ifdef MATERIAL_EMISSIVE_MAP
		materialEmissiveCoord = gl_MultiTexCoord3.xy; // 3 = MULTITEXCOORD_MATERIAL_EMISSIVE
	#endif

	#ifdef MATERIAL_TRANSPARENCY_MAP
		materialTransparencyCoord = gl_MultiTexCoord4.xy; // 4 = MULTITEXCOORD_MATERIAL_TRANSPARENCY
	#endif

	#ifdef MATERIAL_NORMAL_MAP
		materialNormalMapCoord = gl_MultiTexCoord5.xy; // 5 = MULTITEXCOORD_MATERIAL_NORMAL_MAP
		vec3 tangent2, bitangent2;
		if (tangent==vec3(0.0,0.0,0.0))
		{
			// when mesh lacks tangent space, generate something
			vec3 a = worldNormalSmooth;
			vec3 b = tangent2 = (a.x==0.0 && a.y==0.0) ? vec3(0.0,1.0,0.0) : vec3(-a.y,a.x,0.0);
			bitangent2 = vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
		}
		else
		{
			tangent2 = tangent;
			bitangent2 = bitangent;
		}
		#ifdef OBJECT_SPACE
			worldTangent = normalize( ( worldMatrix * vec4(tangent2,0.0) ).xyz );
			worldBitangent = normalize( ( worldMatrix * vec4(bitangent2,0.0) ).xyz );
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
		gl_Position = vec4(gl_MultiTexCoord2.x,gl_MultiTexCoord2.y,0.5,1.0); // 2 = MULTITEXCOORD_FORCED_2D
	#else
		gl_Position = gl_ModelViewProjectionMatrix * worldPos4;
	#endif

	#if defined(LIGHT_INDIRECT_MIRROR_DIFFUSE) || defined(LIGHT_INDIRECT_MIRROR_SPECULAR)
		lightIndirectMirrorCoord = gl_Position;
	#endif
}
