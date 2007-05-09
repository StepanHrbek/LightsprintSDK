// LightsprintGL: DemoEngine vertex ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_INDIRECT_CONST
//  #define LIGHT_INDIRECT_VCOLOR
//  #define LIGHT_INDIRECT_VCOLOR2
//  #define LIGHT_INDIRECT_VCOLOR_PHYSICAL
//  #define LIGHT_INDIRECT_MAP
//  #define LIGHT_INDIRECT_MAP2
//  #define LIGHT_INDIRECT_ENV
//  #define MATERIAL_DIFFUSE
//  #define MATERIAL_DIFFUSE_CONST
//  #define MATERIAL_DIFFUSE_VCOLOR
//  #define MATERIAL_DIFFUSE_MAP
//  #define MATERIAL_SPECULAR
//  #define MATERIAL_SPECULAR_CONST
//  #define MATERIAL_SPECULAR_MAP
//  #define MATERIAL_NORMAL_MAP
//  #define MATERIAL_EMISSIVE_MAP
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define POSTPROCESS_BIGSCREEN
//  #define OBJECT_SPACE
//  #define FORCE_2D_POSITION
//
// Workarounds for driver bugs of one vendor made it a mess, sorry.
//
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

#ifdef OBJECT_SPACE
	uniform mat4 worldMatrix;
#endif

#if SHADOW_MAPS>0
	varying vec4 shadowCoord[SHADOW_MAPS];
#endif

#if defined(LIGHT_DIRECT) && !defined(MATERIAL_NORMAL_MAP)
	uniform vec3 worldLightPos;
	varying float lightDirectColor;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR
	//varying vec4 lightIndirectColor; // passed rather through gl_FrontColor, ATI fails on anything else
#endif

#ifdef LIGHT_INDIRECT_VCOLOR2
	uniform float lightIndirectBlend;
#endif

#ifdef LIGHT_INDIRECT_MAP
	varying vec2 lightIndirectCoord;
#endif

#ifdef MATERIAL_SPECULAR
varying
#endif
	vec3 worldPos;
#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV)
varying 
#endif
	vec3 worldNormalSmooth;

#ifdef MATERIAL_DIFFUSE_VCOLOR
	varying vec4 materialDiffuseColor;
#endif

#ifdef MATERIAL_DIFFUSE_MAP
	varying vec2 materialDiffuseCoord;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	varying vec2 materialEmissiveCoord;
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
	worldPos = worldPos4.xyz;

	#if defined(LIGHT_DIRECT) && !defined(MATERIAL_NORMAL_MAP)
		lightDirectColor = max(dot(normalize(worldLightPos - worldPos), worldNormalSmooth),0.0);
	#endif

	#ifdef LIGHT_INDIRECT_VCOLOR
		#ifdef LIGHT_INDIRECT_VCOLOR2
			vec4 lightIndirectColor = gl_Color*(1.0-lightIndirectBlend)+gl_SecondaryColor*lightIndirectBlend;
		#else
			vec4 lightIndirectColor = gl_Color;
		#endif
		#ifdef LIGHT_INDIRECT_VCOLOR_PHYSICAL
			gl_FrontColor = pow(lightIndirectColor,vec4(0.45,0.45,0.45,0.45));
		#else
			gl_FrontColor = lightIndirectColor;
		#endif
	#endif

	#ifdef LIGHT_INDIRECT_MAP
		lightIndirectCoord = gl_MultiTexCoord1.xy;
	#endif

	#ifdef MATERIAL_DIFFUSE_MAP
		materialDiffuseCoord = gl_MultiTexCoord0.xy;
	#endif

	#ifdef MATERIAL_DIFFUSE_VCOLOR
		materialDiffuseColor = gl_SecondaryColor;
	#endif

	#ifdef MATERIAL_EMISSIVE_MAP
		materialEmissiveCoord = gl_MultiTexCoord3.xy;
	#endif

	// for cycle (for any OpenGL 2.0 compliant card)
	//for(int i=0;i<SHADOW_MAPS;i++)
	//  shadowCoord[i] = gl_TextureMatrix[i] * worldPos4;
	// unrolled version (ATI fails on for cycle)
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

	gl_ClipVertex = worldPos4;
}
