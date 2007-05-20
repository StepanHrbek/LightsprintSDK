// Plain texturing with options:
//
// #define TEXTURE
// #define SHOW_ALPHA0

uniform vec4 color;

#ifdef TEXTURE
	uniform sampler2D map;
	varying vec2 uv;
#endif

void main()
{

#ifdef TEXTURE
	vec4 tex = texture2D(map,uv);
#ifdef SHOW_ALPHA0
	if(tex.a<0.003)
		tex = vec4(0.0,0.0,1.0,0.0);
	if(tex.a>0.997)
		tex = vec4(0.0,0.5,0.0,0.0);
	else
		tex = vec4(1.0-tex.a,1.0-tex.a,1.0-tex.a,1.0-tex.a);
#endif
#endif

	gl_FragColor = color
#ifdef TEXTURE
		* tex
#endif
		;
}
