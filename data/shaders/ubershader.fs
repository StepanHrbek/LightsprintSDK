// LightsprintGL: fragment ubershader
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

#if SHADOW_SAMPLES>0
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
#endif

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

#if defined(LIGHT_DIRECTIONAL) || defined(LIGHT_DIRECT_ATT_SPOT)
	uniform vec3 worldLightDir;
#endif

#ifdef LIGHT_DIRECT_ATT_SPOT
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
	varying vec4 lightIndirectColor;
#endif

#if defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_DETAIL_MAP)
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

#ifdef MATERIAL_DIFFUSE_MAP
	uniform sampler2D materialDiffuseMap;
	varying vec2 materialDiffuseCoord;
#endif

#if defined(MATERIAL_SPECULAR) || defined(LIGHT_DIRECT_ATT_SPOT) || defined(CLIP_PLANE)
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

#ifdef CLIP_PLANE
	uniform float clipPlaneY;
#endif

void main()
{

	/////////////////////////////////////////////////////////////////////
	//
	// clipping

	#ifdef CLIP_PLANE
		if(worldPos.y<clipPlaneY) discard;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// material

	float opacity = 1.0;
	#ifdef MATERIAL_DIFFUSE_CONST
		#if defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
			opacity = materialDiffuseConst.a;
		#endif
	#endif
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
	#endif
	#if (defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) || defined(MATERIAL_TRANSPARENCY_IN_ALPHA)) && !defined(MATERIAL_TRANSPARENCY_BLEND)
		// Shader based alpha test with fixed treshold
		// We don't use GL_ALPHA_TEST because Radeons ignore it when rendering into shadowmap (all Radeons, last version tested: Catalyst 9-10)
		if (opacity<0.5) discard;
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
					+shadow2D(shadowMap, center+0.3*shift1).z \
					+shadow2D(shadowMap, center-0.6*shift1).z \
					+shadow2D(shadowMap, center+shift2).z \
					+shadow2D(shadowMap, center-0.9*shift2).z \
					+shadow2D(shadowMap, center+0.8*shift2).z \
					+shadow2D(shadowMap, center-0.7*shift2).z )
			#endif
		#endif // SHADOW_SAMPLES!=1

		#if defined(LIGHT_DIRECT_ATT_SPOT)
			// optimized path, step() not necessary
			// previosuly used also if defined(LIGHT_DIRECT_MAP), with projected texture set to clamp to black border,
			//  but it projected also backwards in areas where Z was never written to SM after clear (e.g. spotlight looking into the sky), at least on 4870 cat811-901
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index)
		#else
			// standard path
			#define SHADOWMAP_LOOKUP(shadowMap,index) SHADOWMAP_LOOKUP_SUB(shadowMap,index) * step(0.0,shadowCoord[index].z)
		#endif

		vec3 center;
		#if defined(SHADOW_CASCADE) && SHADOW_MAPS>1
			// Catalyst drivers recently introduced new bug (observed on 4870, cat811-901)
			// if neighboring pixels use different path in following "if", some of them incorrectly get visibility=0
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

		#ifdef SHADOW_PENUMBRA
			visibility /= float(SHADOW_SAMPLES*SHADOW_MAPS);
		#else
			visibility /= float(SHADOW_SAMPLES);
		#endif

		#ifdef SHADOW_ONLY
			visibility -= 1.0;
		#endif

	#endif // SHADOW_SAMPLES*SHADOW_MAPS>0


	//////////////////////////////////////////////////////////////////////////////
	//
	// light direct

	#ifdef LIGHT_DIRECT
		#if defined(MATERIAL_NORMAL_MAP) || defined(MATERIAL_SPECULAR) || defined(LIGHT_DIRECT_ATT_SPOT)
			#if defined(LIGHT_DIRECTIONAL)
				vec3 worldLightDirFromPixel = -worldLightDir;
			#else
				vec3 worldLightDirFromPixel = normalize(worldLightPos - worldPos);
			#endif
		#endif
		vec4 lightDirect =
			#ifdef MATERIAL_NORMAL_MAP
				max(0.0,dot(worldLightDirFromPixel, worldNormal)) // per pixel
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
				* clamp( ((lightDirectSpotOuterAngleRad-acos(dot(worldLightDir,-worldLightDirFromPixel)))/lightDirectSpotFallOffAngleRad), 0.0, 1.0 )
			#endif
			#if SHADOW_SAMPLES*SHADOW_MAPS>0
				* visibility
			#endif
			;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// final mix

	#if defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(MATERIAL_EMISSIVE_CONST) || defined(MATERIAL_EMISSIVE_MAP) || defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP)

		#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(LIGHT_DIRECT))
			vec3 worldViewReflected = reflect(worldPos-worldEyePos,worldNormal);
		#endif

		#if defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2)
			vec4 lightIndirectLightmap = 
					#ifdef LIGHT_INDIRECT_VCOLOR
						+ lightIndirectColor
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
				#ifdef MATERIAL_DIFFUSE_MAP
					materialDiffuseMapColor *
				#endif
				#ifdef MATERIAL_SPECULAR_MAP
					materialDiffuseReflectance *
				#endif
				#if defined(MATERIAL_TRANSPARENCY_BLEND) && ( !defined(MATERIAL_DIFFUSE_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) )
					// outside shader, blend operation is: color_in_buffer = RGB + (1-A)*color_in_buffer
					// so if we want RGB modulated by opacity, it must be done here in shader
					// Q: do we want RGB modulated by opacity?
					// A: yes, but only for 
					//        TRANSPARENCY_BLEND (without blend, GPU will force opacity 0 or 1, so here's no reason to darken our material)
					//        &&
					//          pure DIFFUSE or DIFFUSE_MAP
					//          ||
					//          per-pixel transparency
					//    why?
					//        incoming pure DIFFUSE and DIFFUSE_MAP expects we will multiply them by opacity (diffuse maps have rgb rarely premultiplied by alpha)
					//        incoming DIFFUSE_CONST were already premultipled by average opacity, but multiply them again if(transparency_map)
					//             TODO: if(MATERIAL_TRANSPARENCY_MAP), renderer should divide diffuse_const by average opacity before sending them to GPU (it is very atypical setup, never encountered yet)
					//        incoming specular and emission should be separated from transparency, do nothing (buggy look with real data not encountered yet, but it may come)
					opacity *
				#endif
				vec4(( 
					#ifdef LIGHT_DIRECT
						lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_DETAIL_MAP
						+ (
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
					#ifdef LIGHT_INDIRECT_DETAIL_MAP
						) * texture2D(lightIndirectMap, lightIndirectCoord) * 2.0
					#endif
				).rgb,1.0)
			#endif


			//
			// specular reflection
			//

			#ifdef MATERIAL_SPECULAR
				+
				#ifdef MATERIAL_DIFFUSE_MAP
					// special behaviour, not enabled by default
					//materialDiffuseMapColor.a *
				#endif
				#ifdef MATERIAL_EMISSIVE_MAP
					// special behaviour, not enabled by default
					//materialEmissiveMapColor.a *
				#endif
				#ifdef MATERIAL_SPECULAR_CONST
					materialSpecularConst *
				#endif
				#ifdef MATERIAL_SPECULAR_MAP
					materialSpecularReflectance *
				#endif
				vec4((
					#ifdef LIGHT_DIRECT
						+ pow(max(0.0,dot(worldLightDirFromPixel,normalize(worldViewReflected))),10.0)*2.0
						* lightDirect
					#endif
					#ifdef LIGHT_INDIRECT_CONST
						+ lightIndirectConst
					#endif
					#ifdef LIGHT_INDIRECT_ENV_SPECULAR
						+ textureCube(lightIndirectSpecularEnvMap, worldViewReflected)
						#if defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_MAP2)
							// reflection maps for big complex objects like whole building tend to be very inaccurate,
							// modulating reflection by indirect irradiance makes them look better
							// however, it negatively affects simple objects where reflection map is accurate
							//* lightIndirectLightmap
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
			#ifdef MATERIAL_EMISSIVE_MAP
				+ materialEmissiveMapColor
			#endif

			+ vec4(0.0,0.0,0.0,0.0) // prevent broken shader when material properties are disabled
			;

		#if defined(MATERIAL_DIFFUSE) && defined(MATERIAL_SPECULAR) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_SPECULAR_MAP)
			// special behaviour, not enabled by default
			//gl_FragColor.rgb *= 0.5;
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
			#if (defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP)) && !defined(MATERIAL_DIFFUSE_CONST) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_TRANSPARENCY_CONST) && !defined(MATERIAL_TRANSPARENCY_MAP)
				// only if not defined by material, opacity is taken from lightmap/vertex colors
				gl_FragColor.a = lightIndirectLightmap.a;
			#endif
		#endif
	#else
		// all lights are disabled, render black pixels
		gl_FragColor = vec4(0.0,0.0,0.0,1.0);
	#endif
}
