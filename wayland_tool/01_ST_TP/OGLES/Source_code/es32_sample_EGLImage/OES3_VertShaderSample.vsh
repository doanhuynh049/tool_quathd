attribute highp   vec3 aVertex;
attribute mediump vec3 aNormal;
attribute mediump vec2 aUv1;
attribute mediump vec4 aDiffuse;
uniform   mediump mat4 uPMVMatrix;
varying   mediump vec2 vTexCoord;

void main(void)
{
	gl_Position = uPMVMatrix * vec4(aVertex,1.0);
	vTexCoord.x = aUv1.s;
	vTexCoord.y = -aUv1.t;
}
