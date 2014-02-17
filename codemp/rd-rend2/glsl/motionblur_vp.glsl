attribute vec4 attr_Position;
attribute vec4 attr_TexCoord0;

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_ModelViewProjectionMatrixInverse;

uniform vec3   u_ViewForward;
uniform vec3   u_ViewLeft;
uniform vec3   u_ViewUp;

varying vec2   var_ScreenTex;
varying vec3   var_ViewDir;

varying vec4	vPosition;
varying vec4	vPrevPosition;

void main()
{
	gl_Position = attr_Position;
	var_ScreenTex = attr_TexCoord0.xy;
	
	vec2 screenCoords = gl_Position.xy / gl_Position.w;
	var_ViewDir = u_ViewForward + u_ViewLeft * -screenCoords.x + u_ViewUp * screenCoords.y;
	
	vPosition = u_ModelViewProjectionMatrixInverse * gl_Vertex;
	vPrevPosition = u_ModelViewProjectionMatrix * gl_Vertex;
}