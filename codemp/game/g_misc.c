/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// g_misc.c

#include "g_local.h"
#include "ghoul2/G2.h"

#include "ai_main.h" //for the g2animents

#define HOLOCRON_RESPAWN_TIME 30000
#define MAX_AMMO_GIVE 2
#define STATION_RECHARGE_TIME 100

void HolocronThink(gentity_t *ent);

/*QUAKED func_group (0 0 0) ?
Used to group brushes together just for editor convenience.  They are turned into normal brushes by the utilities.
*/
// GalaxyRP fix: [Entity Help] its own spawn function: it shared SP_info_null(), which keeps an info_null
// that has a targetname, so a func_group with one -- from an entity file, a preset, a sub-BSP or /entadd
// (the map compiler merges a map's own into the world) -- stayed as an invisible point in a networked slot.
// It is nothing in the game: it always removes itself, whatever its keys (/enthelp's "Map file only" group).
void SP_func_group( gentity_t *self ) {
	RP_SpawnSaysWhy( self, "func_group is a map-editor grouping, nothing in the game" );
	G_FreeEntity( self );
}


/*QUAKED info_camp (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_camp( gentity_t *self ) {
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_null( gentity_t *self ) {
	// GalaxyRP fix: [SP Maps] single player keeps an info_null around for a few frames so the
	// ref_tags, fx_runners, misc_weapon_shooters, spotlights and cameras that aim at one can
	// resolve their "target" when they link; freeing it on the spot left all of those with no
	// aim point (and a red "invalid target" line for every ref_tag). Here a targeted one is
	// kept for the whole map, inert: it is logical, so it costs no networked slot and no think,
	// and it survives /entsave and /entload -- a preset reloads the aim targets with the map
	// (an entity freed at spawn is never written to the preset). An info_null nothing can
	// target (no targetname) is still freed at once, as is every one once its region runs
	// low: a single-player map carries hundreds of them and G_SpawnLogical() drops the server
	// when the logical region is exhausted.
	if ( !self->targetname || !self->targetname[0]
		|| ( self->isLogical ? level.num_logicalents >= MAX_LOGICENTITIES - 256 : level.num_entities >= MAX_GENTITIES - 256 ) )
	{
		RP_SpawnSaysWhy( self, ( !self->targetname || !self->targetname[0] )
			? "an info_null needs a targetname: it is only an aim target for other entities"
			: "no room left in the entity table for an info_null" );
		G_FreeEntity( self );
		return;
	}
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
target_position does the same thing
*/
void SP_info_notnull( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED lightJunior (0 0.7 0.3) (-8 -8 -8) (8 8 8) nonlinear angle negative_spot negative_point
Non-displayed light that only affects dynamic game models, but does not contribute to lightmaps
"light" overrides the default 300 intensity.
Nonlinear checkbox gives inverse square falloff instead of linear
Angle adds light:surface angle calculations (only valid for "Linear" lights) (wolf)
Lights pointed at a target will be spotlights.
"radius" overrides the default 64 unit radius of a spotlight at the target point.
"fade" falloff/radius adjustment value. multiply the run of the slope by "fade" (1.0f default) (only valid for "Linear" lights) (wolf)
*/

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) linear noIncidence START_OFF
Non-displayed light.
"light" overrides the default 300 intensity. - affects size
a negative "light" will subtract the light's color
'Linear' checkbox gives linear falloff instead of inverse square
'noIncidence' checkbox makes lighting smoother
Lights pointed at a target will be spotlights.
"radius" overrides the default 64 unit radius of a spotlight at the target point.
"scale" multiplier for the light intensity - does not affect size (default 1)
		greater than 1 is brighter, between 0 and 1 is dimmer.
"color" sets the light's color
"targetname" to indicate a switchable light - NOTE that all lights with the same targetname will be grouped together and act as one light (ie: don't mix colors, styles or start_off flag)
"style" to specify a specify light style, even for switchable lights!
"style_off" light style to use when switched off (Only for switchable lights)

   1 FLICKER (first variety)
   2 SLOW STRONG PULSE
   3 CANDLE (first variety)
   4 FAST STROBE
   5 GENTLE PULSE 1
   6 FLICKER (second variety)
   7 CANDLE (second variety)
   8 CANDLE (third variety)
   9 SLOW STROBE (fourth variety)
   10 FLUORESCENT FLICKER
   11 SLOW PULSE NOT FADE TO BLACK
   12 FAST PULSE FOR JEREMY
   13 Test Blending
*/
static void misc_lightstyle_set ( gentity_t *ent)
{
	const int mLightStyle = ent->count;
	const int mLightSwitchStyle = ent->bounceCount;
	const int mLightOffStyle = ent->fly_sound_debounce_time;
	if (!ent->alt_fire)
	{	//turn off
		if (mLightOffStyle)	//i have a light style i'd like to use when off
		{
			char lightstyle[32];
			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightOffStyle*3)+0, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+0, lightstyle);

			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightOffStyle*3)+1, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+1, lightstyle);

			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightOffStyle*3)+2, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+2, lightstyle);
		}else
		{
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+0, "a");
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+1, "a");
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+2, "a");
		}
	}
	else
	{	//Turn myself on now
		if (mLightSwitchStyle)	//i have a light style i'd like to use when on
		{
			char lightstyle[32];
			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightSwitchStyle*3)+0, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+0, lightstyle);

			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightSwitchStyle*3)+1, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+1, lightstyle);

			trap->GetConfigstring(CS_LIGHT_STYLES + (mLightSwitchStyle*3)+2, lightstyle, 32);
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+2, lightstyle);
		}
		else
		{
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+0, "z");
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+1, "z");
			trap->SetConfigstring(CS_LIGHT_STYLES + (mLightStyle*3)+2, "z");
		}
	}
}

void misc_dlight_use ( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	G_ActivateBehavior(ent,BSET_USE);

	ent->alt_fire = !ent->alt_fire;	//toggle
	misc_lightstyle_set (ent);
}

void SP_light( gentity_t *self ) {
	if (!self->targetname )
	{//if i don't have a light style switch, the i go away
		RP_SpawnSaysWhy( self, "a light needs a targetname (it is a switchable light); for a plain light use rp_light" );
		G_FreeEntity( self );
		return;
	}

	G_SpawnInt( "style", "0", &self->count );
	G_SpawnInt( "switch_style", "0", &self->bounceCount );
	G_SpawnInt( "style_off", "0", &self->fly_sound_debounce_time );

	// GalaxyRP fix: [Entity System] misc_lightstyle_set() uses all three as configstring offsets,
	// CS_LIGHT_STYLES + style*3 + 0..2, with no bound. A value outside 0..MAX_LIGHT_STYLES-1 either
	// ran off the configstring table (the engine's "bad index" ERR_DROP, a process exit on a dedicated
	// server) or wrote into the neighbouring blocks for every client -- negative ones into the effect
	// names below CS_LIGHT_STYLES, 64 and up into CS_TERRAINS and CS_BSP_MODELS. The keys come
	// straight from /entadd, /entedit and /entload, so refuse the entity instead.
	if ( self->count < 0 || self->count >= MAX_LIGHT_STYLES ||
		self->bounceCount < 0 || self->bounceCount >= MAX_LIGHT_STYLES ||
		self->fly_sound_debounce_time < 0 || self->fly_sound_debounce_time >= MAX_LIGHT_STYLES )
	{
		G_LogPrintf( "light at %s: style %d, switch_style %d and style_off %d must all be 0 to %d; not spawned.\n",
			vtos( self->s.origin ), self->count, self->bounceCount, self->fly_sound_debounce_time, MAX_LIGHT_STYLES - 1 );
		G_FreeEntity( self );
		return;
	}

	// GalaxyRP fix: [Entity System] a light the Entity System made (/entadd, /entedit, a preset) must
	// use the switchable styles, 32 to 63. style defaults to 0 -- the style every ordinary surface's
	// lightmap is drawn with -- and styles 1 to 31 are the shared flicker patterns: a light added
	// with no style set style 0 to full bright for every client the moment it spawned, and to black
	// when it was used, and nothing brought it back. A map's own light is left to the map.
	if ( RP_EntitySystemMade( self ) &&
		( self->count < LS_SWITCH_START ||
		  ( self->bounceCount && self->bounceCount < LS_SWITCH_START ) ||
		  ( self->fly_sound_debounce_time && self->fly_sound_debounce_time < LS_SWITCH_START ) ) )
	{
		Q_strncpyz( level.rp_spawn_refusal, va( "a light added to the map needs a style from %d to %d (its switch_style and style_off too, when given); it shares the map's switchable light styles. For a plain light use rp_light",
			LS_SWITCH_START, MAX_LIGHT_STYLES - 1 ), sizeof( level.rp_spawn_refusal ) );
		G_LogPrintf( "light at %s: style %d, switch_style %d, style_off %d -- %s; not spawned.\n",
			vtos( self->s.origin ), self->count, self->bounceCount, self->fly_sound_debounce_time, level.rp_spawn_refusal );
		G_FreeEntity( self );
		return;
	}
	G_SetOrigin( self, self->s.origin );
	trap->LinkEntity( (sharedEntity_t *)self );

	self->use = misc_dlight_use;

	self->s.eType = ET_GENERAL;
	self->alt_fire = qfalse;
	self->r.svFlags |= SVF_NOCLIENT;

	if ( !(self->spawnflags & 4) )
	{	//turn myself on now
		self->alt_fire = qtrue;
	}
	misc_lightstyle_set (self);
}


/*
=================================================================================

TELEPORTERS

=================================================================================
*/

void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;
	qboolean	isNPC = qfalse;
	qboolean	noAngles;
	if (player->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	noAngles = (angles[0] > 999999.0) ? qtrue : qfalse;

	// GalaxyRP: [Grapple Hook] a teleport leaves the rope behind; the pull toward the old anchor
	// would otherwise drag the player straight back out of the destination.
	if ( player->client->hook )
	{
		Weapon_HookFree( player->client->hook );
	}

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap->UnlinkEntity ((sharedEntity_t *)player);

	VectorCopy ( origin, player->client->ps.origin );
	player->client->ps.origin[2] += 1;

	// spit the player out
	if ( !noAngles ) {
		AngleVectors( angles, player->client->ps.velocity, NULL, NULL );
		VectorScale( player->client->ps.velocity, 400, player->client->ps.velocity );
		player->client->ps.pm_time = 160;		// hold time
		player->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

		// set angles
		SetClientViewAngle( player, angles );
	}

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// kill anything at the destination
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		G_KillBox (player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );
	if (isNPC)
	{
		player->s.eType = ET_NPC;
	}

	// DAJ_RP: [Phase] /admhidden -- the copy above rebuilt s from ps
	RP_HiddenApplyState( player, &player->s );

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		trap->LinkEntity ((sharedEntity_t *)player);
	}
}


/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
Now that we don't have teleport destination pads, this is just
an info_notnull
*/
void SP_misc_teleporter_dest( gentity_t *ent ) {
}


//===========================================================

/*QUAKED misc_model (1 0 0) (-16 -16 -16) (16 16 16)
"model"		arbitrary .md3 or .ase file to display
turns into map triangles - not solid
*/
void SP_misc_model( gentity_t *ent ) {
	RP_SpawnSaysWhy( ent, "misc_model is a map-compile hint, baked into the map: use misc_model_breakable for a model" );

#if 0
	ent->s.modelindex = G_ModelIndex( ent->model );
	VectorSet (ent->r.mins, -16, -16, -16);
	VectorSet (ent->r.maxs, 16, 16, 16);
	trap->LinkEntity ((sharedEntity_t *)ent);

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
#else
	G_FreeEntity( ent );
#endif
}

/*QUAKED misc_model_static (1 0 0) (-16 -16 0) (16 16 16)
"model"		arbitrary .md3 file to display
"zoffset"	units to offset vertical culling position by, can be
			negative or positive. This does not affect the actual
			position of the model, only the culling position. Use
			it for models with stupid origins that go below the
			ground and whatnot.
"modelscale" scale on all axis
"modelscale_vec" scale difference axis

loaded as a model in the renderer - does not take up precious
bsp space!
*/
void SP_misc_model_static(gentity_t *ent)
{
	// DAJ_RP: [Static Models] the map's own are noted first, for Entity Bounds and /entcopystatic (g_statics.c)
	RP_StaticsRecord();

	RP_SpawnSaysWhy( ent, "a misc_model_static is drawn by each client from its own copy of the map file, so one added on the server is never seen: use misc_model_breakable for a model" );
	G_FreeEntity( ent );
}

/*QUAKED rp_light (1 1 0) (-8 -8 -8) (8 8 8) START_OFF
GalaxyRP: [Entity System] a light and nothing else: a dynamic light at its origin, drawn by every
client, with no model. Unlike "light", which the map compiler bakes into the map, this one exists at
run time, so the Entity System can add it anywhere. Using it (targetname) switches it between its on
and off lights.

"light"		radius when on (default 300)
"color"		red green blue from 0 to 1 when on (default 1 1 1, white)
"offlight"	radius when off (default 0: dark)
"offcolor"	colour when off (default white)
START_OFF	starts switched off
*/
static void rp_light_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	self->count = self->count ? 0 : 1;
	self->s.constantLight = self->count ? self->genericValue1 : self->genericValue2;
}

void SP_rp_light( gentity_t *ent )
{
	float light, offLight;
	vec3_t color, offColor;

	G_SpawnFloat( "light", "300", &light );
	G_SpawnVector( "color", "1 1 1", color );
	G_SpawnFloat( "offlight", "0", &offLight );
	G_SpawnVector( "offcolor", "1 1 1", offColor );

	ent->genericValue1 = RP_PackConstantLight( light, color );
	ent->genericValue2 = ( offLight > 0.0f ) ? RP_PackConstantLight( offLight, offColor ) : 0;
	ent->count = ( ent->spawnflags & 1 ) ? 0 : 1;
	ent->s.constantLight = ent->count ? ent->genericValue1 : ent->genericValue2;

	// zyk: no model, nothing to collide with; the small box is where it is sent to players from, and
	// what the Entity Bounds display and the entity commands aim at
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->r.contents = 0;
	VectorSet( ent->r.mins, -8, -8, -8 );
	VectorSet( ent->r.maxs, 8, 8, 8 );
	ent->use = rp_light_use;

	G_SetOrigin( ent, ent->s.origin );
	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*QUAKED misc_model_breakable (1 0 0) (-16 -16 -16) (16 16 16) SOLID AUTOANIMATE DEADSOLID NO_DMODEL NO_SMOKE USE_MODEL USE_NOT_BREAK PLAYER_USE NO_EXPLOSION
SOLID - Movement is blocked by it, if not set, can still be broken by explosions and shots if it has health
AUTOANIMATE - Will cycle it's anim
DEADSOLID - Stay solid even when destroyed (in case damage model is rather large).
NO_DMODEL - Makes it NOT display a damage model when destroyed, even if one exists
USE_MODEL - When used, will toggle to it's usemodel (model name + "_u1.md3")... this obviously does nothing if USE_NOT_BREAK is not checked
USE_NOT_BREAK - Using it, doesn't make it break, still can be destroyed by damage
PLAYER_USE - Player can use it with the use button
NO_EXPLOSION - By default, will explode when it dies...this is your override.

"model"		arbitrary .md3 file to display
"health"	how much health to have - default is zero (not breakable)  If you don't set the SOLID flag, but give it health, it can be shot but will not block NPCs or players from moving
"targetname" when used, dies and displays damagemodel, if any (if not, removes itself)
"target" What to use when it dies
"target2" What to use when it's repaired
"target3" What to use when it's used while it's broken
"paintarget" target to fire when hit (but not destroyed)
"count"  the amount of armor/health/ammo given (default 50)
"gravity"	if set to 1, this will be affected by gravity
"radius"  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks

Damage: default is none
"splashDamage" - damage to do (will make it explode on death)
"splashRadius" - radius for above damage

"team" - This cannot take damage from members of this team:
	"player"
	"neutral"
	"enemy"

"material" - default is "8 - MAT_NONE" - choose from this list:
0 = MAT_METAL		(grey metal)
1 = MAT_GLASS
2 = MAT_ELECTRICAL	(sparks only)
3 = MAT_ELEC_METAL	(METAL chunks and sparks)
4 =	MAT_DRK_STONE	(brown stone chunks)
5 =	MAT_LT_STONE	(tan stone chunks)
6 =	MAT_GLASS_METAL (glass and METAL chunks)
7 = MAT_METAL2		(blue/grey metal)
8 = MAT_NONE		(no chunks-DEFAULT)
9 = MAT_GREY_STONE	(grey colored stone)
10 = MAT_METAL3		(METAL and METAL2 chunk combo)
11 = MAT_CRATE1		(yellow multi-colored crate chunks)
12 = MAT_GRATE1		(grate chunks--looks horrible right now)
13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
14 = MAT_CRATE2		(red multi-colored crate chunks)
15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
FIXME/TODO:
set size better?
multiple damage models?
custom explosion effect/sound?
*/
void misc_model_breakable_gravity_init( gentity_t *ent, qboolean dropToFloor );
void misc_model_breakable_init( gentity_t *ent );

// GalaxyRP: [Entity System] the axis-aligned box that holds this one turned by angles -- a turned
// model still collides as an axis-aligned box, so this is the box that covers it
static void RP_EncloseTurnedBox( vec3_t mins, vec3_t maxs, const vec3_t angles )
{
	matrix3_t axis;
	vec3_t lo, hi;
	int i, k;

	if ( !angles[0] && !angles[1] && !angles[2] )
		return;

	// zyk: a box that is not a box (a NaN scale made it) is left as it is
	for ( k = 0; k < 3; k++ )
	{
		if ( !( mins[k] <= maxs[k] ) )
			return;
	}

	AnglesToAxis( angles, axis );
	VectorSet( lo, 999999, 999999, 999999 );
	VectorSet( hi, -999999, -999999, -999999 );

	for ( i = 0; i < 8; i++ )
	{
		vec3_t local, p;

		local[0] = ( i & 1 ) ? maxs[0] : mins[0];
		local[1] = ( i & 2 ) ? maxs[1] : mins[1];
		local[2] = ( i & 4 ) ? maxs[2] : mins[2];

		for ( k = 0; k < 3; k++ )
		{
			p[k] = local[0] * axis[0][k] + local[1] * axis[1][k] + local[2] * axis[2][k];
			if ( p[k] < lo[k] ) lo[k] = p[k];
			if ( p[k] > hi[k] ) hi[k] = p[k];
		}
	}

	VectorCopy( lo, mins );
	VectorCopy( hi, maxs );
}

/*
GalaxyRP: [Entity System] additions to misc_model_breakable:

  "light" / "color"  a light on the model, as movers have them: "light" is the radius (default 100 when
                     only "color" is given), "color" red green blue from 0 to 1 (default white). No light
                     without either key; it goes out when the model is destroyed.
  "modelscale"       now scales the model drawn as well as its box, as in single player -- through
                     entityState_t::iModelScale, so 0.01 to 10.23. "zykmodelscale" (a percentage), if
                     given, still decides the drawn size; "modelscale_vec" scales the box per axis, and
                     DAJ_RP: [Static Models] the GalaxyRP client draws the model so too (EF2_RP_VECTOR_SCALE,
                     the scales in entityState_t::origin2); other clients draw it unscaled, since the
                     protocol's own scale is one number for every axis.
  AUTOANIMATE (2)    a model with several frames plays them over and over (RP_Animate, one frame per
                     server frame), and stops when destroyed.

And for one the Entity System made (RP_EntitySystemMade: /entadd, /entaddaim, copies, entity files --
never the map's own): with no "mins"/"maxs" (and not spawnflag 65536), its box is the model's own
frame-0 bounds from the md3 file, scaled and turned with it, rather than a 32-unit cube; and its damage
and use models are only registered when those files exist. (A modelscale still lifts the origin by
single player's amount, which keeps the box's bottom where it was; the entity commands that place a
model allow for it -- RP_EntGrabScaleShift() in g_entgrab.c.)
*/
void SP_misc_model_breakable( gentity_t *ent )
{
	char	damageModel[MAX_QPATH];
	char	useModel[MAX_QPATH];
	int		len;
	float grav = 0;
	qboolean bHasScale = qfalse;
	qboolean vecScale = qfalse;
	qboolean esMade = RP_EntitySystemMade( ent );
	qboolean boxGiven = qfalse;
	qboolean autoBox = qfalse;
	float uniformScale = 0.0f;
	int modelFrames = 0;
	vec3_t modelMins, modelMaxs;
	qboolean haveModelInfo = qfalse;
	
	// Chris F. requested default for misc_model_breakable to be NONE...so don't arbitrarily change this.
	G_SpawnInt( "material", "8", (int*)&ent->material );
	G_SpawnFloat( "radius", "1", &ent->radius ); // used to scale chunk code if desired by a designer
	bHasScale = G_SpawnVector("modelscale_vec", "0 0 0", ent->modelScale);
	vecScale = bHasScale;	// DAJ_RP: [Static Models] for EF2_RP_VECTOR_SCALE below

	// zyk: now the size is set correctly
	if (Q_stricmp(ent->targetname,"zyk_ice_boulder") == 0)
	{
		G_SpawnVector("mins", "-60 -60 -20", ent->r.mins);
		G_SpawnVector("maxs", "60 60 42", ent->r.maxs);
		boxGiven = qtrue;
	}
	else if (Q_stricmp(ent->targetname, "zyk_tree_of_life") == 0)
	{
		G_SpawnVector("mins", "-70 -70 -400", ent->r.mins);
		G_SpawnVector("maxs", "70 70 250", ent->r.maxs);
		boxGiven = qtrue;
	}
	else
	{
		if (!(ent->spawnflags & 65536))
		{ // zyk: do not set default mins and maxs if this spawnflag is set
			if (G_SpawnVector("mins", "-16 -16 -16", ent->r.mins))
				boxGiven = qtrue;
			if (G_SpawnVector("maxs", "16 16 16", ent->r.maxs))
				boxGiven = qtrue;
		}
		else
		{
			boxGiven = qtrue;
		}
	}

	// GalaxyRP: [Entity System] the model's own box, for an entity the Entity System made
	autoBox = (esMade && !boxGiven) ? qtrue : qfalse;

	if (!bHasScale)
	{
		float temp;
		G_SpawnFloat( "modelscale", "0", &temp);
		if (temp != 0.0f)
		{
			ent->modelScale[ 0 ] = ent->modelScale[ 1 ] = ent->modelScale[ 2 ] = temp;
			bHasScale = qtrue;
			uniformScale = temp;
		}
	}

	if (!ent->model)
	{ // zyk: must have a model
		Com_Printf(S_COLOR_RED"ERROR: misc_model_breakable at %s has no md3 model specified\n", vtos(ent->s.origin));

		ent->think = G_FreeEntity;
		ent->nextthink = level.time + FRAMETIME;

		return;
	}

	// GalaxyRP fix: [Entity System] three buffer defects lived in the next few lines, all reachable
	// from an /entadd, an /entload preset or a map's own entity string, because ent->model is an
	// F_STRING parsed straight out of the spawn vars and is bounded only by MAX_STRING_CHARS.
	//
	//  1. len was strlen(ent->model) - 4 with no minimum. A model name of one to three characters
	//     made len negative, so ent->model[len] read before the string and damageModel[len] = 0
	//     wrote before a 64-byte stack array.
	//  2. Nothing checked len against the buffer either. strncpy() fills MAX_QPATH bytes without
	//     terminating, and damageModel[len] = 0 then wrote the terminator at an offset taken
	//     straight from the model name -- a ~900 character name put it ~900 bytes past the end.
	//  3. The strcat()s below appended seven more characters onto buffers strncpy() may have left
	//     unterminated, so even a legal 60-character name overflowed.
	//
	// The length is validated first, the copies are bounded and always terminated, and the
	// extensions are appended with Q_strcat. Which models are accepted does not change: a name
	// still has to end in a four-character ".md3" extension to get past here.
	len = strlen(ent->model) - 4;

	if (len < 1 || len >= (int)sizeof(damageModel) || ent->model[len] != '.') //we're expecting ".md3"
	{ // zyk: if model is not md3, do not spawn the entity
		Com_Printf(S_COLOR_RED"ERROR: misc_model_breakable at %s has no md3 path specified\n", vtos(ent->s.origin));

		ent->think = G_FreeEntity;
		ent->nextthink = level.time + FRAMETIME;

		return;
	}

	misc_model_breakable_init( ent );

	// GalaxyRP: [Entity System] what the md3 file holds, when something here needs it: the model's own
	// box, or its frame count for AUTOANIMATE
	if (autoBox || (ent->spawnflags & 2))
	{
		haveModelInfo = RP_ModelInfo(ent->s.modelindex, ent->model, &modelFrames, modelMins, modelMaxs);
	}

	if (autoBox && haveModelInfo)
	{
		VectorCopy(modelMins, ent->r.mins);
		VectorCopy(modelMaxs, ent->r.maxs);
	}
	else
	{
		autoBox = qfalse;
	}

	Q_strncpyz( damageModel, ent->model, sizeof(damageModel) );
	damageModel[len] = 0;	//chop extension
	Q_strncpyz( useModel, damageModel, sizeof(useModel));

	// GalaxyRP: [Entity System] no longer registers a model file the server does not have, for an
	// entity the Entity System made -- see RP_EntitySystemSpawnRefused() in g_spawn.c
	ent->s.modelindex2 = 0;
	ent->sound1to2 = 0;

	if (ent->takedamage) {
		//Dead/damaged model
		if( !(ent->spawnflags & 8) ) {	//no dmodel
			Q_strcat( damageModel, sizeof(damageModel), "_d1.md3" );
			if (!esMade || RP_FileExists(damageModel))
				ent->s.modelindex2 = RP_EntityModelIndex( ent, damageModel );
		}

		// GalaxyRP fix: [SP Maps] the singleplayer "_c1.md3" chunk model used to be registered here
		// and stored in s.modelGhoul2. In multiplayer that field is not a model slot: any non-zero
		// value tells cgame (CG_General) that the entity is a Ghoul2 model, so every damageable
		// breakable had the client try to build a Ghoul2 instance out of its .md3 -- and a chunk
		// index that happened to be 127 hid the entity entirely ("not ready to be drawn"). It also
		// took a model configstring for a file that almost never exists. Vanilla MP never sets it for
		// this class, and New Zyk mod has since commented it out as not working in MP. With it gone
		// the destruction debris in misc_model_breakable_die() is the material chunks, as in vanilla.
	}

	//Use model
	if( ent->spawnflags & 32 ) {	//has umodel
		Q_strcat( useModel, sizeof(useModel), "_u1.md3" );
		if (!esMade || RP_FileExists(useModel))
			ent->sound1to2 = RP_EntityModelIndex( ent, useModel );
	}

	// Scale up the tie-bomber bbox a little.
	if ( ent->model && Q_stricmp( "models/map_objects/ships/tie_bomber.md3", ent->model ) == 0 )
	{
		VectorSet (ent->r.mins, -80, -80, -80);
		VectorSet (ent->r.maxs, 80, 80, 80); 
		autoBox = qfalse;

		//ent->s.modelScale[ 0 ] = ent->s.modelScale[ 1 ] = ent->s.modelScale[ 2 ] *= 2.0f;
		//bHasScale = qtrue;
	}

	if (bHasScale)
	{
		float oldMins2 = 0;
		//scale the x axis of the bbox up.
		ent->r.maxs[0] *= ent->modelScale[0];//*scaleFactor;
		ent->r.mins[0] *= ent->modelScale[0];//*scaleFactor;
		
		//scale the y axis of the bbox up.
		ent->r.maxs[1] *= ent->modelScale[1];//*scaleFactor;
		ent->r.mins[1] *= ent->modelScale[1];//*scaleFactor;
		
		//scale the z axis of the bbox up and adjust origin accordingly
		ent->r.maxs[2] *= ent->modelScale[2];
		oldMins2 = ent->r.mins[2];
		ent->r.mins[2] *= ent->modelScale[2];
		ent->s.origin[2] += (oldMins2-ent->r.mins[2]);
	}

	// GalaxyRP: [Entity System] the model's own box, turned with the model
	if (autoBox)
	{
		RP_EncloseTurnedBox(ent->r.mins, ent->r.maxs, ent->s.angles);
	}

	// GalaxyRP: [Entity System] the drawn size and the light. Only when spawning from key/value pairs:
	// the game's own models (the duel and melee arenas) are set up field by field with no pairs, and
	// their "zykmodelscale" is already in iModelScale -- nothing here may touch it. With pairs, keys
	// that are gone must be undone too, since /entedit respawns an entity without clearing it.
	if (level.numSpawnVars > 0)
	{
		int percent = 0;
		float light = 100.0f;
		vec3_t color;
		qboolean lightSet, colorSet;
		qboolean percentGiven = G_SpawnInt("zykmodelscale", "0", &percent);

		if (!percentGiven)
		{
			if (uniformScale > 0.0f)
			{
				percent = (int)(uniformScale * 100.0f + 0.5f);
				if (percent < 1)
					percent = 1;
				if (percent > 1023)
					percent = 1023;
				ent->s.iModelScale = percent;
			}
			else
			{
				ent->s.iModelScale = 0;
			}
		}

		lightSet = G_SpawnFloat("light", "100", &light);
		colorSet = G_SpawnVector("color", "1 1 1", color);
		ent->s.constantLight = (lightSet || colorSet) ? RP_PackConstantLight(light, color) : 0;

		// DAJ_RP: [Static Models] "modelscale_vec" drawn per axis by the GalaxyRP client: the three scales in
		// origin2, flagged (EF2_RP_VECTOR_SCALE, bg_public.h); iModelScale stays 0, so every other client
		// draws the model unscaled as before. "zykmodelscale", when given, decides the drawn size instead.
		// Undone when the key is gone, as above: /entedit spawns the entity again without clearing it.
		// Never once a script has played a ROFF on it (roffid, set only by Q3_Play and cleared only when the
		// entity is freed): origin2 is the ROFF's then -- where a "loop" notetrack puts it back -- and a
		// respawn (/entedit, /entrotate, a dropped /entcut) must not overwrite it, so it is drawn unscaled.
		if (vecScale && !percentGiven && !ent->roffid && ent->modelScale[0] > 0.0f && ent->modelScale[1] > 0.0f && ent->modelScale[2] > 0.0f)
		{
			ent->s.eFlags2 |= EF2_RP_VECTOR_SCALE;
			VectorCopy(ent->modelScale, ent->s.origin2);
		}
		else if (ent->s.eFlags2 & EF2_RP_VECTOR_SCALE)
		{
			ent->s.eFlags2 &= ~EF2_RP_VECTOR_SCALE;
			VectorClear(ent->s.origin2);
		}
	}

	// GalaxyRP: [Entity System] AUTOANIMATE: play the model's frames over and over
	if ((ent->spawnflags & 2) && haveModelInfo && modelFrames > 1)
	{
		ent->startFrame = 0;
		ent->endFrame = modelFrames - 1;
		ent->loopAnim = qtrue;
		ent->rpAnimating = qtrue;
		ent->rpAutoAnimate = qtrue;
		ent->s.frame = 0;
	}
	else if (ent->rpAutoAnimate)
	{
		ent->rpAnimating = qfalse;
		ent->rpAutoAnimate = qfalse;
		ent->s.frame = 0;
	}

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );
	trap->LinkEntity ((sharedEntity_t *)ent);

	if ( ent->spawnflags & 128 )
	{//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}
	
	ent->team = NULL;

	G_SpawnFloat( "gravity", "0", &grav );
	if ( grav )
	{//affected by gravity
		G_SetAngles( ent, ent->s.angles );
		G_SetOrigin( ent, ent->r.currentOrigin );
		G_SpawnString( "throwtarget", NULL, &ent->target4 ); // used to throw itself at something
		misc_model_breakable_gravity_init( ent, qtrue );
	}

	// Start off.
	if ( ent->spawnflags & 4096 )
	{
		ent->s.solid = 0;
		ent->r.contents = 0;
		ent->clipmask = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		ent->count = 0;
	}
}

void misc_model_breakable_gravity_init( gentity_t *ent, qboolean dropToFloor )
{
	trace_t		tr;
	vec3_t		top, bottom;

	ent->s.eType = ET_GENERAL;
	//ent->s.eFlags |= EF_BOUNCE_HALF;	// FIXME
	ent->clipmask = MASK_SOLID|CONTENTS_BODY|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP;//?
	ent->physicsBounce = ent->mass = VectorLength( ent->r.maxs ) + VectorLength( ent->r.mins );

	//drop to floor

	if ( dropToFloor )
	{
		VectorCopy( ent->r.currentOrigin, top );
		top[2] += 1;
		VectorCopy( ent->r.currentOrigin, bottom );
		bottom[2] = MIN_WORLD_COORD;
		trap->Trace( &tr, top, ent->r.mins, ent->r.maxs, bottom, ent->s.number, MASK_NPCSOLID, qfalse, 0, 0 );
		if ( !tr.allsolid && !tr.startsolid && tr.fraction < 1.0 )
		{
			G_SetOrigin( ent, tr.endpos );
			trap->LinkEntity( (sharedEntity_t *)ent );
		}
	}
	else
	{
		G_SetOrigin( ent, ent->r.currentOrigin );
		trap->LinkEntity( (sharedEntity_t *)ent );
	}
	//set up for object thinking
	if ( VectorCompare( ent->s.pos.trDelta, vec3_origin ) )
	{//not moving
		ent->s.pos.trType = TR_STATIONARY;
	}
	else
	{
		ent->s.pos.trType = TR_GRAVITY;
	}
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	VectorClear( ent->s.pos.trDelta );
	ent->s.pos.trTime = level.time;
	if ( VectorCompare( ent->s.apos.trDelta, vec3_origin ) )
	{//not moving
		ent->s.apos.trType = TR_STATIONARY;
	}
	else
	{
		ent->s.apos.trType = TR_LINEAR;
	}
	VectorCopy( ent->r.currentAngles, ent->s.apos.trBase );
	VectorClear( ent->s.apos.trDelta );
	ent->s.apos.trTime = level.time;
}

extern void G_MiscModelExplosion( vec3_t mins, vec3_t maxs, int size, material_t chunkType );
void misc_model_breakable_pain ( gentity_t *self, gentity_t *other, int damage )
{
	if ( self->health > 0 )
	{
		// still alive, react to the pain
		if ( self->paintarget )
		{
			G_UseTargets2 (self, self->activator, self->paintarget);
		}

		// Don't do script if dead
		G_ActivateBehavior( self, BSET_PAIN );
	}
}

void G_Chunks( int owner, vec3_t origin, const vec3_t normal, const vec3_t mins, const vec3_t maxs,
						float speed, int numChunks, material_t chunkType, int customChunk, float baseScale );

void misc_model_breakable_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath) 
{
	int		numChunks;
	float	size = 0, scale;
	vec3_t	dir, up, dis;

	if (self->die == NULL)	//i was probably already killed since my die func was removed
	{
#ifndef FINAL_BUILD
		G_Printf(S_COLOR_YELLOW"Recursive misc_model_breakable_die.  Use targets probably pointing back at self.\n");
#endif
		return;	//this happens when you have a cyclic target chain!
	}
	//NOTE: Stop any scripts that are currently running (FLUSH)... ?
	//Turn off animation
	// GalaxyRP: [Entity System] and the light (see SP_misc_model_breakable)
	if ( self->rpAutoAnimate )
		self->s.frame = 0;	// the damage model has frames of its own
	self->rpAnimating = qfalse;
	self->rpAutoAnimate = qfalse;
	self->s.constantLight = 0;

	self->health = 0;
	//Throw some chunks
	AngleVectors( self->s.apos.trBase, dir, NULL, NULL );
	VectorNormalize( dir );

	numChunks = Q_flrand(0.0f, 1.0f) * 6 + 20;

	VectorSubtract( self->r.absmax, self->r.absmin, dis );

	// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
	// Volume is length * width * height...then break that volume down based on how many chunks we have
	scale = sqrt( sqrt( dis[0] * dis[1] * dis[2] )) * 1.75f;

	if ( scale > 48 )
	{
		size = 2;
	}
	else if ( scale > 24 )
	{
		size = 1;
	}

	scale = scale / numChunks;

	if ( self->radius > 0.0f )
	{
		// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
		//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
		numChunks *= self->radius;
	}

	VectorAdd( self->r.absmax, self->r.absmin, dis );
	VectorScale( dis, 0.5f, dis );

	G_Chunks( self->s.number, dis, dir, self->r.absmin, self->r.absmax, 300, numChunks, self->material, self->s.modelGhoul2, scale );

	self->pain = 0;
	self->die  = 0;
//	self->e_UseFunc  = useF_NULL;

	self->takedamage = qfalse;

	if ( !(self->spawnflags & 4) )
	{//We don't want to stay solid
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;

		trap->LinkEntity((sharedEntity_t *)self);
	}

	VectorSet(up, 0, 0, 1);

	if(self->target)
	{
		G_UseTargets(self, attacker);
	}

	if(inflictor->client)
	{
		VectorSubtract( self->r.currentOrigin, inflictor->r.currentOrigin, dir );
		VectorNormalize( dir );
	}
	else
	{
		VectorCopy(up, dir);
	}

	if ( !(self->spawnflags & 2048) ) // NO_EXPLOSION
	{
		// Ok, we are allowed to explode, so do it now!
		if(self->splashDamage > 0 && self->splashRadius > 0)
		{//explode
			vec3_t org;
			AddSightEvent( attacker, self->r.currentOrigin, 256, AEL_DISCOVERED, 100 );
			AddSoundEvent( attacker, self->r.currentOrigin, 128, AEL_DISCOVERED, qfalse );//FIXME: am I on ground or not?
			//FIXME: specify type of explosion?  (barrel, electrical, etc.)  Also, maybe just use the explosion effect below since it's
			//				a bit better?
			// up the origin a little for the damage check, because several models have their origin on the ground, so they don't alwasy do damage, not the optimal solution...
			VectorCopy( self->r.currentOrigin, org );
			if ( self->r.mins[2] > -4 )
			{//origin is going to be below it or very very low in the model
				//center the origin
				org[2] = self->r.currentOrigin[2] + self->r.mins[2] + (self->r.maxs[2] - self->r.mins[2])/2.0f;
			}
			G_RadiusDamage( org, self, self->splashDamage, self->splashRadius, self, NULL, MOD_UNKNOWN );

			G_MiscModelExplosion( self->r.absmin, self->r.absmax, size, self->material );
			G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/explosions/cargoexplode.wav") );
			self->s.loopSound = 0;
		}
		else
		{//just break
			AddSightEvent( attacker, self->r.currentOrigin, 128, AEL_DISCOVERED, qfalse );
			AddSoundEvent( attacker, self->r.currentOrigin, 64, AEL_SUSPICIOUS, qfalse );//FIXME: am I on ground or not?
			// This is the default explosion
			G_MiscModelExplosion( self->r.absmin, self->r.absmax, size, self->material );
			G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/weapons/explosions/cargoexplode.wav"));
		}
	}

	self->think = 0;
	self->nextthink = -1;

	// GalaxyRP: [Entity System] was "!= -1", which modelindex2 never is: with no damage model (0 --
	// its file was not on the server, see SP_misc_model_breakable) the broken model became no model
	// and stayed as an invisible entity. It goes instead, as the FIXME below wanted.
	if(self->s.modelindex2 > 0 && !(self->spawnflags & 8))
	{//FIXME: modelindex doesn't get set to -1 if the damage model doesn't exist
		self->s.modelindex = self->s.modelindex2;
		G_ActivateBehavior( self, BSET_DEATH );
	}
	else
	{
		G_FreeEntity( self );
	}
}


