/*
===========================================================================
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

#include "b_local.h"
#include "w_saber.h"
#include "ai_main.h"
#include "ghoul2/G2.h"

#define METROID_JUMP 1

//NEEDED FOR MIND-TRICK on NPCS=========================================================
extern void NPC_PlayConfusionSound( gentity_t *self );
extern void NPC_Jedi_PlayConfusionSound( gentity_t *self );
extern void NPC_UseResponse( gentity_t *self, gentity_t *user, qboolean useWhenDone );
//NEEDED FOR MIND-TRICK on NPCS=========================================================
extern void Jedi_Decloak( gentity_t *self );
extern void Jedi_DecloakPair( gentity_t *self );
extern qboolean Jedi_PairIsCloaked( gentity_t *self );

// GalaxyRP fix: [Cloak Item] an attack that does not go through a weapon still breaks cloak. Cloak
// already came down on weapon fire (FireWeapon, g_weapon.c) and on a saber swing (EV_SABER_ATTACK,
// g_active.c), but the offensive Force powers, a saber throw and a kick announce themselves through
// neither, so a cloaked player could lightning, drain, grip, push, pull, throw or kick while staying
// invisible. Every such attack calls this at the moment it happens (w_force.c for the powers,
// w_saber.c for the throw and the kick).
//
// Pair-aware for the same reason as the other hooks: riders can use the Force from the saddle (zyk
// removed the vehicle block in BG_CanUseFPNow, bg_misc.c), so a rider's attack takes a cloaked
// vehicle down with them even when the rider is not the cloaked half.
//
// Players only. The one NPC that cloaks, the Shadowtrooper, has its cloak driven every think by
// Jedi_CheckCloak (NPC_AI_Jedi.c) from its saber state, so dropping it here would only be undone on
// the next think -- a decloak/cloak sound pair every frame of a held grip or lightning.
//
// Like the weapon and saber hooks, and unlike taking damage in G_Damage, this sets no re-cloak
// lockout: the attacker only loses the cloak, the toggle's own cooldown still applies, and the held
// powers call this on every tick they run, so re-cloaking mid-attack does not stick.
void RP_AttackBreaksCloak( gentity_t *self )
{
	if ( !self || !self->client || self->NPC )
	{
		return;
	}

	if ( Jedi_PairIsCloaked( self ) )
	{
		Jedi_DecloakPair( self );
	}
}

extern qboolean BG_FullBodyTauntAnim( int anim );

extern bot_state_t *botstates[MAX_CLIENTS];

int		speedLoopSound		= 0;
int		rageLoopSound		= 0;
int		protectLoopSound	= 0;
int		absorbLoopSound		= 0;
int		seeLoopSound		= 0;
int		ysalamiriLoopSound	= 0;

#define FORCE_VELOCITY_DAMAGE 0

int ForceShootDrain( gentity_t *self );

gentity_t *G_PreDefSound(vec3_t org, int pdSound)
{
	gentity_t	*te;

	te = G_TempEntity( org, EV_PREDEFSOUND );
	te->s.eventParm = pdSound;
	VectorCopy(org, te->s.origin);

	return te;
}
const int forcePowerMinRank[NUM_FORCE_POWER_LEVELS][NUM_FORCE_POWERS] = //0 == neutral
{
	{
		999,//FP_HEAL,//instant
		999,//FP_LEVITATION,//hold/duration
		999,//FP_SPEED,//duration
		999,//FP_PUSH,//hold/duration
		999,//FP_PULL,//hold/duration
		999,//FP_TELEPATHY,//instant
		999,//FP_GRIP,//hold/duration
		999,//FP_LIGHTNING,//hold/duration
		999,//FP_RAGE,//duration
		999,//FP_PROTECT,//duration
		999,//FP_ABSORB,//duration
		999,//FP_TEAM_HEAL,//instant
		999,//FP_TEAM_FORCE,//instant
		999,//FP_DRAIN,//hold/duration
		999,//FP_SEE,//duration
		999,//FP_SABER_OFFENSE,
		999,//FP_SABER_DEFENSE,
		999//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		0,//FP_SABER_OFFENSE,
		0,//FP_SABER_DEFENSE,
		0//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		5,//FP_SABER_OFFENSE,
		5,//FP_SABER_DEFENSE,
		5//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		10,//FP_SABER_OFFENSE,
		10,//FP_SABER_DEFENSE,
		10//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	}
};

const int mindTrickTime[NUM_FORCE_POWER_LEVELS] =
{
	0,//none
	5000,
	10000,
	15000,
	// GalaxyRP fix: [Force] this table is declared [NUM_FORCE_POWER_LEVELS] (6) but only ever
	// supplied four initializers, so levels 4 and 5 were implicitly 0. It is indexed by
	// forcePowerLevel[FP_TELEPATHY] when setting an NPC's charmedTime/confusionTime, so Mind Trick
	// at level 4 or 5 set the expiry to level.time + 0 and wore off the same frame it was applied --
	// mind trick did nothing at all to NPCs. Continuing the existing 5000ms-per-level progression.
	20000,
	25000
};

// GalaxyRP fix: [Force] claim the six force loop sounds at map load instead of waiting for the
// first player to connect. Upstream registers them lazily inside WP_InitForcePowers(), which is
// fine until the sound table or the gamestate byte budget is under pressure: G_SoundIndex() answers
// 0 when a registration is refused, and 0 is also what these variables hold when nothing has been
// registered, so a refusal is indistinguishable from "not tried yet". Every later call then walks
// the whole table again looking for a name that will never be in it, and meanwhile Speed, Rage,
// Protect, Absorb, See and Ysalamiri all play their loop silently. Registering here, before a
// single map entity has asked for a sound, means these six are in the table whenever the table
// has room for them at all.
//
// Safe to do unconditionally: the game module is unloaded and reloaded on every map change, so
// these variables start at 0 again each map and never carry a stale index across.
void WP_RegisterForceLoopSounds( void ) {
	speedLoopSound		= G_SoundIndex( "sound/weapons/force/speedloop.wav" );
	rageLoopSound		= G_SoundIndex( "sound/weapons/force/rageloop.wav" );
	absorbLoopSound		= G_SoundIndex( "sound/weapons/force/absorbloop.wav" );
	protectLoopSound	= G_SoundIndex( "sound/weapons/force/protectloop.wav" );
	seeLoopSound		= G_SoundIndex( "sound/weapons/force/seeloop.wav" );
	ysalamiriLoopSound	= G_SoundIndex( "sound/player/nullifyloop.wav" );
}

void WP_InitForcePowers( gentity_t *ent ) {
	int i, i_r, lastFPKnown = -1;
	qboolean warnClient = qfalse, warnClientLimit = qfalse, didEvent = qfalse;
	char userinfo[MAX_INFO_STRING], forcePowers[DEFAULT_FORCEPOWERS_LEN+1], readBuf[DEFAULT_FORCEPOWERS_LEN+1];
	// zyk: if gametype is Duel or Power Duel, uses g_duelForcePowerDisable instead
	// GalaxyRP fix: [Force] moved into G_ForcePowerDisableValue() (g_main.c) so the other readers of
	// g_forcePowerDisable get the same duel-aware answer this function already did.
	int force_powers_disabled = G_ForcePowerDisableValue();

	// if server has no max rank, default to max (50)
	if ( g_maxForceRank.integer <= 0 || g_maxForceRank.integer >= NUM_FORCE_MASTERY_LEVELS ) {
		// ack, prevent user from being dumb
		trap->Cvar_Set( "g_maxForceRank", va( "%i", FORCE_MASTERY_JEDI_MASTER ) );
		trap->Cvar_Update( &g_maxForceRank );
	}

	if ( !ent || !ent->client )
		return;

	ent->client->ps.fd.saberAnimLevel = ent->client->sess.saberLevel;

	if ( ent->client->ps.fd.saberAnimLevel < FORCE_LEVEL_1 || ent->client->ps.fd.saberAnimLevel > FORCE_LEVEL_3 )
		ent->client->ps.fd.saberAnimLevel = FORCE_LEVEL_1;

	// so that the client configstring is already modified with this when we need it
	// GalaxyRP fix: [Force] these six are registered up front by WP_RegisterForceLoopSounds() from
	// G_InitGame() now, so by the time anyone reaches this they are already set and nothing below
	// runs. Kept as a fallback rather than deleted: it costs nothing, and it is what upstream does.
	if ( !speedLoopSound )
		speedLoopSound = G_SoundIndex( "sound/weapons/force/speedloop.wav" );
	if ( !rageLoopSound )
		rageLoopSound = G_SoundIndex( "sound/weapons/force/rageloop.wav" );
	if ( !absorbLoopSound )
		absorbLoopSound = G_SoundIndex( "sound/weapons/force/absorbloop.wav" );
	if ( !protectLoopSound )
		protectLoopSound = G_SoundIndex( "sound/weapons/force/protectloop.wav" );
	if ( !seeLoopSound )
		seeLoopSound = G_SoundIndex( "sound/weapons/force/seeloop.wav" );
	if ( !ysalamiriLoopSound )
		ysalamiriLoopSound = G_SoundIndex( "sound/player/nullifyloop.wav" );

	if ( ent->s.eType == ET_NPC )
		return;

	for ( i=0; i<NUM_FORCE_POWERS; i++ ) {
		ent->client->ps.fd.forcePowerLevel[i] = 0;
		ent->client->ps.fd.forcePowersKnown &= ~(1<<i);
	}

	ent->client->ps.fd.forcePowerSelected = -1;
	ent->client->ps.fd.forceSide = 0;

	// if in siege, then use the powers for this class, and skip all this nonsense.
	if ( level.gametype == GT_SIEGE && ent->client->siegeClass != -1 ) {
		for ( i=0; i<NUM_FORCE_POWERS; i++ ) {
			ent->client->ps.fd.forcePowerLevel[i] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[i];
			if ( !ent->client->ps.fd.forcePowerLevel[i] )
				ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			else
				ent->client->ps.fd.forcePowersKnown |= (1 << i);
		}

		// bring up the class selection menu
		if ( !ent->client->sess.setForce )
			trap->SendServerCommand( ent-g_entities, "scl" );
		ent->client->sess.setForce = qtrue;

		return;
	}

	//rwwFIXMEFIXME: Temp
	if ( ent->s.eType == ET_NPC && ent->s.number >= MAX_CLIENTS )
		Q_strncpyz( userinfo, "forcepowers\\7-1-333003000313003120", sizeof( userinfo ) );
	else
		trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );

	Q_strncpyz( forcePowers, Info_ValueForKey( userinfo, "forcepowers" ), sizeof( forcePowers ) );

	if ( strlen( forcePowers ) != DEFAULT_FORCEPOWERS_LEN ) {
		Q_strncpyz( forcePowers, DEFAULT_FORCEPOWERS, sizeof( forcePowers ) );
		trap->SendServerCommand( ent-g_entities, "print \"" S_COLOR_RED "Invalid forcepowers string, setting default\n\"" );
	}

	//if it's a bot just copy the info directly from its personality
	if ( (ent->r.svFlags & SVF_BOT) && botstates[ent->s.number] )
		Q_strncpyz( forcePowers, botstates[ent->s.number]->forceinfo, sizeof( forcePowers ) );

	if ( g_forceBasedTeams.integer ) {
		if ( ent->client->sess.sessionTeam == TEAM_RED )
			warnClient = !(BG_LegalizedForcePowers( forcePowers, sizeof (forcePowers), g_maxForceRank.integer, HasSetSaberOnly(), FORCE_DARKSIDE, level.gametype, force_powers_disabled ));
		else if ( ent->client->sess.sessionTeam == TEAM_BLUE )
			warnClient = !(BG_LegalizedForcePowers( forcePowers, sizeof (forcePowers), g_maxForceRank.integer, HasSetSaberOnly(), FORCE_LIGHTSIDE, level.gametype, force_powers_disabled ));
		else
			warnClient = !(BG_LegalizedForcePowers( forcePowers, sizeof (forcePowers), g_maxForceRank.integer, HasSetSaberOnly(), 0, level.gametype, force_powers_disabled ));
	}
	else
		warnClient = !(BG_LegalizedForcePowers( forcePowers, sizeof (forcePowers), g_maxForceRank.integer, HasSetSaberOnly(), 0, level.gametype, force_powers_disabled ));

	//rww - parse through the string manually and eat out all the appropriate data
	i = 0;
	i_r = 0;
	while ( forcePowers[i] && forcePowers[i] != '-' ) {
		readBuf[i_r] = forcePowers[i];
		i_r++;
		i++;
	}
	readBuf[i_r] = 0;
	//THE RANK
	ent->client->ps.fd.forceRank = atoi( readBuf );
	i++;

	i_r = 0;
	while ( forcePowers[i] && forcePowers[i] != '-' ) {
		readBuf[i_r] = forcePowers[i];
		i_r++;
		i++;
	}
	readBuf[i_r] = 0;
	//THE SIDE
	ent->client->ps.fd.forceSide = atoi( readBuf );
	i++;

	if ( level.gametype != GT_SIEGE && (ent->r.svFlags & SVF_BOT) && botstates[ent->s.number] ) {
		// hmm..I'm going to cheat here.
		int oldI = i;
		i_r = 0;
		while ( forcePowers[i] && forcePowers[i] != '\n' && i_r < NUM_FORCE_POWERS ) {
			if ( ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE ) {
				if ( i_r == FP_ABSORB )
					forcePowers[i] = '3';
				if ( botstates[ent->s.number]->settings.skill >= 4 ) {
					// cheat and give them more stuff
					if ( i_r == FP_HEAL )
						forcePowers[i] = '3';
					else if ( i_r == FP_PROTECT )
						forcePowers[i] = '3';
				}
			}
			else if ( ent->client->ps.fd.forceSide == FORCE_DARKSIDE ) {
				if ( botstates[ent->s.number]->settings.skill >= 4 ) {
					if ( i_r == FP_GRIP )
						forcePowers[i] = '3';
					else if ( i_r == FP_LIGHTNING )
						forcePowers[i] = '3';
					else if ( i_r == FP_RAGE )
						forcePowers[i] = '3';
					else if ( i_r == FP_DRAIN )
						forcePowers[i] = '3';
				}
			}

			if ( i_r == FP_PUSH )
				forcePowers[i] = '3';
			else if ( i_r == FP_PULL )
				forcePowers[i] = '3';

			i++;
			i_r++;
		}
		i = oldI;
	}

	i_r = 0;
	while (forcePowers[i] && forcePowers[i] != '\n' &&
		i_r < NUM_FORCE_POWERS)
	{
		readBuf[0] = forcePowers[i];
		readBuf[1] = 0;

		ent->client->ps.fd.forcePowerLevel[i_r] = atoi(readBuf);
		if (ent->client->ps.fd.forcePowerLevel[i_r])
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << i_r);
		}
		else
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i_r);
		}
		i++;
		i_r++;
	}
	//THE POWERS

	if ( ent->s.eType != ET_NPC ) {
		if ( HasSetSaberOnly() ) {
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FREE_SABER );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 1;
		}
		else {
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FREE_SABER );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 0;
		}

		if ( force_powers_disabled ) {
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FORCE_DISABLE );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 1;
		}
		else {
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FORCE_DISABLE );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 0;
		}
	}

	if ( ent->s.eType == ET_NPC )
		ent->client->sess.setForce = qtrue;
	else if ( level.gametype == GT_SIEGE ) {
		if ( !ent->client->sess.setForce ) {
			ent->client->sess.setForce = qtrue;
			// bring up the class selection menu
			trap->SendServerCommand( ent-g_entities, "scl" );
		}
	}
	else {
		if ( warnClient || !ent->client->sess.setForce ) {
			// the client's rank is too high for the server and has been autocapped, so tell them
			if ( level.gametype != GT_HOLOCRON && level.gametype != GT_JEDIMASTER ) {
				didEvent = qtrue;

				if ( !(ent->r.svFlags & SVF_BOT) && ent->s.eType != ET_NPC ) {
					if ( !g_teamAutoJoin.integer ) {
						// make them a spectator so they can set their powerups up without being bothered.
						ent->client->sess.sessionTeam = TEAM_SPECTATOR;
						ent->client->sess.spectatorState = SPECTATOR_FREE;
						ent->client->sess.spectatorClient = 0;

						ent->client->pers.teamState.state = TEAM_BEGIN;
						trap->SendServerCommand( ent-g_entities, "spc" ); // Fire up the profile menu
					}
				}

				// event isn't very reliable, I made it a string. This way I can send it to just one client also,
				//	as opposed to making a broadcast event.
				trap->SendServerCommand( ent->s.number, va( "nfr %i %i %i", g_maxForceRank.integer, 1, ent->client->sess.sessionTeam ) );
				// arg1 is new max rank, arg2 is non-0 if force menu should be shown, arg3 is the current team
			}
			ent->client->sess.setForce = qtrue;
		}

		if ( !didEvent )
			trap->SendServerCommand( ent->s.number, va( "nfr %i %i %i", g_maxForceRank.integer, 0, ent->client->sess.sessionTeam ) );

		// the server has one or more force powers disabled and the client is using them in his config
		if ( warnClientLimit )
			trap->SendServerCommand( ent-g_entities, va( "print \"The server has one or more force powers that you have chosen disabled.\nYou will not be able to use the disable force power(s) while playing on this server.\n\"" ) );
	}

	for ( i=0; i<NUM_FORCE_POWERS; i++ ) {
		if ( (ent->client->ps.fd.forcePowersKnown & (1 << i)) && !ent->client->ps.fd.forcePowerLevel[i] )
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		else if ( i != FP_LEVITATION && i != FP_SABER_OFFENSE && i != FP_SABER_DEFENSE && i != FP_SABERTHROW )
			lastFPKnown = i;
	}

	if ( ent->client->ps.fd.forcePowersKnown & ent->client->sess.selectedFP )
		ent->client->ps.fd.forcePowerSelected = ent->client->sess.selectedFP;

	if ( !(ent->client->ps.fd.forcePowersKnown & (1 << ent->client->ps.fd.forcePowerSelected)) ) {
		if ( lastFPKnown != -1 )
			ent->client->ps.fd.forcePowerSelected = lastFPKnown;
		else
			ent->client->ps.fd.forcePowerSelected = 0;
	}

	for ( /*i=0*/; i<NUM_FORCE_POWERS; i++ )
		ent->client->ps.fd.forcePowerBaseLevel[i] = ent->client->ps.fd.forcePowerLevel[i];
	ent->client->ps.fd.forceUsingAdded = 0;
}

void WP_SpawnInitForcePowers( gentity_t *ent )
{
	int i = 0;

	ent->client->ps.saberAttackChainCount = 0;

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}

		i++;
	}

	ent->client->ps.fd.forceDeactivateAll = 0;

	// zyk: set max force power for non-rpg mode users
	if (ent->client->sess.amrpgmode < 2)
		ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax = RP_MAX_FORCE_POWER_LOGGED_OUT;

	ent->client->ps.fd.forcePowerRegenDebounceTime = level.time;
	ent->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
	ent->client->ps.fd.forceMindtrickTargetIndex = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex2 = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex3 = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex4 = 0;

	// GalaxyRP fix: [Dead Code] rpg_class permanently 0, mind control block unreachable
	ent->client->ps.holocronBits = 0;

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.holocronsCarried[i] = 0;
		i++;
	}

	if (level.gametype == GT_HOLOCRON)
	{
		i = 0;
		while (i < NUM_FORCE_POWERS)
		{
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			i++;
		}

		if (HasSetSaberOnly())
		{
			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] < FORCE_LEVEL_1)
			{
				ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = FORCE_LEVEL_1;
			}
			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
			{
				ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = FORCE_LEVEL_1;
			}
		}
	}

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.fd.forcePowerDebounce[i] = 0;
		ent->client->ps.fd.forcePowerDuration[i] = 0;

		i++;
	}

	ent->client->ps.fd.forceJumpZStart = 0;
	ent->client->ps.fd.forceJumpCharge = 0;
	ent->client->ps.fd.forceJumpSound = 0;
	ent->client->ps.fd.forceGripDamageDebounceTime = 0;
	ent->client->ps.fd.forceGripBeingGripped = 0;
	ent->client->ps.fd.forceGripCripple = 0;
	ent->client->ps.fd.forceGripUseTime = 0;
	ent->client->ps.fd.forceGripSoundTime = 0;
	ent->client->ps.fd.forceGripStarted = 0;
	ent->client->ps.fd.forceHealTime = 0;
	ent->client->ps.fd.forceHealAmount = 0;
	ent->client->ps.fd.forceRageRecoveryTime = 0;
	ent->client->ps.fd.forceDrainEntNum = ENTITYNUM_NONE;
	ent->client->ps.fd.forceDrainTime = 0;

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		if ((ent->client->ps.fd.forcePowersKnown & (1 << i)) &&
			!ent->client->ps.fd.forcePowerLevel[i])
		{ //make sure all known powers are cleared if we have level 0 in them
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		}

		i++;
	}

	if (level.gametype == GT_SIEGE &&
		ent->client->siegeClass != -1)
	{ //Then use the powers for this class.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			ent->client->ps.fd.forcePowerLevel[i] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[i];

			if (!ent->client->ps.fd.forcePowerLevel[i])
			{
				ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			}
			else
			{
				ent->client->ps.fd.forcePowersKnown |= (1 << i);
			}
			i++;
		}
	}
}

extern qboolean BG_InKnockDown( int anim ); //bg_pmove.c
extern qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target);

// DAJ_RP: [Force] who Grip may not take hold of: any vehicle (fighter, walker, speeder or animal, empty or
// ridden) and the three NPC classes Push and Pull already refuse in ForceThrow() below -- Galak's mech, the
// non-vehicle AT-ST and the Rancor. Stock refused every force power but Lightning on vehicles (the block
// commented out in ForcePowerUsableOn()); zyk opened them all up, which let a player lift and choke a
// fighter or a walker. The other powers stay open on vehicles; Grip alone is refused again. Asked by
// ForcePowerUsableOn() for every caster -- player, NPC Jedi or bot -- and by the NPC Jedi's own grip
// decisions (NPC_AI_Jedi.c), so the AI picks something else instead of a grip that cannot land.
qboolean RP_GripRefused( const gentity_t *target )
{
	if (!target || !target->client)
	{
		return qfalse;
	}

	if (target->s.eType == ET_NPC && target->s.NPC_class == CLASS_VEHICLE)
	{
		return qtrue;
	}

	if (target->client->NPC_class == CLASS_GALAKMECH ||
		target->client->NPC_class == CLASS_ATST ||
		target->client->NPC_class == CLASS_RANCOR)
	{
		return qtrue;
	}

	return qfalse;
}

int ForcePowerUsableOn(gentity_t *attacker, gentity_t *other, forcePowers_t forcePower)
{
	if (other && other->client && BG_HasYsalamiri(level.gametype, &other->client->ps))
	{
		return 0;
	}

	// zyk: cannot hit players in boss battles and vice-versa
	if (zyk_can_hit_target(attacker, other) == qfalse)
	{
		return 0;
	}

	// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, boss-battle check unreachable

	// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Force User Unique Skill / Force Guardian Force Armor checks unreachable

	// GalaxyRP fix: [Settings] a guard used to sit here, blocking a hostile force power between allies
	// when the target had player_settings bit 6 set (the old /settings 2, "Allow Force Powers from
	// allies", set to OFF). That per-player choice has been removed (see the fix comment on
	// settings_number_to_bit in Cmd_Settings_f) -- allies never block each other's force powers now,
	// which is what this guard already did by default whenever the setting was left at ON.

	// GalaxyRP fix: [Quests] removed the "special quest npcs that cannot be hit by force" guard here
	// (universe_quest_messages == -2000) — that sentinel value is never assigned anywhere in the
	// codebase (the only remaining assignment is a reset to 0), so this was permanently dead.

	if (attacker && attacker->client && !BG_CanUseFPNow(level.gametype, &attacker->client->ps, level.time, forcePower))
	{
		return 0;
	}

	// GalaxyRP fix: [Dueling] the other half of the vanilla restriction, live again, and the half
	// that matters most. BG_CanUseFPNow() above decides whether a power may be ACTIVATED; this pair
	// decides who it may be used ON, and with both of them commented out a duel stopped being sealed
	// off from the rest of the map. A duellist could Grip, Mind Trick or Push a bystander who had
	// nothing to do with the fight, and a bystander could do the same back -- Push in particular can
	// shove a duellist past rp_duel_radius and end a duel the pusher is not even part of. G_Damage
	// blocks the damage in both directions, but these powers do their real work through effects that
	// never reach G_Damage: the grip hold, the trick, the knockback.
	//
	// OpenJK, TaystJK and JA++ all keep this sealed, and the two with full-force duels keep it sealed
	// THERE too -- a force duellist may use powers on their own opponent and on nobody else. The
	// shape below is TaystJK's: the ONLY opening is both players duelling, the duel being a full
	// force one, and the target being this attacker's own duelIndex. Every other combination, in
	// either direction, still refuses.
	if (attacker && attacker->client && attacker->client->ps.duelInProgress)
	{
		if (!(other && other->client && other->client->ps.duelInProgress &&
			attacker->s.number < MAX_CLIENTS &&
			level.duel_types[attacker->s.number] == 1 &&
			other->s.number == attacker->client->ps.duelIndex))
		{
			return 0;
		}
	}
	else if (other && other->client && other->client->ps.duelInProgress)
	{ // the attacker is not duelling, so a duellist is never a valid target for them
		return 0;
	}

	if (forcePower == FP_GRIP)
	{
		if (RP_GripRefused(other))
		{ // DAJ_RP: [Force] no gripping vehicles, Galak's mech, the AT-ST or the Rancor (see RP_GripRefused())
			return 0;
		}
		else if (other && other->client &&
			(other->client->ps.fd.forcePowersActive & (1<<FP_ABSORB)))
		{ //don't allow gripping to begin with if they are absorbing
			//play sound indicating that attack was absorbed
			if (other->client->forcePowerSoundDebounce < level.time)
			{
				gentity_t *abSound = G_PreDefSound(other->client->ps.origin, PDSOUND_ABSORBHIT);
				abSound->s.trickedentindex = other->s.number;
				other->client->forcePowerSoundDebounce = level.time + 400;
			}
			return 0;
		}
		else if (other && other->client &&
			other->client->ps.weapon == WP_SABER &&
			BG_SaberInSpecial(other->client->ps.saberMove))
		{ //don't grip person while they are in a special or some really bad things can happen.
			return 0;
		}
	}

	/* zyk: now it is possible to use Push or Pull at knocked down clients
	if (other && other->client &&
		(forcePower == FP_PUSH ||
		forcePower == FP_PULL))
	{
		if (BG_InKnockDown(other->client->ps.legsAnim))
		{
			return 0;
		}
	}
	*/

	// zyk: now force powers can be used on vehicles, so changed from 0 to 1
	/*
	if (other && other->client && other->s.eType == ET_NPC &&
		other->s.NPC_class == CLASS_VEHICLE)
	{ //can't use the force on vehicles.. except lightning
		if (forcePower == FP_LIGHTNING)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	*/

	if (other && other->client && other->s.eType == ET_NPC &&
		level.gametype == GT_SIEGE)
	{ //can't use powers at all on npc's normally in siege...
		return 0;
	}

	return 1;
}

// GalaxyRP fix: [Force] Force Heal and Shield Heal used to cost a flat half of the server-wide
// force maximum. That worked out to 100, but a logged-in character's pool is only a fifth of that
// maximum per level of the Force Power skill (skill index 54) -- so at skill level 1 the whole pool
// was 40 and neither heal could EVER be cast, and at level 2 it was affordable only from a
// completely full bar. Costing half of the caster's OWN maximum instead means every character gets
// exactly two heals from a full pool, at every skill level, and nothing is unreachable. Logged-out
// players are unaffected in kind: their cost is likewise half of their own maximum, which is
// RP_MAX_FORCE_POWER_LOGGED_OUT, so they too get exactly two heals from a full pool.
//
// The floor of 1 matters: forcePowerMax is legitimately 0 for a character with no levels in Force
// Power. A cost of 0 would hit the "if ( !drain ) return qtrue;" shortcut in WP_ForcePowerAvailable
// below and hand those players unlimited free heals.
static int RP_ForceHealCost( gentity_t *self )
{
	int cost = self->client->ps.fd.forcePowerMax / 2;

	return (cost > 0) ? cost : 1;
}

