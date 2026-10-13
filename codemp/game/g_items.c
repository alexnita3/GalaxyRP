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

#include "g_local.h"
#include "ghoul2/G2.h"
#include "qcommon/q_shared.h"

/*

  Items are any object that a player can touch to gain some effect.

  Pickup will return the number of seconds until they should respawn.

  all items should pop when dropped in lava or slime

  Respawnable items don't actually go away when picked up, they are
  just made invisible and untouchable.  This allows them to ride
  movers and respawn appropriately.
*/

#define	RESPAWN_TEAM_WEAPON	30
// GalaxyRP fix: [Items] RESPAWN_POWERUP (90) is now the default of the rp_powerup_respawn_time cvar

// Item Spawn flags
#define ITMSF_SUSPEND		1
#define ITMSF_NOPLAYER		2
#define ITMSF_ALLOWNPC		4
#define ITMSF_NOTSOLID		8
#define ITMSF_VERTICAL		16
#define ITMSF_INVISIBLE		32
// DAJ_RP: [Still Weapons] a weapon pickup drawn the stock multiplayer way -- spinning above a hologram pad -- instead of
// lying still (RP_ItemSetStill() below). Free in both games' item flags (single player's go to 2048) and in their
// skill filters (256, 512, 1024), and below zyk's 65536 and 131072.
#define ITMSF_HOLOGRAM		4096

extern gentity_t *droppedRedFlag;
extern gentity_t *droppedBlueFlag;


//======================================================================
#define MAX_MEDPACK_HEAL_AMOUNT		25
#define MAX_MEDPACK_BIG_HEAL_AMOUNT	50
#define MAX_SENTRY_DISTANCE			256

// For more than four players, adjust the respawn times, up to 1/4.
int adjustRespawnTime(float preRespawnTime, int itemType, int itemTag)
{
	float respawnTime = preRespawnTime;

	if (itemType == IT_WEAPON)
	{
		if (itemTag == WP_THERMAL ||
			itemTag == WP_TRIP_MINE ||
			itemTag == WP_DET_PACK)
		{ //special case for these, use ammo respawn rate
			respawnTime = rp_ammo_respawn_time.integer;
		}
	}

	if (!g_adaptRespawn.integer)
	{
		return((int)respawnTime);
	}

	if (level.numPlayingClients > 4)
	{	// Start scaling the respawn times.
		if (level.numPlayingClients > 32)
		{	// 1/4 time minimum.
			respawnTime *= 0.25;
		}
		else if (level.numPlayingClients > 12)
		{	// From 12-32, scale from 0.5 to 0.25;
			// GalaxyRP fix: [Items] was 20.0 / (n + 8), inherited from vanilla: that is 0.95 at 13
			// players against 0.5 at 12 -- respawns nearly DOUBLED when the 13th player joined -- and only
			// got back down to 0.5 at 32, so the 1/4 floor this comment promises was never reached.
			// 10.0 / (n + 8) meets the band below at 12 (0.5) and reaches 0.25 at 32, as documented.
			respawnTime *= 10.0 / (float)(level.numPlayingClients + 8);
		}
		else
		{	// From 4-12, scale from 1.0 to 0.5;
			respawnTime *= 8.0 / (float)(level.numPlayingClients + 4);
		}
	}

	if (respawnTime < 1.0)
	{	// No matter what, don't go lower than 1 second, or the pickups become very noisy!
		respawnTime = 1.0;
	}

	return ((int)respawnTime);
}

/*
================
G_ItemRespawnTime

GalaxyRP fix: [Items] the one place that says how long an item takes to come back, in seconds, by
category -- the Pickup_* functions return it, and Force Push/Pull uses it for how long a moved item
stays away from its spot (see G_ItemPushReturnTime). Before this each pickup computed its own value
and the push code had a second, different copy that skipped g_adaptRespawn, the team-FFA weapon
time and the thermal/trip mine/det pack exception, and gave powerups 60s instead of 90.

	weapon		g_weaponRespawn (RESPAWN_TEAM_WEAPON in team FFA)	adapted
	ammo		rp_ammo_respawn_time								adapted
	shield		rp_shield_respawn_time							adapted
	health		rp_health_respawn_time							adapted
	holdable	rp_holdable_item_respawn_time (jetpack too)		adapted
	powerup		rp_powerup_respawn_time							NOT adapted, as vanilla never did
	other		60													adapted

"adapted" is adjustRespawnTime(), which also sends the thermal/trip mine/det pack weapons to the ammo
time. A map item's own "wait"/"random" and dropped items are handled by Touch_Item, not here.
================
*/
int G_ItemRespawnTime( const gentity_t *ent )
{
	const gitem_t *item = ent->item;

	if ( !item )
	{
		return 0;
	}

	switch ( item->giType )
	{
	case IT_WEAPON:
		if ( level.gametype == GT_TEAM )
		{ // team deathmatch has slow weapon respawns
			return adjustRespawnTime(RESPAWN_TEAM_WEAPON, item->giType, item->giTag);
		}
		return adjustRespawnTime(g_weaponRespawn.integer, item->giType, item->giTag);
	case IT_AMMO:
		return adjustRespawnTime(rp_ammo_respawn_time.integer, item->giType, item->giTag);
	case IT_ARMOR:
		return adjustRespawnTime(rp_shield_respawn_time.integer, item->giType, item->giTag);
	case IT_HEALTH:
		return adjustRespawnTime(rp_health_respawn_time.integer, item->giType, item->giTag);
	case IT_HOLDABLE:
		return adjustRespawnTime(rp_holdable_item_respawn_time.integer, item->giType, item->giTag);
	case IT_POWERUP:
		return rp_powerup_respawn_time.integer;
	default:
		return adjustRespawnTime(60, item->giType, item->giTag);
	}
}

/*
================
G_ItemPushReturnTime

GalaxyRP fix: [Items] how long a map item moved by Force Push/Pull stays away before it goes back to
its spot: the map's own "wait" if it has one, else its category time above -- never under a second,
so a pushed item always comes back (a 0 or negative cvar used to leave it at the push spot for good,
or snap it back the next frame).
================
*/
int G_ItemPushReturnTime( const gentity_t *ent )
{
	int seconds = ( ent->wait > 0 ) ? (int)ent->wait : G_ItemRespawnTime( ent );

	return ( seconds < 1 ) ? 1 : seconds;
}


#define SHIELD_HEALTH				3000     // zyk: force field shield health changed from 250 to 3000
#define SHIELD_HEALTH_DEC			1		// 25 seconds zyk: changed from 10 to 1	
#define MAX_SHIELD_HEIGHT			254
#define MAX_SHIELD_HALFWIDTH		255
#define SHIELD_HALFTHICKNESS		4
#define SHIELD_PLACEDIST			64

#define SHIELD_SIEGE_HEALTH			2000
#define SHIELD_SIEGE_HEALTH_DEC		1	// still 25 seconds. zyk: changed to 1	

static qhandle_t	shieldLoopSound=0;
static qhandle_t	shieldAttachSound=0;
static qhandle_t	shieldActivateSound=0;
static qhandle_t	shieldDeactivateSound=0;
static qhandle_t	shieldDamageSound=0;


void ShieldRemove(gentity_t *self)
{
	self->think = G_FreeEntity;
	self->nextthink = level.time + 100;

	// Play kill sound...
	G_AddEvent(self, EV_GENERAL_SOUND, shieldDeactivateSound);
	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;

	return;
}


// Count down the health of the shield.
void ShieldThink(gentity_t *self)
{
	self->s.trickedentindex = 0;

	if ( level.gametype == GT_SIEGE || level.gametype == GT_CTF)
	{ // zyk: added CTF condition
		self->health -= SHIELD_SIEGE_HEALTH_DEC;
	}
	else
	{
		self->health -= SHIELD_HEALTH_DEC;
	}
	self->nextthink = level.time + 1000;

	if (level.duel_tournament_mode > 0 && 
		Distance(self->r.currentOrigin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0))
	{ // zyk: cannot place it inside a Duel Tornament arena, in this case, remove it
		self->health = 0;
	}

	if (self->health <= 0)
	{
		ShieldRemove(self);
	}
	return;
}


// The shield was damaged to below zero health.
void ShieldDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	// Play damaging sound...
	G_AddEvent(self, EV_GENERAL_SOUND, shieldDamageSound);

	ShieldRemove(self);
}


// The shield had damage done to it.  Make it flicker.
void ShieldPain(gentity_t *self, gentity_t *attacker, int damage)
{
	// Set the itemplaceholder flag to indicate the the shield drawing that the shield pain should be drawn.
	self->think = ShieldThink;
	self->nextthink = level.time + 400;

	// Play damaging sound...
	G_AddEvent(self, EV_GENERAL_SOUND, shieldDamageSound);

	self->s.trickedentindex = 1;

	return;
}


// Try to turn the shield back on after a delay.
void ShieldGoSolid(gentity_t *self)
{
	trace_t		tr;

	// see if we're valid
	self->health--;
	if (self->health <= 0)
	{
		ShieldRemove(self);
		return;
	}

	trap->Trace (&tr, self->r.currentOrigin, self->r.mins, self->r.maxs, self->r.currentOrigin, self->s.number, CONTENTS_BODY, qfalse, 0, 0 );
	if(tr.startsolid)
	{	// gah, we can't activate yet
		self->nextthink = level.time + 200;
		self->think = ShieldGoSolid;
		trap->LinkEntity((sharedEntity_t *)self);
	}
	else
	{ // get hard... huh-huh...
		self->s.eFlags &= ~EF_NODRAW;

		self->r.contents = CONTENTS_SOLID;
		self->nextthink = level.time + 1000;
		self->think = ShieldThink;
		self->takedamage = qtrue;
		trap->LinkEntity((sharedEntity_t *)self);

		// Play raising sound...
		G_AddEvent(self, EV_GENERAL_SOUND, shieldActivateSound);
		self->s.loopSound = shieldLoopSound;
		self->s.loopIsSoundset = qfalse;
	}

	return;
}


// Turn the shield off to allow a friend to pass through.
void ShieldGoNotSolid(gentity_t *self)
{
	// make the shield non-solid very briefly
	self->r.contents = 0;
	self->s.eFlags |= EF_NODRAW;
	// nextthink needs to have a large enough interval to avoid excess accumulation of Activate messages
	self->nextthink = level.time + 200;
	self->think = ShieldGoSolid;
	self->takedamage = qfalse;
	trap->LinkEntity((sharedEntity_t *)self);

	// Play kill sound...
	G_AddEvent(self, EV_GENERAL_SOUND, shieldDeactivateSound);
	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;
}


// Somebody (a player) has touched the shield.  See if it is a "friend".
void ShieldTouch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	if (level.gametype >= GT_TEAM)
	{ // let teammates through
		// compare the parent's team to the "other's" team
		if (self->parent && ( self->parent->client) && (other->client))
		{
			if (OnSameTeam(self->parent, other))
			{
				ShieldGoNotSolid(self);
			}
		}
	}
	else
	{//let the person who dropped the shield through
		if (self->parent && self->parent->s.number == other->s.number)
		{
			ShieldGoNotSolid(self);
		}
		else if (self->parent && self->parent->client && zyk_is_ally(self->parent,other) == qtrue)
		{ // zyk: allies can pass through the force field
			ShieldGoNotSolid(self);
		}
	}
}


// After a short delay, create the shield by expanding in all directions.
void CreateShield(gentity_t *ent)
{
	trace_t		tr;
	vec3_t		mins, maxs, end, posTraceEnd, negTraceEnd, start;
	int			height, posWidth, negWidth, halfWidth = 0;
	qboolean	xaxis;
	int			paramData = 0;
//	static int	shieldID;

	// trace upward to find height of shield
	VectorCopy(ent->r.currentOrigin, end);
	end[2] += MAX_SHIELD_HEIGHT;
	trap->Trace (&tr, ent->r.currentOrigin, NULL, NULL, end, ent->s.number, MASK_SHOT, qfalse, 0, 0 );
	height = (int)(MAX_SHIELD_HEIGHT * tr.fraction);

	// use angles to find the proper axis along which to align the shield
	VectorSet(mins, -SHIELD_HALFTHICKNESS, -SHIELD_HALFTHICKNESS, 0);
	VectorSet(maxs, SHIELD_HALFTHICKNESS, SHIELD_HALFTHICKNESS, height);
	VectorCopy(ent->r.currentOrigin, posTraceEnd);
	VectorCopy(ent->r.currentOrigin, negTraceEnd);

	if ((int)(ent->s.angles[YAW]) == 0) // shield runs along y-axis
	{
		posTraceEnd[1]+=MAX_SHIELD_HALFWIDTH;
		negTraceEnd[1]-=MAX_SHIELD_HALFWIDTH;
		xaxis = qfalse;
	}
	else  // shield runs along x-axis
	{
		posTraceEnd[0]+=MAX_SHIELD_HALFWIDTH;
		negTraceEnd[0]-=MAX_SHIELD_HALFWIDTH;
		xaxis = qtrue;
	}

	// trace horizontally to find extend of shield
	// positive trace
	VectorCopy(ent->r.currentOrigin, start);
	start[2] += (height>>1);
	trap->Trace (&tr, start, 0, 0, posTraceEnd, ent->s.number, MASK_SHOT, qfalse, 0, 0 );
	posWidth = MAX_SHIELD_HALFWIDTH * tr.fraction;
	// negative trace
	trap->Trace (&tr, start, 0, 0, negTraceEnd, ent->s.number, MASK_SHOT, qfalse, 0, 0 );
	negWidth = MAX_SHIELD_HALFWIDTH * tr.fraction;

	// kef -- monkey with dimensions and place origin in center
	halfWidth = (posWidth + negWidth)>>1;
	if (xaxis)
	{
		ent->r.currentOrigin[0] = ent->r.currentOrigin[0] - negWidth + halfWidth;
	}
	else
	{
		ent->r.currentOrigin[1] = ent->r.currentOrigin[1] - negWidth + halfWidth;
	}
	ent->r.currentOrigin[2] += (height>>1);

	// set entity's mins and maxs to new values, make it solid, and link it
	if (xaxis)
	{
		VectorSet(ent->r.mins, -halfWidth, -SHIELD_HALFTHICKNESS, -(height>>1));
		VectorSet(ent->r.maxs, halfWidth, SHIELD_HALFTHICKNESS, height>>1);
	}
	else
	{
		VectorSet(ent->r.mins, -SHIELD_HALFTHICKNESS, -halfWidth, -(height>>1));
		VectorSet(ent->r.maxs, SHIELD_HALFTHICKNESS, halfWidth, height);
	}
	ent->clipmask = MASK_SHOT;

	// Information for shield rendering.

//	xaxis - 1 bit
//	height - 0-254 8 bits
//	posWidth - 0-255 8 bits
//  negWidth - 0 - 255 8 bits

	paramData = (xaxis << 24) | (height << 16) | (posWidth << 8) | (negWidth);
	ent->s.time2 = paramData;

	if ( level.gametype == GT_SIEGE || level.gametype == GT_CTF)
	{ // zyk: added CTF condition
		// zyk: Force Field 2/2 in RPG Mode has double health
		// GalaxyRP fix: [Shop] Holdable Items Upgrade moved from secrets_found bit 0 (never persisted),
		// to player_settings bit 0 (account-wide), to skill_levels[38] bit 0 (per-character) -- see
		// g_local.h's skill_levels field.
		if (ent->parent && ent->parent->client && ent->parent->client->sess.amrpgmode == 2 && ent->parent->client->pers.skill_levels[38] & (1 << 0))
		{
			ent->health = SHIELD_SIEGE_HEALTH * 2;
		}
		else
		{
			ent->health = SHIELD_SIEGE_HEALTH;
		}
	}
	else
	{ // zyk: Force Field 2/2 in RPG Mode has double health
		// GalaxyRP fix: [Shop] Holdable Items Upgrade moved from secrets_found bit 0 (never persisted),
		// to player_settings bit 0 (account-wide), to skill_levels[38] bit 0 (per-character) -- see
		// g_local.h's skill_levels field.
		if (ent->parent && ent->parent->client && ent->parent->client->sess.amrpgmode == 2 && ent->parent->client->pers.skill_levels[38] & (1 << 0))
		{
			ent->health = SHIELD_HEALTH * 2;
		}
		else
		{
			ent->health = SHIELD_HEALTH;
		}
	}

	ent->s.time = ent->health;//???
	ent->pain = ShieldPain;
	ent->die = ShieldDie;
	ent->touch = ShieldTouch;

	// see if we're valid
	trap->Trace (&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ent->s.number, CONTENTS_BODY, qfalse, 0, 0 );

	if (tr.startsolid)
	{	// Something in the way!
		// make the shield non-solid very briefly
		ent->r.contents = 0;
		ent->s.eFlags |= EF_NODRAW;
		// nextthink needs to have a large enough interval to avoid excess accumulation of Activate messages
		ent->nextthink = level.time + 200;
		ent->think = ShieldGoSolid;
		ent->takedamage = qfalse;
		trap->LinkEntity((sharedEntity_t *)ent);
	}
	else
	{	// Get solid.
		ent->r.contents = CONTENTS_PLAYERCLIP|CONTENTS_SHOTCLIP;//CONTENTS_SOLID;

		ent->nextthink = level.time;
		ent->think = ShieldThink;

		ent->takedamage = qtrue;
		trap->LinkEntity((sharedEntity_t *)ent);

		// Play raising sound...
		G_AddEvent(ent, EV_GENERAL_SOUND, shieldActivateSound);
		ent->s.loopSound = shieldLoopSound;
		ent->s.loopIsSoundset = qfalse;
	}

	ShieldGoSolid(ent);

	return;
}

