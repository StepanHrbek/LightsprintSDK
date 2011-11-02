// LightsprintGL: fragment ubershader
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint
//
// Options:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define SHADOW_COLOR
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
//  #define MATERIAL_TRANSPARENCY_TO_RGB
//  #define MATERIAL_NORMAL_MAP
//  #define ANIMATION_WAVE
//  #define POSTPROCESS_NORMALS
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define POSTPROCESS_BIGSCREEN
//  #define OBJECT_SPACE
//  #define CLIP_PLANE_[X|Y|Z][A|B]
//  #define FORCE_2D_POSITION
//
// For every great GLSL feature, there is buggy implementation.
// For cycles and some arrays did not work on ATI,
// remaining arrays and ## did not work in OSX etc.
// And I'm not yet talking about Intel.
// This shader evolves into greatness by using less of GLSL.

#define sqr(a) ((a)*(a))

#if SHADOW_SAMPLES>0
#if SHADOW_MAPS>0
	varying vec4 shadowCoord0;
	uniform sampler2DShadow shadowMap0;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap0;
	#endif
#endif
#if SHADOW_MAPS>1
	varying vec4 shadowCoord1;
	uniform sampler2DShadow shadowMap1;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap1;
	#endif
#endif
#if SHADOW_MAPS>2
	varying vec4 shadowCoord2;
	uniform sampler2DShadow shadowMap2;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap2;
	#endif
#endif
#if SHADOW_MAPS>3
	varying vec4 shadowCoord3;
	uniform sampler2DShadow shadowMap3;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap3;
	#endif
#endif
#if SHADOW_MAPS>4
	varying vec4 shadowCoord4;
	uniform sampler2DShadow shadowMap4;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap4;
	#endif
#endif
#if SHADOW_MAPS>5
	varying vec4 shadowCoord5;
	uniform sampler2DShadow shadowMap5;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap5;
	#endif
#endif
#if SHADOW_MAPS>6
	varying vec4 shadowCoord6;
	uniform sampler2DShadow shadowMap6;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap6;
	#endif
#endif
#if SHADOW_MAPS>7
	varying vec4 shadowCoord7;
	uniform sampler2DShadow shadowMap7;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap7;
	#endif
#endif
#if SHADOW_MAPS>8
	varying vec4 shadowCoord8;
	uniform sampler2DShadow shadowMap8;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap8;
	#endif
#endif
#if SHADOW_MAPS>9
	varying vec4 shadowCoord9;
	uniform sampler2DShadow shadowMap9;
	#ifdef SHADOW_COLOR
	uniform sampler2D shadowColorMap9;
	#endif
#endif
#endif

#if SHADOW_SAMPLES>1
	uniform vec4 shadowBlurWidth;
#endif

#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_ENV_SPECULAR))
	uniform vec3 worldEyePos;
#endif

#ifdef LIGHT_DIRECT
	uniform vec3 worldLightPos;
#endif

#if defined(LIGHT_DIRECTIONAL) || defined(LIGHT_DIRECT_ATT_SPOT)
	uniform vec3 worldLightDir;
#endif

#ifdef LIGHT_DIRECT_ATT_POLYNOMIAL
	uniform vec4 lightDistancePolynom;
#endif

#ifdef LIGHT_DIRECT_ATT_EXPONENTIAL
	uniform float lightDistanceRadius;
	uniform float lightDistanceFallOffExponent;
#endif

#ifdef LIGHT_DIRECT_ATT_SPOT
	uniform float lightDirectSpotOuterAngleRad;
	uniform float lightDirectSpotFallOffAngleRad;
	uniform float lightDirectSpotExponent;
#endif

#ifdef LIGHT_DIRECT_COLOR
	uniform vec4 lightDirectColor;
#endif

#ifdef LIGHT_DIRECT_MAP
	#if SHADOW_MAPS==0
		varying vec4 lightCoord;
	#endif
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

varying vec3 worldPos;

#if defined(MATERIAL_SPECULAR) && defined(LIGHT_DIRECT)
	uniform float materialSpecularShininess;
#endif

#ifdef MATERIAL_SPECULAR_CONST
	uniform vec4 materialSpecularConst;
#endif

varying vec3 worldNormalSmooth;

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

#ifdef CLIP_PLANE_XA
	uniform float clipPlaneXA;
#endif
#ifdef CLIP_PLANE_XB
	uniform float clipPlaneXB;
