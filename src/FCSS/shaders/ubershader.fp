// Realtime Radiosity Viewer ubershader
//
// options controlled by program:
//  #define SHADOW_MAPS 6
//  #define SHADOW_SAMPLES 4
//  #define INDIRECT_LIGHT
//  #define FORCE_2D_POSITION
//  #define DIFFUSE_MAP

#if SHADOW_SAMPLES>0
uniform sampler2DShadow shadowMap[SHADOW_MAPS];
#endif

uniform sampler2D lightTex;

varying vec4 projCoord[SHADOW_MAPS];

#ifdef INDIRECT_LIGHT
varying vec4 ambientIrradiance;
#endif

#ifdef DIFFUSE_MAP
uniform sampler2D diffuseTex;
varying vec2 diffuseCoord;
#endif

void main()
{
  vec4 lightValue = texture2DProj(lightTex, projCoord[SHADOW_MAPS/2]);

#if SHADOW_SAMPLES
  float shadowValue = 0.0;
#if SHADOW_SAMPLES==1
  // hard shadows with 1 lookup
  for(int i=0;i<SHADOW_MAPS;i++)
    shadowValue += shadow2DProj(shadowMap[i], projCoord[i]).z;
#else
  // blurred hard shadows (often called 'soft') with 2 or 4 lookups in rotating kernel
  float noise = 8.1*gl_FragCoord.x+5.7*gl_FragCoord.y;
  vec3 sc = vec3(sin(noise),cos(noise),0);
  vec3 shift1 = sc*0.003;
  vec3 shift2 = sc.yxz*vec3(0.006,-0.006,0);
  for(int i=0;i<SHADOW_MAPS;i++)
  {
    vec3 center = projCoord[i].xyz/projCoord[i].w;
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
#ifdef DIFFUSE_MAP
    texture2D(diffuseTex, diffuseCoord) * 
#endif
    ( lightValue * gl_Color
#if SHADOW_SAMPLES>0
      * shadowValue/float(SHADOW_SAMPLES*SHADOW_MAPS)
#endif
#ifdef INDIRECT_LIGHT
      + ambientIrradiance
#endif
    );
}