qboolean PlaceShield(gentity_t *playerent)
{
	static const gitem_t *shieldItem = NULL;
	gentity_t	*shield = NULL;
	trace_t		tr;
	vec3_t		fwd, pos, dest, mins = {-4,-4, 0}, maxs = {4,4,4};
	static qboolean registered = qfalse;

	if ( !registered )
	{
		shieldLoopSound = G_SoundIndex("sound/movers/doors/forcefield_lp.wav");
		shieldAttachSound = G_SoundIndex("sound/weapons/detpack/stick.wav");
		shieldActivateSound = G_SoundIndex("sound/movers/doors/forcefield_on.wav");
		shieldDeactivateSound = G_SoundIndex("sound/movers/doors/forcefield_off.wav");
		shieldDamageSound = G_SoundIndex("sound/effects/bumpfield.wav");
		shieldItem = BG_FindItemForHoldable(HI_SHIELD);
		registered = qtrue;
	}

	// can we place this in front of us?
	AngleVectors (playerent->client->ps.viewangles, fwd, NULL, NULL);
	fwd[2] = 0;
	VectorMA(playerent->client->ps.origin, SHIELD_PLACEDIST, fwd, dest);
	trap->Trace (&tr, playerent->client->ps.origin, mins, maxs, dest, playerent->s.number, MASK_SHOT, qfalse, 0, 0 );
	if (tr.fraction > 0.9)
	{//room in front
		VectorCopy(tr.endpos, pos);
		// drop to floor
		VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
		trap->Trace( &tr, pos, mins, maxs, dest, playerent->s.number, MASK_SOLID, qfalse, 0, 0 );

		if (level.duel_tournament_mode > 0 && 
			Distance(tr.endpos, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0))
		{ // zyk: cannot place it inside a Duel Tornament arena
			return qfalse;
		}

		if ( !tr.startsolid && !tr.allsolid )
		{
			// got enough room so place the portable shield
			shield = G_Spawn();

			// Figure out what direction the shield is facing.
			if (fabs(fwd[0]) > fabs(fwd[1]))
			{	// shield is north/south, facing east.
				shield->s.angles[YAW] = 0;
			}
			else
			{	// shield is along the east/west axis, facing north
				shield->s.angles[YAW] = 90;
			}
			shield->think = CreateShield;
			shield->nextthink = level.time + 500;	// power up after .5 seconds
			shield->parent = playerent;

			// Set team number.
			shield->s.otherEntityNum2 = playerent->client->sess.sessionTeam;

			// GalaxyRP fix: [Shop] Holdable Items Upgrade moved from secrets_found bit 0 (never
			// persisted), to player_settings bit 0 (account-wide), to skill_levels[38] bit 0
			// (per-character) -- see g_local.h's skill_levels field.
			if (level.gametype < GT_TEAM && playerent->client && playerent->client->sess.amrpgmode == 2 && playerent->client->pers.skill_levels[38] & (1 << 0))
			{
				// zyk: setting shield red color if it has the upgrade
				shield->s.otherEntityNum2 = TEAM_RED;
			}

			shield->s.eType = ET_SPECIAL;
			shield->s.modelindex =  HI_SHIELD;	// this'll be used in CG_Useable() for rendering.
			shield->classname = shieldItem->classname;

			shield->r.contents = CONTENTS_TRIGGER;

			shield->touch = 0;
			// using an item causes it to respawn
			shield->use = 0; //Use_Item;

			// allow to ride movers
			shield->s.groundEntityNum = tr.entityNum;

			G_SetOrigin( shield, tr.endpos );

			shield->s.eFlags &= ~EF_NODRAW;
			shield->r.svFlags &= ~SVF_NOCLIENT;

			trap->LinkEntity ((sharedEntity_t *)shield);

			shield->s.owner = playerent->s.number;
			shield->s.shouldtarget = qtrue;
			if (level.gametype >= GT_TEAM)
			{
				shield->s.teamowner = playerent->client->sess.sessionTeam;
			}
			else
			{
				shield->s.teamowner = 16;
			}

			// Play placing sound...
			G_AddEvent(shield, EV_GENERAL_SOUND, shieldAttachSound);

			return qtrue;
		}
	}
	// no room
	return qfalse;
}

void ItemUse_Binoculars(gentity_t *ent)
{
	if (!ent || !ent->client)
	{
		return;
	}

	if (ent->client->ps.weaponstate != WEAPON_READY)
	{ //So we can't fool it and reactivate while switching to the saber or something.
		return;
	}

	/*
	if (ent->client->ps.weapon == WP_SABER)
	{ //No.
		return;
	}
	*/

	// GalaxyRP fix: [RPG classes] removed a dead Thermal Vision cooldown check gated on
	// pers.rpg_class == 2 (Bounty Hunter), which is permanently 0 now.
	// GalaxyRP fix: [Death System] a downed player cannot raise the binoculars. Gates only the
	// zoom-IN direction, like the cloak item and the jetpack above, so a player who was already
	// zoomed when they went down can still lower them.
	if (ent->client->ps.zoomMode == 0 && !G_PlayerIsDowned( ent )) // not zoomed or currently zoomed with the disruptor
	{
		ent->client->ps.zoomMode = 2;
		ent->client->ps.zoomLocked = qfalse;
		ent->client->ps.zoomFov = 40.0f;
	}
	else if (ent->client->ps.zoomMode == 2)
	{
		ent->client->ps.zoomMode = 0;
		ent->client->ps.zoomTime = level.time;
	}
}

void ItemUse_Shield(gentity_t *ent)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing above stops
	// them using this while incapacitated. Same explicit guard as the cloak item below.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	PlaceShield(ent);
}

//--------------------------
// PERSONAL ASSAULT SENTRY
//--------------------------

#define PAS_DAMAGE	2

void SentryTouch(gentity_t *ent, gentity_t *other, trace_t *trace)
{
	return;
}

// GalaxyRP: [SP Maps] the client that placed this sentry (ItemUse_Sentry), or NULL for a
// map-placed misc_sentry_turret, which has no owner and lives as long as the map does
static gentity_t *pas_owner( gentity_t *self )
{
	if ( self->genericValue3 < 0 || self->genericValue3 >= ENTITYNUM_WORLD )
	{
		return NULL;
	}
	return &g_entities[self->genericValue3];
}

//----------------------------------------------------------------
void pas_fire( gentity_t *ent )
//----------------------------------------------------------------
{
	vec3_t fwd, myOrg, enOrg;

	VectorCopy(ent->r.currentOrigin, myOrg);
	myOrg[2] += 24;

	VectorCopy(ent->enemy->client->ps.origin, enOrg);
	enOrg[2] += 24;

	// zyk: if enemy is crouching, aim lower
	if (ent->enemy->client->ps.pm_flags & PMF_DUCKED)
	{
		enOrg[2] -= 24;
	}

	VectorSubtract(enOrg, myOrg, fwd);
	VectorNormalize(fwd);

	myOrg[0] += fwd[0]*16;
	myOrg[1] += fwd[1]*16;
	myOrg[2] += fwd[2]*16;

	// zyk: changed sentry gun shotspeed from 2300 to 2800
	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter Upgrade damage bonus branch
	// gated on pers.rpg_class == 2, which is permanently 0 now.
	WP_FireTurretMissile( pas_owner( ent ) ? pas_owner( ent ) : ent, myOrg, fwd, qfalse, 10, 2800, MOD_SENTRY, ent );

	G_RunObject(ent);
}

#define TURRET_RADIUS 800

extern qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target);
//-----------------------------------------------------
static qboolean pas_find_enemies( gentity_t *self )
//-----------------------------------------------------
{
	qboolean	found = qfalse;
	int			distance_to_find_enemies = TURRET_RADIUS;
	int			count, i;
	float		bestDist = TURRET_RADIUS*TURRET_RADIUS;
	float		enemyDist;
	vec3_t		enemyDir, org, org2;
	gentity_t	*entity_list[MAX_GENTITIES], *target;
	trace_t		tr;

	if ( self->aimDebounceTime > level.time ) // time since we've been shut off
	{
		// We were active and alert, i.e. had an enemy in the last 3 secs
		if ( self->painDebounceTime < level.time )
		{
			G_Sound(self, CHAN_BODY, G_SoundIndex( "sound/chars/turret/ping.wav" ));
			self->painDebounceTime = level.time + 1000;
		}
	}

	// GalaxyRP fix: [Dueling] a sentry whose owner is in a private duel acquires nothing at all. It
	// could only ever pick a target outside the duel, and G_Damage() throws that damage away -- but
	// Jedi_DecloakPair() runs above the duel gate there, so every blank shot still strips a cloaked
	// bystander's cloak and locks them out of re-cloaking for 3-10 seconds. Same reasoning as the
	// seeker drone wind-down in Cmd_EngageDuel_f(). self->parent is the owner (ItemUse_Sentry).
	if (self->parent && self->parent->client && self->parent->client->ps.duelInProgress)
	{
		return qfalse;
	}

	VectorCopy(self->s.pos.trBase, org2);

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter Upgrade detection-range
	// doubling branch gated on pers.rpg_class == 2, which is permanently 0 now.
	count = G_RadiusList( org2, distance_to_find_enemies, self, qtrue, entity_list );

	for ( i = 0; i < count; i++ )
	{
		target = entity_list[i];

		if ( !target->client )
		{
			continue;
		}
		if ( target == self || !target->takedamage || target->health <= 0 || ( target->flags & FL_NOTARGET ))
		{
			continue;
		}
		if ( self->alliedTeam && target->client->sess.sessionTeam == self->alliedTeam )
		{
			continue;
		}
		if (self->genericValue3 == target->s.number)
		{
			continue;
		}

		// GalaxyRP fix: [Dueling] never acquire a player who is in a private duel. The seeker drone's
		// own target search has had this test all along (FindGenericEnemyIndex, w_force.c); the sentry
		// never got it, and zyk_can_hit_target() below covers the Duel Tournament and the Melee Battle
		// but not the private duel. The result was a sentry that locked onto a duellist, wound up, and
		// fired at them for the rest of its life for nothing -- G_Damage() gates the damage -- while
		// ignoring every legitimate target, because ent->enemy is only ever dropped on death.
		if (target->client->ps.duelInProgress)
		{
			continue;
		}

		if ( !trap->InPVS( org2, target->r.currentOrigin ))
		{
			continue;
		}

		if (target->s.eType == ET_NPC &&
			target->s.NPC_class == CLASS_VEHICLE)
		{ //don't get mad at vehicles, silly.
			continue;
		}

		// zyk: dont attack allies
		if (self->parent && zyk_is_ally(self->parent, target) == qtrue)
		{
			continue;
		}

		// zyk: cant hit in this case
		if (self->parent && zyk_can_hit_target(self->parent, target) == qfalse)
		{
			continue;
		}

		if ( target->client )
		{
			if ( target->client->sess.sessionTeam == TEAM_SPECTATOR || target->client->tempSpectate >= level.time )
			{
				continue;
			}
			if ( self->rpSpTurret )
			{ // GalaxyRP: [SP Maps] a map-placed sentry: single player's team rule, leave its own team alone
				if ( target->client->playerTeam == self->rpSpTurretTeam )
				{
					continue;
				}
			}
			else if (target->NPC && target->client->playerTeam == NPCTEAM_PLAYER && target->client->enemyTeam == NPCTEAM_ENEMY)
			{ // zyk: dont attack allied npcs
				continue;
			}

			// GalaxyRP fix: [Guardian] removed a dead boss-battle sentry-targeting restriction
			// comparing pers.guardian_mode to itself (always equal; pers.guardian_mode is
			// permanently 0 now).

			VectorCopy( target->client->ps.origin, org );
		}
		else
		{
			VectorCopy( target->r.currentOrigin, org );
		}

		trap->Trace( &tr, org2, NULL, NULL, org, self->s.number, MASK_SHOT, qfalse, 0, 0 );

		if ( !tr.allsolid && !tr.startsolid && ( tr.fraction == 1.0 || tr.entityNum == target->s.number ))
		{
			// Only acquire if have a clear shot, Is it in range and closer than our best?
			VectorSubtract( target->r.currentOrigin, self->r.currentOrigin, enemyDir );
			enemyDist = VectorLengthSquared( enemyDir );

			if ( enemyDist < bestDist )// all things equal, keep current
			{
				if ( self->attackDebounceTime + 100 < level.time )
				{
					// We haven't fired or acquired an enemy in the last 2 seconds-start-up sound
					G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/chars/turret/startup.wav" ));

					// Wind up turrets for a bit
					self->attackDebounceTime = level.time + 900 + Q_flrand(0.0f, 1.0f) * 200;
				}

				G_SetEnemy( self, target );
				bestDist = enemyDist;
				found = qtrue;
			}
		}
	}

	return found;
}

//---------------------------------
void pas_adjust_enemy( gentity_t *ent )
//---------------------------------
{
	trace_t	tr;
	qboolean keep = qtrue;

	if ( ent->enemy->health <= 0 )
	{
		keep = qfalse;
	}
	else
	{
		vec3_t		org, org2;

		VectorCopy(ent->s.pos.trBase, org2);

		if ( ent->enemy->client )
		{
			VectorCopy( ent->enemy->client->ps.origin, org );
			org[2] -= 15;
		}
		else
		{
			VectorCopy( ent->enemy->r.currentOrigin, org );
		}

		trap->Trace( &tr, org2, NULL, NULL, org, ent->s.number, MASK_SHOT, qfalse, 0, 0 );

		if ( tr.allsolid || tr.startsolid || tr.fraction < 0.9f || tr.entityNum == ent->s.number )
		{
			if (tr.entityNum != ent->enemy->s.number)
			{
				// trace failed
				keep = qfalse;
			}
		}
	}

	if ( keep )
	{
		//ent->bounceCount = level.time + 500 + Q_flrand(0.0f, 1.0f) * 150;
	}
	else if ( ent->bounceCount < level.time && ent->enemy ) // don't ping pong on and off
	{
		ent->enemy = NULL;
		// shut-down sound
		G_Sound( ent, CHAN_BODY, G_SoundIndex( "sound/chars/turret/shutdown.wav" ));

		ent->bounceCount = level.time + 500 + Q_flrand(0.0f, 1.0f) * 150;

		// make turret play ping sound for 5 seconds
		ent->aimDebounceTime = level.time + 5000;
	}
}

#define TURRET_DEATH_DELAY 2000
#define TURRET_LIFETIME 600000 // zyk: this is the time that the Assault Sentry is alive before running out. Changed from 60000 to 600000

void turret_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

void sentryExpire(gentity_t *self)
{
	turret_die(self, self, self, 1000, MOD_UNKNOWN);
}

