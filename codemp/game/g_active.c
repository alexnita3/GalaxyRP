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
#include "bg_saga.h"

extern void Jedi_Cloak( gentity_t *self );
extern void Jedi_Decloak( gentity_t *self );
extern void Jedi_DecloakPair( gentity_t *self );
extern qboolean Jedi_PairIsCloaked( gentity_t *self );

qboolean PM_SaberInTransition( int move );
qboolean PM_SaberInStart( int move );
qboolean PM_SaberInReturn( int move );
qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
qboolean saberCheckKnockdown_DuelLoss(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void P_SetTwitchInfo(gclient_t	*client)
{
	client->ps.painTime = level.time;
	client->ps.painDirection ^= 1;
}

/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the playerState_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD || client->tempSpectate >= level.time ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood + client->damage_armor;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 255 ) {
		count = 255;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		client->ps.damagePitch = angles[PITCH]/360.0 * 256;
		client->ps.damageYaw = angles[YAW]/360.0 * 256;

		//cap them since we can't send negative values in here across the net
		if (client->ps.damagePitch < 0)
		{
			client->ps.damagePitch = 0;
		}
		if (client->ps.damageYaw < 0)
		{
			client->ps.damageYaw = 0;
		}
	}

	// play an appropriate pain sound
	if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && !(player->s.eFlags & EF_DEAD) && (player->client->tempSpectate < level.time)) {

		// don't do more than two pain sounds a second
		// nmckenzie: also don't make him loud and whiny if he's only getting nicked.
		if ( level.time - client->ps.painTime < 500 || count < 10) {
			return;
		}
		P_SetTwitchInfo(client);
		player->pain_debounce_time = level.time + 700;

		G_AddEvent( player, EV_PAIN, player->health );
		client->ps.damageEvent++;

		if (client->damage_armor && !client->damage_blood)
		{
			client->ps.damageType = 1; //pure shields
		}
		else if (client->damage_armor)
		{
			client->ps.damageType = 2; //shields and health
		}
		else
		{
			client->ps.damageType = 0; //pure health
		}
	}


	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_knockback = 0;
}



/*
=============
P_SealedInVehicle

DAJ_RP: [Vehicles] true while a player rides a live walker or fighter -- exactly the riders G_Damage()
(g_combat.c) refuses damage to unless DAMAGE_NO_PROTECTION is passed, which drowning does not pass.
Kept as the same test on purpose: P_WorldEffects() asks it so that drowning and the damage it deals
cannot disagree about who is protected.
=============
*/
static qboolean P_SealedInVehicle( const gentity_t *ent )
{
	const gentity_t *veh;

	if ( !ent->client || ent->client->ps.clientNum >= MAX_CLIENTS || !ent->client->ps.m_iVehicleNum )
		return qfalse;

	veh = &g_entities[ent->client->ps.m_iVehicleNum];

	if ( !veh->m_pVehicle || veh->health <= 0 )
		return qfalse;

	return ( veh->m_pVehicle->m_pVehicleInfo->type == VH_WALKER
		|| veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER ) ? qtrue : qfalse;
}

/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
#ifdef BASE_COMPAT
	qboolean	envirosuit = qfalse;
#endif
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	#ifdef BASE_COMPAT
		envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;
	#endif // BASE_COMPAT

	//
	// check for drowning
	//
	// GalaxyRP: [dmflags] DF_NO_DROWN (64) switches drowning off by taking the "not under water"
	// branch below, which keeps the air supply topped up (so clearing the flag mid-dive still
	// gives the full 12 seconds) and resets the drowning damage ramp. Only drowning: lava and
	// slime below, and any trigger_hurt a map puts in its water, still hurt. (JA++'s version
	// returns from this function early, which switches lava and slime off as well.)
	//
	// DAJ_RP: [Vehicles] and a player sealed in a live walker or fighter (P_SealedInVehicle) takes the
	// same branch. G_Damage() already refused them the drowning damage, but the gurgle below is played
	// before it is called, so a submerged AT-ST or fighter pilot heard himself drown every second and
	// lost nothing -- while the damage ramp kept climbing, ready to hit at full strength the moment he
	// climbed out. Now his air stays topped up and the ramp reset, as with DF_NO_DROWN. Speeders and
	// animals do not seal their riders and still drown them; a destroyed vehicle stops protecting, in
	// G_Damage() and here alike.
	if ( waterlevel == 3 && !(dmflags.integer & DF_NO_DROWN) && !P_SealedInVehicle( ent ) ) {
		#ifdef BASE_COMPAT
			// envirosuit give air
			if ( envirosuit )
				ent->client->airOutTime = level.time + 10000;
		#endif // BASE_COMPAT

		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {
			// drown!
			ent->client->airOutTime += 1000;
			// GalaxyRP fix: [Dead Code] dropped a `!(amrpgmode==2 && rpg_class==3 && ...)` clause here;
			// pers.rpg_class is always 0 server-side, so the clause was tautologically always true.
			if ( ent->health > 0 && ent->client->tempSpectate < level.time)
			{ // zyk: Armored Soldier Upgrade protects from drowning
				// take more damage the longer underwater
				ent->damage += 2;
				if (ent->damage > 15)
					ent->damage = 15;

				// play a gurp sound instead of a normal pain sound
				if (ent->health <= ent->damage) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex(/*"*drown.wav"*/"sound/player/gurp1.wav"));
				} else if (rand()&1) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp1.wav"));
				} else {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp2.wav"));
				}

				// don't play a normal pain sound
				ent->pain_debounce_time = level.time + 200;

				G_Damage (ent, NULL, NULL, NULL, NULL,
					ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if ( waterlevel && (ent->watertype & (CONTENTS_LAVA|CONTENTS_SLIME)) )
	{
		if ( ent->health > 0 && ent->client->tempSpectate < level.time && ent->pain_debounce_time <= level.time )
		{
		#ifdef BASE_COMPAT
			if ( envirosuit )
				G_AddEvent( ent, EV_POWERUP_BATTLESUIT, 0 );
			else
		#endif
			{
				if ( ent->watertype & CONTENTS_LAVA )
					G_Damage( ent, NULL, NULL, NULL, NULL, 30*waterlevel, 0, MOD_LAVA );

				if ( ent->watertype & CONTENTS_SLIME )
					G_Damage( ent, NULL, NULL, NULL, NULL, 10*waterlevel, 0, MOD_SLIME );
			}
		}
	}
}





//==============================================================
extern void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback );
void DoImpact( gentity_t *self, gentity_t *other, qboolean damageSelf )
{
	float magnitude, my_mass;
	vec3_t	velocity;
	int cont;
	qboolean easyBreakBrush = qtrue;

	if( self->client )
	{
		VectorCopy( self->client->ps.velocity, velocity );
		if( !self->mass )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;
		}
	}
	else
	{
		VectorCopy( self->s.pos.trDelta, velocity );
		if ( self->s.pos.trType == TR_GRAVITY )
		{
			velocity[2] -= 0.25f * g_gravity.value;
		}
		if( !self->mass )
		{
			my_mass = 1;
		}
		else if ( self->mass <= 10 )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;///10;
		}
	}

	magnitude = VectorLength( velocity ) * my_mass / 10;

	/*
	if(pointcontents(self.absmax)==CONTENT_WATER)//FIXME: or other watertypes
		magnitude/=3;							//water absorbs 2/3 velocity

	if(self.classname=="barrel"&&self.aflag)//rolling barrels are made for impacts!
		magnitude*=3;

	if(self.frozen>0&&magnitude<300&&self.flags&FL_ONGROUND&&loser==world&&self.velocity_z<-20&&self.last_onground+0.3<time)
		magnitude=300;
	*/
	if ( other->material == MAT_GLASS
		|| other->material == MAT_GLASS_METAL
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8/*THIN*/))
		|| (other->r.svFlags&SVF_GLASS_BRUSH) )
	{
		easyBreakBrush = qtrue;
	}

	if ( !self->client || self->client->ps.lastOnGround+300<level.time || ( self->client->ps.lastOnGround+100 < level.time && easyBreakBrush ) )
	{
		vec3_t dir1, dir2;
		float force = 0, dot;

		if ( easyBreakBrush )
			magnitude *= 2;

		//damage them
		if ( magnitude >= 100 && other->s.number < ENTITYNUM_WORLD )
		{
			VectorCopy( velocity, dir1 );
			VectorNormalize( dir1 );
			if( VectorCompare( other->r.currentOrigin, vec3_origin ) )
			{//a brush with no origin
				VectorCopy ( dir1, dir2 );
			}
			else
			{
				VectorSubtract( other->r.currentOrigin, self->r.currentOrigin, dir2 );
				VectorNormalize( dir2 );
			}

			dot = DotProduct( dir1, dir2 );

			if ( dot >= 0.2 )
			{
				force = dot;
			}
			else
			{
				force = 0;
			}

			force *= (magnitude/50);

			cont = trap->PointContents( other->r.absmax, other->s.number );
			if( (cont&CONTENTS_WATER) )//|| (self.classname=="barrel"&&self.aflag))//FIXME: or other watertypes
			{
				force /= 3;							//water absorbs 2/3 velocity
			}

			/*
			if(self.frozen>0&&force>10)
				force=10;
			*/

			if( ( force >= 1 && other->s.number >= MAX_CLIENTS ) || force >= 10)
			{
	/*
				dprint("Damage other (");
				dprint(loser.classname);
				dprint("): ");
				dprint(ftos(force));
				dprint("\n");
	*/
				if ( other->r.svFlags & SVF_GLASS_BRUSH )
				{
					other->splashRadius = (float)(self->r.maxs[0] - self->r.mins[0])/4.0f;
				}
				if ( other->takedamage )
				{
					G_Damage( other, self, self, velocity, self->r.currentOrigin, force, DAMAGE_NO_ARMOR, MOD_CRUSH);//FIXME: MOD_IMPACT
				}
				else
				{
					G_ApplyKnockback( other, dir2, force );
				}
			}
		}

		if ( damageSelf && self->takedamage && !(self->flags & FL_NO_IMPACT_DMG) )
		{
			//Now damage me
			//FIXME: more lenient falling damage, especially for when driving a vehicle
			if ( self->client && self->client->ps.fd.forceJumpZStart )
			{//we were force-jumping
				if ( self->r.currentOrigin[2] >= self->client->ps.fd.forceJumpZStart )
				{//we landed at same height or higher than we landed
					magnitude = 0;
				}
				else
				{//FIXME: take off some of it, at least?
					magnitude = (self->client->ps.fd.forceJumpZStart-self->r.currentOrigin[2])/3;
				}
			}
			//if(self.classname!="monster_mezzoman"&&self.netname!="spider")//Cats always land on their feet
				if( ( magnitude >= 100 + self->health && self->s.number >= MAX_CLIENTS && self->s.weapon != WP_SABER ) || ( magnitude >= 700 ) )//&& self.safe_time < level.time ))//health here is used to simulate structural integrity
				{
					if ( (self->s.weapon == WP_SABER) && self->client && self->client->ps.groundEntityNum < ENTITYNUM_NONE && magnitude < 1000 )
					{//players and jedi take less impact damage
						//allow for some lenience on high falls
						magnitude /= 2;
						/*
						if ( self.absorb_time >= time )//crouching on impact absorbs 1/2 the damage
						{
							magnitude/=2;
						}
						*/
					}
					magnitude /= 40;
					magnitude = magnitude - force/2;//If damage other, subtract half of that damage off of own injury
					if ( magnitude >= 1 )
					{
		//FIXME: Put in a thingtype impact sound function
		/*
						dprint("Damage self (");
						dprint(self.classname);
						dprint("): ");
						dprint(ftos(magnitude));
						dprint("\n");
		*/
						/*
						if ( self.classname=="player_sheep "&& self.flags&FL_ONGROUND && self.velocity_z > -50 )
							return;
						*/
						G_Damage( self, NULL, NULL, NULL, self->r.currentOrigin, magnitude/2, DAMAGE_NO_ARMOR, MOD_FALLING );//FIXME: MOD_IMPACT
					}
				}
		}

		//FIXME: slow my velocity some?

		// NOTENOTE We don't use lastimpact as of yet
//		self->lastImpact = level.time;

		/*
		if(self.flags&FL_ONGROUND)
			self.last_onground=time;
		*/
	}
}

void Client_CheckImpactBBrush( gentity_t *self, gentity_t *other )
{
	if ( !other || !other->inuse )
	{
		return;
	}
	if (!self || !self->inuse || !self->client ||
		self->client->tempSpectate >= level.time ||
		self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //hmm.. let's not let spectators ram into breakables.
		return;
	}

	/*
	if (BG_InSpecialJump(self->client->ps.legsAnim))
	{ //don't do this either, qa says it creates "balance issues"
		return;
	}
	*/

	if ( other->material == MAT_GLASS
		|| other->material == MAT_GLASS_METAL
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8/*THIN*/))
		|| ((other->flags&FL_BBRUSH)&&(other->health<=10))
		|| (other->r.svFlags&SVF_GLASS_BRUSH) )
	{//clients only do impact damage against easy-break breakables
		DoImpact( self, other, qfalse );
	}
}


/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
	if (ent->client && ent->client->isHacking)
	{ //loop hacking sound
		ent->client->ps.loopSound = level.snd_hack;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedHealed > level.time)
	{ //loop healing sound
		ent->client->ps.loopSound = level.snd_medHealed;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedSupplied > level.time)
	{ //loop supplying sound
		ent->client->ps.loopSound = level.snd_medSupplied;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		ent->client->ps.loopSound = level.snd_fry;
		ent->s.loopIsSoundset = qfalse;
	} else if (ent->client) {
		ent->client->ps.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
	} else {
		ent->s.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
	}
}



//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pmove ) {
	int		i, j;
	trace_t	trace;
	gentity_t	*other;

	memset( &trace, 0, sizeof( trace ) );
	for (i=0 ; i<pmove->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pmove->touchents[j] == pmove->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pmove->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	static vec3_t	range = { 40, 40, 52 };

	if ( !ent->client ) {
		return;
	}

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->r.absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) {
			continue;
		}

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				// this is ugly but adding a new ET_? type will
				// most likely cause network incompatibilities
				hit->touch != Touch_DoorTrigger) {
				continue;
			}
		}

		// use seperate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		if ( hit->s.eType == ET_ITEM ) {
			if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time ) ) {
				continue;
			}
		} else {
			if ( !trap->EntityContact( mins, maxs, (sharedEntity_t *)hit, qfalse ) ) {
				continue;
			}
		}

		// DAJ_RP: [Locks] a trigger, a pickup, a door's trigger with a lock: a player whose account is
		// not on its list does not set it off (g_locks.c). Spectators only pass through teleporters
		// and doors, which they may. A trigger_multiple or trigger_once fired with the Use or fire button
		// (spawnflags 4, 8) is walked into freely, its use icon and all: Touch_Multi() refuses when the
		// button is pressed.
		if ( hit->touch && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
			!( hit->touch == Touch_Multi && ( hit->spawnflags & ( 4 | 8 ) ) ) && !RP_LockAllows( hit, ent, ent ) ) {
			continue;
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent = 0;
	}
}


/*
============
G_MoverTouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void G_MoverTouchPushTriggers( gentity_t *ent, vec3_t oldOrg )
{
	int			i, num;
	float		step, stepSize, dist;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs, dir, size, checkSpot;
	const vec3_t	range = { 40, 40, 52 };

	// non-moving movers don't hit triggers!
	if ( !VectorLengthSquared( ent->s.pos.trDelta ) )
	{
		return;
	}

	VectorSubtract( ent->r.mins, ent->r.maxs, size );
	stepSize = VectorLength( size );
	if ( stepSize < 1 )
	{
		stepSize = 1;
	}

	VectorSubtract( ent->r.currentOrigin, oldOrg, dir );
	dist = VectorNormalize( dir );
	for ( step = 0; step <= dist; step += stepSize )
	{
		VectorMA( ent->r.currentOrigin, step, dir, checkSpot );
		VectorSubtract( checkSpot, range, mins );
		VectorAdd( checkSpot, range, maxs );

		num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

		// can't use ent->r.absmin, because that has a one unit pad
		VectorAdd( checkSpot, ent->r.mins, mins );
		VectorAdd( checkSpot, ent->r.maxs, maxs );

		for ( i=0 ; i<num ; i++ )
		{
			hit = &g_entities[touch[i]];

			if ( hit->s.eType != ET_PUSH_TRIGGER )
			{
				continue;
			}

			if ( hit->touch == NULL )
			{
				continue;
			}

			if ( !( hit->r.contents & CONTENTS_TRIGGER ) )
			{
				continue;
			}


			if ( !trap->EntityContact( mins, maxs, (sharedEntity_t *)hit, qfalse ) )
			{
				continue;
			}

			memset( &trace, 0, sizeof(trace) );

			if ( hit->touch != NULL )
			{
				hit->touch(hit, ent, &trace);
			}
		}
	}
}

static void SV_PMTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask ) {
	trap->Trace( results, start, mins, maxs, end, passEntityNum, contentMask, qfalse, 0, 10 );
}

/*
=================
GalaxyRP: [Phase] Turn body collisions on/off for a player.
=================
*/
qboolean RP_PhasePassesThrough( const gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return qfalse;
	}

	return ( ent->client->pers.phase_mode != RP_PHASE_NONE || ent->client->pers.phase_releasing ) ? qtrue : qfalse;
}

/*
=================
RP_PlayerIsHidden / RP_HiddenApplyState

DAJ_RP: [Phase] /admhidden. A hidden player is walk-through like the other modes (RP_PhasePassesThrough()
above) and, on top of that, not drawn for anyone but himself -- done with Force Mind Trick's own fields.
A Mind Tricking player's entityState carries one bit per client he has tricked (trickedentindex for
clients 0-15, trickedentindex2 for 16-31, and so on), and every client -- the stock cgame included --
skips drawing him, his weapon, his saber, his shadow and the icons over his head, and the crosshair
name, when its own bit is set (CG_IsMindTricked(), cg_players.c). A viewer with Force Sight active
still sees him, by the same stock rule.

So a hidden player's s gets every client's bit except his own, so he still sees himself in third
person (and a spectator following him sees through his eyes as usual). Only s is touched, never
ps.fd.forceMindtrickTargetIndex*: his own Mind Trick keeps working and ending it cannot wipe these
bits, and bots -- which read the ps fields (BotMindTricked(), ai_main.c) -- keep targeting him. Nothing
on the server reads a player's s.trickedentindex*, so damage, targeting and collision are untouched.

s is rebuilt from ps by every BG_PlayerStateToEntityState*() call on a player, so this runs straight
after each of those: ClientThink_real() and ClientEndFrame() here, ClientSpawn() (g_client.c) and both
teleports (TeleportPlayer() in g_misc.c, zyk_TeleportPlayer() in g_main.c). CopyToBodyQue() uses it
for the corpse too. Players only; NPCs cannot be hidden. Not while spectating, when nothing is drawn.
=================
*/
qboolean RP_PlayerIsHidden( const gentity_t *ent )
{
	if ( !ent || !ent->client || ent->s.number < 0 || ent->s.number >= MAX_CLIENTS )
	{
		return qfalse;
	}

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		return qfalse;
	}

	return ( ent->client->pers.phase_mode == RP_PHASE_HIDDEN ) ? qtrue : qfalse;
}

void RP_HiddenApplyState( const gentity_t *player, entityState_t *s )
{
	int i;

	if ( !s || !RP_PlayerIsHidden( player ) )
	{
		return;
	}

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		if ( i == player->s.number )
		{ // he sees himself
			continue;
		}

		if ( i < 16 )
			s->trickedentindex |= ( 1 << i );
		else if ( i < 32 )
			s->trickedentindex2 |= ( 1 << ( i - 16 ) );
		else if ( i < 48 )
			s->trickedentindex3 |= ( 1 << ( i - 32 ) );
		else
			s->trickedentindex4 |= ( 1 << ( i - 48 ) );
	}
}

static int rp_phaseHidden[MAX_GENTITIES];
static int rp_phaseHiddenCount = 0;

// GalaxyRP: [Phase] the NPCs /npc effect has given a mode, plus any still finishing a release. Kept
// as a short list so the per-Pmove scan below never has to walk every entity slot. An entry is only
// a hint: every reader re-checks the slot (in use, still an NPC, still passing through), and
// RP_PhaseNpcEndFrame() drops entries that no longer are -- a freed slot is zeroed by G_FreeEntity,
// and a new NPC in a reused slot starts with a zeroed client (NPC_Spawn_Do), so neither can inherit
// a mode.
static int rp_phasedNpcs[MAX_GENTITIES];
static int rp_phasedNpcCount = 0;

static qboolean RP_PhaseIsLiveNpc( const gentity_t *ent )
{
	return ( ent->inuse && ent->client && ent->s.eType == ET_NPC ) ? qtrue : qfalse;
}

// Adds an NPC to the list; called by /npc effect whenever it gives an NPC a mode or starts a release.
void RP_PhaseTrackNpc( gentity_t *npc )
{
	int i, num;

	if ( !npc || !RP_PhaseIsLiveNpc( npc ) )
	{
		return;
	}

	num = (int)( npc - g_entities );

	for ( i = 0; i < rp_phasedNpcCount; i++ )
	{
		if ( rp_phasedNpcs[i] == num )
		{
			return;
		}
	}

	if ( rp_phasedNpcCount < MAX_GENTITIES )
	{
		rp_phasedNpcs[rp_phasedNpcCount++] = num;
	}
}

/*
=================
RP_StartOverlapRelease

GalaxyRP fix: [Telefrag] let a player or NPC that has just arrived inside another live body walk out of
it. Used by G_KillBox() (g_utils.c) for an arrival that could not -- or must not -- telefrag what it
landed on: an NPC arriving on a player, or a victim the damage never reached (an ally, chat
protection, a private duel, the arena rules, noclip, an invulnerable NPC...). Without it both bodies
stood inside each other and neither could move.

It is the tail end of a phase mode (RP_ClearPhaseMode, g_cmds.c) with no mode in front of it: the
arriver passes through bodies and is sent as walk-through while it overlaps anyone, looks normal
throughout, and RP_PhaseUpdate() ends the release by itself on the first end of frame that finds
nothing solid overlapping it. An NPC is added to the phased-NPC list, which is what runs that
check for it. A body already in a phase mode passes through anyway and is left exactly as it is.
=================
*/
void RP_StartOverlapRelease( gentity_t *ent )
{
	if ( !ent || !ent->inuse || !ent->client )
	{
		return;
	}

	if ( ent->client->pers.phase_mode != RP_PHASE_NONE )
	{
		return;
	}

	ent->client->pers.phase_releasing = qtrue;

	if ( ent->s.eType == ET_NPC )
	{
		RP_PhaseTrackNpc( ent );
	}
}

static void RP_PhaseHideBody( gentity_t *other, const gentity_t *mover )
{
	if ( other == mover || !other->inuse || !other->client )
		return;
	if ( !RP_PhasePassesThrough( other ) )
		return;
	if ( other->r.contents != CONTENTS_BODY )
		return;

	other->r.contents = 0;
	rp_phaseHidden[rp_phaseHiddenCount++] = (int)( other - g_entities );
}

// Only live bodies are touched: a corpse (CONTENTS_CORPSE), a rider a vehicle has Ghost()ed
// (contents 0) and a spectator are left exactly as they are. Players are scanned by slot; NPCs come
// from the phased-NPC list, since /npc effect is the only way an NPC gets a mode.
static void RP_PhaseHideBodies( const gentity_t *mover )
{
	int i;

	rp_phaseHiddenCount = 0;

	for ( i = 0; i < level.maxclients; i++ )
	{
		gentity_t *other = &g_entities[i];

		if ( other->inuse && other->client && other->client->pers.connected != CON_CONNECTED )
			continue;

		RP_PhaseHideBody( other, mover );
	}

	for ( i = 0; i < rp_phasedNpcCount; i++ )
	{
		gentity_t *other = &g_entities[rp_phasedNpcs[i]];

		if ( !RP_PhaseIsLiveNpc( other ) )
			continue;

		RP_PhaseHideBody( other, mover );
	}
}

