// Plain texturing
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define SHOW_ALPHA0

attribute vec2 vertexPosition;

#ifdef TEXTURE
	attribute vec2 vertexUv0;
	varying vec2 uv;
#endif

void main()
{
	gl_Position = vec4(vertexPosition,1.0,1.0);
#ifdef TEXTURE
	uv = vertexUv0;
#endif
}