// GalaxyRP fix: [Force] WP_ForcePowerStart()'s overrideAmt means different things to different
// powers. For most it is a force COST; for FP_DRAIN, FP_LIGHTNING and FP_SPEED the case arms
// instead read it as a DURATION ("if (overrideAmt) duration = overrideAmt;"). Drain and Lightning
// were given an escape hatch in WP_ForcePowerAvailable() so their duration is never mistaken for a
// price, but Speed never was -- so Jedi_DodgeEvasion()'s ForceSpeed(self, 500) asked the gate for
// 500 force. The pool tops out at RP_MAX_FORCE_POWER, so that check could never pass and the
// force-dodge under g_forceDodge 2 never actually granted Speed, while still arming the shared
// deactivate lock and starting the looping speed sound.
//
// The real cost is the one WP_ForcePowerStart() goes on to charge: overrideAmt * 0.025 (12 force
// for the 500ms dodge). Both sites call this helper so the gate and the charge cannot drift apart
// again. The floor of 1 matters because BG_ForcePowerDrain() treats a drain of 0 as "look the cost
// up in the table" -- without it, a short enough duration would truncate to 0, sail through the
// "if (!drain) return qtrue" shortcut as free, and then be charged the full table price of 50.
static int RP_ForceSpeedCost( int overrideAmt )
{
	int cost = (int)(overrideAmt * 0.025);

	return (cost > 0) ? cost : 1;
}

qboolean WP_ForcePowerAvailable( gentity_t *self, forcePowers_t forcePower, int overrideAmt )
{
	int	drain = overrideAmt ? overrideAmt :
				forcePowerNeeded[self->client->ps.fd.forcePowerLevel[forcePower]][forcePower];

	if (forcePower == FP_HEAL) // zyk: added the HEAL condition to keep balance
	{
		drain = RP_ForceHealCost( self );
	}
	// GalaxyRP fix: [Force] for Speed an overrideAmt is a duration, not a price -- charge what
	// WP_ForcePowerStart() will actually take for it rather than the raw millisecond count.
	if (forcePower == FP_SPEED && overrideAmt)
	{
		drain = RP_ForceSpeedCost( overrideAmt );
	}
	// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Force User drain-reduction branch unreachable

	if (self->client->ps.fd.forcePowersActive & (1 << forcePower))
	{ //we're probably going to deactivate it..
		return qtrue;
	}
	if ( forcePower == FP_LEVITATION )
	{
		return qtrue;
	}
	if ( !drain )
	{
		return qtrue;
	}
	if ((forcePower == FP_DRAIN || forcePower == FP_LIGHTNING) &&
		self->client->ps.fd.forcePower >= 25)
	{ //it's ok then, drain/lightning are actually duration
		return qtrue;
	}
	if ( self->client->ps.fd.forcePower < drain )
	{
		return qfalse;
	}
	return qtrue;
}

qboolean WP_ForcePowerInUse( gentity_t *self, forcePowers_t forcePower )
{
	if ( (self->client->ps.fd.forcePowersActive & ( 1 << forcePower )) )
	{//already using this power
		return qtrue;
	}

	return qfalse;
}

qboolean WP_ForcePowerUsable( gentity_t *self, forcePowers_t forcePower )
{
	if (BG_HasYsalamiri(level.gametype, &self->client->ps))
	{
		return qfalse;
	}

	if (self->health <= 0 || self->client->ps.stats[STAT_HEALTH] <= 0 ||
		(self->client->ps.eFlags & EF_DEAD))
	{
		return qfalse;
	}

	// GalaxyRP fix: [Death System] a downed player cannot START a force power. Every one of the
	// thirteen powers routes through this function, so this single test covers them all and both
	// dispatch paths (the GENCMD_FORCE_* switch in g_active.c and the forcepower switch below).
	// It has to be explicit: paralyze_player() leaves a downed player on RP_DOWNED_HEALTH health, so the dead
	// tests just above all pass, and the only powers that refused were Grip/Lightning/Drain/Mind
	// Trick/Push -- purely because those five also guard on forceHandExtend, which the downed state
	// happens to pin at HANDEXTEND_KNOCKDOWN. Heal, Speed, Rage, Protect, Absorb, Seeing and the two
	// team powers had no such guard, so a downed player could sit out rp_downed_timer healing back to
	// full health (and, with the Shield Heal skill, full shields) and stand up untouched.
	//
	// Deliberately placed AFTER the dead tests and nowhere else: every toggle power (Speed, Rage,
	// Protect, Absorb, Seeing) checks for an already-active power and calls WP_ForcePowerStop BEFORE
	// reaching this function, so a downed player can still switch OFF something that was running when
	// they went down -- only starting is blocked. Same shape as the cloak item's downed guard.
	//
	// NPCs are unaffected: the Death System never downs them (G_Damage tests !targ->NPC) and
	// NPC_spawn.c zeroes player_statuses on every spawned NPC.
	if ( G_PlayerIsDowned( self ) )
	{
		return qfalse;
	}

	if (self->client->ps.pm_flags & PMF_FOLLOW)
	{ //specs can't use powers through people
		return qfalse;
	}
	if (self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		return qfalse;
	}
	if (self->client->tempSpectate >= level.time)
	{
		return qfalse;
	}

	if (!BG_CanUseFPNow(level.gametype, &self->client->ps, level.time, forcePower))
	{
		return qfalse;
	}

	if ( !(self->client->ps.fd.forcePowersKnown & ( 1 << forcePower )) )
	{//don't know this power
		return qfalse;
	}

	// zyk: noclip players cannot use force
	if (self->client->noclip == qtrue)
	{
		return qfalse;
	}

	if ( (self->client->ps.fd.forcePowersActive & ( 1 << forcePower )) )
	{//already using this power
		if (forcePower != FP_LEVITATION)
		{
			return qfalse;
		}
	}

	if (forcePower == FP_LEVITATION && self->client->fjDidJump)
	{
		return qfalse;
	}

	if (!self->client->ps.fd.forcePowerLevel[forcePower])
	{
		return qfalse;
	}

	if ( g_debugMelee.integer )
	{
		if ( (self->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
		{//no offensive force powers when stuck to wall
			switch ( forcePower )
			{
			case FP_GRIP:
			case FP_LIGHTNING:
			case FP_DRAIN:
			case FP_SABER_OFFENSE:
			case FP_SABER_DEFENSE:
			case FP_SABERTHROW:
				return qfalse;
				break;
			default:
				break;
			}
		}
	}

	if ( !self->client->ps.saberHolstered )
	{
		if ( (self->client->saber[0].saberFlags&SFL_TWO_HANDED) )
		{
			if ( g_saberRestrictForce.integer )
			{
				switch ( forcePower )
				{
				case FP_PUSH:
				case FP_PULL:
				case FP_TELEPATHY:
				case FP_GRIP:
				case FP_LIGHTNING:
				case FP_DRAIN:
					return qfalse;
					break;
				default:
					break;
				}
			}
		}

		if ( (self->client->saber[0].saberFlags&SFL_TWO_HANDED)
			|| self->client->saber[0].model[0] )
		{//this saber requires the use of two hands OR our other hand is using an active saber too
			if ( (self->client->saber[0].forceRestrictions&(1<<forcePower)) )
			{//this power is verboten when using this saber
				return qfalse;
			}
		}

		if ( self->client->saber[0].model[0] )
		{//both sabers on
			if ( g_saberRestrictForce.integer )
			{
				switch ( forcePower )
				{
				case FP_PUSH:
				case FP_PULL:
				case FP_TELEPATHY:
				case FP_GRIP:
				case FP_LIGHTNING:
				case FP_DRAIN:
					return qfalse;
					break;
				default:
					break;
				}
			}
			if ( (self->client->saber[1].forceRestrictions&(1<<forcePower)) )
			{//this power is verboten when using this saber
				return qfalse;
			}
		}
	}
	return WP_ForcePowerAvailable( self, forcePower, 0 );	// OVERRIDEFIXME
}

int WP_AbsorbConversion(gentity_t *attacked, int atdAbsLevel, gentity_t *attacker, int atPower, int atPowerLevel, int atForceSpent)
{
	int getLevel = 0;
	int addTot = 0;
	gentity_t *abSound;

	if (atPower != FP_LIGHTNING &&
		atPower != FP_DRAIN &&
		atPower != FP_GRIP &&
		atPower != FP_PUSH &&
		atPower != FP_PULL)
	{ //Only these powers can be absorbed
		return -1;
	}

	if (!atdAbsLevel)
	{ //looks like attacker doesn't have any absorb power
		return -1;
	}

	if (!(attacked->client->ps.fd.forcePowersActive & (1 << FP_ABSORB)))
	{ //absorb is not active
		return -1;
	}

	//Subtract absorb power level from the offensive force power
	getLevel = atPowerLevel;
	getLevel -= atdAbsLevel;

	if (getLevel < 0)
	{
		getLevel = 0;
	}

	//let the attacker absorb an amount of force used in this attack based on his level of absorb
	addTot = (atForceSpent/3)*attacked->client->ps.fd.forcePowerLevel[FP_ABSORB];

	if (addTot < 1 && atForceSpent >= 1)
	{
		addTot = 1;
	}

	// GalaxyRP fix: [Force] this was `== 4`, so Absorb 5 silently lost the bonus that Absorb 4 got
	// while still keeping the level-4 no-upkeep-drain benefit (which tests `< FORCE_LEVEL_4`).
	// Spending the points to go 4 -> 5 was a straight downgrade of 20 force per absorbed hit.
	//
	// GalaxyRP fix: [Force] at most once a second against Lightning and Drain. Those two call this
	// on every 50ms tick of the channel (ForceLightningDamage / ForceDrainDamage) with atForceSpent 1,
	// so the bonus meant to reward absorbing an attack was paid twenty times a second: holding Drain
	// on an Absorb 4+ target filled their pool at about 500 force/s instead of emptying it, which made
	// any Drain user a free refill for an Absorb 4+ ally. The timer is per victim and shared by every
	// attacker, so two drainers do not double it. Push and Pull are single hits with their own
	// cooldowns and keep the bonus per hit; Grip never gets here with Absorb active (ForcePowerUsableOn
	// refuses to grip an absorbing target). The stock "at least 1 per absorbed tick" above is unchanged.
	if (attacked->client->sess.amrpgmode == 2 && attacked->client->pers.skill_levels[8] >= FORCE_LEVEL_4)
	{ // zyk: Absorb 4/4 and above in RPG Mode absorbs more force
		if (atPower != FP_LIGHTNING && atPower != FP_DRAIN)
		{
			addTot = addTot + (RP_MAX_FORCE_POWER/10);
		}
		else if (attacked->client->absorbBonusTime <= level.time)
		{
			addTot = addTot + (RP_MAX_FORCE_POWER/10);
			attacked->client->absorbBonusTime = level.time + 1000;
		}
	}

	attacked->client->ps.fd.forcePower += addTot;

	if (attacked->client->sess.amrpgmode == 2 && attacked->client->ps.fd.forcePower > attacked->client->pers.max_force_power)
		attacked->client->ps.fd.forcePower = attacked->client->pers.max_force_power;
	else if (attacked->client->sess.amrpgmode < 2 && attacked->client->ps.fd.forcePower > RP_MAX_FORCE_POWER_LOGGED_OUT)
		attacked->client->ps.fd.forcePower = RP_MAX_FORCE_POWER_LOGGED_OUT;

	//play sound indicating that attack was absorbed
	if (attacked->client->forcePowerSoundDebounce < level.time)
	{
		abSound = G_PreDefSound(attacked->client->ps.origin, PDSOUND_ABSORBHIT);
		abSound->s.trickedentindex = attacked->s.number;

		attacked->client->forcePowerSoundDebounce = level.time + 400;
	}

	return getLevel;
}

void WP_ForcePowerRegenerate( gentity_t *self, int overrideAmt )
{ //called on a regular interval to regenerate force power.
	if ( !self->client )
	{
		return;
	}

	// zyk: if hit by the Duelist Unique Skill, cannot regen force
	if (self->client->ps.powerups[PW_QUAD] > level.time)
	{
		return;
	}

	if ( overrideAmt )
	{ //custom regen amount
		self->client->ps.fd.forcePower += overrideAmt;
	}
	else
	{ //otherwise, just 1
		self->client->ps.fd.forcePower++;
	}

	if ( self->client->ps.fd.forcePower > self->client->ps.fd.forcePowerMax )
	{ //cap it off at the max (default 100)
		self->client->ps.fd.forcePower = self->client->ps.fd.forcePowerMax;
	}
}

void WP_ForcePowerStart( gentity_t *self, forcePowers_t forcePower, int overrideAmt )
{ //activate the given force power
	int	duration = 0;
	qboolean hearable = qfalse;
	float hearDist = 0;

	if (!WP_ForcePowerAvailable( self, forcePower, overrideAmt ))
	{
		return;
	}

	// GalaxyRP fix: [Cloak Item] the offensive powers break cloak the moment they start -- see
	// RP_AttackBreaksCloak. Mind Trick is deliberately left out: it is not an attack, so it keeps
	// the cloak like the defensive and utility powers do. Grip, Drain and Lightning also
	// break it on every frame they are held, in WP_ForcePowerRun.
	switch ( (int)forcePower )
	{
	case FP_PUSH:
	case FP_PULL:
	case FP_GRIP:
	case FP_LIGHTNING:
	case FP_DRAIN:
		RP_AttackBreaksCloak( self );
		break;
	default:
		break;
	}

	if ( BG_FullBodyTauntAnim( self->client->ps.legsAnim ) )
	{//stop taunt
		self->client->ps.legsTimer = 0;
	}
	if ( BG_FullBodyTauntAnim( self->client->ps.torsoAnim ) )
	{//stop taunt
		self->client->ps.torsoTimer = 0;
	}
	//hearable and hearDist are merely for the benefit of bots, and not related to if a sound is actually played.
	//If duration is set, the force power will assume to be timer-based.
	switch( (int)forcePower )
	{
	case FP_HEAL:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_LEVITATION:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_SPEED:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_1)
		{
			duration = 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_2)
		{
			duration = 15000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_3)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_4)
		{ // zyk: added speed level 4
			duration = 25000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_5)
		{ // GalaxyRP (Alex): [Force Powers] Level 5 for more duration
			duration = 30000;
		}
		else //shouldn't get here
		{
			break;
		}

		if (overrideAmt)
		{
			duration = overrideAmt;
		}

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_PUSH:
		hearable = qtrue;
		hearDist = 256;
		break;
	case FP_PULL:
		hearable = qtrue;
		hearDist = 256;
		break;
	case FP_TELEPATHY:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_2)
		{
			duration = 25000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_3)
		{
			duration = 30000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_4)
		{
			duration = 35000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_5)
		{
			duration = 40000;
		}
		else //shouldn't get here
		{
			break;
		}

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_GRIP:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->ps.powerups[PW_DISINT_4] = level.time + 60000;
		break;
	case FP_LIGHTNING:
		hearable = qtrue;
		hearDist = 512;
		duration = overrideAmt;
		overrideAmt = 0;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->ps.activeForcePass = self->client->ps.fd.forcePowerLevel[FP_LIGHTNING];
		break;
	case FP_RAGE:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_1)
		{
			duration = 5000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_2)
		{
			duration = 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_3)
		{
			duration = 15000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_4)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_5)
		{
			duration = 25000;
		}
		else //shouldn't get here
		{
			break;
		}

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_PROTECT:
		hearable = qtrue;
		hearDist = 256;
		duration = 20000;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_ABSORB:
		hearable = qtrue;
		hearDist = 256;
		duration = 20000;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_TEAM_HEAL:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_TEAM_FORCE:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_DRAIN:
		hearable = qtrue;
		hearDist = 256;
		duration = overrideAmt;
		overrideAmt = 0;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		//self->client->ps.activeForcePass = self->client->ps.fd.forcePowerLevel[FP_DRAIN];
		break;
	case FP_SEE:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_1)
		{
			duration = 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_2)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_3)
		{
			duration = 30000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_4)
		{
			duration = 40000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_5)
		{
			duration = 50000;
		}
		else //shouldn't get here
		{
			break;
		}

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	default:
		break;
	}

	if ( duration )
	{
		self->client->ps.fd.forcePowerDuration[forcePower] = level.time + duration;
	}
	else
	{
		self->client->ps.fd.forcePowerDuration[forcePower] = 0;
	}

	if (hearable)
	{
		self->client->ps.otherSoundLen = hearDist;
		self->client->ps.otherSoundTime = level.time + 100;
	}

	self->client->ps.fd.forcePowerDebounce[forcePower] = 0;

	if ((int)forcePower == FP_SPEED && overrideAmt)
	{
		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Force User drain-reduction branch unreachable
		// GalaxyRP fix: [Force] routed through the same helper WP_ForcePowerAvailable() gates on, so
		// the price quoted and the price charged are the same number by construction.
		BG_ForcePowerDrain( &self->client->ps, forcePower, RP_ForceSpeedCost( overrideAmt ) );
	}
	else if ((int)forcePower != FP_GRIP && (int)forcePower != FP_DRAIN)
	{ //grip and drain drain as damage is done
		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Force User drain-reduction branch unreachable
		BG_ForcePowerDrain( &self->client->ps, forcePower, overrideAmt );
	}
}

void ForceHeal( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_HEAL ) )
	{
		return;
	}

	if ( self->health >= self->client->ps.stats[STAT_MAX_HEALTH])
	{
		// zyk: Shield Heal skill. Done when player has full HP
		if (self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[36] > 0 && self->client->ps.fd.forcePower >= RP_ForceHealCost( self ) && self->client->ps.stats[STAT_ARMOR] < self->client->pers.max_rpg_shield)
		{
			self->client->ps.stats[STAT_ARMOR] += 4 * self->client->pers.skill_levels[36];

			if (self->client->ps.stats[STAT_ARMOR] > self->client->pers.max_rpg_shield)
				self->client->ps.stats[STAT_ARMOR] = self->client->pers.max_rpg_shield;

			self->client->ps.fd.forcePower -= RP_ForceHealCost( self );

			G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/player/pickupshield.wav"));
		}
		return;
	}

	if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_5)
	{
		self->health += 50;

		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_4)
	{
		self->health += 40;

		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_3)
	{
		self->health += 30; //This was 50, but that angered the Balance God.

		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_2)
	{
		self->health += 20;

		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
	else
	{
		self->health += 10;

		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
	/*
	else
	{
		WP_ForcePowerStart( self, FP_HEAL, 0 );
	}
	*/
	//NOTE: Decided to make all levels instant.

	// zyk: now heal force power requires force based on the force power max cvar
	// GalaxyRP fix: [Force] costs half the caster's own maximum rather than half the server-wide
	// one -- see RP_ForceHealCost above. WP_ForcePowerUsable() at the top of this function has
	// already confirmed the pool covers this exact amount (it ends by delegating to
	// WP_ForcePowerAvailable, which charges FP_HEAL the same helper), so this cannot go negative.
	self->client->ps.fd.forcePower -= RP_ForceHealCost( self );

	G_Sound( self, CHAN_ITEM, G_SoundIndex("sound/weapons/force/heal.wav") );
}

// GalaxyRP fix: [Force] only client slots go in these bitflags. The receiving side,
// EV_TEAM_POWER in cg_event.c, walks clients 0..MAX_CLIENTS-1 and nothing else, so a bit for any
// higher entity was never read. Worse, Team Heal and Team Energize select NPC targets as well
// (their loop runs over every entity, not just clients), and an NPC in slot 80 or above reached
// the trickedentindex4 arm this function used to have with a shift of 32 or more -- undefined
// behaviour in C. NPC targets get their own event now, see RP_TeamPowerNPCEffect() below.
void WP_AddToClientBitflags(gentity_t *ent, int entNum)
{
	if (!ent || entNum < 0 || entNum >= MAX_CLIENTS)
	{
		return;
	}

	// the trickedentindex3/4 arms that used to follow for slots 32..63 are gone with the bound above
	if (entNum > 15)
	{
		ent->s.trickedentindex2 |= (1 << (entNum-16));
	}
	else
	{
		ent->s.trickedentindex |= (1 << entNum);
	}
}

// GalaxyRP fix: [Force] the most targets a single Team Heal or Team Energize can affect. Both
// used to collect their targets into "int pl[MAX_CLIENTS]" while walking every entity on the map,
// and friendly NPCs qualify as well as players -- so the 33rd qualifying target was written past
// the end of the array, on the stack. An admin NPC event or an SP map with a large friendly squad
// in range of a level 5 cast (768 units) was enough. The collection loops now stop once this many
// are held, which is what keeps them in bounds; the number itself is a gameplay limit. Players sit
// in slots 0..MAX_CLIENTS-1 and the loops walk in slot order, so every eligible player is always
// taken before any NPC.
#define RP_TEAM_POWER_MAX_TARGETS		100

// GalaxyRP fix: [Force] the most NPC targets of one cast that are sent a visual/sound event (see
// RP_TeamPowerNPCEffect). Every NPC in range is still healed or energized; only the effect stops
// here. Each event is a temp entity, and one per target is exactly the "many g_sound events at
// once" the original code folded into a single bitflag event to avoid. These events are also
// limited by RP_TeamPowerEffectBudget() below, which keeps them out of the entity reserve.
#define RP_TEAM_POWER_MAX_NPC_EFFECTS	16

/*
GalaxyRP fix: [Force] how many optional effect temp entities one Team Heal / Team Energize cast may
still create: the NPC visual events (RP_TeamPowerNPCEffect) and Team Shield Heal's pickup sound
(G_Sound -> G_SoundTempEntity), one per shielded player.

Every temp entity takes a real entity slot through G_Spawn() for about 300ms, and G_Spawn() cannot
fail politely: with every slot in use it calls trap->Error(ERR_DROP), which ends a dedicated server
(see ZYK_ENTITY_RESERVE in g_local.h). A cast used to create one temp entity; with an NPC squad and
Team Shield Heal it could create up to 1 + 16 + 31, all of them cosmetic, so on a map already near
the limit the cast itself could be the allocation that took the server down. Player- and
admin-driven spawns keep ZYK_ENTITY_RESERVE slots back for the allocations nothing can refuse (see
G_EntitySlotsAvailable in g_utils.c); these effects now do the same.

One count per cast rather than a G_EntitySlotsAvailable(1) call per effect: each temp entity lowers
G_FreeEntityCount() by exactly one, so counting down from a single scan gives the same answer
without rescanning the entity table up to 47 times. The 1 held back is the cast's own
EV_TEAM_POWER event, created in the same loop and deliberately NOT budgeted -- it predates this
change, it is the one effect players rely on, and it is exactly the kind of single, unavoidable
allocation the reserve is kept for. The result can be zero or negative; the callers then create no
optional effects, while every heal, shield and energize is still applied.
*/
static int RP_TeamPowerEffectBudget( void )
{
	return G_FreeEntityCount() - ZYK_ENTITY_RESERVE - 1;
}

/*
GalaxyRP fix: [Force] who Team Heal, Team Shield Heal and Team Energize may affect.

The two powers used to carry their own hand-written conditions, and they had drifted into a set
of different holes:

- Team Heal's "connected and not spectating" test sat in one half of an OR whose other half
  (playerTeam != NPCTEAM_ENEMY) is true for every real player outside Siege, so it never
  excluded anyone.
  Team Energize had no alive, connected or spectator test at all. A dead, spectating or
  connecting ally in range was picked -- and a spectating ally at full health passed Team Shield
  Heal's test too -- so the caster paid for a cast that did nothing useful.
- Neither excluded a downed or admin-paralyzed target. WP_ForcePowerUsable() stops a downed
  player healing THEMSELVES back to full before standing up; an ally's Team Heal (then Team
  Shield Heal on the next cast) did the same thing for them, and bypassed an admin punishment.
- In FFA a caster who was not logged in (amrpgmode 0) skipped the ally test entirely, on the
  belief that such a player has no allies. /allyadd works while logged out, and the powers reach
  logged-out players through the admin /give force and the Duel Tournament / Melee Battle
  enlightenment prizes -- so those casters healed and energized enemy players, neutral NPCs and
  (Team Energize had no NPC-team test) hostile NPCs.

One rule for both powers now. The caller adds its own "needs it" test, the radius and the PVS
check.
*/
static qboolean RP_TeamPowerTargetValid( gentity_t *self, gentity_t *ent, forcePowers_t power )
{
	if ( !self || !self->client || !ent || !ent->inuse || !ent->client || ent == self )
	{
		return qfalse;
	}

	// alive
	if ( ent->health <= 0 || ent->client->ps.stats[STAT_HEALTH] <= 0 || (ent->client->ps.eFlags & EF_DEAD) )
	{
		return qfalse;
	}

	if ( ent->NPC || ent->s.eType == ET_NPC )
	{
		// kept from the old Team Heal test, now applied to Team Energize as well
		if ( ent->s.NPC_class == CLASS_VEHICLE )
		{
			return qfalse;
		}

		// friendly npcs only, whatever the gametype. In FFA and the team gametypes OnSameTeam()
		// below already answers every player/npc pair this way; GT_SINGLE_PLAYER and GT_POWERDUEL
		// return from it before that point (any two non-bots, or equal duelTeam) and would
		// otherwise let an enemy npc through. The old Team Heal test refused NPCTEAM_ENEMY npcs.
		if ( ent->client->playerTeam != NPCTEAM_PLAYER )
		{
			return qfalse;
		}
	}
	else
	{
		// a real player: in the game, not watching it
		if ( ent->s.number >= MAX_CLIENTS ||
			ent->client->pers.connected != CON_CONNECTED ||
			ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(ent->client->ps.pm_flags & PMF_FOLLOW) ||
			ent->client->tempSpectate >= level.time )
		{
			return qfalse;
		}
	}

	// Both states set the DOWNED bit today; tested separately so neither depends on the other.
	// NPCs are never downed (NPC_spawn.c zeroes player_statuses), so this only ever bites players.
	if ( G_PlayerIsDowned( ent ) || G_PlayerIsAdminParalyzed( ent ) )
	{
		return qfalse;
	}

	// ysalamiri, duels, Duel Tournament, Melee Battle, noclip
	if ( !ForcePowerUsableOn( self, ent, power ) )
	{
		return qfalse;
	}

	// teammates, and friendly NPCs (OnSameTeam treats NPCTEAM_PLAYER NPCs as the players' side in FFA)
	if ( OnSameTeam( self, ent ) )
	{
		return qtrue;
	}

	// zyk: Team Heal and Team Energize can be used in FFA, on the caster's allies.
	// GalaxyRP fix: [Settings] this used to also allow through a target who was not an ally
	// (self->client->pers.player_settings & (1 << 10) -- the old /settings 5, "Use healing force
	// only at allied players", set to OFF/bypass). That per-player choice has been removed (see the
	// fix comment on settings_number_to_bit in Cmd_Settings_f), so in FFA both powers always
	// restrict to allies -- for every caster now, the amrpgmode 0 exemption described above
	// having gone as well.
	if ( level.gametype == GT_FFA && zyk_is_ally( self, ent ) == qtrue )
	{
		return qtrue;
	}

	return qfalse;
}

// GalaxyRP fix: [Force] the shield ceiling Team Shield Heal fills a target to. The target test used
// a hard-coded 100 for anyone not logged in while the heal itself capped at their max health, so
// the two could disagree. G_PublishMaxArmor() is the one place that rule lives (max_rpg_shield for
// a logged-in player, max health for anyone else); refreshing it here rather than reading the value
// left over from the last ClientEndFrame keeps a max-shield change earlier this frame from being
// missed.
static int RP_TeamShieldHealMax( gentity_t *ent )
{
	G_PublishMaxArmor( ent );

	return ent->client->ps.stats[STAT_MAX_ARMOR];
}

/*
GalaxyRP fix: [Force] the visual and sound for one NPC target of Team Heal / Team Energize.

The shared EV_TEAM_POWER event names its targets in a bitflag that only has room for client
slots, so an NPC target never showed the green/blue shell or played the sound. Each NPC target now
gets its own EV_TEAM_POWER at the NPC, with no bitflags set, the NPC's number in otherEntityNum and
TEAM_POWER_NPC_TARGET (bg_public.h) in generic1. A cgame that predates this sees an event with no
bits set and does nothing, so nothing breaks for players on the old plugin. Callers send it only
while RP_TeamPowerEffectBudget() leaves room above the entity reserve.
*/
static void RP_TeamPowerNPCEffect( gentity_t *npc, int eventParm )
{
	gentity_t *te = G_TempEntity( npc->r.currentOrigin, EV_TEAM_POWER );

	te->s.eventParm = eventParm; // 1 is heal, 2 is force regen, as for the player event
	te->s.otherEntityNum = npc->s.number;
	te->s.generic1 = TEAM_POWER_NPC_TARGET;
}

void ForceTeamHeal( gentity_t *self )
{
	float radius = 256;
	int i = 0;
	gentity_t *ent;
	vec3_t a;
	int numpl = 0;
	int pl[RP_TEAM_POWER_MAX_TARGETS];
	int healthadd = 0;
	int npc_effects = 0;
	int effect_budget = 0;
	qboolean shield_heal = qfalse;
	gentity_t *te = NULL;

	if ( self->health <= 0 )
	{
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_TEAM_HEAL ) )
	{
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[FP_TEAM_HEAL] >= level.time)
	{
		return;
	}

	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_2)
	{
		radius *= 1.5;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_3)
	{
		radius *= 2;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_4)
	{
		radius *= 2.5;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_5)
	{
		radius *= 3;
	}

	// zyk: Team Shield Heal skill of RPG Mode
	if (self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[37] > 0)
	{
		shield_heal = qtrue;
	}

	// zyk: now the condition will be the level.num_entities, so Team Heal also reaches npcs.
	// GalaxyRP fix: [Force] who qualifies is RP_TeamPowerTargetValid()'s decision now -- see the
	// comment there -- and the loop stops at RP_TEAM_POWER_MAX_TARGETS so pl[] cannot overflow.
	while (i < level.num_entities && numpl < RP_TEAM_POWER_MAX_TARGETS)
	{
		ent = &g_entities[i];

		if (RP_TeamPowerTargetValid(self, ent, FP_TEAM_HEAL) &&
			// needs health, or (Team Shield Heal) is at full health but short on shield. Npcs get
			// health only.
			(ent->client->ps.stats[STAT_HEALTH] < ent->client->ps.stats[STAT_MAX_HEALTH] ||
			 (shield_heal && !ent->NPC && ent->client->ps.stats[STAT_ARMOR] < RP_TeamShieldHealMax(ent))))
		{
			VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

			if (VectorLength(a) <= radius && trap->InPVS(self->client->ps.origin, ent->client->ps.origin))
			{
				pl[numpl] = i;
				numpl++;
			}
		}

		i++;
	}

	if (numpl < 1)
	{
		return;
	}

	// GalaxyRP (Alex): [Force Powers] For levels 4 and 5 heal a fixed amount, otherwise, based on how many people are healed
	// GalaxyRP fix: [Force] both arms used to also require numpl > 2, so with one or two allies a
	// level 4 or 5 healer fell through to the count-scaled arms below and delivered exactly what a
	// level 1 healer does -- the upgrade was invisible in the most common case. It also made the
	// payout run backwards: healing two allies gave 30 each while healing three gave 50 each. The
	// count guard is dropped so the fixed amount really is fixed, as the comment above says. Levels
	// 1 to 3 keep the count scaling untouched.
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_4)
	{
		healthadd = 40;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] == FORCE_LEVEL_5)
	{
		healthadd = 50;
	}
	else if (numpl == 1)
	{
		healthadd = 40;
	}
	else if (numpl == 2)
	{
		healthadd = 30;
	} 
	else
	{
		healthadd = 25;
	}

	self->client->ps.fd.forcePowerDebounce[FP_TEAM_HEAL] = level.time + 2000;
	effect_budget = RP_TeamPowerEffectBudget();
	i = 0;

	while (i < numpl)
	{
		gentity_t *target = &g_entities[pl[i]];

		if (target->client->ps.stats[STAT_HEALTH] > 0 &&
			target->health > 0)
		{
			int max_health = target->client->ps.stats[STAT_MAX_HEALTH];

			// zyk: Team Shield Heal skill of RPG Mode
			if (shield_heal && !target->NPC && target->client->ps.stats[STAT_HEALTH] >= max_health)
			{ // zyk: can only be used on players with full health already
				int max_shield = RP_TeamShieldHealMax(target);

				if (target->client->ps.stats[STAT_ARMOR] < max_shield)
				{
					target->client->ps.stats[STAT_ARMOR] += 3 * self->client->pers.skill_levels[37];

					if (target->client->ps.stats[STAT_ARMOR] > max_shield)
						target->client->ps.stats[STAT_ARMOR] = max_shield;

					// GalaxyRP fix: [Force] the sound is a temp entity per player -- up to 31 in one
					// cast -- so it is left out once the entity reserve is reached; the shield is not.
					// See RP_TeamPowerEffectBudget().
					if (effect_budget > 0)
					{
						G_Sound(target, CHAN_AUTO, G_SoundIndex("sound/player/pickupshield.wav"));
						effect_budget--;
					}
				}
			}

			// GalaxyRP fix: [Force] health is only added to a target below its maximum. This used to
			// run for every target and then clamp to the maximum -- which, for a target picked by
			// Team Shield Heal (already at full health), pulled anyone ABOVE their maximum back down.
			if (target->client->ps.stats[STAT_HEALTH] < max_health)
			{
				target->client->ps.stats[STAT_HEALTH] += healthadd;

				if (target->client->ps.stats[STAT_HEALTH] > max_health)
				{
					target->client->ps.stats[STAT_HEALTH] = max_health;
				}

				target->health = target->client->ps.stats[STAT_HEALTH];
			}

			//At this point we know we got one, so add him into the collective event client bitflag
			if (!te)
			{
				te = G_TempEntity( self->client->ps.origin, EV_TEAM_POWER);
				te->s.eventParm = 1; //eventParm 1 is heal, eventParm 2 is force regen

				//since we had an extra check above, do the drain now because we got at least one guy
				BG_ForcePowerDrain( &self->client->ps, FP_TEAM_HEAL, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL]][FP_TEAM_HEAL] );
			}

			if (pl[i] < MAX_CLIENTS)
			{
				WP_AddToClientBitflags(te, pl[i]);
				//Now cramming it all into one event.. doing this many g_sound events at once was a Bad Thing.
			}
			else if (npc_effects < RP_TEAM_POWER_MAX_NPC_EFFECTS && effect_budget > 0)
			{
				RP_TeamPowerNPCEffect(target, 1);
				npc_effects++;
				effect_budget--;
			}
		}
		i++;
	}
}

