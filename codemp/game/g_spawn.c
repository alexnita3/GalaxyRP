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

qboolean	G_SpawnString( const char *key, const char *defaultString, char **out ) {
	int		i;

	if ( !level.spawning ) {
		*out = (char *)defaultString;
//		trap->Error( ERR_DROP, "G_SpawnString() called while not spawning" );
	}

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		if ( !Q_stricmp( key, level.spawnVars[i][0] ) ) {
			*out = level.spawnVars[i][1];
			return qtrue;
		}
	}

	*out = (char *)defaultString;
	return qfalse;
}

// GalaxyRP fix: [Entity System] see RP_FINITE in g_local.h: a value that is not a number keeps the
// default, and the key is logged -- once per key per spawn, which is as often as it can happen
static qboolean RP_FiniteKey( const char *key, const char *value, float v )
{
	if ( RP_FINITE( v ) )
		return qtrue;

	G_LogPrintf( "spawn key \"%s\" = \"%s\" is not a number; default kept\n", key ? key : "", value ? value : "" );
	return qfalse;
}

static qboolean RP_FiniteVecKey( const char *key, const char *value, const vec3_t v )
{
	if ( RP_FINITE( v[0] ) && RP_FINITE( v[1] ) && RP_FINITE( v[2] ) )
		return qtrue;

	G_LogPrintf( "spawn key \"%s\" = \"%s\" is not a number; default kept\n", key ? key : "", value ? value : "" );
	return qfalse;
}

qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atof( s );
	if ( !RP_FiniteKey( key, s, *out ) )
	{
		*out = atof( defaultString );
		if ( !RP_FINITE( *out ) )
			*out = 0.0f;
		return qfalse;
	}
	return present;
}

qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atoi( s );
	return present;
}

qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	if ( sscanf( s, "%f %f %f", &out[0], &out[1], &out[2] ) != 3 ) {
		trap->Print( "G_SpawnVector: Failed sscanf on %s (default: %s)\n", key, defaultString );
		VectorClear( out );
		return qfalse;
	}
	// GalaxyRP fix: [Entity System] a component that is not a number: the default, or zero
	if ( !RP_FiniteVecKey( key, s, out ) ) {
		if ( s == defaultString || sscanf( defaultString, "%f %f %f", &out[0], &out[1], &out[2] ) != 3 ||
			!RP_FINITE( out[0] ) || !RP_FINITE( out[1] ) || !RP_FINITE( out[2] ) ) {
			VectorClear( out );
		}
		return qfalse;
	}
	return present;
}

qboolean	G_SpawnBoolean( const char *key, const char *defaultString, qboolean *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );

	if ( !Q_stricmp( s, "qtrue" ) || !Q_stricmp( s, "true" ) || !Q_stricmp( s, "yes" ) || !Q_stricmp( s, "1" ) )
		*out = qtrue;
	else if ( !Q_stricmp( s, "qfalse" ) || !Q_stricmp( s, "false" ) || !Q_stricmp( s, "no" ) || !Q_stricmp( s, "0" ) )
		*out = qfalse;
	else
		*out = qfalse;

	return present;
}

//
// fields are needed for spawning from the entity string
//
typedef enum {
	F_INT,
	F_FLOAT,
	F_STRING,			// string on disk, pointer in memory
	F_VECTOR,
	F_ANGLEHACK,
	F_PARM1,			// Special case for parms
	F_PARM2,			// Special case for parms
	F_PARM3,			// Special case for parms
	F_PARM4,			// Special case for parms
	F_PARM5,			// Special case for parms
	F_PARM6,			// Special case for parms
	F_PARM7,			// Special case for parms
	F_PARM8,			// Special case for parms
	F_PARM9,			// Special case for parms
	F_PARM10,			// Special case for parms
	F_PARM11,			// Special case for parms
	F_PARM12,			// Special case for parms
	F_PARM13,			// Special case for parms
	F_PARM14,			// Special case for parms
	F_PARM15,			// Special case for parms
	F_PARM16			// Special case for parms
} fieldtype_t;

typedef struct field_s {
	const char	*name;
	size_t		ofs;
	fieldtype_t	type;
} field_t;

field_t fields[] = {
	{ "alliedteam",				FOFS( alliedTeam ),						F_INT },//for misc_turrets
	{ "angerscript",			FOFS( behaviorSet[BSET_ANGER] ),		F_STRING },//name of script to run
	{ "angle",					FOFS( s.angles ),						F_ANGLEHACK },
	{ "angles",					FOFS( s.angles ),						F_VECTOR },
	{ "angles2",				FOFS( s.angles2 ),						F_VECTOR }, // zyk: added this field
	{ "attackscript",			FOFS( behaviorSet[BSET_ATTACK] ),		F_STRING },//name of script to run
	{ "awakescript",			FOFS( behaviorSet[BSET_AWAKE] ),		F_STRING },//name of script to run
	{ "blockedscript",			FOFS( behaviorSet[BSET_BLOCKED] ),		F_STRING },//name of script to run
	{ "bouncecount",			FOFS( bounceCount ),					F_INT }, // zyk: added this field
	{ "chunksize",				FOFS( mass ),							F_FLOAT },//for func_breakables
	{ "classname",				FOFS( classname ),						F_STRING },
	{ "closetarget",			FOFS( closetarget ),					F_STRING },//for doors
	{ "count",					FOFS( count ),							F_INT },
	{ "deathscript",			FOFS( behaviorSet[BSET_DEATH] ),		F_STRING },//name of script to run
	{ "delay",					FOFS( delay ),							F_INT },
	{ "delayscript",			FOFS( behaviorSet[BSET_DELAYED] ),		F_STRING },//name of script to run
	{ "delayscripttime",		FOFS( delayScriptTime ),				F_INT },//name of script to run
	{ "dmg",					FOFS( damage ),							F_INT },
	{ "ffdeathscript",			FOFS( behaviorSet[BSET_FFDEATH] ),		F_STRING },//name of script to run
	{ "ffirescript",			FOFS( behaviorSet[BSET_FFIRE] ),		F_STRING },//name of script to run
	{ "fleescript",				FOFS( behaviorSet[BSET_FLEE] ),			F_STRING },//name of script to run
	{ "fly_sound_debounce_time",FOFS( fly_sound_debounce_time ),		F_INT }, // zyk: added this field
	{ "fullname",				FOFS( fullName ),						F_STRING },
	{ "genericvalue7",			FOFS( genericValue7 ),					F_INT }, // zyk: added this field
	{ "goaltarget",				FOFS( goaltarget ),						F_STRING },//for siege
	{ "healingclass",			FOFS( healingclass ),					F_STRING },
	{ "healingrate",			FOFS( healingrate ),					F_INT },
	{ "healingsound",			FOFS( healingsound ),					F_STRING },
	{ "health",					FOFS( health ),							F_INT },
	{ "idealclass",				FOFS( idealclass ),						F_STRING },//for siege spawnpoints
	{ "linear",					FOFS( alt_fire ),						F_INT },//for movers to use linear movement
	{ "lock",					FOFS( rpLock ),							F_STRING },//DAJ_RP: [Locks] the account list that may use it (g_locks.c)
	{ "lostenemyscript",		FOFS( behaviorSet[BSET_LOSTENEMY] ),	F_STRING },//name of script to run
	{ "mass",					FOFS( mass ),							F_FLOAT }, // zyk: added this field
	{ "material",				FOFS( material ),						F_INT }, // zyk: added this field
	{ "maxs",					FOFS( r.maxs ),							F_VECTOR }, // zyk: added this field
	{ "message",				FOFS( message ),						F_STRING },
	{ "mindtrickscript",		FOFS( behaviorSet[BSET_MINDTRICK] ),	F_STRING },//name of script to run
	{ "mins",					FOFS( r.mins ),							F_VECTOR }, // zyk: added this field
	{ "model",					FOFS( model ),							F_STRING },
	{ "model2",					FOFS( model2 ),							F_STRING },
	{ "npc_target",				FOFS( NPC_target ),						F_STRING },
	{ "npc_target2",			FOFS( target2 ),						F_STRING },//NPC_spawner only
	{ "npc_target4",			FOFS( target4 ),						F_STRING },//NPC_spawner only
	{ "npc_targetname",			FOFS( NPC_targetname ),					F_STRING },
	{ "npc_type",				FOFS( NPC_type ),						F_STRING },
	{ "numchunks",				FOFS( radius ),							F_FLOAT },//for func_breakables
	{ "opentarget",				FOFS( opentarget ),						F_STRING },//for doors
	{ "origin",					FOFS( s.origin ),						F_VECTOR },
	{ "ownername",				FOFS( ownername ),						F_STRING },
	{ "painscript",				FOFS( behaviorSet[BSET_PAIN] ),			F_STRING },//name of script to run
	{ "paintarget",				FOFS( paintarget ),						F_STRING },//for doors
	{ "parm1",					0,										F_PARM1 },
	{ "parm10",					0,										F_PARM10 },
	{ "parm11",					0,										F_PARM11 },
	{ "parm12",					0,										F_PARM12 },
	{ "parm13",					0,										F_PARM13 },
	{ "parm14",					0,										F_PARM14 },
	{ "parm15",					0,										F_PARM15 },
	{ "parm16",					0,										F_PARM16 },
	{ "parm2",					0,										F_PARM2 },
	{ "parm3",					0,										F_PARM3 },
	{ "parm4",					0,										F_PARM4 },
	{ "parm5",					0,										F_PARM5 },
	{ "parm6",					0,										F_PARM6 },
	{ "parm7",					0,										F_PARM7 },
	{ "parm8",					0,										F_PARM8 },
	{ "parm9",					0,										F_PARM9 },
	{ "radius",					FOFS( radius ),							F_FLOAT },
	{ "random",					FOFS( random ),							F_FLOAT },
	{ "roffname",				FOFS( roffname ),						F_STRING },
	{ "rofftarget",				FOFS( rofftarget ),						F_STRING },
	{ "script_targetname",		FOFS( script_targetname ),				F_STRING },//scripts look for this when "affecting"
	{ "soundset",				FOFS( soundSet ),						F_STRING },
	{ "spawnflags",				FOFS( spawnflags ),						F_INT },
	{ "spawnscript",			FOFS( behaviorSet[BSET_SPAWN] ),		F_STRING },//name of script to run
	{ "speed",					FOFS( speed ),							F_FLOAT },
	{ "splashdamage",			FOFS( splashDamage ),					F_INT }, // zyk: added this field
	{ "splashradius",			FOFS( splashRadius ),					F_INT }, // zyk: added this field
	{ "target",					FOFS( target ),							F_STRING },
	{ "target2",				FOFS( target2 ),						F_STRING },
	{ "target3",				FOFS( target3 ),						F_STRING },
	{ "target4",				FOFS( target4 ),						F_STRING },
	{ "target5",				FOFS( target5 ),						F_STRING },
	{ "target6",				FOFS( target6 ),						F_STRING },
	{ "targetname",				FOFS( targetname ),						F_STRING },
	{ "targetshadername",		FOFS( targetShaderName ),				F_STRING },
	{ "targetshadernewname",	FOFS( targetShaderNewName ),			F_STRING },
	{ "team",					FOFS( team ),							F_STRING },
	{ "teamnodmg",				FOFS( teamnodmg ),						F_INT },
	{ "teamowner",				FOFS( s.teamowner ),					F_INT },
	{ "teamuser",				FOFS( alliedTeam ),						F_INT },
	{ "trdelta",				FOFS( s.apos.trDelta ),					F_VECTOR }, // zyk: added this field
	{ "usescript",				FOFS( behaviorSet[BSET_USE] ),			F_STRING },//name of script to run
	{ "victoryscript",			FOFS( behaviorSet[BSET_VICTORY] ),		F_STRING },//name of script to run
	{ "wait",					FOFS( wait ),							F_FLOAT },
	{ "zykmodelscale",			FOFS( s.iModelScale ),					F_INT }, // zyk: added this field
};

typedef struct spawn_s {
	const char	*name;
	void		(*spawn)(gentity_t *ent);
	// GalaxyRP: [Logical Entities] qtrue for a class that never links, never networks, has no
	// ICARUS and is never referenced by entity number from a networked entity -- it may then live
	// in the logical region above MAX_GENTITIES (see q_shared.h). Last on purpose so a row that
	// does not mention it is networked, which is the safe default; the rows that say qtrue are
	// the ones that were checked. Whether a given entity actually goes there is
	// RP_SpawnForClassname()'s decision, which also honours the cvar, "nological 1" and
	// script_targetname.
	qboolean	logical;
} spawn_t;

void SP_info_player_start (gentity_t *ent);
void SP_info_player_duel( gentity_t *ent );
void SP_info_player_duel1( gentity_t *ent );
void SP_info_player_duel2( gentity_t *ent );
void SP_info_player_deathmatch (gentity_t *ent);
void SP_info_player_siegeteam1 (gentity_t *ent);
void SP_info_player_siegeteam2 (gentity_t *ent);
void SP_info_player_intermission (gentity_t *ent);
void SP_info_player_intermission_red (gentity_t *ent);
void SP_info_player_intermission_blue (gentity_t *ent);
void SP_info_jedimaster_start (gentity_t *ent);
void SP_info_player_start_red (gentity_t *ent);
void SP_info_player_start_blue (gentity_t *ent);
void SP_info_firstplace(gentity_t *ent);
void SP_info_secondplace(gentity_t *ent);
void SP_info_thirdplace(gentity_t *ent);
void SP_info_podium(gentity_t *ent);

void SP_info_siege_objective (gentity_t *ent);
void SP_info_siege_radaricon (gentity_t *ent);
void SP_info_siege_decomplete (gentity_t *ent);
void SP_target_siege_end (gentity_t *ent);
void SP_misc_siege_item (gentity_t *ent);

void SP_func_plat (gentity_t *ent);
void SP_func_static (gentity_t *ent);
void SP_func_rotating (gentity_t *ent);
void SP_func_bobbing (gentity_t *ent);
void SP_func_pendulum( gentity_t *ent );
void SP_func_button (gentity_t *ent);
void SP_func_door (gentity_t *ent);
void SP_func_train (gentity_t *ent);
void SP_func_timer (gentity_t *self);
void SP_func_breakable (gentity_t *ent);
void SP_func_glass (gentity_t *ent);
void SP_func_usable( gentity_t *ent);
void SP_func_wall( gentity_t *ent );

void SP_trigger_lightningstrike( gentity_t *ent );

void SP_trigger_always (gentity_t *ent);
void SP_trigger_multiple (gentity_t *ent);
void SP_trigger_once( gentity_t *ent );
void SP_trigger_push (gentity_t *ent);
void SP_trigger_teleport (gentity_t *ent);
void SP_trigger_hurt (gentity_t *ent);
void SP_trigger_space(gentity_t *self);
void SP_trigger_shipboundary(gentity_t *self);
void SP_trigger_hyperspace(gentity_t *self);
void SP_trigger_asteroid_field(gentity_t *self);

void SP_target_remove_powerups( gentity_t *ent );
void SP_target_give (gentity_t *ent);
void SP_target_delay (gentity_t *ent);
void SP_target_speaker (gentity_t *ent);
void SP_target_print (gentity_t *ent);
void SP_target_laser (gentity_t *self);
void SP_target_character (gentity_t *ent);
void SP_target_score( gentity_t *ent );
void SP_target_teleporter( gentity_t *ent );
void SP_target_relay (gentity_t *ent);
void SP_target_kill (gentity_t *ent);
void SP_target_position (gentity_t *ent);
void SP_target_location (gentity_t *ent);
void SP_target_counter (gentity_t *self);
void SP_target_random (gentity_t *self);
void SP_target_scriptrunner( gentity_t *self );
void SP_target_interest (gentity_t *self);
void SP_target_activate (gentity_t *self);
void SP_target_deactivate (gentity_t *self);
void SP_target_level_change( gentity_t *self );
void SP_target_play_music( gentity_t *self );
void SP_target_push (gentity_t *ent);

void SP_light (gentity_t *self);
void SP_func_group (gentity_t *self);	// GalaxyRP fix: [Entity Help] g_misc.c
void SP_info_null (gentity_t *self);
void SP_info_notnull (gentity_t *self);
void SP_info_camp (gentity_t *self);
void SP_path_corner (gentity_t *self);

void SP_misc_teleporter_dest (gentity_t *self);
void SP_misc_model(gentity_t *ent);
void SP_misc_model_static(gentity_t *ent);
void SP_misc_exploding_crate( gentity_t *ent ); // zyk: added this function
void SP_misc_model_breakable( gentity_t *ent ) ;
void SP_misc_model_cargo_small( gentity_t *ent );	// DAJ_RP: [SP Maps] g_misc.c
void SP_misc_model_ghoul( gentity_t *ent );			// DAJ_RP: [SP Maps] g_misc.c
void SP_rp_light( gentity_t *ent );
void SP_misc_gas_tank( gentity_t *ent ); // zyk: added this function
void SP_misc_G2model(gentity_t *ent);
void SP_misc_portal_camera(gentity_t *ent);
void SP_misc_portal_surface(gentity_t *ent);
void SP_misc_weather_zone( gentity_t *ent );

void SP_misc_bsp (gentity_t *ent);
void SP_terrain (gentity_t *ent);
void SP_misc_skyportal_orient (gentity_t *ent);
void SP_misc_skyportal (gentity_t *ent);

void SP_misc_ammo_floor_unit(gentity_t *ent);
void SP_misc_model_gun_rack( gentity_t *ent ); // zyk: added this function
void SP_misc_model_ammo_rack( gentity_t *ent ); // zyk: added this function
void SP_misc_shield_floor_unit( gentity_t *ent );
void SP_misc_model_shield_power_converter( gentity_t *ent );
void SP_misc_model_ammo_power_converter( gentity_t *ent );
void SP_misc_model_health_power_converter( gentity_t *ent );

void SP_fx_runner( gentity_t *ent );

void SP_target_screenshake(gentity_t *ent);
void SP_target_escapetrig(gentity_t *ent);

void SP_misc_maglock ( gentity_t *self );

void SP_misc_faller(gentity_t *ent);

void SP_misc_holocron(gentity_t *ent);

void SP_reference_tag ( gentity_t *ent );

void SP_misc_weapon_shooter( gentity_t *self );
// GalaxyRP: [SP Maps] single-player entities that had no spawn function in multiplayer
void SP_misc_security_panel( gentity_t *self );
void SP_item_security_key( gentity_t *self );
void SP_misc_spotlight( gentity_t *self );
void SP_misc_trip_mine( gentity_t *ent );
void SP_misc_sentry_turret( gentity_t *base );
void SP_trigger_visible( gentity_t *self );
void SP_trigger_location( gentity_t *self );
void SP_target_secret( gentity_t *self );
void SP_target_autosave( gentity_t *self );
void SP_func_security_panel( gentity_t *ent );
void SP_func_goodie_panel( gentity_t *ent );

void SP_misc_cubemap( gentity_t *ent );

void SP_NPC_spawner( gentity_t *self );

void SP_NPC_Vehicle( gentity_t *self);
void SP_NPC_Player( gentity_t *self ); // zyk: added this function
void SP_NPC_Kyle( gentity_t *self );
void SP_NPC_Lando( gentity_t *self );
void SP_NPC_Jan( gentity_t *self );
void SP_NPC_Luke( gentity_t *self );
void SP_NPC_MonMothma( gentity_t *self );
void SP_NPC_Rosh_Penin( gentity_t *self ); // zyk: added this function
void SP_NPC_Tavion( gentity_t *self );
void SP_NPC_Tavion_New( gentity_t *self );
void SP_NPC_Alora( gentity_t *self );
void SP_NPC_Reelo( gentity_t *self );
void SP_NPC_Galak( gentity_t *self );
void SP_NPC_Desann( gentity_t *self );
void SP_NPC_Rax( gentity_t *self ); // zyk: added this function
void SP_NPC_BobaFett( gentity_t *self ); // zyk: added this function
void SP_NPC_Ragnos( gentity_t *self ); // zyk: added this function
void SP_NPC_Lannik_Racto( gentity_t *self ); // zyk: added this function
void SP_NPC_Kothos( gentity_t *self ); // zyk: added this function
void SP_NPC_Chewbacca( gentity_t *self ); // zyk: added this function
void SP_NPC_Bartender( gentity_t *self );
void SP_NPC_MorganKatarn( gentity_t *self );
void SP_NPC_Jedi( gentity_t *self );
void SP_NPC_Prisoner( gentity_t *self );
void SP_NPC_Merchant( gentity_t *self ); // zyk: added this function
void SP_NPC_Rebel( gentity_t *self );
void SP_NPC_Human_Merc( gentity_t *self );
void SP_NPC_Stormtrooper( gentity_t *self );
void SP_NPC_StormtrooperOfficer( gentity_t *self );
void SP_NPC_Snowtrooper( gentity_t *self);
void SP_NPC_RocketTrooper( gentity_t *self); // zyk: added this function
void SP_NPC_HazardTrooper( gentity_t *self); // zyk: added this function
void SP_NPC_Tie_Pilot( gentity_t *self );
void SP_NPC_Ugnaught( gentity_t *self );
void SP_NPC_Jawa( gentity_t *self );
void SP_NPC_Gran( gentity_t *self );
void SP_NPC_Rodian( gentity_t *self );
void SP_NPC_Weequay( gentity_t *self );
void SP_NPC_Trandoshan( gentity_t *self );
void SP_NPC_Tusken( gentity_t *self );
void SP_NPC_Noghri( gentity_t *self );
void SP_NPC_SwampTrooper( gentity_t *self );
void SP_NPC_Imperial( gentity_t *self );
void SP_NPC_ImpWorker( gentity_t *self );
void SP_NPC_BespinCop( gentity_t *self );
void SP_NPC_Reborn( gentity_t *self );
void SP_NPC_ShadowTrooper( gentity_t *self );
void SP_NPC_Saboteur( gentity_t *self ); // zyk: added this function
void SP_NPC_Monster_Murjj( gentity_t *self );
void SP_NPC_Monster_Swamp( gentity_t *self );
void SP_NPC_Monster_Howler( gentity_t *self );
void SP_NPC_Monster_Claw( gentity_t *self );
void SP_NPC_Monster_Glider( gentity_t *self );
void SP_NPC_Monster_Flier2( gentity_t *self );
void SP_NPC_Monster_Lizard( gentity_t *self );
void SP_NPC_Monster_Fish( gentity_t *self );
void SP_NPC_Monster_Sand_Creature( gentity_t *self ); // zyk: added this function
void SP_NPC_Monster_Wampa( gentity_t *self );
void SP_NPC_Monster_Rancor( gentity_t *self );
void SP_NPC_Monster_Mutant_Rancor( gentity_t *self ); // zyk: added this function
void SP_NPC_MineMonster( gentity_t *self );
void SP_NPC_Droid_Interrogator( gentity_t *self );
void SP_NPC_Droid_Probe( gentity_t *self );
void SP_NPC_Droid_Mark1( gentity_t *self );
void SP_NPC_Droid_Mark2( gentity_t *self );
void SP_NPC_Droid_ATST( gentity_t *self );
void SP_NPC_Droid_Seeker( gentity_t *self );
void SP_NPC_Droid_Remote( gentity_t *self );
void SP_NPC_Droid_Sentry( gentity_t *self );
void SP_NPC_Droid_Gonk( gentity_t *self );
void SP_NPC_Droid_Mouse( gentity_t *self );
void SP_NPC_Droid_R2D2( gentity_t *self );
void SP_NPC_Droid_R5D2( gentity_t *self );
void SP_NPC_Droid_Protocol( gentity_t *self );
void SP_NPC_Droid_Assassin( gentity_t *self); // zyk: added this function
void SP_NPC_Droid_Saber( gentity_t *self); // zyk: added this function

