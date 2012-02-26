// SceneViewer: Stereo compositing filter, converts top-down images to single interlaced image
// Copyright (C) 2012 Stepan Hrbek, Lightsprint

uniform sampler2D map;
uniform float mapHalfHeight;
varying vec2 texcoord;

void main()
{
	gl_FragColor = texture2D(map,vec2(texcoord.x,0.5*(texcoord.y+floor(2.0*fract(texcoord.y*mapHalfHeight)))));
}