void ForceTeamForceReplenish( gentity_t *self )
{
	float radius = 256;
	int i = 0;
	gentity_t *ent;
	vec3_t a;
	int numpl = 0;
	int pl[RP_TEAM_POWER_MAX_TARGETS];
	int poweradd = 0;
	int npc_effects = 0;
	int effect_budget = 0;
	gentity_t *te = NULL;

	if ( self->health <= 0 )
	{
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_TEAM_FORCE ) )
	{
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[FP_TEAM_FORCE] >= level.time)
	{
		return;
	}

	// GalaxyRP (Alex): [Force Powers] Increase radius by .5 for each level up to 5
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_2)
	{
		radius *= 1.5;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_3)
	{
		radius *= 2;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_4)
	{
		radius *= 2.5;
	}
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_5)
	{
		radius *= 3;
	}

	// zyk: now the condition will be the level.num_entities
	// GalaxyRP fix: [Force] who qualifies is RP_TeamPowerTargetValid()'s decision now -- see the
	// comment there -- and the loop stops at RP_TEAM_POWER_MAX_TARGETS so pl[] cannot overflow.
	while (i < level.num_entities && numpl < RP_TEAM_POWER_MAX_TARGETS)
	{
		ent = &g_entities[i];

		// zyk: created new condition so we can use Team Energize in FFA.
		// GalaxyRP fix: [Skills] this used to also select a target whose force power was already full
		// but who was short on blaster pack/power cell ammo, gated behind pers.skill_levels[55]
		// (Improvements) > 0, so the loop below could restore their ammo instead of force power.
		// Improvements is now a reserved/unused skill (see the matching fix comment in
		// do_upgrade_skill() in g_cmds.c), so that ammo-regen path -- and the target-selection branch
		// that fed it -- have been removed outright, back to selecting only targets below full force.
		if (RP_TeamPowerTargetValid(self, ent, FP_TEAM_FORCE) &&
			ent->client->ps.fd.forcePower < ent->client->ps.fd.forcePowerMax)
		{
			VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

			if (VectorLength(a) <= radius && trap->InPVS(self->client->ps.origin, ent->client->ps.origin))
			{
				pl[numpl] = i;
				numpl++;
			}
		}

		i++;
	}

	if (numpl < 1)
	{
		return;
	}

	// zyk: decreased amount of force recovered. Default values in order: 50, 33 and 25
	// GalaxyRP fix: [Force] same count-guard removal as Team Heal above -- levels 4 and 5 pay their
	// fixed amount regardless of how many allies are in range, instead of silently dropping to the
	// level-1 payout whenever fewer than three were caught.
	if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_4) {
		poweradd = 40;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] == FORCE_LEVEL_5) {
		poweradd = 50;
	}
	else if (numpl == 1)
	{
		poweradd = 40;
	}
	else if (numpl == 2)
	{
		poweradd = 30;
	}
	else
	{
		poweradd = 20;
	}

	// GalaxyRP fix: [Force] a flat 2 second cooldown at every level, the same as Team Heal. It used
	// to shrink with the level (2000/1500/1000/500 ms) down to NONE at level 5, where only the button
	// release (or the 300ms repeat debounce on the force_forcepowerother bind) stood between casts.
	// Level 4 and 5 pay a fixed amount to every target for one price, so two allied level 5
	// energizers broke even at any rate they could cast -- unlimited force for both -- and three or
	// more gained force on every round. The cost now also rises with the level (forcePowerNeeded[] in
	// bg_pmove.c, 40 to 80); together these end the two-player loop and bound the larger ones.
	self->client->ps.fd.forcePowerDebounce[FP_TEAM_FORCE] = level.time + 2000;

	BG_ForcePowerDrain( &self->client->ps, FP_TEAM_FORCE, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE]][FP_TEAM_FORCE] );

	effect_budget = RP_TeamPowerEffectBudget();
	i = 0;

	while (i < numpl)
	{
		gentity_t *target = &g_entities[pl[i]];

		// GalaxyRP fix: [Skills] this used to branch here: if the target's force power was already
		// full, recover their blaster pack/power cell ammo instead (scaled off the now-removed
		// Improvements skill). Since only targets below full force power are selected above now, this
		// branch is always taken -- restore force power unconditionally.
		target->client->ps.fd.forcePower += poweradd;

		// GalaxyRP fix: [Force] cap at the target's own maximum. This used to cap a logged-in
		// character at pers.max_force_power and everyone else -- npcs included -- at
		// RP_MAX_FORCE_POWER_LOGGED_OUT (100), a second copy of a rule forcePowerMax already holds:
		// initialize_rpg_skills() and the Force Power skill upgrade set it to pers.max_force_power,
		// WP_SpawnInitForcePowers() and /logout set it to RP_MAX_FORCE_POWER_LOGGED_OUT, and
		// WP_ForcePowerRegenerate() caps at it. For npcs the two copies agreed only by accident: an
		// npc that knows force powers has its .npc "forcePowerMax" overwritten with 100 by
		// WP_SpawnInitForcePowers() (NPC_Begin, amrpgmode 0), so the fixed 100 matched -- but any
		// other route to an npc maximum above 100 would have had its force cut on being energized.
		// The target test above already requires forcePower < forcePowerMax.
		if (target->client->ps.fd.forcePower > target->client->ps.fd.forcePowerMax)
			target->client->ps.fd.forcePower = target->client->ps.fd.forcePowerMax;

		//At this point we know we got one, so add him into the collective event client bitflag
		if (!te)
		{
			te = G_TempEntity( self->client->ps.origin, EV_TEAM_POWER);
			te->s.eventParm = 2; //eventParm 1 is heal, eventParm 2 is force regen
		}

		if (pl[i] < MAX_CLIENTS)
		{
			WP_AddToClientBitflags(te, pl[i]);
			//Now cramming it all into one event.. doing this many g_sound events at once was a Bad Thing.
		}
		else if (npc_effects < RP_TEAM_POWER_MAX_NPC_EFFECTS && effect_budget > 0)
		{
			RP_TeamPowerNPCEffect(target, 2);
			npc_effects++;
			effect_budget--;
		}

		i++;
	}
}

void ForceGrip( gentity_t *self )
{
	trace_t tr;
	vec3_t tfrom, tto, fwd;

	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if (self->client->ps.fd.forceGripUseTime > level.time)
	{
		return;
	}

	// DAJ_RP: [Force] no grip while you are being gripped yourself -- the same "being gripped" test this
	// function already applies to the target below, and that ItemUse_Jetpack() (g_items.c) uses to refuse the
	// jetpack in a grip. Stock let a held player grip back for the first seconds of a hold (until the choke at
	// grip level 2 or 3 stopped him, never at level 1), so both ended up lifting each other and floating. A
	// refused grip costs nothing: the power never starts, like a grip at empty air. Breaking free with
	// Push or Pull clears the mark at once (ForceThrow()); a grip that simply ends leaves it up to a second.
	if (self->client->ps.fd.forceGripBeingGripped >= level.time)
	{
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_GRIP ) )
	{
		return;
	}

	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]*MAX_GRIP_DISTANCE;
	tto[1] = tfrom[1] + fwd[1]*MAX_GRIP_DISTANCE;
	tto[2] = tfrom[2] + fwd[2]*MAX_GRIP_DISTANCE;

	trap->Trace(&tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if ( tr.fraction != 1.0 &&
		tr.entityNum != ENTITYNUM_NONE &&
		g_entities[tr.entityNum].client &&
		!g_entities[tr.entityNum].client->ps.fd.forceGripCripple &&
		g_entities[tr.entityNum].client->ps.fd.forceGripBeingGripped < level.time &&
		// DAJ_RP: [Force] ...nor grip the one gripping you. Covers the gap the test above cannot: the target's
		// grip on you is active from the moment it starts (WP_DoSpecificPower()), while your "being gripped"
		// mark only comes with its first update -- so two players gripping each other on the same frame
		// cannot both get through, the first one handled wins.
		!( g_entities[tr.entityNum].client->ps.fd.forceGripEntityNum == self->s.number &&
		   ( g_entities[tr.entityNum].client->ps.fd.forcePowersActive & ( 1 << FP_GRIP ) ) ) &&
		ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_GRIP) &&
		(g_friendlyFire.integer || !OnSameTeam(self, &g_entities[tr.entityNum])) ) //don't grip someone who's still crippled
	{
		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist check unreachable

		// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, boss-grip check unreachable

		if (g_entities[tr.entityNum].s.number < MAX_CLIENTS && g_entities[tr.entityNum].client->ps.m_iVehicleNum)
		{ //a player on a vehicle
			gentity_t *vehEnt = &g_entities[g_entities[tr.entityNum].client->ps.m_iVehicleNum];
			if (vehEnt->inuse && vehEnt->client && vehEnt->m_pVehicle)
			{
				if (vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER ||
					vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
				{ //push the guy off
					vehEnt->m_pVehicle->m_pVehicleInfo->Eject(vehEnt->m_pVehicle, (bgEntity_t *)&g_entities[tr.entityNum], qfalse);
				}
			}
		}
		self->client->ps.fd.forceGripEntityNum = tr.entityNum;
		g_entities[tr.entityNum].client->ps.fd.forceGripStarted = level.time;
		self->client->ps.fd.forceGripDamageDebounceTime = 0;

		self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
		self->client->ps.forceHandExtendTime = level.time + 5000;
	}
	else
	{
		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
		return;
	}
}

void ForceSpeed( gentity_t *self, int forceDuration )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_SPEED)) )
	{
		WP_ForcePowerStop( self, FP_SPEED );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_SPEED ) )
	{
		return;
	}

	if ( self->client->holdingObjectiveItem >= MAX_CLIENTS
		&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD )
	{//holding Siege item
		if ( g_entities[self->client->holdingObjectiveItem].genericValue15 )
		{//disables force powers
			return;
		}
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_SPEED, forceDuration );
	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/speed.wav") );
	G_Sound( self, TRACK_CHANNEL_2, speedLoopSound );
}

void ForceSeeing( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_SEE)) )
	{
		WP_ForcePowerStop( self, FP_SEE );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_SEE ) )
	{
		return;
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_SEE, 0 );

	// GalaxyRP fix: these two calls were left commented out, so Force Sense played no sound at
	// all on activation. seeLoopSound is already lazily initialized above (in the same pattern as
	// speedLoopSound, rageLoopSound, absorbLoopSound and protectLoopSound, all of which *do* play
	// here in their respective functions), so this just wires the activation sound back in to
	// match every other hold-style force power.
	G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/see.wav") );
	G_Sound( self, TRACK_CHANNEL_5, seeLoopSound );
}

void ForceProtect( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT)) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_PROTECT ) )
	{
		return;
	}

	// Make sure to turn off Force Rage and Force Absorb.
	/* zyk: now it will be possible to use more than one force power at a time!
	if (self->client->ps.fd.forcePowersActive & (1 << FP_RAGE) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
	}
	*/

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_PROTECT, 0 );
	G_PreDefSound(self->client->ps.origin, PDSOUND_PROTECT);
	G_Sound( self, TRACK_CHANNEL_3, protectLoopSound );
}

void ForceAbsorb( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB)) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_ABSORB ) )
	{
		return;
	}

	// Make sure to turn off Force Rage and Force Protection.
	/* zyk: now it will be possible to use more than one force power at a time!
	if (self->client->ps.fd.forcePowersActive & (1 << FP_RAGE) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
	}
	*/

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_ABSORB, 0 );
	G_PreDefSound(self->client->ps.origin, PDSOUND_ABSORB);
	//G_Sound( self, TRACK_CHANNEL_3, absorbLoopSound );
}

void ForceRage( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_RAGE ) )
	{
		return;
	}

	if (self->client->ps.fd.forceRageRecoveryTime >= level.time)
	{
		return;
	}

	if (self->health < 10)
	{
		return;
	}

	// Make sure to turn off Force Protection and Force Absorb.
	/* zyk: now it will be possible to use more than one force power at a time!
	if (self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
	}
	*/

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_RAGE, 0 );

	G_Sound( self, TRACK_CHANNEL_4, G_SoundIndex("sound/weapons/force/rage.wav") );
	G_Sound( self, TRACK_CHANNEL_3, rageLoopSound );
}

void ForceLightning( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}
	if ( self->client->ps.fd.forcePower < 25 || !WP_ForcePowerUsable( self, FP_LIGHTNING ) )
	{
		return;
	}
	if ( self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] > level.time )
	{//stops it while using it and also after using it, up to 3 second delay
		return;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	// fix: rocket lock bug
	BG_ClearRocketLock(&self->client->ps);

	//Shoot lightning from hand
	//using grip anim now, to extend the burst time
	self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
	self->client->ps.forceHandExtendTime = level.time + 20000;

	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/lightning") );

	WP_ForcePowerStart( self, FP_LIGHTNING, 2000 ); // zyk: changed lightning duration from 500 to 2000
}

extern void Boba_FlyStop( gentity_t *self );
void ForceLightningDamage( gentity_t *self, gentity_t *traceEnt, vec3_t dir, vec3_t impactPoint )
{
	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	if ( traceEnt && traceEnt->takedamage )
	{
		if (!traceEnt->client && traceEnt->s.eType == ET_NPC)
		{ //g2animent
			if (traceEnt->s.genericenemyindex < level.time)
			{
				traceEnt->s.genericenemyindex = level.time + 2000;
			}
		}
		if ( traceEnt->client )
		{//an enemy or object
			if (traceEnt->client->noLightningTime >= level.time)
			{ //give them power and don't hurt them.
				traceEnt->client->ps.fd.forcePower++;
				// zyk: changed the code below so we can use the cvar zyk_FORCE_POWER_MAX instead of hardcoded 100 force power max
				if (traceEnt->client->sess.amrpgmode == 2 && traceEnt->client->ps.fd.forcePower > traceEnt->client->pers.max_force_power)
					traceEnt->client->ps.fd.forcePower = traceEnt->client->pers.max_force_power;
				else if (traceEnt->client->sess.amrpgmode < 2 && traceEnt->client->ps.fd.forcePower > RP_MAX_FORCE_POWER_LOGGED_OUT)
					traceEnt->client->ps.fd.forcePower = RP_MAX_FORCE_POWER_LOGGED_OUT;

				return;
			}
			if (ForcePowerUsableOn(self, traceEnt, FP_LIGHTNING))
			{
				int	dmg = Q_irand(1,2); //Q_irand( 1, 3 );

				int modPowerLevel = -1;

				if (traceEnt->client)
				{
					modPowerLevel = WP_AbsorbConversion(traceEnt, traceEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_LIGHTNING, self->client->ps.fd.forcePowerLevel[FP_LIGHTNING], 1);
				}

				if (modPowerLevel != -1)
				{
					if (!modPowerLevel)
					{
						dmg = 0;
						traceEnt->client->noLightningTime = level.time + 400;
					}
					else if (modPowerLevel == 1)
					{
						dmg = 1;
						traceEnt->client->noLightningTime = level.time + 300;
					}
					else if (modPowerLevel == 2)
					{
						dmg = 1;
						traceEnt->client->noLightningTime = level.time + 100;
					}
				}

				/*
				if ( self->client->ps.weapon == WP_MELEE
					&& self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2 )
				{//2-handed lightning
					//jackin' 'em up, Palpatine-style
					dmg *= 2;
				}
				*/
				// zyk: Lightning level 4 in RPG Mode causes double damage
				if (self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[13] == FORCE_LEVEL_4)
				{
					dmg *= 1.5;
				}

				if (self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[13] == FORCE_LEVEL_5)
				{
					dmg *= 2;
				}

				if (dmg)
				{
					//rww - Shields can now absorb lightning too.
					// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Stealth Attacker electric-absorb branch unreachable
					G_Damage( traceEnt, self, self, dir, impactPoint, dmg, 0, MOD_FORCE_DARK );
				}
				if ( traceEnt->client )
				{
					if ( !Q_irand( 0, 2 ) )
					{
						G_Sound( traceEnt, CHAN_BODY, G_SoundIndex( va("sound/weapons/force/lightninghit%i", Q_irand(1, 3) )) );
					}

					if (traceEnt->client->ps.electrifyTime < (level.time + 400))
					{ //only update every 400ms to reduce bandwidth usage (as it is passing a 32-bit time value)
						traceEnt->client->ps.electrifyTime = level.time + 800;
					}
					// GalaxyRP fix: [Cloak Item] the explicit decloak-on-hit block that used to be here is
					// gone -- the conditional G_Damage() call above (when dmg is nonzero) already runs
					// through the new centralized "any damage decloaks" hook (see G_Damage's own GalaxyRP
					// fix comment in g_combat.c), which covers this hit generically now, pair-aware and
					// with the same temporary re-cloak lockout. One minor behavior change: a lightning
					// tick with dmg == 0 (no G_Damage() call at all) no longer decloaks on its own, since
					// there's no damage there to decloak on.

					// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist branch unreachable
					if (!traceEnt->NPC && traceEnt->client->jetPackOn)
					{ //disable jetpack temporarily
						// GalaxyRP fix: [Dead Code] rpg_class permanently 0, condition always true (amrpgmode!=2 || rpg_class!=5 || ...)
						Jetpack_Off(traceEnt);
						traceEnt->client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
					}
					else if (traceEnt->NPC && traceEnt->client->NPC_class == CLASS_BOBAFETT)
					{ // zyk: also disables npc jetpack
						Boba_FlyStop(traceEnt);
					}
				}
			}
		}
	}
}

void ForceShootLightning( gentity_t *self )
{
	trace_t	tr;
	vec3_t	end, forward;
	gentity_t	*traceEnt;

	if ( self->health <= 0 )
	{
		return;
	}
	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2 )
	{//arc
		vec3_t	center, mins, maxs, dir, ent_org, size, v;
		float	radius = FORCE_LIGHTNING_RADIUS, dot, dist;
		gentity_t	*entityList[MAX_GENTITIES];
		int			iEntityList[MAX_GENTITIES];
		int		e, numListedEntities, i;

		VectorCopy( self->client->ps.origin, center );

		// zyk: Lightning 4/4 has more range
		if (self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[13] > 3)
		{
			radius = FORCE_LIGHTNING_RADIUS * 1.8;
		}

		for ( i = 0 ; i < 3 ; i++ )
		{
			mins[i] = center[i] - radius;
			maxs[i] = center[i] + radius;
		}
		numListedEntities = trap->EntitiesInBox( mins, maxs, iEntityList, MAX_GENTITIES );

		i = 0;
		while (i < numListedEntities)
		{
			entityList[i] = &g_entities[iEntityList[i]];

			i++;
		}

		for ( e = 0 ; e < numListedEntities ; e++ )
		{
			traceEnt = entityList[e];

			if ( !traceEnt )
				continue;
			if ( traceEnt == self )
				continue;
			if ( traceEnt->r.ownerNum == self->s.number && traceEnt->s.weapon != WP_THERMAL )//can push your own thermals
				continue;
			if ( !traceEnt->inuse )
				continue;
			if ( !traceEnt->takedamage )
				continue;
			if ( traceEnt->health <= 0 )//no torturing corpses
				continue;
			if ( !g_friendlyFire.integer && OnSameTeam(self, traceEnt))
				continue;
			//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
			// find the distance from the edge of the bounding box
			for ( i = 0 ; i < 3 ; i++ )
			{
				if ( center[i] < traceEnt->r.absmin[i] )
				{
					v[i] = traceEnt->r.absmin[i] - center[i];
				} else if ( center[i] > traceEnt->r.absmax[i] )
				{
					v[i] = center[i] - traceEnt->r.absmax[i];
				} else
				{
					v[i] = 0;
				}
			}

			VectorSubtract( traceEnt->r.absmax, traceEnt->r.absmin, size );
			VectorMA( traceEnt->r.absmin, 0.5, size, ent_org );

			//see if they're in front of me
			//must be within the forward cone
			VectorSubtract( ent_org, center, dir );
			VectorNormalize( dir );
			if ( (dot = DotProduct( dir, forward )) < 0.5 )
				continue;

			//must be close enough
			dist = VectorLength( v );
			if ( dist >= radius )
			{
				continue;
			}

			//in PVS?
			if ( !traceEnt->r.bmodel && !trap->InPVS( ent_org, self->client->ps.origin ) )
			{//must be in PVS
				continue;
			}

			//Now check and see if we can actually hit it
			trap->Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT, qfalse, 0, 0 );
			if ( tr.fraction < 1.0f && tr.entityNum != traceEnt->s.number )
			{//must have clear LOS
				continue;
			}

			// ok, we are within the radius, add us to the incoming list
			ForceLightningDamage( self, traceEnt, dir, ent_org );
		}
	}
	else
	{//trace-line
		VectorMA( self->client->ps.origin, 1024, forward, end ); // zyk: changed scale factor from 2048 to 1024

		trap->Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, end, self->s.number, MASK_SHOT, qfalse, 0, 0 );
		if ( tr.entityNum == ENTITYNUM_NONE || tr.fraction == 1.0 || tr.allsolid || tr.startsolid )
		{
			return;
		}

		traceEnt = &g_entities[tr.entityNum];
		ForceLightningDamage( self, traceEnt, forward, tr.endpos );
	}
}

void ForceDrain( gentity_t *self )
{
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if ( self->client->ps.fd.forcePower < 25 || !WP_ForcePowerUsable( self, FP_DRAIN ) )
	{
		return;
	}
	if ( self->client->ps.fd.forcePowerDebounce[FP_DRAIN] > level.time )
	{//stops it while using it and also after using it, up to 3 second delay
		return;
	}

//	self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
//	self->client->ps.forceHandExtendTime = level.time + 1000;
	self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
	self->client->ps.forceHandExtendTime = level.time + 20000;

	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/drain.wav") );

	WP_ForcePowerStart( self, FP_DRAIN, 500 );
}

/*
GalaxyRP fix: [Force] Drain Shield's second half: "It also makes Drain suck hp/shield from the enemy
to restore your hp/shield". zyk wrote it as a branch inside ForceDrainDamage() for a target with no
force left, but the check wrapped around the whole drain required the target to HAVE force, and
nothing lets force go below 0, so the branch could never run.

It runs now, for a target that is alive, has no force, and is not a vehicle, when the caster is a
logged-in character with the Drain Shield skill. Downed and admin-paralyzed targets never reach it
(ForceDrainDamage() refuses them outright). The damage goes through G_Damage(), so every protection
that lives there -- allies, chat protection, duels and the mini-games, invulnerability, noclip,
Protect, Rage -- applies, and the caster is restored by exactly what the target lost in health and
shield: nothing when the hit was refused, and never more than was taken. Health first, anything left
over to shield, each capped at the caster's maximum.
*/
static qboolean RP_DrainShieldCanSteal( gentity_t *self, gentity_t *target )
{
	if ( !self || !self->client || self->client->sess.amrpgmode != 2 || self->client->pers.skill_levels[33] <= 0 )
	{
		return qfalse;
	}

	if ( !target || !target->client || target->health <= 0 || target->client->ps.fd.forcePower > 0 )
	{
		return qfalse;
	}

	if ( target->s.eType == ET_NPC && target->s.NPC_class == CLASS_VEHICLE )
	{
		return qfalse;
	}

	return qtrue;
}

// health plus shield, the two things the steal can take
static int RP_DrainShieldPool( gentity_t *target )
{
	int pool = (target->health > 0) ? target->health : 0;

	if ( target->client && target->client->ps.stats[STAT_ARMOR] > 0 )
	{
		pool += target->client->ps.stats[STAT_ARMOR];
	}

	return pool;
}

