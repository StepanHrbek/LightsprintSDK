uniform sampler2D shadowMap, lightTex, diffuseTex;

varying vec4 projCoord;
varying vec2 diffuseCoord;

void main()
{
  vec3 projectiveBiased = (projCoord.xyz / projCoord.q);
  projectiveBiased = (projectiveBiased + 1.0) * 0.5;
  
  vec4 shadowValue = texture2D(shadowMap, projectiveBiased.xy);
  vec4 lightValue = texture2D(lightTex, projectiveBiased.xy);
  vec4 diffuseValue = texture2D(diffuseTex, diffuseCoord);
  
  if(shadowValue.z < projectiveBiased.z)
    gl_FragColor = vec4(0.0);
  else
    gl_FragColor = gl_Color * lightValue * diffuseValue;
}
