// LightsprintGL: fragment ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define SHADOW_BILINEAR
//  #define SHADOW_PENUMBRA
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_COLOR
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_DISTANCE_PHYSICAL
//  #define LIGHT_DISTANCE_POLYNOMIAL
//  #define LIGHT_DISTANCE_EXPONENTIAL
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
//  #define ANIMATION_WAVE
//  #define POSTPROCESS_NORMALS
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define POSTPROCESS_BIGSCREEN
//  #define OBJECT_SPACE
//  #define CLIPPING
//  #define FORCE_2D_POSITION
//
// Workarounds for driver bugs of one vendor made it a mess, sorry.
//
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

//#if SHADOW_SAMPLES>0 // ATI fails on this line
#if SHADOW_MAPS>0
	uniform sampler2DShadow shadowMap0;
#endif
#if SHADOW_MAPS>1
	uniform sampler2DShadow shadowMap1;
#endif
#if SHADOW_MAPS>2
	uniform sampler2DShadow shadowMap2;
#endif
#if SHADOW_MAPS>3
	uniform sampler2DShadow shadowMap3;
#endif
#if SHADOW_MAPS>4
	uniform sampler2DShadow shadowMap4;
#endif
#if SHADOW_MAPS>5
	uniform sampler2DShadow shadowMap5;
#endif
#if SHADOW_MAPS>6
	uniform sampler2DShadow shadowMap6;
#endif
#if SHADOW_MAPS>7
	uniform sampler2DShadow shadowMap7;
#endif
#if SHADOW_MAPS>8
	uniform sampler2DShadow shadowMap8;
#endif
#if SHADOW_MAPS>9
	uniform sampler2DShadow shadowMap9;
#endif
//#endif

#if SHADOW_MAPS>0
	varying vec4 shadowCoord[SHADOW_MAPS];
#endif

#ifdef LIGHT_DIRECT
	uniform vec3 worldLightPos;
	uniform vec3 worldEyePos;
	#ifndef MATERIAL_NORMAL_MAP
		varying float lightDirectVColor;
	#endif
#endif

#ifdef LIGHT_DIRECT_COLOR
	uniform vec4 lightDirectColor;
#endif

#ifdef LIGHT_DIRECT_MAP
	uniform sampler2D lightDirectMap;
#endif

#ifdef LIGHT_INDIRECT_CONST
	uniform vec4 lightIndirectConst;
#endif

#ifdef LIGHT_INDIRECT_VCOLOR
	//varying vec4 lightIndirectColor; // passed rather through gl_Color, ATI fails on anything else
#endif

#ifdef LIGHT_INDIRECT_MAP
	uniform sampler2D lightIndirectMap;
	varying vec2 lightIndirectCoord;
#endif

#ifdef LIGHT_INDIRECT_MAP2
	uniform sampler2D lightIndirectMap2;
	uniform float lightIndirectBlend;
#endif

#ifdef LIGHT_INDIRECT_ENV
	uniform samplerCube lightIndirectSpecularEnvMap;
	uniform samplerCube lightIndirectDiffuseEnvMap;
#endif

#ifdef MATERIAL_DIFFUSE_CONST
	uniform vec4 materialDiffuseConst;
#endif

#ifdef MATERIAL_DIFFUSE_VCOLOR
	varying vec4 materialDiffuseColor;
#endif

#ifdef MATERIAL_DIFFUSE_MAP
	uniform sampler2D materialDiffuseMap;
	varying vec2 materialDiffuseCoord;
#endif

#ifdef MATERIAL_SPECULAR
	varying vec3 worldPos;
#endif

#ifdef MATERIAL_SPECULAR_CONST
	uniform vec4 materialSpecularConst;
#endif

#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV) || defined(POSTPROCESS_NORMALS)
	varying vec3 worldNormalSmooth;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	uniform sampler2D materialEmissiveMap;
	varying vec2 materialEmissiveCoord;
#endif

#ifdef POSTPROCESS_BRIGHTNESS
	uniform vec4 postprocessBrightness;
#endif

#ifdef POSTPROCESS_GAMMA
	uniform float postprocessGamma;
#endif

