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
#include "w_saber.h"
#include "qcommon/q_shared.h"

#define	MISSILE_PRESTEP_TIME	50

extern void laserTrapStick( gentity_t *ent, vec3_t endpos, vec3_t normal );
extern void Jedi_Decloak( gentity_t *self );

extern qboolean FighterIsLanded( Vehicle_t *pVeh, playerState_t *parentPS );

/*
================
G_ReflectMissile

  Reflect the missile roughly back at it's owner
================
*/
float RandFloat(float min, float max);
void G_ReflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward )
{
	vec3_t	bounce_dir;
	int		i;
	float	speed;
	int		isowner = 0;

	if (missile->r.ownerNum == ent->s.number)
	{ //the original owner is bouncing the missile, so don't try to bounce it back at him
		isowner = 1;
	}

	//save the original speed
	speed = VectorNormalize( missile->s.pos.trDelta );

	if ( &g_entities[missile->r.ownerNum] && missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART && !isowner )
	{//bounce back at them if you can
		VectorSubtract( g_entities[missile->r.ownerNum].r.currentOrigin, missile->r.currentOrigin, bounce_dir );
		VectorNormalize( bounce_dir );
	}
	else if (isowner)
	{ //in this case, actually push the missile away from me, and since we're giving boost to our own missile by pushing it, up the velocity
		vec3_t missile_dir;

		speed *= 1.5;

		VectorSubtract( missile->r.currentOrigin, ent->r.currentOrigin, missile_dir );
		VectorCopy( missile->s.pos.trDelta, bounce_dir );
		VectorScale( bounce_dir, DotProduct( forward, missile_dir ), bounce_dir );
		VectorNormalize( bounce_dir );
	}
	else
	{
		vec3_t missile_dir;

		VectorSubtract( ent->r.currentOrigin, missile->r.currentOrigin, missile_dir );
		VectorCopy( missile->s.pos.trDelta, bounce_dir );
		VectorScale( bounce_dir, DotProduct( forward, missile_dir ), bounce_dir );
		VectorNormalize( bounce_dir );
	}
	for ( i = 0; i < 3; i++ )
	{
		bounce_dir[i] += RandFloat( -0.2f, 0.2f );
	}

	VectorNormalize( bounce_dir );
	VectorScale( bounce_dir, speed, missile->s.pos.trDelta );
	missile->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy( missile->r.currentOrigin, missile->s.pos.trBase );
	if ( missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART )
	{//you are mine, now!
		missile->r.ownerNum = ent->s.number;
	}
	if ( missile->s.weapon == WP_ROCKET_LAUNCHER )
	{//stop homing
		missile->think = 0;
		missile->nextthink = 0;
	}
}

void G_DeflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward )
{
	vec3_t	bounce_dir;
	int		i;
	float	speed;
	vec3_t missile_dir;

	//save the original speed
	speed = VectorNormalize( missile->s.pos.trDelta );

	if (ent->client)
	{
		//VectorSubtract( ent->r.currentOrigin, missile->r.currentOrigin, missile_dir );
		AngleVectors(ent->client->ps.viewangles, missile_dir, 0, 0);
		VectorCopy(missile_dir, bounce_dir);
		//VectorCopy( missile->s.pos.trDelta, bounce_dir );
		VectorScale( bounce_dir, DotProduct( forward, missile_dir ), bounce_dir );
		VectorNormalize( bounce_dir );
	}
	else
	{
		VectorCopy(forward, bounce_dir);
		VectorNormalize(bounce_dir);
	}

	for ( i = 0; i < 3; i++ )
	{
		bounce_dir[i] += RandFloat( -1.0f, 1.0f );
	}

	VectorNormalize( bounce_dir );
	VectorScale( bounce_dir, speed, missile->s.pos.trDelta );
	missile->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy( missile->r.currentOrigin, missile->s.pos.trBase );
	if ( missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART )
	{//you are mine, now!
		missile->r.ownerNum = ent->s.number;
	}
	if ( missile->s.weapon == WP_ROCKET_LAUNCHER )
	{//stop homing
		missile->think = 0;
		missile->nextthink = 0;
	}
}

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );


	if ( ent->flags & FL_BOUNCE_SHRAPNEL )
	{
		VectorScale( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta );
		ent->s.pos.trType = TR_GRAVITY;

		// check for stop
		if ( trace->plane.normal[2] > 0.7 && ent->s.pos.trDelta[2] < 40 ) //this can happen even on very slightly sloped walls, so changed it from > 0 to > 0.7
		{
			G_SetOrigin( ent, trace->endpos );
			ent->nextthink = level.time + 100;
			return;
		}
	}
	else if ( ent->flags & FL_BOUNCE_HALF )
	{
		VectorScale( ent->s.pos.trDelta, 0.65f, ent->s.pos.trDelta );
		// check for stop
		if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 )
		{
			G_SetOrigin( ent, trace->endpos );
			return;
		}
	}

	if (ent->s.weapon == WP_THERMAL)
	{ //slight hack for hit sound
		G_Sound(ent, CHAN_BODY, G_SoundIndex(va("sound/weapons/thermal/bounce%i.wav", Q_irand(1, 2))));
	}
	else if (ent->s.weapon == WP_SABER)
	{
		G_Sound(ent, CHAN_BODY, G_SoundIndex(va("sound/weapons/saber/bounce%i.wav", Q_irand(1, 3))));
	}
	else if (ent->s.weapon == G2_MODEL_PART)
	{
		//Limb bounce sound?
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	if (ent->bounceCount != -5)
	{
		ent->bounceCount--;
	}
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

	ent->freeAfterEvent = qtrue;

	ent->takedamage = qfalse;
	// splash damage
	if ( ent->splashDamage ) {
		if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent,
				ent, ent->splashMethodOfDeath ) )
		{
			if (ent->parent)
			{
				g_entities[ent->parent->s.number].client->accuracy_hits++;
			}
			else if (ent->activator)
			{
				g_entities[ent->activator->s.number].client->accuracy_hits++;
			}
		}
	}

	trap->LinkEntity( (sharedEntity_t *)ent );
}

