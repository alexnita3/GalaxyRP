/*
===========================================================================
GLX_U: [Lua] what scripts can call (API version 1, and the events of versions 2 to 4), and the C side of
events, commands, timers and tasks. Everything here runs inside RP_Lua_Call(); a script error raised here is reported against
the plugin that is running. Nothing in this file lets a script run a console command, touch a
file, or reach the engine directly.

Globals: print, include, Plugin, Events, Timer, Task, Commands, Msg, Server (Storage is in
rp_lua_store.c, World in rp_lua_world.c (NPC orders in rp_lua_orders.c), Dialog in rp_lua_dialog.c; what a player handle adds in API version 3 is in
rp_lua_people.c). Player handles are userdata of type GLX.Player; one handle per connection, so the
same player is always the same value (usable as a table key) until they disconnect, after which
using it is a script error rather than a crash.
===========================================================================
*/

#include "rp_lua_local.h"
#include <time.h>

#define RP_LUA_PLAYER_MT	"GLX.Player"

typedef struct {
	int		client;
	int		gen;
} rpLuaPlayerHandle_t;

typedef struct {
	qboolean	active;
	int			plugin;
	int			ref;
	int			id;
	char		filter[RP_LUA_FILTER_SIZE];	// "use": the targetname; "map_event": the name; "npc_death": the npc_type;
											// "damage": "players" or "npcs"; "" for all
	char		filter2[RP_LUA_FILTER_SIZE];	// "npc_death": the spawner's targetname, "" for any
} rpLuaHandler_t;

typedef struct {
	qboolean	active;
	int			plugin;
	int			ref;
	char		name[RP_LUA_CMD_NAME_SIZE];
	char		help[RP_LUA_CMD_HELP_SIZE];
	int			adminBit;		// -1: anyone
	qboolean	loggedIn;
} rpLuaCommand_t;

typedef struct {
	qboolean	active;
	int			plugin;
	int			ref;
	int			id;
	int			due;			// level.time
	int			interval;		// 0: once
} rpLuaTimer_t;

typedef struct {
	qboolean	active;
	qboolean	running;		// being resumed right now
	qboolean	cancelled;		// Task.cancel while running: finished when it next stops
	qboolean	started;
	qboolean	waitSet;		// Task.wait set the wake time during this run
	int			plugin;
	int			threadRef;
	lua_State	*thread;
	int			id;
	int			wake;			// level.time
	int			nargs;
} rpLuaTask_t;

static rpLuaHandler_t	rpHandlers[RPLE_NUM][RP_LUA_MAX_HANDLERS];
static rpLuaCommand_t	rpCommands[RP_LUA_MAX_COMMANDS];
static rpLuaTimer_t		rpTimers[RP_LUA_MAX_TIMERS];
static rpLuaTask_t		rpTasks[RP_LUA_MAX_TASKS];
static int				rpNextId = 1;

static const char *rpLuaEventNames[RPLE_NUM] = {
	"init", "shutdown", "unload", "frame", "client_connect", "client_begin", "client_spawn",
	"client_disconnect", "chat", "command", "use", "map_event", "player_death", "player_downed", "npc_death", "damage",
	"level_up", "level_down"
};

// names a script command may not take: commands the engine, our cgame or our ui handle on the
// client (a player typing them never reaches the server), and commands the engine's server handles
// before the game sees them. Built-in game commands are checked separately (G_IsBuiltinClientCommand).
// The engine part was taken from TaystJK's Cmd_AddCommand calls; tests_lua checks the cgame and ui
// parts against cg_consolecmds.c and ui_atoms.c.
static const char *rpLuaReservedNames[] = {
	// engine (TaystJK client, common, server and renderer console commands)
	"addfavorite", "afk", "automap_button", "automap_toggle", "bind", "bindlist", "bow", "capframes",
	"centerview", "changevectors", "cinematic", "clear", "clearremaps", "clientinfo", "clientkick",
	"cmd", "cmdlist", "colorname", "colorstring", "condump", "configstrings", "connect", "cp", "crash",
	"cvar_modified", "cvar_restart", "cvar_usercreated", "cvaradd", "cvardiv", "cvarlist", "cvarmod",
	"cvarmult", "cvarsub", "delay", "delaycancel", "deletedemo", "demo", "demo_restart", "devmap",
	"devmapall", "devmapbsp", "devmapmdl", "dir", "disconnect", "donedl", "download", "dumpuser",
	"echo", "engage_duel", "error", "exec", "execq", "exit", "fdir", "flourish", "fontlist",
	"force_absorb", "force_distract", "force_forcepowerother", "force_heal", "force_healother",
	"force_protect", "force_pull", "force_rage", "force_seeing", "force_speed", "force_throw",
	"forcepowers", "forcetoggle", "freeze", "fs_openedlist", "fs_referencedlist", "fs_restart",
	"gfxinfo", "gfxmeminfo", "gloat", "globalservers", "heartbeat", "help", "ifcvar", "imagecacheinfo",
	"imagelist", "in_restart", "kick", "kickall", "kickbots", "kicknum", "killserver", "loadmod",
	"localservers", "map", "map_restart", "meditate", "messagemode", "messagemode2", "messagemode3",
	"messagemode4", "minimize", "mnext", "model", "modelcacheinfo", "modelist", "modellist",
	"mp3_calcvols", "mpause", "mprev", "mstop", "music", "net_restart", "nextdl", "path", "ping",
	"play", "playdemo", "print", "quit", "r_atihack", "r_we", "rcon", "reconnect", "record",
	"remapsky", "reset", "s_dynamic", "saberattackcycle", "screenshot", "screenshot_png",
	"screenshot_tga", "sectorlist", "serverinfo", "serverstatus", "set", "seta", "sets", "setu",
	"shaderlist", "showip", "skinlist", "snd_restart", "soundinfo", "soundlist", "soundstop",
	"status", "stopdl", "stopmusic", "stoprecord", "stopvideo", "strsub", "sv_banaddr", "sv_bandel",
	"sv_exceptaddr", "sv_exceptdel", "sv_flushbans", "sv_listbans", "sv_listrecording",
	"sv_rehashbans", "sv_saberswitch", "svdemoclearmeta", "svdemoclearprerecord", "svdemometa",
	"svrecord", "svrenamedemo", "svsay", "svstoprecord", "svtell", "systeminfo", "taunt", "toggle",
	"toggleconsole", "togglemenu", "touchfile", "unbind", "unbindall", "unset", "unset_usercreated",
	"use_ammodisp", "use_bacta", "use_bactabig", "use_cloak", "use_electrobinoculars", "use_eweb",
	"use_field", "use_healthdisp", "use_jetpack", "use_seeker", "use_sentry", "usegivenforce",
	"userinfo", "vbolist", "vdr", "vid_restart", "video", "voicechat", "vstr", "wait", "waitf",
	"waitfcancel", "weapontoggle", "which", "whitelistip", "write", "writeconfig", "zone_details",
	"zone_memrecovertest", "zone_stats", "zoom",
	// our cgame (cg_consolecmds.c)
	"char", "briefing", "clientlist", "cosmetics", "forcenext", "forceprev", "invnext", "invprev",
	"loaddeferred", "loadhud", "nextframe", "nextskin", "prevframe", "prevskin", "rpx",
	"siegecompletecvarupdate", "siegecvarupdate", "sizedown", "sizeup", "startorbit", "tcmd",
	"tell_attacker", "tell_target", "testgun", "testmodel", "viewpos", "weapnext", "weapon",
	"weaponclean", "weapprev",
	// our ui (ui_atoms.c)
	"ui_cache", "ui_load", "ui_openmenu", "ui_opensiegemenu", "ui_report",
	NULL
};

