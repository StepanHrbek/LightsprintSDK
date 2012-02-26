// Plain texturing
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define SHOW_ALPHA0

#ifdef TEXTURE
	varying vec2 uv;
#endif

void main()
{
	gl_Position = gl_Vertex;
#ifdef TEXTURE
	uv = gl_MultiTexCoord0.xy;
#endif
}
