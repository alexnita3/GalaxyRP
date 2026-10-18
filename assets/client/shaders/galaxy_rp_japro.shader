
gfx/effects/sabers/RGBGlow
{	
	nopicmip
	notc
	cull twosided
	{
		map gfx/effects/sabers/RGBGlow
		blendFunc GL_ONE GL_ONE
		glow
		
		rgbGen vertex
	}
}

gfx/effects/sabers/RGBCore
{
	nopicmip
	notc
	cull twosided
	{
		map gfx/effects/sabers/RGBCore
		blendFunc GL_ONE GL_ONE
		
		rgbGen vertex
	}
}

gfx/2d/minimap
{
	nopicmip
	notc
	q3map_nolightmap
	{
		map gfx/2d/minimap
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/effects/grapple_line
{
	cull twosided
	{
		map gfx/effects/grapple_line
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
		glow
	}
}

gfx/misc/splash
{
	cull	disable
    {
        clampmap gfx/misc/splash
        blendFunc GL_ONE GL_ONE
        rgbGen wave sawtooth 1 -1 0 0.8
        tcMod stretch sawtooth 0.5 0.55 0 0.8
        tcMod rotate -15
    }
    {
        clampmap gfx/misc/splash
        blendFunc GL_ONE GL_ONE
        rgbGen wave sawtooth 1 -1 0.33 0.8
        alphaGen const 0.6
        tcMod rotate 30
        tcMod stretch sawtooth 0.5 0.5 0.33 0.8
    }
    {
        clampmap gfx/misc/splash
        blendFunc GL_ONE GL_ONE
        rgbGen wave sawtooth 1 -1 0.66 1
        alphaGen const 0.6
        tcMod rotate -40
        tcMod stretch sawtooth 0.3 0.7 0.66 1
        tcMod turb 0.02 0.01 0 1
    }
}

models/map_objects/mp/flan
{
    q3map_nolightmap
    q3map_onlyvertexlighting
    cull    twosided
    {
        map models/map_objects/mp/flan
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
}

gfx/2d/numbers/zero
{
	nopicmip
	{
		map gfx/2d/numbers/zero
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/one
{
	nopicmip
	{
		map gfx/2d/numbers/one
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/two
{
	nopicmip
	{
		map gfx/2d/numbers/two
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/three
{
	nopicmip
	{
	map gfx/2d/numbers/three
	blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/four
{
	nopicmip
	{
		map gfx/2d/numbers/four
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/five
{
	nopicmip
	{
		map gfx/2d/numbers/five
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/six
{
	nopicmip
	{
		map gfx/2d/numbers/six
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/seven
{
	nopicmip
	{
		map gfx/2d/numbers/seven
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/eight
{
	nopicmip
	{
		map gfx/2d/numbers/eight
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

gfx/2d/numbers/nine
{
	nopicmip
	{
		map gfx/2d/numbers/nine
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}
