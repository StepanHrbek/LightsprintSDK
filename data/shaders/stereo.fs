// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Stereo compositing filter.
//
// Options:
// INTERLACED  - converts top-down image to single interlaced image
// OCULUS_RIFT - converts side by side image to Oculus Rift format

uniform sampler2D map;
in vec2 texcoord;
out vec4 fragColor;

#ifdef INTERLACED

	uniform float mapHalfHeight;

	void main()
	{
		fragColor = texture(map,vec2(texcoord.x,0.5*(texcoord.y+floor(2.0*fract(texcoord.y*mapHalfHeight)))));
	}

#endif // INTERLACED

#ifdef OCULUS_RIFT

	uniform vec2 LensCenter;
	uniform vec2 ScreenCenter;
	uniform vec2 Scale;
	uniform vec2 ScaleIn;
	uniform vec4 HmdWarpParam;
	uniform vec4 ChromAbParam;

	// Scales input texture coordinates for distortion.
	// ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
	// larger due to aspect ratio.
	void main()
	{
		vec2  theta = (texcoord - LensCenter) * ScaleIn; // Scales to [-1, 1]
		float rSq= theta.x * theta.x + theta.y * theta.y;
		vec2  theta1 = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq + HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);

		// Detect whether blue texture coordinates are out of range since these will scaled out the furthest.
		vec2 thetaBlue = theta1 * (ChromAbParam.z + ChromAbParam.w * rSq);
		vec2 tcBlue = LensCenter + Scale * thetaBlue;
		if (!all(equal(clamp(tcBlue, ScreenCenter-vec2(0.25,0.5), ScreenCenter+vec2(0.25,0.5)), tcBlue)))
		{
			fragColor = vec4(0.0);
			return;
		}
		
		// Now do blue texture lookup.
		float blue = texture(map, tcBlue).b;
		
		// Do green lookup (no scaling).
		vec2  tcGreen = LensCenter + Scale * theta1;
		vec4  center = texture(map, tcGreen);
		
		// Do red scale and lookup.
		vec2  thetaRed = theta1 * (ChromAbParam.x + ChromAbParam.y * rSq);
		vec2  tcRed = LensCenter + Scale * thetaRed;
		float red = texture(map, tcRed).r;
		
		fragColor = vec4(red, center.g, blue, 1.0);
	}

#endif // OCULUS_RIFT
