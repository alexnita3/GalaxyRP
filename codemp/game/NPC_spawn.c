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

//b_spawn.cpp
//added by MCG
#include "b_local.h"
#include "anims.h"
#include "w_saber.h"
#include "bg_saga.h"
#include "bg_vehicles.h"
#include "g_nav.h"

extern void G_DebugPrint( int level, const char *format, ... );

extern qboolean G_CheckInSolid (gentity_t *self, qboolean fix);
extern qboolean ClientUserinfoChanged( int clientNum );
extern qboolean SpotWouldTelefrag2( gentity_t *mover, vec3_t dest );
extern void Jedi_Cloak( gentity_t *self );

extern void Q3_SetParm (int entID, int parmNum, const char *parmValue);
extern team_t TranslateTeamName( const char *name );
extern char	*TeamNames[NPCTEAM_NUM_TEAMS];

extern void PM_SetTorsoAnimTimer( gentity_t *ent, int *torsoAnimTimer, int time );
extern void PM_SetLegsAnimTimer( gentity_t *ent, int *legsAnimTimer, int time );

extern void ST_ClearTimers( gentity_t *ent );
extern void Jedi_ClearTimers( gentity_t *ent );
extern void NPC_ShadowTrooper_Precache( void );
extern void NPC_Gonk_Precache( void );
extern void NPC_Mouse_Precache( void );
extern void NPC_Seeker_Precache( void );
extern void NPC_Remote_Precache( void );
extern void	NPC_R2D2_Precache(void);
extern void	NPC_R5D2_Precache(void);
extern void NPC_Probe_Precache(void);
extern void NPC_Interrogator_Precache(gentity_t *self);
extern void NPC_MineMonster_Precache( void );
extern void NPC_Howler_Precache( void );
extern void NPC_ATST_Precache(void);
extern void NPC_Sentry_Precache(void);
extern void NPC_Mark1_Precache(void);
extern void NPC_Mark2_Precache(void);
extern void NPC_GalakMech_Precache( void );
extern void NPC_GalakMech_Init( gentity_t *ent );
extern void NPC_Protocol_Precache( void );
extern void Boba_Precache( void );
extern void NPC_Wampa_Precache( void );
gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle );

extern void Rancor_SetBolts( gentity_t *self );
extern void Wampa_SetBolts( gentity_t *self );

#define	NSF_DROP_TO_FLOOR	16

// GalaxyRP: [Entity System] NPC and vehicle spawners -- see RP_SpawnerRespawn()
static void RP_SpawnerQueuedFire( gentity_t *ent );
static void RP_SpawnerScheduleRespawn( gentity_t *spawner );

