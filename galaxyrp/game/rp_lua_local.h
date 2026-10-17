/*
===========================================================================
GLX_U: [Lua] what the rp_lua*.c files share. Nothing outside them includes this; the rest of the
game module uses rp_lua.h.
===========================================================================
*/
#ifndef RP_LUA_LOCAL_H
#define RP_LUA_LOCAL_H

#include "../../codemp/game/g_local.h"
#include "../../lib/lua/lua.h"
#include "../../lib/lua/lauxlib.h"
#include "../../lib/lua/lualib.h"
#include "rp_lua.h"

#define RP_LUA_API_VERSION			3		// Plugin.register{ api = n } may not ask for more

#define RP_LUA_MAX_PLUGINS			64		// plugins and map scripts together
#define RP_LUA_CONSOLE				RP_LUA_MAX_PLUGINS	// plugins[] slot of rcon lua_exec's "@console"
#define RP_LUA_NAME_SIZE			48		// "welcome", or "map:castle_doors.lua"
#define RP_LUA_PLUGIN_NAME_MAX		32		// a plugin folder name: [a-z0-9_], 1..32
#define RP_LUA_MAX_FILE				( 256 * 1024 )
#define RP_LUA_MAX_INCLUDES			32		// files one plugin may include()

#define RP_LUA_MAX_DEPTH			8		// Lua -> game -> Lua nesting
#define RP_LUA_HOOK_STEP			1000	// the count hook fires every this many VM instructions

#define RP_LUA_MAX_HANDLERS			64		// per event, all plugins together
#define RP_LUA_MAX_COMMANDS			128
#define RP_LUA_CMD_NAME_SIZE		25		// [a-z0-9_]{2,24}
#define RP_LUA_CMD_HELP_SIZE		128
#define RP_LUA_MAX_TIMERS			1024
#define RP_LUA_PLUGIN_TIMERS		256
#define RP_LUA_TIMER_MIN_MS			50
#define RP_LUA_MAX_TASKS			256
#define RP_LUA_PLUGIN_TASKS			64

#define RP_LUA_ERROR_RING			50
#define RP_LUA_ERROR_LINE			200
#define RP_LUA_NOTICE_GAP_MS		30000	// admin notices: once per plugin per 30 s
#define RP_LUA_ERROR_WINDOW_MS		60000
#define RP_LUA_DEFER_WARN_MS		60000

#define RP_LUA_TEXT_MAX				900		// what Msg.* and print() pass on
#define RP_LUA_FILTER_SIZE			64		// a "use" handler's targetname, a "map_event" handler's name, an "npc_death"
											// handler's npc_type and spawner, a "damage" handler's target
#define RP_LUA_EVENT_NAME_MAX		32		// a target_lua's event: [a-z0-9_]{1,32}

typedef enum {
	RPLE_INIT,
	RPLE_SHUTDOWN,
	RPLE_UNLOAD,
	RPLE_FRAME,
	RPLE_CLIENT_CONNECT,
	RPLE_CLIENT_BEGIN,
	RPLE_CLIENT_SPAWN,
	RPLE_CLIENT_DISCONNECT,
	RPLE_CHAT,
	RPLE_COMMAND,
	RPLE_USE,				// API 2: an entity with the handler's targetname is used
	RPLE_MAP_EVENT,			// API 2: a target_lua was used
	RPLE_PLAYER_DEATH,		// API 3: a player died (not a knockdown)
	RPLE_PLAYER_DOWNED,		// API 3: a player was knocked down (the Death System)
	RPLE_NPC_DEATH,			// API 3: an NPC died
	RPLE_DAMAGE,			// API 3: a hit on a player or NPC was applied
	RPLE_NUM
} rpLuaEvent_t;

