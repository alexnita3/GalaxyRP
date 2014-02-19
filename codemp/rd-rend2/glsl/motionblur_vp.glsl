attribute vec3	attr_Position;
attribute vec4	attr_TexCoord0;

uniform mat4	u_ModelViewProjectionMatrix;
uniform mat4	u_ModelViewProjectionMatrixInverse;
uniform vec3	u_ViewForward;
uniform vec3	u_ViewLeft;
uniform vec3	u_ViewUp;
uniform int		u_UserInt1;

varying vec2	var_ScreenTex;

// Only used by r_motionblur 1
varying vec3	var_ViewDir;

// Only used by r_motionblur 2
varying vec4	var_VertPosition;
varying vec4	var_PrevVertPosition;

void main()
{
	gl_Position = u_ModelViewProjectionMatrixInverse * vec4(attr_Position, 1.0);
	var_ScreenTex = attr_TexCoord0.xy;
	
	if(u_UserInt1 == 1) {
		vec2 screenCoords = gl_Position.xy / gl_Position.w;
		var_ViewDir = u_ViewForward + u_ViewLeft * -screenCoords.x + u_ViewUp * screenCoords.y;
	}
	else {
		var_VertPosition = u_ModelViewProjectionMatrixInverse *  vec4(attr_Position, 1.0);
		var_PrevVertPosition = u_ModelViewProjectionMatrix * vec4(attr_Position, 1.0);
	}
}