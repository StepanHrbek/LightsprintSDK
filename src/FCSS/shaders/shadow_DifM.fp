#ifdef SHADOW  
uniform sampler2DShadow shadowMap;
#else
uniform sampler2D shadowMap;
#endif

uniform sampler2D lightTex, diffuseTex;

varying vec4 projCoord;
varying vec2 diffuseCoord;

void main()
{
  vec4 lightValue = texture2DProj(lightTex, projCoord);
  vec4 diffuseValue = texture2D(diffuseTex, diffuseCoord);

#ifdef SHADOW  
  vec4 shadowValue = shadow2DProj(shadowMap, projCoord);
  gl_FragColor = shadowValue * gl_Color * lightValue * diffuseValue;
#else  
  vec4 shadowValue = texture2DProj(shadowMap, projCoord);
  if(shadowValue.z < projCoord.z/projCoord.w)
    gl_FragColor = vec4(0.0);
  else
    gl_FragColor = gl_Color * lightValue * diffuseValue;
#endif
}
