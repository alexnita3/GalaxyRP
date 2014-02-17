uniform mat4 u_ModelViewProjectionMatrixInverse;
uniform mat4 u_ModelViewProjectionMatrix;
uniform sampler2D u_TextureMap;
uniform vec4   u_ViewInfo; // zfar / znear, zfar
uniform sampler2D u_ScreenDepthMap;

varying vec2   var_ScreenTex;

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
		float sampleZDivW = texture2D(depthMap, tex).r;
		return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}
 
void main(void)
{
	vec4 color = texture2D(u_TextureMap, var_ScreenTex);
	float zFar = u_ViewInfo.y;
	float zFarDivZNear = u_ViewInfo.x;
	float sampleZ = zFar * getLinearDepth(u_ScreenDepthMap, var_ScreenTex, zFarDivZNear);
	// H is the viewport position at this pixel in the range -1 to 1.
	vec4 H = vec4(var_ScreenTex.x * 2 - 1, (1 - var_ScreenTex.y) * 2 - 1, gl_FragCoord.z, 1);
	// Transform by the view-projection inverse.
	vec4 D = u_ModelViewProjectionMatrixInverse * H;
	// Divide by w to get the world position.
	vec4 worldPos = D / vec4(D.w);
	 
	// Current viewport position
	vec4 currentPos = H;
	// Use the world position, and transform by the previous view-projection matrix.
	vec4 previousPos = u_ModelViewProjectionMatrix * worldPos;
	// Convert to nonhomogeneous points [-1,1] by dividing by w.
	previousPos = previousPos / vec4(previousPos.w);
	// Use this frame's position and last frame's to compute the pixel velocity.
	vec2 velocity = vec2(currentPos.xy - previousPos.xy)/2.0;
	//velocity = (velocity + 1.0 ) / 2.0;
	 
	//gl_FragColor = vec4(velocity.x, velocity.y, color.g, 1.0);
	gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}