qboolean RP_LuaApi_ReservedCommand( const char *name ) {
	int i;

	if ( G_IsBuiltinClientCommand( name ) || !Q_stricmpn( name, "bot_", 4 ) ) {
		return qtrue;
	}
	for ( i = 0; rpLuaReservedNames[i]; i++ ) {
		if ( !Q_stricmp( name, rpLuaReservedNames[i] ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
================
Players
================
*/
static qboolean RP_LuaApi_PlayerValid( const rpLuaPlayerHandle_t *h ) {
	if ( h->client < 0 || h->client >= level.maxclients || h->client >= MAX_CLIENTS ) {
		return qfalse;
	}
	if ( level.clients[h->client].pers.connected == CON_DISCONNECTED ) {
		return qfalse;
	}
	return ( h->gen == rpLuaClientGen[h->client] ) ? qtrue : qfalse;
}

void RP_LuaApi_PushPlayer( lua_State *L, int client ) {
	rpLuaPlayerHandle_t *h;

	if ( client < 0 || client >= level.maxclients || client >= MAX_CLIENTS
		|| level.clients[client].pers.connected == CON_DISCONNECTED ) {
		lua_pushnil( L );
		return;
	}
	if ( rl.playerRef[client] != LUA_NOREF ) {
		lua_rawgeti( L, LUA_REGISTRYINDEX, rl.playerRef[client] );
		h = (rpLuaPlayerHandle_t *)lua_touserdata( L, -1 );
		if ( h && h->gen == rpLuaClientGen[client] ) {
			return;
		}
		lua_pop( L, 1 );
		luaL_unref( L, LUA_REGISTRYINDEX, rl.playerRef[client] );
		rl.playerRef[client] = LUA_NOREF;
	}
	h = (rpLuaPlayerHandle_t *)lua_newuserdatauv( L, sizeof( *h ), 0 );
	h->client = client;
	h->gen = rpLuaClientGen[client];
	luaL_setmetatable( L, RP_LUA_PLAYER_MT );
	lua_pushvalue( L, -1 );
	rl.playerRef[client] = luaL_ref( L, LUA_REGISTRYINDEX );
}

void RP_LuaApi_PlayerGone( int client ) {
	if ( rl.L && client >= 0 && client < MAX_CLIENTS && rl.playerRef[client] != LUA_NOREF ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, rl.playerRef[client] );
		rl.playerRef[client] = LUA_NOREF;
	}
}

int RP_LuaApi_CheckPlayer( lua_State *L, int idx ) {
	rpLuaPlayerHandle_t *h = (rpLuaPlayerHandle_t *)luaL_checkudata( L, idx, RP_LUA_PLAYER_MT );
	if ( !RP_LuaApi_PlayerValid( h ) ) {
		return luaL_error( L, "that player has left the server" );
	}
	return h->client;
}

static int RP_LuaApi_PlayerHasAdmin( lua_State *L ) {
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	lua_Integer bit = luaL_checkinteger( L, 2 );
	gclient_t *cl = &level.clients[client];

	if ( bit < 0 || bit >= ADM_NUM_CMDS ) {
		return luaL_error( L, "admin power %d does not exist (0 to %d, see /adminlist)", (int)bit, ADM_NUM_CMDS - 1 );
	}
	lua_pushboolean( L, cl->sess.amrpgmode != 0 && ( cl->pers.bitvalue & ( 1 << bit ) ) );
	return 1;
}

static const char *RP_LuaApi_TeamName( team_t team ) {
	switch ( team ) {
	case TEAM_RED:			return "red";
	case TEAM_BLUE:			return "blue";
	case TEAM_SPECTATOR:	return "spectator";
	default:				return "free";
	}
}

static int RP_LuaApi_PlayerIndex( lua_State *L ) {
	rpLuaPlayerHandle_t *h = (rpLuaPlayerHandle_t *)luaL_checkudata( L, 1, RP_LUA_PLAYER_MT );
	const char *key = luaL_checkstring( L, 2 );
	gclient_t *cl;

	if ( !strcmp( key, "valid" ) ) {
		lua_pushboolean( L, RP_LuaApi_PlayerValid( h ) );
		return 1;
	}
	if ( !RP_LuaApi_PlayerValid( h ) ) {
		return luaL_error( L, "that player has left the server" );
	}
	cl = &level.clients[h->client];

	if ( !strcmp( key, "id" ) ) {
		lua_pushinteger( L, h->client );
	} else if ( !strcmp( key, "name" ) ) {
		char name[MAX_NETNAME];
		Q_strncpyz( name, cl->pers.netname, sizeof( name ) );
		Q_StripColor( name );
		lua_pushstring( L, name );
	} else if ( !strcmp( key, "name_raw" ) ) {
		lua_pushstring( L, cl->pers.netname );
	} else if ( !strcmp( key, "team" ) ) {
		lua_pushstring( L, RP_LuaApi_TeamName( cl->sess.sessionTeam ) );
	} else if ( !strcmp( key, "is_bot" ) ) {
		lua_pushboolean( L, ( g_entities[h->client].r.svFlags & SVF_BOT ) ? 1 : 0 );
	} else if ( !strcmp( key, "logged_in" ) ) {
		lua_pushboolean( L, cl->sess.amrpgmode != 0 );
	} else if ( !strcmp( key, "rpg" ) ) {
		lua_pushboolean( L, cl->sess.amrpgmode == 2 );
	} else if ( !strcmp( key, "character_id" ) ) {
		if ( cl->sess.amrpgmode == 2 && cl->pers.CharID > 0 ) {
			lua_pushinteger( L, cl->pers.CharID );
		} else {
			lua_pushnil( L );
		}
	} else if ( !strcmp( key, "has_admin" ) ) {
		lua_pushcfunction( L, RP_LuaApi_PlayerHasAdmin );
	} else if ( !RP_LuaPeople_PlayerField( L, h->client, key ) ) {	// API 3
		lua_pushnil( L );
	}
	return 1;
}

static int RP_LuaApi_PlayerNewIndex( lua_State *L ) {
	return luaL_error( L, "player handles are read-only" );
}

static int RP_LuaApi_PlayerToString( lua_State *L ) {
	rpLuaPlayerHandle_t *h = (rpLuaPlayerHandle_t *)luaL_checkudata( L, 1, RP_LUA_PLAYER_MT );
	if ( RP_LuaApi_PlayerValid( h ) ) {
		char name[MAX_NETNAME];
		Q_strncpyz( name, level.clients[h->client].pers.netname, sizeof( name ) );
		Q_StripColor( name );
		lua_pushfstring( L, "Player(%d: %s)", h->client, name );
	} else {
		lua_pushfstring( L, "Player(%d: left)", h->client );
	}
	return 1;
}

/*
================
print, include, Plugin
================
*/
static int RP_LuaApi_Print( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int n = lua_gettop( L ), i;
	char clean[RP_LUA_TEXT_MAX + 1];
	luaL_Buffer b;

	luaL_buffinit( L, &b );
	for ( i = 1; i <= n; i++ ) {
		// GLX_U: the tab goes in before the next value is pushed. The buffer may only be added to
		// while its own box is on top of the stack: adding the tab with the value already pushed
		// corrupted memory or crashed the server once the text passed about 1 KB.
		if ( i > 1 ) {
			luaL_addchar( &b, '\t' );
		}
		luaL_tolstring( L, i, NULL );
		luaL_addvalue( &b );
	}
	luaL_pushresult( &b );
	RP_Lua_CleanText( clean, sizeof( clean ), lua_tostring( L, -1 ), qtrue );
	// GLX_U: a line of its own for every line, each tagged: a newline began a log line of the script's own
	RP_Lua_LogText( va( "Lua [%s]:", rl.plugins[plugin].name ), va( "Lua [%s]:", rl.plugins[plugin].name ), clean );
	return 0;
}

static int RP_LuaApi_Include( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	const char *file = luaL_checkstring( L, 1 );
	size_t len = strlen( file );
	int i;

	if ( plugin == RP_LUA_CONSOLE ) {
		return luaL_error( L, "include() is not available in lua_exec" );
	}
	if ( len < 5 || len >= MAX_QPATH || Q_stricmp( file + len - 4, ".lua" ) ) {
		return luaL_error( L, "include(\"%s\"): give a .lua file name from the plugin's own folder", file );
	}
	for ( i = 0; file[i]; i++ ) {
		char c = file[i];
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.' ) || ( c == '.' && file[i + 1] == '.' ) ) {
			return luaL_error( L, "include(\"%s\"): only letters, digits, _ - and . are allowed, and no folders", file );
		}
	}
	if ( !Q_stricmp( file, p->mainFile ) ) {
		return luaL_error( L, "include(\"%s\"): a plugin cannot include its own main file", file );
	}
	for ( i = 0; i < p->numIncludes; i++ ) {
		if ( !Q_stricmp( p->includes[i], file ) ) {
			return 0;	// once only
		}
	}
	if ( p->numIncludes >= RP_LUA_MAX_INCLUDES ) {
		return luaL_error( L, "a plugin may include at most %d files", RP_LUA_MAX_INCLUDES );
	}
	Q_strncpyz( p->includes[p->numIncludes++], file, sizeof( p->includes[0] ) );
	RP_Lua_PushChunk( L, plugin, p->dir, file );
	lua_call( L, 0, 0 );
	return 0;
}

static int RP_LuaApi_PluginRegister( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	rpLuaPlugin_t *p = &rl.plugins[plugin];

	luaL_checktype( L, 1, LUA_TTABLE );
	if ( lua_getfield( L, 1, "api" ) != LUA_TNIL ) {
		lua_Integer api = luaL_checkinteger( L, -1 );
		if ( api > RP_LUA_API_VERSION ) {
			return luaL_error( L, "this plugin needs Lua API version %d, but this server has version %d", (int)api, RP_LUA_API_VERSION );
		}
	}
	lua_pop( L, 1 );
	if ( lua_getfield( L, 1, "name" ) == LUA_TSTRING ) {
		RP_Lua_CleanText( p->title, sizeof( p->title ), lua_tostring( L, -1 ), qfalse );
	}
	lua_pop( L, 1 );
	if ( lua_getfield( L, 1, "version" ) != LUA_TNIL ) {
		const char *v = lua_tostring( L, -1 );
		RP_Lua_CleanText( p->version, sizeof( p->version ), v ? v : "", qfalse );
	}
	lua_pop( L, 1 );
	return 0;
}

static int RP_LuaApi_PluginName( lua_State *L ) {
	lua_pushstring( L, rl.plugins[RP_Lua_CurrentPlugin( L )].name );
	return 1;
}

/*
================
Events
================
*/
// one string field of a filter table at index 3: NULL when absent; too long or empty is an error
static const char *RP_LuaApi_FilterField( lua_State *L, const char *field ) {
	const char *value = NULL;

	if ( lua_getfield( L, 3, field ) != LUA_TNIL ) {
		if ( lua_type( L, -1 ) != LUA_TSTRING ) {
			luaL_error( L, "the %s filter must be a string", field );
			return NULL;
		}
		value = lua_tostring( L, -1 );	// stays valid: the table holds it
		if ( !value[0] || strlen( value ) >= RP_LUA_FILTER_SIZE ) {
			luaL_error( L, "the %s filter must be 1 to %d characters", field, RP_LUA_FILTER_SIZE - 1 );
			return NULL;
		}
	}
	lua_pop( L, 1 );
	return value;
}

// API 3: Events.on("npc_death", fn [, { npc_type =, spawner = }]) and Events.on("damage", fn [, { target = }])
static void RP_LuaApi_ActorFilter( lua_State *L, int ev, char *filter, char *filter2 ) {
	const char *value;

	if ( lua_isnoneornil( L, 3 ) ) {
		return;
	}
	luaL_checktype( L, 3, LUA_TTABLE );
	if ( ev == RPLE_NPC_DEATH ) {
		value = RP_LuaApi_FilterField( L, "npc_type" );
		if ( value ) {
			Q_strncpyz( filter, value, RP_LUA_FILTER_SIZE );
		}
		value = RP_LuaApi_FilterField( L, "spawner" );
		if ( value ) {
			Q_strncpyz( filter2, value, RP_LUA_FILTER_SIZE );
		}
		return;
	}
	value = RP_LuaApi_FilterField( L, "target" );
	if ( value ) {
		if ( strcmp( value, "players" ) && strcmp( value, "npcs" ) ) {
			luaL_error( L, "the damage event's target filter is \"players\" or \"npcs\"" );
			return;
		}
		Q_strncpyz( filter, value, RP_LUA_FILTER_SIZE );
	}
}

// Events.on("use", fn, { targetname = }) and Events.on("map_event", fn [, { name = }]): the filter
static void RP_LuaApi_EventFilter( lua_State *L, int ev, char *filter, char *filter2 ) {
	const char *field = ( ev == RPLE_USE ) ? "targetname" : "name";
	const char *value = NULL;

	filter[0] = '\0';
	filter2[0] = '\0';
	if ( ev == RPLE_NPC_DEATH || ev == RPLE_DAMAGE ) {
		RP_LuaApi_ActorFilter( L, ev, filter, filter2 );
		return;
	}
	if ( ev != RPLE_USE && ev != RPLE_MAP_EVENT ) {
		if ( !lua_isnoneornil( L, 3 ) ) {
			luaL_error( L, "only the use, map_event, npc_death and damage events take a filter" );
		}
		return;
	}
	if ( !lua_isnoneornil( L, 3 ) ) {
		luaL_checktype( L, 3, LUA_TTABLE );
		if ( lua_getfield( L, 3, field ) != LUA_TNIL ) {
			if ( lua_type( L, -1 ) != LUA_TSTRING ) {
				luaL_error( L, "the %s filter must be a string", field );
				return;
			}
			value = lua_tostring( L, -1 );
		}
	}
	if ( ev == RPLE_USE ) {
		if ( !value || !value[0] || strlen( value ) >= RP_LUA_FILTER_SIZE ) {
			luaL_error( L, "Events.on(\"use\", fn, { targetname = \"...\" }): the targetname (1 to %d characters) is required", RP_LUA_FILTER_SIZE - 1 );
			return;
		}
	} else if ( value && !RP_Lua_ValidEventName( value ) ) {
		luaL_error( L, "\"%s\" is not an event name (lower-case letters, digits and _, up to %d)", value, RP_LUA_EVENT_NAME_MAX );
		return;
	}
	if ( value ) {
		Q_strncpyz( filter, value, RP_LUA_FILTER_SIZE );
	}
	if ( !lua_isnoneornil( L, 3 ) ) {
		lua_pop( L, 1 );
	}
}

static int RP_LuaApi_EventsOn( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	const char *name = luaL_checkstring( L, 1 );
	char filter[RP_LUA_FILTER_SIZE], filter2[RP_LUA_FILTER_SIZE];
	int ev, i;

	luaL_checktype( L, 2, LUA_TFUNCTION );
	for ( ev = 0; ev < RPLE_NUM; ev++ ) {
		if ( !strcmp( name, rpLuaEventNames[ev] ) ) {
			break;
		}
	}
	if ( ev == RPLE_NUM ) {
		return luaL_error( L, "no event called \"%s\" (init, shutdown, unload, frame, client_connect, client_begin, client_spawn, client_disconnect, chat, command, use, map_event, player_death, player_downed, npc_death, damage, level_up, level_down)", name );
	}
	RP_LuaApi_EventFilter( L, ev, filter, filter2 );
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		if ( !rpHandlers[ev][i].active ) {
			break;
		}
	}
	if ( i == RP_LUA_MAX_HANDLERS ) {
		return luaL_error( L, "too many handlers for \"%s\" (at most %d for all plugins)", name, RP_LUA_MAX_HANDLERS );
	}
	lua_pushvalue( L, 2 );
	rpHandlers[ev][i].ref = luaL_ref( L, LUA_REGISTRYINDEX );
	rpHandlers[ev][i].plugin = plugin;
	rpHandlers[ev][i].id = rpNextId++;
	Q_strncpyz( rpHandlers[ev][i].filter, filter, sizeof( rpHandlers[ev][i].filter ) );
	Q_strncpyz( rpHandlers[ev][i].filter2, filter2, sizeof( rpHandlers[ev][i].filter2 ) );
	rpHandlers[ev][i].active = qtrue;
	lua_pushinteger( L, rpHandlers[ev][i].id );
	return 1;
}

static int RP_LuaApi_EventsOff( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	lua_Integer id = luaL_checkinteger( L, 1 );
	int ev, i;

	for ( ev = 0; ev < RPLE_NUM; ev++ ) {
		for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
			rpLuaHandler_t *h = &rpHandlers[ev][i];
			if ( h->active && h->id == id && h->plugin == plugin ) {
				h->active = qfalse;
				luaL_unref( L, LUA_REGISTRYINDEX, h->ref );
				lua_pushboolean( L, 1 );
				return 1;
			}
		}
	}
	lua_pushboolean( L, 0 );
	return 1;
}

