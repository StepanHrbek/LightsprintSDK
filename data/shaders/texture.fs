// Plain texturing
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define SHOW_ALPHA0

uniform vec4 color;

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

	gl_FragColor = color
#ifdef TEXTURE
		* tex
#endif
		;
}
