/*
===========================================================================
GLX_U: [Lua] API version 3: players. What a player handle tells about the player (health, where they
stand, their character, their skills...) and the few things a script may do to one: teleport them,
heal them, set their armor, give them an item the server owner allows, give them one of the admins'
player effects, raise or lower a skill with their own skillpoints, and play a sound only they hear.
Also the events the game raises about players and NPCs: player_death, player_downed, npc_death and
damage. NPC handles and World.npc are in rp_lua_world.c, with the other entity handles.

Everything a script does to a player goes through the code the matching admin command uses, with the
same refusals: zyk_TeleportPlayer (/teleport), the pickup functions' rules (BG_CanItemBeGrabbed, Add_Ammo
and its caps), RP_SetPhaseMode (/admghost and the others), do_upgrade_skill / do_downgrade_skill
(/skillup, /skilldown). A teleport is refused rather than telefrag anyone or put the player inside a
wall. Admin Protect shields a protected admin from teleports and effects, as it does from the admins'
commands. Teleports, gifts, effect and skill changes are logged to games.log.

Player effects: the last one wins. pers.phase_by_script remembers which plugin set the current one (0
for an admin, or none). When a plugin is switched off or reloaded, the effects still marked as its own
are cleared (RP_LuaPeople_RemovePlugin); when the state closes, every script's are.

The events run inside game code (player_die, G_Damage): a free() or kill() a handler asks for waits for
the next frame, as inside P2's callbacks.
===========================================================================
*/

#include "rp_lua_local.h"

extern const skill_t skills[];
extern char *modNames[MOD_MAX];
extern void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );
extern qboolean do_upgrade_skill( gentity_t *upgrader, gentity_t *upgradee, int skill_id, qboolean dont_show_message, int number_of_upgrades );
extern qboolean do_downgrade_skill( gentity_t *downgrader, gentity_t *downgradee, int skill_id, int number_of_downgrades );
extern void update_skills_table_row_with_current_values( gentity_t *ent );

#define RP_LUA_UNUSED_SKILL		38		// skill_id 38 (/skillup's number 39) is the shop upgrades' storage

static int RP_LuaPeople_Fail( lua_State *L, const char *reason ) {
	lua_pushboolean( L, 0 );
	lua_pushstring( L, reason );
	return 2;
}

static const char *RP_LuaPeople_PluginName( lua_State *L ) {
	return rl.plugins[RP_Lua_CurrentPlugin( L )].name;
}

/*
================
What a player is
================
*/
// connected, playing (not spectating), not dead and not knocked down
static qboolean RP_LuaPeople_Alive( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	return ( ent->inuse && cl && cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR
		&& cl->tempSpectate < level.time && ent->health > 0 && cl->ps.pm_type != PM_DEAD && !G_PlayerIsDowned( ent ) ) ? qtrue : qfalse;
}

static qboolean RP_LuaPeople_Spectating( gentity_t *ent ) {
	return ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time ) ? qtrue : qfalse;
}

// playing a character (RPG mode), as the character_id field says
static qboolean RP_LuaPeople_OnCharacter( gentity_t *ent ) {
	return ( ent->client->sess.amrpgmode == 2 && ent->client->pers.CharID > 0 ) ? qtrue : qfalse;
}

// the rule /give, /scale, /teleport and the player effect commands share (g_cmds.c)
static qboolean RP_LuaPeople_Protected( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	return ( cl->sess.amrpgmode > 0 && ( cl->pers.bitvalue & ( 1 << ADM_ADMPROTECT ) ) && !( cl->pers.player_settings & ( 1 << 13 ) ) ) ? qtrue : qfalse;
}

static int RP_LuaPeople_MaxArmor( gentity_t *ent ) {
	return ( ent->client->sess.amrpgmode == 2 ) ? ent->client->pers.max_rpg_shield : ent->client->ps.stats[STAT_MAX_HEALTH];
}

// the effect names scripts use, for players and NPCs ("hidden" is for players only)
const char *RP_LuaPeople_EffectName( int mode ) {
	switch ( mode ) {
	case RP_PHASE_NONSOLID:	return "walkthrough";
	case RP_PHASE_GHOST:	return "ghost";
	case RP_PHASE_HOLO:		return "holo";
	case RP_PHASE_HIDDEN:	return "hidden";
	default:				return NULL;
	}
}

