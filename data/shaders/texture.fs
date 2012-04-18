// Plain texturing
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define GAMMA
// #define SHOW_ALPHA0
// #define MIRROR_MASK

uniform vec4 color;
uniform float gamma;

#ifdef TEXTURE
	uniform sampler2D map;
	varying vec2 uv;
#endif
#ifdef SHOW_ALPHA0
	#extension GL_EXT_gpu_shader4 : enable // testing bits is much easier with GL3/DX10 generation GPU
	uniform vec2 resolution;
#endif

void main()
{

#ifdef TEXTURE
	vec4 tex = texture2D(map,uv);
#ifdef SHOW_ALPHA0
	#define QUADRANT_X0Y0 1
	#define QUADRANT_X1Y0 2
	#define QUADRANT_X0Y1 4
	#define QUADRANT_X1Y1 8
	#define EDGE_X0 16
	#define EDGE_X1 32
	#define EDGE_Y0 64
	#define EDGE_Y1 128
	vec2 positionInTexel = fract(uv*resolution);
	float gradient = (fract(positionInTexel.x*2.0)+fract(positionInTexel.y*2.0))*0.5;
	int flags = int(tex.a*255.+0.5);
	if (false
		|| ((flags&EDGE_X0)!=0 && positionInTexel.x<0.1)
		|| ((flags&EDGE_X1)!=0 && positionInTexel.x>0.9)
		|| ((flags&EDGE_Y0)!=0 && positionInTexel.y<0.1)
		|| ((flags&EDGE_Y1)!=0 && positionInTexel.y>0.9)
	   )
		tex = vec4(gradient,1.0,0.0,0.0);
	else
	if (false
		|| ((flags&QUADRANT_X0Y0)!=0 && positionInTexel.x<0.5 && positionInTexel.y<0.5)
		|| ((flags&QUADRANT_X1Y0)!=0 && positionInTexel.x>0.5 && positionInTexel.y<0.5)
		|| ((flags&QUADRANT_X0Y1)!=0 && positionInTexel.x<0.5 && positionInTexel.y>0.5)
		|| ((flags&QUADRANT_X1Y1)!=0 && positionInTexel.x>0.5 && positionInTexel.y>0.5)
	   )
		tex = vec4(gradient,0.5,0.0,0.0);
	else
		tex = vec4(0.0,0.0,0.0,0.0);
#endif
#endif

	gl_FragColor =
#ifdef GAMMA
		pow(
#endif
		color
#ifdef TEXTURE
		* tex
#endif
#ifdef GAMMA
		,vec4(gamma,gamma,gamma,1))
#endif
		;

#ifdef MIRROR_MASK_DEPTH
	if (color.a*tex.a<0.51) discard; // 0.51 to not reflect pixels behind wall (excludes some pixels inside too)
#endif
#ifdef MIRROR_MASK_ALPHA
	gl_FragColor = vec4(0.0,0.0,0.0,step(0.51,color.a*tex.a)); // 0.51 to not reflect pixels behind wall (excludes some pixels inside too)
#endif
}