static void RP_DrainShieldRestore( gentity_t *self, int amount )
{
	if ( amount <= 0 || self->health <= 0 || self->client->ps.stats[STAT_HEALTH] <= 0 )
	{
		return;
	}

	if ( self->health < self->client->ps.stats[STAT_MAX_HEALTH] )
	{
		int room = self->client->ps.stats[STAT_MAX_HEALTH] - self->health;

		if ( room > amount )
		{
			room = amount;
		}

		self->health += room;
		self->client->ps.stats[STAT_HEALTH] = self->health;
		amount -= room;
	}

	if ( amount > 0 && self->client->ps.stats[STAT_ARMOR] < self->client->pers.max_rpg_shield )
	{
		self->client->ps.stats[STAT_ARMOR] += amount;

		if ( self->client->ps.stats[STAT_ARMOR] > self->client->pers.max_rpg_shield )
		{
			self->client->ps.stats[STAT_ARMOR] = self->client->pers.max_rpg_shield;
		}
	}
}

void ForceDrainDamage( gentity_t *self, gentity_t *traceEnt, vec3_t dir, vec3_t impactPoint )
{
	gentity_t *tent;

	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	if ( traceEnt && traceEnt->takedamage )
	{
		// GalaxyRP fix: [Force] never a corpse, a downed player or an admin-paralyzed one. The level
		// 1-2 beam had no health test at all (only the level 3+ cone skipped corpses), so it drained
		// the force still sitting on a dead body; and nothing refused a downed or paralyzed target,
		// who cannot defend or start Absorb, so they were a free heal source for the whole countdown.
		// ForceShootDrain() already filters all three; this is the backstop for any other caller.
		if ( traceEnt->health <= 0 || G_PlayerIsDowned( traceEnt ) || G_PlayerIsAdminParalyzed( traceEnt ) )
		{
			return;
		}

		// GalaxyRP fix: [Force] "|| RP_DrainShieldCanSteal" -- see the comment on that function. This
		// test alone is what made the Drain Shield steal branch below unreachable.
		if ( traceEnt->client && (!OnSameTeam(self, traceEnt) || g_friendlyFire.integer) && self->client->ps.fd.forceDrainTime < level.time &&
			(traceEnt->client->ps.fd.forcePower || RP_DrainShieldCanSteal(self, traceEnt)) )
		{//an enemy or object
			if (!traceEnt->client && traceEnt->s.eType == ET_NPC)
			{ //g2animent
				if (traceEnt->s.genericenemyindex < level.time)
				{
					traceEnt->s.genericenemyindex = level.time + 2000;
				}
			}

			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist branch unreachable

			if (ForcePowerUsableOn(self, traceEnt, FP_DRAIN))
			{
				int modPowerLevel = -1;
				int	dmg = 0; //Q_irand( 1, 3 );
				int stolen = 0; // GalaxyRP fix: [Force] health+shield taken by the Drain Shield steal
				qboolean steal_tried = qfalse;
				if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_1)
				{
					dmg = 2; //because it's one-shot
				}
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_2)
				{
					dmg = 3;
				}
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_3)
				{
					dmg = 4;
				}
				// GalaxyRP (Alex): [Force Powers] More damage for levels 4 and 5
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_4)
				{
					dmg = 5;
				}
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_5)
				{
					dmg = 6;
				}

				if (traceEnt->client)
				{
					modPowerLevel = WP_AbsorbConversion(traceEnt, traceEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_DRAIN, self->client->ps.fd.forcePowerLevel[FP_DRAIN], 1);
				}

				if (modPowerLevel != -1)
				{
					// GalaxyRP fix: [Force] this used to be an if/else chain with arms for
					// modPowerLevel 0, 1 and 2 only. modPowerLevel is the attacker's Drain level minus
					// the defender's Absorb level (WP_AbsorbConversion), and in vanilla that could
					// never exceed 2 -- Drain capped at 3 and an Absorb of 0 returns -1 earlier -- so
					// the chain was complete. Raising Drain to 5 while Absorb stays capped at 3 lets
					// the difference reach 3 or 4, which matched no arm and left dmg at its full
					// value: Absorb 1 cut Drain 3 from 4 damage to 2 but did nothing whatsoever
					// against Drain 4 or 5, while still refunding force and playing the absorb sound
					// so it looked like it had worked. Written as a clamp rather than more arms
					// because that is what the old chain computed for every reachable case
					// (modPowerLevel is always below dmg here) and it cannot go stale if either
					// power's level range changes again.
					if (modPowerLevel < dmg)
					{
						dmg = modPowerLevel;
					}
				}
				//G_Damage( traceEnt, self, self, dir, impactPoint, dmg, 0, MOD_FORCE_DARK );

				if (dmg)
				{
					if (RP_DrainShieldCanSteal(self, traceEnt))
					{ // zyk: Drain Shield skill. Enemy has no force. Damages him
						// GalaxyRP fix: [Force] measured before and after, so the caster is restored
						// by what the target really lost (see RP_DrainShieldCanSteal). dmg is cleared
						// so the force-drain restore further down does not also pay out.
						const int before = RP_DrainShieldPool(traceEnt);

						steal_tried = qtrue;

						if (dmg / 2 > 0)
						{
							G_Damage( traceEnt, self, self, NULL, impactPoint, (dmg/2), 0, MOD_FORCE_DARK );
							stolen = before - RP_DrainShieldPool(traceEnt);

							if (stolen < 0)
							{
								stolen = 0;
							}
						}

						dmg = 0;
					}
					else if (traceEnt->client->ps.fd.forcePower >= dmg)
					{
						traceEnt->client->ps.fd.forcePower -= (dmg);
					}
					else if (traceEnt->client->ps.fd.forcePower > 0)
					{
						dmg = traceEnt->client->ps.fd.forcePower;
						traceEnt->client->ps.fd.forcePower -= (dmg);
					}
					else
					{ // zyk: enemy has no force to consume
						dmg = 0;
					}
				}

				if (traceEnt->client->ps.fd.forcePower < 0)
				{
					traceEnt->client->ps.fd.forcePower = 0;
				}

				// GalaxyRP fix: [Force] the Drain Shield steal pays out here, health then shield
				RP_DrainShieldRestore(self, stolen);

				if (self->client->ps.stats[STAT_HEALTH] < self->client->ps.stats[STAT_MAX_HEALTH] &&
					self->health > 0 && self->client->ps.stats[STAT_HEALTH] > 0 && dmg > 0)
				{ // zyk: only recover hp if dmg > 0, which means enemy had force to consume
					self->health += dmg;
					if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
					{
						self->health = self->client->ps.stats[STAT_MAX_HEALTH];
					}
					self->client->ps.stats[STAT_HEALTH] = self->health;
				}
				else if (dmg > 0 && self->client->sess.amrpgmode == 2 && self->client->pers.skill_levels[33] > 0 && self->client->ps.stats[STAT_ARMOR] < self->client->pers.max_rpg_shield)
				{ // zyk: if player has RPG Drain Shield skill and hp is full, recover shield
					self->client->ps.stats[STAT_ARMOR] += 1;
				}

				// GalaxyRP fix: [Force] not when a Drain Shield steal was refused (an ally, chat
				// protection, invulnerability...): a target G_Damage would not touch had its force
				// regeneration frozen for as long as the caster held Drain on them.
				if (!steal_tried || stolen > 0)
				{
					traceEnt->client->ps.fd.forcePowerRegenDebounceTime = level.time + 800; //don't let the client being drained get force power back right away
				}

				//Drain the standard amount since we just drained someone else

				/*
				if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_1)
				{
					BG_ForcePowerDrain( &self->client->ps, FP_DRAIN, 0 );
				}
				else
				{
					BG_ForcePowerDrain( &self->client->ps, FP_DRAIN, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_DRAIN]][FP_DRAIN]/5 );
				}

				if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_1)
				{
					self->client->ps.fd.forceDrainTime = level.time + 100;
				}
				else
				{
					self->client->ps.fd.forceDrainTime = level.time + 20;
				}

				if ( traceEnt->client )
				{
					if ( !Q_irand( 0, 2 ) )
					{
						//G_Sound( traceEnt, CHAN_BODY, G_SoundIndex( "sound/weapons/force/lightninghit.wav" ) );
					}
				//	traceEnt->s.powerups |= ( 1 << PW_DISINT_1 );

				//	traceEnt->client->ps.powerups[PW_DISINT_1] = level.time + 500;
				}
				*/

				if (traceEnt->client->forcePowerSoundDebounce < level.time)
				{
					tent = G_TempEntity( impactPoint, EV_FORCE_DRAINED);
					tent->s.eventParm = DirToByte(dir);
					tent->s.owner = traceEnt->s.number;

					traceEnt->client->forcePowerSoundDebounce = level.time + 400;
				}
			}
		}
	}
}

int ForceShootDrain( gentity_t *self )
{
	trace_t	tr;
	vec3_t	end, forward;
	gentity_t	*traceEnt;
	int			gotOneOrMore = 0;

	if ( self->health <= 0 )
	{
		return 0;
	}

	// GalaxyRP: [Force Drain FX] set the caster's hand/channel FX flag up front, before the target
	// search below, so it refreshes every tick this function runs regardless of whether a target is
	// actually found. It used to sit after the if/else below; the trace-line branch (levels 1-2)
	// returns early the instant it doesn't hit a valid client, which skipped that assignment entirely
	// and silently suppressed the caster's own hand effect whenever Drain was used on empty space at
	// low levels -- while the arc branch (level 3+) has no such early return, so it always reached the
	// assignment and looked correct. Moving it here matches both the arc branch's existing behavior and
	// ForceShootLightning's sibling flag (set once, independent of whether a hit lands -- see
	// WP_ForcePowerStart's FP_LIGHTNING case). This only touches the cosmetic FX flag: the
	// BG_ForcePowerDrain cost and the regen debounce below are untouched and still only apply when a
	// target is actually hit, so this doesn't change how much Force channeling costs.
	self->client->ps.activeForcePass = self->client->ps.fd.forcePowerLevel[FP_DRAIN] + FORCE_LEVEL_3;

	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	if ( self->client->ps.fd.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_2 )
	{//arc
		vec3_t	center, mins, maxs, dir, ent_org, size, v;
		float	radius = MAX_DRAIN_DISTANCE, dot, dist;
		gentity_t	*entityList[MAX_GENTITIES];
		int			iEntityList[MAX_GENTITIES];
		int		e, numListedEntities, i;

		VectorCopy( self->client->ps.origin, center );
		for ( i = 0 ; i < 3 ; i++ )
		{
			mins[i] = center[i] - radius;
			maxs[i] = center[i] + radius;
		}
		numListedEntities = trap->EntitiesInBox( mins, maxs, iEntityList, MAX_GENTITIES );

		i = 0;
		while (i < numListedEntities)
		{
			entityList[i] = &g_entities[iEntityList[i]];

			i++;
		}

		for ( e = 0 ; e < numListedEntities ; e++ )
		{
			traceEnt = entityList[e];

			if ( !traceEnt )
				continue;
			if ( traceEnt == self )
				continue;
			if ( !traceEnt->inuse )
				continue;
			if ( !traceEnt->takedamage )
				continue;
			if ( traceEnt->health <= 0 )//no torturing corpses
				continue;
			if ( !traceEnt->client )
				continue;
			// GalaxyRP fix: [Force] nor the downed or the admin-paralyzed -- see ForceDrainDamage()
			if ( G_PlayerIsDowned( traceEnt ) || G_PlayerIsAdminParalyzed( traceEnt ) )
				continue;
			//if ( !traceEnt->client->ps.fd.forcePower ) zyk: no longer test it here
				//continue;
			if (OnSameTeam(self, traceEnt) && !g_friendlyFire.integer)
				continue;
			//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
			// find the distance from the edge of the bounding box
			for ( i = 0 ; i < 3 ; i++ )
			{
				if ( center[i] < traceEnt->r.absmin[i] )
				{
					v[i] = traceEnt->r.absmin[i] - center[i];
				} else if ( center[i] > traceEnt->r.absmax[i] )
				{
					v[i] = center[i] - traceEnt->r.absmax[i];
				} else
				{
					v[i] = 0;
				}
			}

			VectorSubtract( traceEnt->r.absmax, traceEnt->r.absmin, size );
			VectorMA( traceEnt->r.absmin, 0.5, size, ent_org );

			//see if they're in front of me
			//must be within the forward cone
			VectorSubtract( ent_org, center, dir );
			VectorNormalize( dir );
			if ( (dot = DotProduct( dir, forward )) < 0.5 )
				continue;

			//must be close enough
			dist = VectorLength( v );
			if ( dist >= radius )
			{
				continue;
			}

			//in PVS?
			if ( !traceEnt->r.bmodel && !trap->InPVS( ent_org, self->client->ps.origin ) )
			{//must be in PVS
				continue;
			}

			//Now check and see if we can actually hit it
			trap->Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT, qfalse, 0, 0 );
			if ( tr.fraction < 1.0f && tr.entityNum != traceEnt->s.number )
			{//must have clear LOS
				continue;
			}

			// ok, we are within the radius, add us to the incoming list
			ForceDrainDamage( self, traceEnt, dir, ent_org );
			gotOneOrMore = 1;
		}
	}
	else
	{//trace-line
		VectorMA( self->client->ps.origin, 2048, forward, end );

		trap->Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, end, self->s.number, MASK_SHOT, qfalse, 0, 0 );
		if ( tr.entityNum == ENTITYNUM_NONE || tr.fraction == 1.0 || tr.allsolid || tr.startsolid || !g_entities[tr.entityNum].client || !g_entities[tr.entityNum].inuse )
		{
			return 0;
		}

		traceEnt = &g_entities[tr.entityNum];

		// GalaxyRP fix: [Force] a corpse, a downed player or an admin-paralyzed one counts as a miss:
		// nothing is drained and, like any other miss on this branch, the tick costs nothing. The cone
		// above skips the same three.
		if ( traceEnt->health <= 0 || G_PlayerIsDowned( traceEnt ) || G_PlayerIsAdminParalyzed( traceEnt ) )
		{
			return 0;
		}

		ForceDrainDamage( self, traceEnt, forward, tr.endpos );
		gotOneOrMore = 1;
	}

	BG_ForcePowerDrain( &self->client->ps, FP_DRAIN, 5 ); //used to be 1, but this did, too, anger the God of Balance.

	self->client->ps.fd.forcePowerRegenDebounceTime = level.time + 500;

	return gotOneOrMore;
}

void ForceJumpCharge( gentity_t *self, usercmd_t *ucmd )
{ //I guess this is unused now. Was used for the "charge" jump type.
	float forceJumpChargeInterval = forceJumpStrength[0] / (FORCE_JUMP_CHARGE_TIME/FRAMETIME);

	if ( self->health <= 0 )
	{
		return;
	}

	if (!self->client->ps.fd.forceJumpCharge && self->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		return;
	}

	if (self->client->ps.fd.forcePower < forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION])
	{
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
		return;
	}

	if (!self->client->ps.fd.forceJumpCharge)
	{
		self->client->ps.fd.forceJumpAddTime = 0;
	}

	if (self->client->ps.fd.forceJumpAddTime >= level.time)
	{
		return;
	}

	//need to play sound
	if ( !self->client->ps.fd.forceJumpCharge )
	{
		G_Sound( self, TRACK_CHANNEL_1, G_SoundIndex("sound/weapons/force/jumpbuild.wav") );
	}

	//Increment
	if (self->client->ps.fd.forceJumpAddTime < level.time)
	{
		self->client->ps.fd.forceJumpCharge += forceJumpChargeInterval*50;
		self->client->ps.fd.forceJumpAddTime = level.time + 500;
	}

	//clamp to max strength for current level
	if ( self->client->ps.fd.forceJumpCharge > forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]] )
	{
		self->client->ps.fd.forceJumpCharge = forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]];
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
	}

	//clamp to max available force power
	if ( self->client->ps.fd.forceJumpCharge/forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME)*forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION] > self->client->ps.fd.forcePower )
	{//can't use more than you have
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
		self->client->ps.fd.forceJumpCharge = self->client->ps.fd.forcePower*forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME);
	}

	//trap->Print("%f\n", self->client->ps.fd.forceJumpCharge);
}

int WP_GetVelocityForForceJump( gentity_t *self, vec3_t jumpVel, usercmd_t *ucmd )
{
	float pushFwd = 0, pushRt = 0;
	vec3_t	view, forward, right;
	VectorCopy( self->client->ps.viewangles, view );
	view[0] = 0;
	AngleVectors( view, forward, right, NULL );
	if ( ucmd->forwardmove && ucmd->rightmove )
	{
		if ( ucmd->forwardmove > 0 )
		{
			pushFwd = 50;
		}
		else
		{
			pushFwd = -50;
		}
		if ( ucmd->rightmove > 0 )
		{
			pushRt = 50;
		}
		else
		{
			pushRt = -50;
		}
	}
	else if ( ucmd->forwardmove || ucmd->rightmove )
	{
		if ( ucmd->forwardmove > 0 )
		{
			pushFwd = 100;
		}
		else if ( ucmd->forwardmove < 0 )
		{
			pushFwd = -100;
		}
		else if ( ucmd->rightmove > 0 )
		{
			pushRt = 100;
		}
		else if ( ucmd->rightmove < 0 )
		{
			pushRt = -100;
		}
	}

	G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);

	G_PreDefSound(self->client->ps.origin, PDSOUND_FORCEJUMP);

	if (self->client->ps.fd.forceJumpCharge < JUMP_VELOCITY+40)
	{ //give him at least a tiny boost from just a tap
		self->client->ps.fd.forceJumpCharge = JUMP_VELOCITY+400;
	}

	if (self->client->ps.velocity[2] < -30)
	{ //so that we can get a good boost when force jumping in a fall
		self->client->ps.velocity[2] = -30;
	}

	VectorMA( self->client->ps.velocity, pushFwd, forward, jumpVel );
	VectorMA( self->client->ps.velocity, pushRt, right, jumpVel );
	jumpVel[2] += self->client->ps.fd.forceJumpCharge;
	if ( pushFwd > 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_FORWARD;
	}
	else if ( pushFwd < 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_BACKWARD;
	}
	else if ( pushRt > 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_RIGHT;
	}
	else if ( pushRt < 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_LEFT;
	}
	else
	{
		return FJ_UP;
	}
}

void ForceJump( gentity_t *self, usercmd_t *ucmd )
{
	float forceJumpChargeInterval;
	vec3_t	jumpVel;

	if ( self->client->ps.fd.forcePowerDuration[FP_LEVITATION] > level.time )
	{
		return;
	}
	if ( !WP_ForcePowerUsable( self, FP_LEVITATION ) )
	{
		return;
	}
	if ( self->s.groundEntityNum == ENTITYNUM_NONE )
	{
		return;
	}
	if ( self->health <= 0 )
	{
		return;
	}

	self->client->fjDidJump = qtrue;

	forceJumpChargeInterval = forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]]/(FORCE_JUMP_CHARGE_TIME/FRAMETIME);

	WP_GetVelocityForForceJump( self, jumpVel, ucmd );

	//FIXME: sound effect
	self->client->ps.fd.forceJumpZStart = self->client->ps.origin[2];//remember this for when we land
	VectorCopy( jumpVel, self->client->ps.velocity );
	//wasn't allowing them to attack when jumping, but that was annoying
	//self->client->ps.weaponTime = self->client->ps.torsoAnimTimer;

	WP_ForcePowerStart( self, FP_LEVITATION, self->client->ps.fd.forceJumpCharge/forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME)*forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION] );
	//self->client->ps.fd.forcePowerDuration[FP_LEVITATION] = level.time + self->client->ps.weaponTime;
	self->client->ps.fd.forceJumpCharge = 0;
	self->client->ps.forceJumpFlip = qtrue;

	// We've just jumped, we're not gonna be on the ground in the following frames.
	// This makes sure npc's wont trigger the ForceJump function multiple times before detecting that they have left the ground.
	self->client->ps.groundEntityNum = ENTITYNUM_NONE;
}

void WP_AddAsMindtricked(forcedata_t *fd, int entNum)
{
	if (!fd)
	{
		return;
	}

	if (entNum > 47)
	{
		fd->forceMindtrickTargetIndex4 |= (1 << (entNum-48));
	}
	else if (entNum > 31)
	{
		fd->forceMindtrickTargetIndex3 |= (1 << (entNum-32));
	}
	else if (entNum > 15)
	{
		fd->forceMindtrickTargetIndex2 |= (1 << (entNum-16));
	}
	else
	{
		fd->forceMindtrickTargetIndex |= (1 << entNum);
	}
}

qboolean ForceTelepathyCheckDirectNPCTarget( gentity_t *self, trace_t *tr, qboolean *tookPower )
{
	gentity_t	*traceEnt;
	qboolean	targetLive = qfalse;
	vec3_t		tfrom, tto, fwd;
	float		radius = MAX_TRICK_DISTANCE;

	//Check for a direct usage on NPCs first
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]*radius/2;
	tto[1] = tfrom[1] + fwd[1]*radius/2;
	tto[2] = tfrom[2] + fwd[2]*radius/2;

	trap->Trace( tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0 );


	if ( tr->entityNum == ENTITYNUM_NONE
		|| tr->fraction == 1.0f
		|| tr->allsolid
		|| tr->startsolid )
	{
		return qfalse;
	}

	traceEnt = &g_entities[tr->entityNum];

	// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, guardian mind-trick block unreachable

	return qtrue; // zyk: now always returns here

	if( traceEnt->NPC
		&& traceEnt->NPC->scriptFlags & SCF_NO_FORCE )
	{
		return qfalse;
	}

	if ( traceEnt && traceEnt->client  )
	{
		switch ( traceEnt->client->NPC_class )
		{
		case CLASS_GALAKMECH://cant grip him, he's in armor
		case CLASS_ATST://much too big to grip!
		//no droids either
		case CLASS_PROBE:
		case CLASS_GONK:
		case CLASS_R2D2:
		case CLASS_R5D2:
		case CLASS_MARK1:
		case CLASS_MARK2:
		case CLASS_MOUSE:
		case CLASS_SEEKER:
		case CLASS_REMOTE:
		case CLASS_PROTOCOL:
		case CLASS_BOBAFETT:
		case CLASS_RANCOR:
			break;
		default:
			targetLive = qtrue;
			break;
		}
	}

	if ( traceEnt->s.number < MAX_CLIENTS )
	{//a regular client
		return qfalse;
	}

	if ( targetLive && traceEnt->NPC )
	{//hit an organic non-player
		vec3_t	eyeDir;
		if ( G_ActivateBehavior( traceEnt, BSET_MINDTRICK ) )
		{//activated a script on him
			//FIXME: do the visual sparkles effect on their heads, still?
			WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
		}
		else if ( (self->NPC && traceEnt->client->playerTeam != self->client->playerTeam)
			|| (!self->NPC && traceEnt->client->playerTeam != (npcteam_t)self->client->sess.sessionTeam) )
		{//an enemy
			int override = 0;
			if ( (traceEnt->NPC->scriptFlags&SCF_NO_MIND_TRICK) )
			{
			}
			else if ( traceEnt->s.weapon != WP_SABER
				&& traceEnt->client->NPC_class != CLASS_REBORN )
			{//haha!  Jedi aren't easily confused!
				if ( self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] > FORCE_LEVEL_2 )
				{//turn them to our side
					//if mind trick 3 and aiming at an enemy need more force power
					if ( traceEnt->s.weapon != WP_NONE )
					{//don't charm people who aren't capable of fighting... like ugnaughts and droids
						int newPlayerTeam, newEnemyTeam;

						if ( traceEnt->enemy )
						{
							G_ClearEnemy( traceEnt );
						}
						if ( traceEnt->NPC )
						{
							//traceEnt->NPC->tempBehavior = BS_FOLLOW_LEADER;
							traceEnt->client->leader = self;
						}
						//FIXME: maybe pick an enemy right here?
						if ( self->NPC )
						{//NPC
							newPlayerTeam = self->client->playerTeam;
							newEnemyTeam = self->client->enemyTeam;
						}
						else
						{//client/bot
							if ( self->client->sess.sessionTeam == TEAM_BLUE )
							{//rebel
								newPlayerTeam = NPCTEAM_PLAYER;
								newEnemyTeam = NPCTEAM_ENEMY;
							}
							else if ( self->client->sess.sessionTeam == TEAM_RED )
							{//imperial
								newPlayerTeam = NPCTEAM_ENEMY;
								newEnemyTeam = NPCTEAM_PLAYER;
							}
							else
							{//neutral - wan't attack anyone
								newPlayerTeam = NPCTEAM_NEUTRAL;
								newEnemyTeam = NPCTEAM_NEUTRAL;
							}
						}
						//store these for retrieval later
						traceEnt->genericValue1 = traceEnt->client->playerTeam;
						traceEnt->genericValue2 = traceEnt->client->enemyTeam;
						traceEnt->genericValue3 = traceEnt->s.teamowner;
						//set the new values
						traceEnt->client->playerTeam = newPlayerTeam;
						traceEnt->client->enemyTeam = newEnemyTeam;
						traceEnt->s.teamowner = newPlayerTeam;
						//FIXME: need a *charmed* timer on this...?  Or do TEAM_PLAYERS assume that "confusion" means they should switch to team_enemy when done?
						traceEnt->NPC->charmedTime = level.time + mindTrickTime[self->client->ps.fd.forcePowerLevel[FP_TELEPATHY]];
					}
				}
				else
				{//just confuse them
					//somehow confuse them?  Set don't fire to true for a while?  Drop their aggression?  Maybe just take their enemy away and don't let them pick one up for a while unless shot?
					traceEnt->NPC->confusionTime = level.time + mindTrickTime[self->client->ps.fd.forcePowerLevel[FP_TELEPATHY]];//confused for about 10 seconds
					NPC_PlayConfusionSound( traceEnt );
					if ( traceEnt->enemy )
					{
						G_ClearEnemy( traceEnt );
					}
				}
			}
			else
			{
				NPC_Jedi_PlayConfusionSound( traceEnt );
			}
			WP_ForcePowerStart( self, FP_TELEPATHY, override );
		}
		else if ( traceEnt->client->playerTeam == self->client->playerTeam )
		{//an ally
			//maybe just have him look at you?  Respond?  Take your enemy?
			if ( traceEnt->client->ps.pm_type < PM_DEAD && traceEnt->NPC!=NULL && !(traceEnt->NPC->scriptFlags&SCF_NO_RESPONSE) )
			{
				NPC_UseResponse( traceEnt, self, qfalse );
				WP_ForcePowerStart( self, FP_TELEPATHY, 1 );
			}
		}//NOTE: no effect on TEAM_NEUTRAL?
		AngleVectors( traceEnt->client->renderInfo.eyeAngles, eyeDir, NULL, NULL );
		VectorNormalize( eyeDir );
		G_PlayEffectID( G_EffectIndex( "force/force_touch" ), traceEnt->client->renderInfo.eyePoint, eyeDir );

		//make sure this plays and that you cannot press fire for about 1 second after this
		//FIXME: BOTH_FORCEMINDTRICK or BOTH_FORCEDISTRACT
		//NPC_SetAnim( self, SETANIM_TORSO, BOTH_MINDTRICK1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_RESTART|SETANIM_FLAG_HOLD );
		//FIXME: build-up or delay this until in proper part of anim
	}
	else
	{
		if ( self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] > FORCE_LEVEL_1 && tr->fraction * 2048 > 64 )
		{//don't create a diversion less than 64 from you of if at power level 1
			//use distraction anim instead
			G_PlayEffectID( G_EffectIndex( "force/force_touch" ), tr->endpos, tr->plane.normal );
			//FIXME: these events don't seem to always be picked up...?
			AddSoundEvent( self, tr->endpos, 512, AEL_SUSPICIOUS, qtrue );//, qtrue );
			AddSightEvent( self, tr->endpos, 512, AEL_SUSPICIOUS, 50 );
			WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			*tookPower = qtrue;
		}
		//NPC_SetAnim( self, SETANIM_TORSO, BOTH_MINDTRICK2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_RESTART|SETANIM_FLAG_HOLD );
	}
	//self->client->ps.saberMove = self->client->ps.saberBounceMove = LS_READY;//don't finish whatever saber anim you may have been in
	self->client->ps.saberBlocked = BLOCKED_NONE;
	self->client->ps.weaponTime = 1000;
	/*
	if ( self->client->ps.fd.forcePowersActive&(1<<FP_SPEED) )
	{
		self->client->ps.weaponTime = floor( self->client->ps.weaponTime * g_timescale->value );
	}
	*/
	return qtrue;
}

