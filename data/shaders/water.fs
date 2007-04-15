// Water with planar reflection and waves
// Copyright (C) Stepan Hrbek, Lightsprint 2007

sampler2D mirrorMap;
uniform float time; // in seconds
varying vec4 mirrorCoord;
varying vec2 worldPos;

void main()
{
	vec2 uv = vec2(0.5,0.5)+vec2(0.5,-0.5)*mirrorCoord.xy/mirrorCoord.w;

	vec3 normal = vec3(
		0.003*sin(worldPos.x*6.3+time*5.4)+0.002*cos(worldPos.x*33.0+worldPos.y*46.0+time*9.3),
		1,
		0.040*sin(worldPos.y*8.0+time*3.3)+0.015*cos(worldPos.x*53.0+worldPos.y*33.0+time*12.3));

	gl_FragColor = texture2D(mirrorMap,uv+normal.xz);
}
