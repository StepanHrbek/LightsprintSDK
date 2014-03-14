// Plain texturing
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define GAMMA
// #define SHOW_ALPHA0
// #define MIRROR_MASK

attribute vec2 vertexPosition;

#ifdef TEXTURE
	attribute vec2 vertexUvDiffuse;
	varying vec2 uv;
#endif

#ifdef CUBE_TO_WARP
	uniform vec2 scale;
	attribute float vertexColor;
	varying float intensity;
#endif

void main()
{
	gl_Position = vec4(vertexPosition,1.0,1.0);
#ifdef TEXTURE
	uv = vertexUvDiffuse;
#endif
#ifdef CUBE_TO_WARP
	gl_Position.xy *= scale;
	intensity = vertexColor;
#endif
}