/*
=================
misc_model_cargo_small

DAJ_RP: [SP Maps] single player's small cargo crate (Jedi Outcast's kejim_base and kejim_post place it), which
had no spawn function in MP, so those maps came up without their crates. A misc_model_breakable with single
player's settings: solid, no damage model, 25 health (an explicit "health" 0 leaves it unbreakable), crate
debris (material 11) a half as many again (radius 1.5), a small splash when it goes (splashRadius 96,
splashDamage 1), each only when its key does not say otherwise, and kejim/cargo_small.md3 for a model. Only
heavy weapons hurt it (FL_DMG_BY_HEAVY_WEAP_ONLY: rockets, explosives, mines, repeater alt fire, vehicles...).
When it breaks it drops what its spawnflags say, around where it stood:

  MEDPACK (1)    item_medpak_instant
  SHIELDS (2)    item_shield_sm_instant
  BACTA (4)      item_medpac, MP's bacta canister
  BATTERIES (8)  nothing -- MP has no battery item

The spawnflags are those four only: as breakable flags they would mean something else (8 is "no damage
model"), and single player's difficulty bits, which the kejim maps set too (512, 1024), mean nothing here.
The record keeps them as the map or the admin wrote them.
=================
*/
#define RP_CARGO_MEDPACK	1
#define RP_CARGO_SHIELDS	2
#define RP_CARGO_BACTA		4
#define RP_CARGO_BATTERIES	8

// the item a crate drops for that flag -- precached for clients now, as a G_SpawnItem() after map load does
static gitem_t *RP_CargoDropItem( const char *classname )
{
	gitem_t *item = BG_FindItem( classname );

	if ( !item )
	{
		return NULL;
	}

	if ( level.rp_map_loaded )
	{
		char items[MAX_STRING_CHARS];
		const int index = (int)( item - bg_itemlist );

		trap->GetConfigstring( CS_ITEMS, items, sizeof( items ) );
		if ( index >= 0 && index < bg_numItems && ( index >= (int)strlen( items ) || items[index] != '1' ) )
		{
			RegisterItem( item );
			SaveRegisteredItems();
			G_LogPrintf( "item %s precached after map load (CS_ITEMS rewritten)\n", item->classname );
		}
	}

	RegisterItem( item );
	return item;
}

static void RP_CargoLaunch( const char *classname, const vec3_t org, float x, float y )
{
	gitem_t *item = BG_FindItem( classname );
	vec3_t where, still;

	if ( !item )
	{
		return;
	}

	VectorSet( where, org[0] + Q_flrand( -1.0f, 1.0f ) * 8 + x, org[1] + Q_flrand( -1.0f, 1.0f ) * 8 + y, org[2] + 16 );
	VectorClear( still );
	LaunchItem( item, where, still );
}

void misc_model_cargo_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	const int flags = self->genericValue11;
	vec3_t org;

	// copied first: with no damage model the breakable is freed as it dies
	VectorCopy( self->r.currentOrigin, org );

	misc_model_breakable_die( self, inflictor, attacker, damage, meansOfDeath );

	// each in its own quadrant, so they do not land on top of each other
	if ( flags & RP_CARGO_MEDPACK )
	{
		RP_CargoLaunch( "item_medpak_instant", org, 16, 16 );
	}
	if ( flags & RP_CARGO_SHIELDS )
	{
		RP_CargoLaunch( "item_shield_sm_instant", org, -16, 16 );
	}
	if ( flags & RP_CARGO_BACTA )
	{
		RP_CargoLaunch( "item_medpac", org, -16, -16 );
	}
}

void SP_misc_model_cargo_small( gentity_t *ent )
{
	const int flags = ent->spawnflags;
	int material = 0;

	ent->genericValue11 = flags & ( RP_CARGO_MEDPACK | RP_CARGO_SHIELDS | RP_CARGO_BACTA );

	if ( flags & RP_CARGO_MEDPACK )
	{
		RP_CargoDropItem( "item_medpak_instant" );
	}
	if ( flags & RP_CARGO_SHIELDS )
	{
		RP_CargoDropItem( "item_shield_sm_instant" );
	}
	if ( flags & RP_CARGO_BACTA )
	{
		RP_CargoDropItem( "item_medpac" );
	}

	G_SpawnInt( "health", "25", &ent->health );
	G_SpawnInt( "splashRadius", "96", &ent->splashRadius );
	G_SpawnInt( "splashDamage", "1", &ent->splashDamage );

	// its key's .md3 if the server has it, else the crate -- RP_DispenserModel() in g_spawn.c
	RP_DispenserModel( ent, RP_MODEL_CARGO_SMALL );

	ent->spawnflags = 1 | 8;	// SOLID, NO_DMODEL

	SP_misc_model_breakable( ent );

	if ( !ent->inuse || ent->think == G_FreeEntity )
	{
		return;
	}

	// read again after the breakable's own defaults (material 8, radius 1)
	G_SpawnInt( "material", "11", &material );
	ent->material = (material_t)material;
	G_SpawnFloat( "radius", "1.5", &ent->radius );

	ent->flags |= FL_DMG_BY_HEAVY_WEAP_ONLY;
	if ( ent->takedamage )
	{
		ent->die = misc_model_cargo_die;
	}
}

void misc_model_throw_at_target4( gentity_t *self, gentity_t *activator )
{
	vec3_t	pushDir, kvel;
	float	knockback = 200;
	float	mass = self->mass;
	gentity_t *target = G_Find( NULL, FOFS(targetname), self->target4 );
	if ( !target )
	{//nothing to throw ourselves at...
		return;
	}
	VectorSubtract( target->r.currentOrigin, self->r.currentOrigin, pushDir );
	knockback -= VectorNormalize( pushDir );
	if ( knockback < 100 )
	{
		knockback = 100;
	}
	VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
	self->s.pos.trTime = level.time;								// move a bit on the very first frame
	if ( self->s.pos.trType != TR_INTERPOLATE )
	{//don't do this to rolling missiles
		self->s.pos.trType = TR_GRAVITY;
	}

	if ( mass < 50 )
	{//???
		mass = 50;
	}

	if ( g_gravity.value > 0 )
	{
		VectorScale( pushDir, g_knockback.value * knockback / mass * 0.8, kvel );
		kvel[2] = pushDir[2] * g_knockback.value * knockback / mass * 1.5;
	}
	else
	{
		VectorScale( pushDir, g_knockback.value * knockback / mass, kvel );
	}

	VectorAdd( self->s.pos.trDelta, kvel, self->s.pos.trDelta );
	if ( g_gravity.value > 0 )
	{
		if ( self->s.pos.trDelta[2] < knockback )
		{
			self->s.pos.trDelta[2] = knockback;
		}
	}
	//no trDuration?
	if ( self->think != G_RunObject )
	{//objects spin themselves?
		//spin it
		//FIXME: messing with roll ruins the rotational center???
		self->s.apos.trTime = level.time;
		self->s.apos.trType = TR_LINEAR;
		VectorClear( self->s.apos.trDelta );
		self->s.apos.trDelta[1] = Q_irand( -800, 800 );
	}
}

void misc_model_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if ( self->target4 )
	{//throw me at my target!
		misc_model_throw_at_target4( self, activator );
		return;
	}

	if ( self->health <= 0 && self->maxHealth > 0)
	{//used while broken fired target3
		G_UseTargets2( self, activator, self->target3 );
		return;
	}

	// Become solid again.
	if ( !self->count )
	{
		self->count = 1;
		self->activator = activator;
		self->r.svFlags &= ~SVF_NOCLIENT;
		self->s.eFlags &= ~EF_NODRAW;
	}

	G_ActivateBehavior( self, BSET_USE );
	//Don't explode if they've requested it to not
	if ( self->spawnflags & 64 )
	{//Usemodels toggling
		// GalaxyRP: [Entity System] not when there is no use model to switch to (its file was not on
		// the server, see SP_misc_model_breakable) -- that would switch to no model at all
		if ( (self->spawnflags & 32) && self->sound1to2 )
		{
			if( self->s.modelindex == self->sound1to2 )
			{
				self->s.modelindex = self->sound2to1;
			}
			else
			{
				self->s.modelindex = self->sound1to2;
			}
		}

		return;
	}

	self->die = misc_model_breakable_die;
	misc_model_breakable_die( self, other, activator, self->health, MOD_UNKNOWN );
}

void misc_model_breakable_init( gentity_t *ent )
{
	if (!ent->model) {
		trap->Error( ERR_DROP, "no model set on %s at (%.1f %.1f %.1f)\n", ent->classname, ent->s.origin[0],ent->s.origin[1],ent->s.origin[2] );
	}

	//Main model
	// GalaxyRP: [Slot Reuse] RP_EntityModelIndex(): an Entity System prop's model slot can be reused
	// once the props using it are gone (g_utils.c); for any other entity it is G_ModelIndex()
	ent->s.modelindex = ent->sound2to1 = RP_EntityModelIndex( ent, ent->model );

	if ( ent->spawnflags & 1 )
	{//Blocks movement
		ent->r.contents = CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_BODY|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP;//Was CONTENTS_SOLID, but only architecture should be this
	}
	else if ( ent->health )
	{//Can only be shot
		ent->r.contents = CONTENTS_SHOTCLIP;
	}

	ent->use = misc_model_use;

	if ( ent->health ) 
	{
		// GalaxyRP: [Slot Reuse] RP_BREAKABLE_SOUND in g_spawn.c counts this name's gamestate bytes
		// before an Entity System breakable spawns -- keep the two the same
		G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
		ent->maxHealth = ent->health;
		ent->takedamage = qtrue;
		ent->pain = misc_model_breakable_pain;
		ent->die = misc_model_breakable_die;
	}
}

/*QUAKED misc_exploding_crate (1 0 0.25) (-24 -24 0) (24 24 64)
model="models/map_objects/nar_shaddar/crate_xplode.md3"
Basic exploding crate

"health" - how much health the model has - default 40 (zero makes non-breakable)

"splashRadius" - radius to do damage in - default 128
"splashDamage" - amount of damage to do when it explodes - default 50

"targetname" - auto-explodes
"target" - what to use when it dies

*/
//------------------------------------------------------------
void SP_misc_exploding_crate( gentity_t *ent )
{
	G_SpawnInt( "health", "40", &ent->health );

	// GalaxyRP fix: [Entity System] zyk added splashdamage/splashradius to the entity key table, so
	// these are already parsed into the entity before the spawn function runs -- and an unguarded
	// G_SpawnInt then overwrote whatever the map or /entadd had set with the default. fx_runner
	// already guards its pair this way; the two SP-ported explosives did not, so the same two keys
	// behaved in opposite ways in the same file. They agree now.
	if (!ent->splashRadius)
		G_SpawnInt( "splashRadius", "128", &ent->splashRadius );

	if (!ent->splashDamage)
		G_SpawnInt( "splashDamage", "50", &ent->splashDamage );

	ent->s.modelindex = G_ModelIndex( "models/map_objects/nar_shaddar/crate_xplode.md3" );
	G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
	G_EffectIndex( "chunks/metalexplode" );
	
	VectorSet( ent->r.mins, -24, -24, 0 );
	VectorSet( ent->r.maxs, 24, 24, 64 );

	ent->r.contents = CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_BODY|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP;//CONTENTS_SOLID;
	ent->takedamage = qtrue;

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity((sharedEntity_t *)ent);

	if ( ent->targetname )
	{
		ent->use = misc_model_use;
	}

	ent->material = MAT_CRATE1;
	ent->die = misc_model_breakable_die;//ExplodeDeath;
}

/*QUAKED misc_gas_tank (1 0 0.25) (-4 -4 0) (4 4 40)
model="models/map_objects/imp_mine/tank.md3"
Basic exploding oxygen tank

"health" - how much health the model has - default 20 (zero makes non-breakable)

"splashRadius" - radius to do damage in - default 48
"splashDamage" - amount of damage to do when it explodes - default 32

"targetname" - auto-explodes
"target" - what to use when it dies

*/

void gas_random_jet( gentity_t *self )
{
	vec3_t pt;

	VectorCopy( self->r.currentOrigin, pt );
	pt[2] += 50;

	G_PlayEffect( G_EffectIndex("env/mini_gasjet"), pt, self->r.currentAngles );

	self->nextthink = level.time + Q_flrand(0.0f, 1.0f) * 16000 + 12000; // do this rarely
}

//------------------------------------------------------------
void GasBurst( gentity_t *self, gentity_t *attacker, int damage )
{
	vec3_t pt;

	VectorCopy( self->r.currentOrigin, pt );
	pt[2] += 46;

	G_PlayEffect( G_EffectIndex("env/mini_flamejet"), pt, self->r.currentAngles );

	// do some damage to anything that may be standing on top of it when it bursts into flame
	pt[2] += 32;
	G_RadiusDamage( pt, self, 32, 32, self, NULL, MOD_UNKNOWN );

	//  only get one burst
	self->pain = 0;
}

void SP_misc_gas_tank( gentity_t *ent )
{
	G_SpawnInt( "health", "20", &ent->health );

	// GalaxyRP fix: [Entity System] same as misc_exploding_crate above -- a map-set or /entadd-set
	// splashdamage/splashradius was silently replaced by the default.
	if (!ent->splashRadius)
		G_SpawnInt( "splashRadius", "48", &ent->splashRadius );

	if (!ent->splashDamage)
		G_SpawnInt( "splashDamage", "32", &ent->splashDamage );

	ent->s.modelindex = G_ModelIndex( "models/map_objects/imp_mine/tank.md3" );
	G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
	G_EffectIndex( "chunks/metalexplode" );
	G_EffectIndex( "env/mini_flamejet" );
	G_EffectIndex( "env/mini_gasjet" );

	VectorSet( ent->r.mins, -4, -4, 0 );
	VectorSet( ent->r.maxs, 4, 4, 40 );

	ent->r.contents = CONTENTS_SOLID;
	ent->takedamage = qtrue;

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	ent->pain = GasBurst;

	if ( ent->targetname )
	{
		ent->use = misc_model_use;
	}

	ent->material = MAT_METAL3;

	ent->die = misc_model_breakable_die;

	ent->think = gas_random_jet;
	ent->nextthink = level.time + Q_flrand(0.0f, 1.0f) * 12000 + 6000; // do this rarely
}

/*QUAKED misc_G2model (1 0 0) (-16 -16 -16) (16 16 16)
"model"		arbitrary .glm file to display
*/
void SP_misc_G2model( gentity_t *ent ) {
	RP_SpawnSaysWhy( ent, "misc_G2model does nothing in multiplayer: use misc_model_ghoul for a .glm model, misc_model_breakable for an .md3" );

#if 0
	char name1[200] = "models/players/kyle/modelmp.glm";
	trap->G2API_InitGhoul2Model(&ent->s, name1, G_ModelIndex( name1 ), 0, 0, 0, 0);
	trap->G2API_SetBoneAnim(ent->s.ghoul2, 0, "model_root", 0, 12, BONE_ANIM_OVERRIDE_LOOP, 1.0f, level.time, -1, -1);
	ent->s.radius = 150;
//	VectorSet (ent->r.mins, -16, -16, -16);
//	VectorSet (ent->r.maxs, 16, 16, 16);
	trap->LinkEntity ((sharedEntity_t *)ent);

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
#else
	G_FreeEntity( ent );
#endif
}

/*QUAKED misc_model_ghoul (1 0 0) (-16 -16 -16) (16 16 16) SOLID LOOP x x x x x x x x x x START_OFF
DAJ_RP: [SP Maps] a Ghoul2 (.glm) model, drawn by the client's own code for Ghoul2 props (CG_General in
cgame/cg_ents.c builds it from the model's name, as for Lugormod's .glm models) -- single player's static
model (g_misc_model.cpp), with these added:

"model"			any .glm file: as given, or under models/ ("map_objects/x/y.glm" works); ".glm" is added to a
				name with no extension, and '\' read as '/'. A map's entity whose file the server does not have,
				or that is not a .glm, is logged and removed; the Entity System refuses one
"skin"			the skin file next to the model: "test" is <the model's folder>/model_test.skin. No key, "" or
				"default" is the model's own (a model without one shows model_default.skin from its folder);
				a skin whose file is not on the server shows that too, and says so (logged, and to the admin
				who made it). Players need the DAJ_RP client for a skin of their own -- see CG_General()
"angles"		as usual
"mins"/"maxs"	its box, default -16 -16 -16 / 16 16 16: what the entity commands aim at and place, and, with
				SOLID, what it blocks
"radius"		how far from its origin the model reaches, 1 to 255 (the most the network carries): the client
				stops drawing it when a sphere this big around the origin is out of view. Default: the box's
				farthest corner, at least 50 (single player's). The drawn scale multiplies it on the client
"modelscale"	the drawn size and the box, as misc_model_breakable's: 0.01 to 10.23; "zykmodelscale" (a
				percentage) still decides the drawn size if given; "modelscale_vec" scales the box only.
				A SOLID one is raised by what the scale took off the bottom of its box, as a breakable is,
				to keep the box's bottom where it was (the entity commands, which place it by its box, allow
				for it -- RP_EntGrabScaleShift() in g_entgrab.c); any other is placed, and drawn, from its
				origin, which stays where it is -- as single player's, which has no box to raise it by
"startframe"	frames of the model's animation file (.gla), 0 to 65534, both played: the client plays them
"endframe"		once at 20 a second and stays on the last one. Only "startframe": that frame alone, a pose;
				only "endframe": from frame 0. No key: the model's base pose. A frame past the file's last
				shows frame 0 (the renderer checks)
"light"/"color"	a light on the model, as misc_model_breakable's
"targetname"	using it shows or hides it

SOLID (1)		blocks movement and shots with its box, as misc_model_breakable's SOLID
LOOP (2)		plays the frames over and over: the server restarts them each time the last one is reached
				(with the client's 100 ms blend back to the first); nothing for a single frame
START_OFF (4096)	starts hidden (as misc_model_breakable's): not sent to the clients, not solid

The Entity System refuses one with a negative frame, one past 65534, an "endframe" before "startframe",
or a "radius" out of range; a map's own logs them and leaves them out.
*/
#define RP_GHOUL_SOLID			1
#define RP_GHOUL_LOOP			2
#define RP_GHOUL_START_OFF		4096
#define RP_GHOUL_MAX_FRAME		65534	// the end sent is one past the last frame, in a 16-bit field
#define RP_GHOUL_MIN_RADIUS		50		// single player's s.radius for the class
#define RP_GHOUL_MAX_RADIUS		255		// entityState_t::g2radius is 8 bits on the wire
#define RP_GHOUL_SOLID_CONTENTS	( CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_BODY|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP )

// the model's path: NULL and path filled in, or what is wrong. The path a client is sent has to fit
// MAX_QPATH: the client copies a model name into a buffer that size.
static const char *RP_GhoulModel( const char *model, char *path, int pathSize )
{
	char typed[MAX_QPATH * 2], candidate[MAX_QPATH];
	const char *found;
	size_t len;
	int i, where;

	if ( !model || !model[0] )
		return "it has no model";

	if ( strlen( model ) >= sizeof( typed ) )
		return va( "model %s is not on the server", RP_ShownText( model ) );

	// DAJ_RP: [SP Maps] '\' as '/', and no leading '/' (the path every client is sent is the one name for
	// the model), as Lugormod reads a model key
	Q_strncpyz( typed, model, sizeof( typed ) );
	for ( i = 0; typed[i]; i++ )
	{
		if ( typed[i] == '\\' )
			typed[i] = '/';
	}
	i = 0;
	while ( typed[i] == '/' )
		i++;
	if ( !typed[i] )
		return va( "model %s is not on the server", RP_ShownText( model ) );

	where = RP_FindModelFileExt( typed + i, ".glm", candidate, sizeof( candidate ) );
	if ( !where )
		return va( "model %s is not on the server", RP_ShownText( model ) );

	found = ( where == 2 ) ? candidate : typed + i;
	len = strlen( found );
	if ( len < 5 || Q_stricmp( found + len - 4, ".glm" ) != 0 )
		return va( "model %s is not a .glm model", RP_ShownText( found ) );
	if ( (int)len >= MAX_QPATH || (int)len >= pathSize )
		return va( "model %s has too long a path (at most %d characters)", RP_ShownText( found ), MAX_QPATH - 1 );

	Q_strncpyz( path, found, pathSize );
	return NULL;
}

// the frames "startframe" and "endframe" ask for: NULL, with *animated qfalse for neither key, or what is
// wrong with them
static const char *RP_GhoulFrames( int *start, int *end, qboolean *animated )
{
	char *startText = NULL, *endText = NULL;
	qboolean hasStart, hasEnd;

	hasStart = ( G_SpawnString( "startframe", "", &startText ) && startText && startText[0] ) ? qtrue : qfalse;
	hasEnd = ( G_SpawnString( "endframe", "", &endText ) && endText && endText[0] ) ? qtrue : qfalse;

	*animated = qfalse;
	*start = *end = 0;
	if ( !hasStart && !hasEnd )
		return NULL;

	*start = hasStart ? atoi( startText ) : 0;
	*end = hasEnd ? atoi( endText ) : *start;

	if ( *start < 0 || *start > RP_GHOUL_MAX_FRAME )
		return va( "startframe %d is not a frame from 0 to %d", *start, RP_GHOUL_MAX_FRAME );
	if ( *end < 0 || *end > RP_GHOUL_MAX_FRAME )
		return va( "endframe %d is not a frame from 0 to %d", *end, RP_GHOUL_MAX_FRAME );
	if ( *end < *start )
		return va( "endframe %d is before startframe %d", *end, *start );

	*animated = qtrue;
	return NULL;
}

// the "radius" key: NULL, with *given qfalse when there is none, or what is wrong with it
static const char *RP_GhoulRadiusKey( int *radius, qboolean *given )
{
	float value = 0.0f;

	*given = G_SpawnFloat( "radius", "0", &value );
	*radius = 0;
	if ( !*given )
		return NULL;

	if ( !( value >= 1.0f && value <= (float)RP_GHOUL_MAX_RADIUS ) )
		return va( "radius %s is not from 1 to %d", RP_ShownText( va( "%g", value ) ), RP_GHOUL_MAX_RADIUS );

	*radius = (int)( value + 0.5f );
	if ( *radius > RP_GHOUL_MAX_RADIUS )
		*radius = RP_GHOUL_MAX_RADIUS;
	return NULL;
}

// the skin entry the "skin" key registers for the model at path -- "@" and the skin file's path, which the
// client's code before DAJ_RP's skips as it skips a saber name -- or "" for none. Returns why a skin that
// was asked for is not used, or NULL.
static const char *RP_GhoulSkin( const char *path, char *entry, int entrySize )
{
	char *skin = NULL;
	char file[MAX_QPATH * 2];
	const char *slash;
	int i, folderLen;

	entry[0] = '\0';

	if ( !G_SpawnString( "skin", "", &skin ) || !skin || !skin[0] || !Q_stricmp( skin, "default" ) )
		return NULL;

	for ( i = 0; skin[i]; i++ )
	{
		const char c = skin[i];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) )
			return "the skin key is not a skin name (letters, digits, '_' and '-' only)";
	}

	slash = strrchr( path, '/' );
	folderLen = slash ? (int)( slash - path ) + 1 : 0;	// with the '/'
	if ( folderLen + 6 + i + 5 >= (int)sizeof( file ) )
		return va( "skin %s makes too long a path", skin );

	Com_sprintf( file, sizeof( file ), "%.*smodel_%s.skin", folderLen, path, skin );
	if ( (int)strlen( file ) + 1 >= MAX_QPATH || (int)strlen( file ) + 1 >= entrySize )
		return va( "skin file %s has too long a path (at most %d characters)", RP_ShownText( file ), MAX_QPATH - 2 );
	if ( !RP_FileExists( file ) )
		return va( "skin file %s is not on the server", RP_ShownText( file ) );

	Com_sprintf( entry, entrySize, "@%s", file );
	return NULL;
}

