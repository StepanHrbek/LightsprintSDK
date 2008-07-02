// LightsprintGL: fragment ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define SHADOW_BILINEAR
//  #define SHADOW_PENUMBRA
//  #define SHADOW_CASCADE
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
//  #define LIGHT_INDIRECT_ENV_DIFFUSE
//  #define LIGHT_INDIRECT_ENV_SPECULAR
//  #define MATERIAL_DIFFUSE
//  #define MATERIAL_DIFFUSE_X2
//  #define MATERIAL_DIFFUSE_CONST
//  #define MATERIAL_DIFFUSE_VCOLOR
//  #define MATERIAL_DIFFUSE_MAP
//  #define MATERIAL_SPECULAR
//  #define MATERIAL_SPECULAR_CONST
//  #define MATERIAL_SPECULAR_MAP
//  #define MATERIAL_EMISSIVE_CONST
//  #define MATERIAL_EMISSIVE_VCOLOR
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
//  #define CLIPPING
//  #define FORCE_2D_POSITION
//
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2008

//#if SHADOW_SAMPLES>0 // ATI failed on this line
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

#if SHADOW_SAMPLES>1
	uniform vec4 shadowBlurWidth;
#endif

#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_ENV_SPECULAR))
	uniform vec3 worldEyePos;
#endif

#ifdef LIGHT_DIRECT
	uniform vec3 worldLightPos;
	#ifndef MATERIAL_NORMAL_MAP
		varying float lightDirectVColor;
	#endif
#endif

#ifdef LIGHT_DIRECT_ATT_SPOT
	uniform vec3 worldLightDir;
	uniform float lightDirectSpotOuterAngleRad;
	uniform float lightDirectSpotFallOffAngleRad;
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
	//varying vec4 lightIndirectColor; // passed rather through gl_Color, ATI failed on custom varying
#endif

#ifdef LIGHT_INDIRECT_MAP
	uniform sampler2D lightIndirectMap;
	varying vec2 lightIndirectCoord;
#endif

#ifdef LIGHT_INDIRECT_MAP2
	uniform sampler2D lightIndirectMap2;
	uniform float lightIndirectBlend;
#endif

#ifdef LIGHT_INDIRECT_ENV_DIFFUSE
	uniform samplerCube lightIndirectDiffuseEnvMap;
#endif

#ifdef LIGHT_INDIRECT_ENV_SPECULAR
	uniform samplerCube lightIndirectSpecularEnvMap;
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

#if defined(MATERIAL_SPECULAR) || defined(LIGHT_DIRECT_ATT_SPOT)
	varying vec3 worldPos;
#endif

#ifdef MATERIAL_SPECULAR_CONST
	uniform vec4 materialSpecularConst;
#endif

#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(POSTPROCESS_NORMALS)
	varying vec3 worldNormalSmooth;
#endif

#ifdef MATERIAL_EMISSIVE_CONST
	uniform vec4 materialEmissiveConst;
#endif

#ifdef MATERIAL_EMISSIVE_VCOLOR
	varying vec4 materialEmissiveColor;
#endif

#ifdef MATERIAL_EMISSIVE_MAP
	uniform sampler2D materialEmissiveMap;
	varying vec2 materialEmissiveCoord;
#endif

#ifdef MATERIAL_TRANSPARENCY_CONST
	uniform vec4 materialTransparencyConst;
#endif