#endif
#ifdef CLIP_PLANE_YA
	uniform float clipPlaneYA;
#endif
#ifdef CLIP_PLANE_YB
	uniform float clipPlaneYB;
#endif
#ifdef CLIP_PLANE_ZA
	uniform float clipPlaneZA;
#endif
#ifdef CLIP_PLANE_ZB
	uniform float clipPlaneZB;
#endif

void main()
{

	/////////////////////////////////////////////////////////////////////
	//
	// clipping

	#ifdef CLIP_PLANE_XA
		if(worldPos.x>clipPlaneXA) discard;
	#endif
	#ifdef CLIP_PLANE_XB
		if(worldPos.x<clipPlaneXB) discard;
	#endif
	#ifdef CLIP_PLANE_YA
		if(worldPos.y>clipPlaneYA) discard;
	#endif
	#ifdef CLIP_PLANE_YB
		if(worldPos.y<clipPlaneYB) discard;
	#endif
	#ifdef CLIP_PLANE_ZA
		if(worldPos.z>clipPlaneZA) discard;
	#endif
	#ifdef CLIP_PLANE_ZB
		if(worldPos.z<clipPlaneZB) discard;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// material

	float opacityA = 1.0;
	vec3 transparencyRGB = vec3(0.0);
	#ifdef MATERIAL_DIFFUSE_CONST
		#if defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
			opacityA = materialDiffuseConst.a;
			transparencyRGB = vec3(1.0-materialDiffuseConst.a);
		#endif
	#endif
	#ifdef MATERIAL_TRANSPARENCY_CONST
		#ifdef MATERIAL_TRANSPARENCY_IN_ALPHA
			opacityA = materialTransparencyConst.a;
			transparencyRGB = vec3(1.0-materialTransparencyConst.a);
		#else
			opacityA = 1.0-(materialTransparencyConst.r+materialTransparencyConst.g+materialTransparencyConst.b)*0.33333;
			transparencyRGB = materialTransparencyConst.rgb;
		#endif
	#endif
	#ifdef MATERIAL_TRANSPARENCY_MAP
		vec4 materialTransparencyMapColor = texture2D(materialTransparencyMap, materialTransparencyCoord);
		#ifdef MATERIAL_TRANSPARENCY_IN_ALPHA
			opacityA = materialTransparencyMapColor.a;
			transparencyRGB = vec3(1.0-materialTransparencyMapColor.a);
		#else
			opacityA = 1.0-(materialTransparencyMapColor.r+materialTransparencyMapColor.g+materialTransparencyMapColor.b)*0.33333;
			transparencyRGB = materialTransparencyMapColor.rgb;
		#endif
	#endif
	#ifdef MATERIAL_DIFFUSE_MAP
		vec4 materialDiffuseMapColor = texture2D(materialDiffuseMap, materialDiffuseCoord);
		#if !defined(MATERIAL_TRANSPARENCY_CONST) && !defined(MATERIAL_TRANSPARENCY_MAP) && defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
			opacityA = materialDiffuseMapColor.a;
			transparencyRGB = vec3(1.0-materialDiffuseMapColor.a);
		#endif
	#endif
	#if (defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) || defined(MATERIAL_TRANSPARENCY_IN_ALPHA)) && !defined(MATERIAL_TRANSPARENCY_BLEND) && !defined(MATERIAL_TRANSPARENCY_TO_RGB)
		// Shader based alpha test with fixed threshold
		// We don't use GL_ALPHA_TEST because Radeons ignore it when rendering into shadowmap (all Radeons, last version tested: Catalyst 9-10)
		//  MATERIAL_TRANSPARENCY_BLEND = alpha blending, not alpha keying
		//  MATERIAL_TRANSPARENCY_TO_RGB = rendering blended material into rgb shadowmap or rgb blending, not alpha keying
		if (opacityA<0.5) discard;
	#endif
	#ifdef MATERIAL_SPECULAR_MAP
		float materialSpecularReflectance = step(materialDiffuseMapColor.r,0.6);
		float materialDiffuseReflectance = 1.0 - materialSpecularReflectance;
	#endif
	#if defined(MATERIAL_DIFFUSE) || defined(MATERIAL_SPECULAR) || defined(POSTPROCESS_NORMALS)
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

		#ifdef SHADOW_COLOR
			// colored shadow
			#define VISIBILITY_T vec4
			#if SHADOW_SAMPLES==1
				#define SHADOW_COLOR_LOOKUP(shadowColorMap,shadowCoord) * texture2DProj(shadowColorMap,shadowCoord.xyw)
			#else
				#define SHADOW_COLOR_LOOKUP(shadowColorMap,shadowCoord) * texture2D(shadowColorMap,center.xy)
			#endif
		#else
			// standard shadow
			#define VISIBILITY_T float
			#define SHADOW_COLOR_LOOKUP(shadowColorMap,shadowCoord)
		#endif

		VISIBILITY_T visibility = VISIBILITY_T(0.0); // 0=shadowed, 1=lit, 1,0,0=red shadow

		#if SHADOW_SAMPLES==1
			// hard shadows with 1 lookup
			#define SHADOWMAP_LOOKUP(shadowMap,shadowCoord) visibility += shadow2DProj(shadowMap, shadowCoord).z
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
				#define SHADOWMAP_LOOKUP(shadowMap,shadowCoord) center = shadowCoord.xyz/shadowCoord.w; visibility += ( shadow2D(shadowMap, center+shift1).z +shadow2D(shadowMap, center-shift1).z )
			#elif SHADOW_SAMPLES==4
				#define SHADOWMAP_LOOKUP(shadowMap,shadowCoord) center = shadowCoord.xyz/shadowCoord.w; visibility += ( shadow2D(shadowMap, center+shift1).z +shadow2D(shadowMap, center-shift1).z +shadow2D(shadowMap, center+shift2).z +shadow2D(shadowMap, center-shift2).z )
			#elif SHADOW_SAMPLES==8
				#define SHADOWMAP_LOOKUP(shadowMap,shadowCoord) center = shadowCoord.xyz/shadowCoord.w; visibility += ( shadow2D(shadowMap, center+shift1).z +shadow2D(shadowMap, center-shift1).z +shadow2D(shadowMap, center+0.3*shift1).z +shadow2D(shadowMap, center-0.6*shift1).z +shadow2D(shadowMap, center+shift2).z +shadow2D(shadowMap, center-0.9*shift2).z +shadow2D(shadowMap, center+0.8*shift2).z +shadow2D(shadowMap, center-0.7*shift2).z )
			#endif
		#endif // SHADOW_SAMPLES!=1

		#if defined(LIGHT_DIRECT_ATT_SPOT)
			// optimized path, step() not necessary
			// previosuly used also if defined(LIGHT_DIRECT_MAP), with projected texture set to clamp to black border,
			//  but it projected also backwards in areas where Z was never written to SM after clear (e.g. spotlight looking into the sky), at least on 4870 cat811-901
			#define SHADOW_CLAMP(shadowCoord)
		#else
			// standard path
			#define SHADOW_CLAMP(shadowCoord) * step(0.0,shadowCoord.z)
		#endif

		#define ACCUMULATE_SHADOW(shadowMap,shadowColorMap,shadowCoord) SHADOWMAP_LOOKUP(shadowMap,shadowCoord) SHADOW_CLAMP(shadowCoord) SHADOW_COLOR_LOOKUP(shadowColorMap,shadowCoord)

		vec3 center; // temporary, used by macros
		#if defined(SHADOW_CASCADE) && SHADOW_MAPS>1
			#if SHADOW_MAPS==2
				ACCUMULATE_SHADOW(shadowMap0,shadowColorMap0,shadowCoord0);
				VISIBILITY_T visibility0 = visibility;
				visibility = VISIBILITY_T(0);
				ACCUMULATE_SHADOW(shadowMap1,shadowColorMap1,shadowCoord1);
				center = abs(shadowCoord1.xyz/shadowCoord1.w-vec3(0.5));
				float centerMax = max(center.x,max(center.y,center.z));
				float blendFactor = smoothstep(0.3,0.49,centerMax);
				visibility = mix(visibility,visibility0,blendFactor);
			#else
				ACCUMULATE_SHADOW(shadowMap1,shadowColorMap1,shadowCoord1);
				VISIBILITY_T visibility1 = visibility;
				visibility = VISIBILITY_T(0);
				center = abs(shadowCoord2.xyz/shadowCoord2.w-vec3(0.5));
				float centerMax = max(center.x,max(center.y,center.z));
				if (centerMax>0.49)
				{
					ACCUMULATE_SHADOW(shadowMap0,shadowColorMap0,shadowCoord0);
					center = abs(shadowCoord1.xyz/shadowCoord1.w-vec3(0.5));
					float centerMax = max(center.x,max(center.y,center.z));
					float blendFactor = smoothstep(0.3,0.49,centerMax);
					visibility = mix(visibility1,visibility,blendFactor);
				}
				else
				{
					ACCUMULATE_SHADOW(shadowMap2,shadowColorMap2,shadowCoord2);
					float blendFactor = smoothstep(0.3,0.49,centerMax);
					visibility = mix(visibility,visibility1,blendFactor);
				}
			#endif
		#else
			#if SHADOW_MAPS>0
				ACCUMULATE_SHADOW(shadowMap0,shadowColorMap0,shadowCoord0);
			#endif
			#if SHADOW_MAPS>1
				ACCUMULATE_SHADOW(shadowMap1,shadowColorMap1,shadowCoord1);
			#endif
			#if SHADOW_MAPS>2
				ACCUMULATE_SHADOW(shadowMap2,shadowColorMap2,shadowCoord2);
			#endif
		#endif
		#if SHADOW_MAPS>3
			ACCUMULATE_SHADOW(shadowMap3,shadowColorMap3,shadowCoord3);
		#endif
		#if SHADOW_MAPS>4
			ACCUMULATE_SHADOW(shadowMap4,shadowColorMap4,shadowCoord4);
		#endif
		#if SHADOW_MAPS>5
			ACCUMULATE_SHADOW(shadowMap5,shadowColorMap5,shadowCoord5);
		#endif
		#if SHADOW_MAPS>6
			ACCUMULATE_SHADOW(shadowMap6,shadowColorMap6,shadowCoord6);
		#endif
		#if SHADOW_MAPS>7
			ACCUMULATE_SHADOW(shadowMap7,shadowColorMap7,shadowCoord7);
		#endif
		#if SHADOW_MAPS>8
			ACCUMULATE_SHADOW(shadowMap8,shadowColorMap8,shadowCoord8);
		#endif
		#if SHADOW_MAPS>9
			ACCUMULATE_SHADOW(shadowMap9,shadowColorMap9,shadowCoord9);
		#endif

		#ifdef SHADOW_PENUMBRA
			visibility /= float(SHADOW_SAMPLES*SHADOW_MAPS);
		#else
			visibility /= float(SHADOW_SAMPLES);
		#endif

		#ifdef SHADOW_ONLY
			visibility -= VISIBILITY_T(1.0);
		#endif

		#if SHADOW_MAPS==6 && !defined(SHADOW_PENUMBRA)
			// illuminates space from pointlight to near, otherwise shadowed by SHADOW_CLAMP()
			//visibility += step(0.0,-shadowCoord0.z)
			//	* step(0.0,-shadowCoord1.z)
			//	* step(0.0,-shadowCoord2.z)
			//	* step(0.0,-shadowCoord3.z)
			//	* step(0.0,-shadowCoord4.z)
			//	* step(0.0,-shadowCoord5.z);
		#endif

	#endif // SHADOW_SAMPLES*SHADOW_MAPS>0


	//////////////////////////////////////////////////////////////////////////////
	//
	// light direct

	#ifdef LIGHT_DIRECT
		#if defined(LIGHT_DIRECTIONAL)
			vec3 worldLightDirFromPixel = -worldLightDir;
		#else
			vec3 worldLightDirFromPixel = normalize(worldLightPos - worldPos);
			float distance = distance(worldPos,worldLightPos);
		#endif
		vec4 lightDirect =
			
			// scalar
			max(0.0,dot(worldLightDirFromPixel, worldNormal) * (gl_FrontFacing?1.0:-1.0))
			#ifndef LIGHT_DIRECTIONAL
				#ifdef LIGHT_DIRECT_ATT_PHYSICAL
					* pow(distance,-0.9)
				#endif
				#ifdef LIGHT_DIRECT_ATT_POLYNOMIAL
					/ max( lightDistancePolynom.x + distance*lightDistancePolynom.y + distance*distance*lightDistancePolynom.z, lightDistancePolynom.w )
				#endif
				#ifdef LIGHT_DIRECT_ATT_EXPONENTIAL
					* pow(max(0.0,1.0-sqr(distance/lightDistanceRadius)),lightDistanceFallOffExponent*0.45)
				#endif
			#endif
			#ifdef LIGHT_DIRECT_ATT_SPOT
				* pow(clamp( ((lightDirectSpotOuterAngleRad-acos(dot(worldLightDir,-worldLightDirFromPixel)))/lightDirectSpotFallOffAngleRad), 0.0, 1.0 ),lightDirectSpotExponent)
			#endif

			// scalar or vector
			#if SHADOW_SAMPLES*SHADOW_MAPS>0
				* visibility
			#endif

			// vector
			#ifdef LIGHT_DIRECT_COLOR
				* lightDirectColor
			#endif
			#ifdef LIGHT_DIRECT_MAP
				#if SHADOW_MAPS<1
					* texture2DProj(lightDirectMap, lightCoord)
				#elif SHADOW_MAPS<3
					* texture2DProj(lightDirectMap, shadowCoord0)
				#elif SHADOW_MAPS<5
					* texture2DProj(lightDirectMap, shadowCoord1)
				#else
					* texture2DProj(lightDirectMap, shadowCoord2)
				#endif
			#endif

			// make sure that result is vector
			#if !defined(LIGHT_DIRECT_COLOR) && !defined(LIGHT_DIRECT_MAP)
				* vec4(1.0,1.0,1.0,1.0)
			#endif
			;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// final mix

	#if defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_VCOLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_ENV_DIFFUSE) || defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(MATERIAL_EMISSIVE_CONST) || defined(MATERIAL_EMISSIVE_MAP) || defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) || defined(POSTPROCESS_NORMALS)

		#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_INDIRECT_ENV_SPECULAR) || defined(LIGHT_DIRECT))
			vec3 worldViewReflected = reflect(worldPos-worldEyePos,worldNormal);
		#endif

		#if defined(MATERIAL_SPECULAR) && defined(LIGHT_DIRECT)
			float NH = max(0.0,dot(worldNormal,(worldLightDirFromPixel+normalize(worldEyePos-worldPos))/2.0));
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
					opacityA *
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
				#ifdef MATERIAL_SPECULAR_CONST
					materialSpecularConst *
				#endif
				#ifdef MATERIAL_SPECULAR_MAP
					materialSpecularReflectance *
				#endif
				vec4((
					#ifdef LIGHT_DIRECT
						#if MATERIAL_SPECULAR_MODEL==0
							// Phong, materialSpecularShininess in 1..inf
							+ pow( max(0.0,dot(worldLightDirFromPixel,normalize(worldViewReflected))) ,materialSpecularShininess) * (materialSpecularShininess+1.0)
						#elif MATERIAL_SPECULAR_MODEL==1
							// Blinn-Phong, materialSpecularShininess in 1..inf
							+ pow(NH,materialSpecularShininess) * (materialSpecularShininess+1.0)
						#elif MATERIAL_SPECULAR_MODEL==2
							// Torrance-Sparrow (Gaussian), materialSpecularShininess=m^2=0..1
							+ exp((1.0-1.0/(NH*NH))/materialSpecularShininess) / (4.0*materialSpecularShininess*((NH*NH)*(NH*NH))+0.0000000000001)
						#else
							// Blinn-Torrance-Sparrow, materialSpecularShininess=c3^2=0..1
							+ sqr(materialSpecularShininess/(NH*NH*(materialSpecularShininess-1.0)+1.0))
						#endif
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

		#ifdef MATERIAL_TRANSPARENCY_TO_RGB
			gl_FragColor.rgb = transparencyRGB;
		#endif
		#ifdef FORCE_2D_POSITION
			gl_FragColor.a = 1.0;
		#else
			#if defined(MATERIAL_TRANSPARENCY_CONST) || defined(MATERIAL_TRANSPARENCY_MAP) || defined(MATERIAL_TRANSPARENCY_IN_ALPHA)
				gl_FragColor.a = opacityA;
			#endif
			#if defined(LIGHT_INDIRECT_VCOLOR) && !defined(MATERIAL_DIFFUSE_CONST) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_TRANSPARENCY_CONST) && !defined(MATERIAL_TRANSPARENCY_MAP)
				// only if not defined by material, opacity is taken from indirect vcolor
				// - so we can construct simple shader with color and opacity controlled by glColor4() (e.g. in glMenu)
				// - before doing this also for LIGHT_INDIRECT_MAP, UberProgramSetup::validate() would have to be changed, it disables lightmap if diffuse is not present
				gl_FragColor.a = lightIndirectLightmap.a;
			#endif
		#endif
	#else
		// all lights are disabled, render black pixels
		gl_FragColor = vec4(0.0,0.0,0.0,1.0);
	#endif
}
