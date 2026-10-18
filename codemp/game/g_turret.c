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
#include "qcommon/q_shared.h"

void G_SetEnemy( gentity_t *self, gentity_t *enemy );
qboolean turret_base_spawn_top( gentity_t *base );
void ObjectDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );

//------------------------------------------------------------------------------------------------------------
void TurretPain( gentity_t *self, gentity_t *attacker, int damage )
//------------------------------------------------------------------------------------------------------------
{
	if (self->target_ent)
	{
		self->target_ent->health = self->health;
		if (self->target_ent->maxHealth)
		{
			G_ScaleNetHealth(self->target_ent);
		}
	}

	if ( attacker->client && attacker->client->ps.weapon == WP_DEMP2 )
	{
		self->attackDebounceTime = level.time + 800 + Q_flrand(0.0f, 1.0f) * 500;
		self->painDebounceTime = self->attackDebounceTime;
	}
	if ( !self->enemy )
	{//react to being hit
		G_SetEnemy( self, attacker );
	}
}

//------------------------------------------------------------------------------------------------------------
void TurretBasePain( gentity_t *self, gentity_t *attacker, int damage )
//------------------------------------------------------------------------------------------------------------
{
	if (self->target_ent)
	{
		self->target_ent->health = self->health;
		if (self->target_ent->maxHealth)
		{
			G_ScaleNetHealth(self->target_ent);
		}

		TurretPain(self->target_ent, attacker, damage);
	}
}