typedef struct {
	rpLuaEvent_t	ev;
	int				slot;
	int				client;
	int				extra;
	const char		*text;
	int				target;
	int				result;		// chat: 0 when the handler returned false
} rpLuaEventCall_t;

static void RP_LuaApi_PushArgs( lua_State *L, int first ) {
	int n = trap->Argc(), i;
	char arg[MAX_STRING_CHARS];

	lua_createtable( L, n > first ? n - first : 0, 0 );
	for ( i = first; i < n; i++ ) {
		trap->Argv( i, arg, sizeof( arg ) );
		lua_pushstring( L, arg );
		lua_rawseti( L, -2, i - first + 1 );
	}
}

static const char *RP_LuaApi_ChatMode( int mode ) {
	switch ( mode ) {
	case SAY_OOC:	return "ooc";
	case SAY_TEAM:	return "team";
	case SAY_TELL:	return "tell";
	case SAY_ALLY:	return "ally";
	default:		return "say";
	}
}

static int RP_LuaApi_EventWorker( lua_State *L, void *data ) {
	rpLuaEventCall_t *c = (rpLuaEventCall_t *)data;
	int nargs = 0;

	lua_rawgeti( L, LUA_REGISTRYINDEX, rpHandlers[c->ev][c->slot].ref );
	switch ( c->ev ) {
	case RPLE_FRAME:
		lua_pushinteger( L, c->extra );
		nargs = 1;
		break;
	case RPLE_CLIENT_CONNECT:
		RP_LuaApi_PushPlayer( L, c->client );
		lua_pushboolean( L, c->extra );
		nargs = 2;
		break;
	case RPLE_CLIENT_BEGIN:
	case RPLE_CLIENT_SPAWN:
	case RPLE_CLIENT_DISCONNECT:
		RP_LuaApi_PushPlayer( L, c->client );
		nargs = 1;
		break;
	case RPLE_CHAT:
		RP_LuaApi_PushPlayer( L, c->client );
		lua_pushstring( L, RP_LuaApi_ChatMode( c->extra ) );
		lua_pushstring( L, c->text ? c->text : "" );
		if ( c->target >= 0 ) {
			RP_LuaApi_PushPlayer( L, c->target );
		} else {
			lua_pushnil( L );
		}
		nargs = 4;
		break;
	case RPLE_COMMAND:
		RP_LuaApi_PushPlayer( L, c->client );
		lua_pushstring( L, c->text ? c->text : "" );
		RP_LuaApi_PushArgs( L, 1 );
		nargs = 3;
		break;
	default:
		break;
	}
	lua_call( L, nargs, 1 );
	if ( c->ev == RPLE_CHAT && lua_isboolean( L, -1 ) && !lua_toboolean( L, -1 ) ) {
		c->result = 0;
	}
	return 0;
}