void G_RunStuckMissile( gentity_t *ent )
{
	if ( ent->takedamage )
	{
		if ( ent->s.groundEntityNum >= 0 && ent->s.groundEntityNum < ENTITYNUM_WORLD )
		{
			gentity_t *other = &g_entities[ent->s.groundEntityNum];

			if ( (!VectorCompare( vec3_origin, other->s.pos.trDelta ) && other->s.pos.trType != TR_STATIONARY) ||
				(!VectorCompare( vec3_origin, other->s.apos.trDelta ) && other->s.apos.trType != TR_STATIONARY) )
			{//thing I stuck to is moving or rotating now, kill me
				G_Damage( ent, other, other, NULL, NULL, 99999, 0, MOD_CRUSH );
				return;
			}
		}
	}
	// check think function
	G_RunThink( ent );
}

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
	vec3_t v, newv;
	float dot;

	VectorSubtract( impact, start, v );
	dot = DotProduct( v, dir );
	VectorMA( v, -2*dot, dir, newv );

	VectorNormalize(newv);
	VectorMA(impact, 8192, newv, endout);
}


//-----------------------------------------------------------------------------
gentity_t *CreateMissile( vec3_t org, vec3_t dir, float vel, int life,
							gentity_t *owner, qboolean altFire)
//-----------------------------------------------------------------------------
{
	gentity_t	*missile;

	missile = G_Spawn();

	missile->nextthink = level.time + life;
	missile->think = G_FreeEntity;
	missile->s.eType = ET_MISSILE;
	missile->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	missile->parent = owner;
	missile->r.ownerNum = owner->s.number;

	if (altFire)
	{
		missile->s.eFlags |= EF_ALT_FIRING;
	}

	missile->s.pos.trType = TR_LINEAR;
	missile->s.pos.trTime = level.time;// - MISSILE_PRESTEP_TIME;	// NOTENOTE This is a Quake 3 addition over JK2
	missile->target_ent = NULL;

	SnapVector(org);
	VectorCopy( org, missile->s.pos.trBase );
	VectorScale( dir, vel, missile->s.pos.trDelta );
	VectorCopy( org, missile->r.currentOrigin);
	SnapVector(missile->s.pos.trDelta);

	return missile;
}

void G_MissileBounceEffect( gentity_t *ent, vec3_t org, vec3_t dir )
{
	//FIXME: have an EV_BOUNCE_MISSILE event that checks the s.weapon and does the appropriate effect
	switch( ent->s.weapon )
	{
	case WP_BOWCASTER:
		G_PlayEffectID( G_EffectIndex("bowcaster/deflect"), ent->r.currentOrigin, dir );
		break;
	case WP_BLASTER:
	case WP_BRYAR_PISTOL:
		G_PlayEffectID( G_EffectIndex("blaster/deflect"), ent->r.currentOrigin, dir );
		break;
	default:
		{
			gentity_t *te = G_TempEntity( org, EV_SABER_BLOCK );
			VectorCopy(org, te->s.origin);
			VectorCopy(dir, te->s.angles);
			te->s.eventParm = 0;
			te->s.weapon = 0;//saberNum
			te->s.legsAnim = 0;//bladeNum
		}
		break;
	}
}

/*
================
G_MissileImpact
================
*/
void WP_SaberBlockNonRandom( gentity_t *self, vec3_t hitloc, qboolean missileBlock );
void WP_flechette_alt_blow( gentity_t *ent );
// GalaxyRP: [Grapple Hook] the hook's impact, split out so it can run FIRST. TaystJK's copy of this
// sits two hundred lines further down, after the saber-block and deflection section -- so a hook
// aimed at anyone holding a lit saber was reflected like a blaster bolt, never parked, and its
// owner's button latches stayed set on a hook that had bounced away (the "grapples should probably
// just not bounce off sabers" note in their Weapon_HookFree). Here it is the first thing
// G_MissileImpact() does, so a hook never sees the bounce, saber-block, duel-passthrough or damage
// code at all. Returns qtrue when the missile was a hook and has been dealt with.
//
// Three outcomes. A living client that is not a vehicle and not in a private duel: the hook parks on
// them and Weapon_HookThink() follows them from then on (TaystJK's behaviour, minus the two
// refusals; nobody gets dragged out of a duel and a swoop is not a hitching post). A brush entity:
// parks at the impact point, except func_rotating and func_pendulum, whose movement the anchor
// could not follow. Anything else -- the world -- parks at the impact point. Either way it becomes a
// stationary entity at the anchor, its think turns into Weapon_HookThink(), and the anchor goes
// straight into the owner's ps.lastHitLoc so the pull can start on the next frame rather than the
// one after. The impact effects are the base tusken ones the server plays for everyone, so a
// TaystJK client sees them too.
static qboolean G_HookImpact( gentity_t *ent, trace_t *trace )
{
	gentity_t	*other;
	vec3_t		v;

	if ( !ent->classname || strcmp( ent->classname, RP_HOOK_CLASSNAME ) )
	{
		return qfalse;
	}

	other = &g_entities[trace->entityNum];

	if ( !ent->parent || !ent->parent->client || ent->parent->client->hook != ent )
	{ // orphaned -- the owner respawned or left while it was in flight
		Weapon_HookFree( ent );
		return qtrue;
	}

	if ( ent->think == Weapon_HookThink )
	{ // already parked. A hook sitting inside the player it is attached to starts every frame's
	  // trace in solid, and G_RunMissile() reports that as a fresh impact; there is nothing to do
	  // with it. (TaystJK re-parks and re-links on each one, guarded only against replaying the
	  // effect, through s.hasLookTarget.)
		return qtrue;
	}

	ent->enemy = NULL;
	ent->s.otherEntityNum = ENTITYNUM_NONE;
	ent->s.groundEntityNum = ENTITYNUM_NONE;

	if ( other->client && !( other->s.eFlags & EF_DEAD ) )
	{
		if ( other->s.NPC_class == CLASS_VEHICLE || other->client->ps.duelInProgress )
		{
			Weapon_HookFree( ent );
			return qtrue;
		}

		G_PlayEffectID( G_EffectIndex( "tusken/hit" ), trace->endpos, trace->plane.normal );

		ent->enemy = other;
		ent->s.otherEntityNum = other->s.number;

		VectorCopy( other->r.currentOrigin, v );
		v[2] += ( other->r.mins[2] + other->r.maxs[2] ) * 0.5f;
		SnapVectorTowards( v, ent->s.pos.trBase );
	}
	else if ( other->s.eType == ET_MOVER )
	{
		if ( other->classname && ( !strcmp( other->classname, "func_rotating" ) || !strcmp( other->classname, "func_pendulum" ) ) )
		{
			Weapon_HookFree( ent );
			return qtrue;
		}

		G_PlayEffectID( G_EffectIndex( "tusken/hitwall" ), trace->endpos, trace->plane.normal );

		ent->s.otherEntityNum = other->s.number;
		ent->s.groundEntityNum = other->s.number;
		VectorCopy( trace->endpos, v );
	}
	else
	{
		G_PlayEffectID( G_EffectIndex( "tusken/hitwall" ), trace->endpos, trace->plane.normal );
		VectorCopy( trace->endpos, v );
	}

	VectorCopy( trace->plane.normal, ent->s.angles );
	SnapVectorTowards( v, ent->s.pos.trBase );

	G_SetOrigin( ent, v );

	ent->think = Weapon_HookThink;
	ent->nextthink = level.time + FRAMETIME;

	VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.lastHitLoc );

	trap->LinkEntity( (sharedEntity_t *)ent );

	return qtrue;
}

