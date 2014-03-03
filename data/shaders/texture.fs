// Plain texturing
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define TEXTURE_IS_CUBE
// #define  CUBE_TO_EQUIRECTANGULAR
// #define  CUBE_TO_LITTLE_PLANET
// #define  CUBE_TO_DOME
// #define GAMMA
// #define SHOW_ALPHA0
// #define MIRROR_MASK

#define RR_PI 3.1415926535897932384626433832795

uniform vec4 color;
uniform float gamma;

#ifdef TEXTURE
	#ifdef TEXTURE_IS_CUBE
		uniform samplerCube map;
	#else
		uniform sampler2D map;
	#endif
	varying vec2 uv;
#endif
#ifdef SHOW_ALPHA0
	#extension GL_EXT_gpu_shader4 : require // testing bits is much easier with GL3/DX10 generation GPU
	uniform vec2 resolution;
#endif

void main()
{

#ifdef TEXTURE
	#ifdef TEXTURE_IS_CUBE
		vec3 direction;
		#if !defined(CUBE_TO_LITTLE_PLANET) && !defined(CUBE_TO_DOME)
			#define CUBE_TO_EQUIRECTANGULAR // this happens when user renders cubemap as a 2d texture, without any #defines
		#endif
		#ifdef CUBE_TO_EQUIRECTANGULAR
			// rotated so that render of empty scene with equirectangular environment E is E
			// also implemented in createEquirectangular()
			direction.y = sin(RR_PI*(uv.y-0.5));
			direction.x = sin(RR_PI*(2.0*uv.x+1.5)) * sqrt(1.0-direction.y*direction.y);
			direction.z = sqrt(max(1.0-direction.x*direction.x-direction.y*direction.y,0.0)); // max() fixes center lines on intel
			if (uv.x<0.5)
				direction.z = -direction.z;
			vec4 tex = textureCube(map,direction);
		#endif
		#ifdef CUBE_TO_LITTLE_PLANET
			direction.xz = uv.xy-vec2(0.5,0.5);
			float r = length(direction.xz)+0.000001; // +epsilon fixes center pixel on intel
			direction.xz = direction.xz/r; // /r instead of normalize() fixes noise on intel
			direction.y = tan(RR_PI*2.0*(r-0.25)); // r=0 -> y=-inf, r=0.5 -> y=+inf
			vec4 tex = textureCube(map,direction) * step(r,0.5);
		#endif
		#ifdef CUBE_TO_DOME
			direction.xz = uv.xy-vec2(0.5,0.5);
			float r = 0.5*length(direction.xz)+0.000001; // +epsilon fixes center pixel on intel
			direction.xz = direction.xz/r; // /r instead of normalize() fixes noise on intel
			direction.y = tan(RR_PI*2.0*(r-0.25)); // r=0 -> y=-inf, r=0.5 -> y=+inf
			vec4 tex = textureCube(map,direction.xzy) * step(r,0.25);
		#endif
	#else
		vec4 tex = texture2D(map,uv);
	#endif
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
	if (color.a*tex.a<0.7) discard; // big pixels with over 70% of surface covered by hires mirror are nearly always inside lowres mirror. 0.65 would result in occassional leaks, some pixels with 65% coverage are outside lowres mirror. (btw, when we had lowres exactly 50% of hires, 0.51 seemed sufficient)
#endif
#ifdef MIRROR_MASK_ALPHA
	gl_FragColor = vec4(0.0,0.0,0.0,step(0.7,color.a*tex.a)); // the same 0.7 threshold
#endif
#ifdef MIRROR_MASK_DEBUG
	gl_FragColor = vec4(color.a*tex.a);
#endif
}