// PAIN functions...
//
extern void funcBBrushPain				(gentity_t *self, gentity_t *attacker, int damage);
extern void misc_model_breakable_pain	(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Pain					(gentity_t *self, gentity_t *attacker, int damage);
extern void station_pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void func_usable_pain			(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_ATST_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_ST_Pain					(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Jedi_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Droid_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Probe_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_MineMonster_Pain		(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Howler_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Seeker_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Remote_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void emplaced_gun_pain			(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Mark1_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_GM_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Sentry_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Mark2_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void PlayerPain					(gentity_t *self, gentity_t *attacker, int damage);
extern void GasBurst					(gentity_t *self, gentity_t *attacker, int damage);
extern void CrystalCratePain			(gentity_t *self, gentity_t *attacker, int damage);
extern void TurretPain					(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Wampa_Pain				(gentity_t *self, gentity_t *attacker, int damage);
extern void NPC_Rancor_Pain				(gentity_t *self, gentity_t *attacker, int damage);

int WP_SetSaberModel( gclient_t *client, class_t npcClass )
{
	//rwwFIXMEFIXME: Do something here, need to let the client know.
	return 1;
}

/*
-------------------------
NPC_PainFunc
-------------------------
*/
typedef void (PAIN_FUNC) (gentity_t *self, gentity_t *attacker, int damage);

PAIN_FUNC *NPC_PainFunc( gentity_t *ent )
{
	void (*func)(gentity_t *self, gentity_t *attacker, int damage);

	if ( ent->client->ps.weapon == WP_SABER )
	{
		func = NPC_Jedi_Pain;
	}
	else
	{
		// team no longer indicates race/species, use NPC_class to determine different npc types
		/*
		switch ( ent->client->playerTeam )
		{
		default:
			func = painF_NPC_Pain;
			break;
		}
		*/
		switch( ent->client->NPC_class )
		{
		// troopers get special pain
		case CLASS_STORMTROOPER:
		case CLASS_SWAMPTROOPER:
			func = NPC_ST_Pain;
			break;

		case CLASS_SEEKER:
			func = NPC_Seeker_Pain;
			break;

		case CLASS_REMOTE:
			func = NPC_Remote_Pain;
			break;

		case CLASS_MINEMONSTER:
			func = NPC_MineMonster_Pain;
			break;

		case CLASS_HOWLER:
			func = NPC_Howler_Pain;
			break;

		// all other droids, did I miss any?
		case CLASS_GONK:
		case CLASS_R2D2:
		case CLASS_R5D2:
		case CLASS_MOUSE:
		case CLASS_PROTOCOL:
		case CLASS_INTERROGATOR:
			func = NPC_Droid_Pain;
			break;
		case CLASS_PROBE:
			func = NPC_Probe_Pain;
			break;

		case CLASS_SENTRY:
			func = NPC_Sentry_Pain;
			break;
		case CLASS_MARK1:
			func = NPC_Mark1_Pain;
			break;
		case CLASS_MARK2:
			func = NPC_Mark2_Pain;
			break;
		case CLASS_ATST:
			func = NPC_ATST_Pain;
			break;
		case CLASS_GALAKMECH:
			func = NPC_GM_Pain;
			break;
		case CLASS_RANCOR:
			func = NPC_Rancor_Pain;
			break;
		case CLASS_WAMPA:
			func = NPC_Wampa_Pain;
			break;
		// everyone else gets the normal pain func
		default:
			func = NPC_Pain;
			break;
		}

	}

	return func;
}


/*
-------------------------
NPC_TouchFunc
-------------------------
*/
typedef void (TOUCH_FUNC) (gentity_t *self, gentity_t *other, trace_t *trace);

TOUCH_FUNC *NPC_TouchFunc( gentity_t *ent )
{
	void (*func)(gentity_t *self, gentity_t *other, trace_t *trace);

	func = NPC_Touch;

	return func;
}

/*
-------------------------
NPC_SetMiscDefaultData
-------------------------
*/

extern void G_CreateG2AttachedWeaponModel( gentity_t *ent, const char *weaponModel, int boltNum, int weaponNum );
void NPC_SetMiscDefaultData( gentity_t *ent )
{
	if ( ent->spawnflags & SFB_CINEMATIC )
	{//if a cinematic guy, default us to wait bState
		ent->NPC->behaviorState = BS_CINEMATIC;
	}
	if ( ent->client->NPC_class == CLASS_BOBAFETT )
	{//set some stuff, precache
		Boba_Precache();
		ent->client->ps.fd.forcePowersKnown |= ( 1 << FP_LEVITATION );
		ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_3;
		ent->client->ps.fd.forcePower = 100;
		ent->NPC->scriptFlags |= (SCF_ALT_FIRE|SCF_NO_GROUPS);
	}
	//if ( !Q_stricmp( "atst_vehicle", ent->NPC_type ) )//FIXME: has to be a better, easier way to tell this, no?
	if (ent->s.NPC_class == CLASS_VEHICLE && ent->m_pVehicle)
	{
		ent->s.g2radius = 255;//MAX for this value, was (ent->r.maxs[2]-ent->r.mins[2]), which is 272 or something

		if (ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
		{
			ent->mass = 2000;//???
			ent->flags |= (FL_SHIELDED|FL_NO_KNOCKBACK);
			ent->pain = NPC_ATST_Pain;
		}
		//turn the damn hatch cover on and LEAVE it on
		trap->G2API_SetSurfaceOnOff( ent->ghoul2, "head_hatchcover", 0/*TURN_ON*/ );
	}
	if ( !Q_stricmp( "wampa", ent->NPC_type ) )
	{//FIXME: extern this into NPC.cfg?
		Wampa_SetBolts( ent );
		ent->s.g2radius = 80;//???
		ent->mass = 300;//???
		ent->flags |= FL_NO_KNOCKBACK;
		ent->pain = NPC_Wampa_Pain;
	}
	if ( ent->client->NPC_class == CLASS_RANCOR )
	{
		Rancor_SetBolts( ent );
		ent->s.g2radius = 255;//MAX for this value, was (ent->r.maxs[2]-ent->r.mins[2]), which is 272 or something
		ent->mass = 1000;//???
		ent->flags |= FL_NO_KNOCKBACK;
		ent->pain = NPC_Rancor_Pain;
		ent->health *= 4;
	}
	if ( !Q_stricmp( "Yoda", ent->NPC_type ) )
	{//FIXME: extern this into NPC.cfg?
		ent->NPC->scriptFlags |= SCF_NO_FORCE;//force powers don't work on him
	}
	if ( !Q_stricmp( "emperor", ent->NPC_type )
		|| !Q_stricmp( "cultist_grip", ent->NPC_type )
		|| !Q_stricmp( "cultist_drain", ent->NPC_type )
		|| !Q_stricmp( "cultist_lightning", ent->NPC_type ))
	{//FIXME: extern this into NPC.cfg?
		ent->NPC->scriptFlags |= SCF_DONT_FIRE;//so he uses only force powers
	}
	//==================
//	if ( ent->client->ps.saber[0].type != SABER_NONE )
	//if (ent->client->ps.weapon == WP_SABER) //rwwFIXMEFIXME: is this going to work?
	if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER)) // zyk: new condition. Tests if npc has the saber
	{//if I'm equipped with a saber, initialize it (them)
	//	ent->client->ps.SaberDeactivate();
	//	ent->client->ps.SetSaberLength( 0 );
		// zyk: changes to saber
		if (ent->client->ps.weapon != WP_SABER)
			NPC_ChangeWeapon(WP_SABER);

		WP_SaberInitBladeData( ent );
		ent->client->ps.saberHolstered = 2;
	//	G_CreateG2AttachedWeaponModel( ent, ent->client->ps.saber[0].model, ent->handRBolt, 0 );
	//	if ( ent->client->ps.dualSabers )
	//	{
	//		G_CreateG2AttachedWeaponModel( ent, ent->client->ps.saber[1].model, ent->handLBolt, 1 );
	//	}
		Jedi_ClearTimers( ent );
	}
	if ( ent->client->ps.fd.forcePowersKnown != 0 )
	{
		WP_InitForcePowers( ent );
		WP_SpawnInitForcePowers(ent); //rww
	}
	if ( ent->client->NPC_class == CLASS_SEEKER )
	{
		ent->NPC->defaultBehavior = BS_DEFAULT;
		ent->client->ps.gravity = 0;
		ent->NPC->aiFlags |= NPCAI_CUSTOM_GRAVITY;
		ent->client->ps.eFlags2 |= EF2_FLYING;
		ent->count = 30; // SEEKER shot ammo count
	}
	//***I'm not sure whether I should leave this as a TEAM_ switch, I think NPC_class may be more appropriate - dmv
	switch(ent->client->playerTeam)
	{
	case NPCTEAM_PLAYER:
		//ent->flags |= FL_NO_KNOCKBACK;
		// DAJ_RP: [NPC] CLASS_KYLE is a good Jedi too, as single player has it (code/game/NPC_spawn.cpp): its
		// side's enemy is NPCTEAM_ENEMY, and spawnflag 16 makes a ceiling ambusher. The base game's kyle.npc
		// sets no enemyTeam (GalaxyRP's rp_npc_fix.npc copies of its Kyles do), and with none (NPCTEAM_FREE)
		// NPC_ValidEnemy() takes anyone not of its class and side for an enemy -- bartenders, droids and wild
		// creatures included.
		if ( ent->client->NPC_class == CLASS_JEDI || ent->client->NPC_class == CLASS_LUKE || ent->client->NPC_class == CLASS_KYLE )
		{//good jedi
			ent->client->enemyTeam = NPCTEAM_ENEMY;
			if ( ent->spawnflags & JSF_AMBUSH )
			{//ambusher
				ent->NPC->scriptFlags |= SCF_IGNORE_ALERTS;
				ent->client->noclip = qtrue;//hang
			}
		}
		else
		{
			/*
			if (ent->client->ps.weapon != WP_NONE)
			{
				G_CreateG2AttachedWeaponModel( ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0 );
			}
			*/
			switch ( ent->client->ps.weapon )
			{
			case WP_BRYAR_PISTOL://FIXME: new weapon: imp blaster pistol
	//		case WP_BLASTER_PISTOL:
			case WP_DISRUPTOR:
			case WP_BOWCASTER:
			case WP_REPEATER:
			case WP_DEMP2:
			case WP_FLECHETTE:
			case WP_ROCKET_LAUNCHER:
			default:
				break;
			case WP_THERMAL:
			case WP_BLASTER:
				//FIXME: health in NPCs.cfg, and not all blaster users are stormtroopers
				//ent->health = 25;
				//FIXME: not necc. a ST
				ST_ClearTimers( ent );
				if ( ent->NPC->rank >= RANK_LT || ent->client->ps.weapon == WP_THERMAL )
				{//officers, grenade-throwers use alt-fire
					//ent->health = 50;
					//ent->NPC->scriptFlags |= SCF_ALT_FIRE;
				}
				break;
			}
		}
		// DAJ_RP: [NPC] CLASS_KYLE used to be first here, a leftover from when Kyle was the player and a Kyle
		// NPC only ever stood in cutscenes. So every Kyle on the player's side spawned with no AI
		// (BS_CINEMATIC) -- stood still, never took an enemy, never fought back -- and /npc team or "npcteam"
		// could not wake him, since neither touches defaultBehavior. Single player had already moved the line
		// to CLASS_PLAYER, a class multiplayer does not have (ClassTable in NPC_stats.c leaves it out, so the
		// "player" and "KylePlayer" NPCs of rp_npc_fix.npc that name it get no class, and keep their AI). The
		// CINEMATIC spawnflag still makes any NPC, Kyle included, a no-AI one.
		if ( ent->client->NPC_class == CLASS_VEHICLE || (ent->spawnflags & SFB_CINEMATIC) )
		{
			ent->NPC->defaultBehavior = BS_CINEMATIC;
		}
		else
		{
			/*
			ent->NPC->defaultBehavior = BS_FOLLOW_LEADER;
			ent->client->leader = &g_entities[0];
			*/
		}
		break;

	case NPCTEAM_NEUTRAL:

		if ( Q_stricmp( ent->NPC_type, "gonk" ) == 0 )
		{
			// I guess we generically make them player usable
			ent->r.svFlags |= SVF_PLAYER_USABLE;

			// Not even sure if we want to give different levels of batteries?  ...Or even that these are the values we'd want to use.
			/*
			switch ( g_npcspskill.integer )
			{
			case 0:	//	EASY
				ent->client->ps.batteryCharge = MAX_BATTERIES * 0.8f;
				break;
			case 1:	//	MEDIUM
				ent->client->ps.batteryCharge = MAX_BATTERIES * 0.75f;
				break;
			default :
			case 2:	//	HARD
				ent->client->ps.batteryCharge = MAX_BATTERIES * 0.5f;
				break;
			}*/
			//rwwFIXMEFIXME: Make use of this.
		}
		break;

	case NPCTEAM_ENEMY:
		{
			ent->NPC->defaultBehavior = BS_DEFAULT;
			if ( ent->client->NPC_class == CLASS_SHADOWTROOPER )
			{//FIXME: a spawnflag?
				Jedi_Cloak( ent );
			}
		 	if( ent->client->NPC_class == CLASS_TAVION ||
				ent->client->NPC_class == CLASS_REBORN ||
				ent->client->NPC_class == CLASS_DESANN ||
				ent->client->NPC_class == CLASS_SHADOWTROOPER )
			{
				ent->client->enemyTeam = NPCTEAM_PLAYER;
				if ( ent->spawnflags & JSF_AMBUSH )
				{//ambusher
					ent->NPC->scriptFlags |= SCF_IGNORE_ALERTS;
					ent->client->noclip = qtrue;//hang
				}
			}
			else if( ent->client->NPC_class == CLASS_PROBE || ent->client->NPC_class == CLASS_REMOTE ||
						ent->client->NPC_class == CLASS_INTERROGATOR || ent->client->NPC_class == CLASS_SENTRY)
			{
				ent->NPC->defaultBehavior = BS_DEFAULT;
				ent->client->ps.gravity = 0;
				ent->NPC->aiFlags |= NPCAI_CUSTOM_GRAVITY;
				ent->client->ps.eFlags2 |= EF2_FLYING;
			}
			else
			{
		//		G_CreateG2AttachedWeaponModel( ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0 );
				switch ( ent->client->ps.weapon )
				{
				case WP_BRYAR_PISTOL:
					break;
		//		case WP_BLASTER_PISTOL:
		//			break;
				case WP_DISRUPTOR:
					//Sniper
					//ent->NPC->scriptFlags |= SCF_ALT_FIRE;//FIXME: use primary fire sometimes?  Up close?  Different class of NPC?
					break;
				case WP_BOWCASTER:
					break;
				case WP_REPEATER:
					//machine-gunner
					break;
				case WP_DEMP2:
					break;
				case WP_FLECHETTE:
					//shotgunner
					if ( !Q_stricmp( "stofficeralt", ent->NPC_type ) )
					{
						//ent->NPC->scriptFlags |= SCF_ALT_FIRE;
					}
					break;
				case WP_ROCKET_LAUNCHER:
					break;
				case WP_THERMAL:
					//Gran, use main, bouncy fire
//					ent->NPC->scriptFlags |= SCF_ALT_FIRE;
					break;
				case WP_STUN_BATON:
					break;
				default:
				case WP_BLASTER:
					//FIXME: health in NPCs.cfg, and not all blaster users are stormtroopers
					//FIXME: not necc. a ST
					ST_ClearTimers( ent );
					if ( ent->NPC->rank >= RANK_COMMANDER )
					{//commanders use alt-fire
						//ent->NPC->scriptFlags |= SCF_ALT_FIRE;
					}
					if ( !Q_stricmp( "rodian2", ent->NPC_type ) )
					{
						//ent->NPC->scriptFlags |= SCF_ALT_FIRE;
					}
					break;
				}
				if ( !Q_stricmp( "galak_mech", ent->NPC_type ) )
				{//starts with armor
					NPC_GalakMech_Init( ent );
				}
			}
		}
		break;

	default:
		break;
	}


	if ( ent->client->NPC_class == CLASS_SEEKER
		&& ent->activator )
	{//assume my teams are already set correctly
	}
	else
	{
		//for siege, want "bad" npc's allied with the "bad" team
		if (level.gametype == GT_SIEGE && ent->s.NPC_class != CLASS_VEHICLE)
		{
			if (ent->client->enemyTeam == NPCTEAM_PLAYER)
			{
				ent->client->sess.sessionTeam = SIEGETEAM_TEAM1;
			}
			else if (ent->client->enemyTeam == NPCTEAM_ENEMY)
			{
				ent->client->sess.sessionTeam = SIEGETEAM_TEAM2;
			}
			else
			{
				ent->client->sess.sessionTeam = TEAM_FREE;
			}
		}
	}

	if ( ent->client->NPC_class == CLASS_ATST || ent->client->NPC_class == CLASS_MARK1 ) // chris/steve/kevin requested that the mark1 be shielded also
	{
		ent->flags |= (FL_SHIELDED|FL_NO_KNOCKBACK);
	}
}

/*
-------------------------
NPC_WeaponsForTeam
-------------------------
*/

/* zyk: now always loads weapons from npc .cfg
int NPC_WeaponsForTeam( team_t team, int spawnflags, const char *NPC_type )
{
	//*** not sure how to handle this, should I pass in class instead of team and go from there? - dmv
	switch(team)
	{
//	case TEAM_IMPERIAL:
	case NPCTEAM_ENEMY:
		if ( Q_stricmp( "tavion", NPC_type ) == 0 ||
			Q_strncmp( "reborn", NPC_type, 6 ) == 0 ||
			Q_stricmp( "desann", NPC_type ) == 0 ||
			Q_strncmp( "shadowtrooper", NPC_type, 13 ) == 0 )
			return ( 1 << WP_SABER);
//			return ( 1 << WP_IMPERIAL_BLADE);
		//NOTENOTE: Falls through if not a knife user

//	case TEAM_SCAVENGERS:
//	case TEAM_MALON:
		//FIXME: default weapon in npc config?
		if ( Q_strncmp( "stofficer", NPC_type, 9 ) == 0 )
		{
			return ( 1 << WP_FLECHETTE);
		}
		if ( Q_stricmp( "stcommander", NPC_type ) == 0 )
		{
			return ( 1 << WP_REPEATER);
		}
		if ( Q_stricmp( "swamptrooper", NPC_type ) == 0 )
		{
			return ( 1 << WP_FLECHETTE);
		}
		if ( Q_stricmp( "swamptrooper2", NPC_type ) == 0 )
		{
			return ( 1 << WP_REPEATER);
		}
		if ( Q_stricmp( "rockettrooper", NPC_type ) == 0 )
		{
			return ( 1 << WP_ROCKET_LAUNCHER);
		}
		if ( Q_strncmp( "shadowtrooper", NPC_type, 13 ) == 0 )
		{
			return ( 1 << WP_SABER);//|( 1 << WP_RAPID_CONCUSSION)?
		}
		if ( Q_stricmp( "imperial", NPC_type ) == 0 )
		{
			//return ( 1 << WP_BLASTER_PISTOL);
			return ( 1 << WP_BLASTER);
		}
		if ( Q_strncmp( "impworker", NPC_type, 9 ) == 0 )
		{
			//return ( 1 << WP_BLASTER_PISTOL);
			return ( 1 << WP_BLASTER);
		}
		if ( Q_stricmp( "stormpilot", NPC_type ) == 0 )
		{
			//return ( 1 << WP_BLASTER_PISTOL);
			return ( 1 << WP_BLASTER);
		}
		if ( Q_stricmp( "galak", NPC_type ) == 0 )
		{
			return ( 1 << WP_BLASTER);
		}
		if ( Q_stricmp( "galak_mech", NPC_type ) == 0 )
		{
			return ( 1 << WP_REPEATER);
		}
		if ( Q_strncmp( "ugnaught", NPC_type, 8 ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_stricmp( "granshooter", NPC_type ) == 0 )
		{
			return ( 1 << WP_BLASTER);
		}
		if ( Q_stricmp( "granboxer", NPC_type ) == 0 )
		{
			return ( 1 << WP_STUN_BATON);
		}
		if ( Q_strncmp( "gran", NPC_type, 4 ) == 0 )
		{
			return (( 1 << WP_THERMAL)|( 1 << WP_STUN_BATON));
		}
		if ( Q_stricmp( "rodian", NPC_type ) == 0 )
		{
			return ( 1 << WP_DISRUPTOR);
		}
		if ( Q_stricmp( "rodian2", NPC_type ) == 0 )
		{
			return ( 1 << WP_BLASTER);
		}

		if (( Q_stricmp( "interrogator",NPC_type) == 0) || ( Q_stricmp( "sentry",NPC_type) == 0) || (Q_strncmp( "protocol",NPC_type,8) == 0) )
		{
			return WP_NONE;
		}

		if ( Q_strncmp( "weequay", NPC_type, 7 ) == 0 )
		{
			return ( 1 << WP_BOWCASTER);//|( 1 << WP_STAFF )(FIXME: new weap?)
		}
		if ( Q_stricmp( "impofficer", NPC_type ) == 0 )
		{
			return ( 1 << WP_BLASTER);
		}
		if ( Q_stricmp( "impcommander", NPC_type ) == 0 )
		{
			return ( 1 << WP_BLASTER);
		}
		if (( Q_stricmp( "probe", NPC_type ) == 0 ) || ( Q_stricmp( "seeker", NPC_type ) == 0 ))
		{
			//return ( 1 << WP_BOT_LASER);
			return 0;
		}
		if ( Q_stricmp( "remote", NPC_type ) == 0 )
		{
			//return ( 1 << WP_BOT_LASER );
			return 0;
		}
		if ( Q_stricmp( "trandoshan", NPC_type ) == 0 )
		{
			return (1<<WP_REPEATER);
		}
		if ( Q_stricmp( "atst", NPC_type ) == 0 )
		{
			//return (( 1 << WP_ATST_MAIN)|( 1 << WP_ATST_SIDE));
			return 0;
		}
		if ( Q_stricmp( "mark1", NPC_type ) == 0 )
		{
			//return ( 1 << WP_BOT_LASER);
			return 0;
		}
		if ( Q_stricmp( "mark2", NPC_type ) == 0 )
		{
			//return ( 1 << WP_BOT_LASER);
			return 0;
		}
		if ( Q_stricmp( "minemonster", NPC_type ) == 0 )
		{
			return (( 1 << WP_STUN_BATON));
		}
		if ( Q_stricmp( "howler", NPC_type ) == 0 )
		{
			return (( 1 << WP_STUN_BATON));
		}
		//Stormtroopers, etc.
		return ( 1 << WP_BLASTER);
		break;

	case NPCTEAM_PLAYER:

		if(spawnflags & SFB_RIFLEMAN)
			return ( 1 << WP_REPEATER);

		if(spawnflags & SFB_PHASER)
			//return ( 1 << WP_BLASTER_PISTOL);
			return ( 1 << WP_BLASTER);

		if ( Q_strncmp( "jedi", NPC_type, 4 ) == 0 || Q_stricmp( "luke", NPC_type ) == 0 )
			return ( 1 << WP_SABER);

		if ( Q_strncmp( "prisoner", NPC_type, 8 ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_strncmp( "bespincop", NPC_type, 9 ) == 0 )
		{
			//return ( 1 << WP_BLASTER_PISTOL);
			return ( 1 << WP_BLASTER);
		}

		if ( Q_stricmp( "MonMothma", NPC_type ) == 0 )
		{
			return WP_NONE;
		}

		//rebel
		return ( 1 << WP_BLASTER);
		break;

	case NPCTEAM_NEUTRAL:

		if ( Q_stricmp( "mark1", NPC_type ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_stricmp( "mark2", NPC_type ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_strncmp( "ugnaught", NPC_type, 8 ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_stricmp( "bartender", NPC_type ) == 0 )
		{
			return WP_NONE;
		}
		if ( Q_stricmp( "morgankatarn", NPC_type ) == 0 )
		{
			return WP_NONE;
		}

		break;

	// these no longer exist
//	case TEAM_FORGE:
//		return ( 1 << WP_STUN_BATON);
//		break;

//	case TEAM_STASIS:
//		return ( 1 << WP_STUN_BATON);
//		break;

//	case TEAM_PARASITE:
//		break;

//	case TEAM_8472:
//		break;

	default:
		break;
	}

	return WP_NONE;
}
*/

extern void ChangeWeapon( gentity_t *ent, int newWeapon );

/*
-------------------------
NPC_SetWeapons
-------------------------
*/

void NPC_SetWeapons( gentity_t *ent )
{
	int			bestWeap = WP_NONE;
	int			curWeap;
	int			weapons = ent->client->ps.stats[STAT_WEAPONS];//NPC_WeaponsForTeam( (team_t)ent->client->playerTeam, ent->spawnflags, ent->NPC_type );

	// zyk: now it will use weapons set in npc .cfg
	// ent->client->ps.stats[STAT_WEAPONS] = 0;
	for ( curWeap = WP_SABER; curWeap < WP_NUM_WEAPONS; curWeap++ )
	{
		if ( (weapons & ( 1 << curWeap )) )
		{
			ent->client->ps.stats[STAT_WEAPONS] |= ( 1 << curWeap );
//			RegisterItem( FindItemForWeapon( (weapon_t)(curWeap) ) );	//precache the weapon
			//rwwFIXMEFIXME: Precache
			ent->NPC->currentAmmo = ent->client->ps.ammo[weaponData[curWeap].ammoIndex] = 100;//FIXME: max ammo

			if ( bestWeap == WP_SABER )
			{
				// still want to register other weapons -- force saber to be best weap
				continue;
			}

			if ( curWeap == WP_STUN_BATON )
			{
				if ( bestWeap == WP_NONE )
				{// We'll only consider giving Melee since we haven't found anything better yet.
					bestWeap = curWeap;
				}
			}
			else if ( curWeap > bestWeap || bestWeap == WP_STUN_BATON )
			{
				// This will never override saber as best weap.  Also will override WP_STUN_BATON if something better comes later in the list
				bestWeap = curWeap;
			}
		}
	}

	ent->client->ps.weapon = bestWeap;
}

/*
-------------------------
NPC_SpawnEffect

  NOTE:  Make sure any effects called here have their models, tga's and sounds precached in
			CG_RegisterNPCEffects in cg_player.cpp
-------------------------
*/

void NPC_SpawnEffect (gentity_t *ent)
{
}

//--------------------------------------------------------------
// NPC_SetFX_SpawnStates
//
// Set up any special parms for spawn effects
//--------------------------------------------------------------
void NPC_SetFX_SpawnStates( gentity_t *ent )
{
	if ( !(ent->NPC->aiFlags&NPCAI_CUSTOM_GRAVITY) )
	{
		ent->client->ps.gravity = g_gravity.value;
	}
}

/*
================
NPC_SpotWouldTelefrag

================
*/
qboolean NPC_SpotWouldTelefrag( gentity_t *npc )
{
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( npc->r.currentOrigin, npc->r.mins, mins );
	VectorAdd( npc->r.currentOrigin, npc->r.maxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++)
	{
		hit = &g_entities[touch[i]];
		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if (hit->inuse
			&& hit->client
			&& hit->s.number != npc->s.number
			&& (hit->r.contents&MASK_NPCSOLID)
			&& hit->s.number != npc->r.ownerNum
			&& hit->r.ownerNum != npc->s.number)
		{
			return qtrue;
		}

	}

	return qfalse;
}

/*
==================
RP_NpcSetTeam

GalaxyRP: [NPC System] what /npc team does to the NPC or vehicle it is aimed at: the side it is on and the
side it attacks. Nothing else -- whatever its type set up for its team from its .npc file at spawn (a
shadowtrooper's cloak, an ambush) is kept. Also how an npc_spawner's or NPC_Vehicle's "npcteam" key is
applied (NPC_Begin()), so a spawned NPC ends up exactly as /npc team would have left it.
==================
*/
static void RP_NpcSetTeam( gentity_t *npc, int team )
{
	switch ( team )
	{
	case NPCTEAM_PLAYER:
		npc->client->playerTeam = NPCTEAM_PLAYER;
		npc->client->enemyTeam = NPCTEAM_ENEMY;
		break;
	case NPCTEAM_ENEMY:
		npc->client->playerTeam = NPCTEAM_ENEMY;
		npc->client->enemyTeam = NPCTEAM_PLAYER;
		break;
	case NPCTEAM_NEUTRAL:
		npc->client->playerTeam = NPCTEAM_NEUTRAL;
		npc->client->enemyTeam = NPCTEAM_NEUTRAL;
		break;
	default:
		npc->client->playerTeam = NPCTEAM_FREE;
		npc->client->enemyTeam = NPCTEAM_FREE;
		break;
	}
}

//--------------------------------------------------------------
void NPC_Begin (gentity_t *ent)
{
	vec3_t	spawn_origin, spawn_angles;
	gclient_t	*client;
	usercmd_t	ucmd;
	gentity_t	*spawnPoint = NULL;

	memset( &ucmd, 0, sizeof( ucmd ) );

	if ( !(ent->spawnflags & SFB_NOTSOLID) )
	{//No NPCs should telefrag
		if (NPC_SpotWouldTelefrag(ent))
		{
			// GalaxyRP fix: [Quests] the "|| NPC_type is jawa_seller" clause here was zyk's anti-exploit
			// rule, stopping a player spawning a pile of sellers by placing them in solid. The
			// jawa_seller NPC type no longer exists (it went with the zyk_quest_*.npc assets), so the
			// clause could never fire again; the wait < 0 half is untouched.
			if ( ent->wait < 0 )
			{//remove yourself
				G_DebugPrint( WL_DEBUG, "NPC %s could not spawn, firing target3 (%s) and removing self\n", ent->targetname, ent->target3 );
				//Fire off our target3
				G_UseTargets2( ent, ent, ent->target3 );

				//Kill us
				ent->think = G_FreeEntity;
				ent->nextthink = level.time + 100;
			}
			else
			{
				G_DebugPrint( WL_DEBUG, "NPC %s could not spawn, waiting %4.2 secs to try again\n", ent->targetname, ent->wait/1000.0f );
				ent->think = NPC_Begin;
				ent->nextthink = level.time + ent->wait;//try again in half a second
			}
			return;
		}
	}
	//Spawn effect
	NPC_SpawnEffect( ent );

	VectorCopy( ent->client->ps.origin, spawn_origin);
	VectorCopy( ent->s.angles, spawn_angles);
	spawn_angles[YAW] = ent->NPC->desiredYaw;

	client = ent->client;

	// increment the spawncount so the client will detect the respawn
	client->ps.persistant[PERS_SPAWN_COUNT]++;

	client->airOutTime = level.time + 12000;

	client->ps.clientNum = ent->s.number;
	// clear entity values

	if ( ent->health )	// Was health supplied in map
	{
		client->pers.maxHealth = client->ps.stats[STAT_MAX_HEALTH] = ent->health;
	}
	else if ( ent->NPC->stats.health )	// Was health supplied in NPC.cfg?
	{
		/* zyk: npcs now will always use original health
		if ( ent->client->NPC_class != CLASS_REBORN
			&& ent->client->NPC_class != CLASS_SHADOWTROOPER
			//&& ent->client->NPC_class != CLASS_TAVION
			//&& ent->client->NPC_class != CLASS_DESANN
			&& ent->client->NPC_class != CLASS_JEDI )
		{// up everyone except jedi
			ent->NPC->stats.health += ent->NPC->stats.health/4 * g_npcspskill.integer; // 100% on easy, 125% on medium, 150% on hard
		}
		*/

		client->pers.maxHealth = client->ps.stats[STAT_MAX_HEALTH] = ent->NPC->stats.health;
	}
	else
	{
		client->pers.maxHealth = client->ps.stats[STAT_MAX_HEALTH] = 100;
	}

	if ( !Q_stricmp( "rodian", ent->NPC_type ) )
	{//sniper
		//NOTE: this will get overridden by any aim settings in their spawnscripts
		switch ( g_npcspskill.integer )
		{
		case 0:
			ent->NPC->stats.aim = 1;
			break;
		case 1:
			ent->NPC->stats.aim = Q_irand( 2, 3 );
			break;
		case 2:
			ent->NPC->stats.aim = Q_irand( 3, 4 );
			break;
		}
	}
	else if ( ent->client->NPC_class == CLASS_STORMTROOPER
		|| ent->client->NPC_class == CLASS_SWAMPTROOPER
		|| ent->client->NPC_class == CLASS_IMPWORKER
		|| !Q_stricmp( "rodian2", ent->NPC_type ) )
	{//tweak yawspeed for these NPCs based on difficulty
		switch ( g_npcspskill.integer )
		{
		case 0:
			ent->NPC->stats.yawSpeed *= 0.75f;
			if ( ent->client->NPC_class == CLASS_IMPWORKER )
			{
				ent->NPC->stats.aim -= Q_irand( 3, 6 );
			}
			break;
		case 1:
			if ( ent->client->NPC_class == CLASS_IMPWORKER )
			{
				ent->NPC->stats.aim -= Q_irand( 2, 4 );
			}
			break;
		case 2:
			ent->NPC->stats.yawSpeed *= 1.5f;
			if ( ent->client->NPC_class == CLASS_IMPWORKER )
			{
				ent->NPC->stats.aim -= Q_irand( 0, 2 );
			}
			break;
		}
	}
	else if ( ent->client->NPC_class == CLASS_REBORN
		|| ent->client->NPC_class == CLASS_SHADOWTROOPER )
	{
		switch ( g_npcspskill.integer )
		{
		case 1:
			ent->NPC->stats.yawSpeed *= 1.25f;
			break;
		case 2:
			ent->NPC->stats.yawSpeed *= 1.5f;
			break;
		}
	}


	ent->s.groundEntityNum = ENTITYNUM_NONE;
	ent->mass = 10;
	ent->takedamage = qtrue;
	ent->inuse = qtrue;
	ent->classname = "NPC";
//	if ( ent->client->race == RACE_HOLOGRAM )
//	{//can shoot through holograms, but not walk through them
//		ent->contents = CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP|CONTENTS_ITEM;//contents_corspe to make them show up in ID and use traces
//		ent->clipmask = MASK_NPCSOLID;
//	} else
	if(!(ent->spawnflags & SFB_NOTSOLID))
	{
		ent->r.contents = CONTENTS_BODY;
		ent->clipmask = MASK_NPCSOLID;
	}
	else
	{
		ent->r.contents = 0;
		ent->clipmask = MASK_NPCSOLID&~CONTENTS_BODY;
	}
	/*
	if(!ent->client->moveType)//Static?
	{
		ent->client->moveType = MT_RUNJUMP;
	}
	*/
	//rwwFIXMEFIXME: movetype support

	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->client->ps.rocketLockIndex = ENTITYNUM_NONE;
	ent->client->ps.rocketLockTime = 0;

	//visible to player and NPCs
	if ( ent->client->NPC_class != CLASS_R2D2 &&
		ent->client->NPC_class != CLASS_R5D2 &&
		ent->client->NPC_class != CLASS_MOUSE &&
		ent->client->NPC_class != CLASS_GONK &&
		ent->client->NPC_class != CLASS_PROTOCOL )
	{
		ent->flags &= ~FL_NOTARGET;
	}
	ent->s.eFlags &= ~EF_NODRAW;

	NPC_SetFX_SpawnStates( ent );

	//client->ps.friction = 6;
	//rwwFIXMEFIXME: per ent friction?

	if ( ent->client->ps.weapon == WP_NONE )
	{//not set by the NPCs.cfg
		NPC_SetWeapons(ent);
	}
	//select the weapon
	ent->NPC->currentAmmo = ent->client->ps.ammo[weaponData[ent->client->ps.weapon].ammoIndex];
	ent->client->ps.weaponstate = WEAPON_IDLE;
	ChangeWeapon( ent, ent->client->ps.weapon );

	VectorCopy( spawn_origin, client->ps.origin );

	// the respawned flag will be cleared after the attack and jump keys come up
	client->ps.pm_flags |= PMF_RESPAWNED;

	// clear entity state values
	//ent->s.eType = ET_PLAYER;
	ent->s.eType = ET_NPC;
//	ent->s.skinNum = ent - g_entities - 1;	// used as index to get custom models

	VectorCopy (spawn_origin, ent->s.origin);
//	ent->s.origin[2] += 1;	// make sure off ground

	SetClientViewAngle( ent, spawn_angles );
	client->renderInfo.lookTarget = ENTITYNUM_NONE;

	if(!(ent->spawnflags & 64))
	{
		G_KillBox( ent );
		trap->LinkEntity ((sharedEntity_t *)ent);
	}

	// don't allow full run speed for a bit
	client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	client->ps.pm_time = 100;

	client->respawnTime = level.time;
	client->inactivityTime = level.time + g_inactivity.value * 1000;
	client->latched_buttons = 0;
	if ( ent->s.m_iVehicleNum )
	{//I'm an NPC in a vehicle (or a vehicle), I already have owner set
	}
	else if ( client->NPC_class == CLASS_SEEKER && ent->activator != NULL )
	{//somebody else "owns" me
		ent->s.owner = ent->r.ownerNum = ent->activator->s.number;
	}
	else
	{
		ent->s.owner = ENTITYNUM_NONE;
	}

	// set default animations
	if ( ent->client->NPC_class != CLASS_VEHICLE )
	{
		NPC_SetAnim( ent, SETANIM_BOTH, BOTH_STAND1, SETANIM_FLAG_NORMAL );
	}

	if( spawnPoint )
	{
		// fire the targets of the spawn point
		G_UseTargets( spawnPoint, ent );
	}

	//ICARUS include
	trap->ICARUS_InitEnt( (sharedEntity_t *)ent );

//==NPC initialization
	SetNPCGlobals( ent );

	ent->enemy = NULL;
	NPCS.NPCInfo->timeOfDeath = 0;
	NPCS.NPCInfo->shotTime = 0;
	NPC_ClearGoal();
	NPC_ChangeWeapon( ent->client->ps.weapon );

//==Final NPC initialization
	ent->pain  = NPC_PainFunc( ent ); //painF_NPC_Pain;
	ent->touch = NPC_TouchFunc( ent ); //touchF_NPC_Touch;
//	ent->NPC->side = 1;

	ent->client->ps.ping = ent->NPC->stats.reactions * 50;

	//MCG - Begin: NPC hacks
	//FIXME: Set the team correctly
	if (ent->s.NPC_class != CLASS_VEHICLE || level.gametype != GT_SIEGE)
	{
		ent->client->ps.persistant[PERS_TEAM] = ent->client->playerTeam;
	}

	ent->use   = NPC_Use;
	ent->think = NPC_Think;
	ent->nextthink = level.time + FRAMETIME + Q_irand(0, 100);

	NPC_SetMiscDefaultData( ent );

	// GalaxyRP: [Entity System] the team its type gave it, for /entsave (gentity_t::rpBaseTeam)
	ent->rpBaseTeam = RP_TEAM_RECORD( ent->client->playerTeam, ent->client->enemyTeam );

	// GalaxyRP: [Entity System] the spawner's "npcteam" (RP_NpcTeamFromName()), which NPC_Spawn_Do() copied
	// here: applied after the defaults above, so the NPC or vehicle is left as /npc team would leave it.
	if ( ent->rpSpawnerTeam > 0 )
	{
		RP_NpcSetTeam( ent, ent->rpSpawnerTeam - 1 );
		ent->rpSpawnerTeam = 0;
	}
	if ( ent->health <= 0 )
	{
		//ORIGINAL ID: health will count down towards max_health
		ent->health = client->ps.stats[STAT_HEALTH] = ent->client->pers.maxHealth;
	}
	else
	{
		client->ps.stats[STAT_HEALTH] = ent->health;
	}

	if (ent->s.shouldtarget)
	{
		ent->maxHealth = ent->health;
		G_ScaleNetHealth(ent);
	}

	ChangeWeapon( ent, ent->client->ps.weapon );//yes, again... sigh

	if ( !(ent->spawnflags & SFB_STARTINSOLID) )
	{//Not okay to start in solid
		G_CheckInSolid( ent, qtrue );
	}
	VectorClear( ent->NPC->lastClearOrigin );

	//Run a script if you have one assigned to you
	if ( G_ActivateBehavior( ent, BSET_SPAWN ) )
	{
		trap->ICARUS_MaintainTaskManager(ent->s.number);
	}

	VectorCopy( ent->r.currentOrigin, ent->client->renderInfo.eyePoint );

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	memset( &ucmd, 0, sizeof( ucmd ) );
	//_VectorCopy( client->pers.cmd_angles, ucmd.angles );
	VectorCopyM(client->pers.cmd.angles, ucmd.angles);

	ent->client->ps.groundEntityNum = ENTITYNUM_NONE;

	if ( ent->NPC->aiFlags & NPCAI_MATCHPLAYERWEAPON )
	{
		//G_MatchPlayerWeapon( ent );
		//rwwFIXMEFIXME: Use this? Probably doesn't really matter for MP.
	}

	ClientThink( ent->s.number, &ucmd );

	trap->LinkEntity( (sharedEntity_t *)ent );

	if ( ent->client->playerTeam == NPCTEAM_ENEMY )
	{//valid enemy spawned
		if ( !(ent->spawnflags&SFB_CINEMATIC) && ent->NPC->behaviorState != BS_CINEMATIC )
		{//not a cinematic enemy
			if ( g_entities[0].client )
			{
				//g_entities[0].client->sess.missionStats.enemiesSpawned++;
				//rwwFIXMEFIXME: Do something here? And this is a rather strange place to be storing
				//this sort of data.
			}
		}
	}
	ent->waypoint = ent->NPC->homeWp = WAYPOINT_NONE;

	if ( ent->m_pVehicle )
	{//a vehicle
		//check for droidunit
		if ( ent->m_pVehicle->m_iDroidUnitTag != -1 )
		{
			char	*droidNPCType = NULL;
			gentity_t *droidEnt = NULL;
			if ( ent->model2
				&& ent->model2[0] )
			{//specified on the NPC_Vehicle spawner ent
				droidNPCType = ent->model2;
			}
			else if ( ent->m_pVehicle->m_pVehicleInfo->droidNPC
				&& ent->m_pVehicle->m_pVehicleInfo->droidNPC[0] )
			{//specified in the vehicle's .veh file
				droidNPCType = ent->m_pVehicle->m_pVehicleInfo->droidNPC;
			}

			if ( droidNPCType != NULL )
			{
				if ( Q_stricmp( "random", droidNPCType ) == 0
					|| Q_stricmp( "default", droidNPCType ) == 0 )
				{//use default - R2D2 or R5D2
					if ( Q_irand( 0, 1 ) )
					{
						droidNPCType = "r2d2";
					}
					else
					{
						droidNPCType = "r5d2";
					}
				}
				droidEnt = NPC_SpawnType( ent, droidNPCType, NULL, qfalse );
				if ( droidEnt != NULL )
				{
					// DAJ_RP: [NPC Scale] a scaled vehicle's droid is scaled with it -- its own type's scale times the
					// vehicle's, so it fills the socket that grew or shrank with the vehicle (it sits on that socket, and
					// grows round it as the socket does). Within the 1..1023 a scale can be sent in (RP_SENT_SCALE_*), not
					// npcscale's 20..500: it is not an npcscale but follows the vehicle's, a 150 droid on a 500 vehicle being
					// 750. A droid type made bigger in its own .npc file, on a vehicle made big enough, stops at 1023, smaller
					// next to the vehicle than its type would have it. Not kept in rpNpcScale: the droid is not saved, the
					// vehicle makes it again.
					if ( droidEnt->client && ent->client && ent->client->ps.iModelScale > 0 && ent->client->ps.iModelScale != 100 )
					{
						const int own = droidEnt->client->ps.iModelScale > 0 ? droidEnt->client->ps.iModelScale : 100;
						int scale = ( own * ent->client->ps.iModelScale + 50 ) / 100;
						float f;

						if ( scale < RP_SENT_SCALE_MIN )
							scale = RP_SENT_SCALE_MIN;
						else if ( scale > RP_SENT_SCALE_MAX )
							scale = RP_SENT_SCALE_MAX;
						f = scale / 100.0f;
						droidEnt->client->ps.iModelScale = scale;
						VectorSet( droidEnt->modelScale, f, f, f );
					}
					if ( droidEnt->client )
					{
						droidEnt->client->ps.m_iVehicleNum =
							droidEnt->s.m_iVehicleNum =
							//droidEnt->s.otherEntityNum2 =
							droidEnt->s.owner =
							droidEnt->r.ownerNum = ent->s.number;
						ent->m_pVehicle->m_pDroidUnit = (bgEntity_t *)droidEnt;
						//SP way:
						//droidEnt->s.m_iVehicleNum = ent->s.number;
						//droidEnt->owner = ent;
						VectorCopy( ent->r.currentOrigin, droidEnt->s.origin );
						VectorCopy( ent->r.currentOrigin, droidEnt->client->ps.origin );
						G_SetOrigin( droidEnt, droidEnt->s.origin );
						trap->LinkEntity( (sharedEntity_t *)droidEnt );
						VectorCopy( ent->r.currentAngles, droidEnt->s.angles );
						G_SetAngles( droidEnt, droidEnt->s.angles );
						if ( droidEnt->NPC )
						{
							droidEnt->NPC->desiredYaw = droidEnt->s.angles[YAW];
							droidEnt->NPC->desiredPitch = droidEnt->s.angles[PITCH];
						}
						droidEnt->flags |= FL_UNDYING;
					}
					else
					{//wtf?
						G_FreeEntity( droidEnt );
					}
				}
			}
		}
	}
}

gNPC_t *gNPCPtrs[MAX_GENTITIES];

gNPC_t *New_NPC_t(int entNum)
{
	gNPC_t *ptr;

	if (!gNPCPtrs[entNum])
	{
		gNPCPtrs[entNum] = (gNPC_t *)BG_Alloc (sizeof(gNPC_t));
	}

	ptr = gNPCPtrs[entNum];

	if (ptr)
	{
		// clear it...
		//
		memset(ptr, 0, sizeof( *ptr ) );
	}

	return ptr;
}

/*
-------------------------
NPC_StasisSpawn_Go
-------------------------
*/
/*
qboolean NPC_StasisSpawn_Go( gentity_t *ent )
{
	//Setup an owner pointer if we need it
	if VALIDSTRING( ent->ownername )
	{
		ent->parent = G_Find( NULL, FOFS( targetname ), ent->ownername );

		if ( ( ent->parent ) && ( ent->parent->health <= 0 ) )
		{//our spawner thing is broken
			if ( ent->target2 && ent->target2[0] )
			{
				//Fire off our target2
				G_UseTargets2( ent, ent, ent->target2 );

				//Kill us
				ent->e_ThinkFunc = thinkF_G_FreeEntity;
				ent->nextthink = level.time + 100;
			}
			else
			{
				//Try to spawn again in one second
				ent->e_ThinkFunc = thinkF_NPC_Spawn_Go;
				ent->nextthink = level.time + 1000;
			}
			return qfalse;
		}
	}

	//Test for an entity blocking the spawn
	trace_t	tr;
	trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ent->s.number, MASK_NPCSOLID );

	//Can't have anything in the way
	if ( tr.allsolid || tr.startsolid )
	{
		ent->nextthink = level.time + 1000;
		return qfalse;
	}

	return qtrue;
}
*/
void NPC_DefaultScriptFlags( gentity_t *ent )
{
	if ( !ent || !ent->NPC )
	{
		return;
	}
	//Set up default script flags
	ent->NPC->scriptFlags = (SCF_CHASE_ENEMIES|SCF_LOOK_FOR_ENEMIES);
}
/*
-------------------------
NPC_Spawn_Go
-------------------------
*/
extern void G_CreateAnimalNPC( Vehicle_t **pVeh, const char *strAnimalType );
extern void G_CreateSpeederNPC( Vehicle_t **pVeh, const char *strType );
extern void G_CreateWalkerNPC( Vehicle_t **pVeh, const char *strAnimalType );
extern void G_CreateFighterNPC( Vehicle_t **pVeh, const char *strType );
extern void zyk_main_spawn_entity(gentity_t *ent);
gentity_t *NPC_Spawn_Do( gentity_t *ent )
{
	gentity_t	*newent = NULL;
	int			index;
	vec3_t		saveOrg;

/*	//Do extra code for stasis spawners
	if ( Q_stricmp( ent->classname, "NPC_Stasis" ) == 0 )
	{
		if ( NPC_StasisSpawn_Go( ent ) == qfalse )
			return;
	}
*/

	//Test for drop to floor
	if ( ent->spawnflags & NSF_DROP_TO_FLOOR )
	{
		trace_t		tr;
		vec3_t		bottom;

		VectorCopy( ent->r.currentOrigin, saveOrg );
		VectorCopy( ent->r.currentOrigin, bottom );
		bottom[2] = MIN_WORLD_COORD;
		trap->Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, bottom, ent->s.number, MASK_NPCSOLID, qfalse, 0, 0 );
		if ( !tr.allsolid && !tr.startsolid && tr.fraction < 1.0 )
		{
			G_SetOrigin( ent, tr.endpos );
		}
	}

	// GalaxyRP fix: [Entity System] spent already (its last use cleared ent->use): nothing more.
	// A spawn still scheduled on its think when a trigger used up the count went on to take the
	// count below 0 -- past -1, which means no limit at all -- and the spawner never stopped again.
	if ( ent->count != -1 && ent->count <= 0 )
	{
		return NULL;
	}

	// GalaxyRP fix: [Entity System] both refusals below come before the count is spent (further down):
	// a refused spawn used to take a use all the same, and a count-1 spawner refused once was spent --
	// use cleared, never spawned, never freed.
	// GalaxyRP fix: [Entity System] this is the common path for every NPC the mod creates -- the
	// /npc spawn command, an npc_spawner placed by a map, one added with /entadd, one fired by a
	// trigger. The two G_Spawn() calls below (this entity and its tempGoal) are checked here
	// together, because failing between them would leave a half-built NPC. As above, the NULL
	// checks that follow can never fire; G_Spawn() ERR_DROPs rather than returning NULL.
	// GalaxyRP fix: [Entity System] three, not two: an NPC that carries a saber takes a third slot
	// for it on this same frame (WP_SaberInitBladeData -> G_Spawn, unchecked), and its weapon is only
	// known once its parms are parsed, below.
	if ( G_EntitySlotsAvailable( 3 ) == qfalse )
	{
		Com_Printf( S_COLOR_RED "ERROR: NPC spawn refused, only %d entity slots free\n",
			G_FreeEntityCount() );
		G_LogPrintf( "npc_spawner at %s refused: %d entity slots free\n",
			vtos(ent->s.origin), G_FreeEntityCount() );
		return NULL;
	}

	// GalaxyRP: [Entity System] and a ceiling on NPCs alive at once, bodies included (they hold
	// their slots until removed). The entity reserve above keeps the server up; this keeps it
	// playable when a spawner fed by a trigger would otherwise fill the table to the reserve, after
	// which no item, missile or /entadd fits either. Far above what any map keeps alive.
	if ( RP_NPCsAlive() >= RP_NPC_MAX_LIVE )
	{
		Com_Printf( S_COLOR_RED "ERROR: NPC spawn refused, %d NPCs are alive already (limit %d)\n",
			RP_NPCsAlive(), RP_NPC_MAX_LIVE );
		G_LogPrintf( "npc_spawner at %s refused: %d NPCs alive (limit %d)\n",
			vtos(ent->s.origin), RP_NPCsAlive(), RP_NPC_MAX_LIVE );
		return NULL;
	}

	//Check the spawner's count
	if( ent->count != -1 )
	{
		ent->count--;

		if( ent->count <= 0 )
		{
			ent->use = 0;//never again
			//FIXME: why not remove me...?  Because of all the string pointers?  Just do G_NewStrings?
		}
	}

	newent = G_Spawn();

	if ( newent == NULL )
	{
		Com_Printf ( S_COLOR_RED "ERROR: NPC G_Spawn failed\n" );
		return NULL;
	}

	newent->fullName = ent->fullName;

	newent->NPC = New_NPC_t(newent->s.number);
	if ( newent->NPC == NULL )
	{
		Com_Printf ( S_COLOR_RED "ERROR: NPC G_Alloc NPC failed\n" );
		goto finish;
	//	return NULL;
	}

	//newent->client = (gclient_s *)G_Alloc (sizeof(gclient_s));
	G_CreateFakeClient(newent->s.number, &newent->client);

	newent->NPC->tempGoal = G_Spawn();

	if ( newent->NPC->tempGoal == NULL )
	{
		newent->NPC = NULL;
		goto finish;
	//	return NULL;
	}

	newent->NPC->tempGoal->classname = "NPC_goal";
	newent->NPC->tempGoal->parent = newent;
	newent->NPC->tempGoal->r.svFlags |= SVF_NOCLIENT;

	if ( newent->client == NULL )
	{
		Com_Printf ( S_COLOR_RED "ERROR: NPC BG_Alloc client failed\n" );
		goto finish;
	//	return NULL;
	}

	memset ( newent->client, 0, sizeof(*newent->client) );

	//Assign the pointer for bg entity access
	newent->playerState = &newent->client->ps;

//==NPC_Connect( newent, net_name );===================================

	if ( ent->NPC_type == NULL )
	{
		ent->NPC_type = "random";
	}
	else if ( RP_HasUpper( ent->NPC_type ) )
	{
		// GalaxyRP fix: [Entity System] this copied and lowercased the type on every spawn, from a pool
		// that is never freed -- a "respawn 1" spawner paid for it with every death. The copy is kept
		// on the spawner, so once it is lowercase (the first spawn, or a type given lowercase) there is
		// nothing to copy. The copy itself stays: the type may be a string literal (SP_NPC_Kyle and the
		// like), which cannot be lowercased in place.
		ent->NPC_type = Q_strlwr( G_NewString( ent->NPC_type ) );
	}

	if ( ent->r.svFlags & SVF_NO_BASIC_SOUNDS )
	{
		newent->r.svFlags |= SVF_NO_BASIC_SOUNDS;
	}
	if ( ent->r.svFlags & SVF_NO_COMBAT_SOUNDS )
	{
		newent->r.svFlags |= SVF_NO_COMBAT_SOUNDS;
	}
	if ( ent->r.svFlags & SVF_NO_EXTRA_SOUNDS )
	{
		newent->r.svFlags |= SVF_NO_EXTRA_SOUNDS;
	}

	if ( ent->message )
	{//has a key
		newent->message = ent->message;//transfer the key name
		newent->flags |= FL_NO_KNOCKBACK;//don't fall off ledges
		G_SoundIndex( "sound/weapons/key_pkup.wav" );
	}

	// If this is a vehicle we need to see what kind it is so we properlly allocate it.
	if ( Q_stricmp( ent->classname, "NPC_Vehicle" ) == 0 )
	{
		// Get the vehicle entry index.
		int iVehIndex = BG_VehicleGetIndex( ent->NPC_type );

 		if ( iVehIndex == VEHICLE_NONE )
		{
			G_FreeEntity( newent );
			//get rid of the spawner, too, I guess
			G_FreeEntity( ent );
			return NULL;
		}
		// NOTE: If you change/add any of these, update NPC_Spawn_f for the new vehicle you
		// want to be able to spawn in manually.

		// See what kind of vehicle this is and allocate it properly.
		switch( g_vehicleInfo[iVehIndex].type )
		{
			case VH_ANIMAL:
				// Create the animal (making sure all it's data is initialized).
				G_CreateAnimalNPC( &newent->m_pVehicle, ent->NPC_type );
				break;
			case VH_SPEEDER:
				// Create the speeder (making sure all it's data is initialized).
				G_CreateSpeederNPC( &newent->m_pVehicle, ent->NPC_type );
				break;
			case VH_FIGHTER:
				// Create the fighter (making sure all it's data is initialized).
				G_CreateFighterNPC( &newent->m_pVehicle, ent->NPC_type );
				break;
			case VH_WALKER:
				// Create the walker (making sure all it's data is initialized).
				G_CreateWalkerNPC( &newent->m_pVehicle, ent->NPC_type );
				break;

			default:
				Com_Printf ( S_COLOR_RED "ERROR: Couldn't spawn NPC %s\n", ent->NPC_type );
				G_FreeEntity( newent );
				//get rid of the spawner, too, I guess
				G_FreeEntity( ent );
				return NULL;
		}

		assert(newent->m_pVehicle &&
			newent->m_pVehicle->m_pVehicleInfo &&
			newent->m_pVehicle->m_pVehicleInfo->Initialize);

		//set up my happy prediction hack
		newent->m_pVehicle->m_vOrientation = &newent->client->ps.vehOrientation[0];

		// Setup the vehicle.
		newent->m_pVehicle->m_pParentEntity = (bgEntity_t *)newent;
		newent->m_pVehicle->m_pVehicleInfo->Initialize( newent->m_pVehicle );

		//cache all the assets
		newent->m_pVehicle->m_pVehicleInfo->RegisterAssets( newent->m_pVehicle );
		//set the class
		newent->client->NPC_class = CLASS_VEHICLE;
		if ( g_vehicleInfo[iVehIndex].type == VH_FIGHTER )
		{//FIXME: EXTERN!!!
			newent->flags |= (FL_NO_KNOCKBACK|FL_SHIELDED|FL_DMG_BY_HEAVY_WEAP_ONLY);//don't get pushed around, blasters bounce off, only damage from heavy weaps
		}
		//WTF?!!! Ships spawning in pointing straight down!
		//set them up to start landed
		newent->m_pVehicle->m_vOrientation[YAW] = ent->s.angles[YAW];
		newent->m_pVehicle->m_vOrientation[PITCH] = newent->m_pVehicle->m_vOrientation[ROLL] = 0.0f;
		G_SetAngles( newent, newent->m_pVehicle->m_vOrientation );
		SetClientViewAngle( newent, newent->m_pVehicle->m_vOrientation );

		//newent->m_pVehicle->m_ulFlags |= VEH_GEARSOPEN;
		//why? this would just make it so the initial anim never got played... -rww
		//There was no initial anim, it would just open the gear even though it's already on the ground (fixed now, made an initial anim)

		//For SUSPEND spawnflag, the amount of time to drop like a rock after SUSPEND turns off
		newent->fly_sound_debounce_time = ent->fly_sound_debounce_time;

		//for no-pilot-death delay
		newent->damage = ent->damage;

		//no-pilot-death distance
		newent->speed = ent->speed;

		//for veh transfer all healy stuff
		newent->healingclass = ent->healingclass;
		newent->healingsound = ent->healingsound;
		newent->healingrate = ent->healingrate;
		newent->model2 = ent->model2;//for droidNPC
	}
	else
	{
		newent->client->ps.weapon = WP_NONE;//init for later check in NPC_Begin
	}

	VectorCopy(ent->s.origin, newent->s.origin);
	VectorCopy(ent->s.origin, newent->client->ps.origin);
	VectorCopy(ent->s.origin, newent->r.currentOrigin);
	G_SetOrigin(newent, ent->s.origin);//just to be sure!
	//NOTE: on vehicles, anything in the .npc file will STOMP data on the NPC that's set by the vehicle
	if ( !NPC_ParseParms( ent->NPC_type, newent ) )
	{
		Com_Printf ( S_COLOR_RED "ERROR: Couldn't spawn NPC %s\n", ent->NPC_type );
		G_FreeEntity( newent );
		//get rid of the spawner, too, I guess
		G_FreeEntity( ent );
		return NULL;
	}

	if ( ent->NPC_type )
	{
		if ( !Q_stricmp( ent->NPC_type, "kyle" ) )
		{//FIXME: "player", not Kyle?  Or check NPC_type against player's NPC_type?
			newent->NPC->aiFlags |= NPCAI_MATCHPLAYERWEAPON;
		}
		else if ( !Q_stricmp( ent->NPC_type, "test" ) )
		{
			int	n;
			for ( n = 0; n < 1 ; n++)
			{
				if ( g_entities[n].s.eType != ET_NPC && g_entities[n].client)
				{
					VectorCopy(g_entities[n].s.origin, newent->s.origin);
					newent->client->playerTeam = newent->s.teamowner = g_entities[n].client->playerTeam;
					break;
				}
			}
			newent->NPC->defaultBehavior = newent->NPC->behaviorState = BS_WAIT;
			newent->classname = "NPC";
	//		newent->r.svFlags |= SVF_NOPUSH;
		}
	}
//=====================================================================
	//set the info we want
	if ( !newent->health )
	{
		newent->health = ent->health;
	}
	newent->script_targetname = ent->NPC_targetname;
	newent->targetname = ent->NPC_targetname;
	newent->target = ent->NPC_target;//death
	newent->target2 = ent->target2;//knocked out death
	newent->target3 = ent->target3;//???
	newent->target4 = ent->target4;//ffire death
	newent->wait = ent->wait;

	for( index = BSET_FIRST; index < NUM_BSETS; index++)
	{
		if ( ent->behaviorSet[index] )
		{
			newent->behaviorSet[index] = ent->behaviorSet[index];
		}
	}

	newent->classname = "NPC";
	newent->NPC_type = ent->NPC_type;

	// GalaxyRP fix: [Entity System] remember which spawner made this NPC, so /entsave can tell an NPC
	// its spawner will recreate from one that has to be written out itself. See gentity_t::zyk_spawner_id.
	if ( !ent->zyk_spawner_id )
	{
		ent->zyk_spawner_id = ++level.zyk_next_spawner_id;
	}
	newent->zyk_npc_spawner = ent;
	newent->zyk_npc_spawner_id = ent->zyk_spawner_id;

	trap->UnlinkEntity((sharedEntity_t *)newent);

	VectorCopy(ent->s.angles, newent->s.angles);
	VectorCopy(ent->s.angles, newent->r.currentAngles);
	VectorCopy(ent->s.angles, newent->client->ps.viewangles);
	newent->NPC->desiredYaw =ent->s.angles[YAW];

	trap->LinkEntity((sharedEntity_t *)newent);
	newent->spawnflags = ent->spawnflags;

	if(ent->paintarget)
	{	//safe to point at owner's string since memory is never freed during game
		newent->paintarget = ent->paintarget;
	}
	if(ent->opentarget)
	{
		newent->opentarget = ent->opentarget;
	}

//==New stuff=====================================================================
	newent->s.eType	= ET_NPC;//ET_PLAYER;

	//FIXME: Call CopyParms
	if ( ent->parms )
	{
		int parmNum;

		for ( parmNum = 0; parmNum < MAX_PARMS; parmNum++ )
		{
			if ( ent->parms->parm[parmNum][0] )
			{
				Q3_SetParm( newent->s.number, parmNum, ent->parms->parm[parmNum] );
			}
		}
	}
	//FIXME: copy cameraGroup, store mine in message or other string field

	//set origin
	newent->s.pos.trType = TR_INTERPOLATE;
	newent->s.pos.trTime = level.time;
	VectorCopy( newent->r.currentOrigin, newent->s.pos.trBase );
	VectorClear( newent->s.pos.trDelta );
	newent->s.pos.trDuration = 0;
	//set angles
	newent->s.apos.trType = TR_INTERPOLATE;
	newent->s.apos.trTime = level.time;
	//VectorCopy( newent->r.currentOrigin, newent->s.apos.trBase );
	//Why was the origin being used as angles? Typo I'm assuming -rww
	VectorCopy( newent->s.angles, newent->s.apos.trBase );

	VectorClear( newent->s.apos.trDelta );
	newent->s.apos.trDuration = 0;

	newent->NPC->combatPoint = -1;

	newent->flags |= FL_NOTARGET;//So he's ignored until he's fully spawned
	newent->s.eFlags |= EF_NODRAW;//So he's ignored until he's fully spawned

	newent->think = NPC_Begin;
	newent->nextthink = level.time + FRAMETIME;
	NPC_DefaultScriptFlags( newent );

	//copy over team variables, too
	newent->s.shouldtarget = ent->s.shouldtarget;
	newent->s.teamowner = ent->s.teamowner;
	newent->alliedTeam = ent->alliedTeam;
	newent->teamnodmg = ent->teamnodmg;
	if ( ent->team && ent->team[0] )
	{//specified team directly?
		newent->client->sess.sessionTeam = atoi(ent->team);
	}
	else if ( newent->s.teamowner != TEAM_FREE )
	{
		newent->client->sess.sessionTeam = newent->s.teamowner;
	}
	else if ( newent->alliedTeam != TEAM_FREE )
	{
		newent->client->sess.sessionTeam = newent->alliedTeam;
	}
	else if ( newent->teamnodmg != TEAM_FREE )
	{
		newent->client->sess.sessionTeam = newent->teamnodmg;
	}
	else
	{
		newent->client->sess.sessionTeam = TEAM_FREE;
	}
	newent->client->ps.persistant[PERS_TEAM] = newent->client->sess.sessionTeam;

	// GalaxyRP: [Entity System] an npc_spawner's "npceffect": the NPC starts with that /npc effect mode,
	// set here rather than in NPC_Begin() because the spawner may be gone by then (its last use frees it).
	// Nothing after this resets client->pers. With the NOTSOLID spawnflag as well, NPC_Begin() still gives
	// the NPC contents 0, and that wins: the phase code only ever changes a CONTENTS_BODY's contents
	// (RP_PhaseHideBody, g_active.c), so it stays untouchable, holo and ghost still showing their look.
	// /npc effect clear takes both off, as it does for any NPC. A vehicle spawner never has one.
	if ( ent->rpSpawnerEffect > RP_PHASE_NONE && newent->client->NPC_class != CLASS_VEHICLE )
	{
		newent->client->pers.phase_mode = ent->rpSpawnerEffect;
		newent->client->pers.phase_releasing = qfalse;
		RP_PhaseTrackNpc( newent );
	}

	// DAJ_RP: [NPC System] an npc_spawner's "npcfreeze": the NPC starts under /npc freeze. Set here for the
	// same reason as npceffect above, and before NPC_Begin() starts its spawnscript, so the script waits for
	// the NPC to be unfrozen like any other. A vehicle spawner never has one (SP_NPC_Vehicle() does not read it).
	if ( ent->rpSpawnerFreeze && newent->NPC && newent->client->NPC_class != CLASS_VEHICLE )
	{
		RP_NpcFreeze( newent );
	}

	// GalaxyRP: [Entity System] an npc_spawner's or NPC_Vehicle's "npcteam": carried on the new NPC or
	// vehicle, since the spawner may be gone by the time NPC_Begin() applies it after its type's defaults.
	newent->rpSpawnerTeam = ent->rpSpawnerTeam;

	// DAJ_RP: [NPC Scale] the spawner's "npcscale" (RP_SpawnerScaleKey()), after NPC_ParseParms() so it takes over from
	// the .npc file's own scale -- 100 included, which makes a scaled type its normal size. Both halves, as that scale
	// does: the size sent to the clients (they draw it) and the server's Ghoul2 scale (hit detection, saber and rider
	// bolts). The box is not scaled, as it is not for a .npc scale or /scale either.
	newent->rpNpcScale = ent->rpNpcScale;
	if ( newent->rpNpcScale > 0 )
	{
		const float scale = newent->rpNpcScale / 100.0f;

		newent->client->ps.iModelScale = newent->rpNpcScale;
		VectorSet( newent->modelScale, scale, scale, scale );
	}

	// DAJ_RP: [NPC Rewards] the spawner's "npccredits" and "npcxp": what this NPC's killer is paid. (The old
	// spawnflags 32768 + genericvalue7 credits are not honoured any more -- nothing ever paid them.)
	if ( newent->client->NPC_class != CLASS_VEHICLE )
	{
		newent->rpRewardCredits = ent->rpRewardCredits;
		newent->rpRewardXP = ent->rpRewardXP;
	}
	else
	{
		newent->rpRewardCredits = 0;
		newent->rpRewardXP = 0;
	}

	if ( !newent->message && newent->client->NPC_class == CLASS_IMPERIAL )
	{ // GalaxyRP: [SP Maps] the imperial model carries a key by default; only a key officer keeps it (single player did this too)
		NPC_SetSurfaceOnOff( newent, "l_arm_key", 0x00000100 /*TURN_OFF*/ );
	}

	trap->LinkEntity ((sharedEntity_t *)newent);

	if(!ent->use)
	{
		if( ent->target )
		{//use any target we're pointed at
			G_UseTargets ( ent, ent );
		}
		if(ent->closetarget)
		{//last guy should fire this target when he dies
			newent->target = ent->closetarget;
		}
		ent->targetname = NULL;
		//why not remove me...?  Because of all the string pointers?  Just do G_NewStrings?
		G_FreeEntity( ent );//bye!
	}

finish:
	if ( ent->spawnflags & NSF_DROP_TO_FLOOR )
	{
		G_SetOrigin( ent, saveOrg );
	}

	// zyk: added some attributes initialization
	if (newent && newent->client)
	{
		newent->client->sess.amrpgmode = 0;
		newent->client->pers.being_mind_controlled = -1;
		newent->client->pers.mind_controlled1_id = -1;
		// GalaxyRP fix: [Guardian] guardian_invoked_by_id/guardian_mode resets removed;
		// spawn_boss() (their only setter) was deleted as unreachable dead code, so
		// these fields are permanently 0/-1 and are being removed from g_local.h.
		newent->client->pers.player_statuses = 0;
		// GalaxyRP fix: [Quests] universe_quest_objective_control reset removed here — field removed as dead (see g_local.h)
		// GalaxyRP fix: [Quests] universe_quest_artifact_holder_id init removed -- -1 was the only value
		// ever assigned to it anywhere, while both its readers tested != -1, so neither could ever fire;
		// the field is gone from clientPersistant_t now (see g_local.h).
		// GalaxyRP fix: [Shop] seller_invoked_by_id initializer removed along with the field itself --
		// it only supported the now-removed /callseller command (see g_cmds.c).
		newent->client->pers.stun_baton_less_speed_timer = 0;

		newent->client->sess.ally1 = 0;
		newent->client->sess.ally2 = 0;

		newent->client->cloakDebReduce = 0;

		// zyk: saboteur npcs start with cloak
		if (Q_stristr(newent->NPC_type,"saboteur"))
			Jedi_Cloak(newent);
	}

	// GalaxyRP: [Entity System] a respawn that came while this spawn was being counted down to waited for
	// it (RP_SpawnerRespawn()): its turn now -- next frame, through the spawner's think, so that a shy
	// spawner with no delay does not spawn the rest from inside this spawn, one inside the other. Only after
	// a spawn that worked, of a spawner still here and not counting down to another already.
	if ( newent && newent->inuse && newent->client && newent->NPC && ent->inuse && ent->use && ent->rpSpawnerQueued > 0 &&
		ent->nextthink <= 0 )
	{
		ent->think = RP_SpawnerQueuedFire;
		ent->nextthink = level.time + FRAMETIME;
	}

	return newent;
}

void NPC_Spawn_Go(gentity_t *ent)
{
	NPC_Spawn_Do(ent);
}

/*
-------------------------
NPC_StasisSpawnEffect
-------------------------
*/
/*
void NPC_StasisSpawnEffect( gentity_t *ent )
{
	vec3_t		start, end, forward;
	qboolean	taper;

	//Floor or wall?
	if ( ent->spawnflags & 1 )
	{
		AngleVectors( ent->s.angles, forward, NULL, NULL );
		VectorMA( ent->r.currentOrigin,  24, forward, end );
		VectorMA( ent->r.currentOrigin, -20, forward, start );

		start[2] += 64;

		taper = qtrue;
	}
	else
	{
		VectorCopy( ent->r.currentOrigin, start );
		VectorCopy( start, end );
		end[2] += 48;
		taper = qfalse;
	}

	//Add the effect
//	CG_ShimmeryThing_Spawner( start, end, 32, qtrue, 1000 );
}
*/
/*
-------------------------
NPC_ShySpawn
-------------------------
*/

#define SHY_THINK_TIME			1000
#define SHY_SPAWN_DISTANCE		128
#define SHY_SPAWN_DISTANCE_SQR	( SHY_SPAWN_DISTANCE * SHY_SPAWN_DISTANCE )

void NPC_ShySpawn( gentity_t *ent )
{
	ent->nextthink = level.time + SHY_THINK_TIME;
	ent->think = NPC_ShySpawn;

	// GalaxyRP fix: [NPC] shy spawns waited for g_entities[0] only; wait until no player is near or watching
	if ( DistanceToClosestPlayer( ent->r.currentOrigin, -1 ) <= SHY_SPAWN_DISTANCE )
		return;

	if ( InPlayersFOV( ent->r.currentOrigin, -1, 80, 64, qtrue ) ) // FIXME: hardcoded fov
		return;

	ent->think = 0;
	ent->nextthink = 0;

	NPC_Spawn_Go( ent );
}

/*
-------------------------
NPC_Spawn
-------------------------
*/

void NPC_Spawn ( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	//delay before spawning NPC
	if( ent->delay )
	{
/*		//Stasis does an extra step
		if ( Q_stricmp( ent->classname, "NPC_Stasis" ) == 0 )
		{
			if ( NPC_StasisSpawn_Go( ent ) == qfalse )
				return;
		}
*/
		if ( ent->spawnflags & 2048 )  // SHY
		{
			ent->think = NPC_ShySpawn;
		}
		else
		{
			ent->think = NPC_Spawn_Go;
		}

		ent->nextthink = level.time + ent->delay;
	}
	else
	{
		if ( ent->spawnflags & 2048 )  // SHY
		{
			NPC_ShySpawn( ent );
		}
		else
		{
			NPC_Spawn_Do( ent );
		}
	}
}

/*
===========================================================================
GalaxyRP: [Entity System] NPC and vehicle spawners

Every class whose name starts with "npc_" is one: npc_spawner, NPC_Vehicle, and the map classes
(NPC_Stormtrooper, ...) whose spawn functions end in SP_NPC_spawner(). /entadd and /entaddaim refuse one
without a targetname (zyk_entadd() in g_cmds.c), and two keys of their own apply to a spawner that has one:

- "spawnnow 1": it also spawns one at once -- when it is placed, and again whenever it is spawned
  anew (an entity file loaded, the map restarted) -- not counted against its "count", and without its
  delay or shyness. Spawned again in place (/entedit, /entcut, /entrotate), it does so only when no NPC
  it made is alive, or each edit would add one.
- "respawn 1": when an NPC or vehicle it made dies, it is fired again, as a trigger would fire it --
  its delay, its shyness and one of its count. A fire that comes while it is already counting down to a
  spawn would replace that countdown (an entity has one think), so it waits its turn instead
  (rpSpawnerQueued) and is fired once that spawn has happened.

Which spawner made an NPC is NPC_Spawn_Do()'s link (gentity_t::zyk_npc_spawner and its id).
===========================================================================
*/
extern void G_VehicleSpawn( gentity_t *self );

// GalaxyRP: [Entity System] NPCs (vehicles included) in the networked region, alive or still lying
// there as bodies -- what NPC_Spawn_Do() measures against RP_NPC_MAX_LIVE. A scan: NPC spawns are
// rare next to frames, and a count kept by hand would drift the first time a free path forgot it.
// GalaxyRP: [Entity System] whether an NPC type still has an uppercase letter to lowercase (NPC_Spawn_Do)
qboolean RP_HasUpper( const char *s )
{
	for ( ; s && *s; s++ )
	{
		if ( *s >= 'A' && *s <= 'Z' )
		{
			return qtrue;
		}
	}
	return qfalse;
}

// GalaxyRP: [Entity System] a spawner's "delay" as its key gave it (scale 1000 when the key is in seconds,
// 1 when it is in milliseconds already), kept within [0, RP_SPAWNER_DELAY_MAX] ms before it is scaled --
// the multiply overflowed past 2147483 s and fired the spawner at once. And its "count": -1 (no limit)
// or 1 to RP_SPAWNER_COUNT_MAX; any other negative means no limit too.
void RP_SpawnerClampDelay( gentity_t *self, int scale )
{
	const int max = RP_SPAWNER_DELAY_MAX / ( scale > 0 ? scale : 1 );

	if ( self->delay < 0 || self->delay > max )
	{
		G_LogPrintf( "%s at %s: delay %d clamped to %d\n", self->classname ? self->classname : "spawner",
			vtos( self->s.origin ), self->delay, self->delay < 0 ? 0 : max );
		self->delay = self->delay < 0 ? 0 : max;
	}
	self->delay *= ( scale > 0 ? scale : 1 );
}

void RP_SpawnerClampCount( gentity_t *self )
{
	if ( self->count < -1 || self->count > RP_SPAWNER_COUNT_MAX )
	{
		G_LogPrintf( "%s at %s: count %d clamped to %d\n", self->classname ? self->classname : "spawner",
			vtos( self->s.origin ), self->count, self->count < -1 ? -1 : RP_SPAWNER_COUNT_MAX );
		self->count = self->count < -1 ? -1 : RP_SPAWNER_COUNT_MAX;
	}
}

int RP_NPCsAlive( void )
{
	int i, count = 0;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		if ( g_entities[i].inuse && g_entities[i].s.eType == ET_NPC && g_entities[i].client )
		{
			count++;
		}
	}

	return count;
}

qboolean RP_IsNpcSpawnerClass( const char *classname )
{
	return ( classname && !Q_stricmpn( classname, "npc_", 4 ) ) ? qtrue : qfalse;
}

// the spawner made this NPC or vehicle, and it is still that spawner (a freed and reused slot is
// zeroed, so its id cannot match)
qboolean RP_SpawnerMadeIt( const gentity_t *spawner, const gentity_t *npc )
{
	return ( spawner && npc && spawner != npc && spawner->inuse && npc->zyk_npc_spawner == spawner &&
		npc->zyk_npc_spawner_id != 0 && spawner->zyk_spawner_id == npc->zyk_npc_spawner_id ) ? qtrue : qfalse;
}

// alive: not dead, and not a vehicle already counting down to its explosion -- as /entsave tells them. One
// just spawned has no health until NPC_Begin() runs, the next frame: it is alive.
static qboolean RP_SpawnedAlive( const gentity_t *npc )
{
	if ( !npc->inuse || !npc->client || !npc->NPC || ( npc->s.eFlags & EF_DEAD ) || npc->client->ps.pm_type == PM_DEAD ||
		( npc->m_pVehicle && npc->m_pVehicle->m_iDieTime != 0 ) )
	{
		return qfalse;
	}
	if ( npc->health <= 0 && npc->think != NPC_Begin )
	{
		return qfalse;
	}
	return qtrue;
}

// an NPC or vehicle this spawner made that is still alive, or NULL
gentity_t *RP_SpawnerLiveChild( const gentity_t *spawner )
{
	int i;

	if ( !spawner || !spawner->inuse || !spawner->zyk_spawner_id )
	{
		return NULL;
	}
	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		gentity_t *e = &g_entities[i];

		if ( RP_SpawnedAlive( e ) && RP_SpawnerMadeIt( spawner, e ) )
		{
			return e;
		}
	}
	return NULL;
}

// "spawnnow": one at once, not counted, without the delay or the shyness
static void RP_SpawnerSpawnNow( gentity_t *ent )
{
	ent->think = NULL;
	ent->nextthink = 0;

	if ( !ent->use || RP_SpawnerLiveChild( ent ) )
	{
		return;
	}
	if ( ent->count != -1 && ent->count < INT_MAX )
	{
		ent->count++;	// NPC_Spawn_Do() takes one off: this one is not one of its uses
	}
	if ( !Q_stricmp( ent->classname, "NPC_Vehicle" ) )
	{
		G_VehicleSpawn( ent );
	}
	else
	{
		NPC_Spawn_Do( ent );
	}
}

// the keys of a spawner with a targetname, read while it spawns (SP_NPC_spawner(), SP_NPC_Vehicle())
static void RP_SpawnerKeys( gentity_t *self )
{
	int now = 0, again = 0;

	G_SpawnInt( "respawn", "0", &again );
	self->rpSpawnerRespawn = again ? qtrue : qfalse;
	if ( !self->rpSpawnerRespawn )
	{
		self->rpSpawnerQueued = 0;	// respawns waiting their turn go with the key
	}

	// spawned again in place (/entedit, /entcut, /entrotate), it may be counting down to a spawn already;
	// that is left to come. A spawn one now that has not come yet is decided again, as it is now.
	if ( self->think == RP_SpawnerSpawnNow )
	{
		self->think = NULL;
		self->nextthink = 0;
	}
	G_SpawnInt( "spawnnow", "0", &now );
	if ( !RP_SpawnerCountingDown( self ) )
	{
		if ( now && !RP_SpawnerLiveChild( self ) )
		{
			self->think = RP_SpawnerSpawnNow;
			self->nextthink = level.time + START_TIME_REMOVE_ENTS + 50;
		}
		else if ( self->rpSpawnerQueued > 0 )
		{
			self->think = RP_SpawnerQueuedFire;	// respawns that waited, while it was held say
			self->nextthink = level.time + FRAMETIME;
		}
	}
}

// the spawner is counting down to a spawn -- its delay, waiting to be unseen, its spawn one now, or a
// waiting respawn's turn -- which a fire now would replace (an entity has one think). Due this very frame
// but not run yet counts: nextthink is 0 once it has run. Also what RP_EntResetForRespawn() (g_spawn.c)
// leaves alone, so a spawn in place keeps the countdown, as RP_SpawnerKeys() expects.
qboolean RP_SpawnerCountingDown( const gentity_t *spawner )
{
	return ( spawner->nextthink > 0 && ( spawner->think == NPC_Spawn_Go || spawner->think == NPC_ShySpawn ||
		spawner->think == G_VehicleSpawn || spawner->think == RP_SpawnerSpawnNow || spawner->think == RP_SpawnerQueuedFire ) ) ? qtrue : qfalse;
}

// a respawn that waited its turn: fired now, unless it is still not its turn
static void RP_SpawnerQueuedFire( gentity_t *ent )
{
	ent->think = NULL;
	ent->nextthink = 0;

	if ( ent->rpSpawnerQueued <= 0 || !ent->use )
	{
		ent->rpSpawnerQueued = 0;
		return;
	}
	if ( ent->rpHeldBy )
	{
		// held with /entcut: when it is let go, it is spawned again in place and takes its turn then
		return;
	}
	ent->rpSpawnerQueued--;
	RP_SpawnerScheduleRespawn( ent );
}

// GalaxyRP fix: [Entity System] the spawn a death asks for, scheduled on the spawner's think -- never sooner
// than RP_SPAWNER_RESPAWN_MIN after the death, and after the spawner's own delay when that is longer. This
// used to fire the spawner's use at once: with no delay the next NPC stood there within the same frame, and
// a spawner placed in a trigger_hurt or over a pit made one NPC per frame for as long as the table allowed,
// each spawn costing pool memory that is never given back. Shy spawners wait to be unseen as before.
static void RP_SpawnerScheduleRespawn( gentity_t *spawner )
{
	const int wait = ( spawner->delay > RP_SPAWNER_RESPAWN_MIN ) ? spawner->delay : RP_SPAWNER_RESPAWN_MIN;

	if ( !Q_stricmp( spawner->classname, "NPC_Vehicle" ) )
	{
		spawner->think = G_VehicleSpawn;
	}
	else if ( spawner->spawnflags & 2048 )  // SHY
	{
		spawner->think = NPC_ShySpawn;
	}
	else
	{
		spawner->think = NPC_Spawn_Go;
	}
	spawner->nextthink = level.time + wait;
}

// player_die(): an NPC or vehicle has died -- its spawner fires again if it has "respawn"
void RP_SpawnerRespawn( gentity_t *npc )
{
	gentity_t *spawner;

	if ( !npc || !npc->NPC || !npc->client )
	{
		return;
	}
	spawner = npc->zyk_npc_spawner;
	if ( !RP_SpawnerMadeIt( spawner, npc ) || !spawner->rpSpawnerRespawn || !spawner->use )
	{
		return;
	}

	// already counting down to a spawn, or held with /entcut (hidden, to be put down elsewhere): this one
	// waits its turn
	if ( RP_SpawnerCountingDown( spawner ) || spawner->rpHeldBy )
	{
		spawner->rpSpawnerQueued++;
		return;
	}
	RP_SpawnerScheduleRespawn( spawner );
}

extern void Rancor_DropVictim( gentity_t *self );

// an NPC or vehicle taken away as BS_REMOVE takes one (NPC_RemoveIfOutOfPlayersPVS()), but at once and firing
// nothing: not drawn, not solid, gone next frame -- with the saber it carries, as the removal of a body frees
// it (NPC.c), and with the droid a vehicle carries
static void RP_SpawnedRemove( gentity_t *e )
{
	int saberNum = e->client->ps.saberEntityNum;

	if ( saberNum > 0 && saberNum < ENTITYNUM_WORLD && g_entities[saberNum].inuse && g_entities[saberNum].r.ownerNum == e->s.number )
	{
		G_FreeEntity( &g_entities[saberNum] );
		e->client->ps.saberEntityNum = 0;
	}
	if ( e->m_pVehicle && e->m_pVehicle->m_pDroidUnit )
	{
		gentity_t *droid = (gentity_t *)e->m_pVehicle->m_pDroidUnit;

		if ( droid != e && droid->inuse && droid->client && droid->NPC && droid->think != G_FreeEntity )
		{
			RP_SpawnedRemove( droid );
		}
	}

	if ( e->s.NPC_class == CLASS_RANCOR )
	{
		Rancor_DropVictim( e );	// as player_die() does: the player it holds goes free
	}

	e->takedamage = qfalse;		// nothing can kill it now, and fire its death targets
	e->s.eFlags |= EF_NODRAW;
	e->s.eType = ET_INVISIBLE;
	e->r.contents = 0;
	e->health = 0;
	e->targetname = NULL;
	e->think = G_FreeEntity;
	e->nextthink = level.time + FRAMETIME;
}

// /entundo of a spawner: the NPCs and vehicles it made that are alive are taken away (RP_SpawnedRemove()) --
// not a vehicle someone rides. Nothing fires: no death, no respawn; its use goes first all the same. Returns
// how many went; *ridden is how many vehicles stayed.
int RP_SpawnerRemoveChildren( gentity_t *spawner, int *ridden )
{
	int i, removed = 0;

	if ( ridden )
	{
		*ridden = 0;
	}
	if ( !spawner || !spawner->inuse )
	{
		return 0;
	}

	spawner->rpSpawnerRespawn = qfalse;
	spawner->rpSpawnerQueued = 0;
	spawner->use = NULL;
	spawner->think = NULL;
	spawner->nextthink = 0;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		gentity_t *e = &g_entities[i];

		if ( !RP_SpawnedAlive( e ) || !RP_SpawnerMadeIt( spawner, e ) )
		{
			continue;
		}
		if ( e->m_pVehicle && ( e->m_pVehicle->m_pPilot || e->m_pVehicle->m_iNumPassengers > 0 ) )
		{
			if ( ridden )
			{
				( *ridden )++;
			}
			continue;
		}
		RP_SpawnedRemove( e );
		removed++;
	}
	return removed;
}

/*
==================
RP_NpcEffectFromName

GalaxyRP: [Entity System] an npc_spawner's "npceffect" key: "holo", "ghost" or "walkthrough" -- the words
/npc effect takes -- gives every NPC it makes that mode from the start (NPC_Spawn_Do()). This is the
walk-through kind of non-solid: the NPC stays a body that is hit and damaged, only movement passes
through it. The NOTSOLID spawnflag (64) is the other kind -- nothing touches it at all -- and the two
can be combined, NOTSOLID winning. Returns the RP_PHASE_* mode, RP_PHASE_NONE for an empty value, and -1
for anything else, which /entadd and /entedit refuse (g_cmds.c) and a map's spawner just ignores.
DAJ_RP: [NPC System] the walk-through mode's word is "walkthrough". It was "nonsolid", which read as the
NOTSOLID spawnflag beside it -- the very confusion above; renamed before any release, so "nonsolid" is
simply not a value any more.
==================
*/
int RP_NpcEffectFromName( const char *name )
{
	if ( !name || !name[0] )
		return RP_PHASE_NONE;
	if ( !Q_stricmp( name, "holo" ) )
		return RP_PHASE_HOLO;
	if ( !Q_stricmp( name, "ghost" ) )
		return RP_PHASE_GHOST;
	if ( !Q_stricmp( name, "walkthrough" ) )
		return RP_PHASE_NONSOLID;
	return -1;
}

/*
==================
RP_NpcFreezeFromValue

DAJ_RP: [NPC System] an npc_spawner's "npcfreeze" key: 1 starts every NPC it makes under /npc freeze
(NPC_Spawn_Do(), RP_NpcFreeze()), 0 or no key does not. Returns 1 or 0, 0 for an empty value, and -1 for
anything else, which /entadd and /entedit refuse (g_cmds.c) and a map's spawner treats as 0.
==================
*/
int RP_NpcFreezeFromValue( const char *value )
{
	if ( !value || !value[0] || !strcmp( value, "0" ) )
		return 0;
	if ( !strcmp( value, "1" ) )
		return 1;
	return -1;
}

static int zyk_team_from_string( const char *name );

/*
==================
RP_NpcTeamFromName

GalaxyRP: [Entity System] an npc_spawner's or NPC_Vehicle's "npcteam" key: player, enemy, neutral or free --
or NPCTEAM_PLAYER and the other full names, as /npc team takes them -- gives every NPC or vehicle it makes
that team, applied as /npc team would apply it (NPC_Begin()). Returns the npcteam_t, -1 for an empty value
(no key) and -2 for anything else, which /entadd and /entedit refuse (g_cmds.c) and a map's spawner ignores.
NPCTEAM_FREE is 0, so neither failure can be 0.
==================
*/
int RP_NpcTeamFromName( const char *name )
{
	int team;

	if ( !name || !name[0] )
		return -1;

	team = zyk_team_from_string( name );

	if ( team != NPCTEAM_PLAYER && team != NPCTEAM_ENEMY && team != NPCTEAM_NEUTRAL && team != NPCTEAM_FREE )
		return -2;

	return team;
}

// GalaxyRP: [Entity System] reads "npcteam" into the spawner, for SP_NPC_spawner() and SP_NPC_Vehicle(): read
// every time, so a spawner spawned again in place (/entedit) without the key loses the team.
static void RP_SpawnerTeamKey( gentity_t *self )
{
	char *teamName = NULL;
	int team;

	G_SpawnString( "npcteam", "", &teamName );
	team = RP_NpcTeamFromName( teamName );
	self->rpSpawnerTeam = ( team >= 0 ) ? team + 1 : 0;
}

/*
==================
RP_SpawnerRewardKey / RP_SpawnerRewardKeys

DAJ_RP: [NPC Rewards] an npc_spawner's "npccredits" and "npcxp": the credits and XP each NPC it makes pays the
logged-in player who kills it (RP_PayNpcKillReward(), g_combat.c). Whole numbers, 0 (or no key) being none;
/entadd and /entedit refuse anything outside 0..RP_REWARD_MAX_CREDITS and 0..RP_REWARD_MAX_XP, and a value
that reaches here another way -- an entity file, a map -- is brought into range and logged. Read every time,
like npcteam, so a spawner spawned again in place without the key loses the reward.
==================
*/
static int RP_SpawnerRewardKey( gentity_t *self, const char *key, int max )
{
	char *text = NULL;
	int value;

	if ( !G_SpawnString( key, "", &text ) || !text || !text[0] )
		return 0;

	value = atoi( text );

	if ( value < 0 || value > max )
	{
		const int fixed = ( value < 0 ) ? 0 : max;

		G_LogPrintf( "%s %d: \"%s\" %s is outside 0..%d, using %d\n", self->classname ? self->classname : "spawner",
			self->s.number, key, RP_ShownText( text ), max, fixed );
		value = fixed;
	}

	return value;
}

static void RP_SpawnerRewardKeys( gentity_t *self )
{
	self->rpRewardCredits = RP_SpawnerRewardKey( self, "npccredits", RP_REWARD_MAX_CREDITS );
	self->rpRewardXP = RP_SpawnerRewardKey( self, "npcxp", RP_REWARD_MAX_XP );
}

/*
==================
RP_NpcScaleValue

DAJ_RP: [NPC Scale] what an "npcscale" value makes the size, in percent: 0 for none (its type's own size), else
RP_NPC_SCALE_MIN..RP_NPC_SCALE_MAX, a whole number outside them brought to the nearer end, however long it is. Strict
(/entadd, /entedit) takes digits only and says -1 for anything else, so "50.5", "-5", " 50" or "abc" are refused, not
read as something; otherwise (an entity file, a map, a preset's one-time NPC line) it reads as atoi() always has here --
leading spaces and a sign, the number up to the first non-digit, no number at all being none -- and a negative one is
below the least, so the least.
==================
*/
int RP_NpcScaleValue( const char *text, qboolean strict )
{
	const char *p = text;
	qboolean negative = qfalse;
	int value = 0, digits = 0;

	if ( !p )
	{
		return 0;
	}

	if ( !strict )
	{
		while ( *p == ' ' || *p == '\t' )
		{
			p++;
		}
		if ( *p == '-' || *p == '+' )
		{
			negative = ( *p == '-' ) ? qtrue : qfalse;
			p++;
		}
	}

	for ( ; *p >= '0' && *p <= '9'; p++, digits++ )
	{
		if ( value <= RP_NPC_SCALE_MAX )
		{
			value = value * 10 + ( *p - '0' );	// past the most it stops growing: it only has to stay over it
		}
	}

	if ( strict && ( !digits || *p ) )
	{
		return -1;
	}

	if ( !value )
	{
		return 0;
	}

	if ( negative || value < RP_NPC_SCALE_MIN )
	{
		return RP_NPC_SCALE_MIN;
	}

	return ( value > RP_NPC_SCALE_MAX ) ? RP_NPC_SCALE_MAX : value;
}

/*
==================
RP_SpawnerScaleKey

DAJ_RP: [NPC Scale] an NPC or vehicle spawner's "npcscale": the size of each NPC or vehicle it makes, in percent (100 is
normal), taking over from its .npc file's own scale (NPC_Spawn_Do()). 0 or no key is none. Brought into
RP_NPC_SCALE_MIN..RP_NPC_SCALE_MAX without a word, however it came -- /entadd and /entedit store it already brought in,
an entity file or a map may not (RP_NpcScaleValue()). Read every time, like npcteam, so a spawner spawned again in place
without the key makes its NPCs their type's size again.
==================
*/
static void RP_SpawnerScaleKey( gentity_t *self )
{
	char *text = NULL;

	self->rpNpcScale = 0;

	if ( !G_SpawnString( "npcscale", "", &text ) || !text || !text[0] )
		return;

	self->rpNpcScale = RP_NpcScaleValue( text, qfalse );
}

/*QUAKED NPC_spawner (1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

NPC_type - name of NPC (in npcs.cfg) to spawn in

targetname - name this NPC goes by for targetting
target - NPC will fire this when it spawns it's last NPC (should this be when the last NPC it spawned dies?)
target2 - Fired by stasis spawners when they try to spawn while their spawner model is broken
target3 - Fired by spawner if they try to spawn and are blocked and have a wait < 0 (removes them)

If targeted, will only spawn a NPC when triggered
count - how many NPCs to spawn (only if targetted) default = 1
delay - how long to wait to spawn after used
wait - if trying to spawn and blocked, how many seconds to wait before trying again (default = 0.5, < 0 = never try again and fire target2)

NPC_targetname - NPC's targetname AND script_targetname
NPC_target - NPC's target to fire when killed
NPC_target2 - NPC's target to fire when knocked out
NPC_target4 - NPC's target to fire when killed by friendly fire
NPC_type - type of NPC ("Borg" (default), "Xian", etc)
health - starting health (default = 100)

spawnscript - default script to run once spawned (none by default)
usescript - default script to run when used (none by default)
awakescript - default script to run once awoken (none by default)
angerscript - default script to run once angered (none by default)
painscript - default script to run when hit (none by default)
fleescript - default script to run when hit and below 50% health (none by default)
deathscript - default script to run when killed (none by default)
These strings can be used to activate behaviors instead of scripts - these are checked
first and so no scripts should be names with these names:
    default - 0: whatever
	idle - 1: Stand around, do abolutely nothing
	roam - 2: Roam around, collect stuff
	walk - 3: Crouch-Walk toward their goals
	run - 4: Run toward their goals
	standshoot - 5: Stay in one spot and shoot- duck when neccesary
	standguard - 6: Wait around for an enemy
	patrol - 7: Follow a path, looking for enemies
	huntkill - 8: Track down enemies and kill them
	evade - 9: Run from enemies
	evadeshoot - 10: Run from enemies, shoot them if they hit you
	runshoot - 11: Run to your goal and shoot enemy when possible
	defend - 12: Defend an entity or spot?
	snipe - 13: Stay hidden, shoot enemy only when have perfect shot and back turned
	combat - 14: Attack, evade, use cover, move about, etc.  Full combat AI - id NPC code
	medic - 15: Go for lowest health buddy, hide and heal him.
	takecover - 16: Find nearest cover from enemies
	getammo - 17: Go get some ammo
	advancefight - 18: Go somewhere and fight along the way
	face - 19: turn until facing desired angles
	wait - 20: do nothing
	formation - 21: Maintain a formation
	crouch - 22: Crouch-walk toward their goals

delay - after spawned or triggered, how many seconds to wait to spawn the NPC

showhealth - set to 1 to show health bar on this entity when crosshair is over it

teamowner - crosshair shows green for this team, red for opposite team
	0 - none
	1 - red
	2 - blue

teamuser - only this team can use this NPC
	0 - none
	1 - red
	2 - blue

teamnodmg - team that NPC does not take damage from (turrets and other auto-defenses that have "alliedTeam" set to this team won't target this NPC)
	0 - none
	1 - red
	2 - blue

"noBasicSounds" - set to 1 to prevent loading and usage of basic sounds (pain, death, etc)
"noCombatSounds" - set to 1 to prevent loading and usage of combat sounds (anger, victory, etc.)
"noExtraSounds" - set to 1 to prevent loading and usage of "extra" sounds (chasing the enemy - detecting them, flanking them... also jedi combat sounds)
*/
extern void NPC_PrecacheAnimationCFG( const char *NPC_type );
void NPC_Precache ( gentity_t *spawner );
void NPC_PrecacheType( char *NPC_type )
{
	gentity_t *fakespawner = G_Spawn();
	if ( fakespawner )
	{
		fakespawner->NPC_type = NPC_type;
		NPC_Precache( fakespawner );
		//NOTE: does the spawner have to stay around to send any precached info to the clients...?
		G_FreeEntity( fakespawner );
	}
}

void SP_NPC_spawner( gentity_t *self)
{
	int t;

	if (!g_allowNPC.integer)
	{
		self->think = G_FreeEntity;
		self->nextthink = level.time;
		return;
	}

	// GalaxyRP fix: [NPC] an unconditional "vjun3 drops protocol_imp and r2d2_imp" block used to sit
	// here, to stay under the 16-entry MAX_ANIM_FILES of the time (128 since 3.47). Both types are on
	// the list below, so vjun3 now behaves like every other SP map: trimmed only when the cvar is on.
	if (rp_sp_npc_fix.integer)
	{ // zyk: removing npcs from SP maps which are not that useful
		if (level.sp_map == qtrue && (Q_stricmp(self->NPC_type, "player") == 0 || Q_stricmp(self->NPC_type, "r2d2_imp") == 0 || Q_stricmp(self->NPC_type, "r2d2") == 0 || 
			 Q_stricmp(self->NPC_type, "protocol_imp") == 0 || Q_stricmp(self->NPC_type, "tavion_new") == 0 || Q_stricmp(self->NPC_type, "dkothos") == 0 || 
			 Q_stricmp(self->NPC_type, "vkothos") == 0 || Q_stricmp(self->NPC_type, "rosh_penin") == 0 || Q_stricmp(self->NPC_type, "rosh_dark") == 0 || 
			 Q_stricmp(self->NPC_type, "rosh_penin_noforce") == 0 || Q_stricmp(self->NPC_type, "tavion_scepter") == 0 || Q_stricmp(self->NPC_type, "tavion_sith_sword") == 0 || 
			 Q_stricmp(self->NPC_type, "kyle") == 0 || Q_stricmp(self->NPC_type, "kyle_boss") == 0 || Q_stricmp(self->NPC_type, "luke") == 0 || 
			 Q_stricmp(self->NPC_type, "chewie") == 0 || Q_stricmp(self->NPC_type, "alora") == 0 || Q_stricmp(self->NPC_type, "alora_dual") == 0 || 
			 Q_stricmp(self->NPC_type, "boba_fett") == 0 || Q_stricmp(self->NPC_type, "saber_droid_training") == 0 || 
			 Q_stricmp(self->NPC_type, "merchant") == 0 || Q_stricmp(self->NPC_type, "elder") == 0 || Q_stricmp(self->NPC_type, "mutant_rancor") == 0 || 
			 Q_stricmp(self->NPC_type, "sand_creature") == 0 || Q_stricmp(self->NPC_type, "sand_creature_fast") == 0 || 
			 Q_stricmp(self->NPC_type, "protocol") == 0 || Q_stricmp(self->NPC_type, "r5d2") == 0 || Q_stricmp(self->NPC_type, "r5d2_imp") == 0 || 
			 Q_stricmp(self->NPC_type, "gonk") == 0))
		{
			self->think = G_FreeEntity;
			self->nextthink = level.time;
			return;
		}
	}

	if ( !self->fullName || !self->fullName[0] )
	{
		//FIXME: make an index into an external string table for localization
		self->fullName = "Humanoid Lifeform";
	}

	//register/precache the models needed for this NPC, not anymore
	//self->classname = "NPC_spawner";

	if(!self->count)
	{
		self->count = 1;
	}
	RP_SpawnerClampCount( self );

	{//Stop loading of certain extra sounds
		static	int	garbage;

		if ( G_SpawnInt( "noBasicSounds", "0", &garbage ) )
		{
			self->r.svFlags |= SVF_NO_BASIC_SOUNDS;
		}
		if ( G_SpawnInt( "noCombatSounds", "0", &garbage ) )
		{
			self->r.svFlags |= SVF_NO_COMBAT_SOUNDS;
		}
		if ( G_SpawnInt( "noExtraSounds", "0", &garbage ) )
		{
			self->r.svFlags |= SVF_NO_EXTRA_SOUNDS;
		}
	}

	// GalaxyRP fix: [Entity System] wait and delay are given in seconds and kept in milliseconds, and only
	// a value just read from the entity's keys is multiplied. Spawned again in place (/entedit, /entcut,
	// /entrotate) a spawner keeps the fields its keys do not set -- already in milliseconds -- while its
	// spawnflags are its keys' again, without the 65536 set below. zyk's flag alone decided, so a kept wait
	// was multiplied again when a "spawnflags" key was given (500 ms became 500 s), and a delay read again
	// in seconds was taken as milliseconds when none was (10 s became 10 ms). 65536 given among the
	// spawnflags keys still means the values are milliseconds already, as zyk made it.
	{
		int storedFlags = 0, delayGiven = 0;
		float waitGiven = 0.0f;
		const qboolean typedMs = ( G_SpawnInt( "spawnflags", "0", &storedFlags ) && ( storedFlags & 65536 ) ) ? qtrue : qfalse;
		const qboolean waitKey = G_SpawnFloat( "wait", "0", &waitGiven );
		const qboolean delayKey = G_SpawnInt( "delay", "0", &delayGiven );

		if ( !self->wait )
		{
			self->wait = 500;
		}
		else if ( waitKey && !typedMs )
		{
			self->wait *= 1000;//1 = 1 msec, 1000 = 1 sec
		}

		if ( delayKey )
		{
			RP_SpawnerClampDelay( self, typedMs ? 1 : 1000 );//1 = 1 msec, 1000 = 1 sec
		}
	}

	self->spawnflags |= 65536;

	G_SpawnInt( "showhealth", "0", &t );
	if (t)
	{
		self->s.shouldtarget = qtrue;
	}

	// GalaxyRP: [Entity System] "npceffect" -- see RP_NpcEffectFromName(). Read every time, so a spawner
	// spawned again in place (/entedit) without the key loses the effect rather than keeping the old one.
	{
		char *effectName = NULL;
		int effect;

		G_SpawnString( "npceffect", "", &effectName );
		effect = RP_NpcEffectFromName( effectName );
		self->rpSpawnerEffect = ( effect > RP_PHASE_NONE ) ? effect : RP_PHASE_NONE;
	}

	// GalaxyRP: [Entity System] "npcteam" -- see RP_NpcTeamFromName(). Read every time, like npceffect.
	RP_SpawnerTeamKey( self );

	// DAJ_RP: [NPC Rewards] "npccredits" and "npcxp" -- see RP_SpawnerRewardKeys()
	RP_SpawnerRewardKeys( self );

	// DAJ_RP: [NPC Scale] "npcscale" -- see RP_SpawnerScaleKey()
	RP_SpawnerScaleKey( self );

	// DAJ_RP: [NPC System] "npcfreeze" -- see RP_NpcFreezeFromValue(). Read every time, like npceffect.
	{
		char *freezeValue = NULL;

		G_SpawnString( "npcfreeze", "", &freezeValue );
		self->rpSpawnerFreeze = ( RP_NpcFreezeFromValue( freezeValue ) == 1 ) ? qtrue : qfalse;
	}
	/*
	if ( self->delay > 0 )
	{
		self->r.svFlags |= SVF_NPC_PRECACHE;
	}
	*/
	//rwwFIXMEFIXME: support for this flag?

	//We have to load the animation.cfg now because spawnscripts are going to want to set anims and we need to know their length and if they're valid
	NPC_PrecacheAnimationCFG( self->NPC_type );

	//rww - can't cheat and do this on the client like in SP, so I'm doing this.
	NPC_Precache(self);

	if ( self->targetname )
	{//Wait for triggering
		self->use = NPC_Spawn;
	//	self->r.svFlags |= SVF_NPC_PRECACHE;//FIXME: precache my weapons somehow?

		//NPC_PrecacheModels( self->NPC_type );

		// GalaxyRP: [Entity System] "spawnnow" and "respawn" -- see RP_SpawnerKeys()
		RP_SpawnerKeys( self );
	}
	else
	{
		self->rpSpawnerRespawn = qfalse;	// GalaxyRP: it spawns once and is gone
		//NOTE: auto-spawners never check for shy spawning
		//if ( spawning )
		if (1) //just gonna always do this I suppose.
		{//in entity spawn stage - map starting up
			self->think = NPC_Spawn_Go;
			self->nextthink = level.time + START_TIME_REMOVE_ENTS + 50;
		}
		else
		{//else spawn right now
			NPC_Spawn( self, self, self );
		}
	}

	//FIXME: store cameraGroup somewhere else and apply to spawned NPCs' cameraGroup
	//Or just don't include NPC_spawners in cameraGroupings
}

extern void G_VehicleSpawn( gentity_t *self );
/*QUAKED NPC_Vehicle (1 0 0) (-16 -16 -24) (16 16 32) NO_PILOT_DIE SUSPENDED x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
NO_PILOT_DIE - die after certain amount of time of not having a pilot
SUSPENDED - Fighters: Don't drop until someone gets in it (this only works as long as no-nw has *ever* ridden the vehicle, to simulate ships that are suspended-docked) - note: ships inside trigger_spaces do not drop when unoccupied
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

set NPC_type to vehicle name in vehicles.dat

"dropTime" use with SUSPENDED - if set, the vehicle will drop straight down for this number of seconds before flying forward
"dmg" use with NO_PILOT_DIE - delay in milliseconds for ship to explode if no pilot (default 10000)
"speed" use with NO_PILOT_DIE - distance for pilot to get away from ship after dismounting before it starts counting down the death timer
"model2" - if the vehicle can have a droid (has "*droidunit" tag), this NPC will be spawned and placed there - note: game will automatically use the one specified in the .veh file (if any) or, absent that, it will use an R2D2 or R5D2 NPC)

showhealth - set to 1 to show health bar on this entity when crosshair is over it

teamowner - crosshair shows green for this team, red for opposite team
	0 - none
	1 - red
	2 - blue

teamuser - only this team can use this NPC
	0 - none
	1 - red
	2 - blue

teamnodmg - team that NPC does not take damage from (turrets and other auto-defenses that have "alliedTeam" set to this team won't target this NPC)
	0 - none
	1 - red
	2 - blue
*/
qboolean NPC_VehiclePrecache( gentity_t *spawner )
{
	char *droidNPCType = NULL;
	//This will precache the vehicle
	vehicleInfo_t *pVehInfo;
	int iVehIndex = BG_VehicleGetIndex( spawner->NPC_type );
 	if ( iVehIndex == VEHICLE_NONE )
	{//fixme: error msg?
		return qfalse;
	}

	G_ModelIndex(va("$%s", spawner->NPC_type)); //make sure the thing is frickin precached
	//now cache his model/skin/anim config
	pVehInfo = &g_vehicleInfo[iVehIndex];
	if (pVehInfo->model && pVehInfo->model[0])
	{
		void *tempG2 = NULL;
		int skin = 0;
		if (pVehInfo->skin && pVehInfo->skin[0])
		{
			skin = trap->R_RegisterSkin(va("models/players/%s/model_%s.skin", pVehInfo->model, pVehInfo->skin));
		}
		trap->G2API_InitGhoul2Model(&tempG2, va("models/players/%s/model.glm", pVehInfo->model), 0, skin, 0, 0, 0);
		if (tempG2)
		{ //now, cache the anim config.
			char GLAName[1024];

			GLAName[0] = 0;
			trap->G2API_GetGLAName(tempG2, 0, GLAName);

			if (GLAName[0])
			{
				char *slash = Q_strrchr( GLAName, '/' );
				if ( slash )
				{
					strcpy(slash, "/animation.cfg");

					BG_ParseAnimationFile(GLAName, NULL, qfalse);
				}
			}
			trap->G2API_CleanGhoul2Models(&tempG2);
		}
	}

	//also precache the droid NPC if there is one
	if ( spawner->model2
		&& spawner->model2[0] )
	{
		droidNPCType = spawner->model2;
	}
	else if ( g_vehicleInfo[iVehIndex].droidNPC
		&& g_vehicleInfo[iVehIndex].droidNPC[0] )
	{
		droidNPCType = g_vehicleInfo[iVehIndex].droidNPC;
	}

	if ( droidNPCType )
	{
		if ( Q_stricmp( "random", droidNPCType ) == 0
			|| Q_stricmp( "default", droidNPCType ) == 0 )
		{//precache both r2 and r5, as defaults
			NPC_PrecacheType( "r2d2" );
			NPC_PrecacheType( "r5d2" );
		}
		else
		{
			NPC_PrecacheType( droidNPCType );
		}
	}
	return qtrue;
}

void NPC_VehicleSpawnUse( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( self->delay )
	{
		self->think = G_VehicleSpawn;
		self->nextthink = level.time + self->delay;
	}
	else
	{
		G_VehicleSpawn( self );
	}
}

void SP_NPC_Vehicle( gentity_t *self)
{
	float dropTime;
	int		t;
	if ( !self->NPC_type )
	{
		self->NPC_type = "swoop";
	}

	if ( !self->classname )
	{
		self->classname = "NPC_Vehicle";
	}

	// GalaxyRP fix: [Entity System] only a wait or delay just read from the entity's keys is in seconds:
	// spawned again in place (/entedit, /entcut, /entrotate), the vehicle spawner keeps the ones its keys
	// do not set, in milliseconds already, and multiplying them again made a 500 ms wait 500 s. See
	// SP_NPC_spawner().
	{
		int delayGiven = 0;
		float waitGiven = 0.0f;
		const qboolean waitKey = G_SpawnFloat( "wait", "0", &waitGiven );
		const qboolean delayKey = G_SpawnInt( "delay", "0", &delayGiven );

		if ( !self->wait )
		{
			self->wait = 500;
		}
		else if ( waitKey )
		{
			self->wait *= 1000;//1 = 1 msec, 1000 = 1 sec
		}
		if ( delayKey )
		{
			RP_SpawnerClampDelay( self, 1000 );//1 = 1 msec, 1000 = 1 sec
		}
	}

	// GalaxyRP: [Entity System] no count is one use, as G_VehicleSpawn() has always taken it -- said here,
	// so that a "spawnnow" spawn (RP_SpawnerSpawnNow()), which adds one to the count before it, does not
	// take a count of 0 for its last use
	if ( !self->count )
	{
		self->count = 1;
	}
	RP_SpawnerClampCount( self );

	G_SetOrigin( self, self->s.origin );
	G_SetAngles( self, self->s.angles );
	G_SpawnFloat( "dropTime", "0", &dropTime );
	if ( dropTime )
	{
		self->fly_sound_debounce_time = ceil(dropTime*1000.0);
	}

	G_SpawnInt( "showhealth", "0", &t );
	if (t)
	{
		self->s.shouldtarget = qtrue;
	}

	// GalaxyRP: [Entity System] "npcteam" -- /npc team takes vehicles too; see RP_NpcTeamFromName()
	RP_SpawnerTeamKey( self );

	// DAJ_RP: [NPC Scale] "npcscale" -- a vehicle is drawn, and carries its riders, at that size; see RP_SpawnerScaleKey()
	RP_SpawnerScaleKey( self );

	// DAJ_RP: [NPC Rewards] a vehicle pays nothing: /entadd and /entedit refuse npccredits and npcxp on an
	// NPC_Vehicle, and one given them another way is not read
	self->rpRewardCredits = 0;
	self->rpRewardXP = 0;
	//FIXME: PRECACHE!!!

	if ( self->targetname )
	{
		if ( !NPC_VehiclePrecache( self ) )
		{//FIXME: err msg?
			G_FreeEntity( self );
			return;
		}
		self->use = NPC_VehicleSpawnUse;

		// GalaxyRP: [Entity System] "spawnnow" and "respawn" -- see RP_SpawnerKeys()
		RP_SpawnerKeys( self );
	}
	else
	{
		self->rpSpawnerRespawn = qfalse;	// GalaxyRP: it spawns once and is gone
		if ( self->delay )
		{
			if ( !NPC_VehiclePrecache( self ) )
			{//FIXME: err msg?
				G_FreeEntity( self );
				return;
			}
			self->think = G_VehicleSpawn;
			self->nextthink = level.time + self->delay;
		}
		else
		{
			G_VehicleSpawn( self );
		}
	}
}

//Characters

//STAR WARS NPCs============================================================================
/*QUAKED NPC_spawner (1 0 0) (-16 -16 -24) (16 16 32) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY

CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

targetname - name this NPC goes by for targetting
target - NPC will fire this when it spawns it's last NPC (should this be when the last NPC it spawned dies?)

If targeted, will only spawn a NPC when triggered
count - how many NPCs to spawn (only if targetted) default = 1

NPC_type - the name of the NPC (from NPCs.cfg or a from .npc file)

NPC_targetname - NPC's targetname AND script_targetname
NPC_target - NPC's target to fire when killed
health - starting health (default = 100)
playerTeam - Who not to shoot! (default is TEAM_STARFLEET)
	TEAM_FREE (none) = 0
	TEAM_RED = 1
	TEAM_BLUE = 2
	TEAM_GOLD = 3
	TEAM_GREEN = 4
	TEAM_STARFLEET = 5
	TEAM_BORG = 6
	TEAM_SCAVENGERS = 7
	TEAM_STASIS = 8
	TEAM_NPCS = 9
	TEAM_HARVESTER, = 10
	TEAM_FORGE = 11
enemyTeam - Who to shoot (all but own if not set)

spawnscript - default script to run once spawned (none by default)
usescript - default script to run when used (none by default)
awakescript - default script to run once awoken (none by default)
angerscript - default script to run once angered (none by default)
painscript - default script to run when hit (none by default)
fleescript - default script to run when hit and below 50% health (none by default)
deathscript - default script to run when killed (none by default)

These strings can be used to activate behaviors instead of scripts - these are checked
first and so no scripts should be names with these names:
    default - 0: whatever
	idle - 1: Stand around, do abolutely nothing
	roam - 2: Roam around, collect stuff
	walk - 3: Crouch-Walk toward their goals
	run - 4: Run toward their goals
	standshoot - 5: Stay in one spot and shoot- duck when neccesary
	standguard - 6: Wait around for an enemy
	patrol - 7: Follow a path, looking for enemies
	huntkill - 8: Track down enemies and kill them
	evade - 9: Run from enemies
	evadeshoot - 10: Run from enemies, shoot them if they hit you
	runshoot - 11: Run to your goal and shoot enemy when possible
	defend - 12: Defend an entity or spot?
	snipe - 13: Stay hidden, shoot enemy only when have perfect shot and back turned
	combat - 14: Attack, evade, use cover, move about, etc.  Full combat AI - id NPC code
	medic - 15: Go for lowest health buddy, hide and heal him.
	takecover - 16: Find nearest cover from enemies
	getammo - 17: Go get some ammo
	advancefight - 18: Go somewhere and fight along the way
	face - 19: turn until facing desired angles
	wait - 20: do nothing
	formation - 21: Maintain a formation
	crouch - 22: Crouch-walk toward their goals

delay - after spawned or triggered, how many seconds to wait to spawn the NPC
*/

//=============================================================================================
//CHARACTERS
//=============================================================================================

/*QUAKED NPC_Player (1 0 0) (-16 -16 -24) (16 16 32) x RIFLEMAN PHASER TRICORDER DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Player( gentity_t *self)
{
	self->NPC_type = "Player";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Kyle (1 0 0) (-16 -16 -24) (16 16 32) x RIFLEMAN PHASER TRICORDER DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Kyle( gentity_t *self)
{
	self->NPC_type = "Kyle";

	WP_SetSaberModel( NULL, CLASS_KYLE );

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Lando(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Lando( gentity_t *self)
{
	self->NPC_type = "Lando";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Jan(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Jan( gentity_t *self)
{
	self->NPC_type = "Jan";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Luke(1 0 0) (-16 -16 -24) (16 16 40) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Luke( gentity_t *self)
{
	self->NPC_type = "Luke";

	WP_SetSaberModel( NULL, CLASS_LUKE );

	SP_NPC_spawner( self );
}

/*QUAKED NPC_MonMothma(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_MonMothma( gentity_t *self)
{
	self->NPC_type = "MonMothma";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Rosh_Penin (1 0 0) (-16 -16 -24) (16 16 32) DARKSIDE NOFORCE x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
Good Rosh
DARKSIDE - Evil Rosh
NOFORCE - Can't jump, starts with no saber
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Rosh_Penin( gentity_t *self)
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "rosh_dark";
	}
	else if ( (self->spawnflags&2) )
	{
		self->NPC_type = "rosh_penin_noforce";
	}
	else
	{
		self->NPC_type = "rosh_penin";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Tavion (1 0 0) (-16 -16 -24) (16 16 32) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Tavion( gentity_t *self)
{
	self->NPC_type = "Tavion";

	WP_SetSaberModel( NULL, CLASS_TAVION );

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Tavion_New (1 0 0) (-16 -16 -24) (16 16 32) SCEPTER SITH_SWORD x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
Has a red lightsaber and force powers, uses her saber style from JK2

SCEPTER - Has a red lightsaber and force powers, Ragnos' Scepter in left hand, uses dual saber style and occasionally attacks with Scepter
SITH_SWORD - Has Ragnos' Sith Sword in right hand and force powers, uses strong style
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Tavion_New( gentity_t *self)
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "tavion_scepter";
	}
	else if ( (self->spawnflags&2) )
	{
		self->NPC_type = "tavion_sith_sword";
	}
	else
	{
		self->NPC_type = "tavion_new";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Alora (1 0 0) (-16 -16 -24) (16 16 32) DUAL x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
Lightsaber and level 2 force powers, 300 health

DUAL - Dual sabers and level 3 force powers, 500 health
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Alora( gentity_t *self)
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "alora_dual";
	}
	else
	{
		self->NPC_type = "alora";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Reborn_New(1 0 0) (-16 -16 -24) (16 16 40) DUAL STAFF WEAK x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY

Reborn is an excellent lightsaber fighter, acrobatic and uses force powers.  Full-length red saber, 200 health.

DUAL - Use 2 shorter sabers
STAFF - Uses a saber staff
WEAK - Is a bit less tough
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Reborn_New( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&4) )
		{//weaker guys
			if ( (self->spawnflags&1) )
			{
				self->NPC_type = "reborn_dual2";
			}
			else if ( (self->spawnflags&2) )
			{
				self->NPC_type = "reborn_staff2";
			}
			else
			{
				self->NPC_type = "reborn_new2";
			}
		}
		else
		{
			if ( (self->spawnflags&1) )
			{
				self->NPC_type = "reborn_dual";
			}
			else if ( (self->spawnflags&2) )
			{
				self->NPC_type = "reborn_staff";
			}
			else
			{
				self->NPC_type = "reborn_new";
			}
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Cultist_Saber(1 0 0) (-16 -16 -24) (16 16 40) MED STRONG ALL THROW CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
Uses a saber and no force powers.  100 health.

default fencer uses fast style - weak, but can attack rapidly.  Good defense.

MED - Uses medium style - average speed and attack strength, average defense.
STRONG - Uses strong style, slower than others, but can do a lot of damage with one blow.  Weak defense.
ALL - Knows all 3 styles, switches between them, good defense.
THROW - can throw their saber (level 2) - reduces their defense some (can use this spawnflag alone or in combination with any *one* of the previous 3 spawnflags)

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Cultist_Saber( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_med_throw";
			}
			else
			{
				self->NPC_type = "cultist_saber_med";
			}
		}
		else if ( (self->spawnflags&2) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_strong_throw";
			}
			else
			{
				self->NPC_type = "cultist_saber_strong";
			}
		}
		else if ( (self->spawnflags&4) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_all_throw";
			}
			else
			{
				self->NPC_type = "cultist_saber_all";
			}
		}
		else
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_throw";
			}
			else
			{
				self->NPC_type = "cultist_saber";
			}
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Cultist_Saber_Powers(1 0 0) (-16 -16 -24) (16 16 40) MED STRONG ALL THROW CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
Uses a saber and has a couple low-level powers.  150 health.

default fencer uses fast style - weak, but can attack rapidly.  Good defense.

MED - Uses medium style - average speed and attack strength, average defense.
STRONG - Uses strong style, slower than others, but can do a lot of damage with one blow.  Weak defense.
ALL - Knows all 3 styles, switches between them, good defense.
THROW - can throw their saber (level 2) - reduces their defense some (can use this spawnflag alone or in combination with any *one* of the previous 3 spawnflags)

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Cultist_Saber_Powers( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_med_throw2";
			}
			else
			{
				self->NPC_type = "cultist_saber_med2";
			}
		}
		else if ( (self->spawnflags&2) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_strong_throw2";
			}
			else
			{
				self->NPC_type = "cultist_saber_strong2";
			}
		}
		else if ( (self->spawnflags&4) )
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_all_throw2";
			}
			else
			{
				self->NPC_type = "cultist_saber_all2";
			}
		}
		else
		{
			if ( (self->spawnflags&8) )
			{
				self->NPC_type = "cultist_saber_throw";
			}
			else
			{
				self->NPC_type = "cultist_saber2";
			}
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Cultist(1 0 0) (-16 -16 -24) (16 16 40) SABER GRIP LIGHTNING DRAIN CEILING CINEMATIC NOTSOLID STARTINSOLID SHY

Cultist uses a blaster and force powers.  40 health.

SABER - Uses a saber and no force powers
GRIP - Uses no weapon and grip, push and pull
LIGHTNING - Uses no weapon and lightning and push
DRAIN - Uses no weapons and drain and push

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Cultist( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = NULL;
			self->spawnflags = 0;//fast, no throw
			switch ( Q_irand( 0, 2 ) )
			{
			case 0://medium
				self->spawnflags |= 1;
				break;
			case 1://strong
				self->spawnflags |= 2;
				break;
			case 2://all
				self->spawnflags |= 4;
				break;
			}
			if ( Q_irand( 0, 1 ) )
			{//throw
				self->spawnflags |= 8;
			}
			SP_NPC_Cultist_Saber( self );
			return;
		}
		else if ( (self->spawnflags&2) )
		{
			self->NPC_type = "cultist_grip";
		}
		else if ( (self->spawnflags&4) )
		{
			self->NPC_type = "cultist_lightning";
		}
		else if ( (self->spawnflags&8) )
		{
			self->NPC_type = "cultist_drain";
		}
		else
		{
			self->NPC_type = "cultist";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Cultist_Commando(1 0 0) (-16 -16 -24) (16 16 40) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY

Cultist uses dual blaster pistols and force powers.  40 health.

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Cultist_Commando( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		self->NPC_type = "cultistcommando";
	}
	SP_NPC_spawner( self );
}
/*QUAKED NPC_Cultist_Destroyer(1 0 0) (-16 -16 -24) (16 16 40) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY

Cultist has no weapons, runs up to you chanting & building up a Force Destruction blast - when gets to you, screams & explodes

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Cultist_Destroyer( gentity_t *self)
{
	self->NPC_type = "cultist";//"cultist_explode";
	SP_NPC_spawner( self );
}

/*QUAKED NPC_Reelo(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Reelo( gentity_t *self)
{
	self->NPC_type = "Reelo";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Kothos(1 0 0) (-16 -16 -24) (16 16 40) VIL x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
VIL - spawns Vil Kothos instead of Dasariah (can anyone tell them apart anyway...?)

Force only... will (eventually) be set up to re-inforce their leader (use SET_LEADER) by healing them, recharging them, keeping the player away, etc.

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Kothos( gentity_t *self )
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "VKothos";
	}
	else
	{
		self->NPC_type = "DKothos";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Droid_Saber (1 0 0) (-12 -12 -24) (12 12 40) TRAINING x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Droid_Saber( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "saber_droid_training";
		}
		else
		{
			self->NPC_type = "saber_droid";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Galak(1 0 0) (-16 -16 -24) (16 16 40) MECH x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
MECH - will be the armored Galak

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Galak( gentity_t *self)
{
	if ( self->spawnflags & 1 )
	{
		self->NPC_type = "Galak_Mech";
		NPC_GalakMech_Precache();
	}
	else
	{
		self->NPC_type = "Galak";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Desann(1 0 0) (-16 -16 -24) (16 16 40) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Desann( gentity_t *self)
{
	self->NPC_type = "Desann";

	WP_SetSaberModel( NULL, CLASS_DESANN );

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Bartender(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Bartender( gentity_t *self)
{
	self->NPC_type = "Bartender";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_MorganKatarn(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_MorganKatarn( gentity_t *self)
{
	self->NPC_type = "MorganKatarn";

	SP_NPC_spawner( self );
}

//=============================================================================================
//ALLIES
//=============================================================================================

/*QUAKED NPC_Jedi(1 0 0) (-16 -16 -24) (16 16 40) TRAINER MASTER RANDOM x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
TRAINER - Special Jedi- instructor
MASTER - Special Jedi- master
RANDOM - creates a random Jedi student using the available player models/skins
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Ally Jedi NPC Buddy - tags along with player
*/
void SP_NPC_Jedi( gentity_t *self)
{
	if(!self->NPC_type)
	{
		if ( self->spawnflags & 4 )
		{//random!
			switch ( Q_irand( 0, 11 ) )
			{
			case 0:
				self->NPC_type = "jedi_hf1";
				break;
			case 1:
				self->NPC_type = "jedi_hf2";
				break;
			case 2:
				self->NPC_type = "jedi_hm1";
				break;
			case 3:
				self->NPC_type = "jedi_hm2";
				break;
			case 4:
				self->NPC_type = "jedi_kdm1";
				break;
			case 5:
				self->NPC_type = "jedi_kdm2";
				break;
			case 6:
				self->NPC_type = "jedi_rm1";
				break;
			case 7:
				self->NPC_type = "jedi_rm2";
				break;
			case 8:
				self->NPC_type = "jedi_tf1";
				break;
			case 9:
				self->NPC_type = "jedi_tf2";
				break;
			case 10:
				self->NPC_type = "jedi_zf1";
				break;
			case 11:
			default://just in case
				self->NPC_type = "jedi_zf2";
				break;
			}
		}
		else if ( self->spawnflags & 2 )
		{
			self->NPC_type = "jedimaster";
		}
		else if ( self->spawnflags & 1 )
		{
			self->NPC_type = "jeditrainer";
		}
		else
		{
			/*
			if ( !Q_irand( 0, 2 ) )
			{
				self->NPC_type = "JediF";
			}
			else
			*/if ( Q_irand( 0, 1 ) )
			{
				self->NPC_type = "Jedi";
			}
			else
			{
				self->NPC_type = "Jedi2";
			}
		}
	}

	WP_SetSaberModel( NULL, CLASS_JEDI );

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Prisoner(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Prisoner( gentity_t *self)
{
	if(!self->NPC_type)
	{
		if ( Q_irand( 0, 1 ) )
		{
			self->NPC_type = "Prisoner";
		}
		else
		{
			self->NPC_type = "Prisoner2";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Merchant(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Merchant( gentity_t *self)
{
	self->NPC_type = "merchant";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Saboteur(1 0 0) (-16 -16 -24) (16 16 40) SNIPER PISTOL x x CLOAKED CINEMATIC NOTSOLID STARTINSOLID SHY
Has a blaster rifle, can cloak and roll

SNIPER - Has a sniper rifle, no acrobatics, but can dodge
PISTOL - Just has a pistol, can roll
COMMANDO - Has 2 pistols and can roll & dodge

CLOAKED - Starts cloaked
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Saboteur( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "saboteursniper";
		}
		else if ( (self->spawnflags&2) )
		{
			self->NPC_type = "saboteurpistol";
		}
		else if ( (self->spawnflags&4) )
		{
			self->NPC_type = "saboteurcommando";
		}
		else
		{
			self->NPC_type = "saboteur";
		}
	}
	SP_NPC_spawner( self );
}

/*QUAKED NPC_Human_Merc(1 0 0) (-16 -16 -24) (16 16 40) BOWCASTER REPEATER FLECHETTE CONCUSSION DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
100 health, blaster rifle

BOWCASTER - Starts with a Bowcaster
REPEATER - Starts with a Repeater
FLECHETTE - Starts with a Flechette gun
CONCUSSION - Starts with a Concussion Rifle

If you want them to start with any other kind of weapon, make a spawnscript for them that sets their weapon.

"message" - turns on his key surface.  This is the name of the key you get when you walk over his body.  This must match the "message" field of the func_security_panel you want this key to open.  Set to "goodie" to have him carrying a goodie key that player can use to operate doors with "GOODIE" spawnflag.  NOTE: this overrides all the weapon spawnflags

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Human_Merc( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->message )
		{
			self->NPC_type = "human_merc_key";
		}
		else if ( (self->spawnflags&1) )
		{
			self->NPC_type = "human_merc_bow";
		}
		else if ( (self->spawnflags&2) )
		{
			self->NPC_type = "human_merc_rep";
		}
		else if ( (self->spawnflags&4) )
		{
			self->NPC_type = "human_merc_flc";
		}
		else if ( (self->spawnflags&8) )
		{
			self->NPC_type = "human_merc_cnc";
		}
		else
		{
			self->NPC_type = "human_merc";
		}
	}
	SP_NPC_spawner( self );
}

/*QUAKED NPC_Chewbacca(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Chewbacca( gentity_t *self )
{
	self->NPC_type = "Chewie";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Rax(1 0 0) (-16 -16 -24) (16 16 40) FUN x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
FUN - Makes him magically the funnest thing ever in any game ever made. (actually does nothing, it'll just be fun by the power of suggestion).
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Rax( gentity_t *self )
{
	self->NPC_type = "Rax";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Lannik_Racto(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Lannik_Racto( gentity_t *self )
{
	self->NPC_type = "lannik_racto";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Droid_Assassin (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Droid_Assassin( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		self->NPC_type = "assassin_droid";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_RocketTrooper(1 0 0) (-16 -16 -24) (16 16 40) OFFICER SPOTLIGHT x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
200 health, flies, rockets

OFFICER - starts flying, uses concussion rifle instead of rockets
SPOTLIGHT - uses a shoulder-mounted spotlight

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_RocketTrooper( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "rockettrooper2Officer";
		}
		else
		{
			self->NPC_type = "rockettrooper2";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_HazardTrooper(1 0 0) (-16 -16 -24) (16 16 40) OFFICER CONCUSSION x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
250 health, repeater

OFFICER - 400 health, flechette
CONCUSSION - 400 health, concussion rifle
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_HazardTrooper( gentity_t *self)
{
	if ( !self->NPC_type )
	{


		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "hazardtrooperofficer";
		}
		else if ( (self->spawnflags&2) )
		{
			self->NPC_type = "hazardtrooperconcussion";
		}
		else
		{
			self->NPC_type = "hazardtrooper";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_BobaFett(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_BobaFett( gentity_t *self )
{
	self->NPC_type = "Boba_Fett";
	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Mutant_Rancor (1 0 0) (-60 -60 -24) (60 60 360) x FASTKILL x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
Bigger, meaner, nastier, Frencher.  Breath attack, pound attack.

FASTKILL - Kills NPCs faster
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Mutant_Rancor( gentity_t *self)
{
	self->NPC_type = "mutant_rancor";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Ragnos(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Ragnos( gentity_t *self )
{
	self->NPC_type = "Ragnos";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Rebel(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Rebel( gentity_t *self)
{
	if(!self->NPC_type)
	{
		if ( Q_irand( 0, 1 ) )
		{
			self->NPC_type = "Rebel";
		}
		else
		{
			self->NPC_type = "Rebel2";
		}
	}

	SP_NPC_spawner( self );
}

//=============================================================================================
//ENEMIES
//=============================================================================================

/*QUAKED NPC_Stormtrooper(1 0 0) (-16 -16 -24) (16 16 40) OFFICER COMMANDER ALTOFFICER ROCKET DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
30 health, blaster

OFFICER - 60 health, flechette
COMMANDER - 60 health, heavy repeater
ALTOFFICER - 60 health, alt-fire flechette (grenades)
ROCKET - 60 health, rocket launcher

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Stormtrooper( gentity_t *self)
{
	if ( self->spawnflags & 8 )
	{//rocketer
		self->NPC_type = "rockettrooper";
	}
	else if ( self->spawnflags & 4 )
	{//alt-officer
		self->NPC_type = "stofficeralt";
	}
	else if ( self->spawnflags & 2 )
	{//commander
		self->NPC_type = "stcommander";
	}
	else if ( self->spawnflags & 1 )
	{//officer
		self->NPC_type = "stofficer";
	}
	else
	{//regular trooper
		if ( Q_irand( 0, 1 ) )
		{
			self->NPC_type = "StormTrooper";
		}
		else
		{
			self->NPC_type = "StormTrooper2";
		}
	}

	SP_NPC_spawner( self );
}
void SP_NPC_StormtrooperOfficer( gentity_t *self)
{
	self->spawnflags |= 1;
	SP_NPC_Stormtrooper( self );
}
/*QUAKED NPC_Snowtrooper(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
30 health, blaster

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Snowtrooper( gentity_t *self)
{
	self->NPC_type = "snowtrooper";

	SP_NPC_spawner( self );
}
/*QUAKED NPC_Tie_Pilot(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
30 health, blaster

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Tie_Pilot( gentity_t *self)
{
	self->NPC_type = "stormpilot";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Ugnaught(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Ugnaught( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( Q_irand( 0, 1 ) )
		{
			self->NPC_type = "Ugnaught";
		}
		else
		{
			self->NPC_type = "Ugnaught2";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Jawa(1 0 0) (-16 -16 -24) (16 16 40) ARMED x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
ARMED - starts with the Jawa gun in-hand

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Jawa( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "jawa_armed";
		}
		else
		{
			self->NPC_type = "jawa";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Gran(1 0 0) (-16 -16 -24) (16 16 40) SHOOTER BOXER x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
Uses grenade

SHOOTER - uses blaster instead of
BOXER - uses fists only
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Gran( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->spawnflags & 1 )
		{
			self->NPC_type = "granshooter";
		}
		else if ( self->spawnflags & 2 )
		{
			self->NPC_type = "granboxer";
		}
		else
		{
			if ( Q_irand( 0, 1 ) )
			{
				self->NPC_type = "gran";
			}
			else
			{
				self->NPC_type = "gran2";
			}
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Rodian(1 0 0) (-16 -16 -24) (16 16 40) BLASTER NO_HIDE x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
BLASTER uses a blaster instead of sniper rifle, different skin
NO_HIDE (only applicable with snipers) does not duck and hide between shots
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Rodian( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->spawnflags&1 )
		{
			self->NPC_type = "rodian2";
		}
		else
		{
			self->NPC_type = "rodian";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Weequay(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Weequay( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		switch ( Q_irand( 0, 3 ) )
		{
		case 0:
			self->NPC_type = "Weequay";
			break;
		case 1:
			self->NPC_type = "Weequay2";
			break;
		case 2:
			self->NPC_type = "Weequay3";
			break;
		case 3:
			self->NPC_type = "Weequay4";
			break;
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Trandoshan(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Trandoshan( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		self->NPC_type = "Trandoshan";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Tusken(1 0 0) (-16 -16 -24) (16 16 40) SNIPER x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Tusken( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( (self->spawnflags&1) )
		{
			self->NPC_type = "tuskensniper";
		}
		else
		{
			self->NPC_type = "tusken";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Noghri(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Noghri( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		self->NPC_type = "noghri";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_SwampTrooper(1 0 0) (-16 -16 -24) (16 16 40) REPEATER x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
REPEATER - Swaptrooper who uses the repeater
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_SwampTrooper( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->spawnflags & 1 )
		{
			self->NPC_type = "SwampTrooper2";
		}
		else
		{
			self->NPC_type = "SwampTrooper";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Imperial(1 0 0) (-16 -16 -24) (16 16 40) OFFICER COMMANDER x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY

Greyshirt grunt, uses blaster pistol, 20 health.

OFFICER - Brownshirt Officer, uses blaster rifle, 40 health
COMMANDER - Blackshirt Commander, uses rapid-fire blaster rifle, 80 healt

"message" - if a COMMANDER, turns on his key surface.  This is the name of the key you get when you walk over his body.  This must match the "message" field of the func_security_panel you want this key to open.  Set to "goodie" to have him carrying a goodie key that player can use to operate doors with "GOODIE" spawnflag.

DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Imperial( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->spawnflags & 1 )
		{
			self->NPC_type = "ImpOfficer";
		}
		else if ( self->spawnflags & 2 )
		{
			self->NPC_type = "ImpCommander";
		}
		else
		{
			self->NPC_type = "Imperial";
		}
	}

	/*
	if ( self->message )
	{//may drop a key, precache the key model and pickup sound
		G_SoundIndex( "sound/weapons/key_pkup.wav" );
		if ( !Q_stricmp( "goodie", self->message ) )
		{
			RegisterItem( FindItemForInventory( INV_GOODIE_KEY ) );
		}
		else
		{
			RegisterItem( FindItemForInventory( INV_SECURITY_KEY ) );
		}
	}
	*/
	//rwwFIXMEFIXME: Allow goodie keys
	SP_NPC_spawner( self );
}

/*QUAKED NPC_ImpWorker(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_ImpWorker( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( !Q_irand( 0, 2 ) )
		{
			self->NPC_type = "ImpWorker";
		}
		else if ( Q_irand( 0, 1 ) )
		{
			self->NPC_type = "ImpWorker2";
		}
		else
		{
			self->NPC_type = "ImpWorker3";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_BespinCop(1 0 0) (-16 -16 -24) (16 16 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_BespinCop( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( !Q_irand( 0, 1 ) )
		{
			self->NPC_type = "BespinCop";
		}
		else
		{
			self->NPC_type = "BespinCop2";
		}
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Reborn(1 0 0) (-16 -16 -24) (16 16 40) FORCE FENCER ACROBAT BOSS CEILING CINEMATIC NOTSOLID STARTINSOLID SHY

Default Reborn is A poor lightsaber fighter, acrobatic and uses no force powers.  Yellow saber, 40 health.

FORCE - Uses force powers but is not the best lightsaber fighter and not acrobatic.  Purple saber, 75 health.
FENCER - A good lightsaber fighter, but not acrobatic and uses no force powers.  Yellow saber, 100 health.
ACROBAT - quite acrobatic, but not the best lightsaber fighter and uses no force powers.  Red saber, 100 health.
BOSS - quite acrobatic, good lightsaber fighter and uses force powers.  Orange saber, 150 health.

NOTE: Saber colors are temporary until they have different by skins to tell them apart

CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Reborn( gentity_t *self)
{
	if ( !self->NPC_type )
	{
		if ( self->spawnflags & 1 )
		{
			self->NPC_type = "rebornforceuser";
		}
		else if ( self->spawnflags & 2 )
		{
			self->NPC_type = "rebornfencer";
		}
		else if ( self->spawnflags & 4 )
		{
			self->NPC_type = "rebornacrobat";
		}
		else if ( self->spawnflags & 8 )
		{
			self->NPC_type = "rebornboss";
		}
		else
		{
			self->NPC_type = "reborn";
		}
	}

	WP_SetSaberModel( NULL, CLASS_REBORN );
	SP_NPC_spawner( self );
}

/*QUAKED NPC_ShadowTrooper(1 0 0) (-16 -16 -24) (16 16 40) x x x x CEILING CINEMATIC NOTSOLID STARTINSOLID SHY
CEILING - Sticks to the ceiling until he sees an enemy or takes pain
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_ShadowTrooper( gentity_t *self)
{
	if(!self->NPC_type)
	{
		if ( !Q_irand( 0, 1 ) )
		{
			self->NPC_type = "ShadowTrooper";
		}
		else
		{
			self->NPC_type = "ShadowTrooper2";
		}
	}

	NPC_ShadowTrooper_Precache();
	WP_SetSaberModel( NULL, CLASS_SHADOWTROOPER );

	SP_NPC_spawner( self );
}
//=============================================================================================
//MONSTERS
//=============================================================================================

/*QUAKED NPC_Monster_Murjj (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Murjj( gentity_t *self)
{
	self->NPC_type = "Murjj";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Swamp (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Swamp( gentity_t *self)
{
	self->NPC_type = "Swamp";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Howler (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Howler( gentity_t *self)
{
	self->NPC_type = "howler";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_MineMonster (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_MineMonster( gentity_t *self)
{
	self->NPC_type = "minemonster";

	SP_NPC_spawner( self );
	NPC_MineMonster_Precache();
}

/*QUAKED NPC_Monster_Claw (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Claw( gentity_t *self)
{
	self->NPC_type = "Claw";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Glider (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Glider( gentity_t *self)
{
	self->NPC_type = "Glider";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Flier2 (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Flier2( gentity_t *self)
{
	self->NPC_type = "Flier2";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Lizard (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Lizard( gentity_t *self)
{
	self->NPC_type = "Lizard";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Fish (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Fish( gentity_t *self)
{
	self->NPC_type = "Fish";

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Sand_Creature (1 0 0) (-24 -24 -24) (24 24 0) FAST x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

turfrange - if set, they will not go beyond this dist from their spawn position
*/
void SP_NPC_Monster_Sand_Creature( gentity_t *self)
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "sand_creature_fast";
	}
	else
	{
		self->NPC_type = "sand_creature";
	}

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Wampa (1 0 0) (-12 -12 -24) (12 12 40) WANDER SEARCH x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
WANDER - When I don't have an enemy, I'll just wander the waypoint network aimlessly
SEARCH - When I don't have an enemy, I'll go back and forth between the nearest waypoints, looking for enemies
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Wampa( gentity_t *self)
{
	self->NPC_type = "wampa";

	NPC_Wampa_Precache();

	SP_NPC_spawner( self );
}

/*QUAKED NPC_Monster_Rancor (1 0 0) (-30 -30 -24) (30 30 104) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Monster_Rancor( gentity_t *self)
{
	self->NPC_type = "rancor";

	SP_NPC_spawner( self );
}

//=============================================================================================
//DROIDS
//=============================================================================================

/*QUAKED NPC_Droid_Interrogator (1 0 0) (-12 -12 -24) (12 12 0) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Droid_Interrogator( gentity_t *self)
{
	self->NPC_type = "interrogator";

	SP_NPC_spawner( self );

	NPC_Interrogator_Precache(self);
}

/*QUAKED NPC_Droid_Probe (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Imperial Probe Droid - the multilegged floating droid that Han and Chewie shot on the ice planet Hoth
*/
void SP_NPC_Droid_Probe( gentity_t *self)
{
	self->NPC_type = "probe";

	SP_NPC_spawner( self );

	NPC_Probe_Precache();
}

/*QUAKED NPC_Droid_Mark1 (1 0 0) (-36 -36 -24) (36 36 80) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Big walking droid

*/
void SP_NPC_Droid_Mark1( gentity_t *self)
{
	self->NPC_type = "mark1";

	SP_NPC_spawner( self );

	NPC_Mark1_Precache();
}

/*QUAKED NPC_Droid_Mark2 (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Small rolling droid with one gun.

*/
void SP_NPC_Droid_Mark2( gentity_t *self)
{
	self->NPC_type = "mark2";

	SP_NPC_spawner( self );

	NPC_Mark2_Precache();
}

/*QUAKED NPC_Droid_ATST (1 0 0) (-40 -40 -24) (40 40 248) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy
*/
void SP_NPC_Droid_ATST( gentity_t *self)
{
	if ( (self->spawnflags&1) )
	{
		self->NPC_type = "atst_vehicle";
	}
	else
	{
		self->NPC_type = "atst";
	}

	SP_NPC_spawner( self );

	NPC_ATST_Precache();
}

/*QUAKED NPC_Droid_Remote (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Remote Droid - the floating round droid used by Obi Wan to train Luke about the force while on the Millenium Falcon.
*/
void SP_NPC_Droid_Remote( gentity_t *self)
{
	self->NPC_type = "remote";

	SP_NPC_spawner( self );

	NPC_Remote_Precache();
}

/*QUAKED NPC_Droid_Seeker (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Seeker Droid - floating round droids that shadow troopers spawn
*/
void SP_NPC_Droid_Seeker( gentity_t *self)
{
	self->NPC_type = "seeker";

	SP_NPC_spawner( self );

	NPC_Seeker_Precache();
}

/*QUAKED NPC_Droid_Sentry (1 0 0) (-24 -24 -24) (24 24 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Sentry Droid - Large, armored floating Imperial droids with 3 forward-facing gun turrets
*/
void SP_NPC_Droid_Sentry( gentity_t *self)
{
	self->NPC_type = "sentry";

	SP_NPC_spawner( self );

	NPC_Sentry_Precache();
}

/*QUAKED NPC_Droid_Gonk (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Gonk Droid - the droid that looks like a walking ice machine. Was in the Jawa land crawler, walking around talking to itself.

NOTARGET by default
*/
void SP_NPC_Droid_Gonk( gentity_t *self)
{
	self->NPC_type = "gonk";

	SP_NPC_spawner( self );

	//precache the Gonk sounds
	NPC_Gonk_Precache();
}

/*QUAKED NPC_Droid_Mouse (1 0 0) (-12 -12 -24) (12 12 40) x x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

Mouse Droid - small, box shaped droid, first seen on the Death Star. Chewie yelled at it and it backed up and ran away.

NOTARGET by default
*/
void SP_NPC_Droid_Mouse( gentity_t *self)
{
	self->NPC_type = "mouse";

	SP_NPC_spawner( self );

	//precache the Mouse sounds
	NPC_Mouse_Precache();

}

/*QUAKED NPC_Droid_R2D2 (1 0 0) (-12 -12 -24) (12 12 40) IMPERIAL x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

R2D2 Droid - you probably know this one already.

NOTARGET by default
*/
void SP_NPC_Droid_R2D2( gentity_t *self)
{
	if ( self->spawnflags&1 )
	{//imperial skin
		self->NPC_type = "r2d2_imp";
	}
	else
	{
		self->NPC_type = "r2d2";
	}

	SP_NPC_spawner( self );

	NPC_R2D2_Precache();
}

/*QUAKED NPC_Droid_R5D2 (1 0 0) (-12 -12 -24) (12 12 40) IMPERIAL ALWAYSDIE x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
ALWAYSDIE - won't go into spinning zombie AI when at low health.
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

R5D2 Droid - the droid originally chosen by Uncle Owen until it blew a bad motivator, and they took R2D2 instead.

NOTARGET by default
*/
void SP_NPC_Droid_R5D2( gentity_t *self)
{
	if ( self->spawnflags&1 )
	{//imperial skin
		self->NPC_type = "r5d2_imp";
	}
	else
	{
		self->NPC_type = "r5d2";
	}

	SP_NPC_spawner( self );

	NPC_R5D2_Precache();
}

/*QUAKED NPC_Droid_Protocol (1 0 0) (-12 -12 -24) (12 12 40) IMPERIAL x x x DROPTOFLOOR CINEMATIC NOTSOLID STARTINSOLID SHY
DROPTOFLOOR - NPC can be in air, but will spawn on the closest floor surface below it
CINEMATIC - Will spawn with no default AI (BS_CINEMATIC)
NOTSOLID - Starts not solid
STARTINSOLID - Don't try to fix if spawn in solid
SHY - Spawner is shy

NOTARGET by default
*/
void SP_NPC_Droid_Protocol( gentity_t *self)
{
	if ( self->spawnflags&1 )
	{//imperial skin
		self->NPC_type = "protocol_imp";
	}
	else
	{
		self->NPC_type = "protocol";
	}

	SP_NPC_spawner( self );
	NPC_Protocol_Precache();
}


//NPC console commands
/*
NPC_Spawn_f
*/

gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle )
{
	gentity_t		*NPCspawner;
	vec3_t			forward, end;
	trace_t			trace;

	// GalaxyRP fix: [Entity System] refuse before allocating, not after. One NPC costs three slots
	// -- this spawner, the NPC itself and its NPC->tempGoal -- and G_Spawn() does not report
	// failure, it calls trap->Error(ERR_DROP), which on a dedicated server ends the process (see
	// ZYK_ENTITY_RESERVE in g_local.h). That is the crash an admin gets from overspawning NPCs,
	// and it is why the "if (!NPCspawner)" check that used to be here could never fire: G_Spawn()
	// never returns NULL.
	if ( G_EntitySlotsAvailable( 3 ) == qfalse )
	{
		Com_Printf( S_COLOR_RED"NPC_Spawn Error: too few entity slots left (%d free, %d held in reserve)\n",
			G_FreeEntityCount(), ZYK_ENTITY_RESERVE );

		if ( ent && ent->client )
		{
			trap->SendServerCommand( ent-g_entities,
				va("print \"Cannot spawn an NPC: the server is near its entity limit. %d slots free. Try ^3/npc kill all^7.\n\"",
					G_FreeEntityCount()) );
		}

		G_LogPrintf( "NPC spawn of '%s' refused: %d entity slots free\n",
			npc_type ? npc_type : "?", G_FreeEntityCount() );
		return NULL;
	}

	// GalaxyRP: [Logical Entities] this spawner lives one frame and only exists so NPC_Spawn_Do
	// has an origin, angles and an NPC_type to read; it is an npc_spawner in all but name, so it
	// takes a logical slot when those are on (as TaystJK does) and never a networked one.
	NPCspawner = RP_SpawnForClassname( "npc_spawner", qfalse, qfalse );

	NPCspawner->think = G_FreeEntity;
	NPCspawner->nextthink = level.time + FRAMETIME;

	if ( !npc_type )
	{
		return NULL;
	}

	if (!npc_type[0])
	{
		Com_Printf( S_COLOR_RED"Error, expected one of:\n"S_COLOR_WHITE" NPC spawn [NPC type (from ext_data/NPCs)]\n NPC spawn vehicle [VEH type (from ext_data/vehicles)]\n" );
		return NULL;
	}

	if ( !ent || !ent->client )
	{//screw you, go away
		return NULL;
	}

	//rwwFIXMEFIXME: Care about who is issuing this command/other clients besides 0?
	//Spawn it at spot of first player
	//FIXME: will gib them!
	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
	VectorNormalize(forward);
	VectorMA(ent->r.currentOrigin, 64, forward, end);
	trap->Trace(&trace, ent->r.currentOrigin, NULL, NULL, end, 0, MASK_SOLID, qfalse, 0, 0);
	VectorCopy(trace.endpos, end);
	end[2] -= 24;
	trap->Trace(&trace, trace.endpos, NULL, NULL, end, 0, MASK_SOLID, qfalse, 0, 0);
	VectorCopy(trace.endpos, end);
	end[2] += 24;
	G_SetOrigin(NPCspawner, end);
	VectorCopy(NPCspawner->r.currentOrigin, NPCspawner->s.origin);
	//set the yaw so that they face away from player
	NPCspawner->s.angles[1] = ent->client->ps.viewangles[1];

	// GalaxyRP: [Logical Entities] see above -- a logical spawner is never linked. Nothing reads
	// the link: NPC_Spawn_Do takes the origin from the struct.
	if (!NPCspawner->isLogical)
	{
		trap->LinkEntity((sharedEntity_t *)NPCspawner);
	}

	NPCspawner->NPC_type = G_NewString( npc_type );

	if ( targetname )
	{
		NPCspawner->NPC_targetname = G_NewString(targetname);
	}

	NPCspawner->count = 1;

	NPCspawner->delay = 0;

	//NPCspawner->spawnflags |= SFB_NOTSOLID;

	//NPCspawner->playerTeam = TEAM_FREE;
	//NPCspawner->behaviorSet[BSET_SPAWN] = "common/guard";

	if ( isVehicle )
	{
		NPCspawner->classname = "NPC_Vehicle";
	}

	//call precache funcs for James' builds
	if ( !Q_stricmp( "gonk", NPCspawner->NPC_type))
	{
		NPC_Gonk_Precache();
	}
	else if ( !Q_stricmp( "mouse", NPCspawner->NPC_type))
	{
		NPC_Mouse_Precache();
	}
	else if ( !Q_strncmp( "r2d2", NPCspawner->NPC_type, 4))
	{
		NPC_R2D2_Precache();
	}
	else if ( !Q_stricmp( "atst", NPCspawner->NPC_type))
	{
		NPC_ATST_Precache();
	}
	else if ( !Q_strncmp( "r5d2", NPCspawner->NPC_type, 4))
	{
		NPC_R5D2_Precache();
	}
	else if ( !Q_stricmp( "mark1", NPCspawner->NPC_type))
	{
		NPC_Mark1_Precache();
	}
	else if ( !Q_stricmp( "mark2", NPCspawner->NPC_type))
	{
		NPC_Mark2_Precache();
	}
	else if ( !Q_stricmp( "interrogator", NPCspawner->NPC_type))
	{
		NPC_Interrogator_Precache(NULL);
	}
	else if ( !Q_stricmp( "probe", NPCspawner->NPC_type))
	{
		NPC_Probe_Precache();
	}
	else if ( !Q_stricmp( "seeker", NPCspawner->NPC_type))
	{
		NPC_Seeker_Precache();
	}
	else if ( !Q_stricmp( "remote", NPCspawner->NPC_type))
	{
		NPC_Remote_Precache();
	}
	else if ( !Q_strncmp( "shadowtrooper", NPCspawner->NPC_type, 13 ) )
	{
		NPC_ShadowTrooper_Precache();
	}
	else if ( !Q_stricmp( "minemonster", NPCspawner->NPC_type ))
	{
		NPC_MineMonster_Precache();
	}
	else if ( !Q_stricmp( "howler", NPCspawner->NPC_type ))
	{
		NPC_Howler_Precache();
	}
	else if ( !Q_stricmp( "sentry", NPCspawner->NPC_type ))
	{
		NPC_Sentry_Precache();
	}
	else if ( !Q_stricmp( "protocol", NPCspawner->NPC_type ))
	{
		NPC_Protocol_Precache();
	}
	else if ( !Q_stricmp( "galak_mech", NPCspawner->NPC_type ))
	{
		NPC_GalakMech_Precache();
	}
	else if ( !Q_stricmp( "wampa", NPCspawner->NPC_type ))
	{
		NPC_Wampa_Precache();
	}

	return (NPC_Spawn_Do( NPCspawner ));
}

void NPC_Spawn_f( gentity_t *ent )
{
	char	npc_type[1024];
	char	targetname[1024];
	qboolean	isVehicle = qfalse;

	trap->Argv(2, npc_type, 1024);
	if ( Q_stricmp( "vehicle", npc_type ) == 0 )
	{
		isVehicle = qtrue;
		trap->Argv(3, npc_type, 1024);
		trap->Argv(4, targetname, 1024);
	}
	else
	{
		trap->Argv(3, targetname, 1024);

		// GalaxyRP: [NPC System] a vehicle's own NPC entry: NPC_ParseParms() refuses it, but says so only on
		// the server console ("Shame on you!"), so the admin was left with nothing
		if ( RP_NpcTypeIsVehicle( npc_type ) )
		{
			trap->SendServerCommand( ent-g_entities, "print \"^7That is a vehicle's npc entry: use ^3/npc spawn vehicle <type>^7 (see ^3/list vehicles^7).\n\"" );
			return;
		}
	}

	// GalaxyRP fix: [NPC] a blocklist used to sit here: a guardian_boss_1..10 loop and 29 named
	// types -- every guardian, sage and quest NPC, plus jawa_seller -- each silently refused with
	// a bare return, no message to the admin and no log line. That is zyk's addition; NPC_Spawn_f
	// in OpenJK has no such list. It was there to stop an admin wrecking the Light/Universe quest,
	// and that quest no longer exists: level.guardian_quest, guardian_mode, spawn_boss() and every
	// reward path keyed on these NPCs were removed as dead code in earlier work, so 24 of the 29
	// names had no reference anywhere else in the codebase at all. The list was never a real
	// boundary either -- SP_NPC_spawner has no such check, so "/entadd npc_spawner npc_type
	// guardian_of_chaos" always sidestepped it, and quest_mage was missing from this list while
	// being present in the kill one. Removed; these are now ordinary NPCs, and /npc kill removes
	// them like any other. Note that TaystJK has its own, unrelated blocklist here (ragnos,
	// saber_droid, saber_droid_training) for client crashes rather than for quests; we carry
	// zyk's data-level fix for the two saber_droid types in zyk_npc_fix.npc, but nothing for
	// ragnos.

	NPC_SpawnType( ent, npc_type, targetname, isVehicle );
}

/*
NPC_Kill_f
*/
extern stringID_table_t TeamTable[];

/*
GalaxyRP fix: [NPC] resolve a team name written either way.

The two subcommands that take a team disagreed on spelling. /npc team compared against the short
names ("player", "enemy", ...) while /npc kill team went through GetIDForString(TeamTable, ...),
whose entries are built with ENUM2STRING and so are the full enum names ("NPCTEAM_ENEMY", ...).
So "/npc kill team enemy" -- the obvious thing to type, and what the /npc usage listing shows for
the other subcommand -- matched nothing. Both now accept either spelling.

Returns -1 when the name is not a team. NPCTEAM_FREE is a real team whose value is 0, so it cannot
double as the failure value: that conflation is exactly what made "kill team enemy" fail in
silence. GetIDForString already returns -1 for an unknown string, so the two agree.
*/
static int zyk_team_from_string( const char *name )
{
	if ( !name || !name[0] )
		return -1;

	if ( !Q_stricmp( name, "player" ) )
		return NPCTEAM_PLAYER;
	if ( !Q_stricmp( name, "enemy" ) )
		return NPCTEAM_ENEMY;
	if ( !Q_stricmp( name, "neutral" ) )
		return NPCTEAM_NEUTRAL;
	if ( !Q_stricmp( name, "free" ) )
		return NPCTEAM_FREE;

	return GetIDForString( TeamTable, name );
}

// GalaxyRP: [NPC System] the team names a player types, for /npc kill team's messages
static const char *RP_NpcTeamWord( int team )
{
	switch ( team )
	{
	case NPCTEAM_PLAYER:	return "player";
	case NPCTEAM_ENEMY:		return "enemy";
	case NPCTEAM_NEUTRAL:	return "neutral";
	default:				return "free";
	}
}

/*
GalaxyRP fix: [NPC System] everything this said went through Com_Printf(), which in the game module is
the server console: the admin who typed /npc kill saw nothing at all -- not the usage, not an unknown
team, not whether anything died. The usage and the errors now go to the admin (ent), and so does one
line at the end saying how many it killed. The per-NPC lines stay on the server console, where they
were, as a record; sent to the admin they would be one server command per NPC, and /npc kill all on
a busy map would overflow the admin's reliable commands.
*/
void NPC_Kill_f( gentity_t *ent )
{
	int			n;
	gentity_t	*player;
	char		name[1024];
	npcteam_t	killTeam = NPCTEAM_FREE;
	// GalaxyRP fix: [NPC] "was a team given?" used to be inferred from killTeam != NPCTEAM_FREE,
	// which cannot express "kill the teamless NPCs" -- NPCTEAM_FREE is a real team. Tracked
	// separately so every one of the four teams is selectable.
	qboolean	killByTeam = qfalse;
	qboolean	killNonSF = qfalse;
	int			killed = 0, ridden = 0;
	const int	clientNum = ent - g_entities;

	trap->Argv(2, name, 1024);

	if ( !name[0] )
	{
		trap->SendServerCommand( clientNum, "print \"^1Command Usage: ^3/npc kill <targetname or type>^7, ^3/npc kill all^7, or ^3/npc kill team <player/enemy/neutral/free or nonally>^7 ('nonally' is every npc but your allies). ^3/npc killaim ^7kills the one in your crosshair.\n\"" );
		return;
	}

	if ( Q_stricmp( "team", name ) == 0 )
	{
		trap->Argv(3, name, 1024);

		if ( !name[0] )
		{
			trap->SendServerCommand( clientNum, "print \"^1Command Usage: ^3/npc kill team <player/enemy/neutral/free or nonally>^7 ('nonally' is every npc but your allies)\n\"" );
			return;
		}

		if ( Q_stricmp( "nonally", name ) == 0 )
		{
			killNonSF = qtrue;
		}
		else
		{
			// GalaxyRP fix: [NPC] this tested the result against NPCTEAM_FREE (0) while the lookup
			// reports failure as -1, so an unknown team name skipped the error entirely and left
			// killTeam at -1 -- matching no NPC, killing nothing, and saying nothing. The same
			// confusion rejected "NPCTEAM_FREE" itself as unrecognised.
			int resolvedTeam = zyk_team_from_string( name );

			if ( resolvedTeam == -1 )
			{
				trap->SendServerCommand( clientNum, "print \"^1That is not a team. ^7Teams: ^3player^7, ^3enemy^7, ^3neutral^7, ^3free^7, or ^3nonally ^7for every npc but your allies.\n\"" );
				return;
			}

			killTeam = (npcteam_t)resolvedTeam;
			killByTeam = qtrue;
		}
	}

	for ( n = 1; n < level.num_entities; n++)
	{ // zyk: changed from ENTITYNUM_MAX_NORMAL to level.num_entities
		player = &g_entities[n];
		if (!player->inuse) {
			continue;
		}
		// GalaxyRP: [NPC System] a corpse is not killed again: player_die() returns at once for one
		// already PM_DEAD, so this changes nothing but the count the admin is told
		if ( player->client && player->client->ps.pm_type == PM_DEAD ) {
			continue;
		}

		// GalaxyRP fix: [NPC] a 31-name blocklist used to sit here, skipping every guardian, sage
		// and quest NPC type plus anything carrying the custom-quest flag, so "/npc kill all" quietly
		// left them alive. That is zyk's addition -- NPC_Kill_f in OpenJK and in TaystJK has no such
		// list and kills every NPC, and the newer Zyk mod has since removed it again -- and the
		// intent was reasonable: stop a passing admin from breaking someone's quest. It had no way
		// out, though. It matched on NPC_type, so it protected those NPCs whether or not a quest was
		// running; there was no override; and combined with /entadd being able to spawn a
		// guardian_boss directly it meant an admin could create an NPC that nothing in the mod could
		// remove, short of a map change. Reverted to the upstream behaviour: /npc kill means what it
		// says. Everything below this point -- the team selection, the targetname matching and the
		// pilot guard -- is GalaxyRP's own and is unchanged.

		// DAJ_RP fix: [NPC System] "nonally": every NPC not on the player team -- NPCs only (player->NPC), as the other
		// kinds below. Stock tested any client, so it killed real players whose NPC team was not "player": in Siege
		// every Imperial (g_client.c gives them NPCTEAM_ENEMY, so enemy NPCs side with them) but slot 0, the loop
		// starting at 1. A vehicle someone rides is left alone and counted, as "/npc kill all" leaves it. And it no
		// longer frees NPC spawners (stock removed any entity with an NPC_type): the Entity System's are the
		// admins' placed content, which /entsave would then have lost; "/npc kill all" keeps them too.
		if ( killNonSF )
		{
			if ( player->NPC && player->client && player->client->playerTeam != NPCTEAM_PLAYER )
			{
				if ( player->m_pVehicle && player->m_pVehicle->m_pPilot )
				{
					ridden++;
				}
				else
				{
					Com_Printf( S_COLOR_GREEN"Killing NPC %s named %s\n", player->NPC_type, player->targetname );
					player->health = 0;
					killed++;

					if ( player->die )
					{
						player->die( player, player, player, player->client->pers.maxHealth, MOD_UNKNOWN );
					}
				}
			}
		}
		else if ( player && player->NPC && player->client )
		{
			if ( killByTeam )
			{
				if ( player->client->playerTeam == killTeam )
				{
					Com_Printf( S_COLOR_GREEN"Killing NPC %s named %s\n", player->NPC_type, player->targetname );
					player->health = 0;
					killed++;
					if (player->die)
					{
						player->die(player, player, player, player->client->pers.maxHealth, MOD_UNKNOWN);
					}
				}
			}
			// GalaxyRP fix: [NPC] this matched NPC_type only. NPC_Spawn_f has always accepted a name
			// ("/npc spawn stormtrooper johnny") and NPC_SpawnType stores it as the NPC's targetname,
			// but nothing ever matched on it, so a named NPC could not be killed by its name -- the
			// upstream TaystJK/vanilla code matches targetname and zyk swapped it for NPC_type.
			// Trying targetname first and falling back to NPC_type keeps both: "/npc kill johnny"
			// kills that one NPC, "/npc kill stormtrooper" still clears the whole type. Targetname
			// wins a tie because it is the name an admin deliberately chose.
			else if( (player->targetname && Q_stricmp( name, player->targetname ) == 0)
				|| (player->NPC_type && Q_stricmp( name, player->NPC_type ) == 0)
				|| Q_stricmp( name, "all" ) == 0 )
			{
				if (!(player->m_pVehicle && player->m_pVehicle->m_pPilot))
				{ // zyk: do not kill vehicles that have pilots
					Com_Printf( S_COLOR_GREEN"Killing NPC %s named %s\n", player->NPC_type, player->targetname );
					player->health = 0;
					player->client->ps.stats[STAT_HEALTH] = 0;
					killed++;
					if (player->die)
					{
						player->die(player, player, player, 100, MOD_UNKNOWN);
					}
				}
				else
				{
					ridden++;
				}
			}
		}
		/*
		else if ( player && (player->r.svFlags&SVF_NPC_PRECACHE) )
		{//a spawner
			Com_Printf( S_COLOR_GREEN"Removing NPC spawner %s named %s\n", player->NPC_type, player->targetname );
			G_FreeEntity( player );
		}
		*/
		//rwwFIXMEFIXME: should really do something here.
	}

	// what it did, to the admin
	if ( killNonSF )
	{
		trap->SendServerCommand( clientNum, va( "print \"^7Killed %d npc%s that %s not your allies%s.\n\"",
			killed, killed == 1 ? "" : "s", killed == 1 ? "is" : "are",
			ridden ? va( "; %d vehicle%s with someone riding %s left alone", ridden, ridden == 1 ? "" : "s", ridden == 1 ? "it was" : "them were" ) : "" ) );
	}
	else if ( killByTeam )
	{
		trap->SendServerCommand( clientNum, va( "print \"^7Killed %d npc%s of team ^3%s^7.\n\"",
			killed, killed == 1 ? "" : "s", RP_NpcTeamWord( killTeam ) ) );
	}
	else
	{
		char shown[64];

		// the name as typed, shown back: no quotes (they would end the print), short
		Q_strncpyz( shown, name, sizeof( shown ) );
		for ( n = 0; shown[n]; n++ )
		{
			if ( shown[n] == '"' || (unsigned char)shown[n] < 32 )
				shown[n] = ' ';
		}

		if ( !killed && !ridden )
		{
			trap->SendServerCommand( clientNum, Q_stricmp( name, "all" ) ? va( "print \"^7No npc has the targetname or type ^3%s^7.\n\"", shown )
				: "print \"^7There are no npcs to kill.\n\"" );
		}
		else
		{
			trap->SendServerCommand( clientNum, va( "print \"^7Killed %d npc%s%s.\n\"", killed, killed == 1 ? "" : "s",
				ridden ? va( "; %d vehicle%s with someone riding %s left alone", ridden, ridden == 1 ? "" : "s", ridden == 1 ? "it was" : "them were" ) : "" ) );
		}
	}
}

// GalaxyRP: [NPC System] NPC_PrintScore() used to be here -- see the note where /npc score was, in
// Cmd_NPC_f() below.

/*
Svcmd_NPC_f

parse and dispatch bot commands
*/
/*
==================
RP_NpcInCrosshair

GalaxyRP: [NPC System] the live NPC under the admin's crosshair, for /npc team, /npc effect and
/npc killaim. Vehicles are NPCs too, and are found the same way -- but not the one the admin is
riding, whose box the eye is inside of: it would be "hit" at once and hide everything ahead.

/npc team used to take ps.hasLookTarget / ps.lookTarget, which is not the crosshair at all:
WP_SaberStartMissileBlockCheck() sets it to whatever living body is within 256 units and in view,
so it could name a bystander instead of the NPC being aimed at, and nothing beyond 256 units. And
no ordinary trace can find an NPC that spawned non-solid -- the NOTSOLID spawnflag gives it contents
0 -- which is exactly the NPC /npc effect clear exists for.

So: one trace from the eye along the view to the first wall (MASK_SOLID, so bodies do not stop it),
then the same ray against the real box of every live NPC, nearest hit in front of that wall wins.
Solid or not, any distance up to RP_NPC_AIM_RANGE, never through walls. Dead NPCs and NPCs that are
not drawn (EF_NODRAW, SVF_NOCLIENT -- scripted invisibility) are skipped: nobody can be aiming at
them, and neither is an NPC not yet linked into the world. If the eye itself is inside solid
(noclip), walls are ignored rather than finding nothing.
==================
*/
#define RP_NPC_AIM_RANGE	8192.0f

static qboolean RP_RayHitsBox( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, float *frac )
{
	float tmin = 0.0f, tmax = 1.0f;
	int k;

	for ( k = 0; k < 3; k++ )
	{
		const float d = end[k] - start[k];

		if ( fabs( d ) < 0.0001f )
		{ // parallel to this pair of faces: inside the slab or a miss
			if ( start[k] < mins[k] || start[k] > maxs[k] )
			{
				return qfalse;
			}
		}
		else
		{
			float t1 = ( mins[k] - start[k] ) / d;
			float t2 = ( maxs[k] - start[k] ) / d;

			if ( t1 > t2 )
			{
				const float tmp = t1;
				t1 = t2;
				t2 = tmp;
			}
			if ( t1 > tmin )
			{
				tmin = t1;
			}
			if ( t2 < tmax )
			{
				tmax = t2;
			}
			if ( tmin > tmax )
			{
				return qfalse;
			}
		}
	}

	*frac = tmin;
	return qtrue;
}

static gentity_t *RP_NpcInCrosshair( gentity_t *ent )
{
	vec3_t eye, fwd, end;
	trace_t tr;
	gentity_t *best = NULL;
	float bestFrac;
	int i;

	VectorCopy( ent->client->ps.origin, eye );
	eye[2] += ent->client->ps.viewheight;
	AngleVectors( ent->client->ps.viewangles, fwd, NULL, NULL );
	VectorMA( eye, RP_NPC_AIM_RANGE, fwd, end );

	trap->Trace( &tr, eye, NULL, NULL, end, ent->s.number, MASK_SOLID, qfalse, 0, 0 );
	bestFrac = tr.startsolid ? 1.0f : tr.fraction;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		gentity_t *npc = &g_entities[i];
		vec3_t mins, maxs;
		float frac;

		if ( !npc->inuse || !npc->r.linked || !npc->client || !npc->NPC || npc->s.eType != ET_NPC )
			continue;
		if ( npc->health <= 0 || ( npc->s.eFlags & ( EF_DEAD | EF_NODRAW ) ) || ( npc->r.svFlags & SVF_NOCLIENT ) )
			continue;
		if ( ent->client->ps.m_iVehicleNum && npc->s.number == ent->client->ps.m_iVehicleNum )
			continue;

		VectorAdd( npc->r.currentOrigin, npc->r.mins, mins );
		VectorAdd( npc->r.currentOrigin, npc->r.maxs, maxs );

		if ( RP_RayHitsBox( eye, end, mins, maxs, &frac ) && frac < bestFrac )
		{
			bestFrac = frac;
			best = npc;
		}
	}

	return best;
}

// "^3stormtrooper ^7(johnny)" -- the NPC type, and its targetname when it has one.
// GalaxyRP fix: [security] with any "@@@" run broken up (RP_BreakStringEdRefs, g_utils.c): the
// targetname is whatever the admin who spawned it typed, and the client expands a run of three '@' in
// a printed line as a server phrase, which the stock client can overflow a buffer doing. Only the
// label changes; the NPC keeps its name.
static void RP_NpcLabel( const gentity_t *npc, char *buf, int size )
{
	const char *type = ( npc->NPC_type && npc->NPC_type[0] ) ? npc->NPC_type : "npc";

	if ( npc->targetname && npc->targetname[0] )
		Com_sprintf( buf, size, "^3%s ^7(%s)", type, npc->targetname );
	else
		Com_sprintf( buf, size, "^3%s^7", type );

	RP_BreakStringEdRefs( buf );
}

/*
==================
RP_NpcKillAim_f

GalaxyRP: [NPC System] /npc killaim: kills the NPC or vehicle in the crosshair (RP_NpcInCrosshair()),
the way /npc kill does -- it dies by its own hand, so nobody is credited with the kill. A vehicle
with a player aboard, driving or riding, is left alone, as /npc kill leaves ridden vehicles; one
with only NPCs aboard is killed like any other NPC.
==================
*/
static qboolean RP_VehicleHasPlayerAboard( const gentity_t *veh )
{
	const Vehicle_t *pVeh = veh->m_pVehicle;
	int i;

	if ( !pVeh )
		return qfalse;
	if ( pVeh->m_pPilot && ( (gentity_t *)pVeh->m_pPilot )->s.number < MAX_CLIENTS )
		return qtrue;
	for ( i = 0; i < VEH_MAX_PASSENGERS; i++ )
	{
		if ( pVeh->m_ppPassengers[i] && ( (gentity_t *)pVeh->m_ppPassengers[i] )->s.number < MAX_CLIENTS )
			return qtrue;
	}
	return qfalse;
}

static void RP_NpcKillAim_f( gentity_t *ent )
{
	char label[MAX_STRING_CHARS];
	gentity_t *npc = RP_NpcInCrosshair( ent );

	if ( !npc )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1No npc or vehicle in your crosshair.\n\"" );
		return;
	}

	RP_NpcLabel( npc, label, sizeof( label ) );

	if ( RP_VehicleHasPlayerAboard( npc ) )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7has a player aboard, so it is left alone.\n\"", label ) );
		return;
	}

	Com_Printf( S_COLOR_GREEN"Killing NPC %s named %s\n", npc->NPC_type, npc->targetname );
	npc->health = 0;
	npc->client->ps.stats[STAT_HEALTH] = 0;
	if ( npc->die )
	{
		npc->die( npc, npc, npc, 100, MOD_UNKNOWN );
	}
	trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7killed.\n\"", label ) );
}

