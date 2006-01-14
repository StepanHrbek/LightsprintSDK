uniform sampler2D shadowMap, lightTex;

varying vec4 projCoord;

const vec4 ambient = vec4(0.13), boost = vec4(1.06);

void main()
{
  vec3 projectiveBiased = (projCoord.xyz / projCoord.q);
  projectiveBiased = (projectiveBiased + 1.0) * 0.5;
  
  vec4 shadowValue = texture2D(shadowMap, projectiveBiased.xy);
  vec4 lightValue = texture2D(lightTex, projectiveBiased.xy);
  
  if(shadowValue.z < projectiveBiased.z)
    gl_FragColor = ambient;
  else
    gl_FragColor = boost * gl_Color * lightValue + ambient;
}
