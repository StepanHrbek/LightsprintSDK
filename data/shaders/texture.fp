// Plain texturing

uniform sampler2D map;
uniform vec4 color;
varying vec2 uv;

void main()
{
	gl_FragColor = texture2D(map,uv)*color;
}