int RP_LuaPeople_EffectFromName( const char *name ) {
	if ( !strcmp( name, "walkthrough" ) ) {
		return RP_PHASE_NONSOLID;
	}
	if ( !strcmp( name, "ghost" ) ) {
		return RP_PHASE_GHOST;
	}
	if ( !strcmp( name, "holo" ) ) {
		return RP_PHASE_HOLO;
	}
	if ( !strcmp( name, "hidden" ) ) {
		return RP_PHASE_HIDDEN;
	}
	return -1;
}

const char *RP_LuaPeople_ModName( int mod ) {
	return ( mod >= 0 && mod < MOD_MAX && modNames[mod] ) ? modNames[mod] : "MOD_UNKNOWN";
}

static void RP_LuaPeople_PushOptInt( lua_State *L, qboolean have, int value ) {
	if ( have ) {
		lua_pushinteger( L, value );
	} else {
		lua_pushnil( L );
	}
}

/*
================
Teleporting (players here, a script's NPCs in rp_lua_world.c)
================
*/
// NULL when ent may be put at dest: not riding a vehicle (the vehicle would pull it back), its box clear of walls
// there, and nobody there G_KillBox() would telefrag or have to walk out of
const char *RP_LuaPeople_TeleportRefusal( gentity_t *ent, const vec3_t dest ) {
	vec3_t at;
	trace_t tr;
	int mask = ent->clipmask ? ent->clipmask : MASK_PLAYERSOLID;

	if ( RP_IsVehicleRider( ent ) ) {
		return "riding a vehicle";
	}
	VectorCopy( dest, at );
	at[2] += 1;		// where zyk_TeleportPlayer puts them
	trap->Trace( &tr, at, ent->r.mins, ent->r.maxs, at, ent->s.number, mask & ~CONTENTS_BODY, qfalse, 0, 0 );
	if ( tr.startsolid || tr.allsolid ) {
		return "the destination is blocked (inside a wall or the floor)";
	}
	if ( G_KillBoxWouldHit( ent, at ) ) {
		return "someone is standing at the destination";
	}
	return NULL;
}

void RP_LuaPeople_DoTeleport( gentity_t *ent, const vec3_t dest, float yaw ) {
	vec3_t to, angles;

	VectorCopy( dest, to );
	VectorSet( angles, 0, yaw, 0 );
	zyk_TeleportPlayer( ent, to, angles );
}

// an optional yaw argument, in degrees; def when absent
static float RP_LuaPeople_OptYaw( lua_State *L, int idx, float def ) {
	lua_Number n;
	int isnum;

	if ( lua_isnoneornil( L, idx ) ) {
		return def;
	}
	n = lua_tonumberx( L, idx, &isnum );
	if ( !isnum || n != n || n > 100000 || n < -100000 ) {
		luaL_error( L, "the yaw must be a number of degrees" );
		return def;
	}
	return (float)n;
}

