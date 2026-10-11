/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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
#include "bg_saga.h"
#include "sqlite/sqlite3.h"

// g_client.c -- client functions that don't happen every frame

static vec3_t	playerMins = {-15, -15, DEFAULT_MINS_2};
static vec3_t	playerMaxs = {15, 15, DEFAULT_MAXS_2};

/*
================
RP_SpawnPointInSolid / RP_SpawnPointRefusedAtRuntime

GalaxyRP fix: [Entity System] whether a player put on this point -- 9 units up, where ClientSpawn()
puts one -- would be inside the world's solid, a brush entity (a door, a func_static), player clip or
terrain, or outside the map, whose outer leaves are solid. Not inside a body: a player standing on
the point is SpotWouldTelefrag()'s own question, and the admin stands on the point /entadd makes.
Without the box (an intermission point: a camera) it is the point itself that is tested.

RP_SpawnPointRefusedAtRuntime(): a spawn point the Entity System made is refused when a player
spawning on it would be stuck -- an origin typed into a wall or out of the map, or a map's own point
/entedit'ed there. The map's own points are never tested: a map spawns what it spawns.
================
*/
#define RP_SPAWN_POINT_MASK		( MASK_PLAYERSOLID & ~CONTENTS_BODY )

qboolean RP_SpawnPointInSolid( const vec3_t origin, qboolean playerBox )
{
	trace_t	tr;
	vec3_t	start;

	VectorCopy( origin, start );
	start[2] += 9;

	if ( !playerBox )
		return ( trap->PointContents( start, ENTITYNUM_NONE ) & RP_SPAWN_POINT_MASK ) ? qtrue : qfalse;

	trap->Trace( &tr, start, playerMins, playerMaxs, start, ENTITYNUM_NONE, RP_SPAWN_POINT_MASK, qfalse, 0, 0 );
	return ( tr.startsolid || tr.allsolid ) ? qtrue : qfalse;
}

qboolean RP_SpawnPointRefusedAtRuntime( gentity_t *ent, qboolean playerBox )
{
	if ( !ent || !RP_EntitySystemMade( ent ) )
		return qfalse;

	if ( !RP_FINITE( ent->s.origin[0] ) || !RP_FINITE( ent->s.origin[1] ) || !RP_FINITE( ent->s.origin[2] ) )
		return RP_RefuseAtRuntime( ent, "its origin is not a number" );

	if ( RP_SpawnPointInSolid( ent->s.origin, playerBox ) )
	{
		return RP_RefuseAtRuntime( ent, playerBox
			? "a player spawning there would be inside a wall, a door or outside the map. A spawn point stands where the player will, 24 units above the floor: /entaddaim sets it so on the surface aimed at, /entadd takes where you stand"
			: "the point is inside a wall or outside the map" );
	}

	return qfalse;
}

extern int g_siegeRespawnCheck;

void WP_SaberAddG2Model( gentity_t *saberent, const char *saberModel, qhandle_t saberSkin );
void WP_SaberRemoveG2Model( gentity_t *saberent );
extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
// GalaxyRP fix: [NPC] declared here rather than by including b_local.h, which this file has never
// pulled in. ClientDisconnect() uses it to drop the departing player as any NPC's enemy.
extern void G_ClearEnemy( gentity_t *self );

forcedata_t Client_Force[MAX_CLIENTS];

/*QUAKED info_player_duel (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for duelists in duel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel( gentity_t *ent )
{
	int		i;

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_duel1 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for lone duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel1( gentity_t *ent )
{
	int		i;

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_duel2 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for paired duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel2( gentity_t *ent )
{
	int		i;

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch( gentity_t *ent ) {
	int		i;

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
Targets will be fired when someone spawns in on them.
equivelant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t *ent) {
	ent->classname = "info_player_deathmatch";
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_start_red (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Red Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_red(gentity_t *ent) {
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_start_blue (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Blue Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_blue(gentity_t *ent) {
	SP_info_player_deathmatch( ent );
}

void SiegePointUse( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	//Toggle the point on/off
	if (self->genericValue1)
	{
		self->genericValue1 = 0;
	}
	else
	{
		self->genericValue1 = 1;
	}
}

/*QUAKED info_player_siegeteam1 (1 0 0) (-16 -16 -24) (16 16 32)
siege start point - team1
name and behavior of team1 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam1(gentity_t *ent) {
	int soff = 0;

	if (level.gametype != GT_SIEGE)
	{ //turn into a DM spawn if not in siege game mode
		ent->classname = "info_player_deathmatch";
		SP_info_player_deathmatch( ent );

		return;
	}

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt("startoff", "0", &soff);

	if (soff)
	{ //start disabled
		ent->genericValue1 = 0;
	}
	else
	{
		ent->genericValue1 = 1;
	}

	ent->use = SiegePointUse;
}

/*QUAKED info_player_siegeteam2 (0 0 1) (-16 -16 -24) (16 16 32)
siege start point - team2
name and behavior of team2 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam2(gentity_t *ent) {
	int soff = 0;

	if (level.gametype != GT_SIEGE)
	{ //turn into a DM spawn if not in siege game mode
		ent->classname = "info_player_deathmatch";
		SP_info_player_deathmatch( ent );

		return;
	}

	if ( RP_SpawnPointRefusedAtRuntime( ent, qtrue ) )
		return;

	G_SpawnInt("startoff", "0", &soff);

	if (soff)
	{ //start disabled
		ent->genericValue1 = 0;
	}
	else
	{
		ent->genericValue1 = 1;
	}

	ent->use = SiegePointUse;
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) RED BLUE
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
RED - In a Siege game, the intermission will happen here if the Red (attacking) team wins
BLUE - In a Siege game, the intermission will happen here if the Blue (defending) team wins
*/
void SP_info_player_intermission( gentity_t *ent ) {
	RP_SpawnPointRefusedAtRuntime( ent, qfalse );
}

/*QUAKED info_player_intermission_red (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Red (attacking) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_red( gentity_t *ent ) {
	RP_SpawnPointRefusedAtRuntime( ent, qfalse );
}

/*QUAKED info_player_intermission_blue (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Blue (defending) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_blue( gentity_t *ent ) {
	RP_SpawnPointRefusedAtRuntime( ent, qfalse );
}

#define JMSABER_RESPAWN_TIME 20000 //in case it gets stuck somewhere no one can reach

void ThrowSaberToAttacker(gentity_t *self, gentity_t *attacker)
{
	gentity_t *ent = &g_entities[self->client->ps.saberIndex];
	vec3_t a;
	int altVelocity = 0;

	if (!ent || ent->enemy != self)
	{ //something has gone very wrong (this should never happen)
		//but in case it does.. find the saber manually
#ifdef _DEBUG
		Com_Printf("Lost the saber! Attempting to use global pointer..\n");
#endif
		ent = gJMSaberEnt;

		if (!ent)
		{
#ifdef _DEBUG
			Com_Printf("The global pointer was NULL. This is a bad thing.\n");
#endif
			return;
		}

#ifdef _DEBUG
		Com_Printf("Got it (%i). Setting enemy to client %i.\n", ent->s.number, self->s.number);
#endif

		ent->enemy = self;
		self->client->ps.saberIndex = ent->s.number;
	}

	trap->SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );

	if (attacker && attacker->client && self->client->ps.saberInFlight)
	{ //someone killed us and we had the saber thrown, so actually move this saber to the saber location
	  //if we killed ourselves with saber thrown, however, same suicide rules of respawning at spawn spot still
	  //apply.
		gentity_t *flyingsaber = &g_entities[self->client->ps.saberEntityNum];

		if (flyingsaber && flyingsaber->inuse)
		{
			VectorCopy(flyingsaber->s.pos.trBase, ent->s.pos.trBase);
			VectorCopy(flyingsaber->s.pos.trDelta, ent->s.pos.trDelta);
			VectorCopy(flyingsaber->s.apos.trBase, ent->s.apos.trBase);
			VectorCopy(flyingsaber->s.apos.trDelta, ent->s.apos.trDelta);

			VectorCopy(flyingsaber->r.currentOrigin, ent->r.currentOrigin);
			VectorCopy(flyingsaber->r.currentAngles, ent->r.currentAngles);
			altVelocity = 1;
		}
	}

	self->client->ps.saberInFlight = qtrue; //say he threw it anyway in order to properly remove from dead body

	WP_SaberAddG2Model( ent, self->client->saber[0].model, self->client->saber[0].skin );

	ent->s.eFlags &= ~(EF_NODRAW);
	ent->s.modelGhoul2 = 1;
	ent->s.eType = ET_MISSILE;
	ent->enemy = NULL;

	if (!attacker || !attacker->client)
	{
		VectorCopy(ent->s.origin2, ent->s.pos.trBase);
		VectorCopy(ent->s.origin2, ent->s.origin);
		VectorCopy(ent->s.origin2, ent->r.currentOrigin);
		ent->pos2[0] = 0;
		trap->LinkEntity((sharedEntity_t *)ent);
		return;
	}

	if (!altVelocity)
	{
		VectorCopy(self->s.pos.trBase, ent->s.pos.trBase);
		VectorCopy(self->s.pos.trBase, ent->s.origin);
		VectorCopy(self->s.pos.trBase, ent->r.currentOrigin);

		VectorSubtract(attacker->client->ps.origin, ent->s.pos.trBase, a);

		VectorNormalize(a);

		ent->s.pos.trDelta[0] = a[0]*256;
		ent->s.pos.trDelta[1] = a[1]*256;
		ent->s.pos.trDelta[2] = 256;
	}

	trap->LinkEntity((sharedEntity_t *)ent);
}

void JMSaberThink(gentity_t *ent)
{
	gJMSaberEnt = ent;

	if (ent->enemy)
	{
		if (!ent->enemy->client || !ent->enemy->inuse)
		{ //disconnected?
			VectorCopy(ent->enemy->s.pos.trBase, ent->s.pos.trBase);
			VectorCopy(ent->enemy->s.pos.trBase, ent->s.origin);
			VectorCopy(ent->enemy->s.pos.trBase, ent->r.currentOrigin);
			ent->s.modelindex = G_ModelIndex( DEFAULT_SABER_MODEL );
			ent->s.eFlags &= ~(EF_NODRAW);
			ent->s.modelGhoul2 = 1;
			ent->s.eType = ET_MISSILE;
			ent->enemy = NULL;

			ent->pos2[0] = 1;
			ent->pos2[1] = 0; //respawn next think
			trap->LinkEntity((sharedEntity_t *)ent);
		}
		else
		{
			ent->pos2[1] = level.time + JMSABER_RESPAWN_TIME;
		}
	}
	else if (ent->pos2[0] && ent->pos2[1] < level.time)
	{
		VectorCopy(ent->s.origin2, ent->s.pos.trBase);
		VectorCopy(ent->s.origin2, ent->s.origin);
		VectorCopy(ent->s.origin2, ent->r.currentOrigin);
		ent->pos2[0] = 0;
		trap->LinkEntity((sharedEntity_t *)ent);
	}

	ent->nextthink = level.time + 50;
	G_RunObject(ent);
}

void JMSaberTouch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	int i = 0;
//	gentity_t *te;

	if (!other || !other->client || other->health < 1)
	{
		return;
	}

	if (self->enemy)
	{
		return;
	}

	if (!self->s.modelindex)
	{
		return;
	}

	if (other->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
	{
		return;
	}

	if (other->client->ps.isJediMaster)
	{
		return;
	}

	self->enemy = other;
	other->client->ps.stats[STAT_WEAPONS] = (1 << WP_SABER);
	other->client->ps.weapon = WP_SABER;
	other->s.weapon = WP_SABER;
	other->client->ps.zoomMode = 0;
	G_AddEvent(other, EV_BECOME_JEDIMASTER, 0);

	// Track the jedi master
	trap->SetConfigstring ( CS_CLIENT_JEDIMASTER, va("%i", other->s.number ) );

	if (g_spawnInvulnerability.integer)
	{
		other->client->ps.eFlags |= EF_INVULNERABLE;
		other->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
	}

	trap->SendServerCommand( -1, va("cp \"%s %s\n\"", other->client->pers.netname, G_GetStringEdString("MP_SVGAME", "BECOMEJM")) );

	other->client->ps.isJediMaster = qtrue;
	other->client->ps.saberIndex = self->s.number;

	if (other->health < 200 && other->health > 0)
	{ //full health when you become the Jedi Master
		other->client->ps.stats[STAT_HEALTH] = other->health = 200;
	}

	if (other->client->ps.fd.forcePower < 100)
	{
		other->client->ps.fd.forcePower = 100;
	}

	while (i < NUM_FORCE_POWERS)
	{
		other->client->ps.fd.forcePowersKnown |= (1 << i);
		other->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;

		i++;
	}

	self->pos2[0] = 1;
	self->pos2[1] = level.time + JMSABER_RESPAWN_TIME;

	self->s.modelindex = 0;
	self->s.eFlags |= EF_NODRAW;
	self->s.modelGhoul2 = 0;
	self->s.eType = ET_GENERAL;

	/*
	te = G_TempEntity( vec3_origin, EV_DESTROY_GHOUL2_INSTANCE );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = self->s.number;
	*/
	G_KillG2Queue(self->s.number);

	return;
}

gentity_t *gJMSaberEnt = NULL;

/*QUAKED info_jedimaster_start (1 0 0) (-16 -16 -24) (16 16 32)
"jedi master" saber spawn point
*/
void SP_info_jedimaster_start(gentity_t *ent)
{
	// GalaxyRP fix: [Entity System] not after map load: the saber it makes is marked isSaberEntity,
	// which G_FreeEntity() refuses to free (so it could never be removed and the pool it took was gone
	// for the map), and every one re-points the single gJMSaberEnt. The map's own is the only one.
	if ( RP_RefuseAtRuntime( ent, "the Jedi Master saber cannot be added after map load: it could never be removed, and the map's own is the only one" ) )
	{
		return;
	}

	if (level.gametype != GT_JEDIMASTER)
	{
		gJMSaberEnt = NULL;
		G_FreeEntity(ent);
		return;
	}

	ent->enemy = NULL;

	ent->flags = FL_BOUNCE_HALF;

	ent->s.modelindex = G_ModelIndex( DEFAULT_SABER_MODEL );
	ent->s.modelGhoul2 = 1;
	ent->s.g2radius = 20;
	//ent->s.eType = ET_GENERAL;
	ent->s.eType = ET_MISSILE;
	ent->s.weapon = WP_SABER;
	ent->s.pos.trType = TR_GRAVITY;
	ent->s.pos.trTime = level.time;
	VectorSet( ent->r.maxs, 3, 3, 3 );
	VectorSet( ent->r.mins, -3, -3, -3 );
	ent->r.contents = CONTENTS_TRIGGER;
	ent->clipmask = MASK_SOLID;

	ent->isSaberEntity = qtrue;

	ent->bounceCount = -5;

	ent->physicsObject = qtrue;

	VectorCopy(ent->s.pos.trBase, ent->s.origin2); //remember the spawn spot

	ent->touch = JMSaberTouch;

	trap->LinkEntity((sharedEntity_t *)ent);

	ent->think = JMSaberThink;
	ent->nextthink = level.time + 50;
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	// GalaxyRP fix: [Entity System] a point whose player box is inside a wall or a door (a map's, or
	// one the map's own fixes made) counts as taken, so it is never chosen over a free one -- see
	// RP_SpawnPointInSolid(). Every selector falls back to the map's first point when none is free,
	// as it did.
	if ( RP_SpawnPointInSolid( spot->s.origin, qtrue ) ) {
		return qtrue;
	}

	VectorAdd( spot->s.origin, playerMins, mins );
	VectorAdd( spot->s.origin, playerMaxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if ( hit->client) {
			return qtrue;
		}

	}

	return qfalse;
}

qboolean SpotWouldTelefrag2( gentity_t *mover, vec3_t dest )
{
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( dest, mover->r.mins, mins );
	VectorAdd( dest, mover->r.maxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++)
	{
		hit = &g_entities[touch[i]];
		if ( hit == mover )
		{
			continue;
		}

		if ( hit->r.contents & mover->r.contents )
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectNearestDeathmatchSpawnPoint( vec3_t from ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist, nearestDist;
	gentity_t	*nearestSpot;

	nearestDist = 999999;
	nearestSpot = NULL;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {

		VectorSubtract( spot->s.origin, from, delta );
		dist = VectorLength( delta );
		if ( dist < nearestDist ) {
			nearestDist = dist;
			nearestSpot = spot;
		}
	}

	return nearestSpot;
}


/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectRandomDeathmatchSpawnPoint( qboolean isbot ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_SPAWN_POINTS];

	count = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL && count < MAX_SPAWN_POINTS) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}

		if(((spot->flags & FL_NO_BOTS) && isbot) ||
		   ((spot->flags & FL_NO_HUMANS) && !isbot))
		{
			// spot is not for this human/bot player
			continue;
		}

		spots[ count ] = spot;
		count++;
	}

	if ( !count ) {	// no spots that won't telefrag
		return G_Find( NULL, FOFS(classname), "info_player_deathmatch");
	}

	selection = rand() % count;
	return spots[ selection ];
}

/*
===========
SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
/*
================
RP_ProbeAroundSpawnPoint

GalaxyRP: [Spawning] every spawn point has somebody standing on it (a single-player map with
one start, a small map that is full). Rather than telefrag whoever is on the first one, try
nine spots around it -- the point itself and 60 units out along and across each axis -- and
take the first with a clear player box and a floor within reach. qfalse when all nine are
taken or blocked, and the caller telefrags as before.
================
*/
static qboolean RP_ProbeAroundSpawnPoint( gentity_t *spot, vec3_t origin )
{
	static const float	offsets[9][2] = { { 0, 0 }, { 60, 0 }, { -60, 0 }, { 0, 60 }, { 0, -60 }, { 60, 60 }, { -60, 60 }, { 60, -60 }, { -60, -60 } };
	vec3_t				playerMins = { -15, -15, DEFAULT_MINS_2 };
	vec3_t				playerMaxs = { 15, 15, DEFAULT_MAXS_2 };
	trace_t				tr;
	vec3_t				start, end;
	int					i;

	for ( i = 0; i < 9; i++ )
	{
		VectorCopy( spot->s.origin, start );
		start[0] += offsets[i][0];
		start[1] += offsets[i][1];
		start[2] += 9 + 32;	// a little above the point, dropped onto the floor
		VectorCopy( start, end );
		end[2] -= 200;

		trap->Trace( &tr, start, playerMins, playerMaxs, end, ENTITYNUM_NONE, MASK_PLAYERSOLID, qfalse, 0, 0 );
		if ( tr.startsolid || tr.allsolid || tr.fraction == 1.0f )
		{//inside something (a wall, a player) or no floor within reach
			continue;
		}

		VectorCopy( tr.endpos, origin );
		origin[2] += 1;
		return qtrue;
	}

	return qfalse;
}

/*
================
RP_RecordFallbackSpawnPoint / RP_FallbackSpawnPoint

GalaxyRP fix: [Spawning] the spawn selectors below ended with
trap->Error( ERR_DROP, "Couldn't find a spawn point" ) when the map had no info_player_deathmatch
left, and on a dedicated server ERR_DROP is a process exit. /entremove refuses the map's spawn points, but an
/entload preset that holds none (hand-written, old, or saved before a map update) still frees every
one of the map's and brings none back, and the next spawn, respawn or spectator join ended the server.

So the map's first spawn point is remembered when the map finishes loading, and a selector that finds
none uses that spot instead. It returns NULL as the spot, the same as SelectSpectatorSpawnPoint()
always has: ClientSpawn() only fires the spot's targets through G_UseTargets(), which ignores NULL,
and tests it before touching ->target. If the map had no spawn point even at load -- a broken map,
which vanilla would also have dropped -- it uses the world origin rather than end the server.
================
*/
void RP_RecordFallbackSpawnPoint( void )
{
	gentity_t *spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch" );

	level.rp_fallback_spawn_set = qfalse;
	VectorClear( level.rp_fallback_spawn_origin );
	VectorClear( level.rp_fallback_spawn_angles );

	if ( spot )
	{
		VectorCopy( spot->s.origin, level.rp_fallback_spawn_origin );
		VectorCopy( spot->s.angles, level.rp_fallback_spawn_angles );
		level.rp_fallback_spawn_set = qtrue;
	}
}

static gentity_t *RP_FallbackSpawnPoint( vec3_t origin, vec3_t angles )
{
	// zyk: once per map, so a map in this state does not write a line for every respawn
	if ( !level.rp_fallback_spawn_warned )
	{
		level.rp_fallback_spawn_warned = qtrue;
		G_LogPrintf( "No info_player_deathmatch left on this map: spawning players at %s instead.\n",
			level.rp_fallback_spawn_set ? "the map's first spawn point" : "the world origin" );
	}

	VectorCopy( level.rp_fallback_spawn_origin, origin );
	origin[2] += 9;
	VectorCopy( level.rp_fallback_spawn_angles, angles );

	return NULL;
}

