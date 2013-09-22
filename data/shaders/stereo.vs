// SceneViewer: Stereo compositing filter, converts top-down images to single interlaced image
// Copyright (C) 2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define SIZEX n
// #define SIZEY n

attribute vec2 vertexPosition;
varying vec2 texcoord;

void main()
{
	texcoord = (vertexPosition+vec2(1.0,1.0))*0.5;
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
