// Plain texturing
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint
//
// Options:
// #define TEXTURE
// #define TEXTURE_IS_CUBE
// #define  CUBE_TO_EQUIRECTANGULAR
// #define  CUBE_TO_LITTLE_PLANET
// #define  CUBE_TO_FISHEYE
// #define  CUBE_TO_WARP
// #define GAMMA
// #define HSV
// #define STEPS
// #define SHOW_ALPHA0
// #define MIRROR_MASK

#define RR_PI 3.1415926535897932384626433832795

uniform vec4 color;
uniform float gamma;

#ifdef TEXTURE
	#ifdef TEXTURE_IS_CUBE
		uniform samplerCube map;
		#ifdef CUBE_TO_FISHEYE
			uniform float fisheyeFovDeg;
		#endif
		#ifdef CUBE_TO_WARP
			varying float intensity;
		#endif
	#else
		uniform sampler2D map;
	#endif
	varying vec2 uv;
#endif
#ifdef SHOW_ALPHA0
	#extension GL_EXT_gpu_shader4 : require // testing bits is much easier with GL3/DX10 generation GPU
	uniform vec2 resolution;
#endif

#ifdef HSV
	uniform vec3 hsv; // h adder, s,v multiplier

	vec3 rgb2hsv(vec3 c)
	{
		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

		float d = q.x - min(q.w, q.y);
		float e = 1.0e-10;
		return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
	}

	vec3 hsv2rgb(vec3 c)
	{
		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
		return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
	}
#endif

#ifdef STEPS
	uniform float steps;
#endif

void main()
{

#ifdef TEXTURE
	#ifdef TEXTURE_IS_CUBE
		vec3 direction;
		#if !defined(CUBE_TO_LITTLE_PLANET) && !defined(CUBE_TO_FISHEYE) && !defined(CUBE_TO_WARP)
			#define CUBE_TO_EQUIRECTANGULAR // this happens when user renders cubemap as a 2d texture, without any #defines
		#endif
		// [#42]
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
		#ifdef CUBE_TO_FISHEYE
			direction.xz = uv.xy-vec2(0.5,0.5);
			float r = length(direction.xz)+0.000001; // +epsilon fixes center pixel on intel
			direction.xz = direction.xz/r; // /r instead of normalize() fixes noise on intel
			direction.y = tan(RR_PI*2.0*(r-0.25)); // r=0 -> y=-inf, r=0.5 -> y=+inf
			vec4 tex = textureCube(map,direction.xzy) * step(r*360.0/fisheyeFovDeg,0.5);
		#endif
		#ifdef CUBE_TO_WARP
			// similar to CUBE_TO_EQUIRECTANGULAR
			float uv_x = uv.x*.5+0.25;
			direction.y = sin(RR_PI*(uv.y-0.5));
			direction.x = sin(RR_PI*(2.0*uv_x+1.0)) * sqrt(1.0-direction.y*direction.y);
			direction.z = sqrt(max(1.0-direction.x*direction.x-direction.y*direction.y,0.0)); // max() fixes center lines on intel
			if (uv_x>0.25 && uv_x<0.75)
				direction.z = -direction.z;
			vec4 tex = textureCube(map,direction);
			tex *= intensity;
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

#ifdef TEXTURE
	gl_FragColor = tex;
#else
	gl_FragColor = vec4(1.0,1.0,1.0,1.0);
#endif

#ifdef HSV
	gl_FragColor.xyz = hsv2rgb(rgb2hsv(gl_FragColor.xyz)*vec3(1.0,hsv.yz)+vec3(hsv.x,0.0,0.0));
#endif

#ifdef STEPS
	gl_FragColor = floor(gl_FragColor * steps + vec4(0.5)) / steps;
#endif

	gl_FragColor *= color;

#ifdef GAMMA
	gl_FragColor = pow(gl_FragColor,vec4(gamma,gamma,gamma,1));
#endif

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