// Restores only what RP_PhaseHideBodies cleared, and only if it is still cleared: if something
// during the Pmove changed a hidden body's contents -- a vehicle impact that killed it and turned it
// into CONTENTS_CORPSE -- that newer value is left alone.
static void RP_PhaseRestoreBodies( void )
{
	int i;

	for ( i = 0; i < rp_phaseHiddenCount; i++ )
	{
		gentity_t *other = &g_entities[rp_phaseHidden[i]];

		if ( other->r.contents == 0 )
		{
			other->r.contents = CONTENTS_BODY;
		}
	}

	rp_phaseHiddenCount = 0;
}

// Whether this live body overlaps another live body it would collide with once solid again: a
// player, bot, NPC or vehicle with CONTENTS_BODY that is not itself passing through. Tested on the
// real boxes (currentOrigin + mins/maxs) rather than on r.absmin/absmax, which SV_LinkEntity widens
// by one unit -- standing next to someone is not standing inside them.
static qboolean RP_PhaseOverlapsBody( gentity_t *ent )
{
	int touch[MAX_GENTITIES];
	int num, i, j;
	vec3_t mins, maxs;

	if ( ent->r.contents != CONTENTS_BODY )
	{ // dead, ghosted in a vehicle or spectating: nothing to collide with
		return qfalse;
	}

	VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );

	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for ( i = 0; i < num; i++ )
	{
		gentity_t *other = &g_entities[touch[i]];
		vec3_t omins, omaxs;
		qboolean overlaps = qtrue;

		if ( other == ent || !other->inuse || !other->client )
			continue;
		if ( !(other->r.contents & CONTENTS_BODY) )
			continue;
		if ( RP_PhasePassesThrough( other ) )
			continue;

		VectorAdd( other->r.currentOrigin, other->r.mins, omins );
		VectorAdd( other->r.currentOrigin, other->r.maxs, omaxs );

		for ( j = 0; j < 3; j++ )
		{
			if ( omins[j] >= maxs[j] || omaxs[j] <= mins[j] )
			{
				overlaps = qfalse;
				break;
			}
		}

		if ( overlaps )
		{
			return qtrue;
		}
	}

	return qfalse;
}

// Once per frame per client (ClientEndFrame): finish a release once nothing overlaps, then write the
// mode the clients see. A release in progress is sent as RP_PHASE_NONSOLID -- the player already
// looks normal, but their own prediction must keep passing through bodies until the server does.
static void RP_PhaseUpdate( gentity_t *ent )
{
	gclient_t *client = ent->client;
	int sentMode;

	if ( client->pers.phase_releasing )
	{
		if ( client->pers.phase_mode != RP_PHASE_NONE || !RP_PhaseOverlapsBody( ent ) )
		{
			client->pers.phase_releasing = qfalse;
		}
	}

	sentMode = client->pers.phase_mode;

	// DAJ_RP: [Phase] hidden has no value of its own on the wire (the two bits are full): clients only
	// need to know to walk through the player, and the hiding goes by RP_HiddenApplyState() instead
	if ( sentMode == RP_PHASE_HIDDEN )
	{
		sentMode = RP_PHASE_NONSOLID;
	}

	if ( sentMode == RP_PHASE_NONE && client->pers.phase_releasing )
	{
		sentMode = RP_PHASE_NONSOLID;
	}

	client->ps.eFlags &= ~EF_RP_PHASE_MASK;
	client->ps.eFlags |= ( sentMode << EF_RP_PHASE_SHIFT ) & EF_RP_PHASE_MASK;
}

/*
=================
RP_PhaseNpcEndFrame

GalaxyRP: [Phase] ClientEndFrame() for the NPCs /npc effect has phased -- NPCs never reach
ClientEndFrame(), which only walks player slots. Called from G_RunFrame() straight after that loop,
once every think of the frame is done, so it does for each listed NPC exactly what ClientEndFrame()
does for a phased player:

  - RP_PhaseUpdate(): finish a release that has come clear, and write the mode into ps.eFlags;
  - the same bits into s.eFlags at once, since an NPC's ClientThink_real() -- where its state is
    normally copied from ps -- does not necessarily run every server frame;
  - s.solid 0 for a live phased body, so every client predicts walking through it. Nothing links an
    NPC after this point in the frame, so it holds until the snapshot.

An NPC whose mode and release are both over is written back to normal once more and then dropped
from the list; so is a slot that is no longer an NPC at all.
=================
*/
void RP_PhaseNpcEndFrame( void )
{
	int i = 0;

	while ( i < rp_phasedNpcCount )
	{
		gentity_t *npc = &g_entities[rp_phasedNpcs[i]];
		qboolean wasPassing;

		if ( !RP_PhaseIsLiveNpc( npc ) )
		{ // freed, or reused by something else: nothing of ours left on it
			rp_phasedNpcs[i] = rp_phasedNpcs[--rp_phasedNpcCount];
			continue;
		}

		wasPassing = RP_PhasePassesThrough( npc );

		RP_PhaseUpdate( npc );

		npc->s.eFlags &= ~EF_RP_PHASE_MASK;
		npc->s.eFlags |= ( npc->client->ps.eFlags & EF_RP_PHASE_MASK );

		if ( !RP_PhasePassesThrough( npc ) )
		{ // back to normal for good. If this frame is the one that ended it, relink so s.solid is
		  // rebuilt from r.contents now rather than at the NPC's next think -- but only an NPC that is
		  // in the world already, and never a stale entry (a new NPC in a reused slot), which was not
		  // passing through to begin with.
			if ( wasPassing && npc->r.linked )
			{
				trap->LinkEntity( (sharedEntity_t *)npc );
			}
			rp_phasedNpcs[i] = rp_phasedNpcs[--rp_phasedNpcCount];
			continue;
		}

		if ( npc->r.contents == CONTENTS_BODY )
		{
			npc->s.solid = 0;
		}

		i++;
	}
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pmove;
	gclient_t	*client;

	client = ent->client;

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		client->ps.pm_type = PM_SPECTATOR;
		client->ps.speed = 400;	// faster than normal
		client->ps.basespeed = 400;
		client->ps.forceHandExtend = rp_pluginRequired.integer == 2 && !client->pers.clientPlugin ? HANDEXTEND_KNOCKDOWN : HANDEXTEND_NONE; // Tr!Force: [Plugin] Don't allow user actions

		//hmm, shouldn't have an anim if you're a spectator, make sure
		//it gets cleared.
		client->ps.legsAnim = 0;
		client->ps.legsTimer = 0;
		client->ps.torsoAnim = 0;
		client->ps.torsoTimer = 0;

		// set up for pmove
		memset (&pmove, 0, sizeof(pmove));
		pmove.ps = &client->ps;
		pmove.cmd = *ucmd;
		pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pmove.trace = SV_PMTrace;
		pmove.pointcontents = trap->PointContents;

		pmove.noSpecMove = g_noSpecMove.integer;

		pmove.animations = NULL;
		pmove.nonHumanoid = qfalse;

		//Set up bg entity data
		pmove.baseEnt = (bgEntity_t *)g_entities;
		pmove.entSize = sizeof(gentity_t);

		// perform a pmove
		Pmove (&pmove);
		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		if (ent->client->tempSpectate < level.time)
		{
			G_TouchTriggers( ent );
		}
		trap->UnlinkEntity( (sharedEntity_t *)ent );
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

	if (client->tempSpectate < level.time)
	{
		// attack button cycles through spectators
		if ( (client->buttons & BUTTON_ATTACK) && !(client->oldbuttons & BUTTON_ATTACK) )
			Cmd_FollowCycle_f( ent, 1 );

		else if ( client->sess.spectatorState == SPECTATOR_FOLLOW && (client->buttons & BUTTON_ALT_ATTACK) && !(client->oldbuttons & BUTTON_ALT_ATTACK) )
			Cmd_FollowCycle_f( ent, -1 );

		if (client->sess.spectatorState == SPECTATOR_FOLLOW && (ucmd->upmove > 0))
		{ //jump now removes you from follow mode
			StopFollowing(ent);
		}
	}
}



/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped
=================
*/
qboolean ClientInactivityTimer( gclient_t *client ) {
	if ( ! g_inactivity.integer ) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		client->inactivityTime = level.time + 60 * 1000;
		client->inactivityWarning = qfalse;
	} else if ( client->pers.cmd.forwardmove ||
		client->pers.cmd.rightmove ||
		client->pers.cmd.upmove ||
		(client->pers.cmd.buttons & (BUTTON_ATTACK|BUTTON_ALT_ATTACK)) ) {
		client->inactivityTime = level.time + g_inactivity.integer * 1000;
		client->inactivityWarning = qfalse;
	} else if ( !client->pers.localClient ) {
		if ( level.time > client->inactivityTime ) {
			trap->DropClient( client - level.clients, "Dropped due to inactivity" );
			return qfalse;
		}
		if ( level.time > client->inactivityTime - 10000 && !client->inactivityWarning ) {
			client->inactivityWarning = qtrue;
			trap->SendServerCommand( client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"" );
		}
	}
	return qtrue;
}

//GalaxyRP (Alex): [Ammo Recharge] Recharge player's ammo based on a timer. Bases on rp_ammo_regen_timer and rp_explosives_recharge_timer cvars.
extern void Add_Ammo (gentity_t *ent, int weapon, int count);
void RegenerateAmmo(gentity_t* ent, gclient_t* client) {
	if (ent->client->sess.loggedin == qtrue) {
		if (rp_allow_ammo_regen.integer > 0) {
			if (ent->client->sess.weapon_recharge_timer >= rp_ammo_regen_timer.integer) {
				for (int i = 39; i <= 41; i++) {

					switch (i)
					{
					case 39:
						Add_Ammo(ent, AMMO_BLASTER, 1 * ent->client->pers.skill_levels[i]);
						break;
					case 40:
						Add_Ammo(ent, AMMO_POWERCELL, 1 * ent->client->pers.skill_levels[i]);
						break;
					case 41:
						Add_Ammo(ent, AMMO_METAL_BOLTS, 1 * ent->client->pers.skill_levels[i]);
						break;
					default:
						break;
					}
				}
				ent->client->sess.weapon_recharge_timer = 0;
			}
			ent->client->sess.weapon_recharge_timer++;
		}
		if (rp_allow_explosives_regen.integer > 0) {
			if (ent->client->sess.explosive_recharge_timer >= rp_explosives_recharge_timer.integer) {
				for (int i = 42; i <= 45; i++) {
					switch (i)
					{
					case 42:
						Add_Ammo(ent, AMMO_ROCKETS, 1 * ent->client->pers.skill_levels[i]);
						break;
					case 43:
						Add_Ammo(ent, AMMO_THERMAL, 1 * ent->client->pers.skill_levels[i]);
						break;
					case 44:
						Add_Ammo(ent, AMMO_TRIPMINE, 1 * ent->client->pers.skill_levels[i]);
						break;
					case 45:
						Add_Ammo(ent, AMMO_DETPACK, 1 * ent->client->pers.skill_levels[i]);
						break;
					default:
						break;
					}

				}
				ent->client->sess.explosive_recharge_timer = 0;
			}
			ent->client->sess.explosive_recharge_timer++;
		}
	}
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
/*
==============
RP_DownedTimerTick

One second of a downed player's countdown: the on-screen timer, the decrement, and the auto-release
that ends an admin paralysis once its time is served.

GalaxyRP fix: [Death System] split out of ClientTimerActions(), which is only ever reached by a
player who is alive and in the world: ClientThink_real() returns into ClientIntermissionThink() and
into SpectatorThink() long before it, and returns again just above it for a player waiting to
respawn. A downed player who became a spectator therefore froze their own countdown, and an admin
paralysis never expired -- the target could sit the entire punishment out as a free-flying camera and
rejoin still paralysed with the same time remaining. Being downed is state that lives in pers and
survives a respawn; its countdown is served in wall-clock seconds wherever the player happens to be.

RP_RunDownedTimer() below is the one driver, called from ClientEndFrame() which runs once per server
frame for every connected client -- spectators, followers and intermission included -- and paced by
the server frame delta rather than by a client's usercmd timing. See downedTimeResidual in g_local.h
for why msec was the wrong clock for this.
==============
*/
static void RP_DownedTimerTick( gentity_t *ent )
{
	gclient_t *client;

	if ( !ent || !ent->client )
	{
		return;
	}

	client = ent->client;

	//GalaxyRP (Alex): [Death System] This timer represents the time that a player has left until they can get up from being downed.
	//GalaxyRP (Alex): [Death System] Not paralyzed anymore, means someone else helped you (or you helped yourself with admin permission. Set downed timer to 0 to remove message.
	// GalaxyRP fix: [Death System] hoisted above the display. It used to run after it, which was
	// harmless only while downedTime lived outside pers and was wiped by every ClientSpawn. Now
	// that it survives a respawn alongside the status bits, a player finished off while downed --
	// player_die() clears the bits, then they respawn -- would have seen one stale "You are
	// downed. Time Remaining: N" a second later, while alive and well. Clearing first keeps the
	// countdown and the bits in agreement at every point the message can be sent.
	if (!G_PlayerIsDowned(ent)) {
		client->pers.downedTime = 0;
	}

	if (client->pers.downedTime)
	{
		// GalaxyRP fix: [Death System] this used to be gated on "downedTime <= rp_downed_timer",
		// which assumed the countdown could never exceed the cvar. /paralyze can now set any
		// duration up to 900s, so with the shipped rp_downed_timer of 30 an admin paralysis showed
		// the player nothing at all until its last 30 seconds -- immobile, with no explanation and
		// no timer. downedTime is the authoritative remaining time, so it is simply displayed.
		// GalaxyRP fix: [Death System] an admin paralysis says so, and does not advertise /getup or
		// /helpup, because neither can end one.
		if (G_PlayerIsAdminParalyzed(ent))
		{
			trap->SendServerCommand(ent->s.number, va("cp \"^1You were paralyzed by an admin.\nTime Remaining: %d\"", client->pers.downedTime));
		}
		else
		{
			trap->SendServerCommand(ent->s.number, va("cp \"^1You are downed.\nTime Remaining: %d\"", client->pers.downedTime));
		}

		client->pers.downedTime--;

		// GalaxyRP fix: [Death System] an admin paralysis releases itself the moment its time is
		// served. A combat knockdown does not -- there the countdown only unlocks /getup and the
		// player chooses when to stand -- but /getup and /helpup both refuse an admin paralysis,
		// so without this the target would stay down forever once the timer ran out.
		if (client->pers.downedTime <= 0 && G_PlayerIsAdminParalyzed(ent)) {
			RP_ReleaseFromDownedState(ent);
			trap->SendServerCommand(ent->s.number, "print \"^2Your paralysis has worn off.\n\"");
			trap->SendServerCommand(ent->s.number, "cp \"^2Your paralysis has worn off.\"");
		}
	}
	else {
		//GalaxyRP (Alex): [Death System] Can get up by themselves, but haven't yet.
		// An admin paralysis never reaches here -- it auto-releases above the moment its countdown
		// hits 0 -- but the check keeps this prompt from ever naming /getup and /helpup to someone
		// they cannot help.
		// GalaxyRP fix: [Death System] and not while spectating, now that this tick also runs on
		// the spectator path: /getup carries CMD_ALIVE and refuses a spectator outright, so
		// naming it to one would be advertising a command that cannot work.
		if (G_PlayerIsDowned(ent) && !G_PlayerIsAdminParalyzed(ent) &&
			client->sess.sessionTeam != TEAM_SPECTATOR && client->tempSpectate < level.time){
			trap->SendServerCommand(ent->s.number, va("cp \"^1You are downed.\n^2You may get up by using ^3/getup\n^2Alternatively, someone else can do ^3/helpup ^3%s.\"", client->pers.netname_nocolor));
		}
	}
}

/*
==============
RP_RunDownedTimer

Paces RP_DownedTimerTick() at 1Hz off the server frame clock. Called from ClientEndFrame(), which
G_RunFrame() runs exactly once per server frame for every in-use client slot, above the point where
spectators are split off -- so this reaches a player in the world, a free-flying spectator, a
follower and a client sitting in intermission alike, and reaches each of them at the same rate.

The frame delta is clamped: a single pathological delta (a server hitch, or a level.time that jumped)
must not turn the catch-up loop into thousands of iterations, each sending a server command.

ClientSpawn() calls ClientEndFrame() directly as part of settling a respawn, and SpectatorClientEndFrame()
can reach ClientBegin() -> ClientSpawn() and so nest one inside another. Those add at most an extra
frame delta or two to the accumulator on a respawn -- bounded, one-off, and far below the one-second
resolution this counts in.
==============
*/
static void RP_RunDownedTimer( gentity_t *ent )
{
	int frameMsec;

	if ( !ent || !ent->client || ent->s.number >= MAX_CLIENTS )
	{
		return;
	}

	if ( !G_PlayerIsDowned( ent ) && !ent->client->pers.downedTime )
	{ // not downed and nothing stale left over -- keep the accumulator clean and do nothing
		ent->client->downedTimeResidual = 0;
		return;
	}

	frameMsec = level.time - level.previousTime;

	if ( frameMsec < 1 )
	{
		return;
	}

	if ( frameMsec > 1000 )
	{ // see the note above: bound the loop below to a single pass
		frameMsec = 1000;
	}

	ent->client->downedTimeResidual += frameMsec;

	while ( ent->client->downedTimeResidual >= 1000 )
	{
		ent->client->downedTimeResidual -= 1000;
		RP_DownedTimerTick( ent );
	}
}

/*
===============================================================================
GLX_U: [AFK]

A player with no input for rp_afk_time seconds (or for the /afk countdown) is AFK: the spawn protection
bubble (EF_INVULNERABLE, drawn by every client; G_Damage refuses players and NPCs) and the comlink
animation, held the way emotes are. The first input ends it. An open chat box sends no input, so a
long message counts as idle; BUTTON_TALK (also set while alt-tabbed) and BUTTON_ANY are ignored.
===============================================================================
*/
#define RP_AFK_BUBBLE_MS		1500	// how far ahead the bubble's invulnerableTimer is kept
#define RP_AFK_ANIM_START_MS	800		// BOTH_TALKCOMM1START: 16 frames at 20 fps

static qboolean RP_IsAfk( const gentity_t *ent ) {
	return ( ent->client->pers.player_statuses & ( 1 << PLAYER_STATUS_AFK ) ) ? qtrue : qfalse;
}

qboolean RP_CanGoAfk( gentity_t *ent ) {
	gclient_t *cl = ent ? ent->client : NULL;

	if ( !cl || ent->NPC || ( ent->r.svFlags & SVF_BOT ) || ent->s.number >= MAX_CLIENTS )
		return qfalse;
	if ( level.intermissiontime || cl->pers.connected != CON_CONNECTED )
		return qfalse;
	if ( cl->sess.sessionTeam == TEAM_SPECTATOR || ( cl->ps.pm_flags & PMF_FOLLOW ) || cl->tempSpectate >= level.time )
		return qfalse;
	if ( ent->health <= 0 || cl->ps.stats[STAT_HEALTH] <= 0 || cl->ps.pm_type == PM_DEAD || ( cl->ps.eFlags & EF_DEAD ) )
		return qfalse;
	if ( cl->pers.player_statuses & ( ( 1 << PLAYER_STATUS_DOWNED ) | ( 1 << PLAYER_STATUS_ADMIN_PARALYSIS ) ) )
		return qfalse;
	if ( cl->ps.m_iVehicleNum || cl->ps.duelInProgress || RP_MinigameFighter( ent ) )
		return qfalse;
	return qtrue;
}

static void RP_AfkEnter( gentity_t *ent, qboolean announce ) {
	gclient_t *cl = ent->client;

	cl->pers.player_statuses |= ( 1 << PLAYER_STATUS_AFK );
	cl->pers.afkRequestAt = 0;
	cl->pers.afkStart = level.time;
	// spawn or revive protection still running comes back when the player does
	cl->pers.afkSavedInvulnerable = ( cl->ps.eFlags & EF_INVULNERABLE ) ? cl->invulnerableTimer : 0;
	cl->ps.eFlags |= EF_INVULNERABLE;
	cl->invulnerableTimer = cl->pers.afkBubbleUntil = level.time + RP_AFK_BUBBLE_MS;
	// an emote or meditation the player is in stays: the bubble is added to it
	cl->pers.afkOwnAnim = ( !( cl->pers.player_statuses & ( 1 << PLAYER_STATUS_EMOTE ) ) && cl->ps.forceHandExtend != HANDEXTEND_TAUNT ) ? qtrue : qfalse;

	if ( announce )
		trap->SendServerCommand( ent - g_entities, "print \"You are AFK. Move, look around or press a key to come back.\n\"" );
}

// each frame while AFK: keep the bubble and the comlink animation up
static void RP_AfkHold( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	cl->ps.eFlags |= EF_INVULNERABLE;
	cl->invulnerableTimer = cl->pers.afkBubbleUntil = level.time + RP_AFK_BUBBLE_MS;

	if ( cl->pers.afkOwnAnim ) {
		cl->ps.forceHandExtend = HANDEXTEND_TAUNT;
		cl->ps.forceDodgeAnim = ( level.time - cl->pers.afkStart < RP_AFK_ANIM_START_MS ) ? BOTH_TALKCOMM1START : BOTH_TALKCOMM1;
		cl->ps.forceHandExtendTime = level.time + 1000;
	}
}

void RP_AfkLeave( gentity_t *ent ) {
	gclient_t *cl = ent ? ent->client : NULL;

	if ( !cl || !RP_IsAfk( ent ) )
		return;

	cl->pers.player_statuses &= ~( 1 << PLAYER_STATUS_AFK );

	// the bubble is still ours (an attack, a revive or a respawn did not replace it): put back what was there
	if ( cl->invulnerableTimer == cl->pers.afkBubbleUntil ) {
		if ( cl->pers.afkSavedInvulnerable > level.time ) {
			cl->invulnerableTimer = cl->pers.afkSavedInvulnerable;
		}
		else {
			cl->ps.eFlags &= ~EF_INVULNERABLE;
			cl->invulnerableTimer = 0;
		}
	}

	if ( cl->pers.afkOwnAnim && cl->ps.forceHandExtend == HANDEXTEND_TAUNT &&
		( cl->ps.forceDodgeAnim == BOTH_TALKCOMM1START || cl->ps.forceDodgeAnim == BOTH_TALKCOMM1 ) ) {
		cl->ps.forceHandExtend = HANDEXTEND_NONE;
		cl->ps.forceHandExtendTime = level.time;
	}

	cl->pers.afkOwnAnim = qfalse;
	cl->pers.afkBubbleUntil = 0;
	cl->pers.afkSavedInvulnerable = 0;
}

// ClientSpawn: nothing AFK carries over, and the idle clock starts now
void RP_AfkReset( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	cl->pers.player_statuses &= ~( 1 << PLAYER_STATUS_AFK );
	cl->pers.afkLastActive = level.time;
	cl->pers.afkRequestAt = 0;
	cl->pers.afkStart = 0;
	cl->pers.afkSavedInvulnerable = 0;
	cl->pers.afkBubbleUntil = 0;
	cl->pers.afkOwnAnim = qfalse;
}

// every usercmd, before Pmove, so the first input after AFK is not held by the animation
static void RP_AfkThink( gentity_t *ent, usercmd_t *ucmd ) {
	gclient_t *cl = ent->client;
	qboolean input;

	if ( !cl || ent->NPC || ( ent->r.svFlags & SVF_BOT ) || ent->s.number >= MAX_CLIENTS )
		return;

	input = ( ucmd->forwardmove || ucmd->rightmove || ucmd->upmove ) ? qtrue : qfalse;
	// a bound key held. Not BUTTON_TALK (chat box, console, alt-tab) and not BUTTON_ANY: the key that sends
	// a chat line or closes the console is still down for a moment after, and would cancel /afk at once
	if ( ucmd->buttons & ~( BUTTON_TALK | BUTTON_WALKING | BUTTON_ANY ) )
		input = qtrue;
	if ( ( ucmd->buttons ^ cl->pers.afkPrevButtons ) & BUTTON_WALKING )	// walk toggled
		input = qtrue;
	if ( ucmd->weapon != cl->pers.afkPrevWeapon ||
		ucmd->angles[0] != cl->pers.afkPrevAngles[0] || ucmd->angles[1] != cl->pers.afkPrevAngles[1] ||
		ucmd->angles[2] != cl->pers.afkPrevAngles[2] )
		input = qtrue;

	cl->pers.afkPrevButtons = ucmd->buttons;
	cl->pers.afkPrevWeapon = ucmd->weapon;
	cl->pers.afkPrevAngles[0] = ucmd->angles[0];
	cl->pers.afkPrevAngles[1] = ucmd->angles[1];
	cl->pers.afkPrevAngles[2] = ucmd->angles[2];

	if ( input ) {
		cl->pers.afkLastActive = level.time;
		if ( cl->pers.afkRequestAt ) {
			cl->pers.afkRequestAt = 0;
			trap->SendServerCommand( ent - g_entities, "print \"AFK cancelled.\n\"" );
		}
		RP_AfkLeave( ent );
		return;
	}

	if ( RP_IsAfk( ent ) ) {
		if ( rp_afk_time.integer <= 0 || !RP_CanGoAfk( ent ) )
			RP_AfkLeave( ent );
		else
			RP_AfkHold( ent );
	}
}

// once a second, from ClientTimerActions
static void RP_AfkTick( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	if ( ent->NPC || ( ent->r.svFlags & SVF_BOT ) || ent->s.number >= MAX_CLIENTS || RP_IsAfk( ent ) )
		return;

	if ( rp_afk_time.integer <= 0 || !RP_CanGoAfk( ent ) ) {
		cl->pers.afkRequestAt = 0;		// a pending /afk that can no longer go off is dropped quietly
		return;
	}

	if ( cl->pers.afkRequestAt && level.time >= cl->pers.afkRequestAt )
		RP_AfkEnter( ent, qtrue );
	else if ( level.time - cl->pers.afkLastActive >= rp_afk_time.integer * 1000 )
		RP_AfkEnter( ent, qfalse );
}

void ClientTimerActions( gentity_t *ent, int msec ) {
	gclient_t	*client;
     	char 		serverMotd[MAX_STRING_CHARS];

	client = ent->client;
	client->timeResidual += msec;

	// GalaxyRP: [rp_loginRequired] When enabled, players who are not logged into an account must
	// stay in Spectator mode -- this catches both a freshly connected player who never logged in
	// (sess.loggedin defaults to qfalse) and a player who /logout's while already playing, without
	// needing a dedicated hook in Cmd_LogoutAccount_f, since this runs on every think for every
	// non-spectator client. Adapted from the New Zyk mod's zyk_force_account_login (checked once
	// per server frame in G_RunFrame there); here it's checked once per client think instead, which
	// is this codebase's closest equivalent per-player periodic tick. Bots/NPCs are exempt since
	// they never log into an account. This stacks safely with rp_pluginRequired: each cvar's
	// enforcement is independent, so a player must satisfy both requirements (plugin present AND
	// logged in) to stay off Spectator -- whichever one they fail keeps forcing them back.
	if (rp_loginRequired.integer && !ent->NPC && !(ent->r.svFlags & SVF_BOT) &&
		!client->sess.loggedin && client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		// GalaxyRP fix: [rp_loginRequired] say why. This used to move the player to Spectator without
		// a word, so anyone joining a team with the cvar on was yanked back a second later with no
		// explanation and no idea what to do about it -- rp_pluginRequired at least prints a reason.
		// Deliberately a console print, not a centerprint: a "cp" replaces whatever is already on
		// screen, and rp_pluginRequired's own warning is a cp, so a second one would wipe it for a
		// player who fails both checks. Console lines stack, so the two coexist.
		//
		// Fires once per join attempt rather than once per second: reaching this point at all means
		// the player is a live non-spectator, and SetTeam() below cannot refuse a Spectator request
		// from here -- its only such refusal is the Siege "tempSpectate >= level.time" guard, and a
		// client in tempSpectate never reaches ClientTimerActions(), because ClientThink_real()
		// returns into SpectatorThink() above it on that same condition. So the move always lands,
		// this function stops running for them, and the message is not repeated until they try again.
		//
		// Both commands named here work from Spectator -- /login carries no dispatch flags at all and
		// /new only CMD_NOINTERMISSION -- so the player can act on this without leaving the state it
		// puts them in.
		trap->SendServerCommand(ent->s.number, "print \"^3You must be logged in to join the game. Use ^2/login <name> <password>^3, or ^2/new <name> <password>^3 to create an account.\n\"");

		SetTeam(ent, "spectator");
		return;
	}

	while ( client->timeResidual >= 1000 )
	{
		client->timeResidual -= 1000;

		//GalaxyRP (Alex): [Ammo Recharge] Go check if player can recharge ammo, do it if they can.
		RegenerateAmmo(ent, client);

		// GalaxyRP fix: [gameplay/exploit] the PLAYER_STATUS_CHAT_PROTECTION bit makes G_Damage return early for this
		// player (g_combat.c), so leaving it set means total damage immunity. Both the set and the
		// clear used to live inside the "rp_chat_protection_timer > 0" block, and the clear also
		// required chat_protection_timer != 0 -- so the flag could be stranded set, permanently, in
		// two different ways. (1) An admin setting the cvar to 0 (documented as "0 to disable") while
		// someone was currently protected: the whole block stops running, so nothing can ever clear
		// their flag again -- the cvar meant to disable the feature instead made that player's
		// protection permanent. (2) With no cvar change at all: ClientSpawn() resets
		// chat_protection_timer to 0 but did not touch bit 5, so a protected player killed by a path
		// that bypasses G_Damage (the duel-tournament arena and the melee platform both call
		// player_die() directly) respawned with timer == 0 and the flag still set -- and with the
		// timer at 0 the old clear branch could never fire again, leaving them invulnerable while
		// fully able to fight. The clear now runs whenever the player simply isn't talking, regardless
		// of the timer, and disabling the cvar actively clears any protection still in effect instead
		// of freezing it. ClientSpawn() also clears the flag alongside the timer now, closing (2) at
		// its source as well.
		//
		// GalaxyRP fix: [gameplay] pers.chat_protection_timer now holds the level.time at which the
		// player OPENED chat, and the test below is on the time elapsed since. It used to hold the
		// deadline, "level.time + cvar", baked in at that moment: a lowered cvar did not reach anyone
		// already typing until they closed chat, and an absurdly large value overflowed the sum
		// negative, which made the old "deadline < level.time" true at once -- instant protection from
		// a setting that read as "practically never". With the elapsed form the live value applies on
		// every one-second tick of this loop and a huge value simply never arrives. The cvar is still
		// milliseconds, rounded up to the tick: the tick that sees chat open records the start, and
		// since ticks are at least 1000 ms apart, 1 and 1000 both protect on the next one, 5000 on the
		// fifth after it. 0 stays the "not talking" sentinel: this loop cannot run before level.time
		// has advanced 1000 ms from map start, so level.time is never 0 here, and ClientSpawn() still
		// resets the field to 0 together with the flag, which is what keeps protection from surviving
		// a respawn.
		if (rp_chat_protection_timer.integer > 0)
		{ // zyk: chat protection. If 0, it is off. If greater than 0, set the timer to protect the player
			if (client->ps.eFlags & EF_TALK && client->pers.chat_protection_timer == 0)
			{
				client->pers.chat_protection_timer = level.time;
			}
			else if (ent->client->ps.eFlags & EF_TALK && level.time - client->pers.chat_protection_timer >= rp_chat_protection_timer.integer)
			{
				client->pers.player_statuses |= (1 << PLAYER_STATUS_CHAT_PROTECTION);
			}
			else if (!(client->ps.eFlags & EF_TALK))
			{
				client->pers.player_statuses &= ~(1 << PLAYER_STATUS_CHAT_PROTECTION);
				client->pers.chat_protection_timer = 0;
			}
		}
		else if ((client->pers.player_statuses & (1 << PLAYER_STATUS_CHAT_PROTECTION)) || client->pers.chat_protection_timer != 0)
		{ // zyk: feature disabled -- don't leave anyone holding the protection it granted
			client->pers.player_statuses &= ~(1 << PLAYER_STATUS_CHAT_PROTECTION);
			client->pers.chat_protection_timer = 0;
		}

		RP_AfkTick(ent);	// GLX_U: [AFK]

		// GalaxyRP fix: [Dead Code] the RPG auto-heal block used to be here -- "if (amrpgmode == 2 &&
		// !(quest_power_status & (1 << 2)))" wrapping Magic Regen, Healing Crystal and Energy Crystal.
		// The wrapper's own condition was reachable; all three things inside it were not. Magic Regen
		// needed pers.universe_quest_progress == NUM_OF_UNIVERSE_QUEST_OBJ (22, galaxyrp/game/rp_local.h)
		// and universe_quest_counter bit 0, and neither field was ever written anywhere in the tree, so
		// both sat at 0 and 0 == 22 is false. Both fields have since been removed from
		// clientPersistant_t entirely, along with the last branches that read them.
		// Healing Crystal and Energy Crystal needed PLAYER_STATUS_HEALING_CRYSTAL and
		// PLAYER_STATUS_ENERGY_CRYSTAL, two of the thirteen bits g_local.h marks as never set; each has
		// zero "|=" sites in the tree. Nothing else lived in the block.

		//GalaxyRP (Alex): [Stat Regen] Regen some stats every second. In certain animations the regen will be higher.
		int health_regen_amount = 1 + ent->client->pers.skill_levels[59];
		int shield_regen_amount = 1 + ent->client->pers.skill_levels[58];

		// GalaxyRP fix: [Stat Regen/exploit] gated on being logged in. Everything this block reads is
		// RPG state -- the two regen skills set the rate, pers.max_rpg_health and pers.max_rpg_shield
		// set the ceilings -- but it was the one RPG feature in the tree checking neither
		// sess.amrpgmode nor sess.loggedin, so it ran for logged-out players too. /logout clamps
		// health and shield to 100 and sets STAT_MAX_HEALTH to 100; this block then walked both
		// straight back up to the character's RPG caps at up to 6 a second (12 while meditating,
		// which ignores rp_allow_passive_regen), leaving a logged-out player at up to 300/300 with
		// STAT_MAX_HEALTH still reading 100. Respawning did not help: ClientSpawn refreshes
		// STAT_MAX_HEALTH and health correctly but never touches the two pers caps.
		//
		// Gated the same way as RegenerateAmmo() a few lines above, which is the sibling feature and
		// already used sess.loggedin -- /logout clears it, so the two now agree. The pers fields this
		// reads are reset on logout as well (Cmd_LogoutAccount_f), so this is belt and braces: either
		// change alone closes the hole.
		//GalaxyRP (Alex): [Stat Regen] Never regen while downed or dead.
		if (ent->client->sess.loggedin == qtrue && ent->health > 0 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))) {
			// GalaxyRP fix: [Stat Regen] top up to the maximum, instead of refusing any tick that
			// would not fit a whole step.
			//
			// The guard used to be "value + amount <= max", which stops as soon as you are within
			// amount-1 of full and then leaves you there for good, because the next tick fails the
			// same test. Where you settled depended on how much damage you happened to take, and
			// levelling the skill made it worse rather than better: at amount 1 you always reached
			// full, at amount 6 you could sit five points short of your own maximum forever.
			//
			// Meditating went wrong in the other direction. The guard was written against amount
			// while the branch under it added amount * 2, so it overshot the maximum by up to amount
			// and stayed there -- nothing in the tree decays health or shield back down, so the only
			// way off was to get hit.
			//
			// Testing "below the maximum" and clamping after the add fixes both ends and changes
			// nothing in between: every tick that used to fire still fires, with the same amount, and
			// meditating is still worth exactly double. The two resources are still decided
			// independently, so a full health bar does not hold back shield regen or the reverse.
			//
			// A value already ABOVE the maximum is left alone rather than clamped down. Shield in
			// particular can be raised past max_rpg_shield by other means, and quietly draining it
			// here would be a new behaviour rather than a fix.
			int health_gain = 0;
			int shield_gain = 0;

			if (client->ps.legsAnim == BOTH_MEDITATE) {
				health_gain = health_regen_amount * 2;
				shield_gain = shield_regen_amount * 2;
			}
			else if (rp_allow_passive_regen.integer) {
				health_gain = health_regen_amount;
				shield_gain = shield_regen_amount;
			}

			if (health_gain > 0 && ent->health < client->pers.max_rpg_health) {
				ent->health += health_gain;

				if (ent->health > client->pers.max_rpg_health) {
					ent->health = client->pers.max_rpg_health;
				}
			}

			if (shield_gain > 0 && client->ps.stats[STAT_ARMOR] < client->pers.max_rpg_shield) {
				client->ps.stats[STAT_ARMOR] += shield_gain;

				if (client->ps.stats[STAT_ARMOR] > client->pers.max_rpg_shield) {
					client->ps.stats[STAT_ARMOR] = client->pers.max_rpg_shield;
				}
			}
		}


		if (rp_vote_timer.integer > 0 && client->sess.vote_timer > 0)
		{ // zyk: countdown of the vote timer
			client->sess.vote_timer--;
		}

		// Tr!Force: [Motd] Show server motd
		if (client->motdTime)
		{
			if (client->motdTime <= rp_screen_message_timer.integer)
			{
				RPMod_StringEscape(rp_screen_message.string, serverMotd, MAX_STRING_CHARS);
				trap->SendServerCommand(ent->s.number, va("cp \"%s\nTime: %d\"", serverMotd, client->motdTime));
			}
			
			client->motdTime--;
		}
	}

	// GalaxyRP fix: [Magic] the per-player event cascade used to run here: once a second for a few
	// seconds after spawn, login and logout (send_rpg_events), it pushed the player's RPG state to
	// the client as EV_USE_ITEM13 events -- the magic power bar, the Immunity / Ultra Strength /
	// Ultra Resistance timers and finally the "is an RPG player" flag (parm 104 / 114) for the
	// Force Shield draw. Every one of those readers is gone from cgame: the magic bars with the
	// magic system, the flag with the rpg_class mirror. The cascade, its send_event_timer /
	// send_event_interval fields, PLAYER_STATUS_SENT_FORCE_USER_EVENT and send_rpg_events() went
	// with them.
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;
	if ( client->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) ) {
		// this used to be an ^1 but once a player says ready, it should stick
		client->readyToExit = qtrue;
	}
}