gentity_t *SelectRandomFurthestSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team, qboolean isbot ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist;
	float		list_dist[MAX_SPAWN_POINTS];
	gentity_t	*list_spot[MAX_SPAWN_POINTS];
	int			numSpots, rnd, i, j;

	numSpots = 0;
	spot = NULL;

	//in Team DM, look for a team start spot first, if any
	if ( level.gametype == GT_TEAM
		&& team != TEAM_FREE
		&& team != TEAM_SPECTATOR )
	{
		char *classname = NULL;
		if ( team == TEAM_RED )
		{
			classname = "info_player_start_red";
		}
		else
		{
			classname = "info_player_start_blue";
		}
		while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL) {
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}

			if(((spot->flags & FL_NO_BOTS) && isbot) ||
			   ((spot->flags & FL_NO_HUMANS) && !isbot))
			{
				// spot is not for this human/bot player
				continue;
			}

			VectorSubtract( spot->s.origin, avoidPoint, delta );
			dist = VectorLength( delta );
			for (i = 0; i < numSpots; i++) {
				if ( dist > list_dist[i] ) {
					if ( numSpots >= MAX_SPAWN_POINTS )
						numSpots = MAX_SPAWN_POINTS-1;
					for (j = numSpots; j > i; j--) {
						list_dist[j] = list_dist[j-1];
						list_spot[j] = list_spot[j-1];
					}
					list_dist[i] = dist;
					list_spot[i] = spot;
					numSpots++;
					break;
				}
			}
			if (i >= numSpots && numSpots < MAX_SPAWN_POINTS) {
				list_dist[numSpots] = dist;
				list_spot[numSpots] = spot;
				numSpots++;
			}
		}
	}

	if ( !numSpots )
	{//couldn't find any of the above
		while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}

			if(((spot->flags & FL_NO_BOTS) && isbot) ||
			   ((spot->flags & FL_NO_HUMANS) && !isbot))
			{
				// spot is not for this human/bot player
				continue;
			}

			VectorSubtract( spot->s.origin, avoidPoint, delta );
			dist = VectorLength( delta );
			for (i = 0; i < numSpots; i++) {
				if ( dist > list_dist[i] ) {
					if ( numSpots >= MAX_SPAWN_POINTS )
						numSpots = MAX_SPAWN_POINTS-1;
					for (j = numSpots; j > i; j--) {
						list_dist[j] = list_dist[j-1];
						list_spot[j] = list_spot[j-1];
					}
					list_dist[i] = dist;
					list_spot[i] = spot;
					numSpots++;
					break;
				}
			}
			if (i >= numSpots && numSpots < MAX_SPAWN_POINTS) {
				list_dist[numSpots] = dist;
				list_spot[numSpots] = spot;
				numSpots++;
			}
		}
		if (!numSpots) {
			gentity_t *probe = NULL;

			spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch");
			if (!spot) // GalaxyRP fix: [Spawning] was trap->Error( ERR_DROP ) -- see RP_FallbackSpawnPoint()
				return RP_FallbackSpawnPoint( origin, angles );

			// GalaxyRP: [Spawning] every point is taken: look for room beside one before telefragging
			while ( (probe = G_Find( probe, FOFS(classname), "info_player_deathmatch" )) != NULL )
			{
				if ( ((probe->flags & FL_NO_BOTS) && isbot) || ((probe->flags & FL_NO_HUMANS) && !isbot) )
				{
					continue;
				}
				if ( RP_ProbeAroundSpawnPoint( probe, origin ) )
				{
					VectorCopy( probe->s.angles, angles );
					return probe;
				}
			}

			VectorCopy (spot->s.origin, origin);
			origin[2] += 9;
			VectorCopy (spot->s.angles, angles);
			return spot;
		}
	}

	// select a random spot from the spawn points furthest away
	rnd = Q_flrand(0.0f, 1.0f) * (numSpots / 2);

	VectorCopy (list_spot[rnd]->s.origin, origin);
	origin[2] += 9;
	VectorCopy (list_spot[rnd]->s.angles, angles);

	return list_spot[rnd];
}

gentity_t *SelectDuelSpawnPoint( int team, vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot )
{
	gentity_t	*spot;
	vec3_t		delta;
	float		dist;
	float		list_dist[MAX_SPAWN_POINTS];
	gentity_t	*list_spot[MAX_SPAWN_POINTS];
	int			numSpots, rnd, i, j;
	char		*spotName;

	if (team == DUELTEAM_LONE)
	{
		spotName = "info_player_duel1";
	}
	else if (team == DUELTEAM_DOUBLE)
	{
		spotName = "info_player_duel2";
	}
	else if (team == DUELTEAM_SINGLE)
	{
		spotName = "info_player_duel";
	}
	else
	{
		spotName = "info_player_deathmatch";
	}
tryAgain:

	numSpots = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), spotName)) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}

		if(((spot->flags & FL_NO_BOTS) && isbot) ||
			((spot->flags & FL_NO_HUMANS) && !isbot))
		{
			// spot is not for this human/bot player
			continue;
		}

		VectorSubtract( spot->s.origin, avoidPoint, delta );
		dist = VectorLength( delta );
		for (i = 0; i < numSpots; i++) {
			if ( dist > list_dist[i] ) {
				if ( numSpots >= MAX_SPAWN_POINTS )
					numSpots = MAX_SPAWN_POINTS-1;
				for (j = numSpots; j > i; j--) {
					list_dist[j] = list_dist[j-1];
					list_spot[j] = list_spot[j-1];
				}
				list_dist[i] = dist;
				list_spot[i] = spot;
				numSpots++;
				break;
			}
		}
		if (i >= numSpots && numSpots < MAX_SPAWN_POINTS) {
			list_dist[numSpots] = dist;
			list_spot[numSpots] = spot;
			numSpots++;
		}
	}
	if (!numSpots)
	{
		if (Q_stricmp(spotName, "info_player_deathmatch"))
		{ //try the loop again with info_player_deathmatch as the target if we couldn't find a duel spot
			spotName = "info_player_deathmatch";
			goto tryAgain;
		}

		//If we got here we found no free duel or DM spots, just try the first DM spot
		spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch");
		if (!spot) // GalaxyRP fix: [Spawning] was trap->Error( ERR_DROP ) -- see RP_FallbackSpawnPoint()
			return RP_FallbackSpawnPoint( origin, angles );
		VectorCopy (spot->s.origin, origin);
		origin[2] += 9;
		VectorCopy (spot->s.angles, angles);
		return spot;
	}

	// select a random spot from the spawn points furthest away
	rnd = Q_flrand(0.0f, 1.0f) * (numSpots / 2);

	VectorCopy (list_spot[rnd]->s.origin, origin);
	origin[2] += 9;
	VectorCopy (list_spot[rnd]->s.angles, angles);

	return list_spot[rnd];
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team, qboolean isbot ) {
	return SelectRandomFurthestSpawnPoint( avoidPoint, origin, angles, team, isbot );

	/*
	gentity_t	*spot;
	gentity_t	*nearestSpot;

	nearestSpot = SelectNearestDeathmatchSpawnPoint( avoidPoint );

	spot = SelectRandomDeathmatchSpawnPoint ( );
	if ( spot == nearestSpot ) {
		// roll again if it would be real close to point of death
		spot = SelectRandomDeathmatchSpawnPoint ( );
		if ( spot == nearestSpot ) {
			// last try
			spot = SelectRandomDeathmatchSpawnPoint ( );
		}
	}

	// find a single player start spot
	if (!spot) {
		trap->Error( ERR_DROP, "Couldn't find a spawn point" );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
	*/
}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
gentity_t *SelectInitialSpawnPoint( vec3_t origin, vec3_t angles, team_t team, qboolean isbot ) {
	gentity_t	*spot;

	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if(((spot->flags & FL_NO_BOTS) && isbot) ||
		   ((spot->flags & FL_NO_HUMANS) && !isbot))
		{
			continue;
		}

		if ( spot->spawnflags & 1 ) {
			break;
		}
	}

	if ( !spot || SpotWouldTelefrag( spot ) ) {
		return SelectSpawnPoint( vec3_origin, origin, angles, team, isbot );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t *SelectSpectatorSpawnPoint( vec3_t origin, vec3_t angles ) {
	FindIntermissionPoint();

	VectorCopy( level.intermission_origin, origin );
	VectorCopy( level.intermission_angle, angles );

	return NULL;
}

/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
=======================================================================

BODYQUE

=======================================================================
*/

// DAJ_RP: [Corpses] BODY_SINK_TIME (30000 ms) is now rp_player_corpse_time, in seconds, default 30. It is
// read once, when the corpse is laid down in CopyToBodyQue(), and kept on the body in genericValue1 so a
// change to the cvar never shifts a corpse that is already lying there -- BodySink() compares against
// the same number the corpse was scheduled with (no other code touches genericValue1 on a body).

/*
===============
InitBodyQue
===============
*/
void InitBodyQue (void) {
	int		i;
	gentity_t	*ent;

	level.bodyQueIndex = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++) {
		ent = G_Spawn();
		ent->classname = "bodyque";
		ent->neverFree = qtrue;
		level.bodyQue[i] = ent;
	}
}

/*
=============
BodySink

After sitting around for five seconds, fall into the ground and disappear
=============
*/
void BodySink( gentity_t *ent ) {
	if ( level.time - ent->timestamp > ent->genericValue1 + 2500 ) {
		// the body ques are never actually freed, they are just unlinked
		trap->UnlinkEntity( (sharedEntity_t *)ent );
		ent->physicsObject = qfalse;
		return;
	}
//	ent->nextthink = level.time + 100;
//	ent->s.pos.trBase[2] -= 1;

	G_AddEvent(ent, EV_BODYFADE, 0);
	ent->nextthink = level.time + 18000;
	ent->takedamage = qfalse;
}

/*
=============
CopyToBodyQue

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
static qboolean CopyToBodyQue( gentity_t *ent ) {
	gentity_t		*body;
	int			contents;
	int			islight = 0;

	if (level.intermissiontime)
	{
		return qfalse;
	}

	trap->UnlinkEntity ((sharedEntity_t *)ent);

	// if client is in a nodrop area, don't leave the body
	contents = trap->PointContents( ent->s.origin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		return qfalse;
	}

	if (ent->client && (ent->client->ps.eFlags & EF_DISINTEGRATION))
	{ //for now, just don't spawn a body if you got disint'd
		return qfalse;
	}

	// grab a body que and cycle to the next one
	body = level.bodyQue[ level.bodyQueIndex ];
	level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

	trap->UnlinkEntity ((sharedEntity_t *)body);
	body->s = ent->s;

	//avoid oddly angled corpses floating around
	body->s.angles[PITCH] = body->s.angles[ROLL] = body->s.apos.trBase[PITCH] = body->s.apos.trBase[ROLL] = 0;

	body->s.g2radius = 100;

	body->s.eType = ET_BODY;
	body->s.eFlags = EF_DEAD;		// clear EF_TALK, etc

	// GalaxyRP: [Phase] ...but keep the phase bits, so the corpse of a Force ghost or a hologram
	// still looks like one (drawn by CG_General(), cg_ents.c). Purely visual on a body: its contents
	// are CONTENTS_CORPSE and nothing on the server reads these bits from an ET_BODY.
	body->s.eFlags |= ( ent->s.eFlags & EF_RP_PHASE_MASK );

	if (ent->client && (ent->client->ps.eFlags & EF_DISINTEGRATION))
	{
		body->s.eFlags |= EF_DISINTEGRATION;
	}

	VectorCopy(ent->client->ps.lastHitLoc, body->s.origin2);

	body->s.powerups = 0;	// clear powerups
	body->s.loopSound = 0;	// clear lava burning
	body->s.loopIsSoundset = qfalse;
	body->s.number = body - g_entities;
	body->timestamp = level.time;
	body->physicsObject = qtrue;
	body->physicsBounce = 0;		// don't bounce
	if ( body->s.groundEntityNum == ENTITYNUM_NONE ) {
		body->s.pos.trType = TR_GRAVITY;
		body->s.pos.trTime = level.time;
		VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );
	} else {
		body->s.pos.trType = TR_STATIONARY;
	}
	body->s.event = 0;

	body->s.weapon = ent->s.bolt2;

	if (body->s.weapon == WP_SABER && ent->client->ps.saberInFlight)
	{
		body->s.weapon = WP_BLASTER; //lie to keep from putting a saber on the corpse, because it was thrown at death
	}

	//G_AddEvent(body, EV_BODY_QUEUE_COPY, ent->s.clientNum);
	//Now doing this through a modified version of the rcg reliable command.
	if (ent->client && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
	{
		islight = 1;
	}
	trap->SendServerCommand(-1, va("ircg %i %i %i %i", ent->s.number, body->s.number, body->s.weapon, islight));

	body->r.svFlags = ent->r.svFlags | SVF_BROADCAST;

	// DAJ_RP: [Phase] the corpse of a player under /admhidden stays hidden: it is not sent to anyone at all.
	// The Mind Trick bits that hide the living player (RP_HiddenApplyState(), g_active.c) cannot do it here,
	// because a corpse is drawn by CG_General(), which never reads them. rpHiddenCorpse also keeps its
	// severed limbs from being sent (G_Dismember(), g_combat.c). svFlags and the flag are both rewritten on
	// every copy, so a body-queue slot reused for somebody else's corpse is sent as usual.
	body->rpHiddenCorpse = RP_PlayerIsHidden( ent );
	if ( body->rpHiddenCorpse )
	{
		body->r.svFlags |= SVF_NOCLIENT;
	}
	VectorCopy (ent->r.mins, body->r.mins);
	VectorCopy (ent->r.maxs, body->r.maxs);
	VectorCopy (ent->r.absmin, body->r.absmin);
	VectorCopy (ent->r.absmax, body->r.absmax);

	body->s.torsoAnim = body->s.legsAnim = ent->client->ps.legsAnim;

	body->s.customRGBA[0] = ent->client->ps.customRGBA[0];
	body->s.customRGBA[1] = ent->client->ps.customRGBA[1];
	body->s.customRGBA[2] = ent->client->ps.customRGBA[2];
	body->s.customRGBA[3] = ent->client->ps.customRGBA[3];

	body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	body->r.contents = CONTENTS_CORPSE;
	body->r.ownerNum = ent->s.number;

	body->genericValue1 = RP_CorpseSecondsToMs( rp_player_corpse_time.integer );
	body->nextthink = level.time + body->genericValue1;
	body->think = BodySink;

	body->die = body_die;

	// don't take more damage if already gibbed
	if ( ent->health <= GIB_HEALTH ) {
		body->takedamage = qfalse;
	} else {
		body->takedamage = qtrue;
	}

	VectorCopy ( body->s.pos.trBase, body->r.currentOrigin );
	trap->LinkEntity ((sharedEntity_t *)body);

	return qtrue;
}

//======================================================================


/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle( gentity_t *ent, vec3_t angle ) {
	int			i;

	// set the delta angle
	for (i=0 ; i<3 ; i++) {
		int		cmdAngle;

		cmdAngle = ANGLE2SHORT(angle[i]);
		ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
	}
	VectorCopy( angle, ent->s.angles );
	VectorCopy (ent->s.angles, ent->client->ps.viewangles);
}

void MaintainBodyQueue(gentity_t *ent)
{ //do whatever should be done taking ragdoll and dismemberment states into account.
	qboolean doRCG = qfalse;

	assert(ent && ent->client);
	if (ent->client->tempSpectate >= level.time ||
		(ent->client->ps.eFlags2 & EF2_SHIP_DEATH))
	{
		ent->client->noCorpse = qtrue;
	}

	if (!ent->client->noCorpse && !ent->client->ps.fallingToDeath)
	{
		if (!CopyToBodyQue (ent))
		{
			doRCG = qtrue;
		}
	}
	else
	{
		ent->client->noCorpse = qfalse; //clear it for next time
		ent->client->ps.fallingToDeath = qfalse;
		doRCG = qtrue;
	}

	if (doRCG)
	{ //bodyque func didn't manage to call ircg so call this to assure our limbs and ragdoll states are proper on the client.
		trap->SendServerCommand(-1, va("rcg %i", ent->s.clientNum));
	}
}

/*
================
ClientRespawn
================
*/
void SiegeRespawn(gentity_t *ent);
void ClientRespawn( gentity_t *ent ) {
	MaintainBodyQueue(ent);

	if (gEscaping || level.gametype == GT_POWERDUEL)
	{
		ent->client->sess.sessionTeam = TEAM_SPECTATOR;
		ent->client->sess.spectatorState = SPECTATOR_FREE;
		ent->client->sess.spectatorClient = 0;

		ent->client->pers.teamState.state = TEAM_BEGIN;
		AddTournamentQueue(ent->client);
		ClientSpawn(ent);
		ent->client->iAmALoser = qtrue;
		return;
	}

	trap->UnlinkEntity ((sharedEntity_t *)ent);

	if (level.gametype == GT_SIEGE)
	{
		if (g_siegeRespawn.integer)
		{
			if (ent->client->tempSpectate < level.time)
			{
				int minDel = g_siegeRespawn.integer* 2000;
				if (minDel < 20000)
				{
					minDel = 20000;
				}
				ent->client->tempSpectate = level.time + minDel;
				ent->health = ent->client->ps.stats[STAT_HEALTH] = 1;
				ent->waterlevel = ent->watertype = 0;
				ent->client->ps.weapon = WP_NONE;
				ent->client->ps.stats[STAT_WEAPONS] = 0;
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
				ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
				ent->takedamage = qfalse;
				trap->LinkEntity((sharedEntity_t *)ent);

				// Respawn time.
				if ( ent->s.number < MAX_CLIENTS )
				{
					gentity_t *te = G_TempEntity( ent->client->ps.origin, EV_SIEGESPEC );
					te->s.time = g_siegeRespawnCheck;
					te->s.owner = ent->s.number;
				}

				return;
			}
		}
		SiegeRespawn(ent);
	}
	else
	{
		ClientSpawn(ent);
	}
}

/*
================
TeamCount

Returns number of players on a team
================
*/
int TeamCount( int ignoreClientNum, team_t team ) {
	int		i;
	int		count = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( i == ignoreClientNum ) {
			continue;
		}
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			count++;
		}
		else if (level.gametype == GT_SIEGE &&
            level.clients[i].sess.siegeDesiredTeam == team)
		{
			count++;
		}
	}

	return count;
}

/*
================
TeamLeader

Returns the client number of the team leader
================
*/
int TeamLeader( int team ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			if ( level.clients[i].sess.teamLeader )
				return i;
		}
	}

	return -1;
}


/*
================
PickTeam

================
*/
team_t PickTeam( int ignoreClientNum ) {
	int		counts[TEAM_NUM_TEAMS];

	counts[TEAM_BLUE] = TeamCount( ignoreClientNum, TEAM_BLUE );
	counts[TEAM_RED] = TeamCount( ignoreClientNum, TEAM_RED );

	if ( counts[TEAM_BLUE] > counts[TEAM_RED] ) {
		return TEAM_RED;
	}
	if ( counts[TEAM_RED] > counts[TEAM_BLUE] ) {
		return TEAM_BLUE;
	}
	// equal team count, so join the team with the lowest score
	if ( level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ) {
		return TEAM_RED;
	}
	return TEAM_BLUE;
}

/*
===========
ForceClientSkin

Forces a client's skin (for teamplay)
===========
*/
/*
static void ForceClientSkin( gclient_t *client, char *model, const char *skin ) {
	char *p;

	if ((p = Q_strrchr(model, '/')) != 0) {
		*p = 0;
	}

	Q_strcat(model, MAX_QPATH, "/");
	Q_strcat(model, MAX_QPATH, skin);
}
*/

/*
===========
ClientCheckName
============
*/
static void ClientCleanName( const char *in, char *out, int outSize )
{
	int outpos = 0, colorlessLen = 0, spaces = 0, ats = 0;

	// discard leading spaces
	for ( ; *in == ' '; in++);

	// discard leading asterisk's (fail raven for using * as a skipnotify)
	// apparently .* causes the issue too so... derp
	//for(; *in == '*'; in++);

	for(; *in && outpos < outSize - 1; in++)
	{
		out[outpos] = *in;

		if ( *in == ' ' )
		{// don't allow too many consecutive spaces
			if ( spaces > 2 )
				continue;

			spaces++;
		}
		else if ( *in == '@' )
		{// don't allow too many consecutive at signs
			if ( ++ats > 2 ) {
				outpos -= 2;
				ats = 0;
				continue;
			}
		}
		else if ( (byte)*in < 0x20
				|| (byte)*in == 0x81 || (byte)*in == 0x8D || (byte)*in == 0x8F || (byte)*in == 0x90 || (byte)*in == 0x9D
				|| (byte)*in == 0xA0 || (byte)*in == 0xAD )
		{
			continue;
		}
		else if ( outpos > 0 && out[outpos-1] == Q_COLOR_ESCAPE )
		{
			if ( Q_IsColorStringExt( &out[outpos-1] ) )
			{
				colorlessLen--;

#if 0
				if ( ColorIndex( *in ) == 0 )
				{// Disallow color black in names to prevent players from getting advantage playing in front of black backgrounds
					outpos--;
					continue;
				}
#endif
			}
			else
			{
				spaces = ats = 0;
				colorlessLen++;
			}
		}
		else
		{
			spaces = ats = 0;
			colorlessLen++;
		}

		outpos++;
	}

	out[outpos] = '\0';

	// GalaxyRP: [Name] strip trailing spaces and trailing colour codes ("Bob ^1 ^7" -> "Bob"), so a
	// name can't end in invisible padding. JA++ strips trailing spaces with an unguarded
	// out[len-1] check that reads before the buffer once the name is empty; both checks here are
	// bounded by outpos. Neither a space nor a whole colour code counts towards colorlessLen, so
	// the empty-name check below still sees the right thing.
	while ( outpos > 0 ) {
		if ( out[outpos-1] == ' ' ) {
			out[--outpos] = '\0';
		}
		else if ( outpos >= 2 && Q_IsColorStringExt( &out[outpos-2] ) ) {
			outpos -= 2;
			out[outpos] = '\0';
		}
		else {
			break;
		}
	}

	// don't allow empty names
	if ( *out == '\0' || colorlessLen == 0 )
		Q_strncpyz( out, "Padawan", outSize );
}

#ifdef _DEBUG
void G_DebugWrite(const char *path, const char *text)
{
	fileHandle_t f;

	trap->FS_Open( path, &f, FS_APPEND );
	trap->FS_Write(text, strlen(text), f);
	trap->FS_Close(f);
}
#endif

qboolean G_SaberModelSetup(gentity_t *ent)
{
	int i = 0;
	qboolean fallbackForSaber = qtrue;

	while (i < MAX_SABERS)
	{
		if (ent->client->saber[i].model[0])
		{
			//first kill it off if we've already got it
			if (ent->client->weaponGhoul2[i])
			{
				trap->G2API_CleanGhoul2Models(&(ent->client->weaponGhoul2[i]));
			}
			trap->G2API_InitGhoul2Model(&ent->client->weaponGhoul2[i], ent->client->saber[i].model, 0, 0, -20, 0, 0);

			if (ent->client->weaponGhoul2[i])
			{
				int j = 0;
				char *tagName;
				int tagBolt;

				if (ent->client->saber[i].skin)
				{
					trap->G2API_SetSkin(ent->client->weaponGhoul2[i], 0, ent->client->saber[i].skin, ent->client->saber[i].skin);
				}

				if (ent->client->saber[i].saberFlags & SFL_BOLT_TO_WRIST)
				{
					trap->G2API_SetBoltInfo(ent->client->weaponGhoul2[i], 0, 3+i);
				}
				else
				{ // bolt to right hand for 0, or left hand for 1
					trap->G2API_SetBoltInfo(ent->client->weaponGhoul2[i], 0, i);
				}

				//Add all the bolt points
				while (j < ent->client->saber[i].numBlades)
				{
					tagName = va("*blade%i", j+1);
					tagBolt = trap->G2API_AddBolt(ent->client->weaponGhoul2[i], 0, tagName);

					if (tagBolt == -1)
					{
						if (j == 0)
						{ //guess this is an 0ldsk3wl saber
							tagBolt = trap->G2API_AddBolt(ent->client->weaponGhoul2[i], 0, "*flash");
							fallbackForSaber = qfalse;
							break;
						}

						if (tagBolt == -1)
						{
							assert(0);
							break;

						}
					}
					j++;

					fallbackForSaber = qfalse; //got at least one custom saber so don't need default
				}

				//Copy it into the main instance
				trap->G2API_CopySpecificGhoul2Model(ent->client->weaponGhoul2[i], 0, ent->ghoul2, i+1);
			}
		}
		else
		{
			break;
		}

		i++;
	}

	return fallbackForSaber;
}