//------------------------------------------------------------------------------------------------------------
void auto_turret_die ( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
//------------------------------------------------------------------------------------------------------------
{
	vec3_t	forward = { 0,0, 1 }, pos;

	// Turn off the thinking of the base & use it's targets
	g_entities[self->r.ownerNum].think = NULL;
	g_entities[self->r.ownerNum].use = NULL;

	// clear my data
	self->die = NULL;
	self->takedamage = qfalse;
	self->s.health = self->health = 0;
	self->s.loopSound = 0;
	self->s.shouldtarget = qfalse;
	//self->s.owner = MAX_CLIENTS; //not owned by any client

	VectorCopy( self->r.currentOrigin, pos );
	pos[2] += self->r.maxs[2]*0.5f;
	G_PlayEffect( EFFECT_EXPLOSION_TURRET, pos, forward );
	G_PlayEffectID( G_EffectIndex( "turret/explode" ), pos, forward );

	if ( self->splashDamage > 0 && self->splashRadius > 0 )
	{
		G_RadiusDamage( self->r.currentOrigin,
						attacker,
						self->splashDamage,
						self->splashRadius,
						attacker,
						NULL,
						MOD_UNKNOWN );
	}

	self->s.weapon = 0; // crosshair code uses this to mark crosshair red


	if ( self->s.modelindex2 )
	{
		// switch to damage model if we should
		self->s.modelindex = self->s.modelindex2;

		if (self->target_ent && self->target_ent->s.modelindex2)
		{
			self->target_ent->s.modelindex = self->target_ent->s.modelindex2;
		}

		VectorCopy( self->r.currentAngles, self->s.apos.trBase );
		VectorClear( self->s.apos.trDelta );

		if ( self->target )
		{
			G_UseTargets( self, attacker );
		}
	}
	else
	{
		ObjectDie( self, inflictor, attacker, damage, meansOfDeath );
	}
}

//------------------------------------------------------------------------------------------------------------
void bottom_die ( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
//------------------------------------------------------------------------------------------------------------
{
	if (self->target_ent && self->target_ent->health > 0)
	{
		self->target_ent->health = self->health;
		if (self->target_ent->maxHealth)
		{
			G_ScaleNetHealth(self->target_ent);
		}
		auto_turret_die(self->target_ent, inflictor, attacker, damage, meansOfDeath);
	}
}

#define START_DIS 15

//----------------------------------------------------------------
static void turret_fire ( gentity_t *ent, vec3_t start, vec3_t dir )
//----------------------------------------------------------------
{
	vec3_t		org;
	gentity_t	*bolt;

	if ( (trap->PointContents( start, ent->s.number )&MASK_SHOT) )
	{
		return;
	}

	// GalaxyRP fix: [Entity System] a bolt is an entity that lives 10 seconds; with the table at the
	// reserve the shot is skipped rather than walked into G_Spawn()'s ERR_DROP (see g_utils.c)
	if ( G_EntitySlotsAvailable( 1 ) == qfalse )
	{
		return;
	}

	VectorMA( start, -START_DIS, dir, org ); // dumb....
	G_PlayEffectID( ent->genericValue13, org, dir );

	bolt = G_Spawn();

	//use a custom shot effect
	bolt->s.otherEntityNum2 = ent->genericValue14;
	//use a custom impact effect
	bolt->s.emplacedOwner = ent->genericValue15;

	bolt->classname = "turret_proj";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_FreeEntity;
	bolt->s.eType = ET_MISSILE;
	bolt->s.weapon = WP_EMPLACED_GUN;
	bolt->r.ownerNum = ent->s.number;
	bolt->damage = ent->damage;
	bolt->alliedTeam = ent->alliedTeam;
	bolt->teamnodmg = ent->teamnodmg;
	//bolt->dflags = DAMAGE_NO_KNOCKBACK;// | DAMAGE_HEAVY_WEAP_CLASS;		// Don't push them around, or else we are constantly re-aiming
	bolt->splashDamage = ent->damage;
	bolt->splashRadius = 100;
	bolt->methodOfDeath = MOD_TARGET_LASER;
	bolt->splashMethodOfDeath = MOD_TARGET_LASER;
	bolt->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	//bolt->trigger_formation = qfalse;		// don't draw tail on first frame

	VectorSet( bolt->r.maxs, 1.5, 1.5, 1.5 );
	VectorScale( bolt->r.maxs, -1, bolt->r.mins );
	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time;
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, ent->mass, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );		// save net bandwidth
	VectorCopy( start, bolt->r.currentOrigin);

	bolt->parent = ent;
}

//-----------------------------------------------------
void turret_head_think( gentity_t *self )
//-----------------------------------------------------
{
	gentity_t *top = &g_entities[self->r.ownerNum];
	if ( !top )
	{
		return;
	}
	if ( self->painDebounceTime > level.time )
	{
		vec3_t	v_up;
		VectorSet( v_up, 0, 0, 1 );
		G_PlayEffect( EFFECT_SPARKS, self->r.currentOrigin, v_up );
		if ( Q_irand( 0, 3) )
		{//25% chance of still firing
			return;
		}
	}
	// if it's time to fire and we have an enemy, then gun 'em down!  pushDebounce time controls next fire time
	if ( self->enemy && self->setTime < level.time && self->attackDebounceTime < level.time )
	{
		vec3_t		fwd, org;
		// set up our next fire time
		self->setTime = level.time + self->wait;

		/*
		mdxaBone_t	boltMatrix;

		// Getting the flash bolt here
		trap->G2API_GetBoltMatrix( self->ghoul2, self->playerModel,
					self->torsoBolt,
					&boltMatrix, self->r.currentAngles, self->r.currentOrigin, (cg.time?cg.time:level.time),
					NULL, self->s.modelScale );

		trap->G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, org );
		trap->G2API_GiveMeVectorFromMatrix( boltMatrix, POSITIVE_Y, fwd );
		*/
		VectorCopy( top->r.currentOrigin, org );
		org[2] += top->r.maxs[2]-8;
		AngleVectors( top->r.currentAngles, fwd, NULL, NULL );

		VectorMA( org, START_DIS, fwd, org );

		turret_fire( top, org, fwd );
		self->fly_sound_debounce_time = level.time;//used as lastShotTime
	}
}

//-----------------------------------------------------
static void turret_aim( gentity_t *self )
//-----------------------------------------------------
{
	vec3_t	enemyDir, org, org2;
	vec3_t	desiredAngles, setAngle;
	float	diffYaw = 0.0f, diffPitch = 0.0f, turnSpeed;
	const float pitchCap = 40.0f;
	gentity_t *top = &g_entities[self->r.ownerNum];
	if ( !top )
	{
		return;
	}

	// move our gun base yaw to where we should be at this time....
	BG_EvaluateTrajectory( &top->s.apos, level.time, top->r.currentAngles );
	top->r.currentAngles[YAW] = AngleNormalize180( top->r.currentAngles[YAW] );
	top->r.currentAngles[PITCH] = AngleNormalize180( top->r.currentAngles[PITCH] );
	turnSpeed = top->speed;

	if ( self->painDebounceTime > level.time )
	{
		desiredAngles[YAW] = top->r.currentAngles[YAW]+flrand(-45,45);
		desiredAngles[PITCH] = top->r.currentAngles[PITCH]+flrand(-10,10);

		if (desiredAngles[PITCH] < -pitchCap)
		{
			desiredAngles[PITCH] = -pitchCap;
		}
		else if (desiredAngles[PITCH] > pitchCap)
		{
			desiredAngles[PITCH] = pitchCap;
		}

		diffYaw = AngleSubtract( desiredAngles[YAW], top->r.currentAngles[YAW] );
		diffPitch = AngleSubtract( desiredAngles[PITCH], top->r.currentAngles[PITCH] );
		turnSpeed = flrand( -5, 5 );
	}
	else if ( self->enemy )
	{
		// ...then we'll calculate what new aim adjustments we should attempt to make this frame
		// Aim at enemy
		VectorCopy( self->enemy->r.currentOrigin, org );
		org[2]+=self->enemy->r.maxs[2]*0.5f;
		if (self->enemy->s.eType == ET_NPC &&
			self->enemy->s.NPC_class == CLASS_VEHICLE &&
			self->enemy->m_pVehicle &&
			self->enemy->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
		{ //hack!
			org[2] += 32.0f;
		}
		/*
		mdxaBone_t	boltMatrix;

		// Getting the "eye" here
		trap->G2API_GetBoltMatrix( self->ghoul2, self->playerModel,
					self->torsoBolt,
					&boltMatrix, self->r.currentAngles, self->s.origin, (cg.time?cg.time:level.time),
					NULL, self->s.modelScale );

		trap->G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, org2 );
		*/
		VectorCopy( top->r.currentOrigin, org2 );

		VectorSubtract( org, org2, enemyDir );
		vectoangles( enemyDir, desiredAngles );
		desiredAngles[PITCH] = AngleNormalize180(desiredAngles[PITCH]);

		if (desiredAngles[PITCH] < -pitchCap)
		{
			desiredAngles[PITCH] = -pitchCap;
		}
		else if (desiredAngles[PITCH] > pitchCap)
		{
			desiredAngles[PITCH] = pitchCap;
		}

		diffYaw = AngleSubtract( desiredAngles[YAW], top->r.currentAngles[YAW] );
		diffPitch = AngleSubtract( desiredAngles[PITCH], top->r.currentAngles[PITCH] );
	}
	else
	{//FIXME: Pan back and forth in original facing
		// no enemy, so make us slowly sweep back and forth as if searching for a new one
		desiredAngles[YAW] = sin( level.time * 0.0001f + top->count );
		desiredAngles[YAW] *=  60.0f;
		desiredAngles[YAW] += self->s.angles[YAW];
		desiredAngles[YAW] = AngleNormalize180( desiredAngles[YAW] );
		diffYaw = AngleSubtract( desiredAngles[YAW], top->r.currentAngles[YAW] );
		diffPitch = AngleSubtract( 0, top->r.currentAngles[PITCH] );
		turnSpeed = 1.0f;
	}

	if ( diffYaw )
	{
		// cap max speed....
		if ( fabs(diffYaw) > turnSpeed )
		{
			diffYaw = ( diffYaw >= 0 ? turnSpeed : -turnSpeed );
		}
	}
	if ( diffPitch )
	{
		if ( fabs(diffPitch) > turnSpeed )
		{
			// cap max speed
			diffPitch = (diffPitch > 0.0f ? turnSpeed : -turnSpeed );
		}
	}
	// ...then set up our desired yaw
	VectorSet( setAngle, diffPitch, diffYaw, 0 );

	VectorCopy( top->r.currentAngles, top->s.apos.trBase );
	VectorScale( setAngle, (1000/FRAMETIME), top->s.apos.trDelta );
	top->s.apos.trTime = level.time;
	top->s.apos.trType = TR_LINEAR_STOP;
	top->s.apos.trDuration = FRAMETIME;

	if ( diffYaw || diffPitch )
	{
		top->s.loopSound = G_SoundIndex( "sound/vehicles/weapons/hoth_turret/turn.wav" );
	}
	else
	{
		top->s.loopSound = 0;
	}
}

//-----------------------------------------------------
static void turret_turnoff( gentity_t *self )
//-----------------------------------------------------
{
	gentity_t *top = &g_entities[self->r.ownerNum];
	if ( top != NULL )
	{//still have a top
		//stop it from rotating
		VectorCopy( top->r.currentAngles, top->s.apos.trBase );
		VectorClear( top->s.apos.trDelta );
		top->s.apos.trTime = level.time;
		top->s.apos.trType = TR_STATIONARY;
	}

	self->s.loopSound = 0;
	// shut-down sound
	//G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/chars/turret/shutdown.wav" ));

	// Clear enemy
	self->enemy = NULL;
}

//-----------------------------------------------------
static void turret_sleep( gentity_t *self )
//-----------------------------------------------------
{
	if ( self->enemy == NULL )
	{
		// we don't need to play sound
		return;
	}

	// make turret play ping sound for 5 seconds
	self->aimDebounceTime = level.time + 5000;

	// Clear enemy
	self->enemy = NULL;
}

//-----------------------------------------------------
static qboolean turret_find_enemies( gentity_t *self )
//-----------------------------------------------------
{
	qboolean	found = qfalse;
	int			i, count;
	float		bestDist = self->radius * self->radius;
	float		enemyDist;
	vec3_t		enemyDir, org, org2;
	gentity_t	*entity_list[MAX_GENTITIES], *target, *bestTarget = NULL;
	trace_t		tr;
	gentity_t *top = &g_entities[self->r.ownerNum];
	if ( !top )
	{
		return qfalse;
	}

	if ( self->aimDebounceTime > level.time ) // time since we've been shut off
	{
		// We were active and alert, i.e. had an enemy in the last 3 secs
		if ( self->timestamp < level.time )
		{
			//G_Sound(self, CHAN_BODY, G_SoundIndex( "sound/chars/turret/ping.wav" ));
			self->timestamp = level.time + 1000;
		}
	}

	VectorCopy( top->r.currentOrigin, org2 );

	count = G_RadiusList( org2, self->radius, self, qtrue, entity_list );

	for ( i = 0; i < count; i++ )
	{
		target = entity_list[i];

		if ( !target->client )
		{
			// only attack clients
			continue;
		}
		if ( target == self || !target->takedamage || target->health <= 0 || ( target->flags & FL_NOTARGET ))
		{
			continue;
		}
		if ( target->client->sess.sessionTeam == TEAM_SPECTATOR )
		{
			continue;
		}
		if ( target->client->tempSpectate >= level.time )
		{
			continue;
		}
		if ( self->alliedTeam )
		{
			if ( target->client )
			{
				if ( target->client->sess.sessionTeam == self->alliedTeam )
				{
					// A bot/client/NPC we don't want to shoot
					continue;
				}
			}
			else if ( target->teamnodmg == self->alliedTeam )
			{
				// An ent we don't want to shoot
				continue;
			}
		}
		if ( !trap->InPVS( org2, target->r.currentOrigin ))
		{
			continue;
		}

		VectorCopy( target->r.currentOrigin, org );
		org[2] += target->r.maxs[2]*0.5f;

		trap->Trace( &tr, org2, NULL, NULL, org, self->s.number, MASK_SHOT, qfalse, 0, 0 );

		if ( !tr.allsolid && !tr.startsolid && ( tr.fraction == 1.0 || tr.entityNum == target->s.number ))
		{
			// Only acquire if have a clear shot, Is it in range and closer than our best?
			VectorSubtract( target->r.currentOrigin, top->r.currentOrigin, enemyDir );
			enemyDist = VectorLengthSquared( enemyDir );

			if ( enemyDist < bestDist // all things equal, keep current
				|| (!Q_stricmp( "atst_vehicle", target->NPC_type ) && bestTarget && Q_stricmp( "atst_vehicle", bestTarget->NPC_type ) ) )//target AT-STs over non-AT-STs... FIXME: must be a better, easier way to tell this, no?
			{
				if ( self->attackDebounceTime < level.time )
				{
					// We haven't fired or acquired an enemy in the last 2 seconds-start-up sound
					//G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/chars/turret/startup.wav" ));

					// Wind up turrets for a bit
					self->attackDebounceTime = level.time + 1400;
				}

				bestTarget = target;
				bestDist = enemyDist;
				found = qtrue;
			}
		}
	}

	if ( found )
	{
		G_SetEnemy( self, bestTarget );
		if ( VALIDSTRING( self->target2 ))
		{
			G_UseTargets2( self, self, self->target2 );
		}
	}

	return found;
}

//-----------------------------------------------------
void turret_base_think( gentity_t *self )
//-----------------------------------------------------
{
	qboolean	turnOff = qtrue;
	float		enemyDist;
	vec3_t		enemyDir, org, org2;

	if ( self->spawnflags & 1 )
	{
		// not turned on
		turret_turnoff( self );

		// No target
		self->flags |= FL_NOTARGET;
		self->nextthink = -1;//never think again
		return;
	}
	else
	{
		// I'm all hot and bothered
		self->flags &= ~FL_NOTARGET;
		//remember to keep thinking!
		self->nextthink = level.time + FRAMETIME;
	}

	if ( !self->enemy )
	{
		if ( turret_find_enemies( self ))
		{
			turnOff = qfalse;
		}
	}
	else if ( self->enemy->client && self->enemy->client->sess.sessionTeam == TEAM_SPECTATOR )
	{//don't keep going after spectators
		self->enemy = NULL;
	}
	else if ( self->enemy->client && self->enemy->client->tempSpectate >= level.time )
	{//don't keep going after spectators
		self->enemy = NULL;
	}
	else
	{//FIXME: remain single-minded or look for a new enemy every now and then?
		if ( self->enemy->health > 0 )
		{
			// enemy is alive
			VectorSubtract( self->enemy->r.currentOrigin, self->r.currentOrigin, enemyDir );
			enemyDist = VectorLengthSquared( enemyDir );

			if ( enemyDist < (self->radius * self->radius) )
			{
				// was in valid radius
				if ( trap->InPVS( self->r.currentOrigin, self->enemy->r.currentOrigin ) )
				{
					// Every now and again, check to see if we can even trace to the enemy
					trace_t tr;

					if ( self->enemy->client )
					{
						VectorCopy( self->enemy->client->renderInfo.eyePoint, org );
					}
					else
					{
						VectorCopy( self->enemy->r.currentOrigin, org );
					}
					VectorCopy( self->r.currentOrigin, org2 );
					if ( self->spawnflags & 2 )
					{
						org2[2] += 10;
					}
					else
					{
						org2[2] -= 10;
					}
					trap->Trace( &tr, org2, NULL, NULL, org, self->s.number, MASK_SHOT, qfalse, 0, 0 );

					if ( !tr.allsolid && !tr.startsolid && tr.entityNum == self->enemy->s.number )
					{
						turnOff = qfalse;	// Can see our enemy
					}
				}
			}
		}

		turret_head_think( self );
	}

	if ( turnOff )
	{
		if ( self->bounceCount < level.time ) // bounceCount is used to keep the thing from ping-ponging from on to off
		{
			turret_sleep( self );
		}
	}
	else
	{
		// keep our enemy for a minimum of 2 seconds from now
		self->bounceCount = level.time + 2000 + Q_flrand(0.0f, 1.0f) * 150;
	}

	turret_aim( self );
}

//-----------------------------------------------------------------------------
void turret_base_use( gentity_t *self, gentity_t *other, gentity_t *activator )
//-----------------------------------------------------------------------------
{
	// Toggle on and off
	self->spawnflags = (self->spawnflags ^ 1);

	/*
	if (( self->s.eFlags & EF_SHADER_ANIM ) && ( self->spawnflags & 1 )) // Start_Off
	{
		self->s.frame = 1; // black
	}
	else
	{
		self->s.frame = 0; // glow
	}
	*/
}


/*QUAKED misc_turret (1 0 0) (-48 -48 0) (48 48 144) START_OFF
Large 2-piece turbolaser turret

  START_OFF - Starts off

  radius - How far away an enemy can be for it to pick it up (default 1024)
  wait	- Time between shots (default 300 ms)
  dmg	- How much damage each shot does (default 100)
  health - How much damage it can take before exploding (default 3000)
  speed - how fast it turns (default 10)

  splashDamage - How much damage the explosion does (300)
  splashRadius - The radius of the explosion (128)

  shotspeed - speed at which projectiles will move

  targetname - Toggles it on/off
  target - What to use when destroyed
  target2 - What to use when it decides to start shooting at an enemy

  showhealth - set to 1 to show health bar on this entity when crosshair is over it

  teamowner - crosshair shows green for this team, red for opposite team
	0 - none
	1 - red
	2 - blue

  alliedTeam - team that this turret won't target
	0 - none
	1 - red
	2 - blue

  teamnodmg - team that turret does not take damage from
	0 - none
	1 - red
	2 - blue

"icon" - icon that represents the objective on the radar

GalaxyRP: [SP Maps] on a single-player map this classname is the small ceiling turret instead -- see
RP_SpawnSPTurret() just below.
*/

/*
------------------
RP_SpawnSPTurret

In single player -- Jedi Academy's and Jedi Outcast's alike -- misc_turret is the small turret that
hangs from a ceiling (or sits on a floor), the one multiplayer calls misc_turretG2; the big
two-piece Hoth turbolaser above is multiplayer's own meaning of the name. So every misc_turret on
an SP map spawned a 144-unit Hoth turret -- two networked entities -- where the map wanted a
ceiling turret: 67 of them across 14 Jedi Outcast maps, 13 on three Jedi Academy ones. On a map
level.rp_sp_game names, SP_misc_turret() hands the entity to this instead, and it becomes a
misc_turretG2 read the single-player way. The classname stays misc_turret. (level.rp_sp_game is
kept for the whole level, so a turret an admin or a preset adds later is read the same way; a
preset stores the map's own keys, so a reload translates them again from those, never twice.)

Spawnflags. Both keep START_OFF (1) and UPSIDE_DOWN (2) where misc_turretG2 has them. Jedi
Academy's TURBO is 4, misc_turretG2's is 8, and 4 there is CANRESPAWN; Jedi Outcast has no bit
4 at all (and neither game uses 8). Everything else is dropped, our 32768 (UPSIDE_DOWN without
the 22-unit drop) included. Masking with 5, as OJP does, loses UPSIDE_DOWN -- which would sink
cairn_assembly's four floor turrets into its floor -- and turns t2_wedge's turbolasers into small
turrets that respawn.

Every other key -- radius, wait, dmg, health, splashDamage, splashRadius, shotspeed, target,
target2, targetname -- means the same thing to misc_turretG2, with the same defaults.

"team" does not. In single player it names a team the turret leaves alone: it does not target
clients of it and takes no damage from them -- "player", "enemy" (the default) or "neutral". A
multiplayer turret shoots every client, so these would have gunned down the map's own
stormtroopers. The team goes in rpSpTurretTeam, read by turretG2_find_enemies() and G_Damage();
players are always NPCTEAM_PLAYER, so a default turret still targets them. A name that is not a
team leaves the key to misc_turretG2 and the turret leaves nobody alone, as single player's
lookup would have.

A Jedi Academy TURBO turret is a turbolaser -- t2_wedge's four, which start off and are only
switched on by its closing cutscene, a script multiplayer cannot run. Single player gives it its
own defaults, and so does this for any key the map leaves unset: damage 10, shot speed 4000, a
shot every 500 ms, double size (unless "customscale" says otherwise), no damage taken, and it
leaves only neutral clients alone. Its health, range and explosion are already the same in
multiplayer.
------------------
*/
void SP_misc_turretG2( gentity_t *base );

static void RP_SpawnSPTurret( gentity_t *base )
{
	int		spFlags = base->spawnflags;
	qboolean turbo = ( level.rp_sp_game == RP_SP_GAME_JA && ( spFlags & 4 ) ) ? qtrue : qfalse;
	float	shotSpeed;
	int		customScale;

	base->spawnflags = spFlags & ( 1 | 2 );	// START_OFF, UPSIDE_DOWN
	if ( turbo )
	{
		base->spawnflags |= 8;	// misc_turretG2's TURBO
	}

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
		{ // not a team: nobody is left alone, and misc_turretG2 reads the key as it would anywhere
			base->rpSpTurretTeam = NPCTEAM_NUM_TEAMS;
		}

		if ( base->rpSpTurretTeam != NPCTEAM_NUM_TEAMS )
		{ // a team name: atoi() would read it as 0 anyway, but it is not misc_turretG2's key
			base->team = NULL;
		}
	}

	G_SpawnFloat( "shotspeed", "0", &shotSpeed );
	G_SpawnInt( "customscale", "0", &customScale );

	if ( turbo )
	{
		base->rpSpTurretTeam = NPCTEAM_NEUTRAL;

		// misc_turretG2 only fills in what is still 0
		if ( !base->damage )
		{
			base->damage = 10;
		}
		if ( !base->wait )
		{
			base->wait = 500;
		}
	}

	SP_misc_turretG2( base );

	if ( turbo && base->inuse )
	{
		if ( !shotSpeed )
		{ // misc_turretG2 reads the key itself, over whatever was here, so this goes in after it
			base->mass = 4000;
		}

		if ( !customScale )
		{
			base->s.iModelScale = 200;
			base->modelScale[0] = base->modelScale[1] = base->modelScale[2] = 2.0f;
			VectorScale( base->r.mins, 2.0f, base->r.mins );
			VectorScale( base->r.maxs, 2.0f, base->r.maxs );
			base->s.g2radius = 255;	// twice misc_turretG2's 128, as far as the 8-bit field goes
		}

		base->takedamage = qfalse;
		trap->LinkEntity( (sharedEntity_t *)base );
	}
}

//-----------------------------------------------------
void SP_misc_turret( gentity_t *base )
//-----------------------------------------------------
{
	char* s;

	if ( level.rp_sp_game != RP_SP_GAME_NONE )
	{ // GalaxyRP: [SP Maps] the single-player turret
		RP_SpawnSPTurret( base );
		return;
	}

	base->s.modelindex2 = G_ModelIndex( "models/map_objects/hoth/turret_bottom.md3" );
	base->s.modelindex = G_ModelIndex( "models/map_objects/hoth/turret_base.md3" );
	//base->playerModel = trap->G2API_InitGhoul2Model( base->ghoul2, "models/map_objects/imp_mine/turret_canon.glm", base->s.modelindex );
	//base->s.radius = 80.0f;

	//trap->G2API_SetBoneAngles( &base->ghoul2[base->playerModel], "Bone_body", vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_Y, POSITIVE_Z, POSITIVE_X, NULL );
	//base->torsoBolt = trap->G2API_AddBolt( &base->ghoul2[base->playerModel], "*flash03" );

	G_SpawnString( "icon", "", &s );
	if (s && s[0])
	{
		// We have an icon, so index it now.  We are reusing the genericenemyindex
		// variable rather than adding a new one to the entity state.
		base->s.genericenemyindex = G_IconIndex(s);
	}

	G_SetAngles( base, base->s.angles );
	G_SetOrigin( base, base->s.origin );

	base->r.contents = CONTENTS_BODY;

	VectorSet( base->r.maxs, 32.0f, 32.0f, 128.0f );
	VectorSet( base->r.mins, -32.0f, -32.0f, 0.0f );

	base->use = turret_base_use;
	base->think = turret_base_think;
	// don't start working right away
	base->nextthink = level.time + FRAMETIME * 5;

	trap->LinkEntity( (sharedEntity_t *)base );

	if ( !turret_base_spawn_top( base ) )
	{
		G_FreeEntity( base );
	}
}

//-----------------------------------------------------
qboolean turret_base_spawn_top( gentity_t *base )
{
	vec3_t		org;
	int			t;

	gentity_t *top = G_Spawn();
	if ( !top )
	{
		return qfalse;
	}

	top->s.modelindex = G_ModelIndex( "models/map_objects/hoth/turret_top_new.md3" );
	top->s.modelindex2 = G_ModelIndex( "models/map_objects/hoth/turret_top.md3" );
	G_SetAngles( top, base->s.angles );
	VectorCopy( base->s.origin, org );
	org[2] += 128;
	G_SetOrigin( top, org );

	base->r.ownerNum = top->s.number;
	top->r.ownerNum = base->s.number;

	if ( base->team && base->team[0] && //level.gametype == GT_SIEGE &&
		!base->teamnodmg)
	{
		base->teamnodmg = atoi(base->team);
	}
	base->team = NULL;
	top->teamnodmg = base->teamnodmg;
	top->alliedTeam = base->alliedTeam;

	base->s.eType = ET_GENERAL;

	// Set up our explosion effect for the ExplodeDeath code....
	G_EffectIndex( "turret/explode" );
	G_EffectIndex( "sparks/spark_exp_nosnd" );
	G_EffectIndex( "turret/hoth_muzzle_flash" );

	// this is really the pitch angle.....
	top->speed = 0;

	// this is a random time offset for the no-enemy-search-around-mode
	top->count = Q_flrand(0.0f, 1.0f) * 9000;

	if ( !base->health )
	{
		base->health = 3000;
	}
	top->health = base->health;

	G_SpawnInt( "showhealth", "0", &t );

	if (t)
	{ //a non-0 maxhealth value will mean we want to show the health on the hud
		top->maxHealth = base->health; //acts as "maxhealth"
		G_ScaleNetHealth(top);

		base->maxHealth = base->health;
		G_ScaleNetHealth(base);
	}

	base->takedamage = qtrue;
	base->pain = TurretBasePain;
	base->die = bottom_die;

	//design specified shot speed
	G_SpawnFloat( "shotspeed", "1100", &base->mass );
	// GalaxyRP fix: [Entity System] a bolt that does not move (shotspeed 0, or backwards) never hits
	// anything and lives its full 10 seconds -- the key comes off /entadd, so it is floored
	if ( base->mass < 100 )
	{
		G_LogPrintf( "misc_turret at %s: shotspeed %g raised to 100\n", vtos( base->s.origin ), base->mass );
		base->mass = 100;
	}
	top->mass = base->mass;

	//even if we don't want to show health, let's at least light the crosshair up properly over ourself
	if ( !top->s.teamowner )
	{
		top->s.teamowner = top->alliedTeam;
	}

	base->alliedTeam = top->alliedTeam;
	base->s.teamowner = top->s.teamowner;

	base->s.shouldtarget = qtrue;
	top->s.shouldtarget = qtrue;

	//link them to each other
	base->target_ent = top;
	top->target_ent = base;
	// GalaxyRP: [Entity System] and the top as the base's child, so /entremove, /entundo and /entedit of
	// the base take it along (it has no record of its own) -- see RP_FreeEntityChildren() in g_spawn.c
	RP_MarkChild( base, top );

	//top->s.owner = MAX_CLIENTS; //not owned by any client

	// search radius
	if ( !base->radius )
	{
		base->radius = 1024;
	}
	top->radius = base->radius;

	// How quickly to fire
	// GalaxyRP fix: [Entity System] the zero test let a negative or tiny wait through, and a wait
	// already past means a bolt on every think: floored at 100 ms, the mod's rate for its other
	// timed entities, and the default kept for anything below that
	if ( base->wait < 100 )
	{
		if ( base->wait )
		{
			G_LogPrintf( "misc_turret at %s: wait %g too short, default used\n", vtos( base->s.origin ), base->wait );
		}
		base->wait = 300 + Q_flrand(0.0f, 1.0f) * 55;
	}
	top->wait = base->wait;

	if ( !base->splashDamage )
	{
		base->splashDamage = 300;
	}
	top->splashDamage = base->splashDamage;

	if ( !base->splashRadius )
	{
		base->splashRadius = 128;
	}
	top->splashRadius = base->splashRadius;

	// how much damage each shot does
	if ( !base->damage )
	{
		base->damage = 100;
	}
	top->damage = base->damage;

	// how fast it turns
	if ( !base->speed )
	{
		base->speed = 20;
	}
	top->speed = base->speed;

	VectorSet( top->r.maxs, 48.0f, 48.0f, 16.0f );
	VectorSet( top->r.mins, -48.0f, -48.0f, 0.0f );
	// Precache moving sounds
	//G_SoundIndex( "sound/chars/turret/startup.wav" );
	//G_SoundIndex( "sound/chars/turret/shutdown.wav" );
	//G_SoundIndex( "sound/chars/turret/ping.wav" );
	G_SoundIndex( "sound/vehicles/weapons/hoth_turret/turn.wav" );
	top->genericValue13 = G_EffectIndex( "turret/hoth_muzzle_flash" );
	top->genericValue14 = G_EffectIndex( "turret/hoth_shot" );
	top->genericValue15 = G_EffectIndex( "turret/hoth_impact" );

	top->r.contents = CONTENTS_BODY;

	//base->max_health = base->health;
	top->takedamage = qtrue;
	top->pain = TurretPain;
	top->die  = auto_turret_die;

	top->material = MAT_METAL;
	//base->r.svFlags |= SVF_NO_TELEPORT|SVF_NONNPC_ENEMY|SVF_SELF_ANIMATING;

	// Register this so that we can use it for the missile effect
	RegisterItem( BG_FindItemForWeapon( WP_EMPLACED_GUN ));

	// But set us as a turret so that we can be identified as a turret
	top->s.weapon = WP_EMPLACED_GUN;

	trap->LinkEntity( (sharedEntity_t *)top );
	return qtrue;
}
