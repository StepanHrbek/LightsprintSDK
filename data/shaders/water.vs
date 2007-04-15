// Water with planar reflection and waves
// Copyright (C) Stepan Hrbek, Lightsprint 2007

varying vec4 mirrorCoord;
varying vec2 worldPos;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	//mirrorCoord = (gl_ModelViewProjectionMatrix * (gl_Vertex+gl_Normal)).xy;
	mirrorCoord = gl_Position;
	worldPos = gl_Position.xz;
}
