uniform sampler2D	u_ScreenImageMap;
uniform sampler2D	u_ScreenDepthMap;

uniform mat4		u_ModelViewProjectionMatrix;	// previous MVP
uniform vec4		u_ViewInfo;					// zfar / znear, zfar
uniform vec3		u_ViewOrigin;

uniform int			u_UserInt1;						// r_motionblur
uniform int			u_UserInt2;						// r_motionblurnumsamples
uniform float		u_UserFloat1;					// r_motionblurVelocityScale
uniform float		u_UserFloat2;					// r_motionblurDepthThreshold

varying vec3		var_ViewDir;
varying vec2		var_ScreenTex;

// Only used by r_motionblur 2
varying vec4		var_VertPosition;
varying vec4		var_PrevVertPosition;

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
		float sampleZDivW = texture2D(depthMap, tex).r;
		return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}
 
void main(void)
{
	if(u_UserInt1 == 1) {
		vec4 		color			=	texture2D(u_ScreenImageMap, var_ScreenTex);
		vec4 		depthColor		=	texture2D(u_ScreenDepthMap, var_ScreenTex);
		float 		zFar			=	u_ViewInfo.y;
		float 		zNear			=	zFar / u_ViewInfo.x;
		float 		zFarDivZNear	=	u_ViewInfo.x;
		float		linearDepth		=	getLinearDepth(u_ScreenDepthMap, var_ScreenTex, zFarDivZNear);
		float 		sampleZ			=	zFar *  linearDepth;
	
		vec4		worldPos		=	vec4(u_ViewOrigin + var_ViewDir * sampleZ, 1.0);

		// the viewport position at this pixel in the range -1 to 1.
		vec2		currentPos		=	var_ScreenTex * 2.0 - vec2 (1.0);

		// Use the world position, and transform by the previous view-projection matrix.
		vec4		previousPos		=	u_ModelViewProjectionMatrix * worldPos;


		// Convert to nonhomogeneous points [-1,1] by dividing by w.
		previousPos = previousPos / vec4(previousPos.w);
		// Use this frame's position and last frame's to compute the pixel velocity.
		vec2 velocity = vec2(currentPos.xy - previousPos.xy)/2.0;
		if(linearDepth < u_UserFloat2) {
			// Don't blur the viewmodel.
			velocity = 0.0;
		}

		int nSamples = u_UserInt2;
		vec2 forwardPosition = var_ScreenTex;

		// The velocity vector gets divided by the number of samples, that way the sample count doesn't affect blur length
		velocity /= (nSamples/2.0);
		velocity *= u_UserFloat1;

		// Loop through each sample, blurring along the way.
		for(int i = 1; i < nSamples; ++i, forwardPosition -= velocity)  
		{  
			// Sample the color buffer along the velocity vector.
			vec4 currentColor = texture2D(u_ScreenImageMap, forwardPosition);  
			// Add the current color to our color sum.  
			currentColor.w = 1.0;
			color += currentColor;
		}  
	
		gl_FragColor = color/nSamples;
	}
	else {
		// Let's just render a velocity map for now
		vec2 extrapolatedCurPos = (var_VertPosition.xy / var_VertPosition.w) * 0.5 + 0.5;
		vec2 extrapolatedPrevPos = (var_PrevVertPosition.xy / var_PrevVertPosition.w) * 0.5 + 0.5;
		vec2 vertVelocity = extrapolatedCurPos - extrapolatedPrevPos;

		gl_FragColor = vec4(vertVelocity.x, vertVelocity.y, 0.5, 1.0);
	}
}