// Plain texturing
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define TEXTURE_IS_CUBE
// #define PANORAMA_MODE [1|2]
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
	#extension GL_EXT_gpu_shader4 : enable // testing bits is much easier with GL3/DX10 generation GPU
	uniform vec2 resolution;
#endif

void main()
{

#ifdef TEXTURE
	#ifdef TEXTURE_IS_CUBE
		vec3 direction;
		#if PANORAMA_MODE+0==2
			// little planet
			direction.xz = uv.xy-vec2(0.5,0.5);
			float r = 2.0*length(direction.xz);
			direction.xz = normalize(direction.xz);
			direction.y = tan(RR_PI*(r-0.5)); // r=0 -> y=-inf, r=1 -> y=+inf
			vec4 tex = textureCube(map,direction) * step(r,1.0);
		#else
			// equirectangular
			// rotated so that render of empty scene with equirectangular environment E is E
			// also implemented in createEquirectangular()
			direction.y = sin(RR_PI*(uv.y-0.5));
			direction.x = sin(RR_PI*(2.0*uv.x+1.5)) * sqrt(1.0-direction.y*direction.y);
			direction.z = sqrt(1.0-direction.x*direction.x-direction.y*direction.y);
			if (uv.x<0.5)
				direction.z = -direction.z;
			vec4 tex = textureCube(map,direction);
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