void G_MissileImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t		*other;
	qboolean		hitClient = qfalse;
	qboolean		isKnockedSaber = qfalse;

	// GalaxyRP: [Grapple Hook] first, before anything that could bounce, block or damage -- see
	// G_HookImpact() above for why the order matters.
	if ( G_HookImpact( ent, trace ) )
	{
		return;
	}

	other = &g_entities[trace->entityNum];

	// check for bounce
	if ( !other->takedamage &&
		(ent->bounceCount > 0 || ent->bounceCount == -5) &&
		( ent->flags & ( FL_BOUNCE | FL_BOUNCE_HALF ) ) ) {
		G_BounceMissile( ent, trace );
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
		return;
	}
	else if (ent->neverFree && ent->s.weapon == WP_SABER && (ent->flags & FL_BOUNCE_HALF))
	{ //this is a knocked-away saber
		if (ent->bounceCount > 0 || ent->bounceCount == -5)
		{
			G_BounceMissile( ent, trace );
			G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
			return;
		}

		isKnockedSaber = qtrue;
	}

	// I would glom onto the FL_BOUNCE code section above, but don't feel like risking breaking something else
	if ( (!other->takedamage && (ent->bounceCount > 0 || ent->bounceCount == -5) && ( ent->flags&(FL_BOUNCE_SHRAPNEL) ) ) || ((trace->surfaceFlags&SURF_FORCEFIELD)&&!ent->splashDamage&&!ent->splashRadius&&(ent->bounceCount > 0 || ent->bounceCount == -5)) )
	{
		G_BounceMissile( ent, trace );

		if ( ent->bounceCount < 1 )
		{
			ent->flags &= ~FL_BOUNCE_SHRAPNEL;
		}
		return;
	}

	/*
	if ( !other->takedamage && ent->s.weapon == WP_THERMAL && !ent->alt_fire )
	{//rolling thermal det - FIXME: make this an eFlag like bounce & stick!!!
		//G_BounceRollMissile( ent, trace );
		if ( ent->owner && ent->owner->s.number == 0 )
		{
			G_MissileAddAlerts( ent );
		}
		//trap->linkentity( ent );
		return;
	}
	*/

	if ((other->r.contents & CONTENTS_LIGHTSABER) && !isKnockedSaber)
	{ //hit this person's saber, so..
		gentity_t *otherOwner = &g_entities[other->r.ownerNum];

		if (otherOwner->takedamage && otherOwner->client && otherOwner->client->ps.duelInProgress &&
			otherOwner->client->ps.duelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
	}
	else if (!isKnockedSaber)
	{
		if (other->takedamage && other->client && other->client->ps.duelInProgress &&
			other->client->ps.duelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
	}

	// zyk: rockets and concussion, both with upgrade, can use force pushable/pullable entities, like the rock at start of t3_rift map
	// GalaxyRP fix: [Magic] a third disjunct used to follow the two below, admitting an Ultra Bolt --
	// a concussion shot whose methodOfDeath was MOD_MELEE. Nothing in the tree assigns MOD_MELEE to a
	// missile (magic_fist_velocity() went with the magic cvars), so it could never be true.
	if (Q_stricmp(other->classname, "func_static") == 0 && (other->spawnflags & 1 || other->spawnflags & 2) && 
		ent->parent && ent->parent->client && ent->parent->client->sess.amrpgmode == 2 && 
		((ent->s.weapon == WP_ROCKET_LAUNCHER && ent->parent->client->pers.skill_levels[26] == 2) || 
		(ent->s.weapon == WP_CONCUSSION && ent->parent->client->pers.skill_levels[27] == 2)))
	{
		GlobalUse(other, ent->parent, ent->parent);
		goto killProj;
	}

	if (other->flags & FL_DMG_BY_HEAVY_WEAP_ONLY)
	{
		if (ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_ROCKET &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
			ent->methodOfDeath != MOD_ROCKET_HOMING &&
			ent->methodOfDeath != MOD_THERMAL &&
			ent->methodOfDeath != MOD_THERMAL_SPLASH &&
			ent->methodOfDeath != MOD_TRIP_MINE_SPLASH &&
			ent->methodOfDeath != MOD_TIMED_MINE_SPLASH &&
			ent->methodOfDeath != MOD_DET_PACK_SPLASH &&
			ent->methodOfDeath != MOD_VEHICLE &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT &&
			ent->methodOfDeath != MOD_SABER &&
			ent->methodOfDeath != MOD_TURBLAST)
		{
			// GalaxyRP fix: [Magic] a "cannot_deflect" guard used to wrap this block, set by a Magic Fist
			// test (methodOfDeath == MOD_MELEE on a bowcaster/demp2/concussion shot). Nothing in the tree
			// can produce such a missile any more -- magic_fist_velocity() went with the magic cvars and
			// no code assigns MOD_MELEE to a missile's methodOfDeath -- so the guard was always false and
			// the deflect always ran. What is left is the stock TaystJK deflect, which has no such guard.
			vec3_t fwd;

			if (trace)
			{
				VectorCopy(trace->plane.normal, fwd);
			}
			else
			{ //oh well
				AngleVectors(other->r.currentAngles, fwd, NULL, NULL);
			}

			G_DeflectMissile(other, ent, fwd);
			G_MissileBounceEffect(ent, ent->r.currentOrigin, fwd);
			return;
		}
	}

	// GalaxyRP (Alex): [Armor Skill] restored the zyk_can_deflect_shots(other) disjunct here — Armor
	// skill level 1-5 now gives a percent chance to deflect blaster-type shots (see g_cmds.c). The
	// weapon/MOD exclusion checks were reordered to run first: zyk_can_deflect_shots() rolls a
	// chance and sets a cooldown as a side effect, so it must only be evaluated once we already know
	// this shot is an eligible (non-heavy) weapon -- otherwise an excluded shot (rocket, thermal,
	// etc.) could burn a player's deflect proc/cooldown on a shot that was never going to be
	// deflected anyway. This is a pure reordering of the same && chain (order doesn't change the
	// FL_SHIELDED-only result for NPCs, only when the new stateful check gets reached).
	if (ent->s.weapon != WP_ROCKET_LAUNCHER &&
		ent->s.weapon != WP_THERMAL &&
		ent->s.weapon != WP_TRIP_MINE &&
		ent->s.weapon != WP_DET_PACK &&
		ent->s.weapon != WP_DEMP2 &&
		ent->s.weapon != WP_EMPLACED_GUN &&
		ent->methodOfDeath != MOD_REPEATER_ALT &&
		ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
		ent->methodOfDeath != MOD_TURBLAST &&
		ent->methodOfDeath != MOD_VEHICLE &&
		ent->methodOfDeath != MOD_CONC &&
		ent->methodOfDeath != MOD_CONC_ALT &&
		!(ent->dflags&DAMAGE_HEAVY_WEAP_CLASS) &&
		(other->flags & FL_SHIELDED || zyk_can_deflect_shots(other)) )
	{
		// GalaxyRP fix: [Magic] a "cannot_deflect" guard used to wrap this block, set by three tests that
		// can no longer be true: the shot's owner being NPC_type "guardian_boss_7" or "guardian_boss_10"
		// (both definitions went with the three zyk_quest_*.npc asset files, so neither NPC can exist),
		// and a Magic Fist test on methodOfDeath == MOD_MELEE, which nothing in the tree ever assigns to
		// a missile. The owner lookup and the this_npc local existed only to feed those two NPC tests and
		// went with them. What is left is the stock TaystJK deflect, which has no such guard.
		vec3_t fwd;

		if (other->client)
		{
			AngleVectors(other->client->ps.viewangles, fwd, NULL, NULL);
		}
		else
		{
			AngleVectors(other->r.currentAngles, fwd, NULL, NULL);
		}

		G_DeflectMissile(other, ent, fwd);
		G_MissileBounceEffect(ent, ent->r.currentOrigin, fwd);
		return;
	}

	if (other->takedamage && other->client &&
		ent->s.weapon != WP_ROCKET_LAUNCHER &&
		ent->s.weapon != WP_THERMAL &&
		ent->s.weapon != WP_TRIP_MINE &&
		ent->s.weapon != WP_DET_PACK &&
		ent->s.weapon != WP_DEMP2 &&
		// GalaxyRP fix: [Magic] a "not an Ultra Bolt" conjunct used to be here, excluding a concussion
		// shot whose methodOfDeath was MOD_MELEE. Nothing in the tree assigns MOD_MELEE to a missile,
		// so it was a permanently-true conjunct.
		ent->methodOfDeath != MOD_REPEATER_ALT &&
		ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
		ent->methodOfDeath != MOD_CONC &&
		ent->methodOfDeath != MOD_CONC_ALT &&
		other->client->ps.saberBlockTime < level.time &&
		!isKnockedSaber &&
		// GalaxyRP fix: [Combat] the attacker is whoever last sent this missile (r.ownerNum, which a
		// reflect re-points at the reflector), so WP_SaberCanBlock can tell whether the blocker is
		// aiming at them.
		WP_SaberCanBlock(other, (ent->r.ownerNum >= 0 && ent->r.ownerNum < ENTITYNUM_WORLD) ? &g_entities[ent->r.ownerNum] : NULL,
			ent->r.currentOrigin, 0, 0, qtrue, 0))
	{ //only block one projectile per 200ms (to prevent giant swarms of projectiles being blocked)
		vec3_t fwd;
		gentity_t *te;
		int otherDefLevel = other->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE];

		te = G_TempEntity( ent->r.currentOrigin, EV_SABER_BLOCK );
		VectorCopy(ent->r.currentOrigin, te->s.origin);
		VectorCopy(trace->plane.normal, te->s.angles);
		te->s.eventParm = 0;
		te->s.weapon = 0;//saberNum
		te->s.legsAnim = 0;//bladeNum

		/*if (other->client->ps.velocity[2] > 0 ||
			other->client->pers.cmd.forwardmove ||
			other->client->pers.cmd.rightmove)
			*/
		if (other->client->ps.velocity[2] > 0 ||
			other->client->pers.cmd.forwardmove < 0) //now we only do it if jumping or running backward. Should be able to full-on charge.
		{
			otherDefLevel -= 1;
			if (otherDefLevel < 0)
			{
				otherDefLevel = 0;
			}
		}

		AngleVectors(other->client->ps.viewangles, fwd, NULL, NULL);
		// GalaxyRP fix: [Combat] this tested == FORCE_LEVEL_1, so an effective level of 0 -- Saber
		// Defense 1 while jumping or backpedalling (the -1 just above), or a Defense 0 block --
		// matched none of the arms below and fell into the final else, the Defense 5 100% reflect.
		// Level 0 now stops the shot the way level 1 does; it never reflects.
		if (otherDefLevel <= FORCE_LEVEL_1)
		{
			//if def is only level 1, instead of deflecting the shot it should just die here
		}
		else if (otherDefLevel == FORCE_LEVEL_2)
		{
			G_DeflectMissile(other, ent, fwd);
		}
		// GalaxyRP (Alex): [Combat] Def 3 25% chance to reflect
		else if (otherDefLevel == FORCE_LEVEL_3)
		{
			if (Q_irand(0, 3)) 
			{
				G_DeflectMissile(other, ent, fwd);
			}
			else 
			{
				G_ReflectMissile(other, ent, fwd);
			}
		}
		// GalaxyRP (Alex): [Combat] Def 4 50% chance to reflect
		else if (otherDefLevel == FORCE_LEVEL_4)
		{
			if (Q_irand(0, 1))
			{
				G_DeflectMissile(other, ent, fwd);
			}
			else
			{
				G_ReflectMissile(other, ent, fwd);
			}
		}
		// GalaxyRP (Alex): [Combat] Def 5 100% chance to reflect
		else
		{
			G_ReflectMissile(other, ent, fwd);
		}
		other->client->ps.saberBlockTime = level.time + (350 - (otherDefLevel*100)); //200;

		//For jedi AI
		other->client->ps.saberEventFlags |= SEF_DEFLECTED;

		if (otherDefLevel == FORCE_LEVEL_3)
		{
			other->client->ps.saberBlockTime = 0; //^_^
		}

		if (otherDefLevel <= FORCE_LEVEL_1)
		{
			goto killProj;
		}
		return;
	}
	else if ((other->r.contents & CONTENTS_LIGHTSABER) && !isKnockedSaber)
	{ //hit this person's saber, so..
		gentity_t *otherOwner = &g_entities[other->r.ownerNum];

		if (otherOwner->takedamage && otherOwner->client &&
			ent->s.weapon != WP_ROCKET_LAUNCHER &&
			ent->s.weapon != WP_THERMAL &&
			ent->s.weapon != WP_TRIP_MINE &&
			ent->s.weapon != WP_DET_PACK &&
			ent->s.weapon != WP_DEMP2 &&
			// GalaxyRP fix: [Magic] the matching "not an Ultra Bolt" conjunct went from here too.
			ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT /*&&
			otherOwner->client->ps.saberBlockTime < level.time*/)
		{ //for now still deflect even if saberBlockTime >= level.time because it hit the actual saber
			vec3_t fwd;
			gentity_t *te;
			int otherDefLevel = otherOwner->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE];

			//in this case, deflect it even if we can't actually block it because it hit our saber
			//WP_SaberCanBlock(otherOwner, NULL, ent->r.currentOrigin, 0, 0, qtrue, 0);
			// DAJ_RP: [NPC System] not a frozen NPC: it does not move to block (see RP_NpcFreeze(), NPC_spawn.c)
			if (otherOwner->client && otherOwner->client->ps.weaponTime <= 0 && !RP_NpcIsFrozen(otherOwner))
			{
				WP_SaberBlockNonRandom(otherOwner, ent->r.currentOrigin, qtrue);
			}

			te = G_TempEntity( ent->r.currentOrigin, EV_SABER_BLOCK );
			VectorCopy(ent->r.currentOrigin, te->s.origin);
			VectorCopy(trace->plane.normal, te->s.angles);
			te->s.eventParm = 0;
			te->s.weapon = 0;//saberNum
			te->s.legsAnim = 0;//bladeNum

			/*if (otherOwner->client->ps.velocity[2] > 0 ||
				otherOwner->client->pers.cmd.forwardmove ||
				otherOwner->client->pers.cmd.rightmove)*/
			if (otherOwner->client->ps.velocity[2] > 0 ||
				otherOwner->client->pers.cmd.forwardmove < 0) //now we only do it if jumping or running backward. Should be able to full-on charge.
			{
				otherDefLevel -= 1;
				if (otherDefLevel < 0)
				{
					otherDefLevel = 0;
				}
			}

			// DAJ_RP: [NPC System] a frozen NPC's lit blade is still solid, but it deflects and reflects
			// nothing: the shot dies on it, as at level 0 (see RP_NpcFreeze(), NPC_spawn.c)
			if (RP_NpcIsFrozen(otherOwner))
			{
				otherDefLevel = 0;
			}

			AngleVectors(otherOwner->client->ps.viewangles, fwd, NULL, NULL);

			// GalaxyRP fix: [Combat] same level-0 fall-through as the body block above: a Defense 0
			// blade (or Defense 1 while jumping/backpedalling) used to reflect everything.
			if (otherDefLevel <= FORCE_LEVEL_1)
			{
				//if def is only level 1, instead of deflecting the shot it should just die here
			}
			else if (otherDefLevel == FORCE_LEVEL_2)
			{
				G_DeflectMissile(otherOwner, ent, fwd);
			}
			// GalaxyRP (Alex): [Combat] Def 3 25% chance to reflect
			// GalaxyRP fix: [Combat] these four calls passed `other` (the lightsaber entity that the
			// missile actually struck) where every surrounding arm passes `otherOwner` (the player
			// holding it). G_ReflectMissile/G_DeflectMissile use that argument for the
			// "am I the missile's owner" self-check, for the bounce vector origin, and to reassign
			// missile->r.ownerNum -- so at Defense 3 and 4 the shot bounced off the saber's origin
			// rather than the player's, a player reflecting his own shot was not recognised as its
			// owner (so it could come back and hit him), and the reflected missile ended up owned by
			// a non-client entity, breaking kill attribution. Defense 1, 2 and 5 were already correct.
			else if (otherDefLevel == FORCE_LEVEL_3)
			{
				if (Q_irand(0, 3))
				{
					G_DeflectMissile(otherOwner, ent, fwd);
				}
				else
				{
					G_ReflectMissile(otherOwner, ent, fwd);
				}
			}
			// GalaxyRP (Alex): [Combat] Def 4 50% chance to reflect
			else if (otherDefLevel == FORCE_LEVEL_4)
			{
				if (Q_irand(0, 1))
				{
					G_DeflectMissile(otherOwner, ent, fwd);
				}
				else
				{
					G_ReflectMissile(otherOwner, ent, fwd);
				}
			}
			// GalaxyRP (Alex): [Combat] Def 5 100% chance to reflect
			else
			{
				G_ReflectMissile(otherOwner, ent, fwd);
			}
			otherOwner->client->ps.saberBlockTime = level.time + (350 - (otherDefLevel*100));//200;

			//For jedi AI
			otherOwner->client->ps.saberEventFlags |= SEF_DEFLECTED;

			if (otherDefLevel == FORCE_LEVEL_3)
			{
				otherOwner->client->ps.saberBlockTime = 0; //^_^
			}

			if (otherDefLevel <= FORCE_LEVEL_1)
			{
				goto killProj;
			}
			return;
		}
	}

	// check for sticking
	if ( !other->takedamage && ( ent->s.eFlags & EF_MISSILE_STICK ) )
	{
		laserTrapStick( ent, trace->endpos, trace->plane.normal );
		G_AddEvent( ent, EV_MISSILE_STICK, 0 );
		return;
	}

	// impact damage
	if (other->takedamage && !isKnockedSaber) {
		// FIXME: wrong damage direction?
		if ( ent->damage ) {
			vec3_t	velocity;
			qboolean didDmg = qfalse;

			if( LogAccuracyHit( other, &g_entities[ent->r.ownerNum] ) ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
				hitClient = qtrue;
			}
			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
			if ( VectorLength( velocity ) == 0 ) {
				velocity[2] = 1;	// stepped on a grenade
			}

			if (ent->s.weapon == WP_BOWCASTER || ent->s.weapon == WP_FLECHETTE ||
				ent->s.weapon == WP_ROCKET_LAUNCHER)
			{
				if (ent->s.weapon == WP_FLECHETTE && (ent->s.eFlags & EF_ALT_FIRING))
				{
					/* fix: there are rare situations where flechette did
					explode by timeout AND by impact in the very same frame, then here
					ent->think was set to G_FreeEntity, so the folowing think
					did invalidate this entity, BUT it would be reused later in this
					function for explosion event. This, then, would set ent->freeAfterEvent
					to qtrue, so event later, when reusing this entity by using G_InitEntity(),
					it would have this freeAfterEvent set AND this would in case of dropped
					item erase it from game immeadiately. THIS for example caused
					very rare flag dissappearing bug.	 */
					if (ent->think == WP_flechette_alt_blow)
						ent->think(ent);
				}
				else
				{
					G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
						/*ent->s.origin*/ent->r.currentOrigin, ent->damage,
						DAMAGE_HALF_ABSORB, ent->methodOfDeath);
					didDmg = qtrue;
				}
			}
			else
			{
				G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
					/*ent->s.origin*/ent->r.currentOrigin, ent->damage,
					0, ent->methodOfDeath);
				didDmg = qtrue;
			}

			if (didDmg && other && other->client)
			{ //What I'm wondering is why this isn't in the NPC pain funcs. But this is what SP does, so whatever.
				class_t	npc_class = other->client->NPC_class;

				// If we are a robot and we aren't currently doing the full body electricity...
				if ( npc_class == CLASS_SEEKER || npc_class == CLASS_PROBE || npc_class == CLASS_MOUSE ||
					   npc_class == CLASS_GONK || npc_class == CLASS_R2D2 || npc_class == CLASS_R5D2 || npc_class == CLASS_REMOTE ||
					   npc_class == CLASS_MARK1 || npc_class == CLASS_MARK2 || //npc_class == CLASS_PROTOCOL ||//no protocol, looks odd
					   npc_class == CLASS_INTERROGATOR || npc_class == CLASS_ATST || npc_class == CLASS_SENTRY )
				{
					// special droid only behaviors
					if ( other->client->ps.electrifyTime < level.time + 100 )
					{
						// ... do the effect for a split second for some more feedback
						other->client->ps.electrifyTime = level.time + 450;
					}
					//FIXME: throw some sparks off droids,too
				}
			}
		}

		if ( ent->s.weapon == WP_DEMP2 )
		{//a hit with demp2 decloaks people, disables ships
			if ( other && other->client && other->client->NPC_class == CLASS_VEHICLE )
			{//hit a vehicle
				if ( other->m_pVehicle //valid vehicle ent
					&& other->m_pVehicle->m_pVehicleInfo//valid stats
					&& (other->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER//always affect speeders
						||(other->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER && ent->classname && Q_stricmp("vehicle_proj", ent->classname ) == 0) )//only vehicle ion weapons affect a fighter in this manner
					&& !FighterIsLanded( other->m_pVehicle , &other->client->ps )//not landed
					&& !(other->spawnflags&2) )//and not suspended
				{//vehicles hit by "ion cannons" lose control
					if ( other->client->ps.electrifyTime > level.time )
					{//add onto it
						//FIXME: extern the length of the "out of control" time?
						other->client->ps.electrifyTime += Q_irand(200,500);
						if ( other->client->ps.electrifyTime > level.time + 4000 )
						{//cap it
							other->client->ps.electrifyTime = level.time + 4000;
						}
					}
					else
					{//start it
						//FIXME: extern the length of the "out of control" time?
						other->client->ps.electrifyTime = level.time + Q_irand(200,500);
					}
				}
			}
			// GalaxyRP fix: [Cloak Item] the explicit decloak-on-hit branch that used to be here (for a
			// non-vehicle DEMP2 direct hit) is gone -- the G_Damage() call above on `other` already runs
			// through the new centralized "any damage decloaks" hook (see G_Damage's own GalaxyRP fix
			// comment in g_combat.c), which covers this exact hit generically now, pair-aware and with
			// the same temporary re-cloak lockout.
		}
	}
