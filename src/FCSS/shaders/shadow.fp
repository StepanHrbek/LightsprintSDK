uniform sampler2D shadowMap;

varying vec4 projCoord;

void main()
{
  vec3 projectiveBiased = (projCoord.xyz / projCoord.q);
  projectiveBiased = (projectiveBiased + 1.0) * 0.5;
  
  vec4 shadowValue = texture2D(shadowMap, projectiveBiased.xy);
  
  if(shadowValue.z < projectiveBiased.z)
    gl_FragColor = vec4(0.0);
  else
    gl_FragColor = 1;
}
