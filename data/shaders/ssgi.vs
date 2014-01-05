// Screen space global illumination
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint

attribute vec2 vertexPosition;
varying vec2 mapCoord;

void main()
{
	mapCoord = vertexPosition*0.5+vec2(0.5,0.5);
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
