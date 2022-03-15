gfx/effects/hologramShader
{
	surfaceparm	nonsolid
	surfaceparm	nonopaque
	surfaceparm	trans
	cull	twosided
    {
        map gfx/effects/blue_glow
        blendFunc GL_SRC_ALPHA GL_SRC_COLOR
		rgbGen wave sin 0.9 0.1 0.1 0.1
        alphaGen wave sin 0.7 0.1 0.1 0.1
        tcMod rotate 15
        tcMod turb 0 0.03 0 2
    }
}