void RP_LuaApi_FireEvent( rpLuaEvent_t ev, int onlyPlugin, int client, int extra, const char *text, int target, int *result ) {
	rpLuaEventCall_t c;
	int i;

	if ( !rl.L ) {
		return;
	}
	c.ev = ev;
	c.client = client;
	c.extra = extra;
	c.text = text;
	c.target = target;
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		rpLuaHandler_t *h = &rpHandlers[ev][i];
		if ( !h->active || !RP_Lua_PluginRunning( h->plugin ) ) {
			continue;
		}
		if ( onlyPlugin >= 0 && h->plugin != onlyPlugin ) {
			continue;
		}
		if ( ev == RPLE_FRAME && !RP_Lua_FrameBudgetLeft() ) {
			RP_Lua_NoteDeferred( h->plugin, 1 );
			break;
		}
		c.slot = i;
		c.result = 1;
		RP_Lua_Call( h->plugin, RP_LuaApi_EventWorker, &c );
		if ( ev == RPLE_CHAT && !c.result ) {
			if ( result ) {
				*result = 0;
			}
			return;		// the first handler that returns false stops the message
		}
	}
}

void RP_LuaApi_FireFrame( int levelTime ) {
	RP_LuaApi_FireEvent( RPLE_FRAME, -1, -1, levelTime, NULL, -1, NULL );
}

/*
================
The use and map_event events (API 2): fired from game code through rp_lua_world.c
================
*/
typedef struct {
	int				ev;
	int				slot;
	gentity_t		*ent;
	gentity_t		*activator;
	gentity_t		*other;
	const char		*name;
	int				result;
} rpLuaEntEventCall_t;

static int RP_LuaApi_EntEventWorker( lua_State *L, void *data ) {
	rpLuaEntEventCall_t *c = (rpLuaEntEventCall_t *)data;

	lua_rawgeti( L, LUA_REGISTRYINDEX, rpHandlers[c->ev][c->slot].ref );
	if ( c->ev == RPLE_USE ) {
		RP_LuaWorld_PushEntity( L, c->ent );
		RP_LuaWorld_PushActor( L, c->activator );
		RP_LuaWorld_PushActor( L, c->other );
	} else {
		lua_pushstring( L, c->name );
		RP_LuaWorld_PushActor( L, c->activator );
		RP_LuaWorld_PushEntity( L, c->ent );
	}
	lua_call( L, 3, 1 );
	if ( lua_isboolean( L, -1 ) && !lua_toboolean( L, -1 ) ) {
		c->result = 0;
	}
	return 0;
}

// qfalse when a handler returned false (and it was not forced): the use does not happen
qboolean RP_LuaApi_FireUse( gentity_t *self, gentity_t *other, gentity_t *activator, qboolean forced ) {
	rpLuaEntEventCall_t c;
	int i;

	if ( !rl.L || !self->targetname || !self->targetname[0] ) {
		return qtrue;
	}
	c.ev = RPLE_USE;
	c.ent = self;
	c.activator = activator;
	c.other = other;
	c.name = NULL;
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		rpLuaHandler_t *h = &rpHandlers[RPLE_USE][i];
		if ( !h->active || !RP_Lua_PluginRunning( h->plugin ) || Q_stricmp( h->filter, self->targetname ) ) {
			continue;
		}
		c.slot = i;
		c.result = 1;
		RP_Lua_Call( h->plugin, RP_LuaApi_EntEventWorker, &c );
		if ( !c.result && !forced ) {
			return qfalse;		// the first handler that returns false stops the use
		}
		if ( !self->inuse ) {
			return qfalse;		// a handler removed it (through something it used)
		}
	}
	return qtrue;
}

void RP_LuaApi_FireMapEvent( const char *name, gentity_t *activator, gentity_t *ent ) {
	rpLuaEntEventCall_t c;
	int i;

	if ( !rl.L ) {
		return;
	}
	c.ev = RPLE_MAP_EVENT;
	c.ent = ent;
	c.activator = activator;
	c.other = NULL;
	c.name = name;
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		rpLuaHandler_t *h = &rpHandlers[RPLE_MAP_EVENT][i];
		if ( !h->active || !RP_Lua_PluginRunning( h->plugin ) || ( h->filter[0] && strcmp( h->filter, name ) ) ) {
			continue;
		}
		c.slot = i;
		c.result = 1;
		RP_Lua_Call( h->plugin, RP_LuaApi_EntEventWorker, &c );
	}
}