/*
===========
SetupGameGhoul2Model

There are two ghoul2 model instances per player (actually three).  One is on the clientinfo (the base for the client side
player, and copied for player spawns and for corpses).  One is attached to the centity itself, which is the model acutally
animated and rendered by the system.  The final is the game ghoul2 model.  This is animated by pmove on the server, and
is used for determining where the lightsaber should be, and for per-poly collision tests.
===========
*/
void *g2SaberInstance = NULL;

qboolean BG_IsValidCharacterModel(const char *modelName, const char *skinName);
qboolean BG_ValidateSkinForTeam( const char *modelName, char *skinName, int team, float *colors );
void BG_GetVehicleModelName(char *modelName, const char *vehicleName, size_t len);

void SetupGameGhoul2Model(gentity_t *ent, char *modelname, char *skinName)
{
	int handle;
	char		afilename[MAX_QPATH];
#if 0
	char		/**GLAName,*/ *slash;
#endif
	char		GLAName[MAX_QPATH];
	vec3_t	tempVec = {0,0,0};

	if (strlen(modelname) >= MAX_QPATH )
	{
		Com_Error( ERR_FATAL, "SetupGameGhoul2Model(%s): modelname exceeds MAX_QPATH.\n", modelname );
	}
	if (skinName && strlen(skinName) >= MAX_QPATH )
	{
		Com_Error( ERR_FATAL, "SetupGameGhoul2Model(%s): skinName exceeds MAX_QPATH.\n", skinName );
	}

	// First things first.  If this is a ghoul2 model, then let's make sure we demolish this first.
	if (ent->ghoul2 && trap->G2API_HaveWeGhoul2Models(ent->ghoul2))
	{
		trap->G2API_CleanGhoul2Models(&(ent->ghoul2));
	}

	//rww - just load the "standard" model for the server"
	if (!precachedKyle)
	{
		int defSkin;

		Com_sprintf( afilename, sizeof( afilename ), "models/players/" DEFAULT_MODEL "/model.glm" );
		handle = trap->G2API_InitGhoul2Model(&precachedKyle, afilename, 0, 0, -20, 0, 0);

		if (handle<0)
		{
			return;
		}

		defSkin = trap->R_RegisterSkin("models/players/" DEFAULT_MODEL "/model_default.skin");
		trap->G2API_SetSkin(precachedKyle, 0, defSkin, defSkin);
	}

	if (precachedKyle && trap->G2API_HaveWeGhoul2Models(precachedKyle))
	{
		if (d_perPlayerGhoul2.integer || ent->s.number >= MAX_CLIENTS ||
			G_PlayerHasCustomSkeleton(ent))
		{ //rww - allow option for perplayer models on server for collision and bolt stuff.
			char modelFullPath[MAX_QPATH];
			char truncModelName[MAX_QPATH];
			char skin[MAX_QPATH];
			char vehicleName[MAX_QPATH];
			int skinHandle = 0;
			int i = 0;
			char *p;

			// If this is a vehicle, get it's model name.
			if ( ent->client->NPC_class == CLASS_VEHICLE )
			{
				char realModelName[MAX_QPATH];

				Q_strncpyz( vehicleName, modelname, sizeof( vehicleName ) );
				BG_GetVehicleModelName(realModelName, modelname, sizeof( realModelName ));
				strcpy(truncModelName, realModelName);
				skin[0] = 0;
				if ( ent->m_pVehicle
					&& ent->m_pVehicle->m_pVehicleInfo
					&& ent->m_pVehicle->m_pVehicleInfo->skin
					&& ent->m_pVehicle->m_pVehicleInfo->skin[0] )
				{
					skinHandle = trap->R_RegisterSkin(va("models/players/%s/model_%s.skin", realModelName, ent->m_pVehicle->m_pVehicleInfo->skin));
				}
				else
				{
					skinHandle = trap->R_RegisterSkin(va("models/players/%s/model_default.skin", realModelName));
				}
			}
			else
			{
				if (skinName && skinName[0])
				{
					strcpy(skin, skinName);
					strcpy(truncModelName, modelname);
				}
				else
				{
					strcpy(skin, "default");

					strcpy(truncModelName, modelname);
					p = Q_strrchr(truncModelName, '/');

					if (p)
					{
						*p = 0;
						p++;

						while (p && *p)
						{
							skin[i] = *p;
							i++;
							p++;
						}
						skin[i] = 0;
						i = 0;
					}

					if (!BG_IsValidCharacterModel(truncModelName, skin))
					{
						strcpy(truncModelName, DEFAULT_MODEL);
						strcpy(skin, "default");
					}

					if ( level.gametype >= GT_TEAM && level.gametype != GT_SIEGE && !g_jediVmerc.integer )
					{
						float colorOverride[3];

						colorOverride[0] = colorOverride[1] = colorOverride[2] = 0.0f;

						BG_ValidateSkinForTeam( truncModelName, skin, ent->client->sess.sessionTeam, colorOverride);
						if (colorOverride[0] != 0.0f ||
							colorOverride[1] != 0.0f ||
							colorOverride[2] != 0.0f)
						{
							ent->client->ps.customRGBA[0] = colorOverride[0]*255.0f;
							ent->client->ps.customRGBA[1] = colorOverride[1]*255.0f;
							ent->client->ps.customRGBA[2] = colorOverride[2]*255.0f;
						}

						//BG_ValidateSkinForTeam( truncModelName, skin, ent->client->sess.sessionTeam, NULL );
					}
					else if (level.gametype == GT_SIEGE)
					{ //force skin for class if appropriate
						if (ent->client->siegeClass != -1)
						{
							siegeClass_t *scl = &bgSiegeClasses[ent->client->siegeClass];
							if (scl->forcedSkin[0])
							{
								Q_strncpyz( skin, scl->forcedSkin, sizeof( skin ) );
							}
						}
					}
				}
			}

			if (skin[0])
			{
				char *useSkinName;

				if (strchr(skin, '|'))
				{//three part skin
					useSkinName = va("models/players/%s/|%s", truncModelName, skin);
				}
				else
				{
					useSkinName = va("models/players/%s/model_%s.skin", truncModelName, skin);
				}

				skinHandle = trap->R_RegisterSkin(useSkinName);
			}

			strcpy(modelFullPath, va("models/players/%s/model.glm", truncModelName));
			handle = trap->G2API_InitGhoul2Model(&ent->ghoul2, modelFullPath, 0, skinHandle, -20, 0, 0);

			if (handle<0)
			{ //Huh. Guess we don't have this model. Use the default.

				if (ent->ghoul2 && trap->G2API_HaveWeGhoul2Models(ent->ghoul2))
				{
					trap->G2API_CleanGhoul2Models(&(ent->ghoul2));
				}
				ent->ghoul2 = NULL;
				trap->G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
			}
			else
			{
				trap->G2API_SetSkin(ent->ghoul2, 0, skinHandle, skinHandle);

				GLAName[0] = 0;
				trap->G2API_GetGLAName( ent->ghoul2, 0, GLAName);

				if (!GLAName[0] || (!strstr(GLAName, "players/_humanoid/") && ent->s.number < MAX_CLIENTS && !G_PlayerHasCustomSkeleton(ent)))
				{ //a bad model
					trap->G2API_CleanGhoul2Models(&(ent->ghoul2));
					ent->ghoul2 = NULL;
					trap->G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
				}

				if (ent->s.number >= MAX_CLIENTS)
				{
					ent->s.modelGhoul2 = 1; //so we know to free it on the client when we're removed.

					if (skin[0])
					{ //append it after a *
						strcat( modelFullPath, va("*%s", skin) );
					}

					if ( ent->client->NPC_class == CLASS_VEHICLE )
					{ //vehicles are tricky and send over their vehicle names as the model (the model is then retrieved based on the vehicle name)
						ent->s.modelindex = G_ModelIndex(vehicleName);
					}
					else
					{
						ent->s.modelindex = G_ModelIndex(modelFullPath);
					}
				}
			}
		}
		else
		{
			trap->G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
		}
	}
	else
	{
		return;
	}

	//Attach the instance to this entity num so we can make use of client-server
	//shared operations if possible.
	trap->G2API_AttachInstanceToEntNum(ent->ghoul2, ent->s.number, qtrue);

	// The model is now loaded.

	GLAName[0] = 0;

	if (!BGPAFtextLoaded)
	{
		if (BG_ParseAnimationFile("models/players/_humanoid/animation.cfg", bgHumanoidAnimations, qtrue) == -1)
		{
			Com_Printf( "Failed to load humanoid animation file\n");
			return;
		}
	}

	if (ent->s.number >= MAX_CLIENTS || G_PlayerHasCustomSkeleton(ent))
	{
		ent->localAnimIndex = -1;

		GLAName[0] = 0;
		trap->G2API_GetGLAName(ent->ghoul2, 0, GLAName);

		if (GLAName[0] &&
			!strstr(GLAName, "players/_humanoid/") /*&&
			!strstr(GLAName, "players/rockettrooper/")*/)
		{ //it doesn't use humanoid anims.
			char *slash = Q_strrchr( GLAName, '/' );
			if ( slash )
			{
				strcpy(slash, "/animation.cfg");

				ent->localAnimIndex = BG_ParseAnimationFile(GLAName, NULL, qfalse);
			}
		}
		else
		{ //humanoid index.
			if (strstr(GLAName, "players/rockettrooper/"))
			{
				ent->localAnimIndex = 1;
			}
			else
			{
				ent->localAnimIndex = 0;
			}
		}

		if (ent->localAnimIndex == -1)
		{
			Com_Error(ERR_DROP, "NPC had an invalid GLA\n");
		}
	}
	else
	{
		GLAName[0] = 0;
		trap->G2API_GetGLAName(ent->ghoul2, 0, GLAName);

		if (strstr(GLAName, "players/rockettrooper/"))
		{
			//assert(!"Should not have gotten in here with rockettrooper skel");
			ent->localAnimIndex = 1;
		}
		else
		{
			ent->localAnimIndex = 0;
		}
	}

	if (ent->s.NPC_class == CLASS_VEHICLE &&
		ent->m_pVehicle)
	{ //do special vehicle stuff
		char strTemp[128];
		int i;

		// Setup the default first bolt
		i = trap->G2API_AddBolt( ent->ghoul2, 0, "model_root" );

		// Setup the droid unit.
		ent->m_pVehicle->m_iDroidUnitTag = trap->G2API_AddBolt( ent->ghoul2, 0, "*droidunit" );

		// Setup the Exhausts.
		for ( i = 0; i < MAX_VEHICLE_EXHAUSTS; i++ )
		{
			Com_sprintf( strTemp, 128, "*exhaust%i", i + 1 );
			ent->m_pVehicle->m_iExhaustTag[i] = trap->G2API_AddBolt( ent->ghoul2, 0, strTemp );
		}

		// Setup the Muzzles.
		for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
		{
			Com_sprintf( strTemp, 128, "*muzzle%i", i + 1 );
			ent->m_pVehicle->m_iMuzzleTag[i] = trap->G2API_AddBolt( ent->ghoul2, 0, strTemp );
			if ( ent->m_pVehicle->m_iMuzzleTag[i] == -1 )
			{//ergh, try *flash?
				Com_sprintf( strTemp, 128, "*flash%i", i + 1 );
				ent->m_pVehicle->m_iMuzzleTag[i] = trap->G2API_AddBolt( ent->ghoul2, 0, strTemp );
			}
		}

		// Setup the Turrets.
		for ( i = 0; i < MAX_VEHICLE_TURRET_MUZZLES; i++ )
		{
			if ( ent->m_pVehicle->m_pVehicleInfo->turret[i].gunnerViewTag )
			{
				ent->m_pVehicle->m_iGunnerViewTag[i] = trap->G2API_AddBolt( ent->ghoul2, 0, ent->m_pVehicle->m_pVehicleInfo->turret[i].gunnerViewTag );
			}
			else
			{
				ent->m_pVehicle->m_iGunnerViewTag[i] = -1;
			}
		}
	}

	if (ent->client->ps.weapon == WP_SABER || ent->s.number < MAX_CLIENTS)
	{ //a player or NPC saber user
		trap->G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
		trap->G2API_AddBolt(ent->ghoul2, 0, "*l_hand");

		//rhand must always be first bolt. lhand always second. Whichever you want the
		//jetpack bolted to must always be third.
		trap->G2API_AddBolt(ent->ghoul2, 0, "*chestg");

		//claw bolts
		trap->G2API_AddBolt(ent->ghoul2, 0, "*r_hand_cap_r_arm");
		trap->G2API_AddBolt(ent->ghoul2, 0, "*l_hand_cap_l_arm");

		trap->G2API_SetBoneAnim(ent->ghoul2, 0, "model_root", 0, 12, BONE_ANIM_OVERRIDE_LOOP, 1.0f, level.time, -1, -1);
		trap->G2API_SetBoneAngles(ent->ghoul2, 0, "upper_lumbar", tempVec, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, level.time);
		trap->G2API_SetBoneAngles(ent->ghoul2, 0, "cranium", tempVec, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, POSITIVE_X, NULL, 0, level.time);

		if (!g2SaberInstance)
		{
			trap->G2API_InitGhoul2Model(&g2SaberInstance, DEFAULT_SABER_MODEL, 0, 0, -20, 0, 0);

			if (g2SaberInstance)
			{
				// indicate we will be bolted to model 0 (ie the player) on bolt 0 (always the right hand) when we get copied
				trap->G2API_SetBoltInfo(g2SaberInstance, 0, 0);
				// now set up the gun bolt on it
				trap->G2API_AddBolt(g2SaberInstance, 0, "*blade1");
			}
		}

		if (G_SaberModelSetup(ent))
		{
			if (g2SaberInstance)
			{
				trap->G2API_CopySpecificGhoul2Model(g2SaberInstance, 0, ent->ghoul2, 1);
			}
		}
	}

	if (ent->s.number >= MAX_CLIENTS)
	{ //some extra NPC stuff
		if (trap->G2API_AddBolt(ent->ghoul2, 0, "lower_lumbar") == -1)
		{ //check now to see if we have this bone for setting anims and such
			ent->noLumbar = qtrue;
		}
	}
}




/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap->SetUserinfo
if desired.
============
*/

qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride);
void G_ValidateSiegeClassForTeam(gentity_t *ent, int team);

typedef struct userinfoValidate_s {
	const char		*field, *fieldClean;
	unsigned int	minCount, maxCount;
	int				bit;	// GalaxyRP: [Userinfo] bit in g_userinfoValidate (layout in g_local.h)
} userinfoValidate_t;

#define UIF( x, _min, _max, _bit ) { STRING(\\) #x STRING(\\), STRING( x ), _min, _max, _bit }
static userinfoValidate_t userinfoFields[] = {
	UIF( cl_guid,			0, 0, 0 ), // not allowed, q3fill protection
	UIF( cl_punkbuster,		0, 0, 1 ), // not allowed, q3fill protection
	UIF( ip,				0, 1, 2 ), // engine adds this at the end
	UIF( name,				1, 1, 3 ),
	UIF( rate,				1, 1, 4 ),
	UIF( snaps,				1, 1, 5 ),
	UIF( model,				1, 1, 6 ),
	UIF( forcepowers,		1, 1, 7 ),
	UIF( color1,			1, 1, 8 ),
	UIF( color2,			1, 1, 9 ),
	UIF( handicap,			1, 1, 10 ),
	UIF( sex,				0, 1, 11 ),
	UIF( cg_predictItems,	1, 1, 12 ),
	UIF( saber1,			1, 1, 13 ),
	UIF( saber2,			1, 1, 14 ),
	UIF( char_color_red,	1, 1, 15 ),
	UIF( char_color_green,	1, 1, 16 ),
	UIF( char_color_blue,	1, 1, 17 ),
	UIF( teamtask,			0, 1, 18 ), // optional
	UIF( password,			0, 1, 19 ), // optional
	UIF( teamoverlay,		0, 1, 20 ), // only registered in cgame, not sent when connecting
	// bits 21-24 are the extra checks (userinfoValidateExtra[] below)
	// GalaxyRP: [Userinfo] keys this mod reads, each optional but never allowed twice. A duplicate
	// matters most for cp_sbRGB1/cp_sbRGB2: when the server rewrites them (Info_SetValueForKey in
	// g_cmds.c) only the FIRST copy is removed, so a second copy would move to the front and be
	// re-adopted by the next ClientUserinfoChanged().
	UIF( cp_sbRGB1,			0, 1, USERINFO_VALIDATION_NEW_FIELDS + 0 ), // saber RGB (g_client.c)
	UIF( cp_sbRGB2,			0, 1, USERINFO_VALIDATION_NEW_FIELDS + 1 ), // saber RGB (g_client.c)
	UIF( rpmod_client,		0, 1, USERINFO_VALIDATION_NEW_FIELDS + 2 ), // client plugin check
	UIF( ja_guid,			0, 1, USERINFO_VALIDATION_NEW_FIELDS + 3 ), // logged on connect
};
static const size_t numUserinfoFields = ARRAY_LEN( userinfoFields );

static const char *userinfoValidateExtra[USERINFO_VALIDATION_MAX] = {
	"Size",					// USERINFO_VALIDATION_SIZE
	"# of slashes",			// USERINFO_VALIDATION_SLASH
	"Extended ascii",		// USERINFO_VALIDATION_EXTASCII
	"Control characters",	// USERINFO_VALIDATION_CONTROLCHARS
};

// GalaxyRP: [Userinfo] the extra checks sit at fixed bits 21-24, not after the field count
#define UIV_EXTRA_BIT( x )	(1 << (USERINFO_VALIDATION_LEGACY_FIELDS + (x)))

// GalaxyRP: [Userinfo] name of a g_userinfoValidate bit, or NULL if the bit is unused
static const char *G_UserinfoValidateBitName( int bit ) {
	size_t i;

	if ( bit >= USERINFO_VALIDATION_LEGACY_FIELDS && bit < USERINFO_VALIDATION_NEW_FIELDS )
		return userinfoValidateExtra[bit - USERINFO_VALIDATION_LEGACY_FIELDS];

	for ( i=0; i<numUserinfoFields; i++ ) {
		if ( userinfoFields[i].bit == bit )
			return userinfoFields[i].fieldClean;
	}
	return NULL;
}

void Svcmd_ToggleUserinfoValidation_f( void ) {
	if ( trap->Argc() == 1 ) {
		int i;
		for ( i=0; i<USERINFO_VALIDATION_NUM_BITS; i++ ) {
			const char *bitName = G_UserinfoValidateBitName( i );

			if ( !bitName )
				continue;
			if ( (g_userinfoValidate.integer & (1<<i)) )	trap->Print( "%2d [X] %s\n", i, bitName );
			else											trap->Print( "%2d [ ] %s\n", i, bitName );
		}
		return;
	}
	else {
		char arg[8]={0};
		int index;
		const char *bitName;

		trap->Argv( 1, arg, sizeof( arg ) );
		index = atoi( arg );

		if ( index < 0 || index > USERINFO_VALIDATION_NUM_BITS-1 || !(bitName = G_UserinfoValidateBitName( index )) ) {
			Com_Printf( "ToggleUserinfoValidation: Invalid range: %i [0, %i]\n", index, USERINFO_VALIDATION_NUM_BITS-1 );
			return;
		}

		trap->Cvar_Set( "g_userinfoValidate", va( "%i", (1 << index) ^ (g_userinfoValidate.integer & ((1 << USERINFO_VALIDATION_NUM_BITS) - 1)) ) );
		trap->Cvar_Update( &g_userinfoValidate );

		Com_Printf( "%s %s\n", bitName, ((g_userinfoValidate.integer & (1<<index)) ? "Validated" : "Ignored") );
	}
}

char *G_ValidateUserinfo( const char *userinfo ) {
	unsigned int		i=0, count=0;
	size_t				length = strlen( userinfo );
	userinfoValidate_t	*info = NULL;
	char				key[BIG_INFO_KEY], value[BIG_INFO_VALUE];
	const char			*s;
	unsigned int		fieldCount[ARRAY_LEN( userinfoFields )];

	memset( fieldCount, 0, sizeof( fieldCount ) );

	// size checks
	if ( g_userinfoValidate.integer & UIV_EXTRA_BIT( USERINFO_VALIDATION_SIZE ) ) {
		if ( length < 1 )
			return "Userinfo too short";
		else if ( length >= MAX_INFO_STRING )
			return "Userinfo too long";
	}

	// slash checks
	if ( g_userinfoValidate.integer & UIV_EXTRA_BIT( USERINFO_VALIDATION_SLASH ) ) {
		// there must be a leading slash
		if ( userinfo[0] != '\\' )
			return "Missing leading slash";

		// no trailing slashes allowed, engine will append ip\\ip:port
		if ( userinfo[length-1] == '\\' )
			return "Trailing slash";

		// format for userinfo field is: \\key\\value
		// so there must be an even amount of slashes
		for ( i=0, count=0; i<length; i++ ) {
			if ( userinfo[i] == '\\' )
				count++;
		}
		if ( (count&1) ) // odd
			return "Bad number of slashes";
	}

	// extended characters are impossible to type, may want to disable
	if ( g_userinfoValidate.integer & UIV_EXTRA_BIT( USERINFO_VALIDATION_EXTASCII ) ) {
		for ( i=0, count=0; i<length; i++ ) {
			if ( userinfo[i] < 0 )
				count++;
		}
		if ( count )
			return "Extended ASCII characters found";
	}

	// disallow \n \r \t ; and \"
	// GalaxyRP: [Userinfo] tab added (as JA++ does) -- no client can legitimately send one
	if ( g_userinfoValidate.integer & UIV_EXTRA_BIT( USERINFO_VALIDATION_CONTROLCHARS ) ) {
		if ( Q_strchrs( userinfo, "\n\r\t;\"" ) )
			return "Invalid characters found";
	}

	s = userinfo;
	while ( s ) {
		Info_NextPair( &s, key, value );

		if ( !key[0] )
			break;

		for ( i=0; i<numUserinfoFields; i++ ) {
			if ( !Q_stricmp( key, userinfoFields[i].fieldClean ) )
				fieldCount[i]++;
		}
	}

	// count the number of fields
	for ( i=0, info=userinfoFields; i<numUserinfoFields; i++, info++ ) {
		if ( g_userinfoValidate.integer & (1<<info->bit) ) {
			if ( info->minCount && !fieldCount[i] )
				return va( "%s field not found", info->fieldClean );
			else if ( fieldCount[i] > info->maxCount )
				return va( "Too many %s fields (%i/%i)", info->fieldClean, fieldCount[i], info->maxCount );
		}
	}

	return NULL;
}