//---------------------------------
void pas_think( gentity_t *ent )
//---------------------------------
{
	qboolean	moved;
	float		diffYaw, diffPitch;
	vec3_t		enemyDir, org;
	vec3_t		frontAngles, backAngles;
	vec3_t		desiredAngles;
	int			iEntityList[MAX_GENTITIES];
	int			numListedEntities;
	int			i = 0;
	qboolean	clTrapped = qfalse;
	vec3_t		testMins, testMaxs;

	testMins[0] = ent->r.currentOrigin[0] + ent->r.mins[0]+4;
	testMins[1] = ent->r.currentOrigin[1] + ent->r.mins[1]+4;
	testMins[2] = ent->r.currentOrigin[2] + ent->r.mins[2]+4;

	testMaxs[0] = ent->r.currentOrigin[0] + ent->r.maxs[0]-4;
	testMaxs[1] = ent->r.currentOrigin[1] + ent->r.maxs[1]-4;
	testMaxs[2] = ent->r.currentOrigin[2] + ent->r.maxs[2]-4;

	numListedEntities = trap->EntitiesInBox( testMins, testMaxs, iEntityList, MAX_GENTITIES );

	while (i < numListedEntities)
	{
		if (iEntityList[i] < MAX_CLIENTS)
		{ //client stuck inside me. go nonsolid.
			int clNum = iEntityList[i];

			numListedEntities = trap->EntitiesInBox( g_entities[clNum].r.absmin, g_entities[clNum].r.absmax, iEntityList, MAX_GENTITIES );

			i = 0;
			while (i < numListedEntities)
			{
				if (iEntityList[i] == ent->s.number)
				{
					clTrapped = qtrue;
					break;
				}
				i++;
			}
			break;
		}

		i++;
	}

	if (clTrapped)
	{
		ent->r.contents = 0;
		ent->s.fireflag = 0;
		ent->nextthink = level.time + FRAMETIME;
		return;
	}
	else
	{
		ent->r.contents = CONTENTS_SOLID;
	}

	if ( pas_owner( ent ) && ( !pas_owner( ent )->inuse || !pas_owner( ent )->client ||
		pas_owner( ent )->client->sess.sessionTeam != ent->genericValue2 ) )
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

//	G_RunObject(ent);

	if ( !ent->damage )
	{
		ent->damage = 1;
		ent->nextthink = level.time + FRAMETIME;
		return;
	}

	if ( !ent->rpSpTurret && (ent->genericValue8+TURRET_LIFETIME) < level.time )
	{ // GalaxyRP: [SP Maps] a map-placed sentry has no lifetime
		G_Sound( ent, CHAN_BODY, G_SoundIndex( "sound/chars/turret/shutdown.wav" ));
		ent->s.bolt2 = ENTITYNUM_NONE;
		ent->s.fireflag = 2;

		ent->think = sentryExpire;
		ent->nextthink = level.time + TURRET_DEATH_DELAY;
		return;
	}

	if (level.duel_tournament_mode == 4 && 
		Distance(ent->r.currentOrigin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0))
	{ // zyk: cannot place it inside a Duel Tornament arena, in this case, remove it
		G_Sound(ent, CHAN_BODY, G_SoundIndex("sound/chars/turret/shutdown.wav"));
		ent->s.bolt2 = ENTITYNUM_NONE;
		ent->s.fireflag = 2;

		ent->think = sentryExpire;
		ent->nextthink = level.time + FRAMETIME;
		return;
	}

	ent->nextthink = level.time + FRAMETIME;

	if ( ent->enemy )
	{
		// make sure that the enemy is still valid
		pas_adjust_enemy( ent );
	}

	if (ent->enemy)
	{
		if (!ent->enemy->client)
		{
			ent->enemy = NULL;
		}
		else if (ent->enemy->s.number == ent->s.number)
		{
			ent->enemy = NULL;
		}
		else if (ent->enemy->health < 1)
		{
			ent->enemy = NULL;
		}
		// GalaxyRP fix: [Dueling] ...and let go of the target when a private duel starts, in either
		// direction. The two filters added to pas_find_enemies() only stop the sentry ACQUIRING while
		// a duel is on; a lock that already existed when the duel began would otherwise have survived
		// for the sentry's whole remaining lifetime, which is the same blank-fire-and-decloak problem
		// those filters exist to prevent. Deliberately here rather than in pas_adjust_enemy() just
		// above, which defers its drop behind the bounceCount anti-ping-pong delay -- this one should
		// take effect on the frame the duel starts, like the other three tests it sits with.
		else if (ent->enemy->client->ps.duelInProgress)
		{ // the target has entered a duel and is now off limits
			ent->enemy = NULL;
		}
		else if (ent->parent && ent->parent->client && ent->parent->client->ps.duelInProgress)
		{ // the OWNER has entered a duel, so this sentry stands down entirely
			ent->enemy = NULL;
		}
	}

	if ( !ent->enemy )
	{
		pas_find_enemies( ent );
	}

	if (ent->enemy)
	{
		ent->s.bolt2 = ent->enemy->s.number;
	}
	else
	{
		ent->s.bolt2 = ENTITYNUM_NONE;
	}

	moved = qfalse;
	diffYaw = 0.0f; diffPitch = 0.0f;

	ent->speed = AngleNormalize360( ent->speed );
	ent->random = AngleNormalize360( ent->random );

	if ( ent->enemy )
	{
		// ...then we'll calculate what new aim adjustments we should attempt to make this frame
		// Aim at enemy
		if ( ent->enemy->client )
		{
			VectorCopy( ent->enemy->client->ps.origin, org );

			// zyk: if enemy is crouching, aim lower
			if (ent->enemy->client->ps.pm_flags & PMF_DUCKED)
			{
				org[2] -= 20;
			}
		}
		else
		{
			VectorCopy( ent->enemy->r.currentOrigin, org );
		}

		VectorSubtract( org, ent->r.currentOrigin, enemyDir );
		vectoangles( enemyDir, desiredAngles );

		diffYaw = AngleSubtract( ent->speed, desiredAngles[YAW] );
		diffPitch = AngleSubtract( ent->random, desiredAngles[PITCH] );
	}
	else
	{
		// no enemy, so make us slowly sweep back and forth as if searching for a new one
		diffYaw = sin( level.time * 0.0001f + ent->count ) * 2.0f;
	}

	if ( fabs(diffYaw) > 0.25f )
	{
		moved = qtrue;

		if ( fabs(diffYaw) > 10.0f )
		{
			// cap max speed
			ent->speed += (diffYaw > 0.0f) ? -10.0f : 10.0f;
		}
		else
		{
			// small enough
			ent->speed -= diffYaw;
		}
	}


	if ( fabs(diffPitch) > 0.25f )
	{
		moved = qtrue;

		if ( fabs(diffPitch) > 4.0f )
		{
			// cap max speed
			ent->random += (diffPitch > 0.0f) ? -4.0f : 4.0f;
		}
		else
		{
			// small enough
			ent->random -= diffPitch;
		}
	}

	// the bone axes are messed up, so hence some dumbness here
	VectorSet( frontAngles, -ent->random, 0.0f, 0.0f );
	VectorSet( backAngles, 0.0f, 0.0f, ent->speed );

	if ( moved )
	{
	//ent->s.loopSound = G_SoundIndex( "sound/chars/turret/move.wav" );
	}
	else
	{
		ent->s.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
	}

	if ( ent->enemy && ent->attackDebounceTime < level.time )
	{
		ent->count--;

		// GalaxyRP fix: [Entity System] a shot is a missile, an entity: skipped (the round still
		// counts) while the table is at the reserve, rather than walked into G_Spawn()'s ERR_DROP
		if ( ent->count && G_EntitySlotsAvailable( 1 ) == qfalse )
		{
			ent->attackDebounceTime = level.time + 150;
		}
		else if ( ent->count )
		{
			pas_fire( ent );
			ent->s.fireflag = 1;
			ent->attackDebounceTime = level.time + 200;
		}
		else
		{
			//ent->nextthink = 0;
			G_Sound( ent, CHAN_BODY, G_SoundIndex( "sound/chars/turret/shutdown.wav" ));
			ent->s.bolt2 = ENTITYNUM_NONE;
			ent->s.fireflag = 2;

			ent->think = sentryExpire;
			ent->nextthink = level.time + TURRET_DEATH_DELAY;
		}
	}
	else
	{
		ent->s.fireflag = 0;
	}
}

//------------------------------------------------------------------------------------------------------------
void turret_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
//------------------------------------------------------------------------------------------------------------
{
	// Turn off the thinking of the base & use it's targets
	self->think = 0;//NULL;
	self->use = 0;//NULL;

	if ( self->target )
	{
		G_UseTargets( self, attacker );
	}

	if ( pas_owner( self ) && ( !pas_owner( self )->inuse || !pas_owner( self )->client ) )
	{
		G_FreeEntity(self);
		return;
	}

	// clear my data
	self->die  = 0;//NULL;
	self->takedamage = qfalse;
	self->health = 0;

	// hack the effect angle so that explode death can orient the effect properly
	VectorSet( self->s.angles, 0, 0, 1 );

	G_PlayEffect(EFFECT_EXPLOSION_PAS, self->s.pos.trBase, self->s.angles);
	G_RadiusDamage(self->s.pos.trBase, pas_owner( self ) ? pas_owner( self ) : self, 30, 256, self, self, MOD_UNKNOWN);

	if ( pas_owner( self ) )
	{
		pas_owner( self )->client->ps.fd.sentryDeployed = qfalse;
	}

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter placed-sentries decrement
	// gated on pers.rpg_class == 2, which is permanently 0 now.

	//ExplodeDeath( self );
	G_FreeEntity( self );
}

void turret_free(gentity_t *self)
{
	if ( !pas_owner( self ) || !pas_owner( self )->inuse || !pas_owner( self )->client )
	{
		G_FreeEntity(self);
		return;
	}

	pas_owner( self )->client->ps.fd.sentryDeployed = qfalse;

	G_FreeEntity( self );
}

#define TURRET_AMMO_COUNT 500 // zyk: amount of ammo in the assault sentry. Changed from 40 to 500

//---------------------------------
void SP_PAS( gentity_t *base )
//---------------------------------
{
	if ( base->count == 0 )
	{
		// give ammo
		base->count = TURRET_AMMO_COUNT;
	}

	base->s.bolt1 = 1; //This is a sort of hack to indicate that this model needs special turret things done to it
	base->s.bolt2 = ENTITYNUM_NONE; //store our current enemy index

	base->damage = 0; // start animation flag

	VectorSet( base->r.mins, -8, -8, 0 );
	VectorSet( base->r.maxs, 8, 8, 24 );

	G_RunObject(base);

	base->think = pas_think;
	base->nextthink = level.time + FRAMETIME;

	if ( !base->health )
	{
		base->health = 50;
	}

	base->takedamage = qtrue;
	base->die  = turret_die;

	base->physicsObject = qtrue;

	G_Sound( base, CHAN_BODY, G_SoundIndex( "sound/chars/turret/startup.wav" ));
}

/*QUAKED misc_sentry_turret (1 0 0) (-8 -8 0) (8 8 24)
model="models/items/psgun.glm"
Single player's portable sentry turret placed in a map. The same gun as the sentry item, owned by
nobody: it never expires, and shoots at everyone but the team named in "team".

  "team" - the team it leaves alone: player, enemy (the default), neutral or free
  "count" - shots before it runs out of ammo (default 500)
  "health" - default 50
  "target" - used when it is destroyed
*/
void SP_misc_sentry_turret( gentity_t *base )
{
	vec3_t mins, maxs;

	base->classname = "misc_sentry_turret";
	base->s.modelindex = G_ModelIndex( "models/items/psgun.glm" );
	base->s.g2radius = 30.0f;
	base->s.modelGhoul2 = 1;

	G_SetOrigin( base, base->s.origin );
	G_SetAngles( base, base->s.angles );
	base->parent = NULL;
	base->r.contents = CONTENTS_SOLID;
	base->s.solid = 2;
	base->clipmask = MASK_SOLID;
	VectorSet( mins, -8, -8, 0 );
	VectorSet( maxs, 8, 8, 24 );
	VectorCopy( mins, base->r.mins );
	VectorCopy( maxs, base->r.maxs );
	base->genericValue3 = ENTITYNUM_NONE;	//no owner
	base->genericValue2 = 0;
	base->genericValue15 = HI_SENTRY_GUN;
	base->s.eType = ET_GENERAL;
	base->s.pos.trType = TR_GRAVITY;
	base->s.pos.trTime = level.time;
	base->touch = SentryTouch;
	base->nextthink = level.time;
	base->genericValue4 = ENTITYNUM_NONE;	//enemy index
	base->genericValue5 = 1000;
	base->genericValue8 = level.time;
	base->s.owner = ENTITYNUM_NONE;
	base->s.shouldtarget = qtrue;
	base->s.teamowner = 16;

	// the team it leaves alone, single player's "team" key (misc_turret reads it the same way)
	base->rpSpTurret = qtrue;
	base->rpSpTurretTeam = NPCTEAM_ENEMY;
	if ( base->team && base->team[0] )
	{
		if ( !Q_stricmp( base->team, "player" ) )
		{
			base->rpSpTurretTeam = NPCTEAM_PLAYER;
		}
		else if ( !Q_stricmp( base->team, "enemy" ) )
		{
			base->rpSpTurretTeam = NPCTEAM_ENEMY;
		}
		else if ( !Q_stricmp( base->team, "neutral" ) )
		{
			base->rpSpTurretTeam = NPCTEAM_NEUTRAL;
		}
		else if ( !Q_stricmp( base->team, "free" ) )
		{
			base->rpSpTurretTeam = NPCTEAM_FREE;
		}
		else
		{
			base->rpSpTurretTeam = NPCTEAM_NUM_TEAMS;	//nobody is left alone
		}
		base->team = NULL;
	}

	trap->LinkEntity( (sharedEntity_t *)base );

	SP_PAS( base );
}

//------------------------------------------------------------------------
void ItemUse_Sentry( gentity_t *ent )
//------------------------------------------------------------------------
{
	vec3_t fwd, fwdorg;
	vec3_t yawonly;
	vec3_t mins, maxs;
	gentity_t *sentry;

	if (!ent || !ent->client)
	{
		return;
	}

	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing above stops
	// them using this while incapacitated. Same explicit guard as the cloak item below.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	VectorSet( mins, -8, -8, 0 );
	VectorSet( maxs, 8, 8, 24 );


	yawonly[ROLL] = 0;
	yawonly[PITCH] = 0;
	yawonly[YAW] = ent->client->ps.viewangles[YAW];

	AngleVectors(yawonly, fwd, NULL, NULL);

	fwdorg[0] = ent->client->ps.origin[0] + fwd[0]*64;
	fwdorg[1] = ent->client->ps.origin[1] + fwd[1]*64;
	fwdorg[2] = ent->client->ps.origin[2] + fwd[2]*64;

	sentry = G_Spawn();

	sentry->classname = "sentryGun";
	sentry->s.modelindex = G_ModelIndex("models/items/psgun.glm"); //replace ASAP

	sentry->s.g2radius = 30.0f;
	sentry->s.modelGhoul2 = 1;

	G_SetOrigin(sentry, fwdorg);
	sentry->parent = ent;
	sentry->r.contents = CONTENTS_SOLID;
	sentry->s.solid = 2;
	sentry->clipmask = MASK_SOLID;
	VectorCopy(mins, sentry->r.mins);
	VectorCopy(maxs, sentry->r.maxs);
	sentry->genericValue3 = ent->s.number;
	sentry->genericValue2 = ent->client->sess.sessionTeam; //so we can remove ourself if our owner changes teams
	sentry->genericValue15 = HI_SENTRY_GUN;
	sentry->r.absmin[0] = sentry->s.pos.trBase[0] + sentry->r.mins[0];
	sentry->r.absmin[1] = sentry->s.pos.trBase[1] + sentry->r.mins[1];
	sentry->r.absmin[2] = sentry->s.pos.trBase[2] + sentry->r.mins[2];
	sentry->r.absmax[0] = sentry->s.pos.trBase[0] + sentry->r.maxs[0];
	sentry->r.absmax[1] = sentry->s.pos.trBase[1] + sentry->r.maxs[1];
	sentry->r.absmax[2] = sentry->s.pos.trBase[2] + sentry->r.maxs[2];
	sentry->s.eType = ET_GENERAL;
	sentry->s.pos.trType = TR_GRAVITY;//STATIONARY;
	sentry->s.pos.trTime = level.time;
	sentry->touch = SentryTouch;
	sentry->nextthink = level.time;
	sentry->genericValue4 = ENTITYNUM_NONE; //genericValue4 used as enemy index

	sentry->genericValue5 = 1000;

	sentry->genericValue8 = level.time;

	sentry->alliedTeam = ent->client->sess.sessionTeam;

	ent->client->ps.fd.sentryDeployed = qtrue;

	trap->LinkEntity((sharedEntity_t *)sentry);

	sentry->s.owner = ent->s.number;
	sentry->s.shouldtarget = qtrue;
	if (level.gametype >= GT_TEAM)
	{
		sentry->s.teamowner = ent->client->sess.sessionTeam;
	}
	else
	{
		sentry->s.teamowner = 16;
	}

	SP_PAS( sentry );

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter sentry-HP-bonus /
	// placed-sentries-bookkeeping block gated on pers.rpg_class == 2, which is permanently
	// 0 now. sentry->health keeps the default of 50 set inside SP_PAS() above.
}

extern gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle );
void ItemUse_Seeker(gentity_t *ent)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing above stops
	// them using this while incapacitated. Same explicit guard as the cloak item below.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	if ( level.gametype == GT_SIEGE && d_siegeSeekerNPC.integer )
	{//actualy spawn a remote NPC
		gentity_t *remote = NPC_SpawnType( ent, "remote", NULL, qfalse );
		if ( remote && remote->client )
		{//set it to my team
			remote->s.owner = remote->r.ownerNum = ent->s.number;
			remote->activator = ent;
			if ( ent->client->sess.sessionTeam == TEAM_BLUE )
			{
				remote->client->playerTeam = NPCTEAM_PLAYER;
			}
			else if ( ent->client->sess.sessionTeam == TEAM_RED )
			{
				remote->client->playerTeam = NPCTEAM_ENEMY;
			}
			else
			{
				remote->client->playerTeam = NPCTEAM_NEUTRAL;
			}
		}
	}
	else
	{
		ent->client->ps.eFlags |= EF_SEEKERDRONE;
		// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter Upgrade seeker-drone
		// lifetime-extension branch gated on pers.rpg_class == 2, which is permanently 0 now.
		ent->client->ps.droneExistTime = level.time + 60000; // zyk: the seeker drone lifetime, changed from 30000 to 60000
		ent->client->ps.droneFireTime = level.time + 500;   // zyk: fire time of seeker drone changed from 1500 to 500
	}
}

static void MedPackGive(gentity_t *ent, int amount)
{
	if (!ent || !ent->client)
	{
		return;
	}

	if (ent->health <= 0 ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		(ent->client->ps.eFlags & EF_DEAD))
	{
		return;
	}

	if (ent->health >= ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		return;
	}

	ent->health += amount;

	if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
	}
}

void ItemUse_MedPack_Big(gentity_t *ent)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing above stops
	// them using this while incapacitated. Same explicit guard as the cloak item below.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	// zyk: RPG Mode Big Bacta. Recover 150 HP
	// GalaxyRP fix: [Shop] Holdable Items Upgrade moved from secrets_found bit 0 (never persisted), to
	// player_settings bit 0 (account-wide), to skill_levels[38] bit 0 (per-character) -- see g_local.h's
	// skill_levels field.
	if (ent && ent->client && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[38] & (1 << 0))
		MedPackGive(ent, MAX_MEDPACK_BIG_HEAL_AMOUNT * 3);
	else
		MedPackGive(ent, MAX_MEDPACK_BIG_HEAL_AMOUNT);
}

