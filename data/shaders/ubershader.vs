// LightsprintGL: vertex ubershader
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
//
// Options:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define SHADOW_BILINEAR
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
//  #define MATERIAL_NORMAL_MAP
//  #define ANIMATION_WAVE
//  #define POSTPROCESS_NORMALS
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define POSTPROCESS_BIGSCREEN
//  #define OBJECT_SPACE
//  #define CLIP_PLANE
//  #define FORCE_2D_POSITION

#define sqr(a) ((a)*(a))

#ifdef OBJECT_SPACE
	uniform mat4 worldMatrix;
#endif

#if SHADOW_MAPS>0
	varying vec4 shadowCoord[SHADOW_MAPS];
#endif

#if defined(LIGHT_DIRECT) && !defined(MATERIAL_NORMAL_MAP)
	#ifdef LIGHT_DIRECTIONAL
		uniform vec3 worldLightDir;
	#else
		uniform vec3 worldLightPos;
	#endif
	varying float lightDirectVColor;
#endif

#ifdef LIGHT_DIRECT_ATT_POLYNOMIAL
	uniform vec4 lightDistancePolynom;
#endif

#ifdef LIGHT_DIRECT_ATT_EXPONENTIAL
	uniform float lightDistanceRadius;
	uniform float lightDistanceFallOffExponent;
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

#if defined(MATERIAL_SPECULAR) || defined(LIGHT_DIRECT_ATT_SPOT) || defined(CLIP_PLANE)
varying
#endif
	vec3 worldPos;
#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(POSTPROCESS_NORMALS)
varying 
#endif
	vec3 worldNormalSmooth;

#ifdef MATERIAL_DIFFUSE_MAP
	varying vec2 materialDiffuseCoord;
#endif

#if (defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_ENV_SPECULAR)) && !defined(FORCE_2D_POSITION)
	uniform vec3 worldEyePos;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	varying vec2 materialEmissiveCoord;
#endif

#ifdef MATERIAL_TRANSPARENCY_MAP
	varying vec2 materialTransparencyCoord;
#endif

#ifdef ANIMATION_WAVE
	uniform float animationTime;
#endif

void main()
{
	#ifdef OBJECT_SPACE
		vec4 worldPos4 = worldMatrix * gl_Vertex;
		worldNormalSmooth = normalize( worldMatrix * vec4(gl_Normal,0.0) ).xyz;
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

	#if defined(LIGHT_DIRECT) && !defined(MATERIAL_NORMAL_MAP)
		#ifdef LIGHT_DIRECTIONAL
			lightDirectVColor = dot(worldLightDir, worldNormalSmooth);
		#else
			lightDirectVColor = dot(normalize(worldPos-worldLightPos), worldNormalSmooth);
			float distance = distance(worldPos,worldLightPos);
			#ifdef LIGHT_DIRECT_ATT_PHYSICAL
				lightDirectVColor *= pow(distance,-0.9);
			#endif
			#ifdef LIGHT_DIRECT_ATT_POLYNOMIAL
				lightDirectVColor /= max( lightDistancePolynom.x + distance*lightDistancePolynom.y + distance*distance*lightDistancePolynom.z, lightDistancePolynom.w );
			#endif
			#ifdef LIGHT_DIRECT_ATT_EXPONENTIAL
				lightDirectVColor *= pow(max(0.0,1.0-sqr(distance/lightDistanceRadius)),lightDistanceFallOffExponent*0.45);
			#endif
		#endif
		#ifdef FORCE_2D_POSITION
			// rendering solver input = direct illumination on front side of 2-sided face
			// ignore camera position, worldEyePos is not set
			lightDirectVColor = max(0.0,-lightDirectVColor);
		#else
			// direct illumination only on lit side of 2-sided face
			lightDirectVColor = (lightDirectVColor*dot(worldPos-worldEyePos,worldNormalSmooth)>0.0) ? abs(lightDirectVColor) : 0.0;
		#endif
	#endif

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
		lightIndirectCoord = gl_MultiTexCoord1.xy;
	#endif

	#ifdef MATERIAL_DIFFUSE_MAP
		materialDiffuseCoord = gl_MultiTexCoord0.xy;
	#endif

	#ifdef MATERIAL_EMISSIVE_MAP
		materialEmissiveCoord = gl_MultiTexCoord3.xy;
	#endif

	#ifdef MATERIAL_TRANSPARENCY_MAP
		materialTransparencyCoord = gl_MultiTexCoord4.xy;
	#endif

	// for cycle (for any OpenGL 2.0 compliant card)
	//for(int i=0;i<SHADOW_MAPS;i++)
	//  shadowCoord[i] = gl_TextureMatrix[i] * worldPos4;
	// unrolled version (this fixed broken render on ATI)
	#if SHADOW_MAPS>0
		shadowCoord[0] = gl_TextureMatrix[0] * worldPos4;
	#endif
	#if SHADOW_MAPS>1
		shadowCoord[1] = gl_TextureMatrix[1] * worldPos4;
	#endif
	#if SHADOW_MAPS>2
		shadowCoord[2] = gl_TextureMatrix[2] * worldPos4;
	#endif
	#if SHADOW_MAPS>3
		shadowCoord[3] = gl_TextureMatrix[3] * worldPos4;
	#endif
	#if SHADOW_MAPS>4
		shadowCoord[4] = gl_TextureMatrix[4] * worldPos4;
	#endif
	#if SHADOW_MAPS>5
		shadowCoord[5] = gl_TextureMatrix[5] * worldPos4;
	#endif
	#if SHADOW_MAPS>6
		shadowCoord[6] = gl_TextureMatrix[6] * worldPos4;
	#endif
	#if SHADOW_MAPS>7
		shadowCoord[7] = gl_TextureMatrix[7] * worldPos4;
	#endif
	#if SHADOW_MAPS>8
		shadowCoord[8] = gl_TextureMatrix[8] * worldPos4;
	#endif
	#if SHADOW_MAPS>9
		shadowCoord[9] = gl_TextureMatrix[9] * worldPos4;
	#endif
  
	#ifdef FORCE_2D_POSITION
		gl_Position = vec4(gl_MultiTexCoord2.x,gl_MultiTexCoord2.y,0.5,1.0);
	#else
		gl_Position = gl_ModelViewProjectionMatrix * worldPos4;
	#endif
}
