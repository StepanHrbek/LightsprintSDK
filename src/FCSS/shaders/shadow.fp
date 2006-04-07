uniform sampler2DShadow shadowMap[1];

varying vec4 projCoord;

void main()
{
  gl_FragColor = shadow2DProj(shadowMap[0], projCoord);
}
