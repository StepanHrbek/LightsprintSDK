uniform sampler2DShadow shadowMap[1];
uniform sampler2D lightTex;

varying vec4 projCoord;

void main()
{
  vec4 lightValue = texture2DProj(lightTex, projCoord);
  vec4 shadowValue = shadow2DProj(shadowMap[0], projCoord);
  gl_FragColor = shadowValue * gl_Color * lightValue;
}