void ItemUse_MedPack(gentity_t *ent)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing above stops
	// them using this while incapacitated. Same explicit guard as the cloak item below.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	// zyk: RPG Mode Bacta Canister. Recover 75 HP
	// GalaxyRP fix: [Shop] Holdable Items Upgrade moved from secrets_found bit 0 (never persisted), to
	// player_settings bit 0 (account-wide), to skill_levels[38] bit 0 (per-character) -- see g_local.h's
	// skill_levels field.
	// GalaxyRP fix: [Shop] the Holdable Items Upgrade's Bacta Canister bonus used to be a no-op for
	// healing (it only refilled magic_power instead), which contradicted the shop's own description
	// ("Bacta Canister recovers more health"). Now mirrors Big Bacta's upgrade bonus exactly: heals
	// 3x the base amount (225 HP) instead of a magic_power refill. See the matching GalaxyRP fix
	// comment on PM_ItemUsable's HI_MEDPAC case (bg_pmove.c) -- the old "usable at max health to top
	// off magic_power" carve-out there no longer applies now that this has no non-HP effect.
	if (ent && ent->client && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[38] & (1 << 0))
		MedPackGive(ent, MAX_MEDPACK_HEAL_AMOUNT * 3);
	else
		MedPackGive(ent, MAX_MEDPACK_HEAL_AMOUNT);
}

#define JETPACK_TOGGLE_TIME			900 // zyk: default 1000
void Jetpack_Off(gentity_t *ent)
{ //create effects?
	assert(ent && ent->client);

	if (!ent->client->jetPackOn)
	{ //aready off
		return;
	}

	ent->client->jetPackOn = qfalse;
}

void Jetpack_On(gentity_t *ent)
{ //create effects?
	assert(ent && ent->client);

	if (ent->client->jetPackOn)
	{ //aready on
		return;
	}

	if (ent->client->ps.fd.forceGripBeingGripped >= level.time)
	{ //can't turn on during grip interval
		return;
	}

	if (ent->client->ps.fallingToDeath)
	{ //too late!
		return;
	}

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/boba/JETON"));

	ent->client->jetPackOn = qtrue;
}

extern qboolean duel_tournament_is_duelist(gentity_t *ent);
void ItemUse_Jetpack( gentity_t *ent )
{
	assert(ent && ent->client);

	if (ent->client->jetPackToggleTime >= level.time)
	{
		return;
	}

	if (ent->health <= 0 ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		(ent->client->ps.eFlags & EF_DEAD) ||
		ent->client->ps.pm_type == PM_DEAD)
	{ //can't use it when dead under any circumstances.
		return;
	}

	// GalaxyRP fix: [RPG classes] removed a dead Magic Master fuel-restore exception
	// (pers.rpg_class != 8 was always true, so the OR'd clause was always true and never
	// blocked this return).
	if (!ent->client->jetPackOn &&
		// ent->client->ps.jetpackFuel < 5
		ent->client->pers.jetpack_fuel < JETPACK_SCALE)
	{ //too low on fuel to start it up
		return;
	}

	// GalaxyRP fix: [Death System] a downed player must not be able to switch the jetpack back ON.
	// paralyze_player() calls Jetpack_Off() when they go down, but that is one-shot: nothing stopped
	// them re-enabling it here or via /jetpack, and ClientThink_real() hands any client with
	// jetPackOn a PM_JETPACK movetype regardless of the downed state -- putting them straight back
	// into the flying-while-downed state that Jetpack_Off() call was added to end.
	// Gates only the ON direction, like the cloak item: switching OFF stays reachable.
	if ( !ent->client->jetPackOn && G_PlayerIsDowned( ent ) )
	{
		return;
	}

	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}
	else if (ent->client->ps.duelInProgress == qfalse && (level.duel_tournament_mode != 4 || duel_tournament_is_duelist(ent) == qfalse))
	{ // zyk: added this condition, cannot activate jetpack in private duel or in a Duel Tournament match
		Jetpack_On(ent);
	}

	// zyk: Jetpack Upgrade decreases jetpack toggle time
	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[34] == 3)
		ent->client->jetPackToggleTime = level.time + (JETPACK_TOGGLE_TIME/2);
	else
		ent->client->jetPackToggleTime = level.time + JETPACK_TOGGLE_TIME;
}

#define CLOAK_TOGGLE_TIME			1000
extern void Jedi_Cloak( gentity_t *self );
extern void Jedi_Decloak( gentity_t *self );
extern void Jedi_DecloakPair( gentity_t *self );
void ItemUse_UseCloak( gentity_t *ent )
{
	assert(ent && ent->client);

	if (ent->client->cloakToggleTime >= level.time)
	{
		return;
	}

	if (ent->health <= 0 ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		(ent->client->ps.eFlags & EF_DEAD) ||
		ent->client->ps.pm_type == PM_DEAD)
	{ //can't use it when dead under any circumstances.
		return;
	}

	// GalaxyRP fix: [Cloak Item] explicit possession check -- this is normally already guaranteed by
	// the fact this function is only reached while Cloak Item is the player's selected holdable, but
	// checking it directly here is cheap and matches the same possession gate "use_cloak" applies.
	if ( !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) )
	{
		return;
	}

	// GalaxyRP fix: [Cloak Item] same downed test as the /use_cloak console path (g_active.c) -- a
	// downed player keeps RP_DOWNED_HEALTH health, so nothing above stops them re-cloaking after being downed and
	// lying there invisible. Gates only the cloak direction; the decloak below stays reachable.
	if ( !ent->client->ps.powerups[PW_CLOAKED] &&
		(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)) )
	{
		return;
	}

	/* zyk: now cloak item doesnt use fuel anymore
	if (!ent->client->ps.powerups[PW_CLOAKED] &&
		ent->client->ps.cloakFuel < 5)
	{ //too low on fuel to start it up
		return;
	}
	*/

	// GalaxyRP fix: [Cloak Item] same toggle logic as "use_cloak" now (see g_active.c's
	// GENCMD_USE_CLOAK) -- cloaking is always solo (never touches a vehicle, even though using this
	// item while mounted is still allowed), but decloaking goes through Jedi_DecloakPair so a player
	// who is currently part of a paired vehicle+rider cloak also takes the vehicle down with them.
	if ( ent->client->ps.powerups[PW_CLOAKED] )
	{//decloak (self, plus the vehicle too if paired)
		Jedi_DecloakPair( ent );
	}
	else
	{//cloak (self only)
		Jedi_Cloak( ent );
	}

	ent->client->cloakToggleTime = level.time + CLOAK_TOGGLE_TIME;
}

#define TOSSED_ITEM_STAY_PERIOD			20000
#define TOSSED_ITEM_OWNER_NOTOUCH_DUR	1000

void SpecialItemThink(gentity_t *ent)
{
	float gravity = 3.0f;
	float mass = 0.09f;
	float bounce = 1.1f;

	if (ent->genericValue5 < level.time)
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	G_RunExPhys(ent, gravity, mass, bounce, qfalse, NULL, 0);
	VectorCopy(ent->r.currentOrigin, ent->s.origin);
	ent->nextthink = level.time + 50;
}

void G_SpecialSpawnItem(gentity_t *ent, gitem_t *item)
{
	RegisterItem( item );
	ent->item = item;

	//go away if no one wants me
	ent->genericValue5 = level.time + TOSSED_ITEM_STAY_PERIOD;
	ent->think = SpecialItemThink;
	ent->nextthink = level.time + 50;
	ent->clipmask = MASK_SOLID;

	ent->physicsBounce = 0.50;		// items are bouncy
	VectorSet (ent->r.mins, -8, -8, -0);
	VectorSet (ent->r.maxs, 8, 8, 16);

	ent->s.eType = ET_ITEM;
	ent->s.modelindex = ent->item - bg_itemlist;		// store item number in modelindex

	ent->r.contents = CONTENTS_TRIGGER;
	ent->touch = Touch_Item;

	//can't touch owner for x seconds
	ent->genericValue11 = ent->r.ownerNum;
	ent->genericValue10 = level.time + TOSSED_ITEM_OWNER_NOTOUCH_DUR;

	//so we know to remove when picked up, not respawn
	ent->genericValue9 = 1;

	//kind of a lame value to use, but oh well. This means don't
	//pick up this item clientside with prediction, because we
	//aren't sending over all the data necessary for the player
	//to know if he can.
	ent->s.brokenLimbs = 1;

	//since it uses my server-only physics
	ent->s.eFlags |= EF_CLIENTSMOOTH;
}

#define DISP_HEALTH_ITEM		"item_medpak_instant"
#define DISP_AMMO_ITEM			"ammo_all"

void G_PrecacheDispensers(void)
{
	gitem_t *item;

	item = BG_FindItem(DISP_HEALTH_ITEM);
	if (item)
	{
		RegisterItem(item);
	}

	item = BG_FindItem(DISP_AMMO_ITEM);
	if (item)
	{
		RegisterItem(item);
	}
}

void ItemUse_UseDisp(gentity_t *ent, int type)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health. This one was already
	// blocked in practice by the forceHandExtend test below, but only as a side effect of the
	// downed state pinning HANDEXTEND_KNOCKDOWN -- the exact accident this fix stops relying on.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	gitem_t *item = NULL;
	gentity_t *eItem;

	if (!ent->client ||
		ent->client->tossableItemDebounce > level.time)
	{ //can't use it again yet
		return;
	}

	if (ent->client->ps.weaponTime > 0 ||
		ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //busy doing something else
		return;
	}

	ent->client->tossableItemDebounce = level.time + TOSS_DEBOUNCE_TIME;

	if (type == HI_HEALTHDISP)
	{
		item = BG_FindItem(DISP_HEALTH_ITEM);
	}
	else
	{
		item = BG_FindItem(DISP_AMMO_ITEM);
	}

	if (item)
	{
		vec3_t fwd, pos;
		gentity_t	*te;

		eItem = G_Spawn();
		eItem->r.ownerNum = ent->s.number;
		eItem->classname = item->classname;

		VectorCopy(ent->client->ps.origin, pos);
		pos[2] += ent->client->ps.viewheight;

		G_SetOrigin(eItem, pos);
		VectorCopy(eItem->r.currentOrigin, eItem->s.origin);
		trap->LinkEntity((sharedEntity_t *)eItem);

		G_SpecialSpawnItem(eItem, item);

		AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
		VectorScale(fwd, 128.0f, eItem->epVelocity);
		eItem->epVelocity[2] = 16.0f;

	//	G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_THERMAL_THROW, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );

		te = G_TempEntity( ent->client->ps.origin, EV_LOCALTIMER );
		te->s.time = level.time;
		te->s.time2 = TOSS_DEBOUNCE_TIME;
		te->s.owner = ent->client->ps.clientNum;
	}
}


//===============================================
//Portable E-Web -rww
//===============================================
//put the e-web away/remove it from the owner
void EWebDisattach(gentity_t *owner, gentity_t *eweb)
{
    owner->client->ewebIndex = 0;
	owner->client->ps.emplacedIndex = 0;
	if (owner->health > 0)
	{
		owner->client->ps.stats[STAT_WEAPONS] = eweb->genericValue11;
	}
	else
	{
		owner->client->ps.stats[STAT_WEAPONS] = 0;
	}
	eweb->think = G_FreeEntity;
	eweb->nextthink = level.time;
}

//precache misc e-web assets
void EWebPrecache(void)
{
	RegisterItem( BG_FindItemForWeapon(WP_TURRET) );
	G_EffectIndex("detpack/explosion.efx");
	G_EffectIndex("turret/muzzle_flash.efx");
}

//e-web death
#define EWEB_DEATH_RADIUS		128
#define EWEB_DEATH_DMG			90

extern void BG_CycleInven(playerState_t *ps, int direction); //bg_misc.c

void EWebDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	vec3_t fxDir;

	// GalaxyRP fix: [E-Web] die once. For anything that is not a client, G_Damage() calls ->die() on
	// EVERY hit that lands while health is at or below zero, and nothing here used to clear
	// takedamage -- while the entity itself is only freed on its next think (EWebDisattach() sets
	// think = G_FreeEntity). So each further hit in the same frame, which flechette shards, repeater
	// fire and overlapping splash all deliver, ran this whole function again: another
	// EWEB_DEATH_DMG blast across EWEB_DEATH_RADIUS, landing on whoever was standing next to it --
	// normally the owner, who is positioned right behind it -- another explosion effect, and another
	// disattach. First line on purpose, ahead of the blast, so nothing below can re-enter either.
	self->takedamage = qfalse;

	G_RadiusDamage(self->r.currentOrigin, self, EWEB_DEATH_DMG, EWEB_DEATH_RADIUS, self, self, MOD_SUICIDE);

	VectorSet(fxDir, 1.0f, 0.0f, 0.0f);
	G_PlayEffect(EFFECT_EXPLOSION_DETPACK, self->r.currentOrigin, fxDir);

	if (self->r.ownerNum != ENTITYNUM_NONE)
	{
		gentity_t *owner = &g_entities[self->r.ownerNum];

		if (owner->inuse && owner->client)
		{
			EWebDisattach(owner, self);

			//make sure it resets next time we spawn one in case we someone obtain one before death
			owner->client->ewebHealth = -1;

			//take it away from him, it is gone forever.
			owner->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1<<HI_EWEB);

			if (owner->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 &&
				bg_itemlist[owner->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE &&
				bg_itemlist[owner->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag == HI_EWEB)
			{ //he has it selected so deselect it and select the first thing available
				owner->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
				BG_CycleInven(&owner->client->ps, 1);
			}
		}
	}

	// GalaxyRP fix: [E-Web] and always go away afterwards. The only thing that used to schedule the
	// free was EWebDisattach(), which runs only when there is a live owner to hand back to -- so an
	// e-web that died without one was left in the world for the rest of the map, solid and shootable
	// but no longer thinking: EWebThink() returns straight after calling this, without setting
	// nextthink, and G_RunThink() has already zeroed it. Nothing reaches that branch today
	// (r.ownerNum is only ever set by EWeb_Create(), and ClientDisconnect() frees a player's e-web
	// before this could see them gone), but it was one guard away from a leak, and these two lines
	// are exactly what EWebDisattach() writes on every other path, so repeating them is idempotent.
	self->think = G_FreeEntity;
	self->nextthink = level.time;
}

//e-web pain
void EWebPain(gentity_t *self, gentity_t *attacker, int damage)
{
	//update the owner's health status of me
	if (self->r.ownerNum != ENTITYNUM_NONE)
	{
		gentity_t *owner = &g_entities[self->r.ownerNum];

		if (owner->inuse && owner->client)
		{
			owner->client->ewebHealth = self->health;
		}
	}
}

//special routine for tracking angles between client and server
void EWeb_SetBoneAngles(gentity_t *ent, char *bone, vec3_t angles)
{
	int *thebone = &ent->s.boneIndex1;
	int *firstFree = NULL;
	int i = 0;
	int boneIndex = G_BoneIndex(bone);
	int flags, up, right, forward;
	vec3_t *boneVector = &ent->s.boneAngles1;
	vec3_t *freeBoneVec = NULL;

	while (thebone)
	{
		if (!*thebone && !firstFree)
		{ //if the value is 0 then this index is clear, we can use it if we don't find the bone we want already existing.
			firstFree = thebone;
			freeBoneVec = boneVector;
		}
		else if (*thebone)
		{
			if (*thebone == boneIndex)
			{ //this is it
				break;
			}
		}

		switch (i)
		{
		case 0:
			thebone = &ent->s.boneIndex2;
			boneVector = &ent->s.boneAngles2;
			break;
		case 1:
			thebone = &ent->s.boneIndex3;
			boneVector = &ent->s.boneAngles3;
			break;
		case 2:
			thebone = &ent->s.boneIndex4;
			boneVector = &ent->s.boneAngles4;
			break;
		default:
			thebone = NULL;
			boneVector = NULL;
			break;
		}

		i++;
	}

	if (!thebone)
	{ //didn't find it, create it
		if (!firstFree)
		{ //no free bones.. can't do a thing then.
			Com_Printf("WARNING: E-Web has no free bone indexes\n");
			return;
		}

		thebone = firstFree;

		*thebone = boneIndex;
		boneVector = freeBoneVec;
	}

	//If we got here then we have a vector and an index.

	//Copy the angles over the vector in the entitystate, so we can use the corresponding index
	//to set the bone angles on the client.
	VectorCopy(angles, *boneVector);

	//Now set the angles on our server instance if we have one.

	if (!ent->ghoul2)
	{
		return;
	}

	flags = BONE_ANGLES_POSTMULT;
	up = POSITIVE_Y;
	right = NEGATIVE_Z;
	forward = NEGATIVE_X;

	//first 3 bits is forward, second 3 bits is right, third 3 bits is up
	ent->s.boneOrient = ((forward)|(right<<3)|(up<<6));

	trap->G2API_SetBoneAngles( ent->ghoul2,
					0,
					bone,
					angles,
					flags,
					up,
					right,
					forward,
					NULL,
					100,
					level.time );
}

//start an animation on model_root both server side and client side
void EWeb_SetBoneAnim(gentity_t *eweb, int startFrame, int endFrame)
{
	//set info on the entity so it knows to start the anim on the client next snapshot.
	eweb->s.eFlags |= EF_G2ANIMATING;

	if (eweb->s.torsoAnim == startFrame && eweb->s.legsAnim == endFrame)
	{ //already playing this anim, let's flag it to restart
		eweb->s.torsoFlip = !eweb->s.torsoFlip;
	}
	else
	{
		eweb->s.torsoAnim = startFrame;
		eweb->s.legsAnim = endFrame;
	}

	//now set the animation on the server ghoul2 instance.
	assert(eweb->ghoul2);
	trap->G2API_SetBoneAnim(eweb->ghoul2, 0, "model_root", startFrame, endFrame,
		(BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND), 1.0f, level.time, -1, 100);
}

