// DemoEngine fragment ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS [0..10]
//  #define SHADOW_SAMPLES [0|1|2|4|8]
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_INDIRECT_CONST
//  #define LIGHT_INDIRECT_COLOR
//  #define LIGHT_INDIRECT_MAP
//  #define LIGHT_INDIRECT_ENV
//  #define MATERIAL_DIFFUSE
//  #define MATERIAL_DIFFUSE_COLOR
//  #define MATERIAL_DIFFUSE_MAP
//  #define MATERIAL_SPECULAR
//  #define MATERIAL_SPECULAR_MAP
//  #define MATERIAL_NORMAL_MAP
//  #define OBJECT_SPACE
//  #define FORCE_2D_POSITION
//
// Workarounds for driver bugs of one vendor made it a mess, sorry.
//
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

// for array of samplers (for any OpenGL 2.0 compliant card)
//#if SHADOW_MAPS*SHADOW_SAMPLES>0
//uniform sampler2DShadow shadowMap[SHADOW_MAPS];
//#endif

// for individual samplers (ATI fails on array)
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
		varying float lightDirectColor;
	#endif
#endif

#ifdef LIGHT_DIRECT_MAP
	uniform sampler2D lightDirectMap;
#endif

#ifdef LIGHT_INDIRECT_CONST
	uniform vec4 lightIndirectConst;
#endif

#ifdef LIGHT_INDIRECT_COLOR
	//varying vec4 lightIndirectColor; // passed rather through gl_Color, ATI fails on anything else
#endif

#ifdef LIGHT_INDIRECT_MAP
	uniform sampler2D lightIndirectMap;
	varying vec2 lightIndirectCoord;
#endif

#ifdef LIGHT_INDIRECT_ENV
	uniform samplerCube lightIndirectSpecularEnvMap;
	uniform samplerCube lightIndirectDiffuseEnvMap;
#endif

#ifdef MATERIAL_DIFFUSE_COLOR
	varying vec4 materialDiffuseColor;
#endif

#ifdef MATERIAL_DIFFUSE_MAP
	uniform sampler2D materialDiffuseMap;
	varying vec2 materialDiffuseCoord;
#endif

#ifdef MATERIAL_SPECULAR
	varying vec3 worldPos;
#endif
#if defined(MATERIAL_SPECULAR) || defined(LIGHT_INDIRECT_ENV)
	varying vec3 worldNormalSmooth;
#endif