/*
==================
RP_NpcEffect_f

GalaxyRP: [NPC System] /npc effect <holo|ghost|walkthrough|clear> on the NPC in the crosshair -- the
NPC side of /admholo, /admghost and /admwalkthrough (g_cmds.c), and the same pers.phase_mode underneath,
so an NPC in a mode behaves exactly like a phased player: hit, damaged and targeted like any other,
only movement passes through (RP_PhasePassesThrough(), g_active.c), and ghost/holo get the client
plugin's look and stay off the radar.

Not a toggle: holo, ghost and walkthrough set that mode (switching straight from one to another is
fine), and only clear takes it off. Clearing goes through the same release as a player's, so an NPC
cleared while standing inside somebody stays walk-through until the two are apart.

An NPC can also be non-solid from the map, outside this system: the NOTSOLID spawnflag gives it
contents 0 (nothing can hit it either), and a script's SET_SOLID false gives it CONTENTS_CORPSE.
Any of these commands makes it a normal CONTENTS_BODY again -- holo/ghost/walkthrough into this
system's walk-through-but-hittable body, clear into a plain solid one (through the release, so it
cannot trap anyone). A script that later sets it non-solid again wins, as it would have anyway.

Vehicles are refused -- their physics and riders are a different beast -- and so is an NPC riding
one. The mode lasts until clear, the NPC's removal, or a map change; the next NPC in that slot
starts with a zeroed client.

DAJ_RP: [NPC System] the walk-through mode is "walkthrough", and is named so in the replies: it was "nonsolid"
("is now non-solid"), which read as the NOTSOLID spawnflag -- an NPC nothing touches at all -- when this one is
hit like any other. /admwalkthrough on a player (was /admsolid) uses the same word (RP_PhaseModeName(), g_cmds.c).
There is no /npc effect for /admhidden: an NPC cannot be hidden.
==================
*/
// DAJ_RP: [NPC System] what /npc effect calls a mode in its replies -- since /admsolid became /admwalkthrough,
// the same word a player's mode is given
static const char *RP_NpcEffectName( int mode )
{
	return RP_PhaseModeName( mode );
}