extern void NPC_SetAnim(gentity_t	*ent,int setAnimParts,int anim,int setAnimFlags);
void G_VehicleAttachDroidUnit( gentity_t *vehEnt )
{
	if ( vehEnt && vehEnt->m_pVehicle && vehEnt->m_pVehicle->m_pDroidUnit != NULL )
	{
		gentity_t *droidEnt = (gentity_t *)vehEnt->m_pVehicle->m_pDroidUnit;
		mdxaBone_t boltMatrix;
		vec3_t	fwd;

		trap->G2API_GetBoltMatrix(vehEnt->ghoul2, 0, vehEnt->m_pVehicle->m_iDroidUnitTag, &boltMatrix, vehEnt->r.currentAngles, vehEnt->r.currentOrigin, level.time,
			NULL, vehEnt->modelScale);
		BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, droidEnt->r.currentOrigin);
		BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_Y, fwd);
		vectoangles( fwd, droidEnt->r.currentAngles );

		if ( droidEnt->client )
		{
			VectorCopy( droidEnt->r.currentAngles, droidEnt->client->ps.viewangles );
			VectorCopy( droidEnt->r.currentOrigin, droidEnt->client->ps.origin );
		}

		G_SetOrigin( droidEnt, droidEnt->r.currentOrigin );
		trap->LinkEntity( (sharedEntity_t *)droidEnt );

		if ( droidEnt->NPC )
		{
			NPC_SetAnim( droidEnt, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		}
	}
}

//called gameside only from pmove code (convenience)
extern qboolean BG_SabersOff( playerState_t *ps );
void G_CheapWeaponFire(int entNum, int ev)
{
	gentity_t *ent = &g_entities[entNum];

	if (!ent->inuse || !ent->client)
	{
		return;
	}

	switch (ev)
	{
		case EV_FIRE_WEAPON:
			if (ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER &&
				ent->client && ent->client->ps.m_iVehicleNum)
			{ //a speeder with a pilot
				gentity_t *rider = &g_entities[ent->client->ps.m_iVehicleNum-1];
				if (rider->inuse && rider->client)
				{ //pilot is valid...
                    if (rider->client->ps.weapon != WP_MELEE &&
						(rider->client->ps.weapon != WP_SABER || !BG_SabersOff(&rider->client->ps)))
					{ //can only attack on speeder when using melee or when saber is holstered
						break;
					}
				}
			}

			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
		case EV_ALT_FIRE:
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
	}
}

/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
qboolean BG_InKnockDownOnly( int anim );

void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int		i;//, j;
	int		event;
	gclient_t *client;
	int		damage;
	vec3_t	dir;
//	vec3_t	origin, angles;
//	qboolean	fired;
//	gitem_t *item;
//	gentity_t *drop;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_PS_EVENTS-1) ];

		switch ( event ) {
		case EV_FALL:
		case EV_ROLL:
			{
				int delta = client->ps.eventParms[ i & (MAX_PS_EVENTS-1) ];
				qboolean knockDownage = qfalse;

				if (ent->client && ent->client->ps.fallingToDeath)
				{
					break;
				}

				if ( ent->s.eType != ET_PLAYER )
				{
					break;		// not in the player model
				}

				if ( dmflags.integer & DF_NO_FALLING )
				{
					break;
				}

				if ( ent->flags & FL_NO_IMPACT_DMG )
				{ // GalaxyRP: [Scripts] SET_NO_IMPACT_DAMAGE
					break;
				}

				if (BG_InKnockDownOnly(ent->client->ps.legsAnim))
				{
					if (delta <= 14)
					{
						break;
					}
					knockDownage = qtrue;
				}
				else
				{
					if (delta <= 44)
					{
						break;
					}
				}

				if (knockDownage)
				{
					damage = delta*1; //you suffer for falling unprepared. A lot. Makes throws and things useful, and more realistic I suppose.
				}
				else
				{
					if (level.gametype == GT_SIEGE &&
						delta > 60)
					{ //longer falls hurt more
						damage = delta*1; //good enough for now, I guess
					}
					else
					{
						damage = delta*0.16; //good enough for now, I guess
					}
				}

				VectorSet (dir, 0, 0, 1);
				ent->pain_debounce_time = level.time + 200;	// no normal pain sound
				G_Damage (ent, NULL, NULL, NULL, NULL, damage, DAMAGE_NO_ARMOR, MOD_FALLING);

				if (ent->health < 1)
				{
					G_Sound(ent, CHAN_AUTO, G_SoundIndex( "sound/player/fallsplat.wav" ));
				}
			}
			break;
		case EV_FIRE_WEAPON:
			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_ALT_FIRE:
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_SABER_ATTACK:
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			// GalaxyRP fix: [Cloak Item] a saber swing has to break cloak like any other attack, and it
			// was the one weapon that never did. The decloak hook lives in FireWeapon() (g_weapon.c),
			// which is driven by EV_FIRE_WEAPON / EV_ALT_FIRE -- but PM_Weapon (bg_pmove.c) hands the
			// saber to PM_WeaponLightsaber() and then returns via killAfterItem BEFORE the block that
			// emits either of those, and PM_WeaponLightsaber only ever emits EV_SABER_ATTACK. So a
			// cloaked player could swing, damage and kill while staying fully invisible. Decloaking on
			// this event closes it at the one place the saber does announce itself, pair-aware so a
			// mounted attacker takes their vehicle down with them.
			// GalaxyRP fix: [Cloak Item] pair-gated, same as FireWeapon and G_Damage -- a rider swinging
			// from the saddle must take a cloaked vehicle down with them even when the rider is not the
			// cloaked half.
			if ( Jedi_PairIsCloaked( ent ) )
			{
				Jedi_DecloakPair( ent );
			}
			break;

		//rww - Note that these must be in the same order (ITEM#-wise) as they are in holdable_t
		case EV_USE_ITEM1: //seeker droid
			ItemUse_Seeker(ent);
			break;
		case EV_USE_ITEM2: //shield
			ItemUse_Shield(ent);
			break;
		case EV_USE_ITEM3: //medpack
			ItemUse_MedPack(ent);
			break;
		case EV_USE_ITEM4: //big medpack
			ItemUse_MedPack_Big(ent);
			break;
		case EV_USE_ITEM5: //binoculars
			ItemUse_Binoculars(ent);
			break;
		case EV_USE_ITEM6: //sentry gun
			ItemUse_Sentry(ent);
			break;
		case EV_USE_ITEM7: //jetpack
			ItemUse_Jetpack(ent);
			break;
		case EV_USE_ITEM8: //health disp
			//ItemUse_UseDisp(ent, HI_HEALTHDISP);
			break;
		case EV_USE_ITEM9: //ammo disp
			//ItemUse_UseDisp(ent, HI_AMMODISP);
			break;
		case EV_USE_ITEM10: //eweb
			ItemUse_UseEWeb(ent);
			break;
		case EV_USE_ITEM11: //cloak
			ItemUse_UseCloak(ent);
			break;
		default:
			break;
		}
	}

}

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client who generated the event
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
}

static const float maxJediMasterDistance = 2500.0f * 2500.0f; // x^2, optimisation
static const float maxJediMasterFOV = 100.0f;
static const float maxForceSightDistance = Square( 1500.0f ) * 1500.0f; // x^2, optimisation
static const float maxForceSightFOV = 100.0f;

void G_UpdateClientBroadcasts( gentity_t *self ) {
	int i;
	gentity_t *other;

	// we are always sent to ourselves
	// we are always sent to other clients if we are in their PVS
	// if we are not in their PVS, we must set the broadcastClients bit field
	// if we do not wish to be sent to any particular entity, we must set the broadcastClients bit field and the
	//	SVF_BROADCASTCLIENTS bit flag
	self->r.broadcastClients[0] = 0u;
	self->r.broadcastClients[1] = 0u;

	for ( i = 0, other = g_entities; i < MAX_CLIENTS; i++, other++ ) {
		qboolean send = qfalse;
		float dist;
		vec3_t angles;

		if ( !other->inuse || other->client->pers.connected != CON_CONNECTED ) {
			// no need to compute visibility for non-connected clients
			continue;
		}

		if ( other == self ) {
			// we are always sent to ourselves anyway, this is purely an optimisation
			continue;
		}

		VectorSubtract( self->client->ps.origin, other->client->ps.origin, angles );
		dist = VectorLengthSquared( angles );
		vectoangles( angles, angles );

		// broadcast jedi master to everyone if we are in distance/field of view
		if ( level.gametype == GT_JEDIMASTER && self->client->ps.isJediMaster ) {
			if ( dist < maxJediMasterDistance
				&& InFieldOfVision( other->client->ps.viewangles, maxJediMasterFOV, angles ) )
			{
				send = qtrue;
			}
		}

		// broadcast this client to everyone using force sight if we are in distance/field of view
		if ( (other->client->ps.fd.forcePowersActive & (1 << FP_SEE)) ) {
			if ( dist < maxForceSightDistance
				&& InFieldOfVision( other->client->ps.viewangles, maxForceSightFOV, angles ) )
			{
				send = qtrue;
			}
		}

		if ( send ) {
			Q_AddToBitflags( self->r.broadcastClients, i, 32 );
		}
	}

	trap->LinkEntity( (sharedEntity_t *)self );
}

void G_AddPushVecToUcmd( gentity_t *self, usercmd_t *ucmd )
{
	vec3_t	forward, right, moveDir;
	float	pushSpeed, fMove, rMove;

	if ( !self->client )
	{
		return;
	}
	pushSpeed = VectorLengthSquared(self->client->pushVec);
	if(!pushSpeed)
	{//not being pushed
		return;
	}

	AngleVectors(self->client->ps.viewangles, forward, right, NULL);
	VectorScale(forward, ucmd->forwardmove/127.0f * self->client->ps.speed, moveDir);
	VectorMA(moveDir, ucmd->rightmove/127.0f * self->client->ps.speed, right, moveDir);
	//moveDir is now our intended move velocity

	VectorAdd(moveDir, self->client->pushVec, moveDir);
	self->client->ps.speed = VectorNormalize(moveDir);
	//moveDir is now our intended move velocity plus our push Vector

	fMove = 127.0 * DotProduct(forward, moveDir);
	rMove = 127.0 * DotProduct(right, moveDir);
	ucmd->forwardmove = floor(fMove);//If in the same dir , will be positive
	ucmd->rightmove = floor(rMove);//If in the same dir , will be positive

	if ( self->client->pushVecTime < level.time )
	{
		VectorClear( self->client->pushVec );
	}
}

