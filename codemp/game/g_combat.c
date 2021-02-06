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

// g_combat.c

#include "b_local.h"
#include "bg_saga.h"

extern int G_ShipSurfaceForSurfName( const char *surfaceName );
extern qboolean G_FlyVehicleDestroySurface( gentity_t *veh, int surface );
extern void G_VehicleSetDamageLocFlags( gentity_t *veh, int impactDir, int deathPoint );
extern void G_VehUpdateShields( gentity_t *targ );
extern void G_LetGoOfWall( gentity_t *ent );
extern void BG_ClearRocketLock( playerState_t *ps );
//rww - pd
void BotDamageNotification(gclient_t *bot, gentity_t *attacker);
//end rww

void ThrowSaberToAttacker(gentity_t *self, gentity_t *attacker);

void ObjectDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	if(self->target)
	{
		G_UseTargets(self, attacker);
	}

	//remove my script_targetname
	G_FreeEntity( self );
}

qboolean G_HeavyMelee( gentity_t *attacker )
{
	if (level.gametype == GT_SIEGE
		&& attacker
		&& attacker->client
		&& attacker->client->siegeClass != -1
		&& (bgSiegeClasses[attacker->client->siegeClass].classflags & (1<<CFL_HEAVYMELEE)) )
	{
		return qtrue;
	}
	return qfalse;
}

int G_GetHitLocation(gentity_t *target, vec3_t ppoint)
{
	vec3_t			point, point_dir;
	vec3_t			forward, right, up;
	vec3_t			tangles, tcenter;
//	float			tradius;
	float			udot, fdot, rdot;
	int				Vertical, Forward, Lateral;
	int				HitLoc;

	// Get target forward, right and up.
	if(target->client)
	{
		// Ignore player's pitch and roll.
		VectorSet(tangles, 0, target->r.currentAngles[YAW], 0);
	}

	AngleVectors(tangles, forward, right, up);

	// Get center of target.
	VectorAdd(target->r.absmin, target->r.absmax, tcenter);
	VectorScale(tcenter, 0.5, tcenter);

	// Get radius width of target.
//	tradius = (fabs(target->r.maxs[0]) + fabs(target->r.maxs[1]) + fabs(target->r.mins[0]) + fabs(target->r.mins[1]))/4;

	// Get impact point.
	if(ppoint && !VectorCompare(ppoint, vec3_origin))
	{
		VectorCopy(ppoint, point);
	}
	else
	{
		return HL_NONE;
	}

/*
//get impact dir
	if(pdir && !VectorCompare(pdir, vec3_origin))
	{
		VectorCopy(pdir, dir);
	}
	else
	{
		return;
	}

//put point at controlled distance from center
	VectorSubtract(point, tcenter, tempvec);
	tempvec[2] = 0;
	hdist = VectorLength(tempvec);

	VectorMA(point, hdist - tradius, dir, point);
	//now a point on the surface of a cylinder with a radius of tradius
*/
	VectorSubtract(point, tcenter, point_dir);
	VectorNormalize(point_dir);

	// Get bottom to top (vertical) position index
	udot = DotProduct(up, point_dir);
	if(udot>.800)
	{
		Vertical = 4;
	}
	else if(udot>.400)
	{
		Vertical = 3;
	}
	else if(udot>-.333)
	{
		Vertical = 2;
	}
	else if(udot>-.666)
	{
		Vertical = 1;
	}
	else
	{
		Vertical = 0;
	}

	// Get back to front (forward) position index.
	fdot = DotProduct(forward, point_dir);
	if(fdot>.666)
	{
		Forward = 4;
	}
	else if(fdot>.333)
	{
		Forward = 3;
	}
	else if(fdot>-.333)
	{
		Forward = 2;
	}
	else if(fdot>-.666)
	{
		Forward = 1;
	}
	else
	{
		Forward = 0;
	}

	// Get left to right (lateral) position index.
	rdot = DotProduct(right, point_dir);
	if(rdot>.666)
	{
		Lateral = 4;
	}
	else if(rdot>.333)
	{
		Lateral = 3;
	}
	else if(rdot>-.333)
	{
		Lateral = 2;
	}
	else if(rdot>-.666)
	{
		Lateral = 1;
	}
	else
	{
		Lateral = 0;
	}

	HitLoc = Vertical * 25 + Forward * 5 + Lateral;

	if(HitLoc <= 10)
	{
		// Feet.
		if ( rdot > 0 )
		{
			return HL_FOOT_RT;
		}
		else
		{
			return HL_FOOT_LT;
		}
	}
	else if(HitLoc <= 50)
	{
		// Legs.
		if ( rdot > 0 )
		{
			return HL_LEG_RT;
		}
		else
		{
			return HL_LEG_LT;
		}
	}
	else if(HitLoc == 56||HitLoc == 60||HitLoc == 61||HitLoc == 65||HitLoc == 66||HitLoc == 70)
	{
		// Hands.
		if ( rdot > 0 )
		{
			return HL_HAND_RT;
		}
		else
		{
			return HL_HAND_LT;
		}
	}
	else if(HitLoc == 83||HitLoc == 87||HitLoc == 88||HitLoc == 92||HitLoc == 93||HitLoc == 97)
	{
		// Arms.
		if ( rdot > 0 )
		{
			return HL_ARM_RT;
		}
		else
		{
			return HL_ARM_LT;
		}
	}
	else if((HitLoc >= 107 && HitLoc <= 109)||(HitLoc >= 112 && HitLoc <= 114)||(HitLoc >= 117 && HitLoc <= 119))
	{
		// Head.
		return HL_HEAD;
	}
	else
	{
		if(udot < 0.3)
		{
			return HL_WAIST;
		}
		else if(fdot < 0)
		{
			if(rdot > 0.4)
			{
				return HL_BACK_RT;
			}
			else if(rdot < -0.4)
			{
				return HL_BACK_LT;
			}
			else if(fdot < 0)
			{
				return HL_BACK;
			}
		}
		else
		{
			if(rdot > 0.3)
			{
				return HL_CHEST_RT;
			}
			else if(rdot < -0.3)
			{
				return HL_CHEST_LT;
			}
			else if(fdot < 0)
			{
				return HL_CHEST;
			}
		}
	}
	return HL_NONE;
}

/*
int G_PickPainAnim( gentity_t *self, vec3_t point, int damage )
{
	switch( G_GetHitLocation( self, point ) )
	{
	case HL_FOOT_RT:
		return BOTH_PAIN12;
		//PAIN12 = right foot
		break;
	case HL_FOOT_LT:
		return -1;
		break;
	case HL_LEG_RT:
		if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN11;
		}
		else
		{
			return BOTH_PAIN13;
		}
		//PAIN11 = twitch right leg
		//PAIN13 = right knee
		break;
	case HL_LEG_LT:
		return BOTH_PAIN14;
		//PAIN14 = twitch left leg
		break;
	case HL_BACK_RT:
		return BOTH_PAIN7;
		//PAIN7 = med left shoulder
		break;
	case HL_BACK_LT:
		return Q_irand( BOTH_PAIN15, BOTH_PAIN16 );
		//PAIN15 = med right shoulder
		//PAIN16 = twitch right shoulder
		break;
	case HL_BACK:
		if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN1;
		}
		else
		{
			return BOTH_PAIN5;
		}
		//PAIN1 = back
		//PAIN5 = same as 1
		break;
	case HL_CHEST_RT:
		return BOTH_PAIN3;
		//PAIN3 = long, right shoulder
		break;
	case HL_CHEST_LT:
		return BOTH_PAIN2;
		//PAIN2 = long, left shoulder
		break;
	case HL_WAIST:
	case HL_CHEST:
		if ( !Q_irand( 0, 3 ) )
		{
			return BOTH_PAIN6;
		}
		else if ( !Q_irand( 0, 2 ) )
		{
			return BOTH_PAIN8;
		}
		else if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN17;
		}
		else
		{
			return BOTH_PAIN19;
		}
		//PAIN6 = gut
		//PAIN8 = chest
		//PAIN17 = twitch crotch
		//PAIN19 = med crotch
		break;
	case HL_ARM_RT:
	case HL_HAND_RT:
		return BOTH_PAIN9;
		//PAIN9 = twitch right arm
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		return BOTH_PAIN10;
		//PAIN10 = twitch left arm
		break;
	case HL_HEAD:
		return BOTH_PAIN4;
		//PAIN4 = head
		break;
	default:
		return -1;
		break;
	}
}
*/

void ExplodeDeath( gentity_t *self )
{
//	gentity_t	*tent;
	vec3_t		forward;

	self->takedamage = qfalse;//stop chain reaction runaway loops

	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;

	VectorCopy( self->r.currentOrigin, self->s.pos.trBase );

//	tent = G_TempEntity( self->s.origin, EV_FX_EXPLOSION );
	AngleVectors(self->s.angles, forward, NULL, NULL);

/*
	if ( self->fxID > 0 )
	{
		G_PlayEffect( self->fxID, self->r.currentOrigin, forward );
	}
	else
	*/

	{
//		CG_SurfaceExplosion( self->r.currentOrigin, forward, 20.0f, 12.0f, ((self->spawnflags&4)==qfalse) );	//FIXME: This needs to be consistent to all exploders!
//		G_Sound(self, self->sounds );
	}

	if(self->splashDamage > 0 && self->splashRadius > 0)
	{
		gentity_t *attacker = self;
		if ( self->parent )
		{
			attacker = self->parent;
		}
		G_RadiusDamage( self->r.currentOrigin, attacker, self->splashDamage, self->splashRadius,
				attacker, NULL, MOD_UNKNOWN );
	}

	ObjectDie( self, self, self, 20, 0 );
}


/*
============
ScorePlum
============
*/
void ScorePlum( gentity_t *ent, vec3_t origin, int score ) {
	gentity_t *plum;

	plum = G_TempEntity( origin, EV_SCOREPLUM );
	// only send this temp entity to a single client
	plum->r.svFlags |= SVF_SINGLECLIENT;
	plum->r.singleClient = ent->s.number;
	//
	plum->s.otherEntityNum = ent->s.number;
	plum->s.time = score;
}

/*
============
AddScore

Adds score to both the client and his team
============
*/
extern qboolean g_dontPenalizeTeam; //g_cmds.c
extern void rpg_score(gentity_t *ent, qboolean admin_rp_mode);
void AddScore( gentity_t *ent, vec3_t origin, int score )
{
	/*
	if (level.gametype == GT_SIEGE)
	{ //no scoring in this gametype at all.
		return;
	}
	*/

	if ( !ent->client ) {
		return;
	}
	// no scoring during pre-match warmup
	if ( level.warmupTime ) {
		return;
	}
	// show score plum
	//ScorePlum(ent, origin, score);
	//
	ent->client->ps.persistant[PERS_SCORE] += score;

	if (!ent->NPC && ent->client->sess.amrpgmode == 2 && score > 0)
	{
		rpg_score(ent, qfalse);
	}

	if ( level.gametype == GT_TEAM && !g_dontPenalizeTeam )
		level.teamScores[ ent->client->ps.persistant[PERS_TEAM] ] += score;
	CalculateRanks();
}

/*
=================
TossClientItems

rww - Toss the weapon away from the player in the specified direction
=================
*/
void TossClientWeapon(gentity_t *self, vec3_t direction, float speed)
{
	vec3_t vel;
	gitem_t *item;
	gentity_t *launched;
	int weapon = self->s.weapon;
	int ammoSub;

	if (level.gametype == GT_SIEGE)
	{ //no dropping weaps
		return;
	}

	if (weapon == WP_NONE ||
		weapon == WP_MELEE ||
		weapon == WP_SABER ||
		weapon == WP_EMPLACED_GUN ||
		weapon == WP_TURRET)
	{
		return;
	}

	if (self && self->client && self->NPC && self->client->pers.guardian_invoked_by_id != -1)
	{ // zyk: guardians cant lose their guns
		return;
	}

	// find the item type for this weapon
	item = BG_FindItemForWeapon( weapon );

	ammoSub = (self->client->ps.ammo[weaponData[weapon].ammoIndex] - bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity);

	if (ammoSub < 0)
	{
		int ammoQuan = item->quantity;
		ammoQuan -= (-ammoSub);

		if (ammoQuan <= 0)
		{ //no ammo
			return;
		}
	}

	vel[0] = direction[0]*speed;
	vel[1] = direction[1]*speed;
	vel[2] = direction[2]*speed;

	launched = LaunchItem(item, self->client->ps.origin, vel);

	launched->s.generic1 = self->s.number;
	launched->s.powerups = level.time + 1500;

	launched->count = bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

	self->client->ps.ammo[weaponData[weapon].ammoIndex] -= bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

	if (self->client->ps.ammo[weaponData[weapon].ammoIndex] < 0)
	{
		launched->count -= (-self->client->ps.ammo[weaponData[weapon].ammoIndex]);
		self->client->ps.ammo[weaponData[weapon].ammoIndex] = 0;
	}

	if ((self->client->ps.ammo[weaponData[weapon].ammoIndex] < 1 && weapon != WP_DET_PACK) ||
		(weapon != WP_THERMAL && weapon != WP_DET_PACK && weapon != WP_TRIP_MINE))
	{
		int i = 0;
		int weap = -1;

		self->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);

		while (i < WP_NUM_WEAPONS)
		{
			if ((self->client->ps.stats[STAT_WEAPONS] & (1 << i)) && i != WP_NONE)
			{ //this one's good
				weap = i;
				break;
			}
			i++;
		}

		if (weap != -1)
		{
			self->s.weapon = weap;
			self->client->ps.weapon = weap;
		}
		else
		{
			self->s.weapon = 0;
			self->client->ps.weapon = 0;
		}

		G_AddEvent(self, EV_NOAMMO, weapon);
	}
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
void TossClientItems( gentity_t *self ) {
	gitem_t		*item;
	int			weapon;
	float		angle;
	int			i;
	gentity_t	*drop;

	if (level.gametype == GT_SIEGE)
	{ //just don't drop anything then
		return;
	}

	// drop the weapon if not a gauntlet or machinegun
	weapon = self->s.weapon;

	// make a special check to see if they are changing to a new
	// weapon that isn't the mg or gauntlet.  Without this, a client
	// can pick up a weapon, be killed, and not drop the weapon because
	// their weapon change hasn't completed yet and they are still holding the MG.
	if ( weapon == WP_BRYAR_PISTOL) {
		if ( self->client->ps.weaponstate == WEAPON_DROPPING ) {
			weapon = self->client->pers.cmd.weapon;
		}
		if ( !( self->client->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) ) ) {
			weapon = WP_NONE;
		}
	}

	self->s.bolt2 = weapon;

	// zyk: player or npcs can also drop stun baton and blaster pistol
	if ( weapon != WP_NONE &&
		weapon != WP_MELEE &&
		weapon != WP_SABER &&
		weapon != WP_EMPLACED_GUN &&
		weapon != WP_TURRET &&
		(self->client->ps.ammo[ weaponData[weapon].ammoIndex ] ||
		weapon == WP_STUN_BATON
		) ) {
		gentity_t *te;

		// find the item type for this weapon
		item = BG_FindItemForWeapon( weapon );

		// tell all clients to remove the weapon model on this guy until he respawns
		te = G_TempEntity( vec3_origin, EV_DESTROY_WEAPON_MODEL );
		te->r.svFlags |= SVF_BROADCAST;
		te->s.eventParm = self->s.number;

		// spawn the item
		Drop_Item( self, item, 0 );
	}

	// drop all the powerups if not in teamplay
	if ( level.gametype != GT_TEAM && level.gametype != GT_SIEGE ) {
		angle = 45;
		for ( i = 1 ; i < PW_NUM_POWERUPS ; i++ ) {
			if ( self->client->ps.powerups[ i ] > level.time && i != PW_QUAD && i != PW_NEUTRALFLAG) 
			{ // zyk: do not drop PW_QUAD and PW_NEUTRALFLAG
				item = BG_FindItemForPowerup( i );
				if ( !item ) {
					continue;
				}

				// zyk: RPG players cannot drop force enlightments because they are now used as the effect when using Special Powers
				// and cannot drop neutral flag and quad because they are used by Unique Skill
				if (item->giType == IT_POWERUP && 
					(item->giTag == PW_FORCE_ENLIGHTENED_LIGHT || item->giTag == PW_FORCE_ENLIGHTENED_DARK) && 
					self->client->sess.amrpgmode == 2)
				{
					continue;
				}

				drop = Drop_Item( self, item, angle );
				// decide how many seconds it has left
				drop->count = ( self->client->ps.powerups[ i ] - level.time ) / 1000;
				if ( drop->count < 1 ) {
					drop->count = 1;
				}

				// zyk: artifact holder npc died. Set a targetname in this artifact
				if (self->NPC && self->client->pers.universe_quest_artifact_holder_id != -1 && 
					drop->item->giType == IT_POWERUP && drop->item->giTag == PW_FORCE_BOON)
				{
					drop->targetname = "zyk_quest_artifact";
				}

				angle += 45;
			}
		}
	}
}


/*
==================
LookAtKiller
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker ) {
	vec3_t		dir;

	if ( attacker && attacker != self )
		VectorSubtract (attacker->s.pos.trBase, self->s.pos.trBase, dir);
	else if ( inflictor && inflictor != self )
		VectorSubtract (inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	else {
		self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw ( dir );
}

/*
==================
GibEntity
==================
*/
void GibEntity( gentity_t *self, int killer ) {
	G_AddEvent( self, EV_GIB_PLAYER, killer );
	self->takedamage = qfalse;
	self->s.eType = ET_INVISIBLE;
	self->r.contents = 0;
}

void BodyRid(gentity_t *ent)
{
	trap->UnlinkEntity( (sharedEntity_t *)ent );
	ent->physicsObject = qfalse;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	// NOTENOTE No gibbing right now, this is star wars.
	qboolean doDisint = qfalse;

	if (self->s.eType == ET_NPC)
	{ //well, just rem it then, so long as it's done with its death anim and it's not a standard weapon.
		if ( self->client && self->client->ps.torsoTimer <= 0 &&
			 (meansOfDeath == MOD_UNKNOWN ||
			  meansOfDeath == MOD_WATER ||
			  meansOfDeath == MOD_SLIME ||
			  meansOfDeath == MOD_LAVA ||
			  meansOfDeath == MOD_CRUSH ||
			  meansOfDeath == MOD_TELEFRAG ||
			  meansOfDeath == MOD_FALLING ||
			  meansOfDeath == MOD_SUICIDE ||
			  meansOfDeath == MOD_TARGET_LASER ||
			  meansOfDeath == MOD_TRIGGER_HURT) )
		{
			self->think = G_FreeEntity;
			self->nextthink = level.time;
		}
		return;
	}

	if (self->health < (GIB_HEALTH+1))
	{
		self->health = GIB_HEALTH+1;

		if (self->client && (level.time - self->client->respawnTime) < 2000)
		{
			doDisint = qfalse;
		}
		else
		{
			doDisint = qtrue;
		}
	}

	if (self->client && (self->client->ps.eFlags & EF_DISINTEGRATION))
	{
		return;
	}
	else if (self->s.eFlags & EF_DISINTEGRATION)
	{
		return;
	}

	if (doDisint)
	{
		if (self->client)
		{
			self->client->ps.eFlags |= EF_DISINTEGRATION;
			VectorCopy(self->client->ps.origin, self->client->ps.lastHitLoc);
		}
		else
		{
			self->s.eFlags |= EF_DISINTEGRATION;
			VectorCopy(self->r.currentOrigin, self->s.origin2);

			//since it's the corpse entity, tell it to "remove" itself
			self->think = BodyRid;
			self->nextthink = level.time + 1000;
		}
		return;
	}
}


// these are just for logging, the client prints its own messages
char	*modNames[MOD_MAX] = {
	"MOD_UNKNOWN",
	"MOD_STUN_BATON",
	"MOD_MELEE",
	"MOD_SABER",
	"MOD_BRYAR_PISTOL",
	"MOD_BRYAR_PISTOL_ALT",
	"MOD_BLASTER",
	"MOD_TURBLAST",
	"MOD_DISRUPTOR",
	"MOD_DISRUPTOR_SPLASH",
	"MOD_DISRUPTOR_SNIPER",
	"MOD_BOWCASTER",
	"MOD_REPEATER",
	"MOD_REPEATER_ALT",
	"MOD_REPEATER_ALT_SPLASH",
	"MOD_DEMP2",
	"MOD_DEMP2_ALT",
	"MOD_FLECHETTE",
	"MOD_FLECHETTE_ALT_SPLASH",
	"MOD_ROCKET",
	"MOD_ROCKET_SPLASH",
	"MOD_ROCKET_HOMING",
	"MOD_ROCKET_HOMING_SPLASH",
	"MOD_THERMAL",
	"MOD_THERMAL_SPLASH",
	"MOD_TRIP_MINE_SPLASH",
	"MOD_TIMED_MINE_SPLASH",
	"MOD_DET_PACK_SPLASH",
	"MOD_VEHICLE",
	"MOD_CONC",
	"MOD_CONC_ALT",
	"MOD_FORCE_DARK",
	"MOD_SENTRY",
	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT"
};


/*
==================
CheckAlmostCapture
==================
*/
void CheckAlmostCapture( gentity_t *self, gentity_t *attacker ) {
#if 0
	gentity_t	*ent;
	vec3_t		dir;
	char		*classname;

	// if this player was carrying a flag
	if ( self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG] ||
		self->client->ps.powerups[PW_NEUTRALFLAG] ) {
		// get the goal flag this player should have been going for
		if ( level.gametype == GT_CTF || level.gametype == GT_CTY ) {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_blueflag";
			}
			else {
				classname = "team_CTF_redflag";
			}
		}
		else {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_redflag";
			}
			else {
				classname = "team_CTF_blueflag";
			}
		}
		ent = NULL;
		do
		{
			ent = G_Find(ent, FOFS(classname), classname);
		} while (ent && (ent->flags & FL_DROPPED_ITEM));
		// if we found the destination flag and it's not picked up
		if (ent && !(ent->r.svFlags & SVF_NOCLIENT) ) {
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );
			if ( VectorLength(dir) < 200 ) {
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if ( attacker->client ) {
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
#endif
}

qboolean G_InKnockDown( playerState_t *ps )
{
	switch ( (ps->legsAnim) )
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
		return qtrue;
		break;
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
		return qtrue;
		break;
	}
	return qfalse;
}

