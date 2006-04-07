uniform sampler2DShadow shadowMap[1];
uniform sampler2D lightTex, diffuseTex;

varying vec4 projCoord;
varying vec2 diffuseCoord;

void main()
{
  vec4 lightValue = texture2DProj(lightTex, projCoord);
  vec4 diffuseValue = texture2D(diffuseTex, diffuseCoord);
  vec4 shadowValue = shadow2DProj(shadowMap[0], projCoord);
  gl_FragColor = shadowValue * gl_Color * lightValue * diffuseValue;
}