qboolean G_StandingAnim( int anim )
{//NOTE: does not check idles or special (cinematic) stands
	switch ( anim )
	{
	case BOTH_STAND1:
	case BOTH_STAND2:
	case BOTH_STAND3:
	case BOTH_STAND4:
		return qtrue;
		break;
	}
	return qfalse;
}

qboolean G_ActionButtonPressed(int buttons)
{
	if (buttons & BUTTON_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE_HOLDABLE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_GESTURE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEGRIP)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_ALT_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEPOWER)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_LIGHTNING)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_DRAIN)
	{
		return qtrue;
	}

	return qfalse;
}

int return_idle_animation(int animation) {
	if (animation > 0) {
		return animation;
	}
	return BOTH_STAND1IDLE1;
}

// DAJ_RP: [Saber Keys] one saber's idle for the current style: its own idleAnim* key for the style, else its plain
// idleAnim (every style's fallback, as readyAnim and kataMove are for theirs); <= 0 when it has neither.
static int G_RpSaberIdleAnim( const saberInfo_t *saber, int style )
{
	int anim;

	switch (style)
	{
	case SS_FAST:
		anim = saber->idleAnimBlue;
		break;
	case SS_MEDIUM:
		anim = saber->idleAnimYellow;
		break;
	case SS_STRONG:
		anim = saber->idleAnimRed;
		break;
	case SS_DESANN:
		anim = saber->idleAnimPurple;
		break;
	case SS_TAVION:
		anim = saber->idleAnimGreen;
		break;
	case SS_DUAL:
		anim = saber->idleAnimDual;
		break;
	case SS_STAFF:
		anim = saber->idleAnimStaff;
		break;
	default:
		anim = -1;
		break;
	}

	return (anim > 0) ? anim : saber->idleAnim;
}

// DAJ_RP: [Saber Keys] the first saber's idle (G_RpSaberIdleAnim), else the second saber's when there is one -- the
// same first-then-second order the stance and kata keys use -- else the stock BOTH_STAND1IDLE1. A style outside the
// list gets a plain key or the stock idle too (it used to get no idle at all).
int get_idle_animation_for_style(gentity_t *ent) {
	const int style = ent->client->ps.fd.saberAnimLevel;
	int anim = G_RpSaberIdleAnim( &ent->client->saber[0], style );

	if (anim <= 0 && ent->client->saber[1].model[0])
	{
		anim = G_RpSaberIdleAnim( &ent->client->saber[1], style );
	}

	return return_idle_animation(anim);
}

// DAJ_RP: [Saber Keys] an idle the player breaks out of when he moves, acts or is hurt: the stock ones, and whichever
// G_CheckClientIdle itself last started (a .sab's idleAnim key can name any anim). That one was held to its end before,
// the player gliding around in it, because only the stock ones were listed here. "timer" is that half's (legsTimer or
// torsoTimer): the started one counts only while that half still holds it -- the same anim met any other way (a saber
// stance such as Desann's BOTH_STAND9, an emote) is not an idle, and breaking out of it cut a saber's recovery short.
static qboolean G_RpBreakableIdle( const gclient_t *client, int anim, int timer )
{
	switch ( anim )
	{
	case BOTH_STAND1IDLE1:
	case BOTH_STAND2IDLE1:
	case BOTH_STAND2IDLE2:
	case BOTH_STAND3IDLE1:
	case BOTH_STAND5IDLE1:
		return qtrue;
	}
	return ( client->rpIdleAnim > 0 && anim == client->rpIdleAnim && timer > 0 ) ? qtrue : qfalse;
}

void G_CheckClientIdle( gentity_t *ent, usercmd_t *ucmd )
{
	vec3_t viewChange;
	qboolean actionPressed;
	int buttons;

	if ( !ent || !ent->client || ent->health <= 0 || (ent->client->ps.stats[STAT_HEALTH] <= 0 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))) ||
		ent->client->sess.sessionTeam == TEAM_SPECTATOR || (ent->client->ps.pm_flags & PMF_FOLLOW))
	{
		return;
	}

	// DAJ_RP: [Saber Keys] forget the idle we started as soon as neither half still holds it (G_RpBreakableIdle): every
	// frame, so one that ended while the player stood still is gone before he can light his saber into a stance using
	// the same anim
	if ( ent->client->rpIdleAnim
		&& !( ent->client->ps.legsAnim == ent->client->rpIdleAnim && ent->client->ps.legsTimer > 0 )
		&& !( ent->client->ps.torsoAnim == ent->client->rpIdleAnim && ent->client->ps.torsoTimer > 0 ) )
	{
		ent->client->rpIdleAnim = 0;
	}

	buttons = ucmd->buttons;

	if (ent->r.svFlags & SVF_BOT)
	{ //they press use all the time..
		buttons &= ~BUTTON_USE;
	}
	actionPressed = G_ActionButtonPressed(buttons);

	// zyk: if the player is using an emote, keeps the emote
	if (!ent->NPC && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_EMOTE))
	{
		ent->client->ps.forceHandExtendTime = level.time + 1000;
	}

	VectorSubtract(ent->client->ps.viewangles, ent->client->idleViewAngles, viewChange);
	if ( !VectorCompare( vec3_origin, ent->client->ps.velocity )
		|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove
		|| !G_StandingAnim( ent->client->ps.legsAnim )
		|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
		|| VectorLength(viewChange) > 10
		|| ent->client->ps.legsTimer > 0
		|| ent->client->ps.torsoTimer > 0
		|| ent->client->ps.weaponTime > 0
		|| ent->client->ps.weaponstate == WEAPON_CHARGING
		|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
		|| ent->client->ps.zoomMode
		|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
		|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
		|| ent->client->ps.saberBlocked != BLOCKED_NONE
		|| ent->client->ps.saberBlocking >= level.time
		|| ent->client->ps.weapon == WP_MELEE
		|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
	{//FIXME: also check for turning?
		qboolean brokeOut = qfalse;

		// zyk: if the player is using an emote, removes it
		if (!ent->NPC && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_EMOTE) && actionPressed == qtrue)
		{
			ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_EMOTE);
			ent->client->ps.forceHandExtendTime = level.time;
		}

		if ( !VectorCompare( vec3_origin, ent->client->ps.velocity )
			|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove
			|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
			|| ent->client->ps.zoomMode
			|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
			|| (ent->client->ps.weaponTime > 0 && ent->client->ps.weapon == WP_SABER)
			|| ent->client->ps.weaponstate == WEAPON_CHARGING
			|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
			|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
			|| ent->client->ps.saberBlocked != BLOCKED_NONE
			|| ent->client->ps.saberBlocking >= level.time
			|| ent->client->ps.weapon == WP_MELEE
			|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
		{
			//if in an idle, break out
			// DAJ_RP: [Saber Keys] the stock idles and the one we started (G_RpBreakableIdle)
			if ( G_RpBreakableIdle( ent->client, ent->client->ps.legsAnim, ent->client->ps.legsTimer ) )
			{
				ent->client->ps.legsTimer = 0;
				brokeOut = qtrue;
			}
			if ( G_RpBreakableIdle( ent->client, ent->client->ps.torsoAnim, ent->client->ps.torsoTimer ) )
			{
				ent->client->ps.torsoTimer = 0;
				ent->client->ps.weaponTime = 0;
				ent->client->ps.saberMove = LS_READY;
				brokeOut = qtrue;
			}
			if ( brokeOut )
			{
				ent->client->rpIdleAnim = 0;
			}
		}
		//
		ent->client->idleHealth = (ent->health+ent->client->ps.stats[STAT_ARMOR]);
		VectorCopy(ent->client->ps.viewangles, ent->client->idleViewAngles);
		if ( ent->client->idleTime < level.time )
		{
			ent->client->idleTime = level.time;
		}

		// GalaxyRP fix: [Cvars] this used to refresh pers.quest_afk_timer (via zyk_quest_afk_timer,
		// now removed) whenever a player in the general quest system was active. That system is
		// entirely disabled now (quest_get_new_player, its sole entry point, was deleted as
		// unreachable dead code -- see the GalaxyRP fix comment on its old location in g_cmds.c) and
		// can_play_quest can no longer become 1 anywhere in the codebase, so this block was dead in
		// practice and has been removed.

		if (brokeOut &&
			(ent->client->ps.weaponstate == WEAPON_CHARGING || ent->client->ps.weaponstate == WEAPON_CHARGING_ALT))
		{
			ent->client->ps.torsoAnim = TORSO_RAISEWEAP1;
		}
	}
	else if ( level.time - ent->client->idleTime > 5000 )
	{//been idle for 5 seconds
		int	idleAnim = -1;
		switch ( ent->client->ps.legsAnim )
		{
		case BOTH_STAND1:
			idleAnim = BOTH_STAND1IDLE1;
			break;
		case BOTH_STAND2:
			idleAnim = BOTH_STAND2IDLE1;//Q_irand(BOTH_STAND2IDLE1,BOTH_STAND2IDLE2);
			break;
		case BOTH_STAND3:
			idleAnim = BOTH_STAND3IDLE1;
			break;
		case BOTH_STAND5:
			idleAnim = BOTH_STAND5IDLE1;
			break;
		}

		if (idleAnim == BOTH_STAND2IDLE1 && Q_irand(1, 10) <= 5)
		{
			idleAnim = BOTH_STAND2IDLE2;
		}

		if (ent->s.weapon == WP_SABER && ent->client->ps.saberHolstered == 2) {
			idleAnim = get_idle_animation_for_style(ent);
		}

		if ( /*PM_HasAnimation( ent, idleAnim )*/idleAnim > 0 && idleAnim < MAX_ANIMATIONS )
		{
			G_SetAnim(ent, ucmd, SETANIM_BOTH, idleAnim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
			ent->client->rpIdleAnim = idleAnim;	// DAJ_RP: [Saber Keys] so we can break out of it (G_RpBreakableIdle)

			//don't idle again after this anim for a while
			//ent->client->idleTime = level.time + PM_AnimLength( ent->client->clientInfo.animFileIndex, (animNumber_t)idleAnim ) + Q_irand( 0, 2000 );
			ent->client->idleTime = level.time + ent->client->ps.legsTimer + Q_irand( 0, 2000 );
		}
	}
}

void NPC_Accelerate( gentity_t *ent, qboolean fullWalkAcc, qboolean fullRunAcc )
{
	if ( !ent->client || !ent->NPC )
	{
		return;
	}

	if ( !ent->NPC->stats.acceleration )
	{//No acceleration means just start and stop
		ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
	}
	//FIXME:  in cinematics always accel/decel?
	else if ( ent->NPC->desiredSpeed <= ent->NPC->stats.walkSpeed )
	{//Only accelerate if at walkSpeeds
		if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{
			//ent->client->ps.friction = 0;
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{
			//ent->client->ps.friction = 0;
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullWalkAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{//decelerate even when walking
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{//stop on a dime
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
	else//  if ( ent->NPC->desiredSpeed > ent->NPC->stats.walkSpeed )
	{//Only decelerate if at runSpeeds
		if ( fullRunAcc && ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{//Accelerate to runspeed
			//ent->client->ps.friction = 0;
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{//accelerate instantly
			//ent->client->ps.friction = 0;
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullRunAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
}

/*
-------------------------
NPC_GetWalkSpeed
-------------------------
*/

static int NPC_GetWalkSpeed( gentity_t *ent )
{
	int	walkSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	switch ( ent->client->playerTeam )
	{
	case NPCTEAM_PLAYER:	//To shutup compiler, will add entries later (this is stub code)
	default:
		walkSpeed = ent->NPC->stats.walkSpeed;
		break;
	}

	return walkSpeed;
}

/*
-------------------------
NPC_GetRunSpeed
-------------------------
*/
static int NPC_GetRunSpeed( gentity_t *ent )
{
	int	runSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	// team no longer indicates species/race.  Use NPC_class to adjust speed for specific npc types
	switch( ent->client->NPC_class)
	{
	case CLASS_PROBE:	// droid cases here to shut-up compiler
	case CLASS_GONK:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_PROTOCOL:
	case CLASS_ATST: // hmm, not really your average droid
	case CLASS_MOUSE:
	case CLASS_SEEKER:
	case CLASS_REMOTE:
		runSpeed = ent->NPC->stats.runSpeed;
		break;

	default:
		runSpeed = ent->NPC->stats.runSpeed*1.3f; //rww - seems to slow in MP for some reason.
		break;
	}

	return runSpeed;
}

//Seems like a slightly less than ideal method for this, could it be done on the client?
extern qboolean FlyingCreature( gentity_t *ent );
void G_CheckMovingLoopingSounds( gentity_t *ent, usercmd_t *ucmd )
{
	if ( ent->client )
	{
		if ( (ent->NPC&&!VectorCompare( vec3_origin, ent->client->ps.moveDir ))//moving using moveDir
			|| ucmd->forwardmove || ucmd->rightmove//moving using ucmds
			|| (ucmd->upmove&&FlyingCreature( ent ))//flier using ucmds to move
			|| (FlyingCreature( ent )&&!VectorCompare( vec3_origin, ent->client->ps.velocity )&&ent->health>0))//flier using velocity to move
		{
			switch( ent->client->NPC_class )
			{
			case CLASS_R2D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp.wav" );
				break;
			case CLASS_R5D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp2.wav" );
				break;
			case CLASS_MARK2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mark2/misc/mark2_move_lp" );
				break;
			case CLASS_MOUSE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mouse/misc/mouse_lp" );
				break;
			case CLASS_PROBE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/probe/misc/probedroidloop" );
			default:
				break;
			}
		}
		else
		{//not moving under your own control, stop loopSound
			if ( ent->client->NPC_class == CLASS_R2D2 || ent->client->NPC_class == CLASS_R5D2
					|| ent->client->NPC_class == CLASS_MARK2 || ent->client->NPC_class == CLASS_MOUSE
					|| ent->client->NPC_class == CLASS_PROBE )
			{
				ent->s.loopSound = 0;
			}
		}
	}
}

void G_HeldByMonster( gentity_t *ent, usercmd_t *ucmd )
{
	if ( ent
		&& ent->client
		&& ent->client->ps.hasLookTarget )//NOTE: lookTarget is an entity number, so this presumes that client 0 is NOT a Rancor...
	{
		gentity_t *monster = &g_entities[ent->client->ps.lookTarget];
		if ( monster && monster->client )
		{
			//take the monster's waypoint as your own
			ent->waypoint = monster->waypoint;
			if ( monster->s.NPC_class == CLASS_RANCOR )
			{//only possibility right now, may add Wampa and Sand Creature later
				BG_AttachToRancor( monster->ghoul2, //ghoul2 info
					monster->r.currentAngles[YAW],
					monster->r.currentOrigin,
					level.time,
					NULL,
					monster->modelScale,
					(monster->client->ps.eFlags2&EF2_GENERIC_NPC_FLAG),
					ent->client->ps.origin,
					ent->client->ps.viewangles,
					NULL );
			}
			VectorClear( ent->client->ps.velocity );
			G_SetOrigin( ent, ent->client->ps.origin );
			SetClientViewAngle( ent, ent->client->ps.viewangles );
			G_SetAngles( ent, ent->client->ps.viewangles );
			trap->LinkEntity( (sharedEntity_t *)ent );//redundant?
		}
	}
	// don't allow movement, weapon switching, and most kinds of button presses
	ucmd->forwardmove = 0;
	ucmd->rightmove = 0;
	ucmd->upmove = 0;
}

typedef enum tauntTypes_e
{
	TAUNT_TAUNT = 0,
	TAUNT_BOW,
	TAUNT_MEDITATE,
	TAUNT_FLOURISH,
	TAUNT_GLOAT
} tauntTypes_t;

void G_SetTauntAnim( gentity_t *ent, int taunt )
{
	/* zyk: now taunts are allowed while moving and in all gametypes
	if (ent->client->pers.cmd.upmove ||
		ent->client->pers.cmd.forwardmove ||
		ent->client->pers.cmd.rightmove)
	{ //hack, don't do while moving
		return;
	}
	if ( taunt != TAUNT_TAUNT )
	{//normal taunt always allowed
		if ( level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL )
		{//no taunts unless in Duel
			return;
		}
	}
	*/

	// fix: rocket lock bug
	BG_ClearRocketLock(&ent->client->ps);

	if ( ent->client->ps.torsoTimer < 1
		&& ent->client->ps.forceHandExtend == HANDEXTEND_NONE
		&& ent->client->ps.legsTimer < 1
		&& ent->client->ps.weaponTime < 1
		&& ent->client->ps.saberLockTime < level.time )
	{
		int anim = -1;
		switch ( taunt )
		{
		case TAUNT_TAUNT:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_ENGAGETAUNT;
			}
			else if ( ent->client->saber[0].tauntAnim != -1 )
			{
				anim = ent->client->saber[0].tauntAnim;
			}
			else if ( ent->client->saber[1].model[0]
					&& ent->client->saber[1].tauntAnim != -1 )
			{
				anim = ent->client->saber[1].tauntAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					if ( ent->client->ps.saberHolstered == 1
						&& ent->client->saber[1].model[0] )
					{//turn off second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
					}
					else if ( ent->client->ps.saberHolstered == 0 )
					{//turn off first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
					}
					ent->client->ps.saberHolstered = 2;
					anim = BOTH_GESTURE1;
					break;
				case SS_MEDIUM:
				case SS_STRONG:
				case SS_DESANN:
					anim = BOTH_ENGAGETAUNT;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1
						&& ent->client->saber[1].model[0] )
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 )
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_DUAL_TAUNT;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered > 0 )
					{//turn on all blades
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_STAFF_TAUNT;
					break;
				}
			}
			break;
		case TAUNT_BOW:
			if ( ent->client->saber[0].bowAnim != -1 )
			{
				anim = ent->client->saber[0].bowAnim;
			}
			else if ( ent->client->saber[1].model[0]
					&& ent->client->saber[1].bowAnim != -1 )
			{
				anim = ent->client->saber[1].bowAnim;
			}
			else
			{
				anim = BOTH_BOW;
			}

			if (ent->client->ps.weapon == WP_SABER)
			{ // zyk: added this condition
				if (ent->client->ps.saberHolstered == 1
					&& ent->client->saber[1].model[0])
				{//turn off second saber
					G_Sound(ent, CHAN_WEAPON, ent->client->saber[1].soundOff);
				}
				else if (ent->client->ps.saberHolstered == 0)
				{//turn off first
					G_Sound(ent, CHAN_WEAPON, ent->client->saber[0].soundOff);
				}
				ent->client->ps.saberHolstered = 2;
			}

			break;
		case TAUNT_MEDITATE:
			if ( ent->client->saber[0].meditateAnim != -1 )
			{
				anim = ent->client->saber[0].meditateAnim;
			}
			else if ( ent->client->saber[1].model[0]
					&& ent->client->saber[1].meditateAnim != -1 )
			{
				anim = ent->client->saber[1].meditateAnim;
			}
			else
			{
				anim = BOTH_MEDITATE;
			}

			if (ent->client->ps.weapon == WP_SABER)
			{ // zyk: added this condition
				if (ent->client->ps.saberHolstered == 1
					&& ent->client->saber[1].model[0])
				{//turn off second saber
					G_Sound(ent, CHAN_WEAPON, ent->client->saber[1].soundOff);
				}
				else if (ent->client->ps.saberHolstered == 0)
				{//turn off first
					G_Sound(ent, CHAN_WEAPON, ent->client->saber[0].soundOff);
				}
				ent->client->ps.saberHolstered = 2;
			}

			break;
		case TAUNT_FLOURISH:
			if ( ent->client->ps.weapon == WP_SABER )
			{
				if ( ent->client->ps.saberHolstered == 1
					&& ent->client->saber[1].model[0] )
				{//turn on second saber
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
				}
				else if ( ent->client->ps.saberHolstered == 2 )
				{//turn on first
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
				}
				ent->client->ps.saberHolstered = 0;
				if ( ent->client->saber[0].flourishAnim != -1 )
				{
					anim = ent->client->saber[0].flourishAnim;
				}
				else if ( ent->client->saber[1].model[0]
					&& ent->client->saber[1].flourishAnim != -1 )
				{
					anim = ent->client->saber[1].flourishAnim;
				}
				else
				{
					switch ( ent->client->ps.fd.saberAnimLevel )
					{
					case SS_FAST:
					case SS_TAVION:
						anim = BOTH_SHOWOFF_FAST;
						break;
					case SS_MEDIUM:
						anim = BOTH_SHOWOFF_MEDIUM;
						break;
					case SS_STRONG:
					case SS_DESANN:
						anim = BOTH_SHOWOFF_STRONG;
						break;
					case SS_DUAL:
						anim = BOTH_SHOWOFF_DUAL;
						break;
					case SS_STAFF:
						anim = BOTH_SHOWOFF_STAFF;
						break;
					}
				}
			}
			break;
		case TAUNT_GLOAT:
			if ( ent->client->saber[0].gloatAnim != -1 )
			{
				anim = ent->client->saber[0].gloatAnim;
			}
			else if ( ent->client->saber[1].model[0]
					&& ent->client->saber[1].gloatAnim != -1 )
			{
				anim = ent->client->saber[1].gloatAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					anim = BOTH_VICTORY_FAST;
					break;
				case SS_MEDIUM:
					anim = BOTH_VICTORY_MEDIUM;
					break;
				case SS_STRONG:
				case SS_DESANN:
					if ( ent->client->ps.saberHolstered )
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STRONG;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1
						&& ent->client->saber[1].model[0] )
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 )
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_DUAL;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered )
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STAFF;
					break;
				}
			}
			break;
		}
		if ( anim != -1 )
		{
			if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE )
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
				ent->client->ps.forceDodgeAnim = anim;
				ent->client->ps.forceHandExtendTime = level.time + BG_AnimLength(ent->localAnimIndex, (animNumber_t)anim);
			}
			if ( taunt != TAUNT_MEDITATE
				&& taunt != TAUNT_BOW )
			{//no sound for meditate or bow
				G_AddEvent( ent, EV_TAUNT, taunt );
			}
		}
	}
}

// GalaxyRP fix: [Magic] zyk_do_force_dash() used to be here -- the per-frame half of the Fast Dash
// ability, which accelerated the user and knocked down anyone it passed through. It had no callers
// anywhere in the tree, and it was the only caller of zyk_force_dash_effect() (g_main.c) and the
// only reader of pers.fast_dash_timer; all three have gone together. The two externs above it went
// with it; zyk_unique_ability_can_hit_target() itself outlived them for a while and has since been
// deleted with the magic effect functions.

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
extern void Boba_FlyStop( gentity_t *self );
// GalaxyRP fix: [Magic] removed zyk_show_magic_master_powers()/zyk_show_left_magic_master_powers()/
// zyk_show_right_magic_master_powers() externs here — all three removed as dead, along with
// magic_master_has_this_power() and their other helpers (see g_main.c)
// GalaxyRP fix: [Quests] removed zyk_unique_boost() extern here — function removed as dead (see g_cmds.c)
extern void TossClientWeapon(gentity_t *self, vec3_t direction, float speed);
extern qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
// GalaxyRP fix: [Skills] removed zyk_can_use_unique() extern here — function removed as dead along
// with Cmd_Unique_f and the GENCMD_ENGAGE_DUEL Unique Skill branch above (see g_cmds.c)
extern qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target);
// GalaxyRP fix: [Guardian] removed zyk_can_hit_boss_battle_target() extern here — function removed
// as dead (see g_main.c); it was a stub always returning qtrue.
/*
========================================================================================================
GalaxyRP: [Grapple Hook] -- the gates and the per-frame bookkeeping.

Ported from TaystJK (g_active.c: CanGrapple, CanFireGrapple, and the two "CHUNK" blocks in
ClientThink_real). Their racemode/movement-style/Tribes tests are gone; ours are added. Three
questions, asked in this order every frame:

  RP_HookMayStayOut  -- may this player have a hook out AT ALL right now? No: it is freed. This is
                        where the account (RP_GrappleAllowed), death, spectating, downed, private
                        duels, mini-games, vehicles and emplaced guns are. Each of those also has
                        its own call to Weapon_HookFree() where the state is entered, so a hook
                        never survives even one frame into them; this is the net under those.
  RP_CanKeepHook     -- given it may stay out, may it PULL this frame? No: PMF_GRAPPLE is lowered
                        but the hook stays parked. Rolls and special jumps, as in TaystJK: the pull
                        pauses through them and resumes.
  RP_CanFireHook     -- may a new hook be fired? RP_CanKeepHook plus the jetpack being off and the
                        saber idle, as in TaystJK (a hook fired mid-swing is a hook fired into your
                        own blade).
========================================================================================================
*/
extern qboolean duel_tournament_is_duelist( gentity_t *ent );	// g_main.c