void SP_NPC_Reborn_New( gentity_t *self);
void SP_NPC_Cultist( gentity_t *self );
void SP_NPC_Cultist_Saber( gentity_t *self );
void SP_NPC_Cultist_Saber_Powers( gentity_t *self );
void SP_NPC_Cultist_Destroyer( gentity_t *self );
void SP_NPC_Cultist_Commando( gentity_t *self );

void SP_waypoint (gentity_t *ent);
void SP_waypoint_small (gentity_t *ent);
void SP_waypoint_navgoal (gentity_t *ent);
void SP_waypoint_navgoal_8 (gentity_t *ent);
void SP_waypoint_navgoal_4 (gentity_t *ent);
void SP_waypoint_navgoal_2 (gentity_t *ent);
void SP_waypoint_navgoal_1 (gentity_t *ent);

void SP_CreateWind( gentity_t *ent );
void SP_CreateSpaceDust( gentity_t *ent );
void SP_CreateSnow( gentity_t *ent );
void SP_CreateRain( gentity_t *ent );
void SP_CreateWeather( gentity_t *ent ); // zyk: added this function

void SP_ZykMiniGameJoiner(gentity_t *ent); // zyk: added this function

void SP_ZykRegenUnit( gentity_t *self); // zyk: added this function
void SP_ZykTrainingPole(gentity_t *self); // zyk: added this function

void SP_point_combat( gentity_t *self );

void SP_shooter_blaster( gentity_t *ent );

void SP_team_CTF_redplayer( gentity_t *ent );
void SP_team_CTF_blueplayer( gentity_t *ent );

void SP_team_CTF_redspawn( gentity_t *ent );
void SP_team_CTF_bluespawn( gentity_t *ent );

void SP_misc_turret( gentity_t *ent );
void SP_misc_turretG2( gentity_t *base );

void SP_item_botroam( gentity_t *ent ) { }

void SP_gametype_item ( gentity_t* ent )
{
	gitem_t *item = NULL;
	char *value;
	int team = -1;

	G_SpawnString("teamfilter", "", &value);

	G_SetOrigin( ent, ent->s.origin );

	// If a team filter is set then override any team settings for the spawns
	if ( level.mTeamFilter[0] )
	{
		if ( Q_stricmp ( level.mTeamFilter, "red") == 0 )
		{
			team = TEAM_RED;
		}
		else if ( Q_stricmp ( level.mTeamFilter, "blue") == 0 )
		{
			team = TEAM_BLUE;
		}
	}

	if (ent->targetname && ent->targetname[0])
	{
		if (team != -1)
		{
			if (strstr(ent->targetname, "flag"))
			{
				if (team == TEAM_RED)
				{
					item = BG_FindItem("team_CTF_redflag");
				}
				else
				{ //blue
					item = BG_FindItem("team_CTF_blueflag");
				}
			}
		}
		else if (strstr(ent->targetname, "red_flag"))
		{
			item = BG_FindItem("team_CTF_redflag");
		}
		else if (strstr(ent->targetname, "blue_flag"))
		{
			item = BG_FindItem("team_CTF_blueflag");
		}
		else
		{
			item = NULL;
		}

		if (item)
		{
			ent->targetname = NULL;
			ent->classname = item->classname;
			G_SpawnItem( ent, item );
		}
	}
}

void SP_emplaced_gun( gentity_t *ent );

spawn_t	spawns[] = {
	{ "emplaced_eweb",						SP_emplaced_gun }, // GalaxyRP: [SP Maps] single player's E-Web, served by the emplaced gun
	{ "emplaced_gun",						SP_emplaced_gun },
	{ "func_bobbing",						SP_func_bobbing },
	{ "func_breakable",						SP_func_breakable },
	{ "func_button",						SP_func_button },
	{ "func_door",							SP_func_door },
	{ "func_glass",							SP_func_glass },
	{ "func_goodie_panel",					SP_func_goodie_panel },
	{ "func_group",							SP_func_group },	// GalaxyRP fix: [Entity Help] its own -- see g_misc.c
	{ "func_pendulum",						SP_func_pendulum },
	{ "func_plat",							SP_func_plat },
	{ "func_rotating",						SP_func_rotating },
	{ "func_security_panel",				SP_func_security_panel },
	{ "func_static",						SP_func_static },
	{ "func_timer",							SP_func_timer }, // rename trigger_timer?
	{ "func_train",							SP_func_train },
	{ "func_usable",						SP_func_usable },
	{ "func_wall",							SP_func_wall },
	{ "fx_rain",							SP_CreateRain, qtrue },
	{ "fx_runner",							SP_fx_runner },
	{ "fx_snow",							SP_CreateSnow, qtrue },
	{ "fx_spacedust",						SP_CreateSpaceDust, qtrue },
	// GalaxyRP fix: [Logical Entities] TaystJK marks this one networked while the three fx_*
	// classes above it are logical, and nothing in the code supports the split: its SP_CreateWind
	// is byte-for-byte theirs, and like SP_CreateRain/Snow/SpaceDust it only calls G_EffectIndex
	// and reads its own spawn keys -- it never links, never sets a model or a think/use, and never
	// hands its entity number to the engine. "fx_wind" appears nowhere else in this tree, so
	// nothing looks one up. Their list is curated by class name rather than derived from the spawn
	// function (see misc_cubemap below for the proof), and this is one of its misses. Do not let a
	// re-sync of this table from upstream quietly put it back: a map with a script_targetname on an
	// fx_wind is networked by the router anyway, and "nological 1" is the per-entity override.
	{ "fx_wind",							SP_CreateWind, qtrue },
	{ "gametype_item",						SP_gametype_item },
	{ "info_camp",							SP_info_camp, qtrue },
	{ "info_jedimaster_start",				SP_info_jedimaster_start },
	{ "info_notnull",						SP_info_notnull, qtrue }, // use target_position instead
	{ "info_null",							SP_info_null, qtrue },
	{ "info_player_deathmatch",				SP_info_player_deathmatch, qtrue },
	{ "info_player_duel",					SP_info_player_duel, qtrue },
	{ "info_player_duel1",					SP_info_player_duel1, qtrue },
	{ "info_player_duel2",					SP_info_player_duel2, qtrue },
	{ "info_player_intermission",			SP_info_player_intermission, qtrue },
	{ "info_player_intermission_blue",		SP_info_player_intermission_blue, qtrue },
	{ "info_player_intermission_red",		SP_info_player_intermission_red, qtrue },
	{ "info_player_siegeteam1",				SP_info_player_siegeteam1, qtrue },
	{ "info_player_siegeteam2",				SP_info_player_siegeteam2, qtrue },
	{ "info_player_start",					SP_info_player_start, qtrue },
	{ "info_player_start_blue",				SP_info_player_start_blue, qtrue },
	{ "info_player_start_red",				SP_info_player_start_red, qtrue },
	{ "info_siege_decomplete",				SP_info_siege_decomplete, qtrue },
	{ "info_siege_objective",				SP_info_siege_objective },
	{ "info_siege_radaricon",				SP_info_siege_radaricon },
	{ "item_botroam",						SP_item_botroam, qtrue },
	{ "item_security_key",					SP_item_security_key },
	{ "light",								SP_light },
	{ "misc_ammo_floor_unit",				SP_misc_ammo_floor_unit },
	{ "misc_bsp",							SP_misc_bsp },
	// GalaxyRP: [Logical Entities] left networked deliberately. SP_misc_cubemap is the identical
	// one-liner G_FreeEntity(ent) that SP_info_null is, and info_null IS marked logical -- which is
	// what shows TaystJK's list to be hand-curated rather than derived. Unlike fx_wind above, the
	// marking costs nothing here: the entity is gone on the frame it spawned, so which region it
	// was allocated from is unobservable. Not worth the churn of changing.
	{ "misc_cubemap",						SP_misc_cubemap },
	{ "misc_exploding_crate",				SP_misc_exploding_crate}, // zyk: added this code
	{ "misc_faller",						SP_misc_faller },
	{ "misc_G2model",						SP_misc_G2model },
	{ "misc_gas_tank",						SP_misc_gas_tank}, // zyk: added this code
	{ "misc_holocron",						SP_misc_holocron },
	{ "misc_maglock",						SP_misc_maglock },
	{ "misc_model",							SP_misc_model },
	{ "misc_model_ammo_power_converter",	SP_misc_model_ammo_power_converter },
	{ "misc_model_ammo_rack",				SP_misc_model_ammo_rack}, // zyk: added this code
	{ "misc_model_breakable",				SP_misc_model_breakable },
	{ "misc_model_cargo_small",				SP_misc_model_cargo_small },	// DAJ_RP: [SP Maps]
	{ "misc_model_ghoul",					SP_misc_model_ghoul },			// DAJ_RP: [SP Maps]
	{ "misc_model_gun_rack",				SP_misc_model_gun_rack}, // zyk: added this code
	{ "misc_model_health_power_converter",	SP_misc_model_health_power_converter },
	{ "misc_model_shield_power_converter",	SP_misc_model_shield_power_converter },
	{ "misc_model_static",					SP_misc_model_static, qtrue },
	{ "misc_portal_camera",					SP_misc_portal_camera },
	{ "misc_security_panel",				SP_misc_security_panel },
	{ "misc_sentry_turret",					SP_misc_sentry_turret },
	{ "misc_portal_surface",				SP_misc_portal_surface },
	{ "misc_shield_floor_unit",				SP_misc_shield_floor_unit },
	{ "misc_siege_item",					SP_misc_siege_item },
	{ "misc_skyportal",						SP_misc_skyportal },
	{ "misc_skyportal_orient",				SP_misc_skyportal_orient },
	{ "misc_spotlight",						SP_misc_spotlight },
	{ "misc_teleporter_dest",				SP_misc_teleporter_dest, qtrue },
	{ "misc_trip_mine",						SP_misc_trip_mine, qtrue }, // GalaxyRP: [SP Maps] the placeholder; the mine it spawns is networked
	{ "misc_turret",						SP_misc_turret },
	{ "misc_turretG2",						SP_misc_turretG2 },
	{ "misc_weapon_shooter",				SP_misc_weapon_shooter },
	// GalaxyRP: [Logical Entities] left networked for the same reason as misc_cubemap above --
	// SP_misc_weather_zone frees the entity immediately, so the region never matters. The zone the
	// client actually uses is read from the BSP's own entity lump by cgame, never from this entity
	// (see the "die" note in the /admweather effect table, g_cmds.c).
	{ "misc_weather_zone",					SP_misc_weather_zone },
	{ "npc_alora",							SP_NPC_Alora, qtrue },
	{ "npc_bartender",						SP_NPC_Bartender, qtrue },
	{ "npc_bespincop",						SP_NPC_BespinCop, qtrue },
	{ "npc_bobafett",						SP_NPC_BobaFett, qtrue }, // zyk: added this function
	{ "npc_chewbacca",						SP_NPC_Chewbacca, qtrue }, // zyk: added this function
	{ "npc_colombian_emplacedgunner",		SP_NPC_ShadowTrooper, qtrue },
	{ "npc_colombian_rebel",				SP_NPC_Reborn, qtrue },
	{ "npc_colombian_soldier",				SP_NPC_Reborn, qtrue },
	{ "npc_cultist",						SP_NPC_Cultist, qtrue },
	{ "npc_cultist_commando",				SP_NPC_Cultist_Commando, qtrue },
	{ "npc_cultist_destroyer",				SP_NPC_Cultist_Destroyer, qtrue },
	{ "npc_cultist_saber",					SP_NPC_Cultist_Saber, qtrue },
	{ "npc_cultist_saber_powers",			SP_NPC_Cultist_Saber_Powers, qtrue },
	{ "npc_desann",							SP_NPC_Desann, qtrue },
	{ "npc_droid_assassin",					SP_NPC_Droid_Assassin, qtrue }, // zyk: added this function
	{ "npc_droid_atst",						SP_NPC_Droid_ATST, qtrue },
	{ "npc_droid_gonk",						SP_NPC_Droid_Gonk, qtrue },
	{ "npc_droid_interrogator",				SP_NPC_Droid_Interrogator, qtrue },
	{ "npc_droid_mark1",					SP_NPC_Droid_Mark1, qtrue },
	{ "npc_droid_mark2",					SP_NPC_Droid_Mark2, qtrue },
	{ "npc_droid_mouse",					SP_NPC_Droid_Mouse, qtrue },
	{ "npc_droid_probe",					SP_NPC_Droid_Probe, qtrue },
	{ "npc_droid_protocol",					SP_NPC_Droid_Protocol, qtrue },
	{ "npc_droid_r2d2",						SP_NPC_Droid_R2D2, qtrue },
	{ "npc_droid_r5d2",						SP_NPC_Droid_R5D2, qtrue },
	{ "npc_droid_remote",					SP_NPC_Droid_Remote, qtrue },
	{ "npc_droid_saber",					SP_NPC_Droid_Saber, qtrue }, // zyk: added this function
	{ "npc_droid_seeker",					SP_NPC_Droid_Seeker, qtrue },
	{ "npc_droid_sentry",					SP_NPC_Droid_Sentry, qtrue },
	{ "npc_galak",							SP_NPC_Galak, qtrue },
	{ "npc_gran",							SP_NPC_Gran, qtrue },
	{ "npc_hazardtrooper",					SP_NPC_HazardTrooper, qtrue }, // zyk: added this function
	{ "npc_human_merc",						SP_NPC_Human_Merc, qtrue },
	{ "npc_imperial",						SP_NPC_Imperial, qtrue },
	{ "npc_impworker",						SP_NPC_ImpWorker, qtrue },
	{ "npc_jan",							SP_NPC_Jan, qtrue },
	{ "npc_jawa",							SP_NPC_Jawa, qtrue },
	{ "npc_jedi",							SP_NPC_Jedi, qtrue },
	{ "npc_kothos",							SP_NPC_Kothos, qtrue }, // zyk: added this function
	{ "npc_kyle",							SP_NPC_Kyle, qtrue },
	{ "npc_lando",							SP_NPC_Lando, qtrue },
	{ "npc_lannik_racto",					SP_NPC_Lannik_Racto, qtrue }, // zyk: added this function
	{ "npc_luke",							SP_NPC_Luke, qtrue },
	{ "npc_manuel_vergara_rmg",				SP_NPC_Desann, qtrue },
	{ "npc_merchant",						SP_NPC_Merchant, qtrue }, // zyk: added this function
	{ "npc_minemonster",					SP_NPC_MineMonster, qtrue },
	{ "npc_monmothma",						SP_NPC_MonMothma, qtrue },
	{ "npc_monster_claw",					SP_NPC_Monster_Claw, qtrue },
	{ "npc_monster_fish",					SP_NPC_Monster_Fish, qtrue },
	{ "npc_monster_flier2",					SP_NPC_Monster_Flier2, qtrue },
	{ "npc_monster_glider",					SP_NPC_Monster_Glider, qtrue },
	{ "npc_monster_howler",					SP_NPC_Monster_Howler, qtrue },
	{ "npc_monster_lizard",					SP_NPC_Monster_Lizard, qtrue },
	{ "npc_monster_murjj",					SP_NPC_Monster_Murjj, qtrue },
	{ "npc_monster_mutant_rancor",			SP_NPC_Monster_Mutant_Rancor, qtrue }, // zyk: added this function
	{ "npc_monster_rancor",					SP_NPC_Monster_Rancor, qtrue },
	{ "npc_monster_sand_creature",			SP_NPC_Monster_Sand_Creature, qtrue }, // zyk: added this function
	{ "npc_monster_swamp",					SP_NPC_Monster_Swamp, qtrue },
	{ "npc_monster_wampa",					SP_NPC_Monster_Wampa, qtrue },
	{ "npc_morgankatarn",					SP_NPC_MorganKatarn, qtrue },
	{ "npc_noghri",							SP_NPC_Noghri, qtrue },
	{ "npc_player",							SP_NPC_Player, qtrue }, // zyk: added this function
	{ "npc_prisoner",						SP_NPC_Prisoner, qtrue },
	{ "npc_ragnos",							SP_NPC_Ragnos, qtrue }, // zyk: added this function
	{ "npc_rax",							SP_NPC_Rax, qtrue }, // zyk: added this function
	{ "npc_rebel",							SP_NPC_Rebel, qtrue },
	{ "npc_reborn",							SP_NPC_Reborn, qtrue },
	{ "npc_reborn_new",						SP_NPC_Reborn_New, qtrue },
	{ "npc_reelo",							SP_NPC_Reelo, qtrue },
	{ "npc_rockettrooper",					SP_NPC_RocketTrooper, qtrue }, // zyk: added this function
	{ "npc_rodian",							SP_NPC_Rodian, qtrue },
	{ "npc_rosh_penin",						SP_NPC_Rosh_Penin, qtrue }, // zyk: added this function
	{ "npc_saboteur",						SP_NPC_Saboteur, qtrue }, // zyk: added this function
	{ "npc_shadowtrooper",					SP_NPC_ShadowTrooper, qtrue },
	{ "npc_snowtrooper",					SP_NPC_Snowtrooper, qtrue },
	{ "npc_spawner",						SP_NPC_spawner, qtrue },
	{ "npc_stormtrooper",					SP_NPC_Stormtrooper, qtrue },
	{ "npc_stormtrooperofficer",			SP_NPC_StormtrooperOfficer, qtrue },
	{ "npc_swamptrooper",					SP_NPC_SwampTrooper, qtrue },
	{ "npc_tavion",							SP_NPC_Tavion, qtrue },
	{ "npc_tavion_new",						SP_NPC_Tavion_New, qtrue },
	{ "npc_tie_pilot",						SP_NPC_Tie_Pilot, qtrue },
	{ "npc_trandoshan",						SP_NPC_Trandoshan, qtrue },
	{ "npc_tusken",							SP_NPC_Tusken, qtrue },
	{ "npc_ugnaught",						SP_NPC_Ugnaught, qtrue },
	{ "npc_vehicle",						SP_NPC_Vehicle, qtrue },
	{ "npc_weequay",						SP_NPC_Weequay, qtrue },
	{ "path_corner",						SP_path_corner, qtrue },
	{ "point_combat",						SP_point_combat, qtrue },
	{ "ref_tag",							SP_reference_tag, qtrue },
	{ "ref_tag_huge",						SP_reference_tag, qtrue },
	{ "rp_light",							SP_rp_light },	// GalaxyRP: [Entity System] a light and nothing else, g_misc.c
	{ "shooter_blaster",					SP_shooter_blaster },
	{ "target_autosave",					SP_target_autosave, qtrue }, // GalaxyRP: [SP Maps] no-op
	{ "target_activate",					SP_target_activate, qtrue },
	{ "target_counter",						SP_target_counter, qtrue },
	{ "target_deactivate",					SP_target_deactivate, qtrue },
	{ "target_delay",						SP_target_delay, qtrue },
	{ "target_escapetrig",					SP_target_escapetrig, qtrue },
	{ "target_give",						SP_target_give, qtrue },
	{ "target_interest",					SP_target_interest, qtrue },
	{ "target_kill",						SP_target_kill, qtrue },
	{ "target_laser",						SP_target_laser },
	{ "target_level_change",				SP_target_level_change, qtrue },
	{ "target_location",					SP_target_location, qtrue },
	{ "target_play_music",					SP_target_play_music, qtrue },
	{ "target_position",					SP_target_position, qtrue },
	{ "target_print",						SP_target_print, qtrue },
	{ "target_push",						SP_target_push, qtrue },
	{ "target_random",						SP_target_random, qtrue },
	{ "target_relay",						SP_target_relay, qtrue },
	{ "target_secret",						SP_target_secret, qtrue },
	{ "target_remove_powerups",				SP_target_remove_powerups, qtrue },
	{ "target_score",						SP_target_score, qtrue },
	{ "target_screenshake",					SP_target_screenshake, qtrue },
	{ "target_scriptrunner",				SP_target_scriptrunner },
	{ "target_siege_end",					SP_target_siege_end },
	{ "target_speaker",						SP_target_speaker },
	{ "target_teleporter",					SP_target_teleporter, qtrue },
	{ "team_CTF_blueplayer",				SP_team_CTF_blueplayer, qtrue },
	{ "team_CTF_bluespawn",					SP_team_CTF_bluespawn, qtrue },
	{ "team_CTF_redplayer",					SP_team_CTF_redplayer, qtrue },
	{ "team_CTF_redspawn",					SP_team_CTF_redspawn, qtrue },
	{ "terrain",							SP_terrain },
	{ "trigger_always",						SP_trigger_always },
	{ "trigger_asteroid_field",				SP_trigger_asteroid_field },
	{ "trigger_hurt",						SP_trigger_hurt },
	{ "trigger_hyperspace",					SP_trigger_hyperspace },
	{ "trigger_lightningstrike",			SP_trigger_lightningstrike },
	{ "trigger_location",					SP_trigger_location },
	{ "trigger_multiple",					SP_trigger_multiple },
	{ "trigger_once",						SP_trigger_once },
	{ "trigger_push",						SP_trigger_push },
	{ "trigger_shipboundary",				SP_trigger_shipboundary },
	{ "trigger_space",						SP_trigger_space },
	{ "trigger_teleport",					SP_trigger_teleport },
	{ "trigger_visible",					SP_trigger_visible },
	{ "waypoint",							SP_waypoint, qtrue },
	{ "waypoint_navgoal",					SP_waypoint_navgoal, qtrue },
	{ "waypoint_navgoal_1",					SP_waypoint_navgoal_1, qtrue },
	{ "waypoint_navgoal_2",					SP_waypoint_navgoal_2, qtrue },
	{ "waypoint_navgoal_4",					SP_waypoint_navgoal_4, qtrue },
	{ "waypoint_navgoal_8",					SP_waypoint_navgoal_8, qtrue },
	{ "waypoint_small",						SP_waypoint_small, qtrue },
	{ "zyk_mini_game_joiner",				SP_ZykMiniGameJoiner }, // zyk: added this code
	{ "zyk_regen_unit",						SP_ZykRegenUnit }, // zyk: added this code
	{ "zyk_training_pole",					SP_ZykTrainingPole }, // zyk: added this code
	{ "zyk_weather",						SP_CreateWeather, qtrue }, // zyk: added this code
};

