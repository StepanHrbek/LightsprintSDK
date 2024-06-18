// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Bloom effect
//
// Options:
// #define PASS [1|2|3]

uniform sampler2D map;
uniform vec2 pixelDistance;
uniform float threshold;
in vec2 mapCoord;
out vec4 fragColor;

void main()
{
#if PASS==1
	// downscale source image
	vec4 color = (
		+texture(map,mapCoord+pixelDistance*vec2(-1.0,-1.0))
		+texture(map,mapCoord+pixelDistance*vec2(-1.0,+1.0))
		+texture(map,mapCoord+pixelDistance*vec2(+1.0,-1.0))
		+texture(map,mapCoord+pixelDistance*vec2(+1.0,+1.0))
		) / 4.0;
//	fragColor = step(vec4(1.0,1.0,1.0,1.0),color);
	fragColor = (color.r+color.g+color.b>=threshold)?color:vec4(0.0,0.0,0.0,0.0);
#elif PASS==2
	// blur downscaled image
	fragColor = (
		+texture(map,mapCoord-8.5*pixelDistance)*0.2
		+texture(map,mapCoord-6.5*pixelDistance)*0.4
		+texture(map,mapCoord-4.5*pixelDistance)*0.6
		+texture(map,mapCoord-2.5*pixelDistance)*0.8
		+texture(map,mapCoord-0.5*pixelDistance)
		+texture(map,mapCoord+1.5*pixelDistance)*0.9
		+texture(map,mapCoord+3.5*pixelDistance)*0.7
		+texture(map,mapCoord+5.5*pixelDistance)*0.5
		+texture(map,mapCoord+7.5*pixelDistance)*0.3
		+texture(map,mapCoord+9.5*pixelDistance)*0.1
		) / 5.5;
#else
	// blend blurred image into source
	fragColor = texture(map,mapCoord);
#endif
}