const char *RP_GhoulSpawnProblem( gentity_t *ent, char *path, int pathSize, char *skinEntry, int skinSize )
{
	const char *problem;
	int start, end, radius;
	qboolean animated, given;

	skinEntry[0] = '\0';

	problem = RP_GhoulModel( ent->model, path, pathSize );
	if ( problem )
		return problem;
	problem = RP_GhoulFrames( &start, &end, &animated );
	if ( problem )
		return problem;
	problem = RP_GhoulRadiusKey( &radius, &given );
	if ( problem )
		return problem;

	RP_GhoulSkin( path, skinEntry, skinSize );
	return NULL;
}

static void RP_GhoulShow( gentity_t *ent, qboolean shown )
{
	if ( shown )
	{
		ent->r.svFlags &= ~SVF_NOCLIENT;
		ent->s.eFlags &= ~EF_NODRAW;
		ent->r.contents = ( ent->spawnflags & RP_GHOUL_SOLID ) ? RP_GHOUL_SOLID_CONTENTS : 0;
	}
	else
	{
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
	}
	trap->LinkEntity( (sharedEntity_t *)ent );
}

static void misc_model_ghoul_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	G_ActivateBehavior( self, BSET_USE );
	RP_GhoulShow( self, ( self->r.svFlags & SVF_NOCLIENT ) ? qtrue : qfalse );
}

// LOOP: the client plays the frames again when torsoFlip changes (CG_General's EF_G2ANIMATING routine);
// genericValue5 is how long they take
static void misc_model_ghoul_loop( gentity_t *self )
{
	self->s.torsoFlip = !self->s.torsoFlip;
	self->nextthink = level.time + self->genericValue5;
}

void SP_misc_model_ghoul( gentity_t *ent )
{
	char path[MAX_QPATH], skinEntry[MAX_QPATH];
	const char *problem;
	qboolean esMade = RP_EntitySystemMade( ent );
	qboolean bHasScale, animated, radiusGiven, lightSet, colorSet;
	float uniformScale = 0.0f, light = 100.0f, temp = 0.0f;
	vec3_t color;
	int start, end, radius, percent = 0, k;

	problem = RP_GhoulModel( ent->model, path, sizeof( path ) );
	if ( problem )
	{
		RP_SpawnSaysWhy( ent, problem );
		G_LogPrintf( "misc_model_ghoul %d at %s: %s; removed\n", ent->s.number, vtos( ent->s.origin ), problem );
		G_FreeEntity( ent );
		return;
	}
	if ( strcmp( path, ent->model ) )
		ent->model = G_NewStringRaw( path );

	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = RP_EntityModelIndex( ent, ent->model );
	if ( !ent->s.modelindex && !esMade )
	{
		// the Entity System's own is refused after the spawn instead (RP_EntitySystemSpawnedEmpty)
		G_LogPrintf( "misc_model_ghoul %d at %s: no room for model %s; removed\n", ent->s.number, vtos( ent->s.origin ), ent->model );
		G_FreeEntity( ent );
		return;
	}
	ent->s.modelGhoul2 = 1;

	// the skin entry, or the model's own skin -- said, when one was asked for
	ent->s.modelindex2 = 0;
	problem = RP_GhoulSkin( ent->model, skinEntry, sizeof( skinEntry ) );
	if ( problem )
	{
		G_LogPrintf( "misc_model_ghoul %d at %s: %s; its default skin is shown\n", ent->s.number, vtos( ent->s.origin ), problem );
		if ( esMade )
			Q_strncpyz( level.rp_spawn_note, va( "%s: the model's default skin is shown.", problem ), sizeof( level.rp_spawn_note ) );
	}
	else if ( skinEntry[0] )
	{
		ent->s.modelindex2 = RP_EntityModelIndex( ent, skinEntry );
	}

	// the box
	G_SpawnVector( "mins", "-16 -16 -16", ent->r.mins );
	G_SpawnVector( "maxs", "16 16 16", ent->r.maxs );

	// the radius: the key, or the box's farthest corner (before any scale: the client scales it itself)
	problem = RP_GhoulRadiusKey( &radius, &radiusGiven );
	if ( problem )
		G_LogPrintf( "misc_model_ghoul %d at %s: %s; the default is used\n", ent->s.number, vtos( ent->s.origin ), problem );
	if ( problem || !radiusGiven )
	{
		vec3_t corner;
		float reach;

		for ( k = 0; k < 3; k++ )
			corner[k] = ( fabs( ent->r.mins[k] ) > fabs( ent->r.maxs[k] ) ) ? fabs( ent->r.mins[k] ) : fabs( ent->r.maxs[k] );
		reach = VectorLength( corner );
		radius = ( reach > (float)RP_GHOUL_MAX_RADIUS ) ? RP_GHOUL_MAX_RADIUS : (int)ceil( reach );
		if ( radius < RP_GHOUL_MIN_RADIUS )
			radius = RP_GHOUL_MIN_RADIUS;
	}
	ent->s.g2radius = radius;

	// the scale, as misc_model_breakable's (SP_misc_model_breakable)
	bHasScale = G_SpawnVector( "modelscale_vec", "0 0 0", ent->modelScale );
	if ( !bHasScale )
	{
		G_SpawnFloat( "modelscale", "0", &temp );
		if ( temp != 0.0f )
		{
			ent->modelScale[0] = ent->modelScale[1] = ent->modelScale[2] = temp;
			bHasScale = qtrue;
			uniformScale = temp;
		}
	}
	if ( bHasScale )
	{
		float oldMins2 = ent->r.mins[2];

		ent->r.maxs[0] *= ent->modelScale[0];
		ent->r.mins[0] *= ent->modelScale[0];
		ent->r.maxs[1] *= ent->modelScale[1];
		ent->r.mins[1] *= ent->modelScale[1];
		ent->r.maxs[2] *= ent->modelScale[2];
		ent->r.mins[2] *= ent->modelScale[2];
		if ( ent->spawnflags & RP_GHOUL_SOLID )
			ent->s.origin[2] += ( oldMins2 - ent->r.mins[2] );
	}
	if ( !G_SpawnInt( "zykmodelscale", "0", &percent ) )
	{
		if ( uniformScale > 0.0f )
		{
			percent = (int)( uniformScale * 100.0f + 0.5f );
			if ( percent < 1 )
				percent = 1;
			if ( percent > 1023 )
				percent = 1023;
			ent->s.iModelScale = percent;
		}
		else
		{
			ent->s.iModelScale = 0;
		}
	}

	// the light
	lightSet = G_SpawnFloat( "light", "100", &light );
	colorSet = G_SpawnVector( "color", "1 1 1", color );
	ent->s.constantLight = ( lightSet || colorSet ) ? RP_PackConstantLight( light, color ) : 0;

	// the frames: CG_General plays torsoAnim up to legsAnim, the last one not played, and again whenever
	// torsoFlip changes -- which it does here on every spawn, so a client that builds the model anew
	// (RP_EntRespawnPrepare() has it do that on /entedit) plays them on it too
	ent->s.torsoAnim = ent->s.legsAnim = 0;
	problem = RP_GhoulFrames( &start, &end, &animated );
	if ( problem )
	{
		G_LogPrintf( "misc_model_ghoul %d at %s: %s; not animated\n", ent->s.number, vtos( ent->s.origin ), problem );
		animated = qfalse;
	}
	if ( animated )
	{
		ent->s.eFlags |= EF_G2ANIMATING;
		ent->s.torsoAnim = start;
		ent->s.legsAnim = end + 1;
		ent->s.torsoFlip = !ent->s.torsoFlip;

		if ( ( ent->spawnflags & RP_GHOUL_LOOP ) && end > start )
		{
			ent->genericValue5 = ( end - start + 1 ) * 50;
			ent->think = misc_model_ghoul_loop;
			ent->nextthink = level.time + ent->genericValue5;
		}
	}

	ent->use = misc_model_ghoul_use;
	ent->takedamage = qfalse;

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );

	// shown, or START_OFF -- this links it
	RP_GhoulShow( ent, ( ent->spawnflags & RP_GHOUL_START_OFF ) ? qfalse : qtrue );
}

//===========================================================

void locateCamera( gentity_t *ent ) {
	vec3_t		dir;
	gentity_t	*target;
	gentity_t	*owner;

	owner = G_PickTarget( ent->target );
	if ( !owner ) {
		trap->Print( "Couldn't find target for misc_partal_surface\n" );
		G_FreeEntity( ent );
		return;
	}
	ent->r.ownerNum = owner->s.number;

	// frame holds the rotate speed
	if ( owner->spawnflags & 1 ) {
		ent->s.frame = 25;
	} else if ( owner->spawnflags & 2 ) {
		ent->s.frame = 75;
	}

	// swing camera ?
	if ( owner->spawnflags & 4 ) {
		// set to 0 for no rotation at all
		ent->s.powerups = 0;
	}
	else {
		ent->s.powerups = 1;
	}

	// clientNum holds the rotate offset
	ent->s.clientNum = owner->s.clientNum;

	VectorCopy( owner->s.origin, ent->s.origin2 );

	// see if the portal_camera has a target
	target = G_PickTarget( owner->target );
	if ( target ) {
		VectorSubtract( target->s.origin, owner->s.origin, dir );
		VectorNormalize( dir );
	} else {
		G_SetMovedir( owner->s.angles, dir );
	}

	ent->s.eventParm = DirToByte( dir );
}

/*QUAKED misc_portal_surface (0 0 1) (-8 -8 -8) (8 8 8)
The portal surface nearest this entity will show a view from the targeted misc_portal_camera, or a mirror view if untargeted.
This must be within 64 world units of the surface!
*/
void SP_misc_portal_surface(gentity_t *ent) {
	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap->LinkEntity ((sharedEntity_t *)ent);

	ent->r.svFlags = SVF_PORTAL;
	ent->s.eType = ET_PORTAL;

	if ( !ent->target ) {
		VectorCopy( ent->s.origin, ent->s.origin2 );
	} else {
		ent->think = locateCamera;
		ent->nextthink = level.time + 100;
	}
}

/*QUAKED misc_portal_camera (0 0 1) (-8 -8 -8) (8 8 8) slowrotate fastrotate noswing
The target for a misc_portal_director.  You can set either angles or target another entity to determine the direction of view.
"roll" an angle modifier to orient the camera around the target vector;
*/
void SP_misc_portal_camera(gentity_t *ent) {
	float	roll;

	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap->LinkEntity ((sharedEntity_t *)ent);

	G_SpawnFloat( "roll", "0", &roll );

	ent->s.clientNum = roll/360.0 * 256;
}

/*QUAKED misc_bsp (1 0 0) (-16 -16 -16) (16 16 16)
"bspmodel"		arbitrary .bsp file to display
*/
extern void G_FindTeams( void );

void SP_misc_bsp(gentity_t *ent)
{
	char	temp[MAX_QPATH];
	char	*out;
	float	newAngle;
	int		tempint;
	int		subSlot;

	G_SpawnFloat( "angle", "0", &newAngle );
	if (newAngle != 0.0)
	{
		ent->s.angles[1] = newAngle;
	}
	// don't support rotation any other way
	ent->s.angles[0] = 0.0;
	ent->s.angles[2] = 0.0;

	G_SpawnString("bspmodel", "", &out);

	// GalaxyRP fix: [Entity System] no longer EF_PERMANENT. The engine never sends a permanent entity in a
	// snapshot (sv_snapshot.cpp): clients only ever see it in the baseline, which is taken once, a few
	// frames after the map spawns, and is never rebuilt -- so a misc_bsp spawned later (/entadd, /entedit,
	// /entload, the default.txt preset, which also frees and respawns the map's own) was invisible to
	// everyone, reconnecting or not, and the clients' copies of the ones it replaced lingered as ghosts.
	// Sent like any entity instead, and to every client (SVF_BROADCAST): a sub-BSP placed outside the
	// main map's sealed volume touches none of its visibility clusters, and permanents were drawn
	// regardless of visibility -- this keeps that. Static, so after the first snapshot it costs nothing.
	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_BROADCAST;

	// Mainly for debugging
	G_SpawnInt( "spacing", "0", &tempint);
	ent->s.time2 = tempint;
	G_SpawnInt( "flatten", "0", &tempint);
	ent->s.time = tempint;

	Com_sprintf(temp, MAX_QPATH, "#%s", out);

	// GalaxyRP fix: [Entity System] a misc_bsp spawned after map load -- /entadd, /entedit, /entload
	// and the automatic default.txt preset -- used to be able to end the server three ways:
	//  - SetBrushModel() below loads "maps/<bspmodel>.bsp" through CM_LoadSubBSP(), which
	//    Com_Error(ERR_DROP)s on a file it cannot load and on a 33rd unique sub-BSP. On a dedicated
	//    server that is a process exit. So after map load only a sub-BSP the map itself loaded is
	//    accepted; the engine hands that one back by name without loading anything.
	//  - The nested G_SpawnEntitiesFromString(qtrue) set level.spawning and only the outer, map-load
	//    pass ever cleared it. Run from a command it stayed set for the rest of the map, which
	//    switched off the "*N"/"#name" brush-model guard in zyk_brush_model_allowed() -- the next
	//    out-of-range model any entity command named went straight to the engine's ERR_DROP. The
	//    nested pass now leaves it alone after map load (see G_SpawnEntitiesFromString()).
	//  - The same nested pass spawned the sub-BSP's whole entity list again on every respawn, on top
	//    of the copies an entity preset had saved. /entsave no longer writes those copies (see
	//    gentity_t::rpSubBSPOf) -- they could not load back right anyway, their "*N" models count in
	//    the sub-BSP's numbering -- so rebuilding them here, once per spawn, is the only way they come
	//    back, and /entremove and /entedit on this misc_bsp take its old ones along first.
	if (level.spawning == qfalse)
	{
		if (!out[0] || zyk_subbsp_name_known(temp) == qfalse)
		{
			G_LogPrintf( "misc_bsp at %s: bspmodel \"%s\" is not a sub-BSP this map loaded; not spawned.\n",
				vtos( ent->s.origin ), out );

			// DAJ_RP: [Entity System] and the reason for the command to print, as for the other refusals
			// (level.rp_spawn_refusal -- see RP_EntitySystemSpawnRefused() in g_spawn.c), so /entadd says why
			// rather than that it "did not survive being spawned"
			if ( out[0] )
			{
				char shown[48];
				int i;

				Q_strncpyz( shown, RP_ShownText( out ), sizeof( shown ) );
				for ( i = 0; shown[i]; i++ )
				{
					if ( shown[i] == '"' || shown[i] == '\n' || shown[i] == '\r' || shown[i] == ';' )
						shown[i] = '?';
				}
				Q_strncpyz( level.rp_spawn_refusal, va( "bspmodel %s is not a sub-BSP this map loaded", shown ), sizeof( level.rp_spawn_refusal ) );
			}
			else
			{
				Q_strncpyz( level.rp_spawn_refusal, "it needs a bspmodel, a sub-BSP this map loaded", sizeof( level.rp_spawn_refusal ) );
			}
			G_FreeEntity( ent );
			return;
		}
	}
	else
	{
		zyk_learn_subbsp_name( temp );
	}

	subSlot = zyk_subbsp_name_slot( temp );

	trap->SetBrushModel( (sharedEntity_t *)ent, temp );  // SV_SetBrushModel -- sets mins and maxs
	G_BSPIndex(temp);

	level.mNumBSPInstances++;
	ent->rpBSPInstance = level.mNumBSPInstances;
	Com_sprintf(temp, MAX_QPATH, "%d-", level.mNumBSPInstances);

	/*
	G_SpawnString("filter", "", &out);
	strcpy(level.mFilter, out);
	*/
	// GalaxyRP fix: [Entity System] was strcpy() into the MAX_QPATH-byte level.mTeamFilter from a
	// spawn value bounded only by MAX_STRING_CHARS, so a long "teamfilter" wrote past it into the
	// rest of level_locals_t. Read here, while the spawn vars are still this entity's.
	G_SpawnString("teamfilter", "", &out);

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	VectorCopy( ent->s.angles, ent->r.currentAngles );

	ent->s.eType = ET_MOVER;

	trap->LinkEntity ((sharedEntity_t *)ent);

	// GalaxyRP fix: [Entity System] a misc_bsp inside another's sub-BSP places its geometry only. The
	// engine keeps one sub-BSP parse point: the inner SetActiveSubBSP(-1) below ended the outer one, so
	// at map load the outer misc_bsp went on reading the MAIN map's entity string as if it were its own
	// -- the rest of the map's entities spawned shifted and renamed, and the map's own pass then found
	// nothing left -- and after map load the outer rebuild simply stopped there. Leaving the inner list
	// alone keeps the outer one intact.
	if (level.mBSPInstanceDepth > 0)
	{
		G_LogPrintf( "misc_bsp %d at %s: inside another misc_bsp's sub-BSP, so its own sub-BSP entities are not spawned.\n",
			ent->s.number, vtos( ent->s.origin ) );
		return;
	}

	{
		const char *entityString = trap->SetActiveSubBSP(ent->s.modelindex);
		const int savedInstance = level.rp_subbsp_spawning_instance;
		const int savedActive = level.rp_active_subbsp;

		if (level.spawning == qfalse)
		{
			// GalaxyRP fix: [Entity System] after map load, only a sub-BSP whose entity list a misc_bsp
			// already spawned at map load (so it is known to parse -- G_ParseSpawnVars() has its own
			// ERR_DROPs), and only if there is room for all of it. G_Spawn(), G_SpawnLogical() and
			// G_Alloc() each end the server when they run out:
			//  - entity slots: one entity per '{' can only over-count, and twice that covers the
			//    classes that make a second entity at once (func_plat's trigger, a door's a frame
			//    later) -- the margin /entadd and the preset loader keep for the same reason. None at
			//    all would reach G_SpawnEntitiesFromString()'s "no entities" ERR_DROP;
			//  - the game's memory pool, which never gets memory back within a map: every key and
			//    value is stored at least twice (the spawn-key record and the parsed field), in 32-byte
			//    steps, so four times the text plus a fixed margin.
			// Otherwise the geometry stays, without its entities, and the log says why.
			int needed = 0;
			int textLength = entityString ? (int)strlen(entityString) : 0;
			const char *p;
			const char *refusal = NULL;

			for (p = entityString; p && *p; p++)
			{
				if (*p == '{')
					needed++;
			}

			if (subSlot < 0 || !level.rp_subbsp_entities_spawned[subSlot])
				refusal = "the map never spawned this sub-BSP's entity list";
			else if (needed <= 0)
				refusal = "its entity list is empty";
			else if (G_EntitySlotsAvailable(needed * 2) == qfalse || G_FreeLogicalEntityCount() < needed * 2 + ZYK_LOGICAL_ENTITY_RESERVE)
				refusal = va("%d entities would not fit", needed);
			else if (G_AllocRemaining() < textLength * 4 + 65536)
				refusal = "the game's memory pool is nearly full";

			if (refusal)
			{
				trap->SetActiveSubBSP(-1);
				G_LogPrintf( "misc_bsp %d at %s: sub-BSP entities not rebuilt (%s).\n", ent->s.number, vtos( ent->s.origin ), refusal );
				return;
			}
		}
		else if (subSlot >= 0)
		{
			level.rp_subbsp_entities_spawned[subSlot] = qtrue;
		}

		VectorCopy(ent->s.origin, level.mOriginAdjust);
		level.mRotationAdjust = ent->s.angles[1];
		level.mTargetAdjust = temp;
		//level.hasBspInstances = qtrue; //rww - also not referenced anywhere.
		level.mBSPInstanceDepth++;
		Q_strncpyz(level.mTeamFilter, out, sizeof(level.mTeamFilter));
		level.rp_subbsp_spawning_instance = ent->rpBSPInstance;
		level.rp_active_subbsp = subSlot + 1;

		G_SpawnEntitiesFromString(qtrue);
		trap->SetActiveSubBSP(-1);

		level.rp_subbsp_spawning_instance = savedInstance;
		level.rp_active_subbsp = savedActive;
		level.mBSPInstanceDepth--;
		//level.mFilter[0] = level.mTeamFilter[0] = 0;
		level.mTeamFilter[0] = 0;

		if (level.spawning == qfalse)
		{
			gentity_t *child;

			// GalaxyRP: [Entity System] what the map's own spawn pass does for these at the end, and a
			// runtime rebuild skips there (see G_SpawnEntitiesFromString()): their soundset indexes,
			// one at a time -- G_PrecacheSoundsets() walks every entity and has an ERR_DROP of its own.
			RP_FOR_EACH_ENTITY( child )
			{
				if (child->inuse && child->rpSubBSPOf == ent->rpBSPInstance && child->soundSet && child->soundSet[0])
					child->s.soundSetIndex = G_SoundSetIndex(child->soundSet);
			}

			// GalaxyRP: [Entity System] and, unless an entity preset is being loaded -- the loader does
			// both once, after its last line -- a sub-BSP trigger_shipboundary's marker promoted out of
			// the logical region, and door teams linked, in the order the map's spawn pass uses
			// (G_FindTeams() builds pointer chains, so the promotion's free-and-reallocate goes first)
			if (level.load_entities_timer == 0)
			{
				RP_PromoteShipboundaryTargets();
				G_FindTeams();
			}
		}
	}

	/*
	if ( g_debugRMG.integer )
	{
		G_SpawnDebugCylinder ( ent->s.origin, ent->s.time2, &g_entities[0], 2000, COLOR_WHITE );

		if ( ent->s.time )
		{
			G_SpawnDebugCylinder ( ent->s.origin, ent->s.time, &g_entities[0], 2000, COLOR_RED );
		}
	}
	*/
}

/*QUAKED terrain (1.0 1.0 1.0) ? NOVEHDMG

NOVEHDMG - don't damage vehicles upon impact with this terrain

Terrain entity
It will stretch to the full height of the brush

numPatches - integer number of patches to split the terrain brush into (default 200)
terxels - integer number of terxels on a patch side (default 4) (2 <= count <= 8)
seed - integer seed for random terrain generation (default 0)
textureScale - float scale of texture (default 0.005)
heightmap - name of heightmap data image to use, located in heightmaps/*.png. (must be PNG format)
terrainDef - defines how the game textures the terrain (file is base/ext_data/rmg/*.terrain - default is grassyhills)
instanceDef - defines which bsp instances appear
miscentDef - defines which client models spawn on the terrain (file is base/ext_data/rmg/*.miscents)
densityMap - how dense the client models are packed

*/
void AddSpawnField(char *field, char *value);
#define MAX_INSTANCE_TYPES		16
void SP_terrain(gentity_t *ent)
{
	RP_SpawnSaysWhy( ent, "terrain is random-map (RMG) terrain, which this engine does not have" );
	G_FreeEntity (ent);
}

//rww - Called by skyportal entities. This will check through entities and flag them
//as portal ents if they are in the same pvs as a skyportal entity and pass
//a direct point trace check between origins. I really wanted to use an eFlag for
//flagging portal entities, but too many entities like to reset their eFlags.
//Note that this was not part of the original wolf sky portal stuff.
void G_PortalifyEntities(gentity_t *ent)
{
	int i = 0;
	gentity_t *scan = NULL;

	while (i < MAX_GENTITIES)
	{
		scan = &g_entities[i];

		if (scan && scan->inuse && scan->s.number != ent->s.number && trap->InPVS(ent->s.origin, scan->r.currentOrigin))
		{
			trace_t tr;

			trap->Trace(&tr, ent->s.origin, vec3_origin, vec3_origin, scan->r.currentOrigin, ent->s.number, CONTENTS_SOLID, qfalse, 0, 0);

			if (tr.fraction == 1.0 || (tr.entityNum == scan->s.number && tr.entityNum != ENTITYNUM_NONE && tr.entityNum != ENTITYNUM_WORLD))
			{
				if (!scan->client || scan->s.eType == ET_NPC)
				{ //making a client a portal entity would be bad.
					scan->s.isPortalEnt = qtrue; //he's flagged now
				}
			}
		}

		i++;
	}

	// GalaxyRP: [Entity System] one the Entity System made stays, inert -- see SP_misc_skyportal()
	if ( RP_EntitySystemMade( ent ) )
	{
		ent->think = NULL;
		ent->nextthink = 0;
		return;
	}

	ent->think = G_FreeEntity; //the portal entity is no longer needed because its information is stored in a config string.
	ent->nextthink = level.time;
}

/*QUAKED misc_skyportal_orient (.6 .7 .7) (-8 -8 0) (8 8 16)
point from which to orient the sky portal cam in relation
to the regular view position.

"modelscale"			the scale at which to scale positions
*/
void SP_misc_skyportal_orient (gentity_t *ent)
{
	RP_SpawnSaysWhy( ent, "a misc_skyportal_orient is read by each client from its own copy of the map file, so one added on the server does nothing" );
	G_FreeEntity(ent);
}


/*QUAKED misc_skyportal (.6 .7 .7) (-8 -8 0) (8 8 16)
"fov" for the skybox default is 80
To have the portal sky fogged, enter any of the following values:
"onlyfoghere" if non-0 allows you to set a global fog, but will only use that fog within this sky portal.

Also note that entities in the same PVS and visible (via point trace) from this
object will be flagged as portal entities. This means they will be sent and
updated from the server for every client every update regardless of where
they are, and they will essentially be added to the scene twice if the client
is in the same PVS as them (only once otherwise, but still once no matter
where the client is). In other words, don't go overboard with it or everything
will explode.
*/
void SP_misc_skyportal (gentity_t *ent)
{
	vec3_t	fogv;	//----(SA)
	int		fogn;	//----(SA)
	int		fogf;	//----(SA)
	int		isfog = 0;	// (SA)

	float	fov_x;

	// GalaxyRP fix: [Entity System] added after map load, a sky portal replaces the map's for every
	// client (CS_SKYBOXORG below): refused when the map has one of its own, or when another entity the
	// admins added is the sky portal now (level.rp_skyportal_owner: the one that set the sky, so that
	// spawning it again in place -- /entedit -- is not refused as a second one). One added where the map
	// has none is allowed, and kept as an inert entity (G_PortalifyEntities used to free it) so it is
	// listed, saved by /entsave and can be removed -- and removing it clears the sky again, since the
	// clients read CS_SKYBOXORG every frame: RP_SkyPortalRelease(), from G_FreeEntity(). The marks it
	// set on the entities it can see (isPortalEnt) go with it: RP_SkyPortalUnflag().
	if ( RP_EntitySystemMade( ent ) )
	{
		if ( level.rp_skyportal_owner == -1 && RP_RefuseAtRuntime( ent, "the map has a sky portal of its own; a second one would replace it for everyone" ) )
		{
			return;
		}
		if ( level.rp_skyportal_owner > 0 && level.rp_skyportal_owner != ent->s.number + 1 )
		{
			gentity_t *owner = &g_entities[level.rp_skyportal_owner - 1];

			if ( owner->inuse && RP_RefuseAtRuntime( ent, va( "entity %d is the sky portal already: /entedit it, or /entremove it first", owner->s.number ) ) )
			{
				return;
			}
		}
		// GalaxyRP fix: [Entity System] spawned again in place (/entedit, /entrotate, a dropped /entcut) it is
		// not released first: the entities it marked from where it stood are unmarked before it marks the
		// ones it can see from here (G_PortalifyEntities). The map has no portal of its own, so every mark
		// in the world is this one's.
		RP_SkyPortalUnflag();
		level.rp_skyportal_owner = ent->s.number + 1;
		Q_strncpyz( level.rp_spawn_note, "The sky portal is set for every client; removing this entity clears it again.", sizeof( level.rp_spawn_note ) );
	}
	else
	{
		level.rp_skyportal_owner = -1;
	}

	G_SpawnFloat ("fov", "80", &fov_x);	// GalaxyRP fix: [Entity System] through the parser that refuses NaN/Inf

	isfog += G_SpawnVector ("fogcolor", "0 0 0", fogv);
	isfog += G_SpawnInt ("fognear", "0", &fogn);
	isfog += G_SpawnInt ("fogfar", "300", &fogf);

	trap->SetConfigstring( CS_SKYBOXORG, va("%.2f %.2f %.2f %.1f %i %.2f %.2f %.2f %i %i", ent->s.origin[0], ent->s.origin[1], ent->s.origin[2], fov_x, (int)isfog, fogv[0], fogv[1], fogv[2], fogn, fogf ) );

	ent->think = G_PortalifyEntities;
	ent->nextthink = level.time + 1050; //give it some time first so that all other entities are spawned.
}

/*
=================
RP_SkyPortalUnflag

GalaxyRP fix: [Entity System] G_PortalifyEntities() marks every entity the sky portal can see as part of
the sky (s.isPortalEnt), and the marks are network state that outlived an added portal: the engine sends
a marked entity to every client every frame, whatever it can see (sv_snapshot.cpp), and a client plays a
marked fx_runner's effect only in the sky view (cg_ents.c) -- with the portal gone, invisible until the map
changed. Called only for a portal the Entity System added (the map then has none of its own, so every
mark is that portal's): when it is removed, and before it marks again from where it is spawned anew. An
fx_runner used while marked was also set to be sent to everyone (fx_runner_use()), and nothing else sets
that on one: cleared with its mark.
=================
*/
void RP_SkyPortalUnflag( void )
{
	int i;

	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		gentity_t *e = &g_entities[i];

		if ( !e->inuse || !e->s.isPortalEnt )
			continue;

		e->s.isPortalEnt = qfalse;
		if ( e->s.eType == ET_FX )
			e->r.svFlags &= ~SVF_BROADCAST;
	}
}

/*
=================
RP_SkyPortalRelease

GalaxyRP fix: [Entity System] called by G_FreeEntity() for every entity: if this one is the sky portal
the Entity System set (see SP_misc_skyportal), the sky goes with it -- CS_SKYBOXORG cleared, which every
client reads each frame -- so /entremove, /entundo, /entload's clearing pass and a dropped /entcut all
undo it, and the next one added is not refused as "a second one". The map's own portal (-1) is never
an entity number and never released: its entity frees itself at map start with the sky set for good.
=================
*/
void RP_SkyPortalRelease( gentity_t *ent )
{
	if ( !ent || level.rp_skyportal_owner <= 0 || level.rp_skyportal_owner != ent->s.number + 1 )
		return;

	level.rp_skyportal_owner = 0;
	trap->SetConfigstring( CS_SKYBOXORG, "" );
	RP_SkyPortalUnflag();
	G_LogPrintf( "sky portal entity %d removed: the sky is the map's own again\n", ent->s.number );
}

/*QUAKED misc_holocron (0 0 1) (-8 -8 -8) (8 8 8)
count	Set to type of holocron (based on force power value)
	HEAL = 0
	JUMP = 1
	SPEED = 2
	PUSH = 3
	PULL = 4
	TELEPATHY = 5
	GRIP = 6
	LIGHTNING = 7
	RAGE = 8
	PROTECT = 9
	ABSORB = 10
	TEAM HEAL = 11
	TEAM FORCE = 12
	DRAIN = 13
	SEE = 14
	SABERATTACK = 15
	SABERDEFEND = 16
	SABERTHROW = 17
*/