/*
===============
G_CallSpawn

Finds the spawn function for the entity and calls it,
returning qfalse if not found
===============
*/
static int spawncmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((spawn_t*)b)->name );
}

// DAJ_RP: [Entity Help] the table, for /enthelp (g_enthelp.c): how many classes, and the i-th one's name
// and whether it is a logical class
int RP_SpawnClassCount( void ) {
	return (int)ARRAY_LEN( spawns );
}

const char *RP_SpawnClassName( int i, qboolean *logical ) {
	if ( i < 0 || i >= (int)ARRAY_LEN( spawns ) ) {
		if ( logical ) {
			*logical = qfalse;
		}
		return NULL;
	}

	if ( logical ) {
		*logical = spawns[i].logical;
	}
	return spawns[i].name;
}

// GalaxyRP: [Logical Entities] the table's answer alone. Item classes (bg_itemlist) are not in the
// table and so are never logical: they are pickups, linked and networked.
qboolean G_IsLogicalEntity( const char *classname ) {
	spawn_t	*s;

	if ( !classname || !classname[0] ) {
		return qfalse;
	}

	s = (spawn_t *)Q_LinearSearch( classname, spawns, ARRAY_LEN( spawns ), sizeof( spawn_t ), spawncmp );
	if ( s && s->logical ) {
		return qtrue;
	}

	return qfalse;
}

// GalaxyRP: [Logical Entities] see g_local.h. The four spawn paths that build an entity from
// key/value pairs all allocate through here, so the region a classname lands in is decided once:
//
//   - rp_logical_entities was 0 at map start   -> networked (the feature is off)
//   - the class is not marked logical          -> networked
//   - the entity carries "nological 1"         -> networked (a mapper's or admin's escape hatch,
//                                                 TaystJK's key, kept for preset compatibility)
//   - the entity has a script_targetname       -> networked (ICARUS keeps its state in the engine,
//                                                 indexed by entity number, so it needs a real one)
//   - otherwise                                -> logical
//
// Returns exactly what G_Spawn() / G_SpawnLogical() return; the caller sets the classname on it
// afterwards as it always did.
qboolean RP_ClassnameWantsLogical( const char *classname, qboolean nological, qboolean hasScriptTargetname ) {
	if ( level.logical_entities_enabled && !nological && !hasScriptTargetname && G_IsLogicalEntity( classname ) ) {
		return qtrue;
	}

	return qfalse;
}

gentity_t *RP_SpawnForClassname( const char *classname, qboolean nological, qboolean hasScriptTargetname ) {
	if ( RP_ClassnameWantsLogical( classname, nological, hasScriptTargetname ) ) {
		return G_SpawnLogical();
	}

	return G_Spawn();
}

// GalaxyRP: [Logical Entities] see g_local.h.
void RP_SpawnRouteInit( rpSpawnRoute_t *route ) {
	memset( route, 0, sizeof( *route ) );
}

void RP_SpawnRouteNoteKey( rpSpawnRoute_t *route, const char *key, const char *value ) {
	if ( !route || !key || !value ) {
		return;
	}

	if ( !Q_stricmp( key, "classname" ) ) {
		Q_strncpyz( route->classname, value, sizeof( route->classname ) );
	} else if ( !Q_stricmp( key, "nological" ) ) {
		route->nological = atoi( value ) ? qtrue : qfalse;
	} else if ( !Q_stricmp( key, "script_targetname" ) ) {
		if ( value[0] ) {
			route->hasScriptTargetname = qtrue;
		}
	} else {
		// any behaviour-set key -- spawnscript, usescript, deathscript, ... -- ends in "script".
		// An entity that carries one will be handed to ICARUS_RunScript by G_ActivateBehavior,
		// which the engine indexes by entity number, so it needs a networked slot just as a
		// script_targetname does. (TaystJK only tests script_targetname; a logical target_relay
		// with a usescript would reach the engine's per-entity ICARUS tables with a number they
		// were never sized for.)
		size_t klen = strlen( key );

		if ( klen > 6 && !Q_stricmp( key + klen - 6, "script" ) && value[0] ) {
			route->hasScriptTargetname = qtrue;
		}
	}
}

gentity_t *RP_SpawnForRoute( const rpSpawnRoute_t *route ) {
	return RP_SpawnForClassname( route->classname, route->nological, route->hasScriptTargetname );
}

qboolean RP_SpawnRouteIsLogical( const rpSpawnRoute_t *route ) {
	return RP_ClassnameWantsLogical( route->classname, route->nological, route->hasScriptTargetname );
}

qboolean G_CallSpawn( gentity_t *ent ) {
	spawn_t	*s;
	gitem_t	*item;

	if ( !ent->classname ) {
		trap->Print( "G_CallSpawn: NULL classname\n" );
		return qfalse;
	}

	// check item spawn functions
	//TODO: cant reorder items because compat so....?
	for ( item=bg_itemlist+1 ; item->classname ; item++ ) {
		if ( !strcmp(item->classname, ent->classname) ) {
			G_SpawnItem( ent, item );
			return qtrue;
		}
	}

	// check normal spawn functions
	s = (spawn_t *)Q_LinearSearch( ent->classname, spawns, ARRAY_LEN( spawns ), sizeof( spawn_t ), spawncmp );
	if ( s )
	{// found it
		if ( VALIDSTRING( ent->healingsound ) )
			G_SoundIndex( ent->healingsound );

		s->spawn( ent );
		return qtrue;
	}

	trap->Print( "%s doesn't have a spawn function\n", ent->classname );
	return qfalse;
}

/*
=============
G_NewString

Builds a copy of the string, translating \n to real linefeeds
so message texts can be multi-line
=============
*/
char *G_NewString( const char *string )
{
	char *newb=NULL, *new_p=NULL;
	int i=0, len=0;

	len = strlen( string )+1;
	new_p = newb = (char *)G_Alloc( len );

	for ( i=0; i<len; i++ )
	{// turn \n into a real linefeed
		if ( string[i] == '\\' && i < len-1 )
		{
			if ( string[i+1] == 'n' )
			{
				*new_p++ = '\n';
				i++;
			}
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return newb;
}

// GalaxyRP fix: [Entity System] G_NewString translates a backslash-n in its input into a real
// linefeed. That is right for text a human typed, and wrong for a token the entity-file parser has
// already decoded: a value legitimately containing a backslash would be re-read as an escape and
// corrupted on every load. This is the same allocating copy with no translation.
char *G_NewStringRaw( const char *string )
{
	char *newb = NULL;
	int len = 0;

	if (!string)
		string = "";

	len = strlen( string ) + 1;
	newb = (char *)G_Alloc( len );

	memcpy( newb, string, len );

	return newb;
}

// GalaxyRP fix: [Entity System] /entsave wrote each pair as "key;value;" with no quoting and no
// escaping, and the loader split on ';' and on the newline. So a value containing either character
// did not survive the round trip: a ';' silently re-split one value into further KEYS (a message of
// "a;origin;0 0 9999" came back as a message of "a" plus an origin key that was never set), an odd
// token count threw the whole entity away, and a real linefeed -- which G_NewString produces from a
// backslash-n typed into /entadd -- broke the record into two malformed halves and lost the entity.
// Both characters are now escaped on the way out and decoded on the way back in, so a value means
// the same thing before and after a save. Values already in existing files are unaffected: they
// hold no "\;" or "\\", and a "\n" there already meant a linefeed, which is what it still decodes to.
// GalaxyRP fix: [Entity System] this returned void, so running out of room was invisible to the
// caller. It cannot overrun -- it stops while two characters still fit -- but stopping is not a
// harmless outcome: the result is a cleanly terminated token that is SHORTER than the one asked
// for, and /entsave would write it as if nothing had happened. A truncated value is a changed
// value, and a truncated KEY is a different key, so the record would load back as an entity that
// is not the one that was saved. Report it instead, and let the caller refuse.
//
// qtrue means the whole of "in" is in "out". qfalse means it did not fit, and whatever is in "out"
// must not be written anywhere.
qboolean zyk_entity_file_encode( const char *in, char *out, int out_size )
{
	int i = 0;
	int l = 0;

	if (!out || out_size < 1)
		return qfalse;

	if (!in)
	{
		out[0] = '\0';
		return qtrue;		// zyk: nothing to encode is not a failure to encode it
	}

	// zyk: every escape costs two characters, so stop while two still fit
	for (i = 0; in[i] != '\0' && l < (out_size - 2); i++)
	{
		if (in[i] == '\\')
		{
			out[l++] = '\\';
			out[l++] = '\\';
		}
		else if (in[i] == ';')
		{
			out[l++] = '\\';
			out[l++] = ';';
		}
		else if (in[i] == '\n')
		{
			out[l++] = '\\';
			out[l++] = 'n';
		}
		else if (in[i] == '\r')
		{
			out[l++] = '\\';
			out[l++] = 'r';
		}
		else
		{
			out[l++] = in[i];
		}
	}

	out[l] = '\0';

	// zyk: the loop above ends either at the end of the input or because there was no longer room
	// for another escape. Only the first of those is success
	return (in[i] == '\0') ? qtrue : qfalse;
}

// zyk: decodes one token of an entity-file line into out, starting at k, and returns the index of
// the character that ended it. The caller checks that character is a ';' -- a token that ran to the
// end of the line, or that filled out, ends on something else and the caller treats the line as
// malformed, exactly as it did before escaping existed.
int zyk_entity_file_decode( const char *content, int content_len, int k, char *out, int out_size )
{
	int l = 0;

	if (!content || !out || out_size < 1)
		return k;

	while (k < content_len && l < (out_size - 1))
	{
		if (content[k] == '\\' && (k + 1) < content_len)
		{ // zyk: an escape sequence written by zyk_entity_file_encode
			k++;

			if (content[k] == 'n')
				out[l++] = '\n';
			else if (content[k] == 'r')
				out[l++] = '\r';
			else if (content[k] == ';')
				out[l++] = ';';
			else if (content[k] == '\\')
				out[l++] = '\\';
			else
			{ // zyk: not a sequence we write. Keep the backslash and read this character normally,
			  // which is what a hand-written file has always got from G_NewString
				out[l++] = '\\';
				continue;
			}

			k++;
			continue;
		}

		if (content[k] == ';')
			break;

		out[l++] = content[k];
		k++;
	}

	out[l] = '\0';

	return k;
}

char *G_NewString_Safe( const char *string )
{
	char *newb=NULL, *new_p=NULL;
	int i=0, len=0;

	len = strlen( string )+1;
	new_p = newb = (char *)malloc( len );

	if ( !new_p )
		return NULL;

	for ( i=0; i<len; i++ )
	{// turn \n into a real linefeed
		if ( string[i] == '\\' && i < len-1 )
		{
			if ( string[i+1] == 'n' )
			{
				*new_p++ = '\n';
				i++;
			}
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return newb;
}

/*
===============
RP_EntResetForRespawn

GalaxyRP fix: [Entity System] an entity spawned again in its own slot -- /entedit, /entrotate, a dropped
/entcut, /entcancel of one -- keeps its struct, and every spawn function is written for a fresh one. So
whatever the last spawn (or the game since) left in it stayed, and the spawn function went on from there:
  - a value whose key the record does not have was never read again, and the conversions a spawn
    function does on it ran once more. A func_door with no "wait" took its default 2 s and turned it into
    2000 ms; spawned again, 2000 became 2,000,000 -- 33 minutes -- and the door, opened once, stayed open.
    The same for a func_button's wait (16 minutes pressed) and a trigger_multiple's speed (the delay
    before target2 fires) when it has a target2;
  - a key taken out of the record (zykremovekey) kept its value until the map changed: a door whose
    targetname was removed still answered the old name, and grew no walk-up trigger of its own;
  - what a spawnflag switches on was never switched off again: a door or trigger edited out of INACTIVE
    stayed inactive, a func_usable or func_wall out of START_OFF came back solid but invisible, a
    misc_model_breakable out of it stayed hidden.
Called by RP_EntRespawnPrepare() (g_entgrab.c), the one step every spawn in place goes through, after
the entity is out of a team it followed and its children are gone: everything a key can set (the fields
table -- not the classname, which the record always has, nor the ICARUS parms, which are not kept here),
and the state a newly made entity starts without (flags, what it draws and sends, what it blocks, whether
it takes damage, its callbacks and next think, its looping sound), go back to nothing. The spawn function
then sets up exactly what the record describes, as on a fresh map -- runtime state included: a trigger a
target_deactivate switched off is active again, a breakable is at full health.

One thing stays: an NPC or vehicle spawner counting down to its next spawn keeps the countdown -- the
respawn of an NPC that died, or one waiting its turn, is still to come after an edit, as it always was
(RP_SpawnerKeys() in NPC_spawn.c). The name a team's leader answers for its followers comes back after
the spawn function, as at map load -- RP_TeamLeaderTakeName() below.
===============
*/
void RP_EntResetForRespawn( gentity_t *e )
{
	byte *b = (byte *)e;
	int i;

	if ( !e || !e->inuse )
		return;

	for ( i = 0; i < (int)ARRAY_LEN( fields ); i++ )
	{
		const field_t *f = &fields[i];

		if ( f->type >= F_PARM1 || !Q_stricmp( f->name, "classname" ) )
			continue;

		switch ( f->type )
		{
		case F_INT:
			*(int *)( b + f->ofs ) = 0;
			break;
		case F_FLOAT:
			*(float *)( b + f->ofs ) = 0.0f;
			break;
		case F_STRING:
			*(char **)( b + f->ofs ) = NULL;
			break;
		case F_VECTOR:
		case F_ANGLEHACK:
			VectorClear( (float *)( b + f->ofs ) );
			break;
		default:
			break;
		}
	}

	e->flags &= FL_TEAMSLAVE;	// a follower is out of its team by now; kept as it is, not set here
	e->s.eFlags = 0;
	e->r.svFlags = 0;
	e->r.contents = 0;
	e->s.solid = 0;
	e->clipmask = 0;
	e->takedamage = qfalse;

	// an NPC or vehicle spawner counting down to a spawn keeps it: a respawn of an NPC that died, or one
	// waiting its turn, is still to come after an edit (RP_SpawnerKeys() in NPC_spawn.c)
	if ( !RP_SpawnerCountingDown( e ) )
	{
		e->think = NULL;
		e->nextthink = 0;
	}
	e->reached = NULL;
	e->blocked = NULL;
	e->touch = NULL;
	e->use = NULL;
	e->pain = NULL;
	e->die = NULL;

	e->s.loopSound = 0;
	e->s.loopIsSoundset = qfalse;

	// GalaxyRP: [Entity System] the health bar a client draws under the crosshair (s.maxhealth not 0), what
	// it shows (s.health), the health it is scaled from (maxHealth: G_ScaleNetHealth()), and whether the
	// crosshair marks the entity -- and, on an NPC spawner, gives the NPCs it makes a bar (s.shouldtarget,
	// NPC_Begin()) -- are not keys, so nothing cleared them: a dispenser edited to nodrain 1, a func_breakable
	// or an NPC spawner edited out of showhealth, kept the bar. Every spawn function that wants them sets
	// them again.
	e->maxHealth = 0;
	e->s.maxhealth = 0;
	e->s.health = 0;
	e->s.shouldtarget = qfalse;

	// GalaxyRP: [Entity System] a speaker's hearing range (RP_SpeakerAudienceFrame()), set again by
	// SP_target_speaker() when it still has a sound set
	e->rpAudibleRange = 0.0f;
}

/*
===============
RP_TeamLeaderTakeName

GalaxyRP fix: [Entity System] at map load G_FindTeams() moves a follower's targetname onto its team's
leader -- a trigger targets the team through its leader -- after every spawn function has run, so a door
leading a team named only on a follower builds its walk-up trigger as one without a name does, and answers
the name from then on. A leader spawned again in its slot (RP_EntResetForRespawn() cleared its name, its
spawn function ran with only its own) takes it back the same way, from zyk_main_spawn_entity(), once its
spawn is done: the first follower in its chain whose record names one -- the one G_FindTeams(), and
RP_TeamLinkEntity() since, linked last -- as G_FindTeams() leaves it. Nothing for any other entity: one
that is not leading followers, or whose followers have no name.
===============
*/
void RP_TeamLeaderTakeName( gentity_t *e )
{
	gentity_t *m;
	int steps = 0;

	if ( !e || !e->inuse || e->teammaster != e || !e->teamchain )
		return;

	for ( m = e->teamchain; m && steps < MAX_GENTITIES; m = m->teamchain, steps++ )
	{
		const int num = m->s.number;
		int i;

		if ( !m->inuse || m->teammaster != e )
			continue;

		for ( i = 0; i + 1 < level.zyk_spawn_strings_values_count[num] && i + 1 < ZYK_MAX_SPAWN_STRING_SLOTS; i += 2 )
		{
			const char *key = level.zyk_spawn_strings[num][i];
			const char *value = level.zyk_spawn_strings[num][i + 1];

			if ( key && value && value[0] && !Q_stricmp( key, "targetname" ) )
			{
				e->targetname = G_NewString( value );
				return;
			}
		}
	}
}

/*
===============
G_ParseField

Takes a key/value pair and sets the binary values
in a gentity
===============
*/

static int fieldcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((field_t*)b)->name );
}

void Q3_SetParm ( int entID, int parmNum, const char *parmValue );
void G_ParseField( const char *key, const char *value, gentity_t *ent )
{
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;

	f = (field_t *)Q_LinearSearch( key, fields, ARRAY_LEN( fields ), sizeof( field_t ), fieldcmp );
	if ( f )
	{// found it
		b = (byte *)ent;

		switch( f->type ) {
		case F_STRING:
			*(char **)(b+f->ofs) = G_NewString (value);
			break;
		case F_VECTOR:
			// GalaxyRP fix: [Entity System] a component that is not a number is read as a failed parse
			if ( sscanf( value, "%f %f %f", &vec[0], &vec[1], &vec[2] ) == 3 && RP_FiniteVecKey( key, value, vec ) ) {
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
			}
			else {
				trap->Print( "G_ParseField: Failed sscanf on F_VECTOR (key/value: %s/%s)\n", key, value );
				((float *)(b+f->ofs))[0] = ((float *)(b+f->ofs))[1] = ((float *)(b+f->ofs))[2] = 0.0f;
			}
			break;
		case F_INT:
			*(int *)(b+f->ofs) = atoi(value);
			break;
		case F_FLOAT:
			v = atof(value);
			*(float *)(b+f->ofs) = RP_FiniteKey( key, value, v ) ? v : 0.0f;
			break;
		case F_ANGLEHACK:
			v = atof(value);
			if ( !RP_FiniteKey( key, value, v ) )
				v = 0.0f;
			((float *)(b+f->ofs))[0] = 0;
			((float *)(b+f->ofs))[1] = v;
			((float *)(b+f->ofs))[2] = 0;
			break;
		case F_PARM1:
		case F_PARM2:
		case F_PARM3:
		case F_PARM4:
		case F_PARM5:
		case F_PARM6:
		case F_PARM7:
		case F_PARM8:
		case F_PARM9:
		case F_PARM10:
		case F_PARM11:
		case F_PARM12:
		case F_PARM13:
		case F_PARM14:
		case F_PARM15:
		case F_PARM16:
			Q3_SetParm( ent->s.number, (f->type - F_PARM1), (char *) value );
			break;
		}
		return;
	}
}

// GalaxyRP: [Logical Entities] a logical entity is never a mover (every mover class is networked),
// but the macro hands the entity to the engine, so it tests the flag rather than rely on that.
#define ADJUST_AREAPORTAL() \
	if(ent->s.eType == ET_MOVER && !ent->isLogical) \
	{ \
		trap->LinkEntity((sharedEntity_t *)ent); \
		trap->AdjustAreaPortalState((sharedEntity_t *)ent, qtrue); \
	}

/*
===================
G_SpawnGEntityFromSpawnVars

Spawn an entity and fill in all of the level fields from
level.spawnVars[], then call the class specfic spawn function
===================
*/
void G_SpawnGEntityFromSpawnVars( qboolean inSubBSP ) {
	int			i;
	int j = 0; // zyk: counter to use in the zyk_spawn_strings array
	gentity_t	*ent;
	char		*s, *value, *gametypeName;
	static char *gametypeNames[GT_MAX_GAME_TYPE] = {"ffa", "holocron", "jedimaster", "duel", "powerduel", "single", "team", "siege", "ctf", "cty"};

	// get the next free entity
	// GalaxyRP: [Logical Entities] in the region the classname belongs to. The keys the decision
	// needs are read straight from the spawn vars, before any field is parsed.
	{
		rpSpawnRoute_t route;

		RP_SpawnRouteInit( &route );
		for ( i = 0; i < level.numSpawnVars; i++ ) {
			RP_SpawnRouteNoteKey( &route, level.spawnVars[i][0], level.spawnVars[i][1] );
		}
		ent = RP_SpawnForRoute( &route );
	}

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		// zyk: saving the spawnstring in the spawn string array, so it can be used by entity system
		level.zyk_spawn_strings[ent->s.number][j] = G_NewString(level.spawnVars[i][0]);
		level.zyk_spawn_strings[ent->s.number][j + 1] = G_NewString(level.spawnVars[i][1]);
		j += 2;

		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], ent );
	}

	level.zyk_spawn_strings_values_count[ent->s.number] = j;

	// GalaxyRP: [Entity System] an entity from a misc_bsp's sub-BSP -- see gentity_t::rpSubBSPOf
	if ( inSubBSP )
	{
		ent->rpSubBSPOf = level.rp_subbsp_spawning_instance;
	}

	// GalaxyRP fix: [Entity System] learn this map's inline-model bound HERE, before the four
	// gametype culls below. Those free the entity and return without ever calling G_CallSpawn(),
	// so a brush entity the current gametype does not want never reaches the SP_ function that
	// would otherwise teach the bound -- and the map still holds that inline model. Without this,
	// a preset saved under one gametype could name a brush model that is refused under another,
	// for no reason the admin can see. zyk_learn_inline_model() ignores anything that is not a
	// plain "*<digits>" from the main BSP.
	zyk_learn_inline_model( ent->model );

	// check for "notsingle" flag
	if ( level.gametype == GT_SINGLE_PLAYER ) {
		G_SpawnInt( "notsingle", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	}
	// check for "notteam" flag (GT_FFA, GT_DUEL, GT_SINGLE_PLAYER)
	if ( level.gametype >= GT_TEAM ) {
		G_SpawnInt( "notteam", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	} else {
		G_SpawnInt( "notfree", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	}

	if( G_SpawnString( "gametype", NULL, &value ) ) {
		if( level.gametype >= GT_FFA && level.gametype < GT_MAX_GAME_TYPE ) {
			gametypeName = gametypeNames[level.gametype];

			s = strstr( value, gametypeName );
			if( !s ) {
				ADJUST_AREAPORTAL();
				G_FreeEntity( ent );
				return;
			}
		}
	}

	// move editor origin to pos
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// if we didn't get a classname, don't bother spawning anything
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
	}

	// GalaxyRP: [Logical Entities] never for a logical entity: ICARUS keeps its per-entity state in
	// the engine, indexed by entity number, and the engine does not know this number exists.
	//Tag on the ICARUS scripting information only to valid recipients
	if ( !ent->isLogical && trap->ICARUS_ValidEnt( (sharedEntity_t *)ent ) )
	{
		trap->ICARUS_InitEnt( (sharedEntity_t *)ent );

		if ( ent->classname && ent->classname[0] )
		{
			if ( Q_strncmp( "NPC_", ent->classname, 4 ) != 0 )
			{//Not an NPC_spawner (rww - probably don't even care for MP, but whatever)
				G_ActivateBehavior( ent, BSET_SPAWN );
			}
		}
	}
}

// zyk: set an entity field with a value. Old function
void zyk_set_entity_field(gentity_t *ent, char *key, char *value) {
	if (Q_stricmp(value,"(null)") == 0)
	{ // zyk: initializing correctly the fields with null value
		if (Q_stricmp(key,"targetname") == 0)
			ent->targetname = NULL;
		else if (Q_stricmp(key,"target") == 0)
			ent->target = NULL;
		else if (Q_stricmp(key,"target2") == 0)
			ent->target2 = NULL;
		else if (Q_stricmp(key,"model2") == 0)
			ent->model2 = NULL;
		else if (Q_stricmp(key,"soundSet") == 0)
			ent->soundSet = NULL;
	}
	else
	{
		G_ParseField(key, value, ent);
	}
}

// zyk: spawns the entity. Old function
void zyk_spawn_entity(gentity_t *ent) {
	// zyk: setting these to 0 so manual entities spawned by this function (used in quests, fo example) are not bugged by the spawnvars used by the last effect spawned through new entity system
	level.zyk_spawn_strings_values_count[ent->s.number] = 0;
	level.numSpawnVars = 0;

	// move editor origin to pos
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// if we didn't get a classname, don't bother spawning anything
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
	}

	// GalaxyRP: [Logical Entities] never for a logical entity: ICARUS keeps its per-entity state in
	// the engine, indexed by entity number, and the engine does not know this number exists.
	//Tag on the ICARUS scripting information only to valid recipients
	if ( !ent->isLogical && trap->ICARUS_ValidEnt( (sharedEntity_t *)ent ) )
	{
		trap->ICARUS_InitEnt( (sharedEntity_t *)ent );

		if ( ent->classname && ent->classname[0] )
		{
			if ( Q_strncmp( "NPC_", ent->classname, 4 ) != 0 )
			{//Not an NPC_spawner (rww - probably don't even care for MP, but whatever)
				G_ActivateBehavior( ent, BSET_SPAWN );
			}
		}
	}
}