void ForceTelepathy(gentity_t *self)
{
	trace_t tr;
	vec3_t tto, thispush_org, a;
	vec3_t mins, maxs, fwdangles, forward, right, center;
	int i;
	float visionArc = 0;
	float radius = MAX_TRICK_DISTANCE;
	qboolean	tookPower = qfalse;

	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if (self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG])
	{ //can't mindtrick while carrying the flag
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_TELEPATHY)) )
	{
		WP_ForcePowerStop( self, FP_TELEPATHY );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_TELEPATHY ) )
	{
		return;
	}

	// fix: rocket lock bug
	BG_ClearRocketLock(&self->client->ps);

	if ( ForceTelepathyCheckDirectNPCTarget( self, &tr, &tookPower ) )
	{//hit an NPC directly
		/* zyk: no longer executes this
		self->client->ps.forceAllowDeactivateTime = level.time + 1500;
		G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );
		self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
		self->client->ps.forceHandExtendTime = level.time + 1000;
		return;
		*/
	}

	if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_2)
	{
		visionArc = 90;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_3)
	{
		visionArc = 180;
		radius = MAX_TRICK_DISTANCE * 1.5f;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_4)
	{
		visionArc = 270;
		radius = MAX_TRICK_DISTANCE * 2.0f;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_5)
	{
		visionArc = 360;
		radius = MAX_TRICK_DISTANCE * 2.5f;
	}

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ )
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
	{
		if ((tr.fraction != 1.0 &&
			tr.entityNum != ENTITYNUM_NONE &&
			g_entities[tr.entityNum].inuse &&
			g_entities[tr.entityNum].client &&
			g_entities[tr.entityNum].client->pers.connected &&
			g_entities[tr.entityNum].client->sess.sessionTeam != TEAM_SPECTATOR) || 
			(g_entities[tr.entityNum].client && g_entities[tr.entityNum].NPC))
		{
			gentity_t *tricked_entity;

			tricked_entity = &g_entities[tr.entityNum];

			// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, guardian mind-trick check unreachable

			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist branch unreachable

			if (!ForcePowerUsableOn(self, tricked_entity, FP_TELEPATHY))
			{
				return;
			}

			if (!tricked_entity->NPC) // zyk: NPCs wont have the glowing head effect of mind trick because of how the game handles the tricked entities
				WP_AddAsMindtricked(&self->client->ps.fd, tr.entityNum);

			if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] < tricked_entity->client->ps.fd.forcePowerLevel[FP_SEE]) {
				return;
			}

			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, mind-control-target block unreachable

			if ( !tookPower )
			{
				WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			}

			//G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );

			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;

			return;
		}
		else
		{
			return;
		}
	}
	else	//level 2 & 3
	{
		gentity_t *ent;
		int entityList[MAX_GENTITIES];
		int numListedEntities;
		int e = 0;
		qboolean gotatleastone = qfalse;

		numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

		while (e < numListedEntities)
		{
			ent = &g_entities[entityList[e]];

			if (ent)
			{ //not in the arc, don't consider it
				if (ent->client)
				{
					VectorCopy(ent->client->ps.origin, thispush_org);
				}
				else
				{
					VectorCopy(ent->s.pos.trBase, thispush_org);
				}
				VectorCopy(self->client->ps.origin, tto);
				tto[2] += self->client->ps.viewheight;
				VectorSubtract(thispush_org, tto, a);
				vectoangles(a, a);

				if (!ent->client)
				{
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (!InFieldOfVision(self->client->ps.viewangles, visionArc, a))
				{ //only bother with arc rules if the victim is a client
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (!ForcePowerUsableOn(self, ent, FP_TELEPATHY))
				{
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (OnSameTeam(self, ent))
				{
					entityList[e] = ENTITYNUM_NONE;
				}
			}
			ent = &g_entities[entityList[e]];
			if (ent && ent != self && ent->client)
			{
				gotatleastone = qtrue;

				// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, guardian mind-trick check unreachable

				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist branch unreachable

				if (!ent->NPC) // zyk: NPCs wont have the glowing head effect of mind trick because of how the game handles the tricked entities
					WP_AddAsMindtricked(&self->client->ps.fd, ent->s.number);

				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, mind-control-target block unreachable
			}
			e++;
		}

		if (gotatleastone)
		{
			self->client->ps.forceAllowDeactivateTime = level.time + 1500;

			if ( !tookPower )
			{
				WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			}

			//G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );

			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;
		}
	}

}

void GEntity_UseFunc( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	GlobalUse(self, other, activator);
}

qboolean CanCounterThrow(gentity_t *self, gentity_t *thrower, qboolean pull)
{
	int powerUse = 0;

	// DAJ_RP: [NPC System] a frozen NPC does not push or pull back: it is thrown like anyone who cannot
	// resist -- see RP_NpcFreeze() (NPC_spawn.c)
	if (RP_NpcIsFrozen(self))
	{
		return 0;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return 0;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return 0;
	}

	if ( self->health <= 0 )
	{
		return 0;
	}

	if ( self->client->ps.powerups[PW_DISINT_4] > level.time )
	{
		return 0;
	}

	if (self->client->ps.weaponstate == WEAPON_CHARGING ||
		self->client->ps.weaponstate == WEAPON_CHARGING_ALT)
	{ //don't autodefend when charging a weapon
		return 0;
	}

	if (level.gametype == GT_SIEGE &&
		pull &&
		thrower && thrower->client)
	{ //in siege, pull will affect people if they are not facing you, so they can't run away so much
		vec3_t d;
		float a;

        VectorSubtract(thrower->client->ps.origin, self->client->ps.origin, d);
		vectoangles(d, d);

        a = AngleSubtract(d[YAW], self->client->ps.viewangles[YAW]);

		if (a > 60.0f || a < -60.0f)
		{ //if facing more than 60 degrees away they cannot defend
			return 0;
		}
	}

	if (pull)
	{
		powerUse = FP_PULL;
	}
	else
	{
		powerUse = FP_PUSH;
	}

	if ( !WP_ForcePowerUsable( self, powerUse ) )
	{
		return 0;
	}

	if (self->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{ //you cannot counter a push/pull if you're in the air
		return 0;
	}

	return 1;
}

qboolean G_InGetUpAnim(playerState_t *ps)
{
	switch( (ps->legsAnim) )
	{
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
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	}

	switch( (ps->torsoAnim) )
	{
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
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	}

	return qfalse;
}
void G_LetGoOfWall( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return;
	}
	ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
	if ( BG_InReboundJump( ent->client->ps.legsAnim )
		|| BG_InReboundHold( ent->client->ps.legsAnim ) )
	{
		ent->client->ps.legsTimer = 0;
	}
	if ( BG_InReboundJump( ent->client->ps.torsoAnim )
		|| BG_InReboundHold( ent->client->ps.torsoAnim ) )
	{
		ent->client->ps.torsoTimer = 0;
	}
}

float forcePushPullRadius[NUM_FORCE_POWER_LEVELS] =
{
	0,//none
	384,//256,
	448,//384,
	512
};
//rwwFIXMEFIXME: incorporate this into the below function? Currently it's only being used by jedi AI

extern void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace );

/*
===============
RP_ForceUsableStatic / RP_ForceStaticSeen

DAJ_RP: [Force Powers] a func_static that Force Push (F_PUSH, 1) or Pull (F_PULL, 2) uses can be an invisible brush that
is not solid at all: textures/system/caulk_nonsolid, whose only content is CONTENTS_OPAQUE -- kor1 (three), kor2 (the
pillar, push_brush), t1_surprise, vjun2 (secret_crate) and yavin2 (open_door_switch) among the stock single player maps.
Single player's push and pull traces take CONTENTS_OPAQUE (its MASK_OPAQUE and MASK_FORCE_PUSH, wp_saber.cpp); ours,
MASK_PLAYERSOLID and MASK_SHOT, do not: the aimed (level 1) trace went through such a brush, and the area one found it but
judged it out of sight whenever something solid stood between the player and its centre. So, for these only:
  - level 1: when our trace finds no entity (nothing, or the world), the same trace that also stops at opaque brushes,
    taken only if the first thing it meets is such a func_static for this power; a target our trace finds always wins;
  - levels 2-5: such a func_static our line-of-sight test calls hidden is in sight when a line from the eyes that also
    stops at opaque brushes reaches it first, single player's own test.
Either trace stops at everything ours does and at opaque brushes besides, so it can only ever add such a func_static:
no other target, and no solid func_static, is pushed, pulled or skipped differently.
===============
*/
static qboolean RP_ForceUsableStatic( const gentity_t *ent, qboolean pull )
{
	return ( ent && ent->inuse && !ent->client && ent->classname && !Q_stricmp( ent->classname, "func_static" )
		&& ( ent->spawnflags & ( pull ? 2/*F_PULL*/ : 1/*F_PUSH*/ ) ) ) ? qtrue : qfalse;
}

static qboolean RP_ForceStaticSeen( gentity_t *self, gentity_t *ent, const vec3_t eyePoint, const vec3_t ent_org, qboolean pull )
{
	trace_t	tr;

	if ( !RP_ForceUsableStatic( ent, pull ) )
	{
		return qfalse;
	}
	trap->Trace( &tr, eyePoint, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT|CONTENTS_OPAQUE, qfalse, 0, 0 );
	return ( tr.fraction == 1.0f || tr.entityNum == ent->s.number ) ? qtrue : qfalse;
}

void ForceThrow( gentity_t *self, qboolean pull )
{
	//shove things in front of you away
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	gentity_t	*push_list[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	int			i, e;
	int			ent_count = 0;
	int			radius = 1024; //since it's view-based now. //350;
	int			powerLevel;
	int			visionArc;
	int			pushPower;
	int			pushPowerMod;
	vec3_t		center, ent_org, size, forward, right, end, dir, fwdangles = {0};
	float		dot1;
	trace_t		tr;
	int			x;
	vec3_t		pushDir;
	vec3_t		thispush_org;
	vec3_t		tfrom, tto, fwd, a;
	int			powerUse = 0;

	visionArc = 0;

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE && (self->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN || !G_InGetUpAnim(&self->client->ps)))
	{
		return;
	}

	if (!g_useWhileThrowing.integer && self->client->ps.saberInFlight)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if ( self->health <= 0 )
	{
		return;
	}
	if ( self->client->ps.powerups[PW_DISINT_4] > level.time )
	{
		return;
	}
	if (pull)
	{
		powerUse = FP_PULL;
	}
	else
	{
		powerUse = FP_PUSH;
	}

	if ( !WP_ForcePowerUsable( self, powerUse ) )
	{
		return;
	}

	// fix: rocket lock bug
	BG_ClearRocketLock(&self->client->ps);

	if (!pull && self->client->ps.saberLockTime > level.time && self->client->ps.saberLockFrame)
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
		self->client->ps.powerups[PW_DISINT_4] = level.time + 1500;

		self->client->ps.saberLockHits += self->client->ps.fd.forcePowerLevel[FP_PUSH]*2;

		WP_ForcePowerStart( self, FP_PUSH, 0 );
		return;
	}

	WP_ForcePowerStart( self, powerUse, 0 );

	//make sure this plays and that you cannot press fire for about 1 second after this
	if ( pull )
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/pull.wav" ) );
		if (self->client->ps.forceHandExtend == HANDEXTEND_NONE)
		{
			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPULL;
			if ( level.gametype == GT_SIEGE && self->client->ps.weapon == WP_SABER )
			{//hold less so can attack right after a pull
				self->client->ps.forceHandExtendTime = level.time + 200;
			}
			else
			{
				self->client->ps.forceHandExtendTime = level.time + 400;
			}
		}
		self->client->ps.powerups[PW_DISINT_4] = self->client->ps.forceHandExtendTime + 200;
		self->client->ps.powerups[PW_PULL] = self->client->ps.powerups[PW_DISINT_4];
	}
	else
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
		if (self->client->ps.forceHandExtend == HANDEXTEND_NONE)
		{
			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;
		}
		else if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN && G_InGetUpAnim(&self->client->ps))
		{
			if (self->client->ps.forceDodgeAnim > 4)
			{
				self->client->ps.forceDodgeAnim -= 8;
			}
			self->client->ps.forceDodgeAnim += 8; //special case, play push on upper torso, but keep playing current knockdown anim on legs
		}
		self->client->ps.powerups[PW_DISINT_4] = level.time + 1100;
		self->client->ps.powerups[PW_PULL] = 0;
	}

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ )
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}


	if (pull)
	{
		powerLevel = self->client->ps.fd.forcePowerLevel[FP_PULL];
		pushPower = 256*self->client->ps.fd.forcePowerLevel[FP_PULL];
	}
	else
	{
		powerLevel = self->client->ps.fd.forcePowerLevel[FP_PUSH];
		pushPower = 256*self->client->ps.fd.forcePowerLevel[FP_PUSH];
	}

	if (!powerLevel)
	{ //Shouldn't have made it here..
		return;
	}

	if (powerLevel == FORCE_LEVEL_2)
	{
		visionArc = 60;
	}
	else if (powerLevel == FORCE_LEVEL_3)
	{
		visionArc = 180;
	}
	// GalaxyRP (Alex): [Force Powers] Wider vision for levels 4 and 5
	else if (powerLevel == FORCE_LEVEL_4)
	{
		visionArc = 270;
	}
	else if (powerLevel == FORCE_LEVEL_5)
	{
		visionArc = 360;
	}

	if (powerLevel == FORCE_LEVEL_1)
	{ //can only push/pull targeted things at level 1
		VectorCopy(self->client->ps.origin, tfrom);
		tfrom[2] += self->client->ps.viewheight;
		AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
		tto[0] = tfrom[0] + fwd[0]*radius/2;
		tto[1] = tfrom[1] + fwd[1]*radius/2;
		tto[2] = tfrom[2] + fwd[2]*radius/2;

		// zyk: added CONTENTS_TRIGGER so items are traced
		trap->Trace(&tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID|CONTENTS_TRIGGER, qfalse, 0, 0);

		// DAJ_RP: [Force Powers] nothing found: a non-solid push/pull func_static in the way? (RP_ForceUsableStatic)
		if (tr.fraction == 1.0 || tr.entityNum == ENTITYNUM_NONE || tr.entityNum == ENTITYNUM_WORLD)
		{
			trace_t opaqueTr;
			trap->Trace(&opaqueTr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID|CONTENTS_TRIGGER|CONTENTS_OPAQUE, qfalse, 0, 0);
			if (opaqueTr.fraction != 1.0 && opaqueTr.entityNum >= 0 && opaqueTr.entityNum < ENTITYNUM_WORLD &&
				RP_ForceUsableStatic(&g_entities[opaqueTr.entityNum], pull))
			{
				tr = opaqueTr;
			}
		}

		if (tr.fraction != 1.0 &&
			tr.entityNum != ENTITYNUM_NONE)
		{
			if (!g_entities[tr.entityNum].client && g_entities[tr.entityNum].s.eType == ET_NPC)
			{ //g2animent
				if (g_entities[tr.entityNum].s.genericenemyindex < level.time)
				{
					g_entities[tr.entityNum].s.genericenemyindex = level.time + 2000;
				}
			}

			numListedEntities = 0;
			entityList[numListedEntities] = tr.entityNum;

			if (pull)
			{
				if (!ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_PULL))
				{
					return;
				}
			}
			else
			{
				if (!ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_PUSH))
				{
					return;
				}
			}

			// zyk: if a CONTENTS_TRIGGER was traced, must be an item
			if (g_entities[tr.entityNum].r.contents == CONTENTS_TRIGGER && g_entities[tr.entityNum].s.eType != ET_ITEM)
				return;

			numListedEntities++;
		}
		else
		{
			//didn't get anything, so just
			return;
		}
	}
	else
	{
		numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

		e = 0;

		while (e < numListedEntities)
		{
			ent = &g_entities[entityList[e]];

			if (!ent->client && ent->s.eType == ET_NPC)
			{ //g2animent
				if (ent->s.genericenemyindex < level.time)
				{
					ent->s.genericenemyindex = level.time + 2000;
				}
			}

			if (ent)
			{
				if (ent->client)
				{
					VectorCopy(ent->client->ps.origin, thispush_org);
				}
				else
				{
					VectorCopy(ent->s.pos.trBase, thispush_org);
				}
			}

			if (ent)
			{ //not in the arc, don't consider it
				VectorCopy(self->client->ps.origin, tto);
				tto[2] += self->client->ps.viewheight;
				VectorSubtract(thispush_org, tto, a);
				vectoangles(a, a);

				if (ent->client && !InFieldOfVision(self->client->ps.viewangles, visionArc, a) &&
					ForcePowerUsableOn(self, ent, powerUse))
				{ //only bother with arc rules if the victim is a client
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (ent->client)
				{
					if (pull)
					{
						if (!ForcePowerUsableOn(self, ent, FP_PULL))
						{
							entityList[e] = ENTITYNUM_NONE;
						}
					}
					else
					{
						if (!ForcePowerUsableOn(self, ent, FP_PUSH))
						{
							entityList[e] = ENTITYNUM_NONE;
						}
					}
				}
			}
			e++;
		}
	}

	for ( e = 0 ; e < numListedEntities ; e++ )
	{
		if (entityList[e] != ENTITYNUM_NONE &&
			entityList[e] >= 0 &&
			entityList[e] < MAX_GENTITIES)
		{
			ent = &g_entities[entityList[e]];
		}
		else
		{
			ent = NULL;
		}

		if (!ent)
			continue;
		if (ent == self)
			continue;

		/* zyk: now it is possible to use force push or pull in target clients which are in the same team
		if (ent->client && OnSameTeam(ent, self))
		{
			continue;
		}
		*/

		if ( !(ent->inuse) )
		{
			continue;
		}

		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier ysalamiri-resist branch unreachable

		if ( ent->s.eType != ET_MISSILE )
		{
			if ( ent->s.eType != ET_ITEM )
			{
				//FIXME: need pushable objects
				if ( Q_stricmp( "func_button", ent->classname ) == 0 )
				{//we might push it
					if ( pull || !(ent->spawnflags&SPF_BUTTON_FPUSHABLE) )
					{//not force-pushable, never pullable
						continue;
					}
				}
				else
				{
					if ( ent->s.eFlags & EF_NODRAW )
					{
						continue;
					}
					if ( !ent->client )
					{
						if ( Q_stricmp( "lightsaber", ent->classname ) != 0 )
						{//not a lightsaber
							if ( Q_stricmp( "func_door", ent->classname ) != 0 || !(ent->spawnflags & 2/*MOVER_FORCE_ACTIVATE*/) )
							{//not a force-usable door
								if ( Q_stricmp( "func_static", ent->classname ) != 0 || (!(ent->spawnflags&1/*F_PUSH*/)&&!(ent->spawnflags&2/*F_PULL*/)) )
								{//not a force-usable func_static
									if ( Q_stricmp( "limb", ent->classname ) )
									{//not a limb
										continue;
									}
								}
							}
							else if ( ent->moverState != MOVER_POS1 && ent->moverState != MOVER_POS2 )
							{//not at rest
								continue;
							}
						}
					}
					else if ( ent->client->NPC_class == CLASS_GALAKMECH
						|| ent->client->NPC_class == CLASS_ATST
						|| ent->client->NPC_class == CLASS_RANCOR )
					{//can't push ATST or Galak or Rancor
						continue;
					}
				}
			}
		}
		else
		{
			if ( ent->s.pos.trType == TR_STATIONARY && (ent->s.eFlags&EF_MISSILE_STICK) )
			{//can't force-push/pull stuck missiles (detpacks, tripmines)
				continue;
			}
			if ( ent->s.pos.trType == TR_STATIONARY && ent->s.weapon != WP_THERMAL )
			{//only thermal detonators can be pushed once stopped
				continue;
			}
		}

		//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ )
		{
			if ( center[i] < ent->r.absmin[i] )
			{
				v[i] = ent->r.absmin[i] - center[i];
			} else if ( center[i] > ent->r.absmax[i] )
			{
				v[i] = center[i] - ent->r.absmax[i];
			} else
			{
				v[i] = 0;
			}
		}

		VectorSubtract( ent->r.absmax, ent->r.absmin, size );
		VectorMA( ent->r.absmin, 0.5, size, ent_org );

		VectorSubtract( ent_org, center, dir );
		VectorNormalize( dir );
		if ( (dot1 = DotProduct( dir, forward )) < 0.6 )
			continue;

		dist = VectorLength( v );

		//Now check and see if we can actually deflect it
		//method1
		//if within a certain range, deflect it
		if ( dist >= radius )
		{
			continue;
		}

		//in PVS?
		if ( !ent->r.bmodel && !trap->InPVS( ent_org, self->client->ps.origin ) )
		{//must be in PVS
			continue;
		}

		//really should have a clear LOS to this thing...
		trap->Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT, qfalse, 0, 0 );
		if ( tr.fraction < 1.0f && tr.entityNum != ent->s.number )
		{//must have clear LOS
			//try from eyes too before you give up
			vec3_t eyePoint;
			VectorCopy(self->client->ps.origin, eyePoint);
			eyePoint[2] += self->client->ps.viewheight;
			trap->Trace( &tr, eyePoint, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT, qfalse, 0, 0 );

			if ( tr.fraction < 1.0f && tr.entityNum != ent->s.number
				&& !RP_ForceStaticSeen( self, ent, eyePoint, ent_org, pull ) )	// DAJ_RP: [Force Powers] a non-solid push/pull func_static
			{
				continue;
			}
		}

		// ok, we are within the radius, add us to the incoming list
		push_list[ent_count] = ent;
		ent_count++;
	}

	if ( ent_count )
	{
		//method1:
		for ( x = 0; x < ent_count; x++ )
		{
			int modPowerLevel = powerLevel;


			if (push_list[x]->client)
			{
				modPowerLevel = WP_AbsorbConversion(push_list[x], push_list[x]->client->ps.fd.forcePowerLevel[FP_ABSORB], self, powerUse, powerLevel, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[powerUse]][powerUse]);
				if (modPowerLevel == -1)
				{
					modPowerLevel = powerLevel;
				}
			}

			pushPower = 256*modPowerLevel;

			if (push_list[x]->client)
			{
				VectorCopy(push_list[x]->client->ps.origin, thispush_org);
			}
			else
			{
				VectorCopy(push_list[x]->s.origin, thispush_org);
			}

			if ( push_list[x]->client )
			{//FIXME: make enemy jedi able to hunker down and resist this?
				int otherPushPower = push_list[x]->client->ps.fd.forcePowerLevel[powerUse];
				qboolean canPullWeapon = qtrue;
				float dirLen = 0;

				if ( g_debugMelee.integer )
				{
					if ( (push_list[x]->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
					{//no resistance if stuck to wall
						//push/pull them off the wall
						otherPushPower = 0;
						G_LetGoOfWall( push_list[x] );
					}
				}

				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Armored Soldier push/pull resist branch unreachable

				pushPowerMod = pushPower;

				if (push_list[x]->client->pers.cmd.forwardmove ||
					push_list[x]->client->pers.cmd.rightmove)
				{ //if you are moving, you get one less level of defense
					otherPushPower--;

					if (otherPushPower < 0)
					{
						otherPushPower = 0;
					}
				}

				if (otherPushPower && CanCounterThrow(push_list[x], self, pull))
				{
					if ( pull )
					{
						G_Sound( push_list[x], CHAN_BODY, G_SoundIndex( "sound/weapons/force/pull.wav" ) );
						push_list[x]->client->ps.forceHandExtend = HANDEXTEND_FORCEPULL;
						push_list[x]->client->ps.forceHandExtendTime = level.time + 400;
					}
					else
					{
						G_Sound( push_list[x], CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
						push_list[x]->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
						push_list[x]->client->ps.forceHandExtendTime = level.time + 1000;
					}
					push_list[x]->client->ps.powerups[PW_DISINT_4] = push_list[x]->client->ps.forceHandExtendTime + 200;

					if (pull)
					{
						push_list[x]->client->ps.powerups[PW_PULL] = push_list[x]->client->ps.powerups[PW_DISINT_4];
					}
					else
					{
						push_list[x]->client->ps.powerups[PW_PULL] = 0;
					}

					//Make a counter-throw effect

					if (otherPushPower >= modPowerLevel)
					{
						pushPowerMod = 0;
						canPullWeapon = qfalse;
					}
					else
					{
						int powerDif = (modPowerLevel - otherPushPower);

						if (powerDif >= 3)
						{
							pushPowerMod -= pushPowerMod*0.2;
						}
						else if (powerDif == 2)
						{
							pushPowerMod -= pushPowerMod*0.4;
						}
						else if (powerDif == 1)
						{
							pushPowerMod -= pushPowerMod*0.8;
						}

						if (pushPowerMod < 0)
						{
							pushPowerMod = 0;
						}
					}
				}

				//shove them
				if ( pull )
				{
					VectorSubtract( self->client->ps.origin, thispush_org, pushDir );

					if (push_list[x]->client && VectorLength(pushDir) <= 256)
					{
						int randfact = 0;

						// GalaxyRP fix: [Force Powers] >= FORCE_LEVEL_3 for the top arm, not ==.
						//
						// forcePowerLevels_t runs to FORCE_LEVEL_5 and initialize_rpg_skills() feeds
						// forcePowerLevel[FP_PULL] straight from skill_levels[2], whose Pull skill maxes
						// at 5 -- so a character with Pull 4 or 5 matched none of these three arms,
						// randfact stayed 0, and "Q_irand(1, 10) <= 0" is never true. The disarm below
						// could not fire at all, against players or NPCs. Levelling Pull past 3 removed
						// the ability outright: 100% at level 3, 0% at levels 4 and 5.
						//
						// Level 3 already means "always", so 4 and 5 land on the same 10 rather than
						// anything stronger -- there is nothing above certainty to give them.
						//
						// The visionArc chain near the top of this function was extended for levels 4
						// and 5 when the cap was raised; this chain and the knockdown below were the two
						// the sweep missed. Fourteen of the seventeen exact FORCE_LEVEL_3 tests in this
						// file handle 4/5 already, DoGripAction() covers them with its own
						// "gripLevel > FORCE_LEVEL_3" branch, and these two were the remainder.
						if (modPowerLevel == FORCE_LEVEL_1)
						{
							randfact = 3;
						}
						else if (modPowerLevel == FORCE_LEVEL_2)
						{
							randfact = 7;
						}
						else if (modPowerLevel >= FORCE_LEVEL_3)
						{
							randfact = 10;
						}

						// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Stealth Attacker pull-protect branch unreachable

						// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, guardian pull-protect branch unreachable

						if (push_list[x]->NPC && push_list[x]->client->NPC_class == CLASS_VEHICLE)
						{ // zyk: cannot pull anything from vehicles
							canPullWeapon = qfalse;
						}

						if (!OnSameTeam(self, push_list[x]) && Q_irand(1, 10) <= randfact && canPullWeapon)
						{
							vec3_t uorg, vecnorm;

							VectorCopy(self->client->ps.origin, uorg);
							uorg[2] += 64;

							VectorSubtract(uorg, thispush_org, vecnorm);
							VectorNormalize(vecnorm);

							TossClientWeapon(push_list[x], vecnorm, 500);
						}
					}
				}
				else
				{
					VectorSubtract( thispush_org, self->client->ps.origin, pushDir );
				}

				if ((modPowerLevel > otherPushPower || push_list[x]->client->ps.m_iVehicleNum) && push_list[x]->client)
				{
					// GalaxyRP fix: [Force Powers] >= FORCE_LEVEL_3, same cause as the disarm chain above:
					// this is the level-3 knockdown, and an exact test meant Push or Pull at 4 or 5 --
					// reachable since the force cap was raised to 5 -- knocked nobody down at all. It
					// cost both powers their signature effect at exactly the levels a player pays most
					// for.
					//
					// Note the reach below is "64 * ((modPowerLevel - otherPushPower) - 1)", which was
					// already written to scale with the attacker's level and has simply never run above
					// 3. Letting it do so is the point of the fix rather than a separate buff, and it
					// stays self-balancing: it keys on the DIFFERENCE between attacker and defender, so
					// a level 5 push still only reaches 64 units against someone defending at level 3,
					// the same as level 3 against a level 1.
					if (modPowerLevel >= FORCE_LEVEL_3 &&
						push_list[x]->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
					{
						dirLen = VectorLength(pushDir);

						if (BG_KnockDownable(&push_list[x]->client->ps) &&
							dirLen <= (64*((modPowerLevel - otherPushPower)-1)))
						{ //can only do a knockdown if fairly close
							// GalaxyRP fix: [Dead Code] rpg_class permanently 0, condition always true (amrpgmode!=2 || rpg_class!=3 || ...)
							// GalaxyRP fix: [Dead Code] guardian_mode/guardian_invoked_by_id permanently 0/-1, condition always true
							push_list[x]->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							push_list[x]->client->ps.forceHandExtendTime = level.time + 700;
							push_list[x]->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
							push_list[x]->client->ps.quickerGetup = qtrue;
						}
						else if (push_list[x]->s.number < MAX_CLIENTS && push_list[x]->client->ps.m_iVehicleNum &&
							dirLen <= 128.0f )
						{ //a player on a vehicle
							gentity_t *vehEnt = &g_entities[push_list[x]->client->ps.m_iVehicleNum];
							if (vehEnt->inuse && vehEnt->client && vehEnt->m_pVehicle)
							{
								if (vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER ||
									vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
								{ //push the guy off
									vehEnt->m_pVehicle->m_pVehicleInfo->Eject(vehEnt->m_pVehicle, (bgEntity_t *)push_list[x], qfalse);
								}
							}
						}
					}
				}

				if (!dirLen)
				{
					dirLen = VectorLength(pushDir);
				}

				VectorNormalize(pushDir);

				if (push_list[x]->client)
				{
					//escape a force grip if we're in one
					if (self->client->ps.fd.forceGripBeingGripped > level.time)
					{ //force the enemy to stop gripping me if I managed to push him
						if (push_list[x]->client->ps.fd.forceGripEntityNum == self->s.number)
						{
							if (modPowerLevel >= push_list[x]->client->ps.fd.forcePowerLevel[FP_GRIP])
							{ //only break the grip if our push/pull level is >= their grip level
								WP_ForcePowerStop(push_list[x], FP_GRIP);
								self->client->ps.fd.forceGripBeingGripped = 0;
								push_list[x]->client->ps.fd.forceGripUseTime = level.time + 1000; //since we just broke out of it..
							}
						}
					}

					push_list[x]->client->ps.otherKiller = self->s.number;
					push_list[x]->client->ps.otherKillerTime = level.time + 5000;
					push_list[x]->client->ps.otherKillerDebounceTime = level.time + 100;

					pushPowerMod -= (dirLen*0.7);
					if (pushPowerMod < 16)
					{
						pushPowerMod = 16;
					}

					//fullbody push effect
					push_list[x]->client->pushEffectTime = level.time + 600;

					push_list[x]->client->ps.velocity[0] = pushDir[0]*pushPowerMod;
					push_list[x]->client->ps.velocity[1] = pushDir[1]*pushPowerMod;

					if ((int)push_list[x]->client->ps.velocity[2] == 0)
					{ //if not going anywhere vertically, boost them up a bit
						push_list[x]->client->ps.velocity[2] = pushDir[2]*pushPowerMod;

						if (push_list[x]->client->ps.velocity[2] < 128)
						{
							push_list[x]->client->ps.velocity[2] = 128;
						}
					}
					else
					{
						push_list[x]->client->ps.velocity[2] = pushDir[2]*pushPowerMod;
					}
				}
			}
			else if ( push_list[x]->s.eType == ET_MISSILE && push_list[x]->s.pos.trType != TR_STATIONARY && (push_list[x]->s.pos.trType != TR_INTERPOLATE||push_list[x]->s.weapon != WP_THERMAL) )//rolling and stationary thermal detonators are dealt with below
			{
				if ( pull )
				{//deflect rather than reflect?
				}
				else
				{
					// zyk: do not push missiles coming from allies
					if (zyk_is_ally(self,push_list[x]) == qfalse)
					{
						G_ReflectMissile( self, push_list[x], forward );
					}
				}
			}
			else if ( !Q_stricmp( "func_static", push_list[x]->classname ) )
			{//force-usable func_static
				if ( !pull && (push_list[x]->spawnflags&1/*F_PUSH*/) )
				{
					GEntity_UseFunc( push_list[x], self, self );
				}
				else if ( pull && (push_list[x]->spawnflags&2/*F_PULL*/) )
				{
					GEntity_UseFunc( push_list[x], self, self );
				}
			}
			else if ( !Q_stricmp( "func_door", push_list[x]->classname ) && (push_list[x]->spawnflags&2) )
			{//push/pull the door
				vec3_t	pos1, pos2;
				vec3_t	trFrom;

				VectorCopy(self->client->ps.origin, trFrom);
				trFrom[2] += self->client->ps.viewheight;

				AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
				VectorNormalize( forward );
				VectorMA( trFrom, radius, forward, end );
				trap->Trace( &tr, trFrom, vec3_origin, vec3_origin, end, self->s.number, MASK_SHOT, qfalse, 0, 0 );
				if ( tr.entityNum != push_list[x]->s.number || tr.fraction == 1.0 || tr.allsolid || tr.startsolid )
				{//must be pointing right at it
					continue;
				}

				// DAJ_RP: [Entity System] where the door's middle is at each end: its origin there (pos1, pos2) plus the
				// middle of its own brushes (r.mins/r.maxs, the brush model's bounds), since a brush entity is drawn as its
				// brushes moved by its origin. Stock took pos1 and pos2 themselves for a door with an origin, as if that
				// origin were where the door is -- true of an origin brush, but a copy placed with /entcopy, /entaddaim or
				// /entaddgrab (or /entadd with an origin) gets as its origin how far it was moved, so the side test below
				// was made far from the door and push and pull swapped on one side of it. For a door without an origin
				// this is the same point stock worked out from its absmin/absmax.
				{
					vec3_t	mid;

					VectorAdd( push_list[x]->r.mins, push_list[x]->r.maxs, mid );
					VectorScale( mid, 0.5f, mid );
					VectorAdd( push_list[x]->pos1, mid, pos1 );
					VectorAdd( push_list[x]->pos2, mid, pos2 );
				}

				if ( Distance( pos1, trFrom ) < Distance( pos2, trFrom ) )
				{//pos1 is closer
					if ( push_list[x]->moverState == MOVER_POS1 )
					{//at the closest pos
						if ( pull )
						{//trying to pull, but already at closest point, so screw it
							continue;
						}
					}
					else if ( push_list[x]->moverState == MOVER_POS2 )
					{//at farthest pos
						if ( !pull )
						{//trying to push, but already at farthest point, so screw it
							continue;
						}
					}
				}
				else
				{//pos2 is closer
					if ( push_list[x]->moverState == MOVER_POS1 )
					{//at the farthest pos
						if ( !pull )
						{//trying to push, but already at farthest point, so screw it
							continue;
						}
					}
					else if ( push_list[x]->moverState == MOVER_POS2 )
					{//at closest pos
						if ( pull )
						{//trying to pull, but already at closest point, so screw it
							continue;
						}
					}
				}
				GEntity_UseFunc( push_list[x], self, self );
			}
			else if ( Q_stricmp( "func_button", push_list[x]->classname ) == 0 )
			{//pretend you pushed it
				Touch_Button( push_list[x], self, NULL );
				continue;
			}
			else if (push_list[x]->s.eType == ET_ITEM)
			{ // zyk: now it will be possible to push/pull items
				float dirLen = 0;

				// zyk: do not push/pull ctf flags
				if (push_list[x]->item && push_list[x]->item->giType == IT_TEAM && 
					(push_list[x]->item->giTag == PW_REDFLAG || push_list[x]->item->giTag == PW_BLUEFLAG))
				{
					continue;
				}

				// GalaxyRP fix: [Items] only an item someone could pick up right now. A taken map item is
				// still an entity -- hidden, or a weapon's see-through placeholder, with no contents --
				// waiting on RespawnItem, and pushing it used to overwrite that timer with the push one,
				// so a push could bring an item back early or hold it back late. The contents test also
				// skips an item that has not finished spawning (FinishSpawningItem sets them).
				if (!push_list[x]->item ||
					(push_list[x]->s.eFlags & (EF_NODRAW | EF_ITEMPLACEHOLDER)) ||
					!(push_list[x]->r.contents & CONTENTS_TRIGGER))
				{
					continue;
				}

				// zyk: it must be the current origin, or the pushDir will use the original origin
				VectorCopy(push_list[x]->r.currentOrigin, thispush_org);

				pushPowerMod = pushPower;

				//shove them
				if ( pull )
				{
					VectorSubtract( self->client->ps.origin, thispush_org, pushDir );
				}
				else
				{
					VectorSubtract( thispush_org, self->client->ps.origin, pushDir );
				}

				if (!dirLen)
				{
					dirLen = VectorLength(pushDir);
				}

				VectorNormalize(pushDir);

				pushPowerMod -= (dirLen*0.7);
				if (pushPowerMod < 16)
				{
					pushPowerMod = 16;
				}

				VectorScale(pushDir,pushPowerMod,pushDir);
				push_list[x]->s.pos.trType = TR_GRAVITY;
				push_list[x]->s.pos.trTime = level.time;
				VectorCopy( pushDir, push_list[x]->s.pos.trDelta );

				vectoangles(pushDir, push_list[x]->s.angles);
				push_list[x]->s.angles[PITCH] = 0;

				push_list[x]->physicsObject = qtrue;

				// GalaxyRP fix: [Items] a map item goes back to its spot after G_ItemPushReturnTime -- the
				// same category time a pickup gets (g_adaptRespawn, the team-FFA weapon time and the
				// thermal/trip mine/det pack rule included), or the map's own "wait" -- through
				// G_ReturnPushedItem, which moves only this item. This used to be the raw category cvar
				// (60s for powerups) and RespawnItem.
				//
				// A DROPPED item keeps its own think: the G_FreeEntity LaunchItem gave it (5 minutes). This
				// used to replace that with the respawn one, and a dropped item has no spawn spot -- its
				// s.origin was never set -- so it was sent to the map's 0 0 0 and then kept there for good,
				// an entity that was never freed.
				if (!(push_list[x]->flags & FL_DROPPED_ITEM))
				{
					push_list[x]->nextthink = level.time + G_ItemPushReturnTime(push_list[x]) * 1000;
					push_list[x]->think = G_ReturnPushedItem;
				}

				trap->LinkEntity ((sharedEntity_t *)push_list[x]);
			}
		}
	}

	//attempt to break any leftover grips
	//if we're still in a current grip that wasn't broken by the push, it will still remain
	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	if (self->client->ps.fd.forceGripBeingGripped > level.time)
	{
		self->client->ps.fd.forceGripBeingGripped = 0;
	}
}

void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower )
{
	int wasActive = self->client->ps.fd.forcePowersActive;

	self->client->ps.fd.forcePowersActive &= ~( 1 << forcePower );

	switch( (int)forcePower )
	{
	case FP_HEAL:
		self->client->ps.fd.forceHealAmount = 0;
		self->client->ps.fd.forceHealTime = 0;
		break;
	case FP_LEVITATION:
		break;
	case FP_SPEED:
		if (wasActive & (1 << FP_SPEED))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_2-50], CHAN_VOICE);
		}
		break;
	case FP_PUSH:
		break;
	case FP_PULL:
		break;
	case FP_TELEPATHY:
		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, mind control block unreachable

		if (wasActive & (1 << FP_TELEPATHY))
		{
			//G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distractstop.wav") );
		}
		self->client->ps.fd.forceMindtrickTargetIndex = 0;
		self->client->ps.fd.forceMindtrickTargetIndex2 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex3 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex4 = 0;
		break;
	case FP_SEE:
		if (wasActive & (1 << FP_SEE))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_5-50], CHAN_VOICE);

			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Bounty Hunter stop-binoculars branch unreachable

			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Sense-level-reset branch unreachable
		}
		break;
	case FP_GRIP:
		self->client->ps.fd.forceGripUseTime = level.time + 3000;
		if (self->client->ps.fd.forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1 &&
			g_entities[self->client->ps.fd.forceGripEntityNum].client &&
			g_entities[self->client->ps.fd.forceGripEntityNum].health > 0 &&
			g_entities[self->client->ps.fd.forceGripEntityNum].inuse &&
			(level.time - g_entities[self->client->ps.fd.forceGripEntityNum].client->ps.fd.forceGripStarted) > 500)
		{ //if we had our throat crushed in for more than half a second, gasp for air when we're let go
			if (wasActive & (1 << FP_GRIP))
			{
				G_EntitySound( &g_entities[self->client->ps.fd.forceGripEntityNum], CHAN_VOICE, G_SoundIndex("*gasp.wav") );
			}
		}

		if (g_entities[self->client->ps.fd.forceGripEntityNum].client &&
			g_entities[self->client->ps.fd.forceGripEntityNum].inuse)
		{

			g_entities[self->client->ps.fd.forceGripEntityNum].client->ps.forceGripChangeMovetype = PM_NORMAL;
		}

		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD)
		{
			self->client->ps.forceHandExtendTime = 0;
		}

		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

		self->client->ps.powerups[PW_DISINT_4] = 0;
		break;
	case FP_LIGHTNING:
		if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] < FORCE_LEVEL_2 )
		{//don't do it again for 3 seconds, minimum... FIXME: this should be automatic once regeneration is slower (normal)
			self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] = level.time + 3000;
		}
		else
		{
			self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] = level.time + 1500;
		}
		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD)
		{
			self->client->ps.forceHandExtendTime = 0; //reset hand position
		}

		self->client->ps.activeForcePass = 0;
		break;
	case FP_RAGE:
		if (self->client->ps.fd.forcePowerLevel[FP_RAGE] < FORCE_LEVEL_2)
		{// GalaxyRP (Alex): [Force Powers] Level 1 has a higher cooldown
			self->client->ps.fd.forceRageRecoveryTime = level.time + 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_2)
		{
			self->client->ps.fd.forceRageRecoveryTime = level.time + 15000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_3)
		{
			self->client->ps.fd.forceRageRecoveryTime = level.time + 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_4)
		{
			self->client->ps.fd.forceRageRecoveryTime = level.time + 5000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_5)
		{// GalaxyRP (Alex): [Force Powers] Level 5 has no cooldown
			self->client->ps.fd.forceRageRecoveryTime = level.time;
		}
		if (wasActive & (1 << FP_RAGE))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);
		}
		break;
	case FP_ABSORB:
		if (wasActive & (1 << FP_ABSORB))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);
		}
		break;
	case FP_PROTECT:
		if (wasActive & (1 << FP_PROTECT))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);
		}
		break;
	case FP_DRAIN:
		if ( self->client->ps.fd.forcePowerLevel[FP_DRAIN] < FORCE_LEVEL_2 )
		{//don't do it again for 3 seconds, minimum...
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 3000;
		}
		// GalaxyRP fix: [Force] level 2 had NO arm here and the chain has no else, so it was the one
		// drain level that never had a cooldown written at all. WP_ForcePowerStart() zeroes
		// forcePowerDebounce on every activation and ForceShootDrain() only gates on that value being
		// in the future, so Drain 2 could be fired every single frame -- strictly better than Drain 3
		// and Drain 4. Vanilla read "< FORCE_LEVEL_2 -> 3000" / "else -> 1500"; that else became
		// "else if (== FORCE_LEVEL_3)" when the level 4 and 5 arms were added, orphaning level 2.
		// 2000ms keeps the cooldown curve monotonic across the five levels (3000/2000/1500/1000/0).
		else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_2)
		{
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 2000;
		}
		else if(self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_3)
		{// GalaxyRP (Alex): [Force Powers] Level 3 has a lower cooldown
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 1500;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_4)
		{// GalaxyRP (Alex): [Force Powers] Level 4 has a lower cooldown
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 1000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_5)
		{// GalaxyRP (Alex): [Force Powers] Level 5 has no cooldown
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time;
		}

		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD)
		{
			self->client->ps.forceHandExtendTime = 0; //reset hand position
		}

		self->client->ps.activeForcePass = 0;
	default:
		break;
	}
}

