/*
===========================================================================
GLX_U: [Lua] API version 3: players. What a player handle tells about the player (health, where they
stand, their character, their skills...) and the few things a script may do to one: teleport them,
heal them, set their armor, give them an item the server owner allows, give them one of the admins'
player effects, raise or lower a skill with their own skillpoints, and play a sound only they hear.
Also the events the game raises about players and NPCs: player_death, player_downed, npc_death and
damage. NPC handles and World.npc are in rp_lua_world.c, with the other entity handles.

API version 4: the economy. A script can pay a character credits, charge them (all or nothing), and give
them XP, through the game's own add_credits(), remove_credits() and RP_LevelUpFromXP(), saved at once as
the admin commands save. Every call names its reason; an amount above rp_lua_maxcredits / rp_lua_maxxp is
cut to that cap with a warning, a cap of 0 switches the action off, and every payment is logged. Each
plugin's totals for the map show in /scripts stats. The level_up and level_down events come from
increase_level() and decrease_level(), whoever changed the level.

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
extern void add_credits( gentity_t *ent, int credits );
extern void remove_credits( gentity_t *ent, int credits );
extern void update_credits_value( gentity_t *ent );
extern void update_chars_table_row_with_current_values( gentity_t *ent );
extern int check_xp( int currentLevel );

#define RP_LUA_UNUSED_SKILL		38		// skill_id 38 (/skillup's number 39) is the shop upgrades' storage
#define RP_LUA_REASON_MAX		64		// the reason a payment gives, for the log
#define RP_LUA_AMOUNT_CAP_MAX	1000000000

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

// API 6 (rp_lua_orders.c): an NPC is not ordered to attack an admin-protected player either
qboolean RP_LuaPeople_IsProtected( gentity_t *ent ) {
	return RP_LuaPeople_Protected( ent );
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
	// GLX_U: its two effects are temp entities, counted like World.effect's (a loop of teleports ran the server
	// out of entities)
	if ( !RP_LuaWorld_EventRoom( 2 ) ) {
		return RP_LuaPeople_Fail( L, "too many effects, sounds and shakes this frame" );
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

// what touching the item would give (Pickup_Weapon, Pickup_Ammo, Pickup_Holdable in g_items.c, for an item placed
// in the map), with the pickup's own rules first: BG_CanItemBeGrabbed refuses a full ammo type, a holdable they have,
// and anyone fighting a duel or in a mini-game. GLX_U: any weapon, ammo or holdable item, with no list in the server
// config (rp_lua_items is gone); never the emplaced gun's or the turret's weapon, which only work from inside that
// gun: past LAST_USEABLE_WEAPON, the two the admin /give guns leaves out too (g_cmds.c)
static int RP_LuaPeople_Give( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	const char *name = luaL_checkstring( L, 2 ), *game;
	playerState_t *ps = &ent->client->ps;
	entityState_t es;
	gitem_t *item;

	item = BG_FindItem( name );
	if ( !item ) {
		return RP_LuaPeople_Fail( L, va( "there is no item called %s", name ) );
	}
	if ( item->giType != IT_WEAPON && item->giType != IT_AMMO && item->giType != IT_HOLDABLE ) {
		return RP_LuaPeople_Fail( L, "only weapons, ammo and holdable items can be given (heal and set_armor do health and armor)" );
	}
	if ( item->giType == IT_WEAPON && item->giTag > LAST_USEABLE_WEAPON ) {
		return RP_LuaPeople_Fail( L, va( "%s is not a player weapon", item->classname ) );
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
	// GLX_U: the player is sent a line for every change (show_skill_change_message): it counts in their message
	// budget, so a loop of skill_up and skill_down could no longer flood them off the server
	if ( !RP_Lua_MessageRoom( client, 1 ) ) {
		return RP_Lua_MessageRefused( L, RP_Lua_CurrentPlugin( L ), RP_LUA_MSG_REFUSED );
	}
	// the commands' own code: the player sees its message, then the change is saved as /skillup saves it
	done = up ? do_upgrade_skill( ent, ent, id, qfalse, n ) : do_downgrade_skill( ent, ent, id, n );
	RP_Lua_MessageSent( client, 1 );
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
Credits and XP (API 4)
================
*/
// the amount (argument 2) and the reason (argument 3) of a payment, checked (a Lua error when they are wrong),
// and the cap: *limit is 0 when the action is switched off
static int RP_LuaPeople_Payment( lua_State *L, const char *method, vmCvar_t *cap, int *limit, char *reason, int reasonSize ) {
	lua_Integer n;
	size_t len;
	int isnum;
	const char *r;

	n = lua_tointegerx( L, 2, &isnum );
	if ( !isnum || n < 1 || n > RP_LUA_AMOUNT_CAP_MAX ) {
		luaL_error( L, "%s: the amount must be a whole number from 1 to %d", method, RP_LUA_AMOUNT_CAP_MAX );
		return 0;
	}
	if ( lua_type( L, 3 ) != LUA_TSTRING ) {
		luaL_error( L, "%s: give a reason (1 to %d characters) for the log", method, RP_LUA_REASON_MAX );
		return 0;
	}
	r = lua_tolstring( L, 3, &len );
	if ( !len || len > RP_LUA_REASON_MAX || strlen( r ) != len ) {
		luaL_error( L, "%s: the reason must be 1 to %d characters", method, RP_LUA_REASON_MAX );
		return 0;
	}
	RP_Lua_CleanText( reason, reasonSize, r, qfalse );

	*limit = cap->integer;
	if ( *limit < 0 ) {
		*limit = 0;
	} else if ( *limit > RP_LUA_AMOUNT_CAP_MAX ) {
		*limit = RP_LUA_AMOUNT_CAP_MAX;
	}
	return (int)n;
}