// zyk: function to set an entity field used by Entity System
//
// GalaxyRP fix: [Entity System] this had no bounds check of any kind. level.zyk_spawn_strings rows are
// [ZYK_MAX_SPAWN_STRING_SLOTS] pointers, two per key, and every unrecognised key appended another pair
// -- so the 65th distinct key on one entity wrote past the end of that entity's row and into the next
// entity's, silently corrupting a different entity's spawn vars. It is reachable one key at a time
// through /entedit, which needs no long command line to get there. The same count then drives
// zyk_main_spawn_entity()'s copy into level.spawnVars[MAX_SPAWN_VARS], so the overrun continued there.
// The row holds exactly MAX_SPAWN_VARS pairs, so one bound covers both: a key that would not fit is
// refused rather than written, and zyk_spawn_strings_full() lets the commands say so.
void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value)
{
	int i = 0;

	// GalaxyRP fix: [Entity System] every line below indexes level.zyk_spawn_strings[ent->s.number],
	// and a freed entity has s.number == 0 -- G_FreeEntity() memsets the whole struct and only
	// G_InitGentity() ever puts the number back. So called on a slot that is not in use, this wrote
	// the pair into client 0's row and bumped client 0's count, and zyk_main_spawn_entity() then read
	// that row back to spawn with.
	//
	// /entedit refuses a free slot itself now, with a message. This is here because it was not the
	// only way in: the SP map fixes in G_InitGame() reach both helpers on hardcoded entity indices
	// (see the "fixing the final door" block in g_main.c) and test nothing at all. Every legitimate
	// caller passes a G_Spawn() result, which is in use by definition, so nothing that works today
	// changes.
	if (!ent || !ent->inuse)
	{
		return;
	}

	// zyk: see if this key already exists to edit it. If not, add a new one
	while (i < level.zyk_spawn_strings_values_count[ent->s.number])
	{
		if (Q_stricmp(level.zyk_spawn_strings[ent->s.number][i], key) == 0)
		{ // zyk: found the key
			if (Q_stricmp(value, "zykremovekey") == 0)
			{ // zyk: remove the key
				// zyk: starts from the next key
				i += 2;

				// zyk: moves all keys after this one 2 positions to remove the key
				// GalaxyRP fix: [Entity System] this used to re-G_NewString() every surviving pair,
				// so removing one key from a full entity allocated up to 126 fresh copies of strings
				// that were already in the pool and already owned by this row. The slots hold
				// pointers to storage that lives as long as the map, so moving the pointers is both
				// correct and free. The slots left behind at the end are never read again because
				// the count below is what bounds every reader.
				while (i < level.zyk_spawn_strings_values_count[ent->s.number])
				{
					level.zyk_spawn_strings[ent->s.number][i - 2] = level.zyk_spawn_strings[ent->s.number][i];
					level.zyk_spawn_strings[ent->s.number][i - 1] = level.zyk_spawn_strings[ent->s.number][i + 1];

					i += 2;
				}

				// zyk: decrease the counter
				level.zyk_spawn_strings_values_count[ent->s.number] -= 2;
			}
			else
			{ // zyk: edit the key
				// GalaxyRP fix: [Entity System] this allocated a fresh copy of the key and of the value
				// on every set, from a pool that is never freed -- an /entedit that repeated a pair, a
				// move or turn that wrote the same origin or angles again, each cost the pool the
				// whole pair. The key is there already (the same key, compared without case) and is
				// kept; the value is copied only when it differs from what is stored.
				if (strcmp(level.zyk_spawn_strings[ent->s.number][i + 1], value) != 0)
				{
					level.zyk_spawn_strings[ent->s.number][i + 1] = G_NewString(value);
				}
			}

			return;
		}

		i += 2;
	}

	// zyk: a new key. Add it
	if (i + 1 >= ZYK_MAX_SPAWN_STRING_SLOTS)
	{ // GalaxyRP fix: [Entity System] no room for another pair -- drop it instead of overrunning the row
		return;
	}

	level.zyk_spawn_strings[ent->s.number][i] = G_NewString(key);
	level.zyk_spawn_strings[ent->s.number][i + 1] = G_NewString(value);

	// zyk: increases the counter
	level.zyk_spawn_strings_values_count[ent->s.number] += 2;
}

// GalaxyRP: [Entity System] see g_local.h. One string costs G_Alloc() its length plus the terminator,
// rounded up to 32; a record is read twice when it spawns (its own copy and G_ParseField's), hence twice.
static int RP_StringAllocBytes( const char *s )
{
	return ( (int)strlen( s ? s : "" ) + 1 + 31 ) & ~31;
}

int RP_PairsBytes( char **pairs, int count )
{
	int i, bytes = 64;

	if ( !pairs )
		return bytes;
	if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
		count = ZYK_MAX_SPAWN_STRING_SLOTS;
	for ( i = 0; i < count; i++ )
		bytes += RP_StringAllocBytes( pairs[i] ) * 2;

	return bytes;
}

int RP_EntityRecordBytes( const gentity_t *e )
{
	int num;

	if ( !e || !e->inuse )
		return 0;
	num = (int)( e - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL )
		return 0;

	return RP_PairsBytes( level.zyk_spawn_strings[num], level.zyk_spawn_strings_values_count[num] );
}

// GalaxyRP fix: [Entity System] true when this entity cannot take another key/value pair, so /entadd
// and /entedit can report it rather than appearing to succeed while the pair is dropped.
qboolean zyk_spawn_strings_full(gentity_t *ent)
{
	if (!ent)
		return qtrue;

	return (level.zyk_spawn_strings_values_count[ent->s.number] + 1 >= ZYK_MAX_SPAWN_STRING_SLOTS) ? qtrue : qfalse;
}

/*
=================
RP_EntityHasSpawnKeys / RP_EntityRefusalReason

GalaxyRP fix: [Entity System] which entities the entity commands may edit or remove: only the ones
that have a spawn-key record -- everything the map spawned (G_SpawnGEntityFromSpawnVars), and
everything /entadd, /entload, the default entity file, /spawnplatform, /spawndummy and the per-map
fixes built through zyk_main_set_entity_field(). An entity the game creates for itself has no record:
a player's saber entity, the trigger a door or platform makes for itself, missiles, NPCs and vehicles
(spawners included), grapple hooks, sentries, force fields, dropped items, event entities. Something
else holds a pointer to each of those -- the player's ps.saberEntityNum, the trigger's parent, the
NPC's own bookkeeping -- so freeing or respawning one out from under its owner leaves that owner
working on a freed slot, and on whatever takes the slot next. That is how /entremove on a saber
entity corrupted or crashed the server, and how /entedit on an NPC deleted it (re-running a spawn
with no classname fails, and a failed spawn frees the entity). Lugormod draws the same line: only
spawnstring-backed entities are editable.

The record cannot be stale. G_InitGentity() zeroes the count whenever a slot is handed out and
G_FreeEntity() whenever one is freed, so a count above zero always belongs to the entity in the
slot now.

RP_EntityRefusalReason() returns NULL for an entity that may be touched, or the reason it may not,
worded to follow "Entity <id> ".
=================
*/
qboolean RP_EntityHasSpawnKeys( const gentity_t *ent )
{
	return RP_EntityRefusalReason( ent ) ? qfalse : qtrue;
}

const char *RP_EntityRefusalReason( const gentity_t *ent )
{
	int num;

	if ( !ent )
		return "does not exist";

	num = (int)( ent - g_entities );

	if ( num < 0 || num >= MAX_ENTITIESTOTAL )
		return "does not exist";

	if ( num < MAX_CLIENTS + BODY_QUEUE_SIZE )
		return "is a reserved player or body slot";

	if ( num == ENTITYNUM_WORLD || num == ENTITYNUM_NONE )
		return "is the world";

	if ( !ent->inuse )
		return "is not in use";

	if ( level.zyk_spawn_strings_values_count[num] <= 0 )
	{
		if ( ent->s.eType == ET_NPC || ent->NPC )
			return va( "is an NPC (%s), created by the game rather than by the map or the entity system, and cannot be edited or removed here. Use /npc kill",
				ent->NPC_type ? ent->NPC_type : ( ent->classname ? ent->classname : "no type" ) );

		return va( "(%s) was created by the game, not by the map or the entity system, and cannot be edited or removed",
			ent->classname ? ent->classname : "no classname" );
	}

	return NULL;
}

/*
=================
RP_ClassIsSpawnPoint / RP_EntityIsSpawnPoint / RP_EntityIsCodeMadeSpawnPoint / RP_SpawnPointProtected

GalaxyRP fix: [Entity System] every info_player_* class counts as a spawn point: deathmatch (which
info_player_start becomes when it spawns), start_red/blue, duel, duel1/2, intermission and its red/blue
variants, and siegeteam1/2 -- and so do team_CTF_redspawn/bluespawn/redplayer/blueplayer. /entremove never removes the map's own (rpMapEntity) or a code-made one;
one an admin added, or a map's that was edited, it removes, since a bad one is better gone than moved --
RP_SpawnPointProtected() is that test. (A sub-BSP's spawn point goes with its misc_bsp, as its other
entities do: that rule is /entremove's own.)

The code-made ones are those the per-map fixes create for the SP and Jedi Outcast maps
(zyk_create_info_player_deathmatch, RP_CreateSpawnPoint -- through zyk_spawn_entity(), which leaves no
key/value record). /entsave cannot write an entity without a record, so no preset ever holds them: the
clearing pass of /entload and of the default entity file keeps them, or the first preset loaded on
those maps would delete them for good and leave every spawn on the fallback spot.
=================
*/
qboolean RP_ClassIsSpawnPoint( const char *classname )
{
	if ( !classname )
		return qfalse;

	// GalaxyRP fix: [Entity System] and the four CTF player spawn classes (not the flags). They were left
	// out, and the per-map fixes' code-made ones (t1_fatal, t1_rail, t1_surprise, t2_trip, t2_dpred,
	// t3_bounty: no key/value record) were deleted for good by the clearing pass of /entload and of the
	// default entity file. (The map's own were already kept from /entremove as map entities, M.)
	// RP_EntGrabPlaceBox() places them by a player's box too.
	return ( Q_stricmpn( classname, "info_player_", 12 ) == 0 ||
		!Q_stricmp( classname, "team_CTF_redspawn" ) || !Q_stricmp( classname, "team_CTF_bluespawn" ) ||
		!Q_stricmp( classname, "team_CTF_redplayer" ) || !Q_stricmp( classname, "team_CTF_blueplayer" ) ) ? qtrue : qfalse;
}

qboolean RP_EntityIsSpawnPoint( const gentity_t *ent )
{
	if ( !ent || !ent->inuse || !ent->classname )
		return qfalse;

	return RP_ClassIsSpawnPoint( ent->classname );
}

qboolean RP_EntityIsCodeMadeSpawnPoint( const gentity_t *ent )
{
	int num;

	if ( RP_EntityIsSpawnPoint( ent ) == qfalse )
		return qfalse;

	num = (int)( ent - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL )
		return qfalse;

	return ( level.zyk_spawn_strings_values_count[num] <= 0 ) ? qtrue : qfalse;
}

qboolean RP_SpawnPointProtected( const gentity_t *ent )
{
	return ( RP_EntityIsSpawnPoint( ent ) && ( ent->rpMapEntity || RP_EntityIsCodeMadeSpawnPoint( ent ) ) ) ? qtrue : qfalse;
}

// GalaxyRP fix: [Entity System] one shared brush-model setter for the eleven mover classes that
// zyk taught to carry an md3 model. Thirteen copies of the same block used to sit inline, and every
// one of them handed an unvalidated "*N" straight to trap->SetBrushModel(). The engine resolves that
// through CM_InlineModel(), which Com_Error(ERR_DROP)s on an index the map does not have -- so
// "/entadd func_static model *9999" dropped the whole server. The game module has no trap that
// reports how many inline models a map holds, but it does not need one: every entity the map itself
// spawns necessarily names a valid index, so the highest index seen while level.spawning is set is
// this map's own upper bound, and anything above it afterwards is refused.
//
// It also restores "#" sub-BSP names to the engine. The inline md3 blocks tested only for '*', so a
// "#name" model fell into the md3 branch and was registered as a bogus md3 -- vanilla and TaystJK
// both pass it to SetBrushModel, which loads it as a sub-BSP.
// GalaxyRP fix: [Entity System] raise the bound for one "*N" the map itself names. Split out of
// zyk_brush_model_allowed() below so the entity parser can call it too -- see the call in
// G_SpawnGEntityFromSpawnVars().
//
// Two things it is strict about, both because this is the only place the bound ever WIDENS:
//
//   Sub-BSP entities are skipped. A misc_bsp's entity string names inline models in the sub-BSP's
//   own index space, not the map's: SV_SetBrushModel adds sv.mLocalSubBSPModelOffset to any "*N"
//   it resolves while a sub-BSP is active (sv_gameapi.cpp). So a sub-BSP's *3 is not this map's
//   *3, and letting one in could push the bound ABOVE what the main BSP holds -- which is the
//   direction that lets a later /entadd reach CM_InlineModel() with an index the map does not
//   have, i.e. exactly the drop this guard exists to prevent. level.mBSPInstanceDepth is raised
//   around the nested spawn in SP_misc_bsp(), so it is the signal for "not our index space".
//   Nothing is lost by skipping them: a "*N" is only ever CHECKED after the map has loaded, and
//   by then sv.mLocalSubBSPIndex is -1, so every check is against the main BSP.
//
//   The digits have to be digits. atoi("*3junk") is 3, which is fine to accept -- the engine
//   reads it the same way -- but not fine to widen the bound on. Permissive when testing against
//   the bound, strict when raising it.
void zyk_learn_inline_model( const char *name )
{
	const char *p = NULL;
	int index = 0;

	if (!name || name[0] != '*')
		return;

	// GalaxyRP fix: [Entity System] only the map's own spawn pass teaches a bound. The entity parser
	// calls this for every entity it spawns, and since a misc_bsp now rebuilds its sub-BSP's entities
	// after map load through that same parser, a rebuilt entity would otherwise raise the very bound
	// it is about to be checked against.
	if (level.spawning == qfalse)
		return;

	for (p = name + 1; *p; p++)
	{
		if (*p < '0' || *p > '9')
			return;
	}

	// zyk: "*" on its own needs no test of its own -- atoi("") is 0, and 0 never beats a bound
	// that starts at 0 and only rises
	index = atoi(name + 1);

	// GalaxyRP fix: [Entity System] a sub-BSP entity's "*N" is in that sub-BSP's numbering: it never
	// widens the map's own bound (see above), but it is that sub-BSP's bound, which a misc_bsp
	// rebuilding its entities after map load is checked against -- see zyk_brush_model_allowed().
	if (level.mBSPInstanceDepth > 0)
	{
		if (level.rp_active_subbsp > 0 && level.rp_active_subbsp <= RP_MAX_SUBBSP_NAMES &&
			index > level.rp_subbsp_max_inline[level.rp_active_subbsp - 1])
		{
			level.rp_subbsp_max_inline[level.rp_active_subbsp - 1] = index;
		}
		return;
	}

	if (index > level.zyk_max_inline_model)
		level.zyk_max_inline_model = index;
}