killProj:
	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if ( other->takedamage && other->client && !isKnockedSaber ) {
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
		ent->s.otherEntityNum = other->s.number;
	} else if( trace->surfaceFlags & SURF_METALSTEPS ) {
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
	} else if (ent->s.weapon != G2_MODEL_PART && !isKnockedSaber) {
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	}

	if (!isKnockedSaber)
	{
		ent->freeAfterEvent = qtrue;

		// change over to a normal entity right at the point of impact
		ent->s.eType = ET_GENERAL;
	}

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	ent->takedamage = qfalse;
	// splash damage (doesn't apply to person directly hit)
	if ( ent->splashDamage ) {
		// zyk: added the Flechette alt fire condition so it does not damage againm since the call to think(), which does the flechette splash damage, was already done before
		if(!(ent->s.weapon == WP_FLECHETTE && (ent->s.eFlags & EF_ALT_FIRING)) && G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius,
			other, ent, ent->splashMethodOfDeath ) ) 
		{
			if( !hitClient
				&& g_entities[ent->r.ownerNum].client ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
			}
		}
	}

	if (ent->s.weapon == G2_MODEL_PART)
	{
		ent->freeAfterEvent = qfalse; //it will free itself
	}

	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*
================
G_RunMissile
================
*/
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin, groundSpot;
	trace_t		tr;
	int			passent;
	qboolean	isKnockedSaber = qfalse;

	if (ent->neverFree && ent->s.weapon == WP_SABER && (ent->flags & FL_BOUNCE_HALF))
	{
		isKnockedSaber = qtrue;
		ent->s.pos.trType = TR_GRAVITY;
	}

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if ( ent->target_ent ) {
		passent = ent->target_ent->s.number;
	}
	else {
		// ignore interactions with the missile owner
		if ( (ent->r.svFlags&SVF_OWNERNOTSHARED)
			&& (ent->s.eFlags&EF_JETPACK_ACTIVE) )
		{//A vehicle missile that should be solid to its owner
			//I don't care about hitting my owner
			passent = ent->s.number;
		}
		else
		{
			passent = ent->r.ownerNum;
		}
	}
	// trace a line from the previous position to the current position
	if (d_projectileGhoul2Collision.integer)
	{
		trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask, qfalse, G2TRFLAG_DOGHOULTRACE|G2TRFLAG_GETSURFINDEX|G2TRFLAG_THICK|G2TRFLAG_HITCORPSES, g_g2TraceLod.integer );

		if (tr.fraction != 1.0 && tr.entityNum < ENTITYNUM_WORLD)
		{
			gentity_t *g2Hit = &g_entities[tr.entityNum];

			if (g2Hit->inuse && g2Hit->client && g2Hit->ghoul2)
			{ //since we used G2TRFLAG_GETSURFINDEX, tr.surfaceFlags will actually contain the index of the surface on the ghoul2 model we collided with.
				g2Hit->client->g2LastSurfaceHit = tr.surfaceFlags;
				g2Hit->client->g2LastSurfaceTime = level.time;
			}

			if (g2Hit->ghoul2)
			{
				tr.surfaceFlags = 0; //clear the surface flags after, since we actually care about them in here.
			}
		}
	}
	else
	{
		trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask, qfalse, 0, 0 );
	}

	if ( tr.startsolid || tr.allsolid ) {
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask, qfalse, 0, 0 );
		tr.fraction = 0;
	}
	else {
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	if (ent->passThroughNum && tr.entityNum == (ent->passThroughNum-1))
	{
		VectorCopy( origin, ent->r.currentOrigin );
		trap->LinkEntity( (sharedEntity_t *)ent );
		goto passthrough;
	}

	trap->LinkEntity( (sharedEntity_t *)ent );

	if (ent->s.weapon == G2_MODEL_PART && !ent->bounceCount)
	{
		vec3_t lowerOrg;
		trace_t trG;

		VectorCopy(ent->r.currentOrigin, lowerOrg);
		lowerOrg[2] -= 1;
		trap->Trace( &trG, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, lowerOrg, passent, ent->clipmask, qfalse, 0, 0 );

		VectorCopy(trG.endpos, groundSpot);

		if (!trG.startsolid && !trG.allsolid && trG.entityNum == ENTITYNUM_WORLD)
		{
			ent->s.groundEntityNum = trG.entityNum;
		}
		else
		{
			ent->s.groundEntityNum = ENTITYNUM_NONE;
		}
	}

	if (level.duel_tournament_mode == 4 && ent->s.weapon != WP_SABER && 
		Distance(ent->r.currentOrigin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0))
	{
		// GalaxyRP: [Grapple Hook] G_FreeEntity() knows to detach a hook from its owner (g_utils.c),
		// so this plain free is safe for one too; a hook cannot reach into the arena any more than
		// a rocket can.
		G_FreeEntity(ent);
		return;
	}

	if ( tr.fraction != 1) {
		// never explode or bounce on sky
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			// If grapple, reset owner
			// GalaxyRP: [Grapple Hook] this stock Q3 line nulled the owner's pointer and fell through
			// to the plain G_FreeEntity below, leaving the fire latches set. A hook shot into the sky
			// now goes through the one teardown path like every other way a hook ends.
			if (ent->parent && ent->parent->client && ent->parent->client->hook == ent) {
				Weapon_HookFree( ent );
				return;
			}

			if ((ent->s.weapon == WP_SABER && ent->isSaberEntity) || isKnockedSaber)
			{
				G_RunThink( ent );
				return;
			}
			else if (ent->s.weapon != G2_MODEL_PART)
			{
				G_FreeEntity( ent );
				return;
			}
		}

#if 0 //will get stomped with missile impact event...
		if (ent->s.weapon > WP_NONE && ent->s.weapon < WP_NUM_WEAPONS &&
			(tr.entityNum < MAX_CLIENTS || g_entities[tr.entityNum].s.eType == ET_NPC))
		{ //player or NPC, try making a mark on him
			/*
			gentity_t *evEnt = G_TempEntity(ent->r.currentOrigin, EV_GHOUL2_MARK);

			evEnt->s.owner = tr.entityNum; //the entity the mark should be placed on
			evEnt->s.weapon = ent->s.weapon; //the weapon used (to determine mark type)
			VectorCopy(ent->r.currentOrigin, evEnt->s.origin); //the point of impact

			//origin2 gets the predicted trajectory-based position.
			BG_EvaluateTrajectory( &ent->s.pos, level.time, evEnt->s.origin2 );

			//If they are the same, there will be problems.
			if (VectorCompare(evEnt->s.origin, evEnt->s.origin2))
			{
				evEnt->s.origin2[2] += 2; //whatever, at least it won't mess up.
			}
			*/
			//ok, let's try adding it to the missile ent instead (tempents bad!)
			G_AddEvent(ent, EV_GHOUL2_MARK, 0);

			//copy current pos to s.origin, and current projected traj to origin2
			VectorCopy(ent->r.currentOrigin, ent->s.origin);
			BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->s.origin2 );

			//the index for whoever we are hitting
			ent->s.otherEntityNum = tr.entityNum;

			if (VectorCompare(ent->s.origin, ent->s.origin2))
			{
				ent->s.origin2[2] += 2.0f; //whatever, at least it won't mess up.
			}
		}