// once the payment is going ahead: an amount above the cap is cut to it, with a warning for the plugin's author
static int RP_LuaPeople_Capped( lua_State *L, const char *method, const char *what, const char *capName, int n, int limit ) {
	if ( n > limit ) {
		RP_Lua_ReportWarning( RP_Lua_CurrentPlugin( L ), va( "%s: %d %s cut to %s (%d)", method, n, what, capName, limit ) );
		return limit;
	}
	return n;
}

static int RP_LuaPeople_AddCredits( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	char reason[RP_LUA_REASON_MAX + 1];
	int limit, n = RP_LuaPeople_Payment( L, "add_credits", &rp_lua_maxcredits, &limit, reason, sizeof( reason ) );
	int before, given;

	if ( !limit ) {
		return RP_LuaPeople_Fail( L, "giving credits is switched off on this server (rp_lua_maxcredits 0)" );
	}
	if ( !RP_LuaPeople_OnCharacter( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not playing a character" );
	}
	n = RP_LuaPeople_Capped( L, "add_credits", "credits", "rp_lua_maxcredits", n, limit );
	before = ent->client->pers.credits;
	add_credits( ent, n );		// rp_max_rpg_credits caps the balance
	given = ent->client->pers.credits - before;
	if ( given > 0 ) {
		update_credits_value( ent );
		rl.plugins[plugin].creditsGiven += given;
		G_LogPrintf( "Lua: %s gave %d credits to %s^7 (char %d): %s\n", rl.plugins[plugin].name, given,
			ent->client->pers.netname, ent->client->pers.CharID, reason );
	}
	lua_pushinteger( L, given );
	return 1;
}

static int RP_LuaPeople_TakeCredits( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	char reason[RP_LUA_REASON_MAX + 1];
	int limit, n = RP_LuaPeople_Payment( L, "take_credits", &rp_lua_maxcredits, &limit, reason, sizeof( reason ) );

	if ( !limit ) {
		return RP_LuaPeople_Fail( L, "taking credits is switched off on this server (rp_lua_maxcredits 0)" );
	}
	if ( !RP_LuaPeople_OnCharacter( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not playing a character" );
	}
	n = RP_LuaPeople_Capped( L, "take_credits", "credits", "rp_lua_maxcredits", n, limit );
	if ( ent->client->pers.credits < n ) {
		return RP_LuaPeople_Fail( L, va( "not enough credits (%d needed, %d held)", n, ent->client->pers.credits ) );
	}
	remove_credits( ent, n );
	update_credits_value( ent );
	rl.plugins[plugin].creditsTaken += n;
	G_LogPrintf( "Lua: %s took %d credits from %s^7 (char %d): %s\n", rl.plugins[plugin].name, n,
		ent->client->pers.netname, ent->client->pers.CharID, reason );
	lua_pushinteger( L, n );
	return 1;
}

static int RP_LuaPeople_AddXp( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gentity_t *ent = &g_entities[client];
	char reason[RP_LUA_REASON_MAX + 1];
	int limit, n = RP_LuaPeople_Payment( L, "add_xp", &rp_lua_maxxp, &limit, reason, sizeof( reason ) );
	int levels, charID;
	long long xp;
	char name[MAX_NETNAME];

	if ( !limit ) {
		return RP_LuaPeople_Fail( L, "giving XP is switched off on this server (rp_lua_maxxp 0)" );
	}
	if ( !RP_LuaPeople_OnCharacter( ent ) ) {
		return RP_LuaPeople_Fail( L, "they are not playing a character" );
	}
	if ( ent->client->pers.level >= rp_rpg_max_level.integer ) {
		return RP_LuaPeople_Fail( L, "they are at the maximum level" );
	}
	n = RP_LuaPeople_Capped( L, "add_xp", "XP", "rp_lua_maxxp", n, limit );
	// GLX_U: a level-up is answered with the game's "New Level" line (increase_level): it counts in the player's
	// message budget, so XP that would level them up waits while it is spent
	if ( (long long)ent->client->pers.xp + n >= check_xp( ent->client->pers.level ) && !RP_Lua_MessageRoom( client, 1 ) ) {
		return RP_Lua_MessageRefused( L, plugin, RP_LUA_MSG_REFUSED );
	}
	// noted before the levels come: the level_up handlers run inside RP_LevelUpFromXP(), so the save below checks
	// the player is still on the same character before writing
	Q_strncpyz( name, ent->client->pers.netname, sizeof( name ) );
	charID = ent->client->pers.CharID;
	rl.plugins[plugin].xpGiven += n;
	xp = (long long)ent->client->pers.xp + n;
	ent->client->pers.xp = ( xp > 0x7fffffff ) ? 0x7fffffff : (int)xp;
	levels = RP_LevelUpFromXP( ent );		// as /givexp: the levels it covers at once, the rest kept (level_up runs here)
	if ( levels > 0 ) {		// GLX_U: what increase_level sent: "New Level", and the maximum-level line when it got there
		RP_Lua_MessageSent( client, ( ent->client->pers.level >= rp_rpg_max_level.integer ) ? 2 : 1 );
	}
	if ( RP_LuaPeople_OnCharacter( ent ) && ent->client->pers.CharID == charID ) {
		update_chars_table_row_with_current_values( ent );
	}
	G_LogPrintf( "Lua: %s gave %d XP to %s^7 (char %d, %d level%s): %s\n", rl.plugins[plugin].name, n, name, charID,
		levels, levels == 1 ? "" : "s", reason );
	lua_pushinteger( L, levels );
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
		{ "add_credits", RP_LuaPeople_AddCredits }, { "take_credits", RP_LuaPeople_TakeCredits }, { "add_xp", RP_LuaPeople_AddXp },
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
	} else if ( !strcmp( key, "xp_next" ) ) {		// API 4
		RP_LuaPeople_PushOptInt( L, onChar && cl->pers.level < rp_rpg_max_level.integer, check_xp( cl->pers.level ) );
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

// increase_level() gave levels (> 0) or decrease_level() took some (< 0): level_up / level_down. It may run inside
// player_die (an NPC's reward), so free() and kill() wait as in the other events.
void RP_Lua_LevelChanged( gentity_t *ent, int levels ) {
	rpLuaEvent_t ev = ( levels > 0 ) ? RPLE_LEVEL_UP : RPLE_LEVEL_DOWN;

	if ( !rl.L || !ent || !ent->inuse || !ent->client || !levels || ent->s.number < 0 || ent->s.number >= MAX_CLIENTS
		|| !RP_LuaApi_HasHandlers( ev ) ) {
		return;
	}
	RP_LuaWorld_EnterCallback();
	RP_LuaApi_FireLevelEvent( ev, ent->s.number, ent->client->pers.level, ( levels > 0 ) ? levels : -levels );
	RP_LuaWorld_LeaveCallback();
}
