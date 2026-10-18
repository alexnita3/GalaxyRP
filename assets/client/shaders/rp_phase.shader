// GalaxyRP: [Phase] overlay passes for /admholo and /admghost.
//
// These are drawn by the client plugin as a second pass over a player's own model
// (CG_AddPhasedPlayerModel in cg_players.c). The first pass is the player's normal model with its
// own textures, recoloured and made see-through by the renderer's per-entity flags, which is what
// lets the effect work on any player model without a shader per model. What is left here are only
// the stages that do not depend on the model's texture.
//
// Hologram: scrolling scanlines projected along the model's own axes (tcGen vector, so the model's
// UVs do not matter) plus a flickering "broken camera" noise over it.
// Force ghost: a pulsing flat blue glow added on top.
//
// gfx/menus/scanlines and gfx/2d/brokencamera are base game textures; gfx/colors/blue_glow ships in
// this pk3.

gfx/effects/rp_holo_overlay
{
	{
		map gfx/menus/scanlines
		blendFunc GL_ONE GL_ONE
		tcGen vector ( 0.1 0.1 0 ) ( 0 0 0.1 )
		tcMod scroll 0 -0.5
		glow
	}
	{
		map gfx/2d/brokencamera
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		tcGen vector ( 0.5 0.5 0 ) ( 0 0 0.5 )
		tcMod turb 0 1 0 1
	}
}

gfx/effects/rp_ghost_overlay
{
	{
		map gfx/colors/blue_glow
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.9 0.1 0.1 0.1
	}
}