qboolean RP_HookMayStayOut( gentity_t *ent )
{
	if ( !ent || !ent->client )
		return qfalse;
	if ( !RP_GrappleAllowed( ent ) )
		return qfalse;
	if ( ent->client->ps.pm_type == PM_DEAD || ent->health <= 0 )
		return qfalse;
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time )
		return qfalse;
	if ( ent->client->ps.duelInProgress )
		return qfalse;
	if ( G_PlayerIsDowned( ent ) )
		return qfalse;
	if ( ent->client->ps.m_iVehicleNum || ent->client->ps.emplacedIndex )
		return qfalse;
	if ( level.duel_tournament_mode == 4 && duel_tournament_is_duelist( ent ) )
		return qfalse;
	if ( level.melee_mode > 0 && level.melee_players[ent->s.number] != -1 )
		return qfalse;
	return qtrue;
}

qboolean RP_CanKeepHook( gentity_t *ent )
{
	if ( !RP_HookMayStayOut( ent ) )
		return qfalse;
	if ( BG_InRoll( &ent->client->ps, ent->client->ps.legsAnim ) )
		return qfalse;
	if ( BG_InSpecialJump( ent->client->ps.legsAnim ) )
		return qfalse;
	return qtrue;
}

qboolean RP_CanFireHook( gentity_t *ent )
{
	if ( !RP_CanKeepHook( ent ) )
		return qfalse;
	if ( ent->client->jetPackOn )
		return qfalse;
	if ( !BG_SaberInIdle( ent->client->ps.saberMove ) )
		return qfalse;
	return qtrue;
}

/*
==================
RP_JumpKickLand

GalaxyRP: [Jump Kick] the jump kick -- jump, then jump again with forward held next to a player or NPC
to run up them and flip off. PM_CheckJump() (bg_pmove.c) finds the target and leaves its number in
ps.forceKickFlip; this does the rest, once, in ClientThink_real(). It was written out inline there and
is unchanged in who may kick whom: the team, private duel, duel tournament, melee arena and noclip
rules (OnSameTeam, zyk_can_hit_target) all still apply, and an ally NPC counts as a teammate in FFA.

What changed, and why:

- Damage is rp_melee_jump_kick_damage (20 shipped), not 5% of the kicker's speed. That made a kick
  anything from about 10 to 28 depending on how the jump happened to be going -- 20 is the middle of
  it. G_Damage() goes on to scale it exactly as it did before, as for any melee hit: the Melee skill,
  Force Rage, the kicker's maximum health and where it lands (g_locationBasedDamage). It still ignores
  shields (DAMAGE_NO_ARMOR), as it always has.
- The push follows the way the kicker faces: RP_JUMP_KICK_PUSH forward and RP_JUMP_KICK_LIFT up. It
  used to be the reverse of the kicker's own velocity. At Jump 2 that is the same direction and, the
  arithmetic cancelling, the same 300 units a second give or take rounding -- but at Jump 3 and above
  the flip off the target leaves the kicker going straight up, so the "push" was straight down and
  the target was only lifted. (Stock JKA never kicked at Jump 3; the Zyk mod turned it on.)
- Kill credit is given whenever the kick lands. It used to be set only in the branch that pushed;
  G_Damage() covered most of the rest, but not a hit without knockback, so a target attacking in red
  stance that takes no knockback (FL_NO_KNOCKBACK) could be kicked off a ledge with nobody credited --
  and with the change below, protected targets and large NPCs take no knockback from a kick at all.
  The window is unchanged at five seconds.
- No push or knockdown for a target the damage cannot touch either: spawn protection still running,
  or /god (see RP_JumpKickProtected), as JA++ does. Such a target used to take no damage and still be
  thrown and floored.
- No push or knockdown for vehicles and large NPCs (RP_JumpKickImmovable). They still take the damage.
  For both of these the damage's own knockback is off as well (DAMAGE_NO_KNOCKBACK). Every other
  target still gets that small shove from the hit, as before -- it is all a target already on the
  ground, or attacking in red stance, receives.
- The target must have health above 0, not merely other than 0.

Unchanged: the 30% knockdown chance; no push or knockdown on a target attacking in red stance, already
knocked down -- which a downed or admin-paralysed player always is -- riding, or on an emplaced gun.
==================
*/
#define RP_JUMP_KICK_PUSH	300.0f
#define RP_JUMP_KICK_LIFT	200.0f

// spawn protection still running, or /god -- the two G_Damage() checks that stop a kick's damage. Read
// the same way here: godmode does not protect an NPC there, and protection whose time is up does not
// count (G_Damage clears the flag itself the next time it is hit).
static qboolean RP_JumpKickProtected( const gentity_t *target )
{
	if ( (target->flags & FL_GODMODE) && target->s.eType != ET_NPC )
	{
		return qtrue;
	}

	if ( (target->client->ps.eFlags & EF_INVULNERABLE) && target->client->invulnerableTimer > level.time )
	{
		return qtrue;
	}

	return qfalse;
}

// vehicles, and the NPCs Force Push already will not move (AT-ST, Galak's mech, the rancor) plus the
// wampa: kicked, they take the damage and stay where they are.
static qboolean RP_JumpKickImmovable( const gentity_t *target )
{
	if ( target->s.eType != ET_NPC )
	{
		return qfalse;
	}

	switch ( target->client->NPC_class )
	{
	case CLASS_VEHICLE:
	case CLASS_ATST:
	case CLASS_GALAKMECH:
	case CLASS_RANCOR:
	case CLASS_WAMPA:
		return qtrue;
	default:
		return qfalse;
	}
}

static void RP_JumpKickLand( gentity_t *ent )
{
	gclient_t *client = ent->client;
	gentity_t *faceKicked = &g_entities[client->ps.forceKickFlip-1];
	qboolean protectedTarget = qfalse;
	qboolean immovable = qfalse;
	vec3_t fwdAngles, pushDir;

	// GalaxyRP fix: [Guardian] dropped the zyk_can_hit_boss_battle_target(ent, faceKicked) conjunct
	// here — the function was a stub always returning qtrue.
	if (!faceKicked->client || (OnSameTeam(ent, faceKicked) && !g_friendlyFire.integer) ||
		(faceKicked->client->ps.duelInProgress && faceKicked->client->ps.duelIndex != ent->s.number) ||
		(ent->client->ps.duelInProgress && ent->client->ps.duelIndex != faceKicked->s.number) ||
		!zyk_can_hit_target(ent, faceKicked))
	{ // zyk: also validates if we can hit this target
		return;
	}

	if ( faceKicked->health <= 0 || !faceKicked->takedamage )
	{
		return;
	}

	// judged before the damage, which may itself knock the target down
	protectedTarget = RP_JumpKickProtected( faceKicked );
	immovable = RP_JumpKickImmovable( faceKicked );

	VectorSet( fwdAngles, 0, client->ps.viewangles[YAW], 0 );
	AngleVectors( fwdAngles, pushDir, NULL, NULL );

	// no knockback from the damage either for those two: G_Damage() adds it before it looks at spawn
	// protection or godmode, so a protected target would still be nudged by a hit it takes nothing from
	G_Damage( faceKicked, ent, ent, pushDir, client->ps.origin, rp_melee_jump_kick_damage.integer,
		(protectedTarget || immovable) ? (DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK) : DAMAGE_NO_ARMOR, MOD_MELEE );

	// kill credit, whatever else happens below. G_Damage() gives it too whenever the hit has knockback
	// (and 25 seconds for a vehicle), so this matters where it has none; it never shortens a longer
	// credit the kicker already holds. Not on a target the kick has just killed: the credit is for a
	// death that comes later, and there is none to come.
	if ( faceKicked->health > 0 &&
		( faceKicked->client->ps.otherKiller != ent->s.number || faceKicked->client->ps.otherKillerTime < level.time + 5000 ) )
	{
		faceKicked->client->ps.otherKiller = ent->s.number;
		faceKicked->client->ps.otherKillerTime = level.time + 5000;
		faceKicked->client->ps.otherKillerDebounceTime = level.time + 100;
	}

	if ( !protectedTarget && !immovable &&
		( faceKicked->client->ps.weapon != WP_SABER ||
		  faceKicked->client->ps.fd.saberAnimLevel != FORCE_LEVEL_3 ||
		  (!BG_SaberInAttack(faceKicked->client->ps.saberMove) && !PM_SaberInStart(faceKicked->client->ps.saberMove) && !PM_SaberInReturn(faceKicked->client->ps.saberMove) && !PM_SaberInTransition(faceKicked->client->ps.saberMove)) ) )
	{
		if (faceKicked->health > 0 &&
			faceKicked->client->ps.stats[STAT_HEALTH] > 0 &&
			faceKicked->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
		{
			if (BG_KnockDownable(&faceKicked->client->ps) && Q_irand(1, 10) <= 3)
			{ //only actually knock over sometimes, but always do velocity hit
				faceKicked->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
				faceKicked->client->ps.forceHandExtendTime = level.time + 1100;
				faceKicked->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
			}

			faceKicked->client->ps.velocity[0] = pushDir[0]*RP_JUMP_KICK_PUSH;
			faceKicked->client->ps.velocity[1] = pushDir[1]*RP_JUMP_KICK_PUSH;
			faceKicked->client->ps.velocity[2] = RP_JUMP_KICK_LIFT;
		}
	}

	G_Sound( faceKicked, CHAN_AUTO, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );
}

void ClientThink_real( gentity_t *ent ) {
	gclient_t	*client;
	pmove_t		pmove;
	int			oldEventSequence;
	int			msec;
	usercmd_t	*ucmd;
	qboolean	isNPC = qfalse;
	qboolean	controlledByPlayer = qfalse;
	qboolean	killJetFlags = qtrue;
	qboolean	isFollowing;

	client = ent->client;

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED && !isNPC) {
		return;
	}

	// GalaxyRP fix: [Model] fire any forced kill deferred by select_player_character()/
	// select_account_and_default_character_data() (see pending_relog_kill_time's declaration in
	// g_local.h for the full race this avoids) once the buffer window has elapsed.
	//
	// GalaxyRP fix: [Account] ...but only if the player is still in the world. The schedule is a
	// decision taken 300ms ago by zyk_relog_kill_required(), which refuses a spectator outright --
	// there is no advantage to take away from someone who is not playing. Nothing re-asked that
	// question here, so a player who left the world inside the buffer window was killed anyway.
	//
	// rp_loginRequired made that the normal outcome of /logout rather than a corner case. The
	// sequence: /logout schedules the respawn while the player is still on a team; ClientTimerActions()
	// forces them to Spectator on the very next frame, and SetTeam() kills them on the way out as it
	// does for any team change; then 300ms later this fired a SECOND kill on the spectator they had
	// become. One /logout, two deaths -- two EV_OBITUARY broadcasts (SVF_BROADCAST, so the whole
	// server saw it twice), two suicides on their record and two score penalties. The second death
	// only landed at all because SpectatorThink() resets pm_type to PM_SPECTATOR on the frame after
	// the move, clearing the PM_DEAD that would otherwise have made player_die() refuse it.
	//
	// The test below is the one from the spectator split further down, so "not in the world" means
	// the same thing in both places. Deliberately NOT a full re-ask of zyk_relog_kill_required():
	// that would also re-read rp_seamlesslogin, and a server flipping it inside those 300ms would
	// retroactively cancel a respawn the command had already promised. Only the "did they leave the
	// world" half can go stale in a way that makes the kill wrong, so only that half is re-checked.
	// (This used to say "and the duel state" too -- zyk_relog_kill_required() no longer looks at a
	// duel at all, because the account commands refuse a duellist before they ever reach it.)
	//
	// The timer is cleared either way, so a skipped kill cannot sit pending and fire later when the
	// player rejoins a team, which would kill them on the spawn they just asked for.
	if (client->pers.pending_relog_kill_time > 0 && level.time >= client->pers.pending_relog_kill_time)
	{
		extern void G_Kill(gentity_t* ent);

		client->pers.pending_relog_kill_time = 0;

		if (client->sess.sessionTeam != TEAM_SPECTATOR && client->tempSpectate < level.time)
		{
			// GalaxyRP fix: [Death System] bookkeeping, not a death -- see g_bookkeepingDeath in
			// g_local.h. The respawn is the price an account command pays for being used mid-fight
			// (see zyk_relog_kill_required, g_cmds.c): the player loses their position and their
			// momentum, which is the part that bites. A scoreboard death on top would charge them
			// for typing /login. G_Kill() hands the kill to the opponent when the player is
			// duelling, so the flag has to bracket the call rather than sit inside either branch.
			g_bookkeepingDeath = qtrue;
			G_Kill(ent);
			g_bookkeepingDeath = qfalse;
		}
	}

	// This code was moved here from clientThink to fix a problem with g_synchronousClients
	// being set to 1 when in vehicles.
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		if (g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

			if (veh->m_pVehicle &&
				veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent)
			{ //only take input from the pilot...
				veh->client->ps.commandTime = ent->client->ps.commandTime;
				memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
				if ( veh->m_pVehicle->m_ucmd.buttons & BUTTON_TALK )
				{ //forced input if "chat bubble" is up
					veh->m_pVehicle->m_ucmd.buttons = BUTTON_TALK;
					veh->m_pVehicle->m_ucmd.forwardmove = 0;
					veh->m_pVehicle->m_ucmd.rightmove = 0;
					veh->m_pVehicle->m_ucmd.upmove = 0;
				}
			}
		}
	}

	isFollowing = (client->ps.pm_flags & PMF_FOLLOW) ? qtrue : qfalse;

	if (!isFollowing)
	{
		if (level.gametype == GT_SIEGE &&
			client->siegeClass != -1 &&
			bgSiegeClasses[client->siegeClass].saberStance)
		{ //the class says we have to use this stance set.
			if (!(bgSiegeClasses[client->siegeClass].saberStance & (1 << client->ps.fd.saberAnimLevel)))
			{ //the current stance is not in the bitmask, so find the first one that is.
				int i = SS_FAST;

				while (i < SS_NUM_SABER_STYLES)
				{
					if (bgSiegeClasses[client->siegeClass].saberStance & (1 << i))
					{
						if (i == SS_DUAL
							&& client->ps.saberHolstered == 1 )
						{//one saber should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = SS_FAST;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else if ( i == SS_STAFF
							&& client->ps.saberHolstered == 1
							&& client->saber[0].singleBladeStyle != SS_NONE)
						{//one saber or blade should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else
						{
							client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = i;
							client->ps.fd.saberDrawAnimLevel = i;
						}
						break;
					}

					i++;
				}
			}
		}
		else if (client->saber[0].model[0] && client->saber[1].model[0])
		{ //with two sabs always use akimbo style
			if ( client->ps.saberHolstered == 1 )
			{//one saber should be off, adjust saberAnimLevel accordinly
				client->ps.fd.saberAnimLevelBase = SS_DUAL;
				client->ps.fd.saberAnimLevel = SS_FAST;
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
			else
			{
				if ( !WP_SaberStyleValidForSaber( &client->saber[0], &client->saber[1], client->ps.saberHolstered, client->ps.fd.saberAnimLevel ) )
				{//only use dual style if the style we're trying to use isn't valid
					client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = SS_DUAL;
				}
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
		}
		else
		{
			if (client->saber[0].stylesLearned == (1<<SS_STAFF) )
			{ //then *always* use the staff style
				client->ps.fd.saberAnimLevelBase = SS_STAFF;
			}
			if ( client->ps.fd.saberAnimLevelBase == SS_STAFF )
			{//using staff style
				if ( client->ps.saberHolstered == 1
					&& client->saber[0].singleBladeStyle != SS_NONE)
				{//one blade should be off, adjust saberAnimLevel accordinly
					client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
				else
				{
					client->ps.fd.saberAnimLevel = SS_STAFF;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
			}
		}
	}

	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

	if ( client && !isFollowing && (client->ps.eFlags2&EF2_HELD_BY_MONSTER) )
	{
		G_HeldByMonster( ent, ucmd );
	}

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
//		trap->Print("serverTime <<<<<\n" );
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
//		trap->Print("serverTime >>>>>\n" );
	}

	if (isNPC && (ucmd->serverTime - client->ps.commandTime) < 1)
	{
		ucmd->serverTime = client->ps.commandTime + 100;
	}

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}

	if ( msec > 200 ) {
		msec = 200;
	}

	RP_AfkThink( ent, ucmd );	// GLX_U: [AFK] before Pmove

	if ( pmove_msec.integer < 8 ) {
		trap->Cvar_Set("pmove_msec", "8");
	}
	else if (pmove_msec.integer > 33) {
		trap->Cvar_Set("pmove_msec", "33");
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
		//if (ucmd->serverTime - client->ps.commandTime <= 0)
		//	return;
	}

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime )
	{
		if ( ent->s.number < MAX_CLIENTS
			|| client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			ClientIntermissionThink( client );
			return;
		}
	}

	// spectators don't do much
	// GalaxyRP fix: [NPC] real clients only. NPCTEAM_NEUTRAL and TEAM_SPECTATOR are both 3, and an
	// NPC's sess.sessionTeam is read straight from its spawner's "team" key (NPC_spawn.c), so a
	// neutral NPC spawned that way ran SpectatorThink() instead of moving.
	if ( client->ps.clientNum < MAX_CLIENTS && ( client->sess.sessionTeam == TEAM_SPECTATOR || client->tempSpectate >= level.time ) ) {
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			return;
		}
		SpectatorThink( ent, ucmd );
		return;
	}

	if (ent && ent->client && (ent->client->ps.eFlags & EF_INVULNERABLE))
	{
		if (ent->client->invulnerableTimer <= level.time)
		{
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
		}
	}

	if (ent->s.eType != ET_NPC)
	{
		// check for inactivity timer, but never drop the local client of a non-dedicated server
		if ( !ClientInactivityTimer( client ) ) {
			return;
		}
	}

	//Check if we should have a fullbody push effect around the player
	if (client->pushEffectTime > level.time)
	{
		client->ps.eFlags |= EF_BODYPUSH;
	}
	else if (client->pushEffectTime)
	{
		client->pushEffectTime = 0;
		client->ps.eFlags &= ~EF_BODYPUSH;
	}

	if (client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
	{
		client->ps.eFlags |= EF_JETPACK;
	}
	else
	{
		client->ps.eFlags &= ~EF_JETPACK;
	}

	// GalaxyRP fix: [Skills] the blue jetpack exhaust flame used to be pushed to the client as an
	// EV_ITEMUSEFAIL event (parm 7 on / 8 off) and cached there in cg.rpg_stuff / cg.zyk_rpg_stuff[].
	// One push into a client-side cache had four separate failure modes: events ride the snapshot and
	// are PVS-culled, so anyone who could not see the player during the ~3s window after their spawn
	// never learned; nothing re-announced afterwards; the cache was only ever zeroed in CG_Init, so a
	// reused client slot inherited the previous occupant's colour; and the render had to ask "is this
	// me?" via cg.snap->ps.clientNum, which is the FOLLOWED player while spectating, so a spectator saw
	// their own flame colour on whoever they were watching.
	//
	// Carrying it as an entity-state bit removes all four at once, because none of that machinery is
	// left: the bit is in every snapshot for every player the client can see, it is recomputed here
	// every frame rather than cached, and cg_players.c reads it off the same `cent` as the EF_JETPACK
	// gate around it. Set unconditionally in both directions, so /skillup, /skilldown, login, logout
	// and leaving RPG Mode all take effect on the next frame with nothing to resend.
	if (client->sess.amrpgmode == 2 && client->pers.skill_levels[34] == 3)
	{
		client->ps.eFlags |= EF_RPG_JETPACK_UPGRADE;
	}
	else
	{
		client->ps.eFlags &= ~EF_RPG_JETPACK_UPGRADE;
	}

	// zyk: paralyzed by an admin. Keep him this way
	// GalaxyRP fix: [Death System] hoisted out of the movetype chain below, and it now asserts the
	// knockdown instead of only postponing its expiry.
	//
	// It used to sit in the innermost else of that chain -- not noclipping, not disintegrating, not
	// dead, and not Force Gripped -- so any of those four states stopped it. And all it wrote was
	// forceHandExtendTime, which keeps a downed player down only indirectly, by starving the get-up
	// machine in WP_ForcePowersUpdate() (w_force.c) of its "forceHandExtendTime < level.time"
	// condition. Half a second of lapse was enough for that machine to run, and G_SpecialRollGetup()
	// reads the downed player's own movement keys: held in a Force Grip and shoved around, they
	// rolled to their feet, and a second lapse carried them to HANDEXTEND_WEAPONREADY and full
	// mobility while still flagged downed. Resuming the refresh afterwards held nothing, because
	// forceHandExtend had already been changed and this never set it back.
	//
	// Writing all three fields makes the state self-repairing: whatever overwrote forceHandExtend is
	// undone on the next think. That also clears up a long-standing cosmetic -- Force Grip's squeeze
	// sets HANDEXTEND_CHOKE on its victim, which used to stick for the rest of the countdown, so a
	// gripped player spent it choking instead of lying down. forceDodgeAnim 0 is the "still down"
	// variant in bg_pmove.c's HANDEXTEND_KNOCKDOWN case (non-zero selects a get-up animation), and is
	// what RP_EnterDownedState() sets on the way in.
	//
	// Running above the chain means this also applies while gripped, so the knockdown pose now wins
	// over the choke pose. The real guarantee is the gate in WP_ForcePowersUpdate(); this is the
	// repair that keeps the presentation honest and stops any single missed frame mattering.
	if (client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))
	{
		client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		client->ps.forceDodgeAnim = 0;
		client->ps.forceHandExtendTime = level.time + 500;
	}

	// GalaxyRP fix: [Jetpack] a rider never has the jetpack on. Board() (g_vehicles.c) switches it off when a
	// player gets in; this catches any other way into a vehicle. Left on, the rider kept its flames, sound and fuel
	// drain until the fuel ran out -- and could not switch it off, since the use key, which toggles the jetpack,
	// gets out of the vehicle instead (TryUse()). killJetFlags below then clears the flags this frame.
	if ( client->jetPackOn && client->ps.m_iVehicleNum )
	{
		Jetpack_Off( ent );
	}

	if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.eFlags & EF_DISINTEGRATION ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))) {
		client->ps.pm_type = PM_DEAD;
	} else {
		if (client->ps.forceGripChangeMovetype)
		{
			client->ps.pm_type = client->ps.forceGripChangeMovetype;
		}
		else
		{
			if (client->jetPackOn)
			{
				client->ps.pm_type = PM_JETPACK;
				client->ps.eFlags |= EF_JETPACK_ACTIVE;
				killJetFlags = qfalse;
				client->ps.fd.forcePowersActive &= ~(1 << FP_LEVITATION); // zyk: stop jumping if using jetpack
			}
			else
			{
				client->ps.pm_type = PM_NORMAL;
			}
		}
	}

	if (killJetFlags)
	{
		client->ps.eFlags &= ~EF_JETPACK_ACTIVE;
		client->ps.eFlags &= ~EF_JETPACK_FLAMING;
	}

