// RL - chromatic aberration effect (red shift)

layout(location = 0) in vec2 vertexPosition;
out vec2 mapCoord;

void main()
{
	mapCoord = vertexPosition*0.5+vec2(0.5,0.5);
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