extern void update_current_character_name_and_model(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
qboolean ClientUserinfoChanged( int clientNum ) {
	gentity_t *ent = g_entities + clientNum;
	gclient_t *client = ent->client;
	int team=TEAM_FREE, health=100, maxHealth=100, teamLeader;
	const char *s=NULL;
	char *value=NULL, userinfo[MAX_INFO_STRING], buf[MAX_INFO_STRING], oldClientinfo[MAX_INFO_STRING], model[MAX_QPATH],
		forcePowers[DEFAULT_FORCEPOWERS_LEN], oldname[MAX_NETNAME], className[MAX_QPATH], color1[16], color2[16];
	qboolean modelChanged = qfalse;
	gender_t gender = GENDER_MALE;

	trap->GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	// check for malformed or illegal info strings
	s = G_ValidateUserinfo( userinfo );
	if ( s && *s ) {
		G_SecurityLogPrintf( "Client %d (%s) failed userinfo validation: %s [IP: %s]\n", clientNum, ent->client->pers.netname, s, client->sess.IP );
		trap->DropClient( clientNum, va( "Failed userinfo validation: %s", s ) );
		G_LogPrintf( "Userinfo: %s\n", userinfo );
		return qfalse;
	}

	// check for local client
	s = Info_ValueForKey( userinfo, "ip" );
	if ( !strcmp( s, "localhost" ) && !(ent->r.svFlags & SVF_BOT) )
		client->pers.localClient = qtrue;

	// Tr!Force: [Plugin] Check plugin
	s = Info_ValueForKey( userinfo, "rpmod_client" );
	if ( !strcmp( s, JK_VERSION ) ) {
		client->pers.clientPlugin = qtrue;
	} else {
		client->pers.clientPlugin = qfalse;
	}

	// check the item prediction
	s = Info_ValueForKey( userinfo, "cg_predictItems" );
	if ( !atoi( s ) )	client->pers.predictItemPickup = qfalse;
	else				client->pers.predictItemPickup = qtrue;

	// set name
	Q_strncpyz( oldname, client->pers.netname, sizeof( oldname ) );
	s = Info_ValueForKey( userinfo, "name" );
	ClientCleanName( s, client->pers.netname, sizeof( client->pers.netname ) );
	Q_strncpyz( client->pers.netname_nocolor, client->pers.netname, sizeof( client->pers.netname_nocolor ) );
	Q_StripColor( client->pers.netname_nocolor );

	if ( client->sess.sessionTeam == TEAM_SPECTATOR && client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		Q_strncpyz( client->pers.netname, "scoreboard", sizeof( client->pers.netname ) );
		Q_strncpyz( client->pers.netname_nocolor, "scoreboard", sizeof( client->pers.netname_nocolor ) );
	}

	if ( client->pers.connected == CON_CONNECTED && strcmp( oldname, client->pers.netname ) ) {
		if ( client->pers.netnameTime > level.time ) {
			trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NONAMECHANGE" ) ) );

			Info_SetValueForKey( userinfo, "name", oldname );
			trap->SetUserinfo( clientNum, userinfo );
			Q_strncpyz( client->pers.netname, oldname, sizeof( client->pers.netname ) );
			Q_strncpyz( client->pers.netname_nocolor, oldname, sizeof( client->pers.netname_nocolor ) );
			Q_StripColor( client->pers.netname_nocolor );

			// GalaxyRP fix: [Name] this rejection reverts the SERVER's copy of the name back to
			// oldname via trap->SetUserinfo above, but the client that just tried to rename is the
			// one whose own local "name" cvar still holds the rejected new name -- nothing here
			// told it to revert too, so its own console/display kept showing the rejected name
			// while the server (and everyone else) had already reverted to oldname. Same root
			// cause and same fix as CG_NameUpdate_f in cg_servercmds.c (used by set_netname() in
			// g_cmds.c for the database-driven case); this is the other place a name change can be
			// silently overridden -- the anti-spam cooldown on an organic, player-typed rename.
			trap->SendServerCommand( clientNum, va( "supdatename \"%s\"\n", oldname ) );
		}
		else {
			trap->SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " %s %s\n\"", oldname, G_GetStringEdString( "MP_SVGAME", "PLRENAME" ), client->pers.netname ) );
			G_LogPrintf( "ClientRename: %i [%s] (%s) \"%s^7\" -> \"%s^7\"\n", clientNum, ent->client->sess.IP, ent->client->pers.guid, oldname, ent->client->pers.netname );
			client->pers.netnameTime = level.time + 5000;
		}
	}

	// set model
	Q_strncpyz( model, Info_ValueForKey( userinfo, "model" ), sizeof( model ) );

	if ( d_perPlayerGhoul2.integer&& Q_stricmp( model, client->modelname ) ) {
		Q_strncpyz( client->modelname, model, sizeof( client->modelname ) );
		modelChanged = qtrue;
	}

	client->ps.customRGBA[0] = (value=Info_ValueForKey( userinfo, "char_color_red" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;
	client->ps.customRGBA[1] = (value=Info_ValueForKey( userinfo, "char_color_green" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;
	client->ps.customRGBA[2] = (value=Info_ValueForKey( userinfo, "char_color_blue" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;

	//Prevent skins being too dark
	if ( g_charRestrictRGB.integer && ((client->ps.customRGBA[0]+client->ps.customRGBA[1]+client->ps.customRGBA[2]) < 100) )
		client->ps.customRGBA[0] = client->ps.customRGBA[1] = client->ps.customRGBA[2] = 255;

	client->ps.customRGBA[3]=255;

	Q_strncpyz( forcePowers, Info_ValueForKey( userinfo, "forcepowers" ), sizeof( forcePowers ) );

	// update our customRGBA for team colors.
	if ( level.gametype >= GT_TEAM && level.gametype != GT_SIEGE && !g_jediVmerc.integer ) {
		char skin[MAX_QPATH] = {0};
		vec3_t colorOverride = {0.0f};

		VectorClear( colorOverride );

		BG_ValidateSkinForTeam( model, skin, client->sess.sessionTeam, colorOverride );
		if ( colorOverride[0] != 0.0f || colorOverride[1] != 0.0f || colorOverride[2] != 0.0f )
			VectorScaleM( colorOverride, 255.0f, client->ps.customRGBA );
	}

	// bots set their team a few frames later
	if ( level.gametype >= GT_TEAM && g_entities[clientNum].r.svFlags & SVF_BOT ) {
		s = Info_ValueForKey( userinfo, "team" );
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) )
			team = TEAM_RED;
		else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) )
			team = TEAM_BLUE;
		else
			team = PickTeam( clientNum ); // pick the team with the least number of players
	}
	else
		team = client->sess.sessionTeam;

	//Testing to see if this fixes the problem with a bot's team getting set incorrectly.
	team = client->sess.sessionTeam;

	//Set the siege class
	if ( level.gametype == GT_SIEGE ) {
		Q_strncpyz( className, client->sess.siegeClass, sizeof( className ) );

		//Now that the team is legal for sure, we'll go ahead and get an index for it.
		client->siegeClass = BG_SiegeFindClassIndexByName( className );
		if ( client->siegeClass == -1 ) {
			// ok, get the first valid class for the team you're on then, I guess.
			BG_SiegeCheckClassLegality( team, className );
			Q_strncpyz( client->sess.siegeClass, className, sizeof( client->sess.siegeClass ) );
			client->siegeClass = BG_SiegeFindClassIndexByName( className );
		}
		else {
			// otherwise, make sure the class we are using is legal.
			G_ValidateSiegeClassForTeam( ent, team );
			Q_strncpyz( className, client->sess.siegeClass, sizeof( className ) );
		}

		if ( client->siegeClass != -1 ) {
			// Set the sabers if the class dictates
			siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];

			G_SetSaber( ent, 0, scl->saber1[0] ? scl->saber1 : DEFAULT_SABER, qtrue );
			G_SetSaber( ent, 1, scl->saber2[0] ? scl->saber2 : "none", qtrue );

			//make sure the saber models are updated
			G_SaberModelSetup( ent );

			if ( scl->forcedModel[0] ) {
				// be sure to override the model we actually use
				Q_strncpyz( model, scl->forcedModel, sizeof( model ) );
				if ( d_perPlayerGhoul2.integer && Q_stricmp( model, client->modelname ) ) {
					Q_strncpyz( client->modelname, model, sizeof( client->modelname ) );
					modelChanged = qtrue;
				}
			}

			if ( G_PlayerHasCustomSkeleton( ent ) )
			{//force them to use their class model on the server, if the class dictates
				if ( Q_stricmp( model, client->modelname ) || ent->localAnimIndex == 0 )
				{
					Q_strncpyz( client->modelname, model, sizeof( client->modelname ) );
					modelChanged = qtrue;
				}
			}
		}
	}
	else
		Q_strncpyz( className, "none", sizeof( className ) );

	// only set the saber name on the first connect.
	//	it will be read from userinfo on ClientSpawn and stored in client->pers.saber1/2
	if ( !VALIDSTRING( client->pers.saber1 ) || !VALIDSTRING( client->pers.saber2 ) ) {
		G_SetSaber( ent, 0, Info_ValueForKey( userinfo, "saber1" ), qfalse );
		G_SetSaber( ent, 1, Info_ValueForKey( userinfo, "saber2" ), qfalse );
	}

	// set max health
	if ( level.gametype == GT_SIEGE && client->siegeClass != -1 ) {
		siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];

		if ( scl->maxhealth )
			maxHealth = scl->maxhealth;

		health = maxHealth;
	}
	else
	{
		if (ent->client->sess.amrpgmode == 2)
			health = ent->client->pers.max_rpg_health;
		else
			health = Com_Clampi( 1, 100, atoi( Info_ValueForKey( userinfo, "handicap" ) ) );
	}

	client->pers.maxHealth = health;
	if (ent->client->sess.amrpgmode < 2 && (client->pers.maxHealth < 1 || client->pers.maxHealth > maxHealth) )
		client->pers.maxHealth = 100;
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

	if ( level.gametype >= GT_TEAM )
		client->pers.teamInfo = qtrue;
	else {
		s = Info_ValueForKey( userinfo, "teamoverlay" );
		if ( !*s || atoi( s ) != 0 )
			client->pers.teamInfo = qtrue;
		else
			client->pers.teamInfo = qfalse;
	}

	// team task (0 = none, 1 = offence, 2 = defence)
//	teamTask = atoi(Info_ValueForKey(userinfo, "teamtask"));
	// team Leader (1 = leader, 0 is normal player)
	teamLeader = client->sess.teamLeader;

	// colors
	Q_strncpyz( color1, Info_ValueForKey( userinfo, "color1" ), sizeof( color1 ) );
	Q_strncpyz( color2, Info_ValueForKey( userinfo, "color2" ), sizeof( color2 ) );

	// GalaxyRP: [Saber RGB] custom blade colours. cp_sbRGB1/cp_sbRGB2 are CVAR_USERINFO cvars that
	// an RGB-capable client (ours, or a TaystJK/JAPro one) sets from its saber menu, so they arrive
	// here automatically along with the rest of the userinfo -- but the server keeps the
	// authoritative copy in pers.saberRGB[], because the same colour can also be set by /sabercolor
	// and restored from the character's database row on login, neither of which the client's own
	// cvar knows about. A client without RGB support sends nothing at all, so an absent or zero
	// value is treated as "no opinion" and deliberately does NOT clear a colour set by either of
	// those other two routes -- otherwise every ordinary userinfo change (a name or model edit)
	// would wipe it straight back out again.
	{
		int i;
		static const char *sbRGBKey[2] = { "cp_sbRGB1", "cp_sbRGB2" };
		static const char *colorKey[2] = { "color1", "color2" };

		for ( i = 0; i < 2; i++ ) {
			int fromClient = G_ParseSaberRGB( Info_ValueForKey( userinfo, sbRGBKey[i] ) );

			if ( fromClient )
				client->pers.saberRGB[i] = fromClient;
		}

		// GalaxyRP: [Saber RGB] which of the NUM_SABER_COLORS palette entries is selected is now
		// server-authoritative in pers.saberColorMode[] -- set by /sabercolor, /saberblade, or
		// restored from the database on login -- same "adopt only a plausible value, otherwise
		// keep what the server already has" treatment as saberRGB[] above, and for the same reason
		// (an ordinary client just echoes back whatever color1/color2 it was last told, which must
		// not stomp a change made through one of the other two routes). Unlike saberRGB[], 0 is a
		// legitimate, meaningful value here (SABER_RED), so this can't reuse G_ParseSaberRGB()'s
		// "<=0 means unset" semantics -- any value inside the enum's range is adopted as-is.
		for ( i = 0; i < 2; i++ ) {
			int fromClient = atoi( Info_ValueForKey( userinfo, colorKey[i] ) );

			if ( fromClient >= 0 && fromClient < NUM_SABER_COLORS )
				client->pers.saberColorMode[i] = fromClient;
		}

		// pers.saberColorMode[] is authoritative for what gets published below, replacing whatever
		// the client's own color1/color2 said -- every other client's cgame reads this off "c1"/"c2"
		// to decide both the palette entry and (for the five RGB-family values) whether to also
		// consult "c3"/"c4" for the actual custom colour.
		Q_strncpyz( color1, va( "%i", client->pers.saberColorMode[0] ), sizeof( color1 ) );
		Q_strncpyz( color2, va( "%i", client->pers.saberColorMode[1] ), sizeof( color2 ) );
	}

	// gender hints
	s = Info_ValueForKey( userinfo, "sex" );
	if ( !Q_stricmp( s, "female" ) )
		gender = GENDER_FEMALE;
	else
		gender = GENDER_MALE;

	s = Info_ValueForKey( userinfo, "snaps" );
	if ( atoi( s ) < sv_fps.integer )
		trap->SendServerCommand( clientNum, va( "print \"" S_COLOR_YELLOW "Recommend setting /snaps %d or higher to match this server's sv_fps\n\"", sv_fps.integer ) );

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	buf[0] = '\0';
	Q_strcat( buf, sizeof( buf ), va( "n\\%s\\", client->pers.netname ) );
	Q_strcat( buf, sizeof( buf ), va( "t\\%i\\", client->sess.sessionTeam ) );
	Q_strcat( buf, sizeof( buf ), va( "model\\%s\\", model ) );
	if ( gender == GENDER_FEMALE )	Q_strcat( buf, sizeof( buf ), va( "ds\\%c\\", 'f' ) );
	else							Q_strcat( buf, sizeof( buf ), va( "ds\\%c\\", 'm' ) );
	Q_strcat( buf, sizeof( buf ), va( "st\\%s\\", client->pers.saber1 ) );
	Q_strcat( buf, sizeof( buf ), va( "st2\\%s\\", client->pers.saber2 ) );
	Q_strcat( buf, sizeof( buf ), va( "c1\\%s\\", color1 ) );
	Q_strcat( buf, sizeof( buf ), va( "c2\\%s\\", color2 ) );
	// GalaxyRP: [Saber RGB] the packed custom colour behind a SABER_RGB c1/c2 above. Key names
	// match TaystJK/JAPro's so a TaystJK client rendering with its own cgame reads them too.
	Q_strcat( buf, sizeof( buf ), va( "c3\\%i\\", client->pers.saberRGB[0] ) );
	Q_strcat( buf, sizeof( buf ), va( "c4\\%i\\", client->pers.saberRGB[1] ) );
	Q_strcat( buf, sizeof( buf ), va( "hc\\%i\\", client->pers.maxHealth ) );
	if ( ent->r.svFlags & SVF_BOT )
		Q_strcat( buf, sizeof( buf ), va( "skill\\%s\\", Info_ValueForKey( userinfo, "skill" ) ) );
	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		Q_strcat( buf, sizeof( buf ), va( "w\\%i\\", client->sess.wins ) );
		Q_strcat( buf, sizeof( buf ), va( "l\\%i\\", client->sess.losses ) );
	}
	if ( level.gametype == GT_POWERDUEL )
		Q_strcat( buf, sizeof( buf ), va( "dt\\%i\\", client->sess.duelTeam ) );
	if ( level.gametype >= GT_TEAM ) {
	//	Q_strcat( buf, sizeof( buf ), va( "tt\\%d\\", teamTask ) );
		Q_strcat( buf, sizeof( buf ), va( "tl\\%d\\", teamLeader ) );
	}
	if ( level.gametype == GT_SIEGE ) {
		Q_strcat( buf, sizeof( buf ), va( "siegeclass\\%s\\", className ) );
		// GalaxyRP fix: [Siege] "sdt" ("siege desired team") was being filled with className (a
		// string) through a "%i" conversion instead of the actual desired-team value -- cg_players.c's
		// ClientInfo parsing (CG_NewClientInfo) reads this key back with atoi() into
		// newInfo.siegeDesiredTeam, so this was sending every client's desired Siege team as a
		// garbage number instead of the real value.
		Q_strcat( buf, sizeof( buf ), va( "sdt\\%i\\", client->sess.siegeDesiredTeam ) );
	}

	trap->GetConfigstring( CS_PLAYERS+clientNum, oldClientinfo, sizeof( oldClientinfo ) );
	trap->SetConfigstring( CS_PLAYERS+clientNum, buf );

	// only going to be true for allowable server-side custom skeleton cases
	if ( modelChanged ) {
		// update the server g2 instance if appropriate
		char *modelname = Info_ValueForKey( userinfo, "model" );
		SetupGameGhoul2Model( ent, modelname, NULL );

		if ( ent->ghoul2 && ent->client )
			ent->client->renderInfo.lastG2 = NULL; //update the renderinfo bolts next update.

		client->torsoAnimExecute = client->legsAnimExecute = -1;
		client->torsoLastFlip = client->legsLastFlip = qfalse;
	}

	if ( g_logClientInfo.integer ) {
		if ( strcmp( oldClientinfo, buf ) )
			G_LogPrintf( "ClientUserinfoChanged: %i %s\n", clientNum, buf );
		else
			G_LogPrintf( "ClientUserinfoChanged: %i <no change>\n", clientNum );
	}

	// GalaxyRP (Alex): [Database] Save character's stuff as soon as they change them.
	// GalaxyRP fix: [Saber] this used to also require sessionTeam == TEAM_FREE. The UPDATE below is the
	// only place saberOneModel/saberTwoModel and the two packed blade colours are ever written to the
	// database, so that condition meant those changes were never saved for anyone whose session team is
	// something else: every player in a Team, CTF or Siege game (where the team is RED or BLUE, never
	// FREE), and a logged-in player still sitting in spectator, which is the normal state right after
	// joining. The change applied live and the client was told it had worked, then it was silently gone
	// on the next login. Only the login check is needed here -- the save is keyed on the player's own
	// character and is meaningless without one.
	if (client->sess.loggedin == qtrue) {
		sqlite3* db;
		char* zErrMsg = 0;
		int rc;
		sqlite3_stmt* stmt = NULL;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			// GalaxyRP fix: [macOS/Clang build failure] this was a bare "return;" inside
			// qboolean ClientUserinfoChanged(), which every caller treats as pass/fail for the
			// userinfo change itself (e.g. ClientConnect() drops the client with "Failed userinfo
			// validation" if this returns falsy). The userinfo change already succeeded by this
			// point -- only the character DB save failed, which is already logged above -- so this
			// must report success (qtrue), not fail validation. On Linux/GCC the missing return
			// value was only a warning and happened to leave qtrue in the return register by luck;
			// Clang on macOS correctly rejects it as a hard error (-Wreturn-mismatch).
			return qtrue;
		}

		update_current_character_name_and_model(ent, db, zErrMsg, rc, stmt);

		sqlite3_close(db);
	}

	return qtrue;
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournament restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournament
restarts.
============
*/

static qboolean CompareIPs( const char *ip1, const char *ip2 )
{
	while ( 1 ) {
		if ( *ip1 != *ip2 )
			return qfalse;
		if ( !*ip1 || *ip1 == ':' )
			break;
		ip1++;
		ip2++;
	}

	return qtrue;
}

char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot ) {
	char		*value = NULL;
	gentity_t	*ent = NULL, *te = NULL;
	gclient_t	*client;
	char		userinfo[MAX_INFO_STRING] = {0},
				tmpIP[NET_ADDRSTRMAXLEN] = {0},
				guid[33] = {0};

	ent = &g_entities[ clientNum ];

	ent->s.number = clientNum;
	ent->classname = "connecting";

	trap->GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	value = Info_ValueForKey( userinfo, "ja_guid" );
	if( value[0] )
		Q_strncpyz( guid, value, sizeof( guid ) );
	else if( isBot )
		Q_strncpyz( guid, "BOT", sizeof( guid ) );
	else
		Q_strncpyz( guid, "NOGUID", sizeof( guid ) );

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	Q_strncpyz( tmpIP, isBot ? "Bot" : value, sizeof( tmpIP ) );
	if ( G_FilterPacket( value ) ) {
		return "Banned.";
	}

	if ( !isBot && g_needpass.integer ) {
		// check for a password
		value = Info_ValueForKey (userinfo, "password");
		if ( g_password.string[0] && Q_stricmp( g_password.string, "none" ) &&
			strcmp( g_password.string, value) != 0) {
			static char sTemp[1024];
			Q_strncpyz(sTemp, G_GetStringEdString("MP_SVGAME","INVALID_ESCAPE_TO_MAIN"), sizeof (sTemp) );
			return sTemp;// return "Invalid password";
		}
	}

	if ( !isBot && firstTime )
	{
		if ( g_antiFakePlayer.integer )
		{// patched, check for > g_maxConnPerIP connections from same IP
			int count=0, i=0;
			for ( i=0; i<sv_maxclients.integer; i++ )
			{
				#if 0
					if ( level.clients[i].pers.connected != CON_DISCONNECTED && i != clientNum )
					{
						if ( CompareIPs( clientNum, i ) )
						{
							if ( !level.security.clientConnectionActive[i] )
							{//This IP has a dead connection pending, wait for it to time out
							//	client->pers.connected = CON_DISCONNECTED;
								return "Please wait, another connection from this IP is still pending...";
							}
						}
					}
				#else
					if ( CompareIPs( tmpIP, level.clients[i].sess.IP ) )
						count++;
				#endif
			}
			if ( count > g_maxConnPerIP.integer )
			{
			//	client->pers.connected = CON_DISCONNECTED;
				return "Too many connections from the same IP";
			}
		}
	}

	if ( ent->inuse )
	{// if a player reconnects quickly after a disconnect, the client disconnect may never be called, thus flag can get lost in the ether
		G_LogPrintf( "Forcing disconnect on active client: %i\n", clientNum );
		// so lets just fix up anything that should happen on a disconnect
		ClientDisconnect( clientNum );
	}

	// they can connect
	client = &level.clients[ clientNum ];
	ent->client = client;

	//assign the pointer for bg entity access
	ent->playerState = &ent->client->ps;

	memset( client, 0, sizeof(*client) );

	Q_strncpyz( client->pers.guid, guid, sizeof( client->pers.guid ) );

	client->pers.connected = CON_CONNECTING;
	client->pers.connectTime = level.time;

	// zyk: initializing player_statuses value
	client->pers.player_statuses = 0;

	// read or initialize the session data
	if ( firstTime || level.newSession ) {
		G_InitSessionData( client, userinfo, isBot );
	}
	G_ReadSessionData( client );

	if (level.gametype == GT_SIEGE &&
		(firstTime || level.newSession))
	{ //if this is the first time then auto-assign a desired siege team and show briefing for that team
		client->sess.siegeDesiredTeam = 0;//PickTeam(ent->s.number);
		/*
		trap->SendServerCommand(ent->s.number, va("sb %i", client->sess.siegeDesiredTeam));
		*/
		//don't just show it - they'll see it if they switch to a team on purpose.
	}


	if (level.gametype == GT_SIEGE && client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		if (firstTime || level.newSession)
		{ //start as spec
			client->sess.siegeDesiredTeam = client->sess.sessionTeam;
			client->sess.sessionTeam = TEAM_SPECTATOR;
		}
	}
	else if (level.gametype == GT_POWERDUEL && client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		client->sess.sessionTeam = TEAM_SPECTATOR;
	}

	if( isBot ) {
		ent->r.svFlags |= SVF_BOT;
		ent->inuse = qtrue;
		if( !G_BotConnect( clientNum, !firstTime ) ) {
			return "BotConnectfailed";
		}
	}

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( clientNum ) )
		return "Failed userinfo validation";

	if ( !isBot && firstTime )
	{
		if ( !tmpIP[0] )
		{//No IP sent when connecting, probably an unban hack attempt
			client->pers.connected = CON_DISCONNECTED;
			G_SecurityLogPrintf( "Client %i (%s) sent no IP when connecting.\n", clientNum, client->pers.netname );
			return "Invalid userinfo detected";
		}
	}

	if ( firstTime )
		Q_strncpyz( client->sess.IP, tmpIP, sizeof( client->sess.IP ) );

	G_LogPrintf( "ClientConnect: %i [%s] (%s) \"%s^7\"\n", clientNum, tmpIP, guid, client->pers.netname );

	// don't do the "xxx connected" messages if they were caried over from previous level
	if ( firstTime ) {
		trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCONNECT")) );
	}

	if ( level.gametype >= GT_TEAM &&
		client->sess.sessionTeam != TEAM_SPECTATOR ) {
		BroadcastTeamChange( client, -1 );
	}

	// count current clients and rank for scoreboard
	CalculateRanks();

	te = G_TempEntity( vec3_origin, EV_CLIENTJOIN );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = clientNum;

	// for statistics