void DoGripAction(gentity_t *self, forcePowers_t forcePower)
{
	gentity_t *gripEnt;
	int gripLevel = 0;
	trace_t tr;
	vec3_t a;
	vec3_t fwd, fwd_o, start_o, nvel;

	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	gripEnt = &g_entities[self->client->ps.fd.forceGripEntityNum];

	if (!gripEnt || !gripEnt->client || !gripEnt->inuse || gripEnt->health < 1 || !ForcePowerUsableOn(self, gripEnt, FP_GRIP))
	{
		WP_ForcePowerStop(self, forcePower);
		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

		if (gripEnt && gripEnt->client && gripEnt->inuse)
		{
			gripEnt->client->ps.forceGripChangeMovetype = PM_NORMAL;
		}
		return;
	}

	VectorSubtract(gripEnt->client->ps.origin, self->client->ps.origin, a);

	trap->Trace(&tr, self->client->ps.origin, NULL, NULL, gripEnt->client->ps.origin, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	gripLevel = WP_AbsorbConversion(gripEnt, gripEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_GRIP, self->client->ps.fd.forcePowerLevel[FP_GRIP], forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_GRIP]][FP_GRIP]);

	if (gripLevel == -1)
	{
		gripLevel = self->client->ps.fd.forcePowerLevel[FP_GRIP];
	}

	if (!gripLevel)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if (VectorLength(a) > MAX_GRIP_DISTANCE)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if ( !InFront( gripEnt->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.9f ) &&
		gripLevel < FORCE_LEVEL_3)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if (tr.fraction != 1.0f &&
		tr.entityNum != gripEnt->s.number /*&&
		gripLevel < FORCE_LEVEL_3*/)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[FP_GRIP] < level.time)
	{ //2 damage per second while choking, resulting in 10 damage total (not including The Squeeze<tm>)
		self->client->ps.fd.forcePowerDebounce[FP_GRIP] = level.time + 1000;
		G_Damage(gripEnt, self, self, NULL, NULL, 2, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);
	}

	Jetpack_Off(gripEnt); //make sure the guy being gripped has his jetpack off.

	if (gripLevel == FORCE_LEVEL_1)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 5000)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}

	if (gripLevel == FORCE_LEVEL_2)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		if (gripEnt->client->ps.forceGripMoveInterval < level.time)
		{
			gripEnt->client->ps.velocity[2] = 30;

			gripEnt->client->ps.forceGripMoveInterval = level.time + 300; //only update velocity every 300ms, so as to avoid heavy bandwidth usage
		}

		gripEnt->client->ps.otherKiller = self->s.number;
		gripEnt->client->ps.otherKillerTime = level.time + 5000;
		gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;

		gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 3000 && !self->client->ps.fd.forceGripDamageDebounceTime)
		{ //if we managed to lift him into the air for 2 seconds, give him a crack
			self->client->ps.fd.forceGripDamageDebounceTime = 1;
			G_Damage(gripEnt, self, self, NULL, NULL, 20, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);

			//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
			G_EntitySound( gripEnt, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )) );

			gripEnt->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
			gripEnt->client->ps.forceHandExtendTime = level.time + 2000;

			if (gripEnt->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
			{ //choking, so don't let him keep gripping himself
				WP_ForcePowerStop(gripEnt, FP_GRIP);
			}
		}
		else if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 4000)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}

	if (gripLevel == FORCE_LEVEL_3)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		if (gripEnt->client->ps.forceGripMoveInterval < level.time)
		{
			gripEnt->client->ps.velocity[2] = 30;

			gripEnt->client->ps.forceGripMoveInterval = level.time + 300; //only update velocity every 300ms, so as to avoid heavy bandwidth usage
		}

		gripEnt->client->ps.otherKiller = self->s.number;
		gripEnt->client->ps.otherKillerTime = level.time + 5000;
		gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;

		gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 3000 && !self->client->ps.fd.forceGripDamageDebounceTime)
		{ //if we managed to lift him into the air for 2 seconds, give him a crack
			self->client->ps.fd.forceGripDamageDebounceTime = 1;
			G_Damage(gripEnt, self, self, NULL, NULL, 30, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);

			//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
			G_EntitySound(gripEnt, CHAN_VOICE, G_SoundIndex(va("*choke%d.wav", Q_irand(1, 3))));

			gripEnt->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
			gripEnt->client->ps.forceHandExtendTime = level.time + 2000;

			if (gripEnt->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
			{ //choking, so don't let him keep gripping himself
				WP_ForcePowerStop(gripEnt, FP_GRIP);
			}
		}
		else if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 4000)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}

	if (gripLevel > FORCE_LEVEL_3)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		gripEnt->client->ps.otherKiller = self->s.number;
		gripEnt->client->ps.otherKillerTime = level.time + 5000;
		gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;

		gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;

		if (gripEnt->client->ps.forceGripMoveInterval < level.time)
		{
			float nvLen = 0;

			VectorCopy(gripEnt->client->ps.origin, start_o);
			AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
			fwd_o[0] = self->client->ps.origin[0] + fwd[0]*128;
			fwd_o[1] = self->client->ps.origin[1] + fwd[1]*128;
			fwd_o[2] = self->client->ps.origin[2] + fwd[2]*128;
			fwd_o[2] += 16;
			VectorSubtract(fwd_o, start_o, nvel);

			nvLen = VectorLength(nvel);

			if (nvLen < 16)
			{ //within x units of desired spot
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*8;
				gripEnt->client->ps.velocity[1] = nvel[1]*8;
				gripEnt->client->ps.velocity[2] = nvel[2]*8;
			}
			else if (nvLen < 64)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*128;
				gripEnt->client->ps.velocity[1] = nvel[1]*128;
				gripEnt->client->ps.velocity[2] = nvel[2]*128;
			}
			else if (nvLen < 128)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*256;
				gripEnt->client->ps.velocity[1] = nvel[1]*256;
				gripEnt->client->ps.velocity[2] = nvel[2]*256;
			}
			else if (nvLen < 200)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*512;
				gripEnt->client->ps.velocity[1] = nvel[1]*512;
				gripEnt->client->ps.velocity[2] = nvel[2]*512;
			}
			else
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*700;
				gripEnt->client->ps.velocity[1] = nvel[1]*700;
				gripEnt->client->ps.velocity[2] = nvel[2]*700;
			}

			gripEnt->client->ps.forceGripMoveInterval = level.time + 300; //only update velocity every 300ms, so as to avoid heavy bandwidth usage
		}

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 3000 && !self->client->ps.fd.forceGripDamageDebounceTime)
		{ //if we managed to lift him into the air for 2 seconds, give him a crack
			self->client->ps.fd.forceGripDamageDebounceTime = 1;
			if (gripLevel == FORCE_LEVEL_4) 
			{
				G_Damage(gripEnt, self, self, NULL, NULL, 40, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);
			}
			else
			{
				G_Damage(gripEnt, self, self, NULL, NULL, 60, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);
			}

			//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
			G_EntitySound( gripEnt, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )) );

			gripEnt->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
			gripEnt->client->ps.forceHandExtendTime = level.time + 2000;

			if (gripEnt->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
			{ //choking, so don't let him keep gripping himself
				WP_ForcePowerStop(gripEnt, FP_GRIP);
			}
		}
		else if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 4000)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}
}

qboolean G_IsMindTricked(forcedata_t *fd, int client)
{
	int checkIn;
	int trickIndex1, trickIndex2, trickIndex3, trickIndex4;
	int sub = 0;

	if (!fd)
	{
		return qfalse;
	}

	trickIndex1 = fd->forceMindtrickTargetIndex;
	trickIndex2 = fd->forceMindtrickTargetIndex2;
	trickIndex3 = fd->forceMindtrickTargetIndex3;
	trickIndex4 = fd->forceMindtrickTargetIndex4;

	if (client > 47)
	{
		checkIn = trickIndex4;
		sub = 48;
	}
	else if (client > 31)
	{
		checkIn = trickIndex3;
		sub = 32;
	}
	else if (client > 15)
	{
		checkIn = trickIndex2;
		sub = 16;
	}
	else
	{
		checkIn = trickIndex1;
	}

	if (checkIn & (1 << (client-sub)))
	{
		return qtrue;
	}

	return qfalse;
}

static void RemoveTrickedEnt(forcedata_t *fd, int client)
{
	if (!fd)
	{
		return;
	}

	if (client > 47)
	{
		fd->forceMindtrickTargetIndex4 &= ~(1 << (client-48));
	}
	else if (client > 31)
	{
		fd->forceMindtrickTargetIndex3 &= ~(1 << (client-32));
	}
	else if (client > 15)
	{
		fd->forceMindtrickTargetIndex2 &= ~(1 << (client-16));
	}
	else
	{
		fd->forceMindtrickTargetIndex &= ~(1 << client);
	}
}

extern int g_LastFrameTime;
extern int g_TimeSinceLastFrame;

static void WP_UpdateMindtrickEnts(gentity_t *self)
{
	int i = 0;

	// zyk: changed condition from MAX_CLIENTS to level.num_entities
	while (i < level.num_entities)
	{
		gentity_t *ent = &g_entities[i];
		if (G_IsMindTricked(&self->client->ps.fd, i) || (ent && ent->NPC))
		{ // zyk: added the NPC condition
			if ( !ent || !ent->client || !ent->inuse || ent->health < 1 ||
				(ent->client->ps.fd.forcePowersActive & (1 << FP_SEE) && 
				(ent->client->sess.amrpgmode < 2 || ent->client->pers.thermal_vision == qfalse)) )
			{ // zyk: do not cancel Mind Trick if enemy is using Thermal Vision
				if (ent && !ent->NPC) // zyk: remove tricked entity only for players
					RemoveTrickedEnt(&self->client->ps.fd, i);
			}
			else if ((level.time - self->client->dangerTime) < g_TimeSinceLastFrame*4)
			{ //Untrick this entity if the tricker (self) fires while in his fov
				if (trap->InPVS(ent->client->ps.origin, self->client->ps.origin) &&
					OrgVisible(ent->client->ps.origin, self->client->ps.origin, ent->s.number))
				{
					// GalaxyRP fix: [Dead Code] rpg_class permanently 0, condition always true (amrpgmode<2 || (rpg_class!=1 && rpg_class!=5))
					if (ent && !ent->NPC) // zyk: remove tricked entity only for players
						RemoveTrickedEnt(&self->client->ps.fd, i);
				}
			}
			else if (BG_HasYsalamiri(level.gametype, &ent->client->ps))
			{
				RemoveTrickedEnt(&self->client->ps.fd, i);
			}
		}

		i++;
	}

	if (!self->client->ps.fd.forceMindtrickTargetIndex &&
		!self->client->ps.fd.forceMindtrickTargetIndex2 &&
		!self->client->ps.fd.forceMindtrickTargetIndex3 &&
		!self->client->ps.fd.forceMindtrickTargetIndex4 && self->client->pers.mind_controlled1_id == -1)
	{ //everyone who we had tricked is no longer tricked, so stop the power. zyk: added the mind control conditions
		WP_ForcePowerStop(self, FP_TELEPATHY);
	}
	else if (self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG])
	{
		WP_ForcePowerStop(self, FP_TELEPATHY);
	}
}

#define FORCE_DEBOUNCE_TIME 50 // sv_fps 20 = 50msec frametime, basejka balance/timing