#define	SLOWDOWN_DIST	128.0f
#define	MIN_NPC_SPEED	16.0f

	if (client->bodyGrabIndex != ENTITYNUM_NONE)
	{
		gentity_t *grabbed = &g_entities[client->bodyGrabIndex];

		if (!grabbed->inuse || grabbed->s.eType != ET_BODY ||
			(grabbed->s.eFlags & EF_DISINTEGRATION) ||
			(grabbed->s.eFlags & EF_NODRAW))
		{
			if (grabbed->inuse && grabbed->s.eType == ET_BODY)
			{
				grabbed->s.ragAttach = 0;
			}
			client->bodyGrabIndex = ENTITYNUM_NONE;
		}
		else
		{
			mdxaBone_t rhMat;
			vec3_t rhOrg, tAng;
			vec3_t bodyDir;
			float bodyDist;

			ent->client->ps.forceHandExtend = HANDEXTEND_DRAGGING;

			if (ent->client->ps.forceHandExtendTime < level.time + 500)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1000;
			}

			VectorSet(tAng, 0, ent->client->ps.viewangles[YAW], 0);
			trap->G2API_GetBoltMatrix(ent->ghoul2, 0, 0, &rhMat, tAng, ent->client->ps.origin, level.time,
				NULL, ent->modelScale); //0 is always going to be right hand bolt
			BG_GiveMeVectorFromMatrix(&rhMat, ORIGIN, rhOrg);

			VectorSubtract(rhOrg, grabbed->r.currentOrigin, bodyDir);
			bodyDist = VectorLength(bodyDir);

			if (bodyDist > 40.0f)
			{ //can no longer reach
				grabbed->s.ragAttach = 0;
				client->bodyGrabIndex = ENTITYNUM_NONE;
			}
			else if (bodyDist > 24.0f)
			{
				bodyDir[2] = 0; //don't want it floating
				//VectorScale(bodyDir, 0.1f, bodyDir);
				VectorAdd(grabbed->epVelocity, bodyDir, grabbed->epVelocity);
				G_Sound(grabbed, CHAN_AUTO, G_SoundIndex("sound/player/roll1.wav"));
			}
		}
	}
	else if (ent->client->ps.forceHandExtend == HANDEXTEND_DRAGGING)
	{
		ent->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
	}

	if (ent->NPC && ent->s.NPC_class != CLASS_VEHICLE) //vehicles manage their own speed
	{
		//FIXME: swoop should keep turning (and moving forward?) for a little bit?
		if ( ent->NPC->combatMove == qfalse )
		{
			//if ( !(ucmd->buttons & BUTTON_USE) )
			if (1)
			{//Not leaning
				qboolean Flying = (ucmd->upmove && (ent->client->ps.eFlags2&EF2_FLYING));//ent->client->moveType == MT_FLYSWIM);
				qboolean Climbing = (ucmd->upmove && ent->watertype&CONTENTS_LADDER );

				//client->ps.friction = 6;

				if ( ucmd->forwardmove || ucmd->rightmove || Flying )
				{
					//if ( ent->NPC->behaviorState != BS_FORMATION )
					{//In - Formation NPCs set thier desiredSpeed themselves
						if ( ucmd->buttons & BUTTON_WALKING )
						{
							ent->NPC->desiredSpeed = NPC_GetWalkSpeed( ent );//ent->NPC->stats.walkSpeed;
						}
						else//running
						{
							ent->NPC->desiredSpeed = NPC_GetRunSpeed( ent );//ent->NPC->stats.runSpeed;
						}

						if ( ent->NPC->currentSpeed >= 80 && !controlledByPlayer )
						{//At higher speeds, need to slow down close to stuff
							//Slow down as you approach your goal
							if ( ent->NPC->distToGoal < SLOWDOWN_DIST && !(ent->NPC->aiFlags&NPCAI_NO_SLOWDOWN) )//128
							{
								if ( ent->NPC->desiredSpeed > MIN_NPC_SPEED )
								{
									float slowdownSpeed = ((float)ent->NPC->desiredSpeed) * ent->NPC->distToGoal / SLOWDOWN_DIST;

									ent->NPC->desiredSpeed = ceil(slowdownSpeed);
									if ( ent->NPC->desiredSpeed < MIN_NPC_SPEED )
									{//don't slow down too much
										ent->NPC->desiredSpeed = MIN_NPC_SPEED;
									}
								}
							}
						}
					}
				}
				else if ( Climbing )
				{
					ent->NPC->desiredSpeed = ent->NPC->stats.walkSpeed;
				}
				else
				{//We want to stop
					ent->NPC->desiredSpeed = 0;
				}

				NPC_Accelerate( ent, qfalse, qfalse );

				if ( ent->NPC->currentSpeed <= 24 && ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
				{//No-one walks this slow
					client->ps.speed = ent->NPC->currentSpeed = 0;//Full stop
					ucmd->forwardmove = 0;
					ucmd->rightmove = 0;
				}
				else
				{
					if ( ent->NPC->currentSpeed <= ent->NPC->stats.walkSpeed )
					{//Play the walkanim
						ucmd->buttons |= BUTTON_WALKING;
					}
					else
					{
						ucmd->buttons &= ~BUTTON_WALKING;
					}

					if ( ent->NPC->currentSpeed > 0 )
					{//We should be moving
						if ( Climbing || Flying )
						{
							if ( !ucmd->upmove )
							{//We need to force them to take a couple more steps until stopped
								ucmd->upmove = ent->NPC->last_ucmd.upmove;//was last_upmove;
							}
						}
						else if ( !ucmd->forwardmove && !ucmd->rightmove )
						{//We need to force them to take a couple more steps until stopped
							ucmd->forwardmove = ent->NPC->last_ucmd.forwardmove;//was last_forwardmove;
							ucmd->rightmove = ent->NPC->last_ucmd.rightmove;//was last_rightmove;
						}
					}

					client->ps.speed = ent->NPC->currentSpeed;
				//	if ( player && player->client && player->client->ps.viewEntity == ent->s.number )
				//	{
				//	}
				//	else
					//rwwFIXMEFIXME: do this and also check for all real client
					if (1)
					{
						//Slow down on turns - don't orbit!!!
						float turndelta = 0;
						// if the NPC is locked into a Yaw, we want to check the lockedDesiredYaw...otherwise the NPC can't walk backwards, because it always thinks it trying to turn according to desiredYaw
						//if( client->renderInfo.renderFlags & RF_LOCKEDANGLE ) // yeah I know the RF_ flag is a pretty ugly hack...
						if (0) //rwwFIXMEFIXME: ...
						{
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->lockedDesiredYaw ) ))/180;
						}
						else
						{
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->desiredYaw ) ))/180;
						}

						if ( turndelta < 0.75f )
						{
							client->ps.speed = 0;
						}
						else if ( ent->NPC->distToGoal < 100 && turndelta < 1.0 )
						{//Turn is greater than 45 degrees or closer than 100 to goal
							client->ps.speed = floor(((float)(client->ps.speed))*turndelta);
						}
					}
				}
			}
		}
		else
		{
			ent->NPC->desiredSpeed = ( ucmd->buttons & BUTTON_WALKING ) ? NPC_GetWalkSpeed( ent ) : NPC_GetRunSpeed( ent );

			client->ps.speed = ent->NPC->desiredSpeed;
		}

		if (ucmd->buttons & BUTTON_WALKING)
		{ //sort of a hack I guess since MP handles walking differently from SP (has some proxy cheat prevention methods)
			/*
			if (ent->client->ps.speed > 64)
			{
				ent->client->ps.speed = 64;
			}
			*/

			if (ucmd->forwardmove > 64)
			{
				ucmd->forwardmove = 64;
			}
			else if (ucmd->forwardmove < -64)
			{
				ucmd->forwardmove = -64;
			}

			if (ucmd->rightmove > 64)
			{
				ucmd->rightmove = 64;
			}
			else if ( ucmd->rightmove < -64)
			{
				ucmd->rightmove = -64;
			}

			//ent->client->ps.speed = ent->client->ps.basespeed = NPC_GetRunSpeed( ent );
		}

		if (client->pers.stun_baton_less_speed_timer > level.time)
		{ // zyk: stun baton 3/3 decreases speed
			client->ps.speed /= 2;
		}

		client->ps.basespeed = client->ps.speed;
	}
	else if (!client->ps.m_iVehicleNum &&
		(!ent->NPC || ent->s.NPC_class != CLASS_VEHICLE)) //if riding a vehicle it will manage our speed and such
	{
		float zyk_player_speed = g_speed.value;

		//Check for a siege class speed multiplier
		if (level.gametype == GT_SIEGE &&
			client->siegeClass != -1)
		{
			zyk_player_speed *= bgSiegeClasses[client->siegeClass].speed;
		}

		if (client->bodyGrabIndex != ENTITYNUM_NONE)
		{ //can't go nearly as fast when dragging a body around
			zyk_player_speed *= 0.2f;
		}

		if (client->pers.stun_baton_less_speed_timer > level.time)
		{ // zyk: stun baton 3/3 decreases speed
			zyk_player_speed /= 2;
		}

		// GalaxyRP (Alex): [Armor Skill] Armor reduces your movement based on your level. Better the armor, slower the wearer.
		// GalaxyRP fix: [Armor Skill] this ran unconditionally, unlike the matching damage-resistance
		// half of this same skill in g_combat.c (which is correctly gated on amrpgmode == 2). Since
		// skill_levels[] is never reset on logout, a player's last character's Armor level kept
		// slowing them down indefinitely after /logout (even through /kill and respawn) until they
		// logged into a character with Armor level 0. Added the same amrpgmode == 2 gate.
		if (client->sess.amrpgmode == 2 && client->pers.skill_levels[56] > 0)
		{
			zyk_player_speed = zyk_player_speed - (client->pers.skill_levels[56] * zyk_player_speed * 0.1);
		}

		client->ps.speed = zyk_player_speed;
		client->ps.basespeed = zyk_player_speed;

		// GalaxyRP fix: [Dead Code] removed the Force Dash trigger (rpg_class==7); pers.rpg_class
		// is always 0 server-side, so this was unreachable.
	}

	if ( !ent->NPC || !(ent->NPC->aiFlags&NPCAI_CUSTOM_GRAVITY) )
	{//use global gravity
		if (ent->NPC && ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->gravity)
		{ //use custom veh gravity
			client->ps.gravity = ent->m_pVehicle->m_pVehicleInfo->gravity;
		}
		else
		{
			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //in space, so no gravity...
				client->ps.gravity = 1.0f;
				if (ent->s.number < MAX_CLIENTS)
				{
					VectorScale(client->ps.velocity, 0.8f, client->ps.velocity);
				}
			}
			else
			{
				if (client->ps.eFlags2 & EF2_SHIP_DEATH)
				{ //float there
					VectorClear(client->ps.velocity);
					client->ps.gravity = 1.0f;
				}
				else
				{
					client->ps.gravity = g_gravity.value;
				}
			}
		}
	}

	if (ent->client->ps.duelInProgress)
	{
		gentity_t *duelAgainst = &g_entities[ent->client->ps.duelIndex];

		//Keep the time updated, so once this duel ends this player can't engage in a duel for another
		//10 seconds. This will give other people a chance to engage in duels in case this player wants
		//to engage again right after he's done fighting and someone else is waiting.
		ent->client->ps.fd.privateDuelTime = level.time + 10000;

		if (ent->client->ps.duelTime < level.time)
		{
			//Bring out the sabers
			if (ent->client->ps.weapon == WP_SABER
				&& ent->client->ps.saberHolstered
				&& ent->client->ps.duelTime )
			{
				ent->client->ps.saberHolstered = 0;

				if (ent->client->saber[0].soundOn)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
				}
				if (ent->client->saber[1].soundOn)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				}

				G_AddEvent(ent, EV_PRIVATE_DUEL, 2);

				ent->client->ps.duelTime = 0;

				// zyk: this will guarantee that the player has full health at start of the duel
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
				// GalaxyRP fix: [Duel] this used to apply the RPG Max Shield formula to everyone, with no
				// amrpgmode check. skill_levels[] and max_rpg_health are never reset on logout, so a
				// logged-out duellist got either 0 (never logged in on this connection) or a stale value
				// carried over from whichever character they last played -- a private duel's starting
				// shield depended on account history rather than on any setting. Gated now, and reading
				// pers.max_rpg_shield rather than recomputing the formula inline: set_max_shield() in
				// g_cmds.c keeps that field current on spawn, on level change and on every Max Shield
				// /skillup, and computes exactly this expression.
				ent->client->ps.stats[STAT_ARMOR] = (ent->client->sess.amrpgmode == 2) ? ent->client->pers.max_rpg_shield : rp_starting_shield.integer;
			}

			if (duelAgainst
				&& duelAgainst->client
				&& duelAgainst->inuse
				&& duelAgainst->client->ps.weapon == WP_SABER
				&& duelAgainst->client->ps.saberHolstered
				&& duelAgainst->client->ps.duelTime)
			{
				duelAgainst->client->ps.saberHolstered = 0;

				if (duelAgainst->client->saber[0].soundOn)
				{
					G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[0].soundOn);
				}
				if (duelAgainst->client->saber[1].soundOn)
				{
					G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[1].soundOn);
				}

				G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 2);

				duelAgainst->client->ps.duelTime = 0;

				// zyk: this will guarantee that the player has full health at start of the duel TEST
				duelAgainst->health = duelAgainst->client->ps.stats[STAT_MAX_HEALTH];
				// GalaxyRP fix: [Duel] the opponent's side of the same fix applied to the challenger just
				// above -- see that comment for why the ungated formula was wrong for a logged-out
				// duellist. Both sides must stay identical or the two duellists enter on different rules.
				duelAgainst->client->ps.stats[STAT_ARMOR] = (duelAgainst->client->sess.amrpgmode == 2) ? duelAgainst->client->pers.max_rpg_shield : rp_starting_shield.integer;
			}
		}
		else
		{
			client->ps.speed = 0;
			client->ps.basespeed = 0;
			ucmd->forwardmove = 0;
			ucmd->rightmove = 0;
			ucmd->upmove = 0;
		}

		if (!duelAgainst || !duelAgainst->client || !duelAgainst->inuse ||
			duelAgainst->client->ps.duelIndex != ent->s.number)
		{
			ent->client->ps.duelInProgress = 0;
			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
		}
		else if (duelAgainst->health < 1 || duelAgainst->client->ps.stats[STAT_HEALTH] < 1 || duelAgainst->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))
		{
			int old_health = ent->health;
			int old_shield = ent->client->ps.stats[STAT_ARMOR];

			ent->client->ps.duelInProgress = 0;
			duelAgainst->client->ps.duelInProgress = 0;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
			G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

			//Winner gets full health.. providing he's still alive
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				ent->client->ps.stats[STAT_HEALTH] = ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
				// GalaxyRP fix: [Duel] rp_starting_shield is the shield a LOGGED-OUT player spawns with;
				// an RPG character's is pers.max_rpg_shield, from their Max Shield skill. This line
				// handed the logged-out value to everyone, so an RPG duellist who WON came out of the
				// duel with their shield reset to that cvar (0 in the shipped config) instead of the
				// full shield the line above gives them in health -- winning a duel strictly cost them
				// shield. Mirrors the two duel-start sites earlier in this function.
				ent->client->ps.stats[STAT_ARMOR] = (ent->client->sess.amrpgmode == 2) ? ent->client->pers.max_rpg_shield : rp_starting_shield.integer;

				if (g_spawnInvulnerability.integer)
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
				}
			}

			/*
			trap->SendServerCommand( ent-g_entities, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER")) );
			trap->SendServerCommand( duelAgainst-g_entities, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER")) );
			*/
			//Private duel announcements are now made globally because we only want one duel at a time.
			// zyk: now announce in console
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0 && !(client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)))
			{
				trap->SendServerCommand( -1, va("print \"%s ^7%s %s^7, ending with ^1%d^7/^2%d^7!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"), duelAgainst->client->pers.netname, old_health, old_shield) );
			}
			else
			{ //it was a draw, because we both managed to die in the same frame
				trap->SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")) );
			}
		}
		else
		{
			vec3_t vSub;
			float subLen = 0;

			VectorSubtract(ent->client->ps.origin, duelAgainst->client->ps.origin, vSub);
			subLen = VectorLength(vSub);

			if (subLen >= rp_duel_radius.integer)
			{
				ent->client->ps.duelInProgress = 0;
				duelAgainst->client->ps.duelInProgress = 0;

				G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
				G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

				trap->SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELSTOP")) );
			}
		}
	}

	if (ent->client->doingThrow > level.time)
	{
		gentity_t *throwee = &g_entities[ent->client->throwingIndex];

		if (!throwee->inuse || !throwee->client || throwee->health < 1 ||
			throwee->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(throwee->client->ps.pm_flags & PMF_FOLLOW) ||
			throwee->client->throwingIndex != ent->s.number)
		{
			ent->client->doingThrow = 0;
			ent->client->ps.forceHandExtend = HANDEXTEND_NONE;

			if (throwee->inuse && throwee->client)
			{
				throwee->client->ps.heldByClient = 0;
				throwee->client->beingThrown = 0;

				if (throwee->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
				{
					throwee->client->ps.forceHandExtend = HANDEXTEND_NONE;
				}
			}
		}
	}

	if (ent->client->beingThrown > level.time)
	{
		gentity_t *thrower = &g_entities[ent->client->throwingIndex];

		if (!thrower->inuse || !thrower->client || thrower->health < 1 ||
			thrower->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(thrower->client->ps.pm_flags & PMF_FOLLOW) ||
			thrower->client->throwingIndex != ent->s.number)
		{
			ent->client->ps.heldByClient = 0;
			ent->client->beingThrown = 0;

			if (ent->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}

			if (thrower->inuse && thrower->client)
			{
				thrower->client->doingThrow = 0;
				thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
		}
		else if (thrower->inuse && thrower->client && thrower->ghoul2 &&
			trap->G2API_HaveWeGhoul2Models(thrower->ghoul2))
		{
#if 0
			int lHandBolt = trap->G2API_AddBolt(thrower->ghoul2, 0, "*l_hand");
			int pelBolt = trap->G2API_AddBolt(thrower->ghoul2, 0, "pelvis");


			if (lHandBolt != -1 && pelBolt != -1)
#endif
			{
				float pDif = 40.0f;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles;
				vec3_t vDif;
				vec3_t entDir, otherAngles;
				vec3_t fwd, right;

				//Always look at the thrower.
				VectorSubtract( thrower->client->ps.origin, ent->client->ps.origin, entDir );
				VectorCopy( ent->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( ent, otherAngles );

				VectorCopy(thrower->client->ps.viewangles, tAngles);
				tAngles[PITCH] = tAngles[ROLL] = 0;

				//Get the direction between the pelvis and position of the hand
#if 0
				mdxaBone_t boltMatrix, pBoltMatrix;

				trap->G2API_GetBoltMatrix(thrower->ghoul2, 0, lHandBolt, &boltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				boltOrg[0] = boltMatrix.matrix[0][3];
				boltOrg[1] = boltMatrix.matrix[1][3];
				boltOrg[2] = boltMatrix.matrix[2][3];

				trap->G2API_GetBoltMatrix(thrower->ghoul2, 0, pelBolt, &pBoltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				pBoltOrg[0] = pBoltMatrix.matrix[0][3];
				pBoltOrg[1] = pBoltMatrix.matrix[1][3];
				pBoltOrg[2] = pBoltMatrix.matrix[2][3];
#else //above tends to not work once in a while, for various reasons I suppose.
				VectorCopy(thrower->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];
#endif
				//G_TestLine(boltOrg, pBoltOrg, 0x0000ff, 50);

				VectorSubtract(ent->client->ps.origin, boltOrg, vDif);
				if (VectorLength(vDif) > 32.0f && (thrower->client->doingThrow - level.time) < 4500)
				{ //the hand is too far away, and can no longer hold onto us, so escape.
					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					ent->client->ps.forceDodgeAnim = 2;
					ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
					ent->client->ps.forceHandExtendTime = level.time + 500;
					ent->client->ps.velocity[2] = 400;
					G_PreDefSound(ent->client->ps.origin, PDSOUND_FORCEJUMP);
				}
				else if ((client->beingThrown - level.time) < 4000)
				{ //step into the next part of the throw, and go flying back
					float vScale = 400.0f;
					ent->client->ps.forceHandExtend = HANDEXTEND_POSTTHROWN;
					ent->client->ps.forceHandExtendTime = level.time + 1200;
					ent->client->ps.forceDodgeAnim = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_POSTTHROW;
					thrower->client->ps.forceHandExtendTime = level.time + 200;

					ent->client->ps.heldByClient = 0;

					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					AngleVectors(thrower->client->ps.viewangles, vDif, 0, 0);
					ent->client->ps.velocity[0] = vDif[0]*vScale;
					ent->client->ps.velocity[1] = vDif[1]*vScale;
					ent->client->ps.velocity[2] = 400;

					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*jump1.wav") );

					//Set the thrower as the "other killer", so if we die from fall/impact damage he is credited.
					ent->client->ps.otherKiller = thrower->s.number;
					ent->client->ps.otherKillerTime = level.time + 8000;
					ent->client->ps.otherKillerDebounceTime = level.time + 100;
				}
				else
				{ //see if we can move to be next to the hand.. if it's not clear, break the throw.
					vec3_t intendedOrigin;
					trace_t tr;
					trace_t tr2;

					VectorSubtract(boltOrg, pBoltOrg, vDif);
					VectorNormalize(vDif);

					VectorClear(ent->client->ps.velocity);
					intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
					intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
					intendedOrigin[2] = thrower->client->ps.origin[2];

					trap->Trace(&tr, intendedOrigin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, ent->clipmask, qfalse, 0, 0);
					trap->Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID, qfalse, 0, 0);

					if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
					{
						VectorCopy(intendedOrigin, ent->client->ps.origin);

						if ((client->beingThrown - level.time) < 4800)
						{
							ent->client->ps.heldByClient = thrower->s.number+1;
						}
					}
					else
					{ //if the guy can't be put here then it's time to break the throw off.
						ent->client->ps.heldByClient = 0;
						ent->client->beingThrown = 0;
						thrower->client->doingThrow = 0;

						thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
						G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

						ent->client->ps.forceDodgeAnim = 2;
						ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
						ent->client->ps.forceHandExtendTime = level.time + 500;
						ent->client->ps.velocity[2] = 400;
						G_PreDefSound(ent->client->ps.origin, PDSOUND_FORCEJUMP);
					}
				}
			}
		}
	}
	else if (ent->client->ps.heldByClient)
	{
		ent->client->ps.heldByClient = 0;
	}

	// GalaxyRP: [Grapple Hook] the pull flag, decided fresh every frame from three facts: a hook is
	// out, it has landed (its think is Weapon_HookThink, not the in-flight Weapon_HookFree timer),
	// and the owner still qualifies. pm_flags is networked, so this one line is what the client's
	// prediction runs the pull from. A hook whose owner may no longer have one out at all is freed
	// here as well -- see RP_HookMayStayOut() above. Real players only: NPCs never fire one.
	if ( ent->s.number < MAX_CLIENTS && ent->client )
	{
		if ( ent->client->hook && !RP_HookMayStayOut( ent ) )
		{
			Weapon_HookFree( ent->client->hook );
		}

		if ( ent->client->hook && ent->client->hook->think == Weapon_HookThink && RP_CanKeepHook( ent ) )
		{
			ent->client->ps.pm_flags |= PMF_GRAPPLE;
		}
		else
		{
			ent->client->ps.pm_flags &= ~PMF_GRAPPLE;
		}
	}

	/*
	if ( client->ps.powerups[PW_HASTE] ) {
		client->ps.speed *= 1.3;
	}
	*/

	//Will probably never need this again, since we have g2 properly serverside now.
	//But just in case.
	/*
	if (client->ps.usingATST && ent->health > 0)
	{ //we have special shot clip boxes as an ATST
		ent->r.contents |= CONTENTS_NOSHOT;
		ATST_ManageDamageBoxes(ent);
	}
	else
	{
		ent->r.contents &= ~CONTENTS_NOSHOT;
		client->damageBoxHandle_Head = 0;
		client->damageBoxHandle_RLeg = 0;
		client->damageBoxHandle_LLeg = 0;
	}
	*/

	//rww - moved this stuff into the pmove code so that it's predicted properly
	//BG_AdjustClientSpeed(&client->ps, &client->pers.cmd, level.time);

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	memset (&pmove, 0, sizeof(pmove));

	if ( ent->flags & FL_FORCE_GESTURE ) {
		ent->flags &= ~FL_FORCE_GESTURE;
		ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
	}

	if (ent->client && ent->client->ps.fallingToDeath &&
		(level.time - FALL_FADE_TIME) > ent->client->ps.fallingToDeath)
	{ //die!
		if (ent->health > 0)
		{
			gentity_t *otherKiller = ent;
			if (ent->client->ps.otherKillerTime > level.time &&
				ent->client->ps.otherKiller != ENTITYNUM_NONE)
			{
				otherKiller = &g_entities[ent->client->ps.otherKiller];

				if (!otherKiller->inuse)
				{
					otherKiller = ent;
				}
			}
			G_Damage(ent, otherKiller, otherKiller, NULL, ent->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_FALLING);
			//player_die(ent, ent, ent, 100000, MOD_FALLING);
	//		if (!ent->NPC)
	//		{
	//			ClientRespawn(ent);
	//		}
	//		ent->client->ps.fallingToDeath = 0;

			G_MuteSound(ent->s.number, CHAN_VOICE); //stop screaming, because you are dead!
		}
	}

	/* zyk: now it will always keep the otherKillerTime, even if the player touches the ground
	if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum != ENTITYNUM_NONE &&
		ent->client->ps.otherKillerDebounceTime < level.time)
	{
		ent->client->ps.otherKillerTime = 0;
		ent->client->ps.otherKiller = ENTITYNUM_NONE;
	}
	else if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		if (ent->client->ps.otherKillerDebounceTime < (level.time + 100))
		{
			ent->client->ps.otherKillerDebounceTime = level.time + 100;
		}
	}
	*/

//	WP_ForcePowersUpdate( ent, msec, ucmd); //update any active force powers
//	WP_SaberPositionUpdate(ent, ucmd); //check the server-side saber point, do apprioriate server-side actions (effects are cs-only)

	//NOTE: can't put USE here *before* PMove!!
	if ( ent->client->ps.useDelay > level.time
		&& ent->client->ps.m_iVehicleNum )
	{//when in a vehicle, debounce the use...
		ucmd->buttons &= ~BUTTON_USE;
	}

	//FIXME: need to do this before check to avoid walls and cliffs (or just cliffs?)
	G_AddPushVecToUcmd( ent, ucmd );

	//play/stop any looping sounds tied to controlled movement
	G_CheckMovingLoopingSounds( ent, ucmd );

	pmove.ps = &client->ps;
	pmove.cmd = *ucmd;
	if ( pmove.ps->pm_type == PM_DEAD ) {
		pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else if ( ent->r.svFlags & SVF_BOT ) {
		pmove.tracemask = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
	}
	else {
		pmove.tracemask = MASK_PLAYERSOLID;

		if (rp_duel_no_collision.integer > 0)
		{ // zyk: makes duelists not collide with other players
			int j = 0;

			for (j = 0; j < level.maxclients; j++)
			{
				gentity_t *player_ent = &g_entities[j];

				if (player_ent && player_ent->client && ent != player_ent && player_ent->client->pers.connected == CON_CONNECTED && player_ent->client->sess.sessionTeam != TEAM_SPECTATOR)
				{
					if (((ent->client->ps.duelInProgress == qtrue && ent->client->ps.duelIndex != player_ent->s.number) || ent->client->ps.duelInProgress != player_ent->client->ps.duelInProgress) && 
						(int)Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 75)
					
					{ // zyk: duelist colliding with someone outside this duel
						pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
						break;
					}
				}
			}
		}
	}
	// GalaxyRP: [Phase] a phased player walks through bodies -- see RP_PhasePassesThrough(). Dead
	// players already trace without CONTENTS_BODY; this only matters for the living.
	if ( RP_PhasePassesThrough( ent ) )
	{
		pmove.tracemask &= ~CONTENTS_BODY;
	}

	pmove.trace = SV_PMTrace;
	pmove.pointcontents = trap->PointContents;
	pmove.debugLevel = g_debugMove.integer;
	pmove.noFootsteps = (dmflags.integer & DF_NO_FOOTSTEPS) > 0;

	pmove.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pmove.pmove_msec = pmove_msec.integer;
	pmove.pmove_float = pmove_float.integer;

	pmove.animations = bgAllAnims[ent->localAnimIndex].anims;//NULL;

	//rww - bgghoul2
	pmove.ghoul2 = NULL;

#ifdef _DEBUG
	if (g_disableServerG2.integer)
	{

	}
	else
#endif
	if (ent->ghoul2)
	{
		if (ent->localAnimIndex > 1)
		{ //if it isn't humanoid then we will be having none of this.
			pmove.ghoul2 = NULL;
		}
		else
		{
			pmove.ghoul2 = ent->ghoul2;
			pmove.g2Bolts_LFoot = trap->G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
			pmove.g2Bolts_RFoot = trap->G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
		}
	}

	//point the saber data to the right place
#if 0
	k = 0;
	while (k < MAX_SABERS)
	{
		if (ent->client->saber[k].model[0])
		{
			pm.saber[k] = &ent->client->saber[k];
		}
		else
		{
			pm.saber[k] = NULL;
		}
		k++;
	}
#endif

	//I'll just do this every frame in case the scale changes in realtime (don't need to update the g2 inst for that)
	VectorCopy(ent->modelScale, pmove.modelScale);
	//rww end bgghoul2

	pmove.gametype = level.gametype;
	pmove.debugMelee = g_debugMelee.integer;
	pmove.stepSlideFix = g_stepSlideFix.integer;

	pmove.noSpecMove = g_noSpecMove.integer;

	pmove.nonHumanoid = (ent->localAnimIndex > 0);

	VectorCopy( client->ps.origin, client->oldOrigin );

	/*
	if (level.intermissionQueued != 0 && g_singlePlayer.integer) {
		if ( level.time - level.intermissionQueued >= 1000  ) {
			pm.cmd.buttons = 0;
			pm.cmd.forwardmove = 0;
			pm.cmd.rightmove = 0;
			pm.cmd.upmove = 0;
			if ( level.time - level.intermissionQueued >= 2000 && level.time - level.intermissionQueued <= 2500 ) {
				trap->SendConsoleCommand( EXEC_APPEND, "centerview\n");
			}
			ent->client->ps.pm_type = PM_SPINTERMISSION;
		}
	}
	*/

	//Set up bg entity data
	pmove.baseEnt = (bgEntity_t *)g_entities;
	pmove.entSize = sizeof(gentity_t);

	if (ent->client->ps.saberLockTime > level.time)
	{
		gentity_t *blockOpp = &g_entities[ent->client->ps.saberLockEnemy];

		if (blockOpp && blockOpp->inuse && blockOpp->client)
		{
			vec3_t lockDir, lockAng;

			//VectorClear( ent->client->ps.velocity );
			VectorSubtract( blockOpp->r.currentOrigin, ent->r.currentOrigin, lockDir );
			//lockAng[YAW] = vectoyaw( defDir );
			vectoangles(lockDir, lockAng);
			SetClientViewAngle( ent, lockAng );
		}

		if ( ent->client->ps.saberLockHitCheckTime < level.time )
		{//have moved to next frame since last lock push
			ent->client->ps.saberLockHitCheckTime = level.time;//so we don't push more than once per server frame
			if ( ( ent->client->buttons & BUTTON_ATTACK ) && ! ( ent->client->oldbuttons & BUTTON_ATTACK ) )
			{
				if ( ent->client->ps.saberLockHitIncrementTime < level.time )
				{//have moved to next frame since last saberlock attack button press
					int lockHits = 0;
					ent->client->ps.saberLockHitIncrementTime = level.time;//so we don't register an attack key press more than once per server frame
					//NOTE: FP_SABER_OFFENSE level already taken into account in PM_SaberLocked
					if ( (ent->client->ps.fd.forcePowersActive&(1<<FP_RAGE)) )
					{//raging: push harder
						lockHits = 1+ent->client->ps.fd.forcePowerLevel[FP_RAGE];
					}
					else
					{//normal attack
						switch ( ent->client->ps.fd.saberAnimLevel )
						{
						case SS_FAST:
							lockHits = 1;
							break;
						case SS_MEDIUM:
						case SS_TAVION:
						case SS_DUAL:
						case SS_STAFF:
							lockHits = 2;
							break;
						case SS_STRONG:
						case SS_DESANN:
							lockHits = 3;
							break;
						}
					}
					if ( ent->client->ps.fd.forceRageRecoveryTime > level.time
						&& Q_irand( 0, 1 ) )
					{//finished raging: weak
						lockHits -= 1;
					}
					lockHits += ent->client->saber[0].lockBonus;
					if ( ent->client->saber[1].model[0]
						&& !ent->client->ps.saberHolstered )
					{
						lockHits += ent->client->saber[1].lockBonus;
					}
					ent->client->ps.saberLockHits += lockHits;
					if ( g_saberLockRandomNess.integer )
					{
						ent->client->ps.saberLockHits += Q_irand( 0, g_saberLockRandomNess.integer );
						if ( ent->client->ps.saberLockHits < 0 )
						{
							ent->client->ps.saberLockHits = 0;
						}
					}
				}
			}
			if ( ent->client->ps.saberLockHits > 0 )
			{
				if ( !ent->client->ps.saberLockAdvance )
				{
					ent->client->ps.saberLockHits--;
				}
				ent->client->ps.saberLockAdvance = qtrue;
			}
		}
	}
	else
	{
		ent->client->ps.saberLockFrame = 0;
		//check for taunt
		if ( (pmove.cmd.generic_cmd == GENCMD_ENGAGE_DUEL) && (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) )
		{//already in a duel, make it a taunt command
			pmove.cmd.buttons |= BUTTON_GESTURE;
		}
	}

	if (ent->s.number >= MAX_CLIENTS)
	{
		VectorCopy(ent->r.mins, pmove.mins);
		VectorCopy(ent->r.maxs, pmove.maxs);
#if 1
		if (ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle )
		{
			if ( ent->m_pVehicle->m_pPilot)
			{ //vehicles want to use their last pilot ucmd I guess
				if ((level.time - ent->m_pVehicle->m_ucmd.serverTime) > 2000)
				{ //Previous owner disconnected, maybe
					ent->m_pVehicle->m_ucmd.serverTime = level.time;
					ent->client->ps.commandTime = level.time-100;
					msec = 100;
				}

				memcpy(&pmove.cmd, &ent->m_pVehicle->m_ucmd, sizeof(usercmd_t));

				//no veh can strafe
				pmove.cmd.rightmove = 0;
				//no crouching or jumping!
				pmove.cmd.upmove = 0;

				//NOTE: button presses were getting lost!
				assert(g_entities[ent->m_pVehicle->m_pPilot->s.number].client);
				pmove.cmd.buttons = (g_entities[ent->m_pVehicle->m_pPilot->s.number].client->pers.cmd.buttons&(BUTTON_ATTACK|BUTTON_ALT_ATTACK));
			}
			if ( ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
			{
				if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE )
				{//ATST crushes anything underneath it
					gentity_t	*under = &g_entities[ent->client->ps.groundEntityNum];
					if ( under && under->health && under->takedamage )
					{
						vec3_t	down = {0,0,-1};
						//FIXME: we'll be doing traces down from each foot, so we'll have a real impact origin
						G_Damage( under, ent, ent, down, under->r.currentOrigin, 100, 0, MOD_CRUSH );
					}
				}
			}
		}
#endif
	}

	// GalaxyRP: [Grapple Hook] fire and release, from the usercmd bit a +button12 (or +grapple) bind
	// produces. Edge-triggered: one hook per press, hookHasBeenFired being the latch, with
	// g_hookFloodProtect ms between shots. Letting go of the button clears the latches and lowers
	// the pull flag; the third test then frees the hook, which is what "release" means. The hook
	// itself is not predicted -- the client sees it as an ordinary missile in the snapshot -- so
	// none of this needs to be shared code. Real players only, like the flag above.
	if ( ent->s.number < MAX_CLIENTS && ent->client )
	{
		if ( ( pmove.cmd.buttons & BUTTON_GRAPPLE ) &&
			!ent->client->hookHasBeenFired &&
			ent->client->hookFireTime < level.time - g_hookFloodProtect.integer &&
			RP_CanFireHook( ent ) )
		{
			Weapon_HookFire( ent );
			ent->client->hookHasBeenFired = qtrue;
			ent->client->hookFireTime = level.time;
		}

		if ( !( pmove.cmd.buttons & BUTTON_GRAPPLE ) &&
			ent->client->hookHasBeenFired &&
			ent->client->fireHeld )
		{
			ent->client->fireHeld = qfalse;
			ent->client->hookHasBeenFired = qfalse;
			ent->client->ps.pm_flags &= ~PMF_GRAPPLE;
		}

		if ( ent->client->hook && !ent->client->fireHeld )
		{
			Weapon_HookFree( ent->client->hook );
		}
	}

	if (ent->client->pers.no_attack_timer > level.time)
	{ // zyk: player hit by No Attack ability
		pmove.cmd.buttons &= ~(BUTTON_ATTACK);
		pmove.cmd.buttons &= ~(BUTTON_ALT_ATTACK);
	}

	// GalaxyRP: [Phase] everyone else walks through phased bodies: hidden for exactly this call and
	// put back straight after, before anything else in this function traces against them.
	RP_PhaseHideBodies( ent );
	Pmove (&pmove);
	RP_PhaseRestoreBodies();

	// GalaxyRP fix: [ICARUS] Pmove just counted the animation timers down; a script waiting on
	// one of them is told now -- see RP_IcarusAnimTaskCheck()
	RP_IcarusAnimTaskCheck( ent );

	if (ent->client->solidHack)
	{
		if (ent->client->solidHack > level.time)
		{ //whee!
			ent->r.contents = 0;
		}
		else
		{
			ent->r.contents = CONTENTS_BODY;
			ent->client->solidHack = 0;
		}
	}

	if ( ent->NPC )
	{
		VectorCopy( ent->client->ps.viewangles, ent->r.currentAngles );
	}

	if (pmove.checkDuelLoss)
	{
		if (pmove.checkDuelLoss > 0 && (pmove.checkDuelLoss <= MAX_CLIENTS || (pmove.checkDuelLoss < (MAX_GENTITIES-1) && g_entities[pmove.checkDuelLoss-1].s.eType == ET_NPC) ) )
		{
			gentity_t *clientLost = &g_entities[pmove.checkDuelLoss-1];

			if (clientLost && clientLost->inuse && clientLost->client && Q_irand(0, 40) > clientLost->health)
			{
				vec3_t attDir;
				VectorSubtract(ent->client->ps.origin, clientLost->client->ps.origin, attDir);
				VectorNormalize(attDir);

				VectorClear(clientLost->client->ps.velocity);
				clientLost->client->ps.forceHandExtend = HANDEXTEND_NONE;
				clientLost->client->ps.forceHandExtendTime = 0;

				gGAvoidDismember = 1;
				G_Damage(clientLost, ent, ent, attDir, clientLost->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_SABER);

				if (clientLost->health < 1)
				{
					gGAvoidDismember = 2;
					G_CheckForDismemberment(clientLost, ent, clientLost->client->ps.origin, 999, (clientLost->client->ps.legsAnim), qfalse);
				}

				gGAvoidDismember = 0;
			}
			else if (clientLost && clientLost->inuse && clientLost->client &&
				clientLost->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN && clientLost->client->ps.saberEntityNum)
			{ //if we didn't knock down it was a circle lock. So as punishment, make them lose their saber and go into a proper anim
				saberCheckKnockdown_DuelLoss(&g_entities[clientLost->client->ps.saberEntityNum], clientLost, ent);
			}
		}

		pmove.checkDuelLoss = 0;
	}

	//GalaxyRP (Alex): [Combat] This is purely for switching saber styles. The delay between switching should be 190 ms.
	// GalaxyRP fix: [gameplay] this used to gate on `ent->client->lastGenCmdTime - 110 < level.time`,
	// reusing the shared lastGenCmdTime field that the generic_cmd debounce block below (~line 4212)
	// resets to `level.time + 300` every time IT fires -- which, for a held-down SABERATTACKCYCLE
	// input, only happens roughly every 300ms. In between those resets lastGenCmdTime stays fixed,
	// so the old condition went true 190ms into that 300ms window and then stayed true for every
	// single frame afterward (not just once), causing Cmd_SaberAttackCycle_f() to fire on every
	// server frame for the remaining ~110ms instead of once every ~190ms as the comment above
	// promises. Track this debounce with its own dedicated timestamp instead of borrowing one that
	// belongs to unrelated commands, so a fire here always waits a genuine 190ms before firing again.
	if (pmove.cmd.generic_cmd &&
		(pmove.cmd.generic_cmd != ent->client->lastGenCmd || ent->client->lastSaberAttackCycleTime < level.time))
	{
		switch (pmove.cmd.generic_cmd)
		{
			case GENCMD_SABERATTACKCYCLE:
				Cmd_SaberAttackCycle_f(ent);
				ent->client->lastSaberAttackCycleTime = level.time + 190;
				break;
		}
	}

	if (pmove.cmd.generic_cmd &&
		(pmove.cmd.generic_cmd != ent->client->lastGenCmd || ent->client->lastGenCmdTime < level.time))
	{
		ent->client->lastGenCmd = pmove.cmd.generic_cmd;
		if (pmove.cmd.generic_cmd != GENCMD_FORCE_THROW &&
			pmove.cmd.generic_cmd != GENCMD_FORCE_PULL)
		{ //these are the only two where you wouldn't care about a delay between
			if (ent->client->sess.amrpgmode == 2)
			{
				// GalaxyRP fix: [Cloak Item] the vehicle-cloak trigger that used to live here (toggling
				// vehicle+rider cloak on GENCMD_SABERATTACKCYCLE, gated on the Holdable Items Upgrade and
				// requiring a saber equipped) has been removed completely -- it kept colliding with
				// unrelated saber/weapon-control behavior on that same input (see the removed guard's own
				// history: alt-fire cross-trigger concerns, then a real vehicle weapon-control regression
				// when the saber-equipped guard was lifted). Vehicle-cloak is now its own dedicated
				// "/vehicle_cloak" console command (Cmd_VehicleCloak_f in g_cmds.c) instead, which has no
				// overlap with saber styles, weapon selection, or vehicle weapon-link toggling.
				// DAJ_RP: [Cloak Item] that command is gated on Cloak Item level 2 now, not the upgrade.

				// GalaxyRP fix: [Skills] removed the GENCMD_ENGAGE_DUEL Unique Skill (skill index 38) heal
				// branch that used to be here -- skill 38 is being removed the same way as 39-42 (see the
				// skills[] comment in g_cmds.c), and this was its only trigger. Engaging a duel via this
				// key still runs normally through the switch statement below (Cmd_EngageDuel_f); only the
				// bonus heal that used to fire alongside it for players with skill 38 leveled is gone, so
				// pressing this key can no longer reach that ability at all, regardless of skill level.
				if (pmove.cmd.generic_cmd == GENCMD_SABERATTACKCYCLE)
				{
					// GalaxyRP fix: [Dead Code] removed the Bounty Hunter seeker-drone-recovery
					// (rpg_class==2) and Magic Master fist-selection-cycling (rpg_class==8) branches
					// here; pers.rpg_class is always 0 server-side, so both guards were tautologically
					// false. Branch intentionally left empty (harmless no-op) rather than removing the
					// enclosing else-if, to avoid disturbing the surrounding if/else-if chain.
				}
			}

			ent->client->lastGenCmdTime = level.time + 300; //default 100ms debounce between issuing the same command.
		}

		switch(pmove.cmd.generic_cmd)
		{
		case 0:
			break;
		case GENCMD_SABERSWITCH:
			Cmd_ToggleSaber_f(ent);
			break;
		case GENCMD_ENGAGE_DUEL:
			if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
			{//already in a duel, made it a taunt command
			}
			else
			{
				Cmd_EngageDuel_f(ent, 0);
			}
			break;
		case GENCMD_FORCE_HEAL:
			ForceHeal(ent);
			break;
		case GENCMD_FORCE_SPEED:
			ForceSpeed(ent, 0);
			break;
		case GENCMD_FORCE_THROW:
			ForceThrow(ent, qfalse);
			break;
		case GENCMD_FORCE_PULL:
			ForceThrow(ent, qtrue);
			break;
		case GENCMD_FORCE_DISTRACT:
			ForceTelepathy(ent);
			break;
		case GENCMD_FORCE_RAGE:
			ForceRage(ent);
			break;
		case GENCMD_FORCE_PROTECT:
			ForceProtect(ent);
			break;
		case GENCMD_FORCE_ABSORB:
			ForceAbsorb(ent);
			break;
		case GENCMD_FORCE_HEALOTHER:
			ForceTeamHeal(ent);
			break;
		case GENCMD_FORCE_FORCEPOWEROTHER:
			ForceTeamForceReplenish(ent);
			break;
		case GENCMD_FORCE_SEEING:
			ForceSeeing(ent);
			break;
		case GENCMD_USE_SEEKER:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
				G_ItemUsable(&ent->client->ps, HI_SEEKER) )
			{
				ItemUse_Seeker(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_SEEKER, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER);
			}
			break;
		case GENCMD_USE_FIELD:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) &&
				G_ItemUsable(&ent->client->ps, HI_SHIELD) )
			{
				ItemUse_Shield(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_SHIELD, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SHIELD);
			}
			break;
		case GENCMD_USE_BACTA:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC) )
			{
				ItemUse_MedPack(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_MEDPAC, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
			}
			break;
		case GENCMD_USE_BACTABIG:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC_BIG) )
			{
				ItemUse_MedPack_Big(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_MEDPAC_BIG, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
			}
			break;
		case GENCMD_USE_ELECTROBINOCULARS:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS) )
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_ZOOM:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS) )
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_USE_SENTRY:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) &&
				G_ItemUsable(&ent->client->ps, HI_SENTRY_GUN) )
			{
				ItemUse_Sentry(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_SENTRY_GUN, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SENTRY_GUN);
			}
			break;
		case GENCMD_USE_JETPACK:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) &&
				G_ItemUsable(&ent->client->ps, HI_JETPACK) )
			{
				ItemUse_Jetpack(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_JETPACK, 0);
				/*
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 2);
				}
				*/
			}
			break;
		case GENCMD_USE_HEALTHDISP:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_HEALTHDISP)) &&
				G_ItemUsable(&ent->client->ps, HI_HEALTHDISP) )
			{
				//ItemUse_UseDisp(ent, HI_HEALTHDISP);
				G_AddEvent(ent, EV_USE_ITEM0+HI_HEALTHDISP, 0);
			}
			break;
		case GENCMD_USE_AMMODISP:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP)) &&
				G_ItemUsable(&ent->client->ps, HI_AMMODISP) )
			{
				//ItemUse_UseDisp(ent, HI_AMMODISP);
				G_AddEvent(ent, EV_USE_ITEM0+HI_AMMODISP, 0);
			}
			break;
		case GENCMD_USE_EWEB:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) &&
				G_ItemUsable(&ent->client->ps, HI_EWEB) )
			{
				ItemUse_UseEWeb(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_EWEB, 0);
			}
			break;
		case GENCMD_USE_CLOAK:
			// GalaxyRP fix: [Cloak Item] "use_cloak" is now solo-only, on foot OR mounted -- pairing with
			// a vehicle is exclusively the "/vehicle_cloak" console command's job now (Cmd_VehicleCloak_f
			// in g_cmds.c), which keeps the two commands' responsibilities clean: this one only ever
			// touches `ent`, never the vehicle. G_ItemUsable() is deliberately not used here at all (it unconditionally
			// returns 0 while mounted, which would block this on-vehicle case, and its only other
			// real-world effect for HI_CLOAK is the same "is player alive" check done explicitly below)
			// -- just possession + alive + off-cooldown. Toggle direction reads `ent`'s own PW_CLOAKED
			// (not the vehicle's) -- if this player is currently part of a paired cloak, decloaking goes
			// through Jedi_DecloakPair so the vehicle comes down too; this is what fixes the old "wrong
			// toggle direction after the vehicle auto-decloaked from its own weapon fire" bug from the
			// previous single-command design.
			// GalaxyRP fix: [Cloak Item] the downed test below. Being downed is supposed to break cloak
			// (paralyze_player does exactly that), but it also leaves the player on RP_DOWNED_HEALTH health -- so they
			// passed every aliveness gate here and could simply press use_cloak again a second later and
			// lie there invisible for the rest of rp_downed_timer. It gates only the CLOAK direction:
			// the decloak branch stays reachable, so a player downed while cloaked can always turn it
			// off, and losing the item still forces it off elsewhere.
			if ( ent->client->cloakToggleTime < level.time &&
				ent->client->ps.stats[STAT_HEALTH] > 0 && !(ent->client->ps.eFlags & EF_DEAD) &&
				ent->client->ps.pm_type != PM_DEAD &&
				(ent->client->ps.powerups[PW_CLOAKED] ||
					!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))) &&
				(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) )
			{
				if ( ent->client->ps.powerups[PW_CLOAKED] )
				{//decloak (self, plus the vehicle too if paired)
					Jedi_DecloakPair( ent );
				}
				// GalaxyRP fix: [Dueling] holdables are refused inside a private duel again, and this
				// command is the one that has to ask for itself: it deliberately does not go through
				// G_ItemUsable() (see the note above), so the guard added there does not cover it.
				//
				// Gated on the CLOAKING direction only, exactly like the downed test in this same
				// condition and like ItemUse_Jetpack's Jetpack_Off/Jetpack_On split. A guard on the
				// whole toggle would leave a player who entered the duel already cloaked stuck
				// invisible until it ended -- nothing else clears PW_CLOAKED, and with the duel over
				// they would have been invisible for the whole fight. Cmd_EngageDuel_f() decloaks both
				// players when the duel starts, so this is the backstop rather than the fix.
				else if ( !BG_HoldablesBlocked( &ent->client->ps ) )
				{//cloak (self only, regardless of mount state)
					Jedi_Cloak( ent );
				}
				ent->client->cloakToggleTime = level.time + 1000; // matches g_items.c's CLOAK_TOGGLE_TIME
			}
			break;
		// GalaxyRP fix: [Cloak Item] the GENCMD_USE_CLOAK_VEHICLE case that used to live here was
		// removed -- it required a client engine with a matching generic-command value and a bound
		// key to ever produce it, which the real, actually-distributed TaystJK engine has no support
		// for at all (its own GENCMD_* enum stops at GENCMD_GLOAT), making this permanently
		// unreachable in practice. Replaced with a genuine "/vehicle_cloak" console command
		// (Cmd_VehicleCloak_f in g_cmds.c) instead, which needs no client-engine cooperation -- see
		// that function for the identical vehicle+rider pairing logic that used to be here.
		case GENCMD_TAUNT:
			G_SetTauntAnim( ent, TAUNT_TAUNT );
			break;
		case GENCMD_BOW:
			G_SetTauntAnim( ent, TAUNT_BOW );
			break;
		case GENCMD_MEDITATE:
			G_SetTauntAnim( ent, TAUNT_MEDITATE );
			break;
		case GENCMD_FLOURISH:
			G_SetTauntAnim( ent, TAUNT_FLOURISH );
			break;
		case GENCMD_GLOAT:
			G_SetTauntAnim( ent, TAUNT_GLOAT );
			break;
		default:
			break;
		}
	}

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
	}
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	// DAJ_RP: [Phase] /admhidden -- the copy above rebuilt s from ps
	RP_HiddenApplyState( ent, &ent->s );

	SendPendingPredictableEvents( &ent->client->ps );

	// GalaxyRP fix: [Grapple Hook] the Quake 3 line that used to be here --
	//
	//     if ( !( ent->client->ps.eFlags & EF_FIRING ) ) client->fireHeld = qfalse;  // for grapple
	//
	// -- is gone. Q3's offhand hook fired with the ATTACK button, so "not firing" meant "let go". Ours
	// fires with BUTTON_GRAPPLE, and fireHeld is the latch the release block above reads; with this
	// line still here every ClientThink_real that saw attack up cleared the latch, the next usercmd
	// freed the hook, and g_hookFloodProtect later the still-held button fired a fresh one: a hook
	// that lived for one frame and a fire sound every 600ms. TaystJK removed the line too. fireHeld
	// is now written only by Weapon_HookFire(), Weapon_HookFree() and G_FreeEntity()'s detach net.

	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	if (ent->s.eType != ET_NPC ||
		ent->s.NPC_class != CLASS_VEHICLE ||
		!ent->m_pVehicle ||
		!ent->m_pVehicle->m_iRemovedSurfaces)
	{ //let vehicles that are getting broken apart do their own crazy sizing stuff
		VectorCopy (pmove.mins, ent->r.mins);
		VectorCopy (pmove.maxs, ent->r.maxs);
	}

	ent->waterlevel = pmove.waterlevel;
	ent->watertype = pmove.watertype;

	// execute client events
	ClientEvents( ent, oldEventSequence );

	if ( pmove.useEvent )
	{
		//TODO: Use
//		TryUse( ent );
	}
	if ((ent->client->pers.cmd.buttons & BUTTON_USE) && ent->client->ps.useDelay < level.time)
	{
		TryUse(ent);
		ent->client->ps.useDelay = level.time + 100;
	}

	// link entity now, after any personal teleporters have been used
	trap->LinkEntity ((sharedEntity_t *)ent);
	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	//test for solid areas in the AAS file