#else
		if (ent->s.weapon > WP_NONE && ent->s.weapon < WP_NUM_WEAPONS &&
			(tr.entityNum < MAX_CLIENTS || g_entities[tr.entityNum].s.eType == ET_NPC))
		{ //player or NPC, try making a mark on him
			//copy current pos to s.origin, and current projected traj to origin2
			VectorCopy(ent->r.currentOrigin, ent->s.origin);
			BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->s.origin2 );

			if (VectorCompare(ent->s.origin, ent->s.origin2))
			{
				ent->s.origin2[2] += 2.0f; //whatever, at least it won't mess up.
			}
		}
#endif

		G_MissileImpact( ent, &tr );

		if ( !ent->inuse )
		{ // GalaxyRP: [Grapple Hook] a hook refused by G_HookImpact() is freed inside the call; the
		  // two writes below would land in a slot that is no longer this entity
			return;
		}

		if (tr.entityNum == ent->s.otherEntityNum)
		{ //if the impact event other and the trace ent match then it's ok to do the g2 mark
			ent->s.trickedentindex = 1;
		}

		if ( ent->s.eType != ET_MISSILE && ent->s.weapon != G2_MODEL_PART )
		{
			return;		// exploded
		}
	}

passthrough:
	if ( ent->s.pos.trType == TR_STATIONARY && (ent->s.eFlags&EF_MISSILE_STICK) )
	{//stuck missiles should check some special stuff
		G_RunStuckMissile( ent );
		return;
	}

	if (ent->s.weapon == G2_MODEL_PART)
	{
		if (ent->s.groundEntityNum == ENTITYNUM_WORLD)
		{
			ent->s.pos.trType = TR_LINEAR;
			VectorClear(ent->s.pos.trDelta);
			ent->s.pos.trTime = level.time;

			VectorCopy(groundSpot, ent->s.pos.trBase);
			VectorCopy(groundSpot, ent->r.currentOrigin);

			if (ent->s.apos.trType != TR_STATIONARY)
			{
				ent->s.apos.trType = TR_STATIONARY;
				ent->s.apos.trTime = level.time;

				ent->s.apos.trBase[ROLL] = 0;
				ent->s.apos.trBase[PITCH] = 0;
			}
		}
	}

	// check think function after bouncing
	G_RunThink( ent );
}


