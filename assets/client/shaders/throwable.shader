models/players/toss_core/core
{
    {
        map models/players/toss_core/core
        alphaFunc GE192
        rgbGen lightingDiffuse
    }
    {
        map models/players/toss_core/glow
        blendFunc GL_ONE GL_ONE
        glow
    }
    {
        map models/players/toss_core/spec
        blendFunc GL_SRC_ALPHA GL_ONE
        detail
        alphaGen lightingSpecular
    }
}

models/players/toss_emod/bomb_new_glow
{
    {
        map models/players/toss_emod/bomb_new_glow
        alphaFunc GE192
	glow
        rgbGen lightingDiffuse
    }
}

models/players/toss_statue01/env_crystal
{
    {
        map models/players/toss_statue01/env_crystal
        alphaFunc GE192
	glow
        rgbGen lightingDiffuse
    }
}