//	BotTestAAS(ent->r.currentOrigin);

	// touch other objects
	ClientImpacts( ent, &pmove );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

//	G_VehicleAttachDroidUnit( ent );

	// Did we kick someone in our pmove sequence?
	// GalaxyRP: [Jump Kick] what the kick does is RP_JumpKickLand() above.
	if (client->ps.forceKickFlip)
	{
		RP_JumpKickLand( ent );

		client->ps.forceKickFlip = 0;
	}

	// check for respawning
	if ( client->ps.stats[STAT_HEALTH] <= 0
		&& !(client->ps.eFlags2&EF2_HELD_BY_MONSTER)//can't respawn while being eaten
		&& ent->s.eType != ET_NPC && level.load_entities_timer == 0 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))) { // zyk: cannot respawn while entities are being loaded
		// wait for the attack button to be pressed
		if ( level.time > client->respawnTime && !gDoSlowMoDuel ) {
			// forcerespawn is to prevent users from waiting out powerups
			int forceRes = g_forceRespawn.integer;

			if (level.gametype == GT_POWERDUEL)
			{
				forceRes = 1;
			}
			else if (level.gametype == GT_SIEGE && g_siegeRespawn.integer)
			{ //wave respawning on
				forceRes = 1;
			}

			if ( forceRes > 0 &&
				( level.time - client->respawnTime ) > forceRes * 1000 ) {
				ClientRespawn( ent );
				return;
			}

			// pressing attack or use is the normal respawn method
			if ( ucmd->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) ) {
				ClientRespawn( ent );
			}
		}
		else if (gDoSlowMoDuel)
		{
			client->respawnTime = level.time + 1000;
		}
		return;
	}

	// perform once-a-second actions
	ClientTimerActions( ent, msec );

	G_UpdateClientBroadcasts ( ent );

	//try some idle anims on ent if getting no input and not moving for some time
	G_CheckClientIdle( ent, ucmd );

	// This code was moved here from clientThink to fix a problem with g_synchronousClients
	// being set to 1 when in vehicles.
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		//run it
		if (g_entities[ent->client->ps.m_iVehicleNum].inuse && g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
		}
		else
		{ //vehicle no longer valid?
			ent->client->ps.m_iVehicleNum = 0;
		}
	}

}