void main()
{

	/////////////////////////////////////////////////////////////////////
	//
	// shadowing

	#if SHADOW_SAMPLES*SHADOW_MAPS>0

		float visibility = 0.0; // 0=shadowed, 1=lit

		#if SHADOW_SAMPLES==1
			// hard shadows with 1 lookup
			#define SHADOWMAP_LOOKUP_SUB(shadowMap,index) \
				visibility += shadow2DProj(shadowMap, shadowCoord[index]).z
		#else
			// soft shadows with 2, 4 or 8 lookups in rotating kernel
			#ifdef SHADOW_BILINEAR
				float noise = 8.1*gl_FragCoord.x+5.7*gl_FragCoord.y;
			#else
				float noise = 16.2*gl_FragCoord.x+11.4*gl_FragCoord.y;
			#endif
			vec3 sc = vec3(sin(noise),cos(noise),0.0);
			vec3 shift1 = sc*0.003;
			vec3 shift2 = sc.yxz*vec3(0.006,-0.006,0.0);

			#if SHADOW_SAMPLES==2
				#define SHADOWMAP_LOOKUP_SUB(shadowMap,index) \
				center = shadowCoord[index].xyz/shadowCoord[index].w; \
				visibility += ( \
					shadow2D(shadowMap, center+shift1).z \
					+shadow2D(shadowMap, center-shift1).z )
			#elif SHADOW_SAMPLES==4
				#define SHADOWMAP_LOOKUP_SUB(shadowMap,index) \
				center = shadowCoord[index].xyz/shadowCoord[index].w; \
				visibility += ( \
					shadow2D(shadowMap, center+shift1).z \
					+shadow2D(shadowMap, center-shift1).z \
					+shadow2D(shadowMap, center+shift2).z \
					+shadow2D(shadowMap, center-shift2).z )
			#elif SHADOW_SAMPLES==8
				#define SHADOWMAP_LOOKUP_SUB(shadowMap,index) \
				center = shadowCoord[index].xyz/shadowCoord[index].w; \
				visibility += ( \
					shadow2D(shadowMap, center+shift1).z \
					+shadow2D(shadowMap, center-shift1).z \
					+shadow2D(shadowMap, center+1.8*shift1).z \
					+shadow2D(shadowMap, center-1.8*shift1).z \
					+shadow2D(shadowMap, center+shift2).z \
					+shadow2D(shadowMap, center-shift2).z \
					+shadow2D(shadowMap, center+0.6*shift2).z \
					+shadow2D(shadowMap, center-0.6*shift2).z )
			#endif
		#endif // SHADOW_SAMPLES!=1

		#ifdef LIGHT_DIRECT_MAP
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index)
		#else
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index) * step(0.0,shadowCoord[index].z)
		#endif

		vec3 center;
		#if SHADOW_MAPS>0
			SHADOWMAP_LOOKUP(shadowMap0,0);
		#endif
		#if SHADOW_MAPS>1
			SHADOWMAP_LOOKUP(shadowMap1,1);
		#endif
		#if SHADOW_MAPS>2
			SHADOWMAP_LOOKUP(shadowMap2,2);
		#endif
		#if SHADOW_MAPS>3
			SHADOWMAP_LOOKUP(shadowMap3,3);
		#endif
		#if SHADOW_MAPS>4
			SHADOWMAP_LOOKUP(shadowMap4,4);
		#endif
		#if SHADOW_MAPS>5
			SHADOWMAP_LOOKUP(shadowMap5,5);
		#endif
		#if SHADOW_MAPS>6
			SHADOWMAP_LOOKUP(shadowMap6,6);
		#endif
		#if SHADOW_MAPS>7
			SHADOWMAP_LOOKUP(shadowMap7,7);
		#endif
		#if SHADOW_MAPS>8
			SHADOWMAP_LOOKUP(shadowMap8,8);
		#endif
		#if SHADOW_MAPS>9
			SHADOWMAP_LOOKUP(shadowMap9,9);
		#endif

	#endif // SHADOW_SAMPLES*SHADOW_MAPS>0


	//////////////////////////////////////////////////////////////////////////////
	//
	// material

	#ifdef MATERIAL_DIFFUSE_MAP
		vec4 materialDiffuseMapColor = texture2D(materialDiffuseMap, materialDiffuseCoord);
	#endif
	#ifdef MATERIAL_SPECULAR_MAP
		float materialSpecularReflectance = step(materialDiffuseMapColor.r,0.6);
		float materialDiffuseReflectance = 1.0 - materialSpecularReflectance;
	#endif
	#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV)
		#ifdef MATERIAL_NORMAL_MAP
			vec3 worldNormal = normalize(worldNormalSmooth+materialDiffuseMapColor.rgb-vec3(0.3,0.3,0.3));
		#else
			vec3 worldNormal = worldNormalSmooth; // normalize would slightly improve quality
		#endif
	#endif
	#ifdef MATERIAL_EMISSIVE_MAP
		vec4 materialEmissiveMapColor = texture2D(materialEmissiveMap, materialEmissiveCoord);
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// light direct

	#ifdef LIGHT_DIRECT
		#if defined(MATERIAL_NORMAL_MAP) || defined(MATERIAL_SPECULAR)
			vec3 worldLightDir = normalize(worldLightPos - worldPos);
		#endif
		vec4 lightDirect =
			#ifdef MATERIAL_NORMAL_MAP
				max(0.0,dot(worldLightDir, worldNormal)) // per pixel
			#else
				vec4(lightDirectVColor,lightDirectVColor,lightDirectVColor,lightDirectVColor) // per vertex
			#endif
			#ifdef LIGHT_DIRECT_COLOR
				* lightDirectColor
			#endif
			#ifdef LIGHT_DIRECT_MAP
				* texture2DProj(lightDirectMap, shadowCoord[SHADOW_MAPS/2])
			#endif
			#if SHADOW_SAMPLES*SHADOW_MAPS>0
				#ifdef SHADOW_PENUMBRA
					* visibility/float(SHADOW_SAMPLES*SHADOW_MAPS)
				#else
					* visibility/float(SHADOW_SAMPLES)
				#endif
			#endif
			;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// final mix

	#if defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_ENV)

		#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_INDIRECT_ENV) || defined(LIGHT_DIRECT))
			vec3 worldViewReflected = reflect(worldPos-worldEyePos,worldNormal);
		#endif

		gl_FragColor =

			//
			// diffuse reflection
			//

			#ifdef MATERIAL_DIFFUSE
				#ifdef MATERIAL_DIFFUSE_CONST
					materialDiffuseConst *
				#endif
				#ifdef MATERIAL_DIFFUSE_VCOLOR
					materialDiffuseColor *
				#endif
				#ifdef MATERIAL_DIFFUSE_MAP
					materialDiffuseMapColor *
				#endif
				#ifdef MATERIAL_SPECULAR_MAP
					materialDiffuseReflectance *
				#endif
				( 
					#ifdef LIGHT_DIRECT
						lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_CONST
						+ lightIndirectConst
					#endif
					#ifdef LIGHT_INDIRECT_VCOLOR
						+ gl_Color
					#endif
					#ifdef LIGHT_INDIRECT_MAP
						+ texture2D(lightIndirectMap, lightIndirectCoord)
					#endif
					#ifdef LIGHT_INDIRECT_MAP2
						* (1.0-lightIndirectBlend)
						+ texture2D(lightIndirectMap2, lightIndirectCoord)
						* lightIndirectBlend
					#endif
					#ifdef LIGHT_INDIRECT_ENV
						+ textureCube(lightIndirectDiffuseEnvMap, worldNormal)
					#endif
				)
			#endif


			//
			// specular reflection
			//

			#ifdef MATERIAL_SPECULAR
				+
				#ifdef MATERIAL_DIFFUSE_MAP
					materialDiffuseMapColor.a *
				#endif
				#ifdef MATERIAL_EMISSIVE_MAP
					materialEmissiveMapColor.a *
				#endif
				#ifdef MATERIAL_SPECULAR_CONST
					materialSpecularConst *
				#endif
				#ifdef MATERIAL_SPECULAR_MAP
					materialSpecularReflectance *
				#endif
				(
					#ifdef LIGHT_DIRECT
						+ pow(max(0.0,dot(worldLightDir,normalize(worldViewReflected))),10.0)*2.0
						* lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_CONST
						+ lightIndirectConst
					#endif
					#ifdef LIGHT_INDIRECT_ENV
						+ textureCube(lightIndirectSpecularEnvMap, worldViewReflected)
					#endif
				)
			#endif


			//
			// emission
			//

			#ifdef MATERIAL_EMISSIVE_MAP
				+ materialEmissiveMapColor
			#endif
			;

		#if defined(MATERIAL_DIFFUSE) && defined(MATERIAL_SPECULAR) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_SPECULAR_MAP)
			gl_FragColor *= 0.5;
		#endif
		#ifdef POSTPROCESS_NORMALS
			gl_FragColor.rgb = abs(worldNormalSmooth);
		#endif
		#ifdef POSTPROCESS_BRIGHTNESS
			gl_FragColor *= postprocessBrightness;
		#endif
		#ifdef POSTPROCESS_GAMMA
			gl_FragColor = pow(gl_FragColor,vec4(postprocessGamma,postprocessGamma,postprocessGamma,postprocessGamma));
		#endif
		#ifdef POSTPROCESS_BIGSCREEN
			gl_FragColor.rgb = max(gl_FragColor.rgb,vec3(0.33,0.33,0.33));
		#endif
		#ifdef FORCE_2D_POSITION
			gl_FragColor.a = 1.0;
		#endif
	#else
		// all lights are disabled, render black pixels
		gl_FragColor = vec4(0.0,0.0,0.0,1.0);
	#endif
}
