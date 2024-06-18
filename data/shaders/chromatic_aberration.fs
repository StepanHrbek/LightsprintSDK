// RL - chromatic aberration effect (red shift)

uniform sampler2D map;
uniform vec2 redShift;
in vec2 mapCoord;
out vec4 fragColor;

void main()
{
	vec4 color0 = texture(map,mapCoord);
	vec4 color1 = texture(map,mapCoord+redShift);
	fragColor = vec4(color1.r,color0.gba);
}