typedef struct {
	qboolean	inUse;
	qboolean	enabled;
	qboolean	isMap;
	qboolean	disabling;					// RP_Lua_DisablePlugin is running its unload event
	char		name[RP_LUA_NAME_SIZE];		// plugin folder name, "map:<file>" or "@console"
	char		dir[MAX_QPATH];				// "lua/plugins/<name>" or "lua/maps/<mapname>"
	char		mainFile[MAX_QPATH];		// "plugin.lua" or the map script's file name
	char		title[64];					// Plugin.register{ name = }
	char		version[32];				// Plugin.register{ version = }
	char		failReason[96];				// why it is not running, for /scripts
	int			envRef;						// registry ref of its environment table, LUA_NOREF when none
	char		includes[RP_LUA_MAX_INCLUDES][MAX_QPATH];
	int			numIncludes;

	int			errWindowStart;				// trap->Milliseconds() the current 60 s window began
	int			errWindowCount;
	int			errTotal;
	int			lastNotice;					// trap->Milliseconds() of the last admin notice, 0 none yet
	int			heldNotices;				// errors since then that were not announced

	unsigned	calls;
	double		instructions;
	int			ms;
	int			deferred;					// timers/tasks the frame budget moved to a later frame
	int			numTimers;
	int			numTasks;
	int			storeBytes;					// lua.db bytes counted against rp_lua_maxstore, -1 not read yet
	int			numEnts;					// entities and zones it spawned that still exist (rp_lua_maxents)
	int			numNpcs;					// NPCs it spawned that still exist, corpses included (rp_lua_maxnpcs)
} rpLuaPlugin_t;

typedef struct {
	lua_State		*L;
	qboolean		panicked;			// an unprotected error: the state is abandoned, never closed
	size_t			memUsed;
	size_t			memCap;

	rpLuaPlugin_t	plugins[RP_LUA_MAX_PLUGINS + 1];	// + the "@console" slot
	int				current;			// plugin whose code is running, -1 none
	int				depth;				// protected calls in progress

	// per call (the outermost one): instruction and time budget
	int				instr;
	int				instrLimit;
	int				callStart;
	int				msLimit;
	qboolean		overBudget;			// sticky until the outermost call returns

	// per server frame: timers, tasks and the frame event share rp_lua_framems
	int				frameStart;
	qboolean		frameExhausted;
	int				lastDeferWarn;

	int				rootRef;			// registry ref of the read-only API root every environment reads through
	int				playerRef[MAX_CLIENTS];	// cached player handles, LUA_NOREF when none

	struct {
		int		time;
		char	plugin[RP_LUA_NAME_SIZE];
		char	line[RP_LUA_ERROR_LINE];
	}				errors[RP_LUA_ERROR_RING];
	int				errorNext;
	int				errorCount;
} rpLua_t;

extern rpLua_t rl;
extern int rpLuaClientGen[MAX_CLIENTS];	// bumped when a slot's connection ends; survives state reloads

typedef int ( *rpLuaWorker_t )( lua_State *L, void *data );

// g_cmds.c
qboolean	check_admin_command( gentity_t *ent, int admin_command, qboolean with_message );
qboolean	G_IsBuiltinClientCommand( const char *name );
char		*ConcatArgs( int start );

// rp_lua_core.c
qboolean	RP_Lua_Call( int plugin, rpLuaWorker_t worker, void *data );
void		RP_Lua_ReportError( int plugin, const char *message );
int			RP_Lua_CurrentPlugin( lua_State *L );		// raises a Lua error when none
qboolean	RP_Lua_PluginRunning( int plugin );
void		RP_Lua_DisablePlugin( int plugin, const char *reason );
int			RP_Lua_LoadPluginFile( int plugin );
int			RP_Lua_AddPlugin( const char *name, qboolean isMap, const char *dir, const char *file );
int			RP_Lua_FindPlugin( const char *name );
qboolean	RP_Lua_ValidPluginName( const char *name );
qboolean	RP_Lua_ReadOSFile( const char *relPath, char **buf, int *len, char *err, int errSize );
void		RP_Lua_PushChunk( lua_State *L, int plugin, const char *dir, const char *file );
void		RP_Lua_NewEnv( lua_State *L, rpLuaPlugin_t *p );
qboolean	RP_Lua_FrameBudgetLeft( void );
void		RP_Lua_InitState( void );
void		RP_Lua_CloseState( void );
qboolean	RP_Lua_PluginListed( const char *name );
qboolean	RP_Lua_SetPluginListed( const char *name, qboolean listed );
void		RP_Lua_NoteDeferred( int plugin, int count );
void		RP_Lua_CleanText( char *out, int outSize, const char *in, qboolean keepNewlines );
void		RP_Lua_AdminNotice( const char *text );