static void WP_ForcePowerRun( gentity_t *self, forcePowers_t forcePower, usercmd_t *cmd )
{
//	extern usercmd_t	ucmd;

	switch( (int)forcePower )
	{
	case FP_HEAL:
		if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_1)
		{
			if (self->client->ps.velocity[0] || self->client->ps.velocity[1] || self->client->ps.velocity[2])
			{
				WP_ForcePowerStop( self, forcePower );
				break;
			}
		}

		if (self->health < 1 || self->client->ps.stats[STAT_HEALTH] < 1)
		{
			WP_ForcePowerStop( self, forcePower );
			break;
		}

		if (self->client->ps.fd.forceHealTime > level.time)
		{
			break;
		}
		if ( self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{ //rww - we might start out over max_health and we don't want force heal taking us down to 100 or whatever max_health is
			WP_ForcePowerStop( self, forcePower );
			break;
		}
		self->client->ps.fd.forceHealTime = level.time + 1000;
		self->health++;
		self->client->ps.fd.forceHealAmount++;

		if ( self->health > self->client->ps.stats[STAT_MAX_HEALTH])	// Past max health
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
			WP_ForcePowerStop( self, forcePower );
		}

		if ( (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_1 && self->client->ps.fd.forceHealAmount >= 25) ||
			(self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_2 && self->client->ps.fd.forceHealAmount >= 33)) 
		{
			WP_ForcePowerStop( self, forcePower );
		}
		break;
	case FP_SPEED:
		//This is handled in PM_WalkMove and PM_StepSlideMove
		if ( self->client->holdingObjectiveItem >= MAX_CLIENTS
			&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD )
		{
			if ( g_entities[self->client->holdingObjectiveItem].genericValue15 )
			{//disables force powers
				WP_ForcePowerStop( self, forcePower );
			}
		}
		/*
		if ( self->client->ps.powerups[PW_REDFLAG]
			|| self->client->ps.powerups[PW_BLUEFLAG]
			|| self->client->ps.powerups[PW_NEUTRALFLAG] )
		{//no force speed when carrying flag
			WP_ForcePowerStop( self, forcePower );
		}
		*/
		break;
	case FP_GRIP:
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD)
		{
			WP_ForcePowerStop(self, FP_GRIP);
			break;
		}

		if (self->client->ps.fd.forcePowerDebounce[FP_PULL] < level.time)
		{ //This is sort of not ideal. Using the debounce value reserved for pull for this because pull doesn't need it.
			BG_ForcePowerDrain( &self->client->ps, forcePower, 1 );
			self->client->ps.fd.forcePowerDebounce[FP_PULL] = level.time + 100;
		}

		if (self->client->ps.fd.forcePower < 1)
		{
			WP_ForcePowerStop(self, FP_GRIP);
			break;
		}

		// GalaxyRP fix: [Cloak Item] a held grip keeps breaking cloak, so re-cloaking while still
		// choking someone does not stick -- see RP_AttackBreaksCloak.
		RP_AttackBreaksCloak( self );
		DoGripAction(self, forcePower);
		break;
	case FP_LEVITATION:
		if ( self->client->ps.groundEntityNum != ENTITYNUM_NONE && !self->client->ps.fd.forceJumpZStart )
		{//done with jump
			WP_ForcePowerStop( self, forcePower );
		}
		break;
	case FP_RAGE:
		if (self->health < 1)
		{
			WP_ForcePowerStop(self, forcePower);
			break;
		}
		if (self->client->ps.forceRageDrainTime < level.time)
		{
			int addTime = 0;

			self->health -= 2;

			// GalaxyRP (Alex): [Force Powers] Levels 1 through 3 take extra damage for more time
			if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_1)
			{
				self->health -= 3;
				addTime = 150;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_2)
			{
				self->health -= 2;
				addTime = 200;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_3)
			{
				self->health -= 1;
				addTime = 250;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_4)
			{
				addTime = 300;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_5)
			{
				addTime = 350;
			}
			self->client->ps.forceRageDrainTime = level.time + addTime;
		}

		if (self->health < 1)
		{
			self->health = 1;
			WP_ForcePowerStop(self, forcePower);
		}

		self->client->ps.stats[STAT_HEALTH] = self->health;
		break;
	case FP_DRAIN:
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD)
		{
			WP_ForcePowerStop(self, forcePower);
			break;
		}

		if ( self->client->ps.fd.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_1 )
		{//higher than level 1
			if ( (cmd->buttons & BUTTON_FORCE_DRAIN) || ((cmd->buttons & BUTTON_FORCEPOWER) && self->client->ps.fd.forcePowerSelected == FP_DRAIN) )
			{//holding it keeps it going
				self->client->ps.fd.forcePowerDuration[FP_DRAIN] = level.time + 500;
			}
		}
		// OVERRIDEFIXME
		if ( !WP_ForcePowerAvailable( self, forcePower, 0 ) || self->client->ps.fd.forcePowerDuration[FP_DRAIN] < level.time ||
			self->client->ps.fd.forcePower < 25)
		{
			WP_ForcePowerStop( self, forcePower );
		}
		else
		{
			while ( self->client->force.drainDebounce < level.time )
			{
				// GalaxyRP fix: [Cloak Item] every drain tick breaks cloak, not just the first -- see
				// RP_AttackBreaksCloak.
				RP_AttackBreaksCloak( self );
				ForceShootDrain( self );
				self->client->force.drainDebounce += FORCE_DEBOUNCE_TIME;
			}
		}
		break;
	case FP_LIGHTNING:
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD)
		{ //Animation for hand extend doesn't end with hand out, so we have to limit lightning intervals by animation intervals (once hand starts to go in in animation, lightning should stop)
			WP_ForcePowerStop(self, forcePower);
			break;
		}

		if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_1 )
		{//higher than level 1
			if ( (cmd->buttons & BUTTON_FORCE_LIGHTNING) || ((cmd->buttons & BUTTON_FORCEPOWER) && self->client->ps.fd.forcePowerSelected == FP_LIGHTNING) )
			{//holding it keeps it going
				self->client->ps.fd.forcePowerDuration[FP_LIGHTNING] = level.time + 500;
			}
		}
		// OVERRIDEFIXME
		if ( !WP_ForcePowerAvailable( self, forcePower, 0 ) || self->client->ps.fd.forcePowerDuration[FP_LIGHTNING] < level.time ||
			self->client->ps.fd.forcePower < 25)
		{
			WP_ForcePowerStop( self, forcePower );
		}
		else
		{
			while ( self->client->force.lightningDebounce < level.time )
			{
				// GalaxyRP fix: [Cloak Item] every lightning tick breaks cloak, not just the first -- see
				// RP_AttackBreaksCloak.
				RP_AttackBreaksCloak( self );
				ForceShootLightning( self );
				BG_ForcePowerDrain( &self->client->ps, forcePower, 0 );
				self->client->force.lightningDebounce += FORCE_DEBOUNCE_TIME;
			}
		}
		break;
	case FP_TELEPATHY:
		if ( self->client->holdingObjectiveItem >= MAX_CLIENTS
			&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD
			&& g_entities[self->client->holdingObjectiveItem].genericValue15 )
		{ //if force hindered can't mindtrick whilst carrying a siege item
			WP_ForcePowerStop( self, FP_TELEPATHY );
		}
		else
		{
			WP_UpdateMindtrickEnts(self);
		}
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	case FP_PROTECT:
		if (self->client->sess.amrpgmode < 2 || self->client->pers.skill_levels[10] < 4)
		{ // zyk: Protect 4/4 does not have force debounce
			if (self->client->ps.fd.forcePowerDebounce[forcePower] < level.time)
			{
				BG_ForcePowerDrain( &self->client->ps, forcePower, 1 );
				if (self->client->ps.fd.forcePower < 1)
				{
					WP_ForcePowerStop(self, forcePower);
				}

				self->client->ps.fd.forcePowerDebounce[forcePower] = level.time + 300;
			}
		}
		break;
	case FP_ABSORB:
		if (self->client->sess.amrpgmode < 2 || self->client->pers.skill_levels[8] < FORCE_LEVEL_4)
		{ // zyk: Absorb 4/4 does not have force debounce
			if (self->client->ps.fd.forcePowerDebounce[forcePower] < level.time)
			{
				BG_ForcePowerDrain( &self->client->ps, forcePower, 1 );
				if (self->client->ps.fd.forcePower < 1)
				{
					WP_ForcePowerStop(self, forcePower);
				}

				self->client->ps.fd.forcePowerDebounce[forcePower] = level.time + 600;
			}
		}
		break;
	default:
		break;
	}
}

int WP_DoSpecificPower( gentity_t *self, usercmd_t *ucmd, forcePowers_t forcepower)
{
	int powerSucceeded;

	powerSucceeded = 1;

	// OVERRIDEFIXME
	if ( !WP_ForcePowerAvailable( self, forcepower, 0 ) )
	{
		return 0;
	}

	switch(forcepower)
	{
	case FP_HEAL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceHeal(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_LEVITATION:
		//if leave the ground by some other means, cancel the force jump so we don't suddenly jump when we land.

		if ( self->client->ps.groundEntityNum == ENTITYNUM_NONE )
		{
			self->client->ps.fd.forceJumpCharge = 0;
			G_MuteSound( self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE );
			//This only happens if the groundEntityNum == ENTITYNUM_NONE when the button is actually released
		}
		else
		{//still on ground, so jump
			ForceJump( self, ucmd );
		}
		break;
	case FP_SPEED:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceSpeed(self, 0);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_GRIP:
		if (self->client->ps.fd.forceGripEntityNum == ENTITYNUM_NONE)
		{
			ForceGrip( self );
		}

		if (self->client->ps.fd.forceGripEntityNum != ENTITYNUM_NONE)
		{
			if (!(self->client->ps.fd.forcePowersActive & (1 << FP_GRIP)))
			{
				WP_ForcePowerStart( self, FP_GRIP, 0 );
				BG_ForcePowerDrain( &self->client->ps, FP_GRIP, GRIP_DRAIN_AMOUNT );
			}
		}
		else
		{
			powerSucceeded = 0;
		}
		break;
	case FP_LIGHTNING:
		ForceLightning(self);
		break;
	case FP_PUSH:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease && !(self->r.svFlags & SVF_BOT))
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceThrow(self, qfalse);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_PULL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceThrow(self, qtrue);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TELEPATHY:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTelepathy(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_RAGE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceRage(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_PROTECT:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceProtect(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_ABSORB:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceAbsorb(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TEAM_HEAL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTeamHeal(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TEAM_FORCE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTeamForceReplenish(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_DRAIN:
		ForceDrain(self);
		break;
	case FP_SEE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceSeeing(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	default:
		break;
	}

	return powerSucceeded;
}

void FindGenericEnemyIndex(gentity_t *self)
{ //Find another client that would be considered a threat.
	int i = 0;
	float tlen;
	gentity_t *ent;
	gentity_t *besten = NULL;
	float blen = 99999999.9f;
	vec3_t a;

	while (i < level.num_entities)
	{ // zyk: changed from MAX_CLIENTS to level.num_entities so seeker drone can attack npcs
		ent = &g_entities[i];

		if (ent && ent->client && ent->s.number != self->s.number && ent->health > 0 && !OnSameTeam(self, ent) && ent->client->ps.pm_type != PM_INTERMISSION && ent->client->ps.pm_type != PM_SPECTATOR)
		{
			if (zyk_is_ally(self,ent) == qfalse && 
				(ent->NPC || (ent->client->pers.connected == CON_CONNECTED && ent->client->sess.sessionTeam != TEAM_SPECTATOR && 
				 ent->client->ps.duelInProgress == qfalse && zyk_can_hit_target(self, ent) == qtrue)))
			{ // zyk: allies and not connected players cant be attacked by seeker drone. Also, players in boss battle cannot attack players not in it and vice-versa
				VectorSubtract(ent->client->ps.origin, self->client->ps.origin, a);
				tlen = VectorLength(a);

				if (tlen < blen &&
					InFront(ent->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.8f ) &&
					OrgVisible(self->client->ps.origin, ent->client->ps.origin, self->s.number))
				{
					blen = tlen;
					besten = ent;
				}
			}
		}

		i++;
	}

	if (!besten)
	{
		return;
	}

	self->client->ps.genericEnemyIndex = besten->s.number;
}

void SeekerDroneUpdate(gentity_t *self)
{
	vec3_t org, elevated, dir, a, endir;
	gentity_t *en;
	float angle;
	float prefig = 0;
	trace_t tr;

	if (!(self->client->ps.eFlags & EF_SEEKERDRONE))
	{
		self->client->ps.genericEnemyIndex = -1;
		return;
	}

	if (self->health < 1)
	{
		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		a[ROLL] = 0;
		a[YAW] = 0;
		a[PITCH] = 1;

		G_PlayEffect(EFFECT_SPARK_EXPLOSION, org, a);

		self->client->ps.eFlags &= ~EF_SEEKERDRONE;
		self->client->ps.genericEnemyIndex = -1;

		return;
	}

	if (self->client->ps.droneExistTime >= level.time &&
		self->client->ps.droneExistTime < (level.time+5000))
	{
		self->client->ps.genericEnemyIndex = 1024+self->client->ps.droneExistTime;
		if (self->client->ps.droneFireTime < level.time)
		{
			G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/laser_trap/warning.wav") );
			self->client->ps.droneFireTime = level.time + 100;
		}
		return;
	}
	else if (self->client->ps.droneExistTime < level.time)
	{
		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		prefig = (self->client->ps.droneExistTime-level.time)/80;

		if (prefig > 55)
		{
			prefig = 55;
		}
		else if (prefig < 1)
		{
			prefig = 1;
		}

		elevated[2] -= 55-prefig;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		a[ROLL] = 0;
		a[YAW] = 0;
		a[PITCH] = 1;

		G_PlayEffect(EFFECT_SPARK_EXPLOSION, org, a);

		self->client->ps.eFlags &= ~EF_SEEKERDRONE;
		self->client->ps.genericEnemyIndex = -1;

		return;
	}

	if (self->client->ps.genericEnemyIndex == -1)
	{
		self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
	}

	if (self->client->ps.genericEnemyIndex != ENTITYNUM_NONE && self->client->ps.genericEnemyIndex != -1)
	{
		en = &g_entities[self->client->ps.genericEnemyIndex];

		if (!en || !en->client)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (en->s.number == self->s.number)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (en->health < 1)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (OnSameTeam(self, en))
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else
		{
			if (!InFront(en->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.8f ))
			{
				self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
			}
			else if (!OrgVisible(self->client->ps.origin, en->client->ps.origin, self->s.number))
			{
				self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
			}
		}
	}

	if (self->client->ps.genericEnemyIndex == ENTITYNUM_NONE || self->client->ps.genericEnemyIndex == -1)
	{
		FindGenericEnemyIndex(self);
	}

	if (self->client->ps.genericEnemyIndex != ENTITYNUM_NONE && self->client->ps.genericEnemyIndex != -1)
	{
		en = &g_entities[self->client->ps.genericEnemyIndex];

		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		//org is now where the thing should be client-side because it uses the same time-based offset
		if (self->client->ps.droneFireTime < level.time)
		{
			trap->Trace(&tr, org, NULL, NULL, en->client->ps.origin, -1, MASK_SOLID, qfalse, 0, 0);

			if (tr.fraction == 1 && !tr.startsolid && !tr.allsolid)
			{
				VectorSubtract(en->client->ps.origin, org, endir);
				VectorNormalize(endir);

				// zyk: changed shot speed from 2000 to 4000
				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Bounty Hunter damage-bonus branch unreachable
				WP_FireGenericBlasterMissile(self, org, endir, qfalse, 15, 4000, MOD_BLASTER);

				G_SoundAtLoc( org, CHAN_WEAPON, G_SoundIndex("sound/weapons/bryar/fire.wav") );

				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Bounty Hunter fast-shoot branch unreachable
				self->client->ps.droneFireTime = level.time + Q_irand(400, 700);
			}
		}
	}
}

void HolocronUpdate(gentity_t *self)
{ //keep holocron status updated in holocron mode
	int i = 0;
	int noHRank = 0;

	if (noHRank < FORCE_LEVEL_0)
	{
		noHRank = FORCE_LEVEL_0;
	}
	if (noHRank > FORCE_LEVEL_3)
	{
		noHRank = FORCE_LEVEL_3;
	}

	trap->Cvar_Update(&g_maxHolocronCarry);

	while (i < NUM_FORCE_POWERS)
	{
		if (self->client->ps.holocronsCarried[i])
		{ //carrying it, make sure we have the power
			self->client->ps.holocronBits |= (1 << i);
			self->client->ps.fd.forcePowersKnown |= (1 << i);
			self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
		}
		else
		{ //otherwise, make sure the power is cleared from us
			self->client->ps.fd.forcePowerLevel[i] = 0;
			if (self->client->ps.holocronBits & (1 << i))
			{
				self->client->ps.holocronBits -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersKnown & (1 << i)) && i != FP_LEVITATION && i != FP_SABER_OFFENSE)
			{
				self->client->ps.fd.forcePowersKnown -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION && i != FP_SABER_OFFENSE)
			{
				WP_ForcePowerStop(self, i);
			}

			if (i == FP_LEVITATION)
			{
				if (noHRank >= FORCE_LEVEL_1)
				{
					self->client->ps.fd.forcePowerLevel[i] = noHRank;
				}
				else
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
				}
			}
			else if (i == FP_SABER_OFFENSE)
			{
				self->client->ps.fd.forcePowersKnown |= (1 << i);

				if (noHRank >= FORCE_LEVEL_1)
				{
					self->client->ps.fd.forcePowerLevel[i] = noHRank;
				}
				else
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
				}

				//make sure that the player's saber stance is reset so they can't continue to use that stance when they don't have the skill for it anymore.
				if ( self->client->saber[0].model[0] && self->client->saber[1].model[0] )
				{ //dual
					self->client->ps.fd.saberAnimLevelBase = self->client->ps.fd.saberAnimLevel = self->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
				}
				else if ( (self->client->saber[0].saberFlags & SFL_TWO_HANDED) )
				{ //staff
					self->client->ps.fd.saberAnimLevel = self->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
				}
				else
				{
					self->client->ps.fd.saberAnimLevelBase = self->client->ps.fd.saberAnimLevel = self->client->ps.fd.saberDrawAnimLevel = SS_MEDIUM;
				}
			}
			else
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			}
		}

		i++;
	}

	if (HasSetSaberOnly())
	{ //if saberonly, we get these powers no matter what (still need the holocrons for level 3)
		if (self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = FORCE_LEVEL_1;
		}
		if (self->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = FORCE_LEVEL_1;
		}
	}
}

void JediMasterUpdate(gentity_t *self)
{ //keep jedi master status updated for JM gametype
	int i = 0;

	trap->Cvar_Update(&g_maxHolocronCarry);

	while (i < NUM_FORCE_POWERS)
	{
		if (self->client->ps.isJediMaster)
		{
			self->client->ps.fd.forcePowersKnown |= (1 << i);
			self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;

			if (i == FP_TEAM_HEAL || i == FP_TEAM_FORCE ||
				i == FP_DRAIN || i == FP_ABSORB)
			{ //team powers are useless in JM, absorb is too because no one else has powers to absorb. Drain is just
			  //relatively useless in comparison, because its main intent is not to heal, but rather to cripple others
			  //by draining their force at the same time. And no one needs force in JM except the JM himself.
				self->client->ps.fd.forcePowersKnown &= ~(1 << i);
				self->client->ps.fd.forcePowerLevel[i] = 0;
			}

			if (i == FP_TELEPATHY)
			{ //this decision was made because level 3 mindtrick allows the JM to just hide too much, and no one else has force
			  //sight to counteract it. Since the JM himself is the focus of gameplay in this mode, having him hidden for large
			  //durations is indeed a bad thing.
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_2;
			}
		}
		else
		{
			if ((self->client->ps.fd.forcePowersKnown & (1 << i)) && i != FP_LEVITATION)
			{
				self->client->ps.fd.forcePowersKnown -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION)
			{
				WP_ForcePowerStop(self, i);
			}

			if (i == FP_LEVITATION)
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
			}
			else
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			}
		}

		i++;
	}
}

qboolean WP_HasForcePowers( const playerState_t *ps )
{
	int i;
	if ( ps )
	{
		for ( i = 0; i < NUM_FORCE_POWERS; i++ )
		{
			if ( i == FP_LEVITATION )
			{
				if ( ps->fd.forcePowerLevel[i] > FORCE_LEVEL_1 )
				{
					return qtrue;
				}
			}
			else if ( ps->fd.forcePowerLevel[i] > FORCE_LEVEL_0 )
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

//try a special roll getup move
qboolean G_SpecialRollGetup(gentity_t *self)
{ //fixme: currently no knockdown will actually land you on your front... so froll's are pretty useless at the moment.
	qboolean rolled = qfalse;

	/*
	if (self->client->ps.weapon != WP_SABER &&
		self->client->ps.weapon != WP_MELEE)
	{ //can't do acrobatics without saber selected
		return qfalse;
	}
	*/

	if (/*!self->client->pers.cmd.upmove &&*/
		self->client->pers.cmd.rightmove > 0 &&
		!self->client->pers.cmd.forwardmove)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_R, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (/*!self->client->pers.cmd.upmove &&*/
		self->client->pers.cmd.rightmove < 0 &&
		!self->client->pers.cmd.forwardmove)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_L, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (/*self->client->pers.cmd.upmove > 0 &&*/
		!self->client->pers.cmd.rightmove &&
		self->client->pers.cmd.forwardmove > 0)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_F, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (/*self->client->pers.cmd.upmove > 0 &&*/
		!self->client->pers.cmd.rightmove &&
		self->client->pers.cmd.forwardmove < 0)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_B, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (self->client->pers.cmd.upmove)
	{
		G_PreDefSound(self->client->ps.origin, PDSOUND_FORCEJUMP);
		self->client->ps.forceDodgeAnim = 2;
		self->client->ps.forceHandExtendTime = level.time + 500;

		//self->client->ps.velocity[2] = 300;
	}

	if (rolled)
	{
		G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
	}

	return rolled;
}

// GalaxyRP fix: [Skills] Sense Health, reworked.
//
// Targeting: the skill used to read ps.lookTarget, which is not "who you are looking at" but a side
// product of the saber auto-block routine (WP_SaberStartMissileBlockCheck): the nearest visible
// player/NPC within 256 units, and only on frames that routine got far enough to pick one -- never
// while your weapon was busy, while knocked down, or at all with a saber that cannot actively block.
// It also went unchecked from one frame to the next. The skill now picks its own target: whoever is
// under the crosshair within ZYK_SENSE_HEALTH_AIM_RANGE, else the nearest visible one within
// ZYK_SENSE_HEALTH_NEAR_RANGE (what it effectively did before). ps.lookTarget itself is untouched --
// head turning and /npc still use it.
//
// Display: it used to be a "cp" -- the big centre-print font, 30% down the screen, held for
// cg_centerTime (3s) and overwriting every other centre print twice a second while Sight was on.
// It is now a "sensehp" server command that the mod's cgame draws small, under the crosshair
// (CG_DrawSenseHealth in cg_draw.c). Only the values the sensing player's skill level shows are
// sent; the rest go out as -1, so a modified client cannot read more than its level allows.
#define ZYK_SENSE_HEALTH_AIM_RANGE		1024
#define ZYK_SENSE_HEALTH_NEAR_RANGE		256
#define ZYK_SENSE_HEALTH_NAME_LENGTH	64

// GalaxyRP: [Skills] a vehicle Sense Health can read: a live vehicle NPC with its vehicle data
static qboolean zyk_sense_health_is_vehicle(const gentity_t *ent)
{
	return (ent->s.eType == ET_NPC && ent->s.NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;
}

// zyk: who Sense Health may report on, checked every time. GalaxyRP: [Skills] vehicles included now
// (they were excluded, as the old saber look target excluded them) -- but never the vehicle the
// sensing player is riding, and only with their vehicle data in place.
static qboolean zyk_sense_health_valid_target(const gentity_t *self, const gentity_t *target)
{
	if (!target || target == self || !target->inuse || !target->client)
		return qfalse;

	if (target->s.eType != ET_PLAYER && target->s.eType != ET_NPC)
		return qfalse;

	if (zyk_sense_health_is_vehicle(target))
	{
		if (!target->m_pVehicle || !target->m_pVehicle->m_pVehicleInfo)
			return qfalse;

		if (self->client->ps.m_iVehicleNum == target->s.number)
			return qfalse; // zyk: your own vehicle
	}

	if (target->client->sess.sessionTeam == TEAM_SPECTATOR || (target->client->ps.pm_flags & PMF_FOLLOW))
		return qfalse;

	if (target->health <= 0)
		return qfalse;

	return qtrue;
}

// GalaxyRP: [Skills] someone aboard a vehicle -- its pilot, a passenger, or the droid it carries. The
// nearby search leaves them out: they sit inside the ship (a fighter's are hidden), so standing next to
// an X-wing would otherwise show its droid or pilot rather than the ship.
static qboolean zyk_sense_health_is_aboard(const gentity_t *ent)
{
	return (!zyk_sense_health_is_vehicle(ent) && (ent->client->ps.m_iVehicleNum || ent->s.m_iVehicleNum)) ? qtrue : qfalse;
}

// zyk: a trace from the sensing player's eyes that looks past their own vehicle. The first trace skips
// only the player; if it stops on the vehicle they are riding (always, from inside a fighter), it is
// run again skipping the vehicle instead -- the rider of a fighter is not solid, so nothing is lost.
static void zyk_sense_health_trace(gentity_t *self, trace_t *tr, vec3_t start, vec3_t end, int mask)
{
	const int own_vehicle = self->client->ps.m_iVehicleNum;

	trap->Trace(tr, start, NULL, NULL, end, self->s.number, mask, qfalse, 0, 0);

	if (own_vehicle > 0 && own_vehicle < ENTITYNUM_WORLD && tr->entityNum == own_vehicle)
	{
		trap->Trace(tr, start, NULL, NULL, end, own_vehicle, mask, qfalse, 0, 0);
	}
}

// zyk: whoever is under the crosshair; else, within reach and in sight, the nearest player, or if
// there is none the nearest NPC, or if there is none the nearest vehicle. NULL if there is nobody.
static gentity_t *zyk_sense_health_find_target(gentity_t *self)
{
	int entity_list[MAX_GENTITIES];
	int num_listed = 0;
	int i = 0;
	vec3_t eyes, forward, end, mins, maxs, diff;
	trace_t tr;
	gentity_t *best[3] = { NULL, NULL, NULL }; // zyk: nearest player, NPC, vehicle -- in that order of preference
	float best_dist[3] = { 0.0f, 0.0f, 0.0f };

	VectorCopy(self->client->ps.origin, eyes);
	eyes[2] += self->client->ps.viewheight;

	AngleVectors(self->client->ps.viewangles, forward, NULL, NULL);
	VectorMA(eyes, ZYK_SENSE_HEALTH_AIM_RANGE, forward, end);

	zyk_sense_health_trace(self, &tr, eyes, end, MASK_SHOT);

	if (tr.entityNum >= 0 && tr.entityNum < ENTITYNUM_WORLD && zyk_sense_health_valid_target(self, &g_entities[tr.entityNum]))
	{
		return &g_entities[tr.entityNum];
	}

	for (i = 0; i < 3; i++)
	{
		mins[i] = self->r.currentOrigin[i] - ZYK_SENSE_HEALTH_NEAR_RANGE;
		maxs[i] = self->r.currentOrigin[i] + ZYK_SENSE_HEALTH_NEAR_RANGE;
	}

	num_listed = trap->EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

	for (i = 0; i < num_listed; i++)
	{
		gentity_t *ent = &g_entities[entity_list[i]];
		float dist = 0.0f;
		int kind = 0;

		if (!zyk_sense_health_valid_target(self, ent) || zyk_sense_health_is_aboard(ent))
			continue;

		kind = zyk_sense_health_is_vehicle(ent) ? 2 : (ent->s.eType == ET_NPC ? 1 : 0);

		VectorSubtract(self->client->ps.origin, ent->client->ps.origin, diff);
		dist = VectorLength(diff);

		if (best[kind] && dist >= best_dist[kind])
			continue;

		// zyk: in sight, the test the old look target used
		zyk_sense_health_trace(self, &tr, eyes, ent->client->ps.origin, MASK_PLAYERSOLID);

		if (tr.fraction == 1.0f || tr.entityNum == ent->s.number)
		{
			best[kind] = ent;
			best_dist[kind] = dist;
		}
	}

	return best[0] ? best[0] : (best[1] ? best[1] : best[2]);
}

// zyk: builds the "sensehp" command for this target at this skill level. Arguments, in order:
// level, health, max health, shield, max shield, force, max force, type (0 not logged in,
// 1 logged-in player, 2 NPC, 3 empty vehicle, 4 occupied vehicle), "name" -- -1 (or an empty name)
// for anything the level does not show. For a vehicle, health is its hull and shield its shields
// (the maxima from its .veh data), and force is never shown. The name goes last, bounded, with any
// double quote turned into a single one: a player name may contain '"', which would end the quoted
// argument early on the client.
static void zyk_sense_health_command(const gentity_t *target, int level, char *out, int out_size)
{
	char name[ZYK_SENSE_HEALTH_NAME_LENGTH];
	int health = target->health > 0 ? target->health : 0;
	int max_health = -1, shield = -1, max_shield = -1, force = -1, max_force = -1, type = -1;
	int i = 0;

	name[0] = '\0';

	if (level > 3)
		level = 3;

	if (level >= 2)
	{
		Q_strncpyz(name, (target->NPC && target->NPC_type) ? target->NPC_type : target->client->pers.netname, sizeof(name));

		for (i = 0; name[i]; i++)
		{
			if (name[i] == '"')
				name[i] = '\'';
		}

		shield = target->client->ps.stats[STAT_ARMOR];
	}

	if (level >= 3 && zyk_sense_health_is_vehicle(target))
	{ // GalaxyRP: [Skills] a vehicle: hull, shields, empty or occupied -- no Force
		const Vehicle_t *veh = target->m_pVehicle;

		max_health = target->client->ps.stats[STAT_MAX_HEALTH];
		max_shield = veh->m_pVehicleInfo->shields;
		type = (veh->m_pPilot || veh->m_iNumPassengers > 0) ? 4 : 3;
	}
	else if (level >= 3)
	{
		max_health = target->client->ps.stats[STAT_MAX_HEALTH];
		force = target->client->ps.fd.forcePower;
		max_force = target->client->ps.fd.forcePowerMax;

		// GalaxyRP fix: [Skills] the shield ceiling everyone is actually held to, published every frame
		// by G_PublishMaxArmor(): max_rpg_shield for a logged-in player, max health for anyone else,
		// NPCs included. The earlier rework sent no maximum for NPCs and logged-out players, on the
		// mistaken belief they had none.
		max_shield = target->client->ps.stats[STAT_MAX_ARMOR];

		if (target->NPC)
		{
			type = 2;
		}
		else if (target->client->sess.amrpgmode == 2)
		{
			type = 1;
		}
		else
		{
			type = 0;
		}
	}

	Com_sprintf(out, out_size, "sensehp %d %d %d %d %d %d %d %d \"%s\"",
		level, health, max_health, shield, max_shield, force, max_force, type, name);
}

void sense_health_info(gentity_t *self, gentity_t *target)
{
	char command[MAX_STRING_CHARS];
	const int level = self->client->pers.skill_levels[35]; // zyk: skill index of Sense Health skill

	if (level < 1 || !zyk_sense_health_valid_target(self, target))
	{
		return;
	}

	zyk_sense_health_command(target, level, command, sizeof(command));
	trap->SendServerCommand(self->s.number, command);
}

extern qboolean duel_tournament_is_duelist(gentity_t *ent);

// DAJ_RP: [Force Enlightenment] "is this player fighting in a Melee Battle right now?" -- melee_mode 2
// is the battle itself (1 is sign-up, 3 the few seconds after a winner is declared, by which point the
// winner has had their Force given back and been handed their prize). melee_players[] is indexed by
// client number, so anything past MAX_CLIENTS -- NPCs run WP_ForcePowersUpdate() too -- is never in it.
static qboolean RP_InMeleeBattle( gentity_t *self )
{
	if ( !self || self->s.number < 0 || self->s.number >= MAX_CLIENTS )
	{
		return qfalse;
	}

	return ( level.melee_mode == 2 && level.melee_players[self->s.number] != -1 ) ? qtrue : qfalse;
}

// DAJ_RP: [Force Enlightenment] put a player's Force back the way it was before Enlightenment
// boosted it: the vanilla undo from WP_ForcePowersUpdate(), moved here verbatim so that
// WP_EndForceEnlightenment() below runs exactly the same code. Restores every level from the
// snapshot taken when the boost began, stops and forgets any power that snapshot had at 0, and
// clears the "boosted" flag. Only meaningful while forceUsingAdded is set; both callers check.
static void WP_UndoEnlightenedPowers( gentity_t *self )
{
	int i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		self->client->ps.fd.forcePowerLevel[i] = self->client->ps.fd.forcePowerBaseLevel[i];
		if (!self->client->ps.fd.forcePowerLevel[i])
		{
			if (self->client->ps.fd.forcePowersActive & (1 << i))
			{
				WP_ForcePowerStop(self, i);
			}
			self->client->ps.fd.forcePowersKnown &= ~(1 << i);
		}

		i++;
	}

	self->client->ps.fd.forceUsingAdded = 0;
}

// DAJ_RP: [Force Enlightenment] end a player's Enlightenment on the spot: clear both powerups and, if
// the boost is currently applied, undo it right now rather than on the next WP_ForcePowersUpdate().
//
// Why "right now" matters: the undo restores a snapshot taken when the boost began. Anything that
// rewrites the player's Force levels in the meantime -- loading a character at /login or /new, a
// minigame saving the Force it is about to strip -- is silently overwritten by that stale snapshot
// one frame later if the flag is still set. That is how a logout-then-login onto a full-Force
// character came out with the logged-out player's powers instead. Callers:
//   - zyk_reset_force_powerups() (g_cmds.c), for /new, /char new, /char use, /login and /logout,
//     BEFORE the next character's (or the logged-out) Force is loaded;
//   - duel_tournament_prepare() and melee_battle_prepare() (g_main.c), BEFORE player_backup_force(),
//     so the Force saved and later restored is the player's own and not a boosted copy that would
//     outlive the Enlightenment.
void WP_EndForceEnlightenment( gentity_t *self )
{
	if ( !self || !self->client )
	{
		return;
	}

	self->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
	self->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;

	if ( self->client->ps.fd.forceUsingAdded )
	{
		WP_UndoEnlightenedPowers( self );
	}
}

void WP_ForcePowersUpdate( gentity_t *self, usercmd_t *ucmd )
{
	int			i, holo, holoregen;
	int			prepower = 0;
	//see if any force powers are running
	if ( !self )
	{
		return;
	}

	if ( !self->client )
	{
		return;
	}

	if (self->client->ps.pm_flags & PMF_FOLLOW)
	{ //not a "real" game client, it's a spectator following someone
		return;
	}
	if (self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		return;
	}

	/*
	if (self->client->ps.fd.saberAnimLevel > self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
	{
		self->client->ps.fd.saberAnimLevel = self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
	}
	else if (!self->client->ps.fd.saberAnimLevel)
	{
		self->client->ps.fd.saberAnimLevel = FORCE_LEVEL_1;
	}
	*/
	//The stance in relation to power level is no longer applicable with the crazy new akimbo/staff stances.
	if (!self->client->ps.fd.saberAnimLevel)
	{
		self->client->ps.fd.saberAnimLevel = FORCE_LEVEL_1;
	}

	if (level.gametype != GT_SIEGE)
	{
		if (!(self->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)))
		{
			self->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		}

		if (self->client->ps.fd.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_1;
		}
	}

	if (self->client->ps.fd.forcePowerSelected < 0 || self->client->ps.fd.forcePowerSelected >= NUM_FORCE_POWERS)
	{ //bad
		self->client->ps.fd.forcePowerSelected = 0;
	}

	if ( ((self->client->sess.selectedFP != self->client->ps.fd.forcePowerSelected) ||
		(self->client->sess.saberLevel != self->client->ps.fd.saberAnimLevel)) &&
		!(self->r.svFlags & SVF_BOT) )
	{
		if (self->client->sess.updateUITime < level.time)
		{ //a bit hackish, but we don't want the client to flood with userinfo updates if they rapidly cycle
		  //through their force powers or saber attack levels

			self->client->sess.selectedFP = self->client->ps.fd.forcePowerSelected;
			self->client->sess.saberLevel = self->client->ps.fd.saberAnimLevel;
		}
	}

	if (!g_LastFrameTime)
	{
		g_LastFrameTime = level.time;
	}

	if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
	{
		self->client->ps.zoomFov = 0;
		self->client->ps.zoomMode = 0;
		self->client->ps.zoomLocked = qfalse;
		self->client->ps.zoomTime = 0;
	}

	if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN &&
		self->client->ps.forceHandExtendTime >= level.time)
	{
		self->client->ps.saberMove = 0;
		self->client->ps.saberBlocking = 0;
		self->client->ps.saberBlocked = 0;
		self->client->ps.weaponTime = 0;
		self->client->ps.weaponstate = WEAPON_READY;
	}
	// GalaxyRP fix: [Death System] never run the knockdown get-up machine for a downed player. This
	// block is precisely the thing that must not happen while the PLAYER_STATUS_DOWNED bit is set, and until
	// now nothing said so: the downed state was enforced only by ClientThink_real() holding
	// forceHandExtendTime permanently in the future, which starves this "else if" of its second
	// condition. Starvation is not a guarantee. ClientThink_real() skips that refresh whenever the
	// player is noclipping, disintegrating or being Force Gripped, and half a second of lapse was
	// enough: G_SpecialRollGetup() below reads the VICTIM'S OWN movement keys, so a downed player held
	// in a grip and shoved around simply rolled to their feet, and a second lapse took them from
	// "up but frozen" (forceDodgeAnim set) to the chain's final else, HANDEXTEND_WEAPONREADY, and full
	// mobility -- still flagged downed, still FL_NOTARGET, still counting down. Gating on the state
	// itself removes every timing margin and survives any future movetype branch.
	//
	// Safe at both ends. On release, RP_ReleaseFromDownedState() and help_up() (g_cmds.c) clear bit 6
	// through RP_ClearDownedState() BEFORE calling play_animation(), so the stand-up animation is not
	// blocked. On death, player_die() and G_Damage()'s finish-off branch clear it before targ->die(),
	// so this block's own "health < 1 -> HANDEXTEND_NONE" cleanup still runs for a corpse. The "if"
	// above -- which zeroes saberMove/weaponTime while the knockdown is held -- is deliberately NOT
	// gated: that one is part of keeping a downed player down.
	else if (self->client->ps.forceHandExtend != HANDEXTEND_NONE &&
		self->client->ps.forceHandExtendTime < level.time &&
		!G_PlayerIsDowned(self))
	{
		if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN &&
			!self->client->ps.forceDodgeAnim)
		{
			if (self->health < 1 || (self->client->ps.eFlags & EF_DEAD))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else if (G_SpecialRollGetup(self))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else
			{ //hmm.. ok.. no more getting up on your own, you've gotta push something, unless..
				if ((level.time-self->client->ps.forceHandExtendTime) > 4000)
				{ //4 seconds elapsed, I guess they're too dumb to push something to get up!
					if (self->client->pers.cmd.upmove &&
						self->client->ps.fd.forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
					{ //force getup
						G_PreDefSound(self->client->ps.origin, PDSOUND_FORCEJUMP);
						self->client->ps.forceDodgeAnim = 2;
						self->client->ps.forceHandExtendTime = level.time + 500;

						//self->client->ps.velocity[2] = 400;
					}
					else if (self->client->ps.quickerGetup)
					{
						G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
						self->client->ps.forceDodgeAnim = 3;
						self->client->ps.forceHandExtendTime = level.time + 500;
						self->client->ps.velocity[2] = 300;
					}
					else
					{
						self->client->ps.forceDodgeAnim = 1;
						self->client->ps.forceHandExtendTime = level.time + 1000;
					}
				}
			}
			self->client->ps.quickerGetup = qfalse;
		}
		else if (self->client->ps.forceHandExtend == HANDEXTEND_POSTTHROWN)
		{
			if (self->health < 1 || (self->client->ps.eFlags & EF_DEAD))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else if (self->client->ps.groundEntityNum != ENTITYNUM_NONE && !self->client->ps.forceDodgeAnim)
			{
				self->client->ps.forceDodgeAnim = 1;
				self->client->ps.forceHandExtendTime = level.time + 1000;
				G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
				self->client->ps.velocity[2] = 100;
			}
			else if (!self->client->ps.forceDodgeAnim)
			{
				self->client->ps.forceHandExtendTime = level.time + 100;
			}
			else
			{
				self->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
			}
		}
		else
		{
			self->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
		}
	}

	if (level.gametype == GT_HOLOCRON)
	{
		HolocronUpdate(self);
	}
	if (level.gametype == GT_JEDIMASTER)
	{
		JediMasterUpdate(self);
	}

	SeekerDroneUpdate(self);

	if (self->client->ps.powerups[PW_FORCE_BOON])
	{
		prepower = self->client->ps.fd.forcePower;
	}

	if (self && self->client && (BG_HasYsalamiri(level.gametype, &self->client->ps) ||
		self->client->ps.fd.forceDeactivateAll || self->client->tempSpectate >= level.time))
	{ //has ysalamiri.. or we want to forcefully stop all his active powers
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION)
			{
				WP_ForcePowerStop(self, i);
			}

			i++;
		}

		if (self->client->tempSpectate >= level.time)
		{
			self->client->ps.fd.forcePower = 100;
			self->client->ps.fd.forceRageRecoveryTime = 0;
		}

		self->client->ps.fd.forceDeactivateAll = 0;

		if (self->client->ps.fd.forceJumpCharge)
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
			self->client->ps.fd.forceJumpCharge = 0;
		}
	}
	else
	{ //otherwise just do a check through them all to see if they need to be stopped for any reason.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION &&
				!BG_CanUseFPNow(level.gametype, &self->client->ps, level.time, i))
			{
				WP_ForcePowerStop(self, i);
			}

			i++;
		}
	}

	i = 0;

	// zyk: added the rpg mode condition, because RPG Mode players cant get power ups
	// DAJ_RP: [Force Enlightenment] and not for a Melee Battle combatant while the battle is on, the
	// same way it already never applies to the two duelists of a Duel Tournament. Both minigames
	// strip the player's Force for the match (melee_battle_prepare(), duel_tournament_prepare() in
	// g_main.c) -- an Enlightenment picked up mid-battle would otherwise add those powers straight
	// back as usable. It stays on the player and applies once the battle is over, on top of the
	// Force the minigame gives back. (Any boost the player brought INTO a minigame is ended before
	// their Force is saved -- see WP_EndForceEnlightenment() above.)
	if (self->client->sess.amrpgmode < 2 && duel_tournament_is_duelist(self) == qfalse &&
		!RP_InMeleeBattle(self) &&
		(self->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] || self->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK]))
	{ //enlightenment
		if (!self->client->ps.fd.forceUsingAdded)
		{
			i = 0;

			while (i < NUM_FORCE_POWERS)
			{
				self->client->ps.fd.forcePowerBaseLevel[i] = self->client->ps.fd.forcePowerLevel[i];

				if (!forcePowerDarkLight[i] ||
					self->client->ps.fd.forceSide == forcePowerDarkLight[i])
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
					self->client->ps.fd.forcePowersKnown |= (1 << i);
				}

				i++;
			}

			self->client->ps.fd.forceUsingAdded = 1;
		}
	}
	else if (self->client->ps.fd.forceUsingAdded)
	{ //we don't have enlightenment but we're still using enlightened powers, so clear them back to how they should be.
		// DAJ_RP: [Force Enlightenment] the undo now lives in WP_UndoEnlightenedPowers() so
		// WP_EndForceEnlightenment() can run the very same code -- unchanged.
		WP_UndoEnlightenedPowers(self);
	}

	i = 0;

	if (!(self->client->ps.fd.forcePowersActive & (1 << FP_TELEPATHY)))
	{ //clear the mindtrick index values
		self->client->ps.fd.forceMindtrickTargetIndex = 0;
		self->client->ps.fd.forceMindtrickTargetIndex2 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex3 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex4 = 0;

		// GalaxyRP fix: [Dead Code] rpg_class permanently 0, mind control block unreachable
	}

	if (self->health < 1)
	{
		self->client->ps.fd.forceGripBeingGripped = 0;
	}

	if (self->client->ps.fd.forceGripBeingGripped > level.time)
	{
		self->client->ps.fd.forceGripCripple = 1;

		//keep the saber off during this period
		if (self->client->ps.weapon == WP_SABER && !self->client->ps.saberHolstered)
		{
			Cmd_ToggleSaber_f(self);
		}
	}
	else
	{
		self->client->ps.fd.forceGripCripple = 0;
	}

	if (self->client->ps.fd.forceJumpSound)
	{
		G_PreDefSound(self->client->ps.origin, PDSOUND_FORCEJUMP);
		self->client->ps.fd.forceJumpSound = 0;
	}

	if (self->client->ps.fd.forceGripCripple)
	{
		if (self->client->ps.fd.forceGripSoundTime < level.time)
		{
			G_PreDefSound(self->client->ps.origin, PDSOUND_FORCEGRIP);
			self->client->ps.fd.forceGripSoundTime = level.time + 1000;
		}
	}

	if (self->client->ps.fd.forcePowersActive & (1 << FP_SPEED))
	{
		self->client->ps.powerups[PW_SPEED] = level.time + 100;
	}

	if ( self->health <= 0 )
	{//if dead, deactivate any active force powers
		for ( i = 0; i < NUM_FORCE_POWERS; i++ )
		{
			if ( self->client->ps.fd.forcePowerDuration[i] || (self->client->ps.fd.forcePowersActive&( 1 << i )) )
			{
				WP_ForcePowerStop( self, (forcePowers_t)i );
				self->client->ps.fd.forcePowerDuration[i] = 0;
			}
		}
		goto powersetcheck;
	}

	if (self->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		self->client->fjDidJump = qfalse;
	}

	if (self->client->ps.fd.forceJumpCharge && self->client->ps.groundEntityNum == ENTITYNUM_NONE && self->client->fjDidJump)
	{ //this was for the "charge" jump method... I guess
		if (ucmd->upmove < 10 && (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_LEVITATION))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
			self->client->ps.fd.forceJumpCharge = 0;
		}
	}