/*char *holocronTypeModels[] = {
	"models/chunks/rock/rock_big.md3",//FP_HEAL,
	"models/chunks/rock/rock_big.md3",//FP_LEVITATION,
	"models/chunks/rock/rock_big.md3",//FP_SPEED,
	"models/chunks/rock/rock_big.md3",//FP_PUSH,
	"models/chunks/rock/rock_big.md3",//FP_PULL,
	"models/chunks/rock/rock_big.md3",//FP_TELEPATHY,
	"models/chunks/rock/rock_big.md3",//FP_GRIP,
	"models/chunks/rock/rock_big.md3",//FP_LIGHTNING,
	"models/chunks/rock/rock_big.md3",//FP_RAGE,
	"models/chunks/rock/rock_big.md3",//FP_PROTECT,
	"models/chunks/rock/rock_big.md3",//FP_ABSORB,
	"models/chunks/rock/rock_big.md3",//FP_TEAM_HEAL,
	"models/chunks/rock/rock_big.md3",//FP_TEAM_FORCE,
	"models/chunks/rock/rock_big.md3",//FP_DRAIN,
	"models/chunks/rock/rock_big.md3",//FP_SEE
	"models/chunks/rock/rock_big.md3",//FP_SABER_OFFENSE
	"models/chunks/rock/rock_big.md3",//FP_SABER_DEFENSE
	"models/chunks/rock/rock_big.md3"//FP_SABERTHROW
};*/

void HolocronRespawn(gentity_t *self)
{
	self->s.modelindex = (self->count - 128);
}

void HolocronPopOut(gentity_t *self)
{
	if (Q_irand(1, 10) < 5)
	{
		self->s.pos.trDelta[0] = 150 + Q_irand(1, 100);
	}
	else
	{
		self->s.pos.trDelta[0] = -150 - Q_irand(1, 100);
	}
	if (Q_irand(1, 10) < 5)
	{
		self->s.pos.trDelta[1] = 150 + Q_irand(1, 100);
	}
	else
	{
		self->s.pos.trDelta[1] = -150 - Q_irand(1, 100);
	}
	self->s.pos.trDelta[2] = 150 + Q_irand(1, 100);
}

void HolocronTouch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	int i = 0;
	int othercarrying = 0;
	float time_lowest = 0;
	int index_lowest = -1;
	int hasall = 1;
	int forceReselect = WP_NONE;

	if (trace)
	{
		self->s.groundEntityNum = trace->entityNum;
	}

	if (!other || !other->client || other->health < 1)
	{
		return;
	}

	if (!self->s.modelindex)
	{
		return;
	}

	if (self->enemy)
	{
		return;
	}

	if (other->client->ps.holocronsCarried[self->count])
	{
		return;
	}

	if (other->client->ps.holocronCantTouch == self->s.number && other->client->ps.holocronCantTouchTime > level.time)
	{
		return;
	}

	while (i < NUM_FORCE_POWERS)
	{
		if (other->client->ps.holocronsCarried[i])
		{
			othercarrying++;

			if (index_lowest == -1 || other->client->ps.holocronsCarried[i] < time_lowest)
			{
				index_lowest = i;
				time_lowest = other->client->ps.holocronsCarried[i];
			}
		}
		else if (i != self->count)
		{
			hasall = 0;
		}
		i++;
	}

	if (hasall)
	{ //once we pick up this holocron we'll have all of them, so give us super special best prize!
		//trap->Print("You deserve a pat on the back.\n");
	}

	if (!(other->client->ps.fd.forcePowersActive & (1 << other->client->ps.fd.forcePowerSelected)))
	{ //If the player isn't using his currently selected force power, select this one
		if (self->count != FP_SABER_OFFENSE && self->count != FP_SABER_DEFENSE && self->count != FP_SABERTHROW && self->count != FP_LEVITATION)
		{
			other->client->ps.fd.forcePowerSelected = self->count;
		}
	}

	if (g_maxHolocronCarry.integer && othercarrying >= g_maxHolocronCarry.integer)
	{ //make the oldest holocron carried by the player pop out to make room for this one
		other->client->ps.holocronsCarried[index_lowest] = 0;

		/*
		if (index_lowest == FP_SABER_OFFENSE && !HasSetSaberOnly())
		{ //you lost your saberattack holocron, so no more saber for you
			other->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
			other->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

			if (other->client->ps.weapon == WP_SABER)
			{
				forceReselect = WP_SABER;
			}
		}
		*/
		//NOTE: No longer valid as we are now always giving a force level 1 saber attack level in holocron
	}

	//G_Sound(other, CHAN_AUTO, G_SoundIndex("sound/weapons/w_pkup.wav"));
	G_AddEvent( other, EV_ITEM_PICKUP, self->s.number );

	other->client->ps.holocronsCarried[self->count] = level.time;
	self->s.modelindex = 0;
	self->enemy = other;

	self->pos2[0] = 1;
	self->pos2[1] = level.time + HOLOCRON_RESPAWN_TIME;

	/*
	if (self->count == FP_SABER_OFFENSE && !HasSetSaberOnly())
	{ //player gets a saber
		other->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		other->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);

		if (other->client->ps.weapon == WP_STUN_BATON)
		{
			forceReselect = WP_STUN_BATON;
		}
	}
	*/

	if (forceReselect != WP_NONE)
	{
		G_AddEvent(other, EV_NOAMMO, forceReselect);
	}

	//trap->Print("DON'T TOUCH ME\n");
}

void HolocronThink(gentity_t *ent)
{
	if (ent->pos2[0] && (!ent->enemy || !ent->enemy->client || ent->enemy->health < 1))
	{
		if (ent->enemy && ent->enemy->client)
		{
			HolocronRespawn(ent);
			VectorCopy(ent->enemy->client->ps.origin, ent->s.pos.trBase);
			VectorCopy(ent->enemy->client->ps.origin, ent->s.origin);
			VectorCopy(ent->enemy->client->ps.origin, ent->r.currentOrigin);
			//copy to person carrying's origin before popping out of them
			HolocronPopOut(ent);
			ent->enemy->client->ps.holocronsCarried[ent->count] = 0;
			ent->enemy = NULL;

			goto justthink;
		}
	}
	else if (ent->pos2[0] && ent->enemy && ent->enemy->client)
	{
		ent->pos2[1] = level.time + HOLOCRON_RESPAWN_TIME;
	}

	if (ent->enemy && ent->enemy->client)
	{
		if (!ent->enemy->client->ps.holocronsCarried[ent->count])
		{
			ent->enemy->client->ps.holocronCantTouch = ent->s.number;
			ent->enemy->client->ps.holocronCantTouchTime = level.time + 5000;

			HolocronRespawn(ent);
			VectorCopy(ent->enemy->client->ps.origin, ent->s.pos.trBase);
			VectorCopy(ent->enemy->client->ps.origin, ent->s.origin);
			VectorCopy(ent->enemy->client->ps.origin, ent->r.currentOrigin);
			//copy to person carrying's origin before popping out of them
			HolocronPopOut(ent);
			ent->enemy = NULL;

			goto justthink;
		}

		if (!ent->enemy->inuse || (ent->enemy->client && ent->enemy->client->ps.fallingToDeath))
		{
			if (ent->enemy->inuse && ent->enemy->client)
			{
				ent->enemy->client->ps.holocronBits &= ~(1 << ent->count);
				ent->enemy->client->ps.holocronsCarried[ent->count] = 0;
			}
			ent->enemy = NULL;
			HolocronRespawn(ent);
			VectorCopy(ent->s.origin2, ent->s.pos.trBase);
			VectorCopy(ent->s.origin2, ent->s.origin);
			VectorCopy(ent->s.origin2, ent->r.currentOrigin);

			ent->s.pos.trTime = level.time;

			ent->pos2[0] = 0;

			trap->LinkEntity((sharedEntity_t *)ent);

			goto justthink;
		}
	}

	if (ent->pos2[0] && ent->pos2[1] < level.time)
	{ //isn't in original place and has been there for (HOLOCRON_RESPAWN_TIME) seconds without being picked up, so respawn
		VectorCopy(ent->s.origin2, ent->s.pos.trBase);
		VectorCopy(ent->s.origin2, ent->s.origin);
		VectorCopy(ent->s.origin2, ent->r.currentOrigin);

		ent->s.pos.trTime = level.time;

		ent->pos2[0] = 0;

		trap->LinkEntity((sharedEntity_t *)ent);
	}

justthink:
	ent->nextthink = level.time + 50;

	if (ent->s.pos.trDelta[0] || ent->s.pos.trDelta[1] || ent->s.pos.trDelta[2])
	{
		G_RunObject(ent);
	}
}

void SP_misc_holocron(gentity_t *ent)
{
	vec3_t dest;
	trace_t tr;

	if (level.gametype != GT_HOLOCRON)
	{
		RP_SpawnSaysWhy( ent, "misc_holocron only exists in the Holocron gametype" );
		G_FreeEntity(ent);
		return;
	}

	if (HasSetSaberOnly())
	{
		if (ent->count == FP_SABER_OFFENSE ||
			ent->count == FP_SABER_DEFENSE ||
			ent->count == FP_SABERTHROW)
		{ //having saber holocrons in saber only mode is pointless
			G_FreeEntity(ent);
			return;
		}
	}

	ent->s.isJediMaster = qtrue;

	VectorSet( ent->r.maxs, 8, 8, 8 );
	VectorSet( ent->r.mins, -8, -8, -8 );

	ent->s.origin[2] += 0.1f;
	ent->r.maxs[2] -= 0.1f;

	VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
	trap->Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID, qfalse, 0, 0 );
	if ( tr.startsolid )
	{
		trap->Print ("SP_misc_holocron: misc_holocron startsolid at %s\n", vtos(ent->s.origin));
		G_FreeEntity( ent );
		return;
	}

	//add the 0.1 back after the trace
	ent->r.maxs[2] += 0.1f;

	// allow to ride movers
//	ent->s.groundEntityNum = tr.entityNum;

	G_SetOrigin( ent, tr.endpos );

	if (ent->count < 0)
	{
		ent->count = 0;
	}

	if (ent->count >= NUM_FORCE_POWERS)
	{
		ent->count = NUM_FORCE_POWERS-1;
	}
/*
	if (g_forcePowerDisable.integer &&
		(g_forcePowerDisable.integer & (1 << ent->count)))
	{
		G_FreeEntity(ent);
		return;
	}
*/
	//No longer doing this, causing too many complaints about accidentally setting no force powers at all
	//and starting a holocron game (making it basically just FFA)

	ent->enemy = NULL;

	ent->flags = FL_BOUNCE_HALF;

	ent->s.modelindex = (ent->count - 128);//G_ModelIndex(holocronTypeModels[ent->count]);
	ent->s.eType = ET_HOLOCRON;
	ent->s.pos.trType = TR_GRAVITY;
	ent->s.pos.trTime = level.time;

	ent->r.contents = CONTENTS_TRIGGER;
	ent->clipmask = MASK_SOLID;

	ent->s.trickedentindex4 = ent->count;

	if (forcePowerDarkLight[ent->count] == FORCE_DARKSIDE)
	{
		ent->s.trickedentindex3 = 1;
	}
	else if (forcePowerDarkLight[ent->count] == FORCE_LIGHTSIDE)
	{
		ent->s.trickedentindex3 = 2;
	}
	else
	{
		ent->s.trickedentindex3 = 3;
	}

	ent->physicsObject = qtrue;

	VectorCopy(ent->s.pos.trBase, ent->s.origin2); //remember the spawn spot

	ent->touch = HolocronTouch;

	trap->LinkEntity((sharedEntity_t *)ent);

	ent->think = HolocronThink;
	ent->nextthink = level.time + 50;
}

/*
======================================================================

  SHOOTERS

======================================================================
*/

void Use_Shooter( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	vec3_t		dir;
	float		deg;
	vec3_t		up, right;

	// see if we have a target
	if ( ent->enemy ) {
		VectorSubtract( ent->enemy->r.currentOrigin, ent->s.origin, dir );
		VectorNormalize( dir );
	} else {
		VectorCopy( ent->movedir, dir );
	}

	// randomize a bit
	PerpendicularVector( up, dir );
	CrossProduct( up, dir, right );

	deg = Q_flrand(-1.0f, 1.0f) * ent->random;
	VectorMA( dir, deg, up, dir );

	deg = Q_flrand(-1.0f, 1.0f) * ent->random;
	VectorMA( dir, deg, right, dir );

	VectorNormalize( dir );

	switch ( ent->s.weapon ) {
	case WP_BLASTER:
		WP_FireBlasterMissile( ent, ent->s.origin, dir, qfalse );
		break;
	}

	G_AddEvent( ent, EV_FIRE_WEAPON, 0 );
}


static void InitShooter_Finish( gentity_t *ent ) {
	ent->enemy = G_PickTarget( ent->target );
	ent->think = 0;
	ent->nextthink = 0;
}

