// Contours
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint

attribute vec2 vertexPosition;
varying vec2 tMapCoord;

void main()
{
	tMapCoord = vertexPosition*0.5+vec2(0.5,0.5);
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
