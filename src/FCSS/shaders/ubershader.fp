// Realtime Radiosity Viewer ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS [0..n]
//  #define SHADOW_SAMPLES [0|1|2|4]
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_INDIRECT_COLOR
//  #define LIGHT_INDIRECT_MAP
//  #define MATERIAL_DIFFUSE_COLOR
//  #define MATERIAL_DIFFUSE_MAP
//  #define FORCE_2D_POSITION

// for array of samplers (needs OpenGL 2.0 compliant card)
//#if SHADOW_MAPS*SHADOW_SAMPLES>0
//uniform sampler2DShadow shadowMap[SHADOW_MAPS];
//#endif

// for individual samplers (works on buggy ATI)
//#if SHADOW_SAMPLES>0 // this if doesn't work on buggy ATI
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
varying vec4 lightDirectColor;
#endif

#ifdef LIGHT_DIRECT_MAP
uniform sampler2D lightDirectMap;
#endif

#ifdef LIGHT_INDIRECT_COLOR
varying vec4 lightIndirectColor; // passed rather through gl_Color, anything other fails on buggy ATI Catalyst 6.6
#endif

#ifdef LIGHT_INDIRECT_MAP
uniform sampler2D lightIndirectMap;
varying vec2 lightIndirectCoord;
#endif

#ifdef MATERIAL_DIFFUSE_COLOR
varying vec4 materialDiffuseColor;
#endif

#ifdef MATERIAL_DIFFUSE_MAP
uniform sampler2D materialDiffuseMap;
varying vec2 materialDiffuseCoord;
#endif

void main()
{

#if SHADOW_SAMPLES*SHADOW_MAPS>0
  float shadowValue = 0.0;
#if SHADOW_SAMPLES==1
  // hard shadows with 1 lookup

// for array of samplers (needs OpenGL 2.0 compliant card)
//  for(int i=0;i<SHADOW_MAPS;i++)
//    shadowValue += shadow2DProj(shadowMap[i], shadowCoord[i]).z;

  // for individual samplers (works on buggy ATI)
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
  
#else
  // blurred hard shadows (often called 'soft') with 2 or 4 lookups in rotating kernel
  float noise = 8.1*gl_FragCoord.x+5.7*gl_FragCoord.y;
  vec3 sc = vec3(sin(noise),cos(noise),0);
  vec3 shift1 = sc*0.003;
  vec3 shift2 = sc.yxz*vec3(0.006,-0.006,0);

// for array of samplers (needs OpenGL 2.0 compliant card)
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

// for individual samplers (works on buggy ATI)
  vec3 center;
#if SHADOW_MAPS>0
  center = shadowCoord[0].xyz/shadowCoord[0].w;
  shadowValue +=
       shadow2D(shadowMap0, center+shift1).z
      +shadow2D(shadowMap0, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap0, center+shift2).z
      +shadow2D(shadowMap0, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>1
  center = shadowCoord[1].xyz/shadowCoord[1].w;
  shadowValue +=
       shadow2D(shadowMap1, center+shift1).z
      +shadow2D(shadowMap1, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap1, center+shift2).z
      +shadow2D(shadowMap1, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>2
  center = shadowCoord[2].xyz/shadowCoord[2].w;
  shadowValue +=
       shadow2D(shadowMap2, center+shift1).z
      +shadow2D(shadowMap2, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap2, center+shift2).z
      +shadow2D(shadowMap2, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>3
  center = shadowCoord[3].xyz/shadowCoord[3].w;
  shadowValue +=
       shadow2D(shadowMap3, center+shift1).z
      +shadow2D(shadowMap3, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap3, center+shift2).z
      +shadow2D(shadowMap3, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>4
  center = shadowCoord[4].xyz/shadowCoord[4].w;
  shadowValue +=
       shadow2D(shadowMap4, center+shift1).z
      +shadow2D(shadowMap4, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap4, center+shift2).z
      +shadow2D(shadowMap4, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>5
  center = shadowCoord[5].xyz/shadowCoord[5].w;
  shadowValue +=
       shadow2D(shadowMap5, center+shift1).z
      +shadow2D(shadowMap5, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap5, center+shift2).z
      +shadow2D(shadowMap5, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>6
  center = shadowCoord[6].xyz/shadowCoord[6].w;
  shadowValue +=
       shadow2D(shadowMap6, center+shift1).z
      +shadow2D(shadowMap6, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap6, center+shift2).z
      +shadow2D(shadowMap6, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>7
  center = shadowCoord[7].xyz/shadowCoord[7].w;
  shadowValue +=
       shadow2D(shadowMap7, center+shift1).z
      +shadow2D(shadowMap7, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap7, center+shift2).z
      +shadow2D(shadowMap7, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>8
  center = shadowCoord[8].xyz/shadowCoord[8].w;
  shadowValue +=
       shadow2D(shadowMap8, center+shift1).z
      +shadow2D(shadowMap8, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap8, center+shift2).z
      +shadow2D(shadowMap8, center-shift2).z
#endif
      ;
#endif
#if SHADOW_MAPS>9
  center = shadowCoord[9].xyz/shadowCoord[9].w;
  shadowValue +=
       shadow2D(shadowMap9, center+shift1).z
      +shadow2D(shadowMap9, center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap9, center+shift2).z
      +shadow2D(shadowMap9, center-shift2).z
#endif
      ;
#endif

#endif
#endif

  gl_FragColor =
#ifdef MATERIAL_DIFFUSE_MAP
    texture2D(materialDiffuseMap, materialDiffuseCoord) * 
#endif
#ifdef MATERIAL_DIFFUSE_COLOR
    materialDiffuseColor * 
#endif
    ( 
#ifdef LIGHT_DIRECT
      lightDirectColor
#ifdef LIGHT_DIRECT_MAP
      * texture2DProj(lightDirectMap, shadowCoord[SHADOW_MAPS/2])
#endif
#if SHADOW_SAMPLES*SHADOW_MAPS>0
      * shadowValue/float(SHADOW_SAMPLES*SHADOW_MAPS)
#endif
#endif
#ifdef LIGHT_INDIRECT_COLOR
      + gl_Color
#endif
#ifdef LIGHT_INDIRECT_MAP
      + texture2D(lightIndirectMap, lightIndirectCoord)
#endif
    );
}