#ifndef METROID_JUMP
	else if ( (ucmd->upmove > 10) && (self->client->ps.pm_flags & PMF_JUMP_HELD) && self->client->ps.groundTime && (level.time - self->client->ps.groundTime) > 150 && !BG_HasYsalamiri(level.gametype, &self->client->ps) && BG_CanUseFPNow(level.gametype, &self->client->ps, level.time, FP_LEVITATION) )
	{//just charging up
		ForceJumpCharge( self, ucmd );
	}
	else if (ucmd->upmove < 10 && self->client->ps.groundEntityNum == ENTITYNUM_NONE && self->client->ps.fd.forceJumpCharge)
	{
		self->client->ps.pm_flags &= ~(PMF_JUMP_HELD);
	}
#endif

	if (!(self->client->ps.pm_flags & PMF_JUMP_HELD) && self->client->ps.fd.forceJumpCharge)
	{
		if (!(ucmd->buttons & BUTTON_FORCEPOWER) ||
			self->client->ps.fd.forcePowerSelected != FP_LEVITATION)
		{
			WP_DoSpecificPower( self, ucmd, FP_LEVITATION );
		}
	}

	if ( ucmd->buttons & BUTTON_FORCEGRIP )
	{ //grip is one of the powers with its own button.. if it's held, call the specific grip power function.
		WP_DoSpecificPower( self, ucmd, FP_GRIP );
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_GRIP)
			{
				WP_ForcePowerStop(self, FP_GRIP);
			}
		}
	}

	// GalaxyRP fix: [Dead Code] rpg_class permanently 0, unique-skill disjunct always false
	if ( ucmd->buttons & BUTTON_FORCE_LIGHTNING )
	{ //lightning
		WP_DoSpecificPower(self, ucmd, FP_LIGHTNING);
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_LIGHTNING))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_LIGHTNING)
			{
				WP_ForcePowerStop(self, FP_LIGHTNING);
			}
		}
	}

	if ( ucmd->buttons & BUTTON_FORCE_DRAIN )
	{ //drain
		WP_DoSpecificPower(self, ucmd, FP_DRAIN);
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_DRAIN))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_DRAIN)
			{
				WP_ForcePowerStop(self, FP_DRAIN);
			}
		}
	}

	if ( (ucmd->buttons & BUTTON_FORCEPOWER) &&
		BG_CanUseFPNow(level.gametype, &self->client->ps, level.time, self->client->ps.fd.forcePowerSelected))
	{
		if (self->client->ps.fd.forcePowerSelected == FP_LEVITATION)
			ForceJumpCharge( self, ucmd );
		else
			WP_DoSpecificPower( self, ucmd, self->client->ps.fd.forcePowerSelected );
	}
	else
	{
		self->client->ps.fd.forceButtonNeedRelease = 0;
	}

	for ( i = 0; i < NUM_FORCE_POWERS; i++ )
	{
		if ( self->client->ps.fd.forcePowerDuration[i] )
		{
			if ( self->client->ps.fd.forcePowerDuration[i] < level.time )
			{
				if ( (self->client->ps.fd.forcePowersActive&( 1 << i )) )
				{//turn it off
					WP_ForcePowerStop( self, (forcePowers_t)i );
				}
				self->client->ps.fd.forcePowerDuration[i] = 0;
			}
			// zyk: using Sense Health skill of RPG Mode
			// GalaxyRP fix: [Dead Code] rpg_class permanently 0, rpg_class==8 disjunct always false
			// DAJ_RP: [Settings] and only while the Sense Health Toggle (/settings 1, player_settings bit 5,
			// inverted: set == OFF) is on. Nothing else sends "sensehp", so with it off the readout is
			// simply never sent, and one already on screen fades out by itself within a second.
			else if (i == FP_SEE && self->client->sess.amrpgmode == 2 && (self->client->pers.skill_levels[35] > 0) &&
					 !(self->client->pers.player_settings & (1 << 5)) &&
					 self->client->ps.fd.forcePowersActive & ( 1 << FP_SEE ) && self->client->pers.sense_health_timer < level.time)
			{
				// GalaxyRP fix: [Skills] the skill picks its own target now (crosshair, else nearest in
				// sight) instead of ps.lookTarget -- see zyk_sense_health_find_target() above
				sense_health_info(self, zyk_sense_health_find_target(self));

				// GalaxyRP (Alex): [Force Powers] Update health display every half a second
				self->client->pers.sense_health_timer = level.time + 500;
			}
		}
		if ( (self->client->ps.fd.forcePowersActive&( 1 << i )) )
		{
			WP_ForcePowerRun( self, (forcePowers_t)i, ucmd );
		}
	}

	if ( !(self->client->ps.fd.forcePowersActive & (1<<FP_DRAIN)) )
		self->client->force.drainDebounce = level.time;
	if ( !(self->client->ps.fd.forcePowersActive & (1<<FP_LIGHTNING)) )
		self->client->force.lightningDebounce = level.time;

	if ( (!self->client->ps.fd.forcePowersActive || self->client->ps.fd.forcePowersActive == (1 << FP_DRAIN)) &&
		(!self->client->ps.saberInFlight || self->client->ps.saberEntityNum == 0) && // zyk: now force regenerates if saber is dropped in the ground after being thrown
		(self->client->ps.weapon != WP_SABER || !BG_SaberInSpecial(self->client->ps.saberMove)) )
	{//when not using the force, regenerate at 1 point per half second
		while ( self->client->ps.fd.forcePowerRegenDebounceTime < level.time )
		{
			if (level.gametype != GT_HOLOCRON || g_maxHolocronCarry.value)
			{
				// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Monk Meditation / Force User-Duelist / Force Gunner regen branches unreachable
				if ( self->client->ps.powerups[PW_FORCE_BOON] )
					WP_ForcePowerRegenerate( self, 6 );
				else if ( self->client->ps.isJediMaster && level.gametype == GT_JEDIMASTER )
					WP_ForcePowerRegenerate( self, 4 ); //jedi master regenerates 4 times as fast
				else
					WP_ForcePowerRegenerate( self, 0 );
			}
			else
			{ //regenerate based on the number of holocrons carried
				holoregen = 0;
				holo = 0;
				while (holo < NUM_FORCE_POWERS)
				{
					if (self->client->ps.holocronsCarried[holo])
						holoregen++;
					holo++;
				}

				WP_ForcePowerRegenerate(self, holoregen);
			}

			if (level.gametype == GT_SIEGE)
			{
				if ( self->client->holdingObjectiveItem && g_entities[self->client->holdingObjectiveItem].inuse && g_entities[self->client->holdingObjectiveItem].genericValue15 )
					self->client->ps.fd.forcePowerRegenDebounceTime += 7000; //1 point per 7 seconds.. super slow
				else if (self->client->siegeClass != -1 && (bgSiegeClasses[self->client->siegeClass].classflags & (1<<CFL_FASTFORCEREGEN)))
					self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceRegenTime.integer*0.2, 1); //if this is siege and our player class has the fast force regen ability, then recharge with 1/5th the usual delay
				else
					self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceRegenTime.integer, 1);
			}
			else
			{
				if ( level.gametype == GT_POWERDUEL && self->client->sess.duelTeam == DUELTEAM_LONE )
				{
					if ( duel_fraglimit.integer )
						self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceRegenTime.integer * (0.6 + (.3 * (float)self->client->sess.wins / (float)duel_fraglimit.integer)), 1);
					else
						self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceRegenTime.integer*0.7, 1);
				}
				// GalaxyRP: [Force Duel] a private duel regenerates force on its own clock, one cvar per
				// kind, both adapted from TaystJK which carries the same pair. A full force duel is the
				// one place in this mod where two players spend force against each other with nothing
				// else going on, and an ordinary duel still spends it too -- Jump costs force, and in a
				// saber duel that is most of what force is for.
				//
				// Both default to 200, the same as g_forceRegenTime, so neither changes anything until an
				// an admin sets it -- but note the shipped galaxyrp_server.cfg sets g_forceRegenTime to 60
				// and therefore sets these two to 60 as well, or a duel would silently regenerate slower
				// than the rest of the map on that server.
				//
				// Bounds-checked on the client number for the same reason zyk_duel_is_full_force() is:
				// this runs for NPCs too, whose ps.clientNum is an entity number rather than a client
				// slot. An out-of-range number falls through to g_forceRegenTime, which is the safe
				// answer -- an NPC cannot be in a private duel to begin with.
				else if (self->client->ps.duelInProgress && self->s.number < MAX_CLIENTS &&
					level.duel_types[self->s.number] == 1)
				{
					self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceDuelForceRegenTime.integer, 1);
				}
				else if (self->client->ps.duelInProgress && self->s.number < MAX_CLIENTS)
				{ // an ordinary saber duel
					self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_saberDuelForceRegenTime.integer, 1);
				}
				else
				{
					self->client->ps.fd.forcePowerRegenDebounceTime += Q_max(g_forceRegenTime.integer, 1);
				}
			}
		}
	}
	else
	{
		self->client->ps.fd.forcePowerRegenDebounceTime = level.time;
	}

powersetcheck:

	if (prepower && self->client->ps.fd.forcePower < prepower)
	{
		int dif = ((prepower - self->client->ps.fd.forcePower)/2);
		if (dif < 1)
		{
			dif = 1;
		}

		self->client->ps.fd.forcePower = (prepower-dif);
	}
}

qboolean Jedi_DodgeEvasion( gentity_t *self, gentity_t *shooter, trace_t *tr, int hitLoc )
{
	int	dodgeAnim = -1;

	if ( !self || !self->client || self->health <= 0 )
	{
		return qfalse;
	}

	// DAJ_RP: [NPC System] a frozen NPC does not dodge -- see RP_NpcFreeze() (NPC_spawn.c)
	if ( RP_NpcIsFrozen( self ) )
	{
		return qfalse;
	}

	if (!g_forceDodge.integer)
	{
		return qfalse;
	}

	if (g_forceDodge.integer != 2)
	{
		if (!(self->client->ps.fd.forcePowersActive & (1 << FP_SEE)))
		{
			return qfalse;
		}
	}

	if ( self->client->ps.groundEntityNum == ENTITYNUM_NONE )
	{//can't dodge in mid-air
		return qfalse;
	}

	if ( self->client->ps.weaponTime > 0 || self->client->ps.forceHandExtend != HANDEXTEND_NONE )
	{//in some effect that stops me from moving on my own
		return qfalse;
	}

	if (g_forceDodge.integer == 2)
	{
		if (self->client->ps.fd.forcePowersActive)
		{ //for now just don't let us dodge if we're using a force power at all
			return qfalse;
		}
	}

	if (g_forceDodge.integer == 2)
	{
		if ( !WP_ForcePowerUsable( self, FP_SPEED ) )
		{//make sure we have it and have enough force power
			return qfalse;
		}
	}

	if (g_forceDodge.integer == 2)
	{
		if ( Q_irand( 1, 7 ) > self->client->ps.fd.forcePowerLevel[FP_SPEED] )
		{//more likely to fail on lower force speed level
			return qfalse;
		}
	}
	else
	{
		// GalaxyRP (Alex): [Force Powers] Level 4 will dodge 30% of the time.
		if (self->client->ps.fd.forcePowerLevel[FP_SEE] < FORCE_LEVEL_3)
		{//more likely to fail on lower force sight level
			if (Q_irand(0, 3))
			{
				return qfalse;
			}
		}
		// GalaxyRP (Alex): [Force Powers] Level 4 will dodge half the time.
		if (self->client->ps.fd.forcePowerLevel[FP_SEE] < FORCE_LEVEL_4)
		{//more likely to fail on lower force sight level
			if (Q_irand(0, 1)) 
			{
				return qfalse;
			}
		}
	}

	// GalaxyRP fix: [Dead Code] rpg_class permanently 0, Bounty Hunter no-dodge branch unreachable

	switch( hitLoc )
	{
	case HL_NONE:
		return qfalse;
		break;

	case HL_FOOT_RT:
	case HL_FOOT_LT:
	case HL_LEG_RT:
	case HL_LEG_LT:
		return qfalse;

	case HL_BACK_RT:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	case HL_CHEST_RT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_BACK_LT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_CHEST_LT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_BACK:
	case HL_CHEST:
	case HL_WAIST:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	case HL_ARM_RT:
	case HL_HAND_RT:
		dodgeAnim = BOTH_DODGE_L;
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		dodgeAnim = BOTH_DODGE_R;
		break;
	case HL_HEAD:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	default:
		return qfalse;
	}

	if ( dodgeAnim != -1 )
	{
		//Our own happy way of forcing an anim:
		self->client->ps.forceHandExtend = HANDEXTEND_DODGE;
		self->client->ps.forceDodgeAnim = dodgeAnim;
		self->client->ps.forceHandExtendTime = level.time + 300;

		self->client->ps.powerups[PW_SPEEDBURST] = level.time + 100;

		if (g_forceDodge.integer == 2)
		{
			ForceSpeed( self, 500 );
		}
		else
		{
			G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/speed.wav") );
		}
		return qtrue;
	}
	return qfalse;
}
