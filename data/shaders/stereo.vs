// Stereo compositing filter.
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint

attribute vec2 vertexPosition;
varying vec2 texcoord;

void main()
{
	texcoord = (vertexPosition+vec2(1.0,1.0))*0.5;
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
