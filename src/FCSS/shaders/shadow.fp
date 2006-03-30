uniform sampler2D shadowMap;

varying vec4 projCoord;

void main()
{
  vec4 shadowValue = texture2DProj(shadowMap, projCoord);
  
  if(shadowValue.z < projCoord.z/projCoord.w)
    gl_FragColor = vec4(0.0);
  else
    gl_FragColor = vec4(1.0);
}