/*
================
The player_death, player_downed, npc_death and damage events (API 3): fired from game code through
rp_lua_people.c, which keeps free() and kill() waiting until the game code is done
================
*/
qboolean RP_LuaApi_HasHandlers( rpLuaEvent_t ev ) {
	int i;

	if ( !rl.L ) {
		return qfalse;
	}
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		if ( rpHandlers[ev][i].active && RP_Lua_PluginRunning( rpHandlers[ev][i].plugin ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

typedef struct {
	rpLuaEvent_t	ev;
	int				slot;
	gentity_t		*ent;		// the player or NPC
	gentity_t		*other;		// the killer or attacker, NULL for none
	int				amount;
	int				mod;
} rpLuaActorEventCall_t;

static int RP_LuaApi_ActorEventWorker( lua_State *L, void *data ) {
	rpLuaActorEventCall_t *c = (rpLuaActorEventCall_t *)data;
	int nargs;

	lua_rawgeti( L, LUA_REGISTRYINDEX, rpHandlers[c->ev][c->slot].ref );
	RP_LuaWorld_PushActor( L, c->ent );
	RP_LuaWorld_PushActor( L, c->other );
	switch ( c->ev ) {
	case RPLE_PLAYER_DOWNED:
		nargs = 2;
		break;
	case RPLE_DAMAGE:
		lua_pushinteger( L, c->amount );
		lua_pushstring( L, RP_LuaPeople_ModName( c->mod ) );
		nargs = 4;
		break;
	default:	// player_death, npc_death
		lua_pushstring( L, RP_LuaPeople_ModName( c->mod ) );
		nargs = 3;
		break;
	}
	lua_call( L, nargs, 0 );	// what the handler returns changes nothing
	return 0;
}

// an npc_death handler's filter: the NPC's type, and the spawner that made it (while that spawner exists)
static qboolean RP_LuaApi_NpcDeathMatches( const rpLuaHandler_t *h, const gentity_t *npc ) {
	if ( h->filter[0] && ( !npc->NPC_type || Q_stricmp( h->filter, npc->NPC_type ) ) ) {
		return qfalse;
	}
	if ( h->filter2[0] ) {
		const gentity_t *sp = npc->zyk_npc_spawner;
		if ( !RP_SpawnerMadeIt( sp, npc ) || !sp->targetname || Q_stricmp( h->filter2, sp->targetname ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

void RP_LuaApi_FireActorEvent( rpLuaEvent_t ev, gentity_t *ent, gentity_t *other, int amount, int mod ) {
	rpLuaActorEventCall_t c;
	int i;

	if ( !rl.L || !ent ) {
		return;
	}
	c.ev = ev;
	c.ent = ent;
	c.other = other;
	c.amount = amount;
	c.mod = mod;
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		rpLuaHandler_t *h = &rpHandlers[ev][i];
		if ( !h->active || !RP_Lua_PluginRunning( h->plugin ) || !ent->inuse ) {
			continue;
		}
		if ( ev == RPLE_NPC_DEATH && !RP_LuaApi_NpcDeathMatches( h, ent ) ) {
			continue;
		}
		if ( ev == RPLE_DAMAGE && h->filter[0] && strcmp( h->filter, ( ent->s.number < MAX_CLIENTS ) ? "players" : "npcs" ) ) {
			continue;
		}
		c.slot = i;
		RP_Lua_Call( h->plugin, RP_LuaApi_ActorEventWorker, &c );
	}
}

/*
================
The level_up and level_down events (API 4): fired from increase_level() / decrease_level() through
rp_lua_people.c
================
*/
typedef struct {
	rpLuaEvent_t	ev;
	int				slot;
	int				client;
	int				level;
	int				count;
} rpLuaLevelEventCall_t;

static int RP_LuaApi_LevelEventWorker( lua_State *L, void *data ) {
	rpLuaLevelEventCall_t *c = (rpLuaLevelEventCall_t *)data;

	lua_rawgeti( L, LUA_REGISTRYINDEX, rpHandlers[c->ev][c->slot].ref );
	RP_LuaApi_PushPlayer( L, c->client );
	lua_pushinteger( L, c->level );
	lua_pushinteger( L, c->count );
	lua_call( L, 3, 0 );	// what the handler returns changes nothing
	return 0;
}

void RP_LuaApi_FireLevelEvent( rpLuaEvent_t ev, int client, int level, int count ) {
	rpLuaLevelEventCall_t c;
	int i;

	if ( !rl.L ) {
		return;
	}
	c.ev = ev;
	c.client = client;
	c.level = level;
	c.count = count;
	for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
		rpLuaHandler_t *h = &rpHandlers[ev][i];
		if ( !h->active || !RP_Lua_PluginRunning( h->plugin ) ) {
			continue;
		}
		c.slot = i;
		RP_Lua_Call( h->plugin, RP_LuaApi_LevelEventWorker, &c );
	}
}

/*
================
Commands
================
*/
static qboolean RP_LuaApi_ValidCommandName( const char *name ) {
	int i;
	for ( i = 0; name[i]; i++ ) {
		if ( i >= RP_LUA_CMD_NAME_SIZE - 1 ) {
			return qfalse;
		}
		if ( !( ( name[i] >= 'a' && name[i] <= 'z' ) || ( name[i] >= '0' && name[i] <= '9' ) || name[i] == '_' ) ) {
			return qfalse;
		}
	}
	return ( i >= 2 ) ? qtrue : qfalse;
}

static int RP_LuaApi_CommandsRegister( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	const char *name = luaL_checkstring( L, 1 );
	char help[RP_LUA_CMD_HELP_SIZE] = "";
	int adminBit = -1, i, slot = -1;
	qboolean loggedIn = qfalse;

	luaL_checktype( L, 2, LUA_TFUNCTION );
	if ( !RP_LuaApi_ValidCommandName( name ) ) {
		return luaL_error( L, "\"%s\" is not a usable command name (2 to %d lower-case letters, digits or _)", name, RP_LUA_CMD_NAME_SIZE - 1 );
	}
	if ( RP_LuaApi_ReservedCommand( name ) ) {
		return luaL_error( L, "/%s is already a built-in command; script commands cannot replace one", name );
	}
	for ( i = 0; i < RP_LUA_MAX_COMMANDS; i++ ) {
		if ( rpCommands[i].active && !strcmp( rpCommands[i].name, name ) ) {
			return luaL_error( L, "/%s is already registered by plugin %s", name, rl.plugins[rpCommands[i].plugin].name );
		}
		if ( !rpCommands[i].active && slot < 0 ) {
			slot = i;
		}
	}
	if ( slot < 0 ) {
		return luaL_error( L, "too many script commands (at most %d for all plugins)", RP_LUA_MAX_COMMANDS );
	}
	if ( !lua_isnoneornil( L, 3 ) ) {
		luaL_checktype( L, 3, LUA_TTABLE );
		if ( lua_getfield( L, 3, "help" ) == LUA_TSTRING ) {
			RP_Lua_CleanText( help, sizeof( help ), lua_tostring( L, -1 ), qfalse );
		}
		lua_pop( L, 1 );
		if ( lua_getfield( L, 3, "admin" ) != LUA_TNIL ) {
			lua_Integer bit = luaL_checkinteger( L, -1 );
			if ( bit < 0 || bit >= ADM_NUM_CMDS ) {
				return luaL_error( L, "admin power %d does not exist (0 to %d, see /adminlist)", (int)bit, ADM_NUM_CMDS - 1 );
			}
			adminBit = (int)bit;
		}
		lua_pop( L, 1 );
		lua_getfield( L, 3, "logged_in" );
		loggedIn = lua_toboolean( L, -1 ) ? qtrue : qfalse;
		lua_pop( L, 1 );
	}

	lua_pushvalue( L, 2 );
	rpCommands[slot].ref = luaL_ref( L, LUA_REGISTRYINDEX );
	rpCommands[slot].plugin = plugin;
	Q_strncpyz( rpCommands[slot].name, name, sizeof( rpCommands[slot].name ) );
	Q_strncpyz( rpCommands[slot].help, help, sizeof( rpCommands[slot].help ) );
	rpCommands[slot].adminBit = adminBit;
	rpCommands[slot].loggedIn = loggedIn;
	rpCommands[slot].active = qtrue;
	return 0;
}

typedef struct {
	int			slot;
	int			client;
} rpLuaCommandCall_t;

static int RP_LuaApi_CommandWorker( lua_State *L, void *data ) {
	rpLuaCommandCall_t *c = (rpLuaCommandCall_t *)data;

	lua_rawgeti( L, LUA_REGISTRYINDEX, rpCommands[c->slot].ref );
	RP_LuaApi_PushPlayer( L, c->client );
	RP_LuaApi_PushArgs( L, 1 );
	lua_call( L, 2, 0 );
	return 0;
}

qboolean RP_LuaApi_RunCommand( gentity_t *ent, const char *cmd ) {
	rpLuaCommandCall_t c;
	char name[RP_LUA_CMD_NAME_SIZE];
	int i;

	if ( strlen( cmd ) >= sizeof( name ) ) {
		return qfalse;
	}
	Q_strncpyz( name, cmd, sizeof( name ) );
	Q_strlwr( name );
	for ( i = 0; i < RP_LUA_MAX_COMMANDS; i++ ) {
		if ( rpCommands[i].active && RP_Lua_PluginRunning( rpCommands[i].plugin ) && !strcmp( rpCommands[i].name, name ) ) {
			break;
		}
	}
	if ( i == RP_LUA_MAX_COMMANDS ) {
		return qfalse;
	}
	if ( rpCommands[i].loggedIn && ent->client->sess.amrpgmode == 0 ) {
		trap->SendServerCommand( ent->s.number, "print \"You must be logged in\n\"" );
		return qtrue;
	}
	if ( rpCommands[i].adminBit >= 0 && !check_admin_command( ent, rpCommands[i].adminBit, qtrue ) ) {
		return qtrue;
	}
	c.slot = i;
	c.client = ent->s.number;
	RP_Lua_Call( rpCommands[i].plugin, RP_LuaApi_CommandWorker, &c );
	RP_LuaApi_FireEvent( RPLE_COMMAND, -1, ent->s.number, 0, name, -1, NULL );
	return qtrue;
}

int RP_LuaApi_CommandCount( void ) {
	return RP_LUA_MAX_COMMANDS;
}

qboolean RP_LuaApi_CommandInfo( int index, char *name, int nameSize, char *help, int helpSize, int *plugin ) {
	if ( index < 0 || index >= RP_LUA_MAX_COMMANDS || !rpCommands[index].active || !RP_Lua_PluginRunning( rpCommands[index].plugin ) ) {
		return qfalse;
	}
	Q_strncpyz( name, rpCommands[index].name, nameSize );
	Q_strncpyz( help, rpCommands[index].help, helpSize );
	*plugin = rpCommands[index].plugin;
	return qtrue;
}

/*
================
Timers
================
*/
static int RP_LuaApi_AddTimer( lua_State *L, qboolean repeat ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	lua_Integer ms = luaL_checkinteger( L, 1 );
	int i;

	luaL_checktype( L, 2, LUA_TFUNCTION );
	if ( repeat && ms < RP_LUA_TIMER_MIN_MS ) {
		return luaL_error( L, "Timer.every needs at least %d ms", RP_LUA_TIMER_MIN_MS );
	}
	if ( ms < 0 ) {
		ms = 0;
	}
	if ( ms > 0x7FFFFFF ) {
		return luaL_error( L, "a timer can be at most %d ms away", 0x7FFFFFF );
	}
	if ( rl.plugins[plugin].numTimers >= RP_LUA_PLUGIN_TIMERS ) {
		return luaL_error( L, "a plugin may have at most %d timers", RP_LUA_PLUGIN_TIMERS );
	}
	for ( i = 0; i < RP_LUA_MAX_TIMERS; i++ ) {
		if ( !rpTimers[i].active ) {
			break;
		}
	}
	if ( i == RP_LUA_MAX_TIMERS ) {
		return luaL_error( L, "too many timers (at most %d for all plugins)", RP_LUA_MAX_TIMERS );
	}
	lua_pushvalue( L, 2 );
	rpTimers[i].ref = luaL_ref( L, LUA_REGISTRYINDEX );
	rpTimers[i].plugin = plugin;
	rpTimers[i].id = rpNextId++;
	rpTimers[i].due = level.time + (int)ms;
	rpTimers[i].interval = repeat ? (int)ms : 0;
	rpTimers[i].active = qtrue;
	rl.plugins[plugin].numTimers++;
	lua_pushinteger( L, rpTimers[i].id );
	return 1;
}

static int RP_LuaApi_TimerAfter( lua_State *L ) {
	return RP_LuaApi_AddTimer( L, qfalse );
}

static int RP_LuaApi_TimerEvery( lua_State *L ) {
	return RP_LuaApi_AddTimer( L, qtrue );
}

static void RP_LuaApi_DropTimer( rpLuaTimer_t *t ) {
	if ( !t->active ) {
		return;
	}
	t->active = qfalse;
	if ( rl.L ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, t->ref );
	}
	if ( rl.plugins[t->plugin].numTimers > 0 ) {
		rl.plugins[t->plugin].numTimers--;
	}
}

static int RP_LuaApi_TimerCancel( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	lua_Integer id = luaL_checkinteger( L, 1 );
	int i;

	for ( i = 0; i < RP_LUA_MAX_TIMERS; i++ ) {
		if ( rpTimers[i].active && rpTimers[i].id == id && rpTimers[i].plugin == plugin ) {
			RP_LuaApi_DropTimer( &rpTimers[i] );
			lua_pushboolean( L, 1 );
			return 1;
		}
	}
	lua_pushboolean( L, 0 );
	return 1;
}

static int RP_LuaApi_TimerWorker( lua_State *L, void *data ) {
	int ref = *(int *)data;
	lua_rawgeti( L, LUA_REGISTRYINDEX, ref );
	lua_call( L, 0, 0 );
	return 0;
}

/*
================
Tasks: coroutines resumed from the frame. Task.wait yields; a plain coroutine.yield from the task
body waits one frame.
================
*/
static int RP_LuaApi_TaskStart( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int n = lua_gettop( L ), i;
	lua_State *co;

	luaL_checktype( L, 1, LUA_TFUNCTION );
	if ( rl.plugins[plugin].numTasks >= RP_LUA_PLUGIN_TASKS ) {
		return luaL_error( L, "a plugin may have at most %d tasks", RP_LUA_PLUGIN_TASKS );
	}
	for ( i = 0; i < RP_LUA_MAX_TASKS; i++ ) {
		if ( !rpTasks[i].active ) {
			break;
		}
	}
	if ( i == RP_LUA_MAX_TASKS ) {
		return luaL_error( L, "too many tasks (at most %d for all plugins)", RP_LUA_MAX_TASKS );
	}
	co = lua_newthread( L );
	lua_insert( L, 1 );					// thread, fn, args...
	lua_xmove( L, co, n );				// fn and args onto the new thread
	memset( &rpTasks[i], 0, sizeof( rpTasks[i] ) );
	rpTasks[i].threadRef = luaL_ref( L, LUA_REGISTRYINDEX );	// pops the thread
	rpTasks[i].thread = co;
	rpTasks[i].plugin = plugin;
	rpTasks[i].id = rpNextId++;
	rpTasks[i].wake = level.time;		// starts on the next server frame
	rpTasks[i].nargs = n - 1;
	rpTasks[i].active = qtrue;
	rl.plugins[plugin].numTasks++;
	lua_pushinteger( L, rpTasks[i].id );
	return 1;
}

static int RP_LuaApi_TaskWait( lua_State *L ) {
	lua_Integer ms = luaL_optinteger( L, 1, 0 );
	int i;

	for ( i = 0; i < RP_LUA_MAX_TASKS; i++ ) {
		if ( rpTasks[i].active && rpTasks[i].running && rpTasks[i].thread == L ) {
			break;
		}
	}
	if ( i == RP_LUA_MAX_TASKS ) {
		return luaL_error( L, "Task.wait only works inside a function started with Task.start" );
	}
	if ( ms < 0 ) {
		ms = 0;
	}
	if ( ms > 0x7FFFFFF ) {
		ms = 0x7FFFFFF;
	}
	rpTasks[i].wake = level.time + (int)ms;
	rpTasks[i].waitSet = qtrue;
	return lua_yield( L, 0 );
}

static void RP_LuaApi_FinishTask( rpLuaTask_t *t ) {
	if ( !t->active ) {
		return;
	}
	t->active = qfalse;
	t->running = qfalse;
	if ( rl.L ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, t->threadRef );
	}
	t->thread = NULL;
	if ( rl.plugins[t->plugin].numTasks > 0 ) {
		rl.plugins[t->plugin].numTasks--;
	}
}

static int RP_LuaApi_TaskCancel( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	lua_Integer id = luaL_checkinteger( L, 1 );
	int i;

	for ( i = 0; i < RP_LUA_MAX_TASKS; i++ ) {
		rpLuaTask_t *t = &rpTasks[i];
		if ( t->active && t->id == id && t->plugin == plugin ) {
			if ( t->running ) {
				t->cancelled = qtrue;	// finished as soon as it stops running
			} else {
				RP_LuaApi_FinishTask( t );
			}
			lua_pushboolean( L, 1 );
			return 1;
		}
	}
	lua_pushboolean( L, 0 );
	return 1;
}

static int RP_LuaApi_TaskWorker( lua_State *L, void *data ) {
	rpLuaTask_t *t = &rpTasks[*(int *)data];
	lua_State *co = t->thread;
	int nres = 0, status, nargs;

	lua_rawgeti( L, LUA_REGISTRYINDEX, t->threadRef );	// keeps the thread alive while it runs
	nargs = t->started ? 0 : t->nargs;
	t->started = qtrue;
	t->waitSet = qfalse;
	t->running = qtrue;
	status = lua_resume( co, L, nargs, &nres );
	t->running = qfalse;

	if ( status == LUA_YIELD ) {
		lua_pop( co, nres );
		if ( !t->waitSet ) {
			t->wake = level.time;	// a plain yield: next frame
		}
		if ( t->cancelled ) {
			RP_LuaApi_FinishTask( t );
		}
		return 0;
	}
	if ( status == LUA_OK ) {
		RP_LuaApi_FinishTask( t );
		return 0;
	}
	{
		const char *msg = lua_tostring( co, -1 );
		if ( !msg ) {
			msg = lua_pushfstring( L, "(error object is a %s value)", luaL_typename( co, -1 ) );
		}
		luaL_traceback( L, co, msg, 0 );
	}
	RP_LuaApi_FinishTask( t );
	return lua_error( L );
}

/*
================
Running timers and tasks: due ones in order, inside the frame budget
================
*/
typedef struct {
	int		index;
	int		due;
	int		id;
	int		isTask;
} rpLuaDue_t;

static int RP_LuaApi_DueCompare( const void *a, const void *b ) {
	const rpLuaDue_t *x = (const rpLuaDue_t *)a, *y = (const rpLuaDue_t *)b;
	if ( x->due != y->due ) {
		return x->due < y->due ? -1 : 1;
	}
	return x->id < y->id ? -1 : ( x->id > y->id ? 1 : 0 );
}

void RP_LuaApi_RunTimersAndTasks( int levelTime ) {
	static rpLuaDue_t due[RP_LUA_MAX_TIMERS + RP_LUA_MAX_TASKS];
	int n = 0, i;

	for ( i = 0; i < RP_LUA_MAX_TIMERS; i++ ) {
		rpLuaTimer_t *t = &rpTimers[i];
		if ( t->active && t->due <= levelTime && RP_Lua_PluginRunning( t->plugin ) ) {
			due[n].index = i;
			due[n].due = t->due;
			due[n].id = t->id;
			due[n].isTask = 0;
			n++;
		}
	}
	for ( i = 0; i < RP_LUA_MAX_TASKS; i++ ) {
		rpLuaTask_t *t = &rpTasks[i];
		if ( t->active && !t->running && t->wake <= levelTime && RP_Lua_PluginRunning( t->plugin ) ) {
			due[n].index = i;
			due[n].due = t->wake;
			due[n].id = t->id;
			due[n].isTask = 1;
			n++;
		}
	}
	if ( !n ) {
		return;
	}
	qsort( due, n, sizeof( due[0] ), RP_LuaApi_DueCompare );

	for ( i = 0; i < n; i++ ) {
		if ( !RP_Lua_FrameBudgetLeft() ) {
			int j;
			for ( j = i; j < n; j++ ) {
				int plugin = due[j].isTask ? rpTasks[due[j].index].plugin : rpTimers[due[j].index].plugin;
				RP_Lua_NoteDeferred( plugin, 1 );
			}
			return;
		}
		if ( due[i].isTask ) {
			rpLuaTask_t *t = &rpTasks[due[i].index];
			int index = due[i].index;
			if ( !t->active || t->id != due[i].id || !RP_Lua_PluginRunning( t->plugin ) ) {
				continue;
			}
			RP_Lua_Call( t->plugin, RP_LuaApi_TaskWorker, &index );
		} else {
			rpLuaTimer_t *t = &rpTimers[due[i].index];
			int ref, plugin;
			if ( !t->active || t->id != due[i].id || !RP_Lua_PluginRunning( t->plugin ) ) {
				continue;
			}
			ref = t->ref;
			plugin = t->plugin;
			if ( t->interval > 0 ) {
				t->due += t->interval;
				if ( t->due <= levelTime ) {
					t->due = levelTime + t->interval;	// fell behind: no burst of catch-up calls
				}
				RP_Lua_Call( plugin, RP_LuaApi_TimerWorker, &ref );
			} else {
				// a one-shot timer is spent before it runs; its function stays referenced until then
				t->active = qfalse;
				if ( rl.plugins[plugin].numTimers > 0 ) {
					rl.plugins[plugin].numTimers--;
				}
				RP_Lua_Call( plugin, RP_LuaApi_TimerWorker, &ref );
				if ( rl.L ) {
					luaL_unref( rl.L, LUA_REGISTRYINDEX, ref );
				}
			}
		}
	}
}

/*
================
Msg
================
*/
static int RP_LuaApi_MsgSend( lua_State *L, const char *fmt, qboolean toPlayer, qboolean keepNewlines ) {
	char clean[RP_LUA_TEXT_MAX + 1];
	int client = -1;
	const char *text;

	if ( toPlayer ) {
		client = RP_LuaApi_CheckPlayer( L, 1 );
		text = luaL_checkstring( L, 2 );
	} else {
		text = luaL_checkstring( L, 1 );
	}
	RP_Lua_CleanText( clean, sizeof( clean ), text, keepNewlines );
	trap->SendServerCommand( client, va( fmt, clean ) );
	return 0;
}

static int RP_LuaApi_MsgPrint( lua_State *L ) {
	return RP_LuaApi_MsgSend( L, "print \"%s\n\"", qtrue, qtrue );
}
static int RP_LuaApi_MsgCenter( lua_State *L ) {
	return RP_LuaApi_MsgSend( L, "cp \"%s\"", qtrue, qtrue );
}
static int RP_LuaApi_MsgChat( lua_State *L ) {
	return RP_LuaApi_MsgSend( L, "chat \"%s\"", qtrue, qfalse );
}
static int RP_LuaApi_MsgBroadcast( lua_State *L ) {
	return RP_LuaApi_MsgSend( L, "chat \"%s\"", qfalse, qfalse );
}
static int RP_LuaApi_MsgBroadcastCenter( lua_State *L ) {
	return RP_LuaApi_MsgSend( L, "cp \"%s\"", qfalse, qtrue );
}

/*
================
Server
================
*/
static int RP_LuaApi_ServerTime( lua_State *L ) {
	lua_pushinteger( L, level.time );
	return 1;
}

static int RP_LuaApi_ServerRealtime( lua_State *L ) {
	lua_pushinteger( L, (lua_Integer)time( NULL ) );
	return 1;
}

static int RP_LuaApi_ServerDate( lua_State *L ) {
	static const char *kinds[] = { "date", "time", "datetime", "weekday", "hour", NULL };
	int kind = luaL_checkoption( L, 1, "datetime", kinds );
	time_t now = time( NULL );
	struct tm *tm = localtime( &now );
	char buf[64];

	if ( !tm ) {
		return luaL_error( L, "the server clock is not available" );
	}
	switch ( kind ) {
	case 0:
		strftime( buf, sizeof( buf ), "%Y-%m-%d", tm );
		break;
	case 1:
		strftime( buf, sizeof( buf ), "%H:%M:%S", tm );
		break;
	case 3:
		lua_pushinteger( L, tm->tm_wday == 0 ? 7 : tm->tm_wday );	// Monday 1 .. Sunday 7
		return 1;
	case 4:
		lua_pushinteger( L, tm->tm_hour );
		return 1;
	default:
		strftime( buf, sizeof( buf ), "%Y-%m-%d %H:%M:%S", tm );
		break;
	}
	lua_pushstring( L, buf );
	return 1;
}

static int RP_LuaApi_ServerMap( lua_State *L ) {
	char mapname[MAX_QPATH];
	trap->Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
	lua_pushstring( L, mapname );
	return 1;
}

static const char *rpLuaReadableCvars[] = {
	"sv_hostname", "g_gametype", "mapname", "timelimit", "fraglimit", "sv_maxclients", "g_motd", NULL
};

static int RP_LuaApi_ServerCvar( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	char value[MAX_CVAR_VALUE_STRING];
	qboolean ok = !Q_stricmpn( name, "rp_", 3 ) ? qtrue : qfalse;
	int i;

	for ( i = 0; !ok && rpLuaReadableCvars[i]; i++ ) {
		if ( !Q_stricmp( name, rpLuaReadableCvars[i] ) ) {
			ok = qtrue;
		}
	}
	if ( !ok ) {
		return luaL_error( L, "scripts cannot read %s (sv_hostname, g_gametype, mapname, timelimit, fraglimit, sv_maxclients, g_motd and rp_* only)", name );
	}
	trap->Cvar_VariableStringBuffer( name, value, sizeof( value ) );
	lua_pushstring( L, value );
	return 1;
}

static int RP_LuaApi_ServerPlayers( lua_State *L ) {
	int i, n = 0;

	lua_newtable( L );
	for ( i = 0; i < level.maxclients && i < MAX_CLIENTS; i++ ) {
		if ( level.clients[i].pers.connected != CON_CONNECTED ) {
			continue;
		}
		RP_LuaApi_PushPlayer( L, i );
		lua_rawseti( L, -2, ++n );
	}
	return 1;
}

/*
================
Registration and cleanup
================
*/
static void RP_LuaApi_SetFuncs( lua_State *L, int g, const char *table, const luaL_Reg *funcs ) {
	lua_newtable( L );
	luaL_setfuncs( L, funcs, 0 );
	lua_setfield( L, g, table );
}

void RP_LuaApi_Register( lua_State *L ) {
	static const luaL_Reg pluginFuncs[] = { { "register", RP_LuaApi_PluginRegister }, { "name", RP_LuaApi_PluginName }, { NULL, NULL } };
	static const luaL_Reg eventFuncs[] = { { "on", RP_LuaApi_EventsOn }, { "off", RP_LuaApi_EventsOff }, { NULL, NULL } };
	static const luaL_Reg timerFuncs[] = { { "after", RP_LuaApi_TimerAfter }, { "every", RP_LuaApi_TimerEvery }, { "cancel", RP_LuaApi_TimerCancel }, { NULL, NULL } };
	static const luaL_Reg taskFuncs[] = { { "start", RP_LuaApi_TaskStart }, { "wait", RP_LuaApi_TaskWait }, { "cancel", RP_LuaApi_TaskCancel }, { NULL, NULL } };
	static const luaL_Reg commandFuncs[] = { { "register", RP_LuaApi_CommandsRegister }, { NULL, NULL } };
	static const luaL_Reg msgFuncs[] = {
		{ "print", RP_LuaApi_MsgPrint }, { "center", RP_LuaApi_MsgCenter }, { "chat", RP_LuaApi_MsgChat },
		{ "broadcast", RP_LuaApi_MsgBroadcast }, { "broadcast_center", RP_LuaApi_MsgBroadcastCenter }, { NULL, NULL } };
	static const luaL_Reg serverFuncs[] = {
		{ "time", RP_LuaApi_ServerTime }, { "realtime", RP_LuaApi_ServerRealtime }, { "date", RP_LuaApi_ServerDate },
		{ "map", RP_LuaApi_ServerMap }, { "cvar", RP_LuaApi_ServerCvar }, { "players", RP_LuaApi_ServerPlayers }, { NULL, NULL } };
	static const luaL_Reg playerMeta[] = {
		{ "__index", RP_LuaApi_PlayerIndex }, { "__newindex", RP_LuaApi_PlayerNewIndex }, { "__tostring", RP_LuaApi_PlayerToString }, { NULL, NULL } };
	int g = lua_absindex( L, -1 );

	lua_pushcfunction( L, RP_LuaApi_Print );
	lua_setfield( L, g, "print" );
	lua_pushcfunction( L, RP_LuaApi_Include );
	lua_setfield( L, g, "include" );
	RP_LuaApi_SetFuncs( L, g, "Plugin", pluginFuncs );
	RP_LuaApi_SetFuncs( L, g, "Events", eventFuncs );
	RP_LuaApi_SetFuncs( L, g, "Timer", timerFuncs );
	RP_LuaApi_SetFuncs( L, g, "Task", taskFuncs );
	RP_LuaApi_SetFuncs( L, g, "Commands", commandFuncs );
	RP_LuaApi_SetFuncs( L, g, "Msg", msgFuncs );
	RP_LuaApi_SetFuncs( L, g, "Server", serverFuncs );

	luaL_newmetatable( L, RP_LUA_PLAYER_MT );
	luaL_setfuncs( L, playerMeta, 0 );
	lua_pushboolean( L, 0 );
	lua_setfield( L, -2, "__metatable" );
	lua_pop( L, 1 );
}

void RP_LuaApi_RemovePlugin( int plugin ) {
	lua_State *L = rl.L;
	int ev, i;

	RP_LuaWorld_RemovePlugin( plugin );		// API 2: its entities and zones (API 3: and NPCs)
	RP_LuaPeople_RemovePlugin( plugin );	// API 3: the player effects it set that are still its own
	RP_LuaDialog_RemovePlugin( plugin );	// API 5: its dialogues close
	RP_LuaOrders_RemovePlugin( plugin );	// API 6: its NPC orders are forgotten

	for ( ev = 0; ev < RPLE_NUM; ev++ ) {
		for ( i = 0; i < RP_LUA_MAX_HANDLERS; i++ ) {
			rpLuaHandler_t *h = &rpHandlers[ev][i];
			if ( h->active && h->plugin == plugin ) {
				h->active = qfalse;
				if ( L ) {
					luaL_unref( L, LUA_REGISTRYINDEX, h->ref );
				}
			}
		}
	}
	for ( i = 0; i < RP_LUA_MAX_COMMANDS; i++ ) {
		if ( rpCommands[i].active && rpCommands[i].plugin == plugin ) {
			rpCommands[i].active = qfalse;
			if ( L ) {
				luaL_unref( L, LUA_REGISTRYINDEX, rpCommands[i].ref );
			}
		}
	}
	for ( i = 0; i < RP_LUA_MAX_TIMERS; i++ ) {
		if ( rpTimers[i].active && rpTimers[i].plugin == plugin ) {
			RP_LuaApi_DropTimer( &rpTimers[i] );
		}
	}
	for ( i = 0; i < RP_LUA_MAX_TASKS; i++ ) {
		rpLuaTask_t *t = &rpTasks[i];
		if ( t->active && t->plugin == plugin ) {
			if ( t->running ) {
				t->cancelled = qtrue;
			} else {
				RP_LuaApi_FinishTask( t );
			}
		}
	}
}

void RP_LuaApi_Reset( void ) {
	memset( rpHandlers, 0, sizeof( rpHandlers ) );
	memset( rpCommands, 0, sizeof( rpCommands ) );
	memset( rpTimers, 0, sizeof( rpTimers ) );
	memset( rpTasks, 0, sizeof( rpTasks ) );
	rpNextId = 1;
}