//fire a shot off
#define EWEB_MISSILE_DAMAGE			20
void EWebFire(gentity_t *owner, gentity_t *eweb)
{
	mdxaBone_t boltMatrix;
	gentity_t *missile;
	vec3_t p, d, bPoint;

	if (eweb->genericValue10 == -1)
	{ //oh no
		assert(!"Bad e-web bolt");
		return;
	}

	//get the muzzle point
	trap->G2API_GetBoltMatrix(eweb->ghoul2, 0, eweb->genericValue10, &boltMatrix, eweb->s.apos.trBase, eweb->r.currentOrigin, level.time, NULL, eweb->modelScale);
	BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, p);
	BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_Y, d);

	//Start the thing backwards into the bounding box so it can't start inside other solid things
	VectorMA(p, -16.0f, d, bPoint);

	//create the missile
	missile = CreateMissile( bPoint, d, 5000.0f, 10000, owner, qfalse ); // zyk: changed eweb missile velocity. Default 1200.0f

	missile->classname = "generic_proj";
	missile->s.weapon = WP_TURRET;

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter EWeb damage-bonus branch
	// gated on pers.rpg_class == 2, which is permanently 0 now.
	missile->damage = EWEB_MISSILE_DAMAGE;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_TURBLAST;
	missile->clipmask = (MASK_SHOT|CONTENTS_LIGHTSABER);

	//ignore the e-web entity
	missile->passThroughNum = eweb->s.number+1;

	//times it can bounce before it dies
	missile->bounceCount = 8;

	//play the muzzle flash
	vectoangles(d, d);
	G_PlayEffectID(G_EffectIndex("turret/muzzle_flash.efx"), p, d);
}

//lock the owner into place relative to the cannon pos
void EWebPositionUser(gentity_t *owner, gentity_t *eweb)
{
	mdxaBone_t boltMatrix;
	vec3_t p, d;
	trace_t tr;

	trap->G2API_GetBoltMatrix(eweb->ghoul2, 0, eweb->genericValue9, &boltMatrix, eweb->s.apos.trBase, eweb->r.currentOrigin, level.time, NULL, eweb->modelScale);
	BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, p);
	BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_X, d);

	VectorMA(p, 32.0f, d, p);
	p[2] = eweb->r.currentOrigin[2];

	p[2] += 4.0f;

	trap->Trace(&tr, owner->client->ps.origin, owner->r.mins, owner->r.maxs, p, owner->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (!tr.startsolid && !tr.allsolid && tr.fraction == 1.0f)
	{ //all clear, we can move there
		vec3_t pDown;

		VectorCopy(p, pDown);
		pDown[2] -= 7.0f;
		trap->Trace(&tr, p, owner->r.mins, owner->r.maxs, pDown, owner->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

		if (!tr.startsolid && !tr.allsolid)
		{
			VectorSubtract(owner->client->ps.origin, tr.endpos, d);
			if (VectorLength(d) > 1.0f)
			{ //we moved, do some animating
				vec3_t dAng;
				int aFlags = SETANIM_FLAG_HOLD;

				vectoangles(d, dAng);
				dAng[YAW] = AngleSubtract(owner->client->ps.viewangles[YAW], dAng[YAW]);
				if (dAng[YAW] > 0.0f)
				{
					if (owner->client->ps.legsAnim == BOTH_STRAFE_RIGHT1)
					{ //reset to change direction
						aFlags |= SETANIM_FLAG_OVERRIDE;
					}
					G_SetAnim(owner, &owner->client->pers.cmd, SETANIM_LEGS, BOTH_STRAFE_LEFT1, aFlags, 0);
				}
				else
				{
					if (owner->client->ps.legsAnim == BOTH_STRAFE_LEFT1)
					{ //reset to change direction
						aFlags |= SETANIM_FLAG_OVERRIDE;
					}
					G_SetAnim(owner, &owner->client->pers.cmd, SETANIM_LEGS, BOTH_STRAFE_RIGHT1, aFlags, 0);
				}
			}
			else if (owner->client->ps.legsAnim == BOTH_STRAFE_RIGHT1 || owner->client->ps.legsAnim == BOTH_STRAFE_LEFT1)
			{ //don't keep animating in place
				owner->client->ps.legsTimer = 0;
			}

			G_SetOrigin(owner, tr.endpos);
			VectorCopy(tr.endpos, owner->client->ps.origin);
		}
	}
	else
	{ //can't move here.. stop using the thing I guess
		EWebDisattach(owner, eweb);
	}
}

//keep the bone angles updated based on the owner's look dir
void EWebUpdateBoneAngles(gentity_t *owner, gentity_t *eweb)
{
	vec3_t yAng;
	float ideal;
	float incr;
	const float turnCap = 4.0f; //max degrees we can turn per update

	VectorClear(yAng);
	ideal = AngleSubtract(owner->client->ps.viewangles[YAW], eweb->s.angles[YAW]);
	incr = AngleSubtract(ideal, eweb->angle);

	if (incr > turnCap)
	{
		incr = turnCap;
	}
	else if (incr < -turnCap)
	{
		incr = -turnCap;
	}

	eweb->angle += incr;

	yAng[0] = eweb->angle;
	EWeb_SetBoneAngles(eweb, "cannon_Yrot", yAng);

	EWebPositionUser(owner, eweb);
	if (!owner->client->ewebIndex)
	{ //was removed during position function
		return;
	}

	VectorClear(yAng);
	yAng[2] = AngleSubtract(owner->client->ps.viewangles[PITCH], eweb->s.angles[PITCH])*0.8f;
	EWeb_SetBoneAngles(eweb, "cannon_Xrot", yAng);
}

//keep it updated
extern int BG_EmplacedView(vec3_t baseAngles, vec3_t angles, float *newYaw, float constraint); //bg_misc.c

void EWebThink(gentity_t *self)
{
	qboolean killMe = qfalse;
	const float gravity = 3.0f;
	const float mass = 0.09f;
	const float bounce = 1.1f;

	if (self->r.ownerNum == ENTITYNUM_NONE)
	{
		killMe = qtrue;
	}
	else
	{
		gentity_t *owner = &g_entities[self->r.ownerNum];

		if (!owner->inuse || !owner->client || owner->client->pers.connected != CON_CONNECTED ||
			owner->client->ewebIndex != self->s.number || owner->health < 1)
		{
			killMe = qtrue;
		}
		else if (owner->client->ps.emplacedIndex != self->s.number)
		{ //just go back to the inventory then
			EWebDisattach(owner, self);
			return;
		}
		else if (owner->client->ps.forceHandExtendTime > level.time)
		{ // zyk: knocked down players stop using this weapon
			EWebDisattach(owner, self);
			return;
		}

		if (!killMe)
		{
			float yaw;

			if (BG_EmplacedView(owner->client->ps.viewangles, self->s.angles, &yaw, self->s.origin2[0]))
			{
				owner->client->ps.viewangles[YAW] = yaw;
			}
			owner->client->ps.weapon = WP_EMPLACED_GUN;
			// GalaxyRP fix: [E-Web] a bit, not the enum. This was "= WP_EMPLACED_GUN", and
			// WP_EMPLACED_GUN is 17, so the mask it wrote was bits 0 and 4 -- WP_NONE and
			// WP_BRYAR_PISTOL -- while the one weapon the player is actually holding was not in it.
			// Nothing a player could see showed it: the weapon-select bar and weapon cycling both
			// stand down while emplacedIndex is set, pmove forces cmd.weapon to WP_EMPLACED_GUN, and
			// firing goes through this think rather than FireWeapon(). But every other writer of this
			// bit treats it as a bit -- emplaced_gun_use() sets it with |=, the map-gun release clears
			// it with &= ~ -- and this was the only one that did not.
			//
			// Still a wholesale replace, deliberately: while mounted the player holds this and nothing
			// else, and EWebDisattach() puts the deploy-time mask back from genericValue11.
			//
			// The old value had one accidental virtue, which is why this has a companion in
			// zyk_stop_active_holdables() (g_cmds.c). When an account command frees the e-web, the mask
			// is left as whatever this line last wrote, and the skills load that follows only sets or
			// clears the skill-backed weapons. That happened to correct bit 4, and bit 0 is inert; bit 17
			// is neither, and nothing on that path would clear it -- so the helper takes it back itself.
			owner->client->ps.stats[STAT_WEAPONS] = (1 << WP_EMPLACED_GUN);

			if (self->genericValue8 < level.time)
			{ //make sure the anim timer is done
				EWebUpdateBoneAngles(owner, self);
				if (!owner->client->ewebIndex)
				{ //was removed during position function
					return;
				}

				if (owner->client->pers.cmd.buttons & BUTTON_ATTACK)
				{
					if (self->genericValue5 < level.time)
					{ //we can fire another shot off
						EWebFire(owner, self);

						//cheap firing anim
						EWeb_SetBoneAnim(self, 2, 4);
						self->genericValue3 = 1;

						//set fire debounce time
						self->genericValue5 = level.time + 100;
					}
				}
				else if (self->genericValue5 < level.time && self->genericValue3)
				{ //reset the anim back to non-firing
					EWeb_SetBoneAnim(self, 0, 1);
					self->genericValue3 = 0;
				}
			}
		}
	}

	if (killMe)
	{ //something happened to the owner, let's explode
		EWebDie(self, self, self, 999, MOD_SUICIDE);
		return;
	}

	//run some physics on it real quick so it falls and stuff properly
	G_RunExPhys(self, gravity, mass, bounce, qfalse, NULL, 0);

	self->nextthink = level.time;
}

#define EWEB_HEALTH			200

//spawn and set up an e-web ent
gentity_t *EWeb_Create(gentity_t *spawner)
{
	const char *modelName = "models/map_objects/hoth/eweb_model.glm";
	int failSound = G_SoundIndex("sound/interface/shieldcon_empty");
	int eweb_health = EWEB_HEALTH;
	gentity_t *ent;
	trace_t tr;
	vec3_t fAng, fwd, pos, downPos, s;
	vec3_t mins, maxs;

	VectorSet(mins, -32, -32, -24);
	VectorSet(maxs, 32, 32, 24);

	VectorSet(fAng, 0, spawner->client->ps.viewangles[1], 0);
	AngleVectors(fAng, fwd, 0, 0);

	VectorCopy(spawner->client->ps.origin, s);
	//allow some fudge
	s[2] += 12.0f;

	VectorMA(s, 48.0f, fwd, pos);

	trap->Trace(&tr, s, mins, maxs, pos, spawner->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (tr.allsolid || tr.startsolid || tr.fraction != 1.0f)
	{ //can't spawn here, we are in solid
		G_Sound(spawner, CHAN_AUTO, failSound);
		return NULL;
	}

	ent = G_Spawn();

	ent->clipmask = MASK_PLAYERSOLID;
	ent->r.contents = MASK_PLAYERSOLID;

	ent->physicsObject = qtrue;

	//for the sake of being able to differentiate client-side between this and an emplaced gun
	ent->s.weapon = WP_NONE;

	VectorCopy(pos, downPos);
	downPos[2] -= 18.0f;
	trap->Trace(&tr, pos, mins, maxs, downPos, spawner->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (tr.startsolid || tr.allsolid || tr.fraction == 1.0f || tr.entityNum < ENTITYNUM_WORLD)
	{ //didn't hit ground.
		G_FreeEntity(ent);
		G_Sound(spawner, CHAN_AUTO, failSound);
		return NULL;
	}

	VectorCopy(tr.endpos, pos);

	G_SetOrigin(ent, pos);

	VectorCopy(fAng, ent->s.apos.trBase);
	VectorCopy(fAng, ent->r.currentAngles);

	ent->s.owner = spawner->s.number;
	ent->s.teamowner = spawner->client->sess.sessionTeam;

	ent->takedamage = qtrue;

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter EWeb health-multiplier block
	// gated on pers.rpg_class == 2, which is permanently 0 now.

	if (spawner->client->ewebHealth <= 0)
	{ //refresh the owner's e-web health if its last e-web did not exist or was killed
		spawner->client->ewebHealth = eweb_health;
	}

	//resume health of last deployment
	ent->maxHealth = eweb_health;
	ent->health = spawner->client->ewebHealth;
	G_ScaleNetHealth(ent);

	ent->die = EWebDie;
	ent->pain = EWebPain;

	ent->think = EWebThink;
	ent->nextthink = level.time;

	//set up the g2 model info
	ent->s.modelGhoul2 = 1;
	ent->s.g2radius = 128;
	ent->s.modelindex = G_ModelIndex((char *)modelName);

	trap->G2API_InitGhoul2Model(&ent->ghoul2, modelName, 0, 0, 0, 0, 0);

	if (!ent->ghoul2)
	{ //should not happen, but just to be safe.
		G_FreeEntity(ent);
		return NULL;
	}

	//initialize bone angles
	EWeb_SetBoneAngles(ent, "cannon_Yrot", vec3_origin);
	EWeb_SetBoneAngles(ent, "cannon_Xrot", vec3_origin);

	ent->genericValue10 = trap->G2API_AddBolt(ent->ghoul2, 0, "*cannonflash"); //muzzle bolt
	ent->genericValue9 = trap->G2API_AddBolt(ent->ghoul2, 0, "cannon_Yrot"); //for placing the owner relative to rotation

	//set the constraints for this guy as an emplaced weapon, and his constraint angles
	ent->s.origin2[0] = 360.0f; //360 degrees in either direction

	VectorCopy(fAng, ent->s.angles); //consider "angle 0" for constraint

	//angle of y rot bone
	ent->angle = 0.0f;

	ent->r.ownerNum = spawner->s.number;
	trap->LinkEntity((sharedEntity_t *)ent);

	//store off the owner's current weapons, we will be forcing him to use the "emplaced" weapon
	ent->genericValue11 = spawner->client->ps.stats[STAT_WEAPONS];

	//start the "unfolding" anim
	EWeb_SetBoneAnim(ent, 4, 20);
	//don't allow use until the anim is done playing (rough time estimate)
	ent->genericValue8 = level.time + 500;

	VectorCopy(mins, ent->r.mins);
	VectorCopy(maxs, ent->r.maxs);

	return ent;
}

#define EWEB_USE_DEBOUNCE		1000
//use the e-web
void ItemUse_UseEWeb(gentity_t *ent)
{
	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health. This one was already
	// blocked in practice by the forceHandExtend test below, but only as a side effect of the
	// downed state pinning HANDEXTEND_KNOCKDOWN -- the exact accident this fix stops relying on.
	if ( G_PlayerIsDowned( ent ) )
	{
		return;
	}

	if (ent->client->ewebTime > level.time)
	{ //can't use again yet
		return;
	}

	if (ent->client->ps.weaponTime > 0 ||
		ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //busy doing something else
		return;
	}

	if (ent->client->ps.emplacedIndex && !ent->client->ewebIndex)
	{ //using an emplaced gun already that isn't our own e-web
		return;
	}

	if (ent->client->ewebIndex)
	{ //put it away
		EWebDisattach(ent, &g_entities[ent->client->ewebIndex]);
	}
	else
	{ //create it
		gentity_t *eweb = EWeb_Create(ent);

		if (eweb)
		{ //if it's null the thing couldn't spawn (probably no room)
			ent->client->ewebIndex = eweb->s.number;
			ent->client->ps.emplacedIndex = eweb->s.number;
		}
	}

	ent->client->ewebTime = level.time + EWEB_USE_DEBOUNCE;
}
//===============================================
//End E-Web
//===============================================


int Pickup_Powerup( gentity_t *ent, gentity_t *other ) {
	int			quantity;
	int			i;
	gclient_t	*client;

	if ( !other->client->ps.powerups[ent->item->giTag] ) {
		// round timing to seconds to make multiple powerup timers
		// count in sync
		other->client->ps.powerups[ent->item->giTag] =
			level.time - ( level.time % 1000 );

		G_LogWeaponPowerup(other->s.number, ent->item->giTag);
	}

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	other->client->ps.powerups[ent->item->giTag] += quantity * 1000;

	if (ent->item->giTag == PW_YSALAMIRI)
	{
		other->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		other->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
		other->client->ps.powerups[PW_FORCE_BOON] = 0;
	}

	// give any nearby players a "denied" anti-reward
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		vec3_t		delta;
		float		len;
		vec3_t		forward;
		trace_t		tr;

		client = &level.clients[i];
		if ( client == other->client ) {
			continue;
		}
		if ( client->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
			continue;
		}

    // if same team in team game, no sound
    // cannot use OnSameTeam as it expects to g_entities, not clients
  	if ( level.gametype >= GT_TEAM && other->client->sess.sessionTeam == client->sess.sessionTeam  ) {
      continue;
    }

		// if too far away, no sound
		VectorSubtract( ent->s.pos.trBase, client->ps.origin, delta );
		len = VectorNormalize( delta );
		if ( len > 192 ) {
			continue;
		}

		// if not facing, no sound
		AngleVectors( client->ps.viewangles, forward, NULL, NULL );
		if ( DotProduct( delta, forward ) < 0.4 ) {
			continue;
		}

		// if not line of sight, no sound
		trap->Trace( &tr, client->ps.origin, NULL, NULL, ent->s.pos.trBase, ENTITYNUM_NONE, CONTENTS_SOLID, qfalse, 0, 0 );
		if ( tr.fraction != 1.0 ) {
			continue;
		}

		// anti-reward
		client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_DENIEDREWARD;
	}
	return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] rp_powerup_respawn_time, see G_ItemRespawnTime
}

//======================================================================

int Pickup_Holdable( gentity_t *ent, gentity_t *other ) {

	other->client->ps.stats[STAT_HOLDABLE_ITEM] = ent->item - bg_itemlist;

	other->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << ent->item->giTag);

	G_LogWeaponItem(other->s.number, ent->item->giTag);

	return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] see G_ItemRespawnTime
}


