uniform sampler2D shadowMap[1], lightTex;

varying vec4 projCoord;

void main()
{
  vec4 shadowValue = texture2DProj(shadowMap[0], projCoord);
  vec4 lightValue = texture2DProj(lightTex, projCoord);
  
  if(shadowValue.z < projCoord.z/projCoord.w)
    gl_FragColor = vec4(0.0);
  else
    gl_FragColor = gl_Color * lightValue;
}