static int RP_LuaPeople_Teleport( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	const char *why, *game;
	vec3_t dest;
	float yaw;

	RP_LuaWorld_CheckVec( L, 2, dest, "the position to teleport to" );
	yaw = RP_LuaPeople_OptYaw( L, 3, ent->client->ps.viewangles[YAW] );
	if ( !RP_LuaPeople_Alive( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not alive (spectating, dead or knocked down)" );
	}
	game = zyk_minigame_name( ent );
	if ( game ) {
		return RP_LuaPeople_Fail( L, va( "they are in the %s", game ) );
	}
	if ( ent->client->ps.duelInProgress ) {
		return RP_LuaPeople_Fail( L, "they are fighting a duel" );
	}
	if ( RP_LuaPeople_Protected( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are admin-protected" );
	}
	why = RP_LuaPeople_TeleportRefusal( ent, dest );
	if ( why ) {
		return RP_LuaPeople_Fail( L, why );
	}
	RP_LuaPeople_DoTeleport( ent, dest, yaw );
	G_LogPrintf( "Lua: %s teleported %s ^7to (%d %d %d)\n", RP_LuaPeople_PluginName( L ), ent->client->pers.netname,
		(int)dest[0], (int)dest[1], (int)dest[2] );
	lua_pushboolean( L, 1 );
	return 1;
}

/*
================
Health, armor, items
================
*/
static int RP_LuaPeople_Heal( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	lua_Integer n = luaL_checkinteger( L, 2 );
	int room;

	if ( n < 0 ) {
		return luaL_error( L, "heal: the amount cannot be negative" );
	}
	if ( !RP_LuaPeople_Alive( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not alive (spectating, dead or knocked down)" );
	}
	room = ent->client->ps.stats[STAT_MAX_HEALTH] - ent->health;
	if ( room < 0 ) {
		room = 0;
	}
	if ( n > room ) {
		n = room;
	}
	ent->health += (int)n;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;
	lua_pushinteger( L, n );
	return 1;
}

static int RP_LuaPeople_SetArmor( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	lua_Integer n = luaL_checkinteger( L, 2 );
	int max;

	if ( !RP_LuaPeople_Alive( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not alive (spectating, dead or knocked down)" );
	}
	max = RP_LuaPeople_MaxArmor( ent );
	if ( n > max ) {
		n = max;
	}
	if ( n < 0 ) {
		n = 0;
	}
	ent->client->ps.stats[STAT_ARMOR] = (int)n;
	lua_pushinteger( L, n );
	return 1;
}

// the item is in rp_lua_items, the server owner's list of what scripts may give
static qboolean RP_LuaPeople_ItemAllowed( const char *name ) {
	char list[MAX_CVAR_VALUE_STRING];
	const char *s;

	trap->Cvar_VariableStringBuffer( "rp_lua_items", list, sizeof( list ) );
	s = list;
	while ( 1 ) {
		char *tok = COM_ParseExt( &s, qfalse );
		if ( !tok[0] ) {
			return qfalse;
		}
		if ( !Q_stricmp( tok, name ) ) {
			return qtrue;
		}
	}
}

// what touching the item would give (Pickup_Weapon, Pickup_Ammo, Pickup_Holdable in g_items.c, for an item placed
// in the map), with the pickup's own rules first: BG_CanItemBeGrabbed refuses a full ammo type, a holdable they have,
// and anyone fighting a duel or in a mini-game
static int RP_LuaPeople_Give( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	const char *name = luaL_checkstring( L, 2 ), *game;
	playerState_t *ps = &ent->client->ps;
	entityState_t es;
	gitem_t *item;

	if ( !RP_LuaPeople_ItemAllowed( name ) ) {
		return RP_LuaPeople_Fail( L, va( "%s is not in rp_lua_items, the items scripts may give", name ) );
	}
	item = BG_FindItem( name );
	if ( !item ) {
		return RP_LuaPeople_Fail( L, va( "there is no item called %s", name ) );
	}
	if ( item->giType != IT_WEAPON && item->giType != IT_AMMO && item->giType != IT_HOLDABLE ) {
		return RP_LuaPeople_Fail( L, "only weapons, ammo and holdable items can be given (heal and set_armor do health and armor)" );
	}
	if ( !RP_LuaPeople_Alive( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not alive (spectating, dead or knocked down)" );
	}
	game = zyk_minigame_name( ent );
	if ( game ) {
		return RP_LuaPeople_Fail( L, va( "they are in the %s", game ) );
	}
	memset( &es, 0, sizeof( es ) );
	es.modelindex = item - bg_itemlist;
	es.generic1 = -1;
	if ( !BG_CanItemBeGrabbed( level.gametype, &es, ps ) ) {
		return RP_LuaPeople_Fail( L, "they cannot take it now (they carry all of it they can, or they are fighting a duel)" );
	}

	if ( item->giType == IT_WEAPON ) {
		int quantity = item->quantity;

		if ( level.gametype != GT_TEAM ) {
			// Pickup_Weapon's respawning rule, as it reads it
			if ( ps->ammo[item->giTag] < quantity * 0.5 ) {
				quantity = quantity - ps->ammo[item->giTag];
			} else {
				quantity = quantity * 0.5;
			}
		}
		ps->stats[STAT_WEAPONS] |= ( 1 << item->giTag );
		Add_Ammo( ent, weaponData[item->giTag].ammoIndex, RP_ScaledAmmo( quantity ) );
	} else if ( item->giType == IT_AMMO ) {
		if ( item->giTag == -1 ) {
			// an ammo_all, as Pickup_Ammo gives it outside Siege
			Add_Ammo( ent, AMMO_BLASTER, 50 );
			Add_Ammo( ent, AMMO_POWERCELL, 50 );
			Add_Ammo( ent, AMMO_METAL_BOLTS, 50 );
			Add_Ammo( ent, AMMO_ROCKETS, 2 );
		} else {
			Add_Ammo( ent, item->giTag, RP_ScaledAmmo( item->quantity ) );
		}
	} else {
		ps->stats[STAT_HOLDABLE_ITEM] = item - bg_itemlist;
		ps->stats[STAT_HOLDABLE_ITEMS] |= ( 1 << item->giTag );
	}
	G_LogPrintf( "Lua: %s gave %s ^7%s\n", RP_LuaPeople_PluginName( L ), ent->client->pers.netname, item->classname );
	lua_pushboolean( L, 1 );
	return 1;
}

/*
================
Player effects
================
*/
static int RP_LuaPeople_SetEffect( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	int mode = RP_PHASE_NONE;

	if ( !lua_isnoneornil( L, 2 ) ) {
		mode = RP_LuaPeople_EffectFromName( luaL_checkstring( L, 2 ) );
		if ( mode < 0 ) {
			return luaL_error( L, "set_effect: \"%s\" is not an effect (walkthrough, ghost, holo, hidden, or nil for none)", lua_tostring( L, 2 ) );
		}
	}
	if ( RP_LuaPeople_Spectating( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are spectating" );
	}
	if ( RP_LuaPeople_Protected( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are admin-protected" );
	}
	RP_SetPhaseMode( ent, mode, plugin + 1 );
	G_LogPrintf( "Lua: %s made %s ^7%s\n", rl.plugins[plugin].name, ent->client->pers.netname,
		( mode == RP_PHASE_NONE ) ? "normal" : RP_PhaseModeName( mode ) );
	lua_pushboolean( L, 1 );
	return 1;
}

// a plugin is switched off or reloaded: the effects it set that nobody has changed since go
void RP_LuaPeople_RemovePlugin( int plugin ) {
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t *ent = &g_entities[i];
		if ( ent->client && ent->client->pers.phase_by_script == plugin + 1 ) {
			RP_ClearPhaseMode( ent );
		}
	}
}

// the state is closing: no script is left to own an effect
void RP_LuaPeople_Reset( void ) {
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t *ent = &g_entities[i];
		if ( ent->client && ent->client->pers.phase_by_script ) {
			RP_ClearPhaseMode( ent );
		}
	}
}

/*
================
Skills
================
*/
// skill n as /skillup numbers it (from 1): its index, or a Lua error
static int RP_LuaPeople_CheckSkill( lua_State *L, int idx ) {
	lua_Integer n = luaL_checkinteger( L, idx );

	if ( n < 1 || n > NUM_OF_SKILLS ) {
		return luaL_error( L, "there is no skill %d (1 to %d, as /skillup numbers them)", (int)n, NUM_OF_SKILLS );
	}
	return (int)n - 1;
}

static int RP_LuaPeople_Skill( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	int id = RP_LuaPeople_CheckSkill( L, 2 );

	if ( !RP_LuaPeople_OnCharacter( ent ) ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, ent->client->pers.skill_levels[id] );
	lua_pushinteger( L, skills[id].max_level );
	lua_pushstring( L, skills[id].skill_name ? skills[id].skill_name : "" );
	return 3;
}

static int RP_LuaPeople_SkillChange( lua_State *L, qboolean up ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	int id = RP_LuaPeople_CheckSkill( L, 2 );
	lua_Integer count = luaL_optinteger( L, 3, 1 );
	int room, n;
	qboolean done;

	if ( count < 1 ) {
		return luaL_error( L, "%s: the count must be 1 or more", up ? "skill_up" : "skill_down" );
	}
	if ( !RP_LuaPeople_OnCharacter( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not playing a character" );
	}
	if ( id == RP_LUA_UNUSED_SKILL ) {
		return RP_LuaPeople_Fail( L, va( "skill %d is no longer in use", id + 1 ) );
	}
	room = up ? skills[id].max_level - ent->client->pers.skill_levels[id] : ent->client->pers.skill_levels[id];
	if ( room <= 0 ) {
		return RP_LuaPeople_Fail( L, up ? "the skill is already at its maximum" : "the skill is already at 0" );
	}
	n = ( count < room ) ? (int)count : room;
	if ( up && ent->client->pers.skillpoints < n ) {
		return RP_LuaPeople_Fail( L, va( "not enough skillpoints (%d needed, %d left)", n, ent->client->pers.skillpoints ) );
	}
	// the commands' own code: the player sees its message, then the change is saved as /skillup saves it
	done = up ? do_upgrade_skill( ent, ent, id, qfalse, n ) : do_downgrade_skill( ent, ent, id, n );
	if ( !done ) {
		return RP_LuaPeople_Fail( L, "the change was refused" );
	}
	update_skills_table_row_with_current_values( ent );
	G_LogPrintf( "Lua: %s %s %s^7's skill %d (%s) by %d\n", RP_LuaPeople_PluginName( L ), up ? "raised" : "lowered",
		ent->client->pers.netname, id + 1, skills[id].skill_name ? skills[id].skill_name : "?", n );
	lua_pushinteger( L, n );
	return 1;
}

static int RP_LuaPeople_SkillUp( lua_State *L ) {
	return RP_LuaPeople_SkillChange( L, qtrue );
}

static int RP_LuaPeople_SkillDown( lua_State *L ) {
	return RP_LuaPeople_SkillChange( L, qfalse );
}

/*
================
A sound for one player
================
*/
static int RP_LuaPeople_PlaySound( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client], *te;
	const char *name = luaL_checkstring( L, 2 );
	char reason[256];
	int index;

	RP_Lua_CurrentPlugin( L );
	if ( !RP_LuaWorld_PathName( name ) ) {
		return luaL_error( L, "play_sound: \"%s\" is not a sound file name", name );
	}
	if ( !RP_SoundFileExists( name ) ) {
		return RP_LuaPeople_Fail( L, va( "sound %s is not on the server", name ) );
	}
	if ( ent->client->pers.connected != CON_CONNECTED ) {
		return RP_LuaPeople_Fail( L, "they are still connecting" );
	}
	if ( !RP_LuaWorld_EventRoom( 1 ) ) {
		return RP_LuaPeople_Fail( L, "too many effects, sounds and shakes this frame" );
	}
	reason[0] = '\0';
	index = RP_PlaySoundIndex( name, reason, sizeof( reason ) );
	if ( !index ) {
		return RP_LuaPeople_Fail( L, reason[0] ? reason : "no sound slot is free" );
	}
	// heard at full volume wherever they are, and sent to nobody else (as g_entbounds.c sends its lines)
	te = G_TempEntity( ent->client->ps.origin, EV_GLOBAL_SOUND );
	te->s.eventParm = index;
	te->r.svFlags |= SVF_BROADCAST | SVF_SINGLECLIENT;
	te->r.singleClient = client;
	lua_pushboolean( L, 1 );
	return 1;
}

/*
================
Player handle fields (rp_lua_api.c's __index falls through to here)
================
*/
int RP_LuaPeople_PlayerField( lua_State *L, int client, const char *key ) {
	static const luaL_Reg methods[] = {
		{ "teleport", RP_LuaPeople_Teleport }, { "heal", RP_LuaPeople_Heal }, { "set_armor", RP_LuaPeople_SetArmor },
		{ "give", RP_LuaPeople_Give }, { "set_effect", RP_LuaPeople_SetEffect }, { "skill", RP_LuaPeople_Skill },
		{ "skill_up", RP_LuaPeople_SkillUp }, { "skill_down", RP_LuaPeople_SkillDown }, { "play_sound", RP_LuaPeople_PlaySound },
		{ NULL, NULL } };
	gentity_t *ent = &g_entities[client];
	gclient_t *cl = &level.clients[client];
	qboolean onChar = ( cl->sess.amrpgmode == 2 && cl->pers.CharID > 0 ) ? qtrue : qfalse;
	int i;

	for ( i = 0; methods[i].name; i++ ) {
		if ( !strcmp( key, methods[i].name ) ) {
			lua_pushcfunction( L, methods[i].func );
			return 1;
		}
	}
	if ( !ent->client ) {
		return 0;	// not set up yet (connecting): every field below is nil
	}

	if ( !strcmp( key, "alive" ) ) {
		lua_pushboolean( L, RP_LuaPeople_Alive( ent ) );
	} else if ( !strcmp( key, "downed" ) ) {
		lua_pushboolean( L, G_PlayerIsDowned( ent ) );
	} else if ( !strcmp( key, "health" ) ) {
		lua_pushinteger( L, ent->health );
	} else if ( !strcmp( key, "max_health" ) ) {
		lua_pushinteger( L, cl->ps.stats[STAT_MAX_HEALTH] );
	} else if ( !strcmp( key, "armor" ) ) {
		lua_pushinteger( L, cl->ps.stats[STAT_ARMOR] );
	} else if ( !strcmp( key, "max_armor" ) ) {
		lua_pushinteger( L, RP_LuaPeople_MaxArmor( ent ) );
	} else if ( !strcmp( key, "origin" ) ) {
		RP_LuaWorld_PushVec( L, cl->ps.origin );
	} else if ( !strcmp( key, "yaw" ) ) {
		lua_pushnumber( L, cl->ps.viewangles[YAW] );
	} else if ( !strcmp( key, "account_id" ) ) {
		RP_LuaPeople_PushOptInt( L, cl->sess.amrpgmode > 0, cl->sess.accountID );
	} else if ( !strcmp( key, "character_name" ) ) {
		if ( onChar && cl->sess.rpgchar[0] ) {
			lua_pushstring( L, cl->sess.rpgchar );
		} else {
			lua_pushnil( L );
		}
	} else if ( !strcmp( key, "level" ) ) {
		RP_LuaPeople_PushOptInt( L, onChar, cl->pers.level );
	} else if ( !strcmp( key, "xp" ) ) {
		RP_LuaPeople_PushOptInt( L, onChar, cl->pers.xp );
	} else if ( !strcmp( key, "credits" ) ) {
		RP_LuaPeople_PushOptInt( L, onChar, cl->pers.credits );
	} else if ( !strcmp( key, "skillpoints" ) ) {
		RP_LuaPeople_PushOptInt( L, onChar, cl->pers.skillpoints );
	} else if ( !strcmp( key, "riding" ) ) {
		lua_pushboolean( L, RP_IsVehicleRider( ent ) );
	} else if ( !strcmp( key, "minigame" ) ) {
		const char *game = zyk_minigame_name( ent );
		if ( game ) {
			lua_pushstring( L, game );
		} else {
			lua_pushnil( L );
		}
	} else if ( !strcmp( key, "effect" ) ) {
		const char *name = RP_LuaPeople_EffectName( cl->pers.phase_mode );
		if ( name ) {
			lua_pushstring( L, name );
		} else {
			lua_pushnil( L );
		}
	} else {
		return 0;
	}
	return 1;
}

/*
================
Events: player_death, player_downed, npc_death, damage
================
*/
// whom a handler is told did it: nil for the world, a fall, a suicide or an NPC's own kill()
static gentity_t *RP_LuaPeople_Killer( gentity_t *self, gentity_t *attacker ) {
	if ( !attacker || !attacker->inuse || attacker == self || attacker->s.number == ENTITYNUM_WORLD || attacker->s.number == ENTITYNUM_NONE ) {
		return NULL;
	}
	return attacker;
}

void RP_Lua_Death( gentity_t *self, gentity_t *attacker, int mod ) {
	gentity_t *killer;

	if ( !rl.L || !self || !self->inuse || !self->client ) {
		return;
	}
	killer = RP_LuaPeople_Killer( self, attacker );
	RP_LuaWorld_EnterCallback();
	if ( self->s.number < MAX_CLIENTS ) {
		if ( RP_LuaApi_HasHandlers( RPLE_PLAYER_DEATH ) ) {
			RP_LuaApi_FireActorEvent( RPLE_PLAYER_DEATH, self, killer, 0, mod );
		}
	} else if ( self->NPC ) {
		RP_LuaWorld_NpcDied( self, killer );		// its own script's on_death first
		if ( self->inuse && RP_LuaApi_HasHandlers( RPLE_NPC_DEATH ) ) {
			RP_LuaApi_FireActorEvent( RPLE_NPC_DEATH, self, killer, 0, mod );
		}
	}
	RP_LuaWorld_LeaveCallback();
}

void RP_Lua_Damage( gentity_t *targ, gentity_t *attacker, int amount, int mod ) {
	if ( !rl.L || !targ || !targ->inuse || !targ->client || amount <= 0 || !RP_LuaApi_HasHandlers( RPLE_DAMAGE ) ) {
		return;
	}
	if ( targ->s.number >= MAX_CLIENTS && !targ->NPC ) {
		return;
	}
	RP_LuaWorld_EnterCallback();
	RP_LuaApi_FireActorEvent( RPLE_DAMAGE, targ, RP_LuaPeople_Killer( targ, attacker ), amount, mod );
	RP_LuaWorld_LeaveCallback();
}

void RP_Lua_Downed( gentity_t *ent, gentity_t *attacker ) {
	if ( !rl.L || !ent || !ent->inuse || !ent->client || ent->s.number >= MAX_CLIENTS || !RP_LuaApi_HasHandlers( RPLE_PLAYER_DOWNED ) ) {
		return;
	}
	RP_LuaWorld_EnterCallback();
	RP_LuaApi_FireActorEvent( RPLE_PLAYER_DOWNED, ent, RP_LuaPeople_Killer( ent, attacker ), 0, 0 );
	RP_LuaWorld_LeaveCallback();
}