/*
==================
G_CheckClientTimeouts

Checks whether a client has exceded any timeouts and act accordingly
==================
*/
void G_CheckClientTimeouts ( gentity_t *ent )
{
	// Only timeout supported right now is the timeout to spectator mode
	if ( !g_timeouttospec.integer )
	{
		return;
	}

	// Already a spectator, no need to boot them to spectator
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		return;
	}

	// See how long its been since a command was received by the client and if its
	// longer than the timeout to spectator then force this client into spectator mode
	if ( level.time - ent->client->pers.cmd.serverTime > g_timeouttospec.integer * 1000 )
	{
		SetTeam ( ent, "spectator" );
	}
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum, usercmd_t *ucmd ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	if (clientNum < MAX_CLIENTS)
	{
		trap->GetUsercmd( clientNum, &ent->client->pers.cmd );
	}

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;

	if (!ent->NPC)
	{ // zyk: applying mind control action if this player is controlling another player or a npc
		if (ent->client->pers.mind_controlled1_id != -1)
		{
			g_entities[ent->client->pers.mind_controlled1_id].client->pers.cmd = ent->client->pers.cmd;
			ClientThink_real(&g_entities[ent->client->pers.mind_controlled1_id]);
		}
	}

	if (ucmd)
	{
		ent->client->pers.cmd = *ucmd;
	}

/* 	This was moved to clientthink_real, but since its sort of a risky change i left it here for
    now as a more concrete reference - BSD

	if ( clientNum < MAX_CLIENTS
		&& ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		if (g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

			if (veh->m_pVehicle &&
				veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent)
			{ //only take input from the pilot...
				veh->client->ps.commandTime = ent->client->ps.commandTime;
				memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
				if ( veh->m_pVehicle->m_ucmd.buttons & BUTTON_TALK )
				{ //forced input if "chat bubble" is up
					veh->m_pVehicle->m_ucmd.buttons = BUTTON_TALK;
					veh->m_pVehicle->m_ucmd.forwardmove = 0;
					veh->m_pVehicle->m_ucmd.rightmove = 0;
					veh->m_pVehicle->m_ucmd.upmove = 0;
				}
			}
		}
	}
*/
	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		// zyk: player or npc who is not being mind controlled can think
		if (ent->client->pers.being_mind_controlled == -1)
			ClientThink_real( ent );
	}
	// vehicles are clients and when running synchronous they still need to think here
	// so special case them.
	else if ( g_entities[clientNum].s.NPC_class == CLASS_VEHICLE ) { // zyk: changed vehicle condition from clientNum >= MAX_CLIENTS
		ClientThink_real( ent );
	}

/*	This was moved to clientthink_real, but since its sort of a risky change i left it here for
    now as a more concrete reference - BSD

	if ( clientNum < MAX_CLIENTS
		&& ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		//run it
		if (g_entities[ent->client->ps.m_iVehicleNum].inuse &&
			g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
		}
		else
		{ //vehicle no longer valid?
			ent->client->ps.m_iVehicleNum = 0;
		}
	}
*/
}


void G_RunClient( gentity_t *ent ) {
	// force client updates if they're not sending packets at roughly 4hz
	if ( !(ent->r.svFlags & SVF_BOT) && g_forceClientUpdateRate.integer && ent->client->lastCmdTime < level.time - g_forceClientUpdateRate.integer ) {
		trap->GetUsercmd( ent-g_entities, &ent->client->pers.cmd );

		ent->client->lastCmdTime = level.time;

		// fill with seemingly valid data
		ent->client->pers.cmd.serverTime = level.time;
		ent->client->pers.cmd.buttons = 0;
		ent->client->pers.cmd.forwardmove = ent->client->pers.cmd.rightmove = ent->client->pers.cmd.upmove = 0;

		ClientThink_real( ent );
		return;
	}

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent ) {
	gclient_t	*cl;

	if (ent->s.eType == ET_NPC)
	{
		assert(0);
		return;
	}

	// if we are doing a chase cam or a remote view, grab the latest info
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		int		clientNum;//, flags;

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
				//flags = (cl->mGameFlags & ~(PSG_VOTED | PSG_TEAMVOTED)) | (ent->client->mGameFlags & (PSG_VOTED | PSG_TEAMVOTED));
				//ent->client->mGameFlags = flags;
				ent->client->ps.eFlags = cl->ps.eFlags;
				ent->client->ps = cl->ps;
				ent->client->ps.pm_flags |= PMF_FOLLOW;
				return;
			} else {
				// drop them to free spectators unless they are dedicated camera followers
				if ( ent->client->sess.spectatorClient >= 0 ) {
					ent->client->sess.spectatorState = SPECTATOR_FREE;
					ClientBegin( ent->client - level.clients, qtrue );
				}
			}
		}
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}
}

/*
==============
G_PublishMaxArmor

GalaxyRP fix: [Shield] publish the shield ceiling so BG_CanItemBeGrabbed() -- which cgame runs
too -- refuses a shield the server is going to refuse. See STAT_MAX_ARMOR in bg_public.h for the
bug this closes; in short, an RPG player's cap is pers.max_rpg_shield, which is server-only and
is zero without the Max Shield skill, so the shared predicate was comparing against the wrong
number and every shield was consumed for nothing and respawned a second later.

NPCs: they have clients and do pick shields up (Touch_Item only needs other->client, and their
amrpgmode is 0). ClientEndFrame() only ever runs for player slots, though -- G_RunFrame's end-of-
frame loop stops at level.maxclients -- so an NPC's ceiling stayed at zero and it could never pick
a shield up again. GalaxyRP fix: [Shield] this was a block inside ClientEndFrame(); it is a function
now so G_RunFrame can run it for NPCs too, before they move and touch items.

In ClientEndFrame it is called above the spectator early-return, for the same reason STAT_USE_HINT
is: SpectatorClientEndFrame() copies the followed player's whole playerState over a follower's, this
stat included.

Written unconditionally every frame rather than only when it changes, because zero is a
LEGITIMATE ceiling here -- it is exactly the case being fixed -- so it cannot double as a "not
published yet" sentinel. Writing it always also means it tracks /skillup, login, logout and
respawn with no extra bookkeeping, and the playerState delta only spends bits when it moves.
A ceiling changed mid-frame is seen one frame late, which costs at most a single frame of a
just-upgraded player being refused a shield.

Non-RPG players and NPCs get STAT_MAX_HEALTH, which is precisely what the predicate compared
against before this change -- their behaviour is unchanged to the bit.

The three arms mirror Pickup_Armor()'s (g_items.c) one for one, including the last. sess.amrpgmode
only ever holds 0 or 2, so that arm is unreachable today -- but Pickup_Armor's else gives nothing
for any other value, and a ceiling of 0 is what makes this predicate agree with it there too.
Matching its shape rather than its reachable cases is the cheap way to keep "grabbable" and
"actually gives something" exact complements whatever amrpgmode turns out to hold.
==============
*/
void G_PublishMaxArmor( gentity_t *ent )
{
	if ( ent->client )
	{
		if ( ent->client->sess.amrpgmode == 2 )
		{
			ent->client->ps.stats[STAT_MAX_ARMOR] = ent->client->pers.max_rpg_shield;
		}
		else if ( ent->client->sess.amrpgmode < 2 )
		{
			ent->client->ps.stats[STAT_MAX_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		else
		{
			ent->client->ps.stats[STAT_MAX_ARMOR] = 0;
		}
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent ) {
	int			i;
	qboolean isNPC = qfalse;

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// GalaxyRP: [Use hint] evaluate the usable-entity hand icon once per client per frame, here at the
	// end of the frame so origin and view angles are already final. Deliberately placed ABOVE the
	// spectator early-return below, so a player who was looking at a door and then died or went to
	// free-fly spectate has the stat cleared rather than keeping its last value forever with the client
	// still drawing it. Note this is not literally "every path": SpectatorClientEndFrame runs straight
	// after and copies the followed player's whole playerState over a follower's, this stat included,
	// so a follower sees whatever the player they are watching sees. That is consistent with a follower
	// inheriting the rest of that player's HUD, so it is left alone. G_CanUseInFrontOf() (g_utils.c) itself also refuses for
	// dead and spectating players, so this is belt and braces, not duplication.
	// The gate is the whole cost story: the trace runs only for a logged-in player who turned the
	// feature on via /settings 4 (bit 6 is inverted -- clear == ON), so a server where nobody enabled
	// it pays nothing but this comparison. Gating here rather than client-side also means the setting
	// itself never has to be networked: the client draws whenever the bit arrives.
	if ( ent->s.number < MAX_CLIENTS && ent->client )
	{
		if ( ent->client->sess.loggedin == qtrue && !(ent->client->pers.player_settings & (1 << 6))
			&& G_CanUseInFrontOf( ent ) )
		{
			ent->client->ps.stats[STAT_USE_HINT] = 1;
		}
		else
		{
			ent->client->ps.stats[STAT_USE_HINT] = 0;
		}

		// GalaxyRP fix: [Force] publish the true Force Jump level for the client's movement
		// prediction. The copy the engine networks inside playerState is a 2-bit field and silently
		// truncates levels 4 and 5 to 0 and 1; see STAT_FORCE_JUMP_LEVEL in bg_public.h. Written
		// unconditionally and every frame so it tracks /skillup, respawns and logout with no extra
		// bookkeeping -- the playerState delta only spends bits on the frames where it changes.
		ent->client->ps.stats[STAT_FORCE_JUMP_LEVEL] = ent->client->ps.fd.forcePowerLevel[FP_LEVITATION];

		// DAJ_RP: [Minigames] publish "fighting in a mini-game" for the shared pickup rule and cgame's pickup
		// prediction (RP_PS_MINIGAME_FIGHTER, bg_public.h). Every frame, so it follows the battle and the fight as
		// they begin and end; a one-bit field, so only 0 or 1 may be written.
		ent->client->ps.userInt1 = RP_MinigameFighter( ent ) ? 1 : 0;

		// DAJ_RP: [Entity Bounds] /settings 6 -- the aimed entity's box and the nearby point-entity
		// markers, drawn for this client only. Here, with the use hint, so the view is final for the
		// frame and a free-flying spectator (the early return below) still gets it. The function does
		// its own gating and costs one comparison chain for anyone who has not turned it on.
		RP_EntBoundsFrame( ent );

		// GalaxyRP: [Entity System] an entity held with /entcopy or /entcut: the preview follows the aim,
		// and the hold is let go of if the admin may no longer hold it (g_entgrab.c)
		RP_EntGrabFrame( ent );
	}

	// GalaxyRP fix: [Shield] publish the shield ceiling so BG_CanItemBeGrabbed() -- which cgame runs
	// too -- refuses a shield the server is going to refuse. See G_PublishMaxArmor() above.
	G_PublishMaxArmor( ent );

	// GalaxyRP fix: [Weapons] publish whether this player may alt-fire what they are holding, so the
	// button strip in PM_Weapon() -- which cgame runs too -- refuses the same shot the server refuses.
	// canAltFireWeapon() is the policy and stays server-only; this is its one caller now. See
	// STAT_ALT_FIRE_OK in bg_public.h for the mispredict this closes.
	//
	// Placed beside the STAT_MAX_ARMOR block above and for the same reasons: every client every frame,
	// above the spectator early-return so a follower's copied playerState carries it, and written
	// unconditionally because 0 is a real answer, not a "not yet published" sentinel. Its own block
	// rather than a line inside the shield one, so each publish can be read -- and lifted -- alone. A
	// change mid-frame -- login, logout, a skill bought -- is seen one frame late; a weapon switch is
	// covered by the change and raise times, see the stat's comment. NPCs, if they reach here, get
	// canAltFireWeapon()'s unconditional qtrue, and PM_Weapon() does not consult the stat for them in
	// any case.
	if ( ent->client )
	{
		ent->client->ps.stats[STAT_ALT_FIRE_OK] = canAltFireWeapon( ent ) ? 1 : 0;
	}

	// DAJ_RP: [Phase] tell a hidden player he is hidden -- his own client draws the Force Push body blur on
	// him in third person (CG_Player(), cg_players.c). Through stats[], so no other client is told; see
	// STAT_RP_SELF_FX in bg_public.h. Every frame, above the spectator split like the two above: it drops at
	// once on /admhidden off, and a spectator (whom RP_PlayerIsHidden() never counts) gets 0. A spectator
	// following a hidden player is copied his playerState, bit and all; his client does not draw for him.
	if ( ent->client )
	{
		if ( RP_PlayerIsHidden( ent ) )
		{
			ent->client->ps.stats[STAT_RP_SELF_FX] |= RP_SELF_FX_HIDDEN;
		}
		else
		{
			ent->client->ps.stats[STAT_RP_SELF_FX] &= ~RP_SELF_FX_HIDDEN;
		}
	}

	// GalaxyRP fix: [Death System] serve a downed player's countdown, here for the same structural
	// reason the block above is here: this runs for every client every server frame, and it runs
	// ABOVE the spectator split below. ClientTimerActions() reaches neither a spectator nor a client
	// in intermission, so a countdown left to it stopped the moment its owner left the world. See
	// RP_RunDownedTimer() and RP_DownedTimerTick() above.
	RP_RunDownedTimer( ent );

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		SpectatorClientEndFrame( ent );
		return;
	}

	// turn off any expired powerups
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ent->client->ps.powerups[ i ] < level.time ) {
			// GalaxyRP fix: [Cloak Item] PW_CLOAKED is half of a pair -- Jedi_Cloak() sets FL_NOTARGET
			// alongside it and Jedi_Decloak() clears both, and NPC_ValidEnemy() (NPC_utils.c) relies on
			// the two never being out of step. Zeroing the powerup here on its own would strip the
			// cloak while leaving the player permanently untargetable by every NPC, turret and seeker
			// until their next respawn. In practice this loop cannot currently reach cloak -- it is
			// stored as a flat 2147483647, which only falls below level.time after ~24.9 days on a
			// single map -- but it is the generic expiry path, so it is the one that would bite first
			// if cloak were ever given a real duration. Routed through the same helper every other
			// decloak uses rather than clearing the flag inline, because FL_NOTARGET has other owners
			// (/notarget, the downed state, ICARUS SET_NOTARGET) and this keeps one rule, not two.
			if ( i == PW_CLOAKED && ent->client->ps.powerups[ i ] ) {
				Jedi_Decloak( ent );
			}
			ent->client->ps.powerups[ i ] = 0;
		}
	}

	// save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && (ent->client->ps.pm_type == PM_NORMAL || ent->client->ps.pm_type == PM_JETPACK || ent->client->ps.pm_type == PM_FLOAT) ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermissiontime ) {
		if ( ent->s.number < MAX_CLIENTS
			|| ent->client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			return;
		}
	}

	// burn from lava, etc
	P_WorldEffects (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	// add the EF_CONNECTION flag if we haven't gotten commands recently
	if ( level.time - ent->client->lastCmdTime > 1000 )
		ent->client->ps.eFlags |= EF_CONNECTION;
	else
		ent->client->ps.eFlags &= ~EF_CONNECTION;

	ent->client->ps.stats[STAT_HEALTH] = ent->health;	// FIXME: get rid of ent->health...

	G_SetClientSound (ent);

	// GalaxyRP: [Phase] finish a release that has come clear, and write the mode clients see into
	// ps.eFlags, where the state copy below carries it into s.eFlags.
	RP_PhaseUpdate( ent );

	// set the latest infor
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	// DAJ_RP: [Phase] /admhidden -- the copy above rebuilt s from ps
	RP_HiddenApplyState( ent, &ent->s );

	// GalaxyRP: [Phase] no collision box in the snapshot for a live phased body. SV_LinkEntity packs
	// s.solid from r.contents, which stays CONTENTS_BODY so that shots and powers still hit; clients
	// use s.solid only to build the list they predict movement against, so zeroing it here makes
	// every client -- a stock one included -- walk through this player the way the server now lets
	// them. Nothing on the server reads a player's s.solid, and the next link rebuilds it, so it is
	// redone every frame: ClientEndFrame runs after every think of the frame, and nothing relinks a
	// client between here and the snapshot.
	if ( RP_PhasePassesThrough( ent ) && ent->r.contents == CONTENTS_BODY )
	{
		ent->s.solid = 0;
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	SendPendingPredictableEvents( &ent->client->ps );

	// set the bit for the reachability area the client is currently in
//	i = trap->AAS_PointReachabilityAreaIndex( ent->client->ps.origin );
//	ent->client->areabits[i >> 3] |= 1 << (i & 7);
}