//	client->areabits = areabits;
//	if ( !client->areabits )
//		client->areabits = G_Alloc( (trap->AAS_PointReachabilityAreaIndex( NULL ) + 7) / 8 );

	return NULL;
}

void G_WriteClientSessionData( gclient_t *client );

void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern qboolean g_dontPenalizeTeam; //g_cmds.c
// GalaxyRP fix: [security] kept in sync with the real definition in g_cmds.c -- see the comment
// there for why this is declared char[32] (matching sess.filename below) rather than
// char[MAX_STRING_CHARS].
extern void select_account_and_default_character_data(gentity_t* ent, char username[32], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
extern void initialize_rpg_skills(gentity_t *ent);
extern void duel_tournament_end();
extern void melee_battle_end();
extern void player_discard_backup(gentity_t *ent);
extern void send_ally_update(gentity_t *ent);	// GalaxyRP: [Radar] g_cmds.c
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);
void ClientBegin( int clientNum, qboolean allowTeamReset ) {
	gentity_t	*ent;
	gclient_t	*client;
	int			flags, i;
	char		userinfo[MAX_INFO_VALUE], *modelname;
	int			spawnCount;

	ent = g_entities + clientNum;

	if ((ent->r.svFlags & SVF_BOT) && level.gametype >= GT_TEAM)
	{
		if (allowTeamReset)
		{
			const char *team = "Red";
			int preSess;

			//SetTeam(ent, "");
			ent->client->sess.sessionTeam = PickTeam(-1);
			trap->GetUserinfo(clientNum, userinfo, MAX_INFO_STRING);

			if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			{
				ent->client->sess.sessionTeam = TEAM_RED;
			}

			if (ent->client->sess.sessionTeam == TEAM_RED)
			{
				team = "Red";
			}
			else
			{
				team = "Blue";
			}

			Info_SetValueForKey( userinfo, "team", team );

			trap->SetUserinfo( clientNum, userinfo );

			ent->client->ps.persistant[ PERS_TEAM ] = ent->client->sess.sessionTeam;

			preSess = ent->client->sess.sessionTeam;
			G_ReadSessionData( ent->client );
			ent->client->sess.sessionTeam = preSess;
			G_WriteClientSessionData(ent->client);
			if ( !ClientUserinfoChanged( clientNum ) )
				return;
			ClientBegin(clientNum, qfalse);
			return;
		}
	}

	client = level.clients + clientNum;

	if ( ent->r.linked ) {
		trap->UnlinkEntity( (sharedEntity_t *)ent );
	}
	G_InitGentity( ent );
	ent->touch = 0;
	ent->pain = 0;
	ent->client = client;

	//assign the pointer for bg entity access
	ent->playerState = &ent->client->ps;

	// zyk: initializing attributes used in RPG mode
	// GalaxyRP fix: [Guardian] guardian_mode init removed - field is permanently 0, sole writer spawn_boss() had zero callers
	client->pers.being_mind_controlled = -1;
	client->pers.mind_controlled1_id = -1;
	// GalaxyRP fix: [Guardian] guardian_invoked_by_id init removed - field is permanently -1, sole writer spawn_boss() had zero callers

	// zyk: duelist went to spec. Reset him from duels
	// GalaxyRP: [Force Duel] the private-duel type goes with them. It is only ever read while
	// ps.duelInProgress is set and every path that sets that writes it first, so a stale value is
	// unreachable -- but a slot handed to a new player should not carry the last one's state.
	level.duel_types[ent->s.number] = 0;

	if (level.duel_players[ent->s.number] > -1)
	{
		level.duel_players[ent->s.number] = -1;
		level.duelists_quantity--;
	}

	// GalaxyRP fix: [Duel Tournament] end the tournament when this was the last duelist, exactly as
	// Cmd_DuelMode_f's leave branch already does. Dropping the count without ending it left
	// duel_tournament_mode at 1 and level.duel_tournament_model_id still naming the globe, and the
	// join branch spawns a fresh globe whenever duelists_quantity is 0 -- so the next player to sign
	// up inside the remaining signup window overwrote that id and orphaned the old entity, which
	// nothing then frees. Repeatable at will (sign up, spectate, rejoin, sign up), one leaked entity
	// slot and a few never-freed G_Alloc strings per lap, ending at G_Spawn()'s trap->Error(ERR_DROP)
	// -- a process exit on a dedicated build.
	//
	// Scoped to signup because that is the only mode the leak exists in: the join branch refuses at
	// duel_tournament_mode > 1, so no second globe can be spawned once the tournament starts, and the
	// mode-2 block in G_RunFrame() (g_main.c) already ends an emptied tournament there with its own
	// announcement. Ending early here would silence that message for no gain.
	//
	// GalaxyRP fix: [Duel Tournament] the "or paused" half. The reasoning above holds only while the
	// tournament is running: the mode-2 block that ends an emptied tournament lives inside
	// G_RunFrame()'s "not paused" wrapper, so while an admin has /duelpause on, nothing ends one at
	// all. Everyone leaving a paused mode-2-or-later tournament left it sitting at that mode with
	// duelists_quantity at 0 and the paused flag still set, which also refuses every new sign-up
	// (the join branch stops at duel_tournament_mode > 1) -- so that map had no duel tournament
	// available until somebody resumed or the map changed. Now that pausing genuinely freezes the
	// tournament clock, that state persists instead of resolving the moment anyone resumes.
	//
	// This is the whole of the reachable surface: ClientBegin (this site -- re-entering the world,
	// which is how a team change including one to spectator gets here) and ClientDisconnect are the
	// only two routes out of the roster while paused at mode 2 or later, because Cmd_DuelMode_f
	// refuses to let anyone leave once the tournament has started and its own leave branch already
	// ends an emptied signup. A paused tournament cannot be at mode 0 either -- Cmd_DuelPause_f
	// refuses there, and duel_tournament_end() clears the flag -- so the added clause cannot fire
	// when no tournament exists.
	if (level.duelists_quantity <= 0 &&
		(level.duel_tournament_mode == 1 || level.duel_tournament_paused == qtrue))
	{
		duel_tournament_end();
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7There are no duelists anymore. Tournament is over!\"");
	}

	// zyk: melee battle player went to spec
	if (level.melee_players[ent->s.number] > -1)
	{
		level.melee_players[ent->s.number] = -1;
		level.melee_mode_quantity--;

		// GalaxyRP fix: [Melee Battle] drop the pre-battle snapshot melee_battle_prepare() took.
		// Clearing level.melee_players[] on the line above removes this player from
		// melee_battle_end()'s loop, so without this the backup would stay pending forever: their
		// next battle's backup returns early on the stale flag and keeps the old snapshot, and so
		// does a Duel Tournament entered later, because both mini-games share the one pair of
		// fields. Discarded rather than applied because ClientSpawn() runs further down this same
		// function and rebuilds the loadout from scratch anyway.
		player_discard_backup(ent);
	}

	// GalaxyRP fix: [Melee Battle] end the battle when this was the last player, exactly as
	// Cmd_MeleeMode_f's leave branch already does. Same defect and same shape as the Duel Tournament
	// one above: melee_mode stayed at 1 with level.melee_model_id still naming the catwalk, and the
	// join branch spawns a fresh catwalk whenever melee_mode_quantity is 0, so the next sign-up
	// inside the 12-second window orphaned it. The G_FreeEntity() hook that clears melee_model_id
	// (g_utils.c) does not help here -- nothing ever frees the old entity for it to fire on.
	//
	// Scoped to signup for the same reason: joining is refused at melee_mode > 1, and the mode-2
	// block in G_RunFrame() (g_main.c) already ends an emptied battle with "No players left!".
	if (level.melee_mode == 1 && level.melee_mode_quantity <= 0)
	{
		melee_battle_end();
		trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7No players left! Melee Battle is over!\"");
	}

	client->pers.skill_counter = 0;

	client->pers.stun_baton_less_speed_timer = 0;

	VectorSet(client->pers.teleport_point,0,0,0);
	VectorCopy(client->ps.viewangles,client->pers.teleport_angles);

	VectorSet(client->pers.saved_origin, 0, 0, 0);
	VectorSet(client->pers.saved_view_angles, 0, 0, 0);

	client->pers.connected = CON_CONNECTED;
	client->pers.enterTime = level.time;
	client->pers.teamState.state = TEAM_BEGIN;

	// save eflags around this, because changing teams will
	// cause this to happen with a valid entity, and we
	// want to make sure the teleport bit is set right
	// so the viewpoint doesn't interpolate through the
	// world to the new position
	flags = client->ps.eFlags;
	spawnCount = client->ps.persistant[PERS_SPAWN_COUNT];

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}
		i++;
	}

	i = TRACK_CHANNEL_1;

	while (i < NUM_TRACK_CHANNELS)
	{
		if (ent->client->ps.fd.killSoundEntIndex[i-50] && ent->client->ps.fd.killSoundEntIndex[i-50] < MAX_GENTITIES && ent->client->ps.fd.killSoundEntIndex[i-50] > 0)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[i-50], CHAN_VOICE);
		}
		i++;
	}
	i = 0;

	memset( &client->ps, 0, sizeof( client->ps ) );
	client->ps.eFlags = flags;
	client->ps.persistant[PERS_SPAWN_COUNT] = spawnCount;

	client->ps.hasDetPackPlanted = qfalse;

	//first-time force power initialization
	WP_InitForcePowers( ent );

	//init saber ent
	WP_SaberInitBladeData( ent );

	// First time model setup for that player.
	trap->GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
	modelname = Info_ValueForKey (userinfo, "model");
	SetupGameGhoul2Model(ent, modelname, NULL);

	if ( ent->ghoul2 && ent->client )
		ent->client->renderInfo.lastG2 = NULL; //update the renderinfo bolts next update.

	// GalaxyRP fix: [Account] moved this account/character DB reload here from the very end of
	// ClientBegin (where it used to run *after* ClientSpawn() below already ran) so that
	// pers.skill_levels[]/inventory/saber/model are loaded from the database *before* ClientSpawn
	// grants starting weapons and equipment from them. On a map change (not a full reconnect) pers
	// is wiped fresh for the new map, so previously the very first spawn always used blank/zeroed
	// skill data -- the reload happened too late to help it, and only a manual /kill (which calls
	// ClientSpawn a second time, by which point the reload had already completed) picked up the
	// correct loadout. The original "server doesn't like loading the db while it's changing maps"
	// concern this was worked around for doesn't appear to apply to the DB read itself (this exact
	// SQLite call already ran unconditionally within ClientBegin before this fix, just later) --
	// the explicit initialize_rpg_skills() call below is being restored for the same reason New
	// Zyk mod's equivalent load_account() call in ClientBegin is immediately followed by its own
	// initialize_rpg_skills() call there too, rather than relying solely on ClientSpawn's copy
	// (which doesn't run at all for a player kept in Spectator by the checks just below).
	if (ent->client->sess.amrpgmode > 0)
	{
		// GalaxyRP fix: [Quests] a "if the target goes to spec or something, choose another target"
		// guard used to live here. Cmd_BountyQuest_f (the only setter of level.bounty_quest_choose_target/
		// target_id) was deleted as unreachable dead code (see the GalaxyRP fix comment in g_cmds.c),
		// and those level fields have been removed along with it, so this guard is removed too.

		// zyk: load account again
		ent->client->sess.loggedin = qtrue;

		// GalaxyRP: [Force Enlightenment] push the login state to the client immediately -- not just
		// on the next Profile menu open (see Cmd_GalaxyRpUi_f) -- so cgame's CG_GreyItem greys out
		// both Enlightenment pickups (DAJ_RP: logged-in players cannot take either color) as soon as
		// they're actually logged in, not only after they happen to open the Profile UI.
		trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

		sqlite3* db;
		char* zErrMsg = 0;
		int rc;
		sqlite3_stmt* stmt = NULL;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return;
		}

		select_account_and_default_character_data(ent, ent->client->sess.filename, db, zErrMsg, rc, stmt);

		sqlite3_close(db);

		initialize_rpg_skills(ent);
	}
	else
	{
		// GalaxyRP fix: [Force Enlightenment] the push above only ever sends a "1", so a client that
		// begins a map *without* an account session was never told so. ui_loggedin is CVAR_ROM|
		// CVAR_INTERNAL and cgame never resets it (Cvar_Register does not clobber an existing value),
		// so a player who was logged in on the previous map or server arrives here still holding a
		// stale "1" -- and cgame would then apply the wrong Enlightenment rule to them. (It used to
		// un-grey and predict a pickup of the wrong-side color that Touch_Item() refuses; with the
		// DAJ_RP rule, a stale "1" would instead grey out and refuse to predict their OWN side's
		// color, which the server does grant, so the pickup sound would go missing.) Send the real value
		// on this path too, so ui_loggedin is authoritative for every client from ClientBegin onward.
		//
		// Deliberately an else rather than a single unconditional push below the block: the block has
		// its own early return on RP_DB_Open() failure (which must still leave the client holding the
		// "1" the server already set), and on the normal path select_account_and_default_character_data()
		// pushes again itself, so hoisting the call would both lose a push and duplicate one.
		trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));
	}

	// GalaxyRP fix: [Radar] push the ally set every time a client begins. sess.ally1/ally2 ride the
	// session string and so survive a map change, but cgame's copy is zeroed in CG_Init -- and the
	// per-slot events this replaces were only ever sent when an ally was added or removed. The two
	// went out of step on every map change, and the radar drew every ally as an enemy until the
	// player happened to re-run /allyadd. Unconditional: a player with no allies gets 0 0, which is
	// exactly what their client should hold.
	send_ally_update(ent);

	if ( (level.gametype == GT_POWERDUEL && client->sess.sessionTeam != TEAM_SPECTATOR && client->sess.duelTeam == DUELTEAM_FREE) ||
		level.load_entities_timer != 0)
	{ // zyk: keep player as spectator if entities are not placed yet
		SetTeam( ent, "s" );
	}
	else
	{
		if ( level.gametype == GT_SIEGE && (!gSiegeRoundBegun || gSiegeRoundEnded) )
			SetTeamQuick( ent, TEAM_SPECTATOR, qfalse );

		// locate ent at a spawn point
		ClientSpawn( ent );
	}

	if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
		if ( level.gametype != GT_DUEL || level.gametype == GT_POWERDUEL ) {
			trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLENTER")) );
		}
	}
	G_LogPrintf( "ClientBegin: %i\n", clientNum );

	// count current clients and rank for scoreboard
	CalculateRanks();

	G_ClearClientLog(clientNum);

	// Check client plugin
	if (rp_pluginRequired.integer && !client->pers.clientPlugin)
	{
		char	pluginVersion[MAX_INFO_VALUE];
		int		allowDownload = trap->Cvar_VariableIntegerValue("sv_allowDownload");

		// Force to spectator mode
		if (rp_pluginRequired.integer == 2 && client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			SetTeam(ent, "spectator");
			return;
		}

		// Get info
		Q_strncpyz(pluginVersion, Info_ValueForKey(userinfo, "rpmod_client"), sizeof(pluginVersion));

		// GalaxyRP fix: [TaystJK] dropped the EternalJK/JAPro ("cjp_client" == "1.4JAPRO") detection
		// and its "switch to OpenJK" warning -- this mod now runs natively on TaystJK (EternalJK's
		// modern successor), so telling a player to install EternalJK-era OpenJK no longer applies.
		// The real client-plugin presence/version check below (rpmod_client vs JK_VERSION) is
		// unrelated to that and is kept as-is.

		// Show center print message
		if (!VALIDSTRINGCVAR(pluginVersion))
		{
			trap->SendServerCommand(clientNum, va("cp \"Please download\n^5%s^7 client plugin.\nIf you already have it then relaunch game\nusing the script file shipped with it\"", JK_VERSION));
			// GalaxyRP fix: [Plugin] mirror the center print message to the console too (the
			// EternalJK branch used to be the only place that did this) so the warning is still
			// visible after the on-screen popup fades.
			// GalaxyRP fix: [fs_forcegame] also cover the case where the player already has the
			// plugin/mod installed but launched TaystJK directly (or switched mods via its mod menu)
			// instead of using the DAJ_RP launch script -- fs_forcegame then stays on its "taystjk"
			// default, which wins filesystem search priority over GalaxyRP and silently loads
			// TaystJK's own bundled cgame/ui instead of ours. That can only be corrected by relaunching
			// via the script (fs_forcegame is CVAR_INIT, so no in-game command can change it).
			trap->SendServerCommand(clientNum, va("print \"^3You do not have the required client plugin installed. The server requires ^5%s^7.\n^3If you already have it installed, your game started with the wrong engine folder override - close the game and relaunch using the script file shipped with the client.\n\"", JK_VERSION));
			G_LogPrintf("ClientPlugin: Player does not have any plugin\n");
		}
		else
		{
			trap->SendServerCommand(clientNum, va("cp \"Your client plugin is\n^3%s\nThe server version is ^5%s^7\nCheck the console or %s\"", pluginVersion, JK_VERSION, (allowDownload ? "enable downloads in main menu" : "download from\n^2" JK_URL)));
			// GalaxyRP fix: [Plugin] mirror the center print message to the console too (see above).
			trap->SendServerCommand(clientNum, va("print \"^3Your client plugin is ^7%s^3, but the server requires ^5%s^7.\n\"", pluginVersion, JK_VERSION));
			G_LogPrintf("ClientPlugin: Player is using '%s' instead '%s'\n", pluginVersion, JK_VERSION);
		}

		// Show console print message
		if (allowDownload)
		{
			trap->SendServerCommand(clientNum, va("print \"Update your client plugin typing ^2/cl_allowdownload 1^7 in the console and reconnect\n\""));
		}
		else
		{
			trap->SendServerCommand(clientNum, va("print \"Download ^5%s^7 client plugin from ^2%s\n\"", JK_VERSION, JK_URL));
		}
	}

	if (client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		// Tr!Force: [Motd] Check screen message
		if (!client->sess.motdSeen && VALIDSTRINGCVAR(rp_screen_message.string))
		{
			// GalaxyRP fix: [Settings] this used to be gated behind
			// (ent->client->sess.amrpgmode == 0 || !(ent->client->pers.player_settings & (1 << 9))) --
			// letting a logged-in RPG player disable the screen message via the old /settings 4, "Allow
			// Screen Message". That per-player choice has been removed (see the fix comment on
			// settings_number_to_bit in Cmd_Settings_f) -- the screen message is now always shown.
			{
				// Delay motd for non-plugin clients
				int motdDelayed = rp_pluginRequired.integer && !client->pers.clientPlugin ? rp_screen_message_timer.integer + 2 : 0;

				client->motdTime = motdDelayed ? motdDelayed : rp_screen_message_timer.integer;
				client->sess.motdSeen = qtrue;
			}
		}
	}

	//GalaxyRP (Alex): [Ammo Recharge] Ammo recharge variables init.
	ent->client->sess.explosive_recharge_timer = 0;
	ent->client->sess.weapon_recharge_timer = 0;

	//GalaxyRP (Alex): [Training Saber] Initialise values for training saber.
	// GalaxyRP: [Training Saber fix] reset covers both saber slots now -- see the matching
	// comment on training_stored_damageScale/2 in g_local.h.
	ent->client->pers.training_mode = qfalse;
	for (i = 0; i < MAX_SABERS; i++)
	{
		ent->client->pers.training_stored_damageScale[i] = 0;
		ent->client->pers.training_stored_damageScale2[i] = 0;
	}
}

static qboolean AllForceDisabled(int force)
{
	int i;

	if (force)
	{
		for (i=0;i<NUM_FORCE_POWERS;i++)
		{
			if (!(force & (1<<i)))
			{
				return qfalse;
			}
		}

		return qtrue;
	}

	return qfalse;
}