static void RP_NpcEffect_f( gentity_t *ent )
{
	char arg[MAX_STRING_CHARS];
	char label[MAX_STRING_CHARS];
	gentity_t *npc;
	int mode;
	qboolean mapNonSolid;

	trap->Argv( 2, arg, sizeof( arg ) );

	if ( trap->Argc() != 3 )
		mode = -1;
	else if ( !Q_stricmp( arg, "holo" ) )
		mode = RP_PHASE_HOLO;
	else if ( !Q_stricmp( arg, "ghost" ) )
		mode = RP_PHASE_GHOST;
	else if ( !Q_stricmp( arg, "walkthrough" ) )
		mode = RP_PHASE_NONSOLID;
	else if ( !Q_stricmp( arg, "clear" ) )
		mode = RP_PHASE_NONE;
	else
		mode = -1;

	if ( mode == -1 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/npc effect ^2<holo/ghost/walkthrough/clear>\n^7Aim at the NPC. The effect stays until ^3/npc effect clear^7, which also makes an NPC that spawned non-solid solid.\n\"" );
		return;
	}

	npc = RP_NpcInCrosshair( ent );

	if ( !npc )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1No NPC in your crosshair.\n\"" );
		return;
	}

	RP_NpcLabel( npc, label, sizeof( label ) );

	if ( npc->client->NPC_class == CLASS_VEHICLE || npc->s.NPC_class == CLASS_VEHICLE )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"^1Vehicles cannot be given an effect (%s^1).\n\"", label ) );
		return;
	}

	if ( npc->client->ps.m_iVehicleNum )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"^1%s ^1is riding a vehicle.\n\"", label ) );
		return;
	}

	// a live NPC is CONTENTS_BODY unless the map or a script made it non-solid
	mapNonSolid = ( npc->r.contents != CONTENTS_BODY ) ? qtrue : qfalse;

	if ( mode == RP_PHASE_NONE )
	{
		if ( npc->client->pers.phase_mode == RP_PHASE_NONE && !mapNonSolid )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7has no effect to clear.\n\"", label ) );
			return;
		}

		npc->client->pers.phase_mode = RP_PHASE_NONE;
		npc->client->pers.phase_releasing = qtrue;
	}
	else
	{
		if ( npc->client->pers.phase_mode == mode && !mapNonSolid )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is already %s.\n\"", label, RP_NpcEffectName( mode ) ) );
			return;
		}

		npc->client->pers.phase_mode = mode;
		npc->client->pers.phase_releasing = qfalse;
	}

	if ( mapNonSolid )
	{ // back to a normal body; the mode (or the release) keeps it walk-through for now
		npc->r.contents = CONTENTS_BODY;
		npc->clipmask |= CONTENTS_BODY;
	}

	RP_PhaseTrackNpc( npc );
	trap->LinkEntity( (sharedEntity_t *)npc );

	if ( mode == RP_PHASE_NONE )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is back to normal%s.\n\"", label, mapNonSolid ? " and solid" : "" ) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is now %s%s.\n\"", label, RP_NpcEffectName( mode ),
			mapNonSolid ? " ^7(it spawned non-solid; it can be hit now)" : "" ) );
	}

	G_LogPrintf( "npc effect: %s ^7made NPC %s (%d) %s\n", ent->client->pers.netname,
		( npc->NPC_type && npc->NPC_type[0] ) ? npc->NPC_type : "npc", npc->s.number,
		( mode == RP_PHASE_NONE ) ? "normal" : RP_NpcEffectName( mode ) );
}

