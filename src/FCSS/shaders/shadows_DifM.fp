#ifdef SHADOW  
uniform sampler2DShadow shadowMap[MAPS];
#else
uniform sampler2D shadowMap[MAPS];
#endif

uniform sampler2D lightTex, diffuseTex;

varying vec4 projCoord[MAPS];
varying vec2 diffuseCoord;

void main()
{
  vec4 lightValue = texture2DProj(lightTex, projCoord[MAPS/2]);
  vec4 diffuseValue = texture2D(diffuseTex, diffuseCoord);
  float shadowValue = 0.0;
  for(int i=0;i<MAPS;i++)
    shadowValue +=
#ifdef SHADOW
      shadow2DProj(shadowMap[i], projCoord[i]).z;
#else  
      (texture2DProj(shadowMap[i], projCoord[i]).z >= projCoord[i].z/projCoord[i].w) ? 1.0 : 0.0;
#endif
  gl_FragColor = shadowValue/float(MAPS) * gl_Color * lightValue * diffuseValue;
}