//Convenient interface to set all my limb breakage stuff up -rww
void G_BreakArm(gentity_t *ent, int arm)
{
	int anim = -1;

	assert(ent && ent->client);

	if (ent->s.NPC_class == CLASS_VEHICLE || ent->localAnimIndex > 1)
	{ //no broken limbs for vehicles and non-humanoids
		return;
	}

	if (!arm)
	{ //repair him
		ent->client->ps.brokenLimbs = 0;
		return;
	}

	if (ent->client->ps.fd.saberAnimLevel == SS_STAFF)
	{ //I'm too lazy to deal with this as well for now.
		return;
	}

	if (arm == BROKENLIMB_LARM)
	{
		if (ent->client->saber[1].model[0] &&
			ent->client->ps.weapon == WP_SABER &&
			!ent->client->ps.saberHolstered &&
			ent->client->saber[1].soundOff)
		{ //the left arm shuts off its saber upon being broken
			G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
		}
	}

	ent->client->ps.brokenLimbs = 0; //make sure it's cleared out
	ent->client->ps.brokenLimbs |= (1 << arm); //this arm is now marked as broken

	//Do a pain anim based on the side. Since getting your arm broken does tend to hurt.
	if (arm == BROKENLIMB_LARM)
	{
		anim = BOTH_PAIN2;
	}
	else if (arm == BROKENLIMB_RARM)
	{
		anim = BOTH_PAIN3;
	}

	if (anim == -1)
	{
		return;
	}

	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

	//This could be combined into a single event. But I guess limbs don't break often enough to
	//worry about it.
	G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain25.wav") );
	//FIXME: A nice bone snapping sound instead if possible
	G_Sound(ent, CHAN_AUTO, G_SoundIndex( va("sound/player/bodyfall_human%i.wav", Q_irand(1, 3)) ));
}