// GalaxyRP fix: [Entity System] the "#name" counterpart of the inline-model bound above. The engine
// resolves a "#name" model through CM_LoadSubBSP(), which Com_Error(ERR_DROP)s -- a process exit on a
// dedicated server -- both when maps/<name>.bsp cannot be loaded and when a 33rd unique sub-BSP is
// asked for. So once the map has loaded, the only "#name" that is safe is one the map already
// loaded while it spawned: CM_LoadSubBSP() finds that one by name and returns it without loading
// anything. This records those names. It is only called while level.spawning is set, from the two
// places that hand a "#name" to trap->SetBrushModel() right afterwards (zyk_brush_model_allowed()
// and SP_misc_bsp()), so a name is learned exactly when the map itself loads it -- a map naming a
// sub-BSP that is not there never gets past that SetBrushModel() in the first place.
void zyk_learn_subbsp_name( const char *name )
{
	if (!name || name[0] != '#' || strlen(name) >= MAX_QPATH)
		return;

	if (zyk_subbsp_name_known(name) == qtrue)
		return;

	if (level.rp_num_subbsp_names >= RP_MAX_SUBBSP_NAMES)
		return;

	Q_strncpyz(level.rp_subbsp_names[level.rp_num_subbsp_names], name, sizeof(level.rp_subbsp_names[0]));
	level.rp_num_subbsp_names++;
}

qboolean zyk_subbsp_name_known( const char *name )
{
	return (zyk_subbsp_name_slot(name) >= 0) ? qtrue : qfalse;
}

// zyk: the slot of a learned "#name" in level.rp_subbsp_names, or -1
int zyk_subbsp_name_slot( const char *name )
{
	int i = 0;

	if (!name || name[0] != '#')
		return -1;

	for (i = 0; i < level.rp_num_subbsp_names; i++)
	{
		// zyk: the engine matches sub-BSP file names case-insensitively, so this does too
		if (Q_stricmp(level.rp_subbsp_names[i], name) == 0)
			return i;
	}

	return -1;
}

/*
=================
RP_MiscBspForInstance / RP_FreeEntityTriggers / RP_FreeSubBSPEntities

GalaxyRP: [Entity System] the misc_bsp with this instance number, if it is still there; freeing the
game-made triggers whose parent is an entity (a door's or platform's own trigger -- see RP_EntRemoveFree()
in g_cmds.c for why they must go with it); and freeing everything a misc_bsp's sub-BSP spawned, each
with its triggers. Called when a misc_bsp is removed (/entremove, /entundo) or about to spawn again in
place (/entedit), so its entities neither outlive it nor come back twice. Returns how many were freed.
=================
*/
gentity_t *RP_MiscBspForInstance( int instance )
{
	gentity_t *e;

	if (instance <= 0)
		return NULL;

	RP_FOR_EACH_ENTITY( e )
	{
		if (e->inuse && e->rpBSPInstance == instance)
			return e;
	}

	return NULL;
}

int RP_FreeEntityTriggers( gentity_t *target )
{
	gentity_t *other;
	int children = 0;

	RP_FOR_EACH_ENTITY( other )
	{
		if ( other == target || !other->inuse || other->parent != target )
			continue;
		if ( !(other->r.contents & CONTENTS_TRIGGER) )
			continue;
		if ( RP_EntityHasSpawnKeys( other ) )
			continue;

		G_FreeEntity( other );
		children++;
	}

	return children;
}

/*
=================
RP_MarkChild / RP_IsChildOf / RP_FreeEntityChildren

GalaxyRP: [Entity System] see gentity_t::rpMadeBy. RP_FreeEntityChildren() frees what an entity made for
itself and that has no record of its own: the trigger a door or platform made (RP_FreeEntityTriggers) and
every entity stamped as its child -- a misc_turret's top (turret_base_spawn_top), a misc_trip_mine's laser
trap (SP_misc_trip_mine). Called wherever the maker is removed by command (/entremove, /entundo, a cut
entity hidden) or about to spawn again in place (/entedit, /entrotate, a dropped /entcut -- its spawn
function makes them anew). Returns how many were freed. A child's own death or removal needs nothing
here: G_FreeEntity() zeroes the link.
=================
*/
void RP_MarkChild( gentity_t *maker, gentity_t *child )
{
	if ( !maker || !child || maker == child )
		return;

	if ( !maker->rpMakerStamp )
		maker->rpMakerStamp = ++level.rp_next_maker_stamp;

	child->rpMadeBy = maker;
	child->rpMadeByStamp = maker->rpMakerStamp;
}

qboolean RP_IsChildOf( const gentity_t *maker, const gentity_t *child )
{
	return ( maker && child && child != maker && child->inuse && child->rpMadeBy == maker
		&& maker->rpMakerStamp && child->rpMadeByStamp == maker->rpMakerStamp ) ? qtrue : qfalse;
}

int RP_FreeEntityChildren( gentity_t *maker )
{
	gentity_t *other;
	int children;

	if ( !maker )
		return 0;

	children = RP_FreeEntityTriggers( maker );

	if ( !maker->rpMakerStamp )
		return children;

	RP_FOR_EACH_ENTITY( other )
	{
		if ( !RP_IsChildOf( maker, other ) )
			continue;

		G_FreeEntity( other );
		if ( !other->inuse )
			children++;
	}

	return children;
}

int RP_FreeSubBSPEntities( int instance )
{
	gentity_t *e;
	int freed = 0;

	if (instance <= 0)
		return 0;

	RP_FOR_EACH_ENTITY( e )
	{
		if (!e->inuse || e->rpSubBSPOf != instance || e->s.number < MAX_CLIENTS + BODY_QUEUE_SIZE)
			continue;

		// zyk: a nested misc_bsp's own entities first; instance numbers only grow, so a nested
		// one's is always higher than this one's and this cannot come back round to it
		if (e->rpBSPInstance > instance)
			freed += RP_FreeSubBSPEntities( e->rpBSPInstance );

		// zyk: and the trigger a door or platform among them made for itself -- see RP_EntRemoveFree()
		RP_FreeEntityTriggers( e );
		G_FreeEntity( e );
		if (!e->inuse)
			freed++;
	}

	return freed;
}

// GalaxyRP fix: [Entity System] whether a model name may be handed to trap->SetBrushModel(). Every
// caller does that straight afterwards when this says yes, and SV_SetBrushModel() Com_Error(ERR_DROP)s
// on anything it cannot resolve, so this has to be exact about all three shapes it can be given:
//
//   "*N"    an inline model of the map's own BSP -- bounded by what the map used (see above), and after
//           map load a plain number of 1 or more (see the end of the function).
//   "#name" a sub-BSP -- only one the map itself loaded (see zyk_learn_subbsp_name()).
//   anything else, including NULL and "" -- never. SV_SetBrushModel() answers "isn't a brush model"
//           with ERR_DROP. zyk_set_brush_model() sends md3 names down its own path and never asks,
//           but InitTrigger() and SP_trigger_asteroid_field() ask about whatever the "model" key
//           holds, so "/entadd trigger_multiple model foo" used to reach that ERR_DROP.
// DAJ_RP fix: [Entity System] the number a "*N" model names, read strictly: -1 when what follows the "*" is
// not all digits ("*", "*abc", "* 5", "*3abc", "*-0", "*+3"), INT_MAX when it is too long to be a model
// number (more than 9 digits), otherwise N ("*03" is 3). The engine reads the same text with atoi(), so
// anything that is not a plain number became *0 there -- the map's own world model, see below.
static int zyk_inline_model_number( const char *name )
{
	int i;

	if ( !name || name[0] != '*' || !name[1] )
		return -1;

	for ( i = 1; name[i]; i++ )
	{
		if ( name[i] < '0' || name[i] > '9' )
			return -1;
	}

	if ( i - 1 > 9 )
		return INT_MAX;

	return atoi( name + 1 );
}

qboolean zyk_brush_model_allowed( gentity_t *ent, const char *name )
{
	int index = 0;

	if (!name || (name[0] != '*' && name[0] != '#'))
	{
		G_LogPrintf("model \"%s\" refused on entity %d: not a brush model (\"*N\" or \"#name\")\n",
			name ? name : "", ent ? ent->s.number : -1);

		return qfalse;
	}

	if (name[0] == '#')
	{
		if (level.spawning == qtrue)
		{ // zyk: the map's own entities define which sub-BSPs this map loads
			zyk_learn_subbsp_name( name );

			return qtrue;
		}

		if (zyk_subbsp_name_known(name) == qfalse)
		{
			G_LogPrintf("brush model %s refused on entity %d: this map did not load that sub-BSP\n",
				name, ent ? ent->s.number : -1);

			return qfalse;
		}

		return qtrue;
	}

	index = atoi(name + 1);

	if (level.spawning == qtrue)
	{ // zyk: the map's own entities define what this map considers a valid index
		zyk_learn_inline_model( name );

		return qtrue;
	}

	// GalaxyRP fix: [Entity System] a misc_bsp rebuilding its sub-BSP's entities after map load: the
	// "*N" counts in that sub-BSP's numbering, so it is checked against that sub-BSP's own bound
	if (level.mBSPInstanceDepth > 0 && level.rp_active_subbsp > 0 && level.rp_active_subbsp <= RP_MAX_SUBBSP_NAMES)
	{
		int bound = level.rp_subbsp_max_inline[level.rp_active_subbsp - 1];

		if (index < 0 || index > bound)
		{
			G_LogPrintf("brush model %s refused on entity %d: this sub-BSP only has inline models up to *%d\n",
				name, ent ? ent->s.number : -1, bound);

			return qfalse;
		}

		return qtrue;
	}

	// DAJ_RP fix: [Entity System] and after map load a "*N" of the map's own must be a plain number of 1 or
	// more. *0 is the map's world model (CM_InlineModel(0)), and anything that is not a plain number reached
	// the engine as *0 too: an entity with it was never drawn, but every client predicted collisions with an
	// invisible copy of the whole map moved by the entity's origin, a trigger fired wherever a player touched
	// that copy, and a door's walk-up trigger covered the whole map. No map entity uses *0 (the map's own
	// spawn pass above is not held to this). The sub-BSP branch above is left as it was: a sub-BSP's first
	// model can be real architecture.
	{
		const int number = zyk_inline_model_number(name);

		if (number < 1)
		{
			G_LogPrintf("brush model %s refused on entity %d: %s\n", name, ent ? ent->s.number : -1,
				(number == 0) ? "*0 is the map's world model, not one of its brush models" : "not a brush model number");

			return qfalse;
		}

		// (the number read strictly: atoi() of more digits than an int holds is undefined)
		index = number;
	}

	if (index < 0 || index > level.zyk_max_inline_model)
	{
		G_LogPrintf("brush model %s refused on entity %d: this map only has inline models up to *%d\n",
			name, ent ? ent->s.number : -1, level.zyk_max_inline_model);

		return qfalse;
	}

	return qtrue;
}

/*
==================
RP_BrushModelProblem

DAJ_RP: [Entity System] what is wrong with a "model" key naming a brush model, for an entity the Entity System
makes -- or NULL: when it is fine, and when it names no brush model at all (an .md3, nothing), which is not
this check's business. A "*N" must be a plain number from 1 to the last of the map's own inline models
(level.zyk_max_inline_model, learned from the map's brush entities -- see zyk_learn_inline_model()), a "#name"
a sub-BSP the map loaded: the same bounds zyk_brush_model_allowed() holds a model to after map load, with the
reason worded for the admin. Used
by /entedit before an edit is applied (g_cmds.c) and by RP_EntitySystemSpawnRefused() for every other way such
an entity spawns, so one is refused with the reason given instead of spawning with no model.
==================
*/
const char *RP_BrushModelProblem( const char *name )
{
	static char problem[192];
	char shown[48];
	int number = 0;
	int i;

	if ( !name || ( name[0] != '*' && name[0] != '#' ) )
	{
		return NULL;
	}

	if ( name[0] == '#' )
	{
		if ( zyk_subbsp_name_known( name ) )
		{
			return NULL;
		}
	}
	else
	{
		// DAJ_RP fix: [Entity System] a plain number from *1 up -- see zyk_inline_model_number() and the end of
		// zyk_brush_model_allowed(): *0 and anything the engine reads as *0 are the map's world model
		number = zyk_inline_model_number( name );

		if ( number >= 1 && number <= level.zyk_max_inline_model )
		{
			return NULL;
		}
	}

	// shown in a print "..." line: no quote, line break or ; from the value
	Q_strncpyz( shown, RP_ShownText( name ), sizeof( shown ) );
	for ( i = 0; shown[i]; i++ )
	{
		if ( shown[i] == '"' || shown[i] == '\n' || shown[i] == '\r' || shown[i] == ';' )
		{
			shown[i] = '?';
		}
	}

	if ( name[0] == '#' )
	{
		Com_sprintf( problem, sizeof( problem ), "model %s is not a sub-BSP this map loaded", shown );
	}
	else if ( level.zyk_max_inline_model < 1 )
	{
		Com_sprintf( problem, sizeof( problem ), "model %s cannot be used: this map has no brush models of its own", shown );
	}
	else if ( number < 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "model %s is not a brush model number (*1 to *%d)", shown, level.zyk_max_inline_model );
	}
	else if ( number == 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "model %s is the map itself, not one of its brush models (*1 to *%d)", shown, level.zyk_max_inline_model );
	}
	else
	{
		Com_sprintf( problem, sizeof( problem ), "model %s is not one of this map's brush models (*1 to *%d)", shown, level.zyk_max_inline_model );
	}

	return problem;
}

void zyk_set_brush_model( gentity_t *ent )
{
	if (!ent || !ent->model)
		return;

	if (ent->model[0] == '*' || ent->model[0] == '#')
	{ // zyk: a brush model or a sub-BSP -- the engine resolves both of these
		if (zyk_brush_model_allowed(ent, ent->model) == qfalse)
		{
			// DAJ_RP fix: [Entity System] no brush model left behind either. An entity spawned again in
			// place keeps its struct, and one that had a brush model kept r.bmodel: the server then sent it
			// as SOLID_BMODEL with modelindex 0, which is the whole map's world model -- every client's
			// prediction collided with an invisible copy of the map, moved by the entity's origin.
			ent->r.bmodel = qfalse;
			ent->s.modelindex = 0;
			return;
		}

		trap->SetBrushModel( (sharedEntity_t *)ent, ent->model );

		return;
	}

	// zyk: md3 model
	// GalaxyRP: [Slot Reuse] reusable once the Entity System's props using it are gone (g_utils.c)
	ent->s.modelindex = RP_EntityModelIndex( ent, ent->model );

	// GalaxyRP fix: [Entity System] not a brush model. An entity spawned again in place (/entedit,
	// /entrotate) keeps its struct, so one whose model was "*N" still had r.bmodel set from
	// SetBrushModel: the server clipped against inline model N, and the slot scan (RP_SlotMarkUsed)
	// read its md3 index as that inline number.
	ent->r.bmodel = qfalse;

	// zyk: is a solid model
	if (ent->spawnflags & 1024)
		ent->r.contents = CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_BODY|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP;//Was CONTENTS_SOLID, but only architecture should be this

	// zyk: setting angles so if it is a md3 model (entity system) it will rotate it with these angles
	VectorCopy( ent->s.angles2, ent->r.currentAngles );
	VectorCopy( ent->s.angles2, ent->s.apos.trBase );
}

/*
=================
RP_EntitySystemMade / RP_EntitySystemSpawnRefused

GalaxyRP: [Entity System] RP_EntitySystemMade(): whether this entity is one the Entity System made --
/entadd, /entaddaim, a copy, an entity file's line -- rather than the map's own. It has a key/value
record, the map (with its per-map fixes) had finished loading when it spawned, it is not marked as the
map's (rpMapEntity: an entity file's line for a map entity keeps that mark), and no misc_bsp's sub-BSP
spawned it. Entities the game makes for itself have no record and never count. The checks below and
the model box of a misc_model_breakable (SP_misc_model_breakable) apply to these only, so no map
changes what it spawns or how.

RP_EntitySystemSpawnRefused(): for such an entity, called by zyk_main_spawn_entity() before the spawn
function runs. A misc_model_breakable, or a func_ class carrying an md3 model, whose model file the
server does not have is refused, and so is an fx_runner whose effect file it does not have: registering
the name would take one of the map's 512 model or 64 effect slots and part of the gamestate every client
downloads, for the rest of the map, for something nobody can see. A model path that is not there as
given is tried as models/<path>, with .md3 added if it has no extension, so "map_objects/crate" works;
the record keeps what was typed. A misc_model_breakable must end up with an .md3 model, as its spawn
function requires. The reason is logged and left in level.rp_spawn_refusal for the command to print.
=================
*/
qboolean RP_EntitySystemMade( const gentity_t *ent )
{
	int num;

	if ( !ent || !level.rp_map_loaded || ent->rpMapEntity || ent->rpSubBSPOf > 0 )
		return qfalse;

	num = (int)( ent - g_entities );
	if ( num < MAX_CLIENTS + BODY_QUEUE_SIZE || num >= MAX_ENTITIESTOTAL )
		return qfalse;

	return ( level.zyk_spawn_strings_values_count[num] > 0 ) ? qtrue : qfalse;
}

// GalaxyRP: [Slot Reuse] the func_ classes whose spawn function registers the "model" key (through
// zyk_set_brush_model) and "model2" -- all of them but these two, which never look at either
static qboolean RP_FuncRegistersModels( const char *classname )
{
	return ( Q_stricmpn( classname, "func_", 5 ) == 0 && Q_stricmp( classname, "func_group" ) != 0
		&& Q_stricmp( classname, "func_timer" ) != 0 ) ? qtrue : qfalse;
}

// GalaxyRP: [Slot Reuse] the sound misc_model_breakable_init() registers for a breakable that can be
// damaged, and whether it is registered already
#define RP_BREAKABLE_SOUND "sound/weapons/explosions/cargoexplode.wav"

static qboolean RP_SoundRegistered( const char *name )
{
	char s[MAX_STRING_CHARS];
	int i;

	for ( i = 1; i < MAX_SOUNDS; i++ )
	{
		trap->GetConfigstring( CS_SOUNDS + i, s, sizeof( s ) );
		if ( !s[0] )
			return qfalse;
		if ( !strcmp( s, name ) )
			return qtrue;
	}
	return qfalse;
}

static qboolean RP_SpawnRefuse( gentity_t *ent, const char *reason )
{
	Q_strncpyz( level.rp_spawn_refusal, reason, sizeof( level.rp_spawn_refusal ) );
	G_LogPrintf( "Entity %d (%s) refused: %s\n", ent->s.number, ent->classname ? ent->classname : "noclass", reason );
	return qtrue;
}

/*
==================
RP_SoundSetKnown

GalaxyRP fix: [Entity System] whether an ambient sound set ("soundSet" on a target_speaker, a mover, a
misc_model_breakable...) is one this server's clients can load. A client loads every name in the
CS_AMBIENT_SET configstrings from sound/sound.txt (AS_ParseSets(), snd_ambient.cpp) and DROPS itself
("missing sound sets") on one that file does not define -- so a misspelled soundSet the Entity System
let through locked every player who connected afterwards out of the server for the rest of the map, and
now that cgame loads a set that arrives mid-game (CG_ConfigStringModified()), it would drop everyone
already connected too. The map's own names are the map's business; this is asked of the entities the
Entity System makes (RP_EntitySystemSpawnRefused()).

The names are read from the same file the client reads, once per map, the way the client finds them: a
line that starts with generalSet, localSet or bmodelSet (case-sensitive, as the client compares them),
then -- past one character -- the first word. Names are compared ignoring case, as the client's name map
does (sstring_t). If the server cannot read the file, only a name the map itself has already registered
is accepted: every client has loaded those.

GalaxyRP: [Entity System] each set's "radius" is kept as well, read the way the client reads it
(AS_GetLocalSet(), snd_ambient.cpp), for RP_SoundSetRadius(): only a localSet's body has one, and the
body is the lines after the set's name line for as long as each one starts with a word the client
takes there -- timeBetweenWaves, subWaves, loopedWave, volRange or radius, in any case. The first other
word ends it (a ';' comment, the next set's name, anything). Without a radius line, or in any other
kind of set, it is the client's default, 250.
==================
*/
#define RP_MAX_KNOWN_SOUNDSETS	2048
#define RP_SOUNDSET_NAME_LEN	128
#define RP_SOUNDSET_DEFAULT_RADIUS	250	// the client's default (CSetGroup::AddSet(), snd_ambient.cpp)
static char rp_soundSetNames[RP_MAX_KNOWN_SOUNDSETS][RP_SOUNDSET_NAME_LEN];
static int rp_soundSetRadius[RP_MAX_KNOWN_SOUNDSETS];
static int rp_numSoundSetNames = 0;

// GalaxyRP: [Entity System] whether a line's first word is one the client reads in a localSet's body
static qboolean RP_SoundSetBodyWord( const char *word, qboolean *isRadius )
{
	static const char *bodyWords[] = { "timeBetweenWaves", "subWaves", "loopedWave", "volRange", "radius" };
	int i;

	*isRadius = qfalse;
	for ( i = 0; i < (int)ARRAY_LEN( bodyWords ); i++ )
	{
		if ( !Q_stricmp( word, bodyWords[i] ) )
		{
			*isRadius = ( i == (int)ARRAY_LEN( bodyWords ) - 1 ) ? qtrue : qfalse;
			return qtrue;
		}
	}
	return qfalse;
}