/*
==================
RP_NpcIsFrozen / RP_NpcFreeze / RP_NpcUnfreeze

DAJ_RP: [NPC System] /npc freeze and the spawner key "npcfreeze". A frozen NPC is a statue that still has its
physics: it falls, is pushed, pulled, gripped and dragged, rides lifts, is hurt and can be killed -- what the
engine's own NPC freeze does (SVF_ICARUS_FREEZE, set by a map script's set_icarus_freeze, or d_npcfreeze). That
flag is the base: NPC_Think() (NPC.c) runs only the NPC's physics while it is set, and the engine pauses the
NPC's ICARUS scripts, which carry on when it is lifted. gNPC_t::rpFrozen is ours on top of it, so that neither
a map script's set_icarus_freeze false nor the friendly-fire turn in NPC_Pain() (both clear the flag) can let
the NPC go: NPC_Think() puts the flag back each think while rpFrozen is set.

What the AI being off does not cover, and is closed separately so the statue does not defend itself:
  - WP_SaberStartMissileBlockCheck() (w_saber.c) runs for every NPC each frame, outside its AI: it takes a
    shot's owner as its enemy, lights the saber to block it, drops a ceiling-ambush Jedi, and Force-pushes or
    jumps away from a thermal detonator. Skipped while frozen.
  - WP_SaberCanBlock() (w_saber.c): blocking a saber hit or a missile with the body. Refused while frozen.
  - A shot striking the lit blade itself (G_MissileImpact(), g_missile.c) dies on it: no block move, no
    deflect or reflect, whatever the NPC's Saber Defense.
  - A saber striking the lit blade (CheckSaberDamage(), w_saber.c) is stopped by it, with the clash spark, but
    the NPC does not answer: no parry, knockaway or broken parry, and no saber lock (WP_SabersCheckLock()).
  - Jedi_DodgeEvasion() (w_force.c): dodging a disruptor or other instant hit. Refused while frozen.
  - CanCounterThrow() (w_force.c): pushing or pulling back against a Force push or pull. Refused while
    frozen, so it is thrown like anyone who cannot resist.
  - NPC_Use() (NPC_reactions.c) and the follower claim in TryUse() (g_utils.c): being used does nothing; a
    leader can still dismiss a frozen follower.
Its saber is left as it was, lit or not, and a lit one still does touch damage where the server allows it.

RP_NpcFreeze() also ends what the NPC had under way: a Force power (a gripped player goes free), a rancor's
held victim (as player_die() drops it), and a weapon charge, which PM_Weapon() would otherwise fire as soon as
the attack button is seen up -- one last shot. rpFreezeOwnsIcarus remembers whether the flag was ours to set:
when a map script had the NPC frozen already, RP_NpcUnfreeze() leaves its freeze in place.

The freeze ends with RP_NpcUnfreeze(): /npc freeze again, the NPC's death (player_die(), g_combat.c, so its
deathscript runs), or the NPC going away; the next NPC in the slot starts with a zeroed gNPC_t.
==================
*/
qboolean RP_NpcIsFrozen( const gentity_t *ent )
{
	return ( ent && ent->NPC && ent->NPC->rpFrozen ) ? qtrue : qfalse;
}

