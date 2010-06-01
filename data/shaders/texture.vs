// Plain texturing
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
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