void InitShooter( gentity_t *ent, int weapon ) {
	ent->use = Use_Shooter;
	ent->s.weapon = weapon;

	RegisterItem( BG_FindItemForWeapon( weapon ) );

	G_SetMovedir( ent->s.angles, ent->movedir );

	if ( !ent->random ) {
		ent->random = 1.0;
	}
	ent->random = sin( M_PI * ent->random / 180 );
	// target might be a moving object, so we can't set movedir for it
	if ( ent->target ) {
		ent->think = InitShooter_Finish;
		ent->nextthink = level.time + 500;
	}
	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*QUAKED shooter_blaster (1 0 0) (-16 -16 -16) (16 16 16)
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_blaster( gentity_t *ent ) {
	InitShooter( ent, WP_BLASTER);
}

void check_recharge(gentity_t *ent)
{
	if (ent->fly_sound_debounce_time < level.time ||
		!ent->activator ||
		!ent->activator->client ||
		!(ent->activator->client->pers.cmd.buttons & BUTTON_USE))
	{
		if (ent->activator)
		{
			G_Sound(ent, CHAN_AUTO, ent->genericValue7);
		}
		ent->s.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
		ent->activator = NULL;
		ent->fly_sound_debounce_time = 0;
	}

	if (!ent->activator)
	{ //don't recharge during use
		if (ent->genericValue8 < level.time)
		{
			if (ent->count < ent->genericValue4)
			{
				ent->count++;
			}
			ent->genericValue8 = level.time + ent->genericValue5;
		}
	}
	ent->s.health = ent->count; //the "health bar" is gonna be how full we are
	ent->nextthink = level.time;
}

/*
================
EnergyShieldStationSettings
================
*/
void EnergyShieldStationSettings(gentity_t *ent)
{
	G_SpawnInt( "count", "200", &ent->count );

	G_SpawnInt("chargerate", "0", &ent->genericValue5);

	if (!ent->genericValue5)
	{
		ent->genericValue5 = STATION_RECHARGE_TIME;
	}
}

/*
================
shield_power_converter_use
================
*/
void shield_power_converter_use( gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int dif,add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	if ( level.gametype == GT_SIEGE
		&& other
		&& other->client
		&& other->client->siegeClass )
	{
		if ( !bgSiegeClasses[other->client->siegeClass].maxarmor )
		{//can't use it!
			G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/shieldcon_empty"));
			return;
		}
	}

	if (self->setTime < level.time)
	{
		int	maxArmor;
		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/interface/shieldcon_run");
			self->s.loopIsSoundset = qfalse;
		}
		self->setTime = level.time + 100;

		if ( level.gametype == GT_SIEGE
			&& other
			&& other->client
			&& other->client->siegeClass != -1 )
		{
			maxArmor = bgSiegeClasses[other->client->siegeClass].maxarmor;
		}
		else
		{
			// zyk: RPG Mode max shield
			if (activator->client->sess.amrpgmode == 2)
				maxArmor = activator->client->pers.max_rpg_shield;
			else
				maxArmor = activator->client->ps.stats[STAT_MAX_HEALTH];
		}
		dif = maxArmor - activator->client->ps.stats[STAT_ARMOR];

		if (dif > 0)					// Already at full armor?
		{
			if (dif >MAX_AMMO_GIVE)
			{
				add = MAX_AMMO_GIVE;
			}
			else
			{
				add = dif;
			}

			if (self->count<add)
			{
				add = self->count;
			}

		    if (!self->genericValue12)
			{
				self->count -= add;
			}
			if (self->count <= 0)
			{
				self->setTime = 0;
			}
			stop = 0;

			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;

			activator->client->ps.stats[STAT_ARMOR] += add;
		}
	}

	if (stop || self->count <= 0)
	{
		if (self->s.loopSound && self->setTime < level.time)
		{
			if (self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/shieldcon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		if (self->setTime < level.time)
		{
			self->setTime = level.time + self->genericValue5+100;
		}
	}
}

// DAJ_RP: [Ammo] may an ammo dispenser top up this ammo type for this player? Only the types that
// have a server cap (RP_MaxAmmo() > 0 -- which leaves out emplaced-gun ammo, never a dispenser's to
// hand out), and explosives only for a player who already owns that weapon. Thermals, trip mines and
// detpacks are both the ammo and the weapon: a logged-in character gets the weapon from its Thermals /
// Trip Mines / Detpacks skill (initialize_rpg_skills), a logged-out player from whatever they are
// carrying, and a dispenser must not be a way round either.
static qboolean RP_DispenserMayGiveAmmo( gentity_t *activator, int ammoType )
{
	if ( RP_MaxAmmo( ammoType ) <= 0 )
	{
		return qfalse;
	}

	if ( ammoType == AMMO_THERMAL )
	{
		return ( activator->client->ps.stats[STAT_WEAPONS] & (1 << WP_THERMAL) ) ? qtrue : qfalse;
	}
	if ( ammoType == AMMO_TRIPMINE )
	{
		return ( activator->client->ps.stats[STAT_WEAPONS] & (1 << WP_TRIP_MINE) ) ? qtrue : qfalse;
	}
	if ( ammoType == AMMO_DETPACK )
	{
		return ( activator->client->ps.stats[STAT_WEAPONS] & (1 << WP_DET_PACK) ) ? qtrue : qfalse;
	}

	return qtrue;
}

// DAJ_RP: [Ammo] would an ammo dispenser give this player anything at all, were it charged?
static qboolean RP_PlayerNeedsDispenserAmmo( gentity_t *activator )
{
	int i;

	for ( i = AMMO_BLASTER; i < AMMO_MAX; i++ )
	{
		if ( RP_DispenserMayGiveAmmo( activator, i ) && activator->client->ps.ammo[i] < RP_MaxAmmo( i ) )
		{
			return qtrue;
		}
	}

	return qfalse;
}

//dispense generic ammo
// DAJ_RP: [Ammo] misc_ammo_floor_unit. Reworked:
// - It no longer hands out the Thermal, Trip Mine and Det Pack WEAPONS. zyk added that grant: any
//   player below the explosive caps got all three, so a logged-in character bypassed the Thermals /
//   Trip Mines / Detpacks skills entirely and kept the weapons until their next respawn. Explosive ammo
//   is now topped up only for a weapon the player already has -- see RP_DispenserMayGiveAmmo().
// - It drains its charge once per use, and only when it actually gave something (vanilla behaviour).
//   It used to drain once per ammo TYPE on every use, given or not -- emplaced-gun ammo, which it never
//   gives, included -- so a fully stocked player holding Use emptied it for everyone else.
// - An empty unit (charge 0, not "nodrain") gives nothing. It used to give one type's worth before
//   noticing it was empty.
// - It only takes the unit over (self->activator, which pauses check_recharge()) when it gives
//   something or is empty, so a player who needs nothing no longer stops it recharging by holding Use,
//   while an empty unit still waits for the player to let go before it recharges, as before.
// Caps are unchanged: Add_Ammo() fills to the rp_max_* cvars, as it always did here.
void ammo_generic_power_converter_use( gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	// GalaxyRP fix: [RPG Class] removed the Bounty Hunter (rpg_class==2) max-ammo bonus block;
	// pers.rpg_class is permanently 0, so this was dead code.

	if (self->setTime < level.time)
	{
		int i = AMMO_BLASTER;
		qboolean gave = qfalse;

		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/interface/ammocon_run");
			self->s.loopIsSoundset = qfalse;
		}
		//self->setTime = level.time + 100;

		if (!self->genericValue12 && self->count <= 0)
		{ // empty: give nothing -- the block below plays the "empty" sound and waits for the recharge
			i = AMMO_MAX;
		}

		while (i < AMMO_MAX)
		{
			int max_ammo = RP_MaxAmmo(i);

			add = max_ammo * 0.008;

			if (add < 1)
			{
				add = 1;
			}

			// GalaxyRP fix: [RPG Class] removed the "some RPG classes cannot get ammo" early-break
			// (rpg_class == 1/4/6/8) and the Force Guardian (rpg_class==9) explosive-weapon skip;
			// pers.rpg_class is permanently 0, so both were dead code.
			if (RP_DispenserMayGiveAmmo(activator, i) && activator->client->ps.ammo[i] < max_ammo)
			{
				// zyk: changed this. Now it will use Add_Ammo function
				Add_Ammo(activator, i, add);
				gave = qtrue;
			}

			i++;
		}

		if (gave)
		{
			stop = 0;
			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;

			if (!self->genericValue12)
			{
				self->count--;
				if (self->count <= 0)
				{
					self->count = 0;
					stop = 1;
				}
			}
		}
		else if (!self->genericValue12 && self->count <= 0 && RP_PlayerNeedsDispenserAmmo(activator))
		{ // empty and held by someone who needs ammo: keep hold of it, as vanilla did, so it does not
		  // recharge (and trickle ammo back out) until they let go -- check_recharge() only recharges
		  // a unit with no activator. A player who needs nothing does not hold it up.
			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;
		}
	}

	if (stop || (!self->genericValue12 && self->count <= 0))
	{
		if (self->s.loopSound && self->setTime < level.time)
		{
			if (!self->genericValue12 && self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/ammocon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		if (self->setTime < level.time)
		{
			self->setTime = level.time + self->genericValue5+100;
		}
	}
}

/*QUAKED misc_ammo_floor_unit (1 0 0) (-16 -16 0) (16 16 40)
model="/models/items/a_pwr_converter.md3"
Gives generic ammo when used

"count" - max charge value (default 200)
"chargerate" - rechage 1 point every this many milliseconds (default 2000)
"nodrain" - don't drain power from station if 1
*/
void SP_misc_ammo_floor_unit(gentity_t *ent)
{
	vec3_t dest;
	trace_t tr;

	VectorSet( ent->r.mins, -16, -16, 0 );
	VectorSet( ent->r.maxs, 16, 16, 40 );

	ent->s.origin[2] += 0.1f;
	ent->r.maxs[2] -= 0.1f;

	VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
	trap->Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID, qfalse, 0, 0 );
	if ( tr.startsolid )
	{
		trap->Print ("SP_misc_ammo_floor_unit: misc_ammo_floor_unit startsolid at %s\n", vtos(ent->s.origin));
		G_FreeEntity( ent );
		return;
	}

	//add the 0.1 back after the trace
	ent->r.maxs[2] += 0.1f;

	// allow to ride movers
	ent->s.groundEntityNum = tr.entityNum;

	G_SetOrigin( ent, tr.endpos );

	if (!ent->health)
	{
		ent->health = 60;
	}

	// DAJ_RP: [Dispensers] its "model" key's .md3 if the server has it, else its own -- RP_DispenserModel()
	ent->s.modelindex = G_ModelIndex( RP_DispenserModel( ent, RP_MODEL_AMMO_FLOOR_UNIT ) );

	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->r.contents = CONTENTS_SOLID;
	ent->clipmask = MASK_SOLID;

	EnergyShieldStationSettings(ent);

	ent->genericValue4 = ent->count; //initial value
	ent->think = check_recharge;

	G_SpawnInt("nodrain", "0", &ent->genericValue12);

	if (!ent->genericValue12)
	{
		ent->s.maxhealth = ent->s.health = ent->count;
	}
	ent->s.shouldtarget = qtrue;
	ent->s.teamowner = 0;
	ent->s.owner = ENTITYNUM_NONE;

	ent->nextthink = level.time + 200;// + STATION_RECHARGE_TIME;

	ent->use = ammo_generic_power_converter_use;

	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	G_SoundIndex("sound/interface/ammocon_run");
	ent->genericValue7 = G_SoundIndex("sound/interface/ammocon_done");
	G_SoundIndex("sound/interface/ammocon_empty");

	if (level.gametype == GT_SIEGE)
	{ //show on radar from everywhere
		ent->r.svFlags |= SVF_BROADCAST;
		ent->s.eFlags |= EF_RADAROBJECT;
		ent->s.genericenemyindex = G_IconIndex("gfx/mp/siegeicons/desert/weapon_recharge");
	}
}

/*QUAKED misc_shield_floor_unit (1 0 0) (-16 -16 0) (16 16 40)
model="/models/items/a_shield_converter.md3"
Gives shield energy when used.

"count" - max charge value (default 50)
"chargerate" - rechage 1 point every this many milliseconds (default 3000)
"nodrain" - don't drain power from me
*/
void SP_misc_shield_floor_unit( gentity_t *ent )
{
	vec3_t dest;
	trace_t tr;

	VectorSet( ent->r.mins, -16, -16, 0 );
	VectorSet( ent->r.maxs, 16, 16, 40 );

	ent->s.origin[2] += 0.1f;
	ent->r.maxs[2] -= 0.1f;

	VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
	trap->Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID, qfalse, 0, 0 );
	if ( tr.startsolid )
	{
		trap->Print ("SP_misc_shield_floor_unit: misc_shield_floor_unit startsolid at %s\n", vtos(ent->s.origin));
		G_FreeEntity( ent );
		return;
	}

	//add the 0.1 back after the trace
	ent->r.maxs[2] += 0.1f;

	// allow to ride movers
	ent->s.groundEntityNum = tr.entityNum;

	G_SetOrigin( ent, tr.endpos );

	if (!ent->health)
	{
		ent->health = 60;
	}

	// DAJ_RP: [Dispensers] its "model" key's .md3 if the server has it, else its own -- RP_DispenserModel()
	ent->s.modelindex = G_ModelIndex( RP_DispenserModel( ent, RP_MODEL_SHIELD_FLOOR_UNIT ) );

	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->r.contents = CONTENTS_SOLID;
	ent->clipmask = MASK_SOLID;

	EnergyShieldStationSettings(ent);

	ent->genericValue4 = ent->count; //initial value
	ent->think = check_recharge;

	G_SpawnInt("nodrain", "0", &ent->genericValue12);

    if (!ent->genericValue12)
	{
		ent->s.maxhealth = ent->s.health = ent->count;
	}
	ent->s.shouldtarget = qtrue;
	ent->s.teamowner = 0;
	ent->s.owner = ENTITYNUM_NONE;

	ent->nextthink = level.time + 200;// + STATION_RECHARGE_TIME;

	ent->use = shield_power_converter_use;

	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	G_SoundIndex("sound/interface/shieldcon_run");
	ent->genericValue7 = G_SoundIndex("sound/interface/shieldcon_done");
	G_SoundIndex("sound/interface/shieldcon_empty");

	if (level.gametype == GT_SIEGE)
	{ //show on radar from everywhere
		ent->r.svFlags |= SVF_BROADCAST;
		ent->s.eFlags |= EF_RADAROBJECT;
		ent->s.genericenemyindex = G_IconIndex("gfx/mp/siegeicons/desert/shield_recharge");
	}
}


/*QUAKED misc_model_shield_power_converter (1 0 0) (-16 -16 -16) (16 16 16)
model="models/items/power_converter.md3"
Gives shield energy when used.

"count" - max charge value (default 200)
"chargerate" - recharge 1 point every this many milliseconds (default 100)
"nodrain" - don't drain power from me
*/
//------------------------------------------------------------
void SP_misc_model_shield_power_converter( gentity_t *ent )
{
	if (!ent->health)
	{
		ent->health = 60;
	}

	VectorSet (ent->r.mins, -16, -16, -16);
	VectorSet (ent->r.maxs, 16, 16, 16);

	// zyk: if no model is set, use default model
	// DAJ_RP: [Dispensers] the power converter, as the ammo and health converters have (psd_sm.md3 is not
	// under models/items), and for an empty key or one whose .md3 the server has not -- RP_DispenserModel()
	ent->s.modelindex = G_ModelIndex( RP_DispenserModel( ent, RP_MODEL_POWER_CONVERTER ) );

	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->r.contents = CONTENTS_SOLID;
	ent->clipmask = MASK_SOLID;

	EnergyShieldStationSettings(ent);

	ent->genericValue4 = ent->count; //initial value
	ent->think = check_recharge;

	// DAJ_RP: [Dispensers] "nodrain", as on misc_shield_floor_unit: shield_power_converter_use() already
	// honoured it, nothing read it. A unit that never drains shows no fill bar.
	G_SpawnInt("nodrain", "0", &ent->genericValue12);

	if (!ent->genericValue12)
	{
		ent->s.maxhealth = ent->s.health = ent->count;
	}
	ent->s.shouldtarget = qtrue;
	ent->s.teamowner = 0;
	ent->s.owner = ENTITYNUM_NONE;

	ent->nextthink = level.time + 200;// + STATION_RECHARGE_TIME;

	ent->use = shield_power_converter_use;

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	//G_SoundIndex("sound/movers/objects/useshieldstation.wav");

	// DAJ_RP: [Dispensers] its sounds, as misc_shield_floor_unit has them. genericValue7 is the sound played
	// when it stops giving and when the player lets go (check_recharge()); it was never set, so that was
	// sound 0 -- an empty name every client tried to load ("S_findname: empty name"). The psd_big.md3 it
	// registered as a second model is gone: an ET_GENERAL entity never draws one, so it only took a slot.
	G_SoundIndex("sound/interface/shieldcon_run");
	ent->genericValue7 = G_SoundIndex("sound/interface/shieldcon_done");
	G_SoundIndex("sound/interface/shieldcon_empty");
}


/*
================
EnergyAmmoShieldStationSettings
================
*/
// DAJ_RP: [Dispensers] "count" and now "chargerate" too, as the floor units read them. With no chargerate
// (genericValue5 0) check_recharge() refilled it by 1 every server frame, so how fast depended on sv_fps.
void EnergyAmmoStationSettings(gentity_t *ent)
{
	EnergyShieldStationSettings( ent );
}

/*
================
ammo_power_converter_use
================
*/
// DAJ_RP: [Ammo] misc_model_ammo_power_converter. Reworked -- it had four faults:
// - Infinite ammo. "Has it got any power left?" was "if (self->count)", i.e. "not exactly 0", and each
//   use took 3 off a default charge of 200 -- not a multiple of 3 -- so the charge went 2 -> -1 -> -4
//   and never landed on 0: once it had been drained it gave forever. Empty is now "0 or less", and
//   the charge is never taken below 0.
// - It ignored the server's ammo caps, filling to the engine's ammoData[] maximums instead (1000
//   blaster / powercell / metal bolts, 100 rockets, 30 of each explosive against caps of 300 / 25 /
//   10). Ammo is saved to the database, so a logged-in character kept the overfill for good. It now
//   fills through Add_Ammo(), to the rp_max_* caps, 10% of the cap per use (minimum 1) as before.
// - It gave explosives to players without the weapon, and emplaced-gun ammo to anyone -- see
//   RP_DispenserMayGiveAmmo().
// - It drained 3 per use whether or not it gave anything, and never stopped its loop sound. It now
//   drains only when it actually gave something, and stops the sound when it doesn't.
#define RP_AMMO_CONVERTER_DRAIN	3	// the charge one giving use costs -- unchanged from before
// DAJ_RP: [Dispensers] and its sounds are now the ammo floor unit's: ammocon_run while it gives, ammocon_done
// when it stops giving (and, from check_recharge(), when the player lets go -- that sound was 0, an empty
// name every client tried to load: "S_findname: empty name"), ammocon_empty when someone who needs ammo
// uses it empty, repeated every chargerate + 100 ms as the floor unit does. It gives every 100 ms; between
// those uses nothing changes (it used to switch its loop sound off on every use in between).
void ammo_power_converter_use( gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if (!activator || !activator->client)
	{
		return;
	}

	if (self->setTime >= level.time)
	{
		return;
	}
	self->setTime = level.time + 100;

	if (self->genericValue12 || self->count > 0)	// Has it got any power left?
	{
		int i;
		qboolean gave = qfalse;

		for (i = AMMO_BLASTER; i < AMMO_MAX; i++)
		{
			int max_ammo = RP_MaxAmmo(i);
			int add = max_ammo * 0.1;

			if (!RP_DispenserMayGiveAmmo(activator, i) || activator->client->ps.ammo[i] >= max_ammo)
			{
				continue;
			}

			if (add < 1)
			{
				add = 1;
			}

			Add_Ammo(activator, i, add);
			gave = qtrue;
		}

		if (gave)
		{
			if (!self->s.loopSound)
			{
				self->s.loopSound = G_SoundIndex("sound/interface/ammocon_run");
				self->s.loopIsSoundset = qfalse;
			}

			if (!self->genericValue12)
			{
				self->count -= RP_AMMO_CONVERTER_DRAIN;
				if (self->count < 0)
				{
					self->count = 0;
				}
			}

			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;
			return;
		}
	}
	else if (RP_PlayerNeedsDispenserAmmo(activator))
	{ // empty and held by someone who needs ammo: keep hold of it so it does not recharge until they
	  // let go. check_recharge() refills a converter with no activator, which otherwise matched the
	  // drain and let a held, empty converter keep giving.
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/ammocon_empty"));
		self->setTime = level.time + self->genericValue5 + 100;
		self->fly_sound_debounce_time = level.time + 500;
		self->activator = activator;
		return;
	}

	// nothing given: the player is full (or needs nothing it has to give). If it was running, it is done.
	if (self->s.loopSound)
	{
		G_Sound(self, CHAN_AUTO, self->genericValue7);
	}
	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;
}


/*QUAKED misc_model_ammo_power_converter (1 0 0) (-16 -16 -16) (16 16 16)
model="models/items/power_converter.md3"
Gives ammo energy when used.

"count" - max charge value (default 200)
"chargerate" - recharge 1 point every this many milliseconds (default 100)
"nodrain" - don't drain power from me
*/
//------------------------------------------------------------
void SP_misc_model_ammo_power_converter( gentity_t *ent )
{
	if (!ent->health)
	{
		ent->health = 60;
	}

	VectorSet (ent->r.mins, -16, -16, -16);
	VectorSet (ent->r.maxs, 16, 16, 16);

	// DAJ_RP: [Dispensers] a default model, as the shield and health converters have: with no "model" key
	// this registered no model at all and the converter was invisible (but solid) -- RP_DispenserModel()
	ent->s.modelindex = G_ModelIndex( RP_DispenserModel( ent, RP_MODEL_POWER_CONVERTER ) );

	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->r.contents = CONTENTS_SOLID;
	ent->clipmask = MASK_SOLID;

	G_SpawnInt("nodrain", "0", &ent->genericValue12);
	ent->use = ammo_power_converter_use;

	EnergyAmmoStationSettings(ent);

	ent->genericValue4 = ent->count; //initial value
	ent->think = check_recharge;

	if (!ent->genericValue12)
	{
		ent->s.maxhealth = ent->s.health = ent->count;
	}
	ent->s.shouldtarget = qtrue;
	ent->s.teamowner = 0;
	ent->s.owner = ENTITYNUM_NONE;

	ent->nextthink = level.time + 200;// + STATION_RECHARGE_TIME;

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	//G_SoundIndex("sound/movers/objects/useshieldstation.wav");

	// DAJ_RP: [Dispensers] the ammo floor unit's sounds -- see ammo_power_converter_use()
	G_SoundIndex("sound/interface/ammocon_run");
	ent->genericValue7 = G_SoundIndex("sound/interface/ammocon_done");
	G_SoundIndex("sound/interface/ammocon_empty");
}

/*
================
EnergyHealthStationSettings
================
*/
// DAJ_RP: [Dispensers] "count" and "chargerate", as the shield units read them
void EnergyHealthStationSettings(gentity_t *ent)
{
	EnergyShieldStationSettings( ent );
}

/*
================
health_power_converter_use
================
*/
// DAJ_RP: [Dispensers] it drains now, as the shield converter does: each heal takes what it gave from its
// charge ("count"), an empty one gives nothing, plays shieldcon_empty and recharges ("chargerate") once the
// player lets go -- held empty, it stays held, so it gives no trickle -- and "nodrain" keeps it full. The
// line that took the charge was commented out, so "count" did nothing and it healed without end.
void health_power_converter_use( gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int dif,add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	if (self->setTime < level.time)
	{
		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/player/pickuphealth.wav");
			self->s.loopIsSoundset = qfalse;
		}
		self->setTime = level.time + 100;

		dif = activator->client->ps.stats[STAT_MAX_HEALTH] - activator->health;

		if (dif > 0)					// Already at full health?
		{
			if (dif >/*MAX_AMMO_GIVE*/5)
			{
				add = 5;//MAX_AMMO_GIVE;
			}
			else
			{
				add = dif;
			}

			if (self->count<add)
			{
				add = self->count;
			}

			if (!self->genericValue12)
			{
				self->count -= add;
			}
			if (self->count <= 0)
			{
				self->setTime = 0;
			}
			stop = 0;

			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;

			activator->health += add;
		}
	}

	if (stop || self->count <= 0)
	{
		if (self->s.loopSound && self->setTime < level.time)
		{
			if (self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/shieldcon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		if (self->setTime < level.time)
		{
			self->setTime = level.time + self->genericValue5+100;
		}
	}
}


/*QUAKED misc_model_health_power_converter (1 0 0) (-16 -16 -16) (16 16 16)
model="models/items/power_converter.md3"
Gives health when used.

"count" - max charge value (default 200)
"chargerate" - recharge 1 point every this many milliseconds (default 100)
"nodrain" - don't drain power from me
*/
//------------------------------------------------------------
void SP_misc_model_health_power_converter( gentity_t *ent )
{
	if (!ent->health)
	{
		ent->health = 60;
	}

	VectorSet (ent->r.mins, -16, -16, -16);
	VectorSet (ent->r.maxs, 16, 16, 16);

	// zyk: if no model is set, use default model
	// DAJ_RP: [Dispensers] also for an empty key, or one whose .md3 the server has not -- RP_DispenserModel()
	ent->s.modelindex = G_ModelIndex( RP_DispenserModel( ent, RP_MODEL_POWER_CONVERTER ) );

	ent->s.eFlags = 0;
	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->r.contents = CONTENTS_SOLID;
	ent->clipmask = MASK_SOLID;

	ent->use = health_power_converter_use;

	EnergyHealthStationSettings(ent);

	ent->genericValue4 = ent->count; //initial value
	ent->think = check_recharge;

	// DAJ_RP: [Dispensers] "nodrain", and the fill bar of one that drains -- see health_power_converter_use()
	G_SpawnInt("nodrain", "0", &ent->genericValue12);

	if (!ent->genericValue12)
	{
		ent->s.maxhealth = ent->s.health = ent->count;
	}
	ent->s.shouldtarget = qtrue;
	ent->s.teamowner = 0;
	ent->s.owner = ENTITYNUM_NONE;

	ent->nextthink = level.time + 200;// + STATION_RECHARGE_TIME;

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	trap->LinkEntity ((sharedEntity_t *)ent);

	//G_SoundIndex("sound/movers/objects/useshieldstation.wav");
	G_SoundIndex("sound/player/pickuphealth.wav");
	ent->genericValue7 = G_SoundIndex("sound/interface/shieldcon_done");
	G_SoundIndex("sound/interface/shieldcon_empty");

	if (level.gametype == GT_SIEGE)
	{ //show on radar from everywhere
		ent->r.svFlags |= SVF_BROADCAST;
		ent->s.eFlags |= EF_RADAROBJECT;
		ent->s.genericenemyindex = G_IconIndex("gfx/mp/siegeicons/desert/bacta");
	}
}

#if 0 //damage box stuff
void DmgBoxHit( gentity_t *self, gentity_t *other, trace_t *trace )
{
	return;
}

void DmgBoxUpdateSelf(gentity_t *self)
{
	gentity_t *owner = &g_entities[self->r.ownerNum];

	if (!owner || !owner->client || !owner->inuse)
	{
		goto killMe;
	}

	if (self->damageRedirect == DAMAGEREDIRECT_HEAD &&
		owner->client->damageBoxHandle_Head != self->s.number)
	{
		goto killMe;
	}

	if (self->damageRedirect == DAMAGEREDIRECT_RLEG &&
		owner->client->damageBoxHandle_RLeg != self->s.number)
	{
		goto killMe;
	}

	if (self->damageRedirect == DAMAGEREDIRECT_LLEG &&
		owner->client->damageBoxHandle_LLeg != self->s.number)
	{
		goto killMe;
	}

	if (owner->health < 1)
	{
		goto killMe;
	}

	//G_TestLine(self->r.currentOrigin, owner->client->ps.origin, 0x0000ff, 100);

	trap->LinkEntity((sharedEntity_t *)self);

	self->nextthink = level.time;
	return;

killMe:
	self->think = G_FreeEntity;
	self->nextthink = level.time;
}

void DmgBoxAbsorb_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
	self->health = 1;
}

void DmgBoxAbsorb_Pain(gentity_t *self, gentity_t *attacker, int damage)
{
	self->health = 1;
}

gentity_t *CreateNewDamageBox( gentity_t *ent )
{
	gentity_t *dmgBox;

	//We do not want the client to have any real knowledge of the entity whatsoever. It will only
	//ever be used on the server.
	dmgBox = G_Spawn();
	dmgBox->classname = "dmg_box";

	dmgBox->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	dmgBox->r.ownerNum = ent->s.number;

	dmgBox->clipmask = 0;
	dmgBox->r.contents = MASK_PLAYERSOLID;

	dmgBox->mass = 5000;

	dmgBox->s.eFlags |= EF_NODRAW;
	dmgBox->r.svFlags |= SVF_NOCLIENT;

	dmgBox->touch = DmgBoxHit;

	dmgBox->takedamage = qtrue;

	dmgBox->health = 1;

	dmgBox->pain = DmgBoxAbsorb_Pain;
	dmgBox->die = DmgBoxAbsorb_Die;

	dmgBox->think = DmgBoxUpdateSelf;
	dmgBox->nextthink = level.time + 50;

	return dmgBox;
}

void ATST_ManageDamageBoxes(gentity_t *ent)
{
	vec3_t headOrg, lLegOrg, rLegOrg;
	vec3_t fwd, right, up, flatAngle;

	if (!ent->client->damageBoxHandle_Head)
	{
		gentity_t *dmgBox = CreateNewDamageBox(ent);

		if (dmgBox)
		{
			VectorSet( dmgBox->r.mins, ATST_MINS0, ATST_MINS1, ATST_MINS2 );
			VectorSet( dmgBox->r.maxs, ATST_MAXS0, ATST_MAXS1, ATST_HEADSIZE );

			ent->client->damageBoxHandle_Head = dmgBox->s.number;
			dmgBox->damageRedirect = DAMAGEREDIRECT_HEAD;
			dmgBox->damageRedirectTo = ent->s.number;
		}
	}
	if (!ent->client->damageBoxHandle_RLeg)
	{
		gentity_t *dmgBox = CreateNewDamageBox(ent);

		if (dmgBox)
		{
			VectorSet( dmgBox->r.mins, ATST_MINS0/4, ATST_MINS1/4, ATST_MINS2 );
			VectorSet( dmgBox->r.maxs, ATST_MAXS0/4, ATST_MAXS1/4, ATST_MAXS2-ATST_HEADSIZE );

			ent->client->damageBoxHandle_RLeg = dmgBox->s.number;
			dmgBox->damageRedirect = DAMAGEREDIRECT_RLEG;
			dmgBox->damageRedirectTo = ent->s.number;
		}
	}
	if (!ent->client->damageBoxHandle_LLeg)
	{
		gentity_t *dmgBox = CreateNewDamageBox(ent);

		if (dmgBox)
		{
			VectorSet( dmgBox->r.mins, ATST_MINS0/4, ATST_MINS1/4, ATST_MINS2 );
			VectorSet( dmgBox->r.maxs, ATST_MAXS0/4, ATST_MAXS1/4, ATST_MAXS2-ATST_HEADSIZE );

			ent->client->damageBoxHandle_LLeg = dmgBox->s.number;
			dmgBox->damageRedirect = DAMAGEREDIRECT_LLEG;
			dmgBox->damageRedirectTo = ent->s.number;
		}
	}

	if (!ent->client->damageBoxHandle_Head ||
		!ent->client->damageBoxHandle_LLeg ||
		!ent->client->damageBoxHandle_RLeg)
	{
		return;
	}

	VectorCopy(ent->client->ps.origin, headOrg);
	headOrg[2] += (ATST_MAXS2-ATST_HEADSIZE);

	VectorCopy(ent->client->ps.viewangles, flatAngle);
	flatAngle[PITCH] = 0;
	flatAngle[ROLL] = 0;

	AngleVectors(flatAngle, fwd, right, up);

	VectorCopy(ent->client->ps.origin, lLegOrg);
	VectorCopy(ent->client->ps.origin, rLegOrg);

	lLegOrg[0] -= right[0]*32;
	lLegOrg[1] -= right[1]*32;
	lLegOrg[2] -= right[2]*32;

	rLegOrg[0] += right[0]*32;
	rLegOrg[1] += right[1]*32;
	rLegOrg[2] += right[2]*32;

	G_SetOrigin(&g_entities[ent->client->damageBoxHandle_Head], headOrg);
	G_SetOrigin(&g_entities[ent->client->damageBoxHandle_LLeg], lLegOrg);
	G_SetOrigin(&g_entities[ent->client->damageBoxHandle_RLeg], rLegOrg);
}

int G_PlayerBecomeATST(gentity_t *ent)
{
	if (!ent || !ent->client)
	{
		return 0;
	}

	if (ent->client->ps.weaponTime > 0)
	{
		return 0;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return 0;
	}

	if (ent->client->ps.zoomMode)
	{
		return 0;
	}

	if (ent->client->ps.usingATST)
	{
		ent->client->ps.usingATST = qfalse;
		ent->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
	}
	else
	{
		ent->client->ps.usingATST = qtrue;
	}

	ent->client->ps.weaponTime = 1000;

	return 1;
}
#endif

//----------------------------------------------------------

/*QUAKED fx_runner (0 0 1) (-8 -8 -8) (8 8 8) STARTOFF ONESHOT DAMAGE
Runs the specified effect, can also be targeted at an info_notnull to orient the effect

	STARTOFF - effect starts off, toggles on/off when used
	ONESHOT - effect fires only when used
	DAMAGE - does radius damage around effect every "delay" milliseonds

	"fxFile" - name of the effect file to play
	"target" - direction to aim the effect in, otherwise defaults to up
	"target2" - uses its target2 when the fx gets triggered
	"delay"  - how often to call the effect, don't over-do this ( default 200 )
	"random" - random amount of time to add to delay, ( default 0, 200 = 0ms to 200ms )
	"splashRadius" - only works when damage is checked ( default 16 )
	"splashDamage" - only works when damage is checked ( default 5 )
	"soundset"	- bmodel set to use, plays start sound when toggled on, loop sound while on ( doesn't play on a oneshot), and a stop sound when turned off
*/
#define FX_RUNNER_RESERVED 0x800000
#define FX_ENT_RADIUS 32
extern int	BMS_START;
extern int	BMS_MID;
extern int	BMS_END;
//----------------------------------------------------------
void fx_runner_think( gentity_t *ent )
{
	BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->r.currentOrigin );
	BG_EvaluateTrajectory( &ent->s.apos, level.time, ent->r.currentAngles );

	// call the effect with the desired position and orientation
	if (ent->s.isPortalEnt)
	{
//		G_AddEvent( ent, EV_PLAY_PORTAL_EFFECT_ID, ent->genericValue5 );
	}
	else
	{
//		G_AddEvent( ent, EV_PLAY_EFFECT_ID, ent->genericValue5 );
	}

	// start the fx on the client (continuous)
	ent->s.modelindex2 = FX_STATE_CONTINUOUS;

	VectorCopy(ent->r.currentAngles, ent->s.angles);
	VectorCopy(ent->r.currentOrigin, ent->s.origin);

	ent->nextthink = level.time + ent->delay + Q_flrand(0.0f, 1.0f) * ent->random;

	// GalaxyRP fix: [Entity System] an fx_runner with "delay" and "random" both unset re-thinks on
	// the very next frame, so one carrying the damage spawnflag applied its full G_RadiusDamage
	// every frame for as long as it lived. Because zyk also added splashdamage/splashradius to the
	// entity key table, that made "/entadd fx_runner spawnflags 4 splashdamage 1000 splashradius
	// 4000" a map-wide killing field, and it did the same for any preset or map shipping one.
	//
	// The damage tick is floored at RP_FX_RUNNER_MIN_DAMAGE_DELAY. NOTE this is a balance change as
	// well as a safety one: the Duelist Vertical DFA (zyk_vertical_dfa_effect in g_main.c -- 130
	// damage over a 600 radius, alive 600ms) and the quest magic effects all run with delay 0, so
	// they now tick far fewer times than they did. That was chosen deliberately.
	if ( ent->spawnflags & 4 ) // damage
	{
		if ((ent->nextthink - level.time) < RP_FX_RUNNER_MIN_DAMAGE_DELAY)
			ent->nextthink = level.time + RP_FX_RUNNER_MIN_DAMAGE_DELAY;

		G_RadiusDamage( ent->r.currentOrigin, ent, ent->splashDamage, ent->splashRadius, ent, ent, MOD_UNKNOWN );
	}

	if ( ent->target2 && ent->target2[0] )
	{
		// let our target know that we have spawned an effect
		G_UseTargets2( ent, ent, ent->target2 );
	}

	if ( !(ent->spawnflags & 2 ) && !ent->s.loopSound ) // NOT ONESHOT...this is an assy thing to do
	{
		if ( ent->soundSet && ent->soundSet[0] )
		{
			ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
			ent->s.loopIsSoundset = qtrue;
			ent->s.loopSound = BMS_MID;
		}
	}

	// zyk: Super Beam. Traces enemies and damages them
	if (Q_stricmp(ent->targetname, "zyk_super_beam") == 0)
	{
		gentity_t *user_ent = ent->parent;
		gentity_t *target_ent = NULL;
		trace_t		tr;
		vec3_t		tfrom, tto, fwd;
		vec3_t		shot_mins, shot_maxs;
		int radius = 32768;

		// GalaxyRP fix: [stability] this whole half used to run unguarded, and its first act was to
		// read user_ent->s.number for the trace's pass-entity. ent->parent is the ONLY source of
		// user_ent, and nothing can supply one any more: zyk_super_beam() -- the function that did
		// "new_ent->parent = ent" -- went with the eleven other orphans behind the removed
		// custom-quest-NPC dispatch (see the note in g_main.c's G_RunFrame), and "parent" is not in
		// fields[] (g_spawn.c), so no map, no .ent preset and no /entadd or /entedit key can set it
		// either. So user_ent was NULL for every fx_runner in the game, and gentity_t begins with
		// entityState_t s, which begins with int number -- user_ent->s.number was a read of address
		// zero, i.e. an immediate server crash, not a bad value.
		//
		// That was reachable three ways, the last of them the dangerous one:
		//   /entadd fx_runner targetname zyk_super_beam   (ADM_ENTITYSYSTEM)
		//   /entedit <id> targetname zyk_super_beam       (converts an existing map fx_runner)
		//   a saved preset -- /entsave round-trips the targetname, and g_main.c loads
		//   GalaxyRP/entities/<map>/default.txt automatically ~1s into every map start, with nobody
		//   typing anything. One experiment saved to a preset made that map permanently unbootable.
		//
		// The guard also covers a second, separate hole: the non-client G_Damage below tested
		// "user_ent != target_ent", which is TRUE when user_ent is NULL, and never tested user_ent
		// itself -- so it would have passed NULL as both inflictor and attacker. G_Damage has no
		// NULL substitution at entry here or in stock TaystJK; it survives one only if every
		// internal use happens to be guarded, which is not something to rely on.
		//
		// An ownerless beam now renders and keeps its 100ms cadence (that assignment stays OUTSIDE
		// this guard deliberately -- moving it in would change the tick rate of any map-placed beam
		// to the generic delay default of 200ms) and simply deals no damage, there being nobody to
		// credit it to. The block is otherwise untouched, so it works again unchanged the day
		// something sets parent on an fx_runner once more. The "user_ent &&" inside the first
		// damage branch is redundant now; it is left exactly as it was rather than tidied, so this
		// diff reads as the guard it is and nothing else.
		if (user_ent)
		{
			VectorCopy(ent->s.origin, tfrom);
			AngleVectors(ent->s.angles, fwd, NULL, NULL);
			tto[0] = tfrom[0] + fwd[0] * radius;
			tto[1] = tfrom[1] + fwd[1] * radius;
			tto[2] = tfrom[2] + fwd[2] * radius;

			VectorSet(shot_mins, -20, -20, -20);
			VectorSet(shot_maxs, 20, 20, 20);

			trap->Trace(&tr, tfrom, shot_mins, shot_maxs, tto, user_ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

			if (tr.fraction != 1.0 &&
				tr.entityNum != ENTITYNUM_NONE)
			{ // zyk: actually hit something
				target_ent = &g_entities[tr.entityNum];
			}

			if (target_ent && target_ent->client && user_ent && user_ent->client && user_ent != target_ent &&
				zyk_is_ally(user_ent, target_ent) == qfalse)
			{ // zyk: if the enemy is hit by the super beam, damage him
				G_Damage(target_ent, user_ent, user_ent, NULL, target_ent->client->ps.origin, 28, DAMAGE_NO_PROTECTION, MOD_CONC_ALT);
			}
			else if (target_ent && user_ent != target_ent && !target_ent->client && target_ent->health > 0 && target_ent->takedamage == qtrue)
			{ // zyk: non-client damageable entity
				G_Damage(target_ent, user_ent, user_ent, NULL, target_ent->r.currentOrigin, 28, DAMAGE_NO_PROTECTION, MOD_CONC_ALT);
			}
		}

		ent->nextthink = level.time + 100;
	}
}

//----------------------------------------------------------
void fx_runner_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if (self->s.isPortalEnt)
	{ //rww - mark it as broadcast upon first use if it's within the area of a skyportal
		self->r.svFlags |= SVF_BROADCAST;
	}

	if ( self->spawnflags & 2 ) // ONESHOT
	{
		// call the effect with the desired position and orientation, as a safety thing,
		//	make sure we aren't thinking at all.
		int		saveState = self->s.modelindex2 + 1;

		fx_runner_think( self );
		self->nextthink = -1;
		// one shot indicator
		self->s.modelindex2 = saveState;
		if (self->s.modelindex2 > FX_STATE_ONE_SHOT_LIMIT)
		{
			self->s.modelindex2 = FX_STATE_ONE_SHOT;
		}

		if ( self->target2 )
		{
			// let our target know that we have spawned an effect
			G_UseTargets2( self, self, self->target2 );
		}

		if ( self->soundSet && self->soundSet[0] )
		{
			self->s.soundSetIndex = G_SoundSetIndex(self->soundSet);
			G_AddEvent( self, EV_BMODEL_SOUND, BMS_START);
		}
	}
	else
	{
		// ensure we are working with the right think function
		self->think = fx_runner_think;

		// toggle our state
		if ( self->nextthink == -1 )
		{
			// NOTE: we fire the effect immediately on use, the fx_runner_think func will set
			//	up the nextthink time.
			fx_runner_think( self );

			if ( self->soundSet && self->soundSet[0] )
			{
				self->s.soundSetIndex = G_SoundSetIndex(self->soundSet);
				G_AddEvent( self, EV_BMODEL_SOUND, BMS_START);
				self->s.loopSound = BMS_MID;
				self->s.loopIsSoundset = qtrue;
			}
		}
		else
		{
			// turn off for now
			self->nextthink = -1;

			// turn off fx on client
			self->s.modelindex2 = FX_STATE_OFF;

			if ( self->soundSet && self->soundSet[0] )
			{
				self->s.soundSetIndex = G_SoundSetIndex(self->soundSet);
				G_AddEvent( self, EV_BMODEL_SOUND, BMS_END );
				self->s.loopSound = 0;
				self->s.loopIsSoundset = qfalse;
			}
		}
	}
}

//----------------------------------------------------------
void fx_runner_link( gentity_t *ent )
{
	vec3_t	dir;

	if ( ent->target && ent->target[0] )
	{
		// try to use the target to override the orientation
		gentity_t	*target = NULL;

		target = G_Find( target, FOFS(targetname), ent->target );

		if ( !target )
		{
			// Bah, no good, dump a warning, but continue on and use the UP vector
			Com_Printf( "fx_runner_link: target specified but not found: %s\n", ent->target );
			Com_Printf( "  -assuming UP orientation.\n" );
		}
		else
		{
			// Our target is valid so let's override the default UP vector
			VectorSubtract( target->s.origin, ent->s.origin, dir );
			VectorNormalize( dir );
			vectoangles( dir, ent->s.angles );
		}
	}

	// don't really do anything with this right now other than do a check to warn the designers if the target2 is bogus
	if ( ent->target2 && ent->target2[0] )
	{
		gentity_t	*target = NULL;

		target = G_Find( target, FOFS(targetname), ent->target2 );

		if ( !target )
		{
			// Target2 is bogus, but we can still continue
			Com_Printf( "fx_runner_link: target2 was specified but is not valid: %s\n", ent->target2 );
		}
	}

	G_SetAngles( ent, ent->s.angles );

	if ( ent->spawnflags & 1 || ent->spawnflags & 2 ) // STARTOFF || ONESHOT
	{
		// We won't even consider thinking until we are used
		ent->nextthink = -1;
	}
	else
	{
		if ( ent->soundSet && ent->soundSet[0] )
		{
			ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
			ent->s.loopSound = BMS_MID;
			ent->s.loopIsSoundset = qtrue;
		}

		// Let's get to work right now!
		ent->think = fx_runner_think;

		if (Q_stricmp(ent->targetname, "zyk_super_beam") == 0)
		{ // zyk: starts the super beam effect right now
			ent->s.modelindex2 = FX_STATE_CONTINUOUS;
			ent->nextthink = level.time + 100; // wait a small bit, then start working
			G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/artus/artus_gen.wav"));
		}
		else if (Q_stricmp(ent->targetname, "zyk_force_storm") == 0)
		{ // zyk: starts the force storm effect right now
			ent->s.modelindex2 = FX_STATE_CONTINUOUS;
			ent->nextthink = level.time + 100; // wait a small bit, then start working
			G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1.mp3"));
		}
		else if (Q_stricmp(ent->targetname, "zyk_effect_force_dash") == 0)
		{ // zyk: starts the Fast Dash effect right now
			ent->s.modelindex2 = FX_STATE_CONTINUOUS;
			ent->nextthink = level.time + 100; // wait a small bit, then start working
		}
		// GalaxyRP fix: [Magic] the zyk_quest_effect_* targetnames (enemy_nerf, magic_disable,
		// rockfall, watersplash, sleeping, time, poison, sand, immunity, flaming_area_hit, chaos)
		// used to get their own start cadence here. Only zyk_quest_effect_spawn() ever created an
		// fx_runner with one of those names, and it is gone with the magic engine.
		else
		{
			ent->nextthink = level.time + 200; // wait a small bit, then start working
		}
	}

	// make us useable if we can be targeted
	if ( ent->targetname && ent->targetname[0] )
	{
		ent->use = fx_runner_use;
	}
}

static qboolean RP_WeatherRefusedAtRuntime( gentity_t *ent );
static qboolean RP_WeatherEntityText( gentity_t *ent, const char *text, int *index );

