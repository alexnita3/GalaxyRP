uniform mat4 u_ModelViewProjectionMatrix;
uniform sampler2D u_TextureMap;
uniform vec4   u_ViewInfo; // zfar / znear, zfar
uniform sampler2D u_ScreenDepthMap;
uniform vec3   u_ViewOrigin;

varying vec3   var_ViewDir;

varying vec2   var_ScreenTex;
varying vec4	

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
		float sampleZDivW = texture2D(depthMap, tex).r;
		return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

float getRawPosition(sampler2D depthMap, const vec2 tex)
{
	float sampleZDivW = texture2D(depthMap, tex).r;
	return sampleZDivW * 2.0 - 1.0;
}
 
void main(void)
{
	vec4 		color = texture2D(u_TextureMap, var_ScreenTex);
	vec4 		depthColor = texture2D(u_ScreenDepthMap, var_ScreenTex);
	float 		zFar = u_ViewInfo.y;
	float 		zNear = zFar / u_ViewInfo.x;
	float 		zFarDivZNear = u_ViewInfo.x;
	float 		sampleZ = zFar * getLinearDepth(u_ScreenDepthMap, var_ScreenTex, zFarDivZNear);
	//sampleZ = (sampleZ-zNear)/(zFar-zNear);
	float		rawSampleZ = getRawPosition(u_ScreenDepthMap, var_ScreenTex);
	
	vec4 worldPos = vec4(u_ViewOrigin + var_ViewDir * sampleZ, 1.0);
	
	// the viewport position at this pixel in the range -1 to 1.
	vec4 currentPos = vec4(var_ScreenTex.x * 2 - 1, (1 - var_ScreenTex.y) * 2 - 1, rawSampleZ, 1);;
	// Use the world position, and transform by the previous view-projection matrix.
	vec4 previousPos = u_ModelViewProjectionMatrix * worldPos;
	// Convert to nonhomogeneous points [-1,1] by dividing by w.
	previousPos = previousPos / vec4(previousPos.w);
	// Use this frame's position and last frame's to compute the pixel velocity.
	vec2 velocity = vec2(currentPos.xy - previousPos.xy)/2.0;
	
	vec2 forwardPosition = var_ScreenTex + velocity;
	for(int i = 1; i < 5; ++i, forwardPosition += velocity)  
	{  
		// Sample the color buffer along the velocity vector.  
		vec4 currentColor = texture2D(u_TextureMap, forwardPosition);  
		// Add the current color to our color sum.  
		color += currentColor;  
	}  
	
	//gl_FragColor = color;
	gl_FragColor = vec4(velocity.x, velocity.y, 0, 1.0);
	//gl_FragColor = vec4(worldPos/32.0, 1.0);
}