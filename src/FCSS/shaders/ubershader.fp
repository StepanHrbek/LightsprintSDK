// Realtime Radiosity Viewer ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS 6
//  #define SHADOW_SAMPLES 4
//  #define LIGHT_DIRECT
//  #define LIGHT_DIRECT_MAP
//  #define LIGHT_INDIRECT_COLOR
//  #define LIGHT_INDIRECT_MAP
//  #define MATERIAL_DIFFUSE_COLOR
//  #define MATERIAL_DIFFUSE_MAP
//  #define FORCE_2D_POSITION

#if SHADOW_SAMPLES>0
uniform sampler2DShadow shadowMap[SHADOW_MAPS];
#endif

varying vec4 shadowCoord[SHADOW_MAPS];

#ifdef LIGHT_DIRECT_MAP
uniform sampler2D lightDirectMap;
#endif

#ifdef LIGHT_INDIRECT_COLOR
varying vec4 lightIndirectColor;
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

#if SHADOW_SAMPLES
  float shadowValue = 0.0;
#if SHADOW_SAMPLES==1
  // hard shadows with 1 lookup
  for(int i=0;i<SHADOW_MAPS;i++)
    shadowValue += shadow2DProj(shadowMap[i], shadowCoord[i]).z;
#else
  // blurred hard shadows (often called 'soft') with 2 or 4 lookups in rotating kernel
  float noise = 8.1*gl_FragCoord.x+5.7*gl_FragCoord.y;
  vec3 sc = vec3(sin(noise),cos(noise),0);
  vec3 shift1 = sc*0.003;
  vec3 shift2 = sc.yxz*vec3(0.006,-0.006,0);
  for(int i=0;i<SHADOW_MAPS;i++)
  {
    vec3 center = shadowCoord[i].xyz/shadowCoord[i].w;
    shadowValue +=
       shadow2D(shadowMap[i], center+shift1).z
      +shadow2D(shadowMap[i], center-shift1).z
#if SHADOW_SAMPLES==4
      +shadow2D(shadowMap[i], center+shift2).z
      +shadow2D(shadowMap[i], center-shift2).z
#endif
      ;
  }
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
      gl_Color
#ifdef LIGHT_DIRECT_MAP
      * texture2DProj(lightDirectMap, shadowCoord[SHADOW_MAPS/2])
#endif
#if SHADOW_SAMPLES*SHADOW_MAPS>0
      * shadowValue/float(SHADOW_SAMPLES*SHADOW_MAPS)
#endif
#endif
#ifdef LIGHT_INDIRECT_COLOR
      + lightIndirectColor
#endif
#ifdef LIGHT_INDIRECT_MAP
      + texture2D(lightIndirectMap, lightIndirectCoord)
#endif
    );
}