#ifdef MATERIAL_TRANSPARENCY_MAP
	uniform sampler2D materialTransparencyMap;
	varying vec2 materialTransparencyCoord;
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
			vec3 shift1 = sc*shadowBlurWidth.w;
			vec3 shift2 = sc.yxz*shadowBlurWidth.xyz;
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

		#if defined(LIGHT_DIRECT_MAP) || defined(LIGHT_DIRECT_ATT_SPOT)
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index)
		#else
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index) * step(0.0,shadowCoord[index].z)
		#endif

		vec3 center;
		#if defined(SHADOW_CASCADE) && SHADOW_MAPS>1
			center = shadowCoord[1].xyz/shadowCoord[1].w;
			if(center.x>0.96 || center.x<0.04 || center.y>0.96 || center.y<0.04)
				visibility = shadow2DProj(shadowMap0,shadowCoord[0]).z*float(SHADOW_SAMPLES);
			else
				{SHADOWMAP_LOOKUP(shadowMap1,1);}
		#else
			#if SHADOW_MAPS>0
				SHADOWMAP_LOOKUP(shadowMap0,0);
			#endif
			#if SHADOW_MAPS>1
				SHADOWMAP_LOOKUP(shadowMap1,1);
			#endif
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


	float opacity = 1.0;
	#ifdef MATERIAL_TRANSPARENCY_CONST
		#ifdef MATERIAL_TRANSPARENCY_IN_ALPHA
			opacity = materialTransparencyConst.a;
		#else
			opacity = 1.0-(materialTransparencyConst.r+materialTransparencyConst.g+materialTransparencyConst.b)*0.33333;
		#endif
	#endif
	#ifdef MATERIAL_TRANSPARENCY_MAP
		vec4 materialTransparencyMapColor = texture2D(materialTransparencyMap, materialTransparencyCoord);
		#ifdef MATERIAL_TRANSPARENCY_IN_ALPHA
			opacity = materialTransparencyMapColor.a;
		#else
			opacity = 1.0-(materialTransparencyMapColor.r+materialTransparencyMapColor.g+materialTransparencyMapColor.b)*0.33333;
		#endif
	#endif
	#ifdef MATERIAL_DIFFUSE_MAP
		vec4 materialDiffuseMapColor = texture2D(materialDiffuseMap, materialDiffuseCoord);
		#if !defined(MATERIAL_TRANSPARENCY_CONST) && !defined(MATERIAL_TRANSPARENCY_MAP) && defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
			opacity = materialDiffuseMapColor.a;
		#endif

		#ifdef MATERIAL_TRANSPARENCY_BLEND
			// outside shader, blend operation is: color_in_buffer = RGB + (1-A)*color_in_buffer
			// so if we want RGB modulated by opacity, it must be done here in shader
			// Q: do we want RGB modulated by opacity?
			// A: only for DIFFUSE_MAP (DIFFUSE_CONST, specular and emission are not affected by transparency)
			//    and TRANSPARENCY_BLEND (without blend, GPU will force opacity 0 or 1, so here's no reason to darken our material)
			materialDiffuseMapColor.rgb *= opacity;
		#endif
	#endif
	#ifdef MATERIAL_SPECULAR_MAP
		float materialSpecularReflectance = step(materialDiffuseMapColor.r,0.6);
		float materialDiffuseReflectance = 1.0 - materialSpecularReflectance;
	#endif
	#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR)
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
		#if defined(MATERIAL_NORMAL_MAP) || defined(MATERIAL_SPECULAR) || defined(LIGHT_DIRECT_ATT_SPOT)
			vec3 worldLightDirToPixel = normalize(worldLightPos - worldPos);
		#endif
		vec4 lightDirect =
			#ifdef MATERIAL_NORMAL_MAP
				max(0.0,dot(worldLightDirToPixel, worldNormal)) // per pixel
			#else
				vec4(lightDirectVColor,lightDirectVColor,lightDirectVColor,lightDirectVColor) // per vertex
			#endif
			#ifdef LIGHT_DIRECT_COLOR
				* lightDirectColor
			#endif
			#ifdef LIGHT_DIRECT_MAP
				* texture2DProj(lightDirectMap, shadowCoord[SHADOW_MAPS/2])
			#endif
			#ifdef LIGHT_DIRECT_ATT_SPOT
				* clamp( ((lightDirectSpotOuterAngleRad-acos(dot(worldLightDir,-worldLightDirToPixel)))/lightDirectSpotFallOffAngleRad), 0.0, 1.0 )
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

	#if defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(MATERIAL_EMISSIVE_CONST) || defined(MATERIAL_EMISSIVE_VCOLOR) || defined(MATERIAL_EMISSIVE_MAP) || defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP)

		#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(LIGHT_DIRECT))
			vec3 worldViewReflected = reflect(worldPos-worldEyePos,worldNormal);
		#endif

		#if defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2)
			vec4 lightIndirectLightmap = 
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
					;
		#endif

		gl_FragColor =

			//
			// diffuse reflection
			//

			#if defined(MATERIAL_DIFFUSE) && (defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2) || defined(LIGHT_INDIRECT_ENV_DIFFUSE))
				#ifdef MATERIAL_DIFFUSE_X2
					vec4(2.0,2.0,2.0,1.0) *
				#endif
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
				vec4(( 
					#ifdef LIGHT_DIRECT
						lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_CONST
						+ lightIndirectConst
					#endif
					#if defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2)
						+ lightIndirectLightmap
					#endif
					#ifdef LIGHT_INDIRECT_ENV_DIFFUSE
						+ textureCube(lightIndirectDiffuseEnvMap, worldNormal)
					#endif
				).rgb,1.0)
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
				vec4((
					#ifdef LIGHT_DIRECT
						+ pow(max(0.0,dot(worldLightDirToPixel,normalize(worldViewReflected))),10.0)*2.0
						* lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_CONST
						+ lightIndirectConst
					#endif
					#ifdef LIGHT_INDIRECT_ENV_SPECULAR
						+ textureCube(lightIndirectSpecularEnvMap, worldViewReflected)
						#if defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2)
							// we use fixed cubemap for indirect specular reflections on static surfaces (not very realistic)
							// modulating it by indirect irradiance makes it look bit better
							* lightIndirectLightmap
						#endif
					#endif
				).rgb,1.0)
			#endif


			//
			// emission
			//

			#ifdef MATERIAL_EMISSIVE_CONST
				+ materialEmissiveConst
			#endif
			#ifdef MATERIAL_EMISSIVE_VCOLOR
				+ materialEmissiveColor
			#endif
			#ifdef MATERIAL_EMISSIVE_MAP
				+ materialEmissiveMapColor
			#endif

			+ vec4(0.0,0.0,0.0,0.0) // prevent broken shader when material properties are disabled
			;

		#if defined(MATERIAL_DIFFUSE) && defined(MATERIAL_SPECULAR) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_SPECULAR_MAP)
			gl_FragColor.rgb *= 0.5;
		#endif
		#ifdef POSTPROCESS_NORMALS
			gl_FragColor.rgb = abs(worldNormalSmooth);
		#endif
		#ifdef POSTPROCESS_BRIGHTNESS
			gl_FragColor.rgb *= postprocessBrightness.rgb;
		#endif
		#ifdef POSTPROCESS_GAMMA
			gl_FragColor.rgb = pow(gl_FragColor.rgb,vec3(postprocessGamma,postprocessGamma,postprocessGamma));
		#endif
		#ifdef POSTPROCESS_BIGSCREEN
			gl_FragColor.rgb = max(gl_FragColor.rgb,vec3(0.33,0.33,0.33));
		#endif

		#ifdef FORCE_2D_POSITION
			gl_FragColor.a = 1.0;
		#else
		#if defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) || defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
			gl_FragColor.a = opacity;
		#endif
		#endif
	#else
		// all lights are disabled, render black pixels
		gl_FragColor = vec4(0.0,0.0,0.0,1.0);
	#endif
}