static void RP_LoadSoundSetNames( void )
{
	static const char *keywords[] = { "generalSet", "localSet", "bmodelSet" };
	fileHandle_t f = 0;
	char *buf = NULL;
	const char *p;
	int len, k, overflow = 0;
	int body = -1;	// the localSet whose body the lines are in, -1 outside one

	level.rp_soundsets_state = -1;
	rp_numSoundSetNames = 0;

	len = trap->FS_Open( "sound/sound.txt", &f, FS_READ );
	if ( !f || len <= 0 )
	{
		if ( f )
			trap->FS_Close( f );
		G_LogPrintf( "sound/sound.txt could not be read: only sound sets the map uses are accepted from the entity commands\n" );
		return;
	}

	trap->TrueMalloc( (void **)&buf, len + 1 );
	if ( !buf )
	{
		trap->FS_Close( f );
		return;
	}
	trap->FS_Read( buf, len, f );
	trap->FS_Close( f );
	buf[len] = '\0';

	for ( p = buf; *p; )
	{
		qboolean setLine = qfalse;

		for ( k = 0; k < (int)ARRAY_LEN( keywords ); k++ )
		{
			const int kwLen = (int)strlen( keywords[k] );
			const char *q;
			int n = 0;

			if ( strncmp( p, keywords[k], kwLen ) != 0 )
				continue;

			setLine = qtrue;
			body = -1;

			// the client steps past the keyword and one more character, then reads a word with sscanf
			q = p + kwLen;
			if ( *q )
				q++;
			while ( *q && isspace( (unsigned char)*q ) )
				q++;

			if ( rp_numSoundSetNames >= RP_MAX_KNOWN_SOUNDSETS )
			{
				overflow++;
				break;
			}
			while ( q[n] && !isspace( (unsigned char)q[n] ) && n < RP_SOUNDSET_NAME_LEN - 1 )
			{
				rp_soundSetNames[rp_numSoundSetNames][n] = q[n];
				n++;
			}
			rp_soundSetNames[rp_numSoundSetNames][n] = '\0';
			rp_soundSetRadius[rp_numSoundSetNames] = RP_SOUNDSET_DEFAULT_RADIUS;
			if ( n > 0 && ( !q[n] || isspace( (unsigned char)q[n] ) ) )
			{
				if ( k == 1 )	// localSet: its body may give a radius
					body = rp_numSoundSetNames;
				rp_numSoundSetNames++;	// a name too long to keep whole is left out: it can never match
			}
			break;
		}

		if ( !setLine && body >= 0 )
		{
			char word[64];
			qboolean isRadius;

			// the client reads the first word wherever it is -- blank lines are passed over
			if ( sscanf( p, "%63s", word ) == 1 )
			{
				if ( !RP_SoundSetBodyWord( word, &isRadius ) )
				{
					body = -1;	// any other word ends the body
				}
				else if ( isRadius )
				{
					int radius;

					// as the client: "%s %d" from the line on, the number left as it was when none follows
					if ( sscanf( p, "%*s %d", &radius ) == 1 )
						rp_soundSetRadius[body] = radius;
				}
			}
			else
			{
				body = -1;	// nothing left in the file
			}
		}

		// on to the next line: the client ends a line at either '\r' or '\n'
		while ( *p && *p != '\n' && *p != '\r' )
			p++;
		if ( *p )
			p++;
	}

	trap->TrueFree( (void **)&buf );
	level.rp_soundsets_state = 1;

	if ( overflow )
		G_LogPrintf( "sound/sound.txt: more than %d sound sets; the rest are not accepted from the entity commands\n", RP_MAX_KNOWN_SOUNDSETS );
}

qboolean RP_SoundSetKnown( const char *name )
{
	int i;

	if ( !name || !name[0] )
		return qfalse;

	if ( level.rp_soundsets_state == 0 )
		RP_LoadSoundSetNames();

	if ( level.rp_soundsets_state > 0 )
	{
		for ( i = 0; i < rp_numSoundSetNames; i++ )
		{
			if ( !Q_stricmp( rp_soundSetNames[i], name ) )
				return qtrue;
		}
		return qfalse;
	}

	for ( i = 1; i < MAX_AMBIENT_SETS; i++ )
	{
		char registered[MAX_QPATH];

		trap->GetConfigstring( CS_AMBIENT_SET + i, registered, sizeof( registered ) );
		if ( !registered[0] )
			break;
		if ( !Q_stricmp( registered, name ) )
			return qtrue;
	}
	return qfalse;
}

/*
==================
RP_SoundSetRadius

GalaxyRP: [Entity System] the radius sound/sound.txt gives an ambient set, the distance at which the
client's volume for it reaches 0 (AS_PlayLocalSet(), snd_ambient.cpp): RP_LoadSoundSetNames() keeps it.
A name the file defines twice takes its last definition, as the client's name map does. qfalse when the
server could not read the file; a name it does not define gets the client's default.
==================
*/
qboolean RP_SoundSetRadius( const char *name, int *radius )
{
	int i;

	*radius = RP_SOUNDSET_DEFAULT_RADIUS;

	if ( level.rp_soundsets_state == 0 )
		RP_LoadSoundSetNames();

	if ( level.rp_soundsets_state <= 0 )
		return qfalse;

	if ( !name || !name[0] )
		return qtrue;

	for ( i = rp_numSoundSetNames - 1; i >= 0; i-- )
	{
		if ( !Q_stricmp( rp_soundSetNames[i], name ) )
		{
			*radius = rp_soundSetRadius[i];
			break;
		}
	}
	return qtrue;
}

void RP_SpawnSaysWhy( gentity_t *ent, const char *reason )
{
	if ( !ent || !reason || !RP_EntitySystemMade( ent ) )
		return;
	Q_strncpyz( level.rp_spawn_refusal, reason, sizeof( level.rp_spawn_refusal ) );
}

/*
=================
RP_EffectKeyValue

DAJ_RP: [Weather] an effect key's value (func_breakable's playfx, misc_siege_item's deathfx and respawnfx,
trigger_lightningstrike's lightningfx) as its spawn function should use it. One starting with * is not an
effect but a weather command, which every player's game would run with none of the weather entities'
checks. For one the Entity System made RP_EntitySystemSpawnRefused() has refused the entity already; a
map's own keeps the entity -- a breakable or a siege item is part of the map -- and loses the key: ""
comes back, as if the key were not there, and the server log says why.
=================
*/
char *RP_EffectKeyValue( gentity_t *ent, const char *key, char *value )
{
	static char none[1] = "";

	if ( !value || value[0] != '*' )
		return value;

	G_LogPrintf( "%s at %s: %s %s is a weather command, not an effect; it is left out\n",
		( ent && ent->classname ) ? ent->classname : "noclass", ent ? vtos( ent->s.origin ) : "?", key, value );
	return none;
}

qboolean RP_RefuseAtRuntime( gentity_t *ent, const char *reason )
{
	if ( !ent || !RP_EntitySystemMade( ent ) )
		return qfalse;

	RP_SpawnRefuse( ent, reason ? reason : "not for the entity commands" );
	G_FreeEntity( ent );
	return qtrue;
}

/*
=================
RP_ModelKeyClass

DAJ_RP: [Dispensers] the classes that show a model of their own: the ammo and shield floor units, the three
model power converters and the cargo crate, whose "model" key, when given, replaces it, and the two racks,
which always show theirs, as single player does (their key is not used). On one the Entity System makes,
RP_EntitySystemSpawnRefused() checks a "model" key it uses as it checks a misc_model_breakable's -- the file
must be on the server and be an .md3 -- and that the model it will show, the key's or its own, gets a slot:
a typo used to take a model slot for good and leave an invisible station, and a full table an invisible
one. Any other one (the map's own) is not refused: its spawn function shows its own model instead and logs
why -- RP_DispenserModel(). NULL for any other class.

GalaxyRP: [Entity System] the saber training dummy (zyk_training_pole) is one of them too: its "model" key,
when given, replaces the Rift statue.

GalaxyRP: [Slot Reuse] and the sounds its spawn function registers after the model -- a dispenser's three --
get a slot and their bytes too: with the gamestate nearly full, the model fitted and the sounds did not, and
the station was silent, its "done" sound index 0 an empty name every client tried to load on every use.
Keep each list the same as its spawn function's G_SoundIndex() calls (g_misc.c). (The radar icon a floor
unit or the health converter registers in siege only is not counted.)
=================
*/
#define RP_MODEL_KEY_SOUNDS	3

typedef struct {
	const char	*classname;
	const char	*defaultModel;
	qboolean	breakable;	// spawned as a misc_model_breakable: it registers the breakable's sound
	qboolean	keyUnused;	// its "model" key is not used: it always shows defaultModel (the racks)
	const char	*sounds[RP_MODEL_KEY_SOUNDS];	// what its spawn function registers besides (NULL: no more)
} rpModelKeyClass_t;

#define RP_SOUNDS_AMMOCON	{ "sound/interface/ammocon_run", "sound/interface/ammocon_done", "sound/interface/ammocon_empty" }
#define RP_SOUNDS_SHIELDCON	{ "sound/interface/shieldcon_run", "sound/interface/shieldcon_done", "sound/interface/shieldcon_empty" }
#define RP_SOUNDS_HEALTHCON	{ "sound/player/pickuphealth.wav", "sound/interface/shieldcon_done", "sound/interface/shieldcon_empty" }
#define RP_SOUNDS_NONE		{ NULL, NULL, NULL }

static const rpModelKeyClass_t rp_model_key_classes[] = {
	{ "misc_ammo_floor_unit",				RP_MODEL_AMMO_FLOOR_UNIT,	qfalse,	qfalse,	RP_SOUNDS_AMMOCON },
	{ "misc_shield_floor_unit",				RP_MODEL_SHIELD_FLOOR_UNIT,	qfalse,	qfalse,	RP_SOUNDS_SHIELDCON },
	{ "misc_model_ammo_power_converter",	RP_MODEL_POWER_CONVERTER,	qfalse,	qfalse,	RP_SOUNDS_AMMOCON },
	{ "misc_model_shield_power_converter",	RP_MODEL_POWER_CONVERTER,	qfalse,	qfalse,	RP_SOUNDS_SHIELDCON },
	{ "misc_model_health_power_converter",	RP_MODEL_POWER_CONVERTER,	qfalse,	qfalse,	RP_SOUNDS_HEALTHCON },
	{ "misc_model_gun_rack",				RP_MODEL_GUN_RACK,			qfalse,	qtrue,	RP_SOUNDS_NONE },
	{ "misc_model_ammo_rack",				RP_MODEL_AMMO_RACK,			qfalse,	qtrue,	RP_SOUNDS_NONE },
	{ "misc_model_cargo_small",				RP_MODEL_CARGO_SMALL,		qtrue,	qfalse,	RP_SOUNDS_NONE },
	// GalaxyRP: [Entity System] the saber training dummy: its "model" key checked as these are (a typo used
	// to take a model slot for good and leave an invisible but solid dummy) -- SP_ZykTrainingPole()
	{ "zyk_training_pole",					RP_MODEL_TRAINING_POLE,		qfalse,	qfalse,	RP_SOUNDS_NONE },
};

static const rpModelKeyClass_t *RP_ModelKeyClass( const char *classname )
{
	int i;

	if ( !classname )
		return NULL;

	for ( i = 0; i < (int)ARRAY_LEN( rp_model_key_classes ); i++ )
	{
		if ( !Q_stricmp( classname, rp_model_key_classes[i].classname ) )
			return &rp_model_key_classes[i];
	}

	return NULL;
}

/*
=================
RP_FindModelFile / RP_DispenserModel

DAJ_RP: [Dispensers] RP_FindModelFile(): where a model key's file is -- as typed (1), or under models/ with
".md3" added when the name has no extension (2, the path in candidate) -- the lookup misc_model_breakable
has always had in RP_EntitySystemSpawnRefused(); 0 when neither is on the server.

RP_DispenserModel(): the model a dispenser or cargo crate shows (RP_ModelKeyClass()), set in
ent->model: its "model" key's file, found as above and an .md3, or defaultModel -- with no key, and, for
an entity the Entity System did not make (that one was refused instead), with a key whose file is not on
the server or is not an .md3, which is logged.

DAJ_RP: [SP Maps] RP_FindModelFileExt(): the same lookup with the extension added to a name that has none
given -- ".glm" for a misc_model_ghoul (RP_GhoulModel() in g_misc.c); RP_FindModelFile() is it with ".md3".
=================
*/
int RP_FindModelFileExt( const char *model, const char *ext, char *candidate, int candidateSize )
{
	const char *base;
	const char *path = model;
	qboolean hasExtension;

	if ( !model || !model[0] )
		return 0;

	if ( RP_FileExists( model ) )
		return 1;

	base = strrchr( model, '/' );
	while ( *path == '/' )
		path++;
	base = base ? base + 1 : path;
	hasExtension = strchr( base, '.' ) ? qtrue : qfalse;

	if ( (int)( strlen( path ) + 7 + strlen( ext ) ) >= candidateSize )
		return 0;

	Com_sprintf( candidate, candidateSize, "%s%s%s", Q_stricmpn( path, "models/", 7 ) ? "models/" : "",
		path, hasExtension ? "" : ext );

	return RP_FileExists( candidate ) ? 2 : 0;
}

static int RP_FindModelFile( const char *model, char *candidate, int candidateSize )
{
	return RP_FindModelFileExt( model, ".md3", candidate, candidateSize );
}

static qboolean RP_IsMd3Name( const char *model )
{
	const size_t len = model ? strlen( model ) : 0;

	return ( len >= 5 && Q_stricmp( model + len - 4, ".md3" ) == 0 ) ? qtrue : qfalse;
}

const char *RP_DispenserModel( gentity_t *ent, const char *defaultModel )
{
	char candidate[MAX_QPATH];
	int where;

	if ( !ent->model || !ent->model[0] )
	{
		ent->model = (char *)defaultModel;
		return ent->model;
	}

	where = RP_FindModelFile( ent->model, candidate, sizeof( candidate ) );
	if ( where == 2 )
		ent->model = G_NewStringRaw( candidate );

	if ( where && RP_IsMd3Name( ent->model ) )
		return ent->model;

	G_LogPrintf( "%s %d at %s: model %s %s; %s is shown instead\n", ent->classname ? ent->classname : "noclass",
		ent->s.number, vtos( ent->s.origin ), RP_ShownText( ent->model ),
		where ? "is not an .md3 model" : "is not on the server", defaultModel );
	ent->model = (char *)defaultModel;
	return ent->model;
}

qboolean RP_EntitySystemSpawnRefused( gentity_t *ent )
{
	qboolean breakable, md3Mover;
	const rpModelKeyClass_t *keyClass;

	level.rp_spawn_refusal[0] = '\0';

	if ( !RP_EntitySystemMade( ent ) || !ent->classname )
		return qfalse;

	// GalaxyRP fix: [Entity System] an ambient sound set its clients can load -- see RP_SoundSetKnown()
	if ( ent->soundSet && ent->soundSet[0] && !RP_SoundSetKnown( ent->soundSet ) )
	{
		char shown[64];
		int i;

		Q_strncpyz( shown, RP_ShownText( ent->soundSet ), sizeof( shown ) );
		for ( i = 0; shown[i]; i++ )
		{
			if ( shown[i] == '"' || shown[i] == '\n' || shown[i] == '\r' || shown[i] == ';' )
				shown[i] = '?';
		}
		return RP_SpawnRefuse( ent, ( level.rp_soundsets_state > 0 ) ?
			va( "soundSet %s is not one of the server's ambient sound sets (sound/sound.txt)", shown ) :
			va( "soundSet %s is not one the map already uses (the server cannot read sound/sound.txt to check it)", shown ) );
	}

	// DAJ_RP: [Entity System] a brush model ("*N", "#name") the map does not have -- see RP_BrushModelProblem().
	// Refused whatever the class, a trigger included: it used to spawn with no model at all (a door invisible,
	// a trigger only its mins/maxs box) and be written back by /entsave just the same, with nothing said but a
	// line in the server log.
	{
		const char *problem = RP_BrushModelProblem( ent->model );

		if ( problem )
			return RP_SpawnRefuse( ent, problem );
	}

	// DAJ_RP: [Weather] an effect key holding a weather command. Whatever these keys name is registered as an
	// effect, and a name starting with * is not one: every player's game runs it as a weather command, the
	// same as zyk_weather's message, with none of its checks -- "*clear" in a func_breakable's playfx wiped
	// the map's weather the moment it spawned.
	{
		static const struct { const char *classname; const char *key; } weatherKeys[] = {
			{ "func_breakable",				"playfx" },
			{ "misc_siege_item",			"deathfx" },
			{ "misc_siege_item",			"respawnfx" },
			{ "trigger_lightningstrike",	"lightningfx" }
		};
		int k;

		for ( k = 0; k < (int)ARRAY_LEN( weatherKeys ); k++ )
		{
			char *value = NULL;

			if ( Q_stricmp( ent->classname, weatherKeys[k].classname ) != 0 )
				continue;
			if ( G_SpawnString( weatherKeys[k].key, "", &value ) && value && value[0] == '*' )
				return RP_SpawnRefuse( ent, va( "%s names a weather command (it starts with *), not an effect; weather is added with zyk_weather",
					weatherKeys[k].key ) );
		}
	}

	// DAJ_RP: [SP Maps] a misc_model_ghoul: its .glm model on the server, frames and radius in range
	// (RP_GhoulSpawnProblem() in g_misc.c), and a slot for the model and for the skin entry it will
	// register -- a "skin" whose file is not there is not refused: the default skin is shown instead
	if ( Q_stricmp( ent->classname, "misc_model_ghoul" ) == 0 )
	{
		char path[MAX_QPATH], skinEntry[MAX_QPATH], reason[256];
		const char *names[2];
		const char *problem;
		int count = 0;

		problem = RP_GhoulSpawnProblem( ent, path, sizeof( path ), skinEntry, sizeof( skinEntry ) );
		if ( problem )
			return RP_SpawnRefuse( ent, problem );

		names[count++] = path;
		if ( skinEntry[0] )
			names[count++] = skinEntry;
		if ( !RP_SlotRoomFor( CS_MODELS, names, count, 0, reason, sizeof( reason ) ) )
			return RP_SpawnRefuse( ent, reason );

		return qfalse;
	}

	breakable = ( Q_stricmp( ent->classname, "misc_model_breakable" ) == 0 ) ? qtrue : qfalse;
	md3Mover = ( Q_stricmpn( ent->classname, "func_", 5 ) == 0 && ent->model && ent->model[0] &&
		ent->model[0] != '*' && ent->model[0] != '#' ) ? qtrue : qfalse;
	keyClass = RP_ModelKeyClass( ent->classname );

	if ( breakable || md3Mover || ( keyClass && !keyClass->keyUnused && ent->model && ent->model[0] ) )
	{
		const char *model = ent->model;
		char candidate[MAX_QPATH];
		int where;

		if ( !model || !model[0] )
			return RP_SpawnRefuse( ent, "it has no model" );

		// DAJ_RP: [Dispensers] the lookup, now shared with RP_DispenserModel() -- unchanged
		where = RP_FindModelFile( model, candidate, sizeof( candidate ) );
		if ( !where )
			return RP_SpawnRefuse( ent, va( "model %s is not on the server", model ) );
		if ( where == 2 )
			ent->model = G_NewStringRaw( candidate );

		if ( ( breakable || keyClass ) && !RP_IsMd3Name( ent->model ) )	// keyClass: one whose key is used
			return RP_SpawnRefuse( ent, va( "model %s is not an .md3 model", ent->model ) );
	}

	// DAJ_RP: [Dispensers] the one model a dispenser, rack or cargo crate registers -- its key's, checked
	// above, or its own (always, for a rack) -- and the cargo crate, a breakable that can always be damaged,
	// its sound, have to get a slot, as a misc_model_breakable's below. See RP_ModelKeyClass().
	if ( keyClass )
	{
		const char *names[1];
		const char *sounds[RP_MODEL_KEY_SOUNDS + 1];
		char reason[256];
		int otherBytes = 0, soundCount = 0, i;

		names[0] = ( !keyClass->keyUnused && ent->model && ent->model[0] ) ? ent->model : keyClass->defaultModel;

		// GalaxyRP: [Slot Reuse] its sounds (see RP_ModelKeyClass()): the model goes in first, so the bytes
		// of those not registered yet are counted with it, and then they need slots of their own
		for ( i = 0; i < RP_MODEL_KEY_SOUNDS && keyClass->sounds[i]; i++ )
			sounds[soundCount++] = keyClass->sounds[i];
		if ( keyClass->breakable )
			sounds[soundCount++] = RP_BREAKABLE_SOUND;
		for ( i = 0; i < soundCount; i++ )
		{
			if ( !RP_SoundRegistered( sounds[i] ) )
				otherBytes += (int)strlen( sounds[i] ) + 1;
		}

		if ( !RP_SlotRoomFor( CS_MODELS, names, 1, otherBytes, reason, sizeof( reason ) ) )
			return RP_SpawnRefuse( ent, reason );
		if ( soundCount && !RP_SlotRoomFor( CS_SOUNDS, sounds, soundCount, 0, reason, sizeof( reason ) ) )
			return RP_SpawnRefuse( ent, reason );

		return qfalse;
	}

	// GalaxyRP: [Slot Reuse] every model name the spawn function is about to register has to get a
	// slot -- already there, free, or one a removed prop no longer needs -- or the prop would be drawn
	// as nothing. The names and their order are the spawn functions' own: a misc_model_breakable's
	// model, then its damage model (it can be damaged, the flag 8 is off and the file exists -- see
	// SP_misc_model_breakable), then its use model (flag 32 and the file); a func_'s md3 model
	// (zyk_set_brush_model), then its model2 (InitMover skips a .glm one and InitBBrush does not, so it is
	// always counted). A breakable that can be damaged also registers its explosion sound between its
	// model and its damage model (misc_model_breakable_init), which spends gamestate bytes too. func_group
	// and func_timer register neither key.
	if ( breakable || ( RP_FuncRegistersModels( ent->classname ) && ( md3Mover || ( ent->model2 && ent->model2[0] ) ) ) )
	{
		const char *names[3];
		char damageModel[MAX_QPATH], useModel[MAX_QPATH], reason[256];
		int count = 0, otherBytes = 0;

		if ( breakable || md3Mover )
			names[count++] = ent->model;

		if ( breakable )
		{
			int len = (int)strlen( ent->model ) - 4;

			if ( ent->health && !RP_SoundRegistered( RP_BREAKABLE_SOUND ) )
				otherBytes = (int)strlen( RP_BREAKABLE_SOUND ) + 1;

			if ( len > 0 && len + 8 <= MAX_QPATH )
			{
				if ( ent->health && !( ent->spawnflags & 8 ) )
				{
					Q_strncpyz( damageModel, ent->model, len + 1 );
					Q_strcat( damageModel, sizeof( damageModel ), "_d1.md3" );
					if ( RP_FileExists( damageModel ) )
						names[count++] = damageModel;
				}
				if ( ent->spawnflags & 32 )
				{
					Q_strncpyz( useModel, ent->model, len + 1 );
					Q_strcat( useModel, sizeof( useModel ), "_u1.md3" );
					if ( RP_FileExists( useModel ) )
						names[count++] = useModel;
				}
			}
		}
		else if ( ent->model2 && ent->model2[0] )
		{
			names[count++] = ent->model2;
		}

		if ( !RP_SlotRoomFor( CS_MODELS, names, count, otherBytes, reason, sizeof( reason ) ) )
			return RP_SpawnRefuse( ent, reason );

		return qfalse;
	}

	if ( Q_stricmp( ent->classname, "fx_runner" ) == 0 )
	{
		char *fxFile = NULL;
		char path[MAX_QPATH * 2];

		G_SpawnString( "fxFile", "", &fxFile );
		if ( !fxFile || !fxFile[0] )
			return qfalse;

		// DAJ_RP: [Weather] a weather command, not a file: SP_fx_runner() checks it as zyk_weather's message
		// is checked, and finds its slot (RP_WeatherAdd, g_misc.c). It used to be refused here as "effect
		// *rain is not on the server".
		if ( fxFile[0] == '*' )
			return qfalse;

		// zyk: the path the effect system itself opens (CFxScheduler::RegisterEffect): ".efx" added
		// when the name has no '.' at all, "effects/" put in front unless it already starts with it
		Q_strncpyz( path, fxFile, sizeof( path ) );
		if ( !strchr( path, '.' ) )
			Q_strcat( path, sizeof( path ), ".efx" );
		if ( Q_strncmp( fxFile, "effects", 7 ) != 0 )
		{
			char tmp[MAX_QPATH * 2];

			Q_strncpyz( tmp, path, sizeof( tmp ) );
			Com_sprintf( path, sizeof( path ), "effects/%s", tmp );
		}

		if ( !RP_FileExists( path ) )
			return RP_SpawnRefuse( ent, va( "effect %s is not on the server", fxFile ) );

		// GalaxyRP: [Slot Reuse] and a slot for it (see above)
		{
			const char *names[1];
			char reason[256];

			names[0] = fxFile;
			if ( !RP_SlotRoomFor( CS_EFFECTS, names, 1, 0, reason, sizeof( reason ) ) )
				return RP_SpawnRefuse( ent, reason );
		}
	}

	return qfalse;
}