void RP_NpcFreeze( gentity_t *npc )
{
	int i;

	if ( !npc || !npc->NPC || !npc->client || npc->NPC->rpFrozen )
	{
		return;
	}

	npc->NPC->rpFrozen = qtrue;

	if ( npc->r.svFlags & SVF_ICARUS_FREEZE )
	{ // a map script froze it first: that freeze is the script's to lift
		npc->NPC->rpFreezeOwnsIcarus = qfalse;
	}
	else
	{
		npc->r.svFlags |= SVF_ICARUS_FREEZE;
		npc->NPC->rpFreezeOwnsIcarus = qtrue;
	}

	for ( i = 0; i < NUM_FORCE_POWERS; i++ )
	{
		if ( npc->client->ps.fd.forcePowersActive & ( 1 << i ) )
		{
			WP_ForcePowerStop( npc, (forcePowers_t)i );
		}
	}

	if ( npc->s.NPC_class == CLASS_RANCOR && npc->activator && npc->activator->client
		&& ( npc->activator->client->ps.eFlags2 & EF2_HELD_BY_MONSTER ) )
	{
		Rancor_DropVictim( npc );
	}

	if ( npc->client->ps.weaponstate == WEAPON_CHARGING || npc->client->ps.weaponstate == WEAPON_CHARGING_ALT )
	{
		npc->client->ps.weaponstate = WEAPON_READY;
	}
}