// rp_lua_api.c
void		RP_LuaApi_Register( lua_State *L );				// fills the globals table on top of the stack
void		RP_LuaApi_Reset( void );
void		RP_LuaApi_RemovePlugin( int plugin );			// handlers, commands, timers, tasks
void		RP_LuaApi_FireEvent( rpLuaEvent_t ev, int onlyPlugin, int client, int extra, const char *text, int target, int *result );
void		RP_LuaApi_FireFrame( int levelTime );
void		RP_LuaApi_RunTimersAndTasks( int levelTime );
qboolean	RP_LuaApi_RunCommand( gentity_t *ent, const char *cmd );
void		RP_LuaApi_PushPlayer( lua_State *L, int client );	// nil when the slot is empty
int			RP_LuaApi_CheckPlayer( lua_State *L, int idx );	// the client number, or a Lua error
void		RP_LuaApi_PlayerGone( int client );
int			RP_LuaApi_CommandCount( void );
qboolean	RP_LuaApi_CommandInfo( int index, char *name, int nameSize, char *help, int helpSize, int *plugin );
qboolean	RP_LuaApi_ReservedCommand( const char *name );
qboolean	RP_LuaApi_FireUse( gentity_t *self, gentity_t *other, gentity_t *activator, qboolean forced );
void		RP_LuaApi_FireMapEvent( const char *name, gentity_t *activator, gentity_t *ent );
qboolean	RP_LuaApi_HasHandlers( rpLuaEvent_t ev );		// any running plugin listens
void		RP_LuaApi_FireActorEvent( rpLuaEvent_t ev, gentity_t *ent, gentity_t *other, int amount, int mod );	// API 3 events

// rp_lua_world.c
void		RP_LuaWorld_Register( lua_State *L );			// the World table, on the globals on top of the stack
void		RP_LuaWorld_PushEntity( lua_State *L, gentity_t *ent );	// nil when not in use
void		RP_LuaWorld_PushActor( lua_State *L, gentity_t *ent );	// player handle, entity handle or nil
void		RP_LuaWorld_RemovePlugin( int plugin );			// its entities go (next frame inside a game callback)
void		RP_LuaWorld_FreeAll( void );					// /scripts reload all: every script entity goes
void		RP_LuaWorld_Reset( void );						// the state is closing
void		RP_LuaWorld_Frame( void );						// queued frees, zones' leave checks
void		RP_LuaWorld_ClientGone( int client );			// zones see a disconnecting player leave
// shared with rp_lua_people.c
void		RP_LuaWorld_CheckVec( lua_State *L, int idx, vec3_t out, const char *what );	// a vector argument, or a Lua error
void		RP_LuaWorld_PushVec( lua_State *L, const vec3_t v );
qboolean	RP_LuaWorld_EventRoom( int n );					// room for n more temp entities this frame
qboolean	RP_LuaWorld_PathName( const char *s );			// a file name scripts may give
void		RP_LuaWorld_EnterCallback( void );				// game code calls scripts: free() and kill() wait for the
void		RP_LuaWorld_LeaveCallback( void );				// next frame until the matching leave
void		RP_LuaWorld_NpcDied( gentity_t *npc, gentity_t *killer );	// its on_death, when a script owns it

// rp_lua_people.c
int			RP_LuaPeople_PlayerField( lua_State *L, int client, const char *key );	// pushed one value (1) or nothing (0)
void		RP_LuaPeople_RemovePlugin( int plugin );		// the player effects it set that are still its own go
void		RP_LuaPeople_Reset( void );						// the state is closing: every script's effects go
const char	*RP_LuaPeople_TeleportRefusal( gentity_t *ent, const vec3_t dest );	// NULL when dest is clear for ent
void		RP_LuaPeople_DoTeleport( gentity_t *ent, const vec3_t dest, float yaw );
const char	*RP_LuaPeople_ModName( int mod );				// "MOD_SABER"
const char	*RP_LuaPeople_EffectName( int mode );			// "walkthrough", "ghost", "holo", "hidden", NULL for none
int			RP_LuaPeople_EffectFromName( const char *name );	// RP_PHASE_*, -1 for a name that is none of those

// rp_lua_store.c
qboolean	RP_LuaStore_Open( void );
void		RP_LuaStore_Close( void );
void		RP_LuaStore_Frame( void );
qboolean	RP_LuaStore_Flush( void );
void		RP_LuaStore_Register( lua_State *L );			// the Storage table, on the globals on top of the stack
void		RP_LuaStore_CharacterRemoved( int charID );
int			RP_LuaStore_PendingCount( void );

// rp_lua_admin.c
void		RP_LuaAdmin_Out( gentity_t *ent, const char *line );	// NULL: server console
void		RP_LuaAdmin_Flush( gentity_t *ent );

#endif // RP_LUA_LOCAL_H