/*
=================
RP_EntitySystemSpawnedEmpty

GalaxyRP: [Slot Reuse] after the spawn function: the Entity System prop whose model or effect still got
no slot -- something registered in between took the room RP_EntitySystemSpawnRefused() had found --
names what it is missing, so it is refused rather than left in the world drawn as nothing.
=================
*/
static const char *RP_EntitySystemSpawnedEmpty( gentity_t *ent )
{
	char *fxFile = NULL;

	if ( !ent->inuse || !ent->classname || !RP_EntitySystemMade( ent ) || ent->s.modelindex != 0 )
		return NULL;

	if ( Q_stricmp( ent->classname, "misc_model_breakable" ) == 0 )
		return "model";

	// DAJ_RP: [SP Maps] (its skin entry is not needed: without one it shows its default skin)
	if ( Q_stricmp( ent->classname, "misc_model_ghoul" ) == 0 )
		return "model";

	// GalaxyRP: [Slot Reuse] a dispenser, rack, cargo crate or training dummy (RP_ModelKeyClass()) -- each sets
	// its model in its spawn function, the ammo rack too since it registers it there (SP_misc_model_ammo_rack())
	if ( RP_ModelKeyClass( ent->classname ) )
		return "model";

	if ( RP_FuncRegistersModels( ent->classname ) && ent->model && ent->model[0] && ent->model[0] != '*' && ent->model[0] != '#' )
		return "model";

	if ( Q_stricmp( ent->classname, "fx_runner" ) == 0 && G_SpawnString( "fxFile", "", &fxFile ) && fxFile && fxFile[0] )
		return "effect";

	return NULL;
}

// zyk: function to spawn entities used by entity system
void zyk_main_spawn_entity(gentity_t *ent) {
	int			i = 0;
	int j = 0;
	char		*s, *value, *gametypeName;
	static char *gametypeNames[] = { "ffa", "holocron", "jedimaster", "duel", "powerduel", "single", "team", "siege", "ctf", "cty" };

	// GalaxyRP fix: [Entity System] the same guard zyk_main_set_entity_field() carries, and for the
	// same reason: the row this reads, and everything G_CallSpawn() goes on to link and register, is
	// keyed off ent->s.number, which a freed entity no longer has. Spawning into a slot that is not
	// in use put a live, linked entity in the world wearing entity 0's number and left the slot
	// looking free to G_Spawn().
	if (!ent || !ent->inuse)
	{
		return;
	}

	// GalaxyRP: [Entity System] no reason left over from an earlier spawn -- see RP_EntitySystemSpawnRefused()
	level.rp_spawn_refusal[0] = '\0';
	level.rp_spawn_note[0] = '\0';

	// GalaxyRP fix: [Entity System] i was bounded only by the key count, which nothing bounded, so a
	// row that had been overrun carried straight on into level.spawnVars[MAX_SPAWN_VARS]. The row can
	// no longer exceed MAX_SPAWN_VARS pairs (see zyk_main_set_entity_field), and this stops at that
	// same limit regardless, so neither array can be written past its end even if the count is stale.
	while (j < level.zyk_spawn_strings_values_count[ent->s.number] && i < MAX_SPAWN_VARS)
	{
		// GalaxyRP fix: [Entity System] the two G_NewString() calls that used to be here allocated a
		// fresh copy of every key and value on every spawn and every /entedit, and level.spawnVars is
		// only read by the G_Spawn* helpers during the G_CallSpawn() below -- the row it points at
		// outlives that by the whole life of the entity, so pointing straight at it is safe.
		//
		// It also removes a real inconsistency. G_ParseField on the line above is handed the stored
		// string, while G_SpawnString() used to hand the SP_ function a G_NewString() of it, i.e. the
		// same value translated one more time. For anything set through /entadd that made no
		// difference -- G_NewString never emits a backslash followed by 'n', because it consumes that
		// pair, so it is idempotent on its own output -- but the entity-file loader now stores its
		// tokens untranslated on purpose, and a value from a preset that really contains a backslash
		// and an 'n' would have been read two different ways by the two halves of this function.
		G_ParseField(level.zyk_spawn_strings[ent->s.number][j], level.zyk_spawn_strings[ent->s.number][j + 1], ent);
		level.spawnVars[i][0] = level.zyk_spawn_strings[ent->s.number][j];
		level.spawnVars[i][1] = level.zyk_spawn_strings[ent->s.number][j + 1];

		i++;
		j += 2;
	}

	level.numSpawnVars = i;

	// check for "notsingle" flag
	if (level.gametype == GT_SINGLE_PLAYER) {
		G_SpawnInt("notsingle", "0", &i);
		if (i) {
			ADJUST_AREAPORTAL();
			G_FreeEntity(ent);
			return;
		}
	}
	// check for "notteam" flag (GT_FFA, GT_DUEL, GT_SINGLE_PLAYER)
	if (level.gametype >= GT_TEAM) {
		G_SpawnInt("notteam", "0", &i);
		if (i) {
			ADJUST_AREAPORTAL();
			G_FreeEntity(ent);
			return;
		}
	}
	else {
		G_SpawnInt("notfree", "0", &i);
		if (i) {
			ADJUST_AREAPORTAL();
			G_FreeEntity(ent);
			return;
		}
	}

	if (G_SpawnString("gametype", NULL, &value)) {
		if (level.gametype >= GT_FFA && level.gametype < GT_MAX_GAME_TYPE) {
			gametypeName = gametypeNames[level.gametype];

			s = strstr(value, gametypeName);
			if (!s) {
				ADJUST_AREAPORTAL();
				G_FreeEntity(ent);
				return;
			}
		}
	}

	// GalaxyRP: [Slot Reuse] an Entity System entity spawned again in place (/entedit, /entrotate,
	// a dropped /entcut) keeps its struct: without this, the indices of the model or effect it is
	// about to replace still counted as in use, so an edit with the table full was refused -- and the
	// entity lost -- although its own old slot was free for the new name. Its spawn function sets
	// them again; a breakable's use-model toggle keeps its two in sound1to2 / sound2to1, and whether
	// it can be damaged follows its health again rather than what it was before the edit.
	if (RP_EntitySystemMade(ent)) {
		ent->s.modelindex = 0;
		ent->s.modelindex2 = 0;
		// DAJ_RP fix: [Entity System] and no brush model either: its spawn function sets one again
		// (trap->SetBrushModel), and one that sets none -- the model key removed, or a class that has
		// no brush model -- kept r.bmodel with modelindex 0 here, which the server sends as the whole
		// map's world model: every client's prediction collided with an invisible copy of the map,
		// moved by the entity's origin. See zyk_set_brush_model().
		ent->r.bmodel = qfalse;
		if (ent->classname && Q_stricmp(ent->classname, "misc_model_breakable") == 0) {
			ent->sound1to2 = 0;
			ent->sound2to1 = 0;
			ent->takedamage = qfalse;
		}
	}

	// GalaxyRP: [Entity System] a model or effect file the server does not have is refused here,
	// before anything registers its name -- see RP_EntitySystemSpawnRefused()
	if (RP_EntitySystemSpawnRefused(ent)) {
		G_FreeEntity(ent);
		return;
	}

	// move editor origin to pos
	VectorCopy(ent->s.origin, ent->s.pos.trBase);
	VectorCopy(ent->s.origin, ent->r.currentOrigin);

	// if we didn't get a classname, don't bother spawning anything
	if (!G_CallSpawn(ent)) {
		G_FreeEntity(ent);
	}
	else
	{
		const char *missing = RP_EntitySystemSpawnedEmpty(ent);

		if (missing)
		{
			Q_strncpyz(level.rp_spawn_refusal, va("the map has no room for another %s", missing), sizeof(level.rp_spawn_refusal));
			G_LogPrintf("Entity %d (%s) refused: %s\n", ent->s.number, ent->classname, level.rp_spawn_refusal);
			RP_FreeEntityTriggers(ent);
			G_FreeEntity(ent);
			return;
		}
	}

	// GalaxyRP: [Entity System] into the team its "team" key names, as G_FindTeams() would have linked it
	// at map load -- see RP_TeamLinkEntity() in g_main.c
	if ( ent->inuse )
	{
		// DAJ_RP: [Locks] a lock key on a class that cannot carry one: say it does nothing (g_locks.c)
		RP_LockSpawnNote( ent );

		RP_TeamLinkEntity( ent );

		// GalaxyRP fix: [Entity System] a leader spawned again in place answers its followers' name again
		RP_TeamLeaderTakeName( ent );

		// GalaxyRP fix: [SP Maps] and a rotator, bobber or pendulum of a team goes as its master goes
		// (RP_ToggleMoverTeamMatch() in g_mover.c): one joining a team, or the team of a master spawned
		// again in place
		if ( ent->teammaster && ent->teammaster->inuse )
			RP_ToggleMoverTeamMatch( ent->teammaster );
	}

	// GalaxyRP: [Logical Entities] never for a logical entity: ICARUS keeps its per-entity state in
	// the engine, indexed by entity number, and the engine does not know this number exists.
	//Tag on the ICARUS scripting information only to valid recipients
	if ( !ent->isLogical && trap->ICARUS_ValidEnt((sharedEntity_t *)ent))
	{
		trap->ICARUS_InitEnt((sharedEntity_t *)ent);

		if (ent->classname && ent->classname[0])
		{
			if (Q_strncmp("NPC_", ent->classname, 4) != 0)
			{//Not an NPC_spawner (rww - probably don't even care for MP, but whatever)
				G_ActivateBehavior(ent, BSET_SPAWN);
			}
		}
	}
}

/*
====================
G_AddSpawnVarToken
====================
*/
char *G_AddSpawnVarToken( const char *string ) {
	int		l;
	char	*dest;

	l = strlen( string );
	if ( level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS ) {
		trap->Error( ERR_DROP, "G_AddSpawnVarToken: MAX_SPAWN_VARS_CHARS" );
	}

	dest = level.spawnVarChars + level.numSpawnVarChars;
	memcpy( dest, string, l+1 );

	level.numSpawnVarChars += l + 1;

	return dest;
}

void AddSpawnField(char *field, char *value)
{
	int	i;

	for(i=0;i<level.numSpawnVars;i++)
	{
		if (Q_stricmp(level.spawnVars[i][0], field) == 0)
		{
			level.spawnVars[ i ][1] = G_AddSpawnVarToken( value );
			return;
		}
	}

	level.spawnVars[ level.numSpawnVars ][0] = G_AddSpawnVarToken( field );
	level.spawnVars[ level.numSpawnVars ][1] = G_AddSpawnVarToken( value );
	level.numSpawnVars++;
}

#define NOVALUE "novalue"

static void HandleEntityAdjustment(void)
{
	char		*value;
	vec3_t		origin, newOrigin, angles;
	char		temp[MAX_QPATH];
	float		rotation;

	G_SpawnString("origin", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		if ( sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] ) != 3 ) {
			trap->Print( "HandleEntityAdjustment: failed sscanf on 'origin' (%s)\n", value );
			VectorClear( origin );
		}
	}
	else
	{
		origin[0] = origin[1] = origin[2] = 0.0;
	}

	rotation = DEG2RAD(level.mRotationAdjust);
	newOrigin[0] = origin[0]*cos(rotation) - origin[1]*sin(rotation);
	newOrigin[1] = origin[0]*sin(rotation) + origin[1]*cos(rotation);
	newOrigin[2] = origin[2];
	VectorAdd(newOrigin, level.mOriginAdjust, newOrigin);
	// damn VMs don't handle outputing a float that is compatible with sscanf in all cases
	Com_sprintf(temp, sizeof( temp ), "%0.0f %0.0f %0.0f", newOrigin[0], newOrigin[1], newOrigin[2]);
	AddSpawnField("origin", temp);

	G_SpawnString("angles", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		if ( sscanf( value, "%f %f %f", &angles[0], &angles[1], &angles[2] ) != 3 ) {
			trap->Print( "HandleEntityAdjustment: failed sscanf on 'angles' (%s)\n", value );
			VectorClear( angles );
		}

		angles[YAW] = fmod(angles[YAW] + level.mRotationAdjust, 360.0f);
		// damn VMs don't handle outputing a float that is compatible with sscanf in all cases
		Com_sprintf(temp, sizeof( temp ), "%0.0f %0.0f %0.0f", angles[0], angles[1], angles[2]);
		AddSpawnField("angles", temp);
	}
	else
	{
		G_SpawnString("angle", NOVALUE, &value);
		if (Q_stricmp(value, NOVALUE) != 0)
		{
			angles[YAW] = atof( value );
		}
		else
		{
			angles[YAW] = 0.0;
		}
		angles[YAW] = fmod(angles[YAW] + level.mRotationAdjust, 360.0f);
		Com_sprintf(temp, sizeof( temp ), "%0.0f", angles[YAW]);
		AddSpawnField("angle", temp);
	}

	// RJR experimental code for handling "direction" field of breakable brushes
	// though direction is rarely ever used.
	G_SpawnString("direction", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		if ( sscanf( value, "%f %f %f", &angles[0], &angles[1], &angles[2] ) != 3 ) {
			trap->Print( "HandleEntityAdjustment: failed sscanf on 'direction' (%s)\n", value );
			VectorClear( angles );
		}
	}
	else
	{
		angles[0] = angles[1] = angles[2] = 0.0;
	}
	angles[YAW] = fmod(angles[YAW] + level.mRotationAdjust, 360.0f);
	Com_sprintf(temp, sizeof( temp ), "%0.0f %0.0f %0.0f", angles[0], angles[1], angles[2]);
	AddSpawnField("direction", temp);


	AddSpawnField("BSPInstanceID", level.mTargetAdjust);

	G_SpawnString("targetname", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("targetname", temp);
	}

	G_SpawnString("target", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("target", temp);
	}

	G_SpawnString("killtarget", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("killtarget", temp);
	}

	G_SpawnString("brushparent", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("brushparent", temp);
	}

	G_SpawnString("brushchild", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("brushchild", temp);
	}

	G_SpawnString("enemy", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("enemy", temp);
	}

	G_SpawnString("ICARUSname", NOVALUE, &value);
	if (Q_stricmp(value, NOVALUE) != 0)
	{
		Com_sprintf(temp, sizeof( temp ), "%s%s", level.mTargetAdjust, value);
		AddSpawnField("ICARUSname", temp);
	}
}