//======================================================================

// DAJ_RP: [Ammo] rp_add_ammo_scale as it is used: clamped to RP_AMMO_SCALE_MIN..RP_AMMO_SCALE_MAX
// here as well as by its cvar callback (RP_CVU_addAmmoScale, g_cvar.c), so no reader depends on the
// callback having run. A NaN fails the first test and comes back as the minimum.
float RP_AmmoScale( void )
{
	const float scale = rp_add_ammo_scale.value;

	if ( !( scale >= RP_AMMO_SCALE_MIN ) )
	{
		return RP_AMMO_SCALE_MIN;
	}
	if ( scale > RP_AMMO_SCALE_MAX )
	{
		return RP_AMMO_SCALE_MAX;
	}
	return scale;
}

// DAJ_RP: [Ammo] an amount of ammo as a pickup hands it out: quantity * scale, rounded up. Used by
// Pickup_Ammo() and Pickup_Weapon() below, and as the most a dropped weapon may carry -- the weapon's
// default quantity scaled -- by /drop (Cmd_Drop_f), the death drop (TossClientItems) and the Force
// Pull disarm (TossClientWeapon), so those three take from the dropper exactly what a pickup of the
// same weapon would give.
int RP_ScaledAmmo( int quantity )
{
	return (int)ceil( quantity * RP_AmmoScale() );
}

// DAJ_RP: [Ammo] what a dropped weapon stores in its count so that picking it up gives back exactly
// "give" rounds. Pickup_Weapon() multiplies count by the scale again, so the count is give / scale --
// but not left to float arithmetic alone:
//   - nothing to give is -1, which Pickup_Weapon() reads as "no ammo" (0 would mean "the weapon's
//     full default quantity" -- the reason a count must never be 0: above a scale of 1 a round or two
//     used to divide down to it and came back as a full pack);
//   - at some scales (0.3, 0.6, ...) the float quotient rounds so that a pickup would give one round
//     more than was dropped, a +1 per /drop and pick-up; the count is lowered until it does not.
// Lowering the count by one lowers what a pickup gives by at most one, because the scale is at most 1,
// so the result never undershoots either: a pickup gives back exactly "give".
int RP_DropAmmoCount( int give )
{
	int count;

	if ( give <= 0 )
	{
		return -1;
	}

	count = (int)( give / RP_AmmoScale() );

	if ( count < 1 )
	{
		count = 1;
	}

	while ( count > 1 && RP_ScaledAmmo( count ) > give )
	{
		count--;
	}

	return count;
}

// DAJ_RP: [Ammo] the server's cap for one ammo type -- the same rp_max_* cvar Add_Ammo() below fills
// to, in one place so the ammo dispensers (g_misc.c) and the load-time clamp can ask for it instead of
// carrying their own copy of this list. 0 for anything with no rp_max_* cvar (AMMO_NONE, AMMO_FORCE,
// AMMO_EMPLACED), which Add_Ammo() never fills either.
int RP_MaxAmmo( int ammoType )
{
	switch ( ammoType )
	{
	case AMMO_BLASTER:		return rp_max_blaster_pack_ammo.integer;
	case AMMO_POWERCELL:	return rp_max_power_cell_ammo.integer;
	case AMMO_METAL_BOLTS:	return rp_max_metal_bolt_ammo.integer;
	case AMMO_ROCKETS:		return rp_max_rocket_ammo.integer;
	case AMMO_THERMAL:		return rp_max_thermal_ammo.integer;
	case AMMO_TRIPMINE:		return rp_max_tripmine_ammo.integer;
	case AMMO_DETPACK:		return rp_max_detpack_ammo.integer;
	default:				return 0;
	}
}

// DAJ_RP: [Ammo] bring every capped ammo type down to its rp_max_* cap -- lowers only, never raises.
// Called right after a character's saved ammo is read from the database (/login, /new, /char and
// every respawn -- see g_cmds.c), so ammo saved above the caps is cut back on the next load and saved
// correctly from then on. That covers the overfill the old misc_model_ammo_power_converter handed out
// (it filled to the engine's ammoData[] maximums -- 1000 blaster packs against a cap of 300), and any
// character left above a cap an admin has since lowered.
void RP_ClampAmmoToCaps( gentity_t *ent )
{
	int i;

	if ( !ent || !ent->client )
	{
		return;
	}

	for ( i = AMMO_BLASTER; i < AMMO_MAX; i++ )
	{
		int cap;

		if ( i == AMMO_EMPLACED )
		{ // no rp_max_* cvar and no Weapons column -- not ours to touch
			continue;
		}

		cap = RP_MaxAmmo( i );

		if ( cap >= 0 && ent->client->ps.ammo[i] > cap )
		{
			ent->client->ps.ammo[i] = cap;
		}
	}
}

void Add_Ammo (gentity_t *ent, int weapon, int count)
{
	int max_blasterpack_ammo = rp_max_blaster_pack_ammo.integer;
	int max_powercell_ammo = rp_max_power_cell_ammo.integer;
	int max_metalbolt_ammo = rp_max_metal_bolt_ammo.integer;
	int max_rocket_ammo = rp_max_rocket_ammo.integer;
	int max_thermal_ammo = rp_max_thermal_ammo.integer;
	int max_tripmine_ammo = rp_max_tripmine_ammo.integer;
	int max_detpack_ammo = rp_max_detpack_ammo.integer;

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter max-ammo-bonus block gated
	// on pers.rpg_class == 2, which is permanently 0 now.

	if (weapon == AMMO_BLASTER){
		if (max_blasterpack_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_blasterpack_ammo)
			ent->client->ps.ammo[weapon] = max_blasterpack_ammo;
	}
	else if (weapon == AMMO_POWERCELL){
		if (max_powercell_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_powercell_ammo)
			ent->client->ps.ammo[weapon] = max_powercell_ammo;
	}
	else if (weapon == AMMO_METAL_BOLTS){
		if (max_metalbolt_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_metalbolt_ammo)
			ent->client->ps.ammo[weapon] = max_metalbolt_ammo;
	}
	else if (weapon == AMMO_ROCKETS){
		if (max_rocket_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_rocket_ammo)
			ent->client->ps.ammo[weapon] = max_rocket_ammo;
	}
	else if (weapon == AMMO_THERMAL){
		if (max_thermal_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_thermal_ammo)
			ent->client->ps.ammo[weapon] = max_thermal_ammo;
	}
	else if (weapon == AMMO_TRIPMINE){
		if (max_tripmine_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_tripmine_ammo)
			ent->client->ps.ammo[weapon] = max_tripmine_ammo;
	}
	else if (weapon == AMMO_DETPACK){
		if (max_detpack_ammo - ent->client->ps.ammo[weapon] > count)
			ent->client->ps.ammo[weapon] += count;
		else if (ent->client->ps.ammo[weapon] < max_detpack_ammo)
			ent->client->ps.ammo[weapon] = max_detpack_ammo;
	}
}

int Pickup_Ammo (gentity_t *ent, gentity_t *other)
{
	int		quantity;

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	if (ent->item->giTag == -1)
	{ //an ammo_all, give them a bit of everything
		if ( level.gametype == GT_SIEGE )	// complaints that siege tech's not giving enough ammo.  Does anything else use ammo all?
		{
			Add_Ammo(other, AMMO_BLASTER, 100);
			Add_Ammo(other, AMMO_POWERCELL, 100);
			Add_Ammo(other, AMMO_METAL_BOLTS, 100);
			Add_Ammo(other, AMMO_ROCKETS, 5);
			if (other->client->ps.stats[STAT_WEAPONS] & (1<<WP_DET_PACK))
			{
				Add_Ammo(other, AMMO_DETPACK, 2);
			}
			if (other->client->ps.stats[STAT_WEAPONS] & (1<<WP_THERMAL))
			{
				Add_Ammo(other, AMMO_THERMAL, 2);
			}
			if (other->client->ps.stats[STAT_WEAPONS] & (1<<WP_TRIP_MINE))
			{
				Add_Ammo(other, AMMO_TRIPMINE, 2);
			}
		}
		else
		{
			Add_Ammo(other, AMMO_BLASTER, 50);
			Add_Ammo(other, AMMO_POWERCELL, 50);
			Add_Ammo(other, AMMO_METAL_BOLTS, 50);
			Add_Ammo(other, AMMO_ROCKETS, 2);
		}
	}
	else
	{
		Add_Ammo (other, ent->item->giTag, RP_ScaledAmmo(quantity)); // zyk: cvar to scale the add ammo amount (DAJ_RP: [Ammo] clamped, see RP_AmmoScale)
	}

	return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] see G_ItemRespawnTime
}

//======================================================================


int Pickup_Weapon (gentity_t *ent, gentity_t *other) {
	int		quantity;

	if ( ent->count < 0 ) {
		quantity = 0; // None for you, sir!
	} else {
		if ( ent->count ) {
			quantity = ent->count;
		} else {
			quantity = ent->item->quantity;
		}

		// dropped items and teamplay weapons always have full ammo
		if ( ! (ent->flags & FL_DROPPED_ITEM) && level.gametype != GT_TEAM ) {
			// respawning rules

			// New method:  If the player has less than half the minimum, give them the minimum, else add 1/2 the min.

			// drop the quantity if the already have over the minimum
			if ( other->client->ps.ammo[ ent->item->giTag ] < quantity*0.5 ) {
				quantity = quantity - other->client->ps.ammo[ ent->item->giTag ];
			} else {
				quantity = quantity*0.5;		// only add half the value.
			}

			// Old method:  If the player has less than the minimum, give them the minimum, else just add 1.
/*
			// drop the quantity if the already have over the minimum
			if ( other->client->ps.ammo[ ent->item->giTag ] < quantity ) {
				quantity = quantity - other->client->ps.ammo[ ent->item->giTag ];
			} else {
				quantity = 1;		// only add a single shot
			}
			*/
		}
	}

	// add the weapon
	other->client->ps.stats[STAT_WEAPONS] |= ( 1 << ent->item->giTag );

	//Add_Ammo( other, ent->item->giTag, quantity );
	Add_Ammo( other, weaponData[ent->item->giTag].ammoIndex, RP_ScaledAmmo(quantity) ); // zyk: cvar to scale the add ammo amount (DAJ_RP: [Ammo] clamped, see RP_AmmoScale)

	G_LogWeaponPickup(other->s.number, ent->item->giTag);

	// GalaxyRP fix: [Items] team deathmatch's slow weapon respawns moved into G_ItemRespawnTime
	return G_ItemRespawnTime(ent);
}


//======================================================================

int Pickup_Health (gentity_t *ent, gentity_t *other) {
	int			max;
	int			quantity;

	// small and mega healths will go over the max
	if ( ent->item->quantity != 5 && ent->item->quantity != 100 ) {
		max = other->client->ps.stats[STAT_MAX_HEALTH];
	} else {
		max = other->client->ps.stats[STAT_MAX_HEALTH] * 2;
	}

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	other->health += quantity;

	if (other->health > max ) {
		other->health = max;
	}
	other->client->ps.stats[STAT_HEALTH] = other->health;

	if ( ent->item->quantity == 100 ) {		// mega health respawns slow
		return rp_holdable_item_respawn_time.integer;
	}

	return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] see G_ItemRespawnTime
}

//======================================================================

int Pickup_Armor( gentity_t *ent, gentity_t *other )
{
	/* zyk: new code added
	other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;
	if ( other->client->ps.stats[STAT_ARMOR] > other->client->ps.stats[STAT_MAX_HEALTH] * ent->item->giTag )
	{
		other->client->ps.stats[STAT_ARMOR] = other->client->ps.stats[STAT_MAX_HEALTH] * ent->item->giTag;
	}

	return adjustRespawnTime(RESPAWN_ARMOR, ent->item->giType, ent->item->giTag);
	*/

	if (other->client->sess.amrpgmode == 2 && other->client->ps.stats[STAT_ARMOR] < other->client->pers.max_rpg_shield)
	{ // zyk: RPG Mode has the Max Shield skill that doesnt allow someone to heal shields above this value
		other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;

		if ( other->client->ps.stats[STAT_ARMOR] > other->client->pers.max_rpg_shield ) 
		{
			other->client->ps.stats[STAT_ARMOR] = other->client->pers.max_rpg_shield;
		}

		return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] see G_ItemRespawnTime
	}
	else if (other->client->sess.amrpgmode < 2 && other->client->ps.stats[STAT_ARMOR] < (other->client->ps.stats[STAT_MAX_HEALTH] * ent->item->giTag))
	{ // zyk: player who is not in RPG Mode
		other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;
		if ( other->client->ps.stats[STAT_ARMOR] > other->client->ps.stats[STAT_MAX_HEALTH] * ent->item->giTag ) 
		{
			other->client->ps.stats[STAT_ARMOR] = other->client->ps.stats[STAT_MAX_HEALTH] * ent->item->giTag;
		}

		return G_ItemRespawnTime(ent); // GalaxyRP fix: [Items] see G_ItemRespawnTime
	}
	else
	{
		return adjustRespawnTime(1, ent->item->giType, ent->item->giTag);
	}
}

//======================================================================

/*
===============
G_ItemMoveHome

GalaxyRP fix: [Items] puts a map item that Force Push/Pull moved back on its spot. Split out of
RespawnItem so G_ReturnPushedItem can share it, and taught about suspended items: those were given
TR_GRAVITY like any other and dropped to the floor the first time they came back. A suspended item is
put back stationary, standing on the world (G_RunItem turns an item with no ground entity back into a
falling one), and compared on height too, since a straight pull down moves it on z only. Everything
else keeps the old rule: compared on x and y, and let fall from its spawn point to the floor.
===============
*/
static qboolean G_ItemMoveHome( gentity_t *ent )
{
	const qboolean suspended = ( ent->spawnflags & ITMSF_SUSPEND ) ? qtrue : qfalse;

	if ( ent->r.currentOrigin[0] == ent->s.origin[0] && ent->r.currentOrigin[1] == ent->s.origin[1]
		&& ( !suspended || ent->r.currentOrigin[2] == ent->s.origin[2] ) )
	{
		return qfalse; // zyk: already home
	}

	G_SetOrigin( ent, ent->s.origin );

	if ( suspended )
	{
		ent->s.groundEntityNum = ENTITYNUM_WORLD;
	}
	else
	{
		ent->s.pos.trType = TR_GRAVITY;
		ent->s.pos.trTime = level.time;
	}

	return qtrue;
}

/*
===============
G_ReturnPushedItem

GalaxyRP fix: [Items] think for a map item Force Push/Pull moved and nobody picked up: back to its
spot after G_ItemPushReturnTime. The push used to set RespawnItem for this, which for an item in a
"team" group picks a RANDOM member to bring back -- so the pushed one could stay where it was pushed
while another appeared. This moves only this item. If someone picks it up first, Touch_Item replaces
this think with RespawnItem, which brings it home when it respawns.
===============
*/
void G_ReturnPushedItem( gentity_t *ent )
{
	ent->nextthink = 0;

	if ( !G_ItemMoveHome( ent ) )
	{
		return; // zyk: pushed, but it came to rest on its own spot
	}

	trap->LinkEntity( (sharedEntity_t *)ent );
	G_AddEvent( ent, EV_ITEM_RESPAWN, 0 ); // zyk: the respawn effect, so players see it return
}

/*
===============
RespawnItem
===============
*/
// GalaxyRP fix: [Entity System] whether one entity of an item "team" group can be the one that comes
// back. G_FindTeams() builds the group as raw pointers, and nothing unhooks a member that is freed
// later (/entremove, /entload, a preset respawn), so the chain can lead into a free slot or into one
// that has since been reused by something else entirely.
static qboolean RP_ItemTeamMemberValid( gentity_t *member, gentity_t *master )
{
	return ( member && member->inuse && member->s.eType == ET_ITEM && member->item &&
		member->teammaster == master ) ? qtrue : qfalse;
}

