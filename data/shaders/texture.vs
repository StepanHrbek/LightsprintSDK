// Plain texturing with options:
//
// #define TEXTURE

#ifdef TEXTURE
	varying vec2 uv;
#endif

void main()
{
	gl_Position = gl_Vertex;
#ifdef TEXTURE
	uv = gl_MultiTexCoord0.xy;
#endif
}
