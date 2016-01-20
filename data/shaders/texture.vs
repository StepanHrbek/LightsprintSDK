// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Plain old 2d texturing
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