//----------------------------------------------------------
void SP_fx_runner( gentity_t *ent )
{
	char *fxFile;

	G_SpawnString( "fxFile", "", &fxFile );
	// Get our defaults
	G_SpawnInt( "delay", "200", &ent->delay );
	G_SpawnFloat( "random", "0", &ent->random );

	if (!ent->splashRadius) // zyk: set this only if it has not been set yet
		G_SpawnInt( "splashRadius", "16", &ent->splashRadius );

	if (!ent->splashDamage) // zyk: set this only if it has not been set yet
		G_SpawnInt( "splashDamage", "5", &ent->splashDamage );

	if (!ent->s.angles[0] && !ent->s.angles[1] && !ent->s.angles[2])
	{
		// didn't have angles, so give us the default of up
		VectorSet( ent->s.angles, -90, 0, 0 );
	}

	/* zyk: commented this. Entity system must allow the player to set it
	if ( !fxFile || !fxFile[0] )
	{
		Com_Printf( S_COLOR_RED"ERROR: fx_runner %s at %s has no fxFile specified\n", ent->targetname, vtos(ent->s.origin) );
		G_FreeEntity( ent );
		return;
	}
	*/

	// DAJ_RP: [Weather] an fxFile starting with * is not an effect: it is a weather command, sent to every
	// player as zyk_weather's is, and held to zyk_weather's rules (RP_WeatherEntityText) -- the map's own
	// too: one of the weather effects, built from its name and numbers, and within the map's limits, or
	// refused. Kept, it is weather and nothing else: switched off for good, with nothing to think or be
	// used for. Left on, every player's game tried to load "effects/*rain.efx" as an effect file every delay
	// ms while it was in sight, and printed that it failed each time -- and switching it on or off never
	// touched the weather. Its slot is a plain one, never the Entity System's to reuse (see RP_WeatherAdd).
	if ( fxFile && fxFile[0] == '*' )
	{
		int index = 0;

		if ( RP_WeatherRefusedAtRuntime( ent ) || !RP_WeatherEntityText( ent, fxFile, &index ) )
		{
			return;
		}

		ent->s.modelindex = index;
		ent->message = G_NewString( fxFile );	// zyk: what /entsave writes back
		ent->s.eType = ET_FX;
		ent->s.modelindex2 = FX_STATE_OFF;
		ent->think = NULL;
		ent->nextthink = 0;
		ent->use = NULL;

		G_SetOrigin( ent, ent->s.origin );
		VectorSet( ent->r.maxs, FX_ENT_RADIUS, FX_ENT_RADIUS, FX_ENT_RADIUS );
		VectorScale( ent->r.maxs, -1, ent->r.mins );
		trap->LinkEntity( (sharedEntity_t *)ent );
		return;
	}

	// Try and associate an effect file, unfortunately we won't know if this worked or not
	//	until the cgame trys to register it...
	if (fxFile && fxFile[0]) // zyk: added this condition
	{
		// GalaxyRP: [Slot Reuse] reusable once the Entity System's fx_runners using it are gone
		ent->s.modelindex = RP_EntityEffectIndex( ent, fxFile );
		ent->message = G_NewString(fxFile); // zyk: used by Entity System to save the effect fxFile, so the effect is loaded properly by entload command

	}

	// important info transmitted
	ent->s.eType = ET_FX;
	ent->s.speed = ent->delay;
	ent->s.time = ent->random;
	ent->s.modelindex2 = FX_STATE_OFF;

	// Give us a bit of time to spawn in the other entities, since we may have to target one of 'em
	ent->think = fx_runner_link;

	// zyk: no need to wait 400 ms with these effects
	// GalaxyRP fix: [Magic] the six zyk_quest_effect_* names have left this list too (see fx_runner_link).
	if (Q_stricmp(ent->targetname, "zyk_super_beam") == 0 || Q_stricmp(ent->targetname, "zyk_force_storm") == 0 || 
		Q_stricmp(ent->targetname, "zyk_effect_force_dash") == 0 || Q_stricmp(ent->targetname, "zyk_vertical_dfa") == 0)
	{
		// GalaxyRP fix: [Entity System] these used to link on the very next frame. The newer Zyk mod
		// gives its own equivalent list a 100ms delay instead of zero; adopted here. This only moves
		// the link step -- fx_runner_link() resolves the orientation and then picks the real cadence
		// per effect -- so an effect starts 100ms later and nothing about how often it runs changes.
		// The 400ms below is vanilla's own delay for every other fx_runner and is left alone.
		ent->nextthink = level.time + 100;
	}
	else
	{
		ent->nextthink = level.time + 400;
	}

	// Save our position and link us up!
	G_SetOrigin( ent, ent->s.origin );

	VectorSet( ent->r.maxs, FX_ENT_RADIUS, FX_ENT_RADIUS, FX_ENT_RADIUS );
	VectorScale( ent->r.maxs, -1, ent->r.mins );

	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*QUAKED fx_wind (0 .5 .8) (-16 -16 -16) (16 16 16) NORMAL CONSTANT GUSTING SWIRLING x  FOG LIGHT_FOG
Generates global wind forces

NORMAL    creates a random light global wind
CONSTANT  forces all wind to go in a specified direction
GUSTING   causes random gusts of wind
SWIRLING  causes random swirls of wind

"angles" the direction for constant wind
"speed"  the speed for constant wind
*/
// GalaxyRP fix: [Entity System] a weather entity added after map load registers a global *weather
// command, which G_EffectIndex() refuses (and logs once) while /admweather holds the map's weather
// slots -- the admin read "Entity N spawned" for an entity that did nothing. Say so, and free it.
// DAJ_RP: [Weather] a map's own too -- one tagged E, edited and so spawned again, the only way a map's own
// spawns that late -- through RP_WeatherRefuse below
static void RP_WeatherRefuse( gentity_t *ent, const char *reason );
static qboolean RP_WeatherRefusedAtRuntime( gentity_t *ent )
{
	if ( level.zyk_weather_slot == 0 )
		return qfalse;
	RP_WeatherRefuse( ent, "weather is managed with /admweather on this map (/admweather add); a weather entity added now would show a different sky to players who join later" );
	return qtrue;
}

/*
==================
RP_WeatherRefuse

DAJ_RP: [Weather] a weather entity refused: one the Entity System made is freed and the admin told why
(RP_RefuseAtRuntime); a map's own -- its entity string or the per-map fixes -- is left out of the map the
same way, with no weather sent, and the reason goes to the server log.
==================
*/
static void RP_WeatherRefuse( gentity_t *ent, const char *reason )
{
	if ( RP_RefuseAtRuntime( ent, reason ) )
		return;

	G_LogPrintf( "%s at %s left out of the map's weather: %s\n", ent->classname ? ent->classname : "noclass", vtos( ent->s.origin ), reason );
	G_FreeEntity( ent );
}

/*
==================
RP_WeatherAdd

DAJ_RP: [Weather] the weather a weather entity adds to the map -- one the Entity System made (/entadd,
/entedit, a copy, a line of an entity file) or the map's own, which are held to the same rules: names,
in the order its spawn function registers them -- a plain effect among them (fx_rain's acid fizz) is
registered along with them and is not weather. The map's weather is every weather command in the effect table, the map's own included, counted the way /admweather counts
what it keeps as the map's own for /admweather default (zyk_weather_claim_block); the map's own, spawning
in order, are counted in the order every player's game runs them. The entity is refused -- freed, with
the reason for the admin or, for a map's own, the server log (RP_WeatherRefuse) -- when
  - none of its names is weather;
  - every one of its weather commands is another weather entity's, still in the world: the engine hands
    back the slot it gave out before and sends nothing, so it would add nothing. A command in the table
    that no weather entity holds any more -- its entity was removed, or this is the same entity spawned
    again by /entedit or /entload -- is taken back rather than refused: the weather is still showing, and
    this entity is what keeps it in the map's saved weather;
  - the map's weather would then hold more than ZYK_WEATHER_MAX_BASE commands (what /admweather default
    can bring back), ZYK_WEATHER_MAX_CLOUDS particle effects or ZYK_WEATHER_MAX_WINDS winds (what the
    renderer keeps -- past that it drops them without a word, on the players' side). Commands already in
    the table are counted once, already;
  - the table, or the gamestate, has no room for them (RP_SlotRoomFor).
Weather slots are never the Entity System's to reuse: a command taken out of the table would leave players
who joined before with weather players joining later never see. qfalse: refused. index, when given, is the
slot of the last weather command.
==================
*/
#define RP_WEATHER_ENTITY_NAMES		6	// the most one weather entity registers (fx_wind with every flag: five)

// DAJ_RP: [Weather] the slot a name has in the effect table, 0 for none
static int RP_WeatherSlotOf( const char *name )
{
	char s[MAX_STRING_CHARS];
	int i;

	for ( i = 1; i < MAX_FX; i++ )
	{
		trap->GetConfigstring( CS_EFFECTS + i, s, sizeof( s ) );
		if ( !s[0] )
			break;
		if ( !strcmp( s, name ) )
			return i;
	}

	return 0;
}

// DAJ_RP: [Weather] the weather entity in the world, other than ent, that added the command in this slot;
// NULL for none
static gentity_t *RP_WeatherHolder( int slot, const gentity_t *ent )
{
	gentity_t *other;

	if ( slot <= 0 || slot >= 64 )
		return NULL;

	RP_FOR_EACH_ENTITY( other )
	{
		if ( other->inuse && other != ent && ( other->rpWeatherSlots & ( (uint64_t)1 << slot ) ) )
			return other;
	}

	return NULL;
}

// DAJ_RP: [Weather] the slots of what a weather entity added, so another asking for the same weather is
// told it would add nothing (RP_WeatherHolder)
static void RP_WeatherNoteSlot( gentity_t *ent, int slot )
{
	if ( slot > 0 && slot < 64 )
		ent->rpWeatherSlots |= ( (uint64_t)1 << slot );
}

static qboolean RP_WeatherAdd( gentity_t *ent, const char **names, int count, int *index )
{
	char s[MAX_STRING_CHARS];
	char reason[256];
	int total = 0, clouds = 0, winds = 0;
	int weather = 0, held = 0, fresh = 0, freshClouds = 0, freshWinds = 0;
	gentity_t *holder = NULL;
	const char *last = NULL;
	int i, j;

	if ( index )
		*index = 0;

	for ( i = 1; i < MAX_FX; i++ )
	{
		trap->GetConfigstring( CS_EFFECTS + i, s, sizeof( s ) );
		if ( !s[0] )
			break;
		switch ( RP_WeatherCommandKind( s ) )
		{
		case RP_WEATHER_CLOUD: total++; clouds++; break;
		case RP_WEATHER_WIND: total++; winds++; break;
		default: break;
		}
	}

	for ( i = 0; i < count; i++ )
	{
		int kind = RP_WeatherCommandKind( names[i] );
		int slot;
		qboolean again = qfalse;

		if ( kind == RP_WEATHER_NONE )
			continue;

		for ( j = 0; j < i && !again; j++ )
		{
			if ( !strcmp( names[j], names[i] ) )
				again = qtrue;
		}
		if ( again )
			continue;

		weather++;
		last = names[i];

		slot = RP_WeatherSlotOf( names[i] );
		if ( slot )
		{
			gentity_t *other = RP_WeatherHolder( slot, ent );

			if ( other )
			{
				held++;
				holder = other;
			}
			continue;
		}

		fresh++;
		if ( kind == RP_WEATHER_CLOUD )
			freshClouds++;
		else
			freshWinds++;
	}

	if ( weather == 0 )
	{
		RP_WeatherRefuse( ent, "it adds no weather with these spawnflags" );
		return qfalse;
	}

	if ( held == weather )
	{
		RP_WeatherRefuse( ent, ( weather == 1 ) ?
			va( "%s is already added by entity %d, so this would add nothing", last + 1, holder->s.number ) :
			va( "all of its weather is already added by other entities (entity %d), so this would add nothing", holder->s.number ) );
		return qfalse;
	}

	if ( total + fresh > ZYK_WEATHER_MAX_BASE )
	{
		RP_WeatherRefuse( ent, va( "this map's weather already has %d weather command(s) and this would add %d; %d is the most /admweather default can bring back",
			total, fresh, ZYK_WEATHER_MAX_BASE ) );
		return qfalse;
	}

	if ( clouds + freshClouds > ZYK_WEATHER_MAX_CLOUDS )
	{
		RP_WeatherRefuse( ent, va( "players' games draw at most %d particle effects (rain, snow, sand, fog, dust), and this map's weather already has %d",
			ZYK_WEATHER_MAX_CLOUDS, clouds ) );
		return qfalse;
	}

	if ( winds + freshWinds > ZYK_WEATHER_MAX_WINDS )
	{
		RP_WeatherRefuse( ent, va( "players' games keep at most %d winds, and this map's weather already has %d",
			ZYK_WEATHER_MAX_WINDS, winds ) );
		return qfalse;
	}

	if ( !RP_SlotRoomFor( CS_EFFECTS, names, count, 0, reason, sizeof( reason ) ) )
	{
		RP_WeatherRefuse( ent, reason );
		return qfalse;
	}

	ent->rpWeatherSlots = 0;

	for ( i = 0; i < count; i++ )
	{
		int slot = G_EffectIndex( names[i] );

		if ( RP_WeatherCommandKind( names[i] ) == RP_WEATHER_NONE )
			continue;

		if ( !slot )
		{
			// zyk: RP_SlotRoomFor() found the room, so something took it in between; what went in stays
			RP_WeatherRefuse( ent, "the effect table had no room left for its weather" );
			return qfalse;
		}

		RP_WeatherNoteSlot( ent, slot );
		if ( index )
			*index = slot;
	}

	return qtrue;
}

/*
==================
RP_WeatherEntityText

DAJ_RP: [Weather] zyk_weather's message, or an fx_runner's fxFile starting with *, from an entity the
Entity System made: the command it names (RP_WeatherCommandFromText, g_cmds.c), added by RP_WeatherAdd.
qfalse: refused, and freed.
==================
*/
static qboolean RP_WeatherEntityText( gentity_t *ent, const char *text, int *index )
{
	char command[ZYK_WEATHER_CMD_LENGTH];
	char why[256];
	const char *names[1];

	if ( !RP_WeatherCommandFromText( text, command, sizeof( command ), why, sizeof( why ) ) )
	{
		RP_WeatherRefuse( ent, why );
		return qfalse;
	}

	names[0] = command;
	return RP_WeatherAdd( ent, names, 1, index );
}

void SP_CreateWind( gentity_t *ent )
{
	char	temp[256];
	const char *names[RP_WEATHER_ENTITY_NAMES];
	int		count = 0;

	if ( RP_WeatherRefusedAtRuntime( ent ) )
	{
		return;
	}

	// Normal Wind
	//-------------
	if ( ent->spawnflags & 1 )
	{
		names[count++] = "*wind";
	}

	// Constant Wind
	//---------------
	if ( ent->spawnflags & 2 )
	{
		vec3_t	windDir;
		AngleVectors( ent->s.angles, windDir, 0, 0 );
		G_SpawnFloat( "speed", "500", &ent->speed );
		VectorScale( windDir, ent->speed, windDir );

		// DAJ_RP: [Weather] /admweather's wind: whole numbers, each held to ZYK_WEATHER_WIND_LIMIT (it was
		// sent with six decimals each, which the renderer reads the same, give or take half a unit)
		Com_sprintf( temp, sizeof(temp), "*constantwind ( %d %d %d )",
			Com_Clampi( -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, (int)floor( windDir[0] + 0.5f ) ),
			Com_Clampi( -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, (int)floor( windDir[1] + 0.5f ) ),
			Com_Clampi( -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, (int)floor( windDir[2] + 0.5f ) ) );
		names[count++] = temp;
	}

	// Gusting Wind
	//--------------
	if ( ent->spawnflags & 4 )
	{
		names[count++] = "*gustingwind";
	}

	// Swirling Wind
	//---------------
	/*if ( ent->spawnflags & 8 )
	{
		G_EffectIndex( "*swirlingwind" );
	}*/


	// MISTY FOG
	//===========
	if ( ent->spawnflags & 32 )
	{
		names[count++] = "*fog";
	}

	// MISTY FOG
	//===========
	if ( ent->spawnflags & 64 )
	{
		names[count++] = "*light_fog";
	}

	RP_WeatherAdd( ent, names, count, NULL );
}

/*QUAKED fx_spacedust (1 0 0) (-16 -16 -16) (16 16 16)
This world effect will spawn space dust globally into the level.

"count" the number of snow particles (default of 1000)
*/
//----------------------------------------------------------
void SP_CreateSpaceDust( gentity_t *ent )
{
	if ( RP_WeatherRefusedAtRuntime( ent ) )
	{
		return;
	}

	// DAJ_RP: [Weather] the 1000 the comment above promises, which the code never gave: without a count it
	// sent "*spacedust 0" and showed nothing. Given one, it is held to ZYK_WEATHER_DUST_MIN..MAX -- a
	// negative one crashed every player (see RP_WeatherSafeName, g_utils.c).
	{
		char *given = NULL;
		char dust[ZYK_WEATHER_CMD_LENGTH];
		const char *names[1];

		G_SpawnString( "count", "", &given );
		ent->count = ( given && given[0] ) ? Com_Clampi( ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX, ent->count ) : ZYK_WEATHER_DUST_DEFAULT;

		Com_sprintf( dust, sizeof( dust ), "*spacedust %i", ent->count );
		names[0] = dust;
		RP_WeatherAdd( ent, names, 1, NULL );
	}
	//G_EffectIndex("*constantwind ( 10 -10 0 )");
}


/*QUAKED fx_snow (1 0 0) (-16 -16 -16) (16 16 16)
This world effect will spawn snow globally into the level.

"count" the number of snow particles (default of 1000)
*/
//----------------------------------------------------------
void SP_CreateSnow( gentity_t *ent )
{
	const char *names[3] = { "*snow", "*fog", "*constantwind ( 100 100 -100 )" };

	if ( RP_WeatherRefusedAtRuntime( ent ) )
	{
		return;
	}

	RP_WeatherAdd( ent, names, 3, NULL );
}

/*QUAKED fx_rain (1 0 0) (-16 -16 -16) (16 16 16) LIGHT MEDIUM HEAVY ACID x MISTY_FOG
This world effect will spawn rain globally into the level.

LIGHT   create light drizzle
MEDIUM  create average medium rain
HEAVY   create heavy downpour (with fog)
ACID    create acid rain

MISTY_FOG      causes clouds of misty fog to float through the level
*/
//----------------------------------------------------------
void SP_CreateRain( gentity_t *ent )
{
	const char *names[RP_WEATHER_ENTITY_NAMES];
	int count = 0;

	if ( RP_WeatherRefusedAtRuntime( ent ) )
	{
		return;
	}

	if ( ent->spawnflags == 0 )
	{
		names[count++] = "*rain";
	}
	else
	{
		// Different Types Of Rain
		//-------------------------
		if ( ent->spawnflags & 1 )
		{
			names[count++] = "*lightrain";
		}
		else if ( ent->spawnflags & 2 )
		{
			names[count++] = "*rain";
		}
		else if ( ent->spawnflags & 4 )
		{
			names[count++] = "*heavyrain";

			// Automatically Get Heavy Fog
			//-----------------------------
			names[count++] = "*heavyrainfog";
		}
		else if ( ent->spawnflags & 8 )
		{
			names[count++] = "world/acid_fizz";
			names[count++] = "*acidrain";
		}

		// MISTY FOG
		//===========
		if ( ent->spawnflags & 32 )
		{
			names[count++] = "*fog";
		}
	}

	RP_WeatherAdd( ent, names, count, NULL );
}

/*QUAKED zyk_weather (1 0 0) (-16 -16 -16) (16 16 16)
This world effect will spawn weather globally into the level.

"message" the weather type
*/
//----------------------------------------------------------
void SP_CreateWeather( gentity_t *ent )
{
	if ( RP_WeatherRefusedAtRuntime( ent ) )
	{
		return;
	}

	// DAJ_RP: [Weather] the map's saved weather: its message has to name one of the weather effects, which
	// is then built and added under the map's limits (RP_WeatherEntityText, RP_WeatherAdd) -- for a map's
	// own as for one the Entity System made. It used to go to the players as typed; see
	// RP_WeatherCommandFromText in g_cmds.c for what that did. ("rain" was sent as "*rain init 500", which
	// the renderer reads as plain rain.) (The "mins" and "maxs" keys the comment above used to list were
	// never read.)
	//
	// GalaxyRP fix: [Entity System] "message" is what names the weather effect, and nothing required it --
	// the fallback formatted a NULL message through va("*%s") and registered "*(null)". No weather type,
	// no entity.
	if ( !VALIDSTRING( ent->message ) )
	{
		RP_WeatherRefuse( ent, "it needs a message key, the weather it adds (see /enthelp zyk_weather)" );
		return;
	}

	RP_WeatherEntityText( ent, ent->message, NULL );
}

/* zyk: zyk_regen_unit regens many things
spawnflags:
1 -  regens health
2 -  regens shield
4 -  regens force
8 -  used to regen magic power; the magic system is gone, so this flag does nothing now

"count" amount to regen
"wait" amount of time between regens (in miliseconds)
"mins" bounding box
"maxs" bounding box
*/
void zyk_regen_unit_think(gentity_t *ent)
{
	gentity_t *this_ent = NULL;
	int entity_ids[MAX_GENTITIES];
	int numListedEntities = 0;
	int i = 0;
	vec3_t mins, maxs;

	VectorSet(mins, ent->s.origin[0] + ent->r.mins[0], ent->s.origin[1] + ent->r.mins[1], ent->s.origin[2] + ent->r.mins[2]);
	VectorSet(maxs, ent->s.origin[0] + ent->r.maxs[0], ent->s.origin[1] + ent->r.maxs[1], ent->s.origin[2] + ent->r.maxs[2]);

	numListedEntities = trap->EntitiesInBox( mins, maxs, entity_ids, MAX_GENTITIES );

	i = 0;
	while (i < numListedEntities)
	{
		this_ent = &g_entities[entity_ids[i]];

		if (this_ent && this_ent->client && this_ent->s.number < MAX_CLIENTS && this_ent->health > 0)
		{ // zyk: must be a player that is alive
			// GalaxyRP fix: [Entity System] each of the four tests below used to add count to the
			// player's current value and compare the sum against the maximum. "count" comes straight
			// off an /entadd, so a large one overflowed the int BEFORE the comparison: the sum wrapped
			// negative, read as under the maximum, and the "+=" then wrapped the player's health,
			// armour, force or magic power negative. Nothing here goes through G_Damage, so a player
			// driven below zero this way never died, never respawned and just lay there -- which is
			// exactly what the count < 0 clamp in SP_ZykRegenUnit was written to prevent, reached
			// from the other end of the range.
			//
			// Widening the sum is the whole fix: (long long)a + b cannot overflow for any pair of
			// ints, and every outcome is otherwise identical, so nothing about how a regen unit
			// behaves changes. add_credits() in g_cmds.c already guards its own arithmetic this way.
			// NOTE comparing the headroom instead -- count < (max - current) -- looks tidier and is
			// wrong: that subtraction overflows in its own right once the current value is far
			// enough below the maximum, which is precisely the state the old bug could leave a
			// player in.
			if (ent->spawnflags & 1)
			{
				if (((long long)this_ent->health + ent->count) < this_ent->client->ps.stats[STAT_MAX_HEALTH])
					this_ent->health += ent->count;
				else
					this_ent->health = this_ent->client->ps.stats[STAT_MAX_HEALTH];
			}

			if (ent->spawnflags & 2)
			{
				int max_shield = this_ent->client->ps.stats[STAT_MAX_HEALTH];
				if (this_ent->client->sess.amrpgmode == 2)
					max_shield = this_ent->client->pers.max_rpg_shield;

				if (((long long)this_ent->client->ps.stats[STAT_ARMOR] + ent->count) < max_shield)
					this_ent->client->ps.stats[STAT_ARMOR] += ent->count;
				else
					this_ent->client->ps.stats[STAT_ARMOR] = max_shield;
			}

			if (ent->spawnflags & 4)
			{
				if (((long long)this_ent->client->ps.fd.forcePower + ent->count) < this_ent->client->ps.fd.forcePowerMax)
					this_ent->client->ps.fd.forcePower += ent->count;
				else
					this_ent->client->ps.fd.forcePower = this_ent->client->ps.fd.forcePowerMax;
			}
		}

		i++;
	}

	ent->nextthink = level.time + ent->wait;
}

void SP_ZykRegenUnit( gentity_t *ent)
{
	// GalaxyRP fix: [Entity System] "count" is documented as the amount to regen, but nothing
	// stopped it being negative -- and zyk_regen_unit_think() adds it straight onto health and
	// armour without going through G_Damage. A negative count therefore drained a player past zero
	// with no death, no obituary and no respawn, leaving them lying there at negative health, and
	// drove armour negative too. It is a regen unit; clamp it to one.
	if (ent->count < 0)
		ent->count = 0;

	// GalaxyRP fix: [Entity System] "wait" was used unclamped, so the default of 0 made
	// nextthink == level.time and the think ran every single frame, doing a full EntitiesInBox
	// sweep each time. zyk_training_pole already clamps to 100 for exactly this reason; match it.
	if (ent->wait < 100)
		ent->wait = 100;

	// GalaxyRP fix: [Entity System] unlike the training pole this never defaulted its bounding box,
	// so an entity spawned without explicit mins/maxs got a zero-sized box at its own origin and
	// silently regenerated nobody, with no error to say why. Same default the training pole uses.
	G_SpawnVector("mins", va("-15 -15 %d", DEFAULT_MINS_2), ent->r.mins);
	G_SpawnVector("maxs", va("15 15 %d", DEFAULT_MAXS_2), ent->r.maxs);

	ent->think = zyk_regen_unit_think;
	ent->nextthink = level.time + ent->wait;

	ent->s.eType = ET_GENERAL;

	// Save our position and link us up!
	G_SetOrigin( ent, ent->s.origin );

	trap->LinkEntity( (sharedEntity_t *)ent );
}

/* zyk: zyk_training_pole adds a model that will be used as a saber training pole
spawnflags:
1 - shows amount of damage done to this entity after time in miliseconds set in wait field

"angles" rotate this entity
"wait" amount of time to wait (in miliseconds) until showing a score plum with the count value
"model" allows setting a md3 model. Sets the rift statue if no model passed
"mins"
"maxs"
*/
void zyk_training_pole_damage(gentity_t *ent)
{
	gentity_t *plum;
	gentity_t *player = NULL;
	vec3_t plum_origin;
	int i = 0;

	if (ent->count > 0)
	{ // zyk: shows damage, stored in count, done to the training pole
		VectorSet(plum_origin, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] + DEFAULT_MAXS_2);

		// GalaxyRP fix: [Entity System] a client draws a score plum only when it is addressed to it, so it
		// takes one per player -- and it was made for every connected player, wherever they were: a whole
		// table's worth of temporary entities (each holding an entity slot for its 300 ms) for every series of
		// hits on every dummy, most of them to players too far away to be sent them at all. Now one only for a
		// player who can see the spot (in its PVS, from the eye), and sent to that player alone
		// (SVF_SINGLECLIENT) rather than to everyone in range, who threw the others away.
		for (i = 0; i < level.maxclients; i++)
		{
			vec3_t eye;

			player = &g_entities[i];

			if (!player->inuse || !player->client || player->client->pers.connected != CON_CONNECTED)
				continue;

			VectorCopy(player->client->ps.origin, eye);
			eye[2] += player->client->ps.viewheight;
			if (!trap->InPVS(plum_origin, eye))
				continue;

			plum = G_TempEntity(plum_origin, EV_SCOREPLUM);

			plum->s.otherEntityNum = player->s.number;
			plum->s.time = ent->count;
			plum->r.svFlags |= SVF_SINGLECLIENT;
			plum->r.singleClient = player->s.number;
		}

		ent->count = 0;

		ent->nextthink = level.time + ent->wait;
	}
}

void SP_ZykTrainingPole(gentity_t *ent)
{
	// GalaxyRP fix: [Entity System] this used to open by setting think to zyk_regen_unit_think and
	// arming nextthink off an unclamped wait. Both were overwritten at the bottom of the function
	// with the correct zyk_training_pole_damage and the clamped wait, so neither did anything -- but
	// it read as a copy-paste bug and would have become one the moment the function was reordered.
	ent->s.eType = ET_GENERAL;

	// Save our position and link us up!
	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	// GalaxyRP fix: [Entity System] its "model" key's .md3 if the server has it, else the Rift statue (logged
	// for a map's own whose key is not there; the Entity System refuses one before it spawns --
	// RP_ModelKeyClass() in g_spawn.c). It took any name: a typo took a model slot for good and left an
	// invisible but solid dummy. RP_EntityModelIndex(): an Entity System dummy's slot can be reused once it
	// is gone.
	ent->s.modelindex = RP_EntityModelIndex( ent, RP_DispenserModel( ent, RP_MODEL_TRAINING_POLE ) );

	ent->r.contents = CONTENTS_SOLID | CONTENTS_OPAQUE | CONTENTS_BODY | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP;//Was CONTENTS_SOLID, but only architecture should be this
	ent->health = 1;
	ent->takedamage = qtrue;

	G_SpawnVector("mins", va("-15 -15 %d", DEFAULT_MINS_2), ent->r.mins);
	G_SpawnVector("maxs", va("15 15 %d", DEFAULT_MAXS_2), ent->r.maxs);

	// zyk: cannot be destroyed
	ent->flags |= FL_UNDYING;

	if (ent->wait < 100)
	{ // zyk: adds a minimum of 100 miliseconds
		ent->wait = 100;
	}

	ent->count = 0;
	ent->think = zyk_training_pole_damage;
	ent->nextthink = level.time + ent->wait;

	trap->LinkEntity((sharedEntity_t *)ent);
}


/* zyk: zyk_mini_game_joiner, allows a player to join a mini-game by going inside the bounding box
spawnflags:
1 -  unused (was the Sniper Battle, since removed)
2 -  unused (was Racing Mode, since removed)
4 -  Melee Battle
8 -  Duel Tournament
64 - must press Use key

"wait" amount of time between the entity frames, in which it verifies if a player is standing in the bounding box
"mins" bounding box
"maxs" bounding box
*/
extern void Cmd_MeleeMode_f(gentity_t *ent);
extern void Cmd_DuelMode_f(gentity_t *ent);
void zyk_mini_gamer_joiner_do(gentity_t *ent, gentity_t *player_ent, gentity_t *the_player_ent)
{
	if (ent->spawnflags & 4)
	{
		Cmd_MeleeMode_f(player_ent);
	}

	if (ent->spawnflags & 8)
	{
		Cmd_DuelMode_f(player_ent);
	}
}

void zyk_mini_game_joiner_think(gentity_t *ent)
{
	gentity_t *player_ent = NULL;
	int entity_ids[MAX_GENTITIES];
	int numListedEntities = 0;
	int i = 0;
	vec3_t mins, maxs;

	if (ent->spawnflags & 64) // zyk: if it is an useable entity, wait will be this value
		ent->wait = 100;

	VectorSet(mins, ent->s.origin[0] + ent->r.mins[0], ent->s.origin[1] + ent->r.mins[1], ent->s.origin[2] + ent->r.mins[2]);
	VectorSet(maxs, ent->s.origin[0] + ent->r.maxs[0], ent->s.origin[1] + ent->r.maxs[1], ent->s.origin[2] + ent->r.maxs[2]);

	numListedEntities = trap->EntitiesInBox(mins, maxs, entity_ids, MAX_GENTITIES);

	i = 0;
	while (i < numListedEntities)
	{
		player_ent = &g_entities[entity_ids[i]];

		if (player_ent && player_ent->client && player_ent->s.number < MAX_CLIENTS && player_ent->health > 0)
		{ // zyk: must be a player that is alive
			if (!(ent->spawnflags & 64))
			{
				zyk_mini_gamer_joiner_do(ent, player_ent, player_ent);
			}
			else if (player_ent->client->pers.cmd.buttons & BUTTON_USE)
			{
				zyk_mini_gamer_joiner_do(ent, player_ent, player_ent);

				if (player_ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD ||
					player_ent->client->ps.torsoAnim == BOTH_CONSOLE1)
				{ //extend the time
					player_ent->client->ps.torsoTimer = 500;
				}
				else
				{
					G_SetAnim(player_ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
				}
				player_ent->client->ps.weaponTime = player_ent->client->ps.torsoTimer;

				ent->wait = 1000;
			}
		}

		i++;
	}

	ent->nextthink = level.time + ent->wait;
}

void SP_ZykMiniGameJoiner(gentity_t *ent)
{
	ent->think = zyk_mini_game_joiner_think;

	if (ent->spawnflags & 64)
	{ // zyk: if it is an useable entity, wait will be this value
		ent->wait = 100;
	}

	// GalaxyRP fix: [Entity System] the clamp above only applied to the press-Use variant. Without
	// spawnflag 64 the default wait of 0 made nextthink == level.time, so the think ran every frame
	// and re-invoked Cmd_MeleeMode_f / Cmd_DuelMode_f for every player standing in the box, every
	// frame. Those commands guard themselves, but each answers with a server command,
	// so a player in the box was flooded with refusals at server framerate. Same floor either way.
	if (ent->wait < 100)
		ent->wait = 100;

	ent->nextthink = level.time + ent->wait;

	ent->s.eType = ET_GENERAL;

	// Save our position and link us up!
	G_SetOrigin(ent, ent->s.origin);

	trap->LinkEntity((sharedEntity_t *)ent);
}

qboolean gEscaping = qfalse;
int gEscapeTime = 0;

void Use_Target_Screenshake( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	qboolean bGlobal = qfalse;

	if (ent->genericValue6)
	{
		bGlobal = qtrue;
	}

	G_ScreenShake(ent->s.origin, NULL, ent->speed, ent->genericValue5, bGlobal);
}

void SP_target_screenshake(gentity_t *ent)
{
	G_SpawnFloat( "intensity", "10", &ent->speed );
	//intensity of the shake
	G_SpawnInt( "duration", "800", &ent->genericValue5 );
	//duration of the shake
	G_SpawnInt( "globalshake", "1", &ent->genericValue6 );
	//non-0 if shake should be global (all clients). Otherwise, only in the PVS.

	ent->use = Use_Target_Screenshake;
}

void LogExit( const char *string );

void Use_Target_Escapetrig( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	if (!ent->genericValue6)
	{
		gEscaping = qtrue;
		gEscapeTime = level.time + ent->genericValue5;
	}
	else if (gEscaping)
	{
		int i = 0;
		gEscaping = qfalse;
		while (i < MAX_CLIENTS)
		{ //all of the survivors get 100 points!
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].health > 0 &&
				g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR &&
				!(g_entities[i].client->ps.pm_flags & PMF_FOLLOW))
			{
				AddScore(&g_entities[i], g_entities[i].client->ps.origin, 100);
			}
			i++;
		}
		if (activator && activator->inuse && activator->client)
		{ //the one who escaped gets 500
			AddScore(activator, activator->client->ps.origin, 500);
		}

		LogExit("Escaped!");
	}
}

// zyk: added functions for misc_model_gun_rack entity
#define RACK_BLASTER	1
#define RACK_REPEATER	2
#define RACK_ROCKET		4