//Update the ghoul2 instance anims based on the playerstate values
qboolean BG_SaberStanceAnim( int anim );
qboolean PM_RunningAnim( int anim );
void G_UpdateClientAnims(gentity_t *self, float animSpeedScale)
{
	static int f;
	static int torsoAnim;
	static int legsAnim;
	static int firstFrame, lastFrame;
	static int aFlags;
	static float animSpeed, lAnimSpeedScale;
	qboolean setTorso = qfalse;

	torsoAnim = (self->client->ps.torsoAnim);
	legsAnim = (self->client->ps.legsAnim);

	if (self->client->ps.saberLockFrame)
	{
		trap->G2API_SetBoneAnim(self->ghoul2, 0, "model_root", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		trap->G2API_SetBoneAnim(self->ghoul2, 0, "lower_lumbar", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		trap->G2API_SetBoneAnim(self->ghoul2, 0, "Motion", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		return;
	}

	if (self->localAnimIndex > 1 &&
		bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame == 0 &&
		bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames == 0)
	{ //We'll allow this for non-humanoids.
		goto tryTorso;
	}

	if (self->client->legsAnimExecute != legsAnim || self->client->legsLastFlip != self->client->ps.legsFlip)
	{
		animSpeed = 50.0f / bgAllAnims[self->localAnimIndex].anims[legsAnim].frameLerp;
		lAnimSpeedScale = (animSpeed *= animSpeedScale);

		if (bgAllAnims[self->localAnimIndex].anims[legsAnim].loopFrames != -1)
		{
			aFlags = BONE_ANIM_OVERRIDE_LOOP;
		}
		else
		{
			aFlags = BONE_ANIM_OVERRIDE_FREEZE;
		}

		if (animSpeed < 0)
		{
			lastFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame;
			firstFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame + bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames;
		}
		else
		{
			firstFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame;
			lastFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame + bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames;
		}

		aFlags |= BONE_ANIM_BLEND; //since client defaults to blend. Not sure if this will make much difference if any on server position, but it's here just for the sake of matching them.

		trap->G2API_SetBoneAnim(self->ghoul2, 0, "model_root", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
		self->client->legsAnimExecute = legsAnim;
		self->client->legsLastFlip = self->client->ps.legsFlip;
	}

tryTorso:
	if (self->localAnimIndex > 1 &&
		bgAllAnims[self->localAnimIndex].anims[torsoAnim].firstFrame == 0 &&
		bgAllAnims[self->localAnimIndex].anims[torsoAnim].numFrames == 0)

	{ //If this fails as well just return.
		return;
	}
	else if (self->s.number >= MAX_CLIENTS &&
		self->s.NPC_class == CLASS_VEHICLE)
	{ //we only want to set the root bone for vehicles
		return;
	}

	if ((self->client->torsoAnimExecute != torsoAnim || self->client->torsoLastFlip != self->client->ps.torsoFlip) &&
		!self->noLumbar)
	{
		aFlags = 0;
		animSpeed = 0;

		f = torsoAnim;

		BG_SaberStartTransAnim(self->s.number, self->client->ps.fd.saberAnimLevel, self->client->ps.weapon, f, &animSpeedScale, self->client->ps.brokenLimbs);

		animSpeed = 50.0f / bgAllAnims[self->localAnimIndex].anims[f].frameLerp;
		lAnimSpeedScale = (animSpeed *= animSpeedScale);

		if (bgAllAnims[self->localAnimIndex].anims[f].loopFrames != -1)
		{
			aFlags = BONE_ANIM_OVERRIDE_LOOP;
		}
		else
		{
			aFlags = BONE_ANIM_OVERRIDE_FREEZE;
		}

		aFlags |= BONE_ANIM_BLEND; //since client defaults to blend. Not sure if this will make much difference if any on client position, but it's here just for the sake of matching them.

		if (animSpeed < 0)
		{
			lastFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame;
			firstFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame + bgAllAnims[self->localAnimIndex].anims[f].numFrames;
		}
		else
		{
			firstFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame;
			lastFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame + bgAllAnims[self->localAnimIndex].anims[f].numFrames;
		}

		trap->G2API_SetBoneAnim(self->ghoul2, 0, "lower_lumbar", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, /*firstFrame why was it this before?*/-1, 150);

		self->client->torsoAnimExecute = torsoAnim;
		self->client->torsoLastFlip = self->client->ps.torsoFlip;

		setTorso = qtrue;
	}

	if (setTorso &&
		self->localAnimIndex <= 1)
	{ //only set the motion bone for humanoids.
		trap->G2API_SetBoneAnim(self->ghoul2, 0, "Motion", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
	}

#if 0 //disabled for now
	if (self->client->ps.brokenLimbs != self->client->brokenLimbs ||
		setTorso)
	{
		if (self->localAnimIndex <= 1 && self->client->ps.brokenLimbs &&
			(self->client->ps.brokenLimbs & (1 << BROKENLIMB_LARM)))
		{ //broken left arm
			char *brokenBone = "lhumerus";
			animation_t *armAnim;
			int armFirstFrame;
			int armLastFrame;
			int armFlags = 0;
			float armAnimSpeed;

			armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_DEAD21 ];
			self->client->brokenLimbs = self->client->ps.brokenLimbs;

			armFirstFrame = armAnim->firstFrame;
			armLastFrame = armAnim->firstFrame+armAnim->numFrames;
			armAnimSpeed = 50.0f / armAnim->frameLerp;
			armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

			trap->G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
		}
		else if (self->localAnimIndex <= 1 && self->client->ps.brokenLimbs &&
			(self->client->ps.brokenLimbs & (1 << BROKENLIMB_RARM)))
		{ //broken right arm
			char *brokenBone = "rhumerus";
			char *supportBone = "lhumerus";

			self->client->brokenLimbs = self->client->ps.brokenLimbs;

			//Only put the arm in a broken pose if the anim is such that we
			//want to allow it.
			if ((//self->client->ps.weapon == WP_MELEE ||
				self->client->ps.weapon != WP_SABER ||
				BG_SaberStanceAnim(self->client->ps.torsoAnim) ||
				PM_RunningAnim(self->client->ps.torsoAnim)) &&
				(!self->client->saber[1].model[0] || self->client->ps.weapon != WP_SABER))
			{
				int armFirstFrame;
				int armLastFrame;
				int armFlags = 0;
				float armAnimSpeed;
				animation_t *armAnim;

				if (self->client->ps.weapon == WP_MELEE ||
					self->client->ps.weapon == WP_SABER ||
					self->client->ps.weapon == WP_BRYAR_PISTOL)
				{ //don't affect this arm if holding a gun, just make the other arm support it
					armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_ATTACK2 ];

					//armFirstFrame = armAnim->firstFrame;
					armFirstFrame = armAnim->firstFrame+armAnim->numFrames;
					armLastFrame = armAnim->firstFrame+armAnim->numFrames;
					armAnimSpeed = 50.0f / armAnim->frameLerp;
					armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

					trap->G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
				}
				else
				{ //we want to keep the broken bone updated for some cases
					trap->G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				}

				if (self->client->ps.torsoAnim != BOTH_MELEE1 &&
					self->client->ps.torsoAnim != BOTH_MELEE2 &&
					(self->client->ps.torsoAnim == TORSO_WEAPONREADY2 || self->client->ps.torsoAnim == BOTH_ATTACK2 || self->client->ps.weapon < WP_BRYAR_PISTOL))
				{
					//Now set the left arm to "support" the right one
					armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_STAND2 ];
					armFirstFrame = armAnim->firstFrame;
					armLastFrame = armAnim->firstFrame+armAnim->numFrames;
					armAnimSpeed = 50.0f / armAnim->frameLerp;
					armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

					trap->G2API_SetBoneAnim(self->ghoul2, 0, supportBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
				}
				else
				{ //we want to keep the support bone updated for some cases
					trap->G2API_SetBoneAnim(self->ghoul2, 0, supportBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				}
			}
			else
			{ //otherwise, keep it set to the same as the torso
				trap->G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				trap->G2API_SetBoneAnim(self->ghoul2, 0, supportBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
			}
		}
		else if (self->client->brokenLimbs)
		{ //remove the bone now so it can be set again
			char *brokenBone = NULL;
			int broken = 0;

			//Warning: Don't remove bones that you've added as bolts unless you want to invalidate your bolt index
			//(well, in theory, I haven't actually run into the problem)
			if (self->client->brokenLimbs & (1<<BROKENLIMB_LARM))
			{
				brokenBone = "lhumerus";
				broken |= (1<<BROKENLIMB_LARM);
			}
			else if (self->client->brokenLimbs & (1<<BROKENLIMB_RARM))
			{ //can only have one arm broken at once.
				brokenBone = "rhumerus";
				broken |= (1<<BROKENLIMB_RARM);

				//want to remove the support bone too then
				trap->G2API_SetBoneAnim(self->ghoul2, 0, "lhumerus", 0, 1, 0, 0, level.time, -1, 0);
				trap->G2API_RemoveBone(self->ghoul2, "lhumerus", 0);
			}

			assert(brokenBone);

			//Set the flags and stuff to 0, so that the remove will succeed
			trap->G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, 0, 1, 0, 0, level.time, -1, 0);

			//Now remove it
			trap->G2API_RemoveBone(self->ghoul2, brokenBone, 0);
			self->client->brokenLimbs &= ~broken;
		}
	}
#endif
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
extern saber_db_info_t select_saber_info_using_char_id(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
extern qboolean WP_HasForcePowers( const playerState_t *ps );
// GalaxyRP fix: [Guardian] clean_guardians extern declaration removed - function was a permanent no-op and its definition was deleted from g_cmds.c
extern void zyk_add_force_powers( gentity_t *ent );
extern void zyk_add_guns( gentity_t *ent );
extern void zyk_remove_force_powers( gentity_t *ent );
extern void zyk_remove_guns( gentity_t *ent );
extern void do_scale(gentity_t *ent, int new_size);
extern void zyk_load_common_settings(gentity_t *ent);
extern void select_weapons_table_row_from_entity(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void ClientSpawn(gentity_t *ent) {
	int					i = 0, index = 0, saveSaberNum = ENTITYNUM_NONE, wDisable = 0, savedSiegeIndex = 0, maxHealth = 100;
	vec3_t				spawn_origin, spawn_angles;
	gentity_t			*spawnPoint = NULL, *tent = NULL;
	gclient_t			*client = NULL;
	clientPersistant_t	saved;
	clientSession_t		savedSess;
	forcedata_t			savedForce;
	saberInfo_t			saberSaved[MAX_SABERS];
	int					persistant[MAX_PERSISTANT] = {0};
	int					flags, gameFlags, savedPing, accuracy_hits, accuracy_shots, eventSequence;
	void				*g2WeaponPtrs[MAX_SABERS];
	char				userinfo[MAX_INFO_STRING] = {0}, *key = NULL, *value = NULL, *saber = NULL;
	qboolean			changedSaber = qfalse, inSiegeWithClass = qfalse;

	index = ent - g_entities;
	client = ent->client;

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo( index, userinfo, sizeof( userinfo ) );

	// GalaxyRP fix: only pull the saber from the database on the player's first spawn after
	// logging in (i.e. before client->pers.saber1/2 have ever been populated). This used to run
	// on *every* spawn (including a plain /kill respawn) and unconditionally clobbered the live
	// userinfo saber1/saber2 keys with whatever was already stored in the database. Since those
	// are the only values compared against client->pers.saber1/2 below to detect a saber change,
	// a fresh saber choice picked in the UI (which only updates userinfo, not the DB) could never
	// be detected for logged-in players -- they appeared permanently stuck on their last saved
	// saber. Logged-out players didn't hit this branch at all, and /saber worked instantly
	// because it calls G_SetSaber() directly. Restricting the DB read to the first spawn lets the
	// normal userinfo-vs-pers comparison below (and the resulting DB save triggered from
	// ClientUserinfoChanged) apply and persist saber changes made via the UI, same as it already
	// does for logged-out players.
	if (ent->client->sess.loggedin && (!VALIDSTRING(client->pers.saber1) || !VALIDSTRING(client->pers.saber2))) {
		sqlite3* db;
		char* zErrMsg = 0;
		int rc;
		sqlite3_stmt* stmt = NULL;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return;
		}

		saber_db_info_t saber_info = select_saber_info_using_char_id(ent, db, zErrMsg, rc, stmt);

		Info_SetValueForKey(userinfo, "saber1", saber_info.saber1Model);
		Info_SetValueForKey(userinfo, "saber2", saber_info.saber2Model);

		sqlite3_close(db);
	}

	for ( i=0; i<MAX_SABERS; i++ )
	{
		saber = (i&1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
		value = Info_ValueForKey( userinfo, va( "saber%i", i+1 ) );
		if ( saber && value &&
			(Q_stricmp( value, saber ) || !saber[0] || !ent->client->saber[0].model[0]) )
		{ //doesn't match up (or our saber is BS), we want to try setting it
			if ( G_SetSaber( ent, i, value, qfalse ) )
				changedSaber = qtrue;

			//Well, we still want to say they changed then (it means this is siege and we have some overrides)
			else if ( !saber[0] || !ent->client->saber[0].model[0] )
				changedSaber = qtrue;
		}
	}

	if ( changedSaber )
	{ //make sure our new info is sent out to all the other clients, and give us a valid stance
		if ( !ClientUserinfoChanged( ent->s.number ) )
			return;

		//make sure the saber models are updated
		G_SaberModelSetup( ent );

		for ( i=0; i<MAX_SABERS; i++ )
		{
			saber = (i&1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
			key = va( "saber%d", i+1 );
			value = Info_ValueForKey( userinfo, key );
			if ( Q_stricmp( value, saber ) )
			{// they don't match up, force the user info
				Info_SetValueForKey( userinfo, key, saber );
				trap->SetUserinfo( ent->s.number, userinfo );
			}
		}

		if ( ent->client->saber[0].model[0] && ent->client->saber[1].model[0] )
		{ //dual
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
		}
		else if ( (ent->client->saber[0].saberFlags&SFL_TWO_HANDED) )
		{ //staff
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
		}
		else
		{
			ent->client->sess.saberLevel = Com_Clampi( SS_FAST, SS_STRONG, ent->client->sess.saberLevel );
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

			// limit our saber style to our force points allocated to saber offense
			if ( level.gametype != GT_SIEGE && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
				ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		}
		if ( level.gametype != GT_SIEGE )
		{// let's just make sure the styles we chose are cool
			if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
			{
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
				ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			}
		}

		// GalaxyRP fix: [Saber] this block is a second, independent place (besides update_saber()
		// in g_cmds.c) where G_SetSaber() can end up correcting/rejecting a saber -- e.g. a UI
		// Apply that picked an invalid two-handed combo, only detected here on the player's next
		// spawn/kill. Without this push the client's own local saber1/saber2 cvars (and thus its
		// console echo and the saber-selection UI) never learned about the correction, same root
		// cause as CG_SaberUpdate_f in cg_servercmds.c was added for. Scoped inside changedSaber
		// (unlike update_saber()'s unconditional send) because ClientSpawn runs on every respawn,
		// not just on an explicit player action -- nothing needs telling when nothing changed.
		trap->SendServerCommand(ent - g_entities, va("supdatesaber \"%s\" \"%s\"\n", ent->client->pers.saber1, ent->client->pers.saber2));
	}

	if (client->ps.fd.forceDoInit)
	{ //force a reread of force powers
		WP_InitForcePowers( ent );
		client->ps.fd.forceDoInit = 0;
	}

	if (ent->client->ps.fd.saberAnimLevel != SS_STAFF && ent->client->ps.fd.saberAnimLevel != SS_DUAL &&
		ent->client->ps.fd.saberAnimLevel == ent->client->ps.fd.saberDrawAnimLevel &&
		ent->client->ps.fd.saberAnimLevel == ent->client->sess.saberLevel)
	{
		ent->client->sess.saberLevel = Com_Clampi( SS_FAST, SS_STRONG, ent->client->sess.saberLevel );
		ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

		// limit our saber style to our force points allocated to saber offense
		if ( level.gametype != GT_SIEGE && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
	}

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		spawnPoint = SelectSpectatorSpawnPoint (
						spawn_origin, spawn_angles);
	} else if (level.gametype == GT_CTF || level.gametype == GT_CTY) {
		// all base oriented team games use the CTF spawn points
		spawnPoint = SelectCTFSpawnPoint (
						client->sess.sessionTeam,
						client->pers.teamState.state,
						spawn_origin, spawn_angles, !!(ent->r.svFlags & SVF_BOT));
	}
	else if (level.gametype == GT_SIEGE)
	{
		spawnPoint = SelectSiegeSpawnPoint (
						client->siegeClass,
						client->sess.sessionTeam,
						client->pers.teamState.state,
						spawn_origin, spawn_angles, !!(ent->r.svFlags & SVF_BOT));
	}
	else {
		if (level.gametype == GT_POWERDUEL)
		{
			spawnPoint = SelectDuelSpawnPoint(client->sess.duelTeam, client->ps.origin, spawn_origin, spawn_angles, !!(ent->r.svFlags & SVF_BOT));
		}
		else if (level.gametype == GT_DUEL)
		{	// duel
			spawnPoint = SelectDuelSpawnPoint(DUELTEAM_SINGLE, client->ps.origin, spawn_origin, spawn_angles, !!(ent->r.svFlags & SVF_BOT));
		}
		else
		{
			// the first spawn should be at a good looking spot
			if ( !client->pers.initialSpawn && client->pers.localClient ) {
				client->pers.initialSpawn = qtrue;
				spawnPoint = SelectInitialSpawnPoint( spawn_origin, spawn_angles, client->sess.sessionTeam, !!(ent->r.svFlags & SVF_BOT) );
			} else {
				// don't spawn near existing origin if possible
				spawnPoint = SelectSpawnPoint (
					client->ps.origin,
					spawn_origin, spawn_angles, client->sess.sessionTeam, !!(ent->r.svFlags & SVF_BOT) );
			}
		}
	}
	client->pers.teamState.state = TEAM_ACTIVE;

	// toggle the teleport bit so the client knows to not lerp
	// and never clear the voted flag
	flags = ent->client->ps.eFlags & (EF_TELEPORT_BIT);
	flags ^= EF_TELEPORT_BIT;
	gameFlags = ent->client->mGameFlags & ( PSG_VOTED | PSG_TEAMVOTED);

	// clear everything but the persistant data

	saved = client->pers;
	savedSess = client->sess;
	savedPing = client->ps.ping;
//	savedAreaBits = client->areabits;
	accuracy_hits = client->accuracy_hits;
	accuracy_shots = client->accuracy_shots;
	for ( i=0; i<MAX_PERSISTANT; i++ )
		persistant[i] = client->ps.persistant[i];

	eventSequence = client->ps.eventSequence;

	savedForce = client->ps.fd;

	saveSaberNum = client->ps.saberEntityNum;

	savedSiegeIndex = client->siegeClass;

	for ( i=0; i<MAX_SABERS; i++ )
	{
		saberSaved[i] = client->saber[i];
		g2WeaponPtrs[i] = client->weaponGhoul2[i];
	}

	for ( i=0; i<HL_MAX; i++ )
		ent->locationDamage[i] = 0;

	// GalaxyRP: [Grapple Hook] before the memset below wipes client->hook: a hook that survives a
	// respawn keeps its parent pointer and would spend the rest of its lifetime writing an anchor
	// into the new spawn's lastHitLoc. Weapon_HookThink() also refuses a hook its owner has
	// forgotten, but this is the place the forgetting happens, so this is where it is prevented.
	if ( client->hook )
	{
		Weapon_HookFree( client->hook );
	}

	memset( client, 0, sizeof( *client ) ); // bk FIXME: Com_Memset?
	client->bodyGrabIndex = ENTITYNUM_NONE;

	//Get the skin RGB based on his userinfo
	client->ps.customRGBA[0] = (value=Info_ValueForKey( userinfo, "char_color_red" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;
	client->ps.customRGBA[1] = (value=Info_ValueForKey( userinfo, "char_color_green" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;
	client->ps.customRGBA[2] = (value=Info_ValueForKey( userinfo, "char_color_blue" ))	? Com_Clampi( 0, 255, atoi( value ) ) : 255;

	//Prevent skins being too dark
	if ( g_charRestrictRGB.integer && ((client->ps.customRGBA[0]+client->ps.customRGBA[1]+client->ps.customRGBA[2]) < 100) )
		client->ps.customRGBA[0] = client->ps.customRGBA[1] = client->ps.customRGBA[2] = 255;

	client->ps.customRGBA[3]=255;

	if ( level.gametype >= GT_TEAM && level.gametype != GT_SIEGE && !g_jediVmerc.integer )
	{
		char skin[MAX_QPATH] = {0}, model[MAX_QPATH] = {0};
		vec3_t colorOverride = {0.0f};

		VectorClear( colorOverride );
		Q_strncpyz( model, Info_ValueForKey( userinfo, "model" ), sizeof( model ) );

		BG_ValidateSkinForTeam( model, skin, savedSess.sessionTeam, colorOverride );
		if ( colorOverride[0] != 0.0f || colorOverride[1] != 0.0f || colorOverride[2] != 0.0f )
			VectorScaleM( colorOverride, 255.0f, client->ps.customRGBA );
	}

	client->siegeClass = savedSiegeIndex;

	for ( i=0; i<MAX_SABERS; i++ )
	{
		client->saber[i] = saberSaved[i];
		client->weaponGhoul2[i] = g2WeaponPtrs[i];
	}

	//or the saber ent num
	client->ps.saberEntityNum = saveSaberNum;
	client->saberStoredIndex = saveSaberNum;

	client->ps.fd = savedForce;

	client->ps.duelIndex = ENTITYNUM_NONE;

	//spawn with 100
	client->ps.jetpackFuel = 100;
	client->ps.cloakFuel = 100;

	client->pers = saved;
	client->sess = savedSess;
	client->ps.ping = savedPing;
//	client->areabits = savedAreaBits;
	client->accuracy_hits = accuracy_hits;
	client->accuracy_shots = accuracy_shots;
	client->lastkilled_client = -1;

	for ( i=0; i<MAX_PERSISTANT; i++ )
		client->ps.persistant[i] = persistant[i];

	client->ps.eventSequence = eventSequence;
	// increment the spawncount so the client will detect the respawn
	client->ps.persistant[PERS_SPAWN_COUNT]++;
	client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

	client->airOutTime = level.time + 12000;

	// set max health
	if (level.gametype == GT_SIEGE && client->siegeClass != -1)
	{
		siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];
		maxHealth = 100;

		if (scl->maxhealth)
		{
			maxHealth = scl->maxhealth;
		}
	}
	else
	{
		maxHealth = Com_Clampi( 1, 100, atoi( Info_ValueForKey( userinfo, "handicap" ) ) );
	}
	client->pers.maxHealth = maxHealth;//atoi( Info_ValueForKey( userinfo, "handicap" ) );
	if (ent->client->sess.amrpgmode < 2 && (client->pers.maxHealth < 1 || client->pers.maxHealth > maxHealth) ) {
		client->pers.maxHealth = 100;
	}
	// clear entity values
	if (ent->client->sess.amrpgmode < 2) // zyk: clearmax health if player is not in RPG
		client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
	else
	{
		client->pers.maxHealth = ent->client->pers.max_rpg_health;
		client->ps.stats[STAT_MAX_HEALTH] = ent->client->pers.max_rpg_health;
	}

	client->ps.eFlags = flags;
	client->mGameFlags = gameFlags;

	client->ps.groundEntityNum = ent->s.groundEntityNum = ENTITYNUM_NONE;
	ent->client = &level.clients[index];
	ent->playerState = &ent->client->ps;
	ent->takedamage = qtrue;
	ent->inuse = qtrue;
	ent->classname = "player";
	ent->r.contents = CONTENTS_BODY;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags = 0;

	VectorCopy (playerMins, ent->r.mins);
	VectorCopy (playerMaxs, ent->r.maxs);
	client->ps.crouchheight = CROUCH_MAXS_2;
	client->ps.standheight = DEFAULT_MAXS_2;

	client->ps.clientNum = index;
	//give default weapons
	client->ps.stats[STAT_WEAPONS] = ( 1 << WP_NONE );

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		wDisable = g_duelWeaponDisable.integer;
	}
	else
	{
		wDisable = g_weaponDisable.integer;
	}



	if ( level.gametype != GT_HOLOCRON
		&& level.gametype != GT_JEDIMASTER
		&& !HasSetSaberOnly()
		&& !AllForceDisabled( G_ForcePowerDisableValue() ) // GalaxyRP fix: [Force] duel-aware, see G_ForcePowerDisableValue()
		&& g_jediVmerc.integer )
	{
		if ( level.gametype >= GT_TEAM && (client->sess.sessionTeam == TEAM_BLUE || client->sess.sessionTeam == TEAM_RED) )
		{//In Team games, force one side to be merc and other to be jedi
			if ( level.numPlayingClients > 0 )
			{//already someone in the game
				int forceTeam = TEAM_SPECTATOR;
				for ( i = 0 ; i < level.maxclients ; i++ )
				{
					if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
						continue;
					}
					if ( level.clients[i].sess.sessionTeam == TEAM_BLUE || level.clients[i].sess.sessionTeam == TEAM_RED )
					{//in-game
						if ( WP_HasForcePowers( &level.clients[i].ps ) )
						{//this side is using force
							forceTeam = level.clients[i].sess.sessionTeam;
						}
						else
						{//other team is using force
							if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							{
								forceTeam = TEAM_RED;
							}
							else
							{
								forceTeam = TEAM_BLUE;
							}
						}
						break;
					}
				}
				if ( WP_HasForcePowers( &client->ps ) && client->sess.sessionTeam != forceTeam )
				{//using force but not on right team, switch him over
					const char *teamName = TeamName( forceTeam );
					//client->sess.sessionTeam = forceTeam;
					SetTeam( ent, (char *)teamName );
					return;
				}
			}
		}

		if ( WP_HasForcePowers( &client->ps ) )
		{
			client->ps.trueNonJedi = qfalse;
			client->ps.trueJedi = qtrue;
			//make sure they only use the saber
			client->ps.weapon = WP_SABER;
			client->ps.stats[STAT_WEAPONS] = (1 << WP_SABER);
		}
		else
		{//no force powers set
			client->ps.trueNonJedi = qtrue;
			client->ps.trueJedi = qfalse;
			if (!wDisable || !(wDisable & (1 << WP_BRYAR_PISTOL)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			if (!wDisable || !(wDisable & (1 << WP_BLASTER)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BLASTER );
			}
			if (!wDisable || !(wDisable & (1 << WP_BOWCASTER)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BOWCASTER );
			}
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
			client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			client->ps.ammo[AMMO_POWERCELL] = ammoData[AMMO_POWERCELL].max;
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
	}
	else
	{//jediVmerc is incompatible with this gametype, turn it off!
		trap->Cvar_Set( "g_jediVmerc", "0" );
		trap->Cvar_Update( &g_jediVmerc );
		if (level.gametype == GT_HOLOCRON)
		{
			//always get free saber level 1 in holocron
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );	//these are precached in g_items, ClearRegisteredItems()
		}
		else
		{
			if (client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );	//these are precached in g_items, ClearRegisteredItems()
			}
			//else
			//{ //if you don't have saber attack rank then you don't get a saber
				// zyk: now player will always have melee
			client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			//}
		}

		if (level.gametype != GT_SIEGE)
		{
			if (!wDisable || !(wDisable & (1 << WP_BRYAR_PISTOL)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			else if (level.gametype == GT_JEDIMASTER)
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
		}

		if (level.gametype == GT_JEDIMASTER)
		{
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
			client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
		}

		if (client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
		{
			client->ps.weapon = WP_SABER;
		}
		else if (client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL))
		{
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
		else
		{
			client->ps.weapon = WP_MELEE;
		}
	}

	/*
	client->ps.stats[STAT_HOLDABLE_ITEMS] |= ( 1 << HI_BINOCULARS );
	client->ps.stats[STAT_HOLDABLE_ITEM] = BG_GetItemIndexByTag(HI_BINOCULARS, IT_HOLDABLE);
	*/

	if (level.gametype == GT_SIEGE && client->siegeClass != -1 &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //well then, we will use a custom weaponset for our class
		int m = 0;

		client->ps.stats[STAT_WEAPONS] = bgSiegeClasses[client->siegeClass].weapons;

		if (client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
		{
			client->ps.weapon = WP_SABER;
		}
		else if (client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL))
		{
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
		else
		{
			client->ps.weapon = WP_MELEE;
		}
		inSiegeWithClass = qtrue;

		while (m < WP_NUM_WEAPONS)
		{
			if (client->ps.stats[STAT_WEAPONS] & (1 << m))
			{
				if (client->ps.weapon != WP_SABER)
				{ //try to find the highest ranking weapon we have
					if (m > client->ps.weapon)
					{
						client->ps.weapon = m;
					}
				}

				if (m >= WP_BRYAR_PISTOL)
				{ //Max his ammo out for all the weapons he has.
					// zyk: all weapons, including rocket launcher, will have max ammo based on the max ammo cvars
					int ammo_type = AMMO_BLASTER;
					int max_ammo = rp_max_blaster_pack_ammo.integer;

					if (m == WP_DISRUPTOR || m == WP_BOWCASTER || m == WP_DEMP2)
					{
						ammo_type = AMMO_POWERCELL;
						max_ammo = rp_max_power_cell_ammo.integer;
					}
					else if (m == WP_REPEATER || m == WP_FLECHETTE || m == WP_CONCUSSION)
					{
						ammo_type = AMMO_METAL_BOLTS;
						max_ammo = rp_max_metal_bolt_ammo.integer;
					}
					else if (m == WP_ROCKET_LAUNCHER)
					{
						ammo_type = AMMO_ROCKETS;
						max_ammo = rp_max_rocket_ammo.integer;
					}
					else if (m == WP_THERMAL)
					{
						ammo_type = AMMO_THERMAL;
						max_ammo = rp_max_thermal_ammo.integer;
					}
					else if (m == WP_TRIP_MINE)
					{
						ammo_type = AMMO_TRIPMINE;
						max_ammo = rp_max_tripmine_ammo.integer;
					}
					else if (m == WP_DET_PACK)
					{
						ammo_type = AMMO_DETPACK;
						max_ammo = rp_max_detpack_ammo.integer;
					}

					client->ps.ammo[ammo_type] = max_ammo;

					if ( level.gametype == GT_SIEGE
						&& client->siegeClass != -1
						&& (bgSiegeClasses[client->siegeClass].classflags & (1<<CFL_EXTRA_AMMO)) )
					{//double ammo
						client->ps.ammo[ammo_type] *= 2;
						client->ps.eFlags |= EF_DOUBLE_AMMO;
					}
				}
			}
			m++;
		}
	}

	if (level.gametype == GT_SIEGE &&
		client->siegeClass != -1 &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //use class-specified inventory
		client->ps.stats[STAT_HOLDABLE_ITEMS] = bgSiegeClasses[client->siegeClass].invenItems;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}
	else
	{
		client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	if (level.gametype == GT_SIEGE &&
		client->siegeClass != -1 &&
		bgSiegeClasses[client->siegeClass].powerups &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //this class has some start powerups
		i = 0;
		while (i < PW_NUM_POWERUPS)
		{
			if (bgSiegeClasses[client->siegeClass].powerups & (1 << i))
			{
				client->ps.powerups[i] = Q3_INFINITE;
			}
			i++;
		}
	}

	if ( client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		client->ps.stats[STAT_WEAPONS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

// nmckenzie: DESERT_SIEGE... or well, siege generally.  This was over-writing the max value, which was NOT good for siege.
	if ( inSiegeWithClass == qfalse )
	{
		client->ps.ammo[AMMO_BLASTER] = 100; //ammoData[AMMO_BLASTER].max; //100 seems fair.
	}
//	client->ps.ammo[AMMO_POWERCELL] = ammoData[AMMO_POWERCELL].max;
//	client->ps.ammo[AMMO_FORCE] = ammoData[AMMO_FORCE].max;
//	client->ps.ammo[AMMO_METAL_BOLTS] = ammoData[AMMO_METAL_BOLTS].max;
//	client->ps.ammo[AMMO_ROCKETS] = ammoData[AMMO_ROCKETS].max;
/*
	client->ps.stats[STAT_WEAPONS] = ( 1 << WP_BRYAR_PISTOL);
	if ( level.gametype == GT_TEAM ) {
		client->ps.ammo[WP_BRYAR_PISTOL] = 50;
	} else {
		client->ps.ammo[WP_BRYAR_PISTOL] = 100;
	}
*/
	client->ps.rocketLockIndex = ENTITYNUM_NONE;
	client->ps.rocketLockTime = 0;

	//rww - Set here to initialize the circling seeker drone to off.
	//A quick note about this so I don't forget how it works again:
	//ps.genericEnemyIndex is kept in sync between the server and client.
	//When it gets set then an entitystate value of the same name gets
	//set along with an entitystate flag in the shared bg code. Which
	//is why a value needs to be both on the player state and entity state.
	//(it doesn't seem to just carry over the entitystate value automatically
	//because entity state value is derived from player state data or some
	//such)
	client->ps.genericEnemyIndex = -1;

	client->ps.isJediMaster = qfalse;

	if (client->ps.fallingToDeath)
	{
		client->ps.fallingToDeath = 0;
		client->noCorpse = qtrue;
	}

	//Do per-spawn force power initialization
	WP_SpawnInitForcePowers( ent );

	// health will count down towards max_health
	if (level.gametype == GT_SIEGE &&
		client->siegeClass != -1 &&
		bgSiegeClasses[client->siegeClass].starthealth)
	{ //class specifies a start health, so use it
		ent->health = client->ps.stats[STAT_HEALTH] = bgSiegeClasses[client->siegeClass].starthealth;
	}
	else if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
	{//only start with 100 health in Duel
		if ( level.gametype == GT_POWERDUEL && client->sess.duelTeam == DUELTEAM_LONE )
		{
			if ( duel_fraglimit.integer )
			{

				ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] =
					g_powerDuelStartHealth.integer - ((g_powerDuelStartHealth.integer - g_powerDuelEndHealth.integer) * (float)client->sess.wins / (float)duel_fraglimit.integer);
			}
			else
			{
				ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = 150;
			}
		}
		else
		{
			ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = 100;
		}
	}
	else if (client->ps.stats[STAT_MAX_HEALTH] <= 100)
	{
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] * 1.25;
	}
	else if (client->ps.stats[STAT_MAX_HEALTH] < 125)
	{
		ent->health = client->ps.stats[STAT_HEALTH] = 125;
	}
	else
	{
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];
	}

	// Start with a small amount of armor as well.
	if (level.gametype == GT_SIEGE &&
		client->siegeClass != -1 /*&&
		bgSiegeClasses[client->siegeClass].startarmor*/)
	{ //class specifies a start armor amount, so use it
		client->ps.stats[STAT_ARMOR] = bgSiegeClasses[client->siegeClass].startarmor;
	}
	else if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
	{//no armor in duel
		client->ps.stats[STAT_ARMOR] = 0;
	}
	else
	{ // zyk: starting shield of the player
		client->ps.stats[STAT_ARMOR] = rp_starting_shield.integer;
	}

	G_SetOrigin( ent, spawn_origin );
	VectorCopy( spawn_origin, client->ps.origin );

	// GalaxyRP fix: [Quests] removed universe_quest_objective_control init here — field removed as dead (see g_local.h)

	// zyk: initializing mind control attributes on spawn time
	ent->client->pers.being_mind_controlled = -1;
	ent->client->pers.mind_controlled1_id = -1;

	// zyk: initializing flame thrower timer
	ent->client->pers.flame_thrower = 0;

	// zyk: No Attack ability timer
	ent->client->pers.no_attack_timer = 0;

	// zyk: now we use this attribute as the real fuel and then scale and set it to the jetpackFuel above
	client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
	client->jetPackToggleTime = level.time;

	// zyk: if player is logged at spawn, load his skills
	if (ent->client->sess.amrpgmode == 2)
	{
		// GalaxyRP fix: [Guardian] clean_guardians() call removed - function was a permanent no-op

		initialize_rpg_skills(ent);

		// GalaxyRP fix: [Quests] quest_get_new_player was removed as unreachable dead code (see the
		// GalaxyRP fix comment on its old location in g_cmds.c).
	}
	else if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_FORCE))
	{ // zyk: player received force powers from admin
		zyk_remove_guns(ent);
		zyk_add_force_powers(ent);
	}
	else if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_GUNS))
	{ // zyk: player received guns from admin
		zyk_remove_force_powers(ent);
		zyk_add_guns(ent);
	}

	// GalaxyRP fix: [Account] this amrpgmode == 1 branch was unreachable dead code -- amrpgmode has
	// not been assignable as 1 anywhere in the codebase since commit 5d35b28b8 (2022) made login
	// always set it to 2, and for that == 2 case zyk_load_common_settings() is already called via
	// initialize_rpg_skills() a few lines above, so nothing here was actually missing.

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SCALED))
	{ // zyk: player is scaled, set the scale factor
		do_scale(ent, ent->client->pers.player_scale);
	}

	// GalaxyRP fix: [gameplay/exploit] clear the chat-protection flag alongside its timer. Resetting
	// only the timer left the PLAYER_STATUS_CHAT_PROTECTION bit -- the flag G_Damage checks to skip all damage -- set
	// on a player who respawned while chat-protected, and with the timer back at 0 the clear branch in
	// ClientThink_real() could never fire for them again, so they stayed permanently invulnerable
	// while fully able to fight. See the matching comment on that block in g_active.c.
	ent->client->pers.chat_protection_timer = 0;
	ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_CHAT_PROTECTION);

	// the respawned flag will be cleared after the attack and jump keys come up
	client->ps.pm_flags |= PMF_RESPAWNED;

	trap->GetUsercmd( client - level.clients, &ent->client->pers.cmd );
	SetClientViewAngle( ent, spawn_angles );
	// don't allow full run speed for a bit
	client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	client->ps.pm_time = 100;

	client->respawnTime = level.time;
	client->inactivityTime = level.time + g_inactivity.integer * 1000;
	client->latched_buttons = 0;

	if (!level.intermissiontime) {
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
			G_KillBox(ent);
			// force the base weapon up
			//client->ps.weapon = WP_BRYAR_PISTOL;
			//client->ps.weaponstate = FIRST_WEAPON;
			if (client->ps.weapon <= WP_NONE)
			{
				client->ps.weapon = WP_BRYAR_PISTOL;
			}

			client->ps.torsoTimer = client->ps.legsTimer = 0;

			if (client->ps.weapon == WP_SABER)
			{
				G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_STAND1TO2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS, 0);
			}
			else
			{
				G_SetAnim(ent, NULL, SETANIM_TORSO, TORSO_RAISEWEAP1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS, 0);
				client->ps.legsAnim = WeaponReadyAnim[client->ps.weapon];
			}
			client->ps.weaponstate = WEAPON_RAISING;
			client->ps.weaponTime = client->ps.torsoTimer;

			if (g_spawnInvulnerability.integer)
			{
				ent->client->ps.eFlags |= EF_INVULNERABLE;
				ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
			}

			// fire the targets of the spawn point
			G_UseTargets(spawnPoint, ent);

			// GalaxyRP: [SP Maps] on a single-player map (JO and JA) the spawn points' target is the
			// map's start script (player setup, intro, cinematic), copied onto every added spawn
			// point. It is meant to run once, so the first spawn that fires it clears it from
			// every deathmatch spawn point on the map; later spawns fire nothing.
			if ( level.rp_spawn_target_once && spawnPoint && spawnPoint->target )
			{
				gentity_t *spot = NULL;

				while ( (spot = G_Find( spot, FOFS(classname), "info_player_deathmatch" )) != NULL )
				{
					spot->target = NULL;
				}
				spawnPoint->target = NULL;
			}

			// positively link the client, even if the command times are weird
			VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);

			tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_IN);
			tent->s.clientNum = ent->s.clientNum;

			trap->LinkEntity ((sharedEntity_t *)ent);

			// zyk: if player is paralyzed by an admin, keeps him that way
			if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))
			{
				// GalaxyRP fix: [Death System] and keeps him untargetable with it. The downed state
				// itself survives a respawn -- the PLAYER_STATUS_DOWNED and ADMIN_PARALYSIS bits and pers.downedTime all
				// live in pers, which ClientSpawn() preserves wholesale -- but FL_NOTARGET does not:
				// it sits on ent->flags, which this function clears outright with "ent->flags = 0"
				// further up. So a downed player who respawned (a team change is the reachable route,
				// but rcon's forceteam and the duel-queue rotation reach ClientSpawn() the same way)
				// came back still downed, still on the clock, and alone among downed players in being
				// shootable by NPCs and turrets again. RP_EnterDownedState() (g_cmds.c) sets this flag
				// when the state begins; this block is where the rest of the state's presentation is
				// already re-applied after a respawn, so the flag belongs here with it.
				ent->flags |= FL_NOTARGET;

				// GalaxyRP fix: [Death System] and the ADMIN paralysis invulnerability, for the same
				// reason FL_NOTARGET is re-applied above: it lives on the client rather than in pers,
				// and the g_spawnInvulnerability block a few lines up has just overwritten
				// invulnerableTimer with its own much shorter value. Without this a paralyzed player
				// pushed through a respawn (rcon's forceteam and the duel-queue rotation both reach
				// ClientSpawn() directly) would serve the rest of their punishment killable, which is
				// the one thing RP_EnterDownedState() sets the flag to prevent. pers.downedTime is the
				// authoritative remaining time and survives the respawn alongside the status bits, so
				// the window is rebuilt from it; the trailing second is the same slack
				// RP_EnterDownedState() uses, and RP_ClearDownedState() still ends it at the release.
				if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADMIN_PARALYSIS))
				{
					ent->client->invulnerableTimer = level.time + (ent->client->pers.downedTime * 1000) + 3000;
					ent->client->ps.eFlags |= EF_INVULNERABLE;
				}

				ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
				ent->client->ps.forceHandExtendTime = level.time + 500;
				ent->client->ps.velocity[2] += 150;
				ent->client->ps.forceDodgeAnim = 0;
				ent->client->ps.quickerGetup = qtrue;
			}
		}
	} else {
		// move players to intermission
		MoveClientToIntermission(ent);
	}

	//set teams for NPCs to recognize
	if (level.gametype == GT_SIEGE)
	{ //Imperial (team1) team is allied with "enemy" NPCs in this mode
		if (client->sess.sessionTeam == SIEGETEAM_TEAM1)
		{
			client->playerTeam = ent->s.teamowner = NPCTEAM_ENEMY;
			client->enemyTeam = NPCTEAM_PLAYER;
		}
		else
		{
			client->playerTeam = ent->s.teamowner = NPCTEAM_PLAYER;
			client->enemyTeam = NPCTEAM_ENEMY;
		}
	}
	else
	{
		client->playerTeam = ent->s.teamowner = NPCTEAM_PLAYER;
		client->enemyTeam = NPCTEAM_ENEMY;
	}

	/*
	//scaling for the power duel opponent
	if (level.gametype == GT_POWERDUEL &&
		client->sess.duelTeam == DUELTEAM_LONE)
	{
		client->ps.iModelScale = 125;
		VectorSet(ent->modelScale, 1.25f, 1.25f, 1.25f);
	}
	*/
	//Disabled. At least for now. Not sure if I'll want to do it or not eventually.

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	client->ps.commandTime = level.time - 100;
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink( ent-g_entities, NULL );

	// run the presend to set anything else, follow spectators wait
	// until all clients have been reconnected after map_restart
	if ( ent->client->sess.spectatorState != SPECTATOR_FOLLOW )
		ClientEndFrame( ent );

	// clear entity state values
	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
	// DAJ_RP: [Phase] /admhidden -- the copy above rebuilt s from ps
	RP_HiddenApplyState( ent, &ent->s );

	//rww - make sure client has a valid icarus instance
	trap->ICARUS_FreeEnt( (sharedEntity_t *)ent );
	trap->ICARUS_InitEnt( (sharedEntity_t *)ent );

	if (ent->client->sess.loggedin == qtrue) {

		sqlite3* db;
		char* zErrMsg = 0;
		int rc;
		sqlite3_stmt* stmt = NULL;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return;
		}
		select_weapons_table_row_from_entity(ent, db, zErrMsg, rc, stmt);
		sqlite3_close(db);
	}

}


