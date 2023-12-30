attribute highp   vec3 aVertex;
attribute mediump vec2 aMultiTexCoord0;
uniform   mediump mat4 uPMVMatrix;
varying   mediump vec2 vTexCoord;

void main(void)
{
	gl_Position = uPMVMatrix * vec4(aVertex,1.0);
	vTexCoord.x   = aMultiTexCoord0.x;
	vTexCoord.y   = aMultiTexCoord0.y;
}