/*QUAKED misc_model_gun_rack (1 0 0.25) (-14 -14 -4) (14 14 30) BLASTER REPEATER ROCKET
model="models/map_objects/kejim/weaponsrack.md3"

NOTE: can mix and match these spawnflags to get multi-weapon racks.  If only one type is checked the rack will be full of those weapons
BLASTER - Puts one or more blaster guns on the rack.
REPEATER - Puts one or more repeater guns on the rack.
ROCKET - Puts one or more rocket launchers on the rack.
*/

// DAJ_RP: [Dispensers] rack is the rack the item is put on: the item is its child (RP_MarkChild()), so the
// entity commands that remove the rack or spawn it again in place take its items with it, and the spawn
// puts fresh ones up -- they stayed behind, floating, after /entremove, and doubled with every /entedit.
void GunRackAddItem( gentity_t *rack, gitem_t *gun, vec3_t org, vec3_t angs, float ffwd, float fright, float fup )
{
	vec3_t		fwd, right;
	gentity_t	*it_ent = G_Spawn();
	qboolean	rotate = qtrue;
	int t = 0;

	AngleVectors( angs, fwd, right, NULL );

	if ( it_ent && gun )
	{
		// FIXME: scaling the ammo will probably need to be tweaked to a reasonable amount...adjust as needed
		// Set base ammo per type
		if ( gun->giType == IT_WEAPON )
		{
			it_ent->spawnflags |= 16;// VERTICAL

			switch( gun->giTag )
			{
			case WP_BLASTER:
				it_ent->count = 15;
				break;
			case WP_REPEATER:
				it_ent->count = 100;
				break;
			case WP_ROCKET_LAUNCHER:
				it_ent->count = 4;
				break;
			}
		}
		else
		{
			rotate = qfalse;

			// must deliberately make it small, or else the objects will spawn inside of each other.
			VectorSet( it_ent->r.maxs, 6.75f, 6.75f, 6.75f );
			VectorScale( it_ent->r.maxs, -1, it_ent->r.mins );
		}

		it_ent->spawnflags |= 1;// ITMSF_SUSPEND
		it_ent->classname = G_NewString(gun->classname);	//copy it so it can be freed safely
		G_SpawnItem( it_ent, gun );

		// GalaxyRP fix: [SP Maps] G_SpawnItem() frees the entity when the weapon is disabled
		// (g_weaponDisable, disable_weapon_*) and leaves it without an item when G_ItemDisabled()
		// says so; FinishSpawningItem() on either dereferenced a NULL item and crashed the server
		// on any map with a misc_model_gun_rack.
		if ( !it_ent->inuse )
		{
			return;
		}
		if ( !it_ent->item )
		{
			G_FreeEntity( it_ent );
			return;
		}

		// FinishSpawningItem handles everything, so clear the thinkFunc that was set in G_SpawnItem
		FinishSpawningItem( it_ent );

		if ( gun->giType == IT_AMMO )
		{
			if ( gun->giTag == AMMO_BLASTER ) // I guess this just has to use different logic??
			{
				if ( g_npcspskill.integer >= 2 )
				{
					it_ent->count += 10; // give more on higher difficulty because there will be more/harder enemies?
				}
			}
			else
			{
				// scale ammo based on skill
				switch ( g_npcspskill.integer )
				{
				case 0: // do default
					break;
				case 1:
					it_ent->count *= 0.75f;
					break;
				case 2:
					it_ent->count *= 0.5f;
					break;
				}
			}
		}

		it_ent->nextthink = 0;

		VectorCopy( org, it_ent->s.origin );
		VectorMA( it_ent->s.origin, fright, right, it_ent->s.origin );
		VectorMA( it_ent->s.origin, ffwd, fwd, it_ent->s.origin );
		it_ent->s.origin[2] += fup;

		VectorCopy( angs, it_ent->s.angles );

		// by doing this, we can force the amount of ammo we desire onto the weapon for when it gets picked-up
		it_ent->flags |= FL_DROPPED_ITEM;
		it_ent->physicsBounce = 0.1f;

		for ( t = 0; t < 3; t++ )
		{
			if ( rotate )
			{
				if ( t == YAW )
				{
					it_ent->s.angles[t] = AngleNormalize180( it_ent->s.angles[t] + 180 + Q_flrand(-1.0f, 1.0f) * 14 );
				}
				else
				{
					it_ent->s.angles[t] = AngleNormalize180( it_ent->s.angles[t] + Q_flrand(-1.0f, 1.0f) * 4 );
				}
			}
			else
			{
				if ( t == YAW )
				{
					it_ent->s.angles[t] = AngleNormalize180( it_ent->s.angles[t] + 90 + Q_flrand(-1.0f, 1.0f) * 4 );
				}
			}
		}

		G_SetAngles( it_ent, it_ent->s.angles );
		G_SetOrigin( it_ent, it_ent->s.origin );

		// zyk: add dropped weapon flag if it is a weapon
		if (gun->giType == IT_WEAPON)
			it_ent->s.eFlags |= EF_DROPPEDWEAPON;

		trap->LinkEntity( (sharedEntity_t *)it_ent );

		if ( rack )
		{
			RP_MarkChild( rack, it_ent );
		}
	}
}

//---------------------------------------------
void SP_misc_model_gun_rack( gentity_t *ent )
{
	gitem_t		*blaster = NULL, *repeater = NULL, *rocket = NULL;
	int			ct = 0;
	int			i = 0;
	float		ofz[3];
	gitem_t		*itemList[3];

	// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (( ent->spawnflags & RACK_BLASTER ) || !(ent->spawnflags & ( RACK_BLASTER | RACK_REPEATER | RACK_ROCKET )))
	{
		blaster	= BG_FindItemForWeapon( WP_BLASTER );
	}

	if (( ent->spawnflags & RACK_REPEATER ))
	{
		repeater = BG_FindItemForWeapon( WP_REPEATER );
	}

	if (( ent->spawnflags & RACK_ROCKET ))
	{
		rocket = BG_FindItemForWeapon( WP_ROCKET_LAUNCHER );
	}

	//---------weapon types
	if ( blaster )
	{
		ofz[ct] = 23.0f;
		itemList[ct++] = blaster;
	}

	if ( repeater )
	{
		ofz[ct] = 24.5f;
		itemList[ct++] = repeater;
	}

	if ( rocket )
	{
		ofz[ct] = 25.5f;
		itemList[ct++] = rocket;
	}

	if ( ct ) //..should always have at least one item on their, but just being safe
	{
		for ( ; ct < 3 ; ct++ )
		{
			ofz[ct] = ofz[0];
			itemList[ct] = itemList[0]; // first weapon ALWAYS propagates to fill up the shelf
		}
	}

	// now actually add the items to the shelf...validate that we have a list to add
	if ( ct )
	{
		for ( i = 0; i < ct; i++ )
		{
			GunRackAddItem( ent, itemList[i], ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 2, ( i - 1 ) * 9 + Q_flrand(-1.0f, 1.0f) * 2, ofz[i] );
		}
	}

	// DAJ_RP: [Dispensers] always this one, as single player draws it: a rack's "model" key is not used (the
	// base maps name imperial/ racks, which single player never shows)
	ent->s.modelindex = G_ModelIndex( RP_MODEL_GUN_RACK );

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );

	ent->r.contents = CONTENTS_SOLID;

	trap->LinkEntity( (sharedEntity_t *)ent );
}

// zyk: functions for misc_model_ammo_rack entity
#define RACK_METAL_BOLTS	2
#define RACK_ROCKETS		4
#define RACK_WEAPONS		8
#define RACK_HEALTH			16
#define RACK_PWR_CELL		32
#define RACK_NO_FILL		64

extern gitem_t	*BG_FindItemForAmmo( ammo_t ammo );

// AMMO RACK!!
void spawn_rack_goods( gentity_t *ent )
{
	float		v_off = 0;
	gitem_t		*blaster = NULL, *metal_bolts = NULL, *rockets = NULL, *it = NULL;
	gitem_t		*am_blaster = NULL, *am_metal_bolts = NULL, *am_rockets = NULL, *am_pwr_cell = NULL;
	gitem_t		*health = NULL;
	int			pos = 0, ct = 0;
	int			i = 0;
	gitem_t		*itemList[4]; // allocating 4, but we only use 3.  done so I don't have to validate that the array isn't full before I add another

	trap->LinkEntity( (sharedEntity_t *)ent );

	// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (( ent->spawnflags & RACK_BLASTER ) || !(ent->spawnflags & ( RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS | RACK_PWR_CELL )))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			blaster	= BG_FindItemForWeapon( WP_BLASTER );
		}
		am_blaster	= BG_FindItemForAmmo( AMMO_BLASTER );
	}

	if (( ent->spawnflags & RACK_METAL_BOLTS ))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			metal_bolts = BG_FindItemForWeapon( WP_REPEATER );
		}
		am_metal_bolts = BG_FindItemForAmmo( AMMO_METAL_BOLTS );
	}

	if (( ent->spawnflags & RACK_ROCKETS ))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			rockets = BG_FindItemForWeapon( WP_ROCKET_LAUNCHER );
		}
		am_rockets = BG_FindItemForAmmo( AMMO_ROCKETS );
	}

	if (( ent->spawnflags & RACK_PWR_CELL ))
	{
		am_pwr_cell = BG_FindItemForAmmo( AMMO_POWERCELL );
	}

	if (( ent->spawnflags & RACK_HEALTH ))
	{
		health = BG_FindItem( "item_medpak_instant" );
		RegisterItem( health );
	}

	//---------Ammo types
	if ( am_blaster )
	{
		itemList[ct++] = am_blaster;
	}

	if ( am_metal_bolts )
	{
		itemList[ct++] = am_metal_bolts;
	}

	if ( am_pwr_cell )
	{
		itemList[ct++] = am_pwr_cell;
	}

	if ( am_rockets )
	{
		itemList[ct++] = am_rockets;
	}

	if ( !(ent->spawnflags & RACK_NO_FILL) && ct ) //double negative..should always have at least one item on there, but just being safe
	{
		for ( ; ct < 3 ; ct++ )
		{
			itemList[ct] = itemList[0]; // first item ALWAYS propagates to fill up the shelf
		}
	}

	// now actually add the items to the shelf...validate that we have a list to add
	if ( ct )
	{
		for ( i = 0; i < ct; i++ )
		{
			GunRackAddItem( ent, itemList[i], ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 0.5f, (i-1)* 8, 7.0f );
		}
	}

	// -----Weapon option
	if ( ent->spawnflags & RACK_WEAPONS )
	{
		if ( !(ent->spawnflags & ( RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS | RACK_PWR_CELL )))
		{
			// nothing was selected, so we assume blaster pack
			it = blaster;
		}
		else
		{
			// if weapon is checked...and so are one or more ammo types, then pick a random weapon to display..always give weaker weapons first
			if ( blaster )
			{
				it = blaster;
				v_off = 25.5f;
			}
			else if ( metal_bolts )
			{
				it = metal_bolts;
				v_off = 27.0f;
			}
			else if ( rockets )
			{
				it = rockets;
				v_off = 28.0f;
			}
		}

		if ( it )
		{
			// since we may have to put up a health pack on the shelf, we should know where we randomly put
			//	the gun so we don't put the pack on the same spot..so pick either the left or right side
			pos = (Q_flrand(0.0f, 1.0f) > .5 ) ? -1 : 1;

			GunRackAddItem( ent, it, ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 2, (Q_flrand(0.0f, 1.0f) * 6 + 4 ) * pos, v_off );
		}
	}

	// ------Medpack
	if (( ent->spawnflags & RACK_HEALTH ) && health )
	{
		if ( !pos )
		{
			// we haven't picked a side already...
			pos = (Q_flrand(0.0f, 1.0f) > .5 ) ? -1 : 1;
		}
		else
		{
			// switch to the opposite side
			pos *= -1;
		}

		GunRackAddItem( ent, health, ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 0.5f, (Q_flrand(0.0f, 1.0f) * 4 + 4 ) * pos, 24 );
	}

	// DAJ_RP: [Dispensers] always this one, as single player -- see SP_misc_model_gun_rack()
	ent->s.modelindex = G_ModelIndex( RP_MODEL_AMMO_RACK );

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );

	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*QUAKED misc_model_ammo_rack (1 0 0.25) (-14 -14 -4) (14 14 30) BLASTER METAL_BOLTS ROCKETS WEAPON HEALTH PWR_CELL NO_FILL
model="models/map_objects/kejim/weaponsrung.md3"

NOTE: can mix and match these spawnflags to get multi-ammo racks.  If only one type is checked the rack will be full of that ammo.  Only three ammo packs max can be displayed.


BLASTER - Adds one or more ammo packs that are compatible with Blasters and the Bryar pistol.
METAL_BOLTS - Adds one or more metal bolt ammo packs that are compatible with the heavy repeater and the flechette gun
ROCKETS - Puts one or more rocket packs on a rack.
WEAPON - adds a weapon matching a selected ammo type to the rack.
HEALTH - adds a health pack to the top shelf of the ammo rack
PWR_CELL - Adds one or more power cell packs that are compatible with the Disuptor, bowcaster, and demp2
NO_FILL - Only puts selected ammo on the rack, it never fills up all three slots if only one or two items were checked
*/

//---------------------------------------------
void SP_misc_model_ammo_rack( gentity_t *ent )
{
// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (( ent->spawnflags & RACK_BLASTER ) || !(ent->spawnflags & ( RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS | RACK_PWR_CELL )))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			RegisterItem( BG_FindItemForWeapon( WP_BLASTER ));
		}
		RegisterItem( BG_FindItemForAmmo( AMMO_BLASTER ));
	}

	if (( ent->spawnflags & RACK_METAL_BOLTS ))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			RegisterItem( BG_FindItemForWeapon( WP_REPEATER ));
		}
		RegisterItem( BG_FindItemForAmmo( AMMO_METAL_BOLTS ));
	}

	if (( ent->spawnflags & RACK_ROCKETS ))
	{
		if ( ent->spawnflags & RACK_WEAPONS )
		{
			RegisterItem( BG_FindItemForWeapon( WP_ROCKET_LAUNCHER ));
		}
		RegisterItem( BG_FindItemForAmmo( AMMO_ROCKETS ));
	}

	if (( ent->spawnflags & RACK_PWR_CELL ))
	{
		RegisterItem( BG_FindItemForAmmo( AMMO_POWERCELL ));
	}

	if (( ent->spawnflags & RACK_HEALTH ))
	{
		RegisterItem( BG_FindItem( "item_medpak_instant" ));
	}

	ent->think = spawn_rack_goods;
	ent->nextthink = level.time + 100;

	// GalaxyRP: [Slot Reuse] its model now, as RP_EntitySystemSpawnRefused() (g_spawn.c) found room for it:
	// registered only by spawn_rack_goods() 100 ms later, it could find the room taken by then and leave the
	// rack invisible. spawn_rack_goods() sets the same index again.
	ent->s.modelindex = G_ModelIndex( RP_MODEL_AMMO_RACK );

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );

	ent->r.contents = CONTENTS_SHOTCLIP|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP;//CONTENTS_SOLID;//so use traces can go through them

	trap->LinkEntity( (sharedEntity_t *)ent );
}

void SP_target_escapetrig(gentity_t *ent)
{
	if (level.gametype != GT_SINGLE_PLAYER)
	{
		G_FreeEntity(ent);
		return;
	}

	G_SpawnInt( "escapetime", "60000", &ent->genericValue5);
	//time given (in ms) for the escape
	G_SpawnInt( "escapegoal", "0", &ent->genericValue6);
	//if non-0, when used, will end an ongoing escape instead of start it

	ent->use = Use_Target_Escapetrig;
}

/*QUAKED misc_maglock (0 .5 .8) (-8 -8 -8) (8 8 8) x x x x x x x x
Place facing a door (using the angle, not a targetname) and it will lock that door.  Can only be destroyed by lightsaber and will automatically unlock the door it's attached to

NOTE: place these half-way in the door to make it flush with the door's surface.

"target"	thing to use when destoryed (not doors - it automatically unlocks the door it was angled at)
"health"	default is 10
*/
void maglock_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	//unlock our door if we're the last lock pointed at the door
	if ( self->activator )
	{
		self->activator->lockCount--;
		if ( !self->activator->lockCount )
		{
			self->activator->flags &= ~FL_INACTIVE;
		}
	}

	//use targets
	G_UseTargets( self, attacker );
	//die
	//rwwFIXMEFIXME - weap expl func
//	WP_Explode( self );
}

void maglock_link( gentity_t *self );
gentity_t *G_FindDoorTrigger( gentity_t *ent );

void SP_misc_maglock ( gentity_t *self )
{
	//NOTE: May have to make these only work on doors that are either untargeted
	//		or are targeted by a trigger, not doors fired off by scripts, counters
	//		or other such things?
	self->s.modelindex = G_ModelIndex( "models/map_objects/imp_detention/door_lock.md3" );
	self->genericValue1 = G_EffectIndex( "maglock/explosion" );

	G_SetOrigin( self, self->s.origin );

	self->think = maglock_link;
	//FIXME: for some reason, when you re-load a level, these fail to find their doors...?  Random?  Testing an additional 200ms after the START_TIME_FIND_LINKS
	self->nextthink = level.time + START_TIME_FIND_LINKS+200;//START_TIME_FIND_LINKS;//because we need to let the doors link up and spawn their triggers first!
}
#define RP_MAGLOCK_LINK_TRIES	50	// GalaxyRP: [Entity System] 5 seconds of looking for its door

void maglock_link( gentity_t *self )
{
	//find what we're supposed to be attached to
	vec3_t	forward, start, end;
	trace_t	trace;
	gentity_t *traceEnt;

	AngleVectors( self->s.angles, forward, NULL, NULL );
	VectorMA( self->s.origin, 128, forward, end );
	VectorMA( self->s.origin, -4, forward, start );

	trap->Trace( &trace, start, vec3_origin, vec3_origin, end, self->s.number, MASK_SHOT, qfalse, 0, 0 );

	if ( trace.allsolid || trace.startsolid )
	{
		// GalaxyRP fix: [Entity System] was Com_Error(ERR_DROP) ahead of the free -- a fatal error
		// and process exit on a dedicated server, so the free below never ran. A maglock placed with
		// /entadd or moved with /entedit into a wall took the server down a moment later, from this
		// think. Log it and let the free that was already here do its job.
		G_LogPrintf( "misc_maglock at %s is in solid; removed.\n", vtos(self->s.origin) );
		G_FreeEntity( self );
		return;
	}
	// GalaxyRP fix: [Entity System] pointed at no surface, or at something that is not a door: this
	// tried again every 100 ms for the rest of the map (a door may still be coming, at map load). It
	// gives up after RP_MAGLOCK_LINK_TRIES, logged -- a maglock added with /entadd facing a wall
	// otherwise thought forever.
	if ( trace.fraction == 1.0 || trace.entityNum >= ENTITYNUM_WORLD || Q_stricmp( "func_door", g_entities[trace.entityNum].classname ) )
	{
		if ( ++self->genericValue2 >= RP_MAGLOCK_LINK_TRIES )
		{
			G_LogPrintf( "misc_maglock at %s is not pointed at a func_door (after %d tries); removed.\n", vtos(self->s.origin), self->genericValue2 );
			G_FreeEntity( self );
			return;
		}
		self->think = maglock_link;
		self->nextthink = level.time + 100;
		/*
		Com_Error( ERR_DROP,"misc_maglock at %s pointed at no surface\n", vtos(self->s.origin) );
		Com_Error( ERR_DROP,"misc_maglock at %s not pointed at a door\n", vtos(self->s.origin) );
		G_FreeEntity( self );
		*/
		return;
	}
	traceEnt = &g_entities[trace.entityNum];

	//check the traceEnt, make sure it's a door and give it a lockCount and deactivate it
	//find the trigger for the door
	self->activator = G_FindDoorTrigger( traceEnt );
	if ( !self->activator )
	{
		self->activator = traceEnt;
	}
	self->activator->lockCount++;
	self->activator->flags |= FL_INACTIVE;

	//now position and orient it
	vectoangles( trace.plane.normal, end );
	G_SetOrigin( self, trace.endpos );
	G_SetAngles( self, end );

	//make it hittable
	//FIXME: if rotated/inclined this bbox may be off... but okay if we're a ghoul model?
	//self->s.modelindex = G_ModelIndex( "models/map_objects/imp_detention/door_lock.md3" );
	VectorSet( self->r.mins, -8, -8, -8 );
	VectorSet( self->r.maxs, 8, 8, 8 );
	self->r.contents = CONTENTS_CORPSE;

	//make it destroyable
	self->flags |= FL_SHIELDED;//only damagable by lightsabers
	self->takedamage = qtrue;
	self->health = 10;
	self->die = maglock_die;
	//self->fxID = G_EffectIndex( "maglock/explosion" );

	trap->LinkEntity( (sharedEntity_t *)self );
}

void faller_touch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	if (self->epVelocity[2] < -100 && self->genericValue7 < level.time)
	{
		int r = Q_irand(1, 3);

		if (r == 1)
		{
			self->genericValue11 = G_SoundIndex("sound/chars/stofficer1/misc/pain25");
		}
		else if (r == 2)
		{
			self->genericValue11 = G_SoundIndex("sound/chars/stofficer1/misc/pain50");
		}
		else
		{
			self->genericValue11 = G_SoundIndex("sound/chars/stofficer1/misc/pain75");
		}

		G_EntitySound(self, CHAN_VOICE, self->genericValue11);
		G_EntitySound(self, CHAN_AUTO, self->genericValue10);

		self->genericValue6 = level.time + 3000;

		self->genericValue7 = level.time + 200;
	}
}

void faller_think(gentity_t *ent)
{
	float gravity = 3.0f;
	float mass = 0.09f;
	float bounce = 1.1f;

	if (ent->genericValue6 < level.time)
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	if (ent->epVelocity[2] < -100)
	{
		if (!ent->genericValue8)
		{
			G_EntitySound(ent, CHAN_VOICE, ent->genericValue9);
			ent->genericValue8 = 1;
		}
	}
	else
	{
		ent->genericValue8 = 0;
	}

	G_RunExPhys(ent, gravity, mass, bounce, qtrue, NULL, 0);
	VectorScale(ent->epVelocity, 10.0f, ent->s.pos.trDelta);
	ent->nextthink = level.time + 25;
}

// GalaxyRP fix: [Entity System] a faller is an entity with a ragdoll that lives 15 seconds, and
// misc_faller_create() took one with no check at all -- so a misc_faller with "interval 0" (or a
// targeted one fired every frame) walked the table into G_Spawn()'s ERR_DROP, a process exit on
// a dedicated server. Three bounds now: the interval is floored (SP_misc_faller), a faller is only
// made while the table has room past the reserve, and one misc_faller keeps at most this many
// alive at once (its fallers carry it as parent), which bounds the ragdoll cost even at a legal rate.
#define RP_FALLER_MIN_INTERVAL	500
#define RP_FALLER_MAX_LIVE		32
#define RP_FALLER_MAX_FUDGE		60000	// GalaxyRP: a minute at most on top of the interval: the sum is a think time

static int RP_FallersAlive( const gentity_t *ent )
{
	int i, count = 0;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		if ( g_entities[i].inuse && g_entities[i].parent == ent && g_entities[i].think == faller_think )
		{
			count++;
		}
	}

	return count;
}

void misc_faller_create( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	gentity_t *faller;

	if ( G_EntitySlotsAvailable( 1 ) == qfalse || RP_FallersAlive( ent ) >= RP_FALLER_MAX_LIVE )
	{
		return;
	}

	faller = G_Spawn();
	faller->parent = ent;

	faller->genericValue10 = G_SoundIndex("sound/player/fallsplat");
	faller->genericValue9 = G_SoundIndex("sound/chars/stofficer1/misc/falling1");
	faller->genericValue8 = 0;
	faller->genericValue7 = 0;

	faller->genericValue6 = level.time + 15000;

	G_SetOrigin(faller, ent->s.origin);

	faller->s.modelGhoul2 = 1;
	faller->s.modelindex = G_ModelIndex("models/players/stormtrooper/model.glm");
	faller->s.g2radius = 100;

	faller->s.customRGBA[0]=Q_irand(1,255);
	faller->s.customRGBA[1]=Q_irand(1,255);
	faller->s.customRGBA[2]=Q_irand(1,255);
	faller->s.customRGBA[3]=255;

	VectorSet(faller->r.mins, -15, -15, DEFAULT_MINS_2);
	VectorSet(faller->r.maxs, 15, 15, DEFAULT_MAXS_2);

	faller->clipmask = MASK_PLAYERSOLID;
	faller->r.contents = MASK_PLAYERSOLID;

	faller->s.eFlags = (EF_RAG|EF_CLIENTSMOOTH);

	faller->think = faller_think;
	faller->nextthink = level.time;

	faller->touch = faller_touch;

	faller->epVelocity[0] = flrand(-256.0f, 256.0f);
	faller->epVelocity[1] = flrand(-256.0f, 256.0f);

	trap->LinkEntity((sharedEntity_t *)faller);
}

void misc_faller_think(gentity_t *ent)
{
	misc_faller_create(ent, ent, ent);
	ent->nextthink = level.time + ent->genericValue1 + Q_irand(0, ent->genericValue2);
}

/*QUAKED misc_faller (1 0 0) (-8 -8 -8) (8 8 8)
Falling stormtrooper - spawned every interval+random fudgefactor,
or if specified, when used.

targetname	- if specified, will only spawn when used
interval	- spawn every so often (milliseconds)
fudgefactor	- milliseconds between 0 and this number randomly added to interval
*/
void SP_misc_faller(gentity_t *ent)
{
	G_ModelIndex("models/players/stormtrooper/model.glm");
	G_SoundIndex("sound/chars/stofficer1/misc/pain25");
	G_SoundIndex("sound/chars/stofficer1/misc/pain50");
	G_SoundIndex("sound/chars/stofficer1/misc/pain75");
	G_SoundIndex("sound/chars/stofficer1/misc/falling1");
	G_SoundIndex("sound/player/fallsplat");

	G_SpawnInt("interval", "500", &ent->genericValue1);
	G_SpawnInt("fudgefactor", "0", &ent->genericValue2);

	// GalaxyRP fix: [Entity System] both come straight off spawn keys; see misc_faller_create()
	if ( ent->genericValue1 < RP_FALLER_MIN_INTERVAL || ent->genericValue2 < 0 || ent->genericValue2 > RP_FALLER_MAX_FUDGE )
	{
		G_LogPrintf( "misc_faller at %s: interval %d / fudgefactor %d clamped to at least %d / 0..%d\n",
			vtos( ent->s.origin ), ent->genericValue1, ent->genericValue2, RP_FALLER_MIN_INTERVAL, RP_FALLER_MAX_FUDGE );
		if ( ent->genericValue1 < RP_FALLER_MIN_INTERVAL )
			ent->genericValue1 = RP_FALLER_MIN_INTERVAL;
		if ( ent->genericValue2 < 0 )
			ent->genericValue2 = 0;
		if ( ent->genericValue2 > RP_FALLER_MAX_FUDGE )
			ent->genericValue2 = RP_FALLER_MAX_FUDGE;
	}

	if (!ent->targetname || !ent->targetname[0])
	{
		ent->think = misc_faller_think;
		ent->nextthink = level.time + ent->genericValue1 + Q_irand(0, ent->genericValue2);
	}
	else
	{
		ent->use = misc_faller_create;
	}
}

//rww - ref tag stuff ported from SP (and C-ified)
#define	TAG_GENERIC_NAME	"__WORLD__"	//If a designer chooses this name, cut a finger off as an example to the others

//MAX_TAG_OWNERS is 16 for now in order to not use too much VM memory.
//Each tag owner has preallocated space for tags up to MAX_TAGS.
//As is this means 16*256 sizeof(reference_tag_t)'s in addition to name+inuse*16.
#define MAX_TAGS 256
#define MAX_TAG_OWNERS 16

//Maybe I should use my trap->TrueMalloc/trap->TrueFree stuff with this.
//But I am not yet confident that it can be used without exploding at some point.

typedef struct tagOwner_s
{
	char			name[MAX_REFNAME];
	reference_tag_t	tags[MAX_TAGS];
	qboolean		inuse;
} tagOwner_t;

tagOwner_t refTagOwnerMap[MAX_TAG_OWNERS];

tagOwner_t *FirstFreeTagOwner(void)
{
	int i = 0;

	while (i < MAX_TAG_OWNERS)
	{
		if (!refTagOwnerMap[i].inuse)
		{
			return &refTagOwnerMap[i];
		}
		i++;
	}

	Com_Printf("WARNING: MAX_TAG_OWNERS (%i) REF TAG LIMIT HIT\n", MAX_TAG_OWNERS);
	return NULL;
}

reference_tag_t *FirstFreeRefTag(tagOwner_t *tagOwner)
{
	int i = 0;

	assert(tagOwner);

	while (i < MAX_TAGS)
	{
		if (!tagOwner->tags[i].inuse)
		{
			return &tagOwner->tags[i];
		}
		i++;
	}

	Com_Printf("WARNING: MAX_TAGS (%i) REF TAG LIMIT HIT\n", MAX_TAGS);
	return NULL;
}

/*
-------------------------
TAG_Init
-------------------------
*/

void TAG_Init( void )
{
	int i = 0;
	int x = 0;

	while (i < MAX_TAG_OWNERS)
	{
		while (x < MAX_TAGS)
		{
			memset(&refTagOwnerMap[i].tags[x], 0, sizeof(refTagOwnerMap[i].tags[x]));
			x++;
		}
		memset(&refTagOwnerMap[i], 0, sizeof(refTagOwnerMap[i]));
		i++;
	}
}

/*
-------------------------
TAG_FindOwner
-------------------------
*/

tagOwner_t	*TAG_FindOwner( const char *owner )
{
	int i = 0;

	while (i < MAX_TAG_OWNERS)
	{
		if (refTagOwnerMap[i].inuse && !Q_stricmp(refTagOwnerMap[i].name, owner))
		{
			return &refTagOwnerMap[i];
		}
		i++;
	}

	return NULL;
}

/*
-------------------------
TAG_Find
-------------------------
*/

reference_tag_t	*TAG_Find( const char *owner, const char *name )
{
	tagOwner_t	*tagOwner = NULL;
	int i = 0;

	if (owner && owner[0])
	{
		tagOwner = TAG_FindOwner(owner);
	}
	if (!tagOwner)
	{
		tagOwner = TAG_FindOwner(TAG_GENERIC_NAME);
	}

	//Not found...
	if (!tagOwner)
	{
		tagOwner = TAG_FindOwner( TAG_GENERIC_NAME );

		if (!tagOwner)
		{
			return NULL;
		}
	}

	while (i < MAX_TAGS)
	{
		if (tagOwner->tags[i].inuse && !Q_stricmp(tagOwner->tags[i].name, name))
		{
			return &tagOwner->tags[i];
		}
		i++;
	}

	//Try the generic owner instead
	tagOwner = TAG_FindOwner( TAG_GENERIC_NAME );

	if (!tagOwner)
	{
		return NULL;
	}

	i = 0;
	while (i < MAX_TAGS)
	{
		if (tagOwner->tags[i].inuse && !Q_stricmp(tagOwner->tags[i].name, name))
		{
			return &tagOwner->tags[i];
		}
		i++;
	}

	return NULL;
}

/*
-------------------------
TAG_Add
-------------------------
*/

