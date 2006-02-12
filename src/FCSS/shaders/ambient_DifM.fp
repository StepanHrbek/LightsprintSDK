uniform sampler2D diffuseTex;

varying vec2 diffuseCoord;

void main()
{
  vec4 diffuseValue = texture2D(diffuseTex, diffuseCoord);
  gl_FragColor = gl_Color * diffuseValue;
  gl_FragColor += gl_Color/4.0;
  gl_FragColor *= gl_FragColor; // boost contrast. this is not realistic
}