void main()
{

	/////////////////////////////////////////////////////////////////////
	//
	// shadowing

	#if SHADOW_SAMPLES*SHADOW_MAPS>0

		float shadowValue = 0.0;

		#if SHADOW_SAMPLES==1
			// hard shadows with 1 lookup

			// for array of samplers (for any OpenGL 2.0 compliant card)
			//  for(int i=0;i<SHADOW_MAPS;i++)
			//    shadowValue += shadow2DProj(shadowMap[i], shadowCoord[i]).z;

			// for individual samplers (ATI fails on for cycle)
			#if SHADOW_MAPS>0
				shadowValue += shadow2DProj(shadowMap0, shadowCoord[0]).z;
			#endif
			#if SHADOW_MAPS>1
				shadowValue += shadow2DProj(shadowMap1, shadowCoord[1]).z;
			#endif
			#if SHADOW_MAPS>2
				shadowValue += shadow2DProj(shadowMap2, shadowCoord[2]).z;
			#endif
			#if SHADOW_MAPS>3
				shadowValue += shadow2DProj(shadowMap3, shadowCoord[3]).z;
			#endif
			#if SHADOW_MAPS>4
				shadowValue += shadow2DProj(shadowMap4, shadowCoord[4]).z;
			#endif
			#if SHADOW_MAPS>5
				shadowValue += shadow2DProj(shadowMap5, shadowCoord[5]).z;
			#endif
			#if SHADOW_MAPS>6
				shadowValue += shadow2DProj(shadowMap6, shadowCoord[6]).z;
			#endif
			#if SHADOW_MAPS>7
				shadowValue += shadow2DProj(shadowMap7, shadowCoord[7]).z;
			#endif
			#if SHADOW_MAPS>8
				shadowValue += shadow2DProj(shadowMap8, shadowCoord[8]).z;
			#endif
			#if SHADOW_MAPS>9
				shadowValue += shadow2DProj(shadowMap9, shadowCoord[9]).z;
			#endif
  
		#else // SHADOW_SAMPLES!=1
			// blurred hard shadows (often called 'soft') with 2 or 4 lookups in rotating kernel
			float noise = 8.1*gl_FragCoord.x+5.7*gl_FragCoord.y;
			vec3 sc = vec3(sin(noise),cos(noise),0.0);
			vec3 shift1 = sc*0.003;
			vec3 shift2 = sc.yxz*vec3(0.006,-0.006,0.0);

			// for array of samplers (for any OpenGL 2.0 compliant card)
			//  for(int i=0;i<SHADOW_MAPS;i++)
			//  {
			//    vec3 center = shadowCoord[i].xyz/shadowCoord[i].w;
			//    shadowValue +=
			//       shadow2D(shadowMap[i], center+shift1).z
			//      +shadow2D(shadowMap[i], center-shift1).z
			//#if SHADOW_SAMPLES==4
			//      +shadow2D(shadowMap[i], center+shift2).z
			//      +shadow2D(shadowMap[i], center-shift2).z
			//#endif
			//      ;
			//  }

			// for individual samplers (ATI fails on array)
			#if SHADOW_SAMPLES==2
				#define SHADOWMAP_LOOKUP_CENTER(shadowMap,center) \
				shadowValue += \
					shadow2D(shadowMap, center+shift1).z \
					+shadow2D(shadowMap, center-shift1).z \
					;
			#elif SHADOW_SAMPLES==4
				#define SHADOWMAP_LOOKUP_CENTER(shadowMap,center) \
					shadowValue += \
						shadow2D(shadowMap, center+shift1).z \
						+shadow2D(shadowMap, center-shift1).z \
						+shadow2D(shadowMap, center+shift2).z \
						+shadow2D(shadowMap, center-shift2).z \
						;
			#elif SHADOW_SAMPLES==8
				#define SHADOWMAP_LOOKUP_CENTER(shadowMap,center) \
				shadowValue += \
					shadow2D(shadowMap, center+shift1).z \
					+shadow2D(shadowMap, center-shift1).z \
					+shadow2D(shadowMap, center+1.8*shift1).z \
					+shadow2D(shadowMap, center-1.8*shift1).z \
					+shadow2D(shadowMap, center+shift2).z \
					+shadow2D(shadowMap, center-shift2).z \
					+shadow2D(shadowMap, center+0.6*shift2).z \
					+shadow2D(shadowMap, center-0.6*shift2).z \
					;
			#endif
			#define SHADOWMAP_LOOKUP(shadowMap,index) \
				center = shadowCoord[index].xyz/shadowCoord[index].w; \
				SHADOWMAP_LOOKUP_CENTER(shadowMap,center)

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

		#endif // SHADOW_SAMPLES!=1

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
				lightDirectColor // per vertex
			#endif
			#ifdef LIGHT_DIRECT_MAP
				* texture2DProj(lightDirectMap, shadowCoord[0])
			#endif
			#if SHADOW_SAMPLES*SHADOW_MAPS>0
				* shadowValue/float(SHADOW_SAMPLES*SHADOW_MAPS)
			#endif
			;
	#endif


	//////////////////////////////////////////////////////////////////////////////
	//
	// final mix

	#if defined(MATERIAL_SPECULAR) && (defined(LIGHT_INDIRECT_ENV) || defined(LIGHT_DIRECT))
		vec3 worldViewReflected = reflect(worldPos-worldEyePos,worldNormal);
	#endif

	#if defined(LIGHT_DIRECT) || defined(LIGHT_INDIRECT_CONST) || defined(LIGHT_INDIRECT_COLOR) || defined(LIGHT_INDIRECT_MAP) || defined(LIGHT_INDIRECT_ENV)
		gl_FragColor =

			//
			// diffuse reflection
			//

			#ifdef MATERIAL_DIFFUSE
				#ifdef MATERIAL_DIFFUSE_COLOR
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
					#ifdef LIGHT_INDIRECT_COLOR
						+ gl_Color
					#endif
					#ifdef LIGHT_INDIRECT_MAP
						+ texture2D(lightIndirectMap, lightIndirectCoord)
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
			;

		#if defined(MATERIAL_DIFFUSE) && defined(MATERIAL_SPECULAR) && !defined(MATERIAL_DIFFUSE_MAP) && !defined(MATERIAL_SPECULAR_MAP)
			gl_FragColor *= 0.5;
		#endif
	#endif
}