reference_tag_t	*TAG_Add( const char *name, const char *owner, vec3_t origin, vec3_t angles, int radius, int flags )
{
	reference_tag_t	*tag = NULL;
	tagOwner_t	*tagOwner = NULL;

	//Make sure this tag's name isn't alread in use
	if ( TAG_Find( owner, name ) )
	{
		Com_Printf(S_COLOR_RED"Duplicate tag name \"%s\"\n", name );
		return NULL;
	}

	//Attempt to add this to the owner's list
	if ( !owner || !owner[0] )
	{
		//If the owner isn't found, use the generic world name
		owner = TAG_GENERIC_NAME;
	}

	tagOwner = TAG_FindOwner( owner );

	if (!tagOwner)
	{
		//Create a new owner list
		tagOwner = FirstFreeTagOwner();//new	tagOwner_t;

		if (!tagOwner)
		{
			assert(0);
			return 0;
		}
	}

	//This is actually reverse order of how SP does it because of the way we're storing/allocating.
	//Now that we have the owner, we want to get the first free reftag on the owner itself.
	tag = FirstFreeRefTag(tagOwner);

	if (!tag)
	{
		assert(0);
		return NULL;
	}

	//Copy the information
	VectorCopy( origin, tag->origin );
	VectorCopy( angles, tag->angles );
	tag->radius = radius;
	tag->flags	= flags;

	if ( !name || !name[0] )
	{
		Com_Printf(S_COLOR_RED"ERROR: Nameless ref_tag found at (%i %i %i)\n", (int)origin[0], (int)origin[1], (int)origin[2]);
		return NULL;
	}


	//Copy the name
	Q_strncpyz( (char *) tagOwner->name, owner, MAX_REFNAME );
	Q_strlwr( (char *) tagOwner->name );	//NOTENOTE: For case insensitive searches on a map

	//Copy the name
	Q_strncpyz( (char *) tag->name, name, MAX_REFNAME );
	Q_strlwr( (char *) tag->name );	//NOTENOTE: For case insensitive searches on a map

	tagOwner->inuse = qtrue;
	tag->inuse = qtrue;

	return tag;
}

/*
-------------------------
TAG_GetOrigin
-------------------------
*/

int	TAG_GetOrigin( const char *owner, const char *name, vec3_t origin )
{
	reference_tag_t	*tag = TAG_Find( owner, name );

	if (!tag)
	{
		VectorClear(origin);
		return 0;
	}

	VectorCopy( tag->origin, origin );

	return 1;
}

/*
-------------------------
TAG_GetOrigin2
Had to get rid of that damn assert for dev
-------------------------
*/

int	TAG_GetOrigin2( const char *owner, const char *name, vec3_t origin )
{
	reference_tag_t	*tag = TAG_Find( owner, name );

	if( tag == NULL )
	{
		return 0;
	}

	VectorCopy( tag->origin, origin );

	return 1;
}
/*
-------------------------
TAG_GetAngles
-------------------------
*/

int	TAG_GetAngles( const char *owner, const char *name, vec3_t angles )
{
	reference_tag_t	*tag = TAG_Find( owner, name );

	if (!tag)
	{
		assert(0);
		return 0;
	}

	VectorCopy( tag->angles, angles );

	return 1;
}

/*
-------------------------
TAG_GetRadius
-------------------------
*/

int TAG_GetRadius( const char *owner, const char *name )
{
	reference_tag_t	*tag = TAG_Find( owner, name );

	if (!tag)
	{
		assert(0);
		return 0;
	}

	return tag->radius;
}

/*
-------------------------
TAG_GetFlags
-------------------------
*/

int TAG_GetFlags( const char *owner, const char *name )
{
	reference_tag_t	*tag = TAG_Find( owner, name );

	if (!tag)
	{
		assert(0);
		return 0;
	}

	return tag->flags;
}

/*
==============================================================================

Spawn functions

==============================================================================
*/

/*QUAKED ref_tag_huge (0.5 0.5 1) (-128 -128 -128) (128 128 128)
SAME AS ref_tag, JUST BIGGER SO YOU CAN SEE THEM IN EDITOR ON HUGE MAPS!

Reference tags which can be positioned throughout the level.
These tags can later be refered to by the scripting system
so that their origins and angles can be referred to.

If you set angles on the tag, these will be retained.

If you target a ref_tag at an entity, that will set the ref_tag's
angles toward that entity.

If you set the ref_tag's ownername to the ownername of an entity,
it makes that entity is the owner of the ref_tag.  This means
that the owner, and only the owner, may refer to that tag.

Tags may not have the same name as another tag with the same
owner.  However, tags with different owners may have the same
name as one another.  In this way, scripts can generically
refer to tags by name, and their owners will automatically
specifiy which tag is being referred to.

targetname	- the name of this tag
ownername	- the owner of this tag
target		- use to point the tag at something for angles
*/

/*QUAKED ref_tag (0.5 0.5 1) (-8 -8 -8) (8 8 8)

Reference tags which can be positioned throughout the level.
These tags can later be refered to by the scripting system
so that their origins and angles can be referred to.

If you set angles on the tag, these will be retained.

If you target a ref_tag at an entity, that will set the ref_tag's
angles toward that entity.

If you set the ref_tag's ownername to the ownername of an entity,
it makes that entity is the owner of the ref_tag.  This means
that the owner, and only the owner, may refer to that tag.

Tags may not have the same name as another tag with the same
owner.  However, tags with different owners may have the same
name as one another.  In this way, scripts can generically
refer to tags by name, and their owners will automatically
specifiy which tag is being referred to.

targetname	- the name of this tag
ownername	- the owner of this tag
target		- use to point the tag at something for angles
*/

void ref_link ( gentity_t *ent )
{
	if ( ent->target )
	{
		//TODO: Find the target and set our angles to that direction
		gentity_t	*target = G_Find( NULL, FOFS(targetname), ent->target );
		vec3_t	dir;

		if ( target )
		{
			//Find the direction to the target
			VectorSubtract( target->s.origin, ent->s.origin, dir );
			VectorNormalize( dir );
			vectoangles( dir, ent->s.angles );

			//FIXME: Does pitch get flipped?
		}
		else
		{
			Com_Printf( S_COLOR_RED"ERROR: ref_tag (%s) has invalid target (%s)\n", ent->targetname, ent->target );
		}
	}

	//Add the tag
	TAG_Add( ent->targetname, ent->ownername, ent->s.origin, ent->s.angles, 16, 0 );

	//Delete immediately, cannot be refered to as an entity again
	//NOTE: this means if you wanted to link them in a chain for, say, a path, you can't
	G_FreeEntity( ent );
}

void SP_reference_tag ( gentity_t *ent )
{
	RP_SpawnSaysWhy( ent, "ref_tags are only read by ICARUS scripts and cannot be placed by the entity commands" );
	if ( ent->target )
	{
		//Init cannot occur until all entities have been spawned
		ent->think = ref_link;
		ent->nextthink = level.time + START_TIME_LINK_ENTS;
	}
	else
	{
		ref_link( ent );
	}
}

/*QUAKED misc_weapon_shooter (1 0 0) (-8 -8 -8) (8 8 8) ALTFIRE TOGGLE
ALTFIRE - fire the alt-fire of the chosen weapon
TOGGLE - keep firing until used again (fires at intervals of "wait")

"wait" - debounce time between refires (defaults to 500)

"target" - what to aim at (will update aim every frame if it's a moving target)

"weapon" - specify the weapon to use (default is WP_BLASTER)
	WP_BRYAR_PISTOL
	WP_BLASTER
	WP_DISRUPTOR
	WP_BOWCASTER
	WP_REPEATER
	WP_DEMP2
	WP_FLECHETTE
	WP_ROCKET_LAUNCHER
	WP_THERMAL
	WP_TRIP_MINE
	WP_DET_PACK
	WP_STUN_BATON
	WP_EMPLACED_GUN
	WP_BOT_LASER
	WP_TURRET
	WP_ATST_MAIN
	WP_ATST_SIDE
	WP_TIE_FIGHTER
	WP_RAPID_FIRE_CONC
	WP_BLASTER_PISTOL
*/
//kind of hacky, but we have to do this with no dynamic allocation
#define MAX_SHOOTERS		16
typedef struct shooterClient_s
{
	gclient_t		cl;
	qboolean		inuse;
} shooterClient_t;
static shooterClient_t g_shooterClients[MAX_SHOOTERS];
static qboolean g_shooterClientInit = qfalse;

gclient_t *G_ClientForShooter(void)
{
	int i = 0;

	if (!g_shooterClientInit)
	{ //in theory it should be initialized to 0 on the stack, but just in case.
		memset(g_shooterClients, 0, sizeof(shooterClient_t)*MAX_SHOOTERS);
		g_shooterClientInit = qtrue;
	}

	// GalaxyRP fix: [Entity System] inuse was never set, so every shooter on the map was handed
	// g_shooterClients[0] -- one shared ps.weapon and one shared aim for all of them -- and the
	// ERR_DROP below could never be reached, nor could G_FreeClientForShooter() ever free anything.
	// Marked now, handed out clean, and returned (G_FreeEntity) when the shooter goes; the 17th
	// shooter is refused by its spawn function instead of ending the server.
	while (i < MAX_SHOOTERS)
	{
		if (!g_shooterClients[i].inuse)
		{
			memset(&g_shooterClients[i].cl, 0, sizeof(g_shooterClients[i].cl));
			g_shooterClients[i].inuse = qtrue;
			return &g_shooterClients[i].cl;
		}
		i++;
	}

	return NULL;
}

// GalaxyRP: [Entity System] whether this client is one of the shooter pool's -- G_FreeEntity()
// gives it back; SP_misc_weapon_shooter() keeps it across a spawn in place (/entedit)
qboolean G_IsShooterClient(const gclient_t *cl)
{
	return (cl && g_shooterClientInit && cl >= &g_shooterClients[0].cl && cl <= &g_shooterClients[MAX_SHOOTERS - 1].cl) ? qtrue : qfalse;
}

// DAJ_RP: [Entity System] whether this entity is a character -- a player, an NPC or a vehicle -- which is what
// "it has a client" was taken to mean all through the Entity System. A misc_weapon_shooter has one too, from
// the pool above, only so the weapon code can fire for it: it is map logic, like a shooter_blaster, and was
// called "a player" by /enthelp, skipped by /entuse, refused by /enttrigger and /entlockset, and turned away
// by a lock as if it were an NPC when its shot opened a locked door.
qboolean RP_EntityIsCharacter(const gentity_t *e)
{
	return (e && e->client && !G_IsShooterClient(e->client)) ? qtrue : qfalse;
}

void G_FreeClientForShooter(gclient_t *cl)
{
	int i = 0;
	while (i < MAX_SHOOTERS)
	{
		if (&g_shooterClients[i].cl == cl)
		{
			g_shooterClients[i].inuse = qfalse;
			return;
		}
		i++;
	}
}

/*
================
RP_WeaponShooterAim

GalaxyRP fix: [SP Maps] misc_weapon_shooter_aim()'s body, so the shot can use it too: points the shooter
at its target's current position. qfalse when it has no target or the target is gone.
================
*/
static qboolean RP_WeaponShooterAim( gentity_t *self )
{
	gentity_t *targ;
	vec3_t dir;

	if ( !self->target )
	{
		return qfalse;
	}

	targ = G_Find( NULL, FOFS(targetname), self->target );
	if ( !targ )
	{
		self->enemy = NULL;
		return qfalse;
	}

	self->enemy = targ;
	// GalaxyRP fix: [SP Maps] the direction computed here was overwritten with the target's
	// absolute position before vectoangles(), so the shooter aimed at the angles of a
	// world coordinate instead of at its target.
	VectorSubtract( targ->r.currentOrigin, self->r.currentOrigin, dir );
	VectorCopy( targ->r.currentOrigin, self->pos1 );
	vectoangles( dir, self->client->ps.viewangles );
	SetClientViewAngle( self, self->client->ps.viewangles );
	return qtrue;
}

void misc_weapon_shooter_fire( gentity_t *self )
{
	// GalaxyRP fix: [SP Maps] a repeating shooter's think is this one, not misc_weapon_shooter_aim, so
	// its aim froze where it was when it started firing: each shot now aims at where the target is
	RP_WeaponShooterAim( self );

	// GalaxyRP fix: [Entity System] every shot is a missile, an entity, and some weapons fire more
	// than one: with the table at the reserve the shot is skipped (the repeat below keeps its
	// cadence) rather than walked into G_Spawn()'s ERR_DROP
	if ( G_EntitySlotsAvailable( 4 ) == qtrue )
	{
		FireWeapon( self, (self->spawnflags&1) );
	}
	if ( (self->spawnflags&2) )
	{//repeat
		self->think = misc_weapon_shooter_fire;
		self->nextthink = level.time + self->wait;
		if ( self->random > 0 )
		{ // GalaxyRP: [SP Maps] single player's refire jitter, "random" milliseconds at most
			self->nextthink += (int)( Q_flrand( 0.0f, 1.0f ) * self->random );
		}
	}
}

void misc_weapon_shooter_aim( gentity_t *self );

void misc_weapon_shooter_use ( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( self->think == misc_weapon_shooter_fire )
	{//repeating fire, stop
		/*
		G_FreeClientForShooter(self->client);
		self->think = G_FreeEntity;
		self->nextthink = level.time;
		*/
		// GalaxyRP fix: [SP Maps] only nextthink was cleared, so think stayed misc_weapon_shooter_fire
		// and every later use took this branch again: a TOGGLE shooter, once stopped, never fired
		// again (t1_sour's intro shooter). One with a target goes back to following it.
		if ( self->target )
		{
			self->think = misc_weapon_shooter_aim;
			self->nextthink = level.time + FRAMETIME;
		}
		else
		{
			self->think = NULL;
			self->nextthink = 0;
		}
		return;
	}
	//otherwise, fire
	misc_weapon_shooter_fire( self );
}

void misc_weapon_shooter_aim( gentity_t *self )
{
	//update my aim
	if ( RP_WeaponShooterAim( self ) )
	{
		//FIXME: don't keep doing this unless target is a moving target?
		self->nextthink = level.time + FRAMETIME;
	}
}

extern stringID_table_t WPTable[];

void SP_misc_weapon_shooter( gentity_t *self )
{
	char *s;

	//alloc a client just for the weapon code to use
	// GalaxyRP fix: [Entity System] spawned again in place (/entedit, /entrotate) it keeps the
	// client it has; a new one takes a free one, and when the pool is out (MAX_SHOOTERS) the
	// shooter is refused -- see G_ClientForShooter()
	if ( !G_IsShooterClient( self->client ) )
	{
		self->client = G_ClientForShooter();//(gclient_s *)trap->Malloc(sizeof(gclient_s), TAG_G_ALLOC, qtrue);
	}
	if ( !self->client )
	{
		Q_strncpyz( level.rp_spawn_refusal, va( "the map has its %d weapon shooters already", MAX_SHOOTERS ), sizeof( level.rp_spawn_refusal ) );
		G_LogPrintf( "misc_weapon_shooter at %s refused: %s\n", vtos( self->s.origin ), level.rp_spawn_refusal );
		G_FreeEntity( self );
		return;
	}

	G_SpawnString("weapon", "", &s);

	//set weapon
	self->s.weapon = self->client->ps.weapon = WP_BLASTER;
	if ( s && s[0] )
	{//use a different weapon
		// GalaxyRP fix: [Entity System] this took whatever GetIDForString returned, and that is -1
		// for any name not in WPTable. BG_FindItemForWeapon() below does not return NULL when it
		// fails -- it calls Com_Error( ERR_DROP ), and common.cpp promotes ERR_DROP to ERR_FATAL on
		// a dedicated server ("generally run unattended"), which is Sys_Error and process exit.
		//
		// So a single misspelled weapon field on a misc_weapon_shooter took the server down. That
		// is not only a mapping concern for us: the classname is in g_spawn.c's spawn table, and
		// both zyk_spawn_entity() and zyk_main_spawn_entity() reach it through G_CallSpawn(), so an
		// entity preset loaded with /entload could do it too.
		//
		// Range-check before committing to it and keep the WP_BLASTER default otherwise, so a typo
		// costs a warning line instead of the server. Same shape as the check NPC_stats.c:842
		// already uses.
		int weap = GetIDForString( WPTable, s );

		if ( weap > WP_NONE && weap < WP_NUM_WEAPONS )
		{
			self->s.weapon = self->client->ps.weapon = weap;
		}
		else
		{
			trap->Print( "SP_misc_weapon_shooter: unknown weapon \"%s\" at %s, using WP_BLASTER\n",
				s, vtos( self->s.origin ) );
		}
	}

	RegisterItem(BG_FindItemForWeapon(self->s.weapon));

	//set where our muzzle is
	VectorCopy( self->s.origin, self->client->renderInfo.muzzlePoint );
	//permanently updated (don't need for MP)
	//self->client->renderInfo.mPCalcTime = Q3_INFINITE;

	//set up to link
	if ( self->target )
	{
        self->think = misc_weapon_shooter_aim;
		self->nextthink = level.time + START_TIME_LINK_ENTS;
	}
	else
	{//just set aim angles
		VectorCopy( self->s.angles, self->client->ps.viewangles );
		AngleVectors( self->s.angles, self->pos1, NULL, NULL );
	}

	//set up to fire when used
    self->use = misc_weapon_shooter_use;

	if ( !self->wait )
	{
		self->wait = 500;
	}

	// GalaxyRP fix: [Entity System] the repeat flag reschedules this shooter every self->wait
	// milliseconds and every shot is a missile, which is an entity. wait came straight off a spawn
	// key with only a zero test in front of it, so "/entadd misc_weapon_shooter ... spawnflags 3
	// wait 1" fired once per server frame and filled the entity table with live missiles. Floored
	// at the same 100ms the mod's other timed entities use (zyk_regen_unit, zyk_training_pole,
	// zyk_mini_game_joiner all do exactly this), which is still faster than any hand-held weapon.
	if ( self->wait < 100 )
	{
		Com_Printf( S_COLOR_YELLOW"WARNING: misc_weapon_shooter at %s asked for a %dms interval, floored to 100ms\n",
			vtos(self->s.origin), self->wait );
		self->wait = 100;
	}
}

/*QUAKED misc_weather_zone (0 .5 .8) ?
Determines a region to check for weather contents - will significantly reduce load time
*/
void SP_misc_weather_zone( gentity_t *ent )
{
	RP_SpawnSaysWhy( ent, "a misc_weather_zone is read by each client from its own copy of the map file, so one added on the server does nothing: weather is set with /admweather" );
	G_FreeEntity(ent);
}

void SP_misc_cubemap( gentity_t *ent )
{
	RP_SpawnSaysWhy( ent, "a misc_cubemap is only read by the rend2 renderer, from the map file: nothing for the server" );
	G_FreeEntity( ent );
}

/*
==================================================================================================
GalaxyRP: [Scripts] server-driven map-model animation (SET_STARTFRAME / SET_ENDFRAME /
SET_ANIMFRAME, misc_model_* entities). Single player animated these in G_RunFrame; the cgame
renders s.frame for ET_GENERAL models, so stepping it here is all that is needed. Only entities
a script set up through those commands (rpAnimating) are touched: s.frame is used as a plain
state number by turrets, panels and others, and must not be walked for them.
==================================================================================================
*/
void RP_Animate( gentity_t *self )
{
	if ( self->s.eFlags & EF_SHADER_ANIM )
	{
		return;
	}

	if ( self->s.frame == self->endFrame )
	{
		if ( self->loopAnim )
		{
			self->s.frame = self->startFrame;
		}
		else
		{
			self->rpAnimating = qfalse;
		}
		//Finished sequence - FIXME: only do this once even on looping anims?
		if ( trap->ICARUS_TaskIDPending( (sharedEntity_t *)self, TID_ANIM_BOTH ) )
		{
			trap->ICARUS_TaskIDComplete( (sharedEntity_t *)self, TID_ANIM_BOTH );
		}
		return;
	}

	if ( self->startFrame < self->endFrame )
	{
		if ( self->s.frame < self->startFrame || self->s.frame > self->endFrame )
		{
			self->s.frame = self->startFrame;
		}
		else
		{
			self->s.frame++;
		}
	}
	else if ( self->startFrame > self->endFrame )
	{
		if ( self->s.frame > self->startFrame || self->s.frame < self->endFrame )
		{
			self->s.frame = self->startFrame;
		}
		else
		{
			self->s.frame--;
		}
	}
	else
	{
		self->s.frame = self->endFrame;
	}
}

/*
==================================================================================================
GalaxyRP: [SP Maps] security and goodie keys, per client (clientPersistant_t::rp_securityKey /
rp_goodieKeys). Single player kept these in the player's inventory; a key officer's corpse
(NPC_Touch), an item_security_key or a script hands them out, a misc_security_panel /
func_security_panel with the matching "message" or a func_goodie_panel uses them up. One named
security key at a time, as in single player. Cleared on death (player_die) and on disconnect.
==================================================================================================
*/
qboolean RP_GiveSecurityKey( gentity_t *player, const char *keyname )
{
	if ( !player || !player->client || !keyname || !keyname[0] )
	{
		return qfalse;
	}
	if ( player->client->pers.rp_securityKey[0] )
	{//already carrying one
		return qfalse;
	}
	Q_strncpyz( player->client->pers.rp_securityKey, keyname, sizeof( player->client->pers.rp_securityKey ) );
	return qtrue;
}

qboolean RP_HasSecurityKey( gentity_t *player, const char *keyname )
{
	if ( !player || !player->client || !keyname || !keyname[0] )
	{
		return qfalse;
	}
	return (qboolean)( Q_stricmp( player->client->pers.rp_securityKey, keyname ) == 0 );
}

void RP_TakeSecurityKey( gentity_t *player )
{
	if ( player && player->client )
	{
		player->client->pers.rp_securityKey[0] = '\0';
	}
}

#define RP_MAX_GOODIE_KEYS	5

qboolean RP_GiveGoodieKey( gentity_t *player )
{
	if ( !player || !player->client || player->client->pers.rp_goodieKeys >= RP_MAX_GOODIE_KEYS )
	{
		return qfalse;
	}
	player->client->pers.rp_goodieKeys++;
	return qtrue;
}

qboolean RP_TakeGoodieKey( gentity_t *player )
{
	if ( !player || !player->client || player->client->pers.rp_goodieKeys <= 0 )
	{
		return qfalse;
	}
	player->client->pers.rp_goodieKeys--;
	return qtrue;
}

void RP_ClearKeys( gentity_t *player )
{
	if ( player && player->client )
	{
		player->client->pers.rp_securityKey[0] = '\0';
		player->client->pers.rp_goodieKeys = 0;
	}
}

/*QUAKED misc_security_panel (0 .5 .8) (-8 -8 -8) (8 8 8) x x x x x x x INACTIVE
model="models/map_objects/kejim/sec_panel.md3"
  A model that just sits there and opens when a player uses it and has right key

INACTIVE - Start off, has to be activated to be usable

"message"	name of the key player must have
"target"	thing to use when successfully opened
"target2"	thing to use when player uses the panel without the key
*/
static void panel_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( !activator || !activator->client || activator->s.number >= MAX_CLIENTS )
	{
		return;
	}

	if ( self->message && self->message[0] && !RP_HasSecurityKey( activator, self->message ) )
	{//don't have the key
		G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_fail.mp3" ) );
		G_UseTargets2( self, activator, self->target2 );
		return;
	}

	//unlock
	RP_TakeSecurityKey( activator );
	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_pass.mp3" ) );
	G_UseTargets2( self, activator, self->target );

	//spent, only opens once
	self->use = NULL;
	self->r.svFlags &= ~SVF_PLAYER_USABLE;
}

void SP_misc_security_panel( gentity_t *self )
{
	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	G_SoundIndex( "sound/movers/sec_panel_pass.mp3" );
	G_SoundIndex( "sound/movers/sec_panel_fail.mp3" );

	self->s.modelindex = G_ModelIndex( "models/map_objects/kejim/sec_panel.md3" );
	G_SetOrigin( self, self->s.origin );
	G_SetAngles( self, self->s.angles );
	VectorSet( self->r.mins, -8, -8, -8 );
	VectorSet( self->r.maxs, 8, 8, 8 );
	self->r.contents = CONTENTS_SOLID;
	self->r.svFlags |= SVF_PLAYER_USABLE;
	self->use = panel_use;
	trap->LinkEntity( (sharedEntity_t *)self );
}

/*QUAKED item_security_key (.3 .3 1) (-8 -8 0) (8 8 16) suspended
model="models/items/key.md3"
A security key, picked up by walking over it.

"message" - the key's name, what the misc_security_panel / func_security_panel it opens has as its "message"
*/
static void key_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if ( !other || !other->client || other->s.number >= MAX_CLIENTS || other->health <= 0 )
	{
		return;
	}
	if ( !RP_GiveSecurityKey( other, self->message ) )
	{//already carrying one
		return;
	}
	G_Sound( other, CHAN_AUTO, G_SoundIndex( "sound/weapons/key_pkup.wav" ) );
	trap->SendServerCommand( other->s.number, "cp \"Took the security key\n\"" );
	G_UseTargets( self, other );
	G_FreeEntity( self );
}

void SP_item_security_key( gentity_t *self )
{
	if ( !self->message || !self->message[0] )
	{
		Com_Printf( S_COLOR_YELLOW"WARNING: item_security_key at %s has no message (key name)\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}

	G_SoundIndex( "sound/weapons/key_pkup.wav" );

	self->s.modelindex = G_ModelIndex( "models/items/key.md3" );
	G_SetOrigin( self, self->s.origin );
	G_SetAngles( self, self->s.angles );
	VectorSet( self->r.mins, -8, -8, 0 );
	VectorSet( self->r.maxs, 8, 8, 16 );
	self->r.contents = CONTENTS_TRIGGER;
	self->s.eType = ET_GENERAL;
	self->touch = key_touch;
	trap->LinkEntity( (sharedEntity_t *)self );
}

/*QUAKED misc_spotlight (1 0 0) (-10 -10 0) (10 10 10) START_OFF
model="models/map_objects/imp_mine/spotlight.md3"
Search spotlight that must be targeted at a func_train or other entity. Uses its target2 when it
sees a player.

  START_OFF - the spotlight starts off and is turned on when used (using it again turns it off)

  "wait" - how long between target2 firings while a player stays in the beam (seconds, default 0.5)
  "target" - what to point at
  "target2" - what to use when it detects a player
*/
#define RP_SPOTLIGHT_FOV	15
extern qboolean InFOV3( vec3_t spot, vec3_t from, vec3_t fromAngles, int hFOV, int vFOV );

static void spotlight_think( gentity_t *self )
{
	int i;

	self->nextthink = level.time + FRAMETIME;

	if ( self->spawnflags & 1 )
	{//off
		return;
	}

	//update my aim
	if ( self->target )
	{
		gentity_t *targ = G_Find( NULL, FOFS(targetname), self->target );

		if ( targ )
		{
			vec3_t angles, dir;

			VectorSubtract( targ->r.currentOrigin, self->r.currentOrigin, dir );
			vectoangles( dir, angles );
			VectorCopy( self->r.currentAngles, self->s.apos.trBase );
			for ( i = 0; i < 3; i++ )
			{
				angles[i] = AngleNormalize180( angles[i] );
				self->s.apos.trDelta[i] = AngleNormalize180( ( angles[i] - self->r.currentAngles[i] ) * 10 );
			}
			self->s.apos.trTime = level.time;
			self->s.apos.trDuration = FRAMETIME;
			VectorCopy( angles, self->r.currentAngles );
		}
	}

	//spot a player?
	if ( self->target2 && self->target2[0] && self->painDebounceTime < level.time )
	{
		for ( i = 0; i < MAX_CLIENTS; i++ )
		{
			gentity_t *player = &g_entities[i];

			if ( !player->inuse || !player->client || player->health <= 0
				|| player->client->pers.connected != CON_CONNECTED
				|| player->client->sess.sessionTeam == TEAM_SPECTATOR
				|| player->client->tempSpectate >= level.time
				|| ( player->flags & FL_NOTARGET ) )
			{
				continue;
			}
			if ( !InFOV3( player->r.currentOrigin, self->r.currentOrigin, self->r.currentAngles, RP_SPOTLIGHT_FOV, RP_SPOTLIGHT_FOV ) )
			{
				continue;
			}
			if ( !G_ClearLOS5( self, player->r.currentOrigin ) )
			{
				continue;
			}
			G_UseTargets2( self, player, self->target2 );
			self->painDebounceTime = level.time + (int)( self->wait * 1000 );
			break;
		}
	}
}

static void spotlight_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	self->spawnflags ^= 1;
}

void SP_misc_spotlight( gentity_t *self )
{
	G_SpawnFloat( "wait", "0.5", &self->wait );

	self->s.modelindex = G_ModelIndex( "models/map_objects/imp_mine/spotlight.md3" );
	G_SetOrigin( self, self->s.origin );
	G_SetAngles( self, self->s.angles );
	VectorCopy( self->s.angles, self->r.currentAngles );
	self->s.apos.trType = TR_LINEAR_STOP;
	VectorSet( self->r.mins, -8, -8, -12 );
	VectorSet( self->r.maxs, 8, 8, 0 );
	self->r.contents = CONTENTS_SOLID;
	trap->LinkEntity( (sharedEntity_t *)self );

	self->use = spotlight_use;
	self->think = spotlight_think;
	self->nextthink = level.time + START_TIME_LINK_ENTS;
}

/*QUAKED misc_trip_mine (0.2 0.8 0.2) (-4 -4 -4) (4 4 4) START_ON BROADCAST START_OFF
Place in a map and point the angles at whatever surface you want it to attach to.
The trip mine attaches to that surface and fires its beam away from it, at an angle
perpendicular to it. Owned by the world: it hurts everyone who trips it. The misc_trip_mine
entity itself stays in the map, inert (it is what /entsave writes and /entload respawns);
the mine is the laser trap it creates.

  START_ON / START_OFF - single player's toggling is not supported; the mine is always armed
  BROADCAST - the trip wire and loop sound are sent through area portals
*/
extern void CreateLaserTrap( gentity_t *laserTrap, vec3_t start, gentity_t *owner );
extern void laserTrapStick( gentity_t *ent, vec3_t endpos, vec3_t normal );

void SP_misc_trip_mine( gentity_t *ent )
{
	gentity_t	*laserTrap;
	vec3_t		fwd;

	// GalaxyRP fix: [Entity System] the mine is a second entity; with the table at the reserve the
	// placeholder stays (inert, saved by /entsave) and the mine is not made
	if ( G_EntitySlotsAvailable( 1 ) == qfalse )
	{
		G_LogPrintf( "misc_trip_mine at %s: no entity slot for its mine, %d free\n", vtos( ent->s.origin ), G_FreeEntityCount() );
		return;
	}

	laserTrap = G_Spawn();

	RegisterItem( BG_FindItemForWeapon( WP_TRIP_MINE ) );

	CreateLaserTrap( laserTrap, ent->s.origin, &g_entities[ENTITYNUM_WORLD] );
	// GalaxyRP: [Entity System] the mine as the placeholder's child: /entremove, /entundo and /entedit
	// of the placeholder take it along, and an edit makes one mine, not one more -- see
	// RP_FreeEntityChildren() in g_spawn.c. The mine going off on its own needs nothing: G_FreeEntity()
	// zeroes the link.
	RP_MarkChild( ent, laserTrap );
	laserTrap->count = 1;	//a tripwire, not a proximity mine
	trap->LinkEntity( (sharedEntity_t *)laserTrap );

	AngleVectors( ent->s.angles, fwd, NULL, NULL );
	VectorScale( fwd, -1, fwd );
	laserTrapStick( laserTrap, ent->s.origin, fwd );

	if ( ent->spawnflags & 2 )
	{
		laserTrap->r.svFlags |= SVF_BROADCAST;
	}
}