/*
====================
G_ParseSpawnVars

Parses a brace bounded set of key / value pairs out of the
level's entity strings into level.spawnVars[]

This does not actually spawn an entity.
====================
*/
qboolean G_ParseSpawnVars( qboolean inSubBSP ) {
	char		keyname[MAX_TOKEN_CHARS];
	char		com_token[MAX_TOKEN_CHARS];

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;

	// parse the opening brace
	if ( !trap->GetEntityToken( com_token, sizeof( com_token ) ) ) {
		// end of spawn string
		return qfalse;
	}
	if ( com_token[0] != '{' ) {
		trap->Error( ERR_DROP, "G_ParseSpawnVars: found %s when expecting {",com_token );
	}

	// go through all the key / value pairs
	while ( 1 ) {
		// parse key
		if ( !trap->GetEntityToken( keyname, sizeof( keyname ) ) ) {
			trap->Error( ERR_DROP, "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( keyname[0] == '}' ) {
			break;
		}

		// parse value
		if ( !trap->GetEntityToken( com_token, sizeof( com_token ) ) ) {
			trap->Error( ERR_DROP, "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( com_token[0] == '}' ) {
			trap->Error( ERR_DROP, "G_ParseSpawnVars: closing brace without data" );
		}
		if ( level.numSpawnVars == MAX_SPAWN_VARS ) {
			trap->Error( ERR_DROP, "G_ParseSpawnVars: MAX_SPAWN_VARS" );
		}
		level.spawnVars[ level.numSpawnVars ][0] = G_AddSpawnVarToken( keyname );
		level.spawnVars[ level.numSpawnVars ][1] = G_AddSpawnVarToken( com_token );
		level.numSpawnVars++;
	}

	if (inSubBSP)
	{
		HandleEntityAdjustment();
	}

	return qtrue;
}


static	char *defaultStyles[32][3] =
{
	{	// 0 normal
		"z",
		"z",
		"z"
	},
	{	// 1 FLICKER (first variety)
		"mmnmmommommnonmmonqnmmo",
		"mmnmmommommnonmmonqnmmo",
		"mmnmmommommnonmmonqnmmo"
	},
	{	// 2 SLOW STRONG PULSE
		"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcb",
		"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcb",
		"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcb"
	},
	{	// 3 CANDLE (first variety)
		"mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
		"mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
		"mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
	},
	{	// 4 FAST STROBE
		"mamamamamama",
		"mamamamamama",
		"mamamamamama"
	},
	{	// 5 GENTLE PULSE 1
		"jklmnopqrstuvwxyzyxwvutsrqponmlkj",
		"jklmnopqrstuvwxyzyxwvutsrqponmlkj",
		"jklmnopqrstuvwxyzyxwvutsrqponmlkj"
	},
	{	// 6 FLICKER (second variety)
		"nmonqnmomnmomomno",
		"nmonqnmomnmomomno",
		"nmonqnmomnmomomno"
	},
	{	// 7 CANDLE (second variety)
		"mmmaaaabcdefgmmmmaaaammmaamm",
		"mmmaaaabcdefgmmmmaaaammmaamm",
		"mmmaaaabcdefgmmmmaaaammmaamm"
	},
	{	// 8 CANDLE (third variety)
		"mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
		"mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
		"mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
	},
	{	// 9 SLOW STROBE (fourth variety)
		"aaaaaaaazzzzzzzz",
		"aaaaaaaazzzzzzzz",
		"aaaaaaaazzzzzzzz"
	},
	{	// 10 FLUORESCENT FLICKER
		"mmamammmmammamamaaamammma",
		"mmamammmmammamamaaamammma",
		"mmamammmmammamamaaamammma"
	},
	{	// 11 SLOW PULSE NOT FADE TO BLACK
		"abcdefghijklmnopqrrqponmlkjihgfedcba",
		"abcdefghijklmnopqrrqponmlkjihgfedcba",
		"abcdefghijklmnopqrrqponmlkjihgfedcba"
	},
	{	// 12 FAST PULSE FOR JEREMY
		"mkigegik",
		"mkigegik",
		"mkigegik"
	},
	{	// 13 Test Blending
		"abcdefghijklmqrstuvwxyz",
		"zyxwvutsrqmlkjihgfedcba",
		"aammbbzzccllcckkffyyggp"
	},
	{	// 14
		"",
		"",
		""
	},
	{	// 15
		"",
		"",
		""
	},
	{	// 16
		"",
		"",
		""
	},
	{	// 17
		"",
		"",
		""
	},
	{	// 18
		"",
		"",
		""
	},
	{	// 19
		"",
		"",
		""
	},
	{	// 20
		"",
		"",
		""
	},
	{	// 21
		"",
		"",
		""
	},
	{	// 22
		"",
		"",
		""
	},
	{	// 23
		"",
		"",
		""
	},
	{	// 24
		"",
		"",
		""
	},
	{	// 25
		"",
		"",
		""
	},
	{	// 26
		"",
		"",
		""
	},
	{	// 27
		"",
		"",
		""
	},
	{	// 28
		"",
		"",
		""
	},
	{	// 29
		"",
		"",
		""
	},
	{	// 30
		"",
		"",
		""
	},
	{	// 31
		"",
		"",
		""
	}
};

void *precachedKyle = 0;
void scriptrunner_run (gentity_t *self);

/*QUAKED worldspawn (0 0 0) ?

Every map should have exactly one worldspawn.
"music"		music wav file
"gravity"	800 is default gravity
"message"	Text to print during connection process

BSP Options
"gridsize"     size of lighting grid to "X Y Z". default="64 64 128"
"ambient"      scale of global light (from _color)
"fog"          shader name of the global fog texture - must include the full path, such as "textures/rj/fog1"
"distancecull" value for vis for the maximum viewing distance
"chopsize"     value for bsp on the maximum polygon / portal size
"ls_Xr"	override lightstyle X with this pattern for Red.
"ls_Xg"	green (valid patterns are "a-z")
"ls_Xb"	blue (a is OFF, z is ON)

"fogstart"		override fog start distance and force linear
"radarrange" for Siege/Vehicle radar - default range is 2500
*/
extern void EWebPrecache(void); //g_items.c
float g_cullDistance;
void SP_worldspawn( void )
{
	char		*text, temp[32];
	int			i;
	int			lengthRed, lengthBlue, lengthGreen;

	//I want to "cull" entities out of net sends to clients to reduce
	//net traffic on our larger open maps -rww
	G_SpawnFloat("distanceCull", "6000.0", &g_cullDistance);
	trap->SetServerCull(g_cullDistance);

	G_SpawnString( "classname", "", &text );
	if ( Q_stricmp( text, "worldspawn" ) ) {
		trap->Error( ERR_DROP, "SP_worldspawn: The first entity isn't 'worldspawn'" );
	}

	for ( i = 0 ; i < level.numSpawnVars ; i++ )
	{
		if ( Q_stricmp( "spawnscript", level.spawnVars[i][0] ) == 0 )
		{//ONly let them set spawnscript, we don't want them setting an angle or something on the world.
			G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], &g_entities[ENTITYNUM_WORLD] );
		}
	}
	//The server will precache the standard model and animations, so that there is no hit
	//when the first client connnects.
	if (!BGPAFtextLoaded)
	{
		BG_ParseAnimationFile("models/players/_humanoid/animation.cfg", bgHumanoidAnimations, qtrue);
	}

	if (!precachedKyle)
	{
		int defSkin;

		trap->G2API_InitGhoul2Model(&precachedKyle, "models/players/" DEFAULT_MODEL "/model.glm", 0, 0, -20, 0, 0);

		if (precachedKyle)
		{
			defSkin = trap->R_RegisterSkin("models/players/" DEFAULT_MODEL "/model_default.skin");
			trap->G2API_SetSkin(precachedKyle, 0, defSkin, defSkin);
		}
	}

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

	if (level.gametype == GT_SIEGE)
	{ //a tad bit of a hack, but..
		EWebPrecache();
	}

	// make some data visible to connecting client
	trap->SetConfigstring( CS_GAME_VERSION, GAME_VERSION );

	trap->SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ) );

	G_SpawnString( "music", "", &text );
	trap->SetConfigstring( CS_MUSIC, text );

	// zyk: setting default map music
	strcpy(level.default_map_music,text);

	G_SpawnString( "message", "", &text );
	trap->SetConfigstring( CS_MESSAGE, text );				// map specific message

	trap->SetConfigstring( CS_MOTD, g_motd.string );		// message of the day

	G_SpawnString( "gravity", "800", &text );
	trap->Cvar_Set( "g_gravity", text );
	trap->Cvar_Update( &g_gravity );

	G_SpawnString( "enableBreath", "0", &text );

	G_SpawnString( "soundSet", "default", &text );
	trap->SetConfigstring( CS_GLOBAL_AMBIENT_SET, text );

	g_entities[ENTITYNUM_WORLD].s.number = ENTITYNUM_WORLD;
	g_entities[ENTITYNUM_WORLD].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_WORLD].classname = "worldspawn";

	g_entities[ENTITYNUM_NONE].s.number = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].classname = "nothing";

	// see if we want a warmup time
	trap->SetConfigstring( CS_WARMUP, "" );
	if ( g_restarted.integer ) {
		trap->Cvar_Set( "g_restarted", "0" );
		trap->Cvar_Update( &g_restarted );
		level.warmupTime = 0;
	}
	else if ( g_doWarmup.integer && level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL && level.gametype != GT_SIEGE ) { // Turn it on
		level.warmupTime = -1;
		trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
		G_LogPrintf( "Warmup:\n" );
	}

	trap->SetConfigstring(CS_LIGHT_STYLES+(LS_STYLES_START*3)+0, defaultStyles[0][0]);
	trap->SetConfigstring(CS_LIGHT_STYLES+(LS_STYLES_START*3)+1, defaultStyles[0][1]);
	trap->SetConfigstring(CS_LIGHT_STYLES+(LS_STYLES_START*3)+2, defaultStyles[0][2]);

	for(i=1;i<LS_NUM_STYLES;i++)
	{
		Com_sprintf(temp, sizeof(temp), "ls_%dr", i);
		G_SpawnString(temp, defaultStyles[i][0], &text);
		lengthRed = strlen(text);
		trap->SetConfigstring(CS_LIGHT_STYLES+((i+LS_STYLES_START)*3)+0, text);

		Com_sprintf(temp, sizeof(temp), "ls_%dg", i);
		G_SpawnString(temp, defaultStyles[i][1], &text);
		lengthGreen = strlen(text);
		trap->SetConfigstring(CS_LIGHT_STYLES+((i+LS_STYLES_START)*3)+1, text);

		Com_sprintf(temp, sizeof(temp), "ls_%db", i);
		G_SpawnString(temp, defaultStyles[i][2], &text);
		lengthBlue = strlen(text);
		trap->SetConfigstring(CS_LIGHT_STYLES+((i+LS_STYLES_START)*3)+2, text);

		if (lengthRed != lengthGreen || lengthGreen != lengthBlue)
		{
			Com_Error(ERR_DROP, "Style %d has inconsistent lengths: R %d, G %d, B %d",
				i, lengthRed, lengthGreen, lengthBlue);
		}
	}
}

//rww - Planning on having something here?
qboolean SP_bsp_worldspawn ( void )
{
	return qtrue;
}

void G_PrecacheSoundsets( void )
{
	gentity_t	*ent = NULL;
	int i;
	int countedSets = 0;

	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->soundSet && ent->soundSet[0])
		{
			if (countedSets >= MAX_AMBIENT_SETS)
			{
				Com_Error(ERR_DROP, "MAX_AMBIENT_SETS was exceeded! (too many soundsets)\n");
			}

			ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
			countedSets++;
		}
	}
}

void G_LinkLocations( void ) {
	int i, n;

	if ( level.locations.linked )
		return;

	level.locations.linked = qtrue;

	trap->SetConfigstring( CS_LOCATIONS, "unknown" );

	for ( i=0, n=1; i<level.locations.num; i++ ) {
		level.locations.data[i].cs_index = n;
		trap->SetConfigstring( CS_LOCATIONS + n, level.locations.data[i].message );
		n++;
	}
	// All linked together now
}

// GalaxyRP fix: [Logical Entities] trigger_shipboundary hands its target's ENTITY NUMBER to the
// engine -- trap->LinkEntity() on the target, then ps.vehTurnaroundIndex = target->s.number so the
// client's PM_VehForcedTurning() can look it up in cg_entities[MAX_GENTITIES]. Both of those are
// networked-region operations: SV_SvEntityForGentity() Com_Error(ERR_DROP)s on any s.number >=
// MAX_GENTITIES, and on a dedicated server common.cpp promotes every ERR_DROP to ERR_FATAL, so the
// process exits the instant a fighter reaches a map edge.
//
// The targets these triggers point at are markers -- info_notnull, info_null, target_position --
// and all three are marked logical in spawns[] above, which is right for every other use of them:
// a marker is found by name with G_Find() and read for its origin, and nothing hands it to the
// engine. trigger_shipboundary is the one class that does, and it is the trigger, not the marker,
// that knows it.
//
// So resolve that here, at the end of the spawn pass: for every trigger_shipboundary, find the
// entity its "target" names and, if it landed in the logical region, re-spawn it as a networked
// one. At map load this runs while level.spawning is still qtrue, which matters twice -- G_Spawn()
// is still legal, and RP_LegacySlotAssign()/RP_LegacySlotRelease() still track slots, so the
// marker keeps the legacy number it would have had with rp_logical_entities 0.
//
// It runs a SECOND time, from G_RunFrame's Entity System load block (g_main.c), because /entload
// and the automatic <map>/default.txt load both free every entity in both regions and respawn
// from the file -- which puts the markers straight back in the logical region with nothing to
// promote them. There level.spawning is qfalse, and that is fine: G_Spawn() carries no such guard
// (the only readers of the flag are G_SpawnString, zyk_brush_model_allowed and the two legacy-slot
// helpers, none of which this pass calls), and the legacy-slot swap below simply becomes a no-op
// on zeros, which is correct -- legacySlot only describes the map's own spawn pass, which finished
// long before (see gentity_t::legacySlot). That call site sits above the load block's G_FindTeams(), mirroring
// the order here: G_FindTeams builds teammaster/teamchain POINTER chains and moves a slave's
// targetname onto its master, so a free-and-reallocate has to happen before it, not after.
//
// It has to be a post-pass rather than a decision at spawn time because the entity string may
// name the target before it defines it: on the map this was found with, 4 of the 18 boundaries
// have their marker later in the string than themselves.
//
// Only inert markers are promoted. Anything with a think, use, touch or die function, a model or
// an existing link is not the point-in-space this trigger expects, and quietly moving it between
// regions could change behaviour that has nothing to do with ship boundaries -- those are reported
// and left alone, and shipboundary_touch's own guard catches them at the edge.
//
// Cost is one networked slot per distinct marker (5 on that map), taken from the 1022 the engine
// allows. The de-duplication is free: once a marker is promoted, G_Find() -- which walks the
// networked region first -- returns the networked copy for every later boundary naming it, and
// the isLogical test below skips it.
void RP_PromoteShipboundaryTargets( void ) {
	gentity_t	*bound;
	int			promoted = 0;

	if ( !level.logical_entities_enabled ) {
		return;	// nothing is in the logical region to begin with
	}

	RP_FOR_EACH_ENTITY( bound ) {
		gentity_t	*src, *dst;
		int			number, spare;
		int			i, keys;

		if ( !bound->inuse || !bound->classname ||
			Q_stricmp( bound->classname, "trigger_shipboundary" ) ) {
			continue;
		}

		if ( !bound->target || !bound->target[0] ) {
			continue;	// SP_trigger_shipboundary already refuses these
		}

		src = G_Find( NULL, FOFS(targetname), bound->target );
		if ( !src || !src->inuse ) {
			continue;	// missing target; shipboundary_touch reports it if it is ever hit
		}

		if ( !src->isLogical ) {
			continue;	// already networked, or promoted by an earlier boundary naming it
		}

		if ( src->think || src->use || src->touch || src->die || src->model || src->r.linked ) {
			G_LogPrintf( "trigger_shipboundary target '%s' (classname %s) is not an inert marker; "
				"leaving it in the logical region.\n",
				bound->target, src->classname ? src->classname : "?" );
			continue;
		}

		// G_Spawn() ends the server process on a dedicated build when the table is exhausted, and
		// this pass also runs after a preset load, where /entload puts it within reach of anything
		// an admin can type. Refusing costs the shipboundary_touch fallback -- the ship is
		// destroyed at the edge and the refusal is in the log -- which is survivable; not refusing
		// is not. Same margin discipline as /entadd (4) and /spawnplatform (2).
		if ( !G_EntitySlotsAvailable( 1 ) ) {
			G_LogPrintf( "trigger_shipboundary target '%s' not promoted: %d entity slots free.\n",
				bound->target, G_FreeEntityCount() );
			continue;
		}

		dst = G_Spawn();
		if ( !dst ) {
			continue;	// G_Spawn logs and drops on its own if it truly cannot allocate
		}

		// The marker's legacy identity moves with it, and the slot G_Spawn just reserved for dst
		// goes back on the free below -- swap them so RP_LegacySlotRelease() releases the spare
		// rather than the number dst is about to keep.
		spare = dst->legacySlot;

		number = dst->s.number;
		dst->s = src->s;
		dst->s.number = number;

		dst->classname		= src->classname;
		dst->targetname		= src->targetname;
		dst->target			= src->target;
		dst->target2		= src->target2;
		dst->spawnflags		= src->spawnflags;
		dst->legacySlot		= src->legacySlot;
		dst->rpSubBSPOf		= src->rpSubBSPOf;	// GalaxyRP: a sub-BSP marker stays its misc_bsp's
		dst->rpMapEntity	= src->rpMapEntity;	// GalaxyRP: and a map marker the map's (g_entgrab.c)
		dst->isLogical		= qfalse;

		// exactly what SP_info_notnull/SP_target_position did for it in the logical region:
		// s.pos.trBase, TR_STATIONARY, r.currentOrigin. Still no link -- shipboundary_touch
		// links the target itself on first touch, which is the stock behaviour.
		G_SetOrigin( dst, dst->s.origin );

		// GalaxyRP fix: [Entity System] the Entity System's key/value record lives in
		// level.zyk_spawn_strings[], indexed by s.number, so it does not follow the entity to its
		// new slot: G_Spawn() above zeroed the count at dst's index and G_FreeEntity() below zeroes
		// it at src's. Left alone, the promoted marker ends up with no record at all -- and
		// /entsave writes no line whatsoever for an entity with no pairs (see Cmd_EntSave_f), so
		// the marker silently disappears from every preset saved after a map load. /entload then
		// brings the boundaries back pointing at a target that no longer exists, which is the one
		// condition shipboundary_touch cannot recover the map from.
		//
		// The slots hold pointers into the map-lifetime string pool that G_NewString() allocates
		// from, so moving the pointers is both correct and free -- the same reasoning
		// zyk_main_set_entity_field() uses when it compacts a row after a key removal. The clamp
		// cannot fire today (the row is MAX_SPAWN_VARS*2 and the writer is bounded by
		// MAX_SPAWN_VARS) and is here so the bound is local to the copy rather than two functions
		// away.
		keys = level.zyk_spawn_strings_values_count[src->s.number];
		if ( keys > ZYK_MAX_SPAWN_STRING_SLOTS ) {
			keys = ZYK_MAX_SPAWN_STRING_SLOTS;
		}
		for ( i = 0; i < keys; i++ ) {
			level.zyk_spawn_strings[dst->s.number][i] = level.zyk_spawn_strings[src->s.number][i];
		}
		level.zyk_spawn_strings_values_count[dst->s.number] = keys;

		src->legacySlot = spare;
		G_FreeEntity( src );

		promoted++;
	}

	if ( promoted ) {
		G_LogPrintf( "promoted %d trigger_shipboundary target%s from the logical region to the "
			"networked one.\n", promoted, promoted == 1 ? "" : "s" );
	}
}

/*
==============
G_SpawnEntitiesFromString

Parses textual entity definitions out of an entstring and spawns gentities.
==============
*/
/*
==============
G_StartWorldSpawnScript

GalaxyRP fix: [Entity System] the world entity cannot run ICARUS scripts itself, so a map whose
worldspawn has a "spawnscript" (e.g. a siege space map whose script triggers every fighter spawner)
gets a helper entity that runs the script 100ms later. That helper was only ever created here, by
the map loader. An entity preset load -- the default.txt loaded at map start, or /entload -- first
frees every non-client entity, the helper included (at map start it is freed before it has even
run), and the helper has no Entity System record, so no preset ever brings it back. The script
therefore never ran on a map with a preset: the fighter spawners came back from the file, named
and waiting to be triggered, and nothing triggered them.

The map loader and the preset loader both call this now. At most one helper is ever alive: the
preset loaders free the old one before they read the file. checkSlots is qfalse for the map
loader, which keeps its original behaviour exactly, and qtrue for the preset loader, which runs
mid-game where G_Spawn() running out of slots would end the process (see ZYK_ENTITY_RESERVE).
Returns qtrue if a helper was started.
==============
*/
qboolean G_StartWorldSpawnScript( qboolean checkSlots )
{
	gentity_t *script_runner;

	if ( !g_entities[ENTITYNUM_WORLD].behaviorSet[BSET_SPAWN] || !g_entities[ENTITYNUM_WORLD].behaviorSet[BSET_SPAWN][0] )
	{
		return qfalse;
	}

	if ( checkSlots && G_EntitySlotsAvailable( 1 ) == qfalse )
	{
		G_LogPrintf( "world spawnscript %s not started: %d entity slots free\n",
			g_entities[ENTITYNUM_WORLD].behaviorSet[BSET_SPAWN], G_FreeEntityCount() );
		return qfalse;
	}

	//World has a spawn script, but we don't want the world in ICARUS and running scripts,
	//so make a scriptrunner and start it going.
	script_runner = G_Spawn();
	if ( !script_runner )
	{
		return qfalse;
	}

	script_runner->behaviorSet[BSET_USE] = g_entities[ENTITYNUM_WORLD].behaviorSet[BSET_SPAWN];
	script_runner->count = 1;
	script_runner->think = scriptrunner_run;
	script_runner->nextthink = level.time + 100;

	if ( script_runner->inuse )
	{
		trap->ICARUS_InitEnt( (sharedEntity_t *)script_runner );
	}

	return qtrue;
}

void G_SpawnEntitiesFromString( qboolean inSubBSP ) {
	// GalaxyRP fix: [Entity System] a misc_bsp spawned after map load (/entadd, /entedit, /entload, the
	// default entity file) rebuilds its sub-BSP's entities through here too. That run must leave the
	// map-load state alone: it does not turn level.spawning on -- nothing turned it off again, which
	// switched off the brush-model guard in zyk_brush_model_allowed() for the rest of the map -- and it
	// skips the world spawnscript, location and soundset passes at the end, which belong to the map's
	// own spawn (the soundset pass even has an ERR_DROP of its own). Map load is unchanged.
	const qboolean runtimeSubBSP = ( inSubBSP && !level.spawning ) ? qtrue : qfalse;

	// GalaxyRP: [Logical Entities] start the legacy-slot simulation with the table as it stands
	// (clients and body queue). A sub-BSP spawns nested inside the main pass and must not restart it.
	if (!inSubBSP)
	{
		RP_LegacySlotsBegin();
	}

	// allow calls to G_Spawn*()
	if (!runtimeSubBSP)
	{
		level.spawning = qtrue;
	}
	level.numSpawnVars = 0;

	// the worldspawn is not an actual entity, but it still
	// has a "spawn" function to perform any global setup
	// needed by a level (setting configstrings or cvars, etc)
	if ( !G_ParseSpawnVars(qfalse) ) {
		trap->Error( ERR_DROP, "SpawnEntities: no entities" );
	}

	if (!inSubBSP)
	{
		SP_worldspawn();
	}
	else
	{
		// Skip this guy if its worldspawn fails
		if ( !SP_bsp_worldspawn() )
		{
			return;
		}
	}

	// parse ents
	while( G_ParseSpawnVars(inSubBSP) ) {
		G_SpawnGEntityFromSpawnVars(inSubBSP);
	}

	if (runtimeSubBSP)
	{
		return;
	}

	// GalaxyRP fix: [Entity System] the world's spawnscript is started by G_StartWorldSpawnScript()
	// now, so an entity preset load can start it again -- see the comment on that function.
	G_StartWorldSpawnScript( qfalse );

	if (!inSubBSP)
	{
		// GalaxyRP fix: [Logical Entities] above the line, while G_Spawn() is still legal.
		RP_PromoteShipboundaryTargets();

		level.spawning = qfalse;			// any future calls to G_Spawn*() will be errors
	}

	G_LinkLocations();

	G_PrecacheSoundsets();
}

