// RL - chromatic aberration effect (red shift)

uniform sampler2D map;
uniform vec2 redShift;
varying vec2 mapCoord;

void main()
{
	vec4 color0 = texture2D(map,mapCoord);
	vec4 color1 = texture2D(map,mapCoord+redShift);
	gl_FragColor = vec4(color1.r,color0.gba);
}