void RP_NpcUnfreeze( gentity_t *npc )
{
	if ( !npc || !npc->NPC || !npc->NPC->rpFrozen )
	{
		return;
	}

	npc->NPC->rpFrozen = qfalse;

	if ( npc->NPC->rpFreezeOwnsIcarus )
	{
		npc->r.svFlags &= ~SVF_ICARUS_FREEZE;
	}

	npc->NPC->rpFreezeOwnsIcarus = qfalse;
}

/*
==================
RP_NpcFreeze_f

DAJ_RP: [NPC System] /npc freeze: freezes the NPC in the crosshair (RP_NpcInCrosshair()), or unfreezes it when
it is frozen already -- see RP_NpcFreeze() above for what that means. Vehicles are refused, and an NPC riding
one, as /npc effect refuses them. Freezing also stops the NPC turning: it would otherwise finish turning to
where its AI last meant to face (NPC_UpdateAngles() still runs). Done here rather than in RP_NpcFreeze(): its
other caller, NPC_Spawn_Do(), runs before the NPC has an AI to have meant anything, and NPC_Begin() then takes
the NPC's starting angles from the desiredYaw NPC_Spawn_Do() gave it.
==================
*/
static void RP_NpcFreeze_f( gentity_t *ent )
{
	char label[MAX_STRING_CHARS];
	gentity_t *npc;
	qboolean frozen;

	if ( trap->Argc() != 2 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/npc freeze\n^7Aim at the NPC. Use again to unfreeze it.\n\"" );
		return;
	}

	npc = RP_NpcInCrosshair( ent );

	if ( !npc )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1No NPC in your crosshair.\n\"" );
		return;
	}

	RP_NpcLabel( npc, label, sizeof( label ) );

	if ( npc->client->NPC_class == CLASS_VEHICLE || npc->s.NPC_class == CLASS_VEHICLE )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"^1Vehicles cannot be frozen (%s^1).\n\"", label ) );
		return;
	}

	if ( npc->client->ps.m_iVehicleNum )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"^1%s ^1is riding a vehicle.\n\"", label ) );
		return;
	}

	if ( npc->NPC->rpFrozen )
	{
		RP_NpcUnfreeze( npc );
		frozen = qfalse;
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is no longer frozen%s.\n\"", label,
			( npc->r.svFlags & SVF_ICARUS_FREEZE ) ? " ^7(a map script still holds it frozen)" : "" ) );
	}
	else
	{
		RP_NpcFreeze( npc );
		npc->NPC->desiredYaw = npc->NPC->lockedDesiredYaw = npc->client->ps.viewangles[YAW];
		npc->NPC->desiredPitch = npc->NPC->lockedDesiredPitch = npc->client->ps.viewangles[PITCH];
		frozen = qtrue;
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is now frozen.\n\"", label ) );
	}

	G_LogPrintf( "npc freeze: %s ^7%s NPC %s (%d)\n", ent->client->pers.netname, frozen ? "froze" : "unfroze",
		( npc->NPC_type && npc->NPC_type[0] ) ? npc->NPC_type : "npc", npc->s.number );
}