//=============================================================================





/*
=================
GalaxyRP: [Grapple Hook] fire_grapple

Ported from TaystJK's fire_grapple (g_missile.c). The projectile is a point missile with the JAPro
wire signature -- ET_MISSILE, s.weapon WP_BRYAR_PISTOL, s.saberInFlight set -- which is what our
cgame (and a TaystJK build that honours TAYSTJK_INFO_GRAPPLE) keys the rope drawing on. Its own
classname rather than TaystJK's reused "laserTrap": that name is the trip mine's here, and the mine
code (RemoveLaserTraps, the ten-mine cap) would otherwise count and free the hook as one of them.

It never damages anything: G_HookImpact() takes it before the damage code, and ent->damage is left at
zero besides. Speed inherits g_hookInheritance of the owner's velocity along the shot, floored at
250 so a player running backwards cannot fire a hook that hangs in the air. Racemode, Tribes, haste
and the unlagged prestep are TaystJK's and are not here.
=================
*/
gentity_t *fire_grapple( gentity_t *self, vec3_t start, vec3_t dir )
{
	gentity_t	*hook;
	float		vel = g_hookSpeed.integer;

	VectorNormalize( dir );

	vel = vel + DotProduct( dir, self->client->ps.velocity ) * g_hookInheritance.value;

	if ( vel < 250 )
	{
		vel = 250;
	}

	hook = G_Spawn();
	hook->classname = RP_HOOK_CLASSNAME;
	hook->nextthink = level.time + 30000;
	hook->think = Weapon_HookFree;
	hook->s.eType = ET_MISSILE;
	hook->s.clientNum = self->s.clientNum;
	hook->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	hook->s.weapon = WP_BRYAR_PISTOL;
	hook->r.ownerNum = self->s.number;
	hook->s.owner = self->s.number;
	hook->methodOfDeath = MOD_UNKNOWN;
	hook->clipmask = MASK_SHOT;
	hook->parent = self;

	hook->s.pos.trType = TR_LINEAR;
	hook->s.pos.trTime = level.time;

	hook->s.otherEntityNum = ENTITYNUM_NONE;
	hook->s.groundEntityNum = ENTITYNUM_NONE;

	hook->s.saberInFlight = qtrue;

	VectorCopy( start, hook->s.pos.trBase );
	VectorScale( dir, vel, hook->s.pos.trDelta );
	SnapVector( hook->s.pos.trDelta );
	VectorCopy( start, hook->r.currentOrigin );

	self->client->hook = hook;

	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/weapons/melee/swing2.wav" ) );
	G_SetAnim( self, NULL, SETANIM_TORSO, BOTH_FORCEPUSH, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS, 0 );

	return hook;
}