void RespawnItem( gentity_t *ent ) {
	// randomly select from teamed entities
	if (ent->team) {
		gentity_t	*master;
		gentity_t	*member;
		int	count;
		int choice;
		int steps;

		// GalaxyRP fix: [Entity System] this was trap->Error(ERR_DROP, "RespawnItem: bad teammaster"),
		// a process exit on a dedicated server. Groups are only linked by G_FindTeams(), which runs at
		// map load and after /entload, so any item given a "team" key by /entadd or /entedit has no
		// master: the first time someone picked it up, its respawn ended the server. And when the
		// master itself was removed, the others kept pointing at its freed slot. An item whose group
		// is not intact now simply respawns on its own -- as does one the chain no longer reaches --
		// and the random pick below only ever lands on a member that is still a live item of this
		// group. steps bounds both walks, so a chain bent by slot reuse cannot loop.
		master = ent->teammaster;
		count = 0;

		if ( RP_ItemTeamMemberValid( master, master ) && RP_ItemTeamMemberValid( ent, master ) )
		{
			qboolean reachable = qfalse;

			for (member = master, steps = 0; member && steps < MAX_GENTITIES; member = member->teamchain, steps++)
			{
				if ( RP_ItemTeamMemberValid( member, master ) )
					count++;
				if ( member == ent )
					reachable = qtrue;
			}

			// zyk: a freed member cuts the chain there (G_FreeEntity() zeroes its teamchain), so an
			// item beyond it is no longer reachable from its master and could never be picked
			// again. Bring that one back itself rather than lose it from the map.
			if ( !reachable )
				count = 0;
		}

		if ( count > 0 )
		{
			choice = rand() % count;

			for (member = master, steps = 0; member && steps < MAX_GENTITIES; member = member->teamchain, steps++)
			{
				if ( !RP_ItemTeamMemberValid( member, master ) )
					continue;
				if ( choice == 0 )
				{
					ent = member;
					break;
				}
				choice--;
			}
		}
	}

	// zyk: if a player pushed/pulled this item, make it return to its default origin
	(void)G_ItemMoveHome(ent);

	ent->r.contents = CONTENTS_TRIGGER;
	//ent->s.eFlags &= ~EF_NODRAW;
	ent->s.eFlags &= ~(EF_NODRAW | EF_ITEMPLACEHOLDER);
	ent->r.svFlags &= ~SVF_NOCLIENT;
	// GalaxyRP fix: [SP Maps] shown by its name within the 300 ms a target_give keeps a given item in every
	// snapshot (Use_Target_Give()), it went on being sent to every client: nextthink = 0 below cancels the
	// unlink that would have ended it. No item is sent to everyone otherwise.
	ent->r.svFlags &= ~SVF_BROADCAST;
	trap->LinkEntity ((sharedEntity_t *)ent);

	if ( ent->item->giType == IT_POWERUP ) {
		// play powerup spawn sound to all clients
		gentity_t	*te;

		// if the powerup respawn sound should Not be global
		if (ent->speed) {
			te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
		}
		else {
			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		}
		te->s.eventParm = G_SoundIndex( "sound/items/respawn1" );
		te->r.svFlags |= SVF_BROADCAST;
	}

	// play the normal respawn sound only to nearby clients
	G_AddEvent( ent, EV_ITEM_RESPAWN, 0 );

	ent->nextthink = 0;
}

qboolean CheckItemCanBePickedUpByNPC( gentity_t *item, gentity_t *pickerupper )
{
	if ( (item->flags&FL_DROPPED_ITEM)
		&& item->activator != &g_entities[0]
		&& pickerupper->s.number
		&& pickerupper->s.weapon == WP_NONE
		&& pickerupper->enemy
		&& pickerupper->painDebounceTime < level.time
		&& pickerupper->NPC && pickerupper->NPC->surrenderTime < level.time //not surrendering
		&& !(pickerupper->NPC->scriptFlags&SCF_FORCED_MARCH) //not being forced to march
		/*&& item->item->giTag != INV_SECURITY_KEY*/ )
	{//non-player, in combat, picking up a dropped item that does NOT belong to the player and it *not* a security key
		if ( level.time - item->s.time < 3000 )//was 5000
		{
			return qfalse;
		}
		return qtrue;
	}
	return qfalse;
}

/*
===============
Touch_Item
===============
*/
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace) {
	int			respawn;
	qboolean	predict;

	if (ent->genericValue10 > level.time &&
		other &&
		other->s.number == ent->genericValue11)
	{ //this is the ent that we don't want to be able to touch us for x seconds
		return;
	}

	if (ent->s.eFlags & EF_ITEMPLACEHOLDER)
	{
		return;
	}

	if (ent->s.eFlags & EF_NODRAW)
	{
		return;
	}

	if (ent->item->giType == IT_WEAPON &&
		ent->s.powerups &&
		ent->s.powerups < level.time)
	{
		ent->s.generic1 = 0;
		ent->s.powerups = 0;
	}

	if (!other->client)
		return;
	if (other->health < 1)
		return;		// dead people can't pickup

	// GalaxyRP fix: [Quests] a `zyk_quest_artifact` pickup handler used to live here, along with the
	// `ent->spawnflags & 131072` (Universe Quest prison key) branch just below in the RPG-class pickup
	// restrictions block. Both were part of the general automated Universe Quest system, which is
	// fully unreachable now that quest_get_new_player, its sole entry point, has been deleted as
	// unreachable dead code (see the GalaxyRP fix comment on its old location in g_cmds.c). Removed
	// outright, along with the universe_quest_artifacts_checker call the first block was the last
	// live caller of.

	// zyk: Some RPG classes cant pickup some items
	if (other->client->sess.amrpgmode == 2)
	{
		// GalaxyRP fix: [RPG classes] removed a dead six-way pers.rpg_class pickup
		// restriction chain (classes 1/3/4/6/8/9); pers.rpg_class is permanently 0 now,
		// so pickup restriction never applies for any class -- falls through to normal
		// pickup logic below unconditionally.
	}
	// DAJ_RP: [Admin] a player given force powers with /give <player> force used to be refused every weapon,
	// ammo and holdable item here. Removed: he picks up whatever anyone else can, like a /give guns player.
	else if (level.duel_tournament_mode == 4 && other->s.number < MAX_CLIENTS && duel_tournament_is_duelist(other) == qtrue)
	{ // zyk: duelists in Duel Tournament cannot grab anything
		return;
	}

	if (ent->item->giType == IT_POWERUP &&
		(ent->item->giTag == PW_FORCE_ENLIGHTENED_LIGHT || ent->item->giTag == PW_FORCE_ENLIGHTENED_DARK))
	{ // GalaxyRP: [Force Enlightenment] DAJ_RP: logged-in players cannot pick up either color. The
	  // effect never applied to them -- WP_ForcePowersUpdate() (w_force.c) only boosts players with
	  // sess.amrpgmode < 2 -- so a pickup did nothing but bank a boost that kicked in after /logout
	  // and then clobbered the next character's powers at /login. This used to be the opposite
	  // carve-out (logged-in players could take either color); refusing both is the simpler rule.
	  // cgame's CG_GreyItem() (cg_ents.c) carries the same rule for the grey-out and for pickup
	  // prediction, so the item is drawn disabled and no pickup sound is predicted -- keep the two
	  // in step. Logged-out players keep the vanilla rule: only their own side's color.
		if (other->client->sess.loggedin == qtrue)
		{
			return;
		}

		if (ent->item->giTag == PW_FORCE_ENLIGHTENED_LIGHT)
		{
			if (other->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
			{
				return;
			}
		}
		else
		{
			if (other->client->ps.fd.forceSide != FORCE_DARKSIDE)
			{
				return;
			}
		}
	}

	// GalaxyRP fix: [RPG classes] removed a dead Bounty Hunter Upgrade extra-sentry-grab
	// block gated on pers.rpg_class == 2, which is permanently 0 now -- and, later, the
	// zyk_bounty_sentry_validation flag it used to set, which nothing else ever set.

	// the same pickup rules are used for client side and server side
	if ( !BG_CanItemBeGrabbed( level.gametype, &ent->s, &other->client->ps ) ) {
		return;
	}


	if ( other->client->NPC_class == CLASS_ATST ||
		other->client->NPC_class == CLASS_GONK ||
		other->client->NPC_class == CLASS_MARK1 ||
		other->client->NPC_class == CLASS_MARK2 ||
		other->client->NPC_class == CLASS_MOUSE ||
		other->client->NPC_class == CLASS_PROBE ||
		other->client->NPC_class == CLASS_PROTOCOL ||
		other->client->NPC_class == CLASS_R2D2 ||
		other->client->NPC_class == CLASS_R5D2 ||
		other->client->NPC_class == CLASS_SEEKER ||
		other->client->NPC_class == CLASS_REMOTE ||
		other->client->NPC_class == CLASS_RANCOR ||
		other->client->NPC_class == CLASS_WAMPA ||
		//other->client->NPC_class == CLASS_JAWA || //FIXME: in some cases it's okay?
		other->client->NPC_class == CLASS_UGNAUGHT || //FIXME: in some cases it's okay?
		other->client->NPC_class == CLASS_SENTRY )
	{//FIXME: some flag would be better
		//droids can't pick up items/weapons!
		return;
	}

	if ( CheckItemCanBePickedUpByNPC( ent, other ) )
	{
		if ( other->NPC && other->NPC->goalEntity && other->NPC->goalEntity->enemy == ent )
		{//they were running to pick me up, they did, so clear goal
			other->NPC->goalEntity = NULL;
			other->NPC->squadState = SQUAD_STAND_AND_SHOOT;
		}
	}
	else if ( !(ent->spawnflags &  ITMSF_ALLOWNPC) )
	{// NPCs cannot pick it up
		if ( other->s.eType == ET_NPC )
		{// Not the player?
			qboolean dontGo = qfalse;
			if (ent->item->giType == IT_AMMO &&
				ent->item->giTag == -1 &&
				other->s.NPC_class == CLASS_VEHICLE &&
				other->m_pVehicle &&
				other->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
			{ //yeah, uh, atst gets healed by these things
                if (other->maxHealth &&
					other->health < other->maxHealth)
				{
					other->health += 80;
					if (other->health > other->maxHealth)
					{
						other->health = other->maxHealth;
					}
					G_ScaleNetHealth(other);
					dontGo = qtrue;
				}
			}

			if (!dontGo)
			{
				return;
			}
		}
	}

	G_LogPrintf( "Item: %i %s\n", other->s.number, ent->item->classname );

	predict = other->client->pers.predictItemPickup;

	// call the item-specific pickup function
	switch( ent->item->giType ) {
	case IT_WEAPON:
		respawn = Pickup_Weapon(ent, other);
//		predict = qfalse;
		predict = qtrue;
		break;
	case IT_AMMO:
		respawn = Pickup_Ammo(ent, other);
		if (ent->item->giTag == AMMO_THERMAL || ent->item->giTag == AMMO_TRIPMINE || ent->item->giTag == AMMO_DETPACK)
		{
			int weapForAmmo = 0;

			if (ent->item->giTag == AMMO_THERMAL)
			{
				weapForAmmo = WP_THERMAL;
			}
			else if (ent->item->giTag == AMMO_TRIPMINE)
			{
				weapForAmmo = WP_TRIP_MINE;
			}
			else
			{
				weapForAmmo = WP_DET_PACK;
			}

			if (other && other->client && other->client->ps.ammo[weaponData[weapForAmmo].ammoIndex] > 0 )
			{
				other->client->ps.stats[STAT_WEAPONS] |= (1 << weapForAmmo);
			}
		}
//		predict = qfalse;
		predict = qtrue;
		break;
	case IT_ARMOR:
		respawn = Pickup_Armor(ent, other);
//		predict = qfalse;
		predict = qtrue;
		break;
	case IT_HEALTH:
		respawn = Pickup_Health(ent, other);
//		predict = qfalse;
		predict = qtrue;
		break;
	case IT_POWERUP:
		respawn = Pickup_Powerup(ent, other);
		predict = qfalse;
//		predict = qtrue;
		break;
	case IT_TEAM:
		respawn = Pickup_Team(ent, other);
		break;
	case IT_HOLDABLE:
		respawn = Pickup_Holdable(ent, other);
		break;
	default:
		return;
	}

	if ( !respawn ) {
		return;
	}

	// play the normal pickup sound
	if (predict) {
		if (other->client)
		{
			BG_AddPredictableEventToPlayerstate( EV_ITEM_PICKUP, ent->s.number, &other->client->ps);
		}
		else
		{
			G_AddPredictableEvent( other, EV_ITEM_PICKUP, ent->s.number );
		}
	} else {
		G_AddEvent( other, EV_ITEM_PICKUP, ent->s.number );
	}

	// powerup pickups are global broadcasts
	if ( /*ent->item->giType == IT_POWERUP ||*/ ent->item->giType == IT_TEAM) {
		// if we want the global sound to play
		if (!ent->speed) {
			gentity_t	*te;

			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm = ent->s.modelindex;
			te->r.svFlags |= SVF_BROADCAST;
		} else {
			gentity_t	*te;

			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm = ent->s.modelindex;
			// only send this temp entity to a single client
			te->r.svFlags |= SVF_SINGLECLIENT;
			te->r.singleClient = other->s.number;
		}
	}

	// fire item targets
	G_UseTargets (ent, other);

	// wait of -1 will not respawn
	if ( ent->wait == -1 ) {
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		ent->unlinkAfterEvent = qtrue;
		return;
	}

	// non zero wait overrides respawn time
	if ( ent->wait ) {
		respawn = ent->wait;
	}

	// random can be used to vary the respawn time
	if ( ent->random ) {
		respawn += Q_flrand(-1.0f, 1.0f) * ent->random;
		if ( respawn < 1 ) {
			respawn = 1;
		}
	}

	// dropped items will not respawn
	if ( ent->flags & FL_DROPPED_ITEM ) {
		ent->freeAfterEvent = qtrue;
	}

	// picked up items still stay around, they just don't
	// draw anything.  This allows respawnable items
	// to be placed on movers.
	// DAJ_RP: [Still Weapons] a still weapon (EF_RP_STILL_ITEM, RP_ItemSetStill()) leaves no ghost: it is hidden and
	// not sent while it waits, as ammo, health and shields are, and RespawnItem() brings it back. A HOLOGRAM weapon
	// (4096) and the powerups keep the see-through placeholder on their pad.
	if (!(ent->flags & FL_DROPPED_ITEM) &&
		((ent->item->giType == IT_WEAPON && !(ent->s.eFlags & EF_RP_STILL_ITEM)) || ent->item->giType == IT_POWERUP))
	{
		ent->s.eFlags |= EF_ITEMPLACEHOLDER;
		ent->s.eFlags &= ~EF_NODRAW;
	}
	else
	{
		ent->s.eFlags |= EF_NODRAW;
		ent->r.svFlags |= SVF_NOCLIENT;
	}
	ent->r.contents = 0;

	if (ent->genericValue9)
	{ //dropped item, should be removed when picked up
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	// ZOID
	// A negative respawn times means to never respawn this item (but don't
	// delete it).  This is used by items that are respawned by third party
	// events such as ctf flags
	if ( respawn <= 0 ) {
		ent->nextthink = 0;
		ent->think = 0;
	} else {
		ent->nextthink = level.time + respawn * 1000;
		ent->think = RespawnItem;
	}
	trap->LinkEntity( (sharedEntity_t *)ent );
}


//======================================================================

/*
================
LaunchItem

Spawns an item and tosses it forward
================
*/
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity ) {
	gentity_t	*dropped;

	dropped = G_Spawn();

	dropped->s.eType = ET_ITEM;
	dropped->s.modelindex = item - bg_itemlist;	// store item number in modelindex
	if (dropped->s.modelindex < 0)
	{
		dropped->s.modelindex = 0;
	}
	dropped->s.modelindex2 = 1; // This is non-zero is it's a dropped item

	dropped->classname = item->classname;
	dropped->item = item;
	VectorSet (dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
	VectorSet (dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);

	dropped->r.contents = CONTENTS_TRIGGER;

	dropped->touch = Touch_Item;

	G_SetOrigin( dropped, origin );
	dropped->s.pos.trType = TR_GRAVITY;
	dropped->s.pos.trTime = level.time;
	VectorCopy( velocity, dropped->s.pos.trDelta );

	dropped->flags |= FL_BOUNCE_HALF;
	if ((level.gametype == GT_CTF || level.gametype == GT_CTY) && item->giType == IT_TEAM) { // Special case for CTF flags
		dropped->think = Team_DroppedFlagThink;
		dropped->nextthink = level.time + 30000;
		Team_CheckDroppedItem( dropped );

		//rww - so bots know
		if (dropped->item->giTag == PW_REDFLAG)
		{
			droppedRedFlag = dropped;
		}
		else if (dropped->item->giTag == PW_BLUEFLAG)
		{
			droppedBlueFlag = dropped;
		}
	} else { // auto-remove after rp_item_lifetime seconds
		// DAJ_RP: [Items] was a hard-coded 300000 (zyk's 5 minutes, up from stock's 30 s); now
		// rp_item_lifetime, default 100 s, clamped to 0..200 s. At 0 the item is freed on the next frame.
		dropped->think = G_FreeEntity;
		dropped->nextthink = level.time + RP_CorpseSecondsToMs( rp_item_lifetime.integer );
	}

	dropped->flags = FL_DROPPED_ITEM;

	if (item->giType == IT_WEAPON || item->giType == IT_POWERUP)
	{
		dropped->s.eFlags |= EF_DROPPEDWEAPON;
	}

	vectoangles(velocity, dropped->s.angles);
	dropped->s.angles[PITCH] = 0;

	if (item->giTag == WP_TRIP_MINE ||
		item->giTag == WP_DET_PACK)
	{
		dropped->s.angles[PITCH] = -90;
	}

	if (item->giTag != WP_BOWCASTER &&
		item->giTag != WP_DET_PACK &&
		item->giTag != WP_THERMAL)
	{
		dropped->s.angles[ROLL] = -90;
	}

	dropped->physicsObject = qtrue;

	trap->LinkEntity ((sharedEntity_t *)dropped);

	return dropped;
}

/*
================
Drop_Item

Spawns an item and tosses it forward
================
*/
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle ) {
	vec3_t	velocity;
	vec3_t	angles;

	VectorCopy( ent->s.apos.trBase, angles );
	angles[YAW] += angle;
	angles[PITCH] = 0;	// always forward

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 150, velocity );
	velocity[2] += 200 + Q_flrand(-1.0f, 1.0f) * 50;

	return LaunchItem( item, ent->s.pos.trBase, velocity );
}


/*
================
Use_Item

Respawn the item
================
*/
void Use_Item( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	RespawnItem( ent );
}

//======================================================================

/*
================
RP_ItemSetStill

DAJ_RP: [Still Weapons] whether this placed item is a weapon the GalaxyRP client draws lying still, as single player
does (EF_RP_STILL_ITEM, bg_public.h), and at which angles. Every placed weapon pickup is -- the map's, /entadd's,
/entedit's, a preset's, a misc_model_gun_rack's -- unless it has HOLOGRAM (4096); weapons dropped by players or NPCs
(LaunchItem()) never come through here, and no other item type is. The angles go in s.apos.trBase, which the stock
client ignores on an item (it reads s.angles): the map's angles, and with VERTICAL (16) tilted up as single player's
weapon lockers are -- 75 degrees, a saber 20 (single player takes a saber's pitch from its "random" key, which is the
respawn time here). Kept out of s.angles so the entity commands, which read s.angles back when a weapon has no angles
key, never tilt it twice. The bit is cleared first, so each call decides afresh (an item spawned again in place,
/entedit or /entrotate, has its eFlags cleared already by RP_EntResetForRespawn()). Called by FinishSpawningItem(),
and again by GunRackAddItem() (g_misc.c), which turns the item afterwards.
================
*/
void RP_ItemSetStill( gentity_t *ent )
{
	ent->s.eFlags &= ~EF_RP_STILL_ITEM;

	if ( !ent->item || ent->item->giType != IT_WEAPON || ( ent->spawnflags & ITMSF_HOLOGRAM ) )
		return;

	ent->s.eFlags |= EF_RP_STILL_ITEM;

	ent->s.apos.trType = TR_STATIONARY;
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
	VectorClear( ent->s.apos.trDelta );

	if ( ent->spawnflags & ITMSF_VERTICAL )
		ent->s.apos.trBase[PITCH] -= ( ent->item->giTag == WP_SABER ) ? 20 : 75;
}

/*
================
FinishSpawningItem

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
void FinishSpawningItem( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		dest;
//	gitem_t		*item;

//	VectorSet( ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS );
//	VectorSet( ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS );

	if (level.gametype == GT_SIEGE)
	{ //in siege remove all powerups
		if (ent->item->giType == IT_POWERUP)
		{
			G_FreeEntity(ent);
			return;
		}
	}

	if (level.gametype != GT_JEDIMASTER)
	{
		if (HasSetSaberOnly())
		{
			if (ent->item->giType == IT_AMMO)
			{
				G_FreeEntity( ent );
				return;
			}

			if (ent->item->giType == IT_HOLDABLE)
			{
				if (ent->item->giTag == HI_SEEKER ||
					ent->item->giTag == HI_SHIELD ||
					ent->item->giTag == HI_SENTRY_GUN)
				{
					G_FreeEntity( ent );
					return;
				}
			}
		}
	}
	else
	{ //no powerups in jedi master
		if (ent->item->giType == IT_POWERUP)
		{
			G_FreeEntity(ent);
			return;
		}
	}

	if (level.gametype == GT_HOLOCRON)
	{
		if (ent->item->giType == IT_POWERUP)
		{
			if (ent->item->giTag == PW_FORCE_ENLIGHTENED_LIGHT ||
				ent->item->giTag == PW_FORCE_ENLIGHTENED_DARK)
			{
				G_FreeEntity(ent);
				return;
			}
		}
	}

	// GalaxyRP fix: [Force] duel-aware, see G_ForcePowerDisableValue() -- force powerups used to keep
	// spawning in Duel/Power Duel when only g_duelForcePowerDisable was set.
	if (G_ForcePowerDisableValue())
	{ //if force powers disabled, don't add force powerups
		if (ent->item->giType == IT_POWERUP)
		{
			if (ent->item->giTag == PW_FORCE_ENLIGHTENED_LIGHT ||
				ent->item->giTag == PW_FORCE_ENLIGHTENED_DARK ||
				ent->item->giTag == PW_FORCE_BOON)
			{
				G_FreeEntity(ent);
				return;
			}
		}
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		if ( ent->item->giType == IT_ARMOR ||
			ent->item->giType == IT_HEALTH ||
			(ent->item->giType == IT_HOLDABLE && (ent->item->giTag == HI_MEDPAC || ent->item->giTag == HI_MEDPAC_BIG)) )
		{
			G_FreeEntity(ent);
			return;
		}
	}

	if (level.gametype != GT_CTF &&
		level.gametype != GT_CTY &&
		ent->item->giType == IT_TEAM)
	{
		int killMe = 0;

		switch (ent->item->giTag)
		{
		case PW_REDFLAG:
			killMe = 1;
			break;
		case PW_BLUEFLAG:
			killMe = 1;
			break;
		case PW_NEUTRALFLAG:
			killMe = 1;
			break;
		default:
			break;
		}

		if (killMe)
		{
			G_FreeEntity( ent );
			return;
		}
	}

	VectorSet (ent->r.mins, -8, -8, -0);
	VectorSet (ent->r.maxs, 8, 8, 16);

	ent->s.eType = ET_ITEM;
	ent->s.modelindex = ent->item - bg_itemlist;		// store item number in modelindex
	ent->s.modelindex2 = 0; // zero indicates this isn't a dropped item

	RP_ItemSetStill( ent );	// DAJ_RP: [Still Weapons]

	ent->r.contents = CONTENTS_TRIGGER;
	ent->touch = Touch_Item;
	// useing an item causes it to respawn
	ent->use = Use_Item;

	// create a Ghoul2 model if the world model is a glm
/*	item = &bg_itemlist[ ent->s.modelindex ];
	if (!Q_stricmp(&item->world_model[0][strlen(item->world_model[0]) - 4], ".glm"))
	{
		trap->G2API_InitGhoul2Model(&ent->s, item->world_model[0], G_ModelIndex(item->world_model[0] ), 0, 0, 0, 0);
		ent->s.radius = 60;
	}
*/
	if ( ent->spawnflags & ITMSF_SUSPEND ) {
		// suspended
		G_SetOrigin( ent, ent->s.origin );
	} else {
		// drop to floor

		//if it is directly even with the floor it will return startsolid, so raise up by 0.1
		//and temporarily subtract 0.1 from the z maxs so that going up doesn't push into the ceiling
		ent->s.origin[2] += 0.1f;
		ent->r.maxs[2] -= 0.1f;

		VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
		trap->Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID, qfalse, 0, 0 );
		if ( tr.startsolid ) {
			trap->Print ("FinishSpawningItem: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
			G_FreeEntity( ent );
			return;
		}

		//add the 0.1 back after the trace
		ent->r.maxs[2] += 0.1f;

		// allow to ride movers
		ent->s.groundEntityNum = tr.entityNum;

		G_SetOrigin( ent, tr.endpos );
	}

	// team slaves and targeted items aren't present at start
	if ( ( ent->flags & FL_TEAMSLAVE ) || ent->targetname ) {
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		return;
	}

	// powerups don't spawn in for a while
	/*
	if ( ent->item->giType == IT_POWERUP ) {
		float	respawn;

		respawn = 45 + Q_flrand(-1.0f, 1.0f) * 15;
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		ent->nextthink = level.time + respawn * 1000;
		ent->think = RespawnItem;
		return;
	}
	*/

	trap->LinkEntity ((sharedEntity_t *)ent);
}