static int G_CheckSpecialDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{
	int deathAnim = -1;

	if ( BG_InRoll( &self->client->ps, self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_ROLL;		//# Death anim from a roll
	}
	else if ( BG_FlippingAnim( self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_FLIP;		//# Death anim from a flip
	}
	else if ( G_InKnockDown( &self->client->ps ) )
	{//since these happen a lot, let's handle them case by case
		int animLength = bgAllAnims[self->localAnimIndex].anims[self->client->ps.legsAnim].numFrames * fabs((float)(bgHumanoidAnimations[self->client->ps.legsAnim].frameLerp));
		switch ( self->client->ps.legsAnim )
		{
		case BOTH_KNOCKDOWN1:
			if ( animLength - self->client->ps.legsTimer > 100 )
			{//on our way down
				if ( self->client->ps.legsTimer > 600 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_KNOCKDOWN2:
			if ( animLength - self->client->ps.legsTimer > 700 )
			{//on our way down
				if ( self->client->ps.legsTimer > 600 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_KNOCKDOWN3:
			if ( animLength - self->client->ps.legsTimer > 100 )
			{//on our way down
				if ( self->client->ps.legsTimer > 1300 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_KNOCKDOWN4:
			if ( animLength - self->client->ps.legsTimer > 300 )
			{//on our way down
				if ( self->client->ps.legsTimer > 350 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			else
			{//crouch death
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			break;
		case BOTH_KNOCKDOWN5:
			if ( self->client->ps.legsTimer < 750 )
			{//flat
				deathAnim = BOTH_DEATH_LYING_DN;
			}
			break;
		case BOTH_GETUP1:
			if ( self->client->ps.legsTimer < 350 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );
				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 450 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP2:
			if ( self->client->ps.legsTimer < 150 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 850 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 500 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP3:
			if ( self->client->ps.legsTimer < 250 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 600 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;
				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP4:
			if ( self->client->ps.legsTimer < 250 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 600 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 850 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP5:
			if ( self->client->ps.legsTimer > 850 )
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 1500 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_B1:
			if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 400 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_F1:
			if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B1:
			if ( self->client->ps.legsTimer < 325 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 725 )
			{//spinning up
				deathAnim = BOTH_DEATH_SPIN_180;	//# Death anim when facing backwards
			}
			else if ( self->client->ps.legsTimer < 900 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 50 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B2:
			if ( self->client->ps.legsTimer < 575 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 875 )
			{//spinning up
				deathAnim = BOTH_DEATH_SPIN_180;	//# Death anim when facing backwards
			}
			else if ( self->client->ps.legsTimer < 900 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				//partially up
				deathAnim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B3:
			if ( self->client->ps.legsTimer < 150 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 775 )
			{//flipping
				deathAnim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{//lying down
				//partially up
				deathAnim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B4:
			if ( self->client->ps.legsTimer < 325 )
			{//standing up
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B5:
			if ( self->client->ps.legsTimer < 550 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 1025 )
			{//kicking up
				deathAnim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 50 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B6:
			if ( self->client->ps.legsTimer < 225 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 425 )
			{//crouching up
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else if ( self->client->ps.legsTimer < 825 )
			{//flipping up
				deathAnim = BOTH_DEATHFORWARD3; //backflip
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 225 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F1:
			if ( self->client->ps.legsTimer < 275 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 750 )
			{//flipping
				deathAnim = BOTH_DEATH14;
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 100 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F2:
			if ( self->client->ps.legsTimer < 1200 )
			{//standing
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 225 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		}
	}

	return deathAnim;
}

int G_PickDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{//FIXME: play dead flop anims on body if in an appropriate _DEAD anim when this func is called
	int deathAnim = -1;
	int max_health;
	int legAnim = 0;
	vec3_t objVelocity;

	if (!self || !self->client)
	{
		if (!self || self->s.eType != ET_NPC)
		{ //g2animent
			return 0;
		}
	}

	if (self->client)
	{
		max_health = self->client->ps.stats[STAT_MAX_HEALTH];

		if (self->client->inSpaceIndex && self->client->inSpaceIndex != ENTITYNUM_NONE)
		{
			return BOTH_CHOKE3;
		}
	}
	else
	{
		max_health = 60;
	}

	if (self->client)
	{
		VectorCopy(self->client->ps.velocity, objVelocity);
	}
	else
	{
		VectorCopy(self->s.pos.trDelta, objVelocity);
	}

	if ( hitLoc == HL_NONE )
	{
		hitLoc = G_GetHitLocation( self, point );//self->hitLoc
	}

	if (self->client)
	{
		legAnim = self->client->ps.legsAnim;
	}
	else
	{
		legAnim = self->s.legsAnim;
	}

	if (gGAvoidDismember)
	{
		return BOTH_RIGHTHANDCHOPPEDOFF;
	}

	//dead flops
	switch( legAnim )
	{
	case BOTH_DEATH1:		//# First Death anim
	case BOTH_DEAD1:
	case BOTH_DEATH2:			//# Second Death anim
	case BOTH_DEAD2:
	case BOTH_DEATH8:			//#
	case BOTH_DEAD8:
	case BOTH_DEATH13:			//#
	case BOTH_DEAD13:
	case BOTH_DEATH14:			//#
	case BOTH_DEAD14:
	case BOTH_DEATH16:			//#
	case BOTH_DEAD16:
	case BOTH_DEADBACKWARD1:		//# First thrown backward death finished pose
	case BOTH_DEADBACKWARD2:		//# Second thrown backward death finished pose
		deathAnim = -2;
		break;
		/*
		if ( PM_FinishedCurrentLegsAnim( self ) )
		{//done with the anim
			deathAnim = BOTH_DEADFLOP2;
		}
		else
		{
			deathAnim = -2;
		}
		break;
	case BOTH_DEADFLOP2:
		deathAnim = BOTH_DEADFLOP2;
		break;
		*/
	case BOTH_DEATH10:			//#
	case BOTH_DEAD10:
	case BOTH_DEATH15:			//#
	case BOTH_DEAD15:
	case BOTH_DEADFORWARD1:		//# First thrown forward death finished pose
	case BOTH_DEADFORWARD2:		//# Second thrown forward death finished pose
		deathAnim = -2;
		break;
		/*
		if ( PM_FinishedCurrentLegsAnim( self ) )
		{//done with the anim
			deathAnim = BOTH_DEADFLOP1;
		}
		else
		{
			deathAnim = -2;
		}
		break;
		*/
	case BOTH_DEADFLOP1:
		deathAnim = -2;
		//deathAnim = BOTH_DEADFLOP1;
		break;
	case BOTH_DEAD3:				//# Third Death finished pose
	case BOTH_DEAD4:				//# Fourth Death finished pose
	case BOTH_DEAD5:				//# Fifth Death finished pose
	case BOTH_DEAD6:				//# Sixth Death finished pose
	case BOTH_DEAD7:				//# Seventh Death finished pose
	case BOTH_DEAD9:				//#
	case BOTH_DEAD11:			//#
	case BOTH_DEAD12:			//#
	case BOTH_DEAD17:			//#
	case BOTH_DEAD18:			//#
	case BOTH_DEAD19:			//#
	case BOTH_LYINGDEAD1:		//# Killed lying down death finished pose
	case BOTH_STUMBLEDEAD1:		//# Stumble forward death finished pose
	case BOTH_FALLDEAD1LAND:		//# Fall forward and splat death finished pose
	case BOTH_DEATH3:			//# Third Death anim
	case BOTH_DEATH4:			//# Fourth Death anim
	case BOTH_DEATH5:			//# Fifth Death anim
	case BOTH_DEATH6:			//# Sixth Death anim
	case BOTH_DEATH7:			//# Seventh Death anim
	case BOTH_DEATH9:			//#
	case BOTH_DEATH11:			//#
	case BOTH_DEATH12:			//#
	case BOTH_DEATH17:			//#
	case BOTH_DEATH18:			//#
	case BOTH_DEATH19:			//#
	case BOTH_DEATHFORWARD1:		//# First Death in which they get thrown forward
	case BOTH_DEATHFORWARD2:		//# Second Death in which they get thrown forward
	case BOTH_DEATHBACKWARD1:	//# First Death in which they get thrown backward
	case BOTH_DEATHBACKWARD2:	//# Second Death in which they get thrown backward
	case BOTH_DEATH1IDLE:		//# Idle while close to death
	case BOTH_LYINGDEATH1:		//# Death to play when killed lying down
	case BOTH_STUMBLEDEATH1:		//# Stumble forward and fall face first death
	case BOTH_FALLDEATH1:		//# Fall forward off a high cliff and splat death - start
	case BOTH_FALLDEATH1INAIR:	//# Fall forward off a high cliff and splat death - loop
	case BOTH_FALLDEATH1LAND:	//# Fall forward off a high cliff and splat death - hit bottom
		deathAnim = -2;
		break;
	}
	if ( deathAnim == -1 )
	{
		if (self->client)
		{
			deathAnim = G_CheckSpecialDeathAnim( self, point, damage, mod, hitLoc );
		}

		if (deathAnim == -1)
		{
			//death anims
			switch( hitLoc )
			{
			case HL_FOOT_RT:
			case HL_FOOT_LT:
				if ( mod == MOD_SABER && !Q_irand( 0, 2 ) )
				{
					return BOTH_DEATH10;//chest: back flip
				}
				else if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_RT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_LT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_BACK:
				if ( !VectorLengthSquared( objVelocity ) )
				{
					deathAnim = BOTH_DEATH17;//head/back: croak
				}
				else
				{
					if ( !Q_irand( 0, 2 ) )
					{
						deathAnim = BOTH_DEATH4;//back: forward
					}
					else if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH5;//same as 4
					}
					else
					{
						deathAnim = BOTH_DEATH15;//back: forward
					}
				}
				break;
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_HAND_RT:
			case HL_BACK_RT:
				if ( damage <= max_health*0.25 )
				{
					deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH3;//chest right: back
				}
				else if ( damage <= max_health*0.75 )
				{
					deathAnim = BOTH_DEATH6;//chest right: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH8;//chest right: spin high
					}
					else
					{
						switch ( Q_irand( 0, 2 ) )
						{
						default:
						case 0:
							deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
							break;
						case 1:
							deathAnim = BOTH_DEATH3;//chest right: back
							break;
						case 2:
							deathAnim = BOTH_DEATH6;//chest right: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_HAND_LT:
			case HL_BACK_LT:
				if ( damage <= max_health*0.25 )
				{
					deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH7;//chest left: back
				}
				else if ( damage <= max_health*0.75 )
				{
					deathAnim = BOTH_DEATH12;//chest left: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH14;//chest left: spin high
					}
					else
					{
						switch ( Q_irand( 0, 2 ) )
						{
						default:
						case 0:
							deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
							break;
						case 1:
							deathAnim = BOTH_DEATH7;//chest left: back
							break;
						case 2:
							deathAnim = BOTH_DEATH12;//chest left: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST:
			case HL_WAIST:
				if ( damage <= max_health*0.25 || !VectorLengthSquared( objVelocity ) )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH18;//gut: fall right
					}
					else
					{
						deathAnim = BOTH_DEATH19;//gut: fall left
					}
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH2;//chest: backward short
				}
				else if ( damage <= max_health*0.75 )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH1;//chest: backward med
					}
					else
					{
						deathAnim = BOTH_DEATH16;//same as 1
					}
				}
				else
				{
					deathAnim = BOTH_DEATH10;//chest: back flip
				}
				break;
			case HL_HEAD:
				if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH17;//head/back: croak
				}
				else
				{
					deathAnim = BOTH_DEATH13;//head: stumble, fall back
				}
				break;
			default:
				break;
			}
		}
	}

	// Validate.....
	if ( deathAnim == -1 || !BG_HasAnimation( self->localAnimIndex, deathAnim ))
	{
		// I guess we'll take what we can get.....
		deathAnim = BG_PickAnim( self->localAnimIndex, BOTH_DEATH1, BOTH_DEATH25 );
	}

	return deathAnim;
}

gentity_t *G_GetJediMaster(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.isJediMaster)
		{
			return ent;
		}

		i++;
	}

	return NULL;
}

/*
-------------------------
G_AlertTeam
-------------------------
*/

void G_AlertTeam( gentity_t *victim, gentity_t *attacker, float radius, float soundDist )
{
	int			radiusEnts[ 128 ];
	gentity_t	*check;
	vec3_t		mins, maxs;
	int			numEnts;
	int			i;
	float		distSq, sndDistSq = (soundDist*soundDist);

	if ( attacker == NULL || attacker->client == NULL )
		return;

	//Setup the bbox to search in
	for ( i = 0; i < 3; i++ )
	{
		mins[i] = victim->r.currentOrigin[i] - radius;
		maxs[i] = victim->r.currentOrigin[i] + radius;
	}

	//Get the number of entities in a given space
	numEnts = trap->EntitiesInBox( mins, maxs, radiusEnts, 128 );

	//Cull this list
	for ( i = 0; i < numEnts; i++ )
	{
		check = &g_entities[radiusEnts[i]];

		//Validate clients
		if ( check->client == NULL )
			continue;

		//only want NPCs
		if ( check->NPC == NULL )
			continue;

		//Don't bother if they're ignoring enemies
//		if ( check->svFlags & SVF_IGNORE_ENEMIES )
//			continue;

		//This NPC specifically flagged to ignore alerts
		if ( check->NPC->scriptFlags & SCF_IGNORE_ALERTS )
			continue;

		//This NPC specifically flagged to ignore alerts
		if ( !(check->NPC->scriptFlags&SCF_LOOK_FOR_ENEMIES) )
			continue;

		//this ent does not participate in group AI
		if ( (check->NPC->scriptFlags&SCF_NO_GROUPS) )
			continue;

		//Skip the requested avoid check if present
		if ( check == victim )
			continue;

		//Skip the attacker
		if ( check == attacker )
			continue;

		//Must be on the same team
		if ( check->client->playerTeam != victim->client->playerTeam )
			continue;

		//Must be alive
		if ( check->health <= 0 )
			continue;

		if ( check->enemy == NULL )
		{//only do this if they're not already mad at someone
			distSq = DistanceSquared( check->r.currentOrigin, victim->r.currentOrigin );
			if ( distSq > 16384 /*128 squared*/ && !trap->InPVS( victim->r.currentOrigin, check->r.currentOrigin ) )
			{//not even potentially visible/hearable
				continue;
			}
			//NOTE: this allows sound alerts to still go through doors/PVS if the teammate is within 128 of the victim...
			if ( soundDist <= 0 || distSq > sndDistSq )
			{//out of sound range
				if ( !InFOV( victim, check, check->NPC->stats.hfov, check->NPC->stats.vfov )
					||  !NPC_ClearLOS2( check, victim->r.currentOrigin ) )
				{//out of FOV or no LOS
					continue;
				}
			}

			//FIXME: This can have a nasty cascading effect if setup wrong...
			G_SetEnemy( check, attacker );
		}
	}
}

/*
-------------------------
G_DeathAlert
-------------------------
*/

#define	DEATH_ALERT_RADIUS			512
#define	DEATH_ALERT_SOUND_RADIUS	512

void G_DeathAlert( gentity_t *victim, gentity_t *attacker )
{//FIXME: with all the other alert stuff, do we really need this?
	G_AlertTeam( victim, attacker, DEATH_ALERT_RADIUS, DEATH_ALERT_SOUND_RADIUS );
}

/*
----------------------------------------
DeathFX

Applies appropriate special effects that occur while the entity is dying
Not to be confused with NPC_RemoveBodyEffects (NPC.cpp), which only applies effect when removing the body
----------------------------------------
*/

void DeathFX( gentity_t *ent )
{
	vec3_t		effectPos, right;
	vec3_t		defaultDir;

	if ( !ent || !ent->client )
		return;

	VectorSet(defaultDir, 0, 0, 1);

	// team no longer indicates species/race.  NPC_class should be used to identify certain npc types
	switch(ent->client->NPC_class)
	{
	case CLASS_MOUSE:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 20;
		G_PlayEffectID( G_EffectIndex("env/small_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mouse/misc/death1") );
		break;

	case CLASS_PROBE:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] += 50;
		G_PlayEffectID( G_EffectIndex("explosions/probeexplosion1"), effectPos, defaultDir );
		break;

	case CLASS_ATST:
		AngleVectors( ent->r.currentAngles, NULL, right, NULL );
		VectorMA( ent->r.currentOrigin, 20, right, effectPos );
		effectPos[2] += 180;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -40, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		break;

	case CLASS_SEEKER:
	case CLASS_REMOTE:
		G_PlayEffectID( G_EffectIndex("env/small_explode"), ent->r.currentOrigin, defaultDir );
		break;

	case CLASS_GONK:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 5;
//		statusTextIndex = Q_irand( IGT_RESISTANCEISFUTILE, IGT_NAMEIS8OF12 );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex(va("sound/chars/gonk/misc/death%d.wav",Q_irand( 1, 3 ))) );
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		break;

	// should list all remaining droids here, hope I didn't miss any
	case CLASS_R2D2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 10;
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_PROTOCOL: //c3p0
	case CLASS_R5D2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 10;
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_MARK2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_INTERROGATOR:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/interrogator/misc/int_droid_explo") );
		break;

	case CLASS_MARK1:
		AngleVectors( ent->r.currentAngles, NULL, right, NULL );
		VectorMA( ent->r.currentOrigin, 10, right, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -20, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -20, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark1/misc/mark1_explo") );
		break;

	case CLASS_SENTRY:
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/sentry/misc/sentry_explo") );
		VectorCopy( ent->r.currentOrigin, effectPos );
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		break;

	default:
		break;

	}

}

void G_CheckVictoryScript(gentity_t *self)
{
	if ( !G_ActivateBehavior( self, BSET_VICTORY ) )
	{
		if ( self->NPC && self->s.weapon == WP_SABER )
		{//Jedi taunt from within their AI
			self->NPC->blockedSpeechDebounceTime = 0;//get them ready to taunt
			return;
		}
		if ( self->client && self->client->NPC_class == CLASS_GALAKMECH )
		{
			self->wait = 1;
			TIMER_Set( self, "gloatTime", Q_irand( 5000, 8000 ) );
			self->NPC->blockedSpeechDebounceTime = 0;//get him ready to taunt
			return;
		}
		//FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
		if ( self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC && self->NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand( 0, 2 ) )
		{//sometimes have the group commander speak instead
			self->NPC->group->commander->NPC->greetingDebounceTime = level.time + Q_irand( 2000, 5000 );
			//G_AddVoiceEvent( self->NPC->group->commander, Q_irand(EV_VICTORY1, EV_VICTORY3), 2000 );
		}
		else if ( self->NPC )
		{
			self->NPC->greetingDebounceTime = level.time + Q_irand( 2000, 5000 );
			//G_AddVoiceEvent( self, Q_irand(EV_VICTORY1, EV_VICTORY3), 2000 );
		}
	}
}

void G_AddPowerDuelScore(int team, int score)
{
	int i = 0;
	gentity_t *check;

	while (i < MAX_CLIENTS)
	{
		check = &g_entities[i];
		if (check->inuse && check->client &&
			check->client->pers.connected == CON_CONNECTED && !check->client->iAmALoser &&
			check->client->ps.stats[STAT_HEALTH] > 0 &&
			check->client->sess.sessionTeam != TEAM_SPECTATOR &&
			check->client->sess.duelTeam == team)
		{ //found a living client on the specified team
			check->client->sess.wins += score;
			ClientUserinfoChanged(check->s.number);
		}
		i++;
	}
}

void G_AddPowerDuelLoserScore(int team, int score)
{
	int i = 0;
	gentity_t *check;

	while (i < MAX_CLIENTS)
	{
		check = &g_entities[i];
		if (check->inuse && check->client &&
			check->client->pers.connected == CON_CONNECTED &&
			(check->client->iAmALoser || (check->client->ps.stats[STAT_HEALTH] <= 0 && check->client->sess.sessionTeam != TEAM_SPECTATOR)) &&
			check->client->sess.duelTeam == team)
		{ //found a living client on the specified team
			check->client->sess.losses += score;
			ClientUserinfoChanged(check->s.number);
		}
		i++;
	}
}

/*
==================
player_die
==================
*/
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

extern void AI_DeleteSelfFromGroup( gentity_t *self );
extern void AI_GroupMemberKilled( gentity_t *self );
extern void Boba_FlyStop( gentity_t *self );
extern qboolean Jedi_WaitingAmbush( gentity_t *self );
void CheckExitRules( void );
extern void Rancor_DropVictim( gentity_t *self );

extern qboolean g_dontFrickinCheck;
extern qboolean g_endPDuel;
extern qboolean g_noPDuelCheck;
extern void saberReactivate(gentity_t *saberent, gentity_t *saberOwner);
extern void saberBackToOwner(gentity_t *saberent);
extern void quest_get_new_player(gentity_t *ent);
extern void try_finishing_race();
extern void save_account_to_db();
extern void remove_credits(gentity_t *ent, int credits);
extern void zyk_NPC_Kill_f( char *name );
extern gentity_t *Zyk_NPC_SpawnType(char *npc_type, int x, int y, int z, int yaw);
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern void player_restore_force(gentity_t *ent);
extern void load_custom_quest_mission();
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	gentity_t	*ent;
	int			anim;
	int			killer;
	int			i;
	char		*killerName, *obit;
	qboolean	wasJediMaster = qfalse;
	int			sPMType = 0;
	char		buf[512] = {0};
	gentity_t	*quest_player = NULL;

	if ( self->client->ps.pm_type == PM_DEAD ) {
		return;
	}

	if ( level.intermissiontime ) {
		return;
	}

	if ( !attacker )
		return;

	save_account(self, qtrue);

	// zyk: remove any quest_power status from this player
	self->client->pers.quest_power_status = 0;
	self->client->pers.player_statuses &= ~(1 << 20);
	self->client->pers.unique_skill_duration = 0;

	// zyk: stoping Unique Abilities when player dies
	self->client->pers.player_statuses &= ~(1 << 21);
	self->client->pers.player_statuses &= ~(1 << 22);
	self->client->pers.player_statuses &= ~(1 << 23);

	// zyk: resetting boss battle music to default one if needed
	if (self->client->pers.guardian_invoked_by_id != -1 && self->client->pers.guardian_mode != 15 && self->client->pers.guardian_mode != 17 && self->client->pers.guardian_mode != 18)
	{
		level.boss_battle_music_reset_timer = level.time + 1000;
	}
	else if (self->client->sess.amrpgmode == 2 && self->client->pers.guardian_mode > 0 && self->client->pers.can_play_quest == 1)
	{ // zyk: quest player died. Reset boss battle music and guardian_mode of his allies
		int ally_it = 0;

		level.boss_battle_music_reset_timer = level.time + 1000;

		for (ally_it = 0; ally_it < level.maxclients; ally_it++)
		{
			gentity_t *this_ent = &g_entities[ally_it];

			if (zyk_is_ally(self,this_ent) == qtrue)
			{
				this_ent->client->pers.guardian_mode = 0;
			}
		}
	}

	if (self->client->pers.player_statuses & (1 << 28))
	{// zyk: custom quest npc defeated
		if (self->client->playerTeam == NPCTEAM_PLAYER)
		{
			level.zyk_quest_ally_npc_count--;

			if (level.zyk_quest_ally_npc_count == 0)
			{ // zyk: all enemy npcs defeated
				load_custom_quest_mission();

				trap->SendServerCommand(-1, "chat \"^3Custom Quest: ^7Mission failed\n\"");
			}
		}
		else
		{
			level.zyk_quest_npc_count--;

			// zyk: increasing the number of steps done in this mission
			zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done", va("%d", atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done")) + 1));

			if (level.zyk_quest_npc_count == 0)
			{ // zyk: all enemy npcs defeated
				level.zyk_hold_quest_mission = qfalse;
			}
		}
	}

	// zyk: if someone dies by a custom quest npc and it has the recovery field, recover some of its health
	if (attacker && attacker->client && attacker && attacker->NPC && attacker->client->pers.player_statuses & (1 << 28))
	{
		int zyk_npc_recovery = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "npcrecovery"));

		if ((attacker->health + zyk_npc_recovery) < attacker->client->ps.stats[STAT_MAX_HEALTH])
		{
			attacker->health += zyk_npc_recovery;
		}
		else
		{
			attacker->health = attacker->client->ps.stats[STAT_MAX_HEALTH];
		}
	}

	if (self->client->pers.race_position > 0) // zyk: if a player dies during a race, he loses the race
	{
		self->client->pers.race_position = 0;
		trap->SendServerCommand( -1, va("chat \"^3Race System: ^7%s ^7died during the race!\n\"",self->client->pers.netname) );
		try_finishing_race();
	}

	// zyk: player died in Sniper Battle
	if (level.sniper_mode == 2 && self->s.number < MAX_CLIENTS && level.sniper_players[self->s.number] != -1)
	{
		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7died in Sniper Battle!\n\"", self->client->pers.netname));
		level.sniper_players[self->s.number] = -1;
		level.sniper_mode_quantity--;

		// zyk: resetting his force powers
		self->client->ps.fd.forceDeactivateAll = 0;

		WP_InitForcePowers(self);

		if (attacker && attacker->client && attacker->s.number < MAX_CLIENTS && level.sniper_players[attacker->s.number] != -1)
		{ // zyk: adding score to the attacker
			level.sniper_players[attacker->s.number]++;
		}
	}

	// zyk: player died in RPG LMS
	if (level.rpg_lms_mode == 2 && self->s.number < MAX_CLIENTS && level.rpg_lms_players[self->s.number] != -1)
	{
		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7died in RPG LMS!\n\"", self->client->pers.netname));
		level.rpg_lms_players[self->s.number] = -1;
		level.rpg_lms_quantity--;

		if (attacker && attacker->client && attacker->s.number < MAX_CLIENTS && level.rpg_lms_players[attacker->s.number] != -1)
		{ // zyk: adding score to the attacker
			level.rpg_lms_players[attacker->s.number]++;
		}
	}

	// zyk: player died in Duel Tournament
	if (level.duel_tournament_mode == 4 && self->s.number < MAX_CLIENTS && level.duel_players[self->s.number] != -1 && 
		duel_tournament_is_duelist(self) == qtrue)
	{
		// zyk: restoring force to this player
		player_restore_force(self);

		// zyk: lost the duel
		self->client->pers.player_statuses |= (1 << 27);
	}

	// zyk: player died in Melee Battle
	if (level.melee_mode == 2 && self->s.number < MAX_CLIENTS && level.melee_players[self->s.number] != -1)
	{
		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7died in Melee Battle!\n\"", self->client->pers.netname));
		level.melee_players[self->s.number] = -1;
		level.melee_mode_quantity--;

		// zyk: resetting his force powers
		WP_InitForcePowers(self);

		if (attacker && attacker->client && attacker->s.number < MAX_CLIENTS && level.melee_players[attacker->s.number] != -1)
		{ // zyk: adding score to the attacker
			level.melee_players[attacker->s.number]++;
		}
	}

	// zyk: if player dies being mind controlled or controlling someone, stop mind control
	if (self->client->pers.being_mind_controlled > -1)
	{
		gentity_t *controller_ent = &g_entities[self->client->pers.being_mind_controlled];
		controller_ent->client->pers.mind_controlled1_id = -1;
		self->client->pers.being_mind_controlled = -1;
	}

	if (!self->NPC && self->client->sess.amrpgmode == 2 && self->client->pers.rpg_class == 1 && self->client->pers.mind_controlled1_id > -1)
	{
		gentity_t *controlled_ent = &g_entities[self->client->pers.mind_controlled1_id];
		self->client->pers.mind_controlled1_id = -1;
		controlled_ent->client->pers.being_mind_controlled = -1;
	}

	if (self->client->pers.guardian_invoked_by_id != -1)
	{ // zyk: rpg mode boss. Getting the quest player
		quest_player = &g_entities[self->client->pers.guardian_invoked_by_id];

		if (quest_player && quest_player->client && quest_player->client->pers.guardian_mode == 0)
		{ // zyk: player died before. Do not give anything to quest_player
			quest_player = NULL;
		}
	}

	if (self->client->pers.universe_quest_messages == -10000 && self->NPC)
	{ // zyk: Ymir or Thor defeated
		int j = 0;
		qboolean still_has_boss = qfalse;
		quest_player = &g_entities[self->client->pers.universe_quest_objective_control];

		if (Q_stricmp(self->NPC_type, "guardian_of_universe") == 0)
		{ // zyk: failed mission
			zyk_text_message(quest_player, "universe/mission_16_guardians/mission_16_guardians_fail", qtrue, qfalse, quest_player->client->pers.netname);

			quest_get_new_player(quest_player);
		}
		else
		{
			for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
			{
				gentity_t *old_boss = &g_entities[j];

				if (old_boss && old_boss->NPC && old_boss->health > 0 && old_boss->client && old_boss->client->pers.universe_quest_messages == -10000 && 
					Q_stricmp(old_boss->NPC_type, "guardian_of_universe") != 0)
				{
					if (Q_stricmp(old_boss->NPC_type, "ymir_boss") == 0)
					{
						zyk_text_message(quest_player, "universe/mission_16_guardians/mission_16_guardians_ymir", qtrue, qfalse);
					}
					else
					{
						zyk_text_message(quest_player, "universe/mission_16_guardians/mission_16_guardians_thor", qtrue, qfalse);
					}

					still_has_boss = qtrue;
				}
			}

			if (still_has_boss == qfalse)
			{
				for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
				{
					gentity_t *old_npc = &g_entities[j];

					if (old_npc && old_npc->NPC && old_npc->health > 0 && old_npc->client && Q_stricmp(old_npc->NPC_type, "quest_mage") == 0 && old_npc->die)
					{ // zyk: killing the remaining mages
						old_npc->health = 0;
						old_npc->client->ps.stats[STAT_HEALTH] = 0;
						old_npc->die(old_npc, old_npc, old_npc, 100, MOD_UNKNOWN);
					}
				}

				quest_player->client->pers.universe_quest_messages = 6;
				quest_player->client->pers.universe_quest_timer = level.time + 3000;
			}
		}
	}
	else if (self->client->pers.universe_quest_artifact_holder_id != -1 && self->NPC)
	{ // zyk: artifact holder of Universe Quest, set the player universe_quest_artifact_holder_id to -2 so he can get the artifact when he touches the force boon item
		if (Q_stricmp( self->NPC_type, "quest_ragnos" ) == 0) // zyk: quest_ragnos npc has a different way to get the artifact
		{
			gentity_t *player_ent = &g_entities[self->client->pers.universe_quest_artifact_holder_id];

			zyk_text_message(player_ent, "universe/mission_2/mission_2_artifact_guardian_fail", qtrue, qfalse);
			player_ent->client->pers.universe_quest_artifact_holder_id = -1;

			// zyk: fixed bug in which a boss battle would kill this npc and pass quest turn
			if (player_ent->client->pers.guardian_mode == 0)
				quest_get_new_player(player_ent);
		}
	}
	else if (self->client->pers.universe_quest_objective_control > -1 && self->NPC)
	{ // zyk: Universe Quest objective verification
		gentity_t *the_old_player = &g_entities[self->client->pers.universe_quest_objective_control];

		self->client->pers.universe_quest_objective_control = -1;

		if (Q_stricmp( self->NPC_type, "quest_protocol_imp" ) == 0)
		{ // zyk: quest_protocol_imp npc of the sixth objective of Universe Quest died, player can now receive the Amulet of Darkness from the jawa by setting this value to universe_quest_messages
			the_old_player->client->pers.universe_quest_messages = 51;
		}
		else if (the_old_player->client->pers.universe_quest_progress == 6 && Q_stricmp( self->NPC_type, "quest_reborn_boss" ) == 0)
		{ // zyk: quest reborn npc of the Master of Evil mission of Universe Quest died
			the_old_player->client->pers.universe_quest_messages = 2;
		}
		else if (Q_stricmp( self->NPC_type, "quest_sand_raider_green" ) == 0 || Q_stricmp( self->NPC_type, "quest_sand_raider_brown" ) == 0 || Q_stricmp( self->NPC_type, "quest_sand_raider_blue" ) == 0 || Q_stricmp( self->NPC_type, "quest_sand_raider_red" ) == 0)
		{
			the_old_player->client->pers.universe_quest_objective_control--;
			if (the_old_player->client->pers.universe_quest_objective_control == 0)
			{ // zyk: killed all raiders, set 65 so player can receive the Amulet of Eternity from the jawa citizen
				the_old_player->client->pers.universe_quest_messages = 65;
			}
		}
		else if ((Q_stricmp( self->NPC_type, "sage_of_light" ) == 0 || Q_stricmp( self->NPC_type, "sage_of_darkness" ) == 0 || 
				 Q_stricmp( self->NPC_type, "sage_of_eternity" ) == 0) && the_old_player->client->sess.amrpgmode == 2 && 
				 the_old_player->client->pers.universe_quest_progress == 0 && level.quest_map == 9 && 
				 the_old_player->client->pers.universe_quest_messages != 14) // zyk: if its a Sage, player fails the objective
		{
			zyk_text_message(the_old_player, "universe/mission_0/mission_0_fail", qtrue, qfalse, the_old_player->client->pers.netname);

			// zyk: if player fails the first Universe Quest objective, pass the turn to another player
			the_old_player->client->pers.universe_quest_messages = 14;
			the_old_player->client->pers.universe_quest_timer = level.time + 3000;
			zyk_NPC_Kill_f("all");
		}
		else if (the_old_player->client->sess.amrpgmode == 2 && the_old_player->client->pers.universe_quest_objective_control != -1 && 
				 the_old_player->client->pers.universe_quest_progress == 0 && level.quest_map == 9 && 
				 the_old_player->client->pers.universe_quest_messages != 14 && 
				 (the_old_player->client->pers.universe_quest_objective_control > 1 || Q_stricmp(self->NPC_type, "quest_reborn_boss") == 0))
		{
			the_old_player->client->pers.universe_quest_objective_control--;

			if (the_old_player->client->pers.universe_quest_objective_control == 0)
			{ // zyk: all quest reborn npcs were defeated. The player then completed the first Universe Quest objective
				the_old_player->client->pers.universe_quest_messages = 12;
			}
		}
		else if (Q_stricmp( self->NPC_type, "sage_of_universe" ) == 0 && the_old_player->client->sess.amrpgmode == 2 && the_old_player->client->pers.universe_quest_objective_control == 5 && the_old_player->client->pers.universe_quest_progress == 4)
		{ // zyk: Sage of Universe died in the fifth Universe Quest objective, pass turn to another player
			zyk_text_message(the_old_player, "universe/mission_4/mission_4_fail", qtrue, qfalse, the_old_player->client->pers.netname);

			quest_get_new_player(the_old_player);
		}
		else if (the_old_player->client->pers.universe_quest_progress == 11)
		{ // zyk: Battle for the Temple, soldier was defeated by the player
			if (Q_stricmp( self->NPC_type, "quest_super_soldier" ) == 0)
			{
				the_old_player->client->pers.universe_quest_objective_control--;

				if (the_old_player->client->pers.universe_quest_objective_control == 0)
				{ // zyk: player defeated all soldiers, so he completed the mission
					zyk_text_message(the_old_player, "universe/mission_11/mission_11_end", qtrue, qfalse);
					the_old_player->client->pers.hunter_quest_timer = level.time + 3000;
					the_old_player->client->pers.hunter_quest_messages = 40;
				}
				else if (the_old_player->client->pers.universe_quest_objective_control == 10)
				{ // zyk: after the player defeats some soldiers, Master of Evil will send more
					the_old_player->client->pers.hunter_quest_messages = 12;
					zyk_text_message(the_old_player, "universe/mission_11/mission_11_more", qtrue, qfalse);
				}
			}
			else
			{
				zyk_text_message(the_old_player, "universe/mission_11/mission_11_fail", qtrue, qfalse, the_old_player->client->pers.netname);
				the_old_player->client->pers.hunter_quest_timer = level.time + 3000;
				the_old_player->client->pers.hunter_quest_messages = 50;
			}
		}
		else if (the_old_player->client->pers.universe_quest_progress == 16 && the_old_player->client->pers.universe_quest_counter & (1 << 0))
		{ // zyk: Save the City mission, mage was defeated by the player
			if (Q_stricmp(self->NPC_type, "quest_mage") == 0)
			{
				int j = 0, mages_count = 0;

				for (j = MAX_CLIENTS + BODY_QUEUE_SIZE; j < level.num_entities; j++)
				{
					gentity_t *mage_ent = &g_entities[j];

					if (mage_ent && mage_ent->NPC && mage_ent->health > 0 && Q_stricmp(mage_ent->NPC_type, "quest_mage") == 0)
					{
						mages_count++;
					}
				}

				if (mages_count == 0)
				{ // zyk: defeated all mages
					zyk_text_message(the_old_player, "universe/mission_16_sages/mission_16_sages_end", qtrue, qfalse, the_old_player->client->pers.netname);

					the_old_player->client->pers.universe_quest_messages = 100;
					the_old_player->client->pers.universe_quest_timer = level.time + 3000;
				}
			}
		}
		else if (the_old_player->client->pers.universe_quest_progress == 18 && the_old_player->client->pers.universe_quest_counter & (1 << 2))
		{ // zyk: War at the City mission, citizen or sage was defeated by the player
			int j = 0, citizens_count = 0, key_enemies = 0;

			for (j = MAX_CLIENTS + BODY_QUEUE_SIZE; j < level.num_entities; j++)
			{
				gentity_t *mage_ent = &g_entities[j];

				if (mage_ent && mage_ent->NPC && mage_ent->health > 0)
				{
					if (Q_stricmp(mage_ent->NPC_type, "quest_citizen_warrior") == 0)
					{
						citizens_count++;
					}

					if (strncmp(mage_ent->NPC_type, "sage_of", 7) == 0 || strncmp(mage_ent->NPC_type, "guardian_", 9) == 0)
					{
						key_enemies++;
					}
				}
			}

			if (citizens_count == 0 && key_enemies == 0)
			{ // zyk: defeated all citizens
				zyk_text_message(the_old_player, "universe/mission_18_thor/mission_18_thor_conquer", qtrue, qfalse, the_old_player->client->pers.netname);

				the_old_player->client->pers.universe_quest_messages = 100;
				the_old_player->client->pers.universe_quest_timer = level.time + 3000;
			}
			else if (((citizens_count + key_enemies) % 5) == 0)
			{ // zyk: killed some enemies
				the_old_player->client->pers.hunter_quest_messages = 1;
			}
		}
	}
	else if (quest_player && (quest_player->client->pers.guardian_mode <= 8 || quest_player->client->pers.guardian_mode == 11 || quest_player->client->pers.guardian_mode == 16))
	{ // zyk: Light Quest. If guardian was defeated by the invoker, increase the defeated_guardians value
		if (quest_player->client->pers.guardian_mode == 8)
		{ // zyk: defeated the Guardian of Light
			quest_player->client->pers.defeated_guardians = NUMBER_OF_GUARDIANS;

			if (quest_player->client->pers.magic_power > 0)
			{
				quest_player->client->pers.magic_power--;
				quest_player->client->pers.quest_power_status |= (1 << 14);
			}

			zyk_text_message(quest_player, "light/boss_defeated", qtrue, qfalse);
		}
		else
		{
			int light_quest_bitvalue = quest_player->client->pers.guardian_mode + 3;
			if (quest_player->client->pers.guardian_mode == 11)
			{
				light_quest_bitvalue = 11;
			}
			else if (quest_player->client->pers.guardian_mode == 16)
			{
				light_quest_bitvalue = 12;
			}

			quest_player->client->pers.defeated_guardians |= (1 << light_quest_bitvalue);

			// zyk: make the chat message for each guardian the player defeats
			if (light_quest_bitvalue == 4)
			{
				zyk_text_message(quest_player, "light/guardian_of_water_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 5)
			{
				zyk_text_message(quest_player, "light/guardian_of_earth_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 6)
			{
				zyk_text_message(quest_player, "light/guardian_of_forest_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 7)
			{
				zyk_text_message(quest_player, "light/guardian_of_intelligence_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 8)
			{
				zyk_text_message(quest_player, "light/guardian_of_agility_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 9)
			{
				zyk_text_message(quest_player, "light/guardian_of_fire_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 10)
			{
				zyk_text_message(quest_player, "light/guardian_of_wind_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 11)
			{
				zyk_text_message(quest_player, "light/guardian_of_resistance_defeated", qtrue, qfalse);
			}
			else if (light_quest_bitvalue == 12)
			{
				zyk_text_message(quest_player, "light/guardian_of_ice_defeated", qtrue, qfalse);
			}
		}

		quest_player->client->pers.guardian_mode = 0;
		quest_player->client->pers.light_quest_messages = 0;

		save_account(quest_player, qtrue);

		quest_get_new_player(quest_player);
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 9)
	{ // zyk: Dark Quest. Defeated the Guardian of Darkness
		quest_player->client->pers.guardian_mode = 0;
		quest_player->client->pers.hunter_quest_progress = NUMBER_OF_OBJECTIVES;

		save_account(quest_player, qtrue);

		if (quest_player->client->pers.magic_power > 0)
		{
			quest_player->client->pers.magic_power--;
			quest_player->client->pers.quest_power_status |= (1 << 15);
		}

		zyk_text_message(quest_player, "dark/boss_defeated", qtrue, qfalse);

		quest_get_new_player(quest_player);
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 10)
	{ // zyk: Eternity Quest. Defeated the Guardian of Eternity
		quest_player->client->pers.guardian_mode = 0;
		quest_player->client->pers.eternity_quest_progress = NUMBER_OF_ETERNITY_QUEST_OBJECTIVES;

		save_account(quest_player, qtrue);

		if (quest_player->client->pers.magic_power > 0)
		{
			quest_player->client->pers.magic_power--;
			quest_player->client->pers.quest_power_status |= (1 << 16);
		}

		zyk_text_message(quest_player, "eternity/boss_defeated", qtrue, qfalse);

		quest_get_new_player(quest_player);
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 12)
	{ // zyk: defeated the Master of Evil
		quest_player->client->pers.universe_quest_messages = 12;
		quest_player->client->pers.universe_quest_timer = level.time + 2000;
		quest_player->client->pers.guardian_mode = 0;

		zyk_text_message(quest_player, "universe/mission_6/mission_6_boss_defeated", qtrue, qfalse);
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 13)
	{ // zyk: defeated the Guardian of Universe
		quest_player->client->pers.universe_quest_messages = 5;
		quest_player->client->pers.universe_quest_timer = level.time + 2000;
		quest_player->client->pers.guardian_mode = 0;
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 14)
	{ // zyk: defeated the Guardian of Chaos
		quest_player->client->pers.universe_quest_messages = 20;
		quest_player->client->pers.universe_quest_timer = level.time + 8000;
		quest_player->client->pers.guardian_mode = 0;
		G_Sound(self, CHAN_VOICE, G_SoundIndex("sound/chars/ragnos/misc/death3.mp3"));
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 15)
	{ // zyk: defeated either Ymir or Thor
		int j = 0;
		qboolean still_has_boss = qfalse;

		G_Sound(self, CHAN_VOICE, G_SoundIndex("sound/chars/ragnos/misc/death3.mp3"));

		for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
		{
			gentity_t *old_boss = &g_entities[j];

			if (old_boss && old_boss->NPC && old_boss->health > 0 && old_boss->client && old_boss->client->pers.guardian_mode == 15)
			{
				if (Q_stricmp(old_boss->NPC_type, "ymir_boss") == 0)
				{
					zyk_text_message(quest_player, "universe/mission_20_sages/mission_20_sages_thor_defeated", qtrue, qfalse);
				}
				else
				{
					zyk_text_message(quest_player, "universe/mission_20_sages/mission_20_sages_ymir_defeated", qtrue, qfalse);
				}

				// zyk: wrath of the remaining boss makes him stronger
				old_boss->spawnflags |= 131072;
				old_boss->client->pers.quest_power_status |= (1 << 13);

				still_has_boss = qtrue;
			}
		}

		if (still_has_boss == qfalse)
		{
			level.boss_battle_music_reset_timer = level.time + 1000;

			quest_player->client->pers.universe_quest_messages = 7;
			quest_player->client->pers.universe_quest_timer = level.time + 5000;
			quest_player->client->pers.guardian_mode = 0;
		}
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 17)
	{ // zyk: defeated a boss in Guardian Trials
		quest_player->client->pers.light_quest_messages++;
		quest_player->client->pers.hunter_quest_messages--;

		if (quest_player->client->pers.light_quest_messages == 9)
		{ // zyk: defeated all bosses
			level.boss_battle_music_reset_timer = level.time + 1000;

			quest_player->client->pers.universe_quest_messages = 11;
			quest_player->client->pers.universe_quest_timer = level.time + 3000;
		}
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 18)
	{ // zyk: defeated a boss in The Final Challenge (Guardians Sequel)
		quest_player->client->pers.light_quest_messages++;

		if (quest_player->client->pers.light_quest_messages == 4)
		{ // zyk: defeated all bosses
			level.boss_battle_music_reset_timer = level.time + 1000;

			quest_player->client->pers.universe_quest_messages = 6;
			quest_player->client->pers.universe_quest_timer = level.time + 3000;
		}
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 19)
	{ // zyk: defeated Ymir
		quest_player->client->pers.universe_quest_messages = 3;
		quest_player->client->pers.universe_quest_timer = level.time + 2000;
		quest_player->client->pers.guardian_mode = 0;
		G_Sound(self, CHAN_VOICE, G_SoundIndex("sound/chars/ragnos/misc/death3.mp3"));
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 20)
	{ // zyk: defeated the Guardian of Time
		quest_player->client->pers.universe_quest_messages = 3;
		quest_player->client->pers.universe_quest_timer = level.time + 3000;
		quest_player->client->pers.guardian_mode = 0;

		zyk_text_message(quest_player, "universe/mission_20_thor/mission_20_thor_boss_defeated", qtrue, qfalse);
	}
	else if (quest_player && quest_player->client->pers.guardian_mode == 21)
	{ // zyk: defeated the Soul of Sorrow
		quest_player->client->pers.universe_quest_messages = 54;
		quest_player->client->pers.universe_quest_timer = level.time + 3000;
		quest_player->client->pers.guardian_mode = 0;

		zyk_text_message(quest_player, "universe/mission_20_time/mission_20_time_boss_defeated", qtrue, qfalse);
	}
	
	if (self->client->sess.amrpgmode == 2)
	{ 
		if (self->client->pers.guardian_mode > 0)
		{ // zyk: player lost to a guardian
			self->client->pers.guardian_mode = 0;
		}

		// zyk: removing the armors from the player
		self->client->pers.player_statuses &= ~(1 << 8);
		self->client->pers.player_statuses &= ~(1 << 9);

		// zyk: removing the crystals from the player
		self->client->pers.player_statuses &= ~(1 << 10);
		self->client->pers.player_statuses &= ~(1 << 11);

		// zyk: player has the Resurrection Power, after completing quests in Challenge Mode. Uses mp. Not allowed in CTF gametype
		if (self->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && self->client->pers.universe_quest_counter & (1 << 29) && g_gametype.integer != GT_CTF && 
			!(self->client->ps.eFlags2 & EF2_HELD_BY_MONSTER) && self->client->pers.magic_power >= 5 && zyk_enable_resurrection_power.integer == 1 && 
			!(self->client->sess.magic_more_disabled_powers & (1 << 1)))
		{
			qboolean zyk_allow_vehicle_resurrect = qtrue; // zyk: if player is riding a ship, do not allow resurrection to avoid invisible player bug

			if (self->client->NPC_class != CLASS_VEHICLE
				&& self->client->ps.m_iVehicleNum)
			{ //I'm riding a vehicle
				//tell it I'm getting off
				gentity_t *veh = &g_entities[self->client->ps.m_iVehicleNum];

				if (veh->inuse && veh->client && veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
				{
					zyk_allow_vehicle_resurrect = qfalse;
				}
			}

			if (zyk_allow_vehicle_resurrect == qtrue)
			{
				self->client->pers.magic_power -= 5;
				self->client->pers.quest_power_status |= (1 << 10);
				self->client->pers.quest_power1_timer = level.time + 3000;
			}
		}
	}

	//check player stuff
	g_dontFrickinCheck = qfalse;

	if (level.gametype == GT_POWERDUEL)
	{ //don't want to wait til later in the frame if this is the case
		CheckExitRules();

		if ( level.intermissiontime )
		{
			return;
		}
	}

	if (self->s.eType == ET_NPC &&
		self->s.NPC_class == CLASS_VEHICLE &&
		self->m_pVehicle &&
		!self->m_pVehicle->m_pVehicleInfo->explosionDelay &&
		(self->m_pVehicle->m_pPilot || self->m_pVehicle->m_iNumPassengers > 0 || self->m_pVehicle->m_pDroidUnit))
	{ //kill everyone on board in the name of the attacker... if the vehicle has no death delay
		gentity_t *murderer = NULL;
		gentity_t *killEnt;

		if (self->client->ps.otherKillerTime >= level.time)
		{ //use the last attacker
			murderer = &g_entities[self->client->ps.otherKiller];
			if (!murderer->inuse || !murderer->client)
			{
				murderer = NULL;
			}
			else
			{
				if (murderer->s.number >= MAX_CLIENTS &&
					murderer->s.eType == ET_NPC &&
					murderer->s.NPC_class == CLASS_VEHICLE &&
					murderer->m_pVehicle &&
					murderer->m_pVehicle->m_pPilot)
				{
					gentity_t *murderPilot = &g_entities[murderer->m_pVehicle->m_pPilot->s.number];
					if (murderPilot->inuse && murderPilot->client)
					{ //give the pilot of the offending vehicle credit for the kill
						murderer = murderPilot;
					}
				}
			}
		}
		else if (attacker && attacker->inuse && attacker->client)
		{
			if (attacker->s.number >= MAX_CLIENTS &&
				attacker->s.eType == ET_NPC &&
				attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle &&
				attacker->m_pVehicle->m_pPilot)
			{ //set vehicles pilot's killer as murderer
				murderer = &g_entities[attacker->m_pVehicle->m_pPilot->s.number];
				if (murderer->inuse && murderer->client &&murderer->client->ps.otherKillerTime >= level.time)
				{
					murderer = &g_entities[murderer->client->ps.otherKiller];
					if (!murderer->inuse || !murderer->client)
					{
						murderer = NULL;
					}
				}
				else
				{
					murderer = NULL;
				}
			}
			else
			{
				murderer = &g_entities[attacker->s.number];
			}
		}
		else if (self->m_pVehicle->m_pPilot)
		{
			murderer = (gentity_t *)self->m_pVehicle->m_pPilot;
			if (!murderer->inuse || !murderer->client)
			{
				murderer = NULL;
			}
		}

		//no valid murderer.. just use self I guess
		if (!murderer)
		{
			murderer = self;
		}

		if ( self->m_pVehicle->m_pVehicleInfo->hideRider )
		{//pilot is *inside* me, so kill him, too
			killEnt = (gentity_t *)self->m_pVehicle->m_pPilot;
			if (killEnt && killEnt->inuse && killEnt->client)
			{
				G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
			}
			if ( self->m_pVehicle->m_pVehicleInfo )
			{//FIXME: this wile got stuck in an endless loop, that's BAD!!  This method SUCKS (not initting "i", not incrementing it or using it directly, all sorts of badness), so I'm rewriting it
				//while (i < self->m_pVehicle->m_iNumPassengers)
				int numPass = self->m_pVehicle->m_iNumPassengers;
				for ( i = 0; i < numPass && self->m_pVehicle->m_iNumPassengers; i++ )
				{//go through and eject the last passenger
					killEnt = (gentity_t *)self->m_pVehicle->m_ppPassengers[self->m_pVehicle->m_iNumPassengers-1];
					if ( killEnt )
					{
						self->m_pVehicle->m_pVehicleInfo->Eject(self->m_pVehicle, (bgEntity_t *)killEnt, qtrue);
						if ( killEnt->inuse && killEnt->client )
						{
							G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
						}
					}
				}
			}
		}
		killEnt = (gentity_t *)self->m_pVehicle->m_pDroidUnit;
		if (killEnt && killEnt->inuse && killEnt->client)
		{
			killEnt->flags &= ~FL_UNDYING;
			G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
		}
	}

	self->client->ps.emplacedIndex = 0;

	G_BreakArm(self, 0); //unbreak anything we have broken
	self->client->ps.saberEntityNum = self->client->saberStoredIndex; //in case we died while our saber was knocked away.

	if (self->client->ps.weapon == WP_SABER && self->client->saberKnockedTime)
	{
		gentity_t *saberEnt = &g_entities[self->client->ps.saberEntityNum];
		//trap->Print("DEBUG: Running saber cleanup for %s\n", self->client->pers.netname);
		self->client->saberKnockedTime = 0;
		saberReactivate(saberEnt, self);
		saberEnt->r.contents = CONTENTS_LIGHTSABER;
		saberEnt->think = saberBackToOwner;
		saberEnt->nextthink = level.time;
		G_RunObject(saberEnt);
	}

	self->client->bodyGrabIndex = ENTITYNUM_NONE;
	self->client->bodyGrabTime = 0;

	if (self->client->holdingObjectiveItem > 0)
	{ //carrying a siege objective item - make sure it updates and removes itself from us now in case this is an instant death-respawn situation
		gentity_t *objectiveItem = &g_entities[self->client->holdingObjectiveItem];

		if (objectiveItem->inuse && objectiveItem->think)
		{
            objectiveItem->think(objectiveItem);
		}
	}

	if ( (self->client->inSpaceIndex && self->client->inSpaceIndex != ENTITYNUM_NONE) ||
		 (self->client->ps.eFlags2 & EF2_SHIP_DEATH) )
	{
		self->client->noCorpse = qtrue;
	}

	if ( self->client->NPC_class != CLASS_VEHICLE
		&& self->client->ps.m_iVehicleNum )
	{ //I'm riding a vehicle
		//tell it I'm getting off
		gentity_t *veh = &g_entities[self->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)self, qtrue);

			if (veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
			{ //go into "die in ship" mode with flag
				self->client->ps.eFlags2 |= EF2_SHIP_DEATH;

				//put me over where my vehicle exploded
				G_SetOrigin(self, veh->client->ps.origin);
				VectorCopy(veh->client->ps.origin, self->client->ps.origin);
			}
		}
		//droids throw heads if they haven't yet
		switch(self->client->NPC_class)
		{
		case CLASS_R2D2:
			if ( !trap->G2API_GetSurfaceRenderStatus( self->ghoul2, 0, "head" ) )
			{
				vec3_t	up;
				AngleVectors( self->r.currentAngles, NULL, NULL, up );
				G_PlayEffectID( G_EffectIndex("chunks/r2d2head_veh"), self->r.currentOrigin, up );
			}
			break;

		case CLASS_R5D2:
			if ( !trap->G2API_GetSurfaceRenderStatus( self->ghoul2, 0, "head" ) )
			{
				vec3_t	up;
				AngleVectors( self->r.currentAngles, NULL, NULL, up );
				G_PlayEffectID( G_EffectIndex("chunks/r5d2head_veh"), self->r.currentOrigin, up );
			}
			break;
		default:
			break;
		}
	}

	if ( self->NPC )
	{
		if ( self->client && Jedi_WaitingAmbush( self ) )
		{//ambushing trooper
			self->client->noclip = qfalse;
		}
		NPC_FreeCombatPoint( self->NPC->combatPoint, qfalse );
		if ( self->NPC->group )
		{
			//lastInGroup = (self->NPC->group->numGroup < 2);
			AI_GroupMemberKilled( self );
			AI_DeleteSelfFromGroup( self );
		}

		if ( self->NPC->tempGoal )
		{
			G_FreeEntity( self->NPC->tempGoal );
			self->NPC->tempGoal = NULL;
		}
		/*
		if ( self->s.eFlags & EF_LOCKED_TO_WEAPON )
		{
			// dumb, just get the NPC out of the chair
extern void RunEmplacedWeapon( gentity_t *ent, usercmd_t **ucmd );

			usercmd_t cmd, *ad_cmd;

			memset( &cmd, 0, sizeof( usercmd_t ));

			//gentity_t *old = self->owner;

			if ( self->owner )
			{
				self->owner->s.frame = self->owner->startFrame = self->owner->endFrame = 0;
				self->owner->svFlags &= ~SVF_ANIMATING;
			}

			cmd.buttons |= BUTTON_USE;
			ad_cmd = &cmd;
			RunEmplacedWeapon( self, &ad_cmd );
			//self->owner = old;
		}
		*/
		if ( self->client->NPC_class == CLASS_BOBAFETT && self->client->ps.eFlags2 & EF2_FLYING )
			Boba_FlyStop( self );
		if ( self->s.NPC_class == CLASS_RANCOR )
			Rancor_DropVictim( self );
	}
	if ( attacker && attacker->NPC && attacker->NPC->group && attacker->NPC->group->enemy == self )
	{
		attacker->NPC->group->enemy = NULL;
	}

	//Cheap method until/if I decide to put fancier stuff in (e.g. sabers falling out of hand and slowly
	//holstering on death like sp)
	if (self->client->ps.weapon == WP_SABER &&
		!self->client->ps.saberHolstered &&
		self->client->ps.saberEntityNum)
	{
		if (!self->client->ps.saberInFlight &&
			self->client->saber[0].soundOff)
		{
			G_Sound(self, CHAN_AUTO, self->client->saber[0].soundOff);
		}
		if (self->client->saber[1].soundOff &&
			self->client->saber[1].model[0])
		{
			G_Sound(self, CHAN_AUTO, self->client->saber[1].soundOff);
		}
	}

	//Use any target we had
	G_UseTargets( self, self );

	if (g_slowmoDuelEnd.integer && (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && attacker && attacker->inuse && attacker->client)
	{
		if (!gDoSlowMoDuel)
		{
			gDoSlowMoDuel = qtrue;
			gSlowMoDuelTime = level.time;
		}
	}
	/*
	else if (self->NPC && attacker && attacker->client && attacker->s.number < MAX_CLIENTS && !gDoSlowMoDuel)
	{
		gDoSlowMoDuel = qtrue;
		gSlowMoDuelTime = level.time;
	}
	*/

	//Make sure the jetpack is turned off.
	Jetpack_Off(self);

	self->client->ps.heldByClient = 0;
	self->client->beingThrown = 0;
	self->client->doingThrow = 0;
	BG_ClearRocketLock( &self->client->ps );
	self->client->isHacking = 0;
	self->client->ps.hackingTime = 0;

	if (inflictor && inflictor->activator && !inflictor->client && !attacker->client &&
		inflictor->activator->client && inflictor->activator->inuse &&
		inflictor->s.weapon == WP_TURRET)
	{
		attacker = inflictor->activator;
	}

	if (self->client && self->client->ps.isJediMaster)
	{
		wasJediMaster = qtrue;
	}

	//if he was charging or anything else, kill the sound
	G_MuteSound(self->s.number, CHAN_WEAPON);

	//FIXME: this may not be enough
	if ( level.gametype == GT_SIEGE && meansOfDeath == MOD_TEAM_CHANGE )
		RemoveDetpacks( self );
	else
		BlowDetpacks(self); //blow detpacks if they're planted

	self->client->ps.fd.forceDeactivateAll = 1;

	if ((self == attacker || (attacker && !attacker->client)) &&
		(meansOfDeath == MOD_CRUSH || meansOfDeath == MOD_FALLING || meansOfDeath == MOD_TRIGGER_HURT || meansOfDeath == MOD_UNKNOWN) &&
		self->client->ps.otherKillerTime > level.time)
	{
		attacker = &g_entities[self->client->ps.otherKiller];
	}

	// zyk: setting the credits_modifier and the bonus score for the RPG player
	if (attacker && attacker->client && attacker->client->sess.amrpgmode == 2)
	{
		if (!self->NPC && self->client->sess.amrpgmode == 2)
		{ // zyk: RPG Mode player score and credits
			attacker->client->pers.credits_modifier = self->client->pers.level;
			attacker->client->pers.score_modifier = self->client->pers.level / 50;

			if (self->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES)
			{
				attacker->client->pers.score_modifier += 1;
				attacker->client->pers.credits_modifier += 20;
			}
		}
		else if (self->NPC && self->client->NPC_class == CLASS_VEHICLE)
		{ // zyk: vehicles will not give any score or credits
			attacker->client->pers.credits_modifier = -10;
			attacker->client->pers.score_modifier = -1;
		}
		else if (self->NPC && self->client->pers.guardian_invoked_by_id != -1)
		{ // zyk: guardians give more score and credits
			attacker->client->pers.credits_modifier = 490;
			attacker->client->pers.score_modifier = 6;
		}
		else if (self->NPC && self->client->ps.fd.forcePowerMax > 0 && self->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
		{ // zyk: force-user saber npcs give more score and credits
			attacker->client->pers.credits_modifier = 10;
			attacker->client->pers.score_modifier = 1;
		}

		if (self->NPC && self->client->ps.stats[STAT_MAX_HEALTH] >= 500 && self->client->pers.guardian_invoked_by_id == -1)
		{ // zyk: npcs with more than 500 hp gives more score
			attacker->client->pers.score_modifier += 1;
			attacker->client->pers.credits_modifier += 20;
		}

		if (self->NPC && self->client->pers.credits_modifier > 0)
		{ // zyk: npc with a custom amount of credits set
			attacker->client->pers.credits_modifier = self->client->pers.credits_modifier;
		}

		if (level.guardian_quest > 0 && self->NPC && self->s.number == level.guardian_quest)
		{ // zyk: if player defeated the map guardian npc
			attacker->client->pers.score_modifier = 2;
			attacker->client->pers.credits_modifier = 990;
			trap->SendServerCommand(-1, va("chat \"^3Guardian Quest: ^7%s^7 receives ^31000 ^7credits for defeating the Guardian of Map\n\"", attacker->client->pers.netname));
			level.guardian_quest = 0;
			level.boss_battle_music_reset_timer = level.time + 1000;
			level.guardian_quest_timer = level.time + zyk_guardian_quest_timer.integer;
		}

		if (attacker->client->pers.rpg_class == 2)
		{ // zyk: Bounty Hunter class receives more credits
			attacker->client->pers.credits_modifier += 5 * (attacker->client->pers.skill_levels[55] + 1);
		}

		// zyk: Bounty Quest manager
		if (level.bounty_quest_choose_target == qfalse && attacker != self && self->client->sess.amrpgmode == 2)
		{
			if (level.bounty_quest_target_id == (attacker - g_entities))
			{ // zyk: attacker was the target, so the attacker receives bonus credits
				int bonus_credits = self->client->pers.level * 2;

				attacker->client->pers.credits_modifier += bonus_credits;
				trap->SendServerCommand(-1, va("chat \"^3Bounty Quest: ^7%s ^7was defeated by the target player, ^3%d ^7bonus credits\n\"", self->client->pers.netname, bonus_credits));
			}
			else if (level.bounty_quest_target_id == (self - g_entities))
			{ // zyk: target player was defeated. Gives the reward to the attacker
				attacker->client->pers.credits_modifier += (self->client->pers.level * 15);
				level.bounty_quest_choose_target = qtrue;
				level.bounty_quest_target_id++;
				trap->SendServerCommand(-1, va("chat \"^3Bounty Quest: ^7%s^7 receives ^3%d ^7bonus credits\n\"", attacker->client->pers.netname, (self->client->pers.level * 15)));
			}
		}
	}

	if (level.guardian_quest > 0 && self->NPC && self->s.number == level.guardian_quest)
	{ // zyk: map guardian npc defeated by a non-rpg player
		trap->SendServerCommand(-1, va("chat \"^3Guardian Quest: ^7Map Guardian not defeated by rpg player\n\""));
		level.guardian_quest = 0;
		level.boss_battle_music_reset_timer = level.time + 1000;
	}

	// check for an almost capture
	CheckAlmostCapture( self, attacker );

	self->client->ps.pm_type = PM_DEAD;
	self->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;

	if ( attacker ) {
		killer = attacker->s.number;
		if ( attacker->client ) {
			killerName = attacker->client->pers.netname;
		} else {
			killerName = "<non-client>";
		}
	} else {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( killer < 0 || killer >= MAX_CLIENTS ) {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( meansOfDeath < 0 || meansOfDeath >= sizeof( modNames ) / sizeof( modNames[0] ) ) {
		obit = "<bad obituary>";
	} else {
		obit = modNames[ meansOfDeath ];
	}

	// log the victim and attacker's names with the method of death
	Com_sprintf( buf, sizeof( buf ), "Kill: %i %i %i: %s killed ", killer, self->s.number, meansOfDeath, killerName );
	if ( self->s.eType == ET_NPC ) {
		// check for named NPCs
		if ( self->targetname )
			Q_strcat( buf, sizeof( buf ), va( "%s (%s) by %s\n", self->NPC_type, self->targetname, obit ) );
		else
			Q_strcat( buf, sizeof( buf ), va( "%s by %s\n", self->NPC_type, obit ) );
	}
	else
		Q_strcat( buf, sizeof( buf ), va( "%s by %s\n", self->client->pers.netname, obit ) );
	G_LogPrintf( "%s", buf );

	if ( g_austrian.integer
		&& level.gametype == GT_DUEL
		&& level.numPlayingClients >= 2 )
	{
		int spawnTime = (level.clients[level.sortedClients[0]].respawnTime > level.clients[level.sortedClients[1]].respawnTime) ? level.clients[level.sortedClients[0]].respawnTime : level.clients[level.sortedClients[1]].respawnTime;
		G_LogPrintf("Duel Kill Details:\n");
		G_LogPrintf("Kill Time: %d\n", level.time-spawnTime );
		G_LogPrintf("victim: %s, hits on enemy %d\n", self->client->pers.netname, self->client->ps.persistant[PERS_HITS] );
		if ( attacker && attacker->client )
		{
			G_LogPrintf("killer: %s, hits on enemy %d, health: %d\n", attacker->client->pers.netname, attacker->client->ps.persistant[PERS_HITS], attacker->health );
			//also - if MOD_SABER, list the animation and saber style
			if ( meansOfDeath == MOD_SABER )
			{
				G_LogPrintf("killer saber style: %d, killer saber anim %s\n", attacker->client->ps.fd.saberAnimLevel, animTable[(attacker->client->ps.torsoAnim)].name );
			}
		}
	}

	G_LogWeaponKill(killer, meansOfDeath);
	G_LogWeaponDeath(self->s.number, self->s.weapon);
	if (attacker && attacker->client && attacker->inuse)
	{
		G_LogWeaponFrag(killer, self->s.number);
	}

	// broadcast the death event to everyone
	if (self->s.eType != ET_NPC && !g_noPDuelCheck)
	{
		ent = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
		ent->s.eventParm = meansOfDeath;
		ent->s.otherEntityNum = self->s.number;
		ent->s.otherEntityNum2 = killer;
		ent->r.svFlags = SVF_BROADCAST;	// send to everyone
		ent->s.isJediMaster = wasJediMaster;
	}

	self->enemy = attacker;

	self->client->ps.persistant[PERS_KILLED]++;

	if (self == attacker)
	{
		self->client->ps.fd.suicides++;
	}

	if (attacker && attacker->client) {
		attacker->client->lastkilled_client = self->s.number;

		G_CheckVictoryScript(attacker);

		if ( attacker == self || OnSameTeam (self, attacker ) ) {
			if (level.gametype == GT_DUEL)
			{ //in duel, if you kill yourself, the person you are dueling against gets a kill for it
				int otherClNum = -1;
				if (level.sortedClients[0] == self->s.number)
				{
					otherClNum = level.sortedClients[1];
				}
				else if (level.sortedClients[1] == self->s.number)
				{
					otherClNum = level.sortedClients[0];
				}

				if (otherClNum >= 0 && otherClNum < MAX_CLIENTS &&
					g_entities[otherClNum].inuse && g_entities[otherClNum].client &&
					otherClNum != attacker->s.number)
				{
					AddScore( &g_entities[otherClNum], self->r.currentOrigin, 1 );
				}
				else
				{
					AddScore( attacker, self->r.currentOrigin, -1 );
				}
			}
			else
			{
				AddScore( attacker, self->r.currentOrigin, -1 );
			}
			if (level.gametype == GT_JEDIMASTER)
			{
				if (self->client && self->client->ps.isJediMaster)
				{ //killed ourself so return the saber to the original position
				  //(to avoid people jumping off ledges and making the saber
				  //unreachable for 60 seconds)
					ThrowSaberToAttacker(self, NULL);
					self->client->ps.isJediMaster = qfalse;
				}
			}
		} else {
			if (level.gametype == GT_JEDIMASTER)
			{
				if ((attacker->client && attacker->client->ps.isJediMaster) ||
					(self->client && self->client->ps.isJediMaster))
				{
					AddScore( attacker, self->r.currentOrigin, 1 );

					if (self->client && self->client->ps.isJediMaster)
					{
						ThrowSaberToAttacker(self, attacker);
						self->client->ps.isJediMaster = qfalse;
					}
				}
				else
				{
					gentity_t *jmEnt = G_GetJediMaster();

					if (jmEnt && jmEnt->client)
					{
						AddScore( jmEnt, self->r.currentOrigin, 1 );
					}
				}
			}
			else
			{
				AddScore( attacker, self->r.currentOrigin, 1 );
			}

			if( meansOfDeath == MOD_STUN_BATON ) {

				// play humiliation on player
				attacker->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;

				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

				// also play humiliation on target
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			}

			// check for two kills in a short amount of time
			// if this is close enough to the last kill, give a reward sound
			if ( level.time - attacker->client->lastKillTime < CARNAGE_REWARD_TIME ) {
				// play excellent on player
				attacker->client->ps.persistant[PERS_EXCELLENT_COUNT]++;

				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
			attacker->client->lastKillTime = level.time;

		}
	} else {
		if (self->client && self->client->ps.isJediMaster)
		{ //killed ourself so return the saber to the original position
		  //(to avoid people jumping off ledges and making the saber
		  //unreachable for 60 seconds)
			ThrowSaberToAttacker(self, NULL);
			self->client->ps.isJediMaster = qfalse;
		}

		if (level.gametype == GT_DUEL)
		{ //in duel, if you kill yourself, the person you are dueling against gets a kill for it
			int otherClNum = -1;
			if (level.sortedClients[0] == self->s.number)
			{
				otherClNum = level.sortedClients[1];
			}
			else if (level.sortedClients[1] == self->s.number)
			{
				otherClNum = level.sortedClients[0];
			}

			if (otherClNum >= 0 && otherClNum < MAX_CLIENTS &&
				g_entities[otherClNum].inuse && g_entities[otherClNum].client &&
				otherClNum != self->s.number)
			{
				AddScore( &g_entities[otherClNum], self->r.currentOrigin, 1 );
			}
			else
			{
				AddScore( self, self->r.currentOrigin, -1 );
			}
		}
		else
		{
			AddScore( self, self->r.currentOrigin, -1 );
		}
	}

	// Add team bonuses
	Team_FragBonuses(self, inflictor, attacker);

	// if I committed suicide, the flag does not fall, it returns.
	if (meansOfDeath == MOD_SUICIDE) {
		if ( self->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
			self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
			self->client->ps.powerups[PW_REDFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
			self->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
	}

	if (!self->client->ps.fallingToDeath && (!self->NPC || self->client->NPC_class != CLASS_VEHICLE)) 
	{ // zyk: now npcs also drop their weapons and powerups
		TossClientItems( self );
	}
	else {
		if ( self->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
			self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
			self->client->ps.powerups[PW_REDFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
			self->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
	}

	if ( MOD_TEAM_CHANGE == meansOfDeath )
	{
		// Give them back a point since they didn't really die.
		AddScore( self, self->r.currentOrigin, 1 );
	}
	else
	{
		Cmd_Score_f( self );		// show scores
	}

	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t *cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->sess.spectatorClient == self->s.number ) {
			Cmd_Score_f( g_entities + i );
		}
	}

	self->takedamage = qtrue;	// can still be gibbed

	self->s.weapon = WP_NONE;
	self->s.powerups = 0;
	if (self->s.eType != ET_NPC)
	{ //handled differently for NPCs
		self->r.contents = CONTENTS_CORPSE;
	}
	self->client->ps.zoomMode = 0;	// Turn off zooming when we die

	//rww - 07/19/02 - I removed this because it isn't working and it's ugly (for people on the outside)
	/*
	self->s.angles[0] = 0;
	self->s.angles[2] = 0;
	LookAtKiller (self, inflictor, attacker);

	VectorCopy( self->s.angles, self->client->ps.viewangles );
	*/

	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;

	if (self->s.eType != ET_NPC)
	{ //handled differently for NPCs
		self->r.maxs[2] = -8;
	}

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
	self->client->respawnTime = level.time + 1700;

	// remove powerups
	memset( self->client->ps.powerups, 0, sizeof(self->client->ps.powerups) );

	self->client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
	self->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

	// NOTENOTE No gib deaths right now, this is star wars.
	/*
	// never gib in a nodrop
	if ( (self->health <= GIB_HEALTH && !(contents & CONTENTS_NODROP) && g_blood.integer) || meansOfDeath == MOD_SUICIDE)
	{
		// gib death
		GibEntity( self, killer );
	}
	else
	*/
	{
		// normal death

		static int deathAnim;

		anim = G_PickDeathAnim(self, self->pos1, damage, meansOfDeath, HL_NONE);

		if (anim >= 1)
		{ //Some droids don't have death anims
			// for the no-blood option, we need to prevent the health
			// from going to gib level
			if ( self->health <= GIB_HEALTH ) {
				self->health = GIB_HEALTH+1;
			}

			self->client->respawnTime = level.time + 1000;//((self->client->animations[anim].numFrames*40)/(50.0f / self->client->animations[anim].frameLerp))+300;

			sPMType = self->client->ps.pm_type;
			self->client->ps.pm_type = PM_NORMAL; //don't want pm type interfering with our setanim calls.

			if (self->inuse)
			{ //not disconnecting
				G_SetAnim(self, NULL, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_RESTART, 0);
			}

			self->client->ps.pm_type = sPMType;

			if (meansOfDeath == MOD_SABER || (meansOfDeath == MOD_MELEE && G_HeavyMelee( attacker )) )//saber or heavy melee (claws)
			{ //update the anim on the actual skeleton (so bolt point will reflect the correct position) and then check for dismem
				G_UpdateClientAnims(self, 1.0f);
				G_CheckForDismemberment(self, attacker, self->pos1, damage, anim, qfalse);
			}
		}
		else if (self->NPC && self->client && self->client->NPC_class != CLASS_MARK1 &&
			self->client->NPC_class != CLASS_VEHICLE)
		{ //in this case if we're an NPC it's my guess that we want to get removed straight away.
			self->think = G_FreeEntity;
			self->nextthink = level.time;
		}

		//self->client->ps.legsAnim = anim;
		//self->client->ps.torsoAnim = anim;
//		self->client->ps.pm_flags |= PMF_UPDATE_ANIM;		// Make sure the pmove sets up the GHOUL2 anims.

		//rww - do this on respawn, not death
		//CopyToBodyQue (self);

		//G_AddEvent( self, EV_DEATH1 + i, killer );
		if (wasJediMaster)
		{
			G_AddEvent( self, EV_DEATH1 + deathAnim, 1 );
		}
		else
		{
			G_AddEvent( self, EV_DEATH1 + deathAnim, 0 );
		}

		if (self != attacker)
		{ //don't make NPCs want to murder you on respawn for killing yourself!
			G_DeathAlert( self, attacker );
		}

		// the body can still be gibbed
		if (!self->NPC)
		{ //don't remove NPCs like this!
			self->die = body_die;
		}

		//It won't gib, it will disintegrate (because this is Star Wars).
		self->takedamage = qtrue;

		// globally cycle through the different death animations
		deathAnim = ( deathAnim + 1 ) % 3;
	}

	if ( self->NPC )
	{//If an NPC, make sure we start running our scripts again- this gets set to infinite while we fall to our deaths
		self->NPC->nextBStateThink = level.time;
	}

	if ( G_ActivateBehavior( self, BSET_DEATH ) )
	{
		//deathScript = qtrue;
	}

	if ( self->NPC && (self->NPC->scriptFlags&SCF_FFDEATH) )
	{
		if ( G_ActivateBehavior( self, BSET_FFDEATH ) )
		{//FIXME: should running this preclude running the normal deathscript?
			//deathScript = qtrue;
		}
		G_UseTargets2( self, self, self->target4 );
	}

	/*
	if ( !deathScript && !(self->svFlags&SVF_KILLED_SELF) )
	{
		//Should no longer run scripts
		//WARNING!!! DO NOT DO THIS WHILE RUNNING A SCRIPT, ICARUS WILL CRASH!!!
		//FIXME: shouldn't ICARUS handle this internally?
		ICARUS_FreeEnt(self);
	}
	*/
	//rwwFIXMEFIXME: Do this too?

	// Free up any timers we may have on us.
	TIMER_Clear2( self );

	trap->LinkEntity ((sharedEntity_t *)self);

	if ( self->NPC )
	{
		self->NPC->timeOfDeath = level.time;//this will change - used for debouncing post-death events
	}

	// Start any necessary death fx for this entity
	if ( self->NPC )
		DeathFX( self );


	if (level.gametype == GT_POWERDUEL && !g_noPDuelCheck)
	{ //powerduel checks
		if (self->client->sess.duelTeam == DUELTEAM_LONE)
		{ //automatically means a win as there is only one
			G_AddPowerDuelScore(DUELTEAM_DOUBLE, 1);
			G_AddPowerDuelLoserScore(DUELTEAM_LONE, 1);
			g_endPDuel = qtrue;
		}
		else if (self->client->sess.duelTeam == DUELTEAM_DOUBLE)
		{
			gentity_t *check;
			qboolean heLives = qfalse;

			for ( i=0; i<MAX_CLIENTS; i++ )
			{
				check = &g_entities[i];
				if (check->inuse && check->client && check->s.number != self->s.number &&
					check->client->pers.connected == CON_CONNECTED && !check->client->iAmALoser &&
					check->client->ps.stats[STAT_HEALTH] > 0 &&
					check->client->sess.sessionTeam != TEAM_SPECTATOR &&
					check->client->sess.duelTeam == DUELTEAM_DOUBLE)
				{ //still an active living paired duelist so it's not over yet.
					heLives = qtrue;
					break;
				}
			}

			if (!heLives)
			{ //they're all dead, give the lone duelist the win.
				G_AddPowerDuelScore(DUELTEAM_LONE, 1);
				G_AddPowerDuelLoserScore(DUELTEAM_DOUBLE, 1);
				g_endPDuel = qtrue;
			}
		}
	}
}


/*
================
CheckArmor
================
*/
int CheckArmor (gentity_t *ent, int damage, int dflags)
{
	gclient_t	*client;
	int			save;
	int			count;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	if ( client->NPC_class == CLASS_VEHICLE
		&& ent->m_pVehicle
		&& ent->client->ps.electrifyTime > level.time )
	{//ion-cannon has disabled this ship's shields, take damage on hull!
		return 0;
	}
	// armor
	count = client->ps.stats[STAT_ARMOR];

	if (dflags & DAMAGE_HALF_ABSORB)
	{	// Half the damage gets absorbed by the shields, rather than 100%
		save = ceil( damage * ARMOR_PROTECTION );
	}
	else
	{	// All the damage gets absorbed by the shields.
		save = damage;
	}

	// save is the most damage that the armor is elibigle to protect, of course, but it's limited by the total armor.
	if (save >= count)
		save = count;

	if (!save)
		return 0;

	if (dflags & DAMAGE_HALF_ARMOR_REDUCTION)		// Armor isn't whittled so easily by sniper shots.
	{
		client->ps.stats[STAT_ARMOR] -= (int)(save*ARMOR_REDUCTION_FACTOR);
	}
	else
	{
		client->ps.stats[STAT_ARMOR] -= save;
	}

	return save;
}


void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback )
{
	vec3_t	kvel;
	float	mass;

	if ( targ->physicsBounce > 0 )	//overide the mass
		mass = targ->physicsBounce;
	else
		mass = 200;

	if ( g_gravity.value > 0 )
	{
		VectorScale( newDir, g_knockback.value * (float)knockback / mass * 0.8, kvel );
		kvel[2] = newDir[2] * g_knockback.value * (float)knockback / mass * 1.5;
	}
	else
	{
		VectorScale( newDir, g_knockback.value * (float)knockback / mass, kvel );
	}

	if ( targ->client )
	{
		VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );
	}
	else if ( targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType != TR_NONLINEAR_STOP )
	{
		VectorAdd( targ->s.pos.trDelta, kvel, targ->s.pos.trDelta );
		VectorCopy( targ->r.currentOrigin, targ->s.pos.trBase );
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if ( targ->client && !targ->client->ps.pm_time )
	{
		int		t;

		t = knockback * 2;
		if ( t < 50 ) {
			t = 50;
		}
		if ( t > 200 ) {
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

/*
================
RaySphereIntersections
================
*/
int RaySphereIntersections( vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2] ) {
	float b, c, d, t;

	//	| origin - (point + t * dir) | = radius
	//	a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	//	c = (point[0] - origin[0])^2 + (point[1] - origin[1])^2 + (point[2] - origin[2])^2 - radius^2;

	// normalize dir so a = 1
	VectorNormalize(dir);
	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	c = (point[0] - origin[0]) * (point[0] - origin[0]) +
		(point[1] - origin[1]) * (point[1] - origin[1]) +
		(point[2] - origin[2]) * (point[2] - origin[2]) -
		radius * radius;

	d = b * b - 4 * c;
	if (d > 0) {
		t = (- b + sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[0]);
		t = (- b - sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[1]);
		return 2;
	}
	else if (d == 0) {
		t = (- b ) / 2;
		VectorMA(point, t, dir, intersections[0]);
		return 1;
	}
	return 0;
}

/*
===================================
rww - beginning of the majority of the dismemberment and location based damage code.
===================================
*/
char *hitLocName[HL_MAX] =
{
	"none",	//HL_NONE = 0,
	"right foot",	//HL_FOOT_RT,
	"left foot",	//HL_FOOT_LT,
	"right leg",	//HL_LEG_RT,
	"left leg",	//HL_LEG_LT,
	"waist",	//HL_WAIST,
	"back right shoulder",	//HL_BACK_RT,
	"back left shoulder",	//HL_BACK_LT,
	"back",	//HL_BACK,
	"front right shouler",	//HL_CHEST_RT,
	"front left shoulder",	//HL_CHEST_LT,
	"chest",	//HL_CHEST,
	"right arm",	//HL_ARM_RT,
	"left arm",	//HL_ARM_LT,
	"right hand",	//HL_HAND_RT,
	"left hand",	//HL_HAND_LT,
	"head",	//HL_HEAD
	"generic1",	//HL_GENERIC1,
	"generic2",	//HL_GENERIC2,
	"generic3",	//HL_GENERIC3,
	"generic4",	//HL_GENERIC4,
	"generic5",	//HL_GENERIC5,
	"generic6"	//HL_GENERIC6
};

void G_GetDismemberLoc(gentity_t *self, vec3_t boltPoint, int limbType)
{ //Just get the general area without using server-side ghoul2
	vec3_t fwd, right, up;

	AngleVectors(self->r.currentAngles, fwd, right, up);

	VectorCopy(self->r.currentOrigin, boltPoint);

	switch (limbType)
	{
	case G2_MODELPART_HEAD:
		boltPoint[0] += up[0]*24;
		boltPoint[1] += up[1]*24;
		boltPoint[2] += up[2]*24;
		break;
	case G2_MODELPART_WAIST:
		boltPoint[0] += up[0]*4;
		boltPoint[1] += up[1]*4;
		boltPoint[2] += up[2]*4;
		break;
	case G2_MODELPART_LARM:
		boltPoint[0] += up[0]*18;
		boltPoint[1] += up[1]*18;
		boltPoint[2] += up[2]*18;

		boltPoint[0] -= right[0]*10;
		boltPoint[1] -= right[1]*10;
		boltPoint[2] -= right[2]*10;
		break;
	case G2_MODELPART_RARM:
		boltPoint[0] += up[0]*18;
		boltPoint[1] += up[1]*18;
		boltPoint[2] += up[2]*18;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	case G2_MODELPART_RHAND:
		boltPoint[0] += up[0]*8;
		boltPoint[1] += up[1]*8;
		boltPoint[2] += up[2]*8;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	case G2_MODELPART_LLEG:
		boltPoint[0] -= up[0]*4;
		boltPoint[1] -= up[1]*4;
		boltPoint[2] -= up[2]*4;

		boltPoint[0] -= right[0]*10;
		boltPoint[1] -= right[1]*10;
		boltPoint[2] -= right[2]*10;
		break;
	case G2_MODELPART_RLEG:
		boltPoint[0] -= up[0]*4;
		boltPoint[1] -= up[1]*4;
		boltPoint[2] -= up[2]*4;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	default:
		break;
	}

	return;
}

void G_GetDismemberBolt(gentity_t *self, vec3_t boltPoint, int limbType)
{
	int useBolt = self->genericValue5;
	vec3_t properOrigin, properAngles, addVel;
	//matrix3_t legAxis;
	mdxaBone_t	boltMatrix;
	float fVSpeed = 0;
	char *rotateBone = NULL;

	switch (limbType)
	{
	case G2_MODELPART_HEAD:
		rotateBone = "cranium";
		break;
	case G2_MODELPART_WAIST:
		if (self->localAnimIndex <= 1)
		{ //humanoid
			rotateBone = "thoracic";
		}
		else
		{
			rotateBone = "pelvis";
		}
		break;
	case G2_MODELPART_LARM:
		rotateBone = "lradius";
		break;
	case G2_MODELPART_RARM:
		rotateBone = "rradius";
		break;
	case G2_MODELPART_RHAND:
		rotateBone = "rhand";
		break;
	case G2_MODELPART_LLEG:
		rotateBone = "ltibia";
		break;
	case G2_MODELPART_RLEG:
		rotateBone = "rtibia";
		break;
	default:
		rotateBone = "rtibia";
		break;
	}

	useBolt = trap->G2API_AddBolt(self->ghoul2, 0, rotateBone);

	VectorCopy(self->client->ps.origin, properOrigin);
	VectorCopy(self->client->ps.viewangles, properAngles);

	//try to predict the origin based on velocity so it's more like what the client is seeing
	VectorCopy(self->client->ps.velocity, addVel);
	VectorNormalize(addVel);

	if (self->client->ps.velocity[0] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[0]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[0];
	}
	if (self->client->ps.velocity[1] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[1]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[1];
	}
	if (self->client->ps.velocity[2] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[2]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[2];
	}

	fVSpeed *= 0.08f;

	properOrigin[0] += addVel[0]*fVSpeed;
	properOrigin[1] += addVel[1]*fVSpeed;
	properOrigin[2] += addVel[2]*fVSpeed;

	properAngles[0] = 0;
	properAngles[1] = self->client->ps.viewangles[YAW];
	properAngles[2] = 0;

	trap->G2API_GetBoltMatrix(self->ghoul2, 0, useBolt, &boltMatrix, properAngles, properOrigin, level.time, NULL, self->modelScale);

	boltPoint[0] = boltMatrix.matrix[0][3];
	boltPoint[1] = boltMatrix.matrix[1][3];
	boltPoint[2] = boltMatrix.matrix[2][3];

	trap->G2API_GetBoltMatrix(self->ghoul2, 1, 0, &boltMatrix, properAngles, properOrigin, level.time, NULL, self->modelScale);

	if (self->client && limbType == G2_MODELPART_RHAND)
	{ //Make some saber hit sparks over the severed wrist area
		vec3_t boltAngles;
		gentity_t *te;

		boltAngles[0] = -boltMatrix.matrix[0][1];
		boltAngles[1] = -boltMatrix.matrix[1][1];
		boltAngles[2] = -boltMatrix.matrix[2][1];

		te = G_TempEntity( boltPoint, EV_SABER_HIT );
		te->s.otherEntityNum = self->s.number;
		te->s.otherEntityNum2 = ENTITYNUM_NONE;
		te->s.weapon = 0;//saberNum
		te->s.legsAnim = 0;//bladeNum

		VectorCopy(boltPoint, te->s.origin);
		VectorCopy(boltAngles, te->s.angles);

		if (!te->s.angles[0] && !te->s.angles[1] && !te->s.angles[2])
		{ //don't let it play with no direction
			te->s.angles[1] = 1;
		}

		te->s.eventParm = 16; //lots of sparks
	}
}

void LimbTouch( gentity_t *self, gentity_t *other, trace_t *trace )
{
}

void LimbThink( gentity_t *ent )
{
	float gravity = 3.0f;
	float mass = 0.09f;
	float bounce = 1.3f;

	switch (ent->s.modelGhoul2)
	{
	case G2_MODELPART_HEAD:
		mass = 0.08f;
		bounce = 1.4f;
		break;
	case G2_MODELPART_WAIST:
		mass = 0.1f;
		bounce = 1.2f;
		break;
	case G2_MODELPART_LARM:
	case G2_MODELPART_RARM:
	case G2_MODELPART_RHAND:
	case G2_MODELPART_LLEG:
	case G2_MODELPART_RLEG:
	default:
		break;
	}

	if (ent->speed < level.time)
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	if (ent->genericValue5 <= level.time)
	{ //this will be every frame by standard, but we want to compensate in case sv_fps is not 20.
		G_RunExPhys(ent, gravity, mass, bounce, qtrue, NULL, 0);
		ent->genericValue5 = level.time + 50;
	}

	ent->nextthink = level.time;
}

extern qboolean BG_GetRootSurfNameWithVariant( void *ghoul2, const char *rootSurfName, char *returnSurfName, int returnSize );

void G_Dismember( gentity_t *ent, gentity_t *enemy, vec3_t point, int limbType, float limbRollBase, float limbPitchBase, int deathAnim, qboolean postDeath )
{
	vec3_t	newPoint, dir, vel;
	gentity_t *limb;
	char	limbName[MAX_QPATH];
	char	stubName[MAX_QPATH];
	char	stubCapName[MAX_QPATH];

	if (limbType == G2_MODELPART_HEAD)
	{
		Q_strncpyz( limbName , "head", sizeof( limbName  ) );
		Q_strncpyz( stubCapName, "torso_cap_head", sizeof( stubCapName ) );
	}
	else if (limbType == G2_MODELPART_WAIST)
	{
		Q_strncpyz( limbName, "torso", sizeof( limbName ) );
		Q_strncpyz( stubCapName, "hips_cap_torso", sizeof( stubCapName ) );
	}
	else if (limbType == G2_MODELPART_LARM)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "l_arm", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "torso", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_l_arm", stubName );
	}
	else if (limbType == G2_MODELPART_RARM)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_arm", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "torso", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_arm", stubName );
	}
	else if (limbType == G2_MODELPART_RHAND)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_hand", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_arm", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_hand", stubName );
	}
	else if (limbType == G2_MODELPART_LLEG)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "l_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_l_leg", stubName );
	}
	else if (limbType == G2_MODELPART_RLEG)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_leg", stubName );
	}
	else
	{//umm... just default to the right leg, I guess (same as on client)
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_leg", stubName );
	}

	if (ent->ghoul2 && limbName[0] && trap->G2API_GetSurfaceRenderStatus(ent->ghoul2, 0, limbName))
	{ //is it already off? If so there's no reason to be doing it again, so get out of here.
		return;
	}

	VectorCopy( point, newPoint );
	limb = G_Spawn();
	limb->classname = "playerlimb";

	/*
	if (limbType == G2_MODELPART_WAIST)
	{ //slight hack
		newPoint[2] += 1;
	}
	*/

	G_SetOrigin( limb, newPoint );
	VectorCopy( newPoint, limb->s.pos.trBase );
	limb->think = LimbThink;
	limb->touch = LimbTouch;
	limb->speed = level.time + Q_irand(8000, 16000);
	limb->nextthink = level.time + FRAMETIME;

	limb->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	limb->clipmask = MASK_SOLID;
	limb->r.contents = CONTENTS_TRIGGER;
	limb->physicsObject = qtrue;
	VectorSet( limb->r.mins, -6.0f, -6.0f, -3.0f );
	VectorSet( limb->r.maxs, 6.0f, 6.0f, 6.0f );

	limb->s.g2radius = 200;

	limb->s.eType = ET_GENERAL;
	limb->s.weapon = G2_MODEL_PART;
	limb->s.modelGhoul2 = limbType;
	limb->s.modelindex = ent->s.number;
	if (!ent->client)
	{
		limb->s.modelindex = -1;
		limb->s.otherEntityNum2 = ent->s.number;
	}

	VectorClear(limb->s.apos.trDelta);

	if (ent->client)
	{
		VectorCopy(ent->client->ps.viewangles, limb->r.currentAngles);
		VectorCopy(ent->client->ps.viewangles, limb->s.apos.trBase);
	}
	else
	{
		VectorCopy(ent->r.currentAngles, limb->r.currentAngles);
		VectorCopy(ent->r.currentAngles, limb->s.apos.trBase);
	}

	//Set up the ExPhys values for the entity.
	limb->epGravFactor = 0;
	VectorClear(limb->epVelocity);
	VectorSubtract( point, ent->r.currentOrigin, dir );
	VectorNormalize( dir );
	if (ent->client)
	{
		VectorCopy(ent->client->ps.velocity, vel);
	}
	else
	{
		VectorCopy(ent->s.pos.trDelta, vel);
	}
	VectorMA( vel, 80, dir, limb->epVelocity );

	//add some vertical velocity
	if (limbType == G2_MODELPART_HEAD ||
		limbType == G2_MODELPART_WAIST)
	{
		limb->epVelocity[2] += 10;
	}

	if (enemy && enemy->client && ent && ent != enemy && ent->s.number != enemy->s.number &&
		enemy->client->ps.weapon == WP_SABER && enemy->client->olderIsValid &&
		(level.time - enemy->client->lastSaberStorageTime) < 200)
	{ //The enemy has valid saber positions between this and last frame. Use them to factor in direction of the limb.
		vec3_t dif;
		float totalDistance;
		const float distScale = 1.2f;

		//scale down the initial velocity first, which is based on the speed of the limb owner.
		//ExPhys object velocity operates on a slightly different scale than Q3-based physics velocity.
		VectorScale(limb->epVelocity, 0.4f, limb->epVelocity);

		VectorSubtract(enemy->client->lastSaberBase_Always, enemy->client->olderSaberBase, dif);
		totalDistance = VectorNormalize(dif);

		VectorScale(dif, totalDistance*distScale, dif);
		VectorAdd(limb->epVelocity, dif, limb->epVelocity);

		if (ent->client && (ent->client->ps.torsoTimer > 0 || !BG_InDeathAnim(ent->client->ps.torsoAnim)))
		{ //if he's done with his death anim we don't actually want the limbs going far
			vec3_t preVel;

			VectorCopy(limb->epVelocity, preVel);
			preVel[2] = 0;
			totalDistance = VectorNormalize(preVel);

			if (totalDistance < 40.0f)
			{
				float mAmt = 40.0f;//60.0f/totalDistance;

				limb->epVelocity[0] = preVel[0]*mAmt;
				limb->epVelocity[1] = preVel[1]*mAmt;
			}
		}
		else if (ent->client)
		{
			VectorScale(limb->epVelocity, 0.3f, limb->epVelocity);
		}
	}

	if (ent->s.eType == ET_NPC && ent->ghoul2 && limbName[0] && stubCapName[0])
	{ //if it's an npc remove these surfs on the server too. For players we don't even care cause there's no further dismemberment after death.
		trap->G2API_SetSurfaceOnOff(ent->ghoul2, limbName, 0x00000100);
		trap->G2API_SetSurfaceOnOff(ent->ghoul2, stubCapName, 0);
	}

	if ( level.gametype >= GT_TEAM && ent->s.eType != ET_NPC )
	{//Team game
		switch ( ent->client->sess.sessionTeam )
		{
		case TEAM_RED:
			limb->s.customRGBA[0] = 255;
			limb->s.customRGBA[1] = 0;
			limb->s.customRGBA[2] = 0;
			break;

		case TEAM_BLUE:
			limb->s.customRGBA[0] = 0;
			limb->s.customRGBA[1] = 0;
			limb->s.customRGBA[2] = 255;
			break;

		default:
			limb->s.customRGBA[0] = ent->s.customRGBA[0];
			limb->s.customRGBA[1] = ent->s.customRGBA[1];
			limb->s.customRGBA[2] = ent->s.customRGBA[2];
			limb->s.customRGBA[3] = ent->s.customRGBA[3];
			break;
		}
	}
	else
	{//FFA
		limb->s.customRGBA[0] = ent->s.customRGBA[0];
		limb->s.customRGBA[1] = ent->s.customRGBA[1];
		limb->s.customRGBA[2] = ent->s.customRGBA[2];
		limb->s.customRGBA[3] = ent->s.customRGBA[3];
	}

	trap->LinkEntity( (sharedEntity_t *)limb );
}

void DismembermentTest(gentity_t *self)
{
	int sect = G2_MODELPART_HEAD;
	vec3_t boltPoint;

	while (sect <= G2_MODELPART_RLEG)
	{
		G_GetDismemberBolt(self, boltPoint, sect);
		G_Dismember( self, self, boltPoint, sect, 90, 0, BOTH_DEATH1, qfalse );
		sect++;
	}
}

void DismembermentByNum(gentity_t *self, int num)
{
	int sect = G2_MODELPART_HEAD;
	vec3_t boltPoint;

	switch (num)
	{
	case 0:
		sect = G2_MODELPART_HEAD;
		break;
	case 1:
		sect = G2_MODELPART_WAIST;
		break;
	case 2:
		sect = G2_MODELPART_LARM;
		break;
	case 3:
		sect = G2_MODELPART_RARM;
		break;
	case 4:
		sect = G2_MODELPART_RHAND;
		break;
	case 5:
		sect = G2_MODELPART_LLEG;
		break;
	case 6:
		sect = G2_MODELPART_RLEG;
		break;
	default:
		break;
	}

	G_GetDismemberBolt(self, boltPoint, sect);
	G_Dismember( self, self, boltPoint, sect, 90, 0, BOTH_DEATH1, qfalse );
}

int G_GetHitQuad( gentity_t *self, vec3_t hitloc )
{
	vec3_t diff, fwdangles={0,0,0}, right;
	vec3_t clEye;
	float rightdot;
	float zdiff;
	int hitLoc = gPainHitLoc;

	if (self->client)
	{
		VectorCopy(self->client->ps.origin, clEye);
		clEye[2] += self->client->ps.viewheight;
	}
	else
	{
		VectorCopy(self->s.pos.trBase, clEye);
		clEye[2] += 16;
	}

	VectorSubtract( hitloc, clEye, diff );
	diff[2] = 0;
	VectorNormalize( diff );

	if (self->client)
	{
		fwdangles[1] = self->client->ps.viewangles[1];
	}
	else
	{
		fwdangles[1] = self->s.apos.trBase[1];
	}
	// Ultimately we might care if the shot was ahead or behind, but for now, just quadrant is fine.
	AngleVectors( fwdangles, NULL, right, NULL );

	rightdot = DotProduct(right, diff);
	zdiff = hitloc[2] - clEye[2];

	if ( zdiff > 0 )
	{
		if ( rightdot > 0.3 )
		{
			hitLoc = G2_MODELPART_RARM;
		}
		else if ( rightdot < -0.3 )
		{
			hitLoc = G2_MODELPART_LARM;
		}
		else
		{
			hitLoc = G2_MODELPART_HEAD;
		}
	}
	else if ( zdiff > -20 )
	{
		if ( rightdot > 0.1 )
		{
			hitLoc = G2_MODELPART_RARM;
		}
		else if ( rightdot < -0.1 )
		{
			hitLoc = G2_MODELPART_LARM;
		}
		else
		{
			hitLoc = G2_MODELPART_HEAD;
		}
	}
	else
	{
		if ( rightdot >= 0 )
		{
			hitLoc = G2_MODELPART_RLEG;
		}
		else
		{
			hitLoc = G2_MODELPART_LLEG;
		}
	}

	return hitLoc;
}

int gGAvoidDismember = 0;

void UpdateClientRenderBolts(gentity_t *self, vec3_t renderOrigin, vec3_t renderAngles);

qboolean G_GetHitLocFromSurfName( gentity_t *ent, const char *surfName, int *hitLoc, vec3_t point, vec3_t dir, vec3_t bladeDir, int mod )
{
	qboolean dismember = qfalse;
	int actualTime;
	int kneeLBolt = -1;
	int kneeRBolt = -1;
	int handRBolt = -1;
	int handLBolt = -1;
	int footRBolt = -1;
	int footLBolt = -1;

	*hitLoc = HL_NONE;

	if ( !surfName || !surfName[0] )
	{
		return qfalse;
	}

	if( !ent->client )
	{
		return qfalse;
	}

	if (!point)
	{
		return qfalse;
	}

	if ( ent->client
		&& ( ent->client->NPC_class == CLASS_R2D2
			|| ent->client->NPC_class == CLASS_R5D2
			|| ent->client->NPC_class == CLASS_GONK
			|| ent->client->NPC_class == CLASS_MOUSE
			|| ent->client->NPC_class == CLASS_SENTRY
			|| ent->client->NPC_class == CLASS_INTERROGATOR
			|| ent->client->NPC_class == CLASS_PROBE ) )
	{//we don't care about per-surface hit-locations or dismemberment for these guys
		return qfalse;
	}

	if (ent->localAnimIndex <= 1)
	{ //humanoid
		handLBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*l_hand");
		handRBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
		kneeLBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*hips_l_knee");
		kneeRBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*hips_r_knee");
		footLBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
		footRBolt = trap->G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
	}

	if ( ent->client && (ent->client->NPC_class == CLASS_ATST) )
	{
		//FIXME: almost impossible to hit these... perhaps we should
		//		check for splashDamage and do radius damage to these parts?
		//		Or, if we ever get bbox G2 traces, that may fix it, too
		if (!Q_stricmp("head_light_blaster_cann",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("head_concussion_charger",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK1) )
	{
		if (!Q_stricmp("l_arm",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("r_arm",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		else if (!Q_stricmp("torso_front",surfName))
		{
			*hitLoc = HL_CHEST;
		}
		else if (!Q_stricmp("torso_tube1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_tube2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_tube3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		else if (!Q_stricmp("torso_tube4",surfName))
		{
			*hitLoc = HL_GENERIC4;
		}
		else if (!Q_stricmp("torso_tube5",surfName))
		{
			*hitLoc = HL_GENERIC5;
		}
		else if (!Q_stricmp("torso_tube6",surfName))
		{
			*hitLoc = HL_GENERIC6;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK2) )
	{
		if (!Q_stricmp("torso_canister1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_canister2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_canister3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_GALAKMECH) )
	{
		if (!Q_stricmp("torso_antenna",surfName)||!Q_stricmp("torso_antenna_base",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_shield",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else
		{
			*hitLoc = HL_CHEST;
		}
		return(qfalse);
	}

	//FIXME: check the hitLoc and hitDir against the cap tag for the place
	//where the split will be- if the hit dir is roughly perpendicular to
	//the direction of the cap, then the split is allowed, otherwise we
	//hit it at the wrong angle and should not dismember...
	actualTime = level.time;
	if ( !Q_strncmp( "hips", surfName, 4 ) )
	{//FIXME: test properly for legs
		*hitLoc = HL_WAIST;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (kneeLBolt>=0)
			{
				trap->G2API_GetBoltMatrix( ent->ghoul2, 0, kneeLBolt,
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the knee
					*hitLoc = HL_LEG_LT;
				}
			}
			if (*hitLoc == HL_WAIST)
			{
				if (kneeRBolt>=0)
				{
					trap->G2API_GetBoltMatrix( ent->ghoul2, 0, kneeRBolt,
									&boltMatrix, angles, ent->r.currentOrigin,
									actualTime, NULL, ent->modelScale );
					BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
					if ( DistanceSquared( point, tagOrg ) < 100 )
					{//actually hit the knee
						*hitLoc = HL_LEG_RT;
					}
				}
			}
		}
	}
	else if ( !Q_strncmp( "torso", surfName, 5 ) )
	{
		if ( !ent->client )
		{
			*hitLoc = HL_CHEST;
		}
		else
		{
			vec3_t	t_fwd, t_rt, t_up, dirToImpact;
			float frontSide, rightSide, upSide;
			AngleVectors( ent->client->renderInfo.torsoAngles, t_fwd, t_rt, t_up );

			if (ent->client->renderInfo.boltValidityTime != level.time)
			{
				vec3_t renderAng;

				renderAng[0] = 0;
				renderAng[1] = ent->client->ps.viewangles[YAW];
				renderAng[2] = 0;

				UpdateClientRenderBolts(ent, ent->client->ps.origin, renderAng);
			}

			VectorSubtract( point, ent->client->renderInfo.torsoPoint, dirToImpact );
			frontSide = DotProduct( t_fwd, dirToImpact );
			rightSide = DotProduct( t_rt, dirToImpact );
			upSide = DotProduct( t_up, dirToImpact );
			if ( upSide < -10 )
			{//hit at waist
				*hitLoc = HL_WAIST;
			}
			else
			{//hit on upper torso
				if ( rightSide > 4 )
				{
					*hitLoc = HL_ARM_RT;
				}
				else if ( rightSide < -4 )
				{
					*hitLoc = HL_ARM_LT;
				}
				else if ( rightSide > 2 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_RT;
					}
					else
					{
						*hitLoc = HL_BACK_RT;
					}
				}
				else if ( rightSide < -2 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_LT;
					}
					else
					{
						*hitLoc = HL_BACK_LT;
					}
				}
				else if ( upSide > -3 && mod == MOD_SABER )
				{
					*hitLoc = HL_HEAD;
				}
				else if ( frontSide > 0 )
				{
					*hitLoc = HL_CHEST;
				}
				else
				{
					*hitLoc = HL_BACK;
				}
			}
		}
	}
	else if ( !Q_strncmp( "head", surfName, 4 ) )
	{
		*hitLoc = HL_HEAD;
	}
	else if ( !Q_strncmp( "r_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_RT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (handRBolt>=0)
			{
				trap->G2API_GetBoltMatrix( ent->ghoul2, 0, handRBolt,
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 256 )
				{//actually hit the hand
					*hitLoc = HL_HAND_RT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "l_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_LT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (handLBolt>=0)
			{
				trap->G2API_GetBoltMatrix( ent->ghoul2, 0, handLBolt,
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 256 )
				{//actually hit the hand
					*hitLoc = HL_HAND_LT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "r_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_RT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (footRBolt>=0)
			{
				trap->G2API_GetBoltMatrix( ent->ghoul2, 0, footRBolt,
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the foot
					*hitLoc = HL_FOOT_RT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "l_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_LT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (footLBolt>=0)
			{
				trap->G2API_GetBoltMatrix( ent->ghoul2, 0, footLBolt,
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the foot
					*hitLoc = HL_FOOT_LT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "r_hand", surfName, 6 ) || !Q_strncmp( "w_", surfName, 2 ) )
	{//right hand or weapon
		*hitLoc = HL_HAND_RT;
	}
	else if ( !Q_strncmp( "l_hand", surfName, 6 ) )
	{
		*hitLoc = HL_HAND_LT;
	}
	/*
#ifdef _DEBUG
	else
	{
		Com_Printf( "ERROR: surface %s does not belong to any hitLocation!!!\n", surfName );
	}
#endif //_DEBUG
	*/

	//if ( g_dismemberment->integer >= 11381138 || !ent->client->dismembered )
	if (g_dismember.integer == 100)
	{ //full probability...
		if ( ent->client && ent->client->NPC_class == CLASS_PROTOCOL )
		{
			dismember = qtrue;
		}
		else if ( dir && (dir[0] || dir[1] || dir[2]) &&
			bladeDir && (bladeDir[0] || bladeDir[1] || bladeDir[2]) )
		{//we care about direction (presumably for dismemberment)
			//if ( g_dismemberProbabilities->value<=0.0f||G_Dismemberable( ent, *hitLoc ) )
			if (1) //Fix me?
			{//either we don't care about probabilties or the probability let us continue
				char *tagName = NULL;
				float	aoa = 0.5f;
				//dir must be roughly perpendicular to the hitLoc's cap bolt
				switch ( *hitLoc )
				{
					case HL_LEG_RT:
						tagName = "*hips_cap_r_leg";
						break;
					case HL_LEG_LT:
						tagName = "*hips_cap_l_leg";
						break;
					case HL_WAIST:
						tagName = "*hips_cap_torso";
						aoa = 0.25f;
						break;
					case HL_CHEST_RT:
					case HL_ARM_RT:
					case HL_BACK_LT:
						tagName = "*torso_cap_r_arm";
						break;
					case HL_CHEST_LT:
					case HL_ARM_LT:
					case HL_BACK_RT:
						tagName = "*torso_cap_l_arm";
						break;
					case HL_HAND_RT:
						tagName = "*r_arm_cap_r_hand";
						break;
					case HL_HAND_LT:
						tagName = "*l_arm_cap_l_hand";
						break;
					case HL_HEAD:
						tagName = "*torso_cap_head";
						aoa = 0.25f;
						break;
					case HL_CHEST:
					case HL_BACK:
					case HL_FOOT_RT:
					case HL_FOOT_LT:
					default:
						//no dismemberment possible with these, so no checks needed
						break;
				}
				if ( tagName )
				{
					int tagBolt = trap->G2API_AddBolt( ent->ghoul2, 0, tagName );
					if ( tagBolt != -1 )
					{
						mdxaBone_t	boltMatrix;
						vec3_t	tagOrg, tagDir, angles;

						VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
						trap->G2API_GetBoltMatrix( ent->ghoul2, 0, tagBolt,
										&boltMatrix, angles, ent->r.currentOrigin,
										actualTime, NULL, ent->modelScale );
						BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
						BG_GiveMeVectorFromMatrix( &boltMatrix, NEGATIVE_Y, tagDir );
						if ( DistanceSquared( point, tagOrg ) < 256 )
						{//hit close
							float dot = DotProduct( dir, tagDir );
							if ( dot < aoa && dot > -aoa )
							{//hit roughly perpendicular
								dot = DotProduct( bladeDir, tagDir );
								if ( dot < aoa && dot > -aoa )
								{//blade was roughly perpendicular
									dismember = qtrue;
								}
							}
						}
					}
				}
			}
		}
		else
		{ //hmm, no direction supplied.
			dismember = qtrue;
		}
	}
	return dismember;
}

void G_CheckForDismemberment(gentity_t *ent, gentity_t *enemy, vec3_t point, int damage, int deathAnim, qboolean postDeath)
{
	int hitLoc = -1, hitLocUse = -1;
	vec3_t boltPoint;
	int dismember = g_dismember.integer;

	if (ent->localAnimIndex > 1)
	{
		if (!ent->NPC)
		{
			return;
		}

		if (ent->client->NPC_class != CLASS_PROTOCOL)
		{ //this is the only non-humanoid allowed to do dismemberment.
			return;
		}
	}

	if (!dismember)
	{
		return;
	}

	if (gGAvoidDismember == 1)
	{
		return;
	}

	if (gGAvoidDismember != 2)
	{ //this means do the dismemberment regardless of randomness and damage
		if (Q_irand(0, 100) > dismember)
		{
			return;
		}

		if (damage < 5)
		{
			return;
		}
	}

	if (gGAvoidDismember == 2)
	{
		hitLoc = HL_HAND_RT;
	}
	else
	{
		if (d_saberGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time)
		{
			char hitSurface[MAX_QPATH];

			trap->G2API_GetSurfaceName(ent->ghoul2, ent->client->g2LastSurfaceHit, 0, hitSurface);

			if (hitSurface[0])
			{
				G_GetHitLocFromSurfName(ent, hitSurface, &hitLoc, point, vec3_origin, vec3_origin, MOD_UNKNOWN);
			}
		}

		if (hitLoc == -1)
		{
			hitLoc = G_GetHitLocation( ent, point );
		}
	}

	switch(hitLoc)
	{
	case HL_FOOT_RT:
	case HL_LEG_RT:
		hitLocUse = G2_MODELPART_RLEG;
		break;
	case HL_FOOT_LT:
	case HL_LEG_LT:
		hitLocUse = G2_MODELPART_LLEG;
		break;

	case HL_WAIST:
		hitLocUse = G2_MODELPART_WAIST;
		break;
		/*
	case HL_BACK_RT:
	case HL_BACK_LT:
	case HL_BACK:
	case HL_CHEST_RT:
	case HL_CHEST_LT:
	case HL_CHEST:
		break;
		*/
	case HL_ARM_RT:
		hitLocUse = G2_MODELPART_RARM;
		break;
	case HL_HAND_RT:
		hitLocUse = G2_MODELPART_RHAND;
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		hitLocUse = G2_MODELPART_LARM;
		break;
	case HL_HEAD:
		hitLocUse = G2_MODELPART_HEAD;
		break;
	default:
		hitLocUse = G_GetHitQuad(ent, point);
		break;
	}

	if (hitLocUse == -1)
	{
		return;
	}

	if (ent->client)
	{
		G_GetDismemberBolt(ent, boltPoint, hitLocUse);
		if ( g_austrian.integer
			&& (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) )
		{
			G_LogPrintf( "Duel Dismemberment: %s dismembered at %s\n", ent->client->pers.netname, hitLocName[hitLoc] );
		}
	}
	else
	{
		G_GetDismemberLoc(ent, boltPoint, hitLocUse);
	}
	G_Dismember(ent, enemy, boltPoint, hitLocUse, 90, 0, deathAnim, postDeath);
}

void G_LocationBasedDamageModifier(gentity_t *ent, vec3_t point, int mod, int dflags, int *damage)
{
	int hitLoc = -1;

	if (!g_locationBasedDamage.integer)
	{ //then leave it alone
		return;
	}

	if ( (dflags&DAMAGE_NO_HIT_LOC) )
	{ //then leave it alone
		return;
	}

	if (mod == MOD_SABER && *damage <= 1)
	{ //don't bother for idle damage
		return;
	}

	if (!point)
	{
		return;
	}

	if ( ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{//no location-based damage on vehicles
		return;
	}

	if ((d_saberGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time && mod == MOD_SABER) || //using ghoul2 collision? Then if the mod is a saber we should have surface data from the last hit (unless thrown).
		(d_projectileGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time)) //It's safe to assume we died from the projectile that just set our surface index. So, go ahead and use that as the surf I guess.
	{
		char hitSurface[MAX_QPATH];

		trap->G2API_GetSurfaceName(ent->ghoul2, ent->client->g2LastSurfaceHit, 0, hitSurface);

		if (hitSurface[0])
		{
			G_GetHitLocFromSurfName(ent, hitSurface, &hitLoc, point, vec3_origin, vec3_origin, MOD_UNKNOWN);
		}
	}

	if (hitLoc == -1)
	{
		hitLoc = G_GetHitLocation( ent, point );
	}

	switch (hitLoc)
	{
	case HL_FOOT_RT:
	case HL_FOOT_LT:
		*damage *= 0.5;
		break;
	case HL_LEG_RT:
	case HL_LEG_LT:
		*damage *= 0.7;
		break;
	case HL_WAIST:
	case HL_BACK_RT:
	case HL_BACK_LT:
	case HL_BACK:
	case HL_CHEST_RT:
	case HL_CHEST_LT:
	case HL_CHEST:
		break; //normal damage
	case HL_ARM_RT:
	case HL_ARM_LT:
		*damage *= 0.85;
		break;
	case HL_HAND_RT:
	case HL_HAND_LT:
		*damage *= 0.6;
		break;
	case HL_HEAD:
		*damage *= 1.3;
		break;
	default:
		break; //do nothing then
	}
}
/*
===================================
rww - end dismemberment/lbd
===================================
*/

qboolean G_ThereIsAMaster(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->client && ent->client->ps.isJediMaster)
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

void G_Knockdown( gentity_t *victim )
{
	if ( victim && victim->client && BG_KnockDownable(&victim->client->ps) )
	{
		victim->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		victim->client->ps.forceDodgeAnim = 0;
		victim->client->ps.forceHandExtendTime = level.time + 1100;
		victim->client->ps.quickerGetup = qfalse;
	}
}

// zyk: tests if this rpg player can damage saber-only damage things
qboolean zyk_can_damage_saber_only_entities(gentity_t *attacker, gentity_t *inflictor, int mod)
{
	if (attacker && attacker->client && attacker->client->sess.amrpgmode == 2)
	{
		if ((mod == MOD_ROCKET || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING_SPLASH) && 
			attacker->client->pers.skill_levels[26] == 2)
		{
			return qtrue;
		}
	
		if ((mod == MOD_CONC || mod == MOD_CONC_ALT) && attacker->client->pers.skill_levels[27] == 2)
		{
			return qtrue;
		}

		if (mod == MOD_MELEE && attacker->client->pers.rpg_class == 4)
		{ // zyk: Monk melee
			return qtrue;
		}

		if (mod == MOD_MELEE && attacker->client->pers.rpg_class == 8 &&
			inflictor && inflictor->s.weapon == WP_CONCUSSION)
		{ // zyk: Magic Master bolts, the Ultra Bolt
			return qtrue;
		}

		if (mod == MOD_DET_PACK_SPLASH && attacker->client->pers.secrets_found & (1 << 14))
		{ // zyk: detpacks
			return qtrue;
		}
	}

	return qfalse;
}

/*
============
G_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how G_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
	DAMAGE_HALF_ABSORB		half shields, half health
	DAMAGE_HALF_ARMOR_REDUCTION		Any damage that shields incur is halved
============
*/
extern qboolean gSiegeRoundBegun;

int gPainMOD = 0;
int gPainHitLoc = -1;
vec3_t gPainPoint;

extern void Jedi_Decloak( gentity_t *self );
extern void Boba_FlyStop( gentity_t *self );
extern qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target);
void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod ) {
	gclient_t	*client;
	int			take, asave = 0, knockback;
	float		shieldAbsorbed = 0;
	int			check_shield = 1; // zyk: tests if damage can be absorbed by shields
	qboolean	can_damage_heavy_things = qfalse; // zyk: will be qtrue if attacker is a RPG Mode Monk using melee or a Magic Master using Magic Fist

	if (!targ)
		return;

	// zyk: allies will not receive damage from attacker
	if (attacker && attacker->client && targ && targ->client)
	{
		if (zyk_is_ally(attacker,targ) == qtrue)
		{
			return;
		}
	}

	if (zyk_can_hit_target(attacker, targ) == qfalse)
	{
		return;
	}

	// zyk: players in Duel Tournament cant be hit by anything outside it
	if (level.duel_tournament_mode == 4 && attacker && targ && duel_tournament_is_duelist(attacker) == qfalse && duel_tournament_is_duelist(targ) == qtrue)
	{
		return;
	}

	// zyk: if attacker has nofight, cannot damage sentries
	if (attacker && attacker->client && !attacker->NPC && attacker->client->pers.player_statuses & (1 << 26) && targ && Q_stricmp(targ->classname, "sentryGun") == 0 && 
		(!targ->parent || targ->parent != attacker))
	{
		return;
	}

	// zyk: target has chat protection
	if (targ && targ->client && !targ->NPC && targ->client->pers.player_statuses & (1 << 5))
		return;

	// zyk: target has been paralyzed by an admin
	if (targ && targ->client && !targ->NPC && targ->client->pers.player_statuses & (1 << 6))
		return;

	// zyk: players with noclip cannot damage
	if (attacker && attacker->client && attacker->s.number < MAX_CLIENTS && attacker->client->noclip == qtrue)
		return;

	// zyk: Race Mode. Players in the race waiting for it to start cannot be hit or hit anyone
	if (level.race_mode > 0 && level.race_mode < 3 && attacker && attacker->client && targ && targ->client && 
		((attacker->client->pers.race_position > 0) || 
		 (attacker->client->pers.race_position == 0 && targ->client->pers.race_position > 0)))
	{
		return;
	}

	if (targ && targ->client && targ->NPC && targ->health <= 0 && targ->client->ps.eFlags & EF_DISINTEGRATION)
	{ // zyk: bug fix. If this npc was desintegrated, do not damage it again
		return;
	}

	if (attacker && attacker->client && attacker->client->sess.amrpgmode == 2)
	{
		if (mod == MOD_SABER)
		{ // zyk: player in RPG mode, with duals or staff, has a better damage depending on Saber Attack level
			if (attacker->client->saber[0].saberFlags&SFL_TWO_HANDED || (attacker->client->saber[0].model[0] && attacker->client->saber[1].model[0]))
			{
				if (attacker->client->pers.skill_levels[5] <= FORCE_LEVEL_1)
				{
					damage = (int)ceil(damage*0.2);
				}
				else if (attacker->client->pers.skill_levels[5] == FORCE_LEVEL_2)
				{
					damage = (int)ceil(damage*0.4);
				}
				else if (attacker->client->pers.skill_levels[5] == FORCE_LEVEL_3)
				{
					damage = (int)ceil(damage*0.6);
				}
				else if (attacker->client->pers.skill_levels[5] == FORCE_LEVEL_4)
				{
					damage = (int)ceil(damage*0.8);
				}
			}
		}
		else if (attacker->client->pers.skill_levels[24] == 2 && (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT))
		{ // zyk: DEMP2 2/2 in RPG Mode causes more damage
			damage = (int)ceil(damage * 1.12);
		}
		else if (attacker->client->pers.skill_levels[25] == 2 && (mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH))
		{ // zyk: Flechette 2/2 in RPG Mode causes more damage
			damage = (int)ceil(damage * 1.12);
		}
		else if (attacker->client->pers.skill_levels[27] == 2 && (mod == MOD_CONC || mod == MOD_CONC_ALT))
		{ // zyk: Concussion Rifle 2/2 in RPG Mode causes more damage
			damage = (int)ceil(damage * 1.12);
		}
		else if (mod == MOD_MELEE)
		{ // zyk: setting melee damage in RPG Mode
			if (attacker->client->pers.skill_levels[29] == 0)
				damage = (int)ceil((damage * 1.0) / 2.0);
			else if (attacker->client->pers.skill_levels[29] == 2)
				damage = damage * 2;
			else if (attacker->client->pers.skill_levels[29] == 3)
				damage = damage * 3;
		}
	}

	// zyk: force Rage increases damage of attacks
	if (attacker && attacker->client && attacker->client->ps.fd.forcePowersActive & (1 << FP_RAGE))
	{ // zyk: new Force Rage code
		if (attacker->client->ps.fd.forcePowerLevel[FP_RAGE] == 1)
			damage = (int)ceil(damage * 1.03);
		else if (attacker->client->ps.fd.forcePowerLevel[FP_RAGE] == 2)
			damage = (int)ceil(damage * 1.06);
		else if (attacker->client->ps.fd.forcePowerLevel[FP_RAGE] == 3)
			damage = (int)ceil(damage * 1.09);
	}

	if (attacker && attacker->client && (attacker->NPC || attacker->client->sess.amrpgmode == 2) && attacker->client->pers.quest_power_status & (1 << 3))
	{ // zyk: Ultra Strength bonus damage
		// zyk: Universe Power
		if (attacker->client->pers.quest_power_status & (1 << 13))
			damage = (int)ceil(damage * 1.12);
		else
			damage = (int)ceil(damage * 1.08);
	}

	if (attacker && attacker->client && attacker->client->sess.amrpgmode == 2)
	{ // zyk: bonus damage of each RPG class
		if (attacker->client->pers.rpg_class == 0)
		{ // zyk: Free Warrior
			damage = (int)ceil(damage * (1.0 + (0.03 * attacker->client->pers.skill_levels[55])));
		}
		else if (attacker->client->pers.rpg_class == 1 && (mod == MOD_SABER || mod == MOD_FORCE_DARK))
		{ // zyk: Force User
			damage = (int)ceil(damage * (1.0 + (0.05 * attacker->client->pers.skill_levels[55])));
		}
		else if (attacker->client->pers.rpg_class == 2 && mod != MOD_SABER && mod != MOD_MELEE && mod != MOD_FORCE_DARK)
		{ // zyk: Bounty Hunter
			damage = (int)ceil(damage * (1.0 + (0.05 * attacker->client->pers.skill_levels[55])));
		}
		else if (attacker->client->pers.rpg_class == 4 && mod == MOD_MELEE)
		{ // zyk: Monk
			damage = damage * (1.0 + (attacker->client->pers.skill_levels[55]*0.5));
			can_damage_heavy_things = qtrue;
		}
		else if (attacker->client->pers.rpg_class == 5 && (mod == MOD_STUN_BATON || mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SNIPER || 
			     mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH || mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT || 
				 mod == MOD_LAVA || mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH || mod == MOD_DET_PACK_SPLASH || 
				 mod == MOD_CONC || mod == MOD_CONC_ALT || mod == MOD_DISRUPTOR_SPLASH))
		{ // zyk: Stealth Attacker has more gun damage
			float stealth_attacker_bonus_damage = 0.0;

			// zyk: Stealth Attacker Upgrade increases damage
			if (attacker->client->pers.secrets_found & (1 << 7))
				stealth_attacker_bonus_damage = 0.2;

			damage = (int)ceil(damage * (1.05 + (0.15 * attacker->client->pers.skill_levels[55]) + stealth_attacker_bonus_damage));
		}
		else if (attacker->client->pers.rpg_class == 6 && (mod == MOD_SABER || mod == MOD_MELEE))
		{ // zyk: Duelist has higher damage in saber and melee
			damage = (int)ceil(damage * (1.2 + (0.2 * attacker->client->pers.skill_levels[55])));
		}
		else if (attacker->client->pers.rpg_class == 7)
		{ // zyk: Force Gunner bonus damage
			damage = (int)ceil(damage * (1.0 + (0.06 * attacker->client->pers.skill_levels[55])));
		}
		else if (attacker->client->pers.rpg_class == 8 && mod == MOD_MELEE)
		{ // zyk: Magic Master bolts can damage heavy things
			if (inflictor && (inflictor->s.weapon == WP_BOWCASTER || inflictor->s.weapon == WP_DEMP2 || inflictor->s.weapon == WP_CONCUSSION))
				can_damage_heavy_things = qtrue;
		}
	}

	if (attacker && attacker->client && (attacker->NPC || attacker->client->sess.amrpgmode == 2) && attacker->client->pers.quest_power_status & (1 << 15))
	{ // zyk: Dark Power increases damage of every attack
		damage = (int)ceil(damage*1.1);
	}

	if (attacker && attacker->client && attacker->client->pers.quest_power_status & (1 << 21))
	{ // zyk: Enemy Weakening decreases damage
		damage = (int)ceil(damage*0.92);
	}

	if (level.gametype == GT_SIEGE)
	{
		damage = (int)ceil(damage*zyk_scale_siege_damage.value);
	}

	if (attacker && attacker->client && attacker->NPC && attacker->client->pers.guardian_invoked_by_id != -1)
	{ // zyk: attacker is a RPG boss. Increase damage based in the number of allies of the quest player
		gentity_t *quest_player_ent = &g_entities[attacker->client->pers.guardian_invoked_by_id];

		if (quest_player_ent && quest_player_ent->client && quest_player_ent->client->sess.amrpgmode == 2 && 
			quest_player_ent->client->pers.universe_quest_counter & (1 << 29))
		{ // zyk: Challenge Mode increases more damage
			damage += ((int)ceil(damage * 0.08 * (1 + zyk_number_of_allies(quest_player_ent, qtrue))));
		}
		else
		{
			damage += ((int)ceil(damage * 0.04 * zyk_number_of_allies(quest_player_ent, qtrue)));
		}
	}

	if (targ && targ->client && targ->NPC && targ->client->pers.guardian_invoked_by_id != -1)
	{ // zyk: targ is a RPG mode boss
		// zyk: chaos power, map entities and telefrag cannot hit the boss
		if (!attacker || !attacker->client || mod == MOD_TELEFRAG)
			return;

		if (targ->client->pers.guardian_invoked_by_id != (attacker-g_entities))
		{			
			if (attacker->client->sess.amrpgmode != 2 || attacker->client->pers.guardian_mode == 0)
				return;
		}

		// zyk: guardians remove cloak of the player when hit by him
		if (attacker->client->ps.powerups[PW_CLOAKED])
		{
			Jedi_Decloak(attacker);
			attacker->client->cloakToggleTime = level.time + Q_irand( 5000, 10000 );
		}
	}

	if (targ && targ->client && targ->NPC)
	{
		if (targ->client->pers.universe_quest_messages == -2000)
		{ // zyk: special npcs spawned in the Universe Quest that cannot be killed
			return;
		}
	}

	if (targ && targ->client && targ->client->sess.amrpgmode == 2 && targ->client->pers.can_play_quest == 1 && 
		targ->client->pers.universe_quest_counter & (1 << 29) && targ->client->pers.guardian_mode == 0)
	{ // zyk: Challenge Mode increases damage taken from anything
		damage = (int)ceil(damage*1.15);
	}

	if (targ && targ->client && (targ->NPC || targ->client->sess.amrpgmode == 2) && targ->client->pers.quest_power_status & (1 << 16))
	{ // zyk: Eternity Power reduces damage of every attack
		damage = (int)ceil(damage*0.9);
	}

	if (targ && targ->client && targ->client->pers.quest_power_status & (1 << 21))
	{ // zyk: Enemy Weakening increases damage taken
		damage = (int)ceil(damage*1.08);
	}

	if (targ && targ->client && targ->client->pers.quest_power_status & (1 << 25))
	{ // zyk: Ice Boulder decreases damage taken
		damage = (int)ceil(damage*0.6);
	}

	if (targ && targ->client && targ->client->pers.quest_power_status & (1 << 26))
	{ // zyk: Elemental Attack Ice decreases damage taken
		damage = (int)ceil(damage*0.4);
	}

	if (targ && targ->client && (targ->NPC || targ->client->sess.amrpgmode == 2) && targ->client->pers.quest_power_status & (1 << 22))
	{ // zyk: Ice Block decreases damage taken
		damage = (int)ceil(damage*0.2);
	}

	// zyk: player or npc with Magic Shield takes little damage
	if (targ && targ->client && (targ->client->sess.amrpgmode == 2 || targ->NPC) && targ->client->pers.quest_power_status & (1 << 11))
	{
		damage = (int)ceil(damage * 0.1);
	}

	// zyk: hit by Time Power. Receive less damage
	if (targ && targ->client && targ->client->pers.quest_power_status & (1 << 2))
	{
		damage = (int)ceil(damage * 0.1);
	}

	if (targ && targ->client && (targ->client->sess.amrpgmode == 2 || targ->NPC) && targ->client->pers.quest_power_status & (1 << 7))
	{ // zyk: Ultra Resistance bonus resistance
		// zyk: Universe Power
		if (targ->client->pers.quest_power_status & (1 << 13))
			damage = (int)ceil(damage * 0.88);
		else
			damage = (int)ceil(damage * 0.92);
	}

	if (targ && targ->client && targ->client->pers.quest_power_status & (1 << 24))
	{ // zyk: target hit by Sleeping Flowers. if he takes damage, he can get up
		targ->client->ps.forceHandExtendTime = level.time;
		targ->client->pers.quest_target9_timer = 0;
	}

	if (targ && targ->client && targ->client->sess.amrpgmode == 2)
	{ // zyk: damage resistance of each class
		if (targ->client->pers.player_statuses & (1 << 8) && mod == MOD_SABER)
		{ // zyk: using the Saber Armor, reduces saber damage
			damage = (int)ceil(damage * 0.85);
		}

		if (targ->client->pers.player_statuses & (1 << 9) && mod != MOD_SABER && mod != MOD_UNKNOWN && mod != MOD_TRIGGER_HURT && 
			mod != MOD_FORCE_DARK && mod != MOD_WATER && mod != MOD_FALLING && mod != MOD_SUICIDE && mod != MOD_TELEFRAG && mod != MOD_SLIME)
		{ // zyk: using the Gun Armor, reduces gun and melee damage
			damage = (int)ceil(damage * 0.85);
		}

		if (targ->client->pers.rpg_class == 1 && targ->client->pers.unique_skill_duration > level.time) // zyk: Force User damage resistance
		{ // zyk: Unique Skill of Force User
			damage = (int)ceil(damage * 0.25);
		}
		else if (targ->client->pers.rpg_class == 3) // zyk: Armored Soldier damage resistance
		{
			float armored_soldier_bonus_resistance = 0.0;
			// zyk: Armored Soldier Upgrade increases damage resistance
			if (targ->client->pers.secrets_found & (1 << 16))
				armored_soldier_bonus_resistance = 0.05;

			// zyk: Armored Soldier Lightning Shield reduces damage
			if (targ->client->ps.powerups[PW_SHIELDHIT] > level.time)
			{
				armored_soldier_bonus_resistance += 0.25;
			}
			
			damage = (int)ceil(damage * (0.9 - ((0.05 * targ->client->pers.skill_levels[55]) + armored_soldier_bonus_resistance)));
		}
		else if (targ->client->pers.rpg_class == 4 && 
				 targ->client->ps.legsAnim == BOTH_MEDITATE)
		{ // zyk: Monk Meditation Strength and Meditation Drain increases resistance to damage of Monk
			if (targ->client->pers.player_statuses & (1 << 21) || targ->client->pers.player_statuses & (1 << 23))
				damage = (int)ceil(damage * (0.5));
		}
		else if (targ->client->pers.rpg_class == 0) // zyk: Free Warrior damage resistance
		{
			// zyk: Free Warrior Mimic Damage ability. Deals half of the damage taken back to the enemy
			if (attacker && attacker != targ && (!attacker->NPC || 
				(attacker->client && (attacker->client->NPC_class != CLASS_RANCOR || !(targ->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)))) &&
				targ->client->pers.unique_skill_duration > level.time && targ->client->pers.player_statuses & (1 << 21))
			{
				if (!(attacker && attacker->client && attacker->client->sess.amrpgmode == 2 && attacker->client->pers.rpg_class == 0 && 
					attacker->client->pers.unique_skill_duration > level.time && attacker->client->pers.player_statuses & (1 << 21)))
				{ // zyk: Mimic Damage will not work if attacker pointer is also a Free Warrior using Mimic Damage
					G_Damage(attacker, targ, targ, NULL, NULL, (int)ceil(damage * 0.5), 0, MOD_UNKNOWN);
				}
			}

			damage = (int)ceil(damage * (1.0 - (0.03 * targ->client->pers.skill_levels[55])));
		}
		else if (targ->client->pers.rpg_class == 5 && (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT))
		{ // zyk: Stealth Attacker damage resistance against DEMP2
			// zyk: only takes damage if he does not have the upgrade
			if (targ->client->pers.secrets_found & (1 << 7))
				return;

			damage = (int)ceil(damage * (1 - (0.25 * targ->client->pers.skill_levels[55])));
		}
		else if (targ->client->pers.rpg_class == 7)
		{ // zyk: Force Gunner damage resistance
			damage = (int)ceil(damage * (1.0 - (0.06 * targ->client->pers.skill_levels[55])));
		}
		else if (targ->client->pers.rpg_class == 9)
		{ // zyk: Force Guardian damage resistance
			float force_tank_bonus_resistance = 0.0;

			if (targ->client->pers.secrets_found & (1 << 19))
			{ // zyk: Force Guardian Upgrade increases damage resistance
				force_tank_bonus_resistance += 0.1;
			}

			if (targ->client->pers.unique_skill_duration > level.time)
			{ // zyk: Force Guardian Unique Skill increases damage resistance
				force_tank_bonus_resistance += 0.15;
			}

			damage = (int)ceil(damage * (0.9 - force_tank_bonus_resistance - (0.1 * targ->client->pers.skill_levels[55])));
		}
	}

	if (targ && targ->damageRedirect)
	{
		G_Damage(&g_entities[targ->damageRedirectTo], inflictor, attacker, dir, point, damage, dflags, mod);
		return;
	}

	if ((mod == MOD_DEMP2 || (mod == MOD_MELEE && inflictor && inflictor->s.weapon == WP_DEMP2)) && targ && targ->inuse && targ->client)
	{ // zyk: added the MOD_MELEE condition because of Magic Master Electric Bolts
		if ( targ->client->ps.electrifyTime < level.time )
		{//electrocution effect
			if (targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE &&
				targ->m_pVehicle && (targ->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER || targ->m_pVehicle->m_pVehicleInfo->type == VH_WALKER))
			{ //do some extra stuff to speeders/walkers
				targ->client->ps.electrifyTime = level.time + Q_irand( 3000, 4000 );
			}
			else if ( targ->s.NPC_class != CLASS_VEHICLE
				|| (targ->m_pVehicle && targ->m_pVehicle->m_pVehicleInfo->type != VH_FIGHTER) )
			{//don't do this to fighters
				targ->client->ps.electrifyTime = level.time + Q_irand( 300, 800 );
			}
		}
	}

	if ((mod == MOD_MELEE && inflictor && inflictor->s.weapon == WP_FLECHETTE) && targ && targ->client && targ->health > 0 && attacker && attacker->client &&
		(targ->s.number < MAX_CLIENTS || targ->client->NPC_class != CLASS_VEHICLE))
	{ // zyk: hit by poison dart
		targ->client->pers.poison_dart_hit_counter = 40;
		targ->client->pers.poison_dart_user_id = attacker->s.number;
		targ->client->pers.poison_dart_hit_timer = level.time + 200;
		targ->client->pers.player_statuses |= (1 << 20);
	}

	if (level.gametype == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be damaged til the round starts.
		return;
	}

	if (!targ->takedamage) {
		return;
	}

	if ( (targ->flags&FL_SHIELDED) && mod != MOD_SABER  && !targ->client)
	{//magnetically protected, this thing can only be damaged by lightsabers
		if (zyk_can_damage_saber_only_entities(attacker, inflictor, mod) == qfalse)
			return;
	}

	if ((targ->flags & FL_DMG_BY_SABER_ONLY) && mod != MOD_SABER)
	{ //saber-only damage
		if (zyk_can_damage_saber_only_entities(attacker, inflictor, mod) == qfalse)
			return;
	}

	if ( targ->client )
	{//don't take damage when in a walker, or fighter
		//unless the walker/fighter is dead!!! -rww
		if ( targ->client->ps.clientNum < MAX_CLIENTS && targ->client->ps.m_iVehicleNum )
		{
			gentity_t *veh = &g_entities[targ->client->ps.m_iVehicleNum];
			if ( veh->m_pVehicle && veh->health > 0 )
			{
				if ( veh->m_pVehicle->m_pVehicleInfo->type == VH_WALKER ||
					 veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
				{
					if (!(dflags & DAMAGE_NO_PROTECTION))
					{
						return;
					}
				}
			}
		}
	}

	if ((targ->flags & FL_DMG_BY_HEAVY_WEAP_ONLY))
	{ //only take damage from explosives and such
		if (mod != MOD_REPEATER_ALT &&
			mod != MOD_ROCKET &&
			mod != MOD_FLECHETTE_ALT_SPLASH &&
			mod != MOD_ROCKET_HOMING &&
			mod != MOD_THERMAL &&
			mod != MOD_THERMAL_SPLASH &&
			mod != MOD_TRIP_MINE_SPLASH &&
			mod != MOD_TIMED_MINE_SPLASH &&
			mod != MOD_DET_PACK_SPLASH &&
			mod != MOD_VEHICLE &&
			mod != MOD_CONC &&
			mod != MOD_CONC_ALT &&
			mod != MOD_SABER &&
			mod != MOD_TURBLAST &&
			mod != MOD_SUICIDE &&
			mod != MOD_FALLING &&
			mod != MOD_CRUSH &&
			mod != MOD_TELEFRAG &&
			mod != MOD_TRIGGER_HURT)
		{
			if ( mod != MOD_MELEE || (can_damage_heavy_things == qfalse && !G_HeavyMelee( attacker ) ))
			{ //let classes with heavy melee ability damage heavy wpn dmg doors with fists
				return;
			}
		}
	}

	if (targ->flags & FL_BBRUSH)
	{
		if (mod == MOD_DEMP2 ||
			mod == MOD_DEMP2_ALT ||
			mod == MOD_BRYAR_PISTOL ||
			mod == MOD_BRYAR_PISTOL_ALT ||
			mod == MOD_MELEE)
		{ //these don't damage bbrushes.. ever
			if ( mod != MOD_MELEE || (can_damage_heavy_things == qfalse && !G_HeavyMelee( attacker )) )
			{ //let classes with heavy melee ability damage breakable brushes with fists
				return;
			}
		}
	}

	if (targ && targ->client && targ->client->ps.duelInProgress)
	{
		if (attacker && attacker->client && attacker->s.number != targ->client->ps.duelIndex)
		{
			return;
		}
		else if (attacker && attacker->client && mod != MOD_SABER)
		{
			return;
		}
	}
	if (attacker && attacker->client && attacker->client->ps.duelInProgress)
	{
		if (targ && targ->client && targ->s.number != attacker->client->ps.duelIndex)
		{
			return;
		}
		else if (targ && targ->client && mod != MOD_SABER)
		{
			return;
		}
	}

	// the intermission has allready been qualified for, so don't
	// allow any extra scoring
	if ( level.intermissionQueued ) {
		return;
	}
	if ( !inflictor ) {
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if ( !attacker ) {
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// shootable doors / buttons don't actually have any health

	//if genericValue4 == 1 then it's glass or a breakable and those do have health
	if ( targ->s.eType == ET_MOVER && targ->genericValue4 != 1 ) {
		if ( targ->use && targ->moverState == MOVER_POS1 ) {
			GlobalUse( targ, inflictor, attacker );
		}
		return;
	}

	if ( !(dflags&DAMAGE_NO_HIT_LOC) )
	{//see if we should modify it by damage location
		if (targ->inuse && (targ->client || targ->s.eType == ET_NPC) &&
			attacker->inuse && (attacker->client || attacker->s.eType == ET_NPC))
		{ //check for location based damage stuff.
			G_LocationBasedDamageModifier(targ, point, mod, dflags, &damage);
		}
	}

	client = targ->client;

	if ( client ) {
		if ( client->noclip ) {
			return;
		}
	}

	if ( !dir ) {
		dflags |= DAMAGE_NO_KNOCKBACK;
	} else {
		VectorNormalize(dir);
	}

	// zyk: lowered knockback. Default: knockback = damage
	knockback = damage/2;
	// zyk: Lightning level 4 in RPG Mode causes knockback
	if (attacker && attacker->client && attacker->client->sess.amrpgmode == 2 && attacker->client->pers.skill_levels[13] > 3 && 
		attacker->client->ps.fd.forcePowersActive & (1 << FP_LIGHTNING) && mod == MOD_FORCE_DARK)
	{
		knockback *= 6;
	}

	if ( knockback > 200 ) {
		knockback = 200;
	}
	if ( targ->flags & FL_NO_KNOCKBACK ) {
		knockback = 0;
	}
	if ( dflags & DAMAGE_NO_KNOCKBACK ) {
		knockback = 0;
	}

	// zyk: if player is in RPG Mode, reduce knockback based on the Impact Reducer item of the player and on Force Guardian Upgrade for Force Guardian class
	if (targ && targ->client && targ->client->sess.amrpgmode == 2)
	{
		int new_knockback = knockback;

		if (targ->client->pers.secrets_found & (1 << 9))
			new_knockback -= knockback * 0.8;

		if (targ->client->pers.rpg_class == 9 && targ->client->pers.secrets_found & (1 << 19))
			new_knockback -= knockback * 0.15;

		knockback = new_knockback;
	}

	// zyk: these npcs will not have knockback
	if (targ && targ->client && targ->NPC && (targ->client->pers.guardian_mode == 2 || (targ->client->pers.guardian_mode == 17 && Q_stricmp(targ->NPC_type, "guardian_boss_2") == 0) ||
		targ->client->pers.guardian_mode == 3 || (targ->client->pers.guardian_mode == 17 && Q_stricmp(targ->NPC_type, "guardian_boss_3") == 0) ||
		targ->client->pers.guardian_mode == 7 || (targ->client->pers.guardian_mode == 17 && Q_stricmp(targ->NPC_type, "guardian_boss_7") == 0) ||
		targ->client->pers.guardian_mode == 11 || (targ->client->pers.guardian_mode == 17 && Q_stricmp(targ->NPC_type, "guardian_boss_8") == 0) ||
		targ->client->pers.guardian_mode == 21))
	{
		knockback = 0;
	}

	// figure momentum add, even if the damage won't be taken
	if ( knockback && targ->client ) {
		vec3_t	kvel;
		float	mass;

		mass = 200;

		if (mod == MOD_SABER)
		{
			float saberKnockbackScale = g_saberDmgVelocityScale.value;
			if ( (dflags&DAMAGE_SABER_KNOCKBACK1)
				|| (dflags&DAMAGE_SABER_KNOCKBACK2) )
			{//saber does knockback, scale it by the right number
				if ( !saberKnockbackScale )
				{
					saberKnockbackScale = 1.0f;
				}
				if ( attacker
					&& attacker->client )
				{
					if ( (dflags&DAMAGE_SABER_KNOCKBACK1) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[0].knockbackScale;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK1_B2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[0].knockbackScale2;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[1].knockbackScale;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK2_B2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[1].knockbackScale2;
						}
					}
				}
			}
			VectorScale (dir, (g_knockback.value * (float)knockback / mass)*saberKnockbackScale, kvel);
		}
		else
		{
			VectorScale (dir, g_knockback.value * (float)knockback / mass, kvel);
		}
		VectorAdd (targ->client->ps.velocity, kvel, targ->client->ps.velocity);

		if (attacker && attacker->client && attacker != targ)
		{
			float dur = 5000;
			float dur2 = 100;
			if (targ->client && targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE)
			{
				dur = 25000;
				dur2 = 25000;
			}
			targ->client->ps.otherKiller = attacker->s.number;
			targ->client->ps.otherKillerTime = level.time + dur;
			targ->client->ps.otherKillerDebounceTime = level.time + dur2;
		}
		// set the timer so that the other client can't cancel
		// out the movement immediately
		if ( !targ->client->ps.pm_time && (g_saberDmgVelocityScale.integer || mod != MOD_SABER || (dflags&DAMAGE_SABER_KNOCKBACK1) || (dflags&DAMAGE_SABER_KNOCKBACK2) || (dflags&DAMAGE_SABER_KNOCKBACK1_B2) || (dflags&DAMAGE_SABER_KNOCKBACK2_B2) ) ) {
			int		t;

			t = knockback * 2;
			if ( t < 50 ) {
				t = 50;
			}
			if ( t > 200 ) {
				t = 200;
			}
			targ->client->ps.pm_time = t;
			targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
		}
	}
	else if (targ->client && targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE && attacker != targ)
	{
		targ->client->ps.otherKiller = attacker->s.number;
		targ->client->ps.otherKillerTime = level.time + 25000;
		targ->client->ps.otherKillerDebounceTime = level.time + 25000;
	}


	if ( (g_jediVmerc.integer || level.gametype == GT_SIEGE)
		&& client )
	{//less explosive damage for jedi, more saber damage for non-jedi
		if ( client->ps.trueJedi
			|| (level.gametype == GT_SIEGE&&client->ps.weapon == WP_SABER))
		{//if the target is a trueJedi, reduce splash and explosive damage to 1/2
			switch ( mod )
			{
			case MOD_REPEATER_ALT:
			case MOD_REPEATER_ALT_SPLASH:
			case MOD_DEMP2_ALT:
			case MOD_FLECHETTE_ALT_SPLASH:
			case MOD_ROCKET:
			case MOD_ROCKET_SPLASH:
			case MOD_ROCKET_HOMING:
			case MOD_ROCKET_HOMING_SPLASH:
			case MOD_THERMAL:
			case MOD_THERMAL_SPLASH:
			case MOD_TRIP_MINE_SPLASH:
			case MOD_TIMED_MINE_SPLASH:
			case MOD_DET_PACK_SPLASH:
				damage *= 0.75;
				break;
			}
		}
		else if ( (client->ps.trueNonJedi || (level.gametype == GT_SIEGE&&client->ps.weapon != WP_SABER))
			&& mod == MOD_SABER )
		{//if the target is a trueNonJedi, take more saber damage... combined with the 1.5 in the w_saber stuff, this is 6 times damage!
			if ( damage < 100 )
			{
				damage *= 4;
				if ( damage > 100 )
				{
					damage = 100;
				}
			}
		}
	}

	if (attacker->client && targ->client && level.gametype == GT_SIEGE &&
		targ->client->siegeClass != -1 && (bgSiegeClasses[targ->client->siegeClass].classflags & (1<<CFL_STRONGAGAINSTPHYSICAL)))
	{ //this class is flagged to take less damage from physical attacks.
		//For now I'm just decreasing against any client-based attack, this can be changed later I guess.
		damage *= 0.5;
	}

	// check for completely getting out of the damage
	if ( !(dflags & DAMAGE_NO_PROTECTION) ) {

		// if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
		// if the attacker was on the same team
		if ( targ != attacker)
		{
			if (OnSameTeam (targ, attacker))
			{
				if ( !g_friendlyFire.integer )
				{
					return;
				}
			}
			else if (attacker && attacker->inuse &&
				!attacker->client && attacker->activator &&
				targ != attacker->activator &&
				attacker->activator->inuse && attacker->activator->client)
			{ //emplaced guns don't hurt teammates of user
				if (OnSameTeam (targ, attacker->activator))
				{
					if ( !g_friendlyFire.integer )
					{
						return;
					}
				}
			}
			else if (targ->inuse && targ->client &&
				level.gametype >= GT_TEAM &&
				attacker->s.number >= MAX_CLIENTS &&
				attacker->alliedTeam &&
				targ->client->sess.sessionTeam == attacker->alliedTeam &&
				!g_friendlyFire.integer)
			{ //things allied with my team should't hurt me.. I guess
				return;
			}
		}

		if (level.gametype == GT_JEDIMASTER && !g_friendlyFire.integer &&
			targ && targ->client && attacker && attacker->client &&
			targ != attacker && !targ->client->ps.isJediMaster && !attacker->client->ps.isJediMaster &&
			G_ThereIsAMaster())
		{
			return;
		}

		if (targ->s.number >= MAX_CLIENTS && targ->client
			&& targ->s.shouldtarget && targ->s.teamowner &&
			attacker && attacker->inuse && attacker->client && targ->s.owner >= 0 && targ->s.owner < MAX_CLIENTS)
		{
			gentity_t *targown = &g_entities[targ->s.owner];

			if (targown && targown->inuse && targown->client && OnSameTeam(targown, attacker))
			{
				if (!g_friendlyFire.integer)
				{
					return;
				}
			}
		}

		// check for godmode
		if ( (targ->flags & FL_GODMODE) && targ->s.eType != ET_NPC ) {
			return;
		}

		if (targ && targ->client && (targ->client->ps.eFlags & EF_INVULNERABLE) &&
			attacker && attacker->client && targ != attacker)
		{
			if (targ->client->invulnerableTimer <= level.time)
			{
				targ->client->ps.eFlags &= ~EF_INVULNERABLE;
			}
			else if (!((targ->client->sess.amrpgmode == 2 || targ->NPC) && targ->client->pers.quest_power_status & (1 << 11)))
			{ // zyk: added condition to not consider clients using Magic Shield
				return;
			}
		}
	}

	//check for teamnodmg
	//NOTE: non-client objects hitting clients (and clients hitting clients) purposely doesn't obey this teamnodmg (for emplaced guns)
	if ( attacker && !targ->client )
	{//attacker hit a non-client
		if ( level.gametype == GT_SIEGE &&
			!g_ff_objectives.integer )
		{//in siege mode (and...?)
			if ( targ->teamnodmg )
			{//targ shouldn't take damage from a certain team
				if ( attacker->client )
				{//a client hit a non-client object
					if ( targ->teamnodmg == attacker->client->sess.sessionTeam )
					{
						return;
					}
				}
				else if ( attacker->teamnodmg )
				{//a non-client hit a non-client object
					//FIXME: maybe check alliedTeam instead?
					if ( targ->teamnodmg == attacker->teamnodmg )
					{
						if (attacker->activator &&
							attacker->activator->inuse &&
							attacker->activator->s.number < MAX_CLIENTS &&
							attacker->activator->client &&
							attacker->activator->client->sess.sessionTeam != targ->teamnodmg)
						{ //uh, let them damage it I guess.
						}
						else
						{
							return;
						}
					}
				}
			}
		}
	}

	#ifdef BASE_COMPAT
		// battlesuit protects from all radius damage (but takes knockback)
		// and protects 50% against all damage
		if ( client && client->ps.powerups[PW_BATTLESUIT] ) {
			G_AddEvent( targ, EV_POWERUP_BATTLESUIT, 0 );
			if ( ( dflags & DAMAGE_RADIUS ) || ( mod == MOD_FALLING ) ) {
				return;
			}
			damage *= 0.5;
		}
	#endif

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if ( attacker->client && targ != attacker && targ->health > 0
			&& targ->s.eType != ET_MISSILE
			&& targ->s.eType != ET_GENERAL
			&& client) {
		if ( OnSameTeam( targ, attacker ) ) {
			attacker->client->ps.persistant[PERS_HITS]--;
		} else {
			attacker->client->ps.persistant[PERS_HITS]++;
		}
		attacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);
	}

	if ( damage < 1 ) {
		damage = 1;
	}
	take = damage;

	if (attacker && 
		attacker->client && 
		attacker->client->NPC_class != CLASS_RANCOR && // zyk: grip cant be absorbed by shields
		(attacker->client->ps.fd.forcePowersActive & (1 << FP_GRIP) &&
		mod == MOD_FORCE_DARK)
		) // zyk: grip cant be absorbed by shields
		check_shield = 0;
	
	if (targ && targ->client && targ->client->NPC_class == CLASS_VEHICLE //ion-cannon has disabled this ship's shields, take damage on hull!
		&& targ->m_pVehicle
		&& targ->client->ps.electrifyTime > level.time)
		check_shield = 0;

	// save some from armor
	// zyk: now the shield check is made here
	if (check_shield == 1 && targ && targ->client && take > 0)  
	{ // zyk: check shields if the damage is greater than 0
		int scaled_damage = take;
		float bounty_hunter_shield_resistance = 0.0;

		if (targ->client->sess.amrpgmode == 2) // zyk: Shield Strength skill
		{
			// zyk: if player is Bounty Hunter and has the Bounty Hunter Upgrade, absorbs more damage
			if (targ->client->pers.rpg_class == 2 && targ->client->pers.secrets_found & (1 << 1))
				bounty_hunter_shield_resistance = 0.07;

			scaled_damage = (int)ceil(take * (1.0 - bounty_hunter_shield_resistance - (0.07 * targ->client->pers.skill_levels[31])));
		}

		if (targ->client->ps.stats[STAT_ARMOR] >= scaled_damage)
		{
			targ->client->ps.stats[STAT_ARMOR] -= scaled_damage;
			asave = take;
		}
		else
		{ // zyk: calculates the amount of damage saved by the shields so the remaining damage will be done to health
			asave = take * ((1.0 * targ->client->ps.stats[STAT_ARMOR])/scaled_damage);
			targ->client->ps.stats[STAT_ARMOR] = 0;
		}
	}

	if (asave)
	{
		shieldAbsorbed = asave;
	}

	take -= asave;
	if ( targ->client )
	{//update vehicle shields and armor, check for explode
		if ( targ->client->NPC_class == CLASS_VEHICLE &&
			targ->m_pVehicle )
		{//FIXME: should be in its own function in g_vehicles.c now, too big to be here
			int surface = -1;
			if ( attacker )
			{//so we know the last guy who shot at us
				targ->enemy = attacker;
			}

			if ( targ->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL )
			{
				//((CVehicleNPC *)targ->NPC)->m_ulFlags |= CVehicleNPC::VEH_BUCKING;
			}

			targ->m_pVehicle->m_iShields = targ->client->ps.stats[STAT_ARMOR];
			G_VehUpdateShields( targ );
			targ->m_pVehicle->m_iArmor -= take;
			if ( targ->m_pVehicle->m_iArmor <= 0 )
			{
				targ->s.eFlags |= EF_DEAD;
				targ->client->ps.eFlags |= EF_DEAD;
				targ->m_pVehicle->m_iArmor = 0;
			}
			if ( targ->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			{//get the last surf that was hit
				if ( targ->client && targ->client->g2LastSurfaceTime == level.time)
				{
					char hitSurface[MAX_QPATH];

					trap->G2API_GetSurfaceName(targ->ghoul2, targ->client->g2LastSurfaceHit, 0, hitSurface);

					if (hitSurface[0])
					{
						surface = G_ShipSurfaceForSurfName( &hitSurface[0] );

						if ( take && surface > 0 )
						{//hit a certain part of the ship
							int deathPoint = 0;

							targ->locationDamage[surface] += take;

							switch(surface)
							{
							case SHIPSURF_FRONT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_front;
								break;
							case SHIPSURF_BACK:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_back;
								break;
							case SHIPSURF_RIGHT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_right;
								break;
							case SHIPSURF_LEFT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_left;
								break;
							default:
								break;
							}

							//presume 0 means it wasn't set and so it should never die.
							if ( deathPoint )
							{
								if ( targ->locationDamage[surface] >= deathPoint)
								{ //this area of the ship is now dead
									if ( G_FlyVehicleDestroySurface( targ, surface ) )
									{//actually took off a surface
										G_VehicleSetDamageLocFlags( targ, surface, deathPoint );
									}
								}
								else
								{
									G_VehicleSetDamageLocFlags( targ, surface, deathPoint );
								}
							}
						}
					}
				}
			}
			if ( targ->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL )
			{
				/*
				if ( targ->m_pVehicle->m_iArmor <= 0 )
				{//vehicle all out of armor
					Vehicle_t *pVeh = targ->m_pVehicle;
					if ( pVeh->m_iDieTime == 0 )
					{//just start the flaming effect and explosion delay, if it's not going already...
						pVeh->m_pVehicleInfo->StartDeathDelay( pVeh, 0 );
					}
				}
				else*/
				if ( attacker
						//&& attacker->client
						&& targ != attacker
						&& point
						&& !VectorCompare( targ->client->ps.origin, point )
						&& targ->m_pVehicle->m_LandTrace.fraction >= 1.0f)
				{//just took a hit, knock us around
					vec3_t	vUp, impactDir;
					float	impactStrength = (damage/200.0f)*10.0f;
					float	dot = 0.0f;
					if ( impactStrength > 10.0f )
					{
						impactStrength = 10.0f;
					}
					//pitch or roll us based on where we were hit
					AngleVectors( targ->m_pVehicle->m_vOrientation, NULL, NULL, vUp );
					VectorSubtract( point, targ->r.currentOrigin, impactDir );
					VectorNormalize( impactDir );
					if ( surface <= 0 )
					{//no surf guess where we were hit, then
						vec3_t	vFwd, vRight;
						AngleVectors( targ->m_pVehicle->m_vOrientation, vFwd, vRight, vUp );
						dot = DotProduct( vRight, impactDir );
						if ( dot > 0.4f )
						{
							surface = SHIPSURF_RIGHT;
						}
						else if ( dot < -0.4f )
						{
							surface = SHIPSURF_LEFT;
						}
						else
						{
							dot = DotProduct( vFwd, impactDir );
							if ( dot > 0.0f )
							{
								surface = SHIPSURF_FRONT;
							}
							else
							{
								surface = SHIPSURF_BACK;
							}
						}
					}
					switch ( surface )
					{
					case SHIPSURF_FRONT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[PITCH] += impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[PITCH] -= impactStrength;
						}
						break;
					case SHIPSURF_BACK:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[PITCH] -= impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[PITCH] += impactStrength;
						}
						break;
					case SHIPSURF_RIGHT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[ROLL] -= impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[ROLL] += impactStrength;
						}
						break;
					case SHIPSURF_LEFT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[ROLL] += impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[ROLL] -= impactStrength;
						}
						break;
					}

				}
			}
		}
	}

	// zyk: Electric Bolts of Magic Master can disable jetpacks. Except Stealth Attacker ones
	if (mod == MOD_MELEE && inflictor && inflictor->s.weapon == WP_DEMP2 && client)
	{
		if (client->jetPackOn && (client->sess.amrpgmode != 2 || client->pers.rpg_class != 5 || !(client->pers.secrets_found & (1 << 7))))
		{ //disable jetpack temporarily
			Jetpack_Off(targ);
			client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
		}

		if (client->NPC_class == CLASS_BOBAFETT)
		{ // zyk: DEMP2 also disables npc jetpack
			Boba_FlyStop(targ);
		}
	}

	if ( mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT )
	{//FIXME: screw with non-animal vehicles, too?
		if ( client )
		{
			if ( client->NPC_class == CLASS_VEHICLE
				&& targ->m_pVehicle
				&& targ->m_pVehicle->m_pVehicleInfo
				&& targ->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			{//all damage goes into the disruption of shields and systems
				take = 0;
			}
			else
			{

				if (client->jetPackOn)
				{ //disable jetpack temporarily
					Jetpack_Off(targ);
					client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
				}

				if (client->NPC_class == CLASS_BOBAFETT)
				{ // zyk: DEMP2 also disables npc jetpack
					Boba_FlyStop(targ);
				}

				if ( client->NPC_class == CLASS_PROTOCOL || client->NPC_class == CLASS_SEEKER ||
					client->NPC_class == CLASS_R2D2 || client->NPC_class == CLASS_R5D2 ||
					client->NPC_class == CLASS_MOUSE || client->NPC_class == CLASS_GONK )
				{
					// DEMP2 does more damage to these guys.
					take *= 2;
				}
				else if ( client->NPC_class == CLASS_PROBE || client->NPC_class == CLASS_INTERROGATOR ||
							client->NPC_class == CLASS_MARK1 || client->NPC_class == CLASS_MARK2 || client->NPC_class == CLASS_SENTRY ||
							client->NPC_class == CLASS_ATST )
				{
					// DEMP2 does way more damage to these guys.
					// zyk: Guardian of Wind takes less DEMP2 damage
					if (!(client->pers.guardian_mode == 7 || (client->pers.guardian_mode == 17 && Q_stricmp(targ->NPC_type, "guardian_boss_7") == 0)))
						take *= 4; // zyk: changed from 5 to 4
				}
				else
				{
					if (take > 1)
					{
						take /= 2; // zyk: changed from 3 to 2
					}
				}
			}
		}
	}

	if ( g_debugDamage.integer ) {
		trap->Print( "%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
			targ->health, take, asave );
	}

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( client ) {
		if ( attacker ) {
			client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
		} else {
			client->ps.persistant[PERS_ATTACKER] = ENTITYNUM_WORLD;
		}
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		if ( dir ) {
			VectorCopy ( dir, client->damage_from );
			client->damage_fromWorld = qfalse;
		} else {
			VectorCopy ( targ->r.currentOrigin, client->damage_from );
			client->damage_fromWorld = qtrue;
		}

		if (attacker && attacker->client)
		{
			BotDamageNotification(client, attacker);
		}
		else if (inflictor && inflictor->client)
		{
			BotDamageNotification(client, inflictor);
		}
	}

	// See if it's the player hurting the emeny flag carrier
	if( level.gametype == GT_CTF || level.gametype == GT_CTY) {
		Team_CheckHurtCarrier(targ, attacker);
	}

	if (targ->client) {
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod = mod;
	}

	if (shieldAbsorbed)
	{
		/*
		if ( targ->client->NPC_class == CLASS_VEHICLE )
		{
			targ->client->ps.electrifyTime = level.time + Q_irand( 500, 1000 );
		}
		else
		*/
		{
			gentity_t	*evEnt;

			// Send off an event to show a shield shell on the player, pointing in the right direction.
			//evEnt = G_TempEntity(vec3_origin, EV_SHIELD_HIT);
			//rww - er.. what the? This isn't broadcast, why is it being set on vec3_origin?!
			evEnt = G_TempEntity(targ->r.currentOrigin, EV_SHIELD_HIT);
			evEnt->s.otherEntityNum = targ->s.number;
			evEnt->s.eventParm = DirToByte(dir);
			evEnt->s.time2=shieldAbsorbed;
	/*
			shieldAbsorbed *= 20;

			if (shieldAbsorbed > 1500)
			{
				shieldAbsorbed = 1500;
			}
			if (shieldAbsorbed < 200)
			{
				shieldAbsorbed = 200;
			}

			if (targ->client->ps.powerups[PW_SHIELDHIT] < (level.time + shieldAbsorbed))
			{
				targ->client->ps.powerups[PW_SHIELDHIT] = level.time + shieldAbsorbed;
			}
			//flicker for as many ms as damage was absorbed (*20)
			//therefore 10 damage causes 1/5 of a seond of flickering, whereas
			//a full 100 causes 2 seconds (but is reduced to 1.5 seconds due to the max)

	*/
		}
	}

	// do the damage
	if (take)
	{
		if (targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_PROTECT)))
		{
			if (targ->client->ps.fd.forcePower)
			{
				float force_decrease_change = 1.0; // zyk: Protect 4/4 will make player lose less force

				if (targ->client->sess.amrpgmode == 2 && targ->client->pers.skill_levels[10] == 4)
					force_decrease_change = 0.5;

				if (targ->client->forcePowerSoundDebounce < level.time)
				{
					G_PreDefSound(targ->client->ps.origin, PDSOUND_PROTECTHIT);
					targ->client->forcePowerSoundDebounce = level.time + 400;
				}

				// zyk: changed Force Protect code
				if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_1)
				{
					targ->client->ps.fd.forcePower -= (int)ceil(take*0.5*force_decrease_change);
					take = (int)ceil(take*0.85);
				}
				else if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_2)
				{
					targ->client->ps.fd.forcePower -= (int)ceil(take*0.25*force_decrease_change);
					take = (int)ceil(take*0.7);
				}
				else if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_3)
				{
					targ->client->ps.fd.forcePower -= (int)ceil(take*0.125*force_decrease_change);
					take = (int)ceil(take*0.55);
				}

				if (targ->client->ps.fd.forcePower < 0)
					targ->client->ps.fd.forcePower = 0;
			}
		}

		if (targ->client && targ->s.number < MAX_CLIENTS &&
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT))
		{ //uh.. shock them or something. what the hell, I don't know.
			// zyk: now always stuns the enemy
            //if (targ->client->ps.weaponTime <= 0)
			//{ //yeah, we were supposed to be beta a week ago, I don't feel like
				//breaking the game so I'm gonna be safe and only do this only
				//if your weapon is not busy
				targ->client->ps.weaponTime = 2000;
				targ->client->ps.electrifyTime = level.time + 2000;
				if (targ->client->ps.weaponstate == WEAPON_CHARGING ||
					targ->client->ps.weaponstate == WEAPON_CHARGING_ALT)
				{
					targ->client->ps.weaponstate = WEAPON_READY;
				}
			//}
		}

		if (targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) && (inflictor->client || attacker->client))
		{ // zyk: new Force Rage code
			if (targ->client->ps.fd.forcePowerLevel[FP_RAGE] == 1)
				take = (int)ceil(take*0.85);
			else if (targ->client->ps.fd.forcePowerLevel[FP_RAGE] == 2)
				take = (int)ceil(take*0.7);
			else if (targ->client->ps.fd.forcePowerLevel[FP_RAGE] == 3)
				take = (int)ceil(take*0.55);
		}

		if (!targ->NPC && targ->client && targ->client->sess.amrpgmode == 2)
		{ // zyk: Health Strength skill decreases damage taken
			float bonus_resistance = 0.0;

			// zyk: if player is a Bounty Hunter and has the Bounty Hunter Upgrade, absorbs some damage taken
			if (targ->client->pers.rpg_class == 2 && targ->client->pers.secrets_found & (1 << 1))
				bonus_resistance = 0.07;

			take = (int)ceil(take * (1.0 - bonus_resistance - (0.07 * targ->client->pers.skill_levels[32])));

			// zyk: Improvements skill makes Force Guardian regen some force with Rage when taking damage
			if (targ->client->pers.rpg_class == 9 && targ->client->pers.skill_levels[55] > 0 && 
				(targ->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) && (inflictor->client || attacker->client))
			{
				targ->client->ps.fd.forcePower += (int)ceil((take * 0.1 * targ->client->pers.skill_levels[55]));

				if (targ->client->ps.fd.forcePower > targ->client->ps.fd.forcePowerMax)
					targ->client->ps.fd.forcePower = targ->client->ps.fd.forcePowerMax;
			}
		}

		targ->health = targ->health - take;

		// zyk: training pole. Adds the count and sets the wait to execute the think function
		if (targ && (targ->spawnflags & 1) && Q_stricmp(targ->classname, "zyk_training_pole") == 0)
		{
			targ->count += take;
			
			targ->nextthink = level.time + targ->wait;
		}

		if ( (targ->flags&FL_UNDYING) )
		{//take damage down to 1, but never die
			if ( targ->health < 1 )
			{
				targ->health = 1;
			}
		}

		if ( targ->client ) {
			targ->client->ps.stats[STAT_HEALTH] = targ->health;
		}

		//We want to go ahead and set gPainHitLoc regardless of if we have a pain func,
		//so we can adjust the location damage too.
		if (targ->client && targ->ghoul2 && targ->client->g2LastSurfaceTime == level.time)
		{ //We updated the hit surface this frame, so it's valid.
			char hitSurface[MAX_QPATH];

			trap->G2API_GetSurfaceName(targ->ghoul2, targ->client->g2LastSurfaceHit, 0, hitSurface);

			if (hitSurface[0])
			{
				G_GetHitLocFromSurfName(targ, hitSurface, &gPainHitLoc, point, dir, vec3_origin, mod);
			}
			else
			{
				gPainHitLoc = -1;
			}

			if (gPainHitLoc < HL_MAX && gPainHitLoc >= 0 && targ->locationDamage[gPainHitLoc] < Q3_INFINITE &&
				(targ->s.eType == ET_PLAYER || targ->s.NPC_class != CLASS_VEHICLE))
			{
				targ->locationDamage[gPainHitLoc] += take;

				if (g_armBreakage.integer && !targ->client->ps.brokenLimbs &&
					targ->client->ps.stats[STAT_HEALTH] > 0 && targ->health > 0 &&
					!(targ->s.eFlags & EF_DEAD))
				{ //check for breakage
					if (targ->locationDamage[HL_ARM_RT]+targ->locationDamage[HL_HAND_RT] >= 80)
					{
						G_BreakArm(targ, BROKENLIMB_RARM);
					}
					else if (targ->locationDamage[HL_ARM_LT]+targ->locationDamage[HL_HAND_LT] >= 80)
					{
						G_BreakArm(targ, BROKENLIMB_LARM);
					}
				}
			}
		}
		else
		{
			gPainHitLoc = -1;
		}

		if (targ->maxHealth)
		{ //if this is non-zero this guy should be updated his s.health to send to the client
			G_ScaleNetHealth(targ);
		}

		if ( targ->health <= 0 ) {
			if ( client )
			{
				targ->flags |= FL_NO_KNOCKBACK;

				if (point)
				{
					VectorCopy( point, targ->pos1 );
				}
				else
				{
					VectorCopy(targ->client->ps.origin, targ->pos1);
				}
			}
			else if (targ->s.eType == ET_NPC)
			{ //g2animent
				VectorCopy(point, targ->pos1);
			}

			if (targ->health < -999)
				targ->health = -999;

			// If we are a breaking glass brush, store the damage point so we can do cool things with it.
			if ( targ->r.svFlags & SVF_GLASS_BRUSH )
			{
				VectorCopy( point, targ->pos1 );
				if (dir)
				{
					VectorCopy( dir, targ->pos2 );
				}
				else
				{
					VectorClear(targ->pos2);
				}
			}

			if (targ->s.eType == ET_NPC &&
				targ->client &&
				(targ->s.eFlags & EF_DEAD))
			{ //an NPC that's already dead. Maybe we can cut some more limbs off!
				if ( (mod == MOD_SABER || (mod == MOD_MELEE && G_HeavyMelee( attacker )) )//saber or heavy melee (claws)
					&& take > 2
					&& !(dflags&DAMAGE_NO_DISMEMBER) )
				{
					G_CheckForDismemberment(targ, attacker, targ->pos1, take, targ->client->ps.torsoAnim, qtrue);
				}
			}

			targ->enemy = attacker;
			targ->die (targ, inflictor, attacker, take, mod);
			G_ActivateBehavior( targ, BSET_DEATH );
			return;
		}
		else
		{
			if ( g_debugMelee.integer )
			{//getting hurt makes you let go of the wall
				if ( targ->client && (targ->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
				{
					G_LetGoOfWall( targ );
				}
			}
			if ( targ->pain )
			{
				if (targ->s.eType != ET_NPC || mod != MOD_SABER || take > 1)
				{ //don't even notify NPCs of pain if it's just idle saber damage
					gPainMOD = mod;
					if (point)
					{
						VectorCopy(point, gPainPoint);
					}
					else
					{
						VectorCopy(targ->r.currentOrigin, gPainPoint);
					}
					targ->pain (targ, attacker, take);
				}
			}
		}

		G_LogWeaponDamage(attacker->s.number, mod, take);
	}

}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage (gentity_t *targ, vec3_t origin) {
	vec3_t	dest;
	trace_t	tr;
	vec3_t	midpoint;

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin is 0,0,0
	VectorAdd (targ->r.absmin, targ->r.absmax, midpoint);
	VectorScale (midpoint, 0.5, midpoint);

	VectorCopy (midpoint, dest);
	trap->Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);
	if (tr.fraction == 1.0 || tr.entityNum == targ->s.number)
		return qtrue;

	// this should probably check in the plane of projection,
	// rather than in world coordinate, and also include Z
	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	trap->Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	trap->Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	trap->Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	trap->Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);
	if (tr.fraction == 1.0)
		return qtrue;


	return qfalse;
}


/*
============
G_RadiusDamage
============
*/
extern qboolean npcs_on_same_team(gentity_t *attacker, gentity_t *target);
extern qboolean zyk_unique_ability_can_hit_target(gentity_t *attacker, gentity_t *target);
extern qboolean zyk_check_immunity_power(gentity_t *ent);
qboolean G_RadiusDamage ( vec3_t origin, gentity_t *attacker, float damage, float radius,
					 gentity_t *ignore, gentity_t *missile, int mod) {
	float		points, dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;
	qboolean	roastPeople = qfalse;

	/*
	if (missile && !missile->client && missile->s.weapon > WP_NONE &&
		missile->s.weapon < WP_NUM_WEAPONS && missile->r.ownerNum >= 0 &&
		(missile->r.ownerNum < MAX_CLIENTS || g_entities[missile->r.ownerNum].s.eType == ET_NPC))
	{ //sounds like it's a valid weapon projectile.. is it a valid explosive to create marks from?
		switch(missile->s.weapon)
		{
		case WP_FLECHETTE: //flechette issuing this will be alt-fire
		case WP_ROCKET_LAUNCHER:
		case WP_THERMAL:
		case WP_TRIP_MINE:
		case WP_DET_PACK:
			roastPeople = qtrue; //Then create explosive marks
			break;
		default:
			break;
		}
	}
	*/
	//oh well.. maybe sometime? I am trying to cut down on tempent use.

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		if (i == 2 && attacker && Q_stricmp(attacker->targetname, "zyk_quest_effect_rockfall") == 0)
		{ // zyk: Rockfall quest power calculates the bounding box in a different way
			mins[i] = origin[i] - radius;
			maxs[i] = origin[i] + radius + 1000;
		}
		else if (i == 2 && attacker && Q_stricmp(attacker->targetname, "zyk_quest_effect_dome") == 0)
		{ // zyk: Dome of Damage quest power calculates the bounding box in a different way
			mins[i] = origin[i] - 20;
			maxs[i] = origin[i] + radius - 150;
		}
		else
		{
			mins[i] = origin[i] - radius;
			maxs[i] = origin[i] + radius;
		}
	}

	numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if (i == 2 && attacker && Q_stricmp(attacker->targetname, "zyk_quest_effect_rockfall") == 0)
			{ // zyk: Rockfall quest power will consider only the distance in x and y axis
				v[i] = 0;
			}
			else
			{
				if ( origin[i] < ent->r.absmin[i] ) {
					v[i] = ent->r.absmin[i] - origin[i];
				} else if ( origin[i] > ent->r.absmax[i] ) {
					v[i] = origin[i] - ent->r.absmax[i];
				} else {
					v[i] = 0;
				}
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

	//	if ( ent->health <= 0 )
	//		continue;

		points = damage * ( 1.0 - dist / radius );

		if( CanDamage (ent, origin) ) {
			if( LogAccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
			}
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			if (attacker && attacker->inuse && attacker->client &&
				attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle && attacker->m_pVehicle->m_pPilot)
			{ //say my pilot did it.
				G_Damage (ent, NULL, (gentity_t *)attacker->m_pVehicle->m_pPilot, dir, origin, (int)points, DAMAGE_RADIUS, mod);
			}
			else
			{
				if (attacker && ent && level.special_power_effects[attacker->s.number] != -1 && 
					Q_stricmp(attacker->targetname, "zyk_quest_effect_healing") == 0)
				{ // zyk: Healing Area. Heals the user and his allies
					gentity_t *quest_power_user = &g_entities[level.special_power_effects[attacker->s.number]];

					// zyk: if the power user and the target are allies (player or npc), or the target is the quest power user himself, heal him
					if (quest_power_user && quest_power_user->client && ent && ent->client && ent->health > 0 && 
						(level.special_power_effects[attacker->s.number] == ent->s.number || OnSameTeam(quest_power_user, ent) == qtrue || 
						npcs_on_same_team(quest_power_user, ent) == qtrue || zyk_is_ally(quest_power_user,ent) == qtrue) && 
						quest_power_user->client->pers.guardian_mode == ent->client->pers.guardian_mode)
					{
						if (quest_power_user->client->sess.amrpgmode == 2 && quest_power_user->client->pers.rpg_class == 8 && 
							quest_power_user->client->pers.unique_skill_duration > level.time &&
							!(quest_power_user->client->pers.player_statuses & (1 << 21)) && 
							!(quest_power_user->client->pers.player_statuses & (1 << 22)))
						{ // zyk: Magic Master Unique Skill increases amount of health recovered
							int heal_amount = 8;
							int shield_amount = 8;

							// zyk: Universe Power
							if (quest_power_user->client->pers.quest_power_status & (1 << 13))
							{
								heal_amount += 2;
								shield_amount += 2;
							}

							// zyk: Magic Master Healing Improvement unique ability. Increases healing
							if (quest_power_user->client->pers.player_statuses & (1 << 23))
							{
								heal_amount *= 2;
								shield_amount *= 2;

								// zyk: restores force too
								if (ent->client->ps.fd.forcePower < ent->client->ps.fd.forcePowerMax)
									ent->client->ps.fd.forcePower += 1;
							}

							if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
								ent->health += heal_amount;
							else
								ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

							if (ent->health == ent->client->ps.stats[STAT_MAX_HEALTH])
							{ // zyk: Unique Skill makes it possible to heal shield too, if hp is full
								int max_shield = ent->client->ps.stats[STAT_MAX_HEALTH];

								if (ent->client->sess.amrpgmode == 2)
									max_shield = ent->client->pers.max_rpg_shield;

								if (!ent->NPC)
								{
									if ((ent->client->ps.stats[STAT_ARMOR] + shield_amount) < max_shield)
									{
										ent->client->ps.stats[STAT_ARMOR] += shield_amount;
									}
									else
									{
										ent->client->ps.stats[STAT_ARMOR] = max_shield;
									}
								}
							}
						}
						else
						{
							int heal_amount = 6;

							// zyk: Universe Power
							if (quest_power_user->client->pers.quest_power_status & (1 << 13))
							{
								heal_amount += 2;
							}

							if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
								ent->health += heal_amount;
							else
								ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
						}
					}
				}
				
				if (attacker && ent && level.special_power_effects[attacker->s.number] != -1 && level.special_power_effects[attacker->s.number] != ent->s.number)
				{ // zyk: if it is an effect used by special power, then attacker must be the owner of the effect. Also, do not hit the owner
					gentity_t *quest_power_user = &g_entities[level.special_power_effects[attacker->s.number]];

					if (ent && ent->client &&
						Q_stricmp(attacker->targetname, "zyk_effect_scream") != 0 &&
						Q_stricmp(attacker->targetname, "zyk_timed_bomb_explosion") != 0 &&
						Q_stricmp(attacker->targetname, "zyk_vertical_dfa") != 0 &&
						Q_stricmp(attacker->targetname, "zyk_force_storm") != 0 &&
						((Q_stricmp(attacker->targetname, "zyk_quest_effect_healing") != 0 && zyk_check_immunity_power(ent)) || 
						 (Q_stricmp(attacker->targetname, "zyk_quest_effect_healing") == 0 && zyk_is_ally(quest_power_user, ent) == qfalse && zyk_check_immunity_power(ent))))
					{ // zyk: do not hit enemies using Immunity Power if the effect is not from some unique abilities
						continue;
					}

					// zyk: if the power user and the target are allies (player or npc), then do not hit
					if (quest_power_user && quest_power_user->client && ent && ent->client &&
						(OnSameTeam(quest_power_user, ent) == qtrue || npcs_on_same_team(quest_power_user, ent) == qtrue))
					{
						continue;
					}

					if (zyk_is_ally(quest_power_user, ent) == qtrue)
					{
						continue;
					}

					if (quest_power_user && quest_power_user->client && ent && ent->client && 
						quest_power_user->client->pers.guardian_mode != ent->client->pers.guardian_mode &&
						!(quest_power_user->NPC && quest_power_user->client->pers.guardian_mode == 0) && 
						!(!quest_power_user->NPC && quest_power_user->client->pers.guardian_mode > 0 && ent->NPC))
					{ // zyk: validating boss battles
						continue;
					}

					if (Q_stricmp(attacker->targetname, "zyk_quest_effect_drain") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_quest_effect_watersplash") == 0)
					{ // zyk: Ultra Drain heals the power user
						if (quest_power_user && quest_power_user->client && quest_power_user->health > 0 && 
							zyk_can_hit_target(quest_power_user, ent) == qtrue && zyk_is_ally(quest_power_user, ent) == qfalse && ent->health > 0)
						{
							int heal_amount = (int)points;

							if ((quest_power_user->health + heal_amount) < quest_power_user->client->ps.stats[STAT_MAX_HEALTH])
								quest_power_user->health += heal_amount;
							else
								quest_power_user->health = quest_power_user->client->ps.stats[STAT_MAX_HEALTH];
						}
					}

					// zyk: target will not be knocked back by Rockfall, Dome of Damage, Ultra Flame, Ultra Drain, Water Splash, 
					// Acid Water, Flaming Area, Healing Area, Vertical DFA and Force Storm
					if (Q_stricmp(attacker->targetname, "zyk_quest_effect_rockfall") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_quest_effect_watersplash") == 0 ||
						Q_stricmp(attacker->targetname, "zyk_quest_effect_dome") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_quest_effect_flame") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_quest_effect_flaming_area") == 0 ||
						Q_stricmp(attacker->targetname, "zyk_quest_effect_acid") == 0 ||
						Q_stricmp(attacker->targetname, "zyk_quest_effect_drain") == 0 ||
						Q_stricmp(attacker->targetname, "zyk_quest_effect_healing") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_vertical_dfa") == 0 || 
						Q_stricmp(attacker->targetname, "zyk_force_storm") == 0)
					{
						if (Q_stricmp(attacker->targetname, "zyk_quest_effect_flaming_area") == 0 && quest_power_user && quest_power_user != ent && ent->client)
						{ // zyk: if player touches the flame, will keep catching fire for some seconds
							ent->client->pers.quest_power_status |= (1 << 23);
							ent->client->pers.quest_power_user5_id = quest_power_user->s.number;
							ent->client->pers.quest_power_hit4_counter = 15;
							ent->client->pers.quest_target8_timer = level.time + 200;
						}

						G_Damage (ent, quest_power_user, quest_power_user, NULL, origin, (int)points, DAMAGE_RADIUS, mod);
					}
					else if (Q_stricmp(attacker->targetname, "zyk_effect_scream") == 0)
					{ // zyk: it will also not knockback by Force Scream ability
						if (ent->client && Q_irand(0, 3) == 0 && zyk_unique_ability_can_hit_target(quest_power_user, ent) == qtrue)
						{ // zyk: it has a chance of setting a stun anim on the target
							ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
							ent->client->ps.forceDodgeAnim = BOTH_SONICPAIN_END;
							ent->client->ps.forceHandExtendTime = level.time + 3000;
						}

						G_Damage(ent, quest_power_user, quest_power_user, NULL, origin, (int)points, DAMAGE_RADIUS, mod);
					}
					else
					{
						G_Damage (ent, quest_power_user, quest_power_user, dir, origin, (int)points, DAMAGE_RADIUS, mod);
					}
				}
				else if (!attacker || level.special_power_effects[attacker->s.number] == -1)
				{
					G_Damage (ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
				}
			}

			if (ent && ent->client && roastPeople && missile &&
				!VectorCompare(ent->r.currentOrigin, missile->r.currentOrigin))
			{ //the thing calling this function can create burn marks on people, so create an event to do so
				gentity_t *evEnt = G_TempEntity(ent->r.currentOrigin, EV_GHOUL2_MARK);

				evEnt->s.otherEntityNum = ent->s.number; //the entity the mark should be placed on
				evEnt->s.weapon = WP_ROCKET_LAUNCHER; //always say it's rocket so we make the right mark

				//Try to place the decal by going from the missile location to the location of the person that was hit
				VectorCopy(missile->r.currentOrigin, evEnt->s.origin);
				VectorCopy(ent->r.currentOrigin, evEnt->s.origin2);

				//it's hacky, but we want to move it up so it's more likely to hit
				//the torso.
				if (missile->r.currentOrigin[2] < ent->r.currentOrigin[2])
				{ //move it up less so the decal is placed lower on the model then
					evEnt->s.origin2[2] += 8;
				}
				else
				{
					evEnt->s.origin2[2] += 24;
				}

				//Special col check
				evEnt->s.eventParm = 1;
			}
		}
	}

	return hitClient;
}
