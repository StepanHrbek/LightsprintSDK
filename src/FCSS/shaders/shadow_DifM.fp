uniform sampler2D shadowMap, lightTex, diffuseTex;

const float lightIntensity = 2.0;

//!!! fudge factors depending on scene
// linear decay approximates realistic attenuation + logaritmic transform in eye
//const float linearDecay = 25.0;

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
  {
//    float attenuation = 1-projCoord.z*(1/linearDecay);
    gl_FragColor = gl_Color * lightValue * diffuseValue * lightIntensity;// * attenuation;
  }
}
