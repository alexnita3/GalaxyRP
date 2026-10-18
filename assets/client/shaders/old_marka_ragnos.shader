////ladder animado

models/players/old_marka_ragnos/icon_default
{
	qer_editorimage	gfx/ladder/icon_over
    {
        map gfx/ladder/black
        blendFunc GL_ONE GL_ZERO
        tcMod scale 8 1
    }
    {
        map gfx/ladder/over_energy
        blendFunc GL_ONE GL_ONE
        rgbGen wave sin 1 0.25 0 8
        tcMod scroll 5 0.1
        tcMod scale 1 1
    }
    {
        animMap 6.5 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/01 gfx/ladder/old_marka_ragnos/02 gfx/ladder/old_marka_ragnos/03 gfx/ladder/old_marka_ragnos/04 gfx/ladder/old_marka_ragnos/05 gfx/ladder/old_marka_ragnos/06 gfx/ladder/old_marka_ragnos/05 gfx/ladder/old_marka_ragnos/04 gfx/ladder/old_marka_ragnos/03 gfx/ladder/old_marka_ragnos/02 gfx/ladder/old_marka_ragnos/01 gfx/ladder/old_marka_ragnos/00 gfx/ladder/old_marka_ragnos/07 gfx/ladder/old_marka_ragnos/08 gfx/ladder/old_marka_ragnos/09 gfx/ladder/old_marka_ragnos/10 gfx/ladder/old_marka_ragnos/11 gfx/ladder/old_marka_ragnos/12 gfx/ladder/old_marka_ragnos/13 gfx/ladder/old_marka_ragnos/14 gfx/ladder/old_marka_ragnos/14 gfx/ladder/old_marka_ragnos/13 gfx/ladder/old_marka_ragnos/12 gfx/ladder/old_marka_ragnos/11 gfx/ladder/old_marka_ragnos/10 gfx/ladder/old_marka_ragnos/09 gfx/ladder/old_marka_ragnos/08 gfx/ladder/old_marka_ragnos/07
        blendFunc GL_ONE GL_ONE
    }
    {
        map gfx/ladder/icon_over
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}


///default


models/players/old_marka_ragnos/marka_head
{
	cull	twosided
    {
        map models/players/old_marka_ragnos/marka_head
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map gfx/effects/cm_baremetal
        blendFunc GL_DST_COLOR GL_SRC_COLOR
        tcGen environment
    }
    {
        map models/players/old_marka_ragnos/marka_head2
        alphaFunc GE128
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map models/players/old_marka_ragnos/marka_head_spec
        blendFunc GL_SRC_ALPHA GL_ONE
        detail
        alphaGen lightingSpecular
    }
}

models/players/old_marka_ragnos/marka_arms
{
	qer_editorimage	models/players/old_marka_ragnos/marka_head
	cull	twosided
    {
        map models/players/old_marka_ragnos/marka_arms
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map gfx/effects/cm_baremetal
        blendFunc GL_DST_COLOR GL_SRC_COLOR
        tcGen environment
    }
    {
        map models/players/old_marka_ragnos/marka_arms2
        alphaFunc GE128
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map models/players/old_marka_ragnos/marka_arms_spec
        blendFunc GL_SRC_ALPHA GL_ONE
        detail
        alphaGen lightingSpecular
    }
}


models/players/old_marka_ragnos/marka_torso
{
	qer_editorimage	models/players/old_marka_ragnos/marka_head
	cull	twosided
    {
        map models/players/old_marka_ragnos/marka_torso
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map gfx/effects/cm_baremetal
        blendFunc GL_DST_COLOR GL_SRC_COLOR
        tcGen environment
    }
    {
        map models/players/old_marka_ragnos/marka_torso2
        alphaFunc GE128
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map models/players/old_marka_ragnos/marka_torso_spec
        blendFunc GL_SRC_ALPHA GL_ONE
        detail
        alphaGen lightingSpecular
    }
}

models/players/old_marka_ragnos/marka_legs
{
	qer_editorimage	models/players/old_marka_ragnos/marka_head
	cull	twosided
    {
        map models/players/old_marka_ragnos/marka_legs
        blendFunc GL_ONE GL_ZERO
        rgbGen lightingDiffuse
    }
    {
        map models/players/old_marka_ragnos/marka_legs_spec
        blendFunc GL_SRC_ALPHA GL_ONE
        detail
        alphaGen lightingSpecular
    }
}
