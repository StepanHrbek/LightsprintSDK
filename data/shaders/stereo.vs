// SceneViewer: Stereo compositing filter, converts top-down images to single interlaced image
// Copyright (C) 2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define SIZEX n
// #define SIZEY n

varying vec2 texcoord;

void main()
{
	texcoord = (gl_Vertex.xy+vec2(1.0,1.0))*0.5;
	gl_Position = gl_Vertex;
}