qboolean	itemRegistered[MAX_ITEMS];

/*
==================
G_CheckTeamItems
==================
*/
void G_CheckTeamItems( void ) {

	// Set up team stuff
	Team_InitGame();

	if( level.gametype == GT_CTF || level.gametype == GT_CTY ) {
		gitem_t	*item;

		// check for the two flags
		item = BG_FindItem( "team_CTF_redflag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			trap->Print( S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n" );
		}
		item = BG_FindItem( "team_CTF_blueflag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			trap->Print( S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n" );
		}
	}
}

/*
==============
ClearRegisteredItems
==============
*/
void ClearRegisteredItems( void ) {
	memset( itemRegistered, 0, sizeof( itemRegistered ) );

	// players always start with the base weapon
	RegisterItem( BG_FindItemForWeapon( WP_BRYAR_PISTOL ) );
	RegisterItem( BG_FindItemForWeapon( WP_STUN_BATON ) );
	RegisterItem( BG_FindItemForWeapon( WP_MELEE ) );
	RegisterItem( BG_FindItemForWeapon( WP_SABER ) );

	if (level.gametype == GT_SIEGE)
	{ //kind of cheesy, maybe check if siege class with disp's is gonna be on this map too
		G_PrecacheDispensers();
	}
}

/*
===============
RegisterItem

The item will be added to the precache list
===============
*/
void RegisterItem( gitem_t *item ) {
	if ( !item ) {
		trap->Error( ERR_DROP, "RegisterItem: NULL" );
	}
	itemRegistered[ item - bg_itemlist ] = qtrue;
}


/*
===============
SaveRegisteredItems

Write the needed items to a config string
so the client will know which ones to precache
===============
*/
void SaveRegisteredItems( void ) {
	char	string[MAX_ITEMS+1];
	int		i;
	int		count;

	count = 0;
	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( itemRegistered[i] ) {
			count++;
			string[i] = '1';
		} else {
			string[i] = '0';
		}
	}
	string[ bg_numItems ] = 0;

//	trap->Print( "%i items registered\n", count );
	trap->SetConfigstring(CS_ITEMS, string);
}

/*
============
G_ItemDisabled
============
*/
int G_ItemDisabled( gitem_t *item ) {

	char name[128];

	Com_sprintf(name, sizeof(name), "disable_%s", item->classname);
	return trap->Cvar_VariableIntegerValue( name );
}

/*
============
G_SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
extern qboolean itemRegistered[MAX_ITEMS];	// GalaxyRP: [Entity System] below, with RegisterItem()

void G_SpawnItem (gentity_t *ent, gitem_t *item) {
	int wDisable = 0;

	// GalaxyRP fix: [Items] a placed weapon_melee spawns as a stun baton. The two items share one world
	// model (baton_w.glm), so a melee pickup looks exactly like a stun baton on its pad -- but every
	// player always owns melee, and melee has no ammo (AMMO_NONE, whose cap is 0), so
	// BG_CanItemBeGrabbed() refused it to everyone, forever: a "stun baton" nobody could pick up. Swap
	// the item here, before the weapon-disable test below, so g_weaponDisable, g_duelWeaponDisable and
	// disable_weapon_stun_baton judge the weapon the pickup actually gives. Every placed item comes
	// through this function (map load, /entadd, /entload, /entedit); /entsave writes the entity's
	// original spawn strings, not this classname, so a saved preset keeps weapon_melee and is swapped
	// again on every load.
	if ( item->giType == IT_WEAPON && item->giTag == WP_MELEE )
	{
		item = BG_FindItemForWeapon( WP_STUN_BATON );
		ent->classname = item->classname;
	}

	G_SpawnFloat( "random", "0", &ent->random );

	// zyk: this spawnflags allow setting wait with entadd command
	if (!(ent->spawnflags & 65536))
		G_SpawnFloat( "wait", "0", &ent->wait );

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		wDisable = g_duelWeaponDisable.integer;
	}
	else
	{
		wDisable = g_weaponDisable.integer;
	}

	if (item->giType == IT_WEAPON &&
		wDisable &&
		(wDisable & (1 << item->giTag)) && 
		!(ent->spawnflags & 131072)) //zyk: new condition. Do not apply this rule for special quest items
	{
		if (level.gametype != GT_JEDIMASTER)
		{
			// DAJ_RP: [Entity System] /entadd and the rest say why, as for the disable_ cvar below
			Q_strncpyz( level.rp_spawn_refusal, va( "%s is switched off on this server (%s)", item->classname,
				( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) ? "g_duelWeaponDisable" : "g_weaponDisable" ),
				sizeof( level.rp_spawn_refusal ) );
			G_FreeEntity( ent );
			return;
		}
	}

	// GalaxyRP fix: [Entity System] an item class the map had not precached, spawned after map load
	// (/entadd, /entedit, a preset): the client registers item visuals from CS_ITEMS, which was written
	// once at G_InitGame (SaveRegisteredItems), so the item was invisible -- a hologram cone only --
	// until the map restarted. Written again now, for every later joiner, and for connected clients
	// whose cgame registers a late CS_ITEMS change (CG_ConfigStringModified); older cgames keep the
	// cone, which the note tells the admin.
	// GalaxyRP fix: [Entity System] tested against CS_ITEMS itself, not itemRegistered[]: every NPC
	// spawned after map load registers the items of the weapons it carries (NPC_PrecacheWeapons),
	// without writing CS_ITEMS, so the table said "precached" for a weapon no client had ever seen.
	if ( level.rp_map_loaded )
	{
		char	items[MAX_STRING_CHARS];
		int		index = (int)( item - bg_itemlist );

		trap->GetConfigstring( CS_ITEMS, items, sizeof( items ) );
		if ( index >= 0 && index < bg_numItems && ( index >= (int)strlen( items ) || items[index] != '1' ) )
		{
			RegisterItem( item );
			SaveRegisteredItems();
			Q_strncpyz( level.rp_spawn_note, va( "%s was not precached by this map: players with an older client see a placeholder for it until the map restarts.", item->classname ),
				sizeof( level.rp_spawn_note ) );
			G_LogPrintf( "item %s precached after map load (CS_ITEMS rewritten)\n", item->classname );
		}
	}

	RegisterItem( item );
	// DAJ_RP fix: [Entity System] freed, as a weapon g_weaponDisable switches off is above: it used to stay as an inert
	// entity with no item -- an entity slot held for nothing, shown in /entlist, and reported by /entadd as spawned
	// (stock multiplayer is the same). Every caller already handles a freed entity (G_CallSpawn, the gun rack).
	if ( G_ItemDisabled(item) )
	{
		Q_strncpyz( level.rp_spawn_refusal, va( "disable_%s is 1 on this server", item->classname ), sizeof( level.rp_spawn_refusal ) );
		G_FreeEntity( ent );
		return;
	}

	ent->item = item;
	// some movers spawn on the second frame, so delay item
	// spawns until the third frame so they can ride trains
	ent->nextthink = level.time + FRAMETIME * 2;
	ent->think = FinishSpawningItem;

	ent->physicsBounce = 0.50;		// items are bouncy

	if ( item->giType == IT_POWERUP ) {
		G_SoundIndex( "sound/items/respawn1" );
		G_SpawnFloat( "noglobalsound", "0", &ent->speed);
	}
}


/*
================
G_BounceItem

================
*/
void G_BounceItem( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

	// cut the velocity to keep from bouncing forever
	VectorScale( ent->s.pos.trDelta, ent->physicsBounce, ent->s.pos.trDelta );

	if ((ent->s.weapon == WP_DET_PACK && ent->s.eType == ET_GENERAL && ent->physicsObject))
	{ //detpacks only
		if (ent->touch)
		{
			ent->touch(ent, &g_entities[trace->entityNum], trace);
			return;
		}
	}

	// check for stop
	if ( trace->plane.normal[2] > 0 && ent->s.pos.trDelta[2] < 40 ) {
		trace->endpos[2] += 1.0;	// make sure it is off ground
		SnapVector( trace->endpos );
		G_SetOrigin( ent, trace->endpos );
		ent->s.groundEntityNum = trace->entityNum;
		return;
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	if (ent->s.eType == ET_HOLOCRON ||
		(ent->s.shouldtarget && ent->s.eType == ET_GENERAL && ent->physicsObject))
	{ //holocrons and sentry guns
		if (ent->touch)
		{
			ent->touch(ent, &g_entities[trace->entityNum], trace);
		}
	}
}


/*
================
G_RunItem

================
*/
void G_RunItem( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			contents;
	int			mask;

	// if groundentity has been set to ENTITYNUM_NONE, it may have been pushed off an edge
	if ( ent->s.groundEntityNum == ENTITYNUM_NONE ) {
		if ( ent->s.pos.trType != TR_GRAVITY ) {
			ent->s.pos.trType = TR_GRAVITY;
			ent->s.pos.trTime = level.time;
		}
	}

	if ( ent->s.pos.trType == TR_STATIONARY ) {
		// check think function
		G_RunThink( ent );
		return;
	}

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// trace a line from the previous position to the current position
	if ( ent->clipmask ) {
		mask = ent->clipmask;
	} else {
		mask = MASK_PLAYERSOLID & ~CONTENTS_BODY;//MASK_SOLID;
	}
	trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, ent->r.ownerNum, mask, qfalse, 0, 0 );

	VectorCopy( tr.endpos, ent->r.currentOrigin );

	if ( tr.startsolid ) {

		tr.fraction = 0;
	}

	trap->LinkEntity( (sharedEntity_t *)ent );	// FIXME: avoid this for stationary?

	// check think function
	G_RunThink( ent );

	if ( tr.fraction == 1 ) {
		return;
	}

	// if it is in a nodrop volume, remove it
	contents = trap->PointContents( ent->r.currentOrigin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		if (ent->item && ent->item->giType == IT_TEAM) {
			Team_FreeEntity(ent);
		} else if(ent->genericValue15 == HI_SENTRY_GUN) {
			turret_free(ent);
		} else {
			G_FreeEntity( ent );
		}
		return;
	}

	G_BounceItem( ent, &tr );
}