// GalaxyRP fix: [NPC] "qboolean showBBoxes = qfalse;" used to live here, the flag behind
// "/npc showbounds". The subcommand, its help line and everything that read the flag are gone --
// see the comment where the subcommand was, in Cmd_NPC_f() below.
void Cmd_NPC_f( gentity_t *ent )
{
	char	cmd[1024];

	if (!(ent->client->pers.bitvalue & (1 << ADM_NPC)))
	{ // zyk: npc admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	// GalaxyRP fix: [Admin] a zyk_allow_adm_in_other_gametypes gate used to sit here, refusing this
	// command outside FFA when the cvar was 0. The cvar defaulted to 1 and was never set in any
	// shipped config, so the gate never fired; it has been removed along with the cvar itself.

	// GalaxyRP fix: [Guardian] a "cant spawn npcs while someone is in a guardian
	// battle" loop, gated on client->pers.guardian_mode > 0, used to live here.
	// guardian_mode is permanently 0 (its only setter, spawn_boss(), was deleted
	// as unreachable dead code), so the loop was dead and has been removed.

	trap->Argv( 1, cmd, 1024 );

	if ( Q_stricmp( cmd, "spawn" ) == 0 )
	{
		NPC_Spawn_f( ent );
	}
	else if ( Q_stricmp( cmd, "kill" ) == 0 )
	{
		NPC_Kill_f( ent );
	}
	else if ( Q_stricmp( cmd, "killaim" ) == 0 )
	{
		RP_NpcKillAim_f( ent );
	}
	// GalaxyRP fix: [NPC] "showbounds" used to be here. It toggled showBBoxes, and in multiplayer
	// that flag could never draw anything:
	//
	//   NPC_ShowDebugInfo() (NPC.c), the function that walked the NPCs and drew a box around each
	//   one, had no callers at all. Singleplayer calls it at the end of G_RunFrame, in the
	//   "//DEBUG STUFF" block beside NAV::ShowDebugInfo() (code/game/g_main.cpp); that block was
	//   never ported to multiplayer.
	//
	//   And it could not have worked if it had been. Singleplayer's copy calls CG_Cube() directly,
	//   because there the game and the renderer are one process. Multiplayer's game module has no
	//   renderer, so Raven left G_Cube(), G_Line(), G_CubeOutline() and the rest as empty stubs in
	//   g_nav.c, with the comment "rwwFIXMEFIXME: Write these at some point for the sake of being
	//   able to debug visually". They are still empty in OpenJK, in TaystJK and in the newer Zyk
	//   mod -- this is inherited dead code, not something that broke here.
	//
	// So the command toggled a flag nothing polled, which would have called a function that does
	// nothing, and printed not a word either way. Removed along with the flag, the function and the
	// one other reader (the jump-state box in NPC_BSJump, NPC_behavior.c). "/npc showbounds" is now
	// an unrecognised subcommand, which gets the usage listing (the final else below).
	//
	// Drawing boxes in multiplayer is possible -- CG_Cube() works client-side and is live behind
	// cg_showVehBounds, and G_TestLine() broadcasts debug lines that CG_TestLine() renders, live
	// behind bot_wp_edit -- but it is a new feature, not a repair, so nothing is left here
	// pretending to be one.
	// GalaxyRP: [NPC System] "score" used to be here, with NPC_PrintScore(): Raven's debug listing of
	// ps.persistant[PERS_SCORE] -- kills -- for every entity with a client, printed to the server
	// console. It could only ever print 0: the kill-score line in AddScore() (g_combat.c) has been
	// commented out since the scoreboard switched to counting deaths, and nothing else raises
	// PERS_SCORE. It also listed every player slot, connected or not, as "(null): 0". Removed, with
	// its lines in the usage listing, /adminlist and /list commands; "/npc score" now gets the usage.
	else if ( Q_stricmp( cmd, "team" ) == 0 )
	{ // zyk: new option
		// GalaxyRP fix: [NPC System] this acted on ps.lookTarget, which is not the crosshair -- see
		// RP_NpcInCrosshair() -- and said nothing at all, whether it worked, found no NPC or did not
		// know the team. It now takes the NPC in the crosshair and reports each outcome.
		char		cmd2[1024];
		gentity_t	*thisent;
		char		label[MAX_STRING_CHARS];
		int			newTeam;

		// GalaxyRP fix: [Guardian] a "NPC team cannot be used in bosses" guard, gated on
		// client->pers.guardian_invoked_by_id != -1, used to live here. guardian_invoked_by_id is
		// permanently -1 (its only setter, spawn_boss(), was deleted as unreachable dead code), so the
		// guard was dead and has been removed.

		// GalaxyRP fix: [Quests] a "NPC team cannot be used in the Guardian of Map" guard, gated on
		// level.guardian_quest, used to live here. Cmd_GuardianQuest_f (its only setter) was deleted
		// as unreachable dead code (see the GalaxyRP fix comment in g_cmds.c), and
		// level.guardian_quest has been removed along with it, so this guard is removed too.

		// GalaxyRP fix: [NPC] this compared against the short names only, while /npc kill team
		// accepted only the full NPCTEAM_* spellings. Both subcommands now take either form.
		trap->Argv( 2, cmd2, sizeof( cmd2 ) );
		newTeam = ( trap->Argc() == 3 ) ? zyk_team_from_string( cmd2 ) : -1;

		if ( newTeam != NPCTEAM_PLAYER && newTeam != NPCTEAM_ENEMY && newTeam != NPCTEAM_NEUTRAL && newTeam != NPCTEAM_FREE )
		{
			trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/npc team ^2<player/enemy/neutral/free>\n^7Aim at the NPC.\n\"" );
			return;
		}

		thisent = RP_NpcInCrosshair( ent );

		if ( !thisent )
		{
			trap->SendServerCommand( ent-g_entities, "print \"^1No NPC in your crosshair.\n\"" );
			return;
		}

		// GalaxyRP: [Entity System] shared with the spawner key "npcteam" -- see RP_NpcSetTeam()
		RP_NpcSetTeam( thisent, newTeam );

		RP_NpcLabel( thisent, label, sizeof( label ) );
		trap->SendServerCommand( ent-g_entities, va( "print \"%s ^7is now on team %s.\n\"", label, cmd2 ) );
	}
	else if ( Q_stricmp( cmd, "effect" ) == 0 )
	{
		RP_NpcEffect_f( ent );
	}
	else if ( Q_stricmp( cmd, "freeze" ) == 0 )
	{ // DAJ_RP: [NPC System] see RP_NpcFreeze_f()
		RP_NpcFreeze_f( ent );
	}
	else
	{ // GalaxyRP fix: [NPC System] no subcommand, or one that does not exist: the listing. It used to go
	  // to the server console (Com_Printf), so the admin who typed /npc saw nothing at all, and an
	  // unknown subcommand -- a typo, or a removed one -- was ignored in silence.
		trap->SendServerCommand( ent-g_entities, "print \"Valid NPC commands are:\n\
 spawn [NPC type (from NPCs.cfg)]\n\
 kill [NPC targetname] or [all(kills all NPCs)] or 'team [teamname]'\n\
 killaim (kills the NPC or vehicle in your crosshair)\n\
 team [team (player or enemy or neutral or free)] (the NPC in your crosshair)\n\
 effect [holo or ghost or walkthrough or clear] (the NPC in your crosshair)\n\
 freeze (freezes the NPC in your crosshair, or unfreezes it)\n\"" );
	}
}