/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap->DropClient(), which will call this and do
server system housekeeping.
============
*/
extern void G_LeaveVehicle( gentity_t* ent, qboolean ConCheck );

void G_ClearVote( gentity_t *ent ) {
	if ( level.voteTime ) {
		if ( ent->client->mGameFlags & PSG_VOTED ) {
			if ( ent->client->pers.vote == 1 ) {
				level.voteYes--;
				trap->SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
			}
			else if ( ent->client->pers.vote == 2 ) {
				level.voteNo--;
				trap->SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
			}
		}
		ent->client->mGameFlags &= ~(PSG_VOTED);
		ent->client->pers.vote = 0;
	}
}
void G_ClearTeamVote( gentity_t *ent, int team ) {
	int voteteam;

		 if ( team == TEAM_RED )	voteteam = 0;
	else if ( team == TEAM_BLUE )	voteteam = 1;
	else							return;

	if ( level.teamVoteTime[voteteam] ) {
		// GalaxyRP fix: [Vote] "+ voteteam" on both configstrings. The Red and Blue tallies sit side by side
		// (CS_TEAMVOTE_YES + 0 / + 1), and every other writer -- Cmd_CallTeamVote_f(), Cmd_TeamVote_f() --
		// adds the offset; this one did not, so a Blue voter leaving the team or the server overwrote RED's
		// on-screen count with Blue's number and left Blue's showing the vote that had just been removed.
		// Display only: the real tally is level.teamVoteYes/No, which was always updated correctly.
		if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
			if ( ent->client->pers.teamvote == 1 ) {
				level.teamVoteYes[voteteam]--;
				trap->SetConfigstring( CS_TEAMVOTE_YES + voteteam, va( "%i", level.teamVoteYes[voteteam] ) );
			}
			else if ( ent->client->pers.teamvote == 2 ) {
				level.teamVoteNo[voteteam]--;
				trap->SetConfigstring( CS_TEAMVOTE_NO + voteteam, va( "%i", level.teamVoteNo[voteteam] ) );
			}
		}
		ent->client->mGameFlags &= ~(PSG_TEAMVOTED);
		ent->client->pers.teamvote = 0;
	}
}

// GalaxyRP: [Account] forward-declared so ClientDisconnect (below) can flush the currently active
// character before this client's slot is torn down -- same pattern already used in g_cmds.c for
// Cmd_Char_f, whose own comment goes into the full history of why this matters.
extern void save_account(gentity_t *ent, qboolean save_char_file);
void ClientDisconnect( int clientNum ) {
	gentity_t	*ent;
	gentity_t	*tent;
	int			i;

	// cleanup if we are kicking a bot that
	// hasn't spawned yet
	G_RemoveQueuedBotBegin( clientNum );

	ent = g_entities + clientNum;
	if ( !ent->client || ent->client->pers.connected == CON_DISCONNECTED ) {
		return;
	}

	// GalaxyRP: [SP Maps] security / goodie keys leave with the player
	RP_ClearKeys( ent );

	// GalaxyRP: [Entity System] and so does an entity held with /entcopy or /entcut: a copy is not made,
	// a cut entity goes back where it was (g_entgrab.c)
	RP_EntGrabCancel( ent, qfalse );

	// GalaxyRP: [Account] this is the server-side handler for every kind of disconnect -- a
	// voluntary /quit or /disconnect, a /reconnect (which drops and reconnects to the same server,
	// running through this exact same path), a connection timeout, or a kick/ban -- and it never
	// called save_account() before. Whatever RPG progress this player had made since the last
	// incidental save trigger (a purchase, a /settings toggle, a race win, a /char switch, etc. --
	// there is no periodic autosave in this codebase) was silently discarded the moment their slot
	// was reused: ClientConnect() unconditionally memset()s the whole client struct on the very
	// next connection into this slot. Called here, first, while pers.connected is still
	// CON_CONNECTED and every pers/sess field this player actually has is still intact -- save_account()
	// itself is a no-op unless sess.amrpgmode == 2, so this is harmless for anyone not actually in
	// RPG mode.
	save_account(ent, qtrue);

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}
		i++;
	}

	i = TRACK_CHANNEL_1;

	while (i < NUM_TRACK_CHANNELS)
	{
		if (ent->client->ps.fd.killSoundEntIndex[i-50] && ent->client->ps.fd.killSoundEntIndex[i-50] < MAX_GENTITIES && ent->client->ps.fd.killSoundEntIndex[i-50] > 0)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[i-50], CHAN_VOICE);
		}
		i++;
	}
	i = 0;

	G_LeaveVehicle( ent, qtrue );

	// GalaxyRP: [Grapple Hook] the slot is about to be recycled; the hook must not outlive its owner.
	if ( ent->client->hook )
	{
		Weapon_HookFree( ent->client->hook );
	}

	if ( ent->client->ewebIndex )
	{
		gentity_t *eweb = &g_entities[ent->client->ewebIndex];

		ent->client->ps.emplacedIndex = 0;
		ent->client->ewebIndex = 0;
		ent->client->ewebHealth = 0;
		G_FreeEntity( eweb );
	}

	// stop any following clients
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].sess.sessionTeam == TEAM_SPECTATOR
			&& level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW
			&& level.clients[i].sess.spectatorClient == clientNum ) {
			StopFollowing( &g_entities[i] );
		}
	}

	// send effect if they were completely connected
	if ( ent->client->pers.connected == CON_CONNECTED
		&& ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = ent->s.clientNum;

		// They don't get to take powerups with them!
		// Especially important for stuff like CTF flags
		//
		// GalaxyRP fix: [Drop] ...but a corpse has already given them up, and this block tested only
		// "connected" and "not a spectator", so a player who died and disconnected before respawning
		// ran TossClientItems() a second time.
		//
		// Stated honestly: that second run drops NOTHING today, and this guard is belt and braces
		// rather than a repair. It is safe only by two side effects of where player_die() happens to
		// put things, both of them BELOW its own call to the same function -- s.weapon becomes
		// WP_NONE, which fails the weapon test at the top of the toss, and the powerups array is
		// memset to zero, which empties the loop underneath it. Neither line is there for this
		// reason, and either could move. The guard says the invariant out loud instead of leaving it
		// resting on that, and costs one comparison on a path that runs once per disconnect.
		//
		// ps.pm_type rather than health: player_die() sets PM_DEAD itself, so this is true for every
		// corpse however it got there, including the direct player->die() calls in NPC_spawn.c that
		// never pass through G_Damage and can leave health above zero. A DOWNED player is explicitly
		// not PM_DEAD -- ClientThink_real()'s pm_type ladder suppresses it while bit 6 is set -- so
		// one who disconnects still drops, which is what we want. Spectators are already excluded
		// above.
		//
		// The test could not live inside TossClientItems() in any case: player_die() sets PM_DEAD
		// well before it calls that function, so a guard there would cancel the ordinary death drop.
		if ( ent->client->ps.pm_type != PM_DEAD )
		{
			TossClientItems( ent );
		}
	}

	G_LogPrintf( "ClientDisconnect: %i [%s] (%s) \"%s^7\"\n", clientNum, ent->client->sess.IP, ent->client->pers.guid, ent->client->pers.netname );

	// if we are playing in tourney mode, give a win to the other player and clear his frags for this round
	if ( level.gametype == GT_DUEL && !level.intermissiontime && !level.warmupTime ) {
		if ( level.sortedClients[1] == clientNum ) {
			level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] = 0;
			level.clients[ level.sortedClients[0] ].sess.wins++;
			ClientUserinfoChanged( level.sortedClients[0] );
		}
		else if ( level.sortedClients[0] == clientNum ) {
			level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] = 0;
			level.clients[ level.sortedClients[1] ].sess.wins++;
			ClientUserinfoChanged( level.sortedClients[1] );
		}
	}

	if ( level.gametype == GT_DUEL && ent->client->sess.sessionTeam == TEAM_FREE && level.intermissiontime ) {
		trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
		level.restarted = qtrue;
		level.changemap = NULL;
		level.intermissiontime = 0;
	}

	if (ent->ghoul2 && trap->G2API_HaveWeGhoul2Models(ent->ghoul2))
	{
		trap->G2API_CleanGhoul2Models(&ent->ghoul2);
	}
	i = 0;
	while (i < MAX_SABERS)
	{
		if (ent->client->weaponGhoul2[i] && trap->G2API_HaveWeGhoul2Models(ent->client->weaponGhoul2[i]))
		{
			trap->G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[i]);
		}
		i++;
	}

	G_ClearVote( ent );
	G_ClearTeamVote( ent, ent->client->sess.sessionTeam );

	// zyk: cleaning the ally ids
	ent->client->sess.ally1 = 0;
	ent->client->sess.ally2 = 0;

	// zyk: removing ignored players
	level.ignored_players[ent->s.number][0] = 0;
	level.ignored_players[ent->s.number][1] = 0;

	// GalaxyRP fix: [Chat] also clear this player's bit from everyone ELSE's list. Only their own
	// row was cleared, but the list is keyed by client slot and slots are recycled: if A had
	// /ignore'd B in slot 7, B left and C connected into slot 7, A silently ignored C without ever
	// asking to. Clearing the column costs one pass over the client slots, once per disconnect.
	//
	// The trade is that an ignore no longer survives the ignored player reconnecting, which is the
	// right way round -- forgetting is recoverable with one command, inheriting is not noticeable.
	for ( i = 0; i < level.maxclients; i++ )
	{
		if ( ent->s.number < 31 )
			level.ignored_players[i][0] &= ~(1 << ent->s.number);
		else
			level.ignored_players[i][1] &= ~(1 << (ent->s.number - 31));
	}

	// zyk: duelist disconnected. Reset him from duels
	// GalaxyRP: [Force Duel] the private-duel type goes with them. It is only ever read while
	// ps.duelInProgress is set and every path that sets that writes it first, so a stale value is
	// unreachable -- but a slot handed to a new player should not carry the last one's state.
	level.duel_types[ent->s.number] = 0;

	if (level.duel_players[ent->s.number] > -1)
	{
		level.duel_players[ent->s.number] = -1;
		level.duelists_quantity--;
	}

	// GalaxyRP fix: [Duel Tournament] end the tournament when this was the last duelist, exactly as
	// Cmd_DuelMode_f's leave branch already does. Dropping the count without ending it left
	// duel_tournament_mode at 1 and level.duel_tournament_model_id still naming the globe, and the
	// join branch spawns a fresh globe whenever duelists_quantity is 0 -- so the next player to sign
	// up inside the remaining signup window overwrote that id and orphaned the old entity, which
	// nothing then frees. Repeatable at will (sign up, spectate, rejoin, sign up), one leaked entity
	// slot and a few never-freed G_Alloc strings per lap, ending at G_Spawn()'s trap->Error(ERR_DROP)
	// -- a process exit on a dedicated build.
	//
	// Scoped to signup because that is the only mode the leak exists in: the join branch refuses at
	// duel_tournament_mode > 1, so no second globe can be spawned once the tournament starts, and the
	// mode-2 block in G_RunFrame() (g_main.c) already ends an emptied tournament there with its own
	// announcement. Ending early here would silence that message for no gain.
	//
	// GalaxyRP fix: [Duel Tournament] the "or paused" half. The reasoning above holds only while the
	// tournament is running: the mode-2 block that ends an emptied tournament lives inside
	// G_RunFrame()'s "not paused" wrapper, so while an admin has /duelpause on, nothing ends one at
	// all. Everyone leaving a paused mode-2-or-later tournament left it sitting at that mode with
	// duelists_quantity at 0 and the paused flag still set, which also refuses every new sign-up
	// (the join branch stops at duel_tournament_mode > 1) -- so that map had no duel tournament
	// available until somebody resumed or the map changed. Now that pausing genuinely freezes the
	// tournament clock, that state persists instead of resolving the moment anyone resumes.
	//
	// This is the whole of the reachable surface: ClientBegin (this site -- re-entering the world,
	// which is how a team change including one to spectator gets here) and ClientDisconnect are the
	// only two routes out of the roster while paused at mode 2 or later, because Cmd_DuelMode_f
	// refuses to let anyone leave once the tournament has started and its own leave branch already
	// ends an emptied signup. A paused tournament cannot be at mode 0 either -- Cmd_DuelPause_f
	// refuses there, and duel_tournament_end() clears the flag -- so the added clause cannot fire
	// when no tournament exists.
	if (level.duelists_quantity <= 0 &&
		(level.duel_tournament_mode == 1 || level.duel_tournament_paused == qtrue))
	{
		duel_tournament_end();
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7There are no duelists anymore. Tournament is over!\"");
	}

	// zyk: melee battle player disconnected
	if (level.melee_players[ent->s.number] > -1)
	{
		level.melee_players[ent->s.number] = -1;
		level.melee_mode_quantity--;
	}

	// GalaxyRP fix: [Melee Battle] end the battle when this was the last player, exactly as
	// Cmd_MeleeMode_f's leave branch already does. Same defect and same shape as the Duel Tournament
	// one above: melee_mode stayed at 1 with level.melee_model_id still naming the catwalk, and the
	// join branch spawns a fresh catwalk whenever melee_mode_quantity is 0, so the next sign-up
	// inside the 12-second window orphaned it. The G_FreeEntity() hook that clears melee_model_id
	// (g_utils.c) does not help here -- nothing ever frees the old entity for it to fire on.
	//
	// Scoped to signup for the same reason: joining is refused at melee_mode > 1, and the mode-2
	// block in G_RunFrame() (g_main.c) already ends an emptied battle with "No players left!".
	if (level.melee_mode == 1 && level.melee_mode_quantity <= 0)
	{
		melee_battle_end();
		trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7No players left! Melee Battle is over!\"");
	}

	// zyk: cleaning ally ids of other players who have this player as ally
	//
	// GalaxyRP fix: [Ally] test the bit directly instead of going through zyk_is_ally(). That
	// helper requires ent->client->pers.connected == CON_CONNECTED, and this function sets that
	// field to CON_DISCONNECTED a few dozen lines further down -- so the clear worked only
	// because of the order those two statements happen to sit in. Anything that moved the
	// disconnect flag earlier would have left every other player holding an ally bit for a slot
	// the next person to connect would inherit, with nothing to show for it. The bit test and the
	// clear now both go through the ally helpers, which know the ally1/ally2 layout.
	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (!player_ent->client)
			continue;

		if (zyk_ally_bit_set(player_ent, ent->s.number) == qfalse)
			continue;

		zyk_remove_ally(player_ent, ent->s.number);

		// zyk: sending the updated ally set to the client-side radar
		// GalaxyRP fix: [Radar] was G_AddEvent(player_ent, EV_USE_ITEM14, ...), one bit of a per-slot
		// client cache. send_ally_update() pushes the whole set reliably instead.
		send_ally_update(player_ent);
	}

	// GalaxyRP fix: [NPC] release any NPC that was following this player.
	//
	// client->leader is a raw gentity_t *, and nothing cleared it when the leader left: not this
	// function, not player_die(), and not G_FreeEntity(), which never clears inbound references.
	// g_entities is a static array, so the stale pointer always addressed valid memory -- this was
	// never a crash -- but it went wrong three ways:
	//
	//  1. The NPC was orphaned for good. TryUse() only claims an NPC when leader is NULL and only
	//     releases one when leader is the player pressing Use, so a non-NULL pointer to a departed
	//     player matched neither branch: nobody could take the NPC and nobody could free it. It
	//     kept walking to leader->r.currentOrigin, the position the player left from.
	//
	//  2. Client slots are recycled, so the next person to connect into this slot inherited the
	//     NPCs -- and /order commanded them, since it matches on leader == ent. Spawning needs
	//     ADM_NPC, so what transferred was typically an admin's escort.
	//
	//  3. Worst of the three: an NPC under /order guard or /order cover picks its targets in
	//     NPC_ValidEnemy() by asking zyk_is_ally(leader, ent) -- its leader's ally list. The loop
	//     above has just zeroed that list, so every same-team player became a valid enemy, and
	//     once the slot was reused the newcomer's ally list silently decided who the NPC shot.
	//
	// Releasing is exactly what TryUse() does when the leader presses Use again -- clear the two
	// order bits, drop the leader, stand guard -- so the NPC stays in the world and can be claimed
	// by anyone, and no behaviour here is new. Scans from MAX_CLIENTS like Cmd_Order_f(), since
	// NPCs never occupy a client slot.
	//
	// GalaxyRP fix: [NPC] the same sweep now also drops this player as an NPC's enemy, which dangled
	// in exactly the way leader did and for the same reason: nothing here, in player_die() or in
	// G_FreeEntity() ever clears an inbound reference, and health is not zeroed on the way out, so a
	// player who quit while alive stayed a live, valid-looking target.
	//
	// NPC_ExecuteBState() does carry a `if ( !NPC->enemy->inuse ) G_ClearEnemy()` net, but it is not
	// enough on its own. It runs after NPC_RunBehavior(), so the bState gets one full pass against
	// the departed player first -- firing at the origin they left from, and in NPC_BSFollowLeader()
	// handing that same pointer to other NPCs as their enemy through the leader->enemy branch. And
	// NPC_Think() returns before ever reaching it when the NPC is dead, ICARUS-frozen, riding or
	// driving a vehicle, or simply has not hit nextBStateThink yet, so for those the stale pointer
	// survives until they think again -- indefinitely, in the frozen and vehicle cases. Since client
	// slots are recycled, that window is the whole bug: whoever connects into this slot next inherits
	// being shot at, having done nothing.
	//
	// G_ClearEnemy() rather than a bare assignment, because it also drops the lookTarget and the move
	// goal when they point at the same entity. lastEnemy is only ever compared, never dereferenced,
	// but it is cleared too so that a recycled slot is not mistaken for "the enemy I just fought".
	for (i = MAX_CLIENTS; i < level.num_entities; i++)
	{
		gentity_t *npc_ent = &g_entities[i];

		if (!npc_ent->inuse || !npc_ent->client || !npc_ent->NPC)
			continue;

		if (npc_ent->enemy == ent)
		{
			G_ClearEnemy(npc_ent);
		}

		if (npc_ent->lastEnemy == ent)
		{
			npc_ent->lastEnemy = NULL;
		}

		// Charmed NPCs are released here as well, unlike the team-change path in
		// zyk_release_player_npcs(): NPC_CheckCharmed() would not clear the leader until charmedTime
		// expires, and this pointer cannot be left standing for that long.
		if (npc_ent->client->leader == ent)
		{
			zyk_release_npc_from_leader(npc_ent);
		}
	}

	// GalaxyRP fix: [NPC] do the same for the other players' enemy pointers, which reach NPCs by a
	// second route.
	//
	// player_die() sets self->enemy to whoever killed you (g_combat.c) and G_Damage() sets it to
	// whoever last hurt you; nothing ever clears either, and ClientSpawn() does not reset them on the
	// way back in. An escort NPC reads its leader's enemy directly in NPC_BSFollowLeader() and adopts
	// it with G_SetEnemy(), a path that never consults NPC_ValidEnemy(). G_SetEnemy() does refuse an
	// entity with inuse == 0, so a freed slot is safe -- but the moment the slot is recycled the
	// pointer is live again, and the escort opens fire on whoever just connected, for being killed by
	// someone who has already left. Clearing the inbound references here removes the whole sequence.
	//
	// A plain assignment rather than G_ClearEnemy(): that helper exists to keep an NPC's lookTarget
	// and move goal in step with its enemy, and these are players, with neither.
	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (!player_ent->inuse || !player_ent->client)
			continue;

		if (player_ent->enemy == ent)
		{
			player_ent->enemy = NULL;
		}

		if (player_ent->lastEnemy == ent)
		{
			player_ent->lastEnemy = NULL;
		}
	}

	// And this player's own outbound pointers, unconditionally: the slot is about to be handed to
	// the next person who connects, and ClientSpawn() would not clear these for them. Left alone,
	// the newcomer inherits a grudge against whoever killed the previous occupant -- and hands it
	// to their own escort NPCs the first time they claim one.
	ent->enemy = NULL;
	ent->lastEnemy = NULL;

	// zyk: logout player from account
	ent->client->sess.amrpgmode = 0;

	// GalaxyRP fix: [Guardian] guardian_mode reset removed - field is permanently 0, sole writer spawn_boss() had zero callers

	// GalaxyRP fix: [Guardian] removed the `if (can_play_quest == 1) { boss_battle_music_reset_timer
	// = ...; }` block here -- can_play_quest can no longer become 1 anywhere (see the GalaxyRP fix
	// comment on quest_get_new_player's old location in g_cmds.c), and boss_battle_music_reset_timer
	// itself has now been removed as dead (see g_local.h).

	trap->UnlinkEntity ((sharedEntity_t *)ent);
	ent->s.modelindex = 0;
	ent->inuse = qfalse;
	ent->classname = "disconnected";
	ent->client->pers.connected = CON_DISCONNECTED;
	ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
	ent->client->sess.sessionTeam = TEAM_FREE;
	ent->r.contents = 0;

	// GalaxyRP fix: [Quests] removed universe_quest_objective_control reset here — field removed as dead (see g_local.h)

	// zyk: if this player is being mind controlled, stops mind control on him
	if (ent->client->pers.being_mind_controlled != -1)
	{
		gentity_t *mind_controller = &g_entities[ent->client->pers.being_mind_controlled];

		if (mind_controller->client->pers.mind_controlled1_id == (ent-g_entities))
		{
			mind_controller->client->pers.mind_controlled1_id = -1;
		}

		ent->client->pers.being_mind_controlled = -1;
	}

	// GalaxyRP fix: [Quests] a "if this was the target player, choose another target" guard used to
	// live here -- removed for the same reason as the equivalent guard in ClientBegin above
	// (level.bounty_quest_choose_target/target_id no longer exist).

	ent->client->pers.bitvalue = 0;
	ent->client->pers.player_statuses = 0;

	if (ent->client->holdingObjectiveItem > 0)
	{ //carrying a siege objective item - make sure it updates and removes itself from us now in case this is an instant death-respawn situation
		gentity_t *objectiveItem = &g_entities[ent->client->holdingObjectiveItem];

		if (objectiveItem->inuse && objectiveItem->think)
		{
            objectiveItem->think(objectiveItem);
		}
	}

	trap->SetConfigstring( CS_PLAYERS + clientNum, "");

	CalculateRanks();

	if ( ent->r.svFlags & SVF_BOT ) {
		BotAIShutdownClient( clientNum, qfalse );
	}

	G_ClearClientLog(clientNum);
}


