// Copyright (C) 1999-2021 Stepan Hrbek
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
// #define DIVIDE_UV_BY_W

layout(location = 0) in vec2 vertexPosition;

#ifdef TEXTURE
	#ifdef DIVIDE_UV_BY_W
		layout(location = 2) in vec3 vertexUvDiffuse; // c++ code uses "vertexUvDiffuse" name for both uv and uvw
		out vec3 uvw;
	#else
		layout(location = 2) in vec2 vertexUvDiffuse;
		out vec2 uv;
	#endif
#endif

#ifdef CUBE_TO_WARP
	uniform vec2 scale;
	layout(location = 9) in float vertexColor;
	out float intensity;
#endif

void main()
{
	gl_Position = vec4(vertexPosition,1.0,1.0);
#ifdef TEXTURE
	#ifdef DIVIDE_UV_BY_W
		uvw = vertexUvDiffuse;
	#else
		uv = vertexUvDiffuse;
	#endif
#endif
#ifdef CUBE_TO_WARP
	gl_Position.xy *= scale;
	intensity = vertexColor;
#endif
}
