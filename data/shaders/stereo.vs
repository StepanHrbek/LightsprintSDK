// SceneViewer: Stereo compositing filter, converts top-down images to single interlaced image
// Copyright (C) 2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define SIZEX n
// #define SIZEY n

varying vec2 texcoord;

void main()
{
	texcoord = gl_MultiTexCoord0.xy;
	gl_Position = gl_Vertex;